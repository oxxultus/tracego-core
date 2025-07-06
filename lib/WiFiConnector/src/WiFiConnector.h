#ifndef WIFI_CONNECTOR_H
#define WIFI_CONNECTOR_H

#include <WiFi.h>

class WiFiConnector {
private:
    const char* ssid;
    const char* password;

public:
    // 기본 생성자: nullptr로 초기화
    WiFiConnector();

    // 매개변수 생성자
    WiFiConnector(const char* ssid, const char* password);

    // 연결 시도
    void connect();
    bool connect(uint32_t timeoutMs); // 선언 추가

    // 연결 상태 확인
    bool isConnected() const;
};

#endif // WIFI_CONNECTOR_H