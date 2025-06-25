/**
 ******************************************************************************
 * @file    STM32_WifiESP_MQTT.h
 * @author  manu
 * @version 1.1.0
 * @date    2025
 * @brief   Bibliothèque de gestion MQTT pour le module ESP01 WiFi.
 *
 * @details
 * Ce header regroupe toutes les définitions, structures, macros et prototypes
 * de fonctions pour gérer la communication MQTT (client) via un module ESP01
 * connecté à un microcontrôleur STM32. Il propose des fonctions haut niveau
 * pour la connexion à un broker, la publication, la souscription, le ping,
 * la gestion des messages MQTT et le polling.
 *
 * @note
 * - Compatible STM32CubeIDE.
 * - Nécessite la bibliothèque STM32_WifiESP.h.
 ******************************************************************************
 */

#ifndef STM32_WIFIESP_MQTT_H_
#define STM32_WIFIESP_MQTT_H_

/* ========================== INCLUDES ========================== */
#include "STM32_WifiESP.h"      // Fonctions de base ESP01
#include "STM32_WifiESP_WIFI.h" // Fonctions WiFi ESP01
#include "STM32_WifiESP_HTTP.h" // Pour compatibilité éventuelle HTTP
#include <stdbool.h>            // Types booléens
#include <stdint.h>             // Types entiers standard

/* =========================== DEFINES ========================== */
// ----------- CONSTANTES MQTT -----------
#define ESP01_MQTT_MAX_TOPIC_LEN 128    // Taille max d'un topic MQTT
#define ESP01_MQTT_MAX_PAYLOAD_LEN 256  // Taille max d'un message MQTT
#define ESP01_MQTT_MAX_CLIENT_ID_LEN 32 // Taille max d'un client ID MQTT
#define ESP01_MQTT_KEEPALIVE_DEFAULT 60 // Keepalive par défaut (secondes)
#define ESP01_MQTT_QOS0 0               // QoS 0
#define ESP01_MQTT_QOS1 1               // QoS 1
#define ESP01_MQTT_QOS2 2               // QoS 2
#define ESP01_MQTT_DEFAULT_PORT 1883    // Port MQTT par défaut

/* =========================== TYPES & STRUCTURES ======================= */
/**
 * @brief Structure représentant un client MQTT.
 */
typedef struct
{
    bool connected;                                   // Statut de connexion au broker
    char broker_ip[ESP01_MAX_IP_LEN];                 // Adresse IP du broker
    uint16_t broker_port;                             // Port du broker
    char client_id[ESP01_MQTT_MAX_CLIENT_ID_LEN + 1]; // Identifiant client MQTT
    uint16_t keep_alive;                              // Intervalle keep-alive (s)
    uint16_t packet_id;                               // Dernier packet ID utilisé
} mqtt_client_t;

/**
 * @brief Prototype de callback pour la réception de messages MQTT.
 * @param topic   Sujet du message reçu.
 * @param message Contenu du message reçu.
 */
typedef void (*mqtt_message_callback_t)(const char *topic, const char *message);

/* ========================= VARIABLES GLOBALES ========================= */
extern mqtt_client_t g_mqtt_client; // Instance globale du client MQTT

/**
 * @defgroup ESP01_MQTT_AT_WRAPPERS Wrappers AT et helpers associés (par commande AT)
 * @brief  Fonctions exposant chaque commande AT MQTT à l'utilisateur, avec leurs helpers de parsing/affichage.
 *
 * | Commande AT         | Wrapper principal(s)             | Helpers associés                | Description courte                  |
 * |---------------------|----------------------------------|---------------------------------|-------------------------------------|
 * | AT+MQTTCONN         | esp01_mqtt_connect               | INUTILE                         | Connexion au broker MQTT            |
 * | AT+MQTTPUB          | esp01_mqtt_publish               | INUTILE                         | Publication d'un message            |
 * | AT+MQTTSUB          | esp01_mqtt_subscribe             | INUTILE                         | Souscription à un topic             |
 * | AT+MQTTPING         | esp01_mqtt_ping                  | INUTILE                         | Ping MQTT                           |
 * | AT+MQTTDISC         | esp01_mqtt_disconnect            | INUTILE                         | Déconnexion du broker               |
 */

/* ========================= FONCTIONS PRINCIPALES (API MQTT) ========================= */
/**
 * @brief  Connexion au broker MQTT (AT+MQTTCONN)
 * @param  broker_ip  Adresse IP du broker.
 * @param  port       Port du broker.
 * @param  client_id  Identifiant client.
 * @param  username   Nom d'utilisateur (optionnel, peut être NULL).
 * @param  password   Mot de passe (optionnel, peut être NULL).
 * @return ESP01_Status_t Code de retour (ESP01_OK si succès).
 */
ESP01_Status_t esp01_mqtt_connect(const char *broker_ip, uint16_t port,
                                  const char *client_id, const char *username,
                                  const char *password);

/**
 * @brief  Publication d'un message sur un topic (AT+MQTTPUB)
 * @param  topic   Sujet (topic) du message.
 * @param  message Message à publier.
 * @param  qos     Qualité de service (0, 1 ou 2).
 * @param  retain  true pour conserver le message sur le broker, false sinon.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_mqtt_publish(const char *topic, const char *message,
                                  uint8_t qos, bool retain);

/**
 * @brief  Souscription à un topic (AT+MQTTSUB)
 * @param  topic Sujet (topic) à souscrire.
 * @param  qos   Qualité de service souhaitée.
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_mqtt_subscribe(const char *topic, uint8_t qos);

/**
 * @brief  Envoie un ping MQTT (PINGREQ) (AT+MQTTPING)
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_mqtt_ping(void);

/**
 * @brief  Déconnexion du broker MQTT (AT+MQTTDISC)
 * @return ESP01_Status_t Code de retour.
 */
ESP01_Status_t esp01_mqtt_disconnect(void);

/**
 * @brief  Définit le callback utilisateur pour la réception de messages MQTT.
 * @param  cb Fonction callback à appeler lors de la réception d'un message.
 */
void esp01_mqtt_set_message_callback(mqtt_message_callback_t cb);

/**
 * @brief  Fonction de polling MQTT à appeler régulièrement pour traiter les messages entrants.
 */
void esp01_mqtt_poll(void);

#endif /* STM32_WIFIESP_MQTT_H_ */
