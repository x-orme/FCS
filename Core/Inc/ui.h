#ifndef INC_UI_H_
#define INC_UI_H_

#include "main.h"
#include "bmp280.h"
#include "input.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "fcs_common.h" // Shared Types

// 함수 원형
void UI_Init(FCS_System_t *sys);
// knobs array size explicit or pointer
void UI_Update(FCS_System_t *sys, KeyState key, uint32_t knobs[3]); 
void UI_Draw(FCS_System_t *sys);

#endif /* INC_UI_H_ */
