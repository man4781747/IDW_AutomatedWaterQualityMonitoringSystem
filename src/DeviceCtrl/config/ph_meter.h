#ifndef Config_pH_Meter_H
#define Config_pH_Meter_H

#include "common.h"

struct pH_MeterConfig : ConfigSetting {
  explicit pH_MeterConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    if ((*json_data).as<JsonArray>().size()==0) {
      JsonObject phMeterObj = (*json_data).createNestedObject();
      phMeterObj["title"].set("PH感測器");
    }
    JsonArray checkItem = (*json_data).as<JsonArray>();
    for (int index=0;index<checkItem.size();index++) {
      if ((*json_data)[index].containsKey("title") == false) {(*json_data)[index]["title"].set("無設定名稱");}
      if ((*json_data)[index].containsKey("desp") == false) {(*json_data)[index]["desp"].set("-");}
      if ((*json_data)[index].containsKey("m") == false) {(*json_data)[index]["m"].set(1);}
      if ((*json_data)[index].containsKey("b") == false) {(*json_data)[index]["b"].set(0);}
    }
    ExFile_WriteJsonFile(SD, path, *json_data);
  }
};

#endif