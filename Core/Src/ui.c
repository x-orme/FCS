#include "ui.h"
#include "fcs_math.h" // Added Math Module
#include <stdio.h>
#include <string.h>
#include <math.h> 

// Helper Macros
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// [초기화]
void UI_Init(FCS_System_t *sys) {
    memset(sys, 0, sizeof(FCS_System_t));
    
    sys->state = UI_BOOT;
    
    // 기본값 설정 (Korea Default: Zone 52S)
    sys->user_pos.zone = 52;
    sys->user_pos.band = 'S';
    sys->user_pos.easting = 321000.0;
    sys->user_pos.northing = 4150000.0;
    sys->user_pos.altitude = 100.0f;
    
    // 사격 제원 기본값
    sys->fire.charge = 1;
    sys->fire.rounds = 1;
    
    // 초기화 메시지
    ssd1306_Fill(0);
    ssd1306_SetCursor(20, 20); // Center Aligned
    ssd1306_WriteString("SYSTEM INIT...", Font_7x10, White);
    ssd1306_UpdateScreen();
    HAL_Delay(1000);
    
    // 바로 1단계 진입
    sys->state = UI_BP_SETTING;
    sys->cursor_pos = 0; 
}

// [입력 처리]
void UI_Update(FCS_System_t *sys, KeyState key, uint32_t knobs[3]) {
    
    static KeyState last_key = KEY_NONE;

    // 1. 노브 데이터 처리 (전역 반영 - Continuous)
    
    // Knob 1: 차폐각 (0 ~ 800) with Low Pass Filter
    static uint32_t smooth_knob0 = 0;
    if (smooth_knob0 == 0) smooth_knob0 = knobs[0]; // Init
    
    // LPF: (Old*3 + New*1)/4 -> Faster response
    smooth_knob0 = (smooth_knob0 * 3 + knobs[0]) / 4;
    
    // 10 mil Resolution (Use 81 to ensure max reaches 80)
    uint16_t temp_angle = (uint16_t)(((smooth_knob0 * 81) / 4096) * 10);
    if (temp_angle > 800) temp_angle = 800;
    sys->mask_angle = temp_angle;
    
    // Knob 2: 장약 (1 ~ 7)
    sys->fire.charge = (uint8_t)((knobs[1] * 7) / 4096) + 1;
    if(sys->fire.charge > 7) sys->fire.charge = 7;

    // Knob 3: 발사탄수 (1 ~ 10)
    sys->fire.rounds = (uint8_t)((knobs[2] * 10) / 4096) + 1;
    if(sys->fire.rounds > 10) sys->fire.rounds = 10;

    // 2. 단계별 버튼 처리 (One-Shot Edge Detection + De-bounce Cooldown)
    // 키가 눌려있고, 이전 상태와 다를 때만 실행 (Hold 방지)
    static uint32_t last_act_time = 0;
    
    if (key != KEY_NONE && key != last_key) {
        // [Cooldown Check] Button Chattering Filter (200ms)
        if (HAL_GetTick() - last_act_time > 200) {
            last_act_time = HAL_GetTick(); // Update Action Time

            // Helper to get modifier value (Integer Lookup Table for Stability)
        // 10^0 ~ 10^7
        static const double pow10[] = {
            1.0, 10.0, 100.0, 1000.0, 10000.0, 
            100000.0, 1000000.0, 10000000.0 
        };
        
        double mod_val = 1.0;
        if (sys->cursor_pos >= 2 && sys->cursor_pos <= 7) {
            mod_val = pow10[7 - sys->cursor_pos];
        } else if (sys->cursor_pos >= 8 && sys->cursor_pos <= 14) {
            mod_val = pow10[14 - sys->cursor_pos];
        }
    
            // Target Pointer Setup
            UTM_Coord_t *curr_coord = (sys->state == UI_BP_SETTING) ? &sys->user_pos : &sys->tgt_pos;
            
            switch (sys->state) {
                case UI_BP_SETTING:
                case UI_TARGET_LOCK: // Share Input Logic
                    // [좌우] 커서 이동 (0 ~ 14) (Zone, Band, E(6), N(7))
                    if (key == KEY_RIGHT) {
                        sys->cursor_pos++;
                        if (sys->cursor_pos > 14) sys->cursor_pos = 0;
                    } else if (key == KEY_LEFT) {
                        if (sys->cursor_pos > 0) sys->cursor_pos--;
                        else sys->cursor_pos = 14;
                    }
                    
                    // [상하] 값 변경
                    if (key == KEY_UP || key == KEY_DOWN) {
                        int dir = (key == KEY_UP) ? 1 : -1;
                        
                        if (sys->cursor_pos == 0) { // Zone (51/52)
                            if (curr_coord->zone == 51) curr_coord->zone = 52;
                            else curr_coord->zone = 51;
                        }
                        else if (sys->cursor_pos == 1) { // Band (S/T)
                            if (curr_coord->band == 'S') curr_coord->band = 'T';
                            else curr_coord->band = 'S';
                        }
                        else { 
                            // [Digit Independent Logic] 0->9 Wrap, No Carry/Borrow
                            double *target_val_ptr;
                            if (sys->cursor_pos <= 7) target_val_ptr = &curr_coord->easting;
                            else target_val_ptr = &curr_coord->northing;
                            
                            // Get current full value
                            uint32_t val_int = (uint32_t)(*target_val_ptr);
                            uint32_t multiplier = (uint32_t)mod_val;
                            
                            // Extract specific digit (e.g., 3 from 12345 with mul=100)
                            int current_digit = (val_int / multiplier) % 10;
                            
                            // Wrap around (0-9)
                            int new_digit = (current_digit + dir + 10) % 10;
                            
                            // Apply Difference back to main value
                            // Remove old digit value and add new digit value
                            val_int = val_int - (current_digit * multiplier) + (new_digit * multiplier);
                            
                            *target_val_ptr = (double)val_int;
                        }
                    }
                    
                    // [결정/뒤로가기] 상태 전환
                    if (sys->state == UI_BP_SETTING) {
                        if (key == KEY_ENTER) {
                            sys->state = UI_WAITING;
                        }
                    } else { // UI_TARGET_LOCK
                        if (key == KEY_LEFT) {
                            sys->state = UI_WAITING;
                        }
                        else if (key == KEY_ENTER) {
                           FCS_Calculate_FireData(sys);
                           sys->state = UI_FIRE_DATA;
                        }
                    }
                    break; // Break for case UI_BP/TARGET
    
                case UI_WAITING:
                    // [뒤로가기] 1단계 복귀
                    if (key == KEY_LEFT) {
                        sys->state = UI_BP_SETTING;
                        sys->cursor_pos = 0; // Reset Cursor
                    }
                    // [테스트] 엔터 -> 표적 모드 진입
                    else if (key == KEY_ENTER) {
                        // If Tgt is 0, Copy from Battery for convenience
                        if (sys->tgt_pos.easting < 1.0) {
                            sys->tgt_pos = sys->user_pos;
                            sys->tgt_pos.easting += 500; // Offset Default
                            sys->tgt_pos.northing += 1000;
                        }
                        sys->state = UI_TARGET_LOCK;
                        sys->cursor_pos = 0; // Reset Cursor
                    }
                    break;
    
                case UI_FIRE_DATA:
                    // [뒤로가기] 대기 모드로 복귀
                    if (key == KEY_LEFT) {
                        sys->state = UI_WAITING;
                    }
                    // [결정] 차후 수정 단계로
                    else if (key == KEY_ENTER) {
                        sys->state = UI_ADJUSTMENT;
                    }
                    break;
                    
                case UI_ADJUSTMENT:
                    // 확인(Enter) 누르면 사격 제원(Fire Order) 복귀
                    if (key == KEY_ENTER) {
                        sys->state = UI_FIRE_DATA;
                    }
                    break;
                    
                default:
                    break;
            } // switch end
        } // Cooldown End
    } // if key end
    
    // Update History
    last_key = key;
}

// [화면 그리기]
void UI_Draw(FCS_System_t *sys) {
    char buf[32];
    ssd1306_Fill(0); 

    // Cursor Draw Helper Variables
    int cx = 0, cw = 0, cy = 0;
    
    switch (sys->state) {
        case UI_BOOT:
            ssd1306_SetCursor(29, 27); 
            ssd1306_WriteString("BOOTING...", Font_7x10, White);
            break;

        case UI_BP_SETTING:
            // Title
            ssd1306_SetCursor(25, 0);
            ssd1306_WriteString("[BATTERY POS]", Font_6x8, White);

            // Data
            sprintf(buf, "%d%c E:%06lu", sys->user_pos.zone, sys->user_pos.band, (uint32_t)sys->user_pos.easting);
            ssd1306_SetCursor(0, 16);
            ssd1306_WriteString(buf, Font_7x10, White);

            sprintf(buf, "    N:%07lu", (uint32_t)sys->user_pos.northing);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString(buf, Font_7x10, White);

            // Cursor Highlight
            if (sys->cursor_pos == 0) { cx=0; cw=14; cy=27; }
            else if (sys->cursor_pos == 1) { cx=14; cw=7; cy=27; }
            else if (sys->cursor_pos <= 7) { cx=42+(sys->cursor_pos-2)*7; cw=7; cy=27; }
            else { cx=42+(sys->cursor_pos-8)*7; cw=7; cy=39; }
            ssd1306_Line(cx, cy, cx+cw-1, cy, White);

            // Line
            ssd1306_Line(0, 42, 128, 42, White);

            // Mask
            sprintf(buf, "Mask: %03u mil", sys->mask_angle);
            ssd1306_SetCursor(18, 48);
            ssd1306_WriteString(buf, Font_7x10, White);
            break;

        case UI_WAITING:
            ssd1306_SetCursor(37, 0);
            ssd1306_WriteString("[STANDBY]", Font_6x8, White);
            
            // Env Data
            sprintf(buf, "T:%d.%d  P:%d", 
                (int)sys->sensor.temperature, (int)(sys->sensor.temperature * 10)%10, 
                (int)sys->sensor.pressure);
            ssd1306_SetCursor(10, 25);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            ssd1306_SetCursor(22, 50);
            ssd1306_WriteString("Waiting Msg...", Font_6x8, White);
            break;

        case UI_TARGET_LOCK:
            ssd1306_SetCursor(31, 0);
            ssd1306_WriteString("[MSN CHECK]", Font_6x8, White);
            
            // Target Data (Unified Layout with BP_SETTING for Input consistency)
            sprintf(buf, "%d%c E:%06lu", sys->tgt_pos.zone, sys->tgt_pos.band, (uint32_t)sys->tgt_pos.easting);
            ssd1306_SetCursor(0, 16); // Match Y=16
            ssd1306_WriteString(buf, Font_7x10, White);

            sprintf(buf, "    N:%07lu", (uint32_t)sys->tgt_pos.northing);
            ssd1306_SetCursor(0, 28); // Match Y=28
            ssd1306_WriteString(buf, Font_7x10, White);
            
            // Cursor Highlight (Same logic)
            if (sys->cursor_pos == 0) { cx=0; cw=14; cy=27; }
            else if (sys->cursor_pos == 1) { cx=14; cw=7; cy=27; }
            else if (sys->cursor_pos <= 7) { cx=42+(sys->cursor_pos-2)*7; cw=7; cy=27; }
            else { cx=42+(sys->cursor_pos-8)*7; cw=7; cy=39; }
            ssd1306_Line(cx, cy, cx+cw-1, cy, White);
            
            // ALT Info (Moved down)
            sprintf(buf, "ALT  : %04d m", (int)sys->tgt_pos.altitude);
            ssd1306_SetCursor(5, 42); // Moved to Y=42
            ssd1306_WriteString(buf, Font_6x8, White); // Smaller font to fit
            
            ssd1306_SetCursor(80, 54);
            ssd1306_WriteString("-> OK", Font_6x8, White);
            break;

        case UI_FIRE_DATA:
            ssd1306_SetCursor(28, 0);
            ssd1306_WriteString("[FIRE ORDER]", Font_6x8, White);
            
            sprintf(buf, "CH:%d  AM:HE", sys->fire.charge);
            ssd1306_SetCursor(20, 16);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            sprintf(buf, "FU:Q6 RD:%d", sys->fire.rounds);
            ssd1306_SetCursor(20, 28);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            ssd1306_Line(10, 42, 118, 42, White);
            
            if (sys->fire.elevation < sys->mask_angle) {
                ssd1306_SetCursor(22, 48);
                ssd1306_WriteString("!MASK ERROR!", Font_7x10, White); 
            } else {
                sprintf(buf, "AZ:%04d QE:%03d", (int)sys->fire.azimuth, (int)sys->fire.elevation);
                ssd1306_SetCursor(15, 48);
                ssd1306_WriteString(buf, Font_7x10, White);
            }
            break;

        case UI_ADJUSTMENT:
            ssd1306_SetCursor(25, 0);
            ssd1306_WriteString("[ADJUST FIRE]", Font_6x8, White);
            
            ssd1306_SetCursor(30, 30);
            ssd1306_WriteString("Coming Soon", Font_6x8, White);
            break;
    }
    
    ssd1306_UpdateScreen(); 
}
