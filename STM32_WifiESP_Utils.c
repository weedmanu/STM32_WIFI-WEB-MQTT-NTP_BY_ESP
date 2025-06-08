/*
 * STM32_WifiESP_Utils.c
 * Fonctions utilitaires pour le driver STM32 <-> ESP01 (AT)
 * Version Final V1.0
 * 2025 - manu
 */

#include "STM32_WifiESP.h"
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_HTTP.h"
#include <string.h>
#include <stdio.h>

// ==================== VARIABLES EXTERNES ====================
extern uint8_t *g_dma_rx_buf;
extern uint16_t g_dma_buf_size;
extern volatile uint16_t g_rx_last_pos;
extern int g_route_count;

// ==================== LOG & DEBUG ====================
void _esp_logln(const char *msg)
{
    if (ESP01_DEBUG && g_debug_uart && msg)
    {
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
    }
}

void _esp_log(const char *msg)
{
    if (ESP01_DEBUG && g_debug_uart && msg)
    {
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
    }
}

// ==================== BUFFER & DMA ====================
static uint16_t _get_dma_write_pos(void)
{
    return g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx);
}

static uint16_t _get_available_bytes(void)
{
    uint16_t write_pos = _get_dma_write_pos();
    if (write_pos >= g_rx_last_pos)
        return write_pos - g_rx_last_pos;
    else
        return (g_dma_buf_size - g_rx_last_pos) + write_pos;
}

uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size)
{
    if (!g_esp_uart || !g_dma_rx_buf || !buffer || buffer_size == 0)
    {
        if (buffer && buffer_size > 0)
            buffer[0] = '\0';
        return 0;
    }

    uint16_t available = _get_available_bytes();
    if (available == 0)
    {
        buffer[0] = '\0';
        return 0;
    }

    uint16_t to_copy = (available < buffer_size - 1) ? available : buffer_size - 1;
    uint16_t copied = 0;
    uint16_t write_pos = _get_dma_write_pos();

    if (write_pos > g_rx_last_pos)
    {
        memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], to_copy);
        copied = to_copy;
    }
    else
    {
        uint16_t first_chunk = g_dma_buf_size - g_rx_last_pos;
        if (first_chunk >= to_copy)
        {
            memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], to_copy);
            copied = to_copy;
        }
        else
        {
            memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], first_chunk);
            copied = first_chunk;
            uint16_t remaining = to_copy - first_chunk;
            if (remaining > 0)
            {
                memcpy(buffer + first_chunk, &g_dma_rx_buf[0], remaining);
                copied += remaining;
            }
        }
    }

    buffer[copied] = '\0';
    g_rx_last_pos = (g_rx_last_pos + copied) % g_dma_buf_size;

    if (copied > 0)
    {
        char dbg[ESP01_MAX_DBG_BUF];
        snprintf(dbg, sizeof(dbg), "[GET NEW DATA]  %u octets reçus", copied);
        _esp_logln(dbg);
    }

    return copied;
}

void _flush_rx_buffer(uint32_t timeout_ms)
{
    char temp_buf[ESP01_TMP_BUF_SIZE];
    uint32_t start_time = HAL_GetTick();

    while ((HAL_GetTick() - start_time) < timeout_ms)
    {
        uint16_t len = esp01_get_new_data((uint8_t *)temp_buf, sizeof(temp_buf));
        if (len == 0)
        {
            HAL_Delay(10);
        }
        else
        {
            if (ESP01_DEBUG)
            {
                temp_buf[len] = '\0';
                _esp_logln("Buffer vidé:");
                _esp_logln(temp_buf);
            }
        }
    }
}

// ==================== SYNCHRONISATION & PATTERN ====================
bool _accumulate_and_search(char *acc, uint16_t *acc_len, uint16_t acc_max_size, const char *pattern, uint32_t timeout_ms, bool clear_first)
{
    uint32_t start_tick = HAL_GetTick();
    char temp_buf[ESP01_TMP_BUF_SIZE];

    if (clear_first)
    {
        *acc_len = 0;
        acc[0] = '\0';
    }

    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        uint16_t new_data = esp01_get_new_data((uint8_t *)temp_buf, sizeof(temp_buf));
        if (new_data > 0)
        {
            uint16_t space_left = acc_max_size - *acc_len - 1;
            uint16_t to_add = (new_data < space_left) ? new_data : space_left;
            if (to_add > 0)
            {
                memcpy(&acc[*acc_len], temp_buf, to_add);
                *acc_len += to_add;
                acc[*acc_len] = '\0';
            }
            if (strstr(acc, pattern))
                return true;
        }
        else
        {
            HAL_Delay(10);
        }
    }
    return false;
}

ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms)
{
    if (_accumulate_and_search(g_accumulator, &g_acc_len, sizeof(g_accumulator), pattern, timeout_ms, false))
        return ESP01_OK;
    return ESP01_TIMEOUT;
}

// ==================== PARSING IPD ====================
char *_find_next_ipd(char *buffer, int buffer_len)
{
    char *search_pos = buffer;
    char *ipd_pos;

    while ((ipd_pos = strstr(search_pos, "+IPD,")) != NULL)
    {
        char *colon = strchr(ipd_pos, ':');
        if (colon && (colon - buffer) < buffer_len - 1)
        {
            return ipd_pos;
        }
        search_pos = ipd_pos + IPD_HEADER_MIN_LEN;
    }
    return NULL;
}

http_request_t parse_ipd_header(const char *data)
{
    http_request_t request = {0};

    char *ipd_start = strstr(data, "+IPD,");
    if (!ipd_start)
    {
        _esp_logln("parse_ipd_header: +IPD non trouvé");
        return request;
    }

    int parsed = sscanf(ipd_start, "+IPD,%d,%d,\"%15[0-9.]\",%d:",
                        &request.conn_id,
                        &request.content_length,
                        request.client_ip,
                        &request.client_port);
    if (parsed == 4)
    {
        request.is_valid = true;
        request.has_ip = true;
        char dbg[ESP01_MAX_DBG_BUF];
        snprintf(dbg, sizeof(dbg),
                 "[PARSE] parse_ipd_header: IPD avec IP %s:%d, conn_id=%d, len=%d",
                 request.client_ip, request.client_port, request.conn_id, request.content_length);
        _esp_logln(dbg);
    }
    else
    {
        parsed = sscanf(ipd_start, "+IPD,%d,%d:",
                        &request.conn_id,
                        &request.content_length);
        if (parsed == 2)
        {
            request.is_valid = true;
            request.has_ip = false;
            char dbg[ESP01_MAX_DBG_BUF];
            snprintf(dbg, sizeof(dbg),
                     "[PARSE] parse_ipd_header: IPD sans IP, conn_id=%d, len=%d",
                     request.conn_id, request.content_length);
            _esp_logln(dbg);
        }
        else
        {
            _esp_logln("[PARSE] parse_ipd_header: format IPD non reconnu");
        }
    }
    return request;
}

// ==================== ERREURS & CONNEXIONS ====================
const char *esp01_get_error_string(ESP01_Status_t status)
{
    switch (status)
    {
    case ESP01_OK:
        return "OK";
    case ESP01_FAIL:
        return "Echec général";
    case ESP01_TIMEOUT:
        return "Timeout";
    case ESP01_NOT_INITIALIZED:
        return "Non initialisé";
    case ESP01_INVALID_PARAM:
        return "Paramètre invalide";
    case ESP01_BUFFER_OVERFLOW:
        return "Débordement de buffer";
    case ESP01_WIFI_NOT_CONNECTED:
        return "WiFi non connecté";
    case ESP01_HTTP_PARSE_ERROR:
        return "Erreur parsing HTTP";
    case ESP01_ROUTE_NOT_FOUND:
        return "Route non trouvée";
    case ESP01_CONNECTION_ERROR:
        return "Erreur de connexion";
    case ESP01_MEMORY_ERROR:
        return "Erreur mémoire";
    default:
        return "Code d'erreur inconnu";
    }
}

void esp01_cleanup_inactive_connections(void)
{
    uint32_t now = HAL_GetTick();
    for (int i = 0; i < g_connection_count; ++i)
    {
        if (g_connections[i].is_active && (now - g_connections[i].last_activity > ESP01_CONN_TIMEOUT_MS))
        {
            char cmd[ESP01_MAX_CMD_BUF];
            snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d", g_connections[i].conn_id);
            char resp[ESP01_CMD_RESP_BUF_SIZE];
            esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);

            g_connections[i].is_active = false;
            g_connections[i].conn_id = -1;
            g_connections[i].client_ip[0] = '\0';
            g_connections[i].server_port = 0;
            g_connections[i].client_port = 0;
            _esp_logln("[CONN] Connexion fermée pour inactivité");
        }
    }
}

int esp01_get_active_connection_count(void)
{
    int count = 0;
    for (int i = 0; i < g_connection_count; ++i)
    {
        if (g_connections[i].is_active)
            count++;
    }
    return count;
}
