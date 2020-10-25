
#include <Arduino.h>
#include <SdFat.h>

extern SdFat SD;

extern size_t strlcpy(char , const char , size_t);

enum DISK_TYPE
    {
    DISK_TYPE_35 = 0,
    DISK_TYPE_5Q,
    DISK_TYPE_QMINI,
    DISK_TYPE_8,
    DISK_TYPE_HD5,
    DISK_TYPE_HD10
    };

// From Everett Kaser's HP emulator:
/// Return: (heads,cyls,secs/cyl/total secs are all DECIMAL numbers)
    ///
    /// From the 85A MS ROM source listing (thus what the 85A supports):
    ///												HEADS	CYLS	SECS/CYL	TOTAL_SECS
    ///		x01,x04 mini-floppy (82901/9121)		2		33		32			1056
    ///		x00,x81 8" (9134A/9895A)				2		75		60			4320
    ///		x01,x05 quad mini						4		68		32			2176
    ///
    ///	From the 85B MS ROM source listing (thus what the 85B supports):
    ///												HEADS	CYLS	SECS/CYL	TOTAL_SECS
    ///		x01,x04 mini-floppy (82901/9121)		2		33		32			1056
    ///		x00,x81 8" (9134A/9895A)				2		75		60			4320
    ///		x01,x06 5 MB hard disk (9135A OPT 10)	4		152		124			18848
    ///		x01,x0A 10 MB hard disk (9134B)			4		305		124			37820
    ///
    ///	From the 87 MS ROM source listing (thus what the 87 supports):
    ///												HEADS	CYLS	SECS/CYL	TOTAL_SECS
    ///		x01,x04 mini-floppy (82901/9121)		2		33		32			1056
    ///		x00,x81 8" (9134A/9895A)				2		75		60			4320
    ///		x01,x06 5 MB hard disk (9135A OPT 10)	4		153		124			18972
    ///
    /// NOTES:
    ///		1) The 85A supports an ID of 1,5 (quad mini) that the other two do not.
    ///		2) The 85B adds support for a 5MB and 10MB hard disk.
    ///		3) The 87 drops support for the 10MB disk and increases the 5MB disk's cylinder count by 1.
    ///
class HPDisk
    {
    public:

        static constexpr int SECTOR_SIZE = 256;

        HPDisk(DISK_TYPE typeName, SdFat *sd)
            {
            _sd = sd;
            _error = false;
            _loaded = false;
            _tickCount = 0;
            _flushTime = 50; //5 seconds
            _writeProtect = false;

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
                    _type = 0x0181;
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

        SdFat *_sd;
        File _diskFile;
        bool _error;
        bool _loaded;
        bool _writeProtect;
        bool _attention;
        bool _seekErr;
    };