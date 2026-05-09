#pragma once
#include <stdint.h>
#include "emu2149.h"

class AY8913 {
public:
    AY8913();
    ~AY8913();
    void reset();
    void write_register(uint8_t reg, uint8_t data);
    void tick(); // Not needed if we calculate samples directly
    int16_t get_sample();

private:
    PSG *psg;
};
