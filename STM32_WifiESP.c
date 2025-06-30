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

// ========================= FONCTIONS PRINCIPALES (initialisation, gestion du module, buffer, etc.) ========================= */

/**
 * @brief  Initialise le driver ESP01 (UART, DMA, debug, etc).
 * @param  huart_esp   Pointeur sur l'UART utilisée pour l'ESP01.
 * @param  huart_debug Pointeur sur l'UART de debug/console.
 * @param  dma_rx_buf  Buffer DMA pour la réception UART.
 * @param  dma_buf_size Taille du buffer DMA RX.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 *
 * @details
 * Cette fonction configure les UART, le buffer DMA et les variables globales nécessaires à la communication
 * avec le module ESP01. Elle initialise la réception DMA, vérifie la présence du module via une commande AT,
 * et prépare le port serveur par défaut. À appeler avant toute autre fonction du driver.
 *
 * @note
 * - Doit être appelée avant toute utilisation du driver.
 * - Le buffer DMA RX doit être alloué par l'utilisateur et rester valide pendant toute la durée d'utilisation.
 * - Le port serveur HTTP est initialisé à 80 par défaut.
 */
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
    // Vérifie la validité des pointeurs et de la taille du buffer
    VALIDATE_PARAM(esp01_is_valid_ptr(huart_esp) && esp01_is_valid_ptr(huart_debug) && esp01_is_valid_ptr(dma_rx_buf) && dma_buf_size > 0, ESP01_INVALID_PARAM);

    g_esp_uart = huart_esp;        // Initialise l'UART pour l'ESP01 (communication AT)
    g_debug_uart = huart_debug;    // Initialise l'UART pour le debug/console (logs, terminal)
    g_dma_rx_buf = dma_rx_buf;     // Affecte le buffer DMA pour la réception UART
    g_dma_buf_size = dma_buf_size; // Définit la taille du buffer DMA RX
    g_server_port = 80;            // Définit le port par défaut du serveur HTTP

    // Initialise la réception DMA pour l'ESP01
    if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK) // Si l'initialisation DMA échoue
    {
        ESP01_LOG_ERROR("INIT", "Erreur initialisation DMA RX : %s", esp01_get_error_string(ESP01_FAIL)); // Log l'erreur d'initialisation DMA
        ESP01_RETURN_ERROR("INIT", ESP01_NOT_INITIALIZED);                                                // Retourne une erreur d'initialisation
    }
    HAL_Delay(500); // Petit délai pour laisser l'ESP01 démarrer après reset ou power-on

    ESP01_Status_t status = esp01_test_at(); // Teste la communication avec l'ESP01 via la commande AT
    if (status != ESP01_OK)                  // Si le test AT échoue (module non détecté ou non fonctionnel)
    {
        ESP01_LOG_ERROR("INIT", "ESP01 non détecté !"); // Log l'erreur de détection
        ESP01_RETURN_ERROR("INIT", ESP01_NOT_DETECTED); // Retourne une erreur de détection
    }
    return ESP01_OK; // Retourne OK si l'initialisation réussit
}

// ========================= WRAPPERS AT & HELPERS ASSOCIÉS (par commande AT, cf. tableau header) =========================

// --- AT (test présence module) ---
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

// --- AT+RST (reset logiciel) ---
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

// --- AT+RESTORE (usine) ---
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

// --- AT+GMR (récupération version firmware) ---
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

// --- AT+UART? (récupération config UART) ---
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
    if (esp01_parse_string_after(resp, "+UART", out, out_size) == ESP01_OK || esp01_parse_string_after(resp, "+UART_CUR", out, out_size) == ESP01_OK)
    {                                                      // début if : parsing réussi
        ESP01_LOG_DEBUG("UART", "Config brute : %s", out); // Log la config brute
        return ESP01_OK;                                   // Succès
    } // fin if (parsing réussi)
    ESP01_RETURN_ERROR("UART", ESP01_FAIL); // Retourne une erreur si parsing échoue
}

/**
 * @brief  Convertit la configuration UART brute en chaîne lisible.
 * @param  raw_config Chaîne brute (ex: "115200,8,1,0,0").
 * @param  out       Buffer de sortie.
 * @param  out_size  Taille du buffer de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_uart_config_to_string(const char *raw_config, char *out, size_t out_size)
{
    VALIDATE_PARAM(raw_config && out && out_size > 0, ESP01_INVALID_PARAM);
    unsigned long baud = 0;
    int data = 0, stop = 0, parity = 0, flow = 0;
    if (sscanf(raw_config, "%lu,%d,%d,%d,%d", &baud, &data, &stop, &parity, &flow) != 5)
        return ESP01_FAIL;
    snprintf(out, out_size, "Baudrate: %lu, Data: %d, Stop: %d, Parité: %s, Flow: %s", baud, data, stop,
             (parity == 0 ? "Aucune" : (parity == 1 ? "Impair" : "Pair")),
             (flow == 0 ? "Aucun" : (flow == 1 ? "RTS" : (flow == 2 ? "CTS" : "RTS+CTS"))));
    return ESP01_OK;
}

// --- AT+UART= (configuration UART) ---
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

// --- AT+SLEEP? (récupération mode sommeil) ---
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
 * @brief  Convertit le mode sommeil en chaîne lisible.
 * @param  mode     Valeur du mode (0, 1, 2).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_sleep_mode_to_string(int mode, char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);
    const char *str = "?";
    switch (mode)
    {
    case 0:
        str = "Aucun (modem actif)";
        break;
    case 1:
        str = "Light sleep";
        break;
    case 2:
        str = "Deep sleep";
        break;
    }
    snprintf(out, out_size, "%s", str);
    return ESP01_OK;
}

// --- AT+RFPOWER? (récupération puissance RF) ---
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

// --- AT+RFPOWER= (définition puissance RF) ---
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

// --- AT+SYSLOG? (récupération niveau log système) ---
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
 * @brief  Convertit le niveau de log système en chaîne lisible.
 * @param  syslog   Niveau de log (0-4).
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_syslog_to_string(int syslog, char *out, size_t out_size) // Convertit le niveau de log système en chaîne lisible
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM); // Vérifie la validité du buffer de sortie et de sa taille
    const char *str = "?";                                    // Chaîne par défaut si le niveau n'est pas reconnu
    switch (syslog)                                           // Sélectionne la chaîne en fonction de la valeur de syslog
    {
    case 0:
        str = "Aucun"; // Aucun log
        break;
    case 1:
        str = "Erreur"; // Niveau erreur
        break;
    case 2:
        str = "Avertissement"; // Niveau avertissement
        break;
    case 3:
        str = "Info"; // Niveau info
        break;
    case 4:
        str = "Debug"; // Niveau debug
        break;
    }
    snprintf(out, out_size, "%s", str); // Copie la chaîne sélectionnée dans le buffer de sortie
    return ESP01_OK;                    // Retourne OK
}

// --- AT+SYSRAM? (récupération RAM système) ---
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
 * @brief  Formate la RAM système en chaîne lisible.
 * @param  free_ram RAM libre.
 * @param  min_ram  RAM minimale historique.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_sysram_to_string(uint32_t free_ram, uint32_t min_ram, char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);
    snprintf(out, out_size, "RAM libre: %lu o, min: %lu o", (unsigned long)free_ram, (unsigned long)min_ram);
    return ESP01_OK;
}

// --- AT+SYSSTORE? (récupération taille stockage système) ---
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
 * @brief  Formate le mode de stockage système en chaîne lisible.
 * @param  sysstore Valeur du mode de stockage.
 * @param  out      Buffer de sortie.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_sysstore_to_string(uint32_t sysstore, char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);
    const char *str = "?";
    switch (sysstore)
    {
    case 0:
        str = "RAM";
        break;
    case 1:
        str = "Flash";
        break;
    }
    snprintf(out, out_size, "%s", str);
    return ESP01_OK;
}

// --- AT+USERRAM? (récupération taille RAM utilisateur) ---
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
 * @brief  Formate la RAM utilisateur en chaîne lisible.
 * @param  userram Taille de la RAM utilisateur.
 * @param  out     Buffer de sortie.
 * @param  out_size Taille du buffer de sortie.
 * @retval ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_userram_to_string(uint32_t userram, char *out, size_t out_size)
{
    VALIDATE_PARAM(out && out_size > 0, ESP01_INVALID_PARAM);
    snprintf(out, out_size, "RAM utilisateur: %lu o", (unsigned long)userram);
    return ESP01_OK;
}

// --- AT+GSLP (deep sleep) ---
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

// --- AT+SYSFLASH? (récupération réponse brute AT+SYSFLASH?) ---
/**
 * @brief Récupère la réponse brute de la commande AT+SYSFLASH? du module ESP01.
 * @param out Buffer de sortie pour la réponse complète (chaîne multi-lignes).
 * @param out_size Taille du buffer de sortie.
 * @retval ESP01_Status_t Statut de la lecture (OK, erreur, timeout, overflow, etc.).
 *
 * Cette fonction envoie la commande AT+SYSFLASH? au module ESP01 et copie la réponse
 * brute dans le buffer fourni. À utiliser pour obtenir la liste des partitions flash.
 */
ESP01_Status_t esp01_get_sysflash(char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size > 0, ESP01_INVALID_PARAM);
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+SYSFLASH?", out, out_size, "OK", ESP01_TIMEOUT_SHORT);
    return st;
}

/**
 * @brief Affiche les partitions système de l'ESP01 à partir de la réponse brute AT+SYSFLASH?.
 * @param sysflash_resp Réponse brute de la commande AT+SYSFLASH?.
 * @retval uint8_t Nombre de partitions affichées.
 *
 * Cette fonction analyse la réponse brute de la commande AT+SYSFLASH? et affiche
 * les informations sur chaque partition détectée (nom, type, sous-type, adresse, taille).
 * Retourne le nombre de partitions trouvées.
 */
uint8_t esp01_display_sysflash_partitions(const char *sysflash_resp)
{
    if (!sysflash_resp || !*sysflash_resp)                                                            // Vérifie si la réponse est valide
        return 0;                                                                                     // Retourne 0 si la réponse est vide
    uint8_t count = 0;                                                                                // Compteur de partitions trouvées
    const char *p = sysflash_resp;                                                                    // Pointeur pour parcourir la réponse
    while ((p = strstr(p, "+SYSFLASH:")) != NULL)                                                     // Cherche chaque ligne commençant par "+SYSFLASH:"
    {                                                                                                 // début while : parcours la réponse
        char name[ESP01_SMALL_BUF_SIZE] = {0};                                                        // Buffer pour le nom de la partition
        int type = 0, subtype = 0;                                                                    // Type et sous-type de la partition
        unsigned int addr = 0, size = 0;                                                              // Adresse et taille de la partition
        int n = sscanf(p, "+SYSFLASH:\"%31[^\"]\",%d,%d,%x,%x", name, &type, &subtype, &addr, &size); // Analyse de la ligne
        if (n == 5)                                                                                   // Si tous les champs sont trouvés
        {                                                                                             // début if : tous les champs trouvés
            printf("[ESP01][SYSFLASH] Partition: %s | type: %d | subtype: %d | addr: 0x%X | size: 0x%X\r\n",
                   name, type, subtype, addr, size); // Affiche les informations de la partition
            count++;                                 // Incrémente le compteur de partitions
        } // fin if (tous les champs trouvés)
        // Avance à la ligne suivante
        const char *next = strchr(p, '\n'); // Recherche la prochaine ligne
        if (!next)                          // Si pas de prochaine ligne, on sort de la boucle
            break;                          // Fin de la boucle si pas de saut de ligne
        p = next + 1;                       // Avance au caractère suivant après le saut de ligne
    } // fin while (parcours la réponse)
    if (count == 0)                                                // Si aucune partition n'a été trouvée
        printf("[ESP01][SYSFLASH] Aucune partition détectée\r\n"); // Affiche un message
    return count;                                                  // Retourne le nombre de partitions trouvées
}

// ========================= WRAPPERS API (COMMANDES AT HAUT NIVEAU) =========================

ESP01_Status_t esp01_get_cmd_list(char *out, size_t out_size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(out) && out_size >= ESP01_LARGE_RESP_BUF, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres

    size_t total_len = 0;           // Longueur totale de la réponse accumulée
    uint32_t start = HAL_GetTick(); // Temps de début pour le timeout
    int found_ok = 0;               // Indicateur pour savoir si "OK" a été trouvé
    char line[ESP01_MAX_RESP_BUF];  // Tampon pour une ligne de réponse
    size_t line_len = 0;            // Longueur de la ligne courante

    esp01_flush_rx_buffer(100);                                                // Vide le buffer RX avant d'envoyer la commande
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

// ========================= AUTRES SECTIONS (Parsing, Utilitaires, Terminal, etc.) =========================

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

// ========================= OUTILS DE PARSING =========================

/**
 * @brief       Parses an integer value that appears after a specified pattern in a given text.
 * @param[in]   text      The input string to search within.
 * @param[in]   pattern   The pattern to search for in the input string.
 * @param[out]  result    Pointer to store the parsed integer value if found.
 * @retval      ESP01_OK if the integer is successfully parsed after the pattern.
 * @retval      ESP01_ERROR if the pattern is not found or the integer cannot be parsed.
 */
ESP01_Status_t esp01_parse_int_after(const char *text, const char *pattern, int32_t *result)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(text) && esp01_is_valid_ptr(pattern) && esp01_is_valid_ptr(result), ESP01_INVALID_PARAM); // Vérifie la validité des pointeurs d'entrée
    char *ptr = strstr(text, pattern);                                                                                          // Cherche le motif 'pattern' dans la chaîne 'text'
    if (!ptr)                                                                                                                   // Si le motif n'est pas trouvé
        ESP01_RETURN_ERROR("PARSE_INT", ESP01_FAIL);                                                                            // Retourne une erreur
    ptr = strchr(ptr, ':');                                                                                                     // Cherche le caractère ':' après le motif
    if (!ptr)                                                                                                                   // Si ':' n'est pas trouvé
        ESP01_RETURN_ERROR("PARSE_INT", ESP01_FAIL);                                                                            // Retourne une erreur
    *result = (int32_t)strtol(ptr + 1, NULL, 10);                                                                               // Convertit la sous-chaîne après ':' en entier et stocke dans 'result'
    return ESP01_OK;                                                                                                            // Succès
}

/**
 * @brief      Analyse la sous-chaîne qui apparaît après un motif spécifié dans le texte donné.
 * @param[in]  text     Chaîne d'entrée à analyser.
 * @param[in]  pattern  Motif à rechercher dans la chaîne d'entrée.
 * @param[out] output   Buffer où sera stocké le résultat.
 * @param[in]  size     Taille du buffer de sortie.
 * @retval     ESP01_OK si le motif est trouvé et la sous-chaîne copiée avec succès.
 * @retval     ESP01_FAIL si le motif n'est pas trouvé ou en cas d'erreur.
 */
ESP01_Status_t esp01_parse_string_after(const char *text, const char *pattern, char *output, size_t size)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(text) && esp01_is_valid_ptr(pattern) && esp01_is_valid_ptr(output) && size > 0, ESP01_INVALID_PARAM); // Vérifie la validité des paramètres
    char *start = strstr(text, pattern);                                                                                                    // Cherche le motif dans la chaîne d'entrée
    if (!start)                                                                                                                             // Si le motif n'est pas trouvé
        ESP01_RETURN_ERROR("PARSE_STR", ESP01_FAIL);                                                                                        // Retourne une erreur
    start = strchr(start, ':');                                                                                                             // Cherche le caractère ':' après le motif
    start++;                                                                                                                                // Passe le caractère ':'
    size_t len = 0;                                                                                                                         // Initialise la longueur à 0
    while (start[len] && start[len] != '\r' && start[len] != '\n' && len < size - 1)                                                        // Parcourt la sous-chaîne jusqu'à fin de ligne ou taille max
        len++;                                                                                                                              // Incrémente la longueur
    if (esp01_check_buffer_size(len, size - 1) != ESP01_OK)                                                                                 // Vérifie que la taille ne dépasse pas le buffer
        ESP01_RETURN_ERROR("PARSE_STR", ESP01_BUFFER_OVERFLOW);                                                                             // Retourne une erreur de débordement
    if (len < size)                                                                                                                         // Si la longueur est correcte
    {
        memcpy(output, start, len); // Copie la sous-chaîne dans le buffer de sortie
        output[len] = '\0';         // Termine la chaîne
        esp01_trim_string(output);  // Supprime les espaces en début/fin
    }
    else
    {
        output[0] = '\0'; // Si erreur, chaîne vide
    }
    return ESP01_OK; // Succès
}

/**
 * @brief Extracts a quoted value from a source string following a specific motif.
 *
 * This function searches for a given motif in the source string, then extracts the value
 * enclosed in double quotes (") that immediately follows the motif. The extracted value
 * is copied into the output buffer, ensuring it does not exceed the specified length.
 *
 * @param src      The source string to search in.
 * @param motif    The motif to search for before the quoted value.
 * @param out      The buffer where the extracted value will be stored.
 * @param out_len  The length of the output buffer.
 * @return true if a quoted value was successfully extracted, false otherwise.
 */
bool esp01_extract_quoted_value(const char *src, const char *motif, char *out, size_t out_len)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(src) && esp01_is_valid_ptr(motif) && esp01_is_valid_ptr(out) && out_len > 0, false); // Vérifie la validité des paramètres
    const char *p = strstr(src, motif);                                                                                    // Cherche le motif dans la chaîne source
    if (!p)                                                                                                                // Si le motif n'est pas trouvé
        return false;                                                                                                      // Retourne false
    p += strlen(motif);                                                                                                    // Avance après le motif
    const char *q = strchr(p, '"');                                                                                        // Cherche le premier guillemet ouvrant
    if (!q)                                                                                                                // Si pas de guillemet ouvrant trouvé
        return false;                                                                                                      // Retourne false
    p = q + 1;                                                                                                             // Avance après le guillemet ouvrant
    q = strchr(p, '"');                                                                                                    // Cherche le guillemet fermant
    size_t len = q ? (size_t)(q - p) : 0;                                                                                  // Calcule la longueur de la valeur extraite
    if (!q || esp01_check_buffer_size(len, out_len - 1) != ESP01_OK)                                                       // Si pas de guillemet fermant ou débordement de buffer
        return false;                                                                                                      // Retourne false
    if (len < out_len)                                                                                                     // Si la longueur est correcte
    {
        memcpy(out, p, len);    // Copie la valeur extraite dans le buffer de sortie
        out[len] = 0;           // Termine la chaîne
        esp01_trim_string(out); // Supprime les espaces en début/fin
    }
    else
    {
        out[0] = 0; // Si erreur, chaîne vide
    }
    return true; // Extraction réussie
}

/**
 * @brief      Parse a boolean value from a response string after a specific tag.
 * @param[in]  resp  The response string to search within.
 * @param[in]  tag   The tag after which the boolean value is expected.
 * @param[out] out   Pointer to a bool where the parsed value will be stored.
 * @retval     ESP01_Status_t Returns ESP01_OK if parsing was successful, otherwise an error code.
 *
 * This function searches for the specified tag in the response string, then attempts to parse
 * a boolean value (typically "0" or "1", or "true"/"false") immediately following the tag.
 * The result is stored in the variable pointed to by 'out'.
 */
ESP01_Status_t esp01_parse_bool_after(const char *resp, const char *tag, bool *out)
{
    VALIDATE_PARAM(esp01_is_valid_ptr(resp) && esp01_is_valid_ptr(tag) && esp01_is_valid_ptr(out), ESP01_INVALID_PARAM); // Vérifie la validité des pointeurs d'entrée
    const char *ptr = strstr(resp, tag);                                                                                 // Cherche le tag dans la chaîne de réponse
    if (!ptr)                                                                                                            // Si le tag n'est pas trouvé
        return ESP01_FAIL;                                                                                               // Retourne une erreur
    ptr += strlen(tag);                                                                                                  // Avance le pointeur après le tag
    while (*ptr == ' ' || *ptr == ':' || *ptr == '=')                                                                    // Ignore les espaces, ':' ou '=' après le tag
        ptr++;                                                                                                           // Avance le pointeur
    if (strncmp(ptr, "true", 4) == 0)                                                                                    // Si la valeur est "true"
    {
        *out = true;     // Affecte true à la sortie
        return ESP01_OK; // Succès
    }
    if (strncmp(ptr, "false", 5) == 0) // Si la valeur est "false"
    {
        *out = false;    // Affecte false à la sortie
        return ESP01_OK; // Succès
    }
    if (*ptr == '1') // Si la valeur est '1'
    {
        *out = true;     // Affecte true à la sortie
        return ESP01_OK; // Succès
    }
    if (*ptr == '0') // Si la valeur est '0'
    {
        *out = false;    // Affecte false à la sortie
        return ESP01_OK; // Succès
    }
    return ESP01_FAIL; // Si aucune valeur reconnue, retourne une erreur
}

/**
 * @brief  Sépare une chaîne d'entrée en lignes, en utilisant "\r\n" comme délimiteur.
 * @param  input_str Chaîne d'entrée à séparer.
 * @param  lines     Tableau de pointeurs pour stocker les lignes séparées.
 * @param  max_lines Nombre maximum de lignes à extraire.
 * @param  lines_buffer Buffer pour stocker les lignes extraites.
 * @param  buffer_size Taille du buffer de lignes.
 * @param  skip_empty Si true, ignore les lignes vides.
 * @retval Nombre de lignes extraites.
 */
uint8_t esp01_split_response_lines(const char *input_str, char *lines[], uint8_t max_lines, char *lines_buffer, size_t buffer_size, bool skip_empty)
{
    if (!input_str || !*input_str || max_lines == 0 || !lines_buffer || buffer_size == 0) // Vérifie la validité des paramètres
        return 0;                                                                         // Retourne 0 si paramètres invalides

    size_t total_copied = 0;            // Compteur du nombre total d'octets copiés dans lines_buffer
    uint8_t line_count = 0;             // Compteur du nombre de lignes extraites
    const char *line_start = input_str; // Pointeur sur le début de la ligne courante

    while (*line_start && line_count < max_lines) // Boucle tant qu'il reste des caractères et qu'on n'a pas atteint max_lines
    {
        const char *line_end = strstr(line_start, "\r\n"); // Cherche la fin de la ligne ("\r\n")
        if (!line_end)                                     // Si pas de "\r\n" trouvé
            line_end = line_start + strlen(line_start);    // Utilise la fin de la chaîne comme fin de ligne

        size_t line_length = line_end - line_start; // Calcule la longueur de la ligne courante

        if (line_length > 0 && line_length < buffer_size) // Si la ligne n'est pas vide et tient dans le buffer
        {
            memcpy(lines_buffer + total_copied, line_start, line_length); // Copie la ligne dans lines_buffer
            lines_buffer[total_copied + line_length] = '\0';              // Termine la ligne par un '\0'
            lines[line_count++] = lines_buffer + total_copied;            // Stocke le pointeur vers la ligne extraite
            total_copied += line_length + 1;                              // Met à jour le compteur d'octets copiés
        }
        line_start = line_end + 2; // Passe au début de la prochaine ligne (après "\r\n")
    }

    if (line_count == max_lines) // Si on a extrait le nombre maximum de lignes
    {
        lines[line_count] = NULL; // Termine le tableau de pointeurs par NULL
        total_copied += 1;        // Incrémente total_copied (optionnel ici)
    }
    return line_count; // Retourne le nombre de lignes extraites
}

// ========================= OUTILS UTILITAIRES (BUFFER, INLINE, VALIDATION) =========================

/**
 * @brief  Retourne une chaîne descriptive pour un code d'erreur ESP01_Status_t.
 * @param  status Code d'erreur ESP01_Status_t.
 * @retval Chaîne descriptive de l'erreur.
 */
const char *esp01_get_error_string(ESP01_Status_t status) // Fonction qui retourne une chaîne descriptive pour un code d'erreur ESP01_Status_t
{
    switch (status) // Sélectionne le code d'erreur
    {
    case ESP01_OK:
        return "OK"; // Succès
    case ESP01_FAIL:
        return "Erreur"; // Erreur générique
    case ESP01_TIMEOUT:
        return "Timeout"; // Délai dépassé
    case ESP01_NOT_INITIALIZED:
        return "Non initialisé"; // Driver non initialisé
    case ESP01_INVALID_PARAM:
        return "Paramètre invalide"; // Paramètre incorrect
    case ESP01_BUFFER_OVERFLOW:
        return "Débordement buffer"; // Dépassement de buffer
    case ESP01_UNEXPECTED_RESPONSE:
        return "Réponse inattendue"; // Réponse inattendue du module
    case ESP01_NOT_DETECTED:
        return "Module non détecté"; // Module ESP01 non détecté
    case ESP01_CMD_TOO_LONG:
        return "Commande trop longue"; // Commande AT trop longue
    case ESP01_MEMORY_ERROR:
        return "Erreur mémoire"; // Erreur d'allocation mémoire
    case ESP01_EXIT:
        return "Sortie"; // Sortie ou interruption
    case ESP01_NOT_CONNECTED:
        return "Non connecté"; // Non connecté au réseau
    case ESP01_ALREADY_CONNECTED:
        return "Déjà connecté"; // Déjà connecté
    case ESP01_CONNECTION_ERROR:
        return "Erreur connexion"; // Erreur de connexion
    case ESP01_ROUTE_NOT_FOUND:
        return "Route non trouvée"; // Route réseau non trouvée
    case ESP01_PARSE_ERROR:
        return "Erreur de parsing"; // Erreur d'analyse de réponse
    case ESP01_WIFI_NOT_CONNECTED:
        return "WiFi non connecté"; // Non connecté au WiFi
    case ESP01_WIFI_TIMEOUT:
        return "Timeout WiFi"; // Timeout lors de la connexion WiFi
    case ESP01_WIFI_WRONG_PASSWORD:
        return "Mot de passe WiFi incorrect"; // Mot de passe WiFi erroné
    case ESP01_WIFI_AP_NOT_FOUND:
        return "Point d'accès introuvable"; // Point d'accès WiFi non trouvé
    case ESP01_WIFI_CONNECT_FAIL:
        return "Échec connexion WiFi"; // Échec de connexion WiFi
    case ESP01_HTTP_PARSE_ERROR:
        return "Erreur parsing HTTP"; // Erreur d'analyse HTTP
    case ESP01_HTTP_INVALID_REQUEST:
        return "Requête HTTP invalide"; // Requête HTTP invalide
    case ESP01_HTTP_TIMEOUT:
        return "Timeout HTTP"; // Timeout HTTP
    case ESP01_HTTP_CONNECTION_REFUSED:
        return "Connexion HTTP refusée"; // Connexion HTTP refusée
    case ESP01_MQTT_NOT_CONNECTED:
        return "MQTT non connecté"; // Non connecté au serveur MQTT
    case ESP01_MQTT_PROTOCOL_ERROR:
        return "Erreur protocole MQTT"; // Erreur de protocole MQTT
    case ESP01_MQTT_SUBSCRIPTION_FAILED:
        return "Échec abonnement MQTT"; // Échec d'abonnement MQTT
    case ESP01_MQTT_PUBLISH_FAILED:
        return "Échec publication MQTT"; // Échec de publication MQTT
    case ESP01_NTP_SYNC_ERROR:
        return "Erreur synchronisation NTP"; // Erreur de synchronisation NTP
    case ESP01_NTP_INVALID_RESPONSE:
        return "Réponse NTP invalide"; // Réponse NTP invalide
    case ESP01_NTP_SERVER_NOT_REACHABLE:
        return "Serveur NTP inaccessible"; // Serveur NTP inaccessible
    default:
        return "Code inconnu"; // Code d'erreur inconnu
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
        uint8_t buf[ESP01_SMALL_BUF_SIZE];              // Buffer temporaire pour lecture
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
 * @brief  Supprime les espaces en début et fin de chaîne.
 * @param  str Pointeur sur la chaîne à traiter.
 * @retval Aucun
 * @note   Modifie la chaîne en place.
 */
void esp01_trim_string(char *str) // Fonction pour supprimer les espaces en début et fin de chaîne
{
    if (!str)   // Vérifie si le pointeur est nul
        return; // Si oui, ne fait rien
    // Trim début
    char *start = str;                          // Pointeur sur le début de la chaîne
    while (isspace((unsigned char)*start))      // Boucle tant que le caractère courant est un espace
        start++;                                // Avance le pointeur pour sauter les espaces initiaux
    if (start != str)                           // Si on a sauté des espaces
        memmove(str, start, strlen(start) + 1); // Décale la chaîne pour supprimer les espaces en début
    // Trim fin
    size_t len = strlen(str);                               // Calcule la longueur de la chaîne
    while (len > 0 && isspace((unsigned char)str[len - 1])) // Boucle tant que le dernier caractère est un espace
        str[--len] = '\0';                                  // Remplace l'espace par un caractère de fin de chaîne
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
