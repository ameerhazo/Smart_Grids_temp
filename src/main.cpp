#include <Arduino.h>
#include "webportal.h"
#include "timestamp.h"
#include "deviceinfo.h"
#include "filefunctions.h"
// #include "PubSubClient.h"
#include <ArduinoJson.h>
#include "driver/pcnt.h"
#include <M5StickCPlus.h>
#include <WebThingAdapter.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/**
 * Impulse counter constants
 *
 * Use PCNT module to count rising edges generated by LEDC module.
 *
 * Functionality of GPIOs used in this example:
 *   - GPIO18 - output pin of a sample 1 Hz pulse generator,
 *   - GPIO4 - pulse input pin,
 *   - GPIO5 - control input pin.
 *
 * Load example, open a serial port to view the message printed on your screen.
 *
 * To do this test, you should connect GPIO18 with GPIO4.
 * GPIO5 is the control signal, you can leave it floating with internal pull up,
 * or connect it to ground. If left floating, the count value will be increasing.
 * If you connect GPIO5 to GND, the count value will be decreasing.
 *
 * An interrupt will be triggered when the counter value:
 *   - reaches 'thresh1' or 'thresh0' value,
 *   - reaches 'l_lim' value or 'h_lim' value,
 *   - will be reset to zero.
 */

#define PCNT_INPUT_SIG_IO 26 // Pulse Input GPIO
#define PCNT_INPUT_CTRL_IO 5 // Control GPIO HIGH=count up, LOW=count down
#define LEDC_OUTPUT_IO 0     // Output GPIO of a sample 1 Hz pulse generator

// Temperature sensor constants
#define CONV_FACTOR 0.001
#define M5_LOW 1
#define M5_HIGH 0
#define TEMPERATURE_PRECISION 9

const int oneWireBus = 26;
OneWire oneWire(oneWireBus); // on pin 10 (a 4.7K resistor is necessary)
DallasTemperature sensors(&oneWire);

DeviceAddress insideThermometer, outsideThermometer;

void OneWireTask(void *pvParameters);

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

// void reconnect();

ThingProperty *prop_Gas;
ThingProperty *prop_Strom;
ThingProperty *prop_Wasser;
ThingDevice *multisensor;

xQueueHandle pcnt_evt_queue; // A queue to handle pulse counter events

/* A sample structure to pass events from the PCNT
 * interrupt handler to the main program.
 */
typedef struct
{
  int unit;        // the PCNT unit that originated an interrupt
  uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;

/* Decode what PCNT's unit originated an interrupt
 * and pass this information together with the event type
 * the main program using a queue.
 */
static void IRAM_ATTR pcnt_example_intr_handler(void *arg)
{
  int pcnt_unit = (int)arg;
  pcnt_evt_t evt;
  evt.unit = pcnt_unit;
  /* Save the PCNT event type that caused an interrupt
     to pass it to the main program */
  pcnt_get_event_status(PCNT_UNIT_0, &evt.status);
  xQueueSendFromISR(pcnt_evt_queue, &evt, NULL);
}

void test_button_press()
{
  if (M5.BtnA.pressedFor(3000))
  {
    WiFiSettings.portal();
  }
}

void configure_LED(void)
{
  pinMode(red_LED, OUTPUT);
  digitalWrite(red_LED, HIGH);
  fadeUPStartTime = millis();
  fadeDownStartTime = millis();
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C)
  {
    Serial.println("Error: Could not read temperature data");
    return;
  }
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC));
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress)
{
  Serial.print("Device Address: ");
  printAddress(deviceAddress);
  Serial.print(" ");
  printTemperature(deviceAddress);
  Serial.println();
}

void configure_temperature_sensors(void)
{
  // initialize DSB sensors here
  sensors.begin();
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  if (!sensors.getAddress(insideThermometer, 0))
    Serial.println("Unable to find address for Device 0");
  if (!sensors.getAddress(outsideThermometer, 1))
    Serial.println("Unable to find address for Device 1");

  Serial.print("Parasite power is: ");
  if (sensors.isParasitePowerMode())
    Serial.println("ON");
  else
    Serial.println("OFF");

  if (!sensors.getAddress(insideThermometer, 0))
    Serial.println("Unable to find address for Device 0");
  if (!sensors.getAddress(outsideThermometer, 1))
    Serial.println("Unable to find address for Device 1");

  Serial.print("Device 0 Address: ");
  printAddress(insideThermometer);
  Serial.println();

  Serial.print("Device 1 Address: ");
  printAddress(outsideThermometer);
  Serial.println();

  // set the resolution to 9 bit per device
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);

  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC);
  Serial.println();

  Serial.print("Device 1 Resolution: ");
  Serial.print(sensors.getResolution(outsideThermometer), DEC);
  Serial.println();
}

void configure_impulse_pin(void)
{
  /* Initialize PCNT event queue and PCNT functions */
  pcnt_evt_queue = xQueueCreate(10, sizeof(pcnt_evt_t));

  /* Prepare configuration for the PCNT unit */
  pcnt_config_t pcnt_config = {
      // Set PCNT input signal and control GPIOs
      .pulse_gpio_num = PCNT_INPUT_SIG_IO,
      .ctrl_gpio_num = PCNT_INPUT_CTRL_IO,
      // What to do when control input is low or high?
      .lctrl_mode = PCNT_MODE_REVERSE, // Reverse counting direction if low
      .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
      // What to do on the positive / negative edge of pulse input?
      .pos_mode = PCNT_COUNT_DIS, // Count up on the positive edge
      .neg_mode = PCNT_COUNT_INC, // Keep the counter value on the negative edge
      // Set the maximum and minimum limit values to watch
      // .counter_h_lim = PCNT_H_LIM_VAL,
      // .counter_l_lim = PCNT_L_LIM_VAL,
      .unit = PCNT_UNIT_0,
      .channel = PCNT_CHANNEL_0};

  /* Initialize PCNT unit */
  pcnt_unit_config(&pcnt_config);

  /* Configure and enable the input filter */
  pcnt_set_filter_value(PCNT_UNIT_0, 100);
  pcnt_filter_enable(PCNT_UNIT_0);

  /* Set threshold 0 and 1 values and enable events to watch */
  // pcnt_set_event_value(PCNT_UNIT_0, PCNT_EVT_THRES_1, PCNT_THRESH1_VAL);
  // pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_THRES_1);
  // pcnt_set_event_value(PCNT_UNIT_0, PCNT_EVT_THRES_0, PCNT_THRESH0_VAL);
  // pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_THRES_0);
  /* Enable events on zero, maximum and minimum limit values */
  // pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_ZERO);
  // pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_H_LIM);
  // pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_L_LIM);

  /* Initialize PCNT's counter */
  pcnt_counter_pause(PCNT_UNIT_0);
  pcnt_counter_clear(PCNT_UNIT_0);

  /* Install interrupt service and add isr callback handler */

  int pcnt_unit = PCNT_UNIT_0;
  pcnt_isr_service_install(0);
  pcnt_isr_handler_add(PCNT_UNIT_0, pcnt_example_intr_handler, (void *)pcnt_unit);

  /* Everything is set up, now go to counting */
  pcnt_counter_resume(PCNT_UNIT_0);
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



void update_Meter(void)
{

  // if (checkbox_Wärme)
  // {
  //   int16_t count = 0;
  //   pcnt_evt_t event;
  //   portBASE_TYPE res = xQueueReceive(pcnt_evt_queue, &event, pdMS_TO_TICKS(1000));

  //   if (res == pdTRUE) // counted up
  //   {
  //     pcnt_get_counter_value(PCNT_UNIT_0, &count);

  //     // calculate something

  //     // TODO: check how this is
  //     current_meter_Reading = current_meter_Reading + 1;

  //     ThingPropertyValue temp_new_value;
  //     temp_new_value.number = current_meter_Reading;
  //     prop_Wasser->setValue(temp_new_value);
  //     prop_Wasser->hasChanged();
  //   }
  // }

  if (checkbox_Gas) // Wärme
  {
    int16_t count = 0;
    pcnt_evt_t event;
    portBASE_TYPE res = xQueueReceive(pcnt_evt_queue, &event, pdMS_TO_TICKS(1000));

    if (res == pdTRUE) // counted up
    {
      pcnt_get_counter_value(PCNT_UNIT_0, &count);

      // calculate something

      // TODO: check how this is
      current_meter_Reading = current_meter_Reading + 1.0;

      ThingPropertyValue temp_new_value;
      temp_new_value.number = current_meter_Reading;
      prop_Gas->setValue(temp_new_value);
      prop_Gas->hasChanged();
    }
  }

  if (checkbox_Strom) // add SML here maybe?
  {
  }

  if (checkbox_Wasser)
  {
    int16_t count = 0;
    pcnt_evt_t event;
    portBASE_TYPE res = xQueueReceive(pcnt_evt_queue, &event, pdMS_TO_TICKS(1000));

    if (res == pdTRUE) // counted up
    {
      pcnt_get_counter_value(PCNT_UNIT_0, &count);

      // calculate something

      // TODO: check how this is
      current_meter_Reading = current_meter_Reading + 1.0;

      ThingPropertyValue temp_new_value;
      temp_new_value.number = current_meter_Reading;
      prop_Wasser->setValue(temp_new_value);
      prop_Wasser->hasChanged();
    }
    // else // for debug
    // {
    //   pcnt_get_counter_value(PCNT_UNIT_0, &count);
    // }
  }
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

  WiFiSettings.heading("Device mode");
  bool checkbox_Gas_dummy = WiFiSettings.checkbox("Gas", false);
  bool checkbox_Strom_dummy = WiFiSettings.checkbox("Electricity", false);
  bool checkbox_Wasser_dummy = WiFiSettings.checkbox("Water", false);
  String meter_Number = WiFiSettings.string("Meter ID", "Meter-001");

  WiFiSettings.heading("Meter Configuration");
  // String meter_Number = WiFiSettings.string("Meter number", "Gas_Meter-001");
  String meter_Reading = WiFiSettings.string("Meter reading", "12345.000");

  // WiFiSettings.heading("Meter Configuration");
  // // String meter_Number = WiFiSettings.string("Meter number", "Electricity_Meter-001");
  // String meter_Reading = WiFiSettings.string("Meter reading", "12345.000");

  // WiFiSettings.heading("Configuration");
  // // String meter_Number = WiFiSettings.string("Meter number", "Gas_Meter-001");
  // String meter_Reading = WiFiSettings.string("Meter reading", "12345.000");

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

  // finished WiFi connection process
  // set variables for configuration
  use_custom_Name = use_custom_name_Dummy;
  device_Name = custom_device_ID_Dummy;
  mqttbroker_Address = mqtt_brokeraddress_dummy;
  mqtt_needs_Auth = checkbox_mqtt_auth_dummy;
  mqttauth_Username = mqtt_brokerauth_user;
  mqttauth_Password = mqtt_brokerauth_pass;
  checkbox_Gas = checkbox_Gas_dummy;
  checkbox_Strom = checkbox_Strom_dummy;
  checkbox_Wasser = checkbox_Wasser_dummy;
  current_meter_Reading = meter_Reading.toDouble();

  Serial.println(current_meter_Reading);

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
  // String IP_String = WiFi.localIP().toString();
  // M5.Lcd.printf("%s", IP_String.c_str());
  M5.Lcd.printf("RUNNING");

  const char *multisensorProperties[] = {"Smart_Meter", nullptr};

  if (use_custom_Name)
  {
    multisensor = new ThingDevice(device_Name.c_str(), "SmartMeter_Thing", multisensorProperties);
  }
  else
  {
    multisensor = new ThingDevice(device_ID, "SmartMeter_Thing", multisensorProperties);
  }

  // if (checkbox_Waerme)
  // {
  //   configure_impulse_pin();
  //  prop_Gas = new ThingProperty("Gas", "Gas usage measurement", NUMBER, nullptr, nullptr, nullptr);
  //  multisensor->addProperty(prop_Gas);
  // }

  if (checkbox_Gas)
  {
    configure_temperature_sensors();

    prop_Gas = new ThingProperty("Gas", "Gas usage measurement", NUMBER, nullptr, nullptr, nullptr);
    multisensor->addProperty(prop_Gas);
  }

  if (checkbox_Strom) // add SML here maybe?
  {
    prop_Strom = new ThingProperty("Electricity", "Electricity usage measurement", NUMBER, nullptr, nullptr, nullptr);
    multisensor->addProperty(prop_Strom);
  }

  if (checkbox_Wasser)
  {
    configure_impulse_pin();
    prop_Wasser = new ThingProperty("Water", "Water usage measurement", NUMBER, nullptr, nullptr, nullptr);
    multisensor->addProperty(prop_Wasser);
  }

  // ThingProperty TVOC("tvocConcentration", "TVOC value of SGP30 sensor", NUMBER, nullptr, "AmbientAir", nullptr);
  // ThingProperty eCO2("co2Concentration", "co2 concentration value of SGP30 sensor", NUMBER, nullptr, "CarbonDioxideConcentration", nullptr);
  // ThingProperty temperature("temperature", "temperature value of BME680 sensor", NUMBER, nullptr, "TemperatureSensing", nullptr);
  // ThingProperty humidity("humidity", "humidity value of BME680 sensor", NUMBER, nullptr, "HumiditySensing", nullptr);
  // ThingProperty pressure("pressure", "pressure value of BME680 sensor", NUMBER, nullptr, "PressureSensing", nullptr);

  // multisensor.addProperty(&TVOC);
  // multisensor.addProperty(&eCO2);
  // multisensor->addProperty(&temperature);
  // multisensor->addProperty(&pressure);
  // multisensor->addProperty(&humidity);

  mqttAdapter = new ThingMQTTAdapter("Smart_Meter", mqttbroker_Address.c_str());

  if (mqtt_needs_Auth)
  {
    Serial.println("MQTT Broker needs authentication");
    mqttAdapter->setmqttbroker_Credentials(mqtt_brokerauth_user.c_str(), mqtt_brokerauth_pass.c_str());
  }

  mqttAdapter->addDevice(multisensor);
  mqttAdapter->begin();
  // start update-functionality

  if (checkbox_Gas)
  {
    // start temperature sensor task
    xTaskCreatePinnedToCore(OneWireTask, "OneWireTask", 8000, NULL, 5, NULL, 1);
  }

  if (checkbox_Wasser)
  {
    // start some task for water
  }

  if (checkbox_Strom)
  {
    // start some task for electricity
  }

  AsyncElegantOTA.begin(&server);
  server.begin();
}

void loop()
{
  // put your main code here, to run repeatedly:
  // run update server
  // try to reconnect to the MQTT broker
  // reconnect();

  // perform background tasks for the communication using MQTT
  // MQTT_Client.loop();
  M5.update();
  test_button_press();
  connected_AP();
  update_Meter();
  AsyncElegantOTA.loop();

  // Serial.printf("counter = %d\n", impulse_counter);
  // Serial.println(digitalRead(impulse_pin));
  // current_meter_Reading = current_meter_Reading + 1.0;
  // Serial.println(current_meter_Reading);
  // delay(1000);
}

void OneWireTask(void *pvParameters)
{
  while (1)
  {
    // sensors.requestTemperatures();
    // float temperatureC = sensors.getTempCByIndex(0);
    // float temperatureF = sensors.getTempFByIndex(0);
    // Serial.print(temperatureC);
    // Serial.println("ºC");
    // Serial.print(temperatureF);
    // Serial.println("ºF");
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures();
    Serial.println("DONE");

    // print the device information
    printData(insideThermometer);
    printData(outsideThermometer);

    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

// function to print a device address



// void reconnect()
// {
//   Serial.print("Attempting MQTT connection...");

//   // Loop until we're reconnected
//   while (!MQTT_Client.connected())
//   {
//     int retries = 0;

//     if (retries < 5)
//     {

//       if (mqtt_needs_Auth)
//       {
//         connected_tobroker = MQTT_Client.connect(, mqttauth_Username.c_str(), mqttauth_Password.c_str(), lwt_Topic.c_str(), 1, true, lwt_message.c_str());
//       }
//       else
//       {
//         connected_tobroker = MQTT_Client.connect(device_ID, lwt_Topic.c_str(), 1, true, lwt_message.c_str());
//       }

//       Serial.println("Retries exceeded, restarting device");
//       ESP.restart();
//     }
//   }
// }
