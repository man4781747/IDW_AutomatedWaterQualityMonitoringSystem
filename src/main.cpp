#include <Arduino.h>
#include <esp_log.h>
#include "./DeviceCtrl/MAIN__DeviceCtrl.h"
#include "TimeLibExternalFunction.h"

void setup() {
  setCpuFrequencyMhz(240); //? CPU效能全開
  Serial.begin(115200);
  ESP_LOGI("MAIN", "儀器啟動中, 以下為相關基本資訊");
  ESP_LOGI("MAIN", "Cpu Frequency: %d MHz", getCpuFrequencyMhz());
  ESP_LOGI("MAIN", "Apb Frequency: %d MHz", getApbFrequency());
  ESP_LOGI("MAIN", "Spiram Size: %d Byte", esp_spiram_get_size());
  ESP_LOGI("MAIN", "Flash Chip Size: %d Byte", spi_flash_get_chip_size());
  ESP_LOGI("MAIN", "Heap Size %u", ESP.getHeapSize());
  ESP_LOGI("MAIN", "Max Alloc Heap %u", ESP.getMaxAllocHeap());
  ESP_LOGI("MAIN", "Min Free Heap %u", ESP.getMinFreeHeap());
  ESP_LOGI("MAIN", "Free internal heap before TLS %u", ESP.getFreeHeap());

  ESP_LOGI("MAIN", "=====================================");
  Device_Ctrl.INIT_Pins();
  Device_Ctrl.INIT_oled();
  ESP_LOGV("MAIN", "初始化I2C總線");
  Device_Ctrl.AddNewOledLog("INIT_I2C_Wires");
  Device_Ctrl.INIT_I2C_Wires(Wire);
  ESP_LOGV("MAIN", "初始化SD卡");
  Device_Ctrl.AddNewOledLog("INIT_SD");
  Device_Ctrl.INIT_SD();
  ESP_LOGV("MAIN", "初始化SPIFFS");
  Device_Ctrl.AddNewOledLog("INIT_SPIFFS");
  Device_Ctrl.INIT_SPIFFS();
  ESP_LOGV("MAIN", "初始化SQLITE資料庫");
  Device_Ctrl.AddNewOledLog("INIT_SqliteDB");
  Device_Ctrl.INIT_SqliteDB();
  ESP_LOGV("MAIN", "讀取各設定檔");
  Device_Ctrl.AddNewOledLog("LoadConfigJsonFiles");
  Device_Ctrl.LoadConfigJsonFiles();
  ESP_LOGV("MAIN", "讀取最近的感測器資料");
  Device_Ctrl.AddNewOledLog("INIT_PoolData");
  Device_Ctrl.INIT_PoolData();
  ESP_LOGV("MAIN", "讀取Pipeline列表");
  Device_Ctrl.AddNewOledLog("UpdatePipelineConfigList");
  Device_Ctrl.UpdatePipelineConfigList();
  
  Device_Ctrl.AddNewOledLog("INIT_Motors");
  Device_Ctrl.peristalticMotorsCtrl.INIT_Motors(PIN__Peristaltic_Motor_SHCP,PIN__Peristaltic_Motor_SHTP,PIN__Peristaltic_Motor_DATA,1);
  Device_Ctrl.AddNewOledLog("CreatePipelineFlowScanTask");
  Device_Ctrl.CreatePipelineFlowScanTask();
  Device_Ctrl.AddNewOledLog("CreateStepTasks");
  Device_Ctrl.CreateStepTasks();
  Device_Ctrl.AddNewOledLog("preLoadWebJSFile");
  Device_Ctrl.preLoadWebJSFile();
  Device_Ctrl.AddNewOledLog("ConnectWiFi");
  Device_Ctrl.ConnectWiFi();
  Device_Ctrl.AddNewOledLog("INITWebServer");
  Device_Ctrl.INITWebServer();
  Device_Ctrl.AddNewOledLog("CreateOTAService");
  Device_Ctrl.CreateOTAService();
  Device_Ctrl.AddNewOledLog("CreateScheduleManagerTask");
  Device_Ctrl.CreateScheduleManagerTask();
  Device_Ctrl.AddNewOledLog("CreateTimerCheckerTask");
  Device_Ctrl.CreateTimerCheckerTask();
  
  Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "開機完畢");
  Device_Ctrl.CreateOledQRCodeTask();
}

void loop() {
  if (Serial.available()){
    String getMessage = Serial.readString();
    if (getMessage == "restart") {
      esp_restart();
    }
  }
  vTaskDelay(5000/portTICK_PERIOD_MS);
  // Device_Ctrl.DeleteOldLog();
  // Device_Ctrl.SendGmailNotifyMessage("TSET", "test");
  // vTaskDelay(5000000/portTICK_PERIOD_MS);
}
