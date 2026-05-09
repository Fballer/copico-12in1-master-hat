#include "ay8913.h"

AY8913::AY8913() {
    // CoCo system clock is ~895 kHz, but some PSG boards used a 1.78977 MHz clock.
    // We'll use 1.78977 MHz for standard AY-3-8910. Output rate 44100 Hz.
    psg = PSG_new(1789772, 44100);
    PSG_setVolumeMode(psg, 1); // 1 = YM2149/AY-3-8910 mode
    reset();
}

AY8913::~AY8913() {
    PSG_delete(psg);
}

void AY8913::reset() {
    PSG_reset(psg);
}

void AY8913::write_register(uint8_t reg, uint8_t data) {
    PSG_writeReg(psg, reg, data);
}

void AY8913::tick() {
    // PSG_calc already advances state per sample
}

int16_t AY8913::get_sample() {
    return PSG_calc(psg);
}
