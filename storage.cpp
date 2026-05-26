#include "storage.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5

// Must be HSPI: TFT_eSPI claims VSPI, and sharing it silently breaks tft.getTouch().
static SPIClass sdSPI(HSPI);

static SemaphoreHandle_t sd_mutex = nullptr;

extern "C" void lv_fs_arduino_esp_littlefs_init(void);
extern "C" void lv_fs_arduino_sd_init(void);

void storage_init() {
  sd_mutex = xSemaphoreCreateMutex();
  if (!sd_mutex)
    Serial.println("[ERROR] SD mutex alloc failed");

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI))
    Serial.println("[ERROR] SD init failed");
  else
    Serial.println("[OK] SD mounted");

  if (!LittleFS.begin())
    Serial.println("[ERROR] LittleFS init failed");
  else
    Serial.println("[OK] LittleFS mounted");
}

void storage_register_lvgl_drivers() {
  lv_fs_arduino_esp_littlefs_init();
  lv_fs_arduino_sd_init();
}

bool storage_sd_lock(unsigned long timeout_ms) {
  if (!sd_mutex)
    return true;
  return xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void storage_sd_unlock() {
  if (sd_mutex)
    xSemaphoreGive(sd_mutex);
}
