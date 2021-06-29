/*

    Object to manage communications between the teensy and the esp32 of the EBTKS board
    Comms is via a uart to uart serial link


*/

//  Note: in Base64.cpp at line 62, do this:  unsigned char A3[3] = {0,0,0};        PMF  2/27/2021

// #include <Arduino.h>
#include <ArduinoJson.h>
#include <Base64.h>


// #include "Inc_Common_Headers.h"


#define MAX_LINE 512
#define MAX_READ_LEN 512
#define MAX_STR_LEN 1024

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
      StaticJsonDocument<3000> jreply;

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
            Base64.encode(_strBuff, (char *)_buff, readLen);
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
        Base64.decode((char *)_buff, _strBuff, strlen(_strBuff));
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

      //
      //  video screen request
      //
      else if (jdoc["get_alpha"])
      {
        dumpCrtAlphaAsJSON(_buff);
        Base64.encode(_strBuff, (char *)_buff, 512);
        JsonObject reply = jreply.createNestedObject("hpText");
        reply["isHP87"] = false;    //only HP85 for the moment
        reply["image"] = _strBuff;
      }
      serializeJson(jreply, Serial2);
      Serial2.printf("\r\n");
    }

  private:
    int _lineNdx = 0;
    char _line[MAX_LINE];
    File _currFile;
    int _currFd = -1;
    uint8_t _buff[MAX_READ_LEN];
    char _strBuff[MAX_STR_LEN];
};
