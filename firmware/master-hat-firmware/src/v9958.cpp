#include "v9958.h"

V9958::V9958() {
    memset(_vram, 0, sizeof(_vram));
    memset(_regs, 0, sizeof(_regs));
    memset(_status_regs, 0, sizeof(_status_regs));
    memset(_palette, 0, sizeof(_palette));
    
    _vram_ptr = 0;
    _vram_read_mode = false;
    _latch = 0;
    _latch_ready = false;
    _palette_ptr = 0;
    
    // Initialize default palette (MSX2 default)
    // Placeholder simple palette
    _palette[0] = 0x000; // Transparent/Black
    _palette[1] = 0x000; // Black
    _palette[2] = 0x021; // Medium Green
    _palette[3] = 0x032; // Light Green
    _palette[4] = 0x011; // Dark Blue
    _palette[5] = 0x113; // Light Blue
    _palette[6] = 0x200; // Dark Red
    _palette[7] = 0x133; // Cyan
    _palette[8] = 0x300; // Medium Red
    _palette[9] = 0x311; // Light Red
    _palette[10]= 0x330; // Dark Yellow
    _palette[11]= 0x332; // Light Yellow
    _palette[12]= 0x020; // Dark Green
    _palette[13]= 0x213; // Magenta
    _palette[14]= 0x222; // Gray
    _palette[15]= 0x333; // White

    // Standard 4x4 Bayer Dither Matrix for 3-bit mapping
    // Normalized for 0-7 scaling logic
    uint8_t bayer[4][4] = {
        {  0,  8,  2, 10 },
        { 12,  4, 14,  6 },
        {  3, 11,  1,  9 },
        { 15,  7, 13,  5 }
    };
    memcpy(_bayer_matrix, bayer, sizeof(_bayer_matrix));
    
    // Map colors to bitfields for rendering
    // Since hardware color is R=bit2, G=bit1, B=bit0
    // Wait, VGA hardware mapping: R=bit2, G=bit1, B=bit0?
    // In our driver: we map an index (0-7) to the actual pins.
    // Let's assume standard 3-bit color: 0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red, 5=Magenta, 6=Yellow, 7=White
    for (int i=0; i<8; i++) {
        _r_map[i] = (i & 4) ? 1 : 0;
        _g_map[i] = (i & 2) ? 1 : 0;
        _b_map[i] = (i & 1) ? 1 : 0;
    }
}

void V9958::write_register(uint8_t reg, uint8_t data) {
    if (reg < 47) {
        _regs[reg] = data;
        
        // Handle specific register updates
        if (reg == 14) {
            // VRAM address pointer high bits
            _vram_ptr = (_vram_ptr & 0x03FFF) | ((data & 0x07) << 14);
        }
    }
}

uint8_t V9958::read_status_register(uint8_t reg) {
    if (reg < 10) {
        return _status_regs[reg];
    }
    return 0;
}

uint8_t V9958::read_port(uint16_t addr) {
    uint8_t data = 0;
    
    if (addr == 0xFF78) {
        // Port #98: VRAM Data Read
        data = _vram[_vram_ptr];
        _vram_ptr = (_vram_ptr + 1) & 0x1FFFF; // 128KB wrap
    } else if (addr == 0xFF79) {
        // Port #99: Status Register Read
        uint8_t status_idx = _regs[15]; // R#15 selects the status register
        data = read_status_register(status_idx);
    }
    
    return data;
}

void V9958::write_port(uint16_t addr, uint8_t data) {
    if (addr == 0xFF78) {
        // Port #98: VRAM Data Write
        _vram[_vram_ptr] = data;
        _vram_ptr = (_vram_ptr + 1) & 0x1FFFF; // 128KB wrap
    } else if (addr == 0xFF79) {
        // Port #99: Register / Address Setup
        if (!_latch_ready) {
            _latch = data;
            _latch_ready = true;
        } else {
            if ((data & 0x80) == 0) {
                // Bit 7 is 0: Set VRAM address
                _vram_ptr = (_vram_ptr & 0x1C000) | ((data & 0x3F) << 8) | _latch;
                _vram_read_mode = ((data & 0x40) == 0); // Bit 6 = 0 means read
            } else {
                // Bit 7 is 1: Write Register
                write_register(data & 0x3F, _latch);
            }
            _latch_ready = false;
        }
    } else if (addr == 0xFF7A) {
        // Port #9A: Palette Write
        // V9958 expects 2 bytes per palette entry (R/B, then G)
        // Ignoring for simple implementation right now.
        // Needs proper latching for high/low nibbles.
    } else if (addr == 0xFF7B) {
        // Port #9B: Indirect Register Write
        uint8_t target_reg = _regs[17] & 0x3F;
        write_register(target_reg, data);
        
        // Auto-increment unless inhibited (bit 7 of R#17)
        if ((_regs[17] & 0x80) == 0) {
            _regs[17] = (target_reg + 1) & 0x3F;
        }
    }
}

void V9958::render_scanline(uint16_t line, uint8_t* out_buffer) {
    // Basic Text Mode 2 (80 column) rendering stub.
    // In Text Mode 2, resolution is usually 512 pixels wide? 
    // Wait, SCREEN 0 width 80 means 80 columns of 6 pixels = 480 pixels wide.
    // We will render it centered on our 640 pixel display.
    
    int margin_left = (640 - 480) / 2;
    int margin_right = margin_left + 480;
    
    // Bg/Fg colors from R#7
    uint8_t fg_col = _regs[7] >> 4;
    uint8_t bg_col = _regs[7] & 0x0F;
    
    uint16_t fg_rgb = _palette[fg_col];
    uint16_t bg_rgb = _palette[bg_col];
    
    // Pattern Name Table base (R#2)
    uint32_t pnt_base = (_regs[2] & 0x7F) << 10;
    
    // Pattern Generator Table base (R#4)
    uint32_t pgt_base = (_regs[4] & 0x3F) << 11;
    
    // VDP line relative to scroll (simplified)
    uint16_t vdp_line = line; 
    
    for (int x = 0; x < 640; x++) {
        if (x < margin_left || x >= margin_right || vdp_line >= 212) { // Standard 212 lines
            out_buffer[x] = bg_col & 0x07; // Fast approximation of border color
            continue;
        }
        
        int text_x = x - margin_left;
        int col = text_x / 6;
        int row = vdp_line / 8;
        
        int char_idx = row * 80 + col;
        uint8_t char_code = _vram[pnt_base + char_idx];
        
        int bit_col = text_x % 6;
        int bit_row = vdp_line % 8;
        
        uint8_t font_byte = _vram[pgt_base + (char_code * 8) + bit_row];
        bool pixel_on = (font_byte & (0x80 >> bit_col)) != 0;
        
        uint16_t src_color = pixel_on ? fg_rgb : bg_rgb;
        
        // --- 4x4 Bayer Dithering (9-bit RGB to 3-bit RGB) ---
        // V9958 Palette format: 16-bit word per color.
        // Actually typically it's encoded as R(3), G(3), B(3).
        // Let's assume src_color format is 0000 RRR0 GGG0 BBB0
        uint8_t r = (src_color >> 8) & 0x07; // 0-7
        uint8_t g = (src_color >> 4) & 0x07; // 0-7
        uint8_t b = src_color & 0x07;        // 0-7
        
        // Dither threshold (0-15)
        uint8_t threshold = _bayer_matrix[vdp_line % 4][x % 4];
        
        // Scale 0-7 color up to 0-31 range for dithering logic?
        // Since input is 3 bits per channel (0-7) and output is 1 bit (0 or 1),
        // we can add the threshold and shift.
        // A simple approach: (value * 2) + threshold > 15 ? 1 : 0
        uint8_t r_out = ((r * 2) + threshold > 15) ? 1 : 0;
        uint8_t g_out = ((g * 2) + threshold > 15) ? 1 : 0;
        uint8_t b_out = ((b * 2) + threshold > 15) ? 1 : 0;
        
        out_buffer[x] = (r_out << 2) | (g_out << 1) | b_out;
    }
}
