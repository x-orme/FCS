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

// [Internal State] Line Buffer for Parsing
#define RX_BUFF_SIZE 64
static char line_buffer[RX_BUFF_SIZE];
static uint8_t line_indx = 0;

// [1] 초기화 (Initialization)
void FCS_Init_System(FCS_System_t *sys) {
  memset(sys, 0, sizeof(FCS_System_t));
  sys->state = UI_BOOT;
  // Defaults can be set here
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
  
  // 2. Process Knobs (Raw to Values) - Simple assignment or mapping
  // Rank1,2,3 -> Knob 0,1,2
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
  // Density calculation could be done here
}

// [3] Serial/Comm Task
void FCS_UART_RxCallback(UART_HandleTypeDef *huart) {
    // 1. Enqueue Data to Ring Buffer
    uint8_t next_head = (u_head + 1) % RING_SIZE;
    if (next_head != u_tail) { // Check Full
        u_buf[u_head] = rx_byte_latched;
        u_head = next_head;
    }
    // 2. Re-arm Interrupt
    HAL_UART_Receive_IT(huart, &rx_byte_latched, 1);
}

void FCS_Serial_Start(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(huart, &rx_byte_latched, 1);
}

void FCS_Task_Serial(FCS_System_t *sys, UART_HandleTypeDef *huart) {
    while (u_head != u_tail) {
        // Dequeue
        uint8_t ch = u_buf[u_tail];
        u_tail = (u_tail + 1) % RING_SIZE;
        
        // 1. Echo Back
        if (ch == '\r') {
             uint8_t nl[] = "\r\n";
             HAL_UART_Transmit(huart, nl, 2, 10);
        } else {
             HAL_UART_Transmit(huart, &ch, 1, 10);
        }

        // 2. Build Line Buffer
        if (ch == '\n' || ch == '\r') {
             line_buffer[line_indx] = 0; // Null terminate
             if (line_indx > 0) {
                 char resp[64];
                 if (FCS_Process_Command(sys, line_buffer, resp) > 0) {
                     printf("\r\n[CMD] %s\r\n", resp); 
                 } else {
                     printf("\r\n[CMD] Ignored/Error: %s\r\n", line_buffer);
                 }
                 // Reset State
                 line_indx = 0;
             }
         } else {
             if (line_indx < RX_BUFF_SIZE - 1) {
                 line_buffer[line_indx++] = ch;
             } else {
                 line_indx = 0; // Overflow / Ignore
             }
         }
    }
}


// [4] 명령어 처리기 (Verify & Bluetooth Core)
// CMD Format: "TGT:52,S,333712,4132894,100"
int FCS_Process_Command(FCS_System_t *sys, char *cmd, char *resp) {
  
  // 1. Target Input Command
  if (strncmp(cmd, "TGT:", 4) == 0) {
    int z;
    char b;
    double e, n;
    float a;
    
    // Parse: TGT:52,S,123.0,456.0,100.0
    long e_int, n_int;
    int a_int;
    
    // Format: TGT:52,S,333712,4132894,105
    int count = sscanf(cmd + 4, "%d,%c,%ld,%ld,%d", &z, &b, &e_int, &n_int, &a_int);
    
    if (count == 5) {
      // Cast to Double/Float
      e = (double)e_int;
      n = (double)n_int;
      a = (float)a_int;
      
      // Update System
      FCS_Set_Target(sys, z, b, e, n, a);
      
      // Calculate Ballistics Immediately
      FCS_Calculate_FireData(sys);
      
      // Generate Response (Validation Output)
      int az_i = (int)sys->fire.azimuth;
      int az_d = (int)((sys->fire.azimuth - az_i) * 10); if(az_d<0) az_d = -az_d;
      
      int el_i = (int)sys->fire.elevation;
      int el_d = (int)((sys->fire.elevation - el_i) * 10); if(el_d<0) el_d = -el_d;
      
      int maz_i = (int)sys->fire.map_azimuth;
      int maz_d = (int)((sys->fire.map_azimuth - maz_i) * 10); if(maz_d<0) maz_d = -maz_d;
      
      sprintf(resp, "OK:AZ%d.%d,EL%d.%d,D%d|MAZ%d.%d,MD%d,VI%d", 
              az_i, az_d, el_i, el_d, (int)(sys->fire.distance_km * 1000),
              maz_i, maz_d, (int)sys->fire.map_distance, (int)sys->fire.height_diff);
      
      // Update UI State Context
      sys->state = UI_FIRE_DATA; 
      
      return 1; // Success
    } else {
      sprintf(resp, "ERR:ParseFail(%d)", count);
      return -1;
    }
  }
  
  return 0; // Unknown Command
}
