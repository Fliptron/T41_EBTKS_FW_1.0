//
//  All that is needed to act as a usb->serial gateway to program the esp32.
//
//  Notes on using non IDE Firmware downloading for ESP32
//    https://community.platformio.org/t/export-of-binary-firmware-files-for-esp32-download-tool/9253
//    https://github.com/espressif/esptool
//    D:\2021\HP-85_EBTKS_V1.0\ESP32_Binaries\Where_stuff_comes_from.txt    (on Philip's computer)
//
//  Need to install pyserial.py
//    python -m pip install pyserial
//
//  esptool.py is already installed as a by product of setting up platformio for esp32.
//    it can be found here:   C:\Users\Philip Freidin\.platformio\packages\tool-esptoolpy\esptool.py
//    see D:\2021\HP-85_EBTKS_V1.0\ESP32_Binaries\Where_stuff_comes_from.txt  for an example of use
//
//  This comment about the 30 second time out in post 7/12/2021 sources of production firmware
//  shipped to all EBTKS users, is a doc update only. The timeout is part of the 7/12/2021 firmware
//  The 30 second timeout is reset by activity on the USB-to-Serial
//  port which is used by the ESP32 programmer software on a PC, as well as the
//  service console. After 30 seconds of idle, ESP_Programmer_Setup() exits back to
//  the main console command loop. While ESP_Programmer_Setup() is running, the main
//  background polling loop is stalled, anything that depends on being polled will
//  not get serviced. For example: All AUXROM keywords. A BASIC program that does
//  not try and use EBTKS disk/tape emulation, logic analyzer, remote CRT, or AUXROM
//  keywords, should continue running.
//

#include <Arduino.h>
#include "Inc_Common_Headers.h"

void ESP_Prog_Loop(void);

static uint32_t ESP32_link_timeout;

void ESP_Reset(void)
{
  CLEAR_ESP_EN;                   //  Reset the ESP32
  SET_ESP_BOOT;                   //  Establish mode as "Run Firmware"
  delay(10);                      //  Make sure it gets the message
  SET_ESP_EN;                     //  Release Reset and let firmware run.
}

//
//  Show response message ahead of programming
//

void display_initial_ESP32_message(void)
{
  uint32_t ESP32_link_timeout;

  ESP32_link_timeout = systick_millis_count;
  while (1)
  {
    if (Serial2.available())                        // does the ESP32 have anything to say
    {   //  yes
      uint8_t ch = Serial2.read();
      Serial.write(ch);
      ESP32_link_timeout = systick_millis_count;    //  reset timeout
    }
    //
    //  Test if idle for more than 10 ms
    //
    if (ESP32_link_timeout + 10 < systick_millis_count)
    {
      break;                                        //  exit loop if no more stuff from ESP32 for 10 ms
    }
  }
}

//
//      Windows 11 USB Serial driver is broken, and does not maintain the order
//      of changes to DTR and RTS, which is critical to entering Programming mode,
//      or booting into normal ESP32 Firmware.
//      Think of a processor that boasts of "out of order execution" that gets it wrong
//

//
//      Old code that fails because Windows 11 USD driver is crap
//      Now disabled. (this code did work on Windows 7 Pro, with a different USB driver)
#if 0
//
//  translate the usb serial modem control signals (DTR,RTS)
//  to control the ESP32 download mode
//
void usb_ser_rts_dtr(void)
{
    int mask = 0;
    mask |= Serial.rts()? 0 : 1;    //  EN
    mask |= Serial.dtr()? 0 : 2;    //  BOOT

    switch(mask)
    {
        case 0:
        case 3: // do nothing
            digitalWrite(CORE_PIN_ESP_EN,1);
            digitalWrite(CORE_PIN_ESP_BOOT,1);
            break;

        case 1: //rts active
            digitalWrite(CORE_PIN_ESP_EN,1);
            digitalWrite(CORE_PIN_ESP_BOOT,0);
            break;

        case 2: //dtr active
            digitalWrite(CORE_PIN_ESP_EN,0);
            digitalWrite(CORE_PIN_ESP_BOOT,1);
            break;
    }
}
#endif

void ESP_Programmer_Setup(void)
{
  Serial2.begin(115200);
//
//  Make sure the control lines are configured correctly.
//  and put the ESP32 into Reset state, and BOOT line is low, for programming
//
  pinMode(CORE_PIN_ESP_EN,OUTPUT);
  pinMode(CORE_PIN_ESP_BOOT,OUTPUT);

  CLEAR_ESP_BOOT;
  CLEAR_ESP_EN;
  delay(1000);      //  wait a second to make sure that the ESP32 is in reset
  //
  //  Raise the Enable line, leaving BOOT low, which starts programming mode
  //
  SET_ESP_EN;
  //
  //  and then sends a message that should look something like this:
  //
  //    ets Jun  8 2016 00:22:57
  //    
  //    rst:0x1 (POWERON_RESET),boot:0x3 (DOWNLOAD_BOOT(UART0/UART1/SDIO_REI_REO_V2))
  //    waiting for download
  //
  display_initial_ESP32_message();

  Serial.printf("\nReady to disconnect Tera Term and run esptool.py....\r\n");
  ESP32_link_timeout = systick_millis_count;
  //
  //  When the user sees the above message, they should
  //    - disconnect the terminal emulator, so that Teensy can receive the programming data from esptool.py
  //    - start esptool.py with the appropriate parameters. Currently it looks like this:
  //
  //      python esptool.py -p COM4 -b 115200 --chip esp32 write_flash 0x0000  D:\_Multi_Year_Files\EBTKS_V1.0\ESP32_Binaries\Combined\target.bin
  //

  while(1)
  {
    //
    //  Copy data from esptool.py (running on host computer) to the ESP32 via the
    //  Serial2 port, running at 115200 Baud
    //
    ESP_Prog_Loop();
    //
    //  Check for no serial activity for 30 seconds. This indicates the process is complete
    //  or has failed
    //
    if (ESP32_link_timeout + 30000 < systick_millis_count)      //  30000 milliseconds is 30 seconds.
    {
      Serial.printf("\nExit ESP32 programming link due to 30 second timeout\n");
      //
      //  Restart the ESP32 in "Running Firmware" mode
      //
      ESP_Reset();
      break;
    }
  }
}

//
//  copy data from esptool.py (via T4.1 serial-over-USB port)
//  to the ESP32-WROOM-32D (or E)
//  Data is passed in both directions
//  esptool.py reports progress on the host system's screen
//  T4.1 is unable to report status, as the serial-over-USB port is in use
//

void ESP_Prog_Loop()
{
  //
  //  First see if a character has arrived from esptool.py (running on host PC)
  //
  if (Serial.available())
    {
      //
      //  Yes, so copy it to the ESP32, and reset the timeout
      //
      uint8_t ch = Serial.read();
      Serial2.write(ch);
      ESP32_link_timeout = systick_millis_count;
    }
  //
  //  Second, see if ESP32 is trying to send any info back to esptool.py
  //
  if (Serial2.available())
  {
    //
    //  Yes, so copy it serial port back to host (to esptool.py)
    //  no resetting of timeout
    //
    uint8_t xch = Serial2.read();
    Serial.write(xch);
  }
}

