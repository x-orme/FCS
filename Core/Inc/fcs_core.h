#ifndef __FCS_CORE_H
#define __FCS_CORE_H

#include "fcs_common.h"

// Core API Functions
void FCS_Init_System(FCS_System_t *sys);
void FCS_Set_Battery(FCS_System_t *sys, int zone, char band, double e, double n, float alt);
void FCS_Set_Target(FCS_System_t *sys, int zone, char band, double e, double n, float alt);

// Command Parser for Serial/Bluetooth
// Returns: 1 if handled, 0 if ignored, -1 if parsing error
int FCS_Process_Command(FCS_System_t *sys, char *cmd_buffer, char *response_buffer);

#endif
