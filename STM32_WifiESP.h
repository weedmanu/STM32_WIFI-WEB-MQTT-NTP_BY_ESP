/**
 ******************************************************************************
 * @file    STM32_WifiESP.h
 * @author  manu
 * @version 1.2.0
 * @date    13 juin 2025
 * @brief   Driver bas niveau pour module ESP01 (UART, AT, debug, reset, config, terminal AT)
 *
 * @details
 * Ce header regroupe toutes les fonctions de gestion bas niveau du module ESP01,
 * ne nécessitant pas de connexion WiFi : initialisation, configuration UART,
 * gestion du mode sommeil, puissance RF, logs système, reset, restore, version,
 * envoi de commandes AT, gestion du buffer DMA, debug, etc.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - 1 UART TX/RX avec DMA circulaire sur RX requis pour la communication ESP01.
 * - 1 UART TX/RX avec IT pour le terminal AT.
 * - Toutes les fonctions ici sont utilisables sans connexion WiFi.
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_H_
#define STM32_WIFIESP_H_

/* ========================== INCLUDES ========================== */

#include <string.h>  // Fonctions de manipulation de chaînes
#include <stdbool.h> // Types booléens
#include <stddef.h>  // Types de taille (size_t, etc.)
#include <stdint.h>  // Types entiers standard (uint8_t, uint16_t, etc.)
#include "main.h"    // HAL, UART, etc.

/* =========================== DEFINES ========================== */

// ----------- DEBUG -----------
#define ESP01_DEBUG 1 // 1 = logs de debug activés, 0 = désactivés

// ----------- CONSTANTES -----------
#define ESP01_DMA_RX_BUF_SIZE 1024 // Taille buffer DMA RX UART
#define ESP01_MAX_CMD_BUF 512      // Taille max buffer commande AT
#define ESP01_MAX_RESP_BUF 2048    // Taille max buffer réponse
#define ESP01_LARGE_RESP_BUF 4096  // Taille buffer réponse large (pour les réponses longues)
#define ESP01_SMALL_BUF_SIZE 64    // Taille petit buffer
#define ESP01_TIMEOUT_SHORT 2000   // Timeout court (ms)
#define ESP01_TIMEOUT_MEDIUM 7000  // Timeout moyen (ms)
#define ESP01_TIMEOUT_LONG 15000   // Timeout long (ms)
#define UART_STR_BUF_SIZE 128      // Taille buffer UART string
#define SLEEP_STR_BUF_SIZE 64      // Taille buffer sleep string
#define SYSLOG_STR_BUF_SIZE 32     // Taille buffer syslog string
#define GMR_VERSION_LINES 3        // Nombre de lignes version GMR

/* =========================== TYPES ============================ */

// ----------- Statuts -----------
/**
 * @brief  Codes de statut pour les fonctions du driver ESP01.
 */
typedef enum
{
    ESP01_OK = 0,             ///< Opération réussie
    ESP01_FAIL,               ///< Échec générique
    ESP01_TIMEOUT,            ///< Timeout
    ESP01_NOT_INITIALIZED,    ///< Non initialisé
    ESP01_INVALID_PARAM,      ///< Paramètre invalide
    ESP01_BUFFER_OVERFLOW,    ///< Dépassement de buffer
    ESP01_WIFI_NOT_CONNECTED, ///< WiFi non connecté
    ESP01_HTTP_PARSE_ERROR,   ///< Erreur parsing HTTP
    ESP01_ROUTE_NOT_FOUND,    ///< Route HTTP non trouvée
    ESP01_CONNECTION_ERROR,   ///< Erreur de connexion
    ESP01_MEMORY_ERROR,       ///< Erreur mémoire
    ESP01_EXIT,               ///< Sortie application/terminal
    ESP01_NOT_DETECTED        ///< ESP01 non détecté physiquement
} ESP01_Status_t;

/* ========================= VARIABLES GLOBALES ========================= */
extern UART_HandleTypeDef *g_esp_uart;   // UART principal ESP01
extern UART_HandleTypeDef *g_debug_uart; // UART debug
extern uint8_t *g_dma_rx_buf;            // Buffer DMA RX
extern uint16_t g_dma_buf_size;          // Taille buffer DMA
extern volatile uint16_t g_rx_last_pos;  // Dernière position RX
extern uint16_t g_server_port;           // Port serveur

/* ========================= MACROS UTILES ============================== */

// ----------- Log interne -----------
/**
 * @brief  Log interne formaté sur l'UART de debug.
 * @param  fmt  Format printf.
 * @param  ...  Arguments variables.
 */
void _esp_login(const char *fmt, ...); // Fonction de log interne

#define VALIDATE_PARAM(expr, errcode) \
    do                                \
    {                                 \
        if (!(expr))                  \
            return (errcode);         \
    } while (0)

#define ESP01_LOG_INFO(module, fmt, ...) _esp_login("[" module "][INFO] " fmt, ##__VA_ARGS__)
#define ESP01_LOG_ERROR(module, fmt, ...) _esp_login("[" module "][ERROR] " fmt, ##__VA_ARGS__)
#define ESP01_LOG_WARN(module, fmt, ...) _esp_login("[" module "][WARN] " fmt, ##__VA_ARGS__)
#define ESP01_LOG_DEBUG(module, fmt, ...) _esp_login("[" module "][DEBUG] " fmt, ##__VA_ARGS__)

/**
 * @brief  Log d'erreur harmonisé et retourne le code d'erreur.
 * @param  prefix   Contexte ou nom de la fonction.
 * @param  status   Code de statut à retourner.
 */
#define ESP01_RETURN_ERROR(prefix, status)                                          \
    do                                                                              \
    {                                                                               \
        _esp_login(">>> [%s] Erreur : %s", prefix, esp01_get_error_string(status)); \
        return (status);                                                            \
    } while (0)

/* ========================= FONCTIONS PRINCIPALES ====================== */

// ----------- Initialisation & Driver -----------

/**
 * @brief  Teste la communication AT avec l'ESP01.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_test_at(void);

/**
 * @brief  Initialise le module ESP01 et configure les UART.
 * @param  huart_esp   UART principal pour l'ESP01.
 * @param  huart_debug UART pour le debug.
 * @param  dma_rx_buf  Buffer DMA RX.
 * @param  dma_buf_size Taille du buffer DMA.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size); // Initialisation                                                                                                // Test commande AT

/**
 * @brief  Effectue un reset matériel du module ESP01.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_reset(void);                                                                                                      // Reset module

/**
 * @brief  Restaure les paramètres d'usine du module ESP01.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_restore(void);                                                                                                    // Restore usine

/**
 * @brief  Vide le buffer RX UART du module ESP01.
 * @param  timeout_ms Timeout en millisecondes.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms);                                                                             // Flush RX

/**
 * @brief  Récupère les nouvelles données reçues sur l'ESP01.
 * @param  buf     Buffer de destination.
 * @param  bufsize Taille du buffer.
 * @retval Nombre d'octets lus ou -1 en cas d'erreur.
 */
int esp01_get_new_data(uint8_t *buf, uint16_t bufsize);                                                                                // Récupération nouvelle data

/**
 * @brief  Vide le buffer RX interne (usage interne).
 * @param  timeout_ms Timeout en millisecondes.
 */
void _flush_rx_buffer(uint32_t timeout_ms);                                                                                            // Flush RX interne

// ----------- Version & Infos -----------

/**
 * @brief  Récupère la version du firmware AT de l'ESP01.
 * @param  version_buf Buffer de sortie.
 * @param  buf_size    Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size); // Version AT

// ----------- UART -----------

/**
 * @brief  Récupère la configuration UART actuelle de l'ESP01.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_uart_config(char *out, size_t out_size);                                                          // Get config UART

/**
 * @brief  Convertit la configuration UART brute en chaîne lisible.
 * @param  raw_config Configuration UART brute.
 * @param  out        Buffer de sortie.
 * @param  out_size   Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size);                            // UART config to string

/**
 * @brief  Configure l'UART de l'ESP01.
 * @param  baud      Baudrate.
 * @param  databits  Nombre de bits de données.
 * @param  stopbits  Nombre de bits de stop.
 * @param  parity    Parité.
 * @param  flowctrl  Contrôle de flux.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_set_uart_config(uint32_t baud, uint8_t databits, uint8_t stopbits, uint8_t parity, uint8_t flowctrl); // Set UART config

// ----------- Mode sommeil -----------

/**
 * @brief  Récupère le mode sommeil actuel de l'ESP01.
 * @param  mode Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_sleep_mode(int *mode);                                  // Get sleep mode

/**
 * @brief  Définit le mode sommeil de l'ESP01.
 * @param  mode Mode sommeil à définir.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_set_sleep_mode(int mode);                                   // Set sleep mode

/**
 * @brief  Convertit le mode sommeil en chaîne lisible.
 * @param  mode      Mode sommeil.
 * @param  out       Buffer de sortie.
 * @param  out_size  Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_sleep_mode_to_string(int mode, char *out, size_t out_size); // Sleep mode to string

// ----------- Puissance RF -----------

/**
 * @brief  Récupère la puissance RF actuelle de l'ESP01.
 * @param  dbm Pointeur vers la variable de sortie (dBm).
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_rf_power(int *dbm); // Get RF power

/**
 * @brief  Définit la puissance RF de l'ESP01.
 * @param  dbm Puissance en dBm.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_set_rf_power(int dbm);  // Set RF power

// ----------- Logs système -----------

/**
 * @brief  Récupère le niveau de log système de l'ESP01.
 * @param  level Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_syslog(int *level);                                   // Get syslog

/**
 * @brief  Définit le niveau de log système de l'ESP01.
 * @param  level Niveau de log à définir.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_set_syslog(int level);                                    // Set syslog

/**
 * @brief  Convertit le niveau de log système en chaîne lisible.
 * @param  syslog    Niveau de log.
 * @param  out       Buffer de sortie.
 * @param  out_size  Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_syslog_to_string(int syslog, char *out, size_t out_size); // Syslog to string

// ----------- RAM libre -----------

/**
 * @brief  Récupère la quantité de RAM libre sur l'ESP01.
 * @param  free_ram Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_sysram(uint32_t *free_ram); // Get RAM libre

// ----------- Deep sleep -----------

/**
 * @brief  Met l'ESP01 en deep sleep pour une durée donnée.
 * @param  ms Durée en millisecondes.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_deep_sleep(uint32_t ms); // Deep sleep

// ----------- Liste commandes AT -----------

/**
 * @brief  Récupère la liste des commandes AT supportées par l'ESP01.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size); // Liste commandes AT

// ----------- Utilitaires & Debug -----------

/**
 * @brief  Retourne la chaîne correspondant à un code d'erreur ESP01.
 * @param  status Code d'erreur.
 * @retval Chaîne descriptive.
 */
const char *esp01_get_error_string(ESP01_Status_t status);                                                                               // String erreur

/**
 * @brief  Attend un motif précis dans la réponse de l'ESP01.
 * @param  pattern    Motif à attendre.
 * @param  timeout_ms Timeout en millisecondes.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);                                                         // Attente motif

/**
 * @brief  Envoie une commande AT brute via DMA et récupère la réponse.
 * @param  cmd          Commande à envoyer.
 * @param  resp         Buffer de réponse.
 * @param  resp_size    Taille du buffer de réponse.
 * @param  wait_pattern Motif d'attente dans la réponse.
 * @param  timeout_ms   Timeout en millisecondes.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *resp, size_t resp_size, const char *wait_pattern, uint32_t timeout_ms); // Envoi commande brute

/* ==================== OUTILS DE PARSING ==================== */

/**
 * @brief  Extrait un entier après un motif dans une réponse.
 * @param  resp    Réponse source.
 * @param  pattern Motif à rechercher.
 * @param  out     Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_parse_int_after(const char *resp, const char *pattern, int32_t *out);                  // Parse int après motif

/**
 * @brief  Extrait une chaîne après un motif dans une réponse.
 * @param  resp      Réponse source.
 * @param  pattern   Motif à rechercher.
 * @param  out       Buffer de sortie.
 * @param  out_size  Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_parse_string_after(const char *resp, const char *pattern, char *out, size_t out_size); // Parse string après motif

/**
 * @brief  Extrait une valeur entre guillemets après un motif.
 * @param  src      Chaîne source.
 * @param  motif    Motif à rechercher.
 * @param  out      Buffer de sortie.
 * @param  out_len  Taille du buffer.
 * @retval true si trouvé, false sinon.
 */
bool esp01_extract_quoted_value(const char *src, const char *motif, char *out, size_t out_len);             // Extraction valeur entre guillemets

/**
 * @brief  Extrait un booléen après un motif dans une réponse.
 * @param  resp Réponse source.
 * @param  tag  Motif à rechercher.
 * @param  out  Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_parse_bool_after(const char *resp, const char *tag, bool *out);                        // Parse bool après motif

/* ========================= TERMINAL AT (CONSOLE) ======================= */

/**
 * @brief  Démarre le terminal AT sur l'UART de debug.
 * @param  huart_debug UART de debug.
 */
void esp01_terminal_begin(UART_HandleTypeDef *huart_debug);                                    // Démarrage terminal AT

/**
 * @brief  Callback RX pour la console AT.
 * @param  huart UART concerné.
 */
void esp01_console_rx_callback(UART_HandleTypeDef *huart);                                     // Callback RX console

/**
 * @brief  Tâche principale de la console AT.
 */
void esp01_console_task(void);                                                                 // Tâche console

/**
 * @brief  Parse la version GMR à partir du buffer brut.
 * @param  gmr_buf          Buffer source.
 * @param  version_buf      Buffer de sortie.
 * @param  version_buf_size Taille du buffer de sortie.
 */
void esp01_parse_gmr_version(const char *gmr_buf, char *version_buf, size_t version_buf_size); // Parsing version GMR

/* ========================= MANQUANT : PROTOTYPES MANQUANTS ======================= */

/**
 * @brief  Tâche de gestion du terminal AT interactif (console).
 */
void esp01_console_task(void);

/**
 * @brief  Callback de réception UART pour la console AT.
 * @param  huart Pointeur sur la structure UART.
 */
void esp01_console_rx_callback(UART_HandleTypeDef *huart);

/* ========================= INLINE UTILS ========================= */
/**
 * @brief  Copie sécurisée d'une chaîne dans un buffer.
 * @param  dst      Buffer de destination.
 * @param  dst_size Taille du buffer.
 * @param  src      Chaîne source.
 * @retval ESP01_Status_t
 */
static inline ESP01_Status_t esp01_safe_strcpy(char *dst, size_t dst_size, const char *src) // Copie sécurisée string
{
    if (!dst || !src || dst_size == 0)
        return ESP01_INVALID_PARAM;
    if (strlen(src) >= dst_size)
        return ESP01_BUFFER_OVERFLOW;
    strcpy(dst, src);
    return ESP01_OK;
}

/**
 * @brief Vérifie si un buffer a assez de place pour une écriture.
 * @param needed   Nombre d'octets nécessaires.
 * @param avail    Espace disponible.
 * @retval ESP01_Status_t
 */
static inline ESP01_Status_t esp01_check_buffer_size(size_t needed, size_t avail) // Vérification taille buffer
{
    return (needed > avail) ? ESP01_BUFFER_OVERFLOW : ESP01_OK;
}

#endif /* STM32_WIFIESP_H_ */
