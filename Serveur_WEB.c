/* USER CODE BEGIN Includes */
#include <stdio.h>		   // Inclusion de la bibliothèque standard pour les entrées/sorties (pour printf)
#include "STM32_WifiESP.h" // Inclusion du fichier d'en-tête pour le driver ESP01
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_HTTP.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define SSID "freeman"				  // Nom du réseau WiFi auquel se connecter
#define PASSWORD "manu2612@SOSSO1008" // Mot de passe du réseau WiFi
#define LED_GPIO_PORT GPIOA
#define LED_GPIO_PIN GPIO_PIN_5
/* USER CODE END PD */

/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Tampon DMA pour la réception ESP01
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
// Redirige printf vers l'UART2 (console série)
int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF); // Envoie le caractère sur l'UART2
	return ch;											   // Retourne le caractère envoyé (pour compatibilité printf)
}

// ==================== Constantes HTML et CSS ====================

// --- Parties communes HTML ---
static const char HTML_DOC_START[] = "<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'>";
static const char HTML_TITLE_START[] = "<title>";
static const char HTML_TITLE_END_STYLE_START[] = "</title><style>";
static const char HTML_STYLE_END_HEAD_BODY_CARD_START[] = "</style></head><body><div class='card'>";
static const char HTML_CARD_END_BODY_END[] = "</div></body></html>";

// --- CSS Commun ---
static const char PAGE_CSS[] =
	"body{font-family:sans-serif;background:#222;text-align:center;margin:0;padding:0;}"
	".card{background:linear-gradient(135deg,#c8f7c5 0%,#fff9c4 50%,#ffd6d6 100%);margin:3em auto 0 auto;padding:2.5em 2em 2em 2em;border-radius:18px;box-shadow:0 4px 24px #0004;max-width:420px;display:flex;flex-direction:column;align-items:center;}"
	"h1{color:#2d3a1a;margin-top:0;margin-bottom:1.5em;}";

// Handler pour la page d'accueil "/"
void page_root(int conn_id, const http_parsed_request_t *request)
{
	// --- Page Root ("/") ---
	static const char PAGE_ROOT_TITLE[] = "Accueil STM32 Webserver";
	static const char CSS_PAGE_ROOT_SPECIFIC[] =
		"a.button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#388e3c;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #388e3c;}"
		"a.button.green{background:#28a745;border-color:#28a745;color:#fff;}"
		"a.button.yellow{background:#fbc02d;border-color:#fbc02d;color:#fff;}"
		"a.button.red{background:#d32f2f;border-color:#d32f2f;color:#fff;}"
		"a.button:hover{filter:brightness(1.15);}";
	static const char BODY_PAGE_ROOT[] =
		"<h1>Bienvenue sur le serveur web STM32 !</h1>"
		"<a class='button green' href='/led'>Contrôler la LED</a>"
		"<a class='button yellow' href='/testget'>Tester GET</a>"
		"<a class='button red' href='/status'>Statut</a>"
		"<a class='button red' href='/device'>Device</a>";

	if (!request)
		return;

	char html[2048];
	char *ptr = html;
	char *end_ptr = html + sizeof(html);

	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s", HTML_DOC_START, HTML_TITLE_START, PAGE_ROOT_TITLE, HTML_TITLE_END_STYLE_START);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_ROOT_SPECIFIC);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", BODY_PAGE_ROOT);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_CARD_END_BODY_END);

	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));
}

// --- Page LED ("/led") ---
static void page_led(int conn_id, const http_parsed_request_t *request)
{
	// --- Page LED ("/led") ---
	const char PAGE_LED_TITLE[] = "LED STM32";
	const char CSS_PAGE_LED_SPECIFIC[] =
		"form{margin:1em 0;}"
		"button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#388e3c;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #388e3c;}"
		"button.green{background:#28a745;border-color:#28a745;color:#fff;}"
		"button.red{background:#d32f2f;border-color:#d32f2f;color:#fff;}"
		"button:hover{filter:brightness(1.15);}"
		"a.button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#fbc02d;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #fbc02d;}"
		"a.button.yellow{background:#fbc02d;border-color:#fbc02d;color:#fff;}"
		"a.button:hover{filter:brightness(1.15);}";

	if (request && request->query_string[0])
	{
		if (strstr(request->query_string, "state=on"))
			HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET); // Allume la LED
		else if (strstr(request->query_string, "state=off"))
			HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET); // Éteint la LED
	}
	GPIO_PinState led = HAL_GPIO_ReadPin(LED_GPIO_PORT, LED_GPIO_PIN);

	char html[2048];
	char *ptr = html;
	char *end_ptr = html + sizeof(html);

	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s", HTML_DOC_START, HTML_TITLE_START, PAGE_LED_TITLE, HTML_TITLE_END_STYLE_START);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_LED_SPECIFIC);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);

	ptr += snprintf(ptr, end_ptr - ptr,
					"<h1>Contrôle de la LED (PA0)</h1>"
					"<p>État actuel : <b style='color:%s'>%s</b></p>"
					"<form method='get' action='/led'>"
					"<button class='green' name='state' value='on'>Allumer</button>"
					"<button class='red' name='state' value='off'>Éteindre</button>"
					"</form>"
					"<p><a class='button yellow' href='/'>Retour accueil</a></p>",
					(led == GPIO_PIN_SET) ? "#28a745" : "#dc3545",
					(led == GPIO_PIN_SET) ? "allumée" : "éteinte");

	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_CARD_END_BODY_END);

	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));
}

// --- Page Test GET ("/testget") ---
static void page_testget(int conn_id, const http_parsed_request_t *request)
{
	const char PAGE_TESTGET_TITLE[] = "Test GET";
	const char CSS_PAGE_TESTGET_SPECIFIC[] =
		"div.param{margin:0.7em auto;padding:0.7em 1em;background:#f8fff4;border-radius:8px;max-width:320px;box-shadow:0 1px 4px #e0f5d8;}"
		"span.paramname{color:#3a5d23;font-weight:bold;display:inline-block;width:110px;text-align:right;margin-right:0.5em;}"
		"span.paramval{color:#388e3c;font-weight:bold;}"
		".test-link{display:inline-block;background:#222;color:#ffe066;font-size:1.2em;padding:1em 2em;border-radius:10px;margin:1.5em 0 1em 0;box-shadow:0 2px 8px #e0f5d8;font-family:monospace;word-break:break-all;letter-spacing:1px;}"
		".test-label{font-size:1.1em;color:#388e3c;font-weight:bold;margin-bottom:0.3em;display:block;}"
		"a.button.green{display:inline-block;padding:1em 2em;margin:2em 0 0 0;background:#28a745;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #28a745;}"
		"a.button.green:hover{filter:brightness(1.15);}";

	char html[2048];
	char *ptr = html;
	char *end_ptr = html + sizeof(html);
	char ip[32] = "IP";
	esp01_get_current_ip(ip, sizeof(ip));

	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s", HTML_DOC_START, HTML_TITLE_START, PAGE_TESTGET_TITLE, HTML_TITLE_END_STYLE_START);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_TESTGET_SPECIFIC);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);

	ptr += snprintf(ptr, end_ptr - ptr,
					"<h1>Test GET</h1>"
					"<span class='test-label'>Testez dans votre navigateur :</span>"
					"<div class='test-link'>http://%s/testget?nom=Jean&age=42</div>"
					"<hr><b>Paramètres GET reçus :</b>",
					ip);

	char params_html[512] = "";
	int nb_lignes = 0, max_lignes = 8;

	if (request && request->query_string[0])
	{
		char query_copy[256];
		strncpy(query_copy, request->query_string, sizeof(query_copy) - 1);
		query_copy[sizeof(query_copy) - 1] = '\0';

		char *token = strtok(query_copy, "&");
		while (token && nb_lignes < max_lignes)
		{
			char *eq = strchr(token, '=');
			char row[128];
			if (eq)
			{
				*eq = '\0';
				const char *key = token;
				const char *val = eq + 1;
				snprintf(row, sizeof(row), "<div class='param'><span class='paramname'>%s :</span> <span class='paramval'>%s</span></div>", key, val);
			}
			else
			{
				snprintf(row, sizeof(row), "<div class='param'><span class='paramname'>%s :</span> <span class='paramval'></span></div>", token);
			}
			strncat(params_html, row, sizeof(params_html) - strlen(params_html) - 1);
			nb_lignes++;
			token = strtok(NULL, "&");
		}
	}
	else
	{
		strncpy(params_html, "<div style='margin:1em 0'><i>Aucun paramètre GET reçu</i></div>", sizeof(params_html) - 1);
		params_html[sizeof(params_html) - 1] = '\0';
	}

	strncat(html, params_html, sizeof(html) - strlen(html) - 1);
	strncat(html, "<br><a class='button green' href='/'>Retour accueil</a>", sizeof(html) - strlen(html) - 1);
	strncat(html, HTML_CARD_END_BODY_END, sizeof(html) - strlen(html) - 1);

	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));
}

// --- Page Status ("/status") ---
static void page_status(int conn_id, const http_parsed_request_t *request)
{
	const char PAGE_STATUS_TITLE[] = "Statut Serveur STM32";
	const char CSS_PAGE_STATUS_SPECIFIC[] =
		"table{margin:2em auto 1em auto;border-collapse:collapse;box-shadow:0 2px 8px #e0f5d8;background:#fff;}"
		"th,td{padding:0.4em 1em;border:1px solid #e0f5d8;font-size:1em;}"
		"th{background:#ffe066;color:#3a5d23;}"
		"a.button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#388e3c;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #388e3c;}"
		"a.button.green{background:#28a745;border-color:#28a745;color:#fff;";

	if (!request)
		return;

	char html[2048];
	char *ptr = html;
	char *end_ptr = html + sizeof(html);

	char ip[32] = "N/A";
	if (esp01_get_current_ip(ip, sizeof(ip)) != ESP01_OK)
		strncpy(ip, "Erreur", sizeof(ip) - 1);

	GPIO_PinState led = HAL_GPIO_ReadPin(LED_GPIO_PORT, LED_GPIO_PIN);

	const esp01_stats_t *stats = &g_stats;

	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s", HTML_DOC_START, HTML_TITLE_START, PAGE_STATUS_TITLE, HTML_TITLE_END_STYLE_START);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_STATUS_SPECIFIC);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);

	// Bloc 1 : Infos générales serveur
	ptr += snprintf(ptr, end_ptr - ptr,
					"<h1>Serveur STM32</h1>"
					"<table>"
					"<tr><th>IP serveur</th><td>%s</td></tr>"
					"<tr><th>Port serveur</th><td>%u</td></tr>"
					"<tr><th>LED</th><td style='color:%s'>%s</td></tr>"
					"<tr><th>Connexions actives</th><td>%d</td></tr>"
					"</table>",
					ip,
					g_server_port,
					(led == GPIO_PIN_SET) ? "#28a745" : "#dc3545",
					(led == GPIO_PIN_SET) ? "allumée" : "éteinte",
					esp01_get_active_connection_count());

	// Bloc 2 : Détail des connexions TCP
	ptr += snprintf(ptr, end_ptr - ptr,
					"<h2>Connexions TCP</h2>"
					"<table><tr><th>ID</th><th>Dernière activité (ms)</th><th>IP client</th><th>Port client</th></tr>");
	for (int i = 0; i < g_connection_count; ++i)
	{
		const connection_info_t *c = &g_connections[i];
		if (c->is_active)
		{
			ptr += snprintf(ptr, end_ptr - ptr,
							"<tr><td>%d</td><td>%lu</td><td>%s</td><td>%u</td></tr>",
							c->conn_id,
							(unsigned long)(HAL_GetTick() - c->last_activity),
							c->client_ip,
							c->client_port);
		}
	}
	ptr += snprintf(ptr, end_ptr - ptr, "</table>");

	// Bloc 3 : Statistiques serveur
	ptr += snprintf(ptr, end_ptr - ptr,
					"<h2>Statistiques HTTP</h2>"
					"<table>"
					"<tr><th>Requêtes reçues</th><td>%lu</td></tr>"
					"<tr><th>Réponses envoyées</th><td>%lu</td></tr>"
					"<tr><th>Succès</th><td>%lu</td></tr>"
					"<tr><th>Échecs</th><td>%lu</td></tr>"
					"<tr><th>Temps moyen (ms)</th><td>%lu</td></tr>"
					"</table>",
					(unsigned long)stats->total_requests,
					(unsigned long)stats->response_count,
					(unsigned long)stats->successful_responses,
					(unsigned long)stats->failed_responses,
					(unsigned long)stats->avg_response_time_ms);

	ptr += snprintf(ptr, end_ptr - ptr, "<a class='button green' href='/'>Accueil</a>");
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_CARD_END_BODY_END);

	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));
}

// Structure pour les informations système
typedef struct
{
	char at_version[64];
	char stm32_type[32];
	char wifi_mode[8];
	const char *wifi_ssid;
	uint16_t server_port;
	const char *multi_conn;
} system_info_t;

// Fonction pour collecter les informations système
static void collect_system_info(system_info_t *info)
{
	if (!info)
		return;

	// Firmware ESP01
	strcpy(info->at_version, "N/A");
	esp01_get_at_version(info->at_version, sizeof(info->at_version));

	// Type de carte STM32
	strcpy(info->stm32_type, "STM32 inconnue");
#if defined(STM32L4)
	strcpy(info->stm32_type, "STM32L4");
#elif defined(STM32F4)
	strcpy(info->stm32_type, "STM32F4");
#elif defined(STM32L1)
	strcpy(info->stm32_type, "STM32L1");
#elif defined(STM32F1)
	strcpy(info->stm32_type, "STM32F1");
#elif defined(STM32F7)
	strcpy(info->stm32_type, "STM32F7");
#elif defined(STM32H7)
	strcpy(info->stm32_type, "STM32H7");
#endif

	// Config WiFi - Utiliser directement la valeur de ESP01_WIFI_MODE_STA
	if (ESP01_WIFI_MODE_STA == 1)
	{
		strcpy(info->wifi_mode, "STA");
	}
	else
	{
		strcpy(info->wifi_mode, "AP");
	}

	info->wifi_ssid = SSID;

	// Config serveur
	info->server_port = g_server_port;
	info->multi_conn = (ESP01_MULTI_CONNECTION) ? "Oui" : "Non";
}

// Fonction pour rendre une section HTML avec table
static int render_html_section(char *buffer, size_t buffer_size, const char *title,
							   const char **labels, const char **values, int row_count)
{
	char *ptr = buffer;
	char *end_ptr = buffer + buffer_size;
	int written = 0;

	// Titre de la section
	written = snprintf(ptr, end_ptr - ptr, "<h%d>%s</h%d><table>",
					   (title[0] == 'I') ? 1 : 2, title, (title[0] == 'I') ? 1 : 2);
	ptr += written;

	// Lignes de la table
	for (int i = 0; i < row_count; i++)
	{
		int row_len = snprintf(ptr, end_ptr - ptr,
							   "<tr><th>%s</th><td>%s</td></tr>",
							   labels[i], values[i]);
		ptr += row_len;
		written += row_len;
	}

	// Fermeture de la table
	int end_len = snprintf(ptr, end_ptr - ptr, "</table>");
	written += end_len;

	return written;
}

// --- Page Infos Système & Réseau ("/device") ---
static void page_device(int conn_id, const http_parsed_request_t *request)
{
	const char PAGE_DEVICE_TITLE[] = "Infos Système & Réseau";
	const char CSS_PAGE_DEVICE_SPECIFIC[] =
		"table{margin:2em auto 1em auto;border-collapse:collapse;box-shadow:0 2px 8px #e0f5d8;background:#fff;}"
		"th,td{padding:0.4em 1em;border:1px solid #e0f5d8;font-size:1em;}"
		"th{background:#ffe066;color:#3a5d23;}"
		"a.button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#388e3c;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #388e3c;}"
		"a.button.green{background:#28a745;border-color:#28a745;color:#fff;";

	if (!request)
		return;

	// Collecter les informations système
	system_info_t sys_info;
	collect_system_info(&sys_info);

	// Préparer le buffer HTML
	char html[2048];
	char *ptr = html;
	char *end_ptr = html + sizeof(html);

	// En-tête HTML et CSS
	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s",
					HTML_DOC_START, HTML_TITLE_START,
					PAGE_DEVICE_TITLE, HTML_TITLE_END_STYLE_START);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_DEVICE_SPECIFIC);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);

	// Section Système
	const char *sys_labels[] = {"Firmware ESP01", "Carte STM32"};
	const char *sys_values[] = {sys_info.at_version, sys_info.stm32_type};
	ptr += render_html_section(ptr, end_ptr - ptr, "Informations Système",
							   sys_labels, sys_values, 2);

	// Section WiFi
	const char *wifi_labels[] = {"Mode", "SSID"};
	const char *wifi_values[] = {sys_info.wifi_mode, sys_info.wifi_ssid};
	ptr += render_html_section(ptr, end_ptr - ptr, "Configuration WiFi",
							   wifi_labels, wifi_values, 2);

	// Section Serveur
	char port_str[8];
	snprintf(port_str, sizeof(port_str), "%u", sys_info.server_port);

	const char *server_labels[] = {"Port", "Multi-connexion"};
	const char *server_values[] = {port_str, sys_info.multi_conn};
	ptr += render_html_section(ptr, end_ptr - ptr, "Configuration Serveur",
							   server_labels, server_values, 2);

	// Bouton retour
	ptr += snprintf(ptr, end_ptr - ptr, "<a class='button green' href='/'>Accueil</a>");
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_CARD_END_BODY_END);

	// Envoyer la réponse HTTP
	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));
}
/* USER CODE END 0 */

/* USER CODE BEGIN 2 */
HAL_Delay(1000);
printf("[ESP01] === Démarrage du programme ===\r\n");
HAL_Delay(500);

ESP01_Status_t status;

// 1. Initialisation du driver ESP01
printf("[ESP01] === Initialisation du driver ESP01 ===\r\n");
status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
printf("[ESP01] >>> Initialisation du driver ESP01 : %s\r\n", esp01_get_error_string(status));

// 2. Flush du buffer RX
printf("[ESP01] === Flush RX Buffer ===\r\n");
status = esp01_flush_rx_buffer(500);
printf("[ESP01] >>> Buffer UART/DMA vidé : %s\r\n", esp01_get_error_string(status));
HAL_Delay(100);

// 3. test de communication AT
printf("[ESP01] === Test de communication AT ===\r\n");
status = esp01_test_at();
printf("[ESP01] >>> Test AT : %s\r\n", esp01_get_error_string(status));

// 4. Test de version AT+GMR
printf("[ESP01] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
char at_version[128] = {0};
status = esp01_get_at_version(at_version, sizeof(at_version));
printf("[ESP01] >>> Version ESP01 : %s\r\n", at_version);

// 5. Connexion au réseau WiFi
printf("[WIFI] === Connexion au réseau WiFi ===\r\n");
status = esp01_connect_wifi_config(
	ESP01_WIFI_MODE_STA, // mode
	SSID,				 // ssid
	PASSWORD,			 // password
	true,				 // use_dhcp
	NULL,				 // ip
	NULL,				 // gateway
	NULL				 // netmask
);
printf("[WIFI] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(status));

// 6. Activation du mode multi-connexion ET démarrage du serveur web
printf("[WEB] === Activation multi-connexion + démarrage serveur web ===\r\n");
ESP01_Status_t server_status = esp01_start_server_config(
	true, // true = multi-connexion (CIPMUX=1)
	80	  // port du serveur web
);
if (server_status != ESP01_OK)
{
	printf("[WEB] >>> ERREUR: CIPMUX/CIPSERVER\r\n");
	Error_Handler();
}
else
{
	printf("[WEB] >>> Serveur web démarré sur le port 80\r\n");
}

// 7. Ajout des routes HTTP
printf("[WEB] === Ajout des routes HTTP ===\r\n");
esp01_clear_routes();
printf("[WEB] Ajout route /\r\n");
esp01_add_route("/", page_root);
printf("[WEB] Ajout route /status\r\n");
esp01_add_route("/status", page_status);
printf("[WEB] Ajout route /led\r\n");
esp01_add_route("/led", page_led);
printf("[WEB] Ajout route /testget\r\n");
esp01_add_route("/testget", page_testget);
printf("[WEB] Ajout route /device\r\n");
esp01_add_route("/device", page_device);

// 8. Vérification serveur ESP01
esp01_print_connection_status(); // Affiche l'état des connexions ESP01

// 9. Affichage de l'adresse IP
printf("[WEB] === Serveur Web prêt ===\r\n");
char ip[32];
if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
{
	printf("[WEB] >>> Connectez-vous à : http://%s/\r\n", ip);
}
else
{
	printf("[WIFI] >>> Impossible de récupérer l'IP STA\r\n");
	Error_Handler();
}
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
	esp01_process_requests();			  // Traite les requêtes HTTP entrantes
	esp01_cleanup_inactive_connections(); // Nettoyage automatique des connexions inactives
	HAL_Delay(10);						  // Petite pause pour éviter de surcharger le CPU
/* USER CODE END WHILE */

/* USER CODE BEGIN Error_Handler_Debug */
printf("ERREUR SYSTÈME DÉTECTÉE!\r\n"); // Affiche une erreur système
__disable_irq();						// Désactive les interruptions
while (1)
{
	// Boucle infinie en cas d'erreur
}
/* USER CODE END Error_Handler_Debug */

