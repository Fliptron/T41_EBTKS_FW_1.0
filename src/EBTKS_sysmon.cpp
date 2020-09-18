//
//
//  Emulation of the HP 82928A system monitor module
//
//
//
#include <Arduino.h>

#include "Inc_Common_Headers.h"

#define SYSMON_STATUS (0xffdfU)
#define SYSMON_DAT (0xffdeU)

#define SYSMON_STS_HL (1<<0)
#define SYSMON_STS_BKP (1<<1)
#define SYSMON_STS_BKP1EN (1<<4)
#define SYSMON_STS_BKP2EN (1<<5)
#define SYSMON_SYS_MASTEN (1<<7)
