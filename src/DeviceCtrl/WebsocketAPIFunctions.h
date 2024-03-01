#ifndef WebsocketAPIFunctions_H
#define WebsocketAPIFunctions_H

#include <esp_log.h>
#include <ESPAsyncWebServer.h>
#include "MAIN__DeviceCtrl.h"
#include "SD.h"

/**
 * @brief 強制停止所有動作
 * 
 */
void ws_StopAllActionTask(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  ESP_LOGW("","收到來Websocket的儀器停止需求");
  Device_Ctrl.StopDeviceAllAction();
  // Device_Ctrl.StopDeviceAndINIT();
  // Device_Ctrl.CleanAllStepTask();
  // Device_Ctrl.SetLog(
  //   2,
  //   "儀器排程被強制停止",
  //   "儀器排程被強制停止" ,
  //   server, NULL
  // );
}



// void ws_GetDeiveConfig(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
// {
//   JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
//   D_baseInfoJSON["status"].set("OK");
//   D_baseInfoJSON["action"]["target"].set("DeiveConfig");
//   D_baseInfoJSON["action"]["method"].set("Update");
//   D_baseInfoJSON["parameter"].set(Device_Ctrl.JSON__DeviceBaseInfo->as<JsonObject>());
//   String returnString;
//   serializeJsonPretty(D_baseInfoJSON, returnString);
//   client->binary(returnString);
// }

// void ws_PatchDeiveConfig(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
// {
//   JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
//   JsonObject D_oldConfig = (*Device_Ctrl.JSON__DeviceBaseInfo).as<JsonObject>();;
//   JsonObject D_newConfig = D_FormData->as<JsonObject>();
//   for (JsonPair newConfigItem : D_newConfig) {
//     if (D_oldConfig[newConfigItem.key()].as<String>() != newConfigItem.value().as<String>()) {
//       D_oldConfig[newConfigItem.key()].set(newConfigItem.value().as<String>());
//     }
//   }
//   D_baseInfoJSON["status"].set("OK");
//   D_baseInfoJSON["action"]["target"].set("DeiveConfig");
//   D_baseInfoJSON["action"]["method"].set("Update");
//   D_baseInfoJSON["parameter"].set(D_oldConfig);
//   String returnString;
//   serializeJsonPretty(D_baseInfoJSON, returnString);
//   client->binary(returnString);
// }


void ws_GetNowStatus(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
{
  JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();

  D_baseInfoJSON["status"].set("OK");
  D_baseInfoJSON["action"]["message"].set("OK");
  D_baseInfoJSON["action"]["status"].set("OK");
  D_baseInfoJSON["parameter"]["message"].set("OK");
  String returnString;
  serializeJsonPretty(D_baseInfoJSON, returnString);
  client->binary(returnString);
}

// //!LOG相關API

// void ws_GetLogs(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
// {
//   JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
//   int MaxLogNum = 10;
//   if ((*D_QueryParameter).containsKey("max")) {
//     MaxLogNum = (*D_QueryParameter)["max"].as<int>();
//   }
//   D_baseInfoJSON["status"].set("OK");
//   D_baseInfoJSON["action"]["message"].set("OK");
//   D_baseInfoJSON["action"]["status"].set("OK");
//   D_baseInfoJSON["action"]["target"].set("LogHistory");
//   D_baseInfoJSON["action"]["method"].set("Update");

//   JsonArray logsArray = D_baseInfoJSON["parameter"].createNestedArray("logs");

//   JsonArray logSaves = (*Device_Ctrl.JSON__DeviceLogSave).as<JsonArray>();

//   int lastIndex = logSaves.size() - 1;
//   int startIndex = lastIndex - MaxLogNum;
//   if (startIndex < 0) {
//     startIndex = 0;
//   }
//   for (int indexChose=startIndex;indexChose<lastIndex;indexChose++) {
//     logsArray.add(logSaves[indexChose].as<JsonObject>());
//   }

//   // int logCount = 0;
//   String returnString;
//   serializeJsonPretty(D_baseInfoJSON, returnString);
//   client->binary(returnString);
// }

// //!Pool結果資料

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
    // serializeJsonPretty((*Device_Ctrl.JSON__PoolConfig), Serial);
    for (JsonVariant value : (*Device_Ctrl.JSON__PoolConfig).as<JsonArray>()) {
      JsonObject PoolConfigItem = value.as<JsonObject>();
      // serializeJsonPretty(PoolConfigItem, Serial);
      if (PoolConfigItem["id"].as<String>() == S_PoolID) {
        if (PoolConfigItem["external_mapping"].as<String>() != "") {
          S_PoolID = PoolConfigItem["external_mapping"].as<String>();
        }
        break;
      }
    }
    // Serial.println(S_PoolID);
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




  // (*D_baseInfo)["parameter"].set(*Device_Ctrl.JSON__sensorDataSave);
  String returnString;
  serializeJsonPretty((*D_baseInfo), returnString);
  client->binary(returnString);
  (*D_baseInfo).clear();
}


// //!蝦池設定相關API

// void ws_PatchPoolInfo(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
// {
//   String TargetName = D_PathParameter->as<JsonObject>()["name"];
//   ESP_LOGD("Websocket", "Patch Spectrophotometer Name: %s", TargetName.c_str());
//   JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
//   JsonObject D_pools = (*Device_Ctrl.JSON__DeviceBaseInfo)["pools"].as<JsonObject>();
//   if (D_pools.containsKey(TargetName)) {
//     JsonObject D_newConfig = D_FormData->as<JsonObject>();
//     JsonObject D_oldConfig = D_pools[TargetName];
//     for (JsonPair newConfigItem : D_newConfig) {
//       if (D_oldConfig[newConfigItem.key()].as<String>() != newConfigItem.value().as<String>()) {
//         D_oldConfig[newConfigItem.key()].set(newConfigItem.value().as<String>());
//       }
//     }

//     D_baseInfoJSON["parameter"][TargetName].set(D_oldConfig);
//     D_baseInfoJSON["action"]["status"].set("OK");
//     D_baseInfoJSON["action"]["message"].set("更新事件組設定完畢");
//     D_baseInfoJSON["action"]["target"].set("Pool");
//     D_baseInfoJSON["action"]["method"].set("Update");
//     String returnString;
//     serializeJsonPretty(D_baseInfoJSON, returnString);
//     server->binaryAll(returnString);
//     Device_Ctrl.SetLog(
//       5,
//       "更新事件組設定完畢",
//       "事件組名稱: " + D_oldConfig["title"].as<String>(),
//       server, NULL
//     );
//     Device_Ctrl.SPIFFS__ReWriteDeviceBaseInfo();
//   } else {
//     Device_Ctrl.SetLog(
//       1,
//       "更新蝦池設定失敗",
//       "找不到蝦池設定: " + TargetName,
//       NULL, client
//     );
//   }

// }

// void ws_DeletePoolInfo(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
// {
//   String TargetName = D_PathParameter->as<JsonObject>()["name"];
//   ESP_LOGD("Websocket", "Get Peristaltic Motor Name: %s", TargetName.c_str());
//   JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
//   JsonObject D_pools = (*Device_Ctrl.JSON__DeviceBaseInfo)["pools"].as<JsonObject>();
//   if (D_pools.containsKey(TargetName)) {
//     D_pools.remove(TargetName);
//     D_baseInfoJSON["action"]["status"].set("OK");
//     D_baseInfoJSON["action"]["message"].set("刪除蝦池設定完畢");
//     D_baseInfoJSON["action"]["method"] = "Delete";
//     D_baseInfoJSON["action"]["target"] = "Pool";
//     D_baseInfoJSON["parameter"]["delete_id"] = TargetName;
//     String returnString;
//     serializeJsonPretty(D_baseInfoJSON, returnString);
//     server->binaryAll(returnString);
//     Device_Ctrl.SetLog(
//       5,
//       "刪除蝦池設定完畢",
//       "蝦池設定ID: " + TargetName,
//       server, NULL
//     );
//     Device_Ctrl.SPIFFS__ReWriteDeviceBaseInfo();

//   } else {
//     Device_Ctrl.SetLog(
//       1,
//       "更新蝦池設定失敗",
//       "找不到蝦池設定: " + TargetName,
//       NULL, client
//     );
//   }
// }

// void ws_GetPoolInfo(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
// {
//   String TargetName = D_PathParameter->as<JsonObject>()["name"];
//   ESP_LOGD("Websocket", "Get Pools Name: %s", TargetName.c_str());
//   JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
//   JsonObject D_pools = (*Device_Ctrl.JSON__DeviceBaseInfo)["pools"].as<JsonObject>();
//   if (D_pools.containsKey(TargetName)) {
//     D_baseInfoJSON["action"]["status"].set("OK");
//     D_baseInfoJSON["action"]["message"].set("查詢蝦池設定完畢");
//     D_baseInfoJSON["action"]["target"].set("Pool");
//     D_baseInfoJSON["action"]["method"].set("Get");
//     D_baseInfoJSON["parameter"][TargetName].set(D_pools[TargetName]);
//     String returnString;
//     serializeJsonPretty(D_baseInfoJSON, returnString);
//     client->binary(returnString);
//   } else {
//     Device_Ctrl.SetLog(
//       1,
//       "查詢蝦池設定失敗",
//       "找不到蝦池設定: " + TargetName,
//       NULL, client
//     );
//   }
// }

// void ws_GetAllPoolInfo(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
// {
//   JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
//   JsonObject D_pools = (*Device_Ctrl.JSON__DeviceBaseInfo)["pools"].as<JsonObject>();
//   D_baseInfoJSON["parameter"].set(D_pools);

//   D_baseInfoJSON["action"]["status"].set("OK");
//   D_baseInfoJSON["action"]["message"].set("查詢所有蝦池設定完畢");
//   D_baseInfoJSON["action"]["target"].set("Pool");
//   D_baseInfoJSON["action"]["method"].set("Update");
//   String returnString;
//   serializeJsonPretty(D_baseInfoJSON, returnString);
//   client->binary(returnString);
// }

// void ws_AddNewPoolInfo(AsyncWebSocket *server, AsyncWebSocketClient *client, DynamicJsonDocument *D_baseInfo, DynamicJsonDocument *D_PathParameter, DynamicJsonDocument *D_QueryParameter, DynamicJsonDocument *D_FormData)
// {
//   JsonObject D_baseInfoJSON = D_baseInfo->as<JsonObject>();
//   JsonObject D_newConfig = D_FormData->as<JsonObject>();
//   if (!D_newConfig.containsKey("title")) {
//     Device_Ctrl.SetLog(
//       1,
//       "新增蝦池設定失敗",
//       "title 參數為必要項目",
//       NULL, client
//     );
//   } else {
//     char random_name[16];
//     uint8_t random_bytes[8];
//     esp_fill_random(random_bytes, sizeof(random_bytes));
//     for (int i = 0; i < sizeof(random_bytes); i++) {
//       sprintf(&random_name[i*2], "%02x", random_bytes[i]);
//     }
//     JsonObject D_pools = (*Device_Ctrl.JSON__DeviceBaseInfo)["pools"].as<JsonObject>();
//     D_pools[String(random_name)]["title"].set(D_newConfig["title"].as<String>());
//     D_pools[String(random_name)]["desp"].set(D_newConfig["desp"].as<String>());
//     D_baseInfoJSON["action"]["status"].set("OK");
//     D_baseInfoJSON["action"]["target"].set("Pool");
//     D_baseInfoJSON["action"]["method"].set("Update");
//     D_baseInfoJSON["parameter"][String(random_name)].set(D_pools[String(random_name)]);
//     String returnString;
//     serializeJsonPretty(D_baseInfoJSON, returnString);
//     server->binaryAll(returnString);
//     Device_Ctrl.SetLog(
//       5,
//       "新增蝦池設定: "+D_newConfig["title"].as<String>(),
//       "說明: " + D_newConfig["desp"].as<String>(),
//       server, NULL
//     );

//     Device_Ctrl.SPIFFS__ReWriteDeviceBaseInfo();
//   }
// }



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
    
    // Device_Ctrl.LOAD__ACTION_V2(Device_Ctrl.JSON__pipelineStack);
  }
  else {
    ESP_LOGI("", "儀器忙碌中，請稍後再試");
    // Device_Ctrl.SetLog(
    //   1,
    //   "儀器忙碌中，請稍後再試",
    //   "",
    //   NULL, client, false
    // );
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
    
    // Device_Ctrl.LOAD__ACTION_V2(Device_Ctrl.JSON__pipelineStack);
  }
  else {
    ESP_LOGI("", "儀器忙碌中，請稍後再試");
    // Device_Ctrl.SetLog(
    //   1,
    //   "儀器忙碌中，請稍後再試",
    //   "",
    //   NULL, client, false
    // );
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






  // if (xSemaphoreTake(Device_Ctrl.xMutex__pipelineFlowScan, 0) == pdTRUE) {
  //   if ((*D_QueryParameter).containsKey("order")) {
  //     String orderString = (*D_QueryParameter)["order"].as<String>();
  //     if (orderString.length() == 0) {
  //       ESP_LOGD("WebSocket API", "執行蝦池數值檢測流程失敗，order參數不得為空");
  //       // Device_Ctrl.SetLog(1, "執行蝦池數值檢測流程失敗", "order參數不得為空", NULL, client, false);
  //       xSemaphoreGive(Device_Ctrl.xMutex__pipelineFlowScan);
  //     }
  //     else {
  //       //* 分割order文字
  //       int splitIndex = -1;
  //       int prevSpliIndex = 0;
  //       int eventCount = 0;
  //       splitIndex = orderString.indexOf(',', prevSpliIndex);
  //       (*Device_Ctrl.JSON__pipelineStack).clear();
  //       while (splitIndex != -1) {
  //         String token = orderString.substring(prevSpliIndex, splitIndex);
  //         String TargetName = token+".json";
  //         String FullFilePath = "/pipelines/"+TargetName;
  //         DynamicJsonDocument singlePipelineSetting(10000);
  //         singlePipelineSetting["FullFilePath"].set(FullFilePath);
  //         singlePipelineSetting["TargetName"].set(TargetName);
  //         singlePipelineSetting["stepChose"].set("");
  //         singlePipelineSetting["eventChose"].set("");
  //         singlePipelineSetting["eventIndexChose"].set(-1);
  //         (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
  //         prevSpliIndex = splitIndex +1;
  //         eventCount++;
  //         splitIndex = orderString.indexOf(',', prevSpliIndex);
  //         ESP_LOGD("WebSocket", " - 事件 %d", eventCount);
  //         ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
  //         ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
  //       }
  //       String lastToken = orderString.substring(prevSpliIndex);
  //       if (lastToken.length() > 0) {
  //         String TargetName = lastToken+".json";
  //         String FullFilePath = "/pipelines/"+TargetName;
  //         DynamicJsonDocument singlePipelineSetting(10000);
  //         singlePipelineSetting["FullFilePath"].set(FullFilePath);
  //         singlePipelineSetting["TargetName"].set(TargetName);
  //         singlePipelineSetting["stepChose"].set("");
  //         singlePipelineSetting["eventChose"].set("");
  //         singlePipelineSetting["eventIndexChose"].set(-1);
  //         (*Device_Ctrl.JSON__pipelineStack).add(singlePipelineSetting);
  //         ESP_LOGD("WebSocket", " - 事件 %d", eventCount+1);
  //         ESP_LOGD("WebSocket", "   - 檔案路徑:\t%s", FullFilePath.c_str());
  //         ESP_LOGD("WebSocket", "   - 目標名稱:\t%s", TargetName.c_str());
  //       }
        
        
  //       // Device_Ctrl.LOAD__ACTION_V2(Device_Ctrl.JSON__pipelineStack);
  //     }
  //   } 
  //   else {
  //     ESP_LOGD("WebSocket API", "執行蝦池數值檢測流程失敗，API需要order參數");
  //     // Device_Ctrl.SetLog(1,"執行蝦池數值檢測流程失敗","API需要order參數",NULL, client, false);
  //     xSemaphoreGive(Device_Ctrl.xMutex__pipelineFlowScan);
  //   }
  // }
  // else {
  //   ESP_LOGD("WebSocket API", "儀器忙碌中");
  //   // Device_Ctrl.SetLog(2,"執行流程設定失敗","儀器忙碌中，請稍後再試",NULL, client, false);  
  // }
}

#endif