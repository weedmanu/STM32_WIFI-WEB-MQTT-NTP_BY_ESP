/**
 ******************************************************************************
 * @file    STM32_WifiESP.h
 * @author  [Ton Nom]
 * @version 1.2.0
 * @date    [Date]
 * @brief   Driver bas niveau pour module ESP01 (UART, AT, debug, reset, config)
 *
 * @details
 * Ce header regroupe toutes les fonctions de gestion bas niveau du module ESP01,
 * ne nécessitant pas de connexion WiFi : initialisation, configuration UART,
 * gestion du mode sommeil, puissance RF, logs système, reset, restore, version,
 * envoi de commandes AT, gestion du buffer DMA, debug, etc.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - UART TX/RX avec DMA circulaire sur RX requis pour la communication ESP01.
 * - Toutes les fonctions ici sont utilisables sans connexion WiFi.
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_H_
#define STM32_WIFIESP_H_

#include "main.h"
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ESP01_DEBUG 1

// ==================== CONSTANTES ====================
#define ESP01_MAX_RESP_BUF 2048
#define ESP01_MAX_IP_LEN 32
#define ESP01_TIMEOUT_SHORT 1000
#define ESP01_TIMEOUT_LONG 15000
#define ESP01_TIMEOUT_MEDIUM 5000
#define ESP01_DMA_RX_BUF_SIZE 2048
#define ESP01_MAX_CMD_BUF 256
#define ESP01_SMALL_BUF_SIZE 64
#define ESP01_CMD_RESP_BUF_SIZE 512

// ==================== TYPES & ENUMS ====================
typedef enum
{
    ESP01_OK = 0,
    ESP01_FAIL,
    ESP01_TIMEOUT,
    ESP01_NOT_INITIALIZED,
    ESP01_INVALID_PARAM,
    ESP01_BUFFER_OVERFLOW,
    ESP01_WIFI_NOT_CONNECTED,
    ESP01_HTTP_PARSE_ERROR,
    ESP01_ROUTE_NOT_FOUND,
    ESP01_CONNECTION_ERROR,
    ESP01_MEMORY_ERROR,
    ESP01_EXIT
} ESP01_Status_t;

typedef struct
{
    uint32_t at_ok;
    uint32_t at_fail;
    uint32_t at_timeout;
    uint32_t reset;
    uint32_t restore;
    uint32_t wifi_connect;
    uint32_t wifi_disconnect;
    uint32_t tcp_connect;
    uint32_t tcp_disconnect;
    uint32_t http_get;
    uint32_t http_post;
    uint32_t mqtt_connect;
    uint32_t mqtt_disconnect;
    uint32_t ntp_sync;

    // Statistiques HTTP/MQTT
    uint32_t total_requests;
    uint32_t response_count;
    uint32_t successful_responses;
    uint32_t failed_responses;
    uint32_t total_response_time_ms;
    uint32_t avg_response_time_ms;
} esp01_stats_t;

void _esp_login(const char *fmt, ...);

// ==================== INIT & DRIVER ====================
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);
ESP01_Status_t esp01_test_at(void);
ESP01_Status_t esp01_reset(void);
ESP01_Status_t esp01_restore(void);

// ==================== VERSION & INFOS ====================
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size);
ESP01_Status_t esp01_get_connection_status(void);

// ==================== UART ====================
ESP01_Status_t esp01_get_uart_config(char *out, size_t out_size);
ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size);
ESP01_Status_t esp01_set_uart_config(uint32_t baud, uint8_t databits, uint8_t stopbits, uint8_t parity, uint8_t flowctrl);

// ==================== MODE SOMMEIL ====================
ESP01_Status_t esp01_get_sleep_mode(int *mode);
ESP01_Status_t esp01_set_sleep_mode(int mode);
ESP01_Status_t esp01_sleep_mode_to_string(int mode, char *out, size_t out_size);

// ==================== PUISSANCE RF ====================
ESP01_Status_t esp01_get_rf_power(int *dbm);
ESP01_Status_t esp01_set_rf_power(int dbm);

// ==================== LOGS SYSTÈME ====================
ESP01_Status_t esp01_get_syslog(int *level);
ESP01_Status_t esp01_set_syslog(int level);
ESP01_Status_t esp01_syslog_to_string(int syslog, char *out, size_t out_size);

// ==================== RAM LIBRE ====================
ESP01_Status_t esp01_get_sysram(uint32_t *free_ram);

// ==================== DEEP SLEEP ====================
ESP01_Status_t esp01_deep_sleep(uint32_t ms);

// ==================== COMMANDES AT LISTE ====================
ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size);

// ==================== UTILS & DEBUG ====================
int esp01_get_new_data(uint8_t *buf, uint16_t bufsize);
void _flush_rx_buffer(uint32_t timeout_ms);
const char *esp01_get_error_string(ESP01_Status_t status);
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *resp, size_t resp_size, const char *wait_pattern, uint32_t timeout_ms);
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms);

// ==================== MACRO UTILE ====================
#define VALIDATE_PARAM(expr, errcode) \
    do                                \
    {                                 \
        if (!(expr))                  \
            return (errcode);         \
    } while (0)

// ==================== VARIABLES GLOBALES ====================
extern UART_HandleTypeDef *g_esp_uart;
extern UART_HandleTypeDef *g_debug_uart;
extern uint8_t *g_dma_rx_buf;
extern uint16_t g_dma_buf_size;
extern volatile uint16_t g_rx_last_pos;
extern uint16_t g_server_port;
extern esp01_stats_t g_stats;

#endif /* STM32_WIFIESP_H_ */
