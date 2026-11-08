/*
 * aria_fleet.cpp - ARIA in the DREAM Robotics fleet, on top of CF-Drone.
 *
 * She joins the highest fleet network she can see, registers with whoever hosts its list (RIFT's
 * :5000), serves her controller page, /status and /chirp on :5014, and sends the fleet's ESP-NOW
 * "I'm here" beacon. She also listens for her app's flight packets on UDP 2390.
 *
 * The fleet can look at her and hear her. It can't fly her: RIFT lists no action that moves her, and
 * the browser's /fly-ws only takes a client on her own WiFi, never one on the fleet's network.
 * Flight comes first:
 *   - all of this runs on core 0 at the lowest priority; the flight loop has core 1;
 *   - while she's armed she doesn't scan, join or chirp, because a WiFi scan leaves the channel and
 *     would cut the app that's flying her. She only looks for a higher network on the ground with
 *     nobody on her own WiFi, for the same reason.
 */
#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"

#include "aria_fleet.h"
#include "aria_fleet_logic.h"
#include "aria_link_logic.h"
#include "quaternion.h"
#include "web_files.h"

#define FLEET_CORE 0
#define FLEET_TASK_PRI 1
#define LINK_TASK_PRI 2
#define FLEET_TASK_STACK 4096
#define SMALL_TASK_STACK 3072
#define BEACON_MS 500
#define REGISTER_MS 10000
#define RETRY_NETWORK_MS 30000
#define CLIMB_MS 60000
#define UDP_PORT 2390
#define FLEET_PASSWORD "mmvbkbak"

extern bool armed;
extern float batteryVoltage;
extern Quaternion attitude;

static_assert(sizeof(AriaBeacon) == 91, "the fleet's beacon is 91 bytes");

static const uint8_t BROADCAST[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static esp_netif_t *staNetif;
static QueueHandle_t chirpQueue;
static bool talkMuted = false;   // her sound switch: she chats, but her LED doesn't blink it (/talkmute?on=)
static bool chatOff = false;     // her chat switch: no Brainfuck with the fleet at all (/talkmute?chat_off=)
static char joined[ARIA_NAME_MAX] = "";
static bool online = false;

// ---- the network: only ever looked for on the ground ----
static void joinFleet()
{
    wifi_scan_config_t scan = {};
    if (esp_wifi_scan_start(&scan, true) != ESP_OK) {
        return;
    }
    static wifi_ap_record_t seen[ARIA_SCAN_MAX];
    uint16_t count = ARIA_SCAN_MAX;
    if (esp_wifi_scan_get_ap_records(&count, seen) != ESP_OK) {
        return;
    }
    const char *names[ARIA_SCAN_MAX];
    for (uint16_t i = 0; i < count; i++) {
        names[i] = (const char *)seen[i].ssid;
    }
    const char *best = ariaBestNetwork(names, count);
    if (!best || strcmp(best, joined) == 0 || armed) {
        return;
    }
    esp_wifi_disconnect();        // the ladder: leaving a lower network for this one
    wifi_config_t sta = {};
    strlcpy((char *)sta.sta.ssid, best, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, FLEET_PASSWORD, sizeof(sta.sta.password));
    strlcpy(joined, best, sizeof(joined));
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    esp_wifi_connect();
}

static bool gatewayOf(char *out, size_t len)
{
    esp_netif_ip_info_t ip;
    if (!staNetif || esp_netif_get_ip_info(staNetif, &ip) != ESP_OK || ip.ip.addr == 0) {
        return false;
    }
    snprintf(out, len, IPSTR, IP2STR(&ip.gw));
    return true;
}

// The fleet list on whoever hosts: the same form every robot posts
static void registerWithFleet()
{
    char gw[16], url[48];
    online = gatewayOf(gw, sizeof(gw));
    if (!online) {
        return;
    }
    snprintf(url, sizeof(url), "http://%s:5000/register", gw);
    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 1500;
    esp_http_client_handle_t http = esp_http_client_init(&cfg);
    if (!http) {
        return;
    }
    const char *body = "name=ARIA&type=drone&capabilities=web:5014,talk:5014";
    esp_http_client_set_header(http, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(http, body, strlen(body));
    esp_http_client_perform(http);
    esp_http_client_cleanup(http);
}

// ---- ESP-NOW: the fleet's beacon, sent and heard ----
static AriaPeer peers[ARIA_PEERS_MAX];

static void beaconHeard(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    ariaPeerHeard(peers, data, len, info->rx_ctrl->rssi, millis());
}

static void sendBeacon()
{
    static uint16_t seq = 0;
    AriaBeacon b;
    ariaBeaconFill(&b, armed, seq++);
    esp_now_send(BROADCAST, (const uint8_t *)&b, sizeof(b));
}

static void fleetTask(void *arg)
{
    esp_now_init();
    esp_now_peer_info_t peer = {};
    peer.ifidx = WIFI_IF_AP;
    memcpy(peer.peer_addr, BROADCAST, ESP_NOW_ETH_ALEN);
    esp_now_add_peer(&peer);
    esp_now_register_recv_cb(beaconHeard);

    uint32_t nextRegister = 0, nextLook = 0, nextClimb = CLIMB_MS;
    while (true) {
        const uint32_t now = millis();
        sendBeacon();
        char gw[16];
        if (!gatewayOf(gw, sizeof(gw))) {
            online = false;
            if (!armed && (int32_t)(now - nextLook) >= 0) {
                nextLook = now + RETRY_NETWORK_MS;
                joinFleet();
            }
        } else if ((int32_t)(now - nextRegister) >= 0) {
            nextRegister = now + REGISTER_MS;
            registerWithFleet();
        } else if (!armed && WiFi.softAPgetStationNum() == 0 && (int32_t)(now - nextClimb) >= 0) {
            nextClimb = now + CLIMB_MS;
            joinFleet();
        }
        delay(BEACON_MS);
    }
}

// ---- TALKING: the fleet's Brainfuck phrases as flashes of her LED. On the ground only. The kit has no
// buzzer, and the S3 board has no spare LED: she talks only where she can ----
static void chirpTask(void *arg)
{
    int u;
    while (true) {
        if (xQueueReceive(chirpQueue, &u, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const char *bf = ariaTalkProgram(u);
        for (; bf && *bf && !armed; bf++) {
            ariaTalkLed = 1;
            delay(ariaToneMs(*bf));
            ariaTalkLed = 0;
            delay(ARIA_TONE_GAP_MS);
        }
        ariaTalkLed = -1;
    }
}

// ---- flight packets from her app: the socket waits on this core, the flight loop never does ----
static void udpTask(void *arg)
{
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sock, (sockaddr *)&addr, sizeof(addr));
    uint8_t packet[32], frame[ARIA_FRAME_BYTES];
    while (true) {
        const int len = recvfrom(sock, packet, sizeof(packet), 0, NULL, NULL);
        if (ariaCrtpFrame(packet, len, frame)) {
            ariaLinkFrame(frame);
        }
    }
}

// ---- :5014: her controller page (web/, built into web_files.h by scripts/embed_web.py) ----
static esp_err_t sendFile(httpd_req_t *req, const char *type, const char *text)
{
    httpd_resp_set_type(req, type);
    return httpd_resp_sendstr(req, text);
}

static esp_err_t pageGet(httpd_req_t *req) { return sendFile(req, "text/html", WEB_INDEX_HTML); }
static esp_err_t styleGet(httpd_req_t *req) { return sendFile(req, "text/css", WEB_STYLE_CSS); }
static esp_err_t scriptGet(httpd_req_t *req) { return sendFile(req, "text/javascript", WEB_APP_JS); }
static esp_err_t flyPageGet(httpd_req_t *req) { return sendFile(req, "text/html", WEB_FLY_HTML); }
static esp_err_t flyStyleGet(httpd_req_t *req) { return sendFile(req, "text/css", WEB_FLY_CSS); }
static esp_err_t flyScriptGet(httpd_req_t *req) { return sendFile(req, "text/javascript", WEB_FLY_JS); }

static bool onOwnWifi(httpd_req_t *req)
{
    sockaddr_in6 peer;
    socklen_t len = sizeof(peer);
    if (getpeername(httpd_req_to_sockfd(req), (sockaddr *)&peer, &len) != 0) {
        return false;
    }
    return ariaOnOwnWifi(ntohl(peer.sin6_addr.un.u32_addr[3]));
}

// Flying from the browser: one frame of roll, pitch, yaw and thrust each
static esp_err_t flySocket(httpd_req_t *req)
{
    if (!onOwnWifi(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "only on her own WiFi");
        return ESP_FAIL;
    }
    if (req->method == HTTP_GET) {
        return ESP_OK;          // the handshake
    }
    uint8_t in[ARIA_FRAME_BYTES];
    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_BINARY;
    frame.payload = in;
    if (httpd_ws_recv_frame(req, &frame, sizeof(in)) == ESP_OK && frame.len == sizeof(in)) {
        ariaLinkFrame(in);
    }
    return ESP_OK;
}

static esp_err_t sendJson(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t statusGet(httpd_req_t *req)
{
    char json[320];
    const Quaternion now = attitude;
    wifi_ap_record_t ap = {};
    const int rssi = online && esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
    const char *flying = armed ? "true" : "false";
    snprintf(json, sizeof(json),
             "{\"name\":\"ARIA\",\"type\":\"drone\",\"mode\":\"%s\",\"moving\":%s,\"flying\":%s,"
             "\"battery_v\":%.2f,\"pitch\":%.1f,\"roll\":%.1f,\"network\":\"%s\",\"rssi\":%d,\"talk_muted\":%s,\"chat_off\":%s}",
             armed ? "flying" : "ground", flying, flying, (double)batteryVoltage,
             (double)degrees(now.getPitch()), (double)degrees(now.getRoll()), joined, rssi, talkMuted ? "true" : "false",
             chatOff ? "true" : "false");
    return sendJson(req, json);
}

static esp_err_t nearGet(httpd_req_t *req)
{
    char json[420];
    ariaNearJson(peers, millis(), json, sizeof(json));
    return sendJson(req, json);
}

static esp_err_t pingGet(httpd_req_t *req)
{
    return httpd_resp_sendstr(req, "ARIA");
}

static esp_err_t chirpGet(httpd_req_t *req)
{
    char query[16] = "", value[6] = "";
    int u = -1;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "u", value, sizeof(value)) == ESP_OK) {
        sscanf(value, "%d", &u);
    }
    if (!ariaTalkProgram(u)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return sendJson(req, "{\"ok\":false,\"error\":\"use /chirp?u=0-16\"}");
    }
    if (chatOff) {
        httpd_resp_set_status(req, "409 Conflict");
        return sendJson(req, "{\"ok\":false,\"reason\":\"chat_off\"}");
    }
    if (armed) {
        httpd_resp_set_status(req, "409 Conflict");
        return sendJson(req, "{\"ok\":false,\"error\":\"she's flying\"}");
    }
    if (talkMuted) return sendJson(req, "{\"ok\":true,\"silent\":true}");   // said, without the LED
    xQueueSend(chirpQueue, &u, 0);
    return sendJson(req, "{\"ok\":true}");
}

// Her two talk switches, kept in NVS (no PIN: they only quiet her). 1|0|toggle each:
//   ?on=        sound: she chats, but her LED doesn't blink it
//   ?chat_off=  chat: no Brainfuck with the fleet at all
static bool talkFlag(const char *v, bool now)
{
    if (v[0] == 't' && v[1] == 'o') return !now;   // toggle
    return v[0] == '1' || v[0] == 't';             // 1, true
}

static esp_err_t talkMuteGet(httpd_req_t *req)
{
    char query[40] = "", value[8] = "";
    bool changed = false;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "on", value, sizeof(value)) == ESP_OK) {
            talkMuted = talkFlag(value, talkMuted);
            changed = true;
        }
        if (httpd_query_key_value(query, "chat_off", value, sizeof(value)) == ESP_OK) {
            chatOff = talkFlag(value, chatOff);
            changed = true;
        }
    }
    if (changed) {
        Preferences p;
        p.begin("talk", false);
        p.putBool("muted", talkMuted);
        p.putBool("chatoff", chatOff);
        p.end();
    }
    char json[48];
    snprintf(json, sizeof(json), "{\"talk_muted\":%s,\"chat_off\":%s}", talkMuted ? "true" : "false",
             chatOff ? "true" : "false");
    return sendJson(req, json);
}

static void route(httpd_handle_t server, const char *uri, esp_err_t (*handler)(httpd_req_t *), bool socket = false)
{
    httpd_uri_t r = {};
    r.uri = uri;
    r.method = HTTP_GET;
    r.handler = handler;
    r.is_websocket = socket;
    httpd_register_uri_handler(server, &r);
}

void ariaFleetStart()
{
    staNetif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    chirpQueue = xQueueCreate(2, sizeof(int));
    {
        Preferences p;
        p.begin("talk", true);
        talkMuted = p.getBool("muted", false);
        chatOff = p.getBool("chatoff", false);
        p.end();
    }
    MDNS.begin("aria");           // http://aria.local:5014 on the fleet's network
    MDNS.addService("http", "tcp", ARIA_PORT);

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = ARIA_PORT;
    cfg.ctrl_port = ARIA_PORT + 1;
    cfg.task_priority = FLEET_TASK_PRI;
    cfg.core_id = FLEET_CORE;
    cfg.max_open_sockets = 4;
    cfg.max_uri_handlers = 12;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) == ESP_OK) {
        route(server, "/", pageGet);
        route(server, "/style.css", styleGet);
        route(server, "/app.js", scriptGet);
        route(server, "/fly", flyPageGet);
        route(server, "/fly.css", flyStyleGet);
        route(server, "/fly.js", flyScriptGet);
        route(server, "/fly-ws", flySocket, true);
        route(server, "/status", statusGet);
        route(server, "/near", nearGet);
        route(server, "/ping", pingGet);
        route(server, "/chirp", chirpGet);
        route(server, "/talkmute", talkMuteGet);   // the 12th: max_uri_handlers
    }
#if ARIA_BLE
    ariaBleStart();
#endif
    xTaskCreatePinnedToCore(udpTask, "aria-udp", SMALL_TASK_STACK, NULL, LINK_TASK_PRI, NULL, FLEET_CORE);
    xTaskCreatePinnedToCore(fleetTask, "fleet", FLEET_TASK_STACK, NULL, FLEET_TASK_PRI, NULL, FLEET_CORE);
    xTaskCreatePinnedToCore(chirpTask, "chirp", SMALL_TASK_STACK, NULL, FLEET_TASK_PRI, NULL, FLEET_CORE);
}
