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
#include <Adafruit_GFX.h>
#include "Adafruit_SH1106.h"
#include "qrcode.h"
#include <HTTPClient.h>

const long  gmtOffset_sec = 3600*8; // GMT+8
const int   daylightOffset_sec = 0; // DST+0
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", gmtOffset_sec, daylightOffset_sec);

AsyncWebServer asyncServer(80);
AsyncWebSocket ws("/ws");

// U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN__SCL_1, PIN__SDA_1);
// U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN__SCL_1, PIN__SDA_1);

Adafruit_SH1106 display(PIN__SDA_1, PIN__SCL_1);

void C_Device_Ctrl::ScanI2C()
{
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    _Wire.beginTransmission(address);
    error = _Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");

      nDevices++;
    }
    else if (error==4)
    {
      Serial.print("Unknown error at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

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
  pinMode(PIN__EN_PH, OUTPUT);
  digitalWrite(PIN__EN_PH, HIGH); 

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

bool C_Device_Ctrl::INIT_oled()
{
  // display._i2caddr = 0x3C;
  display.begin(SH1106_SWITCHCAPVCC, 0x3C, false);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.println("HI");
  display.display();
  
  // u8g2.setBusClock(400000UL);
  // u8g2.begin();
  // u8g2.clearBuffer();
  // u8g2.setFont(u8g2_font_HelvetiPixelOutline_te);
  // u8g2.drawStr(0,10,"Hello World!");
  // u8g2.sendBuffer();
  // u8g2.setFont(u8g2_font_HelvetiPixelOutline_te);
  // u8g2.firstPage();
  // do {
  //   u8g2.setFont(u8g2_font_VCR_OSD_tn);
  //   u8g2.setColorIndex(1); // 設成白色
  //   // u8g2.drawStr(0,13,"Hello World !");
  //   u8g2.drawUTF8(0,13,"Hello World !");
  //   // u8g2.printf("test123%s","testtest");
  // } while ( u8g2.nextPage() );
  return false;
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

    // db_exec(DB_Main, "DROP TABLE logs;");
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

//? 刪除過舊的LOG資訊
//! 目前還在考慮是否真的要加入這功能，有發現SQLITE的DELETE耗時超久，有當機風險
//! 不如把整個Table砍掉重來
void C_Device_Ctrl::DeleteOldLog()
{
  DynamicJsonDocument tempJSONItem(1000);
  String sql = "SELECT min(rowid) AS rowid FROM( SELECT rowid FROM logs ORDER BY rowid DESC LIMIT 1000);";
  db_exec(DB_Main, sql, &tempJSONItem);
  String minId = tempJSONItem[0]["rowid"].as<String>();
  serializeJsonPretty(tempJSONItem, Serial);
  sql = "DELETE FROM logs WHERE rowid < "+minId+";";
  db_exec(DB_Main, sql);
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


  AddWebsocketAPI("/api/GetState", "GET", &ws_GetNowStatus);

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
  BaseWSReturnData["firmware_version"].set(FIRMWARE_VERSION);
  String mode = (*Device_Ctrl.JSON__DeviceBaseInfo)["mode"].as<String>();
  String device_no = (*Device_Ctrl.JSON__DeviceBaseInfo)["device_no"].as<String>();
  BaseWSReturnData["mode"].set(mode);
  BaseWSReturnData["device_no"].set(device_no);
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
    ESP_LOGI("OTA", "儀器遠端更新中");
    Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3, "儀器遠端更新中");
    Device_Ctrl.BroadcastLogToClient(NULL, 3, "儀器遠端更新中");
  });
  ArduinoOTA.onEnd([]() {
    ESP_LOGI("OTA", "儀器遠端更新成功，即將重開機");
    Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3, "儀器遠端更新成功，即將重開機");
    Device_Ctrl.BroadcastLogToClient(NULL, 3, "儀器遠端更新成功，即將重開機");
    // Machine_Ctrl.SetLog(3, "儀器遠端更新成功，即將重開機", "", Machine_Ctrl.BackendServer.ws_, NULL);
    // Serial.println("\nOTA end!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      // Machine_Ctrl.SetLog(1, "儀器遠端更新失敗", "OTA auth failed", Machine_Ctrl.BackendServer.ws_, NULL);
      Serial.println("OTA auth failed");
      ESP_LOGE("OTA", "儀器遠端更新失敗: OTA auth failed");
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "儀器遠端更新失敗: OTA auth failed");
      Device_Ctrl.BroadcastLogToClient(NULL, 1, "儀器遠端更新失敗: OTA auth failed");
    } else if (error == OTA_BEGIN_ERROR) {
      ESP_LOGE("OTA", "儀器遠端更新失敗: OTA begin failed");
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "儀器遠端更新失敗: OTA begin failed");
      Device_Ctrl.BroadcastLogToClient(NULL, 1, "儀器遠端更新失敗: OTA begin failed");
      // Serial.println("OTA begin failed");
    } else if (error == OTA_CONNECT_ERROR) {
      ESP_LOGE("OTA", "儀器遠端更新失敗: OTA connect failed");
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "儀器遠端更新失敗: OTA connect failed");
      Device_Ctrl.BroadcastLogToClient(NULL, 1, "儀器遠端更新失敗: OTA connect failed");
      // Serial.println("OTA connect failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      ESP_LOGE("OTA", "儀器遠端更新失敗: OTA receive failed");
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "儀器遠端更新失敗: OTA receive failed");
      Device_Ctrl.BroadcastLogToClient(NULL, 1, "儀器遠端更新失敗: OTA receive failed");
      // Serial.println("OTA receive failed");
    } else if (error == OTA_END_ERROR) {
      ESP_LOGE("OTA", "儀器遠端更新失敗: OTA end failed");
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "儀器遠端更新失敗: OTA end failed");
      Device_Ctrl.BroadcastLogToClient(NULL, 1, "儀器遠端更新失敗: OTA end failed");
      // Serial.println("OTA end failed");
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
  // DeleteOldLog();
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

void C_Device_Ctrl::SendLineNotifyMessage(String content)
{
  String LINE_Notify_id = (*Device_Ctrl.JSON__DeviceBaseInfo)["LINE_Notify_id"].as<String>();
  bool LINE_Notify_switch = (*Device_Ctrl.JSON__DeviceBaseInfo)["LINE_Notify_switch"].as<bool>();
  if (LINE_Notify_id != "" & LINE_Notify_switch) {
    HTTPClient http;
    http.begin("https://notify-api.line.me/api/notify");
    http.addHeader("Host", "notify-api.line.me");
    String AuthorizationInfo = "Bearer "+LINE_Notify_id;
    http.addHeader("Authorization", AuthorizationInfo);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Connection", "close");
    String payload = "message="+content;
    http.POST(payload);
  }

  // #ifdef LINE_NOTIFY_KEY
  // #ifdef LINE_NOTIFY_API
  // HTTPClient http;
  // http.begin(LINE_NOTIFY_API);;
  // DynamicJsonDocument postData(10000);
  // postData["key"] = LINE_NOTIFY_KEY;
  // postData["content"] = content;
  // String sendContent;
  // serializeJson(postData, sendContent);
  // postData.clear();
  // int httpResponseCode = http.POST(sendContent);
  // http.end();
  // #endif
  // #endif
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
    // ESP_LOGI("", "準備檢查是否有排程設定");
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
    if (minute(nowTime) == 0) {
      int Hour = hour(nowTime);
      String targetString = (*Device_Ctrl.JSON__ScheduleConfig)[Hour].as<String>();
      if (targetString != "-") {
        DynamicJsonDocument NewPipelineSetting(60000);
        int eventCount = 0;
        String TargetName = targetString+".json";
        String FullFilePath = "/pipelines/"+TargetName;
        DynamicJsonDocument singlePipelineSetting(10000);
        singlePipelineSetting["FullFilePath"].set(FullFilePath);
        singlePipelineSetting["TargetName"].set(TargetName);
        singlePipelineSetting["stepChose"].set("");
        singlePipelineSetting["eventChose"].set("");
        singlePipelineSetting["eventIndexChose"].set(-1);
        NewPipelineSetting.add(singlePipelineSetting);
        RunNewPipeline(NewPipelineSetting);

        // for (JsonVariant poolScheduleItem : ScheduleConfig["schedule"].as<JsonArray>()[0].as<JsonArray>()) {
        //   int targetIndex = poolScheduleItem.as<int>();
        //   String TargetName = "pool_"+String(targetIndex+1)+"_all_data_get.json";
        //   String FullFilePath = "/pipelines/"+TargetName;
        //   DynamicJsonDocument singlePipelineSetting(10000);
        //   singlePipelineSetting["FullFilePath"].set(FullFilePath);
        //   singlePipelineSetting["TargetName"].set(TargetName);
        //   singlePipelineSetting["stepChose"].set("");
        //   singlePipelineSetting["eventChose"].set("");
        //   singlePipelineSetting["eventIndexChose"].set(-1);
        //   NewPipelineSetting.add(singlePipelineSetting);
        //   ESP_LOGD("WebSocket", " - 事件 %d", eventCount+1);
        //   ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
        //   ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
        // }
        // RunNewPipeline(NewPipelineSetting);
      }
      ESP_LOGI("", "排程檢查完畢，等待下一個檢查時段");
      vTaskDelay(1000*60*30/portTICK_PERIOD_MS);
    } 

    // if (minute(nowTime) % 5 == 0) {
    //   int Weekday = weekday(nowTime);
    //   int Hour = hour(nowTime);
    //   int Minute = minute(nowTime);
    //   for (JsonPair ScheduleConfigChose : (*Device_Ctrl.JSON__ScheduleConfig).as<JsonObject>()) {
    //     String ScheduleConfigID = String(ScheduleConfigChose.key().c_str());
    //     JsonObject ScheduleConfig = ScheduleConfigChose.value().as<JsonObject>();
    //     for (JsonVariant weekdayItem : ScheduleConfig["schedule"].as<JsonArray>()[1].as<JsonArray>()) {
    //       if (weekdayItem.as<int>() == Weekday) {
    //         for (JsonVariant hourItem : ScheduleConfig["schedule"].as<JsonArray>()[2].as<JsonArray>()) {
    //           if (hourItem.as<int>() == Hour) {
    //             for (JsonVariant minuteItem : ScheduleConfig["schedule"].as<JsonArray>()[3].as<JsonArray>()) {
    //               if (minuteItem.as<int>() == Minute) {
    //                 DynamicJsonDocument NewPipelineSetting(60000);
    //                 int eventCount = 0;
    //                 for (JsonVariant poolScheduleItem : ScheduleConfig["schedule"].as<JsonArray>()[0].as<JsonArray>()) {
    //                   int targetIndex = poolScheduleItem.as<int>();
    //                   String TargetName = "pool_"+String(targetIndex+1)+"_all_data_get.json";
    //                   String FullFilePath = "/pipelines/"+TargetName;
    //                   DynamicJsonDocument singlePipelineSetting(10000);
    //                   singlePipelineSetting["FullFilePath"].set(FullFilePath);
    //                   singlePipelineSetting["TargetName"].set(TargetName);
    //                   singlePipelineSetting["stepChose"].set("");
    //                   singlePipelineSetting["eventChose"].set("");
    //                   singlePipelineSetting["eventIndexChose"].set(-1);
    //                   NewPipelineSetting.add(singlePipelineSetting);
    //                   ESP_LOGD("WebSocket", " - 事件 %d", eventCount+1);
    //                   ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
    //                   ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
    //                 }
    //                 RunNewPipeline(NewPipelineSetting);
    //                 break;
    //               }
    //             }
    //             break;
    //           }
    //         }
    //         break;
    //       }
    //     }
    //   }
    // }
    
    // ESP_LOGI("", "排程檢查完畢，等待下一個檢查時段");
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

void C_Device_Ctrl::AddNewOledLog(const char* content, ...)
{
  char buffer[22];
  va_list ap;
  va_start(ap, content);
  vsnprintf(buffer, 21,content, ap);
  va_end(ap);
  (*JSON__oledLogList).add(String(buffer));
  while ((*JSON__oledLogList).size()>8) {
    (*JSON__oledLogList).remove(0);
  }
  display.clearDisplay();
  display.setCursor(0,0);
  for (int i = 0; i < (*JSON__oledLogList).size(); i++) {
    String item = (*JSON__oledLogList)[i].as<String>(); // 使用 as<String>() 將元素轉換為 String
    display.println(item);
  }
  display.display();
}

void OledQRCode(void* parameter)
{ 
  String IPStringSave="";
  int ipList[4];
  for(;;) {
    if (WiFi.isConnected()) {
      String IpString = WiFi.localIP().toString();
      if (IpString == IPStringSave) {
        vTaskDelay(1000/portTICK_PERIOD_MS);
        continue;
      } else {
        IPStringSave = IpString;
      }
      String delimiter = ".";
      byte x0 = 3 + 64;
      byte y0 = 3;
      QRCode qrcode;
      uint8_t qrcodeData[qrcode_getBufferSize(3)];
      qrcode_initText(&qrcode, qrcodeData, 3, 0, ("http://" + IpString).c_str());
      int delimiterIndex = IpString.indexOf(delimiter);
      int rowChose = 0;
      while (delimiterIndex != -1) {
        ipList[rowChose] =  IpString.substring(0, delimiterIndex).toInt();
        IpString = IpString.substring(delimiterIndex+1);
        delimiterIndex = IpString.indexOf(delimiter);
        rowChose++;
      }
      ipList[3] = IpString.toInt();
      display.clearDisplay();
      display.setCursor(0,0);
      display.setTextSize(2);
      display.printf("%d.\n",ipList[0]);
      display.printf("%d.\n",ipList[1]);
      display.printf("%d.\n",ipList[2]);
      display.printf("%d",ipList[3]);
      
      // Serial.println(qrcode.size);
      for (uint8_t y = 0; y < qrcode.size; y++)
      {
        for (uint8_t x = 0; x < qrcode.size; x++)
        {
          if (qrcode_getModule(&qrcode, x, y))
          {
            display.drawPixel(64+3+x*2,3+y*2,0);
            display.drawPixel(64+3+x*2+1,3+y*2,0);
            display.drawPixel(64+3+x*2,3+y*2+1,0);
            display.drawPixel(64+3+x*2+1,3+y*2+1,0);
            // u8g2.setColorIndex(0);
          }
          else
          {
            display.drawPixel(64+3+x*2,3+y*2,1);
            display.drawPixel(64+3+x*2+1,3+y*2,1);
            display.drawPixel(64+3+x*2,3+y*2+1,1);
            display.drawPixel(64+3+x*2+1,3+y*2+1,1);
            // u8g2.setColorIndex(1);
          }
          display.drawRect(64,0,64,64,1);
          display.drawRect(65,1,62,62,1);
          // u8g2.drawBox(x0 + x * 2, y0 + y * 2, 2, 2);
        }
      }
      display.display();
    }
  }
  vTaskDelay(1000/portTICK_PERIOD_MS);
}


void C_Device_Ctrl::CreateOledQRCodeTask()
{
  xTaskCreatePinnedToCore(
    OledQRCode, 
    "OledQRCode", 
    10000, 
    NULL, 
    (UBaseType_t)TaskPriority::OLEDCheckTask, 
    &TASK__OledQRCode, 
    1
  );
}

C_Device_Ctrl Device_Ctrl;