#include <lvgl.h>

#include "config.h"
#include "display.h"
#include "storage.h"
#include "photos.h"
#include "ui.h"

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("[INFO] Booting");

  display_init_tft();
  storage_init();
  display_init_lvgl();
  config_load();

  photos_scan();
  ui_create();
}

void loop() {
  lv_timer_handler();
  config_commit_pending();
  delay(5);
}
