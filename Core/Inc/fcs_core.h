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

// Command Parser for Serial/Bluetooth
// Returns: 1 if handled, 0 if ignored, -1 if parsing error
int FCS_Process_Command(FCS_System_t *sys, char *cmd_buffer, char *response_buffer);

#endif
