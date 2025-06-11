/**
 ******************************************************************************
 * @file    STM32_WifiESP_NTP.h
 * @author  weedm
 * @version 1.1.0
 * @date    8 juin 2025
 * @brief   Bibliothèque de gestion NTP pour le module ESP01 WiFi.
 *
 * @details
 * Ce fichier contient les définitions, structures, macros et prototypes de
 * fonctions pour gérer la synchronisation de l'heure via NTP avec un module
 * ESP01 connecté à un microcontrôleur STM32. Il propose des fonctions haut
 * niveau pour la configuration du serveur NTP, la récupération et le parsing
 * de la date/heure, la gestion de la synchronisation périodique et l'affichage.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la bibliothèque STM32_WifiESP.h.
 *
 * @copyright
 * La licence de ce code est libre.
 ******************************************************************************
 */

#ifndef INC_STM32_WIFIESP_NTP_H_
#define INC_STM32_WIFIESP_NTP_H_

#include "STM32_WifiESP.h"
#include "STM32_WifiESP_WIFI.h"
#include <stdbool.h>
#include <stddef.h>

// ==================== DEFINES SPÉCIFIQUES NTP ====================

#define ESP01_NTP_DEFAULT_SERVER "pool.ntp.org" ///< Serveur NTP par défaut
#define ESP01_NTP_DEFAULT_PERIOD_S 3600			///< Période de synchro par défaut (1h)
#define ESP01_NTP_DATETIME_BUF_SIZE 32			///< Taille buffer date/heure NTP
#define ESP01_NTP_MAX_SERVER_LEN 64				///< Taille max nom serveur NTP
#define ESP01_NTP_SYNC_TIMEOUT_MS 5000			///< Timeout synchro NTP (ms)

// ==================== STRUCTURES NTP ====================

/**
 * @struct ntp_datetime_t
 * @brief  Structure représentant une date/heure NTP décodée.
 */
typedef struct
{
	int day, hour, min, sec, year;
	int wday_num, mon;
} ntp_datetime_t;

// ==================== PROTOTYPES NTP ====================

/**
 * @brief  Configure le serveur NTP et la synchronisation périodique.
 * @param  ntp_server      Adresse du serveur NTP.
 * @param  timezone_offset Décalage horaire.
 * @param  sync_period_s   Période de synchronisation (s).
 * @return ESP01_Status_t  Code de retour.
 */
ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone_offset, int sync_period_s);

/**
 * @brief  Récupère la date/heure NTP courante.
 * @param  datetime_buf Buffer de sortie.
 * @param  bufsize      Taille du buffer.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize);

/**
 * @brief  Synchronise et affiche la date/heure locale (one-shot ou périodique).
 * @param  timezone_offset Décalage horaire (ex: 1 pour UTC+1).
 * @param  print_fr        true pour affichage en français, false pour anglais.
 * @param  sync_period_s   Période de synchronisation automatique (en secondes, 0 = une seule synchro).
 */
void esp01_ntp_sync_and_print(int timezone_offset, bool print_fr, int sync_period_s);

/**
 * @brief  Parse une date/heure NTP et la formate en français.
 * @param  datetime_ntp Chaîne brute reçue du NTP.
 * @param  out          Buffer de sortie pour la date formatée.
 * @param  out_size     Taille du buffer de sortie.
 * @return char*        Pointeur vers le buffer de sortie.
 */
char *esp01_parse_fr_local_datetime(const char *datetime_ntp, char *out, size_t out_size);

/**
 * @brief  Parse une date/heure NTP et la formate en anglais/local.
 * @param  datetime_ntp    Chaîne brute reçue du NTP.
 * @param  timezone_offset Décalage horaire (ex: 1 pour UTC+1).
 * @param  out             Buffer de sortie pour la date formatée.
 * @param  out_size        Taille du buffer de sortie.
 * @return char*           Pointeur vers le buffer de sortie.
 */
char *esp01_parse_local_datetime(const char *datetime_ntp, int timezone_offset, char *out, size_t out_size);

/**
 * @brief  Affiche la date/heure locale en français sur le port debug.
 * @param  datetime_ntp Chaîne brute reçue du NTP.
 */
void esp01_print_fr_local_datetime(const char *datetime_ntp);

/**
 * @brief  Affiche la date/heure locale (anglais/local) sur le port debug.
 * @param  datetime_ntp    Chaîne brute reçue du NTP.
 * @param  timezone_offset Décalage horaire (ex: 1 pour UTC+1).
 */
void esp01_print_local_datetime(const char *datetime_ntp, int timezone_offset);

/**
 * @brief  Parse une chaîne NTP en structure ntp_datetime_t.
 * @param  datetime_ntp Chaîne NTP.
 * @param  dt           Structure de sortie.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_parse_ntp_datetime(const char *datetime_ntp, ntp_datetime_t *dt);

/**
 * @brief  Démarre la synchronisation périodique NTP.
 * @param  ntp_server      Adresse du serveur NTP.
 * @param  timezone_offset Décalage horaire.
 * @param  interval_seconds Intervalle de synchro (s).
 * @param  print_fr        Affichage en français.
 */
void esp01_ntp_start_periodic_sync(const char *ntp_server, int timezone_offset, uint32_t interval_seconds, bool print_fr);

/**
 * @brief  Arrête la synchronisation périodique NTP.
 */
void esp01_ntp_stop_periodic_sync(void);

/**
 * @brief  Tâche périodique de synchronisation NTP.
 */
void esp01_ntp_periodic_task(void);

/**
 * @brief  Récupère la dernière date/heure synchronisée.
 * @return Pointeur sur la chaîne date/heure.
 */
const char *esp01_ntp_get_last_datetime(void);

/**
 * @brief  Indique si la date/heure a été mise à jour.
 * @return 1 si mise à jour, 0 sinon.
 */
uint8_t esp01_ntp_is_updated(void);

/**
 * @brief  Réinitialise le flag de mise à jour.
 */
void esp01_ntp_clear_updated_flag(void);

#endif /* INC_STM32_WIFIESP_NTP_H_ */
