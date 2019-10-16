/*
  Author: Benjamin Low (Lthben@gmail.com)
  Date: Oct 2019
  Description: 6 buttons. 6 sounds. A pair of neon flex led strips as a VU meter during playback. Teensy 3.2 with audio shield.
*/

//-------------------- USER DEFINED SETTINGS --------------------//

const int NUMFILES = 6, NUMBGFILES = 3; 

//SDcard file names must have < 10 characters and only alphanumeric
const char *playlist[NUMFILES] = {"WILLIAM1.WAV", "WILLIAM2.WAV", "WILLIAM3.WAV", "WILLIAM4.WAV", "WILLIAM5.WAV", "WILLIAM6.WAV"}; 
const int dbLvl[NUMFILES] = {69, 50, 58, 47, 85, 72}; //base db levels

const char *bgPlaylist[NUMBGFILES] = {"WILLBG1.WAV", "WILLBG2.WAV", "WILLBG3.WAV"};
const int bgDbLvl[NUMBGFILES] = {78, 78, 78};

const float MASTERVOL = 0.7; //0 - 1
#define NUM_LEDS 26 //10cm per pixel
#define BRIGHTNESS 255

//-------------------- Audio --------------------//
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce.h>

AudioPlaySdWav playSdWav1;
AudioAnalyzePeak peak1;
AudioAnalyzePeak peak2;
AudioOutputI2S i2s1;
AudioConnection patchCord1(playSdWav1, 0, i2s1, 0);
AudioConnection patchCord2(playSdWav1, 1, i2s1, 1);
AudioConnection patchCord3(playSdWav1, 0, peak1, 0);
AudioConnection patchCord4(playSdWav1, 1, peak2, 0);
AudioControlSGTL5000 sgtl5000_1;

int fileNum = 0, bgFileNum = 0; // which background file to play
int baseDbLvl; //of the current sound playing
elapsedMillis msecs; //for peak sampling

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

//-------------------- Buttons --------------------//

Bounce button0 = Bounce(0, 15); // 15 = 15 ms debounce time
Bounce button1 = Bounce(1, 15);
Bounce button2 = Bounce(2, 15);
Bounce button3 = Bounce(3, 15);
Bounce button4 = Bounce(4, 15);
Bounce button5 = Bounce(5, 15);

bool isButtonPressed = false; //track response to button triggered 

//-------------------- Light --------------------//
#include <FastLED.h>

#define LSTRIP_PIN 6
#define RSTRIP_PIN 8

#define LED_TYPE UCS1903
#define COLOR_ORDER GRB //Yes! GRB! 

CRGB lstrip[NUM_LEDS];
CRGB rstrip[NUM_LEDS];

#define UPDATES_PER_SECOND 100

//-------------------- setup --------------------//

void setup()
{
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);

  Serial.begin(9600);

  AudioMemory(8);

  sgtl5000_1.enable();

  sgtl5000_1.volume(MASTERVOL);

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
  delay(2000); //power up safety delay

  FastLED.addLeds<UCS1903, LSTRIP_PIN, COLOR_ORDER>(lstrip, NUM_LEDS);
  FastLED.addLeds<UCS1903, RSTRIP_PIN, COLOR_ORDER>(rstrip, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  delay(10);
}

//-------------------- loop --------------------//

void loop()
{
  read_pushbuttons();

  if (isButtonPressed == true) //process response to button press
  {
    if (playSdWav1.isPlaying() == true) playSdWav1.stop();
    const char *filename = playlist[fileNum];
    playSdWav1.play(filename);
    delay(10);
    Serial.print("Start playing ");
    Serial.println(filename);
    baseDbLvl = dbLvl[fileNum];
    isButtonPressed = false; //ready to start listening again for button presses
  }

  if (playSdWav1.isPlaying() == false) //idle mode
  {
    bgFileNum = (bgFileNum+1) % NUMBGFILES;
    const char *filename = bgPlaylist[bgFileNum];
    playSdWav1.play(filename); 
    delay(10);
    Serial.print("Start playing ");
    Serial.println(filename); 
    baseDbLvl = bgDbLvl[bgFileNum];
  }
  
  if (msecs > 40) //peak levels
  {
    if (peak1.available() && peak2.available())
    {
      msecs = 0;
      float leftNumber = peak1.read(); //0.0 - 1.0
      float rightNumber = peak2.read();
      int leftPeak = ((leftNumber * 20.0) + baseDbLvl) / 100.0 * NUM_LEDS;
      int rightPeak = ((rightNumber * 20.0) + baseDbLvl) / 100.0 * NUM_LEDS;

      //left channel strip
      for (int i = 0; i < NUM_LEDS; i++)
      {
        if (i <= leftPeak)
        {
          if (i >= int(0.9 * NUM_LEDS))
            lstrip[i] = CRGB::Red; //above 90dB
          else if (i >= int(0.8 * NUM_LEDS)) //above 80dB
            lstrip[i] = CRGB::Yellow;
          else
            lstrip[i] = CRGB::Green;
        }
        else
          lstrip[i] = CRGB::Black;
      }

      //right channel strip
      for (int i = 0; i < NUM_LEDS; i++)
      {
        if (i <= rightPeak)
        {
          if (i >= int(0.9 * NUM_LEDS))
            rstrip[i] = CRGB::Red;
          else if (i >= int(0.8 * NUM_LEDS))
            rstrip[i] = CRGB::Yellow;
          else
            rstrip[i] = CRGB::Green;
        }
        else
          rstrip[i] = CRGB::Black;
      }

      FastLED.show();

      Serial.print(leftPeak);
      Serial.print(", ");
      Serial.print(rightPeak);
      Serial.println();
    }
  }
}

//-------------------- Support functions --------------------//

void read_pushbuttons()
{
  button0.update();  
  if (button0.fallingEdge())
  {
    isButtonPressed = true; 
    fileNum = 0;
  }

  button1.update();
  if (button1.fallingEdge())
  {
    isButtonPressed = true; 
    fileNum = 1;
  }
  
  button2.update();
  if (button2.fallingEdge())
  {
    isButtonPressed = true; 
    fileNum = 2;
  }

  button3.update();
  if (button3.fallingEdge())
  {
    isButtonPressed = true; 
    fileNum = 3;
  }

  button4.update();
  if (button4.fallingEdge())
  {
    isButtonPressed = true; 
    fileNum = 4;
  }

  button5.update();
  if (button5.fallingEdge())
  {
    isButtonPressed = true; 
    fileNum = 5;
  }      
}