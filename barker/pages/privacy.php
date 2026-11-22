<?php
require_once dirname(__DIR__) . '/includes/init.php';
$pageTitle = 'Privacy Policy';
if (is_logged_in()) include __DIR__ . '/../includes/header.php';
?>
<?php if (!is_logged_in()): ?>
<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
    <title>Privacy — BARKER</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Bebas+Neue&family=Space+Grotesk:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
</head>
<body>
<div style="max-width:760px;margin:0 auto;padding:40px 20px">
    <a href="<?= SITE_URL ?>/index.php" style="font-family:'Bebas Neue',sans-serif;font-size:1.8rem;color:var(--orange);text-decoration:none;letter-spacing:2px"><i class="fa-solid fa-dog"></i> BARKER</a>
<?php endif; ?>

<div style="<?= is_logged_in() ? 'padding:32px;max-width:760px' : '' ?>">
    <h1 style="font-family:'Bebas Neue',sans-serif;font-size:2rem;letter-spacing:2px;margin:24px 0 8px">🔒 Privacy Policy</h1>
    <p style="color:var(--text-muted);font-size:0.85rem;margin-bottom:28px">Last updated: <?= date('F Y') ?></p>

    <?php
    $sections = [
        ['📦 Data We Collect', 'Username, email, display name, gender, IP address at registration. Content you post (barks, yaps, howls, DMs). Actions you take (paws, follows, bones). Device and browser info via standard server logs.'],
        ['<i class="fa-solid fa-bullseye"></i> How We Use It', 'To operate and personalize BARKER. To send notifications. To detect and prevent abuse, spam, and illegal activity. To process Bone (donation) transactions. We do NOT sell your data to advertisers.'],
        ['<i class="fa-solid fa-glasses"></i> Anonymity & Real Identity', 'When you post anonymously, your username is hidden publicly. However, administrators can always view the real identity behind anonymous content for safety and moderation purposes. IP addresses are always logged.'],
        ['🔐 Data Security', 'Passwords are hashed with bcrypt (cost 12). Sessions use HttpOnly cookies. CSRF tokens protect all forms. We use prepared statements to prevent SQL injection. Your data is stored on secured servers.'],
        ['<i class="fa-solid fa-envelope"></i> Barker Mail', 'If you choose to create a Barker Mail address (@barker.ink) during registration, a mailbox is created on our mail server via our hosting provider\'s account management API. We never store your Barker Mail password in our database — it is only used once, at creation time, to set up the mailbox, and afterwards you authenticate directly against the mail server each time you sign in at ' . MAIL_WEBMAIL_URL . '. Emails you send and receive through Barker Mail are stored on our mail server like any standard mailbox, and are subject to the same security and legal-disclosure rules as the rest of your BARKER data.'],
        ['🌍 Data Sharing', 'We do not sell your personal data. We may share anonymized aggregate statistics. We may disclose data to authorities if legally required. Bone payments may involve third-party payment processors.'],
        ['🗑️ Data Deletion', 'You can delete your account in Settings → Account. Upon deletion, your profile, barks, personal data, and any associated Barker Mail mailbox are permanently removed. Some logs may be retained for legal compliance.'],
        ['🍪 Cookies', 'We use only essential session cookies — no tracking cookies, no advertising cookies. No third-party cookie consent needed.'],
        ['👶 Minors', 'BARKER is not intended for users under 13. If we discover an account belongs to a minor under 13, we will delete it immediately.'],
        ['📬 Contact', 'For privacy concerns: privacy@barker.ink. For data deletion requests, use Settings → Delete Account or email us.'],
    ];
    foreach ($sections as [$title, $text]): ?>
    <div style="margin-bottom:20px;padding:16px;background:var(--bg-card);border:1px solid var(--border);border-radius:12px">
        <h3 style="font-size:1rem;margin-bottom:6px"><?= $title ?></h3>
        <p style="color:var(--text-secondary);font-size:0.9rem;line-height:1.6"><?= $text ?></p>
    </div>
    <?php endforeach; ?>
</div>

<?php if (!is_logged_in()): ?>
</div>
</body>
</html>
<?php else: ?>
<?php include __DIR__ . '/../includes/footer.php'; ?>
<?php endif; ?>
