#pragma once

void photos_scan();
int photos_count();
void photos_show_first();
void photos_show_next();
void photos_show_prev();
// Call after lv_obj_clean(lv_screen_active()) — clears the internal img_obj
// pointer that would otherwise dangle until the next show_at() recreates it.
void photos_reset();
// Call every LVGL tick while the photos screen is active. Advances to the
// next photo automatically when config_get_autoplay_ms() worth of time has
// passed since the last show. Any manual show_first/next/prev resets the
// timer, so user navigation pauses autoplay for a full interval.
void photos_update();
