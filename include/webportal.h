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
bool checkbox_useconfig = false;
String device_Name = "";
String Serveraddress = "";
String Mqttbrokeraddress = "";
String Mqtt_auth_username = "";
String Mqtt_auth_password = "";

bool use_custom_Name = false;
bool checkbox_Gas = false;
bool checkbox_Strom = false;
bool checkbox_Wasser = false;


#endif