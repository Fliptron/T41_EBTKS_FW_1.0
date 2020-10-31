#ifndef _HPIBDEVICE
#define _HPIBDEVICE



class HpibDevice
    {
    public:

        //virtual HpibDevice(int tla);

        // called when there is a HPIB identify request
        virtual void indentify();

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
    };
    #endif //_HPIBDEVICE





