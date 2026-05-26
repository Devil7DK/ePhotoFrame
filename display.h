#pragma once

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

void display_init_tft();
void display_init_lvgl();

// Spawns a dedicated FreeRTOS task that owns all LVGL state — runs
// lv_timer_handler() + ui_update() in a tight loop, pinned to core 1 at a
// priority above AsyncTCP so network activity can never starve display
// refresh. Call as the very last line of setup() — once it's running, no
// other task may touch LVGL.
void display_start_lvgl_task();
