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
