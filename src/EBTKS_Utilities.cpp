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

//
//  The following tables of months and days is lifted from C:\Users\-UserName-\.platformio\packages\framework-arduinoteensy\libraries\Time\DateStrings.cpp
//  with appropriate edits for this project
//

const char monthStr0[]  = "";
const char monthStr1[]  = "January";
const char monthStr2[]  = "February";
const char monthStr3[]  = "March";
const char monthStr4[]  = "April";
const char monthStr5[]  = "May";
const char monthStr6[]  = "June";
const char monthStr7[]  = "July";
const char monthStr8[]  = "August";
const char monthStr9[]  = "September";
const char monthStr10[] = "October";
const char monthStr11[] = "November";
const char monthStr12[] = "December";

const char * const monthNames_P[] =
{
    monthStr0,monthStr1,monthStr2,monthStr3,monthStr4,monthStr5,monthStr6,
    monthStr7,monthStr8,monthStr9,monthStr10,monthStr11,monthStr12
};

//  const char monthShortNames_P[] = "ErrJanFebMarAprMayJunJulAugSepOctNovDec";   //  not currently used

const char dayStr0[] = "Err";
const char dayStr1[] = "Sunday";
const char dayStr2[] = "Monday";
const char dayStr3[] = "Tuesday";
const char dayStr4[] = "Wednesday";
const char dayStr5[] = "Thursday";
const char dayStr6[] = "Friday";
const char dayStr7[] = "Saturday";

const char * const dayNames_P[] =
{
   dayStr0,dayStr1,dayStr2,dayStr3,dayStr4,dayStr5,dayStr6,dayStr7
};

const char daySuffix[] = "xxstndrdthththththththththththththththththstndrdthththththththst";

bool time_adjust_is_minutes;

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


// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //
// //  The rabbit hole for this leads to
// //    C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\libraries\EEPROM\EEPROM.h
// //    C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\cores\teensy4\avr\eeprom.h
// //    https://forum.pjrc.com/threads/57377?p=214566&viewfull=1#post214566
// //    https://forum.pjrc.com/threads/59374-Teensy-4-0-how-to-persist-data-how-to-connect-serial
// //
// //    Quote: The EEPROM lib provides 1080 bytes, using wear leveling across 60K of the flash. The last 4K stores the recovery program
// //    I think T4.1 "EEPROM" size is E2END+1 == 0x10BB+1 = 4284 bytes
// //
// //  The docs are here:  https://www.pjrc.com/teensy/td_libs_EEPROM.html
// //  and confirms 4284 bytes of wear leveled storage. Write endurance of 100,000 cycles
// //  Emulated EEPROM does survive Firmware Updates
// //
// //  My Thread:  https://forum.pjrc.com/threads/67472-Teensy-4-1-accessing-the-8-MB-built-in-Flash?p=281210
// //    Paul wrote:
// //      Install the latest beta
// //      https://forum.pjrc.com/threads/67252...no-1-54-Beta-9
// //      
// //      Use the LittleFS library
// //      Create an instance of LittleFS_Program for a filesystem which accesses program memory.
// //
// //    So LittleFs can also store info in Flash, but is not persistent with a firmware update
// //    "Unfortunately, you won't get this until we do a bootloader update, which won't come
// //     until near the end of 2021 or perhaps sometime in 2022"
// //
// #include <EEPROM.h>

// void EEPROM_Write_test(int addr, uint8_t val)
// {
//   EEPROM.write(addr, val);
// }



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
void CRT_Timing_Test_1(void);
void CRT_Timing_Test_2(void);
void CRT_Timing_Test_3(void);
void CRT_Timing_Test_4(void);
void CRT_Timing_Test_5(void);
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
void show_keyboard_codes(void);
void jay_pi(void);
void ulisp(void);
void diag_dir_tapes(void);
void diag_dir_disks(void);
void diag_dir_roms(void);
void diag_dir_root(void);
void list_mount(void);
void set_date(void);
void set_time(void);
void time_adjust_minutes(void);
void time_adjust_hours(void);
void time_up_by_increment(void);
void time_down_by_decrement(void);
void PSRAM_Test(void);
void show(void);
void dump_keys(bool hp85kbd , bool octal);
void ESP_Programmer_Setup(void);

#if ENABLE_TRACE_EMC
void EMC_Info(void);
#endif
#if ENABLE_TRACE_PTR2
void EMC_Ptr2(void);
#endif


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
  {"dir tapes",        diag_dir_tapes},
  {"dir disks",        diag_dir_disks},
  {"media",            report_media},
  {"dir roms",         diag_dir_roms},
  {"dir root",         diag_dir_root},
  {"crt 1",            CRT_Timing_Test_1},
  {"crt 2",            CRT_Timing_Test_2},
  {"crt 3",            CRT_Timing_Test_3},
  {"crt 4",            CRT_Timing_Test_4},
  {"crt 5",            CRT_Timing_Test_5},
  {"sdreadtimer",      diag_sdread_1},
  {"la setup",         Setup_Logic_Analyzer},
  {"la go",            Logic_analyzer_go},
  {"addr",             proc_addr},
  {"clean log",        clean_logfile},
  {"Date",             show_RTC},
  {"SetDate",          set_date},
  {"SetTime",          set_time},
  {"adj min",          time_adjust_minutes},
  {"adj hour",         time_adjust_hours},
  {"U",                time_up_by_increment},
  {"D",                time_down_by_decrement},
  {"pwo",              pulse_PWO},
  {"reset",            pulse_PWO},
  {"dump ram window",  dump_ram_window},                  //  Currently broken
  {"SDCID",            dump_SD_Card_Info},
  {"kbdcode",          show_keyboard_codes},
  {"graphics test",    Simple_Graphics_Test},
  {"PSRAMTest",        PSRAM_Test},
  {"ESP32 Prog",       ESP_Programmer_Setup},
#if ENABLE_TRACE_EMC
  {"EMC",              EMC_Info},
#endif
#if ENABLE_TRACE_PTR2
  {"EMC",              EMC_Ptr2},
#endif
  {"jay pi",           jay_pi},
  {"auxint",           proc_auxint},
  {"jo",               just_once_func},
//{"ulisp",            ulisp},
  {"TABLE_END",        help_0}                            //  Must be uppercase to mark end of table
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Get string from Serial, with editing
//
//  Not currently used, but here is some Scan Codes / Escape sequences. WHY ARE THESE DIFFERENT???
//
//                  As seen by VSCode   As Seen by SecureCRT  As Seen by Tera Term
//  Up Arrow        0xC3 0xA0 0x48      0x1B 0x41             0x1B 0x5B 0x41
//  Left Arrow      0xC3 0xA0 0x4B      0x1B 0x44
//  Down Arrow      0xC3 0xA0 0x50      0x1B 0x42             0x1B 0x5B 0x42
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

//
//  05/21/2021    Add a little keyboard history buffer. Minimal implementation, KBD_HISTORY_LINES strings of SERIAL_STRING_MAX_LENGTH characters
//                Up arrow gives previous command, down arrow gives next
//                See above comment for Arrow codes
//                Use windows cmd box behavior to guide us:
//                    Up Arrow gives previous command, back to start of history. No wraparound
//                    Down arrow will give next command, if we have moved backwards through buffer with up arrow, will do nothing if we are at end of valid entries
//                    When a command is about to be executed, only store if it does not match the current line in buffer, and add at end, and move pointer to that position
//                    NOT YET: Command "clear history" to delete all history
//

#define KBD_HISTORY_LINES      (10)
#define DEBUG_HISTORY_CODE     ( 0)

//
//  These all depend on auto initialization to 0
//
static char       kbd_history[KBD_HISTORY_LINES][SERIAL_STRING_MAX_LENGTH + 2];
static int8_t     kbd_history_ptr;                                    //  Index of command to return if Up Arrow 0..(KBD_HISTORY_LINES-1)
static int8_t     kbd_history_high_water_mark;                        //  Number of valid entries   0..KBD_HISTORY_LINES
static bool       serial_string_is_from_history = false;

//
//  If an up arrow is seen, replace the serial_string from the buffer
//  at the current pointer position, and decrement the pointer, and dead end at 0
//
void kbd_history_up_arrow()
{
  //
  //  First, make sure there is some history. If not, do nothing
  //
  #if DEBUG_HISTORY_CODE
  Serial.printf("\nUpArrow, hist? %d\n", serial_string_is_from_history);
  #endif
  if (kbd_history_high_water_mark == 0)
  {
    return;
  }
  if (serial_string_is_from_history)
  {   //  if the current serial string is from history without modification, then move the pointer first
    if (kbd_history_ptr)
    {
      kbd_history_ptr--;
    }
  }
  Serial.printf("\x1B[2K\r");                         //  VT100 escape code to clear entire line
  strlcpy(serial_string, kbd_history[kbd_history_ptr], SERIAL_STRING_MAX_LENGTH + 2);
  serial_string_length = strlen(serial_string);
  Serial.printf("EBTKS> %s", serial_string);                 //  Show the command from history
  serial_string_is_from_history = true;
}

//
//  If a down arrow is seen, replace the serial_string from the buffer
//  at the next pointer position. Dead end at the high water mark
//
void kbd_history_down_arrow()
{
  //
  //  First, make sure there is some history. If not, do nothing
  //
  #if DEBUG_HISTORY_CODE
  Serial.printf("\nDownArrow, hist? %d\n", serial_string_is_from_history);
  #endif
  if (kbd_history_high_water_mark == 0)
  {
    return;
  }
  //
  //  If we are at the high water mark, there is no next item to display
  //
  if (kbd_history_ptr == (kbd_history_high_water_mark - 1))
  {
    return;
  }
  if (serial_string_is_from_history)
  {   //  If the current serial string is from history without modification, then move the pointer first
      //  We already know it is less than the high water mark
    kbd_history_ptr++;
  }
  Serial.printf("\x1B[2K\r");                         //  VT100 escape code to clear entire line
  strlcpy(serial_string, kbd_history[kbd_history_ptr], SERIAL_STRING_MAX_LENGTH + 2);
  serial_string_length = strlen(serial_string);
  Serial.printf("EBTKS> %s", serial_string);                 //  Show the command from history
  serial_string_is_from_history = true;
}

//
//  Add a command to the history buffer at the high water mark if it doesn't match any
//  command currently in the buffer. If the buffer is full, discard the
//  oldest entry and shift every thing down. (yes, I know that is not very efficient)
//  Don't call this with an empty line in serial_string
//  If the command is already in the buffer, leave the kbd_history_ptr pointing to it, for a following Up Arrow. This is the way.
//
void kbd_history_add_command()
{
  #if DEBUG_HISTORY_CODE
  Serial.printf("\nADD:  serial_string [%s]  Ptr %2d  hwm %2d\n", serial_string, kbd_history_ptr, kbd_history_high_water_mark);
  #endif

  //
  //  Search to see if command is already in history buffer
  //
  if (kbd_history_high_water_mark)
  {
    for (uint8_t i = 0 ; i < (kbd_history_high_water_mark) ; i++)
    {
      #if DEBUG_HISTORY_CODE
      Serial.printf("test %2d: len[%s] %2d    Len[%s] %2d\n", i, serial_string, strlen(serial_string), kbd_history[i], strlen(kbd_history[i]));
      #endif
      if (strcmp(serial_string, kbd_history[i]) == 0)
      {           //  Found an exact match already in the history buffer
        #if DEBUG_HISTORY_CODE
        Serial.printf("match\n");
        #endif
        kbd_history_ptr = i;
        return;
      }
    }
  }

  //
  //  Test if history buffer is full, and if so, flush oldest command
  //
  if (kbd_history_high_water_mark == KBD_HISTORY_LINES)
  {   //  Buffer is full, so make some room by throwing out the oldest entry
    for (uint8_t i = 1 ; i < KBD_HISTORY_LINES ; i++)
    { //  shift everything down one line 
      strlcpy(kbd_history[i - 1], kbd_history[i], SERIAL_STRING_MAX_LENGTH + 2);
    }
    //
    //  Now store the line in the last slot
    //
    strlcpy(kbd_history[KBD_HISTORY_LINES - 1], serial_string, SERIAL_STRING_MAX_LENGTH + 2);
    kbd_history_ptr = KBD_HISTORY_LINES - 1;    //  and point to it
    return;
  }
  //
  //  History buffer is not full, so append at the end
  //
  strlcpy(kbd_history[kbd_history_high_water_mark], serial_string, SERIAL_STRING_MAX_LENGTH + 2);
  kbd_history_ptr = kbd_history_high_water_mark;
    kbd_history_high_water_mark++;
}

#if DEBUG_HISTORY_CODE
void show_history_buffer()
{
  Serial.printf("Dump of History buffer. Pointer %2d  HWM %2d\n", kbd_history_ptr, kbd_history_high_water_mark);
  for (uint8_t i = 0 ; i < KBD_HISTORY_LINES ; i++)
  {
    Serial.printf("%2d  [%s]\n", i, kbd_history[i]);
  }
  //Serial.printf("\n");
}
#endif

static bool prompt_shown = false;
static int8_t escape_sequence_state = 0;

void get_serial_string_poll(void)
{
  char  current_char;

  if (!prompt_shown)
  {
    #if DEBUG_HISTORY_CODE
    show_history_buffer();
    #endif
    Serial.printf("EBTKS> ");
    prompt_shown = true;
  }

  if (serial_string_available)
  {
    LOGPRINTF("Error: get_serial_string_poll() called, but serial_string_available is true\n");
    Ctrl_C_seen = true;     //  Not really a Ctrl-C , but treat it this way to error out of whatever is calling this
    return;
  }

  if (!Serial.available()) return;       //  No new characters available

  while(1)    //  Got at least 1 new character
  {
    current_char = Serial.read();
    //
    //  If an escape sequence is coming in (Up arrow, Down Arrow), we don't want to echo this to the terminal,
    //  or add to serial_string. So we need to either do some look ahead, or put stuff into a temp buffer, or trivial state machine
    //  Tough choice. All options are messy. I think state machine is cleaner. We will see...
    //
    if (current_char == 0x1B)   //  No plan to handle multiple escapes
    {
      escape_sequence_state = 1;
      return;
    }
    if (escape_sequence_state == 1)
    { //                                              We have seen ESC
      //
      //  The only thing we support is esc + [ , so if we don't get '[' , escape processing resets
      //
      if (current_char == 0x5B)
      {
        escape_sequence_state = 2;
      }
      else
      {
        escape_sequence_state = 0;
      }
      return;
    }
    if (escape_sequence_state == 2)
    { //                                              We have seen ESC + 0x5B
      //
      //  The only thing we support is esc + [
      //
      escape_sequence_state = 0;                  //  Regardless of what follows, this ends escape processing
      if (current_char == 'A')                    //  We have found A, Up Arrow
      {
        kbd_history_up_arrow();
        return;
      }
      if (current_char == 'B')                    //  We have found B, Down Arrow
      {
        kbd_history_down_arrow();
        return;
      }
      return;
    }
    //
    //  End of escape processing
    //

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
        #if DEBUG_HISTORY_CODE
        Serial.printf("\n%s\n", serial_string);
        #endif
        if (serial_string_length > 0)
        {
          kbd_history_add_command();
        }
        while(Serial.read() >= 0){};                        //  Remove any remaining characters in the serial channel. This will kill type-ahead, which might not be a good idea
        prompt_shown = false;
        serial_string_is_from_history = false;
        return;
        break;
      case '\b':                                            //  Backspace. If the string (so far) is not empty, delete the last character, otherwise ignore
        if (serial_string_length > 0)
        {
          serial_string_length--;
          serial_string[serial_string_length] = '\0';
          Serial.printf("\b \b");                           //  Backspace , Space , Backspace
          serial_string_is_from_history = false;
        }
        break;
      case '\x03':                                          //  Ctrl-C.  Flush any current string. This fails with the VSCode terminal because it kils the terminal
        serial_string_length    = 0;
        Serial.printf("  ^C\n");
        Ctrl_C_seen = true;
        prompt_shown = false;
        serial_string_is_from_history = false;
        break;
      case '\x14':                                          //  Ctrl-T.  Show the current incomplete command
        serial_string[serial_string_length] = '\0';
        Serial.printf("\nEBTKS> %s", serial_string);
        break;
      default:                                              //  If buffer is full, the characteris ignored
        if (serial_string_length >= SERIAL_STRING_MAX_LENGTH)
        {
          break;    //  Ignore character, no room for it
        }
        serial_string[serial_string_length++] = current_char;
        serial_string[serial_string_length] = '\0';
        Serial.printf("%c", current_char);
        serial_string_is_from_history = false;
    }
    if (!Serial.available()) return;                           //  No additional new characters available
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
  //  Special version (undocumented for end users) of setdate
  //
  if( (strncasecmp(serial_string , "setdate ", 8) == 0) && (strlen(serial_string) == 18) )
  {
    set_date();
    serial_string_used();
    return;
  }

  //
  //  Special version (undocumented for end users) of settime
  //
  if( (strncasecmp(serial_string , "settime ", 8) == 0) && (strlen(serial_string) == 13) )
  {
    set_time();
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

  if(strcasecmp(serial_string + 5, "CRTboot") == 0)    //  Not strncasecmp() so nothing after CRT
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

  if(strcasecmp(serial_string + 5, "crtvis") == 0)      //  Not strncasecmp() so nothing after CRTVis
  {
    Send_Visible_CRT_to_Serial();
    return;
  }

  if(strcasecmp(serial_string + 5, "crtall") == 0)      //  Not strncasecmp() so nothing after CRTAll
  {
    Send_All_CRT_to_Serial();
    return;
  }

  if(strcasecmp(serial_string + 5, "media") == 0)       //  Not strncasecmp() so nothing after CRT
  {
    report_media();
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
    FsFile showfile = SD.open(serial_string + 5, FILE_READ);
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
  Serial.printf("1     Help for Display Information\n");
  Serial.printf("2     Help for Diagnostic commands\n");
  Serial.printf("3     Help for Directory and Time/Date Commands\n");
//  Serial.printf("4     Help for Auxiliary programs\n");
  Serial.printf("5     Help for Developers\n");
  Serial.printf("6     Help for Demo\n");
//  Serial.printf("7     Help for Transient commands\n");
  Serial.printf("\n");
  show_RTC();
  Serial.printf("\n");
}

void help_1(void)
{
  Serial.printf("Commands to Display Information\n");
  Serial.printf("show -----    Show commands have a parameter after exactly 1 space\n");
  Serial.printf("     log      Show the System Logfile\n");
  Serial.printf("     boot     Show the messages from the boot process, sent to Serial port\n");
  Serial.printf("     CRTboot  Show the messages sent to the CRT at startup\n");
  Serial.printf("     config   Show the CONFIG.TXT file\n");
  Serial.printf("     media    Show the Disk and Tape assignments\n");
  Serial.printf("     mb       Display current mailboxes and related data\n");
  Serial.printf("     CRTVis   Show what is visible on the CRT\n");
  Serial.printf("     CRTAll   Show all of the CRT ALPHA memory\n");
  Serial.printf("     key85_O  Display HP85 Special Keys in Octal\n");
  Serial.printf("     key85_D  Display HP85 Special Keys in Decimal\n");
  Serial.printf("     key87_O  Display HP87 Special Keys in Octal\n");
  Serial.printf("     key87_D  Display HP87 Special Keys in Decimal\n");
  Serial.printf("     other    Anything else is a file name path\n\n");
  //Serial.printf("media         Show the currently mounted tape and disk media\n");  // still supported for now, but maybe delete later
  Serial.printf("\n");
}

void help_2(void)
{
  //uint16_t    nxtmem;
  //uint16_t    i;

  Serial.printf("Commands for Diagnostic\n");
  Serial.printf("la setup      Set up the logic analyzer\n");
  Serial.printf("la go         Start the logic analyzer\n");
  Serial.printf("addr          Instantly show where HP85 is executing\n");
  Serial.printf("kbdcode       Show key codes for next 10 characters in the keyboard buffer\n");
  Serial.printf("clean log     Clean the Logfile on the SD Card\n");
  Serial.printf("sdreadtimer   Test Reading with different start positions\n");
  Serial.printf("SDCID         Display the CID information for the SD Card\n");
  Serial.printf("PSRAMTest     Test the 8 MB PSRAM. You probably should do the PWO command when test has finished\n");
  Serial.printf("ESP32 Prog    Activate a passthrough serial path to program the ESP32\n");
#if ENABLE_TRACE_EMC
  Serial.printf("EMC           display info about the EMC emulation\n");
#endif
#if ENABLE_TRACE_PTR2
  Serial.printf("EMC           display info tracing EMC Ptr2 over a limited range\n");
#endif
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
  Serial.printf("Directory and Date/Time Commands\n");
//Serial.printf("cfgrom        #Select active roms\n");      //  Not yet Implemented
  Serial.printf("dir tapes     Directory of available tapes\n");
  Serial.printf("dir disks     Directory of available disks\n");
  Serial.printf("dir roms      Directory of available ROMs\n");
  Serial.printf("dir root      Directory of available ROMs\n");
  Serial.printf("Date          Show current Date and Time\n");
  Serial.printf("SetDate       Set the Date in MM/DD/YYYY format\n");
  Serial.printf("SetTime       Set the Time in HH:MM 24 hour format\n");
  Serial.printf("adj min       The U and D command will adjust minutes\n");
  Serial.printf("adj hour      The U and D command will adjust hours\n");
  Serial.printf("U             Increment the time by 1 minute or hour\n");
  Serial.printf("D             Decrement the time by 1 minute or hour\n");
//Serial.printf("screen        #Screen emulation\n");        //  Not yet Implemented
//Serial.printf("keyboard      #Keyboard emulation\n");      //  Not yet Implemented
  Serial.printf("\n");
}

void help_4(void)
{
  Serial.printf("Commands for Auxiliary programs\n");
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
  Serial.printf("crt 5         Test writing text to HP86/87 CRT\n");
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

void diag_dir_tapes(void)
{
  diag_dir_path("/tapes");
}

void diag_dir_root(void)
{
  diag_dir_path("/");
}

void diag_dir_disks(void)
{
  diag_dir_path("/disks");
}

void diag_dir_roms(void)
{
  if (IS_HP86_OR_HP87)
  {
    diag_dir_path("/roms87");
  }
  else
  {
    diag_dir_path("/roms85");
  }
}

//
//  Explore the time it takes to do SDREADs of less than a sector at a time
//
//      need to circle back to this: ifstream sdin("getline.txt");      ##########
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
  FsFile testfile = SD.open("/CONFIG.TXT", FILE_READ);
  if(!testfile)
  {
    Serial.printf("File open for /CONFIG.TXT failed\n");
    return;
  }
  file_offset = 0;
  testfile.setTimeout(1000);        //  Default timeout is 1000 ms
  Stop_time = Start_time_total = systick_millis_count;
  while(testfile.available64())			//  This will cause a compile time error when we upgrade to 1.57  Look for available64()
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
  testfile = SD.open("/CONFIG.TXT", FILE_READ);
  if(!testfile)
  {
    Serial.printf("File open for /CONFIG.TXT failed\n");
    return;
  }
  file_offset = 0;
  testfile.setTimeout(0);
  Stop_time = Start_time_total = systick_millis_count;
  while(testfile.available64())				//  Probably change to available64() when 1.57
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
  testfile = SD.open("/CONFIG.TXT", FILE_READ);
  if(!testfile)
  {
    Serial.printf("File open for /CONFIG.TXT failed\n");
    return;
  }
  file_offset = 0;
  testfile.setTimeout(1000);        //  Default timeout is 1000 ms (but it should have no effect)
  Stop_time = Start_time_total = systick_millis_count;
  while(testfile.available64())			//  Probably change to available64() when 1.57
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
        Serial.printf("%d%.2d    %s\n", HPIB_Select, device, filename);
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

void show_keyboard_codes(void)
{
  int         chars_to_report = 10;
  uint8_t     current_char;

  serial_string_used();
  while (chars_to_report)
  {
    while (!Serial.available()) {}
    //
    //  Got a charcter
    //
    current_char = Serial.read();
    Serial.printf("Char code 0x%02X\n", current_char);
    chars_to_report--;
  }
  //
  //  Drain any remaining charcters
  //
  while (Serial.available())
  {
    current_char = Serial.read();
  }
  Serial.printf("\n");
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
//  FsFile          file;
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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

  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
//////  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
//////  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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
  prompt_shown = true;          //  Suppress the EBTKS> prompt
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

  Serial.printf("Logic Analyzer Setup complete. Type 'la go' to start\n\n\n");
}

static  uint32_t    LA_Heartbeat_Timer;

void Logic_analyzer_go(void)
{
//
//  This re-initialization allows re issuing la_go without re-entering parameters
//
  memset(Logic_Analyzer_Data_1, 0, 1024*sizeof(uint32_t));
  memset(Logic_Analyzer_Data_2, 0, 1024*sizeof(uint32_t));
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

  Serial.printf("    Time Sample   Address     Data   Cycle RSE  DRP DMA /IR /HAL          LA Data 1\n");
  Serial.printf("     us                              WRLF  LEC           LX  TX\n");
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
    Serial.printf("  %03o", (Logic_Analyzer_Data_2[j] & 0x0000FF00) >> 8 );
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

#define DIAG_WS2812_LEDS        (0)
#define LED_DATA_LENGTH         (6+(DIAG_WS2812_LEDS*3))      //  so 6 or 9

//
//  Philip's replacement of the FASTLED library
//
uint8_t   led_data[LED_DATA_LENGTH];         //  Extra 3 bytes so that we can send out diagnostic stuff out of LED 2

#if !DIAG_WS2812_LEDS
//
//  Standard WS2812E timing
//                                                                          Data sheet, ns
//                                                                          Min     Max
#define WS2812_0_ON_VAL          300      //  Scope measurement is 336      220     380
#define WS2812_0_OFF_VAL        1300      //  Scope measurement is 1290     580     1000
#define WS2812_1_ON_VAL         1300      //  Scope measurement is 1320     580     1000
#define WS2812_1_OFF_VAL         300      //  Scope measurement is 296      220     420
#define WS2812_FRAME_OFF_VAL  500000      //                                280000
#else
//
//  Experimental tweaked set, slower, but should be rock-solid
//
#define WS2812_0_ON_VAL          400
#define WS2812_0_OFF_VAL        2000
#define WS2812_1_ON_VAL         2000
#define WS2812_1_OFF_VAL        2000
#define WS2812_FRAME_OFF_VAL 1000000      //                                280000
#endif

#define SET_NEOPIX              (GPIO_DR_SET_NEOPIX_TSY    = BIT_MASK_NEOPIX_TSY)
#define CLEAR_NEOPIX            (GPIO_DR_CLEAR_NEOPIX_TSY  = BIT_MASK_NEOPIX_TSY)
#define TOGGLE_NEOPIX           (GPIO_DR_TOGGLE_NEOPIX_TSY = BIT_MASK_NEOPIX_TSY)

// void WS2812_init(void)
// {
//   led_data[0] = 0;    //  LED 1 G     //  GRB is the data sequence that the WS2812B/E require , nor RGB
//   led_data[1] = 0;    //  LED 1 R
//   led_data[2] = 0;    //  LED 1 B
//   led_data[3] = 0;    //  LED 2 G
//   led_data[4] = 0;    //  LED 2 R
//   led_data[5] = 0;    //  LED 2 B
// #if DIAG_WS2812_LEDS
//   led_data[6] = 0xF0; //  Test byte 1
//   led_data[7] = 0xAA; //  Test byte 2
//   led_data[8] = 0x0F; //  Test byte 3
// #endif
//   WS2812_update();
// }

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

  for(i = 0 ; i < LED_DATA_LENGTH ; i++)
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
  release_DMA_request();
  while(DMA_Active){}       // Wait for release
  EBTKS_delay_ns(WS2812_FRAME_OFF_VAL);       //  The > 280 us end of frame delay that moves the shifted
                                              //  data into the LED control registers is not needed, because
                                              //  the fastest HP85 BASIC code with back-to-back calls to SETLED LED,R,G,B
                                              //  takes about 4 ms. So BASIC can't generate LED updates too quickly that
                                              //  we have to expressly generate the 280+ us delay
                                              //
                                              //  Update, this delay was commented out, and may have been the root cause
                                              //  of some LED initialization failures.

}

//    Memory usage analysis of FASTLED library (that needs inverted data, and has LSB of all data stuck high)
//    versus the above purpose built code for EBTKS
//
//    TLDR: FASTLED library is 7 KB and doesn't work
//          This code is 320 bytes and works great.   //  at time of writing this note. Not updated when diag code was added 2021_10_06
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Test PSRAM  This trashes PSRAM, so must do PWO after
//
//  This was lifted from Paul's GitHub, and slightly modified to fit in here.
//

#define PSRAM_DMA_HALT         (0)                //  Set this to 1 to bracket PSRAM R/W with HP85 HALT and TEENSY global interrupt disable
                                                  //  via the HP85 DMA mode

extern "C" uint8_t external_psram_size;           //  startup.c does a simple test to see if there is either no PSRAM, or 8MB (one chip) or
                                                  //  16 MB (2 chips).  Returns one of {0,8,16}  Base address is 0x70000000
                                                  //  startup.c neither zeroes this memory , or pre-loads it (as of 12/27/2023)

extern "C" uint32_t *	  _extram_start;
extern "C" uint32_t *	  _extram_end;

bool memory_ok = false;

uint32_t *memory_begin, *memory_end;

bool check_fixed_pattern(uint32_t pattern);
bool check_lfsr_pattern(uint32_t seed);

//
//  The DMA stuff is because it seems that accessing PSRAM a LOT causes problems for the HP85
//  We aren't actually doing DMA, we are just using DMA mode to stop the HP85 doing bus cycles
//  that need to be serviced with minimal latency. We saw a similar issue in the RTC code.
//

void PSRAM_Test(void)
{
	Serial.printf("EXTMEM Memory Test, %d Mbyte\n", external_psram_size);
	Serial.printf("EXTMEM Memory Alloc start  %08X\n", _extram_start);
	Serial.printf("EXTMEM Memory Alloc end    %08X\n", _extram_end);

	Serial.printf("\nHP8x will be put into DMA mode for duration of the test\n");

	Serial.printf("%8d bytes of EXTMEM are used by application, and won't be tested\n", (uint32_t)((uint32_t)_extram_end) - (uint32_t)_extram_start);

	if (external_psram_size == 0) return;

	const float clocks[4] = {396.0f, 720.0f, 664.62f, 528.0f};
	const float frequency = clocks[(CCM_CBCMR >> 8) & 3] / (float)(((CCM_CBCMR >> 29) & 7) + 1);

	Serial.printf(" CCM_CBCMR=%08X (%.1f MHz)\n", CCM_CBCMR, frequency);

	
	memory_begin = (uint32_t *)(&_extram_end + 16);                     //  Start 16 * 4 bytes above the end of allocated PSRAM, so allocated memory is not tested
  memory_begin = (uint32_t *)(0xFFFFFFF0 & (uint32_t)(memory_begin)); //  but make sure it is on a word boundary
	memory_end =   (uint32_t *)(0x70000000 + (external_psram_size * 1048576));

  Serial.printf("Start at 0x%08X  end at 0x%08X\n\n", memory_begin, memory_end);

  assert_DMA_Request();
  while(!DMA_Active){};     //  Wait for acknowledgment, and Bus ownership. This also disables all EBTKS interrupts, so no pinChange_isr() and no USB Serial

  if (!check_fixed_pattern(0x5A698421)) return;
	if (!check_lfsr_pattern(2976674124ul)) return;
	if (!check_lfsr_pattern(1438200953ul)) return;
	if (!check_lfsr_pattern(3413783263ul)) return;
	if (!check_lfsr_pattern(1900517911ul)) return;
	if (!check_lfsr_pattern(1227909400ul)) return;
	if (!check_lfsr_pattern(276562754ul)) return;
	if (!check_lfsr_pattern(146878114ul)) return;
	if (!check_lfsr_pattern(615545407ul)) return;
	if (!check_lfsr_pattern(110497896ul)) return;
	if (!check_lfsr_pattern(74539250ul)) return;
	if (!check_lfsr_pattern(4197336575ul)) return;
	if (!check_lfsr_pattern(2280382233ul)) return;
	if (!check_lfsr_pattern(542894183ul)) return;
	if (!check_lfsr_pattern(3978544245ul)) return;
	if (!check_lfsr_pattern(2315909796ul)) return;
	if (!check_lfsr_pattern(3736286001ul)) return;
	if (!check_lfsr_pattern(2876690683ul)) return;
	if (!check_lfsr_pattern(215559886ul)) return;
	if (!check_lfsr_pattern(539179291ul)) return;
	if (!check_lfsr_pattern(537678650ul)) return;
	if (!check_lfsr_pattern(4001405270ul)) return;
	if (!check_lfsr_pattern(2169216599ul)) return;
	if (!check_lfsr_pattern(4036891097ul)) return;
	if (!check_lfsr_pattern(1535452389ul)) return;
	if (!check_lfsr_pattern(2959727213ul)) return;
	if (!check_lfsr_pattern(4219363395ul)) return;
	if (!check_lfsr_pattern(1036929753ul)) return;
	if (!check_lfsr_pattern(2125248865ul)) return;
	if (!check_lfsr_pattern(3177905864ul)) return;
	if (!check_lfsr_pattern(2399307098ul)) return;
	if (!check_lfsr_pattern(3847634607ul)) return;
	if (!check_lfsr_pattern(27467969ul)) return;
	if (!check_lfsr_pattern(520563506ul)) return;
	if (!check_lfsr_pattern(381313790ul)) return;
	if (!check_lfsr_pattern(4174769276ul)) return;
	if (!check_lfsr_pattern(3932189449ul)) return;
	if (!check_lfsr_pattern(4079717394ul)) return;
	if (!check_lfsr_pattern(868357076ul)) return;
	if (!check_lfsr_pattern(2474062993ul)) return;
	if (!check_lfsr_pattern(1502682190ul)) return;
	if (!check_lfsr_pattern(2471230478ul)) return;
	if (!check_lfsr_pattern(85016565ul)) return;
	if (!check_lfsr_pattern(1427530695ul)) return;
	if (!check_lfsr_pattern(1100533073ul)) return;
	if (!check_fixed_pattern(0x55555555)) return;
	if (!check_fixed_pattern(0x33333333)) return;
	if (!check_fixed_pattern(0x0F0F0F0F)) return;
	if (!check_fixed_pattern(0x00FF00FF)) return;
	if (!check_fixed_pattern(0x0000FFFF)) return;
	if (!check_fixed_pattern(0xAAAAAAAA)) return;
	if (!check_fixed_pattern(0xCCCCCCCC)) return;
	if (!check_fixed_pattern(0xF0F0F0F0)) return;
	if (!check_fixed_pattern(0xFF00FF00)) return;
	if (!check_fixed_pattern(0xFFFF0000)) return;
	if (!check_fixed_pattern(0xFFFFFFFF)) return;
	if (!check_fixed_pattern(0x00000000)) return;

  release_DMA_request();
  while(DMA_Active){};          //  Wait for release

//
//  With the SCOPE_2 stuff turned on below, the test takes about 7 minutes
//  Turning SCOPE_2 stuff off ahs little effect on timing.
//

	Serial.printf("All memory tests passed :-)\n");
  Serial.printf("\nEven though this test only tested memory not used by the main application,\n");
  Serial.printf("\nit seems to still interfere with the operation of the Series80 computer.\n");
  Serial.printf("\nSo type PWO now, to reset both the Series80 computer and EBTKS.\n");
	memory_ok = true;
}

bool fail_message(volatile uint32_t *location, uint32_t actual, uint32_t expected)
{
	Serial.printf(" Error at %08X, read %08X but expected %08X\n", (uint32_t)location, actual, expected);
	return false;
}

//
// fill the entire RAM with a fixed pattern, then check it
//

bool check_fixed_pattern(uint32_t pattern)
{
	volatile uint32_t *p;
	Serial.printf("testing with fixed pattern %08X\n", pattern);
  Serial.flush();
  delay(500);       //  Give the serial data time to go out the door (via USB)

  if (Serial.available())             //  check for early exit
  {
    if (Serial.read() == '\x03')      //  Exit with Ctrl_C_seen and false status, otherwise throw the character away
    {
      Ctrl_C_seen = true;
      Serial.printf("CTRL-C seen\n");
      return false;
    }
  }

	for (p = memory_begin; p < memory_end; p++)
	{
		*p = pattern;
	}

	arm_dcache_flush_delete((void *)memory_begin, (uint32_t)memory_end - (uint32_t)memory_begin);

	for (p = memory_begin; p < memory_end; p++)
	{
		uint32_t actual = *p;
		if (actual != pattern)
    {
      return fail_message(p, actual, pattern);
    }
	}
	return true;
}

// fill the entire RAM with a pseudo-random sequence, then check it
bool check_lfsr_pattern(uint32_t seed)
{
	volatile uint32_t *p;
	uint32_t reg;

	Serial.printf("testing with pseudo-random sequence, seed=%u\n", seed);
  Serial.flush();
  delay(500);   //  Give the serial data time to go out the door (via USB)

  if (Serial.available())             //  check for early exit
  {
    if (Serial.read() == '\x03')      //  Exit with Ctrl_C_seen and false status, otherwise throw the character away
    {
      Ctrl_C_seen = true;
      Serial.printf("CTRL-C seen\n");
      return false;
    }
  }

	reg = seed;

	for (p = memory_begin; p < memory_end; p++) {
    //  SET_SCOPE_2;
		*p = reg;
    //  CLEAR_SCOPE_2;
		for (int i=0; i < 3; i++) {
			if (reg & 1) {
				reg >>= 1;
				reg ^= 0x7A5BC2E3;
			} else {
				reg >>= 1;
			}
		}
	}
	arm_dcache_flush_delete((void *)memory_begin, (uint32_t)memory_end - (uint32_t)memory_begin);
	reg = seed;
	for (p = memory_begin; p < memory_end; p++) {
    //  SET_SCOPE_2;
		uint32_t actual = *p;
    //  CLEAR_SCOPE_2;
		if (actual != reg)
    {
      return fail_message(p, actual, reg);
    }
		//Serial.printf(" reg=%08X\n", reg);
		for (int i=0; i < 3; i++) {
			if (reg & 1) {
				reg >>= 1;
				reg ^= 0x7A5BC2E3;
			} else {
				reg >>= 1;
			}
		}
	}
	return true;
}

#if ENABLE_TRACE_EMC
struct EMC_Trace_1_Struct {
          uint32_t  ptr_1_contents;
          uint32_t  ptr_2_contents;
          uint32_t  Trace_mask;
          uint32_t  Cycle_count;
          uint8_t   Trace_val;
          uint8_t   Trace_cycleNdx;
};

extern struct EMC_Trace_1_Struct EMC_Trace_1[EMC_DIAG_BUF_SIZE];

extern uint32_t           m_emc_ptr1 , m_emc_ptr2;        //  EMC pointers
extern uint32_t           diag_snap_PTR2;                 //  Looks like this is only needed when ENABLE_TRACE_EMC is enabled
extern bool               diag_snap_lock;                 //  Looks like this is only needed when ENABLE_TRACE_EMC is enabled
extern uint32_t           Trace_index;


void EMC_Info(void)
{
  struct EMC_Trace_1_Struct *trace_ptr;
#if !ENABLE_EMC_SUPPORT
  Serial.printf("The EMC emulation is disabled by a compile time flag\n");
  return;
#endif
  
#if ENABLE_EMC_SUPPORT
  Serial.printf("EMC Configurations base on CONFIG.TXT\n");
  Serial.printf("EMC Support      %s\n",    get_EMC_Enable() ? "true":"false");
  Serial.printf("EMC Num Banks    %d, 32 KB each\n", get_EMC_NumBanks());
  Serial.printf("EMC Start Bank   %d\n", get_EMC_StartBank());
  Serial.printf("EMC Start Addr   %08o Octal\n", get_EMC_StartAddress());
  Serial.printf("EMC End Addr     %08o Octal\n", get_EMC_EndAddress());
  Serial.printf("EMC PTR 1        %08o Octal\n", m_emc_ptr1);
  Serial.printf("EMC PTR 2        %08o Octal\n", m_emc_ptr2);
  Serial.printf("diag_snap_lock   %s\n",    diag_snap_lock ? "true":"false");
  Serial.printf("diag_snap_PTR2   %08o Octal\n", diag_snap_PTR2);
  Serial.printf("Trace_index      %d\n", Trace_index);

  Serial.printf("\ni    val  Ndx    Mask   Ptr1      Ptr2      Cycle\n");
  for (uint32_t i = 0 ; i < Trace_index ;)
  {
    trace_ptr = &EMC_Trace_1[i];
    Serial.printf("%4d ", i);
    Serial.printf("%02X   ",  trace_ptr->Trace_val);
    Serial.printf("%3d ",     trace_ptr->Trace_cycleNdx);
    Serial.printf("%08X ",    trace_ptr->Trace_mask);
    Serial.printf("%08X ",    trace_ptr->ptr_1_contents);
    Serial.printf("%08X  ",   trace_ptr->ptr_2_contents);
    Serial.printf("%10d",     trace_ptr->Cycle_count);
    Serial.printf("\n");

    i++;
    if ((i % 3) == 0)
    {
      Serial.printf("\n");
    }
  }
  diag_snap_lock = false;
  diag_snap_PTR2 = 0xFFFFFFFF;
  Trace_index = 0;

#endif
}

#endif

#if ENABLE_TRACE_PTR2
struct EMC_Trace_2_struct {
          uint32_t  ptr_2_contents_pre;
          uint32_t  ptr_2_contents_post;
          uint32_t  Cycle_count;
          uint8_t   Trace_cycleNdx;
          uint8_t   Trace_op;
          uint8_t   Dec_disp;
          uint8_t   EMC_DRP;
          uint8_t   EMC_DRP_Loaded;
};

extern struct EMC_Trace_2_struct EMC_Trace_2[EMC_PTR2_BUF_SIZE];
extern uint32_t  Trace_index;

void EMC_Ptr2(void)
{
  struct EMC_Trace_2_struct *trace_ptr;
#if !ENABLE_EMC_SUPPORT
  Serial.printf("The EMC emulation is disabled by a compile time flag\n");
  return;
#endif
  
#if ENABLE_EMC_SUPPORT
  Serial.printf("EMC Configurations base on CONFIG.TXT\n");
  Serial.printf("EMC Support      %s\n",    get_EMC_Enable() ? "true":"false");
  Serial.printf("EMC Num Banks    %d, 32 KB each\n", get_EMC_NumBanks());
  Serial.printf("EMC Start Bank   %d\n", get_EMC_StartBank());
  Serial.printf("EMC Start Addr   %08o Octal\n", get_EMC_StartAddress());
  Serial.printf("EMC End Addr     %08o Octal\n", get_EMC_EndAddress());
  Serial.printf("Trace_index      %d\n", Trace_index);

  Serial.printf("\n  i  Ptr2 Pre Ptr2 Post  Op   Ndx    Cycle   disp DRP   DRPLoad\n\n");
  for (uint32_t i = 0 ; i < Trace_index ; i++)
  {
    trace_ptr = &EMC_Trace_2[i];
    Serial.printf("%4d ", i);
    Serial.printf("%8d ", trace_ptr->ptr_2_contents_pre);
    Serial.printf(" %8d  ", trace_ptr->ptr_2_contents_post);
    switch (trace_ptr->Trace_op)
    {
      case 1:
        Serial.printf("DecM ");
        break;
      case 2:
        Serial.printf("Dec1 ");
        break;
      case 3:
        Serial.printf("Rd I ");
        break;
      case 4:
        Serial.printf("Wr I ");
        break;
      default:
        Serial.printf("ERR! ");
        break;
    }
    Serial.printf("%3d ", trace_ptr->Trace_cycleNdx);
    Serial.printf("%10d", trace_ptr->Cycle_count);
    Serial.printf(" %2d   %03o", trace_ptr->Dec_disp, trace_ptr->EMC_DRP & 077);    //  Mask of the DRP instruction bits (top 2 bits), keep the lower 6
    // if (Trace_op[i] < 2)
    // {
    //   Serial.printf(" %2d   %03o", Dec_disp[i], EMC_DRP[i] & 077);    //  Mask of the DRP instruction bits (top 2 bits), keep the lower 6
    // }
    // else
    // {
    //   Serial.printf("         ");
    // }
    if (trace_ptr->EMC_DRP_Loaded != 0xff)
    {
      Serial.printf("   %03o", trace_ptr->EMC_DRP_Loaded);
    }
    Serial.printf("\n");
  }
  Trace_index = 0;

#endif
}

#endif


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

//
//  Look here for docs on Time functions:  https://github.com/PaulStoffregen/Time
//

void show_RTC(void)
{
  // digital clock display of the time
  Serial.printf("The time is %2d", hour());
  Serial.printf(":%02d", minute());
  Serial.printf(":%02d", second());
  Serial.printf(" on %s the %d%.2s", dayNames_P[weekday()], day(), &daySuffix[day()*2]);
  Serial.printf(" of %s", monthNames_P[month()] );
  Serial.printf(" %4d\n",year());

  //
  //  Debug Check Suffix stuff works
  //
  // Serial.printf("\nCheck suffixes\n");
  // for(int i = 0 ; i <= 31 ; i++)
  // {
  //   Serial.printf("%d  %.2s\n", i, &daySuffix[i*2]);
  // }

  //
  //  Debugging the clock not retaining the time of day/date across power fail
  //
  //  Serial.printf("_VectorsRam[15] = %08x\n", _VectorsRam[15]);
  //  Serial.printf("now()           = %08x\n", now());
  //  Serial.printf("SNVS_HPCR       = %08x  LSB is RTC_EN , Bit 16 is sync bit, but apparently always reads 0\n", SNVS_HPCR);
}

//
//  Apparently, while there are no touches of the interrupt logic anywhere in the call chain of
//      Teensy3Clock.set(now());
//  it never the less, affects the latency of interrupts, and of course, EBTKS has no tollerance
//  for that, as the pinChange_isr() expects/depends on sub 100 ns latency.
//  Teensy3Clock.set(now()) actually locks out (or delays response) of interrupts for 960 uS,
//  which means all HP85 bus cycles that EBTKS must either service (RAM and ROM for example) will
//  fail, as well as EBTKS not seeing I/O register updates, etc..
//  My current guess is that the registers in the RTC are in a different clock domain (and maybe quite slow)
//  and a register reference into this domain locks up everything until completed.
//  We have seen this before, see Teensy_4.0_Notes.txt in the "SdFat           Examples and notes" section
//  The solution is to stop the HP85 from doing any bus transfers while the RTC is being set,
//  which we do by initiating the DMA state, and end DMA when we are done.
//
//  Look here for docs on Time functions:  https://github.com/PaulStoffregen/Time
//

void protected_rtc_set(void)
{
  assert_DMA_Request();
  while(!DMA_Active){};     //  Wait for acknowledgment, and Bus ownership. Also locks out interrupts on EBTKS, so can't do USB serial or SD card stuff

  Teensy3Clock.set(now());  //  This take 960 uS

  release_DMA_request();
  while(DMA_Active){};      //  Wait for release
}

//
//  Get new date in MM/DD/YYYY format
//
//  Look here for docs on Time functions:  https://github.com/PaulStoffregen/Time
//

void set_date(void)
{
  int set_day     = day();  
  int set_month   = month();
  int set_year    = year(); 

  if(strlen(serial_string) == 18)                     //  Handle undocumented "setdate MM/DD/YYYY"
  {
    memmove(serial_string, &serial_string[8], 11);    // 1 extra to get the trailing 0x00
    goto date_on_commandline;
  }
  serial_string_used();

  Serial.printf("\nThe current date setting is ");
  Serial.printf("%.2d/%.2d/%4d in MM/DD/YYYY format\n", set_month, set_day, set_year);
  Serial.printf("Enter a new date, or just type return to leave it unchanged\n");

set_date_try_again:
  Serial.printf("\nDate> ");
  prompt_shown = true;
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false;
    return;                                     //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0)
  {
    Serial.printf("No date change\n");
    serial_string_used();
    return;
  }                                             //  Keep existing Date

date_on_commandline:
  if ((strlen(serial_string) != 10) || (serial_string[2] != '/') || (serial_string[5] != '/') )
  {
    Serial.printf("Please enter the date in MM/DD/YYYY format, including the slash characters\n");
    serial_string_used();
    goto set_date_try_again;
  }

  //
  //  depend on atoi() stopping conversion when it gets to a / or end of string
  //
  set_month = atoi(serial_string);
  set_day   = atoi(&serial_string[3]);
  set_year  = atoi(&serial_string[6]);
  serial_string_used();

  //
  //  Validate the date.
  //
  //  We are using UNIX/POSIX time, with an epoch of 1/1/1970 . The time data is a signed 32 bit integer.
  //  The minimum representable date is Friday 1901-12-13, and the maximum representable date is Tuesday 2038-01-19.
  //  One second after 03:14:07 UTC 2038-01-19 this representation will overflow
  //
  //  The warantee for all EBTKS expires on 1/19/2038 at 03:14:06 UTC . Don't say you wern't warned
  //
  if ((set_year > 2038) || (set_year < 1902))
  {
    Serial.printf("Specified Year is outside the range 1902 to 2038\n");
    goto set_date_try_again;
  }
  
  if ((set_month > 12) || (set_month < 1))
  {
    Serial.printf("Specified Month is outside the range 1 to 12\n");
    goto set_date_try_again;
  }

  //
  //  Deal with February
  //
  if (set_month == 2)
  {
    if (((set_year % 4 == 0) && (set_year % 100 != 0)) || (set_year % 400 == 0))    //  This may be correct for the range 1902 to 2038
    {   //  We have a Leap Year, and the month is February
      if ((set_day > 29) || (set_day < 1))
      {
        Serial.printf("Specified Day in February in a leap year is outside the range 1 to 29\n");
        goto set_date_try_again;
      }
    }
    else
    {   //  Not a leap year
      if ((set_day > 28) || (set_day < 1))
      {
        Serial.printf("Specified Day in February in a non-leap year is outside the range 1 to 28\n");
        goto set_date_try_again;
      }
    }
  }

  //
  //  Now deal with months with 30 or 31 days, 30 day months first
  //

  if ((set_month == 4) || (set_month == 6) || (set_month == 9) || (set_month == 11))
  {
    if ((set_day > 30) || (set_day < 1))
      {
        Serial.printf("Specified Day for %s is outside the range 1 to 30\n", monthNames_P[set_month]);
        goto set_date_try_again;
      }
  }
  else
  {
    if ((set_day > 31) || (set_day < 1))
      {
        Serial.printf("Specified Day for %s is outside the range 1 to 31\n", monthNames_P[set_month]);
        goto set_date_try_again;
      }
  }

  //
  //  If we get here, we apparently have a valid Day, Month, Year
  //

  setTime(hour(), minute(), second(), set_day, set_month, set_year);

  protected_rtc_set();

  Serial.printf("Date set to\n");
  show_RTC();
  Serial.printf("\n");
}

//
//  Look here for docs on Time functions:  https://github.com/PaulStoffregen/Time
//

void set_time(void)
{
  int set_hour    = hour();  
  int set_minute  = minute();

  if(strlen(serial_string) == 13)                     //  Handle undocumented "settime hh:mm"
  {
    memmove(serial_string, &serial_string[8], 6);     //  1 extra to get the trailing 0x00
    goto time_on_commandline;
  }
  serial_string_used();

  Serial.printf("\nThe current time setting is ");
  Serial.printf("%.2d:%.2d in HH:MM 24 hour format. Hours are 0 .. 23\n", set_hour, set_minute);
  Serial.printf("Enter a new time, or just type return to leave it unchanged\n");

set_time_try_again:
  Serial.printf("\nTime> ");
  prompt_shown = true;
  if (!wait_for_serial_string())
  {
    Ctrl_C_seen = false;
    return;                                     //  Got a Ctrl-C , so abort command
  }

  if (strlen(serial_string) == 0)
  {
    Serial.printf("No time change\n");
    serial_string_used();
    return;
  }                                             //  Keep existing Time

time_on_commandline:
  if ((strlen(serial_string) != 5) || (serial_string[2] != ':') )
  {
    Serial.printf("Please enter the time in HH:MM 24 hour format, including the : character\n");
    serial_string_used();
    goto set_time_try_again;
  }

  //
  //  depend on atoi() stopping conversion when it gets to a : or end of string
  //
  set_hour    = atoi(serial_string);
  set_minute  = atoi(&serial_string[3]);
  serial_string_used();

  //
  //  Validate the time.
  //

  if ((set_hour > 23) || (set_hour < 0))
  {
    Serial.printf("Specified Hour is outside the range 0 to 23\n");
    goto set_time_try_again;
  }
  
  if ((set_minute > 59) || (set_minute < 0))
  {
    Serial.printf("Specified Minutes is outside the range 0 to 59\n");
    goto set_time_try_again;
  }

  //
  //  If we get here, we apparently have a valid hours and minutes
  //

  setTime(set_hour, set_minute, 0, day(), month(), year());

  protected_rtc_set();

  show_RTC();
  Serial.printf("\n");

}

void time_adjust_minutes(void)
{
  time_adjust_is_minutes = true;
  Serial.printf("Up/Down adjust (U/D) is now minutes\n");
}

void time_adjust_hours(void)
{
  time_adjust_is_minutes = false;
  Serial.printf("Up/Down adjust (U/D) is now hours\n");
}

//
//  Look here for docs on Time functions:  https://github.com/PaulStoffregen/Time
//

void time_up_by_increment(void)
{
  time_t t = now();                   // store the current time in time variable t
  if (time_adjust_is_minutes)
  {
    t += 60;
  }
  else
  {
    t += 3600;
  }
  setTime(t);
  protected_rtc_set();

  Serial.printf("Now %2d:%02d\n", hour(),minute());
}

void time_down_by_decrement(void)
{
  time_t t = now();                   // store the current time in time variable t
  if (time_adjust_is_minutes)
  {
    t -= 60;
  }
  else
  {
    t -= 3600;
  }
  setTime(t);
  protected_rtc_set();

  Serial.printf("Now %2d:%02d\n", hour(),minute());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  Helper functions

void str_tolower(char *p)
{
  for (  ; *p ; ++p) *p = ((*p > 0x40) && (*p < 0x5b)) ? (*p | 0x60) : *p;
}



//**********************************************************   From Everett, 9/22/2020 , with just formatting changes to conform with the rest of the source code
//  Checks pT against possibly wild-card-containing pP.                                   and change of case for bool, true, false
//  Returns TRUE if there's a match, otherwise FALSE.
//  pT must NOT have any wildcards, just be a valid filename.
//  pP may or may not have wildcards (* and ?).
//  * matches 0 or more "any characters"
//  ? matches exactly one "any character"
//
//  This uses recursion, so you know this will bite us in the ass some day
//

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



