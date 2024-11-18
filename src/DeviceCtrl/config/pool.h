#ifndef Config_Pool_H
#define Config_Pool_H

#include "common.h"

struct PoolConfig : ConfigSetting {
  explicit PoolConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    if ((*json_data).as<JsonArray>().size()==0) {
      for (int i = 1;i<5;i++) {
        JsonObject poolObj = (*json_data).createNestedObject();
        poolObj["title"].set("池"+String(i));
        poolObj["id"].set("pool-"+String(i));
      }
    }
    JsonArray checkItem = (*json_data).as<JsonArray>();
    for (int index=0;index<checkItem.size();index++) {
      if ((*json_data)[index].containsKey("title") == false) {(*json_data)[index]["title"].set("池"+String(index+1));}
      if ((*json_data)[index].containsKey("id") == false) {(*json_data)[index]["id"].set("pool-"+String(index+1));}
      if ((*json_data)[index].containsKey("external_mapping") == false) {(*json_data)[index]["external_mapping"].set("");}
      if ((*json_data)[index].containsKey("desp") == false) {(*json_data)[index]["desp"].set("-");}
      if ((*json_data)[index].containsKey("Used") == false) {(*json_data)[index]["Used"].set(0);}
    }
    ExFile_WriteJsonFile(SD, path, *json_data);
  }
};

#endif