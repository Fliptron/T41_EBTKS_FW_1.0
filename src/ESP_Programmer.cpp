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

//#define PIN_ESP_EN (9)
//#define PIN_ESP_BOOT (10)
#define PIN_ESP_EN (CORE_PIN_ESP_EN)
#define PIN_ESP_BOOT (CORE_PIN_ESP_BOOT)

void ESP_Reset(void)
{
  digitalWrite(PIN_ESP_EN,0);
  digitalWrite(PIN_ESP_BOOT,1);   //keep boot high
  delay(10);
  digitalWrite(PIN_ESP_EN,1);     //release reset
}

void ESP_Prog_Loop(void);

//
//
//  test code to configure the teensy 4.1 as a usb->serial interface
//  for firmware upload to the esp32 module
//
//

//
//  translate the usb serial modem control signals (DTR,RTS)
//  to control the ESP32 download mode
//

void usb_ser_rts_dtr(void)
{
    int mask = 0;
    mask |= Serial.rts()? 0 : 1;
    mask |= Serial.dtr()? 0 : 2;

    switch(mask)
    {
        case 0: 
        case 3: // do nothing
            digitalWrite(PIN_ESP_EN,1);
            digitalWrite(PIN_ESP_BOOT,1);
            break;
        
        case 1: //rts active
            digitalWrite(PIN_ESP_EN,1);
            digitalWrite(PIN_ESP_BOOT,0);
            break;
        
        case 2: //dtr active
            digitalWrite(PIN_ESP_EN,0);
            digitalWrite(PIN_ESP_BOOT,1);
            break;
    }   
}

static uint32_t ESP32_link_timeout;

void ESP_Programmer_Setup(void) {
  // put your setup code here, to run once:
  //while(!Serial);
  //Serial.begin(115200);

  Serial2.begin(115200);
  digitalWrite(PIN_ESP_EN,1);
  digitalWrite(PIN_ESP_BOOT,1);
  pinMode(PIN_ESP_EN,OUTPUT);
  pinMode(PIN_ESP_BOOT,OUTPUT);

/*
digitalWrite(PIN_ESP_BOOT,0);
delay(10);
digitalWrite(PIN_ESP_EN,0);
delay(100);
digitalWrite(PIN_ESP_EN,1);
delay(10);
digitalWrite(PIN_ESP_BOOT,1);
*/

  Serial.printf("Ready....\r\n");
  ESP32_link_timeout = systick_millis_count;

  while(1)
  {
    ESP_Prog_Loop();
    if (ESP32_link_timeout + 30000 < systick_millis_count)
    {
      Serial.printf("\nExit ESP32 programming link due to 30 second timeout\n");
      ESP_Reset();
      break;
    }
  }
}

void ESP_Prog_Loop() {
  // put your main code here, to run repeatedly:
  usb_ser_rts_dtr();

  if (Serial.available())
    {
      uint8_t ch = Serial.read();
      Serial2.write(ch);
      ESP32_link_timeout = systick_millis_count;
    }
  if (Serial2.available())
  {
    uint8_t xch = Serial2.read();
    Serial.write(xch);
  }
}


////////////////////////////////////  Example of what is seen on service terminal when this code starts up (command is esp32 prog)
//      
//      
//      EBTKS> esp32 prog
//      Ready....
//      ets Jun  8 2016 00:22:57
//      
//      rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
//      configsip: 0, SPIWP:0xee
//      clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
//      mode:DIO, clock div:2
//      load:0x3fff0018,len:4
//      load:0x3fff001c,len:5656
//      load:0x40078000,len:0
//      ho 12 tail 0 room 4
//      load:0x40078000,len:13844
//      entry 0x40078fc4
//      I (30) boot: ESP-IDF v3.0.7 2nd stage bootloader
//      I (31) boot: compile time 09:04:31
//      I (31) boot: Enabling RNG early entropy source...
//      I (35) boot: SPI Speed      : 40MHz
//      I (39) boot: SPI Mode       : DIO
//      I (43) boot: SPI Flash Size : 4MB
//      I (47) boot: Partition Table:
//      I (51) boot: ## Label            Usage          Type ST Offset   Length
//      I (58) boot:  0 phy_init         RF data          01 01 0000f000 00001000
//      I (66) boot:  1 otadata          OTA data         01 00 00010000 00002000
//      I (73) boot:  2 nvs              WiFi data        01 02 00012000 0000e000
//      I (81) boot:  3 at_customize     unknown          40 00 00020000 000e0000
//      I (88) boot:  4 ota_0            OTA app          00 10 00100000 00180000
//      I (95) boot:  5 ota_1            OTA app          00 11 00280000 00180000
//      I (103) boot: End of partition table
//      I (107) boot: No factory image, trying OTA 0
//      I (112) esp_image: segment 0: paddr=0x00100020 vaddr=0x3f400020 size=0x20614 (132628) map
//      I (168) esp_image: segment 1: paddr=0x0012063c vaddr=0x3ffc0000 size=0x02d7c ( 11644) load
//      I (172) esp_image: segment 2: paddr=0x001233c0 vaddr=0x40080000 size=0x00400 (  1024) load
//      I (175) esp_image: segment 3: paddr=0x001237c8 vaddr=0x40080400 size=0x0c848 ( 51272) load
//      I (205) esp_image: segment 4: paddr=0x00130018 vaddr=0x400d0018 size=0xdfc80 (916608) map
//      I (526) esp_image: segment 5: paddr=0x0020fca0 vaddr=0x4008cc48 size=0x02504 (  9476) load
//      I (530) esp_image: segment 6: paddr=0x002121ac vaddr=0x400c0000 size=0x00064 (   100) load
//      I (541) boot: Loaded app from partition at offset 0x100000
//      I (542) boot: Disabling RNG early entropy source...
//      1.1.3
//      I (663) wifi: wifi firmware version: 703e53b
//      I (664) wifi: config NVS flash: enabled
//      I (664) wifi: config nano formating: disabled
//      I (674) wifi: Init dynamic tx buffer num: 32
//      I (675) wifi: Init data frame dynamic rx buffer num: 32
//      I (675) wifi: Init management frame dynamic rx buffer num: 32
//      I (680) wifi: wifi driver task: 3ffdeeb8, prio:23, stack:3584
//      I (685) wifi: Init static rx buffer num: 10
//      I (688) wifi: Init dynamic rx buffer num: 32
//      I (693) wifi: wifi power manager task: 0x3ffe369c prio: 21 stack: 2560
//      I (726) wifi: mode : softAP (98:cd:ac:6d:19:85)
//      I (733) wifi: mode : sta (98:cd:ac:6d:19:84) + softAP (98:cd:ac:6d:19:85)
//      I (737) wifi: mode : softAP (98:cd:ac:6d:19:85)
//      I (740) wifi: set country: cc=CN schan=1 nchan=13 policy=1
//      
//      