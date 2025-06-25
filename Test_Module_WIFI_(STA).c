/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : Test_Module_WIFI.c
 * @brief          : Programme de test du module WIFI de la bibliothèque STM32_WifiESP
 ******************************************************************************
 * @details
 * Ce programme teste systématiquement les fonctions WiFi de la bibliothèque
 * STM32_WifiESP pour module ESP01 (ESP8266). Il teste successivement :
 *
 * - Mode Station (STA) :
 *   - Configuration et lecture du mode WiFi
 *   - Configuration et vérification du DHCP
 *   - Configuration et lecture du hostname
 *   - Scan des réseaux WiFi disponibles
 *   - Connexion à un réseau WiFi spécifié
 *   - Vérification de la connexion (CWJAP? et CWSTATE)
 *   - Récupération de l'adresse IP et MAC
 *   - Lecture du niveau de signal (RSSI)
 *   - Vérification des connexions TCP/IP
 *   - Test de connectivité (ping)
 *   - Déconnexion du réseau WiFi
 *
 * - Mode Point d'accès (AP) :
 *   - Configuration du mode AP
 *   - Démarrage d'un point d'accès
 *   - Récupération et affichage de la configuration AP
 *   - Attente de connexions clientes
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
 * - Nécessite le driver STM32_WifiESP.h/.c et STM32_WifiESP_WIFI.h/.c
 * - Compatible avec les modules ESP8266 (ESP01, ESP01S, etc.)
 * - Baudrate par défaut: 115200 bps
 * - Définir les constantes SSID/PASSWORD pour votre réseau WiFi avant compilation
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
#define SSID "freeman"				  // Nom du réseau WiFi
#define PASSWORD "manu2612@SOSSO1008" // Mot de passe du réseau WiFi
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
	HAL_Delay(500);
	/* USER CODE BEGIN 2 */
	ESP01_Status_t status;
	char buf[256];
	char ip[ESP01_MAX_IP_LEN];
	char gw[ESP01_MAX_IP_LEN];
	char mask[ESP01_MAX_IP_LEN];
	char mac[ESP01_MAX_MAC_LEN];
	char hostname[ESP01_MAX_HOSTNAME_LEN];
	int rssi = 0;
	bool dhcp_enabled = false;
	uint8_t wifi_mode = 0;
	char resp[256];
	printf("\n[TEST][INFO] === Test du mode station (STA) ===\r\n");
	HAL_Delay(500);
	// 0. Initialisation
	printf("\n[TEST][INFO] === Initialisation du driver ESP01 ===\r\n");
	status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
	printf("[TEST][INFO] Initialisation du driver : %s\r\n", esp01_get_error_string(status));
	if (status != ESP01_OK)
	{
		printf("[TEST][ERROR] Échec de l'initialisation du driver\r\n");
		Error_Handler();
	}
	HAL_Delay(500);

	// 1. Scan des réseaux WiFi
	printf("\n[TEST][INFO] === Scan des réseaux WiFi ===\r\n");
	esp01_network_t networks[8];
	uint8_t found = 0;
	status = esp01_scan_networks(networks, 8, &found);
	printf("[TEST][INFO] Résultat du scan : %s (%d réseaux trouvés)\r\n", esp01_get_error_string(status), found);
	for (uint8_t i = 0; i < found; ++i)
	{
		esp01_network_to_string(&networks[i], buf, sizeof(buf));
		printf("[TEST][INFO]   %d. %s\r\n", i + 1, buf);
	}
	HAL_Delay(500);

	// 2. Configuration du mode STA
	printf("\n[TEST][INFO] === Configuration du mode WiFi (STA) ===\r\n");
	status = esp01_set_wifi_mode(1);
	printf("[TEST][INFO] Configuration mode STA : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 3. Lecture du mode WiFi
	printf("\n[TEST][INFO] === Lecture du mode WiFi actuel ===\r\n");
	status = esp01_get_wifi_mode(&wifi_mode);
	printf("[TEST][INFO] Mode WiFi actuel : %s (%d)\r\n", esp01_wifi_mode_to_string(wifi_mode), wifi_mode);
	HAL_Delay(500);

	// 4. Activation du DHCP
	printf("\n[TEST][INFO] === Activation du DHCP ===\r\n");
	status = esp01_set_dhcp(true);
	printf("[TEST][INFO] Activation DHCP : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 5. Lecture de l'état DHCP
	printf("\n[TEST][INFO] === Lecture de l'état DHCP ===\r\n");
	status = esp01_get_dhcp(&dhcp_enabled);
	printf("[TEST][INFO] DHCP : %s\r\n", dhcp_enabled ? "Activé" : "Désactivé");
	HAL_Delay(500);

	// 6. Configuration du hostname
	printf("\n[TEST][INFO] === Configuration du hostname ===\r\n");
	status = esp01_set_hostname("ESP-TEST");
	printf("[TEST][INFO] Configuration hostname : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 7. Lecture du hostname
	printf("\n[TEST][INFO] === Lecture du hostname actuel ===\r\n");
	status = esp01_get_hostname(hostname, sizeof(hostname));
	printf("[TEST][INFO] Hostname actuel : %s\r\n", (status == ESP01_OK) ? hostname : esp01_get_error_string(status));
	HAL_Delay(500);

	// 8. Connexion au réseau WiFi
	printf("\n[TEST][INFO] === Connexion au réseau WiFi ===\r\n");
	status = esp01_connect_wifi(SSID, PASSWORD);
	printf("[TEST][INFO] Connexion WiFi : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 9. Lecture de la configuration IP complète
	printf("\n[TEST][INFO] === Lecture de la configuration IP complète ===\r\n");
	status = esp01_get_ip_config(ip, sizeof(ip), gw, sizeof(gw), mask, sizeof(mask));
	if (status == ESP01_OK)
		printf("[TEST][INFO] IP: %s, Gateway: %s, Masque: %s\r\n", ip, gw, mask);
	else
		printf("[TEST][INFO] Erreur IP : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 10. Récupération de l'adresse IP actuelle
	printf("\n[TEST][INFO] === Récupération de l'adresse IP actuelle ===\r\n");
	if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
	{
		printf("[TEST][INFO] IP actuelle : %s\r\n", ip);
	}
	else
	{
		printf("[TEST][INFO] Erreur lors de la récupération de l'adresse IP actuelle : %s\r\n", esp01_get_error_string(status));
	}

	// 11. Lecture de l'adresse MAC
	printf("\n[TEST][INFO] === Récupération de l'adresse MAC ===\r\n");
	status = esp01_get_mac(mac, sizeof(mac));
	printf("[TEST][INFO] Adresse MAC : %s\r\n", (status == ESP01_OK) ? mac : esp01_get_error_string(status));
	HAL_Delay(500);

	// 12. Lecture du RSSI
	printf("\n[TEST][INFO] === Récupération du niveau de signal (RSSI) ===\r\n");
	status = esp01_get_rssi(&rssi);
	printf("[TEST][INFO] Niveau de signal : %s\r\n", (status == ESP01_OK) ? esp01_rf_power_to_string(rssi) : esp01_get_error_string(status));
	HAL_Delay(500);

	// 13. Lecture du statut TCP/IP
	printf("\n[TEST][INFO] === Statut des connexions TCP/IP ===\r\n");
	status = esp01_get_tcp_status(buf, sizeof(buf));
	printf("[TEST][INFO] Statut TCP/IP : %s\r\n", (status == ESP01_OK) ? esp01_tcp_status_to_string(buf) : esp01_get_error_string(status));
	HAL_Delay(500);

	// 14. Ping
	printf("\n[TEST][INFO] === Test de ping vers 8.8.8.8 (google) ===\r\n");
	int ping_time = 0;
	status = esp01_ping("8.8.8.8", &ping_time);
	if (status == ESP01_OK)
		printf("[TEST][INFO] Ping 8.8.8.8 : %d ms\r\n", ping_time);
	else
		printf("[TEST][INFO] Ping échoué : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 15. Etat de la connexion WiFi (CWJAP?)
	printf("\n[TEST][INFO] === État de la connexion WiFi (CWJAP?) ===\r\n");
	status = esp01_get_wifi_connection(resp, sizeof(resp));
	printf("[TEST][INFO] État de la connexion : %s\r\n", (status == ESP01_OK) ? esp01_connection_status_to_string(resp) : esp01_get_error_string(status));
	HAL_Delay(500);

	// 16. Etat de la connexion WiFi (CWSTATE)
	printf("\n[TEST][INFO] === État de la connexion WiFi (CWSTATE) ===\r\n");
	status = esp01_get_wifi_state(resp, sizeof(resp));
	printf("[TEST][INFO] État détaillé : %s\r\n", (status == ESP01_OK) ? esp01_cwstate_to_string(resp) : esp01_get_error_string(status));
	HAL_Delay(500);

	// 17. Déconnexion du WiFi
	printf("\n[TEST][INFO] === Déconnexion du réseau WiFi ===\r\n");
	status = esp01_disconnect_wifi();
	printf("[TEST][INFO] Déconnexion : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 18. Vérification du statut après déconnexion
	printf("\n[TEST][INFO] === Vérification du statut après déconnexion ===\r\n");
	status = esp01_get_connection_status();
	printf("[TEST][INFO] Statut après déconnexion : %s\r\n", (status == ESP01_OK) ? "Connecté" : "Déconnecté");
	HAL_Delay(500);

	// 19. Connexion STA config IP fixe
	printf("\n[TEST][INFO] === Connexion STA config IP fixe ===\r\n");
	const char *ip_fix = "192.168.1.152";
	const char *gw_fix = "192.168.1.254";
	const char *mask_fix = "255.255.255.0";
	status = esp01_connect_wifi_config(ESP01_WIFI_MODE_STA, SSID, PASSWORD, false, ip_fix, gw_fix, mask_fix);
	printf("[TEST][INFO] Connexion STA IP fixe : %s\r\n", esp01_get_error_string(status));
	if (status == ESP01_OK)
	{
		// Récupère la réponse brute
		if (esp01_get_wifi_connection(resp, sizeof(resp)) == ESP01_OK)
			printf("[TEST][INFO] Statut après co STA IP fixe :\n%s", esp01_connection_status_to_string(resp));
		else
			printf("[TEST][INFO] Statut après co STA IP fixe : Connecté (détail indisponible)\r\n");
	}
	else
	{
		printf("[TEST][INFO] Échec de la connexion STA avec IP fixe : %s\r\n", esp01_get_error_string(status));
	}
	HAL_Delay(500);

	// 20. Lecture de la configuration IP complète
	printf("\n[TEST][INFO] === Lecture de la configuration IP complète ===\r\n");
	status = esp01_get_ip_config(ip, sizeof(ip), gw, sizeof(gw), mask, sizeof(mask));
	if (status == ESP01_OK)
		printf("[TEST][INFO] IP: %s, Gateway: %s, Masque: %s\r\n", ip, gw, mask);
	else
		printf("[TEST][INFO] Erreur IP : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 21. Récupération de l'adresse IP actuelle
	printf("\n[TEST][INFO] === Récupération de l'adresse IP actuelle ===\r\n");
	if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
	{
		printf("[TEST][INFO] IP actuelle : %s\r\n", ip);
	}
	else
	{
		printf("[TEST][INFO] Erreur lors de la récupération de l'adresse IP actuelle : %s\r\n", esp01_get_error_string(status));
	}

	// 22. Déconnexion du WiFi
	printf("\n[TEST][INFO] === Déconnexion du réseau WiFi ===\r\n");
	status = esp01_disconnect_wifi();
	printf("[TEST][INFO] Déconnexion : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 23. Vérification du statut après déconnexion
	printf("\n[TEST][INFO] === Vérification du statut après déconnexion ===\r\n");
	status = esp01_get_connection_status();
	printf("[TEST][INFO] Statut après déconnexion : %s\r\n", (status == ESP01_OK) ? "Connecté" : "Déconnecté");
	HAL_Delay(500);

	printf("\n[TEST][INFO] === Fin des tests WiFi STA ===\r\n");
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
