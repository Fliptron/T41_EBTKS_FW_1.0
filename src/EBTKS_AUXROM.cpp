//
//  08/08/2020  These Function implement all of the AUXROM functions
//
//  09/11/2020  PMF.  Implemented SDCD, SDCUR$,
//
//  10/17/2020        Fix pervasive errors in how I was handling buffers
//

#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>


#include "Inc_Common_Headers.h"

//
//  USAGE Codes from the AUXROMs
//
                                            //  BASIC Keyword
#define  AUX_USAGE_WROM          (  1)      //  none                                              Write buffer to AUXROM#/ADDR
#define  AUX_USAGE_SDCD          (  2)      //  SDCD path$                                        Change the current SD directory
#define  AUX_USAGE_SDCUR         (  3)      //  SDCUR$                                            Return the current SD directory
#define  AUX_USAGE_SDCAT         (  4)      //  SDCAT [dst$Var, dstSizeVar, dstAttrVar, 0 | 1]    Get a directory listing entry for the current SD path (A.BOPT60=0 for "find first", =1 for "find next")
#define  AUX_USAGE_SDFLUSH       (  5)      //  SDFLUSH file#                                     Flush everything or a specific file#
#define  AUX_USAGE_SDOPEN        (  6)      //  SDOPEN file#, filePath$, mode                     Open an SD file
#define  AUX_USAGE_SDREAD        (  7)      //  SDREAD dst$Var, bytesReadVar, maxBytes, file#     Read an SD file
#define  AUX_USAGE_SDCLOSE       (  8)      //  SDCLOSE file#                                     Close an SD file
#define  AUX_USAGE_SDWRIT        (  9)      //  SDWRITE src$, file#                               Write to an SD file
#define  AUX_USAGE_SDSEEK        ( 10)      //  = SDSEEK(origin#, offset#, file#)                 Seek in an SD file
#define  AUX_USAGE_SDDEL         ( 11)      //  SDDEL fileSpec$                                   Check if mounted disk/tape & error, else delete the file
#define  AUX_USAGE_SDMKDIR       ( 12)      //  SDMKDIR folderName$                               Make a folder (only one level at a time, or error)
#define  AUX_USAGE_SDRMDIR       ( 13)      //  SDRMDIR folderName$                               Remove a folder (error if not empty)
#define  AUX_USAGE_SPF           ( 14)      //  SPF                                               sprintf()
#define  AUX_USAGE_MOUNT         ( 15)      //  MOUNT                                             mount a disk/tape in a unit
#define  AUX_USAGE_UNMOUNT       ( 16)      //  UNMOUNT                                           remove a disk/tape from a unit
#define  AUX_USAGE_FLAGS         ( 17)      //  FLAGS                                             save A.FLAGS to config file
#define  AUX_USAGE_SDREN         ( 18)      //  SDREN                                             Rename a file
#define  AUX_USAGE_DATETIME      ( 19)      //  DATETIME                                          Return Real Time Clock: Year, Month, Day, Seconds since midnight
#define  AUX_USAGE_HELP          ( 20)      //  HELP                                              Provides help
#define  AUX_USAGE_SDMEDIA       ( 21)      //  MEDIA$                                            Returns the file name of the mounted media on a drive
#define  AUX_USAGE_MEMCPY        ( 22)      //  MEMCPY
#define  AUX_USAGE_SETLED        ( 23)      //  SETLED
#define  AUX_USAGE_SDCOPY        ( 24)      //  SDCOPY                                            
#define  AUX_USAGE_SDEOF         ( 25)      //  SDEOF                                             Returns number of characters from current position to the end of the file
#define  AUX_USAGE_SDEXISTS      ( 26)      //  SDEXISTS                                          Returns True (1) if file exists
#define  AUX_USAGE_EBTKSREV      ( 27)      //  EBTKSREV$                                         Returns a revision string, The string is defined at the top of EBTKS.h
#define  AUX_USAGE_RMIDLE        ( 28)      //  none                                              Called from the RMIDLE loop on the HP85. EBTKS can return a KeyCode in AR_Opts[0], and set usage to 1
#define  AUX_USAGE_SDBATCH       ( 29)      //  SDBATCH
#define  AUX_USAGE_BOOT          ( 30)      //  BOOT




//      These Keywords don't have assigned Usage codes as they do not require any EBTKS support functions
//
//  RSECTOR      STMT - RSECTOR dst$Var, sec#, msus$ {read a sector from a LIF disk, dst$Var must be at least 256 bytes long}
//  WSECTOR      STMT - WSECTOR src$, sec#, msus$ {write a sector to a LIF disk, src$ must be 256 bytes}
//  AUXERRN      FUNC - AUXERRN {return last custom error error#}
//  SDATTR       FUNC - a = SDATTR(filePath$) {return bits indicating file attributes, -1 if error}
//  SDSIZE       FUNC - s = SDSIZE(filePath$) {return size of file, or -1 if error}
//
/////////////////////  Hooks to write EBTKS routines without new AUXROM code  //////////////////
//
//  AUXCMD       STMT - AUXCMD buf#, usage#, buf$, opt$
//  	                Errors: 	ARG OUT OF RANGE (system 11D)
//  				                    INVALID PARAMETER (system 89D)
//  				                    STRING OVERFLOW (system 56D)
//  				                    MEMORY OVERFLOW (system 19D)
//  				                    custom AUXROM msg (209D)
//  	                Warning:	NULL DATA (system 7)
//  AUXBUF$(buf#, usage#, buf$, opt$)
//  	For AUXBUF$, a string is returned containing the A.BLENx bytes of A.BUFx.
//
//  AUXOPT$(buf#, usage#, buf$, opt$)
//  	For AUXOPT$, a 16-byte string is returned containing the bytes of A.BOPT00-A.BOPT15.
//
//  For all three of these (AUXCMD, AUXBUF$, AUXOPT$):
//        0-16 bytes of opt$ get written to A.BOPT00-A.BOPT15.
//  		  buf$ gets written to A.BUFx where 'x' is buf#.
//  		  A.BLENx gets set to the length of buf$.
//  		  usage# gets written to A.BUSEx.
//  		  MBx gets set to 1.
//  		  x gets written to HEYEBTKS to send the command.
//  
////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//  HEYEBTKS  0xFFE0/0177740. AUXROM_Mailbox_has_Changed
//  Relative to the base of the I/O area, this is 0340(8) , 0xE0 , 224(10)
//
//  This write only I/O address will have a Maibox/Buffer number written by the AUXROM when there is
//  a new Keyword/Statement that requires EBTKS services
//

void ioWriteAuxROM_Alert(uint8_t val)                 //  This function is running within an ISR, keep it short and fast.
{
  new_AUXROM_Alert        = true;                     //  Let the background Polling loop know we have a function to be processed
  Mailbox_to_be_processed = val;
}

//
// emulated registers - note these run under the interrupt context - keep them short n sweet!
//
//  Return the identification string, 1 character per call
//

static char         identification_string[] = "EBTKS V1.0  2021 (C) Philip Freidin, Russell Bull, Everett Kaser";
static int          ident_string_index = 0;

bool onReadAuxROM_Alert(void)
{
  readData = identification_string[ident_string_index++];
  if (!readData)
  {
    ident_string_index = 0;
  }
  return true;
}

void AUXROM_Poll(void)
{
  //int32_t     param_number;
  //int         i;
  //struct      S_HP85_String_Variable * HP85_String_Pointer;
  //double      param_1_val;
  //int         string_len;
  //uint32_t    string_addr;
  //uint32_t    my_R12;

  if (!new_AUXROM_Alert)
  {
    return;
  }

  // Serial.printf("\n\nBuf 0 address %08lX\n", AUXROM_RAM_Window.as_struct.AR_Buffer_0);
  // Serial.printf("Mailbox_to_be_processed %d\n", Mailbox_to_be_processed);

  p_mailbox = &AUXROM_RAM_Window.as_struct.AR_Mailboxes[Mailbox_to_be_processed];         //  Pointer to the selected primary mailbox for keyword
  p_len     = &AUXROM_RAM_Window.as_struct.AR_Lengths[Mailbox_to_be_processed];           //  Pointer to the selected buffer length
  p_usage   = &AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed];            //  Pointer to the selected buffer usage , and return success/error status
  p_buffer  = &AUXROM_RAM_Window.as_struct.AR_Buffer_0[Mailbox_to_be_processed * 256];    //  Pointer to the selected primary buffer for keyword.

  // if (*p_usage != AUX_USAGE_RMIDLE)
  // {
  //   Serial.printf("p_mailbox %08lX\n", p_mailbox);
  //   Serial.printf("p_len     %08lX\n", p_len);
  //   Serial.printf("p_usage   %08lX\n", p_usage);
  //   Serial.printf("p_buffer  %08lX\n", p_buffer);
  //   Serial.printf("*p_usage  %3d\n",   *p_usage);
  // }

  LOGPRINTF_AUX("AUXROM Function called. Got Mailbox # %d  and Usage %d\n", Mailbox_to_be_processed , *p_usage);

  switch (*p_usage)
  {
    case AUX_USAGE_DATETIME:
      AUXROM_DATETIME();
      break;
    case AUX_USAGE_FLAGS:
      AUXROM_FLAGS();
      break;
    case AUX_USAGE_HELP:
      AUXROM_HELP();
      break;
    case AUX_USAGE_SDCAT:
      AUXROM_SDCAT();
      break;
    case AUX_USAGE_SDCD:
      //Serial.printf("Found SDCD\n");
      //show_mailboxes_and_usage();
      AUXROM_SDCD();
      break;
    case AUX_USAGE_SDCLOSE:
      AUXROM_SDCLOSE();
      break;
    case AUX_USAGE_SDCUR:
      AUXROM_SDCUR();
      break;
    case AUX_USAGE_SDDEL:
      AUXROM_SDDEL();
      break;
    case AUX_USAGE_SDFLUSH:
      AUXROM_SDFLUSH();
      break;
    case AUX_USAGE_SDMEDIA:
      AUXROM_MEDIA();
      break;
    case AUX_USAGE_SDMKDIR:
      AUXROM_SDMKDIR();
      break;
    case AUX_USAGE_MOUNT:
      AUXROM_MOUNT();
      break;
    case AUX_USAGE_SDOPEN:
      AUXROM_SDOPEN();
      break;
    case AUX_USAGE_SPF:
      AUXROM_SPF();
      break;
    case AUX_USAGE_SDREAD:
      AUXROM_SDREAD();
      break;
    case AUX_USAGE_SDREN:
      AUXROM_SDREN();
      break;
    case AUX_USAGE_SDRMDIR:
      AUXROM_SDRMDIR();
      break;
    case AUX_USAGE_SDSEEK:
      AUXROM_SDSEEK();
      break;
    case AUX_USAGE_SDWRIT:
      AUXROM_SDWRITE();
      break;
    case AUX_USAGE_UNMOUNT:
      AUXROM_UNMOUNT();
      break;
    case AUX_USAGE_WROM:
      AUXROM_WROM();
      break;
    case AUX_USAGE_MEMCPY:
      AUXROM_MEMCPY();
      break;
    case AUX_USAGE_SETLED:
      AUXROM_SETLED();
      break;
    case AUX_USAGE_SDCOPY:
      AUXROM_SDCOPY();
      break;
    case AUX_USAGE_SDEOF:
      AUXROM_SDEOF();
      break;
    case AUX_USAGE_SDEXISTS:
      AUXROM_SDEXISTS();
      break;
    case AUX_USAGE_EBTKSREV:
      AUXROM_EBTKSREV();
      break;
    case AUX_USAGE_RMIDLE:
      AUXROM_RMIDLE();
      break;
    case AUX_USAGE_SDBATCH:
      AUXROM_SDBATCH();
      break;
    case AUX_USAGE_BOOT:
      AUXROM_BOOT();
      break;
    default:
      *p_usage = 1;               //  Failure, unrecognized Usage code
  }

  new_AUXROM_Alert = false;
  //show_mailboxes_and_usage();
  *p_mailbox = 0;                 //  Relinquish control of the mailbox

}

//
//  Fetch num_bytes for HP-85 memory. Made difficult because they might be in
//  the built-in DRAM (access via DMA) or they might be in RAM that EBTKS
//  supplies (but only for HP85A)
//
//  The efficient solution would be identify the three scenarios:
//    all in built-in DRAM
//    all in EBTKS memory
//    mixed
//  and then write block transfer for each.
//
//  The expediant (and slower and less bug prone) is just transfer 1 byte at
//  a time and do a test for each. Doing this for now.
//  Actually, probably not that slow, since the overhead here is totally masked by the slow HP85 bus data rate
//
//  #################  does not handle ROMs that EBTKS provides
//  #################  does not handle RAM window in EBTKS ROMs
//

void AUXROM_Fetch_Memory(uint8_t *dest, uint32_t src_addr, uint16_t num_bytes)
{
  while (num_bytes--)
  {
    if (getHP85RamExp())
    {
      //
      //  For HP-85 A, implement 16384 - 256 bytes of RAM, mapped at 0xC000 to 0xFEFF (if enabled)
      //
      if ((src_addr >= HP85A_16K_RAM_module_base_addr) && (src_addr < IO_ADDR))
      {
        *dest++ = HP85A_16K_RAM_module[src_addr++ & 0x3FFFU]; //  Got 85A EBTKS RAM, and address match
        continue;
      }
      //  If we get here, got 85A EBTKS RAM, and address does not match, so must be built-in DRAM,
      //  so just fall into that code
    }
    //
    //  built-in DRAM
    //
    *dest++ = DMA_Peek8(src_addr++);
  }
}

//
//  Store num_bytes into HP-85 memory. Made difficult because they might be in
//  the built-in DRAM (access via DMA) or they might be in RAM that EBTKS
//  supplies (but only for HP85A)
//
//  The efficient solution would be identify the three scenarios:
//    all in built-in DRAM
//    all in EBTKS memory
//    mixed
//  and then write block transfer for each
//
//  The expediant (and slower and less bug prone) is just transfer 1 byte at
//  a time and do a test for each. Doing this for now.
//  Actually, probably not that slow, since the overhead here is totally masked by the slow HP85 bus data rate
//
//  #################  does not handle ROMs that EBTKS provides
//  #################  does not handle RAM window in EBTKS ROMs
//
//  dest_addr is an HP85 memory address. source is a pointer into EBTKS memory
//

void AUXROM_Store_Memory(uint16_t dest_addr, char *source, uint16_t num_bytes)
{
  while (num_bytes--)
  {
    if (getHP85RamExp())
    {
      //
      //  For HP-85 A, implement 16384 - 256 bytes of RAM, mapped at 0xC000 to 0xFEFF (if enabled)
      //
      if ((dest_addr >= HP85A_16K_RAM_module_base_addr) && (dest_addr < IO_ADDR))
      {
        HP85A_16K_RAM_module[dest_addr++ - HP85A_16K_RAM_module_base_addr] = *source++; //  Got 85A EBTKS RAM, and address match
        continue;
      }
      //  If we get here, got 85A EBTKS RAM, and address does not match, so must be built-in DRAM,
      //  so just fall into that code
    }
    //
    //  built-in DRAM
    //
    //Serial.printf("DestAddr %08X  SrcAddr %08X  byte %02X\n", dest_addr, source, *source);
    DMA_Poke8((dest_addr++) & 0x0000FFFF, *source++);
  }
}

//
//  Fetch num_bytes for HP-85 memory which are parameters on the R12 stack
//
//  The parameters to be fetched are on the R12 stack with R12 already in
//  AUXROM_RAM_Window.as_struct.AR_R12_copy . This is a growing to higher
//  addresses stack, and R12 is pointing at the first free byte.
//

void AUXROM_Fetch_Parameters(void *Parameter_Block_XXX, uint16_t num_bytes)
{
  AUXROM_Fetch_Memory((uint8_t *)Parameter_Block_XXX, AUXROM_RAM_Window.as_struct.AR_R12_copy - num_bytes, num_bytes);
}

//
//  Process a Real from the R12 stack. 12 digit sign magnitude mantissa, the exponent is 3 digits
//  in 10's complement format exponent is +499 to -499.
//
//  IEEE 754 Double precision starts to run out of steam at 10E308 and 10E-308 . Some de-normalized
//  numbers extend the range out to about E320 and E-320 but you are no longer playing with 12 digits precision
//  Decided to just clip it at E+307/E-307
//

double cvt_HP85_real_to_IEEE_double(uint8_t number[])
{
  int         exponent;   // 3 digits, 10's complement
  double      result;
  bool        negative_mantisa;
  char        numtext[25];

  if (number[4] == 0377)
  {   //  We have a Tagged Integer
    return (double)cvt_R12_int_to_int32(number);
  }

  //
  //  Handle a 12 digit Real, with an exponent between -499 and +499
  //  IEEE Double only supports 10E308 and 10E-308 , so we can't represent all numbers.
  //  Do our best
  //
  exponent =  (((number[0] & 0x0F)     ) * 1.0);
  exponent += (((number[0] & 0xF0) >> 4) * 10.0);
  exponent += (((number[1] & 0xF0) >> 4) * 100.0);

  //  Serial.printf("10's com exp is %5d ", exponent);      //  Diagnostic

  if (exponent > 499)
  {
    exponent = exponent - 1000;
  }

  //
  //  Limit the exponent, even though this makes the conversion less accurate
  //

  if (exponent > 307) exponent = 307;
  if (exponent < -307) exponent = -307;

  //  Serial.printf("limit exp is %5d ", exponent);      //  Diagnostic

  //
  //  Now deal with the mantissa, which is Sign Magnitude, not 10's complement
  //  Not the prettiest solution, but easy to understand. Convert the BCD nibbles
  //  to text and then let sscanf() sort things out
  //

  negative_mantisa = ((number[1] & 0x0F) == 9);
  numtext[ 0] = negative_mantisa ? '-' : ' ';

  numtext[ 1] = ((number[7] & 0xF0) >> 4) + '0';
  numtext[ 2] = '.';
  numtext[ 3] = ((number[7] & 0x0F)     ) + '0';

  numtext[ 4] = ((number[6] & 0xF0) >> 4) + '0';
  numtext[ 5] = ((number[6] & 0x0F)     ) + '0';

  numtext[ 6] = ((number[5] & 0xF0) >> 4) + '0';
  numtext[ 7] = ((number[5] & 0x0F)     ) + '0';

  numtext[ 8] = ((number[4] & 0xF0) >> 4) + '0';
  numtext[ 9] = ((number[4] & 0x0F)     ) + '0';

  numtext[10] = ((number[3] & 0xF0) >> 4) + '0';
  numtext[11] = ((number[3] & 0x0F)     ) + '0';

  numtext[12] = ((number[2] & 0xF0) >> 4) + '0';
  numtext[13] = ((number[2] & 0x0F)     ) + '0';

  snprintf(&numtext[14], 6, "E%04d", exponent);    //  fills 14..19, including a trailing 0x00
  //  Serial.printf("numtext is [%s] ", numtext);      //  Diagnostic
  sscanf(numtext , "%lf", &result);
  return result;
}

//
//  Process a Tagged Integer from the R12 stack -99999 to 99999 in 10's complement format
//

int32_t cvt_R12_int_to_int32(uint8_t number[])
{
  uint32_t    result;

  result =  (((number[5] & 0x0F)     ) * 1);
  result += (((number[5] & 0xF0) >> 4) * 10);

  result += (((number[6] & 0x0F)     ) * 100);
  result += (((number[6] & 0xF0) >> 4) * 1000);

  result += (((number[7] & 0x0F)     ) * 10000);
  result += (((number[7] & 0xF0) >> 4) * 100000);   //  0 for positive, 9 for 10's complement negative

  if (result > 99999)
  {
    result = result - 1000000;  //  Minus one million because the sign indicator is a 9, so neg numbers will be 9xxxxx
  }
  return result;
}

//
//  This function converts a 32 bit integer to a HP Tagged Integer. Tested and then disabled,
//  because I don't think we need it, given the following function. Although, for numbers in
//  the Tagged Integer range, this would be faster, probably
//
//  Dest points to an 8 byte area that can hold a tagged integer
//
//  Examples of how HP85 numbers are encoded
//                  0                           1                          -1
//    00 00 00 00 FF 00 00 00     00 00 00 00 FF 01 00 00     00 00 00 00 FF 99 99 99
//
//                10                         100                        1000
//    00 00 00 00 FF 10 00 00     00 00 00 00 FF 00 01 00     00 00 00 00 FF 00 10 00
//
//                999                         9999                     99999
//    00 00 00 00 FF 99 09 00     00 00 00 00 FF 99 99 00     00 00 00 00 FF 99 99 09
//
//                -10                        -100                       -1000
//    00 00 00 00 FF 90 99 99     00 00 00 00 FF 00 99 99     00 00 00 00 FF 00 90 99
//
//               -999                        -9999                    -99999
//    00 00 00 00 FF 01 90 99     00 00 00 00 FF 01 00 99     00 00 00 00 FF 01 00 90
//    Sign Digit           ^                           ^                           ^
//
//  Format "[%06d]" on ARM
//          0         1        -1
//    [000000]  [000001]  [-00001]
//
//         10       100      1000
//    [000010]  [000100]  [001000]
//
//        999      9999     99999
//    [000999]  [009999]  [099999]
//
//        -10      -100     -1000
//    [-00010]  [-00100]  [-01000]
//
//       -999     -9999    -99999
//    [-00999]  [-09999]  [-99999]
//
//  Tested with all the above values, and they all converted correctly
//
//void cvt_int32_to_HP85_tagged_integer(uint8_t * dest, int val)
//{
//  char  val_chars[20];
//  bool  negative;
//
//  if ((val<100000) && (val > -100000))
//  {
//    //
//    //  The result can be formatted as a tagged integer
//    //
//    //  Handle negative numbers in 10's complement format
//    //
//    negative = val < 0;
//    if (negative)
//    {
//      val = 100000 + val;
//    }
//    dest += 4;                            //  We know we are doing a tagged integer, so skip the first 4 bytes
//    *dest++ = 0377;                       //  Tag it as an integer
//    sprintf(val_chars, "%06d", val);      //  Convert to decimal
//    *dest++ = (val_chars[5] & 0x0F) | ((val_chars[4] & 0x0F) << 4);
//    *dest++ = (val_chars[3] & 0x0F) | ((val_chars[2] & 0x0F) << 4);
//    *dest++ = (val_chars[1] & 0x0F) | (negative? 0x90 : 0x00);
//    return;
//  }
//}

//
//  This function converts a 64 bit IEEE Double to a HP Real.  While some results could be represented
//  with a Tagged Integer, I don't think that it has to do that. Confirmed, no problem returning integers
//  between -99999 and +99999 as 8 byte Reals (non tagged), HP85 is fine with the results.
//
//  Dest points to an 8 byte area that can hold an 8 byte real
//
//  The conversion was helped by a code example here:
//    https://stackoverflow.com/questions/31331723/how-to-control-the-number-of-exponent-digits-after-e-in-c-printf-e
//
//  Tested with all the above values, and they all converted correctly
//

//                      1 . yyyyyyyyyyy e 0 EEE \0
//                    - 1 . xxxxxxxxxxx e - EEE \0
#define ExpectedSize (1+1+1       +11  +1+1+ 3 + 1)

void cvt_IEEE_double_to_HP85_number(uint8_t * dest, double val)
{
  bool  negative;
  char  buf[ExpectedSize + 10];

  if ((negative = val < 0))
  {
    val = -val;       //  We only want to format positive numbers
  }

  snprintf(buf, sizeof buf, "%.11e", val);
  char *e = strchr(buf, 'e');                           // lucky 'e' not in "Infinity" nor "NaN"
  if (e)
  {
    e++;
    int expo = atoi(e);
    //
    //    But nothing is ever easy. The HP85 DECIMAL Exponent is in 10's complement format
    //
    if (expo < 0)
    {
      expo = 1000 + expo;
    }
    snprintf(e, sizeof buf - (e - buf), "%04d", expo);
  }

//
//  The number is now formatted in buf. See page 3-11 of the HP85 Assembler manual for the Nibble codes
//                      Nibble
//    buf[0 ]   1         M0
//    buf[1 ]   .
//    buf[2 ]   2         M1
//    buf[3 ]   3         M2
//    buf[4 ]   4         M3
//    buf[5 ]   5         M4
//    buf[6 ]   6         M5
//    buf[7 ]   7         M6
//    buf[8 ]   8         M7
//    buf[9 ]   9         M8
//    buf[10]   10        M9
//    buf[11]   11        M10
//    buf[12]   12        M11
//    buf[13]   E
//    buf[14]   - or 0    MS  Mantissa Sign due to above code, always 0 in buf[14]
//    buf[15]   1         E0  most sig digit of exponent, 10's complement format
//    buf[16]   2         E1
//    buf[17]   3         E2  least sig digit of exponent
//    buf[18]
//    buf[19]
//    buf[20]
//
  *dest++ = ((buf[16] & 0x0F) << 4) | (buf[17]        & 0x0F);     //  E1  E2
  *dest++ = ((buf[15] & 0x0F) << 4) | (negative? 0x09 : 0x00);     //  E0  MS
  *dest++ = ((buf[11] & 0x0F) << 4) | (buf[12]        & 0x0F);     //  M10 M11
  *dest++ = ((buf[9]  & 0x0F) << 4) | (buf[10]        & 0x0F);     //  M8  M9
  *dest++ = ((buf[7]  & 0x0F) << 4) | (buf[8]         & 0x0F);     //  M6  M7
  *dest++ = ((buf[5]  & 0x0F) << 4) | (buf[6]         & 0x0F);     //  M4  M5
  *dest++ = ((buf[3]  & 0x0F) << 4) | (buf[4]         & 0x0F);     //  M2  M3
  *dest++ = ((buf[0]  & 0x0F) << 4) | (buf[2]         & 0x0F);     //  M0  M1
}




