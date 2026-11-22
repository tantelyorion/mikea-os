<?php
// ============================================================
// BARKER — Bootstrap / Init
// ============================================================

require_once __DIR__ . '/../config.php';
require_once __DIR__ . '/db.php';
require_once __DIR__ . '/auth.php';
require_once __DIR__ . '/languages.php';
require_once __DIR__ . '/media_security.php';
require_once __DIR__ . '/messaging.php';

session_start_safe();

// Appliquer la direction RTL si nécessaire
$textDirection = get_text_direction();
if ($textDirection === 'rtl') {
    $GLOBALS['text_direction'] = 'rtl';
}

// JSON response helper
function json_response(array $data, int $code = 200): never {
    http_response_code($code);
    header('Content-Type: application/json');
    echo json_encode($data);
    exit;
}

// Redirect helper
function redirect(string $url): never {
    header('Location: ' . $url);
    exit;
}

// Unread notifications count
function get_unread_notifications_count(): int {
    if (!is_logged_in()) return 0;
    $row = DB::fetchOne(
        "SELECT COUNT(*) as c FROM notifications WHERE user_id = ? AND is_read = 0",
        [$_SESSION['user_id']]
    );
    return (int)($row['c'] ?? 0);
}

// Unread messages count — messagerie interne de BARKER (includes/messaging.php)
function get_unread_messages_count(): int {
    if (!is_logged_in()) return 0;
    try {
        return dm_unread_count((int)$_SESSION['user_id']);
    } catch (Throwable $e) {
        // Tables pas encore migrées ou erreur SQL : ne jamais casser la page.
        error_log('get_unread_messages_count: ' . $e->getMessage());
        return 0;
    }
}

// Override de time_ago avec version traduite
// Cette fonction remplace celle dans auth.php
if (!function_exists('time_ago')) {
    function time_ago(string $datetime): string {
        $diff = time() - strtotime($datetime);
        if ($diff < 60)     return __('time_just_now');
        if ($diff < 3600)   return __('time_minute_ago', ['n' => floor($diff/60)]);
        if ($diff < 86400)  return __('time_hour_ago', ['n' => floor($diff/3600)]);
        if ($diff < 604800) return __('time_day_ago', ['n' => floor($diff/86400)]);
        return date('d M Y', strtotime($datetime));
    }
}  