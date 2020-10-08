//
//	    06/27/2020	These Function were moved from EBTKS.cpp to here.
//      07/29/2020  Add support for the special RAM window in the AUXROM(s)
//

#include <Arduino.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include "Inc_Common_Headers.h"

volatile uint8_t rselec = 0;          //holds rom ID currently selected
uint8_t *currRom;                     //pointer to the currently selected rom data. NULL if not selected
DMAMEM uint8_t roms[MAX_ROMS][ROM_PAGE_SIZE];    //  Array to store the rom images loaded from SD card
uint8_t *romMap[256];                  //  Hold the pointers to the rom data based on ID as the index


void ioWriteRSELEC(uint8_t val)                  //  This function is running within an ISR, keep it short and fast.
{
  rselec = val;
  currRom = romMap[val];
}

void initRoms(void)
{
  memset(roms,0,sizeof(roms));
  setIOWriteFunc(RSELEC & 0xff,&ioWriteRSELEC);      // rom select register for banked roms
}

uint8_t getRselec(void)
{
  return rselec;

}

void setRomMap(uint8_t romId,uint8_t slotNum)
{
  romMap[romId] = &roms[slotNum][0];
}

uint8_t * getROMEntry(uint8_t romId)
{
  return romMap[romId];
}

uint8_t *getRomSlotPtr(int slotNum)
{
  return &roms[slotNum][0];
}


bool readBankRom(uint16_t addr)                   //  This function is running within an ISR, keep it short and fast.
{                                                 //  addr is in the range 000000 .. 017777
  //
  //  Check for the special RAM window in the AUXROMs
  //

  if ((rselec >= AUXROM_PRIMARY_ID) && (rselec <= AUXROM_SECONDARY_ID_END))      //  Testing for Primary AUXROM and all secondaries
  {
    if ((addr >= AUXROM_RAM_WINDOW_START) && (addr <= AUXROM_RAM_WINDOW_LAST))   //  Read AUXROM shared RAM window
    {
      readData = AUXROM_RAM_Window.as_bytes[addr - AUXROM_RAM_WINDOW_START];
      return true;
    }
  }

  //
  //  Normal ROM read for all normal ROMs, and the remainder of the AUXROM(s)
  //

  if (currRom)
  {
    readData = currRom[addr];
    return true;
  }
  return false;
}


