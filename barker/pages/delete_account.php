<?php
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

if ($_GET['confirm'] ?? '' === '1') {
    $userId = $_SESSION['user_id'];
    log_security($userId, 'account_deleted', 'User self-deleted');

    // Clean up uploaded files
    $user = current_user();
    if ($user) {
        if ($user['avatar'])      @unlink(__DIR__ . '/../assets/uploads/avatars/' . $user['avatar']);
        if ($user['cover_photo']) @unlink(__DIR__ . '/../assets/uploads/covers/' . $user['cover_photo']);
    }

    // Delete user (cascades to barks, paws, etc.)
    DB::execute("DELETE FROM users WHERE id=?", [$userId]);

    logout_user();
    redirect(SITE_URL . '/pages/login.php');
}

redirect(SITE_URL . '/pages/settings.php?tab=account');
