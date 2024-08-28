#ifndef WebsocketSetting_H
#define WebsocketSetting_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <unordered_map>
#include "MAIN__DeviceCtrl.h"

DynamicJsonDocument urlParamsToJSON(const std::string& urlParams) {
  std::stringstream ss(urlParams);
  std::string item;
  std::unordered_map<std::string, std::string> paramMap;
  while (std::getline(ss, item, '&')) {
    std::stringstream ss2(item);
    std::string key, value;
    std::getline(ss2, key, '=');
    std::getline(ss2, value, '=');
    paramMap[key] = value;
  }
  DynamicJsonDocument json_doc(1000);
  for (auto& it : paramMap) {
    json_doc[it.first].set(it.second);
  }
  return json_doc;
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("WebSocket client connected");
    DynamicJsonDocument D_baseInfo = Device_Ctrl.GetBaseWSReturnData("");
    D_baseInfo["cmd"].set("CONNECTED");
    D_baseInfo["action"]["message"].set("OK");
    D_baseInfo["action"]["status"].set("OK");
    String returnString;
    serializeJsonPretty(D_baseInfo, returnString);
    //? 2024/8/28 NodeRed端新需求
    //? 儀器開一條ws專線
    //? 所有 /ws/NodeRed 線上的資訊都會廣播給所有該線上的所有人得知
    if (String(server->url()) == "/ws") {
      client->binary(returnString);
    } else {
      server->binaryAll(returnString);
    }
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("WebSocket client disconnected");
  } else if (type == WS_EVT_DATA) {
    vTaskDelay(10/portTICK_PERIOD_MS);
    String MessageString = String((char *)data);
    int totalLen = MessageString.length();
    size_t length = 0;
    for (const uint8_t* p = data; *p; ++p) {
      ++length;
    }
    MessageString = MessageString.substring(0, len);
    int commaIndex = MessageString.indexOf("]");
    String METHOD = MessageString.substring(1, commaIndex);
    bool dataFail = false;
    if (METHOD == "PATCH" or METHOD == "POST") {
      int paraLen = MessageString.length();
      if (paraLen == 0) {
        dataFail = true;
      } else {
        char lastChar = MessageString.charAt(paraLen-1);
        if (lastChar != '\n') {
          dataFail = true;
        } else {
          MessageString = MessageString.substring(commaIndex + 1, paraLen);
        }
      }
    } else {
      MessageString = MessageString.substring(commaIndex + 1);
    }
    commaIndex = MessageString.indexOf("?");
    String Message_CMD = MessageString.substring(0, commaIndex);
    String QueryParameter = MessageString.substring(commaIndex + 1);
    DynamicJsonDocument D_QueryParameter = urlParamsToJSON(QueryParameter.c_str());
    DynamicJsonDocument D_FormData(10000);
    String dataString = D_QueryParameter["data"].as<String>();
    DeserializationError error = deserializeJson(D_FormData, dataString);
    if (error or dataFail) {
      DynamicJsonDocument D_errorbaseInfo = Device_Ctrl.GetBaseWSReturnData("["+METHOD+"]"+Message_CMD);
      D_errorbaseInfo["message"] = "FAIL";
      D_errorbaseInfo["parameter"]["message"] = "API帶有的Data解析錯誤，參數格式錯誤?";
      String ErrorMessage;
      serializeJsonPretty(D_errorbaseInfo, ErrorMessage);
      //? 2024/8/28 NodeRed端新需求
      //? 儀器開一條ws專線
      //? 所有 /ws/NodeRed 線上的資訊都會廣播給所有該線上的所有人得知
      if (String(server->url()) == "/ws") {
        client->binary(ErrorMessage);
      } else {
        server->binaryAll(ErrorMessage);
      }
      D_errorbaseInfo.clear();
    }
    else {
      D_QueryParameter.remove("data");
      std::string Message_CMD_std = std::string(Message_CMD.c_str());
      std::string METHOD_std = std::string(METHOD.c_str());
      DynamicJsonDocument D_baseInfo = Device_Ctrl.GetBaseWSReturnData("["+String(METHOD_std.c_str())+"]"+String(Message_CMD_std.c_str()));
      bool IsFind = false;
      for (auto it = Device_Ctrl.websocketApiSetting.rbegin(); it != Device_Ctrl.websocketApiSetting.rend(); ++it) {
        std::regex reg(it->first.c_str());
        std::smatch matches;
        if (std::regex_match(Message_CMD_std, matches, reg)) {
          std::map<int, String> UrlParameter;
          IsFind = true;
          if (it->second.count(METHOD_std)) {
            DynamicJsonDocument D_PathParameter(1000);
            int pathParameterIndex = -1;
            for (auto matches_it = matches.begin(); matches_it != matches.end(); ++matches_it) {
              if (pathParameterIndex == -1) {
                pathParameterIndex++;
                continue;
              }
              if ((int)(it->second[METHOD_std]->pathParameterKeyMapList.size()) <= pathParameterIndex) {
                break;
              }
              D_PathParameter[it->second[METHOD_std]->pathParameterKeyMapList[pathParameterIndex]] = String(matches_it->str().c_str());
            }
            it->second[METHOD_std]->func(server, client, &D_baseInfo, &D_PathParameter, &D_QueryParameter, &D_FormData);
          } else {
            ESP_LOGE("WS", "API %s 並無設定 METHOD: %s", Message_CMD_std.c_str(), METHOD_std.c_str());
            D_baseInfo["message"] = "FAIL";
            D_baseInfo["parameter"]["message"] = "Not allow: "+String(METHOD_std.c_str());
            String returnString;
            serializeJsonPretty(D_baseInfo, returnString);
            //? 2024/8/28 NodeRed端新需求
            //? 儀器開一條ws專線
            //? 所有 /ws/NodeRed 線上的資訊都會廣播給所有該線上的所有人得知
            if (String(server->url()) == "/ws") {
              client->binary(returnString);
            } else {
              server->binaryAll(returnString);
            }
          }
          break;
        }
      }
      if (!IsFind) {
        ESP_LOGE("WS", "找不到API: %s", Message_CMD_std.c_str());
        D_baseInfo["parameter"]["message"] = "找不到API: "+String(Message_CMD_std.c_str());
        D_baseInfo["action"]["status"] = "FAIL";
        D_baseInfo["action"]["message"] = "找不到API: "+String(Message_CMD_std.c_str());
        String returnString;
        serializeJsonPretty(D_baseInfo, returnString);

        //? 2024/8/28 NodeRed端新需求
        //? 儀器開一條ws專線
        //? 所有 /ws/NodeRed 線上的資訊都會廣播給所有該線上的所有人得知
        if (String(server->url()) == "/ws") {
          client->binary(returnString);
        } else {
          server->binaryAll(returnString);
        }
      }
      D_baseInfo.clear();
    }
  }
}
#endif