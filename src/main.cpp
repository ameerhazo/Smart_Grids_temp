#include <Arduino.h>
#include "webportal.h"
#include "timestamp.h"
#include "deviceinfo.h"
#include "filefunctions.h"
#include "PubSubClient.h"
#include <ArduinoJson.h>
#include <M5StickCPlus.h>

int connection_counter = 0;

void setup()
{
  M5.begin();
  // open serial connection (console output)
  Serial.begin(115200);
  // start SPIFFS
  SPIFFS.begin(true);

  // display device info on config page
  WiFiSettings.hostname = device_ID;
  WiFiSettings.heading("Sensor ID:");
  WiFiSettings.heading(device_ID);

  // reset connection counter
  connection_counter = 0;

  // try to connect to WiFi with a timeout of 30 seconds - launch portal if connection fails
  if(WiFiSettings.connect(true, 30))
  {
    Lcd.printf("");
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
  AsyncElegantOTA.loop();
}