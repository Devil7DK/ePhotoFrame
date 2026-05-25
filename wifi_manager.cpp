#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_mac.h>

#define CONNECT_TIMEOUT_MS 15000
#define RETRY_DELAY_MS 30000

static WifiStatus state = WIFI_ST_DISCONNECTED;
static unsigned long last_attempt = 0;
static String ap_ssid;
static String ap_password;
static String scan_results_json = "{\"networks\":[],\"count\":0}";
static bool scan_in_progress = false;
static bool restart_requested = false;

static void derive_ap_creds() {
  uint8_t mac[6] = { 0 };
  // Read from efuse directly — WiFi.macAddress() returns zeros while WiFi is off.
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char ssid_buf[24];
  snprintf(ssid_buf, sizeof(ssid_buf), "ePhotoFrame-%02X%02X", mac[4], mac[5]);
  char pwd_buf[16];
  snprintf(pwd_buf, sizeof(pwd_buf), "%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
  ap_ssid = ssid_buf;
  ap_password = pwd_buf;
}

void wifi_init() {
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.setHostname("ePhotoFrame");
  derive_ap_creds();
  Serial.println("[OK] WiFi manager init");
}

void wifi_start_sta() {
  if (!config_has_wifi()) {
    Serial.println("[WARN] STA requested but no credentials");
    return;
  }
  WiFi.mode(WIFI_STA);
  // L11: use INADDR_NONE to force DHCP cleanly.
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  Serial.print("[INFO] Connecting to ");
  Serial.println(config_get_ssid());
  WiFi.begin(config_get_ssid(), config_get_password());
  state = WIFI_ST_CONNECTING;
  last_attempt = millis();
}

void wifi_start_ap() {
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
  if (!ok) {
    Serial.println("[ERROR] softAP failed");
    state = WIFI_ST_FAILED;
    return;
  }
  IPAddress ip = WiFi.softAPIP();
  Serial.print("[OK] AP started: ");
  Serial.print(ap_ssid);
  Serial.print(" / ");
  Serial.print(ap_password);
  Serial.print(" @ ");
  Serial.println(ip);
  state = WIFI_ST_AP_MODE;
}

static void poll_scan() {
  if (!scan_in_progress) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  if (n == WIFI_SCAN_FAILED) {
    scan_in_progress = false;
    return;
  }
  JsonDocument doc;
  JsonArray nets = doc["networks"].to<JsonArray>();
  int valid = 0;
  for (int i = 0; i < n && i < 20; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    JsonObject net = nets.add<JsonObject>();
    net["ssid"] = s;
    net["rssi"] = WiFi.RSSI(i);
    net["encryption"] = (int)WiFi.encryptionType(i);
    valid++;
  }
  doc["count"] = valid;
  String out;
  serializeJson(doc, out);
  scan_results_json = out;
  WiFi.scanDelete();
  scan_in_progress = false;
  Serial.print("[OK] Scan complete: ");
  Serial.print(valid);
  Serial.println(" networks");
}

void wifi_scan_async() {
  if (scan_in_progress) return;
  int started = WiFi.scanNetworks(true, false);
  // L9: only WIFI_SCAN_RUNNING is success. WIFI_SCAN_FAILED is NOT "running".
  if (started == WIFI_SCAN_RUNNING) {
    scan_in_progress = true;
    Serial.println("[INFO] Async scan started");
  } else {
    Serial.print("[WARN] Scan start returned ");
    Serial.println(started);
  }
}

void wifi_update() {
  poll_scan();

  switch (state) {
    case WIFI_ST_DISCONNECTED:
      if (config_has_wifi() && (last_attempt == 0 || millis() - last_attempt > RETRY_DELAY_MS)) {
        wifi_start_sta();
      }
      break;
    case WIFI_ST_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        state = WIFI_ST_CONNECTED;
        Serial.print("[OK] WiFi connected, IP=");
        Serial.println(WiFi.localIP());
      } else if (millis() - last_attempt > CONNECT_TIMEOUT_MS) {
        Serial.println("[WARN] WiFi connect timed out");
        state = WIFI_ST_FAILED;
        last_attempt = millis();
      }
      break;
    case WIFI_ST_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[INFO] WiFi disconnected");
        state = WIFI_ST_DISCONNECTED;
      }
      break;
    case WIFI_ST_FAILED:
      if (millis() - last_attempt > RETRY_DELAY_MS)
        state = WIFI_ST_DISCONNECTED;
      break;
    case WIFI_ST_AP_MODE:
      break;
  }
}

WifiStatus wifi_status() {
  return state;
}

const char* wifi_status_string() {
  switch (state) {
    case WIFI_ST_DISCONNECTED: return "Disconnected";
    case WIFI_ST_CONNECTING: return "Connecting";
    case WIFI_ST_CONNECTED: return "Connected";
    case WIFI_ST_AP_MODE: return "Setup Mode";
    case WIFI_ST_FAILED: return "Failed";
  }
  return "Unknown";
}

String wifi_ip() {
  if (state == WIFI_ST_AP_MODE) return WiFi.softAPIP().toString();
  if (state == WIFI_ST_CONNECTED) return WiFi.localIP().toString();
  return "";
}

String wifi_ap_ssid() {
  return ap_ssid;
}
String wifi_ap_password() {
  return ap_password;
}

int32_t wifi_rssi() {
  if (state == WIFI_ST_CONNECTED) return WiFi.RSSI();
  return 0;
}

const char* wifi_scan_results_json() {
  return scan_results_json.c_str();
}

bool wifi_restart_requested() {
  return restart_requested;
}
void wifi_request_restart() {
  restart_requested = true;
}
