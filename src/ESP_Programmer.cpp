
//  All that is needed to act as a usb->serial gateway
//  to program the esp32.  tested on real hardware!

#include <Arduino.h>

#define PIN_ESP_EN (9)
#define PIN_ESP_BOOT (10)

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

  while(1)
  {
    ESP_Prog_Loop();
  }
}

void ESP_Prog_Loop() {
  // put your main code here, to run repeatedly:
  usb_ser_rts_dtr();

  if (Serial.available())
    {
      uint8_t ch = Serial.read();
      Serial2.write(ch);
    }
  if (Serial2.available())
  {
    uint8_t xch = Serial2.read();
    Serial.write(xch);
  }
}

