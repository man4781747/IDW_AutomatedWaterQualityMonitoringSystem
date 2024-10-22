#ifndef ExternalFunction_H
#define ExternalFunction_H
#include <Arduino.h>

inline void splitString(String data, char delimiter, String result[], int &count) {
  int startIndex = 0;
  int endIndex = 0;
  count = 0;

  // 遍歷整個字符串，找出分隔符的位置
  while ((endIndex = data.indexOf(delimiter, startIndex)) >= 0) {
    result[count++] = data.substring(startIndex, endIndex);
    startIndex = endIndex + 1;
  }

  // 最後一段字符串
  result[count++] = data.substring(startIndex);
}

#endif