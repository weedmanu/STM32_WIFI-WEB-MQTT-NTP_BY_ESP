#ifndef INC_STM32_WIFIESP_HTTP_H_
#define INC_STM32_WIFIESP_HTTP_H_

#include "STM32_WifiESP.h"

// ==================== VARIABLES GLOBALES ====================

/**
 * @brief Structure d'une route HTTP (chemin + handler).
 * @see esp01_route_t dans STM32_WifiESP.h
 */
extern int g_route_count;                       ///< Nombre de routes HTTP enregistrées
extern esp01_route_t g_routes[];                ///< Tableau des routes HTTP
extern volatile bool g_processing_request;      ///< Indique si une requête HTTP est en cours de traitement

// ==================== FONCTIONS HTTP ====================

/**
 * @brief Parse une requête HTTP (GET/POST) et remplit la structure correspondante.
 * @param raw_request Chaîne contenant la requête HTTP brute.
 * @param parsed Structure de sortie à remplir.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed);

/**
 * @brief Envoie une réponse HTTP générique (code, type, body).
 * @param conn_id ID de la connexion.
 * @param status_code Code HTTP (ex: 200, 404).
 * @param content_type Type MIME (ex: "text/html").
 * @param body Corps de la réponse.
 * @param body_len Taille du corps.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len);

/**
 * @brief Envoie une réponse HTTP JSON (200 OK).
 * @param conn_id ID de la connexion.
 * @param json_data Chaîne JSON à envoyer.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);

/**
 * @brief Envoie une réponse HTTP 404 Not Found.
 * @param conn_id ID de la connexion.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_send_404_response(int conn_id);

/**
 * @brief Envoie une requête HTTP GET et récupère la réponse.
 * @param host Hôte distant (ex: "example.com").
 * @param port Port distant (ex: 80).
 * @param path Chemin HTTP (ex: "/api/data").
 * @param response Buffer pour la réponse.
 * @param response_size Taille du buffer réponse.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_http_get(const char *host, uint16_t port, const char *path, char *response, size_t response_size);

/**
 * @brief Efface toutes les routes HTTP enregistrées.
 */
void esp01_clear_routes(void);

/**
 * @brief Ajoute une route HTTP (chemin + handler).
 * @param path Chemin HTTP (ex: "/status").
 * @param handler Fonction handler associée.
 * @return ESP01_OK si succès, code d'erreur sinon.
 */
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);

/**
 * @brief Recherche le handler associé à un chemin donné.
 * @param path Chemin HTTP à rechercher.
 * @return Pointeur sur le handler, ou NULL si non trouvé.
 */
esp01_route_handler_t esp01_find_route_handler(const char *path);

/**
 * @brief Traite les requêtes HTTP reçues (dispatcher).
 */
void esp01_process_requests(void);

/**
 * @brief Ignore/consomme un payload HTTP restant (ex: body POST non traité).
 * @param expected_length Nombre d'octets à ignorer.
 */
void discard_http_payload(int expected_length);

/**
 * @brief Accumule les données reçues et recherche un motif de terminaison.
 * @param acc Buffer d'accumulation.
 * @param acc_len Pointeur sur la longueur actuelle du buffer.
 * @param acc_size Taille maximale du buffer.
 * @param terminator Motif de terminaison à rechercher.
 * @param timeout_ms Timeout en millisecondes.
 * @param append true pour ajouter à l'accumulateur, false pour écraser.
 * @return true si le motif est trouvé, false sinon.
 */
bool _accumulate_and_search(char *acc, uint16_t *acc_len, uint16_t acc_size, const char *terminator, uint32_t timeout_ms, bool append);

/**
 * @brief Initialise le module HTTP (reset routes et accumulateur).
 */
void esp01_http_init(void);

/**
 * @brief Boucle de traitement HTTP (à appeler régulièrement).
 */
void esp01_http_loop(void);

#endif /* INC_STM32_WIFIESP_HTTP_H_ */