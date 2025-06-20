/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : Test_Driver.c
 * @brief          : Programme de test du driver bas niveau STM32_WifiESP
 ******************************************************************************
 * @details
 * Ce programme réalise des tests systématiques des fonctions bas niveau
 * du driver STM32_WifiESP pour module ESP01 (ESP8266). Il teste successivement :
 *
 * - Initialisation du driver ESP01
 * - Reset logiciel (AT+RST)
 * - Restauration des paramètres usine (AT+RESTORE)
 * - Lecture de la version du firmware (AT+GMR)
 * - Lecture et affichage de la configuration UART (AT+UART?)
 * - Lecture du mode sommeil (AT+SLEEP?)
 * - Lecture de la puissance RF (AT+RFPOWER?)
 * - Lecture du niveau de log système (AT+SYSLSG?)
 * - Lecture de la RAM libre (AT+SYSRAM?)
 * - Récupération de la liste complète des commandes AT
 *
 * L'objectif est de valider le bon fonctionnement de la couche de communication
 * bas niveau entre le STM32 et le module ESP01, sans impliquer les fonctionnalités
 * réseau (WiFi, HTTP, MQTT, etc.) qui sont testées dans d'autres programmes.
 *
 * Configuration matérielle :
 * - UART1 : Communication avec le module ESP01
 *   - Mode : Half-duplex
 *   - DMA RX : Mode circulaire (buffer continu)
 *
 * - UART2 : Console série avec l'ordinateur
 *   - Mode : Full-duplex
 *   - Affichage des résultats via printf redirigé
 *
 * @note
 * - Nécessite le driver STM32_WifiESP.h/.c
 * - Compatible avec les modules ESP8266 (ESP01, ESP01S, etc.)
 * - Baudrate par défaut: 115200 bps
 * - Vérifiez les connexions matérielles avant exécution
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
  HAL_Delay(1000);
  ESP01_Status_t status;
  char buf[ESP01_MAX_RESP_BUF];

  printf("\n[TEST][INFO] === Début des tests du driver STM32_WifiESP ===\r\n");
  HAL_Delay(500);

  printf("\n[TEST][INFO] === Initialisation du driver ESP01 ===\r\n");
  status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
  printf("[TEST][INFO] Initialisation du driver ESP01 : %s\r\n", esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec de l'initialisation du driver\r\n");
    Error_Handler();
  }
  HAL_Delay(500);

  printf("\n[TEST][INFO] === Reset logiciel (AT+RST) ===\r\n");
  status = esp01_reset();
  printf("[TEST][INFO] Reset logiciel : %s\r\n", esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec du reset logiciel\r\n");
  }
  HAL_Delay(1000);

  printf("\n[TEST][INFO] === Restauration paramètres usine (AT+RESTORE) ===\r\n");
  status = esp01_restore();
  printf("[TEST][INFO] Restauration usine : %s\r\n", esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec de la restauration usine\r\n");
  }
  HAL_Delay(1000);

  printf("\n[TEST][INFO] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
  char resp[512] = {0};
  char *lines[5] = {NULL};
  char lines_buffer[512] = {0};

  if (esp01_get_at_version(resp, sizeof(resp)) == ESP01_OK)
  {
    uint8_t line_count = esp01_split_response_lines(resp, lines, 5,
                                                    lines_buffer, sizeof(lines_buffer), true);

    printf("[TEST][INFO] Version du firmware (%d lignes) :\r\n", line_count);
    // Afficher les lignes brutes, sans préfixe
    for (uint8_t i = 0; i < line_count; i++)
    {
      printf("%s\r\n", lines[i]); // Affichage des lignes sans préfixe
    }
  }
  else
  {
    printf("[TEST][ERROR] Échec de la lecture de version\r\n");
  }

  printf("\n[TEST][INFO] === Lecture configuration UART ===\r\n");
  status = esp01_get_uart_config(buf, sizeof(buf));
  char uart_str[ESP01_MAX_RESP_BUF];
  if (status == ESP01_OK)
  {
    esp01_uart_config_to_string(buf, uart_str, sizeof(uart_str));
    printf("[TEST][INFO] Configuration UART : %s\r\n", uart_str);
  }
  else
  {
    printf("[TEST][ERROR] Échec de la lecture de configuration UART : %s\r\n", esp01_get_error_string(status));
  }
  HAL_Delay(500);

  printf("\n[TEST][INFO] === Lecture mode sommeil ===\r\n");
  int sleep_mode = 0;
  status = esp01_get_sleep_mode(&sleep_mode);
  char sleep_str[ESP01_MAX_RESP_BUF];
  esp01_sleep_mode_to_string(sleep_mode, sleep_str, sizeof(sleep_str));
  if (status == ESP01_OK)
  {
    printf("[TEST][INFO] Mode sommeil : %s\r\n", sleep_str);
  }
  else
  {
    printf("[TEST][ERROR] Échec de la lecture du mode sommeil : %s\r\n", esp01_get_error_string(status));
  }
  HAL_Delay(500);

  printf("\n[TEST][INFO] === Lecture puissance RF ===\r\n");
  int rf_dbm = 0;
  status = esp01_get_rf_power(&rf_dbm);
  if (status == ESP01_OK)
  {
    printf("[TEST][INFO] Puissance RF : %d dBm\r\n", rf_dbm);
  }
  else
  {
    printf("[TEST][ERROR] Échec de la lecture de la puissance RF : %s\r\n", esp01_get_error_string(status));
  }
  HAL_Delay(500);

  printf("\n[TEST][INFO] === Lecture niveau log système ===\r\n");
  int syslog = 0;
  status = esp01_get_syslog(&syslog);
  char syslog_str[ESP01_MAX_RESP_BUF];
  esp01_syslog_to_string(syslog, syslog_str, sizeof(syslog_str));
  if (status == ESP01_OK)
  {
    printf("[TEST][INFO] Niveau de log : %s\r\n", syslog_str);
  }
  else
  {
    printf("[TEST][ERROR] Échec de la lecture du niveau de log : %s\r\n", esp01_get_error_string(status));
  }
  HAL_Delay(500);

  printf("\n[TEST][INFO] === Lecture RAM libre ===\r\n");
  uint32_t free_ram = 0;
  status = esp01_get_sysram(&free_ram);
  if (status == ESP01_OK)
  {
    printf("[TEST][INFO] RAM libre : %lu octets\r\n", free_ram);
  }
  else
  {
    printf("[TEST][ERROR] Échec de la lecture de la RAM libre : %s\r\n", esp01_get_error_string(status));
  }
  HAL_Delay(500);

  printf("\n[TEST][INFO] === Liste des commandes AT ===\r\n");
  char cmd_list[4096];
  status = esp01_get_cmd_list(cmd_list, sizeof(cmd_list));
  if (status == ESP01_OK)
  {
    printf("[TEST][INFO] Liste des commandes AT :\r\n%s\r\n", cmd_list);
  }
  else
  {
    printf("[TEST][ERROR] Échec de la lecture de la liste des commandes AT : %s\r\n", esp01_get_error_string(status));
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
