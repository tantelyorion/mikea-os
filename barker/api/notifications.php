<?php
// ============================================================
// BARKER API — Notifications
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$action = $_GET['action'] ?? '';

if ($action === 'count') {
    json_response([
        'notifications' => get_unread_notifications_count(),
        'messages'      => get_unread_messages_count(),
    ]);
}

// 'mark_all_read' est le nom utilisé par la page notifications ('mark_read' conservé).
if (in_array($action, ['mark_read', 'mark_all_read'], true)) {
    if ($_SERVER['REQUEST_METHOD'] !== 'POST') json_response(['error' => 'POST requis'], 405);

    // Jeton CSRF : champ POST classique ou corps JSON.
    $token = $_POST['csrf_token'] ?? '';
    if ($token === '') {
        $body  = json_decode(file_get_contents('php://input'), true);
        $token = is_array($body) ? (string)($body['csrf_token'] ?? '') : '';
    }
    if (!verify_csrf($token)) json_response(['error' => 'Jeton de sécurité invalide'], 403);

    DB::execute("UPDATE notifications SET is_read=1 WHERE user_id=?", [$_SESSION['user_id']]);
    json_response(['success' => true]);
}

json_response(['error' => 'Invalid action'], 400);
