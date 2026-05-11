#pragma once
#include <Arduino.h>

class Acia6551 {
public:
    Acia6551(Stream* serial);
    
    void init(bool turbo_mode);
    void set_turbo_mode(bool turbo_mode);
    
    typedef void (*BaudCallback)(uint32_t);
    void set_baud_callback(BaudCallback cb) { _baud_callback = cb; }

    // Register Handlers
    uint8_t read_status();
    uint8_t read_data();
    void write_command(uint8_t val);
    void write_control(uint8_t val);
    void write_data(uint8_t val);

private:
    Stream* _serial;
    BaudCallback _baud_callback = nullptr;
    bool _turbo_mode;

    uint8_t _command_reg;
    uint8_t _control_reg;
    
    // Internal baud rate calculation
    void update_baud_rate();
    uint32_t get_baud_rate_from_control();
};
