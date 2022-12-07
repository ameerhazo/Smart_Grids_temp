#include <Arduino.h>
#include "webportal.h"
#include "timestamp.h"
#include "deviceinfo.h"
#include "filefunctions.h"
#include "PubSubClient.h"
#include <ArduinoJson.h>
#include "driver/pcnt.h"
#include <M5StickCPlus.h>

#define M5_LOW 1
#define M5_HIGH 0

unsigned long FADE_PEDIOD = 5000; // fade time is 3 seconds

unsigned long fadeUPStartTime;
unsigned long fadeDownStartTime;
long brightness;

bool buttonActive = false;
bool longPressActive = false;

int connection_counter = 0;
int red_LED = 10;

int impulse_pin = 13; // define interrupt pin to 2
int impulse_counter = 0;
volatile int state = LOW; // To make sure variables shared between an ISR

void test_button_press()
{
  if (M5.BtnA.pressedFor(3000))
  {
    WiFiSettings.portal();
  }
  // if (digitalRead(red_Button) == LOW)
  // {

  //   if (buttonActive == false)
  //   {

  //     buttonActive = true;
  //     buttonTimer = millis();
  //   }

  //   if ((millis() - buttonTimer > longPressTime) && (longPressActive == false))
  //   {
  //     if (paused)
  //     {
  //       longPressActive = true;
  //       Serial.println("resume pressed!");
  //       resume_pressed = true;
  //       beep(2);
  //     }
  //   }
  // }
  // else
  // {

  //   if (buttonActive == true)
  //   {

  //     if (longPressActive == true)
  //     {

  //       longPressActive = false;
  //     }
  //     else
  //     {

  //       Serial.println("short pressed!");
  //       pause_pressed = true;
  //       beep(1);
  //     }

  //     buttonActive = false;
  //   }
  // }
}

void configure_LED(void)
{
  pinMode(red_LED, OUTPUT);
  digitalWrite(red_LED, HIGH);
  fadeUPStartTime = millis();
  fadeDownStartTime = millis();
}

void increment_impulse(void)
{
  // ISR
  impulse_counter++;
}

void configure_impulse_pin(void)
{
  pinMode(impulse_pin, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(impulse_pin), increment_impulse, FALLING);
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

void on_Connect(void)
{
}

void on_Failure(void)
{
  M5.Lcd.fillScreen(BLACK);
  delay(500);
  M5.Lcd.setCursor(0, 55);
  M5.Lcd.printf("FAILED");
  digitalWrite(red_LED, M5_HIGH);
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

  // configure_impulse_pin();

  WiFiSettings.onWaitLoop = []()
  {
    bool led_Status = digitalRead(red_LED);
    digitalWrite(red_LED, !led_Status);
    return 500;
  };

  WiFiSettings.onFailure = []()
  {
    on_Failure();
  };

  WiFiSettings.onPortal = []()
  {
    M5.Lcd.fillScreen(BLACK);
    delay(500);
    M5.Lcd.setCursor(0, 55);
    M5.Lcd.printf("PORTAL CONFIG");
  };

  WiFiSettings.onRestart = []()
  {
    M5.Lcd.fillScreen(BLACK);
    delay(500);
    M5.Lcd.setCursor(0, 55);
    M5.Lcd.printf("RESTARTING...");
  };
  // WiFiSettings.onWaitLoop = []() {}; // delay 30 ms
  // WiFiSettings.onPortalWaitLoop = []() {};

  // display device info on config page
  WiFiSettings.hostname = device_ID;
  WiFiSettings.heading("Sensor ID:");
  WiFiSettings.heading(device_ID);

  // reset connection counter
  connection_counter = 0;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 55);
  M5.Lcd.printf("CONNECTING...");
  // try to connect to WiFi with a timeout of 30 seconds - launch portal if connection fails
  WiFiSettings.connect(true, 30);

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
  M5.update();
  test_button_press();
  connected_AP();
  AsyncElegantOTA.loop();
  // Serial.printf("counter = %d\n", impulse_counter);
  // Serial.println(digitalRead(impulse_pin));
  // delay(50);
}