#include "RFIDController.h"
#include <SPI.h>

// 생성자: 포인터 초기화
RFIDController::RFIDController(uint8_t ssPin, uint8_t rstPin)
    : ssPin(ssPin), rstPin(rstPin), rfid(nullptr), debug(nullptr) {}

// 소멸자: 메모리 해제
RFIDController::~RFIDController() {
    if (rfid) {
        delete rfid;
        rfid = nullptr;
    }
}

// 디버깅 없이 초기화
void RFIDController::begin() {
    if (rfid == nullptr) {
        rfid = new MFRC522(ssPin, rstPin);
    }

    SPI.begin();            // 기본 SPI 핀으로 시작
    rfid->PCD_Init();       // RFID 초기화
}

// 디버깅용 시리얼 포함 초기화
void RFIDController::begin(HardwareSerial &debugSerial) {
    debug = &debugSerial;
    if (debug) debug->println("[RFIDController][1/2] RFID 리더기 사용");

    if (rfid == nullptr) {
        rfid = new MFRC522(ssPin, rstPin);
    }

    SPI.begin();            // SPI 시작
    rfid->PCD_Init();       // RFID 초기화

    if (debug) debug->println("[RFIDController][2/2] RFID 리더기 초기화 완료\n");
}

// UID 감지
String RFIDController::getUID() {
    if (!rfid || !rfid->PICC_IsNewCardPresent() || !rfid->PICC_ReadCardSerial()) {
        return "";
    }

    String uidStr;
    for (byte i = 0; i < rfid->uid.size; i++) {
        if (rfid->uid.uidByte[i] < 0x10) uidStr += "0";
        uidStr += String(rfid->uid.uidByte[i], HEX);
    }

    uidStr.toLowerCase();

    rfid->PICC_HaltA();
    rfid->PCD_StopCrypto1();

    if (debug) debug->println("[RFID] 감지된 UID: " + uidStr);
    return uidStr;
}