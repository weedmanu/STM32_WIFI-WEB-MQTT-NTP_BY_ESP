/**
 * @file    STM32_WifiESP.c
 * @author  manu
 * @version 1.2.0
 * @date    13 juin 2025
 * @brief   Implémentation du driver bas niveau ESP01 (UART, AT, debug, reset, etc)
 *
 * @details
 * Ce fichier source contient l’implémentation des fonctions bas niveau pour le module ESP01 :
 *   - Initialisation et configuration UART (DMA, IT)
 *   - Gestion des commandes AT (envoi, réception, parsing)
 *   - Gestion du terminal série (console AT interactive)
 *   - Fonctions utilitaires : reset, restore, logs, gestion du mode sommeil, puissance RF, etc.
 *   - Statistiques d’utilisation et gestion des erreurs
 *
 * @note
 *   - Compatible STM32CubeIDE.
 *   - Nécessite la configuration de 2 UART (ESP01 + debug/console).
 *   - Utilise la réception DMA circulaire pour l’ESP01 et IT pour la console.
 *   - Toutes les fonctions sont utilisables sans connexion WiFi.
 ******************************************************************************
 */

#include "STM32_WifiESP.h" // Header du driver ESP01
#include <stdlib.h>        // Pour strtol
#include <stdarg.h>        // Pour va_list, va_start, va_end
#include <string.h>        // Pour memcpy, strlen, strstr, strchr
#include <stdio.h>         // Pour snprintf, vsnprintf, printf
#include <ctype.h>         // Pour isspace

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

// ========================= WRAPPERS API (COMMANDES AT HAUT NIVEAU) =========================
/**
 * @brief  Initialise le driver ESP01 (UART, DMA, debug, etc).
 * @param  huart_esp   Pointeur sur l'UART utilisée pour l'ESP01.
 * @param  huart_debug Pointeur sur l'UART de debug/console.
 * @param  dma_rx_buf  Buffer DMA pour la réception UART.
 * @param  dma_buf_size Taille du buffer DMA RX.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
    // Vérifie la validité des pointeurs et de la taille du buffer
    VALIDATE_PARAM(esp01_is_valid_ptr(huart_esp) && esp01_is_valid_ptr(huart_debug) && esp01_is_valid_ptr(dma_rx_buf) && dma_buf_size > 0, ESP01_INVALID_PARAM);

    g_esp_uart = huart_esp;        // Initialise l'UART pour l'ESP01
    g_debug_uart = huart_debug;    // Initialise l'UART pour le debug/console
    g_dma_rx_buf = dma_rx_buf;     // Affecte le buffer DMA pour la réception UART
    g_dma_buf_size = dma_buf_size; // Définit la taille du buffer DMA RX
    g_server_port = 80;            // Définit le port par défaut du serveur HTTP

    // Initialise la réception DMA pour l'ESP01
    if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK) // début if : échec initialisation DMA
    {
        ESP01_LOG_ERROR("INIT", "Erreur initialisation DMA RX : %s", esp01_get_error_string(ESP01_FAIL)); // Log l'erreur d'initialisation DMA
        ESP01_RETURN_ERROR("INIT", ESP01_NOT_INITIALIZED);                                                // Retourne une erreur d'initialisation
    } // fin if
    HAL_Delay(500); // Petit délai pour laisser l'ESP01 démarrer

    ESP01_Status_t status = esp01_test_at(); // Teste la communication avec l'ESP01 via la commande AT
    if (status != ESP01_OK)                  // début if : test AT échoué
    {
        ESP01_LOG_ERROR("INIT", "ESP01 non détecté !"); // Log l'erreur de détection
        ESP01_RETURN_ERROR("INIT", ESP01_NOT_DETECTED); // Retourne une erreur de détection
    } // fin if
    return ESP01_OK; // Retourne OK si l'initialisation réussit
}

/**
 * @brief  Teste la communication avec l'ESP01 via la commande AT.
 * @retval Statut de la commande (OK, TIMEOUT, FAIL)
 */
ESP01_Status_t esp01_test_at(void)
{
    char resp[ESP01_MAX_RESP_BUF];                                                                       // Buffer pour la réponse complète
    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT et attend "OK"
    return st;                                                                                           // Retourne le statut (OK, TIMEOUT, FAIL)
}

/**
 * @brief  Effectue un reset logiciel du module ESP01 (AT+RST).
 * @retval ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_reset(void)
{
    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer la commande

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+RST\r\n", 8, HAL_MAX_DELAY); // Envoie la commande AT+RST pour reset

    uint32_t start = HAL_GetTick();      // Timestamp de départ pour le timeout
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse complète
    size_t resp_len = 0;                 // Longueur de la réponse reçue

    ESP01_LOG_DEBUG("ESP01", ">>> AT+RST\n");
    ESP01_LOG_DEBUG("ESP01", ">>> Attente du redémarrage du module...");

    // Lecture de la réponse complète pendant 3 secondes max
    while ((HAL_GetTick() - start) < 3000 && resp_len < sizeof(resp) - 1) // début while : boucle jusqu'à timeout ou buffer plein
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];              // Buffer temporaire pour lecture
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)                                    // début if : des octets ont été reçus
        {
            if (resp_len + len >= sizeof(resp) - 1) // début if : empêche le dépassement de buffer
                len = sizeof(resp) - 1 - resp_len;  // Ajuste la taille à copier
            memcpy(resp + resp_len, buf, len);      // Ajoute au buffer de réponse
            resp_len += len;                        // Met à jour la longueur totale
            resp[resp_len] = '\0';                  // Termine la chaîne
        } // fin if (len > 0)
        else // début else : rien reçu
        {
            HAL_Delay(1); // Petite pause CPU
        } // fin else
    } // fin while
    ESP01_LOG_DEBUG("RESET", "Réponse complète : %s", resp); // Log la réponse complète reçue
    HAL_Delay(1000);                                         // Attend un peu le redémarrage du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Teste la communication AT après le reset

    if (st == ESP01_OK) // début if : test AT OK
    {
        ESP01_LOG_DEBUG("RESET", "AT OK après reset, reset réussi"); // Log succès reset
        return ESP01_OK;                                             // Retourne OK
    } // fin if
    else // début else : test AT échoué
    {
        ESP01_LOG_ERROR("RESET", "AT échoué après reset : %s", esp01_get_error_string(st)); // Log échec reset
        return ESP01_FAIL;                                                                  // Retourne FAIL
    } // fin else
}

/**
 * @brief  Restaure les paramètres d'usine du module ESP01 (AT+RESTORE).
 * @retval ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_restore(void)
{
    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer la commande

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+RESTORE\r\n", 12, HAL_MAX_DELAY); // Envoie la commande AT+RESTORE

    uint32_t start = HAL_GetTick();      // Timestamp de départ pour le timeout
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse complète
    size_t resp_len = 0;                 // Longueur de la réponse reçue

    ESP01_LOG_DEBUG("ESP01", ">>> AT+RESTORE\n");
    ESP01_LOG_DEBUG("ESP01", ">>> Attente du redémarrage du module...");

    // Lecture de la réponse complète pendant 3 secondes max
    while ((HAL_GetTick() - start) < 3000 && resp_len < sizeof(resp) - 1) // début while : boucle jusqu'à timeout ou buffer plein
    {
        uint8_t buf[ESP01_SMALL_BUF_SIZE];              // Buffer temporaire pour lecture
        int len = esp01_get_new_data(buf, sizeof(buf)); // Récupère les nouveaux octets reçus
        if (len > 0)                                    // début if : des octets ont été reçus
        {
            if (resp_len + len >= sizeof(resp) - 1) // début if : empêche le dépassement de buffer
                len = sizeof(resp) - 1 - resp_len;  // Ajuste la taille à copier
            memcpy(resp + resp_len, buf, len);      // Ajoute au buffer de réponse
            resp_len += len;                        // Met à jour la longueur totale
            resp[resp_len] = '\0';                  // Termine la chaîne
        } // fin if (len > 0)
        else // début else : rien reçu
        {
            HAL_Delay(1); // Petite pause CPU
        } // fin else
    } // fin while
    ESP01_LOG_DEBUG("RESTORE", "Réponse complète : %s", resp); // Log la réponse complète reçue

    HAL_Delay(1000); // Attend un peu le redémarrage du module

    ESP01_Status_t st = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Teste la communication AT après le restore

    if (st == ESP01_OK) // début if : test AT OK
    {
        ESP01_LOG_DEBUG("RESTORE", "AT OK après restore, restore réussi"); // Log succès restore
        return ESP01_OK;                                                   // Retourne OK
    } // fin if
    else // début else : test AT échoué
    {
        ESP01_LOG_ERROR("RESTORE", "AT échoué après restore : %s", esp01_get_error_string(st)); // Log échec restore
        return ESP01_FAIL;                                                                      // Retourne FAIL
    } // fin else
}

/**
 * @brief  Récupère la version du firmware AT de l'ESP01.
 * @param  version_buf Buffer de sortie pour la version.
 * @param  buf_size    Taille du buffer.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(version_buf) && buf_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    char resp[ESP01_MAX_RESP_BUF];                                                                           // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+GMR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+GMR

    if (st != ESP01_OK) // début if : erreur lors de la commande AT+GMR
    {
        ESP01_LOG_ERROR("GMR", "Erreur récupération version : %s", esp01_get_error_string(st)); // Log l'erreur
        esp01_safe_strcpy(version_buf, buf_size, "Erreur récupération version");                // Copie un message d'erreur dans le buffer
        return st;                                                                              // Retourne le statut d'erreur
    } // fin if (erreur)

    esp01_safe_strcpy(version_buf, buf_size, resp); // Copie la réponse dans le buffer version

    ESP01_LOG_DEBUG("GMR", "Version AT récupérée (%d octets)", (int)strlen(version_buf)); // Log la version récupérée
    return ESP01_OK;                                                                      // Retourne OK
}

/**
 * @brief  Récupère la configuration UART courante de l'ESP01.
 * @param  out      Buffer de sortie pour la config brute.
 * @param  out_size Taille du buffer.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_uart_config(char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    char resp[ESP01_MAX_RESP_BUF];                                                                             // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+UART?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+UART?

    if (st != ESP01_OK)                 // début if : échec de la commande
        ESP01_RETURN_ERROR("UART", st); // Retourne une erreur si la commande échoue

    // Utilisation du helper de parsing
    if (esp01_parse_string_after(resp, "+UART", out, out_size) == ESP01_OK ||
        esp01_parse_string_after(resp, "+UART_CUR", out, out_size) == ESP01_OK)
    {                                                      // début if : parsing réussi
        ESP01_LOG_DEBUG("UART", "Config brute : %s", out); // Log la config brute
        return ESP01_OK;                                   // Succès
    } // fin if (parsing réussi)
    ESP01_RETURN_ERROR("UART", ESP01_FAIL); // Retourne une erreur si parsing échoue
}

/**
 * @brief  Configure l’UART de l’ESP01 (baudrate, data, stop, parité, flow).
 * @param  baud      Baudrate souhaité.
 * @param  databits  Nombre de bits de données (5-8).
 * @param  stopbits  Nombre de bits de stop (1-2).
 * @param  parity    Parité (0:aucune, 1:impair, 2:pair).
 * @param  flowctrl  Contrôle de flux (0:aucun, 1:RTS, 2:CTS, 3:RTS+CTS).
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_set_uart_config(uint32_t baud, uint8_t databits, uint8_t stopbits, uint8_t parity, uint8_t flowctrl)
{
    VALIDATE_PARAM(databits >= 5 && databits <= 8 && stopbits >= 1 && stopbits <= 2 && parity <= 2 && flowctrl <= 3, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    char cmd[ESP01_MAX_CMD_BUF];                                                                       // Buffer pour la commande AT
    snprintf(cmd, sizeof(cmd), "AT+UART=%lu,%u,%u,%u,%u", baud, databits, stopbits, parity, flowctrl); // Formate la commande AT+UART

    char resp[ESP01_SMALL_BUF_SIZE * 4] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"
    ESP01_LOG_DEBUG("UART", "Réponse : %s", resp);                                                      // Log la réponse
    if (st != ESP01_OK)                                                                                 // début if : échec de la commande
        ESP01_RETURN_ERROR("UART_SET", st);                                                             // Retourne une erreur si la commande échoue
    return ESP01_OK;                                                                                    // Succès
}

/**
 * @brief  Récupère le mode sommeil actuel de l'ESP01.
 * @param  mode Pointeur vers la variable de sortie (0, 1 ou 2).
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_sleep_mode(int *mode)
{
    VALIDATE_PARAM(mode, ESP01_INVALID_PARAM); // Vérifie la validité du pointeur

    char resp[ESP01_MAX_RESP_BUF];                                                                              // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SLEEP?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+SLEEP?

    if (st != ESP01_OK)                  // début if : échec de la commande
        ESP01_RETURN_ERROR("SLEEP", st); // Retourne une erreur si la commande échoue

    int32_t mode_tmp = 0; // Variable temporaire pour le mode sommeil
    if (esp01_parse_int_after(resp, "+SLEEP", &mode_tmp) == ESP01_OK)
    {                                                              // début if : parsing réussi
        *mode = (int)mode_tmp;                                     // Affecte le mode récupéré
        ESP01_LOG_DEBUG("SLEEP", "Mode sommeil brut : %d", *mode); // Log le mode
        return ESP01_OK;                                           // Succès
    } // fin if (parsing réussi)
    ESP01_RETURN_ERROR("SLEEP", ESP01_FAIL); // Retourne une erreur si parsing échoue
}

/**
 * @brief  Définit le mode sommeil de l'ESP01.
 * @param  mode 0:aucun, 1:light sleep, 2:deep sleep.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_set_sleep_mode(int mode)
{
    VALIDATE_PARAM(mode >= 0 && mode <= 2, ESP01_INVALID_PARAM); // Vérifie la validité du paramètre

    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+SLEEP=%d", mode);                                                    // Formate la commande AT+SLEEP
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"
    ESP01_LOG_DEBUG("SLEEP", "Réponse : %s", resp);                                                     // Log la réponse
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SLEEP_SET", st); // Retourne une erreur si la commande échoue
    return ESP01_OK;                         // Succès
}

/**
 * @brief  Récupère la puissance RF actuelle de l'ESP01 (en dBm).
 * @param  dbm Pointeur vers la variable de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_rf_power(int *dbm)
{
    VALIDATE_PARAM(dbm, ESP01_INVALID_PARAM); // Vérifie la validité du pointeur

    char resp[ESP01_MAX_RESP_BUF];                                                                                // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+RFPOWER?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+RFPOWER?

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("RFPOWER", st); // Retourne une erreur si la commande échoue

    int32_t dbm_tmp = 0;
    if (esp01_parse_int_after(resp, "+RFPOWER", &dbm_tmp) == ESP01_OK)
    {
        *dbm = (int)dbm_tmp;                                       // Affecte la valeur récupérée
        ESP01_LOG_DEBUG("RFPOWER", "Puissance RF : %d dBm", *dbm); // Log la puissance
        return ESP01_OK;                                           // Succès
    }
    ESP01_RETURN_ERROR("RFPOWER", ESP01_FAIL); // Retourne une erreur si parsing échoue
}

/**
 * @brief  Définit la puissance RF de l'ESP01 (en dBm).
 * @param  dbm Puissance souhaitée (0 à 82).
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_set_rf_power(int dbm)
{
    VALIDATE_PARAM(dbm >= 0 && dbm <= 82, ESP01_INVALID_PARAM); // Vérifie la validité du paramètre

    char cmd[24];
    snprintf(cmd, sizeof(cmd), "AT+RFPOWER=%d", dbm); // Formate la commande AT+RFPOWER

    char resp[ESP01_SMALL_BUF_SIZE * 4] = {0};
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("RFPOWER_SET", st); // Retourne une erreur si la commande échoue
    return ESP01_OK;                           // Succès
}

/**
 * @brief  Récupère le niveau de log système de l'ESP01.
 * @param  level Pointeur vers la variable de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_syslog(int *level)
{
    VALIDATE_PARAM(level, ESP01_INVALID_PARAM); // Vérifie la validité du pointeur

    char resp[ESP01_MAX_RESP_BUF];                                                                               // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSLOG?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+SYSLOG?

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SYSLOG", st); // Retourne une erreur si la commande échoue

    int32_t level_tmp = 0;
    if (esp01_parse_int_after(resp, "+SYSLOG", &level_tmp) == ESP01_OK)
    {
        *level = (int)level_tmp;                              // Affecte la valeur récupérée
        ESP01_LOG_DEBUG("SYSLOG", "Niveau log : %d", *level); // Log le niveau

        return ESP01_OK; // Succès
    }
    ESP01_RETURN_ERROR("SYSLOG", ESP01_FAIL); // Retourne une erreur si parsing échoue
}

/**
 * @brief  Définit le niveau de log système de l'ESP01.
 * @param  level Niveau souhaité (0 à 4).
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_set_syslog(int level)
{
    VALIDATE_PARAM(level >= 0 && level <= 4, ESP01_INVALID_PARAM); // Vérifie la validité du paramètre

    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+SYSLOG=%d", level); // Formate la commande AT+SYSLOG

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SYSLOG_SET", st); // Retourne une erreur si la commande échoue
    return ESP01_OK;                          // Succès
}

/**
 * @brief  Récupère la quantité de RAM libre et minimale sur l'ESP01.
 * @param  free_ram Pointeur vers la variable de sortie (RAM libre).
 * @param  min_ram  Pointeur vers la variable de sortie (RAM min historique).
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_sysram(uint32_t *free_ram, uint32_t *min_ram)
{
    VALIDATE_PARAM(free_ram && min_ram, ESP01_INVALID_PARAM);
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSRAM?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SYSRAM", st);
    char sysram_str[ESP01_MAX_RESP_BUF] = {0};
    st = esp01_parse_string_after(resp, "+SYSRAM", sysram_str, sizeof(sysram_str));
    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("SYSRAM", ESP01_FAIL);
    unsigned long ram1 = 0, ram2 = 0;
    if (sscanf(sysram_str, "%lu,%lu", &ram1, &ram2) != 2)
        ESP01_RETURN_ERROR("SYSRAM", ESP01_FAIL);
    *free_ram = (uint32_t)ram1;
    *min_ram = (uint32_t)ram2;
    ESP01_LOG_DEBUG("SYSRAM", "RAM libre: %lu, RAM min: %lu", ram1, ram2);
    return ESP01_OK;
}

/**
 * @brief  Récupère la taille du stockage système de l'ESP01.
 * @param  sysstore Pointeur vers la variable de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_sysstore(uint32_t *sysstore)
{
    VALIDATE_PARAM(sysstore, ESP01_INVALID_PARAM);                                                                 // Vérifie la validité du pointeur
    char resp[ESP01_MAX_RESP_BUF];                                                                                 // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSSTORE?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+SYSSTORE?
    if (st != ESP01_OK)
        return st; // Retourne le statut d'erreur
    int32_t val = 0;
    if (esp01_parse_int_after(resp, "+SYSSTORE", &val) == ESP01_OK)
    {
        *sysstore = (uint32_t)val; // Affecte la valeur récupérée
        return ESP01_OK;           // Succès
    }
    return ESP01_FAIL; // Retourne une erreur si parsing échoue
}

/**
 * @brief  Récupère la taille de la RAM utilisateur de l'ESP01.
 * @param  userram Pointeur vers la variable de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_get_userram(uint32_t *userram)
{
    VALIDATE_PARAM(userram, ESP01_INVALID_PARAM);                                                                 // Vérifie la validité du pointeur
    char resp[ESP01_MAX_RESP_BUF];                                                                                // Buffer pour la réponse
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+USERRAM?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+USERRAM?
    if (st != ESP01_OK)
        return st; // Retourne le statut d'erreur
    int32_t val = 0;
    if (esp01_parse_int_after(resp, "+USERRAM", &val) == ESP01_OK)
    {
        *userram = (uint32_t)val; // Affecte la valeur récupérée
        return ESP01_OK;          // Succès
    }
    return ESP01_FAIL; // Retourne une erreur si parsing échoue
}

/**
 * @brief  Met l'ESP01 en deep sleep pour une durée donnée (ms).
 * @param  ms Durée du deep sleep en millisecondes.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_deep_sleep(uint32_t ms)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    snprintf(cmd, sizeof(cmd), "AT+GSLP=%lu", ms);                                                      // Formate la commande AT+GSLP
    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande et attend "OK"

    if (st != ESP01_OK)
        ESP01_RETURN_ERROR("GSLP", st); // Retourne une erreur si la commande échoue
    return ESP01_OK;                    // Succès
}

ESP01_Status_t esp01_get_sysflash(char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM);
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSFLASH?", out, out_size, "OK", ESP01_TIMEOUT_SHORT);
    return st;
}
// ========================= HELPERS (CONVERSION, AFFICHAGE, FORMATAGE HUMAIN) =========================
/**
 * @brief  Affiche les informations du firmware à partir de la réponse AT+GMR.
 * @param  gmr_resp Réponse brute de la commande AT+GMR.
 * @retval Nombre de lignes affichées.
 */
uint8_t esp01_display_firmware_info(const char *gmr_resp)
{
    if (!gmr_resp || !*gmr_resp)                    // début if : vérifie si la chaîne est vide ou nulle
        return 0;                                   // Retourne 0 si rien à afficher
    const char *start = strstr(gmr_resp, "AT+GMR"); // Cherche le début de la réponse AT+GMR
    if (start)                                      // début if : motif trouvé
        start += 6;                                 // Passe le motif
    else                                            // début else : motif non trouvé
        start = gmr_resp;                           // Utilise le début de la chaîne
    // fin if/else (motif)
    while (*start && (*start == '\r' || *start == '\n')) // début while : saute les retours à la ligne
        start++;                                         // Passe les caractères de saut de ligne
    // fin while (sauts de ligne)
    uint8_t line_count = 0;            // Compteur de lignes
    char line_buf[ESP01_MAX_LINE_BUF]; // Buffer pour une ligne
    const char *line_start = start;    // Pointeur de début de ligne
    while (*line_start)                // début while : parcours chaque ligne
    {
        const char *line_end = strpbrk(line_start, "\r\n"); // Cherche la fin de la ligne
        if (!line_end)                                      // début if : pas de fin de ligne trouvée
            line_end = line_start + strlen(line_start);     // Utilise la fin de la chaîne
        // fin if (pas de fin de ligne)
        size_t len = line_end - line_start;    // Calcule la longueur de la ligne
        if (len > 0 && len < sizeof(line_buf)) // début if : ligne non vide et taille correcte
        {
            memcpy(line_buf, line_start, len); // Copie la ligne dans le buffer
            line_buf[len] = '\0';              // Termine la chaîne
            // N'affiche pas la ligne OK
            if (strcmp(line_buf, "OK") != 0)
            {
                printf("[ESP01][GMR] %s\r\n", line_buf); // Affiche la ligne
                line_count++;                            // Incrémente le compteur
            }
        } // fin if (ligne non vide)
        line_start = (*line_end) ? line_end + 1 : line_end; // Passe à la ligne suivante
        while (*line_start == '\r' || *line_start == '\n')  // début while : saute les sauts de ligne
            line_start++;                                   // Passe les caractères de saut de ligne
        // fin while (sauts de ligne)
        if (!*line_start) // début if : fin de chaîne
            break;        // Sort de la boucle
        // fin if (fin de chaîne)
    } // fin while (parcours lignes)
    return line_count; // Retourne le nombre de lignes affichées
}

/**
 * @brief  Convertit un mode sommeil en description lisible.
 * @param  mode     Valeur du mode sommeil (0, 1, 2).
 * @param  out      Buffer de sortie pour la description.
 * @param  out_size Taille du buffer.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_sleep_mode_to_string(int mode, char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres
    const char *desc = "Inconnu";                                                 // Valeur par défaut
    switch (mode)                                                                 // début switch : sélection du mode
    {
    case 0:
        desc = "Aucun (modem actif)"; // Mode 0
        break;
    case 1:
        desc = "Light sleep (modem veille)"; // Mode 1
        break;
    case 2:
        desc = "Deep sleep (modem off)"; // Mode 2
        break;
    } // fin switch (mode)
    snprintf(out, out_size, "%s", desc);               // Copie la description dans le buffer
    ESP01_LOG_DEBUG("SLEEP", "Description : %s", out); // Log la description
    return ESP01_OK;                                   // Succès
}

/**
 * @brief  Convertit un niveau de log système en description lisible.
 * @param  syslog   Niveau de log (0 à 4).
 * @param  out      Buffer de sortie pour la description.
 * @param  out_size Taille du buffer.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_syslog_to_string(int syslog, char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres
    const char *desc = "Inconnu";                                                 // Valeur par défaut
    switch (syslog)                                                               // début switch : sélection du niveau de log
    {
    case 0:
        desc = "Aucun log"; // Niveau 0
        break;
    case 1:
        desc = "Erreur"; // Niveau 1
        break;
    case 2:
        desc = "Avertissement"; // Niveau 2
        break;
    case 3:
        desc = "Info"; // Niveau 3
        break;
    case 4:
        desc = "Debug"; // Niveau 4
        break;
    } // fin switch (syslog)
    snprintf(out, out_size, "%s", desc);                // Copie la description dans le buffer
    ESP01_LOG_DEBUG("SYSLOG", "Description : %s", out); // Log la description
    return ESP01_OK;                                    // Succès
}

/**
 * @brief  Formate la quantité de RAM libre et minimale en chaîne lisible.
 * @param  free_ram Quantité de RAM libre.
 * @param  min_ram  Quantité minimale de RAM jamais disponible.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_sysram_to_string(uint32_t free_ram, uint32_t min_ram, char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM);
    int n = snprintf(out, out_size, "%lu octets libres, %lu octets min", (unsigned long)free_ram, (unsigned long)min_ram);
    ESP01_LOG_DEBUG("SYSRAM", "RAM libre : %s", out);
    return (n > 0 && (size_t)n < out_size) ? ESP01_OK : ESP01_FAIL;
}

/**
 * @brief  Formate la taille de la RAM utilisateur en chaîne lisible.
 * @param  userram  Taille de la RAM utilisateur.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_userram_to_string(uint32_t userram, char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM);
    if (userram == 0)
        snprintf(out, out_size, "Aucune RAM utilisateur allouée");
    else
        snprintf(out, out_size, "RAM utilisateur disponible : %lu octets", (unsigned long)userram);
    ESP01_LOG_DEBUG("USERRAM", "%s", out);
    return ESP01_OK;
}

uint8_t esp01_display_sysflash_partitions(const char *sysflash_resp)
{
    if (!sysflash_resp || !*sysflash_resp)
        return 0;
    uint8_t count = 0;
    const char *p = sysflash_resp;
    while ((p = strstr(p, "+SYSFLASH:")) != NULL)
    {
        char name[ESP01_SMALL_BUF_SIZE] = {0};
        int type = 0, subtype = 0;
        unsigned int addr = 0, size = 0;
        int n = sscanf(p, "+SYSFLASH:\"%31[^\"]\",%d,%d,%x,%x", name, &type, &subtype, &addr, &size);
        if (n == 5)
        {
            printf("[ESP01][SYSFLASH] Partition: %s | type: %d | subtype: %d | addr: 0x%X | size: 0x%X\r\n",
                   name, type, subtype, addr, size);
            count++;
        }
        // Avance à la ligne suivante
        const char *next = strchr(p, '\n');
        if (!next)
            break;
        p = next + 1;
    }
    if (count == 0)
        printf("[ESP01][SYSFLASH] Aucune partition détectée\r\n");
    return count;
}

/**
 * @brief  Formate le mode de stockage des paramètres AT en chaîne lisible.
 * @param  sysstore Valeur du mode (0 ou 1).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_sysstore_to_string(uint32_t sysstore, char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM);
    const char *desc = "Inconnu";
    if (sysstore == 0)
        desc = "Non stocké en flash (RAM uniquement)";
    else if (sysstore == 1)
        desc = "Stocké en flash (défaut)";
    snprintf(out, out_size, "Mode stockage AT : %s", desc);
    ESP01_LOG_DEBUG("SYSSTORE", "%s", out);
    return ESP01_OK;
}

/**
 * @brief  Convertit une config UART brute en chaîne lisible.
 * @param  raw_config Config brute (ex: "115200,8,1,0,0").
 * @param  out       Buffer de sortie.
 * @param  out_size  Taille du buffer.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(raw_config) && esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    uint32_t baud = 0;                                                                   // Baudrate
    int data = 0, stop = 0, parity = 0, flow = 0;                                        // Paramètres UART
    if (sscanf(raw_config, "%lu,%d,%d,%d,%d", &baud, &data, &stop, &parity, &flow) != 5) // début if : parsing échoué
        ESP01_RETURN_ERROR("UART_PARSE", ESP01_FAIL);                                    // Retourne une erreur
    // fin if (parsing)

    const char *parity_str = "aucune"; // Parité par défaut
    if (parity == 1)                   // début if : parité impair
        parity_str = "impair";
    else if (parity == 2) // début else if : parité pair
        parity_str = "pair";
    // fin if/else (parité)

    const char *flow_str = "aucun"; // Contrôle de flux par défaut
    if (flow == 1)                  // début if : RTS
        flow_str = "RTS";
    else if (flow == 2) // début else if : CTS
        flow_str = "CTS";
    else if (flow == 3) // début else if : RTS+CTS
        flow_str = "RTS+CTS";
    // fin if/else (flow)

    snprintf(out, out_size, "baudrate=%lu, data bits=%d, stop bits=%d, parité=%s, flow control=%s",
             baud, data, stop, parity_str, flow_str); // Formate la chaîne finale

    ESP01_LOG_DEBUG("UART", "Config compréhensible : %s", out); // Log la config
    return ESP01_OK;                                            // Succès
}

/**
 * @brief  Récupère la liste des commandes AT supportées par l'ESP01 et logge la réponse par blocs de 15 lignes.
 * @param  out      Buffer de sortie pour la liste complète des commandes.
 * @param  out_size Taille du buffer de sortie (doit être >= ESP01_LARGE_RESP_BUF).
 * @retval ESP01_OK si succès, ESP01_TIMEOUT si timeout ou buffer plein.
 * @details
 * Envoie la commande AT+CMD? à l'ESP01, lit la réponse ligne par ligne jusqu'à trouver "OK" ou atteindre le timeout.
 * La réponse complète est copiée dans le buffer 'out'. Ensuite, la fonction découpe la réponse en blocs de 15 lignes
 * pour les logger proprement (évite les logs tronqués). Le dernier bloc (moins de 15 lignes) est aussi loggé.
 */
ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size >= ESP01_LARGE_RESP_BUF, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    size_t total_len = 0;           // Longueur totale de la réponse accumulée
    uint32_t start = HAL_GetTick(); // Temps de début pour le timeout
    int found_ok = 0;               // Indicateur pour savoir si "OK" a été trouvé
    char line[ESP01_MAX_RESP_BUF];  // Tampon pour une ligne de réponse
    size_t line_len = 0;            // Longueur de la ligne courante

    _flush_rx_buffer(100);                                                     // Vide le buffer RX avant d'envoyer la commande
    HAL_UART_Transmit(g_esp_uart, (uint8_t *)"AT+CMD?\r\n", 9, HAL_MAX_DELAY); // Envoie la commande AT+CMD?

    // Lecture de la réponse ligne par ligne jusqu'à "OK" ou timeout
    while ((HAL_GetTick() - start) < 30000 && total_len < out_size - 1) // début while : lecture de la réponse
    {
        uint8_t buf[ESP01_MAX_RESP_BUF];                // Tampon pour lire les données
        int len = esp01_get_new_data(buf, sizeof(buf)); // Lit les données de l'ESP01
        for (int i = 0; i < len; i++)                   // début for : parcours le tampon
        {
            char c = buf[i];                 // Caractère courant
            if (line_len < sizeof(line) - 1) // début if : vérifie la taille du tampon de ligne
                line[line_len++] = c;        // Ajoute le caractère à la ligne
            if (c == '\n')                   // début if : fin de ligne
            {
                line[line_len] = '\0';                                                                           // Termine la ligne
                size_t copy_len = (total_len + line_len < out_size - 1) ? line_len : (out_size - 1 - total_len); // Calcule la longueur à copier
                memcpy(out + total_len, line, copy_len);                                                         // Copie la ligne dans le buffer de sortie
                total_len += copy_len;                                                                           // Met à jour la longueur totale
                out[total_len] = '\0';                                                                           // Termine le buffer de sortie
                if (strstr(line, "OK"))                                                                          // début if : ligne OK
                {
                    found_ok = 1; // Marque que "OK" a été trouvé
                    break;        // Sort de la boucle de lecture
                }
                line_len = 0; // Réinitialise la longueur de la ligne pour la prochaine itération
            } // fin if (fin de ligne)
        } // fin for (parcours tampon)
        if (found_ok)     // début if : si "OK" a été trouvé
            break;        // Sort de la boucle de lecture
        if (len == 0)     // début if : pas de nouvelles données
            HAL_Delay(1); // Attend un peu avant de relire
    }

    // === Découpage et log de la réponse brute par blocs de 15 lignes ===
    int part = 1;           // Numéro du bloc courant
    int line_count = 0;     // Compteur de lignes dans le bloc courant
    size_t block_start = 0; // Index de début du bloc courant

    // Parcours la réponse pour découper en blocs de 15 lignes
    for (size_t i = 0; i < total_len; ++i) // début for : parcours la réponse
    {
        if (out[i] == '\n') // début if : fin de ligne
        {
            line_count++; // Incrémente le compteur de lignes
            // Si on a atteint 15 lignes, on log le bloc courant
            if (line_count == 15) // début if : fin de bloc
            {
                size_t block_len = i + 1 - block_start; // Longueur du bloc à logger
                char temp[ESP01_MAX_RESP_BUF];          // Tampon pour le message de log
                // Log le bloc de 15 lignes
                snprintf(temp, sizeof(temp), "Retour de la commande bloc %d (%d lignes) :\r\n%.*s", part, line_count, (int)block_len, out + block_start);
                ESP01_LOG_DEBUG("CMD", "%s", temp); // Log du bloc de 15 lignes
                part++;                             // Incrémente le numéro du bloc
                block_start = i + 1;                // Nouveau début de bloc
                line_count = 0;                     // Réinitialise le compteur de lignes
            } // fin if (fin de bloc)
        } // fin if (fin de ligne)
    }
    // Log le dernier bloc s'il reste des lignes (moins de 15)
    if (block_start < total_len) // début if : s'il reste des données à logger
    {
        size_t block_len = total_len - block_start; // Longueur du dernier bloc
        // Compte les lignes restantes dans le dernier bloc
        int last_lines = 0;                              // Compteur de lignes dans le dernier bloc
        for (size_t j = block_start; j < total_len; ++j) // début for : parcours le dernier bloc
            if (out[j] == '\n')                          // début if : fin de ligne
                last_lines++;                            // Incrémente le compteur de lignes
        char temp[ESP01_MAX_RESP_BUF];                   // Tampon pour le message de log
        // Log le dernier bloc (moins de 15 lignes)
        snprintf(temp, sizeof(temp), "Retour de la commande bloc %d (%d lignes) :\r\n%.*s", part, last_lines, (int)block_len, out + block_start);
        ESP01_LOG_DEBUG("CMD", "%s", temp); // Log du dernier bloc (moins de 15 lignes)
    } // fin if (dernier bloc)

    if (found_ok) // début if : si "OK" a été trouvé
    {
        ESP01_LOG_DEBUG("CMD", "Liste complète reçue (%lu octets)", (unsigned long)total_len); // Log le succès de la réception
        return ESP01_OK;                                                                       // Retourne le statut de succès
    } // fin if (OK trouvé)
    else // début else
    {
        ESP01_LOG_DEBUG("CMD", "Timeout ou buffer plein (%lu octets)", (unsigned long)total_len);
        return ESP01_TIMEOUT;
    }
}

// ========================= OUTILS DE PARSING =========================
/**
 * @brief  Extrait un entier après un motif dans une chaîne de caractères.
 * @param  text    Chaîne source à analyser.
 * @param  pattern Motif à rechercher.
 * @param  result  Pointeur vers la variable de sortie (int32_t).
 * @retval ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_parse_int_after(const char *text, const char *pattern, int32_t *result)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(text) && esp01_is_valid_ptr(pattern) && esp01_is_valid_ptr(result), ESP01_INVALID_PARAM); // Vérifie la validité des paramètres
    char *ptr = strstr(text, pattern);                                                                                          // Cherche le motif dans la réponse
    if (!ptr)                                                                                                                   // début if : motif non trouvé
        ESP01_RETURN_ERROR("PARSE_INT", ESP01_FAIL);                                                                            // Retourne une erreur
    // fin if (motif)
    ptr = strchr(ptr, ':');                          // Cherche le caractère ':'
    if (!ptr)                                        // début if : caractère ':' non trouvé
        ESP01_RETURN_ERROR("PARSE_INT", ESP01_FAIL); // Retourne une erreur
    // fin if (':')
    *result = (int32_t)strtol(ptr + 1, NULL, 10); // Convertit la valeur trouvée en int32_t
    return ESP01_OK;                              // Succès
}

/**
 * @brief  Extrait une sous-chaîne après un motif dans une chaîne de caractères.
 * @param  text    Chaîne source à analyser.
 * @param  pattern Motif à rechercher.
 * @param  output  Buffer de sortie.
 * @param  size    Taille du buffer de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_parse_string_after(const char *text, const char *pattern, char *output, size_t size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(text) && esp01_is_valid_ptr(pattern) && esp01_is_valid_ptr(output) && size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    char *start = strstr(text, pattern);             // Cherche le motif dans la réponse
    if (!start)                                      // début if : motif non trouvé
        ESP01_RETURN_ERROR("PARSE_STR", ESP01_FAIL); // Retourne une erreur
    // fin if (motif)
    start = strchr(start, ':'); // Cherche le caractère ':'
    start++;

    size_t len = 0;                                                                  // Compteur de longueur
    while (start[len] && start[len] != '\r' && start[len] != '\n' && len < size - 1) // début while : parcours la sous-chaîne
        len++;                                                                       // Incrémente la longueur
    // fin while (parcours)
    if (esp01_check_buffer_size(len, size - 1) != ESP01_OK)     // début if : buffer trop petit
        ESP01_RETURN_ERROR("PARSE_STR", ESP01_BUFFER_OVERFLOW); // Retourne une erreur
    // fin if (buffer)
    if (len < size) // début if : la longueur est correcte
    {
        memcpy(output, start, len); // Copie la sous-chaîne
        output[len] = '\0';         // Termine la chaîne
    } // fin if (longueur ok)
    else // début else : longueur trop grande
    {
        output[0] = '\0'; // Chaîne vide
    } // fin else (longueur)
    return ESP01_OK; // Succès
}

/**
 * @brief  Extrait une valeur entre guillemets après un motif dans une chaîne.
 * @param  src     Chaîne source à analyser.
 * @param  motif   Motif à rechercher.
 * @param  out     Buffer de sortie.
 * @param  out_len Taille du buffer de sortie.
 * @retval true si succès, false sinon.
 */
bool esp01_extract_quoted_value(const char *src, const char *motif, char *out, size_t out_len)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(src) && esp01_is_valid_ptr(motif) && esp01_is_valid_ptr(out) && out_len > 0, false); // Vérifie la validité des paramètres

    const char *p = strstr(src, motif); // Cherche le motif dans la réponse
    if (!p)                             // début if : motif non trouvé
        return false;                   // Retourne false si motif non trouvé
    // fin if (motif)
    p += strlen(motif);                                              // Passe le motif
    const char *q = strchr(p, '"');                                  // Cherche le guillemet fermant
    size_t len = q ? (size_t)(q - p) : 0;                            // Calcule la longueur
    if (!q || esp01_check_buffer_size(len, out_len - 1) != ESP01_OK) // début if : pas de guillemet ou buffer trop petit
        return false;                                                // Retourne false si pas de guillemet ou buffer trop petit
    // fin if (guillemets/buffer)
    if (len < out_len) // début if : longueur correcte
    {
        memcpy(out, p, len); // Copie la chaîne trouvée
        out[len] = 0;        // Termine la chaîne
    } // fin if (longueur ok)
    else // début else : longueur trop grande
    {
        out[0] = 0; // Chaîne vide
    } // fin else (longueur)
    return true; // Succès
}

/**
 * @brief  Extrait une valeur booléenne après un motif dans une chaîne.
 * @param  resp Chaîne source à analyser.
 * @param  tag  Motif à rechercher.
 * @param  out  Pointeur vers la variable booléenne de sortie.
 * @retval ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_parse_bool_after(const char *resp, const char *tag, bool *out)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(resp) && esp01_is_valid_ptr(tag) && esp01_is_valid_ptr(out), ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    const char *ptr = strstr(resp, tag); // Cherche le motif dans la réponse
    if (!ptr)                            // début if : motif non trouvé
        return ESP01_FAIL;               // Retourne une erreur
    // fin if (motif)

    ptr += strlen(tag);                               // Passe le motif
    while (*ptr == ' ' || *ptr == ':' || *ptr == '=') // début while : saute les séparateurs
        ptr++;                                        // Passe les séparateurs
    // fin while (séparateurs)

    if (strncmp(ptr, "true", 4) == 0) // début if : valeur true
    {
        *out = true;     // Affecte true
        return ESP01_OK; // Succès
    } // fin if (true)
    if (strncmp(ptr, "false", 5) == 0) // début if : valeur false
    {
        *out = false;    // Affecte false
        return ESP01_OK; // Succès
    } // fin if (false)
    if (*ptr == '1') // début if : valeur 1
    {
        *out = true;     // Affecte true
        return ESP01_OK; // Succès
    } // fin if (1)
    if (*ptr == '0') // début if : valeur 0
    {
        *out = false;    // Affecte false
        return ESP01_OK; // Succès
    } // fin if (0)
    return ESP01_FAIL; // Retourne une erreur si aucune valeur reconnue
}

/**
 * @brief  Découpe une chaîne de réponse en lignes distinctes.
 * @param  input_str    Chaîne source à découper.
 * @param  lines        Tableau de pointeurs vers les lignes extraites.
 * @param  max_lines    Nombre maximal de lignes à extraire.
 * @param  lines_buffer Buffer pour stocker les lignes extraites.
 * @param  buffer_size  Taille du buffer de lignes.
 * @param  skip_empty   Indique s'il faut ignorer les lignes vides.
 * @retval Nombre de lignes extraites.
 */
uint8_t esp01_split_response_lines(const char *input_str, char *lines[], uint8_t max_lines, char *lines_buffer, size_t buffer_size, bool skip_empty)
{
    if (!input_str || !*input_str || max_lines == 0 || !lines_buffer || buffer_size == 0) // début if : paramètres invalides
        return 0;                                                                         // Retourne 0 si erreur
    // fin if (paramètres)

    size_t total_copied = 0; // Compteur total copié
    uint8_t line_count = 0;  // Compteur de lignes

    const char *line_start = input_str;           // Pointeur de début de ligne
    while (*line_start && line_count < max_lines) // début while : parcours des lignes
    {
        const char *line_end = strstr(line_start, "\r\n"); // Cherche la fin de la ligne
        if (!line_end)                                     // début if : pas de retour à la ligne trouvé
            line_end = line_start + strlen(line_start);    // Fin de la chaîne si pas de retour à la ligne
        // fin if (fin de ligne)

        size_t line_length = line_end - line_start;       // Calcule la longueur de la ligne
        if (line_length > 0 && line_length < buffer_size) // début if : ligne non vide et taille correcte
        {
            memcpy(lines_buffer + total_copied, line_start, line_length); // Copie la ligne dans le buffer
            lines_buffer[total_copied + line_length] = '\0';              // Termine la chaîne

            lines[line_count++] = lines_buffer + total_copied; // Ajoute le pointeur vers la ligne

            total_copied += line_length + 1; // +1 pour le caractère nul
        } // fin if (ligne non vide)

        line_start = line_end + 2; // +2 pour sauter "\r\n"
    } // fin while (parcours lignes)

    if (line_count == max_lines) // début if : limite atteinte
    {
        lines[line_count] = NULL; // Ajoute une ligne vide
        total_copied += 1;        // Incrémente le compteur
    } // fin if (limite)

    return line_count; // Retourne le nombre de lignes extraites
}

// ========================= OUTILS UTILITAIRES (BUFFER, INLINE, VALIDATION) =========================
/**
 * @brief  Retourne une chaîne descriptive pour un code d'erreur ESP01_Status_t.
 * @param  status Code d'erreur ESP01_Status_t.
 * @retval Chaîne descriptive de l'erreur.
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
 * @brief  Attend l'apparition d'un motif dans le flux RX dans un délai donné.
 * @param  pattern    Motif à attendre dans la réponse.
 * @param  timeout_ms Délai maximal en millisecondes.
 * @retval ESP01_OK si motif trouvé, ESP01_TIMEOUT sinon.
 */
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(pattern), ESP01_INVALID_PARAM); // Validation du paramètre

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

// ========================= TERMINAL / CONSOLE AT =========================
/**
 * @brief  Initialise la console terminal AT sur l'UART de debug.
 * @param  huart_debug Pointeur sur l'UART de debug/console.
 * @retval Aucun
 */
void esp01_terminal_begin(UART_HandleTypeDef *huart_debug)
{
    VALIDATE_PARAM_VOID(esp01_is_valid_ptr(huart_debug)); // Correction : macro dédiée pour void
    g_debug_uart = huart_debug;
    HAL_UART_Receive_IT(g_debug_uart, (uint8_t *)&esp_console_rx_char, 1);
}

/**
 * @brief  Callback de réception d'un caractère sur l'UART de debug (console AT).
 * @param  huart Pointeur sur l'UART ayant reçu le caractère.
 * @retval Aucun
 */
void esp01_console_rx_callback(UART_HandleTypeDef *huart)
{
    VALIDATE_PARAM_VOID(esp01_is_valid_ptr(huart)); // Vérifie la validité du pointeur UART

    if (huart == g_debug_uart) // Vérifie que l'interruption vient bien de l'UART de debug
    {
        char c = esp_console_rx_char; // Récupère le caractère reçu

        // Si aucune commande n'est prête et que le buffer n'est pas plein
        if (!esp_console_cmd_ready && esp_console_cmd_idx < ESP01_MAX_CMD_BUF - 1)
        {
            if (c == '\r' || c == '\n') // Si retour chariot ou saut de ligne
            {
                esp_console_cmd_buf[esp_console_cmd_idx] = '\0'; // Termine la chaîne de commande
                esp_console_cmd_ready = 1;                       // Indique qu'une commande est prête à être traitée
            }
            else if (c >= 32 && c <= 126) // Si caractère affichable (ASCII)
            {
                esp_console_cmd_buf[esp_console_cmd_idx++] = c; // Ajoute le caractère au buffer de commande
            }
        }

        // Relance la réception IT pour le prochain caractère
        HAL_UART_Receive_IT(g_debug_uart, (uint8_t *)&esp_console_rx_char, 1);
    }
}

/**
 * @brief  Traite la commande AT saisie dans la console interactive et envoie la réponse.
 * @param  out_buf      Buffer de sortie pour la réponse.
 * @param  out_buf_size Taille du buffer de sortie.
 * @retval Statut de l'envoi de la commande (ESP01_Status_t)
 */
static ESP01_Status_t esp01_interactive_at_console(char *out_buf, size_t out_buf_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out_buf) && out_buf_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    esp01_flush_rx_buffer(10); // Vide le buffer RX avant d'envoyer la commande

    if (!esp_console_cmd_ready || esp_console_cmd_idx == 0) // Vérifie qu'une commande est prête
        return ESP01_FAIL;                                  // Retourne une erreur si aucune commande

    uint32_t timeout = ESP01_TIMEOUT_SHORT; // Timeout par défaut

    // Liste des commandes longues nécessitant un timeout étendu
    const char *cmds_long[] = {
        "AT+CWJAP", "AT+CWLAP", "AT+CIPSTART", "AT+CIPSEND", "AT+MQTTCONN", "AT+HTTPCLIENT", "AT+RESTORE", "AT+UPDATE", "AT+CMD?", NULL};

    // Parcourt la liste pour adapter le timeout si besoin
    for (int i = 0; cmds_long[i]; ++i)
    {
        if (strncmp((char *)esp_console_cmd_buf, cmds_long[i], strlen(cmds_long[i])) == 0) // Si la commande correspond à une commande longue
        {
            timeout = ESP01_TIMEOUT_LONG; // Utilise un timeout long
            break;                        // Sort de la boucle
        }
    }

    // Envoie la commande AT et récupère la réponse dans le buffer de sortie
    ESP01_Status_t status = esp01_send_raw_command_dma(
        (char *)esp_console_cmd_buf, // Commande à envoyer
        out_buf,                     // Buffer de sortie pour la réponse
        out_buf_size,                // Taille du buffer de sortie
        "OK",                        // Motif attendu dans la réponse
        timeout);                    // Timeout adapté à la commande

    return status; // Retourne le statut de l'envoi
}

/**
 * @brief  Tâche principale de gestion de la console AT interactive (affichage prompt, traitement commande).
 * @retval Aucun
 */
void esp01_console_task(void)
{
    // Pas de paramètre à valider ici
    static uint8_t prompt_affiche = 0; // Indique si le prompt a déjà été affiché

    if (!prompt_affiche && esp_console_cmd_ready == 0) // Si le prompt n'est pas affiché et aucune commande prête
    {
        printf("\r\n[ESP01] === Entrez une commande AT : "); // Affiche le prompt sur la console
        fflush(stdout);                                      // Vide le buffer de sortie
        prompt_affiche = 1;                                  // Marque le prompt comme affiché
    }

    if (esp_console_cmd_ready) // Si une commande AT est prête à être traitée
    {
        char reponse[ESP01_LARGE_RESP_BUF];                     // Buffer pour la réponse de la commande AT
        esp01_interactive_at_console(reponse, sizeof(reponse)); // Envoie la commande AT et récupère la réponse
        printf("[ESP01] >>> %s", reponse);                      // Affiche la réponse sur la console

        // Si la commande est un reset ou un restore, attendre le reboot du module
        if (
            strstr((char *)esp_console_cmd_buf, "AT+RST") ||   // Vérifie si la commande est AT+RST
            strstr((char *)esp_console_cmd_buf, "AT+RESTORE")) // Vérifie si la commande est AT+RESTORE
        {
            printf("\r\n[ESP01] >>> Attente du redémarrage du module...\r\n"); // Affiche un message d'attente
            char boot_msg[ESP01_MAX_RESP_BUF] = {0};                           // Buffer pour les messages de boot
            uint32_t start = HAL_GetTick();                                    // Timestamp de départ pour le timeout
            while ((HAL_GetTick() - start) < 8000)                             // Boucle d'attente du message "ready" (max 8s)
            {
                int len = esp01_get_new_data((uint8_t *)boot_msg, sizeof(boot_msg) - 1); // Récupère les nouveaux octets reçus
                if (len > 0)                                                             // Si des octets ont été reçus
                {
                    boot_msg[len] = '\0';          // Termine la chaîne
                    if (strstr(boot_msg, "ready")) // Si le message "ready" est trouvé
                    {
                        printf("[ESP01] >>> Module prêt !\r\n"); // Affiche que le module est prêt
                        break;                                   // Sort de la boucle d'attente
                    }
                }
                HAL_Delay(10); // Petite pause CPU pour ne pas bloquer le système
            }
        }

        esp_console_cmd_ready = 0; // Réinitialise l'indicateur de commande prête
        esp_console_cmd_idx = 0;   // Réinitialise l'index du buffer de commande
        prompt_affiche = 0;        // Réinitialise l'affichage du prompt
    }
}

/**
 * @brief  Fonction de log/debug pour l'ESP01 (envoi sur UART debug si activé).
 * @param  fmt Format printf.
 * @param  ... Arguments variables.
 * @retval Aucun
 */
void _esp_login(const char *fmt, ...)
{
#if ESP01_DEBUG
    VALIDATE_PARAM_VOID(esp01_is_valid_ptr(fmt)); // Vérifie la validité du format

    if (g_debug_uart) // Si l'UART debug est initialisée
    {
        char bigbuf[ESP01_MAX_RESP_BUF];              // Buffer temporaire pour formatter le message complet
        va_list args;                                 // Liste des arguments variables
        va_start(args, fmt);                          // Démarre la récupération des arguments
        vsnprintf(bigbuf, sizeof(bigbuf), fmt, args); // Formate la chaîne complète dans le buffer
        va_end(args);                                 // Termine la récupération des arguments

        size_t len = strlen(bigbuf); // Calcule la longueur du message formaté
        size_t chunk_size = 128;     // Taille d'un morceau à transmettre
        size_t sent = 0;             // Compteur d'octets déjà envoyés

        // Envoie le message par morceaux pour éviter de saturer l'UART
        while (sent < len)
        {
            size_t to_send = (len - sent > chunk_size) ? chunk_size : (len - sent);              // Calcule la taille du prochain morceau
            HAL_UART_Transmit(g_debug_uart, (uint8_t *)(bigbuf + sent), to_send, HAL_MAX_DELAY); // Envoie le morceau courant
            sent += to_send;                                                                     // Met à jour le compteur d'octets envoyés
        }
    }
#endif
}

/**
 * @brief  Vide le buffer RX DMA de l'ESP01 pendant un délai donné.
 * @param  timeout_ms Délai maximal en millisecondes pour vider le buffer.
 * @retval ESP01_OK si succès, ESP01_NOT_INITIALIZED sinon.
 */
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms)
{
    VALIDATE_PARAM(g_dma_rx_buf && g_dma_buf_size > 0, ESP01_NOT_INITIALIZED); // Vérifie la validité du buffer DMA et de sa taille

    uint32_t start = HAL_GetTick();             // Récupère le timestamp de départ pour le timeout
    volatile uint16_t last_pos = g_rx_last_pos; // Sauvegarde la dernière position lue dans le buffer DMA

    // Boucle jusqu'à ce que le délai soit écoulé ou que la position ne change plus
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        uint16_t pos = g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx); // Calcule la position actuelle dans le buffer DMA

        if (pos != last_pos) // Si la position a changé (nouveaux octets reçus)
        {
            last_pos = pos;        // Met à jour la dernière position lue
            start = HAL_GetTick(); // Réinitialise le timer pour attendre que le buffer soit stable
        }
    }
    g_rx_last_pos = last_pos; // Met à jour la position globale pour le prochain appel
    return ESP01_OK;          // Retourne OK si le buffer a été vidé
}

/**
 * @brief  Récupère les nouveaux octets reçus depuis le dernier appel (DMA circulaire).
 * @param  buf Buffer de destination.
 * @param  bufsize Taille du buffer.
 * @retval Nombre d'octets copiés, 0 si rien de nouveau, -1 si erreur.
 */
int esp01_get_new_data(uint8_t *buf, uint16_t bufsize)
{
    VALIDATE_PARAM(buf && bufsize > 0 && g_dma_rx_buf && g_dma_buf_size > 0, -1); // Vérifie la validité des paramètres et du contexte DMA

    uint16_t pos = g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx); // Calcule la position actuelle dans le buffer DMA circulaire

    int len = 0; // Initialise le nombre d'octets à copier à 0

    if (pos != g_rx_last_pos) // Si la position a changé depuis le dernier appel, il y a des nouveaux octets à lire
    {
        if (pos > g_rx_last_pos)                        // Cas normal (pas de retour en début de buffer)
            len = pos - g_rx_last_pos;                  // Calcule le nombre d'octets à lire
        else                                            // Cas de retour en début de buffer (DMA circulaire)
            len = g_dma_buf_size - g_rx_last_pos + pos; // Calcule le nombre d'octets à lire en deux parties

        if (len > bufsize) // Si le nombre d'octets à lire dépasse la taille du buffer utilisateur
            len = bufsize; // Limite la copie à la taille du buffer utilisateur

        for (int i = 0; i < len; i++) // Copie les nouveaux octets dans le buffer utilisateur
            buf[i] = g_dma_rx_buf[(g_rx_last_pos + i) % g_dma_buf_size];

        g_rx_last_pos = pos; // Met à jour la dernière position lue

        return len; // Retourne le nombre d'octets copiés
    }
    return 0; // Aucun nouvel octet à lire
}

/**
 * @brief  Wrapper pour vider le buffer RX (utilitaire interne).
 * @param  timeout_ms Délai maximal en millisecondes pour vider le buffer.
 * @retval Aucun
 */
void _flush_rx_buffer(uint32_t timeout_ms)
{
    esp01_flush_rx_buffer(timeout_ms); //   Vider le buffer RX de l'ESP01
}

// ========================= FIN DU MODULE =========================
