<?php
// ============================================================
// BARKER MAIL — Bootstrap
// ============================================================
require_once dirname(__DIR__) . '/config.php';

if (!function_exists('imap_open')) {
    http_response_code(500);
    die('Barker Mail a besoin de l\'extension PHP "imap", qui n\'est pas activée sur ce serveur. '
        . 'Dans DirectAdmin, active ou demande à ton hébergeur d’activer l’extension PHP "imap" pour la version PHP utilisée par Barker Mail, puis recharge cette page.');
}

session_name('barkermail_sess');
if (session_status() === PHP_SESSION_NONE) {
    ini_set('session.cookie_httponly', '1');
    session_set_cookie_params([
        'lifetime' => 0,
        'path'     => '/',
        'httponly' => true,
        'samesite' => 'Lax',
    ]);
    session_start();
}

define('MAIL_PAGE_SIZE', 25);

require_once __DIR__ . '/includes/functions.php';
