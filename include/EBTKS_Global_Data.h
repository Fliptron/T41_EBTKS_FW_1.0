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

///////////////////////////////////////////////////  Uninitialized Globals. Actually initialized to 0x00000000  /////////////////////////////////////////

//  These depend on the automatic initialization to Zero (NULL for pointers , false for bool)
//  https:stackoverflow.com/questions/16015656/are-global-variables-always-initialized-to-zero-in-c?lq=1

EXTERN  uint8_t *romMap[256];                       //  Hold the pointers to the rom data based on ID as the index

struct __attribute__((__packed__)) AUXROM_RAM
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
  char       AR_BUF0_OPTS[4];                       //  070370 - 070373     248 -   251           A.BOPT00 - A.BOPT03
  char       AR_BUF1_OPTS[4];                       //  070374 - 070374     252 -   255           A.BOPT10 - A.BOPT13
  char       AR_Buffer_0[256];                      //  070400 - 070777     256 -   511
  char       AR_Buffer_1[256];                      //  071000 - 070377     512 -   767
  char       AR_Buffer_2[256];                      //  071400 - 071777     768 -  1023
  char       AR_Buffer_3[256];                      //  072000 - 072377    1024 -  1279
  char       AR_Buffer_4[256];                      //  072400 - 072777    1280 -  1535
  char       AR_Buffer_5[256];                      //  073000 - 073377    1536 -  1791
  char       AR_Buffer_6[1024];                     //  073400 - 075377    1792 -  2815
  char       AR_Pad_3[256];                         //  075400 - 075777    2816 -  3071
};

//
//    This is similar to the above struct, except for "uint8_t AR_BUF0_OPTS[4];" being replaced with "uint16_t AR_BUF0_OPTS[2];"
//    and many things that were char in the above struct are uint8_t in the AUXROM_RAM_A struct
//    This is to avoid a compiler warning "warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]"
//    when compiling the WROM function in EBTKS_AUXROM_SD_Services.cpp
//
struct __attribute__((__packed__)) AUXROM_RAM_A
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
  uint8_t       AR_BUF6_OPTS[8];                    //  070360 - 070367     240 -   247           A.BOPT60 - A.BOPT67   See 85aux2.lst line 2226 for details. TLDR: used as additional Usage values.
                                                    //                                                                  Same mailbox constraints on ownership
  uint16_t      AR_BUF0_OPTS[2];                    //  070370 - 070373     248 -   251           A.BOPT00 - A.BOPT01    !!!!!!!!!!!!!!!!  this is the only difference
  uint8_t       AR_BUF1_OPTS[4];                    //  070374 - 070374     252 -   255           A.BOPT10 - A.BOPT13
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

struct __attribute__((__packed__)) S_HP85_Number
{
  uint8_t     real_bytes[8];
};

struct __attribute__((__packed__)) S_HP85_String_Ref
{
  uint16_t     unuseable_abs_rel_addr;                              //  Actually could be used by looking up whether in Calculator or Basic program mode
  uint16_t     length;
  uint16_t     address;                                             //  Do I need to set MSB if I am over-writing a string, What about if the param is a quoted string?
};

struct __attribute__((__packed__)) S_HP85_String_Val
{
  uint16_t     length;                                              //  Length of string
  uint16_t     address;                                             //  Location of string, could be a var, or result of an expression. Read only
};

struct __attribute__((__packed__)) S_HP85_String_Variable
{
  uint8_t     flags_1;
  uint8_t     flags_2;
  uint16_t    Total_Length;
  uint16_t    Max_Length;
  int16_t     Actual_Length;        //  Can be -1 for uninitialized string variables
  uint8_t     text[];
};

struct __attribute__((__packed__)) S_Parameter_Block_N_N_N_N_N_N    //  up to 6 numbers, total of 48 bytes
{
  struct S_HP85_Number  numbers[6];
};

struct __attribute__((__packed__)) S_Parameter_Block_SREF           //  1 string ref, total of 6 bytes
{
  struct S_HP85_String_Ref  string;
};

struct __attribute__((__packed__)) S_Parameter_Block_SVAL              //  1 string, total of 4 bytes
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

// Configuration that we'll store on disk
struct Config
{
  bool ram16k;
  bool tapeEmu;
  bool screenEmu;
  char tapeFile[MAX_ROM_NAME_LENGTH + 1];
  char diskFile[MAX_ROM_NAME_LENGTH + 1];
  char roms[MAX_ROMS][MAX_ROM_NAME_LENGTH + 1];
};

EXTERN  Config config;                        //  <- global configuration object

EXTERN  uint8_t Mirror_Video_RAM[8192];       //

EXTERN  uint8_t vram[8192];                   // Virtual Graphics memory, to avoid needing Read-Modify-Write

EXTERN  uint8_t HP85A_16K_RAM_module[EXP_RAM_SIZE]; //map this into the HP85 address space @ 0xc000..0xfeff

EXTERN  uint8_t Shared_DMA_Buffer_1[MAX_DMA_TRANSFER_LENGTH + 8];    // + 8 for a tiny bit of off by error safety
EXTERN  uint8_t Shared_DMA_Buffer_2[MAX_DMA_TRANSFER_LENGTH + 8];    // + 8 for a tiny bit of off by error safety

EXTERN  uint8_t *currRom; //pointer to the currently selected rom data. NULL if not selected

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

EXTERN  volatile bool interruptReq;     //set when we are requesting an interrupt. cleared by the acknowledge code
EXTERN  volatile bool Interrupt_Acknowledge;
EXTERN  volatile uint8_t interruptVector; // vector value used by intack

EXTERN  jmp_buf   PWO_While_Running;

EXTERN  char  serial_string[SERIAL_STRING_MAX_LENGTH + 2];
EXTERN  char  lc_serial_command[SERIAL_COMMAND_MAX_LENGTH + 2];

EXTERN  volatile  uint8_t   just_once;      //  This is used to trigger a temporary diagnostic function in a piece of code, where dumping with Serial.printf() is not an option

//
//  map handler functions for each i/o address                        All of these functions that aren't NULL need to be timed.
//  For I/O we do not represent, ioReadNullFunc() just returns false
//  For I/O we represent (currently none), the function will return data in readData, and the function returns true
//
typedef void (*ioWriteFuncPtr_t)(uint8_t);
typedef bool (*ioReadFuncPtr_t)(void);

EXTERN  ioReadFuncPtr_t ioReadFuncs[256];      //ensure the setup() code initialises this!
EXTERN  ioWriteFuncPtr_t ioWriteFuncs[256];

//
//  Log file support
//

EXTERN  bool logfile_active;
EXTERN  char logfile_temp_text[200];    //  That should be enough, bad news as no checking is done.

//
//  Logic Analyzer
//

EXTERN  enum analyzer_state Logic_Analyzer_State;

EXTERN  uint32_t  Logic_Analyzer_Channel_A[LOGIC_ANALYZER_BUFFER_SIZE];
EXTERN  uint32_t  Logic_Analyzer_Channel_A_index;
EXTERN  uint32_t  Logic_Analyzer_Trigger_Mask;
EXTERN  uint32_t  Logic_Analyzer_Trigger_Value;
EXTERN  bool      Logic_Analyzer_Triggered;
EXTERN  uint32_t  Logic_Analyzer_Pre_Trigger_Samples;
EXTERN  int32_t   Logic_Analyzer_Event_Count_Init;
EXTERN  int32_t   Logic_Analyzer_Event_Count;
EXTERN  uint32_t  Logic_Analyzer_Samples_Till_Done;
EXTERN  uint32_t  Logic_Analyzer_Index_of_Trigger;
EXTERN  uint32_t  Logic_Analyzer_current_bus_cycle_state_LA;
EXTERN  uint32_t  Logic_Analyzer_Valid_Samples;
EXTERN  uint32_t  Logic_Analyzer_Current_Buffer_Length;
EXTERN  uint32_t  Logic_Analyzer_Current_Index_Mask;


EXTERN  volatile uint32_t  Logic_Analyzer_sample;

EXTERN  Tape tape;

EXTERN  SdFat SD;

EXTERN  Print_Splitter PS;

EXTERN EXTMEM char Directory_Listing_Buffer[DIRECTORY_LISTING_BUFFER_SIZE];     //  Normally this should be inside the Class as private, but I don't
                                                                                //  know if the EXTMEM can be done within a class, and we certainly
                                                                                //  don't want this buffer to be dynamically allocated on heap either.

///////////////////////////////////////////////////  Initialized Globals.  /////////////////////////////////////////////////////////////////

//   Initialized Globals can't use EXTERN (as used above) because initialization over-rides extern

#ifdef ALLOCATE

        const char *Config_filename = "/config.txt";    // <- SD library uses 8.3 filenames

        volatile uint8_t rselec = 0;          //holds rom ID currently selected

        volatile uint8_t crtControl = 0;      //write to status register stored here. bit 7 == 1 is graphics mode, else char mode
        volatile bool writeCRTflag = false;

        bool badFlag = false;                 //odd/even flag for Baddr
        uint16_t badAddr = 0;

        bool sadFlag = false;                 //odd/even flag for CRT start address
        uint16_t sadAddr = 0;

        volatile bool DMA_Request = false;
        volatile bool DMA_Acknowledge = false;
        volatile bool DMA_Active = false;

        const char b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

				bool           serial_string_available = false;
				uint16_t       serial_string_length    = 0;


        DMAMEM uint8_t roms[MAX_ROMS][ROM_PAGE_SIZE];    //  Array to store the rom images loaded from SD card

#else
        //  Just declare them, no allocation, no initialization (see above)

        extern  const char *Config_filename;

        extern  volatile uint8_t rselec;

        extern  volatile uint8_t crtControl;
        extern  volatile bool writeCRTflag;

        extern  bool badFlag;
        extern  uint16_t badAddr;

        extern  bool sadFlag;
        extern  uint16_t sadAddr;

        extern  volatile bool DMA_Request;
        extern  volatile bool DMA_Acknowledge;
        extern  volatile bool DMA_Active;

        extern  const char b64_alphabet[];

				extern  bool           serial_string_available;
				extern  uint16_t       serial_string_length;

        extern  uint8_t roms[MAX_ROMS][ROM_PAGE_SIZE];    //  Array to store the rom images loaded from SD card

#endif

//
//  These are always extern, because they are defined by the system library,
//  we are just overriding their management
//

extern "C" volatile uint32_t systick_millis_count;
extern "C" volatile uint32_t systick_cycle_count;

