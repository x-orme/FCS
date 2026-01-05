#include "input.h"

/**
  * @brief  ADC 값을 읽어 현재 눌린 버튼을 판별한다.
  * @param  adc_value: ADC Channel 8 (PB0)에서 읽은 Raw 값 (0~4095)
  * @retval KeyState: 판별된 버튼 상태 (KEY_NONE ~ KEY_ENTER)
  * @note   저항 분배 방식을 사용하는 5-Way Tact Switch 모듈의 특성에 맞게 임계값을 설정함.
  */
KeyState Input_Scan(uint32_t adc_value) {
    // 1. 눌리지 않음 (Pull-up 상태: 3.3V 근처)
    if (adc_value > 3800) {
        return KEY_NONE;
    }
    
    // 2. LEFT (GND 단락: 0V 근처)
    if (adc_value < 200) {
        return KEY_LEFT;
    }
    
    // 3. UP (약 0.5V: 600~700)
    if (adc_value > 400 && adc_value < 900) {
        return KEY_UP;
    }
    
    // 4. DOWN (약 1.0V: 1300대)
    if (adc_value > 1100 && adc_value < 1600) {
        return KEY_DOWN;
    }
    
    // 5. RIGHT (약 1.6V: 2000대)
    if (adc_value > 1800 && adc_value < 2300) {
        return KEY_RIGHT;
    }
    
    // 6. ENTER (약 2.4V: 2900~3000)
    if (adc_value > 2700 && adc_value < 3200) {
        return KEY_ENTER;
    }
    
    // 예외 처리 (정의되지 않은 중간 전압)
    return KEY_NONE;
}
