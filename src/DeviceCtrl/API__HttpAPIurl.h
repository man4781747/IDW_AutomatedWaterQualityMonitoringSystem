#ifndef HTTP_API_H
#define HTTP_API_H

#include <ESPAsyncWebServer.h>
#include "MAIN__DeviceCtrl.h"
#include "SqliteFunctions.h"
#include "AsyncJson.h"
#include <SD.h>
#include <SPIFFS.h>
#include "StorgeSystemExternalFunction.h"

void Set_deviceConfigs_apis(AsyncWebServer &asyncServer);
void Set_scheduleConfig_apis(AsyncWebServer &asyncServer);

uint8_t *newConfigUpdateFileBuffer;
size_t newConfigUpdateFileBufferLen;

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


  asyncServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("API: /");
    AsyncWebServerResponse* response = request->beginResponse(SPIFFS, "/web/index.html", "text/html", false);
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
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "HI");
    request->send(response);
  });

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
        configTempFile = SD.open("/pipelines/"+filename, FILE_WRITE);
        configTempFile.write(newConfigUpdateFileBuffer ,index + len);
        configTempFile.close();
        Serial.printf("檔案更新完成\n", filename.c_str());
        
        Device_Ctrl.UpdatePipelineConfigList();
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
      db_exec_http(Device_Ctrl.DB_Main, SQL_String, &root);
      response->setLength();
      request->send(response);
    }
  );

  Set_deviceConfigs_apis(asyncServer);
  Set_scheduleConfig_apis(asyncServer);
}

//! 儀器設定檔相關API
void Set_deviceConfigs_apis(AsyncWebServer &asyncServer)
{
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
  
  asyncServer.on("/api/config/spectrophotometer_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__SpectrophotometerConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
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
  asyncServer.on("^\\/api\\/schedule\\/([a-zA-Z0-9_.-]+)$", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      AsyncWebServerResponse* response;
      String keyName = request->pathArg(0);
      if ((*Device_Ctrl.JSON__ScheduleConfig).containsKey(keyName)) {
        response = request->beginResponse(200, "application/json", (*Device_Ctrl.JSON__ScheduleConfig)[keyName]);
      } 
      else {
        response = request->beginResponse(500, "application/json", "{\"Result\":\"Can't Find: "+keyName+"\"}");
      }
      request->send(response);
    }
  );
  //? 接收前端上傳的設定更新
  //? FormData強制一定要有 Param: "content", 型態: String, 格式: JSON
  asyncServer.on("^\\/api\\/schedule\\/([a-zA-Z0-9_.-]+)$", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      AsyncWebServerResponse* response;
      String keyName = request->pathArg(0);
      if (request->hasArg("content")) {
        String content = request->getParam("content", true)->value();
        DynamicJsonDocument JSON__content(1000);
        DeserializationError error = deserializeJson(JSON__content, content);
        if (error) {
          ESP_LOGE("schedule更新", "JOSN解析失敗,停止更新排程設定檔內容", error.c_str());
          response = request->beginResponse(500, "application/json", "{\"Result\":\"更新失敗,content所需型態: String, 格式: JSON\"}");
        }
        else {
          (*Device_Ctrl.JSON__ScheduleConfig)[keyName].set(JSON__content);
          ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__ScheduleConfig, *Device_Ctrl.JSON__ScheduleConfig);
          // Device_Ctrl.SD__ReWriteScheduleConfig();
          response = request->beginResponse(200, "application/json", (*Device_Ctrl.JSON__ScheduleConfig)[keyName]);
        }
      }
      else {
        response = request->beginResponse(500, "application/json", "{\"Result\":\"缺少para: 'content',型態: String, 格式: JSON\"}");
      }
      request->send(response);
    }
  );

  asyncServer.on("^\\/api\\/schedule\\/([a-zA-Z0-9_.-]+)$", HTTP_DELETE,
    [&](AsyncWebServerRequest *request)
    { 
      AsyncWebServerResponse* response;
      String keyName = request->pathArg(0);
      if ((*Device_Ctrl.JSON__ScheduleConfig).containsKey(keyName)) {
        (*Device_Ctrl.JSON__ScheduleConfig).remove(keyName);
        ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__ScheduleConfig, *Device_Ctrl.JSON__ScheduleConfig);
        // Device_Ctrl.SD__ReWriteScheduleConfig();
        response = request->beginResponse(200, "application/json", "{\"Result\":\"刪除成功\"}");
      } 
      else {
        response = request->beginResponse(500, "application/json", "{\"Result\":\"Can't Find: "+keyName+"\"}");
      }
      request->send(response);
    }
  );
}

#endif