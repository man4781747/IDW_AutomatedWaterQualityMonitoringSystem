#ifndef CALC_FUNCTION_H
#define CALC_FUNCTION_H

/**
 * @brief 計算平均值
 * 
 * @param x 
 * @param len 
 * @return double 
 */
inline double average(const uint16_t* x, int len)
{
  uint32_t sum = 0;
  for (int i = 0; i < len; i++) {
    sum += x[i];
  }
  double average = static_cast<double>(sum) / len;
  return average;
}

/**
 * @brief 獲得方差
 * 
 * @param x 
 * @param len 
 * @return double 
 */
inline double variance(const uint16_t* x, int len)
{
  double sum = 0;
  double avg = average(x, len);
  for (int i = 0; i < len; i++) {
    double diff = static_cast<double>(x[i]) - avg;
    sum += diff * diff;
  }
  double variance = static_cast<double>(sum) / len;
  return variance;
}

/**
 * @brief 得到標準差
 * 
 * @param x 
 * @param len 
 * @return double 
 */
inline double standardDev(const uint16_t* x, int len)
{
  double var = variance(x, len);
  if (var == 0.) {
    return 0.;
  }
  return sqrt(var);
}

/**
 * @brief 獲得過濾後的平均數值
 * 
 * @param x 
 * @param len 
 * @return double 
 * 
 * @note 計算標準差時，有可能遇到標準差算出來為0的狀況，
 * 此時平均值就為答案
 */
inline double afterFilterValue(const uint16_t* x, int len)
{
  double standard = standardDev(x, len);
  double avg = average(x, len);

  if (standard == 0.) {
    return avg;
  }
  uint32_t sum = 0;
  double sumLen = 0.;

  for (int i = 0; i < len; i++) {
    if (abs((double)x[i]-avg) < standard*2) {
      sum += x[i];
      sumLen += 1;
    }
  }
  return static_cast<double>(sum)/sumLen;
}

inline double getFixValueByLinerFix(double x, double a, double b)
{
  double result;
  result = a*x+b;
  if (isnormal(result)) {
    return result;
  } 
  else {
    return -999.;
  }
}

inline double getFixValueByLogarithmicFix(double x, double a, double logNum,double b)
{
  double result;
  result = a*log(x)/log(logNum)+b;
  if (isnormal(result)) {
    return result;
  } 
  else {
    return -999.;
  }
}


#endif