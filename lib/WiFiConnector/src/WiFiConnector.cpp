#include "WiFiConnector.h"
#include <Arduino.h>  // Serial 관련

WiFiConnector::WiFiConnector()
    : ssid(""), password("") {}  // 초기화 목록 사용

WiFiConnector::WiFiConnector(const char* ssid, const char* password)
    : ssid(ssid), password(password) {}

void WiFiConnector::connect() {
    if (!ssid || !password || strlen(ssid) == 0) {
        Serial.println("[WiFiConnector][ERROR] SSID 또는 Password가 비어 있습니다.");
        return;
    }

    WiFi.begin(ssid, password);
    Serial.println("[WiFiConnector][1/2] WiFi 연결 시도 중...");

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFiConnector][2/2] 연결 성공! IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\n[WiFiConnector][2/2] 연결 실패! 연결 제한 초과");
    }
}
bool WiFiConnector::connect(uint32_t timeoutMs) {
    if (!ssid || !password || strlen(ssid) == 0) {
        Serial.println("[WiFiConnector][ERROR] SSID 또는 Password가 비어 있습니다.");
        return false;
    }

    WiFi.begin(ssid, password);
    Serial.println("[WiFiConnector][1/2] WiFi 연결 시도 중...");

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(100);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFiConnector][2/2] 연결 성공! IP: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\n[WiFiConnector][2/2] 연결 실패! 연결 제한 초과");
        return false;
    }
}

bool WiFiConnector::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}