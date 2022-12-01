#ifndef WEBPORTAL_HEADER
#define WEBPORTAL_HEADER

//webserver for ota-update
#include <WiFi.h>
#include <WiFiSettings.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>;

AsyncWebServer server(80);

//webportal
bool checkbox_useconfig = false;
String Serveraddress = "";
String Mqttbrokeraddress = "";
String Mqtt_auth_username = "";
String Mqtt_auth_password = "";
String Serveraddress_m = "";
String Serveraddress_SML = "/sml";
String Serveraddress_META = "/metadata";
String Apikey = "";
String Authorization = "";
String Bearer = "Bearer ";
bool checkbox_server = true;
bool checkbox_apikey = false;
bool checkbox_authorization = false;
bool checkbox_meta = false;
bool checkbox_use_mqtt = false;
bool checkbox_mqtt_auth = false;

#endif