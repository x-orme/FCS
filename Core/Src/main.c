/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <ssd1306.h>
#include <ssd1306_fonts.h>
#include "bmp280.h"
#include "ui.h"    // Added UI Module
#include "input.h" // Added Input Module
#include "flash_ops.h" // Added Flash Module
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
FCS_System_t fcs; // System Global Instance

#include "fcs_core.h" // Command Parser
// [UART Rx Buffer] (Line Buffer for Parsing)
#define RX_BUFF_SIZE 64
char rx_buffer[RX_BUFF_SIZE];
uint8_t rx_indx = 0;

// [UART Ring Buffer] (Raw Data Queue)
#define RING_SIZE 128
uint8_t u_buf[RING_SIZE];
volatile uint8_t u_head = 0;
volatile uint8_t u_tail = 0;
uint8_t rx_byte; // Temp holder for ISR

// External handles needed for callback
extern UART_HandleTypeDef huart2;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// [UART Rx Callback] - Interrupt Handler (Minimal Latency)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) { // PC Connection
    // 1. Enqueue Data to Ring Buffer
    uint8_t next_head = (u_head + 1) % RING_SIZE;
    if (next_head != u_tail) { // Check Full
        u_buf[u_head] = rx_byte;
        u_head = next_head;
    }
    // (If full, drop data - better than locking system)
    
    // 2. Re-arm Interrupt Immediately
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  // 1. OLED Init
  HAL_Delay(100); 
  ssd1306_Init(); 
  ssd1306_Fill(0); 
  
  // 2. BMP280 Init
  HAL_Delay(100);
  uint8_t bmp_res = BMP280_Init();
  printf("\r\n[SYSTEM] BMP280 Init Result: %s\r\n", bmp_res == 0 ? "OK" : "FAIL (Check Wiring)");
  
  // 3. UI System Init
  UI_Init(&fcs);
  
  // [Flash] Load Saved Battery Position (or set defaults)
  Flash_Load_BatteryPos(&fcs);
  
  // [UART] Start Reception Interrupt (USART2 for PC)
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  
  printf("\r\n[FCS] System Ready. Waiting for Commands...\r\n");
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // [1] Read Inputs (ADC)
    // Rank1(PA0), Rank2(PA1), Rank3(PA4), Rank4(PB0=Button)
    uint32_t adc_val[4] = {0};
    for(int i=0; i<4; i++) {
      HAL_ADC_Start(&hadc1);
      if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        adc_val[i] = HAL_ADC_GetValue(&hadc1);
      }
    }
    HAL_ADC_Stop(&hadc1);

    // Convert ADC to Key Event
    KeyState key = Input_Scan(adc_val[3]);

    // [2] Read Sensor Data & Store to System Struct
    BMP280_Data_t bmp_tmp;
    BMP280_Read_All(&bmp_tmp);
    fcs.env.air_temp = bmp_tmp.temperature;
    fcs.env.air_pressure = bmp_tmp.pressure;

    // [3] Update & Draw UI
    // Pass all inputs: Key state + 3 Knob values (X, Y, Z for Mask, Charge, Rounds)
    UI_Update(&fcs, key, adc_val); 
    UI_Draw(&fcs);

    /*
    // [Testing] Output to Terminal (Every 500ms) - Disabled for CMD Input
    static uint32_t last_test_log = 0;
    if (HAL_GetTick() - last_test_log > 500) {
      last_test_log = HAL_GetTick();

      // [Safe Print] Reverting to Integer casting as %f is not working (if needed)
      int t_int = (int)bmp_tmp.temperature;
      int t_dec = (int)((bmp_tmp.temperature - t_int) * 100);
      if(t_dec < 0) t_dec = -t_dec; 
      
      int p_int = (int)bmp_tmp.pressure;
      int p_dec = (int)((bmp_tmp.pressure - p_int) * 100);
      
      // VT100 Clear Line + Print (Altitude Removed)
      printf("\r\033[K[SENSOR] T:%d.%02d C | P:%d.%02d hPa", 
          t_int, t_dec, p_int, p_dec);

      // If calibration failed or pressure is weird, warn
      if (bmp_tmp.raw_pressure >= 0x80000 || bmp_tmp.pressure < 900) {
        printf(" <BAD SENSOR?>");
      }
      fflush(stdout);
    }
    */

    // [5] Serial Command Check (Ring Buffer Consumer)
    while (u_head != u_tail) {
        // Dequeue
        uint8_t ch = u_buf[u_tail];
        u_tail = (u_tail + 1) % RING_SIZE;
        
        // 1. Echo Back (Safe to block here in Main Loop)
        if (ch == '\r') {
             uint8_t nl[] = "\r\n";
             HAL_UART_Transmit(&huart2, nl, 2, 10);
        } else {
             HAL_UART_Transmit(&huart2, &ch, 1, 10);
        }

        // 2. Build Line Buffer
        if (ch == '\n' || ch == '\r') {
             rx_buffer[rx_indx] = 0; // Null terminate
             if (rx_indx > 0) {
                 char resp[64];
                 if (FCS_Process_Command(&fcs, rx_buffer, resp) > 0) {
                     printf("\r\n[CMD] %s\r\n", resp); 
                 } else {
                     printf("\r\n[CMD] Ignored/Error: %s\r\n", rx_buffer);
                 }
                 // Reset State
                 rx_indx = 0;
             }
        } else {
             if (rx_indx < RX_BUFF_SIZE - 1) {
                 rx_buffer[rx_indx++] = ch;
             } else {
                 rx_indx = 0; // Overflow / Ignore
             }
        }
    }

    // [4] Loop Delay (Control Refresh Rate) (Original Line)
    HAL_Delay(20); // 50Hz Update (High Responsiveness)
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
// [UART Redirect] printf to USART2 (ST-Link Virtual COM Port)
int _write(int file, char *ptr, int len) {
    // Timeout extended to 1000ms to ensure full transmission
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 1000);
    return len;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
