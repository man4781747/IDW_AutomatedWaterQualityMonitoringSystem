#ifndef StepTask_H
#define StepTask_H

#include <esp_log.h>
#include <ArduinoJson.h>
#include "MAIN__DeviceCtrl.h"
#include "lx_20s.h"
#include "PeristalticMotorCtrl.h"
#include <INA226.h>
#include "TimeLibExternalFunction.h"
#include "CalcFunction.h"

void StopStep(StepTaskDetail* StepTaskDetailItem);
int Do_ServoMotorAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);
int Do_WaitAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);
int Do_PeristalticMotorAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);
int Do_SpectrophotometerAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);
int Do_PHmeterAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem);


void StepTask(void* parameter) {
  // ESP_LOGD("","開始執行Step執行Task");
  // StepTaskDetail StepTaskDetailItem = *(StepTaskDetail*)parameter;
  StepTaskDetail* StepTaskDetailItem = (StepTaskDetail*)parameter;
  ESP_LOGD(StepTaskDetailItem->TaskName.c_str(),"開始執行Step執行Task");
  bool EmergencyStop = false;
  bool OnlyStepStop = false;
  bool OnlyPipelineStop = false;
  for (;;) {
    if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
      StopStep(StepTaskDetailItem);
      continue;
    }
    if (StepTaskDetailItem->StepName == NULL) {
      vTaskDelay(2000/portTICK_PERIOD_MS);
      continue;
    }
    String stepsGroupNameString = StepTaskDetailItem->StepName;
    //? 這個 Task 要執行的 steps_group 的 title
    String ThisStepGroupTitle = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["title"].as<String>();
    //? 這個 Task 要執行的 steps_group 的 list
    JsonArray stepsGroupArray = (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["steps"].as<JsonArray>();
    //? 這個 Task 要執行的 parent list
    JsonObject parentList = (*Device_Ctrl.JSON__pipelineConfig)["pipline"][stepsGroupNameString]["parentList"].as<JsonObject>();


    ESP_LOGD(StepTaskDetailItem->TaskName.c_str(),"收到運作要求，目標Step名稱: %s，Title: %s", stepsGroupNameString.c_str(), ThisStepGroupTitle.c_str());
    Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 0, "收到運作要求，目標Step名稱: %s，Title: %s", stepsGroupNameString.c_str(), ThisStepGroupTitle.c_str());
    Device_Ctrl.BroadcastLogToClient(NULL, 0, "收到運作要求，目標Step名稱: %s，Title: %s", stepsGroupNameString.c_str(), ThisStepGroupTitle.c_str());

    bool isStepFail = false;
    //? 一個一個event依序執行
    for (String eventChose : stepsGroupArray) {
      //? eventChose: 待執行的event名稱
      String thisEventTitle = (*Device_Ctrl.JSON__pipelineConfig)["events"][eventChose]["title"].as<String>();
      ESP_LOGI(StepTaskDetailItem->TaskName.c_str(), "執行: %s - %s", ThisStepGroupTitle.c_str(), thisEventTitle.c_str());
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 3, "執行: %s - %s", ThisStepGroupTitle.c_str(), thisEventTitle.c_str());
      Device_Ctrl.BroadcastLogToClient(NULL, 3, "執行: %s - %s", ThisStepGroupTitle.c_str(), thisEventTitle.c_str());
      //? event細節執行內容
      JsonArray eventList = (*Device_Ctrl.JSON__pipelineConfig)["events"][eventChose]["event"].as<JsonArray>();
      //? 一個一個event item依序執行
      for (JsonObject eventItem : eventList) {
        if (eventItem.containsKey("pwm_motor_list")) {
          if (Do_ServoMotorAction(eventItem, StepTaskDetailItem) != 1) {
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
          int PeristalticMotorResult = Do_PeristalticMotorAction(eventItem, StepTaskDetailItem);
          if (PeristalticMotorResult == -1) {
            //! 收到停止
            isStepFail = true;
            OnlyStepStop = true;
            break;
          }
          else if (PeristalticMotorResult == -2) {
            isStepFail = true;
            OnlyPipelineStop = true;
            break;
          }
          else if (PeristalticMotorResult == -3) {
            isStepFail = true;
            EmergencyStop = true;
            break;
          }
        }
        else if (eventItem.containsKey("spectrophotometer_list")) {
          int SpectrophotometerResult = Do_SpectrophotometerAction(eventItem, StepTaskDetailItem);
        }
        else if (eventItem.containsKey("ph_meter")) {
          int PHmeterResult = Do_PHmeterAction(eventItem, StepTaskDetailItem);
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
  StepTaskDetailItem->TaskStatus = StepTaskStatus::Idel;
}

int Do_ServoMotorAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  pinMode(PIN__EN_Servo_Motor, OUTPUT);
  String anyFail = "";
  xSemaphoreTake(Device_Ctrl.xMutex__LX_20S, portMAX_DELAY);
  digitalWrite(PIN__EN_Servo_Motor, HIGH);
  Serial2.begin(115200,SERIAL_8N1, PIN__Serial_LX_20S_RX, PIN__Serial_LX_20S_TX);
  vTaskDelay(1000/portTICK_PERIOD_MS);
  if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"收到緊急中斷要求，準備停止當前Step");
    digitalWrite(PIN__EN_Servo_Motor, LOW);
    xSemaphoreGive(Device_Ctrl.xMutex__LX_20S);
    return 0;
  }
  for (JsonObject servoMotorItem : eventItem["pwm_motor_list"].as<JsonArray>()) {
    int targetAngValue = map(servoMotorItem["status"].as<int>(), -30, 210, 0, 1000);
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"伺服馬達(LX-20S) %d 轉至 %d 度(%d)", 
      servoMotorItem["index"].as<int>(), 
      servoMotorItem["status"].as<int>(), targetAngValue
    );
    LX_20S_SerialServoMove(Serial2, servoMotorItem["index"].as<int>(),targetAngValue,500);
    vTaskDelay(50/portTICK_PERIOD_MS);
    LX_20S_SerialServoMove(Serial2, servoMotorItem["index"].as<int>(),targetAngValue,500);
    vTaskDelay(50/portTICK_PERIOD_MS);
    if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
      ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"收到緊急中斷要求，準備停止當前Step");
      digitalWrite(PIN__EN_Servo_Motor, LOW);
      xSemaphoreGive(Device_Ctrl.xMutex__LX_20S);
      return 0;
    }
  }
  vTaskDelay(2000/portTICK_PERIOD_MS);
  if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"收到緊急中斷要求，準備停止當前Step");
    digitalWrite(PIN__EN_Servo_Motor, LOW);
    xSemaphoreGive(Device_Ctrl.xMutex__LX_20S);
    return 0;
  }
  for (JsonObject servoMotorItem : eventItem["pwm_motor_list"].as<JsonArray>()) {
    int targetAngValue = map(servoMotorItem["status"].as<int>(), -30, 210, 0, 1000);
    int readAng;
    readAng = LX_20S_SerialServoReadPosition(Serial2, servoMotorItem["index"].as<int>());
    if (readAng < -100) {
      vTaskDelay(200/portTICK_PERIOD_MS);
      readAng = LX_20S_SerialServoReadPosition(Serial2, servoMotorItem["index"].as<int>());
    }
    int d_ang = targetAngValue - readAng;
    if (abs(d_ang)>15) {
      ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "伺服馬達(LX-20S) 編號 %d 並無轉到指定角度: %d，讀取到的角度為: %d",
        servoMotorItem["index"].as<int>(), servoMotorItem["status"].as<int>(),
        map(readAng, 0, 1000, -30, 210)
      );
      anyFail += String(servoMotorItem["index"].as<int>());
      anyFail += ",";
    }
    if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
      ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"收到緊急中斷要求，準備停止當前Step");
      digitalWrite(PIN__EN_Servo_Motor, LOW);
      xSemaphoreGive(Device_Ctrl.xMutex__LX_20S);
      return 0;
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
  // Serial2.end();
  digitalWrite(PIN__EN_Servo_Motor, LOW);
  xSemaphoreGive(Device_Ctrl.xMutex__LX_20S);

  if (anyFail != "") {
    ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "伺服馬達(LX-20S)發生預期外的動作，準備中止儀器的所有動作");
    // Device_Ctrl.StopDeviceAndINIT();
    return -1;
  }
  return 1;
}


/**
 * @return int: 1 - 正常完成
 * @return int: 0 - 異常失敗
 * @return int: -1 - 標記失敗，並停下此Step
 * @return int: -2 - 標記失敗，並停下此Pipeline
 * @return int: -3 - 標記失敗，並停下儀器
 */
int Do_PeristalticMotorAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  // serializeJsonPretty(eventItem, Serial);
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

    if (endTimeCheckList.containsKey(motorIndexString)) {
      continue;
    }

    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"蠕動馬達(%d) %s 持續 %.2f 秒或是直到%s被觸發,failType: %s, failAction: %s", 
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
      return -1;
    }
    allFinish = true;
    for (const auto& endTimeCheck : endTimeCheckList.as<JsonObject>()) {
      if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {
        ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"蠕動馬達流程步驟收到中斷要求");
        digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
        return -1;
      }
      JsonObject endTimeCheckJSON = endTimeCheck.value().as<JsonObject>();
      if (endTimeCheckJSON["finish"] == true) {
        //? 跳過已經確定運行完成的馬達
        vTaskDelay(10/portTICK_PERIOD_MS);
        continue;
      }
      allFinish = false;
      long endTime = endTimeCheckJSON["endTime"].as<long>();
      int until = endTimeCheckJSON["until"].as<int>();
      int motorIndex = endTimeCheckJSON["index"].as<int>();
      String thisFailType = endTimeCheckJSON["failType"].as<String>();
      String thisFailAction = endTimeCheckJSON["failAction"].as<String>();

      //? 若馬達執行時間達到最大時間的判斷
      if (millis() >= endTime) {
        //? 執行到這，代表馬達執行到最大執行時間，如果 failType 是 timeout，則代表觸發失敗判斷
        if (thisFailType == "timeout") {
          ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "蠕動馬達觸發Timeout錯誤，錯誤處裡: %s", thisFailAction.c_str());
          if (thisFailAction=="stepStop") {
            //! 觸發timeout，並且停止當前Step的運行
            for (const auto& motorChose : endTimeCheckList.as<JsonObject>()) {
              //? 強制停止當前step執行的馬達
              Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
              Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
            }
            endTimeCheckJSON["finish"].set(true);
            // (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["RESULT"].set("FAIL");
            ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "停止當前Step的運行");
            return -1;
          }
          else if (thisFailAction=="stopImmediately") {
            ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備緊急終止儀器");
            return -3;
          }
          else if (thisFailAction=="stopPipeline") {
            ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備停止當前流程，若有下一個排隊中的流程就執行他");
            // (*Device_Ctrl.JSON__pipelineConfig)["steps_group"][stepsGroupNameString]["RESULT"].set("STOP_THIS_PIPELINE");
            return -2;
          }
          //? 若非，則正常停止馬達運行
          Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
          Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
          ESP_LOGV(StepTaskDetailItem->TaskName.c_str(), "蠕動馬達(%d)執行至最大時間，停止其動作", motorIndex);
          endTimeCheckJSON["finish"].set(true);
        }
        else {
          //? 若非，則正常停止馬達運行
          Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
          Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
          ESP_LOGV(StepTaskDetailItem->TaskName.c_str(), "蠕動馬達(%d)執行至最大時間，停止其動作", motorIndex);
          endTimeCheckJSON["finish"].set(true);
        }
      }
      //? 若要判斷滿水浮球狀態
      else if (until != -1) {
        pinMode(until, INPUT);
        int value = digitalRead(until);
        if (value == HIGH) {
          Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
          Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
          ESP_LOGV(StepTaskDetailItem->TaskName.c_str(), "浮球觸發，關閉蠕動馬達(%d)", motorIndex);
          //? 判斷是否有觸發錯誤
          if (thisFailType == "connect") {
            ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "蠕動馬達(%d)觸發connect錯誤", motorIndex);
            if (thisFailAction=="stepStop") {
              for (const auto& motorChose : endTimeCheckList.as<JsonObject>()) {
                //? 強制停止當前step執行的馬達
                Device_Ctrl.peristalticMotorsCtrl.SetMotorStatus(motorIndex, PeristalticMotorStatus::STOP);
                Device_Ctrl.peristalticMotorsCtrl.RunMotor(Device_Ctrl.peristalticMotorsCtrl.moduleDataList);
              }
              endTimeCheckJSON["finish"].set(true);
              ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "停止當前Step的運行");
              return -1;
            }
            else if (thisFailAction=="stopImmediately") {
              ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "準備緊急終止儀器");
              return -3;
            }
          }
          endTimeCheckJSON["finish"].set(true);
        }
      }
      vTaskDelay(100/portTICK_PERIOD_MS);
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
  if (Device_Ctrl.peristalticMotorsCtrl.IsAllStop()) {
    digitalWrite(PIN__EN_Peristaltic_Motor, LOW);
  }
  return 1;
}


/**
 * @return int: 1 - 正常完成
 * @return int: 0 - 異常失敗
 * @return int: -1 - 緊急中斷
 */
int Do_WaitAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  int waitSeconds = eventItem["wait"].as<int>();
  ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"      等待 %d 秒",waitSeconds);
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


int Do_SpectrophotometerAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  for (JsonObject spectrophotometerItem : eventItem["spectrophotometer_list"].as<JsonArray>()) {
    //? spectrophotometerIndex: 待動作的光度計編號
    //? spectrophotometerIndex 為 0 時，代表藍光模組
    //? spectrophotometerIndex 為 1 時，代表綠光模組
    int spectrophotometerIndex = spectrophotometerItem["index"].as<int>();
  
    JsonObject spectrophotometerConfigChose = (*Device_Ctrl.JSON__SpectrophotometerConfig)[spectrophotometerIndex].as<JsonObject>();
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
    vTaskDelay(500/portTICK_PERIOD_MS);
    INA226 ina226(Device_Ctrl._Wire);
    ina226.begin(sensorAddr);
    ina226.configure(
      INA226_AVERAGES_4, // 采样平均
      INA226_BUS_CONV_TIME_140US, //采样时间
      INA226_SHUNT_CONV_TIME_140US, //采样时间
      INA226_MODE_SHUNT_BUS_CONT // Shunt和Bus电压连续测量
    );
    ina226.calibrate(0.01, 4);
    int countNum = 100;
    double shuntvoltageBuffer_ina226 = 0;
    double busvoltageBuffer_ina226 = 0;
    for (int i=0;i<countNum;i++) {
      shuntvoltageBuffer_ina226 += (double)ina226.readShuntVoltage();
      busvoltageBuffer_ina226 += (double)ina226.readBusVoltage();
      vTaskDelay(10/portTICK_PERIOD_MS);
    }
    double busvoltage_ina226 = busvoltageBuffer_ina226/countNum;
    //?  最終量測的結果數值 (mV)
    double finalValue = busvoltage_ina226*1000;
    digitalWrite(activePin, LOW);
    if (spectrophotometerIndex == 0) {
      Device_Ctrl.lastLightValue_NH4 = finalValue;
    } else if (spectrophotometerIndex == 1) {
      Device_Ctrl.lastLightValue_NO3 = finalValue;
    }
    Serial.printf("%s - %s:%.2f", poolChose.c_str(), value_name.c_str(), finalValue);
    (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][value_name]["Value"].set(String(finalValue,2).toDouble());
    (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][value_name]["data_time"].set(GetDatetimeString());

    Device_Ctrl.InsertNewDataToDB(GetDatetimeString(), poolChose, value_name, finalValue);

    if (value_name.lastIndexOf("wash_volt")!=-1) {
      //! 本次測量的是清洗電壓
    }
    else if (value_name.lastIndexOf("test_volt")!=-1) {
      //! 本次測量的是量測電壓
      //! 需要套用校正參數，獲得真實濃度
      double mValue = spectrophotometerConfigChose["calibration"][0]["ret"]["m"].as<double>();
      double bValue = spectrophotometerConfigChose["calibration"][0]["ret"]["b"].as<double>();
      String TargetType = value_name.substring(0,3); 
      double A0_Value = 0;
      if (TargetType == "NO2") {
        A0_Value = Device_Ctrl.lastLightValue_NO3;
      } else if (TargetType == "NH4") {
        A0_Value = Device_Ctrl.lastLightValue_NH4;
      }
      double finalValue_after = -log10(finalValue/A0_Value)*mValue+bValue;
      if (finalValue_after < 0) {
        finalValue_after = 0;
      }
      (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][TargetType]["Value"].set(String(finalValue_after,2).toDouble());
      (*Device_Ctrl.JSON__sensorDataSave)[poolChose]["DataItem"][TargetType]["data_time"].set(GetDatetimeString());
      Device_Ctrl.InsertNewDataToDB(GetDatetimeString(), poolChose, TargetType, finalValue_after);
      Serial.printf("%s - %s:%.2f", poolChose.c_str(), TargetType.c_str(), finalValue_after);
    }
  }
        
  return 1;
}

int Do_PHmeterAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem)
{
  ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"PH計控制");
  for (JsonObject PHmeterItem : eventItem["ph_meter"].as<JsonArray>()) {
    pinMode(PIN__ADC_PH_IN, INPUT);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    String poolChose = PHmeterItem["pool"].as<String>();
    uint16_t phValue[30];
    for (int i=0;i<30;i++) {
      phValue[i] = analogRead(15);
      vTaskDelay(50/portTICK_PERIOD_MS);
    }
    //* 原始電壓數值獲得
    double PH_RowValue = afterFilterValue(phValue, 30);
    double m = (*Device_Ctrl.JSON__PHmeterConfig)[0]["calibration"][0]["ret"]["m"].as<String>().toDouble();
    double b = (*Device_Ctrl.JSON__PHmeterConfig)[0]["calibration"][0]["ret"]["b"].as<String>().toDouble();
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
    Device_Ctrl.InsertNewDataToDB(GetDatetimeString(), poolChose, "pH", pHValue);


    char logBuffer[1000];
    sprintf(
      logBuffer, 
      "PH量測結果, 測量原始值: %s, 轉換後PH值: %s",
      String(PH_RowValue, 2).c_str(), String(pHValue, 2).c_str()
    );
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"       - %s", String(logBuffer).c_str());
  }
  return 1;
}

#endif