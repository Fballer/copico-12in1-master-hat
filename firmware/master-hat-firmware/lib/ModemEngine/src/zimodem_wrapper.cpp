#include <Arduino.h>

// This wrapper aggregates all the separate Zimodem Arduino .ino files into a single compilation unit.
// This matches the behavior of the Arduino IDE preprocessor.

// ---- Forward Declarations ( normally generated automatically by Arduino IDE ) ----
static void flushSerial();
static void changeBaudRate(int baudRate);
static void changeSerialConfig(uint32_t conf);
static void initSDShell();
static void rawLogPrintf(const char*, ...);
static void rawLogPrintln(const char*);
static void rawLogPrint(const char*);
static inline uint8_t lc(uint8_t c) { return (c >= 'A' && c <= 'Z') ? (c + 32) : c; }
// --------------------------------------------------------------------------------

#include "zimodem.ino"
#include "connSettings.ino"
#include "filelog.ino"
#include "pet2asc.ino"
#include "phonebook.ino"
#include "proto_comet64.ino"
#include "proto_ftp.ino"
#include "proto_hostcm.ino"
#include "proto_http.ino"
#include "proto_kermit.ino"
#include "proto_ping.ino"
#include "proto_punter.ino"
#include "proto_xmodem.ino"
#include "proto_zmodem.ino"
#include "rt_clock.ino"
#include "serout.ino"
#include "wificlientnode.ino"
#include "wifiservernode.ino"
#include "wifisshclient.ino"
#include "zbrowser.ino"
#include "zcomet64mode.ino"
#include "zcommand.ino"
#include "zconfigmode.ino"
#include "zhostcmmode.ino"
#include "zircmode.ino"
#include "zpppmode.ino"
#include "zprint.ino"
#include "zslipmode.ino"
#include "zstream.ino"
