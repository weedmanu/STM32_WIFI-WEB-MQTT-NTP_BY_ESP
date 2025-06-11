/**
 ******************************************************************************
 * @file    STM32_WifiESP_HTTP.h
 * @author  [Ton Nom ou Initiales]
 * @version 1.1.0
 * @date    [Date de création ou de dernière modification]
 * @brief   Bibliothèque de gestion des requêtes HTTP pour le module ESP01 WiFi.
 *
 * @details
 * Ce fichier contient les définitions, structures, macros et prototypes de
 * fonctions pour gérer les requêtes HTTP (serveur et client) via un module
 * ESP01 connecté à un microcontrôleur STM32. Il propose des fonctions haut
 * niveau pour le parsing, la gestion des routes, l'envoi de réponses HTTP,
 * la gestion des connexions et la configuration du serveur web embarqué.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la bibliothèque STM32_WifiESP.h.
 *
 * @copyright
 * La licence de ce code est libre.
 ******************************************************************************
 */
#ifndef INC_STM32_WIFIESP_HTTP_H_
#define INC_STM32_WIFIESP_HTTP_H_

#include "STM32_WifiESP.h"
#include "STM32_WifiESP_WIFI.h"
#include <stdbool.h>
#include <stdint.h>

// ==================== DEFINES SPÉCIFIQUES HTTP ====================

#define ESP01_MAX_HTTP_METHOD_LEN 8 ///< Taille max du verbe HTTP (GET, POST...)
#define ESP01_MAX_HTTP_PATH_LEN 64  ///< Taille max du chemin HTTP
#define ESP01_MAX_HTTP_QUERY_LEN 64 ///< Taille max de la query string
#define ESP01_MAX_ROUTES 8          ///< Nombre max de routes HTTP
#define ESP01_MAX_CONNECTIONS 4     ///< Nombre max de connexions HTTP simultanées
#define ESP01_MAX_HEADER_LINE 256   ///< Taille max d'une ligne d'en-tête HTTP
#define ESP01_MAX_TOTAL_HTTP 2048   ///< Taille max du buffer HTTP total
#define ESP01_MAX_CIPSEND_BUF 64    ///< Taille max du buffer CIPSEND
#define ESP01_MAX_HTTP_REQ_BUF 256  ///< Taille max d'une requête HTTP brute
#define ESP01_MULTI_CONNECTION 1    ///< Mode multi-connexion activé

#define ESP01_HTTP_OK_CODE 200                                                 ///< Code HTTP 200 OK
#define ESP01_HTTP_NOT_FOUND_CODE 404                                          ///< Code HTTP 404 Not Found
#define ESP01_HTTP_INTERNAL_ERR_CODE 500                                       ///< Code HTTP 500 Internal Server Error
#define ESP01_HTTP_404_BODY "<html><body><h1>404 Not Found</h1></body></html>" ///< Corps de la réponse 404

// ==================== STRUCTURES HTTP ====================

/**
 * @struct http_parsed_request_t
 * @brief  Structure représentant une requête HTTP parsée.
 */
typedef struct
{
    char method[ESP01_MAX_HTTP_METHOD_LEN];      ///< Méthode HTTP (GET, POST, etc.)
    char path[ESP01_MAX_HTTP_PATH_LEN];          ///< Chemin de la requête
    char query_string[ESP01_MAX_HTTP_QUERY_LEN]; ///< Query string extraite
    char headers_buf[512];                       ///< Buffer contenant les headers bruts
    bool is_valid;                               ///< Indique si le parsing a réussi
} http_parsed_request_t;

/**
 * @brief Prototype de handler de route HTTP.
 * @param conn_id Identifiant de connexion.
 * @param req     Pointeur sur la requête HTTP parsée.
 */
typedef void (*esp01_route_handler_t)(int conn_id, const http_parsed_request_t *req);

/**
 * @struct esp01_route_t
 * @brief  Structure représentant une route HTTP et son handler associé.
 */
typedef struct
{
    char path[ESP01_MAX_HTTP_PATH_LEN]; ///< Chemin de la route
    esp01_route_handler_t handler;      ///< Fonction handler associée
} esp01_route_t;

/**
 * @struct connection_info_t
 * @brief  Informations sur une connexion HTTP active.
 */
typedef struct
{
    int conn_id;                      ///< Identifiant de connexion
    uint32_t last_activity;           ///< Timestamp de la dernière activité
    bool is_active;                   ///< Etat de la connexion
    char client_ip[ESP01_MAX_IP_LEN]; ///< IP du client
    uint16_t server_port;             ///< Port du serveur
    uint16_t client_port;             ///< Port du client
    uint32_t closed_at;               ///< Timestamp de fermeture
} connection_info_t;

/**
 * @struct http_request_t
 * @brief  Structure représentant une requête HTTP brute reçue.
 */
typedef struct
{
    int conn_id;                      ///< Identifiant de connexion
    int content_length;               ///< Taille du contenu
    bool is_valid;                    ///< Parsing réussi
    bool is_http_request;             ///< Est-ce une requête HTTP ?
    char client_ip[ESP01_MAX_IP_LEN]; ///< IP du client
    int client_port;                  ///< Port du client
    bool has_ip;                      ///< IP détectée
} http_request_t;

// ==================== VARIABLES GLOBALES HTTP ====================

extern esp01_route_t g_routes[ESP01_MAX_ROUTES];               ///< Tableau des routes HTTP enregistrées
extern int g_route_count;                                      ///< Nombre de routes actuellement enregistrées
extern connection_info_t g_connections[ESP01_MAX_CONNECTIONS]; ///< Tableau des connexions actives
extern int g_connection_count;                                 ///< Nombre de connexions actives
extern volatile int g_acc_len;                                 ///< Longueur actuelle du buffer accumulateur
extern char g_accumulator[ESP01_MAX_TOTAL_HTTP];               ///< Buffer accumulateur pour les requêtes HTTP
extern volatile int g_processing_request;                      ///< Indique si une requête est en cours de traitement

// ==================== PROTOTYPES HTTP ====================

/**
 * @brief  Initialise le module HTTP (routes, buffers, etc.).
 */
void esp01_http_init(void);

/**
 * @brief  Boucle principale de gestion HTTP (à appeler régulièrement pour traiter les requêtes).
 */
void esp01_http_loop(void);

/**
 * @brief  Parse une requête HTTP brute en structure http_parsed_request_t.
 * @param  raw_request Chaîne brute de la requête HTTP.
 * @param  parsed      Pointeur vers la structure de sortie.
 * @return ESP01_Status_t Code de retour (ESP01_OK si parsing réussi).
 */
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed);

/**
 * @brief  Envoie une réponse HTTP générique sur une connexion.
 * @param  conn_id      Identifiant de connexion.
 * @param  status_code  Code HTTP à retourner (ex: 200, 404...).
 * @param  content_type Type MIME de la réponse (ex: "text/html").
 * @param  body         Corps de la réponse (peut être NULL).
 * @param  body_len     Taille du corps de la réponse.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len);

/**
 * @brief  Envoie une réponse JSON sur une connexion.
 * @param  conn_id    Identifiant de connexion.
 * @param  json_data  Chaîne JSON à envoyer.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);

/**
 * @brief  Envoie une réponse 404 Not Found sur une connexion.
 * @param  conn_id    Identifiant de connexion.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_send_404_response(int conn_id);

/**
 * @brief  Effectue une requête HTTP GET en mode client.
 * @param  host           Hôte distant (adresse ou IP).
 * @param  port           Port distant.
 * @param  path           Chemin de la ressource.
 * @param  response       Buffer de réception de la réponse.
 * @param  response_size  Taille du buffer de réception.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_http_get(const char *host, uint16_t port, const char *path, char *response, size_t response_size);

/**
 * @brief  Efface toutes les routes HTTP enregistrées.
 */
void esp01_clear_routes(void);

/**
 * @brief  Ajoute une route HTTP et son handler associé.
 * @param  path     Chemin de la route (ex: "/api").
 * @param  handler  Fonction handler à appeler pour cette route.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);

/**
 * @brief  Recherche le handler associé à une route HTTP.
 * @param  path Chemin recherché.
 * @return Handler trouvé ou NULL si non trouvé.
 */
esp01_route_handler_t esp01_find_route_handler(const char *path);

/**
 * @brief  Traite les requêtes HTTP reçues (à appeler dans la boucle principale).
 */
void esp01_process_requests(void);

/**
 * @brief  Récupère l'adresse IP du client pour une connexion donnée.
 * @param  conn_id Identifiant de connexion.
 * @return Pointeur vers la chaîne IP (ou "N/A" si non disponible).
 */
const char *esp01_http_get_client_ip(int conn_id);

/**
 * @brief  Démarre le serveur web embarqué avec configuration.
 * @param  multi_conn true pour activer le mode multi-connexion.
 * @param  port       Port d'écoute du serveur.
 * @param  ipdinfo    true pour activer l'affichage IPDINFO.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_start_server_config(bool multi_conn, uint16_t port, bool ipdinfo);

/**
 * @brief  Arrête le serveur web embarqué.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_stop_web_server(void);

/**
 * @brief  Retourne le nombre de connexions actives.
 * @return Nombre de connexions actives.
 */
int esp01_get_active_connection_count(void);

/**
 * @brief  Vérifie si une connexion est active.
 * @param  conn_id Identifiant de connexion.
 * @return true si la connexion est active, false sinon.
 */
bool esp01_is_connection_active(int conn_id);

/**
 * @brief  Nettoie les connexions inactives (timeout).
 */
void esp01_cleanup_inactive_connections(void);

/**
 * @brief  Cherche le prochain paquet +IPD dans un buffer.
 * @param  buffer     Buffer à analyser.
 * @param  buffer_len Taille du buffer.
 * @return Pointeur sur le début du prochain +IPD ou NULL.
 */
char *_find_next_ipd(char *buffer, int buffer_len);

/**
 * @brief  Parse l'en-tête d'un paquet +IPD.
 * @param  data Données à parser.
 * @return Structure http_request_t remplie.
 */
http_request_t parse_ipd_header(const char *data);

/**
 * @brief  Affiche l'état des connexions (pour débogage).
 */
void esp01_print_connection_status(void);

#endif /* INC_STM32_WIFIESP_HTTP_H_ */