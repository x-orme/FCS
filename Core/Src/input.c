#include "input.h"

/**
  * @brief  ADC 값을 읽어 현재 눌린 버튼을 판별한다.
  * @param  adc_value: ADC Channel 8 (PB0)에서 읽은 Raw 값 (0~4095)
  * @retval KeyState: 판별된 버튼 상태 (KEY_NONE ~ KEY_ENTER)
  * @note   저항 분배 방식을 사용하는 5-Way Tact Switch 모듈의 특성에 맞게 임계값을 설정함.
  */
// [Input Scan] - With Dead Zones (Safety)
KeyState Input_Scan(uint32_t adc_value) {
    if (adc_value > 3800) return KEY_NONE;
    if (adc_value < 200)  return KEY_LEFT;
    if (adc_value > 400  && adc_value < 900)  return KEY_UP;
    if (adc_value > 1100 && adc_value < 1600) return KEY_DOWN;
    if (adc_value > 1800 && adc_value < 2300) return KEY_RIGHT;
    if (adc_value > 2700 && adc_value < 3200) return KEY_ENTER;
    
    return KEY_NONE;
}
