/**
 ******************************************************************************
 * @file    STM32_WifiESP_WIFI.h
 * @brief   Fonctions haut niveau WiFi pour ESP01 (scan, mode, DHCP, connexion, etc)
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_WIFI_H_
#define STM32_WIFIESP_WIFI_H_

#include "STM32_WifiESP.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ==================== MODES WIFI ====================
typedef enum
{
    ESP01_WIFI_MODE_STA = 1,   ///< Station (client)
    ESP01_WIFI_MODE_AP = 2,    ///< Point d'accès
    ESP01_WIFI_MODE_STA_AP = 3 ///< Station + AP simultané
} ESP01_WifiMode_t;

typedef struct
{
    char ssid[33];
    int rssi;
    char encryption[8];
} esp01_network_t;

// ==================== FONCTIONS WIFI ====================
ESP01_Status_t esp01_get_wifi_mode(int *mode);

ESP01_Status_t esp01_set_wifi_mode(int mode);

const char *esp01_wifi_mode_to_string(int mode);

ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks);

char *esp01_print_wifi_networks(char *out, size_t out_size);

ESP01_Status_t esp01_set_dhcp(bool enable);
ESP01_Status_t esp01_get_dhcp(bool *enabled);

ESP01_Status_t esp01_disconnect_wifi(void);

ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password);

ESP01_Status_t esp01_connect_wifi_config(
    ESP01_WifiMode_t mode,
    const char *ssid,
    const char *password,
    bool use_dhcp,
    const char *ip,
    const char *gateway,
    const char *netmask);

ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);

ESP01_Status_t esp01_get_ip_config(char *ip, size_t ip_len, char *gw, size_t gw_len, char *mask, size_t mask_len);

ESP01_Status_t esp01_get_rssi(int *rssi);

ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len);

ESP01_Status_t esp01_set_hostname(const char *hostname);

ESP01_Status_t esp01_get_hostname(char *hostname, size_t len);

ESP01_Status_t esp01_list_ap_clients(char *out, size_t out_size);

// Fonctions nécessitant une connexion WiFi active

ESP01_Status_t esp01_ping(const char *host);

ESP01_Status_t esp01_open_connection(const char *type, const char *addr, int port);

ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size);

ESP01_Status_t esp01_get_multiple_connections(bool *enabled);

#endif /* STM32_WIFIESP_WIFI_H_ */