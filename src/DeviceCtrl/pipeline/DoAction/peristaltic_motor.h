#ifndef DoAction_PeristalticMotor_H
#define DoAction_PeristalticMotor_H

#include <ArduinoJson.h>
#include <vector>
#include "common.h"

struct PeristalticMotorAction : DoAction 
{
  explicit PeristalticMotorAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem) :  DoAction(eventItem, StepTaskDetailItem){}

  void Run() {
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
        result_code = ResultCode::STOP_BY_OUTSIDE;
      }
      allFinish = true;
      for (const auto& endTimeCheck : endTimeCheckList.as<JsonObject>()) {
        // if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
        //   ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"蠕動馬達流程步驟收到中斷要求");
        //   digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
        //   result_code = ResultCode::STOP_BY_OUTSIDE;
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
            ResultMessage="馬達:"+String(motorIndex)+" 運轉超時\n";
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
              result_code = ResultCode::STOP_THIS_STEP;
            }
            else if (thisFailAction=="stopImmediately") {
              ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備緊急終止儀器");
              Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "準備緊急終止儀器");
              Device_Ctrl.BroadcastLogToClient(NULL, 1, "準備緊急終止儀器");
              result_code = ResultCode::STOP_DEVICE;
            }
            else if (thisFailAction=="stopPipeline") {
              ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備停止當前流程，若有下一個排隊中的流程就執行他");
              Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "準備停止當前流程，若有下一個排隊中的流程就執行他");
              Device_Ctrl.BroadcastLogToClient(NULL, 1, "準備停止當前流程，若有下一個排隊中的流程就執行他");
              // (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["RESULT"].set("STOP_THIS_PIPELINE");
              result_code = ResultCode::STOP_THIS_PIPELINE;
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
              // String consumeTarget = endTimeCheckJSON["consumeTarget"].as<String>();
              // Serial.println(consumeTarget);
              // if (consumeTarget or consumeTarget != "null") {
              //   int consumeNum = endTimeCheckJSON["consumeNum"].as<int>();
              //   int consumeRemaining = (*Device_Ctrl.CONFIG__maintenance_item.json_data)[consumeTarget]["remaining"].as<int>();
              //   (*Device_Ctrl.CONFIG__maintenance_item.json_data)[consumeTarget]["remaining"].set(consumeRemaining - consumeNum);
              //   Device_Ctrl.CONFIG__maintenance_item.writeConfig();
              // }
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
              // String consumeTarget = endTimeCheckJSON["consumeTarget"].as<String>();
              // if (consumeTarget) {
              //   int consumeNum = endTimeCheckJSON["consumeNum"].as<int>();
              //   int consumeRemaining = (*Device_Ctrl.CONFIG__maintenance_item.json_data)[consumeTarget]["remaining"].as<int>();
              //   (*Device_Ctrl.CONFIG__maintenance_item.json_data)[consumeTarget]["remaining"].set(consumeRemaining - consumeNum);
              //   Device_Ctrl.CONFIG__maintenance_item.writeConfig();
              // }
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
                result_code = ResultCode::STOP_THIS_STEP;
              }
              else if (thisFailAction=="stopImmediately") {
                ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備緊急終止儀器");
                result_code = ResultCode::STOP_DEVICE;
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
    result_code = ResultCode::SUCCESS;
  };

  void StopByOutside() {
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"收到緊急中斷要求，準備停止當前Step");
    result_code = ResultCode::STOP_BY_OUTSIDE;
    ResultMessage = "";
  }

}

#endif