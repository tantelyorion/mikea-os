<?php
require_once dirname(__DIR__) . '/includes/init.php';
$pageTitle = 'Terms of Service';
if (is_logged_in()) include __DIR__ . '/../includes/header.php';
?>
<!DOCTYPE html>
<?php if (!is_logged_in()): ?>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
    <title>Terms — BARKER</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Bebas+Neue&family=Space+Grotesk:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
</head>
<body>
<div style="max-width:760px;margin:0 auto;padding:40px 20px">
    <a href="<?= SITE_URL ?>/index.php" style="font-family:'Bebas Neue',sans-serif;font-size:1.8rem;color:var(--orange);text-decoration:none;letter-spacing:2px"><i class="fa-solid fa-dog"></i> BARKER</a>
<?php endif; ?>

<div style="<?= is_logged_in() ? 'padding:32px;max-width:760px' : '' ?>">
    <h1 style="font-family:'Bebas Neue',sans-serif;font-size:2rem;letter-spacing:2px;margin:24px 0 8px">📜 Terms of Service</h1>
    <p style="color:var(--text-muted);font-size:0.85rem;margin-bottom:28px">Last updated: <?= date('F Y') ?></p>

    <?php
    $sections = [
        ['<i class="fa-solid fa-paw"></i> Acceptance', 'By creating a BARKER account or using the platform, you agree to these Terms. If you disagree, please do not use BARKER.'],
        ['👤 Accounts', 'You must provide accurate information. You are responsible for all activity on your account. You must be at least 13 years old. One account per person.'],
        ['<i class="fa-solid fa-dog"></i> Barks & Content', 'You retain ownership of your content. By posting, you grant BARKER a license to display it. You may not post: illegal content, hate speech, harassment, spam, malware, or content violating third-party rights.'],
        ['<i class="fa-solid fa-glasses"></i> Anonymity', 'The anonymous posting feature is for privacy, not impersonation or harm. Admins can see real identities. Abuse of anonymity will result in a ban.'],
        ['<i class="fa-solid fa-bone"></i> Bones (Donations)', 'Bones are direct user-to-user donations. BARKER is not responsible for payment disputes. No refunds once sent. Do not use Bones for illegal transactions.'],
        ['<i class="fa-solid fa-envelope"></i> Barker Mail', 'You may optionally create a Barker Mail mailbox (@barker.ink) when registering, or later in Settings. Barker Mail is provided for personal, lawful use only — you may not use it to send spam, malware, phishing, or any illegal content. We may suspend a Barker Mail mailbox that violates these Terms independently of your BARKER account. Mailbox storage is limited by the quota shown at creation time.'],
        ['<i class="fa-solid fa-ban"></i> Prohibited Conduct', 'No spam, no bots, no self-engagement (Barker auto-blocks this), no impersonation, no scraping, no illegal activity, no harassment.'],
        ['🔒 Privacy', 'See our Privacy Policy for how we handle your data. We log IPs for security. Anonymous content is stored with real identity visible only to admins.'],
        ['<i class="fa-solid fa-triangle-exclamation"></i> Termination', 'We may suspend or ban accounts violating these Terms at any time, with or without notice. Deleting or banning a BARKER account may also result in the deletion of its associated Barker Mail mailbox.'],
        ['📋 Changes', 'We may update these Terms. Continued use after changes means acceptance.'],
        ['📧 Contact', 'Questions? Contact us at support@barker.ink'],
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
