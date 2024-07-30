#ifndef HTTP_API_H
#define HTTP_API_H

#include <ESPAsyncWebServer.h>
#include "MAIN__DeviceCtrl.h"
#include "SqliteFunctions.h"
#include "AsyncJson.h"
#include <SD.h>
#include "StorgeSystemExternalFunction.h"
#include <Update.h>

void Set_deviceConfigs_apis(AsyncWebServer &asyncServer);
void Set_scheduleConfig_apis(AsyncWebServer &asyncServer);
void Set_tool_apis(AsyncWebServer &asyncServer);
void Set_Pipeline_apis(AsyncWebServer &asyncServer);

uint8_t *newConfigUpdateFileBuffer;
size_t newConfigUpdateFileBufferLen;

uint8_t *newWebUpdateFileBuffer;
size_t newWebUpdateFileBufferLen;
uint8_t *newFirmwareUpdateFileBuffer;
size_t newFirmwareUpdateFileBufferLen;

void Set_Http_apis(AsyncWebServer &asyncServer)
{
  //! CORS 檢查用
  asyncServer.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404);
    }
  });
  asyncServer.serveStatic("/static/SD/",SD,"/");

  asyncServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("API: /");
    AsyncWebServerResponse* response = request->beginResponse(SD, "/web/index.html", "text/html", false);
    request->send(response);
  });

  asyncServer.on("^\\/assets\\/index\\.([a-zA-Z0-9_.-]+)\\.(css|js)$", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/javascript", [](uint8_t *buffer, size_t maxLen,size_t filledLength) -> size_t
    {
      // ESP_LOGD("beginChunkedResponse", "maxLen: %d, filledLength: %d", maxLen, filledLength);
      size_t len = min(maxLen, Device_Ctrl.webJS_BufferLen - filledLength);
      memcpy(buffer, Device_Ctrl.webJS_Buffer + filledLength, len);
      return len;
    });                                                                    
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public, max-age=691200");
    request->send(response);
  });

  asyncServer.on("/api/hi", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", FIRMWARE_VERSION);
    request->send(response);
  });

  asyncServer.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request){
    String returnString = String("{\"FIRMWARE_VERSION\":\"")+String(FIRMWARE_VERSION)+"\"}";
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
    request->send(response);
  });


  asyncServer.on("/api/sensor", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      DynamicJsonDocument ReturnData(100000);
      String StartTime = "1900-01-01 00:00:00";
      String EndTime = "2900-01-01 00:00:00";
      String PoolList = "'pool-1','pool-2','pool-3','pool-4'";
      String ValueNameList = "'pH','NO2','NH4'";
      if (request->hasParam("st")) {
        StartTime = request->getParam("st")->value();
      }
      if (request->hasParam("et")) {
        EndTime = request->getParam("et")->value();
      }
      if (request->hasParam("pl")) {
        PoolList = request->getParam("pl")->value();
      }
      if (request->hasParam("name")) {
        ValueNameList = request->getParam("name")->value();
      }

      String SQL_String = "SELECT * FROM sensor WHERE pool in (";
      SQL_String += PoolList;
      SQL_String += ") AND value_name in (";
      SQL_String += ValueNameList;
      SQL_String += ") AND time BETWEEN '";
      SQL_String += StartTime;
      SQL_String += "' AND '";
      SQL_String += EndTime;
      SQL_String += "' ORDER BY rowid DESC LIMIT 100";
      AsyncJsonResponse *response = new AsyncJsonResponse(true, 65525);
      JsonArray root = response->getRoot();
      db_exec_http(Device_Ctrl.DB_Main, SQL_String, &root);
      response->setLength();
      request->send(response);
    }
  );

  asyncServer.on("/api/DB/Rebuild", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      sqlite3_close(Device_Ctrl.DB_Main);
      SD.remove("/mainDB.db");
      int rc = sqlite3_open(Device_Ctrl.FilePath__SD__MainDB.c_str(), &Device_Ctrl.DB_Main);
      db_exec(Device_Ctrl.DB_Main, "CREATE TABLE sensor ( time TEXT, pool TEXT , value_name TEXT , result REAL );");
      AsyncWebServerResponse* response = request->beginResponse(200, "OK");
      request->send(response);
    }
  );

  asyncServer.on("/api/logs", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String StartTime = "1900-01-01 00:00:00";
      String EndTime = "2900-01-01 00:00:00";
      String LevelList = "1,2,3,4,5,6";
      if (request->hasParam("st")) {
        StartTime = request->getParam("st")->value();
      }
      if (request->hasParam("et")) {
        EndTime = request->getParam("et")->value();
      }
      if (request->hasParam("lv")) {
        LevelList = request->getParam("lv")->value();
      }
      String SQL_String = "SELECT * FROM logs WHERE level in (";
      SQL_String += LevelList;
      SQL_String += ") AND time BETWEEN '";
      SQL_String += StartTime;
      SQL_String += "' AND '";
      SQL_String += EndTime;
      SQL_String += "' ORDER BY rowid DESC LIMIT 100";
      AsyncJsonResponse *response = new AsyncJsonResponse(true, 100000);
      JsonArray root = response->getRoot();
      db_exec_http(Device_Ctrl.DB_Log, SQL_String, &root);
      response->setLength();
      request->send(response);
    }
  );

  asyncServer.on("/api/logs", HTTP_DELETE,
    [&](AsyncWebServerRequest *request)
    { 
      Device_Ctrl.DropLogsTable();
      AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "OK");
      request->send(response);
    }
  );

  Set_deviceConfigs_apis(asyncServer);
  Set_scheduleConfig_apis(asyncServer);
  Set_tool_apis(asyncServer);
  Set_Pipeline_apis(asyncServer);
}

//! Pipeline檔案相關API
void Set_Pipeline_apis(AsyncWebServer &asyncServer)
{
  asyncServer.on("/api/piplines", HTTP_GET, [&](AsyncWebServerRequest *request){
    String pipelineFilesList;
    serializeJsonPretty(*Device_Ctrl.JSON__PipelineConfigList, pipelineFilesList);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", pipelineFilesList);
    request->send(response);
  });

  asyncServer.on("/api/pipeline/config", HTTP_POST, 
    [&](AsyncWebServerRequest *request)
    { 
      free(newConfigUpdateFileBuffer);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"Result\":\"OK\"}");
      request->send(response);
    },
    [&](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
      if (index == 0) {
        newConfigUpdateFileBuffer = (uint8_t *)malloc(request->contentLength());
      }
      memcpy(newConfigUpdateFileBuffer + index, data, len);
      if (final) {
        Serial.printf("檔案 %s 接收完成， len: %d ，共 %d/%d bytes\n", filename.c_str(), len ,index + len, request->contentLength());
        newConfigUpdateFileBufferLen = index + len;
        if (!SD.exists("/pipelines")) {
          SD.mkdir("/pipelines");
        }
        File configTempFile;
        String filePath = "/pipelines/"+filename;
        configTempFile = SD.open(filePath, FILE_WRITE);
        configTempFile.write(newConfigUpdateFileBuffer ,index + len);
        configTempFile.close();
        Serial.printf("檔案更新完成\n", filename.c_str());
        Device_Ctrl.UpdateOnePipelineConfig(filename);
      } 
      else {
        Serial.printf("檔案 %s 正在傳輸， len: %d ，目前已接收 %d/%d bytes\n", filename.c_str(), len, index + len, request->contentLength());
      }
    }
  );

  asyncServer.on("^\\/api\\/pipeline\\/([a-zA-Z0-9_.-]+)\\.json$", HTTP_GET,[&](AsyncWebServerRequest *request){ 
    String fileName = request->pathArg(0);
    String fullPath = "/pipelines/"+fileName+".json";
    AsyncWebServerResponse* response;
    if (!SD.exists(fullPath)) {
      response = request->beginResponse(500, "application/json", "{\"Result\":\"Can't Find: "+fileName+"\"}");
    }
    else {

      File FileChose = SD.open(fullPath, "r");
      String ContentString = FileChose.readString();
      FileChose.close();       
      response = request->beginResponse(200, "application/json", ContentString);       
    }
    request->send(response);
  });

  asyncServer.on("^\\/api\\/pipeline\\/([a-zA-Z0-9_.-]+)\\.json$", HTTP_DELETE,[&](AsyncWebServerRequest *request){ 
    String fileName = request->pathArg(0);
    String fullPath = "/pipelines/"+fileName+".json";
    AsyncWebServerResponse* response;
    if (!SD.exists(fullPath)) {
      response = request->beginResponse(500, "application/json", "{\"Result\":\"Can't Find: "+fileName+"\"}");
    }
    else {
      // SD.remove(fullPath);
      Device_Ctrl.RemovePipelineConfig(fileName+".json");
      // Device_Ctrl.UpdatePipelineConfigList();
      response = request->beginResponse(200, "application/json", "{\"Result\":\"Delete file: "+fullPath+"\"}");       
    }
    request->send(response);
  });

}

//! 儀器設定檔相關API
void Set_deviceConfigs_apis(AsyncWebServer &asyncServer)
{
  asyncServer.on("/api/config/device_base_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__DeviceBaseInfo), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  asyncServer.on("/api/config/device_base_config", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("device_no")) {
        String NewDeviceNo = request->getParam("device_no")->value();
        (*Device_Ctrl.JSON__DeviceBaseInfo)["device_no"].set(NewDeviceNo);
      }
      if (request->hasParam("LINE_Notify_id")) {
        String LINE_Notify_id = request->getParam("LINE_Notify_id")->value();
        String EncodeKey = Device_Ctrl.AES_encode(LINE_Notify_id);
        (*Device_Ctrl.JSON__DeviceBaseInfo)["LINE_Notify_id"] .set(EncodeKey);
      }
      if (request->hasParam("LINE_Notify_switch")) {
        bool LINE_Notify_switch = request->getParam("LINE_Notify_switch")->value()=="true"?true:false;
        (*Device_Ctrl.JSON__DeviceBaseInfo)["LINE_Notify_switch"].set(LINE_Notify_switch);
      }
      if (request->hasParam("Mail_Notify_switch")) {
        bool Mail_Notify_switch = request->getParam("Mail_Notify_switch")->value()=="true"?true:false;
        (*Device_Ctrl.JSON__DeviceBaseInfo)["Mail_Notify_switch"].set(Mail_Notify_switch);
      }
      if (request->hasParam("Mail_Notify_Auther")) {
        String Mail_Notify_Auther = request->getParam("Mail_Notify_Auther")->value();
        (*Device_Ctrl.JSON__DeviceBaseInfo)["Mail_Notify_Auther"].set(Mail_Notify_Auther);
      }
      if (request->hasParam("Mail_Notify_Key")) {
        String Mail_Notify_Key = request->getParam("Mail_Notify_Key")->value();
        String EncodeKey = Device_Ctrl.AES_encode(Mail_Notify_Key);
        (*Device_Ctrl.JSON__DeviceBaseInfo)["Mail_Notify_Key"].set(EncodeKey);
      }
      if (request->hasParam("Mail_Notify_Target")) {
        String Mail_Notify_Target = request->getParam("Mail_Notify_Target")->value();
        (*Device_Ctrl.JSON__DeviceBaseInfo)["Mail_Notify_Target"].set(Mail_Notify_Target);
      }

      ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__DeviceBaseInfo, (*Device_Ctrl.JSON__DeviceBaseInfo));
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__DeviceBaseInfo), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );


  asyncServer.on("/api/config/device_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__DeviceConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );

  asyncServer.on("/api/config/peristaltic_motor_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__PeristalticMotorConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );

  asyncServer.on("/api/config/pwm_motor_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__ServoConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );

  asyncServer.on("/api/config/pool_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__PoolConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  
  asyncServer.on("/api/config/pool_config", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("content")) {
        String NewContent = request->getParam("content")->value();
        DynamicJsonDocument tempJSONItem(2048);
        DeserializationError error = deserializeJson(tempJSONItem, NewContent);
        if (error) {
          ESP_LOGE("schedule更新", "JOSN解析失敗,停止更新蝦池設定檔內容", error.c_str());
          AsyncWebServerResponse* response = request->beginResponse(500, "application/json", "{\"Result\":\"更新失敗,content所需型態: String, 格式: JSON\"}");
          request->send(response);
        }
        else {
          (*Device_Ctrl.JSON__PoolConfig).clear();
          (*Device_Ctrl.JSON__PoolConfig).set(tempJSONItem);
          ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__PoolConfig, (*Device_Ctrl.JSON__PoolConfig));
          AsyncWebServerResponse* response = request->beginResponse(200, "application/json", NewContent);
          request->send(response);
        }
      }
      else {
        AsyncWebServerResponse* response = request->beginResponse(400, "application/json", "{\"message\":\"需要content\"}");
        request->send(response);
      }
    }
  );


  asyncServer.on("/api/config/spectrophotometer_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__SpectrophotometerConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );

  asyncServer.on("/api/config/spectrophotometer_config", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("index")) {
        int SettingIndex = request->getParam("index")->value().toInt();
        if (request->hasParam("title")) {
          (*Device_Ctrl.JSON__SpectrophotometerConfig)[SettingIndex]["title"].set(request->getParam("title")->value());
        }
        if (request->hasParam("desp")) {
          (*Device_Ctrl.JSON__SpectrophotometerConfig)[SettingIndex]["desp"].set(request->getParam("desp")->value());
        }
        if (request->hasParam("m")) {
          (*Device_Ctrl.JSON__SpectrophotometerConfig)[SettingIndex]["m"].set(request->getParam("m")->value().toDouble());
        }
        if (request->hasParam("b")) {
          (*Device_Ctrl.JSON__SpectrophotometerConfig)[SettingIndex]["b"].set(request->getParam("b")->value().toDouble());
        }
        ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__SpectrophotometerConfig, (*Device_Ctrl.JSON__SpectrophotometerConfig));

        DynamicJsonDocument ReturnData(10000);
        ReturnData["new"].set((*Device_Ctrl.JSON__SpectrophotometerConfig)[SettingIndex]);
        ReturnData["index"].set(SettingIndex);
        String RetuenString;
        serializeJson(ReturnData, RetuenString);
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
        request->send(response);
      }
      else {
        AsyncWebServerResponse* response = request->beginResponse(400, "application/json", "{\"message\":\"需要index\"}");
        request->send(response);
      }
    }
  );

  asyncServer.on("/api/config/PHmeter_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__PHmeterConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );

  asyncServer.on("/api/config/PHmeter_config", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("index")) {
        int SettingIndex = request->getParam("index")->value().toInt();
        if (request->hasParam("title")) {
          (*Device_Ctrl.JSON__PHmeterConfig)[SettingIndex]["title"].set(request->getParam("title")->value());
        }
        if (request->hasParam("desp")) {
          (*Device_Ctrl.JSON__PHmeterConfig)[SettingIndex]["desp"].set(request->getParam("desp")->value());
        }
        if (request->hasParam("m")) {
          (*Device_Ctrl.JSON__PHmeterConfig)[SettingIndex]["m"].set(request->getParam("m")->value().toDouble());
        }
        if (request->hasParam("b")) {
          (*Device_Ctrl.JSON__PHmeterConfig)[SettingIndex]["b"].set(request->getParam("b")->value().toDouble());
        }
        ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__PHmeterConfig, (*Device_Ctrl.JSON__PHmeterConfig));

        DynamicJsonDocument ReturnData(10000);
        ReturnData["new"].set((*Device_Ctrl.JSON__PHmeterConfig)[SettingIndex]);
        ReturnData["index"].set(SettingIndex);
        String RetuenString;
        serializeJson(ReturnData, RetuenString);
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
        request->send(response);
      }
      else {
        AsyncWebServerResponse* response = request->beginResponse(400, "application/json", "{\"message\":\"需要index\"}");
        request->send(response);
      }
    }
  );

  asyncServer.on("/api/config/wifi_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__WifiConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
      
    }
  );

  asyncServer.on("/api/config/wifi_config", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("STA_Name")) {
        (*Device_Ctrl.JSON__WifiConfig)["Remote"]["remote_Name"].set(request->getParam("STA_Name")->value());
      }
      if (request->hasParam("STA_Password")) {
        (*Device_Ctrl.JSON__WifiConfig)["Remote"]["remote_Password"].set(request->getParam("STA_Password")->value());
      }
      ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__WiFiConfig, (*Device_Ctrl.JSON__WifiConfig));
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__WifiConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
      Device_Ctrl.ConnectWiFi();
    }
  );
}

//! 排程相關API
void Set_scheduleConfig_apis(AsyncWebServer &asyncServer)
{
  asyncServer.on("/api/schedule", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson(*Device_Ctrl.JSON__ScheduleConfig, RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );


  asyncServer.on("/api/schedule", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      AsyncWebServerResponse* response;
      String keyName = request->pathArg(0);
      if (request->hasArg("index")) {
        if (request->hasArg("name")) {
          int index = String(request->getParam("index")->value()).toInt();
          String name = request->getParam("name")->value();
          (*Device_Ctrl.JSON__ScheduleConfig)[index].set(name);
          ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__ScheduleConfig, *Device_Ctrl.JSON__ScheduleConfig);
          DynamicJsonDocument returnJSON(1000);
          returnJSON["index"] = index;
          returnJSON["name"] = name;
          String returnString;
          serializeJson(returnJSON, returnString);
          response = request->beginResponse(200, "application/json", returnString);
        }
        else {
          response = request->beginResponse(500, "application/json", "{\"Result\":\"缺少para: 'name'\"}");
        }
      }
      else {
        response = request->beginResponse(500, "application/json", "{\"Result\":\"缺少para: 'index'\"}");
      }
      request->send(response);
    }
  );






  // asyncServer.on("^\\/api\\/schedule\\/([a-zA-Z0-9_.-]+)$", HTTP_GET,
  //   [&](AsyncWebServerRequest *request)
  //   { 
  //     AsyncWebServerResponse* response;
  //     String keyName = request->pathArg(0);
  //     if ((*Device_Ctrl.JSON__ScheduleConfig).containsKey(keyName)) {
  //       response = request->beginResponse(200, "application/json", (*Device_Ctrl.JSON__ScheduleConfig)[keyName]);
  //     } 
  //     else {
  //       response = request->beginResponse(500, "application/json", "{\"Result\":\"Can't Find: "+keyName+"\"}");
  //     }
  //     request->send(response);
  //   }
  // );
  // //? 接收前端上傳的設定更新
  // //? FormData強制一定要有 Param: "content", 型態: String, 格式: JSON
  // asyncServer.on("^\\/api\\/schedule\\/([a-zA-Z0-9_.-]+)$", HTTP_PATCH,
  //   [&](AsyncWebServerRequest *request)
  //   { 
  //     AsyncWebServerResponse* response;
  //     String keyName = request->pathArg(0);
  //     if (request->hasArg("content")) {
  //       String content = request->getParam("content", true)->value();
  //       DynamicJsonDocument JSON__content(1000);
  //       DeserializationError error = deserializeJson(JSON__content, content);
  //       if (error) {
  //         ESP_LOGE("schedule更新", "JOSN解析失敗,停止更新排程設定檔內容", error.c_str());
  //         response = request->beginResponse(500, "application/json", "{\"Result\":\"更新失敗,content所需型態: String, 格式: JSON\"}");
  //       }
  //       else {
  //         (*Device_Ctrl.JSON__ScheduleConfig)[keyName].set(JSON__content);
  //         ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__ScheduleConfig, *Device_Ctrl.JSON__ScheduleConfig);
  //         // Device_Ctrl.SD__ReWriteScheduleConfig();
  //         response = request->beginResponse(200, "application/json", (*Device_Ctrl.JSON__ScheduleConfig)[keyName]);
  //       }
  //     }
  //     else {
  //       response = request->beginResponse(500, "application/json", "{\"Result\":\"缺少para: 'content',型態: String, 格式: JSON\"}");
  //     }
  //     request->send(response);
  //   }
  // );

  // asyncServer.on("^\\/api\\/schedule\\/([a-zA-Z0-9_.-]+)$", HTTP_DELETE,
  //   [&](AsyncWebServerRequest *request)
  //   { 
  //     AsyncWebServerResponse* response;
  //     String keyName = request->pathArg(0);
  //     if ((*Device_Ctrl.JSON__ScheduleConfig).containsKey(keyName)) {
  //       (*Device_Ctrl.JSON__ScheduleConfig).remove(keyName);
  //       ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__ScheduleConfig, *Device_Ctrl.JSON__ScheduleConfig);
  //       // Device_Ctrl.SD__ReWriteScheduleConfig();
  //       response = request->beginResponse(200, "application/json", "{\"Result\":\"刪除成功\"}");
  //     } 
  //     else {
  //       response = request->beginResponse(500, "application/json", "{\"Result\":\"Can't Find: "+keyName+"\"}");
  //     }
  //     request->send(response);
  //   }
  // );
}

void Set_tool_apis(AsyncWebServer &asyncServer)
{
  asyncServer.on("/api/RO/Result", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("NO2")) {
        double NO2_Value = request->getParam("NO2")->value().toDouble();
        (*Device_Ctrl.JSON__RO_Result)["NO2"].set(NO2_Value);
        ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__RO_Result, *Device_Ctrl.JSON__RO_Result);
      }
      if (request->hasParam("NH4")) {
        double NH4_Value = request->getParam("NH4")->value().toDouble();
        (*Device_Ctrl.JSON__RO_Result)["NH4"].set(NH4_Value);
        ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__RO_Result, *Device_Ctrl.JSON__RO_Result);
      }
      String returnString;
      serializeJson(*Device_Ctrl.JSON__RO_Result, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );
  asyncServer.on("/api/device/used", HTTP_DELETE,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("name")) {
        String resetName = request->getParam("name")->value();
        if ((*Device_Ctrl.JSON__ItemUseCount)[resetName] == nullptr) {
          AsyncWebServerResponse* response = request->beginResponse(500, "application/json", "{\"Result\":\"找不到 name: "+resetName+"\"}");
          request->send(response);
        } else {
          (*Device_Ctrl.JSON__ItemUseCount)[resetName] = 0;
          ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__ItemUseCount, *Device_Ctrl.JSON__ItemUseCount);
          String returnString;
          serializeJson(*Device_Ctrl.JSON__ItemUseCount, returnString);
          AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
          request->send(response);
        }
      } 
      else {
        AsyncWebServerResponse* response = request->beginResponse(500, "application/json", "{\"Result\":\"需要參數: name\"}");
        request->send(response);
      }
    }
  );

  asyncServer.on("/api/device/used", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String returnString;
      serializeJson(*Device_Ctrl.JSON__ItemUseCount, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );

  asyncServer.on("/api/SD/info", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      DynamicJsonDocument returnJSON(1000);
      returnJSON["Size"] = SD.cardSize();
      returnJSON["Type"] = (int)SD.cardType();
      returnJSON["Used"] = SD.usedBytes();
      String RetuenString;
      serializeJson(returnJSON, RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  asyncServer.on("/api/test/line_notify", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      AsyncWebServerResponse* response;
      if (Device_Ctrl.AddLineNotifyEvent("手動測試")) {
        response = request->beginResponse(200, "application/json", "{\"message\":\"OK\"}");
      } else {
        response = request->beginResponse(200, "application/json", "{\"message\":\"FAIL\"}");
      }
      request->send(response);
    }
  );

  asyncServer.on("/api/test/mail_notify", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      AsyncWebServerResponse* response;
      if (Device_Ctrl.AddGmailNotifyEvent("手動測試","手動測試")) {
        response = request->beginResponse(200, "application/json", "{\"message\":\"OK\"}");
      } else {
        response = request->beginResponse(200, "application/json", "{\"message\":\"FAIL\"}");
      }
      request->send(response);
    }
  );

  //! 獲得當前Websocket所有連線單位的狀態
  asyncServer.on("/api/wifi/websocket", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String returnString;
      serializeJson(Device_Ctrl.GetWebsocketConnectInfo(), returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );

  asyncServer.on("/api/firmware", HTTP_POST, 
    [&](AsyncWebServerRequest *request)
    { 
      free(newFirmwareUpdateFileBuffer);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"Result\":\"OK\"}");
      request->send(response);
      vTaskDelay(500/portTICK_PERIOD_MS);
      ESP.restart();
    },
    [&](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
      String filePath = "/firmware/firmware.bin";
      if (index == 0) {
        newFirmwareUpdateFileBuffer = (uint8_t *)malloc(request->contentLength());
      }
      memcpy(newFirmwareUpdateFileBuffer + index, data, len);
      if (final) {
        Serial.printf("檔案 %s 接收完成， len: %d ，共 %d/%d bytes\n", filename.c_str(), len ,index + len, request->contentLength());
        newFirmwareUpdateFileBufferLen = index + len;
        if (!SD.exists("/firmware")) {
          SD.mkdir("/firmware");
        }
        String filePath = "/firmware/firmware.bin";
        ExFile_CreateFile(SD, filePath);
        File configTempFile;
        configTempFile = SD.open(filePath, FILE_WRITE);
        configTempFile.write(newFirmwareUpdateFileBuffer ,index + len);
        configTempFile.close();
        Serial.printf("檔案更新完成\n", filename.c_str());
      } 
      else {
        Serial.printf("檔案 %s 正在傳輸， len: %d ，目前已接收 %d/%d bytes\n", filename.c_str(), len, index + len, request->contentLength());
      }
    }
  );

  asyncServer.on("/api/web", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", "<html><body><div><div>HTML update:</div><input type='file' id='new-html-input' /><button onclick='uploadNew(`html`)'>upload</button></div><div><div>JS update:</div><input type='file' id='new-js-input' /><button onclick='uploadNew(`js`)'>upload</button></div><div><div>Firmware update:</div><input type='file' id='new-bin-input' /><button onclick='uploadNewBin()'>upload</button></div><script>function uploadNew(type) {const fileInput = document.getElementById('new-' + type + '-input');const file = fileInput.files[0];const reader = new FileReader();reader.onload = function (event) {const fileContent = event.target.result;const fileBlob = new Blob([fileContent], { type: file.type });var formData = new FormData();formData.append('file', fileBlob, file.name);fetch('web?type=' + type, { method: 'POST', body: formData }).then((response) => response.json()).then((data) => {console.log('Response from server:', data);}).catch((error) => {console.error('Error:', error);});};reader.readAsArrayBuffer(file);}function uploadNewBin() {const fileInput = document.getElementById('new-bin-input');const file = fileInput.files[0];const reader = new FileReader();reader.onload = function (event) {const fileContent = event.target.result;const fileBlob = new Blob([fileContent], { type: file.type });var formData = new FormData();formData.append('file', fileBlob, file.name);fetch('firmware' , { method: 'POST', body: formData }).then((response) => response.json()).then((data) => {console.log('Response from server:', data);}).catch((error) => {console.error('Error:', error);});};reader.readAsArrayBuffer(file);}</script></body></html>");
    request->send(response);
  });

  asyncServer.on("/api/web", HTTP_POST, 
    [&](AsyncWebServerRequest *request)
    { 
      free(newWebUpdateFileBuffer);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"Result\":\"OK\"}");
      request->send(response);
    },
    [&](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
      if (index == 0) {
        newWebUpdateFileBuffer = (uint8_t *)malloc(request->contentLength());
      }
      memcpy(newWebUpdateFileBuffer + index, data, len);
      if (final) {
        Serial.printf("檔案 %s 接收完成， len: %d ，共 %d/%d bytes\n", filename.c_str(), len ,index + len, request->contentLength());
        newWebUpdateFileBufferLen = index + len;
        if (!SD.exists("/pipelines")) {
          SD.mkdir("/pipelines");
        }
        String fileType = request->getParam("type")->value();
        String filePath;
        if (fileType == "html") {
          filePath = "/web/index.html";
        } else if (fileType == "js"){
          filePath = "/assets/index.js.gz";
        }
        ExFile_CreateFile(SD, filePath);
        File configTempFile;
        configTempFile = SD.open(filePath, FILE_WRITE);
        configTempFile.write(newWebUpdateFileBuffer ,index + len);
        configTempFile.close();
        Serial.printf("檔案更新完成\n", filename.c_str());
        
        if (fileType == "js"){
          Device_Ctrl.preLoadWebJSFile();
        }
      } 
      else {
        Serial.printf("檔案 %s 正在傳輸， len: %d ，目前已接收 %d/%d bytes\n", filename.c_str(), len, index + len, request->contentLength());
      }
    }
  );


}

#endif