#include "spo256.h"

extern "C" {
    extern const uint8_t *allophoneindex[64];
    extern const int allophonesizeCorrected[64];
}

SPO256::SPO256() {
    reset();
}

void SPO256::reset() {
    busy = false;
    current_allophone = 0;
    current_sample_index = 0;
    upsample_counter = 0;
}

void SPO256::write_allophone(uint8_t allophone) {
    if (allophone < 64) {
        current_allophone = allophone;
        current_sample_index = 0;
        upsample_counter = 0;
        busy = true;
    }
}

int16_t SPO256::get_sample() {
    if (!busy) return 0;
    
    uint8_t raw_sample = allophoneindex[current_allophone][current_sample_index];
    int16_t sample = ((int16_t)raw_sample - 128) << 8;
    
    upsample_counter++;
    if (upsample_counter >= 4) {
        upsample_counter = 0;
        current_sample_index++;
        if (current_sample_index >= allophonesizeCorrected[current_allophone]) {
            busy = false;
        }
    }
    
    return sample;
}

bool SPO256::is_busy() {
    return busy;
}
