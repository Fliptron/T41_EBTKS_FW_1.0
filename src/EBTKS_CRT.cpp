//
//  06/27/2020  These Function were moved from EBTKS.cpp to here.
//
//  07/17/2020  Re-write some functions, and start adding support
//              for safely writing text to the CRT for status messages
//              and menu support
//  05/05/2021  Use get_screenEmu() to disable writes to CRT memory. Use this
//              parameter on HP86/87 CONFIG.TXT files to avoid writing to the
//              HP86/87 CRT memory, which we don't yet support
//
//  05/23/2021  Start integrating Russell's HP86/87 CRT support
//
//  05/26/2021  Found all the ways to shoot myself in the foot.
//              Created the Safe...() functions to try and mitigate the problems
//

#include <Arduino.h>

#include "Inc_Common_Headers.h"

//
//  The CRT memory layout is described in 00085-90444_85_Assembler_ROM_389_pages_1981_11.pdf at PDF page 273 onwards
//  We have a library of functions in EBTKS_CRT.cpp
//  Memory addresses are nibble addresses, but writes and reads are 8 bits on even boundaries: Just means address inc/dec must be by 2
//  Alpha memory starts at address 000000(8) and is 4 pages of 16 lines or 32 characters -> 2048 characters -> 4096 nibbles, thus alpha memory ends at 007777
//  Graphics memory is 256 x 192 pixels packed into 6144 bytes. Nibble address starts at 010000 to 037777
//  So total memory is 16384 nibbles, which we store as 8192 bytes
//
//  For the HP86/87, the video memory is byte oriented and 16k bytes in size
//
//
//

//
//  These definitions are for the HP86 and HP87
//
//  #define HP86_87_CRTSAD  (0177700)       //  CRT START ADDRESS   HP86/87   These 4 are already defined with the other I/O addresses
//  #define HP86_87_CRTBAD  (0177701)       //  CRT BYTE ADDRESS    HP86/87   in EBTKS.H
//  #define HP86_87_CRTSTS  (0177702)       //  CRT STATUS          HP86/87
//  #define HP86_87_CRTDAT  (0177703)       //  CRT DATA            HP86/87
//
//  CRTSTS Read bit functions
//
#define CRTSTS87_GRAPH          (1 << 7)    //  1 = graphics mode, 0 = alpha mode
#define CRTSTS87_MODE           (1 << 6)    //  0 = normal graphics 400 pixels/line/ 1 = graph all 544 pixels/line
#define CRTSTS87_INV            (1 << 5)    //  1 = inverse, 0 = normal
#define CRTSTS87_ALPHA          (1 << 3)    //  1 = 24 lines of alpha (10 pixels per char height), 0 = 16 lines (15 pixels height)
#define CRTSTS87_PWRD           (1 << 2)    //  1 = powerdown,0 = normal operation
#define CRTSTS87_BLANK          (1 << 1)    //  1 = screen blanked. when blanked, you get higher speed memory access
#define CRTSTS87_BUSY           (1 << 0)    //  1 = CRT controller is busy

//
//  These definitions are for the HP83, HP85, and 9915. See HP85 Assembler manual, PDF page 273
//
//  #define CRTSAD          (0177404)       //  CRT START ADDRESS HP85A/B   These 4 are already defined with the other I/O addresses
//  #define CRTBAD          (0177405)       //  CRT BYTE ADDRESS  HP85A/B   in EBTKS.H
//  #define CRTSTS          (0177406)       //  CRT STATUS        HP85A/B
//  #define CRTDAT          (0177407)       //  CRT DATA          HP85A/B
//
//  CRTSTS Read bit functions
//
#define CRTSTS85_GRAPH          (error)     //  Can't read this status
#define CRTSTS85_MODE           (error)     //  There is no mode bit
#define CRTSTS85_INV            (error)     //  There is no invert bit
#define CRTSTS85_ALPHA          (error)     //  Only 1 character height
#define CRTSTS85_PWRD           (error)     //  Can't read this status
#define CRTSTS85_BLANK          (error)     //  There is no blank bit
#define CRTSTS85_BUSY           (1 << 7)    //  1 = CRT controller is busy
#define CRTSTS85_DISPLAY_TIME   (1 << 1)    //  1 = CRT Controller is sending pixels to CRT (not retrace time)
#define CRTSTS85_DATA_READY     (1 << 0)    //  Requested data to be read from CRT RAM is now available.  (we could super duper speed this up by just reading our local copy, if we trust it)

uint8_t vram[16384];                        //  ######   Virtual Graphics memory, to avoid needing Read-Modify-Write. This is used for either { HP83, HP85, 9915} or {HP86, HP87}

volatile uint8_t crtControl = 0;            //  Write to status register stored here. bit 7 == 1 is graphics mode, else char mode
volatile bool writeCRTflag = false;

bool badFlag = false;                       //  Odd/Even flag for Baddr
uint16_t badAddr = 0;

bool sadFlag = false;                       //  Odd/Even flag for CRT start address
uint16_t sadAddr = 0;

bool Is8687 = false;                        //  True if video is HP86/87 else HP85

//
//  01/08/2021 add HP86/87 video R.Bull
//
//  video memory can only be accessed in the retrace time
//
//
//  video memory layout
//  alpha is 0..4319
//  alpha all is 0..0x3FC0    80x204 lines (16 visible I think)
//  graphics normal is 0x10E0..0x3FC0
//  graphics all is 0x10e0..0x3FFF    68 bytes/line (544 pixels). address wraps around to 0
//
#define HP87_87_LAST_ALPHA (4319) //last addr of sad/bad in alpha mode
#define HP86_87_LAST_GPAPH (0x3FC0)

// structure to hold the video subsystem snapshot
typedef struct
{
  uint16_t sadAddr;
  uint16_t badAddr;
  uint8_t  ctrl;
  uint8_t vram[16384];
} video_capt_t;

video_capt_t current_screen;                // contains the current HP85/86/87 screen state
video_capt_t captured_screen;               // the current screen is copied into here when captured

void ioWriteCrtSad(uint8_t val);
void ioWriteCrtBad(uint8_t val);
void ioWriteCrtCtrl(uint8_t val);
void ioWriteCrtDat(uint8_t val);

void ioWrite8687CrtSad(uint8_t val);
void ioWrite8687CrtBad(uint8_t val);
void ioWrite8687CrtCtrl(uint8_t val);
void ioWrite8687CrtDat(uint8_t val);

uint8_t Safe_Read_CRTSTS_with_DMA_Active(void);
bool Safe_CRT_is_Busy_with_DMA_Active(void);
void Safe_Write_CRTBAD_with_DMA_Active(uint16_t badAddr);
void Safe_Write_CRTDAT_with_DMA_Active(uint8_t val);

bool Safe_CRT_is_Busy(void);
void Safe_Write_CRTBAD(uint16_t badAddr);
void Safe_Write_CRTSAD(uint16_t sadAddr);
void Safe_Write_CRTDAT(uint8_t val);

void initCrtEmu();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////     CRT Mirror Support
//
//  Write only 0xFF04/0177404. CRT Start Address . Tells the CRT controller where to start
//  fetching characters to put on the screen. Supports fast vertical scrolling
//  For HP83, HP85, and 9915
//

void ioWriteCrtSad(uint8_t val)                 //  This function is running within an ISR, keep it short and fast.
{
  if (sadFlag)
  {                                             //  If true, we are doing the high byte
    sadAddr |= (uint16_t)val << 8;              //  High byte
    current_screen.sadAddr = sadAddr;
  }
  else
  {                                             //  Low byte
    sadAddr = (uint16_t)val;
  }
  sadFlag = !sadFlag;                           //  Toggle the flag
  TOGGLE_SCOPE_1;
}

//
//  Write only 0xFF05/0177405. Byte address (cursor pos)
//  For HP83, HP85, and 9915
//

void ioWriteCrtBad(uint8_t val)                 //  This function is running within an ISR, keep it short and fast.
{
  if (badFlag)
  {
    badAddr |= (uint16_t)val << 8;              //  High byte
    if (crtControl & 0x80)
    {
      badAddr &= 0x3FFFU;                       //  Graphics mode
    }
    else
    {
      badAddr &= 0x0FFFU;                       //  Alpha mode
    }
    current_screen.badAddr = badAddr;
  }
  else
  {
    badAddr = (uint16_t)val;                    //  Low Byte
  }
  badFlag = !badFlag; //toggle the flag
}

//
//  Bit 7 determines graphics/alpha mode
//  Write only 0xFF06/0177406
//  For HP83, HP85, and 9915
//

void ioWriteCrtCtrl(uint8_t val)                //  This function is running within an ISR, keep it short and fast.
{
  crtControl = val;
  current_screen.ctrl = val;
}

//
//  For us this is write only. This pokes data into the Video RAM, and into our Mirror_Video_RAM
//  Write only 0xFF07/0177407
//  For HP83, HP85, and 9915
//

void ioWriteCrtDat(uint8_t val)                             //  This function is running within an ISR, keep it short and fast.
{
  if (badAddr & 1)                                          //  If addr is ODD - we split nibbles. Does this ever happen in real life?
  {                                                         //  00085-90444 85 Assembler ROM, page 7-110 says top nibble goes to lower nibble address
                                                            //  So if the address is even, we are pointing at the high nibble, and if the address is
                                                            //  odd, we are pointing to the low nibble
    if (get_screenEmu())
    {
      current_screen.vram[badAddr >> 1] &= 0xF0U;             //  Code by RB. Reviewed by PMF 7/17/2020
      current_screen.vram[badAddr >> 1] |= (val >> 4);
      current_screen.vram[(badAddr >> 1) + 1] &= 0x0FU;
      current_screen.vram[(badAddr >> 1) + 1] |= (val << 4);
    }
  }
  else                                                      //  Else Even address is just a byte write
  {
    if (get_screenEmu())
    {
      current_screen.vram[badAddr >> 1] = val;
    }
  }

  badAddr += 2;

  if (crtControl & 0x80U)
  {
    badAddr &= 0x3FFFU;                                     //  Constrain the graphics addr
  }
  else
  {
    badAddr &= 0x0FFFU;                                     //  Constrain the address for alpha
  }
  writeCRTflag = true;                                      //  Flag Mirror_Video_RAM has changed
}

//
//  Write only addr 0xFFC0/0177700
//
void ioWrite8687CrtSad(uint8_t val) //  This function is running within an ISR, keep it short and fast.
{
  if (sadFlag)
  {                                //  If true, we are doing the high byte
    sadAddr |= (uint16_t)val << 8; //  High byte
    current_screen.sadAddr = sadAddr;
  }
  else
  { //  Low byte
    sadAddr = (uint16_t)val;
  }
  sadFlag = !sadFlag; //  Toggle the flag
}
//
//  write only 0xFFC1/0177701. Byte address (cursor pos)
//
void ioWrite8687CrtBad(uint8_t val) //  This function is running within an ISR, keep it short and fast.
{
  if (badFlag)
  {
    badAddr |= (uint16_t)val << 8; //  High byte
    if (crtControl & 0x80)
    {
      badAddr &= 0x3FFFU; //  Graphics mode
    }
    else
    {
      badAddr &= 0x0FFFU; //  Alpha mode
    }
    current_screen.badAddr = badAddr;
  }
  else
  {
    badAddr = (uint16_t)val; //  Low Byte
  }
  badFlag = !badFlag; //toggle the flag
}
//
//  write only 0xFFC2/0177702
//
void ioWrite8687CrtCtrl(uint8_t val) //  This function is running within an ISR, keep it short and fast.
{
  current_screen.ctrl = val;
}
//
//  For us this is write only. This pokes data into the Mirror_Video_RAM
//  Write only 0xFF07/0177407
//
void ioWrite8687CrtDat(uint8_t val) //  This function is running within an ISR, keep it short and fast.
{
  current_screen.vram[badAddr++] = val;
  if (current_screen.ctrl & CRTSTS87_GRAPH)
  {
    badAddr &= 0x3FFFU; //  Constrain the graphics addr
  }
  else
  {
    badAddr &= 0x0FFFU; //  Constrain the address for alpha
  }
  writeCRTflag = true; //  Flag Mirror_Video_RAM has changed
}

//
//  This function uses the appropriate registers depending on machine type to read the CRT Status register
//  DMA must have been previously set up
//

uint8_t Safe_Read_CRTSTS_with_DMA_Active(void)
{
  uint8_t     data;
  if (Is8687)
  {
    DMA_Read_Block(HP86_87_CRTSTS, &data, 1);
  }
  else
  {
    DMA_Read_Block(CRTSTS, &data, 1);
  }
  return data;
}

bool Safe_CRT_is_Busy_with_DMA_Active(void)
{
  uint8_t     data;
  if (Is8687)
  {
    DMA_Read_Block(HP86_87_CRTSTS, &data, 1);
    return ((data & CRTSTS87_BUSY) == CRTSTS87_BUSY);
  }
  else
  {
    DMA_Read_Block(CRTSTS, &data, 1);
    return ((data & CRTSTS85_BUSY) == CRTSTS85_BUSY);
  }
}

bool Safe_CRT_is_Busy(void)
{
  uint8_t     data;
  if (Is8687)
  {
    data = DMA_Peek8(HP86_87_CRTSTS);
    return ((data & CRTSTS87_BUSY) == CRTSTS87_BUSY);
  }
  else
  {
    data = DMA_Peek8(CRTSTS);
    return ((data & CRTSTS85_BUSY) == CRTSTS85_BUSY);
  }
}

//
//  Test the appropriate bit, and then write a 16 bit to CRTBAD
//

void Safe_Write_CRTBAD_with_DMA_Active(uint16_t badAddr)
{
  while (Safe_CRT_is_Busy_with_DMA_Active()) {};
  if (Is8687)
  {
    DMA_Write_Block(HP86_87_CRTBAD , (uint8_t *)&badAddr , 2);
  }
  else
  {
    DMA_Write_Block(CRTBAD , (uint8_t *)&badAddr , 2);
  }
}

void Safe_Write_CRTBAD(uint16_t badAddr)
{
  
  while (Safe_CRT_is_Busy()) {};
  if (Is8687)
  {
    DMA_Poke16(HP86_87_CRTBAD , badAddr);
  }
  else
  {
    DMA_Poke16(CRTBAD , badAddr);
  }
}

void Safe_Write_CRTSAD(uint16_t sadAddr)
{
  
  while (Safe_CRT_is_Busy()) {};
  if (Is8687)
  {
    DMA_Poke16(HP86_87_CRTSAD , sadAddr);
  }
  else
  {
    DMA_Poke16(CRTSAD , sadAddr);
  }
}

void Safe_Write_CRTDAT_with_DMA_Active(uint8_t val)
{
  while (Safe_CRT_is_Busy_with_DMA_Active()) {};
  if (Is8687)
  {
    DMA_Write_Block(HP86_87_CRTDAT , (uint8_t *)&val , 1);
  }
  else
  {
    DMA_Write_Block(CRTDAT , (uint8_t *)&val , 1);
  }
}

void Safe_Write_CRTDAT(uint8_t val)
{
  
  while (Safe_CRT_is_Busy()) {};
  if (Is8687)
  {
    DMA_Poke8(HP86_87_CRTDAT , val);
  }
  else
  {
    DMA_Poke8(CRTDAT , val);
  }
}

void initCrtEmu()
{
  if (get_screenEmu() && get_CRTRemote())
  {
    Serial2.begin(115200);                                  //  Init the serial port to the ESP32
  }

  Is8687 = IS_HP86_OR_HP87;
  if (Is8687)
  {
    setIOWriteFunc(0xc0, &ioWrite8687CrtSad);   //  Base address is 0177700, offset is 0300 from 0177400
    setIOWriteFunc(0xc1, &ioWrite8687CrtBad);
    setIOWriteFunc(0xc2, &ioWrite8687CrtCtrl);
    setIOWriteFunc(0xc3, &ioWrite8687CrtDat);
  }
  else
  {
    setIOWriteFunc(4, &ioWriteCrtSad);          //  Base address is 0177404, offset is 0004 from 0177400
    setIOWriteFunc(5,&ioWriteCrtBad);
    setIOWriteFunc(6,&ioWriteCrtCtrl);
    setIOWriteFunc(7,&ioWriteCrtDat);
  }
}

//
//  For diagnostics, status, and menu support, put a string on the CRT, but do it "invisibly" to the rest
//  of the HP85/86/87, by maintaining badAddr and sadAddr. If we are in graphics mode, just return.
//  If text goes beyond the end of line, the hardware will just go on to the next line which
//  is probably what we want. No check that characters are printable.
//  ######## need to check that the wrap-around at end of line works for 86/87
//
//  Row is 0..15 , Column is 0..31  . Row 0 is top , column 0 is left
//  ######## Need to check HP86/87 support
//
//  NOTE: The Row and Column are relative to the current display, not the absolute address in memory
//        This uses sadAddr, the start address in memory where data is fetched for the top left of the screen
//

void Write_on_CRT_Alpha(uint16_t row, uint16_t column, const char *  text)
{
  uint16_t        badAddr_restore;
  uint16_t        local_badAddr;

  //  If CRT is in Graphics mode, just ignore for now.
  //  Maybe later we will allow writing text to the
  //  Graphics screen (Implies a Character ROM in EBTKS)
  if (crtControl & 0x80)
  {
    return;                     
  } 

  //
  //  Calculate the byte address for the first character of our string, base on the
  //  current screen start address, and the supplied row and column. For example,
  //  if we wanted to display text on the bottom line of the screen, starting at
  //  the 3rd character position, row would be 15, and column would be 2
  //
  badAddr_restore = badAddr;

  if (Is8687)
  {
    local_badAddr = (sadAddr + (column % 80) + (row * 80)) & 0x0FFF;    //  ####  This is not right. Actual max is 010337 == 0x10DF
                                                                        //        so not really ammenable to masking. Will need to do better
                                                                        //        Also, the 010337 is for 53 lines of 80 characters. There is
                                                                        //        ALPHA_ALL mode that supports 204 lines 0f 80 characters,
                                                                        //        16320 bytes of the 16384, leaving the last 64 bytes of
                                                                        //        CRT memory unused. We don't handle this either.
  }
  else
  {
    local_badAddr = (sadAddr + (column & 0x1F) * 2 + (row & 0x3F) * 64 ) & 0x0FFF;    //  for HP85A/B this is a nibble address
  }

  // Serial.printf("Write_on_CRT_Alpha: Row: %3d  Column: %2d  badAddr: 0x%04X  Addr for write: 0x%04X Is8687: %s  string: [%s]",
  //                                         row,       column,         badAddr,        local_badAddr, Is8687 ? "true":"false", text);
  //
  // Serial.flush();
  // delay(50);                                      //  Really make sure message gets to Serial port

  assert_DMA_Request();                           //  Don't keep negotiating for DMA. Do it once, send the string, and release. Makes status checks faster too
  while(!DMA_Active){}                            //  Wait for acknowledgment, and Bus ownership

  Safe_Write_CRTBAD_with_DMA_Active(local_badAddr);
  while (*text)
  {
    Safe_Write_CRTDAT_with_DMA_Active(*text);
    current_screen.vram[local_badAddr>>1] = *text;
    if (!Is8687)
    {
      local_badAddr += 2;
    }
    else
    {
      local_badAddr++;
    }
    text++;
  }
  //
  //  Restore CRTBAD
  //
  Safe_Write_CRTBAD_with_DMA_Active(badAddr_restore);

  release_DMA_request();
  while(DMA_Active){}       // Wait for release

  // Serial.printf(" done\n");
}

//
//  The purpose of this test is to reverse engineer the timing of the busy flag in the CRT controller
//  I am assuming that it may be tied to scan line and vertical retrace timing, but it may be more
//  complex, and dependent also on previous reads or writes to CRT memory
//
//  The following is from the Service ROM
//
//    CRTSAD (177404, WRITE-ONLY) SETS THE DISP START ADDRESS IN CRT RAM
//    CRTBAD (177405, WRITE-ONLY) SETS THE "WRITE ADDRESS" FOR WRITING CHARS/BYTES TO CRT RAM
//    CRTSTS (177406) READ:
//      BIT0 = 1 WHEN DATA READY (FROM READ REQUEST)
//      BIT1 = 1 DISPLAY TIME, = 0 RETRACE TIME
//      BIT2 =
//      BIT3 =
//      BIT4 =
//      BIT5 = ??? (see 67205) ???
//      BIT6 =
//      BIT7 = 1 IF BUSY, 0 IF NOT BUSY
//    CRTSTS (177406) WRITE:
//      BIT0 = 1 TO ISSUE READ REQUEST (CRT PUTS CHAR IN CRTDAT)
//      BIT1 = 1 TO WIPE-OUT CRT, = 0 TO UNWIPE CRT
//      BIT2 = 1 TO POWER-DOWN CRT, = 0 TO POWER-UP CRT
//      BIT3 =
//      BIT4 =
//      BIT5 =
//      BIT6 =
//      BIT7 = 1 FOR GRAPHICS, = 0 FOR ALPHA
//    CRTDAT (177407) READS/WRITES TO ACCESS CRT RAM WHEN CRTSTS-BIT7==0

//
//  Go into DMA mode and for about 6 seconds, read CRTSTS and copy bit 1 (display time) to
//  the SCOPE_1 pin and bit 7 (busy) to the SCOPE_2 pin  for Oscilloscope monitoring and measurement
//
//  Results:  Bit 1, Display Time is a very simple square wave, 16.67 ms cycle time (59.98 Hz), High for 12.85 ms
//            and low for 3.818ms
//
//            Bit 7, Busy appears to be low if nothing is going on.
//
//////  Commented out because I use the function name below for a different investigation of CRT timing
//void CRT_Timing_Test_1(void)
//{
//  int         loops = 1000000;
//  uint8_t     data;
//
//  assert_DMA_Request();
//  while(!DMA_Active){};     // Wait for acknowledgment, and Bus ownership. Also locks out interrupts on EBTKS, so can't do USB serial or SD card stuff
//  while(loops--)
//  {
//    DMA_Read_Block(CRTSTS , &data , 1);
//    if (data & 0x02)
//    {
//      SET_SCOPE_1;
//    }
//    else
//    {
//      CLEAR_SCOPE_1;
//    }
//
//    if (data & 0x80)
//    {
//      TOGGLE_SCOPE_2;           //  While busy, toggle SCOPE_2
//    }
//    else
//    {
//      CLEAR_SCOPE_2;
//    }
//  }
//  release_DMA_request();
//  while(DMA_Active){};      // Wait for release
//}

//
//  How fast can we write characters to the CRT ????
//
//  Results:    Writing to the screen can start about 920 us before the start of the 3.818ms of retrace time
//              and extends till 340 us after the end of the retrace time, for a total of 5 ms or a 30%
//              There are 4 gaps that appear in cosistent places. I bet this is related to horizontal scan time, maybe,
//              or maybe loading up 32 characters for the display (even though we are in retrace???). Or maybe a badly
//              designed DRAM refresh.
//              The Gaps are 100 us wide, and Gap start to Gap start is 1.52 ms.
//              Within the limit of the DMA base writing which takes a few bus cycles to check the busy bit, and a few to write,
//              I can write a new character to the screen memory every 13.6 us
//              So in 1 retrace time of 5 ms, - 4 * 100 us = 4.6 ms , giving approximately best case of 338 characters written
//              and the full 512 charcters of the screen could be written in 2 retrace times.
//
//              Since the idle state of the busy bit is 'not busy' you can initiate a write at any time, but the next write
//              will be delayed till the start of the retrace time. I will also bet the 5 ms window starts at the beginning of the
//              vertical front porch and ends with the end of the vertical back porch.
//
//        Slight change 10/29/2020    Change from toggling SCOPE_2 to just showing BUSY state
//
//  Mode 1    SCOPE_1 (Scope CH 1)  CRTSTS bit 1 , Retrace Time when low, Display time when high
//            SCOPE_2 (Scope CH 2)  CRTSTS bit 7 , Not Busy when low,  Busy when high
//

void CRT_Timing_Test_1(void)
{
  int         loops = 1000000;
  uint8_t     data;

  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  assert_DMA_Request();
  while(!DMA_Active){};     // Wait for acknowledgment, and Bus ownership. Also locks out interrupts on EBTKS, so can't do USB serial or SD card stuff
  while(loops--)
  {
    DMA_Read_Block(CRTSTS , &data , 1);
    if (data & 0x02)
    {
      SET_SCOPE_1;
    }
    else
    {
      CLEAR_SCOPE_1;
    }

    if (data & 0x80)
    {
      SET_SCOPE_2;          // Busy
    }
    else
    {
      CLEAR_SCOPE_2;        // Not busy
      DMA_Write_Block(CRTDAT , (uint8_t *)&"X" , 1);      //  Write an 'X' occasionally
    }
  }
  release_DMA_request();
  while(DMA_Active){}      // Wait for sweet, sweet release
}

static char test_message[] = "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";   // 90 characters

//
//  Writes 209 characters using 2 bursts of 74 and 135 characters following the start of retrace.
//  Takes 2.26 ms, averaging 10.8 us. Does not poll the status bit for each character, only polls for the start of retrace
//

void CRT_Timing_Test_2(void)
{
  int         loops = 1;
  uint8_t     data;
  int         index, count;

  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  assert_DMA_Request();
  while(!DMA_Active){}     // Wait for acknowledgment, and Bus ownership. Also locks out interrupts on EBTKS, so can't do USB serial or SD card stuff
  SET_SCOPE_1;
  while(loops--)
  {
    //
    //  Find start of retrace
    //
    while(1)                //  This loop takes about 6.6 uS (== jitter in finding leading edge of retrace)
    {
      DMA_Read_Block(CRTSTS , &data , 1);
      if (data & 0x02)
      {
        break;  //  we are in Display Time
      }
    }
    while(1)                //  This loop takes about 6.6 uS (== jitter in finding leading edge of retrace)
    {
      DMA_Read_Block(CRTSTS , &data , 1);
      if (!(data & 0x02))
      {
        break;  //  Start of Retrace
      }
    }
    CLEAR_SCOPE_1;
    count = 0;
    index = 0;
    //
    //  These 74 characters take 728 us  (or maybe 719 us)
    //
    SET_SCOPE_2;
    while(count < 74)         //  This loop that puts characters on the screen takes 9.8 us per character
    {
      DMA_Write_Block(CRTDAT, (uint8_t *)&test_message[index++], 1);
      if(index == 32)
      {
        index = 0;
      }
      EBTKS_delay_ns(3000);   // Fails at 1300, works at 1400, Fails at 2000, use 3000, but need to
      count++;                // test all around 2000 to 4000 and find a sweet spot or a point where it always works
    }
    CLEAR_SCOPE_2;

    EBTKS_delay_ns(200000);   //  Skip Blip 1   //  wait 200 us
    SET_SCOPE_2;
    count = 0;
    while(count < 135)         //  This loop that puts characters on the screen takes 9.8 us per character. Does it for 1.32 ms
    {
      DMA_Write_Block(CRTDAT, (uint8_t *)&test_message[index++], 1);
      if(index == 32)
      {
        index = 0;
      }
      EBTKS_delay_ns(3000);   // Fails at 1300, works at 1400, Fails at 2000, use 3000, but need to
      count++;                // test all around 2000 to 4000 and find a sweet spot or a point where it always works
    }
    CLEAR_SCOPE_2;

  }
  release_DMA_request();
  while(DMA_Active){}      // Wait for sweet, sweet release
}

//
//  Writes 209 characters using 2 calls to Write_on_CRT_Alpha, so wil have overhead of negotiating DMA twice
//  and it tests Busy bit for every character.
//  Takes 3.08 ms, averaging 14.7 us.
//  But the code is way cleaner. So this really is the way to go, even though 50% ish slower

void CRT_Timing_Test_3(void)
{
  Write_on_CRT_Alpha(0, 0, "128 charcters 567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"); // 128
  Write_on_CRT_Alpha(4, 0, "81 charcters  5678901234567890123456789012345678901234567890123456789012345678901");                                                // 81
}

//
//  Test Screen Save and Restore
//

void CRT_Timing_Test_4(void)
{
  int       temp;

  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  CRT_capture_screen();
  Serial.printf("Dump of captured_screen Structure\n");
  temp = captured_screen.sadAddr;
  Serial.printf("sadAddr = %d   Col = %d   Row = %d\n", temp, (temp & 0x3F) >> 1 , temp >> 6);
  temp = captured_screen.badAddr;
  Serial.printf("badAddr = %d   Col = %d   Row = %d\n", temp, (temp & 0x3F) >> 1 , temp >> 6);
  Serial.printf("ctrl    = %02X\n", captured_screen.ctrl);
  for (int v = 0; v < 64; v++)
  {
    Serial.printf("Row %2d [", v);
    for (int h = 0; h < 32; h++)
    {
      char c = captured_screen.vram[(v * 32) + h] & 0x7f; //remove the underline (bit 7) for the moment
      if (c < 0x20)
      {
        c = ' '; //unprintables convert to space char
      }
      Serial.printf("%c", c);
    }
    Serial.printf("]\n");
  }

  delay(5000);
  Serial.printf("Starting restore\n");
  CRT_restore_screen();
}

//
//  Test for writing text on HP86 and HP87
//

void CRT_Timing_Test_5(void)
{
  Write_on_CRT_Alpha(0,0,"Text at Row 0 , Column 0");
  Write_on_CRT_Alpha(1,0,"Text at Row 1 , Column 0");
  Write_on_CRT_Alpha(2,0,"79 charcters  56789012345678901234567890123456789012345678901234567890123456789");    // 79
  Write_on_CRT_Alpha(3,0,"80 charcters  567890123456789012345678901234567890123456789012345678901234567890");   // 80
  Write_on_CRT_Alpha(4,0,"81 charcters  5678901234567890123456789012345678901234567890123456789012345678901");    // 81
  Write_on_CRT_Alpha(6,0,"128 charcters 567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678");   // 128
  Write_on_CRT_Alpha(8,0,"That's all folks");
}

void writePixel(int x, int y, int color)
{
  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  if (get_screenEmu())          //  Only support direct writing to the CRT if screenEmu is true. Use this to block
  {                               //  accidentally trying to write to the HP86/87 screen which we don't yet support
    if (x >= 256)
      x = 0;
    if (y >= 192)
      y = 0;

    // calculate write address
    int offs = (x >> 3) + (y * 32);

    while (DMA_Peek8(CRTSTS) & 0x80) {}; //wait until video controller is ready
    DMA_Poke16(CRTBAD, 0x1000U + (offs * 2));

    uint8_t val = vram[offs];     //  this needs to use the other version  #################


    if (color)
    {
      val |= (1 << ((x ^ 7) & 7));
    }
    else
    {
      val &= ~(1 << ((x ^ 7) & 7));
    }

    vram[offs] = val;           //  this needs to use the other version  #################
    while (DMA_Peek8(CRTSTS) & 0x80)
    {
    }; //wait until video controller is ready
    DMA_Poke8(CRTDAT, val);
  }
}

void writeLine(int x0, int y0, int x1, int y1, int color)
{
  int tmp;
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);

  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  if (get_screenEmu())          //  Only support direct writing to the CRT if screenEmu is true. Use this to block
  {                             //  accidentally trying to write to the HP86/87 screen which we don't yet support
    if (steep)
    {
      //swap_int16_t(x0, y0);
      tmp = x0;
      x0 = y0;
      y0 = tmp;

      // _swap_int16_t(x1, y1);
      tmp = x1;
      x1 = y1;
      y1 = tmp;
    }

    if (x0 > x1)
    {
      //_swap_int16_t(x0, x1);
      tmp = x0;
      x0 = x1;
      x1 = tmp;

      //_swap_int16_t(y0, y1);
      tmp = y0;
      y0 = y1;
      y1 = tmp;
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int err = dx / 2;
    int ystep;

    if (y0 < y1)
    {
      ystep = 1;
    }
    else
    {
      ystep = -1;
    }

    for (; x0 <= x1; x0++)
    {
      if (steep)
      {
        writePixel(y0, x0, color);
      }
      else
      {
        writePixel(x0, y0, color);
      }
      err -= dy;
      if (err < 0)
      {
        y0 += ystep;
        err += dx;
      }
    }
  }
}

//
//  @todo Add ANSI cursor addressing so we get a static output on the terminal screen
//

void dumpCrtAlpha(void)
{
  // dump out the alpha CRT data
  char line[65];

  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  Serial.printf("\x1b[H");

  for (int v = 0; v < DUMP_HEIGHT; v++)
  {
    for (int h = 0; h < 32; h++)
    {
      char c = current_screen.vram[(v * 32) + h] & 0x7f; //remove the underline (bit 7) for the moment
      if (c < 0x20)
      {
        c = ' '; //unprintables convert to space char
      }
      line[h] = c;
    }
    line[32] = '\0'; //terminate the string
    Serial.println(line);
  }
  Serial.printf("--------------------------------");
}


//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>  JSON Screen dump support

inline void a3_to_a4(unsigned char *a4, unsigned char *a3)
{
  a4[0] = ( a3[0] & 0xFC) >> 2;
  a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xF0) >> 4);
  a4[2] = ((a3[1] & 0x0F) << 2) + ((a3[2] & 0xC0) >> 6);
  a4[3] =  (a3[2] & 0x3F);
}

int base64_encode(char *output, char *input, int inputLen)
{
  int i = 0, j = 0;
  int encLen = 0;
  unsigned char a3[3];
  unsigned char a4[4];

  while (inputLen--)
  {
    a3[i++] = *(input++);
    if (i == 3)
    {
      a3_to_a4(a4, a3);

      for (i = 0; i < 4; i++)
      {
        output[encLen++] = b64_alphabet[a4[i]];
      }

      i = 0;
    }
  }

  if (i)
  {
    for (j = i; j < 3; j++)
    {
      a3[j] = '\0';
    }

    a3_to_a4(a4, a3);

    for (j = 0; j < i + 1; j++)
    {
      output[encLen++] = b64_alphabet[a4[j]];
    }

    while ((i++ < 3))
    {
      output[encLen++] = '=';
    }
  }
  output[encLen] = '\0';
  return encLen;
}

// void dumpCrtAlphaAsJSON(void)
// {
//   char base64Buff[16384];

//   base64_encode(base64Buff, (char *)current_screen.vram, 2048); //64x32 alpha only
//   Serial.printf("\"crt\":{\"alpha\":\"%s\"}\r\n", base64Buff);
// }

void dumpCrtAlphaAsJSON(uint8_t *frameBuff)
{

  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  //char base64Buff[16384];

  //uint8_t vbuff[512];
  //
  //  copy with wrap around for the alpha buffer
  //  the 'virtual' screen is 32 x 64 lines
  //
  int addr = current_screen.sadAddr / 2;

  for (int a = 0; a < 512; a++)
  {
      //vbuff[a] = current_screen.vram[addr++];
      frameBuff[a] = current_screen.vram[addr++];
      if (addr > 2047)
        {
          addr = 0;
        }
  }
  //base64_encode(base64Buff, (char *)vbuff, 512); //32x16 alpha only
  //Serial2.printf("{\"type\":\"hp85text\",\"image\":\"%s\",\"sum\":%d}\n", base64Buff , crc16(base64Buff,strlen(base64Buff)));
}

//
//  Copy the current video state into captured_screen
//  no synchronisation is done - not sure if it is needed.
//  Time will tell!
//

void CRT_capture_screen(void)
{
  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  memcpy(&captured_screen, &current_screen, sizeof(video_capt_t));
}

//
//  restore the HP85 video state from one previously captured
//  currently we only restore the alpha pages
//  #######  @todo Add HP86/87 support

void CRT_restore_screen(void)
{
  if (Is8687)
  {
    Serial.printf("This function is not yet supported on HP86 or HP87\n");
    return;
  }

  if (get_screenEmu())                                //  Only support direct writing to the CRT if screenEmu is true. Use this to block
  {                                                   //  accidentally trying to write to the HP86/87 screen which we don't yet support
                                                      //  Copy 2k of alpha data back to the HP85 video controller
    while (DMA_Peek8(CRTSTS) & 0x80) {};              //  Wait until video controller is ready
    DMA_Poke16(CRTBAD, 0);
    while (DMA_Peek8(CRTSTS) & 0x80) {};              //  Wait until video controller is ready
    DMA_Poke16(CRTSAD, 0);

    // Serial.printf("CRTBAD and CRTBAD set to 0\n");
    // delay(1000);
    // Serial.printf("Start block DMA\n");         //  no reporting until DMA end, as interrupts are off

    //  Start DMA mode
    assert_DMA_Request();
    while (!DMA_Active)
    {
    } // Wait for acknowledgment, and Bus ownership

    //  Bus is now ours, All interrupts are disabled on Teensy

    for (int ch = 0; ch < 2048; ch++)
      {
      uint8_t data = 0x80;
      while (data & 0x80)
      {
        DMA_Read_Block(CRTSTS,&data, 1);
      }  // Wait until video controller is ready

      DMA_Write_Block(CRTDAT, &captured_screen.vram[ch], 1);
      }

    release_DMA_request();

    while (DMA_Active)
    {
    } //  Wait for release
      //  DMA mode is ended

    // Serial.printf("Block DMA has ended\n");         //  no reporting until DMA end, as interrupts are off
    // delay(1000);
    // Serial.printf("Restoring CRTBAD, CRTSAD, and CRTSTS\n");         //  no reporting until DMA end, as interrupts are off

    while (DMA_Peek8(CRTSTS) & 0x80) {};              //  Wait until video controller is ready
    DMA_Poke16(CRTBAD, captured_screen.badAddr);
    while (DMA_Peek8(CRTSTS) & 0x80) {};              //  Wait until video controller is ready
    DMA_Poke16(CRTSAD, captured_screen.sadAddr);
    DMA_Poke8(CRTSTS,captured_screen.ctrl);
    //
    //  Update what BASIC thinks these variables are
    //
    // DMA_Poke16(CRTBYT, captured_screen.badAddr);
    // DMA_Poke16(CRTRAM, captured_screen.sadAddr);
    // DMA_Poke8(CRTWRS,captured_screen.ctrl);
  }
}

