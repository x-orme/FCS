/* USER CODE BEGIN Header */
/**
  * @file           : main.c
  * @brief          : Main program body (Refactored for Logic Encapsulation)
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
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "bmp280.h"
#include "ui.h"    
#include "input.h" 
#include "flash_ops.h"
#include "fcs_core.h" // Business Logic Core
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

// [Refactoring Note] 
// UART Ring Buffer & Command logic moved to fcs_core.c
// Only Handle injection remains here.

extern UART_HandleTypeDef huart2;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// [UART Rx Callback] - Redirect to Core
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2 || huart->Instance == USART1) { 
    FCS_UART_RxCallback(huart);
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
  if (bmp_res != 0) {
      ssd1306_SetCursor(0,0);
      ssd1306_WriteString("SENSOR ERROR", Font_7x10, White);
      ssd1306_UpdateScreen();
  }
  
  // 3. System & UI Init
  FCS_Init_System(&fcs);
  UI_Init(&fcs); // Initialize UI State specifically
  
  // [Flash] Load Saved Battery Position
  Flash_Load_BatteryPos(&fcs);
  
  // [UART] Start Reception (Trigger Core ISR)
  // We need to kickstart the interrupt chain.
  // The FCS_UART_RxCallback expects standard HAL Rx IT.
  // We can manually call a helper or just start it here.
  // Note: FCS_core doesn't expose the start function, but the callback relies on HAL_UART_Receive_IT.
  // We need to call it once. But we need a buffer pointer.
  // Ah, the latch buffer is internal to fcs_core.c. 
  // Ideally, fcs_core should expose "FCS_Serial_Start(huart)".
  // For now, I'll access the latch via a slight hack or assuming fcs_core handles it?
  // No, fcs_core static vars are hidden.
  // FIX: Calling FCS_UART_RxCallback directly won't work as it expects to be in ISR.
  // We need to call HAL_UART_Receive_IT from Main once.
  // But we don't know the address of 'rx_byte_latched' in fcs_core.c.
  // I must add FCS_Serial_Init to fcs_core.c public API.
  // WORKAROUND used in previous logic: The main.c declared 'rx_byte'. 
  // I will add a temporary one-byte buffer here solely to kick off the chain *if* I haven't refactored well.
  // BUT I did refactor. I should add `FCS_Serial_Start` to fcs_core.
  
  // Let's assume I'll add `FCS_Serial_Start` later? No, break the code.
  // I will stick to what I wrote: `FCS_UART_RxCallback`.
  // Wait, `FCS_UART_RxCallback` calls `HAL_UART_Receive_IT(huart, &rx_byte_latched, 1);`
  // I need to start the first one.
  // I will cheat: fcs_core.c's `FCS_UART_RxCallback` is the ISR handler.
  // The FIRST call must be done.
  // I will add `FCS_Serial_Start(UART_HandleTypeDef *huart)` to fcs_core right now?
  // Yes, I need to modify `fcs_core.c` and `.h` again? 
  // Or I can just trigger the callback manually? No, `huart->Instance` etc checks.
  
  // Quick Fix: Define `uint8_t dummy;` and start IT here? 
  // `HAL_UART_Receive_IT(&huart2, &dummy, 1);` -> Callback will fire -> calls FCS handler -> FCS handler re-arms with IT'S internal buffer.
  // Does `FCS_UART_RxCallback` use the passed byte? 
  // `u_buf[u_head] = rx_byte_latched;`
  // It uses its INTERNAL `rx_byte_latched`.
  // If I start with `&dummy`, the first byte received will go to `dummy`.
  // Then the callback fires. `FCS_UART_RxCallback` reads `rx_byte_latched` (which acts as junk or 0) and puts in ring buffer?
  // No, `rx_byte_latched` in fcs_core is static.
  // If the first receive uses `dummy`, the valid data is in `dummy`.
  // The callback logic: `u_buf = rx_byte_latched`. 
  // It will read uninitialized `rx_byte_latched` (rubbish) instead of `dummy`.
  // AND the actual character received (in `dummy`) is lost.
  
  // CONCLUSION: I MUST add `FCS_Serial_Start` to `fcs_core.c`.
  // I will update fcs_core.h/c quickly after this (or before?).
  // I'll assume it exists for now and implement it next step to avoid context switch in my head?
  // No, I'll invoke it in comments and then add it.
  // [UART] Start Reception
  FCS_Serial_Start(&huart2); 
  FCS_Serial_Start(&huart1);
  
  printf("\r\n[FCS] System Ready. Waiting for Commands...\r\n");
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // [1] State Updates (Business Logic)
    FCS_Update_Input(&fcs, &hadc1);
    FCS_Update_Sensors(&fcs);
    
    // [2] UI Logic
    UI_Update(&fcs);
    UI_Draw(&fcs);

    // [3] Background Tasks
    FCS_Task_Serial(&fcs, &huart1); // Respond to BT
    
    // [4] Control Loop Rate
    HAL_Delay(20); 
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
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
// [UART Redirect] printf to USART2
int _write(int file, char *ptr, int len) {
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
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
