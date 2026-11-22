<?php
// ============================================================
// BARKER — Homepage (Feed) with FULL Poll Integration - VERSION CORRIGÉE AVEC ANTI-CACHE
// ============================================================
require_once __DIR__ . '/includes/init.php';
require_login();

// ============================================================
// ANTI-CACHE POUR TOUTE LA PAGE
// ============================================================
header('Cache-Control: no-cache, no-store, must-revalidate');
header('Pragma: no-cache');
header('Expires: 0');
header('Last-Modified: ' . gmdate('D, d M Y H:i:s') . ' GMT');

$pageTitle  = __('feed_home');
$currentUser = current_user();
$tab         = $_GET['tab'] ?? 'for_you';
$page        = max(1, (int)($_GET['page'] ?? 1));
$offset      = ($page - 1) * BARKS_PER_PAGE;

// Fonction pour récupérer le bark original avec ses infos
function getOriginalBark($barkId) {
    return DB::fetchOne("
        SELECT b.*, u.username, u.display_name, u.avatar, u.gender, u.is_verified
        FROM barks b
        JOIN users u ON u.id = b.user_id
        WHERE b.id = ? AND b.is_deleted = 0 AND u.is_banned = 0
    ", [$barkId]);
}

// Fonction pour récupérer les données du poll
function getPollData($pollId, $userId) {
    if (!$pollId) return null;
    
    $poll = DB::fetchOne("
        SELECT p.*
        FROM polls p
        WHERE p.id = ?
    ", [$pollId]);
    
    if (!$poll) return null;
    
    $pollOptions = DB::fetchAll("
        SELECT * FROM poll_options 
        WHERE poll_id = ? 
        ORDER BY option_order ASC
    ", [$pollId]);
    
    $userVoted = DB::fetchOne("
        SELECT option_id, rating_value FROM poll_votes 
        WHERE poll_id = ? AND user_id = ?
        LIMIT 1
    ", [$pollId, $userId]);
    
    $totalVotes = (int)$poll['total_votes'];
    foreach ($pollOptions as &$opt) {
        $opt['percentage'] = $totalVotes > 0 ? round(($opt['vote_count'] / $totalVotes) * 100) : 0;
    }
    unset($opt); // IMPORTANT: casser la référence, sinon $opt reste alias du dernier élément
    // et corrompt silencieusement les données lors d'un foreach($options as $opt) ultérieur
    
    return [
        'poll' => $poll,
        'options' => $pollOptions,
        'user_voted' => !is_null($userVoted),
        'user_choice' => $userVoted ? $userVoted['option_id'] : null,
        'user_rating' => $userVoted ? $userVoted['rating_value'] : null,
        'total_votes' => $totalVotes
    ];
}

/**
 * Fonction pour générer l'URL d'une image avec anti-cache basé sur la date de modification
 */
function getImageUrlWithCacheBust($imageUrl, $type = 'poll') {
    if (empty($imageUrl)) return null;
    
    $basePath = __DIR__ . '/assets/uploads/';
    if ($type === 'poll') {
        $filePath = $basePath . 'polls/' . $imageUrl;
    } else {
        $filePath = $basePath . 'media/' . $imageUrl;
    }
    
    if (file_exists($filePath)) {
        $timestamp = filemtime($filePath);
        return SITE_URL . '/assets/uploads/' . ($type === 'poll' ? 'polls/' : 'media/') . rawurlencode($imageUrl) . '?v=' . $timestamp;
    }
    
    return SITE_URL . '/assets/uploads/' . ($type === 'poll' ? 'polls/' : 'media/') . rawurlencode($imageUrl);
}

// Fetch barks avec gestion des rebarks et howls
if ($tab === 'following') {
    $barks = DB::fetchAll("
        SELECT b.*, u.username, u.display_name, u.avatar, u.gender, u.is_verified,
               b.bark_type, b.original_bark_id, b.poll_id,
               (SELECT COUNT(*) FROM paws WHERE bark_id = b.id) AS paw_count,
               (SELECT COUNT(*) FROM yaps WHERE bark_id = b.id) AS yap_count,
               (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id) AS rebark_count,
               (SELECT COUNT(*) FROM bones WHERE bark_id = b.id) AS bone_count,
               (SELECT COUNT(*) FROM paws WHERE bark_id = b.id AND user_id = ?) AS user_pawed,
               (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id AND user_id = ?) AS user_rebarked,
               (SELECT COUNT(*) FROM buries WHERE bark_id = b.id AND user_id = ?) AS user_buried
        FROM barks b
        JOIN users u ON u.id = b.user_id
        WHERE b.is_deleted = 0 AND u.is_banned = 0
        AND b.user_id IN (SELECT following_id FROM follows WHERE follower_id = ?)
        ORDER BY b.created_at DESC
        LIMIT ? OFFSET ?
    ", [
        $_SESSION['user_id'],
        $_SESSION['user_id'],
        $_SESSION['user_id'],
        $_SESSION['user_id'],
        BARKS_PER_PAGE,
        $offset
    ]);
} else {
    $barks = DB::fetchAll("
        SELECT b.*, u.username, u.display_name, u.avatar, u.gender, u.is_verified,
               b.bark_type, b.original_bark_id, b.poll_id,
               (SELECT COUNT(*) FROM paws WHERE bark_id = b.id) AS paw_count,
               (SELECT COUNT(*) FROM yaps WHERE bark_id = b.id) AS yap_count,
               (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id) AS rebark_count,
               (SELECT COUNT(*) FROM bones WHERE bark_id = b.id) AS bone_count,
               (SELECT COUNT(*) FROM paws WHERE bark_id = b.id AND user_id = ?) AS user_pawed,
               (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id AND user_id = ?) AS user_rebarked,
               (SELECT COUNT(*) FROM buries WHERE bark_id = b.id AND user_id = ?) AS user_buried
        FROM barks b
        JOIN users u ON u.id = b.user_id
        WHERE b.is_deleted = 0 AND u.is_banned = 0
        ORDER BY b.created_at DESC
        LIMIT ? OFFSET ?
    ", [
        $_SESSION['user_id'],
        $_SESSION['user_id'],
        $_SESSION['user_id'],
        BARKS_PER_PAGE,
        $offset
    ]);
}

// Récupérer les médias pour chaque bark
$barkIds = array_column($barks, 'id');
$mediaMap = [];
if ($barkIds) {
    $placeholders = implode(',', array_fill(0, count($barkIds), '?'));
    $mediaRows = DB::fetchAll(
        "SELECT * FROM media WHERE bark_id IN ($placeholders)",
        $barkIds
    );
    foreach ($mediaRows as $m) {
        $mediaMap[$m['bark_id']][] = $m;
    }
}

// Pour chaque bark, récupérer l'original si c'est un rebark ou howl
$originalBarks = [];
foreach ($barks as $bark) {
    if (in_array($bark['bark_type'], ['rebark', 'howl']) && $bark['original_bark_id']) {
        if (!isset($originalBarks[$bark['original_bark_id']])) {
            $original = getOriginalBark($bark['original_bark_id']);
            if ($original) {
                $originalBarks[$bark['original_bark_id']] = $original;
                $origMedia = DB::fetchAll("SELECT * FROM media WHERE bark_id = ?", [$bark['original_bark_id']]);
                $originalBarks[$bark['original_bark_id']]['media'] = $origMedia;
            }
        }
    }
}

// Récupérer les sondages pour les barks qui en ont
$pollIds = array_filter(array_column($barks, 'poll_id'));
$pollsData = [];

if (!empty($pollIds)) {
    $pollIds = array_values(array_unique(array_map('intval', $pollIds)));
    $pollIds = array_filter($pollIds);
    
    if (!empty($pollIds)) {
        $placeholders = implode(',', array_fill(0, count($pollIds), '?'));
        $polls = DB::fetchAll("
            SELECT p.*
            FROM polls p
            WHERE p.id IN ($placeholders)
        ", $pollIds);
        
        foreach ($polls as $poll) {
            $pollOptions = DB::fetchAll("
                SELECT * FROM poll_options 
                WHERE poll_id = ? 
                ORDER BY option_order ASC
            ", [$poll['id']]);
            
            $userVoted = DB::fetchOne("
                SELECT option_id, rating_value FROM poll_votes 
                WHERE poll_id = ? AND user_id = ?
                LIMIT 1
            ", [$poll['id'], $_SESSION['user_id']]);
            
            $totalVotes = (int)$poll['total_votes'];
            foreach ($pollOptions as &$opt) {
                $opt['percentage'] = $totalVotes > 0 ? round(($opt['vote_count'] / $totalVotes) * 100) : 0;
            }
            unset($opt); // IMPORTANT: sans ce unset, $opt reste une référence vers le dernier
            // élément de CE poll. Au prochain foreach($options as $opt) plus bas dans la page
            // (affichage des Duels), l'affectation "par valeur" écrit en fait À TRAVERS cette
            // référence et corrompt le dernier élément du poll précédent avec les données du
            // dernier Duel affiché — c'est exactement le bug "l'image d'un Duel devient normale
            // seulement quand un nouveau Duel est publié".
            
            $pollsData[$poll['id']] = [
                'poll' => $poll,
                'options' => $pollOptions,
                'user_voted' => !is_null($userVoted),
                'user_choice' => $userVoted ? $userVoted['option_id'] : null,
                'user_rating' => $userVoted ? $userVoted['rating_value'] : null,
                'total_votes' => $totalVotes
            ];
        }
    }
}

// Who to follow
$suggestions = DB::fetchAll("
    SELECT u.id, u.username, u.display_name, u.avatar, u.gender,
           (SELECT COUNT(*) FROM follows WHERE following_id = u.id) AS follower_count
    FROM users u
    WHERE u.id != ?
      AND u.is_banned = 0
      AND u.id NOT IN (SELECT following_id FROM follows WHERE follower_id = ?)
    ORDER BY follower_count DESC, RAND()
    LIMIT 5
", [$_SESSION['user_id'], $_SESSION['user_id']]);

include __DIR__ . '/includes/header.php';
?>

<style>
/* Poll Styles */
.poll-container {
    margin: 12px 0;
}
.poll-card {
    background: var(--bg-card2);
    border-radius: 16px;
    padding: 16px;
    border: 1px solid var(--border);
}
.poll-question {
    font-weight: 700;
    font-size: 1rem;
    margin-bottom: 16px;
    display: flex;
    align-items: center;
    gap: 8px;
    color: var(--text-primary);
}
.poll-expired-badge {
    background: var(--bg-card);
    padding: 4px 8px;
    border-radius: 20px;
    font-size: 0.7rem;
    color: var(--text-muted);
    display: inline-block;
    margin-bottom: 12px;
}
.poll-options {
    display: flex;
    flex-direction: column;
    gap: 10px;
}
.poll-option {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 10px;
    background: var(--bg-card);
    border-radius: 12px;
    cursor: pointer;
    transition: background 0.15s ease;
}
.poll-option:hover {
    background: var(--bg-card-hover);
}
.poll-option input {
    margin: 0;
}
.poll-option-text {
    flex: 1;
}
.poll-option-result {
    padding: 8px;
}
.poll-option-text {
    display: flex;
    justify-content: space-between;
    margin-bottom: 4px;
}
.poll-option-percent {
    font-weight: 700;
    color: var(--orange);
}
.poll-progress-bar {
    height: 8px;
    background: var(--bg-card);
    border-radius: 4px;
    overflow: hidden;
    margin: 6px 0;
}
.poll-progress-fill {
    height: 100%;
    background: var(--orange);
    border-radius: 4px;
    transition: width 0.3s ease;
}
.poll-option-votes {
    font-size: 0.7rem;
    color: var(--text-muted);
}
.poll-vote-btn {
    margin-top: 12px;
    width: 100%;
}
.poll-footer {
    margin-top: 12px;
    padding-top: 8px;
    border-top: 1px solid var(--border);
    font-size: 0.7rem;
    color: var(--text-muted);
    text-align: center;
}
.rating-results {
    display: flex;
    flex-direction: column;
    gap: 8px;
}
.rating-row {
    display: flex;
    align-items: center;
    gap: 12px;
}
.rating-stars-label {
    width: 80px;
    color: #FFD700;
    letter-spacing: 2px;
}
.rating-percent {
    width: 45px;
    text-align: right;
    font-size: 0.8rem;
    font-weight: 600;
}
.average-rating {
    margin-top: 12px;
    text-align: center;
    font-weight: 700;
    color: var(--orange);
}
.rating-vote {
    text-align: center;
}
.rating-stars-input {
    display: flex;
    justify-content: center;
    gap: 8px;
    margin-bottom: 16px;
}
.rating-star-select {
    font-size: 1.8rem;
    cursor: pointer;
    transition: transform 0.1s ease;
    color: var(--text-muted);
}
.rating-star-select:hover {
    transform: scale(1.1);
}
.rating-star-select.selected,
.rating-star-select.fa-solid {
    color: #FFD700;
}
.rating-media-container {
    margin-bottom: 16px;
    text-align: center;
    background: var(--bg-card);
    border-radius: 12px;
    padding: 8px;
}
.rating-media {
    cursor: pointer;
    text-align: center;
}
.rating-media img,
.rating-media video {
    max-width: 100%;
    max-height: 250px;
    border-radius: 12px;
    object-fit: cover;
}
.rating-media-caption {
    font-size: 0.7rem;
    color: var(--text-muted);
    margin-top: 6px;
}
.poll-duel-container {
    margin-top: 8px;
}
.duel-vote {
    display: flex;
    gap: 16px;
    justify-content: center;
    margin-bottom: 16px;
    flex-wrap: wrap;
}
.duel-option-vote {
    flex: 1;
    min-width: 200px;
    text-align: center;
    padding: 16px;
    background: var(--bg-card);
    border-radius: 16px;
    cursor: pointer;
    transition: all 0.2s ease;
    border: 2px solid transparent;
}
.duel-option-vote:hover {
    transform: translateY(-4px);
    border-color: var(--orange);
}
.duel-option-vote.selected {
    border-color: var(--orange);
    background: var(--orange-faint);
}
.duel-vote-image {
    width: 100%;
    max-height: 150px;
    object-fit: cover;
    border-radius: 12px;
    margin-bottom: 8px;
}
.duel-option-text {
    font-weight: 600;
}
.duel-results {
    display: flex;
    gap: 16px;
    flex-wrap: wrap;
}
.duel-option-result {
    flex: 1;
    min-width: 200px;
    text-align: center;
    padding: 12px;
    background: var(--bg-card);
    border-radius: 12px;
}
.duel-result-image {
    width: 100%;
    max-height: 120px;
    object-fit: cover;
    border-radius: 8px;
    margin-bottom: 8px;
}
.duel-percentage {
    font-size: 1.5rem;
    font-weight: 700;
    color: var(--orange);
    margin: 8px 0;
}
.duel-votes {
    font-size: 0.7rem;
    color: var(--text-muted);
}
.compose-box {
    display: flex;
    gap: 12px;
    padding: 16px;
    background: var(--bg-card);
    border-radius: 16px;
    margin-bottom: 20px;
}
.compose-avatar {
    width: 44px;
    height: 44px;
    border-radius: 50%;
    object-fit: cover;
}
.compose-body {
    flex: 1;
}
.compose-textarea {
    width: 100%;
    padding: 12px;
    border: 1px solid var(--border);
    border-radius: 12px;
    background: var(--bg-card2);
    color: var(--text-primary);
    font-size: 0.95rem;
    resize: vertical;
    font-family: inherit;
}
.compose-textarea:focus {
    outline: none;
    border-color: var(--orange);
}
.compose-options {
    display: flex;
    gap: 8px;
    margin-top: 12px;
    align-items: center;
    flex-wrap: wrap;
}
.compose-opt-btn {
    background: transparent;
    border: none;
    cursor: pointer;
    color: var(--orange);
    font-size: 1.1rem;
    padding: 8px;
    border-radius: 50%;
    transition: all 0.2s;
}
.compose-opt-btn:hover {
    background: var(--orange-faint);
}
.compose-submit {
    margin-left: auto;
    background: var(--orange);
    color: white;
    border: none;
    padding: 8px 20px;
    border-radius: 30px;
    font-weight: 700;
    cursor: pointer;
    transition: all 0.2s;
}
.compose-submit:hover {
    transform: scale(1.02);
    opacity: 0.9;
}
.feed-tabs {
    display: flex;
    border-bottom: 1px solid var(--border);
    position: sticky;
    top: 0;
    background: var(--bg);
    z-index: 10;
}
.feed-tab.active {
    color: var(--orange);
    border-bottom: 2px solid var(--orange);
}
.bark-card {
    padding: 16px;
    border-bottom: 1px solid var(--border);
    transition: background 0.2s;
}
.bark-card:hover {
    background: var(--bg-card2);
}
.bark-header {
    display: flex;
    gap: 12px;
    margin-bottom: 8px;
}
.bark-avatar {
    width: 44px;
    height: 44px;
    border-radius: 50%;
    object-fit: cover;
}
.bark-user-info {
    flex: 1;
}
.bark-display-name {
    font-weight: 700;
    display: flex;
    align-items: center;
    gap: 4px;
}
.bark-handle {
    font-size: 0.8rem;
    color: var(--text-muted);
}
.bark-time {
    font-size: 0.7rem;
    color: var(--text-muted);
}
.bark-content {
    margin: 8px 0;
    line-height: 1.4;
    word-break: break-word;
}
.bark-media {
    display: grid;
    gap: 4px;
    margin: 8px 0;
    border-radius: 16px;
    overflow: hidden;
}
.bark-media.count-1 {
    grid-template-columns: 1fr;
}
.bark-media.count-2 {
    grid-template-columns: 1fr 1fr;
}
.bark-media.count-3 {
    grid-template-columns: 1fr 1fr;
}
.bark-media.count-4 {
    grid-template-columns: 1fr 1fr;
}
.media-item img,
.media-item video {
    width: 100%;
    height: 100%;
    object-fit: cover;
    cursor: pointer;
}
.bark-actions {
    display: flex;
    gap: 16px;
    margin-top: 12px;
    padding-top: 8px;
    border-top: 1px solid var(--border);
    flex-wrap: wrap;
}
.bark-action-btn {
    background: transparent;
    border: none;
    cursor: pointer;
    color: var(--text-muted);
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 0.85rem;
    transition: all 0.2s;
    padding: 6px 12px;
    border-radius: 30px;
}
.bark-action-btn:hover {
    background: var(--bg-card2);
}
.bark-action-btn.active {
    color: var(--orange);
}
.bark-action-btn.paw.active {
    color: #FF6B6B;
}
.bark-action-btn.rebark.active {
    color: #4CAF50;
}
.bark-action-btn.bury.active {
    color: #FFD700;
}
.rebark-indicator {
    font-size: 0.75rem;
    color: var(--text-muted);
    margin-bottom: 8px;
    padding-left: 44px;
}
.quoted-bark {
    margin: 8px 0;
    padding: 12px;
    background: var(--bg-card2);
    border-radius: 12px;
    border: 1px solid var(--border);
    cursor: pointer;
}
.quoted-bark-header {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-bottom: 8px;
    flex-wrap: wrap;
}
.quoted-bark-avatar {
    width: 24px;
    height: 24px;
    border-radius: 50%;
}
.quoted-bark-name {
    font-weight: 600;
    font-size: 0.85rem;
}
.quoted-bark-handle {
    font-size: 0.7rem;
    color: var(--text-muted);
}
.quoted-bark-content {
    font-size: 0.85rem;
    word-break: break-word;
}
.quoted-bark-media {
    display: flex;
    gap: 4px;
    margin-top: 8px;
    flex-wrap: wrap;
}
.quoted-bark-media img,
.quoted-bark-media video {
    max-width: 100px;
    max-height: 80px;
    border-radius: 8px;
    object-fit: cover;
}
.sidebar-right {
    padding: 16px;
}
.widget {
    background: var(--bg-card);
    border-radius: 16px;
    padding: 16px;
    margin-bottom: 20px;
}
.widget-title {
    font-weight: 700;
    margin-bottom: 12px;
}
.follow-item {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 8px 0;
}
.follow-avatar {
    width: 40px;
    height: 40px;
    border-radius: 50%;
    object-fit: cover;
}
.follow-info {
    flex: 1;
}
.follow-name {
    font-weight: 600;
}
.follow-handle {
    font-size: 0.7rem;
    color: var(--text-muted);
}
.follow-btn {
    background: var(--orange);
    color: white;
    border: none;
    padding: 6px 16px;
    border-radius: 20px;
    cursor: pointer;
    font-weight: 600;
}
.follow-btn.following {
    background: transparent;
    border: 1px solid var(--border);
    color: var(--text-primary);
}
.loader {
    text-align: center;
    padding: 40px;
}
.text-muted {
    color: var(--text-muted);
}

/* Responsive Styles */
@media (max-width: 768px) {
    .bark-actions {
        flex-wrap: wrap;
        gap: 8px;
    }
    
    .bark-action-btn {
        padding: 4px 8px;
        font-size: 0.75rem;
    }
    
    .bark-action-btn .action-count {
        font-size: 0.7rem;
    }
    
    .bark-media.count-2,
    .bark-media.count-3,
    .bark-media.count-4 {
        grid-template-columns: repeat(2, 1fr);
    }
    
    .poll-card {
        padding: 12px;
    }
    
    .poll-question {
        font-size: 0.9rem;
    }
    
    .rating-stars-label {
        width: 60px;
    }
    
    .rating-star-select {
        font-size: 1.2rem;
    }
    
    .duel-vote {
        flex-direction: column;
        gap: 12px;
    }
    
    .duel-option-vote {
        padding: 12px;
        min-width: auto;
    }
    
    .duel-results {
        flex-direction: column;
    }
    
    .duel-option-result {
        min-width: auto;
    }
    
    .quoted-bark {
        padding: 8px;
    }
    
    .quoted-bark-header {
        flex-wrap: wrap;
    }
    
    .quoted-bark-media {
        flex-wrap: wrap;
    }
    
    .quoted-bark-media img,
    .quoted-bark-media video {
        max-width: 80px;
        max-height: 60px;
    }
}

@media (max-width: 480px) {
    .bark-header {
        gap: 8px;
    }
    
    .bark-avatar {
        width: 36px;
        height: 36px;
    }
    
    .bark-display-name {
        font-size: 0.85rem;
    }
    
    .bark-handle {
        font-size: 0.7rem;
    }
    
    .bark-content {
        font-size: 0.85rem;
    }
    
    .bark-actions {
        gap: 4px;
    }
    
    .bark-action-btn {
        padding: 2px 6px;
        font-size: 0.7rem;
    }
    
    .poll-options {
        gap: 8px;
    }
    
    .poll-option {
        padding: 8px;
    }
    
    .rating-row {
        gap: 6px;
        flex-wrap: wrap;
    }
    
    .rating-stars-label {
        width: auto;
    }
    
    .duel-option-vote {
        padding: 8px;
    }
    
    .duel-vote-image {
        max-height: 100px;
    }
    
    .duel-result-image {
        max-height: 80px;
    }
}
</style>

<!-- Main Feed -->
<main class="main-feed">
    <div class="feed-tabs">
	    <a href="?tab=for_you" class="feed-tab <?= $tab === 'for_you' ? 'active' : '' ?>"><?= __('feed_for_you') ?></a>
	    <a href="?tab=following" class="feed-tab <?= $tab === 'following' ? 'active' : '' ?>"><?= __('feed_following') ?></a>
	</div>

    <!-- Compose Box -->
    <div class="compose-box">
        <img src="<?= get_user_avatar($currentUser) ?>" class="compose-avatar" alt="avatar">
        <div class="compose-body">
            <textarea class="compose-textarea" placeholder="<?= __('bark_placeholder') ?>"
                onclick="openBarkModal()" readonly rows="2"></textarea>
            <div class="compose-options">
                <button class="compose-opt-btn" onclick="openBarkModal()" title="<?= __('modal_add_media') ?>">
                    <i class="fa-solid fa-image"></i>
                </button>
                <button class="compose-opt-btn" onclick="openBarkModalWithAnon()" title="<?= __('bark_anonymous') ?>">
                    <i class="fa-solid fa-user-secret"></i>
                </button>
                <a href="<?= SITE_URL ?>/pages/create-poll.php" class="compose-opt-btn" title="<?= __('poll_create') ?>">
                    <i class="fa-solid fa-chart-simple"></i>
                </a>
                <button class="compose-submit" onclick="openBarkModal()"><i class="fa-solid fa-paw"></i> <?= __('bark_post') ?></button>
            </div>
        </div>
    </div>

    <!-- Barks List -->
    <?php if (empty($barks)): ?>
        <div class="loader">
            <div style="font-size:3rem;margin-bottom:12px"><i class="fa-solid fa-paw"></i></div>
            <p style="font-weight:600"><?= __('feed_no_barks') ?></p>
            <p class="text-muted"><?= __('feed_follow_suggest') ?></p>
        </div>
    <?php else: ?>
        <?php foreach ($barks as $bark):
            $barkUser = [
                'id'           => $bark['user_id'],
                'username'     => $bark['username'],
                'display_name' => $bark['display_name'],
                'avatar'       => $bark['avatar'],
                'gender'       => $bark['gender'],
                'is_verified'  => $bark['is_verified'],
            ];
            $avatarUrl = get_user_avatar($barkUser);
            $isOwn     = $bark['user_id'] === $_SESSION['user_id'];
            $media     = $mediaMap[$bark['id']] ?? [];
            
            $original = null;
            if (in_array($bark['bark_type'], ['rebark', 'howl']) && $bark['original_bark_id']) {
                $original = $originalBarks[$bark['original_bark_id']] ?? null;
            }
            
            $pollData = $bark['poll_id'] ? ($pollsData[$bark['poll_id']] ?? null) : null;
        ?>
        <article class="bark-card" data-bark-id="<?= $bark['id'] ?>">
            
            <?php if ($bark['bark_type'] === 'rebark' && $original): ?>
                <div class="rebark-indicator">
                    <i class="fa-solid fa-retweet"></i>
                    <?php if ($bark['is_anonymous']): ?>
                        <i class="fa-solid fa-glasses"></i> <?= __('bark_anonymous') ?> rebarked
                    <?php else: ?>
                        <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($bark['username']) ?>">
                            @<?= htmlspecialchars($bark['username']) ?>
                        </a> <?= __('action_rebark') ?>d
                    <?php endif; ?>
                </div>
            <?php elseif ($bark['bark_type'] === 'howl' && $original): ?>
                <div class="rebark-indicator" style="color: #9B59B6">
                    <i class="fa-solid fa-quote-left"></i>
                    <?php if ($bark['is_anonymous']): ?>
                        <i class="fa-solid fa-glasses"></i> <?= __('bark_anonymous') ?> howled
                    <?php else: ?>
                        <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($bark['username']) ?>">
                            @<?= htmlspecialchars($bark['username']) ?>
                        </a> <?= __('action_howl') ?>ed
                    <?php endif; ?>
                </div>
            <?php endif; ?>
            
            <div class="bark-header">
                <?php if (!$bark['is_anonymous']): ?>
                <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($bark['username']) ?>">
                    <img src="<?= $avatarUrl ?>" class="bark-avatar" alt="avatar">
                </a>
                <?php else: ?>
                <div class="bark-avatar" style="background:var(--bg-card2);display:flex;align-items:center;justify-content:center;font-size:1.2rem"><i class="fa-solid fa-glasses"></i></div>
                <?php endif; ?>

                <div class="bark-user-info">
                    <div class="bark-display-name">
                        <?php if ($bark['is_anonymous']): ?>
                            <span><i class="fa-solid fa-glasses"></i> <?= __('bark_anonymous') ?></span>
                        <?php else: ?>
                            <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($bark['username']) ?>">
                                <?= htmlspecialchars($barkUser['display_name']) ?>
                            </a>
                            <?php if ($barkUser['is_verified']): ?>
                                <span class="verified-badge">✓</span>
                            <?php endif; ?>
                        <?php endif; ?>
                    </div>
                    <?php if (!$bark['is_anonymous']): ?>
                    <div class="bark-handle">@<?= htmlspecialchars($bark['username']) ?></div>
                    <?php endif; ?>
                </div>

                <span class="bark-time"><?= time_ago($bark['created_at']) ?></span>

                <div class="bark-more">
                    <button class="bark-more-btn" data-dropdown="dd-<?= $bark['id'] ?>">
                        <i class="fa-solid fa-ellipsis"></i>
                    </button>
                    <div class="dropdown" id="dd-<?= $bark['id'] ?>">
                        <?php if ($isOwn): ?>
                        <div class="dropdown-item" onclick="openEditBarkModal(<?= $bark['id'] ?>, '<?= addslashes(html_entity_decode($bark['content'], ENT_QUOTES, 'UTF-8')) ?>')">
                            <i class="fa-solid fa-pen"></i> <?= __('action_edit') ?>
                        </div>
                        <div class="dropdown-item danger" onclick="deleteBark(<?= $bark['id'] ?>)">
                            <i class="fa-solid fa-trash"></i> <?= __('action_delete') ?>
                        </div>
                        <?php else: ?>
                        <?php if (empty($bark['is_anonymous'])): ?>
                        <div class="dropdown-item" onclick="location.href='<?= SITE_URL ?>/pages/messages.php?to=<?= (int)$bark['user_id'] ?>'">
                            <i class="fa-solid fa-envelope"></i> <?= __('profile_message') ?? 'Envoyer un message' ?>
                        </div>
                        <?php endif; ?>
                        <div class="dropdown-item" onclick="openReport(<?= $bark['id'] ?>)">
                            <i class="fa-solid fa-flag"></i> <?= __('action_report') ?>
                        </div>
                        <?php endif; ?>
                        <div class="dropdown-item" onclick="navigator.clipboard.writeText('<?= SITE_URL ?>/pages/bark.php?id=<?= $bark['id'] ?>').then(()=>showToast('<?= __('action_link_copied') ?>','success'))">
                            <i class="fa-solid fa-link"></i> <?= __('action_copy_link') ?>
                        </div>
                    </div>
                </div>
            </div>

            <?php if ($bark['bark_type'] === 'howl' && !empty($bark['content'])): ?>
                <div class="bark-content" style="margin-bottom: 12px;">
                    <?= display_bark_content($bark['content']) ?>
                </div>
            <?php endif; ?>

            <?php if ($original): 
                $origUser = [
                    'id' => $original['user_id'],
                    'username' => $original['username'],
                    'display_name' => $original['display_name'],
                    'avatar' => $original['avatar'],
                    'gender' => $original['gender'],
                    'is_verified' => $original['is_verified'],
                ];
                $origAvatar = get_user_avatar($origUser);
                $origMedia = $original['media'] ?? [];
                $origMediaCount = count($origMedia);
            ?>
                <div class="quoted-bark" onclick="location.href='<?= SITE_URL ?>/pages/bark.php?id=<?= $original['id'] ?>'">
                    <div class="quoted-bark-header">
                        <?php if (!$original['is_anonymous']): ?>
                        <img src="<?= $origAvatar ?>" class="quoted-bark-avatar" alt="">
                        <div>
                            <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($original['username']) ?>" class="quoted-bark-name">
                                <?= htmlspecialchars($original['display_name']) ?>
                                <?php if ($original['is_verified']): ?>✓<?php endif; ?>
                            </a>
                            <span class="quoted-bark-handle">@<?= htmlspecialchars($original['username']) ?></span>
                        </div>
                        <?php else: ?>
                        <div class="quoted-bark-avatar" style="background:var(--bg-card2);display:flex;align-items:center;justify-content:center;font-size:0.7rem;flex-shrink:0"><i class="fa-solid fa-glasses"></i></div>
                        <div>
                            <span class="quoted-bark-name"><i class="fa-solid fa-glasses"></i> <?= __('bark_anonymous') ?></span>
                        </div>
                        <?php endif; ?>
                        <span style="margin-left:auto; font-size:0.7rem; color:var(--text-muted)">
                            <?= time_ago($original['created_at']) ?>
                        </span>
                    </div>
                    <div class="quoted-bark-content">
                        <?= display_bark_content($original['content']) ?>
                    </div>
                    
                    <?php if ($origMediaCount > 0): ?>
                        <div class="quoted-bark-media">
                            <?php foreach (array_slice($origMedia, 0, 2) as $m): ?>
                                <?php if ($m['type'] === 'image'): ?>
                                    <img src="<?= getImageUrlWithCacheBust($m['url'], 'media') ?>" 
                                         alt="media" loading="lazy"
                                         onclick="event.stopPropagation(); openMediaViewer('<?= SITE_URL ?>/assets/uploads/media/<?= htmlspecialchars($m['url']) ?>')">
                                <?php else: ?>
                                    <video src="<?= SITE_URL ?>/assets/uploads/media/<?= htmlspecialchars($m['url']) ?>" 
                                           controls preload="none"
                                           onclick="event.stopPropagation()"></video>
                                <?php endif; ?>
                            <?php endforeach; ?>
                            <?php if ($origMediaCount > 2): ?>
                                <div class="media-more" onclick="event.stopPropagation(); location.href='<?= SITE_URL ?>/pages/bark.php?id=<?= $original['id'] ?>'">
                                    +<?= $origMediaCount - 2 ?> <?= __('show_more') ?>
                                </div>
                            <?php endif; ?>
                        </div>
                    <?php endif; ?>
                </div>
            <?php else: ?>
                <div class="bark-content">
                    <?= display_bark_content($bark['content']) ?>
                </div>
            <?php endif; ?>

            <?php if ($bark['bark_type'] === 'normal' && !empty($media)): ?>
            <div class="bark-media count-<?= min(count($media), 4) ?>">
                <?php foreach (array_slice($media, 0, 4) as $m): ?>
                <div class="media-item">
                    <?php if ($m['type'] === 'image'): ?>
                        <img src="<?= getImageUrlWithCacheBust($m['url'], 'media') ?>"
                             alt="media" loading="lazy"
                             onclick="openMediaViewer('<?= SITE_URL ?>/assets/uploads/media/<?= htmlspecialchars($m['url']) ?>')">
                    <?php else: ?>
                        <video src="<?= SITE_URL ?>/assets/uploads/media/<?= htmlspecialchars($m['url']) ?>"
                               controls preload="none"></video>
                    <?php endif; ?>
                </div>
                <?php endforeach; ?>
            </div>
            <?php endif; ?>

            <!-- Affichage du sondage -->
            <?php if ($pollData): 
                $poll = $pollData['poll'];
                $options = $pollData['options'];
                $userVoted = $pollData['user_voted'];
                $isExpired = $poll['expires_at'] && strtotime($poll['expires_at']) < time();
                $totalVotes = (int)$pollData['total_votes'];
            ?>
                <div class="poll-container" data-poll-id="<?= $poll['id'] ?>" id="poll-<?= $poll['id'] ?>" data-loaded="true">
                    <div class="poll-card">
                        <div class="poll-question">
                            <i class="fa-solid fa-chart-simple"></i>
                            <?= htmlspecialchars($poll['question']) ?>
                        </div>
                        
                        <?php if ($isExpired): ?>
                            <div class="poll-expired-badge"><i class="fa-solid fa-clock"></i> <?= __('poll_closed') ?></div>
                        <?php endif; ?>
                        
                        <?php if ($poll['poll_type'] === 'rating'): ?>
                            <!-- Rating Poll -->
                            <?php if (!empty($poll['media_url']) && !empty($poll['media_type'])): 
                                $ratingImageUrl = getImageUrlWithCacheBust($poll['media_url'], 'poll');
                            ?>
                                <div class="rating-media-container">
                                    <div class="rating-media" onclick="openMediaViewer('<?= SITE_URL ?>/assets/uploads/polls/<?= htmlspecialchars($poll['media_url']) ?>')">
                                        <?php if ($poll['media_type'] === 'image'): ?>
                                            <img src="<?= $ratingImageUrl ?>" 
                                                 alt="<?= __('poll_type_rating') ?> media" loading="lazy"
                                                 style="max-width:100%; max-height:250px; border-radius:12px; object-fit:cover">
                                        <?php elseif ($poll['media_type'] === 'video'): ?>
                                            <video src="<?= SITE_URL ?>/assets/uploads/polls/<?= htmlspecialchars($poll['media_url']) ?>" 
                                                   controls preload="metadata"
                                                   style="max-width:100%; max-height:250px; border-radius:12px"
                                                   onclick="event.stopPropagation()"></video>
                                        <?php endif; ?>
                                    </div>
                                    <div class="rating-media-caption">
                                        <i class="fa-solid fa-camera"></i> <?= __('modal_click_view_fullsize') ?>
                                    </div>
                                </div>
                            <?php endif; ?>
                            
                            <div class="poll-rating-container">
                                <?php if ($userVoted || $isExpired): ?>
                                    <div class="rating-results">
                                        <?php 
                                        $totalRating = 0;
                                        $ratingCount = 0;
                                        foreach ($options as $opt):
                                            $stars = (int)$opt['option_text'];
                                            $totalRating += $stars * $opt['vote_count'];
                                            $ratingCount += $opt['vote_count'];
                                        ?>
                                        <div class="rating-row">
                                            <div class="rating-stars-label">
                                                <?= str_repeat('★', $stars) . str_repeat('☆', 5 - $stars) ?>
                                            </div>
                                            <div class="poll-progress-bar" style="flex:1">
                                                <div class="poll-progress-fill" style="width: <?= $opt['percentage'] ?>%"></div>
                                            </div>
                                            <div class="rating-percent"><?= $opt['percentage'] ?>%</div>
                                        </div>
                                        <?php endforeach; ?>
                                        <?php $avgRating = $ratingCount > 0 ? round($totalRating / $ratingCount, 1) : 0; ?>
                                        <div class="average-rating">
                                            <i class="fa-solid fa-star"></i> <?= __('poll_average') ?>: <?= $avgRating ?>/5 ★ (<?= $totalVotes ?> <?= __('poll_vote') ?><?= $totalVotes != 1 ? 's' : '' ?>)
                                        </div>
                                    </div>
                                <?php else: ?>
                                    <div class="rating-vote">
                                        <div class="rating-stars-input" id="ratingStars-<?= $poll['id'] ?>">
                                            <?php for ($i = 1; $i <= 5; $i++): ?>
                                                <i class="fa-regular fa-star rating-star-select" data-value="<?= $i ?>" data-poll-id="<?= $poll['id'] ?>"></i>
                                            <?php endfor; ?>
                                        </div>
                                        <button class="poll-vote-btn btn btn-primary btn-sm" onclick="submitRatingVote(<?= $poll['id'] ?>)">
                                            <i class="fa-solid fa-check"></i> <?= __('poll_rate') ?>
                                        </button>
                                    </div>
                                <?php endif; ?>
                            </div>
                            
                        <?php elseif ($poll['poll_type'] === 'duel'): ?>
                            <!-- DUEL POLL (VS) - VERSION CORRIGÉE AVEC ANTI-CACHE -->
                            <div class="poll-duel-container">
                                <?php 
                                // S'assurer que les options sont dans le bon ordre (option_order 0 = option1, 1 = option2)
                                $sortedOptions = $options;
                                usort($sortedOptions, function($a, $b) {
                                    return $a['option_order'] - $b['option_order'];
                                });
                                ?>
                                
                                <?php if ($userVoted || $isExpired): ?>
                                    <div class="duel-results">
                                        <?php foreach ($sortedOptions as $opt): ?>
                                        <div class="duel-option-result">
                                            <?php if (!empty($opt['image_url'])): 
                                                $duelImageUrl = getImageUrlWithCacheBust($opt['image_url'], 'poll');
                                            ?>
                                                <img src="<?= $duelImageUrl ?>" class="duel-result-image" alt="Option image">
                                            <?php else: ?>
                                                <div class="duel-result-image" style="background:var(--bg-card2); display:flex; align-items:center; justify-content:center; min-height:100px;">
                                                    <i class="fa-solid fa-image" style="font-size:2rem; opacity:0.3"></i>
                                                </div>
                                            <?php endif; ?>
                                            <div class="duel-option-text"><?= htmlspecialchars($opt['option_text']) ?></div>
                                            <div class="duel-percentage"><?= $opt['percentage'] ?>%</div>
                                            <div class="poll-progress-bar">
                                                <div class="poll-progress-fill" style="width: <?= $opt['percentage'] ?>%"></div>
                                            </div>
                                            <div class="duel-votes"><?= $opt['vote_count'] ?> <?= __('poll_vote') ?><?= $opt['vote_count'] != 1 ? 's' : '' ?></div>
                                        </div>
                                        <?php endforeach; ?>
                                    </div>
                                <?php else: ?>
                                    <div class="duel-vote">
                                        <?php foreach ($sortedOptions as $opt): ?>
                                        <div class="duel-option-vote" onclick="selectDuelOption(<?= $poll['id'] ?>, <?= $opt['id'] ?>)" data-opt-id="<?= $opt['id'] ?>">
                                            <?php if (!empty($opt['image_url'])): 
                                                $duelImageUrl = getImageUrlWithCacheBust($opt['image_url'], 'poll');
                                            ?>
                                                <img src="<?= $duelImageUrl ?>" class="duel-vote-image" alt="Option image">
                                            <?php else: ?>
                                                <div class="duel-vote-image" style="background:var(--bg-card2); display:flex; align-items:center; justify-content:center; min-height:120px;">
                                                    <i class="fa-solid fa-image" style="font-size:2rem; opacity:0.3"></i>
                                                </div>
                                            <?php endif; ?>
                                            <div class="duel-option-text"><?= htmlspecialchars($opt['option_text']) ?></div>
                                        </div>
                                        <?php endforeach; ?>
                                    </div>
                                    <input type="hidden" id="selectedDuelOption-<?= $poll['id'] ?>" value="">
                                    <button class="poll-vote-btn btn btn-primary btn-sm" onclick="submitDuelVote(<?= $poll['id'] ?>)">
                                        <i class="fa-solid fa-fist-raised"></i> <?= __('poll_fight') ?>
                                    </button>
                                <?php endif; ?>
                            </div>
                            
                        <?php else: ?>
                            <div class="poll-options <?= $poll['poll_type'] === 'multiple' ? 'poll-multiple' : '' ?>">
                                <?php foreach ($options as $opt): ?>
                                    <?php if ($userVoted || $isExpired): ?>
                                        <div class="poll-option-result">
                                            <div class="poll-option-text">
                                                <?= htmlspecialchars($opt['option_text']) ?>
                                                <span class="poll-option-percent"><?= $opt['percentage'] ?>%</span>
                                            </div>
                                            <div class="poll-progress-bar">
                                                <div class="poll-progress-fill" style="width: <?= $opt['percentage'] ?>%"></div>
                                            </div>
                                            <div class="poll-option-votes"><?= $opt['vote_count'] ?> <?= __('poll_vote') ?><?= $opt['vote_count'] != 1 ? 's' : '' ?></div>
                                        </div>
                                    <?php else: ?>
                                        <label class="poll-option <?= $poll['poll_type'] === 'multiple' ? 'poll-checkbox' : 'poll-radio' ?>">
                                            <input type="<?= $poll['poll_type'] === 'multiple' ? 'checkbox' : 'radio' ?>" 
                                                   name="poll_option_<?= $poll['id'] ?>" 
                                                   value="<?= $opt['id'] ?>">
                                            <span class="poll-option-text"><?= htmlspecialchars($opt['option_text']) ?></span>
                                        </label>
                                    <?php endif; ?>
                                <?php endforeach; ?>
                                
                                <?php if (!$userVoted && !$isExpired): ?>
                                    <button class="poll-vote-btn btn btn-primary btn-sm" onclick="submitPollVote(<?= $poll['id'] ?>, '<?= $poll['poll_type'] ?>')">
                                        <i class="fa-solid fa-check"></i> <?= __('poll_vote') ?>
                                    </button>
                                <?php endif; ?>
                            </div>
                        <?php endif; ?>
                        
                        <div class="poll-footer">
                            <span class="poll-vote-count">
                                <i class="fa-regular fa-chart-bar"></i> <?= $totalVotes ?> <?= __('poll_vote') ?><?= $totalVotes != 1 ? 's' : '' ?>
                            </span>
                            <?php if ($poll['expires_at'] && !$isExpired): ?>
                                <span class="poll-expires">· <?= __('poll_closes') ?> <?= date('d M Y', strtotime($poll['expires_at'])) ?></span>
                            <?php endif; ?>
                        </div>
                    </div>
                </div>
            <?php endif; ?>

            <!-- Actions -->
            <div class="bark-actions">
                <?php if (!$isOwn): ?>
                <button class="bark-action-btn paw <?= $bark['user_pawed'] ? 'active' : '' ?>"
                        onclick="togglePaw(<?= $bark['id'] ?>, this)">
                    <i class="fa-solid fa-paw"></i>
                    <span class="action-count"><?= $bark['paw_count'] ?></span>
                </button>
                <?php else: ?>
                <button class="bark-action-btn paw" disabled style="opacity:0.3">
                    <i class="fa-solid fa-paw"></i>
                    <span class="action-count"><?= $bark['paw_count'] ?></span>
                </button>
                <?php endif; ?>

                <button class="bark-action-btn yap"
                        onclick="openYap(<?= $bark['id'] ?>, `<?= addslashes(htmlspecialchars(substr($bark['content'], 0, 100))) ?>`)">
                    <i class="fa-solid fa-comment"></i>
                    <span class="action-count"><?= $bark['yap_count'] ?></span>
                </button>

                <?php if (!$isOwn): ?>
                <button class="bark-action-btn rebark <?= $bark['user_rebarked'] ? 'active' : '' ?>"
                        onclick="toggleRebark(<?= $bark['id'] ?>, this)">
                    <i class="fa-solid fa-retweet"></i>
                    <span class="action-count"><?= $bark['rebark_count'] ?></span>
                </button>
                <?php else: ?>
                <button class="bark-action-btn rebark" disabled style="opacity:0.3">
                    <i class="fa-solid fa-retweet"></i>
                    <span class="action-count"><?= $bark['rebark_count'] ?></span>
                </button>
                <?php endif; ?>

                <button class="bark-action-btn howl"
                        onclick="openHowl(<?= $bark['id'] ?>, `<?= addslashes(htmlspecialchars(substr($bark['content'], 0, 100))) ?>`)">
                    <i class="fa-solid fa-wifi"></i>
                </button>

                <button class="bark-action-btn bury <?= ($bark['user_buried'] ?? 0) ? 'active' : '' ?>" 
                        onclick="toggleBury(<?= $bark['id'] ?>, this)">
                    <i class="fa-solid fa-bookmark"></i>
                </button>

                <?php if (!$isOwn): ?>
                <button class="bark-action-btn bone"
                        onclick="openBoneModal(<?= $bark['id'] ?>, <?= $bark['is_anonymous'] ? 0 : (int)$bark['user_id'] ?>, '<?= addslashes($bark['is_anonymous'] ? __('bark_anonymous') : $barkUser['display_name']) ?>')">
                    <i class="fa-solid fa-bone"></i>
                </button>
                <?php endif; ?>

                <!-- Lien vers la page individuelle du bark -->
                <a href="<?= SITE_URL ?>/pages/bark.php?id=<?= $bark['id'] ?>" class="bark-action-btn" style="margin-left:auto;text-decoration:none" title="<?= __('action_view_bark') ?>">
                    <i class="fa-solid fa-arrow-up-right-from-square"></i>
                </a>
            </div>
        </article>
        <?php endforeach; ?>

        <?php if (count($barks) >= BARKS_PER_PAGE): ?>
        <div style="text-align:center;padding:20px">
            <a href="?tab=<?= $tab ?>&page=<?= $page + 1 ?>" class="btn btn-outline"><?= __('feed_load_more') ?> <i class="fa-solid fa-paw"></i></a>
        </div>
        <?php endif; ?>
    <?php endif; ?>
</main>

<!-- Sidebar Right -->
<aside class="sidebar-right">
    <?php if (!empty($suggestions)): ?>
    <div class="widget">
        <div class="widget-title"><i class="fa-solid fa-paw"></i> <?= __('profile_who_to_follow') ?></div>
        <?php foreach ($suggestions as $s): ?>
        <div class="follow-item">
            <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($s['username']) ?>">
                <img src="<?= get_user_avatar($s) ?>" class="follow-avatar" alt="avatar">
            </a>
            <div class="follow-info">
                <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($s['username']) ?>">
                    <div class="follow-name"><?= htmlspecialchars($s['display_name']) ?></div>
                    <div class="follow-handle">@<?= htmlspecialchars($s['username']) ?></div>
                </a>
            </div>
            <button class="follow-btn" onclick="toggleFollow(<?= $s['id'] ?>, this)"><?= __('btn_follow') ?></button>
        </div>
        <?php endforeach; ?>
        <a href="<?= SITE_URL ?>/pages/people.php" class="widget-see-more"><?= __('view_all') ?> <i class="fa-solid fa-arrow-right"></i></a>
    </div>
    <?php endif; ?>

    <div style="font-size:0.75rem;color:var(--text-muted);padding:8px">
        <a href="<?= SITE_URL ?>/pages/terms.php"><?= __('footer_terms') ?></a> | 
        <a href="<?= SITE_URL ?>/pages/privacy.php"><?= __('footer_privacy') ?></a>
        <br><br><i class="fa-solid fa-dog"></i> BARKER © <?= date('Y') ?> · <?= __('footer_copyright_suffix') ?>
    </div>
</aside>

<script>
async function submitPollVote(pollId, pollType) {
    const container = document.getElementById(`poll-${pollId}`);
    const formData = new FormData();
    formData.append('poll_id', pollId);
    formData.append('csrf_token', getCsrfToken());
    
    if (pollType === 'multiple') {
        const checked = container.querySelectorAll('input[type="checkbox"]:checked');
        const optionIds = Array.from(checked).map(cb => cb.value);
        if (optionIds.length === 0) {
            showToast('<?= __('poll_select_option') ?>', 'error');
            return;
        }
        optionIds.forEach(id => formData.append('option_ids[]', id));
    } else {
        const selected = container.querySelector(`input[name="poll_option_${pollId}"]:checked`);
        if (!selected) {
            showToast('<?= __('poll_select_option') ?>', 'error');
            return;
        }
        formData.append('option_id', selected.value);
    }
    
    try {
        const res = await fetch(`${SITE_URL}/api/poll.php?action=vote`, {
            method: 'POST',
            body: formData,
            cache: 'no-cache',
            headers: {
                'Cache-Control': 'no-cache'
            }
        });
        const data = await res.json();
        if (data.success) {
            showToast('<?= __('poll_vote_recorded') ?>', 'success');
            setTimeout(() => location.reload(), 800);
        } else {
            showToast(data.error || '<?= __('error_voting') ?>', 'error');
        }
    } catch (error) {
        showToast('<?= __('error_network') ?>', 'error');
    }
}

async function submitRatingVote(pollId) {
    const selectedStar = document.querySelector(`#ratingStars-${pollId} .rating-star-select.selected`);
    if (!selectedStar) {
        showToast('<?= __('poll_select_rating') ?>', 'error');
        return;
    }
    
    const ratingValue = selectedStar.dataset.value;
    const formData = new FormData();
    formData.append('poll_id', pollId);
    formData.append('rating_value', ratingValue);
    formData.append('csrf_token', getCsrfToken());
    
    try {
        const res = await fetch(`${SITE_URL}/api/poll.php?action=vote`, {
            method: 'POST',
            body: formData,
            cache: 'no-cache',
            headers: {
                'Cache-Control': 'no-cache'
            }
        });
        const data = await res.json();
        if (data.success) {
            showToast(`<?= __('poll_rated') ?> ${ratingValue} <?= __('poll_stars') ?>!`, 'success');
            setTimeout(() => location.reload(), 800);
        } else {
            showToast(data.error || '<?= __('error_voting') ?>', 'error');
        }
    } catch (error) {
        showToast('<?= __('error_network') ?>', 'error');
    }
}

function selectDuelOption(pollId, optionId) {
    document.querySelectorAll(`#poll-${pollId} .duel-option-vote`).forEach(opt => {
        opt.classList.remove('selected');
    });
    const selected = document.querySelector(`#poll-${pollId} .duel-option-vote[data-opt-id="${optionId}"]`);
    if (selected) {
        selected.classList.add('selected');
        document.getElementById(`selectedDuelOption-${pollId}`).value = optionId;
    }
}

async function submitDuelVote(pollId) {
    const selectedOption = document.getElementById(`selectedDuelOption-${pollId}`).value;
    if (!selectedOption) {
        showToast('<?= __('poll_select_option') ?>', 'error');
        return;
    }
    
    const formData = new FormData();
    formData.append('poll_id', pollId);
    formData.append('option_id', selectedOption);
    formData.append('csrf_token', getCsrfToken());
    
    try {
        const res = await fetch(`${SITE_URL}/api/poll.php?action=vote`, {
            method: 'POST',
            body: formData,
            cache: 'no-cache',
            headers: {
                'Cache-Control': 'no-cache'
            }
        });
        const data = await res.json();
        if (data.success) {
            showToast('<?= __('poll_vote_cast') ?>!', 'success');
            setTimeout(() => location.reload(), 800);
        } else {
            showToast(data.error || '<?= __('error_voting') ?>', 'error');
        }
    } catch (error) {
        showToast('<?= __('error_network') ?>', 'error');
    }
}

document.addEventListener('DOMContentLoaded', function() {
    document.addEventListener('mouseover', function(e) {
        if (e.target.classList && e.target.classList.contains('rating-star-select')) {
            const val = parseInt(e.target.dataset.value);
            const container = e.target.parentElement;
            const stars = container.querySelectorAll('.rating-star-select');
            stars.forEach((star, idx) => {
                if (idx < val) {
                    star.className = 'fa-solid fa-star rating-star-select';
                } else {
                    star.className = 'fa-regular fa-star rating-star-select';
                }
            });
        }
    });
    
    document.addEventListener('mouseout', function(e) {
        if (e.target.classList && e.target.classList.contains('rating-star-select')) {
            const container = e.target.parentElement;
            const stars = container.querySelectorAll('.rating-star-select');
            stars.forEach(star => {
                if (!star.classList.contains('selected')) {
                    star.className = 'fa-regular fa-star rating-star-select';
                } else {
                    star.className = 'fa-solid fa-star rating-star-select selected';
                }
            });
        }
    });
    
    document.addEventListener('click', function(e) {
        if (e.target.classList && e.target.classList.contains('rating-star-select')) {
            const container = e.target.parentElement;
            const val = parseInt(e.target.dataset.value);
            const stars = container.querySelectorAll('.rating-star-select');
            stars.forEach((star, idx) => {
                star.classList.remove('selected');
                if (idx < val) {
                    star.className = 'fa-solid fa-star rating-star-select selected';
                } else {
                    star.className = 'fa-regular fa-star rating-star-select';
                }
            });
        }
    });
});
</script>

<?php include __DIR__ . '/includes/footer.php'; ?>