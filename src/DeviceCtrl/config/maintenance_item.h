#ifndef Config_MaintenanceItem_H
#define Config_MaintenanceItem_H

#include "common.h"

struct MaintenanceItemConfig : ConfigSetting {
  explicit MaintenanceItemConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    bool anyChange = false;
    const char* defaultConfig[5] = {"RO", "NO2_R1", "NH4_R1", "NH4_R2", "pH"};
    const char* nameConfig[5] = {"自來水", "亞硝酸鹽 R1", "氨氮 R1", "氨氮 R2", "pH計"};
    const int resetRemaining[5] = {500, 300, 300, 300, 99999};
    const bool defaultTimeCheckSwitch[5] = {true, true, true, true, true};
    const bool defaultRemainingCheckSwitch[5] = {true,true,true,true,false};
    for (int i=0;i<5;i++) {
      if ((*json_data)[defaultConfig[i]]["time_check_switch"] == nullptr) {
        (*json_data)[defaultConfig[i]]["time_check_switch"].set(defaultTimeCheckSwitch[i]);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["start_time"] == nullptr) {
        (*json_data)[defaultConfig[i]]["start_time"].set(GetDateString("-"));
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["time_check_days"] == nullptr) {
        (*json_data)[defaultConfig[i]]["day_alarm"].set(30);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["remaining_check_switch"] == nullptr) {
        (*json_data)[defaultConfig[i]]["remaining_check_switch"].set(defaultRemainingCheckSwitch[i]);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["remaining"] == nullptr) {
        (*json_data)[defaultConfig[i]]["remaining"].set(resetRemaining[i]);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["reset_remaining"] == nullptr) {
        (*json_data)[defaultConfig[i]]["reset_remaining"].set(resetRemaining[i]);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["remaining_alarm"] == nullptr) {
        (*json_data)[defaultConfig[i]]["remaining_alarm"].set(10);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["name"] == nullptr) {
        (*json_data)[defaultConfig[i]]["name"].set(nameConfig[i]);
        anyChange = true;
      }


    }
    if (anyChange) {
      ExFile_WriteJsonFile(SD, path, *json_data);
    }
  }
};

#endif