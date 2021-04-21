//
//	06/27/2020	These defines were moved from EBTKS.cpp to here.
//
//  07/xx/2020  Continuous changes to support changes elsewhere
//

//
//  Enables for code modules             THESE ARE NOT YET IN USE                 <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//
//
//      Enable if we are maintaining a copy of write to the CRT Character memory
#define ENABLE_CRT_CHAR_COPY                  (0)
//
//      Enable if we are maintaining a copy of write to the CRT Graphics memory
#define ENABLE_CRT_GRAPH_COPY                 (0)
//
//      Enable if we are providing Solid State Tape Drive (SSTD) replacement
#define ENABLE_SSTD                           (0)
//
//      Enable if we are providing at least 1 bank switched ROM
#define ENABLE_PROVIDE_BANK_SWITCHED_ROMS     (1)
//
//      Enable if we are providing the startup configuration menu
#define ENABLE_STARTUP_CONFIG_MENU            (0)
//
//      Not yet supported. Changes many things, including:
//          Max width of addresses (pervasive changes required)
//          ROM ID second byte is 2's complement  (changes probably localized to CONFIG.TXT and EBTKS_SD that loads the ROMs and checks IDs)
//          Memory layout is very different (so things that depend on that will fail, i.e. tripple-shift, but much more)
#define THIS_IS_AN_HP_86_OR_87      (0)

//
//
//
//  Enables for code modules             THESE ARE IN USE                         <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

//
//  Logging control is one of 3 levels:     LOG_NONE      for no logging
//                                          LOG_SERIAL    for Serial.printf
//                                          LOG_FILE      for LOG File
//

//
//  WARNING, WARNING, WARNING, WARNING, WARNING, WARNING, WARNING, WARNING, WARNING
//
//  The settings below (LOG_FILE, LOG_NONE, LOG_NONE, LOG_NONE) for the four LOGLEVEL_...
//  are the ones used during most of the debugging phase leading up to the Beta release
//  in Feb 2021. Changing the LOG_NONE entries to LOG_FILE should not cause any unexpected
//  behavior. Changing any of these to LOG_SERIAL may not give intended results because
//  after all this logging code was written, the serial output that wasn't part of the
//  logging code, got changed to the code that uses log_to_serial_ptr. This is the
//  the code that collects the messages and depending on CONFIG.TXT settings, may send
//  the messages to the serial port both during initial startup and again (maybe) later.
//

#define LOG_NONE                  (0)
#define LOG_SERIAL                (1)
#define LOG_FILE                  (2)

#define LOGLEVEL_GEN              LOG_FILE        //  For General Logging
#define LOGLEVEL_AUX              LOG_NONE        //  For AUXROM Logging
#define LOGLEVEL_1MB5             LOG_NONE        //  For 1MB5 activity
#define LOGLEVEL_TAPE             LOG_NONE        //  For 1MB5 activity

//
//      Enable if we decide to use LISP as a scripting language
#define ENABLE_LISP               (0)

//
//    HP85 banked switched ROM emulation
//

#define ROM_PAGE                          (0x6000)
#define ROM_PAGE_SIZE                     (0x2000)      //  ROM Page size in bytes
#define MAX_ROMS                          (18)          //  The 86/87 have a max of 15 option ROMs, EBTKS AUXROM takes 1
                                                        //  (the other 3 are phantom and don't use up entries in the
                                                        //  system tables), but still need to have space allocated in the
                                                        //  ROM storage area. It is a max of 14 for HP85A/B, so this will
                                                        //  be sufficient.
                                                        //  So 4 ROM slots for AUXROMs, leaving 14 for user selected ROMs
#define MAX_ROM_NAME_LENGTH               (32)

//
//    Special handling of AUXROMS
//
#define AUXROM_PRIMARY_ID                 (0361)
#define AUXROM_SECONDARY_ID_START         (0362)
#define AUXROM_SECONDARY_ID_END           (0375)
#define AUXROM_RAM_WINDOW_START           (010000)      //  Relative to base address of bank switched ROMs. HP85 sees this as 070000
#define AUXROM_RAM_WINDOW_SIZE            (006000)      //  3K bytes
#define AUXROM_RAM_WINDOW_LAST            (AUXROM_RAM_WINDOW_START + AUXROM_RAM_WINDOW_SIZE - 1)

//
//    16K of extra RAM for HP85-A
//

#define HP85A_16K_RAM_module_base_addr    (0140000)
#define IO_ADDR                           (0177400)       //  Top 256 bytes of the address space
#define EXP_RAM_SIZE                      (16384 - 256)   //  16128 bytes of RAM

//
//    Extended Memory Controller
//
#define ENABLE_EMC_SUPPORT                (1)
#define EMC_MAX_BANKS                     (8)             //  Pre allocate 256kB for EMC Memory
#define EMC_RAM_SIZE                      (EMC_MAX_BANKS * 32768)
//
//    Tracking the CRT activity. Can be used to dump to a remote file or printer, and
//    also needed if we want to scribble on the screen
//

#define DUMP_HEIGHT (16)

//
//    Support for DMA transfers.
//    While this hardware could do continuous DMA cycles, this would impact the
//    1MA2 DRAM controller refresh logic. It does a refresh cycle (/RAS only) every
//    9 bus cycles, if there is no system acces to the RAM. If there is a collision,
//    the refresh is postponed for some future available bus cycle. At most two
//    refresh cycles can be postponed. No error is reported if this is exceeded, but
//    it could impact memory reliability. Thus we limit the maximum DMA consecutive
//    cycles, and allow some idle time for the postponed refresh cycles to occur
//    The effective transfer rate approaches (burst_length)/(burst_length + break_cycles)
//    times 1/1.6 us . i.e. 15/(15+3) gives 520 K Bytes per second, approximately
//

#define MAX_DMA_TRANSFER_LENGTH           (256)
#define MAX_DMA_BURST_LENGTH              (15)
#define DMA_BURST_BREAK_CYCLES            (3)

#define SERIAL_STRING_MAX_LENGTH          (81)
#define SERIAL_COMMAND_MAX_LENGTH         (81)

//
//  Logic Analyzer Support
//
//  LOGIC_ANALYZER_BUFFER_SIZE must be a power of two for modulo indexing
//  and index masks to work
//

#define LOGIC_ANALYZER_BUFFER_SIZE        (1024)
#define LOGIC_ANALYZER_INDEX_MASK         (0x000003FFU)
// #define LOGIC_ANALYZER_BUFFER_SIZE     (8192)
// #define LOGIC_ANALYZER_INDEX_MASK      (0x00001FFFU)

#define DIRECTORY_LISTING_BUFFER_SIZE     (65536)
#define CRT_LOG_BUFFER_SIZE               ( 1024)
#define SERIAL_LOG_BUFFER_SIZE            ( 2048)
