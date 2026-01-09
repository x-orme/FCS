#include "ui.h"
#include "fcs_math.h" // Added Math Module
#include "flash_ops.h" // Added Flash Logic
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
    sys->user_pos.easting = 333712.0;
    sys->user_pos.northing = 4132894.0;
    sys->user_pos.altitude = 100.0f;
    
    // 사격 제원 기본값
    sys->fire.charge = 1;
    sys->fire.rounds = 1;
    
    // 수정 사격 초기화
    sys->adj.range_m = 0;
    sys->adj.az_mil = 0;
    
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

    // 1. 노브 데이터 처리
    
    // Knob 2: 장약 (1 ~ 7) - Global available? Usually set in Fire Data, but let's keep it global or restricted?
    // Let's keep continuous update for now as it was.
    sys->fire.charge = (uint8_t)((knobs[1] * 7) / 4096) + 1;
    if(sys->fire.charge > 7) sys->fire.charge = 7;

    // Knob 3: 발사탄수 (1 ~ 10)
    sys->fire.rounds = (uint8_t)((knobs[2] * 10) / 4096) + 1;
    if(sys->fire.rounds > 10) sys->fire.rounds = 10;

    // Knob 1: 차폐각 (Moved to WAITING state only)
    if (sys->state == UI_WAITING) {
        static uint32_t smooth_knob0 = 0;
        if (smooth_knob0 == 0) smooth_knob0 = knobs[0]; 
        smooth_knob0 = (smooth_knob0 * 3 + knobs[0]) / 4;
        
        uint16_t temp_angle = (uint16_t)(((smooth_knob0 * 81) / 4096) * 10);
        if (temp_angle > 800) temp_angle = 800;
        sys->mask_angle = temp_angle;
    }

    // [Real-time Recalculation]
    if (sys->state == UI_FIRE_DATA) {
        FCS_Calculate_FireData(sys);
    }

    // 2. 단계별 버튼 처리
    static uint32_t last_act_time = 0;
    
    if (key != KEY_NONE && key != last_key) {
        if (HAL_GetTick() - last_act_time > 200) {
            last_act_time = HAL_GetTick(); 

            // Helper for Digit Modifiers
            static const double pow10[] = { 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0 };
            double mod_val = 1.0;

            // Target Pointer
            UTM_Coord_t *curr_coord = (sys->state == UI_BP_SETTING) ? &sys->user_pos : &sys->tgt_pos;
            
            // Cursor Range Logic
            // BP_SETTING & TARGET_LOCK: 0-1 (Zone/Band), 2-7 (E), 8-14 (N), 15-18 (Alt)
            int max_cursor = 18; // Both allow Altitude Edit

            // Calculate Mod Val based on cursor position
            if (sys->cursor_pos >= 2 && sys->cursor_pos <= 7) {
                mod_val = pow10[7 - sys->cursor_pos];
            } else if (sys->cursor_pos >= 8 && sys->cursor_pos <= 14) {
                mod_val = pow10[14 - sys->cursor_pos];
            } else if (sys->cursor_pos >= 15 && sys->cursor_pos <= 18) {
                mod_val = pow10[18 - sys->cursor_pos]; 
            }
            
            switch (sys->state) {
                case UI_BP_SETTING:
                case UI_TARGET_LOCK:
                    // [좌우] 커서 이동
                    if (key == KEY_RIGHT) {
                        sys->cursor_pos++;
                        if (sys->cursor_pos > max_cursor) sys->cursor_pos = 0;
                    } else if (key == KEY_LEFT) {
                        if (sys->cursor_pos > 0) sys->cursor_pos--;
                        else sys->cursor_pos = max_cursor;
                    }
                    
                    // [상하] 값 변경
                    if (key == KEY_UP || key == KEY_DOWN) {
                        int dir = (key == KEY_UP) ? 1 : -1;
                        
                        if (sys->cursor_pos == 0) { // Zone
                             if (curr_coord->zone == 51) curr_coord->zone = 52;
                             else curr_coord->zone = 51;
                        }
                        else if (sys->cursor_pos == 1) { // Band
                             if (curr_coord->band == 'S') curr_coord->band = 'T';
                             else curr_coord->band = 'S';
                        }
                        else if (sys->cursor_pos <= 14) { // Easting / Northing
                            double *target_val_ptr = (sys->cursor_pos <= 7) ? &curr_coord->easting : &curr_coord->northing;
                            uint32_t val_int = (uint32_t)(*target_val_ptr);
                            uint32_t multiplier = (uint32_t)mod_val;
                            int current_digit = (val_int / multiplier) % 10;
                            int new_digit = (current_digit + dir + 10) % 10;
                            val_int = val_int - (current_digit * multiplier) + (new_digit * multiplier);
                            *target_val_ptr = (double)val_int;
                        } 
                        else if (sys->cursor_pos >= 15) { // Altitude (Common logic)
                             int alt_int = (int)curr_coord->altitude;
                             int multiplier = (int)mod_val;
                             int current_digit = (alt_int / multiplier) % 10;
                             int new_digit = (current_digit + dir + 10) % 10;
                             alt_int = alt_int - (current_digit * multiplier) + (new_digit * multiplier);
                             curr_coord->altitude = (float)alt_int;
                        }
                    }
                    
                    // [State Transition]
                    if (sys->state == UI_BP_SETTING) {
                        if (key == KEY_ENTER) {
                            Flash_Save_BatteryPos(sys); // Auto-Save to Flash
                            sys->state = UI_WAITING;
                        }
                    } else { // UI_TARGET_LOCK
                        if (key == KEY_LEFT) sys->state = UI_WAITING;
                        else if (key == KEY_ENTER) {
                           FCS_Calculate_FireData(sys);
                           sys->state = UI_FIRE_DATA;
                        }
                    }
                    break;
    
                case UI_WAITING:
                    if (key == KEY_LEFT) {
                        sys->state = UI_BP_SETTING;
                        sys->cursor_pos = 0;
                    }
                     else if (key == KEY_ENTER) {
                         // Reset Target Pos to Zero (Safe by default)
                         sys->tgt_pos.zone = 52;
                         sys->tgt_pos.band = 'S';
                         sys->tgt_pos.easting = 0.0;
                         sys->tgt_pos.northing = 0.0;
                         sys->tgt_pos.altitude = 0.0f;
                         
                         sys->state = UI_TARGET_LOCK;
                        sys->cursor_pos = 0;
                    }
                    break;
    
                case UI_FIRE_DATA:
                    if (key == KEY_LEFT) sys->state = UI_WAITING;
                    else if (key == KEY_ENTER) sys->state = UI_ADJUSTMENT;
                    break;
                    
                case UI_ADJUSTMENT:
                    // [상하] 사거리 수정 (+/- 100m)
                    if (key == KEY_UP) sys->adj.range_m += 100;
                    else if (key == KEY_DOWN) sys->adj.range_m -= 100;
                    
                    // [좌우] 편각 수정 (L/R +/- 10mil)
                    // Note: Usually Right is +, Left is - (Standard Correction)
                    if (key == KEY_RIGHT) sys->adj.az_mil += 10;
                    else if (key == KEY_LEFT) sys->adj.az_mil -= 10;
                    
                    // 확인(Enter) 누르면 재계산 후 사격 제원(Fire Order) 복귀
                    if (key == KEY_ENTER) {
                        FCS_Calculate_FireData(sys);
                        sys->state = UI_FIRE_DATA;
                    }
                    break;

                default: break;
            }
        }
    }
    last_key = key;
}

// [화면 그리기]
void UI_Draw(FCS_System_t *sys) {
    char buf[32];
    ssd1306_Fill(0); 

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

            sprintf(buf, "    N:0%07lu", (uint32_t)sys->user_pos.northing);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            // Data (Line 3: A) Y=40 - Aligned with Colon
            sprintf(buf, "    A:%04d m", (int)sys->user_pos.altitude);
            ssd1306_SetCursor(0, 40); 
            ssd1306_WriteString(buf, Font_7x10, White);

            // Action Button
            ssd1306_SetCursor(80, 54);
            ssd1306_WriteString("[SAVE]", Font_6x8, White);

            // Cursor Highlight
            if (sys->cursor_pos == 0) { cx=0; cw=14; cy=27; }
            else if (sys->cursor_pos == 1) { cx=14; cw=7; cy=27; }
            else if (sys->cursor_pos <= 7) { cx=42+(sys->cursor_pos-2)*7; cw=7; cy=27; }
            else if (sys->cursor_pos <= 14) { cx=42+7+(sys->cursor_pos-8)*7; cw=7; cy=39; } // +7px for '0' prefix
            else { cx=42+(sys->cursor_pos-15)*7; cw=7; cy=51; } // Alt Cursor Y=40+11
            
            ssd1306_Line(cx, cy, cx+cw-1, cy, White);
            break;

        case UI_WAITING:
            ssd1306_SetCursor(37, 0);
            ssd1306_WriteString("[STANDBY]", Font_6x8, White);
            
            // Env Data
            sprintf(buf, "T:%d.%d  P:%d", 
                (int)sys->env.air_temp, (int)(sys->env.air_temp * 10)%10, 
                (int)sys->env.air_pressure);
            ssd1306_SetCursor(10, 20);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            // Mask Data (Centered)
            sprintf(buf, "Mask: %03u mil", sys->mask_angle);
            // Center alignment: 128px width. Text is approx 13 chars * 7 = 91px.
            // (128-91)/2 = 18.5 -> x=18
            ssd1306_SetCursor(18, 40); // Moved to Y=40 for better balance
            ssd1306_WriteString(buf, Font_7x10, White);
            break;

        case UI_TARGET_LOCK:
            ssd1306_SetCursor(31, 0);
            ssd1306_WriteString("[MSN CHECK]", Font_6x8, White);
            
            // Target Data (Unified Layout with BP_SETTING)
            sprintf(buf, "%d%c E:%06lu", sys->tgt_pos.zone, sys->tgt_pos.band, (uint32_t)sys->tgt_pos.easting);
            ssd1306_SetCursor(0, 16);
            ssd1306_WriteString(buf, Font_7x10, White);

            sprintf(buf, "    N:0%07lu", (uint32_t)sys->tgt_pos.northing);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            // Altitude Input (Unified)
            sprintf(buf, "    A:%04d m", (int)sys->tgt_pos.altitude);
            ssd1306_SetCursor(0, 40); 
            ssd1306_WriteString(buf, Font_7x10, White);

            // Action Button
            ssd1306_SetCursor(80, 54);
            ssd1306_WriteString("[FIRE]", Font_6x8, White);
            
            // Cursor Highlight
            if (sys->cursor_pos == 0) { cx=0; cw=14; cy=27; }
            else if (sys->cursor_pos == 1) { cx=14; cw=7; cy=27; }
            else if (sys->cursor_pos <= 7) { cx=42+(sys->cursor_pos-2)*7; cw=7; cy=27; }
            else if (sys->cursor_pos <= 14) { cx=42+7+(sys->cursor_pos-8)*7; cw=7; cy=39; } // +7px for '0' prefix
            else { cx=42+(sys->cursor_pos-15)*7; cw=7; cy=51; } // Alt Cursor
            
            ssd1306_Line(cx, cy, cx+cw-1, cy, White);
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
            
            if (sys->fire.elevation < -0.5f) {
                // Error Handling
                ssd1306_SetCursor(18, 48);
                if (sys->fire.elevation > -1.5f) { // -1.0 (RANGE)
                    ssd1306_WriteString("! RANGE ERR !", Font_7x10, White);
                } else { // -2.0 (CHARGE)
                    ssd1306_WriteString("! CHG ERROR !", Font_7x10, White); 
                }
            }
            else if (sys->fire.elevation < sys->mask_angle) {
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
            
            // Deviation (Azimuth) Display
            // Format: "DEV : L  20" (Fixed width alignment)
            char dir_c = ' '; // Default space for 0
            int abs_mil = sys->adj.az_mil;
            if (abs_mil < 0) { dir_c = 'L'; abs_mil = -abs_mil; }
            else if (abs_mil > 0) { dir_c = 'R'; }
            
            // "DEV : " (6 chars) + "C" (1) + "  " (2) + "XXX" (3)
            sprintf(buf, "DEV : %c  %3d", dir_c, abs_mil);
            ssd1306_SetCursor(10, 20);
            ssd1306_WriteString(buf, Font_7x10, White);

            // Range Display
            // Format: "RNG : + 100"
            char sign_c = '+';
            int abs_rng = sys->adj.range_m;
            if (abs_rng < 0) { sign_c = '-'; abs_rng = -abs_rng; }
            else if (abs_rng == 0) { sign_c = ' '; } // Space for zero to align number? Or Keep +0? +0 implies ready to add. Keep + or space. Let's use space for 0.
            if (sys->adj.range_m == 0) sign_c = ' ';

            // "RNG : " (6 chars) + "S" (1) + " " (1) + "XXXX" (4)
            sprintf(buf, "RNG : %c %4d", sign_c, abs_rng);
            ssd1306_SetCursor(10, 35);
            ssd1306_WriteString(buf, Font_7x10, White);

            // Action
            ssd1306_SetCursor(70, 54);
            ssd1306_WriteString("[UPDATE]", Font_6x8, White);
            break;
    }
    
    ssd1306_UpdateScreen(); 
}
