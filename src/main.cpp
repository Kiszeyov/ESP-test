#include <Arduino.h>

#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <XPT2046_Touchscreen.h>
#include <Wire.h>
#include <SensirionI2cSts3x.h>
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
#define SD_SPEED 40000000U // Max speed for SD card is 80MHz, but it may not work with all SD cards. In case of issues, lower speed to 40000000U (40MHz).

//=======LED CTRL pin=============
#define LED660 1  // LED 1 - 660nm wavelength
#define LED770 3  // LED 2 - 770nm wavelength
#define LED810 12 // LED 3 - 810nm wavelength
#define LED850 13 // LED 4 - 850nm wavelength
#define LED940 26 // LED 5 - 940nm wavelength

// LED control voltages in binary (0-255) for 8-bit DAC
const int L660C{128};
const int L770C{128};
const int L810C{128};
const int L850C{128};
const int L940C{128};

//===========IO pins==============
#define LEDDAC 25
#define DAC2 26
#define ADC1Ph 34
#define ADC2T 35

TFT_eSPI tft = TFT_eSPI();

SensirionI2cSts3x Tsensor;

static char errorMessage[64];
static int16_t error;

const float ADCRes{0.8056640625}; // ADC resolution for 12-bit ADC (3.3V / 4096) in mV
bool isRunning = false;           // controls the start and end of the sensor task
float Ta{0.0};                    // band temperature.
float Tb{0.0};                    // finger temperature.

XPT2046_Touchscreen ts(CS_PIN, TIRQ_PIN);

SPIClass SD_SPI(HSPI);

File data;

TaskHandle_t SensorTaskHandle = NULL;
TaskHandle_t TemperatureUpdateTaskHandle = NULL;

const int headnum = 5;
const String HeadNames[5] = {"LED660", "LED770", "LED810", "LED850", "LED940"};

struct SensorData
{
  float volt;
};

SensorData *SENS660 = nullptr;
SensorData *SENS770 = nullptr;
SensorData *SENS810 = nullptr;
SensorData *SENS850 = nullptr;
SensorData *SENS940 = nullptr;

size_t BufferSize = 100;
size_t SENS660Index = 0;
size_t SENS770Index = 0;
size_t SENS810Index = 0;
size_t SENS850Index = 0;
size_t SENS940Index = 0;

void IOsetup()
{
  pinMode(LED660, OUTPUT);
  pinMode(LED770, OUTPUT);
  pinMode(LED810, OUTPUT);
  pinMode(LED850, OUTPUT);
  pinMode(LED940, OUTPUT);
  pinMode(DAC1, OUTPUT);
  pinMode(DAC2, OUTPUT);
  pinMode(ADC1Ph, INPUT);
  pinMode(ADC2T, INPUT);
}

void SDsetup()
{
  if (!SD.begin(SD_CS, SD_SPI, SD_SPEED))
  {
    Serial.println("SD init fail.");
  }
  data = SD.open("/test.txt", FILE_WRITE);
  if (!data)
  {
    Serial.println("SD write failed");
    return;
  }
  data.println("Hello there!");
  Serial.println("SD write succesful");
  data.close();
}

void TFTsetup()
{
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("TFT up");
  ts.begin();
  ts.setRotation(1);
  Serial.println("TS up");
}

void STS3xSetup()
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
}

bool SDCreateFile(const char *path, bool append = false)
{
  File file = SD.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to create file at:" + String(path));
    return false;
  }
  if (append)
  {
    file.seek(file.size()); // Move to the end of the file for appending
  }
  file.close();
  Serial.println("File created successfully");
  return true;
}

bool SDWriteRawData(const char *path, const String &data)
{
  File file = SD.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing at:" + String(path));
    return false;
  }
  file.println(data);
  file.close();
  Serial.println("Data written successfully");
  return true;
}

String SDReadData(const char *path)
{
  File file = SD.open(path, FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open file for reading at:" + String(path));
    return "";
  }
  String content = file.readString();
  file.close();
  return content;
}

void cvsInit(const char *path, const String &headers, size_t numHeaders)
{
  if (!SDCreateFile(path))
  {
    Serial.println("Failed to create CSV file");
    return;
  }
  String headerLine;
  for (size_t i = 0; i < numHeaders; ++i)
  {
    headerLine += headers + (i < numHeaders - 1 ? "," : "");
  }
  if (!SDWriteRawData(path, headerLine))
  {
    Serial.println("Failed to write CSV headers");
    return;
  }
  Serial.println("CSV file initialized successfully");
}

void LEDCTRL(void *param)
{
  for (;;)
  {
    if (!isRunning)
    {
      vTaskSuspend(SensorTaskHandle); // pre check
    }
    for (int i = BufferSize; i != 0; i--)
    {
      analogWrite(LEDDAC, L660C);
      digitalWrite(LED660, HIGH);
      SENS660[SENS660Index++].volt = analogRead(ADC1Ph) * ADCRes; // Convert ADC reading to voltage in mV
      digitalWrite(LED660, LOW);
      analogWrite(LEDDAC, L770C);
      digitalWrite(LED770, HIGH);
      SENS770[SENS770Index++].volt = analogRead(ADC1Ph) * ADCRes; // Convert ADC reading to voltage in mV
      digitalWrite(LED770, LOW);
      analogWrite(LEDDAC, L810C);
      digitalWrite(LED810, HIGH);
      SENS810[SENS810Index++].volt = analogRead(ADC1Ph) * ADCRes; // Convert ADC reading to voltage in mV
      digitalWrite(LED810, LOW);
      analogWrite(LEDDAC, L850C);
      digitalWrite(LED850, HIGH);
      SENS850[SENS850Index++].volt = analogRead(ADC1Ph) * ADCRes; // Convert ADC reading to voltage in mV
      digitalWrite(LED850, LOW);
      analogWrite(LEDDAC, L940C);
      digitalWrite(LED940, HIGH);
      SENS940[SENS940Index++].volt = analogRead(ADC1Ph) * ADCRes; // Convert ADC reading to voltage in mV
      digitalWrite(LED940, LOW);
    }
    isRunning = false;              // Set the flag to indicate that the sensor task has completed its readings
    vTaskSuspend(SensorTaskHandle); // Task selfterminates when done
  }
}

void Termal(void *param)
{
  for (;;)
  {
    error = Tsensor.measureSingleShot(REPEATABILITY_MEDIUM, false, Ta); // Measure temperature from I2C and store in Ta
    if (error != NO_ERROR)
    {

      Serial.print("Error trying to execute measureSingleShot(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
    }
    Tb = analogRead(ADC2T) * ADCRes * 100.0; // Convert ADC reading to temperature in Celsius // needs tweaking
    vTaskDelay(500 / portTICK_PERIOD_MS);    // Delay for 1 second before the next measurement
  }
}

void setup()
{
  Serial.begin(115200); // Initialize serial communication for debugging
  IOsetup();            // Initialize IO pins
  psramInit();
  SDsetup();    // starts SDcard
  TFTsetup();   // starts TFT and Touchscreen
  STS3xSetup(); // starts STS35 sensor

  if (psramFound())
  {
    Serial.println("PSram up");
  }
  else
  {
    Serial.println("PSram not found");
  }

  delay(100);

  SENS660 = (SensorData *)ps_malloc(BufferSize * sizeof(SensorData));
  SENS770 = (SensorData *)ps_malloc(BufferSize * sizeof(SensorData));
  SENS810 = (SensorData *)ps_malloc(BufferSize * sizeof(SensorData));
  SENS850 = (SensorData *)ps_malloc(BufferSize * sizeof(SensorData));
  SENS940 = (SensorData *)ps_malloc(BufferSize * sizeof(SensorData));

  if (SENS660 == nullptr || SENS770 == nullptr || SENS810 == nullptr || SENS850 == nullptr || SENS940 == nullptr)
  {
    Serial.println("Failure to allocate to PSram");
  }

  // =============================================

  xTaskCreatePinnedToCore(
      LEDCTRL,
      "SensorTask",
      4096,
      NULL,
      0,
      &SensorTaskHandle,
      0);

  xTaskCreate(
      Termal,
      "TemperatureUpdateTask",
      1000,
      NULL,
      1,
      &TemperatureUpdateTaskHandle);
}

void loop()
{
  if (SensorTaskHandle != NULL)
  {
    vTaskResume(SensorTaskHandle); // Resume the sensor task to start reading sensors
    isRunning = true;              // Set the flag to indicate that the sensor task is running
  }
}
