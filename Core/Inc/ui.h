#ifndef INC_UI_H_
#define INC_UI_H_

#include "main.h"
#include "bmp280.h"
#include "input.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

// UI 화면 단계 (5단계 Design)
typedef enum {
    UI_BOOT = 0,      // 0단계: 부팅 및 자가진단
    UI_BP_SETTING,    // 1단계: 포 진지(BP) 좌표 입력
    UI_WAITING,       // 2단계: 대기 (환경정보 무한루프)
    UI_TARGET_LOCK,   // 3단계: 표적 데이터 수신 및 확인
    UI_FIRE_DATA,     // 4단계: 사격 제원 전시
    UI_ADJUSTMENT     // 5단계: 차후 수정 (편의/사거리)
} UI_State;

// 시스템 전역 데이터 구조체
// 좌표계 구조체 (UTM System)
typedef struct {
    uint8_t zone;       // 51 ~ 52
    char band;          // 'S' or 'T'
    double easting;     // M (Meter)
    double northing;    // M (Meter)
    float altitude;     // M (Meter)
} UTM_Coord_t;

// 사격 제원 구조체
typedef struct {
    float azimuth;      // mil
    float elevation;    // mil
    float distance_km;  // km
    uint8_t charge;     // 장약 호수
    uint8_t rounds;     // 발사 탄수
} Fire_Data_t;

// [Deleted Env_Data_t]

// 통합 시스템 구조체
typedef struct {
    UI_State state;
    
    // Coordinates
    UTM_Coord_t user_pos; // Battery Position
    UTM_Coord_t tgt_pos;  // Target Position
    uint16_t mask_angle;  // mil
    
    // Calculation Result
    Fire_Data_t fire;
    
    // Environment & Correction Factors
    struct {
        float air_temp;     // C (from BMP280)
        float air_pressure; // hPa (from BMP280)
        float wind_speed;   // m/s
        float wind_dir;     // mil (Grid North)
        float prop_temp;    // C (Propellant Temperature)
        float weight_diff;  // projectile weight kg diff (placeholder)
    } env;

    // UI Control
    uint8_t cursor_pos; // Current Cursor
} FCS_System_t;

// 함수 원형
void UI_Init(FCS_System_t *sys);
void UI_Update(FCS_System_t *sys, KeyState key, uint32_t knobs[3]); // Knob 값 배열 추가
void UI_Draw(FCS_System_t *sys);

#endif /* INC_UI_H_ */
