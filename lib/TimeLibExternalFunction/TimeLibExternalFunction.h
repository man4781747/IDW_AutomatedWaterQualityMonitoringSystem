#ifndef TimeLibExternalFunction_H
#define TimeLibExternalFunction_H
#include <Arduino.h>
#include <TimeLib.h>

inline String GetNowTimeString()
{
  char datetimeChar[30];
  time_t nowTime = now();
  sprintf(datetimeChar, "%04d-%02d-%02d %02d:%02d:%02d",
    year(nowTime), month(nowTime), day(nowTime),
    hour(nowTime), minute(nowTime), second(nowTime)
  );

  return String(datetimeChar);
}

inline String GetDateString(String interval, time_t timeShift=0)
{
  time_t nowTime = now();
  char date[11];
  sprintf(date, "%04d%s%02d%s%02d",
    year(nowTime+timeShift), interval.c_str(), month(nowTime+timeShift), interval.c_str(), day(nowTime+timeShift)
  );
  return String(date);
}

inline String GetTimeString(String interval, time_t timeShift=0)
{
  time_t nowTime = now();
  char time_[11];
  sprintf(time_, "%02d%s%02d%s%02d",
    hour(nowTime+timeShift), interval.c_str(), minute(nowTime+timeShift), interval.c_str(), second(nowTime+timeShift)
  );
  return String(time_);
}

inline String GetDatetimeString(String interval_date="-", String interval_middle=" ", String interval_time=":", time_t timeShift=0)
{
  return GetDateString(interval_date, timeShift)+interval_middle+GetTimeString(interval_time, timeShift);
}

#endif