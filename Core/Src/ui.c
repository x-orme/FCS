#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <math.h> 

// Helper Macros
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// [초기화]
void UI_Init(FCS_System_t *sys) {
    memset(sys, 0, sizeof(FCS_System_t));
    
    sys->state = UI_BOOT;
    
    // 기본값 설정 (Default Value)
    sys->coord.bp_easting = 123456;
    sys->coord.bp_northing = 12345678;
    
    // 사격 제원 기본값 (고폭탄/순발/1C/1R)
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
    // 1. 노브 데이터 처리 (전역 반영)
    // Knob 1: 차폐각 (0 ~ 800)
    sys->coord.mask_angle = (uint16_t)((knobs[0] * 800) / 4096);
    
    // Knob 2: 장약 (1 ~ 7)
    sys->fire.charge = (uint8_t)((knobs[1] * 7) / 4096) + 1;
    if(sys->fire.charge > 7) sys->fire.charge = 7;

    // Knob 3: 발사탄수 (1 ~ 10)
    sys->fire.rounds = (uint8_t)((knobs[2] * 10) / 4096) + 1;
    if(sys->fire.rounds > 10) sys->fire.rounds = 10;


    // 2. 단계별 버튼 처리
    switch (sys->state) {
        case UI_BP_SETTING:
            // [좌우] 커서 이동 (0 ~ 14)
            if (key == KEY_RIGHT) {
                sys->cursor_pos++;
                if (sys->cursor_pos > 14) sys->cursor_pos = 0;
            } else if (key == KEY_LEFT) {
                if (sys->cursor_pos > 0) sys->cursor_pos--;
                else sys->cursor_pos = 14;
            }
            
            // [결정] 다음 단계로
            if (key == KEY_ENTER) {
                sys->state = UI_WAITING;
            }
            break;

        case UI_WAITING:
            // [뒤로가기] 1단계 복귀
            if (key == KEY_LEFT) {
                sys->state = UI_BP_SETTING;
            }
            // [테스트] 엔터 누르면 강제 표적 생성 및 진입
            else if (key == KEY_ENTER) {
                sys->coord.tgt_easting = sys->coord.bp_easting + 500;
                sys->coord.tgt_northing = sys->coord.bp_northing + 1000;
                sys->coord.tgt_altitude = 150;
                sys->state = UI_TARGET_LOCK;
            }
            break;

        case UI_TARGET_LOCK:
            // [뒤로가기] 대기 모드로
            if (key == KEY_LEFT) {
                sys->state = UI_WAITING;
            }
            // [확인] 사격 제원 산출
            else if (key == KEY_ENTER) {
                float dx = (float)(sys->coord.tgt_easting - sys->coord.bp_easting);
                float dy = (float)(sys->coord.tgt_northing - sys->coord.bp_northing);
                sys->fire.distance_km = sqrtf(dx*dx + dy*dy) / 1000.0f;
                
                sys->fire.azimuth = 1200; // 가상의 방위각
                sys->fire.elevation = 300; // 가상의 사각
                
                sys->state = UI_FIRE_DATA;
            }
            break;

        case UI_FIRE_DATA:
            // [뒤로가기] 대기 모드로 복귀 (Standby)
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
    }
}

// [화면 그리기] - 정렬 개선 버전
void UI_Draw(FCS_System_t *sys) {
    char buf[32];
    ssd1306_Fill(0); 

    switch (sys->state) {
        case UI_BOOT:
            // "BOOTING..." (10글자 * 7 = 70px) -> X=(128-70)/2 = 29
            ssd1306_SetCursor(29, 27); 
            ssd1306_WriteString("BOOTING...", Font_7x10, White);
            break;

        case UI_BP_SETTING:
            // Title: [BATTERY POS] (13글자, 6x8) -> 13*6=78px -> X=25
            ssd1306_SetCursor(25, 0);
            ssd1306_WriteString("[BATTERY POS]", Font_6x8, White);

            // Data: Left Margin 5px for inputs
            sprintf(buf, "E: %06lu", sys->coord.bp_easting);
            ssd1306_SetCursor(5, 16);
            ssd1306_WriteString(buf, Font_7x10, White);

            sprintf(buf, "N: %08lu", sys->coord.bp_northing);
            ssd1306_SetCursor(5, 28);
            ssd1306_WriteString(buf, Font_7x10, White);

            // Line
            ssd1306_Line(0, 42, 128, 42, White);

            // Mask (Center)
            sprintf(buf, "Mask: %03u mil", sys->coord.mask_angle);
            // "Mask: 123 mil" (13글자 * 7 = 91px) -> X=18
            ssd1306_SetCursor(18, 48);
            ssd1306_WriteString(buf, Font_7x10, White);
            break;

        case UI_WAITING:
            // Title: [STANDBY] (9글자, 6x8) -> 54px -> X=37
            ssd1306_SetCursor(37, 0);
            ssd1306_WriteString("[STANDBY]", Font_6x8, White);
            
            // Env Data (Center)
            sprintf(buf, "T:%d.%d  P:%d", 
                (int)sys->sensor.temperature, (int)(sys->sensor.temperature * 10)%10, 
                (int)sys->sensor.pressure);
            // T:25.5 P:1013 (12~13글자) -> 84px -> X=22
            ssd1306_SetCursor(10, 25);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            // Status Msg
            // "Waiting Msg..." (14글자, 6x8) -> 84px -> X=22
            ssd1306_SetCursor(22, 50);
            ssd1306_WriteString("Waiting Msg...", Font_6x8, White);
            break;

        case UI_TARGET_LOCK:
            // Title: [MSN CHECK] (11글자, 6x8) -> 66px -> X=31
            ssd1306_SetCursor(31, 0);
            ssd1306_WriteString("[MSN CHECK]", Font_6x8, White);
            
            // Target Data
            sprintf(buf, "TGT E: %06lu", sys->coord.tgt_easting);
            ssd1306_SetCursor(5, 14);
            ssd1306_WriteString(buf, Font_7x10, White);

            sprintf(buf, "TGT N: %08lu", sys->coord.tgt_northing);
            ssd1306_SetCursor(5, 26);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            sprintf(buf, "ALT  : %04u m", sys->coord.tgt_altitude);
            ssd1306_SetCursor(5, 38);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            // Action
            ssd1306_SetCursor(80, 54);
            ssd1306_WriteString("-> OK", Font_6x8, White);
            break;

        case UI_FIRE_DATA:
            // Title: [FIRE ORDER] (12글자, 6x8) -> 72px -> X=28
            ssd1306_SetCursor(28, 0);
            ssd1306_WriteString("[FIRE ORDER]", Font_6x8, White);
            
            // Row 1: CH:X  AM:HE (11글자) -> 77px -> X=25
            sprintf(buf, "CH:%d  AM:HE", sys->fire.charge);
            ssd1306_SetCursor(20, 16);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            // Row 2: FU:Q6 RD:X (10글자) -> 70px -> X=29
            sprintf(buf, "FU:Q6 RD:%d", sys->fire.rounds);
            ssd1306_SetCursor(20, 28);
            ssd1306_WriteString(buf, Font_7x10, White);
            
            // Divider
            ssd1306_Line(10, 42, 118, 42, White);
            
            // Row 3: Result (AZ:1234 QE:300)
            if (sys->fire.elevation < sys->coord.mask_angle) {
                // !MASK ERROR! (12글자) -> 84px -> X=22
                ssd1306_SetCursor(22, 48);
                ssd1306_WriteString("!MASK ERROR!", Font_7x10, White); 
            } else {
                sprintf(buf, "AZ:%04u QE:%03u", sys->fire.azimuth, sys->fire.elevation);
                // AZ:1234 QE:300 (14글자) -> 98px -> X=15
                ssd1306_SetCursor(15, 48);
                ssd1306_WriteString(buf, Font_7x10, White);
            }
            break;

        case UI_ADJUSTMENT:
            // Title: [ADJUST FIRE] (13글자, 6x8) -> 78px -> X=25
            ssd1306_SetCursor(25, 0);
            ssd1306_WriteString("[ADJUST FIRE]", Font_6x8, White);
            
            ssd1306_SetCursor(30, 30);
            ssd1306_WriteString("Coming Soon", Font_6x8, White);
            break;
    }
    
    ssd1306_UpdateScreen(); 
}
