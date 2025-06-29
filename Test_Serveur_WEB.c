/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : Test_Serveur_WEB.c
 * @brief          : Programme de test du serveur web embarqué avec STM32 et ESP01
 ******************************************************************************
 * @details
 * Ce programme permet de tester un serveur web embarqué sur un STM32 connecté à
 * un module ESP01. Il implémente les fonctionnalités suivantes :
 *
 * - Initialisation du driver ESP01 et connexion au réseau WiFi
 * - Configuration et démarrage d'un serveur web multi-connexions sur le port 80
 * - Gestion des routes HTTP avec plusieurs pages :
 *   - Page d'accueil (/) : menu principal avec liens vers les autres pages
 *   - Page LED (/led) : contrôle de la LED embarquée
 *   - Page Test GET (/testget) : démonstration de traitement des paramètres GET
 *   - Page Statut (/status) : statistiques et informations de connexion
 *   - Page Device (/device) : informations système et réseau
 * - Génération dynamique de contenu HTML/CSS responsive
 * - Traitement des requêtes HTTP entrantes
 * - Affichage des statistiques de connexion
 *
 * Configuration matérielle :
 * - UART1 : Communication avec le module ESP01
 *   - Mode : Half-duplex
 *   - DMA RX : Mode circulaire (buffer continu)
 *
 * - UART2 : Console série avec l'ordinateur
 *   - Mode : Full-duplex
 *   - Affichage des résultats via printf redirigé
 *
 * - LED : LED embarquée sur la carte (PA5)
 *   - Contrôlable via l'interface web (/led)
 *
 * @note
 * - Nécessite les modules STM32_WifiESP, STM32_WifiESP_WIFI et STM32_WifiESP_HTTP
 * - Compatible avec les modules ESP8266 (ESP01, ESP01S, etc.)
 * - Baudrate par défaut: 115200 bps
 * - Définir les constantes SSID/PASSWORD pour votre réseau WiFi
 * - Interface web accessible à l'adresse http://[adresse_ip_module]/
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>				// Pour printf, snprintf, etc.
#include "STM32_WifiESP.h"		// Fonctions du driver ESP01
#include "STM32_WifiESP_WIFI.h" // Fonctions WiFi haut niveau
#include "STM32_WifiESP_HTTP.h" // Fonctions HTTP haut niveau

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SSID "XXXXXXXX"				  // Nom du réseau WiFi
#define PASSWORD "XXXXXXXXXXXXXXXXXX" // Mot de passe du réseau WiFi
#define LED_GPIO_PORT GPIOA			  // Port GPIO de la LED
#define LED_GPIO_PIN GPIO_PIN_5		  // Pin GPIO de la LED
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Tampon DMA pour la réception ESP01
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ==================== Redirection printf UART2 ====================
// Cette fonction permet de rediriger la sortie standard (printf)
// vers l'UART2, afin d'afficher les messages sur la console série du PC.
// Elle est appelée automatiquement par printf pour chaque caractère à envoyer.
//
// Paramètre :
//   ch : caractère à transmettre
// Retour :
//   Le caractère transmis (pour compatibilité avec printf)
int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF); // Envoie le caractère sur UART2 (console PC)
	return ch;											   // Retourne le caractère envoyé
}

// ==================== Constantes HTML et CSS ====================
// Ces constantes facilitent la génération dynamique de pages web embarquées.
// Elles permettent de construire des pages HTML complètes avec un style cohérent et responsive.
//
// --- Parties communes HTML ---
// Début du document HTML, balises d'ouverture et en-tête
static const char HTML_DOC_START[] = "<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'>";	 // Début du document HTML
static const char HTML_TITLE_START[] = "<title>";													 // Début de la balise title
static const char HTML_TITLE_END_STYLE_START[] = "</title><style>";									 // Fin de la balise title et début du style
static const char HTML_STYLE_END_HEAD_BODY_CARD_START[] = "</style></head><body><div class='card'>"; // Fin du style, début du body et de la card
static const char HTML_CARD_END_BODY_END[] = "</div></body></html>";								 // Fin de la card, du body et du document HTML

// --- CSS Commun ---
// Feuille de style commune à toutes les pages générées
static const char PAGE_CSS[] =
	"body{font-family:sans-serif;background:#222;text-align:center;margin:0;padding:0;}"
	".card{background:linear-gradient(135deg,#c8f7c5 0%,#fff9c4 50%,#ffd6d6 100%);margin:3em auto 0 auto;padding:2.5em 2em 2em 2em;border-radius:18px;box-shadow:0 4px 24px #0004;max-width:420px;display:flex;flex-direction:column;align-items:center;}"
	"h1{color:#2d3a1a;margin-top:0;margin-bottom:1.5em;}";

// --- Page Accueil ("/") ---
// Cette fonction gère la page d'accueil du serveur web embarqué.
// Elle affiche un menu principal avec des liens vers les différentes fonctionnalités (LED, test GET, statut, device).
// Le HTML est généré dynamiquement avec un style responsive et des boutons colorés.
static void page_root(int conn_id, const http_parsed_request_t *req)
{
	if (!req)																// Vérifie si la requête est valide
		return;																// Si non, on sort de la fonction
	printf("[TEST][INFO] Entrée dans page_root (conn_id=%d)\r\n", conn_id); // Affiche l'entrée dans la page d'accueil

	static const char PAGE_ROOT_TITLE[] = "Accueil STM32 Webserver"; // Titre de la page d'accueil
	static const char CSS_PAGE_ROOT_SPECIFIC[] =					 // CSS spécifique à la page d'accueil
		"a.button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#388e3c;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #388e3c;}"
		"a.button.green{background:#28a745;border-color:#28a745;color:#fff;}"
		"a.button.yellow{background:#fbc02d;border-color:#fbc02d;color:#fff;}"
		"a.button.red{background:#d32f2f;border-color:#d32f2f;color:#fff;}"
		"a.button:hover{filter:brightness(1.15);}";
	static const char BODY_PAGE_ROOT[] = // Corps de la page d'accueil
		"<h1>Bienvenue sur le serveur web STM32 !</h1>"
		"<a class='button green' href='/led'>Contrôler la LED</a>"
		"<a class='button yellow' href='/testget'>Tester GET</a>"
		"<a class='button red' href='/status'>Statut</a>"
		"<a class='button red' href='/device'>Device</a>";

	char html[2048];					 // Tampon pour générer la page HTML
	html[0] = '\0';						 // Initialise le tampon à vide
	char *ptr = html;					 // Pointeur pour écrire dans le tampon HTML
	char *end_ptr = html + sizeof(html); // Pointeur de fin pour éviter les débordements

	if (esp01_check_buffer_size(strlen(ptr) + 256, end_ptr - ptr) != ESP01_OK) // Vérifie si le tampon est assez grand
		return;

	// Génération du contenu HTML
	// Utilise snprintf pour écrire dans le tampon HTML de manière sécurisée
	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s", HTML_DOC_START, HTML_TITLE_START, PAGE_ROOT_TITLE, HTML_TITLE_END_STYLE_START); // Début du document HTML, titre et début du style
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);																			// Ajoute le CSS commun
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_ROOT_SPECIFIC);																// Ajoute le CSS spécifique à la page d'accueil
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);													// Fin du style, début du body et de la card
	ptr += snprintf(ptr, end_ptr - ptr, "%s", BODY_PAGE_ROOT);																		// Ajoute le corps de la page d'accueil
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_CARD_END_BODY_END);																// Fin de la card, du body et du document HTML

	printf("[TEST][INFO] Sortie de page_root, réponse envoyée sur conn_id=%d, taille=%d\r\n", conn_id, (int)strlen(html)); // Affiche la taille de la réponse
	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));								   // Envoie la page
}

// --- Page LED ("/led") ---
// Cette fonction gère la page de contrôle de la LED embarquée.
// Elle affiche dynamiquement l'état actuel de la LED (allumée/éteinte) et propose un formulaire web pour changer son état.
// Les paramètres GET reçus dans l'URL permettent d'allumer ou d'éteindre la LED à distance via l'interface web.
// Un bouton permet de revenir à la page d'accueil.
static void page_led(int conn_id, const http_parsed_request_t *req)
{
	if (!req)
		return;
	printf("[TEST][INFO] Entrée dans page_led (conn_id=%d)\r\n", conn_id);

	const char PAGE_LED_TITLE[] = "LED STM32";
	const char CSS_PAGE_LED_SPECIFIC[] =
		"form{margin:1em 0;}"
		"button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#388e3c;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #388e3c;}"
		"button.green{background:#28a745;border-color:#28a745;color:#fff;}"
		"button.red{background:#d32f2f;border-color:#d32f2f;color:#fff;}"
		"button:hover{filter:brightness(1.15);}"
		"a.button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#fbc02d;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #fbc02d;}"
		"a.button.yellow{background:#fbc02d;border-color:#fbc02d;color:#fff;}";
	static const char BODY_PAGE_LED[] =
		"<h1>Contrôle de la LED</h1>"
		"<p>État actuel : <b style='color:%s'>%s</b></p>"
		"<form method='get' action='/led'>"
		"<button class='green' name='state' value='on'>Allumer</button>"
		"<button class='red' name='state' value='off'>Éteindre</button>"
		"</form>"
		"<p><a class='button yellow' href='/'>Retour accueil</a></p>";

	// Traitement des paramètres GET pour contrôler la LED
	if (req && req->query_string[0]) // Vérifie si des paramètres GET sont présents dans la requête
	{
		if (strstr(req->query_string, "state=on"))							// Si le paramètre "state=on" est trouvé dans la requête
			HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);	// Allume la LED
		else if (strstr(req->query_string, "state=off"))					// Si le paramètre "state=off" est trouvé dans la requête
			HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET); // Éteint la LED
	}
	GPIO_PinState led = HAL_GPIO_ReadPin(LED_GPIO_PORT, LED_GPIO_PIN); // Lit l'état actuel de la LED

	char html[2048];					 // Tampon pour générer la page HTML
	char *ptr = html;					 // Pointeur pour écrire dans le tampon HTML
	char *end_ptr = html + sizeof(html); // Pointeur de fin pour éviter les débordements

	if (esp01_check_buffer_size(strlen(ptr) + 256, end_ptr - ptr) != ESP01_OK) // Vérifie si le tampon est assez grand
		return;																   // Si non, on sort de la fonction

	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s", HTML_DOC_START, HTML_TITLE_START, PAGE_LED_TITLE, HTML_TITLE_END_STYLE_START);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_LED_SPECIFIC);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);

	ptr += snprintf(ptr, end_ptr - ptr,
					BODY_PAGE_LED,
					(led == GPIO_PIN_SET) ? "#28a745" : "#dc3545",
					(led == GPIO_PIN_SET) ? "allumée" : "éteinte");

	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_CARD_END_BODY_END);

	printf("[TEST][INFO] Sortie de page_led, réponse envoyée sur conn_id=%d, taille=%d\r\n", conn_id, (int)strlen(html));
	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));
}

// --- Page Test GET ("/testget") ---
// Cette fonction permet de tester la récupération et l'affichage des paramètres GET passés dans l'URL.
// Elle affiche un exemple de lien à tester dans le navigateur et liste dynamiquement les paramètres reçus dans la requête.
// Utile pour comprendre le fonctionnement des requêtes GET sur le serveur embarqué et pour le debug.
static void page_testget(int conn_id, const http_parsed_request_t *req)
{
	if (!req)
		return;
	printf("[TEST][INFO] Entrée dans page_testget (conn_id=%d)\r\n", conn_id);

	const char PAGE_TESTGET_TITLE[] = "Test GET";
	const char CSS_PAGE_TESTGET_SPECIFIC[] =
		"div.param{margin:0.7em auto;padding:0.7em 1em;background:#f8fff4;border-radius:8px;max-width:320px;box-shadow:0 1px 4px #e0f5d8;}"
		"span.paramname{color:#3a5d23;font-weight:bold;display:inline-block;width:110px;text-align:right;margin-right:0.5em;}"
		"span.paramval{color:#388e3c;font-weight:bold;}"
		".test-link{display:inline-block;background:#222;color:#ffe066;font-size:1.2em;padding:1em 2em;border-radius:10px;margin:1.5em 0 1em 0;box-shadow:0 2px 8px #e0f5d8;font-family:monospace;word-break:break-all;letter-spacing:1px;}"
		".test-label{font-size:1.1em;color:#388e3c;font-weight:bold;margin-bottom:0.3em;display:block;}"
		"a.button.green{display:inline-block;padding:1em 2em;margin:2em 0 0 0;background:#28a745;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #28a745;}"
		"a.button.green:hover{filter:brightness(1.15);}";
	static const char BODY_PAGE_TESTGET[] =
		"<h1>Test GET</h1>"
		"<span class='test-label'>Testez dans votre navigateur :</span>"
		"<div class='test-link'>http://%s/testget?nom=Jean&age=42</div>"
		"<hr><b>Paramètres GET reçus :</b>";

	char html[2048];
	char *ptr = html;
	char *end_ptr = html + sizeof(html);
	char ip[32] = "IP";
	esp01_get_current_ip(ip, sizeof(ip));

	if (esp01_check_buffer_size(strlen(ptr) + 256, end_ptr - ptr) != ESP01_OK)
		return;

	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s", HTML_DOC_START, HTML_TITLE_START, PAGE_TESTGET_TITLE, HTML_TITLE_END_STYLE_START);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_TESTGET_SPECIFIC);
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);

	ptr += snprintf(ptr, end_ptr - ptr, BODY_PAGE_TESTGET, ip);

	char params_html[512] = "";
	int nb_lignes = 0, max_lignes = 8;

	// Affiche les paramètres GET reçus dans la requête
	if (req && req->query_string[0])
	{
		char query_copy[256];
		strncpy(query_copy, req->query_string, sizeof(query_copy) - 1);
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
			// Concaténation sécurisée avec vérification d'espace restant
			size_t current_len = strlen(params_html);				   // Longueur actuelle du HTML des paramètres
			size_t space_left = sizeof(params_html) - current_len - 1; // -1 pour \0
			if (strlen(row) < space_left)							   // Vérifie s'il reste de l'espace pour ajouter la ligne
			{
				strncat(params_html, row, space_left); // Ajoute la ligne au HTML des paramètres
			}
			else // sinon
			{
				break; // Arrêter si plus d'espace
			}
			nb_lignes++;			   // Incrémente le compteur de lignes
			token = strtok(NULL, "&"); // Passe au paramètre suivant
		}
	}
	else // Si aucun paramètre GET n'est reçu
	{
		strncpy(params_html, "<div style='margin:1em 0'><i>Aucun paramètre GET reçu</i></div>", sizeof(params_html) - 1); // Si aucun paramètre, afficher un message
		params_html[sizeof(params_html) - 1] = '\0';																	  // Assurer la terminaison de la chaîne
	}

	// Concaténation sécurisée du contenu final
	size_t html_current_len = strlen(html);						  // Longueur actuelle du HTML
	size_t html_space_left = sizeof(html) - html_current_len - 1; // -1 pour \0

	char *final_content = "<br><a class='button green' href='/'>Retour accueil</a>";					// Bouton de retour à la page d'accueil
	if (strlen(params_html) + strlen(final_content) + strlen(HTML_CARD_END_BODY_END) < html_space_left) // Vérifie s'il y a assez d'espace pour ajouter le contenu final
	{
		strncat(html, params_html, html_space_left);		   // Ajoute les paramètres HTML au tampon
		html_current_len = strlen(html);					   // Met à jour la longueur actuelle du HTML
		html_space_left = sizeof(html) - html_current_len - 1; // Met à jour l'espace restant

		strncat(html, final_content, html_space_left);		   // Ajoute le contenu final (bouton de retour)
		html_current_len = strlen(html);					   // Met à jour la longueur actuelle du HTML
		html_space_left = sizeof(html) - html_current_len - 1; // Met à jour l'espace restant

		strncat(html, HTML_CARD_END_BODY_END, html_space_left); // Ajoute la fin de la card et du body
	}
	else // Si pas assez d'espace pour ajouter le contenu final
	{
		// Fallback si pas assez d'espace - utiliser snprintf pour tronquer proprement
		snprintf(html + html_current_len, html_space_left, "%s%s%s", params_html, final_content, HTML_CARD_END_BODY_END); // Concatène les paramètres, le bouton de retour et la fin de la card
	}

	printf("[TEST][INFO] Sortie de page_testget, réponse envoyée sur conn_id=%d, taille=%d\r\n", conn_id, (int)strlen(html)); // Affiche la taille de la réponse
	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));									  // Envoie la page générée au client
}

// --- Page Status ("/status") ---
// Cette fonction affiche l'état du serveur web embarqué : IP, port, état de la LED, connexions actives, statistiques HTTP.
// Elle génère dynamiquement des tableaux HTML pour présenter les informations de diagnostic et de monitoring du serveur.
// Permet de surveiller l'activité réseau et le bon fonctionnement du serveur embarqué.
static void page_status(int conn_id, const http_parsed_request_t *req)
{
	if (!req)																  // Vérifie si la requête est valide
		return;																  // Si non, on sort de la fonction
	printf("[TEST][INFO] Entrée dans page_status (conn_id=%d)\r\n", conn_id); // Affiche l'entrée dans la page de statut

	const char PAGE_STATUS_TITLE[] = "Statut Serveur STM32"; // Titre de la page de statut
	const char CSS_PAGE_STATUS_SPECIFIC[] =					 // CSS spécifique à la page de statut
		"table{margin:2em auto 1em auto;border-collapse:collapse;box-shadow:0 2px 8px #e0f5d8;background:#fff;}"
		"th,td{padding:0.4em 1em;border:1px solid #e0f5d8;font-size:1em;}"
		"th{background:#ffe066;color:#3a5d23;}"
		"a.button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#388e3c;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #388e3c;}"
		"a.button.green{background:#28a745;border-color:#28a745;color:#fff;";

	char html[2048];					 // Tampon pour générer la page HTML
	char *ptr = html;					 // Pointeur pour écrire dans le tampon HTML
	char *end_ptr = html + sizeof(html); // Pointeur de fin pour éviter les débordements

	char ip[32] = "N/A";								  // Tampon pour l'adresse IP du serveur
	if (esp01_get_current_ip(ip, sizeof(ip)) != ESP01_OK) // Récupère l'adresse IP actuelle du module ESP01
		strncpy(ip, "Erreur", sizeof(ip) - 1);			  // Si échec, met "Erreur" dans le tampon

	GPIO_PinState led = HAL_GPIO_ReadPin(LED_GPIO_PORT, LED_GPIO_PIN); // Lit l'état de la LED embarquée

	const esp01_stats_t *stats = &g_stats;									   // Récupère les statistiques du serveur
	if (esp01_check_buffer_size(strlen(ptr) + 256, end_ptr - ptr) != ESP01_OK) // Vérifie si le tampon est assez grand
		return;																   // Si non, on sort de la fonction

	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s", HTML_DOC_START, HTML_TITLE_START, PAGE_STATUS_TITLE, HTML_TITLE_END_STYLE_START); // Début du document HTML, titre et début du style
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);																			  // Ajoute le CSS commun
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_STATUS_SPECIFIC);															  // Ajoute le CSS spécifique à la page de statut
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START);													  // Fin du style, début du body et de la card
	// Génération du contenu de la page de statut
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
	// Affiche les informations de connexion TCP
	ptr += snprintf(ptr, end_ptr - ptr,
					"<h2>Connexions TCP</h2>"
					"<table><tr><th>ID</th><th>Dernière activité (ms)</th><th>IP client</th><th>Port client</th></tr>");
	int nb_affichees = 0;						 // Compteur de connexions affichées
	for (int i = 0; i < g_connection_count; ++i) // Parcourt toutes les connexions actives
	{
		if (esp01_is_connection_active(i)) // Vérifie si la connexion est active
		{
			const connection_info_t *c = &g_connections[i]; // Récupère les informations de la connexion
			ptr += snprintf(ptr, end_ptr - ptr,
							"<tr><td>%d</td><td>%lu</td><td>%s</td><td>%u</td></tr>",
							c->conn_id,
							(unsigned long)(HAL_GetTick() - c->last_activity),
							c->client_ip[0] ? c->client_ip : "N/A",
							c->client_port); // Affiche les informations de la connexion
			nb_affichees++;					 // Incrémente le compteur de connexions affichées
		}
	}
	if (nb_affichees == 0) // Si aucune connexion active n'a été trouvée
	{
		ptr += snprintf(ptr, end_ptr - ptr, "<tr><td colspan='4'><i>Aucune connexion active</i></td></tr>"); // Affiche un message indiquant qu'il n'y a pas de connexions actives
	}
	ptr += snprintf(ptr, end_ptr - ptr, "</table>"); // Termine la table des connexions TCP

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
					(unsigned long)stats->avg_response_time_ms); // Affiche les statistiques HTTP

	ptr += snprintf(ptr, end_ptr - ptr, "<a class='button green' href='/'>Accueil</a>"); // Bouton pour revenir à la page d'accueil
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_CARD_END_BODY_END);					 // Termine la card et le body du document HTML

	printf("[TEST][INFO] Sortie de page_status, réponse envoyée sur conn_id=%d, taille=%d\r\n", conn_id, (int)strlen(html)); // Affiche la taille de la réponse
	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));									 // Envoie la page générée au client
}

// Structure pour les informations système
typedef struct
{
	char at_version[64];	// Version du firmware AT du module ESP01
	char stm32_type[32];	// Type de carte STM32 (L1, L4, F1, F4, F7, H7)
	char wifi_mode[8];		// Mode WiFi (STA ou AP)
	const char *wifi_ssid;	// SSID du réseau WiFi configuré
	uint16_t server_port;	// Port du serveur web embarqué
	const char *multi_conn; // Indique si les connexions multiples sont activées (Oui/Non)
} system_info_t;			// Structure pour stocker les informations système

// Fonction pour collecter les informations système
static void collect_system_info(system_info_t *info)
{
	if (!info)	// Vérifie si le pointeur d'information est valide
		return; // Si non, on sort de la fonction

	// Firmware ESP01
	esp01_safe_strcpy(info->at_version, sizeof(info->at_version), "N/A"); // Initialise la version AT à "N/A"
	char resp[256] = {0};												  // Tampon pour la réponse du module ESP01
	if (esp01_get_at_version(resp, sizeof(resp)) == ESP01_OK)			  // Récupère la version AT du module ESP01
	{
		// Utilise la fonction de parsing pour extraire la ligne "AT version:"
		char *line = strstr(resp, "AT version:"); // Cherche la ligne contenant "AT version:"
		if (line)								  // Si la ligne est trouvée
		{
			char *end = strchr(line, '\n');							// Cherche la fin de la ligne
			size_t len = end ? (size_t)(end - line) : strlen(line); // Calcule la longueur de la ligne
			if (len > 0 && len < sizeof(info->at_version))			// Vérifie que la longueur est valide
			{
				strncpy(info->at_version, line, len); // Copie la ligne dans le tampon at_version
				info->at_version[len] = '\0';		  // Assure la terminaison de la chaîne
			}
		}
	}

	// Type de carte STM32
	esp01_safe_strcpy(info->stm32_type, sizeof(info->stm32_type), "STM32 inconnue"); // Initialise le type de carte STM32 à "inconnue"
#if defined(STM32L4)																 // Vérifie si la carte est une STM32L4
	esp01_safe_strcpy(info->stm32_type, sizeof(info->stm32_type), "STM32L4");		 // Initialise le type de carte STM32 à "STM32L4"
#elif defined(STM32F4)																 // Vérifie si la carte est une STM32F4
	esp01_safe_strcpy(info->stm32_type, sizeof(info->stm32_type), "STM32F4"); // Initialise le type de carte STM32 à "STM32F4"
#elif defined(STM32L1)																 // Vérifie si la carte est une STM32L1
	esp01_safe_strcpy(info->stm32_type, sizeof(info->stm32_type), "STM32L1"); // Initialise le type de carte STM32 à "STM32L1"
#elif defined(STM32F1)																 // Vérifie si la carte est une STM32F1
	esp01_safe_strcpy(info->stm32_type, sizeof(info->stm32_type), "STM32F1"); // Initialise le type de carte STM32 à "STM32F1"
#elif defined(STM32F7)																 // Vérifie si la carte est une STM32F7
	esp01_safe_strcpy(info->stm32_type, sizeof(info->stm32_type), "STM32F7"); // Initialise le type de carte STM32 à "STM32F7"
#elif defined(STM32H7)																 // Vérifie si la carte est une STM32H7
	esp01_safe_strcpy(info->stm32_type, sizeof(info->stm32_type), "STM32H7"); // Initialise le type de carte STM32 à "STM32H7"
#endif

	// Config WiFi - Utiliser directement la valeur de ESP01_WIFI_MODE_STA
	if (ESP01_WIFI_MODE_STA == 1) // Vérifie si le mode WiFi est configuré en station (STA)
	{
		esp01_safe_strcpy(info->wifi_mode, sizeof(info->wifi_mode), "STA"); // Initialise le mode WiFi à "STA"
	}
	else // Sinon, on suppose que le mode est AP (Access Point)
	{
		esp01_safe_strcpy(info->wifi_mode, sizeof(info->wifi_mode), "AP"); // Initialise le mode WiFi à "AP"
	}

	info->wifi_ssid = SSID; // Utilise la constante SSID définie dans le code pour le SSID du réseau WiFi

	// Config serveur
	info->server_port = g_server_port;							 // Récupère le port du serveur web embarqué
	info->multi_conn = (ESP01_MULTI_CONNECTION) ? "Oui" : "Non"; // Indique si les connexions multiples sont activées (Oui/Non)
}

// Fonction pour rendre une section HTML avec table
static int render_html_section(char *buffer, size_t buffer_size, const char *title,
							   const char **labels, const char **values, int row_count)
{
	char *ptr = buffer;					  // Pointeur pour écrire dans le tampon
	char *end_ptr = buffer + buffer_size; // Pointeur de fin pour éviter les débordements
	int written = 0;					  // Compteur de caractères écrits

	// Titre de la section
	written = snprintf(ptr, end_ptr - ptr, "<h%d>%s</h%d><table>",
					   (title[0] == 'I') ? 1 : 2, title, (title[0] == 'I') ? 1 : 2); // Écrit le titre de la section et ouvre la table
	ptr += written;																	 // Avance le pointeur de tampon

	// Lignes de la table
	for (int i = 0; i < row_count; i++) // Parcourt les étiquettes et valeurs
	{
		int row_len = snprintf(ptr, end_ptr - ptr,
							   "<tr><th>%s</th><td>%s</td></tr>",
							   labels[i], values[i]); // Écrit une ligne de la table avec l'étiquette et la valeur
		ptr += row_len;								  // Avance le pointeur de tampon
		written += row_len;							  // Incrémente le compteur de caractères écrits
	}

	// Fermeture de la table
	int end_len = snprintf(ptr, end_ptr - ptr, "</table>"); // Écrit la fermeture de la table
	written += end_len;										// Incrémente le compteur de caractères écrits

	return written; // Retourne le nombre total de caractères écrits dans le tampon
}

// --- Page Infos Système & Réseau ("/device") ---
// Cette fonction affiche les informations système et réseau du STM32 et du module ESP01.
// Elle présente le firmware, le type de carte, la configuration WiFi et serveur sous forme de tableaux HTML.
// Utile pour le diagnostic matériel, la vérification de la configuration embarquée et l'identification du matériel.
static void page_device(int conn_id, const http_parsed_request_t *req)
{
	if (!req)																  // Vérifie si la requête est valide
		return;																  // Si non, on sort de la fonction
	printf("[TEST][INFO] Entrée dans page_device (conn_id=%d)\r\n", conn_id); // Affiche l'entrée dans la page d'informations système

	const char PAGE_DEVICE_TITLE[] = "Infos Système & Réseau"; // Titre de la page d'informations système
	const char CSS_PAGE_DEVICE_SPECIFIC[] =					   // CSS spécifique à la page d'informations système
		"table{margin:2em auto 1em auto;border-collapse:collapse;box-shadow:0 2px 8px #e0f5d8;background:#fff;}"
		"th,td{padding:0.4em 1em;border:1px solid #e0f5d8;font-size:1em;}"
		"th{background:#ffe066;color:#3a5d23;}"
		"a.button{display:inline-block;padding:1em 2em;margin:1em 0.5em;background:#388e3c;color:#fff;text-decoration:none;border-radius:8px;font-size:1.1em;transition:background 0.2s,border 0.2s;box-shadow:0 2px 8px #e0f5d8;border:2px solid #388e3c;}"
		"a.button.green{background:#28a745;border-color:#28a745;color:#fff;";

	system_info_t sys_info;			// Structure pour stocker les informations système
	collect_system_info(&sys_info); // Collecte les informations système

	char html[2048];					 // Tampon pour générer la page HTML
	html[0] = '\0';						 // Initialise le tampon à vide
	char *ptr = html;					 // Pointeur pour écrire dans le tampon HTML
	char *end_ptr = html + sizeof(html); // Pointeur de fin pour éviter les débordements

	if (esp01_check_buffer_size(strlen(ptr) + 256, end_ptr - ptr) != ESP01_OK) // Vérifie si le tampon est assez grand
		return;																   // Si non, on sort de la fonction

	ptr += snprintf(ptr, end_ptr - ptr, "%s%s%s%s",
					HTML_DOC_START, HTML_TITLE_START,
					PAGE_DEVICE_TITLE, HTML_TITLE_END_STYLE_START);					// Début du document HTML, titre et début du style
	ptr += snprintf(ptr, end_ptr - ptr, "%s", PAGE_CSS);							// Ajoute le CSS commun
	ptr += snprintf(ptr, end_ptr - ptr, "%s", CSS_PAGE_DEVICE_SPECIFIC);			// Ajoute le CSS spécifique à la page d'informations système
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_STYLE_END_HEAD_BODY_CARD_START); // Fin du style, début du body et de la card

	const char *sys_labels[] = {"Firmware ESP01", "Carte STM32"};		   // Étiquettes pour les informations système
	const char *sys_values[] = {sys_info.at_version, sys_info.stm32_type}; // Valeurs pour les informations système
	ptr += render_html_section(ptr, end_ptr - ptr, "Informations Système",
							   sys_labels, sys_values, 2); // Rendu de la section Informations Système

	const char *wifi_labels[] = {"Mode", "SSID"};						  // Étiquettes pour les informations WiFi
	const char *wifi_values[] = {sys_info.wifi_mode, sys_info.wifi_ssid}; // Valeurs pour les informations WiFi
	ptr += render_html_section(ptr, end_ptr - ptr, "Configuration WiFi",
							   wifi_labels, wifi_values, 2); // Rendu de la section Configuration WiFi

	char port_str[8];												  // Tampon pour le port du serveur
	snprintf(port_str, sizeof(port_str), "%u", sys_info.server_port); // Convertit le port en chaîne de caractères

	const char *server_labels[] = {"Port", "Multi-connexion"};	   // Étiquettes pour les informations du serveur
	const char *server_values[] = {port_str, sys_info.multi_conn}; // Valeurs pour les informations du serveur
	ptr += render_html_section(ptr, end_ptr - ptr, "Configuration Serveur",
							   server_labels, server_values, 2); // Rendu de la section Configuration Serveur

	ptr += snprintf(ptr, end_ptr - ptr, "<a class='button green' href='/'>Accueil</a>"); // Bouton pour revenir à la page d'accueil
	ptr += snprintf(ptr, end_ptr - ptr, "%s", HTML_CARD_END_BODY_END);					 // Termine la card et le body du document HTML

	printf("[TEST][INFO] Sortie de page_device, réponse envoyée sur conn_id=%d, taille=%d\r\n", conn_id, (int)strlen(html)); // Affiche la taille de la réponse
	esp01_send_http_response(conn_id, 200, "text/html; charset=UTF-8", html, strlen(html));									 // Envoie la page générée au client
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_USART2_UART_Init();
	MX_USART1_UART_Init();
	/* USER CODE BEGIN 2 */
	HAL_Delay(1000);
	printf("\n[TEST][INFO] === Démarrage du programme de test du module STM32_WifiESP_HTTP ===\r\n");
	HAL_Delay(500);

	ESP01_Status_t status;

	// 1. Initialisation du driver ESP01
	printf("\n[TEST][INFO] === Initialisation du driver ESP01 ===\r\n");
	status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
	printf("[TEST][INFO] Initialisation du driver ESP01 : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 2. Flush du buffer RX
	printf("\n[TEST][INFO] === Vidage du buffer RX ===\r\n");
	status = esp01_flush_rx_buffer(500);
	printf("[TEST][INFO] Buffer UART/DMA vidé : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 3. test de communication AT
	printf("\n[TEST][INFO] === Test de communication AT ===\r\n");
	status = esp01_test_at();
	printf("[TEST][INFO] Test AT : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 4. Test de version AT+GMR
	printf("\n[TEST][INFO] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
	char at_version[256] = {0};
	status = esp01_get_at_version(at_version, sizeof(at_version));
	printf("[TEST][INFO] Version ESP01 : %s\r\n", at_version);
	HAL_Delay(500);

	// 5. Connexion au réseau WiFi
	printf("\n[TEST][INFO] === Connexion au réseau WiFi ===\r\n");
	status = esp01_connect_wifi_config(
		ESP01_WIFI_MODE_STA, // mode
		SSID,				 // ssid
		PASSWORD,			 // password
		true,				 // use_dhcp
		NULL,				 // ip
		NULL,				 // gateway
		NULL				 // netmask
	);
	printf("[TEST][INFO] Connexion WiFi : %s\r\n", esp01_get_error_string(status));
	HAL_Delay(500);

	// 6. Activation du mode multi-connexion ET démarrage du serveur web
	printf("\n[TEST][INFO] === Activation multi-connexion + démarrage serveur web ===\r\n");
	ESP01_Status_t server_status = esp01_start_server_config(
		true, // multi-connexion
		80,	  // port
		true  // ipdinfo (affichage IP client dans +IPD)
	);
	if (server_status != ESP01_OK)
	{
		printf("[TEST][ERROR] CIPMUX/CIPSERVER : %s\r\n", esp01_get_error_string(server_status));
		Error_Handler();
	}
	else
	{
		printf("[TEST][INFO] Serveur web démarré sur le port 80\r\n");
	}
	HAL_Delay(500);

	// 7. Ajout des routes HTTP
	printf("\n[TEST][INFO] === Ajout des routes HTTP ===\r\n");
	esp01_clear_routes();
	printf("[TEST][INFO] Ajout route /\r\n");
	esp01_add_route("/", page_root);
	printf("[TEST][INFO] Ajout route /status\r\n");
	esp01_add_route("/status", page_status);
	printf("[TEST][INFO] Ajout route /led\r\n");
	esp01_add_route("/led", page_led);
	printf("[TEST][INFO] Ajout route /testget\r\n");
	esp01_add_route("/testget", page_testget);
	printf("[TEST][INFO] Ajout route /device\r\n");
	esp01_add_route("/device", page_device);
	HAL_Delay(500);

	// 8. Vérification serveur ESP01
	printf("\n[TEST][INFO] === Vérification de l'état des connexions ===\r\n");
	esp01_print_connection_status();
	HAL_Delay(500);

	// 9. Affichage de la configuration IP complète
	printf("\n[TEST][INFO] === Configuration IP complète ===\r\n");
	char ipc[32] = "N/A", gw[32] = "N/A", mask[32] = "N/A";
	if (esp01_get_ip_config(ipc, sizeof(ipc), gw, sizeof(gw), mask, sizeof(mask)) == ESP01_OK)
	{
		printf("[TEST][INFO] IP: %s\r\n", ipc);
		printf("[TEST][INFO] Gateway: %s\r\n", gw);
		printf("[TEST][INFO] Masque: %s\r\n", mask);
	}
	else
	{
		printf("[TEST][ERROR] Impossible de récupérer la configuration IP complète\r\n");
	}
	HAL_Delay(500);

	// 10. Affichage de l'adresse IP
	printf("\n[TEST][INFO] === Serveur Web prêt ===\r\n");
	char ip[32] = "N/A";
	if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
		printf("[TEST][INFO] Connectez-vous à : http://%s/\r\n", ip);
	else
		printf("[TEST][ERROR] Impossible de récupérer l'adresse IP du module\r\n");
	printf("[TEST][INFO] Démarrage de la boucle principale\r\n");
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		esp01_http_loop(); // Gère les requêtes HTTP entrantes
		HAL_Delay(10);	   // Délai pour éviter de surcharger le CPU
						   /* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 10;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel5_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : LD2_Pin */
	GPIO_InitStruct.Pin = LD2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
