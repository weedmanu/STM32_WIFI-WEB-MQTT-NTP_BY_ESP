#ifndef INC_STM32_WIFIESP_UTILS_H_
#define INC_STM32_WIFIESP_UTILS_H_

#include "STM32_WifiESP.h"

// ==================== STRUCTURES UTILS ====================

/**
 * @brief Structure de parsing d'un header IPD.
 * @see http_request_t dans STM32_WifiESP.h
 */

// ==================== FONCTIONS UTILS ====================

/**
 * @brief Affiche un message de log (avec saut de ligne).
 * @param msg Message à afficher.
 */
void _esp_logln(const char *msg);

/**
 * @brief Affiche un message de log (sans saut de ligne).
 * @param msg Message à afficher.
 */
void _esp_log(const char *msg);

/**
 * @brief Récupère les nouvelles données reçues via DMA.
 * @param buffer Buffer de sortie.
 * @param buffer_size Taille du buffer.
 * @return Nombre d'octets copiés.
 */
uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size);

/**
 * @brief Vide le buffer de réception UART/DMA pendant un certain temps.
 * @param timeout_ms Timeout en millisecondes.
 */
void _flush_rx_buffer(uint32_t timeout_ms);

/**
 * @brief Recherche le prochain header +IPD dans un buffer.
 * @param buffer Buffer à analyser.
 * @param buffer_len Taille du buffer.
 * @return Pointeur sur le début du header +IPD, ou NULL si non trouvé.
 */
char *_find_next_ipd(char *buffer, int buffer_len);

/**
 * @brief Parse un header +IPD et remplit la structure http_request_t.
 * @param data Chaîne contenant le header +IPD.
 * @return Structure http_request_t remplie.
 */
http_request_t parse_ipd_header(const char *data);

/**
 * @brief Retourne une chaîne descriptive pour un code d'erreur ESP01.
 * @param status Code d'erreur.
 * @return Chaîne descriptive.
 */
const char *esp01_get_error_string(ESP01_Status_t status);

/**
 * @brief Ferme les connexions inactives.
 */
void esp01_cleanup_inactive_connections(void);

/**
 * @brief Retourne le nombre de connexions actives.
 * @return Nombre de connexions actives.
 */
int esp01_get_active_connection_count(void);

/**
 * @brief Attend un motif dans le flux RX (accumulateur).
 * @param pattern Motif à rechercher.
 * @param timeout_ms Timeout en millisecondes.
 * @return ESP01_OK si trouvé, ESP01_TIMEOUT sinon.
 */
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);

#endif /* INC_STM32_WIFIESP_UTILS_H_ */