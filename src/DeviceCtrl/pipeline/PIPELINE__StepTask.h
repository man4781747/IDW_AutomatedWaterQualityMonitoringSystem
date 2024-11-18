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
#include "DoAction/ph_meter.h"
#include "DoAction/spectrophoto_meter.h"
#include "DoAction/wait.h"
#include "DoAction/peristaltic_motor.h"

void StopStep(StepTaskDetail* StepTaskDetailItem);
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

      //? event細節執行內容
      JsonArray eventList = (*Device_Ctrl.JSON__pipelineConfig)["events"][eventChose]["event"].as<JsonArray>();
      serializeJsonPretty(eventList, Serial);
      //? 一個一個event item依序執行
      for (JsonObject eventItem : eventList) {
        if (eventItem.containsKey("pwm_motor_list")) {
          DoServoAction actionItem = DoServoAction(eventItem, StepTaskDetailItem);
          actionItem.Run();
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
          DoWaitAction action = DoWaitAction(eventItem, StepTaskDetailItem);
          action.Run();
          if (action.result_code == ResultCode::STOP_BY_OUTSIDE) {
            isStepFail = true;
            EmergencyStop = true;
            break;
          }
        }
        else if (eventItem.containsKey("peristaltic_motor_list")) {
          DoPeristalticMotorAction action = DoPeristalticMotorAction(eventItem, StepTaskDetailItem);
          action.Run();
          if (action.result_code == ResultCode::STOP_BY_OUTSIDE) {
            //! 收到停止
            isStepFail = true;
            OnlyStepStop = true;
            break;
          }
          else if (action.result_code == ResultCode::STOP_THIS_PIPELINE) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += action.ResultMessage;
            sendString += "後續處理方法: 跳過當前流程\n";
            Device_Ctrl.AddLineNotifyEvent(sendString);
            // Device_Ctrl.SendLineNotifyMessage(FailMessage);
            Device_Ctrl.AddGmailNotifyEvent("機台錯誤訊息",sendString);
            // Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
            isStepFail = true;
            OnlyPipelineStop = true;
            break;
          }
          else if (action.result_code == ResultCode::STOP_DEVICE) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += action.ResultMessage;
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
          DoSpectrophotometerAction action = DoSpectrophotometerAction(eventItem, StepTaskDetailItem);
          action.Run();
          if (action.result_code == ResultCode::KEEP_RUN) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += action.ResultMessage;
            sendString += "後續處理方法: 繼續執行\n";
            Device_Ctrl.AddLineNotifyEvent(sendString);
            // Device_Ctrl.SendLineNotifyMessage(FailMessage);
            Device_Ctrl.AddGmailNotifyEvent("機台錯誤訊息",sendString);
            // Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
          }
        }
        else if (eventItem.containsKey("ph_meter")) {
          DoPhMeterAction action = DoPhMeterAction(eventItem, StepTaskDetailItem);
          action.Run();
          if (action.result_code == ResultCode::KEEP_RUN) {
            String sendString = "\n儀器: " + (*Device_Ctrl.CONFIG__device_base_config.json_data)["device_no"].as<String>() + "("+WiFi.localIP().toString()+") 偵測到異常\n";
            sendString += "Pipeline: "+pipelineName+"\n";
            sendString += "異常步驟: "+ThisStepGroupTitle+"\n===========\n";
            sendString += action.ResultMessage;
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
  StepTaskDetailItem->StepName = "";
  StepTaskDetailItem->TaskStatus = StepTaskStatus::Idle;
}

#endif