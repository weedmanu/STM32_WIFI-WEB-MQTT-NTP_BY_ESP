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
#define UART_STR_BUF_SIZE 128
#define SLEEP_STR_BUF_SIZE 64
#define SYSLOG_STR_BUF_SIZE 32
#define GMR_VERSION_LINES 3
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
extern UART_HandleTypeDef *g_esp_uart;
extern UART_HandleTypeDef *g_debug_uart;
extern uint8_t *g_dma_rx_buf;
extern uint16_t g_dma_buf_size;
extern volatile uint16_t g_rx_last_pos;
extern uint16_t g_server_port;

/* ========================= MACROS UTILES ============================== */

// ----------- Log interne -----------

/**
 * @brief  Log interne formaté sur l'UART de debug.
 * @param  fmt  Format printf.
 * @param  ...  Arguments variables.
 */
void _esp_login(const char *fmt, ...);

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

/* ========================= FONCTIONS PRINCIPALES ====================== */

// ----------- Initialisation & Driver -----------

/**
 * @brief  Initialise le module ESP01 et ses UART.
 * @param  huart_esp    UART utilisé pour l’ESP01 (AT).
 * @param  huart_debug  UART utilisé pour le debug/console.
 * @param  dma_rx_buf   Buffer DMA RX.
 * @param  dma_buf_size Taille du buffer DMA RX.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);

/**
 * @brief  Teste la communication AT avec l’ESP01.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_test_at(void);

/**
 * @brief  Effectue un reset logiciel du module ESP01.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_reset(void);

/**
 * @brief  Restaure les paramètres usine du module ESP01.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_restore(void);

/**
 * @brief  Vide le buffer RX (mode DMA).
 * @param  timeout_ms  Timeout en ms.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms);

/**
 * @brief  Récupère les nouvelles données reçues.
 * @param  buf      Buffer de sortie.
 * @param  bufsize  Taille du buffer.
 * @retval Nombre d'octets lus.
 */
int esp01_get_new_data(uint8_t *buf, uint16_t bufsize);

/**
 * @brief  Vide le buffer RX (utilitaire interne, non DMA).
 * @param  timeout_ms  Timeout en ms.
 */
void _flush_rx_buffer(uint32_t timeout_ms);

/* ----------- Version & Infos ----------- */

/**
 * @brief  Récupère la version AT du firmware ESP01.
 * @param  version_buf  Buffer de sortie.
 * @param  buf_size     Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size);

/* ----------- UART ----------- */

/**
 * @brief  Récupère la configuration UART courante.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_uart_config(char *out, size_t out_size);

/**
 * @brief  Convertit la config UART brute en chaîne lisible.
 * @param  raw_config  Chaîne brute.
 * @param  out        Buffer de sortie.
 * @param  out_size   Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size);

/**
 * @brief  Configure l’UART de l’ESP01.
 * @param  baud      Baudrate.
 * @param  databits  Nombre de bits de données.
 * @param  stopbits  Nombre de bits de stop.
 * @param  parity    Parité.
 * @param  flowctrl  Contrôle de flux.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_set_uart_config(uint32_t baud, uint8_t databits, uint8_t stopbits, uint8_t parity, uint8_t flowctrl);

/* ----------- Mode sommeil ----------- */

/**
 * @brief  Récupère le mode sommeil courant.
 * @param  mode  Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_sleep_mode(int *mode);

/**
 * @brief  Définit le mode sommeil du module.
 * @param  mode  Mode à appliquer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_set_sleep_mode(int mode);

/**
 * @brief  Convertit le mode sommeil en chaîne lisible.
 * @param  mode     Mode à convertir.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_sleep_mode_to_string(int mode, char *out, size_t out_size);

/* ----------- Puissance RF ----------- */

/**
 * @brief  Récupère la puissance RF actuelle.
 * @param  dbm  Pointeur vers la valeur de sortie (dBm).
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_rf_power(int *dbm);

/**
 * @brief  Définit la puissance RF.
 * @param  dbm  Valeur à appliquer (dBm).
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_set_rf_power(int dbm);

/* ----------- Logs système ----------- */

/**
 * @brief  Récupère le niveau de log système.
 * @param  level  Pointeur vers la valeur de sortie.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_syslog(int *level);

/**
 * @brief  Définit le niveau de log système.
 * @param  level  Niveau à appliquer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_set_syslog(int level);

/**
 * @brief  Convertit le niveau de log système en chaîne lisible.
 * @param  syslog   Niveau à convertir.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_syslog_to_string(int syslog, char *out, size_t out_size);

/* ----------- RAM libre ----------- */

/**
 * @brief  Récupère la RAM libre sur l’ESP01.
 * @param  free_ram  Pointeur vers la valeur de sortie.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_sysram(uint32_t *free_ram);

/**
 * @brief  Met l’ESP01 en deep sleep pour une durée donnée.
 * @param  ms  Durée en millisecondes.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_deep_sleep(uint32_t ms);

/* ----------- Liste commandes AT ----------- */

/**
 * @brief  Récupère la liste des commandes AT supportées.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size);

/* ----------- Utilitaires & Debug ----------- */

/**
 * @brief  Retourne une chaîne lisible pour un code d’erreur.
 * @param  status  Code d’erreur.
 * @retval Chaîne descriptive.
 */
const char *esp01_get_error_string(ESP01_Status_t status);

/**
 * @brief  Attend un motif dans la réponse UART.
 * @param  pattern     Motif à attendre.
 * @param  timeout_ms  Timeout en ms.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);

/**
 * @brief  Envoie une commande AT brute en DMA et attend une réponse.
 * @param  cmd           Commande à envoyer.
 * @param  resp          Buffer de réponse.
 * @param  resp_size     Taille du buffer réponse.
 * @param  wait_pattern  Motif à attendre.
 * @param  timeout_ms    Timeout en ms.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *resp, size_t resp_size, const char *wait_pattern, uint32_t timeout_ms);

/**
 * @brief  Vérifie si la taille d'un buffer est suffisante.
 * @param  needed     Taille nécessaire.
 * @param  available  Taille disponible.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_check_buffer_size(size_t needed, size_t available);

/* ==================== OUTILS DE PARSING ==================== */

/**
 * @brief  Analyse une réponse pour extraire un entier 32 bits après un motif donné.
 * @param  resp     Réponse à analyser.
 * @param  pattern  Motif à chercher dans la réponse.
 * @param  out      Pointeur vers l'entier où stocker le résultat.
 * @retval ESP01_Status_t  ESP01_OK si réussi, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_parse_int_after(const char *resp, const char *pattern, int32_t *out);

/**
 * @brief  Extrait une chaîne après un motif et jusqu'à \r, \n ou la fin.
 * @param  resp     Chaîne de réponse à analyser.
 * @param  pattern  Motif à chercher (ex: "+UART", "+UART_CUR").
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_Status_t  ESP01_OK si trouvé, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_parse_string_after(const char *resp, const char *pattern, char *out, size_t out_size);

/**
 * @brief  Extrait la première chaîne entre guillemets après un motif dans une réponse AT.
 * @param  src      Chaîne source (réponse AT complète).
 * @param  motif    Motif à chercher (ex: "+CIFSR:STAIP,\"").
 * @param  out      Buffer de sortie pour la valeur extraite.
 * @param  out_len  Taille du buffer de sortie.
 * @retval true si la valeur a été trouvée et extraite, false sinon.
 */
bool esp01_extract_quoted_value(const char *src, const char *motif, char *out, size_t out_len);

/**
 * @brief  Parse un booléen (0/1 ou true/false) après un tag dans une réponse AT.
 */
ESP01_Status_t esp01_parse_bool_after(const char *resp, const char *tag, bool *out);

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

/* ========================= TERMINAL AT (CONSOLE) ======================= */

/**
 * @brief  Initialise la réception IT pour le terminal AT sur l’UART de debug.
 * @param  huart_debug  UART utilisé pour la console.
 */
void esp01_terminal_begin(UART_HandleTypeDef *huart_debug);

/**
 * @brief  Callback de réception UART pour le terminal AT.
 * @param  huart  UART concerné.
 */
void esp01_console_rx_callback(UART_HandleTypeDef *huart);

/**
 * @brief  Tâche de gestion du terminal AT (console série).
 */
void esp01_console_task(void);

void esp01_parse_gmr_version(const char *gmr_buf, char *version_buf, size_t version_buf_size);

#endif /* STM32_WIFIESP_H_ */
