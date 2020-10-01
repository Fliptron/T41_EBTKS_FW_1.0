//
//  08/30/2020  PMF   These Function implement all of the AUXROM functions
//                    for SD card access from the HP85 BASIC Programs
//  09/01/2020        Work on SDCD. Decide that path separator is '/' and paths have a trailing '/'
//                    Initial path is "/"
//  09/04/2020        Testing of Resolve_Path()
//  09/08/2020        Lots of test cases of how the SdFat library handles paths
//                    Decided that I will maintain a copy of the Current Working Directory, with a trailing '/'
//                    Resolve_Path() will handle ./ and ../ since the SdFat library does not handle it
//                    Decided that I will maintain a copy of the path for all current open files
//                    Directory paths have trailing '/' , File paths do no have a trailing '/'
//  09/23/2020        Add SDOPEN
//  09/24/2020        SDCAT support for wild cards, lower case pattern matching, stripping trailing slash for pattern match
//  09/25/2020        Fix minor bug in SDCAT with RO on subdirectory listings.
//  09/26/2020        SDWRITE
//

/////////////////////On error message / error codes.  Go see email log for this text in context
//
//        Error Message numbers, returned in Usage[0] (or which ever was used for the call)
//
//        0             No error
//        1-99          mail??   amybe he means mailbox specific, or mail is mailbox, usage, len, and buffer
//        100-199       subtract 100 and it is a system error message
//
//        200-299       subtract 200 and index the AUXROM error table. 8 is custom warning, 209 is custom error, 211..N are fixed messages
//                      use AUXERRN to get number
//
////////////////////
//
//        AUX ERROR Numbers returned with AUXERRN for Custom error messages (error 209)
//            Note: many of these numbers won't be used, but pre-allocating them here so we don't get confused later
//                  If just a range is shown, that means none have actually been assigned a specific meaning
//
//
//        300..309      AUXROM_CLOCK
//        310..319      AUXROM_FLAGS
//        320..329      AUXROM_HELP
//        330..339      AUXROM_SDCAT
//        340..349      AUXROM_SDCD
//                                          340       Unable to open directory
//                                          341       Target path is not a directory
//                                          342       Couldn't change directory
//                                          343       Couldn't resolve path
//        350..359      AUXROM_SDCLOSE
//        360..369      AUXROM_SDCUR
//        370..379      AUXROM_SDDEL
//                                          370       Parsing problems with path
//                                          371       Parsing problems with path
//        380..389      AUXROM_SDFLUSH
//        390..399      AUXROM_SDFLUSH
//        400..409      AUXROM_SDMKDIR
//        410..419      AUXROM_MOUNT
//        420..429      AUXROM_SDOPEN
//                                          420       File is already open
//                                          421       Parsing problems with path
//                                          422       Open failed Mode 0
//                                          423       Open failed Mode 1
//                                          424       Open failed Mode 2
//                                          425       Open failedIllegal Mode
//        430..439      AUXROM_SPF
//        440..449      AUXROM_SDREAD
//        450..459      AUXROM_SDREN
//        460..469      AUXROM_SDRMDIR
//        470..479      AUXROM_SDSEEK
//                                          470       Seek on file that isn't open
//                                          471       Trying to seek before beginning
//                                          472       Trying to seek past EOF
//        480..489      AUXROM_SDWRITE
//        490..499      AUXROM_UNMNT
//        500..509      AUXROM_WROM

#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "Inc_Common_Headers.h"

#define   VERBOSE_KEYWORDS          (1)

#define MAX_SD_PATH_LENGTH          (256)

EXTMEM static char          Current_Path[MAX_SD_PATH_LENGTH  + 2];    //  I don't think initialization of EXTMEM is supported yet
EXTMEM static char          Resolved_Path[MAX_SD_PATH_LENGTH + 2];
static bool                 Resolved_Path_ends_with_slash;

EXTMEM static char          Transfer_Buffer[65536];                   //  This is for SDREAD and SDWRITE which need a messy function
                                                                      //  to access HP85 memory. Using this intermediate should cover
                                                                      //  any possible scenario. Prove me wrong.

//
//  Support for AUXROM SD Functions
//
#define MAX_AUXROM_SDFILES        (11)                                //  Usage is 1 through 11, so add 1 when allocating (see next line)

static File Auxrom_Files[MAX_AUXROM_SDFILES+1];                       //  These are the file handles used by the following functions: SDOPEN, SDCLOSE, SDREAD, SDWRITE, SDFLUSH, SDSEEK
                                                                      //                                      and probably these too: SDEOL,  SDEOL$
                                                                      //  The plus 1 is so we can work with the BASIC univers that numbers file 1..N, and the reserved file number 11 that
                                                                      //  is used by some AUXROM internal commands like SDSAVE, SDGET, SDEXPORT(EXPORTLIF?), SDIMPORT (IMPORTLIF ?)

//
//  SdFat Library discoveries, because the documentation is not clear
//
//  SdFat SD;   //  global instantiation of SD
//  1) Must initialize with something like: if(!SD.begin(SdioConfig(FIFO_SDIO))) { //  failure to open SD Card.  }
//  2) If(SD.exists("Folder1"))    for folders returns true with/without trailing slash
//  3) Does not support wildcards
//  4) Does not support ./  and  ../  for path navigation
//  5) No apparent way to access the Volume working durectory (vwd) or Current Working Directory (cwd)
//

File        file;
SdFile      SD_file;
FatFile     Fat_File;
FatVolume   Fat_Volume;
File        HP85_file[11];                                          //  10 file handles for the user, and one for Everett
EXTMEM      char HP85_file_name[11][MAX_SD_PATH_LENGTH + 2];        //  Full path to the file name (the last part)

void initialize_SD_functions(void)
{
  int     i;

  strcpy(Current_Path,"/");
  for(i = 0 ; i < (MAX_AUXROM_SDFILES +1) ; i++)
  {
    Auxrom_Files[i].close();
  }
}

void AUXROM_CLOCK(void)
{

}

void AUXROM_FLAGS(void)
{

}

void AUXROM_HELP(void)
{

}

//
//  Always uses Buffer 6 and associated parameters
//
//  If filespec$ has a trailing slash, it is specifying a subdirectory to be catalogged. Without a slash it is a
//  file selection filter specification, that may include wild cards * and ?
//  Pattern matching removes trailing / so that patterns will match both files and subdirectories
//  Pattern match is case insensitive. Both the filename/directoryname and the match pattern are converted to lower case
//
//  SDCAT                     does a full directory listing to the CRT IS device.
//                            EBTKS never sees this because it gets turned into SDCAT A$,D$,S,0,"*"
//                            and SDCAT A$,D$,S,1,"don't care, but could be *"
//
//  SDCAT fileSpec$           does a full directory listing to the CRT IS device of files matching 'fileSpec$'
//                            EBTKS never sees this because it gets turned into SDCAT A$,D$,S,0,fileSpec$
//                            and SDCAT A$,D$,S,1,"don't care - but could be fileSpec$"
//
//  SDCAT A$,D$,S,0           starts a NEW direcotry listing, putting first name in A$, date in D$, size in S
//                            EBTKS never sees this because it gets turned into SDCAT A$,D$,S,0,"*"
//  SDCAT A$,D$,S,1           continues the ACTIVE directory listing, putting next name in A$, date in D$, size in S
//                            EBTKS never sees this because it gets turned into SDCAT A$,D$,S,1,"*"
//
//  SDCAT A$,D$,S,0,fileSpec$ starts a NEW direcotry listing, putting first name in A$, date in D$, size in S
//                        or  starts a NEW direcotry listing, putting first name in A$, date in D$, size in S of subdirectory if fileSpec$ has a trailing /
//  SDCAT A$,D$,S,1,fileSpec$    continues the ACTIVE direcotry listing, putting next name in A$, date in D$, size in S
//
//  So only the last two are seen by EBTKS. The parameters are passed as follows:
//    The 0 or 1 (4th parameter) is passed in AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0]
//    The fileSpec$              is passed in AUXROM_RAM_Window.as_struct.AR_Buffer_6 (null terminated) and saved in sdcat_filespec
//
//  Returning results:
//    A$ is passed from EBTKS to AUXROM in Buf6 starting at byte 0
//    S  is passed from EBTKS to AUXROM in BLen6 (length of string in Buf6)
//    D$ is passed from EBTKS to AUXROM in Buf6 starting at byte 256    Fixed length of 16 bytes
//
//  returns AR_Usages[6] = 0 for ok, 1 for finished (and 0 length buf6), 213 for any error
//
//  POSSIBLE ERRORS:
//    SD ERROR - if disk error occurs OR 'continue' listing before 'start' listing
//    INVALID PARAM - if "SDCAT A$, D$, x [,fileSpec$], " where 'x' is not 0 or 1
//
//  The following return data placement is from Everett's Series 80 Emulator, 09/20/2020
//    A.BUF6     = filename nul-terminated
//    A.BUF6[256]= formatted nul-terminated file DATE$               currently fixed at 16 characters
//    A.BLEN6    = len of filename
//    A.BOPT60-63 = size                                             File Size
//    A.BOPT64-67 = attributes                                       LSB is set for a SubDirectory, next bit is set for ReadOnly
//
//
//  Test cases        AR_BUF6_OPTS[0]   AR_BUF6_OPTS[1]  Buffer 6       Notes
//                    first=0           wildcards=0      match          Wildcards support adde recently, minimal testing so far
//                    next=1            unique=1         pattern
//
//  SDCAT             0                 0                 *             Should handle this. Initialize and return first result
//                    1                 xxx               xxx           Should handle this. This is the second call and successive
//  SDCAT "*"         0                 0                 *             Should handle this. Initialize and return first result
//  SDCAT "D*"        0                 0                 D*            Can't handle this
//  SDCAT A$,S,F,0    0                 0                 *             Should handle this. Initialize and return first result
//  SDCAT A$,S,F,1    1                 0                 *             Should handle this. Return next result
//  SDCAT A$,S,F,0,fileSpec$                                            This is what EBTKS actually receives
//  SDCAT A$,S,F,1,fileSpec$                                            This is what EBTKS actually receives
//
//  Note: In above table, the "xxx" are data left in OPTS/Buffers from the previous call. This only happens
//  for second and successive calls, where AR_BUF6_OPTS 0..3 are used for passing back the file length, and
//  since successive calls set AR_BUF6_OPTS[0] to 1, it will be 1, but on entry, the rest of AR_BUF6_OPTS 1..3
//  will have the remnants of the previous file's length, and Buffer 6 will have the prior file name, since
//  the pattern specification is only provided on the first call
//
//  For pattern matching, alphabetic case is ignored.
//

//static int              SDCAT_line_count = 0;
static bool               SDCAT_First_seen = false;     //  keeping track of whether we have seen a call for a first line of a SD catalog
EXTMEM static char        dir_line[258];                //  Leave room for a trailing 0x00 (that is not included in the passed max length of PS.get_line)
static int32_t            get_line_char_count;
EXTMEM static char        sdcat_filespec[258];
static int                sdcat_filespec_length;
static bool               filespec_is_dir;
EXTMEM static char        sdcat_dir_path[258];

#define TRACE_SDCAT       (0)

void AUXROM_SDCAT(void)
{
  uint32_t    temp_uint;
  char        file_size[12];
  File        temp_file;
  bool        match;
  char        temp_file_path[258];

  //
  //  Show Parameters
  //
  //Serial.printf("\nSDCAT Start next entry\n");

  if(AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0] == 0)  //  This is a First Call
  {
#if TRACE_SDCAT
    Serial.printf("SDCAT 1\n");
#endif
    //
    //  Filespec$ is captured here on the first call.
    //  If it has a trailing slash, then we are catalogging a subdirectory, and the assumed match pattern is "*"
    //     until Everett changes his mind and adds another parameter, which would be totally ok.
    //
    sdcat_filespec_length = strlcpy(sdcat_filespec, AUXROM_RAM_Window.as_struct.AR_Buffer_6, 129);    //  this could be either a pattern match template, or a subdirectory if trailing slash
    //
    //  Since this is a first call, initialize by reading the whole directory into a buffer (curently 64K)
    //  This conveniently uses the ls() member function.  Alternatively, I could investigate using the built in
    //  SdFat openNext() function.
    //
    //  See EBTKS_Function_and_Keyword_List_2020_09_03.xlsx   for list of functions,  or  G:/PlatformIO_Projects/Teensy_V4.1/T41_EBTKS_FW_1.0/.pio/libdeps/teensy41/SdFat/extras/html/class_fat_file.html
    //    For later reference, it seems the approach it to open the directory     THIS MAY BE WRONG
    //      File dir;
    //      File entry;
    //      if(!dir.open(Current_Path, O_RDONLY))  ....
    //    then do a rewind
    //      dir.rewind();
    //    then open files in that directory, and extract info about them
    //      entry.openNext(dir, oflag)      //  really unsure about this
    //
    //  Hmmmm:  go find this function written a while ago:  printDirectory()   in     EBTKS_SD.cpp
    //          I think the issue is that this works, but maybe there is info that can't be retrieved that the ls() does retrieve  ????
    //  Come back to this another day, and see if we can avoid the 64K buffer
    //
    if((filespec_is_dir = (sdcat_filespec[sdcat_filespec_length-1] == '/')))
    {   //    filespec$ is a subdirectory specification
#if TRACE_SDCAT
      Serial.printf("SDCAT 2\n");
#endif
      if(!Resolve_Path(sdcat_filespec))
      {
#if TRACE_SDCAT
        Serial.printf("SDCAT 3\n");
#endif
        //AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 213;    //  Error
        post_custom_error_message((char *)"Can't resolve path", 301);
        //show_mailboxes_and_usage();
        AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
        Serial.printf("SDCAT Error exit 1.  Error while resolving subdirectory name\n");
        return;
      }
      //
      //  Set the match pattern to the default, "*"
      //
#if TRACE_SDCAT
      Serial.printf("SDCAT 4\n");
#endif
      strcpy(sdcat_filespec, "*");
      sdcat_filespec_length = 1;
    }
    else
    {   //    filespec$ is a Pattern to be matched. Do a directory of the current directory
#if TRACE_SDCAT
      Serial.printf("SDCAT 5\n");
#endif
      strcpy(Resolved_Path, Current_Path);
    }
#if TRACE_SDCAT
    Serial.printf("SDCAT 6\n");
#endif
    str_tolower(sdcat_filespec);                            //  Make all pattern matches case insensitive

    strcpy(sdcat_dir_path, Resolved_Path);                  //  Save the path of the directory being catalogged, since Resolved_Path could be modified between successive calls
                                                            //  We need to keep this info so that we can check the Read-Only state when catalogging a subdirectory
    //Serial.printf("Catalogging directory of [%s]\n", sdcat_dir_path);
    if(!LineAtATime_ls_Init(sdcat_dir_path))                //  This is where the whole directory is listed into a buffer
    {                                                       //  Failed to do a listing of the current directory
#if TRACE_SDCAT
      Serial.printf("SDCAT 7\n");
#endif
      AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 213;    //  Error
      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
      Serial.printf("SDCAT Error exit 2.  Failed to do a listing of the directory [%s]\n", Resolved_Path);
      return;
    }
#if TRACE_SDCAT
    Serial.printf("SDCAT 8\n");
#endif
    SDCAT_First_seen = true;
    //Serial.printf("SDCAT Initial call, Pattern is [%s], directory is [%s]\n", sdcat_filespec, Resolved_Path);
  }

  if(!SDCAT_First_seen)
  {
#if TRACE_SDCAT
    Serial.printf("SDCAT 9\n");
#endif
    //
    //  Either we haven't done a first call, or a previous call reported we were at the end of the directory
    //
    AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 213;    //  Error
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
    Serial.printf("SDCAT Error exit 3.  Missing First call to SDCAT\n");
    return;
  }

  while(1)  //  keep looping till we run out of entries or we get a match between a directory entry and the match pattern
  {
#if TRACE_SDCAT
    Serial.printf("SDCAT 10\n");
#endif
    if(!LineAtATime_ls_Next())
    {   //  No more directory lines
#if TRACE_SDCAT
      Serial.printf("SDCAT 11\n");
#endif
      SDCAT_First_seen = false;                             //  Make sure the next call is a starting call
      AUXROM_RAM_Window.as_struct.AR_Lengths[6]   = 0;      //  nothing more to return
      AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 1;      //  Success and END
      //Serial.printf("SDCAT We are done, no more entries\n");
      //show_mailboxes_and_usage();
      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
      return;
    }
    //
    //  Got an initial or another directory entry. Parse it into little bitz
    //
    //  A directory entry looks like this:
    //    2020-09-07 16:24          0 pathtest/
    //    01234567890123456789012345678901234567890     Sub-directories are marked by a trailing slash, otherwise it is a file
    //
    //Serial.printf("dir_line   [%s]\n", dir_line);
    //
    //
#if TRACE_SDCAT
    Serial.printf("SDCAT 12\n");
#endif
    temp_uint = strlcpy(AUXROM_RAM_Window.as_struct.AR_Buffer_6, &dir_line[28], 129);     //  Copy the filename to buffer 6, and get its length
    AUXROM_RAM_Window.as_struct.AR_Lengths[6] = temp_uint;                                //  Put the filename length in the right place
    //  Make another copy for matching, which will be case insensitive
    strcpy(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[256], AUXROM_RAM_Window.as_struct.AR_Buffer_6);
    str_tolower(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[256]);
    //
    //  Not part of the spec, but I think I want to strip the trailing slash for the pattern match
    //
    if(AUXROM_RAM_Window.as_struct.AR_Buffer_6[temp_uint - 1] == '/')
    {   //  Entry is a subdirectory
#if TRACE_SDCAT
      Serial.printf("SDCAT 13\n");
#endif
      AUXROM_RAM_Window.as_struct.AR_Buffer_6[temp_uint - 1 + 256] = 0x00;                //  Remove the '/' for pattern match
      AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1] = 1;                      //  Record that the file is actually a directory
    }
    else
    {   //  Entry is a file name
#if TRACE_SDCAT
      Serial.printf("SDCAT 14\n");
#endif
      AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1] = 0;                      //  Record that the file is not a directory
    }
#if TRACE_SDCAT
    Serial.printf("SDCAT 15\n");
#endif
    match = MatchesPattern(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[256], sdcat_filespec);    //  See if we have a match

    if(!match)
    {
#if TRACE_SDCAT
      Serial.printf("SDCAT 16\n");
#endif
      continue;                                                                           //  See if the next entry is a match
    }
    //
    //  We have a match, and have alread updated these:
    //    Name        AUXROM_RAM_Window.as_struct.AR_Buffer_6 startin at position 0
    //    Length      AUXROM_RAM_Window.as_struct.AR_Lengths[6]
    //    Dir Status  AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1]
    //
    //  Now finish things
    //
#if TRACE_SDCAT
    Serial.printf("SDCAT 17\n");
#endif
    strlcpy(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[256], dir_line, 17);                 //  The DATE & TIME string is copied to Buffer 6, starting at position 256
    strlcpy(file_size, &dir_line[16], 12);                                                //  Get the size of the file. Still needs some processing
    temp_uint = atoi(file_size);                                                          //  Convert the file size to an int
    AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[0] = temp_uint;                //  Return file size in A.BOPT60-63
    //
    //  Need to get read-only status
    //
    strcpy(temp_file_path, sdcat_dir_path);
    strcat(temp_file_path, AUXROM_RAM_Window.as_struct.AR_Buffer_6);
    //Serial.printf("Read only test of %s\n", temp_file_path);
    temp_file = SD.open(temp_file_path);                                                  //  SD.open() does not mind the trailing slash if the name is a directory
    if(temp_file.isReadOnly())
    {
#if TRACE_SDCAT
      Serial.printf("SDCAT 18\n");
#endif
      AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1] |= 0x02;                  //  Indicate the file is read only
    }
#if TRACE_SDCAT
    Serial.printf("SDCAT 19\n");
#endif
    temp_file.close();
//    Serial.printf("Filename [%s]   Date/time [%s]   Size [%s] = %d  DIR&RO status %d\n\n",  AUXROM_RAM_Window.as_struct.AR_Buffer_6,
//                                                                                            &AUXROM_RAM_Window.as_struct.AR_Buffer_6[256],
//                                                                                            file_size, temp_uint,
//                                                                                            AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1]  );
    AUXROM_RAM_Window.as_struct.AR_Usages[6]  = 0;        //  Success
    //show_mailboxes_and_usage();
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;  //  Indicate we are done
    return;
  }
}

//
//  Update the Current Path with the provided path. Most of the hard work happens in Resolve_Path()
//  See its documentation
//
//  AUXROM puts the update path in Buffer 6, and the length in AR_Lengths[6]
//  Mailbox 6 is used for the handshake
//

void AUXROM_SDCD(void)
{
  int         error_number = 0;
  char        error_message[33];

#if VERBOSE_KEYWORDS
  Serial.printf("Current Path before SDCD [%s]\n", Current_Path);
  Serial.printf("Update Path via AUXROM   [%s]\n", AUXROM_RAM_Window.as_struct.AR_Buffer_6);
  //show_mailboxes_and_usage();
#endif

  do
  {
    if(Resolve_Path((char *)AUXROM_RAM_Window.as_struct.AR_Buffer_6))       //  Returns true if no parsing problems
    {
      if((file = SD.open(Resolved_Path)) == 0)
      {
        error_number = 340;   strlcpy(error_message,"Unable to open directory", 32);
        break;                      //  Unable to open directory
      }
      if(!file.isDir())
      {
        file.close();
        error_number = 341;   strlcpy(error_message,"Target path is not a directory", 32);
        break;                      //  Valid path, but not a directory
      }
      file.close();
      if(!SD.chdir(Resolved_Path))
      {
        error_number = 342;   strlcpy(error_message,"Couldn't change directory", 32);
        break;                      //  Failed to change to new path
      }
      //
      //  Parsed ok, opened ok, is a directory, successfully changed directory, so update the Current path, and make it the current directory
      //
      strlcpy(Current_Path, Resolved_Path, MAX_SD_PATH_LENGTH + 1);
      if(!Resolved_Path_ends_with_slash)
      {
        strlcat(Current_Path,"/", MAX_SD_PATH_LENGTH + 1);
      }
      AUXROM_RAM_Window.as_struct.AR_Usages[6] = 0;                         //  Indicate Success

#if VERBOSE_KEYWORDS
      Serial.printf("Success. Current Path is now [%s]\n", Current_Path);
      //show_mailboxes_and_usage();
      //Serial.printf("\nSetting Mailbox 6 to 0 and then returning\n");
#endif

      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Release mailbox 6.    Must always be the last thing we do
      return;
    }
    error_number = 343;   strlcpy(error_message,"Couldn't resolve path", 32);
  } while(0);
  //
  //  This is the shared error exit
  //

#if VERBOSE_KEYWORDS
    Serial.printf("SDCD failed Message [%s]  error_number %d\n", error_message, error_number);
#endif

  post_custom_error_message(error_message, error_number);
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
  return;
}

//
//  Close 1..n close specific file. 0 means close all open files
//

void AUXROM_SDCLOSE(void)
{
  int         file_index;
  int         i;
  bool        return_status;
  char        filename[258];

  file_index = AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0];               //  File number 1..10 , or 0 for all
  if(file_index == 0)
  {
    for(i = 1 ;  i < MAX_AUXROM_SDFILES ; i++)
    {
      if(Auxrom_Files[i].isOpen())
      {
        return_status = Auxrom_Files[i].close();
        Auxrom_Files[i].getName(filename,255);
        Serial.printf("Close file %2d [%s]  Success status is %s\n", i, filename, return_status ? "true":"false");
        //
        //  No error exit
        //
      }
    }
  }
  else
  {
    return_status = Auxrom_Files[file_index].close();
    Auxrom_Files[file_index].getName(filename,255);
    Serial.printf("Close file %2d [%s]  Success status is %s\n", file_index, filename, return_status ? "true":"false");
  }
  AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 0;     //  File flush successfully
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;     //  Indicate we are done
  return;
}

//
//  Return in the designated buffer the current path and its length
//

void AUXROM_SDCUR(void)
{
  uint16_t    copy_length;
  copy_length = strlcpy((char *)(&AUXROM_RAM_Window.as_struct.AR_Buffer_0[Mailbox_to_be_processed*256]), Current_Path, 256);
  AUXROM_RAM_Window.as_struct.AR_Lengths[Mailbox_to_be_processed] = copy_length;
  AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;                         //  Indicate Success
  //  show_mailboxes_and_usage();
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[Mailbox_to_be_processed] = 0;                      //  Release mailbox.    Must always be the last thing we do
}

void AUXROM_SDDEL(void)
{
  if(!Resolve_Path(AUXROM_RAM_Window.as_struct.AR_Buffer_6))
  {   //  Error, Parsing problems with path
    post_custom_error_message((char *)"Parsing problems with path", 370);
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
    return;
  }
  if(!SD.remove(Resolved_Path))
  {
    post_custom_error_message((char *)"Couldn't delete file", 371);
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
    return;
  }
  AUXROM_RAM_Window.as_struct.AR_Usages[6] = 0;           //  Success, file deleted
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
  return;
}

//
//  Flush any pending write data
//  File number 1..10 are for a single open file
//  If File number provided is 0, Flush all open files
//

void AUXROM_SDFLUSH(void)
{
  int         file_index;
  int         i;

  file_index = AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0];               //  File number 1..10 , or 0 for all
  if(file_index == 0)
  {
    for(i = 1 ;  i < MAX_AUXROM_SDFILES ; i++)
    {
      if(Auxrom_Files[i].isOpen())
      {
        Auxrom_Files[i].flush();
        Serial.printf("Flushing file %2d\n", i);
        //
        //  No error exit
        //
      }
    }
  }
  else
  {
    Auxrom_Files[file_index].flush();
    Serial.printf("Flushing file %2d\n", file_index);
  }
  AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 0;     //  File flush successfully
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;     //  Indicate we are done
  return;
}

//
//
//

void AUXROM_SDMEDIA(void)
{

}



//
//  Create a new directory in the current directory
//  AUXROM puts the new directory name in Buffer 6, Null terminated
//  The length in AR_Lengths[6]
//  Mailbox 6 is used for the handshake
//

void AUXROM_SDMKDIR(void)
{
  bool  mkdir_status;
  Serial.printf("New directory name   [%s]\n", AUXROM_RAM_Window.as_struct.AR_Buffer_6);
  //  show_mailboxes_and_usage();
  mkdir_status = SD.mkdir(AUXROM_RAM_Window.as_struct.AR_Buffer_6, true);    //  second parameter is to create parent directories if needed

  Serial.printf("mkdir return status [%s]\n", mkdir_status ? "true":"false");

  if(mkdir_status)
  {
    AUXROM_RAM_Window.as_struct.AR_Usages[6] = 0;                         //  Indicate Success
  }
  else
  {
    AUXROM_RAM_Window.as_struct.AR_Usages[6] =  213;                      //  Indicate Failure
  }
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Release mailbox 6.    Must always be the last thing we do
  return;
}

void AUXROM_MOUNT(void)
{

}

//
//  Open a file on the SD Card for direct access via the AUXROM functions
//

void AUXROM_SDOPEN(void)
{
  int         file_index;
  bool        error_occured;
  int         error_number;
  char        error_message[33];

  file_index = AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0];               //  File number 1..11

#if VERBOSE_KEYWORDS
  Serial.printf("SDOPEN Name: [%s]  Mode %d  File # %d\n", AUXROM_RAM_Window.as_struct.AR_Buffer_6,
                                                            AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[1],
                                                            file_index);
#endif

  do
  {
    error_number = 420;   strlcpy(error_message,"File is already open", 32);
    if(Auxrom_Files[file_index].isOpen()) break;                          //  Error, File is already open
    error_number = 421;   strlcpy(error_message,"Parsing problems with path", 32);
    if(!Resolve_Path(AUXROM_RAM_Window.as_struct.AR_Buffer_6)) break;     //  Error, Parsing problems with path
    error_occured = false;
    switch(AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[1])
    {
      case 0:       //  Mode 0 (READ-ONLY), error if the file doesn't exist
        error_number = 422;   strlcpy(error_message,"Open failed Mode 0", 32);
        if(!Auxrom_Files[file_index].open(Resolved_Path , O_RDONLY | O_BINARY)) error_occured = true;
        break;
      case 1:       //  Mode 1 (R/W, append)
        error_number = 423;   strlcpy(error_message,"Open failed Mode 1", 32);
        if(!Auxrom_Files[file_index].open(Resolved_Path , O_RDWR | O_APPEND | O_CREAT | O_BINARY)) error_occured = true;
        break;
      case 2:       //  Mode 2 (R/W, truncate)
        error_number = 424;   strlcpy(error_message,"Open failed Mode 2", 32);
        //if(!Auxrom_Files[file_index].open(Resolved_Path , O_RDWR | O_TRUNC | O_CREAT | O_BINARY)) error_occured = true;
        if(!Auxrom_Files[file_index].open(Resolved_Path , FILE_WRITE)) error_occured = true;
       break;
      default:
        error_number = 425;   strlcpy(error_message,"Open failed, Illegal Mode", 32);        // This should never happen because AUXROM checks Mode is 0,1,2
        error_occured = true;                                                              //  And yet, we have seen it due to a bug in AUXROM code
        break;
    }
    if(error_occured) break;
    AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 0;     //  File opened successfully
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;     //  Indicate we are done

#if VERBOSE_KEYWORDS
    Serial.printf("SDOPEN Success [%s]\nHandle status:", Resolved_Path);
    Serial.printf("curPosition :       %d\n", Auxrom_Files[file_index].curPosition());
    Serial.printf("isOpen :            %s\n", Auxrom_Files[file_index].isOpen()  ? "true":"false");
    Serial.printf("isWritable :        %s\n", Auxrom_Files[file_index].isWritable()  ? "true":"false");
    Serial.printf("position :          %d\n", Auxrom_Files[file_index].position());
#endif

    return;
  } while (0);
  //
  //  This is the shared error exit
  //

#if VERBOSE_KEYWORDS
    Serial.printf("SDOPEN failed Message [%s]  error_number %d\n", error_message, error_number);
#endif

  post_custom_error_message(error_message, error_number);
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
  return;
}

void AUXROM_SPF(void)
{

}


//
//  Read specified number of bytes from an open file, and store directly in HP85 Memory
//
//  Check with Everett: We always talked about this read storing to a pre-allocated string. Could it also be
//  used to store into an 8 byte Real, or an array?
//
//  Position after read is next character to be read
//

void AUXROM_SDREAD(void)                                                 //  UNTESTED  UNTESTED  UNTESTED  UNTESTED  UNTESTED
{
  int         file_index;
  int         bytes_to_read;
  int         bytes_actually_read;
  uint16_t    HP85_Mem_Address;

  file_index = AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0];               //  File number 1..11
  bytes_to_read = AUXROM_RAM_Window.as_struct.AR_Lengths[6];              //  Length of read
  if(!Auxrom_Files[file_index].isOpen())
  {
    AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 213;    //  Error
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
    Serial.printf("SDREAD Error. File not open. File Number %d\n", file_index);
    return;
  }
  //  following crappy extract because still haven't found a way around
  //  dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
  HP85_Mem_Address = AUXROM_RAM_Window.as_struct.AR_Buffer_6[0] | (AUXROM_RAM_Window.as_struct.AR_Buffer_6[1] << 8);

  bytes_actually_read = Auxrom_Files[file_index].readBytes(Transfer_Buffer, bytes_to_read);
  Serial.printf("Read file # %2d , requested %d bytes, got %d\n", file_index, bytes_to_read, bytes_actually_read);
  //Serial.printf("Target address in HP85 memory is %06o  %08X\n", HP85_Mem_Address, HP85_Mem_Address);
  //Serial.printf("[%s]\n", Transfer_Buffer);
  AUXROM_Store_Memory(HP85_Mem_Address, Transfer_Buffer, bytes_actually_read);

  //
  //  Assume all is good
  //
  AUXROM_RAM_Window.as_struct.AR_Lengths[6] = bytes_actually_read;
  AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 0;     //  SDREAD successful
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;     //  Indicate we are done
  return;
}

void AUXROM_SDREN(void)
{

}

//
//  Delete a directory but only if it is empty
//  AUXROM puts the directory to be deleted in Buffer 6, Null terminated , apply Resolve_Path()
//  The length in AR_Lengths[6]
//  Mailbox 6 is used for the handshake
//

void AUXROM_SDRMDIR(void)
{
  bool  rmdir_status;
  File  dirfile;
  File  file;
  int   file_count;

  if(!Resolve_Path((char *)AUXROM_RAM_Window.as_struct.AR_Buffer_6))
  {
    post_custom_error_message((char *)"Can't resolve path", 310);
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Indicate we are done
    Serial.printf("SDRMDIR Error exit 1.  Error while resolving subdirectory name\n");
    return;
  }
  Serial.printf("Deleting directory [%s]\n", Resolved_Path);
  if((dirfile.open(Resolved_Path)) == 0)
  {
    Serial.printf("SDRMDIR: Failed SD.open return value %08X\n", (uint32_t)file);
    post_custom_error_message((char *)"Can't open subdirectory", 311);
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Indicate we are done
    return;
  }
  if(!dirfile.isDir())
  {
    Serial.printf("SDRMDIR: Failed file.isDir  Path is not a directory\n");
    post_custom_error_message((char *)"Path is not a directory", 312);
    dirfile.close();
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Indicate we are done
    return;
  }
  file_count = 0;
  dirfile.rewind();
  while(file.openNext(&dirfile, O_RDONLY))
  {
    file_count++;
    file.close();
  }
  Serial.printf("SDRMDIR directory has %d files\n", file_count);
  if(file_count != 0)
  {
    Serial.printf("SDRMDIR: Directory is not empty. found %d files\n", file_count);
    post_custom_error_message((char *)"Directory not empty", 313);
    dirfile.close();
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Indicate we are done
    return;
  }
  //
  //  Specified path is a directory that is empty, so ok to delete
  //
  Serial.printf("SDRMDIR: Deleting directory %s\n", Resolved_Path);
  rmdir_status = dirfile.rmdir();
  Serial.printf("SDRMDIR: Delete status is %s\n", rmdir_status? "true":"false");
  if(rmdir_status)
  {
    AUXROM_RAM_Window.as_struct.AR_Usages[6] = 0;                         //  Indicate Success
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Release mailbox 6.    Must always be the last thing we do
    return;
  }
  else
  {
    Serial.printf("SDRMDIR: Delete directory failed\n");
    post_custom_error_message((char *)"Directory delete failed", 314);
    dirfile.close();
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Indicate we are done
    return;
  }
}

//
//  Seek to a specific position of the designated file. File position 0 is the first character/byte
//
//  Mode      Offset
//  0         Absolute position.            Offset must be 0, or positive
//  1         Offset from current position. Offset could be positive or negative
//  2         Offset from end of file.      Offset must be 0, or negative
//

void AUXROM_SDSEEK(void)
{
  int         file_index;
  int         seek_mode;
  int         offset;
  int         current_position;
  int         end_position;
  int         target_position;

  file_index = AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0];               //  File number 1..11
  seek_mode  = AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[1];               //  0=absolute position, 1=advance from current position, 2=go to end of file
  offset     = AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1]; //  Fetch the file offset, or absolute position
  Serial.printf("\nSDSEEK Mode %d   Offset %d\n", seek_mode, offset);
  //
  //  check the file is open
  //
  if(!Auxrom_Files[file_index].isOpen())
  {
    post_custom_error_message((char *)"Seek on file that isn't open", 470);
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Indicate we are done
    Serial.printf("SDSEEK Error exit 470.  Seek on file that isn't open\n");
    return;
  }
  //
  //  Get current position
  //
  current_position = Auxrom_Files[file_index].curPosition();
  Serial.printf("SDSEEK: Position prior to seek %d\n", current_position);
  //
  //  Get end position
  //
  Auxrom_Files[file_index].seekEnd(0);
  end_position = Auxrom_Files[file_index].curPosition();
  //Serial.printf("SDSEEK: file size %d\n", end_position);
  Auxrom_Files[file_index].seekSet(current_position);                       //  Restore position, in case something goes wrong
  //Serial.printf("SDSEEK: Position after restore is %d\n", Auxrom_Files[file_index].curPosition());
  if(seek_mode == 0)
  {
    target_position = offset;
  }
	else if(seek_mode == 1)
	{
	  target_position = current_position + offset;
	}
  else                                                                      //  Trust that AUXROM never passes a mode other than 0, 1, 2
  {
    target_position = end_position + offset;
  }
  //
  //  Check that seek is within file
  //
  if(target_position < 0)
  {
    post_custom_error_message((char *)"Trying to seek before beginning", 471);
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Indicate we are done
    Serial.printf("SDSEEK Error exit 471.  Trying to seek past EOF\n");
    return;
  }
  if(target_position > end_position)
  {
    post_custom_error_message((char *)"Trying to seek past EOF", 472);
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Indicate we are done
    Serial.printf("SDSEEK Error exit 472.  Trying to seek past EOF\n");
    return;
  }
  Auxrom_Files[file_index].seekSet(target_position);
  Serial.printf("SDSEEK: New position is %d\n", (current_position = Auxrom_Files[file_index].curPosition()) );
  AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1] = current_position;
  AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 0;     //  SDSEEK successful
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;     //  Indicate we are done
  return;
}

//
//  Write the specified number of bytes to an open file
//

void AUXROM_SDWRITE(void)                                                 //  UNTESTED  UNTESTED  UNTESTED  UNTESTED  UNTESTED
{
  int         file_index;
  int         bytes_to_write;
  int         bytes_actually_written;
  uint16_t    HP85_Mem_Address;

  file_index = AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0];               //  File number 1..11
  bytes_to_write = AUXROM_RAM_Window.as_struct.AR_Lengths[6];             //  Length of write
  if(!Auxrom_Files[file_index].isWritable())
  {
    AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 213;    //  Error
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
    Serial.printf("SDWRITE Error. File not open for write. File Number %d\n", file_index);
    return;
  }
  //  following crappy extract because still haven't found a way around
  //  dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
  HP85_Mem_Address = AUXROM_RAM_Window.as_struct.AR_Buffer_6[0] | (AUXROM_RAM_Window.as_struct.AR_Buffer_6[1] << 8);

  AUXROM_Fetch_Memory((uint8_t *)Transfer_Buffer, HP85_Mem_Address, bytes_to_write);
  bytes_actually_written = Auxrom_Files[file_index].write(Transfer_Buffer, bytes_to_write);
  Serial.printf("Write file # %2d , requested write %d bytes, %d actually written\n", file_index, bytes_to_write, bytes_actually_written);
  //
  //  Assume all is good
  //
  AUXROM_RAM_Window.as_struct.AR_Lengths[6] = bytes_actually_written;
  AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 0;     //  SDWRITE successful
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;     //  Indicate we are done
  return;
}

void AUXROM_UNMNT(void)
{

}

//
//  WROM overwrites AUXROMs. This is quite dangerous, so be careful
//
//  WROM COMMAND: FOR THE E.WROM COMMAND: A.BOPT00 = TARGET ROM#, A.BOPT01 = 0, A.BOPT02-03 = TARGET ADDRESS
//
//  Always uses Buffer 0 and associated usage locations etc.
//
//  Target ROM must be between 0361 and 0376 (241 to 254)

void AUXROM_WROM(void)
{
  uint8_t     target_rom      = AUXROM_RAM_Window.as_struct.AR_BUF0_OPTS[0];
  uint16_t    target_address  = AUXROM_RAM_Window.as_struct_a.AR_BUF0_OPTS[1];                      //  address is in the range 060000 to 0100000
                                                                                                    //  NOTE: This uses as_struct_a to avoid a compiler warning. See EBTKS_Global_Data.h
  uint16_t    transfer_length = AUXROM_RAM_Window.as_struct.AR_Lengths[0];
  uint8_t   * data_ptr        = &AUXROM_RAM_Window.as_struct_a.AR_Buffer_0[0];
  uint8_t   * dest_ptr;

  Serial.printf("Target ROM ID(oct):     %03O\n", target_rom);
  Serial.printf("Target Address(oct): %06O\n", target_address);
  Serial.printf("Bytes to write(dec):     %d\n", transfer_length);

  if((dest_ptr = getROMEntry(target_rom)) == NULL)            //  romMap is a table of 256 pointers to locations in EBTKS's address space.
  {                                                           //  i.e. 32 bit pointers. Either NULL, or an address. The index is the ROM ID
    Serial.printf("Selected ROM not loaded\n");
    // return error
  }
  if((target_rom < 0361) || (target_rom > 254))
  {
    Serial.printf("Selected ROM is not an AUXROM\n");
    // return error
  }
  if(target_address + transfer_length  >= 0100000 )
  {
    transfer_length = 0100000 - target_address;	          //  Don't let AUXROM write past end of ROM.  Maybe we should report an error
  }
  dest_ptr = dest_ptr + (target_address - 060000);
  while(transfer_length--)
  {
    *dest_ptr++ = *data_ptr    ++;
  }
}




//
//  This function takes New_Path and appends it to Current_Path (if it is a relative path) and
//  puts the result in Resolved_Path. It handles both Absolute and Relative strings in New_Path,
//  and can be used for directory paths as well as file paths. The result is in Resolved_Path
//  It deals with embedded ../ and ./ which allows relative navigation within the directory tree.
//  The resultant path in Resolved_Path has all ../ and ./ removed, so the result is always an
//  absolute path, suitable for directory and file operations.
//
//  Returns true if no errors detected
//

bool Resolve_Path(char *New_Path)
{
  char *    dest_ptr;                                         //  Points to the trailing 0x00 of the new path we are creating
  char *    dest_ptr2;                                        //  Used while processing ../
  char *    src_ptr;                                          //  Points to next character in New_Path to be processed
  bool      back_up_1_level;

  if(New_Path[0] == '/')
	{
	  Resolved_Path[0] = '/';                                   //  Handle an absolute path
	  Resolved_Path[1] = 0x00;
	  dest_ptr = Resolved_Path + 1;
	  src_ptr = New_Path + 1;
	}
	else
	{
	  strlcpy(Resolved_Path, Current_Path, MAX_SD_PATH_LENGTH); //  Handle relative path, so start by copying the Current_Path to the work area. Assume it does not contain any ../ or ./
	                                                            //  Also, since Current_Path is a directory path, it has both leading and trailing '/' (if root, just one '/')
	  dest_ptr = Resolved_Path + strlen(Resolved_Path);
	  src_ptr = New_Path;
	  if(Resolved_Path[0] != '/')
	  {
	    return false;                                           //  make sure that the first character of the path we are building is a '/'
	  }
	  if(dest_ptr[-1] != '/')
	  {
	    return false;                                           //  make sure that the last character of the path we are building is a '/'
	  }
	}

  //Serial.printf("Path so far [%s]\n", Resolved_Path);
  //
  //  Parse the New_Path. Handle the following:
  //  /         separates directory names
  //  ./        current directory. just remove it
  //  ../       remove a directory level
  //  text      a directory level, or a file name (file name only allowed in final segment)
  //

  while(1)
  {
    //
    //  Look at next part of New_Path
    //
    //  If end of string stop
    //  If next char is "/" it is an error
    //  If next 2 chars are "./" just strip them
    //  If next 3 chars are "../" remove a path segment from the path under construction. Do not remove the first '/'. If nothing to remove, it is an error
    //  If next 2 chars are ".."  AND it is at the end of the string, remove a path segment from the path under construction. Do not remove the first '/'. If nothing to remove, it is an error
    //  Collect characters upto and including the next '/' and append to the path under construction.
    //
    if(src_ptr[0] == 0x00)
    {
      break;                                          //  At end of the New_Path
    }
    if(src_ptr[0] == '/')
    {
      return false;                                   //  Can't have an embedded //. We would have picked the first one when copying a path segment
    }
    if(src_ptr[0] == '.' && src_ptr[1] == '/')
    {
      src_ptr += 2;                                   //  Found "./" which has no use, so just delete it
      continue;
    }
    back_up_1_level = false;
    if(src_ptr[0] == '.' && src_ptr[1] == '.' && src_ptr[2] == '/')
    {
      back_up_1_level = true;
      src_ptr += 3;                                   //  Found "../" which means up 1 directory, so need to delete a path segment
    }
    if(src_ptr[0] == '.' && src_ptr[1] == '.' && src_ptr[2] == 0x00)
    {
      back_up_1_level = true;
      src_ptr += 2;                                   //  Found ".." at end of src, which means up 1 directory, so need to delete a path segment
    }
    if(back_up_1_level)
    {
      if(strlen(Resolved_Path) <= 2)                  //  The minimal Resolved_Path that could have a directory to be removed is 3 characters"/x/"
      {                                               //    This could have been done with dest_ptr - Resolved_Path, strlen is clearer and less bug prone.
        return false;                                 //  The built path does not have a trailing directory to be removed
      }
      dest_ptr2 = dest_ptr - 1;                       //  Point at the last character of the path built so far. Should be a '/'
      if(dest_ptr2[0] != '/')
      {
        return false;                                 //  The built path must have a trailing '/'. This would indicate a bug somewhere in this function
      }
      dest_ptr2--;                                    //  Move just before the trailing '/'
      //
      //  We already have checked that the first character of Resolved_Path is '/' , so scan
      //  backwards from the second last character until we find a '/', and write 0x00 just after it
      //
      while(*dest_ptr2 != '/')
      {
        dest_ptr2--;                                  //  Scan backwards
      }
      dest_ptr2[1] = 0x00;                            //  Found the prior '/', set the following character to 0x00 thus deleting a path segment
      dest_ptr = dest_ptr2 + 1;
      continue;
    }
    //
    //  If we get here, none of the special cases apply, so just append a path segment, including its trailing '/' (except if we finish with the New_Path, which means we may have a file name)
    //
    while(1)
    {
      if(src_ptr[0] == '/')                           //  This should not match on the first pass through this while(1) loop, as we have already checked for a leading '/'
      {
        *dest_ptr++ = *src_ptr++;                     //  Copy the '/' and we are done for this path segment
        *dest_ptr = 0x00;                             //  Mark the new end of the string, probably redundant
        break;
      }
      if(src_ptr[0] == 0x00)
      {                                               //  Finished processing charcters from New_Path. We already have a trailing 0x00, so just exit while loop
        break;
      }
      //
      //  Not a '/' , and not end of string, so just copy the character
      //
      *dest_ptr++     = *src_ptr++;                   //  Copy the character
      *dest_ptr       = 0x00;                         //  Keep it a valid string
    }
    //
    //  End of this piece of the SD path, loop back for more
    //
  }
  //
  //  We have finished Resolving the path.
  //
  Resolved_Path_ends_with_slash = (dest_ptr[-1] == '/');
  //Serial.printf("Resolved_Path is [%s]\n", Resolved_Path);
  return true;
}

File dir;
File dir_entry;
uint8_t dir_flags;
uint8_t dir_indent;
uint32_t    Listing_Buffer_Index;

//
//  Do a directory listing of the Current Working Directory
//  The text buffer is 65536 bytes, so with a line limit of 32 characters per line,
//  this would allow for 2048 lines. If directory listing exceeds 64K characters,
//  we just truncate the listing. The user should split the directory into mutiple
//  separate directories. Sorry. Also, we just do 1 level. If a recursive directory
//  is desired, then the user must implement walking the directory tree in BASIC
//  code.
//
//  Returns true if the directory was opened successfuly. There might not be any
//  listing text, if the directory has no files or sub directories.
//

bool LineAtATime_ls_Init(char * path)
{
  char      dir_name[51];

  Listing_Buffer_Index  = 0;
  if(dir)
  {
    dir.close();
  }
  if(!dir.open(path, O_RDONLY))
  {
    return false;
  }
  dir.getName(dir_name, 50);
  //Serial.printf("LAAT_ls path [%s]   dir_name [%s]\n", path, dir_name);
  PS.init(Directory_Listing_Buffer, DIRECTORY_LISTING_BUFFER_SIZE);
  dir.ls(&PS, LS_SIZE | LS_DATE, 0);    //  No indent needed (3rd parameter) because we are not doing a recursive listing
  dir.close();
  return true;
}

bool LineAtATime_ls_Next()
{
  get_line_char_count = PS.get_line(dir_line, 128);

  if(get_line_char_count >= 0)
  {
    //Serial.printf("%s\n", dir_line);
    return true;
  }
  //
  //  Must be negative, so no more lines
  //
  return false;
}







//
//  Custom Error and Warning Messages
//
//  All addresses are AUXROM 1
//
//    The custom messages area starts at 060024  EBTKSMSG and extends for
//    34D bytes the last of which must always have the MSB set. Exactly two
//    more bytes in this 34 byte area must also have their MSB set and
//    depends on whether a custom warning or error message is being setup.
//
//    For a custom warning, it starts at 0600024 and is no more than 32
//    bytes, with the last valid byte MSB set. The 32 bytes can be padded
//    with spaces. The 33rd byte (at 060064) must have its MSB set, for a
//    total of 3 bytes having the MSB set: at the end of the custom warning,
//    at 060064 and 060065
//
//    For a custom error, the byte at 0600024 must have its MSB set. The
//    error message starts at 0600025 and is no more than 32 bytes, with the
//    last valid byte MSB set. The 32 bytes can be padded with spaces. The
//    byte at 060065 must have its MSB set, for a total of 3 bytes having
//    the MSB set: at 060024, at the end of the custom error, and at 060065
//
//    Custom messages are terminated by the kat byte having the MSB set.
//    Terminating 0x00 is not used
//
//  The image of the ROM on the SD card is not modified.
//  The custom error message is issued by specifying error number 209,
//  which will appear on the HP85 screen as 109.
//  For custom warning the warning number 208 is specified, and displays as
//  warning 108
//


void post_custom_error_message(char * message, uint16_t error_number)
{
  char   * dest_ptr;

  if((dest_ptr = (char *)getROMEntry(0361)) == NULL)            //  romMap is a table of 256 pointers to locations in EBTKS's address space.
  {                                                             //  i.e. 32 bit pointers. Either NULL, or an address. The index is the ROM ID
    Serial.printf("ROM 0361 not loaded\n");
    // return error
  }

  dest_ptr[024] = 0x80;                                         //  Zero length for warning 8
  strncpy(&dest_ptr[025], message, 32);                         //  If message is less than 32 characters, strncpy pads the destination with 0x00

  dest_ptr[025 + strlen(message) - 1] |= 0x80;                  //  Mark the end of the custom error message in HP85 style by setting the MSB
  AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 209;            //  Custom Error
  AUXROM_RAM_Window.as_struct.AR_ERR_NUM      = error_number;   //  AUXERRN
}

void post_custom_warning_message(char * message, uint16_t error_number)
{
  char   * dest_ptr;

  if((dest_ptr = (char *)getROMEntry(0361)) == NULL)            //  romMap is a table of 256 pointers to locations in EBTKS's address space.
  {                                                             //  i.e. 32 bit pointers. Either NULL, or an address. The index is the ROM ID
    Serial.printf("ROM 0361 not loaded\n");
    // return error
  }

  strncpy(&dest_ptr[024], message, 32);                         //  If message is less than 32 characters, strncpy pads the destination with 0x00
  dest_ptr[024 + strlen(message) - 1] |= 0x80;                  //  Mark the end of the custom warning message in HP85 style by setting the MSB
  dest_ptr[064] = 0x80;                                         //  Zero length for error message 9
  AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 208;            //  Custom Warning
  AUXROM_RAM_Window.as_struct.AR_ERR_NUM      = error_number;   //  AUXERRN
}

