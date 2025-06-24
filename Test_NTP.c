/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : Test_NTP.c
 * @brief          : Programme de test de synchronisation NTP avec STM32 et ESP01
 ******************************************************************************
 * @details
 * Ce programme permet de tester les fonctionnalités de synchronisation NTP
 * (Network Time Protocol) via un module ESP01 connecté à un STM32. Il réalise
 * notamment les opérations suivantes :
 *
 * - Initialisation du module ESP01
 * - Connexion au réseau WiFi
 * - Configuration des paramètres NTP (serveur, fuseau horaire, période)
 * - Démarrage de la synchronisation NTP automatique
 * - Affichage de l'heure et de la date obtenues
 * - Gestion des mises à jour périodiques de l'heure
 *
 * Le programme maintient automatiquement la synchronisation avec le serveur NTP
 * et affiche l'heure mise à jour à chaque nouvelle synchronisation.
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
 * Paramètres NTP :
 * - Serveur NTP : fr.pool.ntp.org (serveur français)
 * - Fuseau horaire : GMT+1 (France, hors heure d'été)
 * - Période de mise à jour : 20 secondes (paramètre de démonstration)
 * - Gestion automatique de l'heure d'été (DST)
 *
 * @note
 * - Nécessite les modules STM32_WifiESP, STM32_WifiESP_WIFI et STM32_WifiESP_NTP
 * - Compatible avec les modules ESP8266 (ESP01, ESP01S, etc.)
 * - Baudrate par défaut: 115200 bps
 * - Une connexion internet active est requise pour le fonctionnement du NTP
 * - Pour une utilisation en production, une période plus longue est recommandée (3600s)
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
#include "STM32_WifiESP_WIFI.h" // Fonctions WiFi haut niveau
#include "STM32_WifiESP_NTP.h"  // Fonctions NTP haut niveau
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SSID "XXXXXXXX"               // Nom du réseau WiFi
#define PASSWORD "XXXXXXXXXXXXXXXXXX" // Mot de passe du réseau WiFi
#define NTP_SERVER "fr.pool.ntp.org"  // Serveur NTP français
#define NTP_TIMEZONE 1                // Fuseau horaire (GMT+1 pour la France)
#define NTP_UPDATE_PERIOD_S 20        // Période de mise à jour NTP en secondes
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */
static uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Buffer DMA pour la réception ESP01
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
  HAL_Delay(500);                                                              // Attente initiale pour stabiliser le système
  printf("\n[TEST][INFO] === Démarrage du programme NTP STM32-ESP01 ===\r\n"); // Affichage du message de démarrage
  HAL_Delay(500);                                                              // Attente pour laisser le temps à l'affichage de se stabiliser
  ESP01_Status_t status;                                                       // Variable pour stocker le statut des opérations ESP01

  /* Initialisation du module ESP01 */
  printf("\n[TEST][INFO] === Initialisation du module ESP01 ===\r\n");                 // Affichage du message d'initialisation
  status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, sizeof(esp01_dma_rx_buf));   // Initialisation du module ESP01 avec UART1 et UART2
  printf("[TEST][INFO] Initialisation ESP01: %s\r\n", esp01_get_error_string(status)); // Affichage du statut de l'initialisation
  if (status != ESP01_OK)                                                              // Vérification du statut d'initialisation
  {
    printf("[TEST][ERROR] Échec initialisation ESP01\r\n"); // Affichage d'une erreur si l'initialisation a échoué
    Error_Handler();                                        // Appel de la fonction d'erreur
  }
  HAL_Delay(500); // Attente pour laisser le temps à l'initialisation de se stabiliser

  /* Connexion au réseau WiFi */
  printf("\n[TEST][INFO] === Connexion au réseau WiFi \"%s\" ===\r\n", SSID);                      // Affichage du message de connexion WiFi
  status = esp01_connect_wifi_config(ESP01_WIFI_MODE_STA, SSID, PASSWORD, true, NULL, NULL, NULL); // Connexion au WiFi en mode station
  printf("[TEST][INFO] Connexion WiFi: %s\r\n", esp01_get_error_string(status));                   // Affichage du statut de la connexion WiFi
  if (status != ESP01_OK)                                                                          // Vérification du statut de connexion WiFi
  {
    printf("[TEST][ERROR] Échec connexion WiFi\r\n"); // Affichage d'une erreur si la connexion a échoué
    Error_Handler();                                  // Appel de la fonction d'erreur
  }
  HAL_Delay(500); // Attente pour laisser le temps à la connexion de se stabiliser

  /* Affichage de l'adresse IP */
  printf("\n[TEST][INFO] === Récupération de l'adresse IP ===\r\n"); // Affichage du message de récupération de l'adresse IP
  char ip[32];                                                       // Buffer pour stocker l'adresse IP
  status = esp01_get_current_ip(ip, sizeof(ip));                     // Récupération de l'adresse IP actuelle
  printf("[TEST][INFO] Adresse IP: %s\r\n", ip);                     // Affichage de l'adresse IP récupérée
  HAL_Delay(500);                                                    // Attente pour laisser le temps à l'affichage de se stabiliser

  /* Configuration NTP */
  printf("\r\n[TEST][INFO] === Configuration NTP ===\r\n");                                        // Affichage du message de configuration NTP
  status = esp01_configure_ntp(NTP_SERVER, NTP_TIMEZONE, NTP_UPDATE_PERIOD_S, true);               // Configuration des paramètres NTP
  printf("\r\n[TEST][INFO] Configuration paramètres NTP: %s\r\n", esp01_get_error_string(status)); // Affichage du statut de la configuration NTP
  if (status != ESP01_OK)                                                                          // Vérification du statut de configuration NTP
  {
    printf("[TEST][ERROR] Échec configuration NTP\r\n"); // Affichage d'une erreur si la configuration a échoué
    Error_Handler();                                     // Appel de la fonction d'erreur
  }
  HAL_Delay(500); // Attente pour laisser le temps à la configuration de se stabiliser

  /* Démarrage de la synchronisation NTP */
  status = esp01_ntp_start_sync(false); // Démarrage de la synchronisation NTP en mode one-shot
  if (status != ESP01_OK)               // Vérification du statut de démarrage de la synchronisation NTP
  {
    printf("[TEST][ERROR] Échec démarrage synchronisation NTP\r\n"); // Affichage d'une erreur si le démarrage a échoué
    Error_Handler();                                                 // Appel de la fonction d'erreur
  }
  else // Si le démarrage de la synchronisation NTP a réussi
  {
    printf("[TEST][INFO] Synchronisation NTP: %s\r\n", esp01_get_error_string(status)); // Affichage du statut de la synchronisation NTP
    printf("\r\n");                                                                     // Ligne vide pour séparer les messages
    // Affichage de la date/heure formaté en FR
    char date_fr[64];                                                              // Buffer pour stocker la date/heure formatée en français
    if (esp01_ntp_format_last_datetime('F', date_fr, sizeof(date_fr)) == ESP01_OK) // Formatage de la date/heure en français
    {
      printf("[TEST][INFO] Date/heure FR : %s\r\n", date_fr); // Affichage de la date/heure formatée en français
    }
    else // Si le formatage a échoué
    {
      printf("[TEST][WARN] Impossible de formater la date/heure FR\r\n"); // Affichage d'un avertissement si le formatage a échoué
    }
    printf("\r\n"); // Ligne vide pour séparer les messages

    // Affichage de la date/heure formaté en EN
    char date_en[64];                                                              // Buffer pour stocker la date/heure formatée en anglais
    if (esp01_ntp_format_last_datetime('E', date_en, sizeof(date_en)) == ESP01_OK) // Formatage de la date/heure en anglais
    {
      printf("[TEST][INFO] Date/heure EN : %s\r\n", date_en); // Affichage de la date/heure formatée en anglais
    }
    else // Si le formatage a échoué
    {
      printf("[TEST][WARN] Impossible de formater la date/heure EN\r\n"); // Affichage d'un avertissement si le formatage a échoué
    }
    printf("\r\n"); // Ligne vide pour séparer les messages

    // Affichage de la date/heure brute
    char date_brute[64];                                                               // Buffer pour stocker la date/heure brute
    if (esp01_ntp_format_last_datetime(0, date_brute, sizeof(date_brute)) == ESP01_OK) // Formatage de la date/heure brute
    {
      printf("[TEST][INFO] Date/heure brute formatée : %s\r\n", date_brute); // Affichage de la date/heure brute formatée
    }
    else // Si le formatage a échoué
    {
      printf("[TEST][WARN] Impossible de formater la date/heure brute\r\n"); // Affichage d'un avertissement si le formatage a échoué
    }
    printf("\r\n"); // Ligne vide pour séparer les messages
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    esp01_ntp_handle(); // Gère la synchronisation NTP en cours, vérifie si une nouvelle date/heure est disponible
    HAL_Delay(100);     // Attente de 100 ms pour éviter une surcharge CPU

    // N'affiche rien si la synchro périodique n'est pas active
    if (esp01_ntp_is_periodic_enabled() && esp01_ntp_is_updated()) // Vérifie si la synchronisation périodique est active et si une nouvelle date/heure est disponible
    {
      ntp_datetime_t dt_brut;                                       // Structure pour stocker la date/heure brute
      if (esp01_ntp_get_last_datetime_struct(&dt_brut) == ESP01_OK) //    Récupère la date/heure brute
      {
        printf("[TEST][INFO] Date/heure brute périodique : %04d-%02d-%02d %02d:%02d:%02d (wday=%d)\r\n",
               dt_brut.year, dt_brut.month, dt_brut.day, dt_brut.hour, dt_brut.min, dt_brut.sec, dt_brut.wday); // Affiche la date/heure brute périodique
      }
      char ntp_str[100];                                                             // Buffer pour stocker la date/heure formatée NTP
      if (esp01_ntp_format_last_datetime('F', ntp_str, sizeof(ntp_str)) == ESP01_OK) // Formate la date/heure en français
      {
        printf("[TEST][INFO] Date/heure NTP : %s\r\n", ntp_str); // Affiche la date/heure NTP formatée en français
      }
      else // Si le formatage a échoué
      {
        printf("[TEST][WARN] Impossible de formater la date/heure NTP\r\n"); // Affiche un avertissement si le formatage a échoué
      }
      esp01_ntp_clear_updated_flag(); // Réinitialise le drapeau de mise à jour NTP
    }
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
