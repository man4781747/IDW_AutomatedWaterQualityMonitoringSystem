#ifndef DoAction_Servo_H
#define DoAction_Servo_H

#include "../../MAIN__DeviceCtrl.h"
#include <ArduinoJson.h>
#include <vector>
#include "common.h"
#include "lx_20s.h"


/** 伺服馬達功能設定
 *  可設定參數: 
 *    1. (必要/int)index: 伺服馬達編號
 *    2. (必要/int)status: 目標角度
 * 
 *  設定檔範例: 
 *   {"pwm_motor_list": [
 *       {"index": 1,"status": 0},
 *       {"index": 2,"status": 90},
 *       {"index": 3,"status": 180},
 *  ]},
 *   
 */
struct DoServoAction : DoAction
{
  explicit DoServoAction(JsonObject eventItem, StepTaskDetail* StepTaskDetailItem) :  DoAction(eventItem, StepTaskDetailItem){};
  
  void Init_Serial() {
    Serial2.end();
    Serial2.begin(115200,SERIAL_8N1, PIN__Serial_LX_20S_RX, PIN__Serial_LX_20S_TX);
  }
  void TurnOnPower() {
    digitalWrite(PIN__EN_Servo_Motor, HIGH);
  }
  void TurnOffPower() {
    digitalWrite(PIN__EN_Servo_Motor, LOW);
  }

  void Run() {
    xSemaphoreTake(Device_Ctrl.xMutex__LX_20S, portMAX_DELAY);
    this->Init_Serial();
    this->TurnOnPower();
    vTaskDelay(1000/portTICK_PERIOD_MS);
    if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {this->StopByOutside();return;}
    
    String anyFail;
    
    std::vector<int> UnDoneServoItemIndex;
    // DynamicJsonDocument ServoStatusSave(1024); // 紀錄伺服馬達運行的結果 [true, false,....]
    DynamicJsonDocument ServoOldPostion(1024); // 紀錄伺服馬達運行前的角度 [0, 90, 180,....]
    //? 初始化流程所需的資料
    for (JsonObject servoMotorItem : eventItem["pwm_motor_list"].as<JsonArray>()) {
      int servoMotorIndex = servoMotorItem["index"].as<int>();
      int targetAngValue = map(servoMotorItem["status"].as<int>(), -30, 210, 0, 1000);
      //? 首先讀取目標馬達的角度
      int retryCount = 0;
      int nowPosition = LX_20S_SerialServoReadPosition(Serial2, servoMotorIndex);
      while (nowPosition < -100) {
        //? 若發現該馬達抓不到角度資訊，重試前重新初始化Serial與重新上電
        retryCount++;
        this->TurnOffPower();
        vTaskDelay(100/portTICK_PERIOD_MS);
        this->TurnOnPower();
        this->Init_Serial();
        vTaskDelay(10/portTICK_PERIOD_MS);
        nowPosition = (LX_20S_SerialServoReadPosition(Serial2, servoMotorIndex));
        if (retryCount > 10) {
          anyFail += String(servoMotorIndex)+", ";
          break;
        }
      }
      ServoOldPostion[String(servoMotorItem["index"].as<int>())] = nowPosition;
      if (abs(targetAngValue - nowPosition) > 20) {
        UnDoneServoItemIndex.push_back(servoMotorItem["index"].as<int>());
        // ServoStatusSave[String(servoMotorItem["index"].as<int>())] = false;
      }
      if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {this->StopByOutside();return;}
    }
    if (anyFail.length() != 0) {
      ResultMessage = "無法與伺服馬達溝通\n異常馬達編號: "+anyFail;
      result_code = ResultCode::STOP_DEVICE;
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, ResultMessage.c_str());
      Device_Ctrl.BroadcastLogToClient(NULL, 1, ResultMessage.c_str());
      return;
    }

    for (int ReTry=0;ReTry<10;ReTry++) {
      if (ReTry > 5) {
        ESP_LOGW("", "(重試第%d次)前次伺服馬達 (%s) 運作有誤，即將重試", ReTry, anyFail.c_str());
        Device_Ctrl.BroadcastLogToClient(NULL, 1,  "(重試第%d次)前次伺服馬達 (%s) 運作有誤，即將重試", ReTry, anyFail.c_str());
        //? 馬達運作重試前，Serial重設，電也重上
        Serial2.end();
        this->TurnOffPower();
        vTaskDelay(2000/portTICK_PERIOD_MS);
        this->TurnOnPower();
        this->Init_Serial();
        vTaskDelay(10/portTICK_PERIOD_MS);
      }
      anyFail = "";
      //? 對每個尚未運行完畢的伺服馬達發出指令
      for (int index : UnDoneServoItemIndex) {
        JsonObject servoMotorItem = eventItem["pwm_motor_list"][index].as<JsonObject>();
        int targetAngValue = map(servoMotorItem["status"].as<int>(), -30, 210, 0, 1000);
        ESP_LOGD(StepTaskDetailItem->TaskName.c_str(),"伺服馬達(LX-20S) %d 轉至 %d 度(%d)", servoMotorItem["index"].as<int>(),servoMotorItem["status"].as<int>(), targetAngValue);
        if (ReTry > 5) {
          Device_Ctrl.BroadcastLogToClient(NULL, 0,  "(重試第%d次)前伺服馬達(LX-20S) %d 轉至 %d 度(%d)", ReTry,
            servoMotorItem["index"].as<int>(), servoMotorItem["status"].as<int>(), targetAngValue
          );
        }
        LX_20S_SerialServoMove(Serial2, servoMotorItem["index"].as<int>(),targetAngValue,500);
        vTaskDelay(10/portTICK_PERIOD_MS);
        if (ReTry > 5) {
          LX_20S_SerialServoMove(Serial2, servoMotorItem["index"].as<int>(),targetAngValue,500);
          vTaskDelay(10/portTICK_PERIOD_MS);
          LX_20S_SerialServoMove(Serial2, servoMotorItem["index"].as<int>(),targetAngValue,500);
          vTaskDelay(10/portTICK_PERIOD_MS);
        }
        if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {this->StopByOutside();return;}
      }
      if (ReTry > 5) {vTaskDelay(1500/portTICK_PERIOD_MS);} else {vTaskDelay(700/portTICK_PERIOD_MS);}
      if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {this->StopByOutside();return;}
      
      for (int i=0;i<UnDoneServoItemIndex.size();i++) {
        int indexChose = UnDoneServoItemIndex.front();
        UnDoneServoItemIndex.erase(UnDoneServoItemIndex.begin());
        JsonObject servoMotorItem = eventItem["pwm_motor_list"][indexChose].as<JsonObject>();
        int ServoIndex = servoMotorItem["index"].as<int>();
        int targetAngValue = map(servoMotorItem["status"].as<int>(), -30, 210, 0, 1000);
        int readAng = LX_20S_SerialServoReadPosition(Serial2, ServoIndex);
        for (int readAngRetry = 0;readAngRetry<10;readAngRetry++) {
          if (readAng > -100) {break;}
          ESP_LOGW("", "伺服馬達 %d 讀取角度有誤: %d", ServoIndex, readAng);
          this->TurnOffPower();
          vTaskDelay(100/portTICK_PERIOD_MS);
          this->TurnOnPower();
          this->Init_Serial();
          vTaskDelay(10/portTICK_PERIOD_MS);
          readAng = LX_20S_SerialServoReadPosition(Serial2, ServoIndex);
        }
        int d_ang = targetAngValue - readAng;
        ESP_LOGD("", "伺服馬達 %d 目標角度: %d\t真實角度: %d", ServoIndex, targetAngValue, readAng);
        if (ReTry >= 5 ) {
          Device_Ctrl.BroadcastLogToClient(NULL, 1, "伺服馬達 %d 目標角度: %d\t真實角度: %d", ServoIndex, targetAngValue, readAng);
        }

        if (abs(d_ang)>20) {
          anyFail += String(ServoIndex);
          anyFail += ",";
          UnDoneServoItemIndex.push_back(indexChose);
        }
        else {
          if (abs(ServoOldPostion[String(ServoIndex)].as<int>() - readAng) > 500) {
            Device_Ctrl.ItemUsedAdd("Servo_"+String(ServoIndex), 2);
          } else {
            Device_Ctrl.ItemUsedAdd("Servo_"+String(ServoIndex), 1);
          }
        }
        if (StepTaskDetailItem->TaskStatus == StepTaskStatus::Close) {this->StopByOutside();return;}
        vTaskDelay(100/portTICK_PERIOD_MS);
      }
      if (anyFail == "") {break;}
    }

    // Serial2.end();
    this->TurnOffPower();
    xSemaphoreGive(Device_Ctrl.xMutex__LX_20S);

    if (anyFail != "") {
      ResultMessage = "偵測到伺服馬達異常\n異常馬達編號: "+anyFail;
      ESP_LOGE(StepTaskDetailItem->TaskName.c_str(), "伺服馬達(LX-20S)發生預期外的動作，準備中止儀器的所有動作");
      Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "伺服馬達(LX-20S)發生預期外的動作，準備中止儀器的所有動作");
      Device_Ctrl.BroadcastLogToClient(NULL, 1, "伺服馬達(LX-20S)發生預期外的動作，準備中止儀器的所有動作");
      result_code = ResultCode::STOP_DEVICE;
      return;
    }
    result_code = ResultCode::SUCCESS;
    ResultMessage = "";
    return;
  };

  void StopByOutside() {
    ESP_LOGI(StepTaskDetailItem->TaskName.c_str(),"收到緊急中斷要求，準備停止當前Step");
    digitalWrite(PIN__EN_Servo_Motor, LOW);
    xSemaphoreGive(Device_Ctrl.xMutex__LX_20S);
    result_code = ResultCode::STOP_BY_OUTSIDE;
    ResultMessage = "";
  }
};

#endif