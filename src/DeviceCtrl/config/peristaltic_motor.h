#ifndef Config_PeristalticMotor_H
#define Config_PeristalticMotor_H

#include "common.h"

struct PeristalticMotorConfig : ConfigSetting {
  explicit PeristalticMotorConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    if ((*json_data).as<JsonArray>().size()==0) {
      const char* defaultConfig[8][2] = {
        {"M1", "蝦池抽水"},
        {"M2", "RO水抽水"},
        {"M3", "光度測試/排廢水馬達"},
        {"M4", "NH4-R2試劑馬達"},
        {"目前無作用", "-"},
        {"目前無作用", "-"},
        {"目前無作用", "-"},
        {"目前無作用", "-"},
      };
      for (int i = 0;i<8;i++) {
        JsonObject nerObj = (*json_data).createNestedObject();
        nerObj["title"].set(defaultConfig[i][0]);
        nerObj["desp"].set(defaultConfig[i][1]);
      }
    }
    JsonArray checkItem = (*json_data).as<JsonArray>();
    for (int index=0;index<checkItem.size();index++) {
      if ((*json_data)[index].containsKey("title") == false) {(*json_data)[index]["title"].set("目前無作用");}
      if ((*json_data)[index].containsKey("desp") == false) {(*json_data)[index]["desp"].set("-");}
    }
    ExFile_WriteJsonFile(SD, path, *json_data);
  }
};

#endif