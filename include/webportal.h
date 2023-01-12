#ifndef WEBPORTAL_HEADER
#define WEBPORTAL_HEADER

//webserver for ota-update
#include <WiFi.h>
#include <WiFiSettings.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

AsyncWebServer server(80);

//webportal
String device_Name = "";
String Serveraddress = "";
String mqttbroker_Address = "h2893034.stratoserver.net";
String mqttauth_Username = "user-ikt";
String mqttauth_Password = "G%3bJ@wTt";

bool checkbox_useconfig = false;
bool dev_uses_MQTT = true;
bool dev_uses_CoAP = false;
bool mqtt_needs_Auth = true;
bool use_custom_Name = false;
bool checkbox_Gas = false;
bool checkbox_Strom = false;
bool checkbox_Wasser = true;
bool first_impulse = true;

double current_meter_Reading = 1234.003;


#endif