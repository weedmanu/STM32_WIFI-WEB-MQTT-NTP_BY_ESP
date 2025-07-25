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

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

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
  HAL_Delay(1000);              // Attente pour stabiliser la console série
  ESP01_Status_t status;        // Variable pour stocker le statut des opérations
  char buf[ESP01_MAX_RESP_BUF]; // Tampon pour les réponses des commandes AT

  printf("\n[TEST][INFO] === Début des tests du driver STM32_WifiESP ===\r\n"); // Message d'introduction
  HAL_Delay(500);                                                               // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Initialisation du driver ESP01 ===\r\n");                            // Message d'initialisation
  status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);                 // Initialisation du driver
  printf("[TEST][INFO] Initialisation du driver ESP01 : %s\r\n", esp01_get_error_string(status)); // Affichage du statut
  if (status != ESP01_OK)                                                                         // Vérification de l'état d'initialisation
  {
    printf("[TEST][ERROR] Échec de l'initialisation du driver\r\n"); // Message d'erreur si l'initialisation échoue
    Error_Handler();                                                 // Appel de la fonction d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Reset logiciel (AT+RST) ===\r\n");                   // Message de reset logiciel
  status = esp01_reset();                                                         // Appel de la fonction de reset logiciel
  printf("[TEST][INFO] Reset logiciel : %s\r\n", esp01_get_error_string(status)); // Affichage du statut du reset
  if (status != ESP01_OK)                                                         // Vérification de l'état du reset
  {
    printf("[TEST][ERROR] Échec du reset logiciel\r\n"); // Message d'erreur si le reset échoue
  }
  HAL_Delay(1000); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Lecture version firmware ESP01 (AT+GMR) ===\r\n"); // Message de lecture de la version du firmware
  char resp[512] = {0};                                                         // Tampon pour la réponse de la commande AT
  status = esp01_get_at_version(resp, sizeof(resp));                            // Appel de la fonction pour lire la version du firmware
  if (status == ESP01_OK)                                                       // Vérification de l'état de la lecture de la version
  {
    uint8_t line_count = esp01_display_firmware_info(resp);                                // Affichage des informations de version
    printf("[TEST][INFO] Nombre de lignes d'informations extraites : %d\r\n", line_count); // Affichage du nombre de lignes extraites
  }
  else // Si la lecture de la version échoue
  {
    printf("[TEST][ERROR] Échec de la lecture de la version firmware : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Lecture configuration UART ===\r\n"); // Message de lecture de la configuration UART
  status = esp01_get_uart_config(buf, sizeof(buf));                // Appel de la fonction pour lire la configuration UART
  char uart_str[ESP01_MAX_RESP_BUF];                               // Tampon pour la chaîne de configuration UART
  if (status == ESP01_OK)                                          // Si la lecture de la configuration UART réussit
  {
    esp01_uart_config_to_string(buf, uart_str, sizeof(uart_str)); // Conversion de la configuration en chaîne lisible
    printf("[TEST][INFO] Configuration UART : %s\r\n", uart_str); // Affichage de la configuration UART
  }
  else // Si la lecture de la configuration UART échoue
  {
    printf("[TEST][ERROR] Échec de la lecture de configuration UART : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Lecture mode sommeil ===\r\n");            // Message de lecture du mode sommeil
  int sleep_mode = 0;                                                   // Variable pour stocker le mode sommeil
  status = esp01_get_sleep_mode(&sleep_mode);                           // Appel de la fonction pour lire le mode sommeil
  char sleep_str[ESP01_MAX_RESP_BUF];                                   // Tampon pour la chaîne de mode sommeil
  esp01_sleep_mode_to_string(sleep_mode, sleep_str, sizeof(sleep_str)); // Conversion du mode sommeil en chaîne lisible
  if (status == ESP01_OK)                                               // Si la lecture du mode sommeil réussit
  {
    printf("[TEST][INFO] Mode sommeil : %s\r\n", sleep_str); // Affichage du mode sommeil
  }
  else // Si la lecture du mode sommeil échoue
  {
    printf("[TEST][ERROR] Échec de la lecture du mode sommeil : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Lecture puissance RF ===\r\n"); // Message de lecture de la puissance RF
  int rf_dbm = 0;                                            // Variable pour stocker la puissance RF
  status = esp01_get_rf_power(&rf_dbm);                      // Appel de la fonction pour lire la puissance RF
  if (status == ESP01_OK)                                    // Si la lecture de la puissance RF réussit
  {
    printf("[TEST][INFO] Puissance RF : %d dBm\r\n", rf_dbm); // Affichage de la puissance RF
  }
  else // Si la lecture de la puissance RF échoue
  {
    printf("[TEST][ERROR] Échec de la lecture de la puissance RF : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Lecture niveau log système ===\r\n"); // Message de lecture du niveau de log système
  int syslog = 0;                                                  // Variable pour stocker le niveau de log
  status = esp01_get_syslog(&syslog);                              // Appel de la fonction pour lire le niveau de log
  char syslog_str[ESP01_MAX_RESP_BUF];                             // Tampon pour la chaîne du niveau de log
  esp01_syslog_to_string(syslog, syslog_str, sizeof(syslog_str));  // Conversion du niveau de log en chaîne lisible
  if (status == ESP01_OK)                                          // Si la lecture du niveau de log réussit
  {
    printf("[TEST][INFO] Niveau de log : %s\r\n", syslog_str); // Affichage du niveau de log
  }
  else // Si la lecture du niveau de log échoue
  {
    printf("[TEST][ERROR] Échec de la lecture du niveau de log : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Lecture RAM libre ===\r\n"); // Message de lecture de la RAM libre
  uint32_t free_ram = 0, min_ram = 0;                     // Variables pour stocker la RAM libre et minimale
  status = esp01_get_sysram(&free_ram, &min_ram);         // Appel de la fonction pour lire la RAM
  char ram_str[ESP01_MAX_RESP_BUF];                       // Tampon pour la chaîne de RAM
  if (status == ESP01_OK)                                 // Si la lecture de la RAM réussit
  {
    esp01_sysram_to_string(free_ram, min_ram, ram_str, sizeof(ram_str)); // Conversion des valeurs RAM en chaîne lisible
    printf("[TEST][INFO] RAM libre : %s\r\n", ram_str);                  // Affichage de la RAM libre
  }
  else // Si la lecture de la RAM échoue
  {
    printf("[TEST][ERROR] Échec de la lecture de la RAM libre : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Lecture stockage système ===\r\n"); // Message de lecture du stockage système
  uint32_t sysstore = 0;                                         // Variable pour stocker le stockage système
  status = esp01_get_sysstore(&sysstore);                        // Appel de la fonction pour lire le stockage système
  char sysstore_str[ESP01_MAX_RESP_BUF];                         // Tampon pour la chaîne du stockage système
  if (status == ESP01_OK)                                        // Si la lecture du stockage système réussit
  {
    esp01_sysstore_to_string(sysstore, sysstore_str, sizeof(sysstore_str)); // Conversion du stockage en chaîne lisible
    printf("[TEST][INFO] %s\r\n", sysstore_str);                            // Affichage du stockage système
  }
  else // Si la lecture du stockage système échoue
  {
    printf("[TEST][ERROR] Échec de la lecture du stockage système : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Lecture Flash système (partitions détaillées) ===\r\n"); // Message de lecture de la Flash système
  char sysflash_resp[512] = {0};                                                      // Tampon pour la réponse de la commande SYSFLASH
  status = esp01_get_sysflash(sysflash_resp, sizeof(sysflash_resp));                  // Appel de la fonction pour lire la Flash système
  if (status == ESP01_OK)                                                             // Si la lecture de la Flash système réussit
  {
    printf("[TEST][INFO] Table SYSFLASH récupérée avec succès\r\n");            // Message de succès
    uint8_t part_count = esp01_display_sysflash_partitions(sysflash_resp);      // Affichage des partitions extraites
    printf("[TEST][INFO] Nombre de partitions extraites : %d\r\n", part_count); // Affichage du nombre de partitions
  }
  else // Si la lecture de la Flash système échoue
  {
    printf("[TEST][ERROR] Impossible de récupérer la table SYSFLASH : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }

  printf("\n[TEST][INFO] === Lecture RAM utilisateur ===\r\n"); // Message de lecture de la RAM utilisateur
  uint32_t userram = 0;                                         // Variable pour stocker la RAM utilisateur
  status = esp01_get_userram(&userram);                         // Appel de la fonction pour lire la RAM utilisateur
  char userram_str[ESP01_MAX_RESP_BUF];                         // Tampon pour la chaîne de RAM utilisateur
  if (status == ESP01_OK)                                       // Si la lecture de la RAM utilisateur réussit
  {
    esp01_userram_to_string(userram, userram_str, sizeof(userram_str)); // Conversion de la RAM utilisateur en chaîne lisible
    printf("[TEST][INFO] %s\r\n", userram_str);                         // Affichage de la RAM utilisateur
  }
  else // Si la lecture de la RAM utilisateur échoue
  {
    printf("[TEST][ERROR] Échec de la lecture de la RAM utilisateur : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Liste des commandes AT ===\r\n"); // Message de lecture de la liste des commandes AT
  char cmd_list[4096];                                         // Tampon pour la liste des commandes AT
  status = esp01_get_cmd_list(cmd_list, sizeof(cmd_list));     // Appel de la fonction pour lire la liste des commandes AT
  if (status == ESP01_OK)                                      // Si la lecture de la liste réussit
  {
    printf("[TEST][INFO] Liste des commandes AT :\r\n%s\r\n", cmd_list); // Affichage de la liste des commandes AT
  }
  else // Si la lecture de la liste échoue
  {
    printf("[TEST][ERROR] Échec de la lecture de la liste des commandes AT : %s\r\n", esp01_get_error_string(status)); // Message d'erreur
  }
  HAL_Delay(500); // Pause pour laisser le temps de lire le message

  printf("\n[TEST][INFO] === Restauration paramètres usine (AT+RESTORE) ===\r\n");    // Message de restauration usine
  status = esp01_restore();                                                           // Appel de la fonction de restauration usine
  printf("[TEST][INFO] Restauration usine : %s\r\n", esp01_get_error_string(status)); // Affichage du statut de la restauration
  if (status != ESP01_OK)                                                             // Si la restauration échoue
  {
    printf("[TEST][ERROR] Échec de la restauration usine\r\n"); // Message d'erreur
  }
  printf("\n[TEST][INFO] === Fin des tests du driver STM32_WifiESP ===\r\n"); // Message de fin de test
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
