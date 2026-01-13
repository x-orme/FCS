#ifndef __FCS_COMMON_H
#define __FCS_COMMON_H

#include <stdint.h>

// 1. 공용 열거형 (Enum)
typedef enum {
  UI_BOOT,
  UI_BP_SETTING,   // 포 위치 설정
  UI_WAITING,      // 대기 & 환경 센서 표시
  UI_TARGET_LOCK,  // 표적 위치 입력
  UI_FIRE_DATA,    // 사격 제원 표시
  UI_ADJUSTMENT    // 수정 사격 (추후 구현)
} UI_State_t;

typedef enum {
  KEY_NONE = 0,
  KEY_LEFT,   // ADC < 200 (0V)
  KEY_UP,     // ADC 600~700
  KEY_DOWN,   // ADC 1300~1400
  KEY_RIGHT,  // ADC 2000~2100
  KEY_ENTER   // ADC 2900~3000
} KeyState;

// 2. 공용 구조체 (Struct)
typedef struct {
  int zone;
  char band;
  double easting;
  double northing;
  float altitude; // float for altitude is sufficient
} UTM_Coord_t;

typedef struct {
  // Final Firing Data
  float azimuth;      // mil
  float elevation;    // mil
  int charge;         // 장약 (1-7호)
  float distance_km;  // 사거리 (km)
  float time_of_flight; // 비행시간 (초)
  int rounds;         // 발사 탄수

  // [Validation Data] (Intermediate Values)
  float map_azimuth;  // 도상 방위각 (mil)
  float map_distance; // 도상 사거리 (km/m)
  float height_diff;  // 수직 간격 (m)
} FireData_t;

typedef struct {
  float air_temp;     // 기온 (C)
  float air_pressure; // 기압 (hPa) - used for density calc
  float wind_speed;   // 풍속 (m/s)
  float wind_dir;     // 풍향 (mil)
  float prop_temp;    // C (Propellant Temperature) (Added back)
  float weight_diff;  // projectile weight kg diff (placeholder) (Added back)
} EnvData_t;

typedef struct {
  uint32_t adc_raw[4]; // [0,1,2]=Knobs, [3]=Key
  KeyState key_state;
  uint32_t knob_values[3]; // Processed knob values (if needed, or use adc_raw)
} InputData_t;

typedef struct {
  // Core Data
  UTM_Coord_t user_pos;
  UTM_Coord_t tgt_pos;
  
  FireData_t fire;
  EnvData_t env;
  InputData_t input; // Added Input State
  
  // System State
  UI_State_t state;
  uint8_t cursor_pos; // UI Navigation Cursor
  uint16_t mask_angle; // Safety Mask
  
  // Adjustment Data
  struct {
    int16_t range_m;  // Range Correction (+/- m)
    int16_t az_mil;   // Azimuth Deviation (+R/-L mil)
  } adj;
  
} FCS_System_t;

#endif // __FCS_COMMON_H
