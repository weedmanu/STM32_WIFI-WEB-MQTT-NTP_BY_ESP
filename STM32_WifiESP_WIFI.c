/**
 ******************************************************************************
 * @file    STM32_WifiESP_WIFI.c
 * @brief   Implémentation des fonctions haut niveau WiFi pour ESP01
 ******************************************************************************
 */

#include "STM32_WifiESP_WIFI.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief Récupère le mode WiFi courant (STA, AP, STA+AP).
 */
ESP01_Status_t esp01_get_wifi_mode(int *mode)
{
    if (!mode)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWMODE?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;
    char *line = strstr(resp, "+CWMODE:");
    if (line)
    {
        int val = 0;
        if (sscanf(line, "+CWMODE:%d", &val) == 1)
        {
            *mode = val;
            return ESP01_OK;
        }
    }
    return ESP01_FAIL;
}

/**
 * @brief Définit le mode WiFi (STA, AP, STA+AP).
 */
ESP01_Status_t esp01_set_wifi_mode(int mode)
{
    if (mode < ESP01_WIFI_MODE_STA || mode > ESP01_WIFI_MODE_STA_AP)
        return ESP01_INVALID_PARAM;
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);
    char resp[64];
    return esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
}

/**
 * @brief Retourne une chaîne lisible pour le mode WiFi.
 */
const char *esp01_wifi_mode_to_string(int mode)
{
    switch (mode)
    {
    case ESP01_WIFI_MODE_STA:
        return "STA";
    case ESP01_WIFI_MODE_AP:
        return "AP";
    case ESP01_WIFI_MODE_STA_AP:
        return "STA+AP";
    default:
        return "INCONNU";
    }
}

/**
 * @brief Scanne les réseaux WiFi à proximité.
 */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks)
{
    if (!networks || !found_networks || max_networks == 0)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    *found_networks = 0;
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWLAP", resp, sizeof(resp), "OK", ESP01_TIMEOUT_LONG);
    if (st != ESP01_OK)
        return st;

    // Parsing simplifié : +CWLAP:(<ecn>,<ssid>,<rssi>,<mac>,<channel>)
    char *line = strtok(resp, "\r\n");
    while (line && *found_networks < max_networks)
    {
        if (strncmp(line, "+CWLAP:", 7) == 0)
        {
            int ecn = 0, rssi = 0, channel = 0;
            char ssid[33] = {0}, mac[20] = {0};
            if (sscanf(line, "+CWLAP:(%d,\"%32[^\"]\",%d,\"%19[^\"]\",%d)", &ecn, ssid, &rssi, mac, &channel) >= 4)
            {
                strncpy(networks[*found_networks].ssid, ssid, 32);
                networks[*found_networks].ssid[32] = 0;
                networks[*found_networks].rssi = rssi;
                snprintf(networks[*found_networks].encryption, sizeof(networks[*found_networks].encryption), "%d", ecn);
                (*found_networks)++;
            }
        }
        line = strtok(NULL, "\r\n");
    }
    return ESP01_OK;
}

/**
 * @brief Affiche la liste des réseaux WiFi détectés dans un buffer texte.
 */
char *esp01_print_wifi_networks(char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return NULL;
    esp01_network_t nets[10];
    uint8_t found = 0;
    ESP01_Status_t st = esp01_scan_networks(nets, 10, &found);
    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "Scan WiFi : %s\n", esp01_get_error_string(st));
    for (uint8_t i = 0; i < found && pos < out_size - 1; i++)
    {
        pos += snprintf(out + pos, out_size - pos, "[%u] SSID:%s RSSI:%d ENC:%s\n",
                        i + 1, nets[i].ssid, nets[i].rssi, nets[i].encryption);
    }
    return out;
}

/**
 * @brief Active ou désactive le DHCP.
 */
ESP01_Status_t esp01_set_dhcp(bool enable)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWDHCP=1,%d", enable ? 1 : 0);
    char resp[64];
    return esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
}

/**
 * @brief Récupère l'état du DHCP.
 */
ESP01_Status_t esp01_get_dhcp(bool *enabled)
{
    if (!enabled)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWDHCP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;
    char *line = strstr(resp, "+CWDHCP:");
    if (line)
    {
        int mode = 0, en = 0;
        if (sscanf(line, "+CWDHCP:%d,%d", &mode, &en) == 2)
        {
            *enabled = (en != 0);
            return ESP01_OK;
        }
    }
    return ESP01_FAIL;
}

/**
 * @brief Déconnecte du réseau WiFi courant.
 */
ESP01_Status_t esp01_disconnect_wifi(void)
{
    char resp[64];
    return esp01_send_raw_command_dma("AT+CWQAP", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
}

/**
 * @brief Connecte au réseau WiFi spécifié.
 */
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
    if (!ssid || !password)
        return ESP01_INVALID_PARAM;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    char resp[ESP01_MAX_RESP_BUF];
    return esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_LONG);
}

/**
 * @brief Récupère l'adresse IP courante.
 */
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len)
{
    if (!ip_buf || buf_len == 0)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIFSR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;
    char *line = strstr(resp, "STAIP,\"");
    if (line)
    {
        line += 7;
        char *end = strchr(line, '"');
        if (end && (size_t)(end - line) < buf_len)
        {
            strncpy(ip_buf, line, end - line);
            ip_buf[end - line] = 0;
            return ESP01_OK;
        }
    }
    return ESP01_FAIL;
}

/**
 * @brief Récupère la config IP complète (IP, gateway, masque).
 */
ESP01_Status_t esp01_get_ip_config(char *ip, size_t ip_len, char *gw, size_t gw_len, char *mask, size_t mask_len)
{
    if (!ip || !gw || !mask)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTA?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;
    char *l_ip = strstr(resp, "+CIPSTA:ip:\"");
    char *l_gw = strstr(resp, "+CIPSTA:gateway:\"");
    char *l_mask = strstr(resp, "+CIPSTA:netmask:\"");
    if (l_ip && l_gw && l_mask)
    {
        l_ip += 12;
        l_gw += 16;
        l_mask += 15;
        char *e_ip = strchr(l_ip, '"');
        char *e_gw = strchr(l_gw, '"');
        char *e_mask = strchr(l_mask, '"');
        if (e_ip && e_gw && e_mask)
        {
            strncpy(ip, l_ip, (size_t)(e_ip - l_ip));
            ip[e_ip - l_ip] = 0;
            strncpy(gw, l_gw, (size_t)(e_gw - l_gw));
            gw[e_gw - l_gw] = 0;
            strncpy(mask, l_mask, (size_t)(e_mask - l_mask));
            mask[e_mask - l_mask] = 0;
            return ESP01_OK;
        }
    }
    return ESP01_FAIL;
}

/**
 * @brief Récupère la puissance du signal WiFi (RSSI).
 */
ESP01_Status_t esp01_get_rssi(int *rssi)
{
    if (!rssi)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;
    char *line = strstr(resp, "RSSI:");
    if (line)
    {
        if (sscanf(line, "RSSI:%d", rssi) == 1)
            return ESP01_OK;
    }
    return ESP01_FAIL;
}

/**
 * @brief Récupère l'adresse MAC du module.
 */
ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len)
{
    if (!mac_buf || buf_len == 0)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIFSR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;
    char *line = strstr(resp, "STAMAC,\"");
    if (line)
    {
        line += 8;
        char *end = strchr(line, '"');
        if (end && (size_t)(end - line) < buf_len)
        {
            strncpy(mac_buf, line, end - line);
            mac_buf[end - line] = 0;
            return ESP01_OK;
        }
    }
    return ESP01_FAIL;
}

/**
 * @brief Définit le hostname du module.
 */
ESP01_Status_t esp01_set_hostname(const char *hostname)
{
    if (!hostname)
        return ESP01_INVALID_PARAM;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CWHOSTNAME=\"%s\"", hostname);
    char resp[64];
    return esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
}

/**
 * @brief Récupère le hostname du module.
 */
ESP01_Status_t esp01_get_hostname(char *hostname, size_t len)
{
    if (!hostname || len == 0)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWHOSTNAME?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;
    char *line = strstr(resp, "+CWHOSTNAME:\"");
    if (line)
    {
        line += 13;
        char *end = strchr(line, '"');
        if (end && (size_t)(end - line) < len)
        {
            strncpy(hostname, line, end - line);
            hostname[end - line] = 0;
            return ESP01_OK;
        }
    }
    return ESP01_FAIL;
}

/**
 * @brief Liste les clients connectés en mode AP.
 */
ESP01_Status_t esp01_list_ap_clients(char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWLIF", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;
    strncpy(out, resp, out_size - 1);
    out[out_size - 1] = 0;
    return ESP01_OK;
}

/**
 * @brief Ping une adresse distante (connexion WiFi requise).
 */
ESP01_Status_t esp01_ping(const char *host)
{
    _esp_login("=== [PING] Ping distant ===");
    if (!host)
        return ESP01_INVALID_PARAM;
    char cmd[64], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"", host);
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000);
    _esp_login(">>> [PING] Réponse :\n%s", resp);
    return (st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

/**
 * @brief Ouvre une connexion TCP/UDP (connexion WiFi requise).
 */
ESP01_Status_t esp01_open_connection(const char *type, const char *addr, int port)
{
    _esp_login("=== [CIPSTART] Ouverture connexion TCP/UDP ===");
    if (!type || !addr || port <= 0)
        return ESP01_INVALID_PARAM;
    char cmd[128], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"%s\",\"%s\",%d", type, addr, port);
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 10000);
    _esp_login(">>> [CIPSTART] Réponse :\n%s", resp);
    return (st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

/**
 * @brief Récupère le statut TCP/IP du module.
 */
ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size)
{
    _esp_login("=== [CIPSTATUS] Lecture statut TCP ===");
    if (!out || out_size == 0)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTATUS", resp, sizeof(resp), "OK", 2000);
    strncpy(out, resp, out_size - 1);
    out[out_size - 1] = '\0';
    _esp_login(">>> [CIPSTATUS] Réponse :\n%s", resp);
    return (st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

/**
 * @brief Récupère le mode multi-connexion TCP/IP.
 */
ESP01_Status_t esp01_get_multiple_connections(bool *enabled)
{
    _esp_login("=== [CIPMUX] Lecture mode multi-connexion ===");
    if (!enabled)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF];
    esp01_send_raw_command_dma("AT+CIPMUX?", resp, sizeof(resp), "OK", 1000);
    char *line = strstr(resp, "+CIPMUX:");
    if (line)
    {
        int val = 0;
        if (sscanf(line, "+CIPMUX:%d", &val) == 1)
        {
            *enabled = (val != 0);
            _esp_login(">>> [CIPMUX] Multi-connexion : %s", *enabled ? "ON" : "OFF");
            return ESP01_OK;
        }
    }
    _esp_login(">>> [CIPMUX] Réponse :\n%s", resp);
    return ESP01_FAIL;
}

// ==================== WIFI ====================
ESP01_Status_t esp01_connect_wifi_config(
    ESP01_WifiMode_t mode,
    const char *ssid,
    const char *password,
    bool use_dhcp,
    const char *ip,
    const char *gateway,
    const char *netmask)
{
    ESP01_Status_t status;
    char cmd[ESP01_DMA_RX_BUF_SIZE];
    char resp[ESP01_DMA_RX_BUF_SIZE];

    _esp_login("[WIFI] === Début configuration WiFi ===");

    _esp_login("[WIFI] -> Définition du mode WiFi...");
    status = esp01_set_wifi_mode(mode);
    _esp_login(resp);
    if (status != ESP01_OK)
    {
        _esp_login("[WIFI] !! ERREUR: esp01_set_wifi_mode");
        return status;
    }
    HAL_Delay(300);

    if (mode == ESP01_WIFI_MODE_AP)
    {
        _esp_login("[WIFI] -> Configuration du point d'accès (AP)...");
        snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",5,3", ssid, password);
        status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);
        _esp_login(resp);
        if (status != ESP01_OK)
        {
            _esp_login("[WIFI] !! ERREUR: Configuration AP");
            return status;
        }
        HAL_Delay(300);

        if (ip && strlen(ip) > 0)
        {
            _esp_login("[WIFI] -> Configuration IP fixe AP...");
            snprintf(cmd, sizeof(cmd), "AT+CIPAP=\"%s\"", ip);
            status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);
            _esp_login(resp);
            if (status != ESP01_OK)
            {
                _esp_login("[WIFI] !! ERREUR: Configuration IP AP");
                return status;
            }
        }
    }

    if (use_dhcp)
    {
        if (mode == ESP01_WIFI_MODE_STA)
        {
            _esp_login("[WIFI] -> Activation du DHCP STA...");
            status = esp01_send_raw_command_dma("AT+CWDHCP=1,1", resp, sizeof(resp), "OK", 2000);
        }
        else if (mode == ESP01_WIFI_MODE_AP)
        {
            _esp_login("[WIFI] -> Activation du DHCP AP...");
            status = esp01_send_raw_command_dma("AT+CWDHCP=2,1", resp, sizeof(resp), "OK", 2000);
        }
        _esp_login(resp);
        if (status != ESP01_OK)
        {
            _esp_login("[WIFI] !! ERREUR: Activation DHCP");
            return status;
        }
    }
    else if (ip && gateway && netmask && mode == ESP01_WIFI_MODE_STA)
    {
        _esp_login("[WIFI] -> Déconnexion du WiFi (CWQAP)...");
        esp01_send_raw_command_dma("AT+CWQAP", resp, sizeof(resp), "OK", 2000);

        _esp_login("[WIFI] -> Désactivation du DHCP client...");
        status = esp01_send_raw_command_dma("AT+CWDHCP=0,1", resp, sizeof(resp), "OK", 2000);
        _esp_login(resp);
        if (status != ESP01_OK)
        {
            _esp_login("[WIFI] !! ERREUR: Désactivation DHCP");
            return status;
        }
        _esp_login("[WIFI] -> Configuration IP statique...");
        snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gateway, netmask);
        status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);
        _esp_login(resp);
        if (status != ESP01_OK)
        {
            _esp_login("[WIFI] !! ERREUR: Configuration IP statique");
            return status;
        }
    }

    if (mode == ESP01_WIFI_MODE_STA)
    {
        _esp_login("[WIFI] -> Connexion au réseau WiFi...");
        status = esp01_connect_wifi(ssid, password);
        if (status != ESP01_OK)
        {
            _esp_login("[WIFI] !! ERREUR: Connexion WiFi (CWJAP)");
            return status;
        }
        HAL_Delay(300);
    }

    _esp_login("[WIFI] -> Activation de l'affichage IP client dans +IPD (AT+CIPDINFO=1)...");
    status = esp01_send_raw_command_dma("AT+CIPDINFO=1", resp, sizeof(resp), "OK", 2000);
    _esp_login(resp);
    if (status != ESP01_OK)
    {
        _esp_login("[WIFI] !! ERREUR: AT+CIPDINFO=1");
        return status;
    }

    _esp_login("[WIFI] === Configuration WiFi terminée ===");
    return ESP01_OK;
}
