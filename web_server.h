#pragma once

enum WebMode { MODE_SETUP,
               MODE_MANAGE };

void web_server_begin(WebMode mode);
void web_server_update();
bool web_server_restart_requested();
bool web_server_factory_reset_requested();
