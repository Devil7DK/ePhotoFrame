#include "web_server.h"
#include "config.h"
#include "storage.h"
#include "wifi_manager.h"
#include "HtmlData.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <SD.h>
#include <FS.h>

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

  server.on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (current_mode != MODE_MANAGE) {
      req->send(403, "application/json", "{\"error\":\"manage mode only\"}");
      return;
    }
    req->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    config_clear_wifi();
    config_commit_pending(true);
    restart_requested = true;  // L5
  });

  server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (current_mode != MODE_MANAGE) {
      req->send(403, "application/json", "{\"error\":\"manage mode only\"}");
      return;
    }
    req->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    factory_reset_requested = true;  // L5
  });

  // ---------- /api/images endpoints (manage mode only) ----------

  // GET /api/images — list .bin files in /images
  server.on("/api/images", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (current_mode != MODE_MANAGE) {
      req->send(403, "application/json", "{\"error\":\"manage mode only\"}");
      return;
    }
    if (!storage_sd_lock(1000)) {
      req->send(503, "application/json", "{\"error\":\"sd busy\"}");
      return;
    }
    // L2: stream the response to avoid double-buffering the listing.
    AsyncResponseStream *resp = req->beginResponseStream("application/json");
    resp->print("{\"images\":[");
    File root = SD.open("/images");
    bool first = true;
    if (root && root.isDirectory()) {
      File f = root.openNextFile();
      while (f) {
        if (!f.isDirectory()) {
          String name = f.name();
          String lower = name;
          lower.toLowerCase();
          if (lower.endsWith(".bin")) {
            if (!first) resp->print(",");
            first = false;
            resp->printf("{\"name\":\"%s\",\"size\":%u}",
                         name.c_str(), (unsigned)f.size());
          }
        }
        f = root.openNextFile();
      }
    }
    resp->print("]}");
    storage_sd_unlock();
    req->send(resp);
  });

  // Path-name validator shared by GET-by-name and DELETE.
  auto is_safe_name = [](const String &name) {
    if (name.length() == 0 || name.length() > 64) return false;
    if (name.indexOf('/') >= 0) return false;
    if (name.indexOf("..") >= 0) return false;
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".bin");
  };

  // GET /api/images/:name — stream the .bin file from SD
  server.on("^\\/api\\/images\\/(.+)$", HTTP_GET, [is_safe_name](AsyncWebServerRequest *req) {
    if (current_mode != MODE_MANAGE) {
      req->send(403, "application/json", "{\"error\":\"manage mode only\"}");
      return;
    }
    String name = req->pathArg(0);
    if (!is_safe_name(name)) {
      req->send(400, "application/json", "{\"error\":\"bad name\"}");
      return;
    }
    String path = String("/images/") + name;
    if (!storage_sd_lock(1000)) {
      req->send(503, "application/json", "{\"error\":\"sd busy\"}");
      return;
    }
    if (!SD.exists(path)) {
      storage_sd_unlock();
      req->send(404, "application/json", "{\"error\":\"not found\"}");
      return;
    }
    AsyncWebServerResponse *resp =
        req->beginResponse(SD, path, "application/octet-stream");
    // SD lock is held until the response is fully sent. Release on disconnect.
    req->onDisconnect([]() { storage_sd_unlock(); });
    req->send(resp);
  });

  // DELETE /api/images/:name
  server.on("^\\/api\\/images\\/(.+)$", HTTP_DELETE, [is_safe_name](AsyncWebServerRequest *req) {
    if (current_mode != MODE_MANAGE) {
      req->send(403, "application/json", "{\"error\":\"manage mode only\"}");
      return;
    }
    String name = req->pathArg(0);
    if (!is_safe_name(name)) {
      req->send(400, "application/json", "{\"error\":\"bad name\"}");
      return;
    }
    String path = String("/images/") + name;
    if (!storage_sd_lock(1000)) {
      req->send(503, "application/json", "{\"error\":\"sd busy\"}");
      return;
    }
    bool ok = SD.remove(path);
    storage_sd_unlock();
    if (ok) {
      req->send(200, "application/json", "{\"ok\":true}");
    } else {
      req->send(404, "application/json", "{\"error\":\"not found\"}");
    }
  });

  // POST /api/images?name=<name>.bin — raw body upload (browser converts to
  // .bin client-side, server just stores it).
  server.on(
    "/api/images", HTTP_POST,
    // onRequest: fires after all body chunks are received
    [](AsyncWebServerRequest *req) {
      if (current_mode != MODE_MANAGE) {
        req->send(403, "application/json", "{\"error\":\"manage mode only\"}");
        return;
      }
      if (req->_tempFile) {
        req->_tempFile.close();
        storage_sd_unlock();
        req->send(200, "application/json", "{\"ok\":true}");
      } else {
        req->send(400, "application/json", "{\"error\":\"upload failed\"}");
      }
    },
    nullptr,  // onUpload — not multipart
    // onBody: per-chunk raw body
    [is_safe_name](AsyncWebServerRequest *req, uint8_t *data, size_t len,
                   size_t index, size_t /*total*/) {
      if (current_mode != MODE_MANAGE) return;
      if (index == 0) {
        // First chunk — validate name and open file
        if (!req->hasParam("name")) {
          Serial.println("[WARN] upload: missing name param");
          return;
        }
        String name = req->getParam("name")->value();
        if (!is_safe_name(name)) {
          Serial.print("[WARN] upload: bad name ");
          Serial.println(name);
          return;
        }
        if (!storage_sd_lock(2000)) {
          Serial.println("[WARN] upload: sd busy");
          return;
        }
        String path = String("/images/") + name;
        req->_tempFile = SD.open(path, FILE_WRITE);
        if (!req->_tempFile) {
          storage_sd_unlock();
          Serial.print("[WARN] upload: open failed for ");
          Serial.println(path);
          return;
        }
        Serial.print("[INFO] upload start: ");
        Serial.println(path);
        // L4/L6: clean up partial file if client disconnects mid-upload.
        req->onDisconnect([req]() {
          if (req->_tempFile) {
            String p = req->_tempFile.path();
            req->_tempFile.close();
            SD.remove(p);
            storage_sd_unlock();
            Serial.print("[INFO] upload aborted, removed: ");
            Serial.println(p);
          }
        });
      }
      if (req->_tempFile) {
        req->_tempFile.write(data, len);
      }
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
