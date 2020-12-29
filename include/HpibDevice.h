#ifndef _HPIBDEVICE
#define _HPIBDEVICE

// device type enumeration. Poor man's RTTI
typedef enum {HPDEV_NONE = 0,HPDEV_DISK,HPDEV_PRT} Hpdev_t;

class HpibDevice
    {
    public:

        HpibDevice(int tla, Hpdev_t type)
        {
            _type = type;
            _tla = tla;
        }

        // called when there is a HPIB identify request
        virtual void identify();

        // called when we've received a complete device level command
        virtual void processCmd(uint8_t * cmdBuff, int length);
            
        //
        //  called with a byte from hpib that is ATN
        //
        virtual void atnOut(uint8_t val);

        virtual void onBurstWrite(uint8_t *buff, int length);

        //
        //  return device bit for parallel polling (only works for devices 0..7)
        //   
        virtual uint8_t parallelPoll();

        virtual void tick();

        //
        // return length of message put into readBuff
        //
        virtual int input();

        //
        //
        //
        virtual bool setFile(const char *fname);
        virtual bool setFile(int diskNum, const char *fname, bool wprot);
        virtual bool addDisk(int type);
        virtual bool close(int diskNum);
        virtual char *getFilename(int diskNum);

        //returns true if the device type is equal to this device
        //currently used to distinguish between disk and printer devices
        bool isType(Hpdev_t type)
        {
            return (type == _type);
        }

    protected:
        Hpdev_t _type;
        int _tla;
    };
    #endif //_HPIBDEVICE





