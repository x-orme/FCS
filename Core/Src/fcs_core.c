#include "fcs_core.h"
#include "fcs_math.h"
#include "bmp280.h"
#include "input.h"
#include "ui.h" // For UI Context if needed, but mainly for State Enums
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// [Internal State] Serial Ring Buffer
#define RING_SIZE 128
static uint8_t u_buf[RING_SIZE];
static volatile uint8_t u_head = 0;
static volatile uint8_t u_tail = 0;
static uint8_t rx_byte_latched; // Temp holder for ISR
static volatile uint32_t u_overflow_cnt = 0;

// [1] 초기화 (Initialization)
void FCS_Init_System(FCS_System_t *sys) {
  memset(sys, 0, sizeof(FCS_System_t));
  sys->state = UI_BOOT;
  sys->env.prop_temp = 21.0f;
}

void FCS_Set_Battery(FCS_System_t *sys, int zone, char band, double e, double n, float alt) {
  sys->user_pos.zone = zone;
  sys->user_pos.band = band;
  sys->user_pos.easting = e;
  sys->user_pos.northing = n;
  sys->user_pos.altitude = alt;
}

void FCS_Set_Target(FCS_System_t *sys, int zone, char band, double e, double n, float alt) {
  sys->tgt_pos.zone = zone;
  sys->tgt_pos.band = band;
  sys->tgt_pos.easting = e;
  sys->tgt_pos.northing = n;
  sys->tgt_pos.altitude = alt;
}

// [2] Main Update Tasks (Called from main loop)
void FCS_Update_Input(FCS_System_t *sys, ADC_HandleTypeDef *hadc) {
  // 1. Hardware Poll
  Input_Read_All(hadc, sys->input.adc_raw);
  
  // 2. Process Knobs
  sys->input.knob_values[0] = sys->input.adc_raw[0];
  sys->input.knob_values[1] = sys->input.adc_raw[1];
  sys->input.knob_values[2] = sys->input.adc_raw[2];
  
  // 3. Process Key
  sys->input.key_state = Input_Scan(sys->input.adc_raw[3]);
}

void FCS_Update_Sensors(FCS_System_t *sys) {
  BMP280_Data_t bmp_tmp;
  BMP280_Read_All(&bmp_tmp);
  sys->env.air_temp = bmp_tmp.temperature;
  sys->env.air_pressure = bmp_tmp.pressure;
}

// [3] Serial/Comm Task Helper
void FCS_UART_RxCallback(UART_HandleTypeDef *huart) {
    uint8_t next_head = (u_head + 1) % RING_SIZE;
    if (next_head != u_tail) {
        u_buf[u_head] = rx_byte_latched;
        u_head = next_head;
    } else {
        u_overflow_cnt++;
    }
    HAL_UART_Receive_IT(huart, &rx_byte_latched, 1);
}

void FCS_Serial_Start(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(huart, &rx_byte_latched, 1);
}

uint32_t FCS_Serial_GetOverflowCount(void) {
    return u_overflow_cnt;
}

// [Internal State] Parser Machine
typedef enum {
  P_IDLE,
  P_CMD,
  P_SALT,
  P_LEN,
  P_PAYLOAD,
  P_CRC,
  P_ETX
} ParserState_t;

static ParserState_t p_state = P_IDLE;
static uint8_t p_cmd = 0;
static uint8_t p_salt = 0;
static uint8_t p_len = 0;
static uint8_t p_idx = 0;
static uint8_t p_payload[128]; // Max Payload
static uint8_t p_crc_recv = 0;
static uint32_t p_last_rx_tick = 0;
#define PARSER_TIMEOUT_MS   500
#define PROTO_MAX_PAYLOAD   120
#define UART_TX_TIMEOUT_MS  100

// Simple CRC8 (Polynomial 0x07)
static uint8_t Calc_CRC8(uint8_t *data, int len, uint8_t initial) {
  uint8_t crc = initial;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x07;
      else crc <<= 1;
    }
  }
  return crc;
}

// [3] Serial/Comm Task (State Machine Parser)
void FCS_Task_Serial(FCS_System_t *sys, UART_HandleTypeDef *huart) {
  // Reset parser if incomplete frame stalls > 500ms
  if (p_state != P_IDLE && (HAL_GetTick() - p_last_rx_tick > PARSER_TIMEOUT_MS)) {
    p_state = P_IDLE;
  }

  while (u_head != u_tail) {
    // Dequeue byte
    uint8_t rx = u_buf[u_tail];
    u_tail = (u_tail + 1) % RING_SIZE;
    p_last_rx_tick = HAL_GetTick();
        
    // Protocol State Machine
    switch (p_state) {
      case P_IDLE:
        if (rx == FCS_PROTO_STX) {
          p_state = P_CMD;
        }
        break;
                
      case P_CMD:
        p_cmd = rx;
        p_state = P_SALT;
        break;
                
      case P_SALT:
        p_salt = rx;
        p_state = P_LEN;
        break;
                
      case P_LEN:
        p_len = rx;
        p_idx = 0;
        if (p_len > PROTO_MAX_PAYLOAD) p_state = P_IDLE; // Safety Limit
        else if (p_len == 0) p_state = P_CRC; // Empty Payload
        else p_state = P_PAYLOAD;
        break;
                
      case P_PAYLOAD:
        p_payload[p_idx++] = rx;
        if (p_idx >= p_len) p_state = P_CRC;
        break;
                
      case P_CRC:
        p_crc_recv = rx;
        p_state = P_ETX;
        break;
                
      case P_ETX:
        if (rx == FCS_PROTO_ETX) {
          // 1. Verify CRC
          // CRC covers: CMD + SALT + LEN + PAYLOAD
          uint8_t header[3] = {p_cmd, p_salt, p_len};
          uint8_t cal_crc = Calc_CRC8(header, 3, 0);
          cal_crc = Calc_CRC8(p_payload, p_len, cal_crc);
                    
          char tx_buf[FCS_TX_BUF_SIZE];
                    
          if (cal_crc == p_crc_recv) {
            // 2. Decrypt Payload
            uint8_t session_key = FCS_PROTO_KEY ^ p_salt;
            for(int i=0; i<p_len; i++) {
              p_payload[i] ^= (uint8_t)(session_key + i);
            }
            p_payload[p_len] = 0; // Null Terminate
                        
            // 3. Process Command
            char resp[FCS_RESP_BUF_SIZE];
            if (p_cmd == FCS_CMD_TARGET_INPUT) {
              FCS_Process_Command(sys, (char*)p_payload, resp);
            } 
            else if (p_cmd == FCS_CMD_STATUS_REQ) {
              snprintf(resp, FCS_RESP_BUF_SIZE, "STATUS:READY,Z%d", sys->user_pos.zone);
            }
                        
            // 4. Send Response (Via the connected UART)
            snprintf(tx_buf, sizeof(tx_buf), "\r\n[ACK] %s\r\n", resp);
            HAL_UART_Transmit(huart, (uint8_t*)tx_buf, strlen(tx_buf), UART_TX_TIMEOUT_MS);
                        
          } else {
            // CRC Error Response
            snprintf(tx_buf, sizeof(tx_buf), "\r\n[ERR] CRC Fail\r\n");
            HAL_UART_Transmit(huart, (uint8_t*)tx_buf, strlen(tx_buf), UART_TX_TIMEOUT_MS);
          }
        }
        p_state = P_IDLE;
        break;
      }
    }
}


// [4] 명령어 처리기 (Logic Core)
// Now accepts raw payload string: "52,S,333712,4132894,100" (from 0xA1)
// Or "TGT:..." (legacy, removed in theory but kept logic structure)
int FCS_Process_Command(FCS_System_t *sys, char *cmd, char *resp) {
  
  // 1. Target Input Command Logics
  // Check if it's legacy "TGT:" or raw params?
  // Since this is internal call now, we assume it gets the parameter string directly.
  
  int z;
  char b;
  long e_int, n_int;
  int a_int;
    
  // Try Parsing: "52,S,333712,4132894,105"
  int count = sscanf(cmd, "%d,%c,%ld,%ld,%d", &z, &b, &e_int, &n_int, &a_int);
    
  // If failed, maybe it still has "TGT:" prefix (Testing)?
  if (count != 5) {
    count = sscanf(cmd, "TGT:%d,%c,%ld,%ld,%d", &z, &b, &e_int, &n_int, &a_int);
  }
    
  if (count == 5) {
    double e = (double)e_int;
    double n = (double)n_int;
    float a = (float)a_int;
      
    // Update System
    FCS_Set_Target(sys, z, b, e, n, a);
      
    // Calculate Ballistics Immediately
    FCS_Calculate_FireData(sys);
      
    // Generate Response (Validation Output)
    int az_i = (int)sys->fire.azimuth;
    int az_d = (int)((sys->fire.azimuth - az_i) * 10); if(az_d<0) az_d = -az_d;
      
    int el_i = (int)sys->fire.elevation;
    int el_d = (int)((sys->fire.elevation - el_i) * 10); if(el_d<0) el_d = -el_d;
      
    snprintf(resp, FCS_RESP_BUF_SIZE, "AZ:%d.%d EL:%d.%d", az_i, az_d, el_i, el_d);
      
    // Update UI State Context
    sys->state = UI_FIRE_DATA; 
      
    return 1; // Success
  } else {
    snprintf(resp, FCS_RESP_BUF_SIZE, "ERR:Parse(%d)", count);
    return -1;
  }
}
