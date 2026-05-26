#include "web_server.h"
#include "config.h"
#include "wifi_manager.h"
#include "HtmlData.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>

static AsyncWebServer server(80);
static WebMode current_mode = MODE_SETUP;
static bool restart_requested = false;
static bool factory_reset_requested = false;
static bool begin_called = false;

static void send_html(AsyncWebServerRequest *req) {
  // L14: HTML_DATA is gzip-compressed by the vite plugin.
  AsyncWebServerResponse *resp = req->beginResponse_P(
    200, "text/html; charset=utf-8", HTML_DATA, sizeof(HTML_DATA));
  resp->addHeader("Content-Encoding", "gzip");
  resp->addHeader("Cache-Control", "max-age=600");
  req->send(resp);
}

static void register_routes() {
  server.on("/", HTTP_GET, send_html);
  server.onNotFound([](AsyncWebServerRequest *req) {
    // SPA catch-all: any unknown GET serves the single HTML.
    if (req->method() == HTTP_GET) {
      send_html(req);
      return;
    }
    req->send(404, "application/json", "{\"error\":\"not found\"}");
  });

  server.on("/api/mode", HTTP_GET, [](AsyncWebServerRequest *req) {
    const char *body = (current_mode == MODE_SETUP)
                         ? "{\"mode\":\"setup\"}"
                         : "{\"mode\":\"manage\"}";
    req->send(200, "application/json", body);
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    doc["uptime_ms"] = (uint32_t)millis();
    doc["wifi"] = wifi_status_string();
    doc["ip"] = wifi_ip();
    doc["rssi"] = wifi_rssi();
    doc["free_heap"] = (uint32_t)ESP.getFreeHeap();
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    // Never include the password.
    JsonDocument doc;
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = config_get_ssid();
    wifi["configured"] = config_has_wifi();
    doc["autoplay_ms"] = config_get_autoplay_ms();
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/networks", HTTP_GET, [](AsyncWebServerRequest *req) {
    // Kick a fresh async scan in the background; the next GET returns newer
    // results. If one is already running, this is a no-op.
    wifi_scan_async();
    req->send(200, "application/json", wifi_scan_results_json());
  });

  // POST /api/config — L4: JSON body handler avoids the manual _tempObject
  // accumulator pattern and the leak-on-abort hazard it carries.
  auto *configPost = new AsyncCallbackJsonWebHandler(
    "/api/config",
    [](AsyncWebServerRequest *req, JsonVariant &json) {
      if (current_mode != MODE_SETUP) {
        req->send(403, "application/json", "{\"error\":\"setup mode only\"}");
        return;
      }
      JsonObject wifi = json["wifi"];
      if (!wifi["ssid"].is<const char *>()) {
        req->send(400, "application/json", "{\"error\":\"missing wifi.ssid\"}");
        return;
      }
      const char *ssid = wifi["ssid"];
      const char *pwd  = wifi["password"] | "";
      size_t ssid_len = strlen(ssid);
      size_t pwd_len  = strlen(pwd);
      if (ssid_len == 0) {
        req->send(400, "application/json", "{\"error\":\"empty ssid\"}");
        return;
      }
      if (ssid_len >= 33 || pwd_len >= 65) {
        req->send(400, "application/json", "{\"error\":\"ssid or password too long\"}");
        return;
      }
      config_set_wifi(ssid, pwd);
      config_commit_pending(true);  // L13: bypass debounce; we're about to restart.
      req->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
      restart_requested = true;     // L5: ESP.restart() runs from loop() once response flushes.
    });
  configPost->setMethod(HTTP_POST);
  server.addHandler(configPost);

  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    restart_requested = true;  // L5
  });
}

static bool mdns_started = false;

static void start_mdns_when_connected() {
  if (mdns_started) return;
  if (wifi_status() != WIFI_ST_CONNECTED) return;
  if (MDNS.begin("ephotoframe")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[OK] mDNS: ephotoframe.local");
    mdns_started = true;
  } else {
    Serial.println("[WARN] mDNS begin failed");
  }
}

void web_server_begin(WebMode mode) {
  current_mode = mode;
  register_routes();
  server.begin();
  begin_called = true;
  Serial.print("[OK] Web server started in ");
  Serial.println(mode == MODE_SETUP ? "SETUP mode" : "MANAGE mode");
}

void web_server_update() {
  if (!begin_called) return;
  // mDNS can only register once an IP is bound, which is some ticks after
  // wifi_start_sta() returns. Poll until ready.
  start_mdns_when_connected();
}

bool web_server_restart_requested() {
  return restart_requested;
}
bool web_server_factory_reset_requested() {
  return factory_reset_requested;
}
