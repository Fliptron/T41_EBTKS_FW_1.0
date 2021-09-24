//
//	06/27/2020    All this wonderful code came from Russell.
//
//  2020 & 2021   And then Philip made many changes.
//

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Inc_Common_Headers.h"

#include "HpibDisk.h"
#include "HpibPrint.h"

#include <strings.h>                                    //  Needed for strcasecmp() prototype

#define JSON_DOC_SIZE   7000

//
//  Parameters from the CONFIG.TXT file
//

int           machineNum;
char          machineType[10];          //  Must be long enough to hold the longest string in machineNames[], including trailing 0x00
bool          CRTVerbose;
bool          screenEmu;
bool          CRTRemote;
bool          AutoStartEn;
bool          tapeEn;

extern bool enRam16k;

#if ENABLE_EMC_SUPPORT
bool          EMC_Enable;
int           EMC_NumBanks = 4;         //  Correct value is 8. If we see only 4 being supported, it either got it from CONFIG.TXT, or initialization failed
int           EMC_StartBank = 5;        //  Correct for systems with pre-installed 128 kB
int           EMC_StartAddress;
int           EMC_EndAddress;
bool          EMC_Master = false;
#endif


//
//  Read machineType string (must match enum machine_numbers {} , which is in EBTKS.h) 
//  Valid strings are:
//
const char *machineNames[] = {"HP83", "HP9915A", "HP85A", "HP85AEMC", "HP85B", "HP9915B", "HP86A", "HP86B", "HP87", "HP87XM"};

extern HpibDevice *devices[];

// extern uint8_t roms[MAX_ROMS][ROM_PAGE_SIZE];        //  Just for diag prints

bool loadRom(const char *fname, int slotNum, const char *description)
{
  uint8_t       header[3];
  uint8_t       id, id_comp;

  //  log_to_CRT_ptr should already be setup, but just in case it isn't we set it up here, so that we don't have writes going somewhere dangerous
  if (log_to_CRT_ptr == NULL)
  {
    log_to_CRT_ptr = &CRT_Log_Buffer[0];
  }

  //
  //  This was used to debug mysterious failure to load the first ROM.
  //  The problem turned out to be that part of DMAMEM was already in the dcache, and the DMA base read of
  //  the ROM image was not updating/invalidating the dcache.
  //  Solution was the following line, seen below in the code, just prior to the read of the ROM
  //    arm_dcache_flush_delete(getRomSlotPtr(slotNum), ROM_PAGE_SIZE);
  //
  // unsigned int * roms_diag = (unsigned int *)&roms[0][0];
  // Serial.printf("!About to load ROM %s into slot %d\n", fname, slotNum);                  // diag for when first ROM got lost
  // Serial.printf("!Check slot 0 first 4     %08X\n", *(roms_diag));                        // diag for when first ROM got lost
  // Serial.printf("!Check slot 0+188         %08X\n", *(roms_diag + 188));                  // diag for when first ROM got lost
  // Serial.printf("!Check slot 1 first 4     %08X\n", *(roms_diag + ROM_PAGE_SIZE/4));      // diag for when first ROM got lost, /4 because pointer is an int *
  // Serial.printf("!Check address of Slot 0  %08X\n", (unsigned int)roms_diag);             // diag for when first ROM got lost

  //
  //  Only enable the  Serial.printf() in this function if the processing of the CONFIG.TXT is after the Serial device is connected (wait code in EBTKS.cpp)
  //
  // Serial.printf("In loadrom()  name is %s   slot is %d   description is %s\n", fname, slotNum, description);
  // Serial.flush();

  //
  //  Attempts to read a HP80 ROM file from the SD Card using the given filename
  //  read the file and check the ROM header to verify it is a ROM file and extract
  //  the ID, then insert it into the ROM emulation system
  //
  if (slotNum > MAX_ROMS)
  {
    // Serial.printf("ROM slot number too large %d\n", slotNum);
    // Serial.flush();

    LOGPRINTF("ROM slot number too large %d\n", slotNum);
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Slot error\n");
    return false;
  }

  File rfile = SD.open(fname, FILE_READ);

  if (!rfile)
  {
    // Serial.printf("ROM failed to Open %s\n", fname);
    // Serial.flush();

    LOGPRINTF("ROM failed to Open %s\n", fname);
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Can't Open ROM File\n");
    rfile.close();                                    //  Is this right??? If it failed to open, and rfile is null (0) , then what would this statement do???
    return false;
  }
  //
  //  Validate the ROM image. The first two bytes are the id and the complement (except for secondary AUXROMs)
  //  but to support the way we are handling HP86/87, we read 3 bytes.
  //  See comment below that starts "No special ROM loading for Primary AUXROM at 0361. ....."
  //
  if (rfile.read(header, 3) != 3)
  {
    // Serial.printf("ROM file read error %s\n", fname);
    // Serial.flush();

    LOGPRINTF("ROM file read error %s\n", fname);
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Can't read ROM header %s\n", fname);
    rfile.close();
    return false;
  }

  //
  //  Nothing special for normal HP-85 ROMs, first byte at 060000 is the ROM ID, next byte is the one's complement.
  //    For HP-85A and HP-85B the sum is always 0377  (0xFF)
  //    For HP-86 and 87 it is the twos complement, and the sum is always 0.
  //
  //    For AUXROMs, there is a Primary AUXROM, with normal two byte header of
  //      85A/B:  0361 0016  (0xF1  and 0x0E)
  //      86/87:  0361 0017  (0xF1  and 0x0F)
  //    For Secondary AUXROMs, the first byte is 0377, second byte 0362 to 0375. The third byte is complemented normally (85: 1's or 86/87: 2's) then ORed with 0360.
  //                               AUXROMS move the ID and Complement to their correct position during initialization. See Everett's note below for an explanation
  //               85 86/87
  //          361 016  017
  //          362 375  376
  //          363 374  375
  //          364 373  374
  //          365 372  373
  //          366 371  372
  //          367 370  371
  //          370 367  370
  //          371 366  367
  //          372 365  366
  //          373 364  365
  //          374 363  364
  //          375 362  363
  //          376 361  362
  //
  //  Standard ROMS in an HP87 (at least Philip's one) are
  //  Decimal
  //  001   HP87 Graphics
  //  320   Mass Storage (octal)
  //

  //
  //  No special ROM loading for Primary AUXROM at 0361.  This code handles the Secondaries from 0362 to 0375
  //
  //  Non-primary AUXROMs now have 377 as first byte, ROM# as second byte, aux_complement as third byte
  //  so that the 87 doesn't see them as "non-87 ROMs" and issue a warning.
  //  so, if 377 is the first byte, we move the 2nd and 3rd bytes down before proceeding.
  //  This shift of bytes is in our local copy of the first 2 bytes of the ROM. The actual ROM image
  //  still starts with 377, id "what will be the first byte", "what will be the second byte"
  //
  //  Everett:
  //    I build AUX 1 as normal. For AUX ROMs 2, 3, 4, etc., the first three
  //    bytes are: 377, ROM#, AUXCOMP# (in other words, the same bytes that
  //    USED to be in the first two locations are now in the 2nd and 3rd
  //    locations). This keeps the system from seeing them at "ROM log in"
  //    time. Then, during the ROM "power-on INIT" call, in the INIT routine
  //    from AUX 1, the first thing it does is send a U_WROM command to
  //    write the proper ROM# in the first byte location for each AUX ROM
  //    2, 3, 4, etc. This way, those who look at 60000 to see which ROM is
  //    currently loaded will continue to work fine.
  //
  id      = header[0];
  id_comp = header[1];
  if (id == 0377)               //  If id is 377, then assume it is an AUXROM, but not the first one at 0361
  {
    id      = header[1];
    id_comp = header[2];
  }

  // Serial.printf("%03o %3d  %02X   %02X    %02X   ", id, id, id, id_comp, (uint8_t)(id + id_comp));
  // Serial.flush();

  LOGPRINTF("%03o %3d  %02X   %02X    %02X   ", id, id, id, id_comp, (uint8_t)(id + id_comp));
  log_to_CRT_ptr += sprintf(log_to_CRT_ptr, " ID: %03o ", id);

  //
  //  Check that the id and id_comp are correct. 
  //

  if ((id >= AUXROM_SECONDARY_ID_START) && (id <= AUXROM_SECONDARY_ID_END))         //  Testing AUXROM 362..N  Not AUXROM 361
  {
    //
    //  Special check for the the non-primary AUXROMs.  Note that these are not recognized by the HP-85 at boot time scan (by design)
    //
    //  HP85A/B             use 1's complement for the ID check byte in header[1] (also HP83 and HP9915)
    //  HP86A/B, HP87A/XM   use 2's complement for the ID check byte in header[1]
    //
    //  Note: For Secondary AUXROMs (which is what we are testing here), the upper nibble is always 0xF0
    //
    //  If HP86/87 adding 1 turns a 1's complement into a 2's complement
    //
    uint8_t comp = ((uint8_t)(~id)) + (HAS_2S_COMP_ID ? 1 : 0);

    if (id_comp != (comp | (uint8_t)0360))                                      //  Check the ID check byte
    {
      // Serial.printf("Secondary AUXROM file header error %02X %02X\n", id, (uint8_t)header[1]);
      // Serial.flush();

      LOGPRINTF("Secondary AUXROM file header error %02X %02X\n", id, (uint8_t)header[1]);
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "HeaderError\n");
      rfile.close();
      return false;
    }
  }
  else
  { //  Check for machine type.
    if (HAS_1S_COMP_ID)
    {                                   //  CONFIG.TXT says we are loading ROMS for HP85A/B, HP83, HP9915A/B:  One's complement for second byte of ROM
      if (id != (uint8_t)(~id_comp))    //  now test normal ROM ID's , including the AUXROM Primary 361.
      {
        // Serial.printf("ROM file header error first two bytes %02X %02X   Expect One's Complement\n", id, (uint8_t)id_comp);
        // Serial.flush();

        LOGPRINTF("ROM file header error first two bytes %02X %02X   Expect One's Complement\n", id, (uint8_t)id_comp);
        log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "HeaderError\n");
        rfile.close();
        return false;
      }
    }
    else if (HAS_2S_COMP_ID)
    {                                         //  CONFIG.TXT says we are loading ROMS for HP86 or HP87:  Two's complement for second byte of ROM
      if (id != (uint8_t)(-id_comp))          //  Now test normal ROM ID's , including the AUXROM Primary.
      {
        // Serial.printf("ROM file header error first two bytes %02X %02X   Expect Two's Complement\n", id, (uint8_t)header[1]);
        // Serial.flush();

        LOGPRINTF("ROM file header error first two bytes %02X %02X   Expect Two's Complement\n", id, (uint8_t)header[1]);
        log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "HeaderError\n");
        rfile.close();
        return false;
      }
    }
    else
    {
      LOGPRINTF("ERROR Unsupported Machine type\n");
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Mach???\n");
    }
  }

  //  ROM header looks good, read the ROM file into memory
  rfile.seek(0);                                                    //  Rewind the file

  //
  //  Before loading the ROM into DMAMEM, we must make sure that the target area in DMAMEM is not already
  //  being shadowed in the dcache. (This was a problem, that was quite difficult to track down)
  //

  arm_dcache_flush_delete(getRomSlotPtr(slotNum), ROM_PAGE_SIZE);

  int flen = rfile.read(getRomSlotPtr(slotNum), ROM_PAGE_SIZE);
  if (flen != ROM_PAGE_SIZE)
  {
    LOGPRINTF("ROM file length error length: %d\n", flen);
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Length Error\n");
    rfile.close();
    return false;
  }

  //  We got this far, update the rom mapping table to say we're here
  log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "OK\n");
  setRomMap(id, slotNum);

  LOGPRINTF("%-1s\n", description);
  rfile.close();
  return true;
}

//
//  Returns the enumeration of the machine name selected in the config file
//
int get_machineNum(void)
{
  return machineNum;
}

char * get_machineType(void)
{
  return machineType;
}

bool get_CRTVerbose(void)
{
  return CRTVerbose;
}

bool get_screenEmu(void)
{
  return screenEmu;
}

bool get_CRTRemote(void)
{
  return CRTRemote;
}

bool get_AutoStartEn(void)
{
  return AutoStartEn;
}

bool get_TapeEn(void)
{
  return tapeEn;
}



#if ENABLE_EMC_SUPPORT
bool get_EMC_Enable(void)
{
  return EMC_Enable;
}

int get_EMC_NumBanks(void)
{
  return EMC_NumBanks;
}

int get_EMC_StartBank(void)
{
  return EMC_StartBank;
}

int  get_EMC_StartAddress(void)
{
   return EMC_StartAddress;
}

int  get_EMC_EndAddress(void)
{
  return EMC_EndAddress;
}

bool get_EMC_master(void)
{
  return EMC_Master;
}

#endif

//
//  This part of the CONFIG.TXT JSON processing is a separate function because we need to do this processing
//  both at system boot time, and if we MOUNT the SD Card
//
//  This covers the disk drives, the printer, and the tape drive
//
//  For the MOUNT case we assume that the machineNum has not changed since boot, even though we may be
//  re-mounting the SD Card, so the tape test uses prior info about machineNum with the macro SUPPORTS_TAPE_DRIVE.
//  Thus, a MOUNT call could change whether Tape emulation occurs, and be different from the preceding boot, if
//  CONFIG.TXT was changed between a call to UNMOUNT SDCARD and MOUNT "SDCARD","xyzzy"  .  Damn edge cases.
//

void mount_drives_based_on_JSON(JsonDocument& doc)
{
  char * temp_char_ptr;

  tapeEn = false;             //  Don't support tape, unless we can, and we do
  if (SUPPORTS_TAPE_DRIVE)
  {
    tapeEn = doc["tape"]["enable"] | false;
  }
  tape.enable(tapeEn);

  if (tapeEn)                 //  Only set the filepath if the tape subsystem is enabled
  {
    const char *tapeFname = doc["tape"]["filepath"] | "/tapes/tape1.tap";
    tape.setFile(tapeFname);
    LOGPRINTF("Tape emulation is enabled, file is: %s\n", tapeFname);
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, ":T  %s\n", tape.getFile());
  }

  //
  //  Configure the disk drives. Currently we only handle one HPIB interface
  //
  //  Although we don't check it and report an error if necessary, the following things      ####  should we add checks ? ####
  //  MUST be true of the HPIB device configuration comming from the CONFIG.TXT file
  //
  //    1) The "select" fields must all have the same value 3..10 as we can only emulate 1 HPIB interface
  //    2) The "device" fields must be different, and should probably start at 0 and go no higher than 7
  //

  int  HPIB_Select = doc["hpib"]["select"] | 3;                             //  1MB5 select code (3..10). 3 is the default

  for (JsonVariant hpibDevice : doc["hpib"]["devices"].as<JsonArray>())     //  Iterate HPIB devices on a bus
  {
    int select = HPIB_Select;
    int device = hpibDevice["device"] | 0;                                  //  HPIB device number 0..31 (can contain up to 4 drives)
    bool enable = hpibDevice["enable"] | false;                             //  are we enabled?

    //  log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "HPIB Sel %d Dev %d En %d\n", select, device, enable);

    if (hpibDevice["drives"])                                               //  If the device has a drives section, then it must be a disk drive box
    {
      int type = hpibDevice["type"] | 0;                                    //  Disk drive type 0 = 5 1/4"
      char  type_text[10];
      switch(type)
      {
        case 0:
          strcpy(type_text, "Floppy");
          break;
        case 4:
          strcpy(type_text, "5 MB  ");
          break;
        default:
          strcpy(type_text, "Unknown");
          break;
      }

      if ((devices[device] == NULL) && (enable == true))
      {
        devices[device] = new HpibDisk(device);                             //  Create a new HPIB device (can contain up to 4 drives)
      }

      if (enable == true)
      {
        initTranslator(select);
                                                                            //  Iterate disk drives - we can have up to 4 drive units per device
        for (JsonVariant unit : hpibDevice["drives"].as<JsonArray>())
        {
          int unitNum = unit["unit"] | 0;                                   //  Drive number 0..7
          const char *diskFname = unit["filepath"] | "";                    //  Disk image filepath
          bool wprot = unit["writeProtect"] | false;
          bool en = unit["enable"] | false;
          if ((devices[device] != NULL) && (en == true) && devices[device]->isType(HPDEV_DISK))
          {
            devices[device]->addDisk((int)type);
            devices[device]->setFile(unitNum, diskFname, wprot);

            temp_char_ptr = log_to_serial_ptr;
            log_to_serial_ptr += sprintf(log_to_serial_ptr, "Add Drive %s to msus$ D:%d%d%d with image file: %s\n", type_text, select, device, unitNum, diskFname);
            LOGPRINTF("%s", temp_char_ptr);

            log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "%d%d%d %s\n", select, device, unitNum, diskFname);
          }
        }
      }
    }

    if (hpibDevice["printer"])                                              //  If the device has a printer section, then it must be a printer (or a disk drive that can print)
    {
      //  int type = hpibDevice["type"] | 0;                                //  Not currently used
      if ((devices[device] == NULL) && (enable == true))                    //  If we haven't already created a printer object, do it now. Although really, doesn't
                                                                            //  this only happen once, during boot. (well ok, also with MOUNT "SDCard")
      {
        devices[device] = new HpibPrint(device, HPDEV_PRT);                 //  Create a new HPIB printer device
      }
      const char *printerFname = hpibDevice["filepath"];                    //  Printer printerFname
      if ((devices[device] != NULL) && (enable == true))                    //  
      {
        devices[device]->setFile((char *)printerFname);
        temp_char_ptr = log_to_serial_ptr;
        log_to_serial_ptr += sprintf(log_to_serial_ptr, "Add Printer: Device %1d%2d with print file: %s\n", select, device, printerFname);
        LOGPRINTF("%s", temp_char_ptr);
        log_to_CRT_ptr    += sprintf(log_to_CRT_ptr, "Printer %1d%2d %s\n", select, device, printerFname);
      }
    }
  }
}


//
//  Loads the configuration from a file, return true if successful
//
//  Scattered throughout the following function you will see text being sent to two logging
//  channels. Because this is being processed while the HP8x is being held in HALT/PWO, the
//  HP I/O bus is not runing yet, and we can't put stuff on the CRT. So we buffer the text
//  that is intended for the CRT until we can send it.
//

#define TRACE_LOAD_CONFIG       (0)           //  Activating this will stall the process until the Serial terminal is connected

bool loadConfiguration(const char *filename)
{
  char fname[258];
  char * temp_char_ptr;
  

  SCOPE_1_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us
  SCOPE_1_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us

  #if TRACE_LOAD_CONFIG
  while (!Serial){};    //  wait till the Virtual terminal is connected
  Serial.begin(115200);
  Serial.printf("\nTracing loadConfiguration()\n"); Serial.flush();
  #endif

  LOGPRINTF("Opening Config File [%s]\n", filename);

  // Open file for reading
  File file = SD.open(filename);
  machineNum = -1;                                                    //  In case we have an early exit, make sure this is an invalid
                                                                      //  value. See enum machine_numbers{} at top of file for valid values.
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 1\n"); Serial.flush();
  #endif

  LOGPRINTF("Open was [%s]\n", file ? "Successful" : "Failed");       //  We need to probably push this to the screen if it fails
                                                                      //  Except this happens before the PWO is released
  if (!file)
  {
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Couldn't open %s\n", filename);
    return false;
  }
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 2\n"); Serial.flush();
  #endif
  //
  //  Allocate a temporary JsonDocument
  //  Don't forget to change the capacity to match your requirements.
  //  Use arduinojson.org/v6/assistant to compute the capacity.
  //
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 3\n"); Serial.flush();
  #endif

  //
  //  Deserialize the JSON document
  //

  DeserializationError error = deserializeJson(doc, file);
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 4\n"); Serial.flush();
  #endif
  file.close();
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 5\n"); Serial.flush();
  #endif
  if (error)
  {
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Config Error: %s\n", filename);
    LOGPRINTF("Failed to read configuration file %s\n", filename);
    return false;
  }
  else
  {
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "%s format OK\n", filename);
    LOGPRINTF("%s format OK\n", filename);
  }
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 6\n"); Serial.flush();
  #endif
  strlcpy (machineType, (doc["machineName"]   | "error") , sizeof(machineType));
  //
  //  look through table to find a match to enumerate the machineType
  //
  for (machineNum = MACH_FIRST_ENTRY ; machineNum < MACH_LAST_ENTRY ; machineNum++)
  {
    if (strcasecmp(machineNames[machineNum], machineType) == 0)
    {
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Series 80 Model: %s\n", machineNames[machineNum]);
      LOGPRINTF("%s", temp_char_ptr);
      break; //found it!
    }
  }
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 7\n"); Serial.flush();
  #endif
  if (MACH_NOT_FOUND)
  {
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Invalid machineName in %s\n", filename);
    LOGPRINTF("%s", temp_char_ptr);
  }

  CRTVerbose    = doc["CRTVerbose"]    | true;

  //
  //  Only allow 16k RAM option for systems that might need it
  //
  enRam16k = false;
  if (ONLY_HAS_16K_RAM)
  {
    enHP85RamExp(doc["ram16k"] | false);
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "HP83, HP85A, 9915A 16 KB RAM:\n");
    LOGPRINTF("%s", temp_char_ptr);
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "  %s\n", getHP85RamExp() ? "Enabled":"None");
    LOGPRINTF("%s", temp_char_ptr);
  }
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 8\n"); Serial.flush();
  #endif
  //
  //  Only allow tape drive emulation for systems that have system ROMs that support tape drives
  //
  tapeEn = false;
  if (SUPPORTS_TAPE_DRIVE)
  {
    //
    //  This reading of tapeEn is only for the following sprintf(). We call mount_drives_based_on_JSON() near the end
    //  of this function, and repeat this setting of tapeEn 
    //
    tapeEn = doc["tape"]["enable"] | false;
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "85A/B, 9915A/B Tape Emulation:\n");
    LOGPRINTF("%s", temp_char_ptr);
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "  %s\n", tapeEn ? "Enabled":"Disabled");
    LOGPRINTF("%s", temp_char_ptr);
  }

  screenEmu     = doc["screenEmu"]     | false;
  LOGPRINTF("Screen Emulation:     %s\n", get_screenEmu() ? "Active" : "Inactive");

  CRTRemote     = doc["CRTRemote"]     | false;

  AutoStartEn = doc["AutoStart"]["enable"] | false;
  initialize_RMIDLE_processing();                       //  Initialize to no text, and pointer points at 0x00
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 10\n"); Serial.flush();
  #endif
  if (AutoStartEn)
  {
    if ((strlen(doc["AutoStart"]["command"] | "") != 0) && (strlen(doc["AutoStart"]["batch"] | "") != 0))
    {
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Error AutoStart Command & Batch\n");
      LOGPRINTF("%s", temp_char_ptr);
      //
      //  Can't have both, so ignore
      //
    }
    else if (strlen(doc["AutoStart"]["command"] | "") != 0)
    {
      load_text_for_RMIDLE(doc["AutoStart"]["command"]);
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "AutoStart with Command\n");
      //  No LOGPRINTF("%s", temp_char_ptr); needed here because it was handled in load_text_for_RMIDLE()
    }
    else if (strlen(doc["AutoStart"]["batch"] | "") != 0)
    {
      if (open_RMIDLE_file(doc["AutoStart"]["batch"]))
      {
        temp_char_ptr = log_to_CRT_ptr;
        log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "AutoStart with Batch file\n");
        LOGPRINTF("%s", temp_char_ptr);
      }
      else
      {
        temp_char_ptr = log_to_CRT_ptr;
        log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "AutoStart with Batch Failed\n");
        LOGPRINTF("%s", temp_char_ptr);        
      }
    }
    else
    {
      //  Sigh, enabled, but both command and batch are empty
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "AutoStart: No Command or Batch\n");
      LOGPRINTF("%s", temp_char_ptr);
    }
  }
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 11\n"); Serial.flush();
  #endif

#if ENABLE_EMC_SUPPORT
  //
  //  EMC Support                         From HP 1983 page PDF 593 and 1984 Catalog, page 592 (PDF 595)
  //
  //  Model       Built in EMC memory     Notes
  //              (beyond the 32KB in the
  //               lower 64K address space)
  //  HP85A       0 * 32 K banks          Only works with IF modification to HP85A I/O bus backplane, and then only
  //                                      provides EDisk (with appropriate ROMS). Probably also 9915A
  //  HP85B       1 * 32 K banks          Makes limited sense, and then only provides EDisk (with appropriate ROMS).
  //                                      HP-catalog-1984 PDF 595 says 32K standard, and 32K for EDisk,
  //                                      544K max, so matches my reality. Probably also 9915B
  //  HP86A       1 * 32 K banks          HP-catalog-1983 PDF 593 says 86  comes with 64KB expandable to 576KB
  //                                      HP-catalog-1984 PDF 595 says 86A comes with 64KB expandable to 576KB, max as Edisk is 416KB
  //                                      So this seems correct: 32KB lower RAM, and 1 32KB of EMC memory
  //  HP86B       3 * 32 K banks          EMC memory can be used for EDisk or as program/data memory
  //                                      HP-catalog-1983 PDF 593 says 86B comes with 128KB expandable to 640KB
  //                                      HP-catalog-1984 PDF 595 says 86B comes with 128KB expandable to 640KB, max as Edisk is 608KB
  //  HP87        0 * 32 K banks          My HP87 only has 32KB RAM (in lower 64K address space)
  //                                      HP-catalog-1983 PDF 594 says 87  comes with 128KB expandable to 640KB
  //                                      which does not match my reality
  //                                      (I think the context is 87XM, and there is no catalog info for the HP87 non XM)
  //  HP87XM      3 * 32 K banks          HP-catalog-1984 PDF 596 says 87XM comes with 128KB expandable to 640KB
  //
  //  With no memory modules plugged in, here are tested EBTKS EMC configs that have worked
  //  machineName   Total             Banks   Start      EBTKSEMC         DISKFREE A,B
  //                installed                 Bank    Start     End         A       B
  //                memory                            Address   Address
  //  ---------------------------------------------------------------------------------
  //  HP85AEMC      16K+16K(EBTKS)    8       2       00200000  01177777   1006    1006       (Philip's 85A #1)
  //  HP85AEMC      16K+16K(EBTKS)    8       2       00200000  01177777   1006    1006       (Philip's 85A #2, no 'IF' mod. See below)
  //  HP85B         64KB              0       3       00000000  00000000    124     124       No EBTKS EMC, just built in 32KB EMC RAM. ( 4 blocks for directory)
  //  HP85B         64KB              8       3       00300000  01277777   1132    1132       32KB+256KB->288KB->1152*256B (so 20 blocks for directory)
  //

  //
  //  Interesting observation with Philip's HP85A #2 which does not have the 'IF' modification
  //  With CONFIG.TXT containing the following (excerpts)
  //
  // "machineName": "HP85AEMC",
  // "ram16k": true,
  // "EMC": {
  //   "enable": true,
  //   "NumBanks": 8,
  //   "StartBank": 2
  //   }
  //
  //  The CAT and DISC FREE commands work, and I wrote a program to ED, and LOADed it back and it worked.
  //  I'm guessing that while IF is needed for program execution out of EMC memory, it is not needed for
  //  simple read and writes that are used by the Electronic Disk software in ROM 321 (octal)
  //  No extensive testing performed, so this might just be luck. Probably should not advertise this
  //  interesting result.  I don't know if this would work with an EMC memory module, since the HP85A
  //  does not have the logic for the root of the automatic configuration logic. See discussion in 
  //  EBTKS_Bus_Interface_ISR.cpp just prior to the start of emc_init()
  //

  EMC_Enable    = doc["EMC"]["enable"] | false;
  EMC_NumBanks  = doc["EMC"]["NumBanks"] | 0;
  EMC_StartBank = doc["EMC"]["StartBank"] | 4;      //  This would be right for a system with 128K DRAM pre-installed  #### need to check
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 12 EMC\n"); Serial.flush();
  #endif

  if ((!SUPPORTS_EMC) && EMC_Enable)
  {
    EMC_Enable = false;         //  If above code enables EMC support based on CONFIG.TXT, disable EMC support if machine does not handle it.
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "EMC enabled in CONFIG.TXT, but\nthis computer does not\nsupport EMC, so disabled\n");
    LOGPRINTF("%s", temp_char_ptr);
    EMC_Enable = false;
  }

  if (EMC_Enable)
  {
    if (EMC_NumBanks > EMC_MAX_BANKS)
    {   //  Greedy user wants more EMC memory than we can provide. Clip it to EMC_MAX_BANKS
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Request for more that %d EMC Banks. Setting EMC Banks to %d\n", EMC_MAX_BANKS, EMC_MAX_BANKS);
      LOGPRINTF("%s", temp_char_ptr);
      EMC_NumBanks = EMC_MAX_BANKS;
    }

    //
    //  Not sure about the following case for disabling, but it avoids possible bugs that might be lurking in having the number of banks == 0
    //
    if (EMC_NumBanks == 0)
    {
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "EMC enabled, but Banks set to 0, so disabling EBTKS EMC\n");
      LOGPRINTF("%s", temp_char_ptr);
      EMC_Enable = false;
    }
    else
    {
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "EMC On. Banks %d to %d = %d kB\n", EMC_StartBank, EMC_StartBank + EMC_NumBanks - 1, EMC_NumBanks * 32);
      LOGPRINTF("%s", temp_char_ptr);

      EMC_StartAddress = EMC_StartBank << 15;
      EMC_EndAddress   = EMC_StartAddress + (EMC_NumBanks << 15) - 1;

      temp_char_ptr = log_to_serial_ptr;
      log_to_serial_ptr += sprintf(log_to_serial_ptr, "EMC enabled. Start at %8o Octal, %d kB\n", EMC_StartAddress, (EMC_NumBanks * 32768)/1024);
      LOGPRINTF("%s", temp_char_ptr);
    }

  EMC_Master = (machineNum == MACH_HP85AEMC);       //  The only time our EMC functionality is the EMC master is on a modified HP85A.
                                                    //  Although, I guess you could modify a HP9951A
  }
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 13 EMC\n"); Serial.flush();
  #endif
  //
  //  EMC configuration checking should be (somehow) checked by the EMC init code, maybe probing memory to see what is there
  //  and make sure the config is valid. Although if this could be done, then autoconfig should also be possible.
  //  I wonder if we could monitor the interrupt priority chain after first /LMA (tricky, because it depend on what
  //  slot EBTKS is in). Could EBTKS participate in the EMC autoconfig dance?
  //
#endif

  SCOPE_1_Pulser(1);         //  From beginning of function to here is 15 ms , measured 2/7/2021  ---  code has changed, need to retime ######
  EBTKS_delay_ns(10000); //  10 us
  SCOPE_1_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us
  SCOPE_1_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 14\n"); Serial.flush();
  #endif

  const char *optionRoms_directory = doc["optionRoms"]["directory"] | "/roms/";
  int romIndex = 0;

  LOGPRINTF("\nLoading ROMs from directory %s\n", optionRoms_directory);
  LOGPRINTF("Name        ID:  Oct Dec  Hex  Compl Sum  Description\n");
  log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "ROM Dir %s\n", optionRoms_directory);

  for (JsonVariant theRom : doc["optionRoms"]["roms"].as<JsonArray>())
  {
    const char *description = theRom["description"] | "no description";
    const char *filename = theRom["filename"] | "norom";
    bool enable = theRom["enable"] | false;

    strcpy(fname, optionRoms_directory);     //  Base directory for ROMs
    strlcat(fname, filename, sizeof(fname)); //  Add the filename
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "%-12s %s", filename, enable ? "On " : "Off");

    if (enable == true)
    {
      LOGPRINTF("%-16s ", filename);
      if (loadRom(fname, romIndex, description) )
      {
        romIndex++;
      }
      SCOPE_1_Pulser(1);                                          //  Loading ROMs takes between 6.5 and 8.5 ms each (more or less), measured 2/7/2021
      //  EBTKS_delay_ns(10000);                                  //  10 us
    }
    else
    {
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "\n");
    }
    
  }
  LOGPRINTF("\n");
  log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "\n");
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 15\n"); Serial.flush();
  #endif

  mount_drives_based_on_JSON(doc);
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 16\n"); Serial.flush();
  #endif

  //
  //  Restore the 4 flag bytes
  //

  if (!(file = SD.open("/AUXROM_FLAGS.TXT", READ_ONLY)))
  {   //  File does not exist, so create it with default contents
failed_to_read_flags:
    log_to_serial_ptr += sprintf(log_to_serial_ptr, "Creating /AUXROM_FLAGS.TXT with default contents of 00000000 because it does not exist\n");
    file = SD.open("/AUXROM_FLAGS.TXT", O_RDWR | O_TRUNC | O_CREAT);
    file.write("00000000\n", 10);
    file.close();
    // AUXROM_RAM_Window.as_struct.AR_FLAGS[0] = 0;   //  load with default 0x      00
    // AUXROM_RAM_Window.as_struct.AR_FLAGS[1] = 0;   //  load with default 0x    00
    // AUXROM_RAM_Window.as_struct.AR_FLAGS[2] = 0;   //  load with default 0x  00
    // AUXROM_RAM_Window.as_struct.AR_FLAGS[3] = 0;   //  load with default 0x00
    AUXROM_RAM_Window.as_struct.AR_FLAGS = 0;
  }
  else
  {
    char      flags_to_read[12];
    uint8_t   chars_read;
    chars_read = file.read(flags_to_read, 10);
    if(chars_read != 10)
    {   //  Something is wrong with the flags file, so rewrite it.
      goto failed_to_read_flags;
    }
    sscanf(flags_to_read, "%8lx", &AUXROM_RAM_Window.as_struct.AR_FLAGS);
    file.close();
  }
  #if TRACE_LOAD_CONFIG
  Serial.printf("\nCheckpoint 17\n"); Serial.flush();
  #endif

  SCOPE_1_Pulser(1);      //  2/7/2021 Time for the whole function, with 7 ROMs loaded is 88ms
  EBTKS_delay_ns(10000);  //  10 us
  SCOPE_1_Pulser(1);
  EBTKS_delay_ns(10000);  //  10 us
  return true;            //  maybe we should be more specific about individual successes and failures. Currently only return false if no SD card
}

//
//  This function is similar to the boot time processing of CONFIG.TXT, except it only processes
//  the mounting of Disks, Tape, and Printer. It uses the same block of code, the function mount_drives_based_on_JSON()
//
//  It is called by the user with
//    MOUNT "SDCARD", "anything"
//  when the SD card has been plugged into a running system.
//  The SD.begin must have already occured
//  It reads the specified file (typically CONFIG.TXT) and only does the file system mounting tasks
//

bool remount_drives(const char *filename)
{
  //
  //  Since we will be calling mount_drives_based_on_JSON() which writes to log_to_CRT_ptr and log_to_serial_ptr
  //  let's reset them so we don't run off the end of the buffer and cause some real damage. We don't actually
  //  need the stuff that is being written to the buffers for a remount, but mount_drives_based_on_JSON() is
  //  used during initial boot, when we do. By the time we get to this function, the system is up and running
  //  doing calculator mode commands or running a BASIC program, so the Serial channel is active, unlike at
  //  boot time. So stuff will be written to the two buffers, but we won't use it.
  //

  log_to_CRT_ptr    = &CRT_Log_Buffer[0];
  log_to_serial_ptr = &Serial_Log_Buffer[0];

  *log_to_CRT_ptr    = 0;
  *log_to_serial_ptr = 0;

  LOGPRINTF("Opening Config File [%s]\n", filename);
  Serial.printf("Opening Config File [%s]\n", filename);

  // Open file for reading
  File file = SD.open(filename);

  LOGPRINTF("Open was [%s]\n", file ? "Successful" : "Failed");
  Serial.printf("Open was [%s]\n", file ? "Successful" : "Failed");
  if (!file)
  {
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity, and add a safety margin. Currently about 1500 bytes
  StaticJsonDocument<JSON_DOC_SIZE> doc;

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error)
  {
    LOGPRINTF("Failed to read configuration file %s during remount_drives()\n", filename);
    return false;
  }
  else
  {
    LOGPRINTF("%s format OK\n", filename);
  }

  mount_drives_based_on_JSON(doc);

  //
  //  Restore the 4 flag bytes
  //

  if (!(file = SD.open("/AUXROM_FLAGS.TXT", READ_ONLY)))
  {   //  File does not exist, so create it with default contents
failed_to_read_flags:
    Serial.printf("Creating /AUXROM_FLAGS.TXT with default contents of 00000000 because it does not exist\n");
    file = SD.open("/AUXROM_FLAGS.TXT", O_RDWR | O_TRUNC | O_CREAT);
    file.write("00000000\n", 10);
    file.close();
    // AUXROM_RAM_Window.as_struct.AR_FLAGS[0] = 0;   //  load with default 0x      00
    // AUXROM_RAM_Window.as_struct.AR_FLAGS[1] = 0;   //  load with default 0x    00
    // AUXROM_RAM_Window.as_struct.AR_FLAGS[2] = 0;   //  load with default 0x  00
    // AUXROM_RAM_Window.as_struct.AR_FLAGS[3] = 0;   //  load with default 0x00
    AUXROM_RAM_Window.as_struct.AR_FLAGS = 0;
  }
  else
  {
    char      flags_to_read[12];
    uint8_t   chars_read;
    chars_read = file.read(flags_to_read, 10);
    if(chars_read != 10)
    {   //  Something is wrong with the flags file, so rewrite it.
      goto failed_to_read_flags;
    }
    sscanf(flags_to_read, "%8lx", &AUXROM_RAM_Window.as_struct.AR_FLAGS);
    file.close();
  }
  return true;        //  Maybe we should be more specific about individual successes and failures. Currently only return false if no SD card
}

void printDirectory(File dir, int numTabs)
{
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++)
    {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory())
    {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    }
    else
    {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
  }
}

/////////////////////////////////////////////// Get CID  ////////////////////////////////////////////////
//
//  6/7/2021
//  No SDCID
// Memory region         Used Size  Region Size  %age Used
//             ITCM:        160 KB       512 KB     31.25%
//             DTCM:      111296 B       512 KB     21.23%
//              RAM:      424672 B       512 KB     81.00%
//            FLASH:      168972 B      7936 KB      2.08%
//             ERAM:      139062 B         8 MB      1.66%
// Checking size .pio\build\teensy41\firmware.elf
// Building .pio\build\teensy41\firmware.hex
// Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
// RAM:   [=====     ]  52.5% (used 275124 bytes from 524288 bytes)
// Flash: [          ]   2.1% (used 168960 bytes from 8126464 bytes)
//
//  SDCID with C++ I/O
// Memory region         Used Size  Region Size  %age Used
//             ITCM:        160 KB       512 KB     31.25%
//             DTCM:      115392 B       512 KB     22.01%    4 kB more
//              RAM:      424672 B       512 KB     81.00%
//            FLASH:      178988 B      7936 KB      2.20%    10 kB more
//             ERAM:      139062 B         8 MB      1.66%
// Checking size .pio\build\teensy41\firmware.elf
// Building .pio\build\teensy41\firmware.hex
// Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
// RAM:   [=====     ]  53.3% (used 279220 bytes from 524288 bytes)
// Flash: [          ]   2.2% (used 178976 bytes from 8126464 bytes)
//
//  SDCID with Serial.printf()
// Memory region         Used Size  Region Size  %age Used
//             ITCM:        160 KB       512 KB     31.25%
//             DTCM:      115392 B       512 KB     22.01%    4 kB more
//              RAM:      424672 B       512 KB     81.00%
//            FLASH:      174044 B      7936 KB      2.14%    5 kB more
//             ERAM:      139062 B         8 MB      1.66%
// Checking size .pio\build\teensy41\firmware.elf
// Building .pio\build\teensy41\firmware.hex
// Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
// RAM:   [=====     ]  53.3% (used 279220 bytes from 524288 bytes)
// Flash: [          ]   2.1% (used 174032 bytes from 8126464 bytes)
//
//
//
//  This is a modified version of an example from the SDFAT library for getting the SD Card
//  Card IDentification (CID) info.
//
//  For more info on the SD Card's "Card IDentification" (CID) see
//    https://www.cameramemoryspeed.com/sd-memory-card-faq/reading-sd-card-cid-serial-psn-internal-numbers/
//
//  Some company MID (manufacturer IDs)
//
//  Company	           MID	  OEMID	      Card brands found with this MID/OEMID
//  Panasonic	      0x000001  PAA         Panasonic
//  Toshiba	        0x000002	TM	        Toshiba
//  SanDisk	        0x000003	SD          (some PT)	SanDisk
//  Samsung	        0x00001b	SM	        ProGrade, Samsung
//  AData	          0x00001d	AD	        AData
//  Phison	        0x000027	PH	        AgfaPhoto, Delkin, Integral, Lexar, Patriot, PNY, Polaroid, Sony, Verbatim
//  Lexar	          0x000028	BE	        Lexar, PNY, ProGrade
//  Silicon Power	  0x000031	SP	        Silicon Power
//  Kingston	      0x000041	42	        Kingston
//  Transcend	      0x000074	JE or J`	  Transcend              Gigastone shows up with0x000074	J`
//  Patriot(?)	    0x000076	??	        Patriot
//  Sony(?)	        0x000082	JT	        Gobe, Sony
//  	              0x00009c	SO	        Angelbird (V60), Hoodman
//  	              0x00009c	BE	        Angelbird (V90)
//
//  What I have seen for EBTKS project
//
//----------------------------------------------------------------    Patriot 16GB New style U1
//  
//  SdFat version: 2.0.2-beta.3
//  Assuming an SDIO interface.
//  
//  Card type: SDHC
//  
//  Manufacturer ID: 0X27                                             Phison
//  OEM ID: PH
//  Product: SD16G
//  Version: 6.0
//  Serial number: 0X4FA91BDA
//  Manufacturing date: 4/2013
//  
//  cardSize: 15502.15 MB (MB = 1,000,000 bytes)
//  flashEraseSize: 128 blocks
//  eraseSingleBlock: true
//  
//  OCR: 0XC0FF8000
//  
//  SD Partition Table
//  part,boot,bgnCHS[3],type,endCHS[3],start,length
//  1,0X0,0X82,0X3,0X0,0XC,0XFE,0XFF,0XFF,8192,30269440
//  2,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  3,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  4,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  
//  Scanning FAT, please wait.
//  
//  Volume is FAT32
//  sectorsPerCluster: @
//  clusterCount:      472832
//  freeClusterCount:  471820
//  fatStartSector:    8994
//  dataStartSector:   16384
//
//----------------------------------------------------------------    SanDisk Ultra 16GB Old style C10
//
//  SdFat version: 2.0.2-beta.3
//  Assuming an SDIO interface.
//  
//  Card type: SDHC
//  
//  Manufacturer ID: 0X3                                              SanDisk
//  OEM ID: SD
//  Product: SB16G
//  Version: 8.0
//  Serial number: 0XDB4E251D
//  Manufacturing date: 4/2014
//  
//  cardSize: 15931.54 MB (MB = 1,000,000 bytes)
//  flashEraseSize: 128 blocks
//  eraseSingleBlock: true
//  
//  OCR: 0XC0FF8000
//  
//  SD Partition Table
//  part,boot,bgnCHS[3],type,endCHS[3],start,length
//  1,0X0,0X82,0X3,0X0,0XC,0XFE,0XFF,0XFF,8192,31108096
//  2,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  3,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  4,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  
//  Scanning FAT, please wait.
//  
//  Volume is FAT32
//  sectorsPerCluster: @
//  clusterCount:      485936
//  freeClusterCount:  484769
//  fatStartSector:    8790
//  dataStartSector:   16384
//  
//----------------------------------------------------------------    SanDisk Ultra PLUS 16GB New style U1
//
//  SdFat version: 2.0.2-beta.3
//  Assuming an SDIO interface.
//  
//  Card type: SDHC
//  
//  Manufacturer ID: 0X3                                              SanDisk
//  OEM ID: SD
//  Product: SL16G
//  Version: 8.0
//  Serial number: 0XA1452875
//  Manufacturing date: 12/2012
//  
//  cardSize: 15931.54 MB (MB = 1,000,000 bytes)
//  flashEraseSize: 128 blocks
//  eraseSingleBlock: true
//  
//  OCR: 0XC0FF8000
//  
//  SD Partition Table
//  part,boot,bgnCHS[3],type,endCHS[3],start,length
//  1,0X0,0X82,0X3,0X0,0XC,0XFE,0XFF,0XFF,8192,31108096
//  2,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  3,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  4,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  
//  Scanning FAT, please wait.
//  
//  Volume is FAT32
//  sectorsPerCluster: @
//  clusterCount:      485936
//  freeClusterCount:  485934
//  fatStartSector:    8790
//  dataStartSector:   16384
//
//----------------------------------------------------------------    Gigastone Full HD Video 16GB New style U1 and older C10 , UHS-I
//
//  SdFat version: 2.0.2-beta.3
//  Assuming an SDIO interface.
//  
//  Card type: SDHC
//  
//  Manufacturer ID: 0x74                                             Transcend , and I guess Gigastone
//  OEM ID: J`
//  Product: SD
//  Version: 0.0
//  Serial number: 0x8303E312
//  Manufacturing date: 4/2015
//  
//  cardSize: 15978.20 MB (MB = 1,000,000 bytes)
//  flashEraseSize: 128 blocks
//  eraseSingleBlock: true
//  
//  OCR: 0XC0FF8000
//  
//  SD Partition Table
//  part,boot,bgnCHS[3],type,endCHS[3],start,length
//  1,0X0,0X82,0X3,0X0,0XC,0XFE,0XFF,0XFF,8192,31199232
//  2,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  3,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  4,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0X0,0,0
//  
//  Scanning FAT, please wait.
//  
//  Volume is FAT32
//  sectorsPerCluster: @
//  clusterCount:      487360
//  freeClusterCount:  486254
//  fatStartSector:    8768
//  dataStartSector:   16384
//
//

#define SD_CARD_CID_QUERY_SUPPORT       (1)

#if SD_CARD_CID_QUERY_SUPPORT

#define SD_CONFIG SdioConfig(DMA_SDIO)

//------------------------------------------------------------------------------
//SdFs sd;
cid_t m_cid;
csd_t m_csd;
uint32_t m_eraseSize;
uint32_t m_ocr;
static ArduinoOutStream cout(Serial);

// //------------------------------------------------------------------------------
//        Original version with C++ stream I/O
// bool cidDmp() {
//   cout << F("\nManufacturer ID: ");
//   cout << uppercase << showbase << hex << int(m_cid.mid) << dec << endl;
//   cout << F("OEM ID: ") << m_cid.oid[0] << m_cid.oid[1] << endl;
//   cout << F("Product: ");
//   for (uint8_t i = 0; i < 5; i++) {
//     cout << m_cid.pnm[i];
//   }
//   cout << F("\nVersion: ");
//   cout << int(m_cid.prv_n) << '.' << int(m_cid.prv_m) << endl;
//   cout << F("Serial number: ") << hex << m_cid.psn << dec << endl;
//   cout << F("Manufacturing date: ");
//   cout << int(m_cid.mdt_month) << '/';
//   cout << (2000 + m_cid.mdt_year_low + 10 * m_cid.mdt_year_high) << endl;
//   cout << endl;
//   return true;
// }

//                    PMF version with Serial.printf()
bool cidDmp()
{
  Serial.printf("\nManufacturer ID: 0x%2X\n", int(m_cid.mid));
  Serial.printf("OEM ID: %c%c\n",  m_cid.oid[0], m_cid.oid[1] );
  Serial.printf("Product: %.5s\n", m_cid.pnm);
  Serial.printf("Version: %d.%d\n", int(m_cid.prv_n), int(m_cid.prv_m));
  Serial.printf("Serial number: 0x%8X\n", m_cid.psn);
  Serial.printf("Manufacturing date: %d/%d\n\n", int(m_cid.mdt_month), (2000 + m_cid.mdt_year_low + 10 * m_cid.mdt_year_high));
  return true;
}

// //------------------------------------------------------------------------------
//        Original version with C++ stream I/O
// bool csdDmp() {
//   bool eraseSingleBlock;
//   if (m_csd.v1.csd_ver == 0) {
//     eraseSingleBlock = m_csd.v1.erase_blk_en;
//     m_eraseSize = (m_csd.v1.sector_size_high << 1) | m_csd.v1.sector_size_low;
//   } else if (m_csd.v2.csd_ver == 1) {
//     eraseSingleBlock = m_csd.v2.erase_blk_en;
//     m_eraseSize = (m_csd.v2.sector_size_high << 1) | m_csd.v2.sector_size_low;
//   } else {
//     cout << F("m_csd version error\n");
//     return false;
//   }
//   m_eraseSize++;
//   cout << F("cardSize: ") << 0.000512 * sdCardCapacity(&m_csd);
//   cout << F(" MB (MB = 1,000,000 bytes)\n");

//   cout << F("flashEraseSize: ") << int(m_eraseSize) << F(" blocks\n");
//   cout << F("eraseSingleBlock: ");
//   if (eraseSingleBlock) {
//     cout << F("true\n");
//   } else {
//     cout << F("false\n");
//   }
//   return true;
// }

//                    PMF version with Serial.printf()
bool csdDmp()
{
  bool eraseSingleBlock;
  if (m_csd.v1.csd_ver == 0)
  {
    eraseSingleBlock = m_csd.v1.erase_blk_en;
    m_eraseSize = (m_csd.v1.sector_size_high << 1) | m_csd.v1.sector_size_low;
  }
  else if (m_csd.v2.csd_ver == 1)
  {
    eraseSingleBlock = m_csd.v2.erase_blk_en;
    m_eraseSize = (m_csd.v2.sector_size_high << 1) | m_csd.v2.sector_size_low;
  }
  else
  {
    Serial.printf("m_csd version error\n");
    return false;
  }
  m_eraseSize++;
  Serial.printf("cardSize: %8.2f MB (MB = 1,000,000 bytes)\n", 0.000512 * sdCardCapacity(&m_csd));
  Serial.printf("flashEraseSize: %d blocks\n", int(m_eraseSize));
  Serial.printf("eraseSingleBlock: %s\n", eraseSingleBlock ? "true":"false");
  return true;
}


// //------------------------------------------------------------------------------
//        Original version with C++ stream I/O
// void errorPrint()
// {
//   if (SD.sdErrorCode())
//   {    
//     cout << F("SD errorCode: ") << hex << showbase;
//     printSdErrorSymbol(&Serial, SD.sdErrorCode());
//     cout << F(" = ") << int(SD.sdErrorCode()) << endl;
//     cout << F("SD errorData = ") << int(SD.sdErrorData()) << endl;
//   }
// }

//                    PMF version with Serial.printf()
void errorPrint()
{
  if (SD.sdErrorCode())
  {    
    Serial.printf("SD errorCode: ");
    printSdErrorSymbol(&Serial, SD.sdErrorCode());
    Serial.printf(" = %d\n", int(SD.sdErrorCode()));
    Serial.printf("SD errorData = %d\n", int(SD.sdErrorData()));
  }
}



// //------------------------------------------------------------------------------
//        Original version with C++ stream I/O
// bool mbrDmp() {
//   MbrSector_t mbr;
//   bool valid = true;
//   if (!SD.card()->readSector(0, (uint8_t*)&mbr)) {
//     cout << F("\nread MBR failed.\n");
//     errorPrint();
//     return false;
//   }
//   cout << F("\nSD Partition Table\n");
//   cout << F("part,boot,bgnCHS[3],type,endCHS[3],start,length\n");
//   for (uint8_t ip = 1; ip < 5; ip++) {
//     MbrPart_t *pt = &mbr.part[ip - 1];
//     if ((pt->boot != 0 && pt->boot != 0X80) ||
//         getLe32(pt->relativeSectors) > sdCardCapacity(&m_csd)) {
//       valid = false;
//     }
//     cout << int(ip) << ',' << uppercase << showbase << hex;
//     cout << int(pt->boot) << ',';
//     for (int i = 0; i < 3; i++ ) {
//       cout << int(pt->beginCHS[i]) << ',';
//     }
//     cout << int(pt->type) << ',';
//     for (int i = 0; i < 3; i++ ) {
//       cout << int(pt->endCHS[i]) << ',';
//     }
//     cout << dec << getLe32(pt->relativeSectors) << ',';
//     cout << getLe32(pt->totalSectors) << endl;
//   }
//   if (!valid) {
//     cout << F("\nMBR not valid, assuming Super Floppy format.\n");
//   }
//   return true;
// }

//                    PMF version with Serial.printf()
bool mbrDmp()
{
  MbrSector_t mbr;
  bool valid = true;
  if (!SD.card()->readSector(0, (uint8_t*)&mbr))
  {
    Serial.printf("\nread MBR failed.\n");
    errorPrint();
    return false;
  }
  Serial.printf("\nSD Partition Table\n");
  Serial.printf("part,boot,bgnCHS[3],type,endCHS[3],start,length\n");
  for (uint8_t ip = 1; ip < 5; ip++)
  {
    MbrPart_t *pt = &mbr.part[ip - 1];
    if ((pt->boot != 0 && pt->boot != 0X80) || getLe32(pt->relativeSectors) > sdCardCapacity(&m_csd))
    {
      valid = false;
    }
    Serial.printf("%d,0x%X,", int(ip), int(pt->boot));
    for (int i = 0; i < 3; i++ )
    {
      Serial.printf("0x%X,", int(pt->beginCHS[i]));
    }
    Serial.printf("0x%X,", int(pt->type));
    for (int i = 0; i < 3; i++ )
    {
      Serial.printf("0x%X,", int(pt->endCHS[i]));
    }
    Serial.printf("%d,%d\n", getLe32(pt->relativeSectors), getLe32(pt->totalSectors));
  }
  if (!valid)
  {
    Serial.printf("\nMBR not valid, assuming Super Floppy format.\n");
  }
  return true;
}

// //------------------------------------------------------------------------------
//        Original version with C++ stream I/O
// void dmpVol() {
//   cout << F("\nScanning FAT, please wait.\n");
//   uint32_t freeClusterCount = SD.freeClusterCount();
//   if (SD.fatType() <= 32) {
//     cout << F("\nVolume is FAT") << int(SD.fatType()) << endl;
//   } else {
//     cout << F("\nVolume is exFAT\n");
//   }
//   cout << F("sectorsPerCluster: ") << SD.sectorsPerCluster() << endl;
//   cout << F("clusterCount:      ") << SD.clusterCount() << endl;
//   cout << F("freeClusterCount:  ") << freeClusterCount << endl;
//   cout << F("fatStartSector:    ") << SD.fatStartSector() << endl;
//   cout << F("dataStartSector:   ") << SD.dataStartSector() << endl;
//   if (SD.dataStartSector() % m_eraseSize) {
//     cout << F("Data area is not aligned on flash erase boundary!\n");
//     cout << F("Download and use formatter from www.sdcard.org!\n");
//   }
// }

//                    PMF version with Serial.printf()
void dmpVol()
{
  Serial.printf("\nScanning FAT, please wait.\n");
  uint32_t freeClusterCount = SD.freeClusterCount();
  if (SD.fatType() <= 32)
  {
    Serial.printf("\nVolume is FAT%d\n", int(SD.fatType()));
  } else {
    Serial.printf("\nVolume is exFAT\n");
  }
  Serial.printf("sectorsPerCluster: %d\n", SD.sectorsPerCluster());
  Serial.printf("clusterCount:      %d\n", SD.clusterCount());
  Serial.printf("freeClusterCount:  %d\n", freeClusterCount );
  Serial.printf("fatStartSector:    %d\n", SD.fatStartSector());
  Serial.printf("dataStartSector:   %d\n", SD.dataStartSector());
  if (SD.dataStartSector() % m_eraseSize)
  {
    Serial.printf("Data area is not aligned on flash erase boundary!\n");
    Serial.printf("Download and use formatter from www.sdcard.org!\n");
  }
}


//------------------------------------------------------------------------------
//        Original version with C++ stream I/O
// void printCardType() {
//   cout << F("\nCard type: ");
//   switch (SD.card()->type()) {
//     case SD_CARD_TYPE_SD1:
//       cout << F("SD1\n");
//       break;
//     case SD_CARD_TYPE_SD2:
//       cout << F("SD2\n");
//       break;
//     case SD_CARD_TYPE_SDHC:
//       if (sdCardCapacity(&m_csd) < 70000000) {
//         cout << F("SDHC\n");
//       } else {
//         cout << F("SDXC\n");
//       }
//       break;
//     default:
//       cout << F("Unknown\n");
//   }
// }

//                    PMF version with Serial.printf()
void printCardType()
{
  Serial.printf("\nCard type: ");
  switch (SD.card()->type())
  {
    case SD_CARD_TYPE_SD1:
      Serial.printf("SD1\n");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.printf("SD2\n");
      break;
    case SD_CARD_TYPE_SDHC:
      if (sdCardCapacity(&m_csd) < 70000000)
      {
        Serial.printf("SDHC\n");
      }
      else
      {
        Serial.printf("SDXC\n");
      }
      break;
    default:
      Serial.printf("Unknown\n");
  }
}

//-----------------------------------------------------------------------------
//        Original version with C++ stream I/O
// void printConfig(SdioConfig config) {
//   (void)config;
//   cout << F("Assuming an SDIO interface.\n");
// }

//                    PMF version with Serial.printf()
void printConfig(SdioConfig config)
{
  (void)config;
  Serial.printf("Assuming an SDIO interface.\n");
}

//-----------------------------------------------------------------------------
//        Original version with C++ stream I/O
// void dump_SD_Card_Info(void)
// {
//   Serial.printf("SdFat version: %s\n", SD_FAT_VERSION);
//   printConfig(SD_CONFIG);
//   // SD Card begin has already occured
//   if (!SD.card()->readCID(&m_cid) ||
//       !SD.card()->readCSD(&m_csd) ||
//       !SD.card()->readOCR(&m_ocr))
//   {
//     cout << F("readInfo failed\n");
//     errorPrint();
//     return;
//   }
//   printCardType();
//   cidDmp();
//   csdDmp();
//   cout << F("\nOCR: ") << uppercase << showbase;
//   cout << hex << m_ocr << dec << endl;
//   if (!mbrDmp())
//   {
//     return;
//   }
//   if (!SD.volumeBegin()) {
//     cout << F("\nvolumeBegin failed. Is the card formatted?\n");
//     errorPrint();
//     return;
//   }
//   dmpVol();
// }

//                    PMF version with Serial.printf()
void dump_SD_Card_Info(void)
{
  Serial.printf("SdFat version: %s\n", SD_FAT_VERSION);
  printConfig(SD_CONFIG);
  // SD Card begin has already occured
  if (!SD.card()->readCID(&m_cid) ||
      !SD.card()->readCSD(&m_csd) ||
      !SD.card()->readOCR(&m_ocr))
  {
    Serial.printf("readInfo failed\n");
    errorPrint();
    return;
  }

  printCardType();
  cidDmp();
  csdDmp();
  Serial.printf("\nOCR: 0x%8X\n", m_ocr);
  if (!mbrDmp())
  {
    return;
  }
  if (!SD.volumeBegin())
  {
    Serial.printf("\nvolumeBegin failed. Is the card formatted?\n");
    errorPrint();
    return;
  }
  dmpVol();
}
#else
void dump_SD_Card_Info(void)
{
  
}
#endif
