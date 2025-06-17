/**
 ******************************************************************************
 * @file    STM32_WifiESP_WIFI.h
 * @author  manu
 * @version 1.2.0
 * @date    13 juin 2025
 * @brief   Fonctions haut niveau WiFi pour ESP01 (scan, mode, DHCP, connexion, etc)
 *
 * @details
 * Ce header regroupe toutes les fonctions de gestion WiFi haut niveau du module ESP01 :
 * - Modes WiFi (STA, AP, STA+AP)
 * - Scan des réseaux, connexion, déconnexion
 * - DHCP, IP, MAC, hostname, clients AP
 * - TCP/IP, ping, statut, multi-connexion
 *
 * @note
 * - Nécessite le driver bas niveau STM32_WifiESP.h
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_WIFI_H_
#define STM32_WIFIESP_WIFI_H_

/* ========================== INCLUDES ========================== */
#include "STM32_WifiESP.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* =========================== MACROS =========================== */
#define ESP01_MAX_SSID_LEN 32
#define ESP01_MAX_SSID_BUF (ESP01_MAX_SSID_LEN + 1)
#define ESP01_MAX_ENCRYPTION_LEN 8
#define ESP01_MAX_IP_LEN 32
#define ESP01_MAX_HOSTNAME_LEN 64
#define ESP01_MAX_MAC_LEN 18
#define ESP01_MAX_SCAN_NETWORKS 10

/* =========================== TYPES ============================ */
/**
 * @brief  Structure représentant un réseau WiFi détecté lors d'un scan.
 */
typedef struct
{
    char ssid[ESP01_MAX_SSID_BUF];             ///< SSID du réseau (max 32 + \0)
    int rssi;                                  ///< Puissance du signal (dBm)
    char encryption[ESP01_MAX_ENCRYPTION_LEN]; ///< Type d'encryptage (texte ou code)
} esp01_network_t;

typedef enum
{
    ESP01_WIFI_MODE_STA = 1,
    ESP01_WIFI_MODE_AP = 2,
    ESP01_WIFI_MODE_STA_AP = 3
} ESP01_WifiMode_t;

/* ======================= FONCTIONS PRINCIPALES ======================= */

/* Modes WiFi */
ESP01_Status_t esp01_get_connection_status(void);
ESP01_Status_t esp01_get_wifi_mode(int *mode);
ESP01_Status_t esp01_set_wifi_mode(int mode);
const char *esp01_wifi_mode_to_string(int mode);

/* Scan WiFi */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks);
char *esp01_print_wifi_networks(char *out, size_t out_size);

/* DHCP */
ESP01_Status_t esp01_set_dhcp(bool enable);
ESP01_Status_t esp01_get_dhcp(bool *enabled);

/* Connexion/Déconnexion WiFi */
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

/* IP, MAC, Hostname */
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);
ESP01_Status_t esp01_get_ip_config(char *ip, size_t ip_len, char *gw, size_t gw_len, char *mask, size_t mask_len);
ESP01_Status_t esp01_get_rssi(int *rssi);
ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len);

ESP01_Status_t esp01_set_hostname(const char *hostname);
ESP01_Status_t esp01_get_hostname(char *hostname, size_t len);

/* TCP/IP & OUTILS RÉSEAU */
ESP01_Status_t esp01_ping(const char *host);
ESP01_Status_t esp01_open_connection(const char *type, const char *addr, int port);
ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size);
ESP01_Status_t esp01_get_multiple_connections(bool *enabled);

/* Fonctions de récupération d'état (copie de réponse brute) */
ESP01_Status_t esp01_get_wifi_connection(char *out, size_t out_size);
ESP01_Status_t esp01_get_dhcp_status(char *out, size_t out_size);
ESP01_Status_t esp01_get_ip_info(char *out, size_t out_size);
ESP01_Status_t esp01_get_hostname_raw(char *out, size_t out_size);
ESP01_Status_t esp01_get_ap_config(char *out, size_t out_size);
ESP01_Status_t esp01_get_wifi_state(char *out, size_t out_size);

/* Configuration avancée */
ESP01_Status_t esp01_set_ip(const char *ip, const char *gw, const char *mask);
ESP01_Status_t esp01_start_ap_config(const char *ssid, const char *password, int channel, int encryption);

/* ======================= FONCTIONS UTILITAIRES ======================= */
/* Fonctions to_string pour affichage lisible */
const char *esp01_encryption_to_string(const char *code);
const char *esp01_tcp_status_to_string(const char *resp);
const char *esp01_cwstate_to_string(const char *resp);
const char *esp01_connection_status_to_string(const char *resp);
const char *esp01_ping_result_to_string(const char *resp);
const char *esp01_cwjap_to_string(const char *resp);
const char *esp01_rf_power_to_string(int rf_dbm);
const char *esp01_cwqap_to_string(const char *resp);
const char *esp01_wifi_connection_to_string(const char *resp);
const char *esp01_dhcp_status_to_string(const char *resp);
const char *esp01_ip_info_to_string(const char *resp);
const char *esp01_hostname_raw_to_string(const char *resp);
const char *esp01_ap_config_to_string(const char *resp);

/* ======================= TESTS MODULE WIFI ======================= */
void test_wifi_module_STA(const char *ssid, const char *password);
void test_wifi_module_AP(const char *ssid, const char *password);

#endif /* STM32_WIFIESP_WIFI_H_ */
