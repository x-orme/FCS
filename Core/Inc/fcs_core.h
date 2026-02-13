#ifndef __FCS_CORE_H
#define __FCS_CORE_H

#include "fcs_common.h"

#include "main.h" // For Handles

// Core API Functions
void FCS_Init_System(FCS_System_t *sys);
void FCS_Set_Battery(FCS_System_t *sys, int zone, char band, double e, double n, float alt);
void FCS_Set_Target(FCS_System_t *sys, int zone, char band, double e, double n, float alt);

// [New] Encapsulated Business Logic Tasks
void FCS_Update_Input(FCS_System_t *sys, ADC_HandleTypeDef *hadc);
void FCS_Update_Sensors(FCS_System_t *sys);
void FCS_Task_Serial(FCS_System_t *sys, UART_HandleTypeDef *huart);

// [New] ISR Interface
void FCS_UART_RxCallback(UART_HandleTypeDef *huart);
void FCS_Serial_Start(UART_HandleTypeDef *huart);
uint32_t FCS_Serial_GetOverflowCount(void);

// [Buffer Size Constants]
#define FCS_RESP_BUF_SIZE  64
#define FCS_TX_BUF_SIZE    128

// [Protocol Definitions]
#define FCS_PROTO_STX  0x02
#define FCS_PROTO_ETX  0x03
#define FCS_PROTO_KEY  0xA5

#define FCS_CMD_TARGET_INPUT 0xA1 // Payload: "52,S,E,N,Alt"
#define FCS_CMD_FIRE_RESULT  0xB1 // Payload: "AZ..,EL.."
#define FCS_CMD_STATUS_REQ   0xC1 // Payload: None
#define FCS_CMD_STATUS_ACK   0xC2 // Payload: "READY"

// Command Parser for Serial/Bluetooth
// Returns: 1 if handled, 0 if ignored, -1 if parsing error
int FCS_Process_Command(FCS_System_t *sys, char *cmd_buffer, char *response_buffer);

#endif
