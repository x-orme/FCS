#ifndef __FLASH_OPS_H
#define __FLASH_OPS_H

#include "main.h"
#include "ui.h" // Added to resolve FCS_System_t

// STM32F401RE / F411RE Reference
// Sector 7: 0x08060000 ~ 0x0807FFFF (128KB) - Last Sector for 512KB models
#define FLASH_STORAGE_ADDR 0x08060000 
#define FLASH_MAGIC_CODE   0xFCCF0001

typedef struct {
    uint32_t magic;
    uint8_t  zone;
    uint8_t  band;
    uint8_t  reserved[2]; // Padding
    double   easting;
    double   northing;
    float    altitude;
} Flash_SaveData_t;

// Functions
void Flash_Save_BatteryPos(FCS_System_t *sys);
void Flash_Load_BatteryPos(FCS_System_t *sys);

#endif
