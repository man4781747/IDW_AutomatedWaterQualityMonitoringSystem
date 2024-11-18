#ifndef Config_ItemUseCount_H
#define Config_ItemUseCount_H

#include "common.h"

struct ItemUseCountConfig : ConfigSetting {
  explicit ItemUseCountConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    if ((*json_data).isNull()) {
      ESP_LOGW("CONFIG", "儀器運作計數為空，重建");
      File file = SD.open(path, FILE_WRITE);
      file.print("{}");
      file.close();
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
  }
};

#endif