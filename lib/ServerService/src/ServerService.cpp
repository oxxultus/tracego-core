#include <functional>
#include <WString.h>
#include "ServerService.h"
#include "Config.h"

#include <Preferences.h>
extern Preferences prefs;

// ========== 생성자: 포인터 생성 ==========================================================================
ServerService::ServerService(const int serverPort)
    : serverPort(serverPort)
{
    server = new WebServer(serverPort);
}

// ========== 소멸자: 메모리 해제 ==========================================================================
ServerService::~ServerService() {
    if (server) {
        delete server;
        server = nullptr;
    }
}

// ========== 서버 시작: 라우팅 등록 및 시작 ================================================================
void ServerService::begin() {
    setupRoutes();
    server->begin();
    Serial.println("[ServerService][1/2] TraceGo의 내장 HTTP 서버가 시작되었습니다.");
}

void ServerService::handle() {
    server->handleClient();
}

// ========== 핸들러 등록 ====================================================================================
void ServerService::setStartHandler(const std::function<void()> &handler) { startHandler = handler; }            // 카트 조작
void ServerService::setGoHandler(const std::function<void()> &handler)    { goHandler = handler; }               // 카트 조작
void ServerService::setStopHandler(const std::function<void()> &handler)  { stopHandler = handler; }             // 카트 조작
void ServerService::setResetHandler(const std::function<void()> &handler) { resetHandler = handler; }            // 카트 조작
void ServerService::setPostHandler(const std::function<void(const String&)> &handler) { postHandler = handler; } // 카트 조작

// 설정 페이지 조작
void ServerService::setResetConfigHandler(const std::function<void()> &handler) { resetConfigHandler = handler; }
void ServerService::setStatusHandler(const std::function<String(void)> &handler) { statusHandler = handler; }
void ServerService::setMainPageHandler(std::function<String(void)> handler) { mainPageHandler = handler; }
void ServerService::setUpdateConfigHandler(std::function<String(String)> handler) { updateConfigHandler = handler; }
void ServerService::setAdvancedPageHandler(std::function<String(void)> handler) { advancedPageHandler = handler; }
void ServerService::setStatusViewHandler(std::function<String(void)> handler) { statusViewHandler = handler; }
// ========== GET/POST 요청 전송 =============================================================================
String ServerService::sendGETRequest(const char* host, const uint16_t port, const String& pathWithParams) {
    String response = "";
    WiFiClient client;
    if (client.connect(host, port)) {
        client.print(String("GET ") + pathWithParams + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Connection: close\r\n\r\n");

        unsigned long timeout = millis() + 3000;
        while (client.connected() && millis() < timeout) {
            while (client.available()) {
                response += (char)client.read();
            }
        }
        client.stop();
    }
    return response;
}

String ServerService::sendPostRequest(const char* host, uint16_t port, const String& path, const JsonDocument& jsonDoc) {
    String response = "";
    WiFiClient client;
    if (client.connect(host, port)) {
        String jsonString;
        serializeJson(jsonDoc, jsonString);

        client.print(String("POST ") + path + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Content-Type: application/json\r\n" +
                     "Content-Length: " + jsonString.length() + "\r\n\r\n" +
                     jsonString);

        while (client.connected() || client.available()) {
            if (client.available()) {
                response += client.readStringUntil('\n');
            }
        }
        client.stop();
    }
    return response;
}

// ========== 라우팅 등록 =====================================================================================
void ServerService::setupRoutes() {
    if (startHandler) {
        server->on("/start", HTTP_GET, [this]() {
            startHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "application/json", "{\"message\":\"Handled GET /start\"}");
        });
    }

    if (goHandler) {
        server->on("/go", HTTP_GET, [this]() {
            goHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "application/json", "{\"message\":\"Handled GET /go\"}");
        });
    }

    if (stopHandler) {
        server->on("/stop", HTTP_GET, [this]() {
            stopHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "application/json", "{\"message\":\"Handled GET /stop\"}");
        });
    }

    if (resetHandler) {
        server->on("/reset", HTTP_GET, [this]() {
            resetHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "application/json", "{\"message\":\"Handled GET /reset\"}");
        });
    }
    
    if (postHandler) {
        server->on("/post", HTTP_POST, [this]() {
            String body = server->arg("plain");
            postHandler(body);
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "application/json", "{\"message\":\"Handled POST /post\"}");
        });
    }

    if (statusHandler) {
        server->on("/status", HTTP_GET, [this]() {
            String json = statusHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "application/json", json);
        });
    }

    if (resetConfigHandler) {
        server->on("/reset-config", HTTP_GET, [this]() {
            resetConfigHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "application/json", "{\"message\":\"설정 초기화됨. 재시작합니다.\"}");
            delay(1000);
            ESP.restart();
        });
    }


    if (mainPageHandler) {
        server->on("/", HTTP_GET, [this]() {
            String html = mainPageHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "text/html; charset=utf-8", html);
        });
    }

    if (advancedPageHandler) {
        server->on("/advanced", HTTP_GET, [this]() {
            String html = advancedPageHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "text/html; charset=utf-8", html);
        });
    }

    if (updateConfigHandler) {
    server->on("/update-config", HTTP_POST, [this]() {
        String body = server->arg("plain");
        String result = updateConfigHandler(body);  // ← 여기서 전달
        server->sendHeader("Access-Control-Allow-Origin", "*");
        server->send(200, "application/json", result);
        delay(3000);
        ESP.restart();
    });
}

    if (statusViewHandler) {
        server->on("/status-view", HTTP_GET, [this]() {
            String html = statusViewHandler();
            server->sendHeader("Access-Control-Allow-Origin", "*");
            server->send(200, "text/html; charset=utf-8", html);
        });
    }
    

    // 기본 페이지 핸들러 추가
    /*
    server->on("/", HTTP_GET, [this]() {
        server->send(200, "text/html; charset=utf-8", R"rawliteral(
            <!DOCTYPE html>
            <html lang="ko">
            <head>
                <meta charset="utf-8">
                <title>TraceGo 설정 페이지</title>
                <style>
                    * { box-sizing: border-box; }
                    body {
                        font-family: 'Segoe UI', sans-serif;
                        background-color: #f4f7f8;
                        margin: 0;
                        padding: 0;
                        display: flex;
                        justify-content: center;
                        align-items: center;
                        height: 100vh;
                    }
                    .container {
                        background-color: #fff;
                        padding: 40px;
                        border-radius: 12px;
                        box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
                        text-align: center;
                        width: 100%;
                        max-width: 400px;
                    }
                    h2 {
                        margin-bottom: 30px;
                        color: #00c4c4;
                    }
                    a {
                        display: block;
                        margin: 12px 0;
                        padding: 12px;
                        background-color: #00c4c4;
                        color: #fff;
                        text-decoration: none;
                        border-radius: 8px;
                        font-size: 16px;
                        transition: background-color 0.3s ease;
                    }
                    a:hover {
                        background-color: #00a0a0;
                    }
                </style>
            </head>
            <body>
                <div class="container">
                    <h2>TraceGo 설정 페이지</h2>
                    <a href="/advanced">고급 설정</a>
                    <a href="/status-view">상태 확인</a>
                    <a href="/reset-config">설정 초기화</a>
                </div>
            </body>
            </html>
        )rawliteral");
    });

    server->on("/update-config", HTTP_POST, [this]() {
        JsonDocument doc;
        doc.set(JsonObject());
        DeserializationError err = deserializeJson(doc, server->arg("plain"));
        if (err) {
            server->send(400, "application/json", "{\"message\":\"JSON 파싱 실패\"}");
            return;
        }

        prefs.begin("settings", false);
        prefs.putString("server_ip", doc["server_ip"] | "");
        prefs.putInt("server_port", doc["server_port"] | 8080);
        prefs.putInt("inner_port", doc["inner_port"] | 8081);
        prefs.putString("admin_uid", doc["admin_uid"] | "");
        prefs.putString("master_key", doc["master_key"] | "");
        prefs.putString("test_key", doc["test_key"] | "");
        prefs.putBool("use_rfid", doc["use_rfid"] | false);
        prefs.putInt("comm_rx", doc["comm_rx"] | 16);
        prefs.putInt("comm_tx", doc["comm_tx"] | 17);
        prefs.putInt("rc_sda", doc["rc_sda"] | 5);
        prefs.putInt("rc_rst", doc["rc_rst"] | 22);
        prefs.putInt("baudrate", doc["baudrate"] | 115200);
        prefs.putInt("baudrate2", doc["baudrate2"] | 9600);
        prefs.putString("firstSetWoringLists", doc["firstSetWoringLists"] | "");
        prefs.putString("resetWorkingLists", doc["resetWorkingLists"] | "");
        prefs.putString("getPayment", doc["getPayment"] | "");
        prefs.putString("addWorkingList", doc["addWorkingList"] | "");
        prefs.end();

        server->send(200, "application/json", "{\"message\":\"설정이 저장되었습니다. 3초 후 재시작됩니다.\"}");
        delay(3000);
        ESP.restart();
    });

    server->on("/advanced", HTTP_GET, [this]() {
        String html = R"rawliteral(
            <!DOCTYPE html>
            <html lang="ko">
            <head>
                <meta charset="utf-8">
                <title>고급 설정</title>
                <style>
                    * { box-sizing: border-box; }
                    body {
                        font-family: 'Segoe UI', sans-serif;
                        background-color: #f4f7f8;
                        margin: 0;
                        padding: 0;
                        display: flex;
                        justify-content: center;
                        align-items: flex-start;
                        min-height: 100vh;
                    }
                    .container {
                        width: 100%;
                        max-width: 600px;
                        background: #fff;
                        padding: 30px;
                        margin: 40px auto;
                        border-radius: 12px;
                        box-shadow: 0 4px 10px rgba(0,0,0,0.1);
                    }
                    h2 {
                        text-align: center;
                        color: #00c4c4;
                        margin-bottom: 20px;
                    }
                    fieldset {
                        border: none;
                        margin-bottom: 20px;
                        padding: 0;
                    }
                    legend {
                        font-weight: bold;
                        color: #00a0a0;
                        margin-bottom: 10px;
                    }
                    label {
                        display: block;
                        margin-bottom: 6px;
                        font-weight: 500;
                    }
                    input[type=text],
                    input[type=password],
                    input[type=number] {
                        width: 100%;
                        padding: 10px;
                        margin-bottom: 14px;
                        border: 1px solid #ccc;
                        border-radius: 6px;
                        font-size: 14px;
                    }
                    input[type=checkbox] {
                        transform: scale(1.2);
                        margin-left: 4px;
                    }
                    button {
                        width: 100%;
                        padding: 14px;
                        background-color: #00c4c4;
                        color: #fff;
                        border: none;
                        border-radius: 6px;
                        font-size: 16px;
                        cursor: pointer;
                        transition: background-color 0.3s;
                    }
                    button:hover {
                        background-color: #00a0a0;
                    }
                </style>
                <script>
                function saveConfig() {
                    const config = {
                        server_ip: document.getElementById("server_ip").value,
                        server_port: parseInt(document.getElementById("server_port").value),
                        inner_port: parseInt(document.getElementById("inner_port").value),
                        admin_uid: document.getElementById("admin_uid").value,
                        master_key: document.getElementById("master_key").value,
                        test_key: document.getElementById("test_key").value,
                        use_rfid: document.getElementById("use_rfid").checked,
                        comm_rx: parseInt(document.getElementById("comm_rx").value),
                        comm_tx: parseInt(document.getElementById("comm_tx").value),
                        rc_sda: parseInt(document.getElementById("rc_sda").value),
                        rc_rst: parseInt(document.getElementById("rc_rst").value),
                        baudrate: parseInt(document.getElementById("baudrate").value),
                        baudrate2: parseInt(document.getElementById("baudrate2").value),
                        firstSetWoringLists: document.getElementById("fswl").value,
                        resetWorkingLists: document.getElementById("rwl").value,
                        getPayment: document.getElementById("getpay").value,
                        addWorkingList: document.getElementById("awl").value
                    };

                    fetch("/update-config", {
                        method: "POST",
                        headers: { "Content-Type": "application/json" },
                        body: JSON.stringify(config)
                    })
                    .then(res => res.json())
                    .then(data => alert(data.message));
                }
                </script>
            </head>
            <body>
                <div class="container">
                    <h2>고급 설정</h2>

                    <fieldset>
                        <legend>서버 설정</legend>
                        <label for="server_ip">Server IP</label>
                        <input id="server_ip" value="%SERVER_IP%" type="text">

                        <label for="server_port">Server Port</label>
                        <input id="server_port" value="%SERVER_PORT%" type="number">

                        <label for="inner_port">Inner Port</label>
                        <input id="inner_port" value="%INNER_PORT%" type="number">
                    </fieldset>

                    <fieldset>
                        <legend>보안 설정</legend>
                        <label for="admin_uid">Admin UID</label>
                        <input id="admin_uid" value="%ADMIN_UID%" type="text">

                        <label for="master_key">Master Key</label>
                        <input id="master_key" value="%MASTER_KEY%" type="text">

                        <label for="test_key">Test Key</label>
                        <input id="test_key" value="%TEST_KEY%" type="text">
                    </fieldset>

                    <fieldset>
                        <legend>하드웨어 설정</legend>
                        <label for="use_rfid">
                            <input id="use_rfid" type="checkbox" %USE_RFID%> Use RFID
                        </label>

                        <label for="comm_rx">Comm RX Pin</label>
                        <input id="comm_rx" value="%COMM_RX%" type="number">

                        <label for="comm_tx">Comm TX Pin</label>
                        <input id="comm_tx" value="%COMM_TX%" type="number">

                        <label for="rc_sda">RC SDA Pin</label>
                        <input id="rc_sda" value="%RC_SDA%" type="number">

                        <label for="rc_rst">RC RST Pin</label>
                        <input id="rc_rst" value="%RC_RST%" type="number">

                        <label for="baudrate">Baudrate</label>
                        <input id="baudrate" value="%BAUDRATE%" type="number">

                        <label for="baudrate2">Baudrate2</label>
                        <input id="baudrate2" value="%BAUDRATE2%" type="number">
                    </fieldset>

                    <fieldset>
                        <legend>엔드포인트 설정</legend>
                        <label for="fswl">FirstSetWorkingLists</label>
                        <input id="fswl" value="%FSWL%" type="text">

                        <label for="rwl">ResetWorkingLists</label>
                        <input id="rwl" value="%RWL%" type="text">

                        <label for="getpay">Get Payment</label>
                        <input id="getpay" value="%GETPAY%" type="text">

                        <label for="awl">Add Working List</label>
                        <input id="awl" value="%AWL%" type="text">
                    </fieldset>

                    <button onclick="saveConfig()">설정 저장</button>
                </div>
            </body>
            </html>
        )rawliteral";

        // 치환
        html.replace("%SERVER_IP%", config.serverIP);
        html.replace("%SERVER_PORT%", String(config.serverPort));
        html.replace("%INNER_PORT%", String(config.innerPort));
        html.replace("%ADMIN_UID%", config.adminUID);
        html.replace("%MASTER_KEY%", config.masterKey);
        html.replace("%TEST_KEY%", config.testKey);
        html.replace("%USE_RFID%", config.useRFID ? "checked" : "");
        html.replace("%COMM_RX%", String(config.commRxPin));
        html.replace("%COMM_TX%", String(config.commTxPin));
        html.replace("%RC_SDA%", String(config.rcSdaPin));
        html.replace("%RC_RST%", String(config.rcRstPin));
        html.replace("%BAUDRATE%", String(config.serialBaudrate));
        html.replace("%BAUDRATE2%", String(config.serial2Baudrate));
        html.replace("%FSWL%", config.firstSetWoringLists);
        html.replace("%RWL%", config.resetWorkingLists);
        html.replace("%GETPAY%", config.getPayment);
        html.replace("%AWL%", config.addWorkingList);

        server->send(200, "text/html", html);
    });

    server->on("/status-view", HTTP_GET, [this]() {
        server->send(200, "text/html", R"rawliteral(
            <!DOCTYPE html>
            <html lang="ko">
            <head>
                <meta charset="utf-8">
                <title>시스템 상태</title>
                <style>
                    body { font-family: 'Segoe UI', sans-serif; margin: 20px; background: #f4f7f8; }
                    pre {
                        background: #fff;
                        padding: 20px;
                        border-radius: 10px;
                        box-shadow: 0 4px 8px rgba(0,0,0,0.1);
                        overflow-x: auto;
                        white-space: pre-wrap;
                    }
                    h2 { color: #00c4c4; }
                </style>
            </head>
            <body>
                <h2>시스템 상태</h2>
                <pre id="status">불러오는 중...</pre>

                <script>
                    fetch("/status")
                        .then(response => response.json())
                        .then(data => {
                            document.getElementById("status").textContent = JSON.stringify(data, null, 2);
                        })
                        .catch(error => {
                            document.getElementById("status").textContent = "불러오기 실패: " + error;
                        });
                </script>
            </body>
            </html>
        )rawliteral");
    });
    */
}

// ========== 핸들러 등록 여부 확인 ==========================================================================
bool ServerService::isStartHandlerSet() const { return static_cast<bool>(startHandler); }
bool ServerService::isGoHandlerSet() const { return static_cast<bool>(goHandler); }
bool ServerService::isStopHandlerSet() const { return static_cast<bool>(stopHandler); }
bool ServerService::isResetHandlerSet() const { return static_cast<bool>(resetHandler); }

bool ServerService::isStatusHandlerSet() const { return static_cast<bool>(statusHandler); }
bool ServerService::isResetConfigHandlerSet() const { return static_cast<bool>(resetConfigHandler); }
bool ServerService::isMainPageHandlerSet() const { return static_cast<bool>(mainPageHandler); }
bool ServerService::isUpdateConfigHandlerSet() const { return static_cast<bool>(updateConfigHandler); }
bool ServerService::isAdvancedPageHandlerSet() const { return static_cast<bool>(advancedPageHandler); }
bool ServerService::isStatusViewHandlerSet() const { return static_cast<bool>(statusViewHandler); }