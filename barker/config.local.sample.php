<?php
// ============================================================
// BARKER — Secrets locaux. Copier en config.local.php (non versionné).
// ============================================================
define('BARKER_DEBUG', false);

define('DB_HOST', 'localhost');
define('DB_NAME', 'votre_base_barker');
define('DB_USER', 'votre_utilisateur_db');
define('DB_PASS', 'MOT_DE_PASSE_DB');

define('DIRECTADMIN_USERNAME', 'votre_utilisateur_directadmin');
define('DIRECTADMIN_LOGIN_KEY', 'LOGIN_KEY_DIRECTADMIN');   // limité à CMD_API_POP

define('MAIL_USER', 'noreply@barker.ink');
define('MAIL_PASS', 'MOT_DE_PASSE_NOREPLY');

define('PAPAY_API_KEY', '');
define('PAPAY_API_SECRET', '');

// Derrière Cloudflare / un reverse proxy de confiance uniquement :
// define('TRUST_PROXY_HEADERS', true);
