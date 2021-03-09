#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h> //time
#include <time.h>            //time()ctime()

#include <sys/time.h> //structtimeval
#include "SH1106Wire.h"
#include "OLEDDisplayUi.h"
#include <InfluxDbClient.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

//CreatetheLightsensorinstance
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; //I2C
//DHTespdht;
/****************************BeginSettings**************************/

#define TZ 0      //(utc+)TZinhours
#define DST_MN 60 //use 60mn for summer time in some countries

//Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;

const int SDA_PIN = D2;
const int SDC_PIN = D1;

const uint8_t activeSymbole[] PROGMEM = {
    B00000000,
    B00000000,
    B00011000,
    B00100100,
    B01000010,
    B01000010,
    B00100100,
    B00011000};

const uint8_t inactiveSymbole[] PROGMEM = {
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00011000,
    B00011000,
    B00000000,
    B00000000};
/****************************EndSettings**************************/
//Initialize the oled display for address 0x3c
//sda-pin=14 and sdc-pin=12
//SSD1306Wire display(I2C_DISPLAY_ADDRESS,SDA_PIN,SDC_PIN);

SH1106Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi ui(&display);

#define TZ_MN ((TZ)*60)
#define TZ_SEC ((TZ)*3600)
#define DST_SEC ((DST_MN)*60)

void drawBME(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawBME(OLEDDisplay *display);
void initWifiAutoConnect();
void initBme280();
void uploadMetric(float temperature, float humanity, float pressure);
String currentTime();
void displayText(String text);

void setup()
{
  Serial.begin(115200);
  Serial.println();

  display.init();

  display.clear();
  display.display();
  display.flipScreenVertically();
  display.setContrast(255);

  initBme280();

  initWifiAutoConnect();

  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  displayText("setup done!");
}

void loop()
{
  Serial.println("");
  drawBME(&display);
  int updateInterval = 1000L * 55;
  delay(updateInterval);
}

void displayText(String text)
{
  Serial.println(text);
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 5, 128, text);
  display.display();
  display.flipScreenVertically();
  delay(1000);
}

void initBme280()
{
  displayText("BME280 test");

  while (bme.begin(0x76) == false)
  {
    displayText("Could not find a valid dBME280 sensor,check wiring!");
    displayText("initializing bme280..");
  }
}

void initWifiAutoConnect()
{
  WiFiManager wifiManager;

  displayText("Connecting to WiFi.. wifi config: http://192.168.4.1");

  // wifiManager.resetSettings();
  wifiManager.autoConnect("weatherstation", "chaos radio express");

  displayText("Connected to WiFi");
}

void drawBME(OLEDDisplay *display)
{
  float temperature = bme.readTemperature();
  float pressure = bme.readPressure() / 100.0F;
  float humanity = bme.readHumidity();

  uploadMetric(temperature, humanity, pressure);

  String time = currentTime();

  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  String humanityString = "Hum:" + String(humanity) + "%";
  display->drawString(0, 0, humanityString);
  Serial.println(humanityString);

  String temperatureString = "Temp:" + String(temperature) + "°C";
  display->drawString(0, 15, temperatureString);
  Serial.println(temperatureString);

  String pressureString = "Pres:" + String(pressure) + "hPa";
  display->drawString(0, 30, pressureString);
  Serial.println(pressureString);

  display->drawHorizontalLine(0, 48, 128);

  display->drawString(0, 49, "Time:" + time);
  Serial.println("time: " + time);
  Serial.println("-----------------------");

  display->display();
  display->flipScreenVertically();
}

String currentTime()
{
  time_t now = time(nullptr);
  struct tm *timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  return String(buff);
}

void uploadMetric(float temperature, float humanity, float pressure)
{
  const char *INFLUXDB_URL = "http://raspberrypi.fritz.box:8086";
  const char *INFLUXDB_DB_NAME = "homeautomation";
  InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);

  // Define data point with measurement name 'device_status`
  Point pointDevice("workroom_climate");
  // Set tags
  pointDevice.addTag("device", "weatherstation");
  pointDevice.addTag("room", "workroom");
  // Add data
  pointDevice.addField("temperature", temperature);
  pointDevice.addField("humanity", humanity);
  pointDevice.addField("pressure", pressure);

  // Write data
  client.setInsecure(true);
  bool success = client.writePoint(pointDevice);
  String status = success ? "true" : "false";
  Serial.println("upload metric successful: " + status);
}