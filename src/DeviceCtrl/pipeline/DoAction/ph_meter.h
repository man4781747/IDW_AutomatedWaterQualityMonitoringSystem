#ifndef DoAction_pHMeter_H
#define DoAction_pHMeter_H

#include "../../MAIN__DeviceCtrl.h"
#include <ArduinoJson.h>
#include <vector>
#include "common.h"
#include "CalcFunction.h"

/** 
 *   
 */
struct DoPhMeterAction : DoAction
{
  explicit DoPhMeterAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem) :  DoAction(eventItem, StepTaskDetailItem){};
  
  void Run() {
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"PH計控制");
    for (JsonObject PHmeterItem : eventItem["ph_meter"].as<JsonArray>()) {
      pinMode(PIN__ADC_PH_IN, INPUT);
      Device_Ctrl.ItemUsedAdd("PH-0", 1);
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
        result_code = ResultCode::KEEP_RUN;
        ResultMessage = "pH模組測量異常，測量原始數據: "+String(PH_RowValue);
        return;  
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
      ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"       - %s", logBuffer);
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
    result_code = ResultCode::SUCCESS;
    return;
  };

  void StopByOutside() {
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"收到緊急中斷要求，準備停止當前Step");
    result_code = ResultCode::STOP_BY_OUTSIDE;
    ResultMessage = "";
  }
};

#endif