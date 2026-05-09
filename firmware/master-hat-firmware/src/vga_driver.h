#pragma once
#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "v9958.h"

class VgaDriver {
public:
    VgaDriver(V9958* vdp);
    
    // Initialize the VGA driver and start PIO/DMA
    void init();
    
    // Core 0 loop function to keep the scanline buffer fed
    void tick();

private:
    V9958* _vdp;
    
    // PIO state machines
    PIO _pio;
    uint _sm_hsync;
    uint _sm_vsync;
    uint _sm_rgb;
    
    // DMA channels
    int _dma_rgb;
    int _dma_ctrl;
    
    // Double buffered scanlines (each stores 640 16-bit instructions -> 320 32-bit words)
    uint32_t _scanline_buffer[2][320];
    
    // Pointer to active buffer for DMA to read from
    uint32_t* _dma_read_ptr;
    
    int _current_render_line;
    uint8_t _active_buffer_idx;
    
    void init_pio();
    void init_dma();
    
    // Precalculate the 16-bit PIO instructions for the 8 hardware colors
    void build_color_table();
    uint16_t _color_instruction[8];
};
