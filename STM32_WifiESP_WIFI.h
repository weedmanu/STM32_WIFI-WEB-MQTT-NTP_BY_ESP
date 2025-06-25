/**
 ******************************************************************************
 * @file    STM32_WifiESP_WIFI.h
 * @author  manu
 * @version 1.2.1
 * @date    25 juin 2025
 * @brief   Fonctions haut niveau WiFi pour ESP01 (scan, mode, DHCP, connexion, etc)
 *
 * @details
 * Ce header regroupe toutes les fonctions de gestion WiFi haut niveau du module ESP01,
 * nécessitant une connexion ou une configuration réseau : scan, modes, DHCP, IP, MAC,
 * hostname, ping, TCP/IP, AP, etc.
 *
 * @note
 * - Nécessite le driver bas niveau STM32_WifiESP.h
 * - Compatible STM32CubeIDE.
 * - Toutes les fonctions ici nécessitent une initialisation préalable du module ESP01.
 *
 * ========================= SOMMAIRE DES SECTIONS =========================
 *   - INCLUDES
 *   - DEFINES (constantes WiFi)
 *   - TYPES & STRUCTURES
 *   - VARIABLES GLOBALES EXTERNES
 *   - WRAPPERS AT & HELPERS (par commande AT)
 *   - OUTILS UTILITAIRES (si besoin)
 * ========================================================================
 ******************************************************************************/

#ifndef STM32_WIFIESP_WIFI_H_
#define STM32_WIFIESP_WIFI_H_

/* ========================== INCLUDES ========================== */
#include "STM32_WifiESP.h" // Driver bas niveau requis
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* =========================== DEFINES ========================== */
// ----------- CONSTANTES WI-FI -----------
#define ESP01_MAX_SSID_LEN 32                               // Longueur max d'un SSID WiFi
#define ESP01_MAX_SSID_BUF (ESP01_MAX_SSID_LEN + 1)         // Taille buffer SSID (avec \0)
#define ESP01_MAX_PASSWORD_LEN 64                           // Longueur max d'un mot de passe WiFi
#define ESP01_MAX_PASSWORD_BUF (ESP01_MAX_PASSWORD_LEN + 1) // Taille buffer mot de passe (avec \0)
#define ESP01_MAX_ENCRYPTION_LEN 8                          // Longueur max pour le type d'encryptage
#define ESP01_MAX_IP_LEN 32                                 // Longueur max pour une adresse IP
#define ESP01_MAX_HOSTNAME_LEN 64                           // Longueur max pour un hostname
#define ESP01_MAX_MAC_LEN 18                                // Longueur max pour une adresse MAC
#define ESP01_MAX_SCAN_NETWORKS 10                          // Nombre max de réseaux détectés lors d'un scan
#define ESP01_MAX_WIFI_PWD_LEN 64                           // Longueur max pour un mot de passe WiFi
#define ESP01_MAC_BYTES 6                                   // Nombre d'octets dans une adresse MAC

/* =========================== TYPES & STRUCTURES ============================ */
/**
 * @brief Structure représentant un réseau WiFi trouvé lors d'un scan.
 *
 * @details
 * Contient toutes les informations retournées par la commande AT+CWLAP :
 * - SSID, MAC, RSSI (dBm), canal, type d'encryptage, offset de fréquence, etc.
 */
typedef struct
{
    char ssid[ESP01_MAX_SSID_BUF]; // SSID du réseau WiFi
    char mac[ESP01_MAX_MAC_LEN];   // Adresse MAC du réseau
    int rssi;                      // Puissance du signal (dBm)
    uint8_t channel;               // Canal WiFi
    int encryption;                // Type d'encryptage (code numérique, voir esp01_encryption_to_string)
    int freq_offset;               // Offset de fréquence
    int freqcal_val;               // Valeur de calibration de fréquence
    int pairwise_cipher;           // Cipher pairwise
    int group_cipher;              // Cipher group
    int bgn;                       // Indique si b/g/n supporté
    int wps;                       // Indique si WPS est supporté
} esp01_network_t;

/**
 * @brief Modes WiFi disponibles pour l'ESP01.
 *
 * @details
 * - STA : station (client)
 * - AP  : point d'accès
 * - STA+AP : mode mixte
 */
typedef enum
{
    ESP01_WIFI_MODE_STA = 1,   // Mode station (client)
    ESP01_WIFI_MODE_AP = 2,    // Mode point d'accès
    ESP01_WIFI_MODE_STA_AP = 3 // Mode mixte (STA+AP)
} ESP01_WifiMode_t;

/**
 * @brief Structure représentant une station connectée à l’AP (IP + MAC).
 * @details
 * Utilisé pour parser la réponse de AT+CWLIF (SoftAP) : chaque station connectée à l’AP est identifiée par son IP et son adresse MAC.
 */
typedef struct
{
    char ip[ESP01_MAX_IP_LEN];   // Adresse IP de la station connectée
    char mac[ESP01_MAX_MAC_LEN]; // Adresse MAC de la station connectée
} esp01_ap_station_t;

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
 * @brief  Récupère le mode WiFi actuel (AT+CWMODE?)
 * @param  mode Pointeur vers la variable qui recevra le mode WiFi (1=STA, 2=AP, 3=STA+AP)
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 */
ESP01_Status_t esp01_get_wifi_mode(uint8_t *mode);

/**
 * @brief  Définit le mode WiFi (AT+CWMODE=)
 * @param  mode Mode à appliquer (voir ESP01_WifiMode_t).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_wifi_mode(uint8_t mode);

/**
 * @brief  Retourne une chaîne lisible pour un mode WiFi.
 * @param  mode Mode à convertir.
 * @return Chaîne descriptive.
 */
const char *esp01_wifi_mode_to_string(uint8_t mode);

/**
 * @brief  Scanne les réseaux WiFi à proximité (AT+CWLAP)
 * @param  networks      Tableau de structures à remplir.
 * @param  max_networks  Taille du tableau.
 * @param  found_networks Nombre de réseaux trouvés (en sortie).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks);

/**
 * @brief  Parse une ligne de réponse CWLAP et remplit la structure esp01_network_t (helper parsing)
 * @param  line    Ligne à parser (format CWLAP).
 * @param  network Pointeur vers la structure à remplir.
 * @return true si le parsing a réussi, false sinon.
 */
bool esp01_parse_cwlap_line(const char *line, esp01_network_t *network);

/**
 * @brief  Parse la réponse CWJAP et extrait les informations de connexion WiFi (helper parsing)
 * @param  resp       Réponse brute du module ESP.
 * @param  ssid       Buffer pour stocker le SSID (NULL si non requis).
 * @param  ssid_size  Taille du buffer SSID.
 * @param  bssid      Buffer pour stocker le BSSID/MAC AP (NULL si non requis).
 * @param  bssid_size Taille du buffer BSSID.
 * @param  channel    Pointeur pour stocker le canal (NULL si non requis).
 * @param  rssi       Pointeur pour stocker le RSSI (NULL si non requis).
 * @param  enc_type   Pointeur pour stocker le type d'encryption (NULL si non requis).
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_parse_cwjap_response(const char *resp, char *ssid, size_t ssid_size, char *bssid, size_t bssid_size, uint8_t *channel, int *rssi, uint8_t *enc_type);

/**
 * @brief  Connecte au WiFi (mode simple) (AT+CWJAP=)
 * @param  ssid     SSID du réseau.
 * @param  password Mot de passe.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password);

/**
 * @brief  Connecte au WiFi avec configuration avancée (mode, DHCP/IP statique) (AT+CWJAP=)
 * @param  mode      Mode WiFi.
 * @param  ssid      SSID du réseau.
 * @param  password  Mot de passe.
 * @param  use_dhcp  true pour DHCP, false pour IP statique.
 * @param  ip        IP statique (si non DHCP).
 * @param  gateway   Gateway (si non DHCP).
 * @param  netmask   Masque réseau (si non DHCP).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_connect_wifi_config(ESP01_WifiMode_t mode, const char *ssid, const char *password, bool use_dhcp, const char *ip, const char *gateway, const char *netmask);

/**
 * @brief  Récupère la configuration IP courante (IP, gateway, masque)
 * @param  ip_buf   Buffer pour l'IP.
 * @param  ip_len   Taille du buffer IP.
 * @param  gw_buf   Buffer pour la gateway.
 * @param  gw_len   Taille du buffer gateway.
 * @param  mask_buf Buffer pour le masque.
 * @param  mask_len Taille du buffer masque.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ip_config(char *ip_buf, size_t ip_len, char *gw_buf, size_t gw_len, char *mask_buf, size_t mask_len);

/**
 * @brief  Déconnecte du réseau WiFi (AT+CWQAP)
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_disconnect_wifi(void);

/**
 * @brief  Retourne une chaîne lisible pour la réponse de déconnexion (CWQAP)
 * @param  resp Réponse brute du module.
 * @return Chaîne descriptive.
 */
const char *esp01_cwqap_to_string(const char *resp);

/**
 * @brief  Active ou désactive le DHCP (AT+CWDHCP=)
 * @param  enable true pour activer, false pour désactiver.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_dhcp(bool enable);

/**
 * @brief  Récupère l'état du DHCP (AT+CWDHCP?)
 * @param  enabled Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_dhcp(bool *enabled);

/**
 * @brief  Récupère l'adresse IP courante (AT+CIPSTA?)
 * @param  ip_buf  Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);

/**
 * @brief  Récupère l'adresse MAC courante (AT+CIPSTAMAC?)
 * @param  mac_buf Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len);

/**
 * @brief  Définit le hostname du module (AT+CWHOSTNAME=)
 * @param  hostname Chaîne hostname.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_hostname(const char *hostname);

/**
 * @brief  Récupère le hostname du module (AT+CWHOSTNAME?)
 * @param  hostname Buffer de sortie.
 * @param  len      Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_hostname(char *hostname, size_t len);

/**
 * @brief  Récupère le statut TCP (AT+CIPSTATUS)
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size);

/**
 * @brief  Retourne une chaîne lisible pour le statut TCP
 * @param  resp Réponse brute du module.
 * @return Chaîne descriptive.
 */
const char *esp01_tcp_status_to_string(const char *resp);

/**
 * @brief  Effectue un ping vers une adresse et retourne le temps de réponse (AT+PING)
 * @param  host    Adresse à pinger.
 * @param  time_ms Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_ping(const char *host, int *time_ms);

/**
 * @brief  Récupère l'état de connexion WiFi (brut) (AT+CWJAP?)
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_connection(char *out, size_t out_size);

/**
 * @brief  Récupère l'état WiFi (brut) (AT+CWSTATE?)
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_state(char *out, size_t out_size);

/**
 * @brief  Retourne une chaîne lisible pour l'état WiFi (CWSTATE)
 * @param  resp Réponse brute du module.
 * @return Chaîne descriptive.
 */
const char *esp01_cwstate_to_string(const char *resp);

/**
 * @brief  Récupère la config AP (brut) (AT+CIPAP?)
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ap_config(char *out, size_t out_size);

/**
 * @brief  Retourne une chaîne lisible pour la config AP
 * @param  resp Réponse brute du module.
 * @return Chaîne descriptive.
 */
const char *esp01_ap_config_to_string(const char *resp);

/**
 * @brief  Configure un AP (AT+CWSAP=)
 * @param  ssid      SSID.
 * @param  password  Mot de passe.
 * @param  channel   Canal.
 * @param  encryption Type d'encryptage.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_start_ap_config(const char *ssid, const char *password, uint8_t channel, uint8_t encryption);

/**
 * @brief  Récupère les informations complètes de connexion WiFi (AT+CWJAP?)
 * @param  ssid       Buffer pour stocker le SSID.
 * @param  ssid_size  Taille du buffer SSID.
 * @param  bssid      Buffer pour stocker le BSSID/MAC AP.
 * @param  bssid_size Taille du buffer BSSID.
 * @param  channel    Pointeur pour stocker le canal.
 * @param  rssi       Pointeur pour stocker le RSSI.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_connection_info(char *ssid, size_t ssid_size, char *bssid, size_t bssid_size, uint8_t *channel, int *rssi);

/**
 * @brief  Récupère le mode de connexion actuel (simple ou multiple) (AT+CIPMUX?)
 * @param  multi_conn Pointeur vers la variable qui recevra l'état
 *                   - 0: Mode connexion unique
 *                   - 1: Mode multi-connexions
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 */
ESP01_Status_t esp01_get_connection_mode(uint8_t *multi_conn);

/**
 * @brief  Récupère les informations sur le point d'accès connecté (AT+CWJAP_CUR?)
 * @param  ssid    Buffer pour stocker le SSID (NULL si non requis)
 * @param  bssid   Buffer pour stocker le BSSID (NULL si non requis)
 * @param  channel Pointeur pour stocker le canal (NULL si non requis)
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 */
ESP01_Status_t esp01_get_connected_ap_info(char *ssid, char *bssid, uint8_t *channel);

/**
 * @brief  Récupère le statut de connexion WiFi (AT+CWJAP?)
 * @return ESP01_Status_t Code de statut (OK, erreur, etc.)
 */
ESP01_Status_t esp01_get_connection_status(void);

/**
 * @brief  Retourne une chaîne lisible pour le statut de connexion WiFi
 * @param  resp Réponse brute du module.
 * @return Chaîne descriptive.
 */
const char *esp01_connection_status_to_string(const char *resp);

/**
 * @brief  Retourne une chaîne lisible pour le type d'encryptage.
 * @param  code Code d'encryptage.
 * @return Chaîne descriptive.
 */
const char *esp01_encryption_to_string(int code);

/**
 * @brief  Retourne une chaîne lisible pour la puissance RF.
 * @param  rf_dbm Puissance en dBm (valeur signée).
 * @return Chaîne descriptive.
 */
const char *esp01_rf_power_to_string(int rf_dbm);

/**
 * @brief Configure le SoftAP avec tous les paramètres.
 */
ESP01_Status_t esp01_set_ap_config(const char *ssid, const char *password, uint8_t channel, uint8_t encryption, uint8_t max_conn, uint8_t ssid_hidden);

/**
 * @brief Liste les stations connectées à l’AP.
 */
ESP01_Status_t esp01_list_ap_stations(esp01_ap_station_t *stations, uint8_t max_stations, uint8_t *found);

/**
 * @brief Retourne une chaîne lisible pour une station connectée à l’AP.
 * @param station Pointeur vers la structure esp01_ap_station_t.
 * @param buf Buffer de sortie (doit être assez grand).
 * @param buflen Taille du buffer de sortie.
 * @return ESP01_Status_t
 *
 * @details
 * Format typique :
 *   IP: <ip> | MAC: <mac>
 */
ESP01_Status_t esp01_ap_station_to_string(const esp01_ap_station_t *station, char *buf, size_t buflen);

/**
 * @brief Déconnecte toutes les stations de l’AP.
 */
ESP01_Status_t esp01_ap_disconnect_all(void);

/**
 * @brief Déconnecte une station spécifique de l’AP.
 */
ESP01_Status_t esp01_ap_disconnect_station(const char *mac);

/**
 * @brief  Récupère le RSSI courant (niveau de signal).
 * @param  rssi Pointeur vers la variable de sortie (dBm).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_rssi(int *rssi);

/**
 * @brief Génère une chaîne lisible résumant toutes les infos d'un réseau WiFi scanné.
 *
 * @param[in]  net   Pointeur sur la structure réseau à afficher
 * @param[out] buf  Buffer de sortie (doit être assez grand)
 * @param[in]  buflen Taille du buffer de sortie
 * @return ESP01_OK si succès, ESP01_ERROR sinon (paramètre NULL ou buffer trop petit)
 *
 * @details
 * Format typique :
 *   SSID: <ssid> | MAC: <mac> | RSSI: <rssi> dBm | Ch: <channel> | Enc: <encryption>
 */
ESP01_Status_t esp01_network_to_string(const esp01_network_t *net, char *buf, size_t buflen);

#endif /* STM32_WIFIESP_WIFI_H_ */
