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

ioReadFuncPtr_t ioReadFuncs[256];      //ensure the setup() code initialises this!
ioWriteFuncPtr_t ioWriteFuncs[256];

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
// this register is implemented in the 1MB5 translator chips - they all share this register
void onWriteInterrupt(uint8_t val)
{
    (void)val;
    intEn_1MB5 = true; //a write to this register enables 1MB5 interrupts

}

void initIOfuncTable(void)
{
for (int a = 0; a < 256; a++)
  {
    ioReadFuncs[a] = &ioReadNullFunc; //default all
    ioWriteFuncs[a] = &ioWriteNullFunc;
  }

setIOWriteFunc(0x40,&onWriteInterrupt); //177500 1MB5 INTEN
setIOWriteFunc(0, &onWriteGIE);         //global interrupt enable
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
//
//  EBTKS has only 2 interrupts, one for Phi 1 rising edge, and one for Phi 2 rising edge
//  Both interupts come here to be serviced. They are mutually exclusive.
//
//  The control signals /LMAX, /RD, and /WR are sampled after the falling edge of Phi 1,
//  based on timing analysis of reverse engineering work
//

FASTRUN void pinChange_isr(void)      //  This function is an ISR, keep it short and fast.
{

  uint32_t interrupts;

  TOGGLE_RXD;                           //  Mark the start of all ISR
  interrupts = PHI_1_and_2_ISR;         //  This is a GPIO ISR Register 
  PHI_1_and_2_ISR = interrupts;         //  This clears the interrupts

  if (interrupts & BIT_MASK_PHASE1)     //  Phi 1
  {
    onPhi_1_Rise();               //                                                                                           Need to document Max Duration
    WAIT_WHILE_PHI_1_HIGH;        //  While Phi_1 is high, just hang around, not worth doing a return from interrupt
                                  //  and then having an interrupt on the falling edge.
                                  //
                                  //  Note that if onPhi_1_Rise() takes more than 200 ns minus the time it took
                                  //  to get to the call to onPhi_1_Rise(), the call to onPhi_1_Fall() will occur
                                  //  after the edge by that excess time

    onPhi_1_Fall();               //                                                                                           Need to document Max Duration
  }
  else                            //  Sneaky because we only have two possible interrupts, so must be Phi 2 Rising
  {
    onPhi_2_Rise();
  }
  TOGGLE_RXD;                     //  Mark the end of all ISR
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
//  Actual measurements show data valid prior to the rising edge of Phi 1 as early as 
//
//  Regardless of whether EBTKS or any other device/ROM/RAM/CPU is either reading
//  or writing to the data bus, by the time we get the Phi 1 rising edge, the data
//  has been valid for at least 150 ns. 
//

inline void onPhi_1_Rise(void)                  //  This function is running within an ISR, keep it short and fast.             This needs to be timed.
{
  uint32_t data_from_IO_bus;

  data_from_IO_bus = (GPIO_PAD_STATUS_REG_DB0 >> BIT_POSITION_DB0) & 0x000000FFU;       //  Requires that data bits are contiguous and in the right order

  if (intrState)
    {
      ASSERT_INTPRI;  //if we have asserted /IRL, assert our priority in the chain
    }
  
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

  Logic_Analyzer_main_sample =  data_from_IO_bus |                            //  See above for constraints for this to work.
                                (addReg << 8)    |                            //  because we are very fast, it is ok to take
                                Logic_Analyzer_current_bus_cycle_state_LA;    //  data on Phi 1 Rising edge, unlike HP-85 that    Logic_Analyzer_current_bus_cycle_state_LA is setup in the Phi 2 code
                                                                              //  uses the falling edge.
  Logic_Analyzer_aux_sample  =  getRselec() & 0x000000FF;                     //  Get the Bank switched ROM select code

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
        if (   ((Logic_Analyzer_main_sample & Logic_Analyzer_Trigger_Mask_1) == Logic_Analyzer_Trigger_Value_1) &&
              ((Logic_Analyzer_aux_sample  & Logic_Analyzer_Trigger_Mask_2) == Logic_Analyzer_Trigger_Value_2)    )
        {   //  Trigger pattern has been matched
          if (--Logic_Analyzer_Event_Count <= 0)                             //  Event Count is how many match events needed to trigger
          {
            Logic_Analyzer_Triggered = true;
            Logic_Analyzer_Index_of_Trigger = Logic_Analyzer_Data_index;   //  Record the buffer index at time of trigger
          }
        }
      }
    }
    Logic_Analyzer_Data_1[Logic_Analyzer_Data_index  ] = Logic_Analyzer_main_sample;
    Logic_Analyzer_Data_2[Logic_Analyzer_Data_index++] = Logic_Analyzer_aux_sample;

    Logic_Analyzer_Data_index &= Logic_Analyzer_Current_Index_Mask;           //  Modulo addressing of sample buffer. Requires buffer length to be a power of two
    Logic_Analyzer_Valid_Samples++;                                           //  This could theoretically over flow if we didn't see a trigger in 7000 seconds (1.9 hours). Saturating is not worth the overhead
    if (Logic_Analyzer_Triggered)
    {
      if (--Logic_Analyzer_Samples_Till_Done == 0)
      {
        Logic_Analyzer_State = ANALYZER_ACQUISITION_DONE;
      }
    }
  }

//
//  If this is a Write cycle to EBTKS, this is where it is handled
//

  if (schedule_write)
  {
    onWriteData(addReg, data_from_IO_bus);                      //  Need to document max time
  }
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

inline void onPhi_2_Rise(void)                             //  This function is running within an ISR, keep it short and fast.
{
  bool       lma;
  bool       rd;
  bool       wr;
  uint32_t   dataBus;
  uint32_t   bus_cycle_info;                              //  Bits 26 down to 24 will be /WR , /RD , /24 in that order

  TOGGLE_RXD;                                             //  Glitch the ISR entry oscilloscope signal to identify the Phi 2 duration separately from Phi 1
  TOGGLE_RXD;
//
//  This is Russell's code with some minor edits.
//

  bus_cycle_info = GPIO_PAD_STATUS_REG_LMA;               //  All 3 control bits are in the same GPIO register
  lma = !(bus_cycle_info & BIT_MASK_LMA);                 //  Invert the bits so they are active high
  rd  = !(bus_cycle_info & BIT_MASK_RD );
  wr  = !(bus_cycle_info & BIT_MASK_WR );

  //  Resolve control logic states

  schedule_read  = rd && !wr;
  schedule_write = wr && !rd;
  schedule_address_increment = ((addReg >> 8) != 0xff) &&
                                !(!schedule_address_load & delayed_lma) &&
                                (schedule_read | schedule_write);         //  Only increment addr on a non i/o address
  schedule_address_load = !schedule_address_load && delayed_lma;          //  Load address reg flag
  delayed_lma = lma;                                                      //  Delayed lma
  DMA_Acknowledge = wr && rd && !lma;                                     //  Decode DMA acknowlege state
  Interrupt_Acknowledge = wr && rd && lma;                                //  Decode interrupt acknowlege state

  Logic_Analyzer_current_bus_cycle_state_LA = bus_cycle_info  &           //  Yes, this is supposed to be a single &
                                              (BIT_MASK_WR | BIT_MASK_RD | BIT_MASK_LMA);      //  Require bus cycle state to be in bits 24, 25, and 26 of GPIO register

  Logic_Analyzer_current_bus_cycle_state_LA |=  ((GPIO_PAD_STATUS_REG_T04    & BIT_MASK_T04)    << (29 - BIT_POSITION_T04   )) |    //  T04 is monitoring bus /IRLX
                                                ((GPIO_PAD_STATUS_REG_T05    & BIT_MASK_T05)    << (28 - BIT_POSITION_T05   )) |    //  T05 is monitoring bus /HALTX
                                                ((GPIO_PAD_STATUS_REG_IFETCH & BIT_MASK_IFETCH) << (30 - BIT_POSITION_IFETCH)) ;


//
//  This is Philip's much nicer version of above that uses enums, and doesn't work on rare occasions.
//
//////enum bus_cycle_type{
//////        BUS_INTACK    = 0,
//////        BUS_DMA_GRANT = 1,
//////        BUS_ADDR_WR   = 2,    //  Should never happen
//////        BUS_WR        = 3,
//////        BUS_ADDR_RD   = 4,    //  While reading from somewhere, also load value into address register
//////        BUS_RD        = 5,
//////        BUS_ADDR      = 6,
//////        BUS_NOP       = 7
//////      };
//////
//////
//////  previous_bus_cycle_state   = current_bus_cycle_state;         //  Save what the prior bus cycle was. Used in handling high byte/low byte  of dual /LMA address transfers
//////
//////  Logic_Analyzer_current_bus_cycle_state_LA = GPIO_PAD_STATUS_REG_LMA &         //  Yes, this is supposed to be a single &
//////                               (BIT_MASK_WR | BIT_MASK_RD | BIT_MASK_LMA);      //  Require bus cycle state to be in bits 24, 25, and 26 of GPIO register
//////
//////  current_bus_cycle_state    = (enum bus_cycle_type)(Logic_Analyzer_current_bus_cycle_state_LA >> 24);            //  Make it ready to be used in enum comaprisons
//////
//////  schedule_read              = ((current_bus_cycle_state == BUS_RD) || (current_bus_cycle_state == BUS_ADDR_RD));
//////  schedule_write             = (current_bus_cycle_state == BUS_WR);
//////  schedule_address_increment = ((addReg >> 8) != 0x00FFU) &&                    //  If not I/O space
//////                               !(!schedule_address_load & delayed_lma) &&       //    and
//////                               (schedule_read ||  schedule_write);              //  Any bus read or write
//////  schedule_address_load      = !schedule_address_load && delayed_lma;
//////  delayed_lma                = (current_bus_cycle_state == BUS_ADDR) || (current_bus_cycle_state == BUS_ADDR_RD) || (current_bus_cycle_state == BUS_INTACK);
//////  DMA_Acknowledge            = (current_bus_cycle_state == BUS_DMA_GRANT);
//////  Interrupt_Acknowledge      = (current_bus_cycle_state == BUS_INTACK);
//////

  if (schedule_read)
  {           //  Test if address is in our range and if it is , return true and set readData to the data to be sent to the bus
    HP85_Read_Us = onReadData(addReg);
  }

//  if (HP85_Read_Us && just_once)
//  {
//    SET_TXD;
//    Serial.printf("The address register is %06o\n", addReg);
//    just_once = 0;
//  }
//
//  NOT Tested yet
//

 switch(intrState)
  {
    case 0: //test for a request or see if there is an intack

      if ((Interrupt_Acknowledge) && (!IS_IPRIH_IN_LO))
      {
        intEn_1MB5 = false;   //lower priority device had interrupted, disable our ints until a write to 177500
      }
      else if ((interruptReq == true) && (globalIntEn == true) && (intEn_1MB5 == true))
      {
        intrState = 1; //interrupt was requested
        ASSERT_INT; //IRL low synchronous to phi2 rise - max 500ns to fall
      }  
      break;

    case 1: // IRL is low
      if ((Interrupt_Acknowledge) && (!IS_IPRIH_IN_LO)) //we're interrupting - so it must be our turn
        {
          RELEASE_INT;
          RELEASE_INTPRI;
          globalIntAck = true;  //set when we've been the interrupter
          readData = interruptVector;     
          HP85_Read_Us = true; 
          interruptReq = false; //clear request
          intrState = 0;
        }
      if ((Interrupt_Acknowledge) && (IS_IPRIH_IN_LO)) //higher priority request beat us
        {
          RELEASE_INT;  //let go of /IRL as we lost
          RELEASE_INTPRI;
          intrState = 0; //try for another request
        }
      break;
  }

  //  For a Read Cycle (I/O bus to CPU on main board), this is where we turn the data bus around
  //  and place the data on the bus. We also assert BUS_DIR_TO_HP which changes the direction of
  //  the 8 bit bus level translator. This signal also asserts (drives low) the /RC signal that
  //  tells the 1MA8 that the data direction is from the I/O bus to the CPU.
  //  Timing is not too critical here, as we are about 800 ns before the rising edge of Phi 1
  //  which is when the data is read. Actually latched by Phi 1, so another 200 ns margin.

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
    //  SET_TXD;
    //  EBTKS_delay_ns(100);    //  This is used as a trigger, and delays us driving the bus for 100 ns, which we believe is a "don't care" due to long setup time
    //  CLEAR_TXD;
    //

    //  Set bus to output data 
       
    DISABLE_BUS_BUFFER_U2;
    BUS_DIR_TO_HP;                        //  DIR high  !!! may need to delay for 1MA8 to let go of driving bus: See above diagnostic test. Confirmed
                                          //  that starting to drive the data bus (with /RC) at Phi 2 is not going to cause contention

    SET_T4_BUS_TO_OUTPUT;                 //  set data bus to output      (should be no race condition, since local)

    dataBus  = (uint32_t)(readData & 0x000000FFU) << 16;  // Set up data to be written to the appropriate bits of GPIO_DR_SET_DB0

    //
    //  Use clear/set regs to output the bits we want without touching any others - not sure if this is the fastest method
    //
    GPIO_DR_CLEAR_DB0 = DATA_BUS_MASK;    //  Clear all 8 data bits
    GPIO_DR_SET_DB0   = dataBus;          //  Put data on the bus. This is the central point for ALL I/O and Memory reads that this board supports

    ENABLE_BUS_BUFFER_U2; // !OE low.
  }

  //
  //  ALL DMA requests are handled here
  //  DMA is asserted 200 ns after the falling edge of Phi 2
  //
  
  if (DMA_Request)
  {
    //WAIT_WHILE_PHI_2_HIGH;              //  While Phi_2 is high, just hang around
    // delayNanoseconds(130);             //  There is 70 ns overhead, so this puts the earliest version at 200 ns after falling Phi 2
                                          //  The latest is about 300 ns , so 100 ns jitter, probably due to arrival at the above wait while Phi 2 is high.
    ASSERT_HALT;

    DMA_Request = false;                  //  Only request once
    DMA_has_been_Requested = true;        //  Record that a request has occured on the bus, but has not yet been acknowledged
  }

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
    NVIC_CLEAR_PENDING(IRQ_GPIO6789);     //  Even though we just masked GPIO interrupts, the NVIC can still
                                          //  have pending interrupts. It shouldn't at this point, but clear
                                          //  them, just to be safe. We will need to do this again, just
                                          //  before ending the DMA activity.
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

//  Don't do this. I/O Bus is precharged by 1MA8 during DMA Acknowledge.
//  NOTE:  NOTE:  The following commented out code is from Nirvana V0.1, and the GPIO registes and bit positions do not match EBTKS V2.0
//
//////    //
//////    //  Finally, Set the Data buffer to drive the I/O bus with 0xFF
//////    //
//////    DISABLE_BUS_BUFFER_U2;
//////    BUS_DIR_TO_HP;                  //  DIR high  !!! may need to delay for 1MA8 to let go of driving bus                            Need to investigate
//////    GPIO6_GDIR |= DATA_BUS_MASK;    //  Set data bus to output      (should be no race condition, since local)
//////    GPIO6_DR_SET = DATA_BUS_MASK;   //  Set all bits High
//////    ENABLE_BUS_BUFFER_U2;           //  !OE Low.

    //
    //  The time is now some nanoseconds after Phi 1 Falling.
    //  The 3 control lines are driving the I/O bus, and each is set High
    //  The 8 Data bits are driving te I/O bus with 0xFF

// *** need to deal with data direction, and initially undriven because we don't know what 1MA8 does with bus during DMA. It may still be driving
//     and may need RC to flip it around. So maybe can't put addresses onto bus without RC
//     This may also be a requirement to precharge the bus on the main board, via the 1MA8.

    DMA_Acknowledge = false;
    DMA_Active = true;

  //
  //  We now own the bus, the 3 control lines are high, HALT is still asserted, and interrupts are off, we are driving
  //  the data bus with 0xFF
  //
  //  Be responsible with this Great Power
  //

  __disable_irq();  //  Disable all interrupts for duration of DMA. No USB activity, no Serial via USB, no SysTick
                    //  We need to do this to get precise timing for /LMA /RD /WR /RC
                    //
                    //  10/30/2020 Maybe we could avoid this, allowing diagnostic Serial.printf("for example")
                    //  by only doing this when we are actually doing 1 or more dma transactions. We have already
                    //  disabled the pin interrupts.

  //
  //  The matching release of DMA is in EBTKS_DMA.cpp at release_DMA_request()
  //

  }
  //EBTKS_delay_ns(120);    //  We seem to be exiting so fast that the external logic analyzer is missing this ISR
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

inline bool onReadData(uint16_t Current_Read_Address)                  //  This function is running within an ISR, keep it short and fast.
{
  //
  //  If any of these tests indicate that we need to supply data
  //    Put the data in readData
  //    return true
  //  else
  //    return false
  //

  if ((Current_Read_Address & 0xE000) == ROM_PAGE) // ROM page 0x6000..0x7FFF   (8 KB)
  {
    return readBankRom(Current_Read_Address & (ROM_PAGE_SIZE - 1));     //  Knock off some MSBs so we address the ROM from Current_Read_Address 0
                                                        //  This function knows about the AUXROM RAM Window, and handles these reads as well
  }

  if (enRam16k)
  {
    //
    //  For HP-85 A, implement 16384 - 256 bytes of RAM, mapped at 0xC000 to 0xFEFF (if enabled)
    //
    if ((Current_Read_Address >= HP85A_16K_RAM_module_base_addr) && (Current_Read_Address < IO_ADDR))
    {
      readData = HP85A_16K_RAM_module[Current_Read_Address & 0x3FFF];
      return true;
    }
  }

  //
  //  Process I/O reads (data from I/O bus to the CPU)
  //
  if ((Current_Read_Address & 0xFF00U) == 0xFF00U)
  {
    return (ioReadFuncs[Current_Read_Address & 0x00FFU])();  // Call I/O read handler
  }

  //
  //  If we get here, none of the checked address ranges matche the current Read Address
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
//    Any SpecialRAM that we are implementing within the AUXROMs
//    Any I/O registers that we are implementing
//    Any I/O registers that we are tracking
//
//    Unlike the onReadData() function that is associated with Phi 2, and has rather relaxed timing,
//    onWriteData() is associated with Phi 1, and the timing is quite tight
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

  if (enRam16k)
  {
    //
    //  For HP-85 A, implement 16384 - 256 bytes of RAM, mapped at 0xC000 to 0xFEFF (if enabled)
    //

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

//
//  Handle Capricorn driving I/O bus, of either Address Bytes (/LMA) or Data Bytes (/RD)
//
//  This function is running within an ISR, keep it short and fast.
//  Depending on the execution time of onPhi_1_Rise(), this may
//  start later than the name implies (with jitter)
//
//  This function has been timed (see commented SET_TXD/CLEAR_TXD) and it takes from 42 ns to 74 ns to execute
//  The earliest start of this function is 54 ns after the falling edge of Phi 1
//  The latest start is 92 ns after the falling edge of Phi 1
//  The earliest end of this function is 97 ns after the falling edge of Phi 1
//  The latest end is 150 ns after the falling edge of Phi 1
//  Thus 50 ns margin until precharge during Phi 12
//

inline void onPhi_1_Fall(void)
{
  uint32_t data_from_IO_bus;

  //  SET_TXD;

  data_from_IO_bus = (GPIO_PAD_STATUS_REG_DB0 >> BIT_POSITION_DB0) & 0x000000FFU;       //  Requires that data bits are contiguous and in the right order

  if (HP85_Read_Us)                     //  If a processor read was getting data from this board, we just finished doing it. This was set in onPhi_2_Rise()
  {
    SET_T4_BUS_TO_INPUT;                //  Set data bus to input on Teensy
    BUS_DIR_FROM_HP;                    //  Change direction of bus buffer/level translator to inbound from I/O bus
                                        //  This also de-asserts /RC  (it goes High)
    HP85_Read_Us = false;               //  Doneski
  }

  if (schedule_address_load)            //  Load the address registers. Current data_from_IO_bus is the second byte of
                                        //  the address, top 8 bits. Already got the low 8 bits in pending_address_low_byte
  {
    addReg  = data_from_IO_bus << 8;    //  Rely on zero fill for bottom 8 bits
    addReg |= pending_address_low_byte; //  Should always be only bottom 8 bits
    //  CLEAR_TXD;
    return;                             //  Early exit.
  }

  if (schedule_address_increment)       //  Increment address register. Happens for Read and Write cycles, unless address is being loaded, or address register is I/O space
  {
    addReg++;
  }

  pending_address_low_byte = data_from_IO_bus;    // Load this every cycle, in case we need it, avoiding an if ()

  //  CLEAR_TXD;
}

void setupPinChange(void)
{
  __disable_irq();    //  This code is a critical region.
  //
  //  Let's cheat a little and just use the teensy core code to do most of the work for the setup
  //
  attachInterrupt(CORE_PIN_PHASE1, NULL, RISING);   // PHI 1 is Teensy 4.1 pin 21, Physical pin  43
  attachInterrupt(CORE_PIN_PHASE2, NULL, RISING);   // PHI 2 is Teensy 4.1 pin 38, Physical pin  30

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
}

///////////////////////////////////////////////////////////  Replacement functions for the global IRQ disable and enable in 
///////////////////////////////////////////////////////////  C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\cores\teensy4\usb.c
///////////////////////////////////////////////////////////  

// extern "C" void ebtks_disable_irq(void)
// {
//   DMA_Request = true;
//   while(!DMA_Active){};     //  Wait for acknowledgement, and Bus ownership, and all interrupts disabled
// }

// extern "C" void ebtks_enable_irq(void)
// {
//   release_DMA_request();    //  Release the HP85 bus and enable interrupts
//   while(DMA_Active){};      //  Wait for release
// }
// }
