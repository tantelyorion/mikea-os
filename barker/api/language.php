<?php
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    json_response(['error' => 'POST only'], 405);
}

$lang = $_POST['language'] ?? '';

if (set_language($lang)) {
    json_response(['success' => true, 'language' => $lang]);
} else {
    json_response(['error' => 'Invalid language'], 400);
}