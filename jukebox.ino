/*Sketch written by Ompyl for a music playing jukebox like in Minecraft
dfplayer module & pn532 have to be connected
I know this code is piece a shit but here it is if anyone needs.
*/
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <SoftwareSerial.h>
#include <DFMiniMp3.h>
#include <EEPROM.h>

#define PN532_IRQ   (2)
#define PN532_RESET (3)

#define SDA_PIN A4
#define SCL_PIN A5

#define BUTTON_UP_PIN 2
#define BUTTON_DOWN_PIN 3

#define ERROR_TRACK 999

volatile bool buttonUpState = false;
volatile bool buttonDownState = false;

volatile int volume;
int lasttrack = 0;
int rdtrack = 0;

class Mp3Notify;
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3;
SoftwareSerial mySoftwareSerial(4, 5); //RX/TX for audio module
DfMp3 dfmp3(mySoftwareSerial);

class Mp3Notify
{
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action)
  {
    if (source & DfMp3_PlaySources_Sd) 
    {
      Serial.print("SD Card, ");
    }
    
    Serial.println(action);
  }
  static void OnError([[maybe_unused]] DfMp3& mp3, uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }
  static void OnPlayFinished([[maybe_unused]] DfMp3& mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track)
  {
    Serial.print("Play finished for #");
    Serial.println(track);
  }
  static void OnPlaySourceOnline([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
  }
};

struct Card {
    uint8_t uid[7];  // Max 7 bytes for UID
    int uidLength;   // UID length
    int track;       // MP3 track
};

Card cards[] = { 
  { { 0xA9, 0x96, 0xAA, 0x87 }, 4, 1 },
  { { 0x04, 0x67, 0x23, 0x72, 0xAC, 0x37, 0x80 }, 7, 2 },
  { { 0x04, 0x67, 0x23, 0x72, 0xAC, 0x37, 0x80 }, 7, 3 },
  { { 0x04, 0x2E, 0xB2, 0xAA, 0x5E, 0x71, 0x81 }, 7, 90 }, //Platte 2
  { { 0x04, 0x4D, 0xB3, 0xAA, 0x5E, 0x71, 0x81 }, 7, 13 }, //Platte 1
  { { 0x04, 0x6C, 0x23, 0x72, 0xAC, 0x37, 0x80 }, 7, 6 }
};

int numberOfCards = sizeof(cards) / sizeof(cards[0]);

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);

void increaseVolume(void) {
  if (volume < 30) {
    Serial.print("Volume up --> "); Serial.println(volume);
    volume++;
    EEPROM.write(0, volume);
  } else {
    Serial.println("Volume has to be 0-30! now set to 30");
    volume = 30;
    EEPROM.update(0, 30);
  }
}

void decreaseVolume(void) {
  if (volume > 0) {
    Serial.print("Volume down --> "); Serial.println(volume);
    volume--;
    EEPROM.write(0, volume);
  } else {
    Serial.println("Volume has to be 0-30! now set to 15");
    volume = 15;
    EEPROM.update(0, 15);
  }
}

void setup(void) {
  Serial.begin(9600);
  mySoftwareSerial.begin(9600);
  //while (!Serial) delay(10);
  Serial.println(""); Serial.println("");
  Serial.println("---Hello and welcome to the Los pollos hermanos family. My name is Gustavo but you can call me Gus---");

  nfc.begin();
  dfmp3.begin();

  pinMode(BUTTON_UP_PIN, INPUT);
  pinMode(BUTTON_DOWN_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON_UP_PIN), increaseVolume, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_DOWN_PIN), decreaseVolume, RISING);

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }

  volume = EEPROM.read(0);

  if (volume <= 0) {
    Serial.println("Volume has to be 0-30! now set to 15");
    volume = 15;
    EEPROM.write(0, 15);
  }

  if (volume > 30) {
    Serial.println("Volume has to be 0-30! now set to 30");
    volume = 30;
    EEPROM.write(0, 30);
  }

  dfmp3.setVolume(volume);

  // Print data
  Serial.print("NFC chip: PN5"); Serial.print((versiondata>>24) & 0xFF, HEX); Serial.print(" v");
  Serial.print((versiondata>>16) & 0xFF, DEC); Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  Serial.print(numberOfCards);  Serial.println(" Cards stored");
  Serial.println("");
  dfmp3.getStatus();
  Serial.print("DfPlayer status: "); Serial.println("o");
  Serial.print("Tracks available: "); Serial.println(dfmp3.getTotalTrackCount());
  Serial.print("Volume: "); Serial.println(volume);
  Serial.println("");

  dfmp3.awake();
  delay(11);
  dfmp3.stop();
  Serial.println("Waiting...");

  
  //dfmp3.playMp3FolderTrack(998);  //speakertest
}


void loop(void) {
  uint8_t success;
  uint8_t uid[7];
  uint8_t uidLength;
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    nfc.PrintHex(uid, uidLength);
    for (int i = 0; i < numberOfCards; i++) {
      if (memcmp(uid, cards[i].uid, uidLength) == 0) {

        if ((90 <= cards[i].track) && (cards[i].track <= 99) && (lasttrack != 90)) {
            dfmp3.setVolume(volume);
            rdtrack = 90 + random(9);
            dfmp3.playMp3FolderTrack(rdtrack);
            lasttrack = 90;
            Serial.print("Playing random ov track #"); Serial.println(rdtrack);
            break;
          }

        if ((80 <= cards[i].track) && (cards[i].track <= 89) && (lasttrack != 80)) {
            dfmp3.setVolume(volume);
            rdtrack = 80 + random(9);
            dfmp3.playMp3FolderTrack(rdtrack);
            lasttrack = 80;
            Serial.print("Playing random ne track #"); Serial.println(rdtrack);
            break;
          }

        if (lasttrack != cards[i].track) {
          dfmp3.setVolume(volume);
          dfmp3.playMp3FolderTrack(cards[i].track);
          lasttrack = cards[i].track;
          Serial.print("Playing track #"); Serial.println(cards[i].track);
        } else {
          Serial.println("Track already playing!");
        }
        break;
      } else {
        Serial.print("has checked "); Serial.print(i); Serial.println(" entries");
        if (cards[i].track == numberOfCards) {
          Serial.println("Card not in list");
          lasttrack = ERROR_TRACK;
          dfmp3.setVolume(30);
          dfmp3.playMp3FolderTrack(ERROR_TRACK);
        }
      }
    }
    Serial.println("Search done!");
  }
}