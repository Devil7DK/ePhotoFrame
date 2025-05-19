#include <lvgl.h>
#include <TFT_eSPI.h>

#define LVGL_TICK_PERIOD 5

TFT_eSPI tft = TFT_eSPI();  // Uses your User_Setup.h config

// LVGL tick handler
void lv_tick_handler() {
  lv_tick_inc(LVGL_TICK_PERIOD);
}

// Display flush callback
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)px_map, w * h, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

// Touch callback using TFT_eSPI's getTouch()
void my_touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t touchX, touchY;
  if (tft.getTouch(&touchX, &touchY)) {
    data->point.x = touchX;
    data->point.y = touchY;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Button event
static void btn_event_cb(lv_event_t *e) {
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  lv_obj_t *label = (lv_obj_t *)lv_obj_get_child(btn, 0);
  lv_label_set_text(label, "Clicked!");
}

void setup() {
  Serial.begin(115200);
  lv_init();

  tft.begin();
  tft.setRotation(1);  // Landscape

  // Create LVGL display
  lv_display_t *disp = lv_display_create(320, 240);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp,
                         malloc(320 * 10 * sizeof(lv_color_t)),
                         NULL,
                         320 * 10,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Register touch input using TFT_eSPI
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touch_cb);

  // Create button
  lv_obj_t *btn = lv_button_create(lv_screen_active());
  lv_obj_center(btn);
  lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, "Click Me!");
  lv_obj_center(label);

  // Tick timer
  static const esp_timer_create_args_t tick_args = {
    .callback = reinterpret_cast<esp_timer_cb_t>(lv_tick_handler),
    .name = "lv_tick"
  };
  esp_timer_handle_t tick_timer;
  esp_timer_create(&tick_args, &tick_timer);
  esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD * 1000);
}

void loop() {
  lv_timer_handler();
  delay(5);
}
