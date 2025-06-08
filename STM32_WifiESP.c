/*
 * STM32_WifiESP.c
 * Driver STM32 <-> ESP01 (AT)
 * Version Final V1.0
 * 2025 - manu
 */

// ==================== INCLUDES ====================
#include "STM32_WifiESP.h"
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_HTTP.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

// ==================== DÉFINES & MACROS ====================
#ifndef VALIDATE_PARAM
#define VALIDATE_PARAM(expr, ret) \
	do                            \
	{                             \
		if (!(expr))              \
			return (ret);         \
	} while (0)
#endif

#define ESP01_CONN_TIMEOUT_MS 30000 // Timeout pour les connexions (30 secondes)

// ==================== VARIABLES GLOBALES ====================
esp01_stats_t g_stats = {0};
connection_info_t g_connections[ESP01_MAX_CONNECTIONS] = {0};
int g_connection_count = 0;
char g_accumulator[ESP01_DMA_RX_BUF_SIZE * 2] = {0};
uint16_t g_acc_len = 0;
volatile bool g_processing_request = false;
esp01_route_t g_routes[ESP01_MAX_ROUTES];
int g_route_count = 0;
esp01_mqtt_client_t g_mqtt_client = {0};
UART_HandleTypeDef *g_esp_uart = NULL;
UART_HandleTypeDef *g_debug_uart = NULL;
uint8_t *g_dma_rx_buf = NULL;
uint16_t g_dma_buf_size = 0;
volatile uint16_t g_rx_last_pos = 0;
uint16_t g_server_port = 80;
const ESP01_WifiMode_t g_wifi_mode = ESP01_WIFI_MODE_STA;

// ==================== VARIABLES INTERNES ====================
typedef enum
{
	PARSE_STATE_SEARCHING_IPD,
	PARSE_STATE_READING_HEADER,
	PARSE_STATE_READING_PAYLOAD
} parse_state_t;

static parse_state_t g_parse_state = PARSE_STATE_SEARCHING_IPD;

// ==================== FONCTIONS INTERNES (static) ====================
static ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode)
{
	char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_CMD_BUF * 2];
	snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);
	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);
	return status;
}

static ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
	char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_CMD_BUF * 2];
	snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_WIFI);
	return status;
}

// ==================== DRIVER & COMMUNICATION ====================
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
						  uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
	_esp_logln("[ESP01] Initialisation UART/DMA pour ESP01");
	g_esp_uart = huart_esp;
	g_debug_uart = huart_debug;
	g_dma_rx_buf = dma_rx_buf;
	g_dma_buf_size = dma_buf_size;
	g_rx_last_pos = 0;
	g_acc_len = 0;
	g_accumulator[0] = '\0';
	g_parse_state = PARSE_STATE_SEARCHING_IPD;

	if (!g_esp_uart || !g_dma_rx_buf || g_dma_buf_size == 0)
	{
		_esp_logln("[ESP01] ESP01 Init Error: Invalid params");
		return ESP01_NOT_INITIALIZED;
	}

	if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK)
	{
		_esp_logln("[ESP01] ESP01 Init Error: HAL_UART_Receive_DMA failed");
		return ESP01_FAIL;
	}

	_esp_logln("[ESP01] --- ESP01 Driver Init OK ---");
	HAL_Delay(100);
	return ESP01_OK;
}

ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms)
{
	_flush_rx_buffer(timeout_ms);
	_esp_logln("[ESP01] Buffer RX vidé");
	return ESP01_OK;
}

ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer,
										  uint32_t max_response_size,
										  const char *expected_terminator,
										  uint32_t timeout_ms)
{
	if (!g_esp_uart || !cmd || !response_buffer)
		return ESP01_NOT_INITIALIZED;

	const char *terminator = expected_terminator ? expected_terminator : "OK";

	char full_cmd[ESP01_DMA_RX_BUF_SIZE];
	int cmd_len = snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);

	g_acc_len = 0;
	g_accumulator[0] = '\0';

	_flush_rx_buffer(100);

	if (HAL_UART_Transmit(g_esp_uart, (uint8_t *)full_cmd, cmd_len, ESP01_TIMEOUT_SHORT) != HAL_OK)
		return ESP01_FAIL;

	bool found = _accumulate_and_search(g_accumulator, &g_acc_len, sizeof(g_accumulator), terminator, timeout_ms, true);

	uint32_t copy_len = (g_acc_len < max_response_size - 1) ? g_acc_len : max_response_size - 1;
	memcpy(response_buffer, g_accumulator, copy_len);
	response_buffer[copy_len] = '\0';

	g_acc_len = 0;
	g_accumulator[0] = '\0';

	return found ? ESP01_OK : ESP01_TIMEOUT;
}

// ==================== WIFI ====================
ESP01_Status_t esp01_connect_wifi_config(
	ESP01_WifiMode_t mode,
	const char *ssid,
	const char *password,
	bool use_dhcp,
	const char *ip,
	const char *gateway,
	const char *netmask)
{
	ESP01_Status_t status;
	char cmd[ESP01_DMA_RX_BUF_SIZE];
	char resp[ESP01_DMA_RX_BUF_SIZE];

	_esp_logln("[WIFI] === Début configuration WiFi ===");

	_esp_logln("[WIFI] -> Définition du mode WiFi...");
	status = esp01_set_wifi_mode(mode);
	_esp_logln(resp);
	if (status != ESP01_OK)
	{
		_esp_logln("[WIFI] !! ERREUR: esp01_set_wifi_mode");
		return status;
	}
	HAL_Delay(300);

	if (mode == ESP01_WIFI_MODE_AP)
	{
		_esp_logln("[WIFI] -> Configuration du point d'accès (AP)...");
		snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",5,3", ssid, password);
		status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);
		_esp_logln(resp);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Configuration AP");
			return status;
		}
		HAL_Delay(300);

		if (ip && strlen(ip) > 0)
		{
			_esp_logln("[WIFI] -> Configuration IP fixe AP...");
			snprintf(cmd, sizeof(cmd), "AT+CIPAP=\"%s\"", ip);
			status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);
			_esp_logln(resp);
			if (status != ESP01_OK)
			{
				_esp_logln("[WIFI] !! ERREUR: Configuration IP AP");
				return status;
			}
		}
	}

	if (use_dhcp)
	{
		if (mode == ESP01_WIFI_MODE_STA)
		{
			_esp_logln("[WIFI] -> Activation du DHCP STA...");
			status = esp01_send_raw_command_dma("AT+CWDHCP=1,1", resp, sizeof(resp), "OK", 2000);
		}
		else if (mode == ESP01_WIFI_MODE_AP)
		{
			_esp_logln("[WIFI] -> Activation du DHCP AP...");
			status = esp01_send_raw_command_dma("AT+CWDHCP=2,1", resp, sizeof(resp), "OK", 2000);
		}
		_esp_logln(resp);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Activation DHCP");
			return status;
		}
	}
	else if (ip && gateway && netmask && mode == ESP01_WIFI_MODE_STA)
	{
		_esp_logln("[WIFI] -> Déconnexion du WiFi (CWQAP)...");
		esp01_send_raw_command_dma("AT+CWQAP", resp, sizeof(resp), "OK", 2000);

		_esp_logln("[WIFI] -> Désactivation du DHCP client...");
		status = esp01_send_raw_command_dma("AT+CWDHCP=0,1", resp, sizeof(resp), "OK", 2000);
		_esp_logln(resp);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Désactivation DHCP");
			return status;
		}
		_esp_logln("[WIFI] -> Configuration IP statique...");
		snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gateway, netmask);
		status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);
		_esp_logln(resp);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Configuration IP statique");
			return status;
		}
	}

	if (mode == ESP01_WIFI_MODE_STA)
	{
		_esp_logln("[WIFI] -> Connexion au réseau WiFi...");
		status = esp01_connect_wifi(ssid, password);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Connexion WiFi (CWJAP)");
			return status;
		}
		HAL_Delay(300);
	}

	_esp_logln("[WIFI] -> Activation de l'affichage IP client dans +IPD (AT+CIPDINFO=1)...");
	status = esp01_send_raw_command_dma("AT+CIPDINFO=1", resp, sizeof(resp), "OK", 2000);
	_esp_logln(resp);
	if (status != ESP01_OK)
	{
		_esp_logln("[WIFI] !! ERREUR: AT+CIPDINFO=1");
		return status;
	}

	_esp_logln("[WIFI] === Configuration WiFi terminée ===");
	return ESP01_OK;
}

ESP01_Status_t esp01_scan_networks(esp01_network_t *networks, uint8_t max_networks, uint8_t *found_networks)
{
	char resp[ESP01_DMA_RX_BUF_SIZE * 2] = {0};
	if (found_networks)
		*found_networks = 0;
	if (!networks || max_networks == 0)
		return ESP01_INVALID_PARAM;

	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CWLAP", resp, sizeof(resp), "OK", 10000);
	if (status != ESP01_OK)
		return status;

	const char *p = resp;
	uint8_t count = 0;
	while ((p = strstr(p, "+CWLAP:")) && count < max_networks)
	{
		int enc = 0, rssi = 0, channel = 0;
		char ssid[33] = {0}, bssid[18] = {0};
		int n = sscanf(p, "+CWLAP:(%d,\"%32[^\"]\",%d,\"%17[^\"]\",%d)", &enc, ssid, &rssi, bssid, &channel);
		if (n == 5)
		{
			strncpy(networks[count].ssid, ssid, sizeof(networks[count].ssid));
			networks[count].rssi = rssi;
			networks[count].encryption = enc;
			strncpy(networks[count].bssid, bssid, sizeof(networks[count].bssid));
			networks[count].channel = channel;
			count++;
		}
		p = strchr(p + 1, '\n');
		if (!p)
			break;
	}
	if (found_networks)
		*found_networks = count;
	return (count > 0) ? ESP01_OK : ESP01_FAIL;
}

void esp01_print_wifi_networks(uint8_t max_networks)
{
	esp01_network_t networks[10];
	uint8_t found_networks = 0;

	_esp_logln("[WIFI] Démarrage du scan des réseaux WiFi...");

	if (esp01_scan_networks(networks, max_networks, &found_networks) == ESP01_OK)
	{
		printf("[WIFI] %d réseaux WiFi trouvés:\r\n", found_networks);
		for (int i = 0; i < found_networks; i++)
		{
			printf("  %d. SSID: %s\r\n", i + 1, networks[i].ssid);
			printf("     Signal: %d dBm\r\n", networks[i].rssi);
			printf("     Canal: %d\r\n", networks[i].channel);
			printf("     BSSID: %s\r\n", networks[i].bssid);
			printf("     Sécurité: ");
			switch (networks[i].encryption)
			{
			case 0:
				printf("Ouvert\r\n");
				break;
			case 1:
				printf("WEP\r\n");
				break;
			case 2:
				printf("WPA-PSK\r\n");
				break;
			case 3:
				printf("WPA2-PSK\r\n");
				break;
			case 4:
				printf("WPA/WPA2-PSK\r\n");
				break;
			default:
				printf("Inconnu\r\n");
				break;
			}
			printf("\r\n");
		}
	}
	else
	{
		printf("[WIFI] Échec du scan des réseaux WiFi\r\n");
	}
}

// ==================== SERVEUR WEB ====================
ESP01_Status_t esp01_start_server_config(bool multi_conn, uint16_t port)
{
	g_server_port = port;
	ESP01_Status_t status;
	char resp[ESP01_DMA_RX_BUF_SIZE];
	char cmd[ESP01_DMA_RX_BUF_SIZE];

	snprintf(cmd, sizeof(cmd), "AT+CIPMUX=%d", multi_conn ? 1 : 0);
	status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 3000);
	if (status != ESP01_OK)
	{
		_esp_logln("[WEB] ERREUR: AT+CIPMUX");
		return status;
	}

	snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port);
	status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000);
	if (status != ESP01_OK && !strstr(resp, "no change"))
	{
		_esp_logln("[WEB] ERREUR: AT+CIPSERVER");
		return status;
	}

	_esp_logln("[WEB] Serveur web démarré avec succès");
	return ESP01_OK;
}

ESP01_Status_t esp01_stop_web_server(void)
{
	_esp_logln("[STATUS] Arrêt du serveur web ESP01");
	char response[ESP01_DMA_RX_BUF_SIZE];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSERVER=0", response, sizeof(response), "OK", ESP01_TIMEOUT_MEDIUM);
	return status;
}

// ==================== STATUT & UTILITAIRES ====================
ESP01_Status_t esp01_test_at(void)
{
	char resp[ESP01_DMA_RX_BUF_SIZE];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
	return status;
}

ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size)
{
	if (!version_buf || buf_size == 0)
		return ESP01_INVALID_PARAM;

	char resp[ESP01_DMA_RX_BUF_SIZE];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+GMR", resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
	if (status != ESP01_OK)
		return status;

	char *line = strtok(resp, "\r\n");
	while (line && strlen(line) == 0)
		line = strtok(NULL, "\r\n");
	if (line)
	{
		strncpy(version_buf, line, buf_size - 1);
		version_buf[buf_size - 1] = '\0';
		return ESP01_OK;
	}
	version_buf[0] = '\0';
	return ESP01_FAIL;
}

ESP01_Status_t esp01_get_connection_status(void)
{
	_esp_logln("[STATUS] Vérification du statut de connexion ESP01");
	char response[ESP01_DMA_RX_BUF_SIZE];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSTATUS", response, sizeof(response), "OK", ESP01_TIMEOUT_MEDIUM);

	if (status == ESP01_OK)
	{
		if (strstr(response, "STATUS:2") || strstr(response, "STATUS:3"))
		{
			_esp_logln("[STATUS] ESP01 connecté au WiFi");
			return ESP01_OK;
		}
	}

	_esp_logln("[STATUS] ESP01 non connecté au WiFi");
	return ESP01_FAIL;
}

ESP01_Status_t esp01_get_current_ip(char *ip_buffer, size_t buffer_size)
{
	if (!ip_buffer || buffer_size == 0)
		return ESP01_INVALID_PARAM;

	char response[ESP01_DMA_RX_BUF_SIZE];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIFSR", response, sizeof(response), "OK", ESP01_TIMEOUT_SHORT);
	if (status != ESP01_OK)
		return status;

	_esp_logln("[DEBUG] Réponse brute AT+CIFSR :");
	_esp_logln(response);

	char *ip_line = strstr(response, "STAIP,\"");
	if (!ip_line)
		ip_line = strstr(response, "+CIFSR:STAIP,\"");
	if (!ip_line)
		ip_line = strstr(response, "APIP,\"");
	if (!ip_line)
		ip_line = strstr(response, "+CIFSR:APIP,\"");

	if (ip_line)
	{
		ip_line = strchr(ip_line, '"');
		if (ip_line)
		{
			ip_line++;
			char *end_quote = strchr(ip_line, '"');
			if (end_quote)
			{
				size_t ip_len = end_quote - ip_line;
				if (ip_len < buffer_size)
				{
					strncpy(ip_buffer, ip_line, ip_len);
					ip_buffer[ip_len] = '\0';
					return ESP01_OK;
				}
			}
		}
	}

	_esp_logln("[IP] Adresse IP non trouvée dans la réponse ESP01");
	if (buffer_size > 0)
		ip_buffer[0] = '\0';

	return ESP01_FAIL;
}

ESP01_Status_t esp01_print_connection_status(void)
{
	if (!g_debug_uart)
	{
		_esp_logln("[STATUS] esp01_print_connection_status: g_debug_uart NULL");
		return ESP01_NOT_INITIALIZED;
	}

	const char *header = "=== STATUS ESP01 ===\r\n";
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)header, strlen(header), HAL_MAX_DELAY);

	char response[ESP01_DMA_RX_BUF_SIZE];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT", response, sizeof(response), "OK", 2000);

	char msg[ESP01_MAX_DBG_BUF];
	snprintf(msg, sizeof(msg), "Test AT: %s\r\n", (status == ESP01_OK) ? "OK" : "FAIL");
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

	status = esp01_send_raw_command_dma("AT+CWJAP?", response, sizeof(response), "OK", 3000);
	if (status == ESP01_OK)
	{
		if (strstr(response, "No AP"))
		{
			HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Non connecté\r\n", 20, HAL_MAX_DELAY);
		}
		else
		{
			HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Connecté\r\n", 16, HAL_MAX_DELAY);
		}
	}
	else
	{
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Status inconnu\r\n", 22, HAL_MAX_DELAY);
	}

	char ip[ESP01_IP_BUF_SIZE];
	if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
	{
		snprintf(msg, sizeof(msg), "IP: %s\r\n", ip);
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
	}

	snprintf(msg, sizeof(msg), "Routes définies: %d/%d\r\n", g_route_count, ESP01_MAX_ROUTES);
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

	const char *footer = "==================\r\n";
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)footer, strlen(footer), HAL_MAX_DELAY);
	_esp_logln("[STATUS] Statut ESP01 affiché sur terminal");
	return ESP01_OK;
}

