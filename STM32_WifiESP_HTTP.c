/**
 ******************************************************************************
 * @file    STM32_WifiESP_HTTP.c
 * @author  manu
 * @version 1.2.0
 * @date    17 juin 2025
 * @brief   Fonctions HTTP haut niveau pour ESP01 (serveur, requêtes, parsing, routes)
 *
 * @details
 * Ce fichier source contient l’implémentation des fonctions HTTP haut niveau pour le module ESP01 :
 * - Gestion du serveur HTTP intégré (démarrage, arrêt, configuration)
 * - Parsing des requêtes HTTP reçues via +IPD
 * - Routage des requêtes HTTP vers des handlers personnalisés
 * - Envoi de réponses HTTP (HTML, JSON, 404, etc.)
 * - Gestion des connexions TCP entrantes (suivi, fermeture, nettoyage)
 * - Statistiques d’utilisation HTTP
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la bibliothèque STM32_WifiESP.h.
 * - Nécessite le fichier STM32_WifiESP_WIFI.h pour les fonctions WiFi.
 *
 * @copyright
 * La licence de ce code est libre.
 ******************************************************************************
 */

// ==================== INCLUDES ====================

#include "STM32_WifiESP.h"      // Inclusion du header principal du driver ESP01
#include "STM32_WifiESP_WIFI.h" // Inclusion du header pour la gestion WiFi
#include "STM32_WifiESP_HTTP.h" // Inclusion du header HTTP (déclarations des structures et fonctions)
#include <string.h>             // Pour les fonctions de manipulation de chaînes (memcpy, memset, etc.)
#include <stdio.h>              // Pour les fonctions d'entrée/sortie (snprintf, sscanf, etc.)
#include <stdbool.h>            // Pour le type booléen
#include <stdint.h>				// Pout le type int

// ==================== DEFINES ====================
#define ESP01_CONN_TIMEOUT_MS 30000 ///< Timeout de connexion TCP (ms)

// ==================== CONSTANTES SPÉCIFIQUES AU MODULE ====================
#define ESP01_HTTP_REQUEST_TIMEOUT 5000   // Timeout requête HTTP (ms)
#define ESP01_HTTP_RESPONSE_TIMEOUT 10000 // Timeout réponse HTTP (ms)

// ==================== VARIABLES GLOBALES ====================
connection_info_t g_connections[ESP01_MAX_CONNECTIONS] = {0}; // Tableau des connexions TCP actives
int g_connection_count = ESP01_MAX_CONNECTIONS;               // Nombre maximal de connexions
volatile int g_acc_len = 0;                                   // Longueur courante de l'accumulateur HTTP
char g_accumulator[ESP01_MAX_TOTAL_HTTP] = {0};               // Buffer accumulateur pour les données HTTP reçues
volatile int g_processing_request = 0;                        // Indicateur de traitement en cours d'une requête HTTP
esp01_route_t g_routes[ESP01_MAX_ROUTES] = {0};               // Tableau des routes HTTP enregistrées
int g_route_count = 0;                                        // Nombre de routes HTTP enregistrées
esp01_stats_t g_stats = {0};                                  // Statistiques HTTP globales
extern uint16_t g_server_port;                                // Port du serveur HTTP

// ==================== OUTILS FACTORISÉS ====================

/**
 * @brief Recherche le prochain paquet +IPD dans le buffer.
 * @param buffer      Buffer à analyser.
 * @param buffer_len  Taille du buffer.
 * @retval Pointeur vers le début du +IPD, ou NULL si non trouvé.
 */
static char *_find_next_ipd(char *buffer, int buffer_len)
{
    for (int i = 0; i < buffer_len - 4; ++i)     // Parcours le buffer jusqu'à buffer_len-4
        if (memcmp(&buffer[i], "+IPD,", 5) == 0) // Compare les 5 caractères avec "+IPD,"
            return &buffer[i];                   // Retourne le pointeur si trouvé
    return NULL;                                 // Retourne NULL si non trouvé
}

/**
 * @brief Parse l'en-tête +IPD pour extraire les infos de la requête.
 * @param data  Chaîne brute commençant par +IPD.
 * @retval Structure http_request_t remplie.
 */
static http_request_t parse_ipd_header(const char *data)
{
    http_request_t req = {0};                    // Initialise la structure à zéro
    if (!data || strncmp(data, "+IPD,", 5) != 0) // Vérifie que la chaîne commence bien par "+IPD,"
        return req;                              // Retourne une structure vide si ce n'est pas le cas

    int conn_id = -1, content_length = 0, client_port = 0; // Variables pour stocker les valeurs extraites
    char client_ip[ESP01_MAX_IP_LEN] = {0};                // Buffer pour l'adresse IP du client

    int n = sscanf(data, "+IPD,%d,%d,\"%[^\"]\",%d:", &conn_id, &content_length, client_ip, &client_port); // Tente de parser la version longue avec IP et port
    if (n == 4)                                                                                            // Si les 4 champs sont trouvés
    {
        req.conn_id = conn_id;                                              // Stocke l'identifiant de connexion
        req.content_length = content_length;                                // Stocke la longueur du contenu
        req.has_ip = true;                                                  // Indique que l'IP est présente
        esp01_safe_strcpy(req.client_ip, sizeof(req.client_ip), client_ip); // Copie l'adresse IP
        esp01_trim_string(req.client_ip);                                   // Supprime les espaces superflus
        req.client_port = client_port;                                      // Stocke le port du client
        req.is_valid = true;                                                // Indique que la structure est valide
    }
    else if (sscanf(data, "+IPD,%d,%d:", &conn_id, &content_length) == 2) // Sinon, tente la version courte sans IP
    {
        req.conn_id = conn_id;               // Stocke l'identifiant de connexion
        req.content_length = content_length; // Stocke la longueur du contenu
        req.has_ip = false;                  // Indique que l'IP n'est pas présente
        req.client_ip[0] = '\0';             // Chaîne IP vide
        req.client_port = 0;                 // Port à zéro
        req.is_valid = true;                 // Indique que la structure est valide
    }
    return req; // Retourne la structure remplie
}

// ==================== ROUTES ====================

/**
 * @brief Efface toutes les routes HTTP enregistrées.
 */
void esp01_clear_routes(void)
{
    ESP01_LOG_DEBUG("HTTP", "Effacement de toutes les routes HTTP"); // Log l'effacement des routes
    memset(g_routes, 0, sizeof(g_routes));                           // Réinitialise le tableau des routes à zéro
    g_route_count = 0;                                               // Réinitialise le compteur de routes
}

/**
 * @brief Ajoute une route HTTP et son handler.
 * @param path     Chemin de la route (ex: "/status").
 * @param handler  Fonction handler associée.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler)
{
    ESP01_LOG_DEBUG("HTTP", "Ajout de la route : %s", path);                        // Log l'ajout de la route
    VALIDATE_PARAM(path && handler, ESP01_INVALID_PARAM);                           // Vérifie les paramètres
    if (g_route_count >= ESP01_MAX_ROUTES)                                          // Vérifie qu'il reste de la place
        ESP01_RETURN_ERROR("ADD_ROUTE", ESP01_FAIL);                                // Retourne une erreur si trop de routes
    esp01_safe_strcpy(g_routes[g_route_count].path, ESP01_MAX_HTTP_PATH_LEN, path); // Copie le chemin de la route
    g_routes[g_route_count].handler = handler;                                      // Associe le handler
    g_route_count++;                                                                // Incrémente le compteur de routes
    ESP01_LOG_DEBUG("HTTP", "Route ajoutée : %s (total=%d)", path, g_route_count);  // Log la réussite
    return ESP01_OK;                                                                // Retourne OK
}

/**
 * @brief Supprime une route HTTP enregistrée.
 * @param path  Chemin de la route à supprimer (ex: "/status").
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_remove_route(const char *path)
{
    VALIDATE_PARAM(path, ESP01_INVALID_PARAM);                     // Vérifie le paramètre
    ESP01_LOG_DEBUG("HTTP", "Suppression de la route : %s", path); // Log la suppression de la route

    for (int i = 0; i < g_route_count; ++i) // Parcours les routes enregistrées
    {
        if (strcmp(g_routes[i].path, path) == 0) // Si la route correspondante est trouvée
        {
            // Décale les routes suivantes vers le haut
            memmove(&g_routes[i], &g_routes[i + 1], (g_route_count - i - 1) * sizeof(esp01_route_t));
            g_route_count--;                                                                 // Décrémente le compteur de routes
            ESP01_LOG_DEBUG("HTTP", "Route supprimée : %s (total=%d)", path, g_route_count); // Log la réussite
            return ESP01_OK;                                                                 // Retourne OK
        }
    }

    ESP01_LOG_WARN("HTTP", "Route non trouvée pour suppression : %s", path); // Log un avertissement si la route n'est pas trouvée
    return ESP01_FAIL;                                                       // Retourne une erreur
}

/**
 * @brief Recherche le handler associé à une route HTTP.
 * @param path  Chemin de la route recherchée.
 * @retval Pointeur vers le handler, ou NULL si non trouvé.
 */
esp01_route_handler_t esp01_find_route_handler(const char *path)
{
    ESP01_LOG_DEBUG("HTTP", "Recherche du handler pour la route : %s", path); // Log la recherche
    for (int i = 0; i < g_route_count; ++i)                                   // Parcours toutes les routes enregistrées
        if (strncmp(path, g_routes[i].path, ESP01_MAX_HTTP_PATH_LEN) == 0)    // Compare le chemin
            return g_routes[i].handler;                                       // Retourne le handler si trouvé
    ESP01_LOG_DEBUG("HTTP", "Aucun handler trouvé pour la route : %s", path); // Log si non trouvé
    return NULL;                                                              // Retourne NULL si aucun handler trouvé
}

// ==================== INIT & SERVEUR ====================

/**
 * @brief Initialise le module HTTP (routes, connexions, accumulateur).
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_http_init(void)
{
    ESP01_LOG_DEBUG("HTTP", "Initialisation du module HTTP"); // Log l'initialisation
    memset(g_connections, 0, sizeof(g_connections));          // Réinitialise les connexions
    g_connection_count = ESP01_MAX_CONNECTIONS;               // Réinitialise le compteur de connexions
    g_acc_len = 0;                                            // Réinitialise la longueur de l'accumulateur
    memset(g_accumulator, 0, sizeof(g_accumulator));          // Vide l'accumulateur
    g_processing_request = 0;                                 // Réinitialise l'indicateur de traitement
    esp01_clear_routes();                                     // Efface toutes les routes HTTP
    return ESP01_OK;                                          // Retourne OK
}

/**
 * @brief Démarre le serveur HTTP sur le port spécifié.
 * @param port  Port TCP à ouvrir.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_http_start_server(uint16_t port)
{
    ESP01_LOG_DEBUG("HTTP", "Démarrage du serveur HTTP sur le port %u", port);                          // Log le démarrage du serveur
    char cmd[ESP01_MAX_CMD_BUF];                                                                        // Buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port);                                              // Prépare la commande AT pour démarrer le serveur
    char resp[ESP01_MAX_RESP_BUF];                                                                      // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT
    if (st != ESP01_OK)                                                                                 // Vérifie le statut de la commande
        ESP01_RETURN_ERROR("HTTP_SERVER", st);                                                          // Retourne une erreur si échec
    ESP01_LOG_DEBUG("HTTP", "Serveur HTTP démarré sur le port %u", port);                               // Log la réussite
    g_server_port = port;                                                                               // Enregistre le port du serveur dans la variable globale
    return ESP01_OK;                                                                                    // Retourne OK
}

/**
 * @brief Arrête le serveur HTTP.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_http_stop_server(void)
{
    ESP01_LOG_DEBUG("HTTP", "Arrêt du serveur HTTP");                                                                // Log l'arrêt du serveur
    char resp[ESP01_MAX_RESP_BUF];                                                                                   // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSERVER=0", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT pour arrêter le serveur
    if (st != ESP01_OK)                                                                                              // Vérifie le statut de la commande
        ESP01_RETURN_ERROR("HTTP_STOP", st);                                                                         // Retourne une erreur si échec
    ESP01_LOG_DEBUG("HTTP", "Serveur HTTP arrêté");                                                                  // Log la réussite
    return ESP01_OK;                                                                                                 // Retourne OK
}

/**
 * @brief Configure le serveur HTTP (mode multi-connexion, port, IPDINFO).
 * @param multi_conn  true pour activer le multi-connexion.
 * @param port        Port TCP à ouvrir.
 * @param ipdinfo     true pour activer l'affichage IP/port client.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_start_server_config(bool multi_conn, uint16_t port, bool ipdinfo)
{
    ESP01_LOG_DEBUG("HTTP", "Configuration du serveur : multi_conn=%d, port=%u, ipdinfo=%d", multi_conn, port, ipdinfo); // Log la configuration
    ESP01_Status_t st = ESP01_OK;                                                                                        // Statut de retour
    char resp[ESP01_MAX_RESP_BUF];                                                                                       // Buffer pour la réponse AT

    if (multi_conn) // Si le mode multi-connexion est demandé
    {
        st = esp01_send_raw_command_dma("AT+CIPMUX=1", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Active le multi-connexion
        if (st != ESP01_OK)                                                                            // Vérifie le statut
            ESP01_RETURN_ERROR("CIPMUX", st);                                                          // Retourne une erreur si échec
    }

    if (ipdinfo) // Si l'affichage IP/port client est demandé
    {
        st = esp01_send_raw_command_dma("AT+CIPDINFO=1", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Active l'affichage IP/port
        if (st != ESP01_OK)                                                                              // Vérifie le statut
            ESP01_RETURN_ERROR("CIPDINFO", st);                                                          // Retourne une erreur si échec
    }

    return esp01_http_start_server(port); // Démarre le serveur HTTP sur le port spécifié
}

// ==================== PARSING REQUÊTES HTTP ====================

/**
 * @brief Parse une requête HTTP brute en structure http_parsed_request_t.
 * @param raw_request  Chaîne brute de la requête HTTP.
 * @param parsed       Pointeur vers la structure de sortie.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed)
{
    ESP01_LOG_DEBUG("HTTP", "Parsing de la requête HTTP...");
    VALIDATE_PARAM(raw_request, ESP01_INVALID_PARAM);
    VALIDATE_PARAM(parsed, ESP01_INVALID_PARAM);

    memset(parsed, 0, sizeof(http_parsed_request_t));

    const char *p = raw_request;
    const char *method_start = p, *method_end = NULL;
    const char *path_start = NULL, *path_end = NULL;
    const char *query_start = NULL, *query_end = NULL;
    const char *line_end = strstr(p, "\r\n");
    if (!line_end)
    {
        ESP01_RETURN_ERROR("HTTP_PARSE", ESP01_PARSE_ERROR);
    }

    while (p < line_end && *p != ' ') // Avance jusqu'à l'espace après la méthode
        p++;
    method_end = p;                                                  // Marque la fin de la méthode
    if (method_end - method_start >= ESP01_MAX_HTTP_METHOD_LEN)      // Vérifie la taille de la méthode
        return ESP01_FAIL;                                           // Retourne une erreur si trop long
    memcpy(parsed->method, method_start, method_end - method_start); // Copie la méthode
    parsed->method[method_end - method_start] = '\0';                // Termine la chaîne
    esp01_trim_string(parsed->method);                               // Supprime les espaces superflus

    p++;                                           // Passe l'espace
    path_start = p;                                // Début du chemin
    while (p < line_end && *p != ' ' && *p != '?') // Avance jusqu'à l'espace ou '?'
        p++;
    path_end = p;                                            // Fin du chemin
    if (path_end - path_start >= ESP01_MAX_HTTP_PATH_LEN)    // Vérifie la taille du chemin
        return ESP01_FAIL;                                   // Retourne une erreur si trop long
    memcpy(parsed->path, path_start, path_end - path_start); // Copie le chemin
    parsed->path[path_end - path_start] = '\0';              // Termine la chaîne
    esp01_trim_string(parsed->path);                         // Supprime les espaces superflus

    if (*p == '?') // Si une query string est présente
    {
        p++;                              // Passe le '?'
        query_start = p;                  // Début de la query string
        while (p < line_end && *p != ' ') // Avance jusqu'à l'espace
            p++;
        query_end = p;                                   // Fin de la query string
        size_t qlen = query_end - query_start;           // Calcule la longueur
        if (qlen >= ESP01_MAX_HTTP_QUERY_LEN)            // Vérifie la taille
            qlen = ESP01_MAX_HTTP_QUERY_LEN - 1;         // Tronque si nécessaire
        memcpy(parsed->query_string, query_start, qlen); // Copie la query string
        parsed->query_string[qlen] = '\0';               // Termine la chaîne
        esp01_trim_string(parsed->query_string);         // Supprime les espaces superflus
    }
    else
    {
        parsed->query_string[0] = '\0'; // Pas de query string
    }

    parsed->is_valid = true;                                                                                      // Indique que le parsing est valide
    ESP01_LOG_DEBUG("HTTP", "Méthode=%s, Path=%s, Query=%s", parsed->method, parsed->path, parsed->query_string); // Log le résultat du parsing
    return ESP01_OK;                                                                                              // Retourne OK
}

// ==================== ENVOI DE RÉPONSES ====================

/**
 * @brief Envoie une réponse JSON sur la connexion spécifiée.
 * @param conn_id  Identifiant de connexion.
 * @param json     Chaîne JSON à envoyer.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json)
{
    ESP01_LOG_DEBUG("HTTP", "Envoi d'une réponse JSON sur connexion %d", conn_id);                        // Log l'envoi JSON
    return esp01_send_http_response(conn_id, ESP01_HTTP_OK_CODE, "application/json", json, strlen(json)); // Envoie la réponse HTTP avec le JSON
}

/**
 * @brief Envoie une réponse 404 Not Found sur la connexion spécifiée.
 * @param conn_id  Identifiant de connexion.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_send_404_response(int conn_id)
{
    ESP01_LOG_DEBUG("HTTP", "404 Not Found envoyé sur connexion %d", conn_id);                            // Log l'envoi 404
    const char *body = "<html><body><h1>404 Not Found</h1></body></html>";                                // Corps HTML pour la 404
    return esp01_send_http_response(conn_id, ESP01_HTTP_NOT_FOUND_CODE, "text/html", body, strlen(body)); // Envoie la réponse HTTP 404
}

/**
 * @brief Envoie une réponse HTTP complète (header + body).
 * @param conn_id      Identifiant de connexion.
 * @param status_code  Code HTTP (ex: 200, 404).
 * @param content_type Type MIME (ex: "text/html").
 * @param body         Corps de la réponse.
 * @param body_len     Taille du corps.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len)
{
    ESP01_LOG_DEBUG("HTTP", "Préparation de la réponse HTTP (conn_id=%d, code=%d, type=%s, taille=%d)", conn_id, status_code, content_type ? content_type : "NULL", (int)body_len); // Log la préparation de la réponse
    VALIDATE_PARAM(conn_id >= 0, ESP01_FAIL);                                                                                                                                       // Vérifie l'identifiant de connexion
    VALIDATE_PARAM(status_code >= 100 && status_code < 600, ESP01_FAIL);                                                                                                            // Vérifie le code HTTP
    VALIDATE_PARAM(body || body_len == 0, ESP01_FAIL);                                                                                                                              // Vérifie le corps de la réponse

    uint32_t start = HAL_GetTick();                             // Timestamp de début pour les stats
    g_stats.total_requests++;                                   // Incrémente le nombre total de requêtes
    g_stats.response_count++;                                   // Incrémente le nombre de réponses envoyées
    if (status_code >= ESP01_HTTP_OK_CODE && status_code < 300) // Si code 2xx
        g_stats.successful_responses++;                         // Incrémente les réponses réussies
    else if (status_code >= 400)                                // Si code 4xx ou 5xx
        g_stats.failed_responses++;                             // Incrémente les réponses échouées

    char header[ESP01_MAX_HEADER_LINE]; // Buffer pour l'en-tête HTTP
    const char *status_text = "OK";     // Texte du statut HTTP
    switch (status_code)                // Sélectionne le texte selon le code
    {
    case ESP01_HTTP_OK_CODE:
        status_text = "OK"; // 200
        break;
    case ESP01_HTTP_NOT_FOUND_CODE:
        status_text = "Not Found"; // 404
        break;
    case ESP01_HTTP_INTERNAL_ERR_CODE:
        status_text = "Internal Server Error"; // 500
        break;
    case 204:
        status_text = "No Content"; // 204
        break;
    default:
        status_text = "Unknown"; // Autre
        break;
    }

    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %d\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status_code, status_text, content_type ? content_type : "text/html", (int)body_len); // Prépare l'en-tête HTTP

    char response[ESP01_MAX_TOTAL_HTTP];             // Buffer pour la réponse complète
    if ((header_len + body_len) >= sizeof(response)) // Vérifie la taille totale
    {
        ESP01_LOG_ERROR("HTTP", "Réponse HTTP trop grande (header=%d, body=%d, max=%d)", header_len, (int)body_len, (int)sizeof(response)); // Log l'erreur
        return ESP01_FAIL;                                                                                                                  // Retourne une erreur
    }

    memcpy(response, header, header_len);              // Copie l'en-tête dans le buffer
    if (body && body_len > 0)                          // Si un corps est présent
        memcpy(response + header_len, body, body_len); // Copie le corps dans le buffer

    int total_len = header_len + (int)body_len; // Calcule la taille totale

    char cipsend_cmd[ESP01_MAX_CIPSEND_BUF];                                            // Buffer pour la commande AT+CIPSEND
    snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, total_len); // Prépare la commande AT+CIPSEND

    char resp[ESP01_MAX_RESP_BUF];                                                                            // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma(cipsend_cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_LONG); // Envoie la commande AT+CIPSEND
    if (st != ESP01_OK)                                                                                       // Vérifie le statut
    {
        ESP01_LOG_ERROR("HTTP", "AT+CIPSEND échoué pour la connexion %d", conn_id); // Log l'échec
        return st;                                                                  // Retourne l'erreur
    }

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)response, total_len, HAL_MAX_DELAY); // Envoie la réponse HTTP sur l'UART

    st = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_LONG);                                                                   // Attend la confirmation d'envoi
    ESP01_LOG_DEBUG("HTTP", "Réponse HTTP envoyée sur connexion %d, taille de la page HTML : %d octets", conn_id, (int)body_len); // Log la réussite

    uint32_t elapsed = HAL_GetTick() - start;                                                                              // Calcule le temps de réponse
    g_stats.total_response_time_ms += elapsed;                                                                             // Ajoute au temps total
    g_stats.avg_response_time_ms = g_stats.response_count ? (g_stats.total_response_time_ms / g_stats.response_count) : 0; // Met à jour la moyenne

    return st; // Retourne le statut
}

// ==================== GESTION DES CONNEXIONS ====================

/**
 * @brief Retourne le nombre de connexions TCP actives.
 * @retval Nombre de connexions actives.
 */
int esp01_get_active_connection_count(void)
{
    int count = 0;                                                       // Initialise le compteur
    for (int i = 0; i < ESP01_MAX_CONNECTIONS; ++i)                      // Parcours toutes les connexions
        if (g_connections[i].is_active)                                  // Si la connexion est active
            count++;                                                     // Incrémente le compteur
    ESP01_LOG_DEBUG("HTTP", "Nombre de connexions actives : %d", count); // Log le résultat
    return count;                                                        // Retourne le nombre de connexions actives
}

/**
 * @brief Vérifie si une connexion TCP est active.
 * @param conn_id  Identifiant de connexion.
 * @retval true si active, false sinon.
 */
bool esp01_is_connection_active(int conn_id)
{
    bool active = (conn_id >= 0 && conn_id < ESP01_MAX_CONNECTIONS && g_connections[conn_id].is_active); // Vérifie les bornes et l'état
    ESP01_LOG_DEBUG("HTTP", "Connexion %d active ? %s", conn_id, active ? "OUI" : "NON");                // Log le résultat
    return active;                                                                                       // Retourne l'état
}

/**
 * @brief Nettoie les connexions inactives (timeout).
 */
void esp01_cleanup_inactive_connections(void)
{
    uint32_t now = HAL_GetTick();                   // Récupère le temps courant
    for (int i = 0; i < ESP01_MAX_CONNECTIONS; ++i) // Parcours toutes les connexions
    {
        if (g_connections[i].is_active &&
            (now - g_connections[i].last_activity > ESP01_CONN_TIMEOUT_MS)) // Si la connexion est inactive depuis trop longtemps
        {
            ESP01_LOG_DEBUG("HTTP", "Connexion %d inactive depuis %lu ms, fermeture...", i, (unsigned long)(now - g_connections[i].last_activity)); // Log la fermeture
            esp01_http_close_connection(i);                                                                                                         // Ferme la connexion
            memset(&g_connections[i], 0, sizeof(connection_info_t));                                                                                // Réinitialise la structure
        }
        else if (!g_connections[i].is_active) // Si la connexion n'est pas active
        {
            memset(&g_connections[i], 0, sizeof(connection_info_t)); // Réinitialise la structure
        }
    }
}

/**
 * @brief Ferme une connexion TCP HTTP.
 * @param conn_id  Identifiant de connexion.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_http_close_connection(int conn_id)
{
    ESP01_LOG_DEBUG("HTTP", "Fermeture de la connexion %d", conn_id);                                   // Log la fermeture
    VALIDATE_PARAM(conn_id >= 0 && conn_id < ESP01_MAX_CONNECTIONS, ESP01_INVALID_PARAM);               // Vérifie l'identifiant
    char cmd[ESP01_MAX_CMD_BUF];                                                                        // Buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d", conn_id);                                              // Prépare la commande AT+CIPCLOSE
    char resp[ESP01_MAX_RESP_BUF];                                                                      // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT
    if (st != ESP01_OK)
    {
        ESP01_LOG_WARN("HTTP_CLOSE", "Fermeture connexion %d : échec ou timeout (code=%d)", conn_id, st);
        return st;
    }
    g_connections[conn_id].is_active = 0;                    // Marque la connexion comme inactive
    ESP01_LOG_DEBUG("HTTP", "Connexion %d fermée", conn_id); // Log la réussite
    return ESP01_OK;                                         // Retourne OK
}

/**
 * @brief Retourne l'adresse IP du client pour une connexion.
 * @param conn_id  Identifiant de connexion.
 * @retval Pointeur vers la chaîne IP, ou "N/A".
 */
const char *esp01_http_get_client_ip(int conn_id)
{
    if (conn_id < 0 || conn_id >= ESP01_MAX_CONNECTIONS)                                                    // Vérifie les bornes
        return "N/A";                                                                                       // Retourne "N/A" si hors bornes
    if (!g_connections[conn_id].is_active || !g_connections[conn_id].client_ip[0])                          // Vérifie l'état et la présence d'une IP
        return "N/A";                                                                                       // Retourne "N/A" si pas d'IP
    ESP01_LOG_DEBUG("HTTP", "IP client pour connexion %d : %s", conn_id, g_connections[conn_id].client_ip); // Log l'IP
    return g_connections[conn_id].client_ip;                                                                // Retourne l'IP
}

/**
 * @brief Affiche l'état des connexions actives (log debug).
 */
void esp01_print_connection_status(void)
{
    for (int i = 0; i < ESP01_MAX_CONNECTIONS; ++i) // Parcours toutes les connexions
    {
        if (g_connections[i].is_active)                                                             // Si la connexion est active
            ESP01_LOG_DEBUG("HTTP", "Connexion %d active, IP : %s", i, g_connections[i].client_ip); // Log l'état de la connexion
    }
}

// ==================== TRAITEMENT AUTOMATIQUE DES REQUÊTES ====================

/**
 * @brief Traite automatiquement les requêtes HTTP reçues (DMA accumulateur).
 */
void esp01_process_requests(void)
{
    if (g_processing_request) // Si un traitement est déjà en cours
        return;               // Sort sans rien faire

    g_processing_request = 1; // Marque le début du traitement

    // --- Lecture UART automatique (DMA accumulateur) ---
    uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];                     // Buffer temporaire pour la lecture UART
    uint16_t len = esp01_get_new_data(buffer, sizeof(buffer)); // Récupère les nouveaux octets reçus
    if (len > 0)                                               // Si des données ont été reçues
    {
        if ((g_acc_len + len) < (int)sizeof(g_accumulator) - 1) // Vérifie qu'il reste de la place dans l'accumulateur
        {
            memcpy(g_accumulator + g_acc_len, buffer, len);                                               // Ajoute les données à l'accumulateur
            g_acc_len += len;                                                                             // Met à jour la longueur totale
            g_accumulator[g_acc_len] = '\0';                                                              // Termine la chaîne
            ESP01_LOG_DEBUG("HTTP", "Ajout de %d octets dans l'accumulateur (total=%d)", len, g_acc_len); // Log l'ajout
        }
        else
        {
            ESP01_LOG_ERROR("HTTP", "Dépassement du buffer accumulateur HTTP (g_acc_len=%d, len=%d)", g_acc_len, len); // Log le dépassement
            g_acc_len = 0;                                                                                             // Réinitialise la longueur
            g_accumulator[0] = '\0';                                                                                   // Vide le buffer
        }
    }

    // --- Traitement des paquets +IPD ---
    char *ipd_ptr = _find_next_ipd(g_accumulator, g_acc_len); // Recherche le prochain +IPD dans l'accumulateur
    while (ipd_ptr)                                           // Tant qu'un +IPD est trouvé
    {
        http_request_t ipd = parse_ipd_header(ipd_ptr); // Parse l'en-tête +IPD
        if (ipd.is_valid)                               // Si le parsing est valide
        {
            // --- MISE À JOUR DE LA CONNEXION ---
            connection_info_t *conn = &g_connections[ipd.conn_id];                          // Récupère la structure de connexion
            conn->conn_id = ipd.conn_id;                                                    // Met à jour l'identifiant
            conn->is_active = true;                                                         // Marque la connexion comme active
            conn->last_activity = HAL_GetTick();                                            // Met à jour le timestamp d'activité
            if (ipd.has_ip)                                                                 // Si l'IP est présente
                esp01_safe_strcpy(conn->client_ip, sizeof(conn->client_ip), ipd.client_ip); // Copie l'IP
            else
                conn->client_ip[0] = 0;          // Vide la chaîne IP
            conn->client_port = ipd.client_port; // Met à jour le port client
            // --- FIN MISE À JOUR ---

            char *data_start = strchr(ipd_ptr, ':');                                                // Recherche le début des données HTTP
            if (data_start && (g_acc_len - (data_start - g_accumulator) - 1) >= ipd.content_length) // Vérifie que toutes les données sont présentes
            {
                data_start++;                                     // Passe le ':'
                char http_buf[ESP01_MAX_TOTAL_HTTP] = {0};        // Buffer pour la requête HTTP brute
                memcpy(http_buf, data_start, ipd.content_length); // Copie la requête HTTP
                http_buf[ipd.content_length] = 0;                 // Termine la chaîne
                esp01_trim_string(http_buf);                      // Supprime les espaces superflus

                ESP01_LOG_DEBUG("HTTP", "IPD reçu (brut) :\n%s", http_buf); // Log la requête brute

                http_parsed_request_t req; // Structure pour la requête parsée
                if (esp01_parse_http_request(http_buf, &req) == ESP01_OK && req.is_valid)
                {
                    if (strcmp(req.path, "/favicon.ico") == 0)
                    {
                        ESP01_LOG_DEBUG("HTTP", "favicon.ico demandé, réponse 204 No Content");
                        esp01_send_http_response(ipd.conn_id, 204, "image/x-icon", NULL, 0);
                        goto cleanup;
                    }
                    else
                    {
                        ESP01_LOG_DEBUG("HTTP", "Appel du handler pour la route : %s", req.path);
                        esp01_route_handler_t handler = esp01_find_route_handler(req.path);
                        if (handler)
                            handler(ipd.conn_id, &req);
                        else
                            esp01_send_404_response(ipd.conn_id);
                    }
                }
                else
                {
                    ESP01_LOG_DEBUG("HTTP", "Parsing HTTP échoué, envoi d'une 404");
                    esp01_send_404_response(ipd.conn_id);
                }

            cleanup:
                int to_remove = (data_start - g_accumulator) + ipd.content_length;        // Calcule le nombre d'octets à retirer de l'accumulateur
                memmove(g_accumulator, g_accumulator + to_remove, g_acc_len - to_remove); // Décale le buffer pour supprimer la requête traitée
                g_acc_len -= to_remove;                                                   // Met à jour la longueur de l'accumulateur
                ipd_ptr = _find_next_ipd(g_accumulator, g_acc_len);                       // Recherche le prochain +IPD
                continue;                                                                 // Passe à la prochaine itération
            }
        }
        break; // Sort de la boucle si pas de +IPD complet
    }

    g_processing_request = 0; // Marque la fin du traitement
}

/**
 * @brief Boucle principale HTTP à appeler périodiquement (traitement + nettoyage).
 */
void esp01_http_loop(void)
{
    esp01_process_requests();             // Traite les requêtes HTTP reçues
    esp01_cleanup_inactive_connections(); // Nettoie les connexions inactives
}

// ==================== OUTILS GENERAUX ====================

/**
 * @brief Vide le payload HTTP restant dans l'uart (lecture forcée).
 * @param expected_length  Nombre d'octets à lire/ignorer.
 */
void discard_http_payload(int expected_length)
{
    ESP01_LOG_DEBUG("HTTP", "discard_http_payload: début vidage payload HTTP (%d octets)", expected_length); // Log le début du vidage
    char discard_buf[ESP01_SMALL_BUF_SIZE];                                                                  // Buffer temporaire pour la lecture
    int remaining = expected_length;                                                                         // Nombre d'octets restant à lire
    uint32_t timeout_start = HAL_GetTick();                                                                  // Timestamp de début
    const uint32_t timeout_ms = 200;                                                                         // Timeout maximal

    while (remaining > 0 && (HAL_GetTick() - timeout_start) < timeout_ms) // Boucle jusqu'à avoir tout lu ou timeout
    {
        int to_read = (remaining > sizeof(discard_buf)) ? sizeof(discard_buf) : remaining; // Calcule le nombre d'octets à lire
        int read = esp01_get_new_data((uint8_t *)discard_buf, to_read);                    // Lit les nouveaux octets reçus
        if (read > 0)                                                                      // Si des octets ont été lus
        {
            remaining -= read;             // Décrémente le nombre restant
            timeout_start = HAL_GetTick(); // Réinitialise le timeout
        }
        else
        {
            HAL_Delay(2); // Petite pause si rien reçu
        }
    }

    if (remaining > 0) // Si tous les octets n'ont pas été lus
    {
        ESP01_LOG_WARN("HTTP", "discard_http_payload: %d octets non lus (discard incomplet)", remaining); // Log l'avertissement
    }
}

/**
 * @brief  Récupère le statut du serveur HTTP
 * @param  is_server Pointeur vers variable indiquant si le serveur est en cours d'exécution
 * @param  port      Pointeur vers variable stockant le port du serveur
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 * @note   Cette fonction vérifie l'état du serveur via AT+CIPSTATUS et retourne le port configuré
 * @details
 *         La fonction envoie AT+CIPSTATUS pour vérifier l'état des connexions.
 *         Si "STATUS:" est trouvé dans la réponse, le serveur est considéré comme actif.
 *         Le port est récupéré depuis la variable globale g_server_port.
 */
ESP01_Status_t esp01_http_get_server_status(uint8_t *is_server, uint16_t *port)
{
    VALIDATE_PARAM(is_server && port, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    // Vérifier l'état de la connexion
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTATUS", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_RETURN_ERROR("HTTP_STATUS", st);
    }

    // Le serveur est considéré actif si nous avons un statut de connexion
    // Cette logique peut être adaptée selon les besoins exacts
    char *status_str = strstr(resp, "STATUS:");
    *is_server = (status_str != NULL) ? 1 : 0;

    // Récupérer le port configuré (variable globale)
    *port = g_server_port;

    ESP01_LOG_DEBUG("HTTP", "Statut serveur: %s (port %u)",
                    *is_server ? "Actif" : "Inactif", *port);

    return ESP01_OK;
} // Fin de esp01_http_get_server_status

// ========================= FIN DU MODULE =========================
