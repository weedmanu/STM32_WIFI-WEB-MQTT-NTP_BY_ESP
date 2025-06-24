/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : Test_Receive_MQTT.c
 * @brief          : Programme de test de réception MQTT avec STM32 et ESP01
 ******************************************************************************
 * @details
 * Ce programme teste la réception de messages MQTT avec un STM32 connecté à
 * un module ESP01. Il réalise les opérations suivantes :
 *
 * - Initialisation du driver ESP01
 * - Vidage du buffer de réception
 * - Test de communication AT basique
 * - Connexion au réseau WiFi
 * - Configuration du mode connexion unique (CIPMUX=0)
 * - Récupération et affichage de l'adresse IP
 * - Connexion à un broker MQTT local
 * - Abonnement à un topic spécifique
 * - Configuration d'un callback pour traiter les messages reçus
 * - Traitement continu des messages MQTT entrants
 *
 * Le programme affiche tous les messages reçus sur la console série et fait
 * clignoter une LED pour indiquer que le système fonctionne correctement.
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
 * - LED2 : Indicateur visuel de fonctionnement
 *   - Clignotement à intervalle régulier (500ms)
 *
 * Paramètres MQTT :
 * - Broker : 192.168.1.185:1883 (remplacer par votre broker)
 * - Client ID : généré dynamiquement (stm32_xxxx)
 * - Topic : stm32/test
 * - QoS : 0
 *
 * @note
 * - Nécessite les modules STM32_WifiESP.h/.c et STM32_WifiESP_MQTT.h/.c
 * - Compatible avec les modules ESP8266 (ESP01, ESP01S, etc.)
 * - Baudrate par défaut: 115200 bps
 * - Broker MQTT requis sur le réseau local
 * - Ce programme fonctionne en continu sans s'arrêter
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>              // Pour printf, snprintf, etc.
#include "STM32_WifiESP.h"      // Fonctions du driver ESP01
#include "STM32_WifiESP_MQTT.h" // Fonctions MQTT haut niveau
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Pas de typedef utilisateur spécifique ici
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SSID "XXXXXXXX"               // Nom du réseau WiFi
#define PASSWORD "XXXXXXXXXXXXXXXXXX" // Mot de passe du réseau WiFi
#define BROKER_IP "192.168.XXX.XXX"   // Adresse IP du broker MQTT
#define BROKER_PORT 1883              // Port du broker MQTT
#define BROKER_TOPIC "stm32/test"     // Topic MQTT auquel s'abonner
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

// Callback MQTT : affichera tout message reçu sur le topic
void mqtt_message_callback(const char *topic, const char *payload)
{
  printf("[TEST][INFO] Message MQTT reçu sur %s : %s\r\n", topic, payload); // Affiche le message reçu
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();

  HAL_Delay(1000);
  printf("\n[TEST][INFO] === Démarrage du programme de réception MQTT STM32-ESP01 ===\r\n");
  HAL_Delay(500);

  ESP01_Status_t status;

  // Initialisation ESP01
  printf("\n[TEST][INFO] === Initialisation du module ESP01 ===\r\n");
  status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
  printf("[TEST][INFO] Initialisation ESP01 : %s\r\n", esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec de l'initialisation ESP01\r\n");
    Error_Handler();
  }
  HAL_Delay(500);

  // Flush RX
  printf("\n[TEST][INFO] === Vidage du buffer RX ===\r\n");
  status = esp01_flush_rx_buffer(500);
  printf("[TEST][INFO] Buffer UART/DMA vidé : %s\r\n", esp01_get_error_string(status));
  HAL_Delay(100);

  // Test AT
  printf("\n[TEST][INFO] === Test de communication AT ===\r\n");
  status = esp01_test_at();
  printf("[TEST][INFO] Test AT : %s\r\n", esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec du test de communication\r\n");
    Error_Handler();
  }
  HAL_Delay(500);

  // Connexion WiFi
  printf("\n[TEST][INFO] === Connexion au réseau WiFi \"%s\" ===\r\n", SSID);
  status = esp01_connect_wifi_config(
      ESP01_WIFI_MODE_STA,
      SSID,
      PASSWORD,
      true,
      NULL,
      NULL,
      NULL);
  printf("[TEST][INFO] Connexion WiFi : %s\r\n", esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec de connexion au réseau WiFi\r\n");
    Error_Handler();
  }
  HAL_Delay(1000);

  // Mode connexion unique
  printf("\n[TEST][INFO] === Configuration du mode connexion unique ===\r\n");
  char resp[ESP01_DMA_RX_BUF_SIZE];
  status = esp01_send_raw_command_dma("AT+CIPMUX=0", resp, sizeof(resp), "OK", 3000);
  printf("[TEST][INFO] Mode connexion unique : %s\r\n", esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec de configuration du mode connexion unique\r\n");
    Error_Handler();
  }
  HAL_Delay(500);

  // Adresse IP
  printf("\n[TEST][INFO] === Récupération de l'adresse IP ===\r\n");
  char ip[32];
  status = esp01_get_current_ip(ip, sizeof(ip));
  if (status == ESP01_OK)
  {
    printf("[TEST][INFO] Adresse IP : %s\r\n", ip);
  }
  else
  {
    printf("[TEST][WARN] Impossible d'obtenir l'adresse IP : %s\r\n", esp01_get_error_string(status));
  }
  HAL_Delay(500);

  printf("\n[TEST][INFO] === Connexion au broker MQTT ===\r\n");
  char client_id[16];
  snprintf(client_id, sizeof(client_id), "stm32_%lu", HAL_GetTick() & 0xFFFF);

  printf("[TEST][INFO] Broker : %s:%d, Client ID : %s\r\n", BROKER_IP, BROKER_PORT, client_id);
  status = esp01_mqtt_connect(BROKER_IP, BROKER_PORT, client_id, NULL, NULL);
  printf("[TEST][INFO] Connexion broker MQTT : %s\r\n", esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec de connexion au broker MQTT\r\n");
    Error_Handler();
  }
  HAL_Delay(500);

  // Abonnement
  printf("\n[TEST][INFO] === Abonnement au topic MQTT ===\r\n");
  status = esp01_mqtt_subscribe(BROKER_TOPIC, 0);
  printf("[TEST][INFO] Abonnement au topic \"%s\" : %s\r\n", BROKER_TOPIC, esp01_get_error_string(status));
  if (status != ESP01_OK)
  {
    printf("[TEST][ERROR] Échec d'abonnement au topic\r\n");
    Error_Handler();
  }
  HAL_Delay(500);

  // Callback réception
  printf("\n[TEST][INFO] === Configuration du callback de réception ===\r\n");
  esp01_mqtt_set_message_callback(mqtt_message_callback);
  printf("[TEST][INFO] Callback de réception configuré\r\n");

  printf("\n[TEST][INFO] === Démarrage de la boucle d'écoute MQTT ===\r\n");
  printf("[TEST][INFO] En attente de messages sur le topic \"%s\"...\r\n", BROKER_TOPIC);

  // Boucle principale : juste réception et clignotement LED
  while (1)
  {
    esp01_mqtt_poll();                          // Vérifie si un message est reçu et appelle le callback
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); // Clignote la LED pour indiquer que le programme tourne
    HAL_Delay(500);                             // Délai pour le clignotement
  }
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
