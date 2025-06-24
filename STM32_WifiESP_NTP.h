/**
 ******************************************************************************
 * @file    STM32_WifiESP_NTP.h
 * @author  Weedman
 * @version 1.3.0
 * @date    19 juin 2025
 * @brief   Fonctions NTP haut niveau pour ESP01 (configuration, synchro, parsing, DST)
 *
 * @details
 * Ce header regroupe toutes les fonctions de gestion NTP du module ESP01 :
 *   - Configuration et synchronisation NTP
 *   - Parsing et affichage de la date/heure NTP
 *   - Support du changement d'heure automatique (DST)
 *
 * @note
 *   - Nécessite le driver bas niveau STM32_WifiESP.h
 *   - Nécessite le module WiFi STM32_WifiESP_WIFI.h
 *   - Compatible STM32CubeIDE.
 *   - Toutes les fonctions nécessitent une initialisation préalable du module ESP01.
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_NTP_H_
#define STM32_WIFIESP_NTP_H_

/* ========================== INCLUDES ========================== */
#include <stdint.h>             // Types entiers standard (uint8_t, etc.)
#include <stddef.h>             // Types de taille (size_t, etc.)
#include <stdbool.h>            // Types booléens
#include "STM32_WifiESP.h"      // Driver bas niveau requis
#include "STM32_WifiESP_WIFI.h" // Fonctions WiFi ESP01

/* =========================== DEFINES ========================== */
#define ESP01_NTP_MAX_SERVER_LEN 64             // Longueur max. du nom de serveur NTP
#define ESP01_NTP_DATETIME_BUF_SIZE 64          // Taille buffer date/heure
#define ESP01_NTP_DEFAULT_SERVER "pool.ntp.org" // Serveur NTP par défaut
#define ESP01_NTP_DEFAULT_PERIOD_S 3600         // Période par défaut (1 heure)

/* =========================== TYPES & STRUCTURES ============================ */
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

/* =========================== FONCTIONS PRINCIPALES (API NTP) ============================ */

/**
 * @brief Configure la structure NTP locale (ne modifie pas le module ESP01).
 */
ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone, int sync_period_s, bool dst_enable);

/**
 * @brief Vérifie si la synchronisation NTP périodique est activée.
 */
bool esp01_ntp_is_periodic_enabled(void);

/**
 * @brief Lance la synchronisation NTP (one shot ou périodique selon periodic).
 */
ESP01_Status_t esp01_ntp_start_sync(bool periodic);

/**
 * @brief Gère la tâche de synchronisation NTP (à appeler périodiquement si besoin).
 */
ESP01_Status_t esp01_ntp_handle(void);

/**
 * @brief Applique la configuration NTP au module ESP01 (enable, timezone, serveur, intervalle).
 */
ESP01_Status_t esp01_apply_ntp_config(uint8_t enable, int timezone, const char *server, int interval_s);

/**
 * @brief Récupère la dernière date/heure NTP reçue sous forme brute (chaîne).
 */
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize);

/* =========================== FONCTIONS ONE-SHOT & AFFICHAGE ============================ */
/**
 * @brief Affiche la configuration NTP courante.
 */
void esp01_print_ntp_config(void);

/**
 * @brief Affiche la dernière date/heure NTP reçue (langue FR/EN, voir paramètre lang).
 * @param lang 'F' pour français, 'E' pour anglais
 * @retval ESP01_OK si affichage réussi, ESP01_ERROR sinon
 */
ESP01_Status_t esp01_ntp_print_last_datetime(char lang);

/**
 * @brief Retourne la dernière date/heure NTP reçue (brute).
 */
const char *esp01_ntp_get_last_datetime(void);

/**
 * @brief Indique si une nouvelle date NTP a été reçue depuis le dernier appel.
 */
uint8_t esp01_ntp_is_updated(void);

/**
 * @brief Réinitialise le flag d'update NTP.
 */
void esp01_ntp_clear_updated_flag(void);

/**
 * @brief Retourne un pointeur vers la configuration NTP courante.
 */
const esp01_ntp_config_t *esp01_get_ntp_config(void);

/* =========================== PARSING, FORMATAGE & UTILITAIRES ============================ */
/**
 * @brief Parse une chaîne NTP ESP01 et remplit une structure date/heure.
 */
ESP01_Status_t esp01_parse_ntp_esp01(const char *datetime_ntp, ntp_datetime_t *dt);

/**
 * @brief Affiche une structure date/heure au format français.
 */
void esp01_print_datetime_fr(const ntp_datetime_t *dt);

/**
 * @brief Affiche une structure date/heure au format anglais.
 */
void esp01_print_datetime_en(const ntp_datetime_t *dt);

/**
 * @brief Parse une chaîne de date/heure au format NTP de l'ESP01.
 * @param datetime_str Chaîne à parser (ex: "Thu Jun 19 11:41:56 2025")
 * @param dt Structure de sortie
 * @return true si parsing réussi, false sinon
 */
bool esp01_parse_ntp_datetime(const char *datetime_str, ntp_datetime_t *dt);

/**
 * @brief Formate une date au format français dans un buffer.
 */
ESP01_Status_t esp01_format_datetime_fr(const ntp_datetime_t *dt, char *buffer, size_t size);

/**
 * @brief Formate une date au format anglais avec AM/PM dans un buffer.
 */
ESP01_Status_t esp01_format_datetime_en(const ntp_datetime_t *dt, char *buffer, size_t size);

/**
 * @brief Détermine si l'heure d'été est active pour une date donnée.
 */
bool esp01_is_dst_active(const ntp_datetime_t *dt);

/**
 * @brief Récupère la dernière date/heure NTP reçue dans une structure utilisateur.
 * @param dt Pointeur vers la structure à remplir
 * @retval ESP01_OK si succès, ESP01_FAIL sinon
 */
ESP01_Status_t esp01_ntp_get_last_datetime_struct(ntp_datetime_t *dt);

/**
 * @brief Formate la dernière date/heure NTP reçue.
 * @param lang 'F' pour français, 'E' pour anglais, 0 ou '\0' pour brut (non formaté)
 * @param buffer Buffer de sortie (optionnel si brut)
 * @param bufsize Taille du buffer
 * @retval ESP01_OK si succès, ESP01_FAIL sinon
 *
 * Si lang == 0 ou '\0', la fonction copie la date brute (non formatée, non traduite, sans DST) dans buffer (si non NULL), ou effectue juste le parsing si buffer==NULL.
 * Si lang == 'F' ou 'E', la date est formatée en français ou anglais dans buffer.
 */
ESP01_Status_t esp01_ntp_format_last_datetime(char lang, char *buffer, size_t bufsize);

#endif /* STM32_WIFIESP_NTP_H_ */
