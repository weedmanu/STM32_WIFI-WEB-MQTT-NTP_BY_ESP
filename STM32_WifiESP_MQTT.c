/**
 ******************************************************************************
 * @file    STM32_WifiESP_MQTT.c
 * @author  manu
 * @version 1.1.0
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
#include "STM32_WifiESP_MQTT.h" // Header du module MQTT
#include "STM32_WifiESP.h"      // Fonctions de base ESP01
#include "STM32_WifiESP_WIFI.h" // Fonctions WiFi ESP01
#include <string.h>             // Fonctions de manipulation de chaînes
#include <stdio.h>              // Fonctions d'entrée/sortie

// ==================== CONSTANTES SPÉCIFIQUES AU MODULE ====================
#define ESP01_MQTT_CONNECT_TIMEOUT 5000   // Timeout connexion MQTT (ms)
#define ESP01_MQTT_CONNACK_TIMEOUT 10000  // Timeout CONNACK MQTT (ms)
#define ESP01_MQTT_PUBLISH_TIMEOUT 3000   // Timeout publication MQTT (ms)
#define ESP01_MQTT_SUBSCRIBE_TIMEOUT 3000 // Timeout abonnement MQTT (ms)

// ==================== DEFINES PRIVÉS ====================
#define MQTT_HEADER_CONNECT 0x10     // Paquet MQTT CONNECT
#define MQTT_HEADER_CONNACK 0x20     // Paquet MQTT CONNACK
#define MQTT_HEADER_PUBLISH 0x30     // Paquet MQTT PUBLISH
#define MQTT_HEADER_PUBACK 0x40      // Paquet MQTT PUBACK
#define MQTT_HEADER_SUBSCRIBE 0x82   // Paquet MQTT SUBSCRIBE
#define MQTT_HEADER_SUBACK 0x90      // Paquet MQTT SUBACK
#define MQTT_HEADER_UNSUBSCRIBE 0xA2 // Paquet MQTT UNSUBSCRIBE
#define MQTT_HEADER_PINGREQ 0xC0     // Paquet MQTT PINGREQ
#define MQTT_HEADER_PINGRESP 0xD0    // Paquet MQTT PINGRESP
#define MQTT_HEADER_DISCONNECT 0xE0  // Paquet MQTT DISCONNECT

#define MQTT_PROTOCOL_VERSION 0x04   // Version du protocole MQTT (3.1.1)
#define MQTT_FLAG_CLEAN_SESSION 0x02 // Flag clean session
#define MQTT_FLAG_WILL 0x04          // Flag will
#define MQTT_FLAG_WILL_RETAIN 0x20   // Flag will retain
#define MQTT_FLAG_USERNAME 0x80      // Flag username
#define MQTT_FLAG_PASSWORD 0x40      // Flag password
#define ESP01_MQTT_MAX_PACKET_SIZE 2048
// ==================== VARIABLES GLOBALES ====================
mqtt_client_t g_mqtt_client = {0};                    // Instance globale du client MQTT
static mqtt_message_callback_t g_mqtt_cb = NULL;      // Callback utilisateur pour réception de messages
static uint8_t g_mqtt_accumulator[ESP01_MAX_CMD_BUF]; // Buffer d'accumulation pour messages MQTT
static uint16_t g_mqtt_acc_len = 0;                   // Longueur actuelle de l'accumulateur MQTT

// ==================== CONNEXION MQTT ====================
/**
 * @brief  Connexion au broker MQTT.
 * @param  broker_ip  Adresse IP du broker.
 * @param  port       Port du broker.
 * @param  client_id  Identifiant client.
 * @param  username   Nom d'utilisateur (optionnel, peut être NULL).
 * @param  password   Mot de passe (optionnel, peut être NULL).
 * @return ESP01_Status_t Code de retour (ESP01_OK si succès).
 */
ESP01_Status_t esp01_mqtt_connect(const char *broker_ip, uint16_t port, const char *client_id, const char *username, const char *password)
{
    ESP01_LOG_DEBUG("MQTT", "Connexion au broker %s:%u avec client_id=%s", broker_ip, port, client_id); // Log la tentative de connexion
    VALIDATE_PARAM(broker_ip && client_id, ESP01_INVALID_PARAM);                                        // Vérifie les paramètres

    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF]; // Buffers pour commandes et réponses
    ESP01_Status_t status;                                 // Statut de retour

    // Ouvre une connexion TCP vers le broker MQTT
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", broker_ip, port);             // Prépare la commande AT+CIPSTART
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM); // Envoie la commande
    ESP01_LOG_DEBUG("MQTT", "Réponse brute AT+CIPSTART : %s", resp);                          // Log la réponse brute
    if (status != ESP01_OK)
        return status; // Retourne en cas d'échec

    HAL_Delay(500); // Petite pause pour la stabilité

    // Construction du paquet MQTT CONNECT
    uint8_t mqtt_packet[ESP01_MAX_CMD_BUF]; // Buffer pour le paquet CONNECT
    uint16_t mqtt_len = 0;                  // Taille du paquet MQTT

    // En-tête
    mqtt_packet[mqtt_len++] = MQTT_HEADER_CONNECT;

    // Position pour la longueur variable (à remplir plus tard)
    uint16_t len_pos = mqtt_len++;

    // Protocole "MQTT"
    mqtt_packet[mqtt_len++] = 0x00;
    mqtt_packet[mqtt_len++] = 0x04;
    mqtt_packet[mqtt_len++] = 'M';
    mqtt_packet[mqtt_len++] = 'Q';
    mqtt_packet[mqtt_len++] = 'T';
    mqtt_packet[mqtt_len++] = 'T';
    mqtt_packet[mqtt_len++] = MQTT_PROTOCOL_VERSION;

    // Flags
    uint8_t connect_flags = MQTT_FLAG_CLEAN_SESSION;
    if (username != NULL && strlen(username) > 0)
        connect_flags |= MQTT_FLAG_USERNAME;
    if (password != NULL && strlen(password) > 0)
        connect_flags |= MQTT_FLAG_PASSWORD;
    mqtt_packet[mqtt_len++] = connect_flags;

    // Keep-alive (60s par défaut)
    mqtt_packet[mqtt_len++] = 0x00;
    mqtt_packet[mqtt_len++] = 0x3C;

    // Client ID
    uint16_t client_id_len = strlen(client_id);
    mqtt_packet[mqtt_len++] = client_id_len >> 8;
    mqtt_packet[mqtt_len++] = client_id_len & 0xFF;
    memcpy(&mqtt_packet[mqtt_len], client_id, client_id_len);
    mqtt_len += client_id_len;

    // Username si présent
    if (connect_flags & MQTT_FLAG_USERNAME)
    {
        uint16_t username_len = strlen(username);
        mqtt_packet[mqtt_len++] = username_len >> 8;
        mqtt_packet[mqtt_len++] = username_len & 0xFF;
        memcpy(&mqtt_packet[mqtt_len], username, username_len);
        mqtt_len += username_len;
    }

    // Password si présent
    if (connect_flags & MQTT_FLAG_PASSWORD)
    {
        uint16_t password_len = strlen(password);
        mqtt_packet[mqtt_len++] = password_len >> 8;
        mqtt_packet[mqtt_len++] = password_len & 0xFF;
        memcpy(&mqtt_packet[mqtt_len], password, password_len);
        mqtt_len += password_len;
    }

    // Longueur restante
    mqtt_packet[len_pos] = mqtt_len - 2;

    // Préparation de l'envoi CIPSEND
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len);                                  // Prépare la commande AT+CIPSEND
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT); // Envoie la commande
    if (status != ESP01_OK)
        return status; // Retourne en cas d'échec

    ESP01_LOG_DEBUG("MQTT", "Envoi du paquet CONNECT (%d octets)", mqtt_len); // Log l'envoi du paquet
    for (int i = 0; i < mqtt_len; i++)
    {
        ESP01_LOG_DEBUG("MQTT", ">>> TX[%03d]: %02X", i, mqtt_packet[i]); // Log chaque octet envoyé
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_packet, mqtt_len, HAL_MAX_DELAY); // Envoie le paquet CONNECT

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT); // Attend l'accusé d'envoi
    if (status != ESP01_OK)
        return status; // Retourne en cas d'échec

    // Attente du CONNACK
    uint8_t rx_buf[ESP01_MAX_RESP_BUF]; // Buffer pour la réponse CONNACK
    memset(rx_buf, 0, sizeof(rx_buf));  // Initialise le buffer
    bool found_connack = false;         // Indicateur de réception CONNACK
    uint32_t start = HAL_GetTick();     // Timestamp de départ

    ESP01_LOG_DEBUG("MQTT", "=== Attente du CONNACK (timeout 10s) ==="); // Log attente CONNACK
    HAL_Delay(500);                                                      // Petite pause

    while ((HAL_GetTick() - start) < ESP01_MQTT_CONNACK_TIMEOUT && !found_connack) // Boucle d'attente
    {
        uint16_t rx_len = esp01_get_new_data(rx_buf, sizeof(rx_buf)); // Récupère les nouvelles données
        if (rx_len > 0)
        {
            rx_buf[rx_len] = '\0';                              // Termine la chaîne
            char *ipd_marker = strstr((char *)rx_buf, "+IPD,"); // Cherche le header IPD
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
                            if (ipd_payload_len_val >= 4 && mqtt_data_ptr[0] == MQTT_HEADER_CONNACK && mqtt_data_ptr[1] == 0x02)
                            {
                                if (mqtt_data_ptr[3] == 0x00)
                                {
                                    ESP01_LOG_DEBUG("MQTT", "CONNACK OK (0x00) reçu"); // Log succès CONNACK
                                    found_connack = true;
                                }
                                else
                                {
                                    ESP01_LOG_ERROR("MQTT", "CONNACK Error Code: 0x%02X", mqtt_data_ptr[3]); // Log erreur CONNACK
                                    status = ESP01_CONNECTION_ERROR;
                                    found_connack = true;
                                }
                            }
                            else
                            {
                                ESP01_LOG_DEBUG("MQTT", ">>> IPD: Données reçues, mais pas un CONNACK valide ou trop court");
                            }
                        }
                        else
                        {
                            ESP01_LOG_DEBUG("MQTT", ">>> IPD: Payload incomplet, attente de plus de données");
                        }
                    }
                    else
                    {
                        ESP01_LOG_DEBUG("MQTT", ">>> IPD: Impossible de parser la longueur du header IPD");
                    }
                }
                else
                {
                    ESP01_LOG_DEBUG("MQTT", ">>> IPD: Header mal formé ou fragmenté");
                }
            }
        }
        if (!found_connack)
            HAL_Delay(100); // Petite pause si pas encore trouvé
    }

    if (found_connack)
    {
        ESP01_LOG_DEBUG("MQTT", "=== CONNACK détecté, connexion établie ==="); // Log succès
        status = ESP01_OK;
    }
    else
    {
        ESP01_LOG_WARN("MQTT", ">>> Aucun CONNACK détecté après 10 secondes"); // Log timeout
        ESP01_LOG_WARN("MQTT", ">>> Vérifiez la configuration du broker et les paramètres de connexion");
        status = ESP01_TIMEOUT;
    }

    if (status == ESP01_OK)
    {
        g_mqtt_client.connected = true;                                                         // Marque comme connecté
        esp01_safe_strcpy(g_mqtt_client.broker_ip, sizeof(g_mqtt_client.broker_ip), broker_ip); // Sauvegarde l'IP du broker (sécurisé)
        g_mqtt_client.broker_port = port;                                                       // Sauvegarde le port
        esp01_safe_strcpy(g_mqtt_client.client_id, sizeof(g_mqtt_client.client_id), client_id); // Sauvegarde le client ID (sécurisé)
        g_mqtt_client.keep_alive = ESP01_MQTT_KEEPALIVE_DEFAULT;                                // Keepalive par défaut
        g_mqtt_client.packet_id = 1;                                                            // Réinitialise le packet ID
        ESP01_LOG_DEBUG("MQTT", "=== Connexion établie avec succès ===");                       // Log succès
    }
    else
    {
        ESP01_LOG_ERROR("MQTT", ">>> Échec de la connexion"); // Log échec
    }

    return status; // Retourne le statut final
}

// ==================== PUBLISH MQTT ====================
/**
 * @brief  Publication d'un message sur un topic.
 * @param  topic   Sujet (topic) du message.
 * @param  message Message à publier.
 * @param  qos     Qualité de service (0, 1 ou 2).
 * @param  retain  true pour conserver le message sur le broker, false sinon.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_mqtt_publish(const char *topic, const char *message, uint8_t qos, bool retain)
{
    ESP01_LOG_DEBUG("MQTT", "Publication sur topic '%s', QoS=%d, retain=%d", topic, qos, retain); // Log la publication
    VALIDATE_PARAM(topic && message && qos <= 2, ESP01_INVALID_PARAM);                            // Vérifie les paramètres
    VALIDATE_PARAM(g_mqtt_client.connected, ESP01_FAIL);                                          // Vérifie la connexion

    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF]; // Buffers pour commandes et réponses
    ESP01_Status_t status;                                 // Statut de retour

    ESP01_LOG_DEBUG("MQTT", "=== Préparation publication ==="); // Log la préparation

    uint8_t mqtt_publish[ESP01_MQTT_MAX_PAYLOAD_LEN]; // Buffer pour le paquet MQTT PUBLISH
    uint16_t mqtt_len = 0;                            // Taille du paquet MQTT

    mqtt_publish[mqtt_len++] = MQTT_HEADER_PUBLISH | (qos << 1) | (retain ? 1 : 0); // Header PUBLISH

    uint16_t topic_len = strlen(topic);     // Longueur du topic
    uint16_t message_len = strlen(message); // Longueur du message

    uint16_t var_len = 2 + topic_len; // Longueur variable (topic)
    if (qos > 0)
        var_len += 2;       // Ajoute Packet ID si QoS > 0
    var_len += message_len; // Ajoute la longueur du message

    // Encodage de la longueur variable
    mqtt_publish[mqtt_len++] = var_len; // Longueur variable

    // Topic
    mqtt_publish[mqtt_len++] = (topic_len >> 8) & 0xFF; // Taille topic MSB
    mqtt_publish[mqtt_len++] = topic_len & 0xFF;        // Taille topic LSB
    memcpy(&mqtt_publish[mqtt_len], topic, topic_len);  // Copie le topic
    mqtt_len += topic_len;

    // Packet ID si QoS > 0
    if (qos > 0)
    {
        mqtt_publish[mqtt_len++] = (g_mqtt_client.packet_id >> 8) & 0xFF; // Packet ID MSB
        mqtt_publish[mqtt_len++] = g_mqtt_client.packet_id & 0xFF;        // Packet ID LSB
        g_mqtt_client.packet_id++;                                        // Incrémente le packet ID
    }

    // Message
    memcpy(&mqtt_publish[mqtt_len], message, message_len); // Copie le message
    mqtt_len += message_len;

    ESP01_LOG_DEBUG("MQTT", "=== Envoi paquet PUBLISH ==="); // Log l'envoi
    for (int i = 0; i < mqtt_len && i < 32; i++)
    {
        ESP01_LOG_DEBUG("MQTT", ">>> Byte %02X", mqtt_publish[i]); // Log chaque octet
    }

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len);                                  // Prépare la commande AT+CIPSEND
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT); // Envoie la commande
    if (status != ESP01_OK)
    {
        ESP01_LOG_WARN("MQTT", ">>> Échec préparation envoi"); // Log échec
        return status;
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_publish, mqtt_len, HAL_MAX_DELAY); // Envoie le paquet MQTT

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_MEDIUM); // Attend l'accusé d'envoi

    if (status == ESP01_OK)
    {
        ESP01_LOG_DEBUG("MQTT", "Message publié sur %s: %s", topic, message); // Log succès

        // Attente du PUBACK si QoS 1
        if (qos == 1)
        {
            uint8_t rx_buf[ESP01_SMALL_BUF_SIZE];
            bool found_puback = false;
            uint32_t start = HAL_GetTick();

            ESP01_LOG_DEBUG("MQTT", "=== Attente du PUBACK ==="); // Log attente PUBACK

            while ((HAL_GetTick() - start) < ESP01_MQTT_PUBLISH_TIMEOUT && !found_puback)
            {
                uint16_t rx_len = esp01_get_new_data(rx_buf, sizeof(rx_buf));
                if (rx_len > 0)
                {
                    for (uint16_t i = 0; i + 1 < rx_len; ++i)
                    {
                        if (rx_buf[i] == MQTT_HEADER_PUBACK)
                        {
                            ESP01_LOG_DEBUG("MQTT", ">>> PUBACK reçu"); // Log succès PUBACK
                            found_puback = true;
                            break;
                        }
                    }
                }
                HAL_Delay(50);
            }

            if (!found_puback)
            {
                ESP01_LOG_WARN("MQTT", ">>> Pas de PUBACK reçu"); // Log absence PUBACK
            }
        }
    }
    else
    {
        ESP01_LOG_ERROR("MQTT", "Échec de la publication"); // Log échec
    }

    return status; // Retourne le statut
}

// ==================== SUBSCRIBE MQTT ====================
/**
 * @brief  Souscription à un topic.
 * @param  topic Sujet (topic) à souscrire.
 * @param  qos   Qualité de service souhaitée.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_mqtt_subscribe(const char *topic, uint8_t qos)
{
    ESP01_LOG_DEBUG("MQTT", "Souscription au topic '%s', QoS=%d", topic, qos); // Log la souscription
    VALIDATE_PARAM(topic && qos <= 2, ESP01_INVALID_PARAM);                    // Vérifie les paramètres
    VALIDATE_PARAM(g_mqtt_client.connected, ESP01_FAIL);                       // Vérifie la connexion

    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF]; // Buffers pour commandes et réponses
    ESP01_Status_t status;                                 // Statut de retour

    uint8_t mqtt_subscribe[ESP01_MQTT_MAX_PACKET_SIZE]; // Buffer pour le paquet MQTT SUBSCRIBE
    uint16_t mqtt_len = 0;                              // Taille du paquet

    mqtt_subscribe[mqtt_len++] = MQTT_HEADER_SUBSCRIBE; // Header SUBSCRIBE (0x82)

    uint16_t len_pos = mqtt_len++; // Position du champ "remaining length"

    // Packet ID
    mqtt_subscribe[mqtt_len++] = g_mqtt_client.packet_id >> 8;   // Packet ID MSB
    mqtt_subscribe[mqtt_len++] = g_mqtt_client.packet_id & 0xFF; // Packet ID LSB
    g_mqtt_client.packet_id++;                                   // Incrémente le packet ID

    // Topic
    uint16_t topic_len = strlen(topic);                  // Longueur du topic
    mqtt_subscribe[mqtt_len++] = topic_len >> 8;         // Taille topic MSB
    mqtt_subscribe[mqtt_len++] = topic_len & 0xFF;       // Taille topic LSB
    memcpy(&mqtt_subscribe[mqtt_len], topic, topic_len); // Copie le topic
    mqtt_len += topic_len;

    mqtt_subscribe[mqtt_len++] = qos; // QoS

    mqtt_subscribe[len_pos] = mqtt_len - 2; // Encode la longueur variable

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len);                                  // Prépare la commande AT+CIPSEND
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT); // Envoie la commande
    if (status != ESP01_OK)
        return status; // Retourne en cas d'échec

    HAL_UART_Transmit(g_esp_uart, mqtt_subscribe, mqtt_len, HAL_MAX_DELAY); // Envoie le paquet MQTT

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT); // Attend l'accusé d'envoi

    if (status == ESP01_OK)
    {
        ESP01_LOG_DEBUG("MQTT", "Abonnement au topic %s réussi", topic); // Log succès
    }
    else
    {
        ESP01_LOG_ERROR("MQTT", "Échec de l'abonnement"); // Log échec
    }

    return status; // Retourne le statut
}

// ==================== PING MQTT ====================
/**
 * @brief  Envoie un ping MQTT (PINGREQ).
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_mqtt_ping(void)
{
    ESP01_LOG_DEBUG("MQTT", "Envoi PINGREQ");            // Log l'envoi du ping
    VALIDATE_PARAM(g_mqtt_client.connected, ESP01_FAIL); // Vérifie la connexion

    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF]; // Buffers pour commandes et réponses
    ESP01_Status_t status;                                 // Statut de retour

    ESP01_LOG_DEBUG("MQTT", "=== Envoi PINGREQ (keepalive) ==="); // Log la préparation

    uint8_t mqtt_pingreq[2];
    mqtt_pingreq[0] = MQTT_HEADER_PINGREQ; // Header PINGREQ (0xC0)
    mqtt_pingreq[1] = 0x00;                // Longueur

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", 2);                                         // Prépare la commande AT+CIPSEND
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT); // Envoie la commande
    if (status != ESP01_OK)
    {
        ESP01_LOG_WARN("MQTT", ">>> Échec préparation envoi PINGREQ"); // Log échec
        return status;
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_pingreq, 2, HAL_MAX_DELAY); // Envoie le paquet PINGREQ

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT); // Attend l'accusé d'envoi

    if (status == ESP01_OK)
    {
        ESP01_LOG_DEBUG("MQTT", "=== PINGREQ envoyé avec succès ==="); // Log succès

        uint8_t rx_buf[ESP01_SMALL_BUF_SIZE];
        bool found_pingresp = false;
        uint32_t start = HAL_GetTick();

        while ((HAL_GetTick() - start) < ESP01_MQTT_PUBLISH_TIMEOUT && !found_pingresp)
        {
            uint16_t rx_len = esp01_get_new_data(rx_buf, sizeof(rx_buf));
            if (rx_len > 0)
            {
                for (uint16_t i = 0; i + 1 < rx_len; ++i)
                {
                    if (rx_buf[i] == MQTT_HEADER_PINGRESP && rx_buf[i + 1] == 0x00)
                    {
                        ESP01_LOG_DEBUG("MQTT", "PINGRESP reçu"); // Log succès PINGRESP
                        found_pingresp = true;
                        break;
                    }
                }
            }
            HAL_Delay(50);
        }

        if (!found_pingresp)
        {
            ESP01_LOG_ERROR("MQTT", "Pas de PINGRESP reçu"); // Log absence PINGRESP
        }
    }
    else
    {
        ESP01_LOG_WARN("MQTT", ">>> Échec de l'envoi du PINGREQ"); // Log échec
    }

    return status; // Retourne le statut
}

// ==================== DECONNEXION MQTT ====================
/**
 * @brief  Déconnexion du broker MQTT.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_mqtt_disconnect(void)
{
    ESP01_LOG_DEBUG("MQTT", "Déconnexion du broker MQTT"); // Log la déconnexion
    char resp[ESP01_MAX_RESP_BUF];                         // Buffer pour la réponse
    if (esp01_send_raw_command_dma("AT+CIPCLOSE", resp, sizeof(resp), "OK", ESP01_AT_COMMAND_TIMEOUT) == ESP01_OK || strstr(resp, "CLOSED"))
    {
        g_mqtt_client.connected = false; // Marque comme déconnecté
        return ESP01_OK;                 // Déconnexion réussie
    }
    return ESP01_TIMEOUT; // Timeout ou erreur
}

// ==================== CALLBACK MESSAGE MQTT ====================
/**
 * @brief  Définit le callback utilisateur pour la réception de messages MQTT.
 * @param  cb Fonction callback à appeler lors de la réception d'un message.
 */
void esp01_mqtt_set_message_callback(mqtt_message_callback_t cb)
{
    ESP01_LOG_DEBUG("MQTT", "Callback message MQTT enregistré"); // Log l'enregistrement du callback
    g_mqtt_cb = cb;                                              // Enregistre le callback utilisateur
}

// ==================== POLLING MQTT ====================
/**
 * @brief  Fonction de polling MQTT à appeler régulièrement pour traiter les messages entrants.
 */
void esp01_mqtt_poll(void)
{
    uint8_t buffer[ESP01_MAX_RESP_BUF];                        // Buffer temporaire pour les nouvelles données
    uint16_t len = esp01_get_new_data(buffer, sizeof(buffer)); // Récupère les nouvelles données UART

    if (len > 0)
    {
        if (g_mqtt_acc_len + len < sizeof(g_mqtt_accumulator) - 1)
        {
            memcpy(g_mqtt_accumulator + g_mqtt_acc_len, buffer, len); // Ajoute au buffer accumulateur
            g_mqtt_acc_len += len;
            g_mqtt_accumulator[g_mqtt_acc_len] = '\0';
        }
        else
        {
            g_mqtt_acc_len = 0;
            g_mqtt_accumulator[0] = '\0';
            ESP01_LOG_ERROR("MQTT", "Débordement de l'accumulateur MQTT"); // Log débordement
            return;
        }
    }

    // Traitement des paquets MQTT dans l'accumulateur
    while (1)
    {
        char *ipd_start = strstr((char *)g_mqtt_accumulator, "+IPD,"); // Cherche le début d'un paquet IPD
        if (!ipd_start)
            break;

        char *colon_pos = strchr(ipd_start, ':'); // Cherche le séparateur
        if (!colon_pos)
            break;

        int payload_len = 0;
        char *len_ptr = ipd_start + 5;
        if (sscanf(len_ptr, "%d", &payload_len) != 1)
        {
            ESP01_LOG_WARN("MQTT", "Impossible de parser la longueur du +IPD"); // Log parsing échoué
            break;
        }

        int ipd_start_offset = ipd_start - (char *)g_mqtt_accumulator;
        int ipd_total_len = (colon_pos - ipd_start) + 1 + payload_len;

        if (g_mqtt_acc_len - ipd_start_offset < ipd_total_len)
        {
            ESP01_LOG_DEBUG("MQTT", "Attente suite: ipd_start_offset=%d, g_acc_len=%d, ipd_total_len=%d",
                            ipd_start_offset, g_mqtt_acc_len, ipd_total_len); // Log attente suite
            break;
        }

        uint8_t *payload = (uint8_t *)(colon_pos + 1);

        // Décodage d'un paquet PUBLISH MQTT
        if (payload_len > 4 && (payload[0] & 0xF0) == MQTT_HEADER_PUBLISH && g_mqtt_cb)
        {
            uint16_t topic_len = (payload[2] << 8) | payload[3];
            if (topic_len + 4 < payload_len && topic_len < ESP01_MQTT_MAX_TOPIC_LEN)
            {
                char topic_buf[ESP01_MQTT_MAX_TOPIC_LEN + 1] = {0};
                memcpy(topic_buf, &payload[4], topic_len); // Copie le topic
                topic_buf[topic_len] = '\0';

                int msg_len = payload_len - 4 - topic_len;
                if (msg_len > (ESP01_MQTT_MAX_PAYLOAD_LEN - 1))
                    msg_len = ESP01_MQTT_MAX_PAYLOAD_LEN - 1;

                char msg_buf[ESP01_MQTT_MAX_PAYLOAD_LEN] = {0};
                memcpy(msg_buf, &payload[4 + topic_len], msg_len); // Copie le message
                msg_buf[msg_len] = '\0';

                g_mqtt_cb(topic_buf, msg_buf);                                                                   // Appelle le callback utilisateur
                ESP01_LOG_DEBUG("MQTT", "Paquet PUBLISH reçu sur topic '%s', message='%s'", topic_buf, msg_buf); // Log réception
            }
            else
            {
                ESP01_LOG_WARN("MQTT", "Paquet PUBLISH mal formé ou topic/message trop long"); // Log erreur format
            }
        }

        // Retire le paquet traité de l'accumulateur
        int total_to_remove = ipd_start_offset + ipd_total_len;
        if (g_mqtt_acc_len > total_to_remove)
        {
            memmove(g_mqtt_accumulator, g_mqtt_accumulator + total_to_remove, g_mqtt_acc_len - total_to_remove); // Décale le buffer
            g_mqtt_acc_len -= total_to_remove;
            g_mqtt_accumulator[g_mqtt_acc_len] = '\0';
        }
        else
        {
            g_mqtt_acc_len = 0;
            g_mqtt_accumulator[0] = '\0';
        }
    }
}

// ==================== VÉRIFICATION CONNEXION MQTT ====================
/**
 * @brief  Vérifie et rétablit la connexion MQTT si nécessaire.
 * @return ESP01_Status_t Statut de l'opération.
 */
ESP01_Status_t esp01_mqtt_check_connection(void)
{
    if (!g_mqtt_client.connected)
    {
        ESP01_LOG_DEBUG("MQTT", "Connexion MQTT perdue, tentative de reconnexion");
        return esp01_mqtt_connect(
            g_mqtt_client.broker_ip,
            g_mqtt_client.broker_port,
            g_mqtt_client.client_id,
            NULL, NULL); // Sans username/password pour simplifier
    }
    return ESP01_OK;
}

// ========================= FIN DU MODULE =========================
