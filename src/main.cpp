#include <Arduino.h>
#include "webportal.h"
#include "timestamp.h"
#include "deviceinfo.h"
#include "filefunctions.h"
#include "PubSubClient.h"
#include <ArduinoJson.h>
#include <M5StickCPlus.h>

unsigned long FADE_PEDIOD = 5000; // fade time is 3 seconds

unsigned long fadeUPStartTime;
unsigned long fadeDownStartTime;
long brightness;

int connection_counter = 0;
int red_LED = 10;

void configure_LED(void)
{
  pinMode(red_LED, OUTPUT);
  digitalWrite(red_LED, HIGH);
  fadeUPStartTime = millis();
  fadeDownStartTime = millis();
}

void fade_LED_up(void)
{
  unsigned long progress = millis() - fadeUPStartTime;

  if (progress <= FADE_PEDIOD)
  {
    brightness = 255 - map(progress, 0, FADE_PEDIOD, 0, 255);
    analogWrite(red_LED, brightness);
  }
  else
  {
    fadeUPStartTime = millis();
  }
}

void connected_AP(void)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    // Serial.println("Not connected to AP, start soft AP and fade LED");
    fade_LED_up();
    // fade_LED_down();
  }
}

void setup()
{
  M5.begin();

  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setRotation(1);

  configure_LED();
  // start SPIFFS
  SPIFFS.begin(true);

  // display device info on config page
  WiFiSettings.hostname = device_ID;
  WiFiSettings.heading("Sensor ID:");
  WiFiSettings.heading(device_ID);

  // reset connection counter
  connection_counter = 0;

  // try to connect to WiFi with a timeout of 30 seconds - launch portal if connection fails
  if (WiFiSettings.connect(true, 30))
  {
    M5.Lcd.fillScreen(WHITE);
    delay(500);
    M5.Lcd.fillScreen(RED);
    delay(500);
    M5.Lcd.fillScreen(GREEN);
    delay(500);
    M5.Lcd.fillScreen(BLUE);
    delay(500);
    M5.Lcd.fillScreen(BLACK);
    delay(500);
    M5.Lcd.setCursor(0, 55);
    String IP_String = WiFi.localIP().toString();
    M5.Lcd.printf("%s", IP_String.c_str());
  }
  else
  {
    digitalWrite(red_LED, LOW);
  }

  // get time
  configTime(0, 0, ntpServer);

  // start update-functionality
  AsyncElegantOTA.begin(&server);
  server.begin();
}

void loop()
{
  // put your main code here, to run repeatedly:
  // run update server
  connected_AP();
  AsyncElegantOTA.loop();
}