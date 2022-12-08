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
String mqttbroker_Address = "";
String mqttauth_Username = "";
String mqttauth_Password = "";

bool checkbox_useconfig = false;
bool dev_uses_MQTT = false;
bool dev_uses_CoAP = false;
bool mqtt_needs_Auth = false;
bool use_custom_Name = false;
bool checkbox_Gas = false;
bool checkbox_Strom = false;
bool checkbox_Wasser = false;

double current_meter_Reading;


#endif