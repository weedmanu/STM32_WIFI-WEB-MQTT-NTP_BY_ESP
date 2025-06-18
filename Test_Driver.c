/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : NTP.c
 * @brief          : Exemple de synchronisation NTP avec STM32 et ESP01
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * Ce fichier contient le code utilisateur pour tester la synchronisation NTP
 * depuis un STM32 via un module ESP01. Il gère l'initialisation du WiFi,
 * la synchronisation de l'heure via NTP, l'affichage de l'heure et le clignotement d'une LED.
 *
 * Ce code est fourni "en l'état", sans garantie.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>         // Pour printf, snprintf, etc.
#include "STM32_WifiESP.h" // Fonctions du driver ESP01
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Pas de typedef utilisateur spécifique ici
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
// Pas de macro utilisateur spécifique ici
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Tampon DMA pour la réception ESP01
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
// Pas de prototypes utilisateur spécifiques ici
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Redirige printf vers l'UART2 (console série)
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF); // Envoie le caractère sur UART2
  return ch;                                             // Retourne le caractère envoyé
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(500);
  ESP01_Status_t status;
  char buf[ESP01_MAX_RESP_BUF];

  printf("\n=== [TESTS DRIVER ESP01] Début des tests du driver STM32_WifiESP ===\n");

  printf("\n=== [INIT] Initialisation du driver ESP01 ===\n");
  status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
  if (status == ESP01_OK)
  {
    printf(">>> [INIT] Initialisation réussie\n");
  }
  else
  {
    printf(">>> [INIT] Échec de l'initialisation\n");
  }
  HAL_Delay(500);

  printf("\n=== [RESET] Reset logiciel (AT+RST)\n");
  status = esp01_reset();
  if (status == ESP01_OK)
  {
    printf(">>> [RESET] Reset logiciel réussi\n");
  }
  else
  {
    printf(">>> [RESET] Échec du reset logiciel\n");
  }
  HAL_Delay(1000);

  printf("\n=== [RESTORE] Restore usine (AT+RESTORE)\n");
  status = esp01_restore();
  if (status == ESP01_OK)
  {
    printf(">>> [RESTORE] Restauration usine réussie\n");
  }
  else
  {
    printf(">>> [RESTORE] Échec de la restauration usine\n");
  }
  HAL_Delay(1000);

  printf("\n=== [GMR] Lecture version firmware\n");
  status = esp01_get_at_version(buf, sizeof(buf));
  if (status == ESP01_OK)
  {
    printf(">>> [GMR] Version du firmware : %s\n", buf);
  }
  else
  {
    printf(">>> [GMR] Échec de la lecture de la version du firmware\n");
  }
  HAL_Delay(500);

  printf("\n=== [UART] Lecture config UART\n");
  status = esp01_get_uart_config(buf, sizeof(buf));
  char uart_str[ESP01_MAX_RESP_BUF];
  if (status == ESP01_OK)
  {
    esp01_uart_config_to_string(buf, uart_str, sizeof(uart_str));
    printf(">>> [UART] Configuration UART : %s\n", uart_str);
  }
  else
  {
    printf(">>> [UART] Échec de la lecture de la configuration UART\n");
  }
  HAL_Delay(500);

  printf("\n=== [SLEEP] Lecture mode sommeil\n");
  int sleep_mode = 0;
  status = esp01_get_sleep_mode(&sleep_mode);
  char sleep_str[ESP01_MAX_RESP_BUF];
  esp01_sleep_mode_to_string(sleep_mode, sleep_str, sizeof(sleep_str));
  if (status == ESP01_OK)
  {
    printf(">>> [SLEEP] Mode sommeil : %s\n", sleep_str);
  }
  else
  {
    printf(">>> [SLEEP] Échec de la lecture du mode sommeil\n");
  }
  HAL_Delay(500);

  printf("\n=== [RFPOWER] Lecture puissance RF\n");
  int rf_dbm = 0;
  status = esp01_get_rf_power(&rf_dbm);
  if (status == ESP01_OK)
  {
    printf(">>> [RFPOWER] Puissance RF : %d dBm\n", rf_dbm);
  }
  else
  {
    printf(">>> [RFPOWER] Échec de la lecture de la puissance RF\n");
  }
  HAL_Delay(500);

  printf("\n=== [SYSLOG] Lecture niveau log système\n");
  int syslog = 0;
  status = esp01_get_syslog(&syslog);
  char syslog_str[ESP01_MAX_RESP_BUF];
  esp01_syslog_to_string(syslog, syslog_str, sizeof(syslog_str));
  if (status == ESP01_OK)
  {
    printf(">>> [SYSLOG] Niveau de log : %s\n", syslog_str);
  }
  else
  {
    printf(">>> [SYSLOG] Échec de la lecture du niveau de log\n");
  }
  HAL_Delay(500);

  printf("\n=== [SYSRAM] Lecture RAM libre\n");
  uint32_t free_ram = 0;
  status = esp01_get_sysram(&free_ram);
  if (status == ESP01_OK)
  {
    printf(">>> [SYSRAM] RAM libre : %lu octets\n", free_ram);
  }
  else
  {
    printf(">>> [SYSRAM] Échec de la lecture de la RAM libre\n");
  }
  HAL_Delay(500);

  printf("\n=== [CMD] Liste des commandes AT\n");
  char cmd_list[4096];
  status = esp01_get_cmd_list(cmd_list, sizeof(cmd_list));
  if (status == ESP01_OK)
  {
    printf(">>> [CMD] Liste des commandes AT :\n%s\n", cmd_list);
  }
  else
  {
    printf(">>> [CMD] Échec de la lecture de la liste des commandes AT\n");
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
