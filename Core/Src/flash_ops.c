#include "flash_ops.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>

// Software CRC32 (Polynomial 0xEDB88320, reflected)
// No lookup table — saves Flash, only called on save/load
static uint32_t Calc_CRC32(const uint8_t *data, uint32_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
      else crc >>= 1;
    }
  }
  return crc ^ 0xFFFFFFFF;
}

// CRC covers payload bytes: [zone..altitude] (between magic and crc32)
#define CRC_DATA_OFFSET  offsetof(Flash_SaveData_t, zone)
#define CRC_DATA_SIZE    (offsetof(Flash_SaveData_t, crc32) - CRC_DATA_OFFSET)

// [1] Load Data from Flash
void Flash_Load_BatteryPos(FCS_System_t *sys) {
  Flash_SaveData_t *data = (Flash_SaveData_t*)FLASH_STORAGE_ADDR;

  // Check Magic
  if (data->magic != FLASH_MAGIC_CODE) {
    DBG_PRINT("[FLASH] No valid data (magic mismatch). Defaults set.\r\n");
    sys->user_pos.zone = 52;
    sys->user_pos.band = 'S';
    sys->user_pos.easting = 0.0;
    sys->user_pos.northing = 0.0;
    sys->user_pos.altitude = 0.0f;
    return;
  }

  // Verify CRC32
  uint32_t cal_crc = Calc_CRC32((const uint8_t*)data + CRC_DATA_OFFSET, CRC_DATA_SIZE);
  if (cal_crc != data->crc32) {
    DBG_PRINT("[FLASH] CRC32 mismatch (stored=0x%08lX calc=0x%08lX). Defaults set.\r\n",
           (unsigned long)data->crc32, (unsigned long)cal_crc);
    sys->user_pos.zone = 52;
    sys->user_pos.band = 'S';
    sys->user_pos.easting = 0.0;
    sys->user_pos.northing = 0.0;
    sys->user_pos.altitude = 0.0f;
    return;
  }

  // Valid — load data
  sys->user_pos.zone = data->zone;
  sys->user_pos.band = data->band;
  sys->user_pos.easting = data->easting;
  sys->user_pos.northing = data->northing;
  sys->user_pos.altitude = data->altitude;
  DBG_PRINT("[FLASH] Data loaded (CRC OK).\r\n");
}

// [2] Save Data to Flash
void Flash_Save_BatteryPos(FCS_System_t *sys) {
  Flash_SaveData_t data;

  // Prepare Data
  data.magic = FLASH_MAGIC_CODE;
  data.zone = (uint8_t)sys->user_pos.zone;
  data.band = (uint8_t)sys->user_pos.band;
  data.reserved[0] = 0;
  data.reserved[1] = 0;
  data.easting = sys->user_pos.easting;
  data.northing = sys->user_pos.northing;
  data.altitude = sys->user_pos.altitude;

  // Calculate CRC32 over payload
  data.crc32 = Calc_CRC32((const uint8_t*)&data + CRC_DATA_OFFSET, CRC_DATA_SIZE);

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
    DBG_PRINT("[FLASH] Erase Error (Code: %lu)\r\n", (unsigned long)SectorError);
    HAL_FLASH_Lock();
    return;
  }

  // 2. Write Data (byte-by-byte)
  uint8_t *ptr = (uint8_t*)&data;
  uint32_t addr = FLASH_STORAGE_ADDR;

  for (int i = 0; i < (int)sizeof(Flash_SaveData_t); i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, ptr[i]) != HAL_OK) {
      DBG_PRINT("[FLASH] Write Error at offset %d\r\n", i);
      break;
    }
  }

  HAL_FLASH_Lock();
  DBG_PRINT("[FLASH] Save complete (CRC=0x%08lX).\r\n", (unsigned long)data.crc32);
}
