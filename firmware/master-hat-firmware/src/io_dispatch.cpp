#include "io_dispatch.h"
#include "pic7040.h"
#include "acia6551.h"
#include "v9958.h"
#include "cocosdc.h"
#include "boot_menu.h"
#include "rtc.h"

// External emulator instances (defined in main.cpp)
extern Pic7040   pic7040;
extern Acia6551  acia;
extern Acia6551  wimodem;
extern V9958     v9958;
extern Cocosdc   cocosdc;
extern BootMenu  boot_menu;
extern Rtc       rtc;
extern EmulatorMode current_mode;

// ================================================================
// The dispatch tables — live in RAM for fast access.
// ================================================================
IoReadFunc  io_read_table[256];
IoWriteFunc io_write_table[256];
RomReadFunc rom_read_handler = nullptr;

// ================================================================
// Individual I/O handler functions (thin wrappers).
// Each returns a single byte; the address is passed for registers
// that occupy a range (e.g. ACIA at $FF68-$FF6B).
// ================================================================

// --- Always-active handlers (present in every mode) ---

static uint8_t read_mode_register(uint16_t) {
    return (uint8_t)current_mode;
}

static uint8_t read_rtc(uint16_t addr) {
    return rtc.read(addr);
}

static void write_rtc(uint16_t addr, uint8_t data) {
    rtc.write(addr, data);
}

// --- Speech/Sound handlers ---

static uint8_t read_pic7040_status(uint16_t) {
    return pic7040.read_status();
}

static void write_pic7040_control(uint16_t, uint8_t data) {
    if (data & 0x01) pic7040.reset();
}

static void write_pic7040_data(uint16_t, uint8_t data) {
    pic7040.write_data(data);
}

// --- ACIA handlers (RS-232 and WiModem share the same register layout) ---

static uint8_t read_acia_data(uint16_t)   { return acia.read_data(); }
static uint8_t read_acia_status(uint16_t) { return acia.read_status(); }
static void write_acia_data(uint16_t, uint8_t d)    { acia.write_data(d); }
static void write_acia_command(uint16_t, uint8_t d)  { acia.write_command(d); }
static void write_acia_control(uint16_t, uint8_t d)  { acia.write_control(d); }

static uint8_t read_wimodem_data(uint16_t)   { return wimodem.read_data(); }
static uint8_t read_wimodem_status(uint16_t) { return wimodem.read_status(); }
static void write_wimodem_data(uint16_t, uint8_t d)    { wimodem.write_data(d); }
static void write_wimodem_command(uint16_t, uint8_t d)  { wimodem.write_command(d); }
static void write_wimodem_control(uint16_t, uint8_t d)  { wimodem.write_control(d); }

// --- V9958 (WordPak 2+) handlers ---

static uint8_t read_v9958(uint16_t addr) { return v9958.read_port(addr); }
static void write_v9958(uint16_t addr, uint8_t data) { v9958.write_port(addr, data); }

// --- CoCoSDC handlers ---

static uint8_t read_cocosdc_reg(uint16_t addr) { return cocosdc.read_register(addr); }
static void write_cocosdc_reg(uint16_t addr, uint8_t data) { cocosdc.write_register(addr, data); }

// --- Orch-90 write handlers (reads are open-bus, no handler needed) ---

extern uint8_t dac_left, dac_right;
extern void update_orch90_audio();

static void write_orch90_left(uint16_t, uint8_t data) {
    dac_left = data;
    update_orch90_audio();
}

static void write_orch90_right(uint16_t, uint8_t data) {
    dac_right = data;
    update_orch90_audio();
}

// --- ROM shadow readers ---

static uint8_t read_boot_rom(uint16_t addr) { return boot_menu.read_rom(addr); }
static uint8_t read_sdc_rom(uint16_t addr)  { return cocosdc.read_rom(addr); }

// ================================================================
// rebuild_io_tables(): Wipe and repopulate for the active mode.
// Called once at boot and on every mode switch.
// ================================================================
void rebuild_io_tables(EmulatorMode mode) {
    // 1. Clear everything to NULL (open bus / no handler)
    memset(io_read_table,  0, sizeof(io_read_table));
    memset(io_write_table, 0, sizeof(io_write_table));
    rom_read_handler = nullptr;

    // 2. Always-active registers (present in ALL modes)
    io_read_table[0x7F]  = read_mode_register;   // $FF7F read
    io_read_table[0x50]  = read_rtc;              // $FF50 read
    io_write_table[0x51] = write_rtc;             // $FF51 write
    io_write_table[0x75] = write_rtc;             // $FF75 write
    // Note: $FF7F write is handled specially in loop1() (mode switch logic)
    // Note: $FF70-$FF73 writes are handled specially in loop1() (boot menu config)

    // 3. Mode-specific handlers
    switch (mode) {
        case MODE_BOOT_MENU:
            rom_read_handler = read_boot_rom;
            break;

        case MODE_ORCH90:
            io_write_table[0x7A] = write_orch90_left;   // $FF7A
            io_write_table[0x7B] = write_orch90_right;  // $FF7B
            break;

        case MODE_SPEECH_SOUND:
            io_read_table[0x7E]  = read_pic7040_status; // $FF7E read
            io_write_table[0x7D] = write_pic7040_control;// $FF7D write
            io_write_table[0x7E] = write_pic7040_data;   // $FF7E write
            break;

        case MODE_RS232_PAK_LEGACY:
        case MODE_RS232_PAK_TURBO:
            io_read_table[0x68]  = read_acia_data;      // $FF68 read
            io_read_table[0x69]  = read_acia_status;    // $FF69 read
            io_write_table[0x68] = write_acia_data;     // $FF68 write
            io_write_table[0x6A] = write_acia_command;  // $FF6A write
            io_write_table[0x6B] = write_acia_control;  // $FF6B write
            break;

        case MODE_WIMODEM:
            io_read_table[0x68]  = read_wimodem_data;   // $FF68 read
            io_read_table[0x69]  = read_wimodem_status; // $FF69 read
            io_write_table[0x68] = write_wimodem_data;  // $FF68 write
            io_write_table[0x6A] = write_wimodem_command;// $FF6A write
            io_write_table[0x6B] = write_wimodem_control;// $FF6B write
            break;

        case MODE_WORDPAK2:
            io_read_table[0x78]  = read_v9958;          // $FF78 read
            io_read_table[0x79]  = read_v9958;          // $FF79 read
            io_write_table[0x78] = write_v9958;          // $FF78 write
            io_write_table[0x79] = write_v9958;          // $FF79 write
            io_write_table[0x7A] = write_v9958;          // $FF7A write
            io_write_table[0x7B] = write_v9958;          // $FF7B write
            break;

        case MODE_COCOSDC:
            rom_read_handler = read_sdc_rom;
            io_read_table[0x40]  = read_cocosdc_reg;    // $FF40 read
            io_read_table[0x48]  = read_cocosdc_reg;    // $FF48 read
            io_read_table[0x49]  = read_cocosdc_reg;    // $FF49 read
            io_read_table[0x4A]  = read_cocosdc_reg;    // $FF4A read
            io_read_table[0x4B]  = read_cocosdc_reg;    // $FF4B read
            io_write_table[0x40] = write_cocosdc_reg;   // $FF40 write
            io_write_table[0x48] = write_cocosdc_reg;   // $FF48 write
            io_write_table[0x49] = write_cocosdc_reg;   // $FF49 write
            io_write_table[0x4A] = write_cocosdc_reg;   // $FF4A write
            io_write_table[0x4B] = write_cocosdc_reg;   // $FF4B write
            break;
    }
}
