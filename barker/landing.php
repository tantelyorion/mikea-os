<?php
// ============================================================
// BARKER — Landing Page (non-logged users)
// ============================================================
require_once __DIR__ . '/includes/init.php';

// If logged in, go straight to feed
if (is_logged_in()) redirect(SITE_URL . '/index.php');

// Stats for landing
$stats = DB::fetchOne("
    SELECT
        (SELECT COUNT(*) FROM users WHERE is_banned=0)  AS total_users,
        (SELECT COUNT(*) FROM barks WHERE is_deleted=0) AS total_barks,
        (SELECT COALESCE(SUM(amount),0) FROM bones WHERE status='completed') AS total_donated
");

// Recent public barks preview
$previews = DB::fetchAll("
    SELECT b.content, u.display_name, u.gender, u.avatar, b.is_anonymous
    FROM barks b JOIN users u ON u.id=b.user_id
    WHERE b.is_deleted=0 AND b.is_anonymous=0 AND u.is_banned=0
    ORDER BY b.created_at DESC LIMIT 6
");
?>
<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BARKER — Express yourself. No leash required.</title>
    <meta name="description" content="BARKER is a social network built on authentic expression, anonymity, and direct creator support. Bark freely.">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Bebas+Neue&family=Space+Grotesk:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
    <style>
        .landing-hero {
            min-height: 100vh;
            display: flex; align-items: center; justify-content: center;
            background: radial-gradient(ellipse at 30% 50%, rgba(255,98,0,0.15) 0%, transparent 60%),
                        radial-gradient(ellipse at 80% 20%, rgba(255,98,0,0.08) 0%, transparent 50%),
                        var(--bg);
            text-align: center;
            padding: 40px 20px;
            position: relative; overflow: hidden;
        }
        .hero-dog {
            font-size: 5rem;
            animation: bounce 2s ease-in-out infinite;
            display: block; margin-bottom: 16px;
        }
        @keyframes bounce {
            0%,100% { transform: translateY(0); }
            50%      { transform: translateY(-16px); }
        }
        .hero-logo {
            font-family: 'Bebas Neue', sans-serif;
            font-size: clamp(4rem, 12vw, 9rem);
            color: var(--orange);
            letter-spacing: 6px;
            line-height: 1;
            margin-bottom: 8px;
        }
        .hero-tagline {
            font-size: clamp(1rem, 2.5vw, 1.4rem);
            color: var(--text-secondary);
            margin-bottom: 32px;
            font-weight: 300;
        }
        .hero-actions { display: flex; gap: 12px; justify-content: center; flex-wrap: wrap; margin-bottom: 48px; }
        .stats-strip {
            display: flex; justify-content: center; gap: 40px;
            flex-wrap: wrap; margin-bottom: 60px;
        }
        .stat-pill {
            text-align: center;
        }
        .stat-pill-value { font-family: 'Bebas Neue', sans-serif; font-size: 2rem; color: var(--orange); }
        .stat-pill-label { font-size: 0.8rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 1px; }

        .features-grid {
            display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
            gap: 16px; max-width: 900px; margin: 0 auto 60px;
        }
        .feature-card {
            background: var(--bg-card); border: 1px solid var(--border);
            border-radius: 16px; padding: 24px;
            transition: all 0.2s;
        }
        .feature-card:hover { border-color: var(--orange); background: var(--orange-faint); transform: translateY(-2px); }
        .feature-icon { font-size: 2rem; margin-bottom: 10px; }
        .feature-title { font-weight: 700; font-size: 1rem; margin-bottom: 6px; }
        .feature-desc { font-size: 0.85rem; color: var(--text-secondary); line-height: 1.5; }

        .preview-grid {
            display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
            gap: 12px; max-width: 900px; margin: 0 auto 60px;
        }
        .preview-card {
            background: var(--bg-card); border: 1px solid var(--border);
            border-radius: 14px; padding: 16px;
            text-align: left;
        }
        .preview-header { display: flex; align-items: center; gap: 10px; margin-bottom: 10px; }
        .preview-avatar { width: 36px; height: 36px; border-radius: 50%; object-fit: cover; border: 2px solid var(--border); }
        .preview-name { font-weight: 700; font-size: 0.88rem; }
        .preview-content { font-size: 0.9rem; line-height: 1.5; color: var(--text-secondary); }
        .preview-footer { margin-top: 10px; display: flex; gap: 12px; font-size: 0.8rem; color: var(--text-muted); }

        .landing-footer {
            border-top: 1px solid var(--border); padding: 24px 20px;
            text-align: center; color: var(--text-muted); font-size: 0.85rem;
        }
    </style>
</head>
<body>

<!-- Nav -->
<nav style="position:fixed;top:0;left:0;right:0;z-index:100;padding:14px 24px;display:flex;align-items:center;justify-content:space-between;background:rgba(10,10,10,0.8);backdrop-filter:blur(10px);border-bottom:1px solid var(--border)">
    <div style="font-family:'Bebas Neue',sans-serif;font-size:1.6rem;color:var(--orange);letter-spacing:2px"><i class="fa-solid fa-dog"></i> BARKER</div>
    <div style="display:flex;gap:10px">
        <a href="<?= SITE_URL ?>/pages/login.php" class="btn btn-outline" style="padding:8px 18px;font-size:0.88rem">Sign In</a>
        <a href="<?= SITE_URL ?>/pages/register.php" class="btn btn-primary" style="padding:8px 18px;font-size:0.88rem">Join Free</a>
    </div>
</nav>

<!-- Hero -->
<section class="landing-hero">
    <div>
        <span class="hero-dog"><i class="fa-solid fa-dog"></i></span>
        <div class="hero-logo">BARKER</div>
        <div class="hero-tagline">Express yourself. No leash required.</div>

        <div class="hero-actions">
            <a href="<?= SITE_URL ?>/pages/register.php" class="btn btn-primary" style="padding:14px 32px;font-size:1.05rem">
                <i class="fa-solid fa-paw"></i> Start Barking Free
            </a>
            <a href="<?= SITE_URL ?>/pages/login.php" class="btn btn-outline" style="padding:14px 32px;font-size:1.05rem">
                Sign In
            </a>
        </div>

        <!-- Stats -->
        <div class="stats-strip">
            <div class="stat-pill">
                <div class="stat-pill-value"><?= number_format($stats['total_users']) ?>+</div>
                <div class="stat-pill-label">Barkers</div>
            </div>
            <div class="stat-pill">
                <div class="stat-pill-value"><?= number_format($stats['total_barks']) ?>+</div>
                <div class="stat-pill-label">Barks</div>
            </div>
            <div class="stat-pill">
                <div class="stat-pill-value">€<?= number_format((float)$stats['total_donated']) ?>+</div>
                <div class="stat-pill-label">Bones Given</div>
            </div>
        </div>
    </div>
</section>

<!-- Features -->
<section style="padding:60px 20px;background:var(--bg-card);border-top:1px solid var(--border)">
    <div style="text-align:center;margin-bottom:36px">
        <div style="font-family:'Bebas Neue',sans-serif;font-size:2rem;letter-spacing:2px">Why BARKER?</div>
        <div style="color:var(--text-muted)">Built different. Barking since day one.</div>
    </div>

    <div class="features-grid">
        <?php
        $features = [
            ['fa-solid fa-dog','Bark','Post up to 300 chars + 4 media. Express yourself instantly.'],
            ['fa-solid fa-glasses','Anonymity','Post anonymously when you need to. Real identity stays private.'],
            ['fa-solid fa-bone','Bones','Send direct donations to creators you love. No middleman.'],
            ['fa-solid fa-ban','No Fake Engagement','Can\'t like your own barks. No bots. Pure authenticity.'],
            ['fa-solid fa-volume-high','Howl','Quote and react to barks with your own commentary.'],
            ['fa-solid fa-fire','Trending','Real-time trending barks and hashtags, powered by the pack.'],
            ['fa-solid fa-comment','Yap','Reply directly to any bark. Build real conversations.'],
            ['fa-solid fa-repeat','Rebark','Share what resonates. Amplify voices you believe in.'],
        ];
        foreach ($features as [$icon, $title, $desc]): ?>
        <div class="feature-card">
            <div class="feature-icon"><i class="<?= $icon ?>"></i></div>
            <div class="feature-title"><?= $title ?></div>
            <div class="feature-desc"><?= $desc ?></div>
        </div>
        <?php endforeach; ?>
    </div>
</section>

<!-- Live preview barks -->
<?php if (!empty($previews)): ?>
<section style="padding:60px 20px">
    <div style="text-align:center;margin-bottom:32px">
        <div style="font-family:'Bebas Neue',sans-serif;font-size:2rem;letter-spacing:2px"><i class="fa-solid fa-paw"></i> Live Barks</div>
        <div style="color:var(--text-muted)">What the pack is barking about right now</div>
    </div>
    <div class="preview-grid">
        <?php foreach ($previews as $b): ?>
        <div class="preview-card">
            <div class="preview-header">
                <?php
                $avatar = $b['is_anonymous'] ? SITE_URL . '/assets/default_avatars/' . ($b['gender']??'other') . '.png' : get_user_avatar($b);
                ?>
                <img src="<?= $avatar ?>" class="preview-avatar" alt="">
                <div class="preview-name"><?= $b['is_anonymous'] ? '<i class="fa-solid fa-glasses"></i> Anonymous' : htmlspecialchars($b['display_name']) ?></div>
            </div>
            <div class="preview-content"><?= htmlspecialchars(mb_substr($b['content'], 0, 140)) ?><?= mb_strlen($b['content']) > 140 ? '...' : '' ?></div>
            <div class="preview-footer">
                <span><i class="fa-solid fa-paw"></i> Paw</span>
                <span><i class="fa-solid fa-comment"></i> Yap</span>
                <span><i class="fa-solid fa-bone"></i> Bone</span>
            </div>
        </div>
        <?php endforeach; ?>
    </div>
    <div style="text-align:center">
        <a href="<?= SITE_URL ?>/pages/register.php" class="btn btn-primary" style="padding:14px 32px;font-size:1rem">
            Join the pack <i class="fa-solid fa-paw"></i>
        </a>
    </div>
</section>
<?php endif; ?>

<!-- CTA -->
<section style="padding:80px 20px;background:var(--orange);text-align:center">
    <div style="font-family:'Bebas Neue',sans-serif;font-size:clamp(2rem,6vw,4rem);color:#fff;letter-spacing:4px;margin-bottom:12px">
        READY TO BARK?
    </div>
    <div style="color:rgba(255,255,255,0.85);font-size:1.1rem;margin-bottom:28px">
        Join the pack. No leash. No filters. Just you.
    </div>
    <a href="<?= SITE_URL ?>/pages/register.php" class="btn" style="background:#fff;color:var(--orange);padding:16px 40px;font-size:1.1rem;border-radius:14px;font-weight:700;box-shadow:0 8px 24px rgba(0,0,0,0.2)">
        <i class="fa-solid fa-paw"></i> Create Free Account
    </a>
</section>

<!-- Footer -->
<footer class="landing-footer">
    <div style="margin-bottom:12px">
        <a href="<?= SITE_URL ?>/pages/terms.php" style="color:var(--text-muted);margin:0 10px">Terms</a>
        <a href="<?= SITE_URL ?>/pages/privacy.php" style="color:var(--text-muted);margin:0 10px">Privacy</a>
        <a href="<?= SITE_URL ?>/pages/login.php" style="color:var(--orange);margin:0 10px;font-weight:700">Sign In</a>
        <a href="<?= SITE_URL ?>/pages/register.php" style="color:var(--orange);margin:0 10px;font-weight:700">Join</a>
    </div>
    <i class="fa-solid fa-dog"></i> BARKER © <?= date('Y') ?> · No leash required.
</footer>

<script>
// Simple theme toggle for landing
document.addEventListener('DOMContentLoaded', () => {
    const saved = localStorage.getItem('barker_theme') || 'dark';
    document.documentElement.setAttribute('data-theme', saved);
});
</script>
</body>
</html>
