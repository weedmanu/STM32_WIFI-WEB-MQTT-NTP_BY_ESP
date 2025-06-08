/*
 * STM32_WifiESP_MQTT.c
 * Fonctions MQTT pour le driver STM32 <-> ESP01 (AT)
 * Version Final V1.0
 * 2025 - manu
 */

#include "STM32_WifiESP.h"
#include "STM32_WifiESP_Utils.h"
#include "STM32_WifiESP_MQTT.h"

static mqtt_message_callback_t g_mqtt_cb = NULL;

// ==================== CONNEXION MQTT ====================
ESP01_Status_t esp01_mqtt_connect(const char *broker_ip, uint16_t port,
                                  const char *client_id, const char *username,
                                  const char *password)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t status;

    VALIDATE_PARAM(broker_ip && client_id, ESP01_INVALID_PARAM);

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", broker_ip, port);
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);
    _esp_logln(resp);
    if (status != ESP01_OK)
        return status;

    HAL_Delay(500);

    uint8_t mqtt_connect[256];
    uint16_t mqtt_len = 0;

    mqtt_connect[mqtt_len++] = 0x10;

    uint16_t len_pos = mqtt_len++;

    uint16_t var_len = 10;

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

    mqtt_connect[mqtt_len++] = 0x00;
    mqtt_connect[mqtt_len++] = 0x04;
    mqtt_connect[mqtt_len++] = 'M';
    mqtt_connect[mqtt_len++] = 'Q';
    mqtt_connect[mqtt_len++] = 'T';
    mqtt_connect[mqtt_len++] = 'T';

    mqtt_connect[mqtt_len++] = 0x04;

    uint8_t connect_flags = 0x02;

    if (has_username)
        connect_flags |= 0x80;
    if (has_password)
        connect_flags |= 0x40;

    mqtt_connect[mqtt_len++] = connect_flags;

    mqtt_connect[mqtt_len++] = 0x00;
    mqtt_connect[mqtt_len++] = 0x3C;

    mqtt_connect[mqtt_len++] = (client_id_len >> 8) & 0xFF;
    mqtt_connect[mqtt_len++] = client_id_len & 0xFF;
    memcpy(&mqtt_connect[mqtt_len], client_id, client_id_len);
    mqtt_len += client_id_len;

    if (has_username)
    {
        mqtt_connect[mqtt_len++] = (username_len >> 8) & 0xFF;
        mqtt_connect[mqtt_len++] = username_len & 0xFF;
        memcpy(&mqtt_connect[mqtt_len], username, username_len);
        mqtt_len += username_len;
    }

    if (has_password)
    {
        mqtt_connect[mqtt_len++] = (password_len >> 8) & 0xFF;
        mqtt_connect[mqtt_len++] = password_len & 0xFF;
        memcpy(&mqtt_connect[mqtt_len], password, password_len);
        mqtt_len += password_len;
    }

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

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len);
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
        return status;

    _esp_logln("[MQTT] Envoi du paquet CONNECT:");
    for (int i = 0; i < mqtt_len; ++i)
    {
        char dbg[32];
        snprintf(dbg, sizeof(dbg), "[MQTT] TX[%03d]: %02X", i, mqtt_connect[i]);
        _esp_logln(dbg);
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_connect, mqtt_len, HAL_MAX_DELAY);

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
        return status;

    uint8_t rx_buf[256];
    memset(rx_buf, 0, sizeof(rx_buf));
    bool found_connack = false;
    uint32_t start = HAL_GetTick();

    _esp_logln("[MQTT] Attente du CONNACK (timeout 10s)...");

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
                                    _esp_logln("[MQTT] CONNACK OK (0x00) received.");
                                    found_connack = true;
                                }
                                else
                                {
                                    char err_msg[64];
                                    snprintf(err_msg, sizeof(err_msg), "[MQTT] CONNACK Error Code: 0x%02X", mqtt_data_ptr[3]);
                                    _esp_logln(err_msg);
                                    status = ESP01_CONNECTION_ERROR;
                                    found_connack = true;
                                }
                            }
                            else
                            {
                                _esp_logln("[MQTT] IPD: Received data, but not a valid CONNACK or too short based on IPD length.");
                            }
                        }
                        else
                        {
                            _esp_logln("[MQTT] IPD: Payload incomplete in current rx_buf. Waiting for more data.");
                        }
                    }
                    else
                    {
                        _esp_logln("[MQTT] IPD: Failed to parse <length> from IPD header.");
                    }
                }
                else
                {
                    _esp_logln("[MQTT] IPD: Malformed or fragmented (expected comma and colon not found/ordered correctly).");
                }
            }
        }
        if (!found_connack)
            HAL_Delay(100);
    }

    if (found_connack)
    {
        _esp_logln("[MQTT] CONNACK détecté, connexion établie");
        status = ESP01_OK;
    }
    else
    {
        _esp_logln("[MQTT] Aucun CONNACK détecté après 10 secondes");
        _esp_logln("[MQTT] Vérifiez la configuration du broker et les paramètres de connexion");
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

        _esp_logln("[MQTT] Connexion établie avec succès");
    }
    else
    {
        _esp_logln("[MQTT] Échec de la connexion");
    }

    return status;
}

// ==================== PUBLISH MQTT ====================
ESP01_Status_t esp01_mqtt_publish(const char *topic, const char *message,
                                  uint8_t qos, bool retain)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t status;

    VALIDATE_PARAM(topic && message && qos <= 2, ESP01_INVALID_PARAM);
    VALIDATE_PARAM(g_mqtt_client.connected, ESP01_FAIL);

    _esp_logln("[MQTT] Préparation publication...");

    uint8_t mqtt_publish[512];
    uint16_t mqtt_len = 0;

    mqtt_publish[mqtt_len++] = 0x30 | (qos << 1) | (retain ? 1 : 0);

    uint16_t topic_len = strlen(topic);
    uint16_t message_len = strlen(message);

    uint16_t var_len = 2 + topic_len;

    if (qos > 0)
    {
        var_len += 2;
    }

    var_len += message_len;

    if (var_len < 128)
    {
        mqtt_publish[mqtt_len++] = var_len;
    }
    else
    {
        mqtt_publish[mqtt_len++] = (var_len & 0x7F) | 0x80;
        mqtt_publish[mqtt_len++] = var_len >> 7;
    }

    mqtt_publish[mqtt_len++] = (topic_len >> 8) & 0xFF;
    mqtt_publish[mqtt_len++] = topic_len & 0xFF;
    memcpy(&mqtt_publish[mqtt_len], topic, topic_len);
    mqtt_len += topic_len;

    if (qos > 0)
    {
        mqtt_publish[mqtt_len++] = (g_mqtt_client.packet_id >> 8) & 0xFF;
        mqtt_publish[mqtt_len++] = g_mqtt_client.packet_id & 0xFF;
        g_mqtt_client.packet_id++;
    }

    memcpy(&mqtt_publish[mqtt_len], message, message_len);
    mqtt_len += message_len;

    _esp_logln("[MQTT] Envoi paquet PUBLISH:");
    for (int i = 0; i < mqtt_len && i < 32; i++)
    {
        char dbg[32];
        snprintf(dbg, sizeof(dbg), "[MQTT] Byte %02X", mqtt_publish[i]);
        _esp_logln(dbg);
    }

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len);
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
    {
        _esp_logln("[MQTT] Échec préparation envoi");
        return status;
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_publish, mqtt_len, HAL_MAX_DELAY);

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_MEDIUM);

    if (status == ESP01_OK)
    {
        char dbg[ESP01_MAX_DBG_BUF];
        snprintf(dbg, sizeof(dbg), "[MQTT] Message publié sur %s: %s", topic, message);
        _esp_logln(dbg);

        if (qos == 1)
        {
            uint8_t rx_buf[32];
            bool found_puback = false;
            uint32_t start = HAL_GetTick();

            _esp_logln("[MQTT] Attente du PUBACK...");

            while ((HAL_GetTick() - start) < 2000 && !found_puback)
            {
                uint16_t rx_len = esp01_get_new_data(rx_buf, sizeof(rx_buf));
                if (rx_len > 0)
                {
                    for (uint16_t i = 0; i + 1 < rx_len; ++i)
                    {
                        if (rx_buf[i] == 0x40)
                        {
                            _esp_logln("[MQTT] PUBACK reçu");
                            found_puback = true;
                            break;
                        }
                    }
                }
                HAL_Delay(50);
            }

            if (!found_puback)
            {
                _esp_logln("[MQTT] Pas de PUBACK reçu");
            }
        }
    }
    else
    {
        _esp_logln("[MQTT] Échec de la publication");
    }

    return status;
}

// ==================== SUBSCRIBE MQTT ====================
ESP01_Status_t esp01_mqtt_subscribe(const char *topic, uint8_t qos)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t status;

    VALIDATE_PARAM(topic && qos <= 2, ESP01_INVALID_PARAM);
    VALIDATE_PARAM(g_mqtt_client.connected, ESP01_FAIL);

    uint8_t mqtt_subscribe[256];
    uint16_t mqtt_len = 0;

    mqtt_subscribe[mqtt_len++] = 0x82;

    uint16_t len_pos = mqtt_len++;

    mqtt_subscribe[mqtt_len++] = g_mqtt_client.packet_id >> 8;
    mqtt_subscribe[mqtt_len++] = g_mqtt_client.packet_id & 0xFF;
    g_mqtt_client.packet_id++;

    uint16_t topic_len = strlen(topic);
    mqtt_subscribe[mqtt_len++] = topic_len >> 8;
    mqtt_subscribe[mqtt_len++] = topic_len & 0xFF;
    memcpy(&mqtt_subscribe[mqtt_len], topic, topic_len);
    mqtt_len += topic_len;

    mqtt_subscribe[mqtt_len++] = qos;

    mqtt_subscribe[len_pos] = mqtt_len - 2;

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", mqtt_len);
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
        return status;

    HAL_UART_Transmit(g_esp_uart, mqtt_subscribe, mqtt_len, HAL_MAX_DELAY);

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT);

    if (status == ESP01_OK)
    {
        char dbg[ESP01_MAX_DBG_BUF];
        snprintf(dbg, sizeof(dbg), "[MQTT] Abonnement au topic %s", topic);
        _esp_logln(dbg);
    }
    else
    {
        _esp_logln("[MQTT] Échec de l'abonnement");
    }

    return status;
}

// ==================== PING MQTT ====================
ESP01_Status_t esp01_mqtt_ping(void)
{
    char cmd[ESP01_MAX_CMD_BUF], resp[ESP01_MAX_RESP_BUF];
    ESP01_Status_t status;

    if (!g_mqtt_client.connected)
        return ESP01_FAIL;

    _esp_logln("[MQTT] Envoi PINGREQ (keepalive)...");

    uint8_t mqtt_pingreq[2];
    mqtt_pingreq[0] = 0xC0;
    mqtt_pingreq[1] = 0x00;

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", 2);
    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
    {
        _esp_logln("[MQTT] Échec préparation envoi PINGREQ");
        return status;
    }

    HAL_UART_Transmit(g_esp_uart, mqtt_pingreq, 2, HAL_MAX_DELAY);

    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_SHORT);

    if (status == ESP01_OK)
    {
        _esp_logln("[MQTT] PINGREQ envoyé avec succès");

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
                        _esp_logln("[MQTT] PINGRESP reçu");
                        found_pingresp = true;
                        break;
                    }
                }
            }
            HAL_Delay(50);
        }

        if (!found_pingresp)
        {
            _esp_logln("[MQTT] Pas de PINGRESP reçu");
        }
    }
    else
    {
        _esp_logln("[MQTT] Échec de l'envoi du PINGREQ");
    }

    return status;
}

// ==================== DECONNEXION MQTT ====================
ESP01_Status_t esp01_mqtt_disconnect(void)
{
    char resp[ESP01_DMA_RX_BUF_SIZE];
    if (esp01_send_raw_command_dma("AT+CIPCLOSE", resp, sizeof(resp), "OK", 3000) == ESP01_OK || strstr(resp, "CLOSED"))
    {
        return ESP01_OK;
    }
    return ESP01_TIMEOUT;
}

// ==================== CALLBACK MESSAGE MQTT ====================
void esp01_mqtt_set_message_callback(mqtt_message_callback_t cb)
{
    g_mqtt_cb = cb;
}

// ==================== POLLING MQTT ====================
void esp01_mqtt_poll(void)
{
    uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];
    uint16_t len = esp01_get_new_data(buffer, sizeof(buffer));
    if (len > 0)
    {
        if (g_acc_len + len < sizeof(g_accumulator) - 1)
        {
            memcpy(g_accumulator + g_acc_len, buffer, len);
            g_acc_len += len;
            g_accumulator[g_acc_len] = '\0';
        }
        else
        {
            g_acc_len = 0;
            g_accumulator[0] = '\0';
            _esp_logln("[MQTT] [ERROR] Débordement de l'accumulateur MQTT");
            return;
        }
    }

    while (1)
    {
        char *ipd_start = strstr(g_accumulator, "+IPD,");
        if (!ipd_start)
            break;

        char *colon_pos = strchr(ipd_start, ':');
        if (!colon_pos)
            break;

        int payload_len = 0;
        char *len_ptr = ipd_start + 5;
        if (sscanf(len_ptr, "%d", &payload_len) != 1)
        {
            _esp_logln("[MQTT] [WARN] Impossible de parser la longueur du +IPD");
            break;
        }

        int ipd_start_offset = ipd_start - g_accumulator;
        int ipd_total_len = (colon_pos - ipd_start) + 1 + payload_len;

        if (g_acc_len - ipd_start_offset < ipd_total_len)
        {
            char dbg[128];
            snprintf(dbg, sizeof(dbg),
                     "[MQTT] Attente suite: ipd_start_offset=%d, g_acc_len=%d, ipd_total_len=%d",
                     ipd_start_offset, g_acc_len, ipd_total_len);
            _esp_logln(dbg);
            break;
        }

        uint8_t *payload = (uint8_t *)(colon_pos + 1);

        if (payload_len > 4 && payload[0] == 0x30 && g_mqtt_cb)
        {
            uint16_t topic_len = (payload[2] << 8) | payload[3];
            if (topic_len + 4 < payload_len && topic_len < 64)
            {
                char topic_buf[64] = {0};
                memcpy(topic_buf, &payload[4], topic_len);
                int msg_len = payload_len - 4 - topic_len;
                if (msg_len > 127)
                    msg_len = 127;
                char msg_buf[128] = {0};
                memcpy(msg_buf, &payload[4 + topic_len], msg_len);
                msg_buf[msg_len] = 0;
                g_mqtt_cb(topic_buf, msg_buf);
            }
            else
            {
                _esp_logln("[MQTT] [WARN] Paquet PUBLISH mal formé ou topic/message trop long");
            }
        }

        int total_to_remove = ipd_start_offset + ipd_total_len;
        if (g_acc_len > total_to_remove)
        {
            memmove(g_accumulator, g_accumulator + total_to_remove, g_acc_len - total_to_remove);
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
