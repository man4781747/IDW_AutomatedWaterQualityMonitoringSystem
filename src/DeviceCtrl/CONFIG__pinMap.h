#ifndef PIN_MAP_H
#define PIN_MAP_H


//! 水位浮球
#define PIN__ADC_POOL_FULL 2
#define PIN__ADC_RO_FULL 1


//! 蠕動馬達
#define PIN__Peristaltic_Motor_SHCP 42
#define PIN__Peristaltic_Motor_SHTP 41
#define PIN__Peristaltic_Motor_DATA 40
#define PIN__EN_Peristaltic_Motor   48

//! 伺服馬達
#define PIN__EN_Servo_Motor   4
#define PIN__Serial_LX_20S_RX 9
#define PIN__Serial_LX_20S_TX 10

//! SD卡
#define PIN__SPI_MOSI  11
#define PIN__SPI_CLK   12
#define PIN__SPI_MISO  13
#define PIN__SD_CS     8

//! PH
#define PIN__EN_PH  7
#define PIN__ADC_PH_IN 15

//! 光度計
#define PIN__EN_BlueSensor  16
#define PIN__EN_GreenSensor 17

//! I2C總線
#define PIN__SCL_1 6
#define PIN__SDA_1 5

#endif