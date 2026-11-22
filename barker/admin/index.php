<?php
// ============================================================
// BARKER — Admin Panel
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$currentUser = current_user();

// Accès réservé aux administrateurs (colonne users.is_admin, voir
// sql/migration_messaging_native.sql). "is_verified" n'est QUE le badge.
if (!$currentUser || empty($currentUser['is_admin'])) {
    http_response_code(403);
    die('Access denied. Admin only.');
}

$tab = $_GET['tab'] ?? 'dashboard';

// Handle actions
if ($_SERVER['REQUEST_METHOD'] === 'POST' && verify_csrf($_POST['csrf_token'] ?? '')) {
    $action = $_POST['action'] ?? '';

    if ($action === 'ban_user') {
        $userId = (int)$_POST['user_id'];
        $reason = trim($_POST['reason'] ?? 'Violation of terms');
        $target = DB::fetchOne("SELECT is_admin FROM users WHERE id=?", [$userId]);
        if ($userId === (int)$currentUser['id'] || !empty($target['is_admin'])) {
            $_SESSION['flash'] = ['type'=>'error','msg'=>'Impossible de bannir un administrateur.'];
            header('Location: ' . SITE_URL . '/admin/index.php?tab=' . urlencode($tab)); exit;
        }
        DB::execute("UPDATE users SET is_banned=1, ban_reason=? WHERE id=?", [$reason, $userId]);
        log_security($currentUser['id'], 'admin_ban_user', "target=$userId reason=$reason");
        $_SESSION['flash'] = ['type'=>'success','msg'=>'User banned.'];
    }

    if ($action === 'unban_user') {
        $userId = (int)$_POST['user_id'];
        DB::execute("UPDATE users SET is_banned=0, ban_reason=NULL WHERE id=?", [$userId]);
        log_security($currentUser['id'], 'admin_unban_user', "target=$userId");
        $_SESSION['flash'] = ['type'=>'success','msg'=>'User unbanned.'];
    }

    if ($action === 'verify_user') {
        $userId = (int)$_POST['user_id'];
        DB::execute("UPDATE users SET is_verified=1 WHERE id=?", [$userId]);
        $_SESSION['flash'] = ['type'=>'success','msg'=>'User verified ✓'];
    }

    if ($action === 'delete_bark') {
        $barkId = (int)$_POST['bark_id'];
        DB::execute("UPDATE barks SET is_deleted=1 WHERE id=?", [$barkId]);
        log_security($currentUser['id'], 'admin_delete_bark', "bark_id=$barkId");
        $_SESSION['flash'] = ['type'=>'success','msg'=>'Bark deleted.'];
    }

    if ($action === 'dismiss_report') {
        $reportId = (int)$_POST['report_id'];
        DB::execute("UPDATE reports SET status='dismissed' WHERE id=?", [$reportId]);
        $_SESSION['flash'] = ['type'=>'success','msg'=>'Report dismissed.'];
    }

    if ($action === 'reviewed_report') {
        $reportId = (int)$_POST['report_id'];
        DB::execute("UPDATE reports SET status='reviewed' WHERE id=?", [$reportId]);
        $_SESSION['flash'] = ['type'=>'success','msg'=>'Report marked as reviewed.'];
    }

    redirect(SITE_URL . '/admin/index.php?tab=' . $tab);
}

// Stats
$stats = DB::fetchOne("
    SELECT
        (SELECT COUNT(*) FROM users WHERE is_banned=0)      AS total_users,
        (SELECT COUNT(*) FROM users WHERE is_banned=1)      AS banned_users,
        (SELECT COUNT(*) FROM barks WHERE is_deleted=0)     AS total_barks,
        (SELECT COUNT(*) FROM paws)                          AS total_paws,
        (SELECT COUNT(*) FROM bones)                         AS total_bones,
        (SELECT COALESCE(SUM(amount),0) FROM bones WHERE status='completed') AS total_donated,
        (SELECT COUNT(*) FROM reports WHERE status='pending') AS pending_reports,
        (SELECT COUNT(*) FROM follows)                       AS total_follows
");

// Data per tab
$users   = [];
$reports = [];
$barks   = [];
$logs    = [];

if ($tab === 'users') {
    $search = trim($_GET['s'] ?? '');
    $where  = $search ? "WHERE username LIKE ? OR email LIKE ? OR display_name LIKE ?" : '';
    $params = $search ? ["%$search%","%$search%","%$search%"] : [];
    $users  = DB::fetchAll("SELECT * FROM users $where ORDER BY created_at DESC LIMIT 50", $params);
}

if ($tab === 'reports') {
    $reports = DB::fetchAll("
        SELECT r.*,
               rep.username AS reporter_username,
               b.content AS bark_content,
               bu.username AS bark_author
        FROM reports r
        LEFT JOIN users rep ON rep.id = r.reporter_id
        LEFT JOIN barks b ON b.id = r.bark_id
        LEFT JOIN users bu ON bu.id = b.user_id
        WHERE r.status = 'pending'
        ORDER BY r.created_at DESC
        LIMIT 50
    ");
}

if ($tab === 'barks') {
    $barks = DB::fetchAll("
        SELECT b.*, u.username, u.display_name
        FROM barks b JOIN users u ON u.id=b.user_id
        WHERE b.is_deleted=0
        ORDER BY b.created_at DESC
        LIMIT 100
    ");
}

if ($tab === 'logs') {
    $logs = DB::fetchAll("
        SELECT sl.*, u.username
        FROM security_logs sl
        LEFT JOIN users u ON u.id=sl.user_id
        ORDER BY sl.created_at DESC
        LIMIT 100
    ");
}

$flash = $_SESSION['flash'] ?? null;
unset($_SESSION['flash']);

$pageTitle = 'Admin Panel';
?>
<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Admin — BARKER</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Bebas+Neue&family=Space+Grotesk:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
    <style>
        .admin-layout { display:grid; grid-template-columns:220px 1fr; min-height:100vh; }
        .admin-sidebar {
            background:#0D0D0D; border-right:1px solid var(--border);
            padding:24px 16px; display:flex; flex-direction:column; gap:4px;
            position:sticky; top:0; height:100vh; overflow-y:auto;
        }
        .admin-main { padding:28px; }
        .admin-nav-item {
            display:flex; align-items:center; gap:10px;
            padding:10px 14px; border-radius:10px; border:1px solid transparent;
            color:var(--text-secondary); font-size:0.9rem; font-weight:500;
            text-decoration:none; transition:all 0.2s; cursor:pointer;
        }
        .admin-nav-item:hover, .admin-nav-item.active {
            background:var(--orange-faint); color:var(--orange); border-color:var(--orange-glow);
        }
        .stat-grid { display:grid; grid-template-columns:repeat(4,1fr); gap:16px; margin-bottom:28px; }
        @media(max-width:900px){ .stat-grid{grid-template-columns:repeat(2,1fr);} }
        .stat-card {
            background:var(--bg-card); border:1px solid var(--border); border-radius:14px;
            padding:20px; text-align:center;
        }
        .stat-value { font-family:'Bebas Neue',sans-serif; font-size:2.2rem; color:var(--orange); }
        .stat-label { font-size:0.8rem; color:var(--text-muted); text-transform:uppercase; letter-spacing:1px; margin-top:4px; }
        .admin-table { width:100%; border-collapse:collapse; }
        .admin-table th { background:var(--bg-card2); padding:10px 14px; text-align:left; font-size:0.8rem; text-transform:uppercase; letter-spacing:1px; color:var(--text-muted); border-bottom:1px solid var(--border); }
        .admin-table td { padding:12px 14px; border-bottom:1px solid var(--border); font-size:0.88rem; vertical-align:middle; }
        .admin-table tr:hover td { background:var(--orange-faint); }
        .badge { display:inline-block; padding:3px 10px; border-radius:20px; font-size:0.75rem; font-weight:700; }
        .badge-success { background:rgba(46,204,113,0.15); color:#2ECC71; }
        .badge-danger  { background:rgba(255,59,48,0.15); color:#FF3B30; }
        .badge-warning { background:rgba(255,179,0,0.15); color:#FFB300; }
        .badge-orange  { background:var(--orange-faint); color:var(--orange); }
        .action-form { display:inline; }
        .action-form button { background:none; border:none; cursor:pointer; font-size:0.82rem; padding:4px 10px; border-radius:6px; font-family:'Space Grotesk',sans-serif; font-weight:600; transition:all 0.2s; }
        .btn-ban { color:#FF3B30; border:1px solid rgba(255,59,48,0.3); }
        .btn-ban:hover { background:rgba(255,59,48,0.1); }
        .btn-unban { color:#2ECC71; border:1px solid rgba(46,204,113,0.3); }
        .btn-unban:hover { background:rgba(46,204,113,0.1); }
        .btn-verify { color:var(--orange); border:1px solid var(--orange-glow); }
        .btn-verify:hover { background:var(--orange-faint); }
        .btn-del { color:#FF3B30; border:1px solid rgba(255,59,48,0.3); }
        .btn-del:hover { background:rgba(255,59,48,0.1); }
        .admin-section-title { font-family:'Bebas Neue',sans-serif; font-size:1.4rem; letter-spacing:1px; margin-bottom:16px; }
        .table-wrap { background:var(--bg-card); border:1px solid var(--border); border-radius:14px; overflow:hidden; overflow-x:auto; }
        .log-action { font-family:monospace; font-size:0.8rem; color:var(--orange); }
        .chart-bar-wrap { margin-bottom:20px; }
        .chart-label { font-size:0.8rem; color:var(--text-secondary); margin-bottom:4px; }
        .chart-bar { height:8px; border-radius:4px; background:var(--orange); transition:width 0.5s; }
        .chart-bar-bg { background:var(--bg-card2); border-radius:4px; overflow:hidden; }
    </style>
</head>
<body>
<div class="admin-layout">
    <!-- Sidebar -->
    <aside class="admin-sidebar">
        <a href="<?= SITE_URL ?>/index.php" style="text-decoration:none;margin-bottom:12px">
            <div style="font-family:'Bebas Neue',sans-serif;font-size:1.8rem;color:var(--orange);letter-spacing:2px">
                <i class="fa-solid fa-dog"></i> BARKER
            </div>
            <div style="font-size:0.7rem;color:var(--text-muted);letter-spacing:1px;text-transform:uppercase">Admin Panel</div>
        </a>

        <?php
        $navItems = [
            ['tab'=>'dashboard','icon'=>'fa-gauge','label'=>'Dashboard'],
            ['tab'=>'users',    'icon'=>'fa-users','label'=>'Users'],
            ['tab'=>'barks',    'icon'=>'fa-paw',  'label'=>'Barks'],
            ['tab'=>'reports',  'icon'=>'fa-flag', 'label'=>'Reports', 'badge'=>$stats['pending_reports']],
            ['tab'=>'logs',     'icon'=>'fa-shield-halved','label'=>'Security Logs'],
        ];
        foreach ($navItems as $n):
            $active = $tab === $n['tab'] ? 'active' : '';
        ?>
        <a href="?tab=<?= $n['tab'] ?>" class="admin-nav-item <?= $active ?>">
            <i class="fa-solid <?= $n['icon'] ?>"></i>
            <?= $n['label'] ?>
            <?php if (!empty($n['badge']) && $n['badge'] > 0): ?>
            <span class="nav-badge" style="margin-left:auto"><?= $n['badge'] ?></span>
            <?php endif; ?>
        </a>
        <?php endforeach; ?>

        <div style="flex:1"></div>
        <a href="<?= SITE_URL ?>/index.php" class="admin-nav-item" style="margin-top:8px;border-top:1px solid var(--border);padding-top:12px;border-radius:0">
            <i class="fa-solid fa-arrow-left"></i> Back to BARKER
        </a>
    </aside>

    <!-- Main content -->
    <main class="admin-main">
        <?php if ($flash): ?>
        <div class="alert alert-<?= $flash['type'] ?>" style="margin-bottom:20px">
            <i class="fa-solid fa-check"></i> <?= sanitize($flash['msg']) ?>
        </div>
        <?php endif; ?>

        <!-- ── DASHBOARD ── -->
        <?php if ($tab === 'dashboard'): ?>
        <div class="admin-section-title">📊 Dashboard</div>

        <div class="stat-grid">
            <div class="stat-card">
                <div class="stat-value"><?= number_format($stats['total_users']) ?></div>
                <div class="stat-label">Total Users</div>
            </div>
            <div class="stat-card">
                <div class="stat-value"><?= number_format($stats['total_barks']) ?></div>
                <div class="stat-label">Total Barks</div>
            </div>
            <div class="stat-card">
                <div class="stat-value">€<?= number_format($stats['total_donated'], 0) ?></div>
                <div class="stat-label">Bones Donated</div>
            </div>
            <div class="stat-card" style="border-color:<?= $stats['pending_reports']>0?'rgba(255,59,48,0.4)':'' ?>">
                <div class="stat-value" style="<?= $stats['pending_reports']>0?'color:#FF3B30':'' ?>"><?= $stats['pending_reports'] ?></div>
                <div class="stat-label">Pending Reports</div>
            </div>
            <div class="stat-card">
                <div class="stat-value"><?= number_format($stats['total_paws']) ?></div>
                <div class="stat-label">Total Paws</div>
            </div>
            <div class="stat-card">
                <div class="stat-value"><?= number_format($stats['total_bones']) ?></div>
                <div class="stat-label">Bone Transactions</div>
            </div>
            <div class="stat-card">
                <div class="stat-value"><?= number_format($stats['banned_users']) ?></div>
                <div class="stat-label">Banned Users</div>
            </div>
            <div class="stat-card">
                <div class="stat-value"><?= number_format($stats['total_follows']) ?></div>
                <div class="stat-label">Total Follows</div>
            </div>
        </div>

        <!-- Recent registrations -->
        <div class="admin-section-title">🆕 Recent Users</div>
        <div class="table-wrap" style="margin-bottom:28px">
            <table class="admin-table">
                <thead><tr><th>User</th><th>Email</th><th>Gender</th><th>Joined</th><th>Status</th></tr></thead>
                <tbody>
                <?php
                $recentUsers = DB::fetchAll("SELECT * FROM users ORDER BY created_at DESC LIMIT 10");
                foreach ($recentUsers as $u): ?>
                <tr>
                    <td>
                        <div style="display:flex;gap:8px;align-items:center">
                            <img src="<?= get_user_avatar($u) ?>" style="width:32px;height:32px;border-radius:50%;object-fit:cover">
                            <div>
                                <div style="font-weight:700"><?= htmlspecialchars($u['display_name']) ?></div>
                                <div style="color:var(--text-muted);font-size:0.78rem">@<?= htmlspecialchars($u['username']) ?></div>
                            </div>
                        </div>
                    </td>
                    <td style="color:var(--text-secondary)"><?= htmlspecialchars($u['email']) ?></td>
                    <td><?= $u['gender'] ?></td>
                    <td style="color:var(--text-muted)"><?= date('d M Y', strtotime($u['created_at'])) ?></td>
                    <td>
                        <?php if ($u['is_banned']): ?>
                            <span class="badge badge-danger">Banned</span>
                        <?php elseif ($u['is_verified']): ?>
                            <span class="badge badge-success">Verified ✓</span>
                        <?php else: ?>
                            <span class="badge badge-warning">Active</span>
                        <?php endif; ?>
                    </td>
                </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>

        <!-- Activity overview -->
        <div class="admin-section-title">📈 Platform Overview</div>
        <div style="background:var(--bg-card);border:1px solid var(--border);border-radius:14px;padding:20px">
            <?php
            $metrics = [
                ['label'=>'Users Active', 'val'=>$stats['total_users'], 'max'=>max(1,$stats['total_users'])],
                ['label'=>'Barks Posted', 'val'=>$stats['total_barks'], 'max'=>max(1,$stats['total_barks'])],
                ['label'=>'Paws Given',   'val'=>$stats['total_paws'],  'max'=>max(1,$stats['total_paws'])],
                ['label'=>'Bones Sent',   'val'=>$stats['total_bones'], 'max'=>max(1,$stats['total_bones'])],
                ['label'=>'Follows',      'val'=>$stats['total_follows'],'max'=>max(1,$stats['total_follows'])],
            ];
            $maxVal = max(array_column($metrics,'val'));
            foreach ($metrics as $m):
                $pct = $maxVal > 0 ? round($m['val']/$maxVal*100) : 0;
            ?>
            <div class="chart-bar-wrap">
                <div class="chart-label"><?= $m['label'] ?> — <strong><?= number_format($m['val']) ?></strong></div>
                <div class="chart-bar-bg">
                    <div class="chart-bar" style="width:<?= $pct ?>%"></div>
                </div>
            </div>
            <?php endforeach; ?>
        </div>

        <!-- ── USERS ── -->
        <?php elseif ($tab === 'users'): ?>
        <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:16px">
            <div class="admin-section-title" style="margin-bottom:0">👥 Users</div>
            <form method="GET" style="display:flex;gap:8px">
                <input type="hidden" name="tab" value="users">
                <input type="text" name="s" value="<?= htmlspecialchars($_GET['s']??'') ?>"
                    class="form-control" placeholder="Search users..." style="width:220px;padding:8px 14px">
                <button type="submit" class="btn btn-primary" style="padding:8px 16px">Search</button>
            </form>
        </div>

        <div class="table-wrap">
            <table class="admin-table">
                <thead><tr><th>User</th><th>Email</th><th>Gender</th><th>IP Register</th><th>Joined</th><th>Status</th><th>Actions</th></tr></thead>
                <tbody>
                <?php foreach ($users as $u): ?>
                <tr>
                    <td>
                        <div style="display:flex;gap:8px;align-items:center">
                            <img src="<?= get_user_avatar($u) ?>" style="width:36px;height:36px;border-radius:50%;object-fit:cover;border:2px solid var(--border)">
                            <div>
                                <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($u['username']) ?>" style="font-weight:700;color:var(--text-primary)"><?= htmlspecialchars($u['display_name']) ?></a>
                                <div style="color:var(--text-muted);font-size:0.78rem">@<?= htmlspecialchars($u['username']) ?></div>
                            </div>
                        </div>
                    </td>
                    <td style="color:var(--text-secondary);font-size:0.82rem"><?= htmlspecialchars($u['email']) ?></td>
                    <td><?= $u['gender'] ?></td>
                    <td style="font-family:monospace;font-size:0.78rem;color:var(--text-muted)"><?= htmlspecialchars($u['ip_register'] ?? '') ?></td>
                    <td style="color:var(--text-muted);font-size:0.82rem"><?= date('d M Y', strtotime($u['created_at'])) ?></td>
                    <td>
                        <?php if ($u['is_banned']): ?>
                            <span class="badge badge-danger">Banned</span>
                        <?php elseif ($u['is_verified']): ?>
                            <span class="badge badge-success">Verified</span>
                        <?php else: ?>
                            <span class="badge badge-warning">Active</span>
                        <?php endif; ?>
                    </td>
                    <td>
                        <div style="display:flex;gap:4px;flex-wrap:wrap">
                        <?php if ($u['id'] !== $currentUser['id']): ?>
                            <?php if ($u['is_banned']): ?>
                            <form class="action-form" method="POST">
                                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                                <input type="hidden" name="action" value="unban_user">
                                <input type="hidden" name="user_id" value="<?= $u['id'] ?>">
                                <button type="submit" class="btn-unban">✓ Unban</button>
                            </form>
                            <?php else: ?>
                            <form class="action-form" method="POST" onsubmit="return confirm('Ban '+<?= json_encode($u['username']) ?>+'?')">
                                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                                <input type="hidden" name="action" value="ban_user">
                                <input type="hidden" name="user_id" value="<?= $u['id'] ?>">
                                <input type="hidden" name="reason" value="Violation of terms">
                                <button type="submit" class="btn-ban">⛔ Ban</button>
                            </form>
                            <?php endif; ?>

                            <?php if (!$u['is_verified']): ?>
                            <form class="action-form" method="POST">
                                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                                <input type="hidden" name="action" value="verify_user">
                                <input type="hidden" name="user_id" value="<?= $u['id'] ?>">
                                <button type="submit" class="btn-verify">✓ Verify</button>
                            </form>
                            <?php endif; ?>
                        <?php else: ?>
                            <span style="color:var(--text-muted);font-size:0.8rem">You</span>
                        <?php endif; ?>
                        </div>
                    </td>
                </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>

        <!-- ── BARKS ── -->
        <?php elseif ($tab === 'barks'): ?>
        <div class="admin-section-title"><i class="fa-solid fa-dog"></i> All Barks</div>
        <div class="table-wrap">
            <table class="admin-table">
                <thead><tr><th>#</th><th>Author</th><th>Content</th><th>Anon</th><th>Posted</th><th>Actions</th></tr></thead>
                <tbody>
                <?php foreach ($barks as $b): ?>
                <tr>
                    <td style="color:var(--text-muted)"><?= $b['id'] ?></td>
                    <td>
                        <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($b['username']) ?>" style="color:var(--orange);font-weight:600">@<?= htmlspecialchars($b['username']) ?></a>
                    </td>
                    <td style="max-width:300px">
                        <div style="overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:280px" title="<?= htmlspecialchars($b['content']) ?>">
                            <?= htmlspecialchars(mb_substr($b['content'],0,80)) ?>
                        </div>
                    </td>
                    <td><?= $b['is_anonymous'] ? '<span class="badge badge-orange"><i class="fa-solid fa-glasses"></i> Anon</span>' : '—' ?></td>
                    <td style="color:var(--text-muted);font-size:0.82rem"><?= time_ago($b['created_at']) ?></td>
                    <td>
                        <div style="display:flex;gap:4px">
                            <a href="<?= SITE_URL ?>/pages/bark.php?id=<?= $b['id'] ?>" class="btn-verify" style="display:inline-block;padding:4px 10px;border-radius:6px;border:1px solid var(--orange-glow);color:var(--orange);font-size:0.8rem;text-decoration:none">View</a>
                            <form class="action-form" method="POST" onsubmit="return confirm('Delete this bark?')">
                                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                                <input type="hidden" name="action" value="delete_bark">
                                <input type="hidden" name="bark_id" value="<?= $b['id'] ?>">
                                <button type="submit" class="btn-del">🗑 Delete</button>
                            </form>
                        </div>
                    </td>
                </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>

        <!-- ── REPORTS ── -->
        <?php elseif ($tab === 'reports'): ?>
        <div class="admin-section-title">🚨 Pending Reports</div>

        <?php if (empty($reports)): ?>
        <div style="background:var(--bg-card);border:1px solid var(--border);border-radius:14px;padding:40px;text-align:center">
            <div style="font-size:3rem"><i class="fa-solid fa-circle-check"></i></div>
            <p style="font-weight:700;margin-top:8px">All clear! No pending reports.</p>
        </div>
        <?php else: ?>
        <div class="table-wrap">
            <table class="admin-table">
                <thead><tr><th>Reporter</th><th>Reason</th><th>Bark Content</th><th>Author</th><th>Date</th><th>Actions</th></tr></thead>
                <tbody>
                <?php foreach ($reports as $r): ?>
                <tr>
                    <td>@<?= htmlspecialchars($r['reporter_username'] ?? 'deleted') ?></td>
                    <td><span class="badge badge-danger"><?= htmlspecialchars($r['reason']) ?></span></td>
                    <td style="max-width:250px">
                        <?php if ($r['bark_content']): ?>
                        <div style="overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:230px" title="<?= htmlspecialchars($r['bark_content']) ?>">
                            <?= htmlspecialchars(mb_substr($r['bark_content'],0,70)) ?>
                        </div>
                        <?php else: ?>
                        <span style="color:var(--text-muted)">—</span>
                        <?php endif; ?>
                        <?php if (!empty($r['details'])): ?>
                        <div style="font-size:0.75rem;color:var(--text-muted);margin-top:2px;font-style:italic"><?= htmlspecialchars(mb_substr($r['details'],0,60)) ?></div>
                        <?php endif; ?>
                    </td>
                    <td>
                        <?php if ($r['bark_author']): ?>
                        <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($r['bark_author']) ?>" style="color:var(--orange)">@<?= htmlspecialchars($r['bark_author']) ?></a>
                        <?php else: ?>—<?php endif; ?>
                    </td>
                    <td style="color:var(--text-muted);font-size:0.82rem"><?= time_ago($r['created_at']) ?></td>
                    <td>
                        <div style="display:flex;gap:4px;flex-wrap:wrap">
                            <?php if ($r['bark_id']): ?>
                            <form class="action-form" method="POST" onsubmit="return confirm('Delete this bark?')">
                                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                                <input type="hidden" name="action" value="delete_bark">
                                <input type="hidden" name="bark_id" value="<?= $r['bark_id'] ?>">
                                <button type="submit" class="btn-del">🗑 Del Bark</button>
                            </form>
                            <?php endif; ?>
                            <form class="action-form" method="POST">
                                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                                <input type="hidden" name="action" value="reviewed_report">
                                <input type="hidden" name="report_id" value="<?= $r['id'] ?>">
                                <button type="submit" class="btn-verify">✓ Reviewed</button>
                            </form>
                            <form class="action-form" method="POST">
                                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                                <input type="hidden" name="action" value="dismiss_report">
                                <input type="hidden" name="report_id" value="<?= $r['id'] ?>">
                                <button type="submit" class="btn-unban">✗ Dismiss</button>
                            </form>
                        </div>
                    </td>
                </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>
        <?php endif; ?>

        <!-- ── LOGS ── -->
        <?php elseif ($tab === 'logs'): ?>
        <div class="admin-section-title">🛡️ Security Logs</div>
        <div class="table-wrap">
            <table class="admin-table">
                <thead><tr><th>Time</th><th>User</th><th>IP</th><th>Action</th><th>Details</th></tr></thead>
                <tbody>
                <?php foreach ($logs as $log): ?>
                <tr>
                    <td style="color:var(--text-muted);font-size:0.8rem;white-space:nowrap"><?= date('d/m H:i:s', strtotime($log['created_at'])) ?></td>
                    <td><?= $log['username'] ? '@'.htmlspecialchars($log['username']) : '<span style="color:var(--text-muted)">—</span>' ?></td>
                    <td style="font-family:monospace;font-size:0.78rem;color:var(--text-muted)"><?= htmlspecialchars($log['ip']) ?></td>
                    <td><span class="log-action"><?= htmlspecialchars($log['action']) ?></span></td>
                    <td style="font-size:0.8rem;color:var(--text-secondary);max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">
                        <?= htmlspecialchars(mb_substr($log['details']??'',0,80)) ?>
                    </td>
                </tr>
                <?php endforeach; ?>
                </tbody>
            </table>
        </div>
        <?php endif; ?>
    </main>
</div>

<script>
// Auto-refresh dashboard stats every 60s
<?php if ($tab === 'dashboard'): ?>
setTimeout(() => location.reload(), 60000);
<?php endif; ?>

function showToast(msg, type='success') {
    const t = document.createElement('div');
    t.style.cssText='position:fixed;bottom:20px;right:20px;background:var(--bg-card);border:1px solid var(--border);padding:12px 18px;border-radius:12px;z-index:9999;font-size:0.9rem;box-shadow:var(--shadow)';
    t.textContent = msg;
    document.body.appendChild(t);
    setTimeout(()=>t.remove(),3000);
}
</script>
</body>
</html>
