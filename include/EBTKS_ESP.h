/*

    Object to manage communications between the teensy and the esp32 of the EBTKS board
    Comms is via a uart to uart serial link

    See here on non IDE Firmware downloading for ESP32
      https://community.platformio.org/t/export-of-binary-firmware-files-for-esp32-download-tool/9253
      https://github.com/espressif/esptool
      D:\2021\HP-85_EBTKS_V1.0\ESP32_Binaries\Where_stuff_comes_from.txt

*/
#include <ArduinoJson.h>
#include "Base64.h"
#include "DirLine.h"

Base64Class ourBase64;
DirLine dl;
struct s_Dir_Entry dirent;

#define MAX_LINE 512
#define MAX_READ_LEN 512
#define MAX_STR_LEN 4096


extern void dumpCrtAlphaAsBase64(char *buff, bool comp_graph);

//
//  simple circular buffer class
//
class CircBuff
{
  public:

  CircBuff()
  {
    _head = 0;
    _tail = 0;
  }
  //return -1 if nothing to read, else 0..255 if the char

  int getCh()
  {
    if (_head == _tail)
      {
        return -1;
      }
    else
      {
        int a = _buff[_tail++];

        if (_tail >= _len)
          {
            _tail = 0;
          }
          return a;
      }
  }

  void putch(uint8_t ch)
  {
      _buff[_head++] = ch;
      if (_head >= _len)
        {
          _head = 0;
        }
  }

  private:
  uint32_t _head;
  uint32_t _tail;
  const uint32_t _len = 100;
  uint8_t _buff[100];
};

//
//  Stream class that computes a crc on the stream
//

class CrcStream : public Stream
{
public:
  int available(void)
  {
    return 1;
  }

  int peek(void)
  {
    return 0;
  }

  void flush(void)
  {
  }

  void clearCrc()
  {
    _crc = 0xffffU; //prime the crc with the residual
  }

  //
  //  compute the crc from the write data
  //
  size_t write(uint8_t c)
  {
    uint8_t res = (uint16_t)c ^ _crc;
    _crc >>= 8;
    _crc ^= _crc_table[res];

    return 0;
  }

  uint16_t getCrc()
  {
    return _crc;
  }
  int read(void)
  {
    return 0;
  }

  using Print::write;
  size_t write(unsigned long n) { return write((uint8_t)n); }
  size_t write(long n) { return write((uint8_t)n); }
  size_t write(unsigned int n) { return write((uint8_t)n); }
  size_t write(int n) { return write((uint8_t)n); }

private:
  uint16_t _crc;

  const uint16_t _crc_table[256] = {
      0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
      0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
      0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
      0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
      0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
      0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
      0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
      0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
      0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
      0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
      0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
      0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
      0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
      0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
      0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
      0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
      0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
      0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
      0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
      0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
      0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
      0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
      0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
      0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
      0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
      0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
      0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
      0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
      0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
      0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
      0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
      0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040};
};

CrcStream crcstream;

class ESPComms
{
public:
  //
  //  call regularly to service the serial port
  //
  void poll()
  {
    if (getLine() == true)
    {
      // Serial.printf("RX:%s\n", _line);         //  floods the serial terminal
      processMsg();
    }
  }

  int getKybdCh()
  {
    return kbuff.getCh();
  }

  //
  //  attempt to construct a string of chars terminated by a CR
  //
  bool getLine()
  {
    static int gl_state = 0;
    bool found = false;

    while (Serial2.available() && (found == false))
    {
      char ch = Serial2.read();

      switch (gl_state)
      {
      case 0: //wait for beginning '{'

        if (ch == '{')
        {
          _lineNdx = 0;
          gl_state = 1;
          _line[_lineNdx++] = ch;
        }
        break;

      case 1: //accumulate a line until CR
        if (ch == 13)
        {
          _line[_lineNdx] = 0;
          gl_state = 0;
          found = true; //bail
        }
        else if (_lineNdx < MAX_LINE)
        {
          _line[_lineNdx++] = ch;
        }
        else
        {
          //we've overlowed
          _lineNdx = 0;
          gl_state = 0;
        }
        break;
      }
    }
    return found;
  }

  //
  //
  //
  void processMsg()
  {
    //message is in _line
    int len = strlen(_line);
    if (len < 5)
    {
      return;
    }
    StaticJsonDocument<3000> jdoc;
    StaticJsonDocument<6000> jreply;

    DeserializationError error = deserializeJson(jdoc, _line);
    if (error)
    {
      Serial.printf("Bad message from the ESP32. length:%d\r\n", len);
      return;
    }
    else
    {
      //  Serial.printf("message was properly formed\n");  //  Floods the serial terminal
    }

    // we were able to deserialise the JSON message

    if (jdoc["open"])
    {
      const char *filename = jdoc["open"]["path"] | "";
      int opts = jdoc["open"]["opts"] | O_RDONLY;

      _currFile = SD.open(filename, opts);

      //if opened, store the filehandle
      if (_currFile)
      {
        _currFd = 1;
      }
      JsonObject reply = jreply.createNestedObject("open");
      reply["fd"] = _currFd;
    }

    else if (jdoc["close"])
    {
      //process file close message
      int fd = jdoc["close"]["fd"] | -1;

      if ((_currFile) && (fd == _currFd))
      {
        _currFile.close();
        _currFd = -1;
        //_currFile = NULL; //invalidate the file handle
      }
      JsonObject reply = jreply.createNestedObject("close");
      reply["fd"] = _currFd;
    }

    else if (jdoc["read"])
    {
      //process file read message
      int fd = jdoc["read"]["fd"] | -1;
      int len = jdoc["read"]["len"] | -1;

      //limit our max # of read bytes
      len = (len > MAX_READ_LEN) ? MAX_READ_LEN : len;
      int readLen = -1;
      if (_currFile && (fd == _currFd) && (len > 0))
      {
        readLen = _currFile.read(_buff, len);
        if (readLen > 0)
        {
          ourBase64.encode(_strBuff, (char *)_buff, readLen);
        }
      }
      // send response
      JsonObject reply = jreply.createNestedObject("read");
      reply["fd"] = fd;
      reply["len"] = readLen;
      reply["data"] = _strBuff;
    }

    else if (jdoc["write"])
    {
      //process file write message
      int fd = jdoc["write"]["fd"] | -1;
      int len = jdoc["write"]["len"] | -1;
      strncpy(_strBuff, jdoc["Write"]["data"], MAX_STR_LEN);
      ourBase64.decode((char *)_buff, _strBuff, strlen(_strBuff));
      //limit our max # of write bytes
      len = (len > MAX_READ_LEN) ? MAX_READ_LEN : len;
      int writeLen = -1;
      if (_currFile && (fd == _currFd) && (len > 0))
      {
        writeLen = _currFile.write(_buff, len);
      }
      JsonObject reply = jreply.createNestedObject("write");
      reply["fd"] = fd;
      reply["len"] = writeLen;
    }

    else if (jdoc["stat"])
    {
      //process file stat message
      JsonObject reply = jreply.createNestedObject("stat");
      reply["fd"] = _currFd;
      reply["offset"] = 0;
    }

    else if (jdoc["lseek"])
    {
      //  int fd = jdoc["lseek"]["fd"] | -1;
      int offset = jdoc["lseek"]["offset"] | 0;
      //  int whence = jdoc["lseek"]["offset"] | 0;
      //whence:
      //          SEEK_SET        file_offset = offset
      //          SEEK_CUR        file_offset = current offset + offset
      //          SEEK_END        file_offset = size of file + offset
      //process file seek message
      int foffset = _currFile.seek(offset);

      // returns file_offset or error
      JsonObject reply = jreply.createNestedObject("lseek");
      reply["fd"] = _currFd;
      reply["offset"] = foffset;
    }

    else if (jdoc["set_time"])
    {
      String time = jdoc["set_time"]["time"] | "";
      JsonObject reply = jreply.createNestedObject("set_time");
      reply["ok"] = true;
    }

    else if (jdoc["keyboard"])
    {
      // expect an array of 1 or more keycodes (8bit) and insert them
      // into the keyboard queue where they get injected into the HP85
      //strncpy(_strBuff, jdoc["keyboard"]["data"], MAX_STR_LEN);
      //int len = ourBase64.decode((char *)_buff, _strBuff, strlen(_strBuff));
      //inject the keycodes in _buff to the keyboard buffer

      int ch = jdoc["keyboard"]["data"];
      kbuff.putch(ch);
      Serial.printf("Keycode from webpage: %0d\r\n",ch);
      /*for (int a = 0; a < len; a++)
        {
          kbuff.putch(_buff[a]);
        }*/

      JsonObject reply = jreply.createNestedObject("keyboard");
      reply["ok"] = true;
    }

    //
    //  video screen request
    //  need to determine the machine 85/86/87
    //  and the video mode to encode the corrent length of framebuffer
    //  and to tag the data with the correct screen dimensions
    //  for the webpage to render
    //
    else if (jdoc["get_alpha"])
    {
      dumpCrtAlphaAsBase64(_strBuff, false);
      JsonObject reply = jreply.createNestedObject("hpText");
      reply["isHP87"] = false; //only HP85 for the moment
      reply["image"] = _strBuff;
      reply["width"] = 32;
      reply["lines"] = 16;
      reply["machine"] = "HP85";
    }
    else if (jdoc["get_graph"])
    {
      dumpCrtAlphaAsBase64(_strBuff, true);
      Serial.printf("vbuff size:%d\r\n", strlen(_strBuff));
      JsonObject reply = jreply.createNestedObject("hpGraph");
      reply["isHP87"] = false; //only HP85 for the moment
      reply["image"] = _strBuff;
      reply["width"] = 32;
      reply["lines"] = 16;
      reply["machine"] = "HP85";
    }
    //
    //  get sdcard directory
    //
    else if (jdoc["files"])
    {
      strncpy(_strBuff, jdoc["files"]["path"], MAX_STR_LEN);
      dl.beginDir(_strBuff);

      JsonObject reply = jreply.createNestedObject("theFiles");
      reply["path"] = _strBuff;
      JsonArray dirents = jreply.createNestedArray("dir");
      while (1)
      {
        if (dl.getNextLine(&dirent))
        {
          JsonObject dir = dirents.createNestedObject();
          dir["type"] = (dirent.is_directory) ? "dir" : "file";
          dir["name"] = dirent.dir_entry_text;
          dir["size"] = dirent.file_size;
          dir["hidden"] = dirent.is_hidden;
          dir["readonly"] = dirent.is_read_only;
        }
        else
        {
          break;
        }
      }
    }
    serializeJson(jreply, Serial2);
    //serializeJson(jreply,Serial);

    crcstream.clearCrc();
    serializeJson(jreply, crcstream);
    //Serial.printf("Crc value: %04X\r\n",crcstream.getCrc());
    Serial2.printf(",%d\r\n", crcstream.getCrc());
  }

private:
  int _lineNdx = 0;
  char _line[MAX_LINE];
  FsFile _currFile;
  int _currFd = -1;
  uint8_t _buff[MAX_READ_LEN];
  char _strBuff[MAX_STR_LEN];
  CircBuff kbuff;
};


