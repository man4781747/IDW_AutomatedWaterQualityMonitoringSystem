#ifndef Config_Servo_H
#define Config_Servo_H

#include "common.h"

struct ServoConfig : ConfigSetting {
  explicit ServoConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    if ((*json_data).as<JsonArray>().size()==0) {
      // JsonArray dataList = (*json_data).createNestedArray();
      const char* defaultConfig[12][2] = {
        {"目前無作用", "-"},
        {"1", "藍綠光檢測室選擇"},
        {"2", "PH檢測室選擇"},
        {"3", "氨氮R1"},
        {"4", "亞硝R1"},
        {"5", "待測物"},
        {"6", "廢液口"},
        {"7", "乾淨水"},
        {"8", "第一入水口"},
        {"9", "第二入水口"},
        {"10", "第三入水口"},
        {"11", "第四入水口"}
      };
      for (int i = 0;i<12;i++) {
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