/**
 ******************************************************************************
 * @file    STM32_WifiESP_NTP.c
 * @author  weedm
 * @version 1.1.0
 * @date    8 juin 2025
 * @brief   Implémentation de la gestion NTP pour le module ESP01 WiFi.
 *
 * @details
 * Ce fichier contient l'implémentation des fonctions pour gérer la
 * synchronisation de l'heure via NTP avec un module ESP01 connecté à un
 * microcontrôleur STM32. Il propose des fonctions haut niveau pour la
 * configuration du serveur NTP, la récupération et le parsing de la date/heure,
 * la gestion de la synchronisation périodique et l'affichage.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la bibliothèque STM32_WifiESP.h.
 *
 * @copyright
 * La licence de ce code est libre.
 ******************************************************************************
 */

// ==================== INCLUSIONS ====================
#include "STM32_WifiESP_NTP.h" // Header principal NTP
#include <stdio.h>			   // Pour printf, snprintf
#include <string.h>			   // Pour strncpy, memset, strstr, etc.
#include <time.h>			   // Pour structures de temps

// ==================== VARIABLES STATIQUES ====================
static char g_ntp_server[64] = "pool.ntp.org"; // Nom du serveur NTP utilisé
static int g_ntp_timezone = 0;				   // Décalage horaire (UTC)
static uint32_t g_ntp_interval_ms = 60000;	   // Intervalle de synchro périodique en ms
static uint8_t g_ntp_periodic_enabled = 0;	   // Flag d'activation de la synchro périodique
static bool g_ntp_print_fr = false;			   // Affichage en français ou non
static uint32_t last_sync = 0;				   // Timestamp de la dernière synchro

static char g_last_ntp_datetime[ESP01_MAX_RESP_BUF] = ""; // Dernière date/heure NTP reçue
static volatile uint8_t g_last_ntp_updated = 0;			  // Flag de mise à jour de la date/heure

// ==================== API NTP ====================

/**
 * @brief Configure le serveur NTP et le fuseau horaire.
 */
ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone_offset, int sync_period_s)
{
	char cmd[ESP01_MAX_CMD_BUF];   // Buffer pour la commande AT
	char resp[ESP01_MAX_RESP_BUF]; // Buffer pour la réponse

	snprintf(cmd, sizeof(cmd), "AT+CIPSNTPCFG=1,%d,\"%s\"", timezone_offset, ntp_server ? ntp_server : "pool.ntp.org"); // Prépare la commande AT
	_esp_login("[NTP] === Configuration NTP envoyée ===");																// Log étape importante
	_esp_login("[NTP] >>> %s", cmd);																					// Log la commande envoyée

	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM); // Envoie la commande AT
	if (status != ESP01_OK)
	{
		_esp_login("[NTP][DEBUG] >>> Erreur configuration NTP"); // Log erreur détaillée
		return status;
	}

	return status;
}

/**
 * @brief Récupère la date/heure NTP depuis l'ESP01.
 */
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize)
{
	char resp[ESP01_MAX_RESP_BUF]; // Buffer pour la réponse
	memset(resp, 0, sizeof(resp)); // Vide le buffer

	_esp_login("[NTP] === Lecture de l'heure NTP (AT+CIPSNTPTIME?) ===");												   // Log étape importante
	_esp_login("[NTP][DEBUG] Lecture de l'heure NTP (AT+CIPSNTPTIME?)");												   // Log debug détaillée
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSNTPTIME?", resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM); // Envoie la commande AT
	if (status != ESP01_OK)
	{
		_esp_login("[NTP][DEBUG] Erreur ou délai dépassé pour la lecture NTP"); // Log erreur détaillée
		return status;
	}

	// Log la réponse brute (tronquée si trop longue)
	_esp_login("[NTP][DEBUG] >>> Réponse brute NTP : %.80s", resp);

	char *ptr = strstr(resp, "+CIPSNTPTIME:"); // Cherche la ligne contenant la date/heure
	if (ptr)
	{
		ptr += strlen("+CIPSNTPTIME:"); // Avance après le préfixe
		while (*ptr == ' ')
			ptr++; // Saute les espaces

		char *endl = strpbrk(ptr, "\r\n");						// Cherche la fin de la ligne
		size_t len = endl ? (size_t)(endl - ptr) : strlen(ptr); // Calcule la longueur

		if (len > 0 && len < bufsize) // Si la longueur est valide
		{
			strncpy(datetime_buf, ptr, len); // Copie la date/heure dans le buffer
			datetime_buf[len] = '\0';		 // Termine la chaîne
		}
		else
		{
			datetime_buf[0] = '\0'; // Vide le buffer si erreur
		}
	}
	else
	{
		datetime_buf[0] = '\0'; // Vide le buffer si pas trouvé
	}

	return ESP01_OK;
}

// ==================== SYNCHRONISATION PERIODIQUE ====================

/**
 * @brief Démarre la synchronisation NTP périodique.
 */
void esp01_ntp_start_periodic_sync(const char *ntp_server, int timezone_offset, uint32_t interval_seconds, bool print_fr)
{
	strncpy(g_ntp_server, ntp_server, sizeof(g_ntp_server) - 1); // Copie le nom du serveur NTP
	g_ntp_server[sizeof(g_ntp_server) - 1] = '\0';				 // Termine la chaîne
	g_ntp_timezone = timezone_offset;							 // Stocke le fuseau horaire
	g_ntp_interval_ms = interval_seconds * 1000;				 // Convertit l'intervalle en ms
	g_ntp_periodic_enabled = 1;									 // Active la synchro périodique
	g_ntp_print_fr = print_fr;									 // Stocke le mode d'affichage
	last_sync = 0;												 // Force la première synchro dès le prochain appel à la tâche
}

/**
 * @brief Arrête la synchronisation NTP périodique.
 */
void esp01_ntp_stop_periodic_sync(void)
{
	g_ntp_periodic_enabled = 0; // Désactive la synchro périodique
}

/**
 * @brief Tâche à appeler régulièrement pour gérer la synchro périodique.
 */
void esp01_ntp_periodic_task(void)
{
	static uint32_t last_call = 0; // Dernier appel de la fonction
	uint32_t now = HAL_GetTick();  // Temps courant

	// Vérifie toutes les 1s si une synchro doit être faite
	if (now - last_call >= 1000)
	{
		last_call = now;
		if (!g_ntp_periodic_enabled || g_ntp_interval_ms == 0)
			return;

		if ((last_sync == 0) || (now - last_sync >= g_ntp_interval_ms))
		{
			_esp_login("[NTP] === Synchronisation NTP périodique ==="); // Log étape importante
			char datetime_buf[ESP01_MAX_RESP_BUF] = {0};				// Buffer pour la date/heure
			if (esp01_get_ntp_time(datetime_buf, sizeof(datetime_buf)) == ESP01_OK &&
				strlen(datetime_buf) > 0 && strstr(datetime_buf, "1970") == NULL)
			{
				strncpy(g_last_ntp_datetime, datetime_buf, sizeof(g_last_ntp_datetime) - 1); // Copie la date/heure
				g_last_ntp_datetime[sizeof(g_last_ntp_datetime) - 1] = '\0';				 // Termine la chaîne
				g_last_ntp_updated = 1;														 // Indique qu'une nouvelle date/heure est dispo
			}
			last_sync = now; // Met à jour le dernier timestamp de synchro
		}
	}
}

// ==================== UTILITAIRES PARSING ====================

/**
 * @brief Parse une date/heure NTP et la formate en français.
 */
char *esp01_parse_fr_local_datetime(const char *datetime_ntp, char *out, size_t out_size)
{
	const char *jours[] = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"};
	const char *mois[] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"};
	ntp_datetime_t dt;
	_esp_login("[NTP][DEBUG] Chaîne à parser : '%s'", datetime_ntp); // Log la chaîne à parser
	if (esp01_parse_ntp_datetime(datetime_ntp, &dt) == ESP01_OK)
		snprintf(out, out_size, "%s %d %s %d %02dh%02d:%02d", jours[dt.wday_num], dt.day, mois[dt.mon], dt.year, dt.hour, dt.min, dt.sec);
	else if (out_size > 0)
		out[0] = '\0';
	return out;
}

/**
 * @brief Parse une date/heure NTP et la formate en anglais/local.
 */
char *esp01_parse_local_datetime(const char *datetime_ntp, int timezone_offset, char *out, size_t out_size)
{
	const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
	const char *months[] = {"January", "February", "March", "April", "May", "June",
							"July", "August", "September", "October", "November", "December"};
	ntp_datetime_t dt;
	_esp_login("[NTP][DEBUG] Chaîne à parser : '%s'", datetime_ntp); // Log la chaîne à parser
	if (esp01_parse_ntp_datetime(datetime_ntp, &dt) == ESP01_OK)
		snprintf(out, out_size, "%s %d %s %d %02dh%02d:%02d", days[dt.wday_num], dt.day, months[dt.mon], dt.year, dt.hour, dt.min, dt.sec);
	else if (out_size > 0)
		out[0] = '\0';
	return out;
}

// ==================== AFFICHAGE UTILISATEUR ====================

/**
 * @brief Affiche la date/heure NTP en français.
 */
void esp01_print_fr_local_datetime(const char *datetime_ntp)
{
	char fr_buf[ESP01_MAX_RESP_BUF];									 // Buffer pour la date/heure formatée
	esp01_parse_fr_local_datetime(datetime_ntp, fr_buf, sizeof(fr_buf)); // Parse et formate la date/heure
	if (fr_buf[0])
	{
		_esp_login("[NTP][DEBUG] >>> %s", fr_buf); // Log la date/heure formatée
	}
	else
	{
		_esp_login("[NTP][DEBUG] >>> Erreur de parsing"); // Log erreur de parsing
	}
}

/**
 * @brief Affiche la date/heure NTP en anglais/local.
 */
void esp01_print_local_datetime(const char *datetime_ntp, int timezone_offset)
{
	char buf[ESP01_MAX_RESP_BUF];												 // Buffer pour la date/heure formatée
	esp01_parse_local_datetime(datetime_ntp, timezone_offset, buf, sizeof(buf)); // Parse et formate la date/heure
	_esp_login("[NTP][DEBUG] Date/Heure (UTC+%d): %s", timezone_offset, buf);	 // Log la date/heure formatée
}

/**
 * @brief Synchronise et affiche la date/heure NTP (one-shot ou périodique).
 */
void esp01_ntp_sync_and_print(int timezone_offset, bool print_fr, int sync_period_s)
{
	char datetime[ESP01_MAX_RESP_BUF]; // Buffer pour la date/heure
	int retry = 0, max_retry = 10;	   // Nombre de tentatives max
	int period = sync_period_s;		   // Période de synchro

	_esp_login("[NTP] === Démarrage synchronisation NTP (UTC+%d) ===", timezone_offset); // Log début synchro

	if (esp01_configure_ntp("fr.pool.ntp.org", timezone_offset, period) != ESP01_OK) // Configure le NTP
	{
		_esp_login("[NTP][DEBUG] >>> Erreur configuration NTP"); // Log erreur détaillée
		return;
	}

	if (period > 0) // Si synchro périodique demandée
	{
		esp01_ntp_start_periodic_sync("fr.pool.ntp.org", timezone_offset, period, print_fr); // Démarre la synchro périodique
		_esp_login("[NTP] === Synchro périodique activée (%ds) ===", period);				 // Log activation périodique
		return;
	}

	HAL_Delay(1000); // Laisse le temps à l'ESP de se synchroniser

	while (retry < max_retry) // Boucle de tentatives de lecture NTP
	{
		if (esp01_get_ntp_time(datetime, sizeof(datetime)) == ESP01_OK) // Récupère la date/heure
		{
			if (strstr(datetime, "1970") == NULL && strlen(datetime) > 0) // Vérifie la validité
				break;
		}
		HAL_Delay(2000); // Attend avant de réessayer
		retry++;
	}

	if (retry == max_retry) // Si toutes les tentatives ont échoué
	{
		_esp_login("[NTP][DEBUG] Erreur ou délai dépassé pour la synchro NTP"); // Log erreur détaillée
		return;
	}

	_esp_login("[NTP][DEBUG] Heure locale (fuseau UTC%+d) : %s", timezone_offset, datetime); // Log la date/heure brute

	if (print_fr) // Si affichage en français demandé
	{
		_esp_login("[NTP][DEBUG] Affichage en français demandé"); // Log info
		esp01_print_fr_local_datetime(datetime);				  // Affiche en FR
	}
	else
	{
		esp01_print_local_datetime(datetime, timezone_offset); // Affiche en EN/local
	}
}

// ==================== ACCES DONNEES SYNCHRO ====================

/**
 * @brief Retourne la dernière date/heure NTP reçue.
 */
const char *esp01_ntp_get_last_datetime(void) { return g_last_ntp_datetime; }

/**
 * @brief Indique si la date/heure a été mise à jour.
 */
uint8_t esp01_ntp_is_updated(void) { return g_last_ntp_updated; }

/**
 * @brief Réinitialise le flag de mise à jour.
 */
void esp01_ntp_clear_updated_flag(void) { g_last_ntp_updated = 0; }

// ==================== PARSING DE LA CHAINE NTP ====================

/**
 * @brief Parse la chaîne brute NTP ("Mon Jun  9 21:18:06 2025") et remplit la structure.
 */
ESP01_Status_t esp01_parse_ntp_datetime(const char *datetime_ntp, ntp_datetime_t *dt)
{
	char wday[4] = "", month[4] = ""; // Buffers pour le jour et le mois
	int parsed = sscanf(datetime_ntp, "%3s %3s %d %d:%d:%d %d",
						wday, month, &dt->day, &dt->hour, &dt->min, &dt->sec, &dt->year); // Parse la chaîne

	wday[3] = '\0';	 // Sécurise la terminaison
	month[3] = '\0'; // Sécurise la terminaison

	_esp_login("[NTP][DEBUG] parsed=%d, day=%d, hour=%d, min=%d, sec=%d, year=%d",
			   parsed, dt->day, dt->hour, dt->min, dt->sec, dt->year); // Log parsing
	_esp_login("[NTP][DEBUG] month='%s' (hex: %02X %02X %02X %02X)",
			   month, month[0], month[1], month[2], month[3]); // Log mois
	_esp_login("[NTP][DEBUG] dt.mon=%d", dt->mon);			   // Log numéro de mois

	if (parsed != 7 || dt->year <= 1970)
		return ESP01_FAIL; // Retourne erreur si parsing invalide

	// Conversion du mois (anglais -> numéro)
	const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
	char *p = strstr(months, month);
	if (!p)
		return ESP01_FAIL;
	dt->mon = (p - months) / 3;

	// Conversion du jour de la semaine (anglais -> numéro)
	if (!strcmp(wday, "Sun"))
		dt->wday_num = 0;
	else if (!strcmp(wday, "Mon"))
		dt->wday_num = 1;
	else if (!strcmp(wday, "Tue"))
		dt->wday_num = 2;
	else if (!strcmp(wday, "Wed"))
		dt->wday_num = 3;
	else if (!strcmp(wday, "Thu"))
		dt->wday_num = 4;
	else if (!strcmp(wday, "Fri"))
		dt->wday_num = 5;
	else if (!strcmp(wday, "Sat"))
		dt->wday_num = 6;
	else
		dt->wday_num = 0;

	return ESP01_OK; // Retourne OK si parsing réussi
}
