#include <Arduino.h>
#include <setjmp.h>
#include "SdFat.h"

//  The next line must exist in just one compilation unit, which is this file.
//  All other usage of the globals file msut not have this line, leaving ALLOCATE undefined
#define ALLOCATE  1

#include "Inc_Common_Headers.h"

#include "USBHost_t36.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
KeyboardController keyboard1(myusb);
KeyboardController keyboard2(myusb);



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
//      Logic Analyzer Configuration 1
//
//      Channel         Signal
//
//      00              Phi_1
//      01              Phi_2
//      02              LMA
//      03              RD
//      04              WR
//      05              BUS_CTRL_DIR
//      06              HALT
//      07              PWO_OUT
//      08              PWO_IN
//      09              RXD
//      10              TXD
//      11
//      12
//      13
//      14
//      15
//      GND_1           Teensy_Pin  1
//      GND_2           Teensy_Pin 32
//
//
//      Logic Analyzer Configuration 2
//
//      Channel         Signal     Physical Teensy 4.0 Pin
//                                 1..14, 15..28
//      00              Phi_1      9
//      01              Phi_2      10
//      02              LMA        6
//      03              RD         7
//      04              WR         8
//      05              HALT       13
//      06              D7         25
//      07              D6         24
//      08              D5         18
//      09              D4         19
//      10              D3         17
//      11              D2         16
//      12              D1         20
//      13              D0         21
//      14              RXD        22
//      15              TXD        23
//      GND_1                      1
//      GND_2                      27
//
//
//      Logic Analyzer Configuration 3   I think this table is wrong<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//
//      Channel         Signal     Physical Teensy 4.1 Pin
//                                 1..24, 25..48
//      00              Phi_1      9
//      01              Phi_2      10
//      02              LMA        6
//      03              RD         7
//      04              RC         5            view RC (pin 5) instead of WR (pin 8)
//      05              HALT       13
//      06              D7         25
//      07              D6         24
//      08              D5         18
//      09              D4         19
//      10              D3         17
//      11              D2         16
//      12              D1         20
//      13              D0         21
//      14              RXD        22
//      15              TXD        23
//      GND_1                      1
//      GND_2                      34, 47
//
//
//      Logic Analyzer Configuration 4		See LMA, RD, WR, RC.  Don't see HALT
//
//      Channel         Signal     Physical Teensy 4.1 Pin
//                                 1..24, 25..48
//      00              Phi_1      43
//      01              Phi_2      30
//      02              /LMA       44
//      03              /RD        45
//      04              /WR        42
//      05              /RC        19
//      06              D7         38
//      07              D6         39
//      08              D5         33
//      09              D4         32
//      10              D3         37
//      11              D2         36
//      12              D1         40
//      13              D0         41
//      14              RXD        10
//      15              TXD        9
//      GND_1                      1
//      GND_2                      34
//      GND_3                      47
//
//      Logic Analyzer Configuration 5    Watch all the control signals in/out of Teensy
//
//      Channel         Signal     Physical Teensy 4.1 Pin
//                                 1..24, 25..48
//      00              Phi_1      43
//      01              /LMA       44
//      02              /RD        45
//      03              /WR        42
//      04              DIR/RC     19
//      05              /HALT      20
//      06              BUFEN       2
//      07              CTRL_DIR    3
//      08              CTRLEN     21
//      09              PRIH       22
//      10              INTPRI      5
//      11              INTERRUPT  29
//      12              
//      13              
//      14              RXD        10
//      15              TXD        9
//      GND_1                      1
//      GND_2                      34
//      GND_3                      47

//
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

////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////

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
       T26        26          39      T39
    DIR_RC        27          38      PHASE2
     HALTX        28          37      INTERRUPT
    CTRLEN        29          36      PWO_O
  IPRIH_IN        30          35      PHASE21
      PWO_L       31   SD     34      PHASE12
     IFETCH       32  Card    33      T33


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
T33             CORE_PIN33_BIT      7       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_07
PHASE12         CORE_PIN34_BIT     29       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_13
PHASE21         CORE_PIN35_BIT     28       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_12
PWO_O           CORE_PIN36_BIT     18       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_02
INTERRUPT       CORE_PIN37_BIT     19       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_03
PHASE2          CORE_PIN38_BIT     28       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_12
T39             CORE_PIN39_BIT     29       GPIO6       IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_13
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
//  Startup timing
//
//
//
//
//

static uint32_t   one_second_timer = 0;

void setup()
{

  int   setjmp_reason;
  int   message_count = 0;
  bool  SD_begin_OK;
  bool  config_success;

  setjmp_reason = setjmp(PWO_While_Running);
  if (setjmp_reason == 99)
  {
    //
    //  We are here from a PWO while running. Someone has the covers off, and is forcing a restart.
    //  While setjmp unwinds the stack etc, it doesn't reset any of the processor state. So do some cleanup here
    //
    NVIC_DISABLE_IRQ(IRQ_GPIO6789);   //  This seems the most important one. there are probably other things.
                                      //  Add them as needed
                                      //  - for example, the Service ROM test fails because it reads back FF, FF
                                      //    so that means we aren't yet doing initializing correctly
  }

  //
  //  Diagnostic Pins
  //
  //	Name							Physical Pin		Suggested use
  //	CORE_PIN_RXD			10							ISR Entry and Exit
  //	CORE_PIN_TXD			 9							General entry and exit of whatever is currently being debugged, currently DMA
  //																		Also used by TXD_Pulser, which is used to track timing of the Boot sequence
  //	CORE_PIN_T13			35							Also the on board LED. Toggle when errors are detected in diagnostic tests
  //	CORE_PIN_T33			25							General oscilloscope trigger, used in timing tuning
  //	CORE_PIN_T39			31							No specific use
  //

  pinMode(CORE_PIN_RXD, OUTPUT);
  CLEAR_RXD;                //  RXD tracks entry and exit of the pin_change ISR, at the heart of the bus interface
  pinMode(CORE_PIN_TXD, OUTPUT);
  CLEAR_TXD;                //  TXD is used for any other debug/time tracking. Maybe multiple things at once. Depends on context

  pinMode(CORE_PIN_T13, OUTPUT);
  //  SET_LED;                //  T13 On Board LED
  CLEAR_LED;
  pinMode(CORE_PIN_T33, OUTPUT);
  CLEAR_T33;                //  T33 Diagnostics
  pinMode(CORE_PIN_T39, OUTPUT);
  CLEAR_T39;                //  T39 Diagnostics


  ASSERT_PWO_OUT;           //  Desire glitch free switchover from resistor asserting PWO to Teensy asserting PWO
  pinMode(CORE_PIN_PWO_O, OUTPUT);      //  Core Pin 36 (Physical pin 28) is pulled high by an external resistor, which turns on the FET,
                            //  which pulls the PWO line low, which resets the HP-85
  ASSERT_PWO_OUT;           //  Over-ride external pull up resistor, still put High on gate, thus holdimg PWO on I/O bus Low

  //  TXD starts low (see above), pulses 5 times, ends low. Pulses are 10 us apart
  TXD_Pulser(5);
  EBTKS_delay_ns(10000);    //  10 us

  _VectorsRam[15] = mySystick_isr;

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

  pinMode(CORE_PIN_RD, INPUT);          //  RD
  pinMode(CORE_PIN_WR, INPUT);          //  WR
  pinMode(CORE_PIN_LMA, INPUT);         //  LMA
  pinMode(CORE_PIN_PHASE1, INPUT);      //  Phi 1
  pinMode(CORE_PIN_PHASE2, INPUT);      //  Phi 2

  pinMode(CORE_PIN_CTRL_DIR, OUTPUT);   //  bus control buffer direction /LMA,/WR,/RD
  CTRL_DIR_FROM_HP;

  pinMode(CORE_PIN_INTERRUPT, OUTPUT);  //  INT inverted by hardware
  RELEASE_INT;

  pinMode(CORE_PIN_IPRIH_IN, INPUT);    //PRIH input - interrupt priority chain input. low if someone above us is interrupting
  pinMode(CORE_PIN_INTPRI,OUTPUT);
  RELEASE_INTPRI;

  pinMode(CORE_PIN_IPRIH_IN, INPUT);    //PRIH input - interrupt priority chain input. Low if someone above us is interrupting
  pinMode(CORE_PIN_INTPRI,OUTPUT);
  RELEASE_INTPRI;

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

  // setup io function calls
  initIOfuncTable();
  initRoms();
  initCrtEmu();

  setIOWriteFunc(0340,&ioWriteAuxROM_Alert); // AUXROM Alert that a Mailbox/Buffer has been updated
  setIOReadFunc(0340,&onReadAuxROM_Alert);   // Use HEYEBTKS to return identification string

  TXD_Pulser(4);
  EBTKS_delay_ns(10000); //  10 us

  //extern void OnPress(int key);
  //
  //myusb.begin();
	//keyboard1.attachPress(OnPress);
	//keyboard2.attachPress(OnPress);

#if DEVELOPMENT_MODE
  //
  //  Wait till the Virtual terminal is connected
  //
  while (!Serial) {  };                                               //  Stall startup until the serial terminal is attached. Do this if we need to see startup messages
#endif

  delay(2000);                //  Give me a chance to turn the terminal emulator on, after I hear the USB enumeration Bing.

  Serial.begin(115200);       //  USB Serial Baud value is irrelevant for this serial channel

  Serial.printf("HP-85 EBTKS Board Serial Diagnostics  %-4d\n", message_count++);
  TXD_Pulser(3);
  EBTKS_delay_ns(10000);      //  10 us

  Serial.printf("\n%s", LOGLEVEL_GEN_MESSAGE);
  Serial.printf("%s", LOGLEVEL_AUX_MESSAGE);
  Serial.printf("%s", LOGLEVEL_1MB5_MESSAGE);
  Serial.printf("%s\n", LOGLEVEL_TAPE_MESSAGE);

  //  Use CONFIG.TXT file on sd card for configuration

  Serial.printf("Doing SD.begin\n");
  TXD_Pulser(2);  SET_TXD;
  SD_begin_OK = true;
  if (!SD.begin(SdioConfig(FIFO_SDIO)))          //  This takes about 115 ms.
  {
    CLEAR_TXD; EBTKS_delay_ns(1000);  TXD_Pulser(2);
    Serial.println("SD begin failed");
    SD_begin_OK = false;
    logfile_active = false;
  }
  else
  {
    CLEAR_TXD; EBTKS_delay_ns(1000);  TXD_Pulser(2);
    logfile_active = open_logfile();
  }
  Serial.printf("logfile_active is %d\n", logfile_active);

  LOGPRINTF("\n----------------------------------------------------------------------------------------------------------\n");
  LOGPRINTF("\nBegin new Logging session\n");

  LOGPRINTF("\nFIFO SDIO mode.\n");
  LOGPRINTF("Loading configuration...\n");
  flush_logfile();

  initialize_SD_functions();      //  Used by the AUXROM functions

  // the configuration will init the required devices in most cases.....

                              //  It took 74 ms to get to here from SD.begin (open logfile, send some stuff, Init HPIB/Disk)
                              //  With 9 ROMs being loaded and JSON parsing of CONFIG.TXT , loadConfiguration()  takes 108ms
  config_success = loadConfiguration(Config_filename);  //  Reports success even if SD.begin() failed, as it uses default config
                                                        //  This is probably not a good decission

  setupPinChange();           //  Set up the two critical interrupt handlers on Pin Change (rising) on Phi 1 and Phi 2

//  SET_RXD;                  //  RXD normally indicates we are in the ISR. But during PWO testing, this will show the startup
//                            //  race between Teensy and the HP-85
//                            //  Results: With Firmware.hex at 96 KB, RAM 87K , Flash 35K, from power up, indicated by 6V
//                            //  power supply powering Teensy, which in turn provides 3.3V to the pull-up resistor for
//                            //  PWO_out, to this SET_RXD is 252 ms == Teensy boot time. Total PWO time is 352ms because
//                            //  of the 20 ms delay below.
//                            //  Without Teensy, HP-85A PWO time is from 5V rising to PWO rising is 156 to 158 ms
//                            //  So HP-85 wins the race, so we do need the PWO_OUT to hold it in reset until we are ready
//  CLEAR_RXD;                //  RXD only used here with the above SET_PWO to do PWO startup timing.

  EBTKS_delay_ns(10000);    //  10 us
  TXD_Pulser(7);
  EBTKS_delay_ns(10000);    //  10 us

  //delay(10000);   // delay 10 secs to turn on serial monitor

  RELEASE_PWO_OUT;          // This should allow the HP-85 to start up, and we are ready to serve
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
  PHI_1_and_2_IMR = (BIT_MASK_PHASE1 | BIT_MASK_PHASE2);   //  Enable Phi 1 and Phi 2 interrupts

  delay(10);                                //  Wait 10 ms before falling into loop()  This might be BAD. What if a device op occurs while we are waiting?
                                            //  Seems there are some issues of loop functions interfering with the Service ROM initial sanity check,
                                            //  Specifically the call to Three_Shift_Clicks_Poll() which does DMA every 10 ms  (code currently moved to Deprecated_Code.cpp)

  //
  //  We won't get to this point in the code if the HP-85 is not running with valid clocks and PWO
  //
  //  while (!Serial) {};                       //  wait till the Virtual terminal is connected
  //  Serial.begin(9600);                       //  USB Serial Baud value is irrelevant for this serial channel
  Serial.printf("HP-85 EBTKS Board Serial Diagnostics  %-4d\n", message_count++);
  Serial.printf("Access to SD Card is %s\n", SD_begin_OK     ? "true":"false");
  Serial.printf("Config Success is %s\n"   , config_success  ? "true":"false");

  Logic_Analyzer_Event_Count_Init = -1000;      // Use this to indicate the Logic analyzer has no default values.

  Serial.flush();
  delay(1000);
  if (!SD_begin_OK)
  {
    no_SD_card_message();
  }

}



//  Is it possible with Teensy powered by USB for the double LMA to get out of sync?  Scenario: Power cycle HP-85 after first
//  cycle but before second. Teensy is running in loop() and is un-aware that HP-85 has restarted.
//  Could handle this by resetting the First/Second address byte flags if a non LMA cycle is seen???
//  A related issue is that since Teensy is in loop(), it has state that may not be correct for the power cycled HP-85
//  Maybe a better solution is to have Teensy only powered by HP-85, even if USB is connected, so Teensy is always
//  reset when the power is cycled, and Teensy could also monitor PWO (and only drive it in setup()  )
//  See the commented code at the beginning of loop() that is a failed attempt at this issue.

//
//      All the background tasks live here. They are triggered by flags set in the interrupt handlers
//      and they are serviced here in a round-robin sequence
//

static int  loop_count = 0;



void loop()
{

  if (IS_PWO_LOW)                    //  Not sure if this works. Needed to be tested
  {
    longjmp(PWO_While_Running, 99);
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

  // delay(200);   //  Delay for 200 ms, so loop about 5 times per second

  //print out the address reg and RSELEC values so we can get an idea of activity

  //Serial.printf("Address reg: %04X R_SELECT %02X\n", addReg, rselec);

  // if (crtControl & (1 << 1))  //wipe screen flag
  //     {
  //       crtControl &= ~(1 << 1);
  //       memset(&Mirror_Video_RAM, 0, sizeof(Mirror_Video_RAM));
  //     }

  /*   delay(200);

  writeCRTflag = false;
  dumpCrtAlpha();

  //dumpCrtAlphaAsJSON();   //@todo write a nodejs serial/websocket server and a simple webpage to mimic the hp85 CRT
                            // then add graphics. detect writes to, say a 64x64 bit area, and only send that area
                            // to speed things up
 */


	Serial_Command_Poll();
  tape.poll();
  AUXROM_Poll();
  Logic_Analyzer_Poll();
  loopTranslator();     //  1MB5 / HPIB / DISK poll
  //myusb.Task();


  //
  //  Here are things we do no more than once per second
  //

  if (one_second_timer < (systick_millis_count + 1000))
  {
    logfile.flush();
    one_second_timer = systick_millis_count;
  }

  loop_count++;
}


