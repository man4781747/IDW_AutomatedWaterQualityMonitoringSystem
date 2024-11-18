#ifndef Config_Schedule_H
#define Config_Schedule_H

#include "common.h"

struct ScheduleConfig : ConfigSetting {
  explicit ScheduleConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    if ((*json_data).as<JsonArray>().size()==0) {
      ESP_LOGW("CONFIG", "排程數據有誤，重建之");
      for (int i = 0;i<24;i++) {
        (*json_data).add("-");
      }
      ExFile_WriteJsonFile(SD, path, *json_data);
    }
  }
};

#endif