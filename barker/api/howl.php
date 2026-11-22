<?php
// ============================================================
// BARKER API — Howl (Quote) - AVEC SUPPORT POLL ET MÉDIAS
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

// Vérifier que le bark original existe et récupérer toutes ses informations
$originalBark = DB::fetchOne("
    SELECT b.*, u.username, u.display_name, u.avatar, u.gender, u.is_verified,
           u.id as original_user_id
    FROM barks b
    JOIN users u ON u.id = b.user_id
    WHERE b.id = ? AND b.is_deleted = 0 AND u.is_banned = 0
", [$barkId]);

if (!$originalBark) {
    json_response(['error' => 'Bark not found'], 404);
}

// Récupérer les médias du bark original
$originalMedia = DB::fetchAll("SELECT * FROM media WHERE bark_id = ?", [$barkId]);

// Récupérer les données du poll si présent
$originalPollData = null;
if ($originalBark['poll_id']) {
    $poll = DB::fetchOne("
        SELECT p.*
        FROM polls p
        WHERE p.id = ?
    ", [$originalBark['poll_id']]);
    
    if ($poll) {
        $pollOptions = DB::fetchAll("
            SELECT * FROM poll_options 
            WHERE poll_id = ? 
            ORDER BY option_order ASC
        ", [$poll['id']]);
        
        $originalPollData = [
            'poll' => $poll,
            'options' => $pollOptions,
            'total_votes' => (int)($poll['total_votes'] ?? 0)
        ];
    }
}

try {
    DB::get()->beginTransaction();
    
    // Insérer dans la table howls
    $howlId = DB::insert(
        "INSERT INTO howls (user_id, bark_id, content, is_anonymous) VALUES (?, ?, ?, ?)",
        [$userId, $barkId, $content, $isAnon]
    );

    // Créer un bark de type howl (apparaîtra dans le feed)
    // Inclure le poll_id original pour que le poll soit visible dans le howl
    $newBarkId = DB::insert(
        "INSERT INTO barks (user_id, content, bark_type, original_bark_id, is_anonymous, poll_id) VALUES (?, ?, 'howl', ?, ?, ?)",
        [$userId, $content, $barkId, $isAnon, $originalBark['poll_id']]
    );

    // Extraire et sauvegarder les hashtags du nouveau bark
    preg_match_all('/#([\p{L}\p{N}_]+)/u', $content, $matches);
    $hashtags = array_unique($matches[1]);
    
    foreach ($hashtags as $tag) {
        $tagLower = mb_strtolower($tag, 'UTF-8');
        
        DB::execute(
            "INSERT INTO hashtags (name, bark_count) VALUES (?, 1) 
             ON DUPLICATE KEY UPDATE bark_count = bark_count + 1",
            [$tagLower]
        );
        
        $hashtagId = DB::fetchOne("SELECT id FROM hashtags WHERE name = ?", [$tagLower]);
        if ($hashtagId) {
            DB::execute(
                "INSERT IGNORE INTO bark_hashtags (bark_id, hashtag_id) VALUES (?, ?)",
                [$newBarkId, $hashtagId['id']]
            );
        }
    }

    // Notification pour l'auteur du bark original
    if ($originalBark['original_user_id'] !== $userId) {
        DB::execute(
            "INSERT INTO notifications (user_id, from_user_id, type, bark_id) VALUES (?, ?, ?, ?)",
            [$originalBark['original_user_id'], $userId, 'howl', $barkId]
        );
    }
    
    DB::get()->commit();

    // Log de sécurité
    log_security($userId, 'howl_posted', "howl_id=$howlId, bark_id=$newBarkId");

    json_response([
        'success' => true,
        'howl_id' => $howlId,
        'bark_id' => $newBarkId,
        'has_poll' => !is_null($originalPollData),
        'has_media' => !empty($originalMedia)
    ]);

} catch (PDOException $e) {
    DB::get()->rollBack();
    error_log("Howl post error: " . $e->getMessage());
    error_log('Howl error: ' . $e->getMessage());
    json_response(['error' => 'Database error occurred'], 500);
}