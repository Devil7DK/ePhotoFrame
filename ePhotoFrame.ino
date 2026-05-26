#include <lvgl.h>
#include <WiFi.h>

#include "config.h"
#include "display.h"
#include "storage.h"
#include "photos.h"
#include "ui.h"
#include "wifi_manager.h"
#include "web_server.h"

// L21: default 8 KB Arduino-ESP32 loop stack overflows when an LVGL event
// callback chains into lv_refr_now() → full refresh → image decode → SD read.
// 16 KB clears it with ~8 KB headroom.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("[INFO] Booting");

  display_init_tft();
  storage_init();
  display_init_lvgl();
  config_load();
  wifi_init();

  if (!config_has_wifi()) {
    ui_show_setup_screen();
    wifi_start_ap();
    wifi_scan_async();
    web_server_begin(MODE_SETUP);
  } else {
    photos_scan();
    ui_show_photos();
    // Photo-frame mode runs WITHOUT WiFi / web server — they're started
    // on demand when the user enters manage mode from the settings menu.
  }
}

void loop() {
  lv_timer_handler();
  wifi_update();
  web_server_update();
  config_commit_pending();
  ui_update();

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

  if (web_server_restart_requested()) {
    Serial.println("[INFO] Restart requested");
    config_commit_pending(true);
    delay(100);
    ESP.restart();
  }

  delay(5);
}
