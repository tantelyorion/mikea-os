<?php
// ============================================================
// BARKER — Get Likes (Paws) for a Bark
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

header('Content-Type: application/json');

$barkId = (int)($_GET['bark_id'] ?? 0);

if (!$barkId) {
    echo json_encode(['error' => 'Bark ID required']);
    exit;
}

// Vérifier que le bark existe
$bark = DB::fetchOne("SELECT id FROM barks WHERE id = ? AND is_deleted = 0", [$barkId]);
if (!$bark) {
    echo json_encode(['error' => 'Bark not found']);
    exit;
}

// Récupérer tous les utilisateurs qui ont liké ce bark
$likes = DB::fetchAll("
    SELECT 
        p.id as paw_id,
        p.created_at,
        u.id as user_id,
        u.username,
        u.display_name,
        u.avatar,
        u.gender,
        u.is_verified,
        u.is_banned
    FROM paws p
    JOIN users u ON u.id = p.user_id
    WHERE p.bark_id = ? AND u.is_banned = 0
    ORDER BY p.created_at DESC
", [$barkId]);

// Compter le nombre total de likes
$totalLikes = count($likes);

echo json_encode([
    'success' => true,
    'total' => $totalLikes,
    'likes' => $likes
]);