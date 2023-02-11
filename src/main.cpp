#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <OctoWS2811.h>
#include <Bounce.h>

#define FASTLED_INTERNAL
#include <FastLED.h>

#include "CTeensy4Controller.h"

// RGB LED
// Any group of digital pins may be used
const int numPins = 1;
byte pinList[numPins] = {2};

const int ledsPerStrip = 120;
const int numLeds = numPins * ledsPerStrip;
CRGB leds[numLeds];

// These buffers need to be large enough for all the pixels.
// The total number of pixels is "ledsPerStrip * numPins".
// Each pixel needs 3 bytes, so multiply by 3.  An "int" is
// 4 bytes, so divide by 4.  The array is created using "int"
// so the compiler will align it to 32 bit memory.
const int bytesPerLED = 3; // change to 4 if using RGBW
DMAMEM int displayMemory[ledsPerStrip * numPins * bytesPerLED / 4];
int drawingMemory[ledsPerStrip * numPins * bytesPerLED / 4];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 octo(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

CTeensy4Controller<GRB, WS2811_800kHz> *pcontroller;

// Audio Player
AudioPlaySdWav playSdWav1;
AudioOutputI2S i2s1;
AudioConnection patchCord1(playSdWav1, 0, i2s1, 0);
AudioConnection patchCord2(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000 sgtl5000_1;

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

// Control pin for the white led strip
#define WHITE_LED_PIN 14

// Buzzer pin
#define BUZZER_PIN 5
Bounce pushbutton = Bounce(BUZZER_PIN, 10); // 10 ms debounce

#define BRIGHTNESS 96
#define FRAMES_PER_SECOND 120

void setup()
{
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
  if (!(SD.begin(SDCARD_CS_PIN)))
  {
    while (1)
    {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
  delay(1000);

  pinMode(BUZZER_PIN, INPUT_PULLUP);

  octo.begin();
  pcontroller = new CTeensy4Controller<GRB, WS2811_800kHz>(&octo);

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.addLeds(pcontroller, leds, numPins * ledsPerStrip);
}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

uint32_t gTimeCode = 0;
uint32_t gLastTimeCodeDoneAt = 0;
uint32_t gLastTimeCodeDoneFrom = 0;

#define TC(HOURS, MINUTES, SECONDS)                         \
  ((uint32_t)(((uint32_t)((HOURS) * (uint32_t)(3600000))) + \
              ((uint32_t)((MINUTES) * (uint32_t)(60000))) + \
              ((uint32_t)((SECONDS) * (uint32_t)(1000)))))

#define AT(HOURS, MINUTES, SECONDS) if (atTC(TC(HOURS, MINUTES, SECONDS)))
#define FROM(HOURS, MINUTES, SECONDS) if (fromTC(TC(HOURS, MINUTES, SECONDS)))

static bool atTC(uint32_t tc)
{
  bool maybe = false;
  if (gTimeCode >= tc)
  {
    if (gLastTimeCodeDoneAt < tc)
    {
      maybe = true;
      gLastTimeCodeDoneAt = tc;
    }
  }
  return maybe;
}

static bool fromTC(uint32_t tc)
{
  bool maybe = false;
  if (gTimeCode >= tc)
  {
    if (gLastTimeCodeDoneFrom <= tc)
    {
      maybe = true;
      gLastTimeCodeDoneFrom = tc;
    }
  }
  return maybe;
}

void rainbow();
void rainbowWithGlitter();
void addGlitter(fract8 chanceOfGlitter);
void confetti();
void bpm();
void juggle();
void applause();
void fadeToBlack();

// There are two kinds of things you can put into this performance:
// "FROM" and "AT".
//
// * "FROM" means starting FROM this time AND CALLING IT REPEATEDLY
//   until the next "FROM" time comes.
//
// * "AT" means do this ONE TIME ONLY "AT" the designated time.
//
// At least one of the FROM clauses will ALWAYS be executed.
// In the transitional times, TWO pieces of code will be executed back to back.
// For example, if one piece says "FROM(0,0,1.000) {DrawRed()}" and another says
// "FROM(0,0,2.000) {flashblue();}", what you'll get is this:
//   00:00:01.950  -> calls DrawRed
//   00:00:01.975  -> calls DrawRed
//   00:00:02.000  -> calls DrawRed AND calls DrawBlue !
//   00:00:02.025  -> calls DrawBlue
//   00:00:02.050  -> calls DrawBlue
// In most cases, this probably isn't significant in practice, but it's important
// to note.  It could be avoided by listing the sequence steps in reverse
// chronological order, but that makes it hard to read.
void Performance()
{
  AT(0, 0, 00.001) { FastLED.setBrightness(BRIGHTNESS); }
  FROM(0, 0, 00.100) { confetti(); }
  AT(0, 0, 23.180) { fill_solid(leds, numLeds, CRGB::Red); }
  AT(0, 0, 23.763) { fill_solid(leds, numLeds, CRGB::Green); }
  AT(0, 0, 24.346) { fill_solid(leds, numLeds, CRGB::Blue); }
  AT(0, 0, 24.929) { fill_solid(leds, numLeds, CRGB::Yellow); }
  FROM(0, 0, 25.512) { confetti(); }
  AT(0, 0, 27.890) { fill_solid(leds, numLeds, CRGB::Red); }
  AT(0, 0, 28.473) { fill_solid(leds, numLeds, CRGB::Green); }
  AT(0, 0, 29.056) { fill_solid(leds, numLeds, CRGB::Blue); }
  AT(0, 0, 29.639) { fill_solid(leds, numLeds, CRGB::Yellow); }
  FROM(0, 0, 29.722) { confetti(); }
  AT(0, 0, 32.550) { fill_solid(leds, numLeds, CRGB::Red); }
  AT(0, 0, 33.133) { fill_solid(leds, numLeds, CRGB::Green); }
  AT(0, 0, 33.716) { fill_solid(leds, numLeds, CRGB::Blue); }
  AT(0, 0, 34.299) { fill_solid(leds, numLeds, CRGB::Yellow); }
  FROM(0, 0, 34.882) { confetti(); }
  AT(0, 0, 37.125) { fill_solid(leds, numLeds, CRGB::Red); }
  AT(0, 0, 37.708) { fill_solid(leds, numLeds, CRGB::Green); }
  AT(0, 0, 38.291) { fill_solid(leds, numLeds, CRGB::Blue); }
  AT(0, 0, 38.874) { fill_solid(leds, numLeds, CRGB::Yellow); }
  FROM(0, 0, 44.707) { confetti(); }
  FROM(0, 0, 49.000) { fadeToBlack(); }
  // FROM(0, 0, 01.500) { juggle(); }
  // FROM(0, 0, 03.375) { rainbowWithGlitter(); }
  // FROM(0, 0, 04.333) { bpm(); }
  // FROM(0, 0, 06.666) { juggle(); }
  // FROM(0, 0, 08.750) { confetti(); }
  // AT(0, 0, 11.000) { gHue = HUE_PINK; }
  // AT(0, 0, 12.000) { fill_solid(leds, numLeds, CRGB::Red); }
  // AT(0, 0, 15.000) { fill_solid(leds, numLeds, CRGB::Blue); }
  // FROM(0, 0, 16.500) { fadeToBlack(); }
  // FROM(0, 0, 18.000) { applause(); }
  // AT(0, 0, 19.000) { FastLED.setBrightness(BRIGHTNESS / 2); }
  // AT(0, 0, 20.000) { FastLED.setBrightness(BRIGHTNESS / 4); }
  // AT(0, 0, 21.000) { FastLED.setBrightness(BRIGHTNESS / 8); }
  // AT(0, 0, 22.000) { FastLED.setBrightness(BRIGHTNESS / 16); }
  // FROM(0, 0, 23.000) { fadeToBlack(); }
}

void loop()
{
  if (pushbutton.update())
  {
    digitalWrite(WHITE_LED_PIN, LOW);

    if (playSdWav1.isPlaying() == false)
    {
      gLastTimeCodeDoneAt = 0;
      gLastTimeCodeDoneFrom = 0;
      Serial.println("Start playing");
      playSdWav1.play("test2.wav");
      delay(10); // wait for library to parse WAV info
    }
  }

  if (playSdWav1.isPlaying())
  {
    // Set the current timecode, based on when the performance started
    gTimeCode = playSdWav1.positionMillis();

    Performance();

    // send the 'leds' array out to the actual LED strip
    FastLED.show();
    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / FRAMES_PER_SECOND);
    // do some periodic updates
    EVERY_N_MILLISECONDS(20) { gHue++; } // slowly cycle the "base color" through the rainbow
  }
  else
  {
    FastLED.setBrightness(0);
    FastLED.show();
    digitalWrite(WHITE_LED_PIN, HIGH);
  }
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, numLeds, gHue, 7);
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter(fract8 chanceOfGlitter)
{
  if (random8() < chanceOfGlitter)
  {
    leds[random16(numLeds)] += CRGB::White;
  }
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, numLeds, 10);
  int pos = random16(numLeds);
  leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < numLeds; i++)
  { // 9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle()
{
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, numLeds, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++)
  {
    leds[beatsin16(i + 7, 0, numLeds)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

// An animation to play while the crowd goes wild after the big performance
void applause()
{
  static uint16_t lastPixel = 0;
  fadeToBlackBy(leds, numLeds, 32);
  leds[lastPixel] = CHSV(random8(HUE_BLUE, HUE_PURPLE), 255, 255);
  lastPixel = random16(numLeds);
  leds[lastPixel] = CRGB::White;
}

// An "animation" to just fade to black.  Useful as the last track
// in a non-looping performance.
void fadeToBlack()
{
  fadeToBlackBy(leds, numLeds, 1);
}