
#include <Arduino.h>
#include "SdFat.h"



struct s_Dir_Entry {
  int   file_size;
  char  dir_entry_text[128];
  char  date_text[12];
  char  time_text[6];
  bool  is_directory;
  bool  is_hidden;
  bool  is_read_only;
  bool  struct_is_valid;
};

class DirLine
{

public:

  DirLine()
  {
    //nothing for the moment
    _currNdx = 0;
    _open = false;
    //_lineBuff[0] = '\0';
  }

  bool beginDir(const char *path)
  {
    if (_open)
    { //  previous request abandoned before end of iteration
      _rootPath.close();
      _open = false;
    }

    if (!_rootPath.open(path, O_RDONLY))
    {
      Serial.printf("beginDir failed at _rootPath.open(path, O_RDONLY)  path is [%s]\n", path);
      return false;
    }
    _rootPath.getName(_dirName, 50);

    if (!_rootPath.isDir())
    {
      Serial.printf("beginDir failed at _rootPath.isDir()  path is [%s]\n", path);
      _rootPath.close();    //  Apparently not a directory
      _open = false;
      return false;
    }
    _rootPath.rewind();
    _open = true;
    return true;
  }

  //
  //  A note about line 78 in SdFatConfig.h (2021), which we used to patch.
  //  As distributed, this is 3, but with the 2021 version of SdFat we had
  //  problems like can't find isReadOnly()  .  This was fixed by patching it to 1
  //  Apparently not needed in 2023 version of SdFat, but do need to change "File" to "SdFile"

  bool getNextLine(struct s_Dir_Entry *entry)
  {
    uint16_t    date = 0, time = 0;


    // char        *str;
    // { // Allow YYYY-MM-DD hh:mm
    //   str = &entry->time_text[5];               //  Point at trailing 0x00
    //   *str = 0;
    //   str = fsFmtTime(str, time);               //  Save str, in case we add diag prints later.
    //                                             //  fsFmtTime() and fsFmtDate() expect the pointer to be the trailing 0x00
    //   str = &entry->date_text[10];              //  And write from the end of the string !!!. Both are of known length.
    //   *str = 0;                                 //  Come back and review when date gets to 9999-12-31
    //   str = fsFmtDate(str, date);
    // }
    // else
    // {
    //   strcpy(entry->date_text, "          ");
    //   strcpy(entry->time_text, "     ");

    entry->file_size = 0;
    entry->dir_entry_text[0] = 0x00;
    entry->date_text[0] = 0x00;
    entry->time_text[0] = 0x00;
    entry->is_directory = false;
    entry->is_hidden = false;
    entry->is_read_only = false;
    entry->struct_is_valid = false;

    if (_open == false)
    {
        return false;
    }

    if (_currPath.openNext(&_rootPath, O_RDONLY))
    {
      entry->file_size = _currPath.fileSize();
      _currPath.getName(entry->dir_entry_text, MAX_SD_CARD_FILE_OR_DIR_NAME_LENGTH - 2);    //  allocated storage is 2 bytes longer, to leave room for trailing slash
                                                                                            //  Knock off two more bytes, for some safety margin
      entry->is_directory = _currPath.isDir();
      //entry->is_directory = _currPath.isSubDir();     // #### how is this different
      entry->is_read_only = _currPath.isReadOnly();
      entry->is_hidden = _currPath.isHidden();
      if (_currPath.getModifyDateTime(&date, &time))
      {   // Time is formatted  HH:MM , Date is formatted YYYY-MM-DD
        if (date)
        {
          sprintf(entry->time_text, "%02d:%02d",      time >> 11, (time >> 5) & 63);
          sprintf(entry->date_text, "%04d-%02d-%02d", ((date >> 9) + 1980), (date >> 5 & 15), date & 31);
        }
        else
        {
          strcpy(entry->time_text, "     ");
          strcpy(entry->date_text, "          ");
        }
      }
      _currPath.close();
      entry->struct_is_valid = true;
    }
    else
    {   //  At end of iterating through directory entries
      _rootPath.close();
      _open = false;
      return false;
    }
    return true;
  }

private:
  bool _open;
  FsFile _rootPath;                     //   Changed for 1.57
  FsFile _currPath;                     //   Changed for 1.57
  char _dirName[60];
  char *_lineBuff;
  uint32_t _currNdx;
  uint32_t _maxLineLen;

  void setBuff(char *buff, uint32_t len)
  {
      _lineBuff = buff;
      _maxLineLen = len;
      _lineBuff[0] = '\0';
      _currNdx = 0;
  }
};

