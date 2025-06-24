# Driver STM32 pour ESP01/ESP8266

## Introduction

Cette bibliothèque fournit un driver complet pour contrôler les modules WiFi ESP01/ESP8266 depuis un microcontrôleur STM32. Elle offre une interface simplifiée pour accéder aux fonctionnalités réseau avancées comme:
- Communication WiFi (connexion, AP, scan)
- Serveur web embarqué
- Client/serveur HTTP
- Client MQTT
- Synchronisation NTP

## Installation et configuration

[STM32Guide](http://www.nasfamilyone.synology.me/STM32Guide/)

### Prérequis matériels
- Carte STM32 (testé sur STM32L476RG)
- Module ESP01/ESP8266
- Connexions:
  - TX/RX entre STM32 et ESP
  - Alimentation 3.3V pour l'ESP

### Configuration logicielle
1. **Configuration du projet STM32CubeIDE**:
   - Dans le fichier .ioc:
     - Configurer USART1 pour l'ESP avec DMA circulaire activé sur RX
     - Configurer USART2 pour le printf/debug avec interruption activée
   - Générer le code pour écraser la config de la carte STM32L476RG

2. **Utilisation des exemples**:
   - Copier le contenu d'un fichier Test_XXXX.c dans votre main.c
   - Configurer les paramètres WiFi/MQTT dans le code
   - Compiler et téléverser sur votre carte

## Exemples d'utilisation

Le projet contient plusieurs exemples clé-en-main:
- **Test_Module_WIFI.c** - Test de connexion WiFi et commandes AT basiques
- **Test_Serveur_WEB.c** - Serveur web embarqué avec exemples de routes
- **Test_Send_MQTT.c** - Envoi de messages vers un broker MQTT
- **Test_Receive_MQTT.c** - Réception de messages depuis un broker MQTT
- **Test_NTP.c** - Synchronisation de l'heure via NTP
- **Test_Terminal AT.c** - Terminal pour tester les commandes AT directement

## Architecture

La bibliothèque est organisée de façon modulaire:

```
STM32_WifiESP.h/.c       → Driver de base et commandes AT
   ├── STM32_WifiESP_WIFI.h/.c   → Fonctions WiFi
   ├── STM32_WifiESP_HTTP.h/.c   → Serveur web et requêtes HTTP
   ├── STM32_WifiESP_MQTT.h/.c   → Client MQTT
   └── STM32_WifiESP_NTP.h/.c    → Synchronisation d'horloge
```

## Reste à faire
1. Vérifier ce qu'il reste à factoriser, finir de commenter.
2. Faire la liste des wrappers et helpers utiles manquants, ajouter les plus utiles.
3. Expliquer un template type pour en ajouter en respectant le std.
4. Finir le tutoriel sur le site web .

## Standards de code appliqués

Voici le récapitulatif complet des standards appliqués dans mes fichiers, il me reste quelques vérifications, modifications et ajouts à faire pour être 100% standardisé.


### Structure générale
1. **En-tête de fichier** avec auteur, version, date et description détaillée
2. **Organisation en sections** clairement délimitées par des commentaires
3. **Séparation API/implémentation** : interface publique dans .h, détails dans .c

### Conventions de nommage
1. **Préfixe esp01_** pour toutes les fonctions publiques
2. **Variables globales** avec préfixe `g_` (ex: `g_ntp_config`)
3. **Variables locales importantes** déclarées `static`
4. **Constantes** en MAJUSCULES avec underscores (ex: `ESP01_MAX_BUFFER_SIZE`)
5. **Paramètres** avec noms explicites (ex: `out_buf` plutôt que `buf`)
6. **Structures et enums** avec noms en minuscules et suffixe `_t`

### Documentation
1. **Format Doxygen** pour toutes les fonctions publiques
2. **Structure complète** : `@brief`, `@param`, `@retval`, `@details`, `@note`
3. **Commentaires explicatifs à chaque ligne et alignés** les includes, structure, fonction et accolade pour explication complète et lisible
4. **Sections de code** avec en-têtes pour regrouper les fonctionnalités

### Gestion d'erreurs
1. **Validation des paramètres** avec macro `VALIDATE_PARAM()`
2. **Retour d'erreur normalisé** via macro `ESP01_RETURN_ERROR()`
3. **Codes d'erreur spécifiques** définis dans l'enum `ESP01_Status_t`
4. **Logging multi-niveaux** avec `ESP01_LOG_XXX()` (DEBUG, INFO, WARN, ERROR)

### Sécurité mémoire
1. **Validation des tailles de buffer** avec `esp01_check_buffer_size()`
2. **Copie sécurisée de chaînes** avec `esp01_safe_strcpy()`
3. **Terminaison des chaînes** garantie par vérification explicite
4. **Évitement des débordements** par vérification des limites avant opération

### Style de code
1. **Accolades toujours présentes** même pour les blocs à instruction unique
2. **Indentation de 4 espaces** (pas de tabulations)
3. **Nombre magique évité** : utiliser des constantes nommées
5. **Limitation de la complexité** : fonctions de taille raisonnable

### Composants spécifiques
1. **Macros de validation** : `VALIDATE_PARAM()`, `VALIDATE_PARAM_VOID()`
2. **Macros de logging** : `ESP01_LOG_DEBUG()`, `ESP01_LOG_INFO()`, etc.
3. **Fonction de copie sécurisée** : `esp01_safe_strcpy()`
4. **Gestion d'erreur** : `ESP01_RETURN_ERROR()`
5. **Vérification buffer** : `esp01_check_buffer_size()`

### Organisation modulaire
1. **Structure hiérarchique** : driver bas niveau + modules fonctionnels
2. **Séparation des préoccupations** : communication, parsing, traitement
3. **Dépendances minimales** entre modules
4. **API bien définie** pour chaque module fonctionnel
5. **Routines utilitaires internes** préfixées avec underscore ou `static`

Ces standards garantissent une base de code robuste, maintenable et cohérente.
