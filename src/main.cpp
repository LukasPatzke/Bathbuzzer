#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <OctoWS2811.h>
#include <Bounce2.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FASTLED_INTERNAL
#include <FastLED.h>

#include "CTeensy4Controller.h"
#include "BeatDetector.h"

// RGB LED
// Any group of digital pins may be used
const int numPins = 1;
byte pinList[numPins] = {2};

const int ledsPerStrip = 120;
const int NUM_LEDS = numPins * ledsPerStrip;
CRGB leds[NUM_LEDS];
CRGBSet ledset(leds, NUM_LEDS);

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
AudioMixer4 mixer1;
AudioAnalyzeFFT256 fft256_1;
AudioOutputI2S i2s1;
AudioConnection patchCord1(playSdWav1, 0, i2s1, 0);
AudioConnection patchCord2(playSdWav1, 0, mixer1, 0);
AudioConnection patchCord3(playSdWav1, 1, i2s1, 1);
AudioConnection patchCord4(playSdWav1, 1, mixer1, 1);
AudioConnection patchCord5(mixer1, fft256_1);
AudioControlSGTL5000 sgtl5000_1;

BeatDetector beatDetector(fft256_1);

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

// Control pin for the white led strip
#define WHITE_LED_PIN 14

// Buzzer pin
#define BUZZER_PIN 5
Bounce pushbutton = Bounce();

#define BRIGHTNESS 96
#define FRAMES_PER_SECOND 240

void setup()
{
  // Enable white light first
  pinMode(WHITE_LED_PIN, OUTPUT);
  digitalWrite(WHITE_LED_PIN, HIGH);

  Serial.begin(9600);

  // set gains of stereo to mono mixer
  // I think it needs to be .5 to prevent clipping
  mixer1.gain(0, 0.5);
  mixer1.gain(1, 0.5);
  mixer1.gain(2, 0);
  mixer1.gain(3, 0);
  fft256_1.averageTogether(3); // I this gives me about 115 samples per second

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

  pinMode(BUZZER_PIN, INPUT_PULLUP);
  delay(100);
  pushbutton.attach(BUZZER_PIN);
  pushbutton.interval(10);

  octo.begin();
  pcontroller = new CTeensy4Controller<GRB, WS2811_800kHz>(&octo);

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.addLeds(pcontroller, leds, numPins * ledsPerStrip);
}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
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
  if (playSdWav1.positionMillis() >= tc)
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
  if (playSdWav1.positionMillis() >= tc)
  {
    if (gLastTimeCodeDoneFrom <= tc)
    {
      maybe = true;
      gLastTimeCodeDoneFrom = tc;
    }
  }
  return maybe;
}

void quarters(const CRGB &color1, const CRGB &color2, const CRGB &color3, const CRGB &color4);
void pulsing();
void rainbow();
void rainbowWithGlitter();
void addGlitter(fract8 chanceOfGlitter);
void confetti();
void bpm(uint8_t BeatsPerMinute);
void juggle();
void applause(uint8_t width);
void fadeToBlack();
void twoDots();
void fillAndCC();
void blinkyblink2();
void spewFour();
void spew();
void sinelon();
void flashAtBpm(uint8_t BeatsPerMinute, CHSV hue);
void wiggleLines(uint8_t BeatsPerMinute);
void singleFlashAT(uint32_t seconds, CRGB color);
void flashPulsing();
void fillGradual(uint8_t BeatsPerMinute);

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
void StayinAlive()
{
  AT(0, 0, 00.001) { FastLED.setBrightness(BRIGHTNESS); }
  FROM(0, 0, 00.120) { bpm(103); }
  FROM(0, 0, 23.180) { quarters(CRGB::Red, CRGB::Black, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 23.763) { quarters(CRGB::Red, CRGB::Green, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 24.346) { quarters(CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Black); }
  FROM(0, 0, 24.929) { quarters(CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow); }
  FROM(0, 0, 25.512) { pulsing(); }
  FROM(0, 0, 27.890) { fill_solid(leds, NUM_LEDS, CRGB::Orange); }
  FROM(0, 0, 28.473) { fill_solid(leds, NUM_LEDS, CRGB::White); }
  FROM(0, 0, 29.056) { fill_solid(leds, NUM_LEDS, CRGB::Blue); }
  FROM(0, 0, 29.639) { fill_solid(leds, NUM_LEDS, CRGB::Pink); }
  FROM(0, 0, 29.722) { pulsing(); }
  FROM(0, 0, 32.550) { quarters(CRGB::Red, CRGB::Black, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 33.133) { quarters(CRGB::Red, CRGB::Green, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 33.716) { quarters(CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Black); }
  FROM(0, 0, 34.299) { quarters(CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow); }
  FROM(0, 0, 34.882) { pulsing(); }
  FROM(0, 0, 37.125) { fill_solid(leds, NUM_LEDS, CRGB::Orange); }
  FROM(0, 0, 37.708) { fill_solid(leds, NUM_LEDS, CRGB::White); }
  FROM(0, 0, 38.291) { fill_solid(leds, NUM_LEDS, CRGB::Blue); }
  FROM(0, 0, 38.874) { fill_solid(leds, NUM_LEDS, CRGB::Pink); }
  FROM(0, 0, 39.457) { pulsing(); }
  // FROM(0, 0, 39.457) { bpm(103); }
  FROM(0, 0, 49.800) { fadeToBlackBy(leds, NUM_LEDS, 1); }
}

void Celebrate() {
  AT(0, 0, 00.001) { FastLED.setBrightness(BRIGHTNESS); }
  FROM(0, 0, 00.012) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black); }

  FROM(0, 0, 1.06) { bpm(60);}
  FROM(0, 0, 5.620) { fillGradual(30);}
  FROM(0, 0, 7.149) { bpm(60);}

  FROM(0, 0, 9.175) { quarters(CRGB::Salmon, CRGB::Black, CRGB::Black, CRGB::Black);}
  FROM(0, 0, 9.667) { quarters(CRGB::Black, CRGB::Black, CRGB::Blue, CRGB::Black);}
  FROM(0, 0, 10.185) { quarters(CRGB::Black, CRGB::Green, CRGB::Black, CRGB::Black);}
  FROM(0, 0, 10.677) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Orange);}

  FROM(0, 0, 11.185) { quarters(CRGB::Black, CRGB::Salmon, CRGB::Black, CRGB::Black);}
  FROM(0, 0, 11.435) { quarters(CRGB::Black, CRGB::Salmon, CRGB::LightBlue, CRGB::Black);}
  FROM(0, 0, 11.682) { quarters(CRGB::Lime, CRGB::Salmon, CRGB::LightBlue, CRGB::Black);}
  FROM(0, 0, 11.938) { quarters(CRGB::Lime, CRGB::Salmon, CRGB::LightBlue, CRGB::Red);}

  FROM(0, 0, 13.222) { bpm(60);}
  FROM(0,0,16.471) { applause(30); }

  FROM(0, 0, 17.220) { bpm(60);}
  FROM(0, 0, 20.704) { wiggleLines(60);}

  FROM(0, 0, 21.692) { bpm(60);}

  FROM(0, 0, 25.188) { quarters(CRGB::Salmon, CRGB::Black, CRGB::Black, CRGB::Black);}
  FROM(0, 0, 25.667) { quarters(CRGB::Black, CRGB::Black, CRGB::Blue, CRGB::Black);}
  FROM(0, 0, 26.185) { quarters(CRGB::Black, CRGB::Green, CRGB::Black, CRGB::Black);}
  FROM(0, 0, 26.677) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Orange);}

  FROM(0, 0, 27.185) { quarters(CRGB::Black, CRGB::Salmon, CRGB::Black, CRGB::Black);}
  FROM(0, 0, 27.435) { quarters(CRGB::Black, CRGB::Salmon, CRGB::LightBlue, CRGB::Black);}
  FROM(0, 0, 27.682) { quarters(CRGB::Lime, CRGB::Salmon, CRGB::LightBlue, CRGB::Black);}
  FROM(0, 0, 27.938) { quarters(CRGB::Lime, CRGB::Salmon, CRGB::LightBlue, CRGB::Red);}

  FROM(0, 0, 28.645) { wiggleLines(60);}

  FROM(0, 0, 29.644) { bpm(60);}
  FROM(0, 0, 46.5) { fadeToBlackBy(leds, NUM_LEDS, 1); }
}

void Astro()
{
  AT(0, 0, 00.001) { FastLED.setBrightness(BRIGHTNESS); }

  FROM(0, 0, 00.012) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black); }

  FROM(0, 0, 0.983) { flashPulsing();}
  FROM(0, 0, 05.454) { wiggleLines(127); }
  FROM(0, 0, 06.604) { flashPulsing();}
  FROM(0, 0, 07.348) { wiggleLines(127); }
  FROM(0, 0, 08.546) { flashPulsing();}
  FROM(0, 0, 13.086) { wiggleLines(127); }
  FROM(0, 0, 14.276) { flashPulsing();}
  FROM(0, 0, 14.982) { wiggleLines(127); }
  FROM(0, 0, 16.189) { flashPulsing();}
  FROM(0, 0, 20.697) { wiggleLines(127); }
  FROM(0, 0, 21.983) { flashPulsing();}
  FROM(0, 0, 22.622) { wiggleLines(127); }
  FROM(0, 0, 23.8) { flashPulsing();}
  FROM(0,0,27.454){ applause(1); }
  singleFlashAT(30.215, CRGB::White);
  FROM(0, 0, 30.8) { fadeToBlackBy(leds, NUM_LEDS, 1); }
}

void RamaLama()
{
  AT(0, 0, 00.001) { FastLED.setBrightness(BRIGHTNESS); }

  // Rama Lam
  FROM(0, 0, 01.100) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black); }
  // Ding Dong
  FROM(0, 0, 01.629) { quarters(CRGB::LawnGreen, CRGB::Black, CRGB::LawnGreen, CRGB::Black); }
  FROM(0, 0, 02.085) { quarters(CRGB::Black, CRGB::Salmon, CRGB::Black, CRGB::Salmon); }

  // Rama Lam
  FROM(0, 0, 02.587) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black); }
  // Ding Ding Dong
  FROM(0, 0, 03.604) { quarters(CRGB::Salmon, CRGB::Black, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 03.860) { quarters(CRGB::Salmon, CRGB::Black, CRGB::Salmon, CRGB::Black); }
  FROM(0, 0, 04.094) { quarters(CRGB::Black, CRGB::LawnGreen, CRGB::Black, CRGB::LawnGreen); }

  // Ramalamalamalamalamadingdong Ramalamalamalamalamading
  FROM(0, 0, 04.621) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black); }
  // Uhuh Uhuhuh Uhuhuhuh Uhuhuhuhuhuhu
  FROM(0, 0, 08.454) { bpm(127); }
  // Uuuuh Aaaaah
  FROM(0, 0, 19.880) { bpm(254); }
  // Ah.
  FROM(0, 0, 21.730) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black); }

  // O ohoh ohoh ohoh
  FROM(0, 0, 22.228) { quarters(CRGB::Red, CRGB::Black, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 22.702) { quarters(CRGB::Red, CRGB::Green, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 23.304) { quarters(CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Black); }

  // Ive got a girl named
  FROM(0, 0, 23.610) { bpm(127); }

  // Rama Lama Lama Lama
  FROM(0, 0, 25.731) { applause(5); }
  // Ding Dong
  FROM(0, 0, 26.968) { quarters(CRGB::Red, CRGB::Black, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 27.187) { quarters(CRGB::Black, CRGB::Black, CRGB::Red, CRGB::Black); }
  // She said a thing to me
  FROM(0, 0, 27.450) { bpm(127); }
  // Rama Lama Lama Lama
  FROM(0, 0, 29.5) { applause(5); }
  // Ding Dong
  FROM(0, 0, 30.742) { quarters(CRGB::Black, CRGB::Green, CRGB::Black, CRGB::Black); }
  FROM(0, 0, 31.013) { quarters(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Green); }

  // I never set her free, cause shes mine oh
  FROM(0, 0, 31.261) { bpm(127); }
  // Miiiiine
  FROM(0, 0, 34.985) { applause(1); }
  // Uuuuuuha aaaaaaaah

  FROM(0, 0, 34.985) { applause(1); }
  AT(0, 0, 37) { FastLED.setBrightness(BRIGHTNESS / 2); }
  FROM(0, 0, 37) { applause(2); }
  AT(0, 0, 38) { FastLED.setBrightness(BRIGHTNESS / 4); }
  FROM(0, 0, 38) { applause(3); }
  AT(0, 0, 39) { FastLED.setBrightness(BRIGHTNESS / 6); }
  FROM(0, 0, 39) { applause(4); }
  AT(0, 0, 40) { FastLED.setBrightness(BRIGHTNESS / 8); }
  AT(0, 0, 41) { FastLED.setBrightness(BRIGHTNESS / 10); }
  FROM(0, 0, 41) { applause(5); }
  FROM(0, 0, 41.5) { fadeToBlackBy(leds, NUM_LEDS, 1); }

  // aaaaaaaaah
}

void Demo()
{
  AT(0, 0, 00.001) { FastLED.setBrightness(BRIGHTNESS); }
  FROM(0, 0, 01.500) { juggle(); }
  FROM(0, 0, 03.375) { rainbowWithGlitter(); }
  FROM(0, 0, 04.333) { bpm(62); }
  FROM(0, 0, 06.666) { juggle(); }
  FROM(0, 0, 08.750) { confetti(); }
  AT(0, 0, 11.000) { gHue = HUE_PINK; }
  AT(0, 0, 12.000) { fill_solid(leds, NUM_LEDS, CRGB::Red); }
  AT(0, 0, 15.000) { fill_solid(leds, NUM_LEDS, CRGB::Blue); }
  FROM(0, 0, 16.500) { fadeToBlack(); }
  FROM(0, 0, 18.000) { applause(1); }
  AT(0, 0, 19.000) { FastLED.setBrightness(BRIGHTNESS / 2); }
  AT(0, 0, 20.000) { FastLED.setBrightness(BRIGHTNESS / 4); }
  AT(0, 0, 21.000) { FastLED.setBrightness(BRIGHTNESS / 8); }
  AT(0, 0, 22.000) { FastLED.setBrightness(BRIGHTNESS / 16); }
  FROM(0, 0, 23.000) { fadeToBlack(); }
}

// List of patterns to cycle through.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {RamaLama, StayinAlive, Astro, Celebrate};
char *gFilenames[4] = {"rldd.wav", "test2.wav", "astro.wav", "seleb.wav"};
const uint8_t gNumberOfPatterns = 4;

uint8_t gCurrentPatternNumber = 3; // Index number of which pattern is current

void loop()
{
  if (pushbutton.update())
  {
    digitalWrite(WHITE_LED_PIN, LOW);

    if (playSdWav1.isPlaying() == false)
    {
      //gCurrentPatternNumber = (gCurrentPatternNumber + 1) % 3;
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      /* using nano-seconds instead of seconds */
      srand((time_t)ts.tv_nsec);

      gCurrentPatternNumber = random8(rand()%gNumberOfPatterns);
      delay(1000);
      gLastTimeCodeDoneAt = 0;
      gLastTimeCodeDoneFrom = 0;
      Serial.println("Start playing");
      playSdWav1.play(gFilenames[gCurrentPatternNumber]);
      delay(10); // wait for library to parse WAV info
    }
  }

  if (playSdWav1.isPlaying())
  {
    beatDetector.BeatDetectorLoop();

    // StayinAlive();
    gPatterns[gCurrentPatternNumber]();

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

void quarters(const CRGB &color1, const CRGB &color2, const CRGB &color3, const CRGB &color4)
{
  fill_solid(ledset(0, 35), 36, color1);
  fill_solid(ledset(36, 59), 24, color2);
  fill_solid(ledset(60, 95), 36, color3);
  fill_solid(ledset(96, 119), 24, color4);
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, NUM_LEDS, gHue, 7);
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
    leds[random16(NUM_LEDS)] += CRGB::White;
  }
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void flashPulsing()
{
  CRGBPalette16 palette = PartyColors_p;
  fadeToBlackBy(leds, NUM_LEDS, 8);
  if (beatDetector.virtualBeat)
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = CRGB::White;
    }
  }
}

void pulsing()
{
  CRGBPalette16 palette = PartyColors_p;
  fadeToBlackBy(leds, NUM_LEDS, 1);
  if (beatDetector.virtualBeat)
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = ColorFromPalette(palette, gHue + (i * 2), gHue + (i * 10));
    }
  }
}

void singleFlashAT(uint32_t seconds, CRGB color)
{
  AT(0, 0, seconds) { fill_solid(leds, NUM_LEDS, color); }
  FROM(0, 0, seconds + 0.005) { fadeToBlackBy(leds, NUM_LEDS, 2); }
}

void bpm(uint8_t BeatsPerMinute)
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void fillGradual(uint8_t BeatsPerMinute) {
  uint8_t beat = beatsin8(BeatsPerMinute, 0, NUM_LEDS);

  CRGBPalette16 palette = PartyColors_p;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (i <= beat)
    {
      leds[i] = ColorFromPalette(palette, gHue, 120 + ((beat / NUM_LEDS) * 135));

    }
  }
}

void wiggleLines(uint8_t BeatsPerMinute)
{
  int linelength = 10;
  int moving_distance = 40;
  int start_value = 30;
  uint8_t beat = beatsin8(BeatsPerMinute, start_value, start_value + moving_distance);

  CRGBPalette16 palette = PartyColors_p;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = 0; i < 0.5 * NUM_LEDS; i++)
  {
    if ((beat - i) < linelength && (beat - i) > -linelength)
    {
      leds[i] = ColorFromPalette(palette, gHue, 255 - (10 * abs(beat - i)));
      int otherIndex = (0.5 * NUM_LEDS) + i;
      leds[otherIndex] = ColorFromPalette(palette, gHue, 255 - (10 * abs(beat - i)));
    }
  }
}

void flashAtBpm(uint8_t BeatsPerMinute, CHSV hsv)
{
  // Everything pulsing at hue in beat
  CRGBPalette16 palette = PartyColors_p;
  fadeToBlackBy(leds, NUM_LEDS, 5);
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  if (beat < 67)
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = hsv;
    }
  }
}

void flashSingle(const CRGB &color1, const CRGB &color2, const CRGB &color3, const CRGB &color4)
{
  uint8_t beat = beatsin8(3, 64, 255);
  fadeToBlackBy(leds, NUM_LEDS, 5);
  if (beat < 1)
  {
    quarters(color1, color2, color3, color4);
  }
}

void juggle()
{
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, NUM_LEDS, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++)
  {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

// An animation to play while the crowd goes wild after the big performance
void applause(uint8_t width)
{
  static uint16_t lastPixel = 0;
  static uint8_t hue = random8(HUE_BLUE, HUE_PURPLE);
  fadeToBlackBy(leds, NUM_LEDS, 32);
  for (int i = 0; i <= width; i++)
  {
    leds[(lastPixel + i) % NUM_LEDS] = CHSV(hue, 255, 255);
    leds[(lastPixel + NUM_LEDS - i) % NUM_LEDS] = CHSV(hue, 255, 255);
  }

  lastPixel = random16(NUM_LEDS);
  for (int i = 0; i <= width; i++)
  {
    leds[(lastPixel + i) % NUM_LEDS] = CRGB::White;
    leds[(lastPixel + NUM_LEDS - i) % NUM_LEDS] = CRGB::White;
  }
}

// An "animation" to just fade to black.  Useful as the last track
// in a non-looping performance.
void fadeToBlack()
{
  fadeToBlackBy(leds, NUM_LEDS, 1);
}

//////////////////////////
void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, NUM_LEDS, 12);
  int pos = beatsin16(13, 0, NUM_LEDS - 1);
  leds[pos] += CHSV(gHue, 255, 192);
}

/////////////////////////
void spew()
{
  const uint16_t spewSpeed = 100; // rate of advance
  static boolean spewing = 0;     // pixels are On(1) or Off(0)
  static uint8_t count = 1;       // how many to light (or not light)
  static uint8_t temp = count;
  static uint8_t hue = random8();
  EVERY_N_MILLISECONDS(spewSpeed)
  {
    if (count == 0)
    {
      spewing = !spewing;
      if (spewing == 1)
      {
        count = random8(2, 5);
      } // random number for On pixels
      else
      {
        count = random8(1, 8);
      } // random number for Off pixels
      temp = count;
      // gHue = gHue - 30;
      hue = random8();
    }
    for (uint8_t i = NUM_LEDS - 1; i > 0; i--)
    {
      leds[i] = leds[i - 1]; // shift data down the line by one pixel
    }
    if (spewing == 1)
    { // new pixels are On
      if (temp == count)
      {
        leds[0] = CHSV(hue - 5, 215, 255); // for first dot
        // leds[0] = CHSV(gHue-5, 215, 255);  // for first dot
      }
      else
      {
        leds[0] = CHSV(hue, 255, 255 / (1 + ((temp - count) * 2))); // for following dots
        // leds[0] = CHSV(gHue, 255, 255/(1+((temp-count)*2)) );  // for following dots
      }
    }
    else
    {                          // new pixels are Off
      leds[0] = CHSV(0, 0, 0); // set pixel 0 to black
    }
    count = count - 1; // reduce count by one.
  }                    // end every_n
} // end spew

//////////////////////////
void spewFour()
{
  // Similar to the abouve "spew", but split up into four sections,
  // specifically designed for a 8x4 matrix with Z-layout.
  const uint16_t spewSpeed = 100;           // rate of advance
  static uint8_t spewing[4] = {0, 0, 0, 0}; // pixels are On(1) or Off(0)
  static uint8_t count[4] = {1, 1, 1, 1};   // how many to light (or not light)
  static uint8_t temp[4] = {count[0], count[1], count[2], count[3]};
  static uint8_t hue[4] = {random8(), random8(), random8(), random8()};
  EVERY_N_MILLISECONDS(spewSpeed)
  {
    for (uint8_t j = 0; j < 4; j++)
    {
      if (count[j] == 0)
      {
        if (spewing[j] == 0)
        {
          spewing[j] = 1;
        }
        else
        {
          spewing[j] = 0;
        }
        if (spewing[j] == 1)
        {
          count[j] = random8(2, 5);
        } // random number for On pixels
        else
        {
          count[j] = random8(1, 8);
        } // random number for Off pixels
        temp[j] = count[j];
        EVERY_N_SECONDS(2)
        { // hue going across is constant for awhile
          hue[j] = random8();
        }
      }
      for (uint8_t i = 7; i > 0; i--)
      {
        leds[(j * 8) + i] = leds[(j * 8) + i - 1]; // shift data down the line by one pixel
      }
      if (spewing[j] == 1)
      { // new pixels are On
        if (temp[j] == count[j])
        {
          leds[((j + 1) * 8) - 8] = CHSV(hue[j] - 5, 215, 255); // for first dot
        }
        else
        {
          leds[((j + 1) * 8) - 8] = CHSV(hue[j], 255, 255 / (1 + ((temp[j] - count[j]) * 2))); // for following dots
        }
      }
      else
      {                                          // new pixels are Off
        leds[((j + 1) * 8) - 8] = CHSV(0, 0, 0); // set pixel 0 to black
      }
      count[j] = count[j] - 1; // reduce count by one.
    }                          // end for loop
  }                            // end every_n
} // end spewFour

//////////////////////////
void blinkyblink1()
{
  static boolean dataIncoming = LOW;
  static boolean blinkGate1 = LOW;
  static boolean blinkGate2 = HIGH;
  static int8_t count = -1;

  EVERY_N_MILLISECONDS_I(timingObj, 250)
  {
    count++;
    if (count == 6)
    {
      count = 0;
    }
    blinkGate2 = count;
    dataIncoming = !dataIncoming;
    blinkGate1 = !blinkGate1;
    // Serial.print("c: "); Serial.print(count); Serial.print("\t");
    // Serial.print(dataIncoming); Serial.print("  "); Serial.print(blinkGate1);
    // Serial.print("\t"); Serial.print(dataIncoming * blinkGate1 * 255 * blinkGate2);
    // Serial.print("\tb: "); Serial.print(blinkGate2); Serial.println(" ");
    FastLED.clear();
    leds[0] = CHSV(gHue, 0, dataIncoming * blinkGate1 * 255 * blinkGate2);
    if (count == 2 || count == 3)
    {
      timingObj.setPeriod(50);
    }
    else if (count == 4)
    {
      timingObj.setPeriod(405);
    }
    else
    {
      timingObj.setPeriod(165);
    }
  }
} // end_blinkyblink1

//////////////////////////
void blinkyblink2()
{
  static boolean dataIncoming = LOW;
  static boolean blinkGate1 = LOW;
  static boolean blinkGate2 = HIGH;
  static int8_t count = -1;
  static int8_t P;

  EVERY_N_MILLISECONDS_I(timingObj, 250)
  {
    count++;
    if (count == 8)
    {
      count = 0;
      P = random8(NUM_LEDS);
    }
    blinkGate2 = count;
    dataIncoming = !dataIncoming;
    blinkGate1 = !blinkGate1;
    // Serial.print("c: "); Serial.print(count); Serial.print("\t");
    // Serial.print(dataIncoming); Serial.print("  "); Serial.print(blinkGate1);
    // Serial.print("\t"); Serial.print(dataIncoming * blinkGate1 * 255 * blinkGate2);
    // Serial.print("\tb: "); Serial.print(blinkGate2); Serial.println(" ");
    FastLED.clear();
    leds[P] = CHSV(gHue, 255, dataIncoming * blinkGate1 * 255 * blinkGate2);
    if (count == 6)
    {
      timingObj.setPeriod(250);
    }
    else if (count == 7)
    {
      timingObj.setPeriod(500);
    }
    else
    {
      timingObj.setPeriod(25);
    }
  }
} // end_blinkyblink2

//////////////////////////
void fillAndCC()
{
  static int16_t pos = 0;  // position along strip
  static int8_t delta = 3; // delta (can be negative, and/or odd numbers)
  static uint8_t hue = 0;  // hue to display
  EVERY_N_MILLISECONDS(50)
  {
    leds[pos] = CHSV(hue, 255, 255);
    pos = (pos + delta + NUM_LEDS) % NUM_LEDS;
    if (delta >= 0 && pos == 0)
    { // going forward
      hue = hue + random8(42, 128);
    }
    if (delta < 0 && pos == NUM_LEDS - 1)
    { // going backward
      hue = hue + random8(42, 128);
    }
  }
} // fillAndCC

//////////////////////////
void twoDots()
{
  static uint8_t pos; // used to keep track of position
  EVERY_N_MILLISECONDS(70)
  {
    fadeToBlackBy(leds, NUM_LEDS, 200); // fade all the pixels some
    leds[pos] = CHSV(gHue, random8(170, 230), 255);
    leds[(pos + 5) % NUM_LEDS] = CHSV(gHue + 64, random8(170, 230), 255);
    pos = pos + 1; // advance position

    // This following check is very important.  Do not go past the last pixel!
    if (pos == NUM_LEDS)
    {
      pos = 0;
    } // reset to beginning
    // Trying to write data to non-existent pixels causes bad things.
  }
} // end_twoDots