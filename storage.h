#pragma once

// Mounts SD (on HSPI) + LittleFS and creates the SD mutex. Safe to call any
// time after Serial.begin().
void storage_init();

// Registers LVGL's filesystem drivers for SD ("D:") and LittleFS ("S:").
// MUST be called AFTER lv_init() — otherwise LVGL's FS subsystem is
// uninitialized and the registration crashes.
void storage_register_lvgl_drivers();

// SD bus is shared between LVGL (main loop) and the web server's
// AsyncTCP task. The SD library is not reentrant; every caller must
// hold this lock for the full open/read/write/close window.
bool storage_sd_lock(unsigned long timeout_ms = 1000);
void storage_sd_unlock();
