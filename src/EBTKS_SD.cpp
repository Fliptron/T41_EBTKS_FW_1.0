//
//	06/27/2020	All this wonderful code came from Russell.
//

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Inc_Common_Headers.h"

#include "HpibDisk.h"

extern HpibDisk *devices[];

bool loadRom(const char *fname, int slotNum, const char * description)
{
  uint8_t header[10];       //  Why is this 10?  I think it should be 2
  uint8_t id;

  //attempts to read a hp80 rom file from the sdcard using the given filename
  //read the file and check the rom header to verify it is a rom file and extract the id
  // then insert it into the rom emulation system

  if (slotNum > MAX_ROMS)
  {
    LOGPRINTF("ROM slot number too large %d\n", slotNum);
    return false;
  }

  File rfile = SD.open(fname, FILE_READ);

  if (!rfile)
  {
    LOGPRINTF("ROM failed to read %s\n", fname);
    rfile.close();
    return false;
  }
  // validate the rom image. The first two bytes are the id and the complement (except for secondary AUXROMs)
  if (rfile.read(header, 2) != 2)
  {
    LOGPRINTF("ROM file read error %s\n", fname);
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

  id = header[0];
  LOGPRINTF("%03o %3d  %02X   %02X    %02X   ", id, id, id, header[1], (uint8_t)(id + header[1]));
  //
  //  No special ROM loading for Primary AUXROM at 0361.  This code handles the Secondaries from 0362 to 0375
  //
  if((id >= AUXROM_SECONDARY_ID_START) && (id <= AUXROM_SECONDARY_ID_END))   //  Note, not testing Primary AUXROM at ID = 361
  {
    //
    //  Special check for the the non-primary AUXROMs.  Note that these are not recognized by the HP-85 at boot time scan (by design)
    //
    if(header[1] != ((uint8_t)(~id) | (uint8_t)0360))   //  1's complement for 85 A/B.  No current support for 86/87
    {
      LOGPRINTF("Secondary AUXROM file header error %02X %02X\n", id, (uint8_t)header[1]);
      rfile.close();
      return false;
    }
  }
  else if (id != (uint8_t)(~header[1]))   //  now test normal ROM ID's , including the AUXROM Primary.
  {
    LOGPRINTF("ROM file header error %02X %02X\n", id, (uint8_t)~header[1]);
    rfile.close();
    return false;
  }

  //  ROM header looks good, read the ROM file into memory
  rfile.seek(0);    //  Rewind the file
  int flen = rfile.read(&roms[slotNum][0], 8192);
  if (flen != 8192)
  {
    LOGPRINTF("ROM file length error length: %d\n", flen);
    rfile.close();
    return false;
  }

  //  We got this far, update the rom mapping table to say we're here

  romMap[id] = &roms[slotNum][0];

  LOGPRINTF("%-1s\n", description);
  rfile.close();
  return true;
}

//
//  Saves the configuration to a file
//

void saveConfiguration(const char *filename, const Config &config)
{
  // Delete existing file, otherwise the configuration is appended to the file
  SD.remove(filename);

  // Open file for writing
  File file = SD.open(filename, FILE_WRITE);
  if (!file)
  {
    LOGPRINTF("Failed to create file\n");   //  This is probably pointless, since if we can't create a CONFIG.TXT file
                                            //  what is the probability that Log File stuff is working?
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<3000> doc;

  // Set the values in the document
  doc["ram16k"] = config.ram16k;
  doc["screenEmu"] = config.screenEmu;
  

  // option roms

  JsonObject optRoms = doc.createNestedObject("optionRoms");
  optRoms["directory"] = "/roms/";
  JsonArray romx = optRoms.createNestedArray("roms");

  // add roms

  JsonObject rom0 = romx.createNestedObject();
  rom0["description"] = "service rom 340";
  rom0["filename"] = "rom340";
  rom0["enable"] = true;

  JsonObject rom1 = romx.createNestedObject();
  rom1["description"] = "assembler";
  rom1["filename"] = "rom050";
  rom1["enable"] = true;

  JsonObject rom2 = romx.createNestedObject();
  rom2["description"] = "i/o rom";
  rom2["filename"] = "rom300B";
  rom2["enable"] = true;

  JsonObject rom3 = romx.createNestedObject();
  rom3["description"] = "extended mass storage";
  rom3["filename"] = "rom317";
  rom3["enable"] = true;

  JsonObject rom4 = romx.createNestedObject();
  rom4["description"] = "mass storage";
  rom4["filename"] = "rom320B";
  rom4["enable"] = true;

  JsonObject rom5 = romx.createNestedObject();
  rom5["description"] = "Edisk methinks";
  rom5["filename"] = "rom321B";
  rom5["enable"] = true;

  JsonObject rom6 = romx.createNestedObject();
  rom6["description"] = "advanced programming";
  rom6["filename"] = "rom350";
  rom6["enable"] = true;

  JsonObject rom7 = romx.createNestedObject();
  rom7["description"] = "EBTKS AUXROM 1";
  rom7["filename"] = "85aux1.bin";
  rom7["enable"] = true;

  JsonObject rom8 = romx.createNestedObject();
  rom8["description"] = "EBTKS AUXROM 2";
  rom8["filename"] = "85aux2.bin";
  rom8["enable"] = true;

  // hpib devices

  JsonArray hpib = doc.createNestedArray("hpib");
  JsonObject device = hpib.createNestedObject();
  device["type"] = 0;   //disk type - currently an enumeration
  device["device"] = 0; //first device 0..31
  device["directory"] = "/disks/";
  device["enable"] = true;

  JsonArray drives = device.createNestedArray("drives");

  JsonObject drive0 = drives.createNestedObject();
  drive0["unit"] = 0;
  drive0["filename"] = "85Games1.dsk";
  drive0["enable"] = true;
  drive0["writeProtect"] = false;

  JsonObject drive1 = drives.createNestedObject();
  drive1["unit"] = 1;
  drive1["filename"] = "85Games2.dsk";
  drive1["enable"] = true;
  drive1["writeProtect"] = false;

  // tape drive
  JsonObject tape = doc.createNestedObject("tape");
  tape["filename"] = "tape1.tap";
  tape["enable"] = true;
  tape["directory"] = "/tapes/";

  serializeJsonPretty(doc, Serial);

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0)
  {
    LOGPRINTF("Failed to write to file\n");   //  This is probably pointless, since if we can't write to CONFIG.TXT file
                                              //  what is the probability that Log File stuff is working?
  }
  // Close the file
  file.close();
}

//
//  Loads the configuration from a file, return true if successful
//

bool loadConfiguration(const char *filename, Config &config)
{
   bool tapeEmu;

  TXD_Pulser(1);
  EBTKS_delay_ns(10000);    //  10 us
  TXD_Pulser(1);
  EBTKS_delay_ns(10000);    //  10 us

  char fname[256];

  LOGPRINTF("Opening Config File [%s]\n", filename);

  // Open file for reading
  File file = SD.open(filename);

  LOGPRINTF("File handle for config file %d\n", (int)file);

  if(!file)
  {
    //
    //  No SD card, so no tape, no disk, no ROMs
    //
    config.ram16k = false;
    config.screenEmu = false;
    tapeEmu = false;
    tape.enable(tapeEmu);
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<3000> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    saveConfiguration(filename, config);
    LOGPRINTF("Failed to read file, using default configuration\n");
    LOGPRINTF("Failed to read file, using default configuration\n");
  }
  LOGPRINTF("deserializeJson Successful\n");

  // Copy values from the JsonDocument to the Config
  config.ram16k = doc["ram16k"] | false;
  LOGPRINTF("16K RAM for 85A:      %s\n", config.ram16k ? "Active" : "Inactive");
  config.screenEmu = doc["screenEmu"] | false;
  LOGPRINTF("Screen Emulation:     %s\n", config.screenEmu ? "Active" : "Inactive");
  tapeEmu = doc["tape"]["enable"] | false;
  //LOGPRINTF("Tape Drive Emulation: %s\n", config.tapeEmu ? "Active" : "Inactive");

  const char *tapeFname=  doc["tape"]["filename"] | "tape1.tap";
  const char *path = doc["tape"]["directory"] | "/tapes/";
  tape.setPath(path);
  tape.setFile(tapeFname);
  tape.enable(tapeEmu);
  LOGPRINTF("Tape file: %s%s enabled is: %s\n", path, tapeFname, tapeEmu ? "Active" : "Inactive");  
  

  TXD_Pulser(1);                                                                  //  From beginning of function to here is 23 ms
  EBTKS_delay_ns(10000);    //  10 us
  TXD_Pulser(1);
  EBTKS_delay_ns(10000);    //  10 us
  TXD_Pulser(1);
  EBTKS_delay_ns(10000);    //  10 us

  //  New ROM load code......
  const char* optionRoms_directory = doc["optionRoms"]["directory"] | "/roms/";
  int romIndex = 0;

  LOGPRINTF("\nLoading ROMs from directory %s\n", optionRoms_directory);
  LOGPRINTF("Name         ID: Oct Dec Hex  Compl  Sum  Description\n");

  for (JsonVariant theRom : doc["optionRoms"]["roms"].as <JsonArray>())
  {
    const char* description = theRom["description"] | "no description";
    const char* filename = theRom["filename"] | "norom";
    bool enable = theRom["enable"] | false;

    strcpy(fname, optionRoms_directory);              //  Base directory for ROMs
    strlcat(fname, filename, sizeof(fname));          //  Add the filename

    if (enable == true)
    {
      LOGPRINTF("%-16s ", filename);
      if (loadRom(fname, romIndex, description) == true)
      {
        romIndex++;
      }
      TXD_Pulser(1);                                                                //  Loading ROMs takes between 6.5 and 8.5 ms each (more or less)
      //  EBTKS_delay_ns(10000);    //  10 us
    }
  }
  LOGPRINTF("\n");
  //
  // configure the disk drives. currently we only handle one hpib interface at SC 7 (the default)
  //

  for (JsonVariant hpibDevice : doc["hpib"].as <JsonArray>()) //iterate hpib devices on a bus
  {
    int type = hpibDevice["type"] | 0;         //disk drive type 0 = 5 1/4"
    int device = hpibDevice["device"] | 0;       //hpib device number 0..31
    const char *diskDir = hpibDevice["directory"] | "/disk/";    //disk image folder/directory
    bool enable = hpibDevice["enable"] | false;   //are we enabled?

    if (enable == true)
    {
      //iterate disk drives - we can have up to 4 drive units per device
      for (JsonVariant unit: hpibDevice["drives"].as<JsonArray>())
      {
        int unitNum = unit["unit"] | 0;     //drive number 0..3
        const char *filename = unit["filename"]; //disk image filename
        bool wprot = unit["writeProtect"] | false;
        bool en = unit["enable"] | false;

        //form full path/filename
        strcpy(fname, diskDir);  //get path
        strlcat(fname, filename, sizeof(fname)); //append the filename

        if ((devices[device] != NULL) && (en == true))
        {
          devices[device]->addDisk((DISK_TYPE)type);
          devices[device]->setFile(unitNum, fname, wprot);
          LOGPRINTF("Add Drive type: %d to Device: %d as Unit: %d with image file: %s\r\n",type,device,unitNum,fname);
        }
      }
    }
  }

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();

  TXD_Pulser(1);
  EBTKS_delay_ns(10000);    //  10 us
  TXD_Pulser(1);
  EBTKS_delay_ns(10000);    //  10 us
  return true;    //  maybe we should be more specific about individual successes and failures. Currently only return false if no SD card
}

void printDirectory(File dir, int numTabs) 
{
  while(true) 
  {
    File entry =  dir.openNextFile();
    if (! entry)
    {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }
    for (uint8_t i=0; i<numTabs; i++)
    {
    Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory())
    {
      Serial.println("/");
      printDirectory(entry, numTabs+1);
    }
    else
    {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
  }
}

