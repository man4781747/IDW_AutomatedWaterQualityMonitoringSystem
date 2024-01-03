#include "PeristalticMotorCtrl.h"
#include <Arduino.h>


SemaphoreHandle_t Peristaltic_Motors_xMutex = xSemaphoreCreateMutex();
/**
 * @brief 蠕動馬達們控制物件初始化
 * 
 */
void C_Peristaltic_Motors_Ctrl::INIT_Motors(int SHCP_, int STCP_, int DATA_, int moduleNum_)
{
  ESP_LOGI("Motor_Ctrl","INIT_Motors");
  SHCP = SHCP_;
  STCP = STCP_;
  DATA = DATA_;
  moduleNum = moduleNum_;
  pinMode(SHCP, OUTPUT);
  pinMode(STCP, OUTPUT);  
  pinMode(DATA, OUTPUT);
  moduleDataList = new uint8_t[moduleNum];
  if (xSemaphoreTake(Peristaltic_Motors_xMutex, portMAX_DELAY) == pdTRUE) {
    memset(moduleDataList, 0, sizeof(moduleDataList));
    xSemaphoreGive(Peristaltic_Motors_xMutex);
  }
  RunMotor(moduleDataList);
}

void C_Peristaltic_Motors_Ctrl::RunMotor(uint8_t *moduleDataList)
{
  if (xSemaphoreTake(Peristaltic_Motors_xMutex, portMAX_DELAY) == pdTRUE) {
    digitalWrite(STCP, LOW);
    vTaskDelay(5);
    String settingContent = "";
    for (int index = moduleNum-1;index >= 0;index--) {
      uint8_t i;

      for(i = 0; i < 8; i++) {
          digitalWrite(DATA, !!(moduleDataList[index] & (1 << (7 - i))));
          vTaskDelay(5);
          digitalWrite(SHCP, HIGH);
          vTaskDelay(5);
          digitalWrite(SHCP, LOW);
          vTaskDelay(5);
      }



      // shiftOut(DATA, SHCP, MSBFIRST, moduleDataList[index]);

      char btyeContent[9];
      btyeContent[8] = '\0';
      for (int i = 0; i <= 7; i++) {
        btyeContent[i] = ((moduleDataList[index] >> i) & 1) ? '1' : '0';
      }
      settingContent += String(btyeContent) + "|";
    }
    digitalWrite(STCP, HIGH);
    // ESP_LOGV("蠕動馬達", "當前設定: %s", settingContent.c_str());
    vTaskDelay(10);
    xSemaphoreGive(Peristaltic_Motors_xMutex);
  }
}


void C_Peristaltic_Motors_Ctrl::SetAllMotorStop()
{
  if (xSemaphoreTake(Peristaltic_Motors_xMutex, portMAX_DELAY) == pdTRUE) {
    memset(moduleDataList, 0, moduleNum);
    xSemaphoreGive(Peristaltic_Motors_xMutex);
  }
  RunMotor(moduleDataList);
}

void C_Peristaltic_Motors_Ctrl::OpenAllPin()
{
  if (xSemaphoreTake(Peristaltic_Motors_xMutex, portMAX_DELAY) == pdTRUE) {
    memset(moduleDataList, 0b11111111, moduleNum);
    xSemaphoreGive(Peristaltic_Motors_xMutex);
  }
  RunMotor(moduleDataList);
}

void C_Peristaltic_Motors_Ctrl::SetMotorStatus(int index, PeristalticMotorStatus status)
{
  int moduleChoseIndex = index / 4;
  int motortChoseIndexInModule = index % 4;
  // ESP_LOGI("蠕動馬達", "將模組: %d 的第 %d 顆馬達數值更改為 %d", moduleChoseIndex+1, motortChoseIndexInModule+1, status);
  if (moduleChoseIndex >= moduleNum) {}
  else {
    if (xSemaphoreTake(Peristaltic_Motors_xMutex, portMAX_DELAY) == pdTRUE) {
      moduleDataList[moduleChoseIndex] &= ~(0b11 << motortChoseIndexInModule*2);
      moduleDataList[moduleChoseIndex] |= (status << motortChoseIndexInModule*2);
      xSemaphoreGive(Peristaltic_Motors_xMutex);
    }
  }
}

void C_Peristaltic_Motors_Ctrl::ShowNowSetting()
{
  char btyeContent[9];
  btyeContent[8] = '\0';
  for (int index = 0;index < moduleNum;index++) {
    for (int i = 0; i <= 7; i++) {
      btyeContent[i] = ((moduleDataList[index] >> i) & 1) ? '1' : '0';
    }
    ESP_LOGI("蠕動馬達", "當前設定: %d -> %s", index, btyeContent);
  }
}

bool C_Peristaltic_Motors_Ctrl::IsAllStop() {
  if (xSemaphoreTake(Peristaltic_Motors_xMutex, portMAX_DELAY) == pdTRUE) {
    for (int index = moduleNum-1;index >= 0;index--) {
      if ((int)moduleDataList[index] != 0) {
        xSemaphoreGive(Peristaltic_Motors_xMutex);
        return false;
      }
    }
    xSemaphoreGive(Peristaltic_Motors_xMutex);
    return true;
  }
  return false;
}
