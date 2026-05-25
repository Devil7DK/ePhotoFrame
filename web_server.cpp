#include "web_server.h"
#include "config.h"
#include "wifi_manager.h"
#include "HtmlData.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);
static WebMode current_mode = MODE_SETUP;
static bool restart_requested = false;
static bool factory_reset_requested = false;

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
}

void web_server_begin(WebMode mode) {
  current_mode = mode;
  register_routes();
  server.begin();
  Serial.print("[OK] Web server started in ");
  Serial.println(mode == MODE_SETUP ? "SETUP mode" : "MANAGE mode");
}

void web_server_update() {
  // Phase 2: no pending flags yet. Phase 4+ will apply navigate/rescan here.
}

bool web_server_restart_requested() {
  return restart_requested;
}
bool web_server_factory_reset_requested() {
  return factory_reset_requested;
}
