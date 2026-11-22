<?php
// ============================================================
// BARKER — Profile Page - VERSION CORRIGÉE AVEC ANTI-CACHE
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

// ============================================================
// ANTI-CACHE POUR TOUTE LA PAGE
// ============================================================
header('Cache-Control: no-cache, no-store, must-revalidate');
header('Pragma: no-cache');
header('Expires: 0');
header('Last-Modified: ' . gmdate('D, d M Y H:i:s') . ' GMT');

$username    = $_GET['u'] ?? '';
$currentUser = current_user();

if (!$username) redirect(SITE_URL . '/index.php');

/**
 * Fonction pour générer l'URL d'une image avec anti-cache basé sur la date de modification
 */
function getImageUrlWithCacheBust($imageUrl, $type = 'poll') {
    if (empty($imageUrl)) return null;
    
    $basePath = __DIR__ . '/../assets/uploads/';
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

$profile = DB::fetchOne(
    "SELECT * FROM users WHERE username = ? AND is_banned = 0",
    [$username]
);

if (!$profile) {
    http_response_code(404);
    include __DIR__ . '/../includes/header.php';
    echo '<div class="loader"><div style="font-size:3rem"><i class="fa-solid fa-paw"></i></div><p>' . __('profile_user_not_found') . '</p></div>';
    include __DIR__ . '/../includes/footer.php';
    exit;
}

$isOwnProfile = $profile['id'] === $_SESSION['user_id'];

// Follow status
$isFollowing = (bool)DB::fetchOne(
    "SELECT 1 FROM follows WHERE follower_id = ? AND following_id = ?",
    [$_SESSION['user_id'], $profile['id']]
);

// Stats
$stats = DB::fetchOne("
    SELECT
        (SELECT COUNT(*) FROM barks WHERE user_id = ? AND is_deleted = 0) AS barks,
        (SELECT COUNT(*) FROM follows WHERE following_id = ?)             AS followers,
        (SELECT COUNT(*) FROM follows WHERE follower_id = ?)              AS following,
        (SELECT COALESCE(SUM(amount),0) FROM bones WHERE receiver_id = ? AND status = 'completed') AS total_bones
", [$profile['id'], $profile['id'], $profile['id'], $profile['id']]);

// Fonction pour récupérer le bark original
function getOriginalBarkDetails($barkId) {
    return DB::fetchOne("
        SELECT b.*, u.username, u.display_name, u.avatar, u.gender, u.is_verified
        FROM barks b
        JOIN users u ON u.id = b.user_id
        WHERE b.id = ? AND b.is_deleted = 0 AND u.is_banned = 0
    ", [$barkId]);
}

// Barks avec support rebarks/howl
$anonFilter = $isOwnProfile ? '' : 'AND b.is_anonymous = 0';
$barks = DB::fetchAll("
    SELECT b.*, b.bark_type, b.original_bark_id, b.poll_id,
           u.username, u.display_name, u.avatar, u.gender, u.is_verified,
           (SELECT COUNT(*) FROM paws WHERE bark_id = b.id) AS paw_count,
           (SELECT COUNT(*) FROM yaps WHERE bark_id = b.id) AS yap_count,
           (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id) AS rebark_count,
           (SELECT COUNT(*) FROM paws WHERE bark_id = b.id AND user_id = ?) AS user_pawed,
           (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id AND user_id = ?) AS user_rebarked,
           (SELECT COUNT(*) FROM buries WHERE bark_id = b.id AND user_id = ?) AS user_buried
    FROM barks b
    JOIN users u ON u.id = b.user_id
    WHERE b.user_id = ? AND b.is_deleted = 0 $anonFilter
    ORDER BY b.created_at DESC
    LIMIT 30
", [$_SESSION['user_id'], $_SESSION['user_id'], $_SESSION['user_id'], $profile['id']]);

$barkIds = array_column($barks, 'id');
$mediaMap = [];
if ($barkIds) {
    $ph = implode(',', array_fill(0, count($barkIds), '?'));
    foreach (DB::fetchAll("SELECT * FROM media WHERE bark_id IN ($ph)", $barkIds) as $m) {
        $mediaMap[$m['bark_id']][] = $m;
    }
}

// Récupérer les barks originaux pour les rebarks/howl
$originalBarks = [];
foreach ($barks as $bark) {
    if (in_array($bark['bark_type'], ['rebark', 'howl']) && $bark['original_bark_id']) {
        if (!isset($originalBarks[$bark['original_bark_id']])) {
            $original = getOriginalBarkDetails($bark['original_bark_id']);
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
            unset($opt);
            
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

$pageTitle = $profile['display_name'];
include __DIR__ . '/../includes/header.php';
?>

<style>
/* ============================================================
   PROFILE PAGE — REDESIGNED (Twitter/X + BARKER identity)
   ============================================================ */

/* ===== COVER + HEADER ===== */
.profile-cover-wrap {
    position: relative;
    width: 100%;
    height: 220px;
    overflow: hidden;
    background: linear-gradient(135deg, #1a1a1a 0%, #0a0a0a 100%);
}

.profile-cover {
    width: 100%;
    height: 100%;
    object-fit: cover;
    display: block;
    transition: transform 0.4s ease;
}

.profile-cover-wrap:hover .profile-cover {
    transform: scale(1.02);
}

.profile-cover-wrap::after {
    content: '';
    position: absolute;
    inset: 0;
    background: linear-gradient(180deg, transparent 40%, rgba(0,0,0,0.4) 100%);
    pointer-events: none;
}

.profile-header {
    padding: 0 20px 20px;
    position: relative;
    margin-top: -70px;
    z-index: 2;
}

.profile-avatar-wrap {
    display: inline-block;
    border-radius: 50%;
    border: 4px solid var(--bg, #000);
    background: var(--bg, #000);
    position: relative;
    transition: transform 0.2s ease;
}

.profile-avatar-wrap:hover {
    transform: scale(1.02);
}

.profile-avatar {
    width: 140px;
    height: 140px;
    border-radius: 50%;
    object-fit: cover;
    display: block;
}

/* ===== ACTION BUTTONS ROW ===== */
.profile-actions-row {
    display: flex;
    justify-content: space-between;
    align-items: flex-end;
    flex-wrap: wrap;
    gap: 12px;
    margin-bottom: 8px;
}

.profile-actions {
    display: flex;
    gap: 8px;
    align-items: center;
    padding-top: 76px;
}

/* Pill buttons */
.profile-actions .btn {
    padding: 9px 20px;
    border-radius: 9999px;
    font-size: 0.9rem;
    font-weight: 700;
    border: 1px solid var(--border);
    transition: all 0.2s ease;
    display: inline-flex;
    align-items: center;
    gap: 8px;
    white-space: nowrap;
}

.profile-actions .btn-primary {
    background: var(--orange);
    border-color: var(--orange);
    color: #fff;
}

.profile-actions .btn-primary:hover {
    background: #e65800;
    border-color: #e65800;
    transform: translateY(-1px);
    box-shadow: 0 4px 12px rgba(255, 98, 0, 0.3);
}

.profile-actions .btn-outline {
    background: transparent;
    color: var(--text-primary);
}

.profile-actions .btn-outline:hover {
    background: var(--bg-card2);
    border-color: var(--text-muted);
}

.profile-actions .btn-outline.following {
    border-color: var(--border);
    color: var(--text-secondary);
}

.profile-actions .btn-outline.following:hover {
    border-color: #f4212e;
    color: #f4212e;
    background: rgba(244, 33, 46, 0.08);
}

/* Icon-only button (message) */
.profile-actions .btn-icon {
    width: 40px;
    height: 40px;
    padding: 0;
    border-radius: 50%;
    justify-content: center;
}

/* ===== NAME / HANDLE / BIO ===== */
.profile-identity {
    margin-top: 4px;
}

.profile-name {
    font-size: 1.65rem;
    font-weight: 800;
    line-height: 1.15;
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
    color: var(--text-primary);
    letter-spacing: -0.5px;
}

.profile-handle {
    font-size: 0.95rem;
    color: var(--text-muted);
    margin-top: 2px;
}

.profile-bio {
    margin-top: 14px;
    font-size: 0.97rem;
    color: var(--text-primary);
    line-height: 1.5;
    max-width: 560px;
    word-break: break-word;
}

.profile-meta {
    display: flex;
    align-items: center;
    gap: 16px;
    flex-wrap: wrap;
    margin-top: 12px;
    font-size: 0.85rem;
    color: var(--text-muted);
}

.profile-meta-item {
    display: inline-flex;
    align-items: center;
    gap: 6px;
}

.profile-meta-item i {
    font-size: 0.85rem;
}

/* Bones badge */
.profile-bones-badge {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 4px 10px;
    background: rgba(255, 98, 0, 0.12);
    border-radius: 20px;
    color: var(--orange);
    font-weight: 600;
    font-size: 0.8rem;
}

/* ===== STATS ===== */
.profile-stats {
    display: flex;
    gap: 20px;
    margin-top: 16px;
    flex-wrap: wrap;
}

.profile-stat {
    cursor: pointer;
    font-size: 0.9rem;
    color: var(--text-secondary);
    transition: color 0.15s ease;
    display: inline-flex;
    align-items: baseline;
    gap: 5px;
    padding: 4px 0;
    position: relative;
}

.profile-stat:hover {
    color: var(--orange);
}

.profile-stat span {
    font-weight: 800;
    font-size: 1rem;
    color: var(--text-primary);
    transition: color 0.15s ease;
}

.profile-stat:hover span {
    color: var(--orange);
}

/* ===== TABS ===== */
.feed-tabs {
    display: flex;
    border-bottom: 1px solid var(--border);
    margin-top: 8px;
    overflow-x: auto;
    scrollbar-width: none;
}

.feed-tabs::-webkit-scrollbar { display: none; }

.feed-tab {
    flex: 1;
    min-width: 120px;
    text-align: center;
    padding: 16px;
    font-weight: 600;
    font-size: 0.9rem;
    color: var(--text-muted);
    cursor: pointer;
    transition: all 0.2s ease;
    position: relative;
}

.feed-tab:hover {
    background: var(--bg-card2);
    color: var(--text-primary);
}

.feed-tab.active {
    color: var(--text-primary);
}

.feed-tab.active::after {
    content: '';
    position: absolute;
    bottom: 0;
    left: 50%;
    transform: translateX(-50%);
    width: 56px;
    height: 3px;
    background: var(--orange);
    border-radius: 3px 3px 0 0;
}

/* ===== BARK CARDS ===== */
.bark-card {
    padding: 16px 20px;
    border-bottom: 1px solid var(--border);
    transition: background 0.2s ease;
    position: relative;
}

.bark-card:hover {
    background: rgba(255, 255, 255, 0.015);
}

.bark-header {
    display: flex;
    gap: 12px;
    margin-bottom: 10px;
    align-items: flex-start;
}

.bark-avatar {
    width: 44px;
    height: 44px;
    border-radius: 50%;
    object-fit: cover;
    flex-shrink: 0;
    transition: transform 0.15s ease;
}

.bark-avatar:hover { transform: scale(1.05); }

.bark-user-info {
    flex: 1;
    min-width: 0;
}

.bark-display-name {
    font-weight: 700;
    font-size: 0.95rem;
    display: flex;
    align-items: center;
    gap: 4px;
    flex-wrap: wrap;
    color: var(--text-primary);
}

.bark-display-name a { color: inherit; }
.bark-display-name a:hover { text-decoration: underline; }

.bark-handle {
    font-size: 0.82rem;
    color: var(--text-muted);
    margin-top: 1px;
}

.bark-time {
    font-size: 0.78rem;
    color: var(--text-muted);
    white-space: nowrap;
    padding-top: 2px;
}

.bark-content {
    margin: 4px 0 10px;
    line-height: 1.5;
    font-size: 0.97rem;
    word-break: break-word;
    color: var(--text-primary);
}

/* Media grid */
.bark-media {
    display: grid;
    gap: 4px;
    margin: 10px 0;
    border-radius: 16px;
    overflow: hidden;
    border: 1px solid var(--border);
}

.bark-media.count-1 { grid-template-columns: 1fr; }
.bark-media.count-2 { grid-template-columns: 1fr 1fr; }
.bark-media.count-3 { grid-template-columns: 1fr 1fr; }
.bark-media.count-4 { grid-template-columns: 1fr 1fr; }

.media-item {
    overflow: hidden;
    background: var(--bg-card2);
    position: relative;
}

.media-item img,
.media-item video {
    width: 100%;
    height: 100%;
    object-fit: cover;
    cursor: pointer;
    display: block;
    transition: transform 0.3s ease;
}

.media-item:hover img,
.media-item:hover video {
    transform: scale(1.03);
}

/* ===== ACTIONS ROW ===== */
.bark-actions {
    display: flex;
    gap: 4px;
    margin-top: 10px;
    padding-top: 4px;
    flex-wrap: wrap;
    align-items: center;
}

.bark-action-btn {
    background: transparent;
    border: none;
    cursor: pointer;
    color: var(--text-muted);
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-size: 0.82rem;
    font-weight: 500;
    transition: all 0.15s ease;
    padding: 6px 10px;
    border-radius: 9999px;
    text-decoration: none;
}

.bark-action-btn:hover {
    background: var(--bg-card2);
    color: var(--text-primary);
}

.bark-action-btn i { font-size: 0.95rem; }

.bark-action-btn.active { color: var(--orange); }
.bark-action-btn.paw.active { color: #FF6B6B; }
.bark-action-btn.paw.active:hover { background: rgba(255, 107, 107, 0.1); }
.bark-action-btn.rebark.active { color: #4CAF50; }
.bark-action-btn.rebark.active:hover { background: rgba(76, 175, 80, 0.1); }
.bark-action-btn.bury.active { color: #FFD700; }
.bark-action-btn.bury.active:hover { background: rgba(255, 215, 0, 0.1); }

.bark-action-btn:disabled {
    opacity: 0.3;
    cursor: not-allowed;
}

/* ===== REBARK / HOWL INDICATOR ===== */
.rebark-indicator {
    font-size: 0.78rem;
    color: var(--text-muted);
    margin-bottom: 10px;
    padding-left: 56px;
    display: flex;
    align-items: center;
    gap: 6px;
}

.rebark-indicator i {
    color: var(--text-muted);
}

.rebark-indicator a {
    color: inherit;
    font-weight: 600;
}

.rebark-indicator a:hover {
    text-decoration: underline;
}

/* ===== QUOTED BARK ===== */
.quoted-bark {
    margin: 10px 0;
    padding: 14px;
    background: var(--bg-card2);
    border-radius: 16px;
    border: 1px solid var(--border);
    cursor: pointer;
    transition: border-color 0.2s ease, background 0.2s ease;
}

.quoted-bark:hover {
    border-color: var(--orange);
    background: rgba(255, 98, 0, 0.03);
}

.quoted-bark-header {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-bottom: 8px;
    flex-wrap: wrap;
}

.quoted-bark-avatar {
    width: 26px;
    height: 26px;
    border-radius: 50%;
    object-fit: cover;
}

.quoted-bark-name {
    font-weight: 700;
    font-size: 0.85rem;
    color: var(--text-primary);
}

.quoted-bark-name a { color: inherit; }
.quoted-bark-name a:hover { text-decoration: underline; }

.quoted-bark-handle {
    font-size: 0.72rem;
    color: var(--text-muted);
}

.quoted-bark-content {
    font-size: 0.88rem;
    line-height: 1.45;
    word-break: break-word;
    color: var(--text-secondary);
}

.quoted-bark-media {
    display: flex;
    gap: 4px;
    margin-top: 10px;
    flex-wrap: wrap;
    border-radius: 10px;
    overflow: hidden;
}

.quoted-bark-media img,
.quoted-bark-media video {
    max-width: 120px;
    max-height: 90px;
    border-radius: 8px;
    object-fit: cover;
}

/* ===== VERIFIED BADGE ===== */
.verified-badge {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    background: var(--orange);
    color: #fff;
    border-radius: 50%;
    width: 18px;
    height: 18px;
    font-size: 0.65rem;
    flex-shrink: 0;
}

/* ===== ANON BADGE ===== */
.bark-anon-badge {
    background: var(--bg-card2);
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 0.65rem;
    color: var(--text-muted);
    margin-left: 6px;
}

/* ===== POLLS ===== */
.poll-container { margin: 12px 0; }

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
    flex-wrap: wrap;
}

.poll-expired-badge {
    background: var(--bg-card);
    padding: 4px 10px;
    border-radius: 20px;
    font-size: 0.72rem;
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
    padding: 12px;
    background: var(--bg-card);
    border-radius: 12px;
    cursor: pointer;
    transition: background 0.15s ease, border-color 0.15s ease;
    border: 1px solid transparent;
}

.poll-option:hover {
    background: var(--bg-card-hover);
    border-color: var(--border);
}

.poll-option input { margin: 0; accent-color: var(--orange); }

.poll-option-text {
    flex: 1;
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 10px;
}

.poll-option-result { padding: 10px; }

.poll-option-percent {
    font-weight: 700;
    color: var(--orange);
    font-size: 0.9rem;
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
    background: linear-gradient(90deg, var(--orange), #FF8C42);
    border-radius: 4px;
    transition: width 0.4s ease;
}

.poll-option-votes {
    font-size: 0.72rem;
    color: var(--text-muted);
}

.poll-vote-btn {
    margin-top: 14px;
    width: 100%;
    padding: 10px;
    border-radius: 9999px;
    font-weight: 700;
    font-size: 0.9rem;
}

.poll-footer {
    margin-top: 12px;
    padding-top: 10px;
    border-top: 1px solid var(--border);
    font-size: 0.75rem;
    color: var(--text-muted);
    display: flex;
    justify-content: space-between;
    flex-wrap: wrap;
    gap: 8px;
}

/* Rating */
.rating-results { display: flex; flex-direction: column; gap: 10px; }

.rating-row {
    display: flex;
    align-items: center;
    gap: 12px;
}

.rating-stars-label {
    width: 90px;
    color: #FFD700;
    letter-spacing: 2px;
    font-size: 0.85rem;
}

.rating-percent {
    width: 48px;
    text-align: right;
    font-size: 0.82rem;
    font-weight: 700;
    color: var(--text-primary);
}

.average-rating {
    margin-top: 14px;
    text-align: center;
    font-weight: 700;
    color: var(--orange);
    font-size: 0.9rem;
}

.rating-vote { text-align: center; }

.rating-stars-input {
    display: flex;
    justify-content: center;
    gap: 10px;
    margin-bottom: 16px;
}

.rating-star-select {
    font-size: 1.9rem;
    cursor: pointer;
    transition: transform 0.15s ease;
    color: var(--text-muted);
}

.rating-star-select:hover { transform: scale(1.15); }

.rating-star-select.selected,
.rating-star-select.fa-solid { color: #FFD700; }

.rating-media-container {
    margin-bottom: 16px;
    text-align: center;
    background: var(--bg-card);
    border-radius: 12px;
    padding: 8px;
}

.rating-media { cursor: pointer; text-align: center; }

.rating-media img,
.rating-media video {
    max-width: 100%;
    max-height: 250px;
    border-radius: 12px;
    object-fit: cover;
}

.rating-media-caption {
    font-size: 0.72rem;
    color: var(--text-muted);
    margin-top: 6px;
}

/* Duel */
.poll-duel-container { margin-top: 8px; }

.duel-vote {
    display: flex;
    gap: 12px;
    justify-content: center;
    margin-bottom: 16px;
    flex-wrap: wrap;
}

.duel-option-vote {
    flex: 1;
    min-width: 200px;
    text-align: center;
    padding: 14px;
    background: var(--bg-card);
    border-radius: 16px;
    cursor: pointer;
    transition: all 0.2s ease;
    border: 2px solid transparent;
}

.duel-option-vote:hover {
    transform: translateY(-3px);
    border-color: var(--orange);
}

.duel-option-vote.selected {
    border-color: var(--orange);
    background: rgba(255, 98, 0, 0.08);
}

.duel-vote-image {
    width: 100%;
    max-height: 150px;
    object-fit: cover;
    border-radius: 12px;
    margin-bottom: 10px;
}

.duel-option-text { font-weight: 600; font-size: 0.92rem; }

.duel-results {
    display: flex;
    gap: 12px;
    flex-wrap: wrap;
}

.duel-option-result {
    flex: 1;
    min-width: 200px;
    text-align: center;
    padding: 14px;
    background: var(--bg-card);
    border-radius: 14px;
}

.duel-result-image {
    width: 100%;
    max-height: 120px;
    object-fit: cover;
    border-radius: 10px;
    margin-bottom: 10px;
}

.duel-percentage {
    font-size: 1.5rem;
    font-weight: 800;
    color: var(--orange);
    margin: 8px 0;
}

.duel-votes {
    font-size: 0.72rem;
    color: var(--text-muted);
}

/* ===== SIDEBAR ===== */
.sidebar-right { padding: 16px; }

.widget {
    background: var(--bg-card);
    border-radius: 16px;
    padding: 18px;
    margin-bottom: 20px;
    border: 1px solid var(--border);
    transition: border-color 0.2s ease;
}

.widget:hover { border-color: rgba(255, 98, 0, 0.3); }

.widget-title {
    font-weight: 700;
    margin-bottom: 12px;
    font-size: 0.98rem;
    display: flex;
    align-items: center;
    gap: 8px;
}

/* ===== LOADER ===== */
.loader {
    text-align: center;
    padding: 60px 20px;
    color: var(--text-muted);
}

.loader i {
    color: var(--orange);
    opacity: 0.4;
    display: block;
    margin-bottom: 16px;
}

/* ===== RESPONSIVE ===== */
@media (max-width: 768px) {
    .profile-cover-wrap { height: 160px; }
    .profile-header { margin-top: -55px; padding: 0 16px 16px; }
    .profile-avatar { width: 100px; height: 100px; }
    .profile-actions { padding-top: 60px; width: 100%; }
    .profile-actions .btn { flex: 1; justify-content: center; }
    .profile-actions .btn-icon { flex: 0 0 40px; }
    .profile-name { font-size: 1.35rem; }
    .profile-bio { font-size: 0.9rem; }
    .profile-stats { gap: 14px; font-size: 0.85rem; }
    .bark-card { padding: 14px 16px; }
    .bark-avatar { width: 38px; height: 38px; }
    .bark-actions { gap: 2px; }
    .bark-action-btn { padding: 5px 8px; font-size: 0.75rem; }
    .bark-media.count-2,
    .bark-media.count-3,
    .bark-media.count-4 { grid-template-columns: repeat(2, 1fr); }
    .duel-vote { flex-direction: column; }
    .duel-option-vote { min-width: auto; }
    .duel-results { flex-direction: column; }
    .duel-option-result { min-width: auto; }
    .quoted-bark { padding: 10px; }
    .quoted-bark-media img,
    .quoted-bark-media video { max-width: 90px; max-height: 70px; }
    .rebark-indicator { padding-left: 50px; }
}

@media (max-width: 480px) {
    .profile-cover-wrap { height: 130px; }
    .profile-header { margin-top: -45px; }
    .profile-avatar { width: 80px; height: 80px; }
    .profile-actions { padding-top: 50px; }
    .profile-name { font-size: 1.15rem; }
    .profile-handle { font-size: 0.85rem; }
    .profile-stats { gap: 12px; }
    .profile-stat { font-size: 0.8rem; }
    .profile-stat span { font-size: 0.9rem; }
    .bark-header { gap: 10px; }
    .bark-avatar { width: 34px; height: 34px; }
    .bark-display-name { font-size: 0.85rem; }
    .bark-handle { font-size: 0.72rem; }
    .bark-time { font-size: 0.7rem; }
    .bark-content { font-size: 0.88rem; }
    .bark-action-btn { padding: 4px 6px; font-size: 0.72rem; }
    .bark-action-btn i { font-size: 0.85rem; }
    .bark-action-btn .action-count { font-size: 0.7rem; }
    .rating-stars-label { width: 65px; font-size: 0.72rem; }
    .rating-star-select { font-size: 1.3rem; }
    .duel-vote-image { max-height: 100px; }
    .duel-result-image { max-height: 80px; }
    .rebark-indicator { padding-left: 44px; font-size: 0.72rem; }
}
</style>

<main class="main-feed">
    <!-- ===== COVER PHOTO ===== -->
    <?php $cover = get_user_cover($profile); ?>
    <div class="profile-cover-wrap">
        <img src="<?= htmlspecialchars($cover) ?>" class="profile-cover" alt="<?= __('settings_cover_photo') ?>">
    </div>

    <div class="profile-header">
        <!-- Avatar + actions -->
        <div class="profile-actions-row">
            <div class="profile-avatar-wrap">
                <img src="<?= get_user_avatar($profile) ?>" class="profile-avatar" alt="avatar">
            </div>
            <div class="profile-actions">
                <?php if ($isOwnProfile): ?>
                    <a href="<?= SITE_URL ?>/pages/settings.php" class="btn btn-outline">
                        <i class="fa-solid fa-pen"></i> <?= __('profile_edit') ?>
                    </a>
                <?php else: ?>
                    <button class="btn <?= $isFollowing ? 'btn-outline following' : 'btn-primary' ?>"
                            onclick="toggleFollow(<?= $profile['id'] ?>, this)"
                            id="followBtn">
                        <?= $isFollowing ? __('btn_following') : __('btn_follow') ?>
                    </button>
                    <a href="<?= SITE_URL ?>/pages/messages.php?to=<?= $profile['id'] ?>"
                       class="btn btn-outline btn-icon"
                       title="<?= __('profile_message') ?? 'Message' ?>">
                        <i class="fa-solid fa-envelope"></i>
                    </a>
                <?php endif; ?>
            </div>
        </div>

        <!-- Identity -->
        <div class="profile-identity">
            <div class="profile-name">
                <?= htmlspecialchars($profile['display_name']) ?>
                <?php if ($profile['is_verified']): ?>
                    <span class="verified-badge" title="<?= __('profile_verified') ?>">✓</span>
                <?php endif; ?>
            </div>
            <div class="profile-handle">@<?= htmlspecialchars($profile['username']) ?></div>

            <?php if ($profile['bio']): ?>
                <div class="profile-bio"><?= nl2br(htmlspecialchars($profile['bio'])) ?></div>
            <?php endif; ?>

            <div class="profile-meta">
                <span class="profile-meta-item">
                    <i class="fa-regular fa-calendar"></i>
                    <?= __('profile_joined') ?> <?= date('F Y', strtotime($profile['created_at'])) ?>
                </span>

                <?php if ($isOwnProfile && (float)$stats['total_bones'] > 0): ?>
                    <span class="profile-bones-badge" style="display:none;visibility:hidden;">
                        <i class="fa-solid fa-bone"></i>
                        €<?= number_format((float)$stats['total_bones'], 2) ?> <?= __('profile_received') ?>
                    </span>
                <?php endif; ?>
            </div>
        </div>

        <!-- Stats -->
        <div class="profile-stats">
            <div class="profile-stat" onclick="location.href='<?= SITE_URL ?>/pages/follows.php?u=<?= urlencode($profile['username']) ?>&tab=barks'">
                <span><?= number_format($stats['barks']) ?></span> <?= __('profile_barks') ?>
            </div>
            <div class="profile-stat" onclick="location.href='<?= SITE_URL ?>/pages/follows.php?u=<?= urlencode($profile['username']) ?>&tab=followers'">
                <span><?= number_format($stats['followers']) ?></span> <?= __('btn_followers') ?>
            </div>
            <div class="profile-stat" onclick="location.href='<?= SITE_URL ?>/pages/follows.php?u=<?= urlencode($profile['username']) ?>&tab=following'">
                <span><?= number_format($stats['following']) ?></span> <?= __('btn_following') ?>
            </div>
        </div>
    </div>

    <!-- ===== FEED TABS ===== -->
    <div class="feed-tabs">
        <div class="feed-tab active"><?= __('profile_barks') ?></div>
    </div>

    <!-- ===== BARKS ===== -->
    <?php if (empty($barks)): ?>
        <div class="loader">
            <div style="font-size:3rem"><i class="fa-solid fa-paw"></i></div>
            <p><?= $isOwnProfile ? __('profile_you_no_barks') : __('profile_no_barks', ['name' => htmlspecialchars($profile['display_name'])]) ?></p>
        </div>
    <?php else: ?>
        <?php foreach ($barks as $bark):
            $media = $mediaMap[$bark['id']] ?? [];
            $mediaCount = count($media);
            $isOwn = $bark['user_id'] === $_SESSION['user_id'];
            
            $decodedContent = html_entity_decode($bark['content'], ENT_QUOTES, 'UTF-8');
            $previewAuthorLabel = $bark['is_anonymous']
                ? '<i class="fa-solid fa-glasses"></i> ' . __('bark_anonymous')
                : htmlspecialchars($profile['display_name']);
            $previewHtml = '<strong>' . $previewAuthorLabel . '</strong>: ' .
                           htmlspecialchars(substr($decodedContent, 0, 100)) .
                           (strlen($decodedContent) > 100 ? '...' : '');
            
            $original = null;
            if (in_array($bark['bark_type'], ['rebark', 'howl']) && $bark['original_bark_id']) {
                $original = $originalBarks[$bark['original_bark_id']] ?? null;
            }
            
            $pollData = $bark['poll_id'] ? ($pollsData[$bark['poll_id']] ?? null) : null;
            
            $barkUsername = $bark['username'] ?? $profile['username'];
            $barkDisplayName = $bark['display_name'] ?? $profile['display_name'];
            $barkIsVerified = $bark['is_verified'] ?? $profile['is_verified'];
        ?>
        <article class="bark-card" data-bark-id="<?= $bark['id'] ?>">
            
            <!-- Rebark / Howl indicator -->
            <?php if ($bark['bark_type'] === 'rebark' && $original): ?>
                <div class="rebark-indicator">
                    <i class="fa-solid fa-retweet"></i>
                    <?php if ($bark['is_anonymous']): ?>
                        <i class="fa-solid fa-glasses"></i> <?= __('bark_anonymous') ?> <?= __('action_rebark') ?>ed
                    <?php else: ?>
                        <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($barkUsername) ?>">
                            @<?= htmlspecialchars($barkUsername) ?>
                        </a> <?= __('action_rebark') ?>ed
                    <?php endif; ?>
                </div>
            <?php elseif ($bark['bark_type'] === 'howl' && $original): ?>
                <div class="rebark-indicator" style="color: #9B59B6">
                    <i class="fa-solid fa-retweet"></i>
                    <?php if ($bark['is_anonymous']): ?>
                        <i class="fa-solid fa-glasses"></i> <?= __('bark_anonymous') ?> <?= __('action_howl') ?>ed
                    <?php else: ?>
                        <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($barkUsername) ?>">
                            @<?= htmlspecialchars($barkUsername) ?>
                        </a> <?= __('action_howl') ?>ed
                    <?php endif; ?>
                </div>
            <?php endif; ?>
            
            <div class="bark-header">
                <?php if (!$bark['is_anonymous']): ?>
                    <img src="<?= get_user_avatar($profile) ?>" class="bark-avatar" alt="avatar">
                <?php else: ?>
                    <div class="bark-avatar" style="background:var(--bg-card2);display:flex;align-items:center;justify-content:center;font-size:1.2rem;color:var(--text-muted)">
                        <i class="fa-solid fa-glasses"></i>
                    </div>
                <?php endif; ?>
                <div class="bark-user-info">
                    <div class="bark-display-name">
                        <?php if ($bark['is_anonymous']): ?>
                            <span><i class="fa-solid fa-glasses"></i> <?= __('bark_anonymous') ?></span>
                        <?php else: ?>
                            <?= htmlspecialchars($profile['display_name']) ?>
                            <?php if ($profile['is_verified']): ?><span class="verified-badge">✓</span><?php endif; ?>
                        <?php endif; ?>
                    </div>
                    <?php if (!$bark['is_anonymous']): ?>
                        <div class="bark-handle">@<?= htmlspecialchars($profile['username']) ?></div>
                    <?php endif; ?>
                </div>
                <span class="bark-time"><?= time_ago($bark['created_at']) ?></span>
                <div class="bark-more">
                    <button class="bark-more-btn" data-dropdown="dd-<?= $bark['id'] ?>">
                        <i class="fa-solid fa-ellipsis"></i>
                    </button>
                    <div class="dropdown" id="dd-<?= $bark['id'] ?>">
                        <?php if ($isOwn): ?>
                            <div class="dropdown-item" onclick="openEditBarkModal(<?= $bark['id'] ?>, '<?= addslashes($decodedContent) ?>')">
                                <i class="fa-solid fa-pen"></i> <?= __('action_edit') ?>
                            </div>
                            <div class="dropdown-item danger" onclick="deleteBark(<?= $bark['id'] ?>)">
                                <i class="fa-solid fa-trash"></i> <?= __('action_delete') ?>
                            </div>
                        <?php else: ?>
                            <div class="dropdown-item" onclick="openReport(<?= $bark['id'] ?>)">
                                <i class="fa-solid fa-flag"></i> <?= __('action_report') ?>
                            </div>
                        <?php endif; ?>
                        <div class="dropdown-item" onclick="toggleBury(<?= $bark['id'] ?>, this)">
                            <i class="fa-solid fa-bookmark"></i> <?= $bark['user_buried'] ? __('action_remove_bury') : __('action_bury') ?>
                        </div>
                        <div class="dropdown-item" onclick="navigator.clipboard.writeText('<?= SITE_URL ?>/pages/bark.php?id=<?= $bark['id'] ?>').then(()=>showToast('<?= __('action_link_copied') ?>','success'))">
                            <i class="fa-solid fa-link"></i> <?= __('action_copy_link') ?>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Howl content -->
            <?php if ($bark['bark_type'] === 'howl' && !empty($bark['content'])): ?>
                <div class="bark-content" style="margin-bottom: 12px;">
                    <?= display_bark_content($bark['content']) ?>
                </div>
            <?php endif; ?>

            <!-- Quoted bark -->
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
                            <div class="quoted-bark-avatar" style="background:var(--bg-card2);display:flex;align-items:center;justify-content:center;font-size:0.7rem;flex-shrink:0">
                                <i class="fa-solid fa-glasses"></i>
                            </div>
                            <div>
                                <span class="quoted-bark-name"><i class="fa-solid fa-glasses"></i> <?= __('bark_anonymous') ?></span>
                            </div>
                        <?php endif; ?>
                        <span style="margin-left:auto; font-size:0.72rem; color:var(--text-muted)">
                            <?= time_ago($original['created_at']) ?>
                        </span>
                    </div>
                    <div class="quoted-bark-content">
                        <?= display_bark_content($original['content']) ?>
                    </div>
                    <?php if ($origMediaCount > 0): ?>
                        <div class="quoted-bark-media count-<?= min($origMediaCount, 2) ?>">
                            <?php foreach (array_slice($origMedia, 0, 2) as $m): ?>
                                <?php if ($m['type'] === 'image'): ?>
                                    <img src="<?= getImageUrlWithCacheBust($m['url'], 'media') ?>" 
                                         alt="media" loading="lazy"
                                         onclick="event.stopPropagation(); openMediaViewer('<?= SITE_URL ?>/assets/uploads/media/<?= htmlspecialchars($m['url']) ?>')">
                                <?php else: ?>
                                    <video src="<?= SITE_URL ?>/assets/uploads/media/<?= htmlspecialchars($m['url']) ?>" 
                                           controls preload="none"
                                           poster="<?= SITE_URL ?>/assets/img/video-placeholder.jpg"
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

            <!-- Bark media -->
            <?php if ($bark['bark_type'] === 'normal' && $mediaCount > 0): ?>
                <div class="bark-media count-<?= min($mediaCount,4) ?>">
                    <?php foreach (array_slice($media,0,4) as $m): ?>
                        <div class="media-item">
                            <?php if ($m['type']==='image'): ?>
                                <img src="<?= getImageUrlWithCacheBust($m['url'], 'media') ?>" 
                                     alt="media" loading="lazy"
                                     onclick="openMediaViewer('<?= SITE_URL ?>/assets/uploads/media/<?= htmlspecialchars($m['url']) ?>')">
                            <?php else: ?>
                                <video src="<?= SITE_URL ?>/assets/uploads/media/<?= htmlspecialchars($m['url']) ?>" 
                                       controls preload="none"
                                       poster="<?= SITE_URL ?>/assets/img/video-placeholder.jpg"></video>
                            <?php endif; ?>
                        </div>
                    <?php endforeach; ?>
                </div>
            <?php endif; ?>

            <!-- Poll -->
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
                                                   controls preload="metadata" poster="<?= SITE_URL ?>/assets/img/video-placeholder.jpg"
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
                            <div class="poll-duel-container">
                                <?php 
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
                                                   value="<?= $opt['id'] ?>"
                                                   data-poll-id="<?= $poll['id'] ?>">
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
                    <button class="bark-action-btn paw <?= $bark['user_pawed'] ? 'active' : '' ?>" onclick="togglePaw(<?= $bark['id'] ?>, this)">
                        <i class="fa-solid fa-paw"></i><span class="action-count"><?= $bark['paw_count'] ?></span>
                    </button>
                <?php else: ?>
                    <button class="bark-action-btn paw" disabled>
                        <i class="fa-solid fa-paw"></i><span class="action-count"><?= $bark['paw_count'] ?></span>
                    </button>
                <?php endif; ?>

                <button class="bark-action-btn yap" onclick="openYap(<?= $bark['id'] ?>,`<?= addslashes($previewHtml) ?>`)">
                    <i class="fa-solid fa-comment"></i><span class="action-count"><?= $bark['yap_count'] ?></span>
                </button>

                <?php if (!$isOwn): ?>
                    <button class="bark-action-btn rebark <?= $bark['user_rebarked']?'active':'' ?>" onclick="toggleRebark(<?= $bark['id'] ?>,this)">
                        <i class="fa-solid fa-retweet"></i><span class="action-count"><?= $bark['rebark_count'] ?></span>
                    </button>
                <?php else: ?>
                    <button class="bark-action-btn rebark" disabled>
                        <i class="fa-solid fa-retweet"></i><span class="action-count"><?= $bark['rebark_count'] ?></span>
                    </button>
                <?php endif; ?>

                <button class="bark-action-btn howl" onclick="openHowl(<?= $bark['id'] ?>,`<?= addslashes($previewHtml) ?>`)">
                    <i class="fa-solid fa-wifi"></i><span class="action-count"></span>
                </button>

                <button class="bark-action-btn bury <?= $bark['user_buried'] ? 'active' : '' ?>" onclick="toggleBury(<?= $bark['id'] ?>, this)" title="<?= __('action_bury') ?>">
                    <i class="fa-solid fa-bookmark"></i>
                </button>

                <?php if (!$isOwn): ?>
                    <button class="bark-action-btn bone"
                        onclick="openBoneModal(<?= $bark['id'] ?>,<?= $profile['id'] ?>,'<?= addslashes($profile['display_name']) ?>')"
                        title="<?= __('action_bone') ?>">
                        <i class="fa-solid fa-bone"></i>
                    </button>
                <?php endif; ?>

                <a href="<?= SITE_URL ?>/pages/bark.php?id=<?= $bark['id'] ?>" class="bark-action-btn" style="margin-left:auto;text-decoration:none" title="<?= __('action_view_bark') ?>">
                    <i class="fa-solid fa-arrow-up-right-from-square"></i>
                </a>
            </div>
        </article>
        <?php endforeach; ?>
    <?php endif; ?>
</main>

<aside class="sidebar-right" style="visibility:hidden;">
    <!-- Quick bone -->
    <?php if (!$isOwnProfile): ?>
        <div class="widget">
            <!--div class="widget-title">
                <i class="fa-solid fa-bone"></i>
                <?= __('profile_support', ['name' => htmlspecialchars($profile['display_name'])]) ?>
            </div-->
            <!--p style="font-size:0.85rem;color:var(--text-secondary);margin-bottom:12px">
                <?= __('profile_send_bone') ?>
		</p-->
            <button class="btn btn-primary btn-full" onclick="openBoneModal(0, <?= $profile['id'] ?>, '<?= addslashes($profile['display_name']) ?>')">
                <i class="fa-solid fa-bone"></i> <?= __('action_bone') ?>
            </button>
        </div>
    <?php endif; ?>
    
    <!-- Private section -->
    <?php if ($isOwnProfile): ?>
        <div class="widget">
            <div class="widget-title">🔒 <?= __('profile_your_bones') ?></div>
            <div style="padding:12px 0">
                <div style="display:flex; align-items:center; justify-content:space-between; margin-bottom:12px">
                    <span style="color:var(--text-secondary)"><?= __('profile_total_received') ?>:</span>
                    <span style="font-size:1.5rem; font-weight:700; color:var(--orange)">
                        €<?= number_format((float)$stats['total_bones'], 2) ?>
                    </span>
                </div>
                <p style="font-size:0.8rem; color:var(--text-muted); margin-top:8px">
                    <i class="fa-solid fa-lock"></i> <?= __('profile_only_you_see') ?>
                </p>
            </div>
        </div>
    <?php endif; ?>
</aside>

<script>
function openMediaViewer(src) {
    const overlay = document.createElement('div');
    overlay.style.cssText = `position:fixed;inset:0;background:rgba(0,0,0,0.92);z-index:9999;display:flex;align-items:center;justify-content:center;cursor:pointer`;
    const img = document.createElement('img');
    img.src = src;
    img.style.cssText = 'max-width:90vw;max-height:90vh;object-fit:contain;border-radius:8px';
    overlay.appendChild(img);
    overlay.onclick = () => overlay.remove();
    document.body.appendChild(overlay);
}

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
            headers: { 'Cache-Control': 'no-cache' }
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
            headers: { 'Cache-Control': 'no-cache' }
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
            headers: { 'Cache-Control': 'no-cache' }
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

<?php include __DIR__ . '/../includes/footer.php'; ?>