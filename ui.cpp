#include "ui.h"
#include "config.h"
#include "photos.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <lvgl.h>

enum class PendingAction {
  NONE,
  SHOW_PHOTOS,
  SHOW_SETTINGS_MENU,
  ENTER_MANAGE_MODE,
};

static PendingAction pending = PendingAction::NONE;
static bool wifi_reset_requested = false;
static bool factory_reset_requested = false;
static bool restart_requested = false;
static bool enter_manage_requested = false;

static bool in_manage_mode = false;
static lv_obj_t *manage_status_label = nullptr;
static WifiStatus last_wifi_status_shown = WIFI_ST_DISCONNECTED;

// ---------- per-button click handlers (no global-pointer compare) -----------
// L23: LVGL runs on its own task, so events fire there and we can call
// LVGL-mutating helpers (photos_show_*, ui_show_*) directly without the old
// lv_async_call deferral or lv_refr_now workarounds.

static void on_settings_clicked(lv_event_t *) {
  pending = PendingAction::SHOW_SETTINGS_MENU;
}
static void on_next_clicked(lv_event_t *) {
  photos_show_next();
}
static void on_prev_clicked(lv_event_t *) {
  photos_show_prev();
}
static void on_manage_clicked(lv_event_t *) {
  pending = PendingAction::ENTER_MANAGE_MODE;
}
static void on_back_clicked(lv_event_t *) {
  pending = PendingAction::SHOW_PHOTOS;
}
static void on_wifi_reset_clicked(lv_event_t *) {
  wifi_reset_requested = true;
}
static void on_factory_reset_clicked(lv_event_t *) {
  factory_reset_requested = true;
}
static void on_restart_clicked(lv_event_t *) {
  restart_requested = true;
}

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
  String text = String("Connect to:\n") + wifi_ap_ssid() + "\nPassword: " + wifi_ap_password() + "\n\nThen open:\nhttp://192.168.4.1";
  lv_label_set_text(body, text.c_str());
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(body, 290);
  lv_obj_set_style_text_color(body, lv_color_white(), 0);
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(body, LV_ALIGN_CENTER, 0, 0);
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
}

void ui_show_settings_menu() {
  reset_screen();
  lv_obj_t *bg = make_dark_bg();
  add_white_title(bg, "Settings");

  make_menu_button(bg, "Manage Mode", on_manage_clicked, 30);
  make_menu_button(bg, "Reset WiFi", on_wifi_reset_clicked, 72);
  make_menu_button(bg, "Factory Reset", on_factory_reset_clicked, 114);
  make_menu_button(bg, "Back", on_back_clicked, 156);
}

void ui_show_manage_mode() {
  reset_screen();
  in_manage_mode = true;
  last_wifi_status_shown = WIFI_ST_DISCONNECTED;

  lv_obj_t *bg = make_dark_bg();
  add_white_title(bg, "Manage Mode");

  // WiFi SSID + mDNS hostname — static text, painted once on entry.
  lv_obj_t *info = lv_label_create(bg);
  String text = String("Connect to WiFi:\n") + config_get_ssid() + "\n\nOpen in browser:\nhttp://ephotoframe.local";
  lv_label_set_text(info, text.c_str());
  lv_obj_set_style_text_color(info, lv_color_white(), 0);
  lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(info, 290);
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
  lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 25);

  // Status indicator, updated by update_manage_status_if_changed.
  manage_status_label = lv_label_create(bg);
  lv_label_set_text(manage_status_label, "Status: connecting...");
  lv_obj_set_style_text_color(manage_status_label, lv_color_hex(0xffd060), 0);
  lv_obj_align(manage_status_label, LV_ALIGN_BOTTOM_MID, 0, -50);

  // Back returns to the photo frame UI without restarting. The web server
  // keeps running, so re-entering manage mode is instant.
  lv_obj_t *back_btn = lv_button_create(bg);
  lv_obj_set_size(back_btn, 130, 36);
  lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  lv_obj_add_event_cb(back_btn, on_back_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *back_lbl = lv_label_create(back_btn);
  lv_label_set_text(back_lbl, "Back");
  lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(back_lbl);

  // Restart is a full ESP.restart(). Same as the web app's restart button.
  lv_obj_t *restart_btn = lv_button_create(bg);
  lv_obj_set_size(restart_btn, 130, 36);
  lv_obj_align(restart_btn, LV_ALIGN_BOTTOM_RIGHT, 0, -5);
  lv_obj_add_event_cb(restart_btn, on_restart_clicked, LV_EVENT_CLICKED, NULL);
  lv_obj_t *restart_lbl = lv_label_create(restart_btn);
  lv_label_set_text(restart_lbl, "Restart");
  lv_obj_set_style_text_font(restart_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(restart_lbl);
}

// ---------- per-tick update -------------------------------------------------

static void update_manage_status_if_changed() {
  if (!in_manage_mode || !manage_status_label) return;
  WifiStatus s = wifi_status();
  if (s == last_wifi_status_shown) return;
  last_wifi_status_shown = s;

  const char *txt = "Status: connecting...";
  if (s == WIFI_ST_CONNECTED) txt = "Status: connected";
  else if (s == WIFI_ST_FAILED) txt = "Status: failed";

  lv_label_set_text(manage_status_label, txt);
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
      // WiFi STA + AsyncWebServer init can block the calling task for
      // hundreds of ms. Do them on the loop task instead so this task can
      // keep refreshing LVGL.
      enter_manage_requested = true;
      break;
    default:
      break;
  }
}

bool ui_wifi_reset_requested() {
  return wifi_reset_requested;
}
bool ui_factory_reset_requested() {
  return factory_reset_requested;
}
bool ui_restart_requested() {
  return restart_requested;
}

bool ui_consume_enter_manage() {
  if (!enter_manage_requested) return false;
  enter_manage_requested = false;
  return true;
}
