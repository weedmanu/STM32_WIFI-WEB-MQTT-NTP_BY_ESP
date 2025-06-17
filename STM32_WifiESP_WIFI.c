/**
 ******************************************************************************
 * @file    STM32_WifiESP_WIFI.c
 * @author  manu
 * @version 1.2.0
 * @date    13 juin 2025
 * @brief   Implémentation des fonctions haut niveau WiFi pour ESP01
 ******************************************************************************
 */

#include "STM32_WifiESP_WIFI.h"
#include <string.h>
#include <stdio.h>

/* ========================== OUTILS FACTORISÉS ========================== */

/**
 * @brief  Copie une réponse dans un buffer utilisateur en protégeant contre le débordement.
 */
static ESP01_Status_t esp01_copy_resp(char *dst, size_t dst_size, const char *src)
{
    if (!dst || !src || dst_size == 0)
        return ESP01_INVALID_PARAM;
    size_t len = strlen(src);
    if (len >= dst_size)
        return ESP01_BUFFER_OVERFLOW;
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = 0;
    return ESP01_OK;
}

/**
 * @brief  Envoie une commande AT et gère le buffer de réponse.
 */
static ESP01_Status_t esp01_send_at_with_resp(const char *cmd, const char *expected, int timeout_ms, char *resp, size_t resp_size)
{
    if (!cmd || !expected)
        return ESP01_INVALID_PARAM;
    return esp01_send_raw_command_dma(cmd, resp, resp_size, expected, timeout_ms);
}

/*
static void esp01_log_error(const char *prefix, ESP01_Status_t status)
{
    _esp_login(">>> [%s] Erreur : %s", prefix, esp01_get_error_string(status));
}
*/

/* ========================== FONCTIONS PRINCIPALES ========================== */

/* --- Modes WiFi --- */
ESP01_Status_t esp01_get_connection_status(void)
{
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWJAP?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    ESP01_LOG_DEBUG("STATUS", "Réponse : %s", resp);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("STATUS", ESP01_FAIL);

    if (strstr(resp, "+CWJAP:"))
    {
        ESP01_LOG_INFO("STATUS", "Motif trouvé : +CWJAP (connecté)");
        return ESP01_OK;
    }
    ESP01_LOG_WARN("STATUS", "Motif non trouvé : non connecté");
    return ESP01_WIFI_NOT_CONNECTED;
}

ESP01_Status_t esp01_get_wifi_mode(int *mode)
{
    VALIDATE_PARAM(mode, ESP01_INVALID_PARAM);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWMODE?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWMODE", st);

    char *line = strstr(resp, "+CWMODE:");
    if (line)
    {
        int val = 0;
        if (sscanf(line, "+CWMODE:%d", &val) == 1)
        {
            *mode = val;
            ESP01_LOG_INFO("CWMODE", "Mode WiFi lu : %d", val);
            return ESP01_OK;
        }
    }
    ESP01_LOG_WARN("CWMODE", "Motif non trouvé dans : %s", resp);
    return ESP01_FAIL;
}

ESP01_Status_t esp01_set_wifi_mode(int mode)
{
    if (mode < ESP01_WIFI_MODE_STA || mode > ESP01_WIFI_MODE_STA_AP)
        return ESP01_INVALID_PARAM;
    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWMODE", st);
    ESP01_LOG_INFO("CWMODE", "Mode WiFi défini : %d", mode);
    return st;
}

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

ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size)
{
    ESP01_LOG_INFO("CIPSTATUS", "=== Lecture statut TCP ===");
    if (!out || out_size == 0)
        return ESP01_INVALID_PARAM;
    char resp[ESP01_MAX_RESP_BUF] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTATUS", resp, sizeof(resp), "OK", 2000);
    strncpy(out, resp, out_size - 1);
    out[out_size - 1] = '\0';
    ESP01_LOG_INFO("CIPSTATUS", ">>> Réponse :\n%s", resp);
    return (st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

/* --- Scan WiFi --- */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks)
{
    VALIDATE_PARAM(networks && found_networks && max_networks > 0, ESP01_INVALID_PARAM);
    char resp[ESP01_LARGE_RESP_BUF];
    *found_networks = 0;
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWLAP", "OK", ESP01_TIMEOUT_LONG, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWLAP", st);

    char *line = strtok(resp, "\r\n");
    while (line && *found_networks < max_networks)
    {
        if (strncmp(line, "+CWLAP:", 7) == 0)
        {
            int ecn = 0, rssi = 0, channel = 0;
            char ssid[ESP01_MAX_SSID_BUF] = {0};
            char mac[ESP01_MAX_MAC_LEN] = {0};
            if (sscanf(line, "+CWLAP:(%d,\"%32[^\"]\",%d,\"%17[^\"]\",%d)", &ecn, ssid, &rssi, mac, &channel) >= 4)
            {
                size_t ssid_len = strlen(ssid);
                if (esp01_check_buffer_size(ssid_len, sizeof(networks[*found_networks].ssid) - 1) != ESP01_OK)
                    continue;
                strncpy(networks[*found_networks].ssid, ssid, ESP01_MAX_SSID_LEN);
                networks[*found_networks].ssid[ESP01_MAX_SSID_LEN] = 0;
                networks[*found_networks].rssi = rssi;
                snprintf(networks[*found_networks].encryption, sizeof(networks[*found_networks].encryption), "%d", ecn);
                (*found_networks)++;
            }
        }
        line = strtok(NULL, "\r\n");
    }
    ESP01_LOG_INFO("CWLAP", "Scan terminé : %d réseau(x) trouvé(s)", *found_networks);
    return ESP01_OK;
}

char *esp01_print_wifi_networks(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, NULL);
    esp01_network_t nets[ESP01_MAX_SCAN_NETWORKS];
    uint8_t found = 0;
    ESP01_Status_t st = esp01_scan_networks(nets, ESP01_MAX_SCAN_NETWORKS, &found);
    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "Scan WiFi : %s\n", esp01_get_error_string(st));
    for (uint8_t i = 0; i < found && pos < out_size - 1; i++)
    {
        pos += snprintf(out + pos, out_size - pos, "[%u] SSID:%s RSSI:%d ENC:%s\n",
                        i + 1, nets[i].ssid, nets[i].rssi, nets[i].encryption);
    }
    return out;
}

/* --- DHCP --- */
ESP01_Status_t esp01_set_dhcp(bool enable)
{
    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CWDHCP=1,%d", enable ? 1 : 0);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWDHCP", st);
    ESP01_LOG_INFO("CWDHCP", "DHCP %s", enable ? "activé" : "désactivé");
    return st;
}

ESP01_Status_t esp01_get_dhcp(bool *enabled)
{
    VALIDATE_PARAM(enabled, ESP01_INVALID_PARAM);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWDHCP?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWDHCP?", st);

    int32_t dhcp_mode = 0;
    if (esp01_parse_int_after(resp, "+CWDHCP:", &dhcp_mode) == ESP01_OK)
    {
        *enabled = (dhcp_mode & 0x01) ? true : false;
        ESP01_LOG_INFO("CWDHCP?", "DHCP lu : %s", *enabled ? "activé" : "désactivé");
        return ESP01_OK;
    }

    ESP01_LOG_WARN("CWDHCP?", "Motif non trouvé dans : %s", resp);
    ESP01_RETURN_ERROR("CWDHCP?", ESP01_FAIL);
}

/* --- Connexion/Déconnexion WiFi --- */
ESP01_Status_t esp01_disconnect_wifi(void)
{
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWQAP", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWQAP", st);
    ESP01_LOG_INFO("CWQAP", "Déconnexion WiFi réussie");
    return st;
}

ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
    VALIDATE_PARAM(ssid && password, ESP01_INVALID_PARAM);
    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_LONG);

    if (st == ESP01_OK)
    {
        ESP01_LOG_INFO("CWJAP", "Connexion réussie");
        return ESP01_OK;
    }
    if (strstr(resp, "ERROR"))
    {
        ESP01_LOG_ERROR("CWJAP", "Connexion refusée : %s", resp);
        ESP01_RETURN_ERROR("CWJAP", ESP01_FAIL);
    }
    ESP01_LOG_ERROR("CWJAP", "Timeout ou réponse inattendue");
    ESP01_RETURN_ERROR("CWJAP", ESP01_TIMEOUT);
}

/* --- IP, MAC, Hostname --- */
ESP01_Status_t esp01_get_current_ip(char *ip, size_t ip_size)
{
    VALIDATE_PARAM(ip && ip_size > 0, ESP01_INVALID_PARAM);
    char resp[ESP01_MAX_RESP_BUF] = {0};
    ESP01_Status_t status = esp01_send_at_with_resp("AT+CIFSR", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (status != ESP01_OK)
        ESP01_RETURN_ERROR("CIFSR", status);

    const char *patterns[] = {"+CIFSR:STAIP,\"", "STAIP,\"", "+CIFSR:APIP,\"", "APIP,\""};
    for (unsigned i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i)
    {
        char tmp[ESP01_MAX_IP_LEN] = {0};
        if (esp01_extract_quoted_value(resp, patterns[i], tmp, sizeof(tmp)))
        {
            if (esp01_check_buffer_size(strlen(tmp), ip_size - 1) != ESP01_OK)
                return ESP01_BUFFER_OVERFLOW;
            strncpy(ip, tmp, ip_size - 1);
            ip[ip_size - 1] = 0;
            ESP01_LOG_INFO("CIFSR", "Motif trouvé : %s, IP : %s", patterns[i], ip);
            return ESP01_OK;
        }
    }
    ESP01_LOG_WARN("CIFSR", "Aucun motif IP trouvé dans : %s", resp);
    ip[0] = 0;
    return ESP01_FAIL;
}

ESP01_Status_t esp01_get_ip_config(char *ip, size_t ip_len, char *gw, size_t gw_len, char *mask, size_t mask_len)
{
    VALIDATE_PARAM(ip && ip_len > 0 && gw && gw_len > 0 && mask && mask_len > 0, ESP01_INVALID_PARAM);
    char resp[ESP01_MAX_RESP_BUF] = {0};
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CIPSTA?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CIPSTA?", st);

    snprintf(ip, ip_len, "N/A");
    snprintf(gw, gw_len, "N/A");
    snprintf(mask, mask_len, "N/A");
    esp01_extract_quoted_value(resp, "+CIPSTA:ip:\"", ip, ip_len);
    esp01_extract_quoted_value(resp, "+CIPSTA:gateway:\"", gw, gw_len);
    esp01_extract_quoted_value(resp, "+CIPSTA:netmask:\"", mask, mask_len);
    ESP01_LOG_INFO("CIPSTA?", "IP: %s, GW: %s, MASK: %s", ip, gw, mask);
    return (ip[0] != 0 && strcmp(ip, "N/A") != 0) ? ESP01_OK : ESP01_FAIL;
}

ESP01_Status_t esp01_get_rssi(int *rssi)
{
    VALIDATE_PARAM(rssi, ESP01_INVALID_PARAM);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWJAP?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWJAP?", st);

    char *line = strstr(resp, "+CWJAP:");
    if (line)
    {
        int virgule = 0;
        while (*line && virgule < 3)
        {
            if (*line == ',')
                virgule++;
            line++;
        }
        while (*line == ' ')
            line++;
        int rssi_val = 0;
        if (sscanf(line, "%d", &rssi_val) == 1)
        {
            *rssi = rssi_val;
            ESP01_LOG_INFO("CWJAP?", "Motif trouvé : +CWJAP, RSSI : %d", *rssi);
            return ESP01_OK;
        }
    }
    ESP01_LOG_WARN("CWJAP?", "RSSI non trouvé dans : %s", resp);
    return ESP01_FAIL;
}

ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len)
{
    VALIDATE_PARAM(mac_buf && buf_len > 0, ESP01_INVALID_PARAM);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CIFSR", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CIFSR", st);

    char tmp[ESP01_MAX_MAC_LEN] = {0};
    if (esp01_extract_quoted_value(resp, "STAMAC,\"", tmp, sizeof(tmp)))
    {
        if (esp01_check_buffer_size(strlen(tmp), buf_len - 1) != ESP01_OK)
            return ESP01_BUFFER_OVERFLOW;
        strncpy(mac_buf, tmp, buf_len - 1);
        mac_buf[buf_len - 1] = 0;
        ESP01_LOG_INFO("CIFSR", "Motif trouvé : STAMAC, MAC : %s", mac_buf);
        return ESP01_OK;
    }
    ESP01_LOG_WARN("CIFSR", "MAC non trouvée dans : %s", resp);
    return ESP01_FAIL;
}

ESP01_Status_t esp01_set_hostname(const char *hostname)
{
    VALIDATE_PARAM(hostname, ESP01_INVALID_PARAM);
    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CWHOSTNAME=\"%s\"", hostname);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWHOSTNAME", st);
    ESP01_LOG_INFO("CWHOSTNAME", "Hostname défini : %s", hostname);
    return st;
}

/* --- Fonctions de copie de réponse génériques --- */
#define ESP01_COPY_RESP_FUNC(name, at_cmd)                                                                  \
    ESP01_Status_t name(char *out, size_t out_size)                                                         \
    {                                                                                                       \
        VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);                                           \
        char resp[ESP01_MAX_RESP_BUF] = {0};                                                                \
        ESP01_Status_t st = esp01_send_at_with_resp(at_cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); \
        if (st != ESP01_OK)                                                                                 \
            return st;                                                                                      \
        return esp01_copy_resp(out, out_size, resp);                                                        \
    }

ESP01_COPY_RESP_FUNC(esp01_get_wifi_connection, "AT+CWJAP?")
ESP01_COPY_RESP_FUNC(esp01_get_dhcp_status, "AT+CWDHCP?")
ESP01_COPY_RESP_FUNC(esp01_get_ip_info, "AT+CIPSTA?")
ESP01_COPY_RESP_FUNC(esp01_get_hostname, "AT+CWHOSTNAME?")
ESP01_COPY_RESP_FUNC(esp01_get_ap_config, "AT+CWSAP?")
ESP01_COPY_RESP_FUNC(esp01_get_wifi_state, "AT+CWSTATE?")

/* --- TCP/IP & Réseau --- */
ESP01_Status_t esp01_ping(const char *host)
{
    VALIDATE_PARAM(host, ESP01_INVALID_PARAM);
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"", host);
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", 5000, resp, sizeof(resp));
    ESP01_LOG_INFO("PING", "Réponse : %s", resp);
    return (st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

ESP01_Status_t esp01_open_connection(const char *type, const char *addr, int port)
{
    VALIDATE_PARAM(type && addr && port > 0, ESP01_INVALID_PARAM);
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"%s\",\"%s\",%d", type, addr, port);
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", 10000, resp, sizeof(resp));
    ESP01_LOG_INFO("CIPSTART", "Réponse : %s", resp);
    return (st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

ESP01_Status_t esp01_get_multiple_connections(bool *enabled)
{
    VALIDATE_PARAM(enabled, ESP01_INVALID_PARAM);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CIPMUX?", "OK", 1000, resp, sizeof(resp));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CIPMUX?", st);

    if (esp01_parse_bool_after(resp, "+CIPMUX:", enabled) == ESP01_OK)
    {
        ESP01_LOG_INFO("CIPMUX", "Multi-connexion : %s", *enabled ? "ON" : "OFF");
        return ESP01_OK;
    }
    ESP01_LOG_WARN("CIPMUX?", "Motif non trouvé dans : %s", resp);
    return ESP01_FAIL;
}

/* --- Configuration avancée --- */
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
    char cmd[ESP01_MAX_RESP_BUF];
    char resp[ESP01_MAX_RESP_BUF];

    ESP01_LOG_INFO("WIFI", "=== Début configuration WiFi ===");

    ESP01_LOG_INFO("WIFI", "Définition du mode WiFi...");
    status = esp01_set_wifi_mode(mode);
    ESP01_LOG_INFO("WIFI", "Set mode : %s", esp01_get_error_string(status));
    if (status != ESP01_OK)
    {
        ESP01_LOG_ERROR("WIFI", "Erreur : esp01_set_wifi_mode");
        return status;
    }
    HAL_Delay(300);

    if (mode == ESP01_WIFI_MODE_AP)
    {
        ESP01_LOG_INFO("WIFI", "Configuration du point d'accès (AP)...");
        snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",5,3", ssid, password);
        status = esp01_send_at_with_resp(cmd, "OK", 2000, resp, sizeof(resp));
        ESP01_LOG_INFO("WIFI", "Set AP : %s", esp01_get_error_string(status));
        if (status != ESP01_OK)
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Configuration AP");
            return status;
        }
        HAL_Delay(300);

        if (ip && strlen(ip) > 0)
        {
            ESP01_LOG_INFO("WIFI", "Configuration IP fixe AP...");
            snprintf(cmd, sizeof(cmd), "AT+CIPAP=\"%s\"", ip);
            status = esp01_send_at_with_resp(cmd, "OK", 2000, resp, sizeof(resp));
            ESP01_LOG_INFO("WIFI", "Set IP AP : %s", esp01_get_error_string(status));
            if (status != ESP01_OK)
            {
                ESP01_LOG_ERROR("WIFI", "Erreur : Configuration IP AP");
                return status;
            }
        }
    }

    if (use_dhcp)
    {
        if (mode == ESP01_WIFI_MODE_STA)
        {
            ESP01_LOG_INFO("WIFI", "Activation du DHCP STA...");
            status = esp01_send_at_with_resp("AT+CWDHCP=1,1", "OK", 2000, resp, sizeof(resp));
        }
        else if (mode == ESP01_WIFI_MODE_AP)
        {
            ESP01_LOG_INFO("WIFI", "Activation du DHCP AP...");
            status = esp01_send_at_with_resp("AT+CWDHCP=2,1", "OK", 2000, resp, sizeof(resp));
        }
        ESP01_LOG_INFO("WIFI", "Set DHCP : %s", esp01_get_error_string(status));
        if (status != ESP01_OK)
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Activation DHCP");
            return status;
        }
    }
    else if (ip && gateway && netmask && mode == ESP01_WIFI_MODE_STA)
    {
        ESP01_LOG_INFO("WIFI", "Déconnexion du WiFi (CWQAP)...");
        esp01_send_at_with_resp("AT+CWQAP", "OK", 2000, resp, sizeof(resp));

        ESP01_LOG_INFO("WIFI", "Désactivation du DHCP client...");
        status = esp01_send_at_with_resp("AT+CWDHCP=0,1", "OK", 2000, resp, sizeof(resp));
        ESP01_LOG_INFO("WIFI", "Set DHCP : %s", esp01_get_error_string(status));
        if (status != ESP01_OK)
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Désactivation DHCP");
            return status;
        }
        ESP01_LOG_INFO("WIFI", "Configuration IP statique...");
        snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gateway, netmask);
        status = esp01_send_at_with_resp(cmd, "OK", 2000, resp, sizeof(resp));
        ESP01_LOG_INFO("WIFI", "Set IP statique : %s", esp01_get_error_string(status));
        if (status != ESP01_OK)
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Configuration IP statique");
            return status;
        }
    }

    if (mode == ESP01_WIFI_MODE_STA)
    {
        ESP01_LOG_INFO("WIFI", "Connexion au réseau WiFi...");
        status = esp01_connect_wifi(ssid, password);
        ESP01_LOG_INFO("WIFI", "Connexion WiFi : %s", esp01_get_error_string(status));
        if (status != ESP01_OK)
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Connexion WiFi (CWJAP)");
            return status;
        }
        HAL_Delay(300);
    }

    ESP01_LOG_INFO("WIFI", "Activation de l'affichage IP client dans +IPD (AT+CIPDINFO=1)...");
    status = esp01_send_at_with_resp("AT+CIPDINFO=1", "OK", 2000, resp, sizeof(resp));
    ESP01_LOG_INFO("WIFI", "Set CIPDINFO : %s", esp01_get_error_string(status));
    if (status != ESP01_OK)
    {
        ESP01_LOG_ERROR("WIFI", "Erreur : AT+CIPDINFO=1");
        return status;
    }

    ESP01_LOG_INFO("WIFI", "=== Configuration WiFi terminée ===");
    return ESP01_OK;
}

ESP01_Status_t esp01_set_ip(const char *ip, const char *gw, const char *mask)
{
    VALIDATE_PARAM(ip, ESP01_INVALID_PARAM);
    char cmd[ESP01_MAX_CMD_BUF];
    int needed = 0;
    if (gw && mask)
        needed = snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gw, mask);
    else if (gw)
        needed = snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\"", ip, gw);
    else
        needed = snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\"", ip);

    if (esp01_check_buffer_size(needed, sizeof(cmd) - 1) != ESP01_OK)
        return ESP01_BUFFER_OVERFLOW;

    char resp[ESP01_MAX_RESP_BUF] = {0};
    return esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
}

/* --- Configuration Point d'Accès (AP) --- */
ESP01_Status_t esp01_start_ap_config(const char *ssid, const char *password, int channel, int encryption)
{
    char cmd[ESP01_MAX_CMD_BUF];
    char resp[ESP01_MAX_RESP_BUF] = {0};
    ESP01_Status_t status;

    VALIDATE_PARAM(ssid && ssid[0] != '\0' && strlen(ssid) <= ESP01_MAX_SSID_LEN, ESP01_INVALID_PARAM);
    if (encryption != 0)
        VALIDATE_PARAM(password && strlen(password) >= 8 && strlen(password) <= 64, ESP01_INVALID_PARAM);
    VALIDATE_PARAM(channel >= 1 && channel <= 13, ESP01_INVALID_PARAM);
    VALIDATE_PARAM(encryption == 0 || encryption == 2 || encryption == 3 || encryption == 4, ESP01_INVALID_PARAM);

    snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",%d,%d", ssid, password ? password : "", channel, encryption);

    status = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (status != ESP01_OK)
        ESP01_RETURN_ERROR("CWSAP", status);

    _esp_login(">>> [CWSAP] AP configuré : %s", ssid);
    return ESP01_OK;
}

/* ========================== FONCTIONS UTILITAIRES ========================== */

const char *esp01_encryption_to_string(const char *enc)
{
    if (!enc)
        return "INCONNU";
    if (strcmp(enc, "0") == 0)
        return "Open";
    if (strcmp(enc, "1") == 0)
        return "WEP";
    if (strcmp(enc, "2") == 0)
        return "WPA_PSK";
    if (strcmp(enc, "3") == 0)
        return "WPA2_PSK";
    if (strcmp(enc, "4") == 0)
        return "WPA_WPA2_PSK";
    return "INCONNU";
}

const char *esp01_tcp_status_to_string(const char *resp)
{
    static char buf[64];
    char *line = strstr(resp, "STATUS:");
    int code = -1;
    if (line && sscanf(line, "STATUS:%d", &code) == 1)
    {
        switch (code)
        {
        case 0:
            snprintf(buf, sizeof(buf), "IP initialisation (STATUS:0)");
            break;
        case 1:
            snprintf(buf, sizeof(buf), "Non connecté (STATUS:1)");
            break;
        case 2:
            snprintf(buf, sizeof(buf), "Connecté à un AP, pas de connexion TCP (STATUS:2)");
            break;
        case 3:
            snprintf(buf, sizeof(buf), "Connexion TCP ou UDP ouverte (STATUS:3)");
            break;
        case 4:
            snprintf(buf, sizeof(buf), "Connexion TCP ou UDP fermée (STATUS:4)");
            break;
        case 5:
            snprintf(buf, sizeof(buf), "En attente de fermeture (STATUS:5)");
            break;
        default:
            snprintf(buf, sizeof(buf), "Inconnu (STATUS:%d)", code);
            break;
        }
        return buf;
    }
    return "Réponse TCP/IP inattendue";
}

const char *esp01_cwstate_to_string(const char *resp)
{
    static char buf[64];
    char *line = strstr(resp, "+CWSTATE:");
    int code = -1;
    char ssid[ESP01_MAX_SSID_BUF] = {0};
    if (line)
    {
        if (sscanf(line, "+CWSTATE:%d,\"%[^\"]\"", &code, ssid) == 2)
            snprintf(buf, sizeof(buf), "Connecté (état %d) à SSID: %s", code, ssid);
        else if (sscanf(line, "+CWSTATE:%d", &code) == 1)
            snprintf(buf, sizeof(buf), "État WiFi : %d", code);
        else
            snprintf(buf, sizeof(buf), "Format CWSTATE inconnu");
        return buf;
    }
    return "Réponse CWSTATE inattendue";
}

const char *esp01_connection_status_to_string(const char *resp)
{
    static char buf[64];
    char *line = strstr(resp, "+CWJAP:");
    if (line)
    {
        char ssid[ESP01_MAX_SSID_BUF] = {0};
        if (sscanf(line, "+CWJAP:\"%[^\"]", ssid) == 1)
            snprintf(buf, sizeof(buf), "Connecté à %s", ssid);
        else
            snprintf(buf, sizeof(buf), "Connecté (SSID inconnu)");
        return buf;
    }
    return "Non connecté";
}

const char *esp01_rf_power_to_string(int rf_dbm)
{
    static char buf[ESP01_MAX_CMD_BUF];
    snprintf(buf, sizeof(buf), "%d dBm", rf_dbm);
    return buf;
}

const char *esp01_cwqap_to_string(const char *resp)
{
    if (strstr(resp, "WIFI DISCONNECT"))
        return "WIFI DISCONNECTED";
    if (strstr(resp, "OK"))
        return "OK";
    return "Déconnexion OK";
}

const char *esp01_wifi_connection_to_string(const char *resp)
{
    static char buf[ESP01_MAX_RESP_BUF];
    char ssid[ESP01_MAX_SSID_BUF] = {0};
    if (strstr(resp, "+CWJAP:"))
    {
        if (sscanf(resp, "+CWJAP:\"%[^\"]\"", ssid) == 1)
            snprintf(buf, sizeof(buf), "Connecté à : %s", ssid);
        else
            snprintf(buf, sizeof(buf), "Connecté (SSID inconnu)");
    }
    else if (strstr(resp, "No AP"))
    {
        snprintf(buf, sizeof(buf), "Non connecté à un AP");
    }
    else
    {
        snprintf(buf, sizeof(buf), "Statut WiFi inconnu");
    }
    return buf;
}

const char *esp01_dhcp_status_to_string(const char *resp)
{
    static char buf[ESP01_MAX_RESP_BUF];
    int mode = -1;
    if (sscanf(resp, "+CWDHCP:%d", &mode) == 1)
        snprintf(buf, sizeof(buf), "DHCP %s", (mode & 1) ? "activé" : "désactivé");
    else
        snprintf(buf, sizeof(buf), "Statut DHCP inconnu");
    return buf;
}

const char *esp01_ip_info_to_string(const char *resp)
{
    static char buf[ESP01_MAX_RESP_BUF];
    char ip[ESP01_SMALL_BUF_SIZE] = "N/A", gw[ESP01_SMALL_BUF_SIZE] = "N/A", mask[ESP01_SMALL_BUF_SIZE] = "N/A";
    esp01_extract_quoted_value(resp, "+CIPSTA:ip:\"", ip, sizeof(ip));
    esp01_extract_quoted_value(resp, "+CIPSTA:gateway:\"", gw, sizeof(gw));
    esp01_extract_quoted_value(resp, "+CIPSTA:netmask:\"", mask, sizeof(mask));
    snprintf(buf, sizeof(buf), "IP: %s, GW: %s, MASK: %s", ip, gw, mask);
    return buf;
}

const char *esp01_hostname_raw_to_string(const char *resp)
{
    static char buf[ESP01_MAX_RESP_BUF];
    char hostname[48] = "N/A";
    esp01_extract_quoted_value(resp, "+CWHOSTNAME:\"", hostname, sizeof(hostname));
    snprintf(buf, sizeof(buf), "Hostname: %s", hostname);
    return buf;
}

const char *esp01_ap_config_to_string(const char *resp)
{
    static char out[128];
    char ssid[33] = {0};
    char pwd[33] = {0};
    int channel = 0, enc = 0, maxconn = 0, hidden = 0;

    // Recherche du motif +CWSAP:"SSID","PWD",channel,enc,maxconn,hidden
    const char *start = strstr(resp, "+CWSAP:");
    if (start)
    {
        int n = sscanf(start, "+CWSAP:\"%32[^\"]\",\"%32[^\"]\",%d,%d,%d,%d",
                       ssid, pwd, &channel, &enc, &maxconn, &hidden);
        if (n >= 4)
        {
            snprintf(out, sizeof(out),
                     "SSID: %s, PWD: %s, CH: %d, Sécu: %s, MaxConn: %d, Hidden: %d",
                     ssid, pwd, channel,
                     (enc == 0) ? "Open" : (enc == 2) ? "WPA_PSK"
                                       : (enc == 3)   ? "WPA2_PSK"
                                       : (enc == 4)   ? "WPA_WPA2_PSK"
                                                      : "Inconnu",
                     maxconn, hidden);
            return out;
        }
    }
    return "Config AP inconnue";
}

/* --- dans la fonction de wrapper pour CWQAP... */
ESP01_Status_t esp01_disconnect_ap(void)
{
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWQAP", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));
    if (st == ESP01_OK)
    {
        ESP01_LOG_INFO("CWQAP", "Déconnexion AP réussie");
        return ESP01_OK;
    }

    if (strstr(resp, "ERROR"))
    {
        ESP01_LOG_ERROR("CWQAP", "Déconnexion refusée : %s", resp);
        ESP01_RETURN_ERROR("CWQAP", ESP01_FAIL);
    }

    ESP01_LOG_ERROR("CWQAP", "Timeout ou réponse inattendue");
    ESP01_RETURN_ERROR("CWQAP", ESP01_TIMEOUT);
}

const char *esp01_ping_result_to_string(const char *resp)
{
    static char result[ESP01_SMALL_BUF_SIZE];
    const char *line = strstr(resp, "+PING:");
    if (line)
    {
        int ms = 0;
        if (sscanf(line, "+PING:%d", &ms) == 1)
        {
            snprintf(result, sizeof(result), "%d ms", ms);
            return result;
        }
    }
    return "Ping échoué";
}

/* ========================== TESTS MODULE WIFI ========================== */

void test_wifi_module_STA(const char *ssid, const char *password)
{
    ESP01_Status_t status;
    char buf[128], ip[ESP01_MAX_IP_LEN], mac[ESP01_MAX_MAC_LEN], hostname[ESP01_MAX_HOSTNAME_LEN];
    int mode = 0, rssi = 0;
    bool dhcp = false;
    uint8_t found = 0;
    esp01_network_t networks[ESP01_MAX_SCAN_NETWORKS];
    char resp[ESP01_MAX_RESP_BUF];
    HAL_Delay(1000);

    // 1. Mode WiFi
    ESP01_LOG_INFO("WIFI", "\n=== [CWMODE] Configuration du mode WiFi ===");
    status = esp01_set_wifi_mode(ESP01_WIFI_MODE_STA);
    ESP01_LOG_INFO("WIFI", ">>> [CWMODE] Set : %s\r\n", esp01_get_error_string(status));
    HAL_Delay(500);
    ESP01_LOG_INFO("WIFI", "\n=== [CWMODE] Lecture du mode WiFi ===");
    status = esp01_get_wifi_mode(&mode);
    ESP01_LOG_INFO("WIFI", ">>> [CWMODE] Get : %s (%d)\r\n", esp01_wifi_mode_to_string(mode), mode);
    HAL_Delay(1000);

    // 2. DHCP
    ESP01_LOG_INFO("WIFI", "\n=== [CWDHCP] Configuration du DHCP ===");
    status = esp01_set_dhcp(true);
    ESP01_LOG_INFO("WIFI", ">>> [CWDHCP] Set : %s\r\n", esp01_get_error_string(status));
    HAL_Delay(500);

    ESP01_LOG_INFO("WIFI", "\n=== [CWDHCP] Lecture du DHCP ===");
    status = esp01_get_dhcp(&dhcp);
    ESP01_LOG_INFO("WIFI", ">>> [CWDHCP] Get : %s\r\n", dhcp ? "Activé" : "Désactivé");
    HAL_Delay(1000);

    // 3. Hostname
    ESP01_LOG_INFO("WIFI", "\n=== [CWHOSTNAME] Configuration du hostname ===");
    status = esp01_set_hostname("ESP-TEST");
    ESP01_LOG_INFO("WIFI", ">>> [CWHOSTNAME] Set : %s", esp01_get_error_string(status));
    HAL_Delay(500);

    ESP01_LOG_INFO("WIFI", "\n=== [CWHOSTNAME] Lecture du hostname ===");
    status = esp01_get_hostname(hostname, sizeof(hostname));
    ESP01_LOG_INFO("WIFI", ">>> [CWHOSTNAME] Get : %s", hostname);
    HAL_Delay(1000);

    // 4. Scan réseaux
    ESP01_LOG_INFO("WIFI", "\n=== [CWLAP] Scan des réseaux ===");
    status = esp01_scan_networks(networks, ESP01_MAX_SCAN_NETWORKS, &found);
    ESP01_LOG_INFO("WIFI", ">>> [CWLAP] Scan : %s (%d trouvés)", esp01_get_error_string(status), found);
    if (status == ESP01_OK)
    {
        for (uint8_t i = 0; i < found; ++i)
            ESP01_LOG_INFO("WIFI", "    SSID: %s, RSSI: %d, Sécu: %s", networks[i].ssid, networks[i].rssi, esp01_encryption_to_string(networks[i].encryption));
    }
    HAL_Delay(1000);

    // 5. Connexion WiFi
    ESP01_LOG_INFO("WIFI", "\n=== [CWJAP] Connexion WiFi ===");
    status = esp01_connect_wifi(ssid, password);
    ESP01_LOG_INFO("WIFI", ">>> [CWJAP] Connexion : %s\r\n", esp01_get_error_string(status));
    HAL_Delay(1000);

    status = esp01_get_wifi_connection(resp, sizeof(resp));
    ESP01_LOG_INFO("WIFI", ">>> [CWJAP?] Statut : %s", (status == ESP01_OK) ? esp01_connection_status_to_string(resp) : esp01_get_error_string(status));
    HAL_Delay(1000);

    // 5b. Etat de la connexion WiFi (CWSTATE)
    ESP01_LOG_INFO("WIFI", "\n=== [CWSTATE] Etat de la connexion WiFi ===");
    status = esp01_get_wifi_state(resp, sizeof(resp));
    ESP01_LOG_INFO("WIFI", ">>> [CWSTATE] Etat : %s", (status == ESP01_OK) ? esp01_cwstate_to_string(resp) : esp01_get_error_string(status));
    HAL_Delay(1000);

    // 6. Adresse IP
    ESP01_LOG_INFO("WIFI", "\n=== [CIFSR] Adresse IP ===");
    status = esp01_get_current_ip(ip, sizeof(ip));
    ESP01_LOG_INFO("WIFI", ">>> [CIFSR] IP : %s", (status == ESP01_OK) ? ip : esp01_get_error_string(status));
    HAL_Delay(1000);

    // 7. Adresse MAC
    ESP01_LOG_INFO("WIFI", "\n=== [CIFSR] Adresse MAC ===");
    status = esp01_get_mac(mac, sizeof(mac));
    ESP01_LOG_INFO("WIFI", ">>> [CIFSR] MAC : %s", (status == ESP01_OK) ? mac : esp01_get_error_string(status));
    HAL_Delay(1000);

    // 8. RSSI
    ESP01_LOG_INFO("WIFI", "\n=== [CWJAP?] RSSI ===");
    status = esp01_get_rssi(&rssi);
    if (status == ESP01_OK)
        ESP01_LOG_INFO("WIFI", ">>> [CWJAP?] RSSI : %s", esp01_rf_power_to_string(rssi));
    else
        ESP01_LOG_INFO("WIFI", ">>> [CWJAP?] RSSI : %s", esp01_get_error_string(status));
    HAL_Delay(1000);

    // 9. Statut TCP/IP
    ESP01_LOG_INFO("WIFI", "\n=== [CIPSTATUS] Statut TCP/IP ===");
    status = esp01_get_tcp_status(buf, sizeof(buf));
    ESP01_LOG_INFO("WIFI", ">>> [CIPSTATUS] : %s", (status == ESP01_OK) ? esp01_tcp_status_to_string(buf) : esp01_get_error_string(status));
    HAL_Delay(1000);

    // 10. Ping
    ESP01_LOG_INFO("WIFI", "\n=== [PING] Test du ping ===");
    char ping_resp[ESP01_MAX_RESP_BUF] = {0};
    snprintf(buf, sizeof(buf), "AT+PING=\"8.8.8.8\"");
    status = esp01_send_raw_command_dma(buf, ping_resp, sizeof(ping_resp), "OK", 5000);
    ESP01_LOG_INFO("WIFI", ">>> [PING] : %s", esp01_ping_result_to_string(ping_resp));
    HAL_Delay(1000);

    // 11. Déconnexion WiFi
    ESP01_LOG_INFO("WIFI", "\n=== [CWQAP] Déconnexion WiFi ===");
    memset(resp, 0, sizeof(resp)); // Vide le buffer si besoin
    status = esp01_disconnect_wifi();
    ESP01_LOG_INFO("WIFI", ">>> [CWQAP] : %s", (status == ESP01_OK) ? esp01_cwqap_to_string(resp) : esp01_get_error_string(status));
    HAL_Delay(1000);
}

void test_wifi_module_AP(const char *ssid, const char *password)
{
    char resp[ESP01_MAX_RESP_BUF] = {0};
    ESP01_Status_t status;

    // === [CWMODE] Configuration du mode AP ===
    ESP01_LOG_INFO("WIFI", "\n=== [CWMODE] Configuration du mode WiFi (AP) ===");
    status = esp01_set_wifi_mode(2); // 2 = AP
    ESP01_LOG_INFO("WIFI", ">>> [CWMODE] Set : %s", esp01_get_error_string(status));

    // === [CWSAP] Configuration AP ===
    ESP01_LOG_INFO("WIFI", "\n=== [CWSAP] Configuration AP ===");
    status = esp01_start_ap_config(ssid, password, 1, 3);
    ESP01_LOG_INFO("WIFI", ">>> [CWSAP] Set : %s", esp01_get_error_string(status));

    // === [CWSAP?] Lecture de la config AP ===
    ESP01_LOG_INFO("WIFI", "\n=== [CWSAP?] Lecture de la config AP ===");
    status = esp01_get_ap_config(resp, sizeof(resp));
    ESP01_LOG_INFO("WIFI", ">>> [CWSAP?] : %s", (status == ESP01_OK) ? esp01_ap_config_to_string(resp) : esp01_get_error_string(status));
    HAL_Delay(10000);

    ESP01_LOG_INFO("WIFI", "\n=== [AP TEST] Fin du test AP ===");
}
