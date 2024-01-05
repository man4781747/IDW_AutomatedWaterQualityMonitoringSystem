#include <Arduino.h>
#include <esp_log.h>
#include "./DeviceCtrl/MAIN__DeviceCtrl.h"
#include "TimeLibExternalFunction.h"

void setup() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);
  ESP_LOGI("MAIN", "儀器啟動中, 以下為相關基本資訊");
  ESP_LOGI("MAIN", "Cpu Frequency: %d MHz", getCpuFrequencyMhz());
  ESP_LOGI("MAIN", "Apb Frequency: %d MHz", getApbFrequency());
  ESP_LOGI("MAIN", "Spiram Size: %d Byte", esp_spiram_get_size());
  ESP_LOGI("MAIN", "Flash Chip Size: %d Byte", spi_flash_get_chip_size());
  ESP_LOGI("MAIN", "=====================================");
  Device_Ctrl.INIT_Pins();
  
  ESP_LOGI("MAIN", "初始化I2C總線");
  Device_Ctrl.INIT_I2C_Wires(Wire);

  Device_Ctrl.INIT_SD();

  Device_Ctrl.INIT_SPIFFS();

  Device_Ctrl.INIT_SqliteDB();

  Device_Ctrl.LoadConfigJsonFiles();

  Device_Ctrl.INIT_PoolData();

  Device_Ctrl.UpdatePipelineConfigList();

  Device_Ctrl.peristalticMotorsCtrl.INIT_Motors(PIN__Peristaltic_Motor_SHCP,PIN__Peristaltic_Motor_SHTP,PIN__Peristaltic_Motor_DATA,1);

  Device_Ctrl.CreatePipelineFlowScanTask();

  Device_Ctrl.CreateStepTasks();

  Device_Ctrl.preLoadWebJSFile();

  Device_Ctrl.ConnectWiFi();

  Device_Ctrl.INITWebServer();

  Device_Ctrl.CreateOTAService();

  Device_Ctrl.CreateScheduleManagerTask();

  Device_Ctrl.CreateTimerCheckerTask();

  Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "開機完畢");
}

void loop() {
  if (Serial.available()){
    String getMessage = Serial.readString();
    if (getMessage == "restart") {
      esp_restart();
    }
  }
  vTaskDelay(1000/portTICK_PERIOD_MS);
}