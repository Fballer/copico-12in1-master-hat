#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include <Arduino.h>

class Rtc {
public:
    Rtc();
    void init();
    
    // Core memory interface for $FF50, $FF51, and $FF75
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t data);
    
    // Time sync from ESP32 via SPI
    void sync_time(uint32_t ntp_time);
    
    // Check if there's a timezone index to send to ESP32
    bool has_pending_tz_update();
    uint8_t get_pending_tz_index();
    
private:
    uint8_t address_latch;
    uint32_t unix_time;
    uint32_t last_millis;
    
    bool pending_tz;
    uint8_t tz_index;
    
    void update_internal_time();
};

#endif // RTC_H
