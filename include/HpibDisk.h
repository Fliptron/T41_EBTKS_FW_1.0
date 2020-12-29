#include <Arduino.h>
#include "SdFat.h"
#include "HpibDevice.h"
#include "HPDisk.h"


#define HPIB_UNT 0x5f
#define HPIB_UNL 0x3f

enum {
    AMIGO_SEEK = 2,
    AMIGO_REQ_STATUS = 3,
    AMIGO_READ = 5,
    AMIGO_VERIFY = 7,
    AMIGO_WRITE = 8,
    AMIGO_REQ_ADDR = 0x14,
    AMIGO_FORMAT = 0x18
    };

#define MAX_DISKS 4    //only allow a maximum of 4 disk drives (units)
extern uint8_t readBuff[];
extern uint8_t diskBuff[];
extern void loadReadBuff(int count, bool pack);
extern bool isReadBuffMT();

/*
    enscapulates a hpib disk unit
*/
class HpibDisk : public HpibDevice
    {
    public:
        HpibDisk(int tla) : HpibDevice{tla, HPDEV_DISK}  //  Talker/Listener Address
            {
            _numDisks = 0;
            _readLen = 0;
            _status[0] = 0;
            _status[1] = 0;
            _status[2] = 0;
            _status[3] = 0;

            }

        // note all disks must be the same type - otherwise there'll be problems!
        // only the id from the first disk is used - there's no mechanism to cope 
        // with varied drive types- HP made it this way
        // returns true if there is an error
        bool addDisk(int type)
            {
            bool err = true;
            if (_numDisks < MAX_DISKS)
                {
                _disks[_numDisks] = new HPDisk((enum DISK_TYPE)type, &SD);
                _numDisks++;
                err = false;
                }
            return err;
            }
        // called when there is a HPIB identify request
        void identify()
            {
            if (_numDisks != 0)
                {
                uint16_t id = _disks[0]->getId();
                readBuff[0] = (id >> 8);
                readBuff[1] = (id & 0xff);
                _readLen = 2;
                }

            }

        // called when we've received a complete device level command
        void processCmd(uint8_t * cmdBuff, int length)
            {
            uint16_t cyl;
            uint8_t head, sector;
            if (_listen == false)
                {
                return;
                }

            LOGPRINTF_1MB5("\nComplete device command %02X len:%02X\n", cmdBuff[0], length);

            switch (cmdBuff[0] & 0x1f)
                {
                case 0:
                case AMIGO_SEEK: //seek cmd,unit,cyl lo,cyl hi,head,sector
                    cyl = ((uint16_t)cmdBuff[2]) << 8;
                    cyl += (uint16_t)cmdBuff[3];
                    head = cmdBuff[4];
                    sector = cmdBuff[5];

                    if (isUnitValid(cmdBuff[1]) && isUnitLoaded(cmdBuff[1]))
                        {
                        _disks[_currUnit]->seekCHS(cyl, head, sector); //@todo add error handling
                        clearErrorStatus(cmdBuff[1]);
                        }
                    else
                        {
                        setErrorStatus(cmdBuff[1]); //report bad drive
                        }


                    LOGPRINTF_1MB5("Seek unit:%d track %02d head %02d sector %02d\n", cmdBuff[1], cyl, head, sector);
                    break;

                case AMIGO_REQ_STATUS: //request status cmd unit
                    LOGPRINTF_1MB5("Req Status unit %d\n", cmdBuff[1]);
                    if (isUnitValid(cmdBuff[1]))
                        {
                        clearErrorStatus(cmdBuff[1]);
                        }
                    else
                        {
                        setErrorStatus(cmdBuff[1]); //report bad drive
                        }
                    break;

                case AMIGO_READ: //read cmd,unit
                    LOGPRINTF_1MB5("Read device %d unit %d\n", _tla, cmdBuff[1]);
                    if (isUnitValid(cmdBuff[1]) && isUnitLoaded(cmdBuff[1]))
                        {
                        _disks[_currUnit]->readSector(diskBuff); //@todo add error handling
                        clearErrorStatus(cmdBuff[1]);
                        }
                    else
                        {
                        setErrorStatus(cmdBuff[1]); //report bad drive
                        }

                    break;

                case AMIGO_VERIFY: //verify cmd,unit   @todo always return success
                    LOGPRINTF_1MB5("Verify unit %d\n", cmdBuff[1]);
                    if (isUnitValid(cmdBuff[1]) && isUnitLoaded(cmdBuff[1]))
                        {
                        clearErrorStatus(cmdBuff[1]);
                        }
                    else
                        {
                        setErrorStatus(cmdBuff[1]); //report bad drive
                        }
                    break;

                case AMIGO_WRITE: //write cmd,unit
                    LOGPRINTF_1MB5("Write device %d unit %d\n", _tla, cmdBuff[1]);

                    if (isUnitValid(cmdBuff[1]) && isUnitLoaded(cmdBuff[1]))
                        {
                        LOGPRINTF_1MB5("Write unit %d\n", cmdBuff[1]);
                        clearErrorStatus(cmdBuff[1]);
                        // note: the writeBurst response will call the write sector function
                        }
                    else
                        {
                        setErrorStatus(cmdBuff[1]); //report bad drive
                        }

                    break;

                case AMIGO_REQ_ADDR: //request disk address cmd unit
                    LOGPRINTF_1MB5("Disk addr device %d unit %d\n", _tla, cmdBuff[1]);
                    if (isUnitValid(cmdBuff[1]) && isUnitLoaded(cmdBuff[1]))
                        {
                        _disks[cmdBuff[1]]->getDiskAddr(_diskAddr);
                        clearErrorStatus(cmdBuff[1]);
                        }
                    else
                        {
                        setErrorStatus(cmdBuff[1]); //report bad drive
                        }

                    break;

                case AMIGO_FORMAT: //format disk cmd x x x x x
                    LOGPRINTF_1MB5("Format unit %d\n", cmdBuff[1]);
                    if (isUnitValid(cmdBuff[1]) && isUnitLoaded(cmdBuff[1]))
                        {
                        clearErrorStatus(cmdBuff[1]);
                        }
                    else
                        {
                        setErrorStatus(cmdBuff[1]); //report bad drive
                        }

                    break;

                default:
                    LOGPRINTF_1MB5("Command not recognised %02X\n", cmdBuff[0]);
                    setErrorStatus(cmdBuff[1]); //report bad drive
                    break;
                }
            _prevDiskCmd = cmdBuff[0] & 0x1f;
            }

        //called when a burst write has completed. usually when there is data to write the the disk
        void onBurstWrite(uint8_t *buff, int length)
            {
            if (_prevDiskCmd == AMIGO_WRITE)       //disk write command previously issued?
                {
                _disks[_currUnit]->writeSector(buff); //@todo add error handling
                }
            }

        void tick()
            {
            for (int a = 0; a < _numDisks;a++)
                {
                _disks[a]->tick();
                }
            }
        bool setFile(int diskNum, const char *fname, bool wprot)
            {
            bool retval = false;
            if (isUnitValid(diskNum))
                {
                retval = _disks[diskNum]->setFile(fname, wprot);
                }

            return retval;
            }

        bool close(int diskNum)
            {
            bool retval = false;
            if (isUnitValid(diskNum))
                {
                retval = _disks[diskNum]->close();
                }
            return retval;
            }

        char * getFilename(int diskNum)
            {
            if (isUnitValid(diskNum))
                {
                  return _disks[diskNum]->getFilename();
                }
            else
                {
                  return NULL;
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
                    LOGPRINTF_1MB5("LAD %02X\n", val & 0x1f);
                    }
                }
            else if (val7 == HPIB_UNL)
                {
                //UNL unlisten
                _listen = false;
                LOGPRINTF_1MB5("UNL\n");
                }
            else if ((val7 >= 0x40) && (val7 < 0x5f))
                {
                //TAD talk address
                if ((val & 0x1f) == _tla)
                    {
                    _talk = true;
                    LOGPRINTF_1MB5("TAD %02X\n", val & 0x1f);
                    }
                }
            else if (val7 == HPIB_UNT)
                {
                //UNT
                _talk = false;
                LOGPRINTF_1MB5("UNT\n");
                }
            else if ((val7 >= 0x60) && (val7 < 0x7f))
                {
                //secondary address
                LOGPRINTF_1MB5("SAD %02X\n", val7);
                if (_prevHPIBCmd == HPIB_UNT) //if UNT then secondary
                    {
                    // identify command

                    if ((val7 & 0x1f) == _tla)      //only respond if we're addressed
                        {
                        LOGPRINTF_1MB5("Identify device:%d\n", _tla);
                        identify();
                        }
                    }
                else if (val7 == 0x68) //secondary commands
                    {
                    if (_talk == true)
                        {
                        if (_prevDiskCmd == AMIGO_REQ_STATUS) //request status
                            {
                            readStatus(readBuff);
                            _readLen = 4;
                            LOGPRINTF_1MB5("Request status\n");
                            }

                        if (_prevDiskCmd == AMIGO_REQ_ADDR) //request disk address
                            {
                            readBuff[0] = _diskAddr[0]; //cyl hi
                            readBuff[1] = _diskAddr[1]; //cyl low
                            readBuff[2] = _diskAddr[2]; //head
                            readBuff[3] = _diskAddr[3]; //sector
                            _readLen = 4;
                            LOGPRINTF_1MB5("Request disk address\n");
                            }
                        }
                    }
                else if (val7 == 0x70) //if get DSJ
                    {
                    if (_talk == true)
                        {
                        if (_status[2] & 0x80)
                            {
                            readBuff[0] = 1; //flag error
                            }
                        else
                            {
                            readBuff[0] = 0; //no error to report
                            }
                        _readLen = 1;
                        LOGPRINTF_1MB5("get DSJ %d\n",readBuff[0]);
                        } 
                    }
                }
            _prevHPIBCmd = val7;
            }

        uint8_t parallelPoll()
            {
            uint8_t mask = (~_tla) & 7;
            
            LOGPRINTF_1MB5(" Parallel poll %02X\n", (1U << mask));
            return (1U << mask);
            }

        // return length of message put into readBuff

        int input()
            {
            int len = _readLen;
            _readLen = 0;       //one shot
            LOGPRINTF_1MB5("Input request %d\n", len);
            return len;
            }


        void setErrorStatus(uint8_t unit)
            {
            _status[0] = 0x17;  //bad drive
            _status[1] = unit;
            _status[2] = 0x80;  //set error flag
            _status[3] = 0x02;  //no drive
            }

        void clearErrorStatus(uint8_t unit)
            {
            _status[0] = 0;
            _status[1] = 0;
            _status[2] = 0;
            _status[3] = 0;
            // 'or' in the write protect status
            _status[3] |= (_disks[unit]->getWriteProtect() == true) ? (1U << 6) : 0;

            }

        void readStatus(uint8_t *status)
            {
            status[0] = _status[0];
            status[1] = _status[1];
            status[2] = _status[2];
            status[3] = _status[3];
            }

        bool setFile(const char *fname)
        {
            return false;
        }


    private:
        bool _listen;
        bool _talk;
        uint8_t _prevHPIBCmd;
        HPDisk *_disks[MAX_DISKS];
        uint8_t _currUnit;
        uint8_t _prevDiskCmd;
        uint8_t _status[4];
        uint8_t _diskAddr[4];
        uint8_t _diskBuff[512];
        int _numDisks;
        int _readLen;

        bool isUnitValid(uint8_t unit)
            {
            bool retval = false;

            if (unit < _numDisks)
                {
                _currUnit = unit;
                retval = true;
                }
            return retval;
            }

        bool isUnitLoaded(uint8_t unit)
            {
            bool retval = false;

            if (unit < _numDisks)
                {
                if (_disks[unit]->isLoaded())
                  {
                  retval = true;
                  }
                }
            return retval;
            }
    };





