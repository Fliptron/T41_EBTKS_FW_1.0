
#ifndef TAPE_H

#define TAPE_H

#include "Arduino.h"
#include "SdFat.h"

class Tape
{
    public:
    Tape();
    bool setFile(const char *fname);
    char * getFile(void);
    void close(void);
   
    void flush(void);
    void init(void);
    void poll(void);
    void enable(bool enable);
    bool is_tape_loaded(void);

    private:
    File _tapeFile;
    char _filename[258];
    uint32_t _tick;
    uint8_t _prevCtrl;
    uint32_t _downCount;
    bool _enabled;              //  Is there an emulated tape drive
    bool _tape_inserted;        //  Is there a tape in the tape drive

    bool blockRead(uint32_t blkNum);
    void blockWrite(int blkNum);
};

#endif
