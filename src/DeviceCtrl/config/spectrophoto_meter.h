#ifndef Config_SpectrophotoMeter_H
#define Config_SpectrophotoMeter_H

#include "common.h"

struct SpectrophotoMeterConfig : ConfigSetting {
  explicit SpectrophotoMeterConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }

    if ((*json_data).as<JsonArray>().size()==0) {
      JsonObject blueLightObj = (*json_data).createNestedObject();
      blueLightObj["title"].set("(藍光)氨氮檢測");
      JsonObject greenLightObj = (*json_data).createNestedObject();
      greenLightObj["title"].set("(綠光)亞硝酸鹽檢測");
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