//
//  06/30/2020  Assorted Utility functions
//
//  07/04/2020  wrote and calibrated EBTKS_delay_ns()
//
//  07/25/2020  There has been continuous playing with the menus
//              Got Logic Analyzer to work
//
//  09/12/2020  Solved mysterious Serial input bug that randomly corrupted every 8th character, sometimes.
//              See:  FASTRUN void pinChange_isr(void)   __attribute__ ((interrupt ("IRQ")));         in EBTKS_Function_Declarations.h
//                    Teensy_4.0_Notes.txt , look for    __attribute__ ((interrupt ("IRQ")))
//
//

#include <Arduino.h>
#include "Inc_Common_Headers.h"
#include "HpibDisk.h"
#include <strings.h>                //  needed for strcasecmp() and strncasecmp() prototype

#include <TimeLib.h>

void show_RTC(void);

extern HpibDevice *devices[];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Diagnostic Pulse Generation
//
//  SCOPE_1_Pulser generates the specified number of pulses by Toggling the SCOPE_1 pin
//
//  Each pulse comprises
//    SCOPE_1 is toggled immediately on entry
//    wait 5 us
//    SCOPE_1 is toggled again
//    wait 5 us
//
//  So it takes 10 us per pulse
//

void SCOPE_1_Pulser(uint8_t count)
{
  volatile int32_t timer;

  count <<= 1;

  while(count--)
  {
    TOGGLE_SCOPE_1;
    for (timer = 0 ; timer < 333 ; timer++)
    {
      //  Do nothing. With timer, itterations, it takes 5 us. So 15 ns per itteration.
    }
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Precision Delays
//
//  This function provides fairly accurate delays, provided that all interrupts are disabled,
//  and the requested delay is greater than or equal to 70 ns
//  From the algorithm and SCALE_FACTOR of 10, the resolution and uncertainty are both
//  about 10 ns. If you need an ultra precise nano second delay, consider inline code
//  and fine tuning the number of NOPs at the end. Delays of less than 70 ns, probably
//  require a different SCALE_FACTOR , and some NOPs in the while(){} loop.
//  A general solution might try a fine tuning based on the remainder of the divide
//  driving a switch statement that has a bunch of NOPs that fall through to the bottom,
//  without breaks after each case
//

#define FIXED_OVERHEAD    (59)  //  Seems the fixed overhead is close to 59
#define SCALE_FACTOR      (10)

void EBTKS_delay_ns(int32_t count)
{
  volatile int32_t   local_count;

  local_count = count - FIXED_OVERHEAD;
  local_count /= SCALE_FACTOR;
  if (local_count <= 0)
  {
    return;
  }
  while(local_count--) {} ;
  asm volatile("mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" "mov r0, r0\n\t" );

}


////////////////////  test EBTKS_delay_ns()   checked with an Oscilloscope 2020_07_04
//
//  while(1)
//  {
//    __disable_irq();
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(   50); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    53 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(   60); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    49 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(   68); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    49 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(   69); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    71 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(   70); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    71 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(   80); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    81 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(   90); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    91 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(   95); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    91 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(  100); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //    98 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(  200); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //   201 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(  227); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //   221 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns(  318); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //   311 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns( 1000); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //  1001 ns
//    TOGGLE_SCOPE_1; EBTKS_delay_ns( 2000); TOGGLE_SCOPE_1; EBTKS_delay_ns(100);  //  2000 ns
//    __enable_irq();
//    delay(1);
//  }


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  All the menu functions, and the function call table


//
//  All the simple functions are above. The more complex are below, and need function prototypes here
//


void help_0(void);
void help_1(void);
void help_2(void);
void help_3(void);
void help_4(void);
void help_5(void);
void help_6(void);
void help_7(void);
void tape_handle_command_flush(void);
void tape_handle_command_load(void);
void CRT_Timing_Test_1(void);
void CRT_Timing_Test_2(void);
void CRT_Timing_Test_3(void);
void CRT_Timing_Test_4(void);
void diag_sdread_1(void);
void Setup_Logic_analyzer(void);
void Logic_analyzer_go(void);
void Simple_Graphics_Test(void);
void clean_logfile(void);
void proc_addr(void);
void proc_auxint(void);
void just_once_func(void);
void pulse_PWO(void);
void dump_ram_window(void);
void jay_pi(void);
void ulisp(void);
void diag_dir_tapes(void);
void diag_dir_disks(void);
void diag_dir_roms(void);
void diag_dir_root(void);
void list_mount(void);

void show(void);
void dump_keys(bool hp85kbd , bool octal);

struct S_Command_Entry
{
  char  command_name[21];   //  command names must be no more than 20 characters
  void  (*f_ptr)(void);
};

//
//  This table must come after the functions listed to avoid undefined function name errors
//
//  Not in this table because they have parameters:
//      Show
//

struct S_Command_Entry Command_Table[] =
{
  {"help",             help_0},
  {"0",                help_0},
  {"1",                help_1},
  {"2",                help_2},
  {"3",                help_3},
  {"4",                help_4},
  {"5",                help_5},
  {"6",                help_6},
  {"7",                help_7},
  {"tload",            tape_handle_command_load},
  {"dir tapes",        diag_dir_tapes},
  {"dir disks",        diag_dir_disks},
  {"media",            report_media},
  {"dir roms",         diag_dir_roms},
  {"dir root",         diag_dir_root},
  {"crt 1",            CRT_Timing_Test_1},
  {"crt 2",            CRT_Timing_Test_2},
  {"crt 3",            CRT_Timing_Test_3},
  {"crt 4",            CRT_Timing_Test_4},
  {"sdreadtimer",      diag_sdread_1},
  {"la setup",         Setup_Logic_Analyzer},
  {"la go",            Logic_analyzer_go},
  {"addr",             proc_addr},
  {"clean log",        clean_logfile},
  {"pwo",              pulse_PWO},
  {"reset",            pulse_PWO},
  {"dump ram window",  dump_ram_window},                  //  Currently broken
  {"graphics test",    Simple_Graphics_Test},
  {"jay pi",           jay_pi},
  {"auxint",           proc_auxint},
  {"jo",               just_once_func},
//{"ulisp",            ulisp},
  {"TABLE_END",        help_0}                            //  Must be uppercase to mark end of table
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Get string from Serial, with editing
//
//  Not currently used, but here is some Scan Codes / Escape sequences
//                  As seen by VSCode   As Seen by SecureCRT
//  Up Arrow        0xC3 0xA0 0x48      0x1B 0x41
//  Left Arrow      0xC3 0xA0 0x4B      0x1B 0x44
//  Down Arrow      0xC3 0xA0 0x50      0x1B 0x42
//  Right Arrow     0xC3 0xA0 0x4D      0x1B 0x43

//
//  Wait for a serial string, Normally returns true, but returns false if ^C is typed
//

static bool Ctrl_C_seen;

bool wait_for_serial_string(void)
{
  Ctrl_C_seen = false;
  while (!serial_string_available)
  {
    get_serial_string_poll();
    if (Ctrl_C_seen)
    {
      return false;
    }
  }
  return true;
}

void get_serial_string_poll(void)
{
  char  current_char;

  if (serial_string_available)
  {
    LOGPRINTF("Error: get_serial_string_poll() called, but serial_string_available is true\n");
    Ctrl_C_seen = true;     //  Not really a Ctrl-C , but treat it this way to error out of whatever is calling this
    return;
  }

  if (!Serial.available()) return;       //  No new characters available

  while(1)    //  Got at least 1 new character
  {
    //  Serial.printf("\nget_serial_string diag: available chars %2d  current string length %2d  current string [%s]\n%s", Serial.available(), serial_string_length, serial_string, serial_string);
    //  Serial.printf("[%02X]",current_char );    //  Use this to see what scan codes (as seen by VSCode) or escape sequences as seen by SecureCRT for special PC keys.
    current_char = Serial.read();
    switch (current_char)
    {
    //
    //  These characters are valid, even if the buffer is full
    //
      case '\n':    //  New Line (CTRL-J). Just throw it away
        break;
      case '\r':    //  Cariage return is end of string
        serial_string[serial_string_length] = '\0';         //  This should already be true, but do it to be extra safe
        Serial.printf("\n");
        serial_string_available = true;                     //  Return the string, do not include \r or \n characters.
        while(Serial.read() >= 0){};                        //  Remove any remaining characters in the serial channel. This will kill type-ahead, which might not be a good idea
        return;
        break;
      case '\b':                                            //  Backspace. If the string (so far) is not empty, delete the last character, otherwise ignore
        if (serial_string_length > 0)
        {
          serial_string_length--;
          serial_string[serial_string_length] = '\0';
          Serial.printf("\b \b");                           //  Backspace , Space , Backspace
        }
        break;
      case '\x03':                                          //  Ctrl-C.  Flush any current string. This fails with the VSCode terminal because it kils the terminal
        serial_string_length    = 0;
        Serial.printf("  ^C\n");
        Ctrl_C_seen = true;
        break;
      case '\x14':                                          //  Ctrl-T.  Show the current incomplete command
        serial_string[serial_string_length] = '\0';
        Serial.printf("\n%s", serial_string);
        break;
      default:                                              //  If buffer is full, the characteris ignored
        if (serial_string_length >= SERIAL_STRING_MAX_LENGTH)
        {
          break;    //  Ignore character, no room for it
        }
        serial_string[serial_string_length++] = current_char;
        serial_string[serial_string_length] = '\0';
        Serial.printf("%c", current_char);
    }
  if (!Serial.available()) return;                           //  No new characters available
  }
}


void serial_string_used(void)
{
  serial_string_length = 0;
  serial_string_available = false;
  serial_string[0] = '\0';
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Get menu commands from the USB Serial port that is shared with Programming

void Serial_Command_Poll(void)
{
  int   command_index;

  get_serial_string_poll();
  if (!serial_string_available) return;          //  Don't hang around here if there isn't a command to be processed

  //Serial.printf("\nSerial_Command_Poll diag: serial_string_available %1d\n", serial_string_available);
  //Serial.printf(  "                          serial_string_length    %2d  serial_string [%s]   ", serial_string_length, serial_string);
  //delay(2000);
  //
  //  Most commands are fixed strings, but some have parameters, which we special case first.
  //

  if(strncasecmp(serial_string , "show ", 5) == 0)
  {
    show();
    serial_string_used();
    return;
  }

  //
  //  All commands from here on are fixed strings with no parameters
  //

  if (strlen(serial_string) == 0)
  {
    serial_string_used();
    return;    //  Not interested in zero length commands
  }

  command_index = 0;
  while(1)
  {
    //  Serial.printf("1"); delay(100);
    if (strcasecmp(Command_Table[command_index].command_name , "TABLE_END") == 0)
    {
      Serial.printf("\nUnrecognized Command [%s].  Flushing command\n", serial_string);
      help_0();
      serial_string_used();
      return;
    }
    if (strcasecmp(Command_Table[command_index].command_name , serial_string) == 0)
    {
      (* Command_Table[command_index].f_ptr)();
      serial_string_used();
      return;
    }
    command_index++;
  }
}

//
//  Show one of several predefined objects, or a user specified file
//  Predefined are: log, boot, config, mb
//  If the parameter does not match one of the above, treat it as a file path
//

void show(void)
{
  int   size;
  char  next_char;
  int   i;

  //
  //  handle the special cases first: log, boot, config, mb
  //

  if(strcasecmp(serial_string + 5, "log") == 0)      //  Not strncasecmp() so nothing after log
  {
    if (logfile_active)
    {
      logfile.flush();
      Serial.printf("Log file size:   %d\n", size = logfile.size());
      logfile.seek(0);
      while(logfile.read(&next_char, 1) == 1)
      {
        Serial.printf("%c", next_char);   //  We are assuming the log file has text and line endings
      }
      logfile.seek(size);
    }
    else
    {
      Serial.printf("Sorry, there is no Logfile\n");
    }
    return;
  }

  if(strcasecmp(serial_string + 5, "boot") == 0)       //  Not strncasecmp() so nothing after boot
  {
    Serial.printf("%s\n", Serial_Log_Buffer);
    return;
  }

  if(strcasecmp(serial_string + 5, "CRT") == 0)       //  Not strncasecmp() so nothing after CRT
  {
    Serial.printf("%s\n", CRT_Log_Buffer);
    return;
  }

  if(strcasecmp(serial_string + 5, "config") == 0)     //  Not strncasecmp() so nothing after config
  {
    strcpy(serial_string, "show /CONFIG.TXT");    //  Just change to non-shortform. Then fall into
                                                  //  other code, which will see it as a file name
                                                  //  version of the show command
  }

  if(strcasecmp(serial_string + 5, "mb") == 0)         //  Not strncasecmp() so nothing after mb
  {
    Serial.printf("\nFirst 8 mailboxes, usages, lengths, and some buffer\n  #  MB    Usage  Length  Buffer\n");
    for (i = 0 ; i < 8 ; i++)
    {
      Serial.printf("  %1d   %1d   %4d   %4d     ", i, AUXROM_RAM_Window.as_struct.AR_Mailboxes[i], AUXROM_RAM_Window.as_struct.AR_Usages[i], AUXROM_RAM_Window.as_struct.AR_Lengths[i]);
      HexDump_T41_mem((uint32_t)&AUXROM_RAM_Window.as_struct.AR_Buffer_0[i*256], 8, false, true);
    }
    Serial.printf("\nAR_Opts:  ");
    for (i = 0 ; i < 16 ; i++)
    {
      Serial.printf("%02x ", AUXROM_RAM_Window.as_struct.AR_Opts[i]);
    }
    Serial.printf("\n");
    return;
  }

  if(strcasecmp(serial_string + 5, "key85_O") == 0)     //  Not strncasecmp() so nothing after key85_O
  {
    dump_keys(true, true);
    return;
  }

  if(strcasecmp(serial_string + 5, "key85_D") == 0)     //  Not strncasecmp() so nothing after key85_D
  {
    dump_keys(true, false);
    return;
  }

  if(strcasecmp(serial_string + 5, "key87_O") == 0)     //  Not strncasecmp() so nothing after key87_O
  {
    dump_keys(false, true);
    return;
  }

  if(strcasecmp(serial_string + 5, "key87_D") == 0)     //  Not strncasecmp() so nothing after key87_D
  {
    dump_keys(false, false);
    return;
  }

  //
  //  If we get here, we didn't match any of the predefined specials (well, maybe config)
  //  so we will treat the parameter as a filename, including maybe path elements
  //

  if (SD.exists(serial_string + 5))
  {
    File showfile = SD.open(serial_string + 5, FILE_READ);
    if(!showfile)
    {
      Serial.printf("File open for %s failed\n", serial_string + 5);
      return;
    }
    while(showfile.read(&next_char, 1) == 1)
    {
      Serial.printf("%c", next_char);           //  We are assuming the file has text and line endings
    }
    showfile.close();
    return;
  }
  else
  {
    Serial.printf("Sorry, file does not exist\n");
    return;
  }
}

//
//  Dump a region of memory as bytes. Whenever at an address with low 4 bits zero, do a newline and address
//    If show_addr is false, suppress printing addresses, and the newline every 16 bytes
//    If final_nl is false, suppress final newline
//

void HexDump_T41_mem(uint32_t start_address, uint32_t count, bool show_addr, bool final_nl)
{
  if (show_addr)
  {
    Serial.printf("%08X: ", start_address);  
  }
  while(count--)
  {
    Serial.printf("%02X ", *(uint8_t *)start_address++);
    if (((start_address % 16) == 0) && show_addr)
    {
      Serial.printf("\n%08X: ", start_address);
    }
  }
  if (final_nl)
  {
    Serial.printf("\n");
  }
}

void HexDump_HP85_mem(uint32_t start_address, uint32_t count, bool show_addr, bool final_nl)
{
  uint8_t       temp;
  if (show_addr)
  {
    Serial.printf("%08X: ", start_address);  
  }
  while(count--)
  {
    AUXROM_Fetch_Memory(&temp, start_address++, 1);
    Serial.printf("%02X ", temp);
    if (((start_address % 16) == 0) && show_addr)
    {
      Serial.printf("\n%08X: ", start_address);
    }
  }
  if (final_nl)
  {
    Serial.printf("\n");
  }
}

//void dump(uint8_t *dat, int len)
//    {
//    int ndx = 0;
//
//    while (ndx < len)
//        {
//        if ((ndx & 0x0f) == 0)
//            {
//            Serial.printf("\n%04X ", ndx);
//            }
//        Serial.printf("%02X ", dat[ndx]);
//        ndx++;
//        }
//    Serial.printf("\n");
//    }


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Poll for tripple click of Shift key while in Idle
//
////
////  We can recognize that the HP85 is IDLE (CSTAT / R16 == 0) by detecting address 102434 occuring during execution
////  If we are in IDLE, this occurs every 67 us . So with a little fiddling, we can monitor
////
//
//bool is_HP85_idle(void)   &&&&  ####
//{
//  int32_t       loops;      //  This set to take about 100us, so if we are in IDLE, we will catch the RMIDLE address being issued
//
//  loops = 9940;             //  This value was derived by measuring the execution time of this function, and making it 100 us
//  //
//  //  Logic_Analyzer_main_sample is updated in the Phi 1 interrupt handler every bus cycle. As this function is running outside
//  //  of interrupt context, Logic_Analyzer_main_sample is always a valid sample.
//  //
//  while(loops--)
//  {
//    if ((Logic_Analyzer_main_sample & 0x00FFFF00U) == (RMIDLE << 8))
//    {
//      return true;
//    }
//  }
//  return false;
//}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  All the simple commands

void help_0(void)
{
  Serial.printf("\nEBTKS Control commands - not case-sensitive\n\n");
  Serial.printf("0     Help for the help levels\n");
  Serial.printf("1     Help for Tape/Disk commands\n");
  Serial.printf("2     Help for Assorted Utility commands\n");
  Serial.printf("3     Help for Directory Commands\n");
//  Serial.printf("4     Help for Auxiliary programs\n");
  Serial.printf("5     Help for Developers\n");
  Serial.printf("6     Help for Demo\n");
//  Serial.printf("7     Help for Transient commands\n");
  Serial.printf("\n");
}

void help_1(void)
{
  Serial.printf("Commands for the Tape Drive\n");
  Serial.printf("tload         Load a new tape image from SD\n");
  Serial.printf("                 You will be prompted for a file name\n");
//Serial.printf("dload         #Load a new disk image from SD\n");     //  Not yet Implemented
  Serial.printf("media         Show the currently mounted tape and disk media\n");
//Serial.printf("dflush        #Force a disk flush and reload\n");     //  Not yet Implemented
//Serial.printf("dwhich        #Which disk(s) is(are) currently loaded\n");
  Serial.printf("\n");
}

void help_2(void)
{
  //uint16_t    nxtmem;
  //uint16_t    i;

  Serial.printf("Commands for Diagnostic\n");
  Serial.printf("sdreadtimer   Test Reading with different start positions\n");
  Serial.printf("la setup      Set up the logic analyzer\n");
  Serial.printf("la go         Start the logic analyzer\n");
  Serial.printf("addr          Instantly show where HP85 is executing\n");
  Serial.printf("show -----    Show commands have a parameter after exactly 1 space\n");
  Serial.printf("     log      Show the System Logfile\n");
  Serial.printf("     boot     Show the messages from the boot process\n");
  Serial.printf("     CRT      Show the messages sent to the CRT at startup\n");
  Serial.printf("     config   Show the CONFIG.TXT file\n");
  Serial.printf("     mb       Display current mailboxes and related data\n");
  Serial.printf("     key85_O  Display HP85 Special Keys in Octal\n");
  Serial.printf("     key85_D  Display HP85 Special Keys in Decimal\n");
  Serial.printf("     key87_O  Display HP87 Special Keys in Octal\n");
  Serial.printf("     key87_D  Display HP87 Special Keys in Decimal\n");
  Serial.printf("     other    Anything else is a file name path\n");
  Serial.printf("clean log     Clean the Logfile on the SD Card\n");
  Serial.printf("pwo           Pulse PWO, resetting HP85 and EBTKS\n");
  
//Serial.printf("dump ram window Start(8) Len(8)   Dump RAM in ROM window\n");                          //  Currently broken because of parsing
//Serial.printf("reset #Reset HP85 and EBTKS\n");
  Serial.printf("\n");
  //
  //  test code
  //
  //Write_on_CRT_Alpha(14, 0, (uint8_t *)"This is test text being sent to the CRT");
  //
  //  see what is at NXTMEM
  //
  // nxtmem = DMA_Peek16(NXTMEM);
  // Serial.printf("NXTMEM is:  %06o\n", nxtmem);
  // for (i = 0 ; i < 32 ; i++)
  // {
  //   Serial.printf("%03O  ", DMA_Peek8(nxtmem++));
  //   if ((i%16) == 15)
  //   {
  //     Serial.printf("\n");
  //   }
  // }
  // Serial.printf("\n\n");
}

void help_3(void)
{
  Serial.printf("Directory Commands\n");
//Serial.printf("cfgrom        #Select active roms\n");      //  Not yet Implemented
  Serial.printf("dir tapes     Directory of available tapes\n");
  Serial.printf("dir disks     Directory of available disks\n");
  Serial.printf("dir roms      Directory of available ROMs\n");
  Serial.printf("dir root      Directory of available ROMs\n");
//Serial.printf("screen        #Screen emulation\n");        //  Not yet Implemented
//Serial.printf("keyboard      #Keyboard emulation\n");      //  Not yet Implemented
  Serial.printf("\n");
  show_RTC();
}

void help_4(void)
{
  Serial.printf("Comands for Auxiliary programs\n");
//Serial.printf("cpm           #CP/M operating system\n");                           //  Not yet Implemented
//Serial.printf("ulisp         #uLisp interpreter\n");
//Serial.printf("python        #uPython\n");                                         //  Not yet Implemented
//Serial.printf("pdp8-os8      #PDP-8E inc Fortran-II and IV, Basic, Focal\n");      //  Not yet Implemented
//Serial.printf("cobol         #COBOL\n");                                           //  Not yet Implemented
//Serial.printf("fasthp85      #Turbo HP-85\n");                                     //  Not yet Implemented
  Serial.printf("\n");
}

void help_5(void)
{
  Serial.printf("Commands for Developers (mostly Philip)\n");
  Serial.printf("crt 1         Try and understand CRT Busy status timing\n");
  Serial.printf("crt 2         Fast CRT Write Experiments\n");
  Serial.printf("crt 3         Normal CRT Write Experiments\n");
  Serial.printf("crt 4         Test screen Save and Restore\n");
  Serial.printf("\n");
}

void help_6(void)
{
  Serial.printf("Commands for Demo\n");
  Serial.printf("graphics test  Set graphics mode first\n");
  Serial.printf("jay pi         Jay's Pi calculator running on Teensy\n");
  Serial.printf("\n");
}

void help_7(void)
{
  Serial.printf("Transient commands\n");
//Serial.printf("auxint\n");                //  Implemented, but probably not useful
//Serial.printf("jo\n");                    //  Implemented, but probably not useful
  Serial.printf("\n");
}

void diag_dir_path(const char * path)
{
  char      date[20], file_size[20];
  uint32_t  temp_uint;

  if (!LineAtATime_ls_Init_SDCAT((char *)path))             //  This is where the whole directory is listed into a buffer
  {                                                         //  Failed to do a listing of the current directory
    Serial.printf("Couldn't initialize read directory on SD card\n");
  }
  while(1)                                                  //  keep looping till we run out of entries
  {
    if (!LineAtATime_ls_Next_SDCAT())
    {   //  No more directory lines
      Serial.printf("\n");
      return;
    }
    strlcpy(date, dir_line, 17);
    strlcpy(file_size, &dir_line[16], 12);                                                //  Get the size of the file. Still needs some processing
    temp_uint = atoi(file_size);
    Serial.printf("%-20s   %10d   %s\n", &dir_line[28], temp_uint, date);                      //  filename,  size,  date & time
  }
  
}

void diag_dir_tapes(void)
{
  diag_dir_path("/tapes/");
}

void diag_dir_root(void)
{
  diag_dir_path("/");
}

void diag_dir_disks(void)
{
  diag_dir_path("/disks/");
}

void diag_dir_roms(void)
{
  diag_dir_path("/roms85/");
}

//
//  Explore the time it takes to do SDREADs of less than a sector at a time
//
//      need to circle back to this: ifstream sdin("getline.txt");
//        which supports:  } else if (sdin.eof()) {
//        found in: G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_1.0\.pio\libdeps\teensy41\SdFat\examples\examplesV1\getline\getline.ino
//

void diag_sdread_1(void)
{
  uint32_t    Start_time;
  uint32_t    Start_time_total;
  uint32_t    Stop_time;
  uint32_t    file_offset;
  uint8_t     buffer[256];
  uint32_t    bytes_read;

  Serial.printf("\n\n\nTest pass 1: reading 256 bytes from a file with varying start positions\n");
  Serial.printf("Test pass 1: Using readBytes() and default timeout\n");
  File testfile = SD.open("/help85/00/85_INDEX.txt", FILE_READ);
  if(!testfile)
  {
    Serial.printf("File open for /help85/85_INDEX.txt failed\n");
    return;
  }
  file_offset = 0;
  testfile.setTimeout(1000);        //  Default timeout is 1000 ms
  Stop_time = Start_time_total = systick_millis_count;
  while(testfile.available32())
  {
    Start_time = systick_millis_count;
    if(!testfile.seekSet(file_offset))
    {
      Serial.printf("seekSet failed with file_offset of %d\n", file_offset);
      return;
    }
    bytes_read = testfile.readBytes(buffer, 256);
    Stop_time = systick_millis_count;
    Serial.printf("Pos: %4d  Bytes read: %4d  Duration: %5d ms\n", file_offset, bytes_read, Stop_time - Start_time);
    file_offset += 23;
  }
  Serial.printf("Total time for test is %5d ms\n", Stop_time - Start_time_total);
  Serial.printf("Test pass 1 finished, no more characters\n\n");
  testfile.close();
//
//
//
  Serial.printf("\n\n\nTest pass 2: reading 256 bytes from a file with varying start positions\n");
  Serial.printf("Test pass 2: Using readBytes() and timeout set to 0\n");
  testfile = SD.open("/help85/00/85_INDEX.txt", FILE_READ);
  if(!testfile)
  {
    Serial.printf("File open for /help85/85_INDEX.txt failed\n");
    return;
  }
  file_offset = 0;
  testfile.setTimeout(0);
  Stop_time = Start_time_total = systick_millis_count;
  while(testfile.available32())
  {
    Start_time = systick_millis_count;
    if(!testfile.seekSet(file_offset))
    {
      Serial.printf("seekSet failed with file_offset of %d\n", file_offset);
      return;
    }
    bytes_read = testfile.readBytes(buffer, 256);
    Stop_time = systick_millis_count;
    Serial.printf("Pos: %4d  Bytes read: %4d  Duration: %5d ms\n", file_offset, bytes_read, Stop_time - Start_time);
    file_offset += 23;
  }
  Serial.printf("Total time for test is %5d ms\n", Stop_time - Start_time_total);
  Serial.printf("Test pass 2 finished, no more characters\n\n");
  testfile.close();
//
//
//
  Serial.printf("\n\n\nTest pass 3: reading 256 bytes from a file with varying start positions\n");
  Serial.printf("Test pass 3: Using read() and timeout set to 1000 (but it should have no effect)\n");
  testfile = SD.open("/help85/00/85_INDEX.txt", FILE_READ);
  if(!testfile)
  {
    Serial.printf("File open for /help85/85_INDEX.txt failed\n");
    return;
  }
  file_offset = 0;
  testfile.setTimeout(1000);        //  Default timeout is 1000 ms (but it should have no effect)
  Stop_time = Start_time_total = systick_millis_count;
  while(testfile.available32())
  {
    Start_time = systick_millis_count;
    if(!testfile.seekSet(file_offset))
    {
      Serial.printf("seekSet failed with file_offset of %d\n", file_offset);
      return;
    }
    bytes_read = testfile.read(buffer, 256);
    Stop_time = systick_millis_count;
    Serial.printf("Pos: %4d  Bytes read: %4d  Duration: %5d ms\n", file_offset, bytes_read, Stop_time - Start_time);
    file_offset += 23;
  }
  Serial.printf("Total time for test is %5d ms\n", Stop_time - Start_time_total);
  Serial.printf("Test pass 3 finished, no more characters\n\n");
  testfile.close();

}

////
////  Diagnostic to figure out why report_media() was crashing.  Don't know yet
////
//
//void dump_devices_array(void)
//{
//  int i,j,k;
//  k=0;
//
//  Serial.printf("devices[32], pointers to HpibDevice classes\n");
//  for (i = 0 ; i < 4 ; i++)
//  {
//    for (j = 0 ; j < 8 ; j++)
//    {
//      Serial.printf("%08X  ", devices[k]); 
//      k++;
//    }
//    Serial.printf("\n");
//  }
//  Serial.printf("\n");
//  Serial.flush();
//  delay(1000);
//}

void report_media(void)
{
  int         device;
  int         disknum;
  char        *filename;
  int         HPIB_Select = get_Select_Code();

  Serial.printf("Currently mounted virtual drives\n msu   File Path\n");
  for (device = 0 ; device < NUM_HPIB_DEVICES ; device++)
  {
    if (devices[device] && devices[device]->isType(HPDEV_DISK))
    {
      for (disknum = 0 ; disknum < 4 ; disknum++)
      {
        filename = static_cast<HpibDisk *>(devices[device])->getFilename(disknum);
        if (filename)
        {
          Serial.printf(":D%d%d%d  %s\n", HPIB_Select, device, disknum, filename);
        }
      }
    }
    if (devices[device] && devices[device]->isType(HPDEV_PRT))
    {
      filename = devices[device]->getFilename(0);
      if (filename)
      {
        Serial.printf("%d%.2d  %s\n", HPIB_Select, device, filename);
      }
    }
  }
  filename = tape.getFile();
  Serial.printf(":T     %s\n", filename);
}


void proc_addr(void)
{
//  int i = 16;
//  
//  Serial.printf("Random sampling, sequential in time but not necessarily adjacent cycles\n");
//  
//  while(i--)
//  {
//    Serial.printf("ROM: %03o(8) ADDR: %04X/%06o\n", rselec, addReg, addReg);
//  }

  Logic_Analyzer_State = ANALYZER_IDLE;
  Logic_Analyzer_Data_index = 0;
  Logic_Analyzer_Valid_Samples   = 0;
  Logic_Analyzer_Trigger_Value_1 = 0;   //  trigger on anything
  Logic_Analyzer_Trigger_Value_2 = 0;
  Logic_Analyzer_Trigger_Mask_1  = 0;   //  trigger on anything
  Logic_Analyzer_Trigger_Mask_2  = 0;   //  trigger on anything
  Logic_Analyzer_Index_of_Trigger= -1;                             //  A negative value means that if we display the buffer without a trigger event (time out) we won't display the Trigger message
  Logic_Analyzer_Event_Count_Init= 1;
  Logic_Analyzer_Event_Count     = 1;
  Logic_Analyzer_Triggered       = false;
  Logic_Analyzer_Current_Buffer_Length = 32;
  Logic_Analyzer_Current_Index_Mask  = 0x1F;
  Logic_Analyzer_Pre_Trigger_Samples = 16;
  Logic_Analyzer_Samples_Till_Done   = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;
  Logic_Analyzer_Valid_Samples_1_second_ago = -100000;
  Ctrl_C_seen = false;

  Logic_analyzer_go();

}

void Setup_Boot_Time_Logic_Analyzer(void)
{
  //
  //  Setup the Boot Time Logic Analyzer settings in these initializers. Same value format as the serial terminal.
  //

  char boot_time_wr       = 1;        //  Asserted values are 0
  char boot_time_rd       = 1;
  char boot_time_lma      = 1;
  char boot_time_wr_mask  = 0;        //  Mask == 1 to enable
  char boot_time_rd_mask  = 0;
  char boot_time_lma_mask = 0;

  unsigned int  boot_time_address         = 0000072;      //  Octal address
  unsigned int  boot_time_address_mask    = 0177777;      //  Octal address mask

  unsigned int  boot_time_data            = 0000;         //  Octal data
  unsigned int  boot_time_data_mask       = 0000;         //  Octal data mask

  unsigned int  boot_time_rselec          = 0000;         //  Octal rselec
  unsigned int  boot_time_rselec_mask     = 0000;         //  Octal rselec mask

  unsigned int  boot_time_buffer_length   = 1024;         //  Must be power of two.               This is not checked, so get it right
  unsigned int  boot_time_pre_trigger     =  512;         //  Number of pre-trigger samples.      This is not checked, must be greater than 2, less than (boot_time_buffer_length - 2)
  unsigned int  boot_time_event_count     =    1;         //  Number of pattern matches before triggering

  //
  //  Setup Trigger Pattern
  //

  if (boot_time_wr)
    Logic_Analyzer_Trigger_Value_1 |= BIT_MASK_WR;
  else
    Logic_Analyzer_Trigger_Value_1 &= ~BIT_MASK_WR;
  if (boot_time_rd)
    Logic_Analyzer_Trigger_Value_1 |= BIT_MASK_RD;
  else
    Logic_Analyzer_Trigger_Value_1 &= ~BIT_MASK_RD;
  if (boot_time_lma)
    Logic_Analyzer_Trigger_Value_1 |= BIT_MASK_LMA;
  else
    Logic_Analyzer_Trigger_Value_1 &= ~BIT_MASK_LMA;
  if (boot_time_wr_mask)
    Logic_Analyzer_Trigger_Mask_1 |= BIT_MASK_WR;
  else
    Logic_Analyzer_Trigger_Mask_1 &= ~BIT_MASK_WR;
  if (boot_time_rd_mask)
    Logic_Analyzer_Trigger_Mask_1 |= BIT_MASK_RD;
  else
    Logic_Analyzer_Trigger_Mask_1 &= ~BIT_MASK_RD;
  if (boot_time_lma_mask)
    Logic_Analyzer_Trigger_Mask_1 |= BIT_MASK_LMA;
  else
    Logic_Analyzer_Trigger_Mask_1 &= ~BIT_MASK_LMA;

  boot_time_address &= 0x0000FFFF;
  Logic_Analyzer_Trigger_Value_1 &= 0xFF0000FF;
  Logic_Analyzer_Trigger_Value_1 |= (boot_time_address << 8);
  boot_time_address_mask &= 0x0000FFFF;
  Logic_Analyzer_Trigger_Mask_1 &= 0xFF0000FF;
  Logic_Analyzer_Trigger_Mask_1 |= (boot_time_address_mask << 8);

  boot_time_data &= 0x000000FF;
  Logic_Analyzer_Trigger_Value_1 &= 0xFFFFFF00;
  Logic_Analyzer_Trigger_Value_1 |= (boot_time_data << 0);
  boot_time_data_mask &= 0x000000FF;
  Logic_Analyzer_Trigger_Mask_1 &= 0xFFFFFF00;
  Logic_Analyzer_Trigger_Mask_1 |= (boot_time_data_mask << 0);

  boot_time_rselec &= 0x000000FF;
  Logic_Analyzer_Trigger_Value_2 &= 0xFFFFFF00;
  Logic_Analyzer_Trigger_Value_2 |= (boot_time_rselec << 0);
  boot_time_rselec_mask &= 0x000000FF;
  Logic_Analyzer_Trigger_Mask_2 &= 0xFFFFFF00;
  Logic_Analyzer_Trigger_Mask_2 |= (boot_time_rselec_mask << 0);

  Serial.printf("Match Value                        Mask Value\nCycle  Addr  Data  RSelec  State   Cycle  Addr  Data  RSelec  State\n");
  Serial.printf(" %c%c%c  ", ((Logic_Analyzer_Trigger_Value_1 >> 26) & 0x01) ? '1' : '0',
                             ((Logic_Analyzer_Trigger_Value_1 >> 25) & 0x01) ? '1' : '0',
                             ((Logic_Analyzer_Trigger_Value_1 >> 24) & 0x01) ? '1' : '0'  );
  Serial.printf("%06o  %03o    %03o",
                              (Logic_Analyzer_Trigger_Value_1 >>  8) & 0x0000FFFF,
                              (Logic_Analyzer_Trigger_Value_1 >>  0) & 0x000000FF,
                              (Logic_Analyzer_Trigger_Value_2 >>  0) & 0x000000FF  );
  Serial.printf("    %c%c%c%c    ", ((Logic_Analyzer_Trigger_Value_1 >> 31) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Value_1 >> 30) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Value_1 >> 29) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Value_1 >> 28) & 0x01) ? '1' : '0'  );

  Serial.printf("%c%c%c  ", ((Logic_Analyzer_Trigger_Mask_1 >> 26) & 0x01) ? '1' : '0',
                            ((Logic_Analyzer_Trigger_Mask_1 >> 25) & 0x01) ? '1' : '0',
                            ((Logic_Analyzer_Trigger_Mask_1 >> 24) & 0x01) ? '1' : '0'  );
  Serial.printf("%06o  %03o    %03o",
                              (Logic_Analyzer_Trigger_Mask_1 >>  8) & 0x0000FFFF,
                              (Logic_Analyzer_Trigger_Mask_1 >>  0) & 0x000000FF,
                              (Logic_Analyzer_Trigger_Mask_2 >>  0) & 0x000000FF  );
  Serial.printf("    %c%c%c%c\n\n", ((Logic_Analyzer_Trigger_Mask_1 >> 31) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Mask_1 >> 30) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Mask_1 >> 29) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Mask_1 >> 28) & 0x01) ? '1' : '0'  );

  Logic_Analyzer_Current_Buffer_Length  = boot_time_buffer_length;
  Logic_Analyzer_Current_Index_Mask     = Logic_Analyzer_Current_Buffer_Length - 1;

  Logic_Analyzer_Pre_Trigger_Samples    = boot_time_pre_trigger;
  Logic_Analyzer_Samples_Till_Done      = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;

  Logic_Analyzer_Event_Count = Logic_Analyzer_Event_Count_Init = boot_time_event_count;

  //
  //  These are always initialized when this function is called
  //

  Logic_Analyzer_State = ANALYZER_IDLE;
  Logic_Analyzer_Data_index             = 0;
  Logic_Analyzer_Valid_Samples          = 0;
  Logic_Analyzer_Index_of_Trigger       = -1;             //  A negative value means that if we display the buffer without a trigger event (time out) we won't display the Trigger message
  Logic_Analyzer_Triggered              = false;

  Logic_Analyzer_Samples_Till_Done      = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;
  
}


void proc_auxint(void)
{
  interruptVector = 0x12; //SPAR1
  interruptReq = true;
  ASSERT_INT;
}

void just_once_func(void)
{
  if (just_once) return;

  just_once = 1;

  Serial.printf("Describe the Just Once change\n");

//
//  Do the just once stuff here.
//  
//  Serial.printf("var name here  %08X  ", just_once );   //  insert correct variable names when adding a temp diagnostic
//  Serial.printf("var name here  %08X  ", just_once );
//  Serial.printf("var name here  %08X  ", just_once );
//  Serial.printf("var name here  %08X  ", just_once );
//  Serial.printf("var name here  %08X  ", just_once );
//  Serial.printf("var name here  %08X  ", just_once );
//  Serial.printf("var name here  %08X  ", just_once );
//  Serial.printf("\n");
}

////
////  Prompt for a file to be dumped (as text) to the diag terminal
////
//
//void show_file(void)
//{
//  File          file;
//  int           character;
//  Serial.printf("\nEnter filename including path to be displayed: ");
//  if (!wait_for_serial_string())       //  Hang here till we get a file name (hopefully)
//  {
//    return;                           //  Got a Ctrl-C , so abort command
//  }
//  if (!(file = SD.open(serial_string)))
//  {   // Failed to open
//    Serial.printf("Can't open file\n");
//    return;
//  }
//  while((character = file.read()) > 0)
//  {
//    Serial.printf("%c", character);
//  }
//  file.close();
// 
//  serial_string_used();
//}


void pulse_PWO(void)
{
  ASSERT_PWO_OUT;                       //  Reset the HP85
  pinMode(CORE_PIN_PWO_O, OUTPUT);      //  Core Pin 36 (Physical pin 28) is pulled high by an external resistor, which turns on the FET,
                                        //  which pulls the PWO line low, which resets the HP-85
  ASSERT_PWO_OUT;                       //  Over-ride external pull up resistor, still put High on gate, thus holdimg PWO on I/O bus Low

//  longjmp(PWO_While_Running, 99);     //  This may not be sufficient, since it doesn't run startup.c

//
//  According to research, this is equivalent to a cold start, which should do whatever the CPU defaults to then go to the ResetHandler()
//
//  See Teensy 4.0 Notes about this magic

  SCB_AIRCR = 0x05FA0004;

}

void dump_ram_window(void)      //  dump_ram_window Start(8) Len(8)    dump memory from the AUXROM RAM area
{
  uint32_t    dump_start, dump_len;
  uint8_t     junque[20];
  //
  //  The dump command may only have white space separators "dump_ram_window Start(8) Len(8)"
  //
  Serial.printf("Command to parse  [%s]\n", serial_string);
  sscanf(serial_string, "%s %o %o", &junque[0], (unsigned int*)&dump_start, (unsigned int*)&dump_len );
  dump_start &= 07760;  dump_len &= 07777;    //  Somewhat constrain start address and length to be in the AUXROM RAM window. Not precise, but I'm in a hurry. Note start is forced to 16 byte boundary
  Serial.printf("Dumping AUXROM RAM from %06o for %06o bytes\n", dump_start, dump_len);   // and addresses start at 0000 for dump == 070000 for the HP-85
  Serial.printf("%04o: ", dump_start);
  while(1)
  {
    Serial.printf("%03o ", AUXROM_RAM_Window.as_bytes[dump_start++]);
    if ((--dump_len) == 0)
    {
      Serial.printf("\n");
      break;
    }
    if ((dump_start % 16) == 0)
    {
      Serial.printf("\n%04o: ", dump_start);
    }
  }
}

void no_SD_card_message(void)
{

  //Write_on_CRT_Alpha(2, 0, "                                ");
  Write_on_CRT_Alpha(2, 0, "Hello,");
  Write_on_CRT_Alpha(3, 0, "You have installed an EBTKS, but");
  Write_on_CRT_Alpha(4, 0, "it seems to not have an SD card");
  Write_on_CRT_Alpha(5, 0, "installed. Without it, EBTKS");
  Write_on_CRT_Alpha(6, 0, "won't work. No ROMs, Tape,");
  Write_on_CRT_Alpha(7, 0, "Disk (or extra RAM, 85A only)");
  Write_on_CRT_Alpha(9, 0, "I suggest a class 10, 16 GB card");
  Write_on_CRT_Alpha(10,0, "with the standard EBTKS file set");
  Serial.printf("\nMessage sent to CRT advising that there is no SD card\n");
}
    
void ulisp(void)
{
#if ENABLE_LISP
  if (strcasecmp("ulisp"  , serial_string) == 0)
  {
    lisp_setup();
    while(1)
    {
      lisp_loop();  ///currently no way out!!
    }
  }
#endif
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Logic Analyzer (or maybe Code Trace utility)
//
//  Here is an example of setting up the Logic Analyzer
//
//  The setup should only be done when the state is ANALYZER_IDLE
//
//  Samples are 32 bits, top 5 are 0, next 3 are /WR (bit 26), /RD (bit 25), /LMA (bit 24)
//  the next 16 bits are the current address, and the low 8 bits are the data for that
//  address (may be read or write)
//  The Logic Analyzer does not currently trace DMA activity
//
//  The code will probably fail if there isn't at least 1 pre or post trigger sample,
//  so don't set Logic_Analyzer_Pre_Trigger_Samples to 0 or Logic_Analyzer_Current_Buffer_Length
//
//  Since this setup runs in the background, and acquisition runs in the onPhi_1_Rise() ISR
//  we need to not activate the Logic Analyzer until everything is ready to go.
//
//  With significant effort, this could be re-written as a table driven Q and A, and be
//  more flexible and easier to edit and probably less likely to have bugs, and smaller
//  code size.
//

void Setup_Logic_Analyzer(void)
{
  unsigned int      address;
  unsigned int      data;

  serial_string_used();

  //
  //  Not all of these initializations are needed, but this makes sure I've got all these
  //  values to something reasonable
  //
  if (Logic_Analyzer_Event_Count_Init == -1000)           // Use this to indicate the Logic analyzer has no default values. Set to -1000 in setup()
  {
    Logic_Analyzer_Trigger_Value_1        = 0x00000000;   //  Bits 29 and 28 are /IRLX and /HALTX with logic low being asserted
    Logic_Analyzer_Trigger_Value_2        = 0;
    Logic_Analyzer_Trigger_Mask_1         = 0;
    Logic_Analyzer_Trigger_Mask_2         = 0;
    Logic_Analyzer_Event_Count_Init       = 1;
    Logic_Analyzer_Current_Buffer_Length  = LOGIC_ANALYZER_BUFFER_SIZE;
    Logic_Analyzer_Current_Index_Mask     = LOGIC_ANALYZER_INDEX_MASK;
    Logic_Analyzer_Pre_Trigger_Samples    = Logic_Analyzer_Current_Buffer_Length - 24;   //  Must be less than LOGIC_ANALYZER_BUFFER_SIZE 
  }

  //
  //  These are always initialized when this function is called
  //
  Logic_Analyzer_State = ANALYZER_IDLE;
  Logic_Analyzer_Data_index             = 0;
  Logic_Analyzer_Valid_Samples          = 0;
  Logic_Analyzer_Index_of_Trigger       = -1;                             //  A negative value means that if we display the buffer without a trigger event (time out) we won't display the Trigger message
  Logic_Analyzer_Event_Count            = Logic_Analyzer_Event_Count_Init;
  Logic_Analyzer_Triggered              = false;
  Logic_Analyzer_Samples_Till_Done      = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;

  //
  //  Setup Trigger Pattern
  //

  Serial.printf("Logic Analyzer Setup\n\nWARNING: AUXROM Keywords are not processed while this menu is active\n\n");
  Serial.printf("Typing enter will use the prior value displayed inside [square brackets].   ^C to escape setup menu\n\n");
  Serial.printf("First enter the match pattern, then the mask (set bits to enable match).\n");
  Serial.printf("Order is control signals (3), Address (16), data (8) ROM Select (octal)\n");

redo_bus_control:
  
  Serial.printf("Bus Cycle /WR /RD /LMA pattern as 0/1, 3 bits (0 is asserted)[%c%c%c]:",
                    (Logic_Analyzer_Trigger_Value_1 & BIT_MASK_WR)  ? '1' : '0',
                    (Logic_Analyzer_Trigger_Value_1 & BIT_MASK_RD)  ? '1' : '0',
                    (Logic_Analyzer_Trigger_Value_1 & BIT_MASK_LMA) ? '1' : '0'           );
  if (!wait_for_serial_string())
  {
    return;                                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_bus_control_mask;}      //  Keep existing value
  if (strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 digit number, /WR /RD /LMA, only use 0 and 1\n\n"); serial_string_used(); goto redo_bus_control; }

  if (serial_string[0] == '1') Logic_Analyzer_Trigger_Value_1 |= BIT_MASK_WR;
  else if (serial_string[0] == '0') {Logic_Analyzer_Trigger_Value_1 &= ~BIT_MASK_WR;}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus_control; }

  if (serial_string[1] == '1') Logic_Analyzer_Trigger_Value_1 |= BIT_MASK_RD;
  else if (serial_string[1] == '0') {Logic_Analyzer_Trigger_Value_1 &= ~BIT_MASK_RD;}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus_control; }

  if (serial_string[2] == '1') Logic_Analyzer_Trigger_Value_1 |= BIT_MASK_LMA;
  else if (serial_string[2] == '0') {Logic_Analyzer_Trigger_Value_1 &= ~BIT_MASK_LMA;}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus_control; }

  serial_string_used();

redo_bus_control_mask:
  Serial.printf("Bus Cycle /WR /RD /LMA Mask as 0/1, 3 bits(1 is enabled)[%c%c%c]:",
                    (Logic_Analyzer_Trigger_Mask_1 & BIT_MASK_WR)  ? '1' : '0',
                    (Logic_Analyzer_Trigger_Mask_1 & BIT_MASK_RD)  ? '1' : '0',
                    (Logic_Analyzer_Trigger_Mask_1 & BIT_MASK_LMA) ? '1' : '0' );

  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_address_pattern;}      //  Keep existing value
  if (strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 digit number, only use 0 and 1\n\n"); serial_string_used(); goto redo_bus_control_mask; }

  if (serial_string[0] == '1') Logic_Analyzer_Trigger_Mask_1 |= BIT_MASK_WR;
  else if (serial_string[0] == '0') {Logic_Analyzer_Trigger_Mask_1 &= ~BIT_MASK_WR;}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus_control_mask; }

  if (serial_string[1] == '1') Logic_Analyzer_Trigger_Mask_1 |= BIT_MASK_RD;
  else if (serial_string[1] == '0') {Logic_Analyzer_Trigger_Mask_1 &= ~BIT_MASK_RD;}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus_control_mask; }

  if (serial_string[2] == '1') Logic_Analyzer_Trigger_Mask_1 |= BIT_MASK_LMA;
  else if (serial_string[2] == '0') {Logic_Analyzer_Trigger_Mask_1 &= ~BIT_MASK_LMA;}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus_control_mask; }

  serial_string_used();

redo_address_pattern:
  Serial.printf("Address pattern is 6 octal digits[%06o]:", (Logic_Analyzer_Trigger_Value_1 >> 8) & 0x0000FFFF);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_address_mask;}      //  Keep existing value
  if (strlen(serial_string) != 6)
  { Serial.printf("Please enter a 6 octal digits, only use 0 through 7\n\n"); serial_string_used(); goto redo_address_pattern; }
  sscanf(serial_string, "%o", &address);
  address &= 0x0000FFFF;
  Logic_Analyzer_Trigger_Value_1 &= 0xFF0000FF;
  Logic_Analyzer_Trigger_Value_1 |= ((int)address << 8);
  serial_string_used();

redo_address_mask:
  if ((Logic_Analyzer_Trigger_Value_1 & 0x00FFFF00) && ((Logic_Analyzer_Trigger_Mask_1 & 0x00FFFF00) == 0))
  {   //  We have an address value and the current mask is 0x0000
    Logic_Analyzer_Trigger_Mask_1 |= 0x00FFFF00;          //  for this situation, default the mask to 0xFFFF  (0177777)
  }
  Serial.printf("Address mask is 6 octal digits[%06o]:", (Logic_Analyzer_Trigger_Mask_1 >> 8) & 0x0000FFFF);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_data_pattern;}      //  Keep existing value
  if (strlen(serial_string) != 6)
  { Serial.printf("Please enter a 6 octal digits, only use 0 through 7 (1 bits enable match)\n\n"); serial_string_used(); goto redo_address_mask; }
  sscanf(serial_string, "%o", &address);                  //  reuse address for the mask value, as this is just temporary
  address &= 0x0000FFFF;
  Logic_Analyzer_Trigger_Mask_1 &= 0xFF0000FF;
  Logic_Analyzer_Trigger_Mask_1 |= ((int)address << 8);
  serial_string_used();

redo_data_pattern:
  Serial.printf("Data pattern is 3 octal digits[%03o]:", Logic_Analyzer_Trigger_Value_1 & 0x000000FF);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_data_mask;}      //  Keep existing value
  if (strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 octal digits, only use 0 through 7\n\n"); serial_string_used(); goto redo_data_pattern; }
  sscanf(serial_string, "%o", &data);
  data &= 0x000000FF;
  Logic_Analyzer_Trigger_Value_1 &= 0xFFFFFF00;
  Logic_Analyzer_Trigger_Value_1 |= ((unsigned int)data & 0x000000FF);
  serial_string_used();

redo_data_mask:
  if ((Logic_Analyzer_Trigger_Value_1 & 0x000000FF) && ((Logic_Analyzer_Trigger_Mask_1 & 0x000000FF) == 0))
  {   //  We have a data value and the current mask is 0x00
    Logic_Analyzer_Trigger_Mask_1 |= 0x000000FF;          //  for this situation, default the mask to 0xFF  (0377)
  }
  Serial.printf("Data mask is 3 octal digits[%03o]:", Logic_Analyzer_Trigger_Mask_1 & 0x000000FF);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_rselec_pattern;}      //  Keep existing value
  if (strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 octal digits, only use 0 through 7 (1 bits enable match)\n\n"); serial_string_used(); goto redo_data_mask; }
  sscanf(serial_string, "%o", &data);
  data &= 0x000000FF;
  Logic_Analyzer_Trigger_Mask_1 &= 0xFFFFFF00;
  Logic_Analyzer_Trigger_Mask_1 |= ((unsigned int)data & 0x000000FF);
  serial_string_used();

redo_rselec_pattern:
  Serial.printf("RSELEC is 3 octal digits[%03o]:", Logic_Analyzer_Trigger_Value_2 & 0x000000FF);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_rselec_mask;}      //  Keep existing value
  if (strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 octal digits, only use 0 through 7\n\n"); serial_string_used(); goto redo_rselec_pattern; }
  sscanf(serial_string, "%o", &data);
  data &= 0x000000FF;
  Logic_Analyzer_Trigger_Value_2 &= 0xFFFFFF00;
  Logic_Analyzer_Trigger_Value_2 |= data;
  serial_string_used();

redo_rselec_mask:
  if ((Logic_Analyzer_Trigger_Value_2 & 0x000000FF) && ((Logic_Analyzer_Trigger_Mask_2 & 0x000000FF) == 0))
  {   //  We have a RSELEC value and the current mask is 0x00
    Logic_Analyzer_Trigger_Mask_2 |= 0x000000FF;          //  for this situation, default the mask to 0xFF  (0377)
  }
  Serial.printf("RSELEC mask is 3 octal digits[%03o]:", Logic_Analyzer_Trigger_Mask_2 & 0x000000FF);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_state_pattern;}      //  Keep existing value
  if (strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 octal digits, only use 0 through 7 (1 bits enable match)\n\n"); serial_string_used(); goto redo_rselec_mask; }
  sscanf(serial_string, "%o", &data);
  data &= 0x000000FF;
  Logic_Analyzer_Trigger_Mask_2 &= 0xFFFFFF00;
  Logic_Analyzer_Trigger_Mask_2 |= data;
  serial_string_used();

//
//  State pattern recording and triggering has not been tested at all, and the tracing through DMA is unimplemented very tricky code
//  For now, setting up the pattern and mask are commented out with ////// so as not to raise the hopes of any users
//
redo_state_pattern:
//////  Serial.printf("Bus Cycle State DMA  IF  /IRLX  /HALTX pattern as 0/1, 4 bits (1, 1, 0, 0, is asserted for each)[%c%c%c%c]:",
//////                    (Logic_Analyzer_Trigger_Value_1 & 0x80000000)  ? '1' : '0',
//////                    (Logic_Analyzer_Trigger_Value_1 & 0x40000000)  ? '1' : '0',
//////                    (Logic_Analyzer_Trigger_Value_1 & 0x20000000)  ? '1' : '0', 
//////                    (Logic_Analyzer_Trigger_Value_1 & 0x10000000)  ? '1' : '0' );
//////  if (!wait_for_serial_string())
//////  {
//////    return;                                           //  Got a Ctrl-C , so abort command
//////  }
//////
//////  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto redo_state_mask;}      //  Keep existing value
//////  if (strlen(serial_string) != 4)
//////  { Serial.printf("Please enter a 4 digit number,  DMA  IF  /IRLX  /HALTX, only use 0 and 1\n\n"); serial_string_used(); goto redo_state_pattern; }
//////
//////  if (serial_string[0] == '1') Logic_Analyzer_Trigger_Value_1 |= 0x80000000;
//////  else if (serial_string[0] == '0') {Logic_Analyzer_Trigger_Value_1 &= ~0x80000000;}
//////  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_state_pattern; }
//////
//////  if (serial_string[1] == '1') Logic_Analyzer_Trigger_Value_1 |= 0x40000000;
//////  else if (serial_string[1] == '0') {Logic_Analyzer_Trigger_Value_1 &= ~0x40000000;}
//////  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_state_pattern; }
//////
//////  if (serial_string[2] == '1') Logic_Analyzer_Trigger_Value_1 |= 0x20000000;
//////  else if (serial_string[2] == '0') {Logic_Analyzer_Trigger_Value_1 &= ~0x20000000;}
//////  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_state_pattern; }
//////
//////  if (serial_string[2] == '1') Logic_Analyzer_Trigger_Value_1 |= 0x10000000;
//////  else if (serial_string[2] == '0') {Logic_Analyzer_Trigger_Value_1 &= ~0x10000000;}
//////  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_state_pattern; }
//////
//////  serial_string_used();
//////
//////redo_state_mask:
//////  Serial.printf("Bus Cycle DMA  IF  /IRLX  /HALTX Mask as 0/1, 4 bits(1 is enabled)[%c%c%c%c]:",
//////                    (Logic_Analyzer_Trigger_Mask_1 & 0x80000000)  ? '1' : '0',
//////                    (Logic_Analyzer_Trigger_Mask_1 & 0x40000000)  ? '1' : '0',
//////                    (Logic_Analyzer_Trigger_Mask_1 & 0x20000000)  ? '1' : '0',
//////                    (Logic_Analyzer_Trigger_Mask_1 & 0x10000000) ? '1' : '0'  );
//////
//////  if (!wait_for_serial_string())
//////  {
//////    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
//////  }
//////
//////  if (strlen(serial_string) == 0) {Serial.printf("Using prior value\n"); serial_string_used(); goto state_mask_done;}      //  Keep existing value
//////  if (strlen(serial_string) != 4)
//////  { Serial.printf("Please enter a 4 digit number, only use 0 and 1\n\n"); serial_string_used(); goto redo_state_mask; }
//////
//////  if (serial_string[0] == '1') Logic_Analyzer_Trigger_Mask_1 |= 0x80000000;
//////  else if (serial_string[0] == '0') {Logic_Analyzer_Trigger_Mask_1 &= ~0x80000000;}
//////  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_state_mask; }
//////
//////  if (serial_string[1] == '1') Logic_Analyzer_Trigger_Mask_1 |= 0x40000000;
//////  else if (serial_string[1] == '0') {Logic_Analyzer_Trigger_Mask_1 &= ~0x40000000;}
//////  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_state_mask; }
//////
//////  if (serial_string[2] == '1') Logic_Analyzer_Trigger_Mask_1 |= 0x20000000;
//////  else if (serial_string[2] == '0') {Logic_Analyzer_Trigger_Mask_1 &= ~0x20000000;}
//////  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_state_mask; }
//////
//////  if (serial_string[2] == '1') Logic_Analyzer_Trigger_Mask_1 |= 0x10000000;
//////  else if (serial_string[2] == '0') {Logic_Analyzer_Trigger_Mask_1 &= ~0x10000000;}
//////  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_state_mask; }
//////
//////  serial_string_used();
//////state_mask_done:

  Serial.printf("Match Value                        Mask Value\nCycle  Addr  Data  RSelec  State   Cycle  Addr  Data  RSelec  State\n");
  Serial.printf(" %c%c%c  ", ((Logic_Analyzer_Trigger_Value_1 >> 26) & 0x01) ? '1' : '0',
                             ((Logic_Analyzer_Trigger_Value_1 >> 25) & 0x01) ? '1' : '0',
                             ((Logic_Analyzer_Trigger_Value_1 >> 24) & 0x01) ? '1' : '0'  );
  Serial.printf("%06o  %03o    %03o",
                              (Logic_Analyzer_Trigger_Value_1 >>  8) & 0x0000FFFF,
                              (Logic_Analyzer_Trigger_Value_1 >>  0) & 0x000000FF,
                              (Logic_Analyzer_Trigger_Value_2 >>  0) & 0x000000FF  );
  Serial.printf("    %c%c%c%c    ", ((Logic_Analyzer_Trigger_Value_1 >> 31) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Value_1 >> 30) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Value_1 >> 29) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Value_1 >> 28) & 0x01) ? '1' : '0'  );



  Serial.printf("%c%c%c  ", ((Logic_Analyzer_Trigger_Mask_1 >> 26) & 0x01) ? '1' : '0',
                            ((Logic_Analyzer_Trigger_Mask_1 >> 25) & 0x01) ? '1' : '0',
                            ((Logic_Analyzer_Trigger_Mask_1 >> 24) & 0x01) ? '1' : '0'  );
  Serial.printf("%06o  %03o    %03o",
                              (Logic_Analyzer_Trigger_Mask_1 >>  8) & 0x0000FFFF,
                              (Logic_Analyzer_Trigger_Mask_1 >>  0) & 0x000000FF,
                              (Logic_Analyzer_Trigger_Mask_2 >>  0) & 0x000000FF  );
  Serial.printf("    %c%c%c%c\n\n", ((Logic_Analyzer_Trigger_Mask_1 >> 31) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Mask_1 >> 30) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Mask_1 >> 29) & 0x01) ? '1' : '0',
                                    ((Logic_Analyzer_Trigger_Mask_1 >> 28) & 0x01) ? '1' : '0'  );

//
//  Set the buffer length 32 .. 1024 , must be power of 2
//

#if LOGIC_ANALYZER_BUFFER_SIZE > 8192
#error This code needs to be modified for the changed LOGIC_ANALYZER_BUFFER_SIZE
#endif

redo_buffer_length:
  Serial.printf("Set the buffer length 32 .. %d , must be power of 2)[%d]:", LOGIC_ANALYZER_BUFFER_SIZE, Logic_Analyzer_Current_Buffer_Length);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0)
  {      //  Keep existing value
    Serial.printf("Using prior value\n");
  }
  else
  {
    sscanf(serial_string, "%d", (int *)&Logic_Analyzer_Current_Buffer_Length);
  }
  serial_string_used();
  if ((Logic_Analyzer_Current_Buffer_Length !=   32) &&
     (Logic_Analyzer_Current_Buffer_Length !=   64) &&
     (Logic_Analyzer_Current_Buffer_Length !=  128) &&
     (Logic_Analyzer_Current_Buffer_Length !=  256) &&
     (Logic_Analyzer_Current_Buffer_Length !=  512) &&
     (Logic_Analyzer_Current_Buffer_Length != 1024) &&
     (Logic_Analyzer_Current_Buffer_Length != 2048) &&
     (Logic_Analyzer_Current_Buffer_Length != 4096) &&
     (Logic_Analyzer_Current_Buffer_Length != 8192)    )
  {
    goto redo_buffer_length;
  }
  Logic_Analyzer_Current_Index_Mask = Logic_Analyzer_Current_Buffer_Length - 1;

//
//  Set the pre-trigger samples away from the buffer start and end to avoid possible off by 1 errors in my code.
//  Should not be an issue, since buffers are likely to be greater than 32 entries, and there is no need to start
//  or end right at the limits of the buffer
//
  if (Logic_Analyzer_Pre_Trigger_Samples > (Logic_Analyzer_Current_Buffer_Length - 2))
  {
    Logic_Analyzer_Pre_Trigger_Samples = Logic_Analyzer_Current_Buffer_Length/2;
  }

  Serial.printf("Pretrigger samples (2 to %d)[%d]:", Logic_Analyzer_Current_Buffer_Length - 2, Logic_Analyzer_Pre_Trigger_Samples);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0)
  {      //  Keep existing value
    Serial.printf("Using prior value\n");
  }
  else
  {
    sscanf(serial_string, "%d", (int *)&Logic_Analyzer_Pre_Trigger_Samples);
  }
  serial_string_used();
  if (Logic_Analyzer_Pre_Trigger_Samples < 2) Logic_Analyzer_Pre_Trigger_Samples = 2;
  if (Logic_Analyzer_Pre_Trigger_Samples > Logic_Analyzer_Current_Buffer_Length-2)
  {
    Logic_Analyzer_Pre_Trigger_Samples = Logic_Analyzer_Current_Buffer_Length-2;
  }

//
//  Set the number of occurences of the pattern match to cause a trigger
//
  Serial.printf("Event Count 1..N[%d]:", Logic_Analyzer_Event_Count_Init);
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false; return;                           //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0)
  {      //  Keep existing value
    Serial.printf("Using prior value\n");
  }
  else
  {
    sscanf(serial_string, "%d", (int *)&Logic_Analyzer_Event_Count_Init);
  }
  serial_string_used();

  Logic_Analyzer_Samples_Till_Done     = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;
  Logic_Analyzer_Event_Count           = Logic_Analyzer_Event_Count_Init;

  Serial.printf("Logic Analyzer Setup complete. Type la go to start\n\n\n");
}

static  uint32_t    LA_Heartbeat_Timer;

void Logic_analyzer_go(void)
{
//
//  This re-initialization allows re issuing la_go without re-entering parameters
//
  memset(Logic_Analyzer_Data_1, 0, 1024);
  memset(Logic_Analyzer_Data_2, 0, 1024);
  Logic_Analyzer_Data_index         = 0;
  Logic_Analyzer_Valid_Samples      = 0;
  Logic_Analyzer_Index_of_Trigger   = -1;                             //  A negative value means that if we display the buffer without a trigger event (time out) we won't display the Trigger message
  Logic_Analyzer_Triggered          = false;
  Logic_Analyzer_Event_Count        = Logic_Analyzer_Event_Count_Init;
  Logic_Analyzer_Samples_Till_Done  = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;
  LA_Heartbeat_Timer                = systick_millis_count + 1000;    //  Do heartbeat message every 1000 ms
  Logic_Analyzer_Valid_Samples_1_second_ago = -100000;
  Logic_Analyzer_State              = ANALYZER_ACQUIRING;
  Ctrl_C_seen = false;
}

void Logic_Analyzer_Poll(void)
{
  uint32_t      temp;
  int16_t       sample_number_relative_to_trigger;
  int16_t       i, j;
  int16_t       Samples_to_display;
  int16_t       Display_starting_index;
  float         sample_time;

  if (Logic_Analyzer_State == ANALYZER_IDLE)
  {
    return;
  }

  if (LA_Heartbeat_Timer < systick_millis_count)
  {
    Serial.printf("Heartbeat Timer %10d    systick_millis_count %10d\n", LA_Heartbeat_Timer, systick_millis_count);
    Serial.printf("waiting... Valid Samples:%4d Samples till done:%d\n", Logic_Analyzer_Valid_Samples, Logic_Analyzer_Samples_Till_Done);
    Serial.printf("Current Sample:%08X-%08X Mask:%08X-%08X TrigPattern:%08X-%08X Event Counter:%d Current RSELEC %03o\n\n",
                    Logic_Analyzer_main_sample, Logic_Analyzer_aux_sample,
                    Logic_Analyzer_Trigger_Mask_1,  Logic_Analyzer_Trigger_Mask_2,
                    Logic_Analyzer_Trigger_Value_1, Logic_Analyzer_Trigger_Value_2,
                    Logic_Analyzer_Event_Count, getRselec());


    //
    //  Logic Analyzer can timeout if maybe it gets stuck in DMA, Or maybe something else. Very hard to test this code
    //  Not even sure what we are looking for. This code is motivated by weird interaction between a real HPIB interface and EBTKS.
    //
    if ((Logic_Analyzer_Valid_Samples_1_second_ago == Logic_Analyzer_Valid_Samples) || Ctrl_C_seen)           //  If something crashes (interrupts dead???) , or Ctrl-C activity
    {
      if (Logic_Analyzer_Valid_Samples < Logic_Analyzer_Current_Buffer_Length)                                //  This should never happen
      {                                                                                                       //  So, we didn't even manage to fill the LA buffer
        Samples_to_display = Logic_Analyzer_Valid_Samples;
        Display_starting_index = 0;
      }
      else
      {                                                                                   //  Still timed out but the buffer is full
        Samples_to_display = Logic_Analyzer_Current_Buffer_Length;
        Display_starting_index = Logic_Analyzer_Data_index;                               //  This is where the next sample would have gone
      }
      Serial.printf("\n\nThe Logic Analyzer seems to have stalled, failed to collect %d samples surrounding trigger\n", Logic_Analyzer_Samples_Till_Done);
      Serial.printf("Logic_Analyzer_Valid_Samples_1_second_ago is  %10d\n", Logic_Analyzer_Valid_Samples_1_second_ago);
      Serial.printf("Displaying %5d samples, starting from buffer position %5d\n\n", Samples_to_display, Display_starting_index);

      Logic_Analyzer_State = ANALYZER_ACQUISITION_DONE;                                   //  Force termination, stops acquisition
      
      goto la_display_results;
    }
    Logic_Analyzer_Valid_Samples_1_second_ago = Logic_Analyzer_Valid_Samples;
    LA_Heartbeat_Timer = systick_millis_count + 1000;
  }
//  if (Ctrl_C_seen)                           //  Need to do better clean up of this I think. Really need to re-do all serial I/O
//  {                                         //  Type any character to abort    
//    Logic_Analyzer_State = ANALYZER_IDLE;
//    Serial.printf("LA Abort\n");
//    return;
//  }
  if (Logic_Analyzer_State == ANALYZER_ACQUIRING)
  {
    return;
  }
  //
  //  Logic analyzer has completed acquisition
  //

  Samples_to_display = Logic_Analyzer_Current_Buffer_Length;  
  Display_starting_index = Logic_Analyzer_Index_of_Trigger - Logic_Analyzer_Pre_Trigger_Samples;          //  This is where the next sample would have been written,
                                                                                                          //  except we finished acquisition. This value needs to be masked

la_display_results:

  Logic_Analyzer_State = ANALYZER_IDLE;
  Serial.printf("Logic Analyzer results\n\n");
  Serial.printf("Buffer Length %d  Address Mask  %d\n" , Logic_Analyzer_Current_Buffer_Length, Logic_Analyzer_Current_Index_Mask);
  Serial.printf("Trigger Index %d  Pre Trigger Samples %d\n\n", Logic_Analyzer_Index_of_Trigger, Logic_Analyzer_Pre_Trigger_Samples);
  strcpy(serial_string , "show mb");
  show();
  Serial.printf("The LA was triggered: %s and the trigger index is %5d\n", Logic_Analyzer_Triggered ? "true":"false" , Logic_Analyzer_Index_of_Trigger);
  Serial.printf("ENABLE_BUS_BUFFER_U2 = %d\n", GET_DISABLE_BUS_BUFFER_U2);
  Serial.printf("BUS_DIR_TO_HP        = %d\n", GET_BUS_DIR_TO_HP);
  Serial.printf("CTRL_DIR_TO_HP       = %d\n", GET_CTRL_DIR_TO_HP);
  Serial.printf("ASSERT_INT           = %d\n", GET_ASSERT_INT);
  Serial.printf("ASSERT_HALT          = %d\n", GET_ASSERT_HALT);
  Serial.printf("ASSERT_PWO_OUT       = %d\n", GET_ASSERT_PWO_OUT);
  Serial.printf("ASSERT_INTPRI        = %d\n", GET_ASSERT_INTPRI);
  //
  //  ARMv7-M_Architecture_Reference_Manual___DDI0403E_B.pdf      page 655 documents CPUID (0xE000ED00) and ICSR (0xE000ED04)
  //                                                              Macros in imxrt.h at line 9077
  //
  Serial.printf("ICSR (p655)          = %08X\n", SCB_ICSR);
  //
  //  ARMv7-M_Architecture_Reference_Manual___DDI0403E_B.pdf      page 575 documents PRIMASK which appears to be where the
  //                                                              Global Interrupt Enable/Disable is stored in the LSB
  //                                                              Macros __disable_irq() and __enable_irq() execute "CPSID i"
  //                                                              and "CPSIE i" respectively, which set or clear the PRIMASK bit
  //                                                              "CPSID i" sets PRIMASK LSB to 1 which disables all interrupts
  //                                                              PRIMASK does not live in normal address space. Access it with
  //                                                              the Special Register instructions MRS (read) and MSR (write).
  //                                                              PRIMASK is Special Register 0b00010 (i.e. reg 2)
  //
  __asm__ volatile("MRS %[t],PRIMASK":[t] "=r" (temp));
  Serial.printf("PRIMASK              = %08X   LSB 0 indicates Global Interrupts enabled in Teensy\n", temp);           //  It will be a real surprise if this works. Expect LSB to be 0. Tested, It works and shows Global Interrupt enable
  // //test
  // __disable_irq();
  // __asm__ volatile("MRS %[t],PRIMASK":[t] "=r" (temp));
  // __enable_irq();
  // Serial.printf("PRIMASK Expect 1     = %08X\n", temp);           //  It will be a real surprise if this works. Expect LSB to be 1. Tested, It works and shows Global Interrupt enable
  Serial.printf("\n\n");

  Serial.printf("    Time Sample   Address     Data   Cycle RSE DMA /IR /HAL          LA Data 1\n");
  Serial.printf("     us                              WRLF  LEC      LX  TX\n");
  sample_number_relative_to_trigger = - Logic_Analyzer_Pre_Trigger_Samples;

  for (i = 0 ; i < Samples_to_display ; i++)
  {
    j = (i + Display_starting_index) & Logic_Analyzer_Current_Index_Mask;
    temp = Logic_Analyzer_Data_1[j];
    if (i == 0) {temp = (temp | 0x80000000) & ~0x30000000;}   //  Deliberately Set the DMA, IRLX and HALTX bits so I can align the headings
    // //
    // //  test code to force various flags to make sure they display correctly (alignment and bit flags)
    // //
    // if (i == 10)
    // {
    //   temp |= 0x80000000;  // test DMA flag
    // }
    // if (i == 11)
    // {
    //   temp |= 0x40000000;  // test IF flag
    // }
    // if (i == 12)
    // {
    //   temp &= ~0x20000000;  // test IRLX flag
    // }
    // if (i == 13)
    // {
    //   temp &= ~0x10000000;  // test HALTX flag
    // }

    sample_time = (16.0/9.808) * sample_number_relative_to_trigger;

    //Serial.printf("%08X ", temp);
    if ((temp & (BIT_MASK_LMA | BIT_MASK_RD | BIT_MASK_WR)) == (BIT_MASK_LMA | BIT_MASK_RD | BIT_MASK_WR))
    { //  All 3 control lines are high (not asserted, so data bus is junque)
      Serial.printf("%9.3f %5d %06o/%04X  xxx/xx  ", sample_time, sample_number_relative_to_trigger++, (temp >> 8) & 0x0000FFFFU,
                                                             (temp >> 8) & 0x0000FFFFU );
    }
    else
    {
      Serial.printf("%9.3f %5d %06o/%04X  %03o/%02X  ", sample_time, sample_number_relative_to_trigger++, (temp >> 8) & 0x0000FFFFU,
                                                             (temp >> 8) & 0x0000FFFFU,
                                                             temp & 0x000000FFU,
                                                             temp & 0x000000FFU);
    }
    Serial.printf("%c", (temp & BIT_MASK_WR)  ? '-' : 'W');   //  Remember that these 3 signals are active low
    Serial.printf("%c", (temp & BIT_MASK_RD)  ? '-' : 'R');
    Serial.printf("%c", (temp & BIT_MASK_LMA) ? '-' : 'L');
    Serial.printf("%c", (temp & 0x40000000)   ? 'F' : '-');
    Serial.printf("  %03o", Logic_Analyzer_Data_2[j] & 0x000000FF );
    Serial.printf(" %s", (temp & 0x80000000)     ? " D "  : "   " );
    Serial.printf(" %s",   (temp & 0x20000000)   ? "   "  : " I " );
    Serial.printf(" %s",   (temp & 0x10000000)   ? "   "  : "  H");

    if (j == Logic_Analyzer_Index_of_Trigger)
    {
      Serial.printf("  Trigger");
    }
    else
    {
      Serial.printf("         ");
    }
    Serial.printf("  %08X", temp);
    Serial.printf("\n");
    Serial.flush();
  }
  Serial.printf("\n\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int int_power(int base, int exp)
{
  int result = 1;
  while (exp)
  {
    result = result * base;
    exp--;
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
//  Philip's replacement of the FASTLED library
//
uint8_t   led_data[6];

#define WS2812_0_ON_VAL          300
#define WS2812_0_OFF_VAL        1300
#define WS2812_1_ON_VAL         1300
#define WS2812_1_OFF_VAL         300
#define WS2812_FRAME_OFF_VAL  300000

#define SET_NEOPIX              (GPIO_DR_SET_NEOPIX_TSY    = BIT_MASK_NEOPIX_TSY)
#define CLEAR_NEOPIX            (GPIO_DR_CLEAR_NEOPIX_TSY  = BIT_MASK_NEOPIX_TSY)
#define TOGGLE_NEOPIX           (GPIO_DR_TOGGLE_NEOPIX_TSY = BIT_MASK_NEOPIX_TSY)

void WS2812_init(void)
{
  led_data[0] = 0;    //  LED 1 G     //  GRB is the data sequence that the WS2812B/E require , nor RGB
  led_data[1] = 0;    //  LED 1 R
  led_data[2] = 0;    //  LED 1 B
  led_data[3] = 0;    //  LED 2 G
  led_data[4] = 0;    //  LED 2 R
  led_data[5] = 0;    //  LED 2 B
  WS2812_update();
}

void setLedColor(uint8_t led, uint8_t r, uint8_t g, uint8_t b)
{
  if (led == 0)
  {
    led_data[1] = r;   //  Red
    led_data[0] = g;   //  Green
    led_data[2] = b;   //  Blue
  }
  else
  {
    led_data[4] = r;   //  Red
    led_data[3] = g;   //  Green
    led_data[5] = b;   //  Blue
  }
}

void WS2812_update(void)
{
  int     i,j;

  assert_DMA_Request();
  while(!DMA_Active){}     // Wait for acknowledgment, and Bus ownership

  for(i = 0 ; i < 6 ; i++)
  {
    j = 8;
    while(j)
    {
      j--;
      SET_NEOPIX;
      if(led_data[i] & (1 << j))      //  pick off the bits, starting at the MSB
      {   //  Selected bit is a 1
        EBTKS_delay_ns(WS2812_1_ON_VAL);
        CLEAR_NEOPIX;
        EBTKS_delay_ns(WS2812_1_OFF_VAL);
      }
      else
      {   //  Selected bit is a 0
        EBTKS_delay_ns(WS2812_0_ON_VAL);
        CLEAR_NEOPIX;
        EBTKS_delay_ns(WS2812_0_OFF_VAL);
      }
    }
  }
  //  EBTKS_delay_ns(WS2812_FRAME_OFF_VAL);   The > 280 us end of frame delay that moves the shifted
  //                                          data into the LED control registers is not needed, because
  //                                          the fastest HP85 BASIC code with back-to-back calls to SETLED LED,R,G,B
  //                                          takes about 4 ms. So BASIC cant generate LED updates too quickly that
  //                                          we have to expressly generate the 280+ us delay

  release_DMA_request();
  while(DMA_Active){}       // Wait for release
}

//    Memory usage analysis of FASTLED library (that needs inverted data, and has LSB of all data stuck high)
//    versus the above purpose built code for EBTKS
//    
//    TLDR: FASTLED library is 7 KB and doesn't work
//          This code is 320 bytes and works great.
//
//    ------------------
//    FASTLED Library memory usage
//    
//    section                size         addr
//    .text.progmem          6960   1610612736
//    .text.itcm           125088            0
//    .fini                     4       125088
//    .ARM.exidx                8       125092
//    .text.itcm.padding     5972       125100
//    .data                 21808    536870912
//    .bss                  73104    536892720
//    .bss.dma             178912    538968064
//    .bss.extram          135990   1879048192
//    .debug_frame           5884            0
//    .ARM.attributes          46            0
//    .comment                110            0
//    Total                553886
//    
//    ------------------
//    
//    Adding a simple #define to EBTKS_Config.h
//    
//    section                size         addr
//    .text.progmem          6960   1610612736
//    .text.itcm           125088            0
//    .fini                     4       125088
//    .ARM.exidx                8       125092
//    .text.itcm.padding     5972       125100
//    .data                 21792    536870912  was 21808 so -16
//    .bss                  73120    536892704  was 73104 so +16
//    .bss.dma             178912    538968064
//    .bss.extram          135990   1879048192
//    .debug_frame           5884            0
//    .ARM.attributes          46            0
//    .comment                110            0
//    Total                553886               same
//    
//    ------------------
//    
//    Disable FASTLED code (including the small amount of code that references it)
//    
//    section                size         addr
//    .text.progmem          6960   1610612736
//    .text.itcm           117984            0  -7104
//    .fini                     4       117984
//    .ARM.exidx                8       117988
//    .text.itcm.padding    13076       117996  +7104
//    .data                 21776    536870912  -16
//    .bss                  73136    536892688  +16
//    .bss.dma             178912    538968064
//    .bss.extram          135990   1879048192
//    .debug_frame           5884            0
//    .ARM.attributes          46            0
//    .comment                110            0
//    Total                553886
//    
//    so FASTLED library is approximately 7 KB
//    
//    ------------------
//    
//    Disabled FASTLED code, installed PMF replacement
//    
//    section                size         addr
//    .text.progmem          6960   1610612736
//    .text.itcm           118304            0  +320
//    .fini                     4       118304
//    .ARM.exidx                8       118308
//    .text.itcm.padding    12756       118316  -320
//    .data                 21744    536870912  - 32
//    .bss                  73168    536892656  + 32
//    .bss.dma             178912    538968064
//    .bss.extram          135990   1879048192
//    .debug_frame           5884            0
//    .ARM.attributes          46            0
//    .comment                110            0
//    Total                553886


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void jay_pi(void)
{
      //
      //  Jay Hamlin's little Pi calculator Benchmark 08/11/2020
      //    https://groups.io/g/hpseries80/message/4524
      //
      //  Wikipedia Pi is 3.14159265358979323846264338327950288419716939937510
      //
      //  with  1,000,000 slices:  jay_pi execution time is  273 ms , answer is 3.14159265 241386
      //  with 10,000,000 slices:  jay_pi execution time is 2759 ms , answer is 3.1415926535 5335
      //
      if (strcasecmp("jay_pi"  , serial_string) == 0)
      {
        double    radius;
        double    radiusSquared;
        double    slices;
        double    index;
        double    sliceWidth;
        double    sliceAreaSum;
        double    y0;
        double    y1;

        uint32_t  systick_millis_at_start;

        systick_millis_at_start = systick_millis_count;
        radius        = 1.0000;
        radiusSquared = 1.0000;
        slices = 10000000;
        index = 0;
        sliceWidth  = (radius / slices);
        sliceAreaSum = 0.0000;

        y0 = sqrt(radiusSquared - pow(index/slices, 2.0));
        
        while(index++ < slices)
        {
          y1 = sqrt(radiusSquared - pow(index/slices, 2.0));
          sliceAreaSum += sliceWidth * (y0+y1)/2.0;
          y0 = y1;
        }
        sliceAreaSum *= 4.0000;
        Serial.printf("\njay_pi execution time is %d ms , answer is %-16.14f\n", systick_millis_count-systick_millis_at_start, sliceAreaSum);
      }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Test writing Graphics

void Simple_Graphics_Test(void)
{
  float f_x0;
  float f_y0;
  float f_x1;
  float f_y1;

  int x0;
  int y0;
  int x1;
  int y1;

  f_x1 = 255;
  f_y0 = 0;
  f_y1 = 0;

  for (f_x0 = 0 ; f_x0 < 256 ; f_x0 += 5.12)
  {
    x0 = f_x0;
    y0 = f_y0;
    x1 = f_x1;
    y1 = f_y1;
    writeLine(x0, y0, x1, y1, 1);
    f_y1 += 3.7647;
  }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Time and Date functions
//
//  Based on this thread:  https://forum.pjrc.com/threads/60317-How-to-access-the-internal-RTC-in-a-Teensy-4-0
//
//  Synopsis: There are two RTCs (SRTC and RTC). I think SRTC is set when the firmware is downloaded with the current time, and is the battery backed RTC
//            On startup, in startup.c at line 118 the SRTC is copied to the RTC
//
//  Per https://forum.pjrc.com/threads/62357-TimLib-h-vs-Time-h   it seems there is some library weirdness.
//      the "solution" is to rename 
//            C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\libraries\Time\Time.h
//          to
//            C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\libraries\Time\Time.h___DISABLED_by_PMF
//
time_t getTeensy3Time(void)
{
  return Teensy3Clock.get();
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void show_RTC(void)
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Helper functions

void str_tolower(char *p)
{
  for (  ; *p ; ++p) *p = ((*p > 0x40) && (*p < 0x5b)) ? (*p | 0x60) : *p;
}



///**********************************************************   From Everett, 9/22/2020 , with just formatting changes to conform with the rest of the source code
/// Checks pT against possibly wild-card-containing pP.                                   and change of case for bool, true, false
/// Returns TRUE if there's a match, otherwise FALSE.
/// pT must NOT have any wildcards, just be a valid filename.
/// pP may or may not have wildcards (* and ?).
/// * matches 0 or more "any characters"
/// ? matches exactly one "any character"
bool MatchesPattern(char *pT, char *pP)
{
  if ( *pT==0 )
  {                                                     //  If reached the end of the test string
    return (*pP==0 || (*pP=='*' && *(pP+1)==0));        //  Return TRUE if reached the end of the pattern string, else FALSE
  }
  else if ( *pP=='?' )
  {                                                     //  Pattern matches ANYTHING in the test
    return MatchesPattern(pT+1, pP+1);
  }
  else if ( *pP=='*' )
  {                                                     //  Start with * each 0 characters, then keep advancing pT so * eats more and more characters
    while( *pT )
    {
      if ( MatchesPattern(pT, pP+1) ) return true;
      ++pT;
    }
    return ( *(pP+1)==0 );                              //  Return TRUE if reached the end of the pattern string, else FALSE
  }
  else if ( *pT==*pP )
  {
    return MatchesPattern(pT+1, pP+1);
  }
  return false;
}


//
//  Print out Special Keys in decimal or octal, for HP85 and HP87. Data structure lifted from Everett's Emulator
//

typedef struct {
	const char	*KeyName;
	int	Code85, Code87;
} KEYNAMCOD;

/// NOTE: Any SHORTER names that identically match the beginning of a LONGER name
/// *must* come AFTER the longer name, so that the longer name will be searched
/// and found (if matches) BEFORE the shorter name is searched and found. The
/// only current example of this is K1 and K10-K14.
//
//  List sorted in ascending HP85 order
//

KEYNAMCOD	Keys[]={
	{"K1",            128,  128},
	{"K2",            129,  129},
	{"K3",            130,  130},
	{"K4",            131,  131},
	{"K5",            132,  161},
	{"K6",            133,  162},
	{"K7",            134,  156},
	{"K8",            135,  204},
	{"K9",             -1,  205},
	{"K10",            -1,  206},
	{"K11",            -1,  207},
	{"K12",            -1,  165},
	{"K13",            -1,  172},
	{"K14",            -1,  147},
	{"REW",           136,   -1},
	{"COPY",          137,   -1},
	{"PAPADV",        138,   -1},
	{"RESET",         139,  139},
	{"INIT",          140,  140},
	{"RUN",           141,  141},
	{"PAUSE",         142,  142},
	{"CONT",          143,  143},
	{"STEP",          144,  144},
	{"TEST",          145,  146},
	{"CLEAR",         146,  137},
	{"GRAPH",         147,   -1},   //  error in Emulator script.c
	{"LIST",          148,  148},
	{"PLIST",         149,  149},
	{"KEYLABEL",      150,  150},
	{"-unused-",      151,  151},   // not sure for 87
	{"-unused-",      152,  152},   // not sure for 87
	{"BKSPACE",       153,  153},
	{"ENDLINE",       154,  154},
	{"FASTBKSPACE",   155,  155},
	{"LFCURS",        156,  159},
	{"RTCURS",        157,  170},
	{"ROLLUP",        158,  145},
	{"ROLLDN",        159,  169},
	{"-LINE",         160,  157},
	{"UPCURS",        161,  163},
	{"DNCURS",        162,  164},
	{"INSRT/REPL",    163,  158},
	{"-CHR",          164,  136},
	{"HOMECURS",      165,  163},
	{"RESULT",        166,  166},
	{"-unused-",      167,  167},   // not sure for 87
	{"DELETE",        168,   -1},
	{"STORE",         169,   -1},
	{"LOAD",          170,   -1},
	{"-unused-",      171,  171},   // not sure for 87
	{"AUTO",          172,   -1},   //  error in Emulator script.c
	{"SCRATCH",       173,   -1},   //  error in Emulator script.c
	{"A/G",            -1,  168},
	{"TR/NORM",        -1,  173},
	{"NUME",           -1,  160}

};
int	KeysCnt=sizeof(Keys)/sizeof(Keys[0]);

void dump_keys(bool hp85kbd , bool octal)
{
  int     i;

  if (hp85kbd)
  {
    if (octal)
    {
      Serial.printf("List of HP85 Special Keys in Octal\n");
      for (i = 0 ; i < KeysCnt ; i++)
      {
        if (Keys[i].Code85 < 0)
        {
          continue;
        }
        Serial.printf("%12.12s   %3o\n", Keys[i].KeyName , Keys[i].Code85);
      }
    }
    else
    {
      Serial.printf("List of HP85 Special Keys in Decimal\n");
      for (i = 0 ; i < KeysCnt ; i++)
      {
        if (Keys[i].Code85 < 0)
        {
          continue;
        }
        Serial.printf("%12.12s   %3d\n", Keys[i].KeyName , Keys[i].Code85);
      }
    }
  }
  else
  {
    if (octal)
    {
      Serial.printf("List of HP877 Special Keys in Octal\n");
      for (i = 0 ; i < KeysCnt ; i++)
      {
        if (Keys[i].Code87 < 0)
        {
          continue;
        }
        Serial.printf("%12.12s   %3o\n", Keys[i].KeyName , Keys[i].Code87);
      }
    }
    else
    {
      Serial.printf("List of HP87 Special Keys in Decimal\n");
      for (i = 0 ; i < KeysCnt ; i++)
      {
        if (Keys[i].Code87 < 0)
        {
          continue;
        }
        Serial.printf("%12.12s   %3d\n", Keys[i].KeyName , Keys[i].Code87);
      }
    }
  }

  Serial.printf("\n\n");
}



