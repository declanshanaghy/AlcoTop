#include <Bounce.h>

#include <TimerOne.h>
#include "LPD6803.h"

#define T_TOTAL 500
#define N_PIXELS 20

#define ALCO_MIN 400
#define NOT_ALCO Color(0,0,0)
#define MQ3_MAX 850
#define MODE_MAX 3
#define DOUBLE_CLICK 1000

//Number of bits used to represent a color
#define COLOR_BITS 5
//Number of color values of a single color
#define NUM_COLORS (1 << COLOR_BITS)
//Max color value 0 indexed
#define MAX_COLOR (NUM_COLORS - 1)
//Total color values in all 3 colors
#define NUM_RGB_COLORS (NUM_COLORS * 3)

//Slows down the rainbow when there are more bits in the colors
#define RAINBOW_SPEED 100 + ((5 - COLOR_BITS) * 25) 
#define RANDOM_SPEED 150

// Choose which 2 pins you will use for output.
// Can be any valid output pins.
int dataPin = 2;       // 'yellow' wire
int clockPin = 3;      // 'green' wire

// Timer 1 is also used by the strip to send pixel clocks
// Set the first variable to the NUMBER of pixels. 20 = 20 pixels in a row

LPD6803 strip = LPD6803(N_PIXELS, dataPin, clockPin);
int alcoColors[N_PIXELS] = {0};

boolean isAlco = false;
Bounce bMode = Bounce(9, 5);
uint8_t mode = 0;
uint32_t bLast = 0;
uint32_t tDouble = 0;
uint8_t j = 0;

void setup() {  
  Serial.begin(115200);
  
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
  
  pinMode(7, OUTPUT); 
  pinMode(10, OUTPUT); 
  pinMode(9, INPUT); 
  pinMode(A0, INPUT); 
  pinMode(A5, INPUT); 

  digitalWrite(10, HIGH);

  randomSeed(analogRead(A5));
  
  for ( int i=0; i<strip.numPixels(); i++ )
    randomStrip(0, i);
}


void loop() {
//  bMode.update();
//  int bRead = bMode.read();
//  // Turns on the alco sensor if the button is held down
//  digitalWrite(10, bRead);
    
//  int alco = 0;
//  if ( bRead == HIGH )
    int alco = readAlco();
  
  if ( alco < ALCO_MIN ) {
    if ( isAlco ) {
      digitalWrite(7, LOW);
    }
    loopMode();
    isAlco = false;
  }
  else {
    if ( ! isAlco ) {
      colorWipe(NOT_ALCO, 0);      
      digitalWrite(7, HIGH);
    }
    int h = map(alco, 0, MQ3_MAX, 5, 20);
    doAlco(h);
    isAlco = true;
  }
}

void loopMode() {
  bMode.update ();
  int bRead = bMode.read();
  
  if (bRead == LOW && bLast == HIGH) {
    // Button was clicked.    
    if ( millis() - tDouble < DOUBLE_CLICK ) { 
      // If this is the second click since 
      // DOUBLE_CLICK ms ago, change mode.
      Serial.println("Second click");
      mode = ++mode % MODE_MAX;    
      Serial.print("mode=");
      Serial.println(mode);
      tDouble = 0;
    }
    else {
      Serial.println("Click");
      tDouble = millis();
    }
  }
  
  switch (mode) {
    case 0:
      randomStrip(RANDOM_SPEED, -1);    
      break;
    case 1:
      rainbow(RAINBOW_SPEED);
      break;
    case 2:
      rainbowCycle(RAINBOW_SPEED);
      break;
  }
  
  bLast = bRead;
}

void stripOff() {
  for (int i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, Color(31, 31, 0));
  }
  strip.show();
}

void calcAlcoColors() {
  int8_t mid = 8;
  int16_t step = NUM_COLORS / mid;
  if ( step == 0 )
    step = 1;
  int16_t c = 0, r = 0, g = MAX_COLOR;
  
  for (int i=0; i < mid; i++) {
//    Serial.print("i="); Serial.print(i);
//    Serial.print(", r="); Serial.print(r);
//    Serial.print(", g="); Serial.println(g);
    
    alcoColors[i] = Color(r, g, 0);    
    
    r += step;
    if ( r > MAX_COLOR )
      r = MAX_COLOR;
  }
    
  for (int i=mid; i < 20; i++) {
//    Serial.print("i="); Serial.print(i);
//    Serial.print(", r="); Serial.print(r);
//    Serial.print(", g="); Serial.println(g);
    
    alcoColors[i] = Color(r, g, 0);    
    
    g -= step;
    if ( g < 0 )
      g = 0;      
  }
}

void doAlco(int h) {
  int t_wait = T_TOTAL / h; 
  
  alcoUp(t_wait, h);
  delay(T_TOTAL);
  alcoDown(t_wait, h);
}

void alcoUp(int wait, int h) {
  int d_wipe = 50;
  
  for (int i=0; i < h; i++) {
    strip.setPixelColor(i, alcoColors[i]);
    strip.show();
    delay(wait);
  }
}

void alcoDown(int wait, int h) {
  for (int i=h-1; i >= 0; i--) {
    strip.setPixelColor(i, NOT_ALCO);
    strip.show();
    delay(wait);
  }
}

int readAlco() {
  int a0 = analogRead(A0);
  Serial.print("A0 = ");
  Serial.println(a0);
  return a0;
}

void randomStrip(int wait, int index) {
  if ( index < 0 )
    index = random(0, strip.numPixels());
    
  int r, g, b = 0;
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
  int i;   
  for (i=0; i < strip.numPixels(); i++) {
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
  int i;
  for (i=0; i < strip.numPixels(); i++) {
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
void colorWipe(uint16_t c, uint8_t wait) {
  int i;
  
  for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      if ( wait > 0 ) {
        strip.show();
        delay(wait);
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
  return( ((unsigned int)g & 0x1F )<<10 | ((unsigned int)b & 0x1F)<<5 | (unsigned int)r & 0x1F);
}

//Input a value 0 to 127 to get a color value.
//The colours are a transition r - g -b - back to r
unsigned int Wheel(byte WheelPos)
{
  byte r, g, b;
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

    
    
