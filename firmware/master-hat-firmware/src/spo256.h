#pragma once
#include <stdint.h>

class SPO256 {
public:
    SPO256();
    void reset();
    void write_allophone(uint8_t allophone);
    int16_t get_sample(); // get speech sample at 44100 Hz
    bool is_busy();

private:
    bool busy;
    uint8_t current_allophone;
    int current_sample_index;
    int upsample_counter;
};
