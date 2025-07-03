// ConfigWebServer.h
#ifndef CONFIG_WEB_SERVER_H
#define CONFIG_WEB_SERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "Config.h"

class ConfigWebServer {
private:
    WebServer server;
    Config& config;  // 외부에서 참조하는 Config 객체
    Preferences prefs;

    void handleRoot();        // 설정 입력 폼
    void handleSave();        // 설정 저장 처리
    void handleNotFound();    // 404 페이지

public:
    ConfigWebServer(Config& cfg, int port = 80);
    void begin();
    void handleClient();
};

#endif