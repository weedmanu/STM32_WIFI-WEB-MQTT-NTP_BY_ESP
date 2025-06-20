/**
 ******************************************************************************
 * @file    STM32_WifiESP_NTP.c
 * @author  Weedman
 * @version 1.3.0
 * @date    19 juin 2025
 * @brief   Implémentation des fonctions haut niveau NTP pour ESP01
 *
 * @details
 * Ce fichier source contient l'implémentation des fonctions NTP haut niveau :
 * - Configuration et synchronisation NTP (one shot ou périodique)
 * - Parsing et affichage de la date/heure NTP
 * - Support du changement d'heure automatique (DST - Daylight Saving Time)
 * - Accès à la configuration et à la dernière date reçue
 *
 * @note
 * - Nécessite le driver bas niveau STM32_WifiESP.h
 * - Nécessite le module WiFi STM32_WifiESP_WIFI.h
 *
 ******************************************************************************
 */

#include "STM32_WifiESP_NTP.h" // Inclusion du header du module NTP
#include <stdio.h>             // Pour printf, snprintf, etc.
#include <string.h>            // Pour manipulation de chaînes
#include <ctype.h>             // Pour fonctions de classification de caractères

/* ==================== DÉFINITIONS ==================== */
#define ESP01_NTP_SYNC_RETRY 3        // Nombre de tentatives de synchro
#define ESP01_NTP_SYNC_RETRY_MS 1000  // Délai entre tentatives de synchro (ms)
#define ESP01_NTP_INIT_DELAY_MS 2000  // Délai d'initialisation (ms)
#define ESP01_NTP_MIN_VALID_YEAR 1971 // Année minimum valide
#define ESP01_NTP_MIN_DATE_LEN 8      // Longueur minimale d'une date valide

/* ==================== VARIABLES STATIQUES ==================== */

/**
 * @brief Configuration NTP courante (locale)
 */
static esp01_ntp_config_t g_ntp_config = {
    // Structure de configuration NTP locale
    .server = ESP01_NTP_DEFAULT_SERVER,     // Serveur NTP par défaut
    .timezone = 0,                          // Fuseau horaire par défaut
    .period_s = ESP01_NTP_DEFAULT_PERIOD_S, // Période de synchronisation par défaut
    .dst_enable = true                      // Activation du DST par défaut
};

static char g_last_datetime[ESP01_NTP_DATETIME_BUF_SIZE] = {0}; // Dernière date/heure NTP reçue (brute)
static uint8_t g_ntp_updated = 0;                               // Flag indiquant si une nouvelle date NTP a été reçue
static uint32_t g_last_sync_time = 0;                           // Horodatage de la dernière synchronisation
static bool g_initialized = false;                              // État d'initialisation du module

/* ==================== PROTOTYPES DES FONCTIONS STATIQUES ==================== */
static bool is_dst_active(const ntp_datetime_t *dt);
static void apply_dst(ntp_datetime_t *dt);

/* ==================== FONCTIONS PRINCIPALES ==================== */

/**
 * @brief Configure la structure NTP locale (ne modifie pas le module ESP01).
 * @param ntp_server   Nom du serveur NTP.
 * @param timezone     Décalage horaire.
 * @param sync_period_s Période de synchronisation en secondes.
 * @param dst_enable   Active/désactive le changement d'heure automatique.
 * @return ESP01_OK si succès, ESP01_INVALID_PARAM sinon.
 */
ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone, int sync_period_s, bool dst_enable)
{
    ESP01_LOG_INFO("NTP", "Configuration NTP : serveur=%s, timezone=%d, period=%d, dst=%d",
                   ntp_server, timezone, sync_period_s, dst_enable); // Log de la configuration demandée
    VALIDATE_PARAM(ntp_server && strlen(ntp_server) < ESP01_NTP_MAX_SERVER_LEN, ESP01_INVALID_PARAM);
    if (esp01_check_buffer_size(strlen(ntp_server), ESP01_NTP_MAX_SERVER_LEN - 1) != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Dépassement de la taille du buffer serveur NTP");
        ESP01_RETURN_ERROR("NTP_CONFIG", ESP01_BUFFER_OVERFLOW);
    }

    // Utiliser strncpy de façon sécurisée avec vérification explicite
    memset(g_ntp_config.server, 0, ESP01_NTP_MAX_SERVER_LEN);
    strncpy(g_ntp_config.server, ntp_server, ESP01_NTP_MAX_SERVER_LEN - 1);

    g_ntp_config.timezone = timezone;      // Affectation du fuseau horaire
    g_ntp_config.period_s = sync_period_s; // Affectation de la période de synchronisation
    g_ntp_config.dst_enable = dst_enable;  // Activation/désactivation du DST

    ESP01_LOG_INFO("NTP", "Configuration NTP appliquée"); // Log de succès
    return ESP01_OK;                                      // Retourne OK
}

/**
 * @brief Lance la synchronisation NTP (one shot ou périodique selon periodic).
 * @param periodic true = périodique, false = one shot.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_ntp_start_sync(bool periodic)
{
    ESP01_LOG_INFO("NTP", "Démarrage de la synchronisation NTP (periodic=%d)", periodic);

    // Application de la configuration NTP au module ESP01
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

    if (!periodic) // Mode one-shot
    {
        HAL_Delay(ESP01_NTP_INIT_DELAY_MS); // Attente pour initialisation NTP

        char buf[ESP01_NTP_DATETIME_BUF_SIZE];
        for (int i = 0; i < ESP01_NTP_SYNC_RETRY; ++i)
        {
            st = esp01_get_ntp_time(buf, sizeof(buf));
            if (st == ESP01_OK && strlen(buf) > 0)
            {
                // Vérifier la validité de la date NTP (éviter 1970)
                ntp_datetime_t dt;
                if (esp01_parse_ntp_esp01(buf, &dt) == ESP01_OK && dt.year > 1970)
                {
                    if (esp01_check_buffer_size(strlen(buf), ESP01_NTP_DATETIME_BUF_SIZE - 1) != ESP01_OK)
                    {
                        ESP01_LOG_ERROR("NTP", "Dépassement de la taille du buffer datetime NTP");
                        ESP01_RETURN_ERROR("NTP_START_SYNC", ESP01_BUFFER_OVERFLOW);
                    }
                    // Copier la date obtenue dans le buffer global
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
            HAL_Delay(ESP01_NTP_SYNC_RETRY_MS);
        }
        ESP01_LOG_ERROR("NTP", "Impossible de récupérer une date NTP valide après %d tentatives", ESP01_NTP_SYNC_RETRY);
        ESP01_RETURN_ERROR("NTP_START_SYNC", ESP01_FAIL);
    }

    // Mode périodique
    g_initialized = true;
    g_last_sync_time = 0; // Force une première synchronisation immédiate
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
    VALIDATE_PARAM(server && strlen(server) > 0, ESP01_INVALID_PARAM);

    char cmd[ESP01_MAX_CMD_BUF] = {0};   // Buffer pour la commande AT
    char resp[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse
    ESP01_Status_t st;                   // Variable de statut

    ESP01_LOG_INFO("NTP", "Application de la configuration NTP : enable=%d, timezone=%d, server=%s",
                   enable, timezone, server); // Log de la configuration

    // Construction de la commande sécurisée avec vérification des limites
    int written = snprintf(cmd, sizeof(cmd), "AT+CIPSNTPCFG=%d,%d,\"%s\"", enable, timezone, server);
    if (written < 0 || written >= (int)sizeof(cmd))
    {
        ESP01_LOG_ERROR("NTP", "Débordement du buffer de commande NTP");
        ESP01_RETURN_ERROR("NTP_APPLY_CONFIG", ESP01_BUFFER_OVERFLOW);
    }

    // Envoi de la commande AT au format RAW pour bénéficier de plus d'options
    st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);

    if (st != ESP01_OK) // Vérification du succès
    {
        ESP01_LOG_ERROR("NTP", "Echec de la commande AT+CIPSNTPCFG (code=%d)", st); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_APPLY_CONFIG", st);                                 // Retourne l'erreur
    }

    ESP01_LOG_INFO("NTP", "Configuration NTP envoyée avec succès"); // Log de succès
    return st;                                                      // Retourne le statut
}

/**
 * @brief Fonction principale de gestion périodique du NTP
 * @return ESP01_Status_t - ESP01_OK si tout s'est bien passé
 */
ESP01_Status_t esp01_ntp_handle(void)
{
    if (!g_initialized)
    {
        ESP01_LOG_WARN("NTP", "Module non initialisé");
        ESP01_RETURN_ERROR("NTP_HANDLE", ESP01_NOT_INITIALIZED);
    }

    uint32_t current_time = HAL_GetTick();
    uint32_t sync_interval = g_ntp_config.period_s * 1000; // Conversion en ms

    // Vérifier s'il est temps de synchroniser
    if ((current_time - g_last_sync_time) >= sync_interval)
    {
        ESP01_LOG_DEBUG("NTP", "Synchro NTP périodique...");

        // Récupérer l'heure NTP
        char datetime_str[ESP01_NTP_DATETIME_BUF_SIZE] = {0};
        ESP01_Status_t status = esp01_get_ntp_time(datetime_str, sizeof(datetime_str));

        if (status == ESP01_OK && strlen(datetime_str) > 0)
        {
            ESP01_LOG_DEBUG("NTP", "Date extraite: '%s'", datetime_str);

            // Parse la date récupérée
            ntp_datetime_t dt;

            if (esp01_parse_ntp_esp01(datetime_str, &dt) == ESP01_OK)
            {
                ESP01_LOG_DEBUG("NTP", "Date parsée: %02d/%02d/%04d %02d:%02d:%02d (jour %d)",
                                dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec, dt.wday);

                // Appliquer le DST si activé
                if (g_ntp_config.dst_enable)
                {
                    apply_dst(&dt);
                    dt.dst = true; // Marquer comme ajusté DST
                }

                // Stocker la date NTP brute
                memset(g_last_datetime, 0, ESP01_NTP_DATETIME_BUF_SIZE);
                strncpy(g_last_datetime, datetime_str, ESP01_NTP_DATETIME_BUF_SIZE - 1);
                g_ntp_updated = 1;

                // Formater pour affichage
                char buffer_fr[100] = {0};
                char buffer_en[100] = {0};

                if (esp01_format_datetime_fr(&dt, buffer_fr, sizeof(buffer_fr)) == ESP01_OK &&
                    esp01_format_datetime_en(&dt, buffer_en, sizeof(buffer_en)) == ESP01_OK)
                {
                    printf("\r\n[NTP] Date/heure: %s\r\n", buffer_fr);
                    printf("[NTP] Date/time: %s\r\n", buffer_en);
                }
            }
            else
            {
                ESP01_LOG_ERROR("NTP", "Erreur lors du parsing de la date");
            }
        }
        else
        {
            ESP01_LOG_WARN("NTP", "Échec de récupération de l'heure NTP");
        }

        // Mise à jour du timestamp de dernière synchro, même en cas d'échec
        g_last_sync_time = current_time;
    }

    return ESP01_OK;
}

/**
 * @brief  Récupère la date/heure NTP depuis le module ESP01 (commande AT).
 * @param  datetime_buf Buffer de sortie.
 * @param  bufsize      Taille du buffer.
 * @retval ESP01_Status_t
 */
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize)
{
    VALIDATE_PARAM(datetime_buf && bufsize > 0, ESP01_INVALID_PARAM); // Validation des paramètres

    ESP01_LOG_DEBUG("NTP", "Récupération heure NTP..."); // Log de récupération

    if (esp01_check_buffer_size(ESP01_NTP_DATETIME_BUF_SIZE - 1, bufsize) != ESP01_OK) // Vérification de la taille du buffer
    {
        ESP01_LOG_ERROR("NTP", "Buffer trop petit pour la date NTP"); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_GET_TIME", ESP01_BUFFER_OVERFLOW);    // Retourne l'erreur
    }

    char cmd[] = "AT+CIPSNTPTIME?";          // Commande AT pour obtenir l'heure NTP
    char response[ESP01_MAX_RESP_BUF] = {0}; // Buffer pour la réponse

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, response, sizeof(response), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK) // Vérification du succès
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

    // Nettoyer la chaîne en supprimant les espaces et guillemets au début
    char *ptr = datetime_buf;
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\"')
        ptr++;
    if (ptr != datetime_buf)
        memmove(datetime_buf, ptr, strlen(ptr) + 1);

    if (strlen(datetime_buf) < ESP01_NTP_MIN_DATE_LEN) // Vérification de la longueur minimale
    {
        ESP01_LOG_ERROR("NTP", "Date NTP trop courte ou invalide : %s", datetime_buf); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_GET_TIME", ESP01_FAIL);                                // Retourne l'échec
    }

    ESP01_LOG_DEBUG("NTP", "Heure NTP récupérée : %s", datetime_buf); // Log de succès
    return ESP01_OK;                                                  // Retourne OK
}

/* ==================== FONCTIONS DST (DAYLIGHT SAVING TIME) ==================== */

/**
 * @brief Vérifie si l'heure d'été doit être appliquée selon règles européennes
 * @param dt Structure date/heure
 * @return true si heure d'été active, false sinon
 */
static bool is_dst_active(const ntp_datetime_t *dt)
{
    VALIDATE_PARAM(dt, false);

    ESP01_LOG_DEBUG("NTP", "Vérification du DST pour %02d/%02d/%04d", dt->day, dt->month, dt->year);

    // Ne pas appliquer le DST si désactivé dans la config
    if (!g_ntp_config.dst_enable)
    {
        ESP01_LOG_DEBUG("NTP", "DST désactivé dans la configuration");
        return false;
    }

    // Règle européenne simplifiée:
    // - Dernier dimanche de mars à 2h -> Dernier dimanche d'octobre à 3h

    if (dt->month < 3 || dt->month > 10)
    {
        // Janvier, février, novembre et décembre: heure d'hiver
        return false;
    }

    if (dt->month > 3 && dt->month < 10)
    {
        // Avril à septembre: heure d'été
        return true;
    }

    // Pour mars: dernier dimanche à 2h -> heure d'été
    if (dt->month == 3)
    {
        // Le dernier dimanche est entre le 25 et le 31
        int last_sunday = 31;
        while (last_sunday >= 25)
        {
            // Calcul simpliste pour exemple
            // Une implémentation complète nécessiterait une formule plus précise
            int wday = (dt->year / 4 + dt->year + 3 + last_sunday) % 7; // Approximation
            if (wday == 0)
                break; // C'est un dimanche
            last_sunday--;
        }

        ESP01_LOG_DEBUG("NTP", "Mars - Dernier dimanche le %d", last_sunday);
        return (dt->day > last_sunday || (dt->day == last_sunday && dt->hour >= 2));
    }

    // Pour octobre: dernier dimanche à 3h -> heure d'hiver
    if (dt->month == 10)
    {
        // Le dernier dimanche est entre le 25 et le 31
        int last_sunday = 31;
        while (last_sunday >= 25)
        {
            // Calcul simpliste pour exemple
            int wday = (dt->year / 4 + dt->year + 0 + last_sunday) % 7; // Approximation
            if (wday == 0)
                break; // C'est un dimanche
            last_sunday--;
        }

        ESP01_LOG_DEBUG("NTP", "Octobre - Dernier dimanche le %d", last_sunday);
        return (dt->day < last_sunday || (dt->day == last_sunday && dt->hour < 3));
    }

    return false;
}

/**
 * @brief Applique le changement d'heure DST si nécessaire
 * @param dt Structure date/heure à modifier
 */
static void apply_dst(ntp_datetime_t *dt)
{
    VALIDATE_PARAM_VOID(dt);

    if (!g_ntp_config.dst_enable)
    {
        dt->dst = false;
        return;
    }

    bool dst_active = is_dst_active(dt);

    if (dst_active)
    {
        // Ajout de la sauvegarde de l'heure actuelle pour le log
        int prev_hour = dt->hour;

        dt->hour += 1;

        // Gestion du passage au jour suivant si nécessaire
        if (dt->hour >= 24)
        {
            dt->hour -= 24;
            dt->day += 1;
            dt->wday = (dt->wday + 1) % 7;

            // Gestion du passage au mois suivant
            static const uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            uint8_t max_days = days_in_month[dt->month];

            // Ajustement pour février en année bissextile
            if (dt->month == 2 && ((dt->year % 4 == 0 && dt->year % 100 != 0) || dt->year % 400 == 0))
            {
                max_days = 29;
            }

            if (dt->day > max_days)
            {
                dt->day = 1;
                dt->month += 1;

                if (dt->month > 12)
                {
                    dt->month = 1;
                    dt->year += 1;
                }
            }
        }

        ESP01_LOG_DEBUG("NTP", "DST appliqué: %02d:xx -> %02d:xx", prev_hour, dt->hour);
        dt->dst = true;
    }
    else
    {
        ESP01_LOG_DEBUG("NTP", "DST non appliqué (heure d'hiver)");
        dt->dst = false;
    }
}

/* ==================== FONCTIONS ONE-SHOT SIMPLES ==================== */

/**
 * @brief Fonction one-shot simple pour obtenir l'heure et l'afficher
 * @param lang Format de l'affichage ('F' pour français, 'E' pour anglais)
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_ntp_get_and_display(char lang)
{
    ESP01_LOG_INFO("NTP", "Récupération et affichage de l'heure NTP (lang=%c)", lang);

    char buffer[ESP01_NTP_DATETIME_BUF_SIZE];
    ntp_datetime_t dt;
    ESP01_Status_t status = esp01_get_ntp_time(buffer, sizeof(buffer));

    if (status != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Impossible de récupérer l'heure NTP");
        ESP01_RETURN_ERROR("NTP_DISPLAY", status);
    }

    // Vérifier si la date est valide (pas une date proche de 1970)
    if (!esp01_parse_ntp_datetime(buffer, &dt)) // Utiliser la fonction publique, pas la fonction statique
    {
        ESP01_LOG_ERROR("NTP", "Impossible de parser la date NTP");
        ESP01_RETURN_ERROR("NTP_DISPLAY", ESP01_NTP_INVALID_RESPONSE);
    }

    // Vérifier si la date est proche de 1970 (date non synchronisée)
    if (dt.year < 2022)
    {
        ESP01_LOG_WARN("NTP", "Date non synchronisée (%04d/%02d/%02d), serveur NTP probablement inaccessible",
                       dt.year, dt.month, dt.day);
        ESP01_RETURN_ERROR("NTP_DISPLAY", ESP01_NTP_SERVER_NOT_REACHABLE);
    }

    // Affichage selon la langue demandée
    if (lang == 'F' || lang == 'f')
    {
        esp01_print_datetime_fr(&dt);
    }
    else
    {
        char formatted[128];
        esp01_format_datetime_en(&dt, formatted, sizeof(formatted));
        ESP01_LOG_INFO("NTP", "Date/time: %s", formatted);
    }

    // Mettre à jour la variable globale pour stocker la dernière date/heure
    memcpy(&g_last_datetime, &dt, sizeof(ntp_datetime_t));

    return ESP01_OK;
}

/**
 * @brief Formate une date au format français dans un buffer
 * @param dt Structure date/heure
 * @param buffer Buffer de sortie
 * @param size Taille du buffer
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_format_datetime_fr(const ntp_datetime_t *dt, char *buffer, size_t size)
{
    VALIDATE_PARAM(dt && buffer && size >= 40, ESP01_INVALID_PARAM);
    VALIDATE_PARAM(dt->wday <= 6 && dt->month >= 1 && dt->month <= 12, ESP01_INVALID_PARAM);

    static const char *jours_fr[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
    static const char *mois_fr[] = {"janvier", "février", "mars", "avril", "mai", "juin",
                                    "juillet", "août", "septembre", "octobre", "novembre", "décembre"};

    int written = snprintf(buffer, size, "%s %02d %s %04d à %02dh%02d:%02d%s",
                           jours_fr[dt->wday], dt->day, mois_fr[dt->month - 1], dt->year,
                           dt->hour, dt->min, dt->sec,
                           dt->dst ? " (heure d'été)" : "");

    if (written < 0 || written >= (int)size)
    {
        ESP01_LOG_ERROR("NTP", "Débordement du buffer lors du formatage de date (FR)");
        return ESP01_BUFFER_OVERFLOW;
    }

    return ESP01_OK;
}

/**
 * @brief Formate une date au format anglais dans un buffer
 * @param dt Structure date/heure
 * @param buffer Buffer de sortie
 * @param size Taille du buffer
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_format_datetime_en(const ntp_datetime_t *dt, char *buffer, size_t size)
{
    VALIDATE_PARAM(dt && buffer && size >= 40, ESP01_INVALID_PARAM);
    VALIDATE_PARAM(dt->wday <= 6 && dt->month >= 1 && dt->month <= 12, ESP01_INVALID_PARAM);

    static const char *jours_en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *mois_en[] = {"January", "February", "March", "April", "May", "June",
                                    "July", "August", "September", "October", "November", "December"};

    // Conversion 24h vers 12h avec AM/PM
    int hour_12 = dt->hour % 12;
    if (hour_12 == 0)
        hour_12 = 12; // 0h -> 12 AM, 12h -> 12 PM

    const char *am_pm = (dt->hour < 12) ? "AM" : "PM";

    int written = snprintf(buffer, size, "%s, %d %s, %04d %d:%02d:%02d %s%s",
                           jours_en[dt->wday],
                           dt->day, mois_en[dt->month - 1], dt->year,
                           dt->hour, dt->min, dt->sec, am_pm,
                           dt->dst ? " (DST)" : "");

    if (written < 0 || written >= (int)size)
    {
        ESP01_LOG_ERROR("NTP", "Débordement du buffer lors du formatage de date (EN)");
        return ESP01_BUFFER_OVERFLOW;
    }

    return ESP01_OK;
}

/* ==================== ACCÈS ET AFFICHAGE ==================== */

/**
 * @brief Affiche la configuration NTP courante (logs).
 */
void esp01_print_ntp_config(void)
{
    ESP01_LOG_INFO("NTP", "Affichage de la configuration NTP");                        // Log d'affichage
    ESP01_LOG_INFO("NTP", "Serveur : %s", g_ntp_config.server);                        // Affichage du serveur
    ESP01_LOG_INFO("NTP", "Fuseau horaire : %d", g_ntp_config.timezone);               // Affichage du fuseau horaire
    ESP01_LOG_INFO("NTP", "Période de synchro (s) : %d", g_ntp_config.period_s);       // Affichage de la période
    ESP01_LOG_INFO("NTP", "DST activé : %s", g_ntp_config.dst_enable ? "OUI" : "NON"); // Affichage de l'état DST
}

/**
 * @brief Affiche la dernière date/heure NTP reçue en français (logs).
 */
void esp01_ntp_print_last_datetime_fr(void)
{
    const char *datetime_ntp = esp01_ntp_get_last_datetime(); // Récupération de la dernière date/heure

    if (!datetime_ntp || strlen(datetime_ntp) == 0) // Vérification de la disponibilité
    {
        ESP01_LOG_WARN("NTP", "Date NTP non disponible"); // Log d'avertissement
        return;                                           // Sortie
    }

    ntp_datetime_t dt;
    if (esp01_parse_ntp_esp01(datetime_ntp, &dt) == ESP01_OK) // Parsing de la date/heure
    {
        // Appliquer DST si activé
        if (g_ntp_config.dst_enable)
        {
            apply_dst(&dt);
        }

        char buffer[100] = {0};
        if (esp01_format_datetime_fr(&dt, buffer, sizeof(buffer)) == ESP01_OK)
        {
            ESP01_LOG_INFO("NTP", "%s", buffer);
        }
    }
    else
    {
        ESP01_LOG_WARN("NTP", "Date NTP invalide"); // Log d'avertissement
    }
}

/**
 * @brief Affiche la dernière date/heure NTP reçue en anglais (logs).
 */
void esp01_ntp_print_last_datetime_en(void)
{
    ESP01_LOG_INFO("NTP", "Affichage de la dernière date/heure NTP (EN)"); // Log d'affichage
    const char *datetime_ntp = esp01_ntp_get_last_datetime();              // Récupération de la dernière date/heure

    if (!datetime_ntp || strlen(datetime_ntp) == 0) // Vérification de la disponibilité
    {
        ESP01_LOG_WARN("NTP", "NTP date not available"); // Log d'avertissement
        return;                                          // Sortie
    }

    ntp_datetime_t dt;
    if (esp01_parse_ntp_esp01(datetime_ntp, &dt) == ESP01_OK) // Parsing de la date/heure
    {
        // Appliquer DST si activé
        if (g_ntp_config.dst_enable)
        {
            apply_dst(&dt);
        }

        char buffer[100] = {0};
        if (esp01_format_datetime_en(&dt, buffer, sizeof(buffer)) == ESP01_OK)
        {
            ESP01_LOG_INFO("NTP", "%s", buffer);
        }
    }
    else
    {
        ESP01_LOG_WARN("NTP", "Invalid NTP date"); // Log d'avertissement
    }
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
    // ESP01_LOG_DEBUG("NTP", "Flag de mise à jour NTP : %d", g_ntp_updated); // Log du flag
    return g_ntp_updated; // Retourne le flag
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
ESP01_Status_t esp01_parse_ntp_esp01(const char *datetime_ntp, ntp_datetime_t *dt)
{
    ESP01_LOG_DEBUG("NTP", "Parsing de la date NTP : %s", datetime_ntp); // Log du parsing
    VALIDATE_PARAM(datetime_ntp && dt, ESP01_INVALID_PARAM);             // Validation des paramètres

    memset(dt, 0, sizeof(ntp_datetime_t)); // Initialisation propre de la structure

    char mois[8] = {0}, jour[8] = {0}; // Buffers pour le parsing
    // Variables temporaires pour stocker les valeurs entières
    int day, hour, min, sec, year;

    int res = sscanf(datetime_ntp, "%7s %7s %d %d:%d:%d %d",
                     jour, mois, &day, &hour, &min, &sec, &year);

    // Conversion des entiers vers les types appropriés
    if (res == 7)
    {
        dt->day = (uint8_t)day;
        dt->hour = (uint8_t)hour;
        dt->min = (uint8_t)min;
        dt->sec = (uint8_t)sec;
        dt->year = (uint16_t)year;
    }
    else
    {
        ESP01_LOG_ERROR("NTP", "Format de date NTP invalide");
        ESP01_RETURN_ERROR("NTP_PARSE", ESP01_FAIL);
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
    dt->wday = 7;               // Valeur par défaut invalide
    for (int i = 0; i < 7; ++i) // Recherche du jour
    {
        if (strncmp(jour, jours_en[i], 3) == 0) // Comparaison
        {
            dt->wday = i; // Affectation du jour
            break;        // Sortie de boucle
        }
    }
    if (dt->wday > 6) // Si jour inconnu (valeur reste à 7)
    {
        ESP01_LOG_ERROR("NTP", "Jour NTP inconnu : %s", jour); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_PARSE", ESP01_FAIL);           // Retourne l'échec
    }

    // Initialiser le flag DST à false (il sera mis à jour plus tard si nécessaire)
    dt->dst = false;

    ESP01_LOG_DEBUG("NTP", "Parsing réussi : %02d/%02d/%04d %02d:%02d:%02d (wday=%d)",
                    dt->day, dt->month, dt->year, dt->hour, dt->min, dt->sec, dt->wday); // Log du parsing réussi
    return ESP01_OK;                                                                     // Retourne OK
}

/**
 * @brief Affiche une structure date/heure en français (logs).
 * @param dt Structure à afficher.
 */
void esp01_print_datetime_fr(const ntp_datetime_t *dt)
{
    VALIDATE_PARAM_VOID(dt);

    if (dt->wday > 6 || dt->month < 1 || dt->month > 12)
    {
        ESP01_LOG_ERROR("NTP", "Structure de date invalide (jour=%d, mois=%d)", dt->wday, dt->month);
        return;
    }

    static const char *jours_fr[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
    static const char *mois_fr[] = {"janvier", "février", "mars", "avril", "mai", "juin",
                                    "juillet", "août", "septembre", "octobre", "novembre", "décembre"};

    ESP01_LOG_INFO("NTP", "%s %02d %s %04d à %02dh%02d:%02d%s",
                   jours_fr[dt->wday], dt->day, mois_fr[dt->month - 1], dt->year,
                   dt->hour, dt->min, dt->sec,
                   dt->dst ? " (heure d'été)" : "");
}

/**
 * @brief Affiche une structure date/heure en anglais (logs).
 * @param dt Structure à afficher.
 */
void esp01_print_datetime_en(const ntp_datetime_t *dt)
{
    VALIDATE_PARAM_VOID(dt);
    VALIDATE_PARAM_VOID(dt->wday <= 6 && dt->month >= 1 && dt->month <= 12);

    static const char *jours_en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *mois_en[] = {"January", "February", "March", "April", "May", "June",
                                    "July", "August", "September", "October", "November", "December"};

    ESP01_LOG_INFO("NTP", "%s, %02d %s %04d %02d:%02d:%02d%s",
                   jours_en[dt->wday], dt->day, mois_en[dt->month - 1], dt->year,
                   dt->hour, dt->min, dt->sec,
                   dt->dst ? " (DST)" : "");
}

/**
 * @brief Parse une chaîne de date/heure au format NTP de l'ESP01
 * @param datetime_str Chaîne de date/heure à parser
 * @param dt Structure de sortie
 * @return true si parsing réussi, false sinon
 */
bool esp01_parse_ntp_datetime(const char *datetime_str, ntp_datetime_t *dt)
{
    ESP01_LOG_DEBUG("NTP", "Parsing de la date NTP : %s", datetime_str);
    VALIDATE_PARAM(datetime_str && dt, false);

    // Format attendu: "Thu Jun 19 11:41:56 2025"
    char day_str[4], month_str[4];
    int day, hour, min, sec, year;

    int matched = sscanf(datetime_str, "%3s %3s %d %d:%d:%d %d",
                         day_str, month_str, &day, &hour, &min, &sec, &year);

    if (matched != 7)
    {
        ESP01_LOG_ERROR("NTP", "Format de date invalide: %s", datetime_str);
        return false;
    }

    // Remplit la structure avec les valeurs extraites
    dt->day = day;
    dt->hour = hour;
    dt->min = min;
    dt->sec = sec;
    dt->year = year;

    // Traduit le mois textuel en numérique
    if (strncmp(month_str, "Jan", 3) == 0)
        dt->month = 1;
    else if (strncmp(month_str, "Feb", 3) == 0)
        dt->month = 2;
    else if (strncmp(month_str, "Mar", 3) == 0)
        dt->month = 3;
    else if (strncmp(month_str, "Apr", 3) == 0)
        dt->month = 4;
    else if (strncmp(month_str, "May", 3) == 0)
        dt->month = 5;
    else if (strncmp(month_str, "Jun", 3) == 0)
        dt->month = 6;
    else if (strncmp(month_str, "Jul", 3) == 0)
        dt->month = 7;
    else if (strncmp(month_str, "Aug", 3) == 0)
        dt->month = 8;
    else if (strncmp(month_str, "Sep", 3) == 0)
        dt->month = 9;
    else if (strncmp(month_str, "Oct", 3) == 0)
        dt->month = 10;
    else if (strncmp(month_str, "Nov", 3) == 0)
        dt->month = 11;
    else if (strncmp(month_str, "Dec", 3) == 0)
        dt->month = 12;
    else
    {
        ESP01_LOG_ERROR("NTP", "Mois invalide: %s", month_str);
        return false;
    }

    // Traduit le jour textuel en numérique (0=dimanche, 1=lundi, ..., 6=samedi)
    if (strncmp(day_str, "Sun", 3) == 0)
        dt->wday = 0;
    else if (strncmp(day_str, "Mon", 3) == 0)
        dt->wday = 1;
    else if (strncmp(day_str, "Tue", 3) == 0)
        dt->wday = 2;
    else if (strncmp(day_str, "Wed", 3) == 0)
        dt->wday = 3;
    else if (strncmp(day_str, "Thu", 3) == 0)
        dt->wday = 4;
    else if (strncmp(day_str, "Fri", 3) == 0)
        dt->wday = 5;
    else if (strncmp(day_str, "Sat", 3) == 0)
        dt->wday = 6;
    else
    {
        ESP01_LOG_ERROR("NTP", "Jour invalide: %s", day_str);
        return false;
    }

    ESP01_LOG_DEBUG("NTP", "Parsing réussi : %02d/%02d/%04d %02d:%02d:%02d (wday=%d)",
                    dt->day, dt->month, dt->year, dt->hour, dt->min, dt->sec, dt->wday);

    // Vérification supplémentaire : si année = 1970, c'est probablement une date non synchronisée
    if (dt->year == 1970)
    {
        ESP01_LOG_WARN("NTP", "Date de l'époque Unix détectée (%04d/%02d/%02d), serveur NTP probablement inaccessible",
                       dt->year, dt->month, dt->day);
    }

    return true;
}
