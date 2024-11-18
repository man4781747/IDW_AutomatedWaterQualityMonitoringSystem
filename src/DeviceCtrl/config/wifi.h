#ifndef Config_WiFi_H
#define Config_WiFi_H

#include "common.h"

struct WiFiConfig : ConfigSetting {
  explicit WiFiConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    //? 檢查各參數是否存在，不存在則使用預設值
    bool anyChange = false;
    if ((*json_data)["AP"]["AP_IP"] == nullptr) {
      (*json_data)["AP"]["AP_IP"].set("192.168.0.4");
      anyChange = true;
    }
    if ((*json_data)["AP"]["AP_gateway"] == nullptr) {
      (*json_data)["AP"]["AP_gateway"].set("192.168.0.1");
      anyChange = true;
    }
    if ((*json_data)["AP"]["AP_subnet_mask"] == nullptr) {
      (*json_data)["AP"]["AP_subnet_mask"].set("255.255.255.0");
      anyChange = true;
    }
    if ((*json_data)["AP"]["AP_Name"] == nullptr) {
      (*json_data)["AP"]["AP_Name"].set("ID_PoolSensor");
      anyChange = true;
    }
    if ((*json_data)["AP"]["AP_Password"] == nullptr) {
      (*json_data)["AP"]["AP_Password"].set("12345678");
      anyChange = true;
    }
    if ((*json_data)["Remote"]["remote_Name"] == nullptr) {
      (*json_data)["Remote"]["remote_Name"].set("IDWATER");
      anyChange = true;
    }
    if ((*json_data)["Remote"]["remote_Password"] == nullptr) {
      (*json_data)["Remote"]["remote_Password"].set("56651588");
      anyChange = true;
    }
    if ((*json_data)["Remote"]["checker"]["check"] == nullptr) {
      (*json_data)["Remote"]["checker"]["check"].set(false);
      anyChange = true;
    }
    if ((*json_data)["Remote"]["checker"]["check_IP"] == nullptr) {
      (*json_data)["Remote"]["checker"]["check_IP"].set("8.8.8.8");
      anyChange = true;
    }
    if (anyChange) {
      ExFile_WriteJsonFile(SD, path, *json_data);
    }
  }
};

#endif