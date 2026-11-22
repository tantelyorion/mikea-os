<?php
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

if ($_SERVER['REQUEST_METHOD'] !== 'POST') json_response(['error'=>'POST only'],405);
if (!verify_csrf($_POST['csrf_token'] ?? '')) json_response(['error'=>'Invalid CSRF'],403);

$barkId  = (int)($_POST['bark_id'] ?? 0) ?: null;
$userId  = (int)($_POST['user_id'] ?? 0) ?: null;
$reason  = $_POST['reason'] ?? 'other';
$details = trim($_POST['details'] ?? '');

$validReasons = ['spam','harassment','hate','misinformation','other'];
if (!in_array($reason, $validReasons)) json_response(['error'=>'Invalid reason'],400);
if (!$barkId && !$userId) json_response(['error'=>'Nothing to report'],400);

$reporterId = $_SESSION['user_id'];

// Anti-spam: max 5 reports per user per hour
$recent = DB::fetchOne(
    "SELECT COUNT(*) as c FROM reports WHERE reporter_id=? AND created_at > DATE_SUB(NOW(), INTERVAL 1 HOUR)",
    [$reporterId]
);
if ($recent && $recent['c'] >= 5) {
    json_response(['error'=>'Too many reports. Please wait before submitting more.'],429);
}

DB::execute(
    "INSERT INTO reports (reporter_id, bark_id, user_id, reason, details) VALUES (?,?,?,?,?)",
    [$reporterId, $barkId, $userId, $reason, $details]
);

json_response(['success'=>true]);
