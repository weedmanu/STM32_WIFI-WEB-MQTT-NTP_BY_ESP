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
 * - DHCP, IP, MAC, hostname
 * - TCP/IP, ping, statut
 *
 * @note
 * - Nécessite le driver bas niveau STM32_WifiESP.h
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_WIFI_H_ // Protection contre l'inclusion multiple
#define STM32_WIFIESP_WIFI_H_

/* ========================== INCLUDES ========================== */
#include "STM32_WifiESP.h" // Driver bas niveau requis
#include <stdbool.h>       // Types booléens
#include <stddef.h>        // Types de taille (size_t, etc.)
#include <stdint.h>        // Types entiers standard (uint8_t, etc.)

/* =========================== DEFINES ========================== */
#define ESP01_MAX_SSID_LEN 32                       // Longueur max d'un SSID WiFi
#define ESP01_MAX_SSID_BUF (ESP01_MAX_SSID_LEN + 1) // Taille buffer SSID (avec \0)
#define ESP01_MAX_ENCRYPTION_LEN 8                  // Longueur max pour le type d'encryptage
#define ESP01_MAX_IP_LEN 32                         // Longueur max pour une adresse IP
#define ESP01_MAX_HOSTNAME_LEN 64                   // Longueur max pour un hostname
#define ESP01_MAX_MAC_LEN 18                        // Longueur max pour une adresse MAC
#define ESP01_MAX_SCAN_NETWORKS 10                  // Nombre max de réseaux détectés lors d'un scan
#define ESP01_MAX_WIFI_SSID_LEN ESP01_MAX_SSID_LEN  // Longueur max pour un SSID WiFi
#define ESP01_MAX_WIFI_PWD_LEN 64                   // Longueur max pour un mot de passe WiFi
#define ESP01_WIFI_CONNECT_TIMEOUT 15000            // Timeout pour la connexion WiFi en ms
#define ESP01_WIFI_CONFIG_TIMEOUT 2000              // Timeout pour la configuration WiFi en ms
#define ESP01_MAC_BYTES 6                           // Nombre d'octets dans une adresse MAC
/* =========================== TYPES ============================ */
/**
 * @brief Structure représentant un réseau WiFi détecté lors d'un scan.
 */
typedef struct
{
    char ssid[ESP01_MAX_SSID_BUF];             ///< SSID du réseau (max 32 + \0)
    int rssi;                                  ///< Puissance du signal (dBm)
    char encryption[ESP01_MAX_ENCRYPTION_LEN]; ///< Type d'encryptage (texte ou code)
} esp01_network_t;

/**
 * @brief Modes WiFi disponibles pour l'ESP01.
 */
typedef enum
{
    ESP01_WIFI_MODE_STA = 1,   ///< Mode station (client)
    ESP01_WIFI_MODE_AP = 2,    ///< Mode point d'accès (AP)
    ESP01_WIFI_MODE_STA_AP = 3 ///< Mode mixte (station + AP)
} ESP01_WifiMode_t;

/* ======================= FONCTIONS PRINCIPALES ======================= */

/**
 * @brief  Récupère le statut de connexion WiFi.
 * @return ESP01_Status_t Code de statut (OK, erreur, etc.)
 */
ESP01_Status_t esp01_get_connection_status(void);

/**
 * @brief  Récupère le mode WiFi actuel
 * @param  mode Pointeur vers la variable qui recevra le mode WiFi
 *              - 1: Station
 *              - 2: Access Point
 *              - 3: Station + Access Point
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 * @note   Cette fonction envoie la commande AT+CWMODE?
 */
ESP01_Status_t esp01_get_wifi_mode(uint8_t *mode);

/**
 * @brief  Définit le mode WiFi.
 * @param  mode Mode à appliquer (voir ESP01_WifiMode_t).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_wifi_mode(int mode);

/**
 * @brief  Scanne les réseaux WiFi à proximité.
 * @param  networks      Tableau de structures à remplir.
 * @param  max_networks  Taille du tableau.
 * @param  found_networks Nombre de réseaux trouvés (en sortie).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks);

/**
 * @brief  Active ou désactive le DHCP.
 * @param  enable true pour activer, false pour désactiver.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_dhcp(bool enable);

/**
 * @brief  Récupère l'état du DHCP.
 * @param  enabled Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_dhcp(bool *enabled);

/**
 * @brief  Déconnecte du réseau WiFi.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_disconnect_wifi(void);

/**
 * @brief  Connecte au WiFi (mode simple).
 * @param  ssid     SSID du réseau.
 * @param  password Mot de passe.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password);

/**
 * @brief  Récupère l'adresse IP courante.
 * @param  ip_buf  Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);

/**
 * @brief  Récupère le RSSI courant.
 * @param  rssi Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_rssi(int *rssi);

/**
 * @brief  Récupère l'adresse MAC courante.
 * @param  mac_buf Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len);

/**
 * @brief  Définit le hostname du module.
 * @param  hostname Chaîne hostname.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_hostname(const char *hostname);

/**
 * @brief  Récupère le hostname du module.
 * @param  hostname Buffer de sortie.
 * @param  len      Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_hostname(char *hostname, size_t len);

/**
 * @brief  Effectue un ping vers une adresse.
 * @param  host Adresse à pinger.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_ping(const char *host);

/**
 * @brief  Récupère le statut TCP.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size);

/**
 * @brief  Récupère l'état de connexion WiFi (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_connection(char *out, size_t out_size);

/**
 * @brief  Récupère l'état WiFi (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_state(char *out, size_t out_size);

/**
 * @brief  Récupère la config AP (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ap_config(char *out, size_t out_size);

/**
 * @brief  Configure un AP.
 * @param  ssid      SSID.
 * @param  password  Mot de passe.
 * @param  channel   Canal.
 * @param  encryption Type d'encryptage.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_start_ap_config(const char *ssid, const char *password, int channel, int encryption);

/**
 * @brief  Parse la réponse CWJAP et extrait les informations de connexion WiFi
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
ESP01_Status_t esp01_parse_cwjap_response(
    const char *resp,
    char *ssid, size_t ssid_size,
    char *bssid, size_t bssid_size,
    int *channel, int *rssi, int *enc_type);

/**
 * @brief  Récupère le canal WiFi actuel.
 * @param  channel Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_channel(int *channel);

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
    int *channel, int *rssi);

/**
 * @brief  Connecte au WiFi avec configuration avancée.
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
    ESP01_WifiMode_t mode, // Mode WiFi à utiliser
    const char *ssid,      // SSID du réseau
    const char *password,  // Mot de passe
    bool use_dhcp,         // true = DHCP, false = IP statique
    const char *ip,        // IP statique (optionnel)
    const char *gateway,   // Gateway (optionnel)
    const char *netmask);  // Masque réseau (optionnel)

/**
 * @brief  Récupère le mode de connexion actuel (simple ou multiple)
 * @param  multi_conn Pointeur vers la variable qui recevra l'état
 *                   - 0: Mode connexion unique
 *                   - 1: Mode multi-connexions
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 * @note   Cette fonction envoie la commande AT+CIPMUX?
 */
ESP01_Status_t esp01_get_connection_mode(uint8_t *multi_conn);

/**
 * @brief  Récupère les informations sur le point d'accès connecté
 * @param  ssid    Buffer pour stocker le SSID (NULL si non requis)
 * @param  bssid   Buffer pour stocker le BSSID (NULL si non requis)
 * @param  channel Pointeur pour stocker le canal (NULL si non requis)
 * @retval ESP01_Status_t ESP01_OK en cas de succès ou code d'erreur
 * @note   Cette fonction envoie la commande AT+CWJAP?
 */
ESP01_Status_t esp01_get_connected_ap_info(char *ssid, char *bssid, uint8_t *channel);

/* ======================= FONCTIONS UTILITAIRES ======================= */

/**
 * @brief  Retourne une chaîne lisible pour un mode WiFi.
 * @param  mode Mode à convertir.
 * @return Chaîne descriptive.
 */
const char *esp01_wifi_mode_to_string(int mode);

/**
 * @brief  Retourne une chaîne lisible pour le type d'encryptage.
 * @param  code Code d'encryptage.
 * @return Chaîne descriptive.
 */
const char *esp01_encryption_to_string(const char *code);

/**
 * @brief  Retourne une chaîne lisible pour le statut TCP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_tcp_status_to_string(const char *resp);

/**
 * @brief  Retourne une chaîne lisible pour l'état CWSTATE.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_cwstate_to_string(const char *resp);

/**
 * @brief  Retourne une chaîne lisible pour le statut de connexion.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_connection_status_to_string(const char *resp);

/**
 * @brief  Retourne une chaîne lisible pour le résultat d'un ping.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_ping_result_to_string(const char *resp);

/**
 * @brief  Retourne une chaîne lisible pour le résultat de déconnexion AP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_cwqap_to_string(const char *resp);

/**
 * @brief  Retourne une chaîne lisible pour la puissance RF.
 * @param  rf_dbm Puissance en dBm.
 * @return Chaîne descriptive.
 */
const char *esp01_rf_power_to_string(int rf_dbm);

/**
 * @brief  Retourne une chaîne lisible pour la config AP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_ap_config_to_string(const char *resp);

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
ESP01_Status_t esp01_get_ip_config(char *ip_buf, size_t ip_len, char *gw_buf, size_t gw_len, char *mask_buf, size_t mask_len);

#endif /* STM32_WIFIESP_WIFI_H_ */
