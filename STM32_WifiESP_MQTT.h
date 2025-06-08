#ifndef INC_STM32_WIFIESP_MQTT_H_
#define INC_STM32_WIFIESP_MQTT_H_

#include "STM32_WifiESP.h"

// ==================== TYPES MQTT ====================

/**
 * @brief Prototype du callback appelé lors de la réception d'un message MQTT.
 * @param topic   Topic du message reçu.
 * @param payload Payload du message reçu.
 */
typedef void (*mqtt_message_callback_t)(const char *topic, const char *payload);

// ==================== FONCTIONS MQTT ====================

/**
 * @brief Configure une connexion MQTT.
 * @param broker_ip Adresse IP du broker MQTT.
 * @param port Port du broker MQTT.
 * @param client_id Identifiant client MQTT.
 * @param username Nom d'utilisateur (NULL si non utilisé).
 * @param password Mot de passe (NULL si non utilisé).
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_mqtt_connect(const char *broker_ip, uint16_t port,
                                  const char *client_id, const char *username,
                                  const char *password);

/**
 * @brief Publie un message sur un topic MQTT.
 * @param topic Nom du topic.
 * @param message Message à publier.
 * @param qos Qualité de service (0, 1 ou 2).
 * @param retain Flag de rétention.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_mqtt_publish(const char *topic, const char *message,
                                  uint8_t qos, bool retain);

/**
 * @brief S'abonne à un topic MQTT.
 * @param topic Nom du topic.
 * @param qos Qualité de service (0, 1 ou 2).
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_mqtt_subscribe(const char *topic, uint8_t qos);

/**
 * @brief Implémente un keepalive MQTT en envoyant un paquet PINGREQ.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_mqtt_ping(void);

/**
 * @brief Déconnecte le client MQTT.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_mqtt_disconnect(void);

/**
 * @brief Définit le callback appelé lors de la réception d'un message MQTT.
 * @param cb Fonction callback prenant le topic et le payload.
 */
void esp01_mqtt_set_message_callback(mqtt_message_callback_t cb);

/**
 * @brief Traite les messages MQTT reçus (à appeler régulièrement dans la boucle principale).
 */
void esp01_mqtt_poll(void);

#endif /* INC_STM32_WIFIESP_MQTT_H_ */