#pragma once

void storage_init();

// SD bus is shared between LVGL (main loop) and the web server's
// AsyncTCP task. The SD library is not reentrant; every caller must
// hold this lock for the full open/read/write/close window.
bool storage_sd_lock(unsigned long timeout_ms = 1000);
void storage_sd_unlock();
