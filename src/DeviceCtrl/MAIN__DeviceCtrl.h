#ifndef DEVICE_CTRL_H
#define DEVICE_CTRL_H

#define MAX_STEP_TASK_NUM 4
#define FIRMWARE_VERSION "V4.24.0204.0"

#include "CONFIG__pinMap.h"
#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <sqlite3.h>
#include "PeristalticMotorCtrl.h"
// #include "WebsocketSetting.h"
#include <unordered_map>
#include <map>

#include <Adafruit_GFX.h>
#include "Adafruit_SH1106.h"
#include <ESP_Mail_Client.h>



//! 為了保證各Task優先級不會打架，用enum來自動分配優先級數字，
//! 所有Task應該都要先在此定義優先級，也方便統計當前有多少Task
//! 越往下數字越大，優先級越高
enum class TaskPriority : UBaseType_t {
  NoUSE = 0,
  OTAService,  
  DeviceInfoCheckTask,  //? 定期自我檢查並記錄儀器各項數值與狀態
  OLEDCheckTask,        //? OLED螢幕顯示專用Task
  skip_1,
  APICheckerTask,       
  TimeCheckTask,        //? 時鐘檢查用Task
  ScheduleManager,      //? 儀器本地端排程檢查Task
  PiplelineFlowTask_6,  //? 流程Thread-6
  PiplelineFlowTask_5,  //? 流程Thread-5
  PiplelineFlowTask_4,  //? 流程Thread-4
  PiplelineFlowTask_3,  //? 流程Thread-3
  PiplelineFlowTask_2,  //? 流程Thread-2
  PiplelineFlowTask_1,  //? 流程Thread-1
  PipelineFlowScan      //? 整理流程管理
};

class C_WebsocketAPI;

enum class StepTaskStatus : uint8_t {
  Idel,
  Busy,
  Close
};

struct StepTaskDetail {
  StepTaskStatus TaskStatus = StepTaskStatus::Idel;
  String TaskName;
  String StepName="";
  String PipelineName="";
};


class C_Device_Ctrl
{
  public:
    C_Device_Ctrl(void){
      vSemaphoreCreateBinary(xMutex__pipelineFlowScan);
      vSemaphoreCreateBinary(xMutex__LX_20S);
    };

    //! 初始化相關

    // 預設使用 Wire
    TwoWire _Wire = Wire;

    void ScanI2C();

    //? 初始化各個PIN的初始化狀態
    void INIT_Pins();

    void INIT_I2C_Wires(TwoWire &WireChose);

    bool INIT_PoolData();
    //? 初始化SD卡
    bool INIT_SD();
    bool INIT_SPIFFS();
    bool INIT_oled();


    //! Sqlite3相關操作
    int INIT_SqliteDB();
    sqlite3 *DB_Main;
    String FilePath__SD__MainDB = "/sd/mainDB.db";
    void InsertNewDataToDB(String time, String pool, String ValueName, double result);
    
    /**
     * @brief 
     * 
     * @param level 0: DEBUG, 1: ERROR, 2: WARNING, 3: INFO, 4: DETAIL, 5: SUCCESS, 6: FAIL
     * @param time 
     * @param content 
     */
    void InsertNewLogToDB(String time, int level, const char* content, ...);
    void DeleteOldLog();

    //! SD相關操作與函式
    void LoadConfigJsonFiles();
    String FilePath__SD__DeviceConfig = "/config/device_config.json";
    DynamicJsonDocument *JSON__DeviceConfig = new DynamicJsonDocument(1024);
    String FilePath__SD__SpectrophotometerConfig = "/config/spectrophotometer_config.json";
    DynamicJsonDocument *JSON__SpectrophotometerConfig = new DynamicJsonDocument(1024*4);
    String FilePath__SD__PHmeterConfig = "/config/PHmeter_config.json";
    DynamicJsonDocument *JSON__PHmeterConfig = new DynamicJsonDocument(1024);
    String FilePath__SD__PoolConfig = "/config/pool_config.json";
    DynamicJsonDocument *JSON__PoolConfig = new DynamicJsonDocument(1024*2);
    String FilePath__SD__ScheduleConfig = "/config/schedule_config.json";
    DynamicJsonDocument *JSON__ScheduleConfig = new DynamicJsonDocument(1024*4);
    String FilePath__SD__ServoConfig = "/config/pwm_motor_config.json";
    DynamicJsonDocument *JSON__ServoConfig = new DynamicJsonDocument(1024*8);
    String FilePath__SD__PeristalticMotorConfig = "/config/peristaltic_motor_config.json";
    DynamicJsonDocument *JSON__PeristalticMotorConfig = new DynamicJsonDocument(1024*2);
    String FilePath__SD__WiFiConfig = "/config/wifi_config.json";
    DynamicJsonDocument *JSON__WifiConfig = new DynamicJsonDocument(1024);
    String FilePath__SD__DeviceBaseInfo = "/config/device_base_config.json";
    DynamicJsonDocument *JSON__DeviceBaseInfo = new DynamicJsonDocument(1024);
    String FilePath__SD__LastSensorDataSave = "/datas/temp.json";
    void UpdatePipelineConfigList();
    DynamicJsonDocument *JSON__PipelineConfigList = new DynamicJsonDocument(10000);

    //! WiFi相關功能

    void preLoadWebJSFile();
    void ConnectWiFi();
    void INITWebServer();
    uint8_t *webJS_Buffer;
    size_t webJS_BufferLen;
    void CreateOTAService();
    TaskHandle_t TASK__OTAService = NULL;
    void UpdateDeviceTimerByNTP();
    void BroadcastLogToClient(
      AsyncWebSocketClient *client,
      int Level, const char* content, ...
    );
    DynamicJsonDocument GetWebsocketConnectInfo();
    //! WiFi連線檢查Task
    void CreateWifiManagerTask();
    TaskHandle_t TASK__WifiManager = NULL;

    //! 錯誤警報相關功能

    void SendLineNotifyMessage(String content);

    SMTPSession smtp;
    int SendGmailNotifyMessage(String MailSubject,String content);


    //! Websocket相關
    void INIT_AllWsAPI();
    void AddWebsocketAPI(String APIPath, String METHOD, void (*func)(AsyncWebSocket*, AsyncWebSocketClient*, DynamicJsonDocument*, DynamicJsonDocument*, DynamicJsonDocument*, DynamicJsonDocument*));
    std::map<std::string, std::unordered_map<std::string, C_WebsocketAPI*>> websocketApiSetting;
    DynamicJsonDocument GetBaseWSReturnData(String MessageString);



    //! 流程設定相關

    //? JSON__pipelineStack: 待執行的Step列表
    /** 格式
      {
        "FullFilePath": <str: 檔案路徑>,
        "TargetName": <str: 目標名稱>,
        "stepChose": <str: 單獨步驟指定>,
        "eventChose": <str: 單獨事件>,
        "eventIndexChose": <str: 單獨小事件指定>,
      }
     */
    DynamicJsonDocument *JSON__pipelineStack = new DynamicJsonDocument(10240);
    //? JSON__pipelineConfig: 當前運行的Pipeline詳細設定
    DynamicJsonDocument *JSON__pipelineConfig = new DynamicJsonDocument(65535);
    //? 建立流程管理Task
    void CreatePipelineFlowScanTask();
    TaskHandle_t TASK__pipelineFlowScan = NULL;
    //? xMutex__pipelineFlowScan: 要不到時代表忙碌中
    SemaphoreHandle_t xMutex__pipelineFlowScan = NULL;

    
    TaskHandle_t TASKLIST__StepTask[MAX_STEP_TASK_NUM];
    StepTaskDetail StepTaskDetailList[MAX_STEP_TASK_NUM];
    void CreateStepTasks();

    SemaphoreHandle_t xMutex__LX_20S = NULL;

    //? 停止所有Step流程，使之回歸 Idel狀態
    //? 執行此方程會同時停止當前Pipeline，並確認Pipeline停止後，才陸續將Step停止
    //? 最後將所有Step停止後，才結束方程
    void StopAllStepTask();
    //? 停止當前的Pipeline以及所有Theard Task，讓下一個Pipeline執行
    void StopNowPipelineAndAllStepTask();
    //? 停止所有Pipeline以及所有Theard Task，回歸Idel
    void StopDeviceAllAction();
    //? 獲得儀器是否為待機狀態
    bool IsDeviceIdel();

    //? 是否停止當前執行中的流程
    //? 如果為true，則會陸續將當前流程相關的步驟關閉
    //! 這寫法實在不夠好，太多有的沒的狀態，待日後想到好作法在來取代
    bool StopNowPipeline = false;
    //? 是否停止當前所有執行中與等待執行的流程
    //? 如果為true，則會陸續將所有流程關閉
    //! 這寫法實在不夠好，太多有的沒的狀態，待日後想到好作法在來取代
    bool StopAllPipeline = false;

    //! 排程檢查Task
    void CreateScheduleManagerTask();
    TaskHandle_t TASK__ScheduleManager = NULL;

    //! 時鐘檢查排程
    void CreateTimerCheckerTask();
    TaskHandle_t TASK__TimerChecker = NULL;

    //! 

    C_Peristaltic_Motors_Ctrl peristalticMotorsCtrl;

    //! 感測資料相關
    //? JSON__sensorDataSave: 紀錄當前儀器的感測器資料
    DynamicJsonDocument *JSON__sensorDataSave = new DynamicJsonDocument(50000);

    //! 光度計數值相關
    double lastLightValue_NO3 = 0;
    double lastLightValue_NH4 = 0;

    //! oled螢幕相關
    DynamicJsonDocument *JSON__oledLogList = new DynamicJsonDocument(50000);
    // void AddNewOledLog(String content);
    void AddNewOledLog(const char* content, ...);
    void CreateOledQRCodeTask();
    TaskHandle_t TASK__OledQRCode = NULL;

  private:
};

//! //////////////////////////////////////////////////////////////
//! Websocket API Class
//! //////////////////////////////////////////////////////////////


class C_WebsocketAPI
{
  public:
    String APIPath, METHOD;
    std::vector<String> pathParameterKeyMapList;
    /**
     * @brief 
     * ws, client, returnInfoJSON, PathParameterJSON, QueryParameterJSON, FormDataJSON
     * 
     */
    void (*func)(
      AsyncWebSocket*, AsyncWebSocketClient*, 
      DynamicJsonDocument*, DynamicJsonDocument*, DynamicJsonDocument*, DynamicJsonDocument*
    );
    C_WebsocketAPI(
      String APIPath_, String METHOD_, 
      void (*func_)(
        AsyncWebSocket*, AsyncWebSocketClient*, 
        DynamicJsonDocument*, DynamicJsonDocument*, DynamicJsonDocument*, DynamicJsonDocument*
      )
    ) {
      METHOD = METHOD_;
      func = func_;
      std::regex ms_PathParameterKeyRegex(".*\\(\\<([a-zA-Z0-9_]*)\\>.*\\).*");
      std::regex ms_PathParameterReplace("\\<.*\\>");  
      std::smatch matches;
      char *token;
      char newAPIPathChar[APIPath_.length()+1];
      strcpy(newAPIPathChar, APIPath_.c_str());
      token = strtok(newAPIPathChar, "/");
      std::string reMix_APIPath = "";
      while( token != NULL ) {
        std::string newMSstring = std::string(token);
        if (std::regex_search(newMSstring, matches, ms_PathParameterKeyRegex)) {
          pathParameterKeyMapList.push_back(String(matches[1].str().c_str()));
          newMSstring = std::regex_replace(matches[0].str().c_str(), ms_PathParameterReplace, "");
        }
        if (newMSstring.length() != 0) {
          reMix_APIPath += "/" + newMSstring;
        }
        token = strtok(NULL, "/");
      }
      APIPath = String(reMix_APIPath.c_str());
      ESP_LOGI("WS_API Setting", "註冊 API: [%s]%s", METHOD.c_str(), reMix_APIPath.c_str());
    };
  private:
};


extern C_Device_Ctrl Device_Ctrl;
extern AsyncWebServer asyncServer;
extern AsyncWebSocket ws;
extern Adafruit_SH1106 display;
#endif