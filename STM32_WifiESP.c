/**
 ******************************************************************************
 * @file    STM32_WifiESP.c
 * @author  manu
 * @version 1.2.0
 * @date    13 juin 2025
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
 *
 ******************************************************************************
 */

#include "STM32_WifiESP.h" // Header du driver ESP01
#include <stdlib.h>        // Pour malloc, free
#include <stdarg.h>        // Pour va_list, va_start, va_end
#include <string.h>        // Pour memcpy, strlen, strstr
#include <stdio.h>         // Pour snprintf, vsnprintf

// ==================== VARIABLES GLOBALES ====================

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
volatile uint16_t esp_console_cmd_idx = 0;                  // Index courant dans le buffer de commande AT
volatile uint8_t esp_console_cmd_ready = 0;                 // Indique si une commande AT est prête à être traitée

// ==================== LOGS & DEBUG ====================

/**
 * @brief  Log une information de debug sur l'UART de debug.
 * @param  fmt Format de la chaîne de log.
 * @param  ... Arguments formatés.
 */
void _esp_login(const char *fmt, ...)
{
#if ESP01_DEBUG
    VALIDATE_PARAM_VOID(fmt); // Correction : macro dédiée pour void
    if (g_debug_uart)
    {
        char buf[ESP01_MAX_CMD_BUF];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)buf, strlen(buf), HAL_MAX_DELAY);
        const char crlf[] = "\r\n";
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)crlf, 2, HAL_MAX_DELAY);
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
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
    VALIDATE_PARAM(huart_esp && huart_debug && dma_rx_buf && dma_buf_size > 0, ESP01_INVALID_PARAM);

    g_esp_uart = huart_esp;        // Initialise l'UART pour l'ESP01
    g_debug_uart = huart_debug;    // Initialise l'UART pour le debug/console
    g_dma_rx_buf = dma_rx_buf;     // Buffer DMA pour la réception UART
    g_dma_buf_size = dma_buf_size; // Taille du buffer DMA RX
    g_server_port = 80;            // Port par défaut du serveur HTTP

    if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK) // Initialise la réception DMA pour l'ESP01
    {
        ESP01_LOG_ERROR("INIT", "Erreur initialisation DMA RX : %s", esp01_get_error_string(ESP01_FAIL));
        ESP01_RETURN_ERROR("INIT", ESP01_NOT_INITIALIZED); // Utilisation de RETURN_ERROR au lieu de return
    }
    HAL_Delay(250); // Petit délai pour laisser l'ESP01 démarrer

    ESP01_Status_t status = esp01_test_at(); // Teste la communication avec l'ESP01 via la commande AT
    if (status != ESP01_OK)                  // Si le test AT échoue
    {
        ESP01_LOG_ERROR("INIT", "ESP01 non détecté !"); // Log l'erreur
        ESP01_RETURN_ERROR("INIT", ESP01_NOT_DETECTED); // Utilisation de RETURN_ERROR au lieu de return
    }
    return ESP01_OK; // Retourne OK si l'initialisation réussit
}

/**
 * @brief  Teste la communication avec l'ESP01 via la commande AT.
 * @retval ESP01_Status_t Code de statut (OK si le module répond).
 */
ESP01_Status_t esp01_test_at(void)
{
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Buffer pour la réponse complète
    return st;                                                                                           // Retourne le statut (OK, TIMEOUT, FAIL)
}

/**
 * @brief  Réinitialise le module ESP01 (AT+RST).
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_reset(void)
{
    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer la commande

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+RST\r\n", 8, HAL_MAX_DELAY); // Envoie la commande AT+RST pour reset

    uint32_t start = HAL_GetTick();      // Timestamp de départ pour le timeout
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse complète
    size_t resp_len = 0;                 // Longueur de la réponse reçue

    ESP01_LOG_INFO("ESP01", "=== Envoi de la commande AT+RST");
    ESP01_LOG_INFO("ESP01", ">>> AT+RST\n");
    ESP01_LOG_INFO("ESP01", ">>> Attente du redémarrage du module...");

    // Lecture de la réponse complète pendant 3 secondes max
    while ((HAL_GetTick() - start) < 3000 && resp_len < sizeof(resp) - 1) // Boucle jusqu'à timeout ou buffer plein
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];              // Buffer temporaire pour lecture
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)                                    // Si des octets ont été reçus
        {
            if (resp_len + len >= sizeof(resp) - 1) // Empêche le dépassement de buffer
                len = sizeof(resp) - 1 - resp_len;  // Ajuste la taille à copier
            memcpy(resp + resp_len, buf, len);      // Ajoute au buffer de réponse
            resp_len += len;                        // Met à jour la longueur totale
            resp[resp_len] = '\0';                  // Termine la chaîne
        }
        else // Si rien reçu
        {
            HAL_Delay(1); // Petite pause CPU
        }
    }
    ESP01_LOG_DEBUG("RESET", "Réponse complète : %s", resp); // Log la réponse complète reçue

    HAL_Delay(1000); // Attend un peu le redémarrage du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Teste la communication AT après le reset

    if (st == ESP01_OK) // Si le test AT est OK
    {
        ESP01_LOG_INFO("RESET", "AT OK après reset, reset réussi"); // Log succès reset
        return ESP01_OK;                                            // Retourne OK
    }
    else
    {
        ESP01_LOG_ERROR("RESET", "AT échoué après reset : %s", esp01_get_error_string(st)); // Log échec reset
        return ESP01_FAIL;                                                                  // Retourne FAIL
    }
}

/**
 * @brief  Restaure les paramètres d'usine du module ESP01 (AT+RESTORE).
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_restore(void)
{
    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer la commande

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+RESTORE\r\n", 12, HAL_MAX_DELAY); // Envoie la commande AT+RESTORE

    uint32_t start = HAL_GetTick();      // Timestamp de départ pour le timeout
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse complète
    size_t resp_len = 0;                 // Longueur de la réponse reçue

    ESP01_LOG_INFO("ESP01", "=== Envoi de la commande AT+RESTORE");
    ESP01_LOG_INFO("ESP01", ">>> AT+RESTORE\n");
    ESP01_LOG_INFO("ESP01", ">>> Attente du redémarrage du module...");

    // Lecture de la réponse complète pendant 3 secondes max
    while ((HAL_GetTick() - start) < 3000 && resp_len < sizeof(resp) - 1) // Boucle jusqu'à timeout ou buffer plein
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];              // Buffer temporaire pour lecture
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)                                    // Si des octets ont été reçus
        {
            if (resp_len + len >= sizeof(resp) - 1) // Empêche le dépassement de buffer
                len = sizeof(resp) - 1 - resp_len;  // Ajuste la taille à copier
            memcpy(resp + resp_len, buf, len);      // Ajoute au buffer de réponse
            resp_len += len;                        // Met à jour la longueur totale
            resp[resp_len] = '\0';                  // Termine la chaîne
        }
        else // Si rien reçu
        {
            HAL_Delay(1); // Petite pause CPU
        }
    }
    ESP01_LOG_DEBUG("RESTORE", "Réponse complète : %s", resp); // Log la réponse complète reçue

    HAL_Delay(1000); // Attend un peu le redémarrage du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Teste la communication AT après le restore

    if (st == ESP01_OK) // Si le test AT est OK
    {
        ESP01_LOG_INFO("RESTORE", "AT OK après restore, restore réussi"); // Log succès restore
        return ESP01_OK;                                                  // Retourne OK
    }
    else
    {
        ESP01_LOG_ERROR("RESTORE", "AT échoué après restore : %s", esp01_get_error_string(st)); // Log échec restore
        return ESP01_FAIL;                                                                      // Retourne FAIL
    }
}

// ==================== UTILITAIRES DMA/RX & BUFFER ====================

/**
 * @brief  Vide le buffer de réception UART/DMA.
 * @param  timeout_ms Durée maximale d'attente en ms.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();              // Sauvegarde le temps de départ
    while ((HAL_GetTick() - start) < timeout_ms) // Boucle jusqu'à expiration du timeout
    {
        uint8_t dummy[ESP01_SMALL_BUF_SIZE];                // Buffer temporaire pour lecture
        int len = esp01_get_new_data(dummy, sizeof(dummy)); // Lit les nouvelles données reçues
        if (len == 0)                                       // Si rien à lire, attend un peu
            HAL_Delay(1);                                   // Petite pause CPU
    }
    ESP01_LOG_DEBUG("FLUSH", "Buffer UART/DMA vidé"); // Log de fin de flush
    return ESP01_OK;                                  // Retourne OK
}

/**
 * @brief  Récupère les nouvelles données reçues via DMA.
 * @param  buf     Buffer de destination.
 * @param  bufsize Taille du buffer.
 * @retval Nombre d'octets lus.
 */
int esp01_get_new_data(uint8_t *buf, uint16_t bufsize)
{
    VALIDATE_PARAM(buf && bufsize > 0, 0); // Ajout de la validation des paramètres
    if (!g_dma_rx_buf)
        return 0;

    uint16_t pos = g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx);
    int len = 0;

    if (pos != g_rx_last_pos)
    {
        if (pos > g_rx_last_pos)
        {
            len = pos - g_rx_last_pos;
            memcpy(buf, &g_dma_rx_buf[g_rx_last_pos], len);
        }
        else
        {
            len = g_dma_buf_size - g_rx_last_pos;
            memcpy(buf, &g_dma_rx_buf[g_rx_last_pos], len);
            if (pos > 0)
            {
                memcpy(buf + len, &g_dma_rx_buf[0], pos);
                len += pos;
            }
        }
        g_rx_last_pos = pos;
    }
    return len;
}

/**
 * @brief  Vide le buffer RX (utilitaire interne, non DMA).
 * @param  timeout_ms  Durée maximale (en ms) pendant laquelle on vide le buffer.
 */
void _flush_rx_buffer(uint32_t timeout_ms)
{
    // Pas de paramètre à valider ici (timeout_ms peut être 0)
    uint32_t start = HAL_GetTick();
    uint8_t dummy[ESP01_SMALL_BUF_SIZE];
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        int len = esp01_get_new_data(dummy, sizeof(dummy));
        if (len == 0)
            HAL_Delay(1);
    }
    ESP01_LOG_DEBUG("FLUSH", "Buffer RX vidé (utilitaire)");
}

// ==================== VERSION & INFOS ====================

/**
 * @brief  Récupère la version du firmware AT de l'ESP01
 * @param  version_buf Buffer pour stocker la réponse complète de AT+GMR
 * @param  buf_size    Taille du buffer
 * @retval ESP01_Status_t ESP01_OK en cas de succès, code d'erreur sinon
 * @note   Cette fonction retourne la réponse brute de AT+GMR qui contient
 *         généralement plusieurs lignes d'information sur le firmware
 */
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size)
{
    VALIDATE_PARAM(version_buf && buf_size > 0, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+GMR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("GMR", "Erreur récupération version : %s", esp01_get_error_string(st));
        esp01_safe_strcpy(version_buf, buf_size, "Erreur récupération version");
        return st;
    }

    // Copier la réponse brute AT+GMR directement
    esp01_safe_strcpy(version_buf, buf_size, resp);

    ESP01_LOG_DEBUG("GMR", "Version AT récupérée (%d octets)", (int)strlen(version_buf));
    return ESP01_OK;
}

/**
 * @brief  Extrait et affiche les lignes d'informations du firmware ESP01
 * @param  gmr_resp Réponse brute de la commande AT+GMR
 * @return Nombre de lignes extraites
 */
uint8_t esp01_display_firmware_info(const char *gmr_resp)
{
    if (!gmr_resp || !*gmr_resp)
        return 0;

    // Trouver le début des informations (après "AT+GMR")
    const char *start = strstr(gmr_resp, "AT+GMR");
    if (start)
        start += 6; // Passer "AT+GMR"
    else
        start = gmr_resp; // Si "AT+GMR" n'est pas trouvé, utiliser le début

    // Ignorer les lignes vides au début
    while (*start && (*start == '\r' || *start == '\n'))
        start++;

    // Compter le nombre de lignes et les afficher
    uint8_t line_count = 0;
    char line_buf[128];
    const char *line_start = start;

    while (*line_start)
    {
        const char *line_end = strstr(line_start, "\r\n");
        if (!line_end)
            break;

        size_t line_len = line_end - line_start;
        if (line_len > 0 && line_len < sizeof(line_buf) - 1)
        {
            memcpy(line_buf, line_start, line_len);
            line_buf[line_len] = '\0';

            // Afficher seulement si la ligne n'est pas vide et ne contient pas que des espaces
            bool empty = true;
            for (size_t i = 0; i < line_len; i++)
            {
                if (!isspace((unsigned char)line_buf[i]))
                {
                    empty = false;
                    break;
                }
            }

            if (!empty)
            {
                ESP01_LOG_INFO("GMR", "%s", line_buf);
                line_count++;
            }
        }

        line_start = line_end + 2; // Passer au-delà de \r\n
    }

    return line_count;
}

/**
 * @brief  Configure l’UART du module ESP01.
 * @param  baud      Baudrate.
 * @param  databits  Nombre de bits de données.
 * @param  stopbits  Nombre de bits de stop.
 * @param  parity    Parité (0: aucune, 1: impair, 2: pair).
 * @param  flowctrl  Contrôle de flux.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_set_uart_config(uint32_t baud, uint8_t databits, uint8_t stopbits, uint8_t parity, uint8_t flowctrl)
{
    VALIDATE_PARAM(databits >= 5 && databits <= 8 && stopbits >= 1 && stopbits <= 2 && parity <= 2 && flowctrl <= 3, ESP01_INVALID_PARAM);

    char cmd[ESP01_MAX_CMD_BUF];
    snprintf(cmd, sizeof(cmd), "AT+UART=%lu,%u,%u,%u,%u", baud, databits, stopbits, parity, flowctrl);

    char resp[ESP01_SMALL_BUF_SIZE * 4] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    ESP01_LOG_DEBUG("UART", "Réponse : %s", resp);
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("UART_SET", st);
    return ESP01_OK;
}

/**
 * @brief  Récupère la configuration UART actuelle du module ESP01.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_get_uart_config(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+UART?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("UART", st);

    // Utilisation du helper de parsing
    if (esp01_parse_string_after(resp, "+UART", out, out_size) == ESP01_OK ||
        esp01_parse_string_after(resp, "+UART_CUR", out, out_size) == ESP01_OK)
    {
        ESP01_LOG_DEBUG("UART", "Config brute : %s", out);
        return ESP01_OK;
    }
    ESP01_RETURN_ERROR("UART", ESP01_FAIL);
}

/**
 * @brief  Convertit une configuration UART brute en chaîne compréhensible.
 * @param  raw_config  Chaîne brute (ex: "115200,8,1,0,0").
 * @param  out        Buffer de sortie pour la chaîne compréhensible.
 * @param  out_size   Taille du buffer de sortie.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size)
{
    VALIDATE_PARAM(raw_config && out && out_size > 0, ESP01_INVALID_PARAM);

    uint32_t baud = 0;
    int data = 0, stop = 0, parity = 0, flow = 0;
    if (sscanf(raw_config, "%lu,%d,%d,%d,%d", &baud, &data, &stop, &parity, &flow) != 5)
        ESP01_RETURN_ERROR("UART_PARSE", ESP01_FAIL);

    const char *parity_str = "aucune";
    if (parity == 1)
        parity_str = "impair";
    else if (parity == 2)
        parity_str = "pair";

    const char *flow_str = "aucun";
    if (flow == 1)
        flow_str = "RTS";
    else if (flow == 2)
        flow_str = "CTS";
    else if (flow == 3)
        flow_str = "RTS+CTS";

    snprintf(out, out_size, "baudrate=%lu, data bits=%d, stop bits=%d, parité=%s, flow control=%s",
             baud, data, stop, parity_str, flow_str);

    ESP01_LOG_DEBUG("UART", "Config compréhensible : %s", out);
    return ESP01_OK;
}

/**
 * @brief  Récupère le mode sommeil actuel du module ESP01.
 * @param  mode Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_get_sleep_mode(int *mode)
{
    VALIDATE_PARAM(mode, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SLEEP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SLEEP", st);

    int32_t mode_tmp = 0;
    if (esp01_parse_int_after(resp, "+SLEEP", &mode_tmp) == ESP01_OK)
    {
        *mode = (int)mode_tmp;
        ESP01_LOG_DEBUG("SLEEP", "Mode sommeil brut : %d", *mode);
        return ESP01_OK;
    }
    ESP01_RETURN_ERROR("SLEEP", ESP01_FAIL);
}

/**
 * @brief  Définit le mode sommeil du module ESP01.
 * @param  mode Valeur du mode sommeil.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_set_sleep_mode(int mode)
{
    VALIDATE_PARAM(mode >= 0 && mode <= 2, ESP01_INVALID_PARAM);

    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+SLEEP=%d", mode);
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    ESP01_LOG_DEBUG("SLEEP", "Réponse : %s", resp);
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SLEEP_SET", st);
    return ESP01_OK;
}

/**
 * @brief  Récupère la puissance RF actuelle du module ESP01.
 * @param  dbm Pointeur vers la variable de sortie (dBm).
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_get_rf_power(int *dbm)
{
    VALIDATE_PARAM(dbm, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+RFPOWER?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("RFPOWER", st);

    int32_t dbm_tmp = 0;
    if (esp01_parse_int_after(resp, "+RFPOWER", &dbm_tmp) == ESP01_OK)
    {
        *dbm = (int)dbm_tmp;
        ESP01_LOG_DEBUG("RFPOWER", "Puissance RF : %d dBm", *dbm);
        return ESP01_OK;
    }
    ESP01_RETURN_ERROR("RFPOWER", ESP01_FAIL);
}

/**
 * @brief  Définit la puissance RF du module ESP01.
 * @param  dbm Puissance en dBm.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_set_rf_power(int dbm)
{
    VALIDATE_PARAM(dbm >= 0 && dbm <= 82, ESP01_INVALID_PARAM);

    char cmd[24];
    snprintf(cmd, sizeof(cmd), "AT+RFPOWER=%d", dbm);

    char resp[ESP01_SMALL_BUF_SIZE * 4] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("RFPOWER_SET", st);
    return ESP01_OK;
}

/**
 * @brief  Récupère le niveau de log système du module ESP01.
 * @param  level Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_get_syslog(int *level)
{
    VALIDATE_PARAM(level, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSLOG?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SYSLOG", st);

    int32_t level_tmp = 0;
    if (esp01_parse_int_after(resp, "+SYSLOG", &level_tmp) == ESP01_OK)
    {
        *level = (int)level_tmp;
        ESP01_LOG_DEBUG("SYSLOG", "Niveau log : %d", *level);

        return ESP01_OK;
    }
    ESP01_RETURN_ERROR("SYSLOG", ESP01_FAIL);
}

/**
 * @brief  Définit le niveau de log système du module ESP01.
 * @param  level Niveau de log.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_set_syslog(int level)
{
    VALIDATE_PARAM(level >= 0 && level <= 4, ESP01_INVALID_PARAM);

    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+SYSLOG=%d", level);

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SYSLOG_SET", st);
    return ESP01_OK;
}

/**
 * @brief  Récupère la quantité de RAM libre sur le module ESP01.
 * @param  free_ram Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_get_sysram(uint32_t *free_ram)
{
    VALIDATE_PARAM(free_ram, ESP01_INVALID_PARAM);

    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSRAM?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SYSRAM", st);

    int32_t ram_tmp = 0;
    if (esp01_parse_int_after(resp, "+SYSRAM", &ram_tmp) == ESP01_OK)
    {
        *free_ram = (uint32_t)ram_tmp;
        ESP01_LOG_DEBUG("SYSRAM", "RAM libre brute : %lu octets", *free_ram);
        return ESP01_OK;
    }
    ESP01_RETURN_ERROR("SYSRAM", ESP01_FAIL);
}

/**
 * @brief  Met le module ESP01 en deep sleep pour une durée donnée.
 * @param  ms Durée en millisecondes.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_deep_sleep(uint32_t ms)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+GSLP=%lu", ms);
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("GSLP", st);
    return ESP01_OK;
}

/**
 * @brief  Récupère la liste des commandes AT supportées par le module ESP01.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size >= ESP01_LARGE_RESP_BUF, ESP01_INVALID_PARAM); // Validation des paramètres

    size_t total_len = 0;           // Longueur totale reçue
    uint32_t start = HAL_GetTick(); // Timestamp de départ
    int found_ok = 0;               // Indicateur de succès
    char line[ESP01_MAX_RESP_BUF];  // Buffer pour une ligne
    size_t line_len = 0;            // Longueur de la ligne

    _flush_rx_buffer(100);                                                     // Vide le RX avant d'envoyer
    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+CMD?\r\n", 9, HAL_MAX_DELAY); // Demande la liste

    while ((HAL_GetTick() - start) < 30000 && total_len < out_size - 1) // Boucle jusqu'à timeout ou buffer plein
    {
        uint8_t buf[64];                                // Buffer temporaire
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        for (int i = 0; i < len; i++)
        {
            char c = buf[i]; // Caractère courant
            if (line_len < sizeof(line) - 1)
                line[line_len++] = c; // Ajoute au buffer ligne

            // Fin de ligne détectée
            if (c == '\n')
            {
                line[line_len] = '\0'; // Termine la ligne
                // Ajoute la ligne au buffer global
                size_t copy_len = (total_len + line_len < out_size - 1) ? line_len : (out_size - 1 - total_len);
                memcpy(out + total_len, line, copy_len); // Copie la ligne
                total_len += copy_len;                   // Met à jour la longueur totale
                out[total_len] = '\0';                   // Termine la chaîne

                // Vérifie si c'est la ligne "OK"
                if (strstr(line, "OK"))
                {
                    found_ok = 1; // Succès
                    break;
                }
                line_len = 0; // Réinitialise la ligne
            }
        }
        if (found_ok)
            break;
        if (len == 0)
            HAL_Delay(1); // Petite pause si rien reçu
    }

    if (found_ok)
    {
        ESP01_LOG_DEBUG("CMD", "Liste complète reçue (%lu octets)", (unsigned long)total_len); // Log succès
        return ESP01_OK;                                                                       // Succès
    }
    else
    {
        ESP01_LOG_DEBUG("CMD", "Timeout ou buffer plein (%lu octets)", (unsigned long)total_len); // Log timeout
        return ESP01_TIMEOUT;                                                                     // Timeout
    }
}

// ==================== UTILITAIRES & DEBUG ====================

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
    case ESP01_UNEXPECTED_RESPONSE:
        return "Réponse inattendue";
    case ESP01_NOT_DETECTED:
        return "Module non détecté";
    case ESP01_CMD_TOO_LONG:
        return "Commande trop longue";
    case ESP01_MEMORY_ERROR:
        return "Erreur mémoire";
    case ESP01_EXIT:
        return "Sortie";
    case ESP01_NOT_CONNECTED:
        return "Non connecté";
    case ESP01_ALREADY_CONNECTED:
        return "Déjà connecté";
    case ESP01_CONNECTION_ERROR:
        return "Erreur connexion";
    case ESP01_ROUTE_NOT_FOUND:
        return "Route non trouvée";
    case ESP01_PARSE_ERROR:
        return "Erreur de parsing";
    case ESP01_WIFI_NOT_CONNECTED:
        return "WiFi non connecté";
    case ESP01_WIFI_TIMEOUT:
        return "Timeout WiFi";
    case ESP01_WIFI_WRONG_PASSWORD:
        return "Mot de passe WiFi incorrect";
    case ESP01_WIFI_AP_NOT_FOUND:
        return "Point d'accès introuvable";
    case ESP01_WIFI_CONNECT_FAIL:
        return "Échec connexion WiFi";
    case ESP01_HTTP_PARSE_ERROR:
        return "Erreur parsing HTTP";
    case ESP01_HTTP_INVALID_REQUEST:
        return "Requête HTTP invalide";
    case ESP01_HTTP_TIMEOUT:
        return "Timeout HTTP";
    case ESP01_HTTP_CONNECTION_REFUSED:
        return "Connexion HTTP refusée";
    case ESP01_MQTT_NOT_CONNECTED:
        return "MQTT non connecté";
    case ESP01_MQTT_PROTOCOL_ERROR:
        return "Erreur protocole MQTT";
    case ESP01_MQTT_SUBSCRIPTION_FAILED:
        return "Échec abonnement MQTT";
    case ESP01_MQTT_PUBLISH_FAILED:
        return "Échec publication MQTT";
    case ESP01_NTP_SYNC_ERROR:
        return "Erreur synchronisation NTP";
    case ESP01_NTP_INVALID_RESPONSE:
        return "Réponse NTP invalide";
    case ESP01_NTP_SERVER_NOT_REACHABLE:
        return "Serveur NTP inaccessible";
    default:
        return "Code inconnu";
    }
}

/**
 * @brief  Attend un motif précis dans le flux UART.
 * @param  pattern    Motif à attendre.
 * @param  timeout_ms Timeout en ms.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms)
{
    VALIDATE_PARAM(pattern, ESP01_INVALID_PARAM); // Validation du paramètre

    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse
    uint32_t start = HAL_GetTick();      // Timestamp de départ
    size_t resp_len = 0;                 // Longueur de la réponse

    while ((HAL_GetTick() - start) < timeout_ms && resp_len < sizeof(resp) - 1) // Boucle jusqu'à timeout ou buffer plein
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];              // Buffer temporaire
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)
        {
            if (resp_len + len >= sizeof(resp) - 1) // Empêche le dépassement de buffer
                len = sizeof(resp) - 1 - resp_len;  // Ajuste la taille à copier
            memcpy(resp + resp_len, buf, len);      // Ajoute au buffer de réponse
            resp_len += len;                        // Met à jour la longueur totale
            resp[resp_len] = '\0';                  // Termine la chaîne

            ESP01_LOG_DEBUG("WAIT", "Flux reçu : '%s'", resp); // Log du flux reçu

            if (pattern && strstr(resp, pattern)) // Motif attendu trouvé ?
            {
                ESP01_LOG_DEBUG("WAIT", "Pattern '%s' trouvé", pattern); // Log motif trouvé
                return ESP01_OK;                                         // Succès
            }
        }
        else
        {
            HAL_Delay(1); // Petite pause CPU
        }
    }
    ESP01_LOG_DEBUG("WAIT", "Pattern '%s' NON trouvé", pattern);          // Log motif non trouvé
    return (pattern && strstr(resp, pattern)) ? ESP01_OK : ESP01_TIMEOUT; // Retourne le statut
}

/**
 * @brief  Envoie une commande AT brute et récupère la réponse.
 * @param  cmd             Commande à envoyer.
 * @param  response_buffer Buffer de réponse.
 * @param  response_buf_size Taille du buffer de réponse.
 * @param  expected        Motif attendu dans la réponse.
 * @param  timeout_ms      Timeout en ms.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer, size_t response_buf_size, const char *expected, uint32_t timeout_ms)
{
    VALIDATE_PARAM(cmd && response_buffer && response_buf_size > 0, ESP01_INVALID_PARAM); // Vérifie les paramètres

    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer

    ESP01_LOG_DEBUG("RAWCMD", "Commande envoyée : %s", cmd); // Log la commande envoyée

    if (!g_esp_uart) // Vérifie que l’UART ESP01 est initialisée
    {
        ESP01_LOG_ERROR("RAWCMD", "UART non initialisée : %s", esp01_get_error_string(ESP01_NOT_INITIALIZED)); // Log erreur si UART non initialisée
        return ESP01_NOT_INITIALIZED;                                                                          // Retourne erreur
    }

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY); // Envoie la commande AT
    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);        // Envoie CRLF

    uint32_t start = HAL_GetTick(); // Timestamp de départ
    size_t resp_len = 0;            // Longueur de la réponse reçue
    response_buffer[0] = '\0';      // Initialise le buffer de réponse

    while ((HAL_GetTick() - start) < timeout_ms && resp_len < response_buf_size - 1) // Boucle jusqu'à timeout ou buffer plein
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];              // Buffer temporaire pour lecture
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)                                    // Si des octets ont été reçus
        {
            if (resp_len + len >= response_buf_size - 1)  // Empêche le dépassement de buffer
                len = response_buf_size - 1 - resp_len;   // Ajuste la taille à copier
            memcpy(response_buffer + resp_len, buf, len); // Ajoute au buffer de réponse
            resp_len += len;                              // Met à jour la longueur totale
            response_buffer[resp_len] = '\0';             // Termine la chaîne

            if (expected && strstr(response_buffer, expected)) // Motif attendu trouvé ?
                break;                                         // Sort de la boucle
        }
        else // Si rien reçu
        {
            HAL_Delay(1); // Petite pause CPU
        }
    }
    ESP01_LOG_DEBUG("RAWCMD", "Retour de la commande : %s", response_buffer); // Log la réponse complète

    if (!(expected && strstr(response_buffer, expected))) // Si motif non trouvé
    {
        ESP01_LOG_ERROR("RAWCMD", "Timeout ou motif non trouvé : %s", esp01_get_error_string(ESP01_TIMEOUT)); // Log erreur timeout
        return ESP01_TIMEOUT;                                                                                 // Retourne timeout
    }

    return ESP01_OK; // Retourne OK si motif trouvé
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
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    const char *desc = "Inconnu"; // Valeur par défaut si mode inconnu
    switch (mode)
    {
    case 0:
        desc = "Pas de sommeil (no sleep, 0)"; // Mode 0 : pas de sommeil
        break;
    case 1:
        desc = "Sommeil léger (light sleep, 1)"; // Mode 1 : sommeil léger
        break;
    case 2:
        desc = "Sommeil modem (modem sleep, 2)"; // Mode 2 : sommeil modem
        break;
    }
    snprintf(out, out_size, "%s", desc);               // Copie la description dans le buffer de sortie
    ESP01_LOG_DEBUG("SLEEP", "Description : %s", out); // Log la description pour debug
    return ESP01_OK;                                   // Retourne le statut OK
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
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    const char *desc = "Inconnu"; // Valeur par défaut si niveau inconnu
    switch (syslog)
    {
    case 0:
        desc = "Désactivé (0)"; // Niveau 0 : désactivé
        break;
    case 1:
        desc = "Erreur (1)"; // Niveau 1 : erreur
        break;
    case 2:
        desc = "WARNING (2)"; // Niveau 2 : warning
        break;
    case 3:
        desc = "Info (3)"; // Niveau 3 : info
        break;
    case 4:
        desc = "Debug (4)"; // Niveau 4 : debug
        break;
    }
    snprintf(out, out_size, "%s", desc);                // Copie la description dans le buffer de sortie
    ESP01_LOG_DEBUG("SYSLOG", "Description : %s", out); // Log la description pour debug
    return ESP01_OK;                                    // Retourne le statut OK
}

// ==================== OUTILS DE PARSING ====================

/**
 * @brief  Parse un entier après un motif dans une réponse AT.
 * @param  resp    Réponse AT.
 * @param  pattern Motif à chercher.
 * @param  out     Pointeur vers la variable de sortie.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_parse_int_after(const char *resp, const char *pattern, int32_t *out)
{
    VALIDATE_PARAM(resp && pattern && out, ESP01_INVALID_PARAM);
    char *ptr = strstr(resp, pattern); // Cherche le motif dans la réponse
    if (!ptr)
        ESP01_RETURN_ERROR("PARSE_INT", ESP01_FAIL);
    ptr = strchr(ptr, ':'); // Cherche le caractère ':'
    if (!ptr)
        ESP01_RETURN_ERROR("PARSE_INT", ESP01_FAIL);
    *out = (int32_t)strtol(ptr + 1, NULL, 10); // Convertit la valeur trouvée en int32_t
    return ESP01_OK;
}

/**
 * @brief  Extrait une chaîne après un motif et jusqu'à \r, \n ou la fin.
 * @param  resp     Chaîne de réponse à analyser.
 * @param  pattern  Motif à chercher (ex: "+UART", "+UART_CUR").
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_parse_string_after(const char *resp, const char *pattern, char *out, size_t out_size)
{
    VALIDATE_PARAM(resp && pattern && out && out_size > 0, ESP01_INVALID_PARAM);

    char *start = strstr(resp, pattern); // Cherche le motif dans la réponse
    if (!start)
        ESP01_RETURN_ERROR("PARSE_STR", ESP01_FAIL);
    start = strchr(start, ':'); // Cherche le caractère ':'
    if (!start)
        ESP01_RETURN_ERROR("PARSE_STR", ESP01_FAIL);
    start++;

    size_t len = 0;
    while (start[len] && start[len] != '\r' && start[len] != '\n' && len < out_size - 1)
        len++;
    if (esp01_check_buffer_size(len, out_size - 1) != ESP01_OK)
        ESP01_RETURN_ERROR("PARSE_STR", ESP01_BUFFER_OVERFLOW);
    // strncpy(out, ptr, len);
    // out[len] = '\0';
    if (len < out_size)
    {
        memcpy(out, start, len);
        out[len] = '\0';
    }
    else
    {
        out[0] = '\0';
    }
    return ESP01_OK;
}

/**
 * @brief  Extrait la première chaîne entre guillemets après un motif.
 * @param  src      Buffer source (réponse AT).
 * @param  motif    Motif à chercher (ex: "+CIFSR:STAIP,\"").
 * @param  out      Buffer de sortie.
 * @param  out_len  Taille du buffer de sortie.
 * @retval true si trouvé, false sinon.
 */
bool esp01_extract_quoted_value(const char *src, const char *motif, char *out, size_t out_len)
{
    VALIDATE_PARAM(src && motif && out && out_len > 0, false);

    const char *p = strstr(src, motif); // Cherche le motif dans la réponse
    if (!p)
        return false;               // Retourne false si motif non trouvé
    p += strlen(motif);             // Passe le motif
    const char *q = strchr(p, '"'); // Cherche le guillemet fermant
    size_t len = q ? (size_t)(q - p) : 0;
    if (!q || esp01_check_buffer_size(len, out_len - 1) != ESP01_OK)
        return false;  // Retourne false si pas de guillemet ou buffer trop petit
    if (len < out_len) // Si la longueur est inférieure à la taille du buffer
    {
        memcpy(out, p, len); // Copie la chaîne trouvée dans le buffer de sortie
        out[len] = 0;        // Termine la chaîne par un caractère nul
    }
    else // Si la longueur est égale ou supérieure à la taille du buffer
    {
        // esp01_safe_strcpy(out, out_len, p); // (inutile ici car p n'est pas terminée)
        out[0] = 0;
    }
    return true; // Retourne true si la chaîne a été extraite avec succès
}

/**
 * @brief  Parse un booléen après un motif dans une réponse AT.
 * @param  resp Réponse AT.
 * @param  tag  Motif à chercher.
 * @param  out  Pointeur vers la variable booléenne de sortie.
 * @retval ESP01_Status_t Code de statut.
 */
ESP01_Status_t esp01_parse_bool_after(const char *resp, const char *tag, bool *out)
{
    VALIDATE_PARAM(resp && tag && out, ESP01_INVALID_PARAM);

    const char *ptr = strstr(resp, tag);
    if (!ptr)
        return ESP01_FAIL;

    ptr += strlen(tag);
    while (*ptr == ' ' || *ptr == ':' || *ptr == '=')
        ptr++;

    if (strncmp(ptr, "true", 4) == 0)
    {
        *out = true;
        return ESP01_OK;
    }
    if (strncmp(ptr, "false", 5) == 0)
    {
        *out = false;
        return ESP01_OK;
    }
    if (*ptr == '1')
    {
        *out = true;
        return ESP01_OK;
    }
    if (*ptr == '0')
    {
        *out = false;
        return ESP01_OK;
    }
    return ESP01_FAIL;
}

// ==================== TERMINAL AT (CONSOLE) ====================

/**
 * @brief  Initialise la réception du terminal AT (console).
 * @param  huart_debug Pointeur sur l’UART de debug.
 */
void esp01_terminal_begin(UART_HandleTypeDef *huart_debug)
{
    VALIDATE_PARAM_VOID(huart_debug); // Correction : macro dédiée pour void
    g_debug_uart = huart_debug;
    HAL_UART_Receive_IT(g_debug_uart, (uint8_t *)&esp_console_rx_char, 1);
}

/**
 * @brief  Callback de réception UART pour la console AT.
 * @param  huart Pointeur sur la structure UART.
 */
void esp01_console_rx_callback(UART_HandleTypeDef *huart)
{
    VALIDATE_PARAM_VOID(huart); // Correction : macro dédiée pour void
    if (huart == g_debug_uart)
    {
        char c = esp_console_rx_char;
        if (!esp_console_cmd_ready && esp_console_cmd_idx < ESP01_MAX_CMD_BUF - 1)
        {
            if (c == '\r' || c == '\n')
            {
                esp_console_cmd_buf[esp_console_cmd_idx] = '\0';
                esp_console_cmd_ready = 1;
            }
            else if (c >= 32 && c <= 126)
            {
                esp_console_cmd_buf[esp_console_cmd_idx++] = c;
            }
        }
        HAL_UART_Receive_IT(g_debug_uart, (uint8_t *)&esp_console_rx_char, 1);
    }
}

/**
 * @brief  Traite une commande AT saisie dans la console et récupère la réponse.
 * @param  out_buf      Buffer de sortie.
 * @param  out_buf_size Taille du buffer.
 * @retval ESP01_Status_t Code de statut.
 */
static ESP01_Status_t esp01_interactive_at_console(char *out_buf, size_t out_buf_size)
{
    VALIDATE_PARAM(out_buf && out_buf_size > 0, ESP01_INVALID_PARAM);

    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer

    if (!esp_console_cmd_ready || esp_console_cmd_idx == 0)
        return ESP01_FAIL;

    uint32_t timeout = ESP01_TIMEOUT_SHORT;

    const char *cmds_long[] = {
        "AT+CWJAP", "AT+CWLAP", "AT+CIPSTART", "AT+CIPSEND", "AT+MQTTCONN", "AT+HTTPCLIENT", "AT+RESTORE", "AT+UPDATE", "AT+CMD?", NULL};

    for (int i = 0; cmds_long[i]; ++i)
    {
        if (strncmp((char *)esp_console_cmd_buf, cmds_long[i], strlen(cmds_long[i])) == 0)
        {
            timeout = ESP01_TIMEOUT_LONG;
            break;
        }
    }

    ESP01_Status_t status = esp01_send_raw_command_dma(
        (char *)esp_console_cmd_buf, // Commande à envoyer
        out_buf,                     // Buffer de sortie pour la réponse
        out_buf_size,                // Taille du buffer de sortie
        "OK",                        // Motif attendu dans la réponse
        timeout);                    // Timeout adapté à la commande

    return status; // Retourne le statut de l'envoi
}

/**
 * @brief  Tâche de gestion du terminal AT interactif.
 */
void esp01_console_task(void)
{
    // Pas de paramètre à valider ici
    static uint8_t prompt_affiche = 0;

    if (!prompt_affiche && esp_console_cmd_ready == 0)
    {
        printf("\r\n[ESP01] === Entrez une commande AT : ");
        fflush(stdout);
        prompt_affiche = 1;
    }

    if (esp_console_cmd_ready)
    {
        char reponse[ESP01_LARGE_RESP_BUF];
        esp01_interactive_at_console(reponse, sizeof(reponse));
        printf("[ESP01] >>> %s", reponse);

        if (
            strstr((char *)esp_console_cmd_buf, "AT+RST") ||
            strstr((char *)esp_console_cmd_buf, "AT+RESTORE"))
        {
            printf("\r\n[ESP01] >>> Attente du redémarrage du module...\r\n");
            char boot_msg[ESP01_MAX_RESP_BUF] = {0};
            uint32_t start = HAL_GetTick();
            while ((HAL_GetTick() - start) < 8000)
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

        esp_console_cmd_ready = 0;
        esp_console_cmd_idx = 0;
        prompt_affiche = 0;
    }
}

/**
 * @brief  (Interne) Supprime les espaces/retours en début et fin de chaîne.
 * @param  str  Chaîne à nettoyer.
 */
void _esp_trim_string(char *str)
{
    VALIDATE_PARAM_VOID(str);

    // Supprime les espaces au début
    char *start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n'))
        start++;

    // Si des espaces ont été trouvés au début, décaler la chaîne
    if (start != str)
        memmove(str, start, strlen(start) + 1);

    // Supprime les espaces à la fin
    char *end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';
}
