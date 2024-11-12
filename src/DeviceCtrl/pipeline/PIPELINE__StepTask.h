#ifndef StepTask_H
#define StepTask_H

#include <esp_log.h>
#include <ArduinoJson.h>
#include "../MAIN__DeviceCtrl.h"
#include "lx_20s.h"
#include "PeristalticMotorCtrl.h"
#include <INA226.h>
#include "TimeLibExternalFunction.h"
#include "CalcFunction.h"
#include <driver/ledc.h>
#include <driver/timer.h>
#include <ModbusRTU.h>

#include "DoAction/common.h"
#include "DoAction/servo.h"



void StopStep(StepTaskDetail* StepTaskDetailItem);
int Do_WaitAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);
StepResult Do_PeristalticMotorAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);
StepResult Do_SpectrophotometerAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);
StepResult Do_PHmeterAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);
void StepTask(void* parameter) {
  //! StepTaskDetailItem 指針來源於 Device_Ctrl.StepTaskDetailList[i]
  //! 其中 i 代表不同 task index
  //! 這些 StepTask 目前指定在 CPU 1 中執行
  StepTaskDetail* StepTaskDetailItem = (StepTaskDetail*)parameter;
  ESP_LOGD(StepTaskDetailItem->TaskName.c_str(),"開始執行Step執行Task");
  bool EmergencyStop = false;
  bool OnlyStepStop = false;
  bool OnlyPipelineStop = false;
  for (;;) {
    if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
      //? 此處代表Task外部將 Device_Ctrl.StepTaskDetailList[i].TaskStatus 強制設定為 Close
      //? 目的為停止 Task 運作
      StopStep(StepTaskDetailItem);
      continue;
    }
    if (StepTaskDetailItem->StepName == NULL | StepTaskDetailItem->StepName == "") {
      //? Step 被啟動執行的條件是首先會需要一個 StepName
      vTaskDelay(1000/portTICK_PERIOD_MS);
      continue;
    }
    String pipelineName = StepTaskDetailItem->PipelineName;

    String stepsGroupNameString = StepTaskDetailItem->StepName;
    //? 這個 Task 要執行的 steps_group 的 title
    String ThisStepGroupTitle = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["title"].as<String>();
    //? 這個 Task 要執行的 steps_group 的 list
    JsonArray stepsGroupArray = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["steps"].as<JsonArray>();
    //? 這個 Task 要執行的 parent list
    JsonObject parentList = (*Device_Ctrl.JSON__pipelineConfig)["pipline"][stepsGroupNameString]["parentList"].as<JsonObject>();


    ESP_LOGD(StepTaskDetailItem->TaskName.c_str(),"收到運作要求，目標Step名稱: %s，Title: %s", stepsGroupNameString.c_str(), ThisStepGroupTitle.c_str());
    Device_Ctrl.WritePipelineLogFile(
      Device_Ctrl.Pipeline_LogFileName, 
      "(%s) Received operation request, target Step name: %s, Title: %s", 
      StepTaskDetailItem->TaskName.c_str(),
      StepTaskDetailItem->StepName.c_str(),
      ThisStepGroupTitle.c_str()
    );

    // Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 0, "收到運作要求，目標Step名稱: %s，Title: %s", stepsGroupNameString.c_str(), ThisStepGroupTitle.c_str());
    // Device_Ctrl.BroadcastLogToClient(NULL, 0, "收到運作要求，目標Step名稱: %s，Title: %s", stepsGroupNameString.c_str(), ThisStepGroupTitle.c_str());

    bool isStepFail = false;
    //? 一個一個event依序執行
    for (String eventChose : stepsGroupArray) {
      //? eventChose: 待執行的event名稱
      String thisEventTitle = (*Device_Ctrl.JSON__pipelineConfig)["events"][eventChose]["title"].as<String>();
      ESP_LOGI(StepTaskDetailItem->TaskName.c_str(), "執行: %s - %s", ThisStepGroupTitle.c_str(), thisEventTitle.c_str());
      // Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3, "執行: %s - %s", ThisStepGroupTitle.c_str(), thisEventTitle.c_str());
      Device_Ctrl.BroadcastLogToClient(NULL, 3, "執行: %s - %s", ThisStepGroupTitle.c_str(), thisEventTitle.c_str());
      Device_Ctrl.WritePipelineLogFile(
        Device_Ctrl.Pipeline_LogFileName, 
        "(%s) Run: %s - %s", 
        StepTaskDetailItem->TaskName.c_str(),
        ThisStepGroupTitle.c_str(),
        thisEventTitle.c_str()
      );

      //? event細節執行內容
      JsonArray eventList = (*Device_Ctrl.JSON__pipelineConfig)["events"][eventChose]["event"].as<JsonArray>();
      //? 一個一個event item依序執行
      for (JsonObject eventItem : eventList) {
        if (eventItem.containsKey("pwm_motor_list")) {
          DoServoAction actionItem = DoServoAction(eventItem, StepTaskDetailItem);
          actionItem.Run();
          // StepResult actionResult = DoServoAction(eventItem, StepTaskDetailItem).Run();
          // StepResult actionResult = Do_ServoMotorAction(eventItem, StepTaskDetailItem);
          if (actionItem.result_code == ResultCode::STOP_DEVICE) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += actionItem.ResultMessage;
            sendString += "\n後續處理方法: 整機停止\n";
            Device_Ctrl.AddLineNotifyEvent(sendString);
            Device_Ctrl.AddGmailNotifyEvent("機台錯誤訊息",sendString);
            isStepFail = true;
            EmergencyStop = true;
            break;
          }
        }
        else if (eventItem.containsKey("wait")) {
          if (Do_WaitAction(eventItem, StepTaskDetailItem) != 1) {
            isStepFail = true;
            EmergencyStop = true;
            break;
          }
        }
        else if (eventItem.containsKey("peristaltic_motor_list")) {
          StepResult actionResult = Do_PeristalticMotorAction(eventItem, StepTaskDetailItem);
          if (actionResult.code == ResultCode::STOP_BY_OUTSIDE) {
            //! 收到停止
            isStepFail = true;
            OnlyStepStop = true;
            break;
          }
          else if (actionResult.code == ResultCode::STOP_THIS_PIPELINE) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += actionResult.message;
            sendString += "後續處理方法: 跳過當前流程\n";
            Device_Ctrl.AddLineNotifyEvent(sendString);
            // Device_Ctrl.SendLineNotifyMessage(FailMessage);
            Device_Ctrl.AddGmailNotifyEvent("機台錯誤訊息",sendString);
            // Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
            isStepFail = true;
            OnlyPipelineStop = true;
            break;
          }
          else if (actionResult.code == ResultCode::STOP_DEVICE) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += actionResult.message;
            sendString += "後續處理方法: 整機停止\n";
            Device_Ctrl.AddLineNotifyEvent(sendString);
            // Device_Ctrl.SendLineNotifyMessage(FailMessage);
            Device_Ctrl.AddGmailNotifyEvent("機台錯誤訊息",sendString);
            // Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
            isStepFail = true;
            EmergencyStop = true;
            break;
          }
        }
        else if (eventItem.containsKey("spectrophotometer_list")) {
          StepResult actionResult = Do_SpectrophotometerAction(eventItem, StepTaskDetailItem);
          if (actionResult.code == ResultCode::KEEP_RUN) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += actionResult.message;
            sendString += "後續處理方法: 繼續執行\n";
            Device_Ctrl.AddLineNotifyEvent(sendString);
            // Device_Ctrl.SendLineNotifyMessage(FailMessage);
            Device_Ctrl.AddGmailNotifyEvent("機台錯誤訊息",sendString);
            // Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
          }
        }
        else if (eventItem.containsKey("ph_meter")) {
          StepResult actionResult = Do_PHmeterAction(eventItem, StepTaskDetailItem);
          if (actionResult.code == ResultCode::KEEP_RUN) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += actionResult.message;
            sendString += "後續處理方法: 繼續執行\n";
            Device_Ctrl.AddLineNotifyEvent(sendString);
            // Device_Ctrl.SendLineNotifyMessage(FailMessage);
            Device_Ctrl.AddGmailNotifyEvent("機台錯誤訊息",sendString);
            // Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
          }
        }
      
      
      }


      //! 提早結束原因判斷
      if (EmergencyStop) {
        //! 收到緊急停止整台儀器動作的要求
        Device_Ctrl.StopDeviceAllAction();
        EmergencyStop = false;
        isStepFail = true;
        break;
      } else if (OnlyStepStop) {
        //! 收到緊急停止這個Step的要求
        OnlyStepStop = false;
        isStepFail = true;
        break;
      } else if (OnlyPipelineStop) {
        //! 收到緊急停止當前Pipeline的要求
        Device_Ctrl.StopNowPipelineAndAllStepTask();
        OnlyPipelineStop = false;
        isStepFail = true;
        break;
      }
    }


    //? 所有event結束後，最終判斷這個Step是否標示為失敗or成功
    if (isStepFail) {
      (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["RESULT"].set("FAIL");
    }
    else {
      (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["RESULT"].set("SUCCESS");
    }

    StopStep(StepTaskDetailItem);
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

//! 清空必要項目，使Task回歸Idle狀態
void StopStep(StepTaskDetail* StepTaskDetailItem) {
  ESP_LOGD("","Step Task: %s 執行流程完畢", StepTaskDetailItem->TaskName.c_str());
  Device_Ctrl.WritePipelineLogFile(Device_Ctrl.Pipeline_LogFileName, "(%s) Step Task: %s The execution process is completed", StepTaskDetailItem->TaskName.c_str(), StepTaskDetailItem->StepName.c_str());
  StepTaskDetailItem->StepName = "";
  StepTaskDetailItem->TaskStatus = StepTaskStatus::Idle;
}


/** 蠕動馬達功能設定
 *  可設定參數: 
 *    1. (必要/int)index: 馬達編號
 *    2. (必要/int)status: 0 不轉、1 正轉、2 反轉
 *    3. (必要/float)time: 旋轉秒數
 *    4. (str)until: 觸發停止目標物: "RO"、"SAMPLE"、"-"
 *    5. (str)failType: 錯誤觸發類型: "timeout"、"-"
 *    6. (str)failAction: 錯誤後行為: "stopPipeline"、"continue"
 *    7. (str)pool: 抽取耗時記錄目標: pool-1 ~ pool-4、無
 *    8. (str)consumeTarget: 消耗記數目標: "RO"、"NO2_R1"、"NH4_R1"、"NH4_R2"
 *    9. (int)consumeNum: 消耗記數數量
 *   10. (float)consumeRate: 消耗記數比率
 *  設定檔範例: 
  "peristaltic_motor_list": [{
    "index": 1,
    "status": 1,
    "time": 240,
    "until": "RO",
    "consumeTarget": "RO",
    "consumeNum": 1,
    "failType": "timeout",
    "failAction": "stopPipeline",
    "pool": "pool-1"
  }]
 */
StepResult Do_PeristalticMotorAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  StepResult result;
  pinMode(PIN__EN_Peristaltic_Motor, OUTPUT);
  //? endTimeCheckList: 記錄各蠕動馬達最大運行結束時間
  DynamicJsonDocument endTimeCheckList(10000);
  for (JsonObject peristalticMotorItem : eventItem["peristaltic_motor_list"].as<JsonArray>()) {
    //? motorIndex: 本次event指定的馬達INDEX
    int motorIndex = peristalticMotorItem["index"].as<int>();
    String motorIndexString = String(motorIndex);
    //? runTime: 馬達預計運行狀態
    PeristalticMotorStatus status = peristalticMotorItem["status"].as<int>() == 1 ? PeristalticMotorStatus::FORWARD : PeristalticMotorStatus::REVERSR;
    //? runTime: 馬達預計運行時長
    float runTime = peristalticMotorItem["time"].as<String>().toFloat();
    //? untilString: 判斷是否停止的依據
    //? "-" : 無
    //? "SAMPLE" : 池水浮球觸發
    //? "RO" : RO浮球觸發
    String untilString = peristalticMotorItem["until"].as<String>();
    //? failType: 錯誤判斷的準則，目前有幾種類型
    //? 1. no: 無錯誤判斷
    //? 2. timeout: 超時
    //? 3. connect: 指定pin導通
    String failType = peristalticMotorItem["failType"].as<String>();

    //? failAction: 觸發錯誤判斷時的反應，目前有幾種類型
    //? 1. continue: 繼續執行，但最後流程Task完成後會標記為Fail
    //? 2. stepStop: 當前流程Task關閉
    //? 3. stopImmediately: 整台機器停止動作
    String failAction = peristalticMotorItem["failAction"].as<String>();

    String consumeTarget = peristalticMotorItem["consumeTarget"].as<String>();
    double consumeNum = peristalticMotorItem["consumeNum"].as<double>();

    //? 是否紀錄初次抽水耗時資料
    String firstPumpingTimePoolTarget = peristalticMotorItem["pool"].as<String>();

    if (endTimeCheckList.containsKey(motorIndexString)) {
      continue;
    }

    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"[%s]蠕動馬達(%d) %s 持續 %.2f 秒或是直到%s被觸發,failType: %s, failAction: %s", 
      StepTaskDetailItem->PipelineName.c_str(),
      motorIndex, 
      peristalticMotorItem["status"].as<int>()==-1 ? "正轉" : "反轉", 
      runTime,
      untilString.c_str(),  
      failType.c_str(), failAction.c_str()
    );

    endTimeCheckList[motorIndexString]["index"] = motorIndex;
    endTimeCheckList[motorIndexString]["status"] = status;
    endTimeCheckList[motorIndexString]["time"] = runTime;
    endTimeCheckList[motorIndexString]["failType"] = failType;
    endTimeCheckList[motorIndexString]["failAction"] = failAction;
    endTimeCheckList[motorIndexString]["finish"] = false;
    endTimeCheckList[motorIndexString]["consumeTarget"] = consumeTarget;
    endTimeCheckList[motorIndexString]["consumeNum"] = consumeNum;
    endTimeCheckList[motorIndexString]["pool"] = firstPumpingTimePoolTarget;
    
    if (untilString == "RO") {
      endTimeCheckList[motorIndexString]["until"] = PIN__ADC_RO_FULL;
    } 
    else if (untilString == "SAMPLE") {
      endTimeCheckList[motorIndexString]["until"] = PIN__ADC_POOL_FULL;
    }
    else {
      endTimeCheckList[motorIndexString]["until"] = -1;
    }
    Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, status);
    endTimeCheckList[motorIndexString]["startTime"] = millis();
    endTimeCheckList[motorIndexString]["endTime"] = millis() + (long)(runTime*1000);
  }
  //STEP 改變74HC595的設定
  Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
  //STEP 馬達通電
  digitalWrite(PIN__EN_Peristaltic_Motor, HIGH);
  //STEP 開始loop所有馬達狀態，來決定是否繼續執行、停下、觸發錯誤...等等等
  bool allFinish = false;
  while (allFinish == false){
    if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
      ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"蠕動馬達流程步驟收到中斷要求");
      digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
      result.code = ResultCode::STOP_BY_OUTSIDE;
      // result.code = -1;
      return result;
    }
    allFinish = true;
    for (const auto& endTimeCheck : endTimeCheckList.as<JsonObject>()) {
      // if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
      //   ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"蠕動馬達流程步驟收到中斷要求");
      //   digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
      //   result.code = ResultCode::STOP_BY_OUTSIDE;
      //   return result;
      // }
      JsonObject endTimeCheckJSON = endTimeCheck.value().as<JsonObject>();
      if (endTimeCheckJSON["finish"] == true) {
        //? 跳過已經確定運行完成的馬達
        vTaskDelay(1/portTICK_PERIOD_MS);
        continue;
      }
      allFinish = false;
      long endTime = endTimeCheckJSON["endTime"].as<long>();
      int until = endTimeCheckJSON["until"].as<int>();
      //? 若馬達執行時間達到最大時間的判斷
      if (millis() >= endTime) {
        //? 執行到這，代表馬達執行到最大執行時間，如果 failType 是 timeout，則代表觸發失敗判斷
        String thisFailType = endTimeCheckJSON["failType"].as<String>();
        String thisFailAction = endTimeCheckJSON["failAction"].as<String>();
        int motorIndex = endTimeCheckJSON["index"].as<int>();
        if (thisFailType == "timeout") { //! 馬達運轉超時判斷
          result.message="馬達:"+String(motorIndex)+" 運轉超時\n";
          char buffer[1024];
          sprintf(buffer, "[%s][%s]蠕動馬達(%d)觸發Timeout，檢查錯誤處裡: %s", 
            StepTaskDetailItem->PipelineName.c_str(), 
            (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepTaskDetailItem->StepName]["title"].as<String>().c_str(),
            motorIndex, thisFailAction.c_str()
          );
          ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "%s", buffer);
          Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "%s", buffer);
          Device_Ctrl.BroadcastLogToClient(NULL, 1, "%s", buffer);
          if (thisFailAction=="stepStop") {
            //! 觸發timeout，並且停止當前Step的運行
            for (const auto& motorChose : endTimeCheckList.as<JsonObject>()) {
              //? 強制停止當前step執行的馬達
              if (Device_Ctrl.peristalticMotorsCtrl.GetMotorStatusSetting(motorIndex) == PeristalticMotorStatus::STOP) {

              } else {
                Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
                Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
                int usedTime = millis() - endTimeCheckList[String(motorIndex)]["startTime"].as<int>();
                String itemName = "motor_"+String(motorIndex);
                Device_Ctrl.ItemUsedAdd(itemName, usedTime);
              }
            }
            endTimeCheckJSON["finish"].set(true);
            // (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["RESULT"].set("FAIL");
            ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "停止當前Step的運行");
            Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "停止當前Step的運行");
            Device_Ctrl.BroadcastLogToClient(NULL, 1, "停止當前Step的運行");
            result.code = ResultCode::STOP_THIS_STEP;
            return result;
          }
          else if (thisFailAction=="stopImmediately") {
            ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備緊急終止儀器");
            Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "準備緊急終止儀器");
            Device_Ctrl.BroadcastLogToClient(NULL, 1, "準備緊急終止儀器");
            result.code = ResultCode::STOP_DEVICE;
            return result;
          }
          else if (thisFailAction=="stopPipeline") {
            ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備停止當前流程，若有下一個排隊中的流程就執行他");
            Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "準備停止當前流程，若有下一個排隊中的流程就執行他");
            Device_Ctrl.BroadcastLogToClient(NULL, 1, "準備停止當前流程，若有下一個排隊中的流程就執行他");
            // (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["RESULT"].set("STOP_THIS_PIPELINE");
            result.code = ResultCode::STOP_THIS_PIPELINE;
            return result;
          }
          //? 若非，則正常停止馬達運行
          if (Device_Ctrl.peristalticMotorsCtrl.GetMotorStatusSetting(motorIndex) == PeristalticMotorStatus::STOP) {

          } else {
            Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
            Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
            int usedTime = millis() - endTimeCheckList[String(motorIndex)]["startTime"].as<int>();
            String itemName = "motor_"+String(motorIndex);
            Device_Ctrl.ItemUsedAdd(itemName, usedTime);
          }
          ESP_LOGV(StepTaskDetailItem->TaskName.c_str(), "蠕動馬達(%d)執行至最大時間，停止其動作", motorIndex);
          endTimeCheckJSON["finish"].set(true);
        }
        else {
          //? 若非，則正常停止馬達運行
          if (Device_Ctrl.peristalticMotorsCtrl.GetMotorStatusSetting(motorIndex) == PeristalticMotorStatus::STOP) {

          } else {
            Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
            Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
            int usedTime = millis() - endTimeCheckList[String(motorIndex)]["startTime"].as<int>();
            String itemName = "motor_"+String(motorIndex);
            Device_Ctrl.ItemUsedAdd(itemName, usedTime);
            //! 若設定檔有消耗紀錄設定，則計算預估消耗量，並紀錄之
            String consumeTarget = endTimeCheckJSON["consumeTarget"].as<String>();
            Serial.println(consumeTarget);
            if (consumeTarget or consumeTarget != "null") {
              int consumeNum = endTimeCheckJSON["consumeNum"].as<int>();
              int consumeRemaining = (*Device_Ctrl.CONFIG__maintenance_item.json_data)[consumeTarget]["remaining"].as<int>();
              (*Device_Ctrl.CONFIG__maintenance_item.json_data)[consumeTarget]["remaining"].set(consumeRemaining - consumeNum);
              Device_Ctrl.CONFIG__maintenance_item.writeConfig();
            }
          }
          ESP_LOGV(StepTaskDetailItem->TaskName.c_str(), "蠕動馬達(%d)執行至最大時間，停止其動作", motorIndex);
          endTimeCheckJSON["finish"].set(true);
        }
      }
      //? 若要判斷滿水浮球狀態
      else if (until != -1) {
        String thisFailType = endTimeCheckJSON["failType"].as<String>();
        String thisFailAction = endTimeCheckJSON["failAction"].as<String>();
        int motorIndex = endTimeCheckJSON["index"].as<int>();
        pinMode(until, INPUT);
        int value = digitalRead(until);
        if (value == HIGH) {
          if (Device_Ctrl.peristalticMotorsCtrl.GetMotorStatusSetting(motorIndex) == PeristalticMotorStatus::STOP) {

          } else {
            Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
            Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
            int usedTime = millis() - endTimeCheckList[String(motorIndex)]["startTime"].as<int>();
            String itemName = "motor_"+String(motorIndex);
            Device_Ctrl.ItemUsedAdd(itemName, usedTime);
            Serial.println(endTimeCheckJSON["pool"].as<String>());
            if (endTimeCheckJSON["pool"].as<String>() != "null") {
              ESP_LOGV("", "記錄初次抽池水得耗時: %s, %d 秒", endTimeCheckJSON["pool"].as<String>().c_str(), usedTime/1000);
              Device_Ctrl.SaveSensorDataToBinFile(now(), endTimeCheckJSON["pool"], "pumping_time", usedTime/1000);
            }

            //! 若設定檔有消耗紀錄設定，則計算預估消耗量，並紀錄之
            String consumeTarget = endTimeCheckJSON["consumeTarget"].as<String>();
            if (consumeTarget) {
              int consumeNum = endTimeCheckJSON["consumeNum"].as<int>();
              int consumeRemaining = (*Device_Ctrl.CONFIG__maintenance_item.json_data)[consumeTarget]["remaining"].as<int>();
              (*Device_Ctrl.CONFIG__maintenance_item.json_data)[consumeTarget]["remaining"].set(consumeRemaining - consumeNum);
              Device_Ctrl.CONFIG__maintenance_item.writeConfig();
            }
          }
          ESP_LOGV(StepTaskDetailItem->TaskName.c_str(), "浮球觸發，關閉蠕動馬達(%d)", motorIndex);


          //? 判斷是否有觸發錯誤
          if (thisFailType == "connect") {
            ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "蠕動馬達(%d)觸發connect錯誤", motorIndex);
            if (thisFailAction=="stepStop") {
              for (const auto& motorChose : endTimeCheckList.as<JsonObject>()) {
                //? 強制停止當前step執行的馬達
                if (Device_Ctrl.peristalticMotorsCtrl.GetMotorStatusSetting(motorIndex) == PeristalticMotorStatus::STOP) {

                } else {
                  Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
                  Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
                  int usedTime = millis() - endTimeCheckList[String(motorIndex)]["startTime"].as<int>();
                  String itemName = "motor_"+String(motorIndex);
                  Device_Ctrl.ItemUsedAdd(itemName, usedTime);
                }
              }
              endTimeCheckJSON["finish"].set(true);
              ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "停止當前Step的運行");
              result.code = ResultCode::STOP_THIS_STEP;
              return result;
            }
            else if (thisFailAction=="stopImmediately") {
              ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備緊急終止儀器");
              result.code = ResultCode::STOP_DEVICE;
              return result;
            }
          }
          endTimeCheckJSON["finish"].set(true);
        }
      }
      vTaskDelay(5/portTICK_PERIOD_MS);
    }
    vTaskDelay(5/portTICK_PERIOD_MS);
  }
  if (Device_Ctrl.peristalticMotorsCtrl.IsAllStop()) {
    digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
  }
  result.code = ResultCode::SUCCESS;
  return result;
}


portMUX_TYPE stepMotor_mux0 = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t * stepMotor_timer0;
int stepMotor_countGo = 0;


int Do_WaitAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  int waitSeconds = eventItem["wait"].as<int>();
  char buffer[1024];
  sprintf(buffer, "[%s][%s]等待 %d 秒", 
    StepTaskDetailItem->PipelineName.c_str(), 
    (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepTaskDetailItem->StepName]["title"].as<String>().c_str(),
    waitSeconds
  );


  ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"%s",buffer);
  // Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3,"%s",buffer);
  Device_Ctrl.BroadcastLogToClient(NULL, 3,"%s",buffer);
  unsigned long start_time = millis();
  unsigned long end_time = start_time + waitSeconds*1000;
  while (millis() < end_time) {
    if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
      ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"等待時間步驟收到緊急中斷要求，準備緊急終止儀器");
      return -1;
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
  return 1;
}


StepResult Do_SpectrophotometerAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  StepResult result;
  for (JsonObject spectrophotometerItem : eventItem["spectrophotometer_list"].as<JsonArray>()) {
    //? spectrophotometerIndex: 待動作的光度計編號
    //? spectrophotometerIndex 為 0 時，代表藍光模組
    //? spectrophotometerIndex 為 1 時，代表綠光模組
    int spectrophotometerIndex = spectrophotometerItem["index"].as<int>();
  
    JsonObject spectrophotometerConfigChose = (*Device_Ctrl.CONFIG__spectrophoto_meter.json_data)[spectrophotometerIndex].as<JsonObject>();
    //? spectrophotometerTitle: 光度計Title
    String spectrophotometerTitle = spectrophotometerConfigChose["title"].as<String>();
    //? value_name: 預計獲得的數值類型名稱
    String value_name = spectrophotometerItem["value_name"].as<String>();
    //? poolChose: 獲得的數值預計歸類為哪一池
    String poolChose = spectrophotometerItem["pool"].as<String>();
    //? type: 光度計動作類型，當前有兩種
    //? 1. Adjustment:  純量測清洗數值
    //? 2. Measurement: 不調整電阻並量測出PPM數值
    String type = spectrophotometerItem["type"].as<String>();
    String failAction = spectrophotometerItem["failAction"].as<String>();

    int activePin;
    int sensorAddr;
    if (spectrophotometerIndex == 0) {
      activePin = PIN__EN_BlueSensor;      
      sensorAddr = 0x44;
    } else {
      activePin = PIN__EN_GreenSensor;
      sensorAddr = 0x45;
      // sensorAddr = 0x4F;
    }
    digitalWrite(activePin, HIGH);
    vTaskDelay(2000/portTICK_PERIOD_MS);
    Device_Ctrl.ScanI2C();
    Device_Ctrl._Wire.beginTransmission(sensorAddr);
    byte error = Device_Ctrl._Wire.endTransmission();

    if (error != 0) {
      digitalWrite(activePin, LOW);
      char buffer[1024];
      sprintf(buffer, "[%s][%s] 找不到光度計: %s", 
        StepTaskDetailItem->PipelineName.c_str(), 
        (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepTaskDetailItem->StepName]["title"].as<String>().c_str(),
        spectrophotometerTitle.c_str()
      );
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "%s", buffer);
      Device_Ctrl.BroadcastLogToClient(NULL, 1, "%s", buffer);
      result.code = ResultCode::KEEP_RUN;
      result.message = "找不到光度計: "+spectrophotometerTitle + "\n";
      return result;
    }


    INA226 ina226(Device_Ctrl._Wire);
    ina226.begin(sensorAddr);
    Serial.println(sensorAddr);
    ina226.configure(
      INA226_AVERAGES_4, // 采样平均
      INA226_BUS_CONV_TIME_140US, //采样时间
      INA226_SHUNT_CONV_TIME_140US, //采样时间
      INA226_MODE_SHUNT_BUS_CONT // Shunt和Bus电压连续测量
    );
    ina226.calibrate(0.01, 4);
    int countNum = 100;
    uint16_t dataBuffer[countNum];
    // double busvoltageBuffer_ina226 = 0;
    for (int i=0;i<countNum;i++) {
      // busvoltageBuffer_ina226 += (double)ina226.readBusVoltage();
      dataBuffer[i] = ina226.readBusVoltage()*1000;
      vTaskDelay(100/portTICK_PERIOD_MS);
    }
    ESP_LOGD("", "STD: %.2f, AVG: %.2f, ANS: %.2f", 
      standardDev(dataBuffer, countNum),
      average(dataBuffer, countNum),
      afterFilterValue(dataBuffer, countNum)
    );

    // Serial.printf("STD: %.2f, AVG: %.2f, ANS: %.2f", 
    //   standardDev(dataBuffer, countNum),
    //   average(dataBuffer, countNum),
    //   afterFilterValue(dataBuffer, countNum)
    // );
    double busvoltage_ina226_ = afterFilterValue(dataBuffer, countNum);
    // double busvoltage_ina226 = busvoltageBuffer_ina226/countNum;
    // Serial.println(busvoltage_ina226_);
    // Serial.println(busvoltage_ina226);
    //?  最終量測的結果數值 (mV)
    // double finalValue = busvoltage_ina226*1000;
    double finalValue = busvoltage_ina226_;
    digitalWrite(activePin, LOW);

    Device_Ctrl.ItemUsedAdd("Light"+String(spectrophotometerIndex), 1);

    (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][value_name]["Value"].set(String(finalValue,2).toDouble());
    (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][value_name]["data_time"].set(GetDatetimeString());

    char buffer[1024];
    sprintf(buffer, "[%s][%s] %s 第 %s 池 光度計量測 %s 結果: %s", 
      StepTaskDetailItem->PipelineName.c_str(), 
      (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepTaskDetailItem->StepName]["title"].as<String>().c_str(),
      (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][value_name]["data_time"].as<String>().c_str(),
      poolChose.c_str(),
      value_name.c_str(),
      String(finalValue,2)
    );
    Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 5, "%s", buffer);
    Device_Ctrl.BroadcastLogToClient(NULL, 5, "%s", buffer);


    Device_Ctrl.InsertNewDataToDB(GetDatetimeString(), poolChose, value_name, finalValue);

    Device_Ctrl.SaveSensorDataToBinFile(now(), poolChose, value_name, (int)finalValue);

    ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__LastSensorDataSave, *Device_Ctrl.JSON__sensorDataSave);
    if (value_name.lastIndexOf("wash_volt")!=-1) {
      //! 本次測量的是清洗電壓
      if (spectrophotometerIndex == 0) {
        Device_Ctrl.lastLightValue_NH4 = finalValue;
      } else if (spectrophotometerIndex == 1) {
        Device_Ctrl.lastLightValue_NO3 = finalValue;
      }
      if (finalValue < 16000.) {
        result.code = ResultCode::KEEP_RUN;
        result.message = "光度計:"+spectrophotometerTitle+" 測量初始光強度時數值過低: "+String(finalValue);
        return result;
      }
    }
    else if (value_name.lastIndexOf("test_volt")!=-1) {
      //! 本次測量的是量測電壓
      //! 需要套用校正參數，獲得真實濃度
      double mValue = spectrophotometerConfigChose["m"].as<double>();
      double bValue = spectrophotometerConfigChose["b"].as<double>();
      String TargetType = value_name.substring(0,3); 
      double A0_Value = 0;
      if (TargetType == "NO2") {
        A0_Value = Device_Ctrl.lastLightValue_NO3;
      } else if (TargetType == "NH4") {
        A0_Value = Device_Ctrl.lastLightValue_NH4;
      }
      double finalValue_after = -log10(finalValue/A0_Value)*mValue+bValue;
      double finalValue_Original = finalValue_after;
      if (poolChose == "RO") {
        (*Device_Ctrl.CONFIG__RO_correction.json_data)["data"][TargetType].set(finalValue_after);
        Device_Ctrl.CONFIG__RO_correction.writeConfig();
      }
      else if ((*Device_Ctrl.CONFIG__RO_correction.json_data)["switch"].as<bool>() == true) {
        finalValue_after -= (*Device_Ctrl.CONFIG__RO_correction.json_data)["data"][TargetType].as<double>();
      }
      if (finalValue_after < 0) { finalValue_after = 0; }
      (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][TargetType]["Value"].set(String(finalValue_after,2).toDouble());
      (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][TargetType]["data_time"].set(GetDatetimeString());
      char buffer[1024];
      sprintf(buffer, "[%s][%s] %s 第 %s 池 光度計量測 %s 結果: %s", 
        StepTaskDetailItem->PipelineName.c_str(), 
        (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepTaskDetailItem->StepName]["title"].as<String>().c_str(),
        (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][TargetType]["data_time"].as<String>().c_str(),
        poolChose.c_str(),
        TargetType.c_str(),
        String(finalValue_after,2)
      );
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 5, "%s", buffer);
      Device_Ctrl.BroadcastLogToClient(NULL, 5, "%s", buffer);
      
      Device_Ctrl.InsertNewDataToDB(GetDatetimeString(), poolChose, TargetType, finalValue_after);
      Device_Ctrl.SaveSensorDataToBinFile(now(), poolChose, TargetType, (int)(finalValue_after*100));
      if (poolChose != "RO") {
        String TargetTypeName = TargetType+String("_Original");
        Device_Ctrl.InsertNewDataToDB(GetDatetimeString(), poolChose, TargetTypeName, finalValue_Original);
        Device_Ctrl.SaveSensorDataToBinFile(now(), poolChose, TargetTypeName, (int)(finalValue_Original*100));
      }
      ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__LastSensorDataSave, *Device_Ctrl.JSON__sensorDataSave);
    }
  }
  result.code = ResultCode::SUCCESS;
  return result;
}

StepResult Do_PHmeterAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  StepResult result;
  ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"PH計控制");
  for (JsonObject PHmeterItem : eventItem["ph_meter"].as<JsonArray>()) {
    pinMode(PIN__ADC_PH_IN, INPUT);
    Device_Ctrl.ItemUsedAdd("PH-0", 1);
    // Device_Ctrl.ItemUsedAdd("PH-0", 1);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    String poolChose = PHmeterItem["pool"].as<String>();
    uint16_t phValue[30];
    for (int i=0;i<30;i++) {
      phValue[i] = analogRead(15);
      vTaskDelay(50/portTICK_PERIOD_MS);
    }
    //* 原始電壓數值獲得
    double PH_RowValue = afterFilterValue(phValue, 30);

    if (PH_RowValue < 0. | PH_RowValue > 4000) {
      result.code = ResultCode::KEEP_RUN;
      result.message = "pH模組測量異常，測量原始數據: "+String(PH_RowValue);
      return result;  
    }

    double m = (*Device_Ctrl.CONFIG__ph_meter.json_data)[0]["m"].as<String>().toDouble();
    double b = (*Device_Ctrl.CONFIG__ph_meter.json_data)[0]["b"].as<String>().toDouble();

    double pHValue = m*PH_RowValue + b;
    if (pHValue<0) {
      pHValue = 0.;
    } else if (pHValue > 14) {
      pHValue = 14.;
    }
    //? websocket相關的數值要限縮在小數點下第2位
    (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"]["pH_volt"]["Value"].set(String(PH_RowValue,2).toDouble());
    (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"]["pH_volt"]["data_time"].set(GetDatetimeString());
    (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"]["pH"]["Value"].set(String(pHValue,2).toDouble());
    (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"]["pH"]["data_time"].set(GetDatetimeString());

    Device_Ctrl.InsertNewDataToDB(GetDatetimeString(), poolChose, "pH_volt", PH_RowValue);
    Device_Ctrl.SaveSensorDataToBinFile(now(), poolChose, "pH_volt", (int)PH_RowValue);
    Device_Ctrl.InsertNewDataToDB(GetDatetimeString(), poolChose, "pH", pHValue);
    Device_Ctrl.SaveSensorDataToBinFile(now(), poolChose, "pH", (int)(pHValue*100));
    ExFile_WriteJsonFile(SD, Device_Ctrl.FilePath__SD__LastSensorDataSave, *Device_Ctrl.JSON__sensorDataSave);
    char logBuffer[1024];
    sprintf(logBuffer, "[%s][%s] %s 第 %s 池 PH量測結果, 測量原始值: %s, 轉換後PH值: %s", 
      StepTaskDetailItem->PipelineName.c_str(),
      (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepTaskDetailItem->StepName]["title"].as<String>().c_str(),
      (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"]["pH"]["data_time"].as<String>().c_str(),
      poolChose.c_str(),
      String(PH_RowValue,2),
      String(pHValue,2)
    );
    Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 5, logBuffer);
    Device_Ctrl.BroadcastLogToClient(NULL, 5, logBuffer);
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"       - %s", String(logBuffer).c_str());
    if (pHValue > 10 | pHValue < 4) {
      char buffer[1024];
      sprintf(buffer, "[%s][%s] PH量測數值有誤: %s", 
        StepTaskDetailItem->PipelineName.c_str(), 
        (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepTaskDetailItem->StepName]["title"].as<String>().c_str(),
        String(pHValue, 2).c_str()
      );
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "%s", buffer);
      Device_Ctrl.BroadcastLogToClient(NULL, 1, "%s", buffer);
    }
  }
  result.code = ResultCode::SUCCESS;
  return result;
}

#endif