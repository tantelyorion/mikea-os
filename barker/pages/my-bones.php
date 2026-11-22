<?php
// ============================================================
// BARKER — My Bones Page
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$currentUser = current_user();
$userId = $_SESSION['user_id'];

// Statistiques des Bones
$stats = DB::fetchOne("
    SELECT 
        COALESCE(SUM(amount), 0) AS total_received,
        COALESCE(SUM(CASE WHEN status = 'completed' THEN amount ELSE 0 END), 0) AS total_completed,
        COALESCE(SUM(CASE WHEN status = 'pending' THEN amount ELSE 0 END), 0) AS total_pending,
        COUNT(*) AS total_count,
        COUNT(DISTINCT sender_id) AS unique_senders
    FROM bones 
    WHERE receiver_id = ?
", [$userId]);

// Liste des Bones reçus
$bones = DB::fetchAll("
    SELECT 
        b.*,
        u.username,
        u.display_name,
        u.avatar,
        u.gender,
        bark.content as bark_content
    FROM bones b
    LEFT JOIN users u ON u.id = b.sender_id
    LEFT JOIN barks bark ON bark.id = b.bark_id
    WHERE b.receiver_id = ?
    ORDER BY b.created_at DESC
    LIMIT 50
", [$userId]);

$pageTitle = 'My Bones';
include __DIR__ . '/../includes/header.php';
?>

<main class="main-feed">
    <div class="feed-header">
        <h1 class="feed-header-title"><i class="fa-solid fa-bone"></i> My Bones</h1>
    </div>

    <!-- Stats Cards -->
    <div style="padding: 20px; display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 16px;">
        <div class="bone-stat-card">
            <div class="bone-stat-icon">💰</div>
            <div class="bone-stat-value">€<?= number_format($stats['total_received'], 2) ?></div>
            <div class="bone-stat-label">Total Received</div>
        </div>
        
        <div class="bone-stat-card">
            <div class="bone-stat-icon"><i class="fa-solid fa-circle-check"></i></div>
            <div class="bone-stat-value">€<?= number_format($stats['total_completed'], 2) ?></div>
            <div class="bone-stat-label">Completed</div>
        </div>
        
        <div class="bone-stat-card">
            <div class="bone-stat-icon">⏳</div>
            <div class="bone-stat-value">€<?= number_format($stats['total_pending'], 2) ?></div>
            <div class="bone-stat-label">Pending</div>
        </div>
        
        <div class="bone-stat-card">
            <div class="bone-stat-icon">👥</div>
            <div class="bone-stat-value"><?= number_format($stats['unique_senders']) ?></div>
            <div class="bone-stat-label">Unique Supporters</div>
        </div>
    </div>

    <!-- Bones History -->
    <div class="feed-tabs">
        <div class="feed-tab active">Bones History</div>
    </div>

    <?php if (empty($bones)): ?>
        <div class="loader">
            <div style="font-size:3rem"><i class="fa-solid fa-bone"></i></div>
            <p style="font-weight:600">No bones received yet</p>
            <p class="text-muted">When someone sends you a Bone, it will appear here.</p>
        </div>
    <?php else: ?>
        <?php foreach ($bones as $bone): 
            $sender = [
                'id' => $bone['sender_id'],
                'username' => $bone['username'] ?? 'anonymous',
                'display_name' => $bone['display_name'] ?? 'Anonymous',
                'avatar' => $bone['avatar'] ?? null,
                'gender' => $bone['gender'] ?? 'other'
            ];
        ?>
        <article class="bone-item" style="padding: 16px 20px; border-bottom: 1px solid var(--border);">
            <div style="display: flex; align-items: flex-start; gap: 12px;">
                <?php if ($bone['sender_id']): ?>
                <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($sender['username']) ?>">
                    <img src="<?= get_user_avatar($sender) ?>" style="width: 44px; height: 44px; border-radius: 50%; object-fit: cover; border: 2px solid var(--border);" alt="">
                </a>
                <?php else: ?>
                <div style="width: 44px; height: 44px; border-radius: 50%; background: var(--bg-card2); display: flex; align-items: center; justify-content: center; font-size: 1.2rem; border: 2px solid var(--border);">
                    <i class="fa-solid fa-glasses"></i>
                </div>
                <?php endif; ?>
                
                <div style="flex: 1;">
                    <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 4px;">
                        <div>
                            <?php if ($bone['sender_id']): ?>
                            <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($sender['username']) ?>" style="font-weight: 700; color: var(--text-primary); text-decoration: none;">
                                <?= htmlspecialchars($sender['display_name']) ?>
                            </a>
                            <span style="color: var(--text-muted); font-size: 0.85rem; margin-left: 4px;">
                                @<?= htmlspecialchars($sender['username']) ?>
                            </span>
                            <?php else: ?>
                            <span style="font-weight: 700; color: var(--text-primary);"><i class="fa-solid fa-glasses"></i> Anonymous</span>
                            <?php endif; ?>
                            
                            <span style="margin-left: 8px; font-size: 0.85rem; color: var(--text-muted);">
                                sent you a Bone
                            </span>
                        </div>
                        
                        <div style="display: flex; align-items: center; gap: 12px;">
                            <span style="font-size: 1.1rem; font-weight: 700; color: var(--orange);">
                                €<?= number_format($bone['amount'], 2) ?>
                            </span>
                            
                            <?php if ($bone['status'] === 'pending'): ?>
                            <span style="background: #FFC107; color: #000; padding: 2px 8px; border-radius: 12px; font-size: 0.7rem; font-weight: 600;">Pending</span>
                            <?php elseif ($bone['status'] === 'completed'): ?>
                            <span style="background: #2ECC71; color: #fff; padding: 2px 8px; border-radius: 12px; font-size: 0.7rem; font-weight: 600;">Completed</span>
                            <?php endif; ?>
                        </div>
                    </div>
                    
                    <?php if (!empty($bone['message'])): ?>
                    <div style="background: var(--bg-card2); padding: 8px 12px; border-radius: 12px; margin: 8px 0; font-size: 0.9rem; color: var(--text-secondary);">
                        <i class="fa-solid fa-comment"></i> "<?= htmlspecialchars($bone['message']) ?>"
                    </div>
                    <?php endif; ?>
                    
                    <?php if ($bone['bark_id'] && !empty($bone['bark_content'])): ?>
                    <div style="margin-top: 8px; padding: 8px 12px; background: var(--orange-faint); border-radius: 8px; border-left: 3px solid var(--orange);">
                        <div style="font-size: 0.75rem; color: var(--text-muted); margin-bottom: 4px;">
                            <i class="fa-solid fa-reply"></i> In response to:
                        </div>
                        <a href="<?= SITE_URL ?>/pages/bark.php?id=<?= $bone['bark_id'] ?>" style="color: var(--text-primary); text-decoration: none; font-size: 0.85rem;">
                            <?= htmlspecialchars(substr($bone['bark_content'], 0, 100)) ?>...
                        </a>
                    </div>
                    <?php endif; ?>
                    
                    <div style="font-size: 0.75rem; color: var(--text-muted); margin-top: 6px;">
                        <i class="fa-regular fa-clock"></i> <?= time_ago($bone['created_at']) ?>
                    </div>
                </div>
            </div>
        </article>
        <?php endforeach; ?>
    <?php endif; ?>
</main>

<aside class="sidebar-right">
</aside>

<style>
.bone-stat-card {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 20px;
    text-align: center;
    transition: transform 0.15s, box-shadow 0.15s;
}

.bone-stat-card:hover {
    transform: translateY(-2px);
    box-shadow: var(--shadow);
}

.bone-stat-icon {
    font-size: 2rem;
    margin-bottom: 8px;
}

.bone-stat-value {
    font-size: 1.5rem;
    font-weight: 700;
    color: var(--orange);
    margin-bottom: 4px;
}

.bone-stat-label {
    font-size: 0.8rem;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.5px;
}

.bone-item {
    transition: background 0.15s;
}

.bone-item:hover {
    background: var(--bg-card2);
}
</style>

<?php include __DIR__ . '/../includes/footer.php'; ?>