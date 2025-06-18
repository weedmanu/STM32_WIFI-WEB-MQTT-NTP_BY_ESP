/**
 ******************************************************************************
 * @file    STM32_WifiESP_WIFI.c
 * @author  manu
 * @version 1.2.0
 * @date    13 juin 2025
 * @brief   Implémentation des fonctions haut niveau WiFi pour ESP01
 *
 * @details
 * Ce fichier source contient l’implémentation des fonctions haut niveau WiFi :
 * - Modes WiFi (STA, AP, STA+AP)
 * - Scan des réseaux, connexion, déconnexion
 * - DHCP, IP, MAC, hostname, clients AP
 * - TCP/IP, ping, statut, multi-connexion
 * - Fonctions utilitaires d'affichage et de parsing
 *
 * @note
 * - Nécessite le driver bas niveau STM32_WifiESP.h
 ******************************************************************************
 */

#include "STM32_WifiESP_WIFI.h" // Header du module WiFi haut niveau
#include <string.h>             // Pour manipulation de chaînes
#include <stdio.h>              // Pour snprintf, sscanf, etc.

/* ========================== OUTILS FACTORISÉS ========================== */

/**
 * @brief  Copie une réponse dans un buffer utilisateur en protégeant contre le débordement.
 * @param  dst      Buffer de destination.
 * @param  dst_size Taille du buffer de destination.
 * @param  src      Chaîne source à copier.
 * @retval ESP01_Status_t
 */
static ESP01_Status_t esp01_copy_resp(char *dst, size_t dst_size, const char *src)
{
    if (!dst || !src || dst_size == 0) // Vérifie les paramètres d'entrée
        return ESP01_INVALID_PARAM;
    size_t len = strlen(src); // Longueur de la chaîne source
    if (len >= dst_size)      // Vérifie le débordement
        return ESP01_BUFFER_OVERFLOW;
    strncpy(dst, src, dst_size - 1); // Copie la chaîne source dans le buffer de destination
    dst[dst_size - 1] = 0;           // Termine la chaîne
    return ESP01_OK;
}

/**
 * @brief  Envoie une commande AT et gère le buffer de réponse.
 * @param  cmd        Commande AT à envoyer.
 * @param  expected   Motif attendu dans la réponse.
 * @param  timeout_ms Timeout en ms.
 * @param  resp       Buffer de réponse.
 * @param  resp_size  Taille du buffer de réponse.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_send_at_with_resp(const char *cmd, const char *expected, int timeout_ms, char *resp, size_t resp_size)
{
    if (!cmd || !expected) // Vérifie les paramètres d'entrée
        return ESP01_INVALID_PARAM;
    return esp01_send_raw_command_dma(cmd, resp, resp_size, expected, timeout_ms); // Utilise la fonction bas niveau
}

/* ========================== FONCTIONS PRINCIPALES ========================== */

/* --- Modes WiFi --- */
ESP01_Status_t esp01_get_connection_status(void)
{
    char resp[ESP01_MAX_RESP_BUF];                                                                           // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWJAP?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande AT+CWJAP?
    ESP01_LOG_DEBUG("STATUS", "Réponse : %s", resp);                                                         // Log la réponse

    if (st != ESP01_OK)                           // Si la commande a échoué
        ESP01_RETURN_ERROR("STATUS", ESP01_FAIL); // Retourne une erreur

    if (strstr(resp, "+CWJAP:")) // Si le motif "+CWJAP:" est trouvé, on est connecté
    {
        ESP01_LOG_INFO("STATUS", "Motif trouvé : +CWJAP (connecté)"); // Log info connexion
        return ESP01_OK;                                              // Retourne OK
    }
    ESP01_LOG_WARN("STATUS", "Motif non trouvé : non connecté"); // Log warning non connecté
    return ESP01_WIFI_NOT_CONNECTED;                             // Retourne non connecté
}

ESP01_Status_t esp01_get_wifi_mode(int *mode)
{
    VALIDATE_PARAM(mode, ESP01_INVALID_PARAM);                                                                // Vérifie le pointeur de sortie
    char resp[ESP01_MAX_RESP_BUF];                                                                            // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWMODE?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande AT+CWMODE?
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWMODE", st);

    char *line = strstr(resp, "+CWMODE:"); // Cherche le motif dans la réponse
    if (line)
    {
        int val = 0;
        if (sscanf(line, "+CWMODE:%d", &val) == 1) // Extrait la valeur du mode
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
    if (mode < ESP01_WIFI_MODE_STA || mode > ESP01_WIFI_MODE_STA_AP) // Vérifie la validité du mode
        return ESP01_INVALID_PARAM;
    char cmd[ESP01_MAX_CMD_BUF];                                                                     // Buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);                                                // Prépare la commande
    char resp[ESP01_MAX_RESP_BUF];                                                                   // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
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
        return "STA"; // Mode station
    case ESP01_WIFI_MODE_AP:
        return "AP"; // Mode point d'accès
    case ESP01_WIFI_MODE_STA_AP:
        return "STA+AP"; // Mode mixte
    default:
        return "INCONNU"; // Mode inconnu
    }
}

ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size)
{
    ESP01_LOG_INFO("CIPSTATUS", "=== Lecture statut TCP ===");
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);                                       // Vérifie les paramètres
    char resp[ESP01_MAX_RESP_BUF] = {0};                                                            // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTATUS", resp, sizeof(resp), "OK", 2000); // Envoie la commande
    ESP01_Status_t copy_st = esp01_safe_strcpy(out, out_size, resp);                                // Copie la réponse dans le buffer utilisateur
    ESP01_LOG_INFO("CIPSTATUS", ">>> Réponse :\n%s", resp);
    return (st == ESP01_OK && copy_st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

/* --- Scan WiFi --- */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks)
{
    VALIDATE_PARAM(networks && found_networks && max_networks > 0, ESP01_INVALID_PARAM);                   // Vérifie les paramètres
    char resp[ESP01_LARGE_RESP_BUF];                                                                       // Buffer pour la réponse AT
    *found_networks = 0;                                                                                   // Initialise le compteur de réseaux trouvés
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWLAP", "OK", ESP01_TIMEOUT_LONG, resp, sizeof(resp)); // Scan réseaux
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWLAP", st); // Retourne une erreur si le scan échoue

    char *line = strtok(resp, "\r\n");             // Découpe la réponse en lignes
    while (line && *found_networks < max_networks) // Parcourt chaque ligne tant qu'il reste de la place
    {
        if (strncmp(line, "+CWLAP:", 7) == 0) // Si la ligne commence par +CWLAP:
        {
            int ecn = 0, rssi = 0, channel = 0;                                                                     // Variables temporaires pour le parsing
            char ssid[ESP01_MAX_SSID_BUF] = {0};                                                                    // Buffer pour le SSID
            char mac[ESP01_MAX_MAC_LEN] = {0};                                                                      // Buffer pour la MAC
            if (sscanf(line, "+CWLAP:(%d,\"%32[^\"]\",%d,\"%17[^\"]\",%d)", &ecn, ssid, &rssi, mac, &channel) >= 4) // Parse la ligne
            {
                size_t ssid_len = strlen(ssid); // Longueur du SSID
                if (esp01_check_buffer_size(ssid_len, sizeof(networks[*found_networks].ssid) - 1) != ESP01_OK)
                    continue;                                                                                            // Ignore si le buffer est trop petit
                esp01_safe_strcpy(networks[*found_networks].ssid, ESP01_MAX_SSID_BUF, ssid);                             // Copie le SSID
                networks[*found_networks].ssid[ESP01_MAX_SSID_LEN] = 0;                                                  // Termine la chaîne
                networks[*found_networks].rssi = rssi;                                                                   // Stocke le RSSI
                snprintf(networks[*found_networks].encryption, sizeof(networks[*found_networks].encryption), "%d", ecn); // Stocke l'encryptage
                (*found_networks)++;                                                                                     // Incrémente le compteur
            }
        }
        line = strtok(NULL, "\r\n"); // Passe à la ligne suivante
    }
    ESP01_LOG_INFO("CWLAP", "Scan terminé : %d réseau(x) trouvé(s)", *found_networks); // Log le résultat du scan
    return ESP01_OK;                                                                   // Retourne OK
}

char *esp01_print_wifi_networks(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, NULL);                                                  // Vérifie les paramètres
    esp01_network_t nets[ESP01_MAX_SCAN_NETWORKS];                                              // Tableau pour les réseaux trouvés
    uint8_t found = 0;                                                                          // Nombre de réseaux trouvés
    ESP01_Status_t st = esp01_scan_networks(nets, ESP01_MAX_SCAN_NETWORKS, &found);             // Scan réseaux
    size_t pos = 0;                                                                             // Position d'écriture dans le buffer
    pos += snprintf(out + pos, out_size - pos, "Scan WiFi : %s\n", esp01_get_error_string(st)); // Ajoute le statut du scan
    for (uint8_t i = 0; i < found && pos < out_size - 1; i++)                                   // Parcourt les réseaux trouvés
    {
        int written = snprintf(out + pos, out_size - pos, "[%u] SSID:%s RSSI:%d ENC:%s\n",
                               i + 1, nets[i].ssid, nets[i].rssi, nets[i].encryption); // Ajoute chaque réseau
        if (esp01_check_buffer_size(written, out_size - pos - 1) != ESP01_OK)
            break;      // Arrête si le buffer est plein
        pos += written; // Avance la position d'écriture
    }
    return out; // Retourne le buffer rempli
}

/* --- DHCP --- */
ESP01_Status_t esp01_set_dhcp(bool enable)
{
    char cmd[ESP01_MAX_CMD_BUF];                                                                     // Buffer pour la commande
    snprintf(cmd, sizeof(cmd), "AT+CWDHCP=1,%d", enable ? 1 : 0);                                    // Prépare la commande
    char resp[ESP01_MAX_RESP_BUF];                                                                   // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWDHCP", st);
    ESP01_LOG_INFO("CWDHCP", "DHCP %s", enable ? "activé" : "désactivé");
    return st;
}

ESP01_Status_t esp01_get_dhcp(bool *enabled)
{
    VALIDATE_PARAM(enabled, ESP01_INVALID_PARAM);                                                             // Vérifie le pointeur de sortie
    char resp[ESP01_MAX_RESP_BUF];                                                                            // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWDHCP?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWDHCP?", st);

    int32_t dhcp_mode = 0;
    if (esp01_parse_int_after(resp, "+CWDHCP:", &dhcp_mode) == ESP01_OK) // Extrait le mode DHCP
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
    char resp[ESP01_MAX_RESP_BUF];                                                                          // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWQAP", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWQAP", st);
    ESP01_LOG_INFO("CWQAP", "Déconnexion WiFi réussie");
    return st;
}

ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
    VALIDATE_PARAM(ssid && password, ESP01_INVALID_PARAM);                                             // Vérifie les paramètres
    char cmd[ESP01_MAX_CMD_BUF];                                                                       // Buffer pour la commande
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);                              // Prépare la commande
    char resp[ESP01_MAX_RESP_BUF];                                                                     // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_LONG); // Envoie la commande

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
    VALIDATE_PARAM(ip && ip_size > 0, ESP01_INVALID_PARAM);                                                     // Vérifie les paramètres
    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                        // Buffer pour la réponse
    ESP01_Status_t status = esp01_send_at_with_resp("AT+CIFSR", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
    if (status != ESP01_OK)
        ESP01_RETURN_ERROR("CIFSR", status);

    const char *patterns[] = {"+CIFSR:STAIP,\"", "STAIP,\"", "+CIFSR:APIP,\"", "APIP,\""};
    for (unsigned i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i)
    {
        char tmp[ESP01_MAX_IP_LEN] = {0};
        if (esp01_extract_quoted_value(resp, patterns[i], tmp, sizeof(tmp))) // Extrait l'IP
        {
            return esp01_safe_strcpy(ip, ip_size, tmp);
        }
    }
    ESP01_LOG_WARN("CIFSR", "Aucun motif IP trouvé dans : %s", resp);
    ip[0] = 0;
    return ESP01_FAIL;
}

ESP01_Status_t esp01_get_ip_config(char *ip, size_t ip_len, char *gw, size_t gw_len, char *mask, size_t mask_len)
{
    VALIDATE_PARAM(ip && ip_len > 0 && gw && gw_len > 0 && mask && mask_len > 0, ESP01_INVALID_PARAM);        // Vérifie les paramètres
    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                      // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CIPSTA?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CIPSTA?", st);

    snprintf(ip, ip_len, "N/A");
    snprintf(gw, gw_len, "N/A");
    snprintf(mask, mask_len, "N/A");
    esp01_extract_quoted_value(resp, "+CIPSTA:ip:\"", ip, ip_len);          // Extrait l'IP
    esp01_extract_quoted_value(resp, "+CIPSTA:gateway:\"", gw, gw_len);     // Extrait la gateway
    esp01_extract_quoted_value(resp, "+CIPSTA:netmask:\"", mask, mask_len); // Extrait le masque
    ESP01_LOG_INFO("CIPSTA?", "IP: %s, GW: %s, MASK: %s", ip, gw, mask);
    return (ip[0] != 0 && strcmp(ip, "N/A") != 0) ? ESP01_OK : ESP01_FAIL;
}

ESP01_Status_t esp01_get_rssi(int *rssi)
{
    VALIDATE_PARAM(rssi, ESP01_INVALID_PARAM);                                                               // Vérifie le pointeur de sortie
    char resp[ESP01_MAX_RESP_BUF];                                                                           // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CWJAP?", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CWJAP?", st); // Retourne une erreur si la commande échoue

    char *line = strstr(resp, "+CWJAP:"); // Cherche le motif dans la réponse
    if (line)
    {
        int virgule = 0;             // Compteur de virgules pour trouver la bonne position
        while (*line && virgule < 3) // Avance jusqu'à la 3ème virgule
        {
            if (*line == ',')
                virgule++;
            line++;
        }
        while (*line == ' ')
            line++;                             // Ignore les espaces
        int rssi_val = 0;                       // Variable temporaire pour le RSSI
        if (sscanf(line, "%d", &rssi_val) == 1) // Extrait le RSSI
        {
            *rssi = rssi_val;                                                    // Stocke le RSSI dans la variable de sortie
            ESP01_LOG_INFO("CWJAP?", "Motif trouvé : +CWJAP, RSSI : %d", *rssi); // Log le RSSI
            return ESP01_OK;                                                     // Retourne OK
        }
    }
    ESP01_LOG_WARN("CWJAP?", "RSSI non trouvé dans : %s", resp); // Log si le RSSI n'est pas trouvé
    return ESP01_FAIL;                                           // Retourne une erreur
}

ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len)
{
    VALIDATE_PARAM(mac_buf && buf_len > 0, ESP01_INVALID_PARAM);                                            // Vérifie les paramètres
    char resp[ESP01_MAX_RESP_BUF];                                                                          // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CIFSR", "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CIFSR", st); // Retourne une erreur si la commande échoue

    char tmp[ESP01_MAX_MAC_LEN] = {0};                                   // Buffer temporaire pour la MAC
    if (esp01_extract_quoted_value(resp, "STAMAC,\"", tmp, sizeof(tmp))) // Extrait la MAC
    {
        if (esp01_check_buffer_size(strlen(tmp), buf_len - 1) != ESP01_OK)
            return ESP01_BUFFER_OVERFLOW;                                    // Vérifie la taille du buffer
        strncpy(mac_buf, tmp, buf_len - 1);                                  // Copie la MAC dans le buffer utilisateur
        mac_buf[buf_len - 1] = 0;                                            // Termine la chaîne
        ESP01_LOG_INFO("CIFSR", "Motif trouvé : STAMAC, MAC : %s", mac_buf); // Log la MAC
        return ESP01_OK;                                                     // Retourne OK
    }
    ESP01_LOG_WARN("CIFSR", "MAC non trouvée dans : %s", resp); // Log si la MAC n'est pas trouvée
    return ESP01_FAIL;                                          // Retourne une erreur
}

ESP01_Status_t esp01_set_hostname(const char *hostname)
{
    VALIDATE_PARAM(hostname, ESP01_INVALID_PARAM);                                                   // Vérifie le paramètre
    char cmd[ESP01_MAX_CMD_BUF];                                                                     // Buffer pour la commande
    snprintf(cmd, sizeof(cmd), "AT+CWHOSTNAME=\"%s\"", hostname);                                    // Prépare la commande
    char resp[ESP01_MAX_RESP_BUF];                                                                   // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
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

ESP01_COPY_RESP_FUNC(esp01_get_wifi_connection, "AT+CWJAP?") // Récupère le statut de connexion WiFi
ESP01_COPY_RESP_FUNC(esp01_get_dhcp_status, "AT+CWDHCP?")    // Récupère le statut DHCP
ESP01_COPY_RESP_FUNC(esp01_get_ip_info, "AT+CIPSTA?")        // Récupère les infos IP
ESP01_COPY_RESP_FUNC(esp01_get_hostname, "AT+CWHOSTNAME?")   // Récupère le hostname
ESP01_COPY_RESP_FUNC(esp01_get_ap_config, "AT+CWSAP?")       // Récupère la configuration du point d'accès
ESP01_COPY_RESP_FUNC(esp01_get_wifi_state, "AT+CWSTATE?")    // Récupère l'état WiFi

/* --- TCP/IP & Réseau --- */
ESP01_Status_t esp01_ping(const char *host)
{
    VALIDATE_PARAM(host, ESP01_INVALID_PARAM);                                        // Vérifie le paramètre
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];                            // Buffers pour la commande et la réponse
    snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"", host);                               // Prépare la commande
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", 5000, resp, sizeof(resp)); // Envoie la commande
    ESP01_LOG_INFO("PING", "Réponse : %s", resp);
    return (st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

ESP01_Status_t esp01_open_connection(const char *type, const char *addr, int port)
{
    VALIDATE_PARAM(type && addr && port > 0, ESP01_INVALID_PARAM);                     // Vérifie les paramètres
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];                             // Buffers pour la commande et la réponse
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"%s\",\"%s\",%d", type, addr, port);      // Prépare la commande
    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", 10000, resp, sizeof(resp)); // Envoie la commande
    ESP01_LOG_INFO("CIPSTART", "Réponse : %s", resp);
    return (st == ESP01_OK) ? ESP01_OK : ESP01_FAIL;
}

ESP01_Status_t esp01_get_multiple_connections(bool *enabled)
{
    VALIDATE_PARAM(enabled, ESP01_INVALID_PARAM);                                              // Vérifie le pointeur de sortie
    char resp[ESP01_MAX_RESP_BUF];                                                             // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_at_with_resp("AT+CIPMUX?", "OK", 1000, resp, sizeof(resp)); // Envoie la commande AT+CIPMUX?
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("CIPMUX?", st); // Retourne une erreur si la commande échoue

    if (esp01_parse_bool_after(resp, "+CIPMUX:", enabled) == ESP01_OK) // Extrait la valeur booléenne après le motif
    {
        ESP01_LOG_INFO("CIPMUX", "Multi-connexion : %s", *enabled ? "ON" : "OFF"); // Log l'état multi-connexion
        return ESP01_OK;                                                           // Retourne OK si trouvé
    }
    ESP01_LOG_WARN("CIPMUX?", "Motif non trouvé dans : %s", resp); // Log si le motif n'est pas trouvé
    return ESP01_FAIL;                                             // Retourne une erreur
}

ESP01_Status_t esp01_set_ip(const char *ip, const char *gw, const char *mask)
{
    VALIDATE_PARAM(ip, ESP01_INVALID_PARAM); // Vérifie le paramètre IP
    char cmd[ESP01_MAX_CMD_BUF];             // Buffer pour la commande AT
    int needed = 0;                          // Variable pour la taille de la commande
    if (gw && mask)
        needed = snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gw, mask); // Commande avec IP, GW, masque
    else if (gw)
        needed = snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\"", ip, gw); // Commande avec IP, GW
    else
        needed = snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\"", ip); // Commande avec IP seule

    if (esp01_check_buffer_size(needed, sizeof(cmd) - 1) != ESP01_OK)
        return ESP01_BUFFER_OVERFLOW; // Vérifie la taille du buffer

    char resp[ESP01_MAX_RESP_BUF] = {0};                                                // Buffer pour la réponse
    return esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande et retourne le statut
}

ESP01_Status_t esp01_start_ap_config(const char *ssid, const char *password, int channel, int encryption)
{
    char cmd[ESP01_MAX_CMD_BUF];         // Buffer pour la commande AT
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse
    ESP01_Status_t status;               // Variable pour le statut

    VALIDATE_PARAM(ssid && ssid[0] != '\0' && strlen(ssid) <= ESP01_MAX_SSID_LEN, ESP01_INVALID_PARAM); // Vérifie le SSID
    if (encryption != 0)
        VALIDATE_PARAM(password && strlen(password) >= 8 && strlen(password) <= 64, ESP01_INVALID_PARAM);          // Vérifie le mot de passe si crypté
    VALIDATE_PARAM(channel >= 1 && channel <= 13, ESP01_INVALID_PARAM);                                            // Vérifie le canal
    VALIDATE_PARAM(encryption == 0 || encryption == 2 || encryption == 3 || encryption == 4, ESP01_INVALID_PARAM); // Vérifie l'encryptage

    snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",%d,%d", ssid, password ? password : "", channel, encryption); // Prépare la commande

    status = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp)); // Envoie la commande
    if (status != ESP01_OK)
        ESP01_RETURN_ERROR("CWSAP", status); // Retourne une erreur si la commande échoue

    _esp_login(">>> [CWSAP] AP configuré : %s", ssid); // Log la configuration AP
    return ESP01_OK;                                   // Retourne OK
}

/**
 * @brief  Configure le WiFi en mode AP ou STA avec les paramètres fournis.
 * @param  mode      Mode WiFi (STA, AP, STA+AP).
 * @param  ssid      SSID du réseau WiFi.
 * @param  password  Mot de passe du réseau WiFi (peut être NULL pour AP sans mot de passe).
 * @param  use_dhcp  Indique si le DHCP doit être utilisé.
 * @param  ip        IP statique (peut être NULL si DHCP est utilisé).
 * @param  gateway   Gateway (peut être NULL si DHCP est utilisé).
 * @param  netmask   Masque de sous-réseau (peut être NULL si DHCP est utilisé).
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_connect_wifi_config(
    ESP01_WifiMode_t mode, // Mode WiFi (STA, AP, STA+AP)
    const char *ssid,      // SSID du réseau WiFi
    const char *password,  // Mot de passe du réseau WiFi (peut être NULL pour AP sans mot de passe)
    bool use_dhcp,         // Indique si le DHCP doit être utilisé
    const char *ip,        // IP statique (peut être NULL si DHCP est utilisé)
    const char *gateway,   // Gateway (peut être NULL si DHCP est utilisé)
    const char *netmask)   // Masque de sous-réseau (peut être NULL si DHCP est utilisé)
{
    ESP01_Status_t status;         // Variable pour le statut de la commande
    char cmd[ESP01_MAX_RESP_BUF];  // Buffer pour la commande AT
    char resp[ESP01_MAX_RESP_BUF]; // Buffer pour la réponse AT

    ESP01_LOG_INFO("WIFI", "=== Début configuration WiFi ==="); // Log le début de la configuration WiFi

    ESP01_LOG_INFO("WIFI", "Définition du mode WiFi...");                    // Log la définition du mode WiFi
    status = esp01_set_wifi_mode(mode);                                      // Définit le mode WiFi
    ESP01_LOG_INFO("WIFI", "Set mode : %s", esp01_get_error_string(status)); // Log le statut de la définition du mode
    if (status != ESP01_OK)                                                  // Si la définition du mode échoue
    {
        ESP01_LOG_ERROR("WIFI", "Erreur : esp01_set_wifi_mode"); // Log l'erreur
        return status;                                           // Retourne le statut d'erreur
    }
    HAL_Delay(300); // Attente pour stabiliser le module

    if (mode == ESP01_WIFI_MODE_AP) // Si le mode est AP (point d'accès)
    {
        ESP01_LOG_INFO("WIFI", "Configuration du point d'accès (AP)...");
        snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",5,3", ssid, password); // Prépare la commande pour configurer l'AP
        status = esp01_send_at_with_resp(cmd, "OK", 2000, resp, sizeof(resp));    // Envoie la commande
        ESP01_LOG_INFO("WIFI", "Set AP : %s", esp01_get_error_string(status));    // Log le statut de la commande
        if (status != ESP01_OK)                                                   // Si la configuration de l'AP échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Configuration AP"); // Log l'erreur
            return status;                                        // Retourne le statut d'erreur
        }
        HAL_Delay(300); // Attente pour stabiliser le module

        if (ip && strlen(ip) > 0) // Si une IP fixe est fournie pour l'AP
        {
            ESP01_LOG_INFO("WIFI", "Configuration IP fixe AP...");                    // Log la configuration IP fixe
            snprintf(cmd, sizeof(cmd), "AT+CIPAP=\"%s\"", ip);                        // Prépare la commande pour configurer l'IP de l'AP
            status = esp01_send_at_with_resp(cmd, "OK", 2000, resp, sizeof(resp));    // Envoie la commande
            ESP01_LOG_INFO("WIFI", "Set IP AP : %s", esp01_get_error_string(status)); // Log le statut de la configuration IP
            if (status != ESP01_OK)                                                   // Si la configuration IP de l'AP échoue
            {
                ESP01_LOG_ERROR("WIFI", "Erreur : Configuration IP AP"); // Log l'erreur
                return status;                                           // Retourne le statut d'erreur
            }
        }
    }

    if (use_dhcp) // Si le DHCP doit être utilisé
    {
        if (mode == ESP01_WIFI_MODE_STA) // Si le mode est STA (station)
        {
            ESP01_LOG_INFO("WIFI", "Activation du DHCP client...");                            // Log l'activation du DHCP client
            status = esp01_send_at_with_resp("AT+CWDHCP=1,1", "OK", 2000, resp, sizeof(resp)); // Active le DHCP client
        }
        else if (mode == ESP01_WIFI_MODE_STA_AP) // Si le mode est STA+AP
        {
            ESP01_LOG_INFO("WIFI", "Activation du DHCP STA...");                               // Log l'activation du DHCP STA
            status = esp01_send_at_with_resp("AT+CWDHCP=1,1", "OK", 2000, resp, sizeof(resp)); // Active le DHCP pour la station
        }
        else if (mode == ESP01_WIFI_MODE_AP) // Si le mode est AP (point d'accès)
        {
            ESP01_LOG_INFO("WIFI", "Activation du DHCP AP...");                                // Log l'activation du DHCP pour l'AP
            status = esp01_send_at_with_resp("AT+CWDHCP=2,1", "OK", 2000, resp, sizeof(resp)); // Active le DHCP pour l'AP
        }
        ESP01_LOG_INFO("WIFI", "Set DHCP : %s", esp01_get_error_string(status)); // Log le statut de l'activation du DHCP
        if (status != ESP01_OK)                                                  // Si l'activation du DHCP échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Activation DHCP"); // Log l'erreur
            return status;                                       // Retourne le statut d'erreur
        }
    }
    else if (ip && gateway && netmask && mode == ESP01_WIFI_MODE_STA) // Si une IP statique est fournie et le mode est STA
    {
        ESP01_LOG_INFO("WIFI", "Déconnexion du WiFi (CWQAP)...");            // Log la déconnexion du WiFi
        esp01_send_at_with_resp("AT+CWQAP", "OK", 2000, resp, sizeof(resp)); // Déconnecte le WiFi

        ESP01_LOG_INFO("WIFI", "Désactivation du DHCP client...");                         // Log la désactivation du DHCP client
        status = esp01_send_at_with_resp("AT+CWDHCP=0,1", "OK", 2000, resp, sizeof(resp)); // Désactive le DHCP client
        ESP01_LOG_INFO("WIFI", "Set DHCP : %s", esp01_get_error_string(status));           // Log le statut de la désactivation du DHCP
        if (status != ESP01_OK)                                                            // Si la désactivation du DHCP échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Désactivation DHCP"); // Log l'erreur
            return status;                                          // Retourne le statut d'erreur
        }
        ESP01_LOG_INFO("WIFI", "Configuration IP statique...");                             // Log la configuration de l'IP statique
        snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gateway, netmask); // Prépare la commande pour configurer l'IP statique
        status = esp01_send_at_with_resp(cmd, "OK", 2000, resp, sizeof(resp));              // Envoie la commande
        ESP01_LOG_INFO("WIFI", "Set IP statique : %s", esp01_get_error_string(status));     // Log le statut de la configuration IP statique
        if (status != ESP01_OK)                                                             // Si la configuration de l'IP statique échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Configuration IP statique"); // Log l'erreur
            return status;                                                 // Retourne le statut d'erreur
        }
    }

    if (mode == ESP01_WIFI_MODE_STA) // Si le mode est STA (station)
    {
        ESP01_LOG_INFO("WIFI", "Connexion au réseau WiFi...");                         // Log la connexion au réseau WiFi
        status = esp01_connect_wifi(ssid, password);                                   // Tente de se connecter au réseau WiFi
        ESP01_LOG_INFO("WIFI", "Connexion WiFi : %s", esp01_get_error_string(status)); // Log le statut de la connexion
        if (status != ESP01_OK)                                                        // Si la connexion échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Connexion WiFi (CWJAP)"); // Log l'erreur
            return status;                                              // Retourne le statut d'erreur
        }
        HAL_Delay(300); // Attente pour stabiliser la connexion
    }
    else if (mode == ESP01_WIFI_MODE_STA_AP) // Si le mode est STA+AP
    {
        ESP01_LOG_INFO("WIFI", "Connexion au réseau WiFi...");                         // Log la connexion au réseau WiFi
        status = esp01_connect_wifi(ssid, password);                                   // Tente de se connecter au réseau WiFi
        ESP01_LOG_INFO("WIFI", "Connexion WiFi : %s", esp01_get_error_string(status)); // Log le statut de la connexion
        if (status != ESP01_OK)                                                        // Si la connexion échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Connexion WiFi (CWJAP)"); // Log l'erreur
            return status;                                              // Retourne le statut d'erreur
        }
        HAL_Delay(300); // Attente pour stabiliser la connexion
    }

    ESP01_LOG_INFO("WIFI", "Activation de l'affichage IP client dans +IPD (AT+CIPDINFO=1)..."); // Log l'activation de l'affichage IP client
    status = esp01_send_at_with_resp("AT+CIPDINFO=1", "OK", 2000, resp, sizeof(resp));          // Active l'affichage IP client
    ESP01_LOG_INFO("WIFI", "Set CIPDINFO : %s", esp01_get_error_string(status));                // Log le statut de l'activation
    if (status != ESP01_OK)                                                                     // Si l'activation de l'affichage IP client échoue
    {
        ESP01_LOG_ERROR("WIFI", "Erreur : AT+CIPDINFO=1"); // Log l'erreur
        return status;                                     // Retourne le statut d'erreur
    }

    ESP01_LOG_INFO("WIFI", "=== Configuration WiFi terminée ==="); // Log la fin de la configuration WiFi
    return ESP01_OK;                                               // Retourne OK si tout s'est bien passé
}

/* ========================== FONCTIONS UTILITAIRES ========================== */

const char *esp01_encryption_to_string(const char *enc)
{
    if (!enc)
        return "INCONNU"; // Retourne inconnu si NULL
    if (strcmp(enc, "0") == 0)
        return "Open"; // Aucun chiffrement
    if (strcmp(enc, "1") == 0)
        return "WEP"; // WEP
    if (strcmp(enc, "2") == 0)
        return "WPA_PSK"; // WPA_PSK
    if (strcmp(enc, "3") == 0)
        return "WPA2_PSK"; // WPA2_PSK
    if (strcmp(enc, "4") == 0)
        return "WPA_WPA2_PSK"; // WPA/WPA2_PSK
    return "INCONNU";          // Autre valeur
}

const char *esp01_tcp_status_to_string(const char *resp)
{
    static char buf[64];                               // Buffer statique pour la chaîne retournée
    char *line = strstr(resp, "STATUS:");              // Cherche le motif STATUS:
    int code = -1;                                     // Code de statut TCP
    if (line && sscanf(line, "STATUS:%d", &code) == 1) // Extrait le code
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
        return buf; // Retourne la chaîne formatée
    }
    return "Réponse TCP/IP inattendue"; // Si le motif n'est pas trouvé
}

const char *esp01_cwstate_to_string(const char *resp)
{
    static char buf[64];                    // Buffer statique pour la chaîne retournée
    char *line = strstr(resp, "+CWSTATE:"); // Cherche le motif +CWSTATE:
    int code = -1;                          // Code d'état
    char ssid[ESP01_MAX_SSID_BUF] = {0};    // Buffer pour le SSID
    if (line)
    {
        if (sscanf(line, "+CWSTATE:%d,\"%[^\"]\"", &code, ssid) == 2)
            snprintf(buf, sizeof(buf), "Connecté (état %d) à SSID: %s", code, ssid); // Format connecté
        else if (sscanf(line, "+CWSTATE:%d", &code) == 1)
            snprintf(buf, sizeof(buf), "État WiFi : %d", code); // Format état seul
        else
            snprintf(buf, sizeof(buf), "Format CWSTATE inconnu"); // Format inconnu
        return buf;                                               // Retourne la chaîne formatée
    }
    return "Réponse CWSTATE inattendue"; // Si le motif n'est pas trouvé
}

const char *esp01_connection_status_to_string(const char *resp)
{
    static char buf[64];                  // Buffer statique pour la chaîne retournée
    char *line = strstr(resp, "+CWJAP:"); // Cherche le motif +CWJAP:
    if (line)
    {
        char ssid[ESP01_MAX_SSID_BUF] = {0}; // Buffer pour le SSID
        if (sscanf(line, "+CWJAP:\"%[^\"]", ssid) == 1)
            snprintf(buf, sizeof(buf), "Connecté à %s", ssid); // Format connecté avec SSID
        else
            snprintf(buf, sizeof(buf), "Connecté (SSID inconnu)"); // Format connecté sans SSID
        return buf;                                                // Retourne la chaîne formatée
    }
    return "Non connecté"; // Si le motif n'est pas trouvé
}

const char *esp01_rf_power_to_string(int rf_dbm)
{
    static char buf[ESP01_MAX_CMD_BUF];           // Buffer statique pour la chaîne retournée
    snprintf(buf, sizeof(buf), "%d dBm", rf_dbm); // Formate la puissance RF
    return buf;                                   // Retourne la chaîne formatée
}

const char *esp01_cwqap_to_string(const char *resp)
{
    if (strstr(resp, "WIFI DISCONNECT"))
        return "WIFI DISCONNECTED"; // Déconnexion détectée
    if (strstr(resp, "OK"))
        return "OK";         // OK détecté
    return "Déconnexion OK"; // Autre cas
}

const char *esp01_wifi_connection_to_string(const char *resp)
{
    static char buf[ESP01_MAX_RESP_BUF]; // Buffer statique pour la chaîne retournée
    char ssid[ESP01_MAX_SSID_BUF] = {0}; // Buffer pour le SSID
    if (strstr(resp, "+CWJAP:"))
    {
        if (sscanf(resp, "+CWJAP:\"%[^\"]\"", ssid) == 1)
            snprintf(buf, sizeof(buf), "Connecté à : %s", ssid); // Format connecté avec SSID
        else
            snprintf(buf, sizeof(buf), "Connecté (SSID inconnu)"); // Format connecté sans SSID
    }
    else if (strstr(resp, "No AP"))
    {
        snprintf(buf, sizeof(buf), "Non connecté à un AP"); // Non connecté à un AP
    }
    else
    {
        snprintf(buf, sizeof(buf), "Statut WiFi inconnu"); // Statut inconnu
    }
    return buf; // Retourne la chaîne formatée
}

const char *esp01_dhcp_status_to_string(const char *resp)
{
    static char buf[ESP01_MAX_RESP_BUF]; // Buffer statique pour la chaîne retournée
    int mode = -1;                       // Variable pour le mode DHCP
    if (sscanf(resp, "+CWDHCP:%d", &mode) == 1)
        snprintf(buf, sizeof(buf), "DHCP %s", (mode & 1) ? "activé" : "désactivé"); // Format activé/désactivé
    else
        snprintf(buf, sizeof(buf), "Statut DHCP inconnu"); // Statut inconnu
    return buf;                                            // Retourne la chaîne formatée
}

const char *esp01_ip_info_to_string(const char *resp)
{
    static char buf[ESP01_MAX_RESP_BUF];                                                                         // Buffer statique pour la chaîne retournée
    char ip[ESP01_SMALL_BUF_SIZE] = "N/A", gw[ESP01_SMALL_BUF_SIZE] = "N/A", mask[ESP01_SMALL_BUF_SIZE] = "N/A"; // Buffers pour IP, GW, masque
    esp01_extract_quoted_value(resp, "+CIPSTA:ip:\"", ip, sizeof(ip));                                           // Extrait l'IP
    esp01_extract_quoted_value(resp, "+CIPSTA:gateway:\"", gw, sizeof(gw));                                      // Extrait la gateway
    esp01_extract_quoted_value(resp, "+CIPSTA:netmask:\"", mask, sizeof(mask));                                  // Extrait le masque
    snprintf(buf, sizeof(buf), "IP: %s, GW: %s, MASK: %s", ip, gw, mask);                                        // Formate la chaîne
    return buf;                                                                                                  // Retourne la chaîne formatée
}

const char *esp01_hostname_raw_to_string(const char *resp)
{
    static char buf[ESP01_MAX_RESP_BUF];                                            // Buffer statique pour la chaîne retournée
    char hostname[48] = "N/A";                                                      // Buffer pour le hostname
    esp01_extract_quoted_value(resp, "+CWHOSTNAME:\"", hostname, sizeof(hostname)); // Extrait le hostname
    snprintf(buf, sizeof(buf), "Hostname: %s", hostname);                           // Formate la chaîne
    return buf;                                                                     // Retourne la chaîne formatée
}

const char *esp01_ap_config_to_string(const char *resp)
{
    static char out[128];                              // Buffer statique pour la chaîne retournée
    char ssid[33] = {0};                               // Buffer pour le SSID
    char pwd[33] = {0};                                // Buffer pour le mot de passe
    int channel = 0, enc = 0, maxconn = 0, hidden = 0; // Variables pour les paramètres AP

    // Recherche du motif +CWSAP:"SSID","PWD",channel,enc,maxconn,hidden
    const char *start = strstr(resp, "+CWSAP:"); // Cherche le motif +CWSAP:
    if (start)
    {
        int n = sscanf(start, "+CWSAP:\"%32[^\"]\",\"%32[^\"]\",%d,%d,%d,%d",
                       ssid, pwd, &channel, &enc, &maxconn, &hidden); // Extrait les paramètres
        if (n >= 4)
        {
            snprintf(out, sizeof(out),
                     "SSID: %s, PWD: %s, CH: %d, Sécu: %s, MaxConn: %d, Hidden: %d",
                     ssid, pwd, channel,
                     (enc == 0) ? "Open" : (enc == 2) ? "WPA_PSK"
                                       : (enc == 3)   ? "WPA2_PSK"
                                       : (enc == 4)   ? "WPA_WPA2_PSK"
                                                      : "Inconnu",
                     maxconn, hidden); // Formate la chaîne
            return out;                // Retourne la chaîne formatée
        }
    }
    return "Config AP inconnue"; // Si le motif n'est pas trouvé
}

const char *esp01_ping_result_to_string(const char *resp)
{
    static char result[ESP01_SMALL_BUF_SIZE];  // Buffer statique pour la chaîne retournée
    const char *line = strstr(resp, "+PING:"); // Cherche le motif +PING:
    if (line)
    {
        int ms = 0; // Variable pour le temps de ping
        if (sscanf(line, "+PING:%d", &ms) == 1)
        {
            snprintf(result, sizeof(result), "%d ms", ms); // Formate le résultat
            return result;                                 // Retourne la chaîne formatée
        }
    }
    return "Ping échoué"; // Si le motif n'est pas trouvé
}


