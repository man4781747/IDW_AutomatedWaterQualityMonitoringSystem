#ifndef LX_20S_H
#define LX_20S_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#define GET_LOW_BYTE(A) (uint8_t)((A))
//Macro function  get lower 8 bits of A
#define GET_HIGH_BYTE(A) (uint8_t)((A) >> 8)
//Macro function  get higher 8 bits of A
#define BYTE_TO_HW(A, B) ((((uint16_t)(A)) << 8) | (uint8_t)(B))
//put A as higher 8 bits   B as lower 8 bits   which amalgamated into 16 bits integer
#define LX_20S_SERVO_FRAME_HEADER         0x55
#define LX_20S_SERVO_MOVE_TIME_WRITE      1
#define LX_20S_SERVO_MOVE_TIME_READ       2
#define LX_20S_SERVO_MOVE_TIME_WAIT_WRITE 7
#define LX_20S_SERVO_MOVE_TIME_WAIT_READ  8
#define LX_20S_SERVO_MOVE_START           11
#define LX_20S_SERVO_MOVE_STOP            12
#define LX_20S_SERVO_ID_WRITE             13
#define LX_20S_SERVO_ID_READ              14
#define LX_20S_SERVO_ANGLE_OFFSET_ADJUST  17
#define LX_20S_SERVO_ANGLE_OFFSET_WRITE   18
#define LX_20S_SERVO_ANGLE_OFFSET_READ    19
#define LX_20S_SERVO_ANGLE_LIMIT_WRITE    20
#define LX_20S_SERVO_ANGLE_LIMIT_READ     21
#define LX_20S_SERVO_VIN_LIMIT_WRITE      22
#define LX_20S_SERVO_VIN_LIMIT_READ       23
#define LX_20S_SERVO_TEMP_MAX_LIMIT_WRITE 24
#define LX_20S_SERVO_TEMP_MAX_LIMIT_READ  25
#define LX_20S_SERVO_TEMP_READ            26
#define LX_20S_SERVO_VIN_READ             27
#define LX_20S_SERVO_POS_READ             28
#define LX_20S_SERVO_OR_MOTOR_MODE_WRITE  29
#define LX_20S_SERVO_OR_MOTOR_MODE_READ   30
#define LX_20S_SERVO_LOAD_OR_UNLOAD_WRITE 31
#define LX_20S_SERVO_LOAD_OR_UNLOAD_READ  32
#define LX_20S_SERVO_LED_CTRL_WRITE       33
#define LX_20S_SERVO_LED_CTRL_READ        34
#define LX_20S_SERVO_LED_ERROR_WRITE      35
#define LX_20S_SERVO_LED_ERROR_READ       36

byte LX_20S_CheckSum(byte buf[])
{
  byte i;
  uint16_t temp = 0;
  for (i = 2; i < buf[3] + 2; i++) {
    temp += buf[i];
  }
  temp = ~temp;
  i = (byte)temp;
  return i;
}

void LX_20S_SerialServoSetID(HardwareSerial& SerialX, uint8_t newID)
{

  byte buf[7];
  buf[0] = buf[1] = LX_20S_SERVO_FRAME_HEADER;
  buf[2] = 0xFE;
  buf[3] = 4;
  buf[4] = LX_20S_SERVO_ID_WRITE;
  buf[5] = newID;
  buf[6] = LX_20S_CheckSum(buf);
  SerialX.write(buf, 7);
  
}

int LX_20S_SerialServoReceiveHandle(HardwareSerial& SerialX, byte *ret, int repeatLen)
{
  bool frameStarted = false;
  bool receiveFinished = false;
  byte frameCount = 0;
  byte dataCount = 0;
  byte dataLength = 2;
  byte rxBuf;
  byte recvBuf[32];
  byte i;
  while (SerialX.available()) {
    rxBuf = SerialX.read();
    delayMicroseconds(100);
    if (!frameStarted) {
      if (rxBuf == LX_20S_SERVO_FRAME_HEADER) {
        frameCount++;
        if (frameCount == 2) {
          frameCount = 0;
          frameStarted = true;
          dataCount = 1;
        }
      }
      else {
        frameStarted = false;
        dataCount = 0;
        frameCount = 0;
      }
    }
    if (frameStarted) {
      recvBuf[dataCount] = (uint8_t)rxBuf;
      if (dataCount == 3) {
        dataLength = recvBuf[dataCount];
        if (dataLength < 3 || dataCount > 7) {
          dataLength = 2;
          frameStarted = false;
        }
      }
      dataCount++;
      if (dataCount == dataLength + 3) {
        
#ifdef LX_20S_DEBUG
        Serial.print("RECEIVE DATA:");
        for (i = 0; i < dataCount; i++) {
          Serial.print(recvBuf[i], HEX);
          Serial.print(":");
        }
        Serial.println(" ");
#endif

        if (LX_20S_CheckSum(recvBuf) == recvBuf[dataCount - 1]) {
          
#ifdef LX_20S_DEBUG
          Serial.println("Check SUM OK!!");
          Serial.println("");
#endif

          frameStarted = false;
          memcpy(ret, recvBuf + 4, dataLength);
          return 1;
        }
        return -1;
      }
    }
  }
  return -1;
}

int LX_20S_SerialServoReadPosition(HardwareSerial& SerialX, uint8_t id)
{

  int count = 10000;
  int ret;
  byte buf[6];

  buf[0] = buf[1] = LX_20S_SERVO_FRAME_HEADER;
  buf[2] = id;
  buf[3] = 3;
  buf[4] = LX_20S_SERVO_POS_READ;
  buf[5] = LX_20S_CheckSum(buf);

#ifdef LX_20S_DEBUG
  Serial.println("LX_20S SERVO Pos READ");
  int debug_value_i = 0;
  for (debug_value_i = 0; debug_value_i < buf[3] + 3; debug_value_i++)
  {
    Serial.print(buf[debug_value_i], HEX);
    Serial.print(":");
  }
  Serial.println(" ");
#endif
  while (SerialX.available()) {
    SerialX.read();
  }
  SerialX.write(buf, 6);
  // vTaskDelay(5/portTICK_PERIOD_MS);
  while (!SerialX.available()) {
    count -= 1;
    if (count < 0)
      
      return -2048;
  }
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  while (!SerialX.available()) {
    count -= 1;
    if (count < 0)
      
      return -2048;
  }
  if (LX_20S_SerialServoReceiveHandle(SerialX, buf, 6) > 0)
    ret = (int16_t)BYTE_TO_HW(buf[2], buf[1]);
  else
    ret = -2048;
#ifdef LX_20S_DEBUG
  Serial.println(ret);
#endif
  
  return ret;
}

int LX_20S_SerialServoReadTeampature(HardwareSerial& SerialX, uint8_t id)
{

  int count = 10000;
  int ret;
  byte buf[6];

  buf[0] = buf[1] = LX_20S_SERVO_FRAME_HEADER;
  buf[2] = id;
  buf[3] = 3;
  buf[4] = LX_20S_SERVO_TEMP_READ;
  buf[5] = LX_20S_CheckSum(buf);

#ifdef LX_20S_DEBUG
  Serial.println("LX_20S SERVO TEMP READ");
  int debug_value_i = 0;
  for (debug_value_i = 0; debug_value_i < buf[3] + 3; debug_value_i++)
  {
    Serial.print(buf[debug_value_i], HEX);
    Serial.print(":");
  }
  Serial.println(" ");
#endif
  while (SerialX.available()) {
    SerialX.read();
  }
  SerialX.write(buf, 6);
  // vTaskDelay(5/portTICK_PERIOD_MS);
  while (!SerialX.available()) {
    count -= 1;
    if (count < 0)
      
      return -999;
  }
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  while (!SerialX.available()) {
    count -= 1;
    if (count < 0)
      
      return -2048;
  }
  if (LX_20S_SerialServoReceiveHandle(SerialX, buf, 5) > 0)
    ret = (int)buf[1];
  else
    ret = -999;
// #ifdef LX_20S_DEBUG
//   Serial.println(ret);
// #endif
  
  return ret;
}

int LX_20S_SerialServoReadVIN(HardwareSerial& SerialX, uint8_t id)
{

  int count = 10000;
  int ret;
  byte buf[6];

  buf[0] = buf[1] = LX_20S_SERVO_FRAME_HEADER;
  buf[2] = id;
  buf[3] = 3;
  buf[4] = LX_20S_SERVO_VIN_READ;
  buf[5] = LX_20S_CheckSum(buf);

#ifdef LX_20S_DEBUG
  Serial.println("LX_20S SERVO VIN READ");
  int debug_value_i = 0;
  for (debug_value_i = 0; debug_value_i < buf[3] + 3; debug_value_i++)
  {
    Serial.print(buf[debug_value_i], HEX);
    Serial.print(":");
  }
  Serial.println(" ");
#endif
  while (SerialX.available()) {
    SerialX.read();
  }
  SerialX.write(buf, 6);
  // vTaskDelay(5/portTICK_PERIOD_MS);
  while (!SerialX.available()) {
    count -= 1;
    if (count < 0)
      
      return -999;
  }
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  while (!SerialX.available()) {
    count -= 1;
    if (count < 0)
      
      return -2048;
  }
  if (LX_20S_SerialServoReceiveHandle(SerialX, buf, 5) > 0)
    ret = (int16_t)BYTE_TO_HW(buf[2], buf[1]);
  else
    ret = -999;
// #ifdef LX_20S_DEBUG
//   Serial.println(ret);
// #endif
  
  return ret;
}

int LX_20S_SerialServoReadReadVINLimit(HardwareSerial& SerialX, uint8_t id)
{

  int count = 10000;
  int ret;
  byte buf[10];

  buf[0] = buf[1] = LX_20S_SERVO_FRAME_HEADER;
  buf[2] = id;
  buf[3] = 3;
  buf[4] = LX_20S_SERVO_VIN_LIMIT_READ;
  buf[5] = LX_20S_CheckSum(buf);

#ifdef LX_20S_DEBUG
  Serial.println("LX_20S SERVO VIN LIMIT READ");
  int debug_value_i = 0;
  for (debug_value_i = 0; debug_value_i < buf[3] + 3; debug_value_i++)
  {
    Serial.print(buf[debug_value_i], HEX);
    Serial.print(":");
  }
  Serial.println(" ");
#endif
  while (SerialX.available()) {
    SerialX.read();
  }
  SerialX.write(buf, 6);
  // vTaskDelay(5/portTICK_PERIOD_MS);
  while (!SerialX.available()) {
    count -= 1;
    if (count < 0)
      
      return -999;
  }
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  SerialX.read();
  while (!SerialX.available()) {
    count -= 1;
    if (count < 0)
      
      return -2048;
  }
  if (LX_20S_SerialServoReceiveHandle(SerialX, buf, 7) > 0) {
    Serial.print((int16_t)BYTE_TO_HW(buf[2], buf[1]));
    Serial.print(",");
    Serial.println((int16_t)BYTE_TO_HW(buf[4], buf[3]));
  }
    // ret = (int16_t)BYTE_TO_HW(buf[2], buf[1]);
  else
    ret = -999;
// #ifdef LX_20S_DEBUG
//   Serial.println(ret);
// #endif
  
  return ret;
}


void LX_20S_SerialServoMove(HardwareSerial& SerialX, uint8_t id, int16_t position, uint16_t time)
{
  // ESP_LOGW("LX-20S", "準備控制馬達: %d 在 %d 毫秒內轉動至位置 %d ", id, time, position);
  byte buf[10];
  if(position < 0)
    position = 0;
  if(position > 1000)
    position = 1000;
  buf[0] = buf[1] = LX_20S_SERVO_FRAME_HEADER;
  buf[2] = id;
  buf[3] = 7;
  buf[4] = LX_20S_SERVO_MOVE_TIME_WRITE;
  buf[5] = GET_LOW_BYTE(position);
  buf[6] = GET_HIGH_BYTE(position);
  buf[7] = GET_LOW_BYTE(time);
  buf[8] = GET_HIGH_BYTE(time);
  buf[9] = LX_20S_CheckSum(buf);
  SerialX.write(buf, 10);
  
}

#endif