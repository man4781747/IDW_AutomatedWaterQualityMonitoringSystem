#include "MAIN__DeviceCtrl.h"
#include <esp_log.h>
#include <SD.h>
#include <SPIFFS.h>
#include "StorgeSystemExternalFunction.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "API__HttpAPIurl.h"
#include "SqliteFunctions.h"
#include "PIPELINE__PipelineFlowScanTask.h"
#include "PIPELINE__StepTask.h"
#include <TimeLib.h>
#include "TimeLibExternalFunction.h"
#include "WebsocketSetting.h"
#include "WebsocketAPIFunctions.h"
#include <ArduinoOTA.h> 
#include <NTPClient.h>

const long  gmtOffset_sec = 3600*8; // GMT+8
const int   daylightOffset_sec = 0; // DST+0
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", gmtOffset_sec, daylightOffset_sec);

AsyncWebServer asyncServer(80);
AsyncWebSocket ws("/ws");

void C_Device_Ctrl::INIT_Pins()
{
  pinMode(PIN__ADC_POOL_FULL, INPUT);
  pinMode(PIN__ADC_RO_FULL, INPUT);
  
  pinMode(PIN__Peristaltic_Motor_SHCP, OUTPUT);
  pinMode(PIN__Peristaltic_Motor_SHTP, OUTPUT);
  pinMode(PIN__Peristaltic_Motor_DATA, OUTPUT);
  pinMode(PIN__EN_Peristaltic_Motor, OUTPUT);
  digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
  
  pinMode(PIN__EN_Servo_Motor, OUTPUT);
  digitalWrite(PIN__EN_Servo_Motor, LOW);

  pinMode(PIN__ADC_PH_IN, INPUT);

  pinMode(PIN__EN_BlueSensor, OUTPUT);
  digitalWrite(PIN__EN_BlueSensor, LOW);
  pinMode(PIN__EN_GreenSensor, OUTPUT);
  digitalWrite(PIN__EN_GreenSensor, LOW);
}

void C_Device_Ctrl::INIT_I2C_Wires(TwoWire &WireChose)
{
  _Wire = WireChose;
  _Wire.begin(PIN__SDA_1, PIN__SCL_1, 800000UL);
}

bool C_Device_Ctrl::INIT_PoolData()
{
  if (SD.exists(FilePath__SD__LastSensorDataSave)) {
    if (ExFile_LoadJsonFile(SD, FilePath__SD__LastSensorDataSave, *JSON__sensorDataSave)) {
      ESP_LOGD("", "讀取最新各池感測器測量數值成功");
      return true;
    } else {
      ESP_LOGE("", "讀取最新各池感測器測量數值失敗");
    }
  }
  ESP_LOGE("", "找不到或讀取最新各池感測器測量數值失敗，重建資料庫");
  for (JsonVariant SinPoolInfo : (*JSON__PoolConfig).as<JsonArray>()) {
    JsonObject Array_SinPoolInfo = SinPoolInfo.as<JsonObject>();
    String PoolID = Array_SinPoolInfo["id"].as<String>();
    (*JSON__sensorDataSave)[PoolID]["PoolID"].set(PoolID);
    (*JSON__sensorDataSave)[PoolID]["PoolName"].set(Array_SinPoolInfo["title"].as<String>());
    (*JSON__sensorDataSave)[PoolID]["PoolDescription"].set(Array_SinPoolInfo["desp"].as<String>());
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NO2_wash_volt"]["Value"].set(-1.);
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NO2_wash_volt"]["data_time"].set("1990-01-01 00:00:00");
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NO2_test_volt"]["Value"].set(-1.);
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NO2_test_volt"]["data_time"].set("1990-01-01 00:00:00");
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NO2"]["Value"].set(-1.);
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NO2"]["data_time"].set("1990-01-01 00:00:00");
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NH4_wash_volt"]["Value"].set(-1.);
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NH4_wash_volt"]["data_time"].set("1990-01-01 00:00:00");
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NH4_test_volt"]["Value"].set(-1.);
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NH4_test_volt"]["data_time"].set("1990-01-01 00:00:00");
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NH4"]["Value"].set(-1.);
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["NH4"]["data_time"].set("1990-01-01 00:00:00");
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["pH_volt"]["Value"].set(-1.);
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["pH_volt"]["data_time"].set("1990-01-01 00:00:00");
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["pH"]["Value"].set(-1.);
    (*JSON__sensorDataSave)[PoolID]["DataItem"]["pH"]["data_time"].set("1990-01-01 00:00:00");
  }
  return false;
}

bool C_Device_Ctrl::INIT_SD()
{
  if (!SD.begin(PIN__SD_CS)) {
    ESP_LOGE("", "SD卡讀取失敗");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    ESP.restart();
    return false;
  }
  ESP_LOGD("", "SD卡讀取成功");
  return true;
}

bool C_Device_Ctrl::INIT_SPIFFS()
{
  if(!SPIFFS.begin(true)){
    ESP_LOGE("", "讀取SPIFFS失敗");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    ESP.restart();
    return false;
  }
  ESP_LOGD("", "讀取SPIFFS成功");
  return true;
}

int C_Device_Ctrl::INIT_SqliteDB()
{
  // SD.remove("/mainDB.db");
  sqlite3_initialize();
  int rc = sqlite3_open(FilePath__SD__MainDB.c_str(), &DB_Main);
   if (rc) {
      Serial.printf("Can't open database: %s\n", sqlite3_errmsg(DB_Main));
      return rc;
   } else {
      Serial.printf("Opened database successfully\n");
      db_exec(DB_Main, "CREATE TABLE logs ( time TEXT, level INTEGER, content TEXT );");
      db_exec(DB_Main, "CREATE TABLE sensor ( time TEXT, pool TEXT , value_name TEXT , result REAL );");
   }
  return rc;
}

void C_Device_Ctrl::LoadConfigJsonFiles()
{
  ExFile_LoadJsonFile(SD, FilePath__SD__DeviceBaseInfo, *JSON__DeviceBaseInfo);
  ExFile_LoadJsonFile(SD, FilePath__SD__SpectrophotometerConfig, *JSON__SpectrophotometerConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__PHmeterConfig, *JSON__PHmeterConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__PoolConfig, *JSON__PoolConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__ServoConfig, *JSON__ServoConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__PeristalticMotorConfig, *JSON__PeristalticMotorConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__ScheduleConfig, *JSON__ScheduleConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__DeviceConfig, *JSON__DeviceConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__WiFiConfig, *JSON__WifiConfig);
}

void C_Device_Ctrl::InsertNewDataToDB(String time, String pool, String ValueName, double result)
{
  String SqlString = "INSERT INTO sensor ( time, pool, value_name, result ) VALUES ( '";
  SqlString += time;
  SqlString += "' ,'";
  SqlString += pool;
  SqlString += "' ,'";
  SqlString += ValueName;
  SqlString += "' ,";
  SqlString += String(result,2).toDouble();
  SqlString += " );";
  db_exec(DB_Main, SqlString);
}

void C_Device_Ctrl::InsertNewLogToDB(String time, int level, const char* content, ...)
{
  char buffer[1024];  // 用于存储格式化后的文本
  va_list ap;
  va_start(ap, content);
  vsprintf(buffer, content, ap);
  va_end(ap);
  String SqlString = "INSERT INTO logs ( level, time, content ) VALUES ( ";
  SqlString += String(level);
  SqlString += " ,'";
  SqlString += GetDatetimeString();
  SqlString += "' ,'";
  SqlString += String(buffer);
  SqlString += "' );";
  db_exec(DB_Main, SqlString);
}

void C_Device_Ctrl::UpdatePipelineConfigList()
{
  (*JSON__PipelineConfigList).clear();
  File folder = SD.open("/pipelines");
  while (true) {
    File Entry = folder.openNextFile();
    if (! Entry) {
      break;
    }
    String FileName = String(Entry.name());
    if (FileName == "__temp__.json") {
      continue;
    }
    ESP_LOGD("", "準備讀取設定檔: %s", FileName.c_str());
    DynamicJsonDocument fileInfo(500);
    fileInfo["size"].set(Entry.size());
    fileInfo["name"].set(FileName);
    fileInfo["getLastWrite"].set(Entry.getLastWrite());
    DynamicJsonDocument fileContent(120000);
    DeserializationError error = deserializeJson(fileContent, Entry);
    Entry.close();
    ESP_LOGD("", "設定檔讀取完畢: %s", FileName.c_str());
    fileInfo["title"].set(fileContent["title"].as<String>());
    fileInfo["desp"].set(fileContent["desp"].as<String>());
    fileInfo["tag"].set(fileContent["tag"].as<JsonArray>());
    (*JSON__PipelineConfigList).add(fileInfo);
  }

}

//! WiFi相關功能

void C_Device_Ctrl::ConnectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(
    (*JSON__WifiConfig)["Remote"]["remote_Name"].as<String>().c_str(),
    (*JSON__WifiConfig)["Remote"]["remote_Password"].as<String>().c_str()
  );
  WiFi.setAutoReconnect(true);
}

void C_Device_Ctrl::AddWebsocketAPI(String APIPath, String METHOD, void (*func)(AsyncWebSocket*, AsyncWebSocketClient*, DynamicJsonDocument*, DynamicJsonDocument*, DynamicJsonDocument*, DynamicJsonDocument*))
{
  C_WebsocketAPI *newAPI = new C_WebsocketAPI(APIPath, METHOD, func);
  std::unordered_map<std::string, C_WebsocketAPI*> sub_map;
  if (websocketApiSetting.count(std::string(newAPI->APIPath.c_str())) == 0) {
    sub_map[std::string(newAPI->METHOD.c_str())] = newAPI;
    websocketApiSetting.insert(
      std::make_pair(std::string(newAPI->APIPath.c_str()), sub_map)
    );
  } else {
    websocketApiSetting[std::string(newAPI->APIPath.c_str())][std::string(newAPI->METHOD.c_str())] = newAPI;
  }
}

void C_Device_Ctrl::INIT_AllWsAPI()
{
  AddWebsocketAPI("/api/System/STOP", "GET", &ws_StopAllActionTask);

  // AddWebsocketAPI("/api/System", "GET", &ws_StopAllActionTask);


  // AddWebsocketAPI("/api/DeiveConfig", "GET", &ws_GetDeiveConfig);
  // AddWebsocketAPI("/api/DeiveConfig", "PATCH", &ws_PatchDeiveConfig);


  // AddWebsocketAPI("/api/GetState", "GET", &ws_GetNowStatus);

  // //!LOG相關API
  // AddWebsocketAPI("/api/LOG", "GET", &ws_GetLogs);


  // //!Sensor結果資料

  AddWebsocketAPI("/api/PoolData", "GET", &ws_GetAllPoolData);

  // //!蝦池設定相關API

  // AddWebsocketAPI("/api/Pool/(<name>.*)", "DELETE", &ws_DeletePoolInfo);
  // AddWebsocketAPI("/api/Pool/(<name>.*)", "GET", &ws_GetPoolInfo);
  // AddWebsocketAPI("/api/Pool/(<name>.*)", "PATCH", &ws_PatchPoolInfo);
  // AddWebsocketAPI("/api/Pool", "GET", &ws_GetAllPoolInfo);
  // AddWebsocketAPI("/api/Pool", "POST", &ws_AddNewPoolInfo);

  // //? [GET]/api/Pipeline/pool_all_data_get/RUN 這支API比較特別，目前是寫死的
  // //? 目的在於執行時，他會依序執行所有池的資料，每池檢測完後丟出一次NewData
  AddWebsocketAPI("/api/Pipeline/pool_all_data_get/RUN", "GET", &ws_RunAllPoolPipeline);

  // //! 流程設定V2
  AddWebsocketAPI("/api/v2/Pipeline/(<name>.*)/RUN", "GET", &ws_v2_RunPipeline);

  // //! 儀器控制
  AddWebsocketAPI("/api/v2/DeviceCtrl/Spectrophotometer", "GET", &ws_v2_RunPipeline);
  AddWebsocketAPI("/api/v2/DeviceCtrl/PwmMotor", "GET", &ws_v2_RunPwmMotor);
  AddWebsocketAPI("/api/v2/DeviceCtrl/PeristalticMotor", "GET", &ws_v2_RunPeristalticMotor);
  // AddWebsocketAPI("/api/v2/DeviceCtrl/PHmeter", "GET", &ws_v2_RunPipeline);
}

void C_Device_Ctrl::INITWebServer()
{
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");
  ws.onEvent(onWebSocketEvent);
  asyncServer.addHandler(&ws);
  Set_Http_apis(asyncServer);
  INIT_AllWsAPI();
  asyncServer.begin();
}

void C_Device_Ctrl::preLoadWebJSFile()
{
  File JSFile = SPIFFS.open("/assets/index.js.gz", "r");
  webJS_BufferLen = JSFile.size();
  webJS_Buffer = (uint8_t *)malloc(webJS_BufferLen);
  JSFile.read(webJS_Buffer, webJS_BufferLen);
  JSFile.close();
  ESP_LOGD("網頁檔案初始化", "大小: %d", webJS_BufferLen);
}

DynamicJsonDocument C_Device_Ctrl::GetBaseWSReturnData(String MessageString)
{
  DynamicJsonDocument BaseWSReturnData(65525);
  BaseWSReturnData.set(*(Device_Ctrl.JSON__DeviceBaseInfo));
  BaseWSReturnData["firmware_version"].set(FIRMWARE_VERSION);

  int startIndex = MessageString.indexOf("/", 0);
  startIndex = MessageString.indexOf("/", startIndex+1);
  int endIndex = MessageString.indexOf("/", startIndex+1);
  if (startIndex == -1) {
    BaseWSReturnData["cmd"].set(MessageString);
  } else if (endIndex == -1) {
    BaseWSReturnData["cmd"].set(MessageString.substring(startIndex+1, MessageString.length()));
  } else {
    BaseWSReturnData["cmd"].set(MessageString.substring(startIndex+1, endIndex));
  }
  // BaseWSReturnData["internet"].set(Machine_Ctrl.BackendServer.GetWifiInfo());
  BaseWSReturnData["time"].set(GetNowTimeString());
  BaseWSReturnData["utc"] = "+8";
  BaseWSReturnData["action"]["message"].set("看到這行代表API設定時忘記設定本項目了，請通知工程師修正，謝謝");
  BaseWSReturnData["action"]["status"].set("看到這行代表API設定時忘記設定本項目了，請通知工程師修正，謝謝");
  BaseWSReturnData.createNestedObject("parameter");
  BaseWSReturnData["cmd_detail"].set(MessageString);
  BaseWSReturnData["device_status"].set("Idle");
  // if (xSemaphoreTake(Machine_Ctrl.LOAD__ACTION_V2_xMutex, 0) == pdFALSE) {
  //   BaseWSReturnData["device_status"].set("Busy");
  // }
  // else {
  //   xSemaphoreGive(Machine_Ctrl.LOAD__ACTION_V2_xMutex);
  //   BaseWSReturnData["device_status"].set("Idle");
  // }
  return BaseWSReturnData;
}

void OTAServiceTask(void* parameter) {
  ESP_LOGI("OTAService", "準備建立OTA服務");
  ArduinoOTA.setPort(3232);
  ArduinoOTA.onStart([]() {
    Serial.println("OTA starting...");
    // Machine_Ctrl.SetLog(3, "儀器遠端更新中", "", Machine_Ctrl.BackendServer.ws_, NULL);
  });
  ArduinoOTA.onEnd([]() {
    // Machine_Ctrl.SetLog(3, "儀器遠端更新成功，即將重開機", "", Machine_Ctrl.BackendServer.ws_, NULL);
    Serial.println("\nOTA end!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      // Machine_Ctrl.SetLog(1, "儀器遠端更新失敗", "OTA auth failed", Machine_Ctrl.BackendServer.ws_, NULL);
      Serial.println("OTA auth failed");
    } else if (error == OTA_BEGIN_ERROR) {
      // Machine_Ctrl.SetLog(1, "儀器遠端更新失敗", "OTA begin failed", Machine_Ctrl.BackendServer.ws_, NULL);
      Serial.println("OTA begin failed");
    } else if (error == OTA_CONNECT_ERROR) {
      // Machine_Ctrl.SetLog(1, "儀器遠端更新失敗", "OTA connect failed", Machine_Ctrl.BackendServer.ws_, NULL);
      Serial.println("OTA connect failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      // Machine_Ctrl.SetLog(1, "儀器遠端更新失敗", "OTA receive failed", Machine_Ctrl.BackendServer.ws_, NULL);
      Serial.println("OTA receive failed");
    } else if (error == OTA_END_ERROR) {
      // Machine_Ctrl.SetLog(1, "儀器遠端更新失敗", "OTA end failed", Machine_Ctrl.BackendServer.ws_, NULL);
      Serial.println("OTA end failed");
    }
  });
  ArduinoOTA.begin();
  for(;;){
    ArduinoOTA.handle();
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}


void C_Device_Ctrl::CreateOTAService()
{
  xTaskCreatePinnedToCore(
    OTAServiceTask, "TASK__OTAService",
    10000, NULL, (UBaseType_t)TaskPriority::OTAService, &Device_Ctrl.TASK__OTAService, 1
  );
}

void C_Device_Ctrl::UpdateDeviceTimerByNTP()
{
  ESP_LOGI("Time", "Try to update MCU timer");
  timeClient.begin();
  int errorCount = 0;
  while(!timeClient.update()) {
    timeClient.forceUpdate();
    vTaskDelay(100);
    errorCount++;
    if (errorCount > 1000) {
      ESP.restart();
    }
  }
  configTime(28800, 0, "pool.ntp.org");
  setTime((time_t)timeClient.getEpochTime());
  timeClient.end();
  ESP_LOGI("Time", "Time: %s", timeClient.getFormattedTime().c_str());
}

void C_Device_Ctrl::BroadcastLogToClient(AsyncWebSocketClient *client, int Level, const char *content, ...)
{
  char buffer[4096];  // 用于存储格式化后的文本
  va_list ap;
  va_start(ap, content);
  vsprintf(buffer, content, ap);
  va_end(ap);
  DynamicJsonDocument logItem(10000);
  logItem["type"] = "LOG";
  logItem["level"] = Level;
  logItem["content"] = String(buffer);
  String returnString;
  serializeJson(logItem, returnString);
  logItem.clear();
  Serial.println(returnString);
  if (client != NULL) {
    client->binary(returnString);
  }
  else {
    ws.binaryAll(returnString);
  }
}

void C_Device_Ctrl::CreatePipelineFlowScanTask()
{
  xTaskCreatePinnedToCore(
    PipelineFlowScan, 
    "PipelineScan", 
    20000, 
    NULL, 
    (UBaseType_t)TaskPriority::PipelineFlowScan, 
    &TASK__pipelineFlowScan, 
    1
  );
// xTaskCreateStaticPinnedToCore(
//         PipelineFlowScan, "PipelineScan", 20000,(void*)pipelineStackList, (UBaseType_t)TaskPriority::PipelineFlowScan,
//         PipelineFlowScanTask_xStack, &PipelineFlowScanTask_xTaskBuffer, 1
//       );
}

void C_Device_Ctrl::StopNowPipelineAndAllStepTask()
{
  digitalWrite(PIN__EN_BlueSensor, LOW);
  digitalWrite(PIN__EN_GreenSensor, LOW);
  digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
  digitalWrite(PIN__EN_Servo_Motor, LOW);
  Device_Ctrl.peristalticMotorsCtrl.SetAllMotorStop();
  Device_Ctrl.StopNowPipeline = true;
  while (Device_Ctrl.StopNowPipeline) {
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
  for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
    StepTaskDetailList[i].TaskStatus = StepTaskStatus::Close;
  }
}

void C_Device_Ctrl::StopDeviceAllAction()
{
  digitalWrite(PIN__EN_BlueSensor, LOW);
  digitalWrite(PIN__EN_GreenSensor, LOW);
  digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
  digitalWrite(PIN__EN_Servo_Motor, LOW);
  Device_Ctrl.peristalticMotorsCtrl.SetAllMotorStop();
  Device_Ctrl.StopAllPipeline = true;
  Device_Ctrl.StopNowPipeline = true;
  while (Device_Ctrl.StopNowPipeline) {
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
  for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
    StepTaskDetailList[i].TaskStatus = StepTaskStatus::Close;
  }
}

void C_Device_Ctrl::StopAllStepTask()
{
  Device_Ctrl.StopNowPipeline = true;
  while (Device_Ctrl.StopNowPipeline) {
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
  for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
    StepTaskDetailList[i].TaskStatus = StepTaskStatus::Close;
  }
}

void C_Device_Ctrl::CreateStepTasks()
{
  for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
    String TaskName = "StepTask-"+String(i);
    StepTaskDetailList[i].TaskName = TaskName;
    xTaskCreatePinnedToCore(
      StepTask, 
      TaskName.c_str(), 
      15000, 
      (void*)(&StepTaskDetailList[i]), 
      (UBaseType_t)TaskPriority::PiplelineFlowTask_1 - i, 
      &TASKLIST__StepTask[i], 
      1
    );
  }
}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//! 排程功能相關
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

void ScheduleManager(void* parameter)
{ 
  for(;;) {
    ESP_LOGI("", "準備檢查是否有排程設定");
    time_t nowTime = now();
    if ( nowTime < 946656000 ) {
      ESP_LOGW("", "儀器時間有誤，暫時跳過排程設定檢查");
      if (WiFi.isConnected()) {
        // Machine_Ctrl.BackendServer.UpdateMachineTimerByNTP();
        vTaskDelay(5000/portTICK_PERIOD_MS);
      }
      vTaskDelay(1000/portTICK_PERIOD_MS);
      continue;
    }
    //? 每5分鐘檢查一次排程
    if (minute(nowTime) % 5 == 0) {
      int Weekday = weekday(nowTime);
      int Hour = hour(nowTime);
      int Minute = minute(nowTime);

      for (JsonPair ScheduleConfigChose : (*Device_Ctrl.JSON__ScheduleConfig).as<JsonObject>()) {
        String ScheduleConfigID = String(ScheduleConfigChose.key().c_str());
        JsonObject ScheduleConfig = ScheduleConfigChose.value().as<JsonObject>();
        for (JsonVariant weekdayItem : ScheduleConfig["schedule"].as<JsonArray>()[1].as<JsonArray>()) {
          if (weekdayItem.as<int>() == Weekday) {
            for (JsonVariant hourItem : ScheduleConfig["schedule"].as<JsonArray>()[2].as<JsonArray>()) {
              if (hourItem.as<int>() == Hour) {
                for (JsonVariant minuteItem : ScheduleConfig["schedule"].as<JsonArray>()[3].as<JsonArray>()) {
                  if (minuteItem.as<int>() == Minute) {
                    DynamicJsonDocument NewPipelineSetting(60000);
                    int eventCount = 0;
                    for (JsonVariant poolScheduleItem : ScheduleConfig["schedule"].as<JsonArray>()[0].as<JsonArray>()) {
                      int targetIndex = poolScheduleItem.as<int>();
                      String TargetName = "pool_"+String(targetIndex+1)+"_all_data_get.json";
                      String FullFilePath = "/pipelines/"+TargetName;
                      DynamicJsonDocument singlePipelineSetting(10000);
                      singlePipelineSetting["FullFilePath"].set(FullFilePath);
                      singlePipelineSetting["TargetName"].set(TargetName);
                      singlePipelineSetting["stepChose"].set("");
                      singlePipelineSetting["eventChose"].set("");
                      singlePipelineSetting["eventIndexChose"].set(-1);
                      NewPipelineSetting.add(singlePipelineSetting);
                      ESP_LOGD("WebSocket", " - 事件 %d", eventCount+1);
                      ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
                      ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
                    }
                    RunNewPipeline(NewPipelineSetting);
                    break;
                  }
                }
                break;
              }
            }
            break;
          }
        }
      }
    }
    ESP_LOGI("", "排程檢查完畢，等待下一個檢查時段");
    vTaskDelay(1000*60*1/portTICK_PERIOD_MS);
  }
}

void C_Device_Ctrl::CreateScheduleManagerTask()
{
  xTaskCreatePinnedToCore(
    ScheduleManager, 
    "ScheduleManager", 
    10000, 
    NULL, 
    (UBaseType_t)TaskPriority::ScheduleManager, 
    &TASK__ScheduleManager, 
    1
  );
}

void TimeCheckTask(void* parameter)
{ 
  time_t lasUpdatetime = 0;
  for (;;) {
    if (now() < 100000) {
      if (WiFi.isConnected()) {
        Device_Ctrl.UpdateDeviceTimerByNTP();
        lasUpdatetime = now();
        vTaskDelay(10000/portTICK_PERIOD_MS);
      }
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void C_Device_Ctrl::CreateTimerCheckerTask()
{
  xTaskCreatePinnedToCore(
    TimeCheckTask, 
    "TimeCheckTask", 
    10000, 
    NULL, 
    (UBaseType_t)TaskPriority::TimeCheckTask, 
    &TASK__TimerChecker, 
    1
  );
}

C_Device_Ctrl Device_Ctrl;