#include "ui.h"
#include "photos.h"
#include "wifi_manager.h"
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

void ui_show_setup_screen() {
  lv_obj_t *bg = lv_obj_create(lv_screen_active());
  lv_obj_set_size(bg, 320, 240);
  lv_obj_set_pos(bg, 0, 0);
  lv_obj_set_style_bg_color(bg, lv_color_hex(0x202830), 0);
  lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bg, 0, 0);
  lv_obj_set_style_pad_all(bg, 10, 0);
  lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(bg);
  lv_label_set_text(title, "Setup Mode");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *body = lv_label_create(bg);
  String text = String("Connect to:\n") + wifi_ap_ssid() + "\nPassword: " + wifi_ap_password() + "\n\nThen open:\nhttp://192.168.4.1";
  lv_label_set_text(body, text.c_str());
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(body, 290);
  lv_obj_set_style_text_color(body, lv_color_white(), 0);
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(body, LV_ALIGN_CENTER, 0, 0);

  lv_refr_now(NULL);
}
