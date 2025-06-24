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

/* ==================== CONSTANTES SPÉCIFIQUES AU MODULE ==================== */
#define ESP01_NTP_SYNC_TIMEOUT 5000 // Timeout synchronisation NTP (ms)

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
    ESP01_LOG_DEBUG("NTP", "Configuration NTP : serveur=%s, timezone=%d, period=%d, dst=%d",
                    ntp_server, timezone, sync_period_s, dst_enable); // Log de la configuration demandée

    VALIDATE_PARAM(ntp_server && strlen(ntp_server) < ESP01_NTP_MAX_SERVER_LEN, ESP01_INVALID_PARAM); // Vérifie la validité du nom de serveur

    if (esp01_check_buffer_size(strlen(ntp_server), ESP01_NTP_MAX_SERVER_LEN - 1) != ESP01_OK) // Vérifie la taille du nom de serveur
    {
        ESP01_LOG_ERROR("NTP", "Dépassement de la taille du buffer serveur NTP"); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_CONFIG", ESP01_BUFFER_OVERFLOW);                  // Retourne l'erreur
    }

    // Utiliser esp01_safe_strcpy pour la copie sécurisée
    if (esp01_safe_strcpy(g_ntp_config.server, ESP01_NTP_MAX_SERVER_LEN, ntp_server) != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Erreur lors de la copie du nom de serveur NTP"); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_CONFIG", ESP01_BUFFER_OVERFLOW);                 // Retourne l'erreur
    }

    g_ntp_config.timezone = timezone;      // Affectation du fuseau horaire
    g_ntp_config.period_s = sync_period_s; // Affectation de la période de synchronisation
    g_ntp_config.dst_enable = dst_enable;  // Activation/désactivation du DST

    ESP01_LOG_DEBUG("NTP", "Configuration NTP appliquée"); // Log de succès
    return ESP01_OK;                                       // Retourne OK
}
/**
 * @brief Vérifie si la synchronisation NTP périodique est activée.
 * @retval true si la synchronisation périodique est active, false sinon.
 */
bool esp01_ntp_is_periodic_enabled(void)
{
    return g_initialized; // Retourne l'état de la synchronisation périodique
}

/**
 * @brief Lance la synchronisation NTP (one shot ou périodique selon periodic).
 * @param periodic true = périodique, false = one shot.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_ntp_start_sync(bool periodic)
{
    ESP01_LOG_DEBUG("NTP", "Démarrage de la synchronisation NTP (periodic=%d)", periodic); // Log du mode de synchro

    // Application de la configuration NTP au module ESP01
    ESP01_Status_t st = esp01_apply_ntp_config(
        1,                      // Active NTP
        g_ntp_config.timezone,  // Fuseau horaire
        g_ntp_config.server,    // Serveur NTP
        g_ntp_config.period_s); // Période de synchro

    if (st != ESP01_OK) // Vérifie le succès de la configuration
    {
        ESP01_LOG_ERROR("NTP", "Echec de l'application de la configuration NTP (code=%d)", st); // Log d'erreur
        ESP01_RETURN_ERROR("NTP_START_SYNC", st);                                               // Retourne l'erreur
    }

    if (!periodic) // Mode one-shot
    {
        HAL_Delay(ESP01_NTP_INIT_DELAY_MS);            // Attente pour initialisation NTP
        g_ntp_updated = 0;                             // Réinitialise le flag de mise à jour
        char buf[ESP01_NTP_DATETIME_BUF_SIZE];         // Buffer pour la date/heure NTP
        for (int i = 0; i < ESP01_NTP_SYNC_RETRY; ++i) // Boucle de tentatives de synchro
        {
            st = esp01_get_ntp_time(buf, sizeof(buf)); // Récupère l'heure NTP
            if (st == ESP01_OK && strlen(buf) > 0)     // Vérifie le succès
            {
                ntp_datetime_t dt;                                                 // Structure pour le parsing
                if (esp01_parse_ntp_esp01(buf, &dt) == ESP01_OK && dt.year > 1970) // Parsing et validité
                {
                    if (esp01_check_buffer_size(strlen(buf), ESP01_NTP_DATETIME_BUF_SIZE - 1) != ESP01_OK) // Vérifie la taille du buffer
                    {
                        ESP01_LOG_ERROR("NTP", "Dépassement de la taille du buffer datetime NTP"); // Log d'erreur
                        ESP01_RETURN_ERROR("NTP_START_SYNC", ESP01_BUFFER_OVERFLOW);               // Retourne l'erreur
                    }
                    // Copie la date obtenue dans le buffer global
                    if (esp01_safe_strcpy(g_last_datetime, ESP01_NTP_DATETIME_BUF_SIZE, buf) != ESP01_OK)
                    {
                        ESP01_LOG_ERROR("NTP", "Erreur lors de la copie de la date/heure NTP"); // Log d'erreur
                        ESP01_RETURN_ERROR("NTP_START_SYNC", ESP01_BUFFER_OVERFLOW);            // Retourne l'erreur
                    }
                    ESP01_LOG_DEBUG("NTP", "Synchronisation NTP réussie : %s", g_last_datetime); // Log succès
                    ESP01_LOG_DEBUG("NTP", "Date brute one-shot : %s", g_last_datetime);         // Log date brute
                    ESP01_LOG_DEBUG("NTP", "Structure après parsing : %02d/%02d/%04d %02d:%02d:%02d (wday=%d, DST=%d)",
                                    dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec, dt.wday, dt.dst); // Log structure
                    return ESP01_OK;                                                                      // Succès
                }
                else // Parsing ou date invalide
                {
                    ESP01_LOG_ERROR("NTP", "Synchronisation NTP échouée ou date invalide : %s", buf); // Log d'erreur
                }
            }
            else // Si la récupération de l'heure NTP a échoué
            {
                ESP01_LOG_WARN("NTP", "Tentative %d de récupération de l'heure NTP échouée", i + 1); // Log tentative échouée
            }
            ESP01_LOG_DEBUG("NTP", "Tentative %d : réponse brute = '%s'", i + 1, buf); // Log de la réponse brute
            HAL_Delay(ESP01_NTP_SYNC_RETRY_MS);                                        // Attente avant nouvelle tentative
        }
        ESP01_LOG_ERROR("NTP", "Impossible de récupérer une date NTP valide après %d tentatives", ESP01_NTP_SYNC_RETRY); // Log d'échec final
        ESP01_RETURN_ERROR("NTP_START_SYNC", ESP01_FAIL);                                                                // Retourne l'échec
    }
    else // Mode périodique
    {
        g_initialized = true;                                             // Active la synchro périodique
        g_last_sync_time = 0;                                             // Force une première synchro immédiate
        ESP01_LOG_DEBUG("NTP", "Synchronisation NTP périodique activée"); // Log activation périodique
        return ESP01_OK;                                                  // Succès
    }
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

    ESP01_LOG_DEBUG("NTP", "Application de la configuration NTP : enable=%d, timezone=%d, server=%s",
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

    ESP01_LOG_DEBUG("NTP", "Configuration NTP envoyée avec succès"); // Log de succès
    return st;                                                       // Retourne le statut
}

/**
 * @brief Fonction principale de gestion périodique du NTP
 * @return ESP01_Status_t - ESP01_OK si tout s'est bien passé
 */
ESP01_Status_t esp01_ntp_handle(void)
{
    if (!g_initialized) // Vérifie si la synchro périodique est active
    {
        return ESP01_OK; // Ne rien faire, pas de log et pas de traitement si non initialisé
    }

    uint32_t current_time = HAL_GetTick();                 // Récupère le temps système actuel (ms)
    uint32_t sync_interval = g_ntp_config.period_s * 1000; // Calcule l'intervalle de synchro en ms

    // Vérifie s'il est temps de synchroniser
    if ((current_time - g_last_sync_time) >= sync_interval) // Si oui
    {
        ESP01_LOG_DEBUG("NTP", "Synchro NTP périodique..."); // Log début de synchro

        // Récupère l'heure NTP depuis le module
        char datetime_str[ESP01_NTP_DATETIME_BUF_SIZE] = {0};                           // Buffer pour la date brute
        ESP01_Status_t status = esp01_get_ntp_time(datetime_str, sizeof(datetime_str)); // Récupération

        if (status == ESP01_OK && strlen(datetime_str) > 0) // Vérifie le succès et la présence d'une date
        {
            ESP01_LOG_DEBUG("NTP", "Date extraite: '%s'", datetime_str); // Log la date brute

            ntp_datetime_t dt; // Structure pour le parsing

            if (esp01_parse_ntp_esp01(datetime_str, &dt) == ESP01_OK) // Parsing de la date brute
            {
                ESP01_LOG_DEBUG("NTP", "Date parsée: %02d/%02d/%04d %02d:%02d:%02d (jour %d)",
                                dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec, dt.wday); // Log parsing réussi

                // Applique le DST si activé dans la config
                if (g_ntp_config.dst_enable)
                {
                    apply_dst(&dt); // Applique le DST
                    dt.dst = true;  // Marque comme ajusté DST
                    ESP01_LOG_DEBUG("NTP", "Structure après DST : %02d/%02d/%04d %02d:%02d:%02d (wday=%d, DST=%d)",
                                    dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec, dt.wday, dt.dst); // Log après DST
                }

                // Stocke la date brute dans la variable globale
                if (esp01_safe_strcpy(g_last_datetime, ESP01_NTP_DATETIME_BUF_SIZE, datetime_str) != ESP01_OK)
                {
                    ESP01_LOG_ERROR("NTP", "Erreur lors de la copie de la date NTP brute"); // Log d'erreur
                    return ESP01_BUFFER_OVERFLOW;                                           // Retourne l'erreur
                }
                g_ntp_updated = 1; // Signale qu'une nouvelle date est disponible

                // Formate pour affichage (FR et EN)
                char buffer_fr[100] = {0};
                char buffer_en[100] = {0};

                if (esp01_format_datetime_fr(&dt, buffer_fr, sizeof(buffer_fr)) == ESP01_OK &&
                    esp01_format_datetime_en(&dt, buffer_en, sizeof(buffer_en)) == ESP01_OK)
                {
                    ESP01_LOG_DEBUG("[NTP]", "Date/heure: '%s'", buffer_fr); // Log FR
                    ESP01_LOG_DEBUG("[NTP]", "Date/time: '%s'", buffer_en);  // Log EN
                }
            }
            else
            {
                ESP01_LOG_ERROR("NTP", "Erreur lors du parsing de la date"); // Log parsing échoué
            }
        }
        else
        {
            ESP01_LOG_WARN("NTP", "Échec de récupération de l'heure NTP"); // Log échec récupération
        }

        // Met à jour le timestamp de dernière synchro, même en cas d'échec
        g_last_sync_time = current_time;
    }

    return ESP01_OK; // Retourne OK
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
 * @brief Vérifie si l'heure d'été doit être appliquée selon les règles européennes.
 * @param dt Structure date/heure à tester.
 * @return true si heure d'été active, false sinon.
 */
static bool is_dst_active(const ntp_datetime_t *dt)
{
    VALIDATE_PARAM(dt, false); // Vérifie la validité du pointeur

    ESP01_LOG_DEBUG("NTP", "Vérification du DST pour %02d/%02d/%04d", dt->day, dt->month, dt->year); // Log de vérification

    if (!g_ntp_config.dst_enable) // Si le DST est désactivé dans la config
    {
        ESP01_LOG_DEBUG("NTP", "DST désactivé dans la configuration"); // Log si DST désactivé
        return false;                                                  // DST non appliqué
    }

    // Mois hors période DST (janvier, février, novembre, décembre)
    if (dt->month < 3 || dt->month > 10)
        return false; // Heure d'hiver

    // Mois entièrement en DST (avril à septembre)
    if (dt->month > 3 && dt->month < 10)
        return true; // Heure d'été

    // Calcul du dernier dimanche du mois (algorithme de Zeller)
    int y = dt->year;     // Année courante
    int m = dt->month;    // Mois courant
    int last_sunday = 31; // Dernier jour possible du mois

    // Pour mars : passage à l'heure d'été le dernier dimanche à 2h
    if (dt->month == 3)
    {
        // Recherche du dernier dimanche de mars
        for (; last_sunday >= 25; --last_sunday)
        {
            int d = last_sunday;
            int mm = m;
            int yy = y;
            if (mm < 3)
            {
                mm += 12;
                yy -= 1;
            } // Ajustement pour Zeller
            int zeller = (d + (13 * (mm + 1)) / 5 + yy + yy / 4 - yy / 100 + yy / 400) % 7; // Zeller: 0=sam, 1=dim, ..., 6=ven
            if (zeller == 1)                                                                // 1 = dimanche
                break;                                                                      // On a trouvé le dernier dimanche
        }
        ESP01_LOG_DEBUG("NTP", "Mars - Dernier dimanche le %d", last_sunday); // Log du dernier dimanche de mars
        // DST actif après 2h du dernier dimanche de mars
        return (dt->day > last_sunday || (dt->day == last_sunday && dt->hour >= 2));
    }

    // Pour octobre : retour à l'heure d'hiver le dernier dimanche à 3h
    if (dt->month == 10)
    {
        // Recherche du dernier dimanche d'octobre
        for (; last_sunday >= 25; --last_sunday)
        {
            int d = last_sunday;
            int mm = m;
            int yy = y;
            if (mm < 3)
            {
                mm += 12;
                yy -= 1;
            }
            int zeller = (d + (13 * (mm + 1)) / 5 + yy + yy / 4 - yy / 100 + yy / 400) % 7;
            if (zeller == 1)
                break;
        }
        ESP01_LOG_DEBUG("NTP", "Octobre - Dernier dimanche le %d", last_sunday); // Log du dernier dimanche d'octobre
        // DST actif avant 3h du dernier dimanche d'octobre
        return (dt->day < last_sunday || (dt->day == last_sunday && dt->hour < 3));
    }

    return false; // Cas par défaut (ne devrait pas arriver)
}

/**
 * @brief Applique le changement d'heure DST si nécessaire
 * @param dt Structure date/heure à modifier
 */
static void apply_dst(ntp_datetime_t *dt)
{
    VALIDATE_PARAM_VOID(dt); // Vérifie la validité du pointeur

    if (!g_ntp_config.dst_enable) // Si le DST est désactivé dans la config
    {
        dt->dst = false; // On indique que le DST n'est pas appliqué
        return;          // Sortie immédiate
    }

    bool dst_active = is_dst_active(dt); // Vérifie si le DST doit être appliqué

    if (dst_active) // Si le DST est actif
    {
        int prev_hour = dt->hour; // Sauvegarde de l'heure avant modification (pour le log)

        dt->hour += 1; // Ajoute 1h pour le passage à l'heure d'été

        // Gestion du passage au jour suivant si nécessaire
        if (dt->hour >= 24)
        {
            dt->hour -= 24;                // Remise à zéro de l'heure
            dt->day += 1;                  // Incrémentation du jour
            dt->wday = (dt->wday + 1) % 7; // Incrémentation du jour de la semaine

            // Gestion du passage au mois suivant
            static const uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            uint8_t max_days = days_in_month[dt->month]; // Nombre de jours dans le mois courant

            // Ajustement pour février en année bissextile
            if (dt->month == 2 && ((dt->year % 4 == 0 && dt->year % 100 != 0) || dt->year % 400 == 0)) // Vérifie si c'est une année bissextile
            {
                max_days = 29; // Février a 29 jours en année bissextile
            }

            if (dt->day > max_days) // Si on dépasse la fin du mois
            {
                dt->day = 1;    // Premier jour du mois suivant
                dt->month += 1; // Mois suivant

                if (dt->month > 12) // Si on dépasse décembre
                {
                    dt->month = 1; // Janvier
                    dt->year += 1; // Année suivante
                }
            }
        }

        ESP01_LOG_DEBUG("NTP", "DST appliqué: %02d:xx -> %02d:xx", prev_hour, dt->hour); // Log du changement d'heure
        dt->dst = true;                                                                  // Indique que le DST est appliqué
    }
    else // Si le DST n'est pas actif
    {
        ESP01_LOG_DEBUG("NTP", "DST non appliqué (heure d'hiver)"); // Log si pas de DST
        dt->dst = false;                                            // Indique que le DST n'est pas appliqué
    }
}

/* ==================== FONCTIONS ONE-SHOT SIMPLES ==================== */

/**
 * @brief Formate la dernière date/heure NTP reçue dans un buffer utilisateur (langue FR/EN ou brute)
 * @param lang 'F' pour français, 'E' pour anglais, 0 ou '\0' pour la date brute.
 * @param buffer Buffer de sortie (si NULL, ne fait que parser la date brute dans une structure temporaire)
 * @param bufsize Taille du buffer
 * @retval ESP01_OK si succès, ESP01_FAIL sinon
 *
 * Si buffer == NULL, la fonction ne fait que parser la date brute et retourne ESP01_OK si parsing réussi.
 */
ESP01_Status_t esp01_ntp_format_last_datetime(char lang, char *buffer, size_t bufsize)
{
    ntp_datetime_t dt;                               // Structure pour stocker la date parsée
    const char *raw = esp01_ntp_get_last_datetime(); // Récupère la dernière date brute

    if (!raw || strlen(raw) == 0) // Vérifie la présence d'une date brute
    {
        ESP01_LOG_WARN("NTP", "Aucune date NTP brute à formatter"); // Log warning si non dispo
        return ESP01_FAIL;                                          // Retourne échec
    }

    if (esp01_parse_ntp_esp01(raw, &dt) != ESP01_OK) // Parsing de la date brute
    {
        ESP01_LOG_ERROR("NTP", "Parsing de la date brute échoué"); // Log d'erreur si parsing échoué
        return ESP01_FAIL;                                         // Retourne échec
    }

    // Appliquer DST si activé dans la config
    if (g_ntp_config.dst_enable)
        apply_dst(&dt); // Applique le DST si besoin

    // Si lang == 0 ou lang == '\0', retourne la date brute (non formatée)
    if (lang == 0)
    {
        // Si buffer non NULL, on copie la chaîne brute
        if (buffer && bufsize > 0)
        {
            strncpy(buffer, raw, bufsize - 1); // Copie sécurisée de la date brute
            buffer[bufsize - 1] = '\0';        // Ajout du terminateur
        }
        return ESP01_OK; // Succès
    }

    // Sinon, formatage FR/EN
    if (!buffer || bufsize < ESP01_NTP_DATETIME_BUF_SIZE) // Vérifie la validité du buffer
        return ESP01_INVALID_PARAM;                       // Retourne erreur paramètre

    if (lang == 'F') // Format français
    {
        ESP01_Status_t st = esp01_format_datetime_fr(&dt, buffer, bufsize); // Formate en français
        ESP01_LOG_DEBUG("NTP", "Date formatée FR : %s", buffer);            // Log la date formatée FR
        return st;                                                          // Retourne le statut
    }
    else if (lang == 'E') // Format anglais
    {
        ESP01_Status_t st = esp01_format_datetime_en(&dt, buffer, bufsize); // Formate en anglais
        ESP01_LOG_DEBUG("NTP", "Date formatée EN : %s", buffer);            // Log la date formatée EN
        return st;                                                          // Retourne le statut
    }
    else
    {
        ESP01_LOG_ERROR("NTP", "Langue non supportée : %c", lang); // Log d'erreur si langue invalide
        return ESP01_INVALID_PARAM;                                // Retourne erreur paramètre
    }
}
/* ==================== ACCÈS ET AFFICHAGE ==================== */

/**
 * @brief Affiche la configuration NTP courante (logs).
 */
void esp01_print_ntp_config(void)
{
    ESP01_LOG_DEBUG("NTP", "Affichage de la configuration NTP");                        // Log d'affichage
    ESP01_LOG_DEBUG("NTP", "Serveur : %s", g_ntp_config.server);                        // Affichage du serveur
    ESP01_LOG_DEBUG("NTP", "Fuseau horaire : %d", g_ntp_config.timezone);               // Affichage du fuseau horaire
    ESP01_LOG_DEBUG("NTP", "Période de synchro (s) : %d", g_ntp_config.period_s);       // Affichage de la période
    ESP01_LOG_DEBUG("NTP", "DST activé : %s", g_ntp_config.dst_enable ? "OUI" : "NON"); // Affichage de l'état DST
}

/**
 * @brief Affiche la dernière date/heure NTP reçue formatée (FR ou EN) dans les logs.
 * @param lang 'F' pour français, 'E' pour anglais (insensible à la casse).
 * @retval ESP01_OK si succès, ESP01_FAIL ou ESP01_INVALID_PARAM sinon.
 *
 * Cette fonction parse la dernière date brute reçue, applique le DST si activé,
 * puis formate et affiche la date dans la langue demandée via ESP01_LOG_DEBUG.
 * Si la date n'est pas disponible ou invalide, un warning est loggué.
 */
ESP01_Status_t esp01_ntp_print_last_datetime(char lang)
{
    const char *datetime_ntp = esp01_ntp_get_last_datetime();                                                 // Récupère la dernière date brute
    const char *msg_na = (lang == 'E' || lang == 'e') ? "NTP date not available" : "Date NTP non disponible"; // Message si non dispo
    const char *msg_inv = (lang == 'E' || lang == 'e') ? "Invalid NTP date" : "Date NTP invalide";            // Message si invalide

    if (!datetime_ntp || strlen(datetime_ntp) == 0) // Vérifie la présence d'une date
    {
        ESP01_LOG_WARN("NTP", "%s", msg_na); // Log warning si non dispo
        return ESP01_INVALID_PARAM;          // Retourne erreur paramètre
    }

    ntp_datetime_t dt; // Structure pour le parsing

    if (esp01_parse_ntp_esp01(datetime_ntp, &dt) == ESP01_OK) // Parsing de la date brute
    {
        if (g_ntp_config.dst_enable) // Si DST activé dans la config
        {
            apply_dst(&dt); // Applique le DST
            ESP01_LOG_DEBUG("NTP", "Date/heure après DST : %02d/%02d/%04d %02d:%02d:%02d (wday=%d, DST=%d)",
                            dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec, dt.wday, dt.dst); // Log la date après DST
        }

        char buffer[100] = {0}; // Buffer pour la date formatée

        if (lang == 'E' || lang == 'e') // Si anglais demandé
        {
            if (esp01_format_datetime_en(&dt, buffer, sizeof(buffer)) == ESP01_OK) // Formate en anglais
            {
                ESP01_LOG_DEBUG("NTP", "%s", buffer); // Log la date formatée EN
                return ESP01_OK;                      // Succès
            }
        }
        else // Sinon français par défaut
        {
            if (esp01_format_datetime_fr(&dt, buffer, sizeof(buffer)) == ESP01_OK) // Formate en français
            {
                ESP01_LOG_DEBUG("NTP", "%s", buffer); // Log la date formatée FR
                return ESP01_OK;                      // Succès
            }
        }
        ESP01_LOG_WARN("NTP", "%s", msg_inv); // Log warning si formatage échoué
        return ESP01_FAIL;                    // Retourne échec
    }
    else
    {
        ESP01_LOG_WARN("NTP", "%s", msg_inv); // Log warning si parsing échoué
        return ESP01_FAIL;                    // Retourne échec
    }
}

/**
 * @brief Retourne la dernière date/heure NTP brute reçue.
 * @return Pointeur sur la chaîne date/heure.
 */
const char *esp01_ntp_get_last_datetime(void)
{
    ESP01_LOG_DEBUG("NTP", "Récupération de la dernière date/heure NTP brute");            // Log de récupération
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
    ESP01_LOG_DEBUG("NTP", "Réinitialisation du flag de mise à jour NTP"); // Log de réinitialisation
    g_ntp_updated = 0;                                                     // Réinitialisation du flag
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
    VALIDATE_PARAM_VOID(dt);                             // Vérifie la validité du pointeur
    if (dt->wday > 6 || dt->month < 1 || dt->month > 12) // Vérifie la validité des champs
        return;
    static const char *jours_fr[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};                                                // Tableau des jours
    static const char *mois_fr[] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"}; // Tableau des mois
    ESP01_LOG_DEBUG("NTP", "%s %02d %s %04d à %02dh%02d:%02d%s",
                    jours_fr[dt->wday], dt->day, mois_fr[dt->month - 1], dt->year,
                    dt->hour, dt->min, dt->sec, dt->dst ? " (heure d'été)" : ""); // Affiche la date/heure formatée FR
}

/**
 * @brief Affiche une structure date/heure en anglais (logs).
 * @param dt Structure à afficher.
 */
void esp01_print_datetime_en(const ntp_datetime_t *dt)
{
    VALIDATE_PARAMS_VOID(dt && dt->wday <= 6 && dt->month >= 1 && dt->month <= 12);                                                                            // Vérifie la validité des champs
    static const char *jours_en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};                                            // Tableau des jours
    static const char *mois_en[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"}; // Tableau des mois
    int hour12 = dt->hour % 12;                                                                                                                                // Conversion en format 12h
    if (hour12 == 0)                                                                                                                                           // Si l'heure est 0h, on la convertit en 12h
        hour12 = 12;                                                                                                                                           // 12h pour le format 12h
    const char *ampm = (dt->hour < 12) ? "AM" : "PM";                                                                                                          // AM/PM
    ESP01_LOG_DEBUG("NTP", "%s, %02d %s %04d %02d:%02d:%02d %s%s",
                    jours_en[dt->wday], dt->day, mois_en[dt->month - 1], dt->year,
                    hour12, dt->min, dt->sec, ampm, dt->dst ? " (DST)" : ""); // Affiche la date/heure formatée EN
}

/**
 * @brief Formate une structure date/heure en français.
 * @param dt Structure à formater.
 * @param buffer Buffer de sortie.
 * @param size Taille du buffer.
 * @retval ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_format_datetime_fr(const ntp_datetime_t *dt, char *buffer, size_t size)
{
    VALIDATE_PARAM(dt && buffer && size >= ESP01_NTP_DATETIME_BUF_SIZE, ESP01_INVALID_PARAM);                                                                 // Vérifie la validité des paramètres
    static const char *jours_fr[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};                                                // Tableau des jours
    static const char *mois_fr[] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"}; // Tableau des mois
    if (dt->wday > 6 || dt->month < 1 || dt->month > 12)                                                                                                      // Vérifie la validité des champs
        return ESP01_FAIL;
    int n = snprintf(buffer, size, "%s %02d %s %04d à %02dh%02d:%02d%s",
                     jours_fr[dt->wday], dt->day, mois_fr[dt->month - 1], dt->year,
                     dt->hour, dt->min, dt->sec, dt->dst ? " (heure d'été)" : ""); // Formate la date/heure FR
    return (n > 0 && (size_t)n < size) ? ESP01_OK : ESP01_FAIL;                    // Retourne le statut
}

/**
 * @brief Formate une structure date/heure en anglais.
 * @param dt Structure à formater.
 * @param buffer Buffer de sortie.
 * @param size Taille du buffer.
 * @retval ESP01_OK si succès, ESP01_FAIL sinon.
 */
ESP01_Status_t esp01_format_datetime_en(const ntp_datetime_t *dt, char *buffer, size_t size)
{
    VALIDATE_PARAM(dt && buffer && size >= ESP01_NTP_DATETIME_BUF_SIZE, ESP01_INVALID_PARAM);                                                                  // Vérifie la validité des paramètres
    static const char *jours_en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};                                            // Tableau des jours
    static const char *mois_en[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"}; // Tableau des mois
    if (dt->wday > 6 || dt->month < 1 || dt->month > 12)                                                                                                       // Vérifie la validité des champs
        return ESP01_FAIL;
    int hour12 = dt->hour % 12; // Conversion en format 12h
    if (hour12 == 0)
        hour12 = 12;
    const char *ampm = (dt->hour < 12) ? "AM" : "PM"; // AM/PM
    int n = snprintf(buffer, size, "%s, %02d %s %04d %02d:%02d:%02d %s%s",
                     jours_en[dt->wday], dt->day, mois_en[dt->month - 1], dt->year,
                     hour12, dt->min, dt->sec, ampm, dt->dst ? " (DST)" : ""); // Formate la date/heure EN
    return (n > 0 && (size_t)n < size) ? ESP01_OK : ESP01_FAIL;                // Retourne le statut
}

bool esp01_parse_ntp_datetime(const char *datetime_str, ntp_datetime_t *dt)
{
    VALIDATE_PARAM(datetime_str && dt, false); // Vérifie la validité des paramètres
    // Format attendu: "Thu Jun 19 11:41:56 2025"
    char day_str[4], month_str[4]; // Buffers pour le jour et le mois
    int day, hour, min, sec, year; // Variables pour les valeurs entières
    int matched = sscanf(datetime_str, "%3s %3s %d %d:%d:%d %d",
                         day_str, month_str, &day, &hour, &min, &sec, &year); // Parsing de la chaîne
    if (matched != 7)                                                         // Vérifie si tous les champs ont été correctement extraits
        return false;                                                         // Retourne faux si le format est incorrect
    dt->day = day;                                                            // Affecte le jour
    dt->hour = hour;                                                          // Affecte l'heure
    dt->min = min;                                                            // Affecte les minutes
    dt->sec = sec;                                                            // Affecte les secondes
    dt->year = year;                                                          // Affecte l'année
    // Mois
    if (strncmp(month_str, "Jan", 3) == 0)      // Compare les 3 premiers caractères du mois
        dt->month = 1;                          // Affectation du mois
    else if (strncmp(month_str, "Feb", 3) == 0) // Compare pour février
        dt->month = 2;                          // Affectation du mois
    else if (strncmp(month_str, "Mar", 3) == 0) // Compare pour mars
        dt->month = 3;                          // Affectation du mois
    else if (strncmp(month_str, "Apr", 3) == 0) // Compare pour avril
        dt->month = 4;                          // Affectation du mois
    else if (strncmp(month_str, "May", 3) == 0) // Compare pour mai
        dt->month = 5;                          // Affectation du mois
    else if (strncmp(month_str, "Jun", 3) == 0) // Compare pour juin
        dt->month = 6;                          // Affectation du mois
    else if (strncmp(month_str, "Jul", 3) == 0) // Sinon si Juillet
        dt->month = 7;                          // Affecte 7
    else if (strncmp(month_str, "Aug", 3) == 0) // Sinon si Août
        dt->month = 8;                          // Affecte 8
    else if (strncmp(month_str, "Sep", 3) == 0) // Sinon si Septembre
        dt->month = 9;                          // Affecte 9
    else if (strncmp(month_str, "Oct", 3) == 0) // Sinon si Octobre
        dt->month = 10;                         // Affecte 10
    else if (strncmp(month_str, "Nov", 3) == 0) // Sinon si Novembre
        dt->month = 11;                         // Affecte 11
    else if (strncmp(month_str, "Dec", 3) == 0) // Sinon si Décembre
        dt->month = 12;                         // Affecte 12
    else                                        // Sinon (mois inconnu)
        return false;                           // Retourne faux

    // Conversion du jour texte vers numéro
    if (strncmp(day_str, "Sun", 3) == 0)      // Si le jour est Sunday
        dt->wday = 0;                         // Affecte 0
    else if (strncmp(day_str, "Mon", 3) == 0) // Sinon si Monday
        dt->wday = 1;                         // Affecte 1
    else if (strncmp(day_str, "Tue", 3) == 0) // Sinon si Tuesday
        dt->wday = 2;                         // Affecte 2
    else if (strncmp(day_str, "Wed", 3) == 0) // Sinon si Wednesday
        dt->wday = 3;                         // Affecte 3
    else if (strncmp(day_str, "Thu", 3) == 0) // Sinon si Thursday
        dt->wday = 4;                         // Affecte 4
    else if (strncmp(day_str, "Fri", 3) == 0) // Sinon si Friday
        dt->wday = 5;                         // Affecte 5
    else if (strncmp(day_str, "Sat", 3) == 0) // Sinon si Saturday
        dt->wday = 6;                         // Affecte 6
    else                                      // Sinon (jour inconnu)
        return false;                         // Retourne faux

    dt->dst = false; // Initialisation du flag DST à false (sera mis à jour plus tard)
    return true;     // Parsing réussi
}

/**
 * @brief Récupère la dernière date/heure NTP dans une structure.
 * @param dt Pointeur vers la structure de date/heure à remplir.
 * @return ESP01_OK si succès, ESP01_INVALID_PARAM si pointeur NULL, ESP01_FAIL si aucune date disponible.
 */
ESP01_Status_t esp01_ntp_get_last_datetime_struct(ntp_datetime_t *dt)
{
    if (!g_initialized) // Vérifie si le module NTP est initialisé
    {
        ESP01_LOG_DEBUG("NTP", "Demande de date NTP ignorée (synchro périodique non active)"); // Log si pas de synchro périodique
        return ESP01_OK;                                                                       // Retourne OK si pas de synchro périodique
    }
    if (!dt) // Vérifie si le pointeur de structure est valide
    {
        ESP01_LOG_ERROR("NTP", "Pointeur de structure date/heure NULL"); // Log d'erreur si pointeur invalide
        return ESP01_INVALID_PARAM;                                      // Retourne erreur si pointeur invalide
    }
    const char *datetime_ntp = esp01_ntp_get_last_datetime(); // Récupère la dernière date brute NTP
    if (!datetime_ntp || strlen(datetime_ntp) == 0)           // Vérifie si la date brute est disponible
    {
        ESP01_LOG_WARN("NTP", "Aucune date NTP disponible"); // Log warning si non dispo
        return ESP01_FAIL;                                   // Retourne échec si pas de date
    }
    return esp01_parse_ntp_esp01(datetime_ntp, dt); // Parse la date brute dans la structure fournie
}

// ========================= FIN DU MODULE =========================
