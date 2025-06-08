# STM32_WifiESP

**STM32_WifiESP** est une librairie C permettant à un microcontrôleur STM32 de piloter un module ESP01 (ESP8266) via UART/DMA pour des applications WiFi, serveur web, MQTT et NTP.  
Elle fournit une interface haut niveau pour la gestion du WiFi, la communication AT, la gestion d’un serveur HTTP embarqué, la connexion à un broker MQTT et la synchronisation NTP.

---

## Configuration des UART

Le projet utilise **deux UART** pour la communication :

- **UART1** : communication entre la STM32 et le module WiFi ESP01 (AT commands).
- **UART2** : console série pour le debug (redirection de `printf`).

### Exemple de configuration CubeMX

- **UART1**
  - Baudrate : 115200
  - Mode : TX/RX
  - DMA RX : activé en mode Circulaire (pour la réception efficace des trames AT)
- **UART2**
  - Baudrate : 115200
  - Mode : TX/RX


---

## Fonctionnalités principales

- **Initialisation du module ESP01** (UART, DMA, buffers)
- **Gestion du WiFi** : scan, connexion (STA/AP), DHCP ou IP statique
- **Serveur web intégré** : gestion multi-connexion, routage HTTP, réponses personnalisées
- **Gestion des connexions TCP** : suivi des clients, gestion des timeouts
- **Client MQTT** : connexion à un broker, gestion de l’état MQTT
- **Synchronisation NTP** : configuration, récupération et affichage de la date/heure
- **Statistiques globales** : requêtes HTTP, erreurs, temps de réponse
- **Utilitaires** : test AT, récupération d’IP, affichage du statut, gestion des buffers

---

## Structures et types principaux

- `ESP01_Status_t` : Codes de retour des fonctions (OK, erreur, timeout, etc.)
- `ESP01_WifiMode_t` : Modes WiFi supportés (STA, AP)
- `esp01_stats_t` : Statistiques globales du driver (requêtes, erreurs, temps)
- `connection_info_t` : Informations sur chaque connexion TCP active
- `http_request_t` : Informations extraites d’un header +IPD (requête entrante)
- `esp01_network_t` : Informations sur un réseau WiFi détecté
- `http_parsed_request_t` : Représentation d’une requête HTTP parsée (méthode, chemin, query)
- `esp01_route_handler_t` : Prototype d’un handler de route HTTP
- `esp01_route_t` : Structure d’une route HTTP (chemin + handler)
- `http_header_kv_t` : Couple clé/valeur pour un header HTTP
- `esp01_mqtt_client_t` : Informations sur la connexion MQTT courante

---

## Variables globales

- `g_stats` : Statistiques globales
- `g_connections[]` : Tableau des connexions TCP actives
- `g_connection_count` : Nombre de connexions actives
- `g_server_port` : Port du serveur web
- `g_wifi_mode` : Mode WiFi courant
- `g_esp_uart` : UART utilisé pour l’ESP01
- `g_debug_uart` : UART debug
- `g_mqtt_client` : Client MQTT global
- `g_accumulator[]` : Accumulateur de données RX
- `g_acc_len` : Taille de l’accumulateur

---

## Fonctions principales

### Initialisation & Communication

- `esp01_init()` : Initialise le driver (UART, DMA, buffers)
- `esp01_flush_rx_buffer()` : Vide le buffer de réception UART/DMA
- `esp01_send_raw_command_dma()` : Envoie une commande AT brute et récupère la réponse

### Gestion WiFi

- `esp01_connect_wifi_config()` : Configure et connecte le module ESP01 au WiFi (STA ou AP)
- `esp01_scan_networks()` : Scanne les réseaux WiFi à proximité
- `esp01_print_wifi_networks()` : Affiche la liste des réseaux WiFi détectés

### Serveur Web

- `esp01_start_server_config()` : Démarre le serveur web intégré de l’ESP01
- `esp01_stop_web_server()` : Arrête le serveur web intégré

### Statut & Utilitaires

- `esp01_test_at()` : Teste la communication AT avec l’ESP01
- `esp01_get_at_version()` : Récupère la version du firmware AT
- `esp01_get_connection_status()` : Vérifie le statut de connexion du module ESP01
- `esp01_get_current_ip()` : Récupère l’adresse IP courante du module ESP01
- `esp01_print_connection_status()` : Affiche le statut de connexion sur l’UART debug

### NTP (synchronisation temps)

- `esp01_configure_ntp()` : Configure le client NTP de l’ESP01
- `esp01_ntp_sync_and_print()` : Configure NTP, récupère et affiche la date/heure
- `esp01_get_ntp_time()` : Récupère la date/heure NTP depuis l’ESP01
- `esp01_print_fr_local_datetime()` : Affiche la date/heure NTP en français
- `esp01_print_local_datetime()` : Affiche la date/heure NTP avec décalage horaire

### MQTT

- Fonctions pour envoyer et recevoir des messages MQTT (voir exemples ci-dessous).

---

## Macros et constantes

- **Taille des buffers** : DMA RX, HTTP, debug, commandes AT, etc.
- **Timeouts** : court, moyen, long, WiFi, terminal, connexion
- **HTTP** : codes, types MIME, chemins, headers, etc.
- **Validation** : macro `VALIDATE_PARAM` pour vérifier les paramètres

---

## Organisation du code

- **Fichier d’en-tête** : `STM32_WifiESP.h` (définitions, structures, prototypes)
- **Implémentation** : à placer dans le fichier source correspondant (`STM32_WifiESP.c`)
- **Dépendances** : nécessite `main.h` (HAL STM32), UART HAL, et standard C

---

## Remarques

- La librairie est conçue pour être utilisée avec STM32Cube/HAL.
- Le mode debug peut être activé/désactivé via la macro `ESP01_DEBUG`.
- Les buffers doivent être correctement dimensionnés selon l’application.
- La gestion multi-connexion TCP est supportée (jusqu’à 5 connexions simultanées).
- Les handlers de routes HTTP permettent de personnaliser les réponses du serveur web.

---

# Exemples d’utilisation

## Exemple 1 : Synchronisation NTP

Ce programme montre comment initialiser le module ESP01, se connecter au WiFi, puis configurer et synchroniser l’heure via NTP.  
La date/heure NTP est affichée périodiquement sur la console série.

**Résumé du déroulement :**
- Initialisation du driver ESP01 et des UART
- Scan et connexion au réseau WiFi
- Configuration du serveur NTP (ex : pool.ntp.org)
- Synchronisation NTP unique puis périodique
- Affichage de la date/heure NTP en français

**Extrait de code (`ntp.c`) :**

````c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "STM32_WifiESP.h"
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_NTP.h"
/* USER CODE END Includes */
````

````c
/* USER CODE BEGIN PD */
#define SSID "XXXXXX"      // Nom du réseau WiFi auquel se connecter
#define PASSWORD "YYYYYY"  // Mot de passe du réseau WiFi
#define LED_GPIO_PORT GPIOA
#define LED_GPIO_PIN GPIO_PIN_5
/* USER CODE END PD */
````

````c
/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Tampon DMA pour la réception ESP01
/* USER CODE END PV */
````

````c
/* USER CODE BEGIN 0 */
// Redirige printf vers l'UART2 (console série)
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
/* USER CODE END 0 */
````

````c
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

// 3. Test de communication AT
printf("[ESP01] === Test de communication AT ===\r\n");
status = esp01_test_at();
printf("[ESP01] >>> Test AT : %s\r\n", esp01_get_error_string(status));
HAL_Delay(100);

// 4. Test de version AT+GMR
printf("[ESP01] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
char at_version[64];
status = esp01_get_at_version(at_version, sizeof(at_version));
printf("[ESP01] >>> Version ESP01 : %s\r\n", at_version);
HAL_Delay(100);

// 5. Test du scan WiFi
printf("[TEST] === Test du scan WiFi ===\r\n");
esp01_print_wifi_networks(10);
HAL_Delay(100);

// 6. Connexion au réseau WiFi
printf("[ESP01] === Connexion au réseau WiFi \"%s\" ===\r\n", SSID);
status = esp01_connect_wifi_config(ESP01_WIFI_MODE_STA, SSID, PASSWORD, true, NULL, NULL, NULL);
printf("[ESP01] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(status));
if (status != ESP01_OK)
{
    printf("[ESP01] !!! Échec de la connexion WiFi, arrêt du programme.\r\n");
    while (1);
}
HAL_Delay(500);

// 7. NTP
printf("[NTP] === NTP ===\r\n");

// Synchronisation NTP une fois
esp01_ntp_sync_once("pool.ntp.org", 2, true); // true = affichage FR
HAL_Delay(1000);
// Démarre la synchro NTP toutes les 60 secondes
esp01_ntp_start_periodic_sync("pool.ntp.org", 2, 60, true);
/* USER CODE END 2 */
````

````c
/* USER CODE BEGIN WHILE */
while (1)
{
    esp01_ntp_periodic_task();
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    HAL_Delay(1000);
/* USER CODE END WHILE */
````

## Exemple 2 : Serveur Web

Ce programme montre comment configurer et utiliser le serveur web intégré de l’ESP01.  
Il répond à des requêtes HTTP avec des pages web simples.

**Résumé du déroulement :**
- Initialisation du driver ESP01
- Connexion au WiFi
- Démarrage du serveur web sur un port spécifié
- Gestion des requêtes HTTP entrantes
- Réponses avec des pages web pré-définies

**Extrait de code (`serveur_web.c`) :**

````c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "STM32_WifiESP.h"
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_WebServer.h"
/* USER CODE END Includes */
````

````c
/* USER CODE BEGIN PD */
#define SSID "XXXXXX"      // Nom du réseau WiFi auquel se connecter
#define PASSWORD "YYYYYY"  // Mot de passe du réseau WiFi
#define LED_GPIO_PORT GPIOA
#define LED_GPIO_PIN GPIO_PIN_5
/* USER CODE END PD */
````

````c
/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Tampon DMA pour la réception ESP01
/* USER CODE END PV */
````

````c
/* USER CODE BEGIN 0 */
// Redirige printf vers l'UART2 (console série)
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

/* USER CODE END 0 */
````

````c
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

// 3. Test de communication AT
printf("[ESP01] === Test de communication AT ===\r\n");
status = esp01_test_at();
printf("[ESP01] >>> Test AT : %s\r\n", esp01_get_error_string(status));
HAL_Delay(100);

// 4. Test de version AT+GMR
printf("[ESP01] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
char at_version[64];
status = esp01_get_at_version(at_version, sizeof(at_version));
printf("[ESP01] >>> Version ESP01 : %s\r\n", at_version);
HAL_Delay(100);

// 5. Test du scan WiFi
printf("[TEST] === Test du scan WiFi ===\r\n");
esp01_print_wifi_networks(10);
HAL_Delay(100);

// 6. Connexion au réseau WiFi
printf("[ESP01] === Connexion au réseau WiFi \"%s\" ===\r\n", SSID);
status = esp01_connect_wifi_config(ESP01_WIFI_MODE_STA, SSID, PASSWORD, true, NULL, NULL, NULL);
printf("[ESP01] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(status));
if (status != ESP01_OK)
{
    printf("[ESP01] !!! Échec de la connexion WiFi, arrêt du programme.\r\n");
    while (1);
}
HAL_Delay(500);

// 7. Démarrage du serveur web
printf("[WEB] === Démarrage du serveur web sur le port %d ===\r\n", 80);
status = esp01_start_server_config(80, NULL, NULL);
printf("[WEB] >>> Serveur web : %s\r\n", esp01_get_error_string(status));

/* USER CODE END 2 */
````

````c
/* USER CODE BEGIN WHILE */
while (1)
{
    esp01_webserver_periodic_task();
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    HAL_Delay(1000);
/* USER CODE END WHILE */
````

## Exemple 3 : Envoi d’un message MQTT

Ce programme montre comment publier des messages sur un broker MQTT depuis la STM32 via ESP01.

**Résumé du déroulement :**
- Initialisation du driver ESP01
- Connexion au WiFi
- Passage en mode connexion unique (CIPMUX=0)
- Connexion à un broker MQTT (IP, port, client ID)
- Publication de messages sur un topic MQTT
- Déconnexion propre du broker

**Extrait de code (`Envoi_MQTT.c`) :**

````c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "STM32_WifiESP.h"
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_MQTT.h"
/* USER CODE END Includes */
````

````c
/* USER CODE BEGIN PD */
#define SSID "XXXXXX"            // Nom du réseau WiFi auquel se connecter
#define PASSWORD "YYYYYY"        // Mot de passe du réseau WiFi
#define LED_GPIO_PORT GPIOA
#define LED_GPIO_PIN GPIO_PIN_5
/* USER CODE END PD */
````

````c
/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Tampon DMA pour la réception ESP01
/* USER CODE END PV */
````

````c
/* USER CODE BEGIN 0 */
// Redirige printf vers l'UART2 (console série)
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

// Fonction pour tester la publication MQTT
static void test_mqtt(void)
{
    ESP01_Status_t status;
    char broker_ip[] = "192.168.1.185"; // Adresse IP du broker MQTT
    uint16_t broker_port = 1883;        // Port du broker MQTT

    esp01_flush_rx_buffer(1000);
    esp01_mqtt_disconnect();
    HAL_Delay(1000);

    char client_id[9];
    snprintf(client_id, sizeof(client_id), "stm%04X", (unsigned int)(HAL_GetTick() % 0xFFFF));
    printf("[MQTT] Connexion au broker MQTT %s:%d avec ID %s...\r\n", broker_ip, broker_port, client_id);

    int retry = 0;
    while (retry < 3)
    {
        status = esp01_mqtt_connect(broker_ip, broker_port, client_id, NULL, NULL);
        if (status == ESP01_OK)
        {
            break;
        }
        printf("[MQTT] Échec de connexion, tentative %d/3\r\n", retry + 1);
        retry++;
        HAL_Delay(1000);
    }

    if (status == ESP01_OK)
    {
        printf("[MQTT] Connexion établie avec succès\r\n");
        HAL_Delay(1000);

        const char *message = "Hello World !!!";
        printf("[MQTT] Publication: %s\r\n", message);
        status = esp01_mqtt_publish("stm32/test", message, 0, false);

        if (status == ESP01_OK)
        {
            printf("[MQTT] Message publié avec succès\r\n");
            HAL_Delay(1000);

            printf("[MQTT] Envoi PINGREQ...\r\n");
            status = esp01_mqtt_ping();
            HAL_Delay(1000);

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
        status = esp01_mqtt_disconnect();
        printf("[MQTT] Déconnexion: %s\r\n", esp01_get_error_string(status));
    }
    else
    {
        printf("[MQTT] Échec de la connexion au broker MQTT après 3 tentatives\r\n");
    }
}
/* USER CODE END 0 */
````

````c
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

// 4. Test de version AT+GMR
printf("[ESP01] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
char at_version[64];
status = esp01_get_at_version(at_version, sizeof(at_version));
printf("[ESP01] >>> Version ESP01 : %s\r\n", at_version);

// 6. Connexion au réseau WiFi
printf("[WIFI] === Connexion au réseau WiFi ===\r\n");
status = esp01_connect_wifi_config(
    ESP01_WIFI_MODE_STA,
    SSID,
    PASSWORD,
    true,
    NULL,
    NULL,
    NULL
);
printf("[WIFI] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(status));

// 7. Configuration du mode connexion unique (pour MQTT)
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

// 8. Affichage de l'adresse IP
char ip[32];
if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
{
    printf("[WIFI] >>> Adresse IP actuelle : %s\r\n", ip);
}
else
{
    printf("[WIFI] >>> Impossible de récupérer l'IP\r\n");
}
test_mqtt();
/* USER CODE END 2 */
````

````c
/* USER CODE BEGIN WHILE */
while (1)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    HAL_Delay(1000);
/* USER CODE END WHILE */
````

## Exemple 4 : Réception de messages MQTT

Ce programme montre comment souscrire à un topic MQTT et traiter les messages reçus sur STM32 via ESP01.

**Résumé du déroulement :**
- Initialisation du driver ESP01
- Connexion au WiFi
- Passage en mode connexion unique (CIPMUX=0)
- Connexion à un broker MQTT (IP, port, client ID)
- Souscription à un topic MQTT
- Enregistrement d’un callback pour traiter les messages reçus
- Boucle principale : appel du poller MQTT et clignotement d’une LED

**Extrait de code (`Reception_MQTT.c`) :**

````c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "STM32_WifiESP.h"
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_MQTT.h"
/* USER CODE END Includes */
````

````c
/* USER CODE BEGIN PD */
#define SSID "XXXXXX"      // Nom du réseau WiFi auquel se connecter
#define PASSWORD "YYYYYY"  // Mot de passe du réseau WiFi
#define LED_GPIO_PORT GPIOA
#define LED_GPIO_PIN GPIO_PIN_5
/* USER CODE END PD */
````

````c
/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE];
/* USER CODE END PV */
````

````c
/* USER CODE BEGIN 0 */
// Redirige printf vers l'UART2 (console série)
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
// Callback appelé à chaque message MQTT reçu
void mqtt_message_callback(const char *topic, const char *payload)
{
    printf("[MQTT] Message reçu sur %s : %s\r\n", topic, payload);
}
/* USER CODE END 0 */
````

````c
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
    NULL
);
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
````

````c
/* USER CODE BEGIN WHILE */
while (1)
{
    esp01_mqtt_poll(); // Appelle le callback si un message est reçu
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    HAL_Delay(1000);
}
/* USER CODE END WHILE */
`````




