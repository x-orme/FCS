#ifndef __BMP280_H
#define __BMP280_H

#include "main.h"

/* BMP280 I2C Address (SDO=GND: 0x76, SDO=VDD: 0x77) */
#define BMP280_I2C_ADDR (0x76 << 1) 

/* Registers */
#define BMP280_REG_TEMP_XLSB   0xFC
#define BMP280_REG_TEMP_LSB    0xFB
#define BMP280_REG_TEMP_MSB    0xFA
#define BMP280_REG_PRESS_XLSB  0xF9
#define BMP280_REG_PRESS_LSB   0xF8
#define BMP280_REG_PRESS_MSB   0xF7
#define BMP280_REG_CONFIG      0xF5
#define BMP280_REG_CTRL_MEAS   0xF4
#define BMP280_REG_STATUS      0xF3
#define BMP280_REG_RESET       0xE0
#define BMP280_REG_ID          0xD0
#define BMP280_REG_CALIB       0x88

/* Structure to hold sensor data */
typedef struct {
    float temperature;
    float pressure;
    float altitude; // Calculated altitude
    int32_t raw_pressure; // Debug: Raw ADC value
    int32_t raw_temperature; // Debug: Raw ADC value
} BMP280_Data_t;

/* Functions */
uint8_t BMP280_Init(void);
void BMP280_Read_All(BMP280_Data_t *data);
void BMP280_SetQNH(float qnh_hpa);

#endif /* __BMP280_H */
