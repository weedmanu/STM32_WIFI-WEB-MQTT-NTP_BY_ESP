/**
 ******************************************************************************
 * @file    STM32_WifiESP_NTP.h
 * @author  Weedman
 * @version 1.2.0
 * @date    18 juin 2025
 * @brief   Fonctions haut niveau NTP pour ESP01 (synchronisation, parsing, affichage)
 *
 * @details
 * Ce header regroupe toutes les fonctions de gestion NTP haut niveau du module ESP01 :
 * - Configuration et synchronisation NTP (one shot ou périodique)
 * - Parsing et affichage de la date/heure NTP
 * - Accès à la configuration et à la dernière date reçue
 *
 * @note
 * - Nécessite le driver bas niveau STM32_WifiESP.h et le module WiFi STM32_WifiESP_WIFI.h
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_NTP_H_
#define STM32_WIFIESP_NTP_H_

/* ========================== INCLUDES ========================== */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "STM32_WifiESP.h"
#include "STM32_WifiESP_WIFI.h"

/* ========================== DEFINES ========================== */
#define ESP01_NTP_MAX_SERVER_LEN      64      ///< Longueur max du nom de serveur NTP
#define ESP01_NTP_DEFAULT_SERVER      "pool.ntp.org"
#define ESP01_NTP_DEFAULT_PERIOD_S    3600    ///< Période de synchro par défaut (s)
#define ESP01_NTP_DATETIME_BUF_SIZE   64      ///< Taille buffer date/heure NTP

/* ========================== TYPES ========================== */
/**
 * @brief Structure de configuration NTP
 */
typedef struct
{
    char server[ESP01_NTP_MAX_SERVER_LEN]; ///< Nom du serveur NTP
    int timezone;                          ///< Décalage horaire
    int period_s;                          ///< Période de synchronisation (s)
} esp01_ntp_config_t;

/**
 * @brief Structure date/heure NTP (format français)
 */
typedef struct
{
    int year;   ///< Année
    int month;  ///< Mois (1-12)
    int day;    ///< Jour du mois
    int wday;   ///< Jour de la semaine (0=dimanche)
    int hour;   ///< Heure
    int min;    ///< Minute
    int sec;    ///< Seconde
} ntp_datetime_fr_t;

/* ======================= FONCTIONS PRINCIPALES ======================= */

/**
 * @brief Configure la structure NTP locale (ne modifie pas le module ESP01).
 * @param ntp_server   Nom du serveur NTP.
 * @param timezone     Décalage horaire.
 * @param sync_period_s Période de synchro en secondes.
 * @return ESP01_OK si succès, ESP01_INVALID_PARAM sinon.
 */
ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone, int sync_period_s);

/**
 * @brief Lance la synchronisation NTP (one shot ou périodique selon periodic).
 * @param periodic true = périodique, false = one shot.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_ntp_start_sync(bool periodic);

/**
 * @brief Applique la configuration NTP au module ESP01 (commande AT).
 * @param enable      1 pour activer NTP, 0 pour désactiver.
 * @param timezone    Décalage horaire.
 * @param server      Nom du serveur NTP.
 * @param interval_s  Période de synchronisation (s).
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_apply_ntp_config(uint8_t enable, int timezone, const char *server, int interval_s);

/**
 * @brief  Récupère la date/heure NTP depuis le module ESP01 (commande AT).
 * @param  datetime_buf Buffer de sortie.
 * @param  bufsize      Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize);

/* ======================= ACCÈS ET AFFICHAGE ======================= */

/**
 * @brief Affiche la configuration NTP courante (logs).
 */
void esp01_print_ntp_config(void);

/**
 * @brief Affiche la dernière date/heure NTP reçue en français (logs).
 */
void esp01_ntp_print_last_datetime_fr(void);

/**
 * @brief Affiche la dernière date/heure NTP reçue en anglais (logs).
 */
void esp01_ntp_print_last_datetime_en(void);

/**
 * @brief Retourne la dernière date/heure NTP brute reçue.
 * @return Pointeur sur la chaîne date/heure.
 */
const char *esp01_ntp_get_last_datetime(void);

/**
 * @brief Indique si une nouvelle date NTP a été reçue.
 * @return 1 si oui, 0 sinon.
 */
uint8_t esp01_ntp_is_updated(void);

/**
 * @brief Efface le flag "date NTP reçue".
 */
void esp01_ntp_clear_updated_flag(void);

/**
 * @brief Retourne la configuration NTP courante.
 * @return Pointeur sur la structure de config.
 */
const esp01_ntp_config_t *esp01_get_ntp_config(void);

/* ======================= PARSING ET UTILITAIRES ======================= */

/**
 * @brief Parse une date brute ESP01 en structure française.
 * @param datetime_ntp Chaîne brute.
 * @param dt Structure de sortie.
 * @return ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_parse_ntp_esp01(const char *datetime_ntp, ntp_datetime_fr_t *dt);

/**
 * @brief Affiche une structure date/heure en français (logs).
 * @param dt Structure à afficher.
 */
void esp01_print_datetime_fr(const ntp_datetime_fr_t *dt);

/**
 * @brief Affiche une structure date/heure en anglais (logs).
 * @param dt Structure à afficher.
 */
void esp01_print_datetime_en(const ntp_datetime_fr_t *dt);

#endif /* STM32_WIFIESP_NTP_H_ */
