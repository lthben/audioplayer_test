// Advanced Microcontroller-based Audio Workshop
//
// http://www.pjrc.com/store/audio_tutorial_kit.html
// https://hackaday.io/project/8292-microcontroller-audio-workshop-had-supercon-2015
//
// Part 1-5: Respond to Pushbuttons & Volume Knob
//
// Do more while playing.  Monitor pushbuttons and adjust
// the volume.  Whe the buttons are pressed, stop playing
// the current file and skip to the next or previous.

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce.h>

AudioPlaySdWav           playSdWav1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, i2s1, 0);
AudioConnection          patchCord2(playSdWav1, 1, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;

Bounce button0 = Bounce(0, 15);
Bounce button2 = Bounce(2, 15);  // 15 = 15 ms debounce time

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

void setup() {
  Serial.begin(9600);
  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
  pinMode(13, OUTPUT); // LED on pin 13
  pinMode(0, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  delay(1000);
}

int filenumber = 0;  // while file to play

//USER DEFINED SETTINGS
const int NUMFILES = 12;

const char * filelist[NUMFILES] = { 
//    "REC20190916074309.WAV", "REC20190916093819.WAV", "REC20190916164007.WAV", "REC20190917090420.WAV", "REC20190917194508.WAV", "REC20190918073456.WAV", "REC20190918103429.WAV", "REC20190919073700.WAV", "REC20190919210240.WAV", "REC20190919230245.WAV", "REC20190920181658.WAV", "REC20190920182250.WAV" 
//    "SDTEST1.WAV", "SDTEST2.WAV", "SDTEST3.WAV", "SDTEST4.WAV"
    "SDTEST1.WAV", "SDTEST2.WAV", "SDTEST3.WAV", "SDTEST4.WAV", "SDTEST5.WAV", "SDTEST6.WAV", "SDTEST7.WAV", "SDTEST8.WAV", "SDTEST9.WAV", "SDTEST10.WAV", "SDTEST11.WAV", "SDTEST12.WAV"
    };

elapsedMillis blinkTime;

void loop() {

  if (playSdWav1.isPlaying() == false) {
    const char *filename = filelist[filenumber];
    filenumber = filenumber + 1;
    if (filenumber >= NUMFILES) filenumber = 0;
    Serial.print("Start playing ");
    Serial.println(filename);
    playSdWav1.play(filename);
    delay(10); // wait for library to parse WAV info
  }

  // blink the LED without delays
  //  if (blinkTime < 250) {
  //    digitalWrite(13, LOW);
  //  } else if (blinkTime < 500) {
  //    digitalWrite(13, HIGH);
  //  } else {
  //    blinkTime = 0; // start blink cycle over again
  //  }

  // read pushbuttons
  button0.update();
  if (button0.fallingEdge()) {
    playSdWav1.stop();
  }
  button2.update();
  if (button2.fallingEdge()) {
    playSdWav1.stop();
    filenumber = filenumber - 2;
    if (filenumber < 0) filenumber = filenumber + NUMFILES;
  }

  // read the knob position (analog input A2)
  int knob = analogRead(A2);
  float vol = (float)knob / 1280.0;
  sgtl5000_1.volume(vol);
  //  Serial.print("volume = ");
  //  Serial.println(vol);
}
