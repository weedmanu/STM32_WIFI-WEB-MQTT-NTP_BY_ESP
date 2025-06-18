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
#include <stdio.h> // Pour printf, snprintf, etc.
#include <string.h>
#include "STM32_WifiESP.h"		// Fonctions du driver ESP01
#include "STM32_WifiESP_WIFI.h" // Fonctions WiFi haut niveau
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SSID "XXXXXX"				  // Nom du réseau WiFi
#define PASSWORD "XXXXXXXXXXXXXXXXXX" // Mot de passe du réseau WiFi
#define SSID_AP "STM32"				  // Nom du réseau WiFi
#define PASSWORD_AP "12345678"		  // Mot de passe du réseau WiFi
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
	return ch;											   // Retourne le caractère envoyé
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
	char buf[128], ip[ESP01_MAX_IP_LEN], mac[ESP01_MAX_MAC_LEN], hostname[ESP01_MAX_HOSTNAME_LEN];
	int mode = 0, rssi = 0;
	bool dhcp = false;
	uint8_t found = 0;
	esp01_network_t networks[ESP01_MAX_SCAN_NETWORKS];
	char resp[ESP01_MAX_RESP_BUF];

	printf("\n=== [TESTS DRIVER ESP01] Début des tests du module STM32_WifiESP_WIFI ===\n");

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

	printf("\n=== [WIFI STA] Test du mode station ===\n");

	// 1. Mode WiFi
	printf("=== [CWMODE] Configuration du mode WiFi ===\n");
	status = esp01_set_wifi_mode(ESP01_WIFI_MODE_STA);
	printf(">>> [CWMODE] Set : %s\n", esp01_get_error_string(status));
	HAL_Delay(500);

	printf("=== [CWMODE] Lecture du mode WiFi ===\n");
	status = esp01_get_wifi_mode(&mode);
	printf(">>> [CWMODE] Get : %s (%d)\n", esp01_wifi_mode_to_string(mode), mode);
	HAL_Delay(1000);

	// 2. DHCP
	printf("=== [CWDHCP] Configuration du DHCP ===\n");
	status = esp01_set_dhcp(true);
	printf(">>> [CWDHCP] Set : %s\n", esp01_get_error_string(status));
	HAL_Delay(500);

	printf("=== [CWDHCP] Lecture du DHCP ===\n");
	status = esp01_get_dhcp(&dhcp);
	printf(">>> [CWDHCP] Get : %s\n", dhcp ? "Activé" : "Désactivé");
	HAL_Delay(1000);

	// 3. Hostname
	printf("=== [CWHOSTNAME] Configuration du hostname ===\n");
	status = esp01_set_hostname("ESP-TEST");
	printf(">>> [CWHOSTNAME] Set : %s\n", esp01_get_error_string(status));
	HAL_Delay(500);

	printf("=== [CWHOSTNAME] Lecture du hostname ===\n");
	status = esp01_get_hostname(hostname, sizeof(hostname));
	printf(">>> [CWHOSTNAME] Get : %s\n", hostname);
	HAL_Delay(1000);

	// 4. Scan réseaux
	printf("=== [CWLAP] Scan des réseaux ===\n");
	status = esp01_scan_networks(networks, ESP01_MAX_SCAN_NETWORKS, &found);
	printf(">>> [CWLAP] Scan : %s (%d trouvés)\n", esp01_get_error_string(status), found);
	if (status == ESP01_OK)
	{
		for (uint8_t i = 0; i < found; ++i)
			printf("    SSID: %s, RSSI: %d, Sécu: %s\n", networks[i].ssid, networks[i].rssi, esp01_encryption_to_string(networks[i].encryption));
	}
	HAL_Delay(1000);

	// 5. Connexion WiFi
	printf("=== [CWJAP] Connexion WiFi ===\n");
	status = esp01_connect_wifi(SSID, PASSWORD);
	printf(">>> [CWJAP] Connexion : %s\n", esp01_get_error_string(status));
	HAL_Delay(1000);

	status = esp01_get_wifi_connection(resp, sizeof(resp));
	printf(">>> [CWJAP?] Statut : %s\n", (status == ESP01_OK) ? esp01_connection_status_to_string(resp) : esp01_get_error_string(status));
	HAL_Delay(1000);

	// 5b. Etat de la connexion WiFi (CWSTATE)
	printf("=== [CWSTATE] Etat de la connexion WiFi ===\n");
	status = esp01_get_wifi_state(resp, sizeof(resp));
	printf(">>> [CWSTATE] Etat : %s\n", (status == ESP01_OK) ? esp01_cwstate_to_string(resp) : esp01_get_error_string(status));
	HAL_Delay(1000);

	// 6. Adresse IP
	printf("=== [CIFSR] Adresse IP ===\n");
	status = esp01_get_current_ip(ip, sizeof(ip));
	printf(">>> [CIFSR] IP : %s\n", (status == ESP01_OK) ? ip : esp01_get_error_string(status));
	HAL_Delay(1000);

	// 7. Adresse MAC
	printf("=== [CIFSR] Adresse MAC ===\n");
	status = esp01_get_mac(mac, sizeof(mac));
	printf(">>> [CIFSR] MAC : %s\n", (status == ESP01_OK) ? mac : esp01_get_error_string(status));
	HAL_Delay(1000);

	// 8. RSSI
	printf("=== [CWJAP?] RSSI ===\n");
	status = esp01_get_rssi(&rssi);
	if (status == ESP01_OK)
		printf(">>> [CWJAP?] RSSI : %s\n", esp01_rf_power_to_string(rssi));
	else
		printf(">>> [CWJAP?] RSSI : %s\n", esp01_get_error_string(status));
	HAL_Delay(1000);

	// 9. Statut TCP/IP
	printf("=== [CIPSTATUS] Statut TCP/IP ===\n");
	status = esp01_get_tcp_status(buf, sizeof(buf));
	printf(">>> [CIPSTATUS] : %s\n", (status == ESP01_OK) ? esp01_tcp_status_to_string(buf) : esp01_get_error_string(status));
	HAL_Delay(1000);

	// 10. Ping
	printf("=== [PING] Test du ping ===\n");
	char ping_resp[ESP01_MAX_RESP_BUF] = {0};
	snprintf(buf, sizeof(buf), "AT+PING=\"8.8.8.8\"");
	status = esp01_send_raw_command_dma(buf, ping_resp, sizeof(ping_resp), "OK", 5000);
	printf(">>> [PING] : %s\n", esp01_ping_result_to_string(ping_resp));
	HAL_Delay(1000);

	// 11. Déconnexion WiFi
	printf("=== [CWQAP] Déconnexion WiFi ===\n");
	memset(resp, 0, sizeof(resp)); // Vide le buffer si besoin
	status = esp01_disconnect_wifi();
	printf(">>> [CWQAP] : %s\n", (status == ESP01_OK) ? esp01_cwqap_to_string(resp) : esp01_get_error_string(status));
	HAL_Delay(1000);

	printf("\n=== [WIFI AP] Test du mode point d'accès ===\n");

	// === [CWMODE] Configuration du mode AP ===
	printf("=== [CWMODE] Configuration du mode WiFi (AP) ===\n");
	status = esp01_set_wifi_mode(2); // 2 = AP
	printf(">>> [CWMODE] Set : %s\n", esp01_get_error_string(status));

	// === [CWSAP] Configuration AP ===
	printf("=== [CWSAP] Configuration AP ===\n");
	status = esp01_start_ap_config(SSID_AP, PASSWORD_AP, 1, 3);
	printf(">>> [CWSAP] Set : %s\n", esp01_get_error_string(status));

	// === [CWSAP?] Lecture de la config AP ===
	printf("=== [CWSAP?] Lecture de la config AP ===\n");
	status = esp01_get_ap_config(resp, sizeof(resp));
	printf(">>> [CWSAP?] : %s\n", (status == ESP01_OK) ? esp01_ap_config_to_string(resp) : esp01_get_error_string(status));
	HAL_Delay(10000);

	printf("=== [AP TEST] Fin du test AP ===\n");
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
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | LD2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PA0 LD2_Pin */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | LD2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
