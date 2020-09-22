//
//  06/30/2020  Assorted Utility functions
//
//  07/04/2020  wrote and calibrated EBTKS_delay_ns()
//
//  07/25/2020  There has been continuous playing with the menus
//              Got Logic Analyzer to work
//
//
//

#include <Arduino.h>
#include "Inc_Common_Headers.h"




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Diagnostic Pulse Generation
//
//  TXD_Pulser generates the specified number of pulses by Toggling the TXD pin
//
//  Each pulse comprises
//    TXD is toggled immediately on entry
//    wait 5 us
//    TXD is toggled again
//    wait 5 us
//
//  So it takes 10 us per pulse
//

void TXD_Pulser(uint8_t count)
{
  volatile int32_t timer;

  count <<= 1;

  while(count--)
  {
    TOGGLE_TXD;
    for(timer = 0 ; timer < 333 ; timer++)
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

#define FIXED_OVERHEAD    (59)  //  Seems the fixed overhead is xx
#define SCALE_FACTOR      (10)

void EBTKS_delay_ns(int32_t count)
{
  volatile int32_t   local_count;

  local_count = count - FIXED_OVERHEAD;
  local_count /= SCALE_FACTOR;
  if(local_count <= 0)
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
//    TOGGLE_T33; EBTKS_delay_ns(   50); TOGGLE_T33; EBTKS_delay_ns(100);  //    53 ns
//    TOGGLE_T33; EBTKS_delay_ns(   60); TOGGLE_T33; EBTKS_delay_ns(100);  //    49 ns
//    TOGGLE_T33; EBTKS_delay_ns(   68); TOGGLE_T33; EBTKS_delay_ns(100);  //    49 ns
//    TOGGLE_T33; EBTKS_delay_ns(   69); TOGGLE_T33; EBTKS_delay_ns(100);  //    71 ns
//    TOGGLE_T33; EBTKS_delay_ns(   70); TOGGLE_T33; EBTKS_delay_ns(100);  //    71 ns
//    TOGGLE_T33; EBTKS_delay_ns(   80); TOGGLE_T33; EBTKS_delay_ns(100);  //    81 ns
//    TOGGLE_T33; EBTKS_delay_ns(   90); TOGGLE_T33; EBTKS_delay_ns(100);  //    91 ns
//    TOGGLE_T33; EBTKS_delay_ns(   95); TOGGLE_T33; EBTKS_delay_ns(100);  //    91 ns
//    TOGGLE_T33; EBTKS_delay_ns(  100); TOGGLE_T33; EBTKS_delay_ns(100);  //    98 ns
//    TOGGLE_T33; EBTKS_delay_ns(  200); TOGGLE_T33; EBTKS_delay_ns(100);  //   201 ns
//    TOGGLE_T33; EBTKS_delay_ns(  227); TOGGLE_T33; EBTKS_delay_ns(100);  //   221 ns
//    TOGGLE_T33; EBTKS_delay_ns(  318); TOGGLE_T33; EBTKS_delay_ns(100);  //   311 ns
//    TOGGLE_T33; EBTKS_delay_ns( 1000); TOGGLE_T33; EBTKS_delay_ns(100);  //  1001 ns
//    TOGGLE_T33; EBTKS_delay_ns( 2000); TOGGLE_T33; EBTKS_delay_ns(100);  //  2000 ns
//    __enable_irq();
//    delay(1);
//  }


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  All the menu functions, and the function call table


//
//  All the simple functions are above. The more complex are below, and need function prototypes here
//


void help_0(void);
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
void proc_tdir(void);
void DMA_Test_1(void);
void DMA_Test_2(void);
void DMA_Test_3(void);
void DMA_Test_4(void);
void DMA_Test_5(void);
void CRT_Timing_Test_1(void);
void MEM_Test_1(void);
void MEM_Test_2(void);
void MEM_Test_3(void);
void Setup_Logic_analyzer(void);
void Logic_analyzer_go(void);
void Simple_Graphics_Test(void);
void show_logfile(void);
void clean_logfile(void);
void proc_addr(void);
void proc_auxint(void);
void show_mailboxes_and_usage(void);
void just_once_func(void);
void pulse_PWO(void);
void dump_ram_window(void);
void jay_pi(void);
void ulisp(void);
void proc_ddir(void);
void proc_rdir(void);




struct S_Command_Entry
{
  char  command_name[21];   //  command names must be no more than 20 characters
  void  (*f_ptr)(void);
};

//
//  This table must come after the functions listed to avoid undefined function name errors
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
  {"tflush",           tape_handle_command_flush},
  {"tload",            tape_handle_command_load},
  {"tdir",             proc_tdir},
  {"dma_1",            DMA_Test_1},
  {"dma_2",            DMA_Test_2},
  {"dma_3",            DMA_Test_3},
  {"dma_4",            DMA_Test_4},
  {"dma_5",            DMA_Test_5},
  {"crt_1",            CRT_Timing_Test_1},
  {"mem_1",            MEM_Test_1},
  {"mem_2",            MEM_Test_2},
  {"mem_3",            MEM_Test_3},
  {"la_setup",         Setup_Logic_Analyzer},
  {"la_go",            Logic_analyzer_go},
  {"graphics_test",    Simple_Graphics_Test},
  {"show_logfile",     show_logfile},
  {"clean_logfile",    clean_logfile},
  {"addr",             proc_addr},
  {"auxint",           proc_auxint},
  {"show_mb",          show_mailboxes_and_usage},
  {"jo",               just_once_func},
  {"pwo",              pulse_PWO},
  {"dump_ram_window",  dump_ram_window},
  {"jay_pi",           jay_pi},
  {"ulisp",            ulisp},
  {"ddir",             proc_ddir},
  {"rdir",             proc_rdir},
  {"TABLE_END",        help_0}
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
    if(Ctrl_C_seen)
    {
      return false;
    }
  }
  return true;
}


/////////////////////////////////////////////  disabled while we try Everett's idea on HP85 Lockout during serial I/O

#define USE_ORIGINAL_POLL           (1)

#if USE_ORIGINAL_POLL

void get_serial_string_poll(void)
{
  char  current_char;

  if(serial_string_available)
  {
    LOGPRINTF("Error: get_serial_string_poll() called, but serial_string_available is true\n");
    Ctrl_C_seen = true;     //  Not really a Ctrl-C , but treat it this way to error out of whatever is calling this
    return;
  }

  if(!Serial.available()) return;       //  No new characters available

  while(1)    //  Got at least 1 new character
  {
    //  Serial.printf("\nget_serial_string diag: available chars %2d  current string length %2d  current string [%s]\n%s", Serial.available(), serial_string_length, serial_string, serial_string);
    //  Serial.printf("[%02X]",current_char );    //  Use this to see what scan codes (as seen by VSCode) or escape sequences as seen by SecureCRT for special PC keys.
    current_char = Serial.read();
    switch(current_char)
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
        if(serial_string_length >= SERIAL_STRING_MAX_LENGTH)
        {
          break;    //  Ignore character, no room for it
        }
        serial_string[serial_string_length++] = current_char;
        serial_string[serial_string_length] = '\0';
        Serial.printf("%c", current_char);
    }
  if(!Serial.available()) return;                           //  No new characters available
  }
}


#else

//  Try Everett's idea oh HP85 Lockout during serial I/O

void get_serial_string_poll(void)
{
  char  current_char;

  if(serial_string_available)
  {
    LOGPRINTF("Error: get_serial_string_poll() called, but serial_string_available is true\n");
    Ctrl_C_seen = true;     //  Not really a Ctrl-C , but treat it this way to error out of whatever is calling this
    return;
  }

  if(!Serial.available()) return;       //  No new characters available

//
//  We have a charcter, so put the HP85 to sleep by pretending to do DMA
//


  DMA_Request = true;
  while(!DMA_Active){};     // Wait for acknowledgement, and Bus ownership
  //
  //  Now in DMA state. No more interrupts from our managing the HP85 Bus
  //  but DMA also does "__disable_irq()" , so undo this, as we need interrupts
  //  for USB Serial. This should not be a problem, since we won't be doing
  //  HP85 bus transfers that require the disable
  //

  __enable_irq();

  while(1)    //  Got at least 1 new character
  {
    //  Serial.printf("\nget_serial_string diag: available chars %2d  current string length %2d  current string [%s]\n%s", Serial.available(), serial_string_length, serial_string, serial_string);
    //  Serial.printf("[%02X]",current_char );    //  Use this to see what scan codes (as seen by VSCode) or escape sequences as seen by SecureCRT for special PC keys.
    current_char = Serial.read();
    switch(current_char)
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
        //
        //  Time to exit. Put it in the state it expects
        //
        __disable_irq();
        release_DMA_request();
        while(DMA_Active){};      // Wait for release  
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
        //
        //  Time to exit. Put it in the state it expects
        //
        __disable_irq();
        release_DMA_request();
        while(DMA_Active){};      // Wait for release  
        return;
        break;
      case '\x14':                                          //  Ctrl-T.  Show the current incomplete command
        serial_string[serial_string_length] = '\0';
        Serial.printf("\n%s", serial_string);
        break;
      default:                                              //  If buffer is full, the characteris ignored
        if(serial_string_length >= SERIAL_STRING_MAX_LENGTH)
        {
          break;    //  Ignore character, no room for it
        }
        serial_string[serial_string_length++] = current_char;
        serial_string[serial_string_length] = '\0';
        Serial.printf("%c", current_char);
    }
  while(!Serial.available()) {};                            //  Wait for more characters. Need ^C or \r
  }
}

#endif

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
  if(!serial_string_available) return;          //  Don't hang around here if there isn't a command to be processed

  //  Serial.printf("\nSerial_Command_Poll diag: serial_string_available %1d\n", serial_string_available);
  //  Serial.printf(  "                          serial_string_length    %2d  serial_string [%s]   ", serial_string_length, serial_string);

  strcpy(lc_serial_command, serial_string);
  str_tolower(lc_serial_command);               //  Lower case version for easier command matching. Mixed case still available in serial_command
  serial_string_used();                         //  Throw away the mixed case version
  //  Serial.printf("lower case command [%s]\n", lc_serial_command);
  if(strlen(lc_serial_command) == 0) return;    //  Not interested in zero length commands

  command_index = 0;
  while(1)
  {
    //  Serial.printf("1"); delay(100);
    if(strcmp(Command_Table[command_index].command_name , "TABLE_END") == 0)
    {
      Serial.printf("\nUnrecognized Command [%s].  Flushing command\n", lc_serial_command);
      return;
    }
    if(strcmp(Command_Table[command_index].command_name , lc_serial_command) == 0)
    {
      (* Command_Table[command_index].f_ptr)();
      return;
    }
    command_index++;
  }
}

//
//  Dump a region of memory as bytes. Whenever at an address with low 4 bits zero, do a newline and address
//    If show_addr is false, suppress printing addresses, and the newline every 16 bytes
//    If final_nl is false, suppress final newline
//

void HexDump_T41_mem(uint32_t start_address, uint32_t count, bool show_addr, bool final_nl)
{
  if(show_addr)
  {
    Serial.printf("%08X: ", start_address);  
  }
  while(count--)
  {
    Serial.printf("%02X ", *(uint8_t *)start_address++);
    if(((start_address % 16) == 0) && show_addr)
    {
      Serial.printf("\n%08X: ", start_address);
    }
  }
  if(final_nl)
  {
    Serial.printf("\n");
  }
}

void HexDump_HP85_mem(uint32_t start_address, uint32_t count, bool show_addr, bool final_nl)
{
  uint8_t       temp;
  if(show_addr)
  {
    Serial.printf("%08X: ", start_address);  
  }
  while(count--)
  {
    AUXROM_Fetch_Memory(&temp, start_address++, 1);
    Serial.printf("%02X ", temp);
    if(((start_address % 16) == 0) && show_addr)
    {
      Serial.printf("\n%08X: ", start_address);
    }
  }
  if(final_nl)
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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Poll for tripple click of Shift key while in Idle

//
//  We can recognize that the HP85 is IDLE (CSTAT / R16 == 0) by detecting address 102434 occuring during execution
//  If we are in IDLE, this occurs every 67 us . So with a little fiddling, we can monitor
//

bool is_HP85_idle(void)
{
  int32_t       loops;      //  This set to take about 100us, so if we are in IDLE, we will catch the RMIDLE address being issued

  loops = 9940;             //  This value was derived by measuring the execution time of this function, and making it 100 us
  //
  //  Logic_Analyzer_sample is updated in the Phi 1 interrupt handler every bus cycle. As this function is running outside
  //  of interrupt context, Logic_Analyzer_sample is always a valid sample.
  //
  while(loops--)
  {
    if((Logic_Analyzer_sample & 0x00FFFF00U) == (RMIDLE << 8))
    {
      return true;
    }
  }
  return false;
}

#if ENABLE_THREE_SHIFT_DETECTION
int             shift_pressed_duration = 0;         // in units of 10 ms
bool            click_seq_in_progress = false;
uint8_t         clicks_so_far = 0;
uint32_t        systick_millis_count_last_visit = 0;
uint32_t        time_at_first_click;
uint32_t        time_at_third_click;

//
//  This function is called from the main loop to detect three clicks of the shift key
//  while the HP-85 is in the keboard idle loop. To minimize the amount of interference,
//  we only check the IDLE and keyboard shift key status every 10 SysTicks (should be every 10 milliseconds)
//  Each click is between 40 ms and 300 ms, and the three clicks must be completed in
//  under 2 seconds
//

static void reset_three_clicks(void)
{
  shift_pressed_duration    = 0;
  click_seq_in_progress     = false;
  clicks_so_far             = 0;
  systick_millis_count_last_visit = systick_millis_count;
}

bool Three_Shift_Clicks_Poll(void)
{
  if((systick_millis_count_last_visit + 10) > systick_millis_count)
  {   //  Only test every 10+ milliseconds
    return false;
  }

  systick_millis_count_last_visit = systick_millis_count;
  //  TOGGLE_TXD;

  //  Check for timeout

  if(click_seq_in_progress && ((time_at_first_click + 2000) < systick_millis_count))
  {   //  Time out. Need to see three clicks/clicks in under 2 seconds
    reset_three_clicks();
    //  Serial.printf("e");
    return false;
  }

  if(!is_HP85_idle())         //  This takes 100 us or less, depending on whether HP85 is idle
  {
    return false;
  }

  if(DMA_Peek8(KEYSTS) & 0x08)
  {
    shift_pressed_duration++;     //  Each increment is 10 ms
    return false;
  }

  if(shift_pressed_duration == 0)
  {
    return false;
  }

  //
  //  If we get here, we have waited at leats 10 ms, Shift key is not currently pressed, but it was
  //
  //  Serial.printf("a %d ", shift_pressed_duration);   //  Typical click is around 12

  if(shift_pressed_duration < 4)
  {   //  click too short, maybe contact bounce. Just ignore it, but still continue to look for three clicks
    shift_pressed_duration = 0;
    //  Serial.printf("b");
    return false;
  }

  if(shift_pressed_duration > 30)
  {   //  click is too long. throw it away, and any progress towards three clicks
    reset_three_clicks();
    //  Serial.printf("c");
    return false;
  }

  //  Looks like we have a shift key click, that is not too short, and not too long. Just right

  if(!click_seq_in_progress)
  {   //  This is the first click, we want 3 within 2 seconds
    click_seq_in_progress = true;
    clicks_so_far = 1;
    time_at_first_click = systick_millis_count;   //  Will use this for timeout
    shift_pressed_duration = 0;
    //  Serial.printf("d");
    return false;
  }

  //  Not the first click

  clicks_so_far++;
  shift_pressed_duration = 0;
  
  if(clicks_so_far >= 3)
  {
    reset_three_clicks();
    time_at_third_click = systick_millis_count;
    //  Serial.printf("Three clicks. Time after first click: %d, Time after last click: %d\n", time_at_first_click, time_at_third_click);
    return true;
  }
  //  Serial.printf("f");
  return false;   //  we exit here after the second click.
}

#endif        //  ENABLE_THREE_SHIFT_DETECTION

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  All the simple commands

void help_0(void)
{
  Serial.printf("\nEBTKS Control commands - not case sensitive\n\n");
  Serial.printf("0     Help for the help levels\n");
  Serial.printf("1     Help for Tape commands\n");
  Serial.printf("2     Help for Disk commands\n");
  Serial.printf("3     Help for Configuration commands\n");
  Serial.printf("4     Help for Auxiliary programs\n");
  Serial.printf("5     Help for Diagnostic commands\n");
  Serial.printf("6     Help for Logic Analyzer\n");
  Serial.printf("7     Help for ROMs\n");
  Serial.printf("\n");
}

void help_1(void)
{
  Serial.printf("Commands for the Tape Drive\n");
  Serial.printf("tflush   Force a tape flush and reload\n");
  Serial.printf("tload    Load a new tape image from SD\n");
  Serial.printf("tdir     Directory of available tapes\n");
  Serial.printf("twhich   #Which tape is currently loaded\n");
  Serial.printf("\n");
}

void help_2(void)
{
  Serial.printf("Commands for the Disk Drive\n");
  Serial.printf("dflush   #Force a disk flush and reload\n");     //  Not yet Implemented
  Serial.printf("dload    #Load a new disk image from SD\n");     //  Not yet Implemented
  Serial.printf("ddir     Directory of available disks\n");
  Serial.printf("dwhich   #Which disk(s) is(are) currently loaded\n");
  Serial.printf("\n");
}

void help_3(void)
{
  Serial.printf("Commands for the Configuration\n");
  Serial.printf("cfgrom     #Select active roms\n");      //  Not yet Implemented
  Serial.printf("rdir       Directory of available ROMs\n");
  Serial.printf("screen     #Screen emulation\n");        //  Not yet Implemented
  Serial.printf("keyboard   #Keyboard emulation\n");      //  Not yet Implemented
  Serial.printf("\n");
}

void help_4(void)
{
  Serial.printf("Comands for Auxiliary programs\n");
  Serial.printf("cpm         #CP/M operating system\n");                           //  Not yet Implemented
  Serial.printf("ulisp       uLisp interpreter\n");
  Serial.printf("python      #uPython\n");                                         //  Not yet Implemented
  Serial.printf("pdp8-os8    #PDP-8E inc Fortran-II and IV, Basic, Focal\n");      //  Not yet Implemented
  Serial.printf("cobol       #COBOL\n");                                           //  Not yet Implemented
  Serial.printf("fasthp85    #Turbo HP-85\n");                                     //  Not yet Implemented
  Serial.printf("\n");
}

void help_5(void)
{
  //uint16_t    nxtmem;
  //uint16_t    i;

  Serial.printf("Commands for Diagnostic\n");
  Serial.printf("dma_1    Read 50 bytes from ROM, 1,000,000 times\n");
  Serial.printf("dma_2    Write 50 bytes to  ROM, 1,000,000 times\n");
  Serial.printf("dma_3    Read the following block lengths: 14, 15, 16, 29, 30, 31\n");
  Serial.printf("dma_4    Read/Write of 50 bytes at 0xB000\n");
  Serial.printf("dma_5    DMA Text to the screen\n");
  Serial.printf("crt_1    Try and understand CRT Busy status timing\n");
  Serial.printf("mem_1    Increment ITCM 1E6\n");
  Serial.printf("mem_2    Increment DMAMEM 1E6\n");
  Serial.printf("mem_3    Fetch DMAMEM 1E6\n");
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
  // for(i = 0 ; i < 32 ; i++)
  // {
  //   Serial.printf("%03O  ", DMA_Peek8(nxtmem++));
  //   if((i%16) == 15)
  //   {
  //     Serial.printf("\n");
  //   }
  // }
  // Serial.printf("\n\n");
}

void help_6(void)
{
  Serial.printf("Commands for Logic Analyzer and other diagnostics\n");
  Serial.printf("la_start    Start the Logic analyzer. Currently compiled config\n");
  Serial.printf("la_config   #Configure the Logic analyzer. Currently compiled config\n");
  Serial.printf("addr        print the current address that the HP85 has accessed\n");
  Serial.printf("dump_ram_window Start(8) Len(8)    dump memory from the AUXROM RAM area\n");
  Serial.printf("Other Commands and Demos\n");
  Serial.printf("graphics_test\n");
  Serial.printf("show_logfile\n");
  Serial.printf("clean_logfile\n");
  Serial.printf("auxint\n");
  Serial.printf("show_mb\n");
  Serial.printf("jo\n");
  Serial.printf("pwo\n");
  Serial.printf("jay_pi\n");
  Serial.printf("reset #Reset HP85 and EBTKS\n");
  Serial.printf("\n");
}

void help_7(void)
{
  Serial.printf("ROM commands\n");
  Serial.printf("rdir     List ROMs\n");
  Serial.printf("\n");
}


//bool LineAtATime_ls_Init(const char* path, uint8_t flags, uint8_t indent)
//{
//  dir_flags  = flags;
//  dir_indent = indent;
//  if(dir)
//  {
//    dir.close();
//  }
//  return dir.open(path, O_RDONLY);
//}


//bool LineAtATime_ls_Next()
//{
//  File file;
//
//  PS.flush();
//  if(file.openNext(&dir, O_RDONLY))
//  {
//    // indent for dir level
//    if (!file.isHidden() || (dir_flags & LS_A))
//    {
//      for (uint8_t i = 0; i < dir_indent; i++)
//      {
//        PS.write(' ');
//      }
//      if (dir_flags & LS_DATE) {
//        file.printModifyDateTime(&PS);
//        PS.write(' ');
//      }
//      if (dir_flags & LS_SIZE) {
//        file.printFileSize(&PS);
//        PS.write(' ');
//      }
//      file.printName(&PS);
//      if (file.isDir()) {
//        PS.write('/');
//      }
//      PS.write('\n');
//      if ((dir_flags & LS_R) && file.isDir()) {
//        file.ls(&PS, dir_flags, dir_indent + 2);
//      }
//      Serial.printf("XXXXX");
//    }
//    file.close();
//    return true;
//  }
//  dir.close();
//  return false;
//}

void proc_tdir(void)
{
  File root = SD.open("/tapes/");
  printDirectory(root,0);
}

void proc_ddir(void)
{
  //  SD.ls("/disks/", LS_R | LS_SIZE | LS_DATE);
  //  SD.ls(PS, "/", LS_R | LS_SIZE | LS_DATE, 3);
  //File root = SD.open("/disks/");
  //printDirectory(root,0);
  LineAtATime_ls_Init();
  while(LineAtATime_ls_Next())
  {
    //Serial.printf("%s    end of LineAtATime_ls_Next call %d\n", PS.get_ptr() , call_count++);
  }
}

void proc_rdir(void)
{
  File root = SD.open("/roms/");
  printDirectory(root,0);
}

FASTRUN   volatile uint32_t test_mem_FASTRUN;

void MEM_Test_1(void)
{

  uint32_t  loop;

  Serial.printf("Increment ITCM 1E6, Monitor TXD\n");

  SET_TXD;
  for(loop = 0 ; loop < 1E5 ; loop++)     //  This loop takes 3248 us == 32.48 ns per iteration
  {
    //  Null loop
  }
  CLEAR_TXD;

  EBTKS_delay_ns(1000000);    //  1 ms

  SET_TXD;
  for(loop = 0 ; loop < 1E5 ; loop++)     //  This loop takes 16470 us. Loop overhead is 3248 us, so the remainder is 13222 us
  {
    test_mem_FASTRUN++;                   //  Need 10 instances, because single gets hidden due to pipelining
    test_mem_FASTRUN++;                   //  This is not nearly as precise measurement as I had hoped.
    test_mem_FASTRUN++;                   //  So, 1E5 * 10 increments takes 13222 us -> 13.22 ns per increment
    test_mem_FASTRUN++;
    test_mem_FASTRUN++;
    test_mem_FASTRUN++;
    test_mem_FASTRUN++;
    test_mem_FASTRUN++;
    test_mem_FASTRUN++;
    test_mem_FASTRUN++;
  }
  CLEAR_TXD;
}

DMAMEM   volatile uint32_t test_mem_DMAMEM;

void MEM_Test_2(void)
{
  uint32_t  loop;

  Serial.printf("Increment DMAMEM 1E6, Monitor TXD\n");

  SET_TXD;
  for(loop = 0 ; loop < 1E5 ; loop++)     //  This loop takes 3248 us == 32.48 ns per iteration
  {
    //  Null loop
  }
  CLEAR_TXD;

  EBTKS_delay_ns(1000000);    //  1 ms

  SET_TXD;
  for(loop = 0 ; loop < 1E5 ; loop++)     //  This loop takes 9528 us. Loop overhead is 3248 us, so the remainder is 6280 us
  {
    test_mem_DMAMEM++;                    //  Need 10 instances, because single gets hidden due to pipelining
    test_mem_DMAMEM++;                    //  This is not nearly as precise measurement as I had hoped.
    test_mem_DMAMEM++;                    //  So, 1E5 * 10 increments takes 6280 us -> 6.280 ns per increment
    test_mem_DMAMEM++;                    //  Somehow ??? DMAMEM is faster than ITCM ???
    test_mem_DMAMEM++;
    test_mem_DMAMEM++;
    test_mem_DMAMEM++;
    test_mem_DMAMEM++;
    test_mem_DMAMEM++;
    test_mem_DMAMEM++;
  }
  CLEAR_TXD;
}

FASTRUN   volatile uint32_t FASTRUN_1, FASTRUN_2;

void MEM_Test_3(void)
{
  uint32_t  loop;

  Serial.printf("Copy DMAMEM 1E6, Monitor TXD\n");

  SET_TXD;
  for(loop = 0 ; loop < 1E5 ; loop++)     //  This loop takes 7440 us ==> 4.1 ns per copy (subtract loop overhead seen in prior tests
  {
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
    FASTRUN_1 = FASTRUN_2;
  }
  CLEAR_TXD;

  EBTKS_delay_ns(1000000);    //  1 ms

  SET_TXD;
  for(loop = 0 ; loop < 1E5 ; loop++)     //  This loop takes 5492 us. Loop overhead 3248, leaving 2181 us
  {
    FASTRUN_1 = test_mem_DMAMEM;          //  Need 10 instances, because single gets hidden due to pipelining
    FASTRUN_1 = test_mem_DMAMEM;          //  
    FASTRUN_1 = test_mem_DMAMEM;          //  So, 1E5 * 10 increments takes 2181 after subtracting known loop overhead of 3248
    FASTRUN_1 = test_mem_DMAMEM;          //  so each read/write between two different memory regions is 2.18ns
    FASTRUN_1 = test_mem_DMAMEM;
    FASTRUN_1 = test_mem_DMAMEM;
    FASTRUN_1 = test_mem_DMAMEM;
    FASTRUN_1 = test_mem_DMAMEM;
    FASTRUN_1 = test_mem_DMAMEM;
    FASTRUN_1 = test_mem_DMAMEM;
  }
  CLEAR_TXD;
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
  Logic_Analyzer_Channel_A_index = 0;
  Logic_Analyzer_Valid_Samples   = 0;
  Logic_Analyzer_Trigger_Value   = 0;   //  trigger on anything
  Logic_Analyzer_Trigger_Mask    = 0;   //  trigger on anything
  Logic_Analyzer_Index_of_Trigger= 1;  //  Just give it a safe value
  Logic_Analyzer_Event_Count_Init= 1;
  Logic_Analyzer_Event_Count     = 1;
  Logic_Analyzer_Triggered       = false;
  Logic_Analyzer_Current_Buffer_Length = 32;
  Logic_Analyzer_Current_Index_Mask  = 0x1F;
  Logic_Analyzer_Pre_Trigger_Samples = 16;
  Logic_Analyzer_Samples_Till_Done   = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;

  Logic_analyzer_go();

}

void proc_auxint(void)
{
  interruptVector = 0x12; //SPAR1
  interruptReq = true;
  ASSERT_INT;
}

void show_mailboxes_and_usage(void)
{
  int     i;

  Serial.printf("First 8 mailboxes, usages, lengths, and some buffer\n  #  MB    Usage  Length  Buffer\n");
  for(i = 0 ; i < 8 ; i++)
  {
    Serial.printf("  %1d   %1d   %4d   %4d     ", i, AUXROM_RAM_Window.as_struct.AR_Mailboxes[i], AUXROM_RAM_Window.as_struct.AR_Usages[i], AUXROM_RAM_Window.as_struct.AR_Lengths[i]);
    HexDump_T41_mem((uint32_t)&AUXROM_RAM_Window.as_struct.AR_Buffer_0[i*256], 8, false, true);
  }
  Serial.printf("\nBOPTS6\n");
  for(i = 0 ; i < 8 ; i++)
  {
    Serial.printf("%02x ", AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint8_t[i]);
  }
  Serial.printf("\n");
}

void just_once_func(void)
{
  if(just_once) return;

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
  sscanf(lc_serial_command, "%s %o %o", &junque[0], (unsigned int*)&dump_start, (unsigned int*)&dump_len );
  dump_start &= 07760;  dump_len &= 07777;    //  Somewhat constrain start address and length to be in the AUXROM RAM window. Not precise, but I'm in a hurry. Note start is forced to 16 byte boundary
  Serial.printf("Dumping AUXROM RAM from %06o for %06o bytes\n", dump_start, dump_len);   // and addresses start at 0000 for dump == 070000 for the HP-85
  Serial.printf("%04o: ", dump_start);
  while(1)
  {
    Serial.printf("%03o ", AUXROM_RAM_Window.as_bytes[dump_start++]);
    if((--dump_len) == 0)
    {
      Serial.printf("\n");
      break;
    }
    if((dump_start % 16) == 0)
    {
      Serial.printf("\n%04o: ", dump_start);
    }
  }
}

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
      if(strcmp("jay_pi"  , lc_serial_command) == 0)
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

void no_SD_card_message(void)
{

  //Write_on_CRT_Alpha(2, 0, "                                ");
  Write_on_CRT_Alpha(2, 0, "Hello,");
  Write_on_CRT_Alpha(3, 0, "You have installed an EBTKS, but");
  Write_on_CRT_Alpha(4, 0, "it seems to not have an SD card");
  Write_on_CRT_Alpha(5, 0, "installed. Without it, EBTKS");
  Write_on_CRT_Alpha(6, 0, "won't work. No ROMs, Tape,");
  Write_on_CRT_Alpha(7, 0, "Disk or extra RAM (85A only)");
  Write_on_CRT_Alpha(8, 0, "I suggest a class 10, 16 GB card");
  Write_on_CRT_Alpha(9, 0, "with the standard EBTKS file set");
  Serial.printf("\nMessage sent to CRT advising that there is no SD card\n");
}
    
void ulisp(void)
{
#if ENABLE_LISP
  if(strcmp("ulisp"  , lc_serial_command) == 0)
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
void Setup_Logic_Analyzer(void)
{
  unsigned int  address;
  unsigned int  data;

  //
  //  Not all of these initializations are needed, but this makes sure I've got all these
  //  values to something reasonable
  //
  Logic_Analyzer_State = ANALYZER_IDLE;
  Logic_Analyzer_Channel_A_index        = 0;
  Logic_Analyzer_Valid_Samples          = 0;
  Logic_Analyzer_Trigger_Value          = 0;
  Logic_Analyzer_Trigger_Mask           = 0;
  Logic_Analyzer_Index_of_Trigger       = 1;          //  Just give it a safe value
  Logic_Analyzer_Event_Count_Init       = 1;
  Logic_Analyzer_Event_Count            = 1;
  Logic_Analyzer_Triggered              = false;
  Logic_Analyzer_Current_Buffer_Length  = LOGIC_ANALYZER_BUFFER_SIZE;
  Logic_Analyzer_Current_Index_Mask     = LOGIC_ANALYZER_INDEX_MASK;
  Logic_Analyzer_Pre_Trigger_Samples    = 100;                          //  Must be less than LOGIC_ANALYZER_BUFFER_SIZE 
  Logic_Analyzer_Samples_Till_Done      = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;

  //
  //  Setup Trigger Pattern
  //
redo_bus:
  Serial.printf("Logic Analyzer Setup\nFirst enter the match pattern, then the mask (set bits to enable match)\n");
  Serial.printf("Order is control signals (3), Address (16), data (8)\nBus Cycle /WR /RD /LMA pattern as 0/1, 3 bits (0 is asserted):");

  if(!wait_for_serial_string())
  {
    return;                           //  Got a Ctrl-C , so abort command
  }

  if(strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 digit number, /WR /RD /LMA, only use 0 and 1\n\n"); serial_string_used(); goto redo_bus; }

  if(serial_string[0] == '1') Logic_Analyzer_Trigger_Value |= BIT_MASK_WR;
  else if (serial_string[0] == '0') {}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus; }

  if(serial_string[1] == '1') Logic_Analyzer_Trigger_Value |= BIT_MASK_RD;
  else if (serial_string[1] == '0') {}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus; }

  if(serial_string[2] == '1') Logic_Analyzer_Trigger_Value |= BIT_MASK_LMA;
  else if (serial_string[2] == '0') {}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_bus; }

  serial_string_used();

redo_busmask:
  Serial.printf("Bus Cycle /WR /RD /LMA Mask as 0/1, 3 bits(1 is enabled):");

  if(!wait_for_serial_string())
  {
    return;                           //  Got a Ctrl-C , so abort command
  }

  if(strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 digit number, only use 0 and 1\n\n"); serial_string_used(); goto redo_busmask; }

  if(serial_string[0] == '1') Logic_Analyzer_Trigger_Mask |= BIT_MASK_WR;
  else if (serial_string[0] == '0') {}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_busmask; }

  if(serial_string[1] == '1') Logic_Analyzer_Trigger_Mask |= BIT_MASK_RD;
  else if (serial_string[1] == '0') {}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_busmask; }

  if(serial_string[2] == '1') Logic_Analyzer_Trigger_Mask |= BIT_MASK_LMA;
  else if (serial_string[2] == '0') {}
  else { Serial.printf("Only 0 or 1\n\n"); serial_string_used(); goto redo_busmask; }

  serial_string_used();

redo_address_pattern:
  Serial.printf("Address pattern is 6 octal digits:");
  if(!wait_for_serial_string())
  {
    return;                           //  Got a Ctrl-C , so abort command
  }
  if(strlen(serial_string) != 6)
  { Serial.printf("Please enter a 6 octal digits, only use 0 through 7\n\n"); serial_string_used(); goto redo_address_pattern; }
  sscanf(serial_string, "%o", &address);
  address &= 0x0000FFFF;
  Logic_Analyzer_Trigger_Value |= ((int)address << 8);
  serial_string_used();

redo_address_mask:
  Serial.printf("Address mask is 6 octal digits:");
  if(!wait_for_serial_string())
  {
    return;                           //  Got a Ctrl-C , so abort command
  }
  if(strlen(serial_string) != 6)
  { Serial.printf("Please enter a 6 octal digits, only use 0 through 7 (1 bits enable match)\n\n"); serial_string_used(); goto redo_address_mask; }
  sscanf(serial_string, "%o", &address);
  address &= 0x0000FFFF;
  Logic_Analyzer_Trigger_Mask |= ((int)address << 8);
  serial_string_used();

redo_data_pattern:
  Serial.printf("Data pattern is 3 octal digits:");
  if(!wait_for_serial_string())
  {
    return;                           //  Got a Ctrl-C , so abort command
  }
  if(strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 octal digits, only use 0 through 7\n\n"); serial_string_used(); goto redo_data_pattern; }
  sscanf(serial_string, "%o", &data);
  data &= 0x000000FF;
  Logic_Analyzer_Trigger_Value |= ((unsigned int)data & 0x000000FF);
  serial_string_used();

redo_data_mask:
  Serial.printf("Data mask is 3 octal digits:");
  if(!wait_for_serial_string())
  {
    return;                           //  Got a Ctrl-C , so abort command
  }
  if(strlen(serial_string) != 3)
  { Serial.printf("Please enter a 3 octal digits, only use 0 through 7 (1 bits enable match)\n\n"); serial_string_used(); goto redo_data_mask; }
  sscanf(serial_string, "%o", &data);
  data &= 0x000000FF;
  Logic_Analyzer_Trigger_Mask |= ((unsigned int)data & 0x000000FF);
  serial_string_used();

  Serial.printf("Here is the Pattern %1o %06o %03o and the Mask %1o %06o %03o\n",
                (Logic_Analyzer_Trigger_Value >> 24) & 0x00000007,
                (Logic_Analyzer_Trigger_Value >>  8) & 0x0000FFFF,
                (Logic_Analyzer_Trigger_Value >>  0) & 0x000000FF,
                (Logic_Analyzer_Trigger_Mask >>  24) & 0x00000007,
                (Logic_Analyzer_Trigger_Mask >>   8) & 0x0000FFFF,
                (Logic_Analyzer_Trigger_Mask >>   0) & 0x000000FF);

//
//  Set the pre-trigger samples away from the buffer start and end to avoid possible off by 1 errors in my code.
//  Should not be an issue, since buffers are likely to be greater than 32 entries, and there is no need to start
//  or end right at the limits of the buffer
//
  Serial.printf("Pretrigger samples (2 to %d):", Logic_Analyzer_Current_Buffer_Length-2);
  if(!wait_for_serial_string())
  {
    return;                           //  Got a Ctrl-C , so abort command
  }
  sscanf(serial_string, "%d", (int *)&Logic_Analyzer_Pre_Trigger_Samples);
  if(Logic_Analyzer_Pre_Trigger_Samples < 2) Logic_Analyzer_Pre_Trigger_Samples = 2;
  if(Logic_Analyzer_Pre_Trigger_Samples > Logic_Analyzer_Current_Buffer_Length-2) Logic_Analyzer_Pre_Trigger_Samples = Logic_Analyzer_Current_Buffer_Length-2;
  serial_string_used();

  Serial.printf("Event Count 1..N ");
  if(!wait_for_serial_string())
  {
    return;                           //  Got a Ctrl-C , so abort command
  }
  sscanf(serial_string, "%d", (int *)&Logic_Analyzer_Event_Count_Init);
  serial_string_used();

  Logic_Analyzer_Samples_Till_Done     = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;
  Logic_Analyzer_Event_Count           = Logic_Analyzer_Event_Count_Init;

  Serial.printf("Logic Analyzer Setup complete. Type la_go to start\n");
}

static  uint32_t    LA_Heartbeat_Timer;

void Logic_analyzer_go(void)
{
//
//  This re-initialization allows re issuing la_go without re-entering parameters
//
  Logic_Analyzer_Channel_A_index    = 0;
  Logic_Analyzer_Valid_Samples      = 0;
  Logic_Analyzer_Index_of_Trigger   = 1;          //  Just give it a safe value
  Logic_Analyzer_Triggered          = false;
  Logic_Analyzer_Event_Count        = Logic_Analyzer_Event_Count_Init;
  Logic_Analyzer_Samples_Till_Done  = Logic_Analyzer_Current_Buffer_Length - Logic_Analyzer_Pre_Trigger_Samples;
  LA_Heartbeat_Timer   = systick_millis_count + 1000;    //  Do heartbeat message every 1000 ms
  Logic_Analyzer_State = ANALYZER_ACQUIRING;
}

void Logic_Analyzer_Poll(void)
{
  uint32_t      i, j, temp;
  int16_t       sample_number_relative_to_trigger;

  if(Logic_Analyzer_State == ANALYZER_IDLE)
  {
    return;
  }
  if(LA_Heartbeat_Timer < systick_millis_count)
  {
    LA_Heartbeat_Timer = systick_millis_count + 1000;
    Serial.printf("waiting... Valid Samples:%4d Sample:%08X Mask:%08X TrigPattern:%08X Event:%d RSELEC %03o\n",
                    Logic_Analyzer_Valid_Samples, Logic_Analyzer_sample, Logic_Analyzer_Trigger_Mask,
                    Logic_Analyzer_Trigger_Value, Logic_Analyzer_Event_Count, getRselec());
    //Serial.printf("Mailboxes 0..6  :");
    //show_mailboxes_and_usage();
  }
  if(Ctrl_C_seen)                           //  Need to do better clean up of this I think. Really need to re-do all serial I/O
  {                                         //  Type any character to abort    
    Logic_Analyzer_State = ANALYZER_IDLE;
    Serial.printf("LA Abort\n");
    return;
  }
  if(Logic_Analyzer_State == ANALYZER_ACQUIRING)
  {
    return;
  }
  //
  //  Logic analyzer has completed acquisition
  //
  Logic_Analyzer_State = ANALYZER_IDLE;
  Serial.printf("Logic Analyzer results\n\n");
  Serial.printf("Buffer Length %d  Address Mask  %d\n" , Logic_Analyzer_Current_Buffer_Length, Logic_Analyzer_Current_Index_Mask);
  Serial.printf("Trigger Index %d  Pre Trigger Samples %d\n\n", Logic_Analyzer_Index_of_Trigger, Logic_Analyzer_Pre_Trigger_Samples);
  show_mailboxes_and_usage();
  Serial.printf("Sample  Address      Data    Cycle\n");
  Serial.printf("                             WRL\n");
  sample_number_relative_to_trigger = - Logic_Analyzer_Pre_Trigger_Samples;

  for(i = 0 ; i < Logic_Analyzer_Current_Buffer_Length ; i++)
  {
    j = (i + Logic_Analyzer_Index_of_Trigger - Logic_Analyzer_Pre_Trigger_Samples) & Logic_Analyzer_Current_Index_Mask;
    temp = Logic_Analyzer_Channel_A[j];
    if((temp & (BIT_MASK_LMA | BIT_MASK_RD | BIT_MASK_WR)) == (BIT_MASK_LMA | BIT_MASK_RD | BIT_MASK_WR))
    { //  All 3 control lines are high (not asserted, so data bus is junque)
      Serial.printf("   %-4d %06o/%04X  xxx/xx  ", sample_number_relative_to_trigger++, (temp >> 8) & 0x0000FFFFU,
                                                             (temp >> 8) & 0x0000FFFFU );
    }
    else
    {
      Serial.printf("   %-4d %06o/%04X  %03o/%02X  ", sample_number_relative_to_trigger++, (temp >> 8) & 0x0000FFFFU,
                                                             (temp >> 8) & 0x0000FFFFU,
                                                             temp & 0x000000FFU,
                                                             temp & 0x000000FFU);
    }
    Serial.printf("%c", (temp & BIT_MASK_WR)  ? '-' : 'W');   //  Remember that these 3 signals are active low
    Serial.printf("%c", (temp & BIT_MASK_RD)  ? '-' : 'R');
    Serial.printf("%c", (temp & BIT_MASK_LMA) ? '-' : 'L');
    if(j == Logic_Analyzer_Index_of_Trigger)
    {
      Serial.printf("  Trigger\n");
    }
    else
    {
      Serial.printf("\n");
    }
    Serial.flush();
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

  for(f_x0 = 0 ; f_x0 < 256 ; f_x0 += 5.12)
  {
    x0 = f_x0;
    y0 = f_y0;
    x1 = f_x1;
    y1 = f_y1;
    writeLine(x0, y0, x1, y1, 1);
    f_y1 += 3.7647;
  }

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Helper functions

void str_tolower(char *p)
{
  for(  ; *p ; ++p) *p = ((*p > 0x40) && (*p < 0x5b)) ? (*p | 0x60) : *p;
}





