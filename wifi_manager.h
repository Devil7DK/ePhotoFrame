#pragma once

#include <Arduino.h>

enum WifiStatus {
  WIFI_ST_DISCONNECTED,
  WIFI_ST_CONNECTING,
  WIFI_ST_CONNECTED,
  WIFI_ST_AP_MODE,
  WIFI_ST_FAILED
};

void wifi_init();
void wifi_start_sta();
void wifi_start_ap();
void wifi_update();

WifiStatus wifi_status();
const char* wifi_status_string();
String wifi_ip();
String wifi_ap_ssid();
String wifi_ap_password();
int32_t wifi_rssi();

void wifi_scan_async();
const char* wifi_scan_results_json();
