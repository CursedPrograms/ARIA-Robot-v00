/*
 * aria_fleet.c - ARIA in the DREAM Robotics fleet, on top of ESP-Drone.
 *
 * She joins the highest fleet network she can see, registers with whoever hosts its list (RIFT's
 * :5000), answers /status and /chirp on :5014, and sends the fleet's ESP-NOW "I'm here" beacon.
 *
 * The fleet can look at her and hear her. It can't fly her: nothing here touches a motor, a setpoint
 * or the commander, and RIFT lists no action that moves her. Flight comes first:
 *   - every task here runs at the lowest priority, far under the stabilizer;
 *   - while her motors are spinning she doesn't scan, join or chirp, because a WiFi scan leaves the
 *     channel and would cut the app that's flying her.
 *
 * Off by default (CONFIG_ARIA_FLEET): stock ESP-Drone behaviour otherwise.
 */
#include "sdkconfig.h"

#ifdef CONFIG_ARIA_FLEET

#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "aria_fleet.h"
#include "aria_fleet_logic.h"
#include "buzzer.h"
#include "led.h"
#include "motors.h"
#include "pm_esplane.h"
#define DEBUG_MODULE "FLEET"
#include "debug_cf.h"

#define FLEET_TASK_PRI 1
#define FLEET_TASK_STACK 4096
#define CHIRP_TASK_STACK 2048
#define BEACON_MS 500
#define REGISTER_MS 10000
#define RETRY_NETWORK_MS 30000

static const uint8_t BROADCAST[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static esp_netif_t *staNetif;
static QueueHandle_t chirpQueue;
static char joined[ARIA_NAME_MAX] = "";
static bool online = false;

static bool flying(void)
{
    for (uint32_t i = 0; i < NBR_OF_MOTORS; i++) {
        if (motorsGetRatio(i) > 0) {
            return true;
        }
    }
    return false;
}

// ---- the network: only ever looked for on the ground ----
static void joinFleet(void)
{
    wifi_scan_config_t scan = {0};
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
    if (!best) {
        return;
    }
    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid, best, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, CONFIG_ARIA_FLEET_PASSWORD, sizeof(sta.sta.password));
    strlcpy(joined, best, sizeof(joined));
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    esp_wifi_connect();
    DEBUG_PRINT_LOCAL("joining %s", joined);
}

static bool gatewayOf(char *out, size_t len)
{
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(staNetif, &ip) != ESP_OK || ip.ip.addr == 0) {
        return false;
    }
    snprintf(out, len, IPSTR, IP2STR(&ip.gw));
    return true;
}

// The fleet list on whoever hosts: the same form every robot posts
static void registerWithFleet(void)
{
    char gw[16], url[48];
    online = gatewayOf(gw, sizeof(gw));
    if (!online) {
        return;
    }
    snprintf(url, sizeof(url), "http://%s:5000/register", gw);
    esp_http_client_config_t cfg = {.url = url, .method = HTTP_METHOD_POST, .timeout_ms = 1500};
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

// ---- ESP-NOW: the fleet's beacon. She sends it and doesn't listen (as MILA and NIKA do): the
// others hear her, and ESP-Drone's own ESP-NOW control keeps the receive side ----
static void sendBeacon(void)
{
    static uint16_t seq = 0;
    AriaBeacon b;
    ariaBeaconFill(&b, flying(), seq++);
    esp_now_send(BROADCAST, (const uint8_t *)&b, sizeof(b));
}

static void fleetTask(void *arg)
{
    esp_now_init();   // already up with ESP-Drone's ESP-NOW control: it just says so
    esp_now_peer_info_t peer = {.channel = 0, .ifidx = WIFI_IF_AP, .encrypt = false};
    memcpy(peer.peer_addr, BROADCAST, ESP_NOW_ETH_ALEN);
    esp_now_add_peer(&peer);

    TickType_t nextRegister = 0, nextLook = 0;
    while (1) {
        const TickType_t now = xTaskGetTickCount();
        sendBeacon();
        char gw[16];
        if (!gatewayOf(gw, sizeof(gw))) {
            online = false;
            if (!flying() && (int32_t)(now - nextLook) >= 0) {
                nextLook = now + pdMS_TO_TICKS(RETRY_NETWORK_MS);
                joinFleet();
            }
        } else if ((int32_t)(now - nextRegister) >= 0) {
            nextRegister = now + pdMS_TO_TICKS(REGISTER_MS);
            registerWithFleet();
        }
        vTaskDelay(pdMS_TO_TICKS(BEACON_MS));
    }
}

// ---- TALKING: the fleet's Brainfuck phrases, a tone per symbol on her buzzer (if the buzzer deck's
// fitted) and a flash of her LED, so she talks either way. On the ground only ----
static void chirpTask(void *arg)
{
    int u;
    while (1) {
        if (xQueueReceive(chirpQueue, &u, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const char *bf = ariaTalkProgram(u);
        for (; bf && *bf && !flying(); bf++) {
            buzzerOn(ariaToneHz(*bf));
            ledSet(LED_BLUE, true);
            vTaskDelay(pdMS_TO_TICKS(ariaToneMs(*bf)));
            buzzerOff();
            ledSet(LED_BLUE, false);
            vTaskDelay(pdMS_TO_TICKS(ARIA_TONE_GAP_MS));
        }
    }
}

// ---- :5014 ----
static esp_err_t sendJson(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t statusGet(httpd_req_t *req)
{
    char json[220];
    wifi_ap_record_t ap = {0};
    const int rssi = online && esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
    snprintf(json, sizeof(json),
             "{\"name\":\"ARIA\",\"type\":\"drone\",\"mode\":\"%s\",\"moving\":%s,\"flying\":%s,"
             "\"battery_v\":%.2f,\"network\":\"%s\",\"rssi\":%d}",
             flying() ? "flying" : "ground", flying() ? "true" : "false", flying() ? "true" : "false",
             (double)pmGetBatteryVoltage(), joined, rssi);
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
    if (flying()) {
        httpd_resp_set_status(req, "409 Conflict");
        return sendJson(req, "{\"ok\":false,\"error\":\"she's flying\"}");
    }
    xQueueSend(chirpQueue, &u, 0);
    return sendJson(req, "{\"ok\":true}");
}

void ariaFleetStart(esp_netif_t *sta)
{
    staNetif = sta;
    chirpQueue = xQueueCreate(2, sizeof(int));

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = ARIA_PORT;
    cfg.ctrl_port = ARIA_PORT + 1;
    cfg.task_priority = FLEET_TASK_PRI;
    cfg.max_open_sockets = 3;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) == ESP_OK) {
        const httpd_uri_t routes[] = {
            {.uri = "/status", .method = HTTP_GET, .handler = statusGet},
            {.uri = "/ping", .method = HTTP_GET, .handler = pingGet},
            {.uri = "/chirp", .method = HTTP_GET, .handler = chirpGet},
        };
        for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
            httpd_register_uri_handler(server, &routes[i]);
        }
    }
    xTaskCreate(fleetTask, "fleet", FLEET_TASK_STACK, NULL, FLEET_TASK_PRI, NULL);
    xTaskCreate(chirpTask, "chirp", CHIRP_TASK_STACK, NULL, FLEET_TASK_PRI, NULL);
}

#endif  // CONFIG_ARIA_FLEET
