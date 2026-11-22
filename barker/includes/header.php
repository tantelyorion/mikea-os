<?php
// ============================================================
// BARKER — Header Template - VERSION MOBILE NATIVE AVEC TRADUCTION
// ============================================================
if (!defined('SITE_URL')) {
    require_once dirname(__DIR__) . '/includes/init.php';
}

$currentUser   = current_user();
$notifCount    = get_unread_notifications_count();
$msgCount      = get_unread_messages_count();
$userTheme     = $_SESSION['theme'] ?? 'dark';
$currentPath   = $_SERVER['PHP_SELF'] ?? '';
$textDirection = get_text_direction();
?>
<!DOCTYPE html>
<html lang="<?= get_current_language() ?>" data-theme="<?= htmlspecialchars($userTheme) ?>" dir="<?= $textDirection ?>">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover, user-scalable=no">
    <meta name="site-url" content="<?= SITE_URL ?>">
    <title><?= isset($pageTitle) ? htmlspecialchars($pageTitle) . ' — ' : '' ?>BARKER</title>
    <meta name="description" content="BARKER — <?= __('feed_home') ?>">
    
    <!-- Favicon -->
    <link rel="icon" type="image/png" href="<?= SITE_URL ?>/assets/icons/favicon.png">
    <link rel="apple-touch-icon" href="<?= SITE_URL ?>/assets/icons/logo.png">
    
    <!-- Font Awesome 6 -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    
    <!-- Google Fonts -->
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Bebas+Neue&family=Space+Grotesk:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    
    <!-- Barker CSS -->
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
	 <link rel="stylesheet" href="<?= SITE_URL ?>/css/desac.css">
    
    <!-- RTL Support -->
    <?php if ($textDirection === 'rtl'): ?>
    <style>
        body { direction: rtl; text-align: right; }
        .bark-actions, .bark-header, .feed-tabs, .compose-options { direction: ltr; }
        .sidebar-left, .nav-item, .dropdown { direction: rtl; text-align: right; }
        .nav-item .nav-icon { margin-left: 8px; margin-right: 0; }
        .nav-badge { margin-left: 0; margin-right: auto; }
        .bark-more { margin-left: 0; margin-right: auto; }
        .compose-submit { margin-left: 0; margin-right: auto; }
    </style>
    <?php endif; ?>
    
    <!-- Styles additionnels pour le logo et mobile -->
    <style>
        /* Logo styling */
        .sidebar-logo {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 12px 16px;
            margin-bottom: 8px;
            transition: all 0.2s ease;
        }
        
        <?php if ($textDirection === 'rtl'): ?>
        .sidebar-logo {
            justify-content: flex-end;
        }
        <?php endif; ?>
        
        .sidebar-logo:hover {
            opacity: 0.85;
        }
        
        .logo-image {
            width: 49px;
            height: 49px;
            object-fit: contain;
            filter: drop-shadow(0 2px 4px rgba(0,0,0,0.1));
            transition: transform 0.2s ease;
        }
        
        .sidebar-logo:hover .logo-image {
            transform: scale(1.05);
        }
        
        .logo-text {
            font-family: 'Bebas Neue', cursive;
            font-size: 1.8rem;
            font-weight: 700;
            letter-spacing: 2px;
            color: var(--text-primary);
            text-shadow: 2px 2px 4px rgba(0,0,0,0.1);
        }
        
        [data-theme="light"] .logo-text {
            text-shadow: 2px 2px 4px rgba(0,0,0,0.05);
        }
        
        .sidebar-logo {
            animation: logoFloat 3s ease-in-out infinite;
        }
        
        .sidebar-logo:hover {
            animation: none;
        }
        
        @keyframes logoFloat {
            0%, 100% { transform: translateY(0); }
            50% { transform: translateY(-2px); }
        }
        
        /* Badge de notification amélioré */
        .nav-badge {
            background: var(--orange);
            color: white;
            font-size: 0.7rem;
            font-weight: 700;
            padding: 2px 6px;
            border-radius: 12px;
            min-width: 20px;
            text-align: center;
            margin-left: auto;
            box-shadow: 0 2px 4px rgba(255, 159, 28, 0.3);
            transition: transform 0.15s ease;
        }
        
        .nav-item:hover .nav-badge {
            transform: scale(1.1);
        }
        
        .sidebar-bark-btn {
            width: calc(100% - 16px);
            margin: 16px 8px;
            padding: 14px;
            background: var(--orange);
            color: white;
            border: none;
            border-radius: 30px;
            font-size: 1.1rem;
            font-weight: 700;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            transition: all 0.3s ease;
            box-shadow: 0 4px 12px rgba(255, 159, 28, 0.3);
            position: relative;
            overflow: hidden;
        }
        
        .sidebar-bark-btn::before {
            content: '';
            position: absolute;
            top: 50%;
            left: 50%;
            width: 0;
            height: 0;
            border-radius: 50%;
            background: rgba(255, 255, 255, 0.3);
            transform: translate(-50%, -50%);
            transition: width 0.6s, height 0.6s;
        }
        
        .sidebar-bark-btn:hover::before {
            width: 300px;
            height: 300px;
        }
        
        .sidebar-bark-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(255, 159, 28, 0.4);
        }
        
        .sidebar-bark-btn:active {
            transform: translateY(0);
        }
        
        .sidebar-user {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 12px 16px;
            margin: 8px;
            border-radius: 40px;
            transition: all 0.2s ease;
            background: transparent;
        }
        
        .sidebar-user:hover {
            background: var(--bg-card2);
        }
        
        .sidebar-user-avatar {
            width: 44px;
            height: 44px;
            border-radius: 50%;
            object-fit: cover;
            border: 2px solid var(--orange);
            transition: transform 0.2s ease;
        }
        
        .sidebar-user:hover .sidebar-user-avatar {
            transform: scale(1.05);
        }
        
        .nav-item {
            position: relative;
            overflow: hidden;
        }
        
        .nav-item::before {
            content: '';
            position: absolute;
            left: 0;
            top: 0;
            bottom: 0;
            width: 3px;
            background: var(--orange);
            transform: scaleY(0);
            transition: transform 0.2s ease;
        }
        
        <?php if ($textDirection === 'rtl'): ?>
        .nav-item::before {
            left: auto;
            right: 0;
        }
        <?php endif; ?>
        
        .nav-item:hover::before,
        .nav-item.active::before {
            transform: scaleY(1);
        }
        
        .nav-item .nav-icon {
            transition: transform 0.2s ease;
        }
        
        .nav-item:hover .nav-icon {
            transform: translateX(3px);
        }
        
        <?php if ($textDirection === 'rtl'): ?>
        .nav-item:hover .nav-icon {
            transform: translateX(-3px);
        }
        <?php endif; ?>
        
        /* ============================================================
           MOBILE BOTTOM NAVIGATION BAR (STYLE APP NATIVE)
        ============================================================ */
        
        /* Mobile bottom navigation - caché par défaut sur desktop */
        .mobile-bottom-nav {
            display: none;
            position: fixed;
            bottom: 0;
            left: 0;
            right: 0;
            background: var(--bg-card);
            backdrop-filter: blur(20px);
            background: rgba(var(--bg-rgb), 0.95);
            border-top: 1px solid var(--border);
            padding: 8px 12px 20px;
            padding-bottom: env(safe-area-inset-bottom, 20px);
            z-index: 1000;
            justify-content: space-around;
            align-items: center;
            box-shadow: 0 -4px 20px rgba(0, 0, 0, 0.1);
        }
        
        .mobile-nav-item {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            gap: 4px;
            padding: 8px 12px;
            border-radius: 12px;
            color: var(--text-muted);
            text-decoration: none;
            transition: all 0.2s ease;
            position: relative;
            flex: 1;
            max-width: 70px;
        }
        
        .mobile-nav-item .nav-icon {
            font-size: 1.5rem;
            transition: transform 0.2s ease;
        }
        
        .mobile-nav-item .nav-label {
            font-size: 0.65rem;
            font-weight: 500;
        }
        
        .mobile-nav-item.active {
            color: var(--orange);
        }
        
        .mobile-nav-item.active .nav-icon {
            transform: translateY(-2px);
        }
        
        .mobile-nav-item:active {
            transform: scale(0.95);
        }
        
        /* Badge sur mobile */
        .mobile-nav-badge {
            position: absolute;
            top: 2px;
            right: 8px;
            background: var(--orange);
            color: white;
            font-size: 0.6rem;
            font-weight: 700;
            padding: 2px 5px;
            border-radius: 10px;
            min-width: 16px;
            text-align: center;
        }
        
        /* Bouton BARK flottant sur mobile */
        .mobile-bark-fab {
            display: none;
            position: fixed;
            bottom: 80px;
            right: 20px;
            width: 56px;
            height: 56px;
            border-radius: 28px;
            background: var(--orange);
            color: white;
            border: none;
            box-shadow: 0 4px 12px rgba(255, 98, 0, 0.4);
            cursor: pointer;
            z-index: 1001;
            align-items: center;
            justify-content: center;
            font-size: 1.5rem;
            transition: all 0.3s ease;
        }
        
        .mobile-bark-fab:active {
            transform: scale(0.92);
        }
        
        /* Masquer la sidebar gauche sur mobile */
        @media (max-width: 768px) {
            .sidebar-left {
                display: none !important;
            }
            
            .mobile-bottom-nav {
                display: flex;
            }
            
            .mobile-bark-fab {
                display: flex;
            }
            
            .app-layout {
                grid-template-columns: 1fr !important;
                padding-bottom: 70px;
            }
            
            .main-feed {
                border-right: none !important;
                padding-bottom: 20px;
            }
            
            .sidebar-right {
                display: none !important;
            }
            
            .feed-header {
                position: sticky;
                top: 0;
                z-index: 100;
                background: var(--bg);
                backdrop-filter: blur(10px);
            }
        }
        
        /* Pour les très petits écrans */
        @media (max-width: 480px) {
            .mobile-nav-item .nav-label {
                font-size: 0.6rem;
            }
            
            .mobile-nav-item .nav-icon {
                font-size: 1.3rem;
            }
            
            .mobile-nav-badge {
                font-size: 0.55rem;
                padding: 1px 4px;
                min-width: 14px;
                top: 4px;
                right: 4px;
            }
        }
        
        /* Support iPhone notch */
        @supports (padding-bottom: env(safe-area-inset-bottom)) {
            .mobile-bottom-nav {
                padding-bottom: max(20px, env(safe-area-inset-bottom));
            }
        }
    </style>
    
    <?php if (isset($extraCss)) echo $extraCss; ?>
</head>
<body>
<?php if (is_logged_in()): ?>
<div class="app-layout">
    <!-- ── Sidebar Left (Desktop) ───────────────────────────────── -->
    <aside class="sidebar-left">
        <a href="<?= SITE_URL ?>/index.php" class="sidebar-logo" style="text-decoration:none">
            <img src="<?= SITE_URL ?>/assets/icons/logo.png" 
                 alt="BARKER Logo" 
                 class="logo-image"
                 onerror="this.style.display='none'; this.nextElementSibling.style.display='block';">
            <span class="logo-icon" style="display: none;"><i class="fa-solid fa-dog"></i></span>
            <span class="logo-text">BARKER</span>
        </a>

        <?php
        $navItems = [
            ['icon' => 'fa-house', 'label' => __('nav_home'), 'url' => SITE_URL . '/index.php', 'path' => '/index.php'],
            ['icon' => 'fa-magnifying-glass', 'label' => __('nav_search'), 'url' => '#', 'path' => '/pages/search.php', 'onclick' => 'openSearchModal()'],
            ['icon' => 'fa-bell', 'label' => __('nav_notifications'), 'url' => SITE_URL . '/pages/notifications.php', 'path' => '/pages/notifications.php', 'badge' => $notifCount],
            ['icon' => 'fa-envelope', 'label' => __('nav_messages'), 'url' => SITE_URL . '/pages/messages.php', 'path' => '/pages/messages.php', 'badge' => $msgCount],
            ['icon' => 'fa-fire', 'label' => __('nav_trending'), 'url' => SITE_URL . '/pages/trending.php', 'path' => '/pages/trending.php'],
            /*['icon' => 'fa-bone', 'label' => __('nav_my_bones'), 'url' => SITE_URL . '/pages/my-bones.php', 'path' => '/pages/my-bones.php'],*/
            ['icon' => 'fa-bookmark', 'label' => __('nav_bury'), 'url' => SITE_URL . '/pages/my-buries.php', 'path' => '/pages/my-buries.php'],
            ['icon' => 'fa-gear', 'label' => __('nav_settings'), 'url' => SITE_URL . '/pages/settings.php', 'path' => '/pages/settings.php'],
        ];

        $navItems[] = [
            'icon'    => 'fa-grip',
            'label'   => 'My App',
            'url'     => '#',
            'path'    => '__my_apps__',
            'onclick' => "document.getElementById('myAppsModal').classList.add('active')",
        ];

        foreach ($navItems as $nav):
            $active = false;
            
            if ($nav['icon'] === 'fa-magnifying-glass') {
                $active = strpos($currentPath, '/pages/search.php') !== false;
            } else {
                $active = !empty($nav['path']) && $nav['path'] !== '__external_mail__' && $nav['path'] !== '__my_apps__' && strpos($currentPath, $nav['path']) !== false;
            }
            
            $activeClass = $active ? 'active' : '';
            $onclick = isset($nav['onclick']) ? 'onclick="' . $nav['onclick'] . '; return false;"' : '';
            $targetAttr = isset($nav['target']) ? 'target="' . htmlspecialchars($nav['target']) . '" rel="noopener"' : '';
        ?>
        <a href="<?= $nav['url'] ?>" class="nav-item <?= $activeClass ?>" style="text-decoration:none" <?= $onclick ?> <?= $targetAttr ?>>
            <span class="nav-icon"><i class="fa-solid <?= $nav['icon'] ?>"></i></span>
            <span class="nav-label"><?= $nav['label'] ?></span>
            
            <?php if (!empty($nav['badge']) && $nav['badge'] > 0): ?>
                <span class="nav-badge"><?= $nav['badge'] > 99 ? '99+' : $nav['badge'] ?></span>
            <?php endif; ?>
        </a>
        <?php endforeach; ?>

        <button class="sidebar-bark-btn" onclick="openBarkModal()">
            <i class="fa-solid fa-paw"></i> <?= __('btn_bark') ?>
        </button>

        <div class="sidebar-spacer"></div>

        <button class="theme-toggle" id="themeToggle" onclick="toggleTheme()">
            <i class="fa-solid <?= $userTheme === 'dark' ? 'fa-sun' : 'fa-moon' ?>" id="themeIcon"></i>
            <span id="themeLabel"><?= $userTheme === 'dark' ? __('settings_light_mode') : __('settings_dark_mode') ?></span>
        </button>

        <?php if ($currentUser): ?>
        <a href="<?= SITE_URL ?>/pages/profile.php?u=<?= htmlspecialchars($currentUser['username']) ?>" class="sidebar-user" style="text-decoration:none">
            <img src="<?= get_user_avatar($currentUser) ?>" class="sidebar-user-avatar" alt="avatar">
            <div class="sidebar-user-info">
                <div class="sidebar-user-name"><?= htmlspecialchars($currentUser['display_name']) ?></div>
                <div class="sidebar-user-handle">@<?= htmlspecialchars($currentUser['username']) ?></div>
            </div>
            <!--i class="fa-solid fa-ellipsis" style="color:var(--text-muted)"></i-->
        </a>
        <?php endif; ?>
    </aside>

    <!-- ── Mobile Bottom Navigation Bar (Style App Natif) ── -->
    <nav class="mobile-bottom-nav">
        <?php
        $mobileNavItems = [
            ['icon' => 'fa-house', 'label' => __('nav_home'), 'url' => SITE_URL . '/index.php', 'path' => '/index.php'],
            ['icon' => 'fa-magnifying-glass', 'label' => __('nav_search'), 'url' => '#', 'path' => '/pages/search.php', 'onclick' => 'openSearchModal()'],
            ['icon' => 'fa-bell', 'label' => __('nav_notifications'), 'url' => SITE_URL . '/pages/notifications.php', 'path' => '/pages/notifications.php', 'badge' => $notifCount],
            ['icon' => 'fa-envelope', 'label' => __('nav_messages'), 'url' => SITE_URL . '/pages/messages.php', 'path' => '/pages/messages.php', 'badge' => $msgCount],
            ['icon' => 'fa-user', 'label' => __('profile_edit'), 'url' => SITE_URL . '/pages/profile.php?u=' . ($currentUser['username'] ?? ''), 'path' => '/pages/profile.php'],
        ];
        
        foreach ($mobileNavItems as $nav):
            $active = false;
            
            if ($nav['icon'] === 'fa-magnifying-glass') {
                $active = strpos($currentPath, '/pages/search.php') !== false;
            } elseif ($nav['icon'] === 'fa-user') {
                $active = strpos($currentPath, '/pages/profile.php') !== false;
            } else {
                $active = !empty($nav['path']) && strpos($currentPath, $nav['path']) !== false;
            }
            
            $activeClass = $active ? 'active' : '';
            $onclick = isset($nav['onclick']) ? 'onclick="' . $nav['onclick'] . '; return false;"' : '';
        ?>
        <a href="<?= $nav['url'] ?>" class="mobile-nav-item <?= $activeClass ?>" <?= $onclick ?>>
            <span class="nav-icon"><i class="fa-solid <?= $nav['icon'] ?>"></i></span>
            <span class="nav-label"><?= $nav['label'] ?></span>
            <?php if (!empty($nav['badge']) && $nav['badge'] > 0): ?>
                <span class="mobile-nav-badge"><?= $nav['badge'] > 99 ? '99+' : $nav['badge'] ?></span>
            <?php endif; ?>
        </a>
        <?php endforeach; ?>
    </nav>

    <!-- ── Floating Action Button pour BARK sur mobile ── -->
    <button class="mobile-bark-fab" onclick="openBarkModal()">
        <i class="fa-solid fa-paw"></i>
    </button>

<?php endif; ?> 