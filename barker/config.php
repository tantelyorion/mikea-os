
<?php
// ============================================================
// BARKER — Configuration
// ============================================================

// ============================================================
// Secrets : config.local.php (JAMAIS versionné / jamais dans une archive)
// Copiez config.local.sample.php en config.local.php et remplissez-le.
// ============================================================
if (is_file(__DIR__ . '/config.local.php')) {
    require_once __DIR__ . '/config.local.php';
}

// Erreurs : jamais affichées aux visiteurs en production. Pour déboguer,
// mettez define('BARKER_DEBUG', true) dans config.local.php.
if (!defined('BARKER_DEBUG')) define('BARKER_DEBUG', false);
error_reporting(E_ALL);
ini_set('display_errors', BARKER_DEBUG ? '1' : '0');
ini_set('log_errors', '1');
// Le journal est hors de public_html si possible ; à défaut il est protégé par .htaccess.
ini_set('error_log', defined('BARKER_ERROR_LOG') ? BARKER_ERROR_LOG : __DIR__ . '/logs/error_log.txt');

if (!defined('DB_NAME') && PHP_SAPI !== 'cli') {
    http_response_code(500);
    error_log('BARKER: config.local.php manquant (voir config.local.sample.php)');
    die('Configuration manquante.');
}
if (!defined('DB_HOST')) define('DB_HOST', 'localhost');
if (!defined('DB_NAME')) define('DB_NAME', '');
if (!defined('DB_USER')) define('DB_USER', '');
if (!defined('DB_PASS')) define('DB_PASS', '');
define('DB_CHARSET', 'utf8mb4');

// ============================================================
// URL officielle — barker.ink est le domaine officiel de BARKER.
// En local/dev (localhost, IP, etc.) on retombe sur la détection
// automatique du host pour ne pas casser les tests en local.
// ============================================================
$protocol   = isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] === 'on' ? 'https://' : 'http://';
$host       = $_SERVER['HTTP_HOST'] ?? 'localhost/barker';
$isLocalDev = (bool)preg_match('/^(localhost|127\.0\.0\.1|::1)/i', $host);

define('SITE_NAME', 'BARKER');
define('SITE_URL', $isLocalDev ? ($protocol . $host) : 'https://barker.ink');
define('SITE_TAGLINE', 'Express yourself. No leash required.');

// ============================================================
// BARKER MAIL — messagerie @barker.ink (webmail maison, pas Roundcube)
// ============================================================
define('MAIL_DOMAIN', 'barker.ink');                  // domaine des boîtes mail créées
define('MAIL_WEBMAIL_URL', 'https://mail.barker.ink'); // Barker Mail (notre webmail)

// Connexion IMAP/SMTP utilisée par Barker Mail pour lire/envoyer les mails
// des boîtes @barker.ink (identifiants = ceux de la boîte mail elle-même,
// saisis par l'utilisateur au login sur Barker Mail — jamais stockés en base).
define('BARKERMAIL_IMAP_HOST', 'mail.barker.ink');
define('BARKERMAIL_IMAP_PORT', 993);
define('BARKERMAIL_IMAP_ENCRYPTION', 'ssl'); // 'ssl' ou 'tls'
define('BARKERMAIL_SMTP_HOST', 'mail.barker.ink');
define('BARKERMAIL_SMTP_PORT', 465);
define('BARKERMAIL_SMTP_ENCRYPTION', 'ssl'); // 'ssl' (465) ou 'tls' (587)
// Si le certificat de mail.barker.ink est valide, garder false.
// Mettre true uniquement si le certificat IMAP n'est pas reconnu par PHP.
define('BARKERMAIL_IMAP_NOVALIDATE', true);

// ============================================================
// DirectAdmin — création automatique des comptes email @barker.ink
// depuis l'inscription BARKER.
//
// Ton panneau est accessible sur le port 2222 (DirectAdmin).
// Crée un Login Key dédié dans DirectAdmin → Login Keys et utilise
// la valeur du Login Key comme mot de passe API. Le Login Key peut
// être limité à CMD_API_POP et à l'IP du serveur qui héberge BARKER.
// ============================================================
define('DIRECTADMIN_HOST', 'https://gasy74-amis.com:2222');
if (!defined('DIRECTADMIN_USERNAME')) define('DIRECTADMIN_USERNAME', '');
if (!defined('DIRECTADMIN_LOGIN_KEY')) define('DIRECTADMIN_LOGIN_KEY', '');
define('DIRECTADMIN_VERIFY_SSL', true);

define('MAIL_DEFAULT_QUOTA_MB', 1024); // quota par défaut d'une boîte Barker Mail (Mo), 0 = illimité
define('MAIL_RESERVED_USERNAMES', [
    'postmaster', 'abuse', 'admin', 'administrator', 'webmaster', 'root',
    'noreply', 'no-reply', 'support', 'hostmaster', 'mailer-daemon', 'info',
    'contact', 'security', 'billing', 'privacy',
]);

// Navigation
define('PEOPLE_PER_PAGE', 20); 

define('UPLOAD_DIR', __DIR__ . '/assets/uploads/');
define('UPLOAD_URL', SITE_URL . '/assets/uploads/');
define('MAX_FILE_SIZE', 10 * 1024 * 1024); // 10MB
define('MAX_MEDIA_PER_BARK', 4);
define('BARK_MAX_CHARS', 300);

// Default avatars by gender
define('AVATAR_MALE',   SITE_URL . '/assets/default_avatars/male.png');
define('AVATAR_FEMALE', SITE_URL . '/assets/default_avatars/female.png');
define('AVATAR_OTHER',  SITE_URL . '/assets/default_avatars/other.png');

// PHPMailer config — emails système (mot de passe oublié, notifications...)
// Idéalement envoyés depuis une boîte Barker Mail (ex: noreply@barker.ink)
// créée manuellement dans DirectAdmin, avec son mot de passe ci-dessous.
define('MAIL_HOST', 'mail.barker.ink');
if (!defined('MAIL_USER')) define('MAIL_USER', 'noreply@barker.ink');
if (!defined('MAIL_PASS')) define('MAIL_PASS', '');
define('MAIL_PORT', 465);
define('MAIL_SMTP_SECURE', 'ssl');
define('MAIL_FROM_NAME', 'BARKER');

// Session & security
define('SESSION_LIFETIME', 86400 * 30); // 30 days
define('BCRYPT_COST', 12);

// Pagination
define('BARKS_PER_PAGE', 20);

// Donation limits
define('MIN_BONE_AMOUNT', 0.50);
define('MAX_BONE_AMOUNT', 500.00);

// ============================================================
// Configuration PAPAY
// ============================================================
define('PAPAY_API_URL', 'https://papay.info/api/index.php');
define('PAPAY_BASE_URL', 'https://papay.info');
if (!defined('PAPAY_API_KEY')) define('PAPAY_API_KEY', '');
if (!defined('PAPAY_API_SECRET')) define('PAPAY_API_SECRET', '');
