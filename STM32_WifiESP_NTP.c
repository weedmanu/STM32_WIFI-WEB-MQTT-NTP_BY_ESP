/**
 ******************************************************************************
 * @file    STM32_WifiESP_NTP.c
 * @author  Weedman
 * @brief   Gestion NTP pour ESP01 sur STM32 (implémentation)
 ******************************************************************************
 */

#include "STM32_WifiESP_NTP.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ==================== VARIABLES STATIQUES ==================== */

static esp01_ntp_config_t g_ntp_config = {
    .server = ESP01_NTP_DEFAULT_SERVER,
    .timezone = 0,
    .period_s = ESP01_NTP_DEFAULT_PERIOD_S};

static char g_last_datetime[ESP01_NTP_DATETIME_BUF_SIZE] = "";
static uint8_t g_ntp_updated = 0;

/* ==================== FONCTIONS NTP ==================== */

ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone, int sync_period_s)
{
    ESP01_LOG_INFO("NTP", "Configuration NTP : serveur=%s, timezone=%d, period=%d", ntp_server, timezone, sync_period_s);
    VALIDATE_PARAM(ntp_server && strlen(ntp_server) < ESP01_NTP_MAX_SERVER_LEN, ESP01_INVALID_PARAM);
    if (esp01_check_buffer_size(strlen(ntp_server), ESP01_NTP_MAX_SERVER_LEN - 1) != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Dépassement de la taille du buffer serveur NTP");
        ESP01_RETURN_ERROR("NTP_CONFIG", ESP01_BUFFER_OVERFLOW);
    }
    strncpy(g_ntp_config.server, ntp_server, ESP01_NTP_MAX_SERVER_LEN - 1);
    g_ntp_config.server[ESP01_NTP_MAX_SERVER_LEN - 1] = '\0';
    g_ntp_config.timezone = timezone;
    g_ntp_config.period_s = sync_period_s;
    ESP01_LOG_INFO("NTP", "Configuration NTP appliquée");
    return ESP01_OK;
}

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
            ESP01_LOG_WARN("NTP", "Tentative %d de récupération de l'heure NTP échouée", i + 1);
            HAL_Delay(1000);
        }
        ESP01_LOG_ERROR("NTP", "Impossible de récupérer l'heure NTP après 3 tentatives");
        ESP01_RETURN_ERROR("NTP_START_SYNC", ESP01_FAIL);
    }
    ESP01_LOG_INFO("NTP", "Synchronisation NTP périodique activée");
    return ESP01_OK;
}

void esp01_print_ntp_config(void)
{
    ESP01_LOG_INFO("NTP", "Affichage de la configuration NTP");
    ESP01_LOG_INFO("NTP", "Serveur : %s", g_ntp_config.server);
    ESP01_LOG_INFO("NTP", "Fuseau horaire : %d", g_ntp_config.timezone);
    ESP01_LOG_INFO("NTP", "Période de synchro (s) : %d", g_ntp_config.period_s);
}

void esp01_ntp_print_last_datetime_fr(void)
{
    ESP01_LOG_INFO("NTP", "Affichage de la dernière date/heure NTP (FR)");
    const char *datetime_ntp = esp01_ntp_get_last_datetime();
    ntp_datetime_fr_t dt;
    if (!datetime_ntp || strlen(datetime_ntp) == 0)
    {
        ESP01_LOG_WARN("NTP", "Date NTP non disponible");
        return;
    }
    if (esp01_parse_ntp_esp01(datetime_ntp, &dt) == ESP01_OK)
        esp01_print_datetime_fr(&dt);
    else
        ESP01_LOG_WARN("NTP", "Date NTP invalide");
}

void esp01_ntp_print_last_datetime_en(void)
{
    ESP01_LOG_INFO("NTP", "Affichage de la dernière date/heure NTP (EN)");
    const char *datetime_ntp = esp01_ntp_get_last_datetime();
    ntp_datetime_fr_t dt;
    if (!datetime_ntp || strlen(datetime_ntp) == 0)
    {
        ESP01_LOG_WARN("NTP", "NTP date not available");
        return;
    }
    if (esp01_parse_ntp_esp01(datetime_ntp, &dt) == ESP01_OK)
        esp01_print_datetime_en(&dt);
    else
        ESP01_LOG_WARN("NTP", "Invalid NTP date");
}

const char *esp01_ntp_get_last_datetime(void)
{
    ESP01_LOG_DEBUG("NTP", "Lecture de la dernière date/heure NTP : %s", g_last_datetime);
    return g_last_datetime;
}

uint8_t esp01_ntp_is_updated(void)
{
    ESP01_LOG_DEBUG("NTP", "Flag de mise à jour NTP : %d", g_ntp_updated);
    return g_ntp_updated;
}

void esp01_ntp_clear_updated_flag(void)
{
    ESP01_LOG_INFO("NTP", "Réinitialisation du flag de mise à jour NTP");
    g_ntp_updated = 0;
}

ESP01_Status_t esp01_parse_ntp_esp01(const char *datetime_ntp, ntp_datetime_fr_t *dt)
{
    ESP01_LOG_DEBUG("NTP", "Parsing de la date NTP : %s", datetime_ntp);
    VALIDATE_PARAM(datetime_ntp && dt, ESP01_INVALID_PARAM);

    char mois[8], jour[8];
    int res = sscanf(datetime_ntp, "%s %s %d %d:%d:%d %d",
                     jour, mois, &dt->day, &dt->hour, &dt->min, &dt->sec, &dt->year);
    if (res != 7)
    {
        ESP01_LOG_ERROR("NTP", "Format de date NTP invalide");
        ESP01_RETURN_ERROR("NTP_PARSE", ESP01_FAIL);
    }

    static const char *mois_en[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    dt->month = 0;
    for (int i = 0; i < 12; ++i)
    {
        if (strncmp(mois, mois_en[i], 3) == 0)
        {
            dt->month = i + 1;
            break;
        }
    }
    if (dt->month == 0)
    {
        ESP01_LOG_ERROR("NTP", "Mois NTP inconnu : %s", mois);
        ESP01_RETURN_ERROR("NTP_PARSE", ESP01_FAIL);
    }

    static const char *jours_en[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    dt->wday = -1;
    for (int i = 0; i < 7; ++i)
    {
        if (strncmp(jour, jours_en[i], 3) == 0)
        {
            dt->wday = i;
            break;
        }
    }
    if (dt->wday == -1)
    {
        ESP01_LOG_ERROR("NTP", "Jour NTP inconnu : %s", jour);
        ESP01_RETURN_ERROR("NTP_PARSE", ESP01_FAIL);
    }

    ESP01_LOG_DEBUG("NTP", "Parsing réussi : %02d/%02d/%04d %02d:%02d:%02d (wday=%d)",
                    dt->day, dt->month, dt->year, dt->hour, dt->min, dt->sec, dt->wday);
    return ESP01_OK;
}

void esp01_print_datetime_fr(const ntp_datetime_fr_t *dt)
{
    static const char *jours_fr[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
    static const char *mois_fr[] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"};
    ESP01_LOG_INFO("NTP", "%s %02d %s %04d à %02dh%02d:%02d",
                   jours_fr[dt->wday], dt->day, mois_fr[dt->month - 1], dt->year, dt->hour, dt->min, dt->sec);
}

void esp01_print_datetime_en(const ntp_datetime_fr_t *dt)
{
    static const char *jours_en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *mois_en[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    ESP01_LOG_INFO("NTP", "%s, %02d %s %04d %02d:%02d:%02d",
                   jours_en[dt->wday], dt->day, mois_en[dt->month - 1], dt->year, dt->hour, dt->min, dt->sec);
}

const esp01_ntp_config_t *esp01_get_ntp_config(void)
{
    ESP01_LOG_DEBUG("NTP", "Lecture de la configuration NTP");
    return &g_ntp_config;
}

ESP01_Status_t esp01_apply_ntp_config(uint8_t enable, int timezone, const char *server, int interval_s)
{
    char cmd[ESP01_MAX_CMD_BUF];
    char resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t st;

    ESP01_LOG_INFO("NTP", "Application de la configuration NTP : enable=%d, timezone=%d, server=%s", enable, timezone, server);
    snprintf(cmd, sizeof(cmd), "AT+CIPSNTPCFG=%d,%d,\"%s\"", enable, timezone, server);
    st = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Echec de la commande AT+CIPSNTPCFG (code=%d)", st);
        ESP01_RETURN_ERROR("NTP_APPLY_CONFIG", st);
    }
    ESP01_LOG_INFO("NTP", "Configuration NTP envoyée avec succès");
    return st;
}

ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize)
{
    ESP01_LOG_INFO("NTP", "Récupération de l'heure NTP...");
    VALIDATE_PARAM(datetime_buf && bufsize > 0, ESP01_INVALID_PARAM);

    if (esp01_check_buffer_size(24, bufsize - 1) != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Buffer trop petit pour la date NTP");
        ESP01_RETURN_ERROR("NTP_GET_TIME", ESP01_BUFFER_OVERFLOW);
    }

    char cmd[] = "AT+CIPSNTPTIME?";
    char response[ESP01_MAX_RESP_BUF] = {0};

    ESP01_Status_t st = esp01_send_raw_command_dma(cmd, response, sizeof(response), "OK", ESP01_TIMEOUT_SHORT);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Echec de la commande AT+CIPSNTPTIME? (code=%d)", st);
        ESP01_RETURN_ERROR("NTP_GET_TIME", st);
    }

    ESP01_LOG_DEBUG("NTP", "Réponse brute ESP01 :\n%s", response);

    st = esp01_parse_string_after(response, "+CIPSNTPTIME:", datetime_buf, bufsize);
    if (st != ESP01_OK)
    {
        ESP01_LOG_ERROR("NTP", "Impossible d'extraire la date NTP de la réponse ESP01");
        ESP01_RETURN_ERROR("NTP_GET_TIME", st);
    }

    char *ptr = datetime_buf;
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\"')
        ptr++;
    if (ptr != datetime_buf)
        memmove(datetime_buf, ptr, strlen(ptr) + 1);

    if (strlen(datetime_buf) < 8)
    {
        ESP01_LOG_ERROR("NTP", "Date NTP trop courte ou invalide : %s", datetime_buf);
        ESP01_RETURN_ERROR("NTP_GET_TIME", ESP01_FAIL);
    }

    ESP01_LOG_INFO("NTP", "Heure NTP récupérée : %s", datetime_buf);
    return ESP01_OK;
}
