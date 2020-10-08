//
//  06/27/2020  These Function were moved from EBTKS.cpp to here.
//
//  07/17/2020  Re-write some functions, and start adding support
//              for safely writing text to the CRT for status messages
//              and menu support

#include <Arduino.h>

#include "Inc_Common_Headers.h"

uint8_t vram[8192];                   // Virtual Graphics memory, to avoid needing Read-Modify-Write
uint8_t Mirror_Video_RAM[8192];       //
volatile uint8_t crtControl = 0;      //write to status register stored here. bit 7 == 1 is graphics mode, else char mode
volatile bool writeCRTflag = false;

bool badFlag = false;                 //odd/even flag for Baddr
uint16_t badAddr = 0;

bool sadFlag = false;                 //odd/even flag for CRT start address
uint16_t sadAddr = 0;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////     CRT Mirror Support

//
//  Write only 0xFF04/0177404. CRT Start Address . Tells the CRT controller where to start
//  fetching characters to put on the screen. Supports fast vertical scrolling
//

void ioWriteCrtSad(uint8_t val)                 //  This function is running within an ISR, keep it short and fast.
{
  if (sadFlag)
  {                                             //  If true, we are doing the high byte
    sadAddr |= (uint16_t)val << 8;              //  High byte
  }
  else
  {                                             //  Low byte
    sadAddr = (uint16_t)val;
  }
  sadFlag = !sadFlag;                           //  Toggle the flag
  TOGGLE_T33;
}

//
//  write only 0xFF05/0177405. Byte address (cursor pos)
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
  }
  else
  {
    badAddr = (uint16_t)val;                    //  Low Byte
  }
  badFlag = !badFlag; //toggle the flag
}

//
//  bit 7 determines graphics/alpha mode
//  write only 0xFF06/0177406
//

void ioWriteCrtCtrl(uint8_t val)                //  This function is running within an ISR, keep it short and fast.
{
  crtControl = val;
}

//
//  For us this is write only. This pokes data into the Video RAM, and into our Mirror_Video_RAM
//  write only 0xFF07/0177407
//

void ioWriteCrtDat(uint8_t val)                             //  This function is running within an ISR, keep it short and fast.
{
  if (badAddr & 1)                                          //  If addr is ODD - we split nibbles. Does this ever happen in real life?
  {                                                         //  00085-90444 85 Assembler ROM, page 7-110 says top nibble goes to lower nibble address
                                                            //  So if the address is even, we are pointing at the high nibble, and if the address is
                                                            //  odd, we are pointing to the low nibble
    Mirror_Video_RAM[badAddr >> 1] &= 0xF0U;                //  Code by RB. Reviewed by PMF 7/17/2020
    Mirror_Video_RAM[badAddr >> 1] |= (val >> 4);
    Mirror_Video_RAM[(badAddr >> 1) + 1] &= 0x0FU;
    Mirror_Video_RAM[(badAddr >> 1) + 1] |= (val << 4);
  }
  else                                                      //  Else Even address is just a byte write
  {
    Mirror_Video_RAM[badAddr >> 1] = val;
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


void initCrtEmu(void)
{
  setIOWriteFunc(4,&ioWriteCrtSad); // Address 0xFF04   CRT controller
  setIOWriteFunc(5,&ioWriteCrtBad);
  setIOWriteFunc(6,&ioWriteCrtCtrl);
  setIOWriteFunc(7,&ioWriteCrtDat);
}

//
//  For diagnostics, status, and menu support, put a string on the CRT, but do it "invisibly" to the rest
//  of the HP85, by maintaining badAddr and sadAddr. If we are in graphics mode, just return.
//  If text goes beyond the end of line, the hardware will just go on to the next line which
//  is probably what we want. No check that characters are printable.
//
//  Row is 0..15 , Column is 0..31  . Row 0 is top , column 0 is left
//

void Write_on_CRT_Alpha(uint16_t row, uint16_t column, const char *  text)
{
  uint16_t        badAddr_restore;
  uint16_t        local_badAddr;

  if (crtControl & 0x80)
  {
    return;                     //  CRT is in Graphics mode, so just ignore for now. Maybe later we will allow writing text to the Graphics screen (Implies a Character ROM) 
  }  

  badAddr_restore = badAddr;
  //Serial.printf("WoCA: R=%2d  C=%2d  badAddr = %04x  timeout %6d\n", row, column, badAddr, timeout);

  //
  //  Calculate the byte address for the first character of our string, base on the
  //  current screen start address, and the supplied row and column. For example,
  //  if we wanted to display text on the bottom line of the screen, starting at
  //  the 3rd character position, row would be 15, and column would be 2
  //

  local_badAddr = (sadAddr + (column & 0x1F) * 2 + (row & 0x0F) * 64 ) & 0x0FFF;
  //  Serial.printf("\nWoCA: local_badAddr %04X   ", local_badAddr);

  DMA_Poke16(CRTBAD, local_badAddr);

  //
  //  This code occasionally fails by putting the last character of a line on the first
  //  character of the next line. Which is impossible and makes no sense. Or maybe somehow
  //  CRTSAD is getting corrupted. I could not isolate the problem. Any way among the working
  //  guesses, is that the multiple going in and out of DMA while polling the Busy bit,
  //  and then storing the single character into video memory was stressing something.
  //  So the logically correct code is the next 8 lines of wode that fail randomly.
  //  The alternate code that follows is functionally identical. but since we are not
  //  going in and out of DMA (In just once at the start, Out just once at the end),
  //  the responsiveness is much faster from detecting that the CRT controller is not busy
  //  to when we write to the CRTDAT register
  //
  // while(*text)
  // {
  //   Serial.printf("%02X ", *text);
  //   while(DMA_Peek8(CRTSTS) & 0x80) {};     //  Wait while CRT is Busy, wait
  //   DMA_Poke8 (CRTDAT, *text);
  //   text++;
  // }
  // DMA_Poke16(CRTBAD, badAddr_restore);         //  Restore the CRTBAD register in the CRT controller

  //  See above comments for how this code is a re-implementation of the above 8 lines of code
  //  Alternate approach with 1 long DMA block rather than going in and out of DMA for each character
  //  Do the same as above, but do it directly once in DMA mode. Can't do the Serial.printf() though
  //

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  while(*text)
  {
    while(1)
    {
      uint8_t data;
      DMA_Read_Block(CRTSTS , &data , 1);
      if ((data & 0x80) == 0)
      {
        break;
      }
    }
    //  Busy de-asserted
    DMA_Write_Block(CRTDAT , (uint8_t *)text , 1);
    text++;
  }
  DMA_Write_Block(CRTBAD , (uint8_t *)&badAddr_restore , 2);
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
  //  End of alternate code

}

static char test_message[] = "The Quick Green EBTKS Jumps over the Slow HP-85";

void DMA_Test_5(void)
{
  char  * message_ptr;

  Serial.printf("DMA Text to the screen\n");
  
  message_ptr = test_message;
  
  while(*message_ptr != 0)
  {
    while(DMA_Peek8(CRTSTS) & 0x80)
    {        //  Wait while CRT is Busy
    //   Serial.printf(".");
    }
    //  delayMicroseconds(20);
    DMA_Poke8 (CRTDAT, *message_ptr);
    Serial.printf("%c", *message_ptr);
    message_ptr++;
  }
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
//  the RXD pin and bit 7 (busy) to the TXD pin  for Oscilloscope monitoring and measurement
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
//  DMA_Request = true;
//  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership. Also locks out interrupts on EBTKS, so can't do USB serial or SD card stuff
//  while(loops--)
//  {
//    DMA_Read_Block(CRTSTS , &data , 1);
//    if (data & 0x02)
//    {
//      SET_RXD;
//    }
//    else
//    {
//      CLEAR_RXD;
//    }
//
//    if (data & 0x80)
//    {
//      TOGGLE_TXD;           //  While busy, toggle TXD
//    }
//    else
//    {
//      CLEAR_TXD;
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

void CRT_Timing_Test_1(void)
{
  int         loops = 1000000;
  uint8_t     data;

  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership. Also locks out interrupts on EBTKS, so can't do USB serial or SD card stuff
  while(loops--)
  {
    DMA_Read_Block(CRTSTS , &data , 1);
    if (data & 0x02)
    {
      SET_RXD;
    }
    else
    {
      CLEAR_RXD;
    }

    if (data & 0x80)
    {
      //  If busy, do nothing
    }
    else
    {
      //  If not busy, write an 'X' and do a toggle
      TOGGLE_TXD;
      DMA_Write_Block(CRTDAT , (uint8_t *)&"X" , 1);      //  Write an 'X' occasionally
    }
  }
  release_DMA_request();
  while(DMA_Active){};      // Wait for release
}



void writePixel(int x, int y, int color)
{
  if (x >= 256)
    x = 0;
  if (y >= 192)
    y = 0;

  // calculate write address
  int offs = (x >> 3) + (y * 32);
  while (DMA_Peek8(CRTSTS) & 0x80)
  {
  }; //wait until video controller is ready
  DMA_Poke16(CRTBAD, 0x1000U + (offs * 2));

  uint8_t val = vram[offs];


  if (color)
  {
    val |= (1 << ((x ^ 7) & 7));
  }
  else
  {
    val &= ~(1 << ((x ^ 7) & 7));
  }
  
  vram[offs] = val;
  while (DMA_Peek8(CRTSTS) & 0x80)
  {
  }; //wait until video controller is ready
  DMA_Poke8(CRTDAT, val);
}

void writeLine(int x0, int y0, int x1, int y1, int color)
{
  int tmp;
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
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



//
//  @todo Add ANSI cursor addressing so we get a static output on the terminal screen
//

void dumpCrtAlpha(void)
{
  // dump out the alpha CRT data
  char line[65];

  Serial.printf("\x1b[H");

  for (int v = 0; v < DUMP_HEIGHT; v++)
  {
    for (int h = 0; h < 32; h++)
    {
      char c = Mirror_Video_RAM[(v * 32) + h] & 0x7f; //remove the underline (bit 7) for the moment
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

void dumpCrtAlphaAsJSON(void)
{
  char base64Buff[16384];

  base64_encode(base64Buff, (char *)Mirror_Video_RAM, 4096); //64x32 alpha only
  Serial.printf("\"crt\":{\"alpha\":\"%s\"}\r\n", base64Buff);
}
