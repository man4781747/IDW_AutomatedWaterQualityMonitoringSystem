#include <Arduino.h>
#include <esp_log.h>
#include "./DeviceCtrl/MAIN__DeviceCtrl.h"
#include "TimeLibExternalFunction.h"
#include "hal/efuse_hal.h"

//TODO
// #include <ModbusRTU.h>
// ModbusRTU mb;


//TODO

void setup() {
  setCpuFrequencyMhz(240); //? CPU效能全開
  Serial.begin(115200);
  multi_heap_info_t heap_info;
  heap_caps_get_info(&heap_info, MALLOC_CAP_SPIRAM);
  
  ESP_IDF_VERSION;
  efuse_hal_get_major_chip_version();
  efuse_hal_get_minor_chip_version();
  efuse_ll_get_blk_version_major();
  efuse_ll_get_blk_version_minor();
  ESP_LOGI("MAIN", "儀器啟動中, 以下為相關基本資訊");
  ESP_LOGI("MAIN", "Cpu Frequency: %d MHz", getCpuFrequencyMhz());
  ESP_LOGI("MAIN", "Apb Frequency: %d MHz", getApbFrequency());
  ESP_LOGI("MAIN", "Spiram Size: %d Byte", esp_spiram_get_size());
  ESP_LOGI("MAIN", "Flash Chip Size: %d Byte", spi_flash_get_chip_size());
  ESP_LOGI("MAIN", "Heap Size %d", ESP.getHeapSize());
  ESP_LOGI("MAIN", "Max Alloc Heap %d", ESP.getMaxAllocHeap());
  ESP_LOGI("MAIN", "Min Free Heap %d", ESP.getMinFreeHeap());
  ESP_LOGI("MAIN", "Free internal heap before TLS %d", ESP.getFreeHeap());
  ESP_LOGI("MAIN", "Psram Size %d", ESP.getPsramSize());
  ESP_LOGI("MAIN", "Free Psram Size %d", ESP.getFreePsram());

  ESP_LOGI("MAIN", "=====================================");
  Device_Ctrl.INIT_Pins();
  Device_Ctrl.INIT_oled();
  ESP_LOGV("MAIN", "初始化I2C總線");
  Device_Ctrl.AddNewOledLog("INIT_I2C_Wires");
  Device_Ctrl.INIT_I2C_Wires(Wire);
  ESP_LOGV("MAIN", "初始化SD卡");
  Device_Ctrl.AddNewOledLog("INIT_SD");
  Device_Ctrl.INIT_SD();
  Device_Ctrl.CheckUpdate();
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
  Device_Ctrl.peristalticMotorsCtrl.INIT_Motors(PIN__Peristaltic_Motor_SHCP,PIN__Peristaltic_Motor_SHTP,PIN__Peristaltic_Motor_DATA,2);
  Device_Ctrl.AddNewOledLog("CreatePipelineFlowScanTask");
  Device_Ctrl.CreatePipelineFlowScanTask();
  Device_Ctrl.AddNewOledLog("CreateStepTasks");
  Device_Ctrl.CreateStepTasks();
  Device_Ctrl.AddNewOledLog("preLoadWebJSFile");
  Device_Ctrl.preLoadWebJSFile();
  Device_Ctrl.AddNewOledLog("CreateWifiManager");
  Device_Ctrl.CreateWifiManagerTask();
  Device_Ctrl.AddNewOledLog("INITWebServer");
  Device_Ctrl.INITWebServer();
  Device_Ctrl.AddNewOledLog("CreateOTAService");
  Device_Ctrl.CreateOTAService();
  Device_Ctrl.AddNewOledLog("CreateScheduleManagerTask");
  Device_Ctrl.CreateScheduleManagerTask();
  Device_Ctrl.AddNewOledLog("CreateTimerCheckerTask");
  Device_Ctrl.CreateTimerCheckerTask();
  Device_Ctrl.AddNewOledLog("CreateNotifyTask");
  Device_Ctrl.CreateLINE_MAIN_NotifyTask();
  
  Device_Ctrl.InsertNewLogToDB(GetDatetimeString(), 1, "開機完畢");
  Device_Ctrl.CreateOledQRCodeTask();
  Device_Ctrl.all_INIT_done = true;

  //TODO
  // Serial1.begin(115200,SERIAL_8N1,PIN__Step_Motor_RS485_RX, PIN__Step_Motor_RS485_TX);
  // mb.begin(&Serial1);
  // mb.setBaudrate(115200);
  // mb.master();
  //TODO

}

void loop() {
  // //TODO
  // uint16_t buffer[5];
  // uint16_t wruteData[4] = {1, 2000,0,0};
  // mb.writeHreg(1,1,wruteData,4);
  // while(mb.slave()) { // Check if transaction is active
  //   mb.task();
  //   delay(10);
  // }
  // if (!mb.slave()) {
  //   while (true) {
  //     mb.readHreg(1, 1, buffer, 1);
  //     while(mb.slave()) { // Check if transaction is active
  //       mb.task();
  //       delay(10);
  //     }
  //     if (buffer[0] != 1) {
  //       break;
  //     }
  //   }
  // }
  // //TODO


  vTaskDelay(60000/portTICK_PERIOD_MS);
  Device_Ctrl.WriteSysInfo();


  
}
