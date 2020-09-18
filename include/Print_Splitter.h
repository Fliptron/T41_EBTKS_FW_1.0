//
//  Philip's Class to handle multiline text to be sent to the HP85.
//
//  Only use case is directory listings, so far
//
//  References:
//    G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_1.0\.pio\libdeps\teensy41\SdFat\src\FatLib\FatFile.h      line 405    bool ls(print_t* pr, uint8_t flags = 0, uint8_t indent = 0);
//    G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_1.0\.pio\libdeps\teensy41\SdFat\src\common\SysCall.h      line  63    typedef Print print_t;
//    C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\cores\teensy4\Print.cpp              line  42    size_t Print::write(const uint8_t *buffer, size_t size)
//    C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\cores\teensy4\Print.h                line  54    class Print
//    G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_1.0\.pio\libdeps\teensy41\SdFat\src\FatLib\FatVolume.h    line 125
//    G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_1.0\.pio\libdeps\teensy41\SdFat\src\FatLib\FatFilePrint.cpp   line 80
//
//  G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_1.0\.pio\libdeps\teensy41\SdFat\extras\html\index.html
//

#include <Arduino.h>

extern void cpp_workaround_wr_byte(uint32_t index, char byte);
extern char cpp_workaround_rd_byte(uint32_t index);


class Print_Splitter:public Print
{
  public:

	//virtual size_t write(const uint8_t *buffer, size_t size);
	virtual int availableForWrite(void)		{ return 0; }

  virtual size_t write(uint8_t b)                                 //  Returns the number of characters written to the buffer. Usually 1, or 0 if full
  {
    if(_Buffer_write_index < (_big_buffer_size - 1))
    {
      _big_buffer_ptr[_Buffer_write_index++] = b;
      _big_buffer_ptr[_Buffer_write_index]   = 0x00;
      return 1;
    }
    return 0;
  }

//  virtual void flush()                                          //  Flush usually means push anything that is in local storage out to external storage 
//  {                                                             //  or in the case of a printer, out to the physical paper. So we don't have an equivalent
//   _Buffer_write_index = 0;                                     //  here since data is pulled from the buffer, not pushed out.
//   _Buffer_read_index  = 0;
//  }

  void init(char * big_buffer, int big_buffer_size)
  {
   _Buffer_write_index = 0;
   _Buffer_read_index  = 0;
   _big_buffer_ptr     = big_buffer;
   _big_buffer_size    = big_buffer_size;
  }

  int32_t get_line(char * dest, uint32_t max_chars)               //  Copy the next line from the buffer to dest, up to a CR or LF, but don't include the CR/LF
  {                                                               //  The return value is the number of characters written to dest. Can include 0 for an empty line.
    uint32_t    current_length;                                   //  Return -1 if we are at the end of the buffer. String in dest is 0x00 terminated, but is not
                                                                  //  included in the count. So dest needs to be 1 char longer than max_chars

    if(_Buffer_read_index == _Buffer_write_index)
    {
      return -1;
    }
    current_length = 0;
    while(1)
    {
      if(_Buffer_read_index == _Buffer_write_index)               //  Nothing more to read
      {
        return current_length;
      }
      if(_big_buffer_ptr[_Buffer_read_index] == 0x0D)      //  Found CR. Skip over it and and following LF, and return the with the current length
      {
        _Buffer_read_index++;
        if(_big_buffer_ptr[_Buffer_read_index] == 0x0A)
        {
          _Buffer_read_index++;
        }
        return current_length;                                    //  Line ending removed
      }
      *dest++ = _big_buffer_ptr[_Buffer_read_index++];
      *dest = 0x00;
      current_length++;
      if(current_length == max_chars)
      {
        return current_length;
      }
    }
  }

  private:  
  uint32_t      _Buffer_write_index;
  uint32_t      _Buffer_read_index;
  char *        _big_buffer_ptr;
  uint32_t      _big_buffer_size;

};


