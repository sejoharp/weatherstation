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

//Setup
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 20 minutes
unsigned long delayTime;
const char *INFLUXDB_URL = "http://raspberrypi.fritz.box:8086";
const char *INFLUXDB_DB_NAME = "homeautomation";

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
void setupDone(OLEDDisplay *display);
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

  setupDone(&display);
  delay(1000);
}

void loop()
{
  Serial.println("");
  drawBME(&display);
  int updateInterval = 1000L * 55;
  delay(updateInterval);
}

void initBme(OLEDDisplay *display)
{
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, "init bme280...");
  display->display();
  display->flipScreenVertically();
}

void setupDone(OLEDDisplay *display)
{
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, "Done...");
  display->display();
  display->flipScreenVertically();
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
  Serial.println("BME280 test");
  Serial.println("");

  int counter = 0;
  while (bme.begin(0x76) == false)
  {
    Serial.println("Could not find a valid dBME280 sensor,check wiring!");

    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 10, "initializing bme280");
    display.drawXbm(40, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(54, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(68, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    display.flipScreenVertically();
    counter++;
    delay(1000);
  }
  Serial.println("--DefaultTest--");
}

void initWifiAutoConnect()
{
  WiFiManager wifiManager;

  displayText("Connecting to WiFi webserver:192.168.4.1");

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
  Serial.println("temperature: " + String(temperature));
  Serial.println("pressure: " + String(pressure));
  Serial.println("humidity: " + String(humanity));
  Serial.println("time: " + time);
  Serial.println("-----------------------");

  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  String humi = "Hum:" + String(humanity) + "%";
  display->drawString(0, 0, humi);

  String temp = "Temp:" + String(temperature) + "°C";
  display->drawString(0, 15, temp);

  String pres = "Pres:" + String(pressure) + "hPa";
  display->drawString(0, 30, pres);

  display->drawHorizontalLine(0, 48, 128);

  display->drawString(0, 49, "Time:" + time);

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