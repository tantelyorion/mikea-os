<?php
// Attendu avant inclusion : $pageTitle (string), $activeFolder (string, nom brut du dossier), $folders (array via ml_list_folders)
$__ml_settings = ml_get_settings();
$__ml_theme = ($__ml_settings['theme'] ?? 'dark') === 'light' ? 'light' : 'dark';
$__ml_current_script = basename($_SERVER['SCRIPT_NAME'] ?? '');
?>
<!DOCTYPE html>
<html lang="fr" data-theme="<?= ml_safe($__ml_theme) ?>">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
    <title><?= ml_safe($pageTitle ?? 'Barker Mail') ?></title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:opsz,wght@14..32,400;14..32,500;14..32,600;14..32,700;14..32,800&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="css/mail.css">
    <script src="js/mail.js" defer></script>
</head>
<body>

<div class="ml-topbar">
    <button type="button" class="ml-icon-btn ml-menu-toggle" id="mlMenuToggle" aria-label="Ouvrir le menu" title="Menu">
        <i class="fa-solid fa-bars"></i>
    </button>
    <a href="<?= SITE_URL ?>/index.php" class="ml-back" title="Retour à BARKER"><i class="fa-solid fa-arrow-left"></i></a>
    <div class="ml-logo"><i class="fa-solid fa-envelope-open-text"></i> <span>Barker Mail</span></div>
    <div class="ml-spacer"></div>
    <a href="theme_toggle.php" class="ml-icon-btn ml-theme-btn" title="<?= $__ml_theme === 'dark' ? 'Passer en mode clair' : 'Passer en mode sombre' ?>">
        <i class="fa-solid <?= $__ml_theme === 'dark' ? 'fa-sun' : 'fa-moon' ?>"></i>
    </a>
    <a href="settings.php" class="ml-icon-btn ml-settings-btn <?= $__ml_current_script === 'settings.php' ? 'active' : '' ?>" title="Paramètres">
        <i class="fa-solid fa-gear"></i>
    </a>
    <div class="ml-account" title="<?= ml_safe(ml_current_email()) ?>"><i class="fa-solid fa-circle-user"></i> <span class="ml-account-email"><?= ml_safe(ml_current_email()) ?></span></div>
    <a href="logout.php" class="ml-logout" title="Déconnexion"><i class="fa-solid fa-right-from-bracket"></i><span class="ml-logout-label"> Déconnexion</span></a>
</div>

<div class="ml-layout">
    <div class="ml-sidebar-overlay" id="mlSidebarOverlay"></div>
    <aside class="ml-sidebar" id="mlSidebar">
        <a href="compose.php" class="ml-compose-btn"><i class="fa-solid fa-pen"></i> Nouveau message</a>

        <nav class="ml-folder-nav">
            <?php foreach (($folders ?? []) as $f): ?>
                <a href="index.php?folder=<?= urlencode($f['raw']) ?>"
                   class="ml-folder <?= (($activeFolder ?? 'INBOX') === $f['raw'] && $__ml_current_script !== 'settings.php') ? 'active' : '' ?>">
                    <i class="fa-solid <?= ml_safe($f['icon']) ?>"></i>
                    <span><?= ml_safe($f['label']) ?></span>
                </a>
            <?php endforeach; ?>
        </nav>

        <div class="ml-sidebar-footer">
            <a href="settings.php" class="ml-folder <?= $__ml_current_script === 'settings.php' ? 'active' : '' ?>">
                <i class="fa-solid fa-gear"></i>
                <span>Paramètres</span>
            </a>
            <a href="logout.php" class="ml-folder ml-folder-logout">
                <i class="fa-solid fa-right-from-bracket"></i>
                <span>Déconnexion</span>
            </a>
        </div>
    </aside>
