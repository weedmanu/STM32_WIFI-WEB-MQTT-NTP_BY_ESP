/**
 ******************************************************************************
 * @file    STM32_WifiESP_HTTP.c
 * @author  manu
 * @version 1.2.0
 * @date    17 juin 2025
 * @brief   Fonctions HTTP haut niveau pour ESP01 (serveur, requêtes, parsing, routes)
 ******************************************************************************
 */

// ==================== INCLUDES ====================

#include "STM32_WifiESP.h"
#include "STM32_WifiESP_WIFI.h"
#include "STM32_WifiESP_HTTP.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// ==================== DEFINES ====================
#define ESP01_CONN_TIMEOUT_MS 30000 // 30 secondes

// ==================== VARIABLES GLOBALES ====================
connection_info_t g_connections[ESP01_MAX_CONNECTIONS] = {0};
int g_connection_count = ESP01_MAX_CONNECTIONS;
volatile int g_acc_len = 0;
char g_accumulator[ESP01_MAX_TOTAL_HTTP] = {0};
volatile int g_processing_request = 0;
esp01_route_t g_routes[ESP01_MAX_ROUTES] = {0};
int g_route_count = 0;
esp01_stats_t g_stats = {0};

// ==================== OUTILS FACTORISÉS ====================

char *_find_next_ipd(char *buffer, int buffer_len)
{
    for (int i = 0; i < buffer_len - 4; ++i)
        if (memcmp(&buffer[i], "+IPD,", 5) == 0)
            return &buffer[i];
    return NULL;
}

http_request_t parse_ipd_header(const char *data)
{
    http_request_t req = {0};
    if (!data || strncmp(data, "+IPD,", 5) != 0)
        return req;

    int conn_id = -1, content_length = 0, client_port = 0;
    char client_ip[ESP01_MAX_IP_LEN] = {0};

    int n = sscanf(data, "+IPD,%d,%d,\"%[^\"]\",%d:", &conn_id, &content_length, client_ip, &client_port);
    if (n == 4)
    {
        req.conn_id = conn_id;
        req.content_length = content_length;
        req.has_ip = true;
        strncpy(req.client_ip, client_ip, sizeof(req.client_ip) - 1);
        req.client_ip[sizeof(req.client_ip) - 1] = '\0';
        req.client_port = client_port;
        req.is_valid = true;
    }
    else if (sscanf(data, "+IPD,%d,%d:", &conn_id, &content_length) == 2)
    {
        req.conn_id = conn_id;
        req.content_length = content_length;
        req.has_ip = false;
        req.client_ip[0] = '\0';
        req.client_port = 0;
        req.is_valid = true;
    }
    return req;
}

// ==================== ROUTES ====================

void esp01_clear_routes(void)
{
    ESP01_LOG_DEBUG("HTTP", "Effacement de toutes les routes HTTP");
    memset(g_routes, 0, sizeof(g_routes));
    g_route_count = 0;
}

ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler)
{
    ESP01_LOG_DEBUG("HTTP", "Ajout de la route : %s", path);
    VALIDATE_PARAM(path && handler, ESP01_INVALID_PARAM);
    if (g_route_count >= ESP01_MAX_ROUTES)
        ESP01_RETURN_ERROR("ADD_ROUTE", ESP01_FAIL);
    strncpy(g_routes[g_route_count].path, path, ESP01_MAX_HTTP_PATH_LEN - 1);
    g_routes[g_route_count].path[ESP01_MAX_HTTP_PATH_LEN - 1] = 0;
    g_routes[g_route_count].handler = handler;
    g_route_count++;
    ESP01_LOG_DEBUG("HTTP", "Route ajoutée : %s (total=%d)", path, g_route_count);
    return ESP01_OK;
}

esp01_route_handler_t esp01_find_route_handler(const char *path)
{
    ESP01_LOG_DEBUG("HTTP", "Recherche du handler pour la route : %s", path);
    for (int i = 0; i < g_route_count; ++i)
        if (strncmp(path, g_routes[i].path, ESP01_MAX_HTTP_PATH_LEN) == 0)
            return g_routes[i].handler;
    ESP01_LOG_DEBUG("HTTP", "Aucun handler trouvé pour la route : %s", path);
    return NULL;
}

// ==================== INIT & SERVEUR ====================

ESP01_Status_t esp01_http_init(void)
{
    ESP01_LOG_DEBUG("HTTP", "Initialisation du module HTTP");
    memset(g_connections, 0, sizeof(g_connections));
    g_connection_count = ESP01_MAX_CONNECTIONS;
    g_acc_len = 0;
    memset(g_accumulator, 0, sizeof(g_accumulator));
    g_processing_request = 0;
    esp01_clear_routes();
    return ESP01_OK;
}

ESP01_Status_t esp01_http_start_server(uint16_t port)
{
    ESP01_LOG_DEBUG("HTTP", "Démarrage du serveur HTTP sur le port %u", port);
    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("HTTP_SERVER", st);
    ESP01_LOG_INFO("HTTP", "Serveur HTTP démarré sur le port %u", port);
    return ESP01_OK;
}

ESP01_Status_t esp01_http_stop_server(void)
{
    ESP01_LOG_DEBUG("HTTP", "Arrêt du serveur HTTP");
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSERVER=0", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("HTTP_STOP", st);
    ESP01_LOG_INFO("HTTP", "Serveur HTTP arrêté");
    return ESP01_OK;
}

ESP01_Status_t esp01_start_server_config(bool multi_conn, uint16_t port, bool ipdinfo)
{
    ESP01_LOG_DEBUG("HTTP", "Configuration du serveur : multi_conn=%d, port=%u, ipdinfo=%d", multi_conn, port, ipdinfo);
    ESP01_Status_t st = ESP01_OK;
    char resp[ESP01_MAX_RESP_BUF];

    if (multi_conn)
    {
        st = esp01_send_raw_command_dma("AT+CIPMUX=1", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
        if (st != ESP01_OK)
            ESP01_RETURN_ERROR("CIPMUX", st);
    }

    if (ipdinfo)
    {
        st = esp01_send_raw_command_dma("AT+CIPDINFO=1", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
        if (st != ESP01_OK)
            ESP01_RETURN_ERROR("CIPDINFO", st);
    }

    return esp01_http_start_server(port);
}

// ==================== PARSING REQUÊTES HTTP ====================

ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed)
{
    ESP01_LOG_DEBUG("HTTP", "Parsing de la requête HTTP...");
    VALIDATE_PARAM(raw_request, ESP01_FAIL);
    VALIDATE_PARAM(parsed, ESP01_FAIL);

    memset(parsed, 0, sizeof(http_parsed_request_t));

    const char *p = raw_request;
    const char *method_start = p, *method_end = NULL;
    const char *path_start = NULL, *path_end = NULL;
    const char *query_start = NULL, *query_end = NULL;
    const char *line_end = strstr(p, "\r\n");
    if (!line_end)
        return ESP01_FAIL;

    while (p < line_end && *p != ' ')
        p++;
    method_end = p;
    if (method_end - method_start >= ESP01_MAX_HTTP_METHOD_LEN)
        return ESP01_FAIL;
    memcpy(parsed->method, method_start, method_end - method_start);
    parsed->method[method_end - method_start] = '\0';

    p++;
    path_start = p;
    while (p < line_end && *p != ' ' && *p != '?')
        p++;
    path_end = p;
    if (path_end - path_start >= ESP01_MAX_HTTP_PATH_LEN)
        return ESP01_FAIL;
    memcpy(parsed->path, path_start, path_end - path_start);
    parsed->path[path_end - path_start] = '\0';

    if (*p == '?')
    {
        p++;
        query_start = p;
        while (p < line_end && *p != ' ')
            p++;
        query_end = p;
        size_t qlen = query_end - query_start;
        if (qlen >= ESP01_MAX_HTTP_QUERY_LEN)
            qlen = ESP01_MAX_HTTP_QUERY_LEN - 1;
        memcpy(parsed->query_string, query_start, qlen);
        parsed->query_string[qlen] = '\0';
    }
    else
    {
        parsed->query_string[0] = '\0';
    }

    parsed->is_valid = true;
    ESP01_LOG_DEBUG("HTTP", "Méthode=%s, Path=%s, Query=%s", parsed->method, parsed->path, parsed->query_string);
    return ESP01_OK;
}

// ==================== ENVOI DE RÉPONSES ====================

ESP01_Status_t esp01_send_json_response(int conn_id, const char *json)
{
    ESP01_LOG_DEBUG("HTTP", "Envoi d'une réponse JSON sur connexion %d", conn_id);
    return esp01_send_http_response(conn_id, ESP01_HTTP_OK_CODE, "application/json", json, strlen(json));
}

ESP01_Status_t esp01_send_404_response(int conn_id)
{
    ESP01_LOG_DEBUG("HTTP", "404 Not Found envoyé sur connexion %d", conn_id);
    const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
    return esp01_send_http_response(conn_id, ESP01_HTTP_NOT_FOUND_CODE, "text/html", body, strlen(body));
}

ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len)
{
    ESP01_LOG_DEBUG("HTTP", "Préparation de la réponse HTTP (conn_id=%d, code=%d, type=%s, taille=%d)", conn_id, status_code, content_type ? content_type : "NULL", (int)body_len);
    VALIDATE_PARAM(conn_id >= 0, ESP01_FAIL);
    VALIDATE_PARAM(status_code >= 100 && status_code < 600, ESP01_FAIL);
    VALIDATE_PARAM(body || body_len == 0, ESP01_FAIL);

    uint32_t start = HAL_GetTick();
    g_stats.total_requests++;
    g_stats.response_count++;
    if (status_code >= ESP01_HTTP_OK_CODE && status_code < 300)
        g_stats.successful_responses++;
    else if (status_code >= 400)
        g_stats.failed_responses++;

    char header[ESP01_MAX_HEADER_LINE];
    const char *status_text = "OK";
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
    case 204:
        status_text = "No Content";
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

    char response[ESP01_MAX_TOTAL_HTTP];
    if ((header_len + body_len) >= sizeof(response))
    {
        ESP01_LOG_ERROR("HTTP", "Réponse HTTP trop grande (header=%d, body=%d, max=%d)", header_len, (int)body_len, (int)sizeof(response));
        return ESP01_FAIL;
    }

    memcpy(response, header, header_len);
    if (body && body_len > 0)
        memcpy(response + header_len, body, body_len);

    int total_len = header_len + (int)body_len;

    char cipsend_cmd[ESP01_MAX_CIPSEND_BUF];
    snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, total_len);

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma(cipsend_cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_LONG);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("HTTP", "AT+CIPSEND échoué pour la connexion %d", conn_id);
        return st;
    }

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)response, total_len, HAL_MAX_DELAY);

    st = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_LONG);
    ESP01_LOG_DEBUG("HTTP", "Réponse HTTP envoyée sur connexion %d, taille de la page HTML : %d octets", conn_id, (int)body_len);

    uint32_t elapsed = HAL_GetTick() - start;
    g_stats.total_response_time_ms += elapsed;
    g_stats.avg_response_time_ms = g_stats.response_count ? (g_stats.total_response_time_ms / g_stats.response_count) : 0;

    return st;
}

// ==================== GESTION DES CONNEXIONS ====================

int esp01_get_active_connection_count(void)
{
    int count = 0;
    for (int i = 0; i < ESP01_MAX_CONNECTIONS; ++i)
        if (g_connections[i].is_active)
            count++;
    ESP01_LOG_DEBUG("HTTP", "Nombre de connexions actives : %d", count);
    return count;
}

bool esp01_is_connection_active(int conn_id)
{
    bool active = (conn_id >= 0 && conn_id < ESP01_MAX_CONNECTIONS && g_connections[conn_id].is_active);
    ESP01_LOG_DEBUG("HTTP", "Connexion %d active ? %s", conn_id, active ? "OUI" : "NON");
    return active;
}

void esp01_cleanup_inactive_connections(void)
{
    uint32_t now = HAL_GetTick();
    for (int i = 0; i < ESP01_MAX_CONNECTIONS; ++i)
    {
        if (g_connections[i].is_active &&
            (now - g_connections[i].last_activity > ESP01_CONN_TIMEOUT_MS))
        {
            ESP01_LOG_DEBUG("HTTP", "Connexion %d inactive depuis %lu ms, fermeture...", i, (unsigned long)(now - g_connections[i].last_activity));
            esp01_http_close_connection(i);
            memset(&g_connections[i], 0, sizeof(connection_info_t));
        }
        else if (!g_connections[i].is_active)
        {
            memset(&g_connections[i], 0, sizeof(connection_info_t));
        }
    }
}

ESP01_Status_t esp01_http_close_connection(int conn_id)
{
    ESP01_LOG_DEBUG("HTTP", "Fermeture de la connexion %d", conn_id);
    VALIDATE_PARAM(conn_id >= 0 && conn_id < ESP01_MAX_CONNECTIONS, ESP01_INVALID_PARAM);
    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d", conn_id);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("HTTP_CLOSE", st);
    g_connections[conn_id].is_active = 0;
    ESP01_LOG_INFO("HTTP", "Connexion %d fermée", conn_id);
    return ESP01_OK;
}

const char *esp01_http_get_client_ip(int conn_id)
{
    if (conn_id < 0 || conn_id >= ESP01_MAX_CONNECTIONS)
        return "N/A";
    if (!g_connections[conn_id].is_active || !g_connections[conn_id].client_ip[0])
        return "N/A";
    ESP01_LOG_DEBUG("HTTP", "IP client pour connexion %d : %s", conn_id, g_connections[conn_id].client_ip);
    return g_connections[conn_id].client_ip;
}

void esp01_print_connection_status(void)
{
    for (int i = 0; i < ESP01_MAX_CONNECTIONS; ++i)
    {
        if (g_connections[i].is_active)
            ESP01_LOG_INFO("HTTP", "Connexion %d active, IP : %s", i, g_connections[i].client_ip);
    }
}

// ==================== TRAITEMENT AUTOMATIQUE DES REQUÊTES ====================

void esp01_process_requests(void)
{
    if (g_processing_request)
        return;

    g_processing_request = 1;

    // --- Lecture UART automatique (DMA accumulateur) ---
    uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];
    uint16_t len = esp01_get_new_data(buffer, sizeof(buffer));
    if (len > 0)
    {
        if ((g_acc_len + len) < (int)sizeof(g_accumulator) - 1)
        {
            memcpy(g_accumulator + g_acc_len, buffer, len);
            g_acc_len += len;
            g_accumulator[g_acc_len] = '\0';
            ESP01_LOG_DEBUG("HTTP", "Ajout de %d octets dans l'accumulateur (total=%d)", len, g_acc_len);
        }
        else
        {
            ESP01_LOG_ERROR("HTTP", "Dépassement du buffer accumulateur HTTP (g_acc_len=%d, len=%d)", g_acc_len, len);
            g_acc_len = 0;
            g_accumulator[0] = '\0';
        }
    }

    // --- Traitement des paquets +IPD ---
    char *ipd_ptr = _find_next_ipd(g_accumulator, g_acc_len);
    while (ipd_ptr)
    {
        http_request_t ipd = parse_ipd_header(ipd_ptr);
        if (ipd.is_valid)
        {
            // --- MISE À JOUR DE LA CONNEXION ---
            connection_info_t *conn = &g_connections[ipd.conn_id];
            conn->conn_id = ipd.conn_id;
            conn->is_active = true;
            conn->last_activity = HAL_GetTick();
            if (ipd.has_ip)
                strncpy(conn->client_ip, ipd.client_ip, sizeof(conn->client_ip) - 1);
            else
                conn->client_ip[0] = 0;
            conn->client_port = ipd.client_port;
            // --- FIN MISE À JOUR ---

            char *data_start = strchr(ipd_ptr, ':');
            if (data_start && (g_acc_len - (data_start - g_accumulator) - 1) >= ipd.content_length)
            {
                data_start++;
                char http_buf[ESP01_MAX_TOTAL_HTTP] = {0};
                memcpy(http_buf, data_start, ipd.content_length);
                http_buf[ipd.content_length] = 0;

                ESP01_LOG_DEBUG("HTTP", "IPD reçu (brut) :\n%s", http_buf);

                http_parsed_request_t req;
                if (esp01_parse_http_request(http_buf, &req) == ESP01_OK && req.is_valid)
                {
                    if (strcmp(req.path, "/favicon.ico") == 0)
                    {
                        ESP01_LOG_DEBUG("HTTP", "favicon.ico demandé, réponse 204 No Content");
                        esp01_send_http_response(ipd.conn_id, 204, "image/x-icon", NULL, 0);
                        goto cleanup;
                    }

                    ESP01_LOG_DEBUG("HTTP", "Appel du handler pour la route : %s", req.path);
                    esp01_route_handler_t handler = esp01_find_route_handler(req.path);
                    if (handler)
                        handler(ipd.conn_id, &req);
                    else
                        esp01_send_404_response(ipd.conn_id);
                }
                else
                {
                    ESP01_LOG_DEBUG("HTTP", "Parsing HTTP échoué, envoi d'une 404");
                    esp01_send_404_response(ipd.conn_id);
                }

            cleanup:
                int to_remove = (data_start - g_accumulator) + ipd.content_length;
                memmove(g_accumulator, g_accumulator + to_remove, g_acc_len - to_remove);
                g_acc_len -= to_remove;
                ipd_ptr = _find_next_ipd(g_accumulator, g_acc_len);
                continue;
            }
        }
        break;
    }

    g_processing_request = 0;
}

void esp01_http_loop(void)
{
    esp01_process_requests();
    esp01_cleanup_inactive_connections();
}

// ==================== OUTILS GENERAUX ====================

void discard_http_payload(int expected_length)
{
    ESP01_LOG_DEBUG("HTTP", "discard_http_payload: début vidage payload HTTP (%d octets)", expected_length);
    char discard_buf[ESP01_SMALL_BUF_SIZE];
    int remaining = expected_length;
    uint32_t timeout_start = HAL_GetTick();
    const uint32_t timeout_ms = 200;

    while (remaining > 0 && (HAL_GetTick() - timeout_start) < timeout_ms)
    {
        int to_read = (remaining > sizeof(discard_buf)) ? sizeof(discard_buf) : remaining;
        int read = esp01_get_new_data((uint8_t *)discard_buf, to_read);
        if (read > 0)
        {
            remaining -= read;
            timeout_start = HAL_GetTick();
        }
        else
        {
            HAL_Delay(2);
        }
    }

    if (remaining > 0)
    {
        ESP01_LOG_WARN("HTTP", "discard_http_payload: %d octets non lus (discard incomplet)", remaining);
    }
}
