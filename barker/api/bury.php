<?php
// ============================================================
// BARKER API — Bury (Bookmark)
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

header('Content-Type: application/json');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    json_response(['error' => 'POST only'], 405);
}

$barkId = (int)($_POST['bark_id'] ?? 0);
$userId = $_SESSION['user_id'];

if (!$barkId) {
    json_response(['error' => 'Bark ID required'], 400);
}

// Vérifier que le bark existe
$bark = DB::fetchOne("SELECT id FROM barks WHERE id = ? AND is_deleted = 0", [$barkId]);
if (!$bark) {
    json_response(['error' => 'Bark not found'], 404);
}

// Vérifier si déjà bury
$existing = DB::fetchOne(
    "SELECT id FROM buries WHERE user_id = ? AND bark_id = ?",
    [$userId, $barkId]
);

if ($existing) {
    // Supprimer le bury
    DB::execute("DELETE FROM buries WHERE user_id = ? AND bark_id = ?", [$userId, $barkId]);
    json_response([
        'success' => true,
        'buried' => false,
        'message' => 'Bury removed'
    ]);
} else {
    // Ajouter le bury
    DB::execute("INSERT INTO buries (user_id, bark_id) VALUES (?, ?)", [$userId, $barkId]);
    
    // Notification à l'auteur (optionnel)
    $barkAuthor = DB::fetchOne("SELECT user_id FROM barks WHERE id = ?", [$barkId]);
    if ($barkAuthor && $barkAuthor['user_id'] != $userId) {
        DB::execute(
            "INSERT INTO notifications (user_id, from_user_id, type, bark_id) VALUES (?, ?, 'bury', ?)",
            [$barkAuthor['user_id'], $userId, $barkId]
        );
    }
    
    json_response([
        'success' => true,
        'buried' => true,
        'message' => 'Bark buried'
    ]);
}