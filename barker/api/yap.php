<?php
// ============================================================
// BARKER API — Yap (Reply) - VERSION CORRIGÉE
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

header('Content-Type: application/json');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    json_response(['error' => 'POST only'], 405);
}

if (!verify_csrf($_POST['csrf_token'] ?? '')) {
    json_response(['error' => 'Invalid CSRF'], 403);
}

$barkId  = (int)($_POST['bark_id'] ?? 0);
$content = trim($_POST['content'] ?? '');
// Décoder les entités HTML pour stocker le texte brut
$content = trim($_POST['content'] ?? '');

$isAnon  = (int)!empty($_POST['is_anonymous']);
$userId  = $_SESSION['user_id'];

// Validation
if (!$barkId || empty($content)) {
    json_response(['error' => 'Missing data'], 400);
}

if (mb_strlen($content) > 300) {
    json_response(['error' => 'Content too long (max 300 characters)'], 400);
}

// Vérifier que le bark original existe
$originalBark = DB::fetchOne("SELECT user_id FROM barks WHERE id = ? AND is_deleted = 0", [$barkId]);
if (!$originalBark) {
    json_response(['error' => 'Bark not found'], 404);
}

try {
    // Insérer le yap
    $yapId = DB::insert(
        "INSERT INTO yaps (user_id, bark_id, content, is_anonymous) VALUES (?, ?, ?, ?)",
        [$userId, $barkId, $content, $isAnon]
    );

    // Notification pour l'auteur du bark (sauf si c'est son propre bark)
    if ($originalBark['user_id'] !== $userId) {
        DB::execute(
            "INSERT INTO notifications (user_id, from_user_id, type, bark_id) VALUES (?, ?, ?, ?)",
            [$originalBark['user_id'], $userId, 'yap', $barkId]
        );
    }

    // Log de sécurité
    log_security($userId, 'yap_posted', "yap_id=$yapId, bark_id=$barkId");

    json_response([
        'success' => true,
        'yap_id' => $yapId
    ]);

} catch (PDOException $e) {
    error_log("Yap post error: " . $e->getMessage());
    json_response(['error' => 'Database error occurred'], 500);
}