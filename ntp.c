/* USER CODE BEGIN Includes */
#include <stdio.h>		   // Inclusion de la bibliothèque standard pour les entrées/sorties (pour printf)
#include "STM32_WifiESP.h" // Inclusion du fichier d'en-tête pour le driver ESP01
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_NTP.h"
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
/* USER CODE END 0 */

/* USER CODE BEGIN 2 */
HAL_Delay(1000); // Attente d'une seconde
printf("[ESP01] === Démarrage du programme ===\r\n");
HAL_Delay(500); // Attente de 500 ms

ESP01_Status_t status; // Variable pour stocker le statut des fonctions ESP01

// 1. Initialisation du driver ESP01
printf("[ESP01] === Initialisation du driver ESP01 ===\r\n");
status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE); // Initialise le driver ESP01
printf("[ESP01] >>> Initialisation du driver ESP01 : %s\r\n", esp01_get_error_string(status));

// 2. Flush du buffer RX
printf("[ESP01] === Flush RX Buffer ===\r\n");
status = esp01_flush_rx_buffer(500); // Vide le buffer RX
printf("[ESP01] >>> Buffer UART/DMA vidé : %s\r\n", esp01_get_error_string(status));
HAL_Delay(100); // Attente de 100 ms

// 3. Test de communication AT
printf("[ESP01] === Test de communication AT ===\r\n");
status = esp01_test_at(); // Teste la communication AT
printf("[ESP01] >>> Test AT : %s\r\n", esp01_get_error_string(status));
HAL_Delay(100); // Attente de 100 ms

// 4. Test de version AT+GMR
printf("[ESP01] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
char at_version[64];										   // Tampon pour stocker la version AT
status = esp01_get_at_version(at_version, sizeof(at_version)); // Récupère la version AT
printf("[ESP01] >>> Version ESP01 : %s\r\n", at_version);
HAL_Delay(100); // Attente de 100 ms

// 5. Test du scan WiFi
printf("[TEST] === Test du scan WiFi ===\r\n");
esp01_print_wifi_networks(10); // Scanne les réseaux WiFi disponibles
HAL_Delay(100);				   // Attente de 100 ms

// 6. Connexion au réseau WiFi
printf("[ESP01] === Connexion au réseau WiFi \"%s\" ===\r\n", SSID);
status = esp01_connect_wifi_config(ESP01_WIFI_MODE_STA, SSID, PASSWORD, true, NULL, NULL, NULL);
printf("[ESP01] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(status));
if (status != ESP01_OK)
{
	printf("[ESP01] !!! Échec de la connexion WiFi, arrêt du programme.\r\n");
	while (1)
		; // Stoppe tout si la connexion échoue
}
HAL_Delay(500);

// 7. NTP
printf("[NTP] === NTP ===\r\n");

// Synchronisation NTP une fois
esp01_ntp_sync_once("pool.ntp.org", 2, true); // true = affichage FR
HAL_Delay(1000);							  // Attente de 1 seconde pour laisser le temps à la synchro NTP
// Démarre la synchro NTP toutes les 60 secondes (par exemple)
esp01_ntp_start_periodic_sync("pool.ntp.org", 2, 60, true); // true = affichage FR
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
	esp01_ntp_periodic_task();
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