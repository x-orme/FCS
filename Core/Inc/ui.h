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
typedef struct {
    // 1. 센서 데이터
    BMP280_Data_t sensor;
    
    // 2. 좌표 데이터 (UTM 52S 기준)
    struct {
        // [BP: Battery Position]
        uint32_t bp_easting;   // 6자리 (ex: 123456)
        uint32_t bp_northing;  // 8자리 (ex: 12345678)
        uint16_t mask_angle;   // 차폐각 (0~800 mil)
        
        // [TGT: Target Position]
        uint32_t tgt_easting;
        uint32_t tgt_northing;
        uint16_t tgt_altitude;
        
        // [CORR: Correction]
        int16_t corr_easting;  // 편의 수정량 (L/R) -> 미터 단위 변환 저장 필요하지만 일단 mil단위 관리도 고려
        int16_t corr_range;    // 사거리 수정량 (+/-)
    } coord;

    // 3. 사격 제원 (Fire Solution)
    struct {
        // Inputs
        uint8_t charge;      // 장약 (1~7)
        uint8_t rounds;      // 발사탄수 (1~10)
        
        // Outputs (Calculated)
        uint16_t azimuth;    // 방위각 (mil)
        uint16_t elevation;  // 사각 (mil)
        float    tof;        // 비행시간 (sec)
        float    distance_km; // 표적 거리
    } fire;
    
    // 4. UI 상태 관리
    UI_State state;
    uint8_t  cursor_pos;     // 입력 필드 커서 (0=Easting, 1=Northing, 2=OK)
    uint8_t  sub_step;       // 단계별 내부 상태
    uint32_t last_input_time; // 입력 타임아웃 관리용
    
} FCS_System_t;

// 함수 원형
void UI_Init(FCS_System_t *sys);
void UI_Update(FCS_System_t *sys, KeyState key, uint32_t knobs[3]); // Knob 값 배열 추가
void UI_Draw(FCS_System_t *sys);

#endif /* INC_UI_H_ */
