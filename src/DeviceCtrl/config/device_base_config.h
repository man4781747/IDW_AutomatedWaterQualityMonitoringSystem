#ifndef Config_DeviceBase_H
#define Config_DeviceBase_H

#include "common.h"

struct DeviceBaseConfig : ConfigSetting {
  explicit DeviceBaseConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    //? 檢查各參數是否存在，不存在則使用預設值
    bool anyChange = false;
    if ((*json_data)["device_no"] == nullptr) {
      (*json_data)["device_no"].set("ID_PoolSensor");
      anyChange = true;
    }
    if ((*json_data)["device_name"] == nullptr) {
      (*json_data)["device_name"].set("Pool Sensor Device");
      anyChange = true;
    }
    if ((*json_data)["LINE_Notify_switch"] == nullptr) {
      (*json_data)["LINE_Notify_switch"].set(false);
      anyChange = true;
    }
    if ((*json_data)["LINE_Notify_id"] == nullptr) {
      (*json_data)["LINE_Notify_id"].set("");
      anyChange = true;
    }
    if ((*json_data)["Mail_Notify_switch"] == nullptr) {
      (*json_data)["Mail_Notify_switch"].set(false);
      anyChange = true;
    }
    if ((*json_data)["Mail_Notify_Auther"] == nullptr) {
      (*json_data)["Mail_Notify_Auther"].set("");
      anyChange = true;
    }
    if ((*json_data)["Mail_Notify_Key"] == nullptr) {
      (*json_data)["Mail_Notify_Key"].set("");
      anyChange = true;
    }
    if ((*json_data)["Mail_Notify_Target"] == nullptr) {
      (*json_data)["Mail_Notify_Target"].set("");
      anyChange = true;
    }
    if ((*json_data)["schedule_switch"] == nullptr) {
      (*json_data)["schedule_switch"].set(true);
      anyChange = true;
    }
    if (anyChange) {
      ExFile_WriteJsonFile(SD, path, *json_data);
    }
  }
};
#endif