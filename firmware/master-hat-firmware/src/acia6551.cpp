#include "acia6551.h"

Acia6551::Acia6551(Stream* serial) : _serial(serial) {
    _turbo_mode = false;
    _command_reg = 0x00;
    _control_reg = 0x00;
}

void Acia6551::init(bool turbo_mode) {
    _turbo_mode = turbo_mode;
    _command_reg = 0x02; // Default state
    _control_reg = 0x00;
    update_baud_rate();
}

void Acia6551::set_turbo_mode(bool turbo_mode) {
    _turbo_mode = turbo_mode;
    update_baud_rate();
}

void Acia6551::update_baud_rate() {
    uint32_t baud = 115200;
    if (!_turbo_mode) {
        baud = get_baud_rate_from_control();
    }
    
    if (_baud_callback && baud > 0) {
        _baud_callback(baud);
    }
}

uint8_t Acia6551::read_status() {
    uint8_t status = 0;

    // Bit 0: Parity Error (0)
    // Bit 1: Framing Error (0)
    // Bit 2: Overrun (0)
    // Bit 3: Receiver Data Register Full (1 if available)
    if (_serial->available()) {
        status |= 0x08;
    }

    // Bit 4: Transmitter Data Register Empty (1 if we can write)
    if (_serial->availableForWrite() > 0) {
        status |= 0x10;
    }

    // Bit 5: Data Carrier Detect (DCD) - 0 means active/detected
    // Bit 6: Data Set Ready (DSR) - 0 means ready
    // Note: DCD and DSR are active LOW. Since we have no physical pins, we hardcode to 0.
    
    // Bit 7: Interrupt (0)

    return status;
}

uint8_t Acia6551::read_data() {
    if (_serial->available()) {
        return _serial->read();
    }
    return 0; // Or last read value, but 0 is safe
}

void Acia6551::write_command(uint8_t val) {
    _command_reg = val;
    // Command register controls RTS, DTR, parity, echo, interrupts.
    // For HLE 3-wire, we mainly ignore physical flow control.
}

void Acia6551::write_control(uint8_t val) {
    // If the control register changes, we might need to update the baud rate
    bool changed = (_control_reg != val);
    _control_reg = val;
    
    if (changed) {
        update_baud_rate();
    }
}

void Acia6551::write_data(uint8_t val) {
    _serial->write(val);
}

uint32_t Acia6551::get_baud_rate_from_control() {
    uint8_t baud_sel = _control_reg & 0x0F;
    switch (baud_sel) {
        case 1: return 50;
        case 2: return 75;
        case 3: return 110;
        case 4: return 135;
        case 5: return 150;
        case 6: return 300;
        case 7: return 600;
        case 8: return 1200;
        case 9: return 1800;
        case 10: return 2400;
        case 11: return 3600;
        case 12: return 4800;
        case 13: return 7200;
        case 14: return 9600;
        case 15: return 19200;
        default: return 0; // 0 = 16x external clock, ignore or default
    }
}
