#ifndef DoAction_Wait_H
#define DoAction_Wait_H

#include "../../MAIN__DeviceCtrl.h"
#include <ArduinoJson.h>
#include "common.h"

/** 
 *   
 */
struct DoWaitAction : DoAction
{
  explicit DoWaitAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem) :  DoAction(eventItem, StepTaskDetailItem){};
  
  void Run() {
    int waitSeconds = eventItem["wait"].as<int>();
    char buffer[1024];
    sprintf(buffer, "[%s][%s]等待 %d 秒", 
      StepTaskDetailItem->PipelineName.c_str(), 
      (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][StepTaskDetailItem->StepName]["title"].as<String>().c_str(),
      waitSeconds
    );
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"%s",buffer);
    Device_Ctrl.BroadcastLogToClient(NULL, 3,"%s",buffer);
    unsigned long start_time = millis();
    unsigned long end_time = start_time + waitSeconds*1000;
    while (millis() < end_time) {
      if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
        this->StopByOutside();
        return;
      }
      vTaskDelay(100/portTICK_PERIOD_MS);
    }
    result_code = ResultCode::SUCCESS;
    return;
  };

  void StopByOutside() {
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"等待時間步驟收到緊急中斷要求，準備緊急終止儀器");
    result_code = ResultCode::STOP_BY_OUTSIDE;
    ResultMessage = "";
  }
};

#endif