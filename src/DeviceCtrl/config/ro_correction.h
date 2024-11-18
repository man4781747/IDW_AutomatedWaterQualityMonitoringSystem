#ifndef Config_RO_Correction_H
#define Config_RO_Correction_H

#include "common.h"

struct RO_CorrectionConfig : ConfigSetting {
  explicit RO_CorrectionConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    bool anyChange = false;
    if ((*json_data)["switch"] == nullptr) {
      (*json_data)["switch"].set(false);
      anyChange = true;
    }
    if ((*json_data)["data"]["NO2"] == nullptr) {
      (*json_data)["data"]["NO2"].set(0);
      anyChange = true;
    }
    if ((*json_data)["data"]["NH4"] == nullptr) {
      (*json_data)["data"]["NH4"].set(0);
      anyChange = true;
    }
    if ((*json_data)["data"]["pH"] == nullptr) {
      (*json_data)["data"]["pH"].set(0);
      anyChange = true;
    }
    if (anyChange) {
      ExFile_WriteJsonFile(SD, path, *json_data);
    }
  }
};

#endif