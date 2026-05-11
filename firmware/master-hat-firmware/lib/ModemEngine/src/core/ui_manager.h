// Copico Stub: replaces the OLED UI manager from the old New_Porta_Serial project.
// These functions are no-ops since the Copico hat has no OLED display.
#pragma once
#include <Arduino.h>

inline void ui_manager_init() {}
inline void ui_set_status(const char*) {}
inline void ui_set_modem_status(const char*, ...) {}
inline void ui_push_live_char(char) {}
