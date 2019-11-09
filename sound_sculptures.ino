/*
  Author: Benjamin Low (Lthben@gmail.com)
  Date: Oct 2019
  Description: 
      Teensy 3.2 with audio shield. 
      Both sculptures have 6 buttons for activating 6 different sounds. 
      A pair of neon flex led strips acting as a VU meter during playback for all sounds. 
      Idle mode has a background sound playback.   
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce.h>
#include <FastLED.h>
#include <elapsedMillis.h>

//-------------------- USER DEFINED SETTINGS --------------------//

//Comment out one of the two below
// #define __WILLIAM__
#define __JIMMY__

const int J_NUMLEDS = 42, W_NUMLEDS = 53;

const int NUMFILES = 6, NUMBGFILES = 3;

CRGB j_color = CRGB::Purple;
CRGB w_color = CRGB::DeepPink;

//SDcard file names must have < 10 characters and only uppercase alphanumeric.
String w_playlist[NUMFILES] = {"WILLIAM1.WAV", "WILLIAM2.WAV", "WILLIAM3.WAV", "WILLIAM4.WAV", "WILLIAM5.WAV", "WILLIAM6.WAV"};
int w_dbLvl[NUMFILES] = {72, 85, 50, 47, 58, 69}; //base db levels
String j_playlist[NUMFILES] = {"JIMMY1.WAV", "JIMMY2.WAV", "JIMMY3.WAV", "JIMMY4.WAV", "JIMMY5.WAV", "JIMMY6.WAV"};
int j_dbLvl[NUMFILES] = {55, 53, 51, 60, 56, 55}; //2 & 4 of 1 - 6 are jet sounds

String w_bgPlaylist[NUMBGFILES] = {"WILLBG1.WAV", "WILLBG2.WAV", "WILLBG3.WAV"};
int w_bgDbLvl[NUMBGFILES] = {78, 78, 78};
String j_bgPlaylist[NUMBGFILES] = {"JIMBG1.WAV", "JIMBG2.WAV", "JIMBG3.WAV"};
int j_bgDbLvl[NUMBGFILES] = {51, 52, 56};

const float MASTERVOL = 0.7; //0 - 1
#define BRIGHTNESS 255
#define UPDATES_PER_SECOND 1000 //speed of light animation. Typical value is 120. Higher speed for faster VU level response.

//-------------------- Audio --------------------//

#if defined(__WILLIAM__)
String playlist[NUMFILES] = {w_playlist[0], w_playlist[1], w_playlist[2], w_playlist[3], w_playlist[4], w_playlist[5]};
int dbLvl[NUMFILES] = {w_dbLvl[0], w_dbLvl[1], w_dbLvl[2], w_dbLvl[3], w_dbLvl[4], w_dbLvl[5]};
String bgPlaylist[NUMBGFILES] = {w_bgPlaylist[0], w_bgPlaylist[1], w_bgPlaylist[2]};
int bgDbLvl[NUMBGFILES] = {w_bgDbLvl[0], w_bgDbLvl[1], w_bgDbLvl[2]};
const int NUM_LEDS = W_NUMLEDS;
const int SCULPTURE_ID = 1;
CRGB myColor = w_color;
#elif defined(__JIMMY__)
String playlist[NUMFILES] = {j_playlist[0], j_playlist[1], j_playlist[2], j_playlist[3], j_playlist[4], j_playlist[5]};
int dbLvl[] = {j_dbLvl[0], j_dbLvl[1], j_dbLvl[2], j_dbLvl[3], j_dbLvl[4], j_dbLvl[5]};
String bgPlaylist[NUMBGFILES] = {j_bgPlaylist[0], j_bgPlaylist[1], j_bgPlaylist[2]};
int bgDbLvl[] = {j_bgDbLvl[0], j_bgDbLvl[1], j_bgDbLvl[2]};
const int NUM_LEDS = J_NUMLEDS;
const int SCULPTURE_ID = 2;
CRGB myColor = j_color;
#else
#error "invalid selection for __WILLIAM__ or __JIMMY__"
#endif

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
int baseDbLvl;                  //of the current sound playing
bool isJetSound;
elapsedMillis msecs; //for peak sampling

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

//-------------------- Buttons --------------------//
//make sure pinMode is also set to the right pins
Bounce button0 = Bounce(0, 15); // 15 = 15 ms debounce time
Bounce button1 = Bounce(1, 15);
Bounce button2 = Bounce(2, 15);
Bounce button3 = Bounce(3, 15);
Bounce button4 = Bounce(19, 15);
Bounce button5 = Bounce(18, 15);

bool isButtonPressed = false; //track response to button triggered
bool isIdleMode = true; 
bool hasTransit = false; //tracking whether to turn off leds once during transition

//-------------------- Light --------------------//
#define LSTRIP_PIN 21
#define RSTRIP_PIN 20

#define LED_TYPE WS2811
#define COLOR_ORDER GRB //Yes! GRB!

CRGB lstrip[NUM_LEDS];
CRGB rstrip[NUM_LEDS];

elapsedMillis ledmsecs;
long randNumber;
bool hasDoneRun;     //tracks whether the idle animation has completed a run
bool isUpDir = true; //tracks direction of running pixel
int ledCounter;      //tracks running pixel position

//-------------------- Setup --------------------//

void setup()
{
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(19, INPUT_PULLUP);
  pinMode(18, INPUT_PULLUP);

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

  FastLED.addLeds<LED_TYPE, LSTRIP_PIN, COLOR_ORDER>(lstrip, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, RSTRIP_PIN, COLOR_ORDER>(rstrip, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  delay(10);
}

//-------------------- Loop --------------------//

void loop()
{
  read_pushbuttons();

  //Respond to button press
  if (isButtonPressed == true)
  {
    isButtonPressed = false; //ready to start listening again for button presses
    isIdleMode = false;
    hasTransit = false; //has yet to transit to idle state

    if (playSdWav1.isPlaying() == true)
    {
      playSdWav1.stop();
    }
    String filename = playlist[fileNum];

    String string1 = String(filename);
    String string2 = String("JIMMY2.WAV");
    String string3 = String("JIMMY4.WAV");
    if (string1 == string2 || string1 == string3)
    {
      isJetSound = true;
      // Serial.println("is jet sound: TRUE");
    }
    else
    {
      isJetSound = false;
      // Serial.println("is jet sound: FALSE");
    }

    char buf[filename.length() + 1];
    filename.toCharArray(buf, filename.length() + 1);
    playSdWav1.play(buf);
    delay(10);
    Serial.print("Start playing ");
    Serial.println(buf);
    baseDbLvl = dbLvl[fileNum];
  }

  //Idle mode
  if (playSdWav1.isPlaying() == false)
  {
    isIdleMode = true; 

    if (hasTransit == false)
    {
      turn_off_leds();
      hasTransit = true;
    }

    bgFileNum = (bgFileNum + 1) % NUMBGFILES; //play next background sound
    String filename = bgPlaylist[bgFileNum];

    char buf[filename.length() + 1];
    filename.toCharArray(buf, filename.length() + 1);
    playSdWav1.play(buf);
    delay(10);
    Serial.print("Start playing ");
    Serial.println(buf);
    baseDbLvl = bgDbLvl[bgFileNum];
  }

  //Visualise sound volume levels
  if (msecs > 40 && isIdleMode == false) //peak levels
  {
    if (peak1.available() && peak2.available())
    {
      msecs = 0;
      float leftNumber = peak1.read(); //0.0 - 1.0
      float rightNumber = peak2.read();
      int leftPeak, rightPeak;
      if (!isJetSound) //not jet sound
      {
        leftPeak = ((leftNumber * 20.0) + baseDbLvl) / 100.0 * NUM_LEDS;
        rightPeak = ((rightNumber * 20.0) + baseDbLvl) / 100.0 * NUM_LEDS;
      }
      else //louder for jet sounds
      {
        leftPeak = ((leftNumber * 50.0) + baseDbLvl) / 100.0 * NUM_LEDS;
        rightPeak = ((rightNumber * 50.0) + baseDbLvl) / 100.0 * NUM_LEDS;
      }

      //left channel strip
      for (int i = 0; i < NUM_LEDS; i++)
      {
        if (i <= leftPeak)
        {
          if (i >= int(0.9 * NUM_LEDS))
            lstrip[i] = CRGB::Red;           //above 90dB
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
      // Serial.print(leftPeak);
      // Serial.print(", ");
      // Serial.print(rightPeak);
      // Serial.println();
    }
  }

  if (isIdleMode == true)
  {
    run_idle_animation();
  }

  FastLED.show();
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}

//-------------------- Support functions --------------------//

void run_idle_animation()
{
  if (hasDoneRun == false)
  {
    if (ledmsecs > int(randNumber / NUM_LEDS))
    {
      if (isUpDir == true)
      {
        ledCounter++;
        if (ledCounter == NUM_LEDS - 1)
        {
          lstrip[ledCounter] = CRGB::Black;
          rstrip[ledCounter] = CRGB::Black;
          hasDoneRun = true;
          isUpDir = false;
        }
        else
        {
          lstrip[ledCounter] = myColor;
          rstrip[ledCounter] = myColor;
          lstrip[ledCounter - 1] = CRGB::Black;
          rstrip[ledCounter - 1] = CRGB::Black;
          ledmsecs = 0;
        }
      }
      else
      { //isUpDir is false
        ledCounter--;
        if (ledCounter == 0)
        {
          lstrip[1] = CRGB:: Black;
          rstrip[1] = CRGB:: Black;
          hasDoneRun = true;
          isUpDir = true;
        }
        else
        {
          lstrip[ledCounter] = myColor;
          rstrip[ledCounter] = myColor;
          lstrip[ledCounter + 1] = CRGB::Black;
          rstrip[ledCounter + 1] = CRGB::Black;
          ledmsecs = 0;
        } 
      }
    }
  }
  else //hasDoneRun is true
  { //generate a new random led speed
    randNumber = random(2000, 6000);
    ledmsecs = 0;
    hasDoneRun = false; //begin a new run
  }
}

void turn_off_leds() 
{
  for (int i = 0; i<NUM_LEDS; i++)
  {
    lstrip[i] = CRGB::Black;
    rstrip[i] = CRGB::Black;
  }
}

void read_pushbuttons()
{
  button0.update();
  button1.update();
  button2.update();
  button3.update();
  button4.update();
  button5.update();

  if (button0.fallingEdge())
  {
    isButtonPressed = true;
    fileNum = 0;
    Serial.println("button0 pressed");
  }

  if (button1.fallingEdge())
  {
    isButtonPressed = true;
    fileNum = 1;
    Serial.println("button1 pressed");
  }

  if (button2.fallingEdge())
  {
    isButtonPressed = true;
    fileNum = 2;
    Serial.println("button2 pressed");
  }

  if (button3.fallingEdge())
  {
    isButtonPressed = true;
    fileNum = 3;
    Serial.println("button3 pressed");
  }

  if (button4.fallingEdge())
  {
    isButtonPressed = true;
    fileNum = 4;
    Serial.println("button4 pressed");
  }

  if (button5.fallingEdge())
  {
    isButtonPressed = true;
    fileNum = 5;
    Serial.println("button5 pressed");
  }
}