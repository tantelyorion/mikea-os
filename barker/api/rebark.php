<?php
// ============================================================
// BARKER API — Rebark (avec type)
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

if ($_SERVER['REQUEST_METHOD'] !== 'POST') json_response(['error'=>'POST only'],405);
if (!verify_csrf($_POST['csrf_token'] ?? '')) json_response(['error'=>'Invalid CSRF'],403);

$barkId = (int)($_POST['bark_id'] ?? 0);
$isAnon = (int)!empty($_POST['is_anonymous']);
$userId = $_SESSION['user_id'];

$originalBark = DB::fetchOne("SELECT user_id, content FROM barks WHERE id=? AND is_deleted=0", [$barkId]);
if (!$originalBark) json_response(['error'=>'Bark not found'],404);

// Anti-self
if ($originalBark['user_id'] === $userId) {
    json_response(['error'=>'You cannot rebark your own bark'],403);
}

// Vérifier si déjà rebarké
$existing = DB::fetchOne("SELECT id FROM rebarks WHERE user_id=? AND bark_id=?", [$userId, $barkId]);

if ($existing) {
    // Supprimer le rebark
    DB::execute("DELETE FROM rebarks WHERE user_id=? AND bark_id=?", [$userId, $barkId]);
    // Supprimer le bark de type rebark associé
    DB::execute("DELETE FROM barks WHERE user_id=? AND bark_type='rebark' AND original_bark_id=? AND is_deleted=0", [$userId, $barkId]);
    json_response(['success'=>true,'rebarked'=>false]);
} else {
    // Ajouter le rebark
    DB::execute("INSERT INTO rebarks (user_id, bark_id, is_anonymous) VALUES (?,?,?)", [$userId, $barkId, $isAnon]);
    
    // Créer un bark de type rebark (pour l'affichage dans le feed)
    $rebarkContent = ''; // Pas de contenu supplémentaire pour un rebark simple
    DB::insert(
        "INSERT INTO barks (user_id, content, bark_type, original_bark_id, is_anonymous) VALUES (?, ?, 'rebark', ?, ?)",
        [$userId, $rebarkContent, $barkId, $isAnon]
    );
    
    // Notification
    DB::execute(
        "INSERT INTO notifications (user_id, from_user_id, type, bark_id) VALUES (?,?,?,?)",
        [$originalBark['user_id'], $userId, 'rebark', $barkId]
    );
    json_response(['success'=>true,'rebarked'=>true]);
}