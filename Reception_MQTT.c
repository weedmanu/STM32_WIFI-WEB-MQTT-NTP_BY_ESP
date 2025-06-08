/* USER CODE BEGIN Includes */
#include <stdio.h>		   // Inclusion de la bibliothèque standard pour les entrées/sorties (pour printf)
#include "STM32_WifiESP.h" // Inclusion du fichier d'en-tête pour le driver ESP01
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_MQTT.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define SSID "XXXXXX"	  // Nom du réseau WiFi auquel se connecter (masqué)
#define PASSWORD "YYYYYY" // Mot de passe du réseau WiFi (masqué)
#define LED_GPIO_PORT GPIOA
#define LED_GPIO_PIN GPIO_PIN_5
/* USER CODE END PD */

/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Tampon DMA pour la réception ESP01
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
// Redirige printf vers l'UART2 (console série)
int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF); // Envoie le caractère sur l'UART2
	return ch;											   // Retourne le caractère envoyé (pour compatibilité printf)
}
// Callback appelé à chaque message MQTT reçu
void mqtt_message_callback(const char *topic, const char *payload)
{
	printf("[MQTT] Message reçu sur %s : %s\r\n", topic, payload);
}
/* USER CODE END 0 */

/* USER CODE BEGIN 2 */
HAL_Delay(1000);
printf("[ESP01] === Démarrage du programme ===\r\n");
HAL_Delay(500);

ESP01_Status_t status;

// 1. Initialisation du driver ESP01
printf("[ESP01] === Initialisation du driver ESP01 ===\r\n");
status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
printf("[ESP01] >>> Initialisation du driver ESP01 : %s\r\n", esp01_get_error_string(status));

// 2. Flush du buffer RX
printf("[ESP01] === Flush RX Buffer ===\r\n");
status = esp01_flush_rx_buffer(500);
printf("[ESP01] >>> Buffer UART/DMA vidé : %s\r\n", esp01_get_error_string(status));
HAL_Delay(100);

// 3. test de communication AT
printf("[ESP01] === Test de communication AT ===\r\n");
status = esp01_test_at();
printf("[ESP01] >>> Test AT : %s\r\n", esp01_get_error_string(status));

// 4. Connexion au réseau WiFi
printf("[WIFI] === Connexion au réseau WiFi ===\r\n");
status = esp01_connect_wifi_config(
	ESP01_WIFI_MODE_STA,
	SSID,
	PASSWORD,
	true,
	NULL,
	NULL,
	NULL);
printf("[WIFI] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(status));

// 5. Configuration du mode connexion unique (pour MQTT)
printf("[ESP01] === Configuration mode connexion unique ===\r\n");
char resp[ESP01_DMA_RX_BUF_SIZE];
ESP01_Status_t server_status = esp01_send_raw_command_dma("AT+CIPMUX=0", resp, sizeof(resp), "OK", 3000);
if (server_status != ESP01_OK)
{
	printf("[ESP01] >>> ERREUR: AT+CIPMUX\r\n");
	Error_Handler();
}
else
{
	printf("[ESP01] >>> Mode connexion unique activé\r\n");
}

// 6. Affichage de l'adresse IP
char ip[32];
if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
{
	printf("[WIFI] >>> Adresse IP actuelle : %s\r\n", ip);
}
else
{
	printf("[WIFI] >>> Impossible de récupérer l'IP\r\n");
}

// 7. Connexion au broker MQTT et abonnement
char broker_ip[] = "192.168.1.185"; // IP du broker MQTT
uint16_t broker_port = 1883;
char client_id[16];
snprintf(client_id, sizeof(client_id), "stm32_%lu", HAL_GetTick() & 0xFFFF);

status = esp01_mqtt_connect(broker_ip, broker_port, client_id, NULL, NULL);
printf("[MQTT] Connexion broker : %s\r\n", esp01_get_error_string(status));
if (status != ESP01_OK)
	Error_Handler();

const char *topic = "stm32/test";
status = esp01_mqtt_subscribe(topic, 0);
printf("[MQTT] Abonnement %s : %s\r\n", topic, esp01_get_error_string(status));
if (status != ESP01_OK)
	Error_Handler();

// 8. Enregistrement du callback de réception MQTT
esp01_mqtt_set_message_callback(mqtt_message_callback);
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
	esp01_mqtt_poll(); // Appelle le callback si un message est reçu
	HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
	HAL_Delay(1000);
	/* USER CODE END WHILE */

	/* USER CODE BEGIN Error_Handler_Debug */
	printf("ERREUR SYSTÈME DÉTECTÉE!\r\n"); // Affiche une erreur système
	__disable_irq();						// Désactive les interruptions
	while (1)
	{
		// Boucle infinie en cas d'erreur
	}
/* USER CODE END Error_Handler_Debug */