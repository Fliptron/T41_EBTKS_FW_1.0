//
//	06/27/2020	These Function Declarations were moved from EBTKS.cpp to here.
//

//
//  1MB5/disk functions
//
void initTranslator(int selectNum);
void loopTranslator(void);
uint8_t get_Select_Code(void);

//
//  bank rom functions
//
uint8_t getRselec(void);
uint8_t *getRomSlotPtr(int slotNum);
void initRoms(void);
void setRomMap(uint8_t romId,uint8_t slotNum);

//
//  Tape functions
//
void tape_handle_command_load(void);
bool tape_handle_MOUNT(char *path);
void tape_disk_which(void);             // does both tape and disk

//
//  DMA Functions
//

int32_t DMA_Read_Block(uint32_t DMA_Target_Address, uint8_t buffer[], uint32_t bytecount);
int32_t DMA_Write_Block(uint32_t DMA_Target_Address, uint8_t buffer[], uint32_t bytecount);

uint8_t  DMA_Peek8 (uint32_t address);
uint16_t DMA_Peek16(uint32_t address);

void    DMA_Poke8 (uint32_t address, uint8_t  val);
void    DMA_Poke16(uint32_t address, uint16_t val);


//
//  CRT Functions
//
void initCrtEmu(void);
void dumpCrtAlpha(void);
void Write_on_CRT_Alpha(uint16_t row, uint16_t column, const char *  text);
void a3_to_a4(unsigned char *a4, unsigned char *a3);
int  base64_encode(char *output, char *input, int inputLen);
void dumpCrtAlphaAsJSON(void);
void writePixel(int x, int y, int color);
void writeLine(int x0, int y0, int x1, int y1, int color);

//
//  Bank Switched ROM support
//
bool      readBankRom(uint16_t addr);
uint8_t * getROMEntry(uint8_t romId);

//
//  HP-85 Bus interface and ISR functions
//

inline bool onReadData(uint16_t addr);
inline void onPhi_2_Rise(void);
void release_DMA_request(void);
inline void onPhi_1_Rise(void);
inline void onWriteData(uint16_t addr, uint8_t data) __attribute__((always_inline, unused));
inline void onPhi_1_Fall(void);
FASTRUN void pinChange_isr(void) __attribute__ ((interrupt ("IRQ")));     //  This fixed the keyboard random errors, which was caused by unsaved registers on interrupt
                                                                          //  not being saved by normal function entry
void setupPinChange(void);
void mySystick_isr(void);
void initIOfuncTable(void);
void setIOReadFunc(uint8_t addr,ioReadFuncPtr_t readFuncP);
void setIOWriteFunc(uint8_t addr,ioWriteFuncPtr_t writeFuncP);
void removeIOReadFunc(uint8_t addr);
void removeIOWriteFunc(uint8_t addr);



//
//  SD Card support
//
bool loadRom(const char *fname, int slotNum, const char * description);
void saveConfiguration(const char *filename);
bool loadConfiguration(const char *filename);
void printDirectory(File dir, int numTabs);
void no_SD_card_message(void);

//
//  HP85 16k exp ram
//
void enHP85RamExp(bool en);
bool getHP85RamExp(void);

bool LineAtATime_ls_Init(char * path);
bool LineAtATime_ls_Next(void);

//
//  Log File support
//

bool open_logfile(void);
void append_to_logfile(const char * text);
void flush_logfile(void);
void close_logfile(void);
void show_logfile(void);
void clean_logfile(void);

//
//  AUXROM Support functions
//

void ioWriteAuxROM_Alert(uint8_t val);
bool onReadAuxROM_Alert(void);
void AUXROM_Poll(void);
void AUXROM_Fetch_Memory(uint8_t * dest, uint32_t src_addr, uint16_t num_bytes);
void AUXROM_Store_Memory(uint16_t dest_addr, char * source, uint16_t num_bytes);
void AUXROM_Fetch_Parameters(void * Parameter_Block_XXX , uint16_t num_bytes);
double cvt_HP85_real_to_IEEE_double(uint8_t number[]);
int32_t cvt_R12_int_to_int32(uint8_t number[]);
//void cvt_int32_to_HP85_tagged_integer(uint8_t * dest, int val);
void cvt_IEEE_double_to_HP85_number(uint8_t * dest, double val);
bool Resolve_Path(char *New_Path);
void post_custom_error_message(char * message, uint16_t error_number);
void post_custom_warning_message(char * message, uint16_t error_number);

//
//  AUXROM Functions/Keywords/Statements
//

void initialize_SD_functions(void);

          void AUXROM_CLOCK(void);
          void AUXROM_FLAGS(void);
          void AUXROM_HELP(void);
void AUXROM_SDCAT(void);
void AUXROM_SDCD(void);
void AUXROM_SDCLOSE(void);
void AUXROM_SDCUR(void);
void AUXROM_SDDEL(void);
void AUXROM_SDFLUSH(void);
void AUXROM_SDMEDIA(void);
void AUXROM_SDMKDIR(void);
void AUXROM_MOUNT(void);
void AUXROM_SDOPEN(void);
          void AUXROM_SPF(void);
void AUXROM_SDREAD(void);
          void AUXROM_SDREN(void);
void AUXROM_SDRMDIR(void);
void AUXROM_SDSEEK(void);
void AUXROM_SDWRITE(void);
          void AUXROM_UNMNT(void);
void AUXROM_WROM(void);

//
//  Utility Functions
//

void TXD_Pulser(uint8_t count);

void EBTKS_delay_ns(int32_t count);

bool wait_for_serial_string(void);
void get_serial_string_poll(void);
void serial_string_used(void);

void Serial_Command_Poll(void);

void str_tolower(char *p);
bool MatchesPattern(char *pT, char *pP);
void HexDump_T41_mem (uint32_t start_address, uint32_t count, bool show_addr, bool final_nl);
void HexDump_HP85_mem(uint32_t start_address, uint32_t count, bool show_addr, bool final_nl);
void show_mailboxes_and_usage(void);

void Setup_Logic_Analyzer(void);
void Logic_analyzer_go(void);
void Logic_Analyzer_Poll(void);

void Simple_Graphics_Test(void);

bool is_HP85_idle(void);


//
//  various configuration stuff
//
int getMachineNum(void);
uint32_t getFlags(void);
//
//  Functions that Visual Studio Code can't find, but are in the Arduino library.
//  So this is to just shut up some warning messages
//

extern size_t strlcpy(char *, const char *, size_t);    //  size parameter includes space for the null
extern size_t strlcat(char *, const char *, size_t);

