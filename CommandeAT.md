
# Commandes AT

## Menu
- [Commandes de base et système](#commandes-de-base-et-système)
- [Commandes Wi-Fi](#commandes-wi-fi)
- [Commandes IP et réseau](#commandes-ip-et-réseau)
- [Commandes MQTT](#commandes-mqtt)
- [Commandes diverses](#commandes-diverses)

## Commandes de base et système
| Commande | Description | Retour attendu |
|----------|-------------|----------------|
| AT | Commande de base pour vérifier la communication avec l'appareil. | OK |
| ATE0 | Désactive l'écho des commandes. | OK |
| ATE1 | Active l'écho des commandes. | OK |
| AT+RST | Redémarre le module. | OK ou message d'erreur |
| AT+GMR | Récupère la version du firmware. | Version du firmware |
| AT+CMD? | Liste toutes les commandes disponibles. | Liste des commandes |
| AT+GSLP | Met le module en mode veille profonde pour une durée spécifiée. | OK |
| AT+SYSTIMESTAMP | Récupère le timestamp système actuel. | Timestamp |
| AT+SLEEP | Configure le mode de sommeil du module. | OK |
| AT+RESTORE | Restaure les paramètres d'usine du module. | OK |
| AT+SYSRAM | Affiche l'utilisation actuelle de la RAM système. | Informations sur la RAM |
| AT+SYSFLASH | Affiche les informations sur la mémoire flash système. | Détails de la mémoire flash |
| AT+RFPOWER | Configure la puissance de transmission RF. | OK |
| AT+SYSMSG | Active ou désactive les messages système. | OK |
| AT+SYSROLLBACK | Reviens à la version précédente du firmware. | OK |
| AT+SYSLOG | Configure les paramètres de journalisation système. | OK |
| AT+SYSSTORE | Stocke les paramètres système actuels. | OK |
| AT+SLEEPWKCFG | Configure le réveil du mode sommeil. | OK |
| AT+SYSREG | Affiche les informations d'enregistrement système. | Détails d'enregistrement |
| AT+USERRAM | Affiche l'utilisation de la RAM utilisateur. | Informations sur la RAM |
| AT+UART | Configure les paramètres UART. | OK |
| AT+UART_CUR | Configure les paramètres UART actuels. | OK |
| AT+UART_DEF | Configure les paramètres UART par défaut. | OK |

## Commandes Wi-Fi
| Commande | Description | Retour attendu |
|----------|-------------|----------------|
| AT+CWMODE | Configure le mode Wi-Fi (client, AP, ou les deux). | OK |
| AT+CWSTATE | Affiche l'état de la connexion Wi-Fi. | État de la connexion |
| AT+CWJAP | Connecte le module à un point d'accès Wi-Fi. | OK ou FAIL |
| AT+CWRECONNCFG | Configure les paramètres de reconnexion Wi-Fi automatique. | OK |
| AT+CWLAP | Liste les points d'accès Wi-Fi disponibles. | Liste des AP |
| AT+CWLAPOPT | Configure les options de la commande AT+CWLAP. | OK |
| AT+CWQAP | Déconnecte le module du point d'accès Wi-Fi. | OK |
| AT+CWSAP | Configure le module en tant que point d'accès Wi-Fi. | OK |
| AT+CWLIF | Liste les stations connectées au point d'accès du module. | Liste des stations |
| AT+CWQIF | Déconnecte une station du point d'accès du module. | OK |
| AT+CWDHCP | Active ou désactive le serveur DHCP. | OK |
| AT+CWDHCPS | Configure les paramètres du serveur DHCP. | OK |
| AT+CWSTAPROTO | Configure le protocole de communication pour le point d'accès. | OK |
| AT+CWAPPROTO | Configure le protocole de communication pour la station. | OK |
| AT+CWAUTOCONN | Active ou désactive la reconnexion automatique au Wi-Fi. | OK |
| AT+CWHOSTNAME | Configure le nom d'hôte du module. | OK |
| AT+CWCOUNTRY | Configure le code pays pour le Wi-Fi. | OK |
| AT+WPS | Active le mode WPS pour la configuration Wi-Fi. | OK |
| AT+CWSTARTSMART | Démarre le mode de configuration intelligente Wi-Fi. | OK |
| AT+CWSTOPSMART | Arrête le mode de configuration intelligente Wi-Fi. | OK |

## Commandes IP et réseau
| Commande | Description | Retour attendu |
|----------|-------------|----------------|
| AT+CIFSR | Récupère l'adresse IP locale. | Adresse IP |
| AT+CIPSTAMAC | Configure l'adresse MAC de la station. | OK |
| AT+CIPAPMAC | Configure l'adresse MAC du point d'accès. | OK |
| AT+CIPSTA | Configure l'adresse IP de la station. | OK |
| AT+CIPAP | Configure l'adresse IP du point d'accès. | OK |
| AT+CIPV6 | Active ou désactive le support IPv6. | OK |
| AT+CIPDNS | Configure les serveurs DNS. | OK |
| AT+CIPDOMAIN | Résout un nom de domaine en adresse IP. | Adresse IP |
| AT+CIPSTATUS | Récupère le statut de la connexion IP. | Statut de la connexion |
| AT+CIPSTART | Établit une connexion TCP ou UDP. | OK ou FAIL |
| AT+CIPSTARTEX | Établit une connexion TCP ou UDP avec des options supplémentaires. | OK |
| AT+CIPTCPOPT | Configure les options TCP. | OK |
| AT+CIPCLOSE | Ferme une connexion TCP ou UDP. | OK |
| AT+CIPSEND | Envoie des données via une connexion TCP ou UDP. | OK ou FAIL |
| AT+CIPSENDEX | Envoie des données via une connexion TCP ou UDP avec des options supplémentaires. | OK |
| AT+CIPDINFO | Affiche les informations de réception de données. | Informations de réception |
| AT+CIPMUX | Active ou désactive les connexions multiples. | OK |
| AT+CIPRECVMODE | Configure le mode de réception des données. | OK |
| AT+CIPRECVDATA | Reçoit des données via une connexion TCP ou UDP. | Données reçues |
| AT+CIPRECVLEN | Récupère la longueur des données reçues. | Longueur des données |
| AT+CIPSERVER | Configure le module en tant que serveur. | OK |
| AT+CIPSERVERMAXCONN | Configure le nombre maximum de connexions pour le serveur. | OK |
| AT+CIPSSLCCONF | Configure les paramètres SSL du client. | OK |
| AT+CIPSSLCCN | Configure le nom commun du certificat client SSL. | OK |
| AT+CIPSSLCSNI | Configure l'indication du nom de serveur SSL. | OK |
| AT+CIPSSLCALPN | Configure le protocole ALPN SSL. | OK |
| AT+CIPSSLCPSK | Configure les clés pré-partagées SSL. | OK |
| AT+CIPMODE | Configure le mode de transmission des données. | OK |
| AT+CIPSTO | Configure le timeout de la connexion TCP. | OK |
| AT+SAVETRANSLINK | Sauvegarde la connexion de transmission. | OK |
| AT+CIPSNTPCFG | Configure les paramètres du client SNTP. | OK |
| AT+CIPSNTPTIME | Récupère l'heure SNTP. | Heure SNTP |
| AT+CIPRECONNINTV | Configure l'intervalle de reconnexion. | OK |
| AT+MDNS | Configure le service mDNS. | OK |
| AT+PING | Envoie une requête ping à une adresse IP. | Résultat du ping |

## Commandes MQTT
| Commande | Description | Retour attendu |
|----------|-------------|----------------|
| AT+MQTTUSERCFG | Configure les paramètres de l'utilisateur MQTT. | OK |
| AT+MQTTCLIENTID | Configure l'identifiant du client MQTT. | OK |
| AT+MQTTUSERNAME | Configure le nom d'utilisateur MQTT. | OK |
| AT+MQTTPASSWORD | Configure le mot de passe MQTT. | OK |
| AT+MQTTCONNCFG | Configure les paramètres de connexion MQTT. | OK |
| AT+MQTTCONN | Connecte le client MQTT au broker. | OK ou FAIL |
| AT+MQTTPUB | Publie un message MQTT. | OK |
| AT+MQTTPUBRAW | Publie un message MQTT brut. | OK |
| AT+MQTTSUB | Souscrit à un sujet MQTT. | OK |
| AT+MQTTUNSUB | Se désabonne d'un sujet MQTT. | OK |
| AT+MQTTCLEAN | Nettoie la session MQTT. | OK |

## Commandes diverses
| Commande | Description | Retour attendu |
|----------|-------------|----------------|
| AT+FACTPLCP | Restaure les paramètres d'usine pour PLCP. | OK |
