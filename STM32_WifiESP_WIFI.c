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
 *   - Modes WiFi (STA, AP, STA+AP)
 *   - Scan des réseaux, connexion, déconnexion
 *   - DHCP, IP, MAC, hostname, clients AP
 *   - TCP/IP, ping, statut, multi-connexion
 *   - Fonctions utilitaires d'affichage et de parsing
 *
 * @note
 *   - Nécessite le driver bas niveau STM32_WifiESP.h
 ******************************************************************************
 */

#include "STM32_WifiESP_WIFI.h" // Header du module WiFi haut niveau
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ========================== CONSTANTES SPÉCIFIQUES AU MODULE ========================== */
#define ESP01_WIFI_SCAN_TIMEOUT 10000    // Timeout scan WiFi (ms)
#define ESP01_WIFI_CONNECT_TIMEOUT 15000 // Timeout connexion WiFi (ms)

/* ========================= FONCTIONS PRINCIPALES (API WiFi) ========================= */

/**
 * @brief  Récupère le statut de connexion WiFi (connecté ou non).
 * @return ESP01_Status_t Code de statut (OK, erreur, etc.)
 */
ESP01_Status_t esp01_get_connection_status(void)
{
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT pour vérifier le statut de connexion
    if (st != ESP01_OK)                                                                                         // Vérifie si la commande a réussi
    {
        ESP01_LOG_ERROR("STATUS", "Erreur lors de la vérification du statut: %s", esp01_get_error_string(st)); // Affiche un message d'erreur
        return st;                                                                                             // Retourne le code d'erreur
    }

    ESP01_LOG_DEBUG("STATUS", "Réponse : %s", resp); // Affiche la réponse brute du module

    if (strstr(resp, "+CWJAP:")) // Vérifie si la réponse contient le motif indiquant une connexion réussie
    {
        ESP01_LOG_DEBUG("STATUS", "WiFi connecté"); // Affiche un message de succès
        return ESP01_OK;                            // Retourne ESP01_OK si connecté
    }

    ESP01_LOG_WARN("STATUS", "Motif non trouvé : non connecté"); // Avertit que le motif de connexion n'a pas été trouvé
    return ESP01_WIFI_NOT_CONNECTED;                             // Retourne ESP01_WIFI_NOT_CONNECTED si non connecté
}

/**
 * @brief  Récupère le mode WiFi actuel (STA, AP, STA+AP).
 * @param  mode Pointeur vers la variable qui recevra le mode WiFi (1=STA, 2=AP, 3=STA+AP)
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 */
ESP01_Status_t esp01_get_wifi_mode(uint8_t *mode)
{
    VALIDATE_PARAM(mode, ESP01_INVALID_PARAM); // Vérification du pointeur d'entrée

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWMODE?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoi de la commande AT pour récupérer le mode WiFi
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWMODE", "Erreur lors de la lecture du mode: %s", esp01_get_error_string(st));
        return st;
    }

    int32_t mode_tmp = 0; // Variable temporaire pour stocker le mode WiFi
    if (esp01_parse_int_after(resp, "+CWMODE:", &mode_tmp) != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWMODE", "Impossible de parser le mode dans: %s", resp);
        return ESP01_PARSE_ERROR;
    }

    *mode = (uint8_t)mode_tmp; // Affecte la valeur au pointeur fourni
    ESP01_LOG_DEBUG("CWMODE", "Mode WiFi actuel: %d (%s)", *mode, esp01_wifi_mode_to_string((uint8_t)*mode));
    return ESP01_OK;
}

/**
 * @brief  Définit le mode WiFi (STA, AP, STA+AP).
 * @param  mode Mode à appliquer (voir ESP01_WifiMode_t).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_wifi_mode(uint8_t mode)
{
    if (mode < 1 || mode > 3)
    {
        ESP01_LOG_ERROR("CWMODE", "Mode invalide: %d", mode);
        ESP01_RETURN_ERROR("CWMODE", ESP01_INVALID_PARAM);
    }

    char cmd[ESP01_SMALL_BUF_SIZE];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWMODE", "Erreur lors de la configuration du mode: %s", esp01_get_error_string(st));
        ESP01_RETURN_ERROR("CWMODE", st);
    }

    ESP01_LOG_DEBUG("CWMODE", "Mode WiFi configuré à %d (%s)", mode, esp01_wifi_mode_to_string(mode));
    return ESP01_OK;
}

/**
 * @brief  Scanne les réseaux WiFi à proximité.
 * @param  networks      Tableau de structures à remplir.
 * @param  max_networks  Taille du tableau.
 * @param  found_networks Nombre de réseaux trouvés (en sortie).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks)
{
    VALIDATE_PARAM(networks && found_networks && max_networks > 0, ESP01_INVALID_PARAM);

    char resp[ESP01_LARGE_RESP_BUF] = {0};

    ESP01_LOG_DEBUG("CWLAP", "Scan des réseaux WiFi...");
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWLAP", resp, sizeof(resp), "OK", ESP01_WIFI_SCAN_TIMEOUT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWLAP", "Erreur lors du scan: %s", esp01_get_error_string(st));
        return st;
    }

    *found_networks = 0;
    char *line = resp;

    while (*found_networks < max_networks)
    {
        char *start = strstr(line, "+CWLAP:(");
        if (!start)
            break;

        if (esp01_parse_cwlap_line(start, &networks[*found_networks]))
        {
            (*found_networks)++;
        }
        line = start + 1;
    }

    ESP01_LOG_DEBUG("CWLAP", "%d réseaux trouvés", *found_networks);
    return ESP01_OK;
}

/**
 * @brief  Parse une ligne de réponse CWLAP et remplit la structure esp01_network_t.
 * @param  line    Ligne à parser (format CWLAP).
 * @param  network Pointeur vers la structure à remplir.
 * @return true si le parsing a réussi, false sinon.
 */
bool esp01_parse_cwlap_line(const char *line, esp01_network_t *network)
{
    VALIDATE_PARAM(line && network, false); // Vérification des pointeurs

    int enc = 0, rssi = 0, channel = 0, freq_offset = 0, freqcal_val = 0;
    int pairwise_cipher = 0, group_cipher = 0, bgn = 0, wps = 0;
    char ssid[ESP01_MAX_SSID_BUF] = {0};
    char mac[ESP01_MAX_MAC_LEN] = {0};

    // Parsing complet selon la doc officielle
    int n = sscanf(line, "+CWLAP:(%d,\"%[^\"]\",%d,\"%[^\"]\",%d,%d,%d,%d,%d,%d,%d)",
                   &enc, ssid, &rssi, mac, &channel, &freq_offset, &freqcal_val,
                   &pairwise_cipher, &group_cipher, &bgn, &wps);
    if (n < 5)
        return false;

    esp01_safe_strcpy(network->ssid, ESP01_MAX_SSID_BUF, ssid);
    esp01_safe_strcpy(network->mac, ESP01_MAX_MAC_LEN, mac);
    // Correction du RSSI : il est signé, on le stocke tel quel
    network->rssi = rssi;
    network->channel = (uint8_t)channel;
    snprintf(network->encryption, ESP01_MAX_ENCRYPTION_LEN, "%d", enc);
    if (n > 5)
        network->freq_offset = freq_offset;
    if (n > 6)
        network->freqcal_val = freqcal_val;
    if (n > 7)
        network->pairwise_cipher = pairwise_cipher;
    if (n > 8)
        network->group_cipher = group_cipher;
    if (n > 9)
        network->bgn = bgn;
    if (n > 10)
        network->wps = wps;
    return true;
}

/**
 * @brief  Active ou désactive le DHCP.
 * @param  enable true pour activer, false pour désactiver.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_dhcp(bool enable)
{
    // Pas de pointeur à valider ici
    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CWDHCP=1,%d", enable ? 1 : 0);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWDHCP", "Erreur lors de la configuration du DHCP: %s", esp01_get_error_string(st));
        return st;
    }

    ESP01_LOG_DEBUG("CWDHCP", "DHCP %s", enable ? "activé" : "désactivé");
    return ESP01_OK;
}

/**
 * @brief  Récupère l'état du DHCP.
 * @param  enabled Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_dhcp(bool *enabled)
{
    VALIDATE_PARAM(enabled, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWDHCP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWDHCP", "Erreur lors de la lecture de l'état DHCP: %s", esp01_get_error_string(st));
        return st;
    }

    int32_t dhcp_mode = 0;
    if (esp01_parse_int_after(resp, "+CWDHCP:", &dhcp_mode) != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWDHCP", "Impossible de parser l'état DHCP dans: %s", resp);
        return ESP01_FAIL;
    }

    *enabled = (dhcp_mode & 1) != 0;
    ESP01_LOG_DEBUG("CWDHCP", "DHCP %s", *enabled ? "activé" : "désactivé");
    return ESP01_OK;
}

/**
 * @brief  Déconnecte du réseau WiFi.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_disconnect_wifi(void)
{
    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_LOG_DEBUG("CWQAP", "Déconnexion du WiFi...");
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWQAP", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWQAP", "Erreur lors de la déconnexion: %s", esp01_get_error_string(st));
        return st;
    }

    ESP01_LOG_DEBUG("CWQAP", "Déconnecté avec succès");
    return ESP01_OK;
}

/**
 * @brief  Connecte au WiFi (mode simple).
 * @param  ssid     SSID du réseau.
 * @param  password Mot de passe.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
    char cmd[ESP01_MAX_CMD_BUF] = {0};
    char resp[ESP01_MAX_RESP_BUF] = {0};

    // Validation des paramètres d'entrée
    VALIDATE_PARAM(ssid && strlen(ssid) > 0, ESP01_INVALID_PARAM);
    VALIDATE_PARAM(password && strlen(password) > 0, ESP01_INVALID_PARAM);

    // Construction de la commande de connexion
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

    // Envoi de la commande de connexion avec attente longue
    ESP01_LOG_DEBUG("WIFI", "Connexion au réseau %s...", ssid);
    ESP01_Status_t status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_WIFI_CONNECT_TIMEOUT);

    // Analyse de la réponse
    if (status != ESP01_OK)
    {
        // Analyse des erreurs spécifiques
        if (strstr(resp, "+CWJAP:1"))
        {
            ESP01_LOG_ERROR("WIFI", "Échec de connexion: Délai dépassé");
            return ESP01_WIFI_TIMEOUT;
        }
        else if (strstr(resp, "+CWJAP:2"))
        {
            ESP01_LOG_ERROR("WIFI", "Échec de connexion: Mot de passe incorrect");
            return ESP01_WIFI_WRONG_PASSWORD;
        }
        else if (strstr(resp, "+CWJAP:3"))
        {
            ESP01_LOG_ERROR("WIFI", "Échec de connexion: AP introuvable");
            return ESP01_WIFI_AP_NOT_FOUND;
        }
        else if (strstr(resp, "+CWJAP:4"))
        {
            ESP01_LOG_ERROR("WIFI", "Échec de connexion: Échec de connexion");
            return ESP01_WIFI_CONNECT_FAIL;
        }

        ESP01_LOG_ERROR("WIFI", "Échec de connexion WiFi: %s", resp);
        return ESP01_FAIL;
    }

    ESP01_LOG_DEBUG("WIFI", "Connexion réussie au réseau %s", ssid);
    return ESP01_OK;
} // Fin de esp01_connect_wifi

/**
 * @brief  Récupère l'adresse IP courante.
 * @param  ip_buf  Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len)
{
    VALIDATE_PARAM(ip_buf && buf_len >= ESP01_MAX_IP_LEN, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIFSR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CIFSR", "Erreur lors de la récupération de l'IP: %s", esp01_get_error_string(st));
        ESP01_RETURN_ERROR("CIFSR", st);
    }

    // Recherche de l'adresse IP dans la réponse
    char *ip_start = strstr(resp, "+CIFSR:STAIP,\"");
    if (!ip_start)
    {
        ESP01_LOG_ERROR("CIFSR", "Format de réponse non reconnu: %s", resp);
        ESP01_RETURN_ERROR("CIFSR", ESP01_FAIL);
    }

    ip_start += 14; // Déplace le pointeur après "+CIFSR:STAIP,\""

    // Cherche la fin de l'IP (guillemet fermant)
    char *ip_end = strchr(ip_start, '\"');
    if (!ip_end)
    {
        ESP01_LOG_ERROR("CIFSR", "IP mal formatée dans la réponse: %s", resp);
        ESP01_RETURN_ERROR("CIFSR", ESP01_FAIL);
    }

    // Calcule la longueur de l'IP
    size_t ip_len = ip_end - ip_start;

    // Utilisation de esp01_check_buffer_size et esp01_safe_strcpy
    if (esp01_check_buffer_size(ip_len, buf_len - 1) != ESP01_OK)
    {
        ESP01_LOG_ERROR("CIFSR", "Buffer trop petit pour stocker l'IP (longueur: %u)", ip_len);
        ESP01_RETURN_ERROR("CIFSR", ESP01_BUFFER_OVERFLOW);
    }

    // Remplacement de memcpy par une solution plus sûre
    char temp_ip[ESP01_MAX_IP_LEN] = {0};
    memcpy(temp_ip, ip_start, ip_len);
    temp_ip[ip_len] = '\0';
    esp01_safe_strcpy(ip_buf, buf_len, temp_ip);

    ESP01_LOG_DEBUG("CIFSR", "IP récupérée: %s", ip_buf);
    return ESP01_OK;
}

/**
 * @brief  Récupère le RSSI courant (niveau de signal).
 * @param  rssi Pointeur vers la variable de sortie (dBm).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_rssi(int *rssi)
{
    VALIDATE_PARAM(rssi, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;

    // Format: +CWJAP:"ssid","bssid",channel,rssi,...
    const char *cwjap = strstr(resp, "+CWJAP:");
    if (!cwjap)
        return ESP01_WIFI_NOT_CONNECTED;

    // On cherche la 4ème virgule (après le canal)
    const char *pos = cwjap;
    uint8_t count = 0;
    while (*pos && count < 3)
    {
        if (*pos == ',')
            count++;
        pos++;
    }

    // Maintenant pos pointe au début du RSSI
    if (*pos)
    {
        int temp_rssi;
        if (sscanf(pos, "%d", &temp_rssi) == 1)
        {
            *rssi = (uint8_t)temp_rssi;
            ESP01_LOG_DEBUG("RSSI", "Force du signal: %d dBm", *rssi);
            return ESP01_OK;
        }
    }

    return ESP01_PARSE_ERROR;
}

/**
 * @brief  Récupère l'adresse MAC courante.
 * @param  mac_buf Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len)
{
    VALIDATE_PARAM(mac_buf && buf_len >= ESP01_MAX_MAC_LEN, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIFSR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("MAC", "Erreur lors de la récupération de l'adresse MAC: %s", esp01_get_error_string(st));
        ESP01_RETURN_ERROR("MAC", st);
    }

    // Recherche de l'adresse MAC dans la réponse
    char *mac_start = strstr(resp, "+CIFSR:STAMAC,\"");
    if (!mac_start)
    {
        ESP01_LOG_ERROR("MAC", "Format de réponse non reconnu: %s", resp);
        ESP01_RETURN_ERROR("MAC", ESP01_FAIL);
    }

    mac_start += 15; // Déplace le pointeur après "+CIFSR:STAMAC,\""

    // Cherche la fin de l'adresse MAC (guillemet fermant)
    char *mac_end = strchr(mac_start, '\"');
    if (!mac_end)
    {
        ESP01_LOG_ERROR("MAC", "Adresse MAC mal formatée dans la réponse: %s", resp);
        ESP01_RETURN_ERROR("MAC", ESP01_FAIL);
    }

    // Calcule la longueur de l'adresse MAC
    size_t mac_len = mac_end - mac_start;

    // Utilisation de esp01_check_buffer_size et esp01_safe_strcpy
    if (esp01_check_buffer_size(mac_len, buf_len - 1) != ESP01_OK)
    {
        ESP01_LOG_ERROR("MAC", "Buffer trop petit pour stocker l'adresse MAC (longueur: %u)", mac_len);
        ESP01_RETURN_ERROR("MAC", ESP01_BUFFER_OVERFLOW);
    }

    // Remplacement de memcpy par une solution plus sûre
    char temp_mac[ESP01_MAX_MAC_LEN] = {0};
    memcpy(temp_mac, mac_start, mac_len);
    temp_mac[mac_len] = '\0';
    esp01_safe_strcpy(mac_buf, buf_len, temp_mac);

    ESP01_LOG_DEBUG("MAC", "Adresse MAC récupérée: %s", mac_buf);
    return ESP01_OK;
}

/**
 * @brief  Définit le hostname du module.
 * @param  hostname Chaîne hostname.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_hostname(const char *hostname)
{
    VALIDATE_PARAM(hostname, ESP01_INVALID_PARAM);

    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CWHOSTNAME=\"%s\"", hostname);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("HOSTNAME", "Erreur lors de la configuration du hostname: %s", esp01_get_error_string(st));
        return st;
    }

    ESP01_LOG_DEBUG("HOSTNAME", "Hostname configuré: %s", hostname);
    return ESP01_OK;
}

/**
 * @brief  Récupère le hostname du module.
 * @param  hostname Buffer de sortie.
 * @param  len      Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_hostname(char *hostname, size_t len)
{
    VALIDATE_PARAM(hostname && len >= ESP01_MAX_HOSTNAME_LEN, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWHOSTNAME?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("HOSTNAME", "Erreur lors de la récupération du hostname: %s", esp01_get_error_string(st));
        return st;
    }

    // Recherche du hostname dans la réponse
    char *start = strstr(resp, "+CWHOSTNAME:");
    if (!start)
    {
        ESP01_LOG_ERROR("HOSTNAME", "Format de réponse non reconnu: %s", resp);
        return ESP01_FAIL;
    }

    start += 12; // Passe au-delà de "+CWHOSTNAME:"

    // Enlève les espaces avant
    while (*start && (*start == ' ' || *start == '\r' || *start == '\n'))
        start++;

    // Copie jusqu'au \r\n ou fin de chaîne
    size_t i = 0;
    while (i < len - 1 && start[i] && start[i] != '\r' && start[i] != '\n')
    {
        hostname[i] = start[i];
        i++;
    }
    hostname[i] = '\0';

    ESP01_LOG_DEBUG("HOSTNAME", "Hostname récupéré: %s", hostname);
    return ESP01_OK;
}

/**
 * @brief  Effectue un ping vers une adresse.
 * @param  host Adresse à pinger.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_ping(const char *host)
{
    VALIDATE_PARAM(host, ESP01_INVALID_PARAM);

    char cmd[ESP01_SMALL_BUF_SIZE];
    char resp[ESP01_MAX_RESP_BUF];

    // Construction de la commande AT+PING
    snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"", host);

    // Envoi de la commande
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);
    if (st != ESP01_OK)
        return st;

    return ESP01_OK;
}

/**
 * @brief  Récupère le statut TCP.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);

    ESP01_LOG_DEBUG("CIPSTATUS", "=== Lecture statut TCP ===");

    char resp[ESP01_MAX_RESP_BUF] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTATUS", resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);

    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CIPSTATUS", "Erreur lors de la lecture du statut TCP: %s", esp01_get_error_string(st));
        return st;
    }

    if (strlen(resp) >= out_size)
    {
        ESP01_LOG_ERROR("CIPSTATUS", "Buffer trop petit pour stocker le résultat");
        return ESP01_BUFFER_OVERFLOW;
    }

    esp01_safe_strcpy(out, out_size, resp);

    ESP01_LOG_DEBUG("CIPSTATUS", "Statut TCP récupéré avec succès");
    return ESP01_OK;
}

/**
 * @brief  Récupère l'état de connexion WiFi (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_connection(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWJAP?", "Erreur lors de la lecture de la connexion: %s", esp01_get_error_string(st));
        return st;
    }

    if (strlen(resp) >= out_size)
    {
        ESP01_LOG_ERROR("CWJAP?", "Buffer trop petit pour stocker le résultat");
        return ESP01_BUFFER_OVERFLOW;
    }

    esp01_safe_strcpy(out, out_size, resp);

    ESP01_LOG_DEBUG("CWJAP?", "État de connexion WiFi récupéré");
    return ESP01_OK;
}

/**
 * @brief  Récupère l'état WiFi (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_state(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWSTATE?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWSTATE", "Erreur lors de la lecture de l'état: %s", esp01_get_error_string(st));
        return st;
    }

    if (strlen(resp) >= out_size)
    {
        ESP01_LOG_ERROR("CWSTATE", "Buffer trop petit pour stocker le résultat");
        return ESP01_BUFFER_OVERFLOW;
    }

    esp01_safe_strcpy(out, out_size, resp);

    ESP01_LOG_DEBUG("CWSTATE", "État WiFi récupéré");
    return ESP01_OK;
}

/**
 * @brief  Récupère la config AP (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ap_config(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWSAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWSAP", "Erreur lors de la lecture de la config AP: %s", esp01_get_error_string(st));
        return st;
    }

    if (strlen(resp) >= out_size)
    {
        ESP01_LOG_ERROR("CWSAP", "Buffer trop petit pour stocker le résultat");
        return ESP01_BUFFER_OVERFLOW;
    }

    esp01_safe_strcpy(out, out_size, resp);

    ESP01_LOG_DEBUG("CWSAP", "Configuration AP récupérée");
    return ESP01_OK;
}

/**
 * @brief  Configure un AP.
 * @param  ssid      SSID.
 * @param  password  Mot de passe.
 * @param  channel   Canal.
 * @param  encryption Type d'encryptage.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_start_ap_config(const char *ssid, const char *password, uint8_t channel, uint8_t encryption)
{
    VALIDATE_PARAM(ssid && password && channel >= 1 && channel <= 14 && encryption >= 0 && encryption <= 7, ESP01_INVALID_PARAM);

    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",%d,%d", ssid, password, channel, encryption);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_LOG_DEBUG("CWSAP", "Configuration de l'AP \"%s\" sur le canal %d...", ssid, channel);
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);

    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWSAP", "Erreur lors de la configuration de l'AP: %s", esp01_get_error_string(st));
        return st;
    }

    // Conversion du code d'encryption en chaîne de caractères pour le log
    char enc_str[8];
    snprintf(enc_str, sizeof(enc_str), "%d", encryption);
    const char *enc_description = esp01_encryption_to_string(enc_str);

    ESP01_LOG_DEBUG("CWSAP", "AP configuré avec succès: SSID=\"%s\", Canal=%d, Encryption=%d (%s)",
                    ssid, channel, encryption, enc_description);
    return ESP01_OK;
}

/**
 * @brief  Récupère les informations complètes de connexion WiFi.
 * @param  ssid       Buffer pour stocker le SSID.
 * @param  ssid_size  Taille du buffer SSID.
 * @param  bssid      Buffer pour stocker le BSSID/MAC AP.
 * @param  bssid_size Taille du buffer BSSID.
 * @param  channel    Pointeur pour stocker le canal.
 * @param  rssi       Pointeur pour stocker le RSSI.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_connection_info(
    char *ssid, size_t ssid_size,
    char *bssid, size_t bssid_size,
    uint8_t *channel, int *rssi)
{
    // On valide uniquement les buffers si leur taille > 0
    if ((!ssid && ssid_size > 0) || (!bssid && bssid_size > 0))
    {
        ESP01_LOG_ERROR("CONN_INFO", "Paramètres invalides");
        return ESP01_INVALID_PARAM;
    }

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CONN_INFO", "Erreur lors de la lecture des infos de connexion: %s",
                        esp01_get_error_string(st));
        return st;
    }

    // Utilisation de la fonction de parsing pour extraire toutes les informations
    if (esp01_parse_cwjap_response(resp, ssid, ssid_size, bssid, bssid_size, channel, rssi, NULL) != ESP01_OK)
    {
        ESP01_LOG_ERROR("CONN_INFO", "Erreur de parsing de la réponse: %s", resp);
        return ESP01_FAIL;
    }

    ESP01_LOG_DEBUG("CONN_INFO", "Informations récupérées avec succès");
    return ESP01_OK;
}

/**
 * @brief  Connecte au WiFi avec configuration avancée (mode, DHCP/IP statique).
 * @param  mode      Mode WiFi.
 * @param  ssid      SSID du réseau.
 * @param  password  Mot de passe.
 * @param  use_dhcp  true pour DHCP, false pour IP statique.
 * @param  ip        IP statique (si non DHCP).
 * @param  gateway   Gateway (si non DHCP).
 * @param  netmask   Masque réseau (si non DHCP).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_connect_wifi_config(
    ESP01_WifiMode_t mode,
    const char *ssid,
    const char *password,
    bool use_dhcp,
    const char *ip,
    const char *gateway,
    const char *netmask)
{
    VALIDATE_PARAM(ssid && password, ESP01_INVALID_PARAM);

    ESP01_Status_t status;         // Variable pour le statut de la commande
    char cmd[ESP01_MAX_RESP_BUF];  // Buffer pour la commande AT
    char resp[ESP01_MAX_RESP_BUF]; // Buffer pour la réponse AT

    ESP01_LOG_DEBUG("WIFI", "=== Début configuration WiFi ==="); // Log le début de la configuration WiFi

    ESP01_LOG_DEBUG("WIFI", "Définition du mode WiFi...");                    // Log la définition du mode WiFi
    status = esp01_set_wifi_mode(mode);                                       // Définit le mode WiFi
    ESP01_LOG_DEBUG("WIFI", "Set mode : %s", esp01_get_error_string(status)); // Log le statut de la définition du mode
    if (status != ESP01_OK)                                                   // Si la définition du mode échoue
    {
        ESP01_LOG_ERROR("WIFI", "Erreur : esp01_set_wifi_mode"); // Log l'erreur
        return status;                                           // Retourne le statut d'erreur
    }
    HAL_Delay(300); // Attente pour stabiliser le module

    if (mode == ESP01_WIFI_MODE_AP) // Si le mode est AP (point d'accès)
    {
        ESP01_LOG_DEBUG("WIFI", "Configuration du point d'accès (AP)...");
        snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",5,3", ssid, password);                     // Prépare la commande pour configurer l'AP
        status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Envoie la commande

        ESP01_LOG_DEBUG("WIFI", "Set AP : %s", esp01_get_error_string(status)); // Log le statut de la commande
        if (status != ESP01_OK)                                                 // Si la configuration de l'AP échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Configuration AP"); // Log l'erreur
            return status;                                        // Retourne le statut d'erreur
        }
        HAL_Delay(300); // Attente pour stabiliser le module

        if (ip && strlen(ip) > 0) // Si une IP fixe est fournie pour l'AP
        {
            ESP01_LOG_DEBUG("WIFI", "Configuration IP fixe AP...");                                       // Log la configuration IP fixe
            snprintf(cmd, sizeof(cmd), "AT+CIPAP=\"%s\"", ip);                                            // Prépare la commande pour configurer l'IP de l'AP
            status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Envoie la commande
            ESP01_LOG_DEBUG("WIFI", "Set IP AP : %s", esp01_get_error_string(status));                    // Log le statut de la configuration IP
            if (status != ESP01_OK)                                                                       // Si la configuration IP de l'AP échoue
            {
                ESP01_LOG_ERROR("WIFI", "Erreur : Configuration IP AP"); // Log l'erreur
                return status;                                           // Retourne le statut d'erreur
            }
        }
    }

    if (use_dhcp)
    {
        if (mode == ESP01_WIFI_MODE_STA)
        {
            ESP01_LOG_DEBUG("WIFI", "Activation du DHCP client...");                                                  // Log l'activation du DHCP client
            status = esp01_send_raw_command_dma("AT+CWDHCP=1,1", resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Active le DHCP client
        }
        else if (mode == ESP01_WIFI_MODE_STA_AP)
        {
            ESP01_LOG_DEBUG("WIFI", "Activation du DHCP STA...");                                                     // Log l'activation du DHCP STA
            status = esp01_send_raw_command_dma("AT+CWDHCP=1,1", resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Active le DHCP pour la station
        }
        else if (mode == ESP01_WIFI_MODE_AP)
        {
            ESP01_LOG_DEBUG("WIFI", "Activation du DHCP AP...");                                                      // Log l'activation du DHCP pour l'AP
            status = esp01_send_raw_command_dma("AT+CWDHCP=2,1", resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Active le DHCP pour l'AP
        }
        ESP01_LOG_DEBUG("WIFI", "Set DHCP : %s", esp01_get_error_string(status)); // Log le statut de l'activation du DHCP
        if (status != ESP01_OK)                                                   // Si l'activation du DHCP échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Activation DHCP"); // Log l'erreur
            return status;                                       // Retourne le statut d'erreur
        }
    }
    else if (ip && gateway && netmask && mode == ESP01_WIFI_MODE_STA)
    {
        ESP01_LOG_DEBUG("WIFI", "Déconnexion du WiFi (CWQAP)...");                                  // Log la déconnexion du WiFi
        esp01_send_raw_command_dma("AT+CWQAP", resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Déconnecte le WiFi

        ESP01_LOG_DEBUG("WIFI", "Désactivation du DHCP client...");                                               // Log la désactivation du DHCP client
        status = esp01_send_raw_command_dma("AT+CWDHCP=0,1", resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Désactive le DHCP client
        ESP01_LOG_DEBUG("WIFI", "Set DHCP : %s", esp01_get_error_string(status));                                 // Log le statut de la désactivation du DHCP
        if (status != ESP01_OK)                                                                                   // Si la désactivation du DHCP échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Désactivation DHCP"); // Log l'erreur
            return status;                                          // Retourne le statut d'erreur
        }
        ESP01_LOG_DEBUG("WIFI", "Configuration IP statique...");                                      // Log la configuration de l'IP statique
        snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gateway, netmask);           // Prépare la commande pour configurer l'IP statique
        status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Envoie la commande
        ESP01_LOG_DEBUG("WIFI", "Set IP statique : %s", esp01_get_error_string(status));              // Log le statut de la configuration IP statique
        if (status != ESP01_OK)                                                                       // Si la configuration de l'IP statique échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Configuration IP statique"); // Log l'erreur
            return status;                                                 // Retourne le statut d'erreur
        }
    }

    if (mode == ESP01_WIFI_MODE_STA) // Si le mode est STA (station)
    {
        ESP01_LOG_DEBUG("WIFI", "Connexion au réseau WiFi...");                         // Log la connexion au réseau WiFi
        status = esp01_connect_wifi(ssid, password);                                    // Tente de se connecter au réseau WiFi
        ESP01_LOG_DEBUG("WIFI", "Connexion WiFi : %s", esp01_get_error_string(status)); // Log le statut de la connexion
        if (status != ESP01_OK)                                                         // Si la connexion échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Connexion WiFi (CWJAP)"); // Log l'erreur
            return status;                                              // Retourne le statut d'erreur
        }
        HAL_Delay(300); // Attente pour stabiliser la connexion
    }
    else if (mode == ESP01_WIFI_MODE_STA_AP) // Si le mode est STA+AP
    {
        ESP01_LOG_DEBUG("WIFI", "Connexion au réseau WiFi...");                         // Log la connexion au réseau WiFi
        status = esp01_connect_wifi(ssid, password);                                    // Tente de se connecter au réseau WiFi
        ESP01_LOG_DEBUG("WIFI", "Connexion WiFi : %s", esp01_get_error_string(status)); // Log le statut de la connexion
        if (status != ESP01_OK)                                                         // Si la connexion échoue
        {
            ESP01_LOG_ERROR("WIFI", "Erreur : Connexion WiFi (CWJAP)"); // Log l'erreur
            return status;                                              // Retourne le statut d'erreur
        }
        HAL_Delay(300); // Attente pour stabiliser la connexion
    }

    ESP01_LOG_DEBUG("WIFI", "Activation de l'affichage IP client dans +IPD (AT+CIPDINFO=1)...");              // Log l'activation de l'affichage IP client
    status = esp01_send_raw_command_dma("AT+CIPDINFO=1", resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT); // Active l'affichage IP client
    ESP01_LOG_DEBUG("WIFI", "Set CIPDINFO : %s", esp01_get_error_string(status));                             // Log le statut de l'activation
    if (status != ESP01_OK)                                                                                   // Si l'activation de l'affichage IP client échoue
    {
        ESP01_LOG_ERROR("WIFI", "Erreur : AT+CIPDINFO=1"); // Log l'erreur
        return status;                                     // Retourne le statut d'erreur
    }

    ESP01_LOG_DEBUG("WIFI", "=== Configuration WiFi terminée ==="); // Log la fin de la configuration WiFi
    return ESP01_OK;                                                // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Récupère le mode de connexion actuel (simple ou multiple).
 * @param  multi_conn Pointeur vers la variable qui recevra l'état
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 */
ESP01_Status_t esp01_get_connection_mode(uint8_t *multi_conn)
{
    VALIDATE_PARAM(multi_conn, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_LOG_DEBUG("CIPMUX", "Récupération du mode de connexion...");
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPMUX?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CIPMUX", "Erreur lors de la récupération du mode: %s", esp01_get_error_string(st));
        return st;
    }

    int32_t mode = 0;
    if (esp01_parse_int_after(resp, "+CIPMUX:", &mode) != ESP01_OK)
    {
        ESP01_LOG_ERROR("CIPMUX", "Impossible de parser le mode dans: %s", resp);
        return ESP01_PARSE_ERROR;
    }

    *multi_conn = (uint8_t)mode;
    ESP01_LOG_DEBUG("CIPMUX", "Mode de connexion : %s", *multi_conn ? "Multi-connexion" : "Connexion unique");
    return ESP01_OK;
}

/**
 * @brief  Récupère les informations sur le point d'accès connecté.
 * @param  ssid    Buffer pour stocker le SSID (NULL si non requis)
 * @param  bssid   Buffer pour stocker le BSSID (NULL si non requis)
 * @param  channel Pointeur pour stocker le canal (NULL si non requis)
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 */
ESP01_Status_t esp01_get_connected_ap_info(char *ssid, char *bssid, uint8_t *channel)
{
    char resp[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_RETURN_ERROR("GET_AP_INFO", st);
    }

    // Utiliser la fonction de parsing existante pour extraire les informations
    // Convertir uint8_t* en uint8_t* pour channel car la fonction esp01_parse_cwjap_response attend uint8_t*
    uint8_t ch_tmp = 0;
    st = esp01_parse_cwjap_response(resp,
                                    ssid, ssid ? ESP01_MAX_SSID_BUF : 0,
                                    bssid, bssid ? ESP01_MAX_MAC_LEN : 0,
                                    channel ? &ch_tmp : NULL, NULL, NULL);

    // Convertir le résultat uint8_t en uint8_t si nécessaire
    if (st == ESP01_OK && channel != NULL)
    {
        *channel = (uint8_t)ch_tmp;
    }

    return st;
} // Fin de esp01_get_connected_ap_info

/* ========================= OUTILS DE PARSING & CONVERSION ========================= */

/**
 * @brief  Parse la réponse CWJAP et extrait les informations de connexion WiFi.
 * @param  resp       Réponse brute du module ESP.
 * @param  ssid       Buffer pour stocker le SSID (NULL si non requis).
 * @param  ssid_size  Taille du buffer SSID.
 * @param  bssid      Buffer pour stocker le BSSID/MAC AP (NULL si non requis).
 * @param  bssid_size Taille du buffer BSSID.
 * @param  channel    Pointeur pour stocker le canal (NULL si non requis).
 * @param  rssi       Pointeur pour stocker le RSSI (NULL si non requis).
 * @param  enc_type   Pointeur pour stocker le type d'encryptage (NULL si non requis).
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_parse_cwjap_response(
    const char *resp,
    char *ssid, size_t ssid_size,
    char *bssid, size_t bssid_size,
    uint8_t *channel, int *rssi, uint8_t *enc_type)
{
    VALIDATE_PARAM(resp, ESP01_INVALID_PARAM);

    // Format attendu: +CWJAP:"ssid","bssid",channel,rssi,enc_type,...
    const char *cwjap = strstr(resp, "+CWJAP:");
    if (!cwjap)
        return ESP01_WIFI_NOT_CONNECTED;

    // Extraction du SSID (entre guillemets)
    if (ssid && ssid_size > 0)
    {
        if (!esp01_extract_quoted_value(cwjap, "+CWJAP:", ssid, ssid_size))
            return ESP01_PARSE_ERROR;
    }

    // Extraction du BSSID (entre guillemets, après le SSID)
    if (bssid && bssid_size > 0)
    {
        const char *bssid_start = strchr(cwjap + 7, ',');
        if (!bssid_start || !esp01_extract_quoted_value(bssid_start, ",", bssid, bssid_size))
            return ESP01_PARSE_ERROR;
    }

    // Parsing des valeurs numériques
    const char *after_bssid = strstr(cwjap, "\",");
    if (after_bssid)
    {
        after_bssid += 2; // Saute le guillemet et la virgule

        // Lecture du canal
        if (channel)
        {
            int temp_channel;
            if (sscanf(after_bssid, "%d", &temp_channel) != 1)
                return ESP01_PARSE_ERROR;
            *channel = (uint8_t)temp_channel;
        }

        // Avance jusqu'à la virgule suivante pour le RSSI
        const char *rssi_pos = strchr(after_bssid, ',');
        if (rssi_pos && rssi)
        {
            int temp_rssi;
            if (sscanf(rssi_pos + 1, "%d", &temp_rssi) != 1)
                return ESP01_PARSE_ERROR;
            *rssi = temp_rssi; // Correction : pas de cast, rssi est un int
        }

        // Si besoin de lire le type d'encryption, continuer le parsing...
        if (enc_type && rssi_pos)
        {
            const char *enc_pos = strchr(rssi_pos + 1, ',');
            if (enc_pos)
            {
                int temp_enc_type;
                if (sscanf(enc_pos + 1, "%d", &temp_enc_type) != 1)
                    return ESP01_PARSE_ERROR;
                *enc_type = (uint8_t)temp_enc_type;
            }
        }
    }

    return ESP01_OK;
}

/* ========================= FONCTIONS UTILITAIRES (AFFICHAGE, FORMATAGE) ========================= */

/**
 * @brief  Retourne une chaîne lisible pour un mode WiFi.
 * @param  mode Mode à convertir.
 * @return Chaîne descriptive.
 */
const char *esp01_wifi_mode_to_string(uint8_t mode)
{
    switch (mode)
    {
    case 1:
        return "Station (STA)";
    case 2:
        return "Point d'accès (AP)";
    case 3:
        return "Station + Point d'accès (STA+AP)";
    default:
        return "Mode inconnu";
    }
}

/**
 * @brief  Retourne une chaîne lisible pour le type d'encryptage.
 * @param  code Code d'encryptage.
 * @return Chaîne descriptive.
 */
const char *esp01_encryption_to_string(const char *code)
{
    VALIDATE_PARAM(code, NULL);

    if (strcmp(code, "0") == 0)
        return "Ouvert (pas de sécurité) - Aucun chiffrement, réseau non protégé";
    else if (strcmp(code, "1") == 0)
        return "WEP - Wired Equivalent Privacy (obsolète, déconseillé)";
    else if (strcmp(code, "2") == 0)
        return "WPA_PSK - WiFi Protected Access avec clé pré-partagée";
    else if (strcmp(code, "3") == 0)
        return "WPA2_PSK - WiFi Protected Access 2 avec clé pré-partagée (recommandé)";
    else if (strcmp(code, "4") == 0)
        return "WPA_WPA2_PSK - Mode mixte (compatible avec WPA et WPA2)";
    else if (strcmp(code, "5") == 0)
        return "WPA2_Enterprise - Authentification via serveur RADIUS (entreprises)";
    else if (strcmp(code, "6") == 0)
        return "WPA3_PSK - WiFi Protected Access 3 avec clé pré-partagée (dernière génération)";
    else if (strcmp(code, "7") == 0)
        return "WPA2_WPA3_PSK - Mode mixte (compatible avec WPA2 et WPA3)";

    return code;
}

/**
 * @brief  Retourne une chaîne lisible pour le statut TCP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_tcp_status_to_string(const char *resp)
{
    VALIDATE_PARAM(resp, NULL);

    static char result[ESP01_MAX_RESP_BUF * 2]; // Buffer plus grand pour éviter la troncature
    result[0] = '\0';

    // Cherche la ligne STATUS:
    char *status_line = strstr(resp, "STATUS:");
    if (!status_line)
        return "Format non reconnu";

    int temp_status_code = 0;
    if (sscanf(status_line, "STATUS:%d", &temp_status_code) != 1)
        return "Format de statut invalide";

    char status_desc[ESP01_SMALL_BUF_SIZE] = {0};
    switch (temp_status_code)
    {
    case 0:
        return "WiFi non initialisé (0)";
    case 1:
        return "WiFi en mode veille (1)";
    case 2:
        return "WiFi connecté au point d'accès (2)"; // Corrigé
    case 3:
        return "WiFi en cours de connexion (3)"; // Valeur correcte pour "en cours de connexion"
    case 4:
        return "WiFi en cours de déconnexion (4)";
    case 5:
        return "WiFi déconnecté (5)";
    default:
        return "État inconnu";
    }

    // Chercher les connexions
    char *conn_line = strstr(resp, "+CIPSTATUS:");
    uint8_t conn_count = 0;
    char conn_detail[ESP01_MAX_RESP_BUF / 2] = {0}; // Un buffer plus petit pour les détails

    while (conn_line && conn_count < 10) // Limiter à 10 connexions max
    {
        conn_count++;
        int temp_link_id, temp_remote_port, temp_local_port, temp_tetype;
        char type[ESP01_SMALL_BUF_SIZE], remote_ip[ESP01_SMALL_BUF_SIZE];

        if (sscanf(conn_line, "+CIPSTATUS:%d,\"%[^\"]\",\"%[^\"]\",%d,%d,%d",
                   &temp_link_id, type, remote_ip, &temp_remote_port, &temp_local_port, &temp_tetype) == 6)
        {
            char temp[ESP01_MAX_RESP_BUF];
            snprintf(temp, sizeof(temp), "\n  Conn #%d: %s vers %s:%d (local:%d, type:%d)",
                     temp_link_id, type, remote_ip, temp_remote_port, temp_local_port, temp_tetype);

            // Vérifier si l'ajout dépasserait le buffer
            size_t current_len = strlen(conn_detail);
            size_t temp_len = strlen(temp);
            size_t space_left = sizeof(conn_detail) - current_len - 1; // -1 pour \0

            if (temp_len < space_left)
                strncat(conn_detail, temp, space_left);
            else
            {
                const char *overflow_msg = "\n  ... et plus";
                if (strlen(overflow_msg) < space_left)
                    strncat(conn_detail, overflow_msg, space_left);
                break;
            }
        }

        conn_line = strstr(conn_line + 1, "+CIPSTATUS:");
    }

    // Utiliser snprintf avec vérification pour éviter la troncature
    snprintf(result, sizeof(result), "Statut: %s (%d)%s%s",
             status_desc, temp_status_code,
             conn_count > 0 ? "\nConnexions actives:" : "",
             conn_detail);

    return result;
}

/**
 * @brief  Retourne une chaîne lisible pour l'état CWSTATE.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_cwstate_to_string(const char *resp)
{
    VALIDATE_PARAM(resp, NULL);

    static char result[ESP01_MAX_RESP_BUF];
    result[0] = '\0';

    char *cwstate = strstr(resp, "+CWSTATE:");
    if (!cwstate)
        return "Format non reconnu";

    int temp_state = -1;
    char ssid[ESP01_MAX_SSID_BUF] = {0};

    // Format: +CWSTATE:state[,"ssid"]
    if (sscanf(cwstate, "+CWSTATE:%d,\"%[^\"]\"", &temp_state, ssid) >= 1)
    {
        const char *status_desc;
        switch (temp_state)
        {
        case 0:
            status_desc = "WiFi non initialisé (0)";
            break;
        case 1:
            status_desc = "WiFi en mode veille (1)";
            break;
        case 2:
            status_desc = "WiFi connecté au point d'accès (2)"; // Corrigé
            break;
        case 3:
            status_desc = "WiFi en cours de connexion (3)"; // Valeur correcte pour "en cours de connexion"
            break;
        case 4:
            status_desc = "WiFi en cours de déconnexion (4)";
            break;
        case 5:
            status_desc = "WiFi déconnecté (5)";
            break;
        default:
            status_desc = "État inconnu";
            break;
        }

        if (ssid[0] != '\0')
        {
            // Si un SSID est présent
            snprintf(result, sizeof(result), "%s - SSID: \"%s\"", status_desc, ssid);
        }
        else
        {
            // Si pas de SSID
            snprintf(result, sizeof(result), "%s", status_desc);
        }

        return result;
    }

    return "Format non reconnu";
}

/**
 * @brief  Récupère la configuration IP actuelle (IP, Gateway, Masque).
 * @param  ip_buf  Buffer pour l'adresse IP.
 * @param  ip_len  Longueur du buffer IP.
 * @param  gw_buf  Buffer pour la Gateway.
 * @param  gw_len  Longueur du buffer Gateway.
 * @param  mask_buf Buffer pour le Masque.
 * @param  mask_len Longueur du buffer Masque.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ip_config(char *ip_buf, size_t ip_len, char *gw_buf, size_t gw_len, char *mask_buf, size_t mask_len)
{
    VALIDATE_PARAM(ip_buf && gw_buf && mask_buf && ip_len > 0 && gw_len > 0 && mask_len > 0, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTA?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        return st;

    if (!esp01_extract_quoted_value(resp, "+CIPSTA:ip:\"", ip_buf, ip_len))
        return ESP01_PARSE_ERROR;
    if (!esp01_extract_quoted_value(resp, "+CIPSTA:gateway:\"", gw_buf, gw_len))
        return ESP01_PARSE_ERROR;
    if (!esp01_extract_quoted_value(resp, "+CIPSTA:netmask:\"", mask_buf, mask_len))
        return ESP01_PARSE_ERROR;

    return ESP01_OK;
}

/**
 * @brief  Retourne une chaîne lisible pour le statut de connexion WiFi.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_connection_status_to_string(const char *resp)
{
    VALIDATE_PARAM(resp, NULL);                                                                           // Vérifie la validité du pointeur
    static char result[ESP01_MAX_RESP_BUF];                                                               // Buffer pour la chaîne résultat
    if (strstr(resp, "No AP"))                                                                            // Si la réponse contient "No AP"
        return "Non connecté";                                                                            // Retourne non connecté
    char ssid[ESP01_MAX_SSID_BUF] = {0};                                                                  // Buffer pour le SSID
    char bssid[ESP01_MAX_MAC_LEN] = {0};                                                                  // Buffer pour le BSSID
    int temp_channel = 0, temp_rssi = 0;                                                                  // Variables pour le canal et le RSSI
    char *cwjap = strstr(resp, "+CWJAP:");                                                                // Cherche le motif +CWJAP:
    if (!cwjap)                                                                                           // Si non trouvé
        return "Format non reconnu";                                                                      // Retourne format non reconnu
    if (sscanf(cwjap, "+CWJAP:\"%[^\"]\",\"%[^\"]\",%d,%d", ssid, bssid, &temp_channel, &temp_rssi) >= 3) // Parse les infos
    {
        snprintf(result, sizeof(result), "Connecté à \"%s\", BSSID: %s, Canal: %d, Signal: %d dBm (%s)",
                 ssid, bssid, temp_channel, temp_rssi, esp01_rf_power_to_string(temp_rssi)); // Formate la chaîne descriptive
        return result;                                                                       // Retourne la chaîne formatée
    }
    return "Format de connexion non reconnu"; // Si parsing échoue
}

/**
 * @brief  Retourne une chaîne lisible pour la puissance RF.
 * @param  rf_dbm Puissance en dBm.
 * @return Chaîne descriptive.
 */
const char *esp01_rf_power_to_string(uint8_t rf_dbm)
{
    static char result[ESP01_SMALL_BUF_SIZE]; // Buffer pour la chaîne résultat
    if (rf_dbm >= -30)                        // Excellent
        snprintf(result, sizeof(result), "%d dBm (Excellent)", rf_dbm);
    else if (rf_dbm >= -67) // Très bon
        snprintf(result, sizeof(result), "%d dBm (Très bon)", rf_dbm);
    else if (rf_dbm >= -70) // Bon
        snprintf(result, sizeof(result), "%d dBm (Bon)", rf_dbm);
    else if (rf_dbm >= -80) // Acceptable
        snprintf(result, sizeof(result), "%d dBm (Acceptable)", rf_dbm);
    else if (rf_dbm >= -90) // Faible
        snprintf(result, sizeof(result), "%d dBm (Faible)", rf_dbm);
    else // Très faible
        snprintf(result, sizeof(result), "%d dBm (Très faible)", rf_dbm);
    return result; // Retourne la chaîne descriptive
}

/**
 * @brief  Retourne une chaîne lisible pour le résultat d'un ping.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_ping_result_to_string(const char *resp)
{
    VALIDATE_PARAM(resp, NULL);                            // Vérifie la validité du pointeur
    static char result[ESP01_MAX_RESP_BUF];                // Buffer pour la chaîne résultat
    char *ping_line = strstr(resp, "+PING:");              // Cherche le motif +PING:
    if (!ping_line)                                        // Si non trouvé
        return "Format de réponse ping non reconnu";       // Retourne format non reconnu
    int temp_time_ms = 0;                                  // Variable pour le temps de ping
    if (sscanf(ping_line, "+PING:%d", &temp_time_ms) == 1) // Parse le temps
    {
        snprintf(result, sizeof(result), "Réponse ping : %d ms", temp_time_ms); // Formate la chaîne descriptive
        return result;                                                          // Retourne la chaîne formatée
    }
    return "Format de réponse ping non reconnu"; // Si parsing échoue
}

/**
 * @brief  Retourne une chaîne lisible pour le résultat de déconnexion AP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_cwqap_to_string(const char *resp)
{
    VALIDATE_PARAM(resp, NULL);        // Vérifie la validité du pointeur
    return "Déconnexion WiFi réussie"; // Retourne la chaîne statique
}

/**
 * @brief  Retourne une chaîne lisible pour la config AP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_ap_config_to_string(const char *resp)
{
    VALIDATE_PARAM(resp, NULL);                                                         // Vérifie la validité du pointeur
    static char result[ESP01_MAX_RESP_BUF];                                             // Buffer pour la chaîne résultat
    result[0] = '\0';                                                                   // Initialise le buffer
    char ssid[ESP01_MAX_SSID_BUF] = {0};                                                // Buffer pour le SSID
    char pwd[ESP01_MAX_PASSWORD_BUF] = {0};                                             // Buffer pour le mot de passe
    int temp_channel = 1, temp_encryption = 0, temp_max_conn = 0, temp_ssid_hidden = 0; // Variables pour les champs AP
    char *cwsap = strstr(resp, "+CWSAP:");                                              // Cherche le motif +CWSAP:
    if (!cwsap)                                                                         // Si non trouvé
        return "Format de configuration AP non reconnu";                                // Retourne format non reconnu
    if (sscanf(cwsap, "+CWSAP:\"%[^\"]\",\"%[^\"]\",%d,%d,%d,%d",
               ssid, pwd, &temp_channel, &temp_encryption, &temp_max_conn, &temp_ssid_hidden) >= 4) // Parse les infos
    {
        snprintf(result, sizeof(result), "AP: SSID=\"%s\", PWD=\"%s\", Canal=%d, Encryption=%d, MaxConn=%d, Caché=%d",
                 ssid, pwd, temp_channel, temp_encryption, temp_max_conn, temp_ssid_hidden); // Formate la chaîne descriptive
        return result;                                                                       // Retourne la chaîne formatée
    }
    return "Format de configuration AP non reconnu"; // Si parsing échoue
}

// ========================= FIN DU MODULE =========================
