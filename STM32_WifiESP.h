#ifndef INC_STM32_WIFIESP_H_
#define INC_STM32_WIFIESP_H_

// ==================== INCLUDES ====================

#include "main.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// ==================== VERSION =====================
#define STM32_WIFIESP_VERSION "1.1.0" ///< Version du driver STM32_WifiESP

// ==================== DEFINES & MACROS ====================

#define ESP01_DEBUG 1 ///< Active le mode debug (1=oui, 0=non)

#define ESP01_DMA_RX_BUF_SIZE 512    ///< Taille du buffer DMA RX
#define ESP01_MAX_ROUTES 8           ///< Nombre max de routes HTTP
#define ESP01_MAX_CONNECTIONS 5      ///< Nombre max de connexions TCP
#define ESP01_FLUSH_TIMEOUT_MS 300   ///< Timeout pour flush RX (ms)
#define ESP01_SMALL_BUF_SIZE 128     ///< Taille d'un petit buffer
#define ESP01_DMA_TEST_BUF_SIZE 64   ///< Taille buffer test DMA
#define ESP01_SHORT_DELAY_MS 10      ///< Délai court (ms)
#define ESP01_MAX_LOG_MSG 512        ///< Taille max d'un message log
#define ESP01_MAX_WARN_MSG 100       ///< Taille max d'un message warning
#define ESP01_MAX_HEADER_LINE 256    ///< Taille max d'une ligne header HTTP
#define ESP01_MAX_TOTAL_HTTP 2048    ///< Taille max d'une réponse HTTP totale
#define ESP01_MAX_CIPSEND_CMD 64     ///< Taille max d'une commande CIPSEND
#define ESP01_MAX_IP_LEN 32          ///< Taille max d'une IP
#define ESP01_MAX_HTTP_METHOD_LEN 8  ///< Taille max méthode HTTP
#define ESP01_MAX_HTTP_PATH_LEN 64   ///< Taille max chemin HTTP
#define ESP01_MAX_HTTP_QUERY_LEN 128 ///< Taille max query string HTTP
#define ESP01_MAX_HEADER_BUF 512     ///< Taille max buffer headers HTTP
#define ESP01_MAX_DBG_BUF 128        ///< Taille max buffer debug
#define ESP01_MAX_ROUTE_DBG_BUF 80   ///< Taille max debug route
#define ESP01_MAX_CMD_BUF 256        ///< Taille max buffer commande AT
#define ESP01_MAX_RESP_BUF 2048      ///< Taille max buffer réponse AT
#define ESP01_MAX_CIPSEND_BUF 32     ///< Taille max buffer CIPSEND
#define ESP01_MAX_HTTP_REQ_BUF 256   ///< Taille max buffer requête HTTP
#define ESP01_TMP_BUF_SIZE 256       ///< Taille buffer temporaire
#define ESP01_CMD_RESP_BUF_SIZE 64   ///< Taille buffer réponse commande
#define ESP01_AT_VERSION_BUF 128     ///< Taille buffer version AT
#define ESP01_IP_BUF_SIZE 32         ///< Taille buffer IP
#define ESP01_MAX_BODY_PARAMS 8      ///< Nombre max de paramètres POST
#define ESP01_MAX_ROW_SIZE 128       ///< Taille max d'une ligne
#define ESP01_MAX_PARAMS_HTML 512    ///< Taille max params HTML
#define ESP01_MAX_HTML_SIZE 2048     ///< Taille max page HTML
#define ESP01_MAX_AT_VERSION 64      ///< Taille max version AT

#define IPD_HEADER_MIN_LEN 5 ///< Longueur minimale header +IPD

// --- Timeouts ---
#define ESP01_TIMEOUT_SHORT 1000        ///< Timeout court (ms)
#define ESP01_TIMEOUT_MEDIUM 3000       ///< Timeout moyen (ms)
#define ESP01_TIMEOUT_LONG 20000        ///< Timeout long (ms)
#define ESP01_TIMEOUT_WIFI 15000        ///< Timeout WiFi (ms)
#define ESP01_TERMINAL_TIMEOUT_MS 30000 ///< Timeout terminal (ms)
#define ESP01_CONN_TIMEOUT_MS 30000     ///< Timeout connexion (ms)

#define ESP01_MULTI_CONNECTION 1 ///< Active le mode multi-connexion

// --- HTTP ---
#define ESP01_HTTP_404_BODY "<html><body><h1>404 - Page Not Found</h1></body></html>" ///< Corps de la réponse 404
#define ESP01_HTTP_404_BODY_LEN (sizeof(ESP01_HTTP_404_BODY) - 1)                     ///< Taille corps 404
#define ESP01_HTTP_FAVICON_PATH "/favicon.ico"                                        ///< Chemin favicon
#define ESP01_HTTP_FAVICON_CODE 204                                                   ///< Code HTTP favicon
#define ESP01_HTTP_FAVICON_TYPE "image/x-icon"                                        ///< Type MIME favicon
#define ESP01_HTTP_OK_CODE 200                                                        ///< Code HTTP OK
#define ESP01_HTTP_OK_TYPE "application/json"                                         ///< Type MIME OK
#define ESP01_HTTP_NOT_FOUND_CODE 404                                                 ///< Code HTTP Not Found
#define ESP01_HTTP_NOT_FOUND_TYPE "text/html"                                         ///< Type MIME Not Found
#define ESP01_HTTP_INTERNAL_ERR_CODE 500                                              ///< Code HTTP Internal Error
#define ESP01_HTTP_UNKNOWN_CODE 0                                                     ///< Code HTTP inconnu
#define ESP01_HTTP_CONN_CLOSE "Connection: close\r\n"                                 ///< Header fermeture connexion
#define ESP01_HTTP_VERSION "HTTP/1.1"                                                 ///< Version HTTP
#define ESP01_HTTP_CRLF "\r\n"                                                        ///< Saut de ligne HTTP
#define ESP01_HTTP_HEADER_END "\r\n\r\n"                                              ///< Fin headers HTTP

// --- Validation macro ---
#ifndef VALIDATE_PARAM
#define VALIDATE_PARAM(expr, ret) \
    do                            \
    {                             \
        if (!(expr))              \
            return (ret);         \
    } while (0)
#endif

// ==================== TYPES & STRUCTURES ====================

/**
 * @brief Codes de statut pour les fonctions ESP01.
 */
typedef enum
{
    ESP01_OK = 0,
    ESP01_FAIL = -1,
    ESP01_TIMEOUT = -2,
    ESP01_NOT_INITIALIZED = -3,
    ESP01_INVALID_PARAM = -4,
    ESP01_BUFFER_OVERFLOW = -5,
    ESP01_WIFI_NOT_CONNECTED = -6,
    ESP01_HTTP_PARSE_ERROR = -7,
    ESP01_ROUTE_NOT_FOUND = -8,
    ESP01_CONNECTION_ERROR = -9,
    ESP01_MEMORY_ERROR = -10,
    ESP01_EXIT = -100
} ESP01_Status_t;

/**
 * @brief Modes WiFi supportés par l'ESP01.
 */
typedef enum
{
    ESP01_WIFI_MODE_STA = 1, ///< Station (client)
    ESP01_WIFI_MODE_AP = 2   ///< Point d'accès
} ESP01_WifiMode_t;

/**
 * @brief Statistiques globales du driver ESP01.
 */
typedef struct
{
    uint32_t total_requests;         ///< Nombre total de requêtes HTTP traitées
    uint32_t successful_responses;   ///< Nombre de réponses HTTP 2xx
    uint32_t failed_responses;       ///< Nombre de réponses HTTP 4xx/5xx
    uint32_t parse_errors;           ///< Nombre d'erreurs de parsing HTTP
    uint32_t buffer_overflows;       ///< Nombre de débordements de buffer détectés
    uint32_t connection_timeouts;    ///< Nombre de timeouts de connexion
    uint32_t avg_response_time_ms;   ///< Temps de réponse moyen (ms)
    uint32_t total_response_time_ms; ///< Somme des temps de réponse (ms)
    uint32_t response_count;         ///< Nombre total de réponses envoyées
} esp01_stats_t;

/**
 * @brief Informations sur une connexion TCP active.
 */
typedef struct
{
    int conn_id;                      ///< ID de la connexion (ESP01)
    uint32_t last_activity;           ///< Timestamp de la dernière activité (ms)
    bool is_active;                   ///< true si la connexion est active
    char client_ip[ESP01_MAX_IP_LEN]; ///< Adresse IP du client
    uint16_t server_port;             ///< Port du serveur (local)
    uint16_t client_port;             ///< Port du client (distant)
} connection_info_t;

/**
 * @brief Informations extraites d'un header +IPD (données entrantes).
 */
typedef struct
{
    int conn_id;          ///< ID de la connexion
    int content_length;   ///< Longueur du payload
    bool is_valid;        ///< true si parsing réussi
    bool is_http_request; ///< true si c'est une requête HTTP
    char client_ip[16];   ///< IP du client (si dispo)
    int client_port;      ///< Port du client (si dispo)
    bool has_ip;          ///< true si IP/port présents
} http_request_t;

/**
 * @brief Informations sur un réseau WiFi détecté.
 */
typedef struct
{
    char ssid[33];      ///< SSID du réseau (max 32 + \0)
    int8_t rssi;        ///< Puissance du signal (dBm)
    uint8_t encryption; ///< Type de chiffrement (0=ouvert, 1=WEP, 2=WPA, ...)
    char bssid[18];     ///< Adresse MAC du point d'accès
    uint8_t channel;    ///< Canal WiFi
} esp01_network_t;

/**
 * @brief Représente une requête HTTP parsée (méthode, chemin, query).
 */
typedef struct
{
    char method[ESP01_MAX_HTTP_METHOD_LEN];      ///< Méthode HTTP (GET, POST, ...)
    char path[ESP01_MAX_HTTP_PATH_LEN];          ///< Chemin de la requête
    char query_string[ESP01_MAX_HTTP_QUERY_LEN]; ///< Chaîne de paramètres GET
    char headers_buf[512];                       ///< Buffer pour les headers (optionnel)
    bool is_valid;                               ///< true si parsing réussi
} http_parsed_request_t;

/**
 * @brief Prototype d'un handler de route HTTP.
 */
typedef void (*esp01_route_handler_t)(int conn_id, const http_parsed_request_t *request);

/**
 * @brief Structure d'une route HTTP (chemin + handler).
 */
typedef struct
{
    char path[ESP01_MAX_HTTP_PATH_LEN]; ///< Chemin HTTP (ex: "/status")
    esp01_route_handler_t handler;      ///< Fonction handler associée
} esp01_route_t;

/**
 * @brief Couple clé/valeur pour un header HTTP.
 */
typedef struct
{
    const char *key;   ///< Nom du header
    size_t key_len;    ///< Longueur du nom
    const char *value; ///< Valeur du header
    size_t value_len;  ///< Longueur de la valeur
} http_header_kv_t;

/**
 * @brief Informations sur la connexion MQTT courante.
 */
typedef struct
{
    bool connected;                   ///< true si connecté au broker
    char broker_ip[ESP01_MAX_IP_LEN]; ///< IP du broker MQTT
    uint16_t broker_port;             ///< Port du broker
    char client_id[33];               ///< ID client MQTT
    uint16_t keep_alive;              ///< Keepalive (secondes)
    uint16_t packet_id;               ///< Dernier packet ID utilisé
} esp01_mqtt_client_t;

// ==================== VARIABLES GLOBALES ====================

extern esp01_stats_t g_stats;                                  ///< Statistiques globales
extern connection_info_t g_connections[ESP01_MAX_CONNECTIONS]; ///< Tableau des connexions TCP
extern int g_connection_count;                                 ///< Nombre de connexions actives
extern uint16_t g_server_port;                                 ///< Port du serveur web
extern const ESP01_WifiMode_t g_wifi_mode;                     ///< Mode WiFi courant
extern UART_HandleTypeDef *g_esp_uart;                         ///< UART ESP01
extern UART_HandleTypeDef *g_debug_uart;                       ///< UART debug
extern esp01_mqtt_client_t g_mqtt_client;                      ///< Client MQTT global
extern char g_accumulator[ESP01_DMA_RX_BUF_SIZE * 2];          ///< Accumulateur de données RX
extern uint16_t g_acc_len;                                     ///< Taille de l'accumulateur

// ==================== FONCTIONS HAUT NIVEAU ====================

// --- Driver & Communication ---

/**
 * @brief Initialise le driver ESP01 (UART, DMA, buffers).
 * @param huart_esp   Pointeur sur l’UART utilisé pour l’ESP01.
 * @param huart_debug Pointeur sur l’UART utilisé pour le debug (peut être NULL).
 * @param dma_rx_buf  Buffer DMA de réception.
 * @param dma_buf_size Taille du buffer DMA.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
                          uint8_t *dma_rx_buf, uint16_t dma_buf_size);

/**
 * @brief Vide le buffer de réception UART/DMA.
 * @param timeout_ms Timeout en millisecondes pour l’opération.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms);

/**
 * @brief Envoie une commande AT brute et récupère la réponse.
 * @param cmd                Commande AT à envoyer (ex: "AT+GMR").
 * @param response_buffer    Buffer de réception pour la réponse.
 * @param max_response_size  Taille maximale du buffer de réponse.
 * @param expected_terminator Chaîne attendue pour valider la fin de la réponse (ex: "OK").
 * @param timeout_ms         Timeout en millisecondes pour attendre la réponse.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer,
                                          uint32_t max_response_size,
                                          const char *expected_terminator,
                                          uint32_t timeout_ms);

// --- WiFi ---

/**
 * @brief Configure et connecte le module ESP01 au WiFi (STA ou AP).
 * @param mode      Mode WiFi (STA ou AP).
 * @param ssid      SSID du réseau WiFi.
 * @param password  Mot de passe du réseau WiFi.
 * @param use_dhcp  true pour DHCP, false pour IP statique.
 * @param ip        Adresse IP statique (NULL si DHCP).
 * @param gateway   Passerelle (NULL si DHCP).
 * @param netmask   Masque de sous-réseau (NULL si DHCP).
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_connect_wifi_config(
    ESP01_WifiMode_t mode,
    const char *ssid,
    const char *password,
    bool use_dhcp,
    const char *ip,
    const char *gateway,
    const char *netmask);

/**
 * @brief Scanne les réseaux WiFi à proximité.
 * @param networks      Tableau de structures pour stocker les réseaux trouvés.
 * @param max_networks  Nombre maximum de réseaux à stocker.
 * @param found_networks Pointeur pour stocker le nombre de réseaux trouvés.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks);

/**
 * @brief Affiche la liste des réseaux WiFi détectés sur la sortie standard.
 * @param max_networks Nombre maximum de réseaux à afficher.
 */
void esp01_print_wifi_networks(uint8_t max_networks);

// --- Serveur Web ---

/**
 * @brief Démarre le serveur web intégré de l'ESP01.
 * @param multi_conn true pour activer le mode multi-connexion, false sinon.
 * @param port       Port TCP du serveur web.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_start_server_config(bool multi_conn, uint16_t port);

/**
 * @brief Arrête le serveur web intégré de l'ESP01.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_stop_web_server(void);

// --- Statut & Utilitaires ---

/**
 * @brief Teste la communication AT avec l'ESP01.
 * @return ESP01_OK si la communication fonctionne, code d’erreur sinon.
 */
ESP01_Status_t esp01_test_at(void);

/**
 * @brief Récupère la version du firmware AT de l'ESP01.
 * @param version_buf Buffer de sortie pour la version.
 * @param buf_size    Taille du buffer.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size);

/**
 * @brief Vérifie le statut de connexion du module ESP01.
 * @return ESP01_OK si connecté, code d’erreur sinon.
 */
ESP01_Status_t esp01_get_connection_status(void);

/**
 * @brief Récupère l'adresse IP courante du module ESP01.
 * @param ip_buf   Buffer de sortie pour l’adresse IP.
 * @param buf_len  Taille du buffer.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);

/**
 * @brief Affiche le statut de connexion du module ESP01 sur l'UART debug.
 * @return ESP01_OK si succès, code d’erreur sinon.
 */
ESP01_Status_t esp01_print_connection_status(void);

/**
 * @brief Configure le client NTP de l'ESP01.
 * @param ntp_server      Nom du serveur NTP (ex: "pool.ntp.org").
 * @param timezone_offset Décalage horaire (ex: 2 pour UTC+2).
 * @param sync_period_s   Période de synchronisation en secondes (ex: 60).
 * @return ESP01_OK ou code d'erreur.
 */
ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone_offset, int sync_period_s);

/**
 * @brief Configure NTP, récupère et affiche la date/heure (brut et FR).
 * @param timezone_offset Décalage horaire (ex: 2 pour UTC+2).
 * @param print_fr        true pour affichage français.
 * @param sync_period_s   Période de synchronisation en secondes.
 */
void esp01_ntp_sync_and_print(int timezone_offset, bool print_fr, int sync_period_s);

/**
 * @brief Récupère la date/heure NTP depuis l'ESP01 (format texte).
 * @param datetime_buf    Buffer de sortie.
 * @param bufsize         Taille du buffer.
 * @return ESP01_OK ou code d'erreur.
 */
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize);

/**
 * @brief Affiche la date/heure NTP en français (lecture directe).
 * @param datetime_ntp    Chaîne date/heure NTP brute.
 */
void esp01_print_fr_local_datetime(const char *datetime_ntp);

/**
 * @brief Affiche la date/heure NTP en appliquant un décalage horaire.
 * @param datetime_ntp    Chaîne date/heure NTP brute (UTC).
 * @param timezone_offset Décalage horaire à appliquer.
 */
void esp01_print_local_datetime(const char *datetime_ntp, int timezone_offset);

#endif /* INC_STM32_WIFIESP_H_ */