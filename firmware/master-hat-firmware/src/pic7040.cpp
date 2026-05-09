#include "pic7040.h"
#include "allophoneDefs.h"

Pic7040::Pic7040() {
    reset();
}

void Pic7040::reset() {
    is_busy = false;
    cmd_head = 0;
    cmd_tail = 0;
    allo_head = 0;
    allo_tail = 0;
    ay.reset();
    spo.reset();
    update_status();
}

void Pic7040::write_data(uint8_t data) {
    if (!is_busy) {
        cmd_buffer[cmd_head++] = data;
        if (cmd_head >= 256) cmd_head = 0;
        is_busy = true;
        update_status();
    }
}

uint8_t Pic7040::read_status() {
    return status_byte;
}

void Pic7040::update_status() {
    status_byte = 0xFF; 
    if (is_busy) status_byte &= ~(1 << 7);
    if (spo.is_busy() || allo_head != allo_tail) status_byte &= ~(1 << 6);
    // Sounding bit is rarely checked tightly, but we can set it to idle (1)
}

void Pic7040::tick() {
    // 1. Process incoming commands from CoCo
    if (is_busy) {
        process_command();
        is_busy = false;
        update_status();
    }

    // 2. Feed SPO256 if it's idle and we have queued allophones
    if (!spo.is_busy() && allo_tail != allo_head) {
        uint8_t a = allo_queue[allo_tail++];
        if (allo_tail >= 1024) allo_tail = 0;
        spo.write_allophone(a);
        update_status();
    }
}

void Pic7040::process_command() {
    if (cmd_tail != cmd_head) {
        uint8_t cmd = cmd_buffer[cmd_tail++];
        if (cmd_tail >= 256) cmd_tail = 0;
        
        // Very basic Command Interpreter
        if (cmd < 0x80) {
            // ASCII to simple phoneme (Basic Ruleset)
            simple_tts(cmd);
        } else {
            // Direct allophone or PSG command
            // For now, if > 0x80, treat as direct allophone bypass or ignore
        }
    }
}

void Pic7040::simple_tts(uint8_t ascii) {
    // Ultra-basic Letter-to-Sound mapping for demonstration
    // A real NRL ruleset is a massive state machine.
    // This allows `PRINT "HELLO"` to produce understandable output.
    uint8_t a = PA1; // default pause
    switch (ascii | 0x20) { // convert to lowercase
        case 'a': a = EY; break;
        case 'b': a = BB1; break;
        case 'c': a = SS; break;
        case 'd': a = DD1; break;
        case 'e': a = IY; break;
        case 'f': a = FF; break;
        case 'g': a = GG1; break;
        case 'h': a = HH1; break;
        case 'i': a = AY; break;
        case 'j': a = JH; break;
        case 'k': a = KK1; break;
        case 'l': a = LL; break;
        case 'm': a = MM; break;
        case 'n': a = NN1; break;
        case 'o': a = OW; break;
        case 'p': a = PP; break;
        case 'q': a = KK2; break;
        case 'r': a = RR1; break;
        case 's': a = SS; break;
        case 't': a = TT1; break;
        case 'u': a = UW1; break;
        case 'v': a = VV; break;
        case 'w': a = WW; break;
        case 'x': a = SS; break; // ks
        case 'y': a = YY1; break;
        case 'z': a = ZZ; break;
        case ' ': a = PA3; break;
        default: return; // ignore punctuation
    }
    
    // Queue the allophone
    allo_queue[allo_head++] = a;
    if (allo_head >= 1024) allo_head = 0;
}

void Pic7040::generate_audio(int16_t *left, int16_t *right) {
    // Mix the AY-3-8913 and SPO256-AL2 together
    int16_t ay_sample = ay.get_sample();
    int16_t spo_sample = spo.get_sample();
    
    // Simple additive mixing. Can tweak balance later.
    int32_t mix = ay_sample + spo_sample;
    
    // Hard clipping
    if (mix > 32767) mix = 32767;
    if (mix < -32768) mix = -32768;
    
    *left = (int16_t)mix;
    *right = (int16_t)mix;
}
