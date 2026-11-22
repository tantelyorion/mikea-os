<?php
// ============================================================
// BARKER — API: Get Paw List (likes) pour un bark
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

header('Content-Type: application/json');

$barkId = (int)($_GET['bark_id'] ?? 0);
$limit = (int)($_GET['limit'] ?? 50);
$offset = (int)($_GET['offset'] ?? 0);

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

try {
    // Récupérer le nombre total de paws
    $total = DB::fetchOne(
        "SELECT COUNT(*) as total FROM paws WHERE bark_id = ?",
        [$barkId]
    );
    
    // Récupérer la liste des utilisateurs qui ont pawé
    $paws = DB::fetchAll("
        SELECT 
            u.id,
            u.username,
            u.display_name,
            u.avatar,
            u.gender,
            u.is_verified,
            p.created_at
        FROM paws p
        JOIN users u ON u.id = p.user_id
        WHERE p.bark_id = ? AND u.is_banned = 0
        ORDER BY p.created_at DESC
        LIMIT ? OFFSET ?
    ", [$barkId, $limit, $offset]);
    
    // Pour chaque utilisateur, ajouter l'URL de l'avatar
    foreach ($paws as &$user) {
        $user['avatar_url'] = get_user_avatar($user);
        // Vérifier si l'utilisateur actuel suit cette personne
        $user['is_following'] = (bool)DB::fetchOne(
            "SELECT id FROM follows WHERE follower_id = ? AND following_id = ?",
            [$_SESSION['user_id'], $user['id']]
        );
    }
    
    echo json_encode([
        'success' => true,
        'total' => (int)($total['total'] ?? 0),
        'paws' => $paws,
        'has_more' => count($paws) >= $limit
    ]);
    
} catch (PDOException $e) {
    error_log("Get paws error: " . $e->getMessage());
    echo json_encode(['error' => 'Database error occurred']);
}