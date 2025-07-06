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
        <html lang="ko">
        <head>
            <meta charset="utf-8">
            <title>Wi-Fi 설정</title>
            <style>
                * {
                    box-sizing: border-box;
                }
                body {
                    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                    background-color: #f4f7f8;
                    color: #333;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                    height: 100vh;
                    margin: 0;
                }
                .container {
                    background-color: #fff;
                    padding: 40px;
                    border-radius: 12px;
                    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
                    width: 100%;
                    max-width: 400px;
                }
                h2 {
                    margin-bottom: 24px;
                    color: #00c4c4;
                    text-align: center;
                }
                label {
                    display: block;
                    margin-bottom: 6px;
                    font-weight: 600;
                }
                input[type=text], input[type=password] {
                    width: 100%;
                    padding: 10px;
                    margin-bottom: 16px;
                    border: 1px solid #ccc;
                    border-radius: 6px;
                    font-size: 14px;
                    box-sizing: border-box;
                }
                input[type=submit] {
                    background-color: #00c4c4;
                    color: white;
                    border: none;
                    padding: 12px;
                    width: 100%;
                    border-radius: 6px;
                    font-size: 16px;
                    cursor: pointer;
                    transition: background-color 0.3s ease;
                }
                input[type=submit]:hover {
                    background-color: #00a0a0;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h2>Wi-Fi 설정</h2>
                <form action="/config" method="post">
                    <label for="ssid">SSID</label>
                    <input id="ssid" name="ssid" type="text" value="%SSID%" required>

                    <label for="password">Password</label>
                    <input id="password" name="password" type="password" value="%PASSWORD%">

                    <input type="submit" value="저장">
                </form>
            </div>
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