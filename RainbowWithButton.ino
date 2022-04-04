
#include <FastLED.h>
#include <EasyButton.h>
#include "RainbowDef.h"

// uint8_t is the same as byte
// uint16_t is unsigned int
// I just noticed that I mixed these in this sketch, sorry

#define NUM_LEDS 8 // Number of LEDs in the strip
#define DATA_PIN 5 // WS2812 DATA_PIN.  Nano = old bootloader
#define PUSH_BUTTON 7
#define LOOP_MS 5

#define CYCLE_PATTERN 0
#define FIXED_PATTERN 1
#define MAX_PATTERN 2  // cycling, fixed

#define LONG_PRESS_NEXT_PATTERN 1000

EasyButton button(PUSH_BUTTON);
CRGB leds[NUM_LEDS]; // the array of leds to be shown
CRGB backupLED[NUM_LEDS]; //the backup, as to where the current display is stored before clearing

byte currentPattern = 0;    // 00 = cycling, 1 = fixed, 2 = ...who knows...
bool isLED_lit = true;      // have we requested leds to be visible or not? (i.e: pause mode)
uint8_t iWait = 0;          // current cycle before next color cycle
uint16_t AngleCycling = 0;  // current angle to use in the rainbow array 

/*******************************************************************/

void setRGBpoint(byte LED, uint8_t red, uint8_t green, uint8_t blue)
{
  //move colors along the array as to make them traveling the stick 
  switch (currentPattern) {
    case CYCLE_PATTERN :
      leds[7] = leds[6];
      leds[6] = leds[5];
      leds[5] = leds[4];
      leds[4] = leds[3];
      leds[3] = leds[2];
      leds[2] = leds[1];
      leds[1] = leds[0];
      leds[0] = CRGB(red, green, blue);
      break; 
    case FIXED_PATTERN :
      leds[7] = CRGB(red, green, blue);
      leds[6] = CRGB(128, 128, blue);
      leds[5] = leds[7];
      leds[4] = leds[7];
      leds[3] = leds[7];
      leds[2] = leds[7];
      leds[1] = leds[6];
      leds[0] = leds[7];
      break;
  }
  FastLED.show();
}

// sine wave rainbow
void sineLED(byte LED, int angle)
{
  uint8_t red = pgm_read_byte(lights +  ((angle + 120) % CIRCLE_ANGLES));
  uint8_t green = pgm_read_byte(lights + angle);
  uint8_t blue = pgm_read_byte(lights + ((angle + 240) % CIRCLE_ANGLES));
  setRGBpoint(LED, red, green, blue ); 
}

/*******************************************************************/

void AcknowledgeCommand( byte ledNumber, byte primaryColor ) {
  leds[ledNumber] = CRGB(primaryColor==0 ? 255 : 0,   primaryColor==1 ? 255 : 0, primaryColor==2 ? 255 : 0 ); 
  FastLED.show();
  delay(250);
  leds[ledNumber] = CRGB(0, 0, 0 ); 
  FastLED.show(); 
}

void onPressedForNextPattern() {
 if (!isLED_lit) {
   currentPattern++;
   if (currentPattern == MAX_PATTERN) {
     currentPattern = 0;
   }   
   AcknowledgeCommand(currentPattern ,currentPattern); // first led, in red;
 }  
}

void onSinglePressed() {  
  if (isLED_lit) {
    BackupCurrentPattern(false);
    digitalWrite(LED_BUILTIN, HIGH);  
  } else {
    RestoresSavedPattern(true) ;
    digitalWrite(LED_BUILTIN, LOW);  
  }
  
  FastLED.show();      
  isLED_lit = !isLED_lit;
}

/*******************************************************************/

void BackupCurrentPattern( bool doShow ) {
  //take a copy of the current LEDs, as we are going to clear it
  for (byte i=0; i< NUM_LEDS; i++) {
    backupLED[i] = leds[i];
  }      
  FastLED.clear();

  if (doShow) {
    FastLED.show();
  }  
}

void RestoresSavedPattern( bool doShow ) {
   //restore the state of the leds when we decided to close them
  for (byte i=0;
   i< NUM_LEDS; i++) {
    leds[i] = backupLED[i];
  }
  
  if (doShow) {
    FastLED.show();
  }  
}

/*******************************************************************/

void setup() {
  pinMode(PUSH_BUTTON, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  button.onPressedFor(LONG_PRESS_NEXT_PATTERN, onPressedForNextPattern);
  button.onPressed(onSinglePressed);
}

/*******************************************************************/

void loop() {
  button.read();

  if (iWait < LOOP_MS) {
    delay(1);
    iWait++;
  } else {
    iWait = 0;
  
    if (isLED_lit) {
      if ((AngleCycling % 5) == 0) {
        sineLED(0, AngleCycling);
      }

      //going further on the cycle or resttting it
      if (AngleCycling < CIRCLE_ANGLES) {
        AngleCycling++;
      } else {
       AngleCycling = 0;
      }
    }  
  } 
}  
