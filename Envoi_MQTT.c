/* USER CODE BEGIN Includes */
#include <stdio.h>				 // Inclusion de la bibliothèque standard pour les entrées/sorties (pour printf)
#include "STM32_WifiESP.h"		 // Inclusion du fichier d'en-tête pour le driver ESP01
#include "STM32_WifiESP_Utils.h" // Inclusion des utilitaires du driver ESP01
#include "STM32_WifiESP_MQTT.h"	 // Inclusion des fonctions MQTT du driver ESP01
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define SSID "XXXXXX"			// Nom du réseau WiFi auquel se connecter
#define PASSWORD "YYYYYY"		// Mot de passe du réseau WiFi
#define LED_GPIO_PORT GPIOA		// Port GPIO pour la LED (ex: PA5)
#define LED_GPIO_PIN GPIO_PIN_5 // Pin GPIO pour la LED (ex: PA5)
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

// Fonction pour afficher les réseaux WiFi scannés
static void display_wifi_networks(void)
{
	esp01_network_t networks[10]; // Tableau pour stocker les réseaux trouvés
	uint8_t found_networks;		  // Nombre de réseaux trouvés

	printf("[WIFI] Démarrage du scan des réseaux WiFi...\r\n");

	if (esp01_scan_networks(networks, 10, &found_networks) == ESP01_OK) // Lance le scan WiFi
	{
		printf("[WIFI] %d réseaux WiFi trouvés:\r\n", found_networks);

		for (int i = 0; i < found_networks; i++) // Parcourt les réseaux trouvés
		{
			printf("  %d. SSID: %s\r\n", i + 1, networks[i].ssid); // Affiche le SSID
			printf("     Signal: %d dBm\r\n", networks[i].rssi);   // Affiche la puissance du signal
			printf("     Canal: %d\r\n", networks[i].channel);	   // Affiche le canal
			printf("     BSSID: %s\r\n", networks[i].bssid);	   // Affiche le BSSID
			printf("     Sécurité: ");

			switch (networks[i].encryption) // Affiche le type de sécurité
			{
			case 0:
				printf("Ouvert\r\n");
				break;
			case 1:
				printf("WEP\r\n");
				break;
			case 2:
				printf("WPA-PSK\r\n");
				break;
			case 3:
				printf("WPA2-PSK\r\n");
				break;
			case 4:
				printf("WPA/WPA2-PSK\r\n");
				break;
			default:
				printf("Inconnu\r\n");
				break;
			}
			printf("\r\n");
		}
	}
	else
	{
		printf("[WIFI] Échec du scan des réseaux WiFi\r\n"); // Affiche une erreur si le scan échoue
	}
}

// Fonction pour tester la publication MQTT
static void test_mqtt(void)
{
	ESP01_Status_t status;				// Variable pour stocker le statut des fonctions ESP01
	char broker_ip[] = "192.168.1.185"; // Adresse IP du broker MQTT
	uint16_t broker_port = 1883;		// Port du broker MQTT

	esp01_flush_rx_buffer(1000); // Vide le buffer avant d'envoyer la commande

	esp01_mqtt_disconnect(); // Déconnexion préalable pour partir d'un état propre
	HAL_Delay(1000);		 // Attente d'une seconde

	// Connexion directe au broker MQTT avec l'IP (sans résolution DNS)
	char client_id[9];																		   // ID client MQTT (max 8 caractères)
	snprintf(client_id, sizeof(client_id), "stm%04X", (unsigned int)(HAL_GetTick() % 0xFFFF)); // Génère un ID unique
	printf("[MQTT] Connexion au broker MQTT %s:%d avec ID %s...\r\n", broker_ip, broker_port, client_id);

	int retry = 0;	  // Compteur de tentatives
	while (retry < 3) // Essaye de se connecter jusqu'à 3 fois
	{
		status = esp01_mqtt_connect(broker_ip, broker_port, client_id, NULL, NULL); // Tente la connexion MQTT
		if (status == ESP01_OK)
		{
			break; // Sort de la boucle si la connexion réussit
		}
		printf("[MQTT] Échec de connexion, tentative %d/3\r\n", retry + 1); // Affiche l'échec
		retry++;
		HAL_Delay(1000); // Attente d'une seconde avant de réessayer
	}

	if (status == ESP01_OK)
	{
		printf("[MQTT] Connexion établie avec succès\r\n");
		HAL_Delay(1000);

		const char *message = "Hello World !!!"; // Premier message à publier
		printf("[MQTT] Publication: %s\r\n", message);
		status = esp01_mqtt_publish("stm32/test", message, 0, false); // Publie le message

		if (status == ESP01_OK)
		{
			printf("[MQTT] Message publié avec succès\r\n");
			HAL_Delay(1000);

			printf("[MQTT] Envoi PINGREQ...\r\n"); // Envoie un ping MQTT
			status = esp01_mqtt_ping();
			HAL_Delay(1000);

			const char *message2 = "I am a STM32 with wifi now !!!"; // Second message à publier
			printf("[MQTT] Publication second message: %s\r\n", message2);
			status = esp01_mqtt_publish("stm32/test", message2, 0, false); // Publie le second message

			if (status == ESP01_OK)
			{
				printf("[MQTT] Second message publié avec succès\r\n");
				HAL_Delay(1000);
			}
		}

		printf("[MQTT] Déconnexion du broker MQTT...\r\n"); // Déconnexion propre
		status = esp01_mqtt_disconnect();
		printf("[MQTT] Déconnexion: %s\r\n", esp01_get_error_string(status));
	}
	else
	{
		printf("[MQTT] Échec de la connexion au broker MQTT après 3 tentatives\r\n"); // Affiche l'échec final
	}
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

// 3. test de communication AT
printf("[ESP01] === Test de communication AT ===\r\n");
status = esp01_test_at(); // Teste la communication AT
printf("[ESP01] >>> Test AT : %s\r\n", esp01_get_error_string(status));

// 4. Test de version AT+GMR
printf("[ESP01] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
char at_version[64];										   // Tampon pour stocker la version AT
status = esp01_get_at_version(at_version, sizeof(at_version)); // Récupère la version AT
printf("[ESP01] >>> Version ESP01 : %s\r\n", at_version);

// 5. Test du scan WiFi
// printf("[TEST] === Test du scan WiFi ===\r\n");
// display_wifi_networks();

// 6. Connexion au réseau WiFi
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

// 7. Configuration du mode connexion unique (pour MQTT)
printf("[ESP01] === Configuration mode connexion unique ===\r\n");
char resp[ESP01_DMA_RX_BUF_SIZE];																		  // Tampon pour la réponse AT
ESP01_Status_t server_status = esp01_send_raw_command_dma("AT+CIPMUX=0", resp, sizeof(resp), "OK", 3000); // Configure le mode connexion unique
if (server_status != ESP01_OK)
{
	printf("[ESP01] >>> ERREUR: AT+CIPMUX\r\n");
	Error_Handler(); // Appelle le gestionnaire d'erreur
}
else
{
	printf("[ESP01] >>> Mode connexion unique activé\r\n");
}

// 8. Affichage de l'adresse IP
char ip[32];										  // Tampon pour l'adresse IP
if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK) // Récupère l'adresse IP actuelle
{
	printf("[WIFI] >>> Adresse IP actuelle : %s\r\n", ip);
}
else
{
	printf("[WIFI] >>> Impossible de récupérer l'IP\r\n");
}
test_mqtt(); // Lance le test MQTT
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
	HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN); // Inverse l'état de la LED
	HAL_Delay(1000);								 // Attente d'une seconde
/* USER CODE END WHILE */
