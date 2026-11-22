<?php
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

if ($_SERVER['REQUEST_METHOD'] !== 'POST') json_response(['error'=>'POST only'],405);
if (!verify_csrf($_POST['csrf_token'] ?? '')) json_response(['error'=>'Invalid CSRF'],403);

$targetId = (int)($_POST['user_id'] ?? 0);
$userId   = $_SESSION['user_id'];

if (!$targetId || $targetId === $userId) json_response(['error'=>'Invalid target'],400);

$target = DB::fetchOne("SELECT id FROM users WHERE id=? AND is_banned=0", [$targetId]);
if (!$target) json_response(['error'=>'User not found'],404);

$existing = DB::fetchOne(
    "SELECT id FROM follows WHERE follower_id=? AND following_id=?",
    [$userId, $targetId]
);

if ($existing) {
    DB::execute("DELETE FROM follows WHERE follower_id=? AND following_id=?", [$userId, $targetId]);
    json_response(['success'=>true,'following'=>false]);
} else {
    DB::execute("INSERT INTO follows (follower_id, following_id) VALUES (?,?)", [$userId, $targetId]);
    DB::execute(
        "INSERT INTO notifications (user_id, from_user_id, type) VALUES (?,?,'follow')",
        [$targetId, $userId]
    );
    json_response(['success'=>true,'following'=>true]);
}
