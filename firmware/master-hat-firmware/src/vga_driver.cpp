#include "vga_driver.h"
#include "vga_pio.pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"

// Hardware Pin Definitions (From Build Guide)
#define PIN_GREEN 8
#define PIN_BLUE  9
#define PIN_RED   14
#define PIN_VSYNC 16
#define PIN_HSYNC 18

VgaDriver::VgaDriver(V9958* vdp) : _vdp(vdp) {
    _current_render_line = 0;
    _active_buffer_idx = 0;
}

void VgaDriver::init() {
    build_color_table();

    // Init pins
    gpio_init(PIN_RED);
    gpio_init(PIN_GREEN);
    gpio_init(PIN_BLUE);
    gpio_init(PIN_HSYNC);
    gpio_init(PIN_VSYNC);

    gpio_set_dir(PIN_RED, GPIO_OUT);
    gpio_set_dir(PIN_GREEN, GPIO_OUT);
    gpio_set_dir(PIN_BLUE, GPIO_OUT);
    gpio_set_dir(PIN_HSYNC, GPIO_OUT);
    gpio_set_dir(PIN_VSYNC, GPIO_OUT);

    _pio = pio0; // Using PIO0

    init_pio();
    init_dma();
}

void VgaDriver::build_color_table() {
    // We map 3-bit color (0-7) to a 16-bit PIO instruction
    // PIO instruction format for: set pins, <data> side <side_data>
    // Opcode: 111 (SET) | S (Sideset) | DDDD (Delay=7) | 000 (Destination) | 000 GB (Data)
    // SIDESET base = 14 (RED), SET base = 8 (GREEN, BLUE)
    // Green is bit 0 of SET, Blue is bit 1 of SET.
    // Red is bit 0 of SIDESET.
    
    // Base instruction: 111_0_0111_000_00000 = 0xE700
    for (int i = 0; i < 8; i++) {
        uint16_t instr = 0xE700;
        
        bool r = (i & 0x4) ? 1 : 0;
        bool g = (i & 0x2) ? 1 : 0;
        bool b = (i & 0x1) ? 1 : 0;
        
        if (r) instr |= (1 << 12); // Sideset bit 0 is at bit 12
        if (g) instr |= (1 << 0);  // Green is bit 0 of data
        if (b) instr |= (1 << 1);  // Blue is bit 1 of data
        
        _color_instruction[i] = instr;
    }
}

void VgaDriver::init_pio() {
    // 1. Add programs to PIO
    uint offset_hsync = pio_add_program(_pio, &vga_hsync_program);
    uint offset_vsync = pio_add_program(_pio, &vga_vsync_program);
    uint offset_rgb   = pio_add_program(_pio, &vga_rgb_program);

    _sm_hsync = pio_claim_unused_sm(_pio, true);
    _sm_vsync = pio_claim_unused_sm(_pio, true);
    _sm_rgb   = pio_claim_unused_sm(_pio, true);

    // 2. Configure HSYNC SM
    pio_sm_config c_hsync = vga_hsync_program_get_default_config(offset_hsync);
    sm_config_set_set_pins(&c_hsync, PIN_HSYNC, 1);
    pio_gpio_init(_pio, PIN_HSYNC);
    pio_sm_set_consecutive_pindirs(_pio, _sm_hsync, PIN_HSYNC, 1, true);
    sm_config_set_clkdiv(&c_hsync, 10.0f); // 250MHz / 10 = 25MHz
    pio_sm_init(_pio, _sm_hsync, offset_hsync, &c_hsync);

    // 3. Configure VSYNC SM
    pio_sm_config c_vsync = vga_vsync_program_get_default_config(offset_vsync);
    sm_config_set_set_pins(&c_vsync, PIN_VSYNC, 1);
    sm_config_set_sideset_pins(&c_vsync, PIN_VSYNC);
    pio_gpio_init(_pio, PIN_VSYNC);
    pio_sm_set_consecutive_pindirs(_pio, _sm_vsync, PIN_VSYNC, 1, true);
    sm_config_set_clkdiv(&c_vsync, 10.0f); // 25MHz
    pio_sm_init(_pio, _sm_vsync, offset_vsync, &c_vsync);

    // 4. Configure RGB SM
    pio_sm_config c_rgb = vga_rgb_program_get_default_config(offset_rgb);
    sm_config_set_set_pins(&c_rgb, PIN_GREEN, 2); // GREEN is 8, BLUE is 9
    sm_config_set_sideset_pins(&c_rgb, PIN_RED);  // RED is 14
    pio_gpio_init(_pio, PIN_GREEN);
    pio_gpio_init(_pio, PIN_BLUE);
    pio_gpio_init(_pio, PIN_RED);
    pio_sm_set_consecutive_pindirs(_pio, _sm_rgb, PIN_GREEN, 2, true);
    pio_sm_set_consecutive_pindirs(_pio, _sm_rgb, PIN_RED, 1, true);
    
    // Auto-pull configuration for OUT EXEC (pulls 32-bits automatically)
    sm_config_set_out_shift(&c_rgb, true, true, 32); 
    sm_config_set_clkdiv(&c_rgb, 1.0f); // 250MHz (10 cycles per pixel)
    
    // Important: we must configure OUT pins even if we execute SET, just to satisfy pio config logic?
    // Actually out_exec executes whatever instruction is provided.
    pio_sm_init(_pio, _sm_rgb, offset_rgb, &c_rgb);

    // 5. Pre-load values
    // HSYNC: 654
    pio_sm_put_blocking(_pio, _sm_hsync, 654);
    
    // VSYNC: 479
    pio_sm_put_blocking(_pio, _sm_vsync, 479);
    
    // RGB: 639
    pio_sm_put_blocking(_pio, _sm_rgb, 639);

    // 6. Start State Machines
    // Start HSYNC and VSYNC exactly together to keep them locked
    pio_enable_sm_mask_in_sync(_pio, (1u << _sm_hsync) | (1u << _sm_vsync));
    
    // Start RGB SM (it will wait for VSYNC IRQ)
    pio_sm_set_enabled(_pio, _sm_rgb, true);
}

void VgaDriver::init_dma() {
    _dma_rgb = dma_claim_unused_channel(true);
    _dma_ctrl = dma_claim_unused_channel(true);

    // Populate initial buffers with black
    for (int i = 0; i < 320; i++) {
        _scanline_buffer[0][i] = (_color_instruction[0] << 16) | _color_instruction[0];
        _scanline_buffer[1][i] = (_color_instruction[0] << 16) | _color_instruction[0];
    }
    _dma_read_ptr = &_scanline_buffer[0][0];

    // Data Channel (Sends instructions to RGB PIO)
    dma_channel_config c0 = dma_channel_get_default_config(_dma_rgb);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);
    channel_config_set_dreq(&c0, pio_get_dreq(_pio, _sm_rgb, true)); // Pace to RGB TX FIFO
    channel_config_set_chain_to(&c0, _dma_ctrl); // Chain to control channel

    dma_channel_configure(
        _dma_rgb,
        &c0,
        &_pio->txf[_sm_rgb], // Write to PIO TX FIFO
        _dma_read_ptr,       // Read from active scanline buffer
        320,                 // 320 words (640 instructions)
        false                // Do not start yet
    );

    // Control Channel (Reloads the Data Channel)
    dma_channel_config c1 = dma_channel_get_default_config(_dma_ctrl);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    channel_config_set_read_increment(&c1, false);
    channel_config_set_write_increment(&c1, false);

    dma_channel_configure(
        _dma_ctrl,
        &c1,
        &dma_hw->ch[_dma_rgb].read_addr, // Write new read address to Data channel
        &_dma_read_ptr,                  // Read from our pointer variable
        1,                               // 1 word
        false
    );

    // Start the process
    dma_channel_start(_dma_rgb);
}

void VgaDriver::tick() {
    // Simple polling loop to feed the scanline buffers.
    // In a real implementation, we'd sync this with VSYNC or DMA interrupts.
    // For now, we check if the active buffer index has changed by comparing
    // the DMA read address to our expected buffer.
    
    uint32_t current_dma_addr = (uint32_t)dma_hw->ch[_dma_rgb].read_addr;
    uint32_t active_buffer_start = (uint32_t)&_scanline_buffer[_active_buffer_idx][0];
    uint32_t active_buffer_end = active_buffer_start + sizeof(_scanline_buffer[0]);
    
    // If the DMA is currently reading from the active buffer,
    // we can safely render into the *inactive* buffer.
    if (current_dma_addr >= active_buffer_start && current_dma_addr < active_buffer_end) {
        uint8_t inactive_idx = _active_buffer_idx ^ 1;
        
        // Render 1 scanline (temp buffer for 3-bit colors)
        uint8_t pixel_colors[640];
        _vdp->render_scanline(_current_render_line, pixel_colors);
        
        // Convert to 16-bit PIO instructions
        for (int i = 0; i < 320; i++) {
            uint16_t instr0 = _color_instruction[pixel_colors[i * 2]];
            uint16_t instr1 = _color_instruction[pixel_colors[i * 2 + 1]];
            // Since we shift right in PIO, instr0 needs to be in the lower 16 bits
            _scanline_buffer[inactive_idx][i] = (instr1 << 16) | instr0;
        }
        
        _current_render_line++;
        if (_current_render_line >= 480) {
            _current_render_line = 0;
        }
        
        // Swap buffers for the next DMA cycle
        _active_buffer_idx = inactive_idx;
        _dma_read_ptr = &_scanline_buffer[_active_buffer_idx][0];
        
        // Wait until DMA switches to the new buffer before rendering again
        while ((uint32_t)dma_hw->ch[_dma_rgb].read_addr < active_buffer_end && 
               (uint32_t)dma_hw->ch[_dma_rgb].read_addr >= active_buffer_start) {
            // Tight loop (or yield)
            delayMicroseconds(1); 
        }
    }
}
