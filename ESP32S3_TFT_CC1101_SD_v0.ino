/*
  https://www.waveshare.com/product/esp32-s3-lcd-1.47.htm

  Board: ESP32S3 Dev Module
  Flash Size: 16MB(128Mb)
  Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
  PSRAM: OPI PSRAM
*/

#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library
#include <Adafruit_NeoPixel.h>
#include "cc1101.h"

#include "FS.h"
#include "SD_MMC.h"

#define BOOT_BUTTON 0  // GPIO0

//----------------------------------------------------------------
// FORD TPMS
//----------------------------------------------------------------


//----------------------------------------------------------------
// CC1101 pinout
//----------------------------------------------------------------
#define CC1101_CS   1//1
#define CC1101_GDO0 2//2
#define CC1101_GDO2 3//3

//----------------------------------------------------------------
// SPI pins for CC1101
//----------------------------------------------------------------
#define CC1101_SCK   5//4
#define CC1101_MISO  6//5
#define CC1101_MOSI  4//6

//----------------------------------------------------------------
// SD
//----------------------------------------------------------------
#define SD_SCLK 14
#define SD_MOSI 15
#define SD_MISO 16
#define SD_SD1 18
#define SD_SD2 17
#define SD_CS 21
bool sd_ok = false;
String msg = "";

//----------------------------------------------------------------
// WS-LED SETTINGS 
//----------------------------------------------------------------
#define NUMPIXELS 1
#define LED 38
Adafruit_NeoPixel pixels(NUMPIXELS, LED, NEO_RGB + NEO_KHZ800);

//----------------------------------------------------------------
// TIMERS
//----------------------------------------------------------------
unsigned long t0 = 0;
unsigned long t1 = 0;
int i = 0;
int i_prev = 0;

//----------------------------------------------------------------
// TFT 
//----------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
int x_px = 0, y_px = 0;
int color_px = TFT_WHITE;

//----------------------------------------------------------------
// CC1101
//----------------------------------------------------------------
SPIClass spiCC(VSPI);
CC1101 radio(spiCC, CC1101_CS, CC1101_GDO0, CC1101_MISO);

//----------------------------------------------------------------
// SD LS
//----------------------------------------------------------------
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  fs::File root = fs.open(dirname);  // <-- change here
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  fs::File file = root.openNextFile();  // <-- and here
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  fs::File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  fs::File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
}

void readFileLN(fs::FS &fs, const char *path) {
  fs::File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.length() >= 256) continue;

    //processLine(line);
  }

  file.close();
}

//----------------------------------------------------------------
// SETUP
//----------------------------------------------------------------
void setup() {

  tft.init();
  delay(250);
  pixels.begin();
  pixels.clear();
  Serial.begin(115200);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  delay(500);

  // ----- SD
  if(! SD_MMC.setPins(SD_SCLK, SD_MOSI, SD_MISO, SD_SD1, SD_SD2, SD_CS)){
    Serial.println("Pin change failed!");
  }
  else{
    if (!SD_MMC.begin()) {
      Serial.println("Card Mount Failed");
    }
    else{
      uint8_t cardType = SD_MMC.cardType();
      if (cardType == CARD_NONE) {
        Serial.println("No SD_MMC card attached");
      }
      else{
        Serial.print("SD_MMC Card Type: ");
        if (cardType == CARD_MMC) {
          Serial.println("MMC");
        } else if (cardType == CARD_SD) {
          Serial.println("SDSC");
        } else if (cardType == CARD_SDHC) {
          Serial.println("SDHC");
        } else {
          Serial.println("UNKNOWN");
        }
        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
        listDir(SD_MMC, "/", 0);
        Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
        Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
        sd_ok = true;
      }
    }
  }

  // ----- CC1101 INIT
  spiCC.begin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_CS);

  radio.begin();
  radio.set_frequency(433.919830);
  radio.serial_data();
  pinMode(CC1101_GDO0, INPUT);
  radio.strobe(0x34); // SRX – enter RX mode
 
  uint8_t c1101_ver = radio.status_version();
  if(c1101_ver == 0x14){
    Serial.printf("[+] CC1101 INIT OK\n");
  }
  else{
    Serial.printf("[-] CC1101 INIT FAILD\n");
  }
  Serial.printf("PARTNUM=0x%02X VERSION=0x%02X\n", radio.status_partnum(), radio.status_version());

  // ----- 
  color_px = TFT_GREEN;

  Serial.println("RAW RF CAPTURE READY");

  if(sd_ok){
    msg = String(millis()) + "ms --- NEW SESSION setOOK v2 ---" + "\n";
    appendFile(SD_MMC, "/data4.txt", msg.c_str());
  }

  //readFile(SD_MMC, "/data1.txt");
  //readFileLN(SD_MMC, "/data1.txt"); //49, 42, 41, 5, 3, 5, 32, 8, 11, 3, 7, 2, 10, 3, 36, 8, 5, 3, 33, 7, 15, 8, 4, 6, 8, 22, 3, 7, 18, 15001602,
}


//----------------------------------------------------------------
// LOOP
//----------------------------------------------------------------
void loop() {

  tft.invertDisplay(true); // Where i is true or false
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLUE);
  tft.setTextSize(2);
  tft.setTextFont(2);

  unsigned long one = 0;
  unsigned long zero = 0;
  while(true){

    static uint32_t t = 0;
    if (millis() - t > 10) {
      t = millis();
      ///Serial.println(digitalRead(CC1101_GDO0));
      if(digitalRead(CC1101_GDO0))
        one++;
      else
        zero++;
    }


    if(millis() - t0 > 12){
      walking_px();
      t0 = millis();
    }

    if(millis() - t1 > 1000){
      Serial.printf("%ds 0/1 = %d/%d\n", i, zero, one);

      i++;

      t1 = millis();
      tft.setCursor(7, 10);
      tft.setTextColor(TFT_BLACK);
      tft.println(i_prev);

      tft.setCursor(7, 10);
      tft.setTextColor(TFT_BLUE);
      tft.println(i);

      i_prev = i;
    }

    if (digitalRead(BOOT_BUTTON) == LOW) {
      Serial.println("Button");
      delay(250);
    }

  }
}

void walking_px(){
  int color = color_px;

  if(x_px < tft.width()-4 && y_px == 0)
    x_px++;
  else if(x_px >= tft.width()-4 && y_px < tft.height()-4)
    y_px++;
  else if(x_px > 0 && y_px >= tft.height()-4)
    x_px--;
  else if(x_px == 0 && y_px > 0)
    y_px--;

  tft.drawRect(0, 0, tft.width()-1, tft.height()-1, TFT_BLACK);
  tft.drawRect(1, 1, tft.width()-3, tft.height()-3, TFT_BLACK);
  tft.drawRect(2, 2, tft.width()-5, tft.height()-5, TFT_BLACK);
  tft.fillRect(x_px, y_px, 3, 3, color);
}
 
