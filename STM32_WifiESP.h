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

#include "main.h"    // HAL, UART, etc.
#include <stdbool.h> // Types booléens
#include <stddef.h>  // Types de taille (size_t, etc.)
#include <stdint.h>  // Types entiers standard (uint8_t, uint16_t, etc.)
#include <string.h>  // Pour strlen, strcpy dans les fonctions inline
/* =========================== DEFINES ========================== */

// ----------- DEBUG -----------
#define ESP01_DEBUG 1 // 1 = logs de debug activés, 0 = désactivés

// ----------- CONSTANTES -----------
#define ESP01_DMA_RX_BUF_SIZE 1024 // Taille buffer DMA RX UART
#define ESP01_MAX_CMD_BUF 512      // Taille max buffer commande AT
#define ESP01_MAX_LINE_BUF 256     // Taille max buffer ligne (pour les réponses AT)
#define ESP01_MAX_RESP_BUF 2048    // Taille max buffer réponse
#define ESP01_LARGE_RESP_BUF 4096  // Taille buffer réponse large (pour les réponses longues)
#define ESP01_SMALL_BUF_SIZE 64    // Taille petit buffer (usage courant)
#define ESP01_TIMEOUT_SHORT 2000   // Timeout court (ms)
#define ESP01_TIMEOUT_MEDIUM 7000  // Timeout moyen (ms)
#define ESP01_TIMEOUT_LONG 15000   // Timeout long (ms)

// ----------- TIMEOUT GÉNÉRIQUE -----------
#define ESP01_AT_COMMAND_TIMEOUT 2000 // Timeout commande AT générique (ms)

/* =========================== TYPES & STRUCTURES ============================ */
/**
 * @brief  Codes de statut pour les fonctions du driver ESP01.
 */
typedef enum
{
    ESP01_OK = 0,                   // Opération réussie
    ESP01_FAIL,                     // Échec générique
    ESP01_EXIT,                     // Sortie demandée
    ESP01_TIMEOUT,                  // Timeout sur opération
    ESP01_INVALID_PARAM,            // Paramètre invalide
    ESP01_BUFFER_OVERFLOW,          // Dépassement de buffer
    ESP01_UNEXPECTED_RESPONSE,      // Réponse inattendue
    ESP01_NOT_INITIALIZED,          // Module non initialisé
    ESP01_NOT_DETECTED,             // Module non détecté
    ESP01_CMD_TOO_LONG,             // Commande AT trop longue
    ESP01_MEMORY_ERROR,             // Erreur mémoire
    ESP01_PARSE_ERROR,              // Erreur de parsing
    ESP01_NOT_CONNECTED,            // Non connecté
    ESP01_ALREADY_CONNECTED,        // Déjà connecté
    ESP01_CONNECTION_ERROR,         // Erreur de connexion
    ESP01_ROUTE_NOT_FOUND,          // Route non trouvée
    ESP01_GMR_PARSE_ERROR,          // Erreur parsing GMR
    ESP01_WIFI_NOT_CONNECTED = 100, // WiFi non connecté
    ESP01_WIFI_TIMEOUT,             // Timeout WiFi
    ESP01_WIFI_WRONG_PASSWORD,      // Mauvais mot de passe WiFi
    ESP01_WIFI_AP_NOT_FOUND,        // Point d'accès non trouvé
    ESP01_WIFI_CONNECT_FAIL,        // Échec connexion WiFi
    ESP01_HTTP_PARSE_ERROR = 200,   // Erreur parsing HTTP
    ESP01_HTTP_INVALID_REQUEST,     // Requête HTTP invalide
    ESP01_HTTP_TIMEOUT,             // Timeout HTTP
    ESP01_HTTP_CONNECTION_REFUSED,  // Connexion HTTP refusée
    ESP01_MQTT_NOT_CONNECTED = 300, // MQTT non connecté
    ESP01_MQTT_PROTOCOL_ERROR,      // Erreur protocole MQTT
    ESP01_MQTT_SUBSCRIPTION_FAILED, // Abonnement MQTT échoué
    ESP01_MQTT_PUBLISH_FAILED,      // Publication MQTT échouée
    ESP01_NTP_SYNC_ERROR = 400,     // Erreur de synchro NTP
    ESP01_NTP_INVALID_RESPONSE,     // Réponse NTP invalide
    ESP01_NTP_SERVER_NOT_REACHABLE  // Serveur NTP injoignable
} ESP01_Status_t;                   // Enum statut driver ESP01

/* ========================= VARIABLES GLOBALES EXTERNES ========================= */
extern UART_HandleTypeDef *g_esp_uart;   // UART principal ESP01
extern UART_HandleTypeDef *g_debug_uart; // UART debug
extern uint8_t *g_dma_rx_buf;            // Buffer DMA RX
extern uint16_t g_dma_buf_size;          // Taille buffer DMA
extern volatile uint16_t g_rx_last_pos;  // Dernière position RX
extern uint16_t g_server_port;           // Port serveur

/* ========================= MACROS UTILES & LOGS ============================== */
void _esp_login(const char *fmt, ...);
#define VALIDATE_PARAM_VOID(cond) \
    do                            \
    {                             \
        if (!(cond))              \
            return;               \
    } while (0)

#define VALIDATE_PARAM(expr, errcode) \
    do                                \
    {                                 \
        if (!(expr))                  \
            return (errcode);         \
    } while (0)

#define VALIDATE_PARAMS(errcode, ...) \
    do                                \
    {                                 \
        if (!(__VA_ARGS__))           \
            return (errcode);         \
    } while (0)

#define VALIDATE_PARAMS_VOID(...) \
    do                            \
    {                             \
        if (!(__VA_ARGS__))       \
            return;               \
    } while (0)

#define ESP01_LOG_DEBUG(module, fmt, ...) _esp_login("[" module "][DEBUG] " fmt "\r\n", ##__VA_ARGS__)
#define ESP01_LOG_ERROR(module, fmt, ...) _esp_login("[" module "][ERROR] " fmt "\r\n", ##__VA_ARGS__)
#define ESP01_LOG_WARN(module, fmt, ...) _esp_login("[" module "][WARN] " fmt "\r\n", ##__VA_ARGS__)

#define ESP01_RETURN_ERROR(prefix, status)                                          \
    do                                                                              \
    {                                                                               \
        _esp_login(">>> [%s] Erreur : %s", prefix, esp01_get_error_string(status)); \
        return (status);                                                            \
    } while (0)

/* ========================= FONCTIONS PRINCIPALES (initialisation, gestion du module, buffer, etc.) ========================= */

/**
 * @brief Initialise le driver ESP01 (UART, DMA, variables globales).
 * @param huart_esp UART pour l'ESP01
 * @param huart_debug UART pour la console debug
 * @param dma_rx_buf Buffer DMA RX
 * @param dma_buf_size Taille du buffer DMA RX
 * @retval ESP01_Status_t Statut de l'initialisation
 */
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);

/**
 * @brief Vide le buffer RX UART (flush DMA).
 * @param timeout_ms Timeout en ms
 * @retval ESP01_Status_t Statut du flush
 */
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms);

/**
 * @brief Récupère les nouveaux octets reçus depuis le dernier appel.
 * @param buf Buffer de destination
 * @param bufsize Taille du buffer
 * @retval int Nombre d'octets lus
 */
int esp01_get_new_data(uint8_t *buf, uint16_t bufsize);

/* ========================= WRAPPERS AT & HELPERS ASSOCIÉS (par commande AT) ========================= */

/**
 * @defgroup ESP01_AT_WRAPPERS Wrappers AT et helpers associés (par commande AT)
 * @brief  Fonctions exposant chaque commande AT à l'utilisateur, avec leurs helpers de parsing/affichage.
 *
 * | Commande AT      | Wrapper principal(s)                | Helpers associés                        | Description courte                  |
 * |------------------|-------------------------------------|-----------------------------------------|-------------------------------------|
 * | AT               | esp01_test_at                       | INUTILE                                 | Teste la communication AT           |
 * | AT+RST           | esp01_reset                         | INUTILE                                 | Redémarre le module                 |
 * | AT+RESTORE       | esp01_restore                       | INUTILE                                 | Restaure les paramètres par défaut  |
 * | AT+GMR           | esp01_get_at_version                | esp01_display_firmware_info             | Informations de version             |
 * | AT+CMD           | esp01_get_cmd_list                  | INUTILE                                 | Liste les commandes AT supportées   |
 * | AT+GSLP          | esp01_deep_sleep                    | INUTILE                                 | Mode deep sleep                     |
 * | AT+SLEEP         | esp01_get_sleep_mode                | esp01_sleep_mode_to_string              | Mode de sommeil                     |
 * |                  | esp01_set_sleep_mode                | esp01_sleep_mode_to_string              |                                     |
 * | AT+SYSRAM        | esp01_get_sysram                    | esp01_sysram_to_string                  | Utilisation RAM                     |
 * | AT+SYSFLASH      | esp01_get_sysflash                  | esp01_sysflash_to_string                | Partitions flash                    |
 * |                  |                                     | esp01_display_sysflash_partitions        |                                     |
 * | AT+RFPOWER       | esp01_get_rf_power                  | esp01_rf_power_to_string                | Puissance RF                        |
 * |                  | esp01_set_rf_power                  | INUTILE                                 |                                     |
 * | AT+SYSLOG        | esp01_get_syslog                    | esp01_syslog_to_string                  | Journaux de debug                   |
 * |                  | esp01_set_syslog                    | esp01_syslog_to_string                  |                                     |
 * | AT+SYSSTORE      | esp01_get_sysstore                  | esp01_sysstore_to_string                | Mode de stockage                    |
 * | AT+USERRAM       | esp01_get_userram                   | esp01_userram_to_string                 | RAM utilisateur                     |
 * | AT+UART          | esp01_get_uart_config               | esp01_uart_config_to_string             | Paramètres UART                     |
 * |                  | esp01_set_uart_config               | esp01_uart_config_to_string             |                                     |
 */

/**
 * @brief Teste la communication AT (AT)
 */
ESP01_Status_t esp01_test_at(void);

/**
 * @brief Redémarre le module (AT+RST)
 */
ESP01_Status_t esp01_reset(void);

/**
 * @brief Restaure les paramètres usine (AT+RESTORE)
 */
ESP01_Status_t esp01_restore(void);

/**
 * @brief Récupère la version AT du firmware (AT+GMR)
 */
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size);
/**
 * @brief Affiche les infos firmware à partir de la réponse AT+GMR
 */
uint8_t esp01_display_firmware_info(const char *gmr_resp);

/**
 * @brief Liste les commandes AT supportées (AT+CMD?)
 */
ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size);

/**
 * @brief Met le module en deep sleep (AT+GSLP)
 */
ESP01_Status_t esp01_deep_sleep(uint32_t ms);

/**
 * @brief Récupère ou définit le mode sommeil (AT+SLEEP)
 */
ESP01_Status_t esp01_get_sleep_mode(int *mode);
ESP01_Status_t esp01_set_sleep_mode(int mode);
/**
 * @brief Convertit le mode sommeil en chaîne lisible
 */
ESP01_Status_t esp01_sleep_mode_to_string(int mode, char *out, size_t out_size);

/**
 * @brief Récupère l'utilisation RAM (AT+SYSRAM)
 */
ESP01_Status_t esp01_get_sysram(uint32_t *free_ram, uint32_t *min_ram);
/**
 * @brief Formate la RAM en chaîne lisible
 */
ESP01_Status_t esp01_sysram_to_string(uint32_t free_ram, uint32_t min_ram, char *out, size_t out_size);

/**
 * @brief Récupère la partition flash (AT+SYSFLASH)
 */
ESP01_Status_t esp01_get_sysflash(char *out, size_t out_size);
/**
 * @brief Formate la flash en chaîne lisible
 */
ESP01_Status_t esp01_sysflash_to_string(uint32_t sysflash, char *out, size_t out_size);
/**
 * @brief Affiche les partitions flash à partir de la réponse AT+SYSFLASH?
 */
uint8_t esp01_display_sysflash_partitions(const char *sysflash_resp);

/**
 * @brief Récupère ou définit la puissance RF (AT+RFPOWER)
 */
ESP01_Status_t esp01_get_rf_power(int *dbm);
ESP01_Status_t esp01_set_rf_power(int dbm);

/**
 * @brief Récupère ou définit le niveau de log système (AT+SYSLOG)
 */
ESP01_Status_t esp01_get_syslog(int *level);
ESP01_Status_t esp01_set_syslog(int level);
/**
 * @brief Convertit le niveau de log en chaîne lisible
 */
ESP01_Status_t esp01_syslog_to_string(int syslog, char *out, size_t out_size);

/**
 * @brief Récupère ou définit le mode de stockage (AT+SYSSTORE)
 */
ESP01_Status_t esp01_get_sysstore(uint32_t *sysstore);
/**
 * @brief Formate le mode de stockage en chaîne lisible
 */
ESP01_Status_t esp01_sysstore_to_string(uint32_t sysstore, char *out, size_t out_size);

/**
 * @brief Récupère la RAM utilisateur (AT+USERRAM)
 */
ESP01_Status_t esp01_get_userram(uint32_t *userram);
/**
 * @brief Formate la RAM utilisateur en chaîne lisible
 */
ESP01_Status_t esp01_userram_to_string(uint32_t userram, char *out, size_t out_size);

/**
 * @brief Récupère ou définit la config UART (AT+UART)
 */
ESP01_Status_t esp01_get_uart_config(char *out, size_t out_size);
ESP01_Status_t esp01_set_uart_config(uint32_t baud, uint8_t databits, uint8_t stopbits, uint8_t parity, uint8_t flowctrl);
/**
 * @brief Convertit la config UART brute en chaîne lisible
 */
ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size);

/* ========================= OUTILS DE PARSING ========================= */

/**
 * @brief Parse un entier après un motif dans une chaîne.
 * @param text Chaîne source
 * @param pattern Motif à chercher
 * @param result Pointeur vers la valeur extraite
 * @retval ESP01_Status_t Statut du parsing
 */
ESP01_Status_t esp01_parse_int_after(const char *text, const char *pattern, int32_t *result);

/**
 * @brief Parse une chaîne après un motif dans une chaîne source.
 * @param text Chaîne source
 * @param pattern Motif à chercher
 * @param output Buffer de sortie
 * @param size Taille du buffer
 * @retval ESP01_Status_t Statut du parsing
 */
ESP01_Status_t esp01_parse_string_after(const char *text, const char *pattern, char *output, size_t size);

/**
 * @brief Extrait une valeur entre guillemets après un motif.
 * @param src Source
 * @param motif Motif à chercher
 * @param out Buffer de sortie
 * @param out_len Taille du buffer
 * @retval bool true si trouvé, false sinon
 */
bool esp01_extract_quoted_value(const char *src, const char *motif, char *out, size_t out_len);

/**
 * @brief Parse un booléen après un tag dans une réponse.
 * @param resp Réponse source
 * @param tag Tag à chercher
 * @param out Pointeur vers la valeur extraite
 * @retval ESP01_Status_t Statut du parsing
 */
ESP01_Status_t esp01_parse_bool_after(const char *resp, const char *tag, bool *out);

/**
 * @brief Découpe une réponse multi-lignes en lignes distinctes.
 * @param input_str Chaîne source
 * @param lines Tableau de pointeurs de lignes
 * @param max_lines Nombre max de lignes
 * @param lines_buffer Buffer pour les lignes
 * @param buffer_size Taille du buffer
 * @param skip_empty Sauter les lignes vides
 * @retval uint8_t Nombre de lignes extraites
 */
uint8_t esp01_split_response_lines(const char *input_str, char *lines[], uint8_t max_lines, char *lines_buffer, size_t buffer_size, bool skip_empty);

/* ========================= OUTILS UTILITAIRES (BUFFER, INLINE, VALIDATION) ========================= */

/**
 * @brief Retourne une chaîne descriptive correspondant au code d'erreur ESP01 fourni.
 *
 * Cette fonction traduit un code de statut ESP01_Status_t en un message d'erreur lisible.
 *
 * @param status Code de statut ESP01_Status_t à traduire.
 * @return Pointeur constant vers une chaîne décrivant l'erreur.
 */
const char *esp01_get_error_string(ESP01_Status_t status);

/**
 * @brief Attend l'apparition d'un motif dans la réponse UART.
 * @param pattern Motif à attendre
 * @param timeout_ms Timeout en ms
 * @retval ESP01_Status_t Statut de l'attente
 */
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);

/**
 * @brief Envoie une commande AT brute en DMA et attend une réponse.
 * @param cmd Commande AT à envoyer
 * @param resp Buffer de réponse
 * @param resp_size Taille du buffer réponse
 * @param wait_pattern Motif d'attente
 * @param timeout_ms Timeout en ms
 * @retval ESP01_Status_t Statut de la commande
 */
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *resp, size_t resp_size, const char *wait_pattern, uint32_t timeout_ms);

/**
 * @brief Supprime les espaces et les caractères de contrôle d'une chaîne.
 * @param str Chaîne à nettoyer
 * @note Modifie la chaîne en place
 */
void esp01_trim_string(char *str);

/**
 * @brief Copie une chaîne source dans une destination de façon sécurisée.
 * @param dst Buffer de destination
 * @param dst_size Taille du buffer destination
 * @param src Chaîne source
 * @retval ESP01_Status_t Statut de la copie
 */
static inline ESP01_Status_t esp01_safe_strcpy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || !src || dst_size == 0)
        return ESP01_INVALID_PARAM;
    if (strlen(src) >= dst_size)
        return ESP01_BUFFER_OVERFLOW;
    strcpy(dst, src);
    return ESP01_OK;
}

/**
 * @brief Vérifie si la taille disponible est suffisante pour le besoin.
 * @param needed Taille nécessaire
 * @param avail Taille disponible
 * @retval ESP01_Status_t Statut de la vérification
 */
static inline ESP01_Status_t esp01_check_buffer_size(size_t needed, size_t avail)
{
    return (needed > avail) ? ESP01_BUFFER_OVERFLOW : ESP01_OK;
}

/**
 * @brief Vérifie si un pointeur est valide (non NULL).
 * @param ptr Pointeur à tester
 * @retval bool true si valide, false sinon
 */
static inline bool esp01_is_valid_ptr(const void *ptr)
{
    return (ptr != NULL);
}

/**
 * @brief Concatène une chaîne source à une destination de façon sécurisée.
 * @param dst Buffer de destination
 * @param dst_size Taille du buffer destination
 * @param src Chaîne source
 * @retval ESP01_Status_t Statut de la concaténation
 */
static inline ESP01_Status_t esp01_safe_strcat(char *dst, size_t dst_size, const char *src)
{
    if (!dst || !src || dst_size == 0)
        return ESP01_INVALID_PARAM;
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);
    if (dst_len + src_len + 1 > dst_size)
        return ESP01_BUFFER_OVERFLOW;
    strcat(dst, src);
    return ESP01_OK;
}

/* ========================= TERMINAL / CONSOLE AT ========================= */
/**
 * @brief Initialise le terminal AT sur l'UART debug.
 * @param huart_debug UART debug
 */
void esp01_terminal_begin(UART_HandleTypeDef *huart_debug);

/**
 * @brief Callback de réception UART pour la console AT.
 * @param huart UART concerné
 */
void esp01_console_rx_callback(UART_HandleTypeDef *huart);

/**
 * @brief Tâche principale de gestion du terminal AT (à appeler en boucle).
 */
void esp01_console_task(void);

#endif /* STM32_WIFIESP_H_ */
