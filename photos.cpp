#include "photos.h"
#include "config.h"
#include "storage.h"
#include <Arduino.h>
#include <lvgl.h>
#include <SD.h>
#include <FS.h>

#define IMAGE_FOLDER "/images"
#define MAX_PHOTOS 32

static String photo_paths[MAX_PHOTOS];
static int photo_count = 0;
static int current_index = 0;
static lv_obj_t *img_obj = nullptr;
static unsigned long last_advance_ms = 0;

static void show_at(int idx) {
  Serial.print("[INFO] Loading photo: ");
  Serial.println(photo_paths[idx].c_str());

  // Reuse a single image widget across navigations. Delete + recreate causes
  // a visible white flash between the old image being destroyed and the new
  // one decoded (SD read ~150 ms for 153 KB).
  if (!img_obj) {
    img_obj = lv_image_create(lv_screen_active());
    lv_obj_move_background(img_obj);
    lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_SCROLLABLE);
  }
  if (!storage_sd_lock()) {
    Serial.println("[WARN] SD busy, skipping image load");
    return;
  }
  lv_image_set_src(img_obj, photo_paths[idx].c_str());
  storage_sd_unlock();
  lv_obj_center(img_obj);
  last_advance_ms = millis();
}

void photos_reset() {
  img_obj = nullptr;
}

void photos_update() {
  uint32_t interval = config_get_autoplay_ms();
  if (interval == 0) return;        // autoplay disabled
  if (photo_count <= 1) return;     // nothing to cycle through
  if (millis() - last_advance_ms < interval) return;
  photos_show_next();                // show_at resets last_advance_ms
}

void photos_scan() {
  photo_count = 0;
  if (!storage_sd_lock()) {
    Serial.println("[WARN] SD busy, skipping photo scan");
    return;
  }
  File root = SD.open(IMAGE_FOLDER);
  if (!root || !root.isDirectory()) {
    Serial.println("[WARN] SD image folder not found");
    storage_sd_unlock();
    return;
  }

  File file = root.openNextFile();
  while (file && photo_count < MAX_PHOTOS) {
    if (!file.isDirectory()) {
      String fname = file.name();
      fname.toLowerCase();
      if (fname.endsWith(".bin")) {
        String fullPath = String("D:") + IMAGE_FOLDER + "/" + fname;
        Serial.print("[INFO] Found photo: ");
        Serial.println(fullPath);
        photo_paths[photo_count++] = fullPath;
      }
    }
    file = root.openNextFile();
  }
  storage_sd_unlock();
}

int photos_count() {
  return photo_count;
}

void photos_show_first() {
  if (photo_count == 0)
    return;
  current_index = 0;
  show_at(current_index);
}

void photos_show_next() {
  if (photo_count == 0)
    return;
  current_index = (current_index + 1) % photo_count;
  show_at(current_index);
}

void photos_show_prev() {
  if (photo_count == 0)
    return;
  current_index = (current_index - 1 + photo_count) % photo_count;
  show_at(current_index);
}
