//
//	08/07/2020	Initial Creation. PMF
//

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Inc_Common_Headers.h"

//  #include "SdFat.h"         included by EBTKS_Function_Declarations.h

//
//  Open (or create)the  Log file in SD card root directory
//
//  Note that files opened for FILE_WRITE start with the pointer at the end, so effectively "Open for Append"
//

bool open_logfile(void)
{
  logfile = SD.open("/EBTKSLog.log", FILE_WRITE);
  if(logfile)
  {
    logfile_active = true;
    return true;
  }
  else
  {
    logfile_active = false;
    return false;
  }
}

void append_to_logfile(const char * text)
{
  if(logfile_active)
  {
    logfile.print(text);    //  Leave the new lines to the caller
    //  logfile.flush();    //  This slows things down too much if tracing 1MB5, so add this
                            //  to the polling loop to be run once per second.
  }
}

void flush_logfile(void)
{
  if(logfile_active)
  {
    logfile.flush();
  }
}

void close_logfile(void)
{
  if(logfile_active)
  {
    logfile.close();
    logfile_active = false;
  }
}

void show_logfile(void)
{
  int   size;
  char  next_char;

  if(logfile_active)
  {
    logfile.flush();
    Serial.printf("Log file size:   %d\n", size = logfile.size());
    logfile.seek(0);
    while(logfile.read(&next_char, 1) == 1)
    {
      Serial.printf("%c", next_char);   //  We are assuming the log file has text and line endings
    }
    logfile.seek(size);
  }
  else
  {
    Serial.printf("Sorry, there is no Logfile\n");
  }
}

void clean_logfile(void)
{
  if(logfile_active)
  {
    logfile.close();
    SD.remove("/EBTKSLog.log");
    if((logfile_active = open_logfile()))   //  The extra parentheses tells the compiler "I know what I am doing with that single '=' "
    {
      Serial.printf("Log file deleted and recreated\n");
    }
    else
    {
      Serial.printf("Log file deleted but could not recreate\n");
    }
  }
  else
  {
    Serial.printf("Sorry, there is no Logfile\n");
  }

}
