#ifndef INC_INPUT_H_
#define INC_INPUT_H_

#include "main.h"
#include "fcs_common.h"

// 입력 처리 함수
KeyState Input_Scan(uint32_t adc_value);
void Input_Read_All(ADC_HandleTypeDef *hadc, uint32_t *dest);

#endif /* INC_INPUT_H_ */
