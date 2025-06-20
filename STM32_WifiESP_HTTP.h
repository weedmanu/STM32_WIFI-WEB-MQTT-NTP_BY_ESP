/**
 ******************************************************************************
 * @file    STM32_WifiESP_HTTP.h
 * @author  manu
 * @version 1.2.0
 * @date    17 juin 2025
 * @brief   Bibliothèque de gestion des requêtes HTTP pour le module ESP01 WiFi.
 *
 * @details
 * Définitions, structures, macros et prototypes pour gérer les requêtes HTTP
 * (serveur et client) via un module ESP01 connecté à un STM32.
 * Fonctions haut niveau pour le parsing, la gestion des routes, l'envoi de réponses,
 * la gestion des connexions et la configuration du serveur web embarqué.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la bibliothèque STM32_WifiESP.h.
 ******************************************************************************
 */
#ifndef STM32_WIFIESP_HTTP_H_
#define STM32_WIFIESP_HTTP_H_

/* ========================== INCLUDES ========================== */
#include "STM32_WifiESP.h"      ///< Driver bas niveau requis
#include "STM32_WifiESP_WIFI.h" ///< Module WiFi haut niveau requis
#include <stdbool.h>            ///< Types booléens
#include <stdint.h>             ///< Types entiers standard

/* ========================== DEFINES =========================== */

// --- Limites et tailles ---
#define ESP01_MAX_HTTP_METHOD_LEN 8 ///< Longueur max du verbe HTTP (GET, POST, etc.)
#define ESP01_MAX_HTTP_PATH_LEN 64  ///< Longueur max du chemin d'une route
#define ESP01_MAX_HTTP_QUERY_LEN 64 ///< Longueur max de la query string
#define ESP01_MAX_ROUTES 8          ///< Nombre max de routes HTTP
#define ESP01_MAX_CONNECTIONS 4     ///< Nombre max de connexions HTTP simultanées
#define ESP01_MAX_HEADER_LINE 256   ///< Taille max d'une ligne d'en-tête HTTP
#define ESP01_MAX_TOTAL_HTTP 2048   ///< Taille max du buffer accumulateur HTTP
#define ESP01_MAX_CIPSEND_BUF 64    ///< Taille max du buffer pour CIPSEND
#define ESP01_MAX_HTTP_REQ_BUF 256  ///< Taille max du buffer de requête HTTP
#define ESP01_MULTI_CONNECTION 1    ///< Mode multi-connexion activé

// --- Codes HTTP ---
#define ESP01_HTTP_OK_CODE 200                                                 ///< Code HTTP 200 OK
#define ESP01_HTTP_NOT_FOUND_CODE 404                                          ///< Code HTTP 404 Not Found
#define ESP01_HTTP_INTERNAL_ERR_CODE 500                                       ///< Code HTTP 500 Internal Server Error
#define ESP01_HTTP_404_BODY "<html><body><h1>404 Not Found</h1></body></html>" ///< Corps HTML pour 404

/* ========================== STRUCTURES ======================== */

/**
 * @brief Structure représentant une requête HTTP parsée.
 */
typedef struct
{
    char method[ESP01_MAX_HTTP_METHOD_LEN];      ///< Verbe HTTP (GET, POST, etc.)
    char path[ESP01_MAX_HTTP_PATH_LEN];          ///< Chemin de la requête
    char query_string[ESP01_MAX_HTTP_QUERY_LEN]; ///< Query string extraite
    char headers_buf[512];                       ///< Buffer pour les en-têtes HTTP
    bool is_valid;                               ///< Indique si la requête est valide
} http_parsed_request_t;

/**
 * @brief Prototype de handler de route HTTP.
 */
typedef void (*esp01_route_handler_t)(int conn_id, const http_parsed_request_t *req);

/**
 * @brief Structure représentant une route HTTP et son handler associé.
 */
typedef struct
{
    char path[ESP01_MAX_HTTP_PATH_LEN]; ///< Chemin de la route
    esp01_route_handler_t handler;      ///< Pointeur vers la fonction handler
} esp01_route_t;

/**
 * @brief Informations sur une connexion HTTP active.
 */
typedef struct
{
    int conn_id;                      ///< Identifiant de connexion
    uint32_t last_activity;           ///< Timestamp de la dernière activité
    bool is_active;                   ///< Etat actif/inactif
    char client_ip[ESP01_MAX_IP_LEN]; ///< Adresse IP du client
    uint16_t server_port;             ///< Port du serveur
    uint16_t client_port;             ///< Port du client
    uint32_t closed_at;               ///< Timestamp de fermeture
} connection_info_t;

/**
 * @brief Structure représentant une requête HTTP brute reçue.
 */
typedef struct
{
    int conn_id;                      ///< Identifiant de connexion
    int content_length;               ///< Longueur du contenu
    bool is_valid;                    ///< Indique si la requête est valide
    bool is_http_request;             ///< Indique si c'est une requête HTTP
    char client_ip[ESP01_MAX_IP_LEN]; ///< Adresse IP du client
    int client_port;                  ///< Port du client
    bool has_ip;                      ///< Indique si l'IP est connue
} http_request_t;

/**
 * @brief Statistiques HTTP.
 */
typedef struct
{
    uint32_t total_requests;         ///< Nombre total de requêtes reçues
    uint32_t response_count;         ///< Nombre total de réponses envoyées
    uint32_t successful_responses;   ///< Nombre de réponses avec succès
    uint32_t failed_responses;       ///< Nombre de réponses en erreur
    uint32_t total_response_time_ms; ///< Temps total de réponse (ms)
    uint32_t avg_response_time_ms;   ///< Temps de réponse moyen (ms)
} esp01_stats_t;

/* ========================= VARIABLES GLOBALES ========================= */

extern esp01_route_t g_routes[ESP01_MAX_ROUTES];               ///< Tableau des routes HTTP
extern int g_route_count;                                      ///< Nombre de routes enregistrées
extern connection_info_t g_connections[ESP01_MAX_CONNECTIONS]; ///< Tableau des connexions actives
extern int g_connection_count;                                 ///< Nombre de connexions actives
extern volatile int g_acc_len;                                 ///< Longueur du buffer accumulateur
extern char g_accumulator[ESP01_MAX_TOTAL_HTTP];               ///< Buffer accumulateur pour les requêtes
extern volatile int g_processing_request;                      ///< Flag de traitement de requête en cours
extern esp01_stats_t g_stats;                                  ///< Statistiques HTTP

/* ========================= PROTOTYPES FONCTIONS ======================= */

// --- Initialisation & boucle principale ---
/**
 * @brief Initialise le module HTTP.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_http_init(void);

/**
 * @brief Boucle principale de gestion HTTP.
 */
void esp01_http_loop(void);

// --- Routes ---
/**
 * @brief Efface toutes les routes HTTP.
 */
void esp01_clear_routes(void);

/**
 * @brief Ajoute une route HTTP.
 * @param path    Chemin de la route.
 * @param handler Fonction handler associée.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);

/**
 * @brief Trouve le handler associé à une route.
 * @param path Chemin de la route.
 * @return Pointeur vers la fonction handler, ou NULL si non trouvé.
 */
esp01_route_handler_t esp01_find_route_handler(const char *path);

// --- Parsing requêtes ---
/**
 * @brief Parse une requête HTTP brute.
 * @param raw_request Chaîne brute de la requête HTTP.
 * @param parsed      Structure de sortie.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed);

// --- Réponses HTTP ---
/**
 * @brief Envoie une réponse HTTP.
 * @param conn_id      Identifiant de connexion.
 * @param status_code  Code HTTP à envoyer.
 * @param content_type Type MIME du contenu.
 * @param body         Corps de la réponse.
 * @param body_len     Taille du corps.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len);

/**
 * @brief Envoie une réponse JSON.
 * @param conn_id   Identifiant de connexion.
 * @param json_data Données JSON à envoyer.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);

/**
 * @brief Envoie une réponse 404 Not Found.
 * @param conn_id Identifiant de connexion.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_send_404_response(int conn_id);

// --- Serveur web ---
/**
 * @brief Démarre le serveur HTTP sur le port donné.
 * @param port Port d'écoute.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_http_start_server(uint16_t port);

/**
 * @brief Arrête le serveur HTTP.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_http_stop_server(void);

/**
 * @brief Configure le serveur HTTP.
 * @param multi_conn true pour multi-connexion, false sinon.
 * @param port       Port d'écoute.
 * @param ipdinfo    true pour activer IPDINFO, false sinon.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_start_server_config(bool multi_conn, uint16_t port, bool ipdinfo);

/**
 * @brief  Récupère le statut du serveur HTTP
 * @param  is_server Pointeur vers variable indiquant si le serveur est en cours d'exécution
 * @param  port      Pointeur vers variable stockant le port du serveur
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 * @note   Cette fonction vérifie l'état du serveur et retourne le port configuré
 */
ESP01_Status_t esp01_http_get_server_status(uint8_t *is_server, uint16_t *port);

// --- Connexions ---
/**
 * @brief Retourne le nombre de connexions actives.
 * @return Nombre de connexions actives.
 */
int esp01_get_active_connection_count(void);

/**
 * @brief Indique si une connexion est active.
 * @param conn_id Identifiant de connexion.
 * @return true si active, false sinon.
 */
bool esp01_is_connection_active(int conn_id);

/**
 * @brief Ferme une connexion HTTP.
 * @param conn_id Identifiant de connexion.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_http_close_connection(int conn_id);

/**
 * @brief Nettoie les connexions inactives.
 */
void esp01_cleanup_inactive_connections(void);

/**
 * @brief Retourne l'IP du client pour une connexion.
 * @param conn_id Identifiant de connexion.
 * @return Pointeur vers la chaîne IP.
 */
const char *esp01_http_get_client_ip(int conn_id);

/**
 * @brief Affiche l'état des connexions.
 */
void esp01_print_connection_status(void);

// --- Traitement automatique des requêtes ---
/**
 * @brief Traite automatiquement les requêtes HTTP.
 */
void esp01_process_requests(void);

// --- Clients AP ---
/**
 * @brief Active/désactive le mode multi-connexion.
 * @param enable true pour activer, false pour désactiver.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_set_multiple_connections(bool enable);

/**
 * @brief Liste les clients connectés à l'AP.
 * @param out      Buffer de sortie.
 * @param out_size Taille du buffer.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_list_ap_clients(char *out, size_t out_size);

#endif
