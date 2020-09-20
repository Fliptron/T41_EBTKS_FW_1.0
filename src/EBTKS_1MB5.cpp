
// 1MB5 Series 80 translator chip emulation
// this chip is used in a number of expansion modules- namely:
// HPIB interface
// HP 82939A serial interface
// cp/m interface and others
// Russell Bull July 2020
//
//
//
#include <Arduino.h>

#include "Inc_Common_Headers.h"

#include "HpibDisk.h"

// register defines

#define GLOBAL_INT_ENABLE (0xff00U)  //write only - all 1MB5s respond to this
#define GLOBAL_INT_DISABLE (0xff01U) //write only - all 1MB5s respond to this

#define ENABLE_1MB5_INT (0xff40U) // write - enable 1MB5 interrupts, read returns the select code of the 1MB5 that interrupted - read once per interrupt. return the low byte of the 1MB5 address

#define TICK_TIME 100       //100ms tick



// the base addresses for the 1MB5s are:
//
//  0xff50,1
//  0xff52,3
//  0xff54,5
//  0xff56,7
//  0xff58,9
//  0xff5a,b
//  0xff5c,d
//  0xff5e,f
//
// the cp/m card uses a 'special' 1mb5. the addresses are: 0xff46/7

// The CCR is read-only to us
#define CCR_INT (1 << 0)
#define CCR_COM (1 << 1)
#define CCR_CED (1 << 2)
#define CCR_RST (1 << 7)

//the PSR is write only to us
#define PSR_SVC (1 << 0)  //assert interrupt
#define PSR_BUSY (1 << 1) //tell the hp85 we're busy
#define PSR_PED (1 << 2)  //tell the hp85 this is the last byte of a transfer
#define PSR_PACK (1 << 3) //processor acknowledge
#define PSR_FDPX (1 << 5) //tell the hp85 we can do full duplex
#define PSR_TFLG (1 << 6) //tell the hp85 we've got more to send/receive
#define PSR_HALT (1 << 7) //assert the HALT signal to the HP85

extern uint8_t readByte;

volatile uint8_t statusReg, ib, ob, ccr;
volatile bool obf, ibf;
bool prevReset;
bool prevInt;
//
//  ioTranslator vars
//
uint8_t prevCmd;
int ioState;
uint8_t SR[16];
uint8_t CR[32];
volatile bool readBurst;
volatile bool writeBurst;
volatile int readCount;
volatile int readIndex;
volatile int writeCount;
volatile int writeIndex;
uint8_t readBuff[512];
uint8_t writeBuff[512];
uint8_t diskBuff[512];

volatile uint32_t readIBCount;
volatile uint32_t writeOBCount;
volatile uint32_t readPSRCount;
volatile uint32_t writeCCRCount;
volatile uint32_t intCount;
volatile uint8_t intReason;

void HPIBOutput(uint8_t val, uint8_t ccr);
void HPIBAtnOut(uint8_t val, uint8_t ccr);

uint32_t TA; //talk address bitmap. 1 bit per device
uint32_t LA; //listen address bitmap
uint32_t SA; //secondary address
uint8_t cmdBuff[512];
uint32_t cmdIndex;
bool inputInt = false;
#define TLA85 (21) //hp85 hpib device address

void processOB(void);
void processIB(void);
void requestInterrupt(uint8_t reason);
void loadReadBuff(int count);
bool isReadBuffMT();

#define NUM_DEVICES 31
HpibDisk *devices[NUM_DEVICES];

//
// emulated registers - note these run under the interrupt context - keep them short n sweet!
//
bool onReadStatus(void)
    {
    uint8_t result;

    result = statusReg & 0x7e;
    if (obf == true)
        {
        result |= (1U << 7);
        }
    if (ibf == true)
        {
        result |= (1U << 0);
        }
    readData = result;
    readPSRCount++;
    return true;
    }

bool onReadIB(void)
    {
    readData = 0xff;    //default the return value if mailbox is not loaded
    if (readCount > 0)
        {
        if (ibf == true)
            {
            readData = readBuff[readIndex];
            readIndex++;
            readCount--;
            ibf = false;
            }

        if (readCount == 0)
            {
            statusReg |= PSR_PED;
            }
        else
            {
            statusReg &= ~PSR_PED;
            ibf = true;
            }
        }
    else
        {
        if (readBurst == true)
            {
            statusReg |= PSR_BUSY;
            requestInterrupt(1);
            readBurst = false;
            }
        }
    readIBCount++;
    return true;
    }

void onWriteCCR(uint8_t val)
    {
    writeCCRCount++;
    ccr = val;
    }

void onWriteOb(uint8_t val)
    {
    writeOBCount++;
    ob = val;
    obf = true;

    if (writeBurst == true)
        {
        if (writeCount)
            {
            writeBuff[writeIndex++] = val;
            writeCount--;
            if (writeCount == 0)
                {
                requestInterrupt(1);
                writeBurst = false;
                }
            }
        }
    }

void onWriteInterrupt(uint8_t val)
    {
    (void)val;
    // respond to reset
    //interruptReq = true;
    //ASSERT_INT;
    }
//
//   consider this the interrupt acknowledge function. Note: isr context!
//
bool onReadInterrupt(void)
    {
    intCount++;
    readData = 0x58; //assume device select #7 for the moment

    //switch (intReason)
    //{
    //case 0: // burst out termination
    //    break;
    //case 1: // burst in termination
    //    readBuff[0] = intReason;
    //    readBuff[1] = intReason;
    //    loadReadBuff(2);
    //    break;
    //case 3: // reset
    //case 4: // input interrupt
    //}
    readBuff[0] = intReason;
    readBuff[1] = intReason;
    readIndex = 0;
    readCount = 2;
    statusReg = 0;
    ibf = true;
    return true;
    }
// these are the mainline functions for register access

bool readOb(uint8_t *val)
    {
    bool full;
    uint8_t obval;

    //ensure atomic access
    __disable_irq();
    full = obf;
    obval = ob;
    obf = false;
    __enable_irq();

    if (full == true)
        {
        *val = obval;
        }
    return full;
    }

bool writeIb(uint8_t *val)
    {
    bool full;
    __disable_irq();
    full = ibf;
    if (ibf == false)
        {
        ib = *val;
        ibf = true;
        }
    __enable_irq();
    return full;
    }
//assumes data to send in in the readBuff array
void loadReadBuff(int count)
    {
    __disable_irq();
    readIndex = 0;
    readCount = count;
    if (count == 1)
        {
        statusReg = PSR_PED;
        }
    else
        {
        statusReg = 0;
        }

    ibf = true;
    __enable_irq();
    }

bool isReadBuffMT()
    {
    return readCount == 0 ? true:false;
    }
uint8_t readCcr(void)
    {
    return ccr;
    }

void writeStatus(uint8_t val)
    {
    statusReg = val;
    }

void requestInterrupt(uint8_t reason)
    {
    intReason = reason;
    interruptVector = 0x10;   //  Interrupt vector for the 1MB5
    ASSERT_INT;
    interruptReq = true;
    }

void initTranslator(void)
{

  SR[0] = 1;    //hpib board ID
  SR[1] = 0;    //interrupt cause
  SR[2] = 0xFC; //hpib bits
  SR[3] = 0;    //hpib data
  SR[4] = 0x22; //system controller
  SR[5] = 0xA0; //controller active
  SR[6] = 1;    //SC1

  CR[16] = 2;
  CR[17] = 0x0d;
  CR[18] = 0x0a;
  Serial.printf("HPIB init\n");
  devices[0] = new HpibDisk(0);     //define one disk system as device #0
  //devices[1] = new HpibDisk(1);    //test multiple devices
  //devices[1]->addDisk(DISK_TYPE_5Q);
  //devices[1]->addDisk(DISK_TYPE_5Q);
  //devices[1]->setFile(0, (char *)"/disks/85Games2.dsk", false);
  //devices[1]->setFile(1, (char *)"/disks/85Games1.dsk", false);

//  1MB5 device #7 /hpib/disk interface
  setIOReadFunc(0x58,&onReadStatus);
  setIOReadFunc(0x59,&onReadIB);
  setIOWriteFunc(0x58,&onWriteCCR);
  setIOWriteFunc(0x59,&onWriteOb);
  setIOWriteFunc(0x40,&onWriteInterrupt);
  setIOReadFunc(0x40,&onReadInterrupt);
}

void loopTranslator(void)
    {
    static uint32_t tick = millis();

    processOB();

    if (readCcr() & 0x01)
        {
        if (prevInt == false)
            {
            LOGPRINTF_1MB5("HP85 interrupted us\n");    //  This happens the least significant bit of the CCR register in the 1MB5 is set
            writeStatus(PSR_PACK);                      //  In real hardware this would be an interrupt sent to the 8049 microprocessor
            }
        prevInt = true;
        }
    else
        {
        prevInt = false;
        }

    if (readCcr() & 0x80)
        {

        prevReset = true;
        }
    else
        {
        if (prevReset == true)
            {
            LOGPRINTF_1MB5("IOP RESET\n");
            intReason = 3;
            interruptVector = 0x10; //int vector for the 1MB5
            ASSERT_INT;
            interruptReq = true;
            }
        prevReset = false;
        }

    if (millis() > (TICK_TIME + tick))
        {
        tick = millis();
        for (int a = 0; a < NUM_DEVICES; a++)
            {
            if (devices[a])
                {
                devices[a]->tick();
                }
            }
        }
    }

void processOB(void)
    {
    uint8_t ourOB;
    uint8_t ourCCR;
    uint8_t tmp;
    uint32_t reads;
    uint8_t poll = 0;


    ourCCR = readCcr(); //might be a race condition with reading OB and CCR
    if (readOb(&ourOB))
        {
        if (ourCCR & CCR_COM)
            {
            //LOGPRINTF_1MB5("Count: %04d INT: %d PSR: %02X CCR: %02X CMD: %02X ", readIBCount, intCount, statusReg, ourCCR, ourOB);
            switch (ourOB & 0xf0)
                {
                case 0: //read status reg
                    readBuff[0] = SR[ourOB & 0x0f];
                    loadReadBuff(1);
                    LOGPRINTF_1MB5(" read SR#%02d Val:%02X", ourOB & 0x0f, SR[ourOB & 0x0f]);
                    prevCmd = ourOB + 1; //increment reg addr for next read
                    break;

                case 0x10: //input
                    prevCmd = ourOB;
                    for (int a = 0; a < NUM_DEVICES; a++)
                        {
                        if (devices[a])  
                            {
                            int len = devices[a]->input();
                            if (len > 0)
                                {
                                LOGPRINTF_1MB5("Device: %d sends %d bytes\n", a, len);
                                loadReadBuff(len);
                                while (isReadBuffMT() == false); //wait until the buffer is consumed
                                break;              //first device wins - only one device should respond anyways
                                }
                            }
                        }
                    switch (ourOB & 0x0f)
                        {
                        case 0: //input polled

                            //     readBuff[0] = 0x01;
                            //     readBuff[1] = 0x04;
                            //     readBuff[2] = 0;
                            //     readBuff[3] = 10;
                            //     writeIb(&readBuff[0]); //start proceedings off
                            //     readCount = 1;
                            //     readIndex = 1;
                            inputInt = false;
                            break;

                        case 1: //interrupt input;
                            //inputInt = true; //generate interrupt on end of msg
                            //writeStatus(0);
                            //writeIb(&ourIB);
                            //readBuff[0] = 0x00;
                            //readBuff[1] = 0x04; //default the drive type for the mo
                            //readCount = 256;
                            //readIndex = 0;
                            //requestInterrupt(4); //reason == input interrupt
                            break;
                        }
                    break;

                case 0x20: //burst io. note the delay from reading the OB to getting here and being ready for the 
                        // burst is critical. 
                    switch (ourOB)
                        {
                        case 0x20: //burst write
                            writeCount = 256;
                            writeIndex = 0;
                            writeBurst = true;
                            while (writeBurst == true)
                                {
                                };
                            while (readOb(&tmp) == false); //wait for ending byte. throw it away

                            for (int a = 0; a < NUM_DEVICES; a++)
                                {
                                if ((devices[a]) && (LA & (1u << a)))       //only listeners
                                    {
                                    devices[a]->onBurstWrite(writeBuff, 256);
                                    }
                                }
                            break;

                        case 0x21: //burst read
                            reads = readIBCount;              //diag to see how bytes are actually read in the transfer
                            memcpy(readBuff, diskBuff, 256);  //copy diskBuff to IB readBuff
                            loadReadBuff(256);
                            readBurst = true;
                            readOb(&tmp); //host should send a byte to start the transfer
                            while (readBurst == true)
                                {
                                };
                            while (readOb(&tmp) == false); //wait for ending byte. throw it away
                            reads = readIBCount - reads; //how many reads occured?
                            LOGPRINTF_1MB5("\nBurst read complete. bytes read %d\n", (int)reads);
                            break;
                        }
                    prevCmd = ourOB;
                    break;

                case 0x30: //interrupt control
                    prevCmd = ourOB;
                    break;

                case 0x40: //interface control
                    prevCmd = ourOB;
                    switch (ourOB & 0x0f)
                        {
                        case 0: //abort
                        case 1: //REN = 1
                        case 2: //REN = 0
                        case 3: //ATN = 0
                            break;
                        case 4: //parallel poll
                           
                            for (int a = 0; a < NUM_DEVICES; a++)
                                {
                                if (devices[a])
                                    {
                                    poll |= devices[a]->parallelPoll();  //devices will set their bits
                                    }
                                }
                            readBuff[0] = poll;
                            loadReadBuff(1);
                            break;

                        case 5: //send MTA
                            LOGPRINTF_1MB5("send MTA");
                            TA |= (1 << TLA85);
                            break;

                        case 6: //send MLA
                            LOGPRINTF_1MB5("send MLA");
                            LA |= (1 << TLA85);
                            break;

                        case 7: //send EOL
                            LOGPRINTF_1MB5("Send EOL");
                            break;

                        case 8: //break i/o
                            LOGPRINTF_1MB5("Break IO");
                            break;

                        case 9: //resume i/o
                            LOGPRINTF_1MB5("Resume IO");
                            break;
                        }
                    break;

                case 0x50: //unused
                case 0x60: //unused
                case 0xc0: //unused
                case 0xd0: //unused
                    break;

                case 0x70: //read auxiliary
                    prevCmd = ourOB;
                    readBuff[0] = CR[16];
                    loadReadBuff(1);
                    break;

                case 0x80:
                case 0x90: //write control
                    prevCmd = ourOB;
                    break;

                case 0xa0: //output
                    prevCmd = ourOB;
                    break;

                case 0xb0: //send
                    prevCmd = ourOB;
                    break;

                case 0xe0: //write auxiliary
                    prevCmd = ourOB;
                    break;

                case 0xf0: //extension
                    prevCmd = ourOB;
                    break;
                } //end switch
            //    apparently sprintf handling of %d is different than Serial.printf, and it complains that %d expects int, but readIBCount is uint32_t 
            //    which compiler says is incompatible "aka long unsigned int" .  The casts are to stop the warnings. More below.
            LOGPRINTF_1MB5("Count: %04d INT: %d PSR: %02X CCR: %02X CMD: %02X ", (int)readIBCount, (int)intCount, statusReg, ourCCR, ourOB);
            }
        else
            {
            LOGPRINTF_1MB5("Count:%d PSR: %02X CCR: %02X DAT: %02X ", (int)readIBCount, statusReg, ourCCR, ourOB);
            switch (prevCmd & 0xf0)
                {
                case 0: //read status
                    break;

                case 0x10: //input
                    break;

                case 0x20: //burst io
                    break;

                case 0x30: //interrupt control
                    break;

                case 0x40: //interface control
                    break;

                case 0x50: //unused
                case 0x60: //unused
                case 0xc0: //unused
                case 0xd0: //unused
                    break;

                case 0x70: //read auxiliary
                    break;

                case 0x80:
                case 0x90: //write control
                    LOGPRINTF_1MB5(" write CR#%02d ", prevCmd & 0x1f);
                    CR[prevCmd & 0x1f] = ourOB;
                    prevCmd++;
                    break;

                case 0xa0: //output
                    LOGPRINTF_1MB5(" output");
                    HPIBOutput(ourOB, ourCCR); //in band hpib data
                    break;

                case 0xb0: //send
                    LOGPRINTF_1MB5(" send" /* , ourOB*/ );   //  Russell: please review. no % in format
                    HPIBAtnOut(ourOB, ourCCR); //ATN out of band hpib data
                    break;

                case 0xe0: //write auxiliary
                    LOGPRINTF_1MB5("  aux" /* , ourOB*/ );   //  Russell: please review. no % in format
                    break;

                case 0xf0: //extension
                    LOGPRINTF_1MB5(" ext" /*, ourOB*/);   //  Russell: please review. no % in format
                    break;
                } //end switch
            }
        LOGPRINTF_1MB5("\n");
        }
    }

//
//  called with a byte from hpib that is not ATN
//
void HPIBOutput(uint8_t val, uint8_t ccr)
    {

    //put another byte in the buffer
    cmdBuff[cmdIndex] = val;
    cmdIndex++;
    if (cmdIndex >= sizeof(cmdBuff))
        {
        cmdIndex = sizeof(cmdBuff) - 1; //peg at max
        LOGPRINTF_1MB5("Cmdbuff overflow!!\n");
        }

    if (ccr & CCR_CED) //if message is complete
        {
        for (int a = 0; a < NUM_DEVICES; a++)
            {
            if (devices[a])
                {
                devices[a]->processCmd(cmdBuff, cmdIndex);
                }
            }
        }
    }
//
//  called with a byte from hpib that is ATN
//
void HPIBAtnOut(uint8_t val, uint8_t ccr)
    {
    uint8_t val7 = val & 0x7f; //strip parity

    if ((val7 >= 0x20) && (val7 < 0x3f))
        {
        //LAD listen address
        LA |= (1 << (val & 0x1f));
        LOGPRINTF_1MB5("LAD %02X\n", val & 0x1f);
        }
    else if (val7 == HPIB_UNL)
        {
        //UNL unlisten
        LA = 0; //unlisten all
        LOGPRINTF_1MB5("UNL\n");
        }
    else if ((val7 >= 0x40) && (val7 < 0x5f))
        {
        //TAD talk address
        TA |= (1 << (val & 0x1f));
        LOGPRINTF_1MB5("TAD %02X\n", val & 0x1f);
        }
    else if (val7 == HPIB_UNT)
        {
        //UNT
        TA = 0;
        cmdIndex = 0; //reset our command buffer when untalked
        LOGPRINTF_1MB5("UNT\n");
        }
    for (int a = 0; a < NUM_DEVICES; a++)
        {
        if (devices[a])
            {
            devices[a]->atnOut(val);
            }
        }
    }
