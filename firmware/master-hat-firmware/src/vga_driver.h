#pragma once
#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "v9958.h"

class VgaDriver {
public:
    VgaDriver(V9958* vdp);

    // Initialize the VGA driver and start PIO/DMA
    void init();

    // ================================================================
    // tick(): Called from Core 0 loop to render scanlines into the
    // inactive buffer. NON-BLOCKING — returns immediately if the DMA
    // is not ready for a new line. No delay(), no while() spin.
    // ================================================================
    void tick();

    // DMA completion ISR (public so it can be registered as C callback)
    static void on_dma_complete();

private:
    V9958* _vdp;

    // PIO state machines (all on PIO1 to avoid conflict with bus sniffer on PIO0)
    PIO _pio;
    uint _sm_hsync;
    uint _sm_vsync;
    uint _sm_rgb;

    // DMA channels
    int _dma_rgb;
    int _dma_ctrl;

    // Double buffered scanlines (each = 320 x 32-bit words = 640 pixel instructions)
    // Placed in RAM section for fastest DMA access.
    uint32_t _scanline_buffer[2][320];

    // Pointer to the buffer currently being DMA'd to the PIO FIFO.
    // Written ONLY by the DMA ISR (atomic pointer swap).
    volatile uint32_t* _dma_read_ptr;

    // Index of the buffer the DMA is currently reading (0 or 1).
    // Written ONLY by the DMA ISR.
    volatile uint8_t _active_buffer_idx;

    // Incremented by the DMA ISR each time a scanline completes.
    // Read and decremented by tick(). tick() renders one line per count.
    volatile uint32_t _scanlines_completed;

    // The line number tick() should render NEXT into the inactive buffer.
    int _next_render_line;

    void init_pio();
    void init_dma();
    void build_color_table();
    uint16_t _color_instruction[8];
};

// Global pointer used by the static ISR to reach the VgaDriver instance.
extern VgaDriver* g_vga_driver_instance;
