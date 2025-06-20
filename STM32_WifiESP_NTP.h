/**
 ******************************************************************************
 * @file    STM32_WifiESP_NTP.h
 * @author  Weedman
 * @version 1.3.0
 * @date    19 juin 2025
 * @brief   Définition des fonctions NTP pour ESP01
 *
 * @details
 * Ce fichier contient les déclarations des fonctions de gestion NTP pour ESP01:
 * - Configuration et synchronisation NTP
 * - Parsing et affichage de la date/heure NTP
 * - Support du changement d'heure automatique (DST)
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_NTP_H_
#define STM32_WIFIESP_NTP_H_

/* ========================== INCLUDES ========================== */
#include <stdint.h>        // Pour uint8_t, uint16_t, etc.
#include <stddef.h>        // Pour size_t
#include <stdbool.h>       // Pour bool
#include "STM32_WifiESP.h" // Inclusion du driver bas niveau
#include "STM32_WifiESP_WIFI.h" // Fonctions WiFi ESP01

/* ========================== DEFINES ========================== */
#define ESP01_NTP_MAX_SERVER_LEN 64             // Longueur max. du nom de serveur NTP
#define ESP01_NTP_DATETIME_BUF_SIZE 64          // Taille buffer date/heure
#define ESP01_NTP_DEFAULT_SERVER "pool.ntp.org" // Serveur NTP par défaut
#define ESP01_NTP_DEFAULT_PERIOD_S 3600         // Période par défaut (1 heure)

/* ========================== STRUCTURES ========================== */

/**
 * @brief Structure de date/heure NTP parsée
 */
typedef struct
{
    uint16_t year; // Année (ex: 2025)
    uint8_t month; // Mois (1-12)
    uint8_t day;   // Jour du mois (1-31)
    uint8_t wday;  // Jour de la semaine (0-6, 0=Dimanche)
    uint8_t hour;  // Heure (0-23)
    uint8_t min;   // Minute (0-59)
    uint8_t sec;   // Seconde (0-59)
    bool dst;      // Indicateur d'heure d'été
} ntp_datetime_t;

/**
 * @brief Structure de configuration NTP
 */
typedef struct
{
    char server[ESP01_NTP_MAX_SERVER_LEN]; // Nom du serveur NTP
    int timezone;                          // Fuseau horaire (-12 à +14)
    int period_s;                          // Période de synchro en secondes
    bool dst_enable;                       // Activation du changement d'heure automatique
} esp01_ntp_config_t;

/* ========================== FONCTIONS ========================== */

/* ---------- Fonctions principales ---------- */
ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone, int sync_period_s, bool dst_enable);
ESP01_Status_t esp01_ntp_start_sync(bool periodic);
ESP01_Status_t esp01_ntp_handle(void);
ESP01_Status_t esp01_apply_ntp_config(uint8_t enable, int timezone, const char *server, int interval_s);
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize);

/* ---------- Fonctions one-shot simples ---------- */
ESP01_Status_t esp01_ntp_get_and_display(char lang); // 'F' pour français, 'E' pour anglais

/* ---------- Affichage et accès ---------- */
void esp01_print_ntp_config(void);
void esp01_ntp_print_last_datetime_fr(void);
void esp01_ntp_print_last_datetime_en(void);
const char *esp01_ntp_get_last_datetime(void);
uint8_t esp01_ntp_is_updated(void);
void esp01_ntp_clear_updated_flag(void);
const esp01_ntp_config_t *esp01_get_ntp_config(void);

/* ---------- Parsing et utilitaires ---------- */
ESP01_Status_t esp01_parse_ntp_esp01(const char *datetime_ntp, ntp_datetime_t *dt);
void esp01_print_datetime_fr(const ntp_datetime_t *dt);
void esp01_print_datetime_en(const ntp_datetime_t *dt);

/* ----------- Types de compatibilité ---------- */
typedef ntp_datetime_t ntp_datetime_fr_t; // Pour compatibilité ascendante

/**
 * @brief Parse une chaîne de date/heure au format NTP de l'ESP01
 * @param datetime_str Chaîne de date/heure à parser (format "Thu Jun 19 11:41:56 2025")
 * @param dt Structure de sortie
 * @return true si parsing réussi, false sinon
 */
bool esp01_parse_ntp_datetime(const char *datetime_str, ntp_datetime_t *dt);

/**
 * @brief Formate une date au format français dans un buffer
 * @param dt Structure date/heure
 * @param buffer Buffer de sortie
 * @param size Taille du buffer
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_format_datetime_fr(const ntp_datetime_t *dt, char *buffer, size_t size);

/**
 * @brief Formate une date au format anglais avec AM/PM dans un buffer
 * @param dt Structure date/heure
 * @param buffer Buffer de sortie
 * @param size Taille du buffer
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_format_datetime_en(const ntp_datetime_t *dt, char *buffer, size_t size);

/**
 * @brief Détermine si l'heure d'été est active pour une date donnée
 * @param dt Structure date/heure
 * @return true si heure d'été active, false sinon
 */
bool esp01_is_dst_active(const ntp_datetime_t *dt);

#endif /* STM32_WIFIESP_NTP_H_ */
