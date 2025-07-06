#include <Arduino.h>
#include <Preferences.h>
#include <HTTPClient.h>

#include "Config.h"
#include "ConfigWebServer.h"
#include "CommLink.h"
#include "ServerService.h"
#include "RFIDController.h"
#include "WiFiConnector.h"

#include "model/PaymentData.h" // 구조체, 클래스
// 함수 선언부 ===========================================================================================================
bool sendWithRetry(const String& cmd, const int retries = 3);       // [UTILITY-1] 명령 전송 함수 (재시도 포함)
void simpleMessage(String message);                                 // [UTILITY-2] 간편 메시지 사용 메서드
void sendStartStandRequest(const String& detectedUid);              // [UTILITY-3] /start-stand?uid= 요청을 전송하는 함수
void sendUpRfidCardRequest(const String& detectedUid);              // [UTILITY-4] /up-rfid?uid= 요청을 전송하는 함수
bool isAdminCard(const String& uid);                                // [LOOP-1] 관리자 카드 여부 판별
bool refreshPaymentData(int maxRetries = 3);                        // [LOOP-2] 결제 내역 초기화 및 재요청 로직
bool fetchPaymentDataUntilSuccess(const int count);                 // [LOOP-3] 외부 서버로 GET 요청 전송해 결제 내역을 받아온다.
void handleMatchedProduct(const String& matchedName);               // [LOOP-4] 상품 매칭 시 동작을 처리하는 함수
void checkDetectedUid();                                            // [LOOP-5] UID를 인식해서 결제내역 확인 하는 함수
void modulsSetting();                                               // [SETUP-1] 모듈을 초기 설정 하는 함수입니다.
void setServerHandler();                                            // [SETUP-2] 핸들러 등록을 진행하는 함수입니다.

// 객체 생성 =============================================================================================================
WiFiConnector wifi;                             // WiFiConnect 객체 생성
ServerService* serverService = nullptr;         // WebService 객체 생성
RFIDController* rfidController = nullptr;       // RFIDController 객체 생성
ConfigWebServer* configWebServer = nullptr;     // ConfigWebServer 객체 생성
PaymentData payment;                            // 결제 내역 저장

// 프로그램 설정 및 시작 ====================================================================================================

void setup() {
    config.load(); // EEPROM 또는 Preferences에서 구성 불러오기

    Serial.begin(config.serialBaudrate);  // 시리얼 초기화 (최우선)

    // 객체 동적 생성
    wifi = WiFiConnector(config.ssid.c_str(), config.password.c_str());
    if (!wifi.connect(5000)) {  // 5초 내 미연결 시 설정 모드 전환
        Serial.println("[WiFi] 연결 실패. 설정 모드로 진입합니다.");

        WiFi.mode(WIFI_AP);
        WiFi.softAP("TraceGo_Core_Setting", "12345678");  // SoftAP 시작
        Serial.println("[WiFi] SoftAP 모드 활성화: SSID = TraceGo_Core_Setting 접속 IP: " + WiFi.softAPIP().toString());
        
        configWebServer = new ConfigWebServer(config);
        configWebServer->begin();
        return; // loop에서 configWeb 핸들러로 진입하게 됨
    }
    serverService = new ServerService(config.innerPort);
    rfidController = new RFIDController(config.rcSdaPin, config.rcRstPin);

    modulsSetting();           // 모듈 초기 설정 (Serial2, RFID, WiFi 등)
    setServerHandler();        // 서버 핸들러 등록
    serverService->begin();    // 서버 시작

    Serial.println("[TraceGo][MAIN] 메인 모듈 준비 완료");
    simpleMessage("종료선");
}

void loop() {
    if (configWebServer) {
        configWebServer->handleClient();  // 설정 모드일 경우 처리
        return;
    }

    if (serverService) {serverService->handle();} // 1. 내장 서버 구동
    checkDetectedUid();         // 2. UID를 인식해서 결제내역 확인 하는 함수
    delay(1);                   // 3. WDT 리셋 방지
}

// SETUP FUNCTION =====================================================================================================

// [SETUP-1] 모듈을 초기 설정 하는 함수입니다.

void modulsSetting() {
    Serial.begin(config.serialBaudrate);   // 시리얼 설정

    // 함수: [UTILITY-2]
    simpleMessage("시작선");
    if (config.useRFID) {
        rfidController->begin(Serial); // RFID 리더기 초기화
    } else {
        Serial.println("[INFO] RFID 리더기 비활성화됨 (하드웨어 없음)");
    }
    Serial2.begin(config.serial2Baudrate);

    wifi.connect();             // wifi 연결
}

// [SETUP-2] 핸들러 등록을 진행하는 함수입니다.
void setServerHandler() {

    // [봇 조작 핸들러] 자동화 카트에게 시작 명령을 내리는 핸들러입니다.
    serverService->setStartHandler([]() {
        Serial.println("[ServerService][GET /start] 로봇 시작 명령 수신");
        
        payment.clear();

        // 결제 내역이 없으면 수신 시도
        if (payment.getPaymentId() == "") {
            Serial.println("[ServerService] 결제 내역 없음 → 새로 요청");
            const bool result = fetchPaymentDataUntilSuccess(5);
            if (!result || payment.getPaymentId() == "") {
                Serial.println("[ServerService][BLOCKED] 서버에 결제 내역 없음 → 시작 차단됨");
                return;
            }
        }

        // 작업 리스트 전송 (GET 방식)
        String getResponse = ServerService::sendGETRequest(config.serverIP.c_str(), config.serverPort, config.firstSetWoringLists);
        Serial.println("[응답] " + getResponse);

        if (getResponse.indexOf("초기 작업 리스트 생성 완료") == -1 && getResponse.indexOf("200 OK") == -1) {
            Serial.println("[ServerService][BLOCKED] 작업 리스트 설정 실패 → 로봇 시작 차단됨");
            return;
        }

        // 결제 내역도 존재하고, 작업 리스트도 성공적으로 설정된 경우
        Serial.println("[ServerService][START] 결제 내역 및 작업 리스트 준비 완료 → 로봇 시작");
        sendWithRetry("START");  // 로봇 시작 명령
    });
    
    // [봇 조작 핸들러] 자동화 카트에게 이동 명령을 내리는 핸들러입니다.
    serverService->setGoHandler([]() {
        Serial.println("[ServerService][GET /go] 로봇 이동 명령 수신");
        sendWithRetry("GO"); // 함수: [UTILITY-1]
    });

    // [봇 조작 핸들러] 자동화 카트에게 정지 명령을 내리는 핸들러입니다.
    serverService->setStopHandler([]() {
        Serial.println("[ServerService][GET /stop] 로봇 정지 명령 수신");
        sendWithRetry("STOP"); // 함수: [UTILITY-1]
    });

    // [봇 조작 핸들러] 자동화 카트에게 초기화 명령을 내리는 핸들러입니다.
    serverService->setResetHandler([]() {
        Serial.println("[ServerService][GET /reset] 로봇 정지 명령 수신");

        // 결제 내역 초기화
        payment.clear();

        // 서버에 작업 리스트 초기화 요청
        String getResponse = ServerService::sendGETRequest(config.serverIP.c_str(), config.serverPort, config.resetWorkingLists);
        Serial.println("[응답] " + getResponse);

        // 응답 메시지 기반 판단
        if (getResponse.indexOf("초기화했습니다") == -1 && getResponse.indexOf("200 OK") == -1) {
            Serial.println("[ServerService][BLOCKED] 작업 리스트 초기화 실패 → 로봇 정지 차단됨");
            return;
        }

        sendWithRetry("STOP");  // 로봇 정지 명령 전송
    });
    
    // [메인 페이지 핸들러] 기본 설정 페이지를 반환하는 핸들러입니다.
    serverService->setMainPageHandler([]() -> String {
        return R"rawliteral(
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
        )rawliteral";
    });

    // [고급 설정 핸들러] 고급 설정 페이지를 반환하는 핸들러입니다.
    serverService->setAdvancedPageHandler([]() -> String {
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
                        stand_port: parseInt(document.getElementById("stand_port").value),
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

                        <label for="stand_port">Stand Port</label>
                        <input id="stand_port" value="%STAND_PORT%" type="number">
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
        html.replace("%STAND_PORT%", String(config.standPort));
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

        return html;
    });

    // [고급 설정 핸들러] 고급 설정 변경사항을 저장하는 핸들러입니다.
    serverService->setUpdateConfigHandler([](String body) -> String {
        JsonDocument doc;
        doc.set(JsonObject());
        DeserializationError err = deserializeJson(doc, body);
        if (err) return "{\"message\":\"JSON 파싱 실패\"}";

        extern Preferences prefs;
        prefs.begin("settings", false);
        prefs.putString("server_ip", doc["server_ip"] | "");
        prefs.putInt("server_port", doc["server_port"] | 8080);
        prefs.putInt("inner_port", doc["inner_port"] | 8081);
        prefs.putInt("stand_port", doc["stand_port"] | 8082);
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
        prefs.putString("fswl", doc["firstSetWoringLists"] | "");
        prefs.putString("rwl",  doc["resetWorkingLists"]   | "");
        prefs.putString("gpay", doc["getPayment"]          | "");
        prefs.putString("awl",  doc["addWorkingList"]      | "");
        prefs.end();

        return "{\"message\":\"설정이 저장되었습니다. 3초 후 재시작됩니다.\"}";
    });

    // [상태 핸들러] 현재 시스템 상태를 JSON 형태로 반환하는 핸들러입니다.
    serverService->setStatusHandler([]() -> String {
        JsonDocument doc;  // 권장된 JsonDocument 타입 사용
        doc.set(JsonObject());  // 명시적 초기화 (v7에서는 안전하게 사용하기 위해 권장됨)

        doc["ssid"]                 = config.ssid;
        doc["password"]             = config.password;
        doc["server_ip"]            = config.serverIP;
        doc["server_port"]          = config.serverPort;
        doc["inner_port"]           = config.innerPort;
        doc["stand_port"]           = config.standPort;
        doc["admin_uid"]            = config.adminUID;
        doc["master_key"]           = config.masterKey;
        doc["test_key"]             = config.testKey;
        doc["use_rfid"]             = config.useRFID;
        doc["comm_rx"]              = config.commRxPin;
        doc["comm_tx"]              = config.commTxPin;
        doc["rc_sda"]               = config.rcSdaPin;
        doc["rc_rst"]               = config.rcRstPin;
        doc["baudrate"]             = config.serialBaudrate;
        doc["baudrate2"]            = config.serial2Baudrate;
        doc["firstSetWoringLists"]  = config.firstSetWoringLists;
        doc["resetWorkingLists"]    = config.resetWorkingLists;
        doc["getPayment"]           = config.getPayment;
        doc["addWorkingList"]       = config.addWorkingList;
        doc["localIP"]              = config.localIP;
        
        String output;
        serializeJson(doc, output);
        return output;
    });

    // [상태 뷰 핸들러] 시스템 상태를 HTML로 표시하는 핸들러입니다.
    serverService->setStatusViewHandler([]() -> String {
        return  R"rawliteral(
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
            )rawliteral";
    });

    // [설정 초기화 핸들러] 모든 설정을 초기화하는 핸들러입니다.
    serverService->setResetConfigHandler([]() {
        Preferences prefs;
        prefs.begin("settings", false);
        prefs.clear();  // 모든 설정 삭제
        prefs.end();
    });

    Serial.println("[setServerHandler][1/2] 내장 서버 API 실행 함수 등록 확인 절차 시작");
        auto printHandlerStatus = [](const char* name, bool status) {
            Serial.print("[");
            Serial.print(status ? "\u2714" : "\u2718"); // ✔ 또는 ✘
            Serial.print("] ");
            Serial.print(name);
            Serial.println(" 핸들러 등록 " + String(status ? "완료" : "실패"));
        };
        printHandlerStatus("/start", serverService->isStartHandlerSet());
        printHandlerStatus("/go",    serverService->isGoHandlerSet());
        printHandlerStatus("/stop",  serverService->isStopHandlerSet());
        printHandlerStatus("/reset", serverService->isStartHandlerSet());
        printHandlerStatus("/", serverService->isMainPageHandlerSet());
        printHandlerStatus("/advanced",  serverService->isAdvancedPageHandlerSet());
        printHandlerStatus("/update-config", serverService->isUpdateConfigHandlerSet());
        printHandlerStatus("/status",    serverService->isStatusHandlerSet());
        printHandlerStatus("/status-view", serverService->isStatusViewHandlerSet());
        printHandlerStatus("/reset-config",  serverService->isResetConfigHandlerSet());
    Serial.println("[setServerHandler][2/2] 내장 서버 API 실행 함수 등록 절차 완료\n");
}

// LOOP FUNCTION =======================================================================================================

// [LOOP-1] 관리자 카드 여부 판별
bool isAdminCard(const String& uid) {
    return uid == config.adminUID || uid == config.masterKey;
}

// [LOOP-2] 결제 내역 초기화 및 재요청 로직
bool refreshPaymentData(int maxRetries) {
    Serial.println("\n[RFIDController][[2/3] 관리자 카드 감지됨 → 결제 내역 초기화");
    payment.clear();

    if (!fetchPaymentDataUntilSuccess(maxRetries)) {
        Serial.println("[ServerService][PaymentData][404] " + String(maxRetries) + "회 시도하였지만 결제내역 가져오는데 실패했습니다. 재시도 하려면 카드를 다시 찍어주세요.");
        return false;
    }
    return true;
}

// [LOOP-3] 외부 서버로 GET 요청 전송해 결제 내역을 받아온다.
bool fetchPaymentDataUntilSuccess(const int count) {
    int i = 0;
    while ( i < count) {
        Serial.println("[ServerService][PaymentData][1/3] 결제 내역을 가져오는 중입니다..");
        String getResponse = ServerService::sendGETRequest(config.serverIP.c_str(), config.serverPort, config.getPayment);
        //Serial.println(getResponse);

        String responseBody = getResponse.substring(getResponse.indexOf("\r\n\r\n") + 4);

        if (payment.parseFromJson(responseBody)) {
            Serial.println("[ServerService][PaymentData][2/3] 가져온 결제 내역을 출력합니다.");
            // Serial.println("[ServerService][INFO] 결제 ID: " + payment.getPaymentId());
            // Serial.println("[ServerService][INFO] 결제 상품 목록:");
            payment.printItems();
            Serial.println("[ServerService][PaymentData][3/3] 결제 내역 수신 성공. 다음 단계로 진행합니다.");Serial.println("");
            return true;
        }

        Serial.println("[ServerService][재시도] 결제 내역 파싱 실패. 2초 후 재시도...");Serial.println("");
        delay(3000); i++;
    }
    return false;
}

// [LOOP-4] 상품 매칭 시 동작을 처리하는 함수
void handleMatchedProduct(const String& matchedName, const String& detectedUid) {
    Serial.println("[RFIDController][2/3] 일치하는 상품: " + matchedName + " → 모터 정지 명령 전송");

    if (sendWithRetry("STOP")) {
        Serial.println("[RFIDController][3/3] STOP 명령 전송 및 ACK 수신 성공");

        // UID를 서버에 전송하여 워킹 리스트에 추가
        // String path = "/bot/add-working-list?uid=" + detectedUid;
        String path = config.addWorkingList.c_str() + detectedUid;
        String response = ServerService::sendGETRequest(config.serverIP.c_str(), config.serverPort, path);

        Serial.println("[Server 응답] " + response);

        if (response.indexOf("작업 항목이 성공적으로 추가되었습니다.") != -1 || response.indexOf("200 OK") != -1) {
            Serial.println("[RFIDController] 워킹 리스트 추가 성공");
            // detectedUid 를 기반으로 /start-stand?uid=
            sendStartStandRequest(detectedUid);
        } else {
            Serial.println("[RFIDController] 워킹 리스트 추가 실패");
        }
    } else {
        Serial.println("[RFIDController][3/3] STOP 명령 전송 실패 (ACK 없음)");
    }

    Serial.println("[RFIDController][3/3] 다음 상품으로 이동 합니다.\n");
}

// [LOOP-5] UID를 인식해서 결제내역 확인 하는 함수
void checkDetectedUid() {
    String detectedUid = rfidController->getUID();
    if (detectedUid.isEmpty()) return;
    
    //TODO: 카드가 찍히면 해당하는 UID를 가지는 선반에 요청을 보내 rfid카드를 들어 올린다
    //sendUpRfidCardRequest(detectedUid);

    Serial.println("[RFIDController][1/3] 감지된 UID: " + detectedUid);

    // 함수: [LOOP-2], [LOOP-3], [LOOP-4]
    if (isAdminCard(detectedUid)) {
        refreshPaymentData(); // 기본 3회 시도
        return;
    }

    // test 카드로 작동 확인
    if (detectedUid == config.testKey) {
        if (sendWithRetry("TEST")) {
            Serial.println("[RFIDController][3/3] TEST 명령 전송 및 ACK 수신 성공");
            return;
        } else {
            Serial.println("[RFIDController][3/3] TEST 명령 전송 실패 (ACK 없음)");
            return;
        }
    }

    // 함수 [LOOP-5]
    String matchedName;
    if (payment.matchUID(detectedUid, matchedName)) {
        handleMatchedProduct(matchedName,detectedUid);
    } else {
        Serial.println("[RFIDController][2/3] 감지된 UID는 결제 내역에 없음 → 무시");
        Serial.println("[RFIDController][3/3] 다음 상품으로 이동 합니다.\n");
    }
}

// UTILITY FUNCTION ====================================================================================================

// [UTILITY-1] 명령 전송 함수 (재시도 포함)
bool sendWithRetry(const String& cmd, const int retries) {
    for (int i = 0; i < retries; ++i) {
        Serial2.println(cmd);  // 명령 전송
        Serial.println("[Wired Comm][Serial2][1/2] " + cmd + " 명령 전송");

        unsigned long start = millis();
        while (millis() - start < 1000) {  // 1초 이내 응답 대기
            if (Serial2.available()) {
                String response = Serial2.readStringUntil('\n');
                response.trim();
                if (response == "ACK") {
                    Serial.println("[Wired Comm][Serial2][2/2] ACK 수신 성공");
                    return true;
                } else {
                    Serial.println("[Wired Comm][Serial2][2/2]  잘못된 응답: " + response);
                }
            }
        }

        Serial.println("[Wired Comm][Serial2][RETRY]  ACK 수신 실패, 재시도 " + String(i + 1) + "\n");
        delay(200);
    }

    Serial.println("[Wired Comm][4/4]  " + cmd + " 명령 전송 실패 (ACK 없음)\n");
    return false;
}

// [UTILITY-2] 간편 메시지 사용 메서드
void simpleMessage(String message) {

    if (message == "시작선") {
        Serial.println("= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =");
    }else if (message == "종료선") {
        Serial.println("= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =");
    }
}

// [UTILITY-3] 감지된 UID를 기반으로 /start-stand 요청을 보냅니다.
void sendStartStandRequest(const String& detectedUid) {
        if (detectedUid.length() == 0) {
        Serial.println("[요청 실패] UID가 비어 있습니다.");
        return;
    }

    for (int attempt = 1; attempt <= 3; ++attempt) {
        HTTPClient http;
        String url = "http://" + String(config.serverIP) + ":" + String(config.standPort) + "/start-stand?uid=" + detectedUid;

        Serial.println("[요청 전송] (" + String(attempt) + "회차): " + url);
        http.begin(url);

        int httpCode = http.GET();

        if (httpCode == 200) {
            String response = http.getString();
            Serial.println("[응답 200] 작업 시작됨 → " + response);
            http.end();
            break;
        } else {
            Serial.println("[요청 실패] 응답 코드: " + String(httpCode));
        }

        http.end();
        delay(1000); // 1초 대기 후 재시도
    }
}

// [UTILITY-4] 감지된 UID를 기반으로 /up-rfid? 요청을 보냅니다.
void sendUpRfidCardRequest(const String& detectedUid) {
        if (detectedUid.length() == 0) {
        Serial.println("[요청 실패] UID가 비어 있습니다.");
        return;
    }

    for (int attempt = 1; attempt <= 3; ++attempt) {
        HTTPClient http;
        String url = "http://" + String(config.serverIP) + ":" + String(config.standPort) + "/up-rfid?uid=" + detectedUid;

        Serial.println("[요청 전송] (" + String(attempt) + "회차): " + url);
        http.begin(url);

        int httpCode = http.GET();

        if (httpCode == 200) {
            String response = http.getString();
            Serial.println("[응답 200] 작업 시작됨 → " + response);
            http.end();
            break;
        } else {
            Serial.println("[요청 실패] 응답 코드: " + String(httpCode));
        }

        http.end();
        delay(1000); // 1초 대기 후 재시도
    }
}