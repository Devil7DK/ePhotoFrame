#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <FS.h>

#define CONFIG_PATH "/config.json"
#define CONFIG_SCHEMA_VERSION 1
#define SAVE_DEBOUNCE_MS 2000

struct Config {
  char ssid[33] = "";
  char password[65] = "";
  // 1 hour default — a fresh-flashed device should cycle photos out of the box.
  uint32_t autoplay_ms = 3600000;
};

static Config cfg;
static bool save_pending = false;
static unsigned long save_dirty_at = 0;

bool config_load() {
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    Serial.println("[INFO] No config file, using defaults");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.print("[ERROR] Config parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  const char* ssid = doc["wifi"]["ssid"] | "";
  const char* pwd = doc["wifi"]["password"] | "";
  strlcpy(cfg.ssid, ssid, sizeof(cfg.ssid));
  strlcpy(cfg.password, pwd, sizeof(cfg.password));
  cfg.autoplay_ms = doc["autoplay_ms"] | 3600000;

  Serial.print("[OK] Config loaded; wifi=");
  Serial.println(cfg.ssid[0] ? cfg.ssid : "(none)");
  return true;
}

bool config_has_wifi() {
  return cfg.ssid[0] != '\0';
}

const char* config_get_ssid() {
  return cfg.ssid;
}
const char* config_get_password() {
  return cfg.password;
}

void config_set_wifi(const char* ssid, const char* password) {
  strlcpy(cfg.ssid, ssid ? ssid : "", sizeof(cfg.ssid));
  strlcpy(cfg.password, password ? password : "", sizeof(cfg.password));
  config_request_save();
}

uint32_t config_get_autoplay_ms() {
  return cfg.autoplay_ms;
}

void config_set_autoplay_ms(uint32_t ms) {
  cfg.autoplay_ms = ms;
  config_request_save();
}

void config_request_save() {
  save_pending = true;
  save_dirty_at = millis();
}

static bool write_config_now() {
  JsonDocument doc;
  doc["version"] = CONFIG_SCHEMA_VERSION;
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = cfg.ssid;
  wifi["password"] = cfg.password;
  doc["autoplay_ms"] = cfg.autoplay_ms;

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) {
    Serial.println("[ERROR] Config open for write failed");
    return false;
  }
  size_t written = serializeJson(doc, f);
  f.close();

  if (written == 0) {
    Serial.println("[ERROR] Config write failed");
    return false;
  }
  Serial.print("[OK] Config saved (");
  Serial.print(written);
  Serial.println(" bytes)");
  return true;
}

void config_commit_pending(bool force) {
  if (!save_pending)
    return;
  if (!force && (millis() - save_dirty_at) < SAVE_DEBOUNCE_MS)
    return;
  if (write_config_now())
    save_pending = false;
}

void config_clear_wifi() {
  cfg.ssid[0] = '\0';
  cfg.password[0] = '\0';
  config_request_save();
}

void config_factory_reset() {
  LittleFS.remove(CONFIG_PATH);
  cfg = Config{};
  save_pending = false;
}
