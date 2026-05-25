#include "ui.h"
#include "photos.h"
#include <Arduino.h>
#include <lvgl.h>

static lv_obj_t *btn_prev;
static lv_obj_t *btn_next;
static lv_obj_t *btn_settings;

static void btn_event_cb(lv_event_t *e) {
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  if (btn == btn_next)
    photos_show_next();
  else if (btn == btn_prev)
    photos_show_prev();
  else if (btn == btn_settings)
    Serial.println("Settings pressed");
}

void ui_create() {
  btn_settings = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn_settings, 32, 32);
  lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_add_event_cb(btn_settings, btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *settings_img = lv_image_create(btn_settings);
  lv_image_set_src(settings_img, "S:/settings.png");
  lv_obj_center(settings_img);

  if (photos_count() == 0) {
    lv_obj_t *no_photo_label = lv_label_create(lv_screen_active());
    lv_label_set_text(no_photo_label, "No photos found");
    lv_obj_center(no_photo_label);
    return;
  }

  photos_show_first();

  btn_prev = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn_prev, 32, 32);
  lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  lv_obj_add_event_cb(btn_prev, btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *prev_img = lv_image_create(btn_prev);
  lv_image_set_src(prev_img, "S:/previous.png");
  lv_obj_center(prev_img);

  btn_next = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn_next, 32, 32);
  lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
  lv_obj_add_event_cb(btn_next, btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *next_img = lv_image_create(btn_next);
  lv_image_set_src(next_img, "S:/next.png");
  lv_obj_center(next_img);
}
