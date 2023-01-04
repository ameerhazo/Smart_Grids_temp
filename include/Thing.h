/**
 * Thing.h
 *
 * Provides ThingProperty and ThingDevice classes for creating modular Web
 * Things.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#if !defined(ESP8266) && !defined(ESP32) && !defined(WITHOUT_WS)
#define WITHOUT_WS 1
#endif

#if !defined(WITHOUT_WS) && (defined(ESP8266) || defined(ESP32))
#include <ESPAsyncWebServer.h>
// #include <ESPRandom.h>
#include "IoT_Schema.h"
#endif

#include "mqtt_client.h"

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

#ifndef LARGE_JSON_DOCUMENT_SIZE
#ifdef LARGE_JSON_BUFFERS
#define LARGE_JSON_DOCUMENT_SIZE 4096
#else
#define LARGE_JSON_DOCUMENT_SIZE 4096
#endif
#endif

#ifndef SMALL_JSON_DOCUMENT_SIZE
#ifdef LARGE_JSON_BUFFERS
#define SMALL_JSON_DOCUMENT_SIZE 1024
#else
#define SMALL_JSON_DOCUMENT_SIZE 256
#endif
#endif

esp_mqtt_client_handle_t mqtt_client;

enum ThingDataType
{
  NO_STATE,
  BOOLEAN,
  NUMBER,
  INTEGER,
  STRING
};
typedef ThingDataType ThingPropertyType;

union ThingDataValue
{
  bool boolean;
  double number;
  signed long long integer;
  String *string;
};
typedef ThingDataValue ThingPropertyValue;

class ThingActionObject
{
private:
  void (*start_fn)(const JsonVariant &);
  void (*cancel_fn)();

public:
  String name;
  DynamicJsonDocument *actionRequest = nullptr;
  String timeRequested;
  String timeCompleted;
  String status;
  String id;
  ThingActionObject *next = nullptr;

  ThingActionObject(const char *name_, DynamicJsonDocument *actionRequest_,
                    void (*start_fn_)(const JsonVariant &),
                    void (*cancel_fn_)())
      : start_fn(start_fn_), cancel_fn(cancel_fn_), name(name_),
        actionRequest(actionRequest_),
        timeRequested("1970-01-01T00:00:00+00:00"), status("created")
  {
    generateId();
  }

  void generateId()
  {
    for (uint8_t i = 0; i < 16; ++i)
    {
      char c = (char)random('0', 'g');

      if (c > '9' && c < 'a')
      {
        --i;
        continue;
      }

      id += c;
    }
  }

  void serialize(JsonObject obj, String deviceId)
  {
    JsonObject data = obj.createNestedObject(name);

    JsonObject actionObj = actionRequest->as<JsonObject>();
    data["input"] = actionObj;

    data["status"] = status;
    data["timeRequested"] = timeRequested;

    if (timeCompleted != "")
    {
      data["timeCompleted"] = timeCompleted;
    }

    data["href"] = "/things/" + deviceId + "/actions/" + name + "/" + id;
  }

  void setStatus(const char *s)
  {
    status = s;
  }

  void start()
  {
    setStatus("pending");

    JsonObject actionObj = actionRequest->as<JsonObject>();
    start_fn(actionObj);

    finish();
  }

  void cancel()
  {
    if (cancel_fn != nullptr)
    {
      cancel_fn();
    }
  }

  void finish()
  {
    timeCompleted = "1970-01-01T00:00:00+00:00";
    setStatus("completed");
  }
};

class ThingItem
{
public:
  String id;
  String description;
  ThingDataType type;
  String atType;
  String objectType;
  ThingItem *next = nullptr;

  bool readOnly = false;
  // custom unit, iotschema unit will be derived from the @types of property constructor
  String unit = "";
  String title = "";
  double minimum = 0;
  double maximum = -1;
  double multipleOf = -1;

  ThingItem(const char *id_, const char *description_, ThingDataType type_,
            const char *objectType_, const char *atType_)
      : ThingItem(id_, "", description_, type_, objectType_, atType_, "", 0, -1, false) {}

  ThingItem(const char *id_, const char *description_, ThingDataType type_,
            const char *objectType_, const char *atType_, bool readOnly_)
      : ThingItem(id_, "", description_, type_, objectType_, atType_, "", 0, -1, readOnly_) {}

  ThingItem(const char *id_, const char *description_, ThingDataType type_,
            const char *atType_, const char *unit_, double minimum_, double maximum_, bool readOnly_)
      : ThingItem(id_, "", description_, type_, "", atType_, unit_, minimum_, maximum_, readOnly_) {}

  ThingItem(const char *id_, const char *description_, ThingDataType type_,
            const char *objectType_, const char *atType_, const char *unit_, double minimum_, double maximum_)
      : ThingItem(id_, "", description_, type_, objectType_, atType_, unit_, minimum_, maximum_, false) {}

  ThingItem(const char *id_, const char *description_, ThingDataType type_,
            const char *unit_, double minimum_, double maximum_)
      : ThingItem(id_, "", description_, type_, "", "", unit_, minimum_, maximum_, false) {}

  // constructor with all fields (more parameters can be added here)
  ThingItem(const char *id_, const char *title_, const char *description_, ThingDataType type_,
            const char *objectType_, const char *atType_, const char *unit_, double minimum_, double maximum_, bool readOnly_)
      : id(id_), title(title_), description(description_), type(type_), objectType(objectType_), atType(atType_), unit(unit_), minimum(minimum_), maximum(maximum_), readOnly(readOnly_) {}

  void setValue(ThingDataValue newValue)
  {
    switch (type)
    {
    case NO_STATE:
      break;
    case BOOLEAN:
    {
      bool current_boolean = this->getValue().boolean;
      if (current_boolean != newValue.boolean)
      {
        // Serial.println("Boolean value changed");
        this->value = newValue;
        this->hasChanged = true;
      }
      else
      {
        this->hasChanged = false;
      }
    }
    break;
    case NUMBER:
    {
      double diff = newValue.number - this->getValue().number;
      if (abs(diff) >= 0.5)
      {
        // Serial.println("Number value changed");
        this->value = newValue;
        this->hasChanged = true;
      }
      else
      {
        this->hasChanged = false;
      }
    }
    break;
    case INTEGER:
    {
      int diff = newValue.integer - this->getValue().integer;
      if (abs(diff) >= 1)
      {
        // Serial.println("Integer value changed");
        this->value = newValue;
        this->hasChanged = true;
      }
      else
      {
        this->hasChanged = false;
      }
    }
    break;
    case STRING:
    {
      this->value = newValue;
      this->hasChanged = true;
    }
    break;
    }
  }

  void setValue(const char *s)
  {
    *(this->getValue().string) = s;
    this->hasChanged = true;
  }

  /**
   * Returns the property value if it has been changed via {@link setValue}
   * since the last call or returns a nullptr.
   */
  ThingDataValue *changedValueOrNull()
  {
    ThingDataValue *v = this->hasChanged ? &this->value : nullptr;
    this->hasChanged = false;
    return v;
  }

  bool valuehasChanged(void)
  {
    return this->hasChanged;
  }

  ThingDataValue getValue() { return this->value; }

  void serialize_property(JsonObject obj, String deviceId)
  {

    JsonObject property = obj.createNestedObject(id);

    switch (type)
    {
    case NO_STATE:
      break;
    case BOOLEAN:
      property["instanceOf"] = "org.ict.model.wot.dataschema.BooleanSchema";
      property["type"] = "boolean";
      break;
    case NUMBER:
      property["instanceOf"] = "org.ict.model.wot.dataschema.NumberSchema";
      property["type"] = "number";
      break;
    case INTEGER:
      property["instanceOf"] = "org.ict.model.wot.dataschema.IntegerSchema";
      property["type"] = "integer";
      break;
    case STRING:
      property["instanceOf"] = "org.ict.model.wot.dataschema.StringSchema";
      property["type"] = "string";
      break;
    }

    if (readOnly)
    {
      property["readOnly"] = true;
    }

    if (unit != "")
    {
      property["unit"] = unit;
    }

    if (title != "")
    {
      property["title"] = title;
    }

    if (description != "")
    {
      property["description"] = description;
    }

    if ((minimum < maximum) && ((type == NUMBER) || (type == INTEGER)))
    {
      property["minimum"] = minimum;
    }

    if ((maximum > minimum) && ((type == NUMBER) || (type == INTEGER)))
    {
      property["maximum"] = maximum;
    }

    if ((multipleOf > 0) && ((type == NUMBER) || (type == INTEGER)))
    {
      property["multipleOf"] = multipleOf;
    }

    if (atType != nullptr)
    {
      JsonArray atType_2 = property.createNestedArray("@type");
      atType_2.add(atType);
    }
    else
    {
      JsonArray atType_2 = property.createNestedArray("@type");
      atType_2.add("iot:TODO");
    }
  }

  void serialize_event(JsonObject obj, String deviceId)
  {
    JsonObject data = obj.createNestedObject("data");

    switch (type)
    {
    case NO_STATE:
      break;
    case BOOLEAN:
      data["instanceOf"] = "org.ict.model.wot.dataschema.BooleanSchema";
      data["type"] = "boolean";
      break;
    case NUMBER:
      data["instanceOf"] = "org.ict.model.wot.dataschema.NumberSchema";
      data["type"] = "number";
      break;
    case INTEGER:
      data["instanceOf"] = "org.ict.model.wot.dataschema.IntegerSchema";
      data["type"] = "integer";
      break;
    case STRING:
      data["instanceOf"] = "org.ict.model.wot.dataschema.StringSchema";
      data["type"] = "string";
      break;
    }

    if (readOnly)
    {
      obj["readOnly"] = true;
    }

    if (unit != "")
    {
      data["unit"] = unit;
    }

    if (description != "")
    {
      obj["description"] = description;
    }

    if ((minimum < maximum) && ((type == NUMBER) || (type == INTEGER)))
    {
      data["minimum"] = minimum;
    }

    if ((maximum > minimum) && ((type == NUMBER) || (type == INTEGER)))
    {
      data["maximum"] = maximum;
    }

    if ((multipleOf > 0) && ((type == NUMBER) || (type == INTEGER)))
    {
      data["multipleOf"] = multipleOf;
    }

    if (atType != nullptr)
    {
      data["@type"] = atType;
    }

    // 2.9 Property object: A links array (An array of Link objects linking
    // to one or more representations of a Property resource, each with an
    // implied default rel=property.)
    JsonArray inline_links = obj.createNestedArray("forms");
    JsonObject inline_links_prop = inline_links.createNestedObject();
    JsonArray op = inline_links_prop.createNestedArray("op");
    op.add("subscribeevent");
    // inline_links_prop["op"] = "subscribeevent";
    inline_links_prop["href"] = "/things/" + deviceId + "/actions/" + id;
    inline_links_prop["subprotocol"] = "longpoll";
  }

  void serializeValue(JsonObject prop)
  {
    time_t timestamp;
    time(&timestamp);
    int time_str = timestamp;

    String time = String(time_str);
    prop["time"] = time;
    switch (this->type)
    {
    case NO_STATE:
      break;
    case BOOLEAN:
      prop[this->id] = this->getValue().boolean;
      break;
    case NUMBER:
      prop[this->id] = this->getValue().number;
      break;
    case INTEGER:
      prop[this->id] = this->getValue().integer;
      break;
    case STRING:
      prop[this->id] = *this->getValue().string;
      break;
    }
  }

private:
  ThingDataValue value = {false};
  bool hasChanged = false;
};

class ThingProperty : public ThingItem
{
private:
  void (*callback)(ThingPropertyValue);

public:
  const char **propertyEnum = nullptr;
  bool observable = false;
  String object_Description = "";
  String devID = "";

  ThingProperty(const char *id_, const char *description_, ThingDataType type_,
                const char *objectType_, const char *atType_,
                void (*callback_)(ThingPropertyValue) = nullptr)
      : ThingItem(id_, description_, type_, objectType_, atType_), callback(callback_) {}

  ThingProperty(const char *id_, const char *description_, ThingDataType type_,
                const char *objectType_, const char *atType_, bool readOnly_,
                void (*callback_)(ThingPropertyValue) = nullptr)
      : ThingItem(id_, description_, type_, objectType_, atType_, readOnly_), callback(callback_) {}

  ThingProperty(const char *id_, const char *description_, ThingDataType type_,
                const char *objectType_, const char *atType_, const char *unit_, double minimum_, double maximum_,
                void (*callback_)(ThingPropertyValue) = nullptr)
      : ThingItem(id_, description_, type_, objectType_, atType_, unit_, minimum_, maximum_), callback(callback_) {}

  ThingProperty(const char *id_, const char *description_, ThingDataType type_,
                IOT_SCHEMA iot_, double minimum_, double maximum_, bool readOnly_,
                void (*callback_)(ThingPropertyValue) = nullptr)
      : ThingItem(id_, iot_schema_data_type_title[iot_.propertyType], iot_schema_data_type_description[iot_.propertyType], type_, iot_.capability_Name, iot_schema_data_string[iot_.propertyType], iot_schema_data_type_unit[iot_.propertyType], minimum_, maximum_, readOnly_), object_Description(description_), callback(callback_) {}

  void serialize(JsonObject obj, String ip_addr, String deviceId)
  {
    devID = deviceId;
    JsonObject inner_properties = obj.createNestedObject("properties");
    ThingItem::serialize_property(inner_properties, deviceId);

    // time properties
    JsonObject time = inner_properties.createNestedObject("time");
    time["title"] = "Date time";
    time["description"] = "the date time in ISO 8601 format";
    time["type"] = "string";
    time["instanceOf"] = "org.ict.model.wot.dataschema.StringSchema";
    JsonArray at_Type = time.createNestedArray("@type");
    at_Type.add("http://schema.org/DateTime");
    time["readOnly"] = true;
    time["writeOnly"] = false;

    const char **enumVal = propertyEnum;
    bool hasEnum = propertyEnum != nullptr && *propertyEnum != nullptr;

    if (hasEnum)
    {
      enumVal = propertyEnum;
      JsonArray propEnum = obj.createNestedArray("enum");
      while (propertyEnum != nullptr && *enumVal != nullptr)
      {
        propEnum.add(*enumVal);
        enumVal++;
      }
    }

    JsonArray required = obj.createNestedArray("required");
    required.add("time");
    required.add(id);

    obj["title"] = id;

    obj["instanceOf"] = "org.ict.model.wot.dataschema.ObjectSchema";
    JsonArray schema_at_Type = obj.createNestedArray("@type");

    if (objectType != nullptr)
    {
      String iot_schema = "iot:";
      iot_schema.concat(objectType);
    }
    else
    {
      String iot_schema = "iot:TODO";
      schema_at_Type.add(iot_schema);
    }

    obj["type"] = "object";
    if (callback != nullptr)
    {
      obj["readOnly"] = false;
    }
    else
    {
      obj["readOnly"] = true;
    }
    obj["writeOnly"] = false;

    JsonArray inline_links = obj.createNestedArray("forms");
    JsonObject inline_links_prop = inline_links.createNestedObject();
    JsonArray op = inline_links_prop.createNestedArray("op");
    op.add("subscribeevent");
    inline_links_prop["href"] = ip_addr + ":1883/things/" + deviceId + "/properties/" + id;
    inline_links_prop["contentType"] = "application/json";

    // if (callback != nullptr)
    // {
    //   JsonObject inline_links_prop_put = inline_links.createNestedObject();
    //   JsonArray op_put = inline_links_prop_put.createNestedArray("op");
    //   op_put.add("writeproperty");
    //   // inline_links_prop_put["op"] = "writeproperty";
    //   inline_links_prop_put["cov:methodName"] = "PUT";
    //   inline_links_prop_put["href"] = "coap://" + WiFi.localIP().toString() + ":5683/things/" + deviceId + "/properties/" + id;
    //   inline_links_prop_put["contentType"] = "application/json";
    // }
  }

  void hasChanged(void)
  {
    if (valuehasChanged())
    {
      StaticJsonDocument<500> buf;
      JsonObject prop_Payload = buf.to<JsonObject>();
      this->serializeValue(prop_Payload);
      String json_Payload;
      serializeJson(prop_Payload, json_Payload);
      Serial.println(json_Payload);
      // TODO: figure out how to genericly publish property item
      String prop_topic = "things/" + devID + "/properties/" + id;
      // String prop_topic = "things/" + devID + "/properties/" + id;
      // esp_mqtt_client_enqueue(mqtt_client, prop_topic.c_str(), json_Payload.c_str(), 0, 1, 1, true);
      esp_mqtt_client_publish(mqtt_client, prop_topic.c_str(), json_Payload.c_str(), 0, 1, 1);
    }
  }

  void changed(ThingPropertyValue newValue)
  {
    if (callback != nullptr)
    {
      callback(newValue);
    }
  }
};
using ThingEvent = ThingItem;

class ThingAction
{
private:
  ThingActionObject *(*generator_fn)(DynamicJsonDocument *);

public:
  String id;
  String title;
  String description;
  String type;
  JsonObject *input;
  ThingDataType input_Datatype;
  JsonObject *output;
  ThingDataType output_Datatype;
  ThingAction *next = nullptr;
  ThingProperty *action_Property = nullptr;

  // TODO create constructor with input and check out espweb for action handler
  ThingAction(const char *id_,
              ThingActionObject *(*generator_fn_)(DynamicJsonDocument *))
      : generator_fn(generator_fn_), id(id_) {}

  ThingAction(const char *id_, JsonObject *input_,
              ThingActionObject *(*generator_fn_)(DynamicJsonDocument *))
      : generator_fn(generator_fn_), id(id_), input(input_) {}

  ThingAction(const char *id_, const char *title_, const char *description_,
              const char *type_, JsonObject *input_, JsonObject *output_,
              ThingActionObject *(*generator_fn_)(DynamicJsonDocument *), ThingProperty *action_Property_)
      : generator_fn(generator_fn_), id(id_), title(title_),
        description(description_), type(type_), input(input_), output(output_), action_Property(action_Property_) {}

  ThingAction(const char *id_, const char *title_, const char *description_,
              const char *type_, JsonObject *input_, JsonObject *output_, ThingDataType input_Datatype_, ThingDataType output_Datatype_,
              ThingActionObject *(*generator_fn_)(DynamicJsonDocument *))
      : generator_fn(generator_fn_), id(id_), title(title_),
        description(description_), type(type_), input(input_), output(output_), input_Datatype(input_Datatype_), output_Datatype(output_Datatype_) {}

  ThingActionObject *create(DynamicJsonDocument *actionRequest)
  {
    return generator_fn(actionRequest);
  }

  void serialize(JsonObject obj, String deviceId)
  {

    if (title != "")
    {
      obj["title"] = title;
    }

    if (description != "")
    {
      obj["description"] = description;
    }

    if (type != "")
    {
      JsonArray at_Type = obj.createNestedArray("@type");
      at_Type.add(type);
    }

    obj["safe"] = false;
    obj["idempotent"] = false;

    if (input != nullptr)
    {
      JsonObject input_Root = obj.createNestedObject("input");
      input_Root["type"] = "object";
      input_Root["readOnly"] = false;
      input_Root["writeOnly"] = true;

      for (JsonPair kv : *input)
      {
        input_Root[kv.key()] = kv.value();
      }
    }

    if (output != nullptr)
    {
      JsonObject output_root = obj.createNestedObject("output");
      output_root["type"] = "object";
      output_root["readOnly"] = true;
      output_root["writeOnly"] = false;

      for (JsonPair kv : *output)
      {
        output_root[kv.key()] = kv.value();
      }
    }

    // 2.11 Action object: A links array (An array of Link objects linking
    // to one or more representations of an Action resource, each with an
    // implied default rel=action.)
    JsonArray inline_links = obj.createNestedArray("forms");
    JsonObject inline_links_action = inline_links.createNestedObject();
    JsonArray op = inline_links_action.createNestedArray("op");
    op.add("invokeaction");
    inline_links_action["cov:methodName"] = "PUT";
    inline_links_action["href"] = "coap://" + WiFi.localIP().toString() + ":5683/things/" + deviceId + "/actions/" + id;
    inline_links_action["contentType"] = "application/json";
  }
};

class ThingEventObject
{
public:
  String name;
  ThingDataType type;
  ThingDataValue value = {false};
  String timestamp;
  ThingEventObject *next = nullptr;

  ThingEventObject(const char *name_, ThingDataType type_,
                   ThingDataValue value_)
      : name(name_), type(type_), value(value_),
        timestamp("1970-01-01T00:00:00+00:00") {}

  ThingEventObject(const char *name_, ThingDataType type_,
                   ThingDataValue value_, String timestamp_)
      : name(name_), type(type_), value(value_), timestamp(timestamp_) {}

  ThingDataValue getValue() { return this->value; }

  void serialize(JsonObject obj)
  {
    JsonObject data = obj.createNestedObject(name);
    switch (this->type)
    {
    case NO_STATE:
      break;
    case BOOLEAN:
      data["data"] = this->getValue().boolean;
      break;
    case NUMBER:
      data["data"] = this->getValue().number;
      break;
    case INTEGER:
      data["data"] = this->getValue().integer;
      break;
    case STRING:
      data["data"] = *this->getValue().string;
      break;
    }
  }
};

class ThingDevice
{
public:
  String id;
  String title;
  String description;
  const char **type;
  ThingDevice *next = nullptr;
  ThingProperty *firstProperty = nullptr;
  ThingAction *firstAction = nullptr;
  ThingActionObject *actionQueue = nullptr;
  ThingEvent *firstEvent = nullptr;
  ThingEventObject *eventQueue = nullptr;

  ThingDevice(const char *_id, const char *_title, const char **_type)
      : id(_id), title(_title), type(_type) {}

  ~ThingDevice()
  {
  }

  ThingProperty *findProperty(const char *id)
  {
    ThingProperty *p = this->firstProperty;
    while (p)
    {
      if (!strcmp(p->id.c_str(), id))
        return p;
      p = (ThingProperty *)p->next;
    }
    return nullptr;
  }

  void addProperty(ThingProperty *property)
  {
    property->next = firstProperty;
    firstProperty = property;
  }

  ThingAction *findAction(const char *id)
  {
    ThingAction *a = this->firstAction;
    while (a)
    {
      if (!strcmp(a->id.c_str(), id))
        return a;
      a = (ThingAction *)a->next;
    }
    return nullptr;
  }

  ThingActionObject *findActionObject(const char *id)
  {
    ThingActionObject *a = this->actionQueue;
    while (a)
    {
      if (!strcmp(a->id.c_str(), id))
        return a;
      a = a->next;
    }
    return nullptr;
  }

  void addAction(ThingAction *action)
  {
    action->next = firstAction;
    firstAction = action;
  }

  ThingEvent *findEvent(const char *id)
  {
    ThingEvent *e = this->firstEvent;
    while (e)
    {
      if (!strcmp(e->id.c_str(), id))
        return e;
      e = (ThingEvent *)e->next;
    }
    return nullptr;
  }

  void addEvent(ThingEvent *event)
  {
    event->next = firstEvent;
    firstEvent = event;
  }

  void setProperty(const char *name, const JsonVariant &newValue)
  {
    ThingProperty *property = findProperty(name);

    if (property == nullptr)
    {
      return;
    }

    switch (property->type)
    {
    case NO_STATE:
    {
      break;
    }
    case BOOLEAN:
    {
      ThingDataValue value;
      value.boolean = newValue.as<bool>();
      property->setValue(value);
      property->changed(value);
      break;
    }
    case NUMBER:
    {
      ThingDataValue value;
      value.number = newValue.as<double>();
      property->setValue(value);
      property->changed(value);
      break;
    }
    case INTEGER:
    {
      ThingDataValue value;
      value.integer = newValue.as<signed long long>();
      property->setValue(value);
      property->changed(value);
      break;
    }
    case STRING:
      property->setValue(newValue.as<const char *>());
      property->changed(property->getValue());
      break;
    }
  }

  ThingActionObject *requestAction(DynamicJsonDocument *actionRequest)
  {
    JsonObject actionObj = actionRequest->as<JsonObject>();

    // There should only be one key/value pair
    JsonObject::iterator kv = actionObj.begin();
    if (kv == actionObj.end())
    {
      return nullptr;
    }

    ThingAction *action = findAction(kv->key().c_str());
    if (action == nullptr)
    {
      return nullptr;
    }

    ThingActionObject *obj = action->create(actionRequest);
    if (obj == nullptr)
    {

      return nullptr;
    }

    queueActionObject(obj);

    return obj;
  }

  ThingActionObject *requestAction(DynamicJsonDocument *actionRequest, ThingAction *action)
  {
    JsonObject actionObj = actionRequest->as<JsonObject>();

    // There should only be one key/value pair
    JsonObject::iterator kv = actionObj.begin();
    if (kv == actionObj.end())
    {
      return nullptr;
    }

    ThingActionObject *obj = action->create(actionRequest);
    if (obj == nullptr)
    {

      return nullptr;
    }

    return obj;
  }

  void removeAction(String id)
  {
    ThingActionObject *curr = actionQueue;
    ThingActionObject *prev = nullptr;
    while (curr != nullptr)
    {
      if (curr->id == id)
      {
        if (prev == nullptr)
        {
          actionQueue = curr->next;
        }
        else
        {
          prev->next = curr->next;
        }

        curr->cancel();
        delete curr->actionRequest;
        delete curr;
        return;
      }

      prev = curr;
      curr = curr->next;
    }
  }

  void queueActionObject(ThingActionObject *obj)
  {
    obj->next = actionQueue;
    actionQueue = obj;
  }

  void queueEventObject(ThingEventObject *obj)
  {
    obj->next = eventQueue;
    eventQueue = obj;
  }

  void serialize(JsonObject descr, String ip, String MAC)
  {
    // descr["id"] = ip + ":1883/things/" + this->id;
    descr["id"] = ip + ":1883/things/" + MAC;
    descr["title"] = this->title;
    // WebThingsIO @context
    // descr["@context"] = "https://webthings.io/schemas/";
    // end WebThingsIO @context
    JsonArray context = descr.createNestedArray("@context");
    context.add("https://www.w3.org/2019/wot/td/v1");
    JsonObject iot = context.createNestedObject();
    iot["iot"] = "http://iotschema.org/";
    // JsonObject coap = context.createNestedObject();
    // coap["cov"] = "http://www.example.org/coap-binding#";
    JsonObject mqtt = context.createNestedObject();
    mqtt["mqv"] = "http://www.example.org/mqtt-binding#";

    if (this->description != "")
    {
      descr["description"] = this->description;
    }

    // if (port != 5683)
    // {
    //   descr["base"] = "http://" + ip + "/";
    // }
    // else
    // {
    //   char buffer[33];
    //   itoa(port, buffer, 10);
    //   descr["base"] = "coap://" + ip + ":" + buffer + "/";
    // }

    JsonObject securityDefinitions = descr.createNestedObject("securityDefinitions");
    // JsonObject nosecSc = securityDefinitions.createNestedObject("nosec_sc");
    // nosecSc["scheme"] = "nosec";

    JsonObject basicSc = securityDefinitions.createNestedObject("basic_sc");
    basicSc["scheme"] = "basic";
    basicSc["instanceOf"] = "org.ict.model.wot.security.BasicSecurityScheme";
    basicSc["in"] = "header";

    JsonArray securityJson = descr.createNestedArray("security");
    // securityJson.add("nosec_sc");
    securityJson.add("basic_sc");

    JsonArray typeJson = descr.createNestedArray("@type");
    const char **type = this->type;
    while ((*type) != nullptr)
    {
      typeJson.add(*type);
      type++;
    }

    ThingProperty *property = this->firstProperty;
    if (property != nullptr)
    {
      Serial.println("Try to serialize properties.");
      JsonObject properties = descr.createNestedObject("properties");
      while (property != nullptr)
      {
        JsonObject obj = properties.createNestedObject(property->id);
        property->serialize(obj, ip, MAC);
        property = (ThingProperty *)property->next;
      }
    }

    ThingAction *action = this->firstAction;
    if (action != nullptr)
    {
      JsonObject actions = descr.createNestedObject("actions");
      while (action != nullptr)
      {
        JsonObject obj = actions.createNestedObject(action->id);
        action->serialize(obj, id);
        action = action->next;
      }
    }

    ThingEvent *event = this->firstEvent;
    if (event != nullptr)
    {
      JsonObject events = descr.createNestedObject("events");
      while (event != nullptr)
      {
        JsonObject obj = events.createNestedObject(event->id);
        event->serialize_event(obj, id);
        event = (ThingEvent *)event->next;
      }
    }
  }

  void serializeActionQueue(JsonArray array)
  {
    ThingActionObject *curr = actionQueue;
    while (curr != nullptr)
    {
      JsonObject action = array.createNestedObject();
      curr->serialize(action, id);
      curr = curr->next;
    }
  }

  void serializeActionQueue(JsonArray array, String name)
  {
    ThingActionObject *curr = actionQueue;
    while (curr != nullptr)
    {
      if (curr->name == name)
      {
        JsonObject action = array.createNestedObject();
        curr->serialize(action, id);
      }
      curr = curr->next;
    }
  }

  void serializeEventQueue(JsonArray array)
  {
    ThingEventObject *curr = eventQueue;
    while (curr != nullptr)
    {
      JsonObject event = array.createNestedObject();
      curr->serialize(event);
      curr = curr->next;
    }
  }

  void serializeEventQueue(JsonArray array, String name)
  {
    ThingEventObject *curr = eventQueue;
    while (curr != nullptr)
    {
      if (curr->name == name)
      {
        JsonObject event = array.createNestedObject();
        curr->serialize(event);
      }
      curr = curr->next;
    }
  }
};
