#include "Config.h"
#include <Preferences.h>

Preferences prefs;
Config config;

void Config::load() {
  prefs.begin("settings", true);

  ssid            = prefs.getString("ssid", "");
  password        = prefs.getString("password", "");

  serverIP        = prefs.getString("server_ip", "oxxultus.kro.kr");
  serverPort      = prefs.getInt("server_port", 8080);
  innerPort       = prefs.getInt("inner_port", 8081);
  standPort       = prefs.getInt("stand_port", 8082);
  
  localIP         = prefs.getString("localIP", ""); 

  adminUID        = prefs.getString("admin_uid", "a1b2c3d4");
  masterKey       = prefs.getString("master_key", "c3a27b28");
  testKey         = prefs.getString("test_key", "34e0ef03");

  useRFID         = prefs.getBool("use_rfid", true);

  commRxPin       = prefs.getInt("comm_rx", 16);
  commTxPin       = prefs.getInt("comm_tx", 17);
  rcSdaPin        = prefs.getInt("rc_sda", 5);
  rcRstPin        = prefs.getInt("rc_rst", 22);

  serialBaudrate  = prefs.getInt("baudrate", 115200);
  serial2Baudrate = prefs.getInt("baudrate2", 9600);

  firstSetWoringLists = prefs.getString("fswl", "/bot/first-set-working-list");
  resetWorkingLists   = prefs.getString("rwl", "/bot/reset-working-list");
  getPayment          = prefs.getString("gpay", "/bot/payment");
  addWorkingList      = prefs.getString("awl", "/bot/add-working-list?uid=");

  prefs.end();
}

void Config::save() {
  prefs.begin("settings", false);

  prefs.putString("ssid", ssid);
  prefs.putString("password", password);

  prefs.putString("server_ip", serverIP);
  prefs.putInt("server_port", serverPort);
  prefs.putInt("inner_port", innerPort);
  prefs.putInt("stand_port", standPort);
  prefs.putString("localIP", localIP);

  prefs.putString("admin_uid", adminUID);
  prefs.putString("master_key", masterKey);
  prefs.putString("test_key", testKey);

  prefs.putBool("use_rfid", useRFID);

  prefs.putInt("comm_rx", commRxPin);
  prefs.putInt("comm_tx", commTxPin);
  prefs.putInt("rc_sda", rcSdaPin);
  prefs.putInt("rc_rst", rcRstPin);

  prefs.putInt("baudrate", serialBaudrate);
  prefs.putInt("baudrate2", serial2Baudrate);

  prefs.putString("fswl", firstSetWoringLists);
  prefs.putString("rwl", resetWorkingLists);
  prefs.putString("gpay", getPayment);
  prefs.putString("awl", addWorkingList);

  prefs.end();
}