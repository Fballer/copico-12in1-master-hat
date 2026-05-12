#include "rtc.h"
#include <time.h>

Rtc::Rtc() : address_latch(0), unix_time(0), last_millis(0), pending_tz(false), tz_index(0) {
}

void Rtc::init() {
    last_millis = millis();
}

void Rtc::update_internal_time() {
    uint32_t current_millis = millis();
    if (current_millis - last_millis >= 1000) {
        uint32_t elapsed_seconds = (current_millis - last_millis) / 1000;
        unix_time += elapsed_seconds;
        last_millis = current_millis - ((current_millis - last_millis) % 1000);
    }
}

void Rtc::sync_time(uint32_t ntp_time) {
    // Only update if the time changed significantly to avoid jitter, or just take it as gospel
    unix_time = ntp_time;
    last_millis = millis();
}

bool Rtc::has_pending_tz_update() {
    return pending_tz;
}

uint8_t Rtc::get_pending_tz_index() {
    pending_tz = false;
    return tz_index;
}

uint8_t Rtc::read(uint16_t address) {
    if (address == 0xFF50) {
        update_internal_time();
        
        // Break down unix_time into MSM5832 format
        // Use gmtime because ESP32 already applied the timezone offset to the unix_time it sent
        // Wait, standard Unix epoch is ALWAYS UTC. 
        // We need ESP32 to either send the timezone-adjusted time as the "epoch", 
        // OR we use gmtime and rely on ESP32 sending a falsified epoch that represents local time.
        // It's easiest if ESP32 sends `local_time_epoch = utc_epoch + tz_offset_seconds`
        // Then gmtime() will extract the correct local Y/M/D H:M:S.
        time_t t = (time_t)unix_time;
        struct tm *tm_info = gmtime(&t);
        
        if (!tm_info) return 0xFF; // Fallback
        
        int sec = tm_info->tm_sec;
        int min = tm_info->tm_min;
        int hour = tm_info->tm_hour; // 24-hour format
        int day = tm_info->tm_mday;
        int mon = tm_info->tm_mon + 1;
        int year = tm_info->tm_year % 100; // 2 digit year
        
        switch (address_latch & 0x0F) {
            case 0: return sec % 10;
            case 1: return sec / 10;
            case 2: return min % 10;
            case 3: return min / 10;
            case 4: return hour % 10;
            case 5: return hour / 10; // 24h mode
            case 6: return day % 10;
            case 7: return day / 10;
            case 8: return mon % 10;
            case 9: return mon / 10;
            case 10: return year % 10;
            case 11: return year / 10;
            case 12: return tm_info->tm_wday; // Day of week (0-6)
            default: return 0xFF;
        }
    }
    return 0xFF; // Open bus for 0xFF51 read
}

void Rtc::write(uint16_t address, uint8_t data) {
    if (address == 0xFF51) {
        // Address latch
        address_latch = data & 0x0F;
    } else if (address == 0xFF75) {
        // Timezone configuration register from BIOS
        tz_index = data;
        pending_tz = true;
    }
}
