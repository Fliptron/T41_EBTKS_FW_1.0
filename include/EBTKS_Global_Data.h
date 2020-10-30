//
//      06/27/2020      These Globals were moved from EBTKS.cpp to here.
//
//      The macro ALLOCATE must be defined just once in one of the files
//      that include this file. The logical one is EBTKS.cpp
//
//      Stackoverflow copied my technique and described it here:
//         https://stackoverflow.com/questions/1433204/how-do-i-use-extern-to-share-variables-between-source-files
//

#ifdef ALLOCATE
#define EXTERN  /* nothing */
#else
#define EXTERN  extern
#endif

#include <setjmp.h>

#define PACKED __attribute__((__packed__))

//
//  machine type enumerations. the corresponding string table in EBTKS_SD.cpp needs to follow this
//
enum { MACH_HP85A = 0 , MACH_HP85B , MACH_HP86 , MACH_HP87 , MACH_NUM }; //always ensure MACH_NUM is the last enumeration

///////////////////////////////////////////////////  Uninitialized Globals. Actually initialized to 0x00000000  /////////////////////////////////////////

//  These depend on the automatic initialization to Zero (NULL for pointers , false for bool)
//  https:stackoverflow.com/questions/16015656/are-global-variables-always-initialized-to-zero-in-c?lq=1

struct PACKED AUXROM_RAM
{                                                   //  Octal Addresses   Decimal byte number     AUXROM Internal name
                                                    //                    in this struct
  uint8_t       AR_Mailboxes[32];                   //  070000 - 070037       0 -    31           A.MB0    - A.MB31
  uint16_t      AR_Lengths[8];                      //  070040 - 070057      32 -    47           A.BLEN0  - A.BLEN7
  uint16_t      AR_Usages[8];                       //  070060 - 070077      48 -    63           A.BUSE0  - A.BUSE7
  uint8_t       AR_SPAR1_CPU_Register_save[64];     //  070100 - 070177      64 -   127           A.R0 , A.R2 , A.R4 , A.R6 , A.R10, A.R12, A.R14, A.R16,
                                                    //                                            A.R20, A.R30, A.R40, A.R50, A.R60, A.R70
  char          AR_SPAR1_CPU_SAD[3];                //  070200 - 070202     128 -   130           A.SAD
  char          AR_TMPP;                            //  070203              131                   A.TMPP          1-BYTE VERY temp storage for non-R6-stack short-term value saving during parsing
  uint16_t      AR_Present;                         //  070204 - 070205     132 -   133           A.RLOG          2-byte bit-log of AUXROMs present
  uint16_t      AR_R12_copy;                        //  070206 - 070207     134 -   135           A.MBR12         2-byte R12 value when "passing stack" to EBTKS
  uint16_t      AR_ERR_NUM;                         //  070210 - 070211     136 -   137           A.ERRN          2-BYTE error# for custom error message #9, returned by AUXERRN function
  uint16_t      AR_TMP[9];                          //  070212 - 070233     138 -   155           A.TMP0 - A.TMP8
  char          AR_FLAGS[4];                        //  070234 - 070237     156 -   159           A.FLAGS         4-BYTE misc flags, gets saved by EBTKS in CFG file and restored at power-on
                                                    //                                                            BIT 0   =0 if / used in paths, =1 if \ used in paths
                                                    //                                                            BIT 1   =0 if LF eol, =1 if CRLF eol
                                                    //                                                            BITS 2-31 unused
  char          AR_Pad_2[80];                       //  070240 - 070357     160 -   239
  char          AR_BUF6_OPTS[8];                    //  070360 - 070367     240 -   247           A.BOPT60 - A.BOPT67   See 85aux2.lst line 2226 for details. TLDR: used as additional Usage values.
                                                    //                                                                  Same mailbox constraints on ownership
  char          AR_BUF0_OPTS[4];                    //  070370 - 070373     248 -   251           A.BOPT00 - A.BOPT03
  char          AR_BUF1_OPTS[4];                    //  070374 - 070374     252 -   255           A.COPT0-A.COPT3        U_cmd-specific options
  char          AR_Buffer_0[256];                   //  070400 - 070777     256 -   511
  char          AR_Buffer_1[256];                   //  071000 - 070377     512 -   767
  char          AR_Buffer_2[256];                   //  071400 - 071777     768 -  1023
  char          AR_Buffer_3[256];                   //  072000 - 072377    1024 -  1279
  char          AR_Buffer_4[256];                   //  072400 - 072777    1280 -  1535
  char          AR_Buffer_5[256];                   //  073000 - 073377    1536 -  1791
  char          AR_Buffer_6[1024];                  //  073400 - 075377    1792 -  2815
  char          AR_Pad_3[256];                      //  075400 - 075777    2816 -  3071
};

//
//    This is similar to the above struct, except for "uint8_t AR_BUF0_OPTS[4];" being replaced with "uint16_t AR_BUF0_OPTS[2];"
//    and many things that were char in the above struct are uint8_t in the AUXROM_RAM_A struct
//    This is to avoid a compiler warning "warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]"
//    when compiling the WROM function in EBTKS_AUXROM_SD_Services.cpp
//
struct PACKED AUXROM_RAM_A
{                                                   //  Octal Addresses   Decimal byte number     AUXROM Internal name
                                                    //                    in this struct
  uint8_t       AR_Mailboxes[32];                   //  070000 - 070037       0 -    31           A.MB0    - A.MB31
  uint16_t      AR_Lengths[8];                      //  070040 - 070057      32 -    47           A.BLEN0  - A.BLEN7
  uint16_t      AR_Usages[8];                       //  070060 - 070077      48 -    63           A.BUSE0  - A.BUSE7
  uint8_t       AR_SPAR1_CPU_Register_save[64];     //  070100 - 070177      64 -   127           A.R0 , A.R2 , A.R4 , A.R6 , A.R10, A.R12, A.R14, A.R16,
                                                    //                                            A.R20, A.R30, A.R40, A.R50, A.R60, A.R70
  uint8_t       AR_SPAR1_CPU_SAD[3];                //  070200 - 070202     128 -   130           A.SAD
  uint8_t       AR_TMPP;                            //  070203              131                   A.TMPP          1-BYTE VERY temp storage for non-R6-stack short-term value saving during parsing
  uint16_t      AR_Present;                         //  070204 - 070205     132 -   133           A.RLOG          2-byte bit-log of AUXROMs present
  uint16_t      AR_R12_copy;                        //  070206 - 070207     134 -   135           A.MBR12         2-byte R12 value when "passing stack" to EBTKS
  uint16_t      AR_ERR_NUM;                         //  070210 - 070211     136 -   137           A.ERRN          2-BYTE error# for custom error message #9, returned by AUXERRN function
  uint16_t      AR_TMP[9];                          //  070212 - 070233     138 -   155           A.TMP0 - A.TMP8
  uint8_t       AR_FLAGS[4];                        //  070234 - 070237     156 -   159           A.FLAGS         4-BYTE misc flags, gets saved by EBTKS in CFG file and restored at power-on
                                                    //                                                            BIT 0   =0 if / used in paths, =1 if \ used in paths
                                                    //                                                            BIT 1   =0 if LF eol, =1 if CRLF eol
                                                    //                                                            BITS 2-31 unused
  uint8_t       AR_Pad_2[80];                       //  070240 - 070357     160 -   239

  union anon1                                       //  This union should hopefully put to bed all the problems with strict-aliasing that keep triggering the
  {                                                   //  following warnings:   dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
   uint8_t       as_uint8_t[8];                       //  070360 - 070367     240 -   247           A.BOPT60 - A.BOPT67
   char          as_char[8];                          //  See 85aux2.lst line 2269 (approx) for details. TLDR: used as additional Usage values.
   uint32_t      as_uint32_t[2];
  } AR_BUF6_OPTS;

  uint16_t      AR_BUF0_OPTS[2];                    //  070370 - 070373     248 -   251           A.BOPT00 - A.BOPT01    !!!!!!!!!!!!!!!!  this is the only difference
  uint8_t       AR_BUF1_OPTS[4];                    //  070374 - 070374     252 -   255           A.COPT0-A.COPT3        U_cmd-specific options
  uint8_t       AR_Buffer_0[256];                   //  070400 - 070777     256 -   511
  uint8_t       AR_Buffer_1[256];                   //  071000 - 070377     512 -   767
  uint8_t       AR_Buffer_2[256];                   //  071400 - 071777     768 -  1023
  uint8_t       AR_Buffer_3[256];                   //  072000 - 072377    1024 -  1279
  uint8_t       AR_Buffer_4[256];                   //  072400 - 072777    1280 -  1535
  uint8_t       AR_Buffer_5[256];                   //  073000 - 073377    1536 -  1791
  uint8_t       AR_Buffer_6[1024];                  //  073400 - 075377    1792 -  2815
  uint8_t       AR_Pad_3[256];                      //  075400 - 075777    2816 -  3071
};


union RAM_WINDOW_OVERLAY
{
  uint8_t             as_bytes[3072];
  struct AUXROM_RAM   as_struct;
  struct AUXROM_RAM_A as_struct_a;
};

EXTERN union RAM_WINDOW_OVERLAY AUXROM_RAM_Window;

EXTERN uint8_t              * p_mailbox;                              //  This will be a pointer to the selected primary mailbox for keyword
EXTERN uint16_t             * p_len;                                  //  This will be a pointer to the selected buffer length
EXTERN uint16_t             * p_usage;                                //  This will be a pointer to the selected buffer usage , and return success/error status
EXTERN char                 * p_buffer;                               //  This will be a pointer to the selected primary buffer for keyword.

struct PACKED S_HP85_Number
{
  uint8_t     real_bytes[8];
};

struct PACKED S_HP85_String_Ref
{
  uint16_t     unuseable_abs_rel_addr;                              //  Actually could be used by looking up whether in Calculator or Basic program mode
  uint16_t     length;
  uint16_t     address;                                             //  Do I need to set MSB if I am over-writing a string, What about if the param is a quoted string?
};

struct PACKED S_HP85_String_Val
{
  uint16_t     length;                                              //  Length of string
  uint16_t     address;                                             //  Location of string, could be a var, or result of an expression. Read only
};

struct PACKED S_HP85_String_Variable
{
  uint8_t     flags_1;
  uint8_t     flags_2;
  uint16_t    Total_Length;
  uint16_t    Max_Length;
  int16_t     Actual_Length;        //  Can be -1 for uninitialized string variables
  uint8_t     text[];
};

struct PACKED S_Parameter_Block_N_N_N_N_N_N    //  up to 6 numbers, total of 48 bytes
{
  struct S_HP85_Number  numbers[6];
};

struct PACKED S_Parameter_Block_SREF           //  1 string ref, total of 6 bytes
{
  struct S_HP85_String_Ref  string;
};

struct PACKED S_Parameter_Block_SVAL              //  1 string, total of 4 bytes
{
  struct S_HP85_String_Val  string_val;
};

union PARAMETER_BLOCK_OVERLAY
{
  struct S_Parameter_Block_N_N_N_N_N_N    Parameter_Block_N_N_N_N_N_N;
  struct S_Parameter_Block_SREF           Parameter_Block_SREF;
  struct S_Parameter_Block_SVAL           Parameter_Block_SVAL;
};

EXTERN  union PARAMETER_BLOCK_OVERLAY Parameter_blocks;

EXTERN  bool      new_AUXROM_Alert;
EXTERN  uint8_t   Mailbox_to_be_processed;

EXTERN  uint8_t HP85A_16K_RAM_module[EXP_RAM_SIZE]; //map this into the HP85 address space @ 0xc000..0xfeff

EXTERN  uint8_t Shared_DMA_Buffer_1[MAX_DMA_TRANSFER_LENGTH + 8];    // + 8 for a tiny bit of off by error safety
EXTERN  uint8_t Shared_DMA_Buffer_2[MAX_DMA_TRANSFER_LENGTH + 8];    // + 8 for a tiny bit of off by error safety


EXTERN  bool haltReq; //set true to request the HP85 to halt/DMA request

//////EXTERN  enum bus_cycle_type current_bus_cycle_state;      //  Detected on Rising edge Phi 2
//////EXTERN  enum bus_cycle_type previous_bus_cycle_state;     //  Used to manage the double cycle needed for address low and high bytes

EXTERN  bool  schedule_address_increment;
EXTERN  bool  schedule_address_load;                      //  Address register is loaded on the second /LMA of a pair
EXTERN  bool  schedule_write;                             //  Schedule a write operation
EXTERN  bool  schedule_read;                              //  Schedule a read operation
EXTERN  bool  delayed_lma;                                //  LMA from prior cycle

EXTERN  volatile uint16_t addReg; //hold the current HP85 address

EXTERN  uint16_t pending_address_low_byte;    //  Storage for low address byte of a two byte address sequence. Load on Phi 1 falling edge

EXTERN  bool HP85_Read_Us;   //  Set on rising edge of Phi 2 if a processor read cycle that WE  (us)  will be responding to.
                             //  Ends on falling edge of Phi 1 when data is sent.
                             //  (see tick 41 for falling edge of /RC)
EXTERN  uint8_t readData;    //  Data to be sent to the processor, is driven onto the bus for the duration of /RC asserted. (Ticks 41 through 51)

EXTERN  bool msu_is_tape;
EXTERN  bool msu_is_disk;
EXTERN  uint8_t msu_select_code;
EXTERN  uint8_t msu_device_code;
EXTERN  uint8_t msu_drive_select;

EXTERN  volatile bool interruptReq;     //set when we are requesting an interrupt. cleared by the acknowledge code
EXTERN  volatile bool Interrupt_Acknowledge;
EXTERN  volatile uint8_t interruptVector; // vector value used by intack

EXTERN  jmp_buf   PWO_While_Running;

EXTERN  char  serial_string[SERIAL_STRING_MAX_LENGTH + 2];
EXTERN  char  lc_serial_command[SERIAL_COMMAND_MAX_LENGTH + 2];

EXTERN  volatile  uint8_t   just_once;       // This is used to trigger a temporary diagnostic function in a piece of code, where dumping with Serial.printf() is not an option
EXTERN  volatile bool globalIntAck;          // set when our interrupt was acknowleged
EXTERN  volatile bool globalIntEn;           // global interrupt enable

//
//  map handler functions for each i/o address                        All of these functions that aren't NULL need to be timed.
//  For I/O we do not represent, ioReadNullFunc() just returns false
//  For I/O we represent (currently none), the function will return data in readData, and the function returns true
//
typedef void (*ioWriteFuncPtr_t)(uint8_t);
typedef bool (*ioReadFuncPtr_t)(void);




//
//  Log file support
//

EXTERN  File logfile;
EXTERN  bool logfile_active;
EXTERN  char logfile_temp_text[200];    //  That should be enough, bad news as no checking is done.

//
//  Logic Analyzer
//

EXTERN  enum analyzer_state Logic_Analyzer_State;

EXTERN  uint32_t  Logic_Analyzer_Data_1[LOGIC_ANALYZER_BUFFER_SIZE];      //  See EBTKS_Bus_Interface_ISR.cpp for bit field layout
EXTERN  uint32_t  Logic_Analyzer_Data_2[LOGIC_ANALYZER_BUFFER_SIZE];      //  See EBTKS_Bus_Interface_ISR.cpp for bit field layout
EXTERN  uint32_t  Logic_Analyzer_Data_index;
EXTERN  uint32_t  Logic_Analyzer_Trigger_Mask_1;
EXTERN  uint32_t  Logic_Analyzer_Trigger_Mask_2;
EXTERN  uint32_t  Logic_Analyzer_Trigger_Value_1;
EXTERN  uint32_t  Logic_Analyzer_Trigger_Value_2;
EXTERN  bool      Logic_Analyzer_Triggered;
EXTERN  uint32_t  Logic_Analyzer_Pre_Trigger_Samples;
EXTERN  int32_t   Logic_Analyzer_Event_Count_Init;
EXTERN  int32_t   Logic_Analyzer_Event_Count;
EXTERN  uint32_t  Logic_Analyzer_Samples_Till_Done;
EXTERN  int16_t   Logic_Analyzer_Index_of_Trigger;
EXTERN  uint32_t  Logic_Analyzer_current_bus_cycle_state_LA;
EXTERN  uint32_t  Logic_Analyzer_Valid_Samples;
EXTERN  uint32_t  Logic_Analyzer_Current_Buffer_Length;
EXTERN  uint32_t  Logic_Analyzer_Current_Index_Mask;
EXTERN  uint32_t  Logic_Analyzer_Valid_Samples_1_second_ago;


EXTERN  volatile uint32_t  Logic_Analyzer_main_sample;
EXTERN  volatile uint32_t  Logic_Analyzer_aux_sample;

EXTERN  Tape tape;

EXTERN  SdFat SD;

EXTERN  Print_Splitter PS;

EXTERN  EXTMEM char Directory_Listing_Buffer[DIRECTORY_LISTING_BUFFER_SIZE];    //  Normally this should be inside the Class as private, but I don't
                                                                                //  know if the EXTMEM can be done within a class, and we certainly
                                                                                //  don't want this buffer to be dynamically allocated on heap either.

EXTERN  EXTMEM char dir_line[258];                                              //  Leave room for a trailing 0x00 (that is not included in the passed max length of PS.get_line)


///////////////////////////////////////////////////  Initialized Globals.  /////////////////////////////////////////////////////////////////

//   Initialized Globals can't use EXTERN (as used above) because initialization over-rides extern

#ifdef ALLOCATE

        const char *Config_filename = "/config.txt";    // <- SD library uses 8.3 filenames


        volatile bool DMA_Request = false;
        volatile bool DMA_Acknowledge = false;
        volatile bool DMA_Active = false;
        volatile bool DMA_has_been_Requested = false;

        const char b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

				bool           serial_string_available = false;
				uint16_t       serial_string_length    = 0;


        
#else
        //  Just declare them, no allocation, no initialization (see above)

        extern  const char *Config_filename;

        extern  volatile uint8_t crtControl;
        extern  volatile bool writeCRTflag;

        extern  bool badFlag;
        extern  uint16_t badAddr;

        extern  bool sadFlag;
        extern  uint16_t sadAddr;

        extern  volatile bool DMA_Request;
        extern  volatile bool DMA_Acknowledge;
        extern  volatile bool DMA_Active;
        extern  volatile bool DMA_has_been_Requested;

        extern  const char b64_alphabet[];

				extern  bool           serial_string_available;
				extern  uint16_t       serial_string_length;

#endif

//
//  These are always extern, because they are defined by the system library,
//  we are just overriding their management
//

extern "C" volatile uint32_t systick_millis_count;
extern "C" volatile uint32_t systick_cycle_count;

