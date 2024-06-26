#include <Arduino.h>
#include <setjmp.h>
#include "SdFat.h"

//  The next line must exist in just one compilation unit, which is this file.
//  All other usage of the globals file must not have this line, leaving ALLOCATE undefined
#define ALLOCATE  1

#include "Inc_Common_Headers.h"
#include "EBTKS_ESP.h"
#include <TimeLib.h>
//  #include "core_pins.h"  uncomment to quick access file with editor. Do not leave uncommented

ESPComms esp32;

// #include "USBHost_t36.h"                 //  Commenting out saves about 9KB of ITCM for code and 700 bytes for data
// USBHost myusb;                           //  while we are not using this yet.
// USBHub hub1(myusb);
// USBHub hub2(myusb);
// USBHub hub3(myusb);
// KeyboardController keyboard1(myusb);
// KeyboardController keyboard2(myusb);

//
//  These includes are only needed in this file
//

//  #include "ROMs.h"  //  now comes from SD card

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//      EBTKS Firmware Version 0.1
//
//      06/25/2020      Derived from Nirvana V1.0 Firmware running on Teensy 4.0
//
//      Authors:        Russell Bull
//                      Philip Freidin
//
//      06/25/2020      Migrate all code from prototype of this project, code name Nirvana. New project name is EBTKS
//                      1)      Scan through all comments to check for references to Teensy V4.0 Most will not change for Teensy V4.1
//                      2)      Redo all the documentation for the Pin Mapping
//                      3)      Update all references to Pin Mapping
//
//      06/26/2020      1) Integrate Russell's first version of tape drive emulation.
//                      Functionality is Read Only , with a minimal pre-defined tape that has 3 files, including two small BASIC programs.
//                      The HP-85 System boot test for Tape drive passes, thus eliminating the "Error 23" when power is turned and there is no tape drive
//                      The CAT command shows the directory correctly
//                      The two little BASIC programs can be LOADed and RUN successfully
//                      2) Start the process of splitting the project into multiple files. (was one monolithic file of 2760 lines + 4132 in ROMS.h)
//
//      06/29/2020      Work on startup code and startup timing. Make measurements
//
//
////////////////////////////////////////////////////////////////        Changes to core usb.c           Use a less heavy handed approach to disabling interrupts
//
//     See L:\BUILD_A_SYS_Scripts-Bug_Hunts-Workarounds-Other_Notes\Teensy_4.0_Notes.txt for details
//
////////////////////////////////////////////////////////////////        Replace Systick interrupt handler:  systick_isr()               gets rid of all the "runFromTimer" stuff that
//                                                                                                                                      was significant code bing run in an interrupt handler.
//
//      The standard systick_isr() lives in:
//              C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\cores\teensy4\EventResponder.cpp
//
//      We replace it with our own by adding the folowing to our setup()   (see bottom of this file)
//
//              _VectorsRam[15] = my_systick_function;          // no need to declare _VectorsRam , it is declared as a Global in
//                                                              // C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\cores\teensy4\startup.c         line 21
//
//      And then provide our minimal systick ISR
//
//              extern "C" volatile uint32_t systick_millis_count;
//              extern "C" volatile uint32_t systick_cycle_count;
//
//              void mySystick_isr(void)
//              {
//                  systick_cycle_count = ARM_DWT_CYCCNT;
//                  systick_millis_count++;
//              }
//
////////////////////////////////////////////////////////////////        yield()         Nothing at the moment, but:
//
//      https://forum.pjrc.com/threads/60831-Teensy4-0-and-_disable_irq-in-the-core-code                see message 17
//
//      Maybe not relevant due to replacement of Systick_isr() by mySystick_isr() by changing interrupt vector
//
////////////////////////////////////////////////////////////////
//
//      Logic Analyzer Configurations
//
//  
//  Several configurations have been deleted from this document as they
//  are no longer relevant. The production V3.0 EBTKS has built in logic
//  analyzer ports for 3 pre-wired 8 channel pods, and 1 uncommited 8
//  channel pod. The physical layout for each pod has 8 ground pins
//  towards the back edge of the PCB, and 8 active pins on the side closer
//  to Teensy 4.1 . i.e. pins 1,3,5,7,9,11,13,15 are grounds. Designed for
//  Tektronix P6418 logic analyzer pods, used with a TLA5201 or similar.
//  
//  
//      Channel         Signal
//
//      C3-7            B7X
//      C3-6            B6X
//      C3-5            B5X
//      C3-4            B4X
//      C3-3            B3X
//      C3-2            B2X
//      C3-1            B1X
//      C3-0            B0X
//
//      C2-7            IFETCH
//      C2-6            /LMA
//      C2-5            /RD
//      C2-4            /WR
//      C2-3            RC
//      C2-2            BUFEN
//      C2-1            SCOPE_1
//      C2-0            SCOPE_2
//
//      A3-7            IRLX
//      A3-6            CTRLEN
//      A3-5            CTRLDIR
//      A3-4            HALTX
//      A3-3            INTPRI
//      A3-2            IPRIH
//      A3-1            PWO
//      A3-0            Phase_1
//
//      A2-7
//      A2-6
//      A2-5
//      A2-4
//      A2-3
//      A2-2
//      A2-1
//      A2-0
//
//
////////////////////////////////////////////////////////////////        Assumptions, Expectation, Requirements.
//
//      This code involves very carefully crafted real-time sequences
//      and interrupt handlers.
//
//      The Interrupt handlers must be very fast, and lock out other
//      activity for the minimal amount of time. At the time of writing,
//      the ONLY interrupt handlers are this file's routines that handle
//      all HP-85 bus transaction, and the interrupts that occur in
//      the TeensyDuino USB handler (for serial over USB).
//      The highest priority is the Temperature Monitor (IRQ 63 and 64)
//         (don't know if there is a handler)
//      The next highest is the HP-85 bus transaction related interrupts
//      and then everything else.
//      There is code in the TeensyDuino library that uses the global
//      __disable_irq() and __enable_irq() . This is unacceptable.
//      The USB is an example (at the time of this writing) with this
//      issue. So it needs to be edited as described in
//              Teensy_4.0_Notes.txt
//      So no long 'if chains' allowed in the bus transaction code.
//      Where necessary, use tables to jump directly to required
//      code, even if very inefficient use of memory. If this leads to
//      tasks that will take more than 100ns to execute, then the
//      right approach is to set a flag, and have the task performed
//      in the background. That code should do round-robin testing of
//      all possible such flags
//
//      All of the code here, which depends on Pin Change interrupts
//         (Specifcally Phi 1 rising and Phi 2 Rising)
//      assume that the ONLY pin interrupts are these 2. There is code
//      that makes changes to mask registers, and NVIC registers based
//      this assumption. Changing the code to support more than these
//      two interrupts, would probably require a wholesale rewrite, so
//      a redesign would be required.
//
//      More needs to be written here!
//
//
////////////////////////////////////////////////////////////////

/*    Signal Name <-> Pin Bit Number <-> Bit Position <-> GPIO Group <-> IOMUX Control

The #defines for these are in core_pins.h at line 516 (for teensy 4.1)

The Signal Names come from the schematic for EKBTS V2.0

Core Pin Numbers physical position on Teensy 4.1

                  GND  USB    Vin     Vin
     BUFEN        0           GND     GND
  CTRL_DIR        1           3.3V    3.3V
NEOPIX_TSY        2           23      RD
    INTPRI        3           22      LMA
       T04        4           21      PHASE1
       T05        5           20      WR
       T06        6           19      DB0
       TXD        7           18      DB1
       RXD        8           17      DB6
       T09        9           16      DB7
       T10        10          15      DB3
       T11        11          14      DB2
       T12        12          13      T13  (LED)
      3.3V        3.3V        GND     GND
       SCL        24          41      DB5
       SDA        25          40      DB4
       T26        26          39      SCOPE_2
    DIR_RC        27          38      PHASE2
     HALTX        28          37      INTERRUPT
    CTRLEN        29          36      PWO_O
  IPRIH_IN        30          35      PHASE21
      PWO_L       31   SD     34      PHASE12
     IFETCH       32  Card    33      SCOPE_1


EBTKS           Core               Bit      GPIO        IOMUX
Signal          Pin                         group
Name            Number

BUFEN           CORE_PIN0_BIT       3       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_03
CTRL_DIR        CORE_PIN1_BIT       2       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_02
NEOPIX_TSY      CORE_PIN2_BIT       4       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_04
INTPRI          CORE_PIN3_BIT       5       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_05
T04             CORE_PIN4_BIT       6       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_06
T05             CORE_PIN5_BIT       8       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_08
T06             CORE_PIN6_BIT      10       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_10
TXD             CORE_PIN7_BIT      17       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01
RXD             CORE_PIN8_BIT      16       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00
T09             CORE_PIN9_BIT      11       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_11
T10             CORE_PIN10_BIT      0       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00
T11             CORE_PIN11_BIT      2       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02
T12             CORE_PIN12_BIT      1       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01
T13             CORE_PIN13_BIT      3       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03
DB2             CORE_PIN14_BIT     18       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_02
DB3             CORE_PIN15_BIT     19       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_03
DB7             CORE_PIN16_BIT     23       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_07
DB6             CORE_PIN17_BIT     22       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_06
DB1             CORE_PIN18_BIT     17       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_01
DB0             CORE_PIN19_BIT     16       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_00
WR              CORE_PIN20_BIT     26       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_10
PHASE1          CORE_PIN21_BIT     27       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_11
LMA             CORE_PIN22_BIT     24       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_08
RD              CORE_PIN23_BIT     25       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09
SCL             CORE_PIN24_BIT     12       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_12
SDA             CORE_PIN25_BIT     13       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_13
T26             CORE_PIN26_BIT     30       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_14
DIR_RC          CORE_PIN27_BIT     31       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_15             NOTE: This DIR/RC on the schematic
HALTX           CORE_PIN28_BIT     18       GPIO8       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_32
CTRLEN          CORE_PIN29_BIT     31       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_31
IPRIH_IN        CORE_PIN30_BIT     23       GPIO8       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_37
PWO_L           CORE_PIN31_BIT     22       GPIO8       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_36
IFETCH          CORE_PIN32_BIT     12       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_12
SCOPE_1         CORE_PIN33_BIT      7       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_07
PHASE12         CORE_PIN34_BIT     29       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_13
PHASE21         CORE_PIN35_BIT     28       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_12
PWO_O           CORE_PIN36_BIT     18       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_02
INTERRUPT       CORE_PIN37_BIT     19       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_03
PHASE2          CORE_PIN38_BIT     28       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_12
SCOPE_2         CORE_PIN39_BIT     29       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_13
DB4             CORE_PIN40_BIT     20       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_04
DB5             CORE_PIN41_BIT     21       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_05

This table sorts the Core Pin Number by GPIO group, and bit position within the 32 bit GPIO port
Blank lines separate a change in GPIO port and also  where bits are not contiguous

GPIO    Core      Bit          Pre defined
Group   Pin       Position     GPIO Data
        Number    in DR Reg    Register pointer

GPIO6    1         2           CORE_PIN1_PORTREG
GPIO6    0         3           CORE_PIN0_PORTREG

GPIO6   24        12           CORE_PIN24_PORTREG
GPIO6   25        13           CORE_PIN25_PORTREG

GPIO6   19        16           CORE_PIN19_PORTREG
GPIO6   18        17           CORE_PIN18_PORTREG
GPIO6   14        18           CORE_PIN14_PORTREG
GPIO6   15        19           CORE_PIN15_PORTREG
GPIO6   40        20           CORE_PIN40_PORTREG
GPIO6   41        21           CORE_PIN41_PORTREG
GPIO6   17        22           CORE_PIN17_PORTREG
GPIO6   16        23           CORE_PIN16_PORTREG
GPIO6   22        24           CORE_PIN22_PORTREG
GPIO6   23        25           CORE_PIN23_PORTREG
GPIO6   20        26           CORE_PIN20_PORTREG
GPIO6   21        27           CORE_PIN21_PORTREG
GPIO6   38        28           CORE_PIN38_PORTREG
GPIO6   39        29           CORE_PIN39_PORTREG
GPIO6   26        30           CORE_PIN26_PORTREG
GPIO6   27        31           CORE_PIN27_PORTREG

GPIO7   10         0           CORE_PIN10_PORTREG
GPIO7   12         1           CORE_PIN12_PORTREG
GPIO7   11         2           CORE_PIN11_PORTREG
GPIO7   13         3           CORE_PIN13_PORTREG

GPIO7    6        10           CORE_PIN6_PORTREG
GPIO7    9        11           CORE_PIN9_PORTREG
GPIO7   32        12           CORE_PIN32_PORTREG

GPIO7    8        16           CORE_PIN8_PORTREG
GPIO7    7        17           CORE_PIN7_PORTREG
GPIO7   36        18           CORE_PIN36_PORTREG
GPIO7   37        19           CORE_PIN37_PORTREG

GPIO7   35        28           CORE_PIN35_PORTREG
GPIO7   34        29           CORE_PIN34_PORTREG

GPIO8   28        18           CORE_PIN28_PORTREG

GPIO8   31        22           CORE_PIN31_PORTREG
GPIO8   30        23           CORE_PIN30_PORTREG

GPIO9    2         4           CORE_PIN2_PORTREG
GPIO9    3         5           CORE_PIN3_PORTREG
GPIO9    4         6           CORE_PIN4_PORTREG
GPIO9    5         8           CORE_PIN5_PORTREG

GPIO9   29        31           CORE_PIN29_PORTREG
GPIO9   33         7           CORE_PIN33_PORTREG

*/


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                Main Uses of DMAMEM
//
//    256 KB      EMC for the HP85B, HP86/87                EMC_MAX_BANKS * 32kB EBTKS_Bus_Interface_ISR.cpp
//
//    144 KB      roms[][]                                  MAX_ROMS            EBTKS_Config.h (assumes MAX_ROMS is 18)
//                                                          ROM_PAGE_SIZE       EBTKS_Config.h
//      0.5 KB    Copy Buffer for MOUNT/copy_sd_file()      COPY_BUFFER_SIZE    EBTKS_AUXROM_SD_Services.cpp
//      1 KB      sprintf_result                            SCRATCHLENGTH       EBTKS_AUXROM_SD_Services.cpp
//      1 KB      string_arg                                SCRATCHLENGTH       EBTKS_AUXROM_SD_Services.cpp
//      0.1 KB    format_segment                                                EBTKS_AUXROM_SD_Services.cpp
//
//    436,324 B   Total.  Actual total from linker on 12/15/2020 is 473,312
//    412,160 B   Total.  Actual total from linker on  3/28/2021 is 424,672
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//  Messages for startup indicating LOGLEVEL

#if LOGLEVEL_GEN == LOG_NONE
#define LOGLEVEL_GEN_MESSAGE      "LOGLEVEL_GEN is  None\n"
#elif LOGLEVEL_GEN == LOG_FILE
#define LOGLEVEL_GEN_MESSAGE      "LOGLEVEL_GEN is  File on SD card\n"
#elif LOGLEVEL_GEN == LOG_SERIAL
#define LOGLEVEL_GEN_MESSAGE      "LOGLEVEL_GEN is  Serial via USB\n"
#else
#define LOGLEVEL_GEN_MESSAGE      "LOGLEVEL_GEN is  unknown\n"
#endif

#if LOGLEVEL_AUX == LOG_NONE
#define LOGLEVEL_AUX_MESSAGE      "LOGLEVEL_AUX is  None\n"
#elif LOGLEVEL_AUX == LOG_FILE
#define LOGLEVEL_AUX_MESSAGE      "LOGLEVEL_AUX is  File on SD card\n"
#elif LOGLEVEL_AUX == LOG_SERIAL
#define LOGLEVEL_AUX_MESSAGE      "LOGLEVEL_AUX is  Serial via USB\n"
#else
#define LOGLEVEL_AUX_MESSAGE      "LOGLEVEL_AUX is  unknown\n"
#endif

#if LOGLEVEL_1MB5 == LOG_NONE
#define LOGLEVEL_1MB5_MESSAGE      "LOGLEVEL_1MB5 is None\n"
#elif LOGLEVEL_1MB5 == LOG_FILE
#define LOGLEVEL_1MB5_MESSAGE      "LOGLEVEL_1MB5 is File on SD card\n"
#elif LOGLEVEL_1MB5 == LOG_SERIAL
#define LOGLEVEL_1MB5_MESSAGE      "LOGLEVEL_1MB5 is Serial via USB\n"
#else
#define LOGLEVEL_1MB5_MESSAGE      "LOGLEVEL_1MB5 is unknown\n"
#endif

#if LOGLEVEL_TAPE == LOG_NONE
#define LOGLEVEL_TAPE_MESSAGE      "LOGLEVEL_TAPE is None\n"
#elif LOGLEVEL_TAPE == LOG_FILE
#define LOGLEVEL_TAPE_MESSAGE      "LOGLEVEL_TAPE is File on SD card\n"
#elif LOGLEVEL_TAPE == LOG_SERIAL
#define LOGLEVEL_TAPE_MESSAGE      "LOGLEVEL_TAPE is Serial via USB\n"
#else
#define LOGLEVEL_TAPE_MESSAGE      "LOGLEVEL_TAPE is unknows\n"
#endif


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////    Handlers for I/O device support


//
//  Setup Tasks
//    Configure direction and state of all I/O pins
//    Replace the TeensyDuino SysTick handler with our own, with guaranteed minimum execution time
//    Deal with holding the HP-85 in reset if until this setup code is complete
//      Scenario 1: HP-85 is already running.
//                  We pull down on PWO for 500 ms, and reset it
//      Scenario 2: Teensy code here is running while PWO is still low on HP85
//                  We pull down on PWO for 500 ms anyway, then release our pulling it low
//                  and wait for whatever else is pulling it low to stop. i.e. wait for high
//

bool              is_hp85_idle(void);
void              Boot_Messages_to_CRT(void);

static uint32_t   one_second_timer = 0;
static bool       SD_begin_OK;
static bool       CRT_Boot_Message_Pending;
static bool       CRT_Boot_Message_Pending_to_Serial;
static bool       Serial_Log_Message_Pending;
static uint32_t   last_pin_isr_count;                         //  These are used to detect that the HP85 power is off, and Teensy should be reset
static uint32_t   count_of_pin_isr_count_not_changing;        //  or if we aren't in loop(), we should stall boot.
static bool       HP85_has_been_off;

////
////  Temporary dateTime callback function
////
////  Use compile time for now:  #define EBTKS_COMPILE_TIME_CONDENSED      date_time,&(date_time[4]),&(date_time[9]),&(date_time[14])
////
//void dateTime(uint16_t *date, uint16_t *time, uint8_t* ms10)
//{
//  uint16_t    Year , Month , Day;
//  uint16_t    Hour,  Minute, Second;
//
//  Year    = YEAR;
//  Month   = MONTH;
//  Day     = DAY;
//  Hour    = HOURS;
//  Minute  = MINUTES;
//  Second  = SECONDS;
//
////  *date = FS_DATE(2021,3,8);        //  Year , Month , Day
////  *time = FS_TIME(19,39,0);         //  hour, minute, second
//
//  *date = FS_DATE(Year , Month , Day);
//  *time = FS_TIME(Hour,  Minute, Second);
//
//  *ms10 = 0;
//
//  Serial.printf("\ndateTime callback occured. %02d/%02d/%04d  %02d:%02d:%02d\n", Month , Day, Year, Hour,  Minute, Second);
//  Serial.printf("__DATE__[%s] __TIME__[%s] RTC says the time is %12d\n",__DATE__ , __TIME__ ,  getTeensy3Time());
//  Serial.printf("__DATE__[4][%c] __DATE__[5][%c] \n",__DATE__[4], __DATE__[5]);
//
//}

//
//  Date and time callback routine for the SDFat File system
//  Lifted from ...  .pio\libdeps\teensy41\SdFat\examples\TeensyRtcTimestamp\TeensyRtcTimestamp.ino
//
//  Call back for file timestamps.  Only called for file create and sync().

void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) {

  // Return date using FS_DATE macro to format fields.
  *date = FS_DATE(year(), month(), day());

  // Return time using FS_TIME macro to format fields.
  *time = FS_TIME(hour(), minute(), second());

  // Return low time bits in units of 10 ms.
  *ms10 = second() & 1 ? 100 : 0;
}

//
//  There is something strange about LED initialization, maybe related to an undocumented power up delay in the LEDs.
//  If we initialize too early, the results are random bright LEDs, but not on all boards, just a few.
//  So we repeatedly set them to 0,0,0 until enough time has passed that (empirically) the LEDs will accept the data
//

bool LEDs_Initialized = false;

void Initial_LED_load(bool set_initial_values)
{

  if (set_initial_values)
  {
    setLedColor(0, 10,  0,  0);
    setLedColor(1,  0, 10,  0);
    WS2812_update();
    LEDs_Initialized = true;
  }
  else
  {
    setLedColor(0, 0,  0,  0);
    setLedColor(1,  0, 0,  0);
    WS2812_update();
  }
}

void setup()
{

//  int         setjmp_reason;
  bool        config_success;
  uint32_t    Phi_1_high_count;
  uint32_t    Phi_1_low_count;
  uint32_t    i;

//  setjmp_reason = setjmp(PWO_While_Running);
//  if (setjmp_reason == 99)                    //  This code is from the longjmp() in loop(). But it also has a processor reset
//  {                                           //  function, so maybe this never occurs.
//    //
//    //  We are here from a PWO while running. Someone has the covers off, and is forcing a restart.
//    //  While setjmp unwinds the stack etc, it doesn't reset any of the processor state. So do some cleanup here
//    //
//    NVIC_DISABLE_IRQ(IRQ_GPIO6789);   //  This seems the most important one. there are probably other things.
//                                      //  Add them as needed
//                                      //  - for example, the Service ROM test fails because it reads back FF, FF
//                                      //    so that means we aren't yet doing initializing correctly
//  }

//
//  All of the initial startup tasks are just configuring I/O pins, and none should cause a stall.
//
  pin_isr_count = 0;
  last_pin_isr_count = 0;
  count_of_pin_isr_count_not_changing = 0;
  CRT_Boot_Message_Pending = true;
  CRT_Boot_Message_Pending_to_Serial = true;
  Serial_Log_Message_Pending = true;

  //
  //  Diagnostic Pins
  //
  //	Name							Physical Pin		Suggested use
  //	CORE_PIN_SCOPE_1  25							ISR Entry and Exit. SCOPE_1
  //																		Also used by SCOPE_1_Pulser, which is used to track timing of the Boot sequence,
  //                                    and should only be called before Pin Change Interrupt is activated
  //	CORE_PIN_SCOPE_2	31							General entry and exit of whatever is currently being debugged, currently DMA, SCOPE_2
  //	CORE_PIN_T13			35							Also the on board LED. Toggle when errors are detected in diagnostic tests

  pinMode(CORE_PIN_ESP32_RXD, OUTPUT);      //  Communications with the ESP32
  pinMode(CORE_PIN_ESP32_TXD, INPUT);       //  Communications with the ESP32

  digitalWrite(CORE_PIN_ESP_EN,1);
  digitalWrite(CORE_PIN_ESP_BOOT,1);
  pinMode(CORE_PIN_ESP_EN,OUTPUT);
  pinMode(CORE_PIN_ESP_BOOT,OUTPUT);

  pinMode(CORE_PIN_T13, OUTPUT);
  //  SET_LED;                        //  T13 On Board LED
  CLEAR_LED;

  pinMode(CORE_PIN_SCOPE_1, OUTPUT);
  CLEAR_SCOPE_1;                      //  SCOPE_1 tracks entry and exit of the pin_change ISR, at the heart of the bus interface
                                      //  and also pulses during the boot process, to measure the time various tasks take,
                                      //  and to identify problems.
  pinMode(CORE_PIN_SCOPE_2, OUTPUT);
  CLEAR_SCOPE_2;                      //  SCOPE_2

  ASSERT_PWO_OUT;                     //  Desire glitch free switchover from resistor asserting PWO to Teensy asserting PWO
  pinMode(CORE_PIN_PWO_O, OUTPUT);    //  Core Pin 36 (Physical pin 28) is pulled high by an external resistor, which turns on the FET,
                                      //  which pulls the PWO line low, which resets the HP-85
  ASSERT_PWO_OUT;                     //  Over-ride external pull up resistor, still put High on gate, thus holdimg PWO on I/O bus Low

  //  SCOPE_1 starts low (see above), pulses 5 times, ends low. Pulses are 10 us apart, High duration is 5 us
  SCOPE_1_Pulser(5);
  EBTKS_delay_ns(10000);    //  10 us

  _VectorsRam[15] = mySystick_isr;    //  Replace the system SysTick handler with our own, that doesn't call yield()

  //
  //  Data Bus Transceiver
  //
  pinMode(CORE_PIN_DIR_RC, OUTPUT);   // Direction pin for U2, the 8 bit data bus level shifter
  pinMode(CORE_PIN_BUFEN,  OUTPUT);   // Output enable for the U2 level shifter.  !OE
  BUS_DIR_FROM_HP;      // Default state for the Data Bus transceiver is enabled and inbound
  ENABLE_BUS_BUFFER_U2; // Default state for the Data Bus transceiver is enabled and inbound

  //
  //  Three Control signals and two of the four Clock phases
  //
  //  Clocks are always inbound. The 3 control lines are inbound except during DMA when this
  //  software generates these three signals
  //

  pinMode(CORE_PIN_RD, INPUT);          //  /RD
  pinMode(CORE_PIN_WR, INPUT);          //  /WR
  pinMode(CORE_PIN_LMA, INPUT);         //  /LMA
  pinMode(CORE_PIN_PHASE1, INPUT);      //  Phi 1
  pinMode(CORE_PIN_PHASE2, INPUT);      //  Phi 2

  pinMode(CORE_PIN_T04, INPUT);         //  /IRLX     Used by the Logic Analyzer
  pinMode(CORE_PIN_T05, INPUT);         //  /HALTX    Used by the Logic Analyzer
  pinMode(CORE_PIN_IFETCH, INPUT);      //  IFETCH    Used by the Logic Analyzer

  pinMode(CORE_PIN_CTRL_DIR, OUTPUT);   //  bus control buffer direction /LMA,/WR,/RD
  CTRL_DIR_FROM_HP;

  pinMode(CORE_PIN_INTERRUPT, OUTPUT);  //  INT inverted by hardware
  RELEASE_INT;

  pinMode(CORE_PIN_IPRIH_IN, INPUT);    //PRIH input - interrupt priority chain input. low if someone above us is interrupting
  pinMode(CORE_PIN_INTPRI,OUTPUT);
  RELEASE_INTPRI;

  //  Why is this duplicate here ###### RB?
  //  pinMode(CORE_PIN_IPRIH_IN, INPUT);    //PRIH input - interrupt priority chain input. Low if someone above us is interrupting
  //  pinMode(CORE_PIN_INTPRI,OUTPUT);
  //  RELEASE_INTPRI;

  RELEASE_HALT;                         //  Make activating pin output glitch free
  pinMode(CORE_PIN_HALTX, OUTPUT);      //  HALT inverted by hardware
  RELEASE_HALT;

  pinMode(CORE_PIN_PWO_L, INPUT);      //PWO from HP85

  // data bus pins to input

  pinMode(CORE_PIN_DB0, INPUT);   //  DB0
  pinMode(CORE_PIN_DB1, INPUT);   //  DB1
  pinMode(CORE_PIN_DB2, INPUT);   //  DB2
  pinMode(CORE_PIN_DB3, INPUT);   //  DB3
  pinMode(CORE_PIN_DB4, INPUT);   //  DB4
  pinMode(CORE_PIN_DB5, INPUT);   //  DB5
  pinMode(CORE_PIN_DB6, INPUT);   //  DB6
  pinMode(CORE_PIN_DB7, INPUT);   //  DB7

  //
  //  Before we proceed with boot, make sure the HP85 is powered on and the clocks are running
  //  We do this by counting the number of times CORE_PIN_PHASE1 is high or low in a loop for 100 us
  //  Empirical measurement of this code shows values around 4733 for low count and 700 for high count
  //

  HP85_has_been_off = false;                //  We haven't tested it yet
  loop_count = 0;                           //  This normally used in loop() , but we need a counter, and loop() is not yet running, so ...

  //
  //  End of initial startup (configuring pins). At this point, EBTKS is asserting PWO, so if what follows stalls for any reason,
  //  then it will also block the Series80 computer from starting, and the user wil not have a clue why, except unplugging EBTKS
  //  should return the Series80 computer to its native startup process.
  //

  Serial.begin(115200);                     //  Doing this does not stall the program at this point if there is no
                                            //  attached virtual terminal, via USB. It is the "while (!Serial){};"
                                            //  that typically precedes it that causes the stall. But this might fix
                                            //  Teensy would otherwise not yet initialized the Serial channel, and
                                            //  this then blocks downloading new firmware. Just guessing at this point.

  while(1)
  {
    Phi_1_high_count = Phi_1_low_count = 0;
    loop_count++;
    for (i = 0 ; i < 5435 ; i++)            //  This constant adjusted with SCOPE_2 to run for 100 us
    {
      if (IS_PHI_1_HIGH)
      {
        Phi_1_high_count++;
      }
      else
      {
        Phi_1_low_count++;
      }
    }

    if ((Phi_1_high_count < 350) || (Phi_1_low_count < 2000))
    {   //  Teensy is powered, but clock not running, so probably teensy powered over USB and HP85 is off.
        //  Flash the TEENSY Orange LED on Core pin 13
      HP85_has_been_off =true;
      //
      //  Don't be too annoying, flash LED once approximately every 10 seconds
      //
      if (loop_count % 10 == 0)
      {
        Serial.printf("Teensy powered by USB cable, Series80 computer is off\n");
        //
        //  Little flash sequence on Teensy LED, when Teensy is powered by USB cable, but Series80 computer is off
        //
        for (i=0 ; i < 3 ; i++)
        {
          SET_LED;
          delay(3);                       //  Pulse LED for 3 ms, so not too many photons.
          CLEAR_LED;
          delay(50);
        }
      }
      delay(1000);                      //  Wait 1 seconds till we test again
    //
    }
    else
    {
      if (HP85_has_been_off)
      {
        //
        //  The HP85 has transitioned from off to on, so we need to force a reset of Teensy
        //
        SCB_AIRCR = 0x05FA0004;         //  Forum info indicates this does a processor reset, so we don't get to the next line.
      }
      //
      //  If we get here, it means that the HP85 was already powered either before or concurrently with Teensy, so no need for a forced reset
      //
      break;                            //  Looks like the HP85 clock is running, so we can proceed
    }
  }

  //
  //  LOGGING  LOGGING  LOGGING  LOGGING  LOGGING  LOGGING  LOGGING  LOGGING  LOGGING  LOGGING  LOGGING  
  //
  //  There are 3 logging "channels" used during system boot:
  //    A) Logging to the file EBTKSLog.log the SDCARD root directory.       Look for calls to LOGPRINTF()
  //    B) Logging to the HP8x CRT.                                          Look for usage of log_to_CRT_ptr which is almost entirely in loadConfiguration()
  //    C) Logging to the diagnostic serial port (over USB).                 Look for usage of log_to_serial_ptr which is in this file and loadConfiguration()
  //
  //  (A) Depends on a functioning SD Card interface and an SD Card.
  //  (B) Depends on the HP8x being in run mode and able to do DMA. Until that occurs (when the boot process has completed), we buffer these messages
  //      in CRT_Log_Buffer, which has limited size. We DO NOT check for overflow of this buffer, but we do send a message to (A) and (C) indicating
  //      the amount of the buffer used. Look for something like this "CRT Log buffer usage is 685 of 1024 charcters"
  //  (C) Like (B), we hold off sending these messages to the USB serial port, except the wait is until we get to loop().
  //      The buffer also has a limited size. We DO NOT check for overflow of this buffer, but we do send a message to (A) and (C) indicating
  //      the amount of the buffer used. Look for something like this "Serial Log buffer usage is 1027 of 2048 charcters"
  //
  //  Initially, we can't write to the CRT because PWO is still asserted, and the HP85 bus
  //  is not yet active.
  //
  //  CRT_Log_Buffer    is CRT_LOG_BUFFER_SIZE    long (1 kB when this comment written)
  //  Serial_Log_Buffer is SERIAL_LOG_BUFFER_SIZE long (2 kB when this comment written)
  //
  //  At this point we haven't yet read the CONFIG.TXT file, so we don't yet know if we
  //  should pause booting up until the serial port is active.
  //

  log_to_CRT_ptr    = &CRT_Log_Buffer[0];
  log_to_serial_ptr = &Serial_Log_Buffer[0];

  *log_to_CRT_ptr    = 0;
  *log_to_serial_ptr = 0;

  //
  //  Setup io function calls
  //

  initIOfuncTable();
  initRoms();
  //
  //  Can't init CRT yet as we need to process CONFIG.TXT
  //

  pinMode(CORE_PIN_NEOPIX_TSY, OUTPUT);       //  The single pin interface to the two WS2812B/E RGB LEDs

  setIOWriteFunc(0340,&ioWriteAuxROM_Alert);  //  AUXROM Alert that a Mailbox/Buffer has been updated
  setIOReadFunc(0340,&onReadAuxROM_Alert);    //  Use HEYEBTKS to return identification string

  SCOPE_1_Pulser(4);                          //  2/7/2021 This occurs about 285 us after the previous call to Pulser(5) (inc 10 us delay)
  EBTKS_delay_ns(10000); //  10 us

  //
  //  Only enable the following wait for serial if we are also activating Serial.printf() diagnostics while debugging something that the normal logging can't handle (like system crash during boot)
  //
  //  while (!Serial)
  //  {
  //    //  Wait for serial terminal before we proceed
  //  }

  //
  //    Copy the Time and Date from the Secure RTC to the general RTC
  //
  setSyncProvider(getTeensy3Time);

  //extern void OnPress(int key);
  //
  //myusb.begin();
	//keyboard1.attachPress(OnPress);
	//keyboard2.attachPress(OnPress);

  initialize_RMIDLE_processing();             //  This must be done before we get to CONFIG.TXT processing

  log_to_serial_ptr += sprintf(log_to_serial_ptr, "EBTKS Firmware built on %s\n\n", EBTKS_COMPILE_TIME);
  SCOPE_1_Pulser(3);                          //  2/7/2021 This occurs about 18 us after the previous call to Pulser(4) (inc 10 us delay)
  EBTKS_delay_ns(10000);                      //  10 us

  log_to_serial_ptr += sprintf(log_to_serial_ptr, "%s", LOGLEVEL_GEN_MESSAGE);
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "%s", LOGLEVEL_AUX_MESSAGE);
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "%s", LOGLEVEL_1MB5_MESSAGE);
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "%s", LOGLEVEL_TAPE_MESSAGE);

  log_to_serial_ptr += sprintf(log_to_serial_ptr, "Phi_1_high_count = %lu   Phi_1_low_count = %lu", Phi_1_high_count, Phi_1_low_count);

  //  Use CONFIG.TXT file on sd card for configuration

  log_to_serial_ptr += sprintf(log_to_serial_ptr, "\nDoing SD.begin\n");
  SCOPE_1_Pulser(2);  SET_SCOPE_1;            //  SCOPE_1_Pulser does 2 toggles, so this inverts things to give some additional info
                                              //  2/7/2021 This occurs about 16 us after the previous call to Pulser(3) (inc 10 us delay)
  //
  //  Previously we used the FIFO_SDIO mode, but found that we got random errors because the related
  //  library code in
  //      File:       SdioTeensy.cpp
  //      Function:   bool SdioCard::readData(uint8_t* dst)
  //      At line:    736
  //  has a critical section that is guarded by
  //      noInterrupts();   a macro that is actually __disable_irq()
  //      interrupts();     a macro that is actually __enable_irq()
  //
  //  This caused random delays in the EBTKS pinChange_isr() , sometimes quite long.
  //  The working theory is that the registers being modified in the critical region
  //  are in a different clock domain than all the GPIO registers that we read/write
  //  extensively throughout the EBTKS firmware without significant delays (i.e. the
  //  GPIO registers are in a 600 MHz domain, but the Sdio registers are in maybe an
  //  80 MHz, or 50 MHz domain). Depending on the phase of the clock at the time of
  //  call to SdioCard::readData() , the execution time of the critical region could
  //  be long enough to corrupt the carefully timed execution of pinChange_isr().
  //
  //  The alternative mode, DMA_SDIO , never calls SdioCard::readData(uint8_t* dst),
  //  but rather uses SdioCard::readSector() which does not have an equivalent
  //  critical region. Thus, crisis averted.
  //
  //  Aside: SdioTeensy.cpp has   bool SdioCard::writeData(const uint8_t* src)  at
  //         line 885 , which has very similar looking code that is not guarded with
  //         noInterrupts() / interrupts() .  I should probably report this to the
  //         developer.
  //
  //  Note that DMA_SDIO is not without its own issues, but easily avoided. The
  //  issue is random failures if the target memory for SD Card Read/Write is
  //  memory in the EXTMEM region (accessed via its own serial interface).
  //  So just make sure that all SD Card read/writes are to FASTRUN or DMAMEM memory
  //
  //  Update 2023_01_12   SdioTeensy.cpp does not seem to have changed with regard to these observations. need to do a diff #####
  //


  SD_begin_OK = SD.begin(SdioConfig(DMA_SDIO));   //  This takes about 9 ms.
  CLEAR_SCOPE_1;                                  //  2/7/2021 This occurs about 9 ms after the previous call to Pulser(2)
  EBTKS_delay_ns(1000);
  SCOPE_1_Pulser(2);

  log_to_serial_ptr += sprintf(log_to_serial_ptr, "Access to SD Card is %s\n", SD_begin_OK     ? "true":"false");

  if (SD_begin_OK)
  {
    logfile_active = open_logfile();
    log_to_serial_ptr += sprintf(log_to_serial_ptr, "logfile_active is %s\n", logfile_active ? "true":"false");
    LOGPRINTF("\n----------------------------------------------------------------------------------------------------------\n");
    LOGPRINTF("\nBegin new Logging session at ");

    int now_day     = day();  
    int now_month   = month();
    int now_year    = year(); 
    int now_hour    = hour();  
    int now_minute  = minute();
    int now_second  = second();
    LOGPRINTF("%.2d/%.2d/%4d ", now_month, now_day, now_year);
    LOGPRINTF("%.2d:%.2d:%.2d\n", now_hour, now_minute, now_second);

    // uint32_t ccsidr;
    // SCB_ID_CSSELR = 0;            //  Select the Data Cache     DDI0403E_B_armv7m_arm.pdf page 725
    // ccsidr = SCB_ID_CCSIDR;       //  Get the size info         DDI0403E_B_armv7m_arm.pdf page 724
    // LOGPRINTF("\nCache Configuration\n");
    // LOGPRINTF("Data CCSIDR:               %08X\n"  , (int)ccsidr);
    // LOGPRINTF("%s", ((ccsidr >> 31) & 1) ? "Data Cache level supports write-through\n" : "");
    // LOGPRINTF("%s", ((ccsidr >> 30) & 1) ? "Data Cache level supports write-back\n" : "");
    // LOGPRINTF("%s", ((ccsidr >> 29) & 1) ? "Data Cache level supports read-allocation\n" : "");
    // LOGPRINTF("%s", ((ccsidr >> 28) & 1) ? "Data Cache level supports write-allocation\n" : "");
    // LOGPRINTF("Data Cache Sets:              %5d\n", (int)((ccsidr >> 13) & 0x00007FFF) + 1);
    // LOGPRINTF("Data Cache Associativity:     %5d\n", (int)((ccsidr >>  3) & 0x000003FF) + 1);
    // LOGPRINTF("Data Cache Line Size Bytes:   %5d\n", int_power(2, ((ccsidr >>  0) & 0x00000007) + 2) * 4 );
    // SCB_ID_CSSELR = 1;            //  Select the Instruction Cache     DDI0403E_B_armv7m_arm.pdf page 725
    // ccsidr = SCB_ID_CCSIDR;       //  Get the size info                DDI0403E_B_armv7m_arm.pdf page 724
    // LOGPRINTF("\nInstruction CCSIDR:               %08X\n"  , (int)ccsidr);
    // LOGPRINTF("%s", ((ccsidr >> 31) & 1) ? "Instruction Cache level supports write-through\n" : "");
    // LOGPRINTF("%s", ((ccsidr >> 30) & 1) ? "Instruction Cache level supports write-back\n" : "");
    // LOGPRINTF("%s", ((ccsidr >> 29) & 1) ? "Instruction Cache level supports read-allocation\n" : "");
    // LOGPRINTF("%s", ((ccsidr >> 28) & 1) ? "Instruction Cache level supports write-allocation\n" : "");
    // LOGPRINTF("Instruction Cache Sets:              %5d\n", (int)((ccsidr >> 13) & 0x00007FFF) + 1);
    // LOGPRINTF("Instruction Cache Associativity:     %5d\n", (int)((ccsidr >>  3) & 0x000003FF) + 1);
    // LOGPRINTF("Instruction Cache Line Size Bytes:   %5d\n", int_power(2, ((ccsidr >>  0) & 0x00000007) + 2) * 4 );

    LOGPRINTF("\nDMA SDIO mode.\n");
    LOGPRINTF("Loading configuration...\n");
    flush_logfile();

    initialize_SD_functions();      //  Used by the AUXROM functions

    // the configuration will init the required devices in most cases.....

                                //  It took 18 ms to get to here from SD.begin (open logfile, send some stuff, Init HPIB/Disk)
                                //  With 7 ROMs being loaded and JSON parsing of CONFIG.TXT , loadConfiguration()  takes 108ms
                                //  Note: The "CONFIG.TXT" file name is specified in EBTKS_Global_Data.h
    config_success = loadConfiguration(Config_filename);  //  Reports success even if SD.begin() failed, as it uses default config
                                                          //  This is probably not a good decission
                                                          //  2/7/2021 Time for the whole function, with 7 ROMs loaded is 88ms
  }
  else
  { //  SD.begin failed
    log_to_serial_ptr += sprintf(log_to_serial_ptr, "Logfile is not active\n");
    logfile_active = false;
    config_success = false;
  }

  //
  //  CRT Emu initialization must occur after CONFIG.TXT processing, as it depends on parameters from that process.
  //  #####  Note, if config failed (for example, SD Card not plugged in) then many parameters may be wrong
  //
  if (config_success)   //  Can't emulate the CRT if we don't know if we have a HP85 style CRT controller, or HP86/87 CRT
  {
    initCrtEmu();
  }
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "get_machineNum(): %3d\n", get_machineNum());

//
//  EMC initialization must occur after CONFIG.TXT processing, as it depends on parameters from that process.
//
#if ENABLE_EMC_SUPPORT
  if (config_success && get_EMC_Enable())
  {
    emc_init();
  }
#endif

//  //
//  //  For some diagnostic scenarios, it would be nice to delay startup until the serial terminal is connected.
//  //  This is controlled by a parameter in CONFIG.TXT  .  But there is also the scenario where that parameter
//  //  has been set accidentally by a user that is unfamiliar with this feature, so the following code includes
//  //  a timeout, so the HP85 is not permanently waiting. 10 seconds seems to not too little and not too much.
//  //
//
//  require_serial_timeout = systick_millis_count + 10000;    //  set timeout for serial to 10 seconds from now
//  if (requireserial)
//  {
//    while (!Serial)
//    {
//      if (require_serial_timeout < systick_millis_count)
//      {
//        break;                                              //  Give up, we have waited long enough for the serial terminal to connect
//      }
//    };
//  }

  Serial.begin(115200);           //  USB Serial Baud value is irrelevant for this serial channel

  setupPinChange();           //  Set up the two critical interrupt handlers on Pin Change (rising) on Phi 1 and Phi 2

//  SET_SCOPE_1;                  //  SCOPE_1 normally indicates we are in the ISR. But during PWO testing, this will show the startup
//                            //  race between Teensy and the HP-85
//                            //  Results: With Firmware.hex at 96 KB, RAM 87K , Flash 35K, from power up, indicated by 6V
//                            //  power supply powering Teensy, which in turn provides 3.3V to the pull-up resistor for
//                            //  PWO_out, to this SET_SCOPE_1 is 252 ms == Teensy boot time. Total PWO time is 352ms because
//                            //  of the 20 ms delay below.
//                            //  Without Teensy, HP-85A PWO time is from 5V rising to PWO rising is 156 to 158 ms
//                            //  So HP-85 wins the race, so we do need the PWO_OUT to hold it in reset until we are ready
//  CLEAR_SCOPE_1;                //  SCOPE_1 only used here with the above SET_PWO to do PWO startup timing.

  EBTKS_delay_ns(10000);      //  10 us
  SCOPE_1_Pulser(7);          //        We are about to enable Pin Change Interrupts. Once we do, we should not have any more calls to SCOPE_1_Pulser()
                              //        2/7/2021 This occurs about XXXX ms after the previous call to Pulser(2)
  EBTKS_delay_ns(10000);      //  10 us

  //delay(10000);   // delay 10 secs to turn on serial monitor

  RELEASE_PWO_OUT;          // This should allow the HP-85 to start up, and we are ready to serve. Note that this asynchronous.
  //
  //  Hold off enabling interrupts until PWO has gone high (other devices or the Power supply may be asserting it)
  //
  WAIT_WHILE_PWO_LOW;       //  This is where we apparently hang if PWO does not go high
                            //  Under normal conditions, if we are the last one holding PWO Low,
                            //  when we release it, it should rise immediately. There is no apparent pull up resistor though. Internal to 1MB1?
  //
  //  From PWO going high, we have less than 4 us until the first LMA which will be to the system ROM
  //  and the first access to the service ROM is about 63 us later
  //  The first reference we care about occurs at 63 us
  //  Search L:\acquired_data_F_I\HP\HP85\Philips_HP-85_Architecture_Notes.txt for "Boot Sequence"
  //
  //
  //  Here we enable interrupts, but the end of PWO is asynchronous to the clocks. So first we sync
  //  up to the falling edge of Phi 2
  //

  if (IS_PHI_2_HIGH)
  {
    WAIT_WHILE_PHI_2_HIGH;
  }
  //  Phi 2 is now low (or worst case has just gone high)
  //  but it may have been low, maybe even just before the rising edge, so sync needs to be sure we have a falling edge
  WAIT_WHILE_PHI_2_LOW;
  WAIT_WHILE_PHI_2_HIGH;
  //
  //  Now we are in sync, just after Phi 2 Falling edge, so there shouldn't be any edge interrupts while we clear out any pending ones
  //
  delayNanoseconds(100);                    //  Split the 200 ns delay into two pieces, so we have 100 ns later (see 3 lines below)
  PHI_1_and_2_ISR = PHI_1_and_2_ISR;        //  Clear any pending interrupts
  PHI_1_and_2_IMR = 0;                      //  Still mask the pin interrupts.
  delayNanoseconds(100);                    //  Other half of 200 ns delay. Put here in case there are synchronizer delays after changing
                                            //  ISR/IMR registers.
  NVIC_CLEAR_PENDING(IRQ_GPIO6789);         //  Even though interrupts have been disabled, the NVIC can still set its pending flags
                                            //  thus leading to the undesireable unexpected interrupt when the interrupts are enabled.
  NVIC_ENABLE_IRQ(IRQ_GPIO6789);            //  Enable interrupt processing, and processing of all bus requests to ROM and RAM, etc.
                                            //  At this point, the ISR's are active, bus cycles are being monitored, and if matched
                                            //  to bank-switched ROM, or RAM that we are supporting, those are handled in the ISRs
                                            //  There appears to be a short PWO high for 2 ms, so let's wait a bit and make sure
                                            //  real PWO high is 187 ms later
  //PHI_1_and_2_IMR = (BIT_MASK_PHASE1 | BIT_MASK_PHASE2);   //  Enable Phi 1 and Phi 2 interrupts
  PHI_1_and_2_IMR = (BIT_MASK_PHASE1);      //  Enable Phi 1 only.  2020_12_06

  //
  //  We have just released PWO, and have enabled our pin-change interrupts. So this is our earliest opportunity to use DMA to snoop
  //  around.
  //  ####  What if we snooped the CRT controller if the config failed, so we could figure out whether we are on a HP85A/B/83/9915
  //  ####  or a HP86/87, and do initCrtEmu() here using our snoop info, and in particular, set Is8687 and IS_HP86_OR_HP87
  //  ####  Probably should also set all config things that didn't get set from failed SD.begin or failed loadConfiguration()
  //  ####  to safe values. This would allow the    no_SD_card_message()   call in loop() a chance to work on HP86 and HP87
  //  ####  Current default behavior assumes HP85 style CRT controller, which crashes on an HP87 with SDCard unplugged
  //

  delay(10);                                //  Wait 10 ms before falling into loop()  This might be BAD. What if a device op occurs while we are waiting?    ########
                                            //  Seems there are some issues of loop functions interfering with the Service ROM initial sanity check,
                                            //  Specifically the call to Three_Shift_Clicks_Poll() which does DMA every 10 ms  (code currently moved to Deprecated_Code.cpp)

  //
  //  We won't get to this point in the code if the HP-85 is not running with valid clocks and PWO
  //
  //  while (!Serial) {};                       //  wait till the Virtual terminal is connected
  //  Serial.begin(9600);                       //  USB Serial Baud value is irrelevant for this serial channel

  log_to_serial_ptr += sprintf(log_to_serial_ptr, "Config Success is %s\n"   , config_success  ? "true":"false");

  log_to_serial_ptr += sprintf(log_to_serial_ptr, "Parameters from the CONFIG.TXT (or defaults)\n");
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "Machine number     %d\n", get_machineNum());
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "Machine type       %s\n", get_machineType());
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "CRT Verbose        %s\n", get_CRTVerbose() ? "true":"false");
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "Screen Emu.        %s\n", get_screenEmu()  ? "true":"false");
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "Remote CRT         %s\n", get_CRTRemote()  ? "true":"false");
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "85A 16 KB RAM      %s\n", getHP85RamExp()  ? "true":"false");
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "HP85A/B Tape Emu.  %s\n", get_TapeEn()     ? "true":"false");

#if ENABLE_EMC_SUPPORT
  log_to_serial_ptr += sprintf(log_to_serial_ptr, "EMC Support        %s\n", get_EMC_Enable() ? "true":"false");
  if (get_EMC_Enable())
  {
    log_to_serial_ptr += sprintf(log_to_serial_ptr, "EMC Num Banks      %d, 32 KB each\n", get_EMC_NumBanks());
    log_to_serial_ptr += sprintf(log_to_serial_ptr, "EMC Start Bank     %d\n", get_EMC_StartBank());
    log_to_serial_ptr += sprintf(log_to_serial_ptr, "EMC Start Addr     %08o Octal\n", get_EMC_StartAddress());
    log_to_serial_ptr += sprintf(log_to_serial_ptr, "EMC End Addr       %08o Octal\n", get_EMC_EndAddress());
  }
#endif

  //
  //  This should be the end of our writing to this buffer for these startup messages.
  //  All future messages just use Serial.printf()
  //

  Serial.flush();

  Serial.printf("Waiting for HP85 to get to prompt\n");

  Initial_LED_load(false);      //  turn the LEDs off, if they are on

  delay(1000);

  Initial_LED_load(false);      //  turn the LEDs off, if they are on

  Logic_Analyzer_Event_Count_Init = -1000;      // Use this to indicate the Logic analyzer has no default values.

//
//  hook in time/date to the filesystem
//
  FsDateTime::setCallback(dateTime);

  //  Fall into loop()
  loop_count = 0;
}

//
//  Is it possible with Teensy powered by USB for the double LMA to get out of sync?
//
//  Scenario: Power cycle HP-85 after first cycle but before second. Teensy is running
//            in loop() and is un-aware that HP- 85 has restarted. Could handle this by
//            resetting the First/Second address byte flags if a non LMA cycle is seen???
//            A related issue is that since Teensy is in loop(), it has state that may
//            not be correct for the power cycled HP-85 Maybe a better solution is to
//            have Teensy only powered by HP-85, even if USB is connected, so Teensy
//            is always reset when the power is cycled, and Teensy could also monitor
//            PWO (and only drive it in setup()  ) See the commented code at the
//            beginning of loop() that is a failed attempt at this issue.
//

//
//      All the background tasks live here. They are triggered by flags set in the interrupt handlers
//      and they are serviced here in a round-robin sequence
//

//
//  Loop execution time analysis when the HP85 is idle. 2/8/2021:
//      Apparently bi-modal during idle: 440 ns and 1220 ns
//      Turns out the difference is whether a Phi 1 interrupt occurs during the loop
//      which adds about 800 ns.
//




#define TRACE_LOOPTRANSLATOR_TIMING        (0)

#if TRACE_LOOPTRANSLATOR_TIMING
//uint32_t      loopTranslator_prior_exit_time;
uint32_t      loopTranslator_entry_time;
uint32_t      loopTranslator_duration_ms;
#endif

void loop()
{
  if (IS_PWO_LOW)
  {
    SCB_AIRCR = 0x05FA0004;           //  Do a processor reset, so we don't get to the next line.
  }

//
//  This code is disabled until I think it through better. The problem it is trying to solve is if while working
//  on the HP-85, I pulse PWO Low to reset it. Teensy does not get reset, and this is bad
//
//  if (IS_PWO_LOW)    // if PWO is low, reset Teensy 4.0      I think this need to be moved into the Phi 1 interrupt handler, so that we can reset immediately
//                    //                                      but it will probably need some guarding code in setup() that looks at the reset reason register
//  { // See Teensy 4.0 Notes about this magic
//    SCB_AIRCR = 0x05FA0004;
//  }

  /*   delay(200);

  writeCRTflag = false;
  dumpCrtAlpha();

  //dumpCrtAlphaAsJSON();   //@todo write a nodejs serial/websocket server and a simple webpage to mimic the hp85 CRT
                            // then add graphics. detect writes to, say a 64x64 bit area, and only send that area
                            // to speed things up
 */

	Serial_Command_Poll();    //  84 ns when idle, but 800 ns ish if Phi 1 interrupt occurs while running
  tape.poll();              //  72 ns when idle, but 800 ns ish if Phi 1 interrupt occurs while running
  AUXROM_Poll();            //  32 ns when idle, but 760 ns ish if Phi 1 interrupt occurs while running
  Logic_Analyzer_Poll();    //  54 ns when idle, but 780 ns ish if Phi 1 interrupt occurs while running

#if TRACE_LOOPTRANSLATOR_TIMING
  loopTranslator_entry_time = systick_millis_count;
  //Serial.printf("SDWRITE entry. Time since last exit %d ms  ,", loopTranslator_entry_time - loopTranslator_prior_exit_time);
#endif

  loopTranslator();         //  1MB5 / HPIB / DISK poll
                            //  150 ns when idle, but 920 ns ish if Phi 1 interrupt occurs while running
#if TRACE_LOOPTRANSLATOR_TIMING
  loopTranslator_duration_ms = systick_millis_count - loopTranslator_entry_time;
  if (loopTranslator_duration_ms > 1)
  {
    Serial.printf("loopTranslator() took %d ms\n", loopTranslator_duration_ms);
  }
#endif

  if (get_CRTRemote())
  {
    if ((systick_millis_count % 10) == 0)     //  Only do this every 10 ms, although it may occur multiple times during that 1 ms
    {                                         //  Note: if it turns out that we get buffer overflow between the Teensy and the ESP32
                                              //  we will have to call this more often
      esp32.poll();                           //  Access with "http://esp32_ebtks.local/"
    }
  }

  if (!LEDs_Initialized)
  {
    if (systick_millis_count > 2000)
    {
      Initial_LED_load(true);      //  Set LEDs to initial value
    }
    else
    {
      Initial_LED_load(false);    //  Turn LEDs off
    }
  }

  if (CRT_Boot_Message_Pending)     //  8 ns test is false, but 800 ns ish if Phi 1 interrupt occurs while running
  {                                 //  which is quite rare because the window is so narrow
    if (is_hp85_idle())
    {
      if (!SD_begin_OK)
      {
        no_SD_card_message();
      }
      else
      {
        Boot_Messages_to_CRT();
      }
      CRT_Boot_Message_Pending = false;
    }
    else
    {
      //  Nothing to do, keep waiting
    }
  }

  //
  //  Here are things we do no more than once per second
  //

  if ((one_second_timer + 1000) < systick_millis_count)     //  systick_millis_count rolls over about every 49 days and so does one_second_timer
  {                                                         //  12 ns mostly, but 800 ns ish if Phi 1 interrupt occurs while running
    one_second_timer = systick_millis_count;                //  which is quite rare because the window is so narrow
    //
    //  Things to check
    //
    //  1   If Serial is active and the serial log has not been sent, send it now.
    //  2   If the text sent to the CRT has not yet been also sent to Serial, send it now.
    //
    if (Serial)
    {
      if(Serial_Log_Message_Pending)
      {
        if (strlen(Serial_Log_Buffer) > SERIAL_LOG_BUFFER_SIZE)
        {
           Serial.printf("\nWarning Warning Warning Warning Warning Warning Warning SERIAL_LOG_BUFFER_SIZE is too small\n");
           LOGPRINTF("\nWarning Warning Warning Warning Warning Warning Warning SERIAL_LOG_BUFFER_SIZE is too small\n");      //  Clearly this is inefficient. fix it later
        }
        Serial.printf("\nSerial Log buffer usage is %d of %d charcters\n\n", strlen(Serial_Log_Buffer), SERIAL_LOG_BUFFER_SIZE);
        LOGPRINTF    ("\nSerial Log buffer usage is %d of %d charcters\n", strlen(Serial_Log_Buffer), SERIAL_LOG_BUFFER_SIZE);      //  Clearly this is inefficient. fix it later
        Serial.printf("%s\n", Serial_Log_Buffer);
        Serial_Log_Message_Pending = false;
      }
      if(CRT_Boot_Message_Pending_to_Serial)
      {
        if (strlen(CRT_Log_Buffer) > CRT_LOG_BUFFER_SIZE)
        {
           Serial.printf("\nWarning Warning Warning Warning Warning Warning Warning CRT_LOG_BUFFER_SIZE is too small\n");
           LOGPRINTF    ("\nWarning Warning Warning Warning Warning Warning Warning CRT_LOG_BUFFER_SIZE is too small\n");      //  Clearly this is inefficient. fix it later
        }
        Serial.printf("\nCRT Log buffer usage is %d of %d characters\n\n", strlen(CRT_Log_Buffer), CRT_LOG_BUFFER_SIZE);
        LOGPRINTF    ("CRT Log buffer usage is %d of %d characters\n", strlen(CRT_Log_Buffer), CRT_LOG_BUFFER_SIZE);      //  Clearly this is inefficient. fix it later
        Serial.printf("%s\n", CRT_Log_Buffer);
        Serial.printf("EBTKS> ");
        CRT_Boot_Message_Pending_to_Serial = false;
      }
    }
    flush_logfile();
  }

  loop_count++;               //  Keep track of how many times we loop. We don't currently use this for anything,
                              //  and it probably overflows somewhere between hourly to daily.
}

//
//  The HP85 idle loop includes label "EXEC" at address 000072. This is a relatively
//  tight loop. On any given call to this function, there is a random chance that the
//  current value of the address register is this address. If we call this function
//  in a "tight" loop, eventually we will get a hit. This is reasonable, since this
//  function is only called from one place, it is a tight loop, and we don't have
//  anything better to do, until the HP85 gets to its idle loop.
//

bool is_hp85_idle(void)
{
  if (addReg == 000072)
  {
    return true;
  }
  else
  {
    return false;
  }
}

//
//  During boot the CRT is unavailable. So we store all the boot status info in
//  CRT_Log_Buffer.
//  Once the CRT is available (detected by seeing the HP85 making references
//  to 000072, which is part of the EXEC loop), we dump the array to the CRT
//

extern void Safe_Write_CRTBAD(uint16_t badAddr);
extern void Safe_Write_CRTSAD(uint16_t sadAddr);

void Boot_Messages_to_CRT(void)
{

  int row, column;
  int seg_length;
  char segment[100];

  log_to_CRT_ptr    = &CRT_Log_Buffer[0];
  if ((strlen(log_to_CRT_ptr) == 0) || (!get_CRTVerbose()))
  {
    return;
  }

  row = 1;                                            //  Start at line 1 so there will be a blank line at the top of the screen
  column=0;

  while (*log_to_CRT_ptr)
  {
    seg_length = strcspn(log_to_CRT_ptr,"\n");        //  Number of chars not in second string, so does not count the '\n'
    strlcpy(segment, log_to_CRT_ptr, seg_length+1);   //  Copies the segment, and appends a 0x00
    log_to_CRT_ptr += seg_length+1;                   //  Skip over the segment and the trailing '\n'
    Write_on_CRT_Alpha(row++, column, segment);
    if (row < 16)                                     // While on screen, add some delay between lines. After line 16, no need for delays, since it isn't seen
      {
        delay(40);                                    //  A little delay so they see it happening
      }
    if (row > 63)                                     //  ##########  These constants for line number are specific to HP85A/B.
    {                                                 //  ##########  If we have a HP86/87, there are multiple ALPHA modes each with different max lines
      row = 63;                                       //  Just over-write the last line. Note we start on line 1, so max lines is 63. We leave line 0 alone.
    }
    if (row == 15)
    {
      Write_on_CRT_Alpha(row++, column, "Scroll up for more");
    }
  }
  //
  // Write_on_CRT_Alpha(0,0, first_char);
  //
  Safe_Write_CRTBAD(0);
  Safe_Write_CRTSAD(0);
}
int get_wifi_key()
{
  return esp32.getKybdCh();
}


