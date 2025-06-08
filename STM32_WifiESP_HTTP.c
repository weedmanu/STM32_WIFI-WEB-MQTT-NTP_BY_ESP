/*
 * STM32_WifiESP_HTTP.c
 * Fonctions HTTP pour le driver STM32 <-> ESP01 (AT)
 * Version Final V1.0
 * 2025 - manu
 */

#include "STM32_WifiESP.h"
#include "STM32_WifiESP_HTTP.h"
#include "STM32_WifiESP_Utils.h"

#define MAX_ROUTES ESP01_MAX_ROUTES

extern connection_info_t g_connections[];
extern int g_connection_count;

// ==================== PAYLOAD/DISCARD ====================
void discard_http_payload(int expected_length)
{
    char discard_buf[ESP01_SMALL_BUF_SIZE];
    int remaining = expected_length;
    uint32_t timeout_start = HAL_GetTick();
    const uint32_t timeout_ms = 200;

    _esp_logln("[HTTP] discard_http_payload: début vidage payload HTTP");

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
        char warn[256];
        snprintf(warn, sizeof(warn), "[HTTP] AVERTISSEMENT: %d octets non lus (discard incomplet)", remaining);
        _esp_logln(warn);
    }
}

// ==================== PARSING REQUETE HTTP ====================
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed)
{
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
    return ESP01_OK;
}

// ==================== REPONSE HTTP GENERIQUE ====================
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len)
{
    VALIDATE_PARAM(conn_id >= 0, ESP01_FAIL);
    VALIDATE_PARAM(status_code >= 100 && status_code < 600, ESP01_FAIL);
    VALIDATE_PARAM(body || body_len == 0, ESP01_FAIL);

    uint32_t start = HAL_GetTick();
    g_stats.total_requests++;
    g_stats.response_count++;
    if (status_code >= ESP01_HTTP_OK_CODE && status_code < 300)
    {
        g_stats.successful_responses++;
    }
    else if (status_code >= 400)
    {
        g_stats.failed_responses++;
    }

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
        _esp_logln("[HTTP] esp01_send_http_response: réponse trop grande");
        return ESP01_FAIL;
    }

    memcpy(response, header, header_len);
    if (body && body_len > 0)
        memcpy(response + header_len, body, body_len);

    int total_len = header_len + (int)body_len;

    char cipsend_cmd[ESP01_MAX_CIPSEND_BUF];
    snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, total_len);

    char resp[ESP01_CMD_RESP_BUF_SIZE];
    ESP01_Status_t st = esp01_send_raw_command_dma(cipsend_cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_LONG);
    if (st != ESP01_OK)
    {
        _esp_logln("[HTTP] esp01_send_http_response: AT+CIPSEND échoué");
        return st;
    }

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)response, total_len, HAL_MAX_DELAY);

    st = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_LONG);
    char dbg[ESP01_SMALL_BUF_SIZE];
    snprintf(dbg, sizeof(dbg), "[HTTP] Envoi réponse HTTP sur connexion %d, taille de la page HTML : %d octets", conn_id, (int)body_len);
    _esp_logln(dbg);

    uint32_t elapsed = HAL_GetTick() - start;
    g_stats.total_response_time_ms += elapsed;
    g_stats.avg_response_time_ms = g_stats.response_count ? (g_stats.total_response_time_ms / g_stats.response_count) : 0;

    return st;
}

// ==================== REPONSES SPECIFIQUES ====================
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data)
{
    _esp_logln("[HTTP] Envoi d'une réponse JSON");
    return esp01_send_http_response(conn_id, 200, "application/json", json_data, strlen(json_data));
}

ESP01_Status_t esp01_send_404_response(int conn_id)
{
    _esp_logln("[HTTP] Envoi d'une réponse 404");
    const char *body = ESP01_HTTP_404_BODY;
    return esp01_send_http_response(conn_id, 404, "text/html", body, strlen(body));
}

// ==================== HTTP GET CLIENT ====================
ESP01_Status_t esp01_http_get(const char *host, uint16_t port, const char *path, char *response, size_t response_size)
{
    char dbg[128];
    snprintf(dbg, sizeof(dbg), "esp01_http_get: GET http://%s:%u%s", host, port, path);
    _esp_logln(dbg);

    char cmd[ESP01_DMA_RX_BUF_SIZE];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", host, port);
    char resp[ESP01_DMA_RX_BUF_SIZE];
    if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000) != ESP01_OK)
    {
        _esp_logln("[HTTP] esp01_http_get: AT+CIPSTART échoué");
        return ESP01_FAIL;
    }

    char http_req[ESP01_MAX_HTTP_REQ_BUF];
    snprintf(http_req, sizeof(http_req),
             "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", (int)strlen(http_req));
    if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", 3000) != ESP01_OK)
        return ESP01_FAIL;

    if (esp01_send_raw_command_dma(http_req, resp, sizeof(resp), "CLOSED", 8000) != ESP01_OK)
        return ESP01_FAIL;

    if (response && response_size > 0)
    {
        strncpy(response, resp, response_size - 1);
        response[response_size - 1] = '\0';
    }

    return ESP01_OK;
}

// ==================== ROUTES HTTP ====================
void esp01_clear_routes(void)
{
    _esp_logln("[ROUTE] Effacement de toutes les routes HTTP");
    g_route_count = 0;
}

ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler)
{
    VALIDATE_PARAM(path, ESP01_FAIL);
    VALIDATE_PARAM(handler, ESP01_FAIL);
    VALIDATE_PARAM(g_route_count < ESP01_MAX_ROUTES, ESP01_FAIL);

    strncpy(g_routes[g_route_count].path, path, sizeof(g_routes[g_route_count].path) - 1);
    g_routes[g_route_count].path[sizeof(g_routes[g_route_count].path) - 1] = '\0';
    g_routes[g_route_count].handler = handler;
    g_route_count++;
    char dbg[ESP01_MAX_DBG_BUF];
    snprintf(dbg, sizeof(dbg), "[WEB] Route ajoutée : %s (total %d)", path, g_route_count);
    _esp_logln(dbg);
    return ESP01_OK;
}

esp01_route_handler_t esp01_find_route_handler(const char *path)
{
    for (int i = 0; i < g_route_count; i++)
    {
        if (strcmp(g_routes[i].path, path) == 0)
        {
            char dbg[80];
            snprintf(dbg, sizeof(dbg), "[WEB] Route trouvée pour path : %s", path);
            _esp_logln(dbg);
            return g_routes[i].handler;
        }
    }
    char dbg[80];
    snprintf(dbg, sizeof(dbg), "[WEB] Aucune route trouvée pour path : %s", path);
    _esp_logln(dbg);
    return NULL;
}

// ==================== TRAITEMENT DES REQUETES ====================
void esp01_process_requests(void)
{
    if (g_processing_request)
        return;
    g_processing_request = true;

    uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];
    uint16_t len = esp01_get_new_data(buffer, sizeof(buffer));
    if (len == 0)
    {
        g_processing_request = false;
        return;
    }

    if (g_acc_len + len < sizeof(g_accumulator) - 1)
    {
        memcpy(g_accumulator + g_acc_len, buffer, len);
        g_acc_len += len;
        g_accumulator[g_acc_len] = '\0';
    }
    else
    {
        g_acc_len = 0;
        g_accumulator[0] = '\0';
        g_processing_request = false;
        return;
    }

    char *ipd_start = _find_next_ipd(g_accumulator, g_acc_len);
    if (!ipd_start)
    {
        g_processing_request = false;
        return;
    }

    http_request_t req = parse_ipd_header(ipd_start);
    if (!req.is_valid)
    {
        g_processing_request = false;
        return;
    }

    char *payload_start = strchr(ipd_start, ':');
    if (!payload_start)
    {
        g_processing_request = false;
        return;
    }
    payload_start++;

    int payload_length = req.content_length - (payload_start - ipd_start);
    if (payload_length <= 0)
    {
        g_processing_request = false;
        return;
    }

    if (payload_length > (g_acc_len - (payload_start - g_accumulator)))
    {
        g_processing_request = false;
        return;
    }

    if (req.is_valid)
    {
        int payload_offset = payload_start - g_accumulator;
        if (payload_offset < 0 || payload_offset > g_acc_len)
        {
            _esp_logln("[ERROR] payload_start hors accumulateur !");
            g_processing_request = false;
            return;
        }
        int payload_in_buffer = g_acc_len - payload_offset;
        if (payload_in_buffer < 0)
            payload_in_buffer = 0;
        int to_discard = req.content_length - payload_in_buffer;
        if (to_discard < 0)
            to_discard = 0;
        char dbg[128];
        snprintf(dbg, sizeof(dbg), "[DEBUG] payload_offset=%d, payload_in_buffer=%d, to_discard=%d, content_length=%d", payload_offset, payload_in_buffer, to_discard, req.content_length);
        _esp_logln(dbg);

        if (to_discard > 0)
        {
            discard_http_payload(to_discard);
            _flush_rx_buffer(20);
        }
    }

    char route_dbg[ESP01_MAX_DBG_BUF];
    http_parsed_request_t parsed_request = {0};
    esp01_parse_http_request(payload_start, &parsed_request);
    snprintf(route_dbg, sizeof(route_dbg), "[ROUTE] Traitement de la requête pour %s", parsed_request.path);
    _esp_logln(route_dbg);

    esp01_route_handler_t handler = esp01_find_route_handler(parsed_request.path);
    if (handler)
    {
        handler(req.conn_id, &parsed_request);
    }
    else
    {
        esp01_send_404_response(req.conn_id);
    }

    g_acc_len = 0;
    g_accumulator[0] = '\0';
    g_processing_request = false;
}

// ==================== INIT/LOOP HTTP ====================
void esp01_http_init(void)
{
    _esp_logln("[HTTP] Initialisation du module HTTP");
    esp01_clear_routes();
    g_acc_len = 0;
    g_accumulator[0] = '\0';
}

void esp01_http_loop(void)
{
    esp01_process_requests();
}