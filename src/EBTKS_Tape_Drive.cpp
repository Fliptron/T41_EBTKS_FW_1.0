//
//	06/27/2020	All this wonderful code came from Russell.
//
//	06/27/2020	These function were moved from EBTKS.cpp to here.
//			Also first verion of SD card code and Config file handling from Russell
//
//	07/07/2020	Total re-write of tape handling:
//				pivot - move blocks of tape data to/from ram.
//				the low level access reads/writes data to ram
//				a higher level function via loop() takes care
//				of moving data into/out of ram to the disk file
//

#include <Arduino.h>

#include "Inc_Common_Headers.h"

//
//    Tape Drive Emulation
//

#define TAPE_BLOCKSIZE              (1024)                    //  The amount of tape data we keep in ram - must be a nice binary number
#define TAPE_BLOCKSIZE_SHIFT        (10)                      //  The number of right shifts to calc the block number
#define TAPE_BLOCKSIZE_MASK         (TAPE_BLOCKSIZE - 1)

//
//  This is the data that is not Global, but scope is all functions in this file
//

//  volatile int ioTapeWriteData = -1;	apparently not used

static volatile int holeCount = 0;
static volatile int byteCount = 0;

static volatile uint8_t ioTapCtl;
static volatile uint8_t ioTapSts;
static volatile uint8_t ioTapDat;

static volatile uint16_t tapeBlock[TAPE_BLOCKSIZE];
static volatile bool blockDirty = false;
static volatile uint32_t newBlockNum = 0;
static uint32_t currBlockNum = 10000; //use illegal value to force load
static volatile uint32_t tapeInCount = 5;
static uint32_t tapeRequest = 0;
static volatile uint32_t wState = 0;

// everett's tape emulation

#define TAPEK                   (196)
#define TRACK1_OFFSET           (TAPEK * 1024)

#define TICK_TIME 100U

static int32_t TAPPOS; // current position of tap read/write head on the tape (ie, in TAPBUF)

#define TAP_GAP 0x8000
#define TAP_SYNC 0x4000
#define TAP_DATA 0x2000
#define TAP_HOLE 0x1000

#define STS_INSERTED (1U << 0)
#define STS_STALL (1U << 1)
#define STS_ILIM (1U << 2)
#define STS_WRITE_EN (1U << 3)
#define STS_HOLE (1U << 4)
#define STS_GAP (1U << 5)
#define STS_TACH (1U << 6)
#define STS_READY (1U << 7)

// IO_TAPCTL register bits

#define CTL_TRACK (1 << 0)
#define CTL_PWRUP (1 << 1)
#define CTL_MOTOR_ON (1 << 2)
#define CTL_DIR_FWD (1 << 3)
#define CTL_FAST (1 << 4)
#define CTL_WR_DATA (1 << 5)
#define CTL_WR_SYNC (1 << 6)
#define CTL_WR_GAP (1 << 7)

enum
{
    WSTATE_NO_WRITE,
    WSTATE_WRITE_SYNC,
    WSTATE_WRITE_ONLY,
    WSTATE_WRITE_DATA
};

//
//  Bus read/write functions - called via interrupt context - make them short and sweet
//
//  This routine needs to be timed during read and write operations
//

FASTRUN bool readTapeStatus(void)
{
    uint8_t status = 0;
    uint16_t tapeStatus;
    static bool stickyGap = true;
    bool advanceTape = true;

    if (tapeInCount)
    {
        tapeInCount--;
        status |= STS_GAP;
        status |= STS_READY;
    }
    else
    {
        status |= STS_INSERTED;
        status |= STS_WRITE_EN; //tape always write enabled @todo - logic to enable/disable??
    }

    if (TAPPOS < 0)
    {
        status |= STS_STALL; //something went wrong!
        TAPPOS = 528 + 2048; //re-position the tape to the right of the first hole
    }

    int dir = (ioTapCtl & CTL_DIR_FWD) ? 1 : -1; // tape direction
    if ((ioTapCtl & 0x06) == 0x06)               //if enabled and motor is on
    {
        int32_t tapePosTrack = TAPPOS;
        if (ioTapCtl & CTL_TRACK)
        {
            tapePosTrack += TRACK1_OFFSET;
        }
        uint32_t blk = tapePosTrack >> TAPE_BLOCKSIZE_SHIFT;
        uint32_t ndx = tapePosTrack & TAPE_BLOCKSIZE_MASK;

        if ((blk == currBlockNum) && (tapeRequest == 0)) //if the block we want is in memory
        {
            tapeStatus = tapeBlock[ndx];

            if (tapeStatus & TAP_HOLE)
            {
                if (TAPPOS & 1)
                {
                    status |= STS_HOLE;
                }
            }
            if (tapeStatus & TAP_GAP)
            {
                status |= STS_GAP;
                stickyGap = true;
            }
            else
            {
                stickyGap = false;
            }

            if (ioTapCtl & (CTL_WR_DATA | CTL_WR_GAP | CTL_WR_SYNC))
            {
                //write in progress
                if (ioTapCtl & CTL_WR_GAP)
                {
                    tapeBlock[ndx] = TAP_GAP;
                    blockDirty = true;
                    status &= 0xdf; //clear gap
                    status |= STS_READY;
                }
                else
                {
                    switch (wState)
                    {
                    case WSTATE_NO_WRITE:
                        advanceTape = false; //wait until next status read to activate write
                        wState = WSTATE_WRITE_DATA;
                        status |= STS_READY;
                        break;

                    case WSTATE_WRITE_SYNC:
                        tapeBlock[ndx] = TAP_SYNC;
                        blockDirty = true;
                        wState = WSTATE_WRITE_DATA;
                        status |= STS_READY;
                        break;

                    case WSTATE_WRITE_ONLY:
                        if (tapeStatus & TAP_DATA)
                        {
                            advanceTape = false;
                            wState = WSTATE_WRITE_DATA;
                            status |= STS_READY;
                        }
                        break;

                    case WSTATE_WRITE_DATA:
                        tapeBlock[ndx] = (uint16_t)ioTapDat | TAP_DATA;
                        blockDirty = true;
                        status |= STS_READY;
                        break;
                    }
                    status &= 0xdf; //clear gap
                }
            }
            else
            {
                //else we're reading
                if (tapeStatus & TAP_DATA)
                {
                    status &= 0xdf; //clear gap
                    ioTapDat = tapeStatus & 0xff;
                    if ((ioTapCtl & 0x1e) == 0x0e) //motor on, fwd, normal speed
                    {
                        status |= STS_READY; //there's read data avail
                    }
                }
            }
            if (advanceTape == true)
            {
                TAPPOS += dir; // motor's running so keep the tape advancing
                               // assert tach every two reads
                if (!(TAPPOS & 1))
                {
                    status |= STS_TACH;
                }
            }
        }
        else //wait 'till our new block arrives....
        {
            if (stickyGap == true)
            {
                status |= STS_GAP; //maintain the gap if we cross a block boundary
            }
            // request new block
            if (tapeRequest == 0)
            {
                newBlockNum = blk;
                tapeRequest++;
            }
        }
    }
    else
    {
        status |= STS_READY;
    }
    readData = status;
    return true;
}

FASTRUN bool readTapeData(void)
{
    readData = ioTapDat;
    byteCount++;
    return true;
}

FASTRUN void writeTapeData(uint8_t val)
{
    ioTapDat = val;
}

FASTRUN void writeTapeCtrl(uint8_t val)
{
    ioTapCtl = val;
    if ((wState == WSTATE_NO_WRITE) && ((val & 0xe0) == CTL_WR_DATA))
    {
        // if we transition from no write to write data
        wState = WSTATE_WRITE_ONLY;
    }
    else
    {
        wState = (val & CTL_WR_SYNC) ? WSTATE_WRITE_SYNC : WSTATE_NO_WRITE;
    }
}

// non bus functions

Tape::Tape()
{
    _tick = millis();
    _downCount = 0;
    _enabled = false;
}

bool Tape::setFile(const char *fname)
{
  _tapeFile = SD.open(fname, (O_RDWR));
  if (!_tapeFile)
  {
    Serial.printf("Tape file did not open: %s   <<<<<<<<<<<<<<<<<<\n", fname);
  }
  else
  {
    strlcpy(_filename, fname, sizeof(_filename));
    LOGPRINTF_TAPE("Tape file opened: %s\n", fname);
  }
    return !_tapeFile ? false : true;
}

char * Tape::getFile(void)
{
  return _filename;
}

void Tape::close(void)
{
    if (_tapeFile)
        {
        _tapeFile.close();
        }
}

bool Tape::blockRead(uint32_t blkNum)
{
    bool retval = false; //default to fail

    if (!_tapeFile.seek(blkNum * TAPE_BLOCKSIZE * 2))
        {
        Serial.printf("Tape seek error on block %d\n", blkNum);                      //  Maybe this should be pushed to the screen
        }
    else
        {
        int len = _tapeFile.read((uint8_t *)&tapeBlock[0], TAPE_BLOCKSIZE * 2);
        if (len < (TAPE_BLOCKSIZE * 2))
            {
            Serial.printf("End of tape image at block: %06d\n", blkNum);    //  This is an error message? Need to do better at informing user that tape ran off the spool
            }
        blockDirty = false;
        retval = true;
        }

    LOGPRINTF_TAPE("Read Block %06d\n", blkNum);
    return retval;
}

void Tape::blockWrite(int blkNum)
{
    if (!_tapeFile.seek(blkNum * TAPE_BLOCKSIZE * 2))
        {
        Serial.printf("Tape seek error %d\n", blkNum);                      //  Maybe this should be pushed to the screen
        }
    else
    {
        _tapeFile.write((uint8_t *)&tapeBlock[0], TAPE_BLOCKSIZE * 2);
        blockDirty = false;
        LOGPRINTF_TAPE("Write Block %06d\n", blkNum);
    }
}

void Tape::flush(void)
{
    if (blockDirty)
    {
        blockWrite(currBlockNum);
        blockDirty = false;
    }
    LOGPRINTF_TAPE("Flushing tape image to disk\n");
    close();
}

void Tape::enable(bool enable)
{
    if (enable == true)
        {
        setIOWriteFunc(TAPSTS & 0xff,&writeTapeCtrl);
        setIOWriteFunc(TAPDAT & 0xff,&writeTapeData);
        setIOReadFunc(TAPSTS & 0xff,&readTapeStatus);
        setIOReadFunc(TAPDAT & 0xff,&readTapeData);
        }
    else
        {
        close();
        }   


    if ((_enabled == true ) && (enable == false))       //on disable -> enable
        {
        TAPPOS = 528 + 2048; //position to the right of the first hole
        uint32_t blk = TAPPOS / TAPE_BLOCKSIZE;
        blockRead(blk);
        currBlockNum = blk;
        }

    _enabled = enable;
}

void Tape::poll(void)
{
    if (millis() > (TICK_TIME + _tick))
    {
        _tick = millis();
        if (_downCount)
        {
            _downCount--;
            if (_downCount == 0)
            {
                flush();
                setFile(_filename);
            }
        }
    }

    if (tapeRequest)
    {
        if (blockDirty == true)
        {
            blockWrite(currBlockNum);
            _downCount = 50; //5 seconds to flush tape
            blockDirty = false;
        }
        blockRead(newBlockNum);
        __disable_irq();
        currBlockNum = newBlockNum;
        tapeRequest = 0;
        __enable_irq();
    }

    if (ioTapCtl != _prevCtrl)
    {
        LOGPRINTF_TAPE("Ctrl: %02X\n", ioTapCtl);
    }
    _prevCtrl = ioTapCtl;
}

void tape_handle_command_load(void)
{
  Serial.printf("\nLoad new tape file. Enter filename including path: ");
  if (!wait_for_serial_string())       //  Hang here till we get a file name (hopefully)
  {
    return;                           //  Got a Ctrl-C , so abort command
  }
  Serial.printf("\nOpening tape: %s\n", serial_string);
  tape.close();
  blockDirty = false;
  tapeInCount = 1; //flag the tape removal to the HP85
  tape.setFile(serial_string);
  serial_string_used();
}
