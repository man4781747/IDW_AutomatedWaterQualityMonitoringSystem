#ifndef HTTP_API_H
#define HTTP_API_H

#include <ESPAsyncWebServer.h>
#include "MAIN__DeviceCtrl.h"
#include "SqliteFunctions.h"
#include "AsyncJson.h"
#include <SD.h>
#include "StorgeSystemExternalFunction.h"
#include <Update.h>
#include <TimeLib.h>
#include "TimeLibExternalFunction.h"
#include "ExternalFunctions.h"
#include "esp_task_wdt.h"

void Set_deviceConfigs_apis(AsyncWebServer &asyncServer);
void Set_scheduleConfig_apis(AsyncWebServer &asyncServer);
void Set_tool_apis(AsyncWebServer &asyncServer);
void Set_Pipeline_apis(AsyncWebServer &asyncServer);
void Set_test_apis(AsyncWebServer &asyncServer);
void Set_DB_apis(AsyncWebServer &asyncServer);

uint8_t *newConfigUpdateFileBuffer;
size_t newConfigUpdateFileBufferLen;

uint8_t *newWebUpdateFileBuffer;
size_t newWebUpdateFileBufferLen;
uint8_t *newFirmwareUpdateFileBuffer;
size_t newFirmwareUpdateFileBufferLen;

int UploadPer = 0;

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

  //? 設定 SD 卡的靜態檔案 API
  asyncServer.serveStatic("/static/SD/",SD,"/");

  //? 控制網頁 API
  asyncServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("API: /");
    AsyncWebServerResponse* response = request->beginResponse(SD, "/web/index.html", "text/html", false);
    request->send(response);
  });

  //? 控制網頁的 js.gz 檔案 API 位置
  //! 因 .js.gz 大小很大，並且需要指定以 Content-Encoding : gzip 來回應
  //! 因此目前機台做法為開機/更新網頁時，會將整份檔案資料存在記憶體中，這樣在回應時就不會鎖住 SD 卡
  //! 回覆就從記憶體直接回覆
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

  //? HI API
  asyncServer.on("/api/hi", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", FIRMWARE_VERSION);
    request->send(response);
  });

  //? 韌體版本 API
  asyncServer.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request){
    String returnString = String("{\"FIRMWARE_VERSION\":\"")+String(FIRMWARE_VERSION)+"\"}";
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
    request->send(response);
  });

  Set_deviceConfigs_apis(asyncServer);
  Set_scheduleConfig_apis(asyncServer);
  Set_tool_apis(asyncServer);
  Set_Pipeline_apis(asyncServer);
  Set_test_apis(asyncServer);
  Set_DB_apis(asyncServer);
}

//! Pipeline檔案相關API
void Set_Pipeline_apis(AsyncWebServer &asyncServer)
{
  // asyncServer.on("/api/piplines/run", HTTP_GET, [&](AsyncWebServerRequest *request){

  //   if (request->hasParam("order")) {
  //     String orderString = request->getParam("order")->value();
  //     DynamicJsonDocument NewPipelineSetting(60000);
  //     int splitIndex = -1;
  //     int prevSpliIndex = 0;
  //     int eventCount = 0;
  //     splitIndex = orderString.indexOf(',', prevSpliIndex);
  //     while (splitIndex != -1) {
  //       String token = orderString.substring(prevSpliIndex, splitIndex);
  //       String TargetName = token+".json";
  //       String FullFilePath = "/pipelines/"+TargetName;
  //       DynamicJsonDocument singlePipelineSetting(10000);
  //       singlePipelineSetting["FullFilePath"].set(FullFilePath);
  //       singlePipelineSetting["TargetName"].set(TargetName);
  //       singlePipelineSetting["stepChose"].set("");
  //       singlePipelineSetting["eventChose"].set("");
  //       singlePipelineSetting["eventIndexChose"].set(-1);
  //       // (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
  //       NewPipelineSetting.add(singlePipelineSetting);
  //       prevSpliIndex = splitIndex +1;
  //       eventCount++;
  //       splitIndex = orderString.indexOf(',', prevSpliIndex);
  //       ESP_LOGD("WebSocket", " - 事件 %d", eventCount);
  //       ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
  //       ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
  //     }
  //     String lastToken = orderString.substring(prevSpliIndex);
  //     if (lastToken.length() > 0) {
  //       String TargetName = lastToken+".json";
  //       String FullFilePath = "/pipelines/"+TargetName;
  //       DynamicJsonDocument singlePipelineSetting(3000);
  //       singlePipelineSetting["FullFilePath"].set(FullFilePath);
  //       singlePipelineSetting["TargetName"].set(TargetName);
  //       singlePipelineSetting["stepChose"].set("");
  //       singlePipelineSetting["eventChose"].set("");
  //       singlePipelineSetting["eventIndexChose"].set(-1);
  //       // (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
  //       NewPipelineSetting.add(singlePipelineSetting);
  //       ESP_LOGD("WebSocket", " - 事件 %d", eventCount+1);
  //       ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
  //       ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
  //     }
  //     if (!RunNewPipeline(NewPipelineSetting)) {
  //       Device_Ctrl.BroadcastLogToClient(client,1,"儀器忙碌中，請稍後再試");
  //     }
  //     else {

  //     }
  //   }
  //   else {
  //     AsyncWebServerResponse* response = request->beginResponse(400, "application/json", "缺乏必要參數: order");
  //     request->send(response);
  //   }



  //   String pipelineFilesList;
  //   serializeJsonPretty(*Device_Ctrl.JSON__PipelineConfigList, pipelineFilesList);
  //   AsyncWebServerResponse* response = request->beginResponse(200, "application/json", pipelineFilesList);
  //   request->send(response);
  // });

  //? pipeline 列表 API
  asyncServer.on("/api/piplines", HTTP_GET, [&](AsyncWebServerRequest *request){
    String pipelineFilesList;
    serializeJsonPretty(*Device_Ctrl.JSON__PipelineConfigList, pipelineFilesList);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", pipelineFilesList);
    request->send(response);
  });
  //? 單一 pipeline 檔案上傳/更新 API
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
  //? 單一 pipeline 檔案讀取 API
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
  //? 單一 pipeline 檔案刪除 API
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

  asyncServer.on("/api/running", HTTP_GET, [&](AsyncWebServerRequest *request){
    String pipelineFilesList;
    serializeJsonPretty(*Device_Ctrl.JSON__pipelineConfig, pipelineFilesList);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", pipelineFilesList);
    request->send(response);
  });
}

//! 儀器設定檔相關API
void Set_deviceConfigs_apis(AsyncWebServer &asyncServer)
{
  //? 儀器基礎設定資料 API，無參數
  asyncServer.on("/api/config/device_base_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__DeviceBaseInfo), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  //? 更改儀器基礎設定資料 API
  //? 所需path參數: 
  //? (String) device_no : 儀器名稱
  //? (String) LINE_Notify_id : LINE 權杖 (不加密內容)
  //? (String) LINE_Notify_switch : LINE 通知功能開關
  //? (String) Mail_Notify_switch : GMail 通知功能開關
  //? (String) Mail_Notify_Auther : GMail 通知發信者 mail 地址
  //? (String) Mail_Notify_Key : GMail 通知發信者 key (不加密內容)
  //? (String) Mail_Notify_Target : GMail 通知收信者 mail 地址
  asyncServer.on("/api/config/device_base_config", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("device_name")) {
        String NewDeviceName = request->getParam("device_name")->value();
        (*Device_Ctrl.JSON__DeviceBaseInfo)["device_name"].set(NewDeviceName);
      }
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

  //TODO 目前已無再使用，考慮刪除
  asyncServer.on("/api/config/device_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__DeviceConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  //? 蠕動馬達設定檔案 API
  asyncServer.on("/api/config/peristaltic_motor_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__PeristalticMotorConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  //? 伺服馬達設定檔案 API
  asyncServer.on("/api/config/pwm_motor_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__ServoConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  //? 蝦池設定檔案 API
  asyncServer.on("/api/config/pool_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__PoolConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  //? 更改蝦池設定檔案 API
  //? 所需path參數: (String/必要) content : JSON 格式，直接更新整個蝦池 JSON 檔案
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

  //? 光度計設定檔案 API
  asyncServer.on("/api/config/spectrophotometer_config", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson((*Device_Ctrl.JSON__SpectrophotometerConfig), RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  //? 更改光度計設定檔案 API
  //? 所需path參數: (int/必要) index : 指定光度計 ID
  //?               (String) title : 光度計 title
  //?               (String) desp : 光度計設定說明
  //?               (double) m : 校正斜率
  //?               (double) b : 校正截距
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
  //? 更改pH計設定檔案 API
  //? 所需path參數: (int/必要) index : 指定pH計 ID
  //?               (String) title : pH 設定 title
  //?               (String) desp : pH 設定說明
  //?               (double) m : 校正斜率
  //?               (double) b : 校正截距
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

  asyncServer.on("/api/config/wifi_check", HTTP_GET,
  [&](AsyncWebServerRequest *request)
    { 
      if (request->hasParam("open")) {
        (*Device_Ctrl.JSON__WifiConfig)["Remote"]["check"].set(1);
      }
      if (request->hasParam("close")) {
        (*Device_Ctrl.JSON__WifiConfig)["Remote"]["check"].set(0);
      }
      ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__WiFiConfig, (*Device_Ctrl.JSON__WifiConfig));
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "OK");
      request->send(response);
      
    }
  );
}

//! 排程相關API
void Set_scheduleConfig_apis(AsyncWebServer &asyncServer)
{
  //? 當前排程檔案資料 API，無參數
  asyncServer.on("/api/schedule", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String RetuenString;
      serializeJson(*Device_Ctrl.JSON__ScheduleConfig, RetuenString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", RetuenString);
      request->send(response);
    }
  );
  //? 更改當前排程檔案資料 API
  //? 所需path參數: (int/必要)index, (String/必要)name
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

  //? 單一 pipeline log 檔案讀取 API
  asyncServer.on("^\\/api\\/pipeline_log\\/([a-zA-Z0-9_.-]+)\\.log$", HTTP_GET,[&](AsyncWebServerRequest *request){ 
    String fileName = request->pathArg(0);
    String fullPath = "/pipeline_log/"+fileName+".log";
    AsyncWebServerResponse* response;
    if (!SD.exists(fullPath)) {
      response = request->beginResponse(500, "application/json", "{\"Result\":\"Can't Find: "+fileName+"\"}");
      request->send(response);
    }
    else {
      AsyncWebServerResponse *response = request->beginResponse(SD, fullPath, "text/plain");

      // File FileChose = SD.open(fullPath, "r");
      // String ContentString = FileChose.readString();
      // FileChose.close();       
      // response = request->beginResponse(200, "application/json", ContentString);       
      request->send(response);
    }
  });

}

//! 工具類型API
void Set_tool_apis(AsyncWebServer &asyncServer)
{
  asyncServer.on("/api/PoolData", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      DynamicJsonDocument D_baseInfo(20000);
      D_baseInfo["device_no"] = (*Device_Ctrl.JSON__DeviceBaseInfo)["device_no"].as<String>();
      JsonObject pool_datas = D_baseInfo.createNestedObject("pool_datas");
      for (JsonPair JsonPair_poolsSensorData : (*Device_Ctrl.JSON__sensorDataSave).as<JsonObject>()) {
        String S_PoolID = String(JsonPair_poolsSensorData.key().c_str());
        if (S_PoolID == "test" | S_PoolID == "RO") {
          continue;
        }
        int IsUsed = true;
        for (JsonVariant value : (*Device_Ctrl.JSON__PoolConfig).as<JsonArray>()) {
          JsonObject PoolConfigItem = value.as<JsonObject>();
          if (PoolConfigItem["id"].as<String>() == S_PoolID) {
            //? 檢查 pool id 是否有指定，如果有則給出的資料 id 會改使用這個指定的名稱
            if (PoolConfigItem["external_mapping"].as<String>() != "") {
              S_PoolID = PoolConfigItem["external_mapping"].as<String>();
            }
            IsUsed = PoolConfigItem["Used"].as<int>();
            break;
          }
        }
        //? 如果該池設定上為 NodeRed不上傳資料，則跳過該池
        if (IsUsed == false) {
          continue;
        }
        DynamicJsonDocument D_poolSensorDataSended(5000);
        JsonObject D_poolsSensorData = JsonPair_poolsSensorData.value();
        D_poolSensorDataSended["PoolID"] = S_PoolID;
        for (JsonPair JsonPair_SensorData : D_poolsSensorData["DataItem"].as<JsonObject>()) {
          String ItemName = String(JsonPair_SensorData.key().c_str());
          JsonObject ItemObj = JsonPair_SensorData.value();
          pool_datas[S_PoolID][ItemName]["value"] = ItemObj["Value"].as<double>();
          pool_datas[S_PoolID][ItemName]["data_time"] = ItemObj["data_time"].as<String>();
          // serializeJsonPretty(JsonPair_SensorData.value(), Serial);
        }
      }
      String returnString;
      serializeJson(D_baseInfo, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );
  //! 重設 PoolData 用 api
  asyncServer.on("/api/PoolData", HTTP_DELETE,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasArg("pl")) {
        String PoolID = request->getParam("pl")->value();
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NO2_wash_volt"]["Value"].set(-1.);
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NO2_wash_volt"]["data_time"].set("1990-01-01 00:00:00");
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NO2_test_volt"]["Value"].set(-1.);
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NO2_test_volt"]["data_time"].set("1990-01-01 00:00:00");
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NO2"]["Value"].set(-1.);
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NO2"]["data_time"].set("1990-01-01 00:00:00");
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NH4_wash_volt"]["Value"].set(-1.);
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NH4_wash_volt"]["data_time"].set("1990-01-01 00:00:00");
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NH4_test_volt"]["Value"].set(-1.);
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NH4_test_volt"]["data_time"].set("1990-01-01 00:00:00");
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NH4"]["Value"].set(-1.);
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["NH4"]["data_time"].set("1990-01-01 00:00:00");
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["pH_volt"]["Value"].set(-1.);
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["pH_volt"]["data_time"].set("1990-01-01 00:00:00");
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["pH"]["Value"].set(-1.);
        (*Device_Ctrl.JSON__sensorDataSave)[PoolID]["DataItem"]["pH"]["data_time"].set("1990-01-01 00:00:00");
        ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__LastSensorDataSave, *Device_Ctrl.JSON__sensorDataSave);
      }
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "OK");
      request->send(response);
    }
  );

  //? 耗材通知功能測試API
  //? 無參數，GET後直接執行通知流程
  //! 注意，若儀器當前沒有項目達到發送閥值，則會無反應
  asyncServer.on("/api/consume/test", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      Device_Ctrl.SendComsumeWaring();
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json","OK");
      request->send(response);
    }
  );
  //? 耗材設定值/剩餘量資料獲取API，無參數
  asyncServer.on("/api/consume", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String returnString;
      serializeJson(*Device_Ctrl.JSON__Consume, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );
  //? 耗材設定值/剩餘量資料重設API
  //? 無參數，GET後直接重設各項數值
  asyncServer.on("/api/consume", HTTP_DELETE,
    [&](AsyncWebServerRequest *request)
    { 
      (*Device_Ctrl.JSON__Consume)["RO"]["alarm"].set(10);
      (*Device_Ctrl.JSON__Consume)["RO"]["remaining"].set(300);
      (*Device_Ctrl.JSON__Consume)["NO2_R1"]["alarm"].set(10);
      (*Device_Ctrl.JSON__Consume)["NO2_R1"]["remaining"].set(500);
      (*Device_Ctrl.JSON__Consume)["NH4_R1"]["alarm"].set(10);
      (*Device_Ctrl.JSON__Consume)["NH4_R1"]["remaining"].set(500);
      (*Device_Ctrl.JSON__Consume)["NH4_R2"]["alarm"].set(10);
      (*Device_Ctrl.JSON__Consume)["NH4_R2"]["remaining"].set(500);
      ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__Consume, *Device_Ctrl.JSON__Consume);
      String returnString;
      serializeJson(*Device_Ctrl.JSON__Consume, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );
  //? 耗材設定值/剩餘量資料數值變更API
  //? 所需path參數: (String/必要)name, (double)alarm, (double)remaining
  asyncServer.on("/api/consume", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      AsyncWebServerResponse* response;
      String keyName = request->pathArg(0);
      if (request->hasArg("name")) {
        String name = request->getParam("name")->value();
        if (request->hasArg("alarm")) {
          double alarm = String(request->getParam("alarm")->value()).toDouble();
          (*Device_Ctrl.JSON__Consume)[name]["alarm"].set(alarm);
        }
        if (request->hasArg("remaining")) {
          double remaining = String(request->getParam("remaining")->value()).toDouble();
          (*Device_Ctrl.JSON__Consume)[name]["remaining"].set(remaining);
        }
        ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__Consume, *Device_Ctrl.JSON__Consume);
        String returnString;
        serializeJson(*Device_Ctrl.JSON__Consume, returnString);
        response = request->beginResponse(200, "application/json", returnString);
      }
      else {
        response = request->beginResponse(500, "application/json", "{\"Result\":\"缺少para: 'name'\"}");
      }
      request->send(response);
    }
  );
  //? 維護項目資料獲取API，無參數
  asyncServer.on("/api/maintain", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String returnString;
      serializeJson(*Device_Ctrl.JSON__Maintain, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );
  //? 維護項目重設API
  //? 無參數，DELETE後直接重設各項數值
  asyncServer.on("/api/maintain", HTTP_DELETE,
    [&](AsyncWebServerRequest *request)
    { 
      Device_Ctrl.RebuildMaintainJSON();
      String returnString;
      serializeJson(*Device_Ctrl.JSON__Consume, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );
  //? 維護項目數值變更API
  //? 所需path參數: (String/必要)name, (String/必要)time
  asyncServer.on("/api/maintain", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      AsyncWebServerResponse* response;
      String keyName = request->pathArg(0);
      if (request->hasArg("name")) {
        String name = request->getParam("name")->value();
        if (request->hasArg("time")) {
          String time = request->getParam("time")->value();
          (*Device_Ctrl.JSON__Maintain)[name]["time"].set(time);
          ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__Maintain, *Device_Ctrl.JSON__Maintain);
          String returnString;
          serializeJson(*Device_Ctrl.JSON__Maintain, returnString);
          response = request->beginResponse(200, "application/json", returnString);
          request->send(response);
        }
        else {
          response = request->beginResponse(500, "application/json", "{\"Result\":\"缺少para: 'time'\"}");
          request->send(response);
        }
      }
      else {
        response = request->beginResponse(500, "application/json", "{\"Result\":\"缺少para: 'name'\"}");
        request->send(response);
      }
    }
  );

  //? RO 水最新量測結果修正/獲取，用以修正池水量測值
  //? 所需path參數: (double)NO2, (double)NH4
  //? 若有給出參數，則修改儲存之，並回覆修正後數值
  //? 若無給參數，則直接返回當前數值
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
  //? 各部件使用歷程計數歸零API
  //? 所需path參數: (String/必要)name
  //? 會清除指定目標歷程數紀錄
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
  //? 各部件使用歷程計數獲得，無參數，全返回
  asyncServer.on("/api/device/used", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String returnString;
      serializeJson(*Device_Ctrl.JSON__ItemUseCount, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );
  //? SD卡相關資訊
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
  //? LINE 通知功能測試，無參數
  //? GET 後直接執行 LINE 訊息通知功能
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
  //? GMAIL 通知功能測試，無參數
  //? GET 後直接執行 GMAIL 訊息通知功能
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
  //? 獲得當前Websocket所有連線單位的狀態
  asyncServer.on("/api/wifi/websocket", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String returnString;
      serializeJson(Device_Ctrl.GetWebsocketConnectInfo(), returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", returnString);
      request->send(response);
    }
  );
  //? 韌體.bin上傳API
  //? 上傳成功後會重開機
  asyncServer.on("/api/firmware", HTTP_POST, 
    [&](AsyncWebServerRequest *request)
    { 
      Serial.printf("檔案更新完成，等待韌體更新\n");
      Device_Ctrl.CheckUpdateFile = true;
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"Result\":\"ok\"}");
      request->send(response);
    },
    [&](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
      String filePath = "/firmware/firmware.bin";
      File configTempFile;
      if (index == 0) {
        Device_Ctrl.BroadcastLogToClient(NULL, 3,"收到新韌體上傳需求，資料大小: %d", request->contentLength());
        UploadPer = 0;
        if (Device_Ctrl.firmwareBuffer) {
          free(Device_Ctrl.firmwareBuffer);
        }
        Device_Ctrl.firmwareBuffer = (uint8_t *)malloc(request->contentLength());
        Device_Ctrl.firmwareLen = request->contentLength();
      }
      memcpy(Device_Ctrl.firmwareBuffer + index, data, len);
      if (final) {
        Device_Ctrl.BroadcastLogToClient(NULL, 3,"新韌體上傳完畢");
        Serial.printf("檔案 %s 接收完成， len: %d ，共 %d/%d bytes\n", filename.c_str(), len ,index + len, request->contentLength());
      } 
      else {
        int nowPer = static_cast<int>((float)(index + len)/(float)(request->contentLength()) * 100.);
        nowPer = nowPer/10*10;
        if (nowPer != UploadPer) {
          UploadPer = nowPer;
          Device_Ctrl.BroadcastLogToClient(NULL, 3,"新韌體上傳進度: %d %%", UploadPer);
        }
        Serial.printf("檔案 %s 正在傳輸， len: %d ，目前已接收 %d/%d bytes\n", filename.c_str(), len, index + len, request->contentLength());
      }
    }
  );
  //? 上傳 web 靜態檔案API
  asyncServer.on("/api/web", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", "<html><body><div><div>HTML update:</div><input type='file' id='new-html-input' /><button onclick='uploadNew(`html`)'>upload</button></div><div><div>JS update:</div><input type='file' id='new-js-input' /><button onclick='uploadNew(`js`)'>upload</button></div><div><div>Firmware update:</div><input type='file' id='new-bin-input' /><button onclick='uploadNewBin()'>upload</button></div><script>function uploadNew(type) {const fileInput = document.getElementById('new-' + type + '-input');const file = fileInput.files[0];const reader = new FileReader();reader.onload = function (event) {const fileContent = event.target.result;const fileBlob = new Blob([fileContent], { type: file.type });var formData = new FormData();formData.append('file', fileBlob, file.name);fetch('web?type=' + type, { method: 'POST', body: formData }).then((response) => response.json()).then((data) => {console.log('Response from server:', data);}).catch((error) => {console.error('Error:', error);});};reader.readAsArrayBuffer(file);}function uploadNewBin() {const fileInput = document.getElementById('new-bin-input');const file = fileInput.files[0];const reader = new FileReader();reader.onload = function (event) {const fileContent = event.target.result;const fileBlob = new Blob([fileContent], { type: file.type });var formData = new FormData();formData.append('file', fileBlob, file.name);fetch('firmware' , { method: 'POST', body: formData }).then((response) => response.json()).then((data) => {console.log('Response from server:', data);}).catch((error) => {console.error('Error:', error);});};reader.readAsArrayBuffer(file);}</script></body></html>");
    request->send(response);
  });
  //? 主網頁更新專用 API
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
        String fileType = request->getParam("type")->value();
        if (fileType == "html") {
          Device_Ctrl.BroadcastLogToClient(NULL, 3,"收到網頁HTML更新檔上傳需求: %d", request->contentLength());;
        } else if (fileType == "js"){
          Device_Ctrl.BroadcastLogToClient(NULL, 3,"收到網頁.js更新檔上傳需求: %d", request->contentLength());;
        }
      }
      memcpy(newWebUpdateFileBuffer + index, data, len);
      if (final) {
        Serial.printf("檔案 %s 接收完成， len: %d ，共 %d/%d bytes\n", filename.c_str(), len ,index + len, request->contentLength());
        newWebUpdateFileBufferLen = index + len;
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
          free(Device_Ctrl.webJS_Buffer);
          Device_Ctrl.webJS_Buffer = (uint8_t *)malloc(request->contentLength());
          memcpy(Device_Ctrl.webJS_Buffer, newWebUpdateFileBuffer, request->contentLength());
        }
        Device_Ctrl.BroadcastLogToClient(NULL, 3,"新網頁檔案上傳完畢");
      } 
      else {
        int nowPer = static_cast<int>((float)(index + len)/(float)(request->contentLength()) * 100.);
        nowPer = nowPer/10*10;
        if (nowPer != UploadPer) {
          UploadPer = nowPer;
          Device_Ctrl.BroadcastLogToClient(NULL, 3,"新網頁檔案上傳進度: %d %%", UploadPer);
        }
        Serial.printf("檔案 %s 正在傳輸， len: %d ，目前已接收 %d/%d bytes\n", filename.c_str(), len, index + len, request->contentLength());
      }
    }
  );
  //! 儀器時間設定API
  //! 2024-08-15 彰化廠區安裝時遇到 WIFI 無對外連線，無法透過網路校正時間的問題
  //! 特此開一項 API 來讓機器有機會校正時間
  //! 所需path參數: (long/必要)unix, (int)UTC
  asyncServer.on("/api/TimeSet", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasArg("unix")) {
        long time = request->getParam("unix")->value().toInt();
        if (request->hasArg("UTC")) {
          int utc = request->getParam("UTC")->value().toInt();
          time += utc*60*60;
        }
        setTime(time);
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json",GetDatetimeString());
        request->send(response);
      }
      else {
        AsyncWebServerResponse* response = request->beginResponse(400, "application/json","需要參數: unix");
        request->send(response);
      }
    }
  );

}

//! DB 相關 API
void Set_DB_apis(AsyncWebServer &asyncServer) {
  //? 感測器資料 SQL Query API
  //? 所需path參數: 
  //? (String)st : 開頭時間，default = 1900-01-01 00:00:00
  //? (String)et : 結束時間，default = 2900-01-01 00:00:00
  //? (String)pl : 目標 pool list，default = 'pool-1','pool-2','pool-3','pool-4'
  //? (String)name : 目標 name list，default = 'pH','NO2','NH4'
  //? 資料最長 56625
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

  asyncServer.on("/api/sensor", HTTP_POST,
    [&](AsyncWebServerRequest *request)
    { 
      String Datatime = request->getParam("tm")->value();
      String pool = request->getParam("pl")->value();
      String type = request->getParam("tp")->value();
      double value = request->getParam("value")->value().toDouble();
      Device_Ctrl.InsertNewDataToDB(
        Datatime, pool, type, value
      );

      AsyncWebServerResponse* response = request->beginResponse(200, "OK");
      request->send(response);;
    }
  );

  asyncServer.on("/api/v2/sensor", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      //? 回傳資料格式
      /**
       * [
       *    (date unixtime: 32 bits) (pool count: 8 bits) [
       *      (pool map id: 8bits) (type count: 8 bits) [
       *        (type map id: 8bits) (data count: 16bits) [
      *            (data: 1組 32bits)...
       *        ]
       *      ]
       *    ]
       * ]
       */
      int SENSOR_DATA_MAX_LEN = 1024;
      uint8_t *returnData = (uint8_t *)malloc(SENSOR_DATA_MAX_LEN * sizeof(uint8_t));
      memset(returnData, 0, SENSOR_DATA_MAX_LEN * sizeof(uint8_t));

      String startDateString = GetDateString("");
      String endDateString = GetDateString("");
      String poolNameString = "pool-1,pool-2,pool-3,pool-4";
      String typeNameString = "pH,NO2,NH4";
      int CountNum = 0;
      if (request->hasParam("et")) {
        endDateString = request->getParam("et")->value();
      }
      if (request->hasParam("st")) {
        startDateString = request->getParam("st")->value();
      }
      if (request->hasParam("pl")) {
        poolNameString = request->getParam("pl")->value();
      }
      if (request->hasParam("tp")) {
        typeNameString = request->getParam("tp")->value();
      }
      if (request->hasParam("ct")) {
        CountNum = request->getParam("ct")->value().toInt();
      }
      //? 處理 pl 文字分割
      int poolTargetCount = 0;
      String poolTarget[10];
      splitString(poolNameString, ',', poolTarget, poolTargetCount);
      //? 處理 tp 文字分割
      int typeTargetCount = 0;
      String typeTarget[10];
      splitString(typeNameString, ',', typeTarget, typeTargetCount);

      int poolStart = 0;
      if (request->hasParam("pl_st")) {
        poolStart = request->getParam("pl_st")->value().toInt();
      }
      int poolEnd = 0;
      int typeStart = 0;
      if (request->hasParam("tp_st")) {
        typeStart = request->getParam("tp_st")->value().toInt();
      }
      int typeEnd = 0;

      //? 處理搜尋時間範圍，從 end time 開始搜尋，以日為單位
      struct tm endTime;
      sscanf(endDateString.c_str(), "%4d%2d%2d", &(endTime.tm_year), &(endTime.tm_mon), &(endTime.tm_mday));
      endTime.tm_year -= 1900;
      endTime.tm_mon -= 1;
      endTime.tm_hour = 0;
      endTime.tm_min = 0;
      endTime.tm_sec = 0;
      char dateStringBuffer[9];
      strftime(dateStringBuffer, sizeof(dateStringBuffer), "%Y%m%d", &endTime);

      struct tm startTime;
      sscanf(startDateString.c_str(), "%4d%2d%2d", &(startTime.tm_year), &(startTime.tm_mon), &(startTime.tm_mday));
      startTime.tm_year -= 1900;
      startTime.tm_mon -= 1;
      startTime.tm_hour = 0;
      startTime.tm_min = 0;
      startTime.tm_sec = 0;
      int data_now_index = 0; //? 當前資料 index 位置
      int end = 1;
      int SearchCount = 0;
      int MAX_SEARCH_COUNT = 10;
      ESP_LOGV("DB搜尋","時間範圍: %s ~ %s", startDateString.c_str(), endDateString.c_str());
      ESP_LOGV("DB搜尋","pool 項目: %s", poolNameString.c_str());
      ESP_LOGV("DB搜尋","type 項目: %s", typeNameString.c_str());
      ESP_LOGV("DB搜尋","起始 index 指定: pool: %d, type: %d", poolStart, typeStart);
      ESP_LOGV("DB搜尋","==============================================================================");
      //! 搜尋條件為: 1. 還在搜尋時間範圍內
      while (mktime(&endTime)>=mktime(&startTime)) {
        strftime(dateStringBuffer, sizeof(dateStringBuffer), "%Y%m%d", &endTime);
        String dateString = String(dateStringBuffer);
        ESP_LOGV("DB搜尋"," - 準備搜尋時間: %s", dateString.c_str());
        time_t unixTime = mktime(&endTime);
        ESP_LOGD("DB搜尋","  - 當前 index %d, 時間位置: %p ~ %p", data_now_index, &returnData[data_now_index], &returnData[data_now_index+3]);
        for (int i = 0; i < 4; i++) {
          //? 每個時間區段的開頭為 4 Bytes 的 Unix Time 
          returnData[data_now_index++] = (unixTime >> (i * 8)) & 0xFF;
        }
        ESP_LOGD("DB搜尋","  - 當前 index %d, poolCount位置: %p", data_now_index, &returnData[data_now_index]);
        int poolCountIndex = data_now_index;
        data_now_index++;
        // uint8_t *poolCount = &returnData[data_now_index++]; //! 此 Bytes 負責記錄當前 date 中有幾個不同的 pool 資料
        for (int poolChose=poolStart;poolChose<poolTargetCount;poolChose++) {
          ESP_LOGV("DB搜尋","   - 準備搜尋 pool: %s", poolTarget[poolChose].c_str());
          returnData[poolCountIndex] += 1;
          // *poolCount += 1;  //! pool 數 +1，後續如果發現此 pool 沒資料的話要捨棄掉
          poolEnd = poolChose;
          ESP_LOGD("DB搜尋","    - 當前 index %d, pool ID 位置: %p", data_now_index, &returnData[data_now_index]);
          returnData[data_now_index++] = Device_Ctrl.mappingPoolNameToID(poolTarget[poolChose]);  //! 將此 pool ID 塞入
          ESP_LOGD("DB搜尋","    - 當前 index %d, typeCount 位置: %p", data_now_index, &returnData[data_now_index]);
          int typeCountIndex = data_now_index;
          data_now_index++;
          // uint8_t *typeCount =  &returnData[data_now_index++]; //! 此 Bytes 負責記錄當前 pool 中有幾個不同的 type 資料
          for (int typeChose=typeStart;typeChose<typeTargetCount;typeChose++) {
            ESP_LOGV("DB搜尋","     - 準備搜尋 type: %s", typeTarget[typeChose].c_str());
            SearchCount++;
            String FileFullPath = "/datas/"+dateString+"/"+poolTarget[poolChose]+"/"+typeTarget[typeChose]+".bin";
            typeEnd = typeChose;
            if (SD.exists(FileFullPath)) {
              File sensorFile = SD.open(FileFullPath, FILE_READ);
              int fileSize = sensorFile.size(); 
              ESP_LOGD("DB搜尋","        - 檢查記憶體長度，當前 index %d，初始位置: %p", data_now_index, returnData);
              if (data_now_index+3+fileSize >= SENSOR_DATA_MAX_LEN) {
                ESP_LOGD("DB搜尋","        - 資料過長，預計請求更長的Buffer，總長為 : %d", fileSize + 5 + data_now_index);
                returnData = (uint8_t *)realloc(returnData, (fileSize + 5 + data_now_index) * sizeof(uint8_t));
                // for (int i=data_now_index+1;i<fileSize + 5 + data_now_index;i++) {
                //   returnData[i] = 0;
                // }

                // uint8_t dataBuffer[SENSOR_DATA_MAX_LEN] = {0};
                // memcpy(dataBuffer, returnData, SENSOR_DATA_MAX_LEN * sizeof(uint8_t));
                // returnData = (uint8_t *)realloc(returnData, (fileSize + 5 + data_now_index) * sizeof(uint8_t));
                // for (int i=data_now_index;i<SENSOR_DATA_MAX_LEN;i++) {
                //   returnData[i] = dataBuffer[i];
                // }
                // for (int i=data_now_index;i<fileSize + 5 + data_now_index;i++) {
                //   returnData[i] = 0;
                // }
              }
              ESP_LOGD("DB搜尋","        - 檢查記憶體長度後，當前 index %d，初始位置: %p", data_now_index, returnData);

              returnData[typeCountIndex] += 1;
              // *typeCount = *typeCount + 1;
              ESP_LOGD("DB搜尋","        - 當前 index %d, type ID: %p", data_now_index, &returnData[data_now_index]);
              returnData[data_now_index++] = Device_Ctrl.mappingTypeNameToID(typeTarget[typeChose]);
              Serial.println( returnData[data_now_index-1]);
              ESP_LOGD("DB搜尋","        - 當前 index %d, dataCount: %p ~ %p", data_now_index, &returnData[data_now_index], &returnData[data_now_index+1]);
              returnData[data_now_index++] = fileSize/4 >> 8;  //! 每 4 Bytes 一筆資料
              returnData[data_now_index++] = fileSize/4;
              Serial.println( returnData[data_now_index-1] + returnData[data_now_index-2] << 8);
              ESP_LOGD("DB搜尋","        - 當前 index %d, data 範圍: %p ~ %p, 長度: %d", data_now_index, &returnData[data_now_index], &returnData[data_now_index+fileSize-1], &returnData[data_now_index+fileSize-1] - &returnData[data_now_index] + 1);
              ESP_LOGV("DB搜尋","       - 讀取檔案: %s", FileFullPath.c_str());
              ESP_LOGV("DB搜尋","       - 資料長度: %d Bytes => %d 筆", fileSize, fileSize/4);
              if (fileSize + data_now_index > SENSOR_DATA_MAX_LEN) {
                Serial.printf("%d,%d,%d\n", returnData[data_now_index-3], returnData[data_now_index-2], returnData[data_now_index-1]);
                //! 進到這邊代表資料長度加上去會超過最長長度閥值，需要加長資料欄位
                ESP_LOGD("DB搜尋","        - 資料總長已達上限");
                // returnData = (uint8_t *)realloc(returnData, (fileSize + data_now_index) * sizeof(uint8_t));
                sensorFile.read(returnData+data_now_index, fileSize); //! 在當前指針處 (理論上會在 data count 的下一位) 讀取資料長度的資料
                Serial.printf("%d,%d,%d,%d\n", returnData[data_now_index], returnData[data_now_index+1], returnData[data_now_index+2], returnData[data_now_index+3]);
                data_now_index+=fileSize; //! 位置指針往後 fileSize 個 Bytes
                end = 0;
                break; //! 待傳資料已經滿了，跳出迴圈，準備傳資料
              }
              else {
                sensorFile.read(returnData+data_now_index, fileSize); //! 在當前指針處 (理論上會在 data count 的下一位) 讀取資料長度的資料
                Serial.printf("%d,%d,%d,%d\n", returnData[data_now_index], returnData[data_now_index+1], returnData[data_now_index+2], returnData[data_now_index+3]);
                data_now_index+=fileSize; //! 位置指針往後 fileSize 個 Bytes，移動至下一個 Type map ID 位置
              }
            }
            else {  
              ESP_LOGV("DB搜尋","       - type: %s 沒有找到資料", typeTarget[typeChose].c_str());
            }
            if (SearchCount >= MAX_SEARCH_COUNT) {
              ESP_LOGD("DB搜尋","        - 檔案搜尋數達上限");
              end = 0;
              break; //! 待傳資料已經滿了，跳出迴圈，準備傳資料
            }
            esp_task_wdt_reset();
          }
          ESP_LOGV("DB搜尋","   - pool: %s 下找到 %d 個 type 資料", poolTarget[poolChose].c_str(), returnData[typeCountIndex]);
          if (returnData[typeCountIndex] == 0) {
            ESP_LOGV("DB搜尋","   - pool: %s 無任何 type 資料", poolTarget[poolChose].c_str());
            //! 如果 type count 為 0，代表這個 pool 下沒有任何資料
            //! 此時 poolCount 要減回去
            // *poolCount -= 1;
            returnData[poolCountIndex] -= 1;
            //! 並且 data_now_index 也要往回推 2 個位置
            data_now_index -= 2;
            if (SearchCount >= MAX_SEARCH_COUNT){break;};
          }
          else if (data_now_index > SENSOR_DATA_MAX_LEN or SearchCount >= MAX_SEARCH_COUNT) {break;} //! 待傳資料已經滿了，跳出迴圈，準備傳資料
          typeStart = 0; //? 若 typeStart 非 0，代表第一次 LOOP typeList 時要跳過指定項目數，第一圈後要重製
        }
        ESP_LOGV("DB搜尋"," - Date: %s 下找到 %d 個 pool 資料", dateString.c_str(), returnData[poolCountIndex]);
        if (returnData[poolCountIndex] == 0) {
          ESP_LOGV("DB搜尋","       - Date: %s 沒有找到 pool 資料", dateString.c_str());
          //! 如果 type count 為 0，代表這個 date 下沒有任何資料
          //! 此時 data_now_index 要往回推 1+4 個位置 (pool count & unix time)
          data_now_index -= 5;
          if (SearchCount >= MAX_SEARCH_COUNT){break;};
        }
        else if (data_now_index > SENSOR_DATA_MAX_LEN or SearchCount >= MAX_SEARCH_COUNT) {
          break; //! 待傳資料已經滿了，跳出迴圈，準備傳資料
        }
        poolStart = 0; //? 若 poolStart 非 0，代表第一次 LOOP poolList 時要跳過指定項目數，第一圈後要重製
        endTime.tm_mday -= 1;
      }
      strftime(dateStringBuffer, sizeof(dateStringBuffer), "%Y%m%d", &endTime);
      
      String dateString = String(dateStringBuffer);  //! 停止搜尋時的搜尋日期
      ESP_LOGV("DB搜尋","搜尋停止時間: %s, pool index: %d, type index: %d", dateString.c_str(), poolEnd, typeEnd);
      ESP_LOGV("DB搜尋","資料總長: %d", data_now_index);

      uint8_t returnDataBuffer[data_now_index];
      memcpy(returnDataBuffer, returnData, data_now_index * sizeof(uint8_t));
      free(returnData);

      AsyncWebServerResponse *response = request->beginResponse(
        "application/octet-stream",  // MIME类型为二进制流
        data_now_index,  // 数据长度
        [&](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
          // 复制二进制数据到buffer
          if (index < data_now_index) {
            size_t len = min(maxLen, data_now_index - index);
            memcpy(buffer, returnDataBuffer + index, len);
            return len;
          }
          return 0;  // 传输结束
        }
      );
      response->addHeader("X-pl",String(poolEnd));
      response->addHeader("X-tp",String(typeEnd));
      response->addHeader("X-et",dateString);
      response->addHeader("X-end",String(end));
      response->addHeader("X-ct",String(CountNum));
      response->addHeader("X-search-count",String(SearchCount));

      // AsyncWebServerResponse* response = request->beginResponse(200, "application/octet-stream", returnDataBuffer);
      // AsyncWebServerResponse* response = request->beginResponse(200, "application/octet-stream", &returnDataBuffer, data_now_index);
      request->send(response);
    }
  ); 

  asyncServer.on("/api/v2/sensor", HTTP_DELETE,
    [&](AsyncWebServerRequest *request)
    { 
      String DateString = request->getParam("tm")->value();
      String pool = request->getParam("pl")->value();
      String type = request->getParam("tp")->value();
      String FileFullPath = "/datas/"+DateString+"/"+pool+"/"+type+".bin";
      Serial.println(FileFullPath);
      if (SD.exists(FileFullPath)) {
        SD.remove(FileFullPath);
        AsyncWebServerResponse* response = request->beginResponse(200, "OK");
        request->send(response);
      } else {
        AsyncWebServerResponse* response = request->beginResponse(500, FileFullPath.c_str());
        request->send(response);;
      }
    }
  );
  asyncServer.on("/api/v2/sensor", HTTP_POST,
    [&](AsyncWebServerRequest *request)
    { 
      int unixTime = request->getParam("tm")->value().toInt();
      String pool = request->getParam("pl")->value();
      String type = request->getParam("tp")->value();
      int value = request->getParam("value")->value().toInt();
      Device_Ctrl.SaveSensorDataToBinFile((time_t)unixTime, pool, type, value);
      AsyncWebServerResponse* response = request->beginResponse(200, "OK");
      request->send(response);
    }
  );


  //? 感測器 DB 重設 API
  //? 無參數，GET 後直接重設 DB 檔案
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
}

//! 測試相關API
void Set_test_apis(AsyncWebServer &asyncServer)
{
  //! 純為測試用 API ，可手動修改最新各池量測值
  asyncServer.on("/api/test/PoolData", HTTP_PATCH,
    [&](AsyncWebServerRequest *request)
    { 
      if (request->hasArg("pl")) {
        String pool = request->getParam("pl")->value();
        if (request->hasArg("type")) {
          String type = request->getParam("type")->value();
          if (request->hasArg("value")) {
            double value = request->getParam("value")->value().toDouble();
            if (request->hasArg("data_time")) {
              String data_time = request->getParam("data_time")->value();
              (*Device_Ctrl.JSON__sensorDataSave)[pool]["DataItem"][type]["Value"].set(value);
              (*Device_Ctrl.JSON__sensorDataSave)[pool]["DataItem"][type]["data_time"].set(data_time);
            }
          }
        }
      }
      ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__LastSensorDataSave, *Device_Ctrl.JSON__sensorDataSave);
      String returnString;
      serializeJson(*Device_Ctrl.JSON__sensorDataSave, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json",returnString);
      request->send(response);
    }
  );
  asyncServer.on("/api/test/PoolData", HTTP_GET,
    [&](AsyncWebServerRequest *request)
    { 
      String returnString;
      serializeJson(*Device_Ctrl.JSON__sensorDataSave, returnString);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json",returnString);
      request->send(response);
    }
  );
}


#endif