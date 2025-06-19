# Standards à respecter dans mes fichiers

Voici le récapitulatif complet des standards à vérifier et appliquer ou respecter dans mes fichiers:

## Structure générale
1. **En-tête de fichier** avec auteur, version, date et description détaillée
2. **Organisation en sections** clairement délimitées par des commentaires
3. **Séparation API/implémentation** : interface publique dans .h, détails dans .c

## Conventions de nommage
1. **Préfixe esp01_** pour toutes les fonctions publiques
2. **Variables globales** avec préfixe `g_` (ex: `g_ntp_config`)
3. **Variables locales importantes** déclarées `static`
4. **Constantes** en MAJUSCULES avec underscores (ex: `ESP01_MAX_BUFFER_SIZE`)
5. **Paramètres** avec noms explicites (ex: `out_buf` plutôt que `buf`)
6. **Structures et enums** avec noms en minuscules et suffixe `_t`

## Documentation
1. **Format Doxygen** pour toutes les fonctions publiques
2. **Structure complète** : `@brief`, `@param`, `@retval`, `@details`, `@note`
3. **Commentaires explicatifs** pour les algorithmes complexes
4. **Sections de code** avec en-têtes pour regrouper les fonctionnalités

## Gestion d'erreurs
1. **Validation des paramètres** avec macro `VALIDATE_PARAM()`
2. **Retour d'erreur normalisé** via macro `ESP01_RETURN_ERROR()`
3. **Codes d'erreur spécifiques** définis dans l'enum `ESP01_Status_t`
4. **Logging multi-niveaux** avec `ESP01_LOG_XXX()` (DEBUG, INFO, WARN, ERROR)

## Sécurité mémoire
1. **Validation des tailles de buffer** avec `esp01_check_buffer_size()`
2. **Copie sécurisée de chaînes** avec `esp01_safe_strcpy()`
3. **Terminaison des chaînes** garantie par vérification explicite
4. **Évitement des débordements** par vérification des limites avant opération

## Style de code
1. **Accolades toujours présentes** même pour les blocs à instruction unique
2. **Indentation de 4 espaces** (pas de tabulations)
3. **Nombre magique évité** : utiliser des constantes nommées
4. **Commentaires de fin d'accolade** pour les blocs longs
5. **Limitation de la complexité** : fonctions de taille raisonnable

## Composants spécifiques
1. **Macros de validation** : `VALIDATE_PARAM()`, `VALIDATE_PARAM_VOID()`
2. **Macros de logging** : `ESP01_LOG_DEBUG()`, `ESP01_LOG_INFO()`, etc.
3. **Fonction de copie sécurisée** : `esp01_safe_strcpy()`
4. **Gestion d'erreur** : `ESP01_RETURN_ERROR()`
5. **Vérification buffer** : `esp01_check_buffer_size()`

## Organisation modulaire
1. **Structure hiérarchique** : driver bas niveau + modules fonctionnels
2. **Séparation des préoccupations** : communication, parsing, traitement
3. **Dépendances minimales** entre modules
4. **API bien définie** pour chaque module fonctionnel
5. **Routines utilitaires internes** préfixées avec underscore ou `static`

Ces standards garantissent une base de code robuste, maintenable et cohérente.
