# BARKER — Version autonome (séparée de HAFATRA)

## Séparation
- Suppression de `includes/hafatra_bridge.php`, `pages/hafatra-sso.php` et de toute la config `HAFATRA_*` / secret interne.
- BARKER ne lit plus et n'écrit plus dans la base de HAFATRA. Plus de SSO, plus d'id partagé, plus de téléphone unique « inter-apps ».
- Le lien « Hafatra » du pied de page est un simple lien externe (https://hafatra.xyz).

## Messagerie restaurée (propre à BARKER)
- Nouveau module `includes/messaging.php` + `api/messages.php` réécrit.
- Tables propres : `dm_conversations`, `dm_messages` (`sql/migration_messaging_native.sql`).
- Texte, images, vidéos, documents, messages vocaux, réponses, modification, suppression, accusés de lecture, compteur de non-lus, limite anti-spam (30 msg/min).
- Interface existante conservée (`pages/messages.php`), sans modale « compléter le compte ».

## Bugs corrigés
1. **Admin** : tout compte vérifié (badge) était administrateur → colonne `users.is_admin` dédiée ; un admin ne peut plus être banni par erreur.
2. **Bark anonyme** : le bouton « Message » exposait l'identité réelle de l'auteur → masqué ; l'id de l'auteur n'est plus dans le HTML pour le modal Bone.
3. **Bones** : le destinataire était pris dans la requête du navigateur → déduit du bark côté serveur ; la réponse ne révèle plus le nom d'un auteur anonyme.
4. **Notifications** : « Tout marquer comme lu » appelait une action inexistante (`mark_all_read`) ; « Charger plus » aussi. Les notifications étaient marquées lues AVANT l'affichage (jamais de pastille non lue). Corrigé (+ vérification CSRF).
5. **Sécurité** : erreurs PHP affichées aux visiteurs (désactivé), journal d'erreurs dans le dossier web (déplacé dans `logs/` protégé), IP client falsifiable via X-Forwarded-For (ignoré sauf proxy de confiance), cookie de session non `secure` en HTTPS, régénération de session avant écriture, anti force-brute sur la connexion (10 échecs / 15 min), `.htaccess` racine.
6. **Secrets** : plus aucun mot de passe dans `config.php` → `config.local.php` (voir `config.local.sample.php`).
7. **Inscription** : téléphone désormais facultatif (envoyait « +261 » seul s'il était vide).

## Installation / mise à jour
1. Sauvegarder la base.
2. Exécuter `sql/migration_messaging_native.sql` dans phpMyAdmin
   (puis, si besoin : `UPDATE users SET is_admin = 0 WHERE username <> 'votre_pseudo';`).
3. Copier `config.local.sample.php` → `config.local.php` et le remplir.
4. **Changer immédiatement** les mots de passe qui figuraient dans l'ancien `config.php`
   (base de données, DirectAdmin, mail noreply, clé API) : ils étaient dans l'archive.
5. Supprimer l'ancien `error_log.txt` public et `pages/hafatra-sso.php` sur le serveur.
