#ifndef PipelineFlowScanTask_H
#define PipelineFlowScanTask_H

#include <esp_log.h>
#include <ArduinoJson.h>
#include "MAIN__DeviceCtrl.h"
#include "TimeLibExternalFunction.h"
#include <SD.h>

int TryToRunStep(String StepName, String PipelineName);

bool RunNewPipeline(const DynamicJsonDocument &newPipelineStackList) //? 執行
{
  if (xSemaphoreTake(Device_Ctrl.xMutex__pipelineFlowScan, 0) == pdTRUE) {
    (*Device_Ctrl.JSON__pipelineStack).clear();
    (*Device_Ctrl.JSON__pipelineStack).set(newPipelineStackList);
    return true;
  }
  else {
    ESP_LOGD("", "儀器忙碌中，跳過本次Pipeline需求");
    return false;
  }
}

void PipelineFlowScan(void* parameter) {
  ESP_LOGD("","開始執行流程管理Task");

  for (;;) {
    if ((*Device_Ctrl.JSON__pipelineStack).as<JsonArray>().size() == 0 ) {
      // 目前沒有待執行的Pipeline
      vTaskDelay(100/portTICK_PERIOD_MS);
      continue;
    }
    else {
      //! 保險防呆機制
      if (xSemaphoreTake(Device_Ctrl.xMutex__pipelineFlowScan, 0) == pdTRUE) {
        ESP_LOGE("","偵測到Pipeline執行需求，但xMutex__pipelineFlowScan卻空閒!");
        ESP_LOGE("","xMutex__pipelineFlowScan 出現預期外的使用，為求保險停止所有Step Thread的運作");
        for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
          Device_Ctrl.StepTaskDetailList[i].TaskStatus = StepTaskStatus::Close;
        }
        (*Device_Ctrl.JSON__pipelineStack).clear();
        xSemaphoreGive(Device_Ctrl.xMutex__pipelineFlowScan);
        continue;
      }
      JsonArray PipelineList = (*Device_Ctrl.JSON__pipelineStack).as<JsonArray>();
      ESP_LOGD("","一共有 %d 個流程會依序執行", PipelineList.size());
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3, "準備執行新流程需求，一共有 %d 個流程", PipelineList.size());
      Device_Ctrl.BroadcastLogToClient(NULL, 3, "準備執行新流程需求，一共有 %d 個流程", PipelineList.size());


      for (int pipelineIndex = 0;pipelineIndex<PipelineList.size();pipelineIndex++) {
        ESP_LOGI("", "開始執行第 %d 個流程", pipelineIndex+1);
        JsonObject pipelineChose = PipelineList[pipelineIndex].as<JsonObject>();
        String pipelineConfigFileFullPath = pipelineChose["FullFilePath"].as<String>();
        //STEP 1 檢查檔案是否存在
        ESP_LOGI("", "STEP 1 檢查檔案是否存在: %s", pipelineConfigFileFullPath.c_str());
        if (!SD.exists(pipelineConfigFileFullPath)) {
          ESP_LOGE("", "檔案: %s 不存在,跳至下一流程", pipelineConfigFileFullPath.c_str());
          String FailMessage = "執行流程時發現未知的檔案名稱: "+pipelineConfigFileFullPath+"\n";
          FailMessage += "請檢查相關設定檔案是否正確";
          Device_Ctrl.SendLineNotifyMessage(FailMessage);
          Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
          // Device_Ctrl.SetLog(1, "檔案不存在，跳至下一流程", pipelineConfigFileFullPath, Device_Ctrl.BackendServer.ws_);
          continue;
        }
        //STEP 2 檢查檔案是否可以被讀取
        ESP_LOGI("", "STEP 2 檢查檔案是否可以被讀取");
        File pipelineConfigFile = SD.open(pipelineConfigFileFullPath);
        if (!pipelineConfigFile) {
          ESP_LOGE("", "無法打開檔案: %s ,跳至下一流程", pipelineConfigFileFullPath.c_str());
          String FailMessage = "執行流程時發現無法打開的檔案: "+pipelineConfigFileFullPath+"\n";
          FailMessage += "請檢查相關設定檔案是否正確";
          Device_Ctrl.SendLineNotifyMessage(FailMessage);
          Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
          // Device_Ctrl.SetLog(1, "無法打開檔案，跳至下一流程", pipelineConfigFileFullPath, Device_Ctrl.BackendServer.ws_);
          continue;
        }
        //STEP 3 檢查檔案是否可以被解析成JSON格式
        ESP_LOGI("", "STEP 3 檢查檔案是否可以被解析成JSON格式");
        (*Device_Ctrl.JSON__pipelineConfig).clear();
        DeserializationError error = deserializeJson(*Device_Ctrl.JSON__pipelineConfig, pipelineConfigFile,  DeserializationOption::NestingLimit(20));
        pipelineConfigFile.close();
        if (error) {
          ESP_LOGE("", "JOSN解析失敗: %s ,跳至下一流程", error.c_str());
          String FailMessage = "執行流程時發現JOSN解析失敗的檔案: "+pipelineConfigFileFullPath+"\n";
          FailMessage += "請檢查相關設定檔案是否正確";
          Device_Ctrl.SendLineNotifyMessage(FailMessage);
          Device_Ctrl.SendGmailNotifyMessage("機台錯誤訊息",FailMessage);
          // Device_Ctrl.SetLog(1, "JOSN解析失敗, 跳至下一流程", String(error.c_str()), Device_Ctrl.BackendServer.ws_);
          continue;
        }
        //STEP 4 整理設定檔內容，以利後續使用
        ESP_LOGI("", "STEP 4 整理設定檔內容，以利後續使用");
        /**
        //* 範例
        //* [["NO3WashValueTest",["NO3ValueTest","NO3TestWarning"]],["NO3ValueTest",["NH4WashValueTest"]],["NH4WashValueTest",["NH4Test","NH4TestWarning"]]]
        //* 轉換為
        //* {
        //*   "NO3WashValueTest": {
        //*    "Name": "NO3WashValueTest",
        //*     "childList": {"NO3ValueTest": {},"NO3TestWarning": {}},
        //*     "parentList": {}
        //*   },
        //*   "NO3ValueTest": {
        //*     "Name": "NO3ValueTest",
        //*     "childList": {"NH4WashValueTest": {}},
        //*     "parentList": {"NO3WashValueTest": {}}
        //*   },
        //*   "NO3TestWarning": {
        //*     "Name": "NO3TestWarning",
        //*     "childList": {},
        //*     "parentList": {"NO3WashValueTest": {}}
        //*   },
        //*   "NH4WashValueTest": {
        //*     "Name": "NH4WashValueTest",
        //*     "childList": {"NH4Test": {},"NH4TestWarning": {}},
        //*     "parentList": {"NO3ValueTest": {}}
        //*   },
        //*   "NH4Test": {
        //*     "Name": "NH4Test","childList": {},
        //*     "parentList": {"NH4WashValueTest": {}}
        //*   },
        //*   "NH4TestWarning": {
        //*     "Name": "NH4TestWarning",
        //*     "childList": {},
        //*     "parentList": {"NH4WashValueTest": {}}
        //*   }
        //* }
        */
        String onlyStepGroup = pipelineChose["stepChose"].as<String>();
        DynamicJsonDocument piplineSave(65525);
        //? 若不指定Step
        if (onlyStepGroup == "") {
          ESP_LOGD("", "無指定Step，將會執行完整的流程內容");
          JsonArray piplineArray = (*Device_Ctrl.JSON__pipelineConfig)["pipline"].as<JsonArray>();
          for (JsonArray singlePiplineArray : piplineArray) {
            String ThisNodeNameString = singlePiplineArray[0].as<String>();
            if (!piplineSave.containsKey(ThisNodeNameString)) {
              piplineSave[ThisNodeNameString]["Name"] = ThisNodeNameString;
              piplineSave[ThisNodeNameString].createNestedObject("childList");
              piplineSave[ThisNodeNameString].createNestedObject("parentList");
            }
            for (String piplineChildName :  singlePiplineArray[1].as<JsonArray>()) {
              if (!piplineSave.containsKey(piplineChildName)) {
                piplineSave[piplineChildName]["Name"] = piplineChildName;
                piplineSave[piplineChildName].createNestedObject("childList");
                piplineSave[piplineChildName].createNestedObject("parentList");
              }
              if (!piplineSave[ThisNodeNameString]["childList"].containsKey(piplineChildName)){
                piplineSave[ThisNodeNameString]["childList"].createNestedObject(piplineChildName);
              }

              if (!piplineSave[piplineChildName]["parentList"].containsKey(ThisNodeNameString)){
                piplineSave[piplineChildName]["parentList"].createNestedObject(ThisNodeNameString);
              }
            }
          }
          (*Device_Ctrl.JSON__pipelineConfig)["pipline"].set(piplineSave);
          //? 將 steps_group 內的資料多加上key值: RESULT 來讓後續Task可以判斷流程是否正確執行
          //? 如果沒有"trigger"這個key值，則預設task觸發條件為"allDone"
          JsonObject stepsGroup = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"].as<JsonObject>();
          for (JsonPair stepsGroupItem : stepsGroup) {
            String stepsGroupName = String(stepsGroupItem.key().c_str());
            //? 如果有"same"這個key值，則 steps 要繼承其他設定內容
            if ((*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName].containsKey("same")) {
              (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["steps"].set(
                (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][
                  (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["same"].as<String>()
                ]["steps"].as<JsonArray>()
              );
            }
            (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["RESULT"].set("WAIT");
            if (!(*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName].containsKey("trigger")) {
              (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["trigger"].set("allDone");
            }
          };

          if (CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL==5) {
            String piplineSaveString = "";
            serializeJson((*Device_Ctrl.JSON__pipelineConfig)["pipline"], piplineSaveString);
            ESP_LOGV("", "流程設定檔: %s", piplineSaveString.c_str());
          }
        }
        else {
          ESP_LOGI("", "指定執行Step: %s", onlyStepGroup.c_str());
          JsonObject piplineArray = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"].as<JsonObject>();
          if (piplineArray.containsKey(onlyStepGroup)) {
            piplineSave[onlyStepGroup]["Name"] = onlyStepGroup;
            piplineSave[onlyStepGroup].createNestedObject("childList");
            piplineSave[onlyStepGroup].createNestedObject("parentList");
            (*Device_Ctrl.JSON__pipelineConfig)["pipline"].set(piplineSave);
            (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][onlyStepGroup]["RESULT"].set("WAIT");
            if ((*Device_Ctrl.JSON__pipelineConfig)["steps_group"][onlyStepGroup].containsKey("same")) {
              (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][onlyStepGroup]["steps"].set(
                (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][
                  (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][onlyStepGroup]["same"].as<String>()
                ]["steps"].as<JsonArray>()
              );
            }
            String onlyEvent = pipelineChose["eventChose"].as<String>();
            if (onlyEvent != "") {
              //? 若有指定Event執行
              //? 防呆: 檢查此event是否存在
              if (!(*Device_Ctrl.JSON__pipelineConfig)["events"].containsKey(onlyEvent)) {
                //! 指定的event不存在
                ESP_LOGE("LOAD__ACTION_V2", "設定檔 %s 中找不到Event: %s ，終止流程執行", pipelineConfigFileFullPath.c_str(), onlyEvent.c_str());
                // Device_Ctrl.SetLog(1, "找不到事件: " + onlyEvent + "，終止流程執行", "設定檔名稱: "+pipelineConfigFileFullPath, Device_Ctrl.BackendServer.ws_);
                continue;
              }
              (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][onlyStepGroup]["steps"].clear();
              (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][onlyStepGroup]["steps"].add(onlyEvent);
              int onlyIndex = pipelineChose["eventIndexChose"].as<int>();
              if (onlyIndex != -1) {
                //? 若有指定事件中的步驟執行
                //? 防呆: 檢查此步驟是否存在
                if (onlyIndex >= (*Device_Ctrl.JSON__pipelineConfig)["events"][onlyEvent]["event"].size()) {
                  //! 指定的index不存在
                  ESP_LOGE("LOAD__ACTION_V2", "設定檔 %s 中的Event: %s 並無步驟: %d，終止流程執行", pipelineConfigFileFullPath.c_str(), onlyEvent.c_str(), onlyIndex);
                  // Device_Ctrl.SetLog(1, "事件: " + onlyEvent + "找不到步驟: " + String(onlyIndex) + "，終止流程執行", "設定檔名稱: "+pipelineConfigFileFullPath, Device_Ctrl.BackendServer.ws_);
                  continue;
                }
                DynamicJsonDocument newEventIndexArray(200);
                JsonArray array = newEventIndexArray.to<JsonArray>();
                array.add(
                  (*Device_Ctrl.JSON__pipelineConfig)["events"][onlyEvent]["event"][onlyIndex]
                );
                (*Device_Ctrl.JSON__pipelineConfig)["events"][onlyEvent]["event"].set(array);
              }
            }
          }
        }
        
        //STEP 5 開始執行流程判斷功能
        ESP_LOGI("", "STEP 5 開始執行流程判斷功能");
        // Device_Ctrl.TaskUUID = String(uuid.toCharArray());
        JsonObject stepsGroup = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"].as<JsonObject>();
        JsonObject pipelineSet = (*Device_Ctrl.JSON__pipelineConfig)["pipline"].as<JsonObject>();
        //? isAllDone: 用來判斷是否所有流程都運行完畢，如果都完畢，則此TASK也可以關閉
        //? 判斷完畢的邏輯: 全部step都不為 "WAIT"、"RUNNING" 則代表完畢
        bool isAllDone = false;
        String PipelineName = (*Device_Ctrl.JSON__pipelineConfig)["title"].as<String>();
        Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3, "準備執行第 %d 個Pipeline: %s", pipelineIndex+1, PipelineName.c_str());
        Device_Ctrl.BroadcastLogToClient(NULL, 3, "準備執行第 %d 個Pipeline: %s", pipelineIndex+1, PipelineName.c_str());

        while(isAllDone == false) {
          if (Device_Ctrl.StopNowPipeline) {
            Device_Ctrl.StopNowPipeline = false;
            break;
          }
          isAllDone = true;
          for (JsonPair stepsGroupItem : pipelineSet) {
            //? stepsGroupName: 流程ID
            String stepsGroupName = String(stepsGroupItem.key().c_str());
            //? stepsGroupTitle: 流程名稱
            String stepsGroupTitle = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["title"].as<String>();
            //? stepsGroupResult: 此流程的運行狀態
            String stepsGroupResult = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["RESULT"].as<String>();
            //? 只有在"WAIT"時，才有必要判斷是否要執行
            if (stepsGroupResult == "WAIT") {
              isAllDone = false;
              //? parentList: 此流程的parentList
              JsonObject parentList = (*Device_Ctrl.JSON__pipelineConfig)["pipline"][stepsGroupName]["parentList"].as<JsonObject>();
              //? 如果此step狀態是WAIT並且parent數量為0，則直接觸發
              if (parentList.size() == 0) {
                ESP_LOGI("", "發現初始流程: %s(%s)，準備執行", stepsGroupTitle.c_str(), stepsGroupName.c_str());
                TryToRunStep(stepsGroupName, PipelineName);
                continue;
              }

              //? 程式執行到這邊，代表都不是ENTEY POINT
              //? TrigerRule: 此流程的觸發條件
              String TrigerRule = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["trigger"].as<String>();
              //? 如果觸發條件是 allDone，則會等待parents都不為 "WAIT"、"RUNNING"、"NORUN" 時則會開始執行
              //? 而若任何一個parent為 "NORUN" 時，則本step則視為不再需要執行，立刻設定為 "NORUN"
              if (TrigerRule == "allDone") {
                bool stepRun = true;
                for (JsonPair parentItem : parentList ) {
                  String parentName = String(parentItem.key().c_str());
                  String parentResult = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][parentName]["RESULT"].as<String>();
                  if (parentResult=="WAIT" or parentResult=="RUNNING") {
                    //? 有任何一個parent還沒執行完畢，跳出
                    stepRun = false;
                    break; 
                  } else if ( parentResult=="NORUN") {
                    //? 有任何一個parent不需執行，此step也不再需執行
                    ESP_LOGW("", "Task %s 不符合 allDone 的執行條件，關閉此Task", stepsGroupName.c_str());
                    (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["RESULT"].set("NORUN");
                    stepRun = false;
                    break; 
                  }
                }
                if (stepRun) {
                  ESP_LOGI("", "流程: %s(%s) 符合觸發條件: allDone，準備執行", stepsGroupTitle.c_str(), stepsGroupName.c_str());
                  TryToRunStep(stepsGroupName, PipelineName);
                  // Device_Ctrl.AddNewPiplelineFlowTask(stepsGroupName);
                  continue;
                }
              }
              //? 如果觸發條件是 oneFail，則會等待parent任何一項的RESULT變成 "FAIL" 就執行
              //? 而若所有parent都不是 "FAIL" 並且不為 "WAIT"、"RUNNING" 時，則本Step則視為不再需要執行，立刻設定為 "NORUN"
              else if (TrigerRule == "oneFail") {
                bool setNoRun = true;
                for (JsonPair parentItem : parentList ) {
                  String parentName = String(parentItem.key().c_str());
                  String parentResult = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][parentName]["RESULT"].as<String>();
                  if (parentResult=="FAIL") {
                    ESP_LOGI("", "流程: %s(%s) 符合觸發條件: oneFail，準備執行", stepsGroupTitle.c_str(), stepsGroupName.c_str());
                    TryToRunStep(stepsGroupName, PipelineName);
                    // Device_Ctrl.AddNewPiplelineFlowTask(stepsGroupName);
                    setNoRun = false;
                    break;
                  } else if (parentResult=="WAIT" or parentResult=="RUNNING") {
                    setNoRun = false;
                    break;
                  }
                }
                if (setNoRun) {
                  ESP_LOGW("", "Task %s(%s) 不符合 oneFail 的執行條件，關閉此Task", stepsGroupTitle.c_str(), stepsGroupName.c_str());
                  (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["RESULT"].set("NORUN");
                }
              }
              //? 如果觸發條件是 allSuccess，則會等待所有parent的RESULT變成 "SUCCESS" 就執行
              //? 如果任何一個parent為 "WAIT"、"RUNNING"，則繼續等待
              //? 如果任何一個parent為 "NORUN"、、"FAIL" 時，則本Task則視為不再需要執行，立刻設定為 "NORUN"，並且刪除Task
              else if (TrigerRule == "allSuccess") {
                bool stepRun = true;
                for (JsonPair parentItem : parentList ) {
                  String parentName = String(parentItem.key().c_str());
                  String parentResult = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][parentName]["RESULT"].as<String>();
                  if (parentResult=="NORUN" or parentResult=="FAIL") {
                    ESP_LOGW("", "Task %s(%s) 不符合 allSuccess 的執行條件，關閉此Task", stepsGroupTitle.c_str(), stepsGroupName.c_str());
                    (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupName]["RESULT"].set("NORUN");
                    stepRun = false;
                    break;
                  }
                  else if (parentResult=="WAIT" or parentResult=="RUNNING") {
                    stepRun = false;
                    break;
                  }
                }
                if (stepRun) {
                  ESP_LOGI("", "流程: %s(%s) 符合觸發條件: allSuccess，準備執行", stepsGroupTitle.c_str(), stepsGroupName.c_str());
                  TryToRunStep(stepsGroupName, PipelineName);
                  // Device_Ctrl.AddNewPiplelineFlowTask(stepsGroupName);
                  continue;
                }
              }
            } 
            else if (stepsGroupResult == "RUNNING") {isAllDone = false;}
            // else if (stepsGroupResult == "STOP_THIS_PIPELINE") {
            //   ESP_LOGI("", "發現有流程觸發了 STOP_THIS_PIPELINE");
            //   ESP_LOGI("", " - 流程名稱:\t%s (%s)", stepsGroupTitle.c_str(), stepsGroupName.c_str());
            //   ESP_LOGI("", "準備停止當前正在執行的各項Task");
            //   for (const auto& pipelineTaskHandle : Device_Ctrl.pipelineTaskHandleMap) {
            //     String stepName = pipelineTaskHandle.first;
            //     ESP_LOGI("", "停止流程: %s (%s)", (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepName]["title"].as<String>().c_str(), stepName.c_str());
            //     TaskHandle_t taskChose = pipelineTaskHandle.second;
            //     vTaskSuspend(taskChose);
            //     vTaskDelete(taskChose);
            //     Device_Ctrl.pipelineTaskHandleMap.erase(stepName);
            //   }
            //   Device_Ctrl.peristalticMotorsCtrl.SetAllMotorStop();
            //   Device_Ctrl.MULTI_LTR_329ALS_01_Ctrler.closeAllSensor();
            //   isAllDone = true;
            //   break;
            // }
          };
          vTaskDelay(100/portTICK_PERIOD_MS);
        }
        if (Device_Ctrl.StopAllPipeline) {
          Device_Ctrl.StopAllPipeline = false;
          break;
        }
        ESP_LOGI("", "第 %d 個Pipeline執行完畢", pipelineIndex+1);
        Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3, "第 %d 個Pipeline: %s 執行完畢", pipelineIndex+1, PipelineName.c_str());
        Device_Ctrl.BroadcastLogToClient(NULL, 3, "第 %d 個Pipeline: %s 執行完畢", pipelineIndex+1, PipelineName.c_str());
      }
      ESP_LOGD("","所有流程執行完畢，清空流程列隊");
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3, "所有流程執行完畢，清空流程列隊");
      Device_Ctrl.BroadcastLogToClient(NULL, 3, "所有流程執行完畢");
      (*Device_Ctrl.JSON__pipelineStack).clear();
      xSemaphoreGive(Device_Ctrl.xMutex__pipelineFlowScan);
    }
  }



}

int_fast16_t TryToRunStep(String StepName, String PipelineName)
{
  for (int i=0;i<MAX_STEP_TASK_NUM;i++) {
    if (Device_Ctrl.StepTaskDetailList[i].TaskStatus == StepTaskStatus::Idel) {
      Device_Ctrl.StepTaskDetailList[i].TaskStatus = StepTaskStatus::Busy;
      Device_Ctrl.StepTaskDetailList[i].StepName = StepName;
      Device_Ctrl.StepTaskDetailList[i].PipelineName = PipelineName;
      (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepName]["RESULT"] = "RUNNING";
      return 1;
    }
  }
  ESP_LOGD("","沒有空閒的Thread來執行Step Task: %s", StepName.c_str());
  return 0;
}

#endif