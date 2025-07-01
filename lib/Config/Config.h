#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>  

struct Config {
  // Wi-Fi
  String ssid;
  String password;

  // 서버 정보
  String serverIP;
  int serverPort;
  int innerPort;

  // 요청 api
  String firstSetWoringLists;
  String resetWorkingLists;
  String getPayment;
  String addWorkingList;

  // UID 및 키
  String adminUID;
  String masterKey;
  String testKey;

  // RFID 및 통신 설정
  bool useRFID;
  int commRxPin;
  int commTxPin;
  int rcSdaPin;
  int rcRstPin;

  // 시리얼 통신 속도
  int serialBaudrate;
  int serial2Baudrate;

  // 저장 및 로딩 메서드
  void load();
  void save();
};

extern Config config;

#endif