<?php
// ============================================================
// BARKER API — Bone (Donation)
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

if ($_SERVER['REQUEST_METHOD'] !== 'POST') json_response(['error'=>'POST only'],405);
if (!verify_csrf($_POST['csrf_token'] ?? '')) json_response(['error'=>'Invalid CSRF'],403);

$receiverId = (int)($_POST['receiver_id'] ?? 0);
$barkId     = (int)($_POST['bark_id'] ?? 0) ?: null;
$amount     = (float)($_POST['amount'] ?? 0);
$message    = trim($_POST['message'] ?? '');
$senderId   = (int)$_SESSION['user_id'];

// Si le Bone concerne un bark, le destinataire EST l'auteur de ce bark : on ne
// fait jamais confiance au receiver_id envoyé par le navigateur (sinon on
// pouvait rattacher un Bone à n'importe quel bark, et l'id d'un auteur anonyme
// devait être exposé dans la page).
$barkIsAnonymous = false;
if ($barkId) {
    $bark = DB::fetchOne("SELECT user_id, is_anonymous FROM barks WHERE id=?", [$barkId]);
    if (!$bark) json_response(['error'=>'Bark not found'],404);
    $receiverId      = (int)$bark['user_id'];
    $barkIsAnonymous = !empty($bark['is_anonymous']);
}

// Validations
if (!$receiverId) json_response(['error'=>'Invalid receiver'],400);
if ($receiverId === $senderId) json_response(['error'=>'You cannot send a Bone to yourself'],403);
if ($amount < MIN_BONE_AMOUNT) json_response(['error'=>'Minimum donation is €' . MIN_BONE_AMOUNT],400);
if ($amount > MAX_BONE_AMOUNT) json_response(['error'=>'Maximum donation is €' . MAX_BONE_AMOUNT],400);

$receiver = DB::fetchOne("SELECT id, display_name FROM users WHERE id=? AND is_banned=0", [$receiverId]);
if (!$receiver) json_response(['error'=>'Receiver not found'],404);

// NOTE: In production, integrate a real payment gateway (Stripe, PayPal, etc.)
// here before inserting the bone record. For now we simulate successful payment.

$boneId = DB::insert(
    "INSERT INTO bones (sender_id, receiver_id, bark_id, amount, message, status) VALUES (?,?,?,?,?,'completed')",
    [$senderId, $receiverId, $barkId, $amount, $message]
);

// Notify receiver
DB::execute(
    "INSERT INTO notifications (user_id, from_user_id, type, bark_id, message) VALUES (?,?,?,?,?)",
    [$receiverId, $senderId, 'bone', $barkId,
     "You received a Bone of €" . number_format($amount,2) . (!empty($message) ? " — \"$message\"" : '')]
);

log_security($senderId, 'bone_sent', "receiver=$receiverId amount=$amount");

json_response([
    'success' => true,
    'bone_id' => $boneId,
    'amount'  => $amount,
    'message' => $barkIsAnonymous ? 'Bone sent!' : "Bone sent to {$receiver['display_name']}!",
]);
