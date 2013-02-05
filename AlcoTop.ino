#include <Bounce.h>

#include <TimerOne.h>
#include "LPD6803.h"

#define DBG 1

#define N_PIXELS 20

#define ALCO_MIN_READ_ENABLE 200
#define ALCO_MIN_TRIGGER 500

#define MQ3_MAX 1023
#define DOUBLE_CLICK 1000

//Even modes are light displays
#define MODE_RANDOM 0
#define MODE_RAINBOW 2
#define MODE_RAINBOWCYCLE 4

//All odd modes turn on the sensor
#define MODE_ALCO 1

#define MODE_MAX 6

//Number of bits used to represent a color
#define COLOR_BITS 3
//Number of color values of a single color
#define NUM_COLORS (1 << COLOR_BITS)
//Max color value 0 indexed
#define MAX_COLOR (NUM_COLORS - 1)
//Total color values in all 3 colors
#define NUM_RGB_COLORS (NUM_COLORS * 3)

//Slows down the rainbow when there are more bits in the colors
#define RAINBOW_SPEED 100 + ((5 - COLOR_BITS) * 25) 
#define RANDOM_SPEED 150
//Speed of the light wipe after a positive alcohol reading
#define GO_ALCO_SPEED 300
#define GO_ALCO_PAUSE 500
//Speed of the light flashes while waiting for a positive reading
#define INDICATE_ALCO_SPEED 350

// Some colors
#define COLOR_OFF Color(0, 0, 0)
#define COLOR_ALCO_WAIT Color(MAX_COLOR, 0, 0)
#define COLOR_ALCO_GO Color(0, 0, MAX_COLOR)

//IO Pins
#define INDICATOR_LED 7
#define ALCO_BJT 12
#define ALCO_SENSOR A0
#define B_MODE A5
#define UNCONNECTED_ANALOG A3

// Choose which 2 pins you will use for output.
// Can be any valid output pins.
int dataPin = 2;       // 'yellow' wire
int clockPin = 3;      // 'green' wire

// Timer 1 is also used by the strip to send pixel clocks
// Set the first variable to the NUMBER of pixels. 20 = 20 pixels in a row

LPD6803 strip = LPD6803(N_PIXELS, dataPin, clockPin);
int alcoColors[N_PIXELS] = {0};

boolean bIsAlco = false;
boolean bAlcoUp = true;
boolean bReadAlco = false;
Bounce bMode = Bounce(B_MODE, 5);
uint8_t mode = 0;
uint32_t bLast = 0;
uint32_t tChangeMode = 0;
int32_t j = 0;
uint8_t k = 0;

void setup() {
#if DBG  
  Serial.begin(115200);
  Serial.println("AlcoTop welcomes u, merry drinking!");
  Serial.println("===================================");
  Serial.println("       IMPORTANT SETTINGS          ");
  Serial.println("===================================");
  Serial.print("ALCO_MIN_TRIGGER: "); 
  Serial.println(ALCO_MIN_TRIGGER);
  Serial.print("DOUBLE_CLICK: "); 
  Serial.println(DOUBLE_CLICK);
  Serial.print("MAX_COLOR: "); 
  Serial.println(MAX_COLOR);
#endif

  // The Arduino needs to clock out the data to the pixels
  // this happens in interrupt timer 1, we can change how often
  // to call the interrupt. setting CPUmax to 100 will take nearly all all the
  // time to do the pixel updates and a nicer/faster display, 
  // especially with strands of over 100 dots.
  // (Note that the max is 'pessimistic', its probably 10% or 20% less in reality)  
  strip.setCPUmax(75);  // start with 50% CPU usage. up this if the strand flickers or is slow

  // Start up the LED counter
  strip.begin();

  calcAlcoColors();
  stripOff();

  pinMode(INDICATOR_LED, OUTPUT); 
  pinMode(ALCO_BJT, OUTPUT); 
  pinMode(ALCO_SENSOR, INPUT); 
  pinMode(B_MODE, INPUT); 

  (mode % 2 == 1) ? goAlco() : stopAlco();

  randomSeed(analogRead(UNCONNECTED_ANALOG));

  for ( uint8_t i=0; i<strip.numPixels(); i++ )
    randomStrip(0, i);
}


void loop() {
  bMode.update ();
  uint8_t oldMode = mode;

  int bRead = bMode.read();

  if (bRead == LOW && bLast == HIGH) {
    // Button was clicked, increment mode and loop 
    // around if max value is reached
    ++mode %= MODE_MAX;
#if DBG
    Serial.print("Mode changed to ");
    Serial.println(mode);
#endif
    j = 0;
    tChangeMode = millis();
    bIsAlco = false;
  }

  if ( mode % 2 == 1 ) {
    if ( oldMode != mode ) {
      goAlco();
    }
    else if ( ! bReadAlco ) {
      checkAlco();
    }
    else {
      if ( !processAlco() )
        indicateAlco();
    }
  }
  else {
    switch (mode) {
    case MODE_RANDOM:
      randomStrip(RANDOM_SPEED, -1);    
      if ( oldMode != mode )
        stopAlco();
      break;
    case MODE_RAINBOW:
      rainbow(RAINBOW_SPEED);
      if ( oldMode != mode )
        stopAlco();
      break;
    case MODE_RAINBOWCYCLE:
      rainbowCycle(RAINBOW_SPEED);
      if ( oldMode != mode )
        stopAlco();
      break;
    }
  }

  bLast = bRead;
}

void goAlco() {
  colorWipe(COLOR_ALCO_WAIT, 25, -1, -1, true);
  digitalWrite(ALCO_BJT, HIGH);
  j = -1;
#if DBG
  Serial.println("goAlco");
#endif
}

boolean processAlco() {
//#if DBG
//  Serial.println("processAlco");
//#endif
  int alco = bReadAlco ? readAlco() : 0;

  if ( alco < ALCO_MIN_TRIGGER ) {
    if ( bIsAlco ) {
      digitalWrite(INDICATOR_LED, LOW);
      bIsAlco = false;
    }
  }
  else {
    if ( ! bIsAlco ) {
      digitalWrite(INDICATOR_LED, HIGH);
      colorWipe(COLOR_OFF, 0, -1, -1, false);
      bIsAlco = true;
      bAlcoUp = true;
      j = 0;
      tChangeMode = millis();
    }
    int h = map(alco, 0, MQ3_MAX, 5, 20);
    showAlco(h);
  }

  return bIsAlco;
}

void indicateAlco() {
  uint32_t elapsed = millis() - tChangeMode;

//#if DBG
//  Serial.print("elapsed = ");
//  Serial.print(elapsed);
//  Serial.print("\tk = ");
//  Serial.println(k);
//#endif

  if ( elapsed < INDICATE_ALCO_SPEED / 2 ) {
    if ( k == 0 ) {
      colorWipe(COLOR_OFF, 0, -1, -1, false);
      k++;
//#if DBG
//      Serial.println("k == 0; k++");
//#endif
    }
  }
  else if ( elapsed < INDICATE_ALCO_SPEED ) {
    if ( k == 1 ) {
      colorWipe(COLOR_ALCO_GO, 0, -1, -1, false);
      k++;
      //#if DBG
      //  Serial.println("k == 1; k++");
      //#endif
    }
  }
  else if ( elapsed < INDICATE_ALCO_SPEED  * 1.5 ) {
    if ( k == 2 ) {
      colorWipe(COLOR_OFF, 0, -1, -1, false);
      k++;
      //#if DBG
      //  Serial.println("k == 2; k++");
      //#endif
    }
  }
  else if ( elapsed < INDICATE_ALCO_SPEED  * 2 ) {
    if ( k == 3 ) {
      colorWipe(COLOR_ALCO_GO, 0, -1, -1, false);
      k++;
      //#if DBG
      //  Serial.println("k == 3; k++");
      //#endif
    }
  }
  else if ( elapsed > INDICATE_ALCO_SPEED  * 6 ) {
    tChangeMode = millis();
    k = 0;
    //#if DBG
    //  Serial.println("k = 0; reset");
    //#endif
  }
}

void checkAlco() {
  int alco = readAlco();
//#if DBG
//  Serial.print("checkAlco");
//#endif

  if ( alco > ALCO_MIN_READ_ENABLE ) {
    bReadAlco = true;
    tChangeMode = millis();
    k = 0;
#if DBG
    Serial.println("bReadAlco = true");
#endif
  }
  else {
    int i = map(alco, 0, ALCO_MIN_READ_ENABLE, 0, 20);
    if ( i != j ) {
      j = i;
      colorWipe(COLOR_ALCO_GO, 0, -1, j, true);
//#if DBG
//  Serial.print("checkAlco: ");
//  Serial.println(j);
//#endif
      delay(50);
    }
  }
}

void stopAlco() {
  digitalWrite(ALCO_BJT, LOW);
  bReadAlco = false;
#if DBG
  Serial.println("stopAlco");
#endif
}

void stripOff() {
  for (uint8_t i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, Color(31, 31, 0));
  }
  strip.show();
}

void calcAlcoColors() {
  int8_t mid = 8;
  int16_t step = NUM_COLORS / mid;
  if ( step == 0 )
    step = 1;
  int16_t r = 0, g = MAX_COLOR;

  for (int8_t i=0; i < mid; i++) {
//#if DBG    
//  Serial.print("i="); Serial.print(i);
//  Serial.print(", r="); Serial.print(r);
//  Serial.print(", g="); Serial.println(g);
//#endif

    alcoColors[i] = Color(r, g, 0);    

    r += step;
    if ( r > MAX_COLOR )
      r = MAX_COLOR;
  }

  for (uint8_t i=mid; i < 20; i++) {
//#if DBG    
//  Serial.print("i="); Serial.print(i);
//  Serial.print(", r="); Serial.print(r);
//  Serial.print(", g="); Serial.println(g);
//#endif

    alcoColors[i] = Color(r, g, 0);    

    g -= step;
    if ( g < 0 )
      g = 0;      
  }
}

void showAlco(int h) {
  long elapsed = millis() - tChangeMode;
  int perPixel = GO_ALCO_SPEED / h;

  if ( elapsed >  perPixel ) {
//#if DBG
//  Serial.print("elapsed = ");
//  Serial.println(elapsed);
//  Serial.print("perPixel = ");
//  Serial.println(perPixel);
//#endif
//#if DBG
//  Serial.print("bAlcoUp = ");
//  Serial.println(bAlcoUp);
//  Serial.print("j = ");
//  Serial.println(j);
//#endif

    if ( bAlcoUp ) {
      // Pause at the top
      if ( j == h + 1 ) {
        if ( elapsed >  GO_ALCO_PAUSE ) {
          bAlcoUp = false;
          tChangeMode = millis();
        }        
      }
      else {
        strip.setPixelColor(j, alcoColors[j]);
        strip.show();

        tChangeMode = millis();
        bAlcoUp = j++ <= h;
      }
    }
    else {
      strip.setPixelColor(j, COLOR_OFF);
      strip.show();
      bAlcoUp = j-- < 0;
      tChangeMode = millis();      
    }    
  }
}

int readAlco() {
  int alco = analogRead(ALCO_SENSOR);

#if DBG
  Serial.print("alco = ");
  Serial.println(alco);
  delay(25);
#endif
 
  return alco;
}

void randomStrip(int wait, int index) {
  if ( index < 0 )
    index = random(0, strip.numPixels());

  //#if DBG
  //  Serial.print("randomStrip: ");
  //  Serial.println(index);
  //#endif

  int r = 0, g = 0, b = 0;
  int mix = random(0, 2);
  switch ( mix ) {
  case 0:
    r = random(0, MAX_COLOR);
    g = random(0, MAX_COLOR);
    break;
  case 1:
    g = random(0, MAX_COLOR);
    b = random(0, MAX_COLOR);
    break;
  case 2:
    r = random(0, MAX_COLOR);
    b = random(0, MAX_COLOR);
    break;
  }

  strip.setPixelColor(index, Color(r, g, b));
  strip.show();
  if ( wait > 0 )
    delay(wait);
}

void rainbow(uint8_t wait) {
  //#if DBG
  //  Serial.print("rainbow: ");
  //  Serial.println(j);
  //#endif
  for (uint8_t i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel((i + j) % NUM_RGB_COLORS));
  }  
  strip.show();   // write all the pixels out
  delay(wait);

  if ( j++ > NUM_RGB_COLORS * 3 ) {
    // 3 cycles of all NUM_RGB_COLORS colors in the wheel
    j = 0;
  }
}

void rainbowCycle(uint8_t wait) {
  //#if DBG
  //  Serial.print("rainbowCycle: ");
  //  Serial.println(j);
  //#endif
  for (uint8_t i=0; i < strip.numPixels(); i++) {
    // tricky math! we use each pixel as a fraction of the full NUM_RGB_COLORS-color wheel
    // (thats the i / strip.numPixels() part)
    // Then add in j which makes the colors go around per pixel
    // the % NUM_RGB_COLORS is to make the wheel cycle around
    strip.setPixelColor(i, Wheel( ((i * NUM_RGB_COLORS / strip.numPixels()) + j) % NUM_RGB_COLORS)  );
  }  
  strip.show();   // write all the pixels out
  delay(wait);

  if ( j++ > NUM_RGB_COLORS * 5 ) {
    // 5 cycles of all NUM_RGB_COLORS colors in the wheel
    j = 0;    
  }
}

// fill the dots one after the other with said color
// good for testing purposes
void colorWipe(uint16_t c, uint8_t wait, uint8_t start, uint8_t finish, boolean up) {
  //#if DBG
  //  Serial.print("colorWipe: ");
  //  Serial.println(c);
  //#endif
  if ( up ) {
    if ( start < 0 || start > strip.numPixels() ) 
      start = 0;
    if ( finish < 0 || finish > strip.numPixels() ) 
      finish = strip.numPixels();

    for (int i=start; i < finish; i++) {
      strip.setPixelColor(i, c);
      if ( wait > 0 ) {
        strip.show();
        delay(wait);
      }
    }
  }
  else {
    if ( start < 0 || start > strip.numPixels() - 1 )
      start = strip.numPixels() - 1;
    if ( finish < 0 || finish > strip.numPixels() - 1 )
      finish = 0;

    for (int i=start; i >= finish; i--) {
      strip.setPixelColor(i, c);
      if ( wait > 0 ) {
        strip.show();
        delay(wait);
      }
    }
  }

  if ( wait == 0 ) {
    strip.show();
  }
}

/* Helper functions */

// Create a 15 bit color value from R,G,B
unsigned int Color(byte r, byte g, byte b)
{
  //Take the lowest 5 bits of each value and append them end to end
  return( (((unsigned int)g & 0x1F) << 10) | (((unsigned int)b & 0x1F) << 5) | ((unsigned int)r & 0x1F) );
}

//Input a value 0 to 127 to get a color value.
//The colours are a transition r - g -b - back to r
unsigned int Wheel(byte WheelPos)
{
  byte r=0, g=0, b=0;
  switch(WheelPos >> COLOR_BITS)
  {
  case 0:
    r = MAX_COLOR - WheelPos % NUM_COLORS;   //Red down
    g = WheelPos % NUM_COLORS;      // Green up
    b = 0;                  //blue off
    break; 
  case 1:
    g = MAX_COLOR - WheelPos % NUM_COLORS;  //green down
    b = WheelPos % NUM_COLORS;      //blue up
    r = 0;                  //red off
    break; 
  case 2:
    b = MAX_COLOR - WheelPos % NUM_COLORS;  //blue down 
    r = WheelPos % NUM_COLORS;      //red up
    g = 0;                  //green off
    break; 
  }
  return(Color(r,g,b));
}


