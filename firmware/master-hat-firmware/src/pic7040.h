#pragma once
#include <Arduino.h>
#include "ay8913.h"
#include "spo256.h"

class Pic7040 {
public:
    Pic7040();
    
    void reset();
    void write_data(uint8_t data);
    uint8_t read_status();
    
    void tick();
    void generate_audio(int16_t *left, int16_t *right);

private:
    uint8_t status_byte;
    bool is_busy;
    
    // Command buffer (from CoCo)
    uint8_t cmd_buffer[256];
    uint16_t cmd_head;
    uint16_t cmd_tail;

    // Allophone queue (to play)
    uint8_t allo_queue[1024];
    uint16_t allo_head;
    uint16_t allo_tail;

    void update_status();
    void process_command();
    void simple_tts(uint8_t ascii);

    AY8913 ay;
    SPO256 spo;
};
