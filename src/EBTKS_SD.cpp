//
//	06/27/2020	All this wonderful code came from Russell.
//

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Inc_Common_Headers.h"

#include "HpibDisk.h"
#include "HpibPrint.h"

extern HpibDevice *devices[];

int machineNum = 0;
uint32_t Config_Flag_word;

char * boot_log_ptr;

bool loadRom(const char *fname, int slotNum, const char *description)
{
  uint8_t header[10]; //  Why is this 10?  I think it should be 2
  uint8_t id;

  //  boot_log_ptr should already be setup, but just in case it isn't we set it up here, so that we don't have writes going somewhere dangerous
  if (boot_log_ptr == NULL)
  {
    boot_log_ptr = Directory_Listing_Buffer_for_SDDEL;
  }

  //attempts to read a hp80 rom file from the sdcard using the given filename
  //read the file and check the rom header to verify it is a rom file and extract the id
  // then insert it into the rom emulation system

  if (slotNum > MAX_ROMS)
  {
    LOGPRINTF("ROM slot number too large %d\n", slotNum);
    boot_log_ptr += sprintf(boot_log_ptr, "Slot error\n");
    return false;
  }

  File rfile = SD.open(fname, FILE_READ);

  if (!rfile)
  {
    LOGPRINTF("ROM failed to Open %s\n", fname);
    boot_log_ptr += sprintf(boot_log_ptr, "Can't Open ROM File\n");
    rfile.close(); //  Is this right??? if it failed to open, and rfile is null (0) , the what would this statement do???
    return false;
  }
  // validate the rom image. The first two bytes are the id and the complement (except for secondary AUXROMs)
  // but to support the way we are handling HP86/87, we read 3 bytes. See comment below that starts "No special ROM loading for Primary AUXROM at 0361. ....."
  if (rfile.read(header, 3) != 3)
  {
    LOGPRINTF("ROM file read error %s\n", fname);
    boot_log_ptr += sprintf(boot_log_ptr, "Can't read ROM");
    rfile.close();
    return false;
  }

  //
  //  Nothing special for normal HP-85 ROMs, first byte at 060000 is the ROM ID, next byte is the complement. The sum is always 0377  (0xFF)
  //    For HP-86 and 87 it is the twos complement, and the sum is always 0.    ***We do not handle this yet***
  //    For AUXROMs, there is a Primary AUXROM, with normal two byte header of 0361 0016  (0xF1  and 0x0E)
  //    For Secondary AUXROMs, the first byte is 0362 to 0375. The second byte is complemented normally (85: 1's or 86/87: 2's) then ORed with 0360.
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
  //  208   Mass Storage
  //

  id = header[0];
  LOGPRINTF("%03o %3d  %02X   %02X    %02X   ", id, id, id, header[1], (uint8_t)(id + header[1]));
  if(id < 0362)
  {
    boot_log_ptr += sprintf(boot_log_ptr, " ID: %03o ", id);    //  Primary AUXROM and normal ROMs have ID in first byte
  }
  else
  {
    boot_log_ptr += sprintf(boot_log_ptr, " ID: %03o ", header[1]);    //  Secondary AUXROMs have ID in second byte
  }
  
  //
  //  No special ROM loading for Primary AUXROM at 0361.  This code handles the Secondaries from 0362 to 0375
  //
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
  //
  if (id==0377)
  {
    id = header[0] = header[1];
    header[1] = header[2];
    //  Serial.printf("AUX HEADER ROM# %03o\n", id);
  }
  if ((id >= AUXROM_SECONDARY_ID_START) && (id <= AUXROM_SECONDARY_ID_END)) //  Note, not testing Primary AUXROM at ID = 361
  {
    //
    //  Special check for the the non-primary AUXROMs.  Note that these are not recognized by the HP-85 at boot time scan (by design)
    //
    //  HP85A/B      use 1's complement for the ID check byte in header[1]
    //  HP86 or HP87 use 2's complement for the ID check byte in header[1]
    //  Use machineNum 2 or 3 (HP86 or HP87) to choose between 1's complement in the following test.
    //  Note: For Secondary AUXROMs (which is what we are testing here), the upper nibble is always 0xF0
    //
    //  If HP86/87 (machineNum 2 or 3) adding 1 turns a 1's complement into a 2's complement
    //
    uint8_t comp = ((uint8_t)(~id)) + ((machineNum==2 || machineNum==3) ? 1 : 0);

    if (header[1] != (comp | (uint8_t)0360)) //  Check the ID check byte
    {
      LOGPRINTF("Secondary AUXROM file header error %02X %02X\n", id, (uint8_t)header[1]);
      boot_log_ptr += sprintf(boot_log_ptr, "HeaderError\n");
      rfile.close();
      return false;
    }
  }
  else
  { //  Check for machine type.   This is going to break for HP83 (so CONFIG.TXT should say 85A) and 99151/B (also 85A ?)
    if ((machineNum == 0) || (machineNum == 1))
    {                                  //  CONFIG.TXT says we are loading ROMS for HP85A or HP85B:  One's complement for second byte of ROM
      if (id != (uint8_t)(~header[1])) //  now test normal ROM ID's , including the AUXROM Primary.
      {
        LOGPRINTF("ROM file header error first two bytes %02X %02X   Expect One's Complement\n", id, (uint8_t)header[1]);
        boot_log_ptr += sprintf(boot_log_ptr, "HeaderError\n");
        rfile.close();
        return false;
      }
    }
    else if ((machineNum == 2) || (machineNum == 3))
    {                                  //  CONFIG.TXT says we are loading ROMS for HP86 or HP87:  Two's complement for second byte of ROM
      if (id != (uint8_t)(-header[1])) //  now test normal ROM ID's , including the AUXROM Primary.
      {
        LOGPRINTF("ROM file header error first two bytes %02X %02X   Expect Two's Complement\n", id, (uint8_t)header[1]);
        boot_log_ptr += sprintf(boot_log_ptr, "HeaderError\n");
        rfile.close();
        return false;
      }
    }
    else
    {
      LOGPRINTF("ERROR Unsupported Machine type\n");
      boot_log_ptr += sprintf(boot_log_ptr, "Mach???\n");
    }
  }

  //  ROM header looks good, read the ROM file into memory
  rfile.seek(0); //  Rewind the file
  int flen = rfile.read(getRomSlotPtr(slotNum), ROM_PAGE_SIZE);
  if (flen != ROM_PAGE_SIZE)
  {
    LOGPRINTF("ROM file length error length: %d\n", flen);
    boot_log_ptr += sprintf(boot_log_ptr, "Length Error\n");
    rfile.close();
    return false;
  }

  //  We got this far, update the rom mapping table to say we're here
  boot_log_ptr += sprintf(boot_log_ptr, "OK\n");
  setRomMap(id, slotNum);

  LOGPRINTF("%-1s\n", description);
  rfile.close();
  return true;
}

//
//  Saves the configuration to a file
//

void saveConfiguration(const char *filename)
{
  // Delete existing file, otherwise the configuration is appended to the file
  SD.remove(filename);

  // Open file for writing
  File file = SD.open(filename, FILE_WRITE);
  if (!file)
  {
    LOGPRINTF("Failed to create file\n"); //  This is probably pointless, since if we can't create a CONFIG.TXT file
                                          //  what is the probability that Log File stuff is working?
    return;
  }

  //
  //  Allocate a temporary JsonDocument
  //  Don't forget to change the capacity to match your requirements.
  //  Use arduinojson.org/assistant to compute the capacity.
  //
  //  On 10/1/2020, report is 1720
  //

  StaticJsonDocument<3000> doc;

  Serial.printf("\n\nError reading CONFIG.TXT   Recreating default Configuration\n\n");

  // Set the values in the document
  doc["machineName"] = "HP85A";
  doc["flags"] = 173368985; //  0x0A556699    Hex constants are not allowed with this SdFat library
  doc["ram16k"] = false;    //  For safety, since we don't know what machine we are plugged into
  doc["screenEmu"] = false;

  // tape drive

  JsonObject tape = doc.createNestedObject("tape");
  tape["enable"] = false; //  For safety, since we don't know what machine we are plugged into
  tape["filename"] = "tape1.tap";
  tape["directory"] = "/tapes/";

  // option roms

  JsonObject optRoms = doc.createNestedObject("optionRoms");
  optRoms["directory"] = "/roms/";
  JsonArray romx = optRoms.createNestedArray("roms");

  // add roms

  JsonObject rom0 = romx.createNestedObject();
  rom0["description"] = "Service ROM 340 AUXROM Aware";
  rom0["filename"] = "340aux.bin";
  rom0["enable"] = true;

  JsonObject rom1 = romx.createNestedObject();
  rom1["description"] = "Assembler ROM";
  rom1["filename"] = "rom050";
  rom1["enable"] = true;

  JsonObject rom2 = romx.createNestedObject();
  rom2["description"] = "I/O ROM";
  rom2["filename"] = "rom300B";
  rom2["enable"] = true;

  JsonObject rom3 = romx.createNestedObject();
  rom3["description"] = "Extended Mass Storage ROM 85B variant";
  rom3["filename"] = "ROM317.85B";
  rom3["enable"] = true;

  JsonObject rom4 = romx.createNestedObject();
  rom4["description"] = "Mass Storage";
  rom4["filename"] = "320RevB.bin";
  rom4["enable"] = true;

  JsonObject rom5 = romx.createNestedObject();
  rom5["description"] = "EDisk";
  rom5["filename"] = "rom321B";
  rom5["enable"] = true;

  JsonObject rom6 = romx.createNestedObject();
  rom6["description"] = "Advanced Programming";
  rom6["filename"] = "rom350";
  rom6["enable"] = true;

  JsonObject rom7 = romx.createNestedObject();
  rom7["description"] = "AUXROM Primary 2020_09_28";
  rom7["filename"] = "85aux1.bin";
  rom7["enable"] = true;

  JsonObject rom8 = romx.createNestedObject();
  rom8["description"] = "AUXROM Secondary 1 2020_09_28";
  rom8["filename"] = "85aux2.bin";
  rom8["enable"] = true;

  JsonObject rom9 = romx.createNestedObject();
  rom9["description"] = "AUXROM Secondary 2 2020_09_28";
  rom9["filename"] = "85aux3.bin";
  rom9["enable"] = true;

  // hpib devices

  JsonArray hpib = doc.createNestedArray("hpib");
  JsonObject device = hpib.createNestedObject();
  device["select"] = 7; //  HPIB Select code
  device["type"] = 0;   //  Disk type - currently an enumeration
  device["device"] = 0; //  First device 0..31
  device["directory"] = "/disks/";
  device["enable"] = true;

  JsonArray drives = device.createNestedArray("drives");

  JsonObject drive0 = drives.createNestedObject();
  drive0["unit"] = 0;
  drive0["filename"] = "85Games1.dsk";
  drive0["writeProtect"] = false;
  drive0["enable"] = true;

  JsonObject drive1 = drives.createNestedObject();
  drive1["unit"] = 1;
  drive1["filename"] = "85Games2.dsk";
  drive1["writeProtect"] = false;
  drive1["enable"] = true;

  JsonObject printer = hpib.createNestedObject();
  device["select"] = 7; //  HPIB Select code
  device["type"] = 0;   //  printer type - currently an enumeration
  device["device"] = 10; // device 0..31
  device["directory"] = "/printers/";
  device["enable"] = true;
  JsonObject printer0 = printer.createNestedObject("printer");
  printer0["filename"] = "printerFile.txt";

  serializeJsonPretty(doc, Serial);

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0)
  {
    LOGPRINTF("Failed to write to file\n"); //  This is probably pointless, since if we can't write to CONFIG.TXT file
                                            //  what is the probability that Log File stuff is working?
  }
  // Close the file
  Serial.printf("\n\n");
  file.close();
}

//
//  returns the enumeration of the machine name selected in the config file
//
int getMachineNum(void)
{
  return machineNum;
}

uint32_t getFlags(void)
{
  return Config_Flag_word;
}

//
//  Loads the configuration from a file, return true if successful
//

bool loadConfiguration(const char *filename)
{
  TXD_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us
  TXD_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us

  char fname[258];
  
  boot_log_ptr = Directory_Listing_Buffer_for_SDDEL;            //  use this buffer during boot (when it can't be in use) to log messages to be displayed once system has started

  //  boot_log_ptr += sprintf(boot_log_ptr, "Boot Log\n");

  LOGPRINTF("Opening Config File [%s]\n", filename);

  // Open file for reading
  File file = SD.open(filename);

  LOGPRINTF("Open was [%s]\n", file ? "Successful" : "Failed"); //  We need to probably push this to the screen if it fails
                                                                //  Except this happens before the PWO is released
  if (!file)
  {
    boot_log_ptr += sprintf(boot_log_ptr, "Couldn't open %s\n", filename);
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<5000> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    file.close();
    boot_log_ptr += sprintf(boot_log_ptr, "deserializeJson failed\n");

    saveConfiguration(filename);                                                        //  ####  This is dangerous, since we don't know 85/86/87  ####
    LOGPRINTF("Failed to read file, using default configuration\n");
    //
    //  Try number 2:  Read the file we hopefully just wrote
    //
    file = SD.open(filename);
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    { //  Failed a second time, give up
      boot_log_ptr += sprintf(boot_log_ptr, "deserializeJson failed twice\n");
      LOGPRINTF("Failed to read (or write) the default configuration. Giving up\n");
      return false;
    }
    //  else fall into the normal config processing with the defaults we just created
  }

  Config_Flag_word = doc["flags"] | 0;

  // read machineType string
  // valid strings are:
  const char *machineNames[] = {"HP85A", "HP85B", "HP86", "HP87"};

  const char *machineType = doc["machineName"] | "HP85A";
  //
  //  look through table to find a match to enumerate the machineType
  //

  machineNum = 0;
  while (machineNum < MACH_NUM)
  {
    if (strcmp(machineNames[machineNum], machineType) == 0)
    {
      break; //found it!
    }
    machineNum++;
  }

  boot_log_ptr += sprintf(boot_log_ptr, "Mach:%1d ", machineNum);

  enHP85RamExp(doc["ram16k"] | false);
  boot_log_ptr += sprintf(boot_log_ptr, "RAM:%c ", getHP85RamExp() ? 'T':'F');
  //bool enScreenEmu = doc["screenEmu"] | false;
  bool tapeEn = doc["tape"]["enable"] | false;
  boot_log_ptr += sprintf(boot_log_ptr, "Tape:%c\n", tapeEn ? 'T':'F');

  // Copy values from the JsonDocument to the Config

  LOGPRINTF("16K RAM for 85A:      %s\n", (doc["ram16k"] | false) ? "Active" : "Inactive");

  LOGPRINTF("Screen Emulation:     %s\n", (doc["screenEmu"] | false) ? "Active" : "Inactive");

  //LOGPRINTF("Tape Drive Emulation: %s\n", tapeEn ? "Active" : "Inactive");

  const char *tapeFname = doc["tape"]["filename"] | "tape1.tap";
  const char *path = doc["tape"]["directory"] | "/tapes/";

  if (tapeEn) //only set the path/filename if the tape subsystem is enabled
  {
    tape.enable(tapeEn);
    strcpy(fname, path);
    strlcat(fname, tapeFname, sizeof(fname));
    tape.setFile(fname);
  }
  LOGPRINTF("Tape file: %s%s enabled is: %s\n", path, tapeFname, tapeEn ? "Active" : "Inactive");

  TXD_Pulser(1);         //  From beginning of function to here is 23 ms
  EBTKS_delay_ns(10000); //  10 us
  TXD_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us
  TXD_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us

  //  New ROM load code......
  const char *optionRoms_directory = doc["optionRoms"]["directory"] | "/roms/";
  int romIndex = 0;

  LOGPRINTF("\nLoading ROMs from directory %s\n", optionRoms_directory);
  LOGPRINTF("Name         ID: Oct Dec Hex  Compl  Sum  Description\n");
  boot_log_ptr += sprintf(boot_log_ptr, "ROM Dir %s\n", optionRoms_directory);

  for (JsonVariant theRom : doc["optionRoms"]["roms"].as<JsonArray>())
  {
    const char *description = theRom["description"] | "no description";
    const char *filename = theRom["filename"] | "norom";
    bool enable = theRom["enable"] | false;

    strcpy(fname, optionRoms_directory);     //  Base directory for ROMs
    strlcat(fname, filename, sizeof(fname)); //  Add the filename
    boot_log_ptr += sprintf(boot_log_ptr, "%-12s en:%c", filename, enable ? 'T':'F');

    if (enable == true)
    {
      LOGPRINTF(" %-16s ", filename);
      if (loadRom(fname, romIndex, description) == true)
      {
        romIndex++;
      }
      TXD_Pulser(1); //  Loading ROMs takes between 6.5 and 8.5 ms each (more or less)
      //  EBTKS_delay_ns(10000);    //  10 us
    }
    else
    {
      boot_log_ptr += sprintf(boot_log_ptr, "\n");
    }
    
  }
  LOGPRINTF("\n");
  //
  // configure the disk drives. currently we only handle one hpib interface
  //

  //
  //  Although we don't check it and report an error if necessary, the following things      ####  should we add checks ? ####
  //  MUST be true of the HPIB device configuration comming from the CONFIG.TXT file
  //
  //    1) The "select" fields must all have the same value 3..10 as we can only emulate 1 HPIB interface
  //    2) The "device" fields must be different, and should probably start at 0 and go no higher than 7
  //

  for (JsonVariant hpibDevice : doc["hpib"].as<JsonArray>()) //iterate hpib devices on a bus
  {
    int select = hpibDevice["select"] | 7;      //  1MB5 select code (3..10). 3 is the default
    int device = hpibDevice["device"] | 0;      //  HPIB device number 0..31 (can contain up to 4 drives)
    bool enable = hpibDevice["enable"] | false; //are we enabled?

    //  boot_log_ptr += sprintf(boot_log_ptr, "HPIB Sel %d Dev %d En %d\n", select, device, enable);

    if (hpibDevice["drives"]) //if the device is a disk drive
    {
      const char *diskDir = hpibDevice["directory"] | "/disks/"; //disk image folder/directory
      int type = hpibDevice["type"] | 0;                         //  disk drive type 0 = 5 1/4"

      if ((devices[device] == NULL) && (enable == true))
      {
        devices[device] = new HpibDisk(device); //  create a new HPIB device (can contain up to 4 drives)
      }

      if (enable == true)
      {
        initTranslator(select);
        //  Iterate disk drives - we can have up to 4 drive units per device
        for (JsonVariant unit : hpibDevice["drives"].as<JsonArray>())
        {
          int unitNum = unit["unit"] | 0;             //  Drive number 0..3
          const char *filename = unit["filename"];    //  Disk image filename
          bool wprot = unit["writeProtect"] | false;
          bool en = unit["enable"] | false;

          //form full path/filename
          strcpy(fname, diskDir);                  //get path
          strlcat(fname, filename, sizeof(fname)); //append the filename
          if ((devices[device] != NULL) && (en == true))
          {
            devices[device]->addDisk((int)type);
            devices[device]->setFile(unitNum, fname, wprot);
            LOGPRINTF("Add Drive type: %d to Device: %d as Unit: %d with image file: %s\r\n", type, device, unitNum, fname);
            boot_log_ptr += sprintf(boot_log_ptr, "%d%d%d %s\n", select, device, unitNum, fname);
          }
        }
      }
    }

    if (hpibDevice["printer"]) //if the device is a printer
    {
      int type = hpibDevice["type"] | 0;
      const char *printDir = hpibDevice["directory"] | "/printers/"; //printer folder/directory

      if ((devices[device] == NULL) && (enable == true))
      {
        devices[device] = new HpibPrint(device); //  create a new HPIB printer device 
      }
      JsonObject printer = hpibDevice["printer"];
      const char *filename = printer["filename"]; //printer filename

      //form full path/filename
      strcpy(fname, printDir);                  //get path
      strlcat(fname, filename, sizeof(fname)); //append the filename
      if ((devices[device] != NULL) && (enable == true))
      {
        devices[device]->setFile((char *)fname);
        LOGPRINTF("Add Printer type: %d to Device: %d with print file: %s\r\n", type, device, fname);
        Serial.printf("Add Printer type: %d to Device: %d with print file: %s\r\n", type, device, fname);
      }
    }
  }

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();

  //
  //  Restore the 4 flag bytes
  //

  if (!(file = SD.open("/AUXROM_FLAGS.TXT", READ_ONLY)))
  {   //  File does not exist, so create it with default contents
failed_to_read_flags:
    Serial.printf("Creating /AUXROM_FLAGS.TXT with default contents of 00000000 because it does not exist\n");
    file = SD.open("/AUXROM_FLAGS.TXT", O_RDWR | O_TRUNC | O_CREAT);
    file.write("00000000\r\n", 10);
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
  
  TXD_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us
  TXD_Pulser(1);
  EBTKS_delay_ns(10000); //  10 us
  return true;           //  maybe we should be more specific about individual successes and failures. Currently only return false if no SD card
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


bool      boot_message_displayed = false;

void Boot_Message_Poll(void)
{
  int row, column;
  int seg_length;
  char segment[100];

  if(boot_message_displayed)        //  If Boot message has been displayed, we don't do it ever again (untill next boot)
  {
    return;
  }

  if (addReg != 000072)             //  Sneaky check to see if we are in the EXEC loop in the system ROM
  {                                 //  Hold off displaying boot message untill we are in the EXEC loop
    return;                         //  We may miss the first occurrence, but we should see it pretty quickly
  }

  //  Write_on_CRT_Alpha(8, 0, "Please wait, checking ROMs");     //  Haven't figured out how to display this once at the right time,
  //                                                                  without interfeering with the HP85 boot up and ROM check process.

  boot_log_ptr = Directory_Listing_Buffer_for_SDDEL;
  if (strlen(boot_log_ptr) == 0)
  {
    return;
  }
  Serial.printf("\n\nDump of Boot Log\n");
  row = 1;
  column=0;
  //first_char[0] = (*boot_log_ptr) +128;                //  Add an undersdscore
  //first_char[1] = 0;

  while (*boot_log_ptr)
  {
    seg_length = strcspn(boot_log_ptr,"\n");        //  Number of chars not in second string, so does not count the '\n'
    strlcpy(segment, boot_log_ptr, seg_length+1);   //  Copies the segment, and appends a 0x00
    boot_log_ptr += seg_length+1;                   //  Skip over the segment and the trailing '\n'
    Serial.printf("[%s]\n", segment);
    Serial.flush();
    Write_on_CRT_Alpha(row++, column, segment);
    // delay(50);                                   //  A little delay so they see it happening
    if(row > 63)
    {
      row = 63;                                     //  Just over-write the last line. Note we start on line 1, so max lines is 63. We leave line 0 alone.
    }
  }
  // Write_on_CRT_Alpha(0,0, first_char);
  while (DMA_Peek8(CRTSTS) & 0x80) {}; //wait until video controller is ready
  DMA_Poke16(CRTBAD, 0);
  while (DMA_Peek8(CRTSTS) & 0x80) {}; //wait until video controller is ready
  DMA_Poke16(CRTSAD, 0);

  boot_message_displayed = true;

}


