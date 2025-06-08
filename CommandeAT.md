# Commandes AT utiles pour ESP01 (ESP8266)

Ce document regroupe les principales commandes AT utilisées dans le projet STM32 <-> ESP01, y compris celles nécessaires pour le WiFi, le serveur HTTP, MQTT, la gestion IP avancée et la synchronisation NTP.

---

## Version du firmware ESP01 utilisé

Ce projet utilise le firmware AT suivant :

- **AT version** : 2.2.0.0 (b097cdf - ESP8266 - Jun 17 2021 12:57:45)
- **SDK version** : v3.4-22-g967752e2
- **Bin version** : 2.2.0 (Cytron_ESP-01S)
- **Compile time** : Aug  4 2021 17:20:05

👉 [Lien vers le dépôt officiel Espressif AT Firmware (GitHub)](https://github.com/espressif/ESP8266_AT)

👉 [Lien vers le dépôt Cytron ESP-01S AT Firmware (GitHub)](https://github.com/CytronTechnologies/esp8266-at)

---

## Commandes réseau WiFi

| Commande AT                        | Description                                      | Retour attendu (exemple)                |
|-------------------------------------|--------------------------------------------------|-----------------------------------------|
| `AT`                               | Test de communication                            | `OK`                                    |
| `AT+GMR`                           | Version du firmware                              | `AT version:...` + `OK`                 |
| `AT+RST`                           | Redémarrage du module                            | `OK` puis `ready`                       |
| `AT+CWMODE?`                       | Lire le mode WiFi                                | `+CWMODE:<mode>` + `OK`                 |
| `AT+CWMODE=1`                      | Mode station (client WiFi)                       | `OK`                                    |
| `AT+CWLAP`                         | Liste des réseaux WiFi disponibles               | Liste des réseaux + `OK`                |
| `AT+CWJAP?`                        | Affiche le réseau WiFi connecté                  | `+CWJAP:"SSID"` + `OK` ou `No AP`       |
| `AT+CWJAP="SSID","PWD"`            | Connexion à un réseau WiFi                       | `WIFI CONNECTED` + `WIFI GOT IP` + `OK` |
| `AT+CWQAP`                         | Déconnexion du WiFi                              | `OK`                                    |
| `AT+CIFSR`                         | Affiche l’adresse IP actuelle                    | `+CIFSR:STAIP,"192.168.x.x"` + `OK`     |
| `AT+CIPSTA?`                       | Affiche l’IP en mode STA                         | `+CIPSTA:ip:"192.168.x.x"` + `OK`       |
| `AT+CWDHCP=1,1`                    | Active le DHCP client (STA)                      | `OK`                                    |
| `AT+CWDHCP=0,1`                    | Désactive le DHCP client (STA)                   | `OK`                                    |
| `AT+CWDHCP=2,1`                    | Active le DHCP serveur (AP)                      | `OK`                                    |
| `AT+CWDHCP=0,2`                    | Désactive le DHCP serveur (AP)                   | `OK`                                    |
| `AT+CIPSTATUS`                     | Statut des connexions réseau                     | `STATUS:<n>` + liste + `OK`             |

---

## Commandes IP avancées

| Commande AT                                 | Description                                         | Retour attendu (exemple)                |
|----------------------------------------------|-----------------------------------------------------|-----------------------------------------|
| `AT+CIPSTA="IP","GATEWAY","NETMASK"`        | Définit l’IP statique, la passerelle et le netmask  | `OK`                                    |
| `AT+CIPAP="IP"`                             | Définit l’IP du point d’accès (mode AP)             | `OK`                                    |

---

## Commandes serveur TCP/HTTP

| Commande AT                        | Description                                      | Retour attendu (exemple)                |
|-------------------------------------|--------------------------------------------------|-----------------------------------------|
| `AT+CIPMUX=1`                      | Active le mode multi-connexion                   | `OK`                                    |
| `AT+CIPMUX=0`                      | Active le mode connexion unique (MQTT)           | `OK`                                    |
| `AT+CIPSERVER=1,80`                | Démarre le serveur TCP sur le port 80            | `OK` ou `no change`                     |
| `AT+CIPSERVER=0`                   | Arrête le serveur TCP                            | `OK`                                    |
| `AT+CIPDINFO=1`                    | Affiche l’IP du client dans les trames +IPD      | `OK`                                    |
| `AT+CIPSTO=60`                     | Timeout de connexion TCP (en secondes)           | `OK`                                    |
| `AT+CIPSTART="TCP","host",port`    | Ouvre une connexion TCP (client)                 | `OK` puis `CONNECT`                     |
| `AT+CIPSEND=<id>,<len>`            | Envoie des données sur la connexion `<id>`       | `>` puis `SEND OK`                      |
| `AT+CIPCLOSE=<id>`                 | Ferme la connexion TCP `<id>`                    | `CLOSED` + `OK`                         |
| `AT+CIPCLOSE`                      | Ferme la connexion TCP courante (MQTT, TCP)      | `CLOSED` + `OK`                         |

---

## Commandes NTP

| Commande AT                                 | Description                                         | Retour attendu (exemple)                |
|----------------------------------------------|-----------------------------------------------------|-----------------------------------------|
| `AT+CIPSNTPCFG=1,<timezone>,"ntp_server"`   | Configure le client NTP (active, fuseau, serveur)   | `OK`                                    |
| `AT+CIPSNTPTIME?`                           | Récupère la date/heure NTP                          | `+CIPSNTPTIME: ...` + `OK`              |

---
