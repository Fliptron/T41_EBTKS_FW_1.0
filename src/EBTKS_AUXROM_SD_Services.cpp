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

#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "Inc_Common_Headers.h"

#define MAX_SD_PATH_LENGTH          (256)

EXTMEM static char          Current_Path[MAX_SD_PATH_LENGTH  + 2];    //  I don't think initialization of EXTMEM is supported yet
EXTMEM static char          Resolved_Path[MAX_SD_PATH_LENGTH + 2];
static bool                 Resolved_Path_ends_with_slash;

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

void initialize_Current_Path(void)
{
  strcpy(Current_Path,"/");
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

  if((dest_ptr = getROMEntry(target_rom)) == NULL)            //  romMap is a table of 256 pointers to locations in EBTKS's address space. i.e. 32 bit pointers. Either NULL, or an address. The index is the ROM ID
  {
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
//  Update the Current Path with the provided path. Most of the hard work happens in Resolve_Path()
//  See its documentation
//
//  AUXROM puts the update path in Buffer 6, and the length in AR_Lengths[6]
//  Mailbox 6 is used for the handshake
//

void AUXROM_SDCD(void)
{
    Serial.printf("Current Path before SDCD [%s]\n", Current_Path);
    Serial.printf("Update Path via AUXROM   [%s]\n", AUXROM_RAM_Window.as_struct.AR_Buffer_6);
    show_mailboxes_and_usage();
  do
  {
    if(Resolve_Path((char *)AUXROM_RAM_Window.as_struct.AR_Buffer_6))       //  Returns true if no parsing problems
    {
      if((file = SD.open(Resolved_Path)) == 0)
      {
        Serial.printf("AUXROM_SDCD: Failed SD.open return value %08X\n", (uint32_t)file);
        break;                      //  Unable to open directory
      }
      if(!file.isDir())
      {
        Serial.printf("AUXROM_SDCD: Failed file.isDir\n");
        break;                      //  Valid path, but not a directory
      }
      file.close();
      if(!SD.chdir(Resolved_Path))
      {
        Serial.printf("AUXROM_SDCD: Failed SD.chdir\n");
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
      //  Serial.printf("Success. Current Path is now [%s]\n", Current_Path);
      //  show_mailboxes_and_usage();
      //  Serial.printf("\nSetting Mailbox 6 to 0 and then returning\n");
      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                      //  Release mailbox 6.    Must always be the last thing we do
      return;
    }
  } while(0);
  //
  //  Something went wrong. 
  //
  Serial.printf("AUXROM_SDCD: Resolve_Path had a problem. Failure to update Current Path\n");
  AUXROM_RAM_Window.as_struct.AR_Usages[6] = 213;                           //  Indicate Failure
  //  show_mailboxes_and_usage();
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                          //  Release mailbox 6.    Must always be the last thing we do
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
  show_mailboxes_and_usage();
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[Mailbox_to_be_processed] = 0;                      //  Release mailbox.    Must always be the last thing we do
}

//
//  Always uses Buffer 6 and associated parameters
//
//  SDCAT                     does a full directory listing to the CRT IS device (stk=0 bytes)
//  SDCAT fileSpec$           does a full directory listing to the CRT IS device of files matching 'fileSpec$' (stk=4 bytes)
//  SDCAT A$,D$,S,0           starts a NEW direcotry listing, putting first name in A$, date in D$, size in S,(stk=24,29 bytes)
//  SDCAT A$,D$,S,1           continues the ACTIVE directory listing, putting next name in A$, date in D$, size in S (stk=24,29 bytes)
//  SDCAT A$,D$,S,0,fileSpec$ starts a NEW direcotry listing, putting first name in A$, date in D$, size in S (stk=28,33 bytes)
//  SDCAT A$,D$,S,1,fileSpec$ continues the ACTIVE direcotry listing, putting next name in A$, date in D$, size in S (stk=28,38 bytes)
//
//  A$ is passed from EBTKS to AUXROM in Buf6 starting at byte 0      BLen6 holds the length which will end up in S
//  D$ is passed from EBTKS to AUXROM in Buf6 starting at byte 256    Fixed length of 16 bytes
//  S is the size of the filename starting at Buf6[0]
//  If filespec$ has a trailing slash, it is specifying a subdirectory to be catalogged. Without a slash it is a
//  file selection filter specification, that may in the future support wildcards
//
//  returns AR_Usages[6] = 0 for ok, 1 for finished (and 0 length buf6), 213 for any error
//  
//  POSSIBLE ERRORS:
//    SD ERROR - if disk error occurs OR 'continue' listing before 'start' listing
//    INVALID PARAM - if "SDCAT x,A$" where 'x' is not 0 or 1
//
//  The following is from the Series 80 Emulator, 09/20/2020
//    A.BUF6     = filename nul-terminated
//    A.BUF6[256]= formatted nul-terminated file DATE$              currently fixed at 16 characters
//    A.BLEN6    = len of filename
//    A.BOPT60-63 = size                                             File Size
//    A.BOPT64-67 = attributes                                       LSB is set for a SubDirectory, next bit is set for ReadOnly
//
//
//  No current suport for filespec$ variant. Must be "*" for now.
//

// these need to be re-done

//  Test cases        AR_BUF6_OPTS[0]   AR_BUF6_OPTS[1]  Buffer 6       Notes
//                    first=0           wildcards=0      match          We don't support wildcards in this revision
//                    next=1            unique=1         pattern
//  SDCAT             0                 0                 *             Should handle this. Initialize and return first result
//                    1                 xxx               xxx           Should handle this. This is the second call
//                    1                 xxx               xxx           Should handle this. This is for the rest of the calls
//  SDCAT "*"         0                 0                 *             Should handle this. Initialize and return first result
//  SDCAT "D*"        0                 0                 D*            Can't handle this
//  SDCAT A$,S,F,0    0                 0                 *             Should handle this. Initialize and return first result
//  SDCAT A$,S,F,1    1                 0                 *             Should handle this. Return next result
//  SDCAT A$,S,F,0,fileSpec$                                            Can't handle this
//  SDCAT A$,S,F,1,fileSpec$                                            Can't handle this
//
//  Note: In above table, the "xxx" are data left in OPTS/Buffers from the previous call. This only happens
//  for second and successive calls, where AR_BUF6_OPTS 0..3 are used for passing back the file length, and
//  since successive calls set AR_BUF6_OPTS[0] to 1, it will be 1, but on entry, the rest of AR_BUF6_OPTS 1..3
//  will have the remnants of the previous file's length, and Buffer 6 will have the prior file name, since
//  the pattern specification, which we ignore, is only provided on the first call
//

//static int        SDCAT_line_count = 0;
static bool       SDCAT_First_seen = false;     //  keeping track of whether we have seen a call for a first line of a SD catalog
static char       dir_line[130];                //  Leave room for a trailing 0x00 (that is not included in the passed max length of PS.get_line)
static int32_t    get_line_char_count;
static char       sdcat_filespec[130];
static int        sdcat_filespec_length;
static bool       filespec_is_dir;

void AUXROM_SDCAT(void)
{
  uint32_t    temp_uint;
  char        file_size[12];
  File        temp_file;

  //
  //  Show Parameters
  //
  Serial.printf("\nSDCAT Start next entry\n");

  if(AUXROM_RAM_Window.as_struct.AR_BUF6_OPTS[0] == 0)  //  This is a First Call
  {

    ///////  filespec$ would be handled here. if trailing slash, then we are catalogging a subdirectory. need to save it
    sdcat_filespec_length = strlcpy(sdcat_filespec, AUXROM_RAM_Window.as_struct.AR_Buffer_6, 129);    //  this could be either a pattern match template, or a subdirectory if trailing slash
    filespec_is_dir = (sdcat_filespec[sdcat_filespec_length-1] == '/');
    if(!LineAtATime_ls_Init())
    {                                                       //  Failed to do a listing of the current directory
      AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 213;    //  Error
      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
      Serial.printf("SDCAT Error exit 2.  Failed to do a listing of the current directory\n");
      return;
    }
    SDCAT_First_seen = true;
    Serial.printf("Initial call, Filespec is %s, and [is a directory] is %s\n", sdcat_filespec, filespec_is_dir ? "true":"false");
  }
  if(!SDCAT_First_seen)
  {
    //
    //  Either we haven't done a first call, or a previous call reported we were at the end of the directory
    //
    AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 213;    //  Error
    AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;      //  Indicate we are done
    Serial.printf("SDCAT Error exit 3.  Missing First call to SDCAT\n");
    return;
  }
  if(!LineAtATime_ls_Next())
  {   //  No more directory lines
    SDCAT_First_seen = false;                             //  Make sure the next call is a starting call
    AUXROM_RAM_Window.as_struct.AR_Lengths[6]   = 0;      //  nothing more to return
    AUXROM_RAM_Window.as_struct.AR_Usages[6]    = 1;      //  Success and END
    Serial.printf("We are done, no more entries\n");
    show_mailboxes_and_usage();
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
  Serial.printf("dir_line   [%s]\n", dir_line);
  strlcpy(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[256], dir_line, 17);                 //  The DATE & TIME string is copied to Buffer 6, starting at position 256
  strlcpy(file_size, &dir_line[16], 12);                                                //  Get the size of the file. Still needs some processing
  temp_uint = strlcpy(AUXROM_RAM_Window.as_struct.AR_Buffer_6, &dir_line[28], 129);     //  Copy the filename to buffer 6, and get its length
  AUXROM_RAM_Window.as_struct.AR_Lengths[6] = temp_uint;                                //  Put the filename length in the right place
  AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1] = (AUXROM_RAM_Window.as_struct.AR_Buffer_6[temp_uint - 1] == '/') ? 1:0;  // record whether the file is actually a directory

  temp_uint = atoi(file_size);                                                          //  Convert the file size to an int
  AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[0] = temp_uint;                //  Return file size in A.BOPT60-63
  //
  //  Need to get read-only status, and whether it is a directory
  //
  temp_file = SD.open(AUXROM_RAM_Window.as_struct.AR_Buffer_6);                         //  SD.open() does not mind the trailing slash if the name is a directory
  if(temp_file.isReadOnly())
  {
    AUXROM_RAM_Window.as_struct_a.AR_BUF6_OPTS.as_uint32_t[1] |= 0x02;                  //  Indicate the file is read only
  }
  temp_file.close();

  //Serial.printf("Filename [%s]   Date/time [%s]   Size [%s] = %d\n", AUXROM_RAM_Window.as_struct.AR_Buffer_6, date_time, file_size, temp_uint);
  AUXROM_RAM_Window.as_struct.AR_Usages[6]  = 0;        //  Success
  Serial.printf("\nSuccess\n");
  show_mailboxes_and_usage();
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;  //  Indicate we are done
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

  Serial.printf("Path so far [%s]\n", Resolved_Path);
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
    if(src_ptr[0] == '.' && src_ptr[1] == '.' && src_ptr[2] == '/')
    {
      src_ptr += 3;                                   //  Found "../" which means up 1 directory, so need to delete a path segment
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
  Serial.printf("Resolved_Path is [%s]\n", Resolved_Path);
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

bool LineAtATime_ls_Init(void)
{
  Listing_Buffer_Index  = 0;
  
  if(dir)
  {
    dir.close();
  }

  if(!dir.open(Current_Path, O_RDONLY))
  {
    return false;
  }

  PS.init(Directory_Listing_Buffer, DIRECTORY_LISTING_BUFFER_SIZE);
  dir.ls(&PS, LS_SIZE | LS_DATE, 0);    //  No indent needed (3rd parameter) because we are not doing a recursive listing
  return true;
}

bool LineAtATime_ls_Next()
{
  get_line_char_count = PS.get_line(dir_line, 128);

  if(get_line_char_count >= 0)
  {
    Serial.printf("%s\n", dir_line);
    return true;
  }
  //
  //  Must be negative, so no more lines
  //
  return false;
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
  show_mailboxes_and_usage();
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




////////    case AUX_USAGE_3NUM:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_3NUM
////////      //  AUX3NUMS n1, n2, n3
////////      //
////////      //  From email with Everett, 8/9/2020 @ 11:33
////////      //
////////      //  AUX3NUMS n1, n2, n3
////////      //  a) When AUX3NUMS runtime gets called, leave the three 8-byte values
////////      //     on the stack, write the R12 value to A.MBR12.
////////      //  b) Send a message to EBTKS:
////////      //      Write "TEST" to BUF0
////////      //      Write 1 to USAGE0
////////      //      Write 1 to Mailbox0
////////      //  b') Write 0 to 177740  (HEYEBTKS)
////////      //  c) Wait for MailBox0==0
////////      //  d) If USAGE0==0, throw away 24-bytes from R12, else load R12
////////      //     from A.MBR12 (assuming EBTKS is "popping the values").
////////      //
////////      //LOGPRINTF_AUX("R12, got %06o   ", AUXROM_RAM_Window.as_struct.AR_R12_copy);
////////      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0] , 3*8);
////////
////////      for(param_number = 0 ; param_number < 3 ; param_number++)
////////      {
////////        HexDump_T41_mem((uint32_t)&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0], 8, false, false);
////////        Serial.printf("    ");
////////      }
////////      Serial.printf("\n");
////////
////////      for(param_number = 0 ; param_number < 3 ; param_number++)
////////      {
////////        if(Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[4] == 0xFF)
////////        { //  The parameter is a tagged integer
////////          LOGPRINTF_AUX("Integer %7d       ", cvt_R12_int_to_int32(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0]));
////////        }
////////        else
////////        {
////////          LOGPRINTF_AUX("Real %20.14G ", cvt_HP85_real_to_IEEE_double(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0]));
////////        }
////////      }
////////      LOGPRINTF_AUX("\n");
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;         //  Claim success
////////      break;
////////    case AUX_USAGE_1SREF:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_1SREF
////////      //  AUX1STRREF
////////      //
////////      //  From email with Everett, 8/9/2020 @ 11:33
////////      //
////////      //  AUX1STRREF A$
////////      //  a) When AUX1STRREF runtime gets called, leave the string reference
////////      //    on the stack, write the R12 value to A.MBR12.
////////      //  b) Send a message to EBTKS:
////////      //      Write "TEST" to BUF0
////////      //      Write 2 to USAGE0
////////      //      Write 1 to Mailbbox0
////////      //  b') Write 0 to 177740  (HEYEBTKS)
////////      //  c) Wait for Mailbox==0
////////      //  d) If USAGE0==0, throw away the strref from R12, else load R12
////////      //     from A.MBR12.
////////      //
////////      //  Discovered: what is passed is the pointer to the text part of the string. There is a header of
////////      //  8 bytes prior to this point, documented on page 5-32 of the HP-85 Assembler manual. Of these
////////      //  8 bytes, the last 2 (just before the text area starts) is the Valid Length of the text. For
////////      //  a string variable like A$, if not dimensioned, 18 bytes are allocated for the text area, and
////////      //  the 4 bytes prior to the just mentioned Valid Length are two copies of the Max Length, which
////////      //  is 18 (22 octal). So we will be needing the Valid Length.
////////      //  Not documented: If the String Variable is un-initialized, its Actual_Length is -1
////////      //
////////
////////      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_SREF.string.unuseable_abs_rel_addr , 6);
////////      LOGPRINTF_AUX("Unuseable Addr %06o   Length %06o  Address Passed %06o\n" ,
////////                        Parameter_blocks.Parameter_Block_SREF.string.unuseable_abs_rel_addr,
////////                        Parameter_blocks.Parameter_Block_SREF.string.length,
////////                        Parameter_blocks.Parameter_Block_SREF.string.address );
////////      //
////////      //  Per the above description, we need to collect the string starting 8 bytes lower in memory,
////////      //  and retrieve 8 more bytes than indicated by the passed length, which is the max length, not
////////      //  the Valid Length
////////      //
////////      AUXROM_Fetch_Memory(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], Parameter_blocks.Parameter_Block_SREF.string.address - 8, Parameter_blocks.Parameter_Block_SREF.string.length + 8);
////////      //
////////      //  these variables are getting unwieldly. try and make it simple
////////      //
////////      HP85_String_Pointer = (struct  S_HP85_String_Variable *)(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0]);
////////      LOGPRINTF_AUX("String Header Flags bytes %03o , %03o TotalLen %4d MaxLen %4d Actual Len %5d\n", HP85_String_Pointer->flags_1, HP85_String_Pointer->flags_2,
////////                    HP85_String_Pointer->Total_Length, HP85_String_Pointer->Max_Length, HP85_String_Pointer->Actual_Length);
////////      if(HP85_String_Pointer->Actual_Length == -1)
////////      {
////////        LOGPRINTF_AUX("String Variable is uninitialized\n");
////////      }
////////      else
////////      {
////////        LOGPRINTF_AUX("[%.*s]\n", HP85_String_Pointer->Actual_Length, HP85_String_Pointer->text);
////////        LOGPRINTF_AUX("Dump of Header and Text area\n");
////////        for(i = 0 ; i < HP85_String_Pointer->Total_Length + 10 ; i++)
////////        {
////////          LOGPRINTF_AUX("%02X ", AUXROM_RAM_Window.as_struct.AR_Buffer_6[i]);
////////          if(i == 7) LOGPRINTF_AUX("\n");        //  a new line after the header portion
////////        }
////////        LOGPRINTF_AUX("\n");
////////      }
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;         //  Claim success
////////      break;
////////    case AUX_USAGE_NFUNC:                            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_NFUNC
////////      //  HP85 function is TESTNUMFUN(123.456) returns the value after two conversions
////////      //  Inbound number is on the R12 stack, result goes into Buffer 6 (8 bytes)
////////      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0] , 8);    //  Get one HP85 Real 8 byte number off the stack
////////      //
////////      //  Convert the HP85 number to IEEE Double, and convert it back and put in Buf 6
////////      //
////////      param_1_val = cvt_HP85_real_to_IEEE_double(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0]);
////////      cvt_IEEE_double_to_HP85_number(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], param_1_val);
////////
////////#if TRACE_TESTNUMFUN
////////      Serial.printf("Input number: ");
////////      HexDump_T41_mem((uint32_t)&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0], 8, false, false);
////////      Serial.printf("     Output number: ");
////////      HexDump_T41_mem((uint32_t)&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], 8, false, false);
////////      Serial.printf("difference  %.11e  ", param_1_val - cvt_HP85_real_to_IEEE_double(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0]) );
////////      Serial.printf(" Value %20.14G\n", param_1_val);
////////#else
////////      if((param_1_val - cvt_HP85_real_to_IEEE_double(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0])) != 0)
////////      {
////////        Serial.printf("TESTNUMFUN ERROR converting  %20.14G\n", param_1_val);
////////      }
////////#endif
////////
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;     //  Success
////////      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                        //  Release mailbox 6
////////      break;
////////    case AUX_USAGE_SFUNC:                            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SFUNC
////////      //  show_mailboxes_and_usage();
////////      //  HP85 function is TESTSTRFUN$("Some Text")) returns the string in Buffer 6
////////      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_SVAL.string_val.length , 4);    //  Get one HP85 string_val off the stack
////////      string_len = Parameter_blocks.Parameter_Block_SVAL.string_val.length;
////////      string_addr = Parameter_blocks.Parameter_Block_SVAL.string_val.address;
////////      //  Serial.printf("String Length:  %d\n", string_len);
////////      //  Serial.printf("String Address: %06o\n", string_addr);
////////      //  HexDump_HP85_mem(string_addr, 16, true, true);
////////      AUXROM_Fetch_Memory(AUXROM_RAM_Window.as_struct.AR_Buffer_6, string_addr, string_len);
////////
//////////#if TRACE_TESTNUMFUN
//////////      Serial.printf("Input number: ");
//////////      HexDump_T41_mem((uint32_t)&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0], 8, false, false);
//////////      Serial.printf("     Output number: ");
//////////      HexDump_T41_mem((uint32_t)&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], 8, false, false);
//////////      Serial.printf("difference  %.11e  ", param_1_val - cvt_HP85_real_to_IEEE_double(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0]) );
//////////      Serial.printf(" Value %20.14G\n", param_1_val);
//////////#endif
////////
////////      AUXROM_RAM_Window.as_struct.AR_Lengths[6] = string_len;
////////      AUXROM_RAM_Window.as_struct.AR_Usages[6] = string_len;
////////
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;     //  Success
////////      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                        //  Release mailbox 6
////////      break;
////////
////////    default:
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 1;     //  Failure, unrecognized Usage code
////////  }
////////
////////  new_AUXROM_Alert = false;
////////  AUXROM_RAM_Window.as_struct.AR_Mailboxes[Mailbox_to_be_processed] = 0;      //  Relinquish control of the mailbox
////////  show_mailboxes_and_usage();
////////
////////}
