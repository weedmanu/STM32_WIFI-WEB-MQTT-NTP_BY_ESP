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
#include "STM32_WifiESP.h"
#include "STM32_WifiESP_WIFI.h"
#include <stdbool.h>
#include <stdint.h>

/* ========================== DEFINES =========================== */

// --- Limites et tailles ---
#define ESP01_MAX_HTTP_METHOD_LEN 8
#define ESP01_MAX_HTTP_PATH_LEN 64
#define ESP01_MAX_HTTP_QUERY_LEN 64
#define ESP01_MAX_ROUTES 8
#define ESP01_MAX_CONNECTIONS 4
#define ESP01_MAX_HEADER_LINE 256
#define ESP01_MAX_TOTAL_HTTP 2048
#define ESP01_MAX_CIPSEND_BUF 64
#define ESP01_MAX_HTTP_REQ_BUF 256
#define ESP01_MULTI_CONNECTION 1

// --- Codes HTTP ---
#define ESP01_HTTP_OK_CODE 200
#define ESP01_HTTP_NOT_FOUND_CODE 404
#define ESP01_HTTP_INTERNAL_ERR_CODE 500
#define ESP01_HTTP_404_BODY "<html><body><h1>404 Not Found</h1></body></html>"

/* ========================== STRUCTURES ======================== */

/**
 * @brief Structure représentant une requête HTTP parsée.
 */
typedef struct
{
    char method[ESP01_MAX_HTTP_METHOD_LEN];
    char path[ESP01_MAX_HTTP_PATH_LEN];
    char query_string[ESP01_MAX_HTTP_QUERY_LEN];
    char headers_buf[512];
    bool is_valid;
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
    char path[ESP01_MAX_HTTP_PATH_LEN];
    esp01_route_handler_t handler;
} esp01_route_t;

/**
 * @brief Informations sur une connexion HTTP active.
 */
typedef struct
{
    int conn_id;
    uint32_t last_activity;
    bool is_active;
    char client_ip[ESP01_MAX_IP_LEN];
    uint16_t server_port;
    uint16_t client_port;
    uint32_t closed_at;
} connection_info_t;

/**
 * @brief Structure représentant une requête HTTP brute reçue.
 */
typedef struct
{
    int conn_id;
    int content_length;
    bool is_valid;
    bool is_http_request;
    char client_ip[ESP01_MAX_IP_LEN];
    int client_port;
    bool has_ip;
} http_request_t;

/**
 * @brief Statistiques HTTP.
 */
typedef struct
{
    uint32_t total_requests;
    uint32_t response_count;
    uint32_t successful_responses;
    uint32_t failed_responses;
    uint32_t total_response_time_ms;
    uint32_t avg_response_time_ms;
} esp01_stats_t;

/* ========================= VARIABLES GLOBALES ========================= */

extern esp01_route_t g_routes[ESP01_MAX_ROUTES];
extern int g_route_count;
extern connection_info_t g_connections[ESP01_MAX_CONNECTIONS];
extern int g_connection_count;
extern volatile int g_acc_len;
extern char g_accumulator[ESP01_MAX_TOTAL_HTTP];
extern volatile int g_processing_request;
extern esp01_stats_t g_stats;

/* ========================= PROTOTYPES FONCTIONS ======================= */

// --- Initialisation & boucle principale ---
ESP01_Status_t esp01_http_init(void);
void esp01_http_loop(void);

// --- Routes ---
void esp01_clear_routes(void);
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);
esp01_route_handler_t esp01_find_route_handler(const char *path);

// --- Parsing requêtes ---
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed);

// --- Réponses HTTP ---
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len);
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);
ESP01_Status_t esp01_send_404_response(int conn_id);

// --- Serveur web ---
ESP01_Status_t esp01_http_start_server(uint16_t port);
ESP01_Status_t esp01_http_stop_server(void);
ESP01_Status_t esp01_start_server_config(bool multi_conn, uint16_t port, bool ipdinfo);

// --- Connexions ---
int esp01_get_active_connection_count(void);
bool esp01_is_connection_active(int conn_id);
ESP01_Status_t esp01_http_close_connection(int conn_id);
void esp01_cleanup_inactive_connections(void);
const char *esp01_http_get_client_ip(int conn_id);
void esp01_print_connection_status(void);

// --- Traitement automatique des requêtes ---
void esp01_process_requests(void);

// --- Outils internes ---
char *_find_next_ipd(char *buffer, int buffer_len);
http_request_t parse_ipd_header(const char *data);

// --- Clients AP ---
ESP01_Status_t esp01_set_multiple_connections(bool enable);
ESP01_Status_t esp01_list_ap_clients(char *out, size_t out_size);

#endif /* STM32_WIFIESP_HTTP_H_ */
