<?php
// ============================================================
// BARKER — People Page (Explorer les utilisateurs)
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

$currentUser = current_user();
$tab = $_GET['tab'] ?? 'active';
$page = max(1, (int)($_GET['page'] ?? 1));
$limit = 20;
$offset = ($page - 1) * $limit;
$searchQuery = trim($_GET['q'] ?? '');

// ============================================================
// FONCTIONS
// ============================================================

function getUserStats($userId) {
    return DB::fetchOne("
        SELECT
            (SELECT COUNT(*) FROM barks WHERE user_id = ? AND is_deleted = 0) as barks,
            (SELECT COUNT(*) FROM follows WHERE following_id = ?) as followers,
            (SELECT COUNT(*) FROM follows WHERE follower_id = ?) as following,
            (SELECT COUNT(*) FROM paws WHERE user_id = ?) as paws_received,
            (SELECT COALESCE(SUM(amount),0) FROM bones WHERE receiver_id = ? AND status = 'completed') as bones_received
    ", [$userId, $userId, $userId, $userId, $userId]);
}

function isFollowing($followerId, $followingId) {
    return (bool)DB::fetchOne(
        "SELECT 1 FROM follows WHERE follower_id = ? AND following_id = ?",
        [$followerId, $followingId]
    );
}

// ============================================================
// RÉCUPÉRATION DES UTILISATEURS
// ============================================================

$users = [];
$totalUsers = 0;
$title = '';

if ($tab === 'active') {
    $title = __('people_active_users');
    // Utilisateurs les plus actifs (nombre de barks récents)
    $users = DB::fetchAll("
        SELECT u.*,
               (SELECT COUNT(*) FROM barks WHERE user_id = u.id AND is_deleted = 0 AND created_at > DATE_SUB(NOW(), INTERVAL 7 DAY)) as recent_barks,
               (SELECT COUNT(*) FROM follows WHERE following_id = u.id) as follower_count,
               (SELECT COUNT(*) FROM follows WHERE follower_id = u.id) as following_count,
               (SELECT COUNT(*) FROM barks WHERE user_id = u.id AND is_deleted = 0) as total_barks,
               (SELECT COUNT(*) FROM paws WHERE bark_id IN (SELECT id FROM barks WHERE user_id = u.id AND is_deleted = 0)) as paws_received,
               (SELECT COUNT(*) FROM follows WHERE follower_id = ? AND following_id = u.id) as is_following
        FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
        HAVING recent_barks > 0
        ORDER BY recent_barks DESC, follower_count DESC
        LIMIT ? OFFSET ?
    ", [$_SESSION['user_id'], $_SESSION['user_id'], $limit, $offset]);
    
    $totalUsers = DB::fetchOne("
        SELECT COUNT(*) as total FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
          AND (SELECT COUNT(*) FROM barks WHERE user_id = u.id AND is_deleted = 0 AND created_at > DATE_SUB(NOW(), INTERVAL 7 DAY)) > 0
    ", [$_SESSION['user_id']]);
    $totalUsers = (int)($totalUsers['total'] ?? 0);
    
} elseif ($tab === 'popular') {
    $title = __('people_popular_users');
    // Utilisateurs avec le plus de followers
    $users = DB::fetchAll("
        SELECT u.*,
               (SELECT COUNT(*) FROM follows WHERE following_id = u.id) as follower_count,
               (SELECT COUNT(*) FROM follows WHERE follower_id = u.id) as following_count,
               (SELECT COUNT(*) FROM barks WHERE user_id = u.id AND is_deleted = 0) as total_barks,
               (SELECT COUNT(*) FROM paws WHERE bark_id IN (SELECT id FROM barks WHERE user_id = u.id AND is_deleted = 0)) as paws_received,
               (SELECT COUNT(*) FROM follows WHERE follower_id = ? AND following_id = u.id) as is_following
        FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
        ORDER BY follower_count DESC
        LIMIT ? OFFSET ?
    ", [$_SESSION['user_id'], $_SESSION['user_id'], $limit, $offset]);
    
    $totalUsers = DB::fetchOne("
        SELECT COUNT(*) as total FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
    ", [$_SESSION['user_id']]);
    $totalUsers = (int)($totalUsers['total'] ?? 0);
    
} elseif ($tab === 'new') {
    $title = __('people_new_users');
    // Nouveaux utilisateurs
    $users = DB::fetchAll("
        SELECT u.*,
               (SELECT COUNT(*) FROM follows WHERE following_id = u.id) as follower_count,
               (SELECT COUNT(*) FROM follows WHERE follower_id = u.id) as following_count,
               (SELECT COUNT(*) FROM barks WHERE user_id = u.id AND is_deleted = 0) as total_barks,
               (SELECT COUNT(*) FROM paws WHERE bark_id IN (SELECT id FROM barks WHERE user_id = u.id AND is_deleted = 0)) as paws_received,
               (SELECT COUNT(*) FROM follows WHERE follower_id = ? AND following_id = u.id) as is_following
        FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
        ORDER BY u.created_at DESC
        LIMIT ? OFFSET ?
    ", [$_SESSION['user_id'], $_SESSION['user_id'], $limit, $offset]);
    
    $totalUsers = DB::fetchOne("
        SELECT COUNT(*) as total FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
    ", [$_SESSION['user_id']]);
    $totalUsers = (int)($totalUsers['total'] ?? 0);
    
} elseif ($tab === 'top_bones') {
    $title = __('people_top_bones');
    // Utilisateurs avec le plus de bones reçus
    $users = DB::fetchAll("
        SELECT u.*,
               (SELECT COALESCE(SUM(amount),0) FROM bones WHERE receiver_id = u.id AND status = 'completed') as bones_received,
               (SELECT COUNT(*) FROM follows WHERE following_id = u.id) as follower_count,
               (SELECT COUNT(*) FROM follows WHERE follower_id = u.id) as following_count,
               (SELECT COUNT(*) FROM barks WHERE user_id = u.id AND is_deleted = 0) as total_barks,
               (SELECT COUNT(*) FROM paws WHERE bark_id IN (SELECT id FROM barks WHERE user_id = u.id AND is_deleted = 0)) as paws_received,
               (SELECT COUNT(*) FROM follows WHERE follower_id = ? AND following_id = u.id) as is_following
        FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
        HAVING bones_received > 0
        ORDER BY bones_received DESC
        LIMIT ? OFFSET ?
    ", [$_SESSION['user_id'], $_SESSION['user_id'], $limit, $offset]);
    
    $totalUsers = DB::fetchOne("
        SELECT COUNT(*) as total FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
          AND (SELECT COALESCE(SUM(amount),0) FROM bones WHERE receiver_id = u.id AND status = 'completed') > 0
    ", [$_SESSION['user_id']]);
    $totalUsers = (int)($totalUsers['total'] ?? 0);
    
} elseif ($tab === 'search' && !empty($searchQuery)) {
    $title = __('people_search_results', ['query' => htmlspecialchars($searchQuery)]);
    $like = '%' . $searchQuery . '%';
    $users = DB::fetchAll("
        SELECT u.*,
               (SELECT COUNT(*) FROM follows WHERE following_id = u.id) as follower_count,
               (SELECT COUNT(*) FROM follows WHERE follower_id = u.id) as following_count,
               (SELECT COUNT(*) FROM barks WHERE user_id = u.id AND is_deleted = 0) as total_barks,
               (SELECT COUNT(*) FROM paws WHERE bark_id IN (SELECT id FROM barks WHERE user_id = u.id AND is_deleted = 0)) as paws_received,
               (SELECT COUNT(*) FROM follows WHERE follower_id = ? AND following_id = u.id) as is_following
        FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
          AND (u.username LIKE ? OR u.display_name LIKE ? OR u.bio LIKE ?)
        ORDER BY follower_count DESC
        LIMIT ? OFFSET ?
    ", [$_SESSION['user_id'], $_SESSION['user_id'], $like, $like, $like, $limit, $offset]);
    
    $totalUsers = DB::fetchOne("
        SELECT COUNT(*) as total FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
          AND (u.username LIKE ? OR u.display_name LIKE ? OR u.bio LIKE ?)
    ", [$_SESSION['user_id'], $like, $like, $like]);
    $totalUsers = (int)($totalUsers['total'] ?? 0);

}

$pageTitle = __('nav_people');
include __DIR__ . '/../includes/header.php';
?>

<style>
/* People Page Styles */
.people-header {
    padding: 16px;
    border-bottom: 1px solid var(--border);
    position: sticky;
    top: 0;
    background: var(--bg);
    z-index: 10;
}

.people-header-title {
    font-size: 1.5rem;
    font-weight: 700;
    display: flex;
    align-items: center;
    gap: 10px;
}

.people-search-box {
    display: flex;
    align-items: center;
    background: var(--bg-card2);
    border-radius: 40px;
    padding: 8px 16px;
    border: 1px solid var(--border);
    transition: all 0.2s;
    margin-top: 12px;
}

.people-search-box:focus-within {
    border-color: var(--orange);
    box-shadow: 0 0 0 3px var(--orange-faint);
}

.people-search-box i {
    color: var(--text-muted);
    margin-right: 10px;
}

.people-search-box input {
    flex: 1;
    background: transparent;
    border: none;
    color: var(--text-primary);
    font-size: 0.95rem;
    outline: none;
}

.people-search-box input::placeholder {
    color: var(--text-muted);
}

.feed-tabs {
    display: flex;
    border-bottom: 1px solid var(--border);
    overflow-x: auto;
    scrollbar-width: none;
    -webkit-overflow-scrolling: touch;
}

.feed-tabs::-webkit-scrollbar {
    display: none;
}

.feed-tab {
    flex: 0 0 auto;
    text-align: center;
    padding: 12px 20px;
    font-weight: 600;
    color: var(--text-secondary);
    transition: all 0.2s;
    text-decoration: none;
    font-size: 0.9rem;
    white-space: nowrap;
}

.feed-tab.active {
    color: var(--orange);
    border-bottom: 2px solid var(--orange);
}

.feed-tab:hover {
    color: var(--text-primary);
    background: var(--bg-card2);
}

.user-card {
    display: flex;
    align-items: center;
    gap: 16px;
    padding: 16px 20px;
    border-bottom: 1px solid var(--border);
    transition: background 0.2s;
    text-decoration: none;
    color: var(--text-primary);
}

.user-card:hover {
    background: var(--bg-card2);
}

.user-card-avatar {
    width: 56px;
    height: 56px;
    border-radius: 50%;
    object-fit: cover;
    border: 2px solid var(--border);
    transition: border-color 0.2s;
    flex-shrink: 0;
}

.user-card:hover .user-card-avatar {
    border-color: var(--orange);
}

.user-card-info {
    flex: 1;
    min-width: 0;
}

.user-card-name {
    font-weight: 700;
    font-size: 1rem;
    display: flex;
    align-items: center;
    gap: 6px;
    flex-wrap: wrap;
}

.user-card-name .verified-badge {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    background: var(--orange);
    color: white;
    border-radius: 50%;
    width: 18px;
    height: 18px;
    font-size: 0.65rem;
}

.user-card-handle {
    font-size: 0.8rem;
    color: var(--text-muted);
}

.user-card-bio {
    font-size: 0.85rem;
    color: var(--text-secondary);
    margin-top: 4px;
    display: -webkit-box;
    -webkit-line-clamp: 2;
    -webkit-box-orient: vertical;
    overflow: hidden;
    line-height: 1.4;
}

.user-card-stats {
    display: flex;
    gap: 16px;
    margin-top: 6px;
    font-size: 0.75rem;
    color: var(--text-muted);
    flex-wrap: wrap;
}

.user-card-stats span {
    display: flex;
    align-items: center;
    gap: 4px;
}

.user-card-stats .stat-value {
    font-weight: 700;
    color: var(--text-primary);
}

.user-card-stats .stat-label {
    color: var(--text-muted);
}

.user-card-actions {
    flex-shrink: 0;
    display: flex;
    gap: 8px;
    align-items: center;
}

.follow-btn {
    padding: 6px 18px;
    border-radius: 30px;
    border: none;
    font-weight: 700;
    font-size: 0.8rem;
    cursor: pointer;
    transition: all 0.2s;
    background: var(--orange);
    color: white;
}

.follow-btn:hover {
    transform: scale(1.05);
    opacity: 0.9;
}

.follow-btn.following {
    background: transparent;
    border: 1px solid var(--border);
    color: var(--text-primary);
}

.follow-btn.following:hover {
    background: var(--bg-card2);
    border-color: var(--orange);
    color: var(--orange);
}

.user-card-rank {
    font-size: 0.7rem;
    font-weight: 700;
    color: var(--text-muted);
    min-width: 24px;
    text-align: center;
}

.bones-badge {
    background: var(--orange-faint);
    color: var(--orange);
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 0.7rem;
    font-weight: 600;
}

.empty-state {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 60px 20px;
    color: var(--text-muted);
    text-align: center;
}

.empty-state-icon {
    font-size: 4rem;
    margin-bottom: 16px;
    opacity: 0.4;
}

.empty-state h3 {
    font-weight: 700;
    color: var(--text-primary);
    margin-bottom: 8px;
    font-size: 1.2rem;
}

.empty-state p {
    font-size: 0.95rem;
}

.pagination {
    display: flex;
    justify-content: center;
    gap: 8px;
    padding: 20px;
    flex-wrap: wrap;
}

.pagination a {
    padding: 8px 16px;
    border-radius: 20px;
    border: 1px solid var(--border);
    color: var(--text-primary);
    text-decoration: none;
    font-size: 0.85rem;
    font-weight: 600;
    transition: all 0.2s;
}

.pagination a:hover {
    background: var(--bg-card2);
    border-color: var(--orange);
}

.pagination a.active {
    background: var(--orange);
    color: white;
    border-color: var(--orange);
}

.pagination a.disabled {
    opacity: 0.4;
    cursor: not-allowed;
    pointer-events: none;
}

.user-card-bones {
    background: var(--orange-faint);
    padding: 2px 10px;
    border-radius: 20px;
    font-size: 0.75rem;
    font-weight: 700;
    color: var(--orange);
    white-space: nowrap;
}

/* Responsive */
@media (max-width: 768px) {
    .user-card {
        padding: 12px 16px;
        gap: 12px;
    }
    
    .user-card-avatar {
        width: 48px;
        height: 48px;
    }
    
    .user-card-name {
        font-size: 0.9rem;
    }
    
    .user-card-stats {
        gap: 10px;
        font-size: 0.7rem;
    }
    
    .follow-btn {
        padding: 4px 12px;
        font-size: 0.7rem;
    }
    
    .feed-tab {
        padding: 10px 16px;
        font-size: 0.8rem;
    }
}

@media (max-width: 480px) {
    .user-card {
        flex-wrap: wrap;
        padding: 12px;
    }
    
    .user-card-avatar {
        width: 44px;
        height: 44px;
    }
    
    .user-card-info {
        flex: 1 1 100%;
        order: 2;
    }
    
    .user-card-actions {
        order: 3;
        width: 100%;
        justify-content: flex-start;
    }
    
    .user-card-rank {
        order: 0;
    }
    
    .user-card-stats {
        gap: 8px;
    }
}
</style>

<main class="main-feed">
    <div class="people-header">
        <div class="people-header-title">
            <i class="fa-solid fa-users" style="color: var(--orange);"></i>
            <?= __('nav_people') ?>
        </div>
        
        <form method="GET" action="" class="people-search-box">
            <i class="fa-solid fa-magnifying-glass"></i>
            <input type="text" name="q" placeholder="<?= __('people_search_placeholder') ?>" value="<?= htmlspecialchars($searchQuery) ?>">
            <input type="hidden" name="tab" value="search">
            <button type="submit" style="background:transparent;border:none;color:var(--orange);cursor:pointer;font-weight:700;padding:4px 12px;display:<?= !empty($searchQuery) ? 'block' : 'none' ?>">
                <i class="fa-solid fa-arrow-right"></i>
            </button>
        </form>
    </div>

    <!-- Tabs -->
    <div class="feed-tabs">
        <a href="?tab=active" class="feed-tab <?= $tab === 'active' ? 'active' : '' ?>">
            <i class="fa-solid fa-fire"></i> <?= __('people_active') ?>
        </a>
        <a href="?tab=popular" class="feed-tab <?= $tab === 'popular' ? 'active' : '' ?>">
            <i class="fa-solid fa-star"></i> <?= __('people_popular') ?>
        </a>
        <a href="?tab=new" class="feed-tab <?= $tab === 'new' ? 'active' : '' ?>">
            <i class="fa-regular fa-clock"></i> <?= __('people_new') ?>
        </a>
        <a href="?tab=top_bones" class="feed-tab <?= $tab === 'top_bones' ? 'active' : '' ?>">
            <i class="fa-solid fa-bone"></i> <?= __('people_top_bones') ?>
        </a>
    </div>

    <!-- Title -->
    <div style="padding: 12px 20px; border-bottom: 1px solid var(--border); font-size: 0.85rem; color: var(--text-muted);">
        <?php if ($tab === 'search' && !empty($searchQuery)): ?>
            <?= __('people_results_for', ['count' => $totalUsers, 'query' => htmlspecialchars($searchQuery)]) ?>
        <?php else: ?>
            <?= $totalUsers ?> <?= __('people_users_found') ?>
        <?php endif; ?>
    </div>

    <!-- Users list -->
    <?php if (empty($users)): ?>
        <div class="empty-state">
            <div class="empty-state-icon">
                <?php if ($tab === 'search' && !empty($searchQuery)): ?>
                    <i class="fa-solid fa-user-slash"></i>
                <?php else: ?>
                    <i class="fa-regular fa-users"></i>
                <?php endif; ?>
            </div>
            <h3>
                <?php if ($tab === 'search' && !empty($searchQuery)): ?>
                    <?= __('people_no_results', ['query' => htmlspecialchars($searchQuery)]) ?>
                <?php else: ?>
                    <?= __('people_no_users_found') ?>
                <?php endif; ?>
            </h3>
            <p>
                <?php if ($tab === 'search' && !empty($searchQuery)): ?>
                    <?= __('people_try_different_search') ?>
                <?php elseif ($tab === 'active'): ?>
                    <?= __('people_no_active_users') ?>
                <?php elseif ($tab === 'top_bones'): ?>
                    <?= __('people_no_bones_yet') ?>
                <?php else: ?>
                    <?= __('people_check_back_later') ?>
                <?php endif; ?>
            </p>
            <?php if ($tab === 'search' && !empty($searchQuery)): ?>
                <a href="?tab=active" class="btn btn-primary" style="margin-top:16px">
                    <i class="fa-solid fa-arrow-left"></i> <?= __('people_back_to_active') ?>
                </a>
            <?php endif; ?>
        </div>
    <?php else: ?>
        <?php 
        $rank = $offset + 1;
        foreach ($users as $user):
            $followStatus = isFollowing($_SESSION['user_id'], $user['id']);
            $avatarUrl = get_user_avatar($user);
            $stats = getUserStats($user['id']);
            
            $bonesReceived = $stats['bones_received'] ?? 0;
            $pawsReceived = $stats['paws_received'] ?? 0;
            $totalFollowers = $user['follower_count'] ?? 0;
            $totalBarks = $user['total_barks'] ?? 0;
            
            // Déterminer la couleur de rang
            $rankColor = '';
            $rankIcon = '';
            if ($rank <= 3) {
                $rankColor = '#FFD700';
                $rankIcon = '🥇';
            } elseif ($rank <= 10) {
                $rankColor = '#C0C0C0';
                $rankIcon = '🥈';
            } elseif ($rank <= 25) {
                $rankColor = '#CD7F32';
                $rankIcon = '🥉';
            }
        ?>
        <div class="user-card" data-user-id="<?= $user['id'] ?>">
            <?php if ($tab === 'popular' || $tab === 'active'): ?>
            <div class="user-card-rank" style="color: <?= $rankColor ?>; font-size: <?= $rank <= 3 ? '1.2rem' : '0.8rem' ?>">
                <?= $rank <= 3 ? $rankIcon : '#' . $rank ?>
            </div>
            <?php endif; ?>
            
            <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($user['username']) ?>" style="flex-shrink:0;">
                <img src="<?= $avatarUrl ?>" class="user-card-avatar" alt="">
            </a>
            
            <div class="user-card-info">
                <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($user['username']) ?>" style="text-decoration:none;color:var(--text-primary);">
                    <div class="user-card-name">
                        <?= htmlspecialchars($user['display_name']) ?>
                        <?php if ($user['is_verified']): ?>
                            <span class="verified-badge" title="<?= __('profile_verified') ?>">✓</span>
                        <?php endif; ?>
                        <?php if ((float)$bonesReceived > 0): ?>
                            <span class="user-card-bones">
                                <i class="fa-solid fa-bone"></i> €<?= number_format((float)$bonesReceived, 2) ?>
                            </span>
                        <?php endif; ?>
                    </div>
                    <div class="user-card-handle">@<?= htmlspecialchars($user['username']) ?></div>
                </a>
                
                <?php if (!empty($user['bio'])): ?>
                <div class="user-card-bio"><?= htmlspecialchars($user['bio']) ?></div>
                <?php endif; ?>
                
                <div class="user-card-stats">
                    <span>
                        <i class="fa-solid fa-message"></i>
                        <span class="stat-value"><?= number_format($totalBarks) ?></span>
                        <span class="stat-label"><?= __('profile_barks') ?></span>
                    </span>
                    <span>
                        <i class="fa-solid fa-users"></i>
                        <span class="stat-value"><?= number_format($totalFollowers) ?></span>
                        <span class="stat-label"><?= __('btn_followers') ?></span>
                    </span>
                    <span>
                        <i class="fa-solid fa-paw"></i>
                        <span class="stat-value"><?= number_format($pawsReceived) ?></span>
                        <span class="stat-label"><?= __('people_paws_received') ?></span>
                    </span>
                    <?php if ($tab === 'active' && isset($user['recent_barks'])): ?>
                    <span>
                        <i class="fa-solid fa-fire" style="color: var(--orange);"></i>
                        <span class="stat-value"><?= $user['recent_barks'] ?></span>
                        <span class="stat-label"><?= __('people_this_week') ?></span>
                    </span>
                    <?php endif; ?>
                </div>
            </div>
            
            <div class="user-card-actions">
                <?php if ($user['id'] !== $_SESSION['user_id']): ?>
                    <button class="follow-btn <?= $followStatus ? 'following' : '' ?>" 
                            onclick="toggleFollow(<?= $user['id'] ?>, this)">
                        <?= $followStatus ? __('btn_following') : __('btn_follow') ?>
                    </button>
                    <button class="follow-btn" style="background:transparent;border:1px solid var(--border);color:var(--text-muted);padding:6px 10px;" 
                            onclick="openBoneModal(0, <?= $user['id'] ?>, '<?= addslashes($user['display_name']) ?>')"
                            title="<?= __('action_bone') ?>">
                        <i class="fa-solid fa-bone"></i>
                    </button>
                <?php else: ?>
                    <span style="font-size:0.75rem;color:var(--text-muted);padding:6px 12px;border:1px dashed var(--border);border-radius:20px;">
                        <i class="fa-regular fa-user"></i> <?= __('people_you') ?>
                    </span>
                <?php endif; ?>
            </div>
        </div>
        <?php 
        $rank++;
        endforeach; 
        ?>
        
        <!-- Pagination -->
        <?php if ($totalUsers > $limit): 
            $totalPages = ceil($totalUsers / $limit);
            $currentPage = $page;
        ?>
        <div class="pagination">
            <?php if ($currentPage > 1): ?>
                <a href="?tab=<?= $tab ?>&page=<?= $currentPage - 1 ?><?= !empty($searchQuery) ? '&q=' . urlencode($searchQuery) : '' ?>">
                    <i class="fa-solid fa-chevron-left"></i>
                </a>
            <?php else: ?>
                <a href="#" class="disabled"><i class="fa-solid fa-chevron-left"></i></a>
            <?php endif; ?>
            
            <?php 
            $startPage = max(1, $currentPage - 2);
            $endPage = min($totalPages, $currentPage + 2);
            
            if ($startPage > 1): ?>
                <a href="?tab=<?= $tab ?>&page=1<?= !empty($searchQuery) ? '&q=' . urlencode($searchQuery) : '' ?>">1</a>
                <?php if ($startPage > 2): ?>
                    <span style="padding:8px 4px;color:var(--text-muted);">…</span>
                <?php endif; ?>
            <?php endif; ?>
            
            <?php for ($i = $startPage; $i <= $endPage; $i++): ?>
                <a href="?tab=<?= $tab ?>&page=<?= $i ?><?= !empty($searchQuery) ? '&q=' . urlencode($searchQuery) : '' ?>" 
                   class="<?= $i === $currentPage ? 'active' : '' ?>">
                    <?= $i ?>
                </a>
            <?php endfor; ?>
            
            <?php if ($endPage < $totalPages): ?>
                <?php if ($endPage < $totalPages - 1): ?>
                    <span style="padding:8px 4px;color:var(--text-muted);">…</span>
                <?php endif; ?>
                <a href="?tab=<?= $tab ?>&page=<?= $totalPages ?><?= !empty($searchQuery) ? '&q=' . urlencode($searchQuery) : '' ?>">
                    <?= $totalPages ?>
                </a>
            <?php endif; ?>
            
            <?php if ($currentPage < $totalPages): ?>
                <a href="?tab=<?= $tab ?>&page=<?= $currentPage + 1 ?><?= !empty($searchQuery) ? '&q=' . urlencode($searchQuery) : '' ?>">
                    <i class="fa-solid fa-chevron-right"></i>
                </a>
            <?php else: ?>
                <a href="#" class="disabled"><i class="fa-solid fa-chevron-right"></i></a>
            <?php endif; ?>
        </div>
        <?php endif; ?>
    <?php endif; ?>
</main>

<aside class="sidebar-right">
    <!-- Quick stats -->
    <div class="widget">
        <div class="widget-title"><i class="fa-solid fa-chart-simple"></i> <?= __('people_community_stats') ?></div>
        <?php
        $totalUsers = DB::fetchOne("SELECT COUNT(*) as total FROM users WHERE is_banned = 0");
        $totalBarks = DB::fetchOne("SELECT COUNT(*) as total FROM barks WHERE is_deleted = 0");
        $totalBones = DB::fetchOne("SELECT COALESCE(SUM(amount),0) as total FROM bones WHERE status = 'completed'");
        $totalPaws = DB::fetchOne("SELECT COUNT(*) as total FROM paws");
        ?>
        <div style="font-size:0.85rem;color:var(--text-secondary);line-height:2;">
            <div style="display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid var(--border);">
                <span><i class="fa-solid fa-users"></i> <?= __('people_total_users') ?></span>
                <span><strong><?= number_format($totalUsers['total'] ?? 0) ?></strong></span>
            </div>
            <div style="display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid var(--border);">
                <span><i class="fa-solid fa-message"></i> <?= __('people_total_barks') ?></span>
                <span><strong><?= number_format($totalBarks['total'] ?? 0) ?></strong></span>
            </div>
            <div style="display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid var(--border);">
                <span><i class="fa-solid fa-paw"></i> <?= __('people_total_paws') ?></span>
                <span><strong><?= number_format($totalPaws['total'] ?? 0) ?></strong></span>
            </div>
            <div style="display:flex;justify-content:space-between;padding:4px 0;">
                <span><i class="fa-solid fa-bone"></i> <?= __('people_total_bones') ?></span>
                <span><strong>€<?= number_format($totalBones['total'] ?? 0, 2) ?></strong></span>
            </div>
        </div>
    </div>

    <!-- Random user -->
    <?php
    $randomUser = DB::fetchOne("
        SELECT u.id, u.username, u.display_name, u.avatar, u.gender 
        FROM users u
        WHERE u.is_banned = 0 AND u.id != ?
        ORDER BY RAND()
        LIMIT 1
    ", [$_SESSION['user_id']]);
    if ($randomUser):
    ?>
    <div class="widget">
        <div class="widget-title"><i class="fa-solid fa-dice"></i> <?= __('people_random_user') ?></div>
        <div style="display:flex;align-items:center;gap:12px;padding:4px 0;">
            <img src="<?= get_user_avatar($randomUser) ?>" style="width:48px;height:48px;border-radius:50%;object-fit:cover;border:2px solid var(--orange);">
            <div>
                <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($randomUser['username']) ?>" style="font-weight:700;color:var(--text-primary);text-decoration:none;">
                    <?= htmlspecialchars($randomUser['display_name']) ?>
                </a>
                <div style="font-size:0.75rem;color:var(--text-muted);">@<?= htmlspecialchars($randomUser['username']) ?></div>
            </div>
        </div>
    </div>
    <?php endif; ?>
</aside>

<script>
// Fonctions pour les actions
async function toggleFollow(userId, btn) {
    const fd = new FormData();
    fd.append('user_id', userId);
    fd.append('csrf_token', getCsrfToken());

    try {
        const res = await fetch(`${SITE_URL}/api/follow.php`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            btn.textContent = data.following ? '<?= __('btn_following') ?>' : '<?= __('btn_follow') ?>';
            btn.classList.toggle('following', data.following);
            showToast(data.following ? '<?= __('people_followed') ?>' : '<?= __('people_unfollowed') ?>', 'success');
        } else {
            showToast(data.error || '<?= __('error_occurred') ?>', 'error');
        }
    } catch (error) {
        console.error('Follow error:', error);
        showToast('<?= __('error_network') ?>', 'error');
    }
}

function openBoneModal(barkId, receiverId, receiverName) {
    if (typeof window.openBoneModal === 'function') {
        window.openBoneModal(barkId, receiverId, receiverName);
    } else {
        showToast('<?= __('people_bone_feature') ?>', 'info');
    }
}

function showToast(message, type = 'success') {
    if (typeof window.showToast === 'function') {
        window.showToast(message, type);
    } else {
        const container = document.createElement('div');
        container.style.cssText = 'position:fixed;bottom:20px;right:20px;background:var(--bg-card);color:var(--text-primary);padding:12px 20px;border-radius:30px;border-left:4px solid ' + (type === 'error' ? '#FF3B30' : '#2ECC71') + ';z-index:9999;box-shadow:0 4px 20px rgba(0,0,0,0.3);max-width:90vw;';
        container.textContent = message;
        document.body.appendChild(container);
        setTimeout(() => container.remove(), 3000);
    }
}

function getCsrfToken() {
    return document.querySelector('input[name="csrf_token"]')?.value || '';
}

// Soumission de la recherche avec Enter
document.querySelector('.people-search-box input')?.addEventListener('keydown', function(e) {
    if (e.key === 'Enter') {
        e.preventDefault();
        const query = this.value.trim();
        if (query.length >= 2) {
            window.location.href = '?tab=search&q=' + encodeURIComponent(query);
        } else {
            showToast('<?= __('people_search_min_chars') ?>', 'error');
        }
    }
});
</script>

<?php include __DIR__ . '/../includes/footer.php'; ?>