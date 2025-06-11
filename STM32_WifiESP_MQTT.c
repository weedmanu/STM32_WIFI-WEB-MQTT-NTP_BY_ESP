/**
 ******************************************************************************
 * @file    STM32_WifiESP_MQTT.c
 * @author  manu
 * @version 1.0.0
 * @date    2025
 * @brief   Implémentation des fonctions MQTT pour le module ESP01 WiFi.
 *
 * @details
 * Ce fichier contient l'implémentation des fonctions pour gérer la
 * communication MQTT (client) via un module ESP01 connecté à un
 * microcontrôleur STM32. Il propose des fonctions haut niveau pour la
 * connexion à un broker, la publication, la souscription, le ping,
 * la gestion des messages MQTT et le polling.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la bibliothèque STM32_WifiESP.h.
 *
 * @copyright
 * La licence de ce code est libre.
 ******************************************************************************
 */

// ==================== INCLUDES ====================
#include "STM32_WifiESP_MQTT.h" // Header MQTT
#include <string.h>             // Pour memcpy, strncpy, etc.
#include <stdio.h>              // Pour snprintf

// ==================== VARIABLES GLOBALES ====================
mqtt_client_t g_mqtt_client = {0};               // Structure client MQTT globale
static mqtt_message_callback_t g_mqtt_cb = NULL; // Callback utilisateur pour les messages MQTT

// ==================== CONNEXION MQTT ====================
ESP01_Status_t esp01_mqtt_connect(const char *broker_ip, uint16_t port,
                                  const char *client_id, const char *username,
                                  const char *password)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF]; // Buffers pour commandes et réponses
    ESP01_Status_t status;                                 // Statut de retour

    VALIDATE_PARAM(broker_ip && client_id, ESP01_INVALID_PARAM); // Vérifie les paramètres

    // Ouvre une connexion TCP vers le broker MQTT
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", broker_ip, port);
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);
    _esp_login("[MQTT] >>> %s", resp); // Log la réponse brute
    if (status != ESP01_OK)
        return status;

    HAL_Delay(500); // Petite pause pour la stabilité

    // Construction du paquet MQTT CONNECT
    uint8_t mqtt_connect[256];
    uint16_t mqtt_len = 0;
    mqtt_connect[mqtt_len++] = 0x10; // Type CONNECT

    uint16_t len_pos = mqtt_len++; // Position du champ "remaining length"
    uint16_t var_len = 10;         // Taille variable de base

    uint16_t client_id_len = strlen(client_id);
    var_len += 2 + client_id_len;

    bool has_username = (username && strlen(username) > 0);
    uint16_t username_len = 0;
    if (has_username)
    {
        username_len = strlen(username);
        var_len += 2 + username_len;
    }

    bool has_password = (password && strlen(password) > 0);
    uint16_t password_len = 0;
    if (has_password)
    {
        password_len = strlen(password);
        var_len += 2 + password_len;
    }

    // Protocole MQTT
    mqtt_connect[mqtt_len++] = 0x00;
    mqtt_connect[mqtt_len++] = 0x04;
    mqtt_connect[mqtt_len++] = 'M';
    mqtt_connect[mqtt_len++] = 'Q';
    mqtt_connect[mqtt_len++] = 'T';
    mqtt_connect[mqtt_len++] = 'T';
    mqtt_connect[mqtt_len++] = 0x04; // Version 3.1.1

    uint8_t connect_flags = 0x02; // Clean session
    if (has_username)
        connect_flags |= 0x80;
    if (has_password)
        connect_flags |= 0x40;
    mqtt_connect[mqtt_len++] = connect_flags;

    mqtt_connect[mqtt_len++] = 0x00;
    mqtt_connect[mqtt_len++] = 0x3C; // Keep alive 60s

    // Client ID
    mqtt_connect[mqtt_len++] = (client_id_len >> 8) & 0xFF;
    mqtt_connect[mqtt_len++] = client_id_len & 0xFF;
    memcpy(&mqtt_connect[mqtt_len], client_id, client_id_len);
    mqtt_len += client_id_len;

    // Username
    if (has_username)
    {
        mqtt_connect[mqtt_len++] = (username_len >> 8) & 0xFF;
        mqtt_connect[mqtt_len++] = username_len & 0xFF;
        memcpy(&mqtt_connect[mqtt_len], username, username_len);
        mqtt_len += username_len;
    }

    // Password
    if (has_password)
    {
        mqtt_connect[mqtt_len++] = (password_len >> 8) & 0xFF;
        mqtt_connect[mqtt_len++] = password_len & 0xFF;
        memcpy(&mqtt_connect[mqtt_len], password, password_len);
        mqtt_len += password_len;
    }

    // Encode la longueur variable MQTT
    if (var_len < 128)
    {
        mqtt_connect[len_pos] = var_len;
    }
    else
    {
        mqtt_connect[len_pos] = (var_len & 0x7F) | 0x80;
        mqtt_connect[len_pos + 1] = var_len >> 7;
        mqtt_len++;
    }

    // Prépare l'envoi du paquet MQTT CONNECT
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len);
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
        return status;

    _esp_login("[MQTT] === Envoi du paquet CONNECT ===");
    for (int i = 0; i < mqtt_len; ++i)
    {
        _esp_login("[MQTT] >>> TX[%03d]: %02X", i, mqtt_connect[i]);
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_connect, mqtt_len, HAL_MAX_DELAY); // Envoie le paquet CONNECT

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT); // Attend l'accusé d'envoi
    if (status != ESP01_OK)
        return status;

    // Attente du CONNACK
    uint8_t rx_buf[256];
    memset(rx_buf, 0, sizeof(rx_buf));
    bool found_connack = false;
    uint32_t start = HAL_GetTick();

    _esp_login("[MQTT] === Attente du CONNACK (timeout 10s) ===");
    HAL_Delay(500);

    while ((HAL_GetTick() - start) < 10000 && !found_connack)
    {
        uint16_t rx_len = esp01_get_new_data(rx_buf, sizeof(rx_buf));
        if (rx_len > 0)
        {
            rx_buf[rx_len] = '\0';
            char *ipd_marker = strstr((char *)rx_buf, "+IPD,");
            if (ipd_marker)
            {
                int ipd_payload_len_val;
                char *colon_ptr = strchr(ipd_marker, ':');
                char *first_comma_ptr = strchr(ipd_marker, ',');

                if (first_comma_ptr && colon_ptr && (colon_ptr > first_comma_ptr))
                {
                    if (sscanf(first_comma_ptr + 1, "%d", &ipd_payload_len_val) == 1)
                    {
                        uint8_t *mqtt_data_ptr = (uint8_t *)(colon_ptr + 1);
                        uint16_t actual_bytes_after_colon = rx_len - (mqtt_data_ptr - rx_buf);

                        if (actual_bytes_after_colon >= ipd_payload_len_val)
                        {
                            if (ipd_payload_len_val >= 4 && mqtt_data_ptr[0] == 0x20 && mqtt_data_ptr[1] == 0x02)
                            {
                                if (mqtt_data_ptr[3] == 0x00)
                                {
                                    _esp_login("[MQTT] >>> CONNACK OK (0x00) reçu");
                                    found_connack = true;
                                }
                                else
                                {
                                    _esp_login("[MQTT] >>> CONNACK Error Code: 0x%02X", mqtt_data_ptr[3]);
                                    status = ESP01_CONNECTION_ERROR;
                                    found_connack = true;
                                }
                            }
                            else
                            {
                                _esp_login("[MQTT] >>> IPD: Données reçues, mais pas un CONNACK valide ou trop court");
                            }
                        }
                        else
                        {
                            _esp_login("[MQTT] >>> IPD: Payload incomplet, attente de plus de données");
                        }
                    }
                    else
                    {
                        _esp_login("[MQTT] >>> IPD: Impossible de parser la longueur du header IPD");
                    }
                }
                else
                {
                    _esp_login("[MQTT] >>> IPD: Header mal formé ou fragmenté");
                }
            }
        }
        if (!found_connack)
            HAL_Delay(100);
    }

    if (found_connack)
    {
        _esp_login("[MQTT] === CONNACK détecté, connexion établie ===");
        status = ESP01_OK;
    }
    else
    {
        _esp_login("[MQTT] >>> Aucun CONNACK détecté après 10 secondes");
        _esp_login("[MQTT] >>> Vérifiez la configuration du broker et les paramètres de connexion");
        status = ESP01_TIMEOUT;
    }

    if (status == ESP01_OK)
    {
        g_mqtt_client.connected = true;
        strncpy(g_mqtt_client.broker_ip, broker_ip, sizeof(g_mqtt_client.broker_ip) - 1);
        g_mqtt_client.broker_port = port;
        strncpy(g_mqtt_client.client_id, client_id, sizeof(g_mqtt_client.client_id) - 1);
        g_mqtt_client.keep_alive = 60;
        g_mqtt_client.packet_id = 1;
        _esp_login("[MQTT] === Connexion établie avec succès ===");
    }
    else
    {
        _esp_login("[MQTT] >>> Échec de la connexion");
    }

    return status;
}

// ==================== PUBLISH MQTT ====================
ESP01_Status_t esp01_mqtt_publish(const char *topic, const char *message,
                                  uint8_t qos, bool retain)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF]; // Buffers pour commandes et réponses
    ESP01_Status_t status;                                 // Statut de retour

    VALIDATE_PARAM(topic && message && qos <= 2, ESP01_INVALID_PARAM); // Vérifie les paramètres
    VALIDATE_PARAM(g_mqtt_client.connected, ESP01_FAIL);               // Vérifie la connexion

    _esp_login("[MQTT] === Préparation publication ===");

    uint8_t mqtt_publish[512]; // Buffer pour le paquet MQTT PUBLISH
    uint16_t mqtt_len = 0;     // Taille du paquet MQTT

    mqtt_publish[mqtt_len++] = 0x30 | (qos << 1) | (retain ? 1 : 0); // Header PUBLISH

    uint16_t topic_len = strlen(topic);     // Longueur du topic
    uint16_t message_len = strlen(message); // Longueur du message

    uint16_t var_len = 2 + topic_len; // Longueur variable (topic)
    if (qos > 0)
        var_len += 2;       // Ajoute Packet ID si QoS > 0
    var_len += message_len; // Ajoute la longueur du message

    // Encodage de la longueur variable
    if (var_len < 128)
    {
        mqtt_publish[mqtt_len++] = var_len;
    }
    else
    {
        mqtt_publish[mqtt_len++] = (var_len & 0x7F) | 0x80;
        mqtt_publish[mqtt_len++] = var_len >> 7;
    }

    // Topic
    mqtt_publish[mqtt_len++] = (topic_len >> 8) & 0xFF;
    mqtt_publish[mqtt_len++] = topic_len & 0xFF;
    memcpy(&mqtt_publish[mqtt_len], topic, topic_len); // Copie le topic
    mqtt_len += topic_len;

    // Packet ID si QoS > 0
    if (qos > 0)
    {
        mqtt_publish[mqtt_len++] = (g_mqtt_client.packet_id >> 8) & 0xFF;
        mqtt_publish[mqtt_len++] = g_mqtt_client.packet_id & 0xFF;
        g_mqtt_client.packet_id++;
    }

    // Message
    memcpy(&mqtt_publish[mqtt_len], message, message_len); // Copie le message
    mqtt_len += message_len;

    _esp_login("[MQTT] === Envoi paquet PUBLISH ===");
    for (int i = 0; i < mqtt_len && i < 32; i++)
    {
        _esp_login("[MQTT] >>> Byte %02X", mqtt_publish[i]);
    }

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len); // Prépare la commande AT+CIPSEND
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
    {
        _esp_login("[MQTT] >>> Échec préparation envoi");
        return status;
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_publish, mqtt_len, HAL_MAX_DELAY); // Envoie le paquet MQTT

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_MEDIUM); // Attend l'accusé d'envoi

    if (status == ESP01_OK)
    {
        _esp_login("[MQTT] >>> Message publié sur %s: %s", topic, message);

        // Attente du PUBACK si QoS 1
        if (qos == 1)
        {
            uint8_t rx_buf[32];
            bool found_puback = false;
            uint32_t start = HAL_GetTick();

            _esp_login("[MQTT] === Attente du PUBACK ===");

            while ((HAL_GetTick() - start) < 2000 && !found_puback)
            {
                uint16_t rx_len = esp01_get_new_data(rx_buf, sizeof(rx_buf));
                if (rx_len > 0)
                {
                    for (uint16_t i = 0; i + 1 < rx_len; ++i)
                    {
                        if (rx_buf[i] == 0x40)
                        {
                            _esp_login("[MQTT] >>> PUBACK reçu");
                            found_puback = true;
                            break;
                        }
                    }
                }
                HAL_Delay(50);
            }

            if (!found_puback)
            {
                _esp_login("[MQTT] >>> Pas de PUBACK reçu");
            }
        }
    }
    else
    {
        _esp_login("[MQTT] >>> Échec de la publication");
    }

    return status;
}

// ==================== SUBSCRIBE MQTT ====================
ESP01_Status_t esp01_mqtt_subscribe(const char *topic, uint8_t qos)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF]; // Buffers pour commandes et réponses
    ESP01_Status_t status;                                 // Statut de retour

    VALIDATE_PARAM(topic && qos <= 2, ESP01_INVALID_PARAM); // Vérifie les paramètres
    VALIDATE_PARAM(g_mqtt_client.connected, ESP01_FAIL);    // Vérifie la connexion

    uint8_t mqtt_subscribe[256]; // Buffer pour le paquet MQTT SUBSCRIBE
    uint16_t mqtt_len = 0;       // Taille du paquet

    mqtt_subscribe[mqtt_len++] = 0x82; // Header SUBSCRIBE

    uint16_t len_pos = mqtt_len++; // Position du champ "remaining length"

    // Packet ID
    mqtt_subscribe[mqtt_len++] = g_mqtt_client.packet_id >> 8;
    mqtt_subscribe[mqtt_len++] = g_mqtt_client.packet_id & 0xFF;
    g_mqtt_client.packet_id++;

    // Topic
    uint16_t topic_len = strlen(topic);
    mqtt_subscribe[mqtt_len++] = topic_len >> 8;
    mqtt_subscribe[mqtt_len++] = topic_len & 0xFF;
    memcpy(&mqtt_subscribe[mqtt_len], topic, topic_len); // Copie le topic
    mqtt_len += topic_len;

    mqtt_subscribe[mqtt_len++] = qos; // QoS

    mqtt_subscribe[len_pos] = mqtt_len - 2; // Encode la longueur variable

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len); // Prépare la commande AT+CIPSEND
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
        return status;

    HAL_UART_Transmit(g_esp_uart, mqtt_subscribe, mqtt_len, HAL_MAX_DELAY); // Envoie le paquet MQTT

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT); // Attend l'accusé d'envoi

    if (status == ESP01_OK)
    {
        _esp_login("[MQTT] === Abonnement au topic %s ===", topic);
    }
    else
    {
        _esp_login("[MQTT] >>> Échec de l'abonnement");
    }

    return status;
}

// ==================== PING MQTT ====================
ESP01_Status_t esp01_mqtt_ping(void)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF]; // Buffers pour commandes et réponses
    ESP01_Status_t status;                                 // Statut de retour

    if (!g_mqtt_client.connected)
        return ESP01_FAIL;

    _esp_login("[MQTT] === Envoi PINGREQ (keepalive) ===");

    uint8_t mqtt_pingreq[2];
    mqtt_pingreq[0] = 0xC0;
    mqtt_pingreq[1] = 0x00;

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", 2); // Prépare la commande AT+CIPSEND
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
    {
        _esp_login("[MQTT] >>> Échec préparation envoi PINGREQ");
        return status;
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_pingreq, 2, HAL_MAX_DELAY); // Envoie le paquet PINGREQ

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT); // Attend l'accusé d'envoi

    if (status == ESP01_OK)
    {
        _esp_login("[MQTT] === PINGREQ envoyé avec succès ===");

        uint8_t rx_buf[32];
        bool found_pingresp = false;
        uint32_t start = HAL_GetTick();

        while ((HAL_GetTick() - start) < 2000 && !found_pingresp)
        {
            uint16_t rx_len = esp01_get_new_data(rx_buf, sizeof(rx_buf));
            if (rx_len > 0)
            {
                for (uint16_t i = 0; i + 1 < rx_len; ++i)
                {
                    if (rx_buf[i] == 0xD0 && rx_buf[i + 1] == 0x00)
                    {
                        _esp_login("[MQTT] >>> PINGRESP reçu");
                        found_pingresp = true;
                        break;
                    }
                }
            }
            HAL_Delay(50);
        }

        if (!found_pingresp)
        {
            _esp_login("[MQTT] >>> Pas de PINGRESP reçu");
        }
    }
    else
    {
        _esp_login("[MQTT] >>> Échec de l'envoi du PINGREQ");
    }

    return status;
}

// ==================== DECONNEXION MQTT ====================
ESP01_Status_t esp01_mqtt_disconnect(void)
{
    char resp[ESP01_DMA_RX_BUF_SIZE]; // Buffer pour la réponse
    if (esp01_send_raw_command_dma("AT+CIPCLOSE", resp, sizeof(resp), "OK", 3000) == ESP01_OK || strstr(resp, "CLOSED"))
    {
        return ESP01_OK; // Déconnexion réussie
    }
    return ESP01_TIMEOUT; // Timeout ou erreur
}

// ==================== CALLBACK MESSAGE MQTT ====================
void esp01_mqtt_set_message_callback(mqtt_message_callback_t cb)
{
    g_mqtt_cb = cb; // Enregistre le callback utilisateur
}

// ==================== POLLING MQTT ====================
void esp01_mqtt_poll(void)
{
    uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];                     // Buffer temporaire pour les nouvelles données
    uint16_t len = esp01_get_new_data(buffer, sizeof(buffer)); // Récupère les nouvelles données UART
    if (len > 0)
    {
        if (g_acc_len + len < sizeof(g_accumulator) - 1)
        {
            memcpy(g_accumulator + g_acc_len, buffer, len); // Ajoute au buffer accumulateur
            g_acc_len += len;
            g_accumulator[g_acc_len] = '\0';
        }
        else
        {
            g_acc_len = 0;
            g_accumulator[0] = '\0';
            _esp_login("[MQTT] [ERROR] Débordement de l'accumulateur MQTT");
            return;
        }
    }

    // Traitement des paquets MQTT dans l'accumulateur
    while (1)
    {
        char *ipd_start = strstr(g_accumulator, "+IPD,"); // Cherche le début d'un paquet IPD
        if (!ipd_start)
            break;

        char *colon_pos = strchr(ipd_start, ':'); // Cherche le séparateur
        if (!colon_pos)
            break;

        int payload_len = 0;
        char *len_ptr = ipd_start + 5;
        if (sscanf(len_ptr, "%d", &payload_len) != 1)
        {
            _esp_login("[MQTT] [WARN] Impossible de parser la longueur du +IPD");
            break;
        }

        int ipd_start_offset = ipd_start - g_accumulator;
        int ipd_total_len = (colon_pos - ipd_start) + 1 + payload_len;

        if (g_acc_len - ipd_start_offset < ipd_total_len)
        {
            _esp_login("[MQTT] Attente suite: ipd_start_offset=%d, g_acc_len=%d, ipd_total_len=%d",
                       ipd_start_offset, g_acc_len, ipd_total_len);
            break;
        }

        uint8_t *payload = (uint8_t *)(colon_pos + 1);

        // Décodage d'un paquet PUBLISH MQTT
        if (payload_len > 4 && payload[0] == 0x30 && g_mqtt_cb)
        {
            uint16_t topic_len = (payload[2] << 8) | payload[3];
            if (topic_len + 4 < payload_len && topic_len < 64)
            {
                char topic_buf[64] = {0};
                memcpy(topic_buf, &payload[4], topic_len); // Copie le topic
                int msg_len = payload_len - 4 - topic_len;
                if (msg_len > 127)
                    msg_len = 127;
                char msg_buf[128] = {0};
                memcpy(msg_buf, &payload[4 + topic_len], msg_len); // Copie le message
                msg_buf[msg_len] = 0;
                g_mqtt_cb(topic_buf, msg_buf); // Appelle le callback utilisateur
            }
            else
            {
                _esp_login("[MQTT] [WARN] Paquet PUBLISH mal formé ou topic/message trop long");
            }
        }

        // Retire le paquet traité de l'accumulateur
        int total_to_remove = ipd_start_offset + ipd_total_len;
        if (g_acc_len > total_to_remove)
        {
            memmove(g_accumulator, g_accumulator + total_to_remove, g_acc_len - total_to_remove); // Décale le buffer
            g_acc_len -= total_to_remove;
            g_accumulator[g_acc_len] = '\0';
        }
        else
        {
            g_acc_len = 0;
            g_accumulator[0] = '\0';
        }
    }
}
