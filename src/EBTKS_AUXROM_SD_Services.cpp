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
//  10/17/2020        Fix pervasive errors in how I was handling buffers
//
//  10/19/2020        SDMOUNT
//  10/20/2020        SDMOUNT
//  10/21/2020        SDMOUNT
//  10/22/2020        SDDEL  rewrite to handle wildcards. Not as easy as you might think.
//                    MEDIA$
//  10/23/2020        SDMOUNT total re-write to support Tape and Disk
//  10/24/2020        Update SDREAD and SDWRITE to match AUXROM Release 11
//                    UNMOUNT
//  11/xx/2020        lots of work, forgot to update this
//  11/25/2020        SPRINTF
//
//  02/26/2021        Undid 100's of edits, so they could be staged to github in a more reasonable manner.
//                    Did this by cloning from Github, Commit 102 of 2/17/2021
//                    On trying to build the cloned project, got multiple errors in EBTKS_AUXROM_SD_Services.cpp
//                    like:   src\EBTKS_AUXROM_SD_Services.cpp:565:19: error: 'File {aka class FsFile}' has no member named 'isReadOnly'
//                    The fix is to edit D:\2021\HP-85_EBTKS_V1.0\Firmware\.pio\libdeps\teensy41\SdFat\src\SdFatConfig.h at line 78
//                    and change it from
//                        #define SDFAT_FILE_TYPE 3
//                    to
//                        #define SDFAT_FILE_TYPE 1     // PMF and RB was 3, but caused library problems like can't find isReadOnly()

/////////////////////On error message / error codes.  Go see email log for this text in context
//
//        Error Message numbers, returned in Usage[0] (or which ever was used for the call)
//
//        0             No error
//        1-99          mail??   maybe he means mailbox specific, or mail is mailbox, usage, len, and buffer
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
//                                          310       Can't open /AUXROM_FLAGS.TXT
//                                          311       Can't write /AUXROM_FLAGS.TXT
//        320..329      AUXROM_HELP
//        330..339      AUXROM_SDCAT
//                                          330       Can't resolve path                  Used by multiple functions when Resolve_Path() fails
//                                          331       Can't list directory
//                                          332       No SDCAT init or past end
//                                          333       SDCAT no wildcards in path
//        340..349      AUXROM_SDCD
//                                                    Shares use of 330
//                                          340       Unable to open directory
//                                          341       Target path is not a directory
//                                          342       Couldn't change directory
//        350..359      AUXROM_SDCLOSE
//        360..369      AUXROM_SDCUR
//        370..379      AUXROM_SDDEL
//                                                    Shares use of 330
//                                          370       SDDEL no file specified
//                                          371       Couldn't delete file
//                                          372       SDDEL no wildcards in path
//                                          373       SDDEL problem with path
//        380..389      AUXROM_SDFLUSH
//        390..399      AUXROM_SDFLUSH
//        400..409      AUXROM_SDMKDIR
//                                          409       MOUNT File already exists
//        410..419      AUXROM_MOUNT
//                                                    Shares use of 330
//                                          410       Invalid MOUNT mode
//                                          411       MOUNT file does not exist
//                                          412       MOUNT MSU$ error
//                                          413       MOUNT Filename must end in .tap
//                                          414       MOUNT HPIB Select must match
//                                          415       MOUNT failed
//                                          416       MOUNT Filename must end in .dsk
//                                          417       Couldn't open Ref Disk
//                                          418       Couldn't open New Disk
//                                          419       Couldn't init New Disk
//        420..429      AUXROM_SDOPEN
//                                          420       File is already open
//                                          421       Parsing problems with path
//                                          422       Open failed Mode 0
//                                          423       Open failed Mode 1
//                                          424       Open failed Mode 2
//                                          425       Open failedIllegal Mode
//                                          426       Couldn't open Ref Tape
//                                          427       Couldn't open New Tape
//                                          428       Couldn't init New Tape
//        430..439      AUXROM_SPF
//        440..449      AUXROM_SDREAD
//                                          440       SDREAD File not open
//        450..459      AUXROM_SDREN
//                                          450       Can't resolve Old path
//                                          451       Can't resolve New path
//                                          452       SDREN rename failed
//                                          453       SDREN Mystery bug
//                                          454       SDREN Old file doesn't exist
//        460..469      AUXROM_SDRMDIR
//        470..479      AUXROM_SDSEEK
//                                          470       Seek on file that isn't open
//                                          471       Trying to seek before beginning
//                                          472       Trying to seek past EOF
//                                          473       SDSEEK failed somehow
//        480..489      AUXROM_SDWRITE
//                                          480       SDWRITE File not open for write
//        490..499      AUXROM_UNMOUNT
//                                          490       UNMOUNT MSU$ error
//                                          491       UNMOUNT Disk error
//        500..509      AUXROM_WROM
//        510..519      AUXROM_MEDIA
//                                          510       MEDIA$ MSU$ error
//                                          511       MEDIA$ HPIB Select must match
//                                          512       Device code not supported
//        520..529      AUXROM_SDCOPY
//                                          520       Can't resolve Source path
//                                          521       Can't resolve Destination path
//                                          522       SDCOPY bug in code
//                                          523       Source file doesn't exist
//                                          524       Couldn't open Source File
//                                          525       Couldn't open Destination File
//                                          526       File copy failed
//                                          527       SDCOPY File already Exists
//        530..539      AUXROM_SDEOF
//                                          530       SDEOF file isn't open
//        540..549      AUXROM_SDEXISTS
//



#include <Arduino.h>
#include <string.h>
#include <strings.h>                //  needed for strcasecmp() prototype
#include <math.h>
#include <stdlib.h>

#include "Inc_Common_Headers.h"
#include "HpibDisk.h"

#define   VERBOSE_KEYWORDS          (0)
#define   VERBOSE_SPRINTF           (0)

#define MAX_SD_PATH_LENGTH          (256)

extern HpibDevice *devices[];

EXTMEM static char          Current_Path[MAX_SD_PATH_LENGTH  + 2];    //  I don't think initialization of EXTMEM is supported yet
EXTMEM static char          Resolved_Path[MAX_SD_PATH_LENGTH + 2];
static bool                 Resolved_Path_ends_with_slash;
EXTMEM static char          SDCAT_path_part_of_Resolved_Path[MAX_SD_PATH_LENGTH + 2];
EXTMEM static char          SDCAT_pattern_part_of_Resolved_Path[MAX_SD_PATH_LENGTH + 2];
EXTMEM static char          SDDEL_path_part_of_Resolved_Path[MAX_SD_PATH_LENGTH + 2];
EXTMEM static char          SDDEL_pattern_part_of_Resolved_Path[MAX_SD_PATH_LENGTH + 2];
EXTMEM static char          Resolved_Old_Path[MAX_SD_PATH_LENGTH + 2];


bool parse_MSU(char *msu);

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
//  1) Must initialize with something like: if (!SD.begin(SdioConfig(FIFO_SDIO))) { //  failure to open SD Card.  }
//  2) if (SD.exists("Folder1"))    for folders returns true with/without trailing slash
//  3) Does not support wildcards
//  4) Does not support ./  and  ../  for path navigation
//  5) No apparent way to access the Volume working durectory (vwd) or Current Working Directory (cwd)
//

File        file;
//  SdFile      SD_file;    //  We never used this. How is this different from File?
                            //  Apparently "FAT16/FAT32 file with Print"
                            //  See G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_1.0\.pio\libdeps\teensy41\SdFat\examples\examplesV1\rename\rename.ino  line 53
FatFile     Fat_File;
FatVolume   Fat_Volume;
File        HP85_file[11];                                          //  10 file handles for the user, and one for Everett
EXTMEM      char HP85_file_name[11][MAX_SD_PATH_LENGTH + 2];        //  Full path to the file name (the last part)

void initialize_SD_functions(void)
{
  int     i;

  strcpy(Current_Path,"/");
  for (i = 0 ; i < (MAX_AUXROM_SDFILES + 1) ; i++)
  {
    Auxrom_Files[i].close();
  }
}

void AUXROM_CLOCK(void)
{
#if VERBOSE_KEYWORDS
    Serial.printf("Call to CLOCK\n");
#endif

}

//
//  This function is called whenever AUXROM makes changes to FLAGS, a 4 byte area in the shared RAM window
//  We save the 4 bytes in file in the root directory of the SD Card named AUXROM_FLAGS.TXT
//  We restore from the file on Boot.
//
//  As of 11/20/2020 , the buffer used is 0. Uses SENDCMD -> SENDWAIT with R20 ==
//

void AUXROM_FLAGS(void)
{
  File      temp_file;
  char      flags_to_write[12];
  uint8_t   chars_written;

#if VERBOSE_KEYWORDS
    Serial.printf("Call to FLAGS\n");
#endif

  if (!(temp_file = SD.open("/AUXROM_FLAGS.TXT", O_RDWR | O_TRUNC | O_CREAT)))
  {
    post_custom_error_message("Can't open /AUXROM_FLAGS.TXT", 310);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("Can't open /AUXROM_FLAGS.TXT\n");
    return;
  }
  sprintf(flags_to_write, "%08lx\r\n", AUXROM_RAM_Window.as_struct.AR_FLAGS);
  chars_written = temp_file.write(flags_to_write, 10);
  temp_file.close();
  if (chars_written != 10)
  {
    post_custom_error_message("Can't write /AUXROM_FLAGS.TXT", 311);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("Can't write /AUXROM_FLAGS.TXT\n");
    return;
  }
  Serial.printf("Write /AUXROM_FLAGS.TXT success\n");
  *p_usage = 0;
  return;
}

#if 0
char HelpScreen[]="\
********************************\
*                              *\
*                              *\
*      Feed me, Seymour!       *\
*                              *\
* INITIALIZE newLabel,msus$,   *\
*        dirSize,interleave    *\
*                              *\
* newLabel: up to 6 chars      *\
* msus$: \":D700\" or            *\
*     or \".volume\"             *\
* dirSize: # directory sectors *\
*     default=14               *\
* interleave: 1-16 (default=1) *\
*                              *\
********************************";
#endif

//
//  Buffer 6:   Text of requested HELP$
//  Length 6:   Length of requested HELP$
//  Opt6[0]:    0 = Restore Screen state
//              1 = Save screen state
//

void AUXROM_HELP(void)
{

#if VERBOSE_KEYWORDS
    Serial.printf("Call to HELP\n");
#endif

  if(AUXROM_RAM_Window.as_struct.AR_Opts[0])
  {
    CRT_capture_screen();
    //Write_on_CRT_Alpha(0,0,HelpScreen);     // if you uncomment this, uncomment the above test data
  }
  else
  {
    CRT_restore_screen();
  }
  Serial.printf("HELP %d done.\n", AUXROM_RAM_Window.as_struct.AR_Opts[0]);
  *p_usage = 0;           //  Success
  *p_mailbox = 0;         //  Indicate we are done
  return;

}

//
//  Always uses Buffer 6 and associated parameters
//
//  If filespec$ has a trailing slash, it is specifying a subdirectory to be catalogged. Without a slash it is a
//  file selection filter specification, that may include wild cards * and ?
//  Pattern matching removes trailing / (from the directory listing entries) so that patterns will match both files and subdirectories
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
//    The 0 or 1 (4th parameter) is passed in AUXROM_RAM_Window.as_struct.AR_Opts[0]
//    The fileSpec$              is passed in AUXROM_RAM_Window.as_struct.AR_Buffer_6 (null terminated) and saved in sdcat_filespec
//
//  Returning results:
//    A$ is passed from EBTKS to AUXROM in Buf6 starting at byte 0
//    S  is passed from EBTKS to AUXROM in BLen6 (length of string in Buf6)
//    D$ is passed from EBTKS to AUXROM in Buf6 starting at byte 256    Fixed length of 16 bytes
//
//  returns AR_Usages[Mailbox_to_be_processed] = 0 for ok, 1 for finished (and 0 length buf6), 213 for any error
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
//  Test cases        AR_Opts[0]        AR_Opts[1]       Buffer 6       Notes
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
//  for second and successive calls, where AR_Opts 0..3 are used for passing back the file length, and
//  since successive calls set AR_Opts[0] to 1, it will be 1, but on entry, the rest of AR_Opts 1..3
//  will have the remnants of the previous file's length, and Buffer 6 will have the prior file name, since
//  the pattern specification is only provided on the first call
//
//  For pattern matching, alphabetic case is ignored.
//

//static int              SDCAT_line_count = 0;
static bool               SDCAT_First_seen = false;     //  keeping track of whether we have seen a call for a first line of a SD catalog
static int32_t            get_line_char_count;

void AUXROM_SDCAT(void)
{
  char        *c_ptr;
  uint32_t    temp_uint;
  char        file_size[12];
  File        temp_file;
  bool        match;
  char        temp_file_path[258];

#if VERBOSE_KEYWORDS
    Serial.printf("Call to SDCAT\n");
#endif


  //  ta ta bye bye   SD.cacheClear();      ###########  deleted because new SdFat lib

  //
  //  Show Parameters
  //
  //Serial.printf("\nSDCAT Start next entry\n");
  //Serial.printf("."); //  less noisy progress for SDCAT

  if (AUXROM_RAM_Window.as_struct.AR_Opts[0] == 0)  //  This is a First Call
  {
  //
  //  Filespec$ is captured here on the first call.
  //  If it has a trailing slash, then we are cataloging a subdirectory, and the assumed match pattern is "*"
  //     until Everett changes his mind and adds another parameter, which would be totally ok.
  //
  //  Need to deal with the following cases
  //    The filespec that may be prefixed with path info, is merged with the current path to get an absolute path
  //    If the result ends with a slash, then we are doing a full catalog of a directory
  //      and the merged path must not have any '?' or '*' in it, since that would be wildcarded directories
  //      the resultant filespec for matching is '*'
  //      Special case 1: The Resolved_Path is 1 character long, which means we are cataloging the root directory
  //                      We actually don't need to do anything special.
  //    If the result does not end in slash, then we have a path with a trailing pattern to be matched
  //      Search backward from the end of the Resolved_Path looking for the last '/'
  //        Everything after the slash is the pattern and may include '?' or '*'
  //        Everything before the slash is the path, must end with the just found slash, and must not contain any '?' or '*'
  //          Special case 2: After removing the pattern, the trailing slash is also the leading slash, i.e we are
  //                          pattern matching in the root directory. Should not be a problem, but just a heads up that
  //                          this is special in that there is only 1 slash.
  //
    if (!Resolve_Path(p_buffer))
    {
      //*p_usage    = 213;    //  Error
      post_custom_error_message("Can't resolve path", 330);
      //show_mailboxes_and_usage();
      *p_mailbox = 0;      //  Indicate we are done
      Serial.printf("SDCAT Error exit 1.  Error while resolving subdirectory name\n");
      return;
    }
    //Serial.printf("SDCAT call 0   Resolve_Path = [%s]\n", Resolved_Path);
    //
    //  Split Resolved_Path into the path and filename/pattern sections.
    //  Since Resolved_Path is an absolute path, we know it has a leading '/'
    //
    strcpy(SDCAT_path_part_of_Resolved_Path, Resolved_Path);
    c_ptr = strrchr(SDCAT_path_part_of_Resolved_Path, '/');                   //  Search backwards from the end of the string. Find the last '/'
    //Serial.printf("[%s]  %08x  %08x\n", SDCAT_path_part_of_Resolved_Path, SDCAT_path_part_of_Resolved_Path, c_ptr);
    strcpy(SDCAT_pattern_part_of_Resolved_Path, ++c_ptr);                     //  Copy what ever is after the last slash into the pattern
    *c_ptr = 0x00;                                                      //  Follow the last '/' with 0x00, thus trimming SDCAT_path_part_of_Resolved_Path to just the path.
    if (strlen(SDCAT_pattern_part_of_Resolved_Path) == 0)
    {   //  Apparently no pattern to match, so set it to "*"
      strcpy(SDCAT_pattern_part_of_Resolved_Path, "*");
    }
    str_tolower(SDCAT_pattern_part_of_Resolved_Path);                         //  Make it lower case, for case insensitive matching
#if VERBOSE_KEYWORDS
    Serial.printf("SDCAT call 0   Path Part is [%s]   Pattern part is [*]\n", SDCAT_path_part_of_Resolved_Path, SDCAT_pattern_part_of_Resolved_Path);
#endif
    //  At this ponint the path part is either a single '/' or a path with '/' at each end
    if (strchr(SDCAT_path_part_of_Resolved_Path, '*') || strchr(SDCAT_path_part_of_Resolved_Path, '?'))
    {   //  No wildcards allowed in path part
      post_custom_error_message("SDCAT no wildcards in path", 333);
      *p_mailbox = 0;      //  Indicate we are done
      return;
    }
    //
    //  Since this is a first call, initialize by reading the whole directory into a buffer (curently 64K)
    //  This conveniently uses the ls() member function.  Alternatively, I could investigate using the built in
    //  SdFat openNext() function.
    //
    //  See EBTKS_Function_and_Keyword_List_2020_09_03.xlsx   for list of functions,  or  G:/PlatformIO_Projects/Teensy_V4.1/T41_EBTKS_FW_1.0/.pio/libdeps/teensy41/SdFat/extras/html/class_fat_file.html
    //    For later reference, it seems the approach it to open the directory     THIS MAY BE WRONG
    //      File dir;
    //      File entry;
    //      if (!dir.open(Current_Path, O_RDONLY))  ....
    //    then do a rewind
    //      dir.rewind();
    //    then open files in that directory, and extract info about them
    //      entry.openNext(dir, oflag)      //  really unsure about this
    //
    //  Hmmmm:  go find this function written a while ago:  printDirectory()   in     EBTKS_SD.cpp
    //          I think the issue is that this works, but maybe there is info that can't be retrieved that the ls() does retrieve  ????
    //  Come back to this another day, and see if we can avoid the 64K buffer
    //
    // Serial.printf("Catalogging directory of [%s]\n", SDCAT_path_part_of_Resolved_Path);
    if (!LineAtATime_ls_Init_SDCAT(SDCAT_path_part_of_Resolved_Path))               //  This is where the whole directory is listed into a buffer
    {                                                                   //  Failed to do a listing of the current directory
      post_custom_error_message("Can't list directory", 331);
      *p_mailbox = 0;                                                   //  Indicate we are done
      Serial.printf("SDCAT Error exit 2.  Failed to do a listing of the directory [%s]\n", Resolved_Path);
      return;
    }
    SDCAT_First_seen = true;
    //Serial.printf("SDCAT Initial call, Pattern is [%s], directory is [%s]\n", sdcat_filespec, Resolved_Path);
  }

  if (!SDCAT_First_seen)
  {
    //
    //  Either we haven't done a first call, or a previous call reported we were at the end of the directory
    //
    post_custom_error_message("No SDCAT init or past end", 332);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("SDCAT Error exit 3.  Missing First call to SDCAT\n");
    return;
  }

  while(1)  //  keep looping till we run out of entries or we get a match between a directory entry and the match pattern
  {
    if (!LineAtATime_ls_Next_SDCAT())
    {   //  No more directory lines
      SDCAT_First_seen = false;                             //  Make sure the next call is a starting call
      *p_len   = 0;                                         //  nothing more to return
      *p_usage    = 1;                                      //  Success and END
      //Serial.printf("SDCAT We are done, no more entries\n");
      //show_mailboxes_and_usage();
      *p_mailbox = 0;                                       //  Indicate we are done
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
    temp_uint = strlcpy(p_buffer, &dir_line[28], 129);      //  Copy the filename to buffer 6, and get its length
    *p_len = temp_uint;                                     //  Put the filename length in the right place
    //  Make another copy for matching, which will be case insensitive
    strcpy(&p_buffer[256], p_buffer);
    str_tolower(&p_buffer[256]);
    //
    //  Not part of the spec, but I think I want to strip the trailing slash for the pattern match
    //
    if (p_buffer[temp_uint - 1] == '/')
    {   //  Entry is a subdirectory
      p_buffer[temp_uint - 1 + 256] = 0x00;                           //  Remove the '/' for pattern match
      *(uint32_t *)(AUXROM_RAM_Window.as_struct.AR_Opts + 4) = 1;     //  Record that the file is actually a directory

    }
    else
    {   //  Entry is a file name
      *(uint32_t *)(AUXROM_RAM_Window.as_struct.AR_Opts + 4) = 0;                 //  Record that the file is not a directory
    }
    match = MatchesPattern(&p_buffer[256], SDCAT_pattern_part_of_Resolved_Path);  //  See if we have a match

    if (!match)
    {
      continue;                                                                   //  See if the next entry is a match
    }
    //
    //  We have a match, and have alread updated these:
    //    Name        p_buffer starting at position 0
    //    Length      *p_len
    //    Dir Status  AUXROM_RAM_Window.as_struct.AR_Opts[4] (as uint32_t)
    //
    //  Now finish things
    //
    strlcpy(&p_buffer[256], dir_line, 17);                                                //  The DATE & TIME string is copied to Buffer 6, starting at position 256
    strlcpy(file_size, &dir_line[16], 12);                                                //  Get the size of the file. Still needs some processing
    temp_uint = atoi(file_size);                                                          //  Convert the file size to an int
    *(uint32_t *)(AUXROM_RAM_Window.as_struct.AR_Opts) = temp_uint;                       //  Return file size in A.BOPT60-63
    //
    //  Need to get read-only status
    //
    strcpy(temp_file_path, SDCAT_path_part_of_Resolved_Path);
    strcat(temp_file_path, p_buffer);
    //  Serial.printf("Read only test of %s\n", temp_file_path);
    temp_file = SD.open(temp_file_path);                                                  //  SD.open() does not mind the trailing slash if the name is a directory
    if (temp_file.isReadOnly())                                                           //  If this throws an error (no member named isReadOnly) see note at top of this file
    {
      *(uint32_t *)(AUXROM_RAM_Window.as_struct.AR_Opts + 4) |= 0x02;                     //  Indicate the file is read only
    }
    temp_file.close();
//    Serial.printf("Filename [%s]   Date/time [%s]   Size [%s] = %d  DIR&RO status %d\n\n",  p_buffer,
//                                                                                            &p_buffer[256],
//                                                                                            file_size, temp_uint,
//                                                                                            *(uint32_t *)(AUXROM_RAM_Window.as_struct.AR_Opts + 4)  );
    *p_usage  = 0;        //  Success
    //show_mailboxes_and_usage();
    *p_mailbox = 0;      //  Indicate we are done
    return;
  }
}

//
//  Update the Current Path with the provided path. Most of the hard work happens in Resolve_Path()
//  See its documentation
//
//  AUXROM puts the update path in Buffer 6, and the length in AR_Lengths[Mailbox_to_be_processed]
//  Mailbox 6 is used for the handshake
//

void AUXROM_SDCD(void)
{
  int         error_number = 0;
  char        error_message[33];

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDCD  Current Path before SDCD [%s] New Path will be [%s]\n", Current_Path, p_buffer);
  //show_mailboxes_and_usage();
#endif

  do
  {
    if (Resolve_Path((char *)p_buffer))       //  Returns true if no parsing problems
    {
      if ((file = SD.open(Resolved_Path)) == 0)
      {
        error_number = 340;
        strlcpy(error_message, "Unable to open directory", 32);
        break;                      //  Unable to open directory
      }
      if (!file.isDir())
      {
        file.close();
        error_number = 341;
        strlcpy(error_message, "Target path is not a directory", 32);
        break;                      //  Valid path, but not a directory
      }
      file.close();
      if (!SD.chdir(Resolved_Path))
      {
        error_number = 342;
        strlcpy(error_message, "Couldn't change directory", 32);
        break;                      //  Failed to change to new path
      }
      //
      //  Parsed ok, opened ok, is a directory, successfully changed directory, so update the Current path, and make it the current directory
      //
      strlcpy(Current_Path, Resolved_Path, MAX_SD_PATH_LENGTH + 1);
      if (!Resolved_Path_ends_with_slash)
      {
        strlcat(Current_Path,"/", MAX_SD_PATH_LENGTH + 1);
      }
      *p_usage = 0;                         //  Indicate Success

#if VERBOSE_KEYWORDS
      Serial.printf("SDCD: OK. Current Path is now [%s]\n", Current_Path);
      //show_mailboxes_and_usage();
      //Serial.printf("\nSetting Mailbox 6 to 0 and then returning\n");
#endif

      *p_mailbox = 0;                      //  Must always be the last thing we do
      return;
    }
    error_number = 330;
    strlcpy(error_message, "Can't resolve path", 32);
  } while(0);
  //
  //  This is the shared error exit
  //

#if VERBOSE_KEYWORDS
    Serial.printf("SDCD failed Message [%s]  error_number %d\n", error_message, error_number);
#endif

  post_custom_error_message(error_message, error_number);
  *p_mailbox = 0;      //  Indicate we are done
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

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDCLOSE\n");
#endif

  file_index = AUXROM_RAM_Window.as_struct.AR_Opts[0];               //  File number 1..10 , or 0 for all
  if (file_index == 0)
  {
    for (i = 1 ;  i < MAX_AUXROM_SDFILES ; i++)
    {
      if (Auxrom_Files[i].isOpen())
      {
        if(!Auxrom_Files[i].getName(filename,255))
        {
          Serial.printf("In SDCLOSE, couldn't retrieve filename\n");
        }
        return_status = Auxrom_Files[i].close();
        Serial.printf("Close file %2d [%s]  Success status is %s\n", i, filename, return_status ? "true":"false");
        //
        //  No error exit
        //
      }
    }
  }
  else
  {
    Auxrom_Files[file_index].getName(filename,255);
    return_status = Auxrom_Files[file_index].close();
    Serial.printf("Close file %2d [%s]  Success status is %s\n", file_index, filename, return_status ? "true":"false");
  }
  *p_usage    = 0;     //  File flush successfully
  *p_mailbox = 0;     //  Indicate we are done
  return;
}

//
//  Returns in the designated buffer the current path and its length (AUXROM_RAM_Window.as_struct.AR_Opts[0] == 0)     SDCUR$
//  Returns in the designated buffer "/"                             (AUXROM_RAM_Window.as_struct.AR_Opts[0] != 0)     SDHOME$
//

void AUXROM_SDCUR(void)
{

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDCUR\n");
#endif

  uint16_t    copy_length;
  if (AUXROM_RAM_Window.as_struct.AR_Opts[0] == 0)
  {
    copy_length = strlcpy(p_buffer, Current_Path, 256);
    *p_len = copy_length;
  }
  else
  {
    strlcpy(p_buffer, "/", 2);
    *p_len = 1;
  }
  //  delay(1000);
  *p_usage = 0;                        //  Indicate Success
  //  show_mailboxes_and_usage();
  *p_mailbox = 0;                      //  Release mailbox.    Must always be the last thing we do
}

//
//  SDDEL
//      If the resolved path ends in a "/" exit with an error that no file specified
//      Otherwise, use SDCAT like code to match files and delete them. Report an error if none are deleted.
//          Maybe we should have SDDEL pattern$ , A  with A being set to the number of files deleted
//

void AUXROM_SDDEL(void)
{
  int         number_of_deleted_files = 0;
  char        *c_ptr;
  uint32_t    temp_uint;
  bool        match;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDDEL\n");
#endif

  //  Serial.printf("SDDEL 1:  %s\n", p_buffer);
  if (!Resolve_Path(p_buffer))
  {   //  Error, Parsing problems with path
    post_custom_error_message("Can't resolve path", 330);
    *p_mailbox = 0;      //  Indicate we are done
    return;
  }
  //  Serial.printf("SDDEL 2:  %s\n", Resolved_Path);
  if (Resolved_Path_ends_with_slash)
  {
    post_custom_error_message("SDDEL no file specified", 370);
    *p_mailbox = 0;      //  Indicate we are done
    return;
  }
  //
  //  Use code very similar to SDCAT to delete files with potentially wildcards
  //
  if ((strchr(Resolved_Path, '*') == NULL) && (strchr(Resolved_Path, '?') == NULL))
  {   //  No wild cards, so we are just deleting 1 file, or failing
    //  Serial.printf("SDDEL 3:  No wildcards\n");
    if (!SD.remove(Resolved_Path))
    {
      post_custom_error_message("Couldn't delete file", 371);
      *p_mailbox = 0;      //  Indicate we are done
      return;
    }
    //
    //  Success, 1 file deleted
    //
    number_of_deleted_files++;
    goto SDDEL_Exit;
  }
  //
  //  Resolved_Path has wildcards
  //
  //  Serial.printf("SDDEL 4:  Wildcards in match pattern\n");
  SD.cacheClear();

  //
  //  Split Resolved_Path into the path and filename/template sections
  //  We have already rejected Resolved_Path that has trailing '/' and
  //  since Resolved_Path is an absolute path, we know it has a leading '/'
  //  Also, non-wildcard deletes have also been dealt with.
  //
  strcpy(SDDEL_path_part_of_Resolved_Path, Resolved_Path);
  c_ptr = strrchr(SDDEL_path_part_of_Resolved_Path, '/');             //  Find the last '/'
  //Serial.printf("[%s]  %08x  %08x\n", SDDEL_path_part_of_Resolved_Path, SDDEL_path_part_of_Resolved_Path, c_ptr);
  if (c_ptr == SDDEL_path_part_of_Resolved_Path)
  {   //  We have found the leading '/' , so the file(s) to be deleted are in root
    SDDEL_path_part_of_Resolved_Path[1] = 0x00;                       //  special case, leave the slash alone
    strcpy(SDDEL_pattern_part_of_Resolved_Path, &Resolved_Path[1]);
  }
  else
  {
    strcpy(SDDEL_pattern_part_of_Resolved_Path, ++c_ptr);
    *c_ptr = 0x00;                                              //  Follow the last '/' with 0x00, thus trimming SDDEL_path_part_of_Resolved_Path to just the path.
  }

  str_tolower(SDDEL_pattern_part_of_Resolved_Path);                   //  Make it lower case, for case insensitive matching

  //  At this ponint the path part is either a single '/' or a path with '/' at each end
  if (strchr(SDDEL_path_part_of_Resolved_Path, '*') || strchr(SDDEL_path_part_of_Resolved_Path, '?'))
  {   //  No wildcards allowed in path part
    post_custom_error_message("SDDEL no wildcards in path", 372);
      *p_mailbox = 0;      //  Indicate we are done
      return;
  }
  //  Serial.printf("SDDEL 5: path [%s]    pattern [%s]\n", SDDEL_path_part_of_Resolved_Path, SDDEL_pattern_part_of_Resolved_Path);
  //
  //  Create a directory listing of the specified path
  //
  if (!LineAtATime_ls_Init_SDDEL(SDDEL_path_part_of_Resolved_Path))   //  This is where the whole directory is listed into a buffer
  {                                                                   //  Failed to do a listing of the current directory
    post_custom_error_message("SDDEL problem with path", 373);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("SDDEL Error.  Failed to do a listing of the directory [%s]\n", SDDEL_path_part_of_Resolved_Path);
    return;
  }
  //
  //  Now loop through the directory listing doing pattern matches to see what to delete
  //
  while(1)  //  keep looping till we run out of entries or we get a match between a directory entry and the match pattern
  {
    if (!LineAtATime_ls_Next_SDDEL())
    {   //  No more directory lines
      goto SDDEL_Exit;
    }
    //
    //  See what we found. Throw away subdirectory entries. Since we no longer need Resolved_Path, use it as a temporary buffer
    //
    //  A directory entry looks like this:
    //    2020-09-07 16:24          0 pathtest/
    //    01234567890123456789012345678901234567890     Sub-directories are marked by a trailing slash, otherwise it is a file
    //
    temp_uint = strlcpy(Resolved_Path, &dir_line[28], 129);                   //  Copy the filename to Resolved_Path, and get its length
    if(Resolved_Path[temp_uint-1] == '/')
    {
      continue;                                                               //  This entry ends with a '/' (a subdirectory), so just move on
    }
    str_tolower(Resolved_Path);                                               //  Make it lower case, for case insensitive matching
    match = MatchesPattern(Resolved_Path, SDDEL_pattern_part_of_Resolved_Path);     //  See if we have a match

    //  Serial.printf("SDDEL 6: Matching %s   with   %20s   %s\n", SDDEL_pattern_part_of_Resolved_Path, Resolved_Path, match ? "true":"false");
    //
    //  If it matches, delete the file. We need another buffer, and the one pointed to by p_buffer is not in use anymore
    //
    if (match)
    {
      strcpy(p_buffer, SDDEL_path_part_of_Resolved_Path);     //  This already has a trailing '/'
      strcat(p_buffer, Resolved_Path);                  //  Remember that we are using Resolved_Path as a temp buffer at this point and it has the filename currently
      //  Serial.printf("SDDEL 7: Deleting %s\n", p_buffer);
      if (!SD.remove(p_buffer))
      {
        post_custom_error_message("Couldn't delete file", 371);
        *p_mailbox = 0;      //  Indicate we are done
        return;
      }
      number_of_deleted_files++;
    }
  }

SDDEL_Exit:
  //  Serial.printf("SDDEL 8: Deleted %d files\n", number_of_deleted_files);
  *p_usage = 0;           //  Success, file deleted
  *p_mailbox = 0;         //  Indicate we are done
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

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDFLUSH\n");
#endif

  file_index = AUXROM_RAM_Window.as_struct.AR_Opts[0];               //  File number 1..10 , or 0 for all
  if (file_index == 0)
  {
    for (i = 1 ;  i < MAX_AUXROM_SDFILES ; i++)
    {
      if (Auxrom_Files[i].isOpen())
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
  *p_usage   = 0;       //  File flush successfully
  *p_mailbox = 0;       //  Indicate we are done
  return;
}

//
//  Return the path for a specified msu$.  Return an empty string if there is no mounted virtual tape/disk
//
//  MSU$ is in buf

void AUXROM_MEDIA(void)
{
  char        *filename;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to MEDIA$\n");
#endif

  if (!parse_MSU(p_buffer))
  {
    post_custom_error_message("MEDIA$ MSU$ error", 510);
    *p_mailbox = 0;     //  Indicate we are done
    return;
  }

  //Serial.printf("MSU Parser Tape:%s  Disk:%s  SelectCode:%2d  Device:%1d  Drive:%1d\n",
  //                      msu_is_tape ? "true":"false", msu_is_disk  ? "true":"false", msu_select_code, msu_device_code, msu_drive_select);
  //Serial.flush();

  if (msu_is_tape)
  {
    filename = tape.getFile();
    if(filename)
    {
      strcpy(p_buffer, filename);        //  Copy the file path and name to the buffer that held msu$
      *p_len = strlen(p_buffer);
      *p_usage = 0;
      *p_mailbox = 0;     //  Indicate we are done
      return;
    }
  }

  //
  //  If not tape, must be disk
  //

  //
  //  Check that the requested HPIB Select code is the one we are emulating
  //
  if(get_Select_Code() != msu_select_code)
  {
    post_custom_error_message("MEDIA$ HPIB Select must match", 511);
    *p_mailbox = 0;     //  Indicate we are done
    return;
  }

  //
  //  Check that the Device number is a valid device
  //
  if (devices[msu_device_code])
  {
    filename = devices[msu_device_code]->getFilename(msu_drive_select);
    //Serial.printf("Disk mounted filename [%s]\n", filename);
    //Serial.flush();

    if(filename)
    {
      strcpy(p_buffer, filename);        //  Copy the file path and name to the buffer that held msu$
      *p_len = strlen(p_buffer);
      *p_usage = 0;
      *p_mailbox = 0;     //  Indicate we are done
      return;
    }
  }
  else
  {
    post_custom_error_message("Device code not supported", 512);
    *p_mailbox = 0; //  Indicate we are done
    return;
  }
  //
  //  If we get here, no mounted media, or msu_device_code is not valid
  //
  //Serial.printf("No mounted media or msu_device_code is not valid\n");
  //Serial.flush();
  //
  *p_buffer  = 0x00;                             //  Zero length string
  *p_len     = 0;                                //  Length is zero
  *p_usage   = 0;                                //  OK return
  *p_mailbox = 0;                                //  Indicate we are done
  return;
}

//
//  Create a new directory in the current directory
//  AUXROM puts the new directory name in Buffer 6, Null terminated
//  The length in AR_Lengths[Mailbox_to_be_processed]
//  Mailbox 6 is used for the handshake
//

void AUXROM_SDMKDIR(void)
{
  bool  mkdir_status;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDMKDIR\n");
#endif

  Serial.printf("New directory name   [%s]\n", p_buffer);
  //  show_mailboxes_and_usage();
  mkdir_status = SD.mkdir(p_buffer, true);    //  second parameter is to create parent directories if needed

  Serial.printf("mkdir return status [%s]\n", mkdir_status ? "true":"false");

  if (mkdir_status)
  {
    *p_usage = 0;             //  Indicate Success
  }
  else
  {
    *p_usage =  213;          //  Indicate Failure
  }
  *p_mailbox = 0;            //  Must always be the last thing we do
  return;
}

//
//  An msu (Mass Storage Unit) is a text string tape or disk drive
//
//  For tape, the only valid msu is ":T" , and this only exists on HP85A/B
//
//  For disks, the msu looks like this: ":D701"
//      :D        Identifies this as a disk msu
//      7         First number, can be 3 to 10. Note that although 10 is 2 digits, it is unambiguous because 1 is not a valid first number
//                This number is the select code for a HPIB module, typically HP-82937A. The default is 7
//      0         Second number is the device address, valid values are 0..7, this number is set by the DIP switches on the back of a disk drive chassis
//      1         Third number is the drive select, 0..3, and identifies a specific disk drive within the chassis specified by the second number
//
//  This function takes an msu and breaks it into a set of variables
//  It returns true if there are no parsing errors
//
//  RETURNS:
//    Either msu_is_disk or msu_is_tape is true
//    msu_select_code   is 3..10
//    msu_device_code   is 0..7
//    msu_drive_select  is 0..4
//
//
///////////////////////////////////  test code  ///////////////////////
//
//  test code to check parse_MSU().  450 msu codes tested and the results check by reading
//  the resultant printf() messages. PMF 10/17/2020
//
////// int     i,j,k;
////// char    dummy_msu[8];
////// bool    result;
//////
////// Serial.printf("\n\nTest all valid MSUs and some illegal ones too\n\n");
////// for (i = 2 ; i < 12 ; i++)
////// {
//////   for (j = 0 ; j < 9 ; j++)
//////   {
//////     for (k = 0 ; k < 5 ; k++)
//////     {
//////       sprintf(dummy_msu , ":D%1d%1d%1d", i,j,k);
//////       result = parse_MSU(dummy_msu);
//////       Serial.printf("%8s   func retn %s   disk %s  %2d %1d %1d\n", dummy_msu, result ? "true ":"false", msu_is_disk ? "true ":"false",
//////                   msu_select_code, msu_device_code, msu_drive_select);
//////     }
//////   }
////// }

bool parse_MSU(char *msu)
{
  char * msu_ptr = msu;

  msu_is_tape = msu_is_disk = false;
  if (strcasecmp(msu, ":T") == 0)
  {
    msu_is_tape = true;
    return true;
  }
  if (strncasecmp(msu, ":D", 2) != 0)
  {
    Serial.printf("parse_MSU fail exit 1 MSU is [%s]\n", msu);
    return false;
  }
  msu_ptr += 2;
  if (strncmp(msu_ptr, "10", 2) == 0)
  {
    msu_select_code = 10;
    msu_ptr += 2;
  }
  else
  {
    msu_select_code = *msu_ptr++ - '0';
    if ((msu_select_code < 3) || (msu_select_code > 9))
    {
      Serial.printf("parse_MSU fail exit 2\n");
      return false;
    }
  }
  msu_device_code = *msu_ptr++ - '0';
  if ((msu_device_code < 0) || (msu_device_code > 7))     //  msu_device_code is uint8_t, so it can't go negative, and will wrap to a large positive
  {                                                       //  leave the < 0 test for clarity.
    Serial.printf("parse_MSU fail exit 3\n");
    return false;
  }
  msu_drive_select = *msu_ptr - '0';
  if ((msu_drive_select < 0) || (msu_drive_select > 3))   //  msu_drive_select is uint8_t, so it can't go negative, and will wrap to a large positive
  {                                                       //  leave the < 0 test for clarity.
    Serial.printf("parse_MSU fail exit 4\n");
    return false;
  }
  msu_is_disk = true;
  return true;
}

//
//  MOUNT msus$, filePath$ [, modeFlag]
//  Mode:
//      0 mount file, error if not there (default if mode not specified)
//      1 create file if not there, mount file
//      2 error if file there, else create & mount file
//
//  Enforce that disk image file names end in .dsk
//
/////////////////////////////////////////////////////////////////
//
//  General FYI that is relevant MOUNT
//    devices is an array of pointers to HpibDisk class instances, think of a device as a drive
//        chassis that may have multiple drives. So this is an array of chassis. These are
//        allocated at system boot, and the number depends on the CONFIG.TXT file. The number
//        of entries in the array is not kept track of, but boot allocates starting at 0, and
//        realistically only a few would ever be allocated.
//        A new device is created by the following line
//            devices[device] = new HpibDisk(device);
//        See:
//            EBTKS_SD.cpp
//              bool loadConfiguration(const char *filename)
//            HpibDisk *devices[NUM_DEVICES];
//
//    A disk instance is created with the following line (during boot)
//        devices[device]->addDisk((DISK_TYPE)type);
//    The addDisk member function of Class HpibDisk.  See HpibDisk.h
//        It creates an instance of HPDisk.  HPDisk.h
//        so adddisk creates a new drive with this command during boot
//            _disks[_numDisks] = new HPDisk(type, &SD);
//
//    Another member function of Class HpibDisk is setFile, which associates a path on the SD Card
//        with the just created HPDisk instance, within the HpibDisk instance called like this
//        after creation with sddDisk
//        devices[device]->setFile(unitNum, fname, wprot);
//        setFile will close a previous file if one is open, it will open the new file for WRITE
//        (which includes read), and the filename is stored in the HPDisk instance.
//
//    The filename can be retrieved with
//        filename = devices[device]->getFilename(disknum)
//        see list_mount() for example useage
//
/////////////////////////////////////////////////////////////////
//
//  msu$      is in AR_Buffer_0
//            length is in AR_Lengths[0]  but we don't care. msu$ is zero terminated
//  path$     is in AR_Buffer_6
//            length is in AR_Lengths[6]  but we don't care. path$ is zero terminated
//  mode      is in AR_BUF0_OPTS[0]
//

//
//  Copy a file from one fully qualified path to another. Used here for creating new media files
//  and also in the SDCOPY Keyword function. The error returns have custom error messages for creating
//  a new blank disk. the caller must translate as needed (for new tape, or SDCOPY)
//

//
//  Read/Write block size statistics for copying a reference Floppy Disk image, which happens in MOUNT
//  when new media is created. Buffer memory is in DMAMEM
//  
//   Buf    #blocks   Min   Max     Ave      Std Dev   Total copy
//   Size             us    us      us       us        time, ms
//    512   528       405   19425   1157     121       1490.3
//    512   528       405    4905   1080      46       1480.0
//    512   528       405   15565   1100      64       1455.6
//    512   528       405   14625   1113      72       1468.1
//    512   528       405   19415   1142     109       1483.9
//                                          
//   8192    33       721   5250    3009    2239        149.415
//   8192    33       721   5250    2961    2272        139.964
//   8192    33       721   5245    2961    2275        147.209
//   8192    33       721   5236    2958    2278        139.964
//   8192    33       721   5248    2967    2281        147.84
//
//  32768     9      1744   6875    6176     322         79.856
//  32768     9      1743   6883    6186     329         79.981
//  32768     9      1731   6880    6178     326         79.982
//  32768     9      1741   6880    6179     325         79.890
//  32768     9      1743   6875    6180     326         79.891
//

#define COPY_BUFFER_SIZE          512

DMAMEM char Copy_buffer[COPY_BUFFER_SIZE];       //  Must be in DMAMEM (or maybe ITCM/DTCM, but definitely not EXTMEM)

bool copy_sd_file(const char * Source_Path, const char * Destination_Path)
{
  int chars_read, chars_written;

  Serial.printf("Copy file from [%s] to [%s]\n", Source_Path, Destination_Path);
  File Source = SD.open(Source_Path, FILE_READ);
  if (!Source)
  {
    Serial.printf("Couldn't open [%s] for Read\n", Source_Path);
    post_custom_error_message("Couldn't open Ref Disk", 417);
    return false;
  }
  File Destination = SD.open(Resolved_Path, FILE_WRITE);
  if (!Destination)
  {
    Serial.printf("Couldn't open New disk image for Write\n");
    Source.close();
    post_custom_error_message("Couldn't open New Disk", 418);
    return false;
  }
  //
  //  Copy the Source File to the Destination File
  //
  Serial.printf("Copying [%s] to [%s]\n", Source_Path, Destination_Path);

  while(1)
  {
    //SET_SCOPE_2;
    chars_read = Source.read(Copy_buffer, COPY_BUFFER_SIZE);
    //CLEAR_SCOPE_2;
    if (chars_read == COPY_BUFFER_SIZE)
    {
      Serial.printf(".");
    }
    else
    {
      Serial.printf("Read ref-file, rqst %d, got %d\n", COPY_BUFFER_SIZE, chars_read);
    }

    if(chars_read > 0)      //  make sure we don't try using the -1 we get on error
    {
      chars_written = Destination.write(Copy_buffer, chars_read);
      if(chars_read != chars_written)
      {
        Serial.printf("Couldn't copy file, writing wrote less than reading\n");
        post_custom_error_message("Couldn't init New Disk", 419);
        Source.close();
        Destination.close();
        Destination.remove();
        return false;
      }
    }
    else if (chars_read == 0)
    {
      Serial.printf("Got 0 characters, so copy finished\n");
      Source.close();
      Destination.close();
      break;  //  Out of while(1) loop
    }
    else      //  chars_read must be negative, which means an error
    {
      Serial.printf("\nsomehow chars_read == %d  so Copy failed\n", chars_read);
      Source.close();
      Destination.close();
      //  Destination.remove();     // leave it around for diagnostics

      int read_error_bits = Source.getError();
      Serial.printf("Error bits %08X\n", read_error_bits);
      Source.clearError();
      uint8_t sd_error = SD.sdErrorCode();
      Serial.printf("SD Error code %02X\n", sd_error);
      Serial.printf("\nCopy failed\n");
      return false;
    }
  }
  //
  //  The new disk image has been created and initialized as a blank disk
  //
  Serial.printf("\nCopy complete\n");
  return true;
}

//
//  MOUNT   msus$, filePath$ [, modeFlag]
//
//        filePath$ starts at p_buffer , msus$ starts at p_buffer+256 , both 0x00 terminated
//        modeFlag  is in AUXROM_RAM_Window.as_struct.AR_Opts[0]
//
//  Suggested Test Sequence.  Use ":D320" for mounting images
//
//    DIM O$[100]
//    M$ = ":D320"
//    T$ = "testdisk.dsk"
//
//    SDCD    /testfiles/
//    SDCAT                                     check we are in the right directory. If testdisk.dsk is there, delete it
//    SDDEL T$                                    if needed
//    O$ = MEDIA$(":D320")                      Save the current media so we can restore it
//    DISP O$                                   show the current mounted LIF disk
//    MOUNT M$,"///"&O$,0                       Should fail path resolution.
//    expect: Error 109 : Can't resolve path
//    AUXERRN
//    expect: 330
//    MOUNT "Z"&M$,O$,0                         Should fail msu$ can be parsed"
//    expect: Error 109 : MOUNT MSU$ error
//    AUXERRN
//    expect: 412
//    MOUNT ":T","newtape",0                    Should fail "MOUNT Filename must end in .tap"
//    expect: Error 109 : MOUNT Filename must end in .tap
//    AUXERRN
//    expect: 413
//    MOUNT M$,"newdisk",0                      Should fail "MOUNT Filename must end in .dsk"
//    expect: Error 109 : MOUNT Filename must end in .dsk
//    AUXERRN
//    expect: 416
//    MOUNT ":D720",T$,0                        Should fail "MOUNT HPIB Select must match"
//    expect: Error 109 : MOUNT HPIB Select must match
//    AUXERRN
//    expect: 414
//    MOUNT ":D373",T$,0                        Should fail "Device code not supported"
//    expect: Error 109 : Device code not supported
//    AUXERRN
//    expect: 512
//    MOUNT M$,T$,0                             Should fail mode 0 expects file to exist
//    expect: Error 109 : MOUNT file does not exist
//    AUXERRN
//    expect: 411
//    MOUNT M$,T$,1                             Should succeed

//    Not yet tested are errors 409 and 410 , and mode 2

void AUXROM_MOUNT(void)
{
//
//  Phase 1 Checks:
//                  Check that the path can be resolved         error 330
//                  Check that the msu$ can be parsed           error 412
//                  Check that the path ends in .tap            error 413   case insensitive
//                  Check that the path ends in .dsk            error 416   case insensitive
//                  Check that HPIB select code is correct      error 414   current default is 3
//                  Check that the device code is supported     error 512
//
//                      we could also check file length is 270336, but we don't
//
//  Phase 2 Mode 0
//                  Check that the file exists                  error 411
//                  Mode 0 Mount failed, could not be opened    error 415
//
//  11/4/2020       Change parameter passing
//                    No longer using AR_Buffer_6. Only AR_Buffer_0 and AR_Mailboxes[0], both of which have pointers setup by AUXROM_Poll()
//
//                    filename starts at AUXROM_RAM_Window.as_struct.AR_Buffer_0[0] and is zero terminated
//                                       AUXROM_RAM_Window.as_struct.AR_Lengths[0] is not used
//                    MSUS     starts at AUXROM_RAM_Window.as_struct.AR_Buffer_0[256] and is zero terminated
//                    mode     is in AR_Opts[0]
//

  //bool  SD_begin_OK;

  #if VERBOSE_KEYWORDS
  Serial.printf("Call to MOUNT\n");
  #endif

  *p_usage = 0;     //  Assume success

  //
  //  First check for special case of MOUNT "SDCard","anything"
  //
  //  Currently we dont check if the SDCard is already mounted. Also, there does nots eem to
  //  be a complementary function to SD.begin()  , i.e. a close/detach. Hopefully a new SD.begin()
  //  just over-writes any previous state
  //
  #if VERBOSE_KEYWORDS
  Serial.printf("p_buffer + 256 is [%s]\n", p_buffer + 256);
  #endif
  if (strcasecmp(p_buffer + 256, "SDCard") == 0)
  {
    Serial.printf("Mounting an SD Card\n");
    //
    //  This code is lifted and modified version of code in EBTKS.c
    //
    if (!SD.begin(SdioConfig(DMA_SDIO)))             //  This takes about ??? ms.          ###
    {
      Serial.printf("SD begin failed\nLogfile is not active\n");
      //SD_begin_OK = false;
      logfile_active = false;
      return;
    }
    #if VERBOSE_KEYWORDS
    Serial.printf("SD.begin ok\n");
    #endif
    logfile_active = open_logfile();
    #if VERBOSE_KEYWORDS
    Serial.printf("logfile_active is %s\n", logfile_active ? "true":"false");
    Serial.printf("calling remount\n");
    #endif
    if(!remount_drives(Config_filename))
    {
      Serial.printf("Failed to re-mount SD Card\n");
    }
    #if VERBOSE_KEYWORDS
    Serial.printf("MOUNT SDCard exit\n\n\n");
    #endif
    return;
  }

  if (!Resolve_Path(p_buffer))
  {
    post_custom_error_message("Can't resolve path", 330);
    Serial.printf("MOUNT failed:  Error while resolving subdirectory name 330\n");
    goto Mount_exit;
  }

  if (!parse_MSU(p_buffer + 256))
  {
    Serial.printf("Failing MSU [%s]\n", p_buffer + 256);
    post_custom_error_message("MOUNT MSU$ error", 412);
    goto Mount_exit;
  }

  //
  //  There are significant similarities but also differences between how MOUNT works for tapes and disks.
  //  So try and avoid duplication by sharing code where possible
  //
  //
  //  but first do some common checks that are independent of mode
  //
  if (msu_is_tape)
  {
    if (strcasecmp(".tap", &Resolved_Path[strlen(Resolved_Path) - 4]) != 0)
    {
      post_custom_error_message("MOUNT Filename must end in .tap", 413);
      goto Mount_exit;
    }
  }
  if (msu_is_disk)
  {
    if (strcasecmp(".dsk", &Resolved_Path[strlen(Resolved_Path) - 4]) != 0)
    {
      post_custom_error_message("MOUNT Filename must end in .dsk", 416);
      goto Mount_exit;
    }
    if(get_Select_Code() != msu_select_code)
    {
      post_custom_error_message("MOUNT HPIB Select must match", 414);
      goto Mount_exit;
    }
    if (!devices[msu_device_code])
    {
      post_custom_error_message("Device code not supported", 512);
      goto Mount_exit;
    }
  }

  switch (AUXROM_RAM_Window.as_struct.AR_Opts[0]) //  Switch on MODE
  {
    case 0:   //  Mount an existing file, Error if it does not exist
      if (!SD.exists(Resolved_Path))
      {
        post_custom_error_message("MOUNT file does not exist", 411);
        goto Mount_exit;
      }
      if(msu_is_tape)
      {
mount_a_tape:
        if(!tape_handle_MOUNT(Resolved_Path))
        {
          post_custom_error_message("MOUNT failed", 415);
          goto Mount_exit;
        }
      }
      else    //  must be a disk msu$
      {
mount_a_disk:
        if(!devices[msu_device_code]->setFile(msu_drive_select, Resolved_Path, false))
        {
          post_custom_error_message("MOUNT failed", 415);
          goto Mount_exit;
        }
      }
      //  Common success exit
      Serial.printf("MOUNT success %s  %s\n", p_buffer+256, p_buffer);
      goto Mount_exit;      //  Success exit
      break;


    case 1:   //  Mount an existing file, Create if it does not exist
      #if VERBOSE_KEYWORDS
      Serial.printf("MOUNT mode 1: Resolved_Path [%s]\n", Resolved_Path);
      #endif
      if (SD.exists(Resolved_Path))
      {
        #if VERBOSE_KEYWORDS
        Serial.printf("   Resolved_Path already exists\n");
        #endif
        if(msu_is_tape)
        {
          #if VERBOSE_KEYWORDS
          Serial.printf("   Mounting a tape\n");
          #endif
          goto mount_a_tape;
        }
        else
        { //    Must be a disk (checked in parse_MSU()  )
          #if VERBOSE_KEYWORDS
          Serial.printf("   Mounting a disk\n");
          #endif
          goto mount_a_disk;
        }
      }
      //
      //  The File does not exist, so need to create it.  (###  need to come back to this and look at the select:device:drive
      //                                                        to find out topology and copy the right image. Currently defaulting
      //                                                        to a floppy disk image ###)
      //
      //  Does not exist so create a new file by copying the reference image
      //
      #if VERBOSE_KEYWORDS
      Serial.printf("   Resolved_Path does not exists, need to create\n");
      #endif
      if(msu_is_tape)
      { //  Create and mount for tape
Mount_create_and_mount_tape:
        Serial.printf("   Create and mount a tape\n");
        if (!copy_sd_file("/Original_images/blank_T.tap", Resolved_Path))     //  If this fails, the error status and message has already been setup in create_tape_image()
        {                                                                     //  Possible errors are 426 , 427 , and 428
          if (AUXROM_RAM_Window.as_struct.AR_ERR_NUM == 417)
          {
            post_custom_error_message("Couldn't open Ref Tape", 426);
          }
          if (AUXROM_RAM_Window.as_struct.AR_ERR_NUM == 418)
          {
            post_custom_error_message("Couldn't open New Tape", 427);
          }
          if (AUXROM_RAM_Window.as_struct.AR_ERR_NUM == 419)
          {
            post_custom_error_message("Couldn't init New Tape", 428);
          }
          goto Mount_exit;
        }
        goto mount_a_tape;
      }
      else
      { //  Create and mount for disk
        #if VERBOSE_KEYWORDS
        Serial.printf("   Create and mount a disk\n");
        #endif
Mount_create_and_mount_disk:
        if (!copy_sd_file("/Original_images/blank_D.dsk", Resolved_Path)) //  If this fails, the error status and message has already been setup
        {
          Serial.printf("   Create (copy) failed\n");
          goto Mount_exit;
        }
        Serial.printf("   Create (copy) success\n");
        goto mount_a_disk;
      }
      break;


    case 2:   //  Mount new file, error if already exists, create & mount
      if (SD.exists(Resolved_Path))
      {     //  File exist which is an error in Mode 2
        #if VERBOSE_KEYWORDS
        post_custom_error_message("MOUNT File already exists", 409);
        #endif
        goto Mount_exit;
      }
      if(msu_is_tape)
      {
        goto Mount_create_and_mount_tape;
      }
      else
      {
        goto Mount_create_and_mount_disk;
      }
      break;


    default:  //  all others are an error. (this is probably caught by the AUXROMs)
      post_custom_error_message("Invalid MOUNT mode", 410);
      *p_mailbox = 0;             //  Must always be the last thing we do
      return;
      break;
  }


Mount_exit:
  *p_mailbox = 0;            //  Must always be the last thing we do
  return;
}

//
//  Open a file on the SD Card for direct access via the AUXROM functions
//
//  SDOPEN filePathName$, mode#, file#
//

void AUXROM_SDOPEN(void)
{
  int         file_index;
  bool        error_occured;
  int         error_number;
  char        error_message[33];

  file_index = AUXROM_RAM_Window.as_struct.AR_Opts[0];               //  File number 1..11

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDOPEN Name: [%s]  Mode %d  File # %d\n", p_buffer,
                                                            AUXROM_RAM_Window.as_struct.AR_Opts[1],
                                                            file_index);
#endif

  do
  {
    error_number = 420;
    strlcpy(error_message, "File is already open", 32);
    if (Auxrom_Files[file_index].isOpen())
      break; //  Error, File is already open
    error_number = 330;
    strlcpy(error_message, "Can't resolve path", 32);
    if (!Resolve_Path(p_buffer))
      break; //  Error, Parsing problems with path
    error_occured = false;
    switch (AUXROM_RAM_Window.as_struct.AR_Opts[1])
    {
      case 0:       //  Mode 0 (READ-ONLY), error if the file doesn't exist
      error_number = 422;
      strlcpy(error_message, "Open failed Mode 0", 32);
      if (!Auxrom_Files[file_index].open(Resolved_Path, O_RDONLY | O_BINARY))
        error_occured = true;
        break;
      case 1:       //  Mode 1 (R/W, append)
      error_number = 423;
      strlcpy(error_message, "Open failed Mode 1", 32);
      if (!Auxrom_Files[file_index].open(Resolved_Path, O_RDWR | O_APPEND | O_CREAT | O_BINARY))
        error_occured = true;
        break;
      case 2:       //  Mode 2 (R/W, truncate)
      error_number = 424;
      strlcpy(error_message, "Open failed Mode 2", 32);
        //if (!Auxrom_Files[file_index].open(Resolved_Path , O_RDWR | O_TRUNC | O_CREAT | O_BINARY)) error_occured = true;
      if (!Auxrom_Files[file_index].open(Resolved_Path, O_RDWR | O_TRUNC | O_CREAT))
        error_occured = true;
       break;
      default:
      error_number = 425;
      strlcpy(error_message, "Open failed, Illegal Mode", 32); // This should never happen because AUXROM checks Mode is 0,1,2
        error_occured = true;                                                              //  And yet, we have seen it due to a bug in AUXROM code
        break;
    }
    if (error_occured)
      break;
    *p_usage    = 0;     //  File opened successfully
    *p_mailbox = 0;     //  Indicate we are done

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
  *p_mailbox = 0;      //  Indicate we are done
  return;
}

//
//            These are the comments taken from the AUXROM source
//
//  SPRINTF dst$Var, format$ [,#-or-$-arg[,#-or-$-arg[...]]]
//  This gets parsed as:
//    SPRINTF
//    dst$Var reference
//    format$ expression
//    optional numeric or string expression for arg1
//    SPF#TOK or SPF$TOK
//    optional numeric or string expression for arg2
//    SPF#TOK or SPF$TOK
//    ...
//    optional numeric or string expression
//    SPF#TOK or SPF$TOK
//    SPFETOK (SPRINTF END token)
//  At runtime:
//    dst$Var reference goes on stack
//    format$ len/address goes on stack
//    At either the first argument or at SPFEND:
//        Pushes 1 byte of 2 on to A.BUF6 (2=string for format$)
//        Pushes 2 bytes of length of format$ to A.BUF6
//        Pushes x bytes of format$ to A.BUF6
//    (so that format$ is always the first argument in A.BUF6, followed by...)
//
//    For each STRING argument, another 2 (type=string), 2-byte len, and x-byte string
//        gets added to A.BUF6
//    For each NUMBER argument:
//        If REAL, a 0 and the 8-byte REAL gets pushed to A.BUF6
//        If TAGGED-INT, a 1 and the 3-byte INT gets pushed to A.BUF6
//    Repeat for each arg until SPFEND encountered.
//    SPFEND (SPRINTF END) pushes the format$ to A.BUF6 if NO other args (which would have already done it),
//    SPFEND also pushes a 377 'tag' on the end to indicate "end of arguments",
//    then passes the A.BUF6 (containing format$ and ALL args) to EBTKS.
//    EBTKS marches through A.BUF6, using the tags, formating format$ and the args
//    into a temporary buffer. Once done, it then copies the temp buf to A.BUF6 and
//    returns it, which SPFEND then stores into the dst$Var.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//            These are the comments taken from 85auxrom.txt
//
//  SPRINTF dst$Var, format$[, comma-separated-arg-list]
//    A reasonable facsimile of the C language/library sprintf() function.
//    'dst$Var' is the string into which the formatting output will be
//    stored. 'format$' is the string that specifies the formatting, which
//    will use the arguments in the comma-separated-arg-list to sequentially
//    fill formatting paramters in the 'format$'. The comma-separated-arg-list
//    entries can be numeric or string expressions, although the type MUST
//    match the ones specified in 'format$'.
//    
//    NOTE: The behavior of this code is VERY dependent upon the library used.
//    The behavior of SPRINTF may very possibly differ between EBTKS and the
//    Series 80 Emulator, due to different sprintf() library routines being
//    linked in for the different environments. Most of the more 'common'
//    sprintf() options should behave the same and be supported. However, some
//    of the less common ones may work differently OR not work at all. Beware.
//    
//    The format$ string will simply output any normal text included, but
//    whenever it sees a '%' character, that will start the formatting of
//    an argument from the arg-list (unless the '%' is immediately followed
//    by another '%', in which case the two are replaced by a single '%'
//    character in the output and no arg-list items are used). The general
//    format of the % formatting is:
//      %[flags][width][.precision]type
//    NOTE: the []'s in the above indicate OPTIONAL things, they are not included in your format$.
//    
//    [flags] can be any (or none) of the following:
//      -   left-align output rather than right-align output (the default)
//      +   prepends a plus for positive signed-numeric types (the default doesn't prepend
//          anything for positive values)
//      SPACE (space character, not the word SPACE) prepends a space for the sign of positive values
//      0   if [width] is specified, prepends zeros for numeric types instead of spaces
//      #   alternate forms:
//            for g and G types, trailing zeros are not removed
//            for f, F, e, E, g, G types, the output always contains a decimal point.
//            for o, x, X types, 0, 0x, 0X respectively is prepended to non-zero numbers.
//    [width] is a number that specifies the MINIMUM NUMBER of characters to output, used to pad
//            output of smaller numbers; no truncation, though, of numbers too large for the width specified.
//    
//    [.precision] is a number that specifies a MAXIMUM limit on the output, depending upon the 'type'.
//    
//    NOTE: Both [width] and [.precision] can either be a literal number included in the 'format$' OR
//    they can be the '*' character, in which case the '*' gets replaced by a number from the arg-list.
//    
//    'type' is a single letter indicating the desired formatting of the next item from the arg-list:
//      i or d  format the next argument (MUST be numeric) as a signed integer.
//      u   formats the next argument (MUST be numeric) as an unsigned integer.
//      f or F  formats the next argument (MUST be numeric) as a REAL in fixed-point notation.
//          The only difference is whether VERY large or VERY small numbers are output as
//          upper or lowercase INF, INFINITY, or NAN.
//      e or E  formats the next argument (MUST be numeric) in standard "[-]d.ddd e[+/-]ddd" form.
//          The only difference is the case of the 'e' or 'E' used for the exponent.
//      g or G  format the next argument (MUST be numeric) in either fixed-point or standard-exponential
//          format, whichever is more appropriate for the magnitude.
//      x or X  formats the next argument (MUST be numeric) as a hexidecimal value.
//      o   formats the next argument (MUST be numeric) as an octal value.
//      s   copies the next argument (MUST be string) into the output.
//      c   outputs a single character to the output. The argument may be the NUMERIC value of the
//          character, or the argument may be a STRING in which case the FIRST character is output.
//    
//    You can also include special characters in the output by placing
//    these character strings in the 'format$':
//      \\    outputs a single '\' character
//      \r    outputs a CR character
//      \n    outputs a LF character
//      \t    outputs a TAB character
//      \xHH  outputs a character who's value is specified by the two HH hexidecimal digits
//      \nnn  outputs a character who's value is specified by the three nnn octal digits
//    So, for example
//      SPRINTF A$, "\t\r\n"
//    would achieve the exact same thing as
//      SPRINTF A$, "%c%c%c",9,13,10
//    You could also achieve the same thing by:
//      SDEOL 1
//      SPRINTF A$, "\t%s", SDEOL$
//    
//    Errors: SD ERROR (213D) formatted output length > 1024 characters
//        BAD FORMAT (219D) something wrong with format$
//        FORMAT/ARG MISMATCH (220D) argument list doesn't line up with format$
//  
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//              These are Philip's comments
//
//  Copied code from the Series 80 Emulator and then modified it for the EBTKS environment
//

#define SCRATCHLENGTH             (1024)


//  Real Arg
//      Tag = 0
//      8 bytes of HP85 format real
//
#define FORMAT_ARG_REAL           (0)
//
//  Integer Arg
//      Tag = 1
//      3 bytes of HP85 format tagged integer, without the 0xFF byte (yet) or the 4 ignored bytes
//                                              5 decimal digits plus sign
//
#define FORMAT_ARG_INTEGER        (1)
//
//  String Arg
//      Tag = 2
//      2 bytes of length
//      n bytes of string, NOT 0 terminated
//
#define FORMAT_ARG_STRING         (2)
//
//  End of argument list
//
#define FORMAT_ARG_END            (0xFF)

DMAMEM  char    sprintf_result[SCRATCHLENGTH];
DMAMEM  char    format_segment[100];                  //  This will hold a segment of the main format string, starting with a '%'
DMAMEM  char    string_arg[SCRATCHLENGTH];

void AUXROM_SPF(void)
{
  char      *format_ptr;
  int       format_remaining;       //  t       in Everett's emulator
  char      *arg_list_ptr;
  int       next_store_index;       //  j       in Everett's emulator
  double    numeric;                //  narg
  char      *pCopy;
  int       string_length;          //  slen    in Everett's emulator
  char      next_output_char;       //  sb      in Everett's emulator

    //  blen          len of stuff in buf
    //  buf           sprintf information:
    //    BYTE  2     string type
    //    WORD        strlen    len of FORMAT$  (16 bits)
    //    BYTE        [strlen]  bytes of FORMAT$
    //  The FORMAT$ in buf is then optionally followed by numeric and string arguments until a TYPE of 0xFF is seen.
    //  The TYPEs are:
    //    BYTE  0     REAL type
    //    BYTE        8 bytes of REAL# value
    //
    //    BYTE  1     INT type
    //    BYTE        3 bytes of INT# value
    //
    //    BYTE  2     string type
    //    WORD        strlen    len of string  (16 bits)
    //    BYTE        [strlen]  bytes of string value
    //
    //    BYTE  0xFF    end of argument list

    format_ptr = p_buffer;
    sprintf_result[0] = 0;

    if (*format_ptr++ != FORMAT_ARG_STRING)
    {
      goto badformat;                                   //  No FORMAT$ (should never happen, should be caught by AUXROM,
                                                        //  but let's be safe, in case an IDIOT wrote the AUXROM)
    }
    format_remaining = *(uint16_t *)format_ptr;         //  Get length of format$
    format_ptr += 2;                                    //  Skip len of format$

#if VERBOSE_KEYWORDS | VERBOSE_SPRINTF
    Serial.printf("Call to SPRINTF  Format string length %d  Format string [%.*s]\n", format_remaining, format_remaining, format_ptr);
#endif

    arg_list_ptr = format_ptr + format_remaining;       //  Skip format$ to point to first arg

    next_store_index = 0;                               //  How many chars written to sprintf_result (limit of SCRATCHLENGTH)
    //
    //  format_ptr                    points to current char in format$
    //  format_remaining              remaining chars in format$ (including the current one). Remember: format$ is NOT NUL-terminated!
    //  arg_list_ptr                  points to the next arg's TYPE byte in buf
    //  next_store_index              index into sprintf_result of where to store next formatted output
    //

    while ((next_store_index < SCRATCHLENGTH) && format_remaining )
    {   //  Process format$ until buffer is full or run out of format$
      int q;

      #if VERBOSE_SPRINTF
      Serial.printf("Format [%.*s] StorIndex %d ScratchBuf [%s]\n", format_remaining, format_ptr, next_store_index, sprintf_result);
      #endif

      if (*format_ptr != '%' || ((format_remaining > 1) && (*(format_ptr+1) == '%')) )
      {                                           //  Format is not a conversion
        if( *format_ptr=='%' )
        {                                         //  Handle %%
          format_ptr++;                                 //  Skip over the first one
          --format_remaining;
          next_output_char = *format_ptr++;             //  Skip first %, output the second
          --format_remaining;
        }
        else if (*format_ptr == '\\')
        {
          format_ptr++;                                 //  Skip the '\'
          --format_remaining;
          if     (*format_ptr == '\\') {next_output_char =    *format_ptr++; --format_remaining;} // skipped the first '/', output the second '/'
          else if(*format_ptr == 'r' ) {next_output_char = 13; format_ptr++; --format_remaining;}
          else if(*format_ptr == 'n' ) {next_output_char = 10; format_ptr++; --format_remaining;}
          else if(*format_ptr == 't' ) {next_output_char =  9; format_ptr++; --format_remaining;}
          else if(*format_ptr == 'x' )
          {                                 //  Handle \xHH two hex digit char
            format_ptr++;                         //  Skip x
            --format_remaining;
            if (!isxdigit(*format_ptr) || !isxdigit(*(format_ptr+1)) )
            {
              goto badformat;
            }
            //
            //  Convert 2 Hex Characters (A..F0..9) to an 8 bit character       #####  Emulator does not handle lower case for a..f  ####
            //
            //  First character, convert lower case a..f to upper case, in the format string
            //
            if (*format_ptr >= 'a')   //  This is sufficient, since we already know it is a Hexadecimal character
            {
              *format_ptr -= 0x20;    //  convert a..f to A..F
            }
            //  Calculate high nibble
            next_output_char = (*format_ptr - '0' - ((*format_ptr > '9') ? 7 : 0 )) << 4;
            format_ptr++;
            --format_remaining;
            //
            //  Second character, convert lower case a..f to upper case, in the format string
            //
            if (*format_ptr >= 'a')   //  This is sufficient, since we already know it is a Hexadecimal character
            {
              *format_ptr -= 0x20;    //  convert a..f to A..F
            }
            next_output_char += (*format_ptr - '0' - ((*format_ptr > '9') ? 7 : 0 ));
            format_ptr++;
            --format_remaining;
          }
          else if (isdigit(*format_ptr))
          {   //  Handle "\nnn" octal number char.  consume no more than 3 octal digits        ######  does not catch 8 and 9 and throw an error
            for (next_output_char = q = 0 ; (q < 3) && isdigit(*format_ptr); q++, format_remaining--, format_ptr++)
            {
              next_output_char = (next_output_char << 3) + *format_ptr - '0';     //  Add in the digit
            }
          }
          else
          {
            goto badformat;
          }
        }
        else
        {
          next_output_char = *format_ptr++;
          --format_remaining;
        }
        sprintf_result[next_store_index++] = next_output_char;
        sprintf_result[next_store_index] = 0;                      //  For diagnostics, we make the result string zero terminated
      }
      else
      {                 //  Handle %  , Format is a conversion
        pCopy = format_segment;
        //
        //  Copy '%' and any FLAGS
        //
        do
        {
          *pCopy++ = *format_ptr++;
          *pCopy   = 0;
        } while (--format_remaining && ((*format_ptr == '-' ) ||
                                        (*format_ptr == '+' ) ||
                                        (*format_ptr == ' ' ) ||
                                        (*format_ptr == '0' ) ||
                                        (*format_ptr == '\'') ||
                                        (*format_ptr == '#' )   ) );
        //  Copy WIDTH field
        if (format_remaining && (*format_ptr == '*') && (*arg_list_ptr != FORMAT_ARG_END) )
        {   //  If '*' and there's args left on the list
            //  Substitute a value from the ARG LIST
subarg:
          format_ptr++;                                         //  Skip '*'
          --format_remaining;                             //  Decrement remaining char count
          if (*arg_list_ptr == FORMAT_ARG_REAL)
          {
            numeric = cvt_HP85_real_to_IEEE_double((uint8_t *)(arg_list_ptr+1));  //  Get REAL arg in the usable format. This is for an '*' flag
            arg_list_ptr += 9;                                      //  Skip tag and 8-byte value
          }
          else if (*arg_list_ptr == FORMAT_ARG_INTEGER)
          {
            *arg_list_ptr = 0xFF;                                   //  Change tag to tagged-INT tag
            numeric = cvt_HP85_real_to_IEEE_double((uint8_t *)(arg_list_ptr-4));  //  Get tagged-INT arg in the usable format       //  ######  need to check if this works  #####
            arg_list_ptr += 4;                                      //  Skip tag and 3-byte value
          }
          else
          {
argmismatch:
            *p_usage = 220;                                 //  FORMAT/ARG MISMATCH We found an '*' flag, but no matching numeric parameter.
            *p_mailbox = 0;                                 //  Indicate we are done
            Serial.printf("SPRINTF FORMAT/ARG MISMATCH\n");
            return;
          }
          sprintf((char*)pCopy, "%d", (int)numeric);        //  Ignore any fractional part of the number, and just use the Int part
          pCopy += strlen((char*)pCopy);                    //  Advance pCopy by however many chars we output to the temp format string
          // I think this could be written: pCopy += sprintf((char*)pCopy, "%d", (int)numeric);   //  #####  slightly more efficient
        }
        else
        {                                                   //  Not a '*', check for digits. Copy any WIDTH number that's there
          while (format_remaining && isdigit(*format_ptr))
          {
            *pCopy++ = *format_ptr++;
            --format_remaining;
          }
          *pCopy   = 0;
        }
        //  Copy possible WIDTH/PRECISION separating '.'
        if (format_remaining && *format_ptr=='.')
        {
          *pCopy++ = *format_ptr++;
          --format_remaining;
        }
        *pCopy   = 0;
        //  Copy PRECISION field
        if (format_remaining && *format_ptr=='*')
        {                                                   //  If the user does something stupid like "%*.***d", they'll go to subarg multiple times and get what they deserve
          //  Substitute a value from the ARG LIST
          goto subarg;
        }
        else
        {                                                   //  Copy any PRECISION number that's there
          while (format_remaining && isdigit(*format_ptr))
          {
            *pCopy++ = *format_ptr++;
            --format_remaining;
          }
          *pCopy   = 0;
        }
        //
        //  NOTE: LENGTH field NOT supported
        //
        //  We have now processed any Flags, '*', WIDTH and PRECISION
        //
        //  Get TYPE field
        //                                  ######  the next few lines are different from the Emulator. 1 indent level removed
        //                                          One non-existent else  { section } now not needed
        if (!format_remaining)
        {
          //
          //  We saw a %, and maybe Flags, '*', WIDTH and PRECISION, but now no field type. Oh so sad
          //
          goto badformat;
        }
        *pCopy++ = *format_ptr;                                 //  Copy the TYPE char
        format_remaining--;
        *pCopy = 0;                                       //  NUL-term the copied format$ piece
        switch (*format_ptr++)
        {               //  First, all the integer numeric formats
          case 'd':
          case 'D':     //  Not supported in real C library, but so much of HP85 is in upper case. Help the user
          case 'i':
          case 'I':     //  Not supported in real C library, but so much of HP85 is in upper case. Help the user
          case 'u':
          case 'U':     //  Not supported in real C library, but so much of HP85 is in upper case. Help the user
          case 'x':
          case 'X':     //  This is supported, affects the case of a..f in Hex numbers
          case 'o':
          case 'O':     //  Not supported in real C library, but so much of HP85 is in upper case. Help the user
              //  Get next (numeric!) arg
              if (*arg_list_ptr == FORMAT_ARG_REAL)
              {
                numeric = cvt_HP85_real_to_IEEE_double((uint8_t *)(arg_list_ptr+1));  //  Get REAL arg in the usable format
                arg_list_ptr += 9;                                                    //  Skip tag and 8-byte value
              }
              else if (*arg_list_ptr == FORMAT_ARG_INTEGER)
              {
                *arg_list_ptr = 0xFF;                                                 //  Change tag to tagged-INT tag
                numeric = cvt_HP85_real_to_IEEE_double((uint8_t *)(arg_list_ptr-4));  //  Get tagged-INT arg in the usable format
                arg_list_ptr += 4;                                                    //  Skip tag and 3-byte value
              }
              else
              {
                goto argmismatch;
              }
              sprintf((char*)sprintf_result + next_store_index, (char*)format_segment, (long)numeric);
              break;
                          //  Now, the floating point/real formats
          case 'f':
          case 'F':
          case 'e':
          case 'E':
          case 'g':
          case 'G':
            //  Get next (numeric!) arg
            if (*arg_list_ptr == FORMAT_ARG_REAL)
            {
              numeric = cvt_HP85_real_to_IEEE_double((uint8_t *)(arg_list_ptr+1));    //  Get REAL arg in the usable format
              arg_list_ptr += 9;                                                      //  Skip tag and 8-byte value
            }
            else if (*arg_list_ptr == FORMAT_ARG_INTEGER)
            {
              *arg_list_ptr = 0xFF;                                                   //  Change tag to tagged-INT tag
              numeric = cvt_HP85_real_to_IEEE_double((uint8_t *)(arg_list_ptr-4));    //  Get tagged-INT arg in the usable format
              arg_list_ptr += 4;                                                      //  Skip tag and 3-byte value
            }
            else
            {
              goto argmismatch;
            }
            sprintf((char*)sprintf_result + next_store_index, (char*)format_segment, numeric);
            break;
          case 's':
            //  Get the next (string) arg
            if (*arg_list_ptr != FORMAT_ARG_STRING)
            {
              goto argmismatch;                                                   //  Fail if next arg is not STRING!
            }
            arg_list_ptr++;                                                       //  Skip to len
            string_length = *(uint16_t *)arg_list_ptr;                            //  Get string length
            arg_list_ptr += 2;                                                    //  Skip len
            if ((string_length + next_store_index) > SCRATCHLENGTH)
            {
              goto badformat;                                                     //  Actually, string too long   ######
            }

            strlcpy(string_arg, arg_list_ptr, string_length+1);                           //  Copy string
            arg_list_ptr += string_length;                                                //  Skip string arg
            sprintf((char*)(sprintf_result + next_store_index),
                    (char*)(format_segment),
                    (char*)(string_arg)      );                                   //  Copy the string to the output according to the %s formatting
            break;
          case 'c':
            //  Get next numeric arg and make it a char
            //  or get the next string arg and use the first char
            if (*arg_list_ptr == FORMAT_ARG_REAL)
            {
              numeric = cvt_HP85_real_to_IEEE_double((uint8_t *)(arg_list_ptr+1));    //  Get REAL arg in the usable format
              arg_list_ptr += 9;                                                      //  Skip tag and 8-byte value
            }
            else if (*arg_list_ptr == FORMAT_ARG_INTEGER)
            {
              *arg_list_ptr = 0xFF;                                                   //  Change tag to tagged-INT tag
              numeric = cvt_HP85_real_to_IEEE_double((uint8_t *)(arg_list_ptr-4));    //  Get tagged-INT arg in the usable format
              arg_list_ptr += 4;                                                      //  Skip tag and 3-byte value
            }
            else if (*arg_list_ptr == FORMAT_ARG_STRING)
            {
              ++arg_list_ptr;                                                         //  Point to len
              string_length = *(uint16_t *)arg_list_ptr;                              //  Get string length
              if (string_length == 0)
              {
                goto badformat;
              }
              arg_list_ptr += 2;
              numeric = (double)(long)(*arg_list_ptr);                                //  Get first char of string as numeric value
              arg_list_ptr += string_length;                                          //  Skip string to next arg
            }
            else
            {
              goto badformat;
            }
            sprintf_result[next_store_index++] = (char)(long)numeric;
            sprintf_result[next_store_index] = 0;                              //  So advance of next_store_index via strlen() below works
            break;
          case 'p':
          case 'a':
          case 'A':
          case 'n':
          default:
badformat:
            *p_len = 0;
            *p_usage = 219;                                 //  BAD FORMAT
            *p_mailbox = 0;                                 //  Indicate we are done
            Serial.printf("SPRINTF BAD FORMAT\n");
            return;
        }
        next_store_index += strlen((char*)(sprintf_result + next_store_index));
      }             //  end of conversion processing
    }               //  end of while loop
    strncpy(p_buffer, sprintf_result, next_store_index);     //  Copy formatted output to buf
    *p_len =  next_store_index;                             //  Set returned length
    *p_usage = 0;                                           //  Good exit
    *p_mailbox = 0;                                         //  Indicate we are done
}


//
//  Read specified number of bytes from an open file, and store in the specified buffer which should always be buffer 6
//
//  Position after read is next character to be read
//

void AUXROM_SDREAD(void)
{
  int         file_index;
  int         bytes_to_read;
  int         bytes_actually_read;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDREAD\n");
#endif

  file_index = AUXROM_RAM_Window.as_struct.AR_Opts[0];               //  File number 1..11
  bytes_to_read = *p_len;                                            //  Length of read
  if (!Auxrom_Files[file_index].isOpen())
  {
    post_custom_error_message("SDREAD File not open", 440);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("SDREAD Error. File not open. File Number %d\n", file_index);
    return;
  }
  bytes_actually_read = Auxrom_Files[file_index].read(p_buffer, bytes_to_read);
  //  Serial.printf("Read file # %2d , requested %d bytes, got %d\n", file_index, bytes_to_read, bytes_actually_read);
  //
  //  Assume all is good
  //
  *p_len = bytes_actually_read;
  *p_usage    = 0;                                                        //  SDREAD successful
  *p_mailbox = 0;                                                         //  Indicate we are done
  return;
}

//
//  Rename old [path1/]filename1  to  new [path2/]filename2
//
//  Old [path1/]filename1 starts at p_buffer and is 0x00 terminated
//  New [path2/]filename2 starts at p_buffer+256 and is 0x00 terminated
//
//  Can rename a directory, including moving it to a different place in the directory hierarchy
//
//
//  Suggested Test Sequence.
//
//  sdcd "/testfiles/deltest/roms2/"
//  sdcur$ check we got there
//  sdcat
//  sdren "rom360", "rom360_renamed"
//  sdcat
//  sdren "rom360_renamed", "rom360"
//  sdcat
//

//  ##############  Unresolved Bug  ##############
//
//  Occasionally while running the SDREN test program, it would fail
//  with the following (or equiv)
//
//    Error 113 on line 220 : SD ERROR
//
//  but examining directories shows that the SDREN completed OK
//  and AUXROM_SDREN() does not have any generic Error exits, they
//  are all Custom Error messages.
//  Suspicion fell on SDEXISTS(...) which precedes it. But it is all
//  in AUXROM, and uses SDCAT. SDCAT has been used extensively and
//  has not shown any sign of error.
//  The error has occured on SDREN keywords on other lines in the
//  program, as well as line 220
//
//  I have also seen the test program reporting an error where a
//  call to SDEXISTS() indicates a file is "not present"/"present",
//  yet it is either "not present"/"present". I.e. the expected result
//  of a SDREN has occured, but the confirmation test with SDEXISTS
//  indicates failure. Looking at the directory with SDCAT after
//  the failure message shows the SDREN was successful.
//
//  Maybe we need a flush operation to make sure the directory is
//  updated before we proceed. Maybe that should be built into SDREN???
//  There is no SD.flush(), but there is SD.cacheClear()
//  that has been added. Now the test program has cycled 2000 times
//  without an error. Time to stop before wearing out the SD Card.
//  Not conclusive, but pretty good for now.
//  Removing this fix brout back the errors in typically less than
//  20 cycles
//
//  11/15/2020  Can't reproduce errors that were occuring yesterday.
//              If it happens again, suggest LA Setup for 076662 + Read
//              Instruction is "stmd r43,=TEMP22"
//

void AUXROM_SDREN(void)
{
  bool        rename_status;

  #if VERBOSE_KEYWORDS
    Serial.printf("Call to SDREN\n");
  #endif

  if(!Resolve_Path(p_buffer))
  {
    Serial.printf("SDREN error return 450\n");
    post_custom_error_message("Can't resolve Old path", 450);
    Serial.printf("SDREN can't resolve Old path  [%s]\n", p_buffer);
    goto SDREN_Error_exit;
  }
  if (!SD.exists(Resolved_Path))
  {
    Serial.printf("SDREN Old file doesn't exist 454\n");
    post_custom_error_message("SDREN Old file doesn't exist", 454);
    Serial.printf("SDREN Old File path  [%s]\n", p_buffer);
    goto SDREN_Error_exit;
  }

  strlcpy(Resolved_Old_Path, Resolved_Path, MAX_SD_PATH_LENGTH+1);      //  Save path to old file, as we need Resolve_Path() to process new path
  if(!Resolve_Path(p_buffer+256))
  {
    Serial.printf("SDREN error return 451\n");
    post_custom_error_message("Can't resolve New path", 451);
    Serial.printf("SDREN can't resolve New path  [%s]\n", p_buffer+256);
    goto SDREN_Error_exit;
  }

  rename_status = SD.rename(Resolved_Old_Path, Resolved_Path);

  if(!rename_status)
  {
    Serial.printf("SDREN error return 452\n");
    post_custom_error_message("SDREN rename failed", 452);
    Serial.printf("SDREN rename failed  Resolved paths are old [%s]   new [%s]\n", Resolved_Old_Path, Resolved_Path);
    goto SDREN_Error_exit;
  }
  //
  //  Success exit
  //
  //SD.cacheClear();                      // Docs say:  Not for normal apps.
  if (!SD.exists(Resolved_Path))
  {
    Serial.printf("SDREN success verification failure\n");
    Serial.printf("SDREN Mystery bug 453\n");
    post_custom_error_message("SDREN Mystery bug", 453);
    goto SDREN_Error_exit;
  }
  //
  //  Fall into normal successful exit
  //
  *p_usage = 0;                         //  Indicate Success
SDREN_Error_exit:
  *p_mailbox = 0;                       //  Must always be the last thing we do
  return;
}

//
//  Delete a directory but only if it is empty
//  AUXROM puts the directory to be deleted in Buffer 6, Null terminated , apply Resolve_Path()
//  The length in AR_Lengths[Mailbox_to_be_processed]
//  Mailbox 6 is used for the handshake
//

void AUXROM_SDRMDIR(void)
{
  bool  rmdir_status;
  File  dirfile;
  File  file;
  int   file_count;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDRMDIR\n");
#endif

  if (!Resolve_Path(p_buffer))
  {
    post_custom_error_message("Can't resolve path", 330);
    *p_mailbox = 0;                      //  Indicate we are done
    Serial.printf("SDRMDIR Error exit 1.  Error while resolving subdirectory name\n");
    return;
  }
  Serial.printf("Deleting directory [%s]\n", Resolved_Path);
  if ((dirfile.open(Resolved_Path)) == 0)
  {
    Serial.printf("SDRMDIR: Failed SD.open return value %08X\n", (uint32_t)file);
    post_custom_error_message("Can't open subdirectory", 311);
    *p_mailbox = 0;                      //  Indicate we are done
    return;
  }
  if (!dirfile.isDir())
  {
    Serial.printf("SDRMDIR: Failed file.isDir  Path is not a directory\n");
    post_custom_error_message("Path is not a directory", 312);
    dirfile.close();
    *p_mailbox = 0;                      //  Indicate we are done
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
  if (file_count != 0)
  {
    Serial.printf("SDRMDIR: Directory is not empty. found %d files\n", file_count);
    post_custom_error_message("Directory not empty", 313);
    dirfile.close();
    *p_mailbox = 0;                       //  Indicate we are done
    return;
  }
  //
  //  Specified path is a directory that is empty, so ok to delete
  //
  Serial.printf("SDRMDIR: Deleting directory %s\n", Resolved_Path);
  rmdir_status = dirfile.rmdir();
  Serial.printf("SDRMDIR: Delete status is %s\n", rmdir_status? "true":"false");
  if (rmdir_status)
  {
    *p_usage = 0;                         //  Indicate Success
    *p_mailbox = 0;                       //  Must always be the last thing we do
    return;
  }
  else
  {
    Serial.printf("SDRMDIR: Delete directory failed\n");
    post_custom_error_message("Directory delete failed", 314);
    dirfile.close();
    *p_mailbox = 0;                       //  Indicate we are done
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

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDSEEK\n");
#endif

  file_index = AUXROM_RAM_Window.as_struct.AR_Opts[0];                      //  File number 1..11
  seek_mode  = AUXROM_RAM_Window.as_struct.AR_Opts[1];                      //  0=absolute position, 1=advance from current position, 2=go to end of file
  offset     = *(uint32_t *)(AUXROM_RAM_Window.as_struct.AR_Opts + 4);      //  Fetch the file offset, or absolute position
  //  Serial.printf("\nSDSEEK Mode %d   Offset %d\n", seek_mode, offset);
  //
  //  check the file is open
  //
  if (!Auxrom_Files[file_index].isOpen())
  {
    post_custom_error_message("Seek on file that isn't open", 470);
    *p_mailbox = 0;                      //  Indicate we are done
    //  Serial.printf("SDSEEK Error exit 470.  Seek on file that isn't open\n");
    return;
  }
  //
  //  Get current position
  //
  current_position = Auxrom_Files[file_index].curPosition();
  //  Serial.printf("SDSEEK: Position prior to seek %d\n", current_position);
  //
  //  Get end position
  //
  Auxrom_Files[file_index].seekEnd(0);
  end_position = Auxrom_Files[file_index].curPosition();
  //  Serial.printf("SDSEEK: file size %d\n", end_position);
  Auxrom_Files[file_index].seekSet(current_position);                       //  Restore position, in case something goes wrong
  //  Serial.printf("SDSEEK: Position after restore is %d\n", Auxrom_Files[file_index].curPosition());
  if (seek_mode == 0)
  {
    target_position = offset;
  }
  else if (seek_mode == 1)
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
  if (target_position < 0)
  {
    post_custom_error_message("Trying to seek before beginning", 471);
    *p_mailbox = 0;                      //  Indicate we are done
    Serial.printf("SDSEEK Error exit 471.  Trying to seek past EOF\n");
    return;
  }
  if (target_position > end_position)
  {
    post_custom_error_message("Trying to seek past EOF", 472);
    *p_mailbox = 0;                      //  Indicate we are done
    Serial.printf("SDSEEK Error exit 472.  Trying to seek past EOF\n");
    return;
  }
  Auxrom_Files[file_index].seekSet(target_position);
  current_position = Auxrom_Files[file_index].curPosition();
  if (target_position != current_position)
  {
    post_custom_error_message("SDSEEK failed somehow", 473);
    *p_mailbox = 0;                      //  Indicate we are done
    Serial.printf("SDSEEK failed Target position %d   Current position %d\n", target_position, current_position);
    return;
  }

  //  Serial.printf("SDSEEK: New position is %d\n", current_position);
  *(uint32_t *)(AUXROM_RAM_Window.as_struct.AR_Opts + 4) = current_position;
  *p_usage    = 0;     //  SDSEEK successful
  *p_mailbox = 0;     //  Indicate we are done
  return;
}

//
//  Write the specified number of bytes to an open file
//

void AUXROM_SDWRITE(void)
{
  int         file_index;
  int         bytes_to_write;
  int         bytes_actually_written;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDWRITE\n");
#endif

  file_index = AUXROM_RAM_Window.as_struct.AR_Opts[0];               //  File number 1..11
  bytes_to_write = *p_len;             //  Length of write
  if (!Auxrom_Files[file_index].isWritable())
  {
    post_custom_error_message("SDWRITE File not open for write", 480);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("SDWRITE Error. File not open for write. File Number %d\n", file_index);
    return;
  }
  bytes_actually_written = Auxrom_Files[file_index].write(p_buffer, bytes_to_write);
  //  Serial.printf("SDWRITE to file # %2d , requested write %d bytes, %d actually written\n", file_index, bytes_to_write, bytes_actually_written);
  //
  //  Assume all is good
  //
  *p_len      = bytes_actually_written;
  *p_usage    = 0;                                                        //  SDWRITE successful
  *p_mailbox  = 0;                                                        //  Indicate we are done
  return;
}

//
//  Unmount the virtual drive specified by the MSU$         (####  currently uses Buffer 6 ####)
//

void AUXROM_UNMOUNT(void)
{
  int         device;
  int         disknum;
  char        *filename;
  int         HPIB_Select = get_Select_Code();

#if VERBOSE_KEYWORDS
  Serial.printf("Call to UNMOUNT\n");
#endif

  *p_usage = 0;     //  Assume success

  //
  //  Special case to UNMOUNT the SD Card.
  //    Closes the log file
  //    Closes all msus
  //    Closes tape-file
  //
  if (strcasecmp(p_buffer, "SDCard") == 0)
  {
    //
    //  Close the LogFile
    //
    if (logfile_active)
    {
      flush_logfile();
      close_logfile();      //  Also does logfile_active = false;
      Serial.printf("Closed logfile\n");
    }
    //
    //  Close any open Disk images    only works for device numbers 0..7 because 8 and 9 aren't used, and 10 is the printer
    //
    for (device = 0 ; device <= 7 ; device++)      //  actual upper limit is "#define NUM_DEVICES 31"  found in EBTKS_1MB5.cpp
    {
      if (devices[device])
      {
        for (disknum = 0 ; disknum < 4 ; disknum++)
        {
          filename = devices[device]->getFilename(disknum);
          if (filename)
          {
            Serial.printf("Closing MSUS :D%d%d%d   media: %s\n", HPIB_Select, device, disknum, filename);
            if (!devices[device]->close(disknum))
            {
              post_custom_error_message("UNMOUNT Disk error", 491);
              goto Unmount_exit;
            }
          }
        }
        devices[device] = NULL;             //  Need to discuss with RB  #########
      }
    }
    //
    //  Close any printer file, currently only check for device 310 (i.e. HPIB select code 3, device 10)
    //
    device  = 10;
    disknum =  0;
    if (devices[device])
    {
      filename = devices[device]->getFilename(disknum);
      if (filename)
      {
        Serial.printf("Closing Printer device %.1d10   media: %s\n", HPIB_Select, filename);
        if (!devices[device]->close(disknum))
        {
          post_custom_error_message("UNMOUNT Printer error", 491);        //    #############  this is the wrong code number. need to assign a new one
          goto Unmount_exit;
        }
      }
      devices[device] = NULL;             //  Need to discuss with RB  #########
    }

    //
    //  Close any Tape-file
    //
 
    filename = tape.getFile();
    if(filename)
    {
      Serial.printf("Closing Tape drive  media: %s\n", filename);
      tape_handle_UNMOUNT();                                                //  Will get a spurious "Closing tape" message, which is not worth fixing
    }
    goto Unmount_exit;
  }                         //  End of UNMOUNT "SDCard"    --- not case-sensitive

  //
  //  Unmount an msus$
  //

  if (!parse_MSU(p_buffer))
  {
    post_custom_error_message("UNMOUNT msus$ error", 490);
    goto Unmount_exit;
  }
  if (msu_is_tape)
  {
    tape_handle_UNMOUNT();
  }
  else
  {
    if (!devices[msu_device_code])
    {
      post_custom_error_message("Device code not supported", 512);
      goto Unmount_exit;
    }
    if(!devices[msu_device_code]->close(msu_drive_select))
    {
      post_custom_error_message("UNMOUNT Disk error", 491);
      goto Unmount_exit;
    }
  }


Unmount_exit:
  *p_mailbox = 0;            //  Must always be the last thing we do
  return;
}

//
//  WROM overwrites AUXROMs. This is quite dangerous, so be careful
//
//  WROM COMMAND: FOR THE E.WROM COMMAND: A.BOPT00 = TARGET ROM#, A.BOPT01 = 0, A.BOPT02-03 = TARGET ADDRESS
//
//  Always uses Buffer 0 and associated usage locations etc.
//
//  Target ROM must be between 0361 and 0376 (241 to 254)
//

void AUXROM_WROM(void)
{
  uint8_t     target_rom      = AUXROM_RAM_Window.as_struct.AR_Opts[8];
  uint16_t    target_address  = *(uint16_t *)(AUXROM_RAM_Window.as_struct.AR_Opts + 10);                      //  address is in the range 060000 to 0100000
  uint16_t    transfer_length = AUXROM_RAM_Window.as_struct.AR_Lengths[0];
  uint8_t   * data_ptr        = (uint8_t *)&AUXROM_RAM_Window.as_struct.AR_Buffer_0[0];
  uint8_t   * dest_ptr;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to WROM\n");
#endif

//  Serial.printf("/nTarget ROM ID(oct):     %03O\n", target_rom);
//  Serial.printf("Target Address(oct): %06O\n", target_address);
//  Serial.printf("Bytes to write(dec):     %d\n", transfer_length);

  if ((dest_ptr = getROMEntry(target_rom)) == NULL)            //  romMap is a table of 256 pointers to locations in EBTKS's address space.
  {                                                           //  i.e. 32 bit pointers. Either NULL, or an address. The index is the ROM ID
    Serial.printf("Selected ROM not loaded\n");
    // return error
  }
  if ((target_rom < 0361) || (target_rom > 254))
  {
    Serial.printf("Selected ROM is not an AUXROM\n");
    // return error
  }
  if (target_address + transfer_length  >= 0100000 )
  {
    transfer_length = 0100000 - target_address;           //  Don't let AUXROM write past end of ROM.  Maybe we should report an error
  }
  dest_ptr = dest_ptr + (target_address - 060000);
  while(transfer_length--)
  {
    *dest_ptr++ = *data_ptr    ++;
  }
}

void AUXROM_MEMCPY(void)
{
#if VERBOSE_KEYWORDS
  Serial.printf("Call to MEMCPY (not yet implemented)\n");
#endif


}

//
//  Set the color of the two RGB LEDs
//
//  AR_Opts[0]    1   Set LED closest to Power Inlet
//                2   Set LED closest to SD Card
//                3   Set both LEDs to same color
//  AR_Opts[1]    RED   value 0..255
//  AR_Opts[2]    GREEN value 0..255
//  AR_Opts[3]    BLUE  value 0..255
//

void AUXROM_SETLED(void)
{

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SETLED\n");
#endif

  if ((AUXROM_RAM_Window.as_struct.AR_Opts[0] == 1) || (AUXROM_RAM_Window.as_struct.AR_Opts[0] == 3))
  {
    setLedColor(0,  AUXROM_RAM_Window.as_struct.AR_Opts[1],   //  Red
                    AUXROM_RAM_Window.as_struct.AR_Opts[2],   //  Green
                    AUXROM_RAM_Window.as_struct.AR_Opts[3]);  //  Blue
  }
  if ((AUXROM_RAM_Window.as_struct.AR_Opts[0] == 2) || (AUXROM_RAM_Window.as_struct.AR_Opts[0] == 3))
  {
    setLedColor(1,  AUXROM_RAM_Window.as_struct.AR_Opts[1],   //  Red
                    AUXROM_RAM_Window.as_struct.AR_Opts[2],   //  Green
                    AUXROM_RAM_Window.as_struct.AR_Opts[3]);  //  Blue
  }
  WS2812_update();

}

//
//  SDCOPY srcfile$, dstfile$
//

void AUXROM_SDCOPY(void)
{
  bool        copy_status;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDCOPY\n");
#endif

  if(!Resolve_Path(p_buffer))
  {
    Serial.printf("SDCOPY error return 520\n");
    post_custom_error_message("Can't resolve Source path", 520);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("SDCOPY Can't resolve Source path  [%s]\n", p_buffer);
    return;
  }
  if (!SD.exists(Resolved_Path))
  {
    Serial.printf("SDCOPY Source file doesn't exist 523\n");
    post_custom_error_message("Source file doesn't exist", 523);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("SDCOPY Source File path  [%s]\n", p_buffer);
    return;
  }
  //
  //  We need a temporary buffer for the Source file resolved path. We could allocate another 258 bytes,
  //  but instead we use Resolved_Old_Path from SDREN, since that function isn't currently running
  //
  strlcpy(Resolved_Old_Path, Resolved_Path, MAX_SD_PATH_LENGTH+1);      //  Save path to Source file, as we need Resolve_Path() to process the destination path
  if(!Resolve_Path(p_buffer+256))
  {
    Serial.printf("SDCOPY error return 521\n");
    post_custom_error_message("Can't resolve Destination path", 521);
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("SDCOPY Can't resolve Destination path  [%s]\n", p_buffer+256);
    return;
  }

  if (SD.exists(Resolved_Path))
  {
    if (AUXROM_RAM_Window.as_struct.AR_Opts[0] == 0)
    {   //  Overwrite is not allowed
      post_custom_error_message("SDCOPY File already Exists", 527);
      *p_mailbox = 0;      //  Indicate we are done
      Serial.printf("SDCOPY File already Exists  [%s]\n", Resolved_Path);
      return;
    }
    //
    //  Overwrite flag must be non-zero, so overwrite is allowed. Easy solution is delete the destination file 
    //
    SD.remove(Resolved_Path);
  }

  copy_status = copy_sd_file(Resolved_Old_Path, Resolved_Path);

  if(!copy_status)
  {
    if (AUXROM_RAM_Window.as_struct.AR_ERR_NUM == 417)
    {
      post_custom_error_message("Couldn't open Source File", 524);
    }
    else if (AUXROM_RAM_Window.as_struct.AR_ERR_NUM == 418)
    {
      post_custom_error_message("Couldn't open Destination File", 525);
    }
    else if (AUXROM_RAM_Window.as_struct.AR_ERR_NUM == 419)
    {
      post_custom_error_message("File copy failed", 526);
    }
    else
    {
      post_custom_error_message("SDCOPY bug in code", 522);     //  This should never happen
    }
    *p_mailbox = 0;      //  Indicate we are done
    Serial.printf("SDCOPY Copy failed.  Resolved paths are Source [%s]   Destination [%s]\n", Resolved_Old_Path, Resolved_Path);
    return;
  }
  //
  //  Success exit
  //
  *p_usage = 0;                         //  Indicate Success
  *p_mailbox = 0;                       //  Must always be the last thing we do
  return;
}

//
//  SDEOF(file#)
//    File number is in AR_Opts[0]
//    return value is placed in AR_Opts[4..7]
//  Returns the number of bytes from the current position to the end of file.
//  Returns 0 if the current position is the end of file
//

void AUXROM_SDEOF(void)
{
  int         file_index;
  int         bytes_till_the_end;

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDEOF\n");
#endif

  file_index = AUXROM_RAM_Window.as_struct.AR_Opts[0];                      //  File number 1..12
  //
  //  check the file is open
  //
  if (!Auxrom_Files[file_index].isOpen())
  {
    post_custom_error_message("SDEOF file isn't open", 530);
    *p_mailbox = 0;                                                         //  Indicate we are done
    Serial.printf("SDEOF File isn't open. File # %d\n", AUXROM_RAM_Window.as_struct.AR_Opts[0]);
    return;
  }
  bytes_till_the_end = Auxrom_Files[file_index].available();
  *(uint32_t *)(AUXROM_RAM_Window.as_struct.AR_Opts + 4) = bytes_till_the_end;
  *p_usage    = 0;                                                          //  SDEOF successful
  *p_mailbox = 0;                                                           //  Indicate we are done
  return;
}

//
//  SDEXISTS(filePathName$)
//
//  Returns true if file/directory exists
//
//  Directory Tests:                returns
//    SDEXISTS("tapes")               1
//    SDEXISTS("TAPES")               1
//    SDEXISTS("TApeS")               1
//    SDEXISTS("/tapes")              1
//    SDEXISTS("/tapes/")             1
//    SDEXISTS("tapes/")              1
//    SDEXISTS("tapez")               0
//
//  Filename Tests:                 returns
//    SDEXISTS("CONFIG.TXT")          1
//    SDEXISTS("/CONFIG.TXT")         1
//    SDEXISTS("config.txt")          1
//    SDEXISTS("/CONFIG.TXT/")        1     Surprising, but not worth fixing
//    SDEXISTS("CONFIG.zzz")          0
//

void AUXROM_SDEXISTS(void)
{

#if VERBOSE_KEYWORDS
  Serial.printf("Call to SDEXISTS\n");
#endif

  if(!Resolve_Path(p_buffer))
  {
    Serial.printf("Can't resolve path  330\n");
    post_custom_error_message("Can't resolve path", 330);
    *p_mailbox = 0;                                                           //  Indicate we are done
    Serial.printf("SDEXISTS can't resolve Old path  [%s]\n", p_buffer);
    return;
  }

  AUXROM_RAM_Window.as_struct.AR_Opts[0] = SD.exists(Resolved_Path) ? 1 : 0;
  *p_usage    = 0;                                                            //  SDEXISTS successful
  *p_mailbox  = 0;                                                            //  Indicate we are done
  return;
}

//
//  Return the EBTKS Firmware Revision string
//

static char  date_time[25];

void AUXROM_EBTKSREV(void)
{

#if VERBOSE_KEYWORDS
  Serial.printf("Call to EBTKSREV\n");
#endif

  strcpy(date_time, EBTKS_COMPILE_TIME);
  sprintf(p_buffer, "EBTKS built %.19s", date_time);
  *p_len = strlen(p_buffer);
  *p_usage    = 0;                                                            //  SDEXISTS successful
  *p_mailbox  = 0;                                                            //  Indicate we are done
  return;
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

//
//  Remove leading and trailing spaces from New_Path
//
  src_ptr = New_Path;
  //Serial.printf("Trimming Leading space [%s]\n", src_ptr);
  while (*src_ptr == ' ')   //  skip over leading spaces
  {
    src_ptr++;
    //Serial.printf("Trimming Leading space [%s]\n", src_ptr);
  }
  if (*src_ptr)             //  Skip over trailing spaces trim if New_Path in now an empty string
  { //  not an empty string
    //  use dest_ptr temporarily for trimming trailing spaces
    dest_ptr = src_ptr + strlen(src_ptr) - 1;                 //  point at last character of New_Path
    while (src_ptr != dest_ptr)
    {
      //Serial.printf("Trimming Trailing space [%s]\n", src_ptr);
      if (*dest_ptr == ' ')
      {
        *dest_ptr = 0x00;                                     //  trim a trailing space
        dest_ptr--;
        continue;
      }
      break;
    }
  }


  if (*src_ptr == '/')
  {
    Resolved_Path[0] = '/';                                 //  Handle an absolute path
    Resolved_Path[1] = 0x00;
    dest_ptr = Resolved_Path + 1;
    src_ptr++;
  }
  else
  {
    strlcpy(Resolved_Path, Current_Path, MAX_SD_PATH_LENGTH); //  Handle relative path, so start by copying the Current_Path to the work area. Assume it does not contain any ../ or ./
                                                              //  Also, since Current_Path is a directory path, it has both leading and trailing '/' (if root, just one '/')
    dest_ptr = Resolved_Path + strlen(Resolved_Path);
    if (Resolved_Path[0] != '/')
    {
      return false;                                           //  make sure that the first character of the path we are building is a '/'
    }
    if (dest_ptr[-1] != '/')
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
    if (src_ptr[0] == 0x00)
    {
      break;                                          //  At end of the New_Path
    }
    if (src_ptr[0] == '/')
    {
      return false;                                   //  Can't have an embedded //. We would have picked the first one when copying a path segment
    }
    if (src_ptr[0] == '.' && src_ptr[1] == '/')
    {
      src_ptr += 2;                                   //  Found "./" which has no use, so just delete it
      continue;
    }
    back_up_1_level = false;
    if (src_ptr[0] == '.' && src_ptr[1] == '.' && src_ptr[2] == '/')
    {
      back_up_1_level = true;
      src_ptr += 3;                                   //  Found "../" which means up 1 directory, so need to delete a path segment
    }
    if (src_ptr[0] == '.' && src_ptr[1] == '.' && src_ptr[2] == 0x00)
    {
      back_up_1_level = true;
      src_ptr += 2;                                   //  Found ".." at end of src, which means up 1 directory, so need to delete a path segment
    }
    if (back_up_1_level)
    {
      if (strlen(Resolved_Path) <= 2)                  //  The minimal Resolved_Path that could have a directory to be removed is 3 characters"/x/"
      {                                               //    This could have been done with dest_ptr - Resolved_Path, strlen is clearer and less bug prone.
        return false;                                 //  The built path does not have a trailing directory to be removed
      }
      dest_ptr2 = dest_ptr - 1;                       //  Point at the last character of the path built so far. Should be a '/'
      if (dest_ptr2[0] != '/')
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
      if (src_ptr[0] == '/')                           //  This should not match on the first pass through this while(1) loop, as we have already checked for a leading '/'
      {
        *dest_ptr++ = *src_ptr++;                     //  Copy the '/' and we are done for this path segment
        *dest_ptr = 0x00;                             //  Mark the new end of the string, probably redundant
        break;
      }
      if (src_ptr[0] == 0x00)
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
//  Do a directory listing of the specified Directory
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

bool LineAtATime_ls_Init_SDCAT(char * path)
{
  char      dir_name[51];

  Listing_Buffer_Index  = 0;
  if (dir)
  {
    dir.close();
  }
  if (!dir.open(path, O_RDONLY))
  {
    return false;
  }
  dir.getName(dir_name, 50);
  //Serial.printf("LAAT_ls path [%s]   dir_name [%s]\n", path, dir_name);
  PS_SDCAT.init(Directory_Listing_Buffer_for_SDCAT, DIRECTORY_LISTING_BUFFER_SIZE);
  dir.ls(&PS_SDCAT, LS_SIZE | LS_DATE, 0);    //  No indent needed (3rd parameter) because we are not doing a recursive listing
  dir.close();
  return true;
}

bool LineAtATime_ls_Next_SDCAT()
{
  get_line_char_count = PS_SDCAT.get_line(dir_line, 128);

  if (get_line_char_count >= 0)
  {
    //Serial.printf("%s\n", dir_line);
    return true;
  }
  //
  //  Must be negative, so no more lines
  //
  return false;
}

bool LineAtATime_ls_Init_SDDEL(char * path)
{
  char      dir_name[51];

  Listing_Buffer_Index  = 0;
  if (dir)
  {
    dir.close();
  }
  if (!dir.open(path, O_RDONLY))
  {
    return false;
  }
  dir.getName(dir_name, 50);
  //Serial.printf("LAAT_ls path [%s]   dir_name [%s]\n", path, dir_name);
  PS_SDDEL.init(Directory_Listing_Buffer_for_SDDEL, DIRECTORY_LISTING_BUFFER_SIZE);
  dir.ls(&PS_SDDEL, LS_SIZE | LS_DATE, 0);    //  No indent needed (3rd parameter) because we are not doing a recursive listing
  dir.close();
  return true;
}

bool LineAtATime_ls_Next_SDDEL()
{
  get_line_char_count = PS_SDDEL.get_line(dir_line, 128);

  if (get_line_char_count >= 0)
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
//  All addresses are AUXROM 1, regardless of which ROM is currently selected by RSELEC
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


void post_custom_error_message(const char * message, uint16_t error_number)
{
  char   * dest_ptr;

  //
  //  Make sure AUXROMs are loaded
  //
  if ((dest_ptr = (char *)getROMEntry(0361)) == NULL)                       //  romMap is a table of 256 pointers to locations in EBTKS's address space.
  {                                                                         //  i.e. 32 bit pointers. Either NULL, or an address. The index is the ROM ID
    Serial.printf("ROM 0361 not loaded\n");
    // return error
  }
  //
  //  Make Warning 8 zero length
  //
  dest_ptr[024] = 0x80;                                                     //  Setting the MSB of location 024 means that warning 8 is zero length
  //
  //  Now copy Custom Error Message 9 into locations 025 through 064 (32 characters), and 0x00 pad if string is shorter than 32 characters
  //
  strncpy(&dest_ptr[025], message, 32);                                     //  If message is less than 32 characters, strncpy pads the destination with 0x00
  //
  //  Now mark the end of message in HP85 format which is the MSB set in the last character
  //
  dest_ptr[025 + strlen(message) - 1] |= 0x80;                              //  Mark the end of the custom error message in HP85 style by setting the MSB
  //
  //  Let the AUXROMs know the error message is in slot 9
  //
  *p_usage = 209;     //  Custom Error
  //
  //  And finally, set up AUXERRN for BASIC land
  //
  AUXROM_RAM_Window.as_struct.AR_ERR_NUM = error_number;                    //  AUXERRN
}

void post_custom_warning_message(const char * message, uint16_t error_number)
{
  char   * dest_ptr;

  if ((dest_ptr = (char *)getROMEntry(0361)) == NULL)                       //  romMap is a table of 256 pointers to locations in EBTKS's address space.
  {                                                                         //  i.e. 32 bit pointers. Either NULL, or an address. The index is the ROM ID
    Serial.printf("ROM 0361 not loaded\n");
    // return error
  }

  strncpy(&dest_ptr[024], message, 32);                                     //  If message is less than 32 characters, strncpy pads the destination with 0x00
  dest_ptr[024 + strlen(message) - 1] |= 0x80;                              //  Mark the end of the custom warning message in HP85 style by setting the MSB
  dest_ptr[064] = 0x80;                                                     //  Zero length for error message 9
  *p_usage = 208;     //  Custom Warning
  AUXROM_RAM_Window.as_struct.AR_ERR_NUM = error_number;                    //  AUXERRN
}

