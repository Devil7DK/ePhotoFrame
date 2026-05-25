#include <lvgl.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <LittleFS.h>

// Calibration is rotation-specific; regenerate if setRotation() changes.
uint16_t touch_cal_data[5] = { 189, 3519, 273, 3565, 1 };

extern "C" void lv_fs_arduino_esp_littlefs_init(void);
extern "C" void lv_fs_arduino_sd_init(void);

#define LVGL_TICK_PERIOD 5
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define IMAGE_FOLDER "/images"

TFT_eSPI tft = TFT_eSPI();

#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5
// Must be HSPI: TFT_eSPI claims VSPI, and sharing it silently breaks tft.getTouch().
SPIClass sdSPI(HSPI);

lv_obj_t *img_obj;
lv_obj_t *no_photo_label;
lv_obj_t *btn_prev;
lv_obj_t *btn_next;
lv_obj_t *btn_settings;

String photo_paths[32];
int photo_count = 0;
int current_photo_index = 0;

void lv_tick_handler() {
  lv_tick_inc(LVGL_TICK_PERIOD);
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)px_map, w * h, true);
  tft.endWrite();
  lv_display_flush_ready(disp);
}

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

void show_photo(const char *path) {
  Serial.print("[INFO] Loading photo: ");
  Serial.println(path);

  if (img_obj)
    lv_obj_del(img_obj);
  img_obj = lv_image_create(lv_screen_active());
  lv_image_set_src(img_obj, path);
  lv_obj_center(img_obj);
  lv_obj_move_background(img_obj);
  lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_SCROLLABLE);

  if (no_photo_label)
    lv_obj_add_flag(no_photo_label, LV_OBJ_FLAG_HIDDEN);
}

void show_next_photo() {
  if (photo_count == 0)
    return;
  current_photo_index = (current_photo_index + 1) % photo_count;
  show_photo(photo_paths[current_photo_index].c_str());
}

void show_prev_photo() {
  if (photo_count == 0)
    return;
  current_photo_index = (current_photo_index - 1 + photo_count) % photo_count;
  show_photo(photo_paths[current_photo_index].c_str());
}

void btn_event_cb(lv_event_t *e) {
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  if (btn == btn_next)
    show_next_photo();
  else if (btn == btn_prev)
    show_prev_photo();
  else if (btn == btn_settings) {
    Serial.println("Settings pressed");
  }
}

void scan_photos() {
  File root = SD.open(IMAGE_FOLDER);
  if (!root || !root.isDirectory()) {
    Serial.println("[WARN] SD image folder not found");
    return;
  }

  File file = root.openNextFile();
  while (file && photo_count < 32) {
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
}

void create_ui() {
  btn_settings = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn_settings, 32, 32);
  lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_add_event_cb(btn_settings, btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *settings_img = lv_image_create(btn_settings);
  lv_image_set_src(settings_img, "S:/settings.png");
  lv_obj_center(settings_img);

  if (photo_count == 0) {
    no_photo_label = lv_label_create(lv_screen_active());
    lv_label_set_text(no_photo_label, "No photos found");
    lv_obj_center(no_photo_label);
    return;
  }

  show_photo(photo_paths[0].c_str());

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

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("[INFO] Booting");

  lv_init();
  tft.begin();
  tft.setRotation(1);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI))
    Serial.println("[ERROR] SD init failed");
  else
    Serial.println("[OK] SD mounted");

  if (!LittleFS.begin())
    Serial.println("[ERROR] LittleFS init failed");
  else
    Serial.println("[OK] LittleFS mounted");

  tft.setTouch(touch_cal_data);

  lv_fs_arduino_esp_littlefs_init();
  lv_fs_arduino_sd_init();

  lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, malloc(SCREEN_WIDTH * 10 * sizeof(lv_color_t)), NULL, SCREEN_WIDTH * 10, LV_DISPLAY_RENDER_MODE_PARTIAL);

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

  scan_photos();
  create_ui();
}

void loop() {
  lv_timer_handler();
  delay(5);
}