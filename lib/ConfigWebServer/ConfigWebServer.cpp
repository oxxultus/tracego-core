#include "ConfigWebServer.h"
#include <Preferences.h>

extern Preferences prefs;

ConfigWebServer::ConfigWebServer(Config& cfg, int port)
    : server(port), config(cfg) {}

void ConfigWebServer::begin() {
    server.on("/", HTTP_GET, std::bind(&ConfigWebServer::handleRoot, this));
    server.on("/config", HTTP_POST, std::bind(&ConfigWebServer::handleSave, this));
    server.onNotFound(std::bind(&ConfigWebServer::handleNotFound, this));
    server.begin();
}

void ConfigWebServer::handleClient() {
    server.handleClient();
}

void ConfigWebServer::handleRoot() {
    String html = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="utf-8">
            <title>WiFi 설정</title>
            <style>
                body { font-family: Arial; margin: 20px; }
                input[type=text], input[type=password] { width: 100%; padding: 8px; margin: 4px 0; }
                input[type=submit] { padding: 10px 20px; }
            </style>
        </head>
        <body>
            <h2>WiFi 설정</h2>
            <form action="/config" method="post">
                SSID: <input name="ssid" value="%SSID%"><br>
                Password: <input name="password" type="password" value="%PASSWORD%"><br>
                <input type="submit" value="저장">
            </form>
        </body>
        </html>
    )rawliteral";

    html.replace("%SSID%", config.ssid);
    html.replace("%PASSWORD%", config.password);

    server.send(200, "text/html; charset=utf-8", html);
}

void ConfigWebServer::handleSave() {
    prefs.begin("settings", false);
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("password", server.arg("password"));
    prefs.end();

    server.send(200, "text/html; charset=utf-8", R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head><meta charset="utf-8"><title>저장 완료</title></head>
        <body>
            <h3>설정이 저장되었습니다.</h3>
            <p>3초 후 장치가 자동으로 재시작됩니다.</p>
        </body>
        </html>
    )rawliteral");

    delay(3000);
    ESP.restart();  // 재부팅
}

void ConfigWebServer::handleNotFound() {
    server.send(404, "text/plain", "Not found");
}