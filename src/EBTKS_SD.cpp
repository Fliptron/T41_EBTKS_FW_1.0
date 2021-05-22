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

#define JSON_DOC_SIZE   5500

//
//  Parameters from the CONFIG.TXT file
//

int           machineNum;
char          machineType[10];          //  Must be long enough to hold the longest string in machineNames[], including trailing 0x00
bool          CRTVerbose;
bool          screenEmu;
bool          CRTRemote;
bool          AutoStartEn;

#if ENABLE_EMC_SUPPORT
bool          EMC_Enable;
int           EMC_NumBanks = 4;         //  Correct value is 8. If we see only 4 being supported, it either got it from CONFIG.TXT, or initialization failed
int           EMC_StartBank = 5;        //  Correct for systems with pre-installed 128 kB
int           EMC_StartAddress;
int           EMC_EndAddress;
bool          EMC_Master = false;
#endif


//
//  Read machineType string (must match enum machine_numbers {})
//  Valid strings are:
//
const char *machineNames[] = {"HP85A", "HP85B", "HP86A", "HP86B", "HP87", "HP87XM", "HP85AEMC"};

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
    //  HP85A/B      use 1's complement for the ID check byte in header[1]
    //  HP86 or HP87 use 2's complement for the ID check byte in header[1]
    //  Use machineNum 2 through 5 (HP86A/B,  HP87A/XM) to choose between 1's complement in the following test.
    //  Note: For Secondary AUXROMs (which is what we are testing here), the upper nibble is always 0xF0
    //
    //  If HP86/87 (machineNum 2 through 5) adding 1 turns a 1's complement into a 2's complement
    //
    uint8_t comp = ((uint8_t)(~id)) + ((machineNum >= MACH_HP86A) && (machineNum <= MACH_HP87XM) ? 1 : 0);

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
  { //  Check for machine type.   This is going to break for HP83 (so CONFIG.TXT should say 85A) and 99151/B (also 85A ?)
    if ((machineNum == MACH_HP85A) || (machineNum == MACH_HP85B) || (machineNum == MACH_HP85AEMC))
    {                                   //  CONFIG.TXT says we are loading ROMS for HP85A or HP85B:  One's complement for second byte of ROM
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
    else if ((machineNum >= MACH_HP86A) && (machineNum <= MACH_HP87XM))
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
int get_MachineNum(void)
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

void mount_drives_based_on_JSON(JsonDocument& doc)
{
  //char fname[258];
  char * temp_char_ptr;

  bool tapeEn = doc["tape"]["enable"] | false;

  const char *tapeFname = doc["tape"]["filepath"] | "/tapes/tape1.tap";

  if (tapeEn) //only set the filepath if the tape subsystem is enabled
  {
    tape.enable(tapeEn);
    tape.setFile(tapeFname);
  }
  LOGPRINTF("Tape file: %s is: %s\n", tapeFname, tapeEn ? "Enabled" : "Disabled");

  if (tapeEn)
  {
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, ":T  %s\n", tape.getFile());
  }

  //
  //  Configure the disk drives. Currently we only handle one HPIB interface
  //

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

bool loadConfiguration(const char *filename)
{
  char fname[258];
  char * temp_char_ptr;
  

  SCOPE_1_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us
  SCOPE_1_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us

  LOGPRINTF("Opening Config File [%s]\n", filename);

  // Open file for reading
  File file = SD.open(filename);
  machineNum = -1;                                                    //  In case we have an early exit, make sure this is an invalid
                                                                      //  value. See enum machine_numbers{} at top of file for valid values.

  LOGPRINTF("Open was [%s]\n", file ? "Successful" : "Failed");       //  We need to probably push this to the screen if it fails
                                                                      //  Except this happens before the PWO is released
  if (!file)
  {
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Couldn't open %s\n", filename);
    return false;
  }

  //
  //  Allocate a temporary JsonDocument
  //  Don't forget to change the capacity to match your requirements.
  //  Use arduinojson.org/v6/assistant to compute the capacity.
  //
  StaticJsonDocument<JSON_DOC_SIZE> doc;

  //
  //  Deserialize the JSON document
  //

  DeserializationError error = deserializeJson(doc, file);
  file.close();
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

  strlcpy (machineType, (doc["machineName"]   | "error") , sizeof(machineType));
  //
  //  look through table to find a match to enumerate the machineType
  //
  for (machineNum = MACH_HP85A ; machineNum < MACH_NUM ; machineNum++)
  {
    if (strcasecmp(machineNames[machineNum], machineType) == 0)
    {
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Series 80 Model: %s\n", machineNames[machineNum]);
      LOGPRINTF("%s", temp_char_ptr);
      break; //found it!
    }
  }
  if (machineNum == MACH_NUM)
  {
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Invalid machineName in %s\n", filename);
    LOGPRINTF("%s", temp_char_ptr);
    machineNum = -1;                          //  Set it to the invalid state.
  }

  CRTVerbose    = doc["CRTVerbose"]    | true;

  //
  //  Only check for the 16K RAM option if we have an HP85A
  //
  if ((machineNum == MACH_HP85A) || (machineNum == MACH_HP85AEMC))
  {
    enHP85RamExp(doc["ram16k"] | false);
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "HP85A 16 KB RAM: %s\n", getHP85RamExp() ? "Enabled":"None");
    LOGPRINTF("%s", temp_char_ptr);
  }

  //
  //  Only check for the the tape drive if we have an HP85A or HP85B
  //
  if ((machineNum == MACH_HP85A) || (machineNum == MACH_HP85AEMC) || (machineNum == MACH_HP85B))
  {
    bool tapeEn = doc["tape"]["enable"] | false;                                                        //  This access is only for the following sprintf().
    temp_char_ptr = log_to_CRT_ptr;
    log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "HP85A/B Tape Emulation: %s\n", tapeEn ? "Yes":"No");     //  It is repeated in mount_drives_based_on_JSON()
    LOGPRINTF("%s", temp_char_ptr);
  }

  screenEmu     = doc["screenEmu"]     | false;
  LOGPRINTF("Screen Emulation:     %s\n", get_screenEmu() ? "Active" : "Inactive");

  CRTRemote     = doc["CRTRemote"]     | false;

  AutoStartEn = doc["AutoStart"]["enable"] | false;
  initialize_RMIDLE_processing();                       //  Initialize to no text, and pointer points at 0x00
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
      LOGPRINTF("%s", temp_char_ptr);
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

#if ENABLE_EMC_SUPPORT
  //
  //  EMC Support                         From HP 1983 page PDF 593 and 1984 Catalog, page 592 (PDF 595)
  //
  //  Model       Built in EMC memory     Notes
  //              (beyond the 32KB in the
  //               lower 64K address space)
  //  HP85A       0 * 32 K banks          Only works with IF modification to HP85A I/O bus backplane, and then only provides EDisk (with appropriate ROMS)
  //  HP85B       1 * 32 K banks          Makes limited sense, and then only provides EDisk (with appropriate ROMS).
  //                                      HP-catalog-1984 PDF 595 says 32K standard, and 32K for EDisk, 544K max, so matches my reality
  //  HP86A       1 * 32 K banks          HP-catalog-1983 PDF 593 says 86  comes with 64KB expandable to 576KB
  //                                      HP-catalog-1984 PDF 595 says 86A comes with 64KB expandable to 576KB, max as Edisk is 416KB
  //                                      So this seems correct: 32KB lower RAM, and 1 32KB of EMC memory
  //  HP86B       3 * 32 K banks          EMC memory can be used for EDisk or as program/data memory
  //                                      HP-catalog-1983 PDF 593 says 86B comes with 128KB expandable to 640KB
  //                                      HP-catalog-1984 PDF 595 says 86B comes with 128KB expandable to 640KB, max as Edisk is 608KB
  //  HP87        0 * 32 K banks          My HP87 only has 32KB RAM (in lower 64K address space)
  //                                      HP-catalog-1983 PDF 594 says 87  comes with 128KB expandable to 640KB
  //                                      which does not match my reality
  //                                      (I think the context is 87XM, and there is no catalog info for the HP87)
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

  if (EMC_Enable)
  {
    if (machineNum > MACH_HP85A)
    {
      if (EMC_NumBanks > EMC_MAX_BANKS)
      {   //  Greedy user wants more EMC memory than we can provide. Clip it to EMC_MAX_BANKS
        temp_char_ptr = log_to_CRT_ptr;
        log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "Request for more that %d EMC Banks. Setting EMC Banks to %d\n", EMC_MAX_BANKS, EMC_MAX_BANKS);
        LOGPRINTF("%s", temp_char_ptr);
        EMC_NumBanks = EMC_MAX_BANKS;
      }

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
 
      EMC_Master = (machineNum == MACH_HP85AEMC);       //  The only time our EMC functionality is the EMC master is on a modified HP85A
 
     }
    else
    {
      temp_char_ptr = log_to_CRT_ptr;
      log_to_CRT_ptr += sprintf(log_to_CRT_ptr, "EMC enabled in CONFIG.TXT, but\ncomputer is a HP85A which does\nnot support EMC, so disabled\n");
      LOGPRINTF("%s", temp_char_ptr);
      EMC_Enable = false;
    }
  }

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

  mount_drives_based_on_JSON(doc);

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
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<JSON_DOC_SIZE> doc;

  DeserializationError error = deserializeJson(doc, file);
  file.close();
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
