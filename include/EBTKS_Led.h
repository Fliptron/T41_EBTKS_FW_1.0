#include <Arduino.h>
#define FASTLED_INTERNAL  // shut up pragma that reports version
#include <FastLED.h>

extern volatile bool DMA_Active;
extern volatile bool DMA_Request;
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

void begin(void)
{
  FastLED.addLeds<WS2812, LED_PIN, GRB > (_leds, NUM_LEDS);
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
  //    we don't actually dma into the HP85 - but we do want to
  //    disable the interrupts and this is the only way we can
  //    achieve this without upsetting the HP85

  //start dma
  DMA_Request = true;
  while (!DMA_Active)
    {
    }; // Wait for acknowledgement, and Bus ownership

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
