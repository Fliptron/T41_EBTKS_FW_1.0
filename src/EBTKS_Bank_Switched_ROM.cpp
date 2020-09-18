//
//	    06/27/2020	These Function were moved from EBTKS.cpp to here.
//      07/29/2020  Add support for the special RAM window in the AUXROM(s)
//

#include <Arduino.h>
#include <setjmp.h>

#include "Inc_Common_Headers.h"

void ioWriteRSELEC(uint8_t val)                  //  This function is running within an ISR, keep it short and fast.
{
  rselec = val;
  currRom = romMap[val];
}

bool readBankRom(uint16_t addr)                   //  This function is running within an ISR, keep it short and fast.
{                                                 //  addr is in the range 000000 .. 017777
  //
  //  Check for the special RAM window in the AUXROMs
  //

  if((rselec >= AUXROM_PRIMARY_ID) && (rselec <= AUXROM_SECONDARY_ID_END))      //  Testing for Primary AUXROM and all secondaries
  {
    if((addr >= AUXROM_RAM_WINDOW_START) && (addr <= AUXROM_RAM_WINDOW_LAST))   //  Read AUXROM shared RAM window
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


