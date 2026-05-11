// Copico Stub: replaces the NVS mode manager from the old New_Porta_Serial project.
// On Copico, the RP2350 controls the mode — the ESP32 is always in WiModem mode.
#pragma once
#include <Arduino.h>

enum SystemMode {
    MODE_PRINTER = 1,
    MODE_CASSETTE,
    MODE_WIMODEM,
    MODE_UI_SETUP,
    MODE_WEB_SETUP
};

inline SystemMode mode_get_active() { return MODE_WIMODEM; }
inline void mode_set_active_and_reboot(SystemMode) { ESP.restart(); }
inline uint32_t mode_get_baud_rate() { return 115200; }
inline void mode_set_baud_rate(uint32_t) {}
inline uint32_t mode_get_serial_config() { return SERIAL_8N1; }
inline void mode_set_serial_config(uint32_t) {}
