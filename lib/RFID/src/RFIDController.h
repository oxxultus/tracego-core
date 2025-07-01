// RFIDController.h
#ifndef RFIDCONTROLLER_H
#define RFIDCONTROLLER_H

#include <Arduino.h>
#include <MFRC522.h>

/**
 * @class RFIDController
 * @brief MFRC522 기반 RFID 리더기 제어 클래스 (포인터 기반)
 */
class RFIDController {
public:
    RFIDController(uint8_t ssPin, uint8_t rstPin);
    ~RFIDController();

    void begin();
    void begin(HardwareSerial &debugSerial);
    String getUID();

private:
    uint8_t ssPin;
    uint8_t rstPin;
    MFRC522* rfid = nullptr;
    HardwareSerial* debug = nullptr;
};

#endif // RFIDCONTROLLER_H