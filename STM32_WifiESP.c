/**
 ******************************************************************************
 * @file    STM32_WifiESP.c
 * @brief   Implémentation du driver bas niveau ESP01 (UART, AT, debug, reset, etc)
 ******************************************************************************
 */

#include "STM32_WifiESP.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

// ==================== VARIABLES GLOBALES ====================
esp01_stats_t g_stats = {0};
UART_HandleTypeDef *g_esp_uart = NULL;
UART_HandleTypeDef *g_debug_uart = NULL;
uint8_t *g_dma_rx_buf = NULL;
uint16_t g_dma_buf_size = 0;
volatile uint16_t g_rx_last_pos = 0;
uint16_t g_server_port = 80;

// ==================== LOGGING ====================

void _esp_login(const char *fmt, ...)
{
#if ESP01_DEBUG
    if (g_debug_uart && fmt)
    {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)buf, strlen(buf), HAL_MAX_DELAY);
        const char crlf[] = "\r\n";
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)crlf, 2, HAL_MAX_DELAY);
    }
#endif
}

// ==================== DRIVER & COMMUNICATION ====================

ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
                          uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
    _esp_login("=== [INIT] Initialisation du driver ESP01 ===");
    g_esp_uart = huart_esp;
    g_debug_uart = huart_debug;
    g_dma_rx_buf = dma_rx_buf;
    g_dma_buf_size = dma_buf_size;
    g_server_port = 80;

    if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK)
    {
        _esp_login(">>> [INIT] Erreur initialisation DMA RX");
        return ESP01_FAIL;
    }
    _esp_login(">>> [INIT] Initialisation du driver ESP01 : OK");
    return ESP01_OK;
}

ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms)
{
    _esp_login("=== [FLUSH] Flush du buffer DMA/RX ===");
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        uint8_t dummy[ESP01_SMALL_BUF_SIZE];
        int len = esp01_get_new_data(dummy, sizeof(dummy));
        if (len == 0)
            HAL_Delay(1);
    }
    _esp_login(">>> [FLUSH] Buffer UART/DMA vidé");
    return ESP01_OK;
}

ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer, size_t response_buf_size,
                                          const char *expected, uint32_t timeout_ms)
{
    _esp_login("=== [RAWCMD] Envoi d'une commande AT ===");

    esp01_flush_rx_buffer(10);

    _esp_login(">>> [RAWCMD] AT > %s", cmd);

    if (!g_esp_uart || !cmd)
        return ESP01_NOT_INITIALIZED;

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);

    uint32_t start = HAL_GetTick();
    size_t resp_len = 0;
    response_buffer[0] = '\0';

    while ((HAL_GetTick() - start) < timeout_ms && resp_len < response_buf_size - 1)
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];
        int len = esp01_get_new_data(buf, sizeof(buf));
        if (len > 0)
        {
            if (resp_len + len >= response_buf_size - 1)
                len = response_buf_size - 1 - resp_len;
            memcpy(response_buffer + resp_len, buf, len);
            resp_len += len;
            response_buffer[resp_len] = '\0';

            if (expected && strstr(response_buffer, expected))
                break;
        }
        else
        {
            HAL_Delay(1);
        }
    }
    _esp_login(">>> [RAWCMD] AT < %s", response_buffer);

    return (expected && strstr(response_buffer, expected)) ? ESP01_OK : ESP01_TIMEOUT;
}

ESP01_Status_t esp01_test_at(void)
{
    _esp_login("=== [AT] Test de communication AT ===");
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", 1000);
    _esp_login(">>> [AT] Réponse : %s", resp);
    return st;
}

ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size)
{
    _esp_login("=== [GMR] Lecture version firmware ESP01 ===");
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+GMR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [GMR] Réponse : %s", resp);
    if (st != ESP01_OK)
        return ESP01_FAIL;
    strncpy(version_buf, resp, buf_size - 1);
    version_buf[buf_size - 1] = '\0';
    _esp_login(">>> [GMR] Version : %s", version_buf);
    return ESP01_OK;
}

ESP01_Status_t esp01_get_connection_status(void)
{
    _esp_login("=== [STATUS] Lecture statut connexion WiFi ===");
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [STATUS] Réponse : %s", resp);
    if (st != ESP01_OK)
        return ESP01_FAIL;
    if (strstr(resp, "+CWJAP:"))
        return ESP01_OK;
    return ESP01_WIFI_NOT_CONNECTED;
}

ESP01_Status_t esp01_reset(void)
{
    _esp_login("=== [RESET] Reset logiciel (AT+RST) ===");
    esp01_flush_rx_buffer(10);

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+RST\r\n", 8, HAL_MAX_DELAY);

    uint32_t start = HAL_GetTick();
    char resp[ESP01_MAX_RESP_BUF] = {0};
    size_t resp_len = 0;
    while ((HAL_GetTick() - start) < 3000 && resp_len < sizeof(resp) - 1)
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];
        int len = esp01_get_new_data(buf, sizeof(buf));
        if (len > 0)
        {
            if (resp_len + len >= sizeof(resp) - 1)
                len = sizeof(resp) - 1 - resp_len;
            memcpy(resp + resp_len, buf, len);
            resp_len += len;
            resp[resp_len] = '\0';
        }
        else
        {
            HAL_Delay(1);
        }
    }
    _esp_login(">>> [RESET] Réponse complète : %s", resp);

    HAL_Delay(1000);
    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [RESET] Test AT après reset : %s", resp);
    if (st == ESP01_OK)
    {
        _esp_login(">>> [RESET] AT OK après reset, reset réussi");
        return ESP01_OK;
    }
    else
    {
        _esp_login(">>> [RESET] AT échoué après reset");
        return ESP01_FAIL;
    }
}

ESP01_Status_t esp01_restore(void)
{
    _esp_login("=== [RESTORE] Restore usine (AT+RESTORE) ===");
    esp01_flush_rx_buffer(10);

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+RESTORE\r\n", 12, HAL_MAX_DELAY);

    uint32_t start = HAL_GetTick();
    char resp[ESP01_MAX_RESP_BUF] = {0};
    size_t resp_len = 0;
    while ((HAL_GetTick() - start) < 3000 && resp_len < sizeof(resp) - 1)
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];
        int len = esp01_get_new_data(buf, sizeof(buf));
        if (len > 0)
        {
            if (resp_len + len >= sizeof(resp) - 1)
                len = sizeof(resp) - 1 - resp_len;
            memcpy(resp + resp_len, buf, len);
            resp_len += len;
            resp[resp_len] = '\0';
        }
        else
        {
            HAL_Delay(1);
        }
    }
    _esp_login(">>> [RESTORE] Réponse complète : %s", resp);

    HAL_Delay(1000);
    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [RESTORE] Test AT après restore : %s", resp);
    if (st == ESP01_OK)
    {
        _esp_login(">>> [RESTORE] AT OK après restore, restore réussi");
        return ESP01_OK;
    }
    else
    {
        _esp_login(">>> [RESTORE] AT échoué après restore");
        return ESP01_FAIL;
    }
}

ESP01_Status_t esp01_get_uart_config(char *out, size_t out_size)
{
    _esp_login("=== [UART] Lecture configuration UART ===");
    if (!out || out_size == 0)
        return ESP01_INVALID_PARAM;

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+UART?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [UART] Réponse : %s", resp);
    if (st != ESP01_OK)
        return st;

    // Cherche la ligne +UART_CUR: ou +UART:
    char *line = strstr(resp, "+UART:");
    if (!line)
        line = strstr(resp, "+UART_CUR:");
    if (line)
    {
        // Avance jusqu'à la première occurrence de ':' puis copie la config
        char *conf = strchr(line, ':');
        if (conf)
        {
            conf++; // saute ':'
            // Copie jusqu'à la fin de ligne ou la taille max
            size_t len = 0;
            while (conf[len] && conf[len] != '\r' && conf[len] != '\n' && len < out_size - 1)
                len++;
            strncpy(out, conf, len);
            out[len] = '\0';
            _esp_login(">>> [UART] Config brute : %s", out);
            return ESP01_OK;
        }
    }
    return ESP01_FAIL;
}

ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size)
{
    _esp_login("=== [UART] Décodage configuration UART ===");
    if (!raw_config || !out || out_size == 0)
        return ESP01_INVALID_PARAM;

    uint32_t baud = 0;
    int data = 0, stop = 0, parity = 0, flow = 0;
    if (sscanf(raw_config, "%lu,%d,%d,%d,%d", &baud, &data, &stop, &parity, &flow) != 5)
        return ESP01_FAIL;

    const char *parity_str = "aucune";
    if (parity == 1)
        parity_str = "impair";
    else if (parity == 2)
        parity_str = "pair";

    const char *flow_str = "aucun";
    if (flow == 1)
        flow_str = "RTS";
    else if (flow == 2)
        flow_str = "CTS";
    else if (flow == 3)
        flow_str = "RTS+CTS";

    snprintf(out, out_size, "baudrate=%lu, data bits=%d, stop bits=%d, parité=%s, flow control=%s",
             baud, data, stop, parity_str, flow_str);

    _esp_login(">>> [UART] Config lisible : %s", out);
    return ESP01_OK;
}

ESP01_Status_t esp01_set_uart_config(uint32_t baud, uint8_t databits, uint8_t stopbits, uint8_t parity, uint8_t flowctrl)
{
    _esp_login("=== [UART] Définition configuration UART ===");
    if (databits < 5 || databits > 8 || stopbits < 1 || stopbits > 2 || parity > 2 || flowctrl > 3)
        return ESP01_INVALID_PARAM;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+UART=%lu,%u,%u,%u,%u", baud, databits, stopbits, parity, flowctrl);

    char resp[ESP01_SMALL_BUF_SIZE * 4] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [UART] Réponse : %s", resp);
    return st;
}

ESP01_Status_t esp01_get_sleep_mode(int *mode)
{
    _esp_login("=== [SLEEP] Lecture mode sommeil ===");
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SLEEP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [SLEEP] Réponse : %s", resp);
    if (st != ESP01_OK)
        return ESP01_FAIL;

    // Extraction du mode
    char *ptr = strstr(resp, "+SLEEP:");
    if (ptr)
    {
        *mode = atoi(ptr + 7);
        _esp_login(">>> [SLEEP] Mode sommeil brut : %d", *mode);
        switch (*mode)
        {
        case 0:
            _esp_login(">>> [SLEEP] Mode sommeil lisible : Pas de sommeil (no sleep, 0)");
            break;
        case 1:
            _esp_login(">>> [SLEEP] Mode sommeil lisible : Sommeil léger (light sleep, 1)");
            break;
        case 2:
            _esp_login(">>> [SLEEP] Mode sommeil lisible : Sommeil modem (modem sleep, 2)");
            break;
        default:
            _esp_login(">>> [SLEEP] Mode sommeil lisible : Inconnu");
            break;
        }
        return ESP01_OK;
    }
    return ESP01_FAIL;
}

ESP01_Status_t esp01_set_sleep_mode(int mode)
{
    _esp_login("=== [SLEEP] Définition mode sommeil ===");
    if (mode < 0 || mode > 2)
        return ESP01_INVALID_PARAM;
    char cmd[16], resp[64];
    snprintf(cmd, sizeof(cmd), "AT+SLEEP=%d", mode);
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [SLEEP] Réponse : %s", resp);
    return st;
}

ESP01_Status_t esp01_sleep_mode_to_string(int mode, char *out, size_t out_size)
{
    _esp_login("=== [SLEEP] Conversion mode sommeil en chaîne ===");
    if (!out || out_size == 0)
        return ESP01_INVALID_PARAM;

    const char *desc = "Inconnu";
    switch (mode)
    {
    case 0:
        desc = "Pas de sommeil (no sleep, 0)";
        break;
    case 1:
        desc = "Sommeil léger (light sleep, 1)";
        break;
    case 2:
        desc = "Sommeil modem (modem sleep, 2)";
        break;
    }
    snprintf(out, out_size, "%s", desc);
    _esp_login(">>> [SLEEP] Description : %s", out);
    return ESP01_OK;
}

ESP01_Status_t esp01_get_rf_power(int *dbm)
{
    _esp_login("=== [RFPOWER] Lecture puissance RF ===");
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+RFPOWER?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [RFPOWER] Réponse : %s", resp);
    if (st != ESP01_OK)
        return ESP01_FAIL;

    char *ptr = strstr(resp, "+RFPOWER:");
    if (ptr)
    {
        *dbm = atoi(ptr + 9);
        _esp_login(">>> [RFPOWER] Puissance RF brute : %d dBm", *dbm);
        return ESP01_OK;
    }
    return ESP01_FAIL;
}

ESP01_Status_t esp01_set_rf_power(int dbm)
{
    _esp_login("=== [RFPOWER] Définition puissance RF ===");
    if (dbm < 0 || dbm > 82)
        return ESP01_INVALID_PARAM;
    char cmd[24];
    snprintf(cmd, sizeof(cmd), "AT+RFPOWER=%d", dbm);

    char resp[ESP01_SMALL_BUF_SIZE * 4] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [RFPOWER] Réponse : %s", resp);
    return st;
}

ESP01_Status_t esp01_get_syslog(int *level)
{
    _esp_login("=== [SYSLOG] Lecture niveau log système ===");
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSLOG?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [SYSLOG] Réponse : %s", resp);
    if (st != ESP01_OK)
        return ESP01_FAIL;

    char *ptr = strstr(resp, "+SYSLOG:");
    if (ptr)
    {
        *level = atoi(ptr + 8);
        _esp_login(">>> [SYSLOG] Niveau log brut : %d", *level);
        switch (*level)
        {
        case 0:
            _esp_login(">>> [SYSLOG] Niveau log lisible : désactivé (0)");
            break;
        case 1:
            _esp_login(">>> [SYSLOG] Niveau log lisible : erreur (1)");
            break;
        case 2:
            _esp_login(">>> [SYSLOG] Niveau log lisible : warning (2)");
            break;
        case 3:
            _esp_login(">>> [SYSLOG] Niveau log lisible : info (3)");
            break;
        case 4:
            _esp_login(">>> [SYSLOG] Niveau log lisible : debug (4)");
            break;
        default:
            _esp_login(">>> [SYSLOG] Niveau log lisible : inconnu");
            break;
        }
        return ESP01_OK;
    }
    return ESP01_FAIL;
}

ESP01_Status_t esp01_set_syslog(int level)
{
    _esp_login("=== [SYSLOG] Définition niveau log système ===");
    if (level < 0 || level > 4)
        return ESP01_INVALID_PARAM;

    char cmd[16], resp[64];
    snprintf(cmd, sizeof(cmd), "AT+SYSLOG=%d", level);
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [SYSLOG] Réponse : %s", resp);
    return st;
}

ESP01_Status_t esp01_syslog_to_string(int syslog, char *out, size_t out_size)
{
    _esp_login("=== [SYSLOG] Conversion niveau log en chaîne ===");
    if (!out || out_size == 0)
        return ESP01_INVALID_PARAM;

    const char *desc = "Inconnu";
    switch (syslog)
    {
    case 0:
        desc = "Désactivé (0)";
        break;
    case 1:
        desc = "Erreur (1)";
        break;
    case 2:
        desc = "Warning (2)";
        break;
    case 3:
        desc = "Info (3)";
        break;
    case 4:
        desc = "Debug (4)";
        break;
    }
    snprintf(out, out_size, "%s", desc);
    _esp_login(">>> [SYSLOG] Description : %s", out);
    return ESP01_OK;
}

ESP01_Status_t esp01_get_sysram(uint32_t *free_ram)
{
    _esp_login("=== [SYSRAM] Lecture RAM libre ===");
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSRAM?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [SYSRAM] Réponse : %s", resp);
    if (st != ESP01_OK)
        return ESP01_FAIL;

    char *ptr = strstr(resp, "+SYSRAM:");
    if (ptr)
    {
        *free_ram = (uint32_t)atoi(ptr + 8);
        _esp_login(">>> [SYSRAM] RAM libre brute : %lu octets", *free_ram);
        return ESP01_OK;
    }
    return ESP01_FAIL;
}

ESP01_Status_t esp01_deep_sleep(uint32_t ms)
{
    _esp_login("=== [GSLP] Deep sleep ===");
    char cmd[32], resp[64];
    snprintf(cmd, sizeof(cmd), "AT+GSLP=%lu", ms);
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [GSLP] Réponse : %s", resp);
    return st;
}

int esp01_get_new_data(uint8_t *buf, uint16_t bufsize)
{
    // Pas de log ici pour éviter de flooder
    if (!g_dma_rx_buf || bufsize == 0)
        return 0;

    uint16_t pos = g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx);
    int len = 0;

    if (pos != g_rx_last_pos)
    {
        if (pos > g_rx_last_pos)
        {
            len = pos - g_rx_last_pos;
            memcpy(buf, &g_dma_rx_buf[g_rx_last_pos], len);
        }
        else
        {
            len = g_dma_buf_size - g_rx_last_pos;
            memcpy(buf, &g_dma_rx_buf[g_rx_last_pos], len);
            if (pos > 0)
            {
                memcpy(buf + len, &g_dma_rx_buf[0], pos);
                len += pos;
            }
        }
        g_rx_last_pos = pos;
    }
    return len;
}

void _flush_rx_buffer(uint32_t timeout_ms)
{
    _esp_login("=== [FLUSH] Flush RX buffer utilitaire ===");
    uint32_t start = HAL_GetTick();
    uint8_t dummy[ESP01_SMALL_BUF_SIZE];
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        int len = esp01_get_new_data(dummy, sizeof(dummy));
        if (len == 0)
            HAL_Delay(1);
    }
    _esp_login(">>> [FLUSH] Buffer RX vidé (utilitaire)");
}

const char *esp01_get_error_string(ESP01_Status_t status)
{
    switch (status)
    {
    case ESP01_OK:
        return "OK";
    case ESP01_FAIL:
        return "Erreur";
    case ESP01_TIMEOUT:
        return "Timeout";
    case ESP01_NOT_INITIALIZED:
        return "Non initialisé";
    case ESP01_INVALID_PARAM:
        return "Paramètre invalide";
    case ESP01_BUFFER_OVERFLOW:
        return "Débordement buffer";
    case ESP01_WIFI_NOT_CONNECTED:
        return "WiFi non connecté";
    case ESP01_HTTP_PARSE_ERROR:
        return "Erreur parsing HTTP";
    case ESP01_ROUTE_NOT_FOUND:
        return "Route HTTP non trouvée";
    case ESP01_CONNECTION_ERROR:
        return "Erreur connexion";
    case ESP01_MEMORY_ERROR:
        return "Erreur mémoire";
    case ESP01_EXIT:
        return "Sortie";
    default:
        return "Code inconnu";
    }
}

ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms)
{
    _esp_login("=== [WAIT] Attente du pattern '%s' pendant %lu ms ===", pattern, timeout_ms);
    char resp[ESP01_MAX_RESP_BUF] = {0};
    uint32_t start = HAL_GetTick();
    size_t resp_len = 0;

    while ((HAL_GetTick() - start) < timeout_ms && resp_len < sizeof(resp) - 1)
    {
        uint8_t buf[32];
        int len = esp01_get_new_data(buf, sizeof(buf));
        if (len > 0)
        {
            if (resp_len + len >= sizeof(resp) - 1)
                len = sizeof(resp) - 1 - resp_len;
            memcpy(resp + resp_len, buf, len);
            resp_len += len;
            resp[resp_len] = '\0';

            _esp_login(">>> [WAIT] Flux reçu : '%s'", resp);

            if (pattern && strstr(resp, pattern))
            {
                _esp_login(">>> [WAIT] Pattern '%s' trouvé", pattern);
                return ESP01_OK;
            }
        }
        else
        {
            HAL_Delay(1);
        }
    }
    _esp_login(">>> [WAIT] Pattern '%s' NON trouvé", pattern);
    return (pattern && strstr(resp, pattern)) ? ESP01_OK : ESP01_TIMEOUT;
}

ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size)
{
    _esp_login("=== [CMD] Lecture de la liste des commandes AT (ligne par ligne) ===");
    if (!out || out_size < 8192)
        return ESP01_INVALID_PARAM;

    size_t total_len = 0;
    uint32_t start = HAL_GetTick();
    int found_ok = 0;
    char line[256];
    size_t line_len = 0;

    _flush_rx_buffer(100);
    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+CMD?\r\n", 9, HAL_MAX_DELAY);

    while ((HAL_GetTick() - start) < 30000 && total_len < out_size - 1)
    {
        uint8_t buf[64];
        int len = esp01_get_new_data(buf, sizeof(buf));
        for (int i = 0; i < len; i++)
        {
            char c = buf[i];
            if (line_len < sizeof(line) - 1)
                line[line_len++] = c;

            // Fin de ligne détectée
            if (c == '\n')
            {
                line[line_len] = '\0';
                // Ajoute la ligne au buffer global
                size_t copy_len = (total_len + line_len < out_size - 1) ? line_len : (out_size - 1 - total_len);
                memcpy(out + total_len, line, copy_len);
                total_len += copy_len;
                out[total_len] = '\0';

                // Vérifie si c'est la ligne "OK"
                if (strstr(line, "OK"))
                {
                    found_ok = 1;
                    break;
                }
                line_len = 0; // Réinitialise la ligne
            }
        }
        if (found_ok)
            break;
        if (len == 0)
            HAL_Delay(1);
    }

    if (found_ok)
    {
        _esp_login(">>> [CMD] Liste complète reçue (%lu octets)", (unsigned long)total_len);
        return ESP01_OK;
    }
    else
    {
        _esp_login(">>> [CMD] Timeout ou buffer plein (%lu octets)", (unsigned long)total_len);
        return ESP01_TIMEOUT;
    }
}
