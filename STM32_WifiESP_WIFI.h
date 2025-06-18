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
 * @brief  Envoie une commande AT et gère le buffer de réponse.
 * @param  cmd        Commande AT à envoyer.
 * @param  expected   Motif attendu dans la réponse.
 * @param  timeout_ms Timeout en ms.
 * @param  resp       Buffer de réponse.
 * @param  resp_size  Taille du buffer de réponse.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_send_at_with_resp(const char *cmd, const char *expected, int timeout_ms, char *resp, size_t resp_size);

/**
 * @brief  Récupère le statut de connexion WiFi.
 * @return ESP01_Status_t Code de statut (OK, erreur, etc.)
 */
ESP01_Status_t esp01_get_connection_status(void); // Statut de connexion WiFi

/**
 * @brief  Récupère le mode WiFi actuel.
 * @param  mode Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_mode(int *mode); // Récupère le mode WiFi courant

/**
 * @brief  Définit le mode WiFi.
 * @param  mode Mode à appliquer (voir ESP01_WifiMode_t).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_wifi_mode(int mode); // Définit le mode WiFi

/**
 * @brief  Retourne une chaîne lisible pour un mode WiFi.
 * @param  mode Mode à convertir.
 * @return Chaîne descriptive.
 */
const char *esp01_wifi_mode_to_string(int mode); // Mode WiFi en texte

/**
 * @brief  Scanne les réseaux WiFi à proximité.
 * @param  networks      Tableau de structures à remplir.
 * @param  max_networks  Taille du tableau.
 * @param  found_networks Nombre de réseaux trouvés (en sortie).
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks); // Scan réseaux

/**
 * @brief  Affiche les réseaux WiFi scannés dans un buffer texte.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return Pointeur vers le buffer rempli.
 */
char *esp01_print_wifi_networks(char *out, size_t out_size); // Affiche réseaux scannés

/**
 * @brief  Active ou désactive le DHCP.
 * @param  enable true pour activer, false pour désactiver.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_dhcp(bool enable); // Active/Désactive DHCP

/**
 * @brief  Récupère l'état du DHCP.
 * @param  enabled Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_dhcp(bool *enabled); // Récupère l'état DHCP

/**
 * @brief  Déconnecte du réseau WiFi.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_disconnect_wifi(void); // Déconnexion WiFi

/**
 * @brief  Connecte au WiFi (mode simple).
 * @param  ssid     SSID du réseau.
 * @param  password Mot de passe.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password); // Connexion simple

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
 * @brief  Récupère l'adresse IP courante.
 * @param  ip_buf  Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len); // IP courante

/**
 * @brief  Récupère la configuration IP (IP, gateway, masque).
 * @param  ip      Buffer IP.
 * @param  ip_len  Taille buffer IP.
 * @param  gw      Buffer gateway.
 * @param  gw_len  Taille buffer gateway.
 * @param  mask    Buffer masque.
 * @param  mask_len Taille buffer masque.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ip_config(char *ip, size_t ip_len, char *gw, size_t gw_len, char *mask, size_t mask_len); // Config IP

/**
 * @brief  Récupère le RSSI courant.
 * @param  rssi Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_rssi(int *rssi); // RSSI courant

/**
 * @brief  Récupère l'adresse MAC courante.
 * @param  mac_buf Buffer de sortie.
 * @param  buf_len Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_mac(char *mac_buf, size_t buf_len); // MAC courante

/**
 * @brief  Définit le hostname du module.
 * @param  hostname Chaîne hostname.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_hostname(const char *hostname); // Définit le hostname

/**
 * @brief  Récupère le hostname du module.
 * @param  hostname Buffer de sortie.
 * @param  len      Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_hostname(char *hostname, size_t len); // Récupère le hostname

/**
 * @brief  Effectue un ping vers une adresse.
 * @param  host Adresse à pinger.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_ping(const char *host); // Ping

/**
 * @brief  Ouvre une connexion TCP/UDP.
 * @param  type Type ("TCP" ou "UDP").
 * @param  addr Adresse IP ou DNS.
 * @param  port Port distant.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_open_connection(const char *type, const char *addr, int port); // Ouvre une connexion TCP/UDP

/**
 * @brief  Récupère le statut TCP.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_tcp_status(char *out, size_t out_size); // Statut TCP

/**
 * @brief  Récupère l'état multi-connexion.
 * @param  enabled Pointeur vers la variable de sortie.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_multiple_connections(bool *enabled); // Multi-connexion

/**
 * @brief  Récupère l'état de connexion WiFi (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_connection(char *out, size_t out_size); // Etat WiFi brut

/**
 * @brief  Récupère l'état DHCP (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_dhcp_status(char *out, size_t out_size); // Etat DHCP brut

/**
 * @brief  Récupère les infos IP (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ip_info(char *out, size_t out_size); // Infos IP brutes

/**
 * @brief  Récupère le hostname (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_hostname_raw(char *out, size_t out_size); // Hostname brut

/**
 * @brief  Récupère la config AP (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_ap_config(char *out, size_t out_size); // Config AP brute

/**
 * @brief  Récupère l'état WiFi (brut).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_get_wifi_state(char *out, size_t out_size); // Etat WiFi brut

/**
 * @brief  Définit une IP statique.
 * @param  ip    Adresse IP.
 * @param  gw    Gateway.
 * @param  mask  Masque réseau.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_set_ip(const char *ip, const char *gw, const char *mask); // IP statique

/**
 * @brief  Configure un AP avancé.
 * @param  ssid      SSID.
 * @param  password  Mot de passe.
 * @param  channel   Canal.
 * @param  encryption Type d'encryptage.
 * @return ESP01_Status_t
 */
ESP01_Status_t esp01_start_ap_config(const char *ssid, const char *password, int channel, int encryption); // AP avancé

/* ======================= FONCTIONS UTILITAIRES ======================= */

/**
 * @brief  Retourne une chaîne lisible pour le type d'encryptage.
 * @param  code Code d'encryptage.
 * @return Chaîne descriptive.
 */
const char *esp01_encryption_to_string(const char *code); // Encryption en texte

/**
 * @brief  Retourne une chaîne lisible pour le statut TCP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_tcp_status_to_string(const char *resp); // Statut TCP en texte

/**
 * @brief  Retourne une chaîne lisible pour l'état CWSTATE.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_cwstate_to_string(const char *resp); // Etat CWSTATE en texte

/**
 * @brief  Retourne une chaîne lisible pour le statut de connexion.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_connection_status_to_string(const char *resp); // Statut connexion en texte

/**
 * @brief  Retourne une chaîne lisible pour le résultat d'un ping.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_ping_result_to_string(const char *resp); // Résultat ping en texte

/**
 * @brief  Retourne une chaîne lisible pour le résultat de connexion AP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_cwjap_to_string(const char *resp); // Résultat connexion AP en texte

/**
 * @brief  Retourne une chaîne lisible pour la puissance RF.
 * @param  rf_dbm Puissance en dBm.
 * @return Chaîne descriptive.
 */
const char *esp01_rf_power_to_string(int rf_dbm); // Puissance RF en texte

/**
 * @brief  Retourne une chaîne lisible pour le résultat de déconnexion AP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_cwqap_to_string(const char *resp); // Résultat déconnexion AP en texte

/**
 * @brief  Retourne une chaîne lisible pour l'état de connexion WiFi.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_wifi_connection_to_string(const char *resp); // Etat connexion WiFi en texte

/**
 * @brief  Retourne une chaîne lisible pour l'état DHCP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_dhcp_status_to_string(const char *resp); // Etat DHCP en texte

/**
 * @brief  Retourne une chaîne lisible pour les infos IP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_ip_info_to_string(const char *resp); // Infos IP en texte

/**
 * @brief  Retourne une chaîne lisible pour le hostname brut.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_hostname_raw_to_string(const char *resp); // Hostname brut en texte

/**
 * @brief  Retourne une chaîne lisible pour la config AP.
 * @param  resp Réponse brute.
 * @return Chaîne descriptive.
 */
const char *esp01_ap_config_to_string(const char *resp); // Config AP en texte


#endif
