#ifndef EXFILE_FUNCTION_H
#define EXFILE_FUNCTION_H
#include <esp_log.h>
#include <SD.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

inline DynamicJsonDocument ExFile_listDir(fs::FS& fileSystem, String dir) {
  DynamicJsonDocument fileNameListItem(10000);
  if (fileSystem.exists(dir)) {
    File folder = fileSystem.open(dir.c_str());
    while (true) {
      File Entry = folder.openNextFile();
      if (! Entry) {
        break;
      }
      DynamicJsonDocument fileInfo(500);
      fileInfo["size"].set(Entry.size());
      fileInfo["name"].set(String(Entry.name()));
      fileInfo["getLastWrite"].set(Entry.getLastWrite());
      Entry.close();
      fileNameListItem.add(fileInfo);
    }
  }
  return fileNameListItem;
}

inline bool ExFile_CreateFile(fs::FS& fileSystem, String FilePath) {
  if (fileSystem.exists(FilePath) == true){
    return true;
  }
  ESP_LOGI("", "建立檔案: %s", FilePath.c_str());
  String folderPath = FilePath.substring(0,FilePath.lastIndexOf('/')+1);
  if (folderPath.indexOf('/') == 0){
    folderPath = folderPath.substring(1,folderPath.length());
  }
  String folderPathStack = "";
  int index;
  int length;
  while (1){
    index = folderPath.indexOf('/');
    if (index <0) {
      break;
    }
    folderPathStack = folderPathStack + "/" + folderPath.substring(0,index);
    folderPath = folderPath.substring(index+1,folderPath.length());
    if (fileSystem.exists(folderPathStack) == false){
      ESP_LOGD("", "建立資料夾: %s", folderPathStack.c_str());
      if (fileSystem.mkdir(folderPathStack) == false) {
        return false;
      }
    }
  }
  if (fileSystem.exists(FilePath) == false){
    File selectedFile = fileSystem.open(FilePath, FILE_WRITE);
    selectedFile.close();
  }
  return true;
}

inline bool ExFile_WriteJsonFile(fs::FS& fileSystem, String filePath, JsonDocument &jsonData) {
  if (!fileSystem.exists(filePath)) {
     return true;
  }
  File file = fileSystem.open(filePath, FILE_WRITE);
  size_t writeSize = serializeJson(jsonData, file);
  file.close();
  file.close();
  file.close();
  ESP_LOGD("", "已將JSON格式資料寫入檔案: %s, 寫入長度: %d", filePath.c_str(), writeSize);
  return true;
}

inline bool ExFile_LoadJsonFile(fs::FS& fileSystem, String filePath, JsonDocument &jsonData) {
  if (fileSystem.exists(filePath)) {
    File file = fileSystem.open(filePath, FILE_READ);
    DeserializationError error = deserializeJson(jsonData, file);
    if (error) {
      ESP_LOGE("", "JSON格式檔案: %s 讀取失敗，失敗原因: %s", filePath.c_str(), error.c_str());
      return false;
    }
    return true;
  }
  ESP_LOGE("", "找不到檔案: %s, 無法讀取", filePath.c_str());
  return false;
}

#endif