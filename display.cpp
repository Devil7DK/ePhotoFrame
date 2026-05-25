#include "display.h"
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <esp_timer.h>

#define LVGL_TICK_PERIOD 5

// Calibration is rotation-specific; regenerate if setRotation() changes.
static uint16_t touch_cal_data[5] = { 189, 3519, 273, 3565, 1 };

static TFT_eSPI tft = TFT_eSPI();

static void lv_tick_handler() {
  lv_tick_inc(LVGL_TICK_PERIOD);
}

static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)px_map, w * h, true);
  tft.endWrite();
  lv_display_flush_ready(disp);
}

static void my_touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t touchX, touchY;
  if (tft.getTouch(&touchX, &touchY)) {
    data->point.x = touchX;
    data->point.y = touchY;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void display_init_tft() {
  lv_init();
  tft.begin();
  tft.setRotation(1);
}

void display_init_lvgl() {
  tft.setTouch(touch_cal_data);

  lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, malloc(SCREEN_WIDTH * 10 * sizeof(lv_color_t)), NULL,
                         SCREEN_WIDTH * 10, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touch_cb);

  static const esp_timer_create_args_t tick_args = {
    .callback = reinterpret_cast<esp_timer_cb_t>(lv_tick_handler),
    .name = "lv_tick"
  };
  esp_timer_handle_t tick_timer;
  esp_timer_create(&tick_args, &tick_timer);
  esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD * 1000);
}
