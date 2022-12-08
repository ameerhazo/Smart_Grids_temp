#include <Arduino.h>
#include "webportal.h"
#include "timestamp.h"
#include "deviceinfo.h"
#include "filefunctions.h"
#include "PubSubClient.h"
#include <ArduinoJson.h>
#include "driver/pcnt.h"

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
volatile int last_state = HIGH; // To make sure variables shared between an ISR

void increment_impulse(void)
{
  // IS
  if(last_state == HIGH)
  {
    impulse_counter++;
  }
  
}

void configure_impulse_pin(void)
{
  pinMode(impulse_pin, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(impulse_pin), increment_impulse, FALLING);
}

void setup()
{

  Serial.begin(115200);
  configure_impulse_pin();
}

void loop()
{
  // put your main code here, to run repeatedly:
  // run update server

  // Serial.printf("counter = %d\n", impulse_counter);
  Serial.println(digitalRead(impulse_pin));
  // delay(1000);
}