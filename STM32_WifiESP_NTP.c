/**
 ******************************************************************************
 * @file    STM32_WifiESP_NTP.c
 * @author  Weedman
 * @version 1.2.0
 * @date    18 juin 2025
 * @brief   Implémentation des fonctions haut niveau NTP pour ESP01
 *
 * @details
 * Ce fichier source contient l’implémentation des fonctions NTP haut niveau :
 * - Configuration et synchronisation NTP (one shot ou périodique)
 * - Parsing et affichage de la date/heure NTP
 * - Accès à la configuration et à la dernière date reçue
 *
 * @note
 * - Nécessite le driver bas niveau STM32_WifiESP.h et le module WiFi STM32_WifiESP_WIFI.h
 ******************************************************************************
 */

#include "STM32_WifiESP_NTP.h" // Inclusion du header du module NTP
#include <stdio.h>             // Pour printf, snprintf, etc.
#include <string.h>            // Pour manipulation de chaînes
#include <ctype.h>             // Pour fonctions de classification de caractères

/* ==================== VARIABLES STATIQUES ==================== */

/**
 * @brief Configuration NTP courante (locale)
 */
static esp01_ntp_config_t g_ntp_config = {
    // Structure de configuration NTP locale
    .server = ESP01_NTP_DEFAULT_SERVER,    // Serveur NTP par défaut
    .timezone = 0,                         // Fuseau horaire par défaut
    .period_s = ESP01_NTP_DEFAULT_PERIOD_S // Période de synchronisation par défaut
};

static char g_last_datetime[ESP01_NTP_DATETIME_BUF_SIZE] = ""; // Dernière date/heure NTP reçue (brute)
static uint8_t g_ntp_updated = 0;                              // Flag indiquant si une nouvelle date NTP a été reçue

/* ==================== FONCTIONS PRINCIPALES ==================== */

/**
 * @brief Configure la structure NTP locale (ne modifie pas le module ESP01).
 * @param ntp_server   Nom du serveur NTP.
 * @param timezone     Décalage horaire.
 * @param sync_period_s Période de synchro en secondes.
 * @return ESP01_OK si succès, ESP01_INVALID_PARAM sinon.
 */
ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone, int sync_period_s)
{
    ESP01_LOG_INFO("NTP", "Configuration NTP : serveur=%s, timezone=%d, period=%d", ntp_server, timezone, sync_period_s); // Log de la configuration demandée
    VALIDATE_PARAM(ntp_server && strlen(ntp_server) < ESP01_NTP_MAX_SERVER_LEN, ESP01_INVALID_PARAM);                     // Validation des paramètres
    if (esp01_check_buffer_size(strlen(ntp_server), ESP01_NTP_MAX_SERVER_LEN - 1) != ESP01_OK)                            // Vérification de la taille du buffer
    {
        ESP01_LOG_ERROR("NTP", "Dépassement de la taille du buffer serveur NTP"); // Log d'erreur si dépassement
        ESP01_RETURN_ERROR("NTP_CONFIG", ESP01_BUFFER_OVERFLOW);                  // Retourne une erreur de buffer
    }
    strncpy(g_ntp_config.server, ntp_server, ESP01_NTP_MAX_SERVER_LEN - 1); // Copie du nom du serveur NTP
    g_ntp_config.server[ESP01_NTP_MAX_SERVER_LEN - 1] = '\0';               // Ajout du caractère de fin de chaîne
    g_ntp_config.timezone = timezone;                                       // Affectation du fuseau horaire
    g_ntp_config.period_s = sync_period_s;                                  // Affectation de la période de synchronisation
    ESP01_LOG_INFO("NTP", "Configuration NTP appliquée");                   // Log de succès
    return ESP01_OK;                                                        // Retourne OK
}

/**
 * @brief Lance la synchronisation NTP (one shot ou périodique selon periodic).
 * @param periodic true = périodique, false = one shot.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_ntp_start_sync(bool periodic)
{
    ESP01_LOG_INFO("NTP", "Démarrage de la synchronisation NTP (periodic=%d)", periodic);
    ESP01_Status_t st = esp01_apply_ntp_config(
        1,
        g_ntp_config.timezone,
        g_ntp_config.server,
        g_ntp_config.period_s);

    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Echec de l'application de la configuration NTP (code=%d)", st);
        ESP01_RETURN_ERROR("NTP_START_SYNC", st);
    }

    if (!periodic)
    {
        HAL_Delay(2000);

        char buf[ESP01_NTP_DATETIME_BUF_SIZE];
        for (int i = 0; i < 3; ++i)
        {
            st = esp01_get_ntp_time(buf, sizeof(buf));
            if (st == ESP01_OK && strlen(buf) > 0)
            {
                // Vérification de la validité de la date NTP (évite 1970)
                ntp_datetime_fr_t dt;
                if (esp01_parse_ntp_esp01(buf, &dt) == ESP01_OK && dt.year > 1970)
                {
                    if (esp01_check_buffer_size(strlen(buf), ESP01_NTP_DATETIME_BUF_SIZE - 1) != ESP01_OK)
                    {
                        ESP01_LOG_ERROR("NTP", "Dépassement de la taille du buffer datetime NTP");
                        ESP01_RETURN_ERROR("NTP_START_SYNC", ESP01_BUFFER_OVERFLOW);
                    }
                    strncpy(g_last_datetime, buf, ESP01_NTP_DATETIME_BUF_SIZE - 1);
                    g_last_datetime[ESP01_NTP_DATETIME_BUF_SIZE - 1] = '\0';
                    g_ntp_updated = 1;
                    ESP01_LOG_INFO("NTP", "Synchronisation NTP réussie : %s", g_last_datetime);
                    return ESP01_OK;
                }
                else
                {
                    ESP01_LOG_ERROR("NTP", "Synchronisation NTP échouée ou date invalide : %s", buf);
                }
            }
            else
            {
                ESP01_LOG_WARN("NTP", "Tentative %d de récupération de l'heure NTP échouée", i + 1);
            }
            HAL_Delay(1000);
        }
        ESP01_LOG_ERROR("NTP", "Impossible de récupérer une date NTP valide après 3 tentatives");
        ESP01_RETURN_ERROR("NTP_START_SYNC", ESP01_FAIL);
    }
    ESP01_LOG_INFO("NTP", "Synchronisation NTP périodique activée");
    return ESP01_OK;
}

/**
 * @brief Applique la configuration NTP au module ESP01 (commande AT).
 * @param enable      1 pour activer NTP, 0 pour désactiver.
 * @param timezone    Décalage horaire.
 * @param server      Nom du serveur NTP.
 * @param interval_s  Période de synchronisation (s).
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_apply_ntp_config(uint8_t enable, int timezone, const char *server, int interval_s)
{
    char cmd[ESP01_MAX_CMD_BUF];   // Buffer pour la commande AT
    char resp[ESP01_MAX_RESP_BUF]; // Buffer pour la réponse
    ESP01_Status_t st;             // Variable de statut

    ESP01_LOG_INFO("NTP", "Application de la configuration NTP : enable=%d, timezone=%d, server=%s", enable, timezone, server); // Log de la configuration
    snprintf(cmd, sizeof(cmd), "AT+CIPSNTPCFG=%d,%d,\"%s\"", enable, timezone, server);                                         // Construction de la commande AT
    st = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, resp, sizeof(resp));                                           // Envoi de la commande et attente de la réponse
    if (st != ESP01_OK)                                                                                                         // Vérification du succès
    {
        ESP01_LOG_ERROR("NTP", "Echec de la commande AT+CIPSNTPCFG (code=%d)", st); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_APPLY_CONFIG", st);                                 // Retourne l'erreur
    }
    ESP01_LOG_INFO("NTP", "Configuration NTP envoyée avec succès"); // Log de succès
    return st;                                                      // Retourne le statut
}

/**
 * @brief  Récupère la date/heure NTP depuis le module ESP01 (commande AT).
 * @param  datetime_buf Buffer de sortie.
 * @param  bufsize      Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize)
{
    ESP01_LOG_INFO("NTP", "Récupération de l'heure NTP...");          // Log de récupération
    VALIDATE_PARAM(datetime_buf && bufsize > 0, ESP01_INVALID_PARAM); // Validation des paramètres

    if (esp01_check_buffer_size(24, bufsize - 1) != ESP01_OK) // Vérification de la taille du buffer
    {
        ESP01_LOG_ERROR("NTP", "Buffer trop petit pour la date NTP"); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_GET_TIME", ESP01_BUFFER_OVERFLOW);    // Retourne l'erreur
    }

    char cmd[] = "AT+CIPSNTPTIME?";          // Commande AT pour obtenir l'heure NTP
    char response[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse

    ESP01_Status_t st = esp01_send_at_with_resp(cmd, "OK", ESP01_TIMEOUT_SHORT, response, sizeof(response)); // Envoi de la commande et attente de la réponse
    if (st != ESP01_OK)                                                                                      // Vérification du succès
    {
        ESP01_LOG_ERROR("NTP", "Echec de la commande AT+CIPSNTPTIME? (code=%d)", st); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_GET_TIME", st);                                       // Retourne l'erreur
    }

    ESP01_LOG_DEBUG("NTP", "Réponse brute ESP01 :\n%s", response); // Log de la réponse brute

    st = esp01_parse_string_after(response, "+CIPSNTPTIME:", datetime_buf, bufsize); // Extraction de la date/heure de la réponse
    if (st != ESP01_OK)                                                              // Vérification du parsing
    {
        ESP01_LOG_ERROR("NTP", "Impossible d'extraire la date NTP de la réponse ESP01"); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_GET_TIME", st);                                          // Retourne l'erreur
    }

    char *ptr = datetime_buf;                           // Pointeur pour nettoyage
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\"') // Suppression des espaces et guillemets en début de chaîne
        ptr++;
    if (ptr != datetime_buf)                         // Si nettoyage nécessaire
        memmove(datetime_buf, ptr, strlen(ptr) + 1); // Décalage de la chaîne

    if (strlen(datetime_buf) < 8) // Vérification de la longueur minimale
    {
        ESP01_LOG_ERROR("NTP", "Date NTP trop courte ou invalide : %s", datetime_buf); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_GET_TIME", ESP01_FAIL);                                // Retourne l'échec
    }

    ESP01_LOG_INFO("NTP", "Heure NTP récupérée : %s", datetime_buf); // Log de succès
    return ESP01_OK;                                                 // Retourne OK
}

/* ==================== ACCÈS ET AFFICHAGE ==================== */

/**
 * @brief Affiche la configuration NTP courante (logs).
 */
void esp01_print_ntp_config(void)
{
    ESP01_LOG_INFO("NTP", "Affichage de la configuration NTP");                  // Log d'affichage
    ESP01_LOG_INFO("NTP", "Serveur : %s", g_ntp_config.server);                  // Affichage du serveur
    ESP01_LOG_INFO("NTP", "Fuseau horaire : %d", g_ntp_config.timezone);         // Affichage du fuseau horaire
    ESP01_LOG_INFO("NTP", "Période de synchro (s) : %d", g_ntp_config.period_s); // Affichage de la période
}

/**
 * @brief Affiche la dernière date/heure NTP reçue en français (logs).
 */
void esp01_ntp_print_last_datetime_fr(void)
{
    ESP01_LOG_INFO("NTP", "Affichage de la dernière date/heure NTP (FR)"); // Log d'affichage
    const char *datetime_ntp = esp01_ntp_get_last_datetime();              // Récupération de la dernière date/heure
    ntp_datetime_fr_t dt;                                                  // Structure pour le parsing
    if (!datetime_ntp || strlen(datetime_ntp) == 0)                        // Vérification de la disponibilité
    {
        ESP01_LOG_WARN("NTP", "Date NTP non disponible"); // Log d'avertissement
        return;                                           // Sortie
    }
    if (esp01_parse_ntp_esp01(datetime_ntp, &dt) == ESP01_OK) // Parsing de la date/heure
        esp01_print_datetime_fr(&dt);                         // Affichage en français
    else
        ESP01_LOG_WARN("NTP", "Date NTP invalide"); // Log d'avertissement
}

/**
 * @brief Affiche la dernière date/heure NTP reçue en anglais (logs).
 */
void esp01_ntp_print_last_datetime_en(void)
{
    ESP01_LOG_INFO("NTP", "Affichage de la dernière date/heure NTP (EN)"); // Log d'affichage
    const char *datetime_ntp = esp01_ntp_get_last_datetime();              // Récupération de la dernière date/heure
    ntp_datetime_fr_t dt;                                                  // Structure pour le parsing
    if (!datetime_ntp || strlen(datetime_ntp) == 0)                        // Vérification de la disponibilité
    {
        ESP01_LOG_WARN("NTP", "NTP date not available"); // Log d'avertissement
        return;                                          // Sortie
    }
    if (esp01_parse_ntp_esp01(datetime_ntp, &dt) == ESP01_OK) // Parsing de la date/heure
        esp01_print_datetime_en(&dt);                         // Affichage en anglais
    else
        ESP01_LOG_WARN("NTP", "Invalid NTP date"); // Log d'avertissement
}

/**
 * @brief Retourne la dernière date/heure NTP brute reçue.
 * @return Pointeur sur la chaîne date/heure.
 */
const char *esp01_ntp_get_last_datetime(void)
{
    ESP01_LOG_DEBUG("NTP", "Lecture de la dernière date/heure NTP : %s", g_last_datetime); // Log de lecture
    return g_last_datetime;                                                                // Retourne la dernière date/heure
}

/**
 * @brief Indique si une nouvelle date NTP a été reçue.
 * @return 1 si oui, 0 sinon.
 */
uint8_t esp01_ntp_is_updated(void)
{
    ESP01_LOG_DEBUG("NTP", "Flag de mise à jour NTP : %d", g_ntp_updated); // Log du flag
    return g_ntp_updated;                                                  // Retourne le flag
}

/**
 * @brief Efface le flag "date NTP reçue".
 */
void esp01_ntp_clear_updated_flag(void)
{
    ESP01_LOG_INFO("NTP", "Réinitialisation du flag de mise à jour NTP"); // Log de réinitialisation
    g_ntp_updated = 0;                                                    // Réinitialisation du flag
}

/**
 * @brief Retourne la configuration NTP courante.
 * @return Pointeur sur la structure de config.
 */
const esp01_ntp_config_t *esp01_get_ntp_config(void)
{
    ESP01_LOG_DEBUG("NTP", "Lecture de la configuration NTP"); // Log de lecture
    return &g_ntp_config;                                      // Retourne la configuration
}

/* ==================== PARSING ET UTILITAIRES ==================== */

/**
 * @brief Parse une date brute ESP01 en structure française.
 * @param datetime_ntp Chaîne brute.
 * @param dt Structure de sortie.
 * @return ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_parse_ntp_esp01(const char *datetime_ntp, ntp_datetime_fr_t *dt)
{
    ESP01_LOG_DEBUG("NTP", "Parsing de la date NTP : %s", datetime_ntp); // Log du parsing
    VALIDATE_PARAM(datetime_ntp && dt, ESP01_INVALID_PARAM);             // Validation des paramètres

    char mois[8], jour[8]; // Buffers pour le parsing
    int res = sscanf(datetime_ntp, "%s %s %d %d:%d:%d %d",
                     jour, mois, &dt->day, &dt->hour, &dt->min, &dt->sec, &dt->year); // Parsing de la chaîne
    if (res != 7)                                                                     // Vérification du succès du parsing
    {
        ESP01_LOG_ERROR("NTP", "Format de date NTP invalide"); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_PARSE", ESP01_FAIL);           // Retourne l'échec
    }

    static const char *mois_en[] = {// Tableau des mois anglais
                                    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    dt->month = 0;               // Initialisation du mois
    for (int i = 0; i < 12; ++i) // Recherche du mois
    {
        if (strncmp(mois, mois_en[i], 3) == 0) // Comparaison
        {
            dt->month = i + 1; // Affectation du mois
            break;             // Sortie de boucle
        }
    }
    if (dt->month == 0) // Si mois inconnu
    {
        ESP01_LOG_ERROR("NTP", "Mois NTP inconnu : %s", mois); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_PARSE", ESP01_FAIL);           // Retourne l'échec
    }

    static const char *jours_en[] = {// Tableau des jours anglais
                                     "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    dt->wday = -1;              // Initialisation du jour
    for (int i = 0; i < 7; ++i) // Recherche du jour
    {
        if (strncmp(jour, jours_en[i], 3) == 0) // Comparaison
        {
            dt->wday = i; // Affectation du jour
            break;        // Sortie de boucle
        }
    }
    if (dt->wday == -1) // Si jour inconnu
    {
        ESP01_LOG_ERROR("NTP", "Jour NTP inconnu : %s", jour); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_PARSE", ESP01_FAIL);           // Retourne l'échec
    }

    ESP01_LOG_DEBUG("NTP", "Parsing réussi : %02d/%02d/%04d %02d:%02d:%02d (wday=%d)",
                    dt->day, dt->month, dt->year, dt->hour, dt->min, dt->sec, dt->wday); // Log du parsing réussi
    return ESP01_OK;                                                                     // Retourne OK
}

/**
 * @brief Affiche une structure date/heure en français (logs).
 * @param dt Structure à afficher.
 */
void esp01_print_datetime_fr(const ntp_datetime_fr_t *dt)
{
    static const char *jours_fr[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};                                                // Tableau des jours français
    static const char *mois_fr[] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"}; // Tableau des mois français
    ESP01_LOG_INFO("NTP", "%s %02d %s %04d à %02dh%02d:%02d",
                   jours_fr[dt->wday], dt->day, mois_fr[dt->month - 1], dt->year, dt->hour, dt->min, dt->sec); // Affichage formaté en français
}

/**
 * @brief Affiche une structure date/heure en anglais (logs).
 * @param dt Structure à afficher.
 */
void esp01_print_datetime_en(const ntp_datetime_fr_t *dt)
{
    static const char *jours_en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};                                            // Tableau des jours anglais
    static const char *mois_en[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"}; // Tableau des mois anglais
    ESP01_LOG_INFO("NTP", "%s, %02d %s %04d %02d:%02d:%02d",
                   jours_en[dt->wday], dt->day, mois_en[dt->month - 1], dt->year, dt->hour, dt->min, dt->sec); // Affichage formaté en anglais
}
