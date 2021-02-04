#include <Arduino.h>
#define FASTLED_INTERNAL  // shut up pragma that reports version
#include <FastLED.h>


extern volatile bool DMA_Active;
extern volatile bool DMA_Request;
extern void assert_DMA_Request(void);
extern void release_DMA_request(void);

#define LED_PIN     2
#define NUM_LEDS    2
#define BRIGHTNESS  10      //64 is max. note these are like *freakin* lasers at full brightness

//
//  init our two WS2812 rgb leds
//  Note that the color values are inverted - not quite a design feature
//  but a published issue with the Teensy. Thus each color value is 255 for off
//  and 0 for full on. Our accessor functions will correct this
//

class LED {

public:

//
//  begin() does not use DMA state because it is called before interrupts have been enabled
//
//  2/3/2021  There seems to be a bug where the LSB of all 6 bytes sent are always set.
//            This hints of other issues:  https://github.com/FastLED/FastLED/issues/899
//            But trying its recommendations (adding 1, at the beginning) just killed everything.
//
void begin(void)
{
  //  FastLED.addLeds<1, WS2812B, LED_PIN, GRB > (_leds, NUM_LEDS);   THIS DOES NOT WORK
  FastLED.addLeds<WS2812B, LED_PIN, GRB > (_leds, NUM_LEDS);

  _leds[0].r = 255;
  _leds[0].g = 255; 
  _leds[0].b = 255;

  _leds[1].r = 255; 
  _leds[1].g = 255;
  _leds[1].b = 255;

  FastLED.show();
  delay(1);
  FastLED.show(); 
}
// call this function to copy the led data to the leds themselves
void update(void)
{
  //
  //  Just for debug.  Seems the LSB for all bytes is always stuck on.
  //
  //  Serial.printf("LED 0 RGB:  %.3d %.3d %.3d   LED 1 RGB:  %.3d %.3d %.3d\n", _leds[0].r, _leds[0].g, _leds[0].b, _leds[1].r, _leds[1].g, _leds[1].b);

  //  We don't actually DMA into the HP85 - but we do want to
  //  disable the Teensy interrupts and this is the only way we can
  //  achieve this without upsetting the HP85

  //  start DMA
  assert_DMA_Request();
  while (!DMA_Active)
    {
    }; // Wait for acknowledgment, and Bus ownership

  FastLED.show();

  release_DMA_request();
}

void setLedColor(int ledNum, CRGB color)
{
  if (ledNum < NUM_LEDS)
    {
    _leds[ledNum].r = 255u - color.r;        //we fix the library's inversion here
    _leds[ledNum].g = 255u - color.g;
    _leds[ledNum].b = 255u - color.b;
    }
}

private:
    CRGB _leds[NUM_LEDS];
};
