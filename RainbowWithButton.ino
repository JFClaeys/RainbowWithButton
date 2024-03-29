
#include <FastLED.h>
#include <OneButton.h>
#include <EEPROM.h>
#include <alphas.h>
#include <RainbowDef.h>

#define NUM_LEDS 144   // Number of LEDs in the strip
#define DATA_PIN 5     // WS2812 DATA_PIN.  Nano = old bootloader
#define PUSH_BUTTON 7  // PIN used for controling the button. Connected to ground 
#define LOOP_MS 3      // how long in ms  before iteration of colour
#define EEPROM_PATTERN_ADDRESS 0

#define CYCLE_PATTERN 0 // each led uses colour of precedent and a new colour is added on last led
#define FIXED_PATTERN 1 // each led uses the new color apart 2nd and 6th, where red and green channels are fixed as 128, blue channel takes blue of new colour
#define ARROW_PATTERN 2 // temptative
#define SCAN_PATTERN  3
#define ALPHA_PATTERN 4
#define FULL_PATTERN  5
#define SWEEP_PATTERN 6
#define LETTERS_PATTERN 7
#define MAX_PATTERN   8

#define LONG_PRESS_NEXT_PATTERN 1000  // 1 second long press will increment pattern pointer while not lit
#define CLICK_MS_DURATION 120
#define HASBITSET(x, i) ((x & bit(i)) == bit(i))

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
byte currentPattern = LETTERS_PATTERN;    // pattern pointer

bool isLED_lit = true;      // have we requested leds to be visible or not? (i.e: pause mode)
uint8_t iWait = 0;          // current cycle before next color cycle
uint16_t AngleCycling = 0;  // current angle to use in the rainbow array 
uint8_t gHue = 0;           // from DemoReel100
uint16_t LedCounter[MAX_PATTERN];
uint8_t LedWaitLoop[MAX_PATTERN];
bool directionForward = true;
uint8_t letterControl =0;
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

uint8_t getNextIteration( uint16_t currentID, uint8_t aMin, uint8_t aMax, bool& forward) {
  if (forward) {
   currentID++;
  } else {
   currentID--;
  }

  if (currentID >= aMax) {
    forward = false;
    currentID = aMax;
  } else {
    if (currentID <= aMin) {
      forward =  true;
      currentID = aMin;
    }
  }
  return currentID;
}

void rainbow() {
  uint16_t i;
  byte max_color = 250;
  byte min_color = 100;

  for(i = 0;  i < FastLED.size(); i++) {
    leds[i] =  Wheel((i * 1 + LedCounter[currentPattern]) & 255);
  }

  LedCounter[currentPattern] = getNextIteration(LedCounter[currentPattern], min_color, max_color, directionForward);
  FastLED.show();
}

void fullcolor_rainbow() {
  FastLED.showColor( Wheel((1+LedCounter[currentPattern]) & 255));
  LedCounter[currentPattern] = getNextIteration(LedCounter[currentPattern], 0, 255, directionForward);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);

  int pos = beatsin16( 13, 0, NUM_LEDS-2 );
  leds[pos] += CHSV( gHue, 255, 192);
  FastLED.show();
  gHue++;
  //LedCounter[currentPattern] = getNextIteration(LedCounter[currentPattern], 0,  NUM_LEDS-2, directionForward);   
}

void HappyNewYear() {
const LettersToIndex sequence[] = {letter_SPACE, /*letter_SPACE, letter_SPACE, */
                                   letter_H, letter_A, letter_P, letter_P, letter_Y, letter_SPACE, 
                                   letter_N, letter_E, letter_W, letter_SPACE,
                                   letter_Y, letter_E, letter_A, letter_R, letter_SPACE, 
                                   letter_exclam, letter_exclam, letter_exclam};
  

  byte v = pgm_read_byte( &alphas[sequence[LedCounter[currentPattern]]].drawing[letterControl]);
  switch (v) {
    case 0 : 
      FastLED.showColor( CRGB::Black);
      break;
          
    case 0xFF :
      FastLED.showColor( CRGB::Blue);
      break;

    default:
      for (byte i=0; i < 8; i++) {
        leds[i] = (bitRead(v, i) ? CRGB::Blue : CRGB::Black);
      }
      FastLED.show();  
      break;    
  }
  
  letterControl++;
  if (letterControl > NUM_LINES_CHAR-1) {
    LedCounter[currentPattern]++;
    if (LedCounter[currentPattern] >= NUM_OF(sequence)) {
      LedCounter[currentPattern] = 0;
    }
    // if ()
    letterControl = 0;
    //FastLED.showColor( CRGB::Red);   //this is to verify the shape of each letter by adding a red bar between
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
 // EVERY_N_MILLISECONDS( 10 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  
  if (iWait < LedWaitLoop[currentPattern]) {
    delay(1);
    iWait++;
    return;
  }
  
  iWait = 0;

  switch (currentPattern) {
    case CYCLE_PATTERN :
    case FIXED_PATTERN :
    case ARROW_PATTERN :
    case SCAN_PATTERN  :
      if ((AngleCycling % 5) == 0) {
        sineLED(LedCounter[currentPattern], AngleCycling);
      }

      //going further on the cycle or resttting it
      if (AngleCycling < CIRCLE_ANGLES) {
       AngleCycling++;
       break;
      } 

      AngleCycling = 0;
      if (currentPattern == SCAN_PATTERN) {
        LedCounter[currentPattern] = getNextIteration(LedCounter[currentPattern], 0,  NUM_LEDS-2, directionForward);        
      }
      break;

    case ALPHA_PATTERN:
      rainbow();
      break;

    case FULL_PATTERN:
      fullcolor_rainbow();
      break;

    case SWEEP_PATTERN:
      sinelon();
      break;

    case LETTERS_PATTERN:
      HappyNewYear();
      break;
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
  LedWaitLoop[SCAN_PATTERN] = 1;
  LedWaitLoop[ALPHA_PATTERN] = LOOP_MS * 10;
  LedWaitLoop[FULL_PATTERN] = LOOP_MS * 10;
  LedWaitLoop[LETTERS_PATTERN] = 200;
  //currentPattern = EEPROM.read(EEPROM_PATTERN_ADDRESS);
}

/*******************************************************************/

void loop() {
  button.read();

  if (isLED_lit) {
    processLoopContent();
  } 
}  
