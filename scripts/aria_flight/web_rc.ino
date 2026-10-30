// Web RC server

#if WEB_RC_ENABLED

#include <WebServer.h>
#include <WiFi.h>
#include "web_rc_html.h"
#include "board_config.h"

// shared control inputs (written by each protocol, as SBUS and MAVLink)
extern float t;
extern float controlTime;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;

// ==================== Settings ====================
#define WEB_RC_TIMEOUT_MS    10000 // connection timeout (ms) since the last packet: must be over the heartbeat
// VBAT_ADC_PIN, VBAT_ADC_SAMPLES and VBAT_DIVIDER moved to battery.ino

// ==================== Connection flags ====================
bool webRCEnabled    = false; // Web RC has a live connection (updated by readWebRC() every frame)
bool useWebRC        = false; // Web RC is in control now (kept with webRCEnabled)
bool webRCUpdated    = false; // has had at least one stick packet (false before connecting)
bool webConsoleEnabled = false; // web console switch: POST /console/enable makes print() write to the buffer
char webRCWarnMsg[64] = ""; // warning waiting for the page, cleared once sent

// ==================== Last request context (for the response) ====================
static int lastProcType        = 0; // 1 stick, 2 button, 4 heartbeat
static int lastProcButtonIdx   = -1; // button event: button number (0-15), otherwise -1
static int lastProcButtonState = -1; // button event: 1 pressed, 0 released, otherwise -1

// ==================== Latest sticks (for the status endpoint) ====================
// units: throttle 0-100 (%), attitude +/-30 (deg), buttons 1 pressed, 0 released
float webRCRoll     = 0.0f;
float webRCPitch    = 0.0f;
float webRCYaw      = 0.0f;
float webRCThrottle = 0.0f;
uint16_t webRCButtons    = 0; // 16-bit button mask: bit0 arm, bit1 disarm, bit2 emergency stop, bit6 STAB, bit7 ACRO, bit8 ALTHOLD
unsigned long webRCLastUpdate = 0; // millis() of the last packet

// ==================== Sensitivity ====================
// output = processed angle * scale / STICK_MAX, into control* ([-1,1])
// lower scale is gentler, higher is sharper
float webRCThrottleScale = 1.0f; // throttle limit: 1.0 is 100%, 0.8 stops at 80%
float webRCStickScale    = 0.85f; // roll and pitch sensitivity, suggested 0.5-1.0
float webRCYawScale      = 0.68f; // yaw sensitivity, lower because yaw has little inertia, suggested 0.3-0.8

// ==================== Dead zones ====================
// normalised (0-1): stick centring error inside the zone outputs 0
// wider means fewer accidents, narrower responds more but can drift
float stickDeadzone    = 0.06f; // shared by roll, pitch and yaw, 6%
float throttleDeadzone = 0.06f; // throttle bottom dead zone: below this, 0%

// ==================== Input ranges (must match the page) ====================
static const float THROTTLE_MIN = 0.0f; // lowest throttle (%)
static const float THROTTLE_MAX = 100.0f; // highest throttle (%)
static const float STICK_MAX    = 30.0f; // maximum attitude (deg) at full stick: keep in step with TILT_MAX
static const float RAW_MAX      = 100.0f; // full stick from the page, whose JS always sends +/-100

// ==================== Internal state ====================
static float lastValidThrottle = 0.0f; // last good throttle, used on bad data
static float lastValidRoll     = 0.0f; // last good roll
static float lastValidPitch    = 0.0f; // last good pitch
static float lastValidYaw      = 0.0f; // last good yaw
static unsigned long lastDataErrorTime = 0; // last bad-data time, to limit error logging

// ==================== Web server ====================
WebServer webRCServer(80); // main server on port 80

// ------ old PCB printed address: :8080 redirects to 80; remove when those are gone ------------
// :8080 redirect: a heap pointer built in setupWebRC(), 4 bytes of BSS
// the C3 has no old users: compiled out, avoiding a 200 ms spin on one core and an extra socket
#ifndef CONFIG_IDF_TARGET_ESP32C3
static WiFiServer* redirectServer8080 = nullptr;
#endif
// ------ old PCB printed address: :8080 redirects to 80; remove when those are gone ------------

// ==================== Console log buffer ====================
#define CONSOLE_LINES    BOARD_CONSOLE_LINES // console lines: C3 20 (saves about 7 KB of RAM), ESP32 and S3 50
#define CONSOLE_LINE_LEN BOARD_CONSOLE_LINE_LEN // characters a line: C3 160, ESP32 and S3 240
static char consoleBuf[CONSOLE_LINES][CONSOLE_LINE_LEN];
static int  consoleTail   = 0;
static int  consoleFilled = 0;
static int  consoleTotal  = 0; // running total of lines, for fetching only new ones

#define CONSOLE_CMD_QUEUE_SIZE 4
#define CONSOLE_CMD_LEN 64
static char consoleCmdQueue[CONSOLE_CMD_QUEUE_SIZE][CONSOLE_CMD_LEN];
static int  consoleCmdHead  = 0;
static int  consoleCmdTail  = 0;
static int  consoleCmdCount = 0;

bool enqueueConsoleCmd(const char* cmd) {
    if (!cmd || !*cmd) return false;
    if (consoleCmdCount >= CONSOLE_CMD_QUEUE_SIZE) return false;
    strncpy(consoleCmdQueue[consoleCmdTail], cmd, CONSOLE_CMD_LEN - 1);
    consoleCmdQueue[consoleCmdTail][CONSOLE_CMD_LEN - 1] = '\0';
    consoleCmdTail = (consoleCmdTail + 1) % CONSOLE_CMD_QUEUE_SIZE;
    consoleCmdCount++;
    return true;
}

void processConsoleCommandQueue() {
    if (consoleCmdCount <= 0) return;

    String cmd = consoleCmdQueue[consoleCmdHead];
    consoleCmdHead = (consoleCmdHead + 1) % CONSOLE_CMD_QUEUE_SIZE;
    consoleCmdCount--;
    doCommand(cmd, false);
}

void webLog(const char* msg) {
    // split on \n, so newlines don't break the JSON
    const char* start = msg;
    while (*start) {
        const char* end = strchr(start, '\n');
        int len = end ? (int)(end - start) : (int)strlen(start);
        if (len > 0) {
            int copy = (len < CONSOLE_LINE_LEN - 1) ? len : (CONSOLE_LINE_LEN - 1);
            strncpy(consoleBuf[consoleTail], start, copy);
            consoleBuf[consoleTail][copy] = '\0';
            consoleTail = (consoleTail + 1) % CONSOLE_LINES;
            if (consoleFilled < CONSOLE_LINES) consoleFilled++;
            consoleTotal++;
        }
        if (!end) break;
        start = end + 1;
    }
}

// ==================== Stick processing ====================

// normalised dead zone (input and output [-1, 1])
// what's past the zone is stretched to full range: full stick is full output
float applyDeadzone(float norm, float deadzone) {
    if (fabsf(norm) < deadzone) return 0.0f;
    float sign = (norm > 0.0f) ? 1.0f : -1.0f;
    return sign * (fabsf(norm) - deadzone) / (1.0f - deadzone);
}

// attitude axes (roll, pitch, yaw)
// input raw in [-100, +100], output in [-STICK_MAX, +STICK_MAX] (deg)
float processAxis(float raw, float& lastValid) {
    if (isnan(raw) || isinf(raw) || fabsf(raw) > 1000.0f) {
        if (millis() - lastDataErrorTime > 2000) lastDataErrorTime = millis();
        return lastValid;
    }
    float norm = constrain(raw, -RAW_MAX, RAW_MAX) / RAW_MAX;
    norm = applyDeadzone(norm, stickDeadzone);
    lastValid = norm * STICK_MAX;
    return lastValid;
}

// throttle axis
// page: left stick Y, bottom -100, top +100
// output: pct in [0, 100]
float processThrottle(float raw) {
    if (isnan(raw) || isinf(raw) || fabsf(raw) > 1000.0f) return lastValidThrottle;
    raw = constrain(raw, -RAW_MAX, RAW_MAX);
    // linear: -100 is 0%, 0 is 50%, +100 is 100%
    float pct = (raw + RAW_MAX) / (2.0f * RAW_MAX) * THROTTLE_MAX;
    if (pct < throttleDeadzone * THROTTLE_MAX) pct = 0.0f;
    pct = constrain(pct * webRCThrottleScale, THROTTLE_MIN, THROTTLE_MAX);
    lastValidThrottle = pct;
    return pct;
}

// ==================== Core data ====================

void setWebRCInput(float roll, float pitch, float yaw, float throttle, uint16_t buttons) {
    float pThrottle = processThrottle(throttle);
    float pYaw      = processAxis(yaw,   lastValidYaw);
    float pPitch    = processAxis(pitch, lastValidPitch);
    float pRoll     = processAxis(roll,  lastValidRoll);

    // keep the processed values (for the status endpoint)
    webRCThrottle = pThrottle;
    webRCYaw      = pYaw;
    webRCPitch    = pPitch;
    webRCRoll     = pRoll;
    webRCButtons  = buttons;
    webRCLastUpdate = millis();
    webRCUpdated  = true;

    // write the shared control inputs (same path as SBUS and MAVLink)
    controlRoll     = constrain(pRoll  * webRCStickScale / STICK_MAX, -1.0f, 1.0f);
    controlPitch    = constrain(pPitch * webRCStickScale / STICK_MAX, -1.0f, 1.0f);
    controlYaw      = constrain(pYaw   * webRCYawScale   / STICK_MAX, -1.0f, 1.0f);
    controlThrottle = pThrottle / THROTTLE_MAX;
    controlMode     = NAN;
    controlTime     = t;

    static float lastPrintedThrottle = -1.0f;
    if (fabsf(pThrottle - lastPrintedThrottle) > 5.0f) {
        print("WebRC T=%.0f%% R=%.1f P=%.1f Y=%.1f Btn=0x%04X\n",
              pThrottle, pRoll, pPitch, pYaw, buttons);
        lastPrintedThrottle = pThrottle;
    }
}

// ==================== JSON protocol ====================

const char* findJsonValue(const char* json, const char* key) {
    const char* p = strstr(json, key);
    if (!p) return nullptr;
    p = strchr(p, ':');
    if (!p) return nullptr;
    p++;
    while (*p == ' ' || *p == '"') p++;
    return p;
}

bool handleJSONProtocol(String& body) {
    if (body.length() == 0 || body.indexOf('{') == -1) return false;
    const char* json = body.c_str();
    const char* typePos = strstr(json, "\"t\":");
    if (!typePos) return false;
    int type = atoi(typePos + 4);
    const char* v;

    lastProcType        = type;
    lastProcButtonIdx   = -1;
    lastProcButtonState = -1;

    switch (type) {
        case 1: { // sticks
            float th = 0, r = 0, p = 0, y = 0;
            uint32_t ts = 0;
            if ((v = findJsonValue(json, "\"th\""))) th = atof(v);
            if ((v = findJsonValue(json, "\"r\"")))  r  = atof(v);
            if ((v = findJsonValue(json, "\"p\"")))  p  = atof(v);
            if ((v = findJsonValue(json, "\"y\"")))  y  = atof(v);
            if ((v = findJsonValue(json, "\"ts\""))) ts = atol(v);
            setWebRCInput(r, p, y, th, webRCButtons);
            break;
        }
        case 2: { // button event
            int idx = 0, state = 0;
            uint32_t ts = 0;
            if ((v = findJsonValue(json, "\"b\"")))  idx   = atoi(v);
            if ((v = findJsonValue(json, "\"s\"")))  state = atoi(v);
            if ((v = findJsonValue(json, "\"ts\""))) ts    = atol(v);
            if (idx >= 0 && idx < 16) {
                if (state) webRCButtons |=  (1 << idx);
                else       webRCButtons &= ~(1 << idx);
                webRCLastUpdate = millis();
                webRCUpdated    = true;
                lastProcButtonIdx   = idx;
                lastProcButtonState = state;
            }
            break;
        }
        case 4: // heartbeat
            webRCLastUpdate = millis();
            webRCUpdated    = true;
            break;
    }
    return true;
}

void setWebRCWarn(const char* msg) {
    strncpy(webRCWarnMsg, msg, sizeof(webRCWarnMsg) - 1);
    webRCWarnMsg[sizeof(webRCWarnMsg) - 1] = '\0';
}

// ==================== Last request context (for the response) ====================
// ==================== HTTP requests ====================

void handleWebRCRequest() {
    if (!webRCServer.hasArg("plain")) {
        webRCServer.send(400, "application/json", "{\"e\":\"no data\"}");
        return;
    }
    String body = webRCServer.arg("plain");
    if (handleJSONProtocol(body)) {
        char resp[320];
        // warn goes out on button (rt=2) and heartbeat (rt=4) responses, then clears:
        // rt=2: the user did something (arming refused and so on), they need to know now
        // rt=4: the system did something (low battery disarm and so on), a heartbeat's 2 s delay is fine
        // stick packets (rt=1) don't take warn, or they could clear it before the state=0 response
        bool deliverWarn = (lastProcType == 2 || lastProcType == 4) && webRCWarnMsg[0];
        if (deliverWarn) {
            if (lastProcButtonIdx >= 0) {
                snprintf(resp, sizeof(resp),
                    "{\"s\":\"ok\",\"m\":%d,\"arm\":%d,\"rt\":%d,\"bi\":%d,\"bs\":%d,\"warn\":\"%s\"}",
                    mode, (int)armed, lastProcType, lastProcButtonIdx, lastProcButtonState, webRCWarnMsg);
            } else {
                snprintf(resp, sizeof(resp),
                    "{\"s\":\"ok\",\"m\":%d,\"arm\":%d,\"rt\":%d,\"warn\":\"%s\"}",
                    mode, (int)armed, lastProcType, webRCWarnMsg);
            }
            webRCWarnMsg[0] = '\0'; // clear once sent
        } else {
            if (lastProcButtonIdx >= 0) {
                snprintf(resp, sizeof(resp),
                    "{\"s\":\"ok\",\"m\":%d,\"arm\":%d,\"rt\":%d,\"bi\":%d,\"bs\":%d}",
                    mode, (int)armed, lastProcType, lastProcButtonIdx, lastProcButtonState);
            } else {
                snprintf(resp, sizeof(resp),
                    "{\"s\":\"ok\",\"m\":%d,\"arm\":%d,\"rt\":%d}",
                    mode, (int)armed, lastProcType);
            }
        }
        webRCServer.send(200, "application/json", resp);
    } else {
        webRCServer.send(400, "application/json", "{\"e\":\"parse failed\"}");
    }
}

// ==================== Connection state ====================

bool isWebRCEnabled() {
    return webRCUpdated && (millis() - webRCLastUpdate < WEB_RC_TIMEOUT_MS);
}

bool isUsingWebRC() {
    return useWebRC && isWebRCEnabled();
}

// ------ old PCB printed address: :8080 redirects to 80; remove when those are gone ------------
// old RC port: 301 redirect on 8080 (raw WiFiServer TCP, HTTP parsed by hand)
// the C3 has no old users: compiled out, so a 200 ms spin can't starve WiFi on one core
#ifndef CONFIG_IDF_TARGET_ESP32C3
static void handleRedirect8080() {
    if (!redirectServer8080) return;
    WiFiClient client = redirectServer8080->accept();
    if (!client) return;

    String path = "/";
    String host = "";
    unsigned long t0 = millis();
    while (!client.available() && millis() - t0 < 200) {}

    if (client.available()) {
        // request line: GET /path HTTP/1.1
        String reqLine = client.readStringUntil('\n');
        int s1 = reqLine.indexOf(' ');
        int s2 = reqLine.indexOf(' ', s1 + 1);
        if (s1 >= 0 && s2 > s1) path = reqLine.substring(s1 + 1, s2);

        // read headers for Host (without the :8080)
        while (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) break;
            if (line.startsWith("Host:") || line.startsWith("host:")) {
                host = line.substring(5);
                host.trim();
                int colon = host.indexOf(':');
                if (colon >= 0) host = host.substring(0, colon);
            }
        }
    }

    client.print("HTTP/1.1 301 Moved Permanently\r\nLocation: http://");
    client.print(host);
    client.print(path);
    client.print("\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
    client.stop();
}
#endif
// ------ old PCB printed address: :8080 redirects to 80; remove when those are gone ------------

// ==================== Main setup ====================

void setupWebRC() {
    print("Setup WEB RC\n");
    lastValidThrottle = THROTTLE_MIN;
    lastValidRoll = lastValidPitch = lastValidYaw = 0.0f;

    webRCServer.on("/", HTTP_GET, []() {
        webRCServer.send(200, "text/html", webRCIndexHtml);
    });
    webRCServer.on("/web_rc",           HTTP_POST, handleWebRCRequest);
    webRCServer.on("/web_rc/heartbeat", HTTP_POST, handleWebRCRequest);

    webRCServer.on("/console", HTTP_GET, []() {
        int since = -1;
        int limit = 20;
        if (webRCServer.hasArg("since")) since = webRCServer.arg("since").toInt();
        if (webRCServer.hasArg("limit")) limit = webRCServer.arg("limit").toInt();
        if (limit <= 0) limit = 20;
        if (limit > CONSOLE_LINES) limit = CONSOLE_LINES;

        int availableFrom = max(0, consoleTotal - consoleFilled);
        int sendFrom = (since >= 0) ? since : availableFrom;
        if (sendFrom < availableFrom) sendFrom = availableFrom;
        if (sendFrom > consoleTotal) sendFrom = consoleTotal;
        int sendTo = min(consoleTotal, sendFrom + limit);

        String json = "{\"total\":";
        json += consoleTotal;
        json += ",\"next\":";
        json += sendTo;
        json += ",\"has_more\":";
        json += (sendTo < consoleTotal) ? "true" : "false";
        json += ",\"lines\":[";

        bool first = true;
        for (int i = sendFrom; i < sendTo; i++) {
            int idx = i % CONSOLE_LINES;
            if (!first) json += ",";
            first = false;
            json += "\"";
            String line = consoleBuf[idx];
            line.replace("\\", "\\\\");  // \ → \\
            line.replace("\"", "\\\"");  // " → \"
            line.replace("\n", "\\n"); // fallback: newline to \n
            line.replace("\r", "\\r"); // fallback: CR to \r
            json += line + "\"";
        }
        json += "]}";
        webRCServer.send(200, "application/json", json);
    });

    webRCServer.on("/console/cmd", HTTP_POST, []() {
        String cmd = webRCServer.arg("plain");
        cmd.trim();
        if (cmd.length() == 0) {
            webRCServer.send(400, "application/json", "{\"ok\":0,\"e\":\"empty command\"}");
            return;
        }

        char buf[CONSOLE_LINE_LEN];
        snprintf(buf, sizeof(buf), "> %s", cmd.c_str());
        webLog(buf);

        if (!enqueueConsoleCmd(cmd.c_str())) {
            webLog("! command queue is full");
            webRCServer.send(503, "application/json", "{\"ok\":0,\"e\":\"queue full\"}");
            return;
        }

        webRCServer.send(200, "application/json", "{\"ok\":1,\"queued\":1}");
    });

    webRCServer.on("/console/enable", HTTP_POST, []() {
        webConsoleEnabled = true;
        webRCServer.send(200, "application/json", "{\"ok\":1}");
    });

    webRCServer.on("/console/disable", HTTP_POST, []() {
        webConsoleEnabled = false;
        webRCServer.send(200, "application/json", "{\"ok\":1}");
    });

    webRCServer.on("/web_rc/status", HTTP_GET, []() {
        float vbat = readBatteryVoltage();
        if (isnan(vbat) || vbat < 0.0f) vbat = 0.0f;
        char json[384];
        snprintf(json, sizeof(json),
            "{\"enabled\":%s,\"active\":%s,"
            "\"voltage\":%.2f,"
            "\"throttle\":%.1f,\"roll\":%.1f,\"pitch\":%.1f,\"yaw\":%.1f}",
            isWebRCEnabled() ? "true" : "false",
            isUsingWebRC()   ? "true" : "false",
            vbat,
            webRCThrottle, webRCRoll, webRCPitch, webRCYaw);
        webRCServer.send(200, "application/json", json);
    });

    webRCServer.begin();

    // ------ old PCB printed address: :8080 redirects to 80; remove when those are gone ------------
#ifndef CONFIG_IDF_TARGET_ESP32C3
    redirectServer8080 = new WiFiServer(8080); // built on the heap, no BSS
    redirectServer8080->begin(); // light :8080 redirect (WiFiServer), for the address printed on old PCBs
#endif
    // ------ old PCB printed address: :8080 redirects to 80; remove when those are gone ------------

    print("Web RC started: http://192.168.43.42\n");
    print("  dead zone stick=%.0f%% throttle=%.0f%% | scale stick=%.2f yaw=%.2f\n",
          stickDeadzone * 100.0f, throttleDeadzone * 100.0f,
          webRCStickScale, webRCYawScale);
}

// ==================== Main loop ====================

void readWebRC() {
    webRCServer.handleClient();

    // ------ old PCB printed address: :8080 redirects to 80; remove when those are gone ------------
#ifndef CONFIG_IDF_TARGET_ESP32C3
    handleRedirect8080(); // :8080 redirect for old PCBs
#endif
    // ------ old PCB printed address: :8080 redirects to 80; remove when those are gone ------------
    
    if (isWebRCEnabled()) {
        webRCEnabled = useWebRC = true;
    } else {
        webRCEnabled = useWebRC = false;
    }
}

#else
void setupWebRC() { print("Web RC disabled\n"); }
void readWebRC()  {}
void processConsoleCommandQueue() {}
#endif
