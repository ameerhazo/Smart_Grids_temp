#include <Arduino.h>
#include "webportal.h"
#include "timestamp.h"
#include "deviceinfo.h"
#include "filefunctions.h"
#include "PubSubClient.h"
#include <ArduinoJson.h>
#include "driver/pcnt.h"
#include <M5StickCPlus.h>

#define CONV_FACTOR 0.001
#define M5_LOW 1
#define M5_HIGH 0

WiFiClient Client;

PubSubClient MQTT_Client;

// MQTT sml topic string
String mainTopic = "things/";
// MQTT SUITE topic string
String metadata_topic = String("things/" + String(device_ID));
// MQTT SUITE lwt topic string
String lwt_Topic = String(metadata_topic + "/lwt");
// TD_topic
String TD_topic;

/*                   MQTT property topic example                    */
// things/EHz_Server_ID/properties/property_name (SML data name)
//

// MQTT last will testament message
String lwt_message = String("{ \"status\": \"offline\"}");

// boolean to check for connected to broker
bool connected_tobroker = false;

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

void reconnect();

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
  M5.Lcd.printf("SMART GRIDS");
  delay(2000);

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
    delay(500);
  };

  bool use_custom_name_Dummy = false;
  String custom_device_ID_Dummy;

  if (device_Name == "")
  {
    // use default name
    WiFiSettings.hostname = device_ID;
    WiFiSettings.heading("Sensor ID:");
    WiFiSettings.heading(device_ID);
    bool use_custom_name_dummy = WiFiSettings.checkbox("Use custom device name", false);
    custom_device_ID_Dummy = WiFiSettings.string("Custom device name", "device_name001");
  }
  else
  {
    WiFiSettings.hostname = device_Name;
    WiFiSettings.heading("Sensor ID:");
    WiFiSettings.heading(device_Name);
    use_custom_name_Dummy = WiFiSettings.checkbox("Use custom device name", true);
    String device_ID_dummy = WiFiSettings.string("Custom device name", "device_name001");
  }

  WiFiSettings.heading("CoAP Configuration");
  bool dev_uses_CoAP_dummy = WiFiSettings.checkbox("Send data via CoAP");

  WiFiSettings.heading("MQTT Configuration");
  bool dev_uses_MQTT_dummy = WiFiSettings.checkbox("Send data via MQTT");
  String mqtt_brokeraddress_dummy = WiFiSettings.string("MQTT broker address", "mqtt.eclipse.org");
  bool checkbox_mqtt_auth_dummy = WiFiSettings.checkbox("MQTT broker authentification", false);
  String mqtt_brokerauth_user = WiFiSettings.string("MQTT Broker Username", "username");
  String mqtt_brokerauth_pass = WiFiSettings.string("MQTT Broker Password", "password");

  // WiFiSettings.heading("Device mode");
  // bool checkbox_Gas_dummy = WiFiSettings.checkbox("Gas", false);
  // bool checkbox_Strom_dummy = WiFiSettings.checkbox("Strom", false);
  // bool checkbox_Wasser_dummy = WiFiSettings.checkbox("Wasser", false);

  WiFiSettings.heading("Gas Meter Configuration");
  String meter_Number = WiFiSettings.string("Meter number", "Gas_Meter-001");
  String meter_Reading = WiFiSettings.string("Meter reading", "12345.000");
  // int meter_Reading = WiFiSettings.integer("Meter reading", );

  // reset connection counter
  connection_counter = 0;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 55);
  M5.Lcd.printf("CONNECTING...");

  // try to connect to WiFi with a timeout of 30 seconds - launch portal if connection fails
  WiFiSettings.connect(true, 30);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 55);
  M5.Lcd.println("CONNECTED");
  delay(1000);

  //
  use_custom_Name = use_custom_name_Dummy;
  mqttbroker_Address = mqtt_brokeraddress_dummy;
  mqtt_needs_Auth = checkbox_mqtt_auth_dummy;
  mqttauth_Username = mqtt_brokerauth_user;
  mqttauth_Password = mqtt_brokerauth_pass;
  current_meter_Reading = meter_Reading.toDouble();

  if (use_custom_Name)
  {
    if (custom_device_ID_Dummy != "")
    {
      device_Name = custom_device_ID_Dummy;
      WiFiSettings.hostname = device_Name;
    }
  }
  else
  {
    device_Name = "";
  }

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.println("TIME SYNC");
  delay(1000);

  configTime(3600, 0, ntpServer);

  M5.Lcd.setCursor(0, 55);
  M5.Lcd.println("SUCCESS");
  delay(1000);

  // checkbox_Gas = checkbox_Gas_dummy;
  // checkbox_Strom = checkbox_Strom_dummy;
  // checkbox_Wasser = checkbox_Wasser_dummy;

  if (mqttbroker_Address != "")
  {
  }
  else
  {
    Serial.println("User forgot to set MQTT broker address, restarting portal");
    WiFiSettings.portal();
  }

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

  // start update-functionality
  AsyncElegantOTA.begin(&server);
  server.begin();
}

void loop()
{
  // put your main code here, to run repeatedly:
  // run update server
  // try to reconnect to the MQTT broker
  reconnect();

  // perform background tasks for the communication using MQTT
  MQTT_Client.loop();

  M5.update();
  test_button_press();
  connected_AP();
  AsyncElegantOTA.loop();
  // Serial.printf("counter = %d\n", impulse_counter);
  // Serial.println(digitalRead(impulse_pin));
  // delay(50);
}

void reconnect()
{
  Serial.print("Attempting MQTT connection...");

  // Loop until we're reconnected
  while (!MQTT_Client.connected())
  {
    int retries = 0;

    if (retries < 5)
    {
      
      if (mqtt_needs_Auth)
      {
        connected_tobroker = MQTT_Client.connect(, mqttauth_Username.c_str(), mqttauth_Password.c_str(), lwt_Topic.c_str(), 1, true, lwt_message.c_str());
      }
      else
      {
        connected_tobroker = MQTT_Client.connect(device_ID, lwt_Topic.c_str(), 1, true, lwt_message.c_str());
      }

      Serial.println("Retries exceeded, restarting device");
      ESP.restart();
    }
  }
}