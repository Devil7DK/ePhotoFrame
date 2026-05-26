#include <WiFi.h>

#include "config.h"
#include "display.h"
#include "storage.h"
#include "photos.h"
#include "ui.h"
#include "wifi_manager.h"
#include "web_server.h"

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("[INFO] Booting");

  storage_init();
  config_load();
  wifi_init();

  // WiFi MUST be started (STA connect or AP) before the display is
  // initialized. Bringing the WiFi driver fully online after the panel is
  // configured causes the panel to stop honoring later refresh requests —
  // observed as a "Status:" label that updates internally but never reaches
  // the screen. Putting WiFi first avoids it entirely.
  if (config_has_wifi()) {
    wifi_start_sta();
  } else {
    wifi_start_ap();
    wifi_scan_async();
  }

  display_init_tft();               // lv_init() lives in here
  storage_register_lvgl_drivers();  // must run AFTER lv_init()
  display_init_lvgl();

  if (config_has_wifi()) {
    photos_scan();
    ui_show_photos();
  } else {
    ui_show_setup_screen();
  }

  display_start_lvgl_task();

  // Setup mode needs the web server up immediately so the phone can configure
  // creds. Manage mode brings up the web server on demand via
  // ui_consume_enter_manage() (WiFi STA is already connected by then).
  if (!config_has_wifi()) {
    web_server_begin(MODE_SETUP);
  }
}

void loop() {
  // Network state + restart flags only. LVGL runs on its own task.
  wifi_update();
  web_server_update();
  config_commit_pending();

  // Lazy manage-mode startup driven by the on-device settings menu. WiFi STA
  // is already connected from boot — we only need to bring the web server
  // up. Doing it on the loop task (not the LVGL task) keeps server.begin's
  // task creation off the LVGL refresh path.
  if (ui_consume_enter_manage()) {
    web_server_begin(MODE_MANAGE);
  }

  if (ui_factory_reset_requested() || web_server_factory_reset_requested()) {
    Serial.println("[INFO] Factory reset");
    config_factory_reset();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    ESP.restart();
  }

  if (ui_wifi_reset_requested()) {
    Serial.println("[INFO] WiFi reset");
    config_clear_wifi();
    config_commit_pending(true);
    delay(100);
    ESP.restart();
  }

  if (web_server_restart_requested() || ui_restart_requested()) {
    Serial.println("[INFO] Restart requested");
    config_commit_pending(true);
    delay(100);
    ESP.restart();
  }

  delay(20);
}
