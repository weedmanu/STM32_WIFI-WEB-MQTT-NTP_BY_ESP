/*
 * STM32_WifiESP_NTP.c
 *  Created on: Jun 8, 2025
 *      Author: weedm
 */

#include "STM32_WifiESP_NTP.h"
#include "STM32_WifiESP.h"
#include "STM32_WifiESP_Utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// ==================== VARIABLES STATIQUES ====================
static char g_ntp_server[64] = "pool.ntp.org";
static int g_ntp_timezone = 0;
static uint32_t g_ntp_interval_ms = 60000;
static uint8_t g_ntp_periodic_enabled = 0;
static bool g_ntp_print_fr = false;
static uint32_t last_sync = 0;

// Variables pour communication avec l'utilisateur
static char g_last_ntp_datetime[64] = "";
static volatile uint8_t g_last_ntp_updated = 0;

// ==================== API NTP ====================

ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone_offset, int sync_period_s)
{
	char resp[ESP01_MAX_RESP_BUF];
	ESP01_Status_t status;
	snprintf(resp, sizeof(resp), "AT+CIPSNTPCFG=1,%d,\"%s\"", timezone_offset, ntp_server);
	status = esp01_send_raw_command_dma(resp, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);
	if (status != ESP01_OK)
		return status;
	return ESP01_OK;
}

ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize)
{
	char resp[256];
	printf("[NTP][DEBUG] Envoi : AT+CIPSNTPTIME?\r\n");
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSNTPTIME?", resp, sizeof(resp), "OK", 2000);
	printf("[NTP][DEBUG] Réponse brute : %s\r\n", resp);

	if (status != ESP01_OK)
	{
		printf("[NTP][DEBUG] Erreur lors de la récupération NTP\r\n");
		return status;
	}

	char *start = strstr(resp, "+CIPSNTPTIME:");
	if (start)
	{
		start += strlen("+CIPSNTPTIME:");
		while (*start == ' ' || *start == '\t')
			start++;
		char *end = start;
		while (*end && *end != '\r' && *end != '\n')
			end++;
		size_t len = end - start;
		if (len < bufsize)
		{
			strncpy(datetime_buf, start, len);
			datetime_buf[len] = '\0';
			printf("[NTP][DEBUG] Date/heure récupérée : %s\r\n", datetime_buf);
			return ESP01_OK;
		}
	}

	printf("[NTP][DEBUG] Impossible de parser la date/heure NTP\r\n");
	if (bufsize > 0)
		datetime_buf[0] = '\0';
	return ESP01_FAIL;
}

// ==================== SYNCHRONISATION PERIODIQUE ====================

void esp01_ntp_start_periodic_sync(const char *ntp_server, int timezone_offset, uint32_t interval_seconds, bool print_fr)
{
	strncpy(g_ntp_server, ntp_server, sizeof(g_ntp_server) - 1);
	g_ntp_server[sizeof(g_ntp_server) - 1] = '\0';
	g_ntp_timezone = timezone_offset;
	g_ntp_interval_ms = interval_seconds * 1000;
	g_ntp_periodic_enabled = 1;
	g_ntp_print_fr = print_fr;
	last_sync = 0; // Force la première synchro dès le prochain appel à la tâche
}

void esp01_ntp_stop_periodic_sync(void)
{
	g_ntp_periodic_enabled = 0;
}

void esp01_ntp_periodic_task(void)
{
	uint32_t now = HAL_GetTick();

	if (!g_ntp_periodic_enabled || g_ntp_interval_ms == 0)
		return;

	if ((last_sync == 0) || (now - last_sync >= g_ntp_interval_ms))
	{
		char dbg[ESP01_MAX_DBG_BUF];
		snprintf(dbg, sizeof(dbg), "[NTP][DEBUG] Déclenchement synchro périodique NTP (intervalle %lus)", g_ntp_interval_ms / 1000);
		_esp_logln(dbg);

		char datetime[64];
		if (esp01_configure_ntp(g_ntp_server, g_ntp_timezone, 0) == ESP01_OK &&
			esp01_get_ntp_time(datetime, sizeof(datetime)) == ESP01_OK)
		{
			snprintf(dbg, sizeof(dbg), "[NTP][DEBUG] Synchro NTP OK : %s", datetime);
			_esp_logln(dbg);

			// Stocke la date/heure pour l'utilisateur
			strncpy(g_last_ntp_datetime, datetime, sizeof(g_last_ntp_datetime) - 1);
			g_last_ntp_datetime[sizeof(g_last_ntp_datetime) - 1] = '\0';
			g_last_ntp_updated = 1;
		}
		else
		{
			_esp_logln("[NTP][DEBUG] Erreur de mise à jour NTP");
			g_last_ntp_datetime[0] = '\0';
			g_last_ntp_updated = 1;
		}
		last_sync = now;
	}
}

// ==================== UTILITAIRES PARSING ====================

void esp01_parse_fr_local_datetime(const char *datetime_ntp, char *out, size_t out_size)
{
	const char *jours[] = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"};
	const char *mois[] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"};

	int day, hour, min, sec, year;
	char wday[4], month[4];
	int mon, wday_num = 0;

	char dbg[ESP01_MAX_DBG_BUF];
	snprintf(dbg, sizeof(dbg), "[NTP][DEBUG] Parsing pour affichage français : %s", datetime_ntp);
	_esp_logln(dbg);

	if (sscanf(datetime_ntp, "%3s %3s %d %d:%d:%d %d", wday, month, &day, &hour, &min, &sec, &year) == 7)
	{
		const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
		mon = (strstr(months, month) - months) / 3 + 1;

		struct tm t = {0};
		t.tm_year = year - 1900;
		t.tm_mon = mon - 1;
		t.tm_mday = day;
		t.tm_hour = hour;
		t.tm_min = min;
		t.tm_sec = sec;
		mktime(&t);
		wday_num = t.tm_wday;

		char debug_buf[ESP01_MAX_DBG_BUF];
		snprintf(debug_buf, sizeof(debug_buf), "[NTP][DEBUG] Résultat parsing FR : jour=%d, mois=%d, année=%d, heure=%d, min=%d, sec=%d, wday=%d",
				 day, mon, year, hour, min, sec, wday_num);
		_esp_logln(debug_buf);

		snprintf(out, out_size, "%s %d %s %d %02dh%02d", jours[wday_num], day, mois[mon - 1], year, hour, min);
	}
	else
	{
		_esp_logln("[NTP][DEBUG] Erreur de parsing date NTP pour affichage FR");
		if (out_size > 0)
			out[0] = '\0';
	}
}

void esp01_parse_local_datetime(const char *datetime_ntp, int timezone_offset, char *out, size_t out_size)
{
	int day, hour, min, sec, year;
	char wday[4], month[4];
	int mon;

	char dbg[ESP01_MAX_DBG_BUF];
	snprintf(dbg, sizeof(dbg), "[NTP][DEBUG] Parsing pour affichage local : %s", datetime_ntp);
	_esp_logln(dbg);

	if (sscanf(datetime_ntp, "%3s %3s %d %d:%d:%d %d", wday, month, &day, &hour, &min, &sec, &year) == 7)
	{
		const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
		mon = (strstr(months, month) - months) / 3 + 1;

		struct tm t = {0};
		t.tm_year = year - 1900;
		t.tm_mon = mon - 1;
		t.tm_mday = day;
		t.tm_hour = hour;
		t.tm_min = min;
		t.tm_sec = sec;
		mktime(&t);

		char debug_buf[ESP01_MAX_DBG_BUF];
		snprintf(debug_buf, sizeof(debug_buf), "[NTP][DEBUG] Résultat parsing LOCAL : %04d-%02d-%02d %02dh%02d (UTC%+d)",
				 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, timezone_offset);
		_esp_logln(debug_buf);

		snprintf(out, out_size, "%04d-%02d-%02d %02dh%02d (UTC%+d)",
				 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, timezone_offset);
	}
	else
	{
		_esp_logln("[NTP][DEBUG] Erreur de parsing date NTP pour affichage local");
		if (out_size > 0)
			out[0] = '\0';
	}
}

// ==================== AFFICHAGE UTILISATEUR ====================

void esp01_print_fr_local_datetime(const char *datetime_ntp)
{
	char fr_buf[64];
	esp01_parse_fr_local_datetime(datetime_ntp, fr_buf, sizeof(fr_buf));
	printf("[NTP][FR] %s\n", fr_buf);
}

void esp01_ntp_sync_and_print(int timezone_offset, bool print_fr, int sync_period_s)
{
	char datetime[64];

	printf("[NTP][DEBUG] Démarrage synchronisation NTP (UTC%+d)...\r\n", timezone_offset);

	if (esp01_configure_ntp("pool.ntp.org", timezone_offset, sync_period_s) != ESP01_OK)
	{
		printf("[NTP][DEBUG] Erreur configuration NTP\r\n");
		return;
	}

	if (esp01_get_ntp_time(datetime, sizeof(datetime)) != ESP01_OK)
	{
		printf("[NTP][DEBUG] Erreur ou délai dépassé pour la synchro NTP\r\n");
		return;
	}

	printf("[NTP] Heure locale (fuseau UTC%+d) : %s\r\n", timezone_offset, datetime);

	if (print_fr)
	{
		printf("[NTP][DEBUG] Affichage en français demandé\r\n");
		esp01_print_fr_local_datetime(datetime);
	}
}

void esp01_ntp_sync_once(const char *ntp_server, int timezone_offset, bool print_fr)
{
	char datetime[64];
	printf("[NTP][DEBUG] Démarrage synchronisation NTP (UTC%+d)...\r\n", timezone_offset);

	if (esp01_configure_ntp(ntp_server, timezone_offset, 1) != ESP01_OK)
	{
		printf("[NTP][DEBUG] Erreur configuration NTP\r\n");
		return;
	}

	if (esp01_get_ntp_time(datetime, sizeof(datetime)) != ESP01_OK)
	{
		printf("[NTP][DEBUG] Erreur ou délai dépassé pour la synchro NTP\r\n");
		return;
	}

	printf("[NTP] Heure locale (fuseau UTC%+d) : %s\r\n", timezone_offset, datetime);

	char fr_buf[64];
	char local_buf[64];
	esp01_parse_fr_local_datetime(datetime, fr_buf, sizeof(fr_buf));
	esp01_parse_local_datetime(datetime, timezone_offset, local_buf, sizeof(local_buf));
	if (print_fr)
		printf("[NTP][FR] %s\n", fr_buf);
	printf("[NTP][LOCAL] %s\n", local_buf);
}

const char *esp01_ntp_get_last_datetime(void) { return g_last_ntp_datetime; }
uint8_t esp01_ntp_is_updated(void) { return g_last_ntp_updated; }
void esp01_ntp_clear_updated_flag(void) { g_last_ntp_updated = 0; }

#if USE_RTC_FROM_NTP
int esp01_ntp_update_rtc(RTC_HandleTypeDef *hrtc, int timezone_offset)
{
	const char *datetime = esp01_ntp_get_last_datetime();
	if (!datetime || !datetime[0])
	{
		_esp_logln("[NTP][DEBUG] Pas de date NTP disponible pour mise à jour RTC");
		return -1;
	}

	int day, hour, min, sec, year;
	char wday[4], month[4];
	int mon;

	// Parsing du format NTP (ex: "Sun Jun  8 16:38:36 2025")
	if (sscanf(datetime, "%3s %3s %d %d:%d:%d %d", wday, month, &day, &hour, &min, &sec, &year) != 7)
	{
		_esp_logln("[NTP][DEBUG] Erreur parsing date NTP pour RTC");
		return -1;
	}

	// Conversion mois texte -> numéro
	const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
	char *p = strstr(months, month);
	if (!p)
	{
		_esp_logln("[NTP][DEBUG] Erreur parsing mois NTP pour RTC");
		return -1;
	}
	mon = (p - months) / 3 + 1;

	struct tm t = {0};
	t.tm_year = year - 1900;
	t.tm_mon = mon - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = min;
	t.tm_sec = sec;
	mktime(&t);

	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};

	sTime.Hours = t.tm_hour;
	sTime.Minutes = t.tm_min;
	sTime.Seconds = t.tm_sec;
	sTime.TimeFormat = RTC_HOURFORMAT12_AM;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;

	sDate.WeekDay = t.tm_wday ? t.tm_wday : 7; // 1=lundi, 7=dimanche
	sDate.Month = t.tm_mon + 1;
	sDate.Date = t.tm_mday;
	sDate.Year = t.tm_year % 100;

	if (HAL_RTC_SetTime(hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
	{
		_esp_logln("[NTP][DEBUG] HAL_RTC_SetTime a échoué");
		return -1;
	}
	if (HAL_RTC_SetDate(hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
	{
		_esp_logln("[NTP][DEBUG] HAL_RTC_SetDate a échoué");
		return -1;
	}

	_esp_logln("[NTP][DEBUG] RTC mis à jour depuis NTP");
	return 0;
}
#endif // USE_RTC_FROM_NTP