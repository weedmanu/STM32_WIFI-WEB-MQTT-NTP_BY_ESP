/**
 ******************************************************************************
 * @file    STM32_WifiESP.c
 * @author  [Ton Nom]
 * @version 1.2.0
 * @date    [Date]
 * @brief   Implémentation du driver bas niveau ESP01 (UART, AT, debug, reset, etc)
 *
 * @details
 * Ce fichier source contient l’implémentation des fonctions bas niveau pour le module ESP01 :
 * - Initialisation et configuration UART (DMA, IT)
 * - Gestion des commandes AT (envoi, réception, parsing)
 * - Gestion du terminal série (console AT interactive)
 * - Fonctions utilitaires : reset, restore, logs, gestion du mode sommeil, puissance RF, etc.
 * - Statistiques d’utilisation et gestion des erreurs
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la configuration de 2 UART (ESP01 + debug/console).
 * - Utilise la réception DMA circulaire pour l’ESP01 et IT pour la console.
 * - Toutes les fonctions sont utilisables sans connexion WiFi.
 ******************************************************************************
 */

#include "STM32_WifiESP.h" // Header du driver ESP01
#include <stdlib.h>        // Pour malloc, free
#include <stdarg.h>        // Pour va_list, va_start, va_end
#include <string.h>        // Pour memcpy, strlen, strstr
#include <stdio.h>         // Pour snprintf, vsnprintf

// ==================== VARIABLES GLOBALES ====================

esp01_stats_t g_stats = {0};             // Statistiques d'utilisation du module ESP01
UART_HandleTypeDef *g_esp_uart = NULL;   // UART pour communication avec l'ESP01
UART_HandleTypeDef *g_debug_uart = NULL; // UART pour debug/console AT
uint8_t *g_dma_rx_buf = NULL;            // Buffer DMA pour réception UART
uint16_t g_dma_buf_size = 0;             // Taille du buffer DMA RX
volatile uint16_t g_rx_last_pos = 0;     // Dernière position lue dans le buffer DMA RX
uint16_t g_server_port = 80;             // Port par défaut du serveur HTTP

// === Variables terminal AT ===
volatile uint8_t esp_console_rx_flag = 0;                   // Indicateur de réception d'un caractère dans le terminal AT
volatile uint8_t esp_console_rx_char = 0;                   // Caractère reçu dans le terminal AT
volatile char esp_console_cmd_buf[ESP01_MAX_CMD_BUF] = {0}; // Buffer pour la commande AT en cours
volatile int esp_console_cmd_idx = 0;                       // Index de la commande AT en cours
volatile uint8_t esp_console_cmd_ready = 0;                 // Indicateur que la commande AT est prête à être traitée

// ==================== LOGS & DEBUG ====================

/**
 * @brief  Log interne formaté sur l'UART de debug.
 * @param  fmt  Format printf.
 * @param  ...  Arguments variables.
 */
void _esp_login(const char *fmt, ...)
{
#if ESP01_DEBUG              // Si le debug est activé
    if (g_debug_uart && fmt) // Si l'UART de debug est initialisée et le format n'est pas NULL
    {
        char buf[ESP01_MAX_CMD_BUF];                                                 // Buffer pour le message formaté
        va_list args;                                                                // Liste d'arguments variable pour le format printf
        va_start(args, fmt);                                                         // Initialisation de la liste d'arguments
        vsnprintf(buf, sizeof(buf), fmt, args);                                      // Formatage du message dans le buffer
        va_end(args);                                                                // Fin de la liste d'arguments
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)buf, strlen(buf), HAL_MAX_DELAY); // Envoi du message formaté sur l'UART de debug
        const char crlf[] = "\r\n";                                                  // Caractères de fin de ligne
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)crlf, 2, HAL_MAX_DELAY);          // Envoi des caractères de fin de ligne
    }
#endif
}

// ==================== INITIALISATION & DRIVER ====================

/**
 * @brief  Initialise le driver ESP01 et configure les UART et DMA.
 * @param  huart_esp    Pointeur sur l’UART utilisé pour l’ESP01 (AT).
 * @param  huart_debug  Pointeur sur l’UART utilisé pour le debug/console.
 * @param  dma_rx_buf   Buffer DMA pour la réception UART ESP01.
 * @param  dma_buf_size Taille du buffer DMA RX.
 * @retval ESP01_Status_t  Code de statut (OK ou erreur).
 */
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
                          uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
    VALIDATE_PARAM(huart_esp && huart_debug && dma_rx_buf && dma_buf_size > 0, ESP01_INVALID_PARAM); // Validation des paramètres

    g_esp_uart = huart_esp;        // Sauvegarde le pointeur UART ESP01 global
    g_debug_uart = huart_debug;    // Sauvegarde le pointeur UART debug global
    g_dma_rx_buf = dma_rx_buf;     // Sauvegarde le buffer DMA RX global
    g_dma_buf_size = dma_buf_size; // Sauvegarde la taille du buffer DMA RX
    g_server_port = 80;            // Définit le port serveur HTTP par défaut

    // Lance la réception DMA sur l’UART ESP01
    if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK)
    {
        _esp_login(">>> [DEBUG][INIT] Erreur initialisation DMA RX : %s", esp01_get_error_string(ESP01_FAIL)); // Log d'erreur si échec DMA
        return ESP01_FAIL;                                                                                     // Retourne une erreur
    }

    _esp_login(">>> [DEBUG][INIT] Initialisation du driver ESP01 : OK"); // Log de succès
    return ESP01_OK;                                                     // Retourne OK si tout est bon
}

// ==================== UTILITAIRES DMA/RX & BUFFER ====================

/**
 * @brief  Vide le buffer DMA/RX de l’ESP01 pendant un temps donné.
 * @param  timeout_ms  Durée maximale (en ms) pendant laquelle on vide le buffer.

 * @retval ESP01_Status_t  Code de statut (OK).
 */
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();              // Sauvegarde le temps de départ
    while ((HAL_GetTick() - start) < timeout_ms) // Boucle jusqu'à expiration du timeout
    {
        uint8_t dummy[ESP01_SMALL_BUF_SIZE];                // Buffer temporaire pour lecture
        int len = esp01_get_new_data(dummy, sizeof(dummy)); // Lit les nouvelles données reçues
        if (len == 0)                                       // Si rien à lire, attend un peu
            HAL_Delay(1);
    }
    _esp_login(">>> [DEBUG][FLUSH] Buffer UART/DMA vidé"); // Log de fin de flush
    return ESP01_OK;                                       // Retourne OK
}

/**
 * @brief  Récupère les nouvelles données reçues via DMA.
 * @param  buf      Buffer de sortie.
 * @param  bufsize  Taille du buffer.
 * @retval Nombre d'octets lus.
 */
int esp01_get_new_data(uint8_t *buf, uint16_t bufsize)
{
    if (!g_dma_rx_buf || !buf || bufsize == 0) // Validation des paramètres
        return 0;

    uint16_t pos = g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx); // Calcule la position courante du DMA
    int len = 0;

    if (pos != g_rx_last_pos) // Si de nouvelles données sont arrivées
    {
        if (pos > g_rx_last_pos)
        {
            len = pos - g_rx_last_pos;                      // Nombre d'octets à lire
            memcpy(buf, &g_dma_rx_buf[g_rx_last_pos], len); // Copie les nouveaux octets
        }
        else
        {
            len = g_dma_buf_size - g_rx_last_pos; // Partie jusqu'à la fin du buffer
            memcpy(buf, &g_dma_rx_buf[g_rx_last_pos], len);
            if (pos > 0)
            {
                memcpy(buf + len, &g_dma_rx_buf[0], pos); // Partie depuis le début du buffer
                len += pos;
            }
        }
        g_rx_last_pos = pos; // Met à jour la dernière position lue
    }
    return len;
}

/**
 * @brief  Vide le buffer RX (utilitaire interne, non DMA).
 * @param  timeout_ms  Durée maximale (en ms) pendant laquelle on vide le buffer.
 */
void _flush_rx_buffer(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();              // Sauvegarde le temps de départ
    uint8_t dummy[ESP01_SMALL_BUF_SIZE];         // Buffer temporaire pour lecture
    while ((HAL_GetTick() - start) < timeout_ms) // Boucle jusqu'à expiration du timeout
    {
        int len = esp01_get_new_data(dummy, sizeof(dummy)); // Lit les nouvelles données reçues
        if (len == 0)
            HAL_Delay(1); // Si rien à lire, attend un peu
    }
    _esp_login(">>> [DEBUG][FLUSH] Buffer RX vidé (utilitaire)");
}

// ==================== ENVOI/RECEPTION COMMANDES AT BAS NIVEAU ====================

/**
 * @brief  Envoie une commande AT brute en DMA et attend une réponse.
 * @param  cmd               Commande AT à envoyer (ex: "AT+GMR").
 * @param  response_buffer   Buffer de réception pour la réponse complète.
 * @param  response_buf_size Taille du buffer de réception.
 * @param  expected          Motif attendu dans la réponse (ex: "OK").
 * @param  timeout_ms        Timeout maximal en millisecondes.
 * @retval ESP01_Status_t    ESP01_OK si motif trouvé, ESP01_TIMEOUT sinon.
 */
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer, size_t response_buf_size,
                                          const char *expected, uint32_t timeout_ms)
{
    VALIDATE_PARAM(cmd && response_buffer && response_buf_size > 0, ESP01_INVALID_PARAM); // Validation des paramètres

    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer

    _esp_login(">>> [DEBUG][RAWCMD] Commande envoyée : %s", cmd); // Log la commande envoyée

    if (!g_esp_uart)
    {
        _esp_login(">>> [DEBUG][RAWCMD] Erreur : %s", esp01_get_error_string(ESP01_NOT_INITIALIZED));
        return ESP01_NOT_INITIALIZED;
    }

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY); // Envoie la commande AT
    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);        // Envoie CRLF

    uint32_t start = HAL_GetTick(); // Timestamp de départ
    size_t resp_len = 0;            // Longueur de la réponse reçue
    response_buffer[0] = '\0';      // Initialise le buffer de réponse

    while ((HAL_GetTick() - start) < timeout_ms && resp_len < response_buf_size - 1)
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)
        {
            if (resp_len + len >= response_buf_size - 1) // Empêche le dépassement de buffer
                len = response_buf_size - 1 - resp_len;
            memcpy(response_buffer + resp_len, buf, len); // Ajoute au buffer de réponse
            resp_len += len;
            response_buffer[resp_len] = '\0'; // Null-terminate

            if (expected && strstr(response_buffer, expected)) // Motif attendu trouvé ?
                break;
        }
        else
        {
            HAL_Delay(1); // Petite pause si rien reçu
        }
    }
    _esp_login(">>> [DEBUG][RAWCMD] Retour de la commande : %s", response_buffer); // Log la réponse complète

    if (!(expected && strstr(response_buffer, expected)))
    {
        _esp_login(">>> [DEBUG][RAWCMD] Erreur : %s", esp01_get_error_string(ESP01_TIMEOUT));
        return ESP01_TIMEOUT;
    }

    return ESP01_OK;
}

// ==================== FONCTIONS AT UTILISATEUR (HAUT NIVEAU) ====================

/**
 * @brief  Teste la communication AT avec l’ESP01.
 * @retval ESP01_Status_t  ESP01_OK si la commande AT répond "OK", sinon ESP01_FAIL ou ESP01_TIMEOUT.
 */
ESP01_Status_t esp01_test_at(void)
{
    char resp[ESP01_MAX_RESP_BUF];                                                        // Buffer pour la réponse complète
    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", 1000); // Envoie "AT" et attend "OK"
    _esp_login(">>> [DEBUG][AT] Réponse : %s", resp);                                     // Log la réponse reçue

    return st; // Retourne le statut (OK, TIMEOUT, FAIL)
}

/**
 * @brief  Récupère la version AT du firmware ESP01.
 * @param  version_buf  Buffer de sortie pour la version.
 * @param  buf_size     Taille du buffer de sortie.
 * @retval ESP01_Status_t  ESP01_OK si la version est lue, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size)
{
    VALIDATE_PARAM(version_buf && buf_size > 0, ESP01_INVALID_PARAM); // Validation des paramètres

    char resp[ESP01_MAX_RESP_BUF];                                                                           // Buffer pour la réponse complète
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+GMR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie "AT+GMR" et attend "OK"
    _esp_login(">>> [DEBUG][GMR] Réponse : %s", resp);                                                       // Log la réponse reçue

    if (st != ESP01_OK)
    {
        _esp_login(">>> [GMR] Erreur : %s", esp01_get_error_string(st));
        return ESP01_FAIL;
    }

    strncpy(version_buf, resp, buf_size - 1);                 // Copie la réponse dans le buffer utilisateur
    version_buf[buf_size - 1] = '\0';                         // Ajoute le zéro terminal
    _esp_login(">>> [DEBUG][GMR] Version : %s", version_buf); // Log la version extraite

    return ESP01_OK; // Retourne OK si tout est bon
}

/**
 * @brief  Vérifie l’état de connexion du module ESP01 au WiFi.
 * @retval ESP01_Status_t  ESP01_OK si connecté, ESP01_WIFI_NOT_CONNECTED sinon.
 */
ESP01_Status_t esp01_get_connection_status(void)
{
    char resp[ESP01_MAX_RESP_BUF];                                                                              // Buffer pour la réponse complète
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CWJAP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie "AT+CWJAP?" et attend "OK"
    _esp_login(">>> [STATUS] Réponse : %s", resp);                                                              // Log la réponse reçue

    if (st != ESP01_OK)
    {
        _esp_login(">>> [STATUS] Erreur : %s", esp01_get_error_string(st));
        return ESP01_FAIL;
    }

    if (strstr(resp, "+CWJAP:")) // Si la réponse contient "+CWJAP:", le module est connecté
        return ESP01_OK;
    _esp_login(">>> [STATUS] Erreur : %s", esp01_get_error_string(ESP01_WIFI_NOT_CONNECTED));
    return ESP01_WIFI_NOT_CONNECTED; // Sinon, non connecté
}

/**
 * @brief  Effectue un reset logiciel du module ESP01 via la commande AT+RST.
 * @retval ESP01_Status_t  ESP01_OK si le reset et le test AT sont OK, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_reset(void)
{
    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+RST\r\n", 8, HAL_MAX_DELAY); // Envoie la commande AT+RST

    uint32_t start = HAL_GetTick();      // Timestamp de départ
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse complète
    size_t resp_len = 0;

    // Lecture de la réponse complète pendant 3 secondes max
    while ((HAL_GetTick() - start) < 3000 && resp_len < sizeof(resp) - 1)
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)
        {
            if (resp_len + len >= sizeof(resp) - 1) // Empêche le dépassement de buffer
                len = sizeof(resp) - 1 - resp_len;
            memcpy(resp + resp_len, buf, len); // Ajoute au buffer de réponse
            resp_len += len;
            resp[resp_len] = '\0'; // Null-terminate
        }
        else
        {
            HAL_Delay(1); // Petite pause si rien reçu
        }
    }
    _esp_login(">>> [DEBUG][RESET] Réponse complète : %s", resp); // Log la réponse complète

    HAL_Delay(1000); // Attend un peu le redémarrage du module

    // Teste la communication AT après le reset
    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [RESET] Test AT après reset : %s", resp);

    if (st == ESP01_OK)
    {
        _esp_login(">>> [RESET] AT OK après reset, reset réussi");
        return ESP01_OK;
    }
    else
    {
        _esp_login(">>> [RESET] AT échoué après reset : %s", esp01_get_error_string(st));
        return ESP01_FAIL;
    }
}

/**
 * @brief  Restaure les paramètres usine du module ESP01 via la commande AT+RESTORE.
 * @retval ESP01_Status_t  ESP01_OK si le restore et le test AT sont OK, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_restore(void)
{
    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+RESTORE\r\n", 12, HAL_MAX_DELAY); // Envoie la commande AT+RESTORE

    uint32_t start = HAL_GetTick();      // Timestamp de départ
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse complète
    size_t resp_len = 0;

    // Lecture de la réponse complète pendant 3 secondes max
    while ((HAL_GetTick() - start) < 3000 && resp_len < sizeof(resp) - 1)
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)
        {
            if (resp_len + len >= sizeof(resp) - 1) // Empêche le dépassement de buffer
                len = sizeof(resp) - 1 - resp_len;
            memcpy(resp + resp_len, buf, len); // Ajoute au buffer de réponse
            resp_len += len;
            resp[resp_len] = '\0'; // Null-terminate
        }
        else
        {
            HAL_Delay(1); // Petite pause si rien reçu
        }
    }
    _esp_login(">>> [DEBUG][RESTORE] Réponse complète : %s", resp); // Log la réponse complète

    HAL_Delay(1000); // Attend un peu le redémarrage du module

    // Teste la communication AT après le restore
    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [RESTORE] Test AT après restore : %s", resp);

    if (st == ESP01_OK)
    {
        _esp_login(">>> [RESTORE] AT OK après restore, restore réussi");
        return ESP01_OK;
    }
    else
    {
        _esp_login(">>> [RESTORE] AT échoué après restore : %s", esp01_get_error_string(st));
        return ESP01_FAIL;
    }
}

// ==================== CONFIGURATION UART ====================

/**
 * @brief  Récupère la configuration UART courante du module ESP01.
 * @param  out      Buffer de sortie pour la configuration brute.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_get_uart_config(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+UART?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    _esp_login(">>> [DEBUG][UART] Réponse : %s", resp);

    if (st != ESP01_OK)
    {
        _esp_login(">>> [UART] Erreur : %s", esp01_get_error_string(st));
        return st;
    }

    // UART config
    if (esp01_parse_string_after(resp, "+UART", out, out_size) == ESP01_OK ||
        esp01_parse_string_after(resp, "+UART_CUR", out, out_size) == ESP01_OK)
    {
        _esp_login(">>> [DEBUG][UART] Config brute : %s", out);
        return ESP01_OK;
    }
    esp01_log_pattern_not_found("UART", "+UART/+UART_CUR");
    return ESP01_FAIL;
}

/**
 * @brief  Convertit une configuration UART brute en chaîne compréhensible.
 * @param  raw_config  Chaîne brute (ex: "115200,8,1,0,0").
 * @param  out        Buffer de sortie pour la chaîne compréhensible.
 * @param  out_size   Taille du buffer de sortie.
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size)
{
    VALIDATE_PARAM(raw_config && out && out_size > 0, ESP01_INVALID_PARAM); // Validation des paramètres

    uint32_t baud = 0;                                                                   // Baudrate
    int data = 0, stop = 0, parity = 0, flow = 0;                                        // Paramètres UART
    if (sscanf(raw_config, "%lu,%d,%d,%d,%d", &baud, &data, &stop, &parity, &flow) != 5) // Parse la chaîne brute
    {
        _esp_login(">>> [DEBUG][UART] Erreur : parsing config UART");
        return ESP01_FAIL; // Retourne FAIL si parsing échoué
    }

    const char *parity_str = "aucune"; // Parité par défaut
    if (parity == 1)
        parity_str = "impair";
    else if (parity == 2)
        parity_str = "pair";

    const char *flow_str = "aucun"; // Contrôle de flux par défaut
    if (flow == 1)
        flow_str = "RTS";
    else if (flow == 2)
        flow_str = "CTS";
    else if (flow == 3)
        flow_str = "RTS+CTS";

    snprintf(out, out_size, "baudrate=%lu, data bits=%d, stop bits=%d, parité=%s, flow control=%s",
             baud, data, stop, parity_str, flow_str); // Formate la chaîne compréhensible

    _esp_login(">>> [DEBUG][UART] Config compréhensible : %s", out); // Log la config compréhensible
    return ESP01_OK;                                                 // Retourne OK si tout est bon
}

/**
 * @brief  Définit la configuration UART du module ESP01.
 * @param  baud      Baudrate souhaité.
 * @param  databits  Nombre de bits de données.
 * @param  stopbits  Nombre de bits de stop.
 * @param  parity    Parité (0: aucune, 1: impair, 2: pair).
 * @param  flowctrl  Contrôle de flux (0: aucun, 1: RTS, 2: CTS, 3: RTS+CTS).
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_set_uart_config(uint32_t baud, uint8_t databits, uint8_t stopbits, uint8_t parity, uint8_t flowctrl)
{
    VALIDATE_PARAM(databits >= 5 && databits <= 8 && stopbits >= 1 && stopbits <= 2 && parity <= 2 && flowctrl <= 3, ESP01_INVALID_PARAM);

    char cmd[64];                                                                                      // Buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+UART=%lu,%u,%u,%u,%u", baud, databits, stopbits, parity, flowctrl); // Formate la commande

    char resp[ESP01_SMALL_BUF_SIZE * 4] = {0};                                                          // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"
    _esp_login(">>> [DEBUG][UART] Réponse : %s", resp);                                                 // Log la réponse
    if (st != ESP01_OK)
    {
        _esp_login(">>> [UART] Erreur : %s", esp01_get_error_string(st));
    }
    return st; // Retourne le statut
}

// ==================== MODE SOMMEIL ====================

/**
 * @brief  Récupère le mode sommeil actuel du module ESP01.
 * @param  mode  Pointeur vers un int où sera stocké le mode (0, 1 ou 2).
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_get_sleep_mode(int *mode)
{
    VALIDATE_PARAM(mode, ESP01_INVALID_PARAM); // Validation du paramètre

    char resp[ESP01_MAX_RESP_BUF];                                                                              // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SLEEP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+SLEEP?
    _esp_login(">>> [DEBUG][SLEEP] Réponse : %s", resp);                                                        // Log la réponse

    if (st != ESP01_OK)
    {
        _esp_login(">>> [SLEEP] Erreur : %s", esp01_get_error_string(st)); // Log erreur si la commande échoue
        return ESP01_FAIL;
    }

    int32_t mode_tmp = 0;                                             // Variable temporaire pour le parsing
    if (esp01_parse_int_after(resp, "+SLEEP", &mode_tmp) == ESP01_OK) // Extraction du mode
    {
        *mode = (int)mode_tmp;                                          // Stocke le résultat dans le paramètre utilisateur
        _esp_login(">>> [DEBUG][SLEEP] Mode sommeil brut : %d", *mode); // Log du mode brut
        switch (*mode)
        {
        case 0:
            _esp_login(">>> [SLEEP] Mode sommeil compréhensible : Pas de sommeil (no sleep, 0)");
            break;
        case 1:
            _esp_login(">>> [SLEEP] Mode sommeil compréhensible : Sommeil léger (light sleep, 1)");
            break;
        case 2:
            _esp_login(">>> [SLEEP] Mode sommeil compréhensible : Sommeil modem (modem sleep, 2)");
            break;
        default:
            _esp_login(">>> [SLEEP] Mode sommeil compréhensible : Inconnu");
            break;
        }
        return ESP01_OK;
    }
    esp01_log_pattern_not_found("SLEEP", "+SLEEP"); // Log harmonisé si motif non trouvé
    return ESP01_FAIL;
}

/**
 * @brief  Définit le mode sommeil du module ESP01.
 * @param  mode  Mode à appliquer (0: aucun, 1: light sleep, 2: modem sleep).
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_set_sleep_mode(int mode)
{
    VALIDATE_PARAM(mode >= 0 && mode <= 2, ESP01_INVALID_PARAM); // Validation du paramètre

    char cmd[16], resp[64];                                                                             // Buffers pour la commande et la réponse
    snprintf(cmd, sizeof(cmd), "AT+SLEEP=%d", mode);                                                    // Formate la commande AT
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"
    _esp_login(">>> [DEBUG][SLEEP] Réponse : %s", resp);                                                // Log la réponse
    if (st != ESP01_OK)
    {
        _esp_login(">>> [SLEEP] Erreur : %s", esp01_get_error_string(st)); // Log erreur si la commande échoue
    }
    return st; // Retourne le statut
}

/**
 * @brief  Convertit un mode sommeil en chaîne compréhensible.
 * @param  mode      Valeur du mode (0, 1 ou 2).
 * @param  out       Buffer de sortie pour la chaîne compréhensible.
 * @param  out_size  Taille du buffer de sortie.
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_INVALID_PARAM sinon.
 */
ESP01_Status_t esp01_sleep_mode_to_string(int mode, char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Validation des paramètres

    const char *desc = "Inconnu"; // Description par défaut
    switch (mode)
    {
    case 0:
        desc = "Pas de sommeil (no sleep, 0)";
        break;
    case 1:
        desc = "Sommeil léger (light sleep, 1)";
        break;
    case 2:
        desc = "Sommeil modem (modem sleep, 2)";
        break;
    }
    snprintf(out, out_size, "%s", desc);                    // Copie la description dans le buffer
    _esp_login(">>> [DEBUG][SLEEP] Description : %s", out); // Log la description
    return ESP01_OK;
}

// ==================== PUISSANCE RF ====================

/**
 * @brief  Récupère la puissance RF actuelle du module ESP01.
 * @param  dbm  Pointeur vers un int où sera stockée la puissance en dBm.
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_get_rf_power(int *dbm)
{
    VALIDATE_PARAM(dbm, ESP01_INVALID_PARAM); // Validation du paramètre

    char resp[ESP01_MAX_RESP_BUF];                                                                                // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+RFPOWER?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+RFPOWER?
    _esp_login(">>> [DEBUG][RFPOWER] Réponse : %s", resp);                                                        // Log la réponse

    if (st != ESP01_OK)
    {
        _esp_login(">>> [RFPOWER] Erreur : %s", esp01_get_error_string(st)); // Log erreur si la commande échoue
        return ESP01_FAIL;
    }

    int32_t dbm_tmp = 0;                                               // Variable temporaire pour le parsing
    if (esp01_parse_int_after(resp, "+RFPOWER", &dbm_tmp) == ESP01_OK) // Extraction de la puissance
    {
        *dbm = (int)dbm_tmp;                                                  // Stocke le résultat dans le paramètre utilisateur
        _esp_login(">>> [DEBUG][RFPOWER] Puissance RF brute : %d dBm", *dbm); // Log de la puissance brute
        return ESP01_OK;
    }
    esp01_log_pattern_not_found("RFPOWER", "+RFPOWER"); // Log harmonisé si motif non trouvé
    return ESP01_FAIL;
}

/**
 * @brief  Définit la puissance RF du module ESP01.
 * @param  dbm  Puissance à appliquer (0 à 82 dBm).
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_set_rf_power(int dbm)
{
    VALIDATE_PARAM(dbm >= 0 && dbm <= 82, ESP01_INVALID_PARAM); // Validation du paramètre

    char cmd[24];                                     // Buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+RFPOWER=%d", dbm); // Formate la commande

    char resp[ESP01_SMALL_BUF_SIZE * 4] = {0};                                                          // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attends "OK"
    _esp_login(">>> [DEBUG][RFPOWER] Réponse : %s", resp);                                              // Log la réponse
    if (st != ESP01_OK)
    {
        _esp_login(">>> [RFPOWER] Erreur : %s", esp01_get_error_string(st)); // Log erreur si la commande échoue
    }
    return st; // Retourne le statut
}

// ==================== LOG SYSTEME ====================

/**
 * @brief  Récupère le niveau de log système du module ESP01.
 * @param  level  Pointeur vers un int où sera stocké le niveau (0 à 4).
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_get_syslog(int *level)
{
    VALIDATE_PARAM(level, ESP01_INVALID_PARAM); // Validation du paramètre

    char resp[ESP01_MAX_RESP_BUF];                                                                               // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSLOG?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+SYSLOG?
    _esp_login(">>> [DEBUG][SYSLOG] Réponse : %s", resp);                                                        // Log la réponse

    if (st != ESP01_OK)
    {
        _esp_login(">>> [SYSLOG] Erreur : %s", esp01_get_error_string(st)); // Log erreur si la commande échoue
        return ESP01_FAIL;
    }

    int32_t level_tmp = 0;                                              // Variable temporaire pour le parsing
    if (esp01_parse_int_after(resp, "+SYSLOG", &level_tmp) == ESP01_OK) // Extraction du niveau de log
    {
        *level = (int)level_tmp;                                        // Stocke le résultat dans le paramètre utilisateur
        _esp_login(">>> [DEBUG][SYSLOG] Niveau log brut : %d", *level); // Log du niveau brut
        switch (*level)
        {
        case 0:
            _esp_login(">>> [SYSLOG] Niveau log compréhensible : désactivé (0)");
            break;
        case 1:
            _esp_login(">>> [SYSLOG] Niveau log compréhensible : erreur (1)");
            break;
        case 2:
            _esp_login(">>> [SYSLOG] Niveau log compréhensible : WARNING (2)");
            break;
        case 3:
            _esp_login(">>> [SYSLOG] Niveau log compréhensible : info (3)");
            break;
        case 4:
            _esp_login(">>> [SYSLOG] Niveau log compréhensible : debug (4)");
            break;
        default:
            _esp_login(">>> [SYSLOG] Niveau log compréhensible : inconnu");
            break;
        }
        return ESP01_OK;
    }
    esp01_log_pattern_not_found("SYSLOG", "+SYSLOG"); // Log harmonisé si motif non trouvé
    return ESP01_FAIL;
}

/**
 * @brief  Définit le niveau de log système du module ESP01.
 * @param  level  Niveau à appliquer (0 à 4).
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_set_syslog(int level)
{
    VALIDATE_PARAM(level >= 0 && level <= 4, ESP01_INVALID_PARAM); // Validation du paramètre

    char cmd[16], resp[64];                            // Buffers pour la commande et la réponse
    snprintf(cmd, sizeof(cmd), "AT+SYSLOG=%d", level); // Formate la commande AT

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp),
                                                   "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"
    _esp_login(">>> [DEBUG][SYSLOG] Réponse : %s", resp);                      // Log la réponse
    if (st != ESP01_OK)
    {
        _esp_login(">>> [SYSLOG] Erreur : %s", esp01_get_error_string(st)); // Log erreur si la commande échoue
    }
    return st; // Retourne le statut
}

/**
 * @brief  Convertit un niveau de log système en chaîne compréhensible.
 * @param  syslog    Valeur du niveau (0 à 4).
 * @param  out       Buffer de sortie pour la chaîne compréhensible.
 * @param  out_size  Taille du buffer de sortie.
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_INVALID_PARAM sinon.
 */
ESP01_Status_t esp01_syslog_to_string(int syslog, char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Validation des paramètres

    const char *desc = "Inconnu"; // Description par défaut
    switch (syslog)
    {
    case 0:
        desc = "Désactivé (0)";
        break;
    case 1:
        desc = "Erreur (1)";
        break;
    case 2:
        desc = "WARNING (2)";
        break;
    case 3:
        desc = "Info (3)";
        break;
    case 4:
        desc = "Debug (4)";
        break;
    }
    snprintf(out, out_size, "%s", desc);                     // Copie la description dans le buffer
    _esp_login(">>> [DEBUG][SYSLOG] Description : %s", out); // Log la description
    return ESP01_OK;
}

// ==================== MÉMOIRE ====================

/**
 * @brief  Récupère la quantité de RAM libre sur le module ESP01.
 * @param  free_ram  Pointeur vers un uint32_t où sera stockée la RAM libre (en octets).
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_get_sysram(uint32_t *free_ram)
{
    VALIDATE_PARAM(free_ram, ESP01_INVALID_PARAM); // Validation du paramètre

    char resp[ESP01_MAX_RESP_BUF];                                                                               // Buffer pour la réponse AT
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSRAM?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+SYSRAM?
    _esp_login(">>> [DEBUG][SYSRAM] Réponse : %s", resp);                                                        // Log la réponse

    if (st != ESP01_OK)
    {
        _esp_login(">>> [SYSRAM] Erreur : %s", esp01_get_error_string(st)); // Log erreur si la commande échoue
        return ESP01_FAIL;
    }

    int32_t ram_tmp = 0;                                              // Variable temporaire pour le parsing
    if (esp01_parse_int_after(resp, "+SYSRAM", &ram_tmp) == ESP01_OK) // Extraction de la RAM libre
    {
        *free_ram = (uint32_t)ram_tmp;                                             // Stocke le résultat dans le paramètre utilisateur
        _esp_login(">>> [DEBUG][SYSRAM] RAM libre brute : %lu octets", *free_ram); // Log de la RAM brute
        return ESP01_OK;
    }
    esp01_log_pattern_not_found("SYSRAM", "+SYSRAM"); // Log harmonisé si motif non trouvé
    return ESP01_FAIL;
}

// ==================== DEEP SLEEP ====================

/**
 * @brief  Met le module ESP01 en deep sleep pour une durée donnée.
 * @param  ms  Durée du deep sleep en millisecondes.
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_deep_sleep(uint32_t ms)
{
    char cmd[32], resp[64];                                                                             // Buffers pour la commande et la réponse
    snprintf(cmd, sizeof(cmd), "AT+GSLP=%lu", ms);                                                      // Formate la commande AT
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"
    _esp_login(">>> [GSLP] Réponse : %s", resp);
    if (st != ESP01_OK)
    {
        _esp_login(">>> [GSLP] Erreur : %s", esp01_get_error_string(st));
    }
    return st;
}

// ==================== OUTILS & UTILITAIRES GÉNÉRAUX ====================

/**
 * @brief  Retourne une chaîne compréhensible correspondant à un code d'erreur ESP01_Status_t.
 * @param  status  Code de statut à convertir.
 * @retval const char*  Chaîne descriptive de l'erreur.
 */
const char *esp01_get_error_string(ESP01_Status_t status)
{
    switch (status)
    {
    case ESP01_OK:
        return "OK";
    case ESP01_FAIL:
        return "Erreur";
    case ESP01_TIMEOUT:
        return "Timeout";
    case ESP01_NOT_INITIALIZED:
        return "Non initialisé";
    case ESP01_INVALID_PARAM:
        return "Paramètre invalide";
    case ESP01_BUFFER_OVERFLOW:
        return "Débordement buffer";
    case ESP01_WIFI_NOT_CONNECTED:
        return "WiFi non connecté";
    case ESP01_HTTP_PARSE_ERROR:
        return "Erreur parsing HTTP";
    case ESP01_ROUTE_NOT_FOUND:
        return "Route HTTP non trouvée";
    case ESP01_CONNECTION_ERROR:
        return "Erreur connexion";
    case ESP01_MEMORY_ERROR:
        return "Erreur mémoire";
    case ESP01_EXIT:
        return "Sortie";
    default:
        return "Code inconnu";
    }
}

/**
 * @brief  Attend un motif précis dans le flux RX du module ESP01 pendant un timeout donné.
 * @param  pattern     Motif à attendre dans la réponse.
 * @param  timeout_ms  Timeout maximal en millisecondes.
 * @retval ESP01_Status_t  ESP01_OK si motif trouvé, ESP01_TIMEOUT sinon.
 */
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms)
{
    VALIDATE_PARAM(pattern, ESP01_INVALID_PARAM); // Validation du paramètre

    char resp[ESP01_MAX_RESP_BUF] = {0};
    uint32_t start = HAL_GetTick();
    size_t resp_len = 0;

    while ((HAL_GetTick() - start) < timeout_ms && resp_len < sizeof(resp) - 1)
    {
        uint8_t buf[32];
        int len = esp01_get_new_data(buf, sizeof(buf));
        if (len > 0)
        {
            if (resp_len + len >= sizeof(resp) - 1)
                len = sizeof(resp) - 1 - resp_len;
            memcpy(resp + resp_len, buf, len);
            resp_len += len;
            resp[resp_len] = '\0';

            _esp_login(">>> [DEBUG][WAIT] Flux reçu : '%s'", resp);

            if (pattern && strstr(resp, pattern))
            {
                _esp_login(">>> [DEBUG][WAIT] Pattern '%s' trouvé", pattern);
                return ESP01_OK;
            }
        }
        else
        {
            HAL_Delay(1);
        }
    }
    _esp_login(">>> [DEBUG][WAIT] Pattern '%s' NON trouvé", pattern);
    return (pattern && strstr(resp, pattern)) ? ESP01_OK : ESP01_TIMEOUT;
}

/**
 * @brief  Récupère la liste complète des commandes AT supportées par l'ESP01.
 * @param  out      Buffer de sortie pour la liste.
 * @param  out_size Taille du buffer de sortie (doit être grand, ex: 8192).
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_TIMEOUT sinon.
 */
ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size >= 8192, ESP01_INVALID_PARAM); // Validation des paramètres

    size_t total_len = 0;
    uint32_t start = HAL_GetTick();
    int found_ok = 0;
    char line[256];
    size_t line_len = 0;

    _flush_rx_buffer(100);                                                     // Vide le RX avant d'envoyer
    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+CMD?\r\n", 9, HAL_MAX_DELAY); // Demande la liste

    while ((HAL_GetTick() - start) < 30000 && total_len < out_size - 1)
    {
        uint8_t buf[64];
        int len = esp01_get_new_data(buf, sizeof(buf));
        for (int i = 0; i < len; i++)
        {
            char c = buf[i];
            if (line_len < sizeof(line) - 1)
                line[line_len++] = c;

            // Fin de ligne détectée
            if (c == '\n')
            {
                line[line_len] = '\0';
                // Ajoute la ligne au buffer global
                size_t copy_len = (total_len + line_len < out_size - 1) ? line_len : (out_size - 1 - total_len);
                memcpy(out + total_len, line, copy_len);
                total_len += copy_len;
                out[total_len] = '\0';

                // Vérifie si c'est la ligne "OK"
                if (strstr(line, "OK"))
                {
                    found_ok = 1;
                    break;
                }
                line_len = 0; // Réinitialise la ligne
            }
        }
        if (found_ok)
            break;
        if (len == 0)
            HAL_Delay(1);
    }

    if (found_ok)
    {
        _esp_login(">>> [DEBUG][CMD] Liste complète reçue (%lu octets)", (unsigned long)total_len);
        return ESP01_OK;
    }
    else
    {
        _esp_login(">>> [DEBUG][CMD] Timeout ou buffer plein (%lu octets)", (unsigned long)total_len);
        return ESP01_TIMEOUT;
    }
}

/**
 * @brief  Vérifie si la taille d'un buffer est suffisante.
 * @param  needed     Taille nécessaire.
 * @param  available  Taille disponible.
 * @retval ESP01_Status_t  ESP01_OK si suffisant, ESP01_BUFFER_OVERFLOW sinon.
 */
ESP01_Status_t esp01_check_buffer_size(size_t needed, size_t available)
{
    return (needed > available) ? ESP01_BUFFER_OVERFLOW : ESP01_OK;
}

// ==================== TERMINAL AT INTERACTIF (CONSOLE SERIE) ====================

/**
 * @brief  Envoie la commande AT saisie dans la console et récupère la réponse.
 * @param  out_buf      Buffer de sortie pour la réponse.
 * @param  out_buf_size Taille du buffer de sortie.
 * @retval ESP01_Status_t  ESP01_OK si succès, ESP01_FAIL sinon.
 *
 * Cette fonction gère le timeout adapté selon la commande (certaines commandes AT sont longues).
 */
static ESP01_Status_t esp01_interactive_at_console(char *out_buf, size_t out_buf_size)
{
    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer

    if (!esp_console_cmd_ready || esp_console_cmd_idx == 0) // Vérifie qu'une commande est prête
        return ESP01_FAIL;

    uint32_t timeout = ESP01_TIMEOUT_SHORT; // Timeout par défaut

    // Liste des commandes nécessitant un timeout long
    const char *cmds_long[] = {
        "AT+CWJAP", "AT+CWLAP", "AT+CIPSTART", "AT+CIPSEND", "AT+MQTTCONN", "AT+HTTPCLIENT", "AT+RESTORE", "AT+UPDATE", "AT+CMD?", NULL};

    // Si la commande tapée commence par une des commandes longues, on met un timeout long
    for (int i = 0; cmds_long[i]; ++i)
    {
        if (strncmp((char *)esp_console_cmd_buf, cmds_long[i], strlen(cmds_long[i])) == 0)
        {
            timeout = ESP01_TIMEOUT_LONG;
            break;
        }
    }

    ESP01_Status_t status = esp01_send_raw_command_dma(
        (char *)esp_console_cmd_buf,
        out_buf,
        out_buf_size,
        "OK",
        timeout);

    return status;
}

/**
 * @brief  Callback de réception pour la console série (UART debug).
 * @param  huart  Pointeur sur la structure UART.
 */
void esp01_console_rx_callback(UART_HandleTypeDef *huart)
{
    if (huart == g_debug_uart)
    {
        char c = esp_console_rx_char; // Récupère le caractère reçu
        if (!esp_console_cmd_ready && esp_console_cmd_idx < ESP01_MAX_CMD_BUF - 1)
        {
            if (c == '\r' || c == '\n')
            {
                esp_console_cmd_buf[esp_console_cmd_idx] = '\0'; // Termine la commande
                esp_console_cmd_ready = 1;                       // Indique que la commande est prête
            }
            else if (c >= 32 && c <= 126)
            {
                esp_console_cmd_buf[esp_console_cmd_idx++] = c; // Ajoute le caractère au buffer
            }
        }
        HAL_UART_Receive_IT(g_debug_uart, (uint8_t *)&esp_console_rx_char, 1); // Relance la réception IT
    }
}

/**
 * @brief  Tâche de gestion du terminal AT interactif.
 *
 * Affiche le prompt, traite la commande saisie, affiche la réponse,
 * et gère l'attente lors d'un reset ou restore.
 */
void esp01_console_task(void)
{
    static uint8_t prompt_affiche = 0; // Indique si le prompt a été affiché

    // Affiche le prompt si aucune commande est prête à être envoyée
    if (!prompt_affiche && esp_console_cmd_ready == 0)
    {
        printf("\r\n[ESP01] === Entrez une commande AT : ");
        fflush(stdout);
        prompt_affiche = 1;
    }

    // Si une commande AT a été saisie, l'envoie à l'ESP01 et affiche la réponse
    if (esp_console_cmd_ready)
    {
        char reponse[8196];
        esp01_interactive_at_console(reponse, sizeof(reponse)); // Envoie la commande et récupère la réponse
        printf("[ESP01] >>> %s", reponse);

        // Si la commande était un reset ou un restore, attendre le reboot du module
        if (
            strstr((char *)esp_console_cmd_buf, "AT+RST") ||
            strstr((char *)esp_console_cmd_buf, "AT+RESTORE"))
        {
            printf("\r\n[ESP01] >>> Attente du redémarrage du module...\r\n");
            char boot_msg[256] = {0};
            uint32_t start = HAL_GetTick();
            while ((HAL_GetTick() - start) < 8000) // Timeout 8 secondes
            {
                int len = esp01_get_new_data((uint8_t *)boot_msg, sizeof(boot_msg) - 1);
                if (len > 0)
                {
                    boot_msg[len] = '\0';
                    if (strstr(boot_msg, "ready"))
                    {
                        printf("[ESP01] >>> Module prêt !\r\n");
                        break;
                    }
                }
                HAL_Delay(10);
            }
        }

        esp_console_cmd_ready = 0; // Réinitialise l'état de la commande
        esp_console_cmd_idx = 0;
        prompt_affiche = 0;
    }
}

/**
 * @brief  Initialise la réception IT pour la console série.
 * @param  huart_debug  Pointeur sur l’UART de debug.
 */
void esp01_terminal_begin(UART_HandleTypeDef *huart_debug)
{
    if (!huart_debug)
        return;                                                           // Validation du paramètre pour fonction void
    HAL_UART_Receive_IT(huart_debug, (uint8_t *)&esp_console_rx_char, 1); // Démarre la réception IT
}

// ==================== OUTILS DE PARSING ====================

/**
 * @brief  Analyse une réponse pour extraire un entier 32 bits après un motif donné.
 * @param  resp     Réponse à analyser.
 * @param  pattern  Motif à chercher dans la réponse.
 * @param  out      Pointeur vers l'entier où stocker le résultat.
 * @retval ESP01_Status_t  ESP01_OK si réussi, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_parse_int_after(const char *resp, const char *pattern, int32_t *out)
{
    VALIDATE_PARAM(resp && pattern && out, ESP01_INVALID_PARAM); // Validation des paramètres
    char *ptr = strstr(resp, pattern);                           // Cherche le motif dans la réponse
    if (!ptr)
        return ESP01_FAIL;
    ptr = strchr(ptr, ':'); // Cherche le caractère ':'
    if (!ptr)
        return ESP01_FAIL;
    *out = (int32_t)strtol(ptr + 1, NULL, 10); // Convertit la valeur trouvée en int32_t
    return ESP01_OK;
}

/**
 * @brief  Extrait une chaîne après un motif et jusqu'à \r, \n ou la fin.
 * @param  resp     Chaîne de réponse à analyser.
 * @param  pattern  Motif à chercher (ex: "+UART", "+UART_CUR").
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_Status_t  ESP01_OK si trouvé, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_parse_string_after(const char *resp, const char *pattern, char *out, size_t out_size)
{
    VALIDATE_PARAM(resp && pattern && out && out_size > 0, ESP01_INVALID_PARAM); // Validation des paramètres
    char *ptr = strstr(resp, pattern);                                           // Cherche le motif dans la réponse
    if (!ptr)
        return ESP01_FAIL;
    ptr = strchr(ptr, ':'); // Cherche le caractère ':'
    if (!ptr)
        return ESP01_FAIL;
    ptr++; // Passe le ':'
    size_t len = 0;
    while (ptr[len] && ptr[len] != '\r' && ptr[len] != '\n' && len < out_size - 1)
        len++;
    strncpy(out, ptr, len); // Copie la chaîne extraite dans le buffer de sortie
    out[len] = '\0';        // Ajoute le zéro terminal
    return ESP01_OK;
}

/**
 * @brief  Log d'erreur harmonisé pour motif non trouvé lors du parsing.
 * @param  context  Contexte ou nom de la fonction (ex: "UART", "SLEEP").
 * @param  pattern  Motif recherché (ex: "+UART", "+SLEEP").
 */
void esp01_log_pattern_not_found(const char *context, const char *pattern)
{
    if (pattern)
        _esp_login(">>> [%s] Erreur : motif '%s' non trouvé", context, pattern);
    else
        _esp_login(">>> [%s] Erreur : motif non trouvé", context);
}
