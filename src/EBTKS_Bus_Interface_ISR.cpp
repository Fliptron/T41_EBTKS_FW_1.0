//
//      06/27/2020      These Function were moved from EBTKS.cpp to here.
//
//      07/04/2020      Changes to DMA support. Turn off all interrupts once the DMA process starts
//                      and don't re-enable until all DMA activity is complete
//
//      07/18/2020      Reorganize these routines, check timing, add Logic Analyzer support
//         19           Change the state machine for /LMAX
//         20           Write far more documentation
//         21           Need to enforce globaly that unless EBTKS is driving data onto the bus,
//         22             1) Teensy bus must be set to input:     SET_T4_BUS_TO_INPUT
//                        2) U2 must be enabled:                  ENABLE_BUS_BUFFER_U2
//                        3) U2 direction must be towards Teensy: BUS_DIR_FROM_HP
//                      Go through the code with a fine toothed comb
//
//      07/29/2020      Add support for the (potentially multiple) AUXROM(s) and the special embedded RAM buffers
//      07/30/2020      Fine tune all bus timing, mostly /RC and buffer enables
//      08/10/2020      More timing fine tuning, catch the ones I missed the first time through
//
//      10/05/2020      First attempt ot trace DMA with the logic analyzer
//
//
//  HP85 bus emulation
//      Interrupt on:
//        Phi 1 Rising
//        Phi 2 Rising
//      Logic derived from the reverse engineered ROM drawer board by Jorge Amodio
//        See HP82929A-v3.2_by_Jorge.pdf
//
//  These functions area all running within an ISR, The all need to be short and fast
//
//    The Phi 2 Rising interupt is used to sample the control lines /LMAX, /RDX, /WRX to decide on the cycle type.
//    This is where most of the decissions take place, and the timing in this ISR is not too critical.
//    If the operation is a read, this is where we will be initiating puting data onto the bus and asserting /RCX
//
//    The Phi 1 Rising interrupt is more timing sensitive. If EBTKS is outputting data (bus read from the processor's
//    point of view), we initiated output back in Phi 2. The responsibility in this ISR is to turn the data off,
//    approximately 100 ns AFTER the falling edge of Phi 1.
//    Alternatively, if the processor is doing a bus write, and the address matches one of the addresses that EBTKS
//    must respond to (by reading the value off the bus), the data may become valid as late as 80 ns before the
//    Rising edge of Phi 1. We capture the data somewhat after the Rising edge of Phi 1. The timing for this is fairly tight.
//    If it ever turns out this is too soon, we could add a 100 ns delat (thus mid Phi 1 pulse) which should fix things.
//


#include <Arduino.h>
#include <setjmp.h>

#include "Inc_Common_Headers.h"

inline bool onReadData(void);
inline void mid_cycle_processing(void);
inline void onPhi_1_Rise(void);
inline void onWriteData(uint16_t addr, uint8_t data) __attribute__((always_inline, unused));
inline void onPhi_1_Fall(void);

ioReadFuncPtr_t ioReadFuncs[256];      //ensure the setup() code initialises this!
ioWriteFuncPtr_t ioWriteFuncs[256];

//
//  Variables for the EMC (Extended Memory Controller)
//

#if ENABLE_EMC_SUPPORT
DMAMEM uint8_t      emcram[EMC_RAM_SIZE];
volatile bool       ifetch , m_emc_mult;
volatile uint8_t    m_emc_drp;
uint32_t            m_emc_ptr1 , m_emc_ptr2;        //  EMC pointers
uint8_t             m_emc_mode;
uint8_t             m_emc_state;
volatile bool       m_emc_lmard;
volatile uint32_t   cycleNdx;                       //  Counts the byte index for multi-byte transfers
volatile uint32_t   m_emc_read,m_emc_write;
volatile uint32_t   m_emc_start_addr;
volatile uint32_t   m_emc_end_addr;
volatile bool       m_emc_master;

enum {
		  EMC_IDLE,
		  EMC_INDIRECT_1,
		  EMC_INDIRECT_2
	};
bool emc_r(void);
void emc_w(uint8_t val);
void lma_cycle(bool state);
#endif

volatile bool intEn_1MB5 = false;
int intrState = 0;

bool ioReadNullFunc(void) //  This function is running within an ISR, keep it short and fast.
{
  return false;
}

void ioWriteNullFunc(uint8_t) //  This function is running within an ISR, keep it short and fast.
{
  return;
}

void onWriteGIE(uint8_t val)
{
  (void)val;
  globalIntEn = true;
}

void onWriteGID(uint8_t val)
{
  (void)val;
  globalIntEn = false;
}

//
//  This register is implemented in the 1MB5 translator chips - they all share this register
//
void onWriteInterrupt(uint8_t val)
{
  (void)val;
  intEn_1MB5 = true;                              //  A write to this register enables 1MB5 interrupts
}

void initIOfuncTable(void)
{
  for (int a = 0; a < 256; a++)
  {
    ioReadFuncs[a] = &ioReadNullFunc;             //  Default all
    ioWriteFuncs[a] = &ioWriteNullFunc;
  }
  setIOWriteFunc(0x40, &onWriteInterrupt);        //  177500 1MB5 INTEN
  setIOWriteFunc(0, &onWriteGIE);                 //  Global interrupt enable
}

void removeIOReadFunc(uint8_t addr)
{
  ioReadFuncs[addr] = &ioReadNullFunc;
}

void removeIOWriteFunc(uint8_t addr)
{
  ioWriteFuncs[addr] = &ioWriteNullFunc;
}

void setIOReadFunc(uint8_t addr,ioReadFuncPtr_t readFuncP)
{
  ioReadFuncs[addr] = readFuncP;
}

void setIOWriteFunc(uint8_t addr,ioWriteFuncPtr_t writeFuncP)
{
  ioWriteFuncs[addr] = writeFuncP;
}

bool enRam16k = false;

void enHP85RamExp(bool en)
{
  enRam16k = en;
}

bool getHP85RamExp(void)      //  Report true if HP85A RAM expansion is enabled
{
  return enRam16k;
}

///////////////////////////////////////////////// OLD ///////////////////////////////////////////////////
//  Prior Interrupt scheme:                                                                            //
//      EBTKS has only 2 interrupts, one for Phi 1 rising edge, and one for Phi 2 rising edge          //
//      Both interupts come here to be serviced. They are mutually exclusive.                          //
/////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////// NEW ///////////////////////////////////////////////////
//  New Interrupt scheme (12/06/2020 onward):
//      EBTKS has only 1 interrupt for handling the HP85 I/O bus: Phi 1 rising.
//      Where previously we had a distict Phi 2 Rising interrupt, we now process
//      4 sections:
//          Phi 1 rising edge tasks:    Capture bus for bus writes that are targetted at us
//                                      Process I/O address range writes
//                                      Process memory address range writes
//                                      Process ROM-Window memory address range writes
//          Phi 1 falling edge tasks:   Complete bus reads (where we supply the data and the /RC signal, de-assert /RC)
//                                      Capture bus cycle state for next cycle
//                                      Update our copy of the address register
//                                      Logic Analyzer Acquisition
//          Tweaked delay so that the /RC signal in the next section meets timing requirements of (A) After Phi 2 rising
//                                      and (B) must start no later than 380 ns After Phi 2 rising
//          Handle bus reads that we are responding to, by placing data on the bus and asserting /RC
//                                      Includes RAM reads, ROM reads, ROM-Window reads, I/O reads
//
//
//  The control signals /LMAX, /RD, and /WR are sampled after the falling edge of Phi 1,
//  based on timing analysis of reverse engineering work
//
//  The identified issue that lead to this change of design (OLD -> NEW) is that the
//  old code caused violations of the time from the rising edge of Phi 2 to the latest
//  occurence of the falling edge of /RC on the I/O bus. According to the 1MB5
//  specification, Figure 5, tIF2, should fall no later than 380 ns after Phi 2 rising,
//  and can be asserted a maximum of 200 ns after the falling edge of Phi 1 , spec tRCH.
//
//  We can infer that /RC should not occur any earlier than the rising edge of Phi 2
//  (tIF2 == 0 , not actually specified, but implied) , and the /RC signal can de-assert
//  as early as the falling edge of Phi 1 (tRCH == 0 , not actually specified, but implied)
//  It seems some safety margin for both the start and end of /RC would be prudent.
//  From the patent it would seem that the 200 ns prior to Phi 2 Rising edge, the data
//  bus is in float following precharge, so /RC falling prior to Phi 2 rising should also
//  be ok. (maybe no more than 100 ns before)
//
//
//  12/17/2020  Timing analysis, from a single instance of EBTKS board
//  12/18/2020  Ongoing measurements. Completed top level, and /RC related adjustment in
//              delay prior to mid-cycle processing
//  12/28/2020  Testing finished.
//
//  Note: There is the overhead of the SET/CLEAR_SCOPE_1 , SET/CLEAR_SCOPE_2. The pair is about 10 ns
//
//  Test program during measurements is BOARD-T (12/17/2020 version)
//
//  Time point == TP  R == Rising  F == Falling          Duration/Delay   Scope     Description
//                                                       Min ns  Max ns   Pic #     
//  From Phi 1 rising to first instruction (SET_SCOPE_1)   52     73      001,002   Interrupt response time
//  From Phi 1 rising to TP C (TP B suppressed)            76    102      003,004   Interrupt to start of processing
//  From Phi 1 rising to TP C (TP A,B suppressed)          68     90      005,006   Interrupt to start of processing
//  Delta of above 2 lines                                  8     12       -   -    Overhead for the SET_SCOPE_1 at TP A
//  TP A to TP B                                           25     25      007       Time to clear interrupts
//  TP C to TP D   onPhi_1_Rise()                          72    187      008,009   Time to process Phi 1 Rising edge
//  onPhi_1_Rise() execution time max-min                 115             010       Variability in processing time of Phi 1 Rising edge
//  Worst case Phi 1 R processing beyond Phi 1 F                  78      011       Can't depend on seeing Phi 1 F, May be overestimated
//  TP E to TP F   onPhi_1_Fall()                          75    150      012,013   Time to process Phi 1 Falling edge
//  onPhi_1_Rise() execution time max-min                  75             014       Variability in processing time of Phi 1 Falling edge
//  TP G to TP H   EBTKS_delay_ns(200)                    205    212      015,016   Test accuracy of EBTKS_delay_ns()
//  TP I to TP J   mid_cycle_processing()                 206    480      017,018   Time to process around Phi 2 Rising (but not locked to edge)
//  mid_cycle_processing()  execution time max-min        274             019       Variability in processing time of mid_cycle_processing()
//  Falling edge of /RC relative to Phi 2 R               -38    129      020,021   with the delay at TP G set to 200
//  Jitter in assertion time of /RC                       167                       129 + 38
//  Falling edge of /RC relative to Phi 2 R               -13    170      022,023   with the delay at TP G set to 240
//  Jitter in assertion time of /RC                       183                       170 + 13
//  Falling edge of /RC relative to Phi 2 R                 3    285      024,025   with the delay at TP G set to 260 some outliers not seen above
//  Jitter in assertion time of /RC                       282                       285 - 3  .  Still less than Max spec of 380
//                                                                                  
//  Calculated ISR, TP A to TP J MIN                      643                       A..B + C..D + E..F + G..H(260) + I..J
//                                                                                  25     72     75     265         206
//  Measured ISR, TP A to TP J   MIN                      748             026       
//                                                                                  
//  Calculated ISR, TP A to TP J MAX                            1107                A..B + C..D + E..F + G..H(260) + I..J
//                                                                                  25     187    150    265         480
//  Measured ISR, TP A to TP J   MAX                            1060      027       Actually 1190 if Tape drive emulation is used
//
//  Decided that for best safety margin, without getting too aggressive, I
//  would adjust the TP G to TP H delay to make the earliest /RC (as seen
//  on the bus) to be 50 ns before the rising edge of Phi 2. With delay of
//  200 ns, -38 was seen, but that was with SET_SCOPE_1 and CLEAR_SCOPE_1 around
//  it. So we will have to hunt a bit for the right value with SCOPE_1 stuff
//  only at A and J, and direct measurement of /RC so try 222, and verify.
//  -54 and +162 looks pretty good.
//                                  Update/Sigh: So it turns out that none     #### Tape Drive
//  of the testing exercised the Tape Drive emulation (remember that, the      #### Tape Drive
//  original goal of EBTKS). The worst case delay for the start of /RC for     #### Tape Drive
//  reads of the tape drive status register, readTapeStatus() , is 144 ns      #### Tape Drive
//  worse than anything else seen, with a latest falling edge of /RC being     #### Tape Drive
//  306 ns after the rising edge of Phi 2. Still within the requirement of     #### Tape Drive
//  "less than 380 ns", but with not as much safety margin as everything       #### Tape Drive
//  else. For now we will just leave it, since it works. If it fails at        #### Tape Drive
//  some later date, the most likely solution is to pipeline some of the       #### Tape Drive
//  calculations in readTapeStatus(), and do them at the end of processing     #### Tape Drive
//  in onPhi_1_Fall(), and adjust the following delay amount to compensate.    #### Tape Drive
//  
//  Falling edge of /RC relative to Phi 2 R               -54    162      028,029   With the delay at TP G set to 222  (does not include #### Tape Drive)
//  Jitter in assertion time of /RC                       216             030       162 + 54                           (does not include #### Tape Drive)
//
//  The above did not involve tape emulation, which takes longer               #### Tape Drive
//
//  Falling edge of /RC relative to Phi 2 R, Tape RD             306          080   With the delay at TP G set to 222
//                                                                                  Still under requirement of max 380 from 1MB5 spec
//
//  Although not seen in the 028, 029,030 measurement, the MAX measurement
//  from 025 (rare outlier) can be applied to this If we assume the spread
//  is 285 rather than 216, and using the tuned earliest time of -54, then
//  the unseen latest would be 285 - 54 = 231 ns after the rising edge of
//  Phi 2. The goal is to maximize the safety margin of the spec of the
//  latest rising edge being 380 ns after Phi 2 R.
//  Thus we have 380 - 231 = 149 ns margin                                      (does not include #### Tape Drive results. )
//
//  Virtual latest /RC falling @ 285 ns after earliest,
//  and 380 after Phi 2 R:                                       148      031
//
//  REMEMBER: All these measurements have an overhead of about
//            10 ns due to the SET_SCOPE_2 & CLEAR_SCOPE_2 . The way
//            the measurements were made, this 10 ns overhead
//            is non-accumulating (i.e. only 1 set of SCOPE_1
//            SET/CLEAR active at a time) . When we are not
//            making these measurements. Then SCOPE_1 is used to
//            track the overall time for pinChange_isr()
//            All of these numbers are with BOARD-T running
//
//  Time point == TP  R == Rising  F == Falling          Duration/Delay   Scope     Description
//                                                       Min ns  Max ns   Pic #
//  All A? to A? are part of onPhi_1_Rise()
//  TP AA to TP AB    Get data from the bus                20     20      032
//  TP AB to TP AC    Assert INTPRI (cond)                 10     20      033       Always 20 ns for false  path. Hard to explain.
//  TP AC to TP AD    Calculate Logic Analyzer sample      33     53      034,035   33 ns while idle, 53 occured during "la setup"
//                                                                                  when it printed the setup values after RSELEC setup
//                                                                                  I'm guessing this is L1 I/D cache related
//  TP AD to TP AE    If Logic Analyzer is running         10             036       This is when LA is not acquiring
//                      Check for trigger             }-   72             037       Acquiring, trigger not matched
//                      Store a sample                }
//                      Update buffer address         }
//                      Check for acquisition complete}
//  TP AE to TP AF    Writes to I/O, RAM, ROM Window       10    118      038       Remember, all of these numbers are with BOARD-T running
//
//  All B? to B? are part of onPhi_1_Fall()
//  TP BA to TP BB    Get data from the bus                20     20      039       Again, same data being read into a local variable
//  TP BB to TP BC    End bus read (we drive data)         10     32      040
//  TP BC to TP BD    EMC (test with EDISK R/W)            28     68      041,042
//  TP BD to TP BE    Address Load                          8     33      043,044
//  TP BE to TP BF    Address Increment                    10     33      045,046
//  TP BF to TP BG    Address Copy                          4             047
//
//  TP G  to TP H     EBTKS_delay_ns(222)                 227    236      048,049   Re-do of previous measurement with tweaked value
//
//  All C? to C? are part of mid_cycle_processing()
//  TP CA to TP CB    Get bus control signals     }--      61     88      050,051
//                    Identify Reads and Writes   }
//                    Identify DMA and Interrupt  }
//                    Acknowledge                 }
//  TP CB to TP CC    EMC mid-cycle processing             38     82      052,053
//  TP CC to TP CD    Schedule Addr inc and load           22     79      054,055
//  TP CD to TP CE    Read. Is it an EBTKS Resource         8    169      056,057   See below for TP D? measurements that break this down into specific tasks
//  TP CE to TP CF    Process Interrupt state              22     57      058,059
//  TP CF to TP CG    Handle Reads (EBTKS drives bus)       5     55      060,061   Data is put on the bus, /RC asserted
//  TP CG to TP CH    Logic Analyzer capture sample        55             062
//  TP CH to TP CI    Process DMA Request                   5     18      063,064
//  TP CI to TP CJ    Process DMA, no DMA Ack              11     20      065,066
//  TP CI to TP CJ    Process DMA, when DMA ACK                                     It's complicated, but it is entering DMA so no real impact on
//                                                                                  the normal critical timing that we are researching. Here is the breakdown:
//                                                       1120             067       TP CI to TP CJ
//                                                         57             068       TP CI to NVIC_CLEAR_PENDING()
//                                                          7             069       NVIC_CLEAR_PENDING() duration
//                                                        992             070       Wait till end of next Phi 1 H, then release /LMA, /RD, /WR
//                                                         28             071       Change direction of /LMA, /RD, /WR, switch control direction, flag DMA Active
//                                                         14             072       __disable_irq() and get to TP CJ
//
//
//  onReadData()  (TP CD to TP CE  , 8 to 169 ns) is further analyzed
//
//  Time point == TP  R == Rising  F == Falling          Duration/Delay   Scope     Description
//                                                       Min ns  Max ns   Pic #
//  TP DA to TP DB    Read ROM                              7     87     073,074    7 ns for Addr non-match, 87 for tests to get to match
//                                                                                  includes RAM Window accesses
//  TP DC to TP DD    Read 16 KB RAM on 85A/B              16     41     075,076    Not sure this exercised the 16 KB RAM               
//  TP DE to TP DF    I/O Read functions, non-tape         10     96     077,078    Apparently Tape I/O takes longer. See next line
//  TP DE to TP DF    I/O Read for tape                          234     079        Tape Read I/O. This is a surprise. Need to re-check /RC
//
//  The heart of EBTKS is this Pin Change Interrupt Service Routine (ISR).
//  ALL of the following tasks are processed while the processor is
//  running in interrupt context, and our interrupt level is the highest
//  level possible (maybe there is an unhandled overtemperature interrupt
//  that is higher). As such, this sequence of task runs in sequence, and
//  should not be interrupted. The total time to do all tasks must take no
//  more than approximately 1000 ns. Note that while the list is very
//  long, many of the tasks are bracketed by a conditional, which means
//  only the test is performed and the bracketed code is skiped.
//
//
//  Tasks are performed in the following order:
//    Start onPhi_1_Rise() processing
//      Get data from the bus
//      Assert INTPRI if we have asserted /IRL
//      Calculate Logic Analyzer sample
//      If Logic Analyzer is running
//        Check for trigger
//        Store a sample
//        Update buffer address
//        Check for acquisition complete
//      Process writes to I/O devices or registers (may require non-trivial processing)
//      Process writes to 16 KB RAM on HP85A if enabled and address match
//      Process writes to special RAM window in the AUXROM(s)
//      If Phi 1 is still high, wait
//
//    Start onPhi_1_Fall() processing
//      Get data from the bus (again, same data being read into a local variable)
//      If the HP85 was reading data from EBTKS, end the read, and de-assert /RC
//      If we are supporting EMC
//        Update cycle count
//        test for IFETCH, and handle it
//      Handle Address Register Loading or incrementing
//      Save the data from the bus in case we need it as the low byte of an address register load
//
//    Delay 222 ns so that /RC happens not too early (no sooner than 50 ns before Phi 2 rising)
//
//    Start mid_cycle_processing()
//      Get bus control signals /LMA, /RD, /WR and figure out what is hapening
//      Identify Reads and Writes
//      Identify DMA and Interrupt Acknowledge
//      If EMC is supported, get IFETCH state
//      Schedule Address Increment and Load
//      If Read Cycle, see if it is an EBTKS Resource
//        Is it a bank switched ROM address
//          Is it an AUXROM && RAM Window address
//            Read from RAM Window
//        Is it a ROM that EBTKS is providing? If so, read the selected ROM
//        Process Reads from 16 KB RAM on HP85A if enabled and address match
//        Process any I/O space Reads (may require non-trivial processing)
//      Process Interrupt state (HP85 interrupts)
//      If any resource is being read from EBTKS
//        While avoiding contention, turn the data bus around and drive
//        the backplane, put our data on the bus
//        Assert /RC
//      Save the Bus Cycle information to be used by the Logic Analyzer
//      Check for a request for DMA transactions from anywhere else in the EBTKS firmware
//        Assert /HALT
//        record that the request is pending
//      If DMA_Acknowledge && we requested it
//        Take ownership of the bus by
//          Stop processing interrupts for Phi 1 Rising edge
//          Wait for Phi 1 Falling edge (even waste a cycle, just to be sure, but should not ned to)
//          Take control of the /LMA, /RD, and /WR , while avoiding contention
//          Start driving the control lines
//        Indicate to the rest of EBTKS Firmware that we are now in HP85 DMA mode
//        Disable all EBTKS interrupts. So no USB Serial, no SYSTICK
//      That's all Folks
//
//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
//  The declaration for this function (in EBTKS_Function_Declarations.h)
//  must explicitly indicate that this is an ISR. It looks like this
//    FASTRUN void pinChange_isr(void) __attribute__ ((interrupt ("IRQ")));
//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

FASTRUN void pinChange_isr(void)      //  This function is an ISR, keep it short and fast.
{

  uint32_t interrupts;

  SET_SCOPE_1;        //  Time point A
  interrupts = PHI_1_and_2_ISR;         //  This is a GPIO ISR Register
  PHI_1_and_2_ISR = interrupts;         //  This clears the interrupts
  //CLEAR_SCOPE_1;    //  Time point B

  //SET_SCOPE_2;      //  Time point C
  onPhi_1_Rise();
  //CLEAR_SCOPE_2;    //  Time point D
  WAIT_WHILE_PHI_1_HIGH;          //  While Phi_1 is high, just hang around, not worth doing a return from interrupt
                                  //  and then having an interrupt on the falling edge.
                                  //  Note that if onPhi_1_Rise() takes more than 200 ns minus the time it took
                                  //  to get to the call to onPhi_1_Rise(), the call to onPhi_1_Fall() will occur
                                  //  after the edge by that excess time. This does occur.

  //SET_SCOPE_1;      //  Time point E
  onPhi_1_Fall();
  pin_isr_count++;    //  Used to detect that HP85 power is off
  //CLEAR_SCOPE_1;    //  Time point F

  //SET_SCOPE_2;      //  Time point G
  EBTKS_delay_ns(222);
  //CLEAR_SCOPE_2;    //  Time point H

  //SET_SCOPE_1;      //  Time point I
  mid_cycle_processing();
  CLEAR_SCOPE_1;      //  Time point J

}

//
//  Data is valid during the Phi 1, possible drivers are:
//      Capricorn
//      ROM
//      RAM
//      I/O devices
//      EBTKS
//
//  Depending on the source, the data became valid as early as the end of Phi 2 and as late at the end of Phi 21
//  Actual measurements show data valid prior to the rising edge of Phi 1 as early as ???
//
//  Regardless of whether EBTKS or any other device/ROM/RAM/CPU is either reading
//  or writing to the data bus, by the time we get the Phi 1 rising edge, the data
//  has been valid for at least 150 ns.
//
//  Tasks are performed in the following order:
//      Get data from the bus
//      Assert INTPRI if we have asserted /IRL
//      Calculate Logic Analyzer sample
//      If Logic Analyzer is running
//        Check for trigger
//        Store a sample
//        Update buffer address
//        Check for acquisition complete
//      Process writes to I/O devices or registers
//      Process writes to 16 KB RAM on HP85A if enabled and address match
//      Process writes to special RAM window in the AUXROM(s)
//      If Phi 1 is still high, wait
//

inline void onPhi_1_Rise(void)                  //  This function is running within an ISR, keep it short and fast.             This needs to be timed.
{
  uint32_t data_from_IO_bus;

  //SET_SCOPE_2;        //  Time point AA
  data_from_IO_bus = (GPIO_PAD_STATUS_REG_DB0 >> BIT_POSITION_DB0) & 0x000000FFU;       //  Requires that data bits are contiguous and in the right order
  //CLEAR_SCOPE_2;      //  Time point AB

  //SET_SCOPE_2;        //  Time point AB
  if (intrState)
    {
      ASSERT_INTPRI;                            //  If we have asserted /IRL, assert our priority in the chain
    }
  //CLEAR_SCOPE_2;      //  Time point AC

//
//  Read the local data bus. Regardless of whether we are idle, providing data, or receiving data,
//  the Pad Status Register should have the current data bus for this cycle
//
//  The Logic Analyzer is overhead in every bus cycle, so the code must be extremely minimal
//
//  This code depends on the following being true, found in EBTKS.h  (all 3 control signals are consecutive bits in the same GPIO register, in these bit positions)
//      #define BIT_POSITION_WR           (26)
//      #define BIT_POSITION_LMA          (24)
//      #define BIT_POSITION_RD           (25)
//
//  Logic_Analyzer_main_sample layout is:
//  Bits          Content
//  31            DMA Cycle (may be combined with /WRX, /RDX, /LMA)                                                         Bit      Group
//  30            Instruction Fetch Flag                                                            IF   CORE_PIN32_BIT     12       GPIO7       IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_12
//  29            /IRLX    Interrupt Request Signal from Bus (Might be us or some other module)     T04  CORE_PIN4_BIT       6       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_06
//  28            /HALTX   DMA Request Signal from Bus       (Might be us or some other module)     T05  CORE_PIN5_BIT       8       GPIO9       IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_08
//  27            currently unused
//  26            /WRX    recorded at the prior Phi 2
//  25            /RDX    recorded at the prior Phi 2
//  24            /LMA    recorded at the prior Phi 2
//  23 ..  8      Current Address
//   7 ..  0      Data Bus
//
//  Logic_Analyzer_aux_sample layout is:
//  Bits          Content
//  31 ..  8      Currently always 0000 0000 0000 0000 0000 0000
//   7 ..  0      RSELEC

//
//  We use Logic_Analyzer_main_sample in the Logic analyzer, and also anywhere where we want to watch out for specific addresses occuring
//  such as recognizing that the HP85 is in the IDLE state
//

  //SET_SCOPE_2;        //  Time point AC
  Logic_Analyzer_main_sample =  data_from_IO_bus |                            //  See above for constraints for this to work.
                                (addReg << 8)    |                            //  because we are very fast, it is ok to take
                                Logic_Analyzer_current_bus_cycle_state_LA;    //  data on Phi 1 Rising edge, unlike HP-85 that    Logic_Analyzer_current_bus_cycle_state_LA is setup in the Phi 2 code
                                                                              //  uses the falling edge.
  Logic_Analyzer_aux_sample  =  getRselec() & 0x000000FF;                     //  Get the Bank switched ROM select code
  //CLEAR_SCOPE_2;      //  Time point AD

  //SET_SCOPE_2;        //  Time point AD
  if (Logic_Analyzer_State == ANALYZER_ACQUIRING)
  {
    //
    //  Check for trigger pattern
    //      Don't look for trigger if the pre-trigger number of samples has not yet occured
    //      Don't look for trigger if we have already triggered
    //  Once triggered, count down till we have collected al post-trigger samples
    //

    if (!Logic_Analyzer_Triggered)
    {
      if (Logic_Analyzer_Valid_Samples >= Logic_Analyzer_Pre_Trigger_Samples)
      { //  Triggering is allowed

        //  Enable this to toggle SCOPE_2 on a reference to HEYEBTKS by the HP85
        // if (((Logic_Analyzer_main_sample) & 0x00FFFF00) == (HEYEBTKS << 8))
        // {
        //   TOGGLE_SCOPE_2;
        // }

        if ( ((Logic_Analyzer_main_sample & Logic_Analyzer_Trigger_Mask_1) == Logic_Analyzer_Trigger_Value_1) &&
             ((Logic_Analyzer_aux_sample  & Logic_Analyzer_Trigger_Mask_2) == Logic_Analyzer_Trigger_Value_2)    )
        {   //  Trigger pattern has been matched
          if (--Logic_Analyzer_Event_Count <= 0)                            //  Event Count is how many match events needed to trigger
          {
            Logic_Analyzer_Triggered = true;
            Logic_Analyzer_Index_of_Trigger = Logic_Analyzer_Data_index;    //  Record the buffer index at time of trigger
            //  SET_SCOPE_2;                                                    //  Trigger an external logic analyzer
          }
        }
      }
    }
    //
    //  Store a Logic Analyzer sample into the sample buffer
    //
    Logic_Analyzer_Data_1[Logic_Analyzer_Data_index  ] = Logic_Analyzer_main_sample;
    Logic_Analyzer_Data_2[Logic_Analyzer_Data_index++] = Logic_Analyzer_aux_sample;
    //
    //  Modulo addressing of sample buffer. Requires buffer length to be a power of two
    //
    Logic_Analyzer_Data_index &= Logic_Analyzer_Current_Index_Mask;
    Logic_Analyzer_Valid_Samples++;                                         //  This could theoretically over flow if we didn't see a trigger in 7000 seconds (1.9 hours). Saturating is not worth the overhead
    if (Logic_Analyzer_Triggered)
    {
      if (--Logic_Analyzer_Samples_Till_Done == 0)
      {
        Logic_Analyzer_State = ANALYZER_ACQUISITION_DONE;
        //  CLEAR_SCOPE_2;                                                    //  End of Trigger of an external logic analyzer
      }
    }
  }
  //CLEAR_SCOPE_2;      //  Time point AE

//
//  If this is a Write cycle to EBTKS, this is where it is handled
//

  //SET_SCOPE_2;        //  Time point AE
  if (schedule_write)
  {
    onWriteData(addReg, data_from_IO_bus);                      //  Need to document max time
  }
  //CLEAR_SCOPE_2;      //  Time point AF
}

//
//  Handle Capricorn driving I/O bus, of either Address Bytes (/LMA) or Data Bytes (/RD)
//
//  This function is running within an ISR, keep it short and fast.
//  Depending on the execution time of onPhi_1_Rise(), this may
//  start later than the name implies (with jitter)
//
//  This function has been timed (see commented SET_SCOPE_2/CLEAR_SCOPE_2) and it takes from ?? ns to ?? ns to execute    ##########  timing neeeds to be redone
//  The earliest start of this function is ?? ns after the falling edge of Phi 1
//  The latest start is ?? ns after the falling edge of Phi 1
//  The earliest end of this function is ?? ns after the falling edge of Phi 1
//  The latest end is ??? ns after the falling edge of Phi 1
//  Thus ?? ns margin until precharge during Phi 12
//

inline void onPhi_1_Fall(void)
{
  uint32_t data_from_IO_bus;

  //  ### this is duplicate code to onPhi_1_Rise() , using local variables. Might be more efficient due to local variable in a reg vs a shared static var.
  //SET_SCOPE_2;      //  Time point BA
  data_from_IO_bus = (GPIO_PAD_STATUS_REG_DB0 >> BIT_POSITION_DB0) & 0x000000FFU;       //  Requires that data bits are contiguous and in the right order
  //CLEAR_SCOPE_2;      //  Time point BB

  //SET_SCOPE_2;      //  Time point BB
  if (HP85_Read_Us)                     //  If a processor read was getting data from this board, we just finished doing it. This was set in mid_cycle_processing()
  {
                                        //  According to the 1MB5 specification, data should be held for a minimum of 40 ns
                                        //  after Phi 1 fall. It looks like the Phi 1 rising code takes long enough that we
                                        //  don't need to add any additional delay to meet this minimum. i.e. Phi 1 Rising processing takes more than 240 ns
                                        //  #####  need to check this when we have stopped making changes to this code
    SET_T4_BUS_TO_INPUT;                //  Set data bus to input on Teensy
    BUS_DIR_FROM_HP;                    //  Change direction of bus buffer/level translator to inbound from I/O bus
                                        //  This also de-asserts /RC  (it goes High)
    //CLEAR_SCOPE_2;                          //  Use this to track /RC timing
    HP85_Read_Us = false;               //  Doneski
  }
  //CLEAR_SCOPE_2;      //  Time point BC

#if ENABLE_EMC_SUPPORT
  //SET_SCOPE_2;      //  Time point BC
  if (schedule_read || schedule_write)
  {
    cycleNdx++;                         //  Keep track of the bus cycle count
  }
  //
  //  Capture DRP instructions for the extended memory controller
  //  and if the instruction is a multiple byte transfer
  //
  if (ifetch == true)
  {
    if ((data_from_IO_bus & 0xc0u) == 0x40u)    //  DRP instruction 0x40..0x7f?
    {
      m_emc_drp = data_from_IO_bus;
    }
    m_emc_mult = data_from_IO_bus & 1u;         //  Set if multiple byte instruction
  }
  //CLEAR_SCOPE_2;      //  Time point BD
#endif

  //SET_SCOPE_2;      //  Time point BD
  if (schedule_address_load)            //  Load the address registers. Current data_from_IO_bus is the second byte of
                                        //  the address, top 8 bits. Already got the low 8 bits in pending_address_low_byte
  {
    addReg  = data_from_IO_bus << 8;    //  Rely on zero fill for bottom 8 bits
    addReg |= pending_address_low_byte; //  Should always be only bottom 8 bits
    //CLEAR_SCOPE_2;      //  Time point BE
    return;                             //  Early exit.
  }
  //CLEAR_SCOPE_2;      //  Time point BE

  //SET_SCOPE_2;      //  Time point BE
  if (schedule_address_increment)       //  Increment address register. Happens for Read and Write cycles, unless address is being loaded, or address register is I/O space
  {
    addReg++;
//address_register_has_changed:
                                        //  Comming soon: pipelined processing of the addReg, so that all the time consuming stuff happens here
                                        //  rather than in the time critical processing of Phi 2
                                        //  PMF to RB 12/03/2020 at 2:08 am
                                        //    "You know, if we started the Phi 2 processing on the falling edge of Phi 12, a lot of our timing issues would disappear. "
  }
  //CLEAR_SCOPE_2;      //  Time point BF

  //SET_SCOPE_2;      //  Time point BF
  pending_address_low_byte = data_from_IO_bus;    // Load this every cycle, in case we need it, avoiding an if ()
  //CLEAR_SCOPE_2;      //  Time point BG

}

//
//  Read data/buffer enable/RC active on Phi 2 rise and finish on Phi 1 fall
//  LMA/RD/WR latched on Phi 2 rise
//
//  Total re-write 7/22/2020
//
//  08/27/2020 Total restore of old code, but with nicer variable names because
//  the old code apparently works better.
//
//  Bus Cycle Type from Patent US4424563 , Fig 13
//
//  This code depends on the following being true, found in above
//    (all 3 control signals are consecutive bits in the same GPIO register, in these bit positions)
//      #define BIT_POSITION_WR           (26)      MSB
//      #define BIT_POSITION_RD           (25)
//      #define BIT_POSITION_LMA          (24)      LSB
//
//  Re-arange the columns of the Fig 13 in the patent to get these values without any bit shuffling
//    /WR   /RD   /LMA      Note all signals are active low in hardware.
//
//
//  11/19/2020    There was for a long time, a belief that timing around Phi 2 was not as critical as Phi 1.
//                Over a week of bug tracking indicates that this is not true.
//

inline void mid_cycle_processing(void)                             //  This function is running within an ISR, keep it short and fast.
{
  bool       lma;
  bool       rd;
  bool       wr;
  uint32_t   dataBus;
  uint32_t   bus_cycle_info;                              //  Bits 26 down to 24 will be /WR , /RD , /24 in that order

//
//  This is Russell's code with some minor edits.
//
  //SET_SCOPE_2;        //  Time point CA
  bus_cycle_info = GPIO_PAD_STATUS_REG_LMA;                   //  All 3 control bits are in the same GPIO register
  lma = !(bus_cycle_info & BIT_MASK_LMA);                     //  Invert the bits so they are active high
  rd  = !(bus_cycle_info & BIT_MASK_RD );
  wr  = !(bus_cycle_info & BIT_MASK_WR );
  schedule_read  = rd && !wr;                                 //  Resolve control logic states
  schedule_write = wr && !rd;
  DMA_Acknowledge = wr && rd && !lma;                         //  Decode DMA acknowlege state
  Interrupt_Acknowledge = wr && rd && lma;                    //  Decode interrupt acknowlege state
  //CLEAR_SCOPE_2;      //  Time point CB

#if ENABLE_EMC_SUPPORT
  //SET_SCOPE_2;        //  Time point CB
  ifetch = !(GPIO_PAD_STATUS_REG_IFETCH & BIT_MASK_IFETCH);   //  Grab the instruction fetch bit. Only exists on HP85B, 86 and 87
                                                              //  Used for tracking instructions for the extended memory controller
  //if ((rd != schedule_read) || (wr != schedule_write))      //  Reset on any change of read or write
  if (lma != delayed_lma)                                     //  Reset count on change of lma
  {
    cycleNdx = 0;
  }
  if (lma == true)
  {
    lma_cycle(rd);    //for emc support
    cycleNdx &= 1;    //ensure cycleNdx is only 0 or 1 in an lma cycle
  }
  else
  {
    m_emc_lmard = false;
  }
  //CLEAR_SCOPE_2;      //  Time point CC
#endif

  //SET_SCOPE_2;        //  Time point CC
  schedule_address_increment = ((addReg >> 8) != 0xff) &&
                                !(!schedule_address_load & delayed_lma) &&
                                (schedule_read | schedule_write);         //  Only increment addr on a non i/o address
  schedule_address_load = !schedule_address_load && delayed_lma;          //  Load address reg flag
  delayed_lma = lma;                                                      //  Delayed lma
  //CLEAR_SCOPE_2;      //  Time point CD

  //SET_SCOPE_2;        //  Time point CD
  if (schedule_read)
  {           //  Test if address is in our range and if it is , return true and set readData to the data to be sent to the bus
    HP85_Read_Us = onReadData();
    //CLEAR_SCOPE_2;    //  Time point DB
    //CLEAR_SCOPE_2;    //  Time point DF
  }
  //CLEAR_SCOPE_2;      //  Time point CE

//  if (HP85_Read_Us && just_once)
//  {
//    SET_SCOPE_2;
//    Serial.printf("The address register is %06o\n", addReg);
//    just_once = 0;
//  }
//
//  NOT Tested yet
//

  //SET_SCOPE_2;        //  Time point CE
  switch(intrState)
  {
    case 0:                                         //  Test for a request or see if there is an intack
      if ((Interrupt_Acknowledge) && (!IS_IPRIH_IN_LO))
      {
        intEn_1MB5 = false;                         //  Lower priority device had interrupted, disable our ints until a write to 177500
      }
      else if ((interruptReq == true) && (globalIntEn == true) && (intEn_1MB5 == true))
      {
        intrState = 1;                              //  Interrupt was requested
        ASSERT_INT;                                 //  IRL low synchronous to phi2 rise - max 500ns to fall
      }
      break;

    case 1:                                         // IRL is low
      if ((Interrupt_Acknowledge) && (!IS_IPRIH_IN_LO))     //  We're interrupting - so it must be our turn
      {
        RELEASE_INT;
        RELEASE_INTPRI;
        globalIntAck = true;                        //  Set when we've been the interrupter
        readData = interruptVector;
        HP85_Read_Us = true;
        interruptReq = false;                       //  Clear request
        intrState = 0;
      }
      if ((Interrupt_Acknowledge) && (IS_IPRIH_IN_LO)) //higher priority request beat us
      {
        RELEASE_INT;                                //  Let go of /IRL as we lost
        RELEASE_INTPRI;
        intrState = 0;                              //  Try for another request
      }
      break;
  }
  //CLEAR_SCOPE_2;      //  Time point CF

  //
  //  For a Read Cycle (I/O bus to CPU on main board), this is where we turn the data bus around
  //  and place the data on the bus. We also assert BUS_DIR_TO_HP which changes the direction of
  //  the 8 bit bus level translator. This signal also asserts (drives low) the /RC signal that
  //  tells the 1MA8 that the data direction is from the I/O bus to the CPU.
  //  Timing is not too critical here, as we are about 800 ns before the rising edge of Phi 1
  //  which is when the data is read. Actually latched by Phi 1, so another 200 ns margin.
  //

  //SET_SCOPE_2;        //  Time point CF
  if (HP85_Read_Us)
  {
    //
    //  This is diagnostic code to see if 1MA8 has let go of bus by the time we get here.
    //  for 100 ns we look at the I/O data bus with an oscilloscope and see what it looks
    //  like in the normal state, and with a few different resitive loads to see if it is actively
    //  driven. Results:
    //          1MA8 drives the I/O bus data pins for /LMA about 375 ns before Phi 1 Rising edge
    //          1MA8 for all bus cycles precharges the I/O bus during Phi 12
    //          The actual address coming from 1MB1 is preceded by precharge for the first 256 ns
    //          of the above 375 ns, then it drive the actual address, starting 120 ns before Phi 1
    //          rising, and continuing for 400 ns, and ending at 80 ns after Phi 1 falling edge (i.e. 80 ns hold)
    //
    //  SET_SCOPE_2;
    //  EBTKS_delay_ns(100);              //  This is used as a trigger, and delays us driving the bus for 100 ns,
    //  CLEAR_SCOPE_2;                        //  which we believe is a "don't care" due to long setup time
    //

    //  Set bus to output data

    DISABLE_BUS_BUFFER_U2;
    BUS_DIR_TO_HP;                        //  DIR high  !!! may need to delay for 1MA8 to let go of driving bus: See above diagnostic test. Confirmed
                                          //  that starting to drive the data bus (with /RC) at Phi 2 is not going to cause contention
    //SET_SCOPE_2;                            //  Use this to track /RC timing

    SET_T4_BUS_TO_OUTPUT;                 //  set data bus to output      (should be no race condition, since local)

    dataBus  = (uint32_t)(readData & 0x000000FFU) << 16;  // Set up data to be written to the appropriate bits of GPIO_DR_SET_DB0

    //
    //  Use clear/set regs to output the bits we want without touching any others - not sure if this is the fastest method
    //

    GPIO_DR_CLEAR_DB0 = DATA_BUS_MASK;    //  Clear all 8 data bits
    GPIO_DR_SET_DB0   = dataBus;          //  Put data on the bus. This is the central point for ALL I/O and Memory reads that this board supports

    ENABLE_BUS_BUFFER_U2; // !OE low.
  }
  //CLEAR_SCOPE_2;      //  Time point CG

  //SET_SCOPE_2;        //  Time point CG
  Logic_Analyzer_current_bus_cycle_state_LA = bus_cycle_info  &           //  Yes, this is supposed to be a single &
                                              (BIT_MASK_WR | BIT_MASK_RD | BIT_MASK_LMA);      //  Require bus cycle state to be in bits 24, 25, and 26 of GPIO register

  Logic_Analyzer_current_bus_cycle_state_LA |=  ((GPIO_PAD_STATUS_REG_T04    & BIT_MASK_T04)    << (29 - BIT_POSITION_T04   )) |    //  T04 is monitoring bus /IRLX
                                                ((GPIO_PAD_STATUS_REG_T05    & BIT_MASK_T05)    << (28 - BIT_POSITION_T05   )) |    //  T05 is monitoring bus /HALTX
                                                ((GPIO_PAD_STATUS_REG_IFETCH & BIT_MASK_IFETCH) << (30 - BIT_POSITION_IFETCH)) ;
  //CLEAR_SCOPE_2;      //  Time point CH

  //
  //  ALL DMA requests are handled here
  //  DMA is asserted 200 ns after the falling edge of Phi 2      ####  Need to check the timing here. 1MB5 docs say it must be asserted no later than
  //                                                                    500 ns after the Rising edge of Phi2. (implies at least 300 ns before Phi 1 rise)
  //

  //SET_SCOPE_2;        //  Time point CH
  if (DMA_Request)
  {
    //WAIT_WHILE_PHI_2_HIGH;              //  While Phi_2 is high, just hang around
    //delayNanoseconds(130);              //  There is 70 ns overhead, so this puts the earliest version at 200 ns after falling Phi 2
                                          //  The latest is about 300 ns , so 100 ns jitter, probably due to arrival at the above wait while Phi 2 is high.
    ASSERT_HALT;

    DMA_Request = false;                  //  Only request once
    DMA_has_been_Requested = true;        //  Record that a request has occured on the bus, but has not yet been acknowledged
  }
  //CLEAR_SCOPE_2;      //  Time point CI

//
//  ##### In the following section, the are multiple references to interrupts coming from both Phi 1 and Phi 2 rising edges.
//        As of 12/8/2020 it is only Phi 1. Old comments are retained for now in case we need to go back to the old scheme
//
  //SET_SCOPE_2;        //  Time point CI
  if (DMA_Acknowledge && DMA_has_been_Requested)
  {
    DMA_has_been_Requested = false;       //  Bus DMA request is no longer pending
    //
    //  Although we are in an interrupt handler, we are about to turn interrupts off
    //  (for the Phi 1 and 2 pin interrupts), so we can spend extra time here to get
    //  everything done. When we exit from this handler, it will be an ISR exit
    //  (actually one up in the call stack)
    //
    NVIC_DISABLE_IRQ(IRQ_GPIO6789);       //  Turn off Phi 1 and Phi 2 interrupts. We are on our own for now.
                                          //  Need to wait for Phi 1 rising before we drive the control lines high
    PHI_1_and_2_IMR = 0;                  //  Block any interrupts while DMA is happening. This assumes that the ONLY
                                          //  interrupts that can happen in this GPIO group is Phi 1 and Phi 2 rising edge
    PHI_1_and_2_ISR = PHI_1_and_2_ISR;    //  Clear any set bits (should be none)
    //TOGGLE_SCOPE_2;
    NVIC_CLEAR_PENDING(IRQ_GPIO6789);     //  Even though we just masked GPIO interrupts, the NVIC can still
                                          //  have pending interrupts. It shouldn't at this point, but clear
                                          //  them, just to be safe. We will need to do this again, just
                                          //  before ending the DMA activity.
    //TOGGLE_SCOPE_2;
    //
    //  Don't rush to get started, let the Acknowledge cycle finish.
    //  The Acknowledge starts during Phi 1, but here we are in the
    //  Phi 2 rising code, so we KNOW that Phi 1 is low. Since this
    //  is where we go into DMA mode, it is ok for us to stay in
    //  this code well beyond normal ISR duration. Also we have
    //  already turned off the Phi 1 and 2 rising edge interrupts.
    //
    WAIT_WHILE_PHI_1_LOW;   //  By the time we get to Phi 1 rising edge (end of this wait), the control lines
                            //  will have gone high, and will be in Precharge state
    WAIT_WHILE_PHI_1_HIGH;  //  We are going to just waste the cycle immediately after DMA Grant, because it is
                            //  too tricky to jump into the first cycle of the DMA (Address Low Byte)
                            //  This gives us the opportunity to exit this code knowing that Phi 1
                            //  has just gone Low, and we have oodles of time to set up for the DMA LMAs (about 800 ns)
                            //
                            //  LMA, WR, and RD, should be 1, 1, 1
    RELEASE_LMA;            //  get them in the right state before turning the Teensy pins  around
    RELEASE_RD;
    RELEASE_WR;
    //TOGGLE_SCOPE_2;
    GPIO_DIRECTION_LMA |= (BIT_MASK_LMA | BIT_MASK_RD | BIT_MASK_WR);     //  Switch LMA, RD, and WR to Output
                                                                          //  Assumes/requires that LMA, RD, and WR
                                                                          //  are in the same GPIO group.
                                                                          //  They are, GPIO6
    //
    //  At this point the control buffer is still driving from the bus into the board,
    //  we have just started driving these signals, which should not cause contention,
    //  because the values should be the same. Anyway, we will now change the control
    //  buffer direction, and we will drive LMA, RD, and WR high. Should be contention free.
    //
    CTRL_DIR_TO_HP;

    //
    //  Note:  The I/O Bus is precharged by 1MA8 during DMA Acknowledge.
    //
    //  The time is now some nanoseconds after Phi 1 Falling.
    //  The 3 control lines are driving the I/O bus, and each is set High
    //  The 8 Data bits are driving te I/O bus with 0xFF
    //

// #### Need to deal with data direction, and initially undriven because we don't know what
//      1MA8 does with bus during DMA. It may still be driving and may need RC to flip it
//      around. So maybe can't put addresses onto bus without RC
//      This may also be a requirement to precharge the bus on the main board, via the 1MA8.

    DMA_Acknowledge = false;
    DMA_Active = true;
    //TOGGLE_SCOPE_2;
  //
  //  We now own the bus, the 3 control lines are high, HALT is still asserted, and interrupts are off, we are driving
  //  the data bus with 0xFF
  //
  //  Be responsible with this Great Power
  //

  __disable_irq();  //  Disable all interrupts for duration of DMA. No USB activity, no Serial via USB, no SysTick
                    //  We need to do this to get precise timing for /LMA /RD /WR /RC
                    //
                    //  ####  10/30/2020 Maybe we could avoid this, allowing diagnostic Serial.printf("for example")
                    //        by only doing this when we are actually doing 1 or more dma transactions. We have already
                    //        disabled the pin interrupts.

  //
  //  The matching release of DMA is in EBTKS_DMA.cpp at release_DMA_request()
  //

  }
  //CLEAR_SCOPE_2;      //  Time point CJ

  //EBTKS_delay_ns(120);    //  We seem to be exiting so fast that the external logic analyzer is missing this ISR
                            //  Upgraded logic analyzer. No more problem
}

//
//  Bus cycle is a read. See if it one of the addresses we need to respond to:
//    Any ROM that we are emulating
//    Any RAM that we are emulating
//    Any SpecialRAM that we are implementing within the AUXROMs
//    Any I/O registers that we are implementing
//
//  While this is an interrupt routine that is providing data back to the HP-85
//  processor, we entered this routine from the rising edge interrupt for Phi 2
//  The data we are going to put on the bus won't be looked at till Phi 1, which
//  is about 800 ns in the future. So timing to get the data onto the bus is not
//  nanosecond critical timing.
//

inline bool onReadData(void)                  //  This function is running within an ISR, keep it short and fast.
{
  //
  //  If any of these tests indicate that we need to supply data
  //    Put the data in readData
  //    return true
  //  else
  //    return false
  //

  //SET_SCOPE_2;        //  Time point DA
  if ((addReg & 0xE000) == ROM_PAGE) // ROM page 0x6000..0x7FFF   (8 KB)
  {
    //
    //  Knock off some MSBs so we address the ROM from addReg 0
    //  This function knows about the AUXROM RAM Window, and handles these reads as well
    //
    return readBankRom(addReg & (ROM_PAGE_SIZE - 1));
  }
  //CLEAR_SCOPE_2;      //  Time point DB, if test false

  //SET_SCOPE_2;        //  Time point DC
  if (enRam16k)
  {
    //
    //  For HP-85 A, implement 16384 - 256 bytes of RAM, mapped at 0xC000 to 0xFEFF (if enabled)
    //
    if ((addReg >= HP85A_16K_RAM_module_base_addr) && (addReg < IO_ADDR))
    {
      readData = HP85A_16K_RAM_module[addReg & 0x3FFF];
      //CLEAR_SCOPE_2;      //  Time point DD, if test true, true
      return true;
    }
    //CLEAR_SCOPE_2;      //  Time point DD, if test true, false
  }
  //CLEAR_SCOPE_2;      //  Time point DD, if test false

  //
  //  Process I/O reads (data from I/O bus to the CPU)
  //
  //SET_SCOPE_2;        //  Time point DE
  if ((addReg & 0xFF00U) == 0xFF00U)
  {
    return (ioReadFuncs[addReg & 0x00FFU])();  // Call I/O read handler
  }

  //
  //  If we get here, none of the checked address ranges match the current Read Address
  //

  return false;
}

//
//  Bus cycle is a Write. See if it one of the addresses we need to capture the data on the bus:
//
//    Note: By the time we get to this routine, the bus has already been captured.
//
//    Any ROM that we are emulating
//    Any RAM that we are emulating
//    Any Special RAM that we are implementing within the AUXROMs
//    Any I/O registers that we are implementing
//    Any I/O registers that we are tracking
//
//    This code is running within an ISR, during the Phi 1 High time. As always with ISR code, keep it simple and fast
//

inline void onWriteData(uint16_t addr, uint8_t data)
{
  //
  //  Process I/O writes
  //
  if ((addr & 0xFF00U) == 0xFF00U)
  {
    (ioWriteFuncs[addr & 0xFFU])(data);  //  Call an I/O write handler
    return;
  }

  //
  //  For HP-85 A, implement 16384 - 256 bytes of RAM, mapped at 0xC000 to 0xFEFF (if enabled)
  //
  if (enRam16k)
  {
    if ((addr >= HP85A_16K_RAM_module_base_addr) && (addr < IO_ADDR))
    {
      HP85A_16K_RAM_module[addr & 0x3FFFU] = data;
      return;
    }
  }

  //
  //  Handle special RAM window in the AUXROM(s)
  //

  if ((getRselec() >= AUXROM_PRIMARY_ID) && (getRselec() <= AUXROM_SECONDARY_ID_END))      //  Testing for Primary AUXROM and all secondaries
  {
    if ((addr >= (AUXROM_RAM_WINDOW_START + 060000)) && (addr <= (AUXROM_RAM_WINDOW_LAST + 060000)))     //  Write AUXROM shared RAM window
    {
      AUXROM_RAM_Window.as_bytes[addr - AUXROM_RAM_WINDOW_START - 060000] = data;
      return;
    }
  }
}

void setupPinChange(void)
{
  __disable_irq();    //  This code is a critical region.
  //
  //  Let's cheat a little and just use the teensy core code to do most of the work for the setup
  //
  attachInterrupt(CORE_PIN_PHASE1, NULL, RISING);   // PHI 1 is Teensy 4.1 pin 21, Physical pin  43
  //  attachInterrupt(CORE_PIN_PHASE2, NULL, RISING);   // PHI 2 is Teensy 4.1 pin 38, Physical pin  30   //  2020_12_06 only 1 pin change interrupt: Phi 1 rising

  NVIC_DISABLE_IRQ(IRQ_GPIO6789);
  //
  //  And then use our ISR to bypass the longwinded Teensy core one
  //
  attachInterruptVector(IRQ_GPIO6789, &pinChange_isr);
  NVIC_SET_PRIORITY(IRQ_GPIO6789, 16);  //  irqnum, priority. lower num = higher priority
  // NVIC_ENABLE_IRQ(IRQ_GPIO6789);     //  Disabled by PMF 5/25/2020   Don't do interrupts until PWO goes high
  __enable_irq();
}

void mySystick_isr(void)
{
  systick_cycle_count = ARM_DWT_CYCCNT;
  systick_millis_count++;
  //  TOGGLE_SCOPE_2;     //  Used during debug to see when Systick interrupts start. Answer: Some time shortly after setup() starts.
                          //  i.e. It is running as soon as the Teensy boot processor passes control to startup.c
}

#if ENABLE_EMC_SUPPORT

//
//  Extended Memory code. It's here because it is tightly coupled with the bus cycles rather than just a
//  simple peripheral
//
//  Start_bank is the starting 32k bank of EMC memory provided by EBTKS. Currently we emulate up to 256kB
//  (8 banks) set by get_EMC_NumBanks() from the CONFIG.TXT file
//
//  Systems can only have one EMC Master that responds to reading of control/status registers. Non master
//  EMC chips should have identical control/status content but dont respond. All Series 80 computers after
//  the original HP85A have an EMC controller (1MC4) on the main board, and the 64K and 128K memory modules
//  also have 1MC4 controllers, but they are never a master. For diagnostic and development purposes, a
//  HP85A can be modified to support EMC memory supplied by EBTKS, including EBTKS acting as an EMC Master.
//  To designate such a modified HP85A, we referto it in the CONFIG.TXT file as MACH_HP85AEMC. This is
//  not a configuration that should ever be of interest to an end user. It is just for Russell and Philip
//
//  The real HP hardware supposedly only supports up to 1M of EMC but the architecture supports up to 16M.
//  For 1 MB there would be a total of 32 banks of 32 KB each numbered 0..31 .The minimum start bank should
//  be 2 (bank 1 is mapped as real memory in the real hardware, bank 0 is the ROM space).
//
//  The handling of one or more memory modules with EMC controllers (64KB and 128KB) automatically figure
//  out their respective start bank during a little dance that involves the interupt priority diasy chain
//  that occurs almost immediately after the release of PWO, and well before and EMC memory references can
//  take place. EBTKS does not have the required hardware to participate in this process, and so requires
//  the user to manually setup the start bank in the CONFIG.TXT file base on what will be appropriate for
//  the specific system. EBTKS provided EMC memory start bank should be set to the bank number AFTER the
//  last provided bank of the main system plus any plugged in memory modules. A table of tested values
//  can be found in EBTKS_SD where the CONFIG.TXT is processed.
//
////////////////////////////
//  Little warning, not sure if this is a bug:
//  on an HP85A (that does not have EMC), but with the following ROMs enabled
//      rom320B          320 208  D0   2F    FF   85B Mass Storage
//      rom321B          321 209  D1   2E    FF   EDisk
//
//  the command CAT ":D000"  gives an error message of "Error 126 : MSUS"
//  but the command CAT ".ED" gives the error message of "Error 131 : TIME-OUT"
//    and then both the HP85A and EBTKS hang and even grounding PWO does not recover.
//    Need to cycle power (or press the reset button on Teensy)
////////////////////////////
//

void emc_init(void)
{
  m_emc_start_addr = get_EMC_StartAddress();
  m_emc_master     = get_EMC_master();           //  When true - we are the only or master EMC in the system. Only on HP85AEMC with IF modification

  setIOReadFunc( 0xc8 , &emc_r );
  setIOReadFunc( 0xc9 , &emc_r );
  setIOReadFunc( 0xca , &emc_r );
  setIOReadFunc( 0xcb , &emc_r );
  setIOReadFunc( 0xcc , &emc_r );
  setIOReadFunc( 0xcd , &emc_r );
  setIOReadFunc( 0xce , &emc_r );
  setIOReadFunc( 0xcf , &emc_r );

  setIOWriteFunc( 0xc8 , &emc_w );
  setIOWriteFunc( 0xc9 , &emc_w );
  setIOWriteFunc( 0xca , &emc_w );
  setIOWriteFunc( 0xcb , &emc_w );
  setIOWriteFunc( 0xcc , &emc_w );
  setIOWriteFunc( 0xcd , &emc_w );
  setIOWriteFunc( 0xce , &emc_w );
  setIOWriteFunc( 0xcf , &emc_w );
}

//
//  note ! uses C++ 'reference'
//

inline uint32_t &get_ptr()
{
  if (m_emc_mode & 0x04u) //ptr1 or ptr2?
  {
    return m_emc_ptr2;
  }
  else
  {
    return m_emc_ptr1;
  }
}

inline void emc_ptr12_decrement(void)
{
  uint8_t disp;

  if (m_emc_drp & 0x20u) //8 byte regs or 2 byte regs?
  {
    disp = 8u - (m_emc_drp & 7u);
  }
  else
  {
    disp = 2u - (m_emc_drp & 1u);
  }

  if (m_emc_mult)
  {
    get_ptr() -= disp;
  }
  else
  {
    get_ptr()--;
  }
}

inline void lma_cycle(bool rd)
{
  m_emc_lmard = rd; //  True if LMA read

  if (m_emc_state == EMC_INDIRECT_1)
  {
    m_emc_state = EMC_INDIRECT_2;
  }
  else if (m_emc_state == EMC_INDIRECT_2)
  {
    if (!(m_emc_mode & 2u))
    {
      //  In PTRx & PTRx- cases, bring the PTR back to start
      emc_ptr12_decrement();
    }
    m_emc_state = EMC_IDLE;
  }
}

bool emc_r(void)
{
  readData = 0xff;
  bool didRead = false;
  //m_emc_read++;

  if (m_emc_state == EMC_INDIRECT_1)
  {
    uint32_t &ptr = get_ptr();
    //
    //  decide if the address is in the range we should respond to
    //
    if ((ptr >= m_emc_start_addr) && ((ptr - m_emc_start_addr) < EMC_RAM_SIZE))
    {
      readData = emcram[ptr - m_emc_start_addr];
      didRead = true;
    }
    ptr++;
  }
  else if (m_emc_lmard)
  {
    m_emc_mode = uint8_t((addReg & 0xffu) - 0xC8u);
    // During a LMARD pair, address 0xffc8 is returned to CPU and indirect mode is activated
    if (cycleNdx == 0)
    {
      readData = 0xc8;
      didRead = m_emc_master;   //only one EMC controller should respond
    }
    else if (cycleNdx == 1)
    {
      m_emc_state = EMC_INDIRECT_1;
      if (m_emc_mode & 1u)
      {
        // Pre-decrement
        emc_ptr12_decrement();
      }
    }
  }
  else
  {
    m_emc_mode = uint8_t((addReg & 0xffu) - 0xC8u);
    // Read PTRx
    if (cycleNdx < 3)
    {
      readData = uint8_t(get_ptr() >> (8 * cycleNdx));
      didRead = m_emc_master;     //only one EMC controller should respond
    }
  }
  return didRead;
}

void emc_w(uint8_t val)
{
  //m_emc_write++;
  if (m_emc_state == EMC_INDIRECT_1)
  {
    uint32_t &ptr = get_ptr();

    if ((ptr >= m_emc_start_addr) && ((ptr - m_emc_start_addr) < EMC_RAM_SIZE))
    {
      emcram[ptr - m_emc_start_addr] = val;
    }
    ptr++;
  }
  else
  {
    m_emc_mode = uint8_t((addReg & 0xffu) - 0xC8u);
    // Write PTRx
    if (cycleNdx < 3)
    {
      uint32_t &ptr = get_ptr();
      uint32_t mask = (uint32_t)0xffU << (8u * cycleNdx);
      ptr = (ptr & ~mask) | (uint32_t(val) << (8u * cycleNdx));
    }
  }
}
#endif
//
//  If you need SCOPE_2 and SCOPE_1 diag pins in non-EBTKS code (like the SdFat library), then
//  the following should be helpful
//
//  #define GPIO_DR_SET_SCOPE_2                     GPIO6_DR_SET
//  #define GPIO_DR_CLEAR_SCOPE_2                   GPIO6_DR_CLEAR
//  #define GPIO_DR_TOGGLE_SCOPE_2                  GPIO6_DR_TOGGLE
//  #define BIT_POSITION_SCOPE_2                    (29)
//  #define BIT_MASK_SCOPE_2                        (1 << BIT_POSITION_SCOPE_2       )
//  #define SET_SCOPE_2                             (GPIO_DR_SET_SCOPE_2    = BIT_MASK_SCOPE_2)
//  #define CLEAR_SCOPE_2                           (GPIO_DR_CLEAR_SCOPE_2  = BIT_MASK_SCOPE_2)
//  #define TOGGLE_SCOPE_2                          (GPIO_DR_TOGGLE_SCOPE_2 = BIT_MASK_SCOPE_2)
//
//  #define GPIO_DR_SET_SCOPE_1                     GPIO9_DR_SET
//  #define GPIO_DR_CLEAR_SCOPE_1                   GPIO9_DR_CLEAR
//  #define GPIO_DR_TOGGLE_SCOPE_1                  GPIO9_DR_TOGGLE
//  #define BIT_POSITION_SCOPE_1                    (7)
//  #define BIT_MASK_SCOPE_1                        (1 << BIT_POSITION_SCOPE_1       )
//  #define SET_SCOPE_1                             (GPIO_DR_SET_SCOPE_1    = BIT_MASK_SCOPE_1)
//  #define CLEAR_SCOPE_1                           (GPIO_DR_CLEAR_SCOPE_1  = BIT_MASK_SCOPE_1)
//  #define TOGGLE_SCOPE_1                          (GPIO_DR_TOGGLE_SCOPE_1 = BIT_MASK_SCOPE_1)
