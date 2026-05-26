#include "ui.h"
#include "config.h"
#include "photos.h"
#include "wifi_manager.h"
#include "web_server.h"
#include <Arduino.h>
#include <lvgl.h>

enum class PendingAction {
  NONE,
  SHOW_PHOTOS,
  SHOW_SETTINGS_MENU,
  ENTER_MANAGE_MODE,
};

static PendingAction       pending = PendingAction::NONE;
static bool                wifi_reset_requested = false;
static bool                factory_reset_requested = false;

static bool                in_manage_mode = false;
static lv_obj_t           *manage_status_label = nullptr;
static WifiStatus          last_wifi_status_shown = WIFI_ST_DISCONNECTED;

// ---------- async deferrals (run from top of next lv_timer_handler tick) ----

static void async_show_next(void *) { photos_show_next(); }
static void async_show_prev(void *) { photos_show_prev(); }

// ---------- per-button click handlers (no global-pointer compare) -----------

static void on_settings_clicked(lv_event_t *)      { pending = PendingAction::SHOW_SETTINGS_MENU; }
static void on_next_clicked(lv_event_t *)          { lv_async_call(async_show_next, NULL); }
static void on_prev_clicked(lv_event_t *)          { lv_async_call(async_show_prev, NULL); }
static void on_manage_clicked(lv_event_t *)        { pending = PendingAction::ENTER_MANAGE_MODE; }
static void on_back_clicked(lv_event_t *)          { pending = PendingAction::SHOW_PHOTOS; }
static void on_wifi_reset_clicked(lv_event_t *)    { wifi_reset_requested = true; }
static void on_factory_reset_clicked(lv_event_t *) { factory_reset_requested = true; }

// ---------- screen helpers ---------------------------------------------------

static void reset_screen() {
  in_manage_mode = false;
  manage_status_label = nullptr;
  lv_obj_clean(lv_screen_active());
  photos_reset();
}

static lv_obj_t *make_dark_bg() {
  lv_obj_t *bg = lv_obj_create(lv_screen_active());
  lv_obj_set_size(bg, 320, 240);
  lv_obj_set_pos(bg, 0, 0);
  lv_obj_set_style_bg_color(bg, lv_color_hex(0x202830), 0);
  lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bg, 0, 0);
  lv_obj_set_style_pad_all(bg, 10, 0);
  lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
  return bg;
}

static void add_white_title(lv_obj_t *parent, const char *text) {
  lv_obj_t *t = lv_label_create(parent);
  lv_label_set_text(t, text);
  lv_obj_set_style_text_color(t, lv_color_white(), 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);
}

static void make_menu_button(lv_obj_t *parent, const char *label, lv_event_cb_t cb, int y) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, 240, 36);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);
}

// ---------- screens ---------------------------------------------------------

void ui_show_setup_screen() {
  reset_screen();
  lv_obj_t *bg = make_dark_bg();
  add_white_title(bg, "Setup Mode");

  lv_obj_t *body = lv_label_create(bg);
  String text = String("Connect to:\n") + wifi_ap_ssid() +
                "\nPassword: " + wifi_ap_password() +
                "\n\nThen open:\nhttp://192.168.4.1";
  lv_label_set_text(body, text.c_str());
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(body, 290);
  lv_obj_set_style_text_color(body, lv_color_white(), 0);
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(body, LV_ALIGN_CENTER, 0, 0);

  lv_refr_now(NULL);
}

void ui_show_photos() {
  reset_screen();

  lv_obj_t *btn_settings = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn_settings, 32, 32);
  lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_add_event_cb(btn_settings, on_settings_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *settings_img = lv_image_create(btn_settings);
  lv_image_set_src(settings_img, "S:/settings.png");
  lv_obj_center(settings_img);

  if (photos_count() == 0) {
    lv_obj_t *no_photo = lv_label_create(lv_screen_active());
    lv_label_set_text(no_photo, "No photos found");
    lv_obj_center(no_photo);
    lv_refr_now(NULL);
    return;
  }

  photos_show_first();

  lv_obj_t *btn_prev = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn_prev, 32, 32);
  lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  lv_obj_add_event_cb(btn_prev, on_prev_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *prev_img = lv_image_create(btn_prev);
  lv_image_set_src(prev_img, "S:/previous.png");
  lv_obj_center(prev_img);

  lv_obj_t *btn_next = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn_next, 32, 32);
  lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
  lv_obj_add_event_cb(btn_next, on_next_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *next_img = lv_image_create(btn_next);
  lv_image_set_src(next_img, "S:/next.png");
  lv_obj_center(next_img);

  lv_refr_now(NULL);
}

void ui_show_settings_menu() {
  reset_screen();
  lv_obj_t *bg = make_dark_bg();
  add_white_title(bg, "Settings");

  make_menu_button(bg, "Manage Mode",   on_manage_clicked,        30);
  make_menu_button(bg, "Reset WiFi",    on_wifi_reset_clicked,    72);
  make_menu_button(bg, "Factory Reset", on_factory_reset_clicked, 114);
  make_menu_button(bg, "Back",          on_back_clicked,          156);

  lv_refr_now(NULL);
}

void ui_show_manage_mode() {
  reset_screen();
  in_manage_mode = true;
  last_wifi_status_shown = WIFI_ST_DISCONNECTED;

  lv_obj_t *bg = make_dark_bg();
  add_white_title(bg, "Manage Mode");

  // Static layout — WiFi SSID and mDNS hostname are both known up front.
  // Painted once on entry; never updated (L22).
  lv_obj_t *info = lv_label_create(bg);
  String text = String("Connect to WiFi:\n") + config_get_ssid() +
                "\n\nOpen in browser:\nhttp://ephotoframe.local";
  lv_label_set_text(info, text.c_str());
  lv_obj_set_style_text_color(info, lv_color_white(), 0);
  lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(info, 290);
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
  lv_obj_align(info, LV_ALIGN_CENTER, 0, -5);

  // Small fixed-position status indicator. Updates are best-effort — even if
  // a repaint stalls, the URL above stays visible.
  manage_status_label = lv_label_create(bg);
  lv_label_set_text(manage_status_label, "Status: connecting...");
  lv_obj_set_style_text_color(manage_status_label, lv_color_hex(0xffd060), 0);
  lv_obj_align(manage_status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

  lv_refr_now(NULL);
}

// ---------- per-tick update -------------------------------------------------

static void update_manage_status_if_changed() {
  if (!in_manage_mode || !manage_status_label) return;
  WifiStatus s = wifi_status();
  if (s == last_wifi_status_shown) return;
  last_wifi_status_shown = s;

  const char *txt = "Status: connecting...";
  if (s == WIFI_ST_CONNECTED)   txt = "Status: connected";
  else if (s == WIFI_ST_FAILED) txt = "Status: failed";

  lv_label_set_text(manage_status_label, txt);
  // Best-effort. The URL is already on screen from the initial paint; this
  // is just a connection-state hint. We deliberately don't force a refresh —
  // that path stalls under WiFi load and the URL remains visible regardless.
}

void ui_update() {
  update_manage_status_if_changed();

  if (pending == PendingAction::NONE) return;
  PendingAction action = pending;
  pending = PendingAction::NONE;

  switch (action) {
    case PendingAction::SHOW_PHOTOS:
      ui_show_photos();
      break;
    case PendingAction::SHOW_SETTINGS_MENU:
      ui_show_settings_menu();
      break;
    case PendingAction::ENTER_MANAGE_MODE:
      ui_show_manage_mode();
      wifi_start_sta();
      web_server_begin(MODE_MANAGE);
      break;
    default:
      break;
  }
}

bool ui_wifi_reset_requested()    { return wifi_reset_requested; }
bool ui_factory_reset_requested() { return factory_reset_requested; }
