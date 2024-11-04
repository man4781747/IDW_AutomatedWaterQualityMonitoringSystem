/**
 * @file ConfigSetting.h
 * @author Akria Chen
 * @brief 
 * @version 0.1
 * @date 2024-11-04
 * 
 * @copyright Copyright (c) 2024
 * 
 * 
 * 此程式定義了所有水質機所需的所有設定檔行為
 * 包含它們如何讀取、重建
 */

#ifndef CONFIG_SETTING_H
#define CONFIG_SETTING_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "StorgeSystemExternalFunction.h"
#include "TimeLibExternalFunction.h"

struct ConfigSetting
{
  explicit ConfigSetting(String filePath, uint16_t dataSize) {
    path = filePath;
    json_data = new DynamicJsonDocument(dataSize);
  };
  virtual void loadConfig(bool rebuild=false) {
    ExFile_LoadJsonFile(SD, path, *json_data);
  };
  virtual void writeConfig() {
    ExFile_WriteJsonFile(SD, path, *json_data);
  };
  virtual ~ConfigSetting() {
    json_data->clear();
  }
  String JsonFormatString(bool Pretty=false){
    String returnString;
    if (Pretty) {serializeJsonPretty(*json_data, returnString);}
    else {serializeJson(*json_data, returnString);}
    return returnString;
  };
  String path = "";
  DynamicJsonDocument *json_data;
};

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

struct WiFiConfig : ConfigSetting {
  explicit WiFiConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    //? 檢查各參數是否存在，不存在則使用預設值
    bool anyChange = false;
    if ((*json_data)["AP"]["AP_IP"] == nullptr) {
      (*json_data)["AP"]["AP_IP"].set("192.168.0.4");
      anyChange = true;
    }
    if ((*json_data)["AP"]["AP_gateway"] == nullptr) {
      (*json_data)["AP"]["AP_gateway"].set("192.168.0.1");
      anyChange = true;
    }
    if ((*json_data)["AP"]["AP_subnet_mask"] == nullptr) {
      (*json_data)["AP"]["AP_subnet_mask"].set("255.255.255.0");
      anyChange = true;
    }
    if ((*json_data)["AP"]["AP_Name"] == nullptr) {
      (*json_data)["AP"]["AP_Name"].set("ID_PoolSensor");
      anyChange = true;
    }
    if ((*json_data)["AP"]["AP_Password"] == nullptr) {
      (*json_data)["AP"]["AP_Password"].set("12345678");
      anyChange = true;
    }
    if ((*json_data)["Remote"]["remote_Name"] == nullptr) {
      (*json_data)["Remote"]["remote_Name"].set("IDWATER");
      anyChange = true;
    }
    if ((*json_data)["Remote"]["remote_Password"] == nullptr) {
      (*json_data)["Remote"]["remote_Password"].set("56651588");
      anyChange = true;
    }
    if ((*json_data)["Remote"]["checker"]["check"] == nullptr) {
      (*json_data)["Remote"]["checker"]["check"].set(false);
      anyChange = true;
    }
    if ((*json_data)["Remote"]["checker"]["check_IP"] == nullptr) {
      (*json_data)["Remote"]["checker"]["check_IP"].set("8.8.8.8");
      anyChange = true;
    }
    if (anyChange) {
      ExFile_WriteJsonFile(SD, path, *json_data);
    }
  }
};

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

struct PeristalticMotorConfig : ConfigSetting {
  explicit PeristalticMotorConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
    if ((*json_data).as<JsonArray>().size()==0) {
      JsonArray dataList = (*json_data).createNestedArray();
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
        JsonObject nerObj = dataList.createNestedObject();
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

struct ScheduleConfig : ConfigSetting {
  explicit ScheduleConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
      for (int i = 0;i<24;i++) {
        (*json_data).add("-");
      }
      ExFile_WriteJsonFile(SD, path, *json_data);
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
  }
};

struct ItemUseCountConfig : ConfigSetting {
  explicit ItemUseCountConfig(String filePath, uint16_t dataSize) :  ConfigSetting(filePath, dataSize){};
  void loadConfig(bool rebuild=false) override {
    if (rebuild) {
      (*json_data).clear();
      json_data->createNestedObject();
      ExFile_WriteJsonFile(SD, path, *json_data);
    } else {
      ExFile_LoadJsonFile(SD, path, *json_data);
    }
  }
};

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
    for (int i=0;i<5;i++) {
      if ((*json_data)[defaultConfig[i]]["time_check_switch"] == nullptr) {
        (*json_data)[defaultConfig[i]]["time_check_switch"].set(false);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["start_time"] == nullptr) {
        (*json_data)[defaultConfig[i]]["start_time"].set(GetDatetimeString());
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["time_check_days"] == nullptr) {
        (*json_data)[defaultConfig[i]]["day_alarm"].set(30);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["remaining_check_switch"] == nullptr) {
        (*json_data)[defaultConfig[i]]["remaining_check_switch"].set(false);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["remaining"] == nullptr) {
        (*json_data)[defaultConfig[i]]["remaining"].set(500);
        anyChange = true;
      }
      if ((*json_data)[defaultConfig[i]]["remaining_alarm"] == nullptr) {
        (*json_data)[defaultConfig[i]]["remaining_alarm"].set(10);
        anyChange = true;
      }
    }
    if (anyChange) {
      ExFile_WriteJsonFile(SD, path, *json_data);
    }
  }
};

#endif