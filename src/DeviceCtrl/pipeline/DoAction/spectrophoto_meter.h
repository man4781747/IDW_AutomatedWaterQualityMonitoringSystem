#ifndef DoAction_Spectrophotometer_H
#define DoAction_Spectrophotometer_H

#include "../../MAIN__DeviceCtrl.h"
#include <ArduinoJson.h>
#include "common.h"
#include <INA226.h>
#include "CalcFunction.h"

struct DoSpectrophotometerAction : DoAction
{
  explicit DoSpectrophotometerAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem) :  DoAction(eventItem, StepTaskDetailItem){};
  


  void Run() {
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
      }
      digitalWrite(activePin, HIGH);
      vTaskDelay(2000/portTICK_PERIOD_MS);
      // Device_Ctrl.ScanI2C();
      //? 檢查光度計感光元件是否存在
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
        result_code = ResultCode::KEEP_RUN;
        ResultMessage = "找不到光度計: "+spectrophotometerTitle + "\n";
        return;
      }
      
      INA226 ina226(Device_Ctrl._Wire);
      ina226.begin(sensorAddr);
      // Serial.println(sensorAddr);
      ina226.configure(
        INA226_AVERAGES_4, // 采样平均
        INA226_BUS_CONV_TIME_140US, //采样时间
        INA226_SHUNT_CONV_TIME_140US, //采样时间
        INA226_MODE_SHUNT_BUS_CONT // Shunt和Bus电压连续测量
      );

      ina226.calibrate(0.01, 4);
      int countNum = 100;
      uint16_t dataBuffer[countNum];
      for (int i=0;i<countNum;i++) {
        dataBuffer[i] = ina226.readBusVoltage()*1000;
        vTaskDelay(100/portTICK_PERIOD_MS);
      }
      double busvoltage_ina226_ = afterFilterValue(dataBuffer, countNum);
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
          result_code = ResultCode::KEEP_RUN;
          ResultMessage = "光度計:"+spectrophotometerTitle+" 測量初始光強度時數值過低: "+String(finalValue);
          return;
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
    return;
  };

  void StopByOutside() {
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"收到緊急中斷要求，準備停止當前Step");
    result_code = ResultCode::STOP_BY_OUTSIDE;
    ResultMessage = "";
  }
};

#endif