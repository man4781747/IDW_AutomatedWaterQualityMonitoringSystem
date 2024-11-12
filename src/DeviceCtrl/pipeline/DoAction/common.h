#ifndef DoAction_Common_H
#define DoAction_Common_H

#include "../../MAIN__DeviceCtrl.h"
#include <ArduinoJson.h>

enum ResultCode: int {
  SUCCESS = 0,
  STOP_BY_OUTSIDE = 1,
  KEEP_RUN = 2,
  IDEL_EVENT = 3,
  STOP_THIS_STEP = -1,
  STOP_THIS_PIPELINE = -2,
  STOP_DEVICE = -3,
};

struct StepResult {
  ResultCode code;
  String message;
};

struct DoAction
{
  explicit DoAction(JsonObject eventItem_, StepTaskDetail* StepTaskDetailItem_) {
    eventItem = eventItem_;
    StepTaskDetailItem = StepTaskDetailItem_;
  }
  virtual ~DoAction() {}
  virtual void Run() {};
  virtual void StopByOutside() {};
  JsonObject eventItem;
  StepTaskDetail *StepTaskDetailItem;
  ResultCode result_code = ResultCode::IDEL_EVENT;
  String ResultMessage = "Idel";
};
#endif