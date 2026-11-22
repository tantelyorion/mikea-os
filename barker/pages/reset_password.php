<?php
// ============================================================
// BARKER — Reset Password
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';

if (is_logged_in()) redirect(SITE_URL . '/index.php');

$token  = $_GET['token'] ?? '';
$uid    = (int)($_GET['uid'] ?? 0);
$error  = '';
$success = '';
$valid  = false;

// Validate token
if ($token && $uid) {
    $log = DB::fetchOne(
        "SELECT * FROM security_logs
         WHERE user_id = ? AND action = 'password_reset_request'
         ORDER BY created_at DESC LIMIT 1",
        [$uid]
    );

    if ($log) {
        [$storedToken, $expiresAt] = explode('|', $log['details']);
        if (hash_equals($storedToken, $token) && strtotime($expiresAt) > time()) {
            $valid = true;
        } else {
            $error = 'This reset link has expired. Please request a new one.';
        }
    } else {
        $error = 'Invalid reset link.';
    }
}

if ($_SERVER['REQUEST_METHOD'] === 'POST' && $valid) {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        $error = 'Invalid CSRF token.';
    } else {
        $newPass = $_POST['new_password'] ?? '';
        $confirm = $_POST['confirm_password'] ?? '';

        if (strlen($newPass) < 8) {
            $error = 'Password must be at least 8 characters.';
        } elseif ($newPass !== $confirm) {
            $error = 'Passwords do not match.';
        } else {
            DB::execute("UPDATE users SET password = ? WHERE id = ?", [hash_password($newPass), $uid]);
            // Invalidate the token by deleting it
            DB::execute(
                "DELETE FROM security_logs WHERE user_id = ? AND action = 'password_reset_request'",
                [$uid]
            );
            log_security($uid, 'password_reset_completed', '');
            $success = 'Password updated! You can now sign in.';
            $valid = false;
        }
    }
}
?>
<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Reset Password — BARKER</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Bebas+Neue&family=Space+Grotesk:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
</head>
<body>
<div class="auth-page">
    <div class="auth-card">
        <div class="auth-logo"><img src="../assets/icons/logo.png" style="width: auto; height:50px;" > BARKER</div>
        <div class="auth-tagline">Create a new password</div>

        <?php if ($error): ?>
            <div class="alert alert-error"><i class="fa-solid fa-triangle-exclamation"></i> <?= sanitize($error) ?></div>
            <a href="<?= SITE_URL ?>/pages/forgot_password.php" class="btn btn-outline btn-full" style="margin-top:8px">Request new link</a>
        <?php elseif ($success): ?>
            <div class="alert alert-success"><i class="fa-solid fa-check"></i> <?= sanitize($success) ?></div>
            <a href="<?= SITE_URL ?>/pages/login.php" class="btn btn-primary btn-full" style="margin-top:8px">
                <i class="fa-solid fa-right-to-bracket"></i> Sign In
            </a>
        <?php elseif ($valid): ?>
        <h2 class="auth-title" style="margin-bottom:20px">New Password</h2>
        <form method="POST">
            <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
            <div class="form-group">
                <label class="form-label">New Password</label>
                <input type="password" name="new_password" class="form-control" placeholder="Min. 8 characters" required minlength="8" autofocus>
            </div>
            <div class="form-group">
                <label class="form-label">Confirm Password</label>
                <input type="password" name="confirm_password" class="form-control" placeholder="Repeat password" required>
            </div>
            <button type="submit" class="btn btn-primary btn-full">
                <i class="fa-solid fa-lock"></i> Update Password
            </button>
        </form>
        <?php endif; ?>

        <div class="divider"></div>
        <p class="text-center" style="font-size:0.9rem;color:var(--text-secondary)">
            <a href="<?= SITE_URL ?>/pages/login.php">← Back to login</a>
        </p>
    </div>
</div>
</body>
</html>
