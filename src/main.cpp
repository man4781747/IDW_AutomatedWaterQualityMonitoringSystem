#include <Arduino.h>
#include <esp_log.h>
#include "./DeviceCtrl/MAIN__DeviceCtrl.h"
#include "TimeLibExternalFunction.h"
#include "hal/efuse_hal.h"

//TODO
#include "esp_core_dump.h"

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
  // TODO
  esp_core_dump_init();
  if (esp_core_dump_image_check() == ESP_OK) {
    Serial.println("Core Dump gefunden");
  } else {
    Serial.println("Core Dump NOT gefunden");
  }
  esp_core_dump_summary_t *summary = (esp_core_dump_summary_t*)malloc(sizeof(esp_core_dump_summary_t));
	if (summary) {
		esp_log_level_set("esp_core_dump_elf", ESP_LOG_VERBOSE); // so that func below prints stuff.. but doesn't actually work, have to set logging level globally through menuconfig
		printf("Retrieving core dump summary..\n");
		esp_err_t err = esp_core_dump_get_summary(summary);	
		if (err == ESP_OK) {
			//get summary function already pints stuff
			printf("Getting core dump summary ok.\n");

      esp_core_dump_bt_info_t bt_info = summary->exc_bt_info;
      char results[1024]; // Assuming a maximum of 256 characters for the backtrace string
      int offset = snprintf(results, sizeof(results), "Backtrace:");
      for (int i = 0; i < bt_info.depth; i++)
      {
        uintptr_t pc = bt_info.bt[i]; // Program Counter (PC)
        int len = snprintf(results + offset, sizeof(results) - offset, " 0x%08X", pc);
        if (len >= 0 && offset + len < sizeof(results))
        {
          offset += len;
        }
        else
        {
          break; // Reached the limit of the results buffer
        }
      }

      ESP_LOGI("", "[backtrace]: %s", results);
      Serial.println(results);
      ESP_LOGI("", "[backtrace]Backtrace Task: %s", summary->exc_task);
      ESP_LOGI("", "[backtrace]Backtrace Depth: %u", bt_info.depth);
      ESP_LOGI("", "[backtrace]Backtrace Corrupted: %s", bt_info.corrupted ? "Yes" : "No");
      ESP_LOGI("", "[backtrace]Program Counter: %d", summary->exc_pc);
      ESP_LOGI("", "[backtrace]Coredump Version: %d", summary->core_dump_version);

			//todo: do something with dump summary
      
		} else {
			printf("Getting core dump summary not ok. Error: %d\n", (int) err);
			printf("Probably no coredump present yet.\n");
			printf("esp_core_dump_image_check() = %d\n", esp_core_dump_image_check());
		}		
		free(summary);
	}
  // TODO


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
}

void loop() {
  

  vTaskDelay(1000/portTICK_PERIOD_MS);
  // if ((*Device_Ctrl.JSON__DeviceBaseInfo)["schedule_switch"].as<bool>()) {
  //   Serial.println("排程開啟");
  // }


  // Device_Ctrl.WriteSysInfo();


  
}
