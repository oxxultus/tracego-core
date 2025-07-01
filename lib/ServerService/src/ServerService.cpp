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
    Serial.println("[ServerService][1/2] Delivery BOT의 내장 HTTP 서버가 시작되었습니다.");
}

void ServerService::handle() {
    server->handleClient();
}

// ========== 핸들러 등록 ====================================================================================
void ServerService::setStartHandler(const std::function<void()> &handler) { startHandler = handler; }
void ServerService::setGoHandler(const std::function<void()> &handler)    { goHandler = handler; }
void ServerService::setStopHandler(const std::function<void()> &handler)  { stopHandler = handler; }
void ServerService::setResetHandler(const std::function<void()> &handler) { resetHandler = handler; }
void ServerService::setPostHandler(const std::function<void(const String&)> &handler) { postHandler = handler; }
void ServerService::setStatusHandler(const std::function<String(void)> &handler) { statusHandler = handler; }
void ServerService::setResetConfigHandler(const std::function<void()> &handler) { resetConfigHandler = handler; }
void ServerService::setUpdateConfigHandler(const std::function<void(const String&)>& handler) { updateConfigHandler = handler; }
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
    // 기본 페이지 핸들러 추가
    server->on("/", HTTP_GET, [this]() {
        server->send(200, "text/html; charset=utf-8", R"rawliteral(
            <!DOCTYPE html>
            <html><head><meta charset="utf-8"><title>설정 페이지</title></head>
            <body>
                <h2>Delivery BOT 설정 페이지</h2>
                <ul>
                    <li><a href="/advanced">고급 설정</a></li>
                    <li><a href="/status">상태 확인</a></li>
                    <li><a href="/reset-config">설정 초기화</a></li>
                </ul>
            </body>
            </html>
        )rawliteral");
    });
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

    // 사용안함
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
        <html>
        <head>
            <meta charset="utf-8">
            <title>고급 설정</title>
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
            <h2>고급 설정</h2>
            <fieldset><legend>서버 설정</legend>
            Server IP: <input id="server_ip" value="%SERVER_IP%"><br>
            Server Port: <input id="server_port" type="number" value="%SERVER_PORT%"><br>
            Inner Port: <input id="inner_port" type="number" value="%INNER_PORT%"><br>
            </fieldset>

            <fieldset><legend>보안 설정</legend>
            Admin UID: <input id="admin_uid" value="%ADMIN_UID%"><br>
            Master Key: <input id="master_key" value="%MASTER_KEY%"><br>
            Test Key: <input id="test_key" value="%TEST_KEY%"><br>
            </fieldset>

            <fieldset><legend>하드웨어 설정</legend>
            Use RFID: <input id="use_rfid" type="checkbox" %USE_RFID%><br>
            Comm RX Pin: <input id="comm_rx" type="number" value="%COMM_RX%"><br>
            Comm TX Pin: <input id="comm_tx" type="number" value="%COMM_TX%"><br>
            RC SDA Pin: <input id="rc_sda" type="number" value="%RC_SDA%"><br>
            RC RST Pin: <input id="rc_rst" type="number" value="%RC_RST%"><br>
            Baudrate: <input id="baudrate" type="number" value="%BAUDRATE%"><br>
            Baudrate2: <input id="baudrate2" type="number" value="%BAUDRATE2%"><br>
            </fieldset>

            <fieldset><legend>엔드포인트 설정</legend>
            FirstSetWorkingLists: <input id="fswl" value="%FSWL%"><br>
            ResetWorkingLists: <input id="rwl" value="%RWL%"><br>
            Get Payment: <input id="getpay" value="%GETPAY%"><br>
            Add Working List: <input id="awl" value="%AWL%"><br>
            </fieldset>

            <button onclick="saveConfig()">설정 저장</button>
        </body>
        </html>
        )rawliteral";

        // 현재 config 값 치환
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
}

// ========== 핸들러 등록 여부 확인 ==========================================================================
bool ServerService::isStartHandlerSet() const { return static_cast<bool>(startHandler); }
bool ServerService::isGoHandlerSet() const { return static_cast<bool>(goHandler); }
bool ServerService::isStopHandlerSet() const { return static_cast<bool>(stopHandler); }
bool ServerService::isResetHandlerSet() const { return static_cast<bool>(resetHandler); }
bool ServerService::isStatusHandlerSet() const { return static_cast<bool>(statusHandler); }
bool ServerService::isResetConfigHandlerSet() const { return static_cast<bool>(resetConfigHandler); }