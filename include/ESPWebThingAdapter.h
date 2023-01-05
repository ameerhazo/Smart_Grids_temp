/**
 * ESPWebThingAdapter.h
 *
 * Exposes the Web Thing API based on provided ThingDevices.
 * Suitable for ESP32 and ESP8266 using ESPAsyncWebServer and ESPAsyncTCP
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>

#ifdef ESP8266
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#endif
#include "Thing.h"

#define ESP_MAX_PUT_BODY_SIZE 512

#ifndef LARGE_JSON_DOCUMENT_SIZE
#ifdef LARGE_JSON_BUFFERS
#define LARGE_JSON_DOCUMENT_SIZE 10000
#else
#define LARGE_JSON_DOCUMENT_SIZE 10000
#endif
#endif

#ifndef SMALL_JSON_DOCUMENT_SIZE
#ifdef LARGE_JSON_BUFFERS
#define SMALL_JSON_DOCUMENT_SIZE 1024
#else
#define SMALL_JSON_DOCUMENT_SIZE 256
#endif
#endif

String test_TD;
int TD_len;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

class ThingMQTTAdapter
{
public:
  ThingMQTTAdapter(String _name, String mqttbroker_Address_)
      : name(_name)
  {
    mqttbroker_Address.concat(mqttbroker_Address_);
  }

  bool begin()
  {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[18] = {0};
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    MAC = String(macStr);
    MAC.toLowerCase();
    // create the TD

    esp_mqtt_client_config_t mqtt_cfg = {};

    mqtt_cfg.uri = mqttbroker_Address.c_str();

    get_TD();

    // if for later integration
    // if (mqtt_needs_Auth)
    // {
    if (mqttauth_Username != "" && mqttauth_Password != "")
    {
      Serial.println("added credentials");
      mqtt_cfg.username = mqttauth_Username.c_str();
      mqtt_cfg.password = mqttauth_Password.c_str();
    }
    else
    {
      return false;
    }
    // }

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);

    // then connect to broker

    esp_mqtt_client_start(mqtt_client);

    return true;
  }

  void publish_TD()
  {
    ThingDevice *device = this->firstDevice;

    // publish the device TD on its topics
    while (device != nullptr)
    {
      String abs_path_td = "things/" + MAC;
      Serial.println(abs_path_td);

      // publish here
      esp_mqtt_client_publish(mqtt_client, abs_path_td.c_str(), test_TD.c_str(), 0, 1, 1);

      device = device->next;
    }
  }

  void get_TD()
  {
    DynamicJsonDocument buf(LARGE_JSON_DOCUMENT_SIZE);

    JsonObject thing = buf.to<JsonObject>();
    ThingDevice *device = this->firstDevice;

    while (device != nullptr)
    {
      device->serialize(thing, mqttbroker_Address, MAC);
      device = device->next;
    }

    serializeJson(thing, test_TD);

    TD_len = test_TD.length();

    Serial.printf("TD len = %d\n", TD_len);

    Serial.println(test_TD);

    // serializeJson(thing, TD);
  }

  void update()
  {
#ifdef ESP8266
    MDNS.update();
#endif
  }

  void addDevice(ThingDevice *device)
  {
    if (this->lastDevice == nullptr)
    {
      this->firstDevice = device;
      this->lastDevice = device;
    }
    else
    {
      this->lastDevice->next = device;
      this->lastDevice = device;
    }
  }

  ThingDevice *get_devices()
  {
    return this->firstDevice;
  }

  void setmqttbroker_Credentials(String mqttauth_Username_, String mqttauth_Password_)
  {
    this->mqttauth_Username = mqttauth_Username_;
    this->mqttauth_Password = mqttauth_Password_;
  }

private:
  String name;
  String mqttbroker_Address = "mqtt://";
  String mqttauth_Username;
  String mqttauth_Password;
  String MAC;
  uint16_t port;
  bool disableHostValidation;
  ThingDevice *firstDevice = nullptr;
  ThingDevice *lastDevice = nullptr;
};

ThingMQTTAdapter *mqttAdapter;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  // ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
  esp_mqtt_event_t *event = (esp_mqtt_event_t *)event_data;
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;
  switch ((esp_mqtt_event_id_t)event_id)
  {
  case MQTT_EVENT_CONNECTED:
    Serial.println("Connected to MQTT broker, publishing TD");
    mqttAdapter->publish_TD();
    // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
    break;
  // case MQTT_EVENT_DISCONNECTED:
  //   ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
  //   break;

  // case MQTT_EVENT_SUBSCRIBED:
  //   ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
  //   msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
  //   ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
  //   break;
  // case MQTT_EVENT_UNSUBSCRIBED:
  //   ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
  //   break;
  // case MQTT_EVENT_PUBLISHED:
  //   ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
  //   break;
  // case MQTT_EVENT_DATA:
  //   ESP_LOGI(TAG, "MQTT_EVENT_DATA");
  //   printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
  //   printf("DATA=%.*s\r\n", event->data_len, event->data);
  //   break;
  // case MQTT_EVENT_ERROR:
  //   ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
  //   if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
  //   {
  //     log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
  //     log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
  //     log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
  //     ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
  //   }
  //   break;
  default:
    break;
  }
}
