#ifndef Config_Common_H
#define Config_Common_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include "StorgeSystemExternalFunction.h"
#include "TimeLibExternalFunction.h"

struct ConfigSetting
{
  explicit ConfigSetting(String filePath, uint16_t dataSize) {
    path = filePath;
    json_data = new DynamicJsonDocument(dataSize);
  };
  virtual void loadConfig(bool rebuild=false) {
    ExFile_LoadJsonFile(SD, path, *json_data);
  };
  virtual void writeConfig() {
    ExFile_WriteJsonFile(SD, path, *json_data);
  };
  virtual ~ConfigSetting() {
    json_data->clear();
  }
  String JsonFormatString(bool Pretty=false){
    String returnString;
    if (Pretty) {serializeJsonPretty(*json_data, returnString);}
    else {serializeJson(*json_data, returnString);}
    return returnString;
  };
  String path = "";
  DynamicJsonDocument *json_data;
};


#endif