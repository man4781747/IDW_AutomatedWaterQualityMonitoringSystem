#include "MAIN__DeviceCtrl.h"
#include <esp_log.h>
#include <SD.h>
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
#include <ESP_Mail_Client.h>
#include "mbedtls/aes.h"
#include <Update.h>
#include <ModbusRTU.h>
#include <ESP32Ping.h>

ModbusRTU mb_;

const long  gmtOffset_sec = 3600*8; // GMT+8
const int   daylightOffset_sec = 0; // DST+0
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", gmtOffset_sec, daylightOffset_sec);

// web server 主物件
AsyncWebServer asyncServer(80);
// websocket service 控制物件
AsyncWebSocket ws("/ws");
AsyncWebSocket ws_nodered("/ws/NodeRed");

// led 螢幕控制物件
Adafruit_SH1106 display(PIN__SDA_1, PIN__SCL_1);


/**
 * @brief 測試用 - 掃描當前所有 I2C 裝置
 * 
 */
void C_Device_Ctrl::ScanI2C()
{
  byte error, address;
  int nDevices;

  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
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


/**
 * @brief 初始化各個PIN的初始化狀態
 * 
 */
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

  // pinMode(PIN__Step_Motor_EN, OUTPUT);
  // digitalWrite(PIN__Step_Motor_EN, LOW);
  // pinMode(PIN__Step_Motor_STEP, OUTPUT);
  // digitalWrite(PIN__Step_Motor_STEP, LOW);
  // pinMode(PIN__Step_Motor_DIR, OUTPUT);
  // digitalWrite(PIN__Step_Motor_DIR, LOW);
}

/**
 * @brief 設定 Device Ctrl 用的 I2C Wire
 * 
 * @param WireChose 
 */
void C_Device_Ctrl::INIT_I2C_Wires(TwoWire &WireChose)
{
  _Wire = WireChose;
  _Wire.begin(PIN__SDA_1, PIN__SCL_1, 800000UL);
}

/**
 * @brief 初始化 - 讀取最新 PoolData 檔案
 * 如果讀取失敗，則會重新建立一個初始檔案，所有池最新資料為 1990-01-01，數值皆為-1
 * 
 * @return true 
 * @return false 
 */
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

/**
 * @brief 初始化 - 初始化 SD 卡，如果讀取不到 SD 卡則會自動重開機
 * 
 * @return true 
 * @return false 
 */
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

/**
 * @brief 初始化 - 初始化 led 螢幕
 * 
 * @return true 
 * @return false 
 */
bool C_Device_Ctrl::INIT_oled()
{
  display.begin(SH1106_SWITCHCAPVCC, 0x3C, false);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.println("HI");
  display.display();
  return false;
}

bool C_Device_Ctrl::CheckUpdate()
{
  if (SD.exists("/firmware/firmware.bin")) {
    File firmware =  SD.open("/firmware/firmware.bin");
    if (firmware) {
      ESP_LOGI("","找到新的韌體檔案，嘗試更新");
      Device_Ctrl.AddNewOledLog("Update Firmware");
      Device_Ctrl.AddNewOledLog("Update: 0%");
      Update.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA progress: %u%%\r", (progress / (total / 100)));
        Device_Ctrl.ChangeLastOledLog("Update: %u%%\r", (progress / (total / 100)));
      });
      Update.begin(firmware.size(), U_FLASH);
      Update.writeStream(firmware);
      if (Update.end()){
        Serial.println(F("Update finished!"));
        Device_Ctrl.AddNewOledLog("Update finished!");
        firmware.close();
      } else {
        Device_Ctrl.AddNewOledLog("Update error!: %d", Update.getError());
        Serial.println(F("Update error!"));
        Serial.println(Update.getError());
        firmware.close();
      }
      SD.remove("/firmware/firmware.bin");
      // if (SD.rename("/firmware/firmware.bin", "/firmware/firmware.bak")){
      //   Serial.println(F("Firmware rename succesfully!"));
      // }else{
      //   Serial.println(F("Firmware rename error!"));
      // }
      Device_Ctrl.AddNewOledLog("ReStart in 2s");
      delay(2000);
      ESP.restart();
    }
  }
  return true;
}

/**
 * @brief 初始化 - 初始化 Sqlite DB
 * 目前有 2 DB 檔案:
 *  1. "/mainDB.db" : 負責記錄 sensor 資料
 *  2. "/logDB.db"  : 負責記錄 log 以及 儀器歷程 資料
 * 
 * @return int 
 */
int C_Device_Ctrl::INIT_SqliteDB()
{
  // SD.remove("/mainDB.db");
  sqlite3_initialize();
  int rc = sqlite3_open(FilePath__SD__MainDB.c_str(), &DB_Main);
  if (rc) {
    ESP_LOGE("DB", "Can't open sensor database: %s", sqlite3_errmsg(DB_Main));
    return rc;
  } else {
    ESP_LOGV("DB", "Opened sensor database successfully");
    db_exec(DB_Main, "CREATE TABLE sensor ( time TEXT, pool TEXT , value_name TEXT , result REAL );");
  }

  rc = sqlite3_open(FilePath__SD__LogDB.c_str(), &DB_Log);
  if (rc) {
    ESP_LOGE("DB", "Can't open log database: %s", sqlite3_errmsg(DB_Log));
    return rc;
  } else {
    ESP_LOGV("DB", "Opened log database successfully");
    db_exec(DB_Log, "CREATE TABLE logs ( time TEXT, level INTEGER, content TEXT );");
    db_exec(DB_Log, "CREATE TABLE used ( time INTEGER, item TEXT, count INTEGER );");
  }
  return rc;
}


/**
 * @brief 刪除 logDB.db 檔案，並重建他
 * 如果 log 資料庫過大讓機器沒法運行才需要執行這個 function
 * 
 * @return int 
 */
int C_Device_Ctrl::DropLogsTable()
{
  ESP_LOGV("DB", "Opened log database successfully");
  sqlite3_close(DB_Log);
  SD.remove("/logDB.db");
  sqlite3_open(FilePath__SD__LogDB.c_str(), &DB_Log);
  db_exec(DB_Log, "CREATE TABLE logs ( time TEXT, level INTEGER, content TEXT );");
  db_exec(DB_Log, "CREATE TABLE used ( time INTEGER, item TEXT, count INTEGER );");
  return 0;
}


/**
 * @brief 初始化用 - 讀取各個 json 格式設定檔
 * 
 */
void C_Device_Ctrl::LoadConfigJsonFiles()
{
  LoadDeviceBaseInfoJSONFile();
  ExFile_LoadJsonFile(SD, FilePath__SD__SpectrophotometerConfig, *JSON__SpectrophotometerConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__PHmeterConfig, *JSON__PHmeterConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__PoolConfig, *JSON__PoolConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__ServoConfig, *JSON__ServoConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__PeristalticMotorConfig, *JSON__PeristalticMotorConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__ScheduleConfig, *JSON__ScheduleConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__DeviceConfig, *JSON__DeviceConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__WiFiConfig, *JSON__WifiConfig);
  ExFile_LoadJsonFile(SD, FilePath__SD__ItemUseCount, *JSON__ItemUseCount);
  ExFile_LoadJsonFile(SD, FilePath__SD__RO_Result, *JSON__RO_Result);
  ExFile_LoadJsonFile(SD, FilePath__SD__Consume, *JSON__Consume);
  ExFile_LoadJsonFile(SD, FilePath__SD__Maintain, *JSON__Maintain);
}

void C_Device_Ctrl::LoadDeviceBaseInfoJSONFile(bool rebuild=false)
{
  if (rebuild) {
    (*JSON__DeviceBaseInfo).clear();
  } else {
    ExFile_LoadJsonFile(SD, FilePath__SD__DeviceBaseInfo, *JSON__DeviceBaseInfo);
  }
  //? 檢查各參數是否存在，不存在則使用預設值
  bool anyChange = false;
  if ((*JSON__DeviceBaseInfo)["device_no"] == nullptr) {
    (*JSON__DeviceBaseInfo)["device_no"].set("ID_PoolSensor");
    anyChange = true;
  }
  if ((*JSON__DeviceBaseInfo)["LINE_Notify_switch"] == nullptr) {
    (*JSON__DeviceBaseInfo)["LINE_Notify_switch"].set(false);
    anyChange = true;
  }
  if ((*JSON__DeviceBaseInfo)["LINE_Notify_id"] == nullptr) {
    (*JSON__DeviceBaseInfo)["LINE_Notify_id"].set("");
    anyChange = true;
  }
  if ((*JSON__DeviceBaseInfo)["Mail_Notify_switch"] == nullptr) {
    (*JSON__DeviceBaseInfo)["Mail_Notify_switch"].set(false);
    anyChange = true;
  }
  if ((*JSON__DeviceBaseInfo)["Mail_Notify_Auther"] == nullptr) {
    (*JSON__DeviceBaseInfo)["Mail_Notify_Auther"].set("");
    anyChange = true;
  }
  if ((*JSON__DeviceBaseInfo)["Mail_Notify_Key"] == nullptr) {
    (*JSON__DeviceBaseInfo)["Mail_Notify_Key"].set("");
    anyChange = true;
  }
  if ((*JSON__DeviceBaseInfo)["Mail_Notify_Target"] == nullptr) {
    (*JSON__DeviceBaseInfo)["Mail_Notify_Target"].set("");
    anyChange = true;
  }
  if ((*JSON__DeviceBaseInfo)["schedule_switch"] == nullptr) {
    (*JSON__DeviceBaseInfo)["schedule_switch"].set(true);
    anyChange = true;
  }
  if (anyChange) {
    ExFile_WriteJsonFile(SD, FilePath__SD__DeviceBaseInfo, *JSON__DeviceBaseInfo);
  }
}



/**
 * @brief 對 sensor DB Insert 一筆新的資料 
 * 
 * @param time        格式: YYYY-mm-dd HH:MM:SS
 * @param pool        目標名稱 ex: "pool-1" or "test" or "RO"
 * @param ValueName   資料名稱 ex: "NO2_wash_volt" or "NH4_test_volt"
 * @param result      資料數值
 */
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

  
  // String FileSavePath = String("/datas/")+String(GetDateString("-"))+String(".csv");
  // ExFile_CreateFile(SD, FileSavePath);
  // File SaveFile = SD.open(FileSavePath, FILE_APPEND);
  // SaveFile.printf(
  //   "%s,%s,%s,%s\n", time.c_str(), pool.c_str(), ValueName.c_str(), String(result,2).c_str()
  // );
  // SaveFile.close();
}

/**
 * @brief 對 log DB Insert 一筆新的資料
 * 
 * @param time      格式: YYYY-mm-dd HH:MM:SS
 * @param level     log level : 0: DEBUG, 1: ERROR, 2: WARNING, 3: INFO, 4: DETAIL, 5: SUCCESS, 6: FAIL
 * @param content   log content format
 * @param ...       log content 
 */
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
  db_exec(DB_Log, SqlString);
}

int C_Device_Ctrl::mappingPoolNameToID(String poolName)
{
  if (poolName == "pool-1") {return (int)SensorData_Pool::Pool1;}
  else if (poolName == "pool-2") {return (int)SensorData_Pool::Pool2;}
  else if (poolName == "pool-3") {return (int)SensorData_Pool::Pool3;}
  else if (poolName == "pool-4") {return (int)SensorData_Pool::Pool4;}
  else if (poolName == "RO") {return (int)SensorData_Pool::RO;}
  else if (poolName == "test") {return (int)SensorData_Pool::test;}
  else {return (int)SensorData_Pool::unknow;}
}

int C_Device_Ctrl::mappingTypeNameToID(String typeName)
{
  if (typeName == "NO2_test_volt") {return (int)SensorData_Type::NO2_test_volt;}
  else if (typeName == "NO2_wash_volt") {return (int)SensorData_Type::NO2_wash_volt;}
  else if (typeName == "NO2") {return (int)SensorData_Type::NO2;}
  else if (typeName == "NH4_test_volt") {return (int)SensorData_Type::NH4_test_volt;}
  else if (typeName == "NH4_wash_volt") {return (int)SensorData_Type::NH4_wash_volt;}
  else if (typeName == "NH4") {return (int)SensorData_Type::NH4;}
  else if (typeName == "pH_volt") {return (int)SensorData_Type::pH_volt;}
  else if (typeName == "pH") {return (int)SensorData_Type::pH;}
  else {return (int)SensorData_Type::unknow;}
}

void C_Device_Ctrl::SaveSensorDataToBinFile(time_t time, String pool, String type, int value)
{
  SensorData_Short data_short;
  //? 為縮減資料大小，unix只取當日秒數時間
  data_short.parts.day_time = time % 86400;
  //? 為縮減資料大小，value 只取 0 ~ 32768 間的整數數值
  data_short.parts.value = value>32767?32767:value;
  //? 儲存檔案名稱格式為: (資料日期:YYYYMMDD)__(Pool名稱)__(資料類型名稱).bin
  struct tm *DateTime = localtime(&time);
  char buffer[9];
  strftime(buffer, sizeof(buffer), "%Y%m%d", DateTime);
  String FileSavePath = "/datas/"+String(buffer)+"/"+pool+"/"+type+".bin";
  ExFile_CreateFile(SD, FileSavePath);
  File SaveFile = SD.open(FileSavePath, FILE_APPEND);
  SaveFile.write((uint8_t *)&(data_short.value), 4);
  SaveFile.close();
}


//! 維護項目紀錄JSON檔案重設
void C_Device_Ctrl::RebuildMaintainJSON()
{
  (*Device_Ctrl.JSON__Maintain).clear();
  (*Device_Ctrl.JSON__Maintain)["pH"]["time"].set(GetDatetimeString());
  ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__Maintain, *Device_Ctrl.JSON__Maintain);
}

/**
 * @brief 更新 Pipeline 資料列表
 * ! 盡量別在初始化以外的場合使用，速度很慢
 * 如果有列表項目重載、新增、刪除，請用其他的 function
 */
void C_Device_Ctrl::UpdatePipelineConfigList()
{
  ESP_LOGD("", "準備重建 Pipeline 列表");
  (*JSON__PipelineConfigList).clear();
  File folder = SD.open("/pipelines");
  while (true) {
    File Entry = folder.openNextFile();
    if (! Entry) {
      break;
    }
    String FileName = String(Entry.name());
    if (FileName == "__temp__.json") {
      Entry.close();
      continue;
    }
    ESP_LOGD("", "準備讀取設定檔: %s", FileName.c_str());
    DynamicJsonDocument fileInfo(500);
    fileInfo["size"].set(Entry.size());
    fileInfo["name"].set(FileName);
    fileInfo["getLastWrite"].set(Entry.getLastWrite());
    DynamicJsonDocument fileContent(1024*50);
    DeserializationError error = deserializeJson(fileContent, Entry);
    Entry.close();
    if (error) {
      ESP_LOGD("", "設定檔 %s 讀取失敗: %s", FileName.c_str(), error.c_str());
      continue;
    }
    ESP_LOGD("", "設定檔讀取完畢: %s", FileName.c_str());
    fileInfo["title"].set(fileContent["title"].as<String>());
    fileInfo["desp"].set(fileContent["desp"].as<String>());
    fileInfo["tag"].set(fileContent["tag"].as<JsonArray>());
    (*JSON__PipelineConfigList)[FileName] = fileInfo;
    // (*JSON__PipelineConfigList).add(fileInfo);
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
  ESP_LOGD("", "重建 Pipeline 列表完畢");
}

/**
 * @brief 刪除指定 Pipeline檔案並將其移除出列表
 * 
 */
void C_Device_Ctrl::RemovePipelineConfig(String FileName)
{
  String FullFilePath = "/pipelines/" + FileName;
  SD.remove(FullFilePath);
  (*JSON__PipelineConfigList).remove(FileName);
}


/**
 * @brief 重新讀取指定的 pipeline config JSON 檔案並更新列表內容
 * 
 * @param FileName 
 */
int C_Device_Ctrl::UpdateOnePipelineConfig(String FileName)
{
  String FullFilePath = "/pipelines/" + FileName;
  if (SD.exists(FullFilePath) == false) {
    return 1;
  }
  File file = SD.open(FullFilePath, FILE_READ);
  if (!file) {return 2;}

  DynamicJsonDocument fileContent(1024*50);
  DeserializationError error = deserializeJson(fileContent, file);
  if (error) {
    ESP_LOGD("", "設定檔 %s 讀取失敗: %s", FileName.c_str(), error.c_str());
    file.close();
    return 3;
  }
  ESP_LOGD("", "設定檔讀取完畢: %s", FileName.c_str());
  DynamicJsonDocument fileInfo(500);
  fileInfo["size"].set(file.size());
  fileInfo["getLastWrite"].set(file.getLastWrite());
  file.close();
  fileInfo["name"].set(FileName);
  fileInfo["title"].set(fileContent["title"].as<String>());
  fileInfo["desp"].set(fileContent["desp"].as<String>());
  fileInfo["tag"].set(fileContent["tag"].as<JsonArray>());
  (*JSON__PipelineConfigList)[FileName] = fileInfo;
  return 0;
}

//! WiFi相關功能

/**
 * @brief 
 * 
 */
void C_Device_Ctrl::ConnectWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  // WiFi.setAutoReconnect(false);
  // Serial.println(WiFi.getAutoConnect());
  // WiFi.begin(
  //   (*JSON__WifiConfig)["Remote"]["remote_Name"].as<String>().c_str(),
  //   (*JSON__WifiConfig)["Remote"]["remote_Password"].as<String>().c_str()
  // );
  IPAddress AP_IP;
  AP_IP.fromString((*JSON__WifiConfig)["AP"]["AP_IP"].as<String>());
  IPAddress AP_gateway;
  AP_gateway.fromString((*JSON__WifiConfig)["AP"]["AP_gateway"].as<String>());
  IPAddress AP_subnet_mask;
  AP_subnet_mask.fromString((*JSON__WifiConfig)["AP"]["AP_subnet_mask"].as<String>());


  WiFi.softAPConfig(AP_IP,AP_gateway,AP_subnet_mask);
  WiFi.softAP(
    (*JSON__WifiConfig)["AP"]["AP_Name"].as<String>()
  );
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("softAP macAddress: ");
  Serial.println(WiFi.softAPmacAddress());
  Serial.println("Server started.....");

  // WiFi.setAutoReconnect(true);
  // WiFi.setAutoConnect(true);
  // WiFi.setAutoConnect(false);
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

  AddWebsocketAPI("/api/GetState", "GET", &ws_GetNowStatus);

  // //!Sensor結果資料
  AddWebsocketAPI("/api/GetSensorData", "GET", &ws_GetSensorData);
  AddWebsocketAPI("/api/PoolData", "GET", &ws_GetAllPoolData);
  AddWebsocketAPI("/api/PoolDataForWeb", "GET", &ws_GetAllPoolData_ForWeb);


  // //? [GET]/api/Pipeline/pool_all_data_get/RUN 這支API比較特別，目前是寫死的
  // //? 目的在於執行時，他會依序執行所有池的資料，每池檢測完後丟出一次NewData
  AddWebsocketAPI("/api/Pipeline/pool_all_data_get/RUN", "GET", &ws_RunAllPoolPipeline);

  // //! 流程設定V2
  AddWebsocketAPI("/api/v2/Pipeline/(<name>.*)/RUN", "GET", &ws_v2_RunPipeline);

  // //! 儀器控制
  AddWebsocketAPI("/api/v2/DeviceCtrl/Spectrophotometer", "GET", &ws_v2_RunPipeline);
  AddWebsocketAPI("/api/v2/DeviceCtrl/PwmMotor", "GET", &ws_v2_RunPwmMotor);
  AddWebsocketAPI("/api/v2/DeviceCtrl/PeristalticMotor", "GET", &ws_v2_RunPeristalticMotor);



}

void C_Device_Ctrl::INITWebServer()
{
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");
  // DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Expose-Headers", "*");
  ws.onEvent(onWebSocketEvent);
  asyncServer.addHandler(&ws);
  ws_nodered.onEvent(onWebSocketEvent);
  asyncServer.addHandler(&ws_nodered);
  Set_Http_apis(asyncServer);
  INIT_AllWsAPI();
  asyncServer.begin();
}

void C_Device_Ctrl::preLoadWebJSFile()
{
  ESP_LOGD("網頁檔案初始化", "準備執行js預讀取步驟");
  File JSFile = SD.open("/assets/index.js.gz", "r"); 
  webJS_BufferLen = JSFile.size();
  if (webJS_BufferLen == 0) {
    ESP_LOGD("網頁檔案初始化", "無法讀取SD中的網頁檔案");
  }
  else {
    ESP_LOGD("網頁檔案初始化", "大小: %d", webJS_BufferLen);
    free(webJS_Buffer);
    webJS_Buffer = (uint8_t *)malloc(webJS_BufferLen);
    JSFile.read(webJS_Buffer, webJS_BufferLen);
    JSFile.close();
  }
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
  if (IsDeviceIdle()) {
    BaseWSReturnData["device_status"].set("Idle");
  }
  else {
    String PipelineName = (*Device_Ctrl.JSON__pipelineConfig)["title"].as<String>();
    BaseWSReturnData["device_status"].set(PipelineName);
  }

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
  int UpdateStatus = 0;
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
    //! ArduinoOTA 服務檢查
    ArduinoOTA.handle();
    //! File OTA 檢查
    if (Device_Ctrl.CheckUpdateFile == true) {
      Device_Ctrl.CheckUpdateFile = false;
      Device_Ctrl.BroadcastLogToClient(NULL, 3,"準備更新儀器");
      ESP_LOGI("","準備更新儀器");
      Device_Ctrl.AddNewOledLog("Update Firmware");
      Device_Ctrl.AddNewOledLog("Update: 0%");
      UpdateStatus = 0;
      Update.onProgress([&](unsigned int progress, unsigned int total) {
        Serial.printf("OTA progress: %u%%\r", (progress / (total / 100)));
        Device_Ctrl.ChangeLastOledLog("Update: %u%%\r", (progress / (total / 100)));
        int NowStatus = (int)(progress / (total / 100))/10*10;
        if (UpdateStatus != NowStatus) {
          UpdateStatus = NowStatus;
          Device_Ctrl.BroadcastLogToClient(NULL, 3, "韌體更新中: %d %%",UpdateStatus);
        };
      });
      Update.begin(Device_Ctrl.firmwareLen, U_FLASH);
      Update.write(Device_Ctrl.firmwareBuffer, Device_Ctrl.firmwareLen);
      if (Update.end()){
        Device_Ctrl.BroadcastLogToClient(NULL, 3,"韌體更新成功，2秒後重啟");
        Serial.println(F("Update finished!"));
        Device_Ctrl.AddNewOledLog("Update finished!");
        Device_Ctrl.AddNewOledLog("ReStart in 2s");
        vTaskDelay(2000/portTICK_PERIOD_MS);
        ESP.restart();
      } else {
        free(Device_Ctrl.firmwareBuffer);
        Device_Ctrl.BroadcastLogToClient(NULL, 1,"韌體更新失敗");
        Device_Ctrl.AddNewOledLog("Update error!: %d", Update.getError());
        Serial.println(F("Update error!"));
        Serial.println(Update.getError());
      }
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}


void C_Device_Ctrl::CreateOTAService()
{
  xTaskCreatePinnedToCore(
    OTAServiceTask, 
    TaskSettingMap["OTAService"].TaskName,
    TaskSettingMap["OTAService"].stack_depth, 
    NULL, 
    (UBaseType_t)TaskSettingMap["OTAService"].task_priority,
    TaskSettingMap["OTAService"].task_handle, 
    1
  );

  // xTaskCreatePinnedToCore(
  //   OTAServiceTask, "TASK__OTAService",
  //   1024*10, NULL, (UBaseType_t)TaskPriority::OTAService, &Device_Ctrl.TASK__OTAService, 1
  // );
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
  if (IsDeviceIdle() == true) {
    logItem["status"] = "Idle";
  } else {
    if (Device_Ctrl.StopNowPipeline == true) {
      logItem["status"] = "儀器運行終止中";
    } else {
      String NowPiplineName = (*Device_Ctrl.JSON__pipelineConfig)["title"].as<String>();
      if (NowPiplineName == "null") {
        logItem["status"] = "準備運行中";
      }
      else {
        logItem["status"] = NowPiplineName;
      }
    }
  }
  String returnString;
  serializeJson(logItem, returnString);
  logItem.clear();
  if (client != NULL) {
    //! 若有指定 client 回傳需檢查此 client 是否在 NodeRed 專線
    //! 若為專線，則要廣撥給專線所有人得知
    //? 2024-08-28 NodeRed端需求
    //? 希望開一個群組，群組收到訊息時，所有的回傳訊息是傳給原始訊號端以外的所有人
    if (String(client->server()->url()) == "/ws/NodeRed") {
      for(const auto& c: client->server()->getClients()){
        if(c->status() == WS_CONNECTED & c->id() != client->id()) {
          c-> binary(returnString);
        }
      }
    } else {
      //? 如果不是專線，則誰提需求就回傳給誰
      client->binary(returnString);
    }
  } else {
    ws.binaryAll(returnString);
    ws_nodered.binaryAll(returnString);
  }
}

DynamicJsonDocument C_Device_Ctrl::GetWebsocketConnectInfo()
{
  DynamicJsonDocument returnData(10000);
  for (AsyncWebSocketClient *clientChose : ws.getClients()) {
    DynamicJsonDocument singleItem(1000);
    singleItem["remoteIP"] = clientChose->remoteIP().toString();
    singleItem["remotePort"] = clientChose->remotePort();
    singleItem["canSend"] = clientChose->canSend();
    singleItem["status"] = clientChose->status();
    returnData.add(singleItem);
  }
  return returnData;
}

/**
 * @brief WiFi Manager 設定
 * 
 * @param parameter 
 */
void WifiManager(void* parameter)
{ 
  WiFi.disconnect(); // 先斷開所有連接

  long CONNECT_TIMEOUT = 10*60*1000;
  time_t lastConnectTime = now();
  WiFi.mode(WIFI_AP_STA);

  //? 先建立 AP 熱點
  IPAddress AP_IP;
  AP_IP.fromString((*Device_Ctrl.JSON__WifiConfig)["AP"]["AP_IP"].as<String>());
  IPAddress AP_gateway;
  AP_gateway.fromString((*Device_Ctrl.JSON__WifiConfig)["AP"]["AP_gateway"].as<String>());
  IPAddress AP_subnet_mask;
  AP_subnet_mask.fromString((*Device_Ctrl.JSON__WifiConfig)["AP"]["AP_subnet_mask"].as<String>());
  WiFi.softAPConfig(AP_IP,AP_gateway,AP_subnet_mask);
  WiFi.softAP((*Device_Ctrl.JSON__WifiConfig)["AP"]["AP_Name"].as<String>());
  ESP_LOGI("", "AP 熱點 IP address: %s, macAddress: %s", WiFi.softAPIP().toString().c_str(), WiFi.softAPmacAddress().c_str());

  //? 而後嘗試與基地台連接
  int reconnectRetryCount = 0;
  int MAX_RECONNECT_RETRY = 3;
  WiFi.begin(
    (*Device_Ctrl.JSON__WifiConfig)["Remote"]["remote_Name"].as<String>().c_str(),
    (*Device_Ctrl.JSON__WifiConfig)["Remote"]["remote_Password"].as<String>().c_str()
  );
  WiFi.setAutoReconnect(true);
  WiFi.setAutoConnect(true);

  for (;;) {
    vTaskDelay(60*1000/portTICK_PERIOD_MS);
    //! 每一分鐘 Ping WiFi 基地台
    //! 若 Ping 不到，則斷開 WiFi 並重新建立連線
    IPAddress LocalWiFi (192, 168, 1, 1); 
    bool LocalWiFiResult = Ping.ping(LocalWiFi);
    ESP_LOGD("SYS", "WiFi基地台 Ping 測試: %s", LocalWiFiResult?"成功":"失敗");
    if (LocalWiFiResult == false) {
      int ifCheck = (*Device_Ctrl.JSON__WifiConfig)["Remote"]["check"].as<int>();
      if (ifCheck == 1) {
        reconnectRetryCount++;
        //? 若 ping 不到 WiFi 分享器並且機器閒置，且當前時間不介於 55 分 ~ 59 分時，將儀器重開
        if (Device_Ctrl.IsDeviceIdle() == true & minute() < 55 & reconnectRetryCount > MAX_RECONNECT_RETRY) {
          Device_Ctrl.InsertNewLogToDB(
            GetDatetimeString(), 1, "偵測到與WiFi基地台連線異常，準備重開機"
          );
          ESP.restart();
        }
        else {
          Device_Ctrl.InsertNewLogToDB(
            GetDatetimeString(), 1, "偵測到與WiFi基地台連線異常，準備嘗試重連"
          );
          WiFi.disconnect();
          IPAddress AP_IP;
          AP_IP.fromString((*Device_Ctrl.JSON__WifiConfig)["AP"]["AP_IP"].as<String>());
          IPAddress AP_gateway;
          AP_gateway.fromString((*Device_Ctrl.JSON__WifiConfig)["AP"]["AP_gateway"].as<String>());
          IPAddress AP_subnet_mask;
          AP_subnet_mask.fromString((*Device_Ctrl.JSON__WifiConfig)["AP"]["AP_subnet_mask"].as<String>());
          WiFi.softAPConfig(AP_IP,AP_gateway,AP_subnet_mask);
          WiFi.softAP((*Device_Ctrl.JSON__WifiConfig)["AP"]["AP_Name"].as<String>());
          WiFi.begin(
            (*Device_Ctrl.JSON__WifiConfig)["Remote"]["remote_Name"].as<String>().c_str(),
            (*Device_Ctrl.JSON__WifiConfig)["Remote"]["remote_Password"].as<String>().c_str()
          );
          WiFi.setAutoReconnect(true);
          WiFi.setAutoConnect(true);
        }
      }
    }
    else {
      reconnectRetryCount = 0;
    }
  }
}

/**
 * @brief 建立 WiFi Manager Task
 * 
 */
void C_Device_Ctrl::CreateWifiManagerTask()
{
  xTaskCreatePinnedToCore(
    WifiManager, 
    TaskSettingMap["WifiManager"].TaskName, 
    TaskSettingMap["WifiManager"].stack_depth, 
    NULL, 
    (UBaseType_t)TaskSettingMap["WifiManager"].task_priority, 
    TaskSettingMap["WifiManager"].task_handle,
    1
  );
}

String C_Device_Ctrl::AES_encode(String content)
{
  mbedtls_aes_context aes_ctx;
  unsigned char key[16] = { 'i', 'd', 'w', 'a', 't', 'e', 'r', '5','6','6','5','1','5','8','8'};
  unsigned char iv[16];
  for (int i=0;i<16;i++) { iv[i] = 0x01; }
  unsigned char plain[64];
  unsigned char cipher[64] = {0};
  memset(plain, 0, sizeof(plain));
  strncpy(reinterpret_cast<char*>(plain), content.c_str(), sizeof(plain) - 1);
  unsigned char dec_plain[64] = {0};
  mbedtls_aes_init(&aes_ctx);
  mbedtls_aes_setkey_enc(&aes_ctx, key, 128);
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, 64, iv, plain, cipher);
  mbedtls_aes_free(&aes_ctx);
  String cipherText="";
  for (int i=0;i<64;i++) { 
    char str[3];
    sprintf(str, "%02X", cipher[i]);
    cipherText = cipherText + String(str);
  } 
  return cipherText;
}

String C_Device_Ctrl::AES_decode(String content)
{
  mbedtls_aes_context aes_ctx;
  unsigned char key[16] = { 'i', 'd', 'w', 'a', 't', 'e', 'r', '5','6','6','5','1','5','8','8'};
  unsigned char iv[16];
  unsigned char dec_plain[64] = {0};
  for (int i=0;i<16;i++) { iv[i] = 0x01; }
  String hexDigits = "0123456789ABCDEF";
  uint8_t result = 0;
  unsigned char cipherTest[64] = {0};
  for (int i = 0; i < content.length(); i=i+2) {
    result = hexDigits.indexOf(content[i])*16+hexDigits.indexOf(content[i+1]);
    cipherTest[i/2] = result;
  }
  mbedtls_aes_setkey_dec(&aes_ctx, key, 128);
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, 64, iv, cipherTest, dec_plain);
  mbedtls_aes_free(&aes_ctx);
  String returnString = "";
  for (int i = 0; i < sizeof(dec_plain); i++) {
    returnString += String((char)dec_plain[i]);
  }
  return returnString;
}

void C_Device_Ctrl::SendLineNotifyMessage(char * content)
{
  String LINE_Notify_id;
  String LINE_Notify_id_encode = (*Device_Ctrl.JSON__DeviceBaseInfo)["LINE_Notify_id"].as<String>();
  if (LINE_Notify_id_encode != "null") {
    LINE_Notify_id = Device_Ctrl.AES_decode(LINE_Notify_id_encode);
  } else {
    LINE_Notify_id = "";
  }
  bool LINE_Notify_switch = (*Device_Ctrl.JSON__DeviceBaseInfo)["LINE_Notify_switch"].as<bool>();
  if (LINE_Notify_id != "" & LINE_Notify_switch) {
    HTTPClient http;
    http.setTimeout(2000);
    http.setConnectTimeout(2000);
    http.begin("https://notify-api.line.me/api/notify");
    http.setTimeout(2000);
    http.setConnectTimeout(2000);
    http.addHeader("Host", "notify-api.line.me");
    String AuthorizationInfo = "Bearer "+LINE_Notify_id;
    http.addHeader("Authorization", AuthorizationInfo);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Connection", "close");
    String payload = "message="+String(content);
    http.POST(payload);
    http.end();
  }
}

/* 取得信件寄送狀態的回呼函式本體 */
void smtpCallback(SMTP_Status status){
  /* 顯示目前的狀態 */
  Serial.println(status.info());

  /* 顯示傳送結果 */
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("訊息傳送成功：%d\n", status.completedCount());
    Serial.println("----------------\n");
    struct tm dt;

    // for (int i = 0; i < Device_Ctrl.smtp.sendingResult.size(); i++){
    //   SMTP_Result result = Device_Ctrl.smtp.sendingResult.getItem(i);
    //   time_t ts = (time_t)result.timestamp;

    //   ESP_MAIL_PRINTF("訊息編號：%d\n", i + 1);
    //   ESP_MAIL_PRINTF("狀態：%s\n", result.completed ? "成功" : "失敗");
    //   ESP_MAIL_PRINTF("日期/時間：%s\n", asctime(localtime(&ts)));
    //   ESP_MAIL_PRINTF("收信人：%s\n", result.recipients.c_str());
    //   ESP_MAIL_PRINTF("主旨：%s\n", result.subject.c_str());
    // }
    // Serial.println("----------------\n");
    // Device_Ctrl.smtp.sendingResult.clear();
  } else {
    // ESP_MAIL_PRINTF("訊息傳送失敗：%d\n", status.failedCount());
    
  }
}

int C_Device_Ctrl::SendGmailNotifyMessage(char * MailSubject, char * content)
{
  //? https://support.google.com/accounts/answer/185833
  bool isSendMail = (*JSON__DeviceBaseInfo)["Mail_Notify_switch"].as<bool>();
  if (isSendMail) {
    String SMTP_HOST = "smtp.gmail.com";
    int SMTP_PORT = 465;
    String DeviceName = (*JSON__DeviceBaseInfo)["device_no"].as<String>();
    String SenderName = String("自動化水質機-") + DeviceName;
    String User = "User";
    String AutherMail = (*JSON__DeviceBaseInfo)["Mail_Notify_Auther"].as<String>();
    if (AutherMail=="null") {
      ESP_LOGE("Mail", "Mail警告訊息功能缺乏");
      smtp.sendingResult.clear();
      return -4;
    }
    String Key_encode = (*JSON__DeviceBaseInfo)["Mail_Notify_Key"].as<String>();
    if (Key_encode=="null") {
      smtp.sendingResult.clear();
      return -5;
    }
    String Key = Device_Ctrl.AES_decode(Key_encode);
    String TargetMail = (*JSON__DeviceBaseInfo)["Mail_Notify_Target"].as<String>();
    if (TargetMail=="null") {
      smtp.sendingResult.clear();
      return -6;
    }
    smtp.debug(1);
    smtp.callback(smtpCallback);
    //? 宣告SMTP郵件伺服器連線物件
    ESP_Mail_Session session;
    //? 設定SMTP郵件伺服器連線物件
    session.server.host_name = SMTP_HOST;      // 設定寄信的郵件伺服器名稱
    session.server.port = SMTP_PORT;           // 郵件伺服器的埠號
    session.login.email = AutherMail;         // 你的帳號
    session.login.password = Key;             // 密碼
    //? 設置時區
    session.time.ntp_server = F("pool.ntp.org,time.nist.gov");
    session.time.gmt_offset = 8;        // 台北時區
    session.time.day_light_offset = 0;  // 無日光節約時間

    if (!smtp.connect(&session)) {
      ESP_LOGE("Mail", "Mail訊息通知無法連上STMP伺服器");
      smtp.sendingResult.clear();
      return -2;
    }

    SMTP_Message message;

    /* 設定郵件標頭 */
    message.sender.name = SenderName;    // 寄信人的名字
    message.sender.email = AutherMail;  // 寄信人的e-mail
    message.subject = "[自動化水質機台]"+String(MailSubject);       // 信件主旨

    int strIndex[] = { 0, -1 };
    int maxIndex = TargetMail.length() - 1;
    for (int i = 0; i <= maxIndex; i++) {
      if (TargetMail.charAt(i) == ';' || i == maxIndex) {
        strIndex[0] = strIndex[1] + 1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
        message.addRecipient("USER", TargetMail.substring(strIndex[0], strIndex[1]));
      }
    }

    // message.html.content = content;  // 設定郵件內容（HTML格式訊息
    message.text.content = content;
    message.text.charSet = "utf-8";  // 設定訊息文字的編碼
    message.text.transfer_encoding = "base64"; //! recommend for non-ASCII words in message
    if (!MailClient.sendMail(&smtp, &message)) {
      Serial.println("寄信時出錯了：" + smtp.errorReason());
      smtp.sendingResult.clear();
      smtp.closeSession();
      return -3;
    }
    smtp.sendingResult.clear();
    smtp.closeSession();
    return 1;
  }
  else {
    return -1;
  }
}


typedef enum {
  LINE, GMAIL
} line_mail_event_t;

typedef struct {
  line_mail_event_t event;
  union {
    struct {
      char * content;
    } line_event;
    struct {
      char * title;
      char * content;
    } gmail_event;
  };
} line_mail_notify_t;

static inline bool _send_async_line_mail_event(line_mail_notify_t ** e){
  return Device_Ctrl._async_queue__LINE_MAIN_Notify_Task && 
    xQueueSend(Device_Ctrl._async_queue__LINE_MAIN_Notify_Task, e, portMAX_DELAY) == pdPASS;
}

bool C_Device_Ctrl::AddLineNotifyEvent(char * content, int len) {
  line_mail_notify_t * e = (line_mail_notify_t *)malloc(sizeof(line_mail_notify_t));
  e->event = line_mail_event_t::LINE;
  char *contentCharArray = (char*)malloc((len+1) * sizeof(char));
  strcpy(contentCharArray, content);
  e->line_event.content = contentCharArray;
  if (!_send_async_line_mail_event(&e)) {
    free((void*)(e));
    free(contentCharArray);
    return false;
  }
  return true;
};

bool C_Device_Ctrl::AddLineNotifyEvent(String content) {
  line_mail_notify_t * e = (line_mail_notify_t *)malloc(sizeof(line_mail_notify_t));
  e->event = line_mail_event_t::LINE;
  char *contentCharArray = (char*)malloc((content.length()+1) * sizeof(char));
  content.toCharArray(contentCharArray, content.length()+1);
  e->line_event.content = contentCharArray;
  if (!_send_async_line_mail_event(&e)) {
    free((void*)(e));
    free(contentCharArray);
    return false;
  }
  return true;
};

bool C_Device_Ctrl::AddGmailNotifyEvent(String MailSubject,String content) {
  line_mail_notify_t * e = (line_mail_notify_t *)malloc(sizeof(line_mail_notify_t));
  e->event = line_mail_event_t::GMAIL;
  char *titleCharArray = (char*)malloc((MailSubject.length()+1) * sizeof(char));
  MailSubject.toCharArray(titleCharArray, MailSubject.length()+1);
  e->gmail_event.title = titleCharArray;
  char *contentCharArray = (char*)malloc((content.length()+1) * sizeof(char));
  content.toCharArray(contentCharArray, content.length()+1);
  e->gmail_event.content = contentCharArray;
  if (!_send_async_line_mail_event(&e)) {
    free(titleCharArray);
    free(contentCharArray);
    free((void*)(e));
    return false;
  }
  return true;
};

bool C_Device_Ctrl::AddGmailNotifyEvent(char * MailSubject, int MailSubject_len, char * content, int content_len) {
  line_mail_notify_t * e = (line_mail_notify_t *)malloc(sizeof(line_mail_notify_t));
  e->event = line_mail_event_t::GMAIL;
  char *titleCharArray = (char*)malloc((MailSubject_len+1) * sizeof(char));
  strcpy(titleCharArray, MailSubject);
  e->gmail_event.title = titleCharArray;
  char *contentCharArray = (char*)malloc((content_len+1) * sizeof(char));
  strcpy(contentCharArray, content);
  e->gmail_event.content = contentCharArray;
  if (!_send_async_line_mail_event(&e)) {
    free(titleCharArray);
    free(contentCharArray);
    free((void*)(e));
    return false;
  }
  return true;
};

static inline bool _get_async_line_mail_event(line_mail_notify_t ** e){
  return Device_Ctrl._async_queue__LINE_MAIN_Notify_Task && 
    xQueueReceive(Device_Ctrl._async_queue__LINE_MAIN_Notify_Task, e, portMAX_DELAY) == pdPASS;
}

static void _handle_async_line_mail_event(line_mail_notify_t * e){
  if (e->event == line_mail_event_t::GMAIL) {
    Device_Ctrl.SendGmailNotifyMessage(e->gmail_event.title, e->gmail_event.content);
    free(e->gmail_event.title);
    free(e->gmail_event.content);
  } else if (e->event == line_mail_event_t::LINE) {
    Device_Ctrl.SendLineNotifyMessage(e->line_event.content);
    free(e->line_event.content);
  }
  free((void*)(e));
}

void LINE_MAIN_NotifyTask(void* parameter) 
{
  line_mail_notify_t * packet = NULL;
  for (;;) {
    if(_get_async_line_mail_event(&packet)){
      _handle_async_line_mail_event(packet);
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
  Device_Ctrl._async_queue__LINE_MAIN_Notify_Task = NULL;
  vTaskDelete(NULL);
}

void C_Device_Ctrl::CreateLINE_MAIN_NotifyTask()
{
  _async_queue__LINE_MAIN_Notify_Task = xQueueCreate(32, sizeof(line_mail_notify_t *));
  if(_async_queue__LINE_MAIN_Notify_Task){
    xTaskCreatePinnedToCore(
      LINE_MAIN_NotifyTask, 
      TaskSettingMap["LINEMAINNotify"].TaskName, 
      TaskSettingMap["LINEMAINNotify"].stack_depth, 
      NULL, 
      (UBaseType_t)TaskSettingMap["LINEMAINNotify"].task_priority, 
      TaskSettingMap["LINEMAINNotify"].task_handle,
      1
    );
    // xTaskCreatePinnedToCore(
    //   LINE_MAIN_NotifyTask, 
    //   "LINEMAINNotify", 
    //   1024*10, 
    //   NULL, 
    //   (UBaseType_t)TaskPriority::LINE_MAIN_Notify, 
    //   &TASK__LINE_MAIN_Notify, 
    //   1
    // );
  } else {
    ESP_LOGE("", "通知功能駐列空間要求失敗");
  }
}

DynamicJsonDocument C_Device_Ctrl::CheckComsumeStatus()
{
  DynamicJsonDocument ReturnData(1024);
  for (JsonPair ConsumeData : (*JSON__Consume).as<JsonObject>()) {
    String TargetName = String(ConsumeData.key().c_str());
    if (TargetName == "null") {
      continue;
    }
    double alarm = (*JSON__Consume)[TargetName]["alarm"].as<double>();
    double remaining = (*JSON__Consume)[TargetName]["remaining"].as<double>();
    ESP_LOGD("", "%s, %.2f, %.2f",
      TargetName, alarm, remaining
    );
    if (remaining < alarm) {
      ReturnData[TargetName].set((*JSON__Consume)[TargetName]);
    }
  }
  return ReturnData;
}

void C_Device_Ctrl::SendComsumeWaring()
{
  String WarningStr = "";
  DynamicJsonDocument WarningData = CheckComsumeStatus();
  for (JsonPair ConsumeData : WarningData.as<JsonObject>()) {
    String TargetName = String(ConsumeData.key().c_str());
    if (TargetName == "null") {
      continue;
    }
    double alarm = (*JSON__Consume)[TargetName]["alarm"].as<double>();
    double remaining = (*JSON__Consume)[TargetName]["remaining"].as<double>();
    if (TargetName == "RO") {
      WarningStr += " - 自來水桶剩餘量: "+String(remaining,2)+"ml, 低於設定值: "+String(alarm,2)+" ml\n";
    }
    else if (TargetName == "NO2_R1") {
      WarningStr += " - 亞硝酸鹽R1試劑剩餘量: "+String(remaining,2)+"ml, 低於設定值: "+String(alarm,2)+" ml\n";
    }
    else if (TargetName == "NH4_R1") {
      WarningStr += " - 氨氮R1試劑剩餘量: "+String(remaining,2)+"ml, 低於設定值: "+String(alarm,2)+" ml\n";
    }
    else if (TargetName == "NH4_R2") {
      WarningStr += " - 氨氮R2試劑剩餘量: "+String(remaining,2)+"ml, 低於設定值: "+String(alarm,2)+" ml\n";
    }
  }
  if (WarningStr.length() == 0) {return;}
  String sendString = "\n儀器: " + (*Device_Ctrl.JSON__DeviceBaseInfo)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 以下耗材可能需要更換\n";
  sendString += WarningStr;
  sendString += "請盡速確認列表內耗材是否充足";
  Serial.println(sendString);

  char *contentCharArray = (char*)malloc((sendString.length()+1) * sizeof(char));
  sendString.toCharArray(contentCharArray, sendString.length()+1);
  AddLineNotifyEvent(contentCharArray, sendString.length());
  AddGmailNotifyEvent((char*)"耗材警告訊息",19 ,contentCharArray, sendString.length());
  free(contentCharArray);
}

void C_Device_Ctrl::CreatePipelineFlowScanTask()
{
  xTaskCreatePinnedToCore(
    PipelineFlowScan, 
    TaskSettingMap["PipelineScan"].TaskName, 
    TaskSettingMap["PipelineScan"].stack_depth, 
    NULL, 
    (UBaseType_t)TaskSettingMap["PipelineScan"].task_priority, 
    TaskSettingMap["PipelineScan"].task_handle,
    1
  );

  // xTaskCreatePinnedToCore(
  //   PipelineFlowScan, 
  //   "PipelineScan", 
  //   1024*30, 
  //   NULL, 
  //   (UBaseType_t)TaskPriority::PipelineFlowScan, 
  //   &TASK__pipelineFlowScan, 
  //   1
  // );
}

void C_Device_Ctrl::StopNowPipelineAndAllStepTask()
{
  //! 關閉所有光度計
  digitalWrite(PIN__EN_BlueSensor, LOW);
  digitalWrite(PIN__EN_GreenSensor, LOW);
  //! 蠕動馬達斷電
  digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
  //! 伺服馬達斷電
  digitalWrite(PIN__EN_Servo_Motor, LOW);
  //! 所有蠕動馬達狀態定為 不轉動
  Device_Ctrl.peristalticMotorsCtrl.SetAllMotorStop();

  //! 關閉所有步進蠕動馬達
  // TODO 目前只有一顆 測試中
  Serial1.begin(115200,SERIAL_8N1,PIN__Step_Motor_RS485_RX, PIN__Step_Motor_RS485_TX);
  mb_.begin(&Serial1);
  mb_.setBaudrate(115200);
  mb_.master();
  uint16_t writeData[4] = {1, 0,0,0};
  mb_.writeHreg(1,1,writeData,4);
  while(mb_.slave()) {
    mb_.task();
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
  // TODO 目前只有一顆 測試中

  Device_Ctrl.StopAllStepTask();

  // Device_Ctrl.StopNowPipeline = true;
  // while (Device_Ctrl.StopNowPipeline) {
  //   vTaskDelay(10/portTICK_PERIOD_MS);
  // }
  // for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
  //   StepTaskDetailList[i].TaskStatus = StepTaskStatus::Close;
  // }
}


void C_Device_Ctrl::StopDeviceAllAction()
{
  //! 關閉所有光度計
  digitalWrite(PIN__EN_BlueSensor, LOW);
  digitalWrite(PIN__EN_GreenSensor, LOW);
  //! 蠕動馬達斷電
  digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
  //! 所有蠕動馬達狀態定為 不轉動
  Device_Ctrl.peristalticMotorsCtrl.SetAllMotorStop();
  //! 伺服馬達斷電
  digitalWrite(PIN__EN_Servo_Motor, LOW);

  //! 關閉所有步進蠕動馬達
  // TODO 目前只有一顆 測試中
  Serial1.begin(115200,SERIAL_8N1,PIN__Step_Motor_RS485_RX, PIN__Step_Motor_RS485_TX);
  mb_.begin(&Serial1);
  mb_.setBaudrate(115200);
  mb_.master();
  uint16_t writeData[4] = {1, 0,0,0};
  mb_.writeHreg(1,1,writeData,4);
  while(mb_.slave()) {
    mb_.task();
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
  // TODO 目前只有一顆 測試中

  ESP_LOGD("StopDeviceAllAction", "已斷開所有設施電源");

  //! 將 StopAllPipeline 與 StopAllPipeline 狀態值改為true
  //! 改為 true 後，會先關閉 PipelineFlowScanTask 中執行的流程
  //! 中斷執行會需要一點時間，並非立即中斷
  //! 這寫法實在不夠好，太多有的沒的狀態，待日後想到好作法在來取代
  Device_Ctrl.StopAllPipeline = true;

  Device_Ctrl.StopAllStepTask();
  // Device_Ctrl.StopNowPipeline = true;

  // //! 等待 PipelineFlowScanTask 執行停止
  // //! 再慢慢將各個Step停止
  // while (Device_Ctrl.StopNowPipeline) {
  //   vTaskDelay(10/portTICK_PERIOD_MS);
  // }
  // for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
  //   StepTaskDetailList[i].TaskStatus = StepTaskStatus::Close;
  // }
}

bool C_Device_Ctrl::IsDeviceIdle()
{
  if (all_INIT_done == false) {
    return false;
  }

  //? 嘗試獲得 xMutex__pipelineFlowScan 
  //? 若要得到，代表沒有流程在跑
  if (xSemaphoreTake(Device_Ctrl.xMutex__pipelineFlowScan, 0) != pdTRUE) {
    return false;
  }
  //? 要到後記得還回去
  xSemaphoreGive(Device_Ctrl.xMutex__pipelineFlowScan);

  //? 另外再檢查是否各個 Step 都在 Idle
  for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
    if (StepTaskDetailList[i].TaskStatus != StepTaskStatus::Idle) {
      return false;
    }
  }

  //? 另外再檢查 PipelineList 是否為空
  JsonArray PipelineList = (*Device_Ctrl.JSON__pipelineStack).as<JsonArray>();
  if (PipelineList.size() != 0) {
    return false;
  }

  return true;
}

void C_Device_Ctrl::StopAllStepTask()
{
  ESP_LOGD("StopAllStepTask", "準備停止所有Step，同時停止Pipeline流程");
  //! 將 Device_Ctrl.StopNowPipeline 改為 true 時
  //! 在 PIPELINE__PipelineFlowScanTask.h 中會執行 pipeline Task 停止流程
  //! 等停下後 Device_Ctrl.StopNowPipeline 會被改為 false
  Device_Ctrl.StopNowPipeline = true;
  while (Device_Ctrl.StopNowPipeline) {
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
  ESP_LOGD("StopAllStepTask", "停止所有Step");
  //? 強制修改所有 Step Task 的狀態為 StepTaskStatus::Close
  //? 而後在 Step Task 自己的迴圈內會自行判斷，並將狀態更改為 StepTaskStatus::Idle
  //? 詳細請至 PIPELINE__StepTask.h 觀看
  for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
    StepTaskDetailList[i].TaskStatus = StepTaskStatus::Close;
  }
  //? 檢查是否所有Step都停止了，都停止才繼續執行下去
  int timeOut = 5000;
  bool allStop = true;
  time_t waitStart = now();
  //? 所有 Stap Task 變為 StepTaskStatus::Idle 才算結束
  while (allStop) {
    allStop = true;
    for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
      if (StepTaskDetailList[i].TaskStatus != StepTaskStatus::Idle) {
        allStop = false;
        break;
      }
    }
    if (now() - waitStart >= timeOut) {
      ESP_LOGE("嚴重系統性錯誤", "停止所有Step流程Timeout，請聯絡工程師排除此BUG");
      Device_Ctrl.AddGmailNotifyEvent("嚴重系統性錯誤", "停止所有Step流程Timeout，請聯絡工程師排除此BUG");
      Device_Ctrl.AddLineNotifyEvent("[嚴重系統性錯誤]停止所有Step流程Timeout，請聯絡工程師排除此BUG");
      break;
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
}

void C_Device_Ctrl::WritePipelineLogFile(String FileFullPath, const char *content, ...)
{
  char buffer[4096];  // 用于存储格式化后的文本
  va_list ap;
  va_start(ap, content);
  vsprintf(buffer, content, ap);
  va_end(ap);
  File logFile = SD.open(FileFullPath, FILE_APPEND);
  logFile.printf("[%s] %s\n",GetDatetimeString().c_str(), String(buffer).c_str());
  logFile.close();
}

void C_Device_Ctrl::CreateStepTasks()
{
  for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
    String TaskName = "StepTask-"+String(i);
    std::string TaskName_cSting = std::string(TaskName.c_str());
    StepTaskDetailList[i].TaskName = TaskName;
    xTaskCreatePinnedToCore(
      StepTask, 
      TaskSettingMap[TaskName_cSting].TaskName, 
      TaskSettingMap[TaskName_cSting].stack_depth, 
      (void*)(&StepTaskDetailList[i]),  //! 將指針傳入 Task 中
      (UBaseType_t)TaskSettingMap[TaskName_cSting].task_priority, 
      TaskSettingMap[TaskName_cSting].task_handle,
      1
    );
    // xTaskCreatePinnedToCore(
    //   StepTask, 
    //   TaskName.c_str(), 
    //   15000, 
    //   (void*)(&StepTaskDetailList[i]), 
    //   (UBaseType_t)TaskPriority::PiplelineFlowTask_1 - i, 
    //   &TASKLIST__StepTask[i], 
    //   1
    // );
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
    if ((*Device_Ctrl.JSON__DeviceBaseInfo)["schedule_switch"].as<bool>()) {
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
        }
        ESP_LOGI("", "排程檢查完畢，等待下一個檢查時段");
        vTaskDelay(1000*60*30/portTICK_PERIOD_MS);
      }
    }
    vTaskDelay(1000*60*1/portTICK_PERIOD_MS);
  }
}

void C_Device_Ctrl::CreateScheduleManagerTask()
{
  xTaskCreatePinnedToCore(
    ScheduleManager, 
    TaskSettingMap["ScheduleManager"].TaskName, 
    TaskSettingMap["ScheduleManager"].stack_depth, 
    NULL, 
    (UBaseType_t)TaskSettingMap["ScheduleManager"].task_priority, 
    TaskSettingMap["ScheduleManager"].task_handle,
    1
  );
  // xTaskCreatePinnedToCore(
  //   ScheduleManager, 
  //   "ScheduleManager", 
  //   10000, 
  //   NULL, 
  //   (UBaseType_t)TaskPriority::ScheduleManager, 
  //   &TASK__ScheduleManager, 
  //   1
  // );
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
    TaskSettingMap["TimeCheckTask"].TaskName, 
    TaskSettingMap["TimeCheckTask"].stack_depth, 
    NULL, 
    (UBaseType_t)TaskSettingMap["TimeCheckTask"].task_priority, 
    TaskSettingMap["TimeCheckTask"].task_handle,
    1
  );
  // xTaskCreatePinnedToCore(
  //   TimeCheckTask, 
  //   "TimeCheckTask", 
  //   1024*10, 
  //   NULL, 
  //   (UBaseType_t)TaskPriority::TimeCheckTask, 
  //   &TASK__TimerChecker, 
  //   1
  // );
}

void C_Device_Ctrl::ChangeLastOledLog(const char* content, ...)
{
  char buffer[22];
  va_list ap;
  va_start(ap, content);
  vsnprintf(buffer, 21,content, ap);
  va_end(ap);
  (*JSON__oledLogList).remove((*JSON__oledLogList).size()-1);
  (*JSON__oledLogList).add(String(buffer));
  display.clearDisplay();
  display.setCursor(0,0);
  for (int i = 0; i < (*JSON__oledLogList).size(); i++) {
    String item = (*JSON__oledLogList)[i].as<String>(); // 使用 as<String>() 將元素轉換為 String
    display.println(item);
  }
  display.display();
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
    else {
      if (IPStringSave != "") {
        IPStringSave = "";
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextSize(1);
        display.printf("WiFi is not connected");
        display.display();
      }
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
  vTaskDelay(1000/portTICK_PERIOD_MS);
}


void C_Device_Ctrl::CreateOledQRCodeTask()
{
  xTaskCreatePinnedToCore(
    OledQRCode, 
    TaskSettingMap["OledQRCode"].TaskName, 
    TaskSettingMap["OledQRCode"].stack_depth, 
    NULL, 
    (UBaseType_t)TaskSettingMap["OledQRCode"].task_priority, 
    TaskSettingMap["OledQRCode"].task_handle,
    1
  );
  // xTaskCreatePinnedToCore(
  //   OledQRCode, 
  //   "OledQRCode", 
  //   1024*10, 
  //   NULL, 
  //   (UBaseType_t)TaskPriority::OLEDCheckTask, 
  //   &TASK__OledQRCode, 
  //   1
  // );
}


void C_Device_Ctrl::WriteSysInfo()
{
  String InfoFileName = GetDateString("")+".log";
  String InfoFileFullPath = "/sys/"+InfoFileName;
  if(SD.exists("/sys")==false) {
    SD.mkdir("/sys");
  }
  // File logFile = SD.open(InfoFileFullPath, "a");
  ESP_LOGD("SYS", "Time: %s", GetDatetimeString().c_str());
  ESP_LOGD("SYS", "FreeRTOS Task Info:");
  for (const auto& pair : Device_Ctrl.TaskSettingMap) {
    ESP_LOGD("SYS", " - %s:\t%d/%d", pair.first.c_str(), pair.second.stack_depth-uxTaskGetStackHighWaterMark(pair.second.task_handle),pair.second.stack_depth);
  }
  ESP_LOGD("SYS", "WiFi Mode: %d", WiFi.getMode());
  ESP_LOGD("SYS", "WiFi isConnected: %s", WiFi.isConnected()?"Y":"N");
  ESP_LOGD("SYS", "WiFi status: %d", WiFi.status());
  ESP_LOGD("SYS", "WiFi RSSI: %d", WiFi.RSSI());
  ESP_LOGD("SYS", "STA IP: %s", WiFi.localIP().toString().c_str());

  IPAddress LocalWiFi (192,168,1,1); 
  bool LocalWiFiResult = Ping.ping(LocalWiFi);
  ESP_LOGD("SYS", "WiFi基地台 Ping 測試: %s", LocalWiFiResult?"成功":"失敗");
  if (LocalWiFiResult == false) {
    //? 若 ping 不到 WiFi 分享器並且機器閒置，且當前時間不介於 55 分 ~ 59 分時，將儀器重開
    if (IsDeviceIdle() == true & minute() < 55 ) {
      InsertNewLogToDB(
        GetDatetimeString(), 1, "偵測到與WiFi基地台連線異常，準備重開機"
      );
      ESP.restart();
    }
  }



  // int IsCheckOpen = (*Device_Ctrl.JSON__WifiConfig)["Remote"]["checker"]["check"].as<int>();
  // Serial.println(IsCheckOpen);
  // if (IsCheckOpen == 1) {
  //   IPAddress LocalWiFi; 
  //   LocalWiFi.fromString((*Device_Ctrl.JSON__WifiConfig)["Remote"]["checker"]["check_IP"].as<String>());
  //   bool LocalWiFiResult = Ping.ping(LocalWiFi);
  //   ESP_LOGD("SYS", "WiFi基地台 Ping 測試: %s", LocalWiFiResult?"成功":"失敗");
  //   if (LocalWiFiResult == false) {
  //     //? 若 ping 不到 WiFi 分享器並且機器閒置，且當前時間不介於 55 分 ~ 59 分時，將儀器重開
  //     if (IsDeviceIdle() == true & minute() < 55 ) {
  //       InsertNewLogToDB(
  //         GetDatetimeString(), 1, "偵測到與WiFi基地台連線異常，準備重開機"
  //       );
  //       ESP.restart();
  //     }
  //   }
  // }

  // IPAddress GlobalNet (8, 8, 8, 8); 
  // bool GlobalNetResult = Ping.ping(GlobalNet);
  // ESP_LOGD("SYS", "網際網路 Ping 測試: %s", GlobalNetResult?"成功":"失敗");
  // if (GlobalNetResult == false) {
  //   InsertNewLogToDB(
  //     GetDatetimeString(), 1, "偵測到與網際網路連線異常"
  //   );
  // }
}                                                                                                  

/**
 * @brief 計數增加會對2種檔案進行寫入
 * 1. Sqlite 檔案: 預計純寫入，紀錄項目,時間,次數
 * 2. json 檔案: 會讀取 + 重寫
 * 
 * @param ItemName 
 * @param countAdd 
 */
void C_Device_Ctrl::ItemUsedAdd(String ItemName, int countAdd)
{
  //? 寫入 sqlite DB
  InsertNewUsedDataToDB(now(), ItemName, countAdd);
  //? 寫入JSON
  if ((*JSON__ItemUseCount)[ItemName] == nullptr) {
    (*JSON__ItemUseCount)[ItemName] = countAdd;
  } else {
    (*JSON__ItemUseCount)[ItemName] = (*JSON__ItemUseCount)[ItemName].as<int>() + countAdd;
  }
  ExFile_WriteJsonFile(SD, FilePath__SD__ItemUseCount, *JSON__ItemUseCount);
}

void C_Device_Ctrl::InsertNewUsedDataToDB(time_t time, String item, int count)
{
  String SqlString = "INSERT INTO used ( time, item, count ) VALUES ( ";
  SqlString += String(time);
  SqlString += " ,'";
  SqlString += item;
  SqlString += "' ,";
  SqlString += String(count);
  SqlString += " );";
  db_exec(DB_Log, SqlString);
}

C_Device_Ctrl Device_Ctrl;