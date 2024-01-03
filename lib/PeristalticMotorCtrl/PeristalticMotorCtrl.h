#ifndef PeristalticMotorCtrl_H
#define PeristalticMotorCtrl_H

#include <Arduino.h>

enum PeristalticMotorStatus : int {
  FORWARD = 0b10,
  STOP = 0b00,
  REVERSR = 0b01,
};

class C_Peristaltic_Motors_Ctrl
{
  public:
    C_Peristaltic_Motors_Ctrl(void){};
    void INIT_Motors(int SCHP, int STHP, int DATA, int moduleNum);
    int SHCP, STCP, DATA, moduleNum;
    uint8_t *moduleDataList;
    void SetAllMotorStop();
    void RunMotor(uint8_t *moduleDataList);
    void SetMotorStatus(int index, PeristalticMotorStatus status);
    void ShowNowSetting();
    void OpenAllPin();
    bool IsAllStop();
  private:
};

#endif