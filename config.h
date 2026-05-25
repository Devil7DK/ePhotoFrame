#pragma once

#include <Arduino.h>

bool config_load();
bool config_has_wifi();
const char* config_get_ssid();
const char* config_get_password();
void config_set_wifi(const char* ssid, const char* password);

uint32_t config_get_autoplay_ms();
void config_set_autoplay_ms(uint32_t ms);

void config_request_save();
void config_commit_pending(bool force = false);

void config_clear_wifi();
void config_factory_reset();
