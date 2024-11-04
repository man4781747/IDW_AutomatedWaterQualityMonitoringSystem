#ifndef WebsocketAPIFunctions_H
#define WebsocketAPIFunctions_H

#include <esp_log.h>
#include <ESPAsyncWebServer.h>
#include "MAIN__DeviceCtrl.h"
#include "SD.h"
#include <time.h>
#include "esp_task_wdt.h"
#include "ExternalFunctions.h"

/**
 * @brief 強制停止所有動作
 * 
 */
void ws_StopAllActionTask(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  ESP_LOGW("","收到來Websocket的儀器停止需求");
  Device_Ctrl.StopDeviceAllAction();
}

void ws_GetNowStatus(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();

  D_baseInfoJSON["status"].set("OK");
  D_baseInfoJSON["action"]["message"].set("OK");
  D_baseInfoJSON["action"]["status"].set("OK");
  D_baseInfoJSON["parameter"]["message"].set("OK");
  String returnString;
  serializeJsonPretty(D_baseInfoJSON, returnString);

  //? 2024-08-28 NodeRed端需求
  //? 希望開一個群組，群組收到訊息時，所有的回傳訊息是傳給原始訊號端以外的所有人
  if (String(server->url()) == "/ws/NodeRed") {
    for(const auto& c: server->getClients()){
      if(c->status() == WS_CONNECTED & c->id() != client->id()) {
        c-> binary(returnString);
      }
    }
  } else {
    client->binary(returnString);
  }
}

//!Pool結果資料

void ws_GetAllPoolData(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
  (*D_baseInfo)["status"].set("OK");
  (*D_baseInfo)["cmd"].set("poolData");
  (*D_baseInfo)["action"]["target"].set("PoolData");
  (*D_baseInfo)["action"]["method"].set("Update");
  (*D_baseInfo)["action"]["message"].set("OK");
  (*D_baseInfo)["action"]["status"].set("OK");
  if (!(*D_baseInfo)["action"].containsKey("message")) {
    (*D_baseInfo)["action"]["message"].set("獲得各蝦池最新感測器資料");
  }
  JsonArray parameterList = (*D_baseInfo).createNestedArray("parameter");
  for (JsonPair JsonPair_poolsSensorData : (*Device_Ctrl.JSON__sensorDataSave).as<JsonObject>()) {
    DynamicJsonDocument D_poolSensorDataSended(5000);
    JsonObject D_poolsSensorData = JsonPair_poolsSensorData.value();
    String S_PoolID = String(JsonPair_poolsSensorData.key().c_str());
    if (S_PoolID == "test") {
      continue;
    }
    int IsUsed = true;
    for (JsonVariant value : (*Device_Ctrl.CONFIG__pool.json_data).as<JsonArray>()) {
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
    D_poolSensorDataSended["PoolID"] = S_PoolID;
    D_poolSensorDataSended["PoolName"] = D_poolsSensorData["PoolName"].as<String>();
    D_poolSensorDataSended["PoolDescription"] = D_poolsSensorData["PoolDescription"].as<String>();
    // D_poolSensorDataSended["Used"] = false;
    JsonArray DataItemList = D_poolSensorDataSended.createNestedArray("DataItem");
    for (JsonPair JsonPair_SensorData : D_poolsSensorData["DataItem"].as<JsonObject>()) {
      JsonObject D_SensorData = JsonPair_SensorData.value();
      D_SensorData["ItemName"] = String(JsonPair_SensorData.key().c_str());
      DataItemList.add(D_SensorData);
    }
    parameterList.add(D_poolSensorDataSended);
  }
  String returnString;
  serializeJsonPretty((*D_baseInfo), returnString);
  (*D_baseInfo).clear();
  //? 2024-08-28 NodeRed端需求
  //? 希望開一個群組，群組收到訊息時，所有的回傳訊息是傳給原始訊號端以外的所有人
  if (String(server->url()) == "/ws/NodeRed") {
    for(const auto& c: server->getClients()){
      if(c->status() == WS_CONNECTED & c->id() != client->id()) {
        c-> binary(returnString);
      }
    }
  } else {
    client->binary(returnString);
  }
}

//! 2024/9/12 因 NodeRed 端需求，將原始 ws_GetAllPoolData() 的 API 給 NodeRed 專用
//! 本身操作網頁的 API 獨立出一個 Function 來使用
void ws_GetAllPoolData_ForWeb(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
  (*D_baseInfo)["status"].set("OK");
  (*D_baseInfo)["cmd"].set("poolData");
  (*D_baseInfo)["action"]["target"].set("PoolData");
  (*D_baseInfo)["action"]["method"].set("Update");
  (*D_baseInfo)["action"]["message"].set("OK");
  (*D_baseInfo)["action"]["status"].set("OK");
  if (!(*D_baseInfo)["action"].containsKey("message")) {
    (*D_baseInfo)["action"]["message"].set("獲得各蝦池最新感測器資料");
  }
  JsonArray parameterList = (*D_baseInfo).createNestedArray("parameter");
  for (JsonPair JsonPair_poolsSensorData : (*Device_Ctrl.JSON__sensorDataSave).as<JsonObject>()) {
    DynamicJsonDocument D_poolSensorDataSended(5000);
    JsonObject D_poolsSensorData = JsonPair_poolsSensorData.value();
    String S_PoolID = String(JsonPair_poolsSensorData.key().c_str());
    if (S_PoolID == "test") {
      continue;
    }
    D_poolSensorDataSended["PoolID"] = S_PoolID;
    D_poolSensorDataSended["PoolName"] = D_poolsSensorData["PoolName"].as<String>();
    D_poolSensorDataSended["PoolDescription"] = D_poolsSensorData["PoolDescription"].as<String>();
    JsonArray DataItemList = D_poolSensorDataSended.createNestedArray("DataItem");
    for (JsonPair JsonPair_SensorData : D_poolsSensorData["DataItem"].as<JsonObject>()) {
      JsonObject D_SensorData = JsonPair_SensorData.value();
      D_SensorData["ItemName"] = String(JsonPair_SensorData.key().c_str());
      DataItemList.add(D_SensorData);
    }

    parameterList.add(D_poolSensorDataSended);
  }
  String returnString;
  serializeJsonPretty((*D_baseInfo), returnString);
  (*D_baseInfo).clear();
  client->binary(returnString);
}


void ws_v2_RunPipeline(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  ESP_LOGD("WebSocket API", "收到Pipeline執行需求");

  //? 先判斷儀器是否空閒
  //! 注意，這邊流程只有失敗時會釋放互斥鎖，但如果收到訓則會將互斥鎖鎖起來，要記得再其他流程釋放他
  //! 如果執行失敗，要記得釋放
  if (xSemaphoreTake(Device_Ctrl.xMutex__pipelineFlowScan, 0) == pdTRUE) {
    DynamicJsonDocument singlePipelineSetting(60000);
    String stepChose = "";
    String eventChose = "";
    int eventIndexChose = -1;
    if ((*D_QueryParameter).containsKey("step")) {
      stepChose = (*D_QueryParameter)["step"].as<String>();
      if ((*D_QueryParameter).containsKey("event")) {
        eventChose = (*D_QueryParameter)["event"].as<String>();
        if ((*D_QueryParameter).containsKey("index")) {
          eventIndexChose = (*D_QueryParameter)["index"].as<String>().toInt();
        }
      }
    }
    String TargetName = D_PathParameter->as<JsonObject>()["name"];
    String FullFilePath = "/pipelines/"+TargetName+".json";
    singlePipelineSetting["FullFilePath"].set(FullFilePath);
    singlePipelineSetting["TargetName"].set(TargetName);
    singlePipelineSetting["stepChose"].set(stepChose);
    singlePipelineSetting["eventChose"].set(eventChose);
    singlePipelineSetting["eventIndexChose"].set(eventIndexChose);
    (*Device_Ctrl.JSON__pipelineStack).clear();
    (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
    ESP_LOGD("WebSocket", " - 檔案路徑:\t%s", FullFilePath.c_str());
    ESP_LOGD("WebSocket", " - 目標名稱:\t%s", TargetName.c_str());
    ESP_LOGD("WebSocket", " - 指定步驟:\t%s", stepChose.c_str());
    ESP_LOGD("WebSocket", " - 指定事件:\t%s", eventChose.c_str());
    ESP_LOGD("WebSocket", " - 指定事件編號:\t%d", eventIndexChose);
    // Device_Ctrl.LOAD__ACTION_V2(Device_Ctrl.JSON__pipelineStack);
    Device_Ctrl.BroadcastLogToClient(client,3,"已提交執行需求，請稍候");
  }
  else {
    ESP_LOGD("WebSocket", "儀器忙碌中");
    Device_Ctrl.BroadcastLogToClient(client,1,"儀器忙碌中，請稍後再試");
    // Device_Ctrl.SetLog(1,"儀器忙碌中，請稍後再試","",NULL, client, false);
  }
}

//! 儀器控制
void ws_v2_RunPwmMotor(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  if (xSemaphoreTake(Device_Ctrl.xMutex__pipelineFlowScan, 0) == pdTRUE) {
    DynamicJsonDocument tempFile(10000);
    //? 作法為了對其正式執行的流程, 若要測試儀器會先生成測試用的temp檔案
    int motorIndex = (*D_QueryParameter)["index"].as<int>();
    int motorStatus = (*D_QueryParameter)["status"].as<int>();
    tempFile["title"].set("手動觸發-伺服馬達測試");
    tempFile["desp"].set("");

    //? events 設定
    tempFile["events"]["test"]["title"].set("伺服馬達轉動");
    tempFile["events"]["test"]["desp"].set("伺服馬達轉動");
    JsonArray eventList = tempFile["events"]["test"].createNestedArray("event");
    DynamicJsonDocument eventObj(1000);
    JsonArray pwmMotorList = eventObj.createNestedArray("pwm_motor_list");
    DynamicJsonDocument indexObj(800);
    indexObj["index"].set(motorIndex);
    indexObj["status"].set(motorStatus);
    pwmMotorList.add(indexObj);
    eventList.add(eventObj);

    //? steps_group 設定
    tempFile["steps_group"]["test"]["title"].set("伺服馬達轉動");
    tempFile["steps_group"]["test"]["desp"].set("伺服馬達轉動");
    JsonArray stepsList = tempFile["steps_group"]["test"].createNestedArray("steps");
    stepsList.add("test");

    //? pipline 設定
    JsonArray piplineList = tempFile.createNestedArray("pipline");
    JsonArray piplineSetList = piplineList.createNestedArray();
    piplineSetList.add("test");
    DynamicJsonDocument emptyObject(1000);
    JsonArray emptyList = emptyObject.createNestedArray();
    piplineSetList.add(emptyList);


    // serializeJsonPretty(tempFile, Serial);
    File tempFileItem = SD.open("/pipelines/__temp__.json", FILE_WRITE);
    serializeJson(tempFile, tempFileItem);
    tempFileItem.close();

    DynamicJsonDocument singlePipelineSetting(10000);
    String TargetName = "__temp__.json";
    String FullFilePath = "/pipelines/__temp__.json";
    singlePipelineSetting["FullFilePath"].set(FullFilePath);
    singlePipelineSetting["TargetName"].set(TargetName);
    singlePipelineSetting["stepChose"].set("");
    singlePipelineSetting["eventChose"].set("");
    singlePipelineSetting["eventIndexChose"].set(-1);

    (*Device_Ctrl.JSON__pipelineStack).clear();
    (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
  }
  else {
    ESP_LOGI("", "儀器忙碌中，請稍後再試");
  }
}

void ws_v2_RunPeristalticMotor(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  if (xSemaphoreTake(Device_Ctrl.xMutex__pipelineFlowScan, 0) == pdTRUE) {
    DynamicJsonDocument tempFile(10000);
    //? 作法為了對其正式執行的流程, 若要測試儀器會先生成測試用的temp檔案
    int motorIndex = (*D_QueryParameter)["index"].as<int>();
    int motorStatus = (*D_QueryParameter)["status"].as<int>();
    double motorTime = (*D_QueryParameter)["time"].as<String>().toDouble();
    tempFile["title"].set("手動觸發-蠕動馬達測試");
    tempFile["desp"].set("index: "+String(motorIndex) +", status: "+String(motorStatus) +", time: "+String(motorTime));

    //? events 設定
    tempFile["events"]["test"]["title"].set("蠕動馬達轉動");
    tempFile["events"]["test"]["desp"].set("蠕動馬達轉動");
    JsonArray eventList = tempFile["events"]["test"].createNestedArray("event");
    DynamicJsonDocument eventObj(1000);
    JsonArray pwmMotorList = eventObj.createNestedArray("peristaltic_motor_list");
    DynamicJsonDocument indexObj(800);
    indexObj["index"].set(motorIndex);
    indexObj["status"].set(motorStatus);
    indexObj["time"].set(motorTime);
    indexObj["until"].set("-");
    indexObj["failType"].set("-");
    indexObj["failAction"].set("-");

    pwmMotorList.add(indexObj);
    eventList.add(eventObj);

    //? steps_group 設定
    tempFile["steps_group"]["test"]["title"].set("蠕動馬達轉動");
    tempFile["steps_group"]["test"]["desp"].set("蠕動馬達轉動");
    JsonArray stepsList = tempFile["steps_group"]["test"].createNestedArray("steps");
    stepsList.add("test");

    //? pipline 設定
    JsonArray piplineList = tempFile.createNestedArray("pipline");
    JsonArray piplineSetList = piplineList.createNestedArray();
    piplineSetList.add("test");
    DynamicJsonDocument emptyObject(1000);
    JsonArray emptyList = emptyObject.createNestedArray();
    piplineSetList.add(emptyList);
    File tempFileItem = SD.open("/pipelines/__temp__.json", FILE_WRITE);
    serializeJson(tempFile, tempFileItem);
    tempFileItem.close();
    DynamicJsonDocument singlePipelineSetting(10000);
    String TargetName = "__temp__.json";
    String FullFilePath = "/pipelines/__temp__.json";
    singlePipelineSetting["FullFilePath"].set(FullFilePath);
    singlePipelineSetting["TargetName"].set(TargetName);
    singlePipelineSetting["stepChose"].set("");
    singlePipelineSetting["eventChose"].set("");
    singlePipelineSetting["eventIndexChose"].set(-1);
    (*Device_Ctrl.JSON__pipelineStack).clear();
    (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
  }
  else {
    ESP_LOGI("", "儀器忙碌中，請稍後再試");
  }
}

//? [GET]/api/Pipeline/pool_all_data_get/RUN 這支API比較特別，目前是寫死的
//? 目的在於執行時，他會依序執行所有池的資料，每池檢測完後丟出一次NewData
void ws_RunAllPoolPipeline(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  ESP_LOGD("WebSocket API", "收到多項Pipeline執行需求");
  if ((*D_QueryParameter).containsKey("order")) {
    String orderString = (*D_QueryParameter)["order"].as<String>();
    if (orderString.length() == 0) {
      ESP_LOGD("WebSocket API", "執行蝦池數值檢測流程失敗，order參數不得為空");
    }
    else {
      DynamicJsonDocument NewPipelineSetting(60000);
      int splitIndex = -1;
      int prevSpliIndex = 0;
      int eventCount = 0;
      splitIndex = orderString.indexOf(',', prevSpliIndex);
      // (*Device_Ctrl.JSON__pipelineStack).clear();
      while (splitIndex != -1) {
        String token = orderString.substring(prevSpliIndex, splitIndex);
        String TargetName = token+".json";
        String FullFilePath = "/pipelines/"+TargetName;
        DynamicJsonDocument singlePipelineSetting(10000);
        singlePipelineSetting["FullFilePath"].set(FullFilePath);
        singlePipelineSetting["TargetName"].set(TargetName);
        singlePipelineSetting["stepChose"].set("");
        singlePipelineSetting["eventChose"].set("");
        singlePipelineSetting["eventIndexChose"].set(-1);
        // (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
        NewPipelineSetting.add(singlePipelineSetting);
        prevSpliIndex = splitIndex +1;
        eventCount++;
        splitIndex = orderString.indexOf(',', prevSpliIndex);
        ESP_LOGD("WebSocket", " - 事件 %d", eventCount);
        ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
        ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
      }
      String lastToken = orderString.substring(prevSpliIndex);
      if (lastToken.length() > 0) {
        String TargetName = lastToken+".json";
        String FullFilePath = "/pipelines/"+TargetName;
        DynamicJsonDocument singlePipelineSetting(3000);
        singlePipelineSetting["FullFilePath"].set(FullFilePath);
        singlePipelineSetting["TargetName"].set(TargetName);
        singlePipelineSetting["stepChose"].set("");
        singlePipelineSetting["eventChose"].set("");
        singlePipelineSetting["eventIndexChose"].set(-1);
        // (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
        NewPipelineSetting.add(singlePipelineSetting);
        ESP_LOGD("WebSocket", " - 事件 %d", eventCount+1);
        ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
        ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
      }
      if (!RunNewPipeline(NewPipelineSetting)) {
        Device_Ctrl.BroadcastLogToClient(client,1,"儀器忙碌中，請稍後再試");
      }
    }
  } 
  else {
    ESP_LOGD("WebSocket API", "執行蝦池數值檢測流程失敗，API需要order參數");
  }
}

//? 
void ws_GetSensorData(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData) {
  //? 初始化個參數，如果沒指定則使用預設值
  String startDateString = (*D_QueryParameter)["st"].as<String>();
  if (startDateString.length() != 8) {startDateString = GetDateString("");}
  String endDateString = (*D_QueryParameter)["et"].as<String>();
  if (endDateString.length() != 8) {endDateString = GetDateString("");}
  String typeNmaeString = (*D_QueryParameter)["tp"].as<String>();
  if (typeNmaeString == "null") {typeNmaeString = "pH,NO2,NH4";}
  String poolNameString = (*D_QueryParameter)["pl"].as<String>();
  if (poolNameString == "null") {poolNameString = "pool-1,pool-2,pool-3,pool-4";}
  


  //? 處理 pl 文字分割
  int poolTargetCount = 0;
  String poolTarget[10];
  splitString(poolNameString, ',', poolTarget, poolTargetCount);
  Serial.printf("%s, %d\n",poolNameString.c_str(), poolTargetCount);

  //? 處理 tp 文字分割
  int typeTargetCount = 0;
  String typeTarget[10];
  splitString(typeNmaeString, ',', typeTarget, typeTargetCount);
  Serial.printf("%s, %d\n",typeNmaeString.c_str(), typeTargetCount);

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
  
  while (mktime(&endTime)>=mktime(&startTime)) {
    strftime(dateStringBuffer, sizeof(dateStringBuffer), "%Y%m%d", &endTime);
    String dateString = String(dateStringBuffer);
    time_t unixTime = mktime(&endTime);
    for (int poolChose=0;poolChose<poolTargetCount;poolChose++) {
      for (int typeChose=0;typeChose<typeTargetCount;typeChose++) {
        String FileFullPath = "/datas/"+dateString+"/"+poolTarget[poolChose]+"/"+typeTarget[typeChose]+".bin";
        if (SD.exists(FileFullPath)) {
          Serial.println(FileFullPath);
          File sensorFile = SD.open(FileFullPath, FILE_READ);
          int fileSize = sensorFile.size(); 
          uint8_t fileDataBuffer[fileSize+7];
          // uint8_t* fileDataBuffer = (uint8_t*)malloc(fileSize+7);
          fileDataBuffer[0] = 0b00000000;
          fileDataBuffer[1] = Device_Ctrl.mappingPoolNameToID(poolTarget[poolChose]);
          fileDataBuffer[2] = Device_Ctrl.mappingTypeNameToID(typeTarget[typeChose]);
          for (int i = 0; i < 4; i++) {
            fileDataBuffer[3+i] = (unixTime >> (i * 8)) & 0xFF;
          }
          sensorFile.read(fileDataBuffer+7, fileSize);
          sensorFile.close();
          client->binary(fileDataBuffer, fileSize+7);
          esp_task_wdt_reset();
        }
        esp_task_wdt_reset();
      }
    }
    endTime.tm_mday -= 1;
  }

}


#endif