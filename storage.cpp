#include "storage.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <LittleFS.h>

#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5

// Must be HSPI: TFT_eSPI claims VSPI, and sharing it silently breaks tft.getTouch().
static SPIClass sdSPI(HSPI);

extern "C" void lv_fs_arduino_esp_littlefs_init(void);
extern "C" void lv_fs_arduino_sd_init(void);

void storage_init() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI))
    Serial.println("[ERROR] SD init failed");
  else
    Serial.println("[OK] SD mounted");

  if (!LittleFS.begin())
    Serial.println("[ERROR] LittleFS init failed");
  else
    Serial.println("[OK] LittleFS mounted");

  lv_fs_arduino_esp_littlefs_init();
  lv_fs_arduino_sd_init();
}
