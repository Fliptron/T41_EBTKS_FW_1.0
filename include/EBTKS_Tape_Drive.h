
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
    void setPath(const char *pname);
    char * getPath(void);
    void close(void);
   
    void flush(void);
    void init(void);
    void poll(void);
    void enable(bool enable);

    private:
    File _tapeFile;
    char _filename[15];
    char _pathname[250];
    uint32_t _tick;
    uint8_t _prevCtrl;
    uint32_t _downCount;
    bool _enabled;

    bool blockRead(uint32_t blkNum);
    void blockWrite(int blkNum);
};

#endif
