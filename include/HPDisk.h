
#include <Arduino.h>
#include <SdFat.h>

extern SdFs SD;         //   Changed for 1.57

//extern size_t strlcpy(char , const char , size_t);

//
//  Primary source for this comment section is http://www.hpmuseum.net/display_item.php?hw=287 and related pages and PDFs
//
//  Philip's notes on Disk Drive models
//  Model   KibiBytes   Description
//  82901M  2 x 264     Dual 5.25" floppy drives, Master (has controller)
//  82901S  2 x 264     Dual 5.25" floppy drives, can be added to 82901M, does not have a controller
//  82902M  1 x 264     Single 5.25" floppy drive, Master (has controller)
//  82902S  1 x 264     Single 5.25" floppy drive, can be added to 82901M, does not have a controller
//            Either 8290xS can be used with either 8290xM
//
//  9885M   1 x 457     Opt 025  Single 8" floppy drive and controller, can support 3 9885S add-ons, DD/SS media
//          1 x 487     Opt 031
//
//  9895A   1 x 1080    Opt 010  Single 8" floppy drive and controller, DD/DS media
//  9895D   2 x 1080             Dual   8" floppy drive and controller, DD/DS media
//
//  9121S   1 x 264     Single 3.5" floppy drives, Single Density
//  9121D   2 x 528     Dual   3.5" floppy drives, Single Density


enum DISK_TYPE
    {                       //  Force enumeration values because these numbers are used in CONFIG.TXT to specify drive type
    DISK_TYPE_35    = 0,    //  3.5"  Floppy, HP9121  , 1056 sectors , 264 Kibibytes, uses SS/SD disks
    DISK_TYPE_5Q    = 1,    //  5.25" Floppy, HP82901 , 1056 sectors , 264 Kibibytes
    DISK_TYPE_QMINI = 2,
    DISK_TYPE_8     = 3,    //  8"    Floppy, 9134A/9895A , 4320 sectors , 1080 Kibibytes
    DISK_TYPE_HD5   = 4,
    DISK_TYPE_HD10  = 5
                            //  Not supported yet (may not be supported by Amigo predefined drive list?)
                            //  HP82902   http://www.hpmuseum.net/display_item.php?hw=263   apparently uses DS/DD 5.25" media, yet only provides 264 Kibibytes
                            //  
    };

// From Everett Kaser's HP emulator:
/// Return: (heads,cyls,secs/cyl/total secs are all DECIMAL numbers)
    ///
    /// From the 85A MS ROM source listing (thus what the 85A supports):
    ///                                         HEADS CYLS  SECS/CYL  TOTAL_SECS
    ///   x01,x04 mini-floppy (82901/9121)        2    33      32        1056
    ///   x00,x81 8" (9134A/9895A)                2    75      60        4320
    ///   x01,x05 quad mini                       4    68      32        2176
    ///
    /// From the 85B MS ROM source listing (thus what the 85B supports):
    ///                                         HEADS CYLS  SECS/CYL  TOTAL_SECS
    ///   x01,x04 mini-floppy (82901/9121)        2    33      32        1056
    ///   x00,x81 8" (9134A/9895A)                2    75      60        4320
    ///   x01,x06 5 MB hard disk (9135A OPT 10)   4   152     124       18848
    ///   x01,x0A 10 MB hard disk (9134B)         4   305     124       37820
    ///
    /// From the 87 MS ROM source listing (thus what the 87 supports):
    ///                                         HEADS CYLS  SECS/CYL  TOTAL_SECS
    ///   x01,x04 mini-floppy (82901/9121)        2    33      32        1056 
    ///   x00,x81 8" (9134A/9895A)                2    75      60        4320
    ///   x01,x06 5 MB hard disk (9135A OPT 10)   4   153     124       18972
    ///
    /// NOTES:
    ///   1) The 85A supports an ID of 1,5 (quad mini) that the other two do not.
    ///   2) The 85B adds support for a 5MB and 10MB hard disk.
    ///   3) The 87 drops support for the 10MB disk and increases the 5MB disk's cylinder count by 1.
    ///
class HPDisk
    {
    public:

        static constexpr int SECTOR_SIZE = 256;

        HPDisk(DISK_TYPE typeName, SdFs *sd)            //   Changed for 1.57
            {
            _sd = sd;
            _error = false;
            _loaded = false;
            _tickCount = 0;
            _flushTime = 50; //5 seconds
            _writeProtect = false;
            _filename[0] = '\0';

            switch (typeName)
                {
                case DISK_TYPE_35:
                    _cyls = 33;
                    _heads = 2;
                    _sectors = 32;
                    _totalSectors = 1056;
                    _type = 0x0104;
                    break;

                case DISK_TYPE_5Q:
                    _cyls = 33;
                    _heads = 2;
                    _sectors = 32;
                    _totalSectors = 1056;
                    _type = 0x0104;
                    break;

                case DISK_TYPE_QMINI:
                    _cyls = 68;
                    _heads = 4;
                    _sectors = 32;
                    _totalSectors = 2176;
                    _type = 0x0105;
                    break;

                case DISK_TYPE_8:
                    _cyls = 75;
                    _heads = 2;
                    _sectors = 60;
                    _totalSectors = 4320;
                    _type = 0x0081;
                    break;

                case DISK_TYPE_HD5:
                    _cyls = 152;
                    _heads = 4;
                    _sectors = 124;
                    _totalSectors = 18848;
                    _type = 0x0106;
                    break;

                case DISK_TYPE_HD10:
                    _cyls = 305;
                    _heads = 4;
                    _sectors = 124;
                    _totalSectors = 37820;
                    _type = 0x010a;
                    break;

                default:
                    _error = true;
                    break;
                }
            }

        bool setFile(const char *fname, bool wprot)
            {
            _writeProtect = wprot; //copy write protect flag

            if (_diskFile)      //if a file was open already
                {
                _diskFile.close();  //close it
                _loaded = false;
                }

            _diskFile = _sd->open(fname, FILE_WRITE);
            if (!_diskFile)
                {
                _loaded = false;
                LOGPRINTF_1MB5("Disk file did not open: %s\n", fname);
                }
            else
                {
                _loaded = true;
                strlcpy(_filename, fname, sizeof(_filename));
                }


            return !!_diskFile;     //!! forces a boolean response.
            }

        //  Get a pointer to the filename associated with this virtual drive
        char * getFilename()
            {
              //Serial.printf("In HPDisk, returning %08x\n", _filename);
              return _filename;
            }
 
        //
        //  reads a 'sector' from the disk image file
        //  using the current lba value.
        //
        bool readSector(uint8_t *buff)
            {
            bool err = false; //default to fail

            if (!_diskFile.seek(_lba * SECTOR_SIZE))
                {
                LOGPRINTF_1MB5("Disk seek error %d\n", _lba);
                _seekErr = true;
                err = true;
                }
            else
                {
                int len = _diskFile.read(buff, SECTOR_SIZE);
                if (len < SECTOR_SIZE)
                    {
                    LOGPRINTF_1MB5("End of disk image at block: %06d\n", _lba);
                    err = true;
                    }
                }

            LOGPRINTF_1MB5("Read Block %06d\n", _lba);
            if (err == false)
                {
                incSector();
                }
            return err;
            }

        bool writeSector(uint8_t *buff)
            {
            bool err = false;

            if (_writeProtect == true)
                {
                err = true;
                }
            else
                {
                if (!_diskFile.seek(_lba * SECTOR_SIZE))
                    {
                    LOGPRINTF_1MB5("Disk seek error %d\n", _lba);
                    _seekErr = true;
                    err = true;
                    }
                else
                    {
                    _diskFile.write(buff, SECTOR_SIZE);

                    LOGPRINTF_1MB5("Write Block %06d\n", _lba);
                    incSector();
                    _tickCount = _flushTime; //load flush timer
                    }
                }
            return err;
            }
        //
        //
        //  converts c/h/s into the logical block address of the sector in the image file
        //  sets the interval var used by subsequent reads/writes
        //  getLba() returns the resultant lba value
        //
        bool seekCHS(int cyl, int head, int sector)
            {
            bool err = false;

            _currCyl = cyl;
            _currHead = head;
            _currSector = sector;

            _lba = (cyl * _sectors) +
                ((head * _sectors) / _heads) +
                sector;

            if (_lba >= _totalSectors)
                {
                err = true;
                }

            return err;
            }

        void incSector()
            {
            _currSector++;

            if (_currSector >= _sectors)
                {
                _currSector = 0;
                _currCyl++;
                }
            // calc the logical block address from CHS
            _lba = (_currCyl * _sectors) +
                ((_currHead * _sectors) / _heads) +
                _currSector;

            if (_lba >= _totalSectors)
                {
                _seekErr = true;
                }
            }

        bool flush()
            {
            //_diskFile.flush();
            return setFile(_filename, _writeProtect); //close then re-open
            }

        int getBlock()
            {
            return _lba;
            }

        int getHeads()
            {
            return _heads;
            }
        int getSectors()
            {
            return _sectors;
            }

        int getCyls()
            {
            return _cyls;
            }

        void getDiskAddr(uint8_t *addrs)
            {
            addrs[0] = (_currCyl >> 8);
            addrs[1] = _currCyl & 0xffU;
            addrs[2] = _currHead;
            addrs[3] = _currSector;
            }

        uint16_t getId()
            {
            return _type;
            }

        bool isLoaded()
            {
            return _loaded;
            }

        bool close()
            {
            if (_diskFile)
                {
                _diskFile.close();
                _loaded = false;
                _filename[0] = 0x00;
                }
            return true;
            }

        //call every 100ms or so to take care of disk flush

        void tick()
            {
            if (_tickCount)
                {
                _tickCount--;
                if (_tickCount == 0)
                    {
                    //one shot
                    flush();
                    }
                }
            }

        bool getWriteProtect()
            {
            return _writeProtect;
            }

        void setWriteProtect(bool wp)
            {
            _writeProtect = wp;
            }

        uint16_t getStatus()
            {
            uint16_t status = 0;

            status |= (6U << 9);    //hp double sided disk loaded

            if (_attention == true)
                {
                status |= (1U << 7);    //attention flag
                _attention = false;
                }

            if (_loaded == false)
                {
                status |= 0x8003;  //disk not ready & err flag
                }

            if (_writeProtect == true)
                {
                status |= (1U << 6);
                }

            if (_seekErr)
                {
                status |= (1U << 2);
                _seekErr = false;
                }

            return status;
            }

    private:
        int _cyls;
        int _heads;
        int _sectors;
        int _totalSectors;
        uint16_t _type;
        char _filename[258]; //arbitrary max length
        uint32_t _tickCount;
        uint32_t _flushTime;

        int _currCyl;
        int _currHead;
        int _currSector;
        int _lba;

        SdFs *_sd;                  //   Changed for 1.57
        FsFile _diskFile;           //   Changed for 1.57
        bool _error;
        bool _loaded;
        bool _writeProtect;
        bool _attention;
        bool _seekErr;
    };