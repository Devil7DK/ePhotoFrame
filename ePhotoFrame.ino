#include <lvgl.h>
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
    ui_create();
    wifi_start_sta();
    web_server_begin(MODE_MANAGE);
  }
}

void loop() {
  lv_timer_handler();
  wifi_update();
  web_server_update();
  config_commit_pending();

  if (web_server_factory_reset_requested()) {
    Serial.println("[INFO] Factory reset");
    config_factory_reset();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    ESP.restart();
  }

  if (web_server_restart_requested() || wifi_restart_requested()) {
    Serial.println("[INFO] Restart requested");
    config_commit_pending(true);
    delay(100);
    ESP.restart();
  }

  delay(5);
}
