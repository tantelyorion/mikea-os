<?php
require_once __DIR__ . '/config.php';
ml_require_login();

ml_toggle_theme(ml_current_email());

$fallback = 'index.php';
$ref = $_SERVER['HTTP_REFERER'] ?? '';
if ($ref !== '' && strpos($ref, '://') !== false) {
    $host = parse_url($ref, PHP_URL_HOST);
    if ($host !== null && $host === ($_SERVER['HTTP_HOST'] ?? '')) {
        $fallback = $ref;
    }
}

header('Location: ' . $fallback);
exit;
