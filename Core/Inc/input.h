#ifndef INC_INPUT_H_
#define INC_INPUT_H_

#include "main.h"

// 버튼 상태 정의 (ADC 값에 따른 맵핑)
typedef enum {
  KEY_NONE = 0,
  KEY_LEFT,   // ADC < 200 (0V)
  KEY_UP,     // ADC 600~700
  KEY_DOWN,   // ADC 1300~1400
  KEY_RIGHT,  // ADC 2000~2100
  KEY_ENTER   // ADC 2900~3000
} KeyState;

// 입력 처리 함수
KeyState Input_Scan(uint32_t adc_value);

#endif /* INC_INPUT_H_ */
