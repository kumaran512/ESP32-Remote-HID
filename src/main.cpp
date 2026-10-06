/**
 * ESP32 Remote HID
 * Based on ESP32 WiFi Virtual HID by Lee Chee Yong.
 *
 * ESP32-S3 N16R8: Wi-Fi WebUI + WebSocket HID control + native USB HID.
 * Licensed under MIT; see original project for original copyright/license.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDMouse.h>
#include <USBHIDKeyboard.h>
#include <ESPAsyncWebServer.h>
#include <html.h>

USBHIDMouse Mouse;
USBHIDKeyboard Keyboard;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Preferences preferences;

String ssid = "ESP32 Remote HID";
String password = "password123";
String startupBody;

static void sendOK(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void safeMouseMove(int x, int y, int wheel = 0) {
    while (x != 0 || y != 0 || wheel != 0) {
        int mx = constrain(x, -127, 127);
        int my = constrain(y, -127, 127);
        int mw = constrain(wheel, -127, 127);
        Mouse.move(mx, my, mw);
        x -= mx;
        y -= my;
        wheel -= mw;
    }
}

uint8_t resolveKey(int code) {
    switch (code) {
        case 0xB0: return KEY_RETURN;
        case 0xB1: return KEY_ESC;
        case 0xB2: return KEY_BACKSPACE;
        case 0xB3: return KEY_TAB;
        case 0xD1: return KEY_INSERT;
        case 0xD2: return KEY_HOME;
        case 0xD3: return KEY_PAGE_UP;
        case 0xD4: return KEY_DELETE;
        case 0xD5: return KEY_END;
        case 0xD6: return KEY_PAGE_DOWN;
        case 0xD7: return KEY_RIGHT_ARROW;
        case 0xD8: return KEY_LEFT_ARROW;
        case 0xD9: return KEY_DOWN_ARROW;
        case 0xDA: return KEY_UP_ARROW;
        case 0x80: return KEY_LEFT_CTRL;
        case 0x81: return KEY_LEFT_SHIFT;
        case 0x82: return KEY_LEFT_ALT;
        case 0x83: return KEY_LEFT_GUI;
        case 0x84: return KEY_RIGHT_CTRL;
        case 0x85: return KEY_RIGHT_SHIFT;
        case 0x86: return KEY_RIGHT_ALT;
        case 0x87: return KEY_RIGHT_GUI;
        case 0xC1: return KEY_CAPS_LOCK;
        case 0xC2: return KEY_F1;
        case 0xC3: return KEY_F2;
        case 0xC4: return KEY_F3;
        case 0xC5: return KEY_F4;
        case 0xC6: return KEY_F5;
        case 0xC7: return KEY_F6;
        case 0xC8: return KEY_F7;
        case 0xC9: return KEY_F8;
        case 0xCA: return KEY_F9;
        case 0xCB: return KEY_F10;
        case 0xCC: return KEY_F11;
        case 0xCD: return KEY_F12;
        default: return (uint8_t)code;
    }
}

void executeCommand(String line) {
    line.trim();
    if (line.length() == 0 || line.startsWith("REM")) return;

    int spaceIdx = line.indexOf(' ');
    String cmd = spaceIdx != -1 ? line.substring(0, spaceIdx) : line;
    String arg = spaceIdx != -1 ? line.substring(spaceIdx + 1) : "";

    if (cmd == "STRING") Keyboard.print(arg);
    else if (cmd == "ENTER") Keyboard.write(KEY_RETURN);
    else if (cmd == "TAB") Keyboard.write(KEY_TAB);
    else if (cmd == "SPACE") Keyboard.write(' ');
    else if (cmd == "UP") Keyboard.write(KEY_UP_ARROW);
    else if (cmd == "DOWN") Keyboard.write(KEY_DOWN_ARROW);
    else if (cmd == "LEFT") Keyboard.write(KEY_LEFT_ARROW);
    else if (cmd == "RIGHT") Keyboard.write(KEY_RIGHT_ARROW);
    else if (cmd == "GUI" || cmd == "WINDOWS") {
        Keyboard.press(KEY_LEFT_GUI);
        if (arg.length() > 0) Keyboard.press(arg.charAt(0));
        Keyboard.releaseAll();
    }
    else if (cmd == "CTRL" || cmd == "CONTROL") {
        Keyboard.press(KEY_LEFT_CTRL);
        if (arg.length() > 0) Keyboard.press(arg.charAt(0));
        Keyboard.releaseAll();
    }
    else if (cmd == "ALT") {
        Keyboard.press(KEY_LEFT_ALT);
        if (arg.length() > 0) Keyboard.press(arg.charAt(0));
        Keyboard.releaseAll();
    }
    else if (cmd == "SHIFT") {
        Keyboard.press(KEY_LEFT_SHIFT);
        if (arg.length() > 0) Keyboard.press(arg.charAt(0));
        Keyboard.releaseAll();
    }
    else if (cmd == "DELAY") {
        delay(arg.toInt());
    }
    else if (cmd == "MOUSE_MOVE") {
        int split = arg.indexOf(' ');
        if (split != -1) safeMouseMove(arg.substring(0, split).toInt(), arg.substring(split + 1).toInt(), 0);
    }
    else if (cmd == "MOUSE_SCROLL") safeMouseMove(0, 0, arg.toInt());
    else if (cmd == "MOUSE_CLICK") {
        if (arg == "LEFT") Mouse.click(MOUSE_LEFT);
        else if (arg == "RIGHT") Mouse.click(MOUSE_RIGHT);
        else if (arg == "MIDDLE") Mouse.click(MOUSE_MIDDLE);
    }
    else if (cmd == "MOUSE_DOWN") Mouse.press(MOUSE_LEFT);
    else if (cmd == "MOUSE_UP") Mouse.release(MOUSE_LEFT);
    else if (cmd == "MOUSE_RESET") {
        for (int i = 0; i < 30; i++) safeMouseMove(-127, -127, 0);
    }
}

void handleHIDMessage(const String &msg) {
    if (msg.length() < 1) return;

    const char type = msg.charAt(0);

    if (type == 'M') {
        int c1 = msg.indexOf(',');
        int c2 = msg.indexOf(',', c1 + 1);
        int c3 = msg.indexOf(',', c2 + 1);
        if (c1 > 0 && c2 > c1 && c3 > c2) {
            int dx = msg.substring(c1 + 1, c2).toInt();
            int dy = msg.substring(c2 + 1, c3).toInt();
            int wheel = msg.substring(c3 + 1).toInt();
            safeMouseMove(dx, dy, wheel);
        }
    }
    else if (type == 'B') {
        // B,<button>,<action> where button=L/R/M and action=click/down/up
        int c1 = msg.indexOf(',');
        int c2 = msg.indexOf(',', c1 + 1);
        if (c1 < 0 || c2 < 0) return;
        char button = msg.charAt(c1 + 1);
        String action = msg.substring(c2 + 1);
        uint8_t mouseButton = button == 'R' ? MOUSE_RIGHT : (button == 'M' ? MOUSE_MIDDLE : MOUSE_LEFT);
        if (action == "click") Mouse.click(mouseButton);
        else if (action == "down") Mouse.press(mouseButton);
        else if (action == "up") Mouse.release(mouseButton);
    }
    else if (type == 'P' || type == 'R') {
        int code = msg.substring(1).toInt();
        uint8_t key = resolveKey(code);
        if (type == 'P') Keyboard.press(key);
        else Keyboard.release(key);
    }
    else if (type == 'T') {
        Keyboard.print(msg.substring(1));
    }
    else if (type == 'G') {
        String gesture = msg.substring(2); // skip G,
        if (gesture == "3U") {
            Keyboard.press(KEY_LEFT_GUI); Keyboard.press(KEY_TAB); Keyboard.releaseAll();
        } else if (gesture == "3D") {
            Keyboard.press(KEY_LEFT_GUI); Keyboard.press('d'); Keyboard.releaseAll();
        } else if (gesture == "3L" || gesture == "3R") {
            Keyboard.press(KEY_LEFT_ALT); Keyboard.press(KEY_TAB); Keyboard.releaseAll();
        }
    }
}

void onWsEvent(AsyncWebSocket *serverPtr, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    (void)serverPtr;
    (void)client;

    if (type != WS_EVT_DATA) return;

    AwsFrameInfo *info = reinterpret_cast<AwsFrameInfo *>(arg);
    if (!info || info->opcode != WS_TEXT || !info->final || info->index != 0 || info->len != len) return;

    String msg(reinterpret_cast<char *>(data), len);
    handleHIDMessage(msg);
}

void setup() {
    Serial.begin(115200);
    delay(100);

    Mouse.begin();
    Keyboard.begin();
    USB.begin();

    preferences.begin("airpad", false);
    ssid = preferences.getString("wifi_ssid", ssid);
    password = preferences.getString("wifi_pass", password);

    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.setHostname("esp32hid");
    WiFi.softAP(ssid.c_str(), password.c_str());

    if (MDNS.begin("esp32hid")) {
        MDNS.addService("http", "tcp", 80);
    }

    // Preserve the original optional startup script behavior.
    String bootScript = preferences.getString("boot_script", "");
    if (bootScript.length() > 0) {
        delay(3000);
        char *buffer = strdup(bootScript.c_str());
        if (buffer) {
            char *line = strtok(buffer, "\n");
            while (line != nullptr) {
                executeCommand(String(line));
                line = strtok(nullptr, "\n");
            }
            free(buffer);
        }
    }

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html; charset=utf-8", html);
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });

    // Keep the original HTTP endpoints for compatibility and Ducky/script features.
    server.on("/startup", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (startupBody.length() == 0) preferences.remove("boot_script");
        else preferences.putString("boot_script", startupBody);
        startupBody = "";
        sendOK(request);
    }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        (void)request;
        if (index == 0) startupBody = "";
        startupBody.concat(reinterpret_cast<const char *>(data), len);
        if (index + len >= total) {
            // Body is complete; the request handler will save it.
        }
    });

    server.on("/m", HTTP_GET, [](AsyncWebServerRequest *request) {
        int dx = request->getParam("dx") ? request->getParam("dx")->value().toInt() : 0;
        int dy = request->getParam("dy") ? request->getParam("dy")->value().toInt() : 0;
        int s  = request->getParam("s")  ? request->getParam("s")->value().toInt()  : 0;
        safeMouseMove(dx, dy, s); sendOK(request);
    });
    server.on("/cl", HTTP_GET, [](AsyncWebServerRequest *request) { Mouse.click(MOUSE_LEFT); sendOK(request); });
    server.on("/cr", HTTP_GET, [](AsyncWebServerRequest *request) { Mouse.click(MOUSE_RIGHT); sendOK(request); });
    server.on("/md", HTTP_GET, [](AsyncWebServerRequest *request) { Mouse.press(MOUSE_LEFT); sendOK(request); });
    server.on("/mu", HTTP_GET, [](AsyncWebServerRequest *request) { Mouse.release(MOUSE_LEFT); sendOK(request); });
    server.on("/k", HTTP_GET, [](AsyncWebServerRequest *request) { if (request->hasParam("text")) Keyboard.print(request->getParam("text")->value()); sendOK(request); });
    server.on("/kb", HTTP_GET, [](AsyncWebServerRequest *request) { Keyboard.write(KEY_BACKSPACE); sendOK(request); });
    server.on("/en", HTTP_GET, [](AsyncWebServerRequest *request) { Keyboard.write(KEY_RETURN); sendOK(request); });
    server.on("/kp", HTTP_GET, [](AsyncWebServerRequest *request) { if (request->hasParam("c")) Keyboard.press(resolveKey(request->getParam("c")->value().toInt())); sendOK(request); });
    server.on("/kr", HTTP_GET, [](AsyncWebServerRequest *request) { if (request->hasParam("c")) Keyboard.release(resolveKey(request->getParam("c")->value().toInt())); sendOK(request); });
    server.on("/g", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("v")) handleHIDMessage(String("G,") + request->getParam("v")->value());
        sendOK(request);
    });
    server.on("/ducky", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("cmd")) executeCommand(request->getParam("cmd")->value());
        sendOK(request);
    });
    server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool changed = false;
        if (request->hasParam("ssid", true)) {
            String v = request->getParam("ssid", true)->value();
            if (v.length()) { preferences.putString("wifi_ssid", v); changed = true; }
        }
        if (request->hasParam("pass", true)) {
            preferences.putString("wifi_pass", request->getParam("pass", true)->value());
            changed = true;
        }
        if (!changed) { request->send(400, "text/plain", "Bad Request"); return; }
        sendOK(request);
        delay(300);
        ESP.restart();
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });

    server.begin();

    Serial.println();
    Serial.println("ESP32 Remote HID ready");
    Serial.print("AP SSID: "); Serial.println(ssid);
    Serial.print("IP: "); Serial.println(WiFi.softAPIP());
    Serial.println("mDNS: http://esp32hid.local");
}

void loop() {
    // AsyncWebServer/WebSocket runs outside the Arduino loop.
    ws.cleanupClients();
    delay(1);
}
