#ifndef SQLITE_FUNCTIONS_H
#define SQLITE_FUNCTIONS_H

#include <Arduino.h>
#include <sqlite3.h>
#include <ArduinoJson.h>

SemaphoreHandle_t SQL_xMutex = NULL;
char *zErrMsg = 0;

static int callback(void *data, int argc, char **argv, char **azColName){
  int i;
  JsonDocument *targetJsonObj = (JsonDocument*)data;
  if (targetJsonObj==NULL) {
    return 0;
  }
  DynamicJsonDocument oneLogData(2000);
  for (i = 0; i<argc; i++){
    String KeyName = String(azColName[i]);
    String Value = String(argv[i] ? argv[i] : "NULL");
    oneLogData[KeyName].set(Value);
  }
  (*targetJsonObj).add(oneLogData);
  return 0;
}

int db_exec(sqlite3 *db, String sql, JsonDocument *jsonData=NULL) {
  if (SQL_xMutex == NULL) {
    vSemaphoreCreateBinary(SQL_xMutex);
  }
  if (jsonData!=NULL) {
    (*jsonData).clear();
  }
  xSemaphoreTake(SQL_xMutex, portMAX_DELAY);
  Serial.println(sql);
  long start = micros();
  int rc = sqlite3_exec(db, sql.c_str(), callback, (void*)jsonData, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("SQL error: %s\n", zErrMsg);
    String ErrType = String(zErrMsg);
    sqlite3_free(zErrMsg);
    if (ErrType == "disk I/O error") {
      SD.end();
      SD.begin(PIN__SD_CS);
      sqlite3_initialize();
      sqlite3_open(Device_Ctrl.FilePath__SD__MainDB.c_str(), &Device_Ctrl.DB_Main);
      int retry_rc = sqlite3_exec(Device_Ctrl.DB_Main, sql.c_str(), callback, (void*)jsonData, &zErrMsg);
      if (retry_rc != SQLITE_OK) {
        ESP_LOGE("Sqlite", "Sqlite發現DB無法讀寫，強制重開機");
        ESP.restart();
      }
    }
  } else {
    Serial.printf("Operation done successfully\n");
  }
  Serial.print(F("Time taken:"));
  Serial.println(micros()-start);
  // if (jsonData!=NULL) {
  //   serializeJsonPretty(*jsonData, Serial);
  // }
  xSemaphoreGive(SQL_xMutex);
  return rc;
}

static int callback_v2(void *data, int argc, char **argv, char **azColName){
  int i;
  JsonArray *targetJsonObj = (JsonArray*)data;
  if (targetJsonObj==NULL) {
    return 0;
  }
  DynamicJsonDocument oneLogData(2000);
  for (i = 0; i<argc; i++){
    String KeyName = String(azColName[i]);
    String Value = String(argv[i] ? argv[i] : "NULL");
    oneLogData[KeyName].set(Value);
    // Serial.printf("%s: %s\n", KeyName.c_str(), Value.c_str());
  }
  (*targetJsonObj).add(oneLogData);
  return 0;
}


int db_exec_http(sqlite3 *db, String sql, JsonArray *jsonData=NULL) {
  xSemaphoreTake(SQL_xMutex, portMAX_DELAY);
  // if (jsonData!=NULL) {
  //   (*jsonData).clear();
  // }
  Serial.println(sql);
  long start = micros();
  int rc = sqlite3_exec(db, sql.c_str(), callback_v2, (void*)jsonData, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    Serial.printf("Operation done successfully\n");
  }
  Serial.print(F("Time taken:"));
  Serial.println(micros()-start);
  // if (jsonData!=NULL) {
  //   serializeJsonPretty(*jsonData, Serial);
  // }
  xSemaphoreGive(SQL_xMutex);
  return rc;
}

#endif