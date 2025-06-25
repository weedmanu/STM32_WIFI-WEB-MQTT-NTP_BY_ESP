/**
 ******************************************************************************
 * @file    STM32_WifiESP_WIFI.c
 * @author  manu
 * @version 1.2.1
 * @date    25 juin 2025
 * @brief   Implémentation des fonctions haut niveau WiFi pour ESP01
 *
 * @details
 * Ce fichier source contient l’implémentation des fonctions haut niveau pour le module ESP01 :
 *   - Modes WiFi (STA, AP, STA+AP)
 *   - Scan des réseaux, connexion, déconnexion
 *   - DHCP, IP, MAC, hostname, clients AP
 *   - TCP/IP, ping, statut, multi-connexion
 *   - Fonctions utilitaires d'affichage et de parsing
 *
 * @note
 *   - Nécessite le driver bas niveau STM32_WifiESP.h
 *   - Compatible STM32CubeIDE.
 *   - Toutes les fonctions nécessitent une initialisation préalable du module ESP01.
 ******************************************************************************/

/* ========================== INCLUDES ========================== */
#include "STM32_WifiESP_WIFI.h" // Header du module WiFi haut niveau
#include <string.h>             // Fonctions de manipulation de chaînes
#include <stdio.h>              // Fonctions d'affichage/formatage
#include <stdlib.h>             // Fonctions utilitaires standard

/* ========================== CONSTANTES SPÉCIFIQUES AU MODULE ========================== */
#define ESP01_WIFI_SCAN_TIMEOUT 10000    // Timeout scan WiFi (ms)
#define ESP01_WIFI_CONNECT_TIMEOUT 15000 // Timeout connexion WiFi (ms)

/* ========================= WRAPPERS AT & HELPERS ASSOCIÉS (par commande AT) ========================= */
/**
 * @defgroup ESP01_WIFI_AT_WRAPPERS Wrappers AT et helpers associés (par commande AT)
 * @brief  Fonctions exposant chaque commande AT WiFi à l'utilisateur, avec leurs helpers de parsing/affichage.
 *
 * | Commande AT         | Wrapper principal(s)                        | Helpers associés                        | Description courte                  |
 * |---------------------|---------------------------------------------|-----------------------------------------|-------------------------------------|
 * | AT+CWMODE?          | esp01_get_wifi_mode                         | esp01_wifi_mode_to_string               | Récupère le mode WiFi               |
 * | AT+CWMODE=          | esp01_set_wifi_mode                         | esp01_wifi_mode_to_string               | Définit le mode WiFi                |
 * | AT+CWLAP            | esp01_scan_networks                         | esp01_parse_cwlap_line                  | Scan réseaux WiFi                   |
 * |                     |                                             | esp01_network_to_string                 |                                     |
 * | AT+CWJAP=           | esp01_connect_wifi                          | INUTILE                                 | Connexion à un réseau WiFi (simple) |
 * |                     | esp01_connect_wifi_config                   | INUTILE                                 | Connexion à un réseau WiFi (config avancée) |
 * | AT+CWQAP            | esp01_disconnect_wifi                       | esp01_cwqap_to_string                   | Déconnexion du WiFi                 |
 * | AT+CWDHCP=          | esp01_set_dhcp                              | INUTILE                                 | Active/désactive le DHCP            |
 * | AT+CWDHCP?          | esp01_get_dhcp                              | INUTILE                                 | Récupère l'état du DHCP             |
 * | AT+CIPSTA?          | esp01_get_current_ip                        | INUTILE                                 | Récupère l'adresse IP               |
 * |                     | esp01_get_ip_config                         | INUTILE                                 | Récupère la configuration IP        |
 * | AT+CIPSTAMAC?       | esp01_get_mac                               | INUTILE                                 | Récupère l'adresse MAC              |
 * | AT+CWHOSTNAME=      | esp01_set_hostname                          | INUTILE                                 | Définit le hostname                 |
 * | AT+CWHOSTNAME?      | esp01_get_hostname                          | INUTILE                                 | Récupère le hostname                |
 * | AT+PING             | esp01_ping                                  | INUTILE                                 | Ping une adresse                    |
 * | AT+CIPSTATUS        | esp01_get_tcp_status                        | esp01_tcp_status_to_string              | Statut TCP                          |
 * | AT+CWSTATE?         | esp01_get_wifi_state                        | esp01_cwstate_to_string                 | État WiFi                           |
 * | AT+CIPAP?           | esp01_get_ap_config                         | esp01_ap_config_to_string               | Config AP                           |
 * | AT+CWSAP=           | esp01_start_ap_config                       | INUTILE                                 | Configure un AP                     |
 * |                     | esp01_set_ap_config                         | INUTILE                                 | Configure SoftAP complet            |
 * | AT+CWJAP?           | esp01_get_connection_info                   | esp01_parse_cwjap_response              | Infos connexion WiFi                |
 * |                     | esp01_get_wifi_connection                   | INUTILE                                 | État de connexion WiFi (brut)       |
 * |                     | esp01_get_connection_status                 | esp01_connection_status_to_string       | Statut connexion WiFi               |
 * | AT+CIPMUX?          | esp01_get_connection_mode                   | INUTILE                                 | Mode de connexion (simple/multi)    |
 * | AT+CWJAP_CUR?       | esp01_get_connected_ap_info                 | INUTILE                                 | Infos AP connecté                   |
 * | AT+CWLIF            | esp01_list_ap_stations                      | esp01_ap_station_to_string              | Liste stations connectées à l’AP    |
 * | AT+CWQIF            | esp01_ap_disconnect_all                     | INUTILE                                 | Déconnexion stations AP             |
 * |                     | esp01_ap_disconnect_station                 | INUTILE                                 | Déconnexion station AP (MAC)        |
 */

/**
 * @brief  Récupère le mode de connexion actuel (simple ou multiple).
 * @param  multi_conn Pointeur vers la variable qui recevra l'état
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 */
ESP01_Status_t esp01_get_connection_mode(uint8_t *multi_conn) // Récupère le mode de connexion (simple ou multiple)
{
    VALIDATE_PARAM(multi_conn, ESP01_INVALID_PARAM); // Vérifie que le pointeur d'entrée est valide

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour stocker la réponse du module

    ESP01_LOG_DEBUG("CIPMUX", "Récupération du mode de connexion...");                                           // Log le début de la récupération du mode
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPMUX?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CIPMUX? et récupère la réponse
    if (st != ESP01_OK)                                                                                          // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("CIPMUX", "Erreur lors de la récupération du mode: %s", esp01_get_error_string(st)); // Log l'erreur de récupération
        return st;                                                                                           // Retourne le code d'erreur
    }

    int32_t mode = 0;                                               // Variable temporaire pour stocker le mode extrait
    if (esp01_parse_int_after(resp, "+CIPMUX:", &mode) != ESP01_OK) // Parse la réponse pour extraire la valeur du mode après "+CIPMUX:"
    {
        ESP01_LOG_ERROR("CIPMUX", "Impossible de parser le mode dans: %s", resp); // Log l'échec du parsing
        return ESP01_PARSE_ERROR;                                                 // Retourne une erreur de parsing
    }

    *multi_conn = (uint8_t)mode;                                                                               // Affecte la valeur extraite au pointeur fourni
    ESP01_LOG_DEBUG("CIPMUX", "Mode de connexion : %s", *multi_conn ? "Multi-connexion" : "Connexion unique"); // Log le mode courant
    return ESP01_OK;                                                                                           // Retourne OK si tout s'est bien passé
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
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour stocker la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CWJAP? et récupère la réponse
    if (st != ESP01_OK)                                                                                         // Vérifie si la commande a échoué
    {
        ESP01_RETURN_ERROR("GET_AP_INFO", st); // Retourne l'erreur si la commande a échoué
    }

    // Utiliser la fonction de parsing existante pour extraire les informations
    // Convertir uint8_t* en uint8_t* pour channel car la fonction esp01_parse_cwjap_response attend uint8_t*
    uint8_t ch_tmp = 0;                                                    // Variable temporaire pour stocker le canal
    st = esp01_parse_cwjap_response(resp,                                  // Appelle la fonction de parsing pour extraire les infos
                                    ssid, ssid ? ESP01_MAX_SSID_BUF : 0,   // Buffer SSID et sa taille si fourni
                                    bssid, bssid ? ESP01_MAX_MAC_LEN : 0,  // Buffer BSSID et sa taille si fourni
                                    channel ? &ch_tmp : NULL, NULL, NULL); // Pointeur vers le canal temporaire si demandé

    // Convertir le résultat uint8_t en uint8_t si nécessaire
    if (st == ESP01_OK && channel != NULL) // Si parsing réussi et channel demandé
    {
        *channel = (uint8_t)ch_tmp; // Affecte la valeur du canal au pointeur fourni
    }

    return st; // Retourne le statut final
} // Fin de esp01_get_connected_ap_info

/**
 * @brief  Récupère le statut de connexion WiFi (connecté ou non).
 * @details
 * Envoie la commande AT+CWJAP? pour vérifier si le module est connecté à un réseau WiFi.
 * Analyse la réponse pour détecter la présence du motif +CWJAP:.
 * @note Retourne ESP01_OK si connecté, ESP01_WIFI_NOT_CONNECTED sinon.
 * @return ESP01_Status_t Code de statut (OK, erreur, etc.)
 */
ESP01_Status_t esp01_get_connection_status(void)
{
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse

    // Envoie la commande AT pour vérifier le statut de connexion
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT et récupère le statut
    if (st != ESP01_OK)                                                                                         // Vérifie si la commande a réussi
    {
        ESP01_LOG_ERROR("STATUS", "Erreur lors de la vérification du statut: %s", esp01_get_error_string(st)); // Log l'erreur
        return st;                                                                                             // Retourne le code d'erreur
    }

    // Vérifie si la réponse contient le motif indiquant une connexion réussie
    if (strstr(resp, "+CWJAP:"))
    {
        ESP01_LOG_DEBUG("STATUS", "WiFi connecté"); // Log la connexion détectée
        return ESP01_OK;                            // Retourne OK si connecté
    }

    ESP01_LOG_WARN("STATUS", "Motif non trouvé : non connecté"); // Log l'absence de connexion
    return ESP01_WIFI_NOT_CONNECTED;                             // Retourne le statut non connecté
}

/**
 * @brief  Récupère le mode WiFi actuel (STA, AP, STA+AP).
 * @details
 * Envoie la commande AT+CWMODE? et parse la réponse pour extraire le mode courant.
 * @param  mode Pointeur vers la variable qui recevra le mode WiFi (1=STA, 2=AP, 3=STA+AP)
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 */
ESP01_Status_t esp01_get_wifi_mode(uint8_t *mode)
{
    VALIDATE_PARAM(mode, ESP01_INVALID_PARAM); // Vérifie que le pointeur d'entrée est valide
    char resp[ESP01_MAX_RESP_BUF] = {0};       // Buffer pour stocker la réponse du module

    // Envoie la commande AT+CWMODE? pour obtenir le mode WiFi courant
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWMODE?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT et récupère le statut
    if (st != ESP01_OK)                                                                                          // Vérifie si la commande a réussi
    {
        ESP01_LOG_ERROR("CWMODE", "Erreur lors de la lecture du mode: %s", esp01_get_error_string(st)); // Log l'erreur
        return st;                                                                                      // Retourne le code d'erreur
    }
    int32_t mode_tmp = 0; // Variable temporaire pour stocker le mode WiFi extrait

    // Parse la réponse pour extraire la valeur du mode après le motif +CWMODE:
    if (esp01_parse_int_after(resp, "+CWMODE:", &mode_tmp) != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWMODE", "Impossible de parser le mode dans: %s", resp); // Log l'échec du parsing
        return ESP01_PARSE_ERROR;                                                 // Retourne une erreur de parsing
    }
    *mode = (uint8_t)mode_tmp;                                                                                // Affecte la valeur extraite au pointeur fourni
    ESP01_LOG_DEBUG("CWMODE", "Mode WiFi actuel: %d (%s)", *mode, esp01_wifi_mode_to_string((uint8_t)*mode)); // Log le mode courant
    return ESP01_OK;                                                                                          // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Définit le mode WiFi (STA, AP, STA+AP).
 * @details
 * Envoie la commande AT+CWMODE= pour changer le mode WiFi du module.
 * @param  mode Mode à appliquer (voir ESP01_WifiMode_t).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_wifi_mode(uint8_t mode)
{
    if (mode < 1 || mode > 3) // Vérifie que le mode est dans l'intervalle valide (1=STA, 2=AP, 3=STA+AP)
    {
        ESP01_LOG_ERROR("CWMODE", "Mode invalide: %d", mode); // Log une erreur si le mode est invalide
        ESP01_RETURN_ERROR("CWMODE", ESP01_INVALID_PARAM);    // Retourne une erreur de paramètre
    }
    char cmd[ESP01_SMALL_BUF_SIZE];                                                                     // Buffer pour la commande AT à envoyer
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);                                                   // Formate la commande AT pour définir le mode
    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                // Buffer pour la réponse du module
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT et récupère le statut
    if (st != ESP01_OK)                                                                                 // Vérifie si la commande a réussi
    {
        ESP01_LOG_ERROR("CWMODE", "Erreur lors de la configuration du mode: %s", esp01_get_error_string(st)); // Log l'erreur
        ESP01_RETURN_ERROR("CWMODE", st);                                                                     // Retourne le code d'erreur
    }
    ESP01_LOG_DEBUG("CWMODE", "Mode WiFi configuré à %d (%s)", mode, esp01_wifi_mode_to_string(mode)); // Log le mode configuré
    return ESP01_OK;                                                                                   // Retourne OK si tout s'est bien passé
}

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
    const char *resp,                               // Réponse brute du module ESP
    char *ssid, size_t ssid_size,                   // Buffer et taille pour le SSID
    char *bssid, size_t bssid_size,                 // Buffer et taille pour le BSSID/MAC AP
    uint8_t *channel, int *rssi, uint8_t *enc_type) // Pointeurs pour canal, RSSI, type d'encryptage
{
    VALIDATE_PARAM(resp, ESP01_INVALID_PARAM); // Vérifie que la réponse n'est pas NULL

    // Format attendu: +CWJAP:"ssid","bssid",channel,rssi,enc_type,...
    const char *cwjap = strstr(resp, "+CWJAP:"); // Cherche le motif "+CWJAP:" dans la réponse
    if (!cwjap)                                  // Si non trouvé
        return ESP01_WIFI_NOT_CONNECTED;         // Retourne statut non connecté

    // Extraction du SSID (entre guillemets)
    if (ssid && ssid_size > 0) // Si le buffer SSID est fourni
    {
        if (!esp01_extract_quoted_value(cwjap, "+CWJAP:", ssid, ssid_size)) // Extrait la valeur entre guillemets après "+CWJAP:"
            return ESP01_PARSE_ERROR;                                       // Retourne erreur si extraction échoue
    }

    // Extraction du BSSID (entre guillemets, après le SSID)
    if (bssid && bssid_size > 0) // Si le buffer BSSID est fourni
    {
        const char *bssid_start = strchr(cwjap + 7, ',');                                     // Cherche la première virgule après "+CWJAP:"
        if (!bssid_start || !esp01_extract_quoted_value(bssid_start, ",", bssid, bssid_size)) // Extrait la valeur entre guillemets après la virgule
            return ESP01_PARSE_ERROR;                                                         // Retourne erreur si extraction échoue
    }

    // Parsing des valeurs numériques
    const char *after_bssid = strstr(cwjap, "\","); // Cherche la fin du BSSID (guillemet et virgule)
    if (after_bssid)                                // Si trouvé
    {
        after_bssid += 2; // Saute le guillemet et la virgule

        // Lecture du canal
        if (channel) // Si le pointeur channel est fourni
        {
            int temp_channel;                                  // Variable temporaire pour le canal
            if (sscanf(after_bssid, "%d", &temp_channel) != 1) // Extrait le canal avec sscanf
                return ESP01_PARSE_ERROR;                      // Retourne erreur si extraction échoue
            *channel = (uint8_t)temp_channel;                  // Stocke la valeur extraite
        }

        // Avance jusqu'à la virgule suivante pour le RSSI
        const char *rssi_pos = strchr(after_bssid, ','); // Cherche la virgule après le canal
        if (rssi_pos && rssi)                            // Si trouvé et pointeur rssi fourni
        {
            int temp_rssi;                                   // Variable temporaire pour le RSSI
            if (sscanf(rssi_pos + 1, "%d", &temp_rssi) != 1) // Extrait le RSSI avec sscanf
                return ESP01_PARSE_ERROR;                    // Retourne erreur si extraction échoue
            *rssi = temp_rssi;                               // Stocke la valeur extraite
        }

        // Si besoin de lire le type d'encryption, continuer le parsing...
        if (enc_type && rssi_pos) // Si pointeur enc_type fourni
        {
            const char *enc_pos = strchr(rssi_pos + 1, ','); // Cherche la virgule après le RSSI
            if (enc_pos)                                     // Si trouvé
            {
                int temp_enc_type;                                  // Variable temporaire pour le type d'encryptage
                if (sscanf(enc_pos + 1, "%d", &temp_enc_type) != 1) // Extrait le type d'encryptage
                    return ESP01_PARSE_ERROR;                       // Retourne erreur si extraction échoue
                *enc_type = (uint8_t)temp_enc_type;                 // Stocke la valeur extraite
            }
        }
    }

    return ESP01_OK; // Retourne OK si tout s'est bien passé
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
    VALIDATE_PARAM(networks && found_networks && max_networks > 0, ESP01_INVALID_PARAM); // Vérifie la validité des pointeurs et du nombre max

    char resp[ESP01_LARGE_RESP_BUF] = {0}; // Buffer pour stocker la réponse du module (grande taille)

    ESP01_LOG_DEBUG("CWLAP", "Scan des réseaux WiFi...");                                                       // Log le début du scan
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWLAP", resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM); // Envoie la commande AT+CWLAP pour scanner les réseaux
    if (st != ESP01_OK)                                                                                         // Vérifie si la commande a réussi
    {
        ESP01_LOG_ERROR("CWLAP", "Erreur lors du scan: %s", esp01_get_error_string(st)); // Log l'erreur
        return st;                                                                       // Retourne le code d'erreur
    }

    *found_networks = 0; // Initialise le compteur de réseaux trouvés à 0
    char *line = resp;   // Pointeur pour parcourir la réponse ligne par ligne

    while (*found_networks < max_networks) // Boucle tant qu'on n'a pas atteint le max
    {
        char *start = strstr(line, "+CWLAP:("); // Cherche le début d'une ligne réseau
        if (!start)                             // Si plus de ligne trouvée, on sort
            break;

        if (esp01_parse_cwlap_line(start, &networks[*found_networks])) // Si le parsing de la ligne réussit
        {
            (*found_networks)++; // Incrémente le compteur de réseaux trouvés
        }
        line = start + 1; // Avance d'un caractère pour continuer la recherche
    }

    ESP01_LOG_DEBUG("CWLAP", "%d réseaux trouvés", *found_networks); // Log le nombre de réseaux trouvés
    return ESP01_OK;                                                 // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Parse une ligne de réponse +CWLAP et remplit une structure réseau.
 * @param  line    Ligne à parser (format: +CWLAP:(...)).
 * @param  network Pointeur vers la structure à remplir.
 * @retval true  Parsing réussi, structure remplie.
 * @retval false Parsing échoué (format inattendu ou pointeur NULL).
 * @details
 *  - Remplit tous les champs de la structure esp01_network_t si présents dans la ligne.
 *  - Gère les champs optionnels (offset, cipher, etc.).
 *  - Utilise esp01_safe_strcpy et esp01_trim_string pour la robustesse.
 */
bool esp01_parse_cwlap_line(const char *line, esp01_network_t *network)
{
    VALIDATE_PARAM(line && network, false); // Vérification des pointeurs

    // Déclaration unique des variables en début de fonction
    int enc = 0, rssi = 0, channel = 0, freq_offset = 0, freqcal_val = 0; // Variables pour les champs numériques
    int pairwise_cipher = 0, group_cipher = 0, bgn = 0, wps = 0;          // Variables pour les champs optionnels
    char ssid[ESP01_MAX_SSID_BUF] = {0};                                  // Buffer temporaire pour le SSID
    char mac[ESP01_MAX_MAC_LEN] = {0};                                    // Buffer temporaire pour la MAC

    // Parsing complet selon la doc officielle
    int n = sscanf(line, "+CWLAP:(%d,\"%[^\"]\",%d,\"%[^\"]\",%d,%d,%d,%d,%d,%d,%d)",
                   &enc, ssid, &rssi, mac, &channel, &freq_offset, &freqcal_val,
                   &pairwise_cipher, &group_cipher, &bgn, &wps); // Extraction des champs de la ligne
    if (n < 5)                                                   // Vérifie que les champs essentiels sont présents
        return false;                                            // Retourne false si parsing incomplet

    esp01_safe_strcpy(network->ssid, ESP01_MAX_SSID_BUF, ssid); // Copie le SSID dans la structure
    esp01_safe_strcpy(network->mac, ESP01_MAX_MAC_LEN, mac);    // Copie la MAC dans la structure
    esp01_trim_string(network->ssid);                           // Nettoie le SSID
    esp01_trim_string(network->mac);                            // Nettoie la MAC
    network->rssi = rssi;                                       // Stocke le RSSI (niveau de signal)
    network->channel = (uint8_t)channel;                        // Stocke le canal
    network->encryption = enc;                                  // Stocke le type d'encryptage

    if (n > 6)                                      // Si le champ freqcal_val est présent
        network->freqcal_val = freqcal_val;         // Stocke freqcal_val
    if (n > 7)                                      // Si le champ pairwise_cipher est présent
        network->pairwise_cipher = pairwise_cipher; // Stocke pairwise_cipher
    if (n > 8)                                      // Si le champ group_cipher est présent
        network->group_cipher = group_cipher;       // Stocke group_cipher
    if (n > 9)                                      // Si le champ bgn est présent
        network->bgn = bgn;                         // Stocke bgn
    if (n > 10)                                     // Si le champ wps est présent
        network->wps = wps;                         // Stocke wps

    return true; // Retourne true si parsing réussi
}

/**
 * @brief  Active ou désactive le DHCP.
 * @param  enable true pour activer, false pour désactiver.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_dhcp(bool enable) // Fonction pour activer ou désactiver le DHCP
{
    char cmd[ESP01_MAX_CMD_BUF];                                  // Déclare un buffer pour la commande AT à envoyer
    snprintf(cmd, sizeof(cmd), "AT+CWDHCP=1,%d", enable ? 1 : 0); // Formate la commande AT selon l'état demandé (1=activer, 0=désactiver)

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Déclare un buffer pour la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT et récupère le statut
    if (st != ESP01_OK)                                                                                 // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("CWDHCP", "Erreur lors de la configuration du DHCP: %s", esp01_get_error_string(st)); // Log l'erreur de configuration DHCP
        return st;                                                                                            // Retourne le code d'erreur
    }

    ESP01_LOG_DEBUG("CWDHCP", "DHCP %s", enable ? "activé" : "désactivé"); // Log l'état du DHCP (activé/désactivé)
    return ESP01_OK;                                                       // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Récupère l'état du DHCP.
 * @param  enabled Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_dhcp(bool *enabled) // Fonction pour récupérer l'état du DHCP (activé/désactivé)
{
    VALIDATE_PARAM(enabled, ESP01_INVALID_PARAM); // Vérifie que le pointeur de sortie est valide

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour stocker la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWDHCP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CWDHCP? et récupère la réponse
    if (st != ESP01_OK)                                                                                          // Si la commande AT a échoué
    {
        ESP01_LOG_ERROR("CWDHCP", "Erreur lors de la lecture de l'état DHCP: %s", esp01_get_error_string(st)); // Log l'erreur de lecture du DHCP
        return st;                                                                                             // Retourne le code d'erreur
    }

    int32_t dhcp_mode = 0;                                               // Variable temporaire pour stocker le mode DHCP extrait
    if (esp01_parse_int_after(resp, "+CWDHCP:", &dhcp_mode) != ESP01_OK) // Parse la réponse pour extraire la valeur du mode DHCP
    {
        ESP01_LOG_ERROR("CWDHCP", "Impossible de parser l'état DHCP dans: %s", resp); // Log l'échec du parsing
        return ESP01_FAIL;                                                            // Retourne une erreur générique
    }

    *enabled = (dhcp_mode & 1) != 0;                                         // Affecte true si le bit 0 est à 1 (DHCP activé), false sinon
    ESP01_LOG_DEBUG("CWDHCP", "DHCP %s", *enabled ? "activé" : "désactivé"); // Log l'état du DHCP
    return ESP01_OK;                                                         // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Déconnecte le module du réseau WiFi courant.
 * @details
 *  - Envoie la commande AT+CWQAP pour forcer la déconnexion.
 *  - Retourne ESP01_OK si la déconnexion est réussie, code d'erreur sinon.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_disconnect_wifi(void)
{
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse du module

    ESP01_LOG_DEBUG("CWQAP", "Déconnexion du WiFi...");                                                        // Log le début de la déconnexion
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWQAP", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CWQAP et récupère le statut

    if (st != ESP01_OK) // Si la commande AT a échoué
    {
        ESP01_LOG_ERROR("CWQAP", "Erreur lors de la déconnexion: %s", esp01_get_error_string(st)); // Log l'erreur de déconnexion
        return st;                                                                                 // Retourne le code d'erreur
    }

    ESP01_LOG_DEBUG("CWQAP", "Déconnecté avec succès"); // Log le succès de la déconnexion
    return ESP01_OK;                                    // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Connecte au WiFi (mode simple).
 * @param  ssid     SSID du réseau à rejoindre (doit être non NULL et non vide).
 * @param  password Mot de passe du réseau (doit être non NULL et non vide).
 * @retval ESP01_OK                Connexion réussie.
 * @retval ESP01_WIFI_TIMEOUT      Délai de connexion dépassé.
 * @retval ESP01_WIFI_WRONG_PASSWORD Mot de passe incorrect.
 * @retval ESP01_WIFI_AP_NOT_FOUND Point d'accès introuvable.
 * @retval ESP01_WIFI_CONNECT_FAIL Échec de connexion générique.
 * @retval ESP01_FAIL              Autre erreur (commande AT ou parsing).
 * @retval ESP01_INVALID_PARAM     Paramètre d'entrée invalide.
 * @details
 *  - Envoie la commande AT+CWJAP pour se connecter à un réseau WiFi.
 *  - Analyse la réponse pour détecter les erreurs spécifiques (+CWJAP:1 à +CWJAP:4).
 *  - Log chaque étape et chaque erreur pour faciliter le debug.
 *  - Utilise un timeout long pour laisser le temps à la connexion.
 */
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
    char cmd[ESP01_MAX_CMD_BUF] = {0};   // Buffer pour la commande AT
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse du module

    VALIDATE_PARAM(ssid && strlen(ssid) > 0, ESP01_INVALID_PARAM);         // Vérifie la validité du SSID
    VALIDATE_PARAM(password && strlen(password) > 0, ESP01_INVALID_PARAM); // Vérifie la validité du mot de passe

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password); // Construit la commande AT

    ESP01_LOG_DEBUG("WIFI", "Connexion au réseau %s...", ssid);                                            // Log la tentative de connexion
    ESP01_Status_t status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_LONG); // Envoie la commande AT

    if (status != ESP01_OK) // Si la commande AT échoue
    {
        // Analyse des erreurs spécifiques de la réponse
        if (strstr(resp, "+CWJAP:1")) // Timeout
        {
            ESP01_LOG_ERROR("WIFI", "Échec de connexion: Délai dépassé"); // Log timeout
            return ESP01_WIFI_TIMEOUT;                                    // Retourne code timeout
        }
        else if (strstr(resp, "+CWJAP:2")) // Mot de passe incorrect
        {
            ESP01_LOG_ERROR("WIFI", "Échec de connexion: Mot de passe incorrect"); // Log mauvais mot de passe
            return ESP01_WIFI_WRONG_PASSWORD;                                      // Retourne code mauvais mot de passe
        }
        else if (strstr(resp, "+CWJAP:3")) // AP introuvable
        {
            ESP01_LOG_ERROR("WIFI", "Échec de connexion: AP introuvable"); // Log AP non trouvé
            return ESP01_WIFI_AP_NOT_FOUND;                                // Retourne code AP non trouvé
        }
        else if (strstr(resp, "+CWJAP:4")) // Échec générique
        {
            ESP01_LOG_ERROR("WIFI", "Échec de connexion: Échec de connexion"); // Log échec générique
            return ESP01_WIFI_CONNECT_FAIL;                                    // Retourne code échec générique
        }
        ESP01_LOG_ERROR("WIFI", "Échec de connexion WiFi: %s", resp); // Log toute autre erreur
        return ESP01_FAIL;                                            // Retourne code d'échec générique
    }

    ESP01_LOG_DEBUG("WIFI", "Connexion réussie au réseau %s", ssid); // Log le succès
    return ESP01_OK;                                                 // Retourne OK si tout s'est bien passé
} // Fin de esp01_connect_wifi

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
 * @brief  Récupère l'adresse IP courante.
 * @param  ip_buf  Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len)
{
    VALIDATE_PARAM(ip_buf && buf_len >= ESP01_MAX_IP_LEN, ESP01_INVALID_PARAM); // Vérifie que le buffer IP est valide et assez grand

    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                       // Buffer pour stocker la réponse du module
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIFSR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CIFSR pour obtenir l'IP
    if (st != ESP01_OK)                                                                                        // Si la commande AT a échoué
    {
        ESP01_LOG_ERROR("CIFSR", "Erreur lors de la récupération de l'IP: %s", esp01_get_error_string(st)); // Log l'erreur
        ESP01_RETURN_ERROR("CIFSR", st);                                                                    // Retourne le code d'erreur
    }

    // Recherche de l'adresse IP dans la réponse (STA ou AP)
    char *ip_start = strstr(resp, "+CIFSR:STAIP,\""); // Cherche le motif STAIP dans la réponse
    if (!ip_start)                                    // Si non trouvé
        ip_start = strstr(resp, "+CIFSR:APIP,\"");    // Cherche le motif APIP dans la réponse
    if (!ip_start)                                    // Si toujours pas trouvé
    {
        ESP01_LOG_ERROR("CIFSR", "Format de réponse non reconnu: %s", resp); // Log une erreur de format
        ESP01_RETURN_ERROR("CIFSR", ESP01_FAIL);                             // Retourne une erreur générique
    }

    ip_start = strchr(ip_start, '\"'); // Cherche le premier guillemet
    if (!ip_start)                     // Si non trouvé
    {
        ESP01_LOG_ERROR("CIFSR", "IP mal formatée dans la réponse: %s", resp); // Log une erreur de format
        ESP01_RETURN_ERROR("CIFSR", ESP01_FAIL);                               // Retourne une erreur générique
    }
    ip_start++; // Passe le guillemet ouvrant

    char *ip_end = strchr(ip_start, '\"'); // Cherche le guillemet fermant
    if (!ip_end)                           // Si non trouvé
    {
        ESP01_LOG_ERROR("CIFSR", "IP mal formatée dans la réponse: %s", resp); // Log une erreur de format
        ESP01_RETURN_ERROR("CIFSR", ESP01_FAIL);                               // Retourne une erreur générique
    }

    size_t ip_len = ip_end - ip_start;                            // Calcule la longueur de l'IP extraite
    if (esp01_check_buffer_size(ip_len, buf_len - 1) != ESP01_OK) // Vérifie que le buffer de sortie est assez grand
    {
        ESP01_LOG_ERROR("CIFSR", "Buffer trop petit pour stocker l'IP (longueur: %u)", ip_len); // Log une erreur de taille
        ESP01_RETURN_ERROR("CIFSR", ESP01_BUFFER_OVERFLOW);                                     // Retourne une erreur de dépassement de buffer
    }

    char temp_ip[ESP01_MAX_IP_LEN] = {0};                 // Buffer temporaire pour l'IP
    memcpy(temp_ip, ip_start, ip_len);                    // Copie l'IP extraite dans le buffer temporaire
    temp_ip[ip_len] = '\0';                               // Termine la chaîne par un caractère nul
    esp01_safe_strcpy(ip_buf, buf_len, temp_ip);          // Copie l'IP dans le buffer de sortie de façon sécurisée
    esp01_trim_string(ip_buf);                            // Nettoie la chaîne IP (enlève espaces éventuels)
    ESP01_LOG_DEBUG("CIFSR", "IP récupérée: %s", ip_buf); // Log l'IP récupérée
    return ESP01_OK;                                      // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Récupère le RSSI courant (niveau de signal).
 * @param  rssi Pointeur vers la variable de sortie (dBm).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_rssi(int *rssi) // Fonction pour récupérer le RSSI courant (niveau de signal)
{
    VALIDATE_PARAM(rssi, ESP01_INVALID_PARAM); // Vérifie que le pointeur rssi est valide

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour stocker la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CWJAP? pour obtenir les infos de connexion
    if (st != ESP01_OK)                                                                                         // Vérifie si la commande a échoué
        return st;                                                                                              // Retourne le code d'erreur si échec

    // Format: +CWJAP:"ssid","bssid",channel,rssi,...
    const char *cwjap = strstr(resp, "+CWJAP:"); // Cherche la ligne contenant "+CWJAP:"
    if (!cwjap)                                  // Si la ligne n'est pas trouvée
        return ESP01_WIFI_NOT_CONNECTED;         // Retourne le statut non connecté

    // On cherche la 4ème virgule (après le canal)
    const char *pos = cwjap;  // Initialise le pointeur de recherche
    uint8_t count = 0;        // Compteur de virgules trouvées
    while (*pos && count < 3) // Boucle jusqu'à la 4ème virgule
    {
        if (*pos == ',') // Si le caractère courant est une virgule
            count++;     // Incrémente le compteur
        pos++;           // Avance le pointeur
    }

    // Maintenant pos pointe au début du RSSI
    if (*pos) // Si on n'est pas à la fin de la chaîne
    {
        int temp_rssi;                          // Variable temporaire pour stocker le RSSI
        if (sscanf(pos, "%d", &temp_rssi) == 1) // Tente d'extraire le RSSI avec sscanf
        {
            *rssi = temp_rssi;                                         // Stocke le RSSI dans la variable de sortie
            ESP01_LOG_DEBUG("RSSI", "Force du signal: %d dBm", *rssi); // Log le RSSI récupéré
            return ESP01_OK;                                           // Retourne OK si tout s'est bien passé
        }
    }

    return ESP01_PARSE_ERROR; // Retourne une erreur de parsing si extraction échouée
}

/**
 * @brief  Récupère l'adresse MAC courante.
 * @details
 * Envoie la commande AT+CIFSR pour obtenir l'adresse MAC du module ESP01.
 * Analyse la réponse pour extraire l'adresse MAC au format texte.
 * Utilise des fonctions utilitaires pour la robustesse (taille de buffer, copie sécurisée, nettoyage).
 * @param  mac_buf Buffer de sortie pour l'adresse MAC (doit être de taille suffisante).
 * @param  buf_len Taille du buffer de sortie.
 * @return ESP01_Status_t ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len)
{
    VALIDATE_PARAM(mac_buf && buf_len >= ESP01_MAX_MAC_LEN, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIFSR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("MAC", "Erreur lors de la récupération de la MAC: %s", esp01_get_error_string(st));
        ESP01_RETURN_ERROR("MAC", st);
    }

    // Recherche de la MAC STA ou AP selon le mode
    char *mac_start = strstr(resp, "+CIFSR:STAMAC,\"");
    if (!mac_start)
        mac_start = strstr(resp, "+CIFSR:APMAC,\"");
    if (!mac_start)
    {
        ESP01_LOG_ERROR("MAC", "Format de réponse non reconnu: %s", resp);
        ESP01_RETURN_ERROR("MAC", ESP01_FAIL);
    }

    mac_start = strchr(mac_start, '\"');
    if (!mac_start)
    {
        ESP01_LOG_ERROR("MAC", "MAC mal formatée dans la réponse: %s", resp);
        ESP01_RETURN_ERROR("MAC", ESP01_FAIL);
    }
    mac_start++; // Passe le guillemet ouvrant

    char *mac_end = strchr(mac_start, '\"');
    if (!mac_end)
    {
        ESP01_LOG_ERROR("MAC", "MAC mal formatée dans la réponse: %s", resp);
        ESP01_RETURN_ERROR("MAC", ESP01_FAIL);
    }

    size_t mac_len = mac_end - mac_start;
    if (esp01_check_buffer_size(mac_len, buf_len - 1) != ESP01_OK)
    {
        ESP01_LOG_ERROR("MAC", "Buffer trop petit pour stocker la MAC (longueur: %u)", mac_len);
        ESP01_RETURN_ERROR("MAC", ESP01_BUFFER_OVERFLOW);
    }

    char temp_mac[ESP01_MAX_MAC_LEN] = {0};
    memcpy(temp_mac, mac_start, mac_len);
    temp_mac[mac_len] = '\0';
    esp01_safe_strcpy(mac_buf, buf_len, temp_mac);
    esp01_trim_string(mac_buf);
    ESP01_LOG_DEBUG("MAC", "MAC récupérée: %s", mac_buf);
    return ESP01_OK;
}

/**
 * @brief  Définit le hostname du module.
 * @details
 * Envoie la commande AT+CWHOSTNAME="..." pour configurer le hostname du module ESP01.
 * @param  hostname Chaîne hostname à appliquer (doit être non NULL).
 * @return ESP01_Status_t ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_set_hostname(const char *hostname)
{
    VALIDATE_PARAM(hostname, ESP01_INVALID_PARAM); // Vérifie la validité du pointeur d'entrée

    char cmd[ESP01_MAX_CMD_BUF];                                  // Buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+CWHOSTNAME=\"%s\"", hostname); // Formate la commande AT

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("HOSTNAME", "Erreur lors de la configuration du hostname: %s", esp01_get_error_string(st)); // Log l'erreur
        return st;                                                                                                  // Retourne le code d'erreur
    }

    ESP01_LOG_DEBUG("HOSTNAME", "Hostname configuré: %s", hostname); // Log le hostname configuré
    return ESP01_OK;                                                 // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Récupère le hostname du module.
 * @details
 * Envoie la commande AT+CWHOSTNAME? pour lire le hostname courant du module ESP01.
 * Analyse la réponse pour extraire le hostname.
 * @param  hostname Buffer de sortie pour le hostname (doit être de taille suffisante).
 * @param  len      Taille du buffer de sortie.
 * @return ESP01_Status_t ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_hostname(char *hostname, size_t len)
{
    VALIDATE_PARAM(hostname && len >= ESP01_MAX_HOSTNAME_LEN, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWHOSTNAME?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("HOSTNAME", "Erreur lors de la récupération du hostname: %s", esp01_get_error_string(st)); // Log l'erreur
        return st;                                                                                                 // Retourne le code d'erreur
    }

    // Recherche du hostname dans la réponse
    char *start = strstr(resp, "+CWHOSTNAME:"); // Cherche le motif dans la réponse
    if (!start)
    {
        ESP01_LOG_ERROR("HOSTNAME", "Format de réponse non reconnu: %s", resp); // Log l'erreur
        return ESP01_FAIL;                                                      // Retourne une erreur générique
    }

    start += 12; // Passe au-delà de "+CWHOSTNAME:"

    // Enlève les espaces avant
    while (*start && (*start == ' ' || *start == '\r' || *start == '\n')) // Ignore les espaces et les retours à la ligne
        start++;                                                          // Avance jusqu'au premier caractère non espace

    // Copie jusqu'au \r\n ou fin de chaîne
    size_t i = 0;                                                           // Compteur pour le buffer de sortie
    while (i < len - 1 && start[i] && start[i] != '\r' && start[i] != '\n') // Copie les caractères jusqu'à la fin de la chaîne ou jusqu'à un retour à la ligne
    {
        hostname[i] = start[i]; // Copie le caractère dans le buffer de sortie
        i++;                    // Incrémente le compteur
    }
    hostname[i] = '\0';
    esp01_trim_string(hostname);                                    // Nettoie la chaîne extraite
    ESP01_LOG_DEBUG("HOSTNAME", "Hostname récupéré: %s", hostname); // Log le hostname récupéré
    return ESP01_OK;                                                // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Récupère le statut TCP du module.
 * @details
 * Envoie la commande AT+CIPSTATUS pour obtenir le statut TCP/IP du module ESP01.
 * Copie la réponse brute dans le buffer de sortie fourni.
 * @param  out      Buffer de sortie pour la réponse brute.
 * @param  out_size Taille du buffer de sortie.
 * @return ESP01_Status_t ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                            // Buffer pour la réponse du module
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTATUS", resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM); // Envoie la commande AT

    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CIPSTATUS", "Erreur lors de la lecture du statut TCP: %s", esp01_get_error_string(st)); // Log l'erreur
        return st;                                                                                               // Retourne le code d'erreur
    }

    if (strlen(resp) >= out_size)
    {
        ESP01_LOG_ERROR("CIPSTATUS", "Buffer trop petit pour stocker le résultat"); // Log l'erreur
        return ESP01_BUFFER_OVERFLOW;                                               // Retourne une erreur de buffer
    }

    esp01_safe_strcpy(out, out_size, resp); // Copie la réponse dans le buffer de sortie

    ESP01_LOG_DEBUG("CIPSTATUS", "Statut TCP récupéré avec succès"); // Log le succès
    return ESP01_OK;                                                 // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Effectue un ping vers une adresse et récupère le temps de réponse.
 * @details
 * Envoie la commande AT+PING="..." pour tester la connectivité réseau vers une adresse cible.
 * Analyse la réponse pour extraire le temps de réponse en millisecondes.
 * @param  host    Adresse à pinger (nom de domaine ou IP).
 * @param  time_ms Pointeur vers la variable de sortie pour le temps de réponse (en ms), peut être NULL.
 * @return ESP01_Status_t ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_ping(const char *host, int *time_ms)
{
    VALIDATE_PARAM(host, ESP01_INVALID_PARAM); // Vérifie la validité du paramètre host
    char cmd[64];                              // Buffer pour la commande AT
    char resp[ESP01_MAX_RESP_BUF] = {0};       // Buffer pour la réponse du module

    snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"", host); // Formate la commande AT+PING avec l'hôte cible
    ESP01_LOG_DEBUG("PING", "Ping vers %s...", host);   // Log la tentative de ping

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000); // Envoie la commande AT et attend la réponse
    if (st != ESP01_OK)                                                                  // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("PING", "Erreur ping: %s", esp01_get_error_string(st)); // Log l'erreur de ping
        return st;                                                              // Retourne le code d'erreur
    }

    // Cherche "+PING:<temps>"
    char *start = strstr(resp, "+PING:"); // Recherche le motif "+PING:" dans la réponse
    if (start && time_ms)                 // Si trouvé et time_ms non NULL
    {
        int val = -1;                             // Variable temporaire pour stocker le temps
        if (sscanf(start, "+PING:%d", &val) == 1) // Extrait la valeur du temps de ping
        {
            *time_ms = val;                                       // Stocke le temps dans la variable de sortie
            ESP01_LOG_DEBUG("PING", "Réponse ping : %d ms", val); // Log le temps de réponse
        }
        else
        {
            ESP01_LOG_WARN("PING", "Format de réponse ping inattendu: %s", resp); // Log un avertissement si parsing échoué
        }
    }
    else
    {
        ESP01_LOG_WARN("PING", "Motif +PING: non trouvé dans la réponse: %s", resp); // Log un avertissement si motif non trouvé
    }
    return ESP01_OK; // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Récupère l'état de connexion WiFi (brut).
 * @details
 * Envoie la commande AT+CWJAP? pour obtenir l'état de connexion WiFi du module ESP01.
 * Copie la réponse brute dans le buffer de sortie fourni.
 * @param  out      Buffer de sortie pour la réponse brute.
 * @param  out_size Taille du buffer de sortie.
 * @return ESP01_Status_t ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_wifi_connection(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("CWJAP?", "Erreur lors de la lecture de la connexion: %s", esp01_get_error_string(st)); // Log l'erreur
        return st;                                                                                              // Retourne le code d'erreur
    }

    if (strlen(resp) >= out_size)
    {
        ESP01_LOG_ERROR("CWJAP?", "Buffer trop petit pour stocker le résultat"); // Log l'erreur
        return ESP01_BUFFER_OVERFLOW;                                            // Retourne une erreur de buffer
    }

    esp01_safe_strcpy(out, out_size, resp); // Copie la réponse dans le buffer de sortie

    ESP01_LOG_DEBUG("CWJAP?", "État de connexion WiFi récupéré"); // Log le succès
    return ESP01_OK;                                              // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Récupère l'état WiFi (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_state(char *out, size_t out_size) // Récupère l'état WiFi (brut)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres
    char resp[ESP01_MAX_RESP_BUF] = {0};                      // Buffer pour la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWSTATE?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CWSTATE? pour lire l'état WiFi
    if (st != ESP01_OK)                                                                                           // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("CWSTATE", "Erreur lors de la lecture de l'état: %s", esp01_get_error_string(st)); // Log l'erreur de lecture
        return st;                                                                                         // Retourne le code d'erreur
    }

    if (strlen(resp) >= out_size) // Vérifie si le buffer de sortie est assez grand
    {
        ESP01_LOG_ERROR("CWSTATE", "Buffer trop petit pour stocker le résultat"); // Log l'erreur de taille de buffer
        return ESP01_BUFFER_OVERFLOW;                                             // Retourne une erreur de dépassement de buffer
    }

    esp01_safe_strcpy(out, out_size, resp); // Copie la réponse dans le buffer de sortie

    ESP01_LOG_DEBUG("CWSTATE", "État WiFi récupéré"); // Log le succès de la récupération
    return ESP01_OK;                                  // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Récupère la config AP (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ap_config(char *out, size_t out_size) // Récupère la configuration AP (brut)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres
    char resp[ESP01_MAX_RESP_BUF] = {0};                      // Buffer pour la réponse du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWSAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CWSAP? pour lire la config AP
    if (st != ESP01_OK)                                                                                         // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("CWSAP", "Erreur lors de la lecture de la config AP: %s", esp01_get_error_string(st)); // Log l'erreur de lecture
        return st;                                                                                             // Retourne le code d'erreur
    }

    if (strlen(resp) >= out_size) // Vérifie si le buffer de sortie est assez grand
    {
        ESP01_LOG_ERROR("CWSAP", "Buffer trop petit pour stocker le résultat"); // Log l'erreur de taille de buffer
        return ESP01_BUFFER_OVERFLOW;                                           // Retourne une erreur de dépassement de buffer
    }

    esp01_safe_strcpy(out, out_size, resp); // Copie la réponse dans le buffer de sortie

    ESP01_LOG_DEBUG("CWSAP", "Configuration AP récupérée"); // Log le succès de la récupération
    return ESP01_OK;                                        // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Configure un AP (SoftAP) avec tous les paramètres.
 * @details
 * Envoie la commande AT+CWSAP pour configurer le SSID, mot de passe, canal, chiffrement, nombre max de connexions et visibilité SSID.
 * @param ssid        SSID de l'AP (8-32 caractères ASCII).
 * @param password    Mot de passe (8-64 caractères ASCII).
 * @param channel     Canal (1-14).
 * @param encryption  Méthode de chiffrement (0:OPEN, 2:WPA_PSK, 3:WPA2_PSK, 4:WPA_WPA2_PSK).
 * @param max_conn    Nombre max de stations (1-10), 0 pour valeur par défaut.
 * @param ssid_hidden 0: SSID visible, 1: caché, -1 pour valeur par défaut.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_ap_config(const char *ssid, const char *password, uint8_t channel, uint8_t encryption, uint8_t max_conn, uint8_t ssid_hidden)
{
    VALIDATE_PARAM(ssid && password && channel >= 1 && channel <= 14 && encryption >= 0 && encryption <= 4 && max_conn >= 1 && max_conn <= 10 && (ssid_hidden == 0 || ssid_hidden == 1), ESP01_INVALID_PARAM); // Vérifie la validité des paramètres d'entrée
    char cmd[ESP01_MAX_CMD_BUF];                                                                                                                                                                               // Déclare un buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",%d,%d,%d,%d", ssid, password, channel, encryption, max_conn, ssid_hidden);                                                                              // Formate la commande AT+CWSAP avec tous les paramètres
    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                                                                                                                       // Déclare un buffer pour la réponse du module
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);                                                                                                       // Envoie la commande AT et récupère le statut
    if (st != ESP01_OK)                                                                                                                                                                                        // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("CWSAP", "Erreur config AP: %s", esp01_get_error_string(st)); // Log l'erreur de configuration AP
        return st;                                                                    // Retourne le code d'erreur
    }
    ESP01_LOG_DEBUG("CWSAP", "AP configuré: SSID=%s, Ch=%d, Enc=%d, Max=%d, Caché=%d", ssid, channel, encryption, max_conn, ssid_hidden); // Log le succès de la configuration AP
    return ESP01_OK;                                                                                                                      // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Configure un AP (SoftAP) avec les paramètres de base.
 * @details
 * Envoie la commande AT+CWSAP pour configurer le SSID, mot de passe, canal et chiffrement du SoftAP.
 * @param  ssid      SSID de l'AP (8-32 caractères ASCII).
 * @param  password  Mot de passe (8-64 caractères ASCII).
 * @param  channel   Canal (1-14).
 * @param  encryption Méthode de chiffrement (0:OPEN, 2:WPA_PSK, 3:WPA2_PSK, 4:WPA_WPA2_PSK).
 * @return ESP01_Status_t
 *
 * @note Pour configurer tous les paramètres avancés (max_conn, ssid_hidden), utiliser esp01_set_ap_config.
 */
ESP01_Status_t esp01_start_ap_config(const char *ssid, const char *password, uint8_t channel, uint8_t encryption)
{
    VALIDATE_PARAM(ssid && password && channel >= 1 && channel <= 14 && encryption >= 0 && encryption <= 4, ESP01_INVALID_PARAM); // Vérifie les paramètres d'entrée
    char cmd[ESP01_MAX_CMD_BUF];                                                                                                  // Déclare un buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",%d,%d", ssid, password, channel, encryption);                              // Formate la commande AT+CWSAP avec les paramètres
    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                                          // Déclare un buffer pour la réponse du module
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);                          // Envoie la commande AT et récupère le statut
    if (st != ESP01_OK)                                                                                                           // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("CWSAP", "Erreur configuration AP: %s", esp01_get_error_string(st)); // Log l'erreur de configuration
        return st;                                                                           // Retourne le code d'erreur
    }
    ESP01_LOG_DEBUG("CWSAP", "AP configuré: SSID=%s, Canal=%d, Encryption=%d (%s)", ssid, channel, encryption, esp01_encryption_to_string(encryption)); // Log le succès de la configuration
    return ESP01_OK;                                                                                                                                    // Retourne OK si tout s'est bien passé
}

/**
 * @brief Liste les stations connectées à l'ESP SoftAP.
 * @details
 * Envoie la commande AT+CWLIF et parse la réponse pour extraire les IP/MAC des stations connectées.
 * @param stations Tableau de structures à remplir.
 * @param max_stations Taille du tableau.
 * @param found_stations Nombre de stations trouvées (en sortie).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_list_ap_stations(esp01_ap_station_t *stations, uint8_t max_stations, uint8_t *found)
{
    VALIDATE_PARAM(stations && found && max_stations > 0, ESP01_INVALID_PARAM);                                // Vérifie la validité des pointeurs et du nombre max
    char resp[ESP01_LARGE_RESP_BUF] = {0};                                                                     // Buffer pour stocker la réponse du module
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWLIF", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CWLIF pour lister les stations
    if (st != ESP01_OK)                                                                                        // Vérifie si la commande a échoué
        return st;                                                                                             // Retourne le code d'erreur si échec
    *found = 0;                                                                                                // Initialise le compteur de stations trouvées à 0
    char *line = strtok(resp, "\r\n");                                                                         // Découpe la réponse ligne par ligne
    while (line && *found < max_stations)                                                                      // Boucle tant qu'il y a des lignes et qu'on n'a pas atteint le max
    {
        if (strncmp(line, "+CWLIF:", 7) == 0) // Vérifie si la ligne commence par "+CWLIF:"
        {
            char ip[ESP01_MAX_IP_LEN] = {0}, mac[ESP01_MAX_MAC_LEN] = {0}; // Buffers temporaires pour IP et MAC
            if (sscanf(line, "+CWLIF:%15[^,],%17[^\r\n]", ip, mac) == 2)   // Extrait l'IP et la MAC de la ligne
            {
                esp01_safe_strcpy(stations[*found].ip, ESP01_MAX_IP_LEN, ip);    // Copie l'IP dans la structure de sortie
                esp01_safe_strcpy(stations[*found].mac, ESP01_MAX_MAC_LEN, mac); // Copie la MAC dans la structure de sortie
                (*found)++;                                                      // Incrémente le compteur de stations trouvées
            }
        }
        line = strtok(NULL, "\r\n"); // Passe à la ligne suivante
    }
    ESP01_LOG_DEBUG("CWLIF", "%d stations trouvées", *found); // Log le nombre de stations trouvées
    return ESP01_OK;                                          // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Génère une chaîne lisible pour une station connectée à l’AP.
 * @param  station Pointeur sur la structure station.
 * @param  buf     Buffer de sortie.
 * @param  buflen  Taille du buffer de sortie.
 * @return ESP01_OK si succès, ESP01_ERROR sinon.
 */
ESP01_Status_t esp01_ap_station_to_string(const esp01_ap_station_t *station, char *buf, size_t buflen)
{
    VALIDATE_PARAM(station && buf && buflen >= 32, ESP01_INVALID_PARAM);
    int n = snprintf(buf, buflen, "IP: %s | MAC: %s", station->ip, station->mac);
    if (n < 0 || (size_t)n >= buflen)
        return ESP01_BUFFER_OVERFLOW;
    return ESP01_OK;
}

/**
 * @brief Déconnecte toutes les stations du SoftAP.
 * @details
 * Envoie la commande AT+CWQIF pour forcer la déconnexion de toutes les stations connectées à l'AP.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_ap_disconnect_all(void) // Déclare la fonction pour déconnecter toutes les stations du SoftAP
{
    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                       // Déclare un buffer pour la réponse du module
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWQIF", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+CWQIF pour déconnecter toutes les stations
    if (st != ESP01_OK)                                                                                        // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("CWQIF", "Erreur déconnexion AP: %s", esp01_get_error_string(st)); // Log l'erreur de déconnexion
        return st;                                                                         // Retourne le code d'erreur
    }
    ESP01_LOG_DEBUG("CWQIF", "Toutes les stations déconnectées"); // Log le succès de la déconnexion
    return ESP01_OK;                                              // Retourne OK si tout s'est bien passé
}

/**
 * @brief Déconnecte une station précise du SoftAP par son adresse MAC.
 * @details
 * Envoie la commande AT+CWQIF=<mac> pour déconnecter une station spécifique.
 * @param mac Adresse MAC de la station à déconnecter (format XX:XX:XX:XX:XX:XX).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_ap_disconnect_station(const char *mac) // Déclare la fonction pour déconnecter une station AP par son adresse MAC
{
    VALIDATE_PARAM(mac && strlen(mac) == 17, ESP01_INVALID_PARAM);                                      // Vérifie que le pointeur MAC est valide et que la longueur est correcte (17 caractères)
    char cmd[ESP01_SMALL_BUF_SIZE];                                                                     // Déclare un buffer pour la commande AT à envoyer
    snprintf(cmd, sizeof(cmd), "AT+CWQIF=%s", mac);                                                     // Formate la commande AT+CWQIF avec l'adresse MAC cible
    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                // Déclare un buffer pour la réponse du module
    ESP01_LOG_DEBUG("CWQIF", "Déconnexion de la station %s...", mac);                                   // Log la tentative de déconnexion de la station
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT et récupère le statut
    if (st != ESP01_OK)                                                                                 // Vérifie si la commande a échoué
    {
        ESP01_LOG_ERROR("CWQIF", "Erreur déconnexion station %s: %s", mac, esp01_get_error_string(st)); // Log l'erreur de déconnexion
        return st;                                                                                      // Retourne le code d'erreur
    }
    ESP01_LOG_DEBUG("CWQIF", "Station %s déconnectée", mac); // Log le succès de la déconnexion
    return ESP01_OK;                                         // Retourne OK si tout s'est bien passé
}

/**
 * @brief Génère une chaîne lisible résumant toutes les infos d'un réseau WiFi scanné.
 * @note Utiliser uniquement cette fonction pour l'affichage d'un réseau scanné (ne pas faire de printf manuel ailleurs).
 *
 * Format typique :
 *   SSID: <ssid>, RSSI: <rssi> dBm, Sécurité: <encryption>
 */
ESP01_Status_t esp01_network_to_string(const esp01_network_t *net, char *buf, size_t buflen)
{
    VALIDATE_PARAM(net && buf && buflen >= 64, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres d'entrée
    int enc_code = net->encryption;                                  // Récupère le code d'encryptage du réseau
    const char *enc_str = esp01_encryption_to_string(enc_code);      // Convertit le code d'encryptage en chaîne lisible
    int n = snprintf(buf, buflen,                                    // Formate la chaîne de sortie dans le buffer
                     "SSID: %s, RSSI: %d dBm, Sécurité: %s",         // Format d'affichage
                     net->ssid, net->rssi, enc_str);                 // Valeurs à afficher
    if (n < 0 || (size_t)n >= buflen)                                // Vérifie si le buffer est assez grand ou si une erreur est survenue
    {
        ESP01_LOG_WARN("WIFI", "Buffer trop petit ou erreur de formatage pour l'affichage réseau"); // Log un avertissement en cas de problème
        return ESP01_FAIL;                                                                          // Retourne une erreur
    }
    ESP01_LOG_DEBUG("WIFI", "%s", buf); // Log la chaîne générée pour debug
    return ESP01_OK;                    // Retourne OK si tout s'est bien passé
}

// ========================= OUTILS DE PARSING & CONVERSION =========================

/**
 * @brief  Retourne une chaîne lisible pour un mode WiFi.
 * @param  mode Mode à convertir.
 * @return Chaîne descriptive.
 */
const char *esp01_wifi_mode_to_string(uint8_t mode) // Retourne une chaîne lisible pour un mode WiFi
{
    switch (mode) // Sélectionne selon la valeur du mode
    {
    case 1:
        return "Station (STA)"; // Mode station (client WiFi)
    case 2:
        return "Point d'accès (AP)"; // Mode point d'accès (AP)
    case 3:
        return "Station + Point d'accès (STA+AP)"; // Mode mixte STA+AP
    default:
        return "Mode inconnu"; // Mode non reconnu
    }
}

/**
 * @brief  Retourne une chaîne lisible pour le type d'encryptage.
 * @param  code Code d'encryptage.
 * @return Chaîne descriptive.
 */
const char *esp01_encryption_to_string(int code) // Retourne une chaîne lisible pour le type d'encryptage
{
    switch (code) // Sélectionne selon le code d'encryptage
    {
    case 0:
        return "Ouvert (pas de sécurité) - Aucun chiffrement, réseau non protégé"; // Aucun chiffrement
    case 1:
        return "WEP - Wired Equivalent Privacy (obsolète, déconseillé)"; // WEP (obsolète)
    case 2:
        return "WPA_PSK - WiFi Protected Access avec clé pré-partagée"; // WPA_PSK
    case 3:
        return "WPA2_PSK - WiFi Protected Access 2 avec clé pré-partagée (recommandé)"; // WPA2_PSK
    case 4:
        return "WPA_WPA2_PSK - Mode mixte (compatible avec WPA et WPA2)"; // WPA/WPA2 mixte
    case 5:
        return "WPA2_Enterprise - Authentification via serveur RADIUS (entreprises)"; // WPA2 Enterprise
    case 6:
        return "WPA3_PSK - WiFi Protected Access 3 avec clé pré-partagée (dernière génération)"; // WPA3_PSK
    case 7:
        return "WPA2_WPA3_PSK - Mode mixte (compatible avec WPA2 et WPA3)"; // WPA2/WPA3 mixte
    default:
        return "Type d'encryptage inconnu"; // Code inconnu
    }
}

/**
 * @brief  Retourne une chaîne lisible pour le statut TCP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_tcp_status_to_string(const char *resp) // Fonction pour convertir le statut TCP en chaîne lisible
{
    VALIDATE_PARAM(resp, NULL); // Vérifie la validité du pointeur d'entrée

    static char result[ESP01_MAX_RESP_BUF * 2]; // Buffer pour la chaîne résultat
    result[0] = '\0';                           // Initialise le buffer

    // Recherche du statut global
    char *status_line = strstr(resp, "STATUS:"); // Cherche la ligne contenant "STATUS:"
    if (!status_line)                            // Si non trouvée
        return "Format non reconnu";             // Retourne une erreur de format

    int stat = -1;                                    // Variable pour stocker le code de statut
    if (sscanf(status_line, "STATUS:%d", &stat) != 1) // Parse le code de statut
        return "Format de statut invalide";           // Retourne une erreur de parsing

    // Traduction officielle du statut
    const char *stat_desc = NULL; // Pointeur pour la description du statut
    switch (stat)                 // Sélectionne la description selon le code
    {
    case 0:
        stat_desc = "Non initialisé (0) : L'interface station ESP n'est pas initialisée."; // Statut 0
        break;
    case 1:
        stat_desc = "Initialisé (1) : L'interface station ESP est initialisée, mais aucune connexion Wi-Fi n'est démarrée."; // Statut 1
        break;
    case 2:
        stat_desc = "Connecté (2) : Connecté à un AP et adresse IP obtenue."; // Statut 2
        break;
    case 3:
        stat_desc = "Transmission active (3) : Une transmission TCP/SSL a été créée."; // Statut 3
        break;
    case 4:
        stat_desc = "Toutes connexions fermées (4) : Toutes les connexions TCP/UDP/SSL sont fermées."; // Statut 4
        break;
    case 5:
        stat_desc = "Connexion Wi-Fi en cours ou perdue (5) : Connexion Wi-Fi démarrée mais non connectée à un AP ou déconnectée."; // Statut 5
        break;
    default:
        stat_desc = "État inconnu"; // Statut inconnu
        break;
    }

    // Ajout du statut global
    snprintf(result, sizeof(result), "Statut global : %s. ", stat_desc); // Formate la description globale

    // Recherche et affichage des connexions actives
    char *conn_line = strstr(resp, "+CIPSTATUS:"); // Cherche la première ligne de connexion
    uint8_t conn_count = 0;                        // Compteur de connexions trouvées
    while (conn_line && conn_count < 10)           // Boucle sur chaque connexion (max 10)
    {
        // Déclaration des variables AVANT le if (C99)
        int link_id = 0, remote_port = 0, local_port = 0, tetype = 0; // Variables pour les champs de connexion
        char type[8] = {0};                                           // Type de connexion (TCP/UDP)
        char remote_ip[32] = {0};                                     // Adresse IP distante

        // Parse la ligne de connexion
        if (sscanf(conn_line, "+CIPSTATUS:%d,\"%[^\"]\",\"%[^\"]\",%d,%d,%d",
                   &link_id, type, remote_ip, &remote_port, &local_port, &tetype) == 6) // Extraction des champs
        {
            const char *role = (tetype == 0) ? "client" : (tetype == 1) ? "serveur"
                                                                        : "?"; // Détermine le rôle
            char temp[ESP01_MAX_RESP_BUF / 4];                                 // Buffer temporaire pour la ligne
            snprintf(temp, sizeof(temp),
                     "  Conn #%d : %s vers %s:%d (local:%d, %s)\n",
                     link_id, type, remote_ip, remote_port, local_port, role); // Formate la ligne de connexion
            strncat(result, temp, sizeof(result) - strlen(result) - 1);        // Ajoute au résultat
            conn_count++;                                                      // Incrémente le compteur
        }
        // Recherche de la prochaine connexion
        conn_line = strstr(conn_line + 1, "+CIPSTATUS:"); // Passe à la ligne suivante
    }
    if (conn_count == 0)                                                                    // Si aucune connexion trouvée
        strncat(result, "Aucune connexion active.\n", sizeof(result) - strlen(result) - 1); // Ajoute un message d'absence de connexion
    return result;                                                                          // Retourne la chaîne descriptive finale
}

/**
 * @brief  Retourne une chaîne lisible pour l'état CWSTATE.
 * @details
 * Décode la réponse brute de la commande AT+CWSTATE? pour fournir une description humaine de l'état Wi-Fi.
 * @param  resp Réponse brute de la commande AT+CWSTATE?
 * @return Chaîne descriptive de l'état Wi-Fi.
 *
 * Codes d'état officiels (ESP-AT):
 *   0: ESP station n'a pas démarré de connexion Wi-Fi.
 *   1: Connecté à un AP, mais pas d'adresse IPv4.
 *   2: Connecté à un AP, adresse IPv4 obtenue.
 *   3: Connexion/reconnexion en cours.
 *   4: Déconnecté du Wi-Fi.
 *   <ssid>: SSID du point d'accès cible (optionnel).
 */
const char *esp01_cwstate_to_string(const char *resp)
{
    VALIDATE_PARAM(resp, NULL); // Vérifie la validité du pointeur d'entrée

    static char result[ESP01_MAX_RESP_BUF]; // Buffer pour la chaîne résultat
    result[0] = '\0';                       // Initialise le buffer

    char *cwstate = strstr(resp, "+CWSTATE:"); // Cherche le motif +CWSTATE: dans la réponse
    if (!cwstate)                              // Si non trouvé
        return "Format non reconnu";           // Retourne format non reconnu

    int temp_state = -1;                 // Variable pour stocker l'état temporaire
    char ssid[ESP01_MAX_SSID_BUF] = {0}; // Buffer pour le SSID (max 32 caractères)

    // Format: +CWSTATE:<state>,"ssid"
    int n = sscanf(cwstate, "+CWSTATE:%d,\"%[^\"]\"", &temp_state, ssid); // Parse la réponse pour obtenir l'état et le SSID
    if (n < 1)                                                            // Si l'état n'est pas trouvé
        return "Format non reconnu";                                      // Retourne format non reconnu

    const char *status_desc; // Variable pour la description de l'état
    switch (temp_state)      // Décode l'état
    {
    case 0:                                                                             // Station non connectée
        status_desc = "0: Station non connectée (aucune tentative de connexion Wi-Fi)"; // Description de l'état 0
        break;                                                                          // Station connectée mais pas d'adresse IPv4
    case 1:                                                                             // Station connectée mais pas d'adresse IPv4
        status_desc = "1: Connecté à un AP, pas d'adresse IPv4 (DHCP en attente)";      // Description de l'état 1
        break;                                                                          // Station connectée avec adresse IPv4
    case 2:                                                                             // Station connectée avec adresse IPv4
        status_desc = "2: Connecté à un AP, adresse IPv4 obtenue";                      // Description de l'état 2
        break;                                                                          // Connexion/reconnexion en cours
    case 3:                                                                             // Connexion/reconnexion en cours
        status_desc = "3: Connexion ou reconnexion en cours";                           // Description de l'état 3
        break;                                                                          // Déconnecté du Wi-Fi
    case 4:                                                                             // Déconnecté du Wi-Fi
        status_desc = "4: Déconnecté du Wi-Fi";                                         // Description de l'état 4
        break;                                                                          // État inconnu ou non géré
    default:                                                                            // État inconnu ou non géré
        status_desc = "État inconnu";                                                   // Description de l'état inconnu
        break;                                                                          // État inconnu ou non géré
    }

    if (n == 2 && ssid[0] != '\0') // Si un SSID a été extrait
    {
        // Si un SSID est présent
        snprintf(result, sizeof(result), "%s - SSID: \"%s\"", status_desc, ssid); // Formate la chaîne descriptive
    }
    else
    {
        // Si pas de SSID
        snprintf(result, sizeof(result), "%s", status_desc); // Formate la chaîne descriptive sans SSID
    }

    return result; // Retourne la chaîne formatée
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
    VALIDATE_PARAM(ip_buf && gw_buf && mask_buf && ip_len > 0 && gw_len > 0 && mask_len > 0, ESP01_INVALID_PARAM); // Vérifie la validité des pointeurs et tailles

    char resp[ESP01_MAX_RESP_BUF] = {0};                                                                         // Buffer pour stocker la réponse du module
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIPSTA?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT pour obtenir la config IP
    if (st != ESP01_OK)                                                                                          // Vérifie si la commande a réussi
        return st;                                                                                               // Retourne le code d'erreur si échec

    if (!esp01_extract_quoted_value(resp, "+CIPSTA:ip:\"", ip_buf, ip_len))          // Extrait l'adresse IP du buffer
        return ESP01_PARSE_ERROR;                                                    // Retourne une erreur de parsing si échec
    if (!esp01_extract_quoted_value(resp, "+CIPSTA:gateway:\"", gw_buf, gw_len))     // Extrait la gateway du buffer
        return ESP01_PARSE_ERROR;                                                    // Retourne une erreur de parsing si échec
    if (!esp01_extract_quoted_value(resp, "+CIPSTA:netmask:\"", mask_buf, mask_len)) // Extrait le masque réseau du buffer
        return ESP01_PARSE_ERROR;                                                    // Retourne une erreur de parsing si échec

    return ESP01_OK; // Retourne OK si tout s'est bien passé
}

/**
 * @brief  Retourne une chaîne lisible pour le statut de connexion WiFi.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_connection_status_to_string(const char *resp)
{
    VALIDATE_PARAM(resp, NULL);                                                            // Vérifie que le pointeur d'entrée n'est pas NULL
    static char result[ESP01_MAX_RESP_BUF];                                                // Buffer statique pour la chaîne résultat
    char ssid[ESP01_MAX_SSID_BUF] = {0};                                                   // Buffer pour le SSID
    char bssid[ESP01_MAX_MAC_LEN] = {0};                                                   // Buffer pour le BSSID/MAC
    int channel = 0, rssi = 0;                                                             // Variables pour le canal et le RSSI
    int pci_en = -1, reconn_interval = -1, listen_interval = -1, scan_mode = -1, pmf = -1; // Variables pour les champs avancés

    // Isole la ligne +CWJAP:
    const char *start = strstr(resp, "+CWJAP:"); // Cherche le motif "+CWJAP:" dans la réponse
    if (!start)                                  // Si non trouvé
    {
        if (strstr(resp, "No AP"))                // Si la réponse contient "No AP"
            return "Non connecté";                // Retourne "Non connecté"
        return "Format de connexion non reconnu"; // Sinon, retourne format non reconnu
    }
    // Copie la ligne utile sans \r\n ni OK
    char line[128] = {0};                                                      // Buffer temporaire pour la ligne utile
    size_t i = 0;                                                              // Compteur d'index
    while (*start && *start != '\r' && *start != '\n' && i < sizeof(line) - 1) // Parcourt la ligne jusqu'à \r, \n ou fin de buffer
        line[i++] = *start++;                                                  // Copie le caractère courant dans le buffer
    line[i] = '\0';                                                            // Termine la chaîne

    int n = sscanf(line, "+CWJAP:\"%[^\"]\",\"%[^\"]\",%d,%d,%d,%d,%d,%d,%d", // Parse les champs de la ligne
                   ssid, bssid, &channel, &rssi, &pci_en, &reconn_interval,   // Extraction du SSID, BSSID, canal, RSSI, etc.
                   &listen_interval, &scan_mode, &pmf);

    esp01_trim_string(ssid);  // Nettoie le SSID
    esp01_trim_string(bssid); // Nettoie le BSSID

    if (n >= 4) // Si au moins SSID, BSSID, canal et RSSI sont présents
    {
        snprintf(result, sizeof(result),    // Formate la chaîne principale
                 "Connecté à \"%s\"\n"      // Affiche le SSID
                 "  BSSID: %s\n"            // Affiche le BSSID
                 "  Canal: %d\n"            // Affiche le canal
                 "  Signal: %d dBm (%s)\n", // Affiche le RSSI et sa description
                 ssid, bssid, channel, rssi, esp01_rf_power_to_string(rssi));
        if (n >= 5)                                                            // Si PCI Auth présent
            snprintf(result + strlen(result), sizeof(result) - strlen(result), // Ajoute la ligne PCI Auth
                     "  PCI Auth: %d (%s)\n", pci_en,
                     (pci_en == 0) ? "tous AP (OPEN/WEP inclus)" : (pci_en == 1) ? "tous sauf OPEN/WEP"
                                                                                 : "inconnu");
        if (n >= 6)                                                            // Si reconn_interval présent
            snprintf(result + strlen(result), sizeof(result) - strlen(result), // Ajoute la ligne reconn_interval
                     "  Reconn. interval: %d s (%s)\n", reconn_interval,
                     (reconn_interval == 0) ? "pas de reconnexion" : (reconn_interval >= 1 && reconn_interval <= 7200) ? "reconnexion auto"
                                                                                                                       : "inconnu");
        if (n >= 7)                                                            // Si listen_interval présent
            snprintf(result + strlen(result), sizeof(result) - strlen(result), // Ajoute la ligne listen_interval
                     "  Listen interval: %d (AP beacon intervals)\n", listen_interval);
        if (n >= 8)                                                            // Si scan_mode présent
            snprintf(result + strlen(result), sizeof(result) - strlen(result), // Ajoute la ligne scan_mode
                     "  Scan mode: %d (%s)\n", scan_mode,
                     (scan_mode == 0) ? "scan rapide (1er AP trouvé)" : (scan_mode == 1) ? "scan tous canaux (meilleur signal)"
                                                                                         : "inconnu");
        if (n >= 9)                                                            // Si pmf présent
            snprintf(result + strlen(result), sizeof(result) - strlen(result), // Ajoute la ligne PMF
                     "  PMF: %d (%s)\n", pmf,
                     (pmf == 0) ? "PMF désactivé" : (pmf == 1) ? "PMF capable"
                                                : (pmf == 2)   ? "PMF requis"
                                                               : "inconnu");
        return result; // Retourne la chaîne descriptive finale
    }
    return "Format de connexion non reconnu"; // Si parsing échoué, retourne format non reconnu
}

/**
 * @brief  Retourne une chaîne lisible pour la puissance RF.
 * @param  rf_dbm Puissance en dBm (valeur signée).
 * @return Chaîne descriptive.
 */
const char *esp01_rf_power_to_string(int rf_dbm)
{
    static char result[ESP01_SMALL_BUF_SIZE];                             // Buffer pour la chaîne résultat
    if (rf_dbm >= -30)                                                    // Excellent
        snprintf(result, sizeof(result), "%d dBm (Excellent)", rf_dbm);   // Prépare la chaîne descriptive
    else if (rf_dbm >= -67)                                               // Très bon
        snprintf(result, sizeof(result), "%d dBm (Très bon)", rf_dbm);    // Prépare la chaîne descriptive
    else if (rf_dbm >= -70)                                               // Bon
        snprintf(result, sizeof(result), "%d dBm (Bon)", rf_dbm);         // Prépare la chaîne descriptive
    else if (rf_dbm >= -80)                                               // Acceptable
        snprintf(result, sizeof(result), "%d dBm (Acceptable)", rf_dbm);  // Prépare la chaîne descriptive
    else if (rf_dbm >= -90)                                               // Faible
        snprintf(result, sizeof(result), "%d dBm (Faible)", rf_dbm);      // Prépare la chaîne descriptive
    else                                                                  // Très faible
        snprintf(result, sizeof(result), "%d dBm (Très faible)", rf_dbm); // Prépare la chaîne descriptive
    return result;                                                        // Retourne la chaîne descriptive
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
        const char *enc = esp01_encryption_to_string(temp_encryption);
        snprintf(result, sizeof(result), "AP: SSID=\"%s\", PWD=\"%s\", Canal=%d, Encryption=%s, MaxConn=%d, Caché=%d",
                 ssid, pwd, temp_channel, enc, temp_max_conn, temp_ssid_hidden); // Formate la chaîne descriptive
        return result;                                                           // Retourne la chaîne formatée
    }
    return "Format de configuration AP non reconnu"; // Si parsing échoue
}

// ========================= FIN DU MODULE =========================
