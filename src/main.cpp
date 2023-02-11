#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <OctoWS2811.h>
#include <Bounce.h>

// RGB LED
// Any group of digital pins may be used
const int numPins = 1;
byte pinList[numPins] = {2};

const int ledsPerStrip = 120;

// These buffers need to be large enough for all the pixels.
// The total number of pixels is "ledsPerStrip * numPins".
// Each pixel needs 3 bytes, so multiply by 3.  An "int" is
// 4 bytes, so divide by 4.  The array is created using "int"
// so the compiler will align it to 32 bit memory.
const int bytesPerLED = 3;  // change to 4 if using RGBW
DMAMEM int displayMemory[ledsPerStrip * numPins * bytesPerLED / 4];
int drawingMemory[ledsPerStrip * numPins * bytesPerLED / 4];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

#define RED    0xFF0000
#define GREEN  0x00FF00
#define BLUE   0x0000FF
#define YELLOW 0xFFFF00
#define PINK   0xFF1088
#define ORANGE 0xE05800
#define ORANGE2 0xEA5500
#define ORANGE3 0xD94400
#define ORANGE4 0xD22201
#define ORANGE5 0xDE2500
#define WHITE  0xFFFFFF
#define BLACK  0x000000
#define DARKWHITE 0x040408

#define PASTEL1 0x913CCD
#define PASTEL2 0xF05F74
#define PASTEL3 0xF76D3C
#define PASTEL4 0xF7D842
#define PASTEL5 0x2CA8C2
#define PASTEL6 0x98CB4A
#define PASTEL7 0x839098
#define PASTEL8 0x5381E6

// Audio Player
AudioPlaySdWav           playSdWav1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, i2s1, 0);
AudioConnection          patchCord2(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;


// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14


// Control pin for the white led strip
#define WHITE_LED_PIN 14

// Buzzer pin
#define BUZZER_PIN 5

Bounce pushbutton = Bounce(BUZZER_PIN, 10);  // 10 ms debounce

void stayinAlive();
void Twinkle(unsigned long endTime, int SpeedDelay);
void animateSnake(int snakeLength, int startPosition, int endPosition, bool clockwise, unsigned long endTime, int color);
void fillQuarters(int msDelay, int colorOffset);
void colorWipeInstant(int color);
void colorWipe(int color, int wait);
void fadeInLED(int pin);

void setup() {
 // Enable white light first
  pinMode(WHITE_LED_PIN, OUTPUT);
  digitalWrite(WHITE_LED_PIN, HIGH);
  
  Serial.begin(9600);
  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);
  sgtl5000_1.audioPostProcessorEnable();
  sgtl5000_1.enhanceBassEnable();
  sgtl5000_1.enhanceBass(0.7, 0.7, 0, 2);
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
  delay(1000); 

  pinMode(BUZZER_PIN, INPUT_PULLUP);

  leds.begin();
  leds.show();
}

byte active = LOW;                 // weather the 
unsigned int count = 0;            // how many times has it changed to low
unsigned long countAt = 0;         // when count changed
unsigned int countPrinted = 0;     // last count printed

void loop() {
  if (active == LOW) {
    digitalWrite(WHITE_LED_PIN, HIGH);
  } else {
    digitalWrite(WHITE_LED_PIN, LOW);
    stayinAlive();

    colorWipeInstant(BLACK);
    fadeInLED(WHITE_LED_PIN);
    active = LOW;
  }

  if (pushbutton.update()) {
    if (pushbutton.fallingEdge() or pushbutton.risingEdge()) {
      if (active == LOW) {
        active = HIGH;
      } else {
        active = LOW;
      }
    }
  }
}

void stayinAlive(){
  if (playSdWav1.isPlaying() == false) {
    Serial.println("Start playing");
    playSdWav1.play("test2.wav");
    delay(10); // wait for library to parse WAV info
  }
    
  unsigned long startTime = millis();
  Twinkle(startTime + 23180, 42);
  fillQuarters(583, 0);
  Twinkle(startTime + 27890, 42);
  fillQuarters(583, 1);
  Twinkle(startTime + 32550, 42);
  fillQuarters(583, 2);
  Twinkle(startTime + 37125, 42);
  fillQuarters(583, 3);
  Twinkle(startTime + 54000, 42);
}

// Light patches in a random color
void Twinkle(unsigned long endTime, int SpeedDelay) {
  colorWipeInstant(DARKWHITE);
  int colorList[8] = {BLUE, RED, GREEN, YELLOW, ORANGE, PINK, WHITE, PASTEL4};
  unsigned long presentTime = millis();
  while (presentTime < endTime) {
 
     int pixelNumber = random(2,118);
     int pixelColor = leds.getPixel(pixelNumber);

      int newColor = colorList[random(8)]; 
        for(int q = pixelNumber-2; q < pixelNumber+2; q++){
          if(pixelColor == DARKWHITE){
            leds.setPixel(q,newColor);

          }
          else {
            leds.setPixel(q,DARKWHITE);
          }
        }
     leds.show();
     delay(SpeedDelay);
     presentTime = millis();
   }
}

void animateSnake(int snakeLength, int startPosition, int endPosition, bool clockwise, unsigned long endTime, int color){
   int positionDifference = 0;
   positionDifference = (120 + (endPosition - startPosition)) % 120;
   if(clockwise){
    positionDifference = positionDifference;
   }
   else {
      positionDifference = 120 - positionDifference; 
   }
   unsigned long presentTime = millis();
   unsigned long timeDifference = endTime - presentTime;
   unsigned long stepTime = timeDifference / positionDifference;

   int presentPosition = startPosition;
   while(presentPosition != endPosition){
       colorWipeInstant(BLACK);
       for(int index = presentPosition - snakeLength; index < presentPosition; index++){
          leds.setPixel((120 + index)%120, color); 
       }
       if(clockwise){
          presentPosition = (120 + presentPosition + 1)% 120; 
       }
       else {
          presentPosition = (120 + presentPosition - 1)% 120;
       }
       leds.show();
       delay(stepTime);
   }
}

void fillQuarters(int msDelay, int colorOffset){
  int colors[8] = {WHITE, RED, GREEN, YELLOW, BLUE, PINK, ORANGE, PASTEL4};
  colorWipeInstant(DARKWHITE);
  int presentSide = 0;
  int presentLed = 0;
  while(presentLed < 120){
      leds.setPixel(presentLed, colors[(presentSide+colorOffset)%8]);
      if(presentLed == 35 || presentLed == 59 || presentLed == 95 || presentLed == 119 ){
        presentSide += 1;
        leds.show();
        delay(msDelay);  
      }
      presentLed += 1;
  }  
}

// Set all pixels to a common color
void colorWipeInstant(int color){
   for (int i=0; i < leds.numPixels(); i++) {
    leds.setPixel(i, color);
  }
  leds.show();
}

void colorWipe(int color, int wait)
{
  for (int i=0; i < leds.numPixels(); i++) {
    leds.setPixel(i, color);
    leds.show();
    delayMicroseconds(wait);
  }
}

// Slowly fade in an LED
void fadeInLED(int pin) {
  for (int fadeValue = 0 ; fadeValue <= 255; fadeValue = fadeValue+5) {
    analogWrite(pin, fadeValue);
    delay(5);
  }
}