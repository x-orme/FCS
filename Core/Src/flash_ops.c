#include "flash_ops.h"
#include <string.h>
#include <stdio.h>

// [1] Load Data from Flash
void Flash_Load_BatteryPos(FCS_System_t *sys) {
  Flash_SaveData_t *data = (Flash_SaveData_t*)FLASH_STORAGE_ADDR;
    
  // Check Validity
  if (data->magic == FLASH_MAGIC_CODE) {
    sys->user_pos.zone = data->zone;
    sys->user_pos.band = data->band;
    sys->user_pos.easting = data->easting;
    sys->user_pos.northing = data->northing;
    sys->user_pos.altitude = data->altitude;
    printf("[FLASH] Data Loaded!\r\n");
  } else {
    // Default Initialization (If No Data)
    sys->user_pos.zone = 52;
    sys->user_pos.band = 'S';
    sys->user_pos.easting = 0.0;
    sys->user_pos.northing = 0.0;
    sys->user_pos.altitude = 0.0f;
    printf("[FLASH] No Data Found. Defaults Set.\r\n");
  }
}

// [2] Save Data to Flash
void Flash_Save_BatteryPos(FCS_System_t *sys) {
  Flash_SaveData_t data;
    
  // Prepare Data
  data.magic = FLASH_MAGIC_CODE;
  data.zone = sys->user_pos.zone;
  data.band = sys->user_pos.band;
  data.easting = sys->user_pos.easting;
  data.northing = sys->user_pos.northing;
  data.altitude = sys->user_pos.altitude;
    
  // Unlock Flash
  HAL_FLASH_Unlock();
    
  // 1. Erase Sector 7
  FLASH_EraseInitTypeDef EraseInitStruct;
  uint32_t SectorError;
    
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
  EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  EraseInitStruct.Sector = FLASH_SECTOR_7;
  EraseInitStruct.NbSectors = 1;
    
  if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
    printf("[FLASH] Erase Error (Code: %lu)\r\n", SectorError);
    HAL_FLASH_Lock();
    return;
  }
    
  // 2. Write Data (Byte by Byte or Word by Word)
  // We will write byte stream for simplicity and structure alignment safety
  uint8_t *ptr = (uint8_t*)&data;
  uint32_t addr = FLASH_STORAGE_ADDR;
    
  for (int i = 0; i < sizeof(Flash_SaveData_t); i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, ptr[i]) != HAL_OK) {
      printf("[FLASH] Write Error at %d\r\n", i);
      break;
    }
  }
    
  HAL_FLASH_Lock();
  printf("[FLASH] Save Complete.\r\n");
}
