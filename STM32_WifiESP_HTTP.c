/**
 ******************************************************************************
 * @file    STM32_WifiESP_HTTP.c
 * @author  manu
 * @version 1.0.0
 * @date    2025
 * @brief   Implémentation des fonctions HTTP pour le module ESP01 WiFi.
 *
 * @details
 * Ce fichier contient l'implémentation des fonctions pour gérer les requêtes
 * HTTP (serveur et client) via un module ESP01 connecté à un microcontrôleur
 * STM32. Il propose des fonctions haut niveau pour le parsing, la gestion des
 * routes, l'envoi de réponses HTTP, la gestion des connexions et la
 * configuration du serveur web embarqué.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la bibliothèque STM32_WifiESP.h.
 *
 * @copyright
 * La licence de ce code est libre.
 ******************************************************************************
 */

// ==================== INCLUDES ====================

#include "STM32_WifiESP.h" // Fonctions de base ESP01
#include "STM32_WifiESP_WIFI.h"
#include "STM32_WifiESP_HTTP.h" // Header HTTP
#include <string.h>             // Pour memcpy, strncpy, memset, etc.
#include <stdio.h>              // Pour snprintf, printf

// ==================== DEFINES ====================
#define ESP01_CONN_TIMEOUT_MS 30000 // 30 secondes

// ==================== VARIABLES GLOBALES ====================
connection_info_t g_connections[ESP01_MAX_CONNECTIONS]; // Tableau des connexions actives
int g_connection_count = ESP01_MAX_CONNECTIONS;         // Nombre de connexions max supportées
volatile int g_acc_len = 0;                             // Longueur actuelle du buffer accumulateur
char g_accumulator[ESP01_MAX_TOTAL_HTTP] = {0};         // Buffer accumulateur pour les requêtes HTTP
volatile int g_processing_request = 0;                  // Indique si une requête est en cours de traitement

esp01_route_t g_routes[ESP01_MAX_ROUTES]; // Tableau des routes HTTP enregistrées
int g_route_count = 0;                    // Nombre de routes actuellement enregistrées

#define MAX_ROUTES ESP01_MAX_ROUTES // Alias pour la taille max des routes

// ==================== PAYLOAD/DISCARD ====================
// Vide le payload HTTP restant dans le buffer UART
void discard_http_payload(int expected_length)
{
    char discard_buf[ESP01_SMALL_BUF_SIZE]; // Buffer temporaire pour lecture
    int remaining = expected_length;        // Nombre d'octets restants à lire
    uint32_t timeout_start = HAL_GetTick(); // Début du timeout
    const uint32_t timeout_ms = 200;        // Timeout max en ms

    _esp_login("[HTTP] discard_http_payload: début vidage payload HTTP");

    while (remaining > 0 && (HAL_GetTick() - timeout_start) < timeout_ms) // Boucle jusqu'à tout lire ou timeout
    {
        int to_read = (remaining > sizeof(discard_buf)) ? sizeof(discard_buf) : remaining; // Taille à lire
        int read = esp01_get_new_data((uint8_t *)discard_buf, to_read);                    // Lecture UART
        if (read > 0)
        {
            remaining -= read;             // Met à jour le reste à lire
            timeout_start = HAL_GetTick(); // Reset le timeout si on lit
        }
        else
        {
            HAL_Delay(2); // Petite pause si rien reçu
        }
    }

    if (remaining > 0) // Si tout n'a pas été lu
    {
        _esp_login("[HTTP] AVERTISSEMENT: %d octets non lus (discard incomplet)", remaining);
    }
}

// ==================== PARSING REQUETE HTTP ====================
// Parse une requête HTTP brute en structure http_parsed_request_t
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed)
{
    VALIDATE_PARAM(raw_request, ESP01_FAIL); // Vérifie le pointeur d'entrée
    VALIDATE_PARAM(parsed, ESP01_FAIL);      // Vérifie le pointeur de sortie

    memset(parsed, 0, sizeof(http_parsed_request_t)); // Initialise la structure

    const char *p = raw_request; // Pointeur de parcours
    const char *method_start = p, *method_end = NULL;
    const char *path_start = NULL, *path_end = NULL;
    const char *query_start = NULL, *query_end = NULL;
    const char *line_end = strstr(p, "\r\n"); // Fin de la première ligne
    if (!line_end)
        return ESP01_FAIL; // Retourne erreur si pas de fin de ligne

    while (p < line_end && *p != ' ')
        p++; // Avance jusqu'à l'espace (fin de la méthode)
    method_end = p;
    if (method_end - method_start >= ESP01_MAX_HTTP_METHOD_LEN)
        return ESP01_FAIL;                                           // Méthode trop longue
    memcpy(parsed->method, method_start, method_end - method_start); // Copie la méthode
    parsed->method[method_end - method_start] = '\0';                // Termine la chaîne

    p++; // Passe l'espace
    path_start = p;
    while (p < line_end && *p != ' ' && *p != '?')
        p++; // Avance jusqu'à l'espace ou '?'
    path_end = p;
    if (path_end - path_start >= ESP01_MAX_HTTP_PATH_LEN)
        return ESP01_FAIL;                                   // Chemin trop long
    memcpy(parsed->path, path_start, path_end - path_start); // Copie le chemin
    parsed->path[path_end - path_start] = '\0';              // Termine la chaîne

    if (*p == '?') // Si présence d'une query string
    {
        p++;
        query_start = p;
        while (p < line_end && *p != ' ')
            p++; // Avance jusqu'à l'espace
        query_end = p;
        size_t qlen = query_end - query_start;
        if (qlen >= ESP01_MAX_HTTP_QUERY_LEN)
            qlen = ESP01_MAX_HTTP_QUERY_LEN - 1;
        memcpy(parsed->query_string, query_start, qlen); // Copie la query string
        parsed->query_string[qlen] = '\0';               // Termine la chaîne
    }
    else
    {
        parsed->query_string[0] = '\0'; // Pas de query string
    }

    parsed->is_valid = true; // Marque la requête comme valide
    return ESP01_OK;         // Retourne OK
}

// ==================== REPONSE HTTP GENERIQUE ====================
// Envoie une réponse HTTP générique sur une connexion
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len)
{
    VALIDATE_PARAM(conn_id >= 0, ESP01_FAIL);                            // Vérifie l'ID de connexion
    VALIDATE_PARAM(status_code >= 100 && status_code < 600, ESP01_FAIL); // Vérifie le code HTTP
    VALIDATE_PARAM(body || body_len == 0, ESP01_FAIL);                   // Vérifie le corps

    uint32_t start = HAL_GetTick(); // Début du traitement
    g_stats.total_requests++;       // Incrémente le nombre total de requêtes
    g_stats.response_count++;       // Incrémente le nombre de réponses envoyées
    if (status_code >= ESP01_HTTP_OK_CODE && status_code < 300)
    {
        g_stats.successful_responses++; // Incrémente le compteur de succès
    }
    else if (status_code >= 400)
    {
        g_stats.failed_responses++; // Incrémente le compteur d'échecs
    }

    char header[ESP01_MAX_HEADER_LINE]; // Buffer pour l'entête HTTP
    const char *status_text = "OK";     // Texte du statut HTTP
    switch (status_code)
    {
    case ESP01_HTTP_OK_CODE:
        status_text = "OK";
        break;
    case ESP01_HTTP_NOT_FOUND_CODE:
        status_text = "Not Found";
        break;
    case ESP01_HTTP_INTERNAL_ERR_CODE:
        status_text = "Internal Server Error";
        break;
    default:
        status_text = "Unknown";
        break;
    }

    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %d\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status_code, status_text, content_type ? content_type : "text/html", (int)body_len);

    char response[ESP01_MAX_TOTAL_HTTP]; // Buffer pour la réponse complète
    if ((header_len + body_len) >= sizeof(response))
    {
        _esp_login("[HTTP] esp01_send_http_response: réponse trop grande");
        return ESP01_FAIL; // Retourne erreur si la réponse est trop grande
    }

    memcpy(response, header, header_len); // Copie l'entête dans le buffer
    if (body && body_len > 0)
        memcpy(response + header_len, body, body_len); // Copie le corps

    int total_len = header_len + (int)body_len; // Taille totale de la réponse

    char cipsend_cmd[ESP01_MAX_CIPSEND_BUF]; // Buffer pour la commande AT+CIPSEND
    snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, total_len);

    char resp[ESP01_CMD_RESP_BUF_SIZE]; // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma(cipsend_cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_LONG);
    if (st != ESP01_OK)
    {
        _esp_login("[HTTP] esp01_send_http_response: AT+CIPSEND échoué");
        return st; // Retourne le statut d'erreur
    }

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)response, total_len, HAL_MAX_DELAY); // Envoie la réponse

    st = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_LONG); // Attend l'accusé d'envoi
    _esp_login("[HTTP] Envoi réponse HTTP sur connexion %d, taille de la page HTML : %d octets", conn_id, (int)body_len);

    uint32_t elapsed = HAL_GetTick() - start;  // Temps écoulé pour la réponse
    g_stats.total_response_time_ms += elapsed; // Mise à jour des stats
    g_stats.avg_response_time_ms = g_stats.response_count ? (g_stats.total_response_time_ms / g_stats.response_count) : 0;

    return st; // Retourne le statut final
}

// ==================== REPONSES SPECIFIQUES ====================
// Envoie une réponse JSON
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data)
{
    _esp_login("[HTTP] Envoi d'une réponse JSON");
    return esp01_send_http_response(conn_id, 200, "application/json", json_data, strlen(json_data));
}

// Envoie une réponse 404 Not Found
ESP01_Status_t esp01_send_404_response(int conn_id)
{
    _esp_login("[HTTP] Envoi d'une réponse 404");
    const char *body = ESP01_HTTP_404_BODY; // Corps de la réponse 404
    return esp01_send_http_response(conn_id, 404, "text/html", body, strlen(body));
}

// ==================== HTTP GET CLIENT ====================
// Effectue une requête HTTP GET en client
ESP01_Status_t esp01_http_get(const char *host, uint16_t port, const char *path, char *response, size_t response_size)
{
    _esp_login("[HTTP] esp01_http_get: GET http://%s:%u%s", host, port, path);

    char cmd[ESP01_DMA_RX_BUF_SIZE]; // Buffer pour la commande AT+CIPSTART
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", host, port);
    char resp[ESP01_DMA_RX_BUF_SIZE]; // Buffer pour la réponse AT
    if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000) != ESP01_OK)
    {
        _esp_login("[HTTP] esp01_http_get: AT+CIPSTART échoué");
        return ESP01_FAIL;
    }

    char http_req[ESP01_MAX_HTTP_REQ_BUF]; // Buffer pour la requête HTTP GET
    snprintf(http_req, sizeof(http_req),
             "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", (int)strlen(http_req)); // Prépare la commande AT+CIPSEND
    if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", 3000) != ESP01_OK)
        return ESP01_FAIL;

    if (esp01_send_raw_command_dma(http_req, resp, sizeof(resp), "CLOSED", 8000) != ESP01_OK)
        return ESP01_FAIL;

    if (response && response_size > 0)
    {
        strncpy(response, resp, response_size - 1); // Copie la réponse dans le buffer utilisateur
        response[response_size - 1] = '\0';         // Termine la chaîne
    }

    return ESP01_OK; // Retourne OK
}

// ==================== ROUTES HTTP ====================
// Efface toutes les routes HTTP
void esp01_clear_routes(void)
{
    _esp_login("[ROUTE] Effacement de toutes les routes HTTP");
    g_route_count = 0; // Réinitialise le compteur de routes
}

// Ajoute une route HTTP
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler)
{
    VALIDATE_PARAM(path, ESP01_FAIL);                             // Vérifie le chemin
    VALIDATE_PARAM(handler, ESP01_FAIL);                          // Vérifie le handler
    VALIDATE_PARAM(g_route_count < ESP01_MAX_ROUTES, ESP01_FAIL); // Vérifie la limite

    strncpy(g_routes[g_route_count].path, path, sizeof(g_routes[g_route_count].path) - 1); // Copie le chemin
    g_routes[g_route_count].path[sizeof(g_routes[g_route_count].path) - 1] = '\0';         // Termine la chaîne
    g_routes[g_route_count].handler = handler;                                             // Associe le handler
    g_route_count++;                                                                       // Incrémente le compteur
    _esp_login("[WEB] Route ajoutée : %s (total %d)", path, g_route_count);
    return ESP01_OK; // Retourne OK
}

// Recherche le handler d'une route HTTP
esp01_route_handler_t esp01_find_route_handler(const char *path)
{
    for (int i = 0; i < g_route_count; i++) // Parcourt toutes les routes
    {
        if (strcmp(g_routes[i].path, path) == 0) // Si le chemin correspond
        {
            _esp_login("[WEB] Route trouvée pour path : %s", path);
            return g_routes[i].handler; // Retourne le handler trouvé
        }
    }
    _esp_login("[WEB] Aucune route trouvée pour path : %s", path);
    return NULL; // Aucun handler trouvé
}

// ==================== TRAITEMENT DES REQUETES ====================
// Traite les requêtes HTTP reçues
void esp01_process_requests(void)
{
    if (g_processing_request) // Si déjà en cours de traitement
        return;
    g_processing_request = true; // Marque comme en traitement

    uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];                     // Buffer temporaire pour UART
    uint16_t len = esp01_get_new_data(buffer, sizeof(buffer)); // Récupère les nouvelles données UART
    if (len == 0)
    {
        g_processing_request = false; // Rien à traiter
        return;
    }

    if (g_acc_len + len < sizeof(g_accumulator) - 1)
    {
        memcpy(g_accumulator + g_acc_len, buffer, len); // Ajoute au buffer accumulateur
        g_acc_len += len;                               // Met à jour la longueur
        g_accumulator[g_acc_len] = '\0';                // Termine la chaîne
    }
    else
    {
        g_acc_len = 0;                // Réinitialise le buffer
        g_accumulator[0] = '\0';      // Vide le buffer
        g_processing_request = false; // Fin du traitement
        return;
    }

    char *ipd_start = _find_next_ipd(g_accumulator, g_acc_len); // Cherche le début d'un paquet IPD
    if (!ipd_start)
    {
        g_processing_request = false; // Rien à traiter
        return;
    }

    _esp_login("%s", ipd_start); // Log debug du paquet IPD

    http_request_t req = parse_ipd_header(ipd_start); // Parse l'entête IPD
    if (!req.is_valid)
    {
        g_processing_request = false; // Paquet non valide
        return;
    }

    // Correction : mettre à jour g_connection_count si besoin
    if (req.conn_id >= 0 && req.conn_id < ESP01_MAX_CONNECTIONS)
    {
        if (req.conn_id >= g_connection_count)
            g_connection_count = req.conn_id + 1; // Met à jour le nombre de connexions

        g_connections[req.conn_id].is_active = true;              // Marque la connexion active
        g_connections[req.conn_id].last_activity = HAL_GetTick(); // Met à jour l'activité
        g_connections[req.conn_id].conn_id = req.conn_id;         // Stocke l'ID
        if (req.has_ip)
            strncpy(g_connections[req.conn_id].client_ip, req.client_ip, sizeof(g_connections[req.conn_id].client_ip) - 1); // Copie l'IP
        else
            g_connections[req.conn_id].client_ip[0] = '\0';                                            // Pas d'IP
        g_connections[req.conn_id].client_ip[sizeof(g_connections[req.conn_id].client_ip) - 1] = '\0'; // Termine la chaîne
        g_connections[req.conn_id].client_port = req.client_port;                                      // Stocke le port
    }

    char *payload_start = strchr(ipd_start, ':'); // Cherche le début du payload
    if (!payload_start)
    {
        g_processing_request = false; // Pas de payload
        return;
    }
    payload_start++; // Passe le ':'

    int payload_length = req.content_length - (payload_start - ipd_start); // Calcule la longueur du payload
    if (payload_length <= 0)
    {
        g_processing_request = false; // Rien à traiter
        return;
    }

    if (payload_length > (g_acc_len - (payload_start - g_accumulator)))
    {
        g_processing_request = false; // Pas assez de données
        return;
    }

    if (req.is_valid)
    {
        int payload_offset = payload_start - g_accumulator; // Offset du payload
        if (payload_offset < 0 || payload_offset > g_acc_len)
        {
            _esp_login("[ERROR] payload_start hors accumulateur !");
            g_processing_request = false;
            return;
        }
        int payload_in_buffer = g_acc_len - payload_offset; // Nombre d'octets dans le buffer
        if (payload_in_buffer < 0)
            payload_in_buffer = 0;
        int to_discard = req.content_length - payload_in_buffer; // Octets à jeter
        if (to_discard < 0)
            to_discard = 0;
        _esp_login("[DEBUG] payload_offset=%d, payload_in_buffer=%d, to_discard=%d, content_length=%d", payload_offset, payload_in_buffer, to_discard, req.content_length);

        if (to_discard > 0)
        {
            discard_http_payload(to_discard); // Vide le reste du payload
            _flush_rx_buffer(20);             // Vide le buffer UART
        }
    }

    http_parsed_request_t parsed_request = {0};               // Structure de requête parsée
    esp01_parse_http_request(payload_start, &parsed_request); // Parse la requête HTTP
    _esp_login("[ROUTE] Traitement de la requête pour %s", parsed_request.path);

    esp01_route_handler_t handler = esp01_find_route_handler(parsed_request.path); // Cherche le handler
    if (handler)
    {
        handler(req.conn_id, &parsed_request); // Appelle le handler
    }
    else
    {
        esp01_send_404_response(req.conn_id); // Réponse 404 si pas de handler
    }

    g_acc_len = 0;                // Réinitialise le buffer accumulateur
    g_accumulator[0] = '\0';      // Vide le buffer
    g_processing_request = false; // Fin du traitement
}

// ==================== INIT/LOOP HTTP ====================
// Initialise le module HTTP
void esp01_http_init(void)
{
    _esp_login("[HTTP] Initialisation du module HTTP"); // Log d'init
    esp01_clear_routes();                               // Efface toutes les routes
    g_acc_len = 0;                                      // Réinitialise le buffer accumulateur
    g_accumulator[0] = '\0';                            // Vide le buffer
}

// Boucle principale HTTP (à appeler régulièrement)
void esp01_http_loop(void)
{
    esp01_process_requests(); // Traite les requêtes HTTP
    // Debug : afficher les connexions actives
    /*
    for (int i = 0; i < g_connection_count; ++i)
    {
        printf("DEBUG: Connexion %d active=%d IP=%s Port=%u\n", i, g_connections[i].is_active, g_connections[i].client_ip, g_connections[i].client_port);
    }
    */
}

// ==================== SERVEUR WEB ====================
// Démarre le serveur web embarqué
ESP01_Status_t esp01_start_server_config(bool multi_conn, uint16_t port, bool ipdinfo)
{
    char cmd[64], resp[256]; // Buffers pour commandes et réponses

    snprintf(cmd, sizeof(cmd), "AT+CIPMUX=%d", multi_conn ? 1 : 0); // Configure le mode multi-connexion
    if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000) != ESP01_OK)
        return ESP01_FAIL;

    snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port); // Démarre le serveur sur le port donné
    if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000) != ESP01_OK)
        return ESP01_FAIL;

    // Active ou désactive l'affichage de l'IP client dans les +IPD selon le paramètre
    if (ipdinfo)
    {
        char resp[64];
        esp01_send_raw_command_dma("AT+CIPDINFO=1", resp, sizeof(resp), "OK", 1000);
    }
    else
    {
        char resp[64];
        esp01_send_raw_command_dma("AT+CIPDINFO=0", resp, sizeof(resp), "OK", 1000);
    }

    return ESP01_OK; // Retourne OK
}

// Arrête le serveur web
ESP01_Status_t esp01_stop_web_server(void)
{
    char resp[256]; // Buffer pour la réponse AT
    if (esp01_send_raw_command_dma("AT+CIPSERVER=0", resp, sizeof(resp), "OK", 2000) != ESP01_OK)
        return ESP01_FAIL;
    return ESP01_OK; // Retourne OK
}

// ==================== CONNEXIONS TCP ====================
// Retourne le nombre de connexions actives
int esp01_get_active_connection_count(void)
{
    int count = 0;                               // Compteur de connexions actives
    for (int i = 0; i < g_connection_count; ++i) // Parcourt toutes les connexions
        if (g_connections[i].is_active)          // Si la connexion est active
            count++;                             // Incrémente le compteur
    return count;                                // Retourne le nombre de connexions actives
}

// Vérifie si une connexion est active
bool esp01_is_connection_active(int conn_id)
{
    if (conn_id < 0 || conn_id >= g_connection_count) // Vérifie l'ID
        return false;
    return g_connections[conn_id].is_active; // Retourne l'état de la connexion
}

// Nettoie les connexions inactives
void esp01_cleanup_inactive_connections(void)
{
    uint32_t now = HAL_GetTick();                // Temps courant
    for (int i = 0; i < g_connection_count; ++i) // Parcourt toutes les connexions
    {
        if (g_connections[i].is_active &&
            (now - g_connections[i].last_activity > ESP01_CONN_TIMEOUT_MS)) // Si la connexion est inactive trop longtemps
        {
            g_connections[i].is_active = false;   // Marque comme inactive
            g_connections[i].client_ip[0] = '\0'; // Vide l'IP
            g_connections[i].client_port = 0;     // Vide le port
        }
        if (!g_connections[i].is_active)
            memset(&g_connections[i], 0, sizeof(connection_info_t)); // Réinitialise la structure
    }
}

// ==================== PARSING +IPD ====================
// Cherche le prochain +IPD dans le buffer
char *_find_next_ipd(char *buffer, int buffer_len)
{
    for (int i = 0; i < buffer_len - 4; ++i)     // Parcourt le buffer
        if (memcmp(&buffer[i], "+IPD,", 5) == 0) // Cherche la séquence "+IPD,"
            return &buffer[i];                   // Retourne le pointeur trouvé
    return NULL;                                 // Aucun +IPD trouvé
}

// Parse l'entête d'un paquet +IPD
http_request_t parse_ipd_header(const char *data)
{
    http_request_t req = {0};                    // Initialise la structure
    if (!data || strncmp(data, "+IPD,", 5) != 0) // Vérifie le format
        return req;

    int conn_id = -1, content_length = 0, client_port = 0; // Variables temporaires
    char client_ip[ESP01_MAX_IP_LEN] = {0};                // Buffer pour l'IP

    // Nouveau format : +IPD,<id>,<len>,"<ip>",<port>:
    int n = sscanf(data, "+IPD,%d,%d,\"%[^\"]\",%d:", &conn_id, &content_length, client_ip, &client_port);
    if (n == 4)
    {
        req.conn_id = conn_id;                                        // ID de connexion
        req.content_length = content_length;                          // Longueur du contenu
        req.has_ip = true;                                            // IP présente
        strncpy(req.client_ip, client_ip, sizeof(req.client_ip) - 1); // Copie l'IP
        req.client_ip[sizeof(req.client_ip) - 1] = '\0';              // Termine la chaîne
        req.client_port = client_port;                                // Port du client
        req.is_valid = true;                                          // Marque comme valide
    }
    else if (sscanf(data, "+IPD,%d,%d:", &conn_id, &content_length) == 2)
    {
        req.conn_id = conn_id;               // ID de connexion
        req.content_length = content_length; // Longueur du contenu
        req.has_ip = false;                  // Pas d'IP
        req.client_ip[0] = '\0';             // Vide l'IP
        req.client_port = 0;                 // Pas de port
        req.is_valid = true;                 // Marque comme valide
    }
    return req; // Retourne la structure
}

// ==================== UTILITAIRE IP CLIENT ====================
// Retourne l'adresse IP du client pour une connexion donnée
const char *esp01_http_get_client_ip(int conn_id)
{
    if (conn_id < 0 || conn_id >= g_connection_count) // Vérifie l'ID
        return "N/A";
    if (!g_connections[conn_id].is_active || !g_connections[conn_id].client_ip[0]) // Vérifie l'état et l'IP
        return "N/A";
    return g_connections[conn_id].client_ip; // Retourne l'IP du client
}

// ==================== DEBUG CONNECTION STATUS ====================
// Affiche l'état des connexions dans le terminal
void esp01_print_connection_status(void)
{
    printf("[ESP01] Etat des connexions :\r\n");
    for (int i = 0; i < g_connection_count; ++i)
    {
        if (esp01_is_connection_active(i))
        {
            const connection_info_t *c = &g_connections[i];
            printf("  Connexion %d : IP=%s, Port=%u, Dernière activité=%lu ms\r\n",
                   c->conn_id, c->client_ip, c->client_port, (unsigned long)(HAL_GetTick() - c->last_activity));
        }
    }
}
