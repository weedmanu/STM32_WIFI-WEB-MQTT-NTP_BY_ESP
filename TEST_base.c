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
#include <stdio.h>		   // Pour printf, snprintf, etc.
#include "STM32_WifiESP.h" // Fonctions du driver ESP01
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Pas de typedef utilisateur spécifique ici
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SSID "freeman"				  // Nom du réseau WiFi
#define PASSWORD "manu2612@SOSSO1008" // Mot de passe du réseau WiFi
#define LED_GPIO_PORT GPIOA			  // Port GPIO de la LED
#define LED_GPIO_PIN GPIO_PIN_5		  // Pin GPIO de la LED
#define NTP_PERIOD_S 20				  // Par exemple, 10 secondes entre chaque synchro
#define MY_DMA_RX_BUF_SIZE 8192
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
	ESP01_Status_t status;
	char buf[256];

	// 1. Initialisation du driver
	printf("[ESP01] === Initialisation du driver ESP01 ===\r\n");
	status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
	printf("[ESP01] >>> Initialisation : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 2. Test AT
	printf("[ESP01] === Test AT ===\r\n");
	status = esp01_test_at();
	printf("[ESP01] >>> Test AT : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 3. Reset logiciel
	printf("[ESP01] === Reset logiciel (AT+RST) ===\r\n");
	status = esp01_reset();
	printf("[ESP01] >>> Reset : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(1000);

	// 4. Restore usine
	printf("[ESP01] === Restore usine (AT+RESTORE) ===\r\n");
	status = esp01_restore();
	printf("[ESP01] >>> Restore : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(2000);

	// 5. Version firmware
	printf("[ESP01] === Lecture version firmware ===\r\n");
	status = esp01_get_at_version(buf, sizeof(buf));
	printf("[ESP01] >>> Version : %s\r\n", esp01_get_error_string(status));
	printf("%s\r\n", buf);

	// 6. Lecture config UART
	printf("[ESP01] === Lecture config UART ===\r\n");
	status = esp01_get_uart_config(buf, sizeof(buf));
	printf("[ESP01] >>> UART config brute : %s (%s)\r\n", esp01_get_error_string(status), buf);
	char uart_str[128];
	if (status == ESP01_OK)
	{
		esp01_uart_config_to_string(buf, uart_str, sizeof(uart_str));
		printf("[ESP01] >>> UART config lisible : %s\r\n", uart_str);
	}

	// 7. Changement config UART (exemple, à adapter selon ton besoin)
	// status = esp01_set_uart_config(115200, 8, 1, 0, 0);
	// printf("[ESP01] >>> Set UART config : %s\r\n", esp01_get_error_string(status));

	// 8. Lecture mode sommeil
	int sleep_mode = 0; // <-- Ajoute cette ligne AVANT d'utiliser sleep_mode
	status = esp01_get_sleep_mode(&sleep_mode);
	printf("[ESP01] === Mode sommeil : %s (%d)\r\n", esp01_get_error_string(status), sleep_mode);
	char sleep_str[64];
	esp01_sleep_mode_to_string(sleep_mode, sleep_str, sizeof(sleep_str));
	printf("[ESP01] >>> Mode sommeil lisible : %s\r\n", sleep_str);

	// 9. Changement mode sommeil (exemple)
	// status = esp01_set_sleep_mode(0);
	// printf("[ESP01] >>> Set sleep mode : %s\r\n", esp01_get_error_string(status));

	// 10. Lecture puissance RF
	int rf_dbm = 0;
	status = esp01_get_rf_power(&rf_dbm);
	printf("[ESP01] === Puissance RF : %s (%d dBm)\r\n", esp01_get_error_string(status), rf_dbm);

	// 11. Changement puissance RF (exemple)
	// status = esp01_set_rf_power(82);
	// printf("[ESP01] >>> Set RF power : %s\r\n", esp01_get_error_string(status));

	// 12. Lecture niveau log système
	int syslog = 0;
	status = esp01_get_syslog(&syslog);
	printf("[ESP01] === Niveau log système : %s (%d)\r\n", esp01_get_error_string(status), syslog);
	char syslog_str[32];
	esp01_syslog_to_string(syslog, syslog_str, sizeof(syslog_str));
	printf("[ESP01] >>> Niveau log lisible : %s\r\n", syslog_str);

	// 13. Changement niveau log (exemple)
	// status = esp01_set_syslog(0);
	// printf("[ESP01] >>> Set syslog : %s\r\n", esp01_get_error_string(status));

	// 14. Lecture RAM libre
	uint32_t free_ram = 0;
	status = esp01_get_sysram(&free_ram);
	printf("[ESP01] === RAM libre : %s (%lu octets)\r\n", esp01_get_error_string(status), free_ram);

	// 15. Deep sleep (exemple, attention le module ne répondra plus pendant la durée)
	// status = esp01_deep_sleep(2000);
	// printf("[ESP01] >>> Deep sleep : %s\r\n", esp01_get_error_string(status));

	char cmd_list[8192];
	status = esp01_get_cmd_list(cmd_list, sizeof(cmd_list));
	printf("[ESP01] === Liste des commandes AT : %s\r\n", esp01_get_error_string(status));
	printf("%s\r\n", cmd_list);
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
