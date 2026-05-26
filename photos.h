#pragma once

void photos_scan();
int  photos_count();
void photos_show_first();
void photos_show_next();
void photos_show_prev();
// Call after lv_obj_clean(lv_screen_active()) — clears the internal img_obj
// pointer that would otherwise dangle until the next show_at() recreates it.
void photos_reset();
