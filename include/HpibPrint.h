#include <Arduino.h>
#include "SdFat.h"

class HpibPrint : public HpibDevice
{
public:
    HpibPrint(int tla) : HpibDevice()
    {
        _tla = tla;
        _fileOpen = false;
        _fileName[0] = '\0';
    }

    // called when there is a HPIB identify request
    void identify()
    {
    }

    // called when we've received a complete device level command
    // might need to massage the print data methinks
    //
    void processCmd(uint8_t *cmdBuff, int length)
    {
        if (_fileOpen && (_listen == true))
        {
            _printFile.write(cmdBuff, length); 
            _flushTimer = 50; //5 seconds
        }
    }

    void onBurstWrite(uint8_t *buff, int length)
    {
        //placeholder. this function not used
    }

    int input()
    {
        return _readLen;
    }

    //
    //  manage the print file flush. After X time has elapsed, flush the print
    //  file if it is currently open
    //
    void tick()
    {
        if (_flushTimer)
        {
            _flushTimer--;
            if (_flushTimer == 0)
            {
                flush();
            }
        }
    }

    bool setFile(const char *fname)
    {
        if (_printFile) //if a file was open already
        {
            _printFile.close(); //close it
            _fileOpen = false;
        }

        _printFile = SD.open(fname, FILE_WRITE);
        _fileOpen = true;
        if (!_printFile)
        {
            _fileOpen = false;
            //  Serial.printf("Print file did not open: %s\n", fname);
        }
        else
        {
            _fileOpen = true;
            strlcpy(_fileName, fname, sizeof(_fileName));
        }

        return _fileOpen;
    }

    char * getFile()
    {
        return (_fileOpen) ? _fileName : NULL;
    }

    void close()
    {
        if (_printFile)
        {
            _printFile.close();
            _fileOpen = false;
            _flushTimer = 0;
        }
    }

    void flush()
    {
        if (_printFile)
        {
            _printFile.flush();
        }
    }

    //
    //  called with a byte from hpib that is ATN
    //
    void atnOut(uint8_t val)
    {
        uint8_t val7 = val & 0x7f; //strip parity

        if ((val7 >= 0x20) && (val7 < 0x3f))
        {
            //LAD listen address
            if ((val & 0x1f) == _tla)
            {
                _listen = true;
            }
        }
        else if (val7 == HPIB_UNL)
        {
            //UNL unlisten
            _listen = false;
        }
        else if ((val7 >= 0x40) && (val7 < 0x5f))
        {
            //TAD talk address
            if ((val & 0x1f) == _tla)
            {
                _talk = true;
            }
        }
        else if (val7 == HPIB_UNT)
        {
            //UNT
            _talk = false;
        }
        else if ((val7 >= 0x60) && (val7 < 0x7f))
        {
            //  secondary address
            //  Serial.printf("HpibPrint.h: SAD %02X\r\n", val7);
            if (_prevHPIBCmd == HPIB_UNT) //if UNT then secondary
            {
                // identify command

                if ((val7 & 0x1f) == _tla) //only respond if we're addressed
                {
                    //Serial.printf("HpibPrint.h: Identify device:%d\r\n", _tla);
                    identify();
                }
            }
        }
        _prevHPIBCmd = val7;
    }

    uint8_t parallelPoll()
    {
        return 0;
    }
    bool setFile(int diskNum, const char *fname, bool wprot)
    {

        return false;
    }

    bool addDisk(int type)
    {
        return false;
    }

private:
    int _tla;
    bool _listen;
    bool _talk;
    uint8_t _prevHPIBCmd;
    int _readLen;
    char _fileName[258];
    File _printFile;
    bool _fileOpen;
    uint32_t _flushTimer;
};