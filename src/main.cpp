#include <Arduino.h>

#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <freertos/task.h>
#include <Wire.h>
#include <SensirionI2cSts3x.h>
#include <time.h>
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
#define SD_SPEED 80000000U // Max speed for SD card is 80MHz, but it may not work with all SD cards. In case of issues, lower speed to 40000000U (40MHz).

//=======LED CTRL pin=============
#define LED660 32 // LED 1 - 660nm wavelength
#define LED810 33 // LED 3 - 810nm wavelength
#define LED940 2 // LED 5 - 940nm wavelength

// LED control voltages in binary (0-255) for 8-bit DAC
const int L660C{254};
const int L810C{254};
const int L940C{254};

//===========IO pins==============
#define LEDDAC 25
#define DAC2 26
#define ADC1Ph 34
#define ADC2T 35

TFT_eSPI tft = TFT_eSPI();

SensirionI2cSts3x Tsensor;

static char errorMessage[64];
static int16_t error;

const float ADCRes{0.0008056640625}; // ADC resolution for 12-bit ADC (3.3V / 4096) in V
float Ta{0.0};                       // band temperature.
float Tb{0.0};                       // finger temperature.
float SpO2{0.0};                     // blood oxygen saturation level.
float Hemoglobin{0.0};               // hemoglobin concentration.
const float matrix[2][3] = {{-0.0000216869, 0.000597656, 0.000397867}, {0.000330283, -0.0000697138, -0.0000240794}};

XPT2046_Touchscreen ts(CS_PIN, TIRQ_PIN);

File data;

hw_timer_t *Timer0_Cfg = NULL;

struct SensorData
{
  float AC;
  float DC;
};

SensorData *SENS660 = nullptr;
SensorData *SENS810 = nullptr;
SensorData *SENS940 = nullptr;



size_t BufferSize = 100;
size_t Index = 0;



void TFTsetup()
{
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  Serial.println("TFT up");
  ts.begin();
  ts.setRotation(1);
  Serial.println("TS up");
}

/*void STS3xSetup()
{
  Wire.begin();
  Tsensor.begin(Wire, STS35_I2C_ADDR_4A);

  Tsensor.stopMeasurement();
  delay(1);
  Tsensor.softReset();
  delay(100);
  uint16_t aStatusRegister = 0u;
  error = Tsensor.readStatusRegister(aStatusRegister);
  if (error != NO_ERROR)
  {
    Serial.print("Error trying to execute readStatusRegister(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }
  Serial.print("aStatusRegister: ");
  Serial.print(aStatusRegister);
  Serial.println();

  Serial.println("STS3x sensor setup complete");
}*/

/*void DPupdate()
{
  
    

}*/
int LastIndex {0};
void filter(int NewIndex){
  if (LastIndex != NewIndex) {
    LastIndex = NewIndex;
    if (LastIndex > 2 & LastIndex < 99) {
      SENS660[LastIndex-1].DC = ( SENS660[LastIndex-2].DC + SENS660[LastIndex-1].DC + SENS660[LastIndex].DC ) / 3;
      SENS810[LastIndex-1].DC = ( SENS810[LastIndex-2].DC + SENS810[LastIndex-1].DC + SENS810[LastIndex].DC ) / 3;
      SENS940[LastIndex-1].DC = ( SENS940[LastIndex-2].DC + SENS940[LastIndex-1].DC + SENS940[LastIndex].DC ) / 3;
    }
  }
}

void DPupdate( void * pvParameters){
  Serial.println("Task created and started");
  for (;;){
    tft.fillScreen(TFT_BLACK);
    tft.print("band temperature(°C): ");
    tft.println(Ta);
    tft.setCursor(0, 10);
    tft.print("finger temperature(°C): ");
    tft.println(Tb);
    tft.setCursor(0, 19);
    tft.print("SpO2(%): ");
    tft.println(SpO2);
    tft.setCursor(0, 38);
    tft.print("Hemoglobin(g/dl): ");
    tft.println(Hemoglobin);
    tft.setCursor(0, 47);
    tft.println("Tap to end measurement");
    tft.setCursor(0, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);


  }
}
void Tread()
{
  /*error = Tsensor.measureSingleShot(REPEATABILITY_MEDIUM, false, Ta);
  if (error != NO_ERROR)
  {
    Serial.print("Error trying to execute measureSingleShot(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }*/
  Tb = analogRead(ADC2T) * ADCRes; // Convert ADC reading to voltage and then to temperature
}

void test(void * pvParameter)
{
  for (;;){
      digitalWrite(LED660, HIGH);
      analogWrite(LEDDAC, L660C);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      SENS660[Index].AC = analogRead(ADC1Ph);
      SENS660[Index].DC = analogRead(DAC2);
      digitalWrite(LED660, LOW);
      analogWrite(LEDDAC, L810C);
      digitalWrite(LED810, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      SENS810[Index].AC = analogRead(ADC1Ph);
      SENS810[Index].DC = analogRead(DAC2);
      digitalWrite(LED810, LOW);
      analogWrite(LEDDAC, L940C);
      digitalWrite(LED940, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      SENS940[Index].AC = analogRead(ADC1Ph);
      SENS940[Index].DC = analogRead(DAC2);
      Index++;
      digitalWrite(LED940, LOW);
    
  }
}


void HBcalc(float R660, float R810, float R940)
{
float Hemo1;float Hemo2;

Hemo1 = (R660 * matrix[1][1])+(R940 * matrix[1][2])+(R810 * matrix[1][3]);
Hemo2 = (R660 * matrix[2][1])+(R940 * matrix[2][2])+(R810 * matrix[2][3]);

Hemoglobin =  (Hemo1+Hemo2)*1000;

}

void calc()
{
  int Lindex = Index - 6;
  if (Lindex > 0)
  {
    float R660 = abs(SENS660[Lindex].AC / (SENS660[Lindex].DC - 1.6));
    float R810 = abs(SENS810[Lindex].AC / (SENS810[Lindex].DC - 1.6));
    float R940 = abs(SENS940[Lindex].AC / (SENS940[Lindex].DC - 1.6));
    float RSpO2 = R660 / R940;

    SpO2 = 110 - 20 * RSpO2;

    HBcalc(R660, R810, R940);
    if (Index >= 98) {
      Index =0;
    }
  }
  Lindex = Index;
}

TaskHandle_t Task1 = NULL;

void setup()
{
  Serial.begin(115200); // Initialize serial communication for debugging          // Initialize IO pins

 // SDsetup();    // starts SDcard
  TFTsetup();   // starts TFT and Touchscreen
  //STS3xSetup(); // starts STS35 sensor
  delay(100);

  SENS660 = (SensorData *)malloc(BufferSize * sizeof(SensorData));
  SENS810 = (SensorData *)malloc(BufferSize * sizeof(SensorData));
  SENS940 = (SensorData *)malloc(BufferSize * sizeof(SensorData));

  if (SENS660 == nullptr || SENS810 == nullptr || SENS940 == nullptr)
  {
    Serial.println("Failure to allocate to PSram");
  }

  xTaskCreate(
      DPupdate,
      "DPupdate",
      8192,
      NULL,
      1,
    &Task1);

  xTaskCreate(
      test,
      "sensor read",
      16384,
      NULL,
      1,
    &Task1);

  
    pinMode(LED660,OUTPUT);
    digitalWrite(LED660,LOW);
    pinMode(LED810,OUTPUT);
    digitalWrite(LED810,LOW);
    pinMode(LED940,OUTPUT);
    digitalWrite(LED940,LOW);
}


void loop()
{
    Tread();
    filter(Index);
    calc();
}
