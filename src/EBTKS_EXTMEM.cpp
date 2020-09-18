//
//      08/01/2020    Initial creation by Russell
//


#include <Arduino.h>

#include "Inc_Common_Headers.h"

//
//  we need to implement in the bus isr -
//  DRP instruction snooping
//  detect lmard cycle. 'lmard'
//  keep count of multiple rd/wr cycle. lma resets the count, rd/wr increments it. 'idx'
//  current address 'addr'
//

// static constexpr uint32_t EMC_RAM_SIZE  = (65536);        //  'go big or go home'!

// DMAMEM uint8_t emc_ram[EMC_RAM_SIZE];                     //  Place this in the PSRAM

// // Extended RAM access
// uint32_t    emc_ptr1;     //  PTR1 (24 bits)
// uint32_t    emc_ptr2;     //  PTR2 (24 bits)
// uint8_t     emc_disp;     //  Displacement (3 bits)
// bool        emc_mult;     //  Multibyte access
// uint8_t     emc_mode;     //  Mode (3 bits)

// enum
// {
//     EMC_IDLE,
//     EMC_INDIRECT_1,
//     EMC_INDIRECT_2
// };

// int emc_state; // EMC indirect access state
// bool lmard;    // LMARD cycles in progress

// uint32_t &get_ptr(void)
// {
//     return (emc_mode & (1 << 2)) ? emc_ptr2 : emc_ptr1;
// }

// void ptr12_decrement(void)
// {
//     if (emc_mult)
//     {
//         &get_ptr() -= emc_disp;
//     }
//     else
//     {
//         &get_ptr()--;
//     }
// }

// uint8_t emc_r(void)
// {

//     readByte = 0xff; //default the read response

//     if (emc_state == EMC_INDIRECT_2)
//     {
//         uint32_t ptr = get_ptr();

//         if (ptr >= 0x8000 && (ptr - 0x8000) < EMC_RAM_SIZE)
//         {
//             readByte = emc_ram[ptr - 0x8000];
//         }

//         ptr++;
//     }
//     else if (lmard)
//     {
//         emc_mode = uint8_t(addr & 7);

//         // During a LMARD pair, address 0xffc8 is returned to CPU and indirect mode is activated
//         if (idx == 0)
//         {
//             readByte = 0xc8;
//         }
//         else if (idx == 1)
//         {
//             emc_state = EMC_INDIRECT_1;
//             if (emc_mode & 1)
//             {
//                 // Pre-decrement
//                 ptr12_decrement();
//             }
//         }
//     }
//     else
//     {
//         emc_mode = uint8_t(addr & 7);
//         // Read PTRx
//         if (idx < 3)
//         {
//             readByte = uint8_t(get_ptr() >> (8 * idx));
//         }
//     }
//     return true;
// }

// void emc_w(uint8_t data)
// {
//     //auto idx = m_cpu->flatten_burst();

//     if (emc_state == EMC_INDIRECT_2)
//     {

//         uint32_t ptr = get_ptr();

//         if (ptr >= 0x8000 && (ptr - 0x8000) < EMC_RAM_SIZE)
//         {
//             emc_ram[ptr - 0x8000] = data;
//         }
//         ptr++;
//     }
//     else
//     {
//         emc_mode = uint8_t(addr & 7);
//         // Write PTRx
//         if (idx < 3)
//         {
//             uint32_t &ptr = get_ptr();
//             uint32_t mask = 0xffU << (8 * idx);
//             ptr = (ptr & ~mask) | (uint32_t(data) << (8 * idx));
//         }
//     }
// }


// void lma_cycle()
// {
//     lmard = state;
//     if (emc_state == EMC_INDIRECT_1)
//     {
//         emc_state = EMC_INDIRECT_2;
//     }
//     else if (emc_state == EMC_INDIRECT_2)
//     {
//         if (!(emc_mode & 2))
//         {
//             // In PTRx & PTRx- cases, bring the PTR back to start
//             ptr12_decrement();
//         }
//         emc_state = EMC_IDLE;
//     }
// }


// void opcode_cb(uint8_t opcode)
// {
//     // Intercept DRP instructions & load displacement
//     if ((opcode & 0xc0) == 0x40)
//     {
//         if (opcode & (1 << 5))
//         {
//             emc_disp = 8 - (opcode & 7);
//         }
//         else
//         {
//             emc_disp = 2 - (opcode & 1);
//         }
//     }

//     emc_mult = opcode & 1;
// }

