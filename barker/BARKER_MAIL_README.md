# Barker Mail — Guide d'installation (DirectAdmin)

Cette version est adaptée à un hébergement **DirectAdmin**. Ton panneau est accessible sur le port `2222` (`https://gasy74-amis.com:2222/evo/`), donc l'ancienne intégration cPanel UAPI a été remplacée par l'API DirectAdmin `CMD_API_POP`.

## 1. Base de données

Dans phpMyAdmin, sur la base `gasyamis_barker`, exécute :

```sql
sql/migration_barker_mail.sql
```

Cela ajoute `mail_username` et `has_barker_mail` à `users`.

## 2. Configuration DirectAdmin

Dans `config.php`, remplis :

```php
define('DIRECTADMIN_HOST', 'https://gasy74-amis.com:2222');
define('DIRECTADMIN_USERNAME', 'VOTRE_UTILISATEUR_DIRECTADMIN');
define('DIRECTADMIN_LOGIN_KEY', 'COLLEZ_VOTRE_LOGIN_KEY_DIRECTADMIN_ICI');
define('DIRECTADMIN_VERIFY_SSL', true);
```

`DIRECTADMIN_USERNAME` est ton **nom d'utilisateur DirectAdmin**.

`DIRECTADMIN_LOGIN_KEY` est une clé API dédiée, à créer dans DirectAdmin → **Login Keys**. DirectAdmin recommande les Login Keys pour les appels API, et elles peuvent être limitées à certaines commandes et adresses IP. citeturn1search0turn1search4

Pour Barker Mail, la clé doit au minimum pouvoir exécuter :

```text
CMD_API_POP
```

DirectAdmin documente `CMD_API_POP` pour créer, lister, modifier et supprimer les comptes email virtuels. citeturn0search1turn0search3

### Créer le Login Key

1. Connecte-toi à DirectAdmin.
2. Va dans **User Level → Login Keys**.
3. Crée une clé, par exemple `barker-mail-api`.
4. Donne-lui une longue valeur secrète générée aléatoirement.
5. Limite les commandes à `CMD_API_POP`.
6. Si possible, limite aussi la clé à l'adresse IP publique du serveur qui héberge BARKER.
7. Copie la valeur de la clé et mets-la dans `DIRECTADMIN_LOGIN_KEY`.

La clé est utilisée comme mot de passe HTTP Basic Auth avec le nom d'utilisateur DirectAdmin normal. citeturn1search3turn1search5

## 3. Domaine et serveur mail

`barker.ink` doit être un domaine géré par ce compte DirectAdmin et son service mail doit fonctionner.

Les boîtes créées seront de la forme :

```text
pseudo@barker.ink
```

Barker Mail utilise ensuite :

- IMAP : `mail.barker.ink:993` en SSL
- SMTP : `mail.barker.ink:465` en SSL

Ces paramètres sont indépendants de l'API DirectAdmin : ils servent uniquement au webmail pour lire et envoyer les messages.

## 4. Sous-domaine mail.barker.ink

Dans DirectAdmin, vérifie que `mail.barker.ink` possède un certificat SSL valide et pointe vers le serveur mail / service correspondant à ta configuration.

Le dossier `mail/` de ce ZIP doit continuer à être servi par ton site à l'adresse :

```text
https://mail.barker.ink
```

Si `mail.barker.ink` est un sous-domaine séparé dont le Document Root est le dossier `mail/`, conserve cette configuration.

## 5. Extension PHP IMAP

Barker Mail nécessite l'extension PHP `imap`.

Sur DirectAdmin, l'activation dépend de la configuration PHP de l'hébergeur. Si tu n'as pas accès à la gestion des extensions, demande à l'hébergeur d'activer `imap` pour la version PHP utilisée par le dossier `mail/`.

Le webmail affichera une erreur explicite si `imap` n'est pas disponible.

## 6. Création automatique pendant l'inscription

Lorsqu'un utilisateur choisit Barker Mail pendant l'inscription :

1. BARKER vérifie le nom demandé.
2. BARKER vérifie sa disponibilité via `CMD_API_POP`.
3. BARKER crée le compte via DirectAdmin.
4. Seuls `mail_username` et `has_barker_mail` sont enregistrés dans BARKER.
5. Le mot de passe de la boîte mail n'est jamais enregistré en base.

Si DirectAdmin refuse la création, l'inscription est annulée afin d'éviter de créer un compte BARKER dont l'adresse mail ne fonctionnerait pas.

## 7. Configuration des emails système

Dans `config.php`, les constantes :

```php
define('MAIL_HOST', 'mail.barker.ink');
define('MAIL_USER', 'noreply@barker.ink');
define('MAIL_PASS', 'MOT_DE_PASSE_NOREPLY'); // dans config.local.php
```

servent aux emails système de BARKER. Crée manuellement `noreply@barker.ink` dans DirectAdmin et renseigne son mot de passe dans `MAIL_PASS`.

## 8. Test avant production

Avant d'autoriser les utilisateurs à créer des boîtes :

1. Vérifie que `DIRECTADMIN_HOST` répond sur HTTPS/2222.
2. Vérifie que le Login Key fonctionne.
3. Vérifie que `barker.ink` est bien géré par le compte DirectAdmin utilisé.
4. Crée une petite boîte de test depuis l'inscription.
5. Connecte-toi sur `https://mail.barker.ink` avec l'adresse créée.
6. Teste réception, envoi, pièces jointes et suppression.

> **Sécurité :** ne mets jamais le mot de passe principal DirectAdmin dans le code si un Login Key peut être utilisé. Conserve également les secrets hors Git/public et limite la clé API à `CMD_API_POP` et à l'IP du serveur quand ton hébergeur le permet. citeturn1search0turn1search4
