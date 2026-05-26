#pragma once

// Screen builders — each one cleans the active screen and rebuilds.
void ui_show_setup_screen();
void ui_show_photos();
void ui_show_settings_menu();
void ui_show_manage_mode();

// Called every loop() tick — applies pending screen transitions and refreshes
// the manage-mode URL line when WiFi state changes.
void ui_update();

// Latched flags consumed by .ino to drive ESP.restart() with the right cleanup.
bool ui_wifi_reset_requested();
bool ui_factory_reset_requested();
bool ui_restart_requested();

// True once when the user has entered manage mode from the device settings —
// the .ino consumes this on the loop task and starts WiFi STA + the web
// server. Kept off the LVGL task so heavy WiFi driver init doesn't block
// display refresh.
bool ui_consume_enter_manage();
