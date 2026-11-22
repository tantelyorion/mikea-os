<?php
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

if ($_SERVER['REQUEST_METHOD'] !== 'POST') json_response(['error'=>'POST only'],405);
if (!verify_csrf($_POST['csrf_token'] ?? '')) json_response(['error'=>'Invalid CSRF'],403);

$barkId = (int)($_POST['bark_id'] ?? 0);
$userId = $_SESSION['user_id'];

if (!$barkId) json_response(['error'=>'Invalid bark'],400);

// Anti-self-engagement: check if this bark belongs to current user
$bark = DB::fetchOne("SELECT user_id FROM barks WHERE id = ? AND is_deleted = 0", [$barkId]);
if (!$bark) json_response(['error'=>'Bark not found'],404);

if ($bark['user_id'] === $userId) {
    json_response(['error'=>'You cannot paw your own bark'],403);
}

// Toggle paw
$existing = DB::fetchOne("SELECT id FROM paws WHERE user_id=? AND bark_id=?", [$userId, $barkId]);

if ($existing) {
    DB::execute("DELETE FROM paws WHERE user_id=? AND bark_id=?", [$userId, $barkId]);
    json_response(['success'=>true, 'pawed'=>false]);
} else {
    DB::execute("INSERT INTO paws (user_id, bark_id) VALUES (?,?)", [$userId, $barkId]);

    // Notify owner
    if ($bark['user_id'] !== $userId) {
        DB::execute(
            "INSERT INTO notifications (user_id, from_user_id, type, bark_id) VALUES (?,?,?,?)",
            [$bark['user_id'], $userId, 'paw', $barkId]
        );
    }

    json_response(['success'=>true, 'pawed'=>true]);
}
