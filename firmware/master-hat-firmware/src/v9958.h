#pragma once
#include <Arduino.h>

class V9958 {
public:
    V9958();
    
    // Core 1 Bus access methods
    void write_port(uint16_t addr, uint8_t data);
    uint8_t read_port(uint16_t addr);

    // Core 0 scanline rendering
    // Populates a buffer of 640 3-bit hardware colors (0-7)
    void render_scanline(uint16_t line, uint8_t* out_buffer);

private:
    // 128KB VRAM
    uint8_t _vram[128 * 1024];
    
    // VDP Registers
    uint8_t _regs[47];
    uint8_t _status_regs[10];
    
    // Palette RAM (16 colors, 9-bit RGB)
    uint16_t _palette[16];
    
    // Port state machine
    uint32_t _vram_ptr;
    bool _vram_read_mode;
    uint8_t _latch;
    bool _latch_ready;
    uint8_t _palette_ptr;
    
    // Internal rendering state
    uint8_t _bayer_matrix[4][4];
    
    // Color mapping lookup
    uint8_t _r_map[8];
    uint8_t _g_map[8];
    uint8_t _b_map[8];

    // Helper functions
    void write_register(uint8_t reg, uint8_t data);
    uint8_t read_status_register(uint8_t reg);
};
