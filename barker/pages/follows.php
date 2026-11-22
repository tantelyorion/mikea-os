<?php
// ============================================================
// BARKER — Followers / Following page - VERSION TRADUITE
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$currentUser = current_user();
$username    = $_GET['u'] ?? '';
$tab         = $_GET['tab'] ?? 'followers';

if (!$username) redirect(SITE_URL . '/index.php');

$profile = DB::fetchOne("SELECT * FROM users WHERE username=? AND is_banned=0", [$username]);
if (!$profile) redirect(SITE_URL . '/index.php');

if ($tab === 'followers') {
    $users = DB::fetchAll("
        SELECT u.*,
               (SELECT COUNT(*) FROM follows WHERE follower_id=? AND following_id=u.id) AS is_following
        FROM follows f
        JOIN users u ON u.id=f.follower_id
        WHERE f.following_id=? AND u.is_banned=0
        ORDER BY f.created_at DESC
        LIMIT 100
    ", [$_SESSION['user_id'], $profile['id']]);
    $title = __('follow_followers_of', ['name' => $profile['display_name']]);
} else {
    $users = DB::fetchAll("
        SELECT u.*,
               (SELECT COUNT(*) FROM follows WHERE follower_id=? AND following_id=u.id) AS is_following
        FROM follows f
        JOIN users u ON u.id=f.following_id
        WHERE f.follower_id=? AND u.is_banned=0
        ORDER BY f.created_at DESC
        LIMIT 100
    ", [$_SESSION['user_id'], $profile['id']]);
    $title = __('follow_following_of', ['name' => $profile['display_name']]);
}

$pageTitle = $title;
include __DIR__ . '/../includes/header.php';
?>

<main class="main-feed">
    <div class="feed-header">
        <button onclick="history.back()" style="background:none;border:none;cursor:pointer;color:var(--text-primary);font-size:1.1rem;padding:4px 8px;border-radius:8px">
            <i class="fa-solid fa-arrow-left"></i>
        </button>
        <div>
            <h1 class="feed-header-title"><?= htmlspecialchars($profile['display_name']) ?></h1>
            <div style="font-size:0.82rem;color:var(--text-muted)">@<?= htmlspecialchars($profile['username']) ?></div>
        </div>
    </div>

    <div class="feed-tabs">
        <a href="?u=<?= urlencode($username) ?>&tab=followers" class="feed-tab <?= $tab==='followers'?'active':'' ?>" style="text-decoration:none">
            <?= __('btn_followers') ?>
        </a>
        <a href="?u=<?= urlencode($username) ?>&tab=following" class="feed-tab <?= $tab==='following'?'active':'' ?>" style="text-decoration:none">
            <?= __('btn_following') ?>
        </a>
    </div>

    <?php if (empty($users)): ?>
        <div class="loader">
            <div style="font-size:3rem"><i class="fa-solid fa-paw"></i></div>
            <p style="margin-top:8px">
                <?= $tab === 'followers'
                    ? __('follow_no_followers', ['name' => htmlspecialchars($profile['display_name'])])
                    : __('follow_no_following', ['name' => htmlspecialchars($profile['display_name'])]) ?>
            </p>
        </div>
    <?php else: ?>
        <?php foreach ($users as $u): ?>
        <div class="follow-item" style="padding:14px 20px;border-bottom:1px solid var(--border)">
            <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($u['username']) ?>">
                <img src="<?= get_user_avatar($u) ?>" class="follow-avatar" style="width:48px;height:48px" alt="">
            </a>
            <div class="follow-info">
                <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($u['username']) ?>" style="text-decoration:none;color:var(--text-primary)">
                    <div class="follow-name" style="font-size:0.95rem">
                        <?= htmlspecialchars($u['display_name']) ?>
                        <?php if ($u['is_verified']): ?><span class="verified-badge">✓</span><?php endif; ?>
                    </div>
                    <div class="follow-handle">@<?= htmlspecialchars($u['username']) ?></div>
                </a>
                <?php if (!empty($u['bio'])): ?>
                <div style="font-size:0.82rem;color:var(--text-secondary);margin-top:3px"><?= htmlspecialchars(substr($u['bio'],0,80)) ?></div>
                <?php endif; ?>
            </div>
            <?php if ($u['id'] !== $_SESSION['user_id']): ?>
            <button class="follow-btn <?= $u['is_following']?'following':'' ?>"
                    onclick="toggleFollow(<?= $u['id'] ?>,this)">
                <?= $u['is_following'] ? __('btn_following') : __('btn_follow') ?>
            </button>
            <?php else: ?>
            <span style="font-size:0.82rem;color:var(--text-muted);padding:6px 12px"><?= __('follow_you') ?></span>
            <?php endif; ?>
        </div>
        <?php endforeach; ?>
    <?php endif; ?>
</main>

<aside class="sidebar-right">
    <div class="widget">
        <div class="widget-title"><i class="fa-solid fa-paw"></i> <?= htmlspecialchars($profile['display_name']) ?></div>
        <div style="display:flex;gap:12px;align-items:center;margin-bottom:12px">
            <img src="<?= get_user_avatar($profile) ?>" style="width:52px;height:52px;border-radius:50%;border:2px solid var(--orange)" alt="">
            <div>
                <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($profile['username']) ?>" style="font-weight:700;color:var(--text-primary);text-decoration:none">
                    <?= htmlspecialchars($profile['display_name']) ?>
                </a>
                <div style="color:var(--text-muted);font-size:0.85rem">@<?= htmlspecialchars($profile['username']) ?></div>
            </div>
        </div>
        <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($profile['username']) ?>" class="btn btn-outline btn-full" style="font-size:0.85rem">
            <?= __('profile_view_profile') ?>
        </a>
    </div>
</aside>

<?php include __DIR__ . '/../includes/footer.php'; ?>