//
//  !!!!!!   While DMA is active, ALL Teensy interrupts are disabled
//              including Phi 1 & 2 ISRs, USB, Serial, SysTick
//
//      06/27/2020      These Function were moved from EBTKS.cpp to here.
//                      So far just the DMA Read
//      07/01/2020      Started on DMA Write. Got distracted by discovering errors in DMA Read
//
//      07/02/2020 .. 07/05/2020 Chase down random Read failures in DMA blocks over 30 bytes.
//                      Strongly suspect mySystick_isr() and/or some keep-alive stuff
//                      associated with USB and/or Serial printing via USB. Solution is to
//                      disable all interrupts while DMA is running. Also wrote and calibrated
//                      EBTKS_delay_ns() which is in the file EBTKS_Utilities.c
//                      Success: seems like DMA_Preamble() is tuned perfectly, and interrupt
//                      lockout seems to have fixed the intermittent read errors.
//
//      07/06/2020      Start on fixing DMA_Read_Block()
//                      Clean up some documentation at the top of the file
//
//      07/09/2020      Write DMA Write routines
//                      Start to work on standardizing all timing
//                              Data driven onto the I/O bus, starts at Phi 2 Falling edge,
//                                and /RC is asserted at the same time.
//                              Data remains driven onto the I/O bus until 100 ns after the
//                                falling edge of Phi 1, and /RC is deasserted at this time.
//                              /LMAX, /RDX, and /WRX all have the same timing. They are
//                                adjusted so that the falling edge as seen by the 1MB1
//                                (/LMA, /RD,  and /WR) occurs 130 ns after the rising edge
//                                of Phi 1, and remains asserted for 1160 ns.
//                                (about the middle of the Phi 21 pulse)
//
//      07/10/2020      Go through all critical timing, and tweak as necessary. Get within 10 ns of goals
//                          /LMAX
//                                Adjust this so that /LMA at 1MB1 pin 16 has a Falling edge 130 ns after Rising edge of Phi 1, Low duration of 1160 ns
//      07/11/2020          /RDX
//                                Adjust this so that /RD  at 1MB1 pin 17 has a Falling edge 130 ns after Rising edge of Phi 1, Low duration of 1160 ns
//      07/12/2020          /WRX
//                                Adjust this so that /WR  at 1MB1 pin 15 has a Falling edge 130 ns after Rising edge of Phi 1, Low duration of 1160 ns
//                      Oscilloscope channels are:
//                        CH1   Phi 1
//                        CH2   Selected pin on 1MB1
//                        Ch3   is trigger for measurement, placed where it won't interfere with the Low pulse width or start time. Use T33
//
//      07/14/2020      Re do all the timing tweaks, and get oscilloscope pics as needed
//


#include <Arduino.h>

#include "Inc_Common_Headers.h"

static void DMA_Preamble(uint16_t DMA_Target_Address);
static int32_t DMA_Read_Burst(uint8_t buffer[], uint32_t bytecount);           //  This function is only called by DMA_Read_Block()
static int32_t DMA_Write_Burst(uint8_t buffer[], uint32_t bytecount);          //  This function is only called by DMA_Write_Block()

#define OUTPUT_DATA_HOLD_TWEAK               EBTKS_delay_ns(90)                //  Adjusts the Hold time after the Falling edge of Phi 1 for Address bytes and Write data. Goal is 100 ns
#define CTRL_START_LMA_1_TWEAK               EBTKS_delay_for_LMA_start()       //  Adjusts the start time of /LMAX, /RDX, and /WRX after Phi 1 Rising edge. Goal is 130 ns after Phi 1 Rising
#define CTRL_START_LMA_2_TWEAK               EBTKS_delay_for_LMA_start()       //  Adjusts the start time of /LMAX, /RDX, and /WRX after Phi 1 Rising edge. Goal is 130 ns after Phi 1 Rising
#define CTRL_START_RD_TWEAK                  EBTKS_delay_for_LMA_start()       //  Adjusts the start time of /LMAX, /RDX, and /WRX after Phi 1 Rising edge. Goal is 130 ns after Phi 1 Rising
#define CTRL_START_WR_TWEAK                  EBTKS_delay_for_LMA_start()       //  Adjusts the start time of /LMAX, /RDX, and /WRX after Phi 1 Rising edge. Goal is 130 ns after Phi 1 Rising
#define CTRL_END_LMA_TWEAK_3                 EBTKS_delay_ns(117)               //  Adjusts /LMA low duration. Goal is 1160 ns
#define CTRL_END_LMA_TWEAK_4                 EBTKS_delay_ns(136)               //  Adjusts /LMA low duration. Goal is 1160 ns
#define CTRL_END_RD_TWEAK                    EBTKS_delay_ns(140)               //  Adjusts the end time of /RDX  after Phi 1 Rising edge. Goal is 130 ns after Phi 1 Rising
#define CTRL_END_WR_TWEAK                    EBTKS_delay_ns(145)               //  Adjusts the end time of /WRX  after Phi 1 Rising edge. Goal is 130 ns after Phi 1 Rising


//
//  The following delays include the time to make the call and return
//

//void EBTKS_delay_15_ns(void)
//{
//  asm volatile("mov r0, r0\n\t");
//}

void EBTKS_delay_34_ns(void)
{
  asm volatile("mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" );
}

void EBTKS_delay_for_LMA_start(void)
{
  asm volatile("mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" );
}



//
//  Don't touch this code unless you absolutely can explain each and every line of it.
//  This code is extremely deeply intertwingled.
//  These routines only receives validated addresses of resources that this board
//  does NOT provide. For example system ROMs, first 16K or RAM, I/O device registers
//    (but not I/O devices that this board provides)
//  The buffer must be big enough for the transfer.
//  The bytecount must not be bigger than the buffer size, but can be smaller
//
//  Future enhancement: This may change in the future for a more unified approach to DMA
//    that looks at the address and recognizes the following regions:
//      ROMs
//      First  16 KB of DRAM
//      Second 16 KB of DRAM/SRAM
//      I/O space (with maybe different behavior depending on I/O device)
//      EMS
//    and based on the address, does the right thing
//
//  By the time these routines are called,
//    DMA bus ownership has been negotiated.
//    All interupts have been disabled
//    The 3 control lines are Outputs, and they are all driving High
//    The control signal buffer/level shifter (U3) has the direction set to
//      take the control signals from the Teensy outputs and drive them on
//      to the I/O bus.
//    The data buffer/level shifter (U2) has the direction set to BUS_DIR_FROM_HP
//
//NEED TO EDIT THE FOLLOWING, TO MATCH REALITY
//
//  Send just 2 /LMA Pulses, starting during Phi 1 high, going for 1.17 us, followed by starting a READ transaction
//  The timing of the falling and rising edges that we generate here are ultra-tweaked so
//  that after they have gone through the level shifters, the I/O bus cable, and the 1MA8,
//  the /LMA at pin 16 of the 1MB1 (Capricorn) has nearly identical timing to the /LMA                                  <<<<<<<<<<<<<<<<<<<<<<<<<<  Need to do the same for DMA Write DATA
//  signals that the 1MB1 creates.
//
//  This function should not be called for DMA R/W of resources that this board provides (ROM, RAM, ExtendedRAM, I/O)
//
//  The DMA addresses are driven starting at Phi 2 falling, and remain active until Phi 1 falling
//
//  In the following, C obviously objects to back slash as last character of line, so just ignore the '   ...'
//
//  Timing              Event
//  ---                 Preamble to make sure we sync with 'Phi 1 R'
//  Phi 1 R
//  delay 1 ns          LMA \   ...
//  Phi 2 F             Disable data bus buffer
//                      Set data bus to output
//                      Put low address byte on pins
//                      Set bus direction to BUS_DIR_TO_HP
//                      /RC \   ...
//                      Enable bus buffer
//  Delay 95ns          LMA /
//  Phi 1 R
//  delay 1 ns          LMA \   ...
//  Phi 1 F             Disable data bus buffer
//                      Set bus direction to BUS_DIR_FROM_HP
//                      /RC /
//                      Put high address byte on pins
//  Phi 2 F             Bus buffer already disabled
//                      I/O already set to output
//                      High address byte already on pins
//                      Set bus direction to BUS_DIR_TO_HP
//                      /RC \   ...
//                      Enable bus buffer
//  Delay 95ns          LMA /
//  Phi 1 F             Disable bus buffer
//                      Set bus direction to BUS_DIR_FROM_HP
//                      /RC /
//                      Set I/O to input
//                      Enable bus buffer


int32_t DMA_Read_Block(uint32_t DMA_Target_Address, uint8_t buffer[], uint32_t bytecount)
{

  uint32_t               buffer_index;
  uint8_t                refresh_count;

  if((bytecount == 0) || (buffer == NULL) || (!DMA_Active))
  {
    return 0;   //  Pointless or dangerous call
  }

  __disable_irq();   //  Disable all interrupts while DMA is happening. In particular the Systick stuff.
                     //  This means that if we want delays, we need to have our own EBTKS_delay_ns()
                     //  All interrupts are re-enabled at the end of release_DMA_request()

  DMA_Preamble(DMA_Target_Address);
  //  /LMAX has just been deasserted, and time is about mid to late Phi 21
  //  /RC is still asserted, and the High byte of the address is on the bus
  //
  //  Start the first read by asserting /RD. Use same timing as /LMAX
  //
  WAIT_WHILE_PHI_1_LOW;
  //
  //  Phi 1 has just gone high
  //  Allowing for assorted overhead, try and place the falling edge of /RD 130 ns after
  //  the rising edge of Phi 1, as seen by Capricorn 1MB1 pin 17
  //  Tweaked with oscilloscope observations.
  //
  CTRL_START_RD_TWEAK;                //  Extremely finely tuned so that the falling edge of /RD will arrive at pin 17 of 1MB1 130 ns after rising edge of Phi 1
                                      //  Tuned 2020_07_14                                                                  DMA_Tweak_5_for_RD_Falling_edge_2020_07_14_132_ns.png
  ASSERT_RD;                          //  /RDX goes low During Phi 1 High
  //SET_T33;                            //  Trigger for timing /RD   matching CLEAR_T33 is in DMA_Read_Burst()
  WAIT_WHILE_PHI_1_HIGH;              
  OUTPUT_DATA_HOLD_TWEAK;             //  Hold time of High address byte after falling edge of Phi 1. The 1MB5 spec indicates a hold time of
                                      //  40 to 150 ns. We are going to target 100 ns, which will be tweaked here and similar code sequences                                 <<<<<<<<<<<<<<<<<<<<<<
  BUS_DIR_FROM_HP;                    //  DIR Low, this also de-asserts /RC . This ends the data phase of Address High byte
  SET_T4_BUS_TO_INPUT;                //  Prep for Read
  //
  //  At this point, we have sent both bytes of the address, and we have initiated a read cycle.
  //  The Teensy data bus is set to input and the data bus buffer/translator (U2) is disabled
  //  Timing wise, we are just before the rising edge of Phi 12
  //
  //  Now we loop until all data has been transfered by DMA
  //  Need to also allow time for the 1MA2 to do refresh. (but only if we are interfering with the DRAM on the main board.
  //
  buffer_index = 0;                  //  Index into the DMA buffer, and also indicates how many bytes we have transfered so far
  //
  //  DMA burst length is limited so that we can allow for some idle cycles for the 1MA2 DRAM memory controller to do refresh
  //
  while((bytecount - buffer_index) > MAX_DMA_BURST_LENGTH)
  {                                  //  We have more than the max burst length still to be completed
    DMA_Read_Burst(&buffer[buffer_index], MAX_DMA_BURST_LENGTH);
    buffer_index += MAX_DMA_BURST_LENGTH;
    for(refresh_count = 0 ; refresh_count < (DMA_BURST_BREAK_CYCLES - 1); refresh_count++)
    {
      WAIT_WHILE_PHI_1_LOW;
      WAIT_WHILE_PHI_1_HIGH;
    }
    WAIT_WHILE_PHI_1_LOW;
    //
    //  Phi 1 has just gone high
    //  Allowing for assorted overhead, try and place the falling
    //  edge of /RD 130 ns after the rising edge of Phi 1, as seen by Capricorn 1MB1 pin 17
    //  Tweaked with oscilloscope observations.
    //
    CTRL_START_RD_TWEAK;               //  Extremely finely tuned so that the falling edge of /RD will arrive at pin 17 of 1MB1 130 ns after rising edge of Phi 1
                                       //  Tuned 2020_07_14                                                                  DMA_Tweak_5_for_RD_Falling_edge_2020_07_14_132_ns.png
    ASSERT_RD;                         //  /RDX goes low During Phi 1 High
    WAIT_WHILE_PHI_1_HIGH;
  }
  //
  //  When we get here, there is at most MAX_DMA_BURST_LENGTH transfers pending
  //
  DMA_Read_Burst(&buffer[buffer_index], bytecount - buffer_index);

  return bytecount;
}


//
//  This is common to both DMA Read and DMA Write.
//
//  These are the steps performed by this function:
//    1) Synchronize to bus timing, so we assert the initial /LMA consistently
//    2) Assert /LMA for the low byte of the address
//    3) Start sending the low byte of the address. DeAssert /LMA
//    4) Assert /LMA for the high byte of the address
//    5) Start sending the high byte of the address. DeAssert /LMA
//    6) Exit.
//
//  PreCharge. The HP-85 busses use precharge prior to data transfers, with the precharge
//  originating in the Capricorn CPU on the main board, and maybe the 1MA8 for the I/O
//  bus. HP used this technique presumably because the custom chips were NMOS technology
//  that has crappy logic high drive. EBTKS uses high speed CMOS drivers, that have the
//  normal ability to both drive High and Low with low impedance. So there does not seem
//  to be any need for precharge activity prior to this board transmiting data on to
//  the bus. This includes DMA addresses and DMA Write data.
//
//  At the time we exit, the high byte of the address is still being sent,
//  /RC is still asserted and /LMA has just been DeAsserted.
//  The Clocks state is approximately Phi 21 high. The next event we care
//  about for both DMA Read and DMA Write is Phi 1 Rising. At this point
//  the activity for Read and Write follow different paths.
//
//  The goal for LMA is a pulse that arives on pin 16 of Capricorn (1MB1) with the falling
//  edge 130 ns after the rising edge of Phi 1. Low pulse width of 1160 ns.
//
//
//  All numbers in [] in comments are references to points to the master "DMA Preamble" Timing Diagram
//
//  Takes 2.79 us from TIME_MARKER_1 to TIME_MARKER_2

static void DMA_Preamble(uint16_t DMA_Target_Address)
{

  uint32_t               low_addr_byte, high_addr_byte;

              //
              //  Phase 1, get everything ready before we pull the trigger, as once we do, timing is critical
              //           Everything happening within DMA code is with interrupts off, so no background
              //           servicing of Pin Edge interrupts, or the activities they support
              //           Update 07/04/2020: No Interrupts of any kind, even mySystick_isr()
              //
              //  Get the address ready for driving onto the bus
              //
              low_addr_byte  = DMA_Target_Address & 0x00FF;
              high_addr_byte = (DMA_Target_Address >> 8) & 0x00FF;
              //
              //  Now make it ready to be written into GPIO_DR_SET_DB0
              //
              low_addr_byte  <<= 16;
              high_addr_byte <<= 16;
              //
              //  Phase 2, From here on everything is very finely tuned to work on Teensy 4.0/4.1 running at 600 MHz
              //
              //  Since this function is called asynchronously, and we want to place the falling
              //  edge of LMA accurately, we might arrive here while Phi 1 is already high, and
              //  we just missed a rising edge that we are looking for
              //
              if(IS_PHI_1_HIGH)
              {
                WAIT_WHILE_PHI_1_HIGH;
              }
              //  Phi 1 is now low (or worst case has just gone high)
              WAIT_WHILE_PHI_1_LOW;
              //SET_T33;                             //  TIME_MARKER_1    From here to exit from this function, it takes 2.808            DMA_preamble_after_sync_Duration_2020_07_14_2808_ns.png
              //
              //  Phi 1 has just gone high
              //  Allowing for assorted overhead, try and place the falling
              //  edge of /LMA 130 ns after the rising edge of Phi 1, as seen by Capricorn pin 16
              //  Tweaked with oscilloscope observations.
              //
/* ADDR Low Byte sequence starts here */
              CTRL_START_LMA_1_TWEAK;               //  Extremely finely tuned so that the falling edge of /LMA will arrive at pin 16 of 1MB1 130 ns after rising edge of Phi 1
                                                    //  Tuned 2020_07_14                                                                  DMA_Tweak_1_for_LMA_Falling_edge_2020_07_14_127_ns.png
/* [01] */    ASSERT_LMA;                           //  /LMAX goes low During Phi 1 High
              WAIT_WHILE_PHI_2_LOW;
              //SET_T33;                              //  Trigger for timing the previous ASSERT_LMA and the next. A twofer
              WAIT_WHILE_PHI_2_HIGH;                //  After this we are just after Phi 2 falling.
                                                    //  Put the low byte of the address on the local bus,
                                                    //  change the direction of the bus buffer (which also assert /RC)
                                                    //  and enable the bus buffer
/* [02] */    DISABLE_BUS_BUFFER_U2;                //  Disable the buffer while we turn the direction around
/* [03] */    SET_T4_BUS_TO_OUTPUT;                 //  Set Teensy's local bus to output (should be no race condition,
                                                    //  since local and we just disabled the external Buffer/Translator)
                                                    //  Use clear/set regs to output the bits we want without touching
                                                    //  any others - not sure if this is the fastest method
                                                    //  This also asserts /RC
              GPIO_DR_CLEAR_DB0 = DATA_BUS_MASK;    //  Clear all 8 data bits
/* [04] */    GPIO_DR_SET_DB0   = low_addr_byte;    //  Put low address byte on the bus.
/* [05] */    BUS_DIR_TO_HP;                        //  DIR high, this also asserts /RC
                                                    //  may need to delay for 1MA8 to let go of driving bus   Need to investigate
/* [06] */    ENABLE_BUS_BUFFER_U2;                 //  !OE low

/* [07] */    //  After transit time, Low address will be on the I/O bus, and then the main board

              //
              //  The address byte remains driven on the I/O bus until 100 ns after the falling edge of Phi 1
              //
              CTRL_END_LMA_TWEAK_3;                 //  After going through the I/O bus cable and 1MA8, plus the delay of the above code,
                                                    //  this generates a 1.160 ns /LMA on the processor bus, measured at the 1MB1, pin 16
                                                    //  Tuned 2020_07_14                                                                  DMA_Tweak_3_for_LMA_Duration_1_2020_07_14_1160_ns.png
/* [08] */    RELEASE_LMA;
              //CLEAR_T33;                            //  Clear the trigger for timing LMA
              //
              //  Do the second LMA pulse, note the first byte of the address is still driven onto the bus, and /RC is still asserted
              //
              WAIT_WHILE_PHI_1_LOW;
/* ADDR High Byte sequence starts here */
              CTRL_START_LMA_2_TWEAK;               //  Extremely finely tuned so that the falling edge of /LMA will arrive at pin 16 of 1MB1 130 ns after rising edge of Phi 1
                                                    //  Tuned 2020_07_14                                                                  DMA_Tweak_2_for_LMA_Falling_edge_2020_07_14_127_ns.png
/* [09] */    ASSERT_LMA;                           //  LMA goes low During Phi 1 High
              WAIT_WHILE_PHI_1_HIGH;
              OUTPUT_DATA_HOLD_TWEAK;               //  Hold time of Low address byte after falling edge of Phi 1
                                                    //  See similar code in DMA_Read_Block() for a description
/* [10] */    BUS_DIR_FROM_HP;                      //  DIR low, this also de-asserts /RC . This ends the data phase of Address Low byte
/* [11] */    DISABLE_BUS_BUFFER_U2;                //  Floats the data bus. This is to avoid contention
              WAIT_WHILE_PHI_2_LOW;
              WAIT_WHILE_PHI_2_HIGH;                //  After this we are just after Phi 2 falling.
                                                    //  Put the high byte of the address on the local bus,      #####
                                                    //  change the direction of the bus buffer (which also assert /RC)
                                                    //  and enable the bus buffer
                                                    //  Use clear/set regs to output the bits we want without touching any others - not sure if this is the fastest method
              GPIO_DR_CLEAR_DB0 = DATA_BUS_MASK;    //  Clear all 8 data bits
/* [12] */    GPIO_DR_SET_DB0   = high_addr_byte;   //  Put high address byte on the bus.                  #####
/* [13] */    BUS_DIR_TO_HP;                        //  DIR high, this also asserts /RC
                                                    //  may need to delay for 1MA8 to let go of driving bus   Need to investigate                              <<<<<<<<<<<<<<<<<<<<<<
/* [14] */    ENABLE_BUS_BUFFER_U2;                 //  !OE low
              //
              //  The address byte remains driven on the I/O bus till the falling edge of Phi 1
              //
              CTRL_END_LMA_TWEAK_4;                 //  After going through the I/O bus cable and 1MA8, plus the delay of the above code,
                                                    //  Tuned 2020_07_14                                                                   DMA_Tweak_4_for_LMA_Duration_2_2020_07_14_1161_ns.png
/* [15] */    RELEASE_LMA;
              //CLEAR_T33;                          //  TIME_MARKER_2

//
//  At the time we exit, the High byte of the address is still being sent,
//  /RC is still asserted, and /LMAX has just been deasserted.
//  Teensy data bus is an output
//  The Clocks state is approximately Phi 21 high. The next event we care
//  about for both DMA Read and DMA Write is Phi 1 Rising. At this point
//  the activity for Read and Write follow different paths.
//

}


//
//  On entry, we are just just before the rising edge of Phi 12, and /RD has been asserted
//
//	The current code cycles /RC for every transaction, but maybe this is not needed,
//	because if all it does is control the data direction in the 1MA8, and we are doing
//	back-to-back transactions, no one else is going to be driving either the I/O bus
//	(EBTKS is the driver) or the main bus (1MA8 is the driver, using what EBTKS is
//	putting on the I/O bus)
//
//  On exit,  we are just after Phi 1 falling edge, and /RD is not asserted
//

static int32_t DMA_Read_Burst(uint8_t buffer[], uint32_t bytecount)
{
  uint32_t               buffer_index;
  uint32_t               data_from_IO_bus;

  buffer_index = 0;

  while(buffer_index < bytecount)
  {
    //
    //  Timing wise, we are just before the rising edge of Phi 12, and /RD has been asserted
    //  for input to this board, and the Teensy Bus I/O pins are configured for input
    //  Starting at Phi 2 going low we enable data from the I/O bus to arrive at the Teensy inputs
    //  The bus buffer/level translator U2 is enabled and set for reading the I/O bus (BUS_DIR_FROM_HP)
    //
    WAIT_WHILE_PHI_2_LOW;
    //CLEAR_T33;                        //  Clear Trigger for timing
    WAIT_WHILE_PHI_2_HIGH;              //  After this we are just after Phi 2 falling.
    CTRL_END_RD_TWEAK;                  //  After going through the I/O bus cable and 1MA8, plus the delay of the above code,
                                        //  this generates a nominal 1.160 ns /RD on the processor bus, sensed on 1MB1 pin 17
                                        //  Tuned 2020_07_14                                                DMA_Tweak_6_for_RD_Duration_2020_07_14_1154_ns.png
    RELEASE_RD;
    //
    //  Due to the pipelined nature of bus transactions, if we need to do more transfers after the current read,
    //  we need to assert /RD for the next transaction, even though we haven't received the data for the current cycle
    //
    if((buffer_index + 1) != bytecount) //  See if there are more reads in the current burst, and if so, start another read
    {                                   //  This is not the last byte, so start another /RD pulse
      WAIT_WHILE_PHI_1_LOW;
      CTRL_START_RD_TWEAK;              //  Extremely finely tuned so that the falling edge of /RD will arrive at pin 17 of 1MB1 130 ns after rising edge of Phi 1
                                        //  Tuned 2020_07_11       DMA_Tweak_4_for_RD_Falling_edge_2020_07_11.png
      ASSERT_RD;                        //  /RD goes low about xxx ns after prior release. Tweak with oscilloscope
    }
    else
    {
      WAIT_WHILE_PHI_1_LOW;             // this is in both branches of the if() so that the other one is not separated from the CTRL_START_TWEAK and ASSERT
    }
    WAIT_WHILE_PHI_1_HIGH;
    data_from_IO_bus = (GPIO_PAD_STATUS_REG_DB0 >> BIT_POSITION_DB0) & 0x000000FFU;     //  Requires that data bits are contiguous and in the right order
    buffer[buffer_index++] = data_from_IO_bus;     //  Save the data that has just been read
  }
  return buffer_index;
}

//
//  Just like DMA Read, but going the other way. We will call it DMA Write
//

int32_t DMA_Write_Block(uint32_t DMA_Target_Address, uint8_t buffer[], uint32_t bytecount)
{

  uint32_t               buffer_index;
  uint8_t                refresh_count;

  if((bytecount == 0) || (buffer == NULL) || (!DMA_Active))
  {
    return 0;        //  Pointless or dangerous call
  }

  __disable_irq();   //  Disable all interrupts while DMA is happening. In particular the Systick stuff.
                     //  This means that if we want delays, we need to have our own EBTKS_delay_ns()
                     //  All interrupts are re-enabled at the end of release_DMA_request()

  DMA_Preamble(DMA_Target_Address);
  //  /LMAX has just been deasserted, and time is about mid to late Phi 21
  //  /RC is still asserted, and the High byte of the address is on the bus
  //
  //  Start the first write by asserting /WR. Use same timing as /LMAX
  //
  WAIT_WHILE_PHI_1_LOW;
  //
  //  Phi 1 has just gone high
  //  Allowing for assorted overhead, try and place the falling edge of /WR 130 ns after
  //  the rising edge of Phi 1, as seen by Capricorn 1MB1 pin 15
  //  Tweaked with oscilloscope observations.
  //
  CTRL_START_WR_TWEAK;           //  Extremely finely tuned so that the falling edge of /WR will arrive at pin 15 of 1MB1 130 ns after rising edge of Phi 1
                                 //  Tuned 2020_07_14                                                                  DMA_Tweak_7_for_WR_Falling_edge_2020_07_14_132_ns.png
  ASSERT_WR;                     //  /WRX goes low During Phi 1 High
  //SET_T33;                       //  Trigger for timing /WR   matching CLEAR_T33 is in DMA_Write_Burst()
  WAIT_WHILE_PHI_1_HIGH;
  OUTPUT_DATA_HOLD_TWEAK;        //  Hold time of High address byte after falling edge of Phi 1. The 1MB5 spec indicates a hold time of
                                 //  40 to 150 ns. We are going to target 100 ns, which will be tweaked here and similar code sequences                                 <<<<<<<<<<<<<<<<<<<<<<
  BUS_DIR_FROM_HP;               //  DIR Low, this also de-asserts /RC . This ends the data phase of Address High byte
  DISABLE_BUS_BUFFER_U2;         //  Floats the data bus. This is to avoid contention, as we are leaving T4 as a driver of the local bus
  //
  //  At this point, we have sent both bytes of the address, and we have initiated a write cycle.
  //  The Teensy data bus is still set to output and the data bus buffer/translator (U2) is disabled, direction is input to T4
  //  Timing wise, we are just before the rising edge of Phi 12
  //
  //  Now we loop until all data has been transfered by DMA
  //  Need to also allow time for the 1MA2 to do refresh. (but only if we are interfering with the DRAM on the main board.
  //
  buffer_index = 0;              //  Index into the DMA buffer, and also indicates how many bytes we have transfered so far
  //
  //  DMA burst length is limited so that we can allow for some idle cycles for the 1MA2 DRAM memory controller to do refresh
  //
  while((bytecount - buffer_index) > MAX_DMA_BURST_LENGTH)
  {  //  We have more than the max burst length still to be completed
    DMA_Write_Burst(&buffer[buffer_index], MAX_DMA_BURST_LENGTH);     //  On exit, we are just after the falling edge of Phi 1, /WRX is not asserted,
                                                                      //  /RC not asserted, U2 disabled, T4 bus is output, I/O bus direction is from HP
    buffer_index += MAX_DMA_BURST_LENGTH;
    for(refresh_count = 0 ; refresh_count < (DMA_BURST_BREAK_CYCLES - 1); refresh_count++)
    {
      WAIT_WHILE_PHI_1_LOW;
      WAIT_WHILE_PHI_1_HIGH;
    }
    WAIT_WHILE_PHI_1_LOW;
    //
    //  Phi 1 has just gone high
    //  Allowing for assorted overhead, try and place the falling
    //  edge of /WR 130 ns after the rising edge of Phi 1, as seen by Capricorn 1MB1 pin 15
    //  Tweaked with oscilloscope observations.
    //
    CTRL_START_WR_TWEAK;               //  Extremely finely tuned so that the falling edge of /WR will arrive at pin 15 of 1MB1 130 ns after rising edge of Phi 1
                                       //  Tuned 2020_07_14                                                                  DMA_Tweak_7_for_WR_Falling_edge_2020_07_14_132_ns.png
    ASSERT_WR;                         //  /WRX goes low During Phi 1 High
    WAIT_WHILE_PHI_1_HIGH;
  }
  //
  //  When we get here, there is at most MAX_DMA_BURST_LENGTH transfers pending
  //
  DMA_Write_Burst(&buffer[buffer_index], bytecount - buffer_index);     //  On exit, we are just after the falling edge of Phi 1, /WRX is not asserted,
                                                                        //  /RC not asserted, U2 disabled, T4 bus is output, I/O bus direction is from HP
  //
  //  Put us in a known state. Set T4 bus to input
  //  The Data Bus buffer/translator U2 is enabled and set for reading the bus
  //
  SET_T4_BUS_TO_INPUT;
  ENABLE_BUS_BUFFER_U2;

  return bytecount;
}

//
//  On entry, we are just just before the rising edge of Phi 12, and /WR has been asserted
//
//  On exit, we are at Phi 1 falling edge + data hold time,
//    /WR is not asserted
//    /RC is not asserted, bus direction is from HP
//    we are finished with the last byte of the burst
//    U2 is disabled
//    T4 Data bus is set to output
//

static int32_t DMA_Write_Burst(uint8_t buffer[], uint32_t bytecount)
{
  uint32_t               buffer_index;
  uint32_t               temp;

  buffer_index = 0;

  while(buffer_index < bytecount)
  {
    //
    //  Timing wise, we are just before the rising edge of Phi 12, and /RD has been asserted
    //  The Data Bus buffer/translator is disabled, but the direction is set correctly
    //  for input to this board, and the Teensy Bus I/O pins are configured for input
    //  Starting at Phi 2 going low we enable data from the I/O bus to arrive at the Teensy inputs
    //
    WAIT_WHILE_PHI_2_LOW;

    //CLEAR_T33;                        //  Clear Trigger for timing
    DISABLE_BUS_BUFFER_U2;                       //  This is probably redundant
    SET_T4_BUS_TO_OUTPUT;                        //  Set Teensy's local bus to output (should be no race condition, since local and we just disabled the external Buffer/Translator)
                                                 //  Use clear/set regs to output the bits we want without touching any others - not sure if this is the fastest method
//    GPIO_DR_CLEAR_DB0 = DATA_BUS_MASK;           //  Clear all 8 data bits
//    GPIO_DR_SET_DB0   = buffer[buffer_index++] << 16;  //  Put the data byte on the bus.                  #####

    temp = GPIO_DR_DB0;
    temp &= (~DATA_BUS_MASK);
    temp |= (buffer[buffer_index++] << 16);
    GPIO_DR_DB0 = temp;

    BUS_DIR_TO_HP;                               //  DIR high, this also asserts /RC

    WAIT_WHILE_PHI_2_HIGH;                       //  After this we are just after Phi 2 falling.
    ENABLE_BUS_BUFFER_U2;                        //  !OE low
    //
    //  The data byte remains driven on the I/O bus until 100 ns after the falling edge of Phi 1
    //
    CTRL_END_WR_TWEAK;                           //  After going through the I/O bus cable and 1MA8, plus the delay of the above code,
                                                 //  this generates a nominal 1.160 ns /WR on the processor bus, sensed on 1MB1 pin 15
                                                 //  Tuned 2020_07_14                                                DMA_Tweak_8_for_WR_Duration_2020_07_14_1157_ns.png

    RELEASE_WR;
    //
    //  Due to the pipelined nature of bus transactions, if we need to do more transfers after the current write,
    //  we need to assert /WR for the next transaction, even though we haven't written the data for the current cycle
    //
    if(buffer_index != bytecount)
    { //  This is not the last byte, so start another /WR pulse
      WAIT_WHILE_PHI_1_LOW;
      CTRL_START_WR_TWEAK;              //  Extremely finely tuned so that the falling edge of /WR will arrive at pin 15 of 1MB1 130 ns after rising edge of Phi 1
      ASSERT_WR;                        //  /WRX goes low During Phi 1 High
    }
    else
    { //  This is the last transfer, so don't issue another /WR
      WAIT_WHILE_PHI_1_LOW;             // this is in both branches of the if() so that the other one is not separated from the CTRL_START_TWEAK and ASSERT
    }
    WAIT_WHILE_PHI_1_HIGH;
    //
    //  Phi 1 falling edge just occured, so our outbound data is sent
    //
    OUTPUT_DATA_HOLD_TWEAK;               //  Hold time of data byte after falling edge of Phi 1
                                          //  See similar code in DMA_Read_Block() for a description
    BUS_DIR_FROM_HP;                      //  DIR low, this also de-asserts /RC . This ends the data phase of the DMA Write Byte
    DISABLE_BUS_BUFFER_U2;                //  Floats the data bus. This is to avoid contention
  }
  //
  //  On exit, we are just after the falling edge of Phi 1, /WRX is not asserted,
  //  /RC is asserted and the last data byte to be written is on the data bus
  //
  return buffer_index;
}

////////////////////////////////////////////////////////////////////////////////  DMA Routines to Read and Write Memory and I/O registers  ////////////////////////////////////////////////////

uint8_t DMA_Peek8(uint32_t address)
{
  uint8_t data;

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  DMA_Read_Block(address , (uint8_t *)&data , 1);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
  //  Serial.printf("DMA_Peek8  %06O  O: %03O  H: %02X  D: %03d\n", address, data, data, data);
  return data;
}

uint16_t DMA_Peek16(uint32_t address)
{
  uint16_t data;

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  DMA_Read_Block(address , (uint8_t *)&data , 2);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release

  //  Serial.printf("DMA_Peek16 %06O  O: %06O  H: %04X  D: %06d\n", address, data, data, data);
  return data;
}

void DMA_Poke8(uint32_t address, uint8_t val)
{

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  DMA_Write_Block(address , (uint8_t *)&val , 1);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
}

void DMA_Poke16(uint32_t address, uint16_t val)
{

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  DMA_Write_Block(address , (uint8_t *)&val , 2);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
}

//
//  Since we are doing DMA (otherwise why call this routine), Pin change interrupts for Phi 1 and Phi 2 are disabled.
//  DMA is released 200 ns after the falling edge of Phi 2
//  Switch Teensy control lines back to inputs. They must already be high.
//  Control buffer needs to be reversed
//  Interrupts need to be re-enabled (do this last) AND do it so that an interrupt is not immediately generated.
//    THIS REQUIRES clearing the Interrupt Pending Register at a strategic time: after Phi 2 Falling edge and way before the
//    rising edge of Phi 1.  Easy Peasy.
//
//  Due to the tight timing requirements, the matching code that acquires DMA ownership of the bus is
//  within the function onPhi_2_Rise()
//

void release_DMA_request(void)
{
  //
  //  We want to consistently release HALT 100 ns after Phi 2 rising. But this routine is called asynchronously, so Phi 2 could be in either state
  //  We are ok with wasting a cycle to get synchronized
  //
  if(IS_PHI_2_HIGH)   // approximately 12.5% chance of this happening
  {
    WAIT_WHILE_PHI_2_HIGH;
  }
  if (IS_PHI_2_LOW)                         //  If Phi 2 is Low, then we wait for it to go high, and get us in sync
  {                                         //  There is a low probability that Phi 2 goes high between this test,
                                            //  and the wait that follows. It could introduce some jitter in the
                                            //  release of a few nanoseconds.
    WAIT_WHILE_PHI_2_LOW;
  }
  delayNanoseconds(30);                     //  Tuned to make the release occur 100 ns after Phi 2 rising.
  RELEASE_HALT;                             //  Release HALT 100 ns after Phi_2 Rising,
  DMA_Active = false;

  //  Now we know Phi 2 is High. Hang around till it goes low, and we will be time aligned with the falling edge of Phi 2
  WAIT_WHILE_PHI_2_HIGH;

  delayNanoseconds(100);                    //  Split the 200 ns delay into two pieces, so we have 100 ns later (see 3 lines below)
  PHI_1_and_2_IMR = 0;                      //  Block any interrupts while DMA is happening. This assumes that the ONLY
                                            //  interrupts that can happen in this GPIO group is Phi 1 and Phi 2 rising edge
  PHI_1_and_2_ISR = PHI_1_and_2_ISR;        //  Clear any set bits (should be none)
  delayNanoseconds(100);                    //  Other half of 200 ns delay. Put here in case there are synchronizer delays after changing
                                            //  ISR/IMR registers.
  NVIC_CLEAR_PENDING(IRQ_GPIO6789);         //  Even though interrupts have been disabled, the NVIC can still set its pending flags
                                            //  thus leading to the undesireable unexpected interrupt when the interrupts are enabled.

  GPIO_DIRECTION_LMA &= (~(BIT_MASK_LMA | BIT_MASK_RD | BIT_MASK_WR));    //  Switch LMA, RD, and WR to Input
                                                                          //  Assumes/requires that LMA, RD, and WR
                                                                          //  are in the same GPIO group.
                                                                          //  They are, GPIO6

  CTRL_DIR_FROM_HP;                         //  Go back to listening to the HP-85 /LMA, /RD, /WR
  NVIC_CLEAR_PENDING(IRQ_GPIO6789);         //  Do it again, just to be sure
  NVIC_ENABLE_IRQ(IRQ_GPIO6789);            //  and re-enable the interrupt controller for these Pin interrupts
  PHI_1_and_2_IMR = (BIT_MASK_PHASE1 | BIT_MASK_PHASE2);   //  Enable Phi 1 and Phi 2 interrupts
  __enable_irq();                           //  Enable all interrupts, now that DMA is complete. Allows USB activity, Serial via USB, SysTick
}

////////////////////////////////////////////////////////////////////////////////  DMA Diagnostic and Tuning routines  /////////////////////////////////////////////////////////////////////////


int   checksum, last_checksum = 5489;
int   test_cycles;
int   test_errors;

static int checksum_calc(uint8_t buffer[] , uint32_t bytes)
{
  uint32_t      i;
  int           local_checksum;

  local_checksum = 0;

  for(i = 0 ; i < bytes ; i++)
  {
    local_checksum += buffer[i];
  }
  return local_checksum;
}

//
//  Run DMA test 1: read 50 bytes from ROM, 1,000,000 times. Start address is 0x0002 , from the first system ROM. The known checksum is 5489
//
//  This takes about 108 seconds if the deliberate delay is commented. Takes 145 seconds with it active.  (the delayMicroseconds(20); )
//
//  The other delay is the print every 50,000 passes, which takes about 550 ms (times 20 ~> 11 seconds of overhead
//

void DMA_Test_1(void)
{
  int   i;
  int   bytes_read;

  test_errors = 0;

  Serial.printf("DMA Read Test 1: Read and check 1,000,000 DMA block reads of 50 bytes each. \n");

  for(test_cycles = 0 ; test_cycles < 1000000 ; test_cycles++ )
  {
    DMA_Request = true;
    while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
    SET_TXD;

    bytes_read = DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 50);     //  Read 50 bytes
    release_DMA_request();
    while(DMA_Active){};      // Wait for release
    CLEAR_TXD;

    checksum = 0;             //  Expected checksum for 50 bytes starting at 0x0002 is 5489 decimal
                              //  Checksumming 50 bytes takes about a few us

    for(i = 0 ; i < bytes_read ; i++)
    {
      checksum += Shared_DMA_Buffer_1[i];
    }

    delayMicroseconds(20);    //  Give the HP-85 processor some bus time, so it isn't completely dead. this will increase
                              //  the test time by about 20%

    if(checksum != last_checksum)
    {
      TOGGLE_LED;
      delay(20);
      test_errors++;
//      for(i = 0 ; i < bytes_read ; i++)
//      {
//        Serial.printf("%03O  ", Shared_DMA_Buffer_1[i]);
//        if((i%16) == 15)
//        {
//          Serial.printf("\n");
//        }
//      }
//      Serial.printf("\n\n");
    }

    if((test_cycles % 50000) == 0)
    {
      Serial.printf("DMA Read Test 1: Completed %-8d cycles\n", test_cycles);
      if(Serial.available())
      {
      	break;
      }
    }

  }

  Serial.printf("DMA Read Test 1 Results: DMA Blocks %-7d  DMA Bytes read    %-8d   Checksum errors %7d\n\n", test_cycles, test_cycles * 50, test_errors);

}

//
//  Exercise various transfer lengths, this is a single shot to be checked with a logic analyzer.
//  intended to walk across the boundaries of the burst lengths, to see if I made an off-by-one error
//

void DMA_Test_2(void)
{

  Serial.printf("DMA Read Test 2: Read the following block lengths: 14, 15, 16, 29, 30, 31\n");

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  SET_TXD;
  DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 14);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
  CLEAR_TXD;
  delayMicroseconds(50);

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  SET_TXD;
  DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 15);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
  CLEAR_TXD;
  delayMicroseconds(50);

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  SET_TXD;
  DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 16);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
  CLEAR_TXD;
  delayMicroseconds(50);

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  SET_TXD;
  DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 29);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
  CLEAR_TXD;
  delayMicroseconds(50);

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  SET_TXD;
  DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 30);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
  CLEAR_TXD;
  delayMicroseconds(50);

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  SET_TXD;
  DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 31);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
  CLEAR_TXD;
  delayMicroseconds(50);

}


//
//  Run DMA test 2: Write 50 bytes to ROM, 1,000,000 times. Start address is 0x0002,
//  from the first system ROM. This should be a safe write, as ROMs will just ignore the write
//

void DMA_Test_3(void)
{
  int   i;
  //int   bytes_written;

  //
  //  Initialize the write data to a counting pattern, ignoring the bottom 3 bits because of how the logic analyzer is connected
  //
  for(i = 0 ; i < 50 ; i++)
  {
    Shared_DMA_Buffer_1[i] = i << 3;
  }

//  Serial.printf("DMA Test data is\n");
//  for(i = 0 ; i < 50 ; i++)
//  {
//    Serial.printf("%03O  ", Shared_DMA_Buffer_1[i]);
//    if((i%16) == 15)
//    {
//      Serial.printf("\n");
//    }
//  }
//  Serial.printf("\n\n");

  Serial.printf("DMA Write Test 1: Write a block of 50 bytes to address 0x0002 onwards. Do it 1,000,000 times\n");

  for(test_cycles = 0 ; test_cycles < 1000000 ; test_cycles++ )
  {
    DMA_Request = true;
    while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
    SET_TXD;

    /* bytes_written = */ DMA_Write_Block(0x00000002, Shared_DMA_Buffer_1, 50);     //  Write 50 bytes
    release_DMA_request();
    while(DMA_Active){};      // Wait for release
    CLEAR_TXD;

    delayMicroseconds(20);    //  Give the HP-85 processor some bus time, so it isn't completely dead. This will increase
                              //  the test time by about 20%

    if((test_cycles % 50000) == 0)
    {
      Serial.printf("DMA Write Test 1: Completed %-8d cycles\n", test_cycles);
      if(Serial.available())
      {
      	break;
      }
    }

  }

  Serial.printf("DMA Write Test 1 Results: DMA Blocks %-7d  DMA Bytes written    %-8d\n\n", test_cycles, test_cycles * 50);

}


//
//  Run DMA test 4: Read and write various data from ROM to RAM and check it
//                  I am assuming that address 0xB000 should be useable if there is no Basic programs loaded,
//                  and this should work for both 16K and 32K bytes RAM systems
//  Repeat 10,000 times
//    A) Read 50 bytes from 0x0002   Known checksum is 5489
//    B) Read 50 bytes from 0xB000 and save it away
//    C) Write 50 bytes 0..49 to 0xB000, and read it back, checking it. Should be 1225
//    D) Write 50 bytes from 0x0002 to 0xB000
//    E) Read 50 bytes from 0xB000 and check the checksum, or just match the bytes
//    F) Restore the 50 bytes that were originally fetched from 0xB000
//    G) Read 50 bytes from 0xB000 and check it matches the fetched copy (phase B).
//
//


void DMA_Test_4(void)
{
  int   i;

  Serial.printf("DMA Read/Write Test 1: Read/Write a block of 50 bytes from/to address 0xB000 onwards. Do it 10,000 times\n");

  for(test_cycles = 0 ; test_cycles < 10000 ; test_cycles++ )
  {
    DMA_Request = true;
    while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
    SET_TXD;

    DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 50);
    if((i = checksum_calc(Shared_DMA_Buffer_1, 50)) != 5489)
    {
      Serial.printf("Error in Phase A %d\n", i);
    }

    DMA_Read_Block(0x0000B000, Shared_DMA_Buffer_2, 50);    //  No test for Phase B

    for(i=0 ; i < 50 ; i++)
    {
      DMA_Write_Block(0x0000B000 + i , (uint8_t *)&i , 1);
    }
    DMA_Read_Block(0x0000B000, Shared_DMA_Buffer_1, 50);
    if((i = checksum_calc(Shared_DMA_Buffer_1, 50)) != 1225)
    {
      Serial.printf("Error in Phase C %d\n", i);
    }

    DMA_Read_Block(0x00000002, Shared_DMA_Buffer_1, 50);
    if((i = checksum_calc(Shared_DMA_Buffer_1, 50)) != 5489)
    {
      Serial.printf("Error in Phase D %d\n", i);
    }
    DMA_Write_Block(0x0000B000,  Shared_DMA_Buffer_1, 50);

    for(i=0 ; i < 50 ; i++)
    {
      Shared_DMA_Buffer_1[i] = 0xAA;
    }
    DMA_Read_Block(0x0000B000, Shared_DMA_Buffer_1, 50);
    if((i = checksum_calc(Shared_DMA_Buffer_1, 50)) != 5489)
    {
      Serial.printf("Error in Phase E %d\n", i);
    }

    DMA_Write_Block(0x0000B000, Shared_DMA_Buffer_2, 50);    //  No test for Phase F

    for(i=0 ; i < 50 ; i++)
    {
      Shared_DMA_Buffer_1[i] = 0x55;
    }
    DMA_Read_Block(0x0000B000, Shared_DMA_Buffer_1, 50);

    for(i=0 ; i < 50 ; i++)
    {
      if(Shared_DMA_Buffer_1[i] != Shared_DMA_Buffer_2[i])
      {
        Serial.printf("Error in Phase G at byte %d\n", i);
      }
    }

    release_DMA_request();
    while(DMA_Active){};      // Wait for release
    CLEAR_TXD;

    delay(1);                 //  Give the HP-85 processor some bus time, so it isn't completely dead.

    if((test_cycles % 500) == 0)
    {
      Serial.printf("DMA Read/Write Test 1: Completed %-8d cycles\n", test_cycles);
      if(Serial.available())
      {
      	break;
      }
    }

  }

  Serial.printf("DMA Read/Write Test 1 Done\n");

}


