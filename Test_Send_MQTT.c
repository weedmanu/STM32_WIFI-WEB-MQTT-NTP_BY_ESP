/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : Test_Send_MQTT.c
 * @brief          : Programme de test d'envoi MQTT avec STM32 et ESP01
 ******************************************************************************
 * @details
 * Ce programme teste l'envoi de messages MQTT depuis un STM32 connecté à
 * un module ESP01. Il exécute séquentiellement les opérations suivantes :
 *
 * - Initialisation du driver ESP01
 * - Vidage du buffer de réception
 * - Connexion au réseau WiFi
 * - Configuration du mode connexion unique (CIPMUX=0)
 * - Récupération et affichage de l'adresse IP
 * - Connexion à un broker MQTT local (avec ID client généré dynamiquement)
 * - Publication d'un premier message MQTT
 * - Envoi d'un PING pour maintenir la connexion
 * - Publication d'un second message MQTT
 * - Déconnexion propre du broker MQTT
 *
 * Le programme affiche tous les résultats sur la console série et gère
 * les cas d'erreur avec plusieurs tentatives de connexion au broker.
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
 * Paramètres MQTT :
 * - Broker : 192.168.1.185:1883 (broker local)
 * - Client ID : stmXXXX (généré dynamiquement)
 * - Topic : stm32/test
 * - QoS : 0
 * - Messages publiés :
 *   1. "Hello World !!!"
 *   2. "I am a STM32 with wifi now !!!"
 *
 * @note
 * - Nécessite les modules STM32_WifiESP.h/.c et STM32_WifiESP_MQTT.h/.c
 * - Compatible avec les modules ESP8266 (ESP01, ESP01S, etc.)
 * - Baudrate par défaut: 115200 bps
 * - Programme démonstratif prévu pour s'exécuter une seule fois
 * - Nécessite un broker MQTT actif sur le réseau local
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>				// Pour printf, snprintf, etc.
#include "STM32_WifiESP.h"		// Fonctions du driver ESP01
#include "STM32_WifiESP_HTTP.h" // Fonctions HTTP haut niveau
#include "STM32_WifiESP_MQTT.h" // Fonctions MQTT haut niveau
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SSID "XXXXXXXX"				  // Nom du réseau WiFi
#define PASSWORD "XXXXXXXXXXXXXXXXXX" // Mot de passe du réseau WiFi
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
	HAL_Delay(1000); // Attente d'une seconde
	printf("\n[ESP01] === Démarrage du programme ===\r\n");
	HAL_Delay(500);						// Attente de 500 ms
	ESP01_Status_t status;				// Variable pour stocker le statut des fonctions ESP01
	char broker_ip[] = "192.168.1.185"; // Adresse IP du broker MQTT
	uint16_t broker_port = 1883;		// Port du broker MQTT
	char client_id[9];					// Buffer pour l'ID client MQTT

	// 1. Initialisation du driver ESP01
	printf("\n[ESP01] === Initialisation du driver ESP01 ===\r\n");
	status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE); // Initialise le driver ESP01
	printf("[ESP01] >>> Initialisation du driver ESP01 : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(250); // Attente de 250 ms

	// 2. Flush du buffer RX
	printf("\n[ESP01] === Flush RX Buffer ===\r\n");
	status = esp01_flush_rx_buffer(500); // Vide le buffer RX
	printf("[ESP01] >>> Buffer UART/DMA vidé : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(250); // Attente de 250 ms

	// 3. Connexion au réseau WiFi
	printf("[WIFI] === Connexion au réseau WiFi ===\r\n");
	status = esp01_connect_wifi_config(
		ESP01_WIFI_MODE_STA, // mode station
		SSID,				 // ssid du réseau
		PASSWORD,			 // mot de passe du réseau
		true,				 // utilisation du DHCP
		NULL,				 // ip statique (NULL = DHCP)
		NULL,				 // gateway (NULL = DHCP)
		NULL				 // netmask (NULL = DHCP)
	);
	printf("[WIFI] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(250); // Attente de 250 ms

	// 4. Configuration du mode connexion unique (pour MQTT)
	printf("\n[ESP01] === Configuration mode connexion unique ===\r\n");
	char resp[ESP01_DMA_RX_BUF_SIZE];																		  // Tampon pour la réponse AT
	ESP01_Status_t server_status = esp01_send_raw_command_dma("AT+CIPMUX=0", resp, sizeof(resp), "OK", 3000); // Configure le mode connexion unique
	if (server_status != ESP01_OK)
	{
		printf("[ESP01] >>> ERREUR: AT+CIPMUX (peut être ignoré si déjà en mode single)\r\n");
		// Optionnel : continuer sans Error_Handler();
	}
	else
	{
		printf("[ESP01] >>> Mode connexion unique activé\r\n");
	}

	// 5. Affichage de l'adresse IP
	char ip[32];										  // Tampon pour l'adresse IP
	if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK) // Récupère l'adresse IP actuelle
	{
		printf("[WIFI] >>> Adresse IP actuelle : %s\r\n", ip);
	}
	else
	{
		printf("[WIFI] >>> Impossible de récupérer l'IP\r\n");
	}
	HAL_Delay(250); // Attente de 250 ms

	esp01_flush_rx_buffer(250); // Vide le buffer RX du module ESP01
	// esp01_mqtt_disconnect(); // <-- Laisser commenté pour éviter le double CIPCLOSE

	snprintf(client_id, sizeof(client_id), "stm%04X", (unsigned int)(HAL_GetTick() % 0xFFFF)); // Génère un ID unique
	printf("[MQTT] Connexion au broker MQTT %s:%d avec ID %s...\r\n", broker_ip, broker_port, client_id);

	int retry = 0; // Compteur de tentatives
	while (retry < 3)
	{
		status = esp01_mqtt_connect(broker_ip, broker_port, client_id, NULL, NULL); // Tente la connexion MQTT
		if (status == ESP01_OK)
			break;
		printf("[MQTT] Échec de connexion, tentative %d/3\r\n", retry + 1);
		retry++;
		HAL_Delay(1000); // Attente avant nouvelle tentative
	}

	if (status == ESP01_OK)
	{
		printf("[MQTT] Connexion établie avec succès\r\n");
		HAL_Delay(1000);

		// Premier message
		const char *message = "Hello World !!!"; // Message à publier
		printf("[MQTT] Publication: %s\r\n", message);
		status = esp01_mqtt_publish("stm32/test", message, 0, false); // Publication sur le topic

		if (status == ESP01_OK)
		{
			printf("[MQTT] Message publié avec succès\r\n");
			HAL_Delay(1000);

			// PINGREQ
			printf("[MQTT] Envoi PINGREQ...\r\n");
			status = esp01_mqtt_ping(); // Envoie un ping MQTT
			HAL_Delay(1000);

			// Deuxième message
			const char *message2 = "I am a STM32 with wifi now !!!";
			printf("[MQTT] Publication second message: %s\r\n", message2);
			status = esp01_mqtt_publish("stm32/test", message2, 0, false);

			if (status == ESP01_OK)
			{
				printf("[MQTT] Second message publié avec succès\r\n");
				HAL_Delay(1000);
			}
		}

		printf("[MQTT] Déconnexion du broker MQTT...\r\n");
		status = esp01_mqtt_disconnect(); // Déconnexion du broker
		printf("[MQTT] Déconnexion: %s\r\n", esp01_get_error_string(status));
	}
	else
	{
		printf("[MQTT] Échec de la connexion au broker MQTT après 3 tentatives\r\n");
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
