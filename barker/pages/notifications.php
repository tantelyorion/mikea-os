<?php
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$currentUser = current_user();

// Pagination simple : ?limit=60 (par défaut), +60 à chaque « Charger plus »
$limit = max(60, min(300, (int)($_GET['limit'] ?? 60)));

// Fetch notifications with sender info
$notifications = DB::fetchAll("
    SELECT n.*,
           u.username AS from_username,
           u.display_name AS from_display_name,
           u.avatar AS from_avatar,
           u.gender AS from_gender,
           b.content AS bark_content,
           b.is_anonymous AS bark_anon
    FROM notifications n
    LEFT JOIN users u ON u.id = n.from_user_id
    LEFT JOIN barks b ON b.id = n.bark_id
    WHERE n.user_id = ?
    ORDER BY n.created_at DESC
    LIMIT " . ($limit + 1) . "
", [$_SESSION['user_id']]);

$hasMore = count($notifications) > $limit;
if ($hasMore) array_pop($notifications);

// Marquer comme lues APRÈS la lecture : sinon les notifications
// « non lues » n'apparaissaient jamais comme telles.
DB::execute("UPDATE notifications SET is_read=1 WHERE user_id=?", [$_SESSION['user_id']]);

// Group notifications by date
$groupedNotifications = [];
$today = date('Y-m-d');
$yesterday = date('Y-m-d', strtotime('-1 day'));
$thisWeek = date('Y-m-d', strtotime('-7 days'));

foreach ($notifications as $notif) {
    $notifDate = date('Y-m-d', strtotime($notif['created_at']));
    
    if ($notifDate == $today) {
        $group = __('notif_today');
    } elseif ($notifDate == $yesterday) {
        $group = __('notif_yesterday');
    } elseif ($notifDate >= $thisWeek) {
        $group = __('notif_this_week');
    } else {
        $group = __('notif_earlier');
    }
    
    $groupedNotifications[$group][] = $notif;
}

$pageTitle = __('nav_notifications');
include __DIR__ . '/../includes/header.php';
?>

<main class="main-feed">
    <div class="feed-header">
        <h1 class="feed-header-title">
            <i class="fa-regular fa-bell" style="margin-right: 8px;"></i>
            <?= __('nav_notifications') ?>
        </h1>
        <?php if (!empty($notifications)): ?>
        <button onclick="markAllNotificationsRead()" style="background:none; border:none; color:var(--orange); cursor:pointer; font-size:0.85rem; margin-left:auto;">
            <i class="fa-regular fa-check-circle"></i> <?= __('notif_mark_all_read') ?>
        </button>
        <?php endif; ?>
    </div>

    <?php if (empty($notifications)): ?>
        <div class="empty-state">
            <div class="empty-state-icon">
                <i class="fa-regular fa-bell-slash"></i>
            </div>
            <h3 class="empty-state-title"><?= __('notif_empty') ?></h3>
            <p class="empty-state-text">
                <?= __('notif_when_interact') ?>
            </p>
            <a href="<?= SITE_URL ?>/index.php" class="btn btn-primary" style="margin-top: 16px;">
                <i class="fa-solid fa-paw"></i> <?= __('feed_home') ?>
            </a>
        </div>
    <?php else: ?>
        <?php foreach ($groupedNotifications as $group => $groupNotifs): ?>
            <div class="notif-group">
                <div class="notif-group-header">
                    <span class="notif-group-title"><?= $group ?></span>
                </div>
                <?php foreach ($groupNotifs as $notif):
                    $fromUser = [
                        'avatar' => $notif['from_avatar'],
                        'gender' => $notif['from_gender'],
                    ];
                    $fromAvatar = get_user_avatar($fromUser);
                    
                    // IMPORTANT : "wolf-pack-battalion" n'existe QUE dans le
                    // style "Brands" de Font Awesome (fa-brands / fab), pas
                    // dans "Solid" (fa-solid / fas). Avec le préfixe fa-solid
                    // codé en dur plus bas, cette icône ne s'affichait jamais
                    // (glyphe manquant) — d'où les icônes "cassées" signalées.
                    $iconConfig = match($notif['type']) {
                        'paw'     => ['icon' => 'fa-paw', 'style' => 'fa-solid', 'color' => '#FF4757', 'bg' => 'rgba(255,71,87,0.1)'],
                        'yap'     => ['icon' => 'fa-comment', 'style' => 'fa-solid', 'color' => '#4DABF7', 'bg' => 'rgba(77,171,247,0.1)'],
                        'rebark'  => ['icon' => 'fa-retweet', 'style' => 'fa-solid', 'color' => '#2ECC71', 'bg' => 'rgba(46,204,113,0.1)'],
                        'howl'    => ['icon' => 'fa-wolf-pack-battalion', 'style' => 'fa-brands', 'color' => '#9B59B6', 'bg' => 'rgba(155,89,182,0.1)'],
                        'bone'    => ['icon' => 'fa-bone', 'style' => 'fa-solid', 'color' => '#FF6200', 'bg' => 'rgba(255,98,0,0.1)'],
                        'follow'  => ['icon' => 'fa-user-plus', 'style' => 'fa-solid', 'color' => '#20D9C2', 'bg' => 'rgba(32,217,194,0.1)'],
                        'mention' => ['icon' => 'fa-at', 'style' => 'fa-solid', 'color' => '#FFB300', 'bg' => 'rgba(255,179,0,0.1)'],
                        default   => ['icon' => 'fa-bell', 'style' => 'fa-solid', 'color' => '#888', 'bg' => 'rgba(136,136,136,0.1)']
                    };
                    
                    $typeLabel = match($notif['type']) {
                        'paw'    => __('notif_pawed'),
                        'yap'    => __('notif_yapped'),
                        'rebark' => __('notif_rebarked'),
                        'howl'   => __('notif_howled'),
                        'bone'   => __('notif_bone'),
                        'follow' => __('notif_followed'),
                        'mention' => __('notif_mentioned'),
                        default  => __('notif_interacted')
                    };
                    
                    $fromName = !empty($notif['from_username'])
                        ? htmlspecialchars($notif['from_display_name'])
                        : __('bark_anonymous');
                    
                    $actionUrl = '';
                    if ($notif['bark_id']) {
                        $actionUrl = SITE_URL . "/pages/bark.php?id={$notif['bark_id']}";
                    } elseif (!empty($notif['from_username'])) {
                        $actionUrl = SITE_URL . "/pages/profile.php?u={$notif['from_username']}";
                    }
                ?>
                <div class="notif-item <?= !$notif['is_read'] ? 'unread' : '' ?>" 
                     onclick="<?= $actionUrl ? "location.href='$actionUrl'" : '' ?>"
                     data-notif-id="<?= $notif['id'] ?>">
                    
                    <div class="notif-avatar">
                        <?php if (!empty($notif['from_username'])): ?>
                            <img src="<?= $fromAvatar ?>" alt="">
                            <div class="notif-icon-badge" style="background: <?= $iconConfig['bg'] ?>; color: <?= $iconConfig['color'] ?>;">
                                <i class="<?= $iconConfig['style'] ?> <?= $iconConfig['icon'] ?>"></i>
                            </div>
                        <?php else: ?>
                            <div class="notif-avatar-placeholder" style="background: <?= $iconConfig['bg'] ?>; color: <?= $iconConfig['color'] ?>;">
                                <i class="<?= $iconConfig['style'] ?> <?= $iconConfig['icon'] ?>"></i>
                            </div>
                        <?php endif; ?>
                    </div>
                    
                    <div class="notif-content">
                        <div class="notif-text">
                            <strong class="notif-user"><?= $fromName ?></strong>
                            <span class="notif-action"><?= $typeLabel ?></span>
                            
                            <?php if ($notif['type'] === 'bone' && !empty($notif['message'])): ?>
                                <div class="notif-message">
                                    <i class="fa-solid fa-quote-left"></i>
                                    "<?= htmlspecialchars($notif['message']) ?>"
                                </div>
                            <?php elseif ($notif['bark_id'] && !empty($notif['bark_content'])): ?>
                                <div class="notif-preview">
                                    <?= htmlspecialchars(mb_substr($notif['bark_content'], 0, 80)) ?>
                                    <?php if (mb_strlen($notif['bark_content']) > 80): ?>...<?php endif; ?>
                                </div>
                            <?php endif; ?>
                        </div>
                        <div class="notif-time">
                            <i class="fa-regular fa-clock"></i>
                            <?= time_ago($notif['created_at']) ?>
                        </div>
                    </div>
                    
                    <?php if (!$notif['is_read']): ?>
                        <div class="notif-unread-dot"></div>
                    <?php endif; ?>
                </div>
                <?php endforeach; ?>
            </div>
        <?php endforeach; ?>
        
        <?php if ($hasMore): ?>
        <div class="load-more-container">
            <a class="btn btn-outline" href="?limit=<?= $limit + 60 ?>">
                <i class="fa-solid fa-arrow-down"></i> <?= __('feed_load_more') ?>
            </a>
        </div>
        <?php endif; ?>
    <?php endif; ?>
</main>

<style>
.empty-state {
    text-align: center;
    padding: 60px 24px;
}

.empty-state-icon {
    font-size: 4rem;
    color: var(--text-muted);
    margin-bottom: 16px;
    opacity: 0.5;
}

.empty-state-title {
    font-size: 1.3rem;
    font-weight: 700;
    margin-bottom: 8px;
    color: var(--text-primary);
}

.empty-state-text {
    color: var(--text-muted);
    max-width: 280px;
    margin: 0 auto;
}

.notif-group {
    margin-bottom: 8px;
}

.notif-group-header {
    padding: 12px 20px;
    background: var(--bg-card2);
    border-bottom: 1px solid var(--border);
    position: sticky;
    top: 0;
    z-index: 5;
}

.notif-group-title {
    font-size: 0.8rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: var(--text-muted);
}

.notif-item {
    display: flex;
    align-items: flex-start;
    gap: 14px;
    padding: 16px 20px;
    border-bottom: 1px solid var(--border);
    cursor: pointer;
    transition: all 0.2s ease;
    position: relative;
}

.notif-item:hover {
    background: var(--orange-faint);
}

.notif-item.unread {
    background: rgba(255, 98, 0, 0.04);
}

.notif-item.unread:hover {
    background: var(--orange-faint);
}

.notif-avatar {
    position: relative;
    flex-shrink: 0;
}

.notif-avatar img {
    width: 48px;
    height: 48px;
    border-radius: 50%;
    object-fit: cover;
    border: 2px solid var(--border);
}

.notif-icon-badge {
    position: absolute;
    bottom: -4px;
    right: -4px;
    width: 22px;
    height: 22px;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 0.7rem;
    border: 2px solid var(--bg-card);
}

.notif-avatar-placeholder {
    width: 48px;
    height: 48px;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 1.3rem;
    flex-shrink: 0;
}

.notif-content {
    flex: 1;
    min-width: 0;
}

.notif-text {
    font-size: 0.9rem;
    line-height: 1.4;
    color: var(--text-secondary);
    margin-bottom: 4px;
}

.notif-user {
    color: var(--text-primary);
    font-weight: 700;
}

.notif-action {
    color: var(--text-secondary);
}

.notif-preview {
    margin-top: 6px;
    padding: 8px 12px;
    background: var(--bg-card2);
    border-radius: 12px;
    font-size: 0.85rem;
    color: var(--text-muted);
    border-left: 3px solid var(--orange);
    word-break: break-word;
}

.notif-message {
    margin-top: 6px;
    padding: 8px 12px;
    background: var(--orange-faint);
    border-radius: 12px;
    font-size: 0.85rem;
    color: var(--orange);
    font-style: italic;
}

.notif-time {
    font-size: 0.7rem;
    color: var(--text-muted);
    display: flex;
    align-items: center;
    gap: 4px;
}

.notif-unread-dot {
    width: 8px;
    height: 8px;
    background: var(--orange);
    border-radius: 50%;
    flex-shrink: 0;
    margin-left: 8px;
}

.load-more-container {
    text-align: center;
    padding: 20px;
}

@media (max-width: 768px) {
    .notif-item {
        padding: 14px 16px;
        gap: 12px;
    }
    
    .notif-avatar img,
    .notif-avatar-placeholder {
        width: 44px;
        height: 44px;
    }
    
    .notif-icon-badge {
        width: 18px;
        height: 18px;
        font-size: 0.6rem;
    }
    
    .notif-text {
        font-size: 0.85rem;
    }
    
    .notif-preview,
    .notif-message {
        font-size: 0.8rem;
        padding: 6px 10px;
    }
    
    .notif-group-header {
        padding: 10px 16px;
    }
    
    .empty-state {
        padding: 40px 20px;
    }
    
    .empty-state-icon {
        font-size: 3rem;
    }
    
    .empty-state-title {
        font-size: 1.1rem;
    }
}

@media (max-width: 480px) {
    .notif-avatar img,
    .notif-avatar-placeholder {
        width: 40px;
        height: 40px;
    }
    
    .notif-text {
        font-size: 0.8rem;
    }
    
    .feed-header {
        flex-wrap: wrap;
    }
    
    .feed-header button {
        font-size: 0.75rem;
    }
}
</style>

<script>
async function markAllNotificationsRead() {
    try {
        const response = await fetch(SITE_URL + '/api/notifications.php?action=mark_all_read', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                csrf_token: getCsrfToken()
            })
        });
        
        const data = await response.json();
        
        if (data.success) {
            document.querySelectorAll('.notif-item.unread').forEach(item => {
                item.classList.remove('unread');
                const dot = item.querySelector('.notif-unread-dot');
                if (dot) dot.remove();
            });
            showToast('<?= __('notif_all_marked_read') ?>', 'success');
            
            const badge = document.querySelector('.nav-badge');
            if (badge) badge.style.display = 'none';
        }
    } catch (error) {
        console.error('Error marking notifications read:', error);
        showToast('<?= __('error_marking_read') ?>', 'error');
    }
}

</script>

<?php include __DIR__ . '/../includes/footer.php'; ?>