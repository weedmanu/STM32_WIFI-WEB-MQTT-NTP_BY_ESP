# Convention de codage STM32_WifiESP

## 1. Structure des fichiers
- **En-tête Doxygen** obligatoire en début de chaque fichier (auteur, version, date, description).
- **Organisation en sections** : séparer clairement les parties (typedef, defines, variables, fonctions, etc.) par des commentaires.
- **API publique** dans le `.h`, **implémentation** et variables statiques dans le `.c`.

## 2. Nommage
- **Fonctions publiques** : préfixe `esp01_` + nom du module + action (ex : `esp01_http_post_json`).
- **Variables globales** : préfixe `g_` (ex : `g_ntp_config`).
- **Variables statiques locales** : déclarées `static`.
- **Constantes** : MAJUSCULES avec underscores (ex : `ESP01_MAX_BUFFER_SIZE`).
- **Structures** : nom en minuscules avec suffixe `_t` (ex : `esp01_config_t`).
- **Enums** : 
  - Codes d’état globaux : `ESP01_Status_t`
  - Enums internes : minuscules avec suffixe `_t`
- **Paramètres** : noms explicites (ex : `out_buf`).

## 3. Documentation
- **Format Doxygen** pour toutes les fonctions publiques.
- Toujours : `@brief`, `@param`, `@retval`, et commentaires pour chaque section/fonction/structure/enum/ligne complexe.

## 4. Gestion d’erreur et validation
- **Validation des paramètres** : toujours avec `VALIDATE_PARAM()` ou variantes.
- **Retour d’erreur** : toujours via `ESP01_RETURN_ERROR()`.
- **Codes d’erreur** : utiliser l’enum `ESP01_Status_t`.
- **Logging** : toujours tracer avec `ESP01_LOG_DEBUG()`, `ESP01_LOG_INFO()`, `ESP01_LOG_WARN()`, `ESP01_LOG_ERROR()`.

## 5. Sécurité mémoire
- **Vérification des tailles de buffer** : `esp01_check_buffer_size()`.
- **Copie de chaînes** : toujours `esp01_safe_strcpy()` ou `esp01_safe_strcat()`.
- **Jamais de buffer overflow** : toujours vérifier les limites avant toute opération mémoire.

## 6. Style de code
- **Accolades toujours présentes** même pour une seule instruction.
- **Indentation 4 espaces** (jamais de tabulations).
- **Nombres magiques interdits** : toujours utiliser une constante nommée.
- **Fonctions courtes et lisibles** : limiter la complexité cyclomatique.

## 7. Organisation modulaire
- **Modules indépendants** : chaque fonctionnalité dans son fichier.
- **Pas d’inclusion croisée inutile**.
- **API claire** pour chaque module.
- **Fonctions internes** : préfixe `_` ou `static`.

## 8. Outils du driver à utiliser
- **Validation** : `VALIDATE_PARAM()`, `VALIDATE_PARAM_VOID()`, etc.
- **Logging** : `ESP01_LOG_DEBUG()`, `ESP01_LOG_INFO()`, etc.
- **Gestion d’erreur** : `ESP01_RETURN_ERROR()`
- **Sécurité mémoire** : `esp01_safe_strcpy()`, `esp01_check_buffer_size()`
- **Helpers parsing** : `esp01_parse_string_after()`, etc.

## 9. Ajout de fonctions
- **Toujours respecter ces conventions** pour toute nouvelle fonction.
- **Toujours documenter** au format Doxygen.
- **Toujours valider les entrées** et vérifier la taille des buffers.
- **Toujours utiliser les macros de logging et de gestion d’erreur**.
- **Toujours utiliser les constantes nommées** (jamais de nombre magique).

---

**Rappel :  
Respecter ce standard garantit la robustesse, la sécurité, la lisibilité et la maintenabilité du code