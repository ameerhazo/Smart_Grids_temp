#ifndef IOT_SCHEMA_H
#define IOT_SCHEMA_H

static const char *iot_schema_data_string[] = {
    "VoltageData",
    "ActiveEnergyData",
    "ElectricCurrentData",
    "ActivePowerData",
    "ReactivePowerData"};

static const char *iot_schema_data_type_unit[] = {
    "Volt",
    "WattHour",
    "Ampere",
    "Watt",
    "VoltAmpereReactive"};

static const char *iot_schema_data_type_description[] = {
    "Voltage(Volt)",
    "Energie(Wh)",
    "current in ampere",
    "Power(W)",
    "Reactive power(VAR)"};

static const char *iot_schema_data_type_title[] = {
    "voltage",
    "energy",
    "electric current",
    "power",
    "reactive power"};

static const char *om_data_string[] = {
    "Angle",
};

static const char *om_data_type_unit[] = {
    "degree",
};

static const char *om_data_type_description[] = {
    "phase angle in degree",
};

enum iot_schema_data_type
{
  VoltageData,
  ActiveEnergyData,
  ElectricCurrentData,
  ActivePowerData,
  ReactivePowerData
};

enum om_data_type
{
  Angle,
};

typedef struct iot_schema_interaction_pattern
{
    char capability_Name[40]; 
    iot_schema_data_type propertyType;
} IOT_SCHEMA;

// IOT_SCHEMA ActiveEnergyImported;
// strcpy(ActiveEnergyImported.capability_Name, "ActiveEnergyImported");
// ActiveEnergyImported.propertyType = ActiveEnergyData;
// IOT_SCHEMA ActiveEnergyExported;
// strcpy(ActiveEnergyExp.capability_Name, "ActiveEnergyExported");
// ActiveEnergyImported.propertyType = ActiveEnergyData;
 



#endif