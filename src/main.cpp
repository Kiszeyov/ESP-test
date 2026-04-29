#include <Arduino.h>

#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <XPT2046_Touchscreen.h>

//=========LCD connections========
/*========DO NOT UNCOMMENT========
Guideline only
// For ESP32 Dev board (only tested with ILI9341 display)
// The hardware SPI can be mapped to any pins

#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 5  // Chip select control pin
#define TFT_DC 27 // Data Command control pin
#define TFT_RST 4 // Reset pin (could connect to RST pin)
#define TFT_RST 33  // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST*/

//========Touch connections=======
/*========DO NOT UNCOMMENT========
#define Touch_MISO 19 //T_DO
#define Touch_MOSI 23 //T_DIN
#define Touch_SCLK 18 //T_CLK*/
#define CS_PIN 4    // T_CS
#define TIRQ_PIN 36 // T_IRQ

//========SD connections==========
#define SD_CS 15
#define SD_CLK 14
#define SD_MOSI 12
#define SD_MISO 13

//========SD speed================
#define SD_SPEED 80000000U // Max speed for SD card is 80MHz, but it may not work with all SD cards. In case of issues, lower speed to 4000000U (4MHz).

//=======LED CTRL pin=============
#define LED660 1  // LED 1 - 660nm wavelength
#define LED770 3  // LED 2 - 770nm wavelength
#define LED810 12 // LED 3 - 810nm wavelength
#define LED850 13 // LED 4 - 850nm wavelength
#define LED940 26 // LED 5 - 940nm wavelength

//===========IO pins==============
#define DAC1 25
#define DAC2 26
#define ADC1Ph 34
#define ADC2T 35

TFT_eSPI tft = TFT_eSPI();

float ADCRes {0.0008056640625};

XPT2046_Touchscreen ts(CS_PIN, TIRQ_PIN);

SPIClass SD_SPI(HSPI);

File data;

struct SensorData {
  uint16_t Raw;
  uint8_t Led;
  uint16_t timestamp;
  float volt;
};

SensorData *Sensorbuffer = nullptr;
size_t BufferSize = 0;
size_t BufferIndex = 0;

void SDsetup() {
  if (!SD.begin(SD_CS, SD_SPI, SD_SPEED)) {
    Serial.println("SD init fail.");
  }
  data = SD.open("/test.txt", FILE_WRITE);
  if (!data){
    Serial.println("SD write failed");
    return;
  }
  data.println("Hello there!");
  Serial.println("SD write succesful");
  data.close();
}

void TFTsetup() {
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("TFT up");
  ts.begin();
  ts.setRotation(1);
  Serial.println("TS up");
}


void setup() {
  Serial.begin(115200);

  psramInit();

  SDsetup();
  TFTsetup();


  if(psramFound()) {
    Serial.println("PSram up");
  }
  else {
    Serial.println("PSram not found");
  }

  delay(1000);

  BufferSize = 100;

  Sensorbuffer = (SensorData *)ps_malloc(BufferSize * sizeof(SensorData));

  if (Sensorbuffer == nullptr) {
    Serial.println("Faliure to allocate to PSram");

  }

}

void loop() {
  Serial.println("Go Fuck Yourself!");
}






















