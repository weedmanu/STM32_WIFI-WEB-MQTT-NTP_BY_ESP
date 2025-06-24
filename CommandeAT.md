# Documentation des commandes AT – Firmware Cyprus (ESP|| ATE1             |   0    |  0  |  0  |  1  | Active l'écho des commandes AT    |ATE0             |   0    |  0  |  0  |  1  | Désactive l'écho des commandes AT |1/ESP8266)

## À propos

Ce document décrit les commandes AT du firmware Cyprus pour modules ESP01/ESP8266, leur usage, les types de commandes (get, set, execute), et l’interprétation du retour de la commande `AT+CMD?`.

- **Firmware Cyprus** : [GitHub Cyprus ESP8266 AT](https://github.com/cypresssemiconductorco/esp-at)
- **Documentation officielle** : [AT Command Set](https://docs.espressif.com/projects/esp-at/en/latest/AT_Command_Set/)

---

## 1. Structure du retour `AT+CMD?`

La commande `AT+CMD?` retourne la liste des commandes AT supportées, sous la forme :

```
+CMD:<id>,"<nom>",<global>,<get>,<set>,<exe>
```

- **global** : 0 = utilisable partout (global), 1 = contexte limité
- **get** : 1 si la commande accepte un mode "get" (ex : `AT+CMD?`), 0 sinon
- **set** : 1 si la commande accepte un mode "set" (ex : `AT+CMD=...`), 0 sinon
- **exe** : 1 si la commande accepte un mode "execute" (ex : `AT+CMD`), 0 sinon

> **Attention :**
> Dans les tableaux ci-dessous, l’ordre des colonnes est :  
> **Global, Get, Set, Exe**  
> Cela correspond à la sortie réelle de `AT+CMD?` :
> - **Get** = 1 si la commande accepte le mode `?`
> - **Set** = 1 si la commande accepte le mode `=`
> - **Exe** = 1 si la commande accepte le mode exécution directe
> - La colonne "Exe" n’est pas toujours fiable, il faut tester la commande pour vérifier.

**Exemple :**
```
+CMD:4,"AT+GMR",0,0,0,1
```
- AT+GMR : utilisable partout (global=0), seule la forme exécution (`AT+GMR`) est valide (exe=1).

```
+CMD:22,"AT+CWJAP",0,1,1,1
```
- AT+CWJAP : utilisable partout (global=0), accepte les modes set (`AT+CWJAP=...`) et get (`AT+CWJAP?`).  
  **Attention :** même si la dernière colonne (exe=1) indique que la forme exécution directe (`AT+CWJAP`) serait possible, en pratique `AT+CWJAP` seul retourne `ERROR`.  
  **La colonne "Exe" n'est donc pas toujours fiable et doit être vérifiée par des tests réels.**

---

## 3. Commandes AT classées par responsabilité

### Commandes système de base

| Commande         | Global | Get | Set | Exe | Description                       |
|------------------|--------|-----|-----|-----|-----------------------------------|
| AT               |   0    |  0  |  0  |  1  | Tests la communication AT        |
| ATE0             |   0    |  0  |  0  |  1  | Désactive l’echo                  |
| ATE1             |   0    |  0  |  0  |  1  | Active l’echo                     |
| AT+RST           |   0    |  0  |  0  |  1  | Redémarre le module               |
| AT+GMR           |   0    |  0  |  0  |  1  | Interroge les informations de version |
| AT+CMD           |   0    |  1  |  0  |  0  | Interroge les commandes AT supportées |
| AT+GSLP          |   0    |  0  |  1  |  0  | Entre en mode deep sleep          |
| AT+SYSTIMESTAMP  |   0    |  1  |  1  |  0  | Interroge/configure l'horodatage système |
| AT+SLEEP         |   0    |  1  |  1  |  0  | Interroge/configure le mode de sommeil |
| AT+RESTORE       |   0    |  0  |  0  |  1  | Restaure les paramètres par défaut |
| AT+SYSRAM        |   0    |  1  |  0  |  0  | Interroge l'utilisation actuelle de la RAM |
| AT+SYSFLASH      |   0    |  1  |  1  |  0  | Interroge/configure les paramètres de partition flash utilisateur |
| AT+RFPOWER       |   0    |  1  |  1  |  0  | Interroge/configure la puissance RF maximum |
| AT+SYSMSG        |   0    |  1  |  1  |  0  | Interroge/configure l'affichage des messages système |
| AT+SYSROLLBACK   |   0    |  0  |  0  |  1  | Fait un rollback vers la version précédente du firmware |
| AT+SYSLOG        |   0    |  1  |  1  |  0  | Active/configure les journaux de débogage AT |
| AT+SYSSTORE      |   0    |  1  |  1  |  0  | Interroge/configure le mode de stockage des paramètres |
| AT+SLEEPWKCFG    |   0    |  0  |  1  |  0  | Configure les sources de réveil du sleep |
| AT+SYSREG        |   0    |  0  |  1  |  0  | Lit/écrit les registres du système |
| AT+USERRAM       |   0    |  1  |  1  |  0  | Opère sur la RAM utilisateur      |

### Commandes WiFi

| Commande        | Global | Get | Set | Exe | Description                       |
|-----------------|--------|-----|-----|-----|-----------------------------------|
| AT+CWMODE       |   0    |  1  |  1  |  0  | Interroge/configure le mode WiFi  |
| AT+CWSTATE      |   0    |  0  |  1  |  0  | État WiFi                         |
| AT+CWJAP        |   0    |  1  |  1  |  1  | Se connecte à un point d'accès    |
| AT+CWRECONNCFG  |   0    |  1  |  1  |  0  | Config reconnexion WiFi           |
| AT+CWLAP        |   0    |  0  |  1  |  1  | Liste les points d'accès disponibles |
| AT+CWLAPOPT     |   0    |  0  |  1  |  0  | Options scan WiFi                 |
| AT+CWQAP        |   0    |  0  |  0  |  1  | Se déconnecte du point d'accès    |
| AT+CWSAP        |   0    |  1  |  1  |  0  | Interroge/configure le point d'accès |
| AT+CWLIF        |   0    |  0  |  0  |  1  | Liste IP clients AP               |
| AT+CWQIF        |   0    |  1  |  0  |  1  | Déconnexion client WiFi           |
| AT+CWDHCP       |   0    |  1  |  1  |  0  | DHCP WiFi                         |
| AT+CWDHCPS      |   0    |  1  |  1  |  0  | DHCP serveur WiFi                 |
| AT+CWSTAPROTO   |   0    |  1  |  1  |  0  | Protocole station WiFi            |
| AT+CWAPPROTO    |   0    |  1  |  1  |  0  | Protocole AP WiFi                 |
| AT+CWAUTOCONN   |   0    |  1  |  1  |  0  | Auto connexion WiFi               |
| AT+CWHOSTNAME   |   0    |  1  |  1  |  0  | Hostname WiFi                     |
| AT+CWCOUNTRY    |   0    |  1  |  1  |  0  | Pays WiFi                         |
| AT+CWSTARTSMART |   0    |  1  |  0  |  1  | SmartConfig WiFi                  |
| AT+CWSTOPSMART  |   0    |  0  |  0  |  1  | Arrêt SmartConfig                 |
| AT+WPS          |   0    |  1  |  0  |  0  | WPS WiFi                          |

### Commandes réseau (TCP/IP)

| Commande           | Global | Get | Set | Exe | Description                       |
|--------------------|--------|-----|-----|-----|-----------------------------------|
| AT+CIFSR           |   0    |  0  |  0  |  1  | Obtient l'adresse IP locale       |
| AT+CIPSTAMAC       |   0    |  1  |  1  |  0  | Interroge/configure l'adresse MAC de station |
| AT+CIPAPMAC        |   0    |  1  |  1  |  0  | MAC AP                            |
| AT+CIPSTA          |   0    |  1  |  1  |  0  | IP station                        |
| AT+CIPAP           |   0    |  1  |  1  |  0  | IP AP                             |
| AT+CIPV6           |   0    |  1  |  1  |  0  | IPv6                              |
| AT+CIPDNS          |   0    |  1  |  1  |  0  | DNS                               |
| AT+CIPDOMAIN       |   0    |  1  |  0  |  0  | Résolution DNS                    |
| AT+CIPSTATUS       |   0    |  0  |  0  |  1  | Statut IP                         |
| AT+CIPSTART        |   0    |  1  |  0  |  0  | Établit une connexion TCP/UDP/SSL |
| AT+CIPSTARTEX      |   0    |  1  |  0  |  0  | Démarrer une connexion TCP/UDP avancée |
| AT+CIPTCPOPT       |   0    |  1  |  1  |  0  | Options TCP                       |
| AT+CIPCLOSE        |   0    |  1  |  0  |  1  | Ferme la connexion TCP/UDP/SSL    |
| AT+CIPSEND         |   0    |  1  |  0  |  1  | Envoie des données                |
| AT+CIPSENDEX       |   0    |  1  |  0  |  0  | Envoyer des données avancé        |
| AT+CIPDINFO        |   0    |  1  |  1  |  0  | Infos données reçues              |
| AT+CIPMUX          |   0    |  1  |  1  |  0  | Mode multi-connexion              |
| AT+CIPRECVMODE     |   0    |  1  |  1  |  0  | Mode réception                    |
| AT+CIPRECVDATA     |   0    |  1  |  0  |  0  | Lire données reçues               |
| AT+CIPRECVLEN      |   0    |  0  |  1  |  0  | Taille données reçues             |
| AT+CIPSERVER       |   0    |  1  |  1  |  0  | Serveur TCP                       |
| AT+CIPSERVERMAXCONN|   0    |  1  |  1  |  0  | Max connexions serveur            |
| AT+CIPSSLCCONF     |   0    |  1  |  1  |  0  | Config SSL client                 |
| AT+CIPSSLCCN       |   0    |  1  |  1  |  0  | Certificat SSL client             |
| AT+CIPSSLCSNI      |   0    |  1  |  1  |  0  | SNI SSL client                    |
| AT+CIPSSLCALPN     |   0    |  1  |  1  |  0  | ALPN SSL client                   |
| AT+CIPSSLCPSK      |   0    |  1  |  1  |  0  | PSK SSL client                    |
| AT+CIPMODE         |   0    |  1  |  1  |  0  | Mode transmission                 |
| AT+CIPSTO          |   0    |  1  |  1  |  0  | Timeout transmission              |
| AT+SAVETRANSLINK   |   0    |  1  |  0  |  0  | Lien transparent sauvegardé       |
| AT+CIPSNTPCFG      |   0    |  1  |  1  |  0  | Config SNTP                       |
| AT+CIPSNTPTIME     |   0    |  0  |  1  |  0  | Heure SNTP                        |
| AT+CIPRECONNINTV   |   0    |  1  |  1  |  0  | Intervalle reconnexion            |
| AT+PING            |   0    |  1  |  0  |  0  | Ping réseau                       |

### Commandes MQTT

| Commande         | Global | Get | Set | Exe | Description                       |
|------------------|--------|-----|-----|-----|-----------------------------------|
| AT+MQTTUSERCFG   |   0    |  1  |  0  |  0  | Config MQTT utilisateur           |
| AT+MQTTCLIENTID  |   0    |  1  |  0  |  0  | ClientID MQTT                     |
| AT+MQTTUSERNAME  |   0    |  1  |  0  |  0  | Username MQTT                     |
| AT+MQTTPASSWORD  |   0    |  1  |  0  |  0  | Password MQTT                     |
| AT+MQTTCONNCFG   |   0    |  1  |  0  |  0  | Config connexion MQTT             |
| AT+MQTTCONN      |   0    |  1  |  1  |  0  | Connexion MQTT                    |
| AT+MQTTPUB       |   0    |  1  |  0  |  0  | Publie des messages MQTT          |
| AT+MQTTPUBRAW    |   0    |  1  |  0  |  0  | Publication brute MQTT            |
| AT+MQTTSUB       |   0    |  1  |  1  |  0  | Souscription MQTT                 |
| AT+MQTTUNSUB     |   0    |  1  |  0  |  0  | Désabonnement MQTT                |
| AT+MQTTCLEAN     |   0    |  1  |  0  |  0  | Nettoyage MQTT                    |

### Commandes diverses

| Commande      | Global | Get | Set | Exe | Description                       |
|---------------|--------|-----|-----|-----|-----------------------------------|
| AT+MDNS       |   0    |  1  |  0  |  0  | Configure le service mDNS         |
| AT+WPS        |   0    |  1  |  0  |  0  | Configure la fonction WPS        |
| AT+CWSTARTSMART|  0    |  1  |  0  |  1  | SmartConfig WiFi                  |
| AT+CWSTOPSMART|   0    |  0  |  0  |  1  | Arrêt SmartConfig                 |
| AT+FACTPLCP   |   0    |  1  |  0  |  0  | Factory PLC                       |
| AT+UART       |   0    |  1  |  1  |  0  | Interroge/configure les paramètres UART |
| AT+UART_CUR   |   0    |  1  |  1  |  0  | Interroge/configure la config UART courante |
| AT+UART_DEF   |   0    |  1  |  1  |  0  | Interroge/configure la config UART par défaut |

---