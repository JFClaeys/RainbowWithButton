
#include <FastLED.h>
#include <OneButton.h>
#include <EEPROM.h>
#include <alphas.h>
#include <RainbowDef.h>

#define NUM_LEDS 60     // Number of LEDs in the strip
#define DATA_PIN 5     // WS2812 DATA_PIN.  Nano = old bootloader
#define PUSH_BUTTON 7  // PIN used for controling the button. Connected to ground 
#define LOOP_MS 3      // how long in ms  before iteration of colour
#define EEPROM_PATTERN_ADDRESS 0

#define CYCLE_PATTERN 0 // each led uses colour of precedent and a new colour is added on last led
#define FIXED_PATTERN 1 // each led uses the new color apart 2nd and 6th, where red and green channels are fixed as 128, blue channel takes blue of new colour
#define ARROW_PATTERN 2 // temptative
#define SCAN_PATTERN  3
#define ALPHA_PATTERN 4
#define MAX_PATTERN   5

#define LONG_PRESS_NEXT_PATTERN 1000  // 1 second long press will increment pattern pointer while not lit
#define CLICK_MS_DURATION 120

CRGB leds[NUM_LEDS]; // the array of leds to be shown
CRGB backupLED[NUM_LEDS]; //the backup, as to where the current display is stored before clearing

//forward declarations
void onPressedForNextPattern();
void onSinglePressed();
void onDoubleClick();

class Button{
private:
  OneButton button;
public:
  explicit Button(uint8_t pin):button(pin) {
    button.setClickTicks(CLICK_MS_DURATION);
    button.attachClick([](void *scope) { ((Button *) scope)->Clicked();}, this);
    button.attachDoubleClick([](void *scope) { ((Button *) scope)->DoubleClicked();}, this);
    button.attachLongPressStart([](void *scope) { ((Button *) scope)->LongPressed();}, this);
  }

  void Clicked() {
    onSinglePressed();
  }

  void DoubleClicked() {
    onDoubleClick();
  }

  void LongPressed() {
    onPressedForNextPattern();
  }

  void read() {
    button.tick();
  }
};

Button button(PUSH_BUTTON);
byte currentPattern = CYCLE_PATTERN;    // pattern pointer

bool isLED_lit = true;      // have we requested leds to be visible or not? (i.e: pause mode)
uint8_t iWait = 0;          // current cycle before next color cycle
uint16_t AngleCycling = 0;  // current angle to use in the rainbow array 
uint16_t LedCounter[MAX_PATTERN];
uint8_t LedWaitLoop[MAX_PATTERN];
bool directionForward = true;

/*******************************************************************/

void setRGBpoint(byte LED, uint8_t red, uint8_t green, uint8_t blue)
{
  //move colors along the array as to make them traveling the stick 
  switch (currentPattern) {
    case CYCLE_PATTERN :
      for (byte i=NUM_LEDS-1; i>0; i--) {
        leds[i] = leds[i-1];
      }
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
    case ARROW_PATTERN :
      leds[0] = leds[1];
      leds[1] = leds[2];
      leds[2] = leds[3];
      leds[7] = leds[6];
      leds[6] = leds[5];
      leds[5] = leds[4];
      leds[4] = CRGB(red, green, blue);
      leds[3] = leds[4];
      break;
  
    case SCAN_PATTERN :
      for (byte i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::DarkRed;
      }  
      leds[LED] = CRGB(red, green, blue);
      leds[LED + 1] = CRGB(red, green, blue);
      break;
      
    case ALPHA_PATTERN :
      for (byte i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB(0,0,20);
      }
      leds[LED] = CRGB(0, 50, 20);
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

CRGB Wheel(byte WheelPos) {
  if(WheelPos < 85) {
    return CRGB(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) {
    WheelPos -= 85;
    return CRGB(255 - WheelPos * 3, 0, WheelPos * 3);
  } 
  else {
    WheelPos -= 170;
    return CRGB(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

/*******************************************************************/

void AcknowledgeCommand( byte ledNumber ) {
  leds[ledNumber] = Wheel((255 / MAX_PATTERN) * ledNumber);
  FastLED.show();
  delay(250);
  leds[ledNumber] = CRGB(0, 0, 0 ); 
  FastLED.show(); 
}

void onPressedForNextPattern() {
 if (!isLED_lit) {
   currentPattern++;
   if (currentPattern >= MAX_PATTERN) {
     currentPattern = 0;
   }
   AcknowledgeCommand(currentPattern);
 }
}

void onSinglePressed() {
  if (isLED_lit) {
    BackupCurrentPattern(false);
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    RestoresSavedPattern(true);
    digitalWrite(LED_BUILTIN, LOW);
    //EEPROM.update(EEPROM_PATTERN_ADDRESS, currentPattern); // will only write the memory if it differs.
  }

  FastLED.show();
  isLED_lit = !isLED_lit;
}

void onDoubleClick() {  // test to see double clicking behaviour
  if (!isLED_lit) {
    currentPattern = SCAN_PATTERN;
    AcknowledgeCommand(currentPattern);
  }
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
  for (byte i=0; i< NUM_LEDS; i++) {
    leds[i] = backupLED[i];
  }

  if (doShow) {
    FastLED.show();
  }
}

void processLoopContent() {
  if (iWait < LedWaitLoop[currentPattern]) {
    delay(1);
    iWait++;
    return;
  }
  
  iWait = 0;

  if ((AngleCycling % 5) == 0) {
     sineLED(LedCycling, AngleCycling);
  }

  //going further on the cycle or resttting it
  if (AngleCycling < CIRCLE_ANGLES) {
    AngleCycling++;
  } else {
    AngleCycling = 0;

    switch (currentPattern) {
      case SCAN_PATTERN: 
      case ALPHA_PATTERN:
        /*allow the whole color cycle before changing to next led*/
        if (directionForward) {
          LedCounter[currentPattern]++;
        } else {
          LedCounter[currentPattern]--;
        }

        if (LedCounter[currentPattern] >= NUM_LEDS-2) {
          directionForward = false;
          LedCounter[currentPattern] = NUM_LEDS-2;
        } else {
          if (LedCounter[currentPattern] <= 0) {
            directionForward =  true;
            LedCounter[currentPattern] = 0;
          }
        }
    }
  }
}

/*******************************************************************/

void setup() {
  pinMode(PUSH_BUTTON, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  memset(LedCounter, 0, sizeof(LedCounter));
  memset(LedWaitLoop, LOOP_MS, sizeof(LedWaitLoop));
  LedWaitLoop[ALPHA_PATTERN] = LOOP_MS * 10;
  //currentPattern = EEPROM.read(EEPROM_PATTERN_ADDRESS);
}

/*******************************************************************/

void loop() {
  button.read();

  if (isLED_lit) {
    processLoopContent();
  } 
}  
