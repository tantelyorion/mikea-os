<?php
// ============================================================
// BARKER — Forgot Password
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';

if (is_logged_in()) redirect(SITE_URL . '/index.php');

$success = '';
$error   = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        $error = 'Invalid CSRF token.';
    } else {
        $email = trim($_POST['email'] ?? '');

        if (!filter_var($email, FILTER_VALIDATE_EMAIL)) {
            $error = 'Please enter a valid email address.';
        } else {
            $user = DB::fetchOne("SELECT id, display_name FROM users WHERE email = ?", [$email]);

            if ($user) {
                $token     = bin2hex(random_bytes(32));
                $expiresAt = date('Y-m-d H:i:s', strtotime('+1 hour'));

                DB::execute(
                    "INSERT INTO security_logs (user_id, ip, action, details) VALUES (?,?,?,?)",
                    [$user['id'], get_client_ip(), 'password_reset_request', $token . '|' . $expiresAt]
                );

                $resetLink = SITE_URL . '/pages/reset_password.php?token=' . $token . '&uid=' . $user['id'];

                // ⚠️ DEV ONLY — remove in production and send via PHPMailer
                $success = "Reset link generated! In production this would be emailed.<br>
                            <a href='$resetLink' style='color:#FF6200;font-weight:600'>Click here to reset (dev only)</a>";
            } else {
                // Don't reveal if email exists
                $success = "If this email is registered, you'll receive a reset link shortly.";
            }
        }
    }
}
?>
<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
    <title>Forgot Password — BARKER</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:opsz,wght@14..32,300;14..32,400;14..32,500;14..32,600;14..32,700;14..32,800&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }

        body {
            font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            background: #000000;
            min-height: 100vh;
            color: #e7e9ea;
            -webkit-font-smoothing: antialiased;
        }

        /* ===== SPLIT LAYOUT ===== */
        .forgot-wrapper {
            display: flex;
            width: 100%;
            min-height: 100vh;
        }

        /* LEFT — Branding */
        .brand-panel {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 60px 40px;
            position: relative;
            overflow: hidden;
            background: #000000;
        }

        .brand-panel::before {
            content: '';
            position: absolute;
            top: -20%; left: -20%;
            width: 80%; height: 80%;
            background: radial-gradient(circle, rgba(255, 98, 0, 0.18) 0%, transparent 60%);
            filter: blur(60px);
            pointer-events: none;
        }

        .brand-panel::after {
            content: '';
            position: absolute;
            bottom: -20%; right: -20%;
            width: 80%; height: 80%;
            background: radial-gradient(circle, rgba(255, 140, 66, 0.12) 0%, transparent 60%);
            filter: blur(60px);
            pointer-events: none;
        }

        .brand-content {
            max-width: 480px;
            position: relative;
            z-index: 1;
        }

        .brand-logo-large {
            width: 72px; height: 72px;
            border-radius: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            margin-bottom: 40px;
        }

        .brand-logo-large img {
            width: 100%; height: 100%;
            object-fit: contain;
            filter: drop-shadow(0 8px 24px rgba(255, 98, 0, 0.4));
        }

        .brand-content h1 {
            font-size: 52px;
            font-weight: 800;
            line-height: 1.05;
            letter-spacing: -2px;
            color: #ffffff;
            margin-bottom: 24px;
            background: linear-gradient(135deg, #ffffff 0%, #FF8C42 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }

        .brand-content p {
            font-size: 18px;
            color: #71767b;
            line-height: 1.5;
            max-width: 380px;
        }

        .brand-features {
            margin-top: 48px;
            display: flex;
            flex-direction: column;
            gap: 20px;
        }

        .brand-feature {
            display: flex;
            align-items: center;
            gap: 14px;
            color: #e7e9ea;
            font-size: 15px;
            font-weight: 500;
        }

        .brand-feature i {
            width: 36px; height: 36px;
            border-radius: 50%;
            background: rgba(255, 98, 0, 0.12);
            color: #FF6200;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 15px;
            flex-shrink: 0;
        }

        /* RIGHT — Form */
        .form-panel {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 40px;
            background: #000000;
        }

        .forgot-box {
            width: 100%;
            max-width: 440px;
            animation: fadeIn 0.6s ease-out;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .mobile-logo {
            display: none;
            justify-content: center;
            margin-bottom: 32px;
        }

        .mobile-logo img { height: 48px; width: auto; }

        /* ===== BACK LINK ===== */
        .back-link {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            color: #71767b;
            font-size: 14px;
            text-decoration: none;
            margin-bottom: 32px;
            padding: 6px 12px 6px 8px;
            border-radius: 9999px;
            transition: color 0.2s, background 0.2s;
        }

        .back-link:hover {
            color: #FF6200;
            background: rgba(255, 98, 0, 0.08);
        }

        /* ===== HEADER ===== */
        .forgot-header {
            margin-bottom: 32px;
        }

        .forgot-header .icon-circle {
            width: 56px;
            height: 56px;
            border-radius: 50%;
            background: rgba(255, 98, 0, 0.12);
            color: #FF6200;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 22px;
            margin-bottom: 24px;
        }

        .forgot-header h2 {
            font-size: 32px;
            font-weight: 800;
            color: #ffffff;
            margin-bottom: 10px;
            letter-spacing: -0.8px;
            line-height: 1.1;
        }

        .forgot-header p {
            color: #71767b;
            font-size: 15px;
            line-height: 1.5;
        }

        /* ===== FIELDS (labels fixes au-dessus) ===== */
        .field {
            margin-bottom: 22px;
            position: relative;
        }

        .field-label-top {
            display: block;
            font-size: 13px;
            font-weight: 600;
            color: #a1a5aa;
            margin-bottom: 8px;
            padding-left: 2px;
            letter-spacing: 0.1px;
        }

        .field-input-wrap {
            position: relative;
        }

        .field-input {
            width: 100%;
            padding: 14px 16px 14px 48px;
            font-size: 15px;
            font-family: inherit;
            color: #e7e9ea;
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 12px;
            outline: none;
            transition: border-color 0.2s, background 0.2s, box-shadow 0.2s;
            min-height: 52px;
            line-height: 1.4;
        }

        .field-input::placeholder {
            color: #4a4d52;
            font-size: 14px;
        }

        .field-input:hover {
            border-color: #3f4347;
            background: #111;
        }

        .field-input:focus {
            border-color: #FF6200;
            background: #141414;
            box-shadow: 0 0 0 3px rgba(255, 98, 0, 0.12);
        }

        .field-icon {
            position: absolute;
            left: 16px;
            top: 50%;
            transform: translateY(-50%);
            color: #5a5e63;
            font-size: 15px;
            pointer-events: none;
            transition: color 0.2s;
        }

        .field-input:focus ~ .field-icon { color: #FF6200; }

        /* ===== BUTTON ===== */
        .btn-primary {
            width: 100%;
            height: 52px;
            padding: 0 24px;
            font-size: 16px;
            font-weight: 700;
            font-family: inherit;
            color: #ffffff;
            background: #FF6200;
            border: none;
            border-radius: 9999px;
            cursor: pointer;
            transition: background 0.2s, transform 0.1s;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
        }

        .btn-primary:hover { background: #e65800; }
        .btn-primary:active { transform: scale(0.98); background: #cc4e00; }
        .btn-primary:disabled { opacity: 0.7; cursor: not-allowed; }

        /* ===== ALERT ===== */
        .alert {
            padding: 14px 16px;
            border-radius: 12px;
            margin-bottom: 20px;
            font-size: 14px;
            display: flex;
            align-items: flex-start;
            gap: 10px;
            line-height: 1.5;
            animation: fadeIn 0.3s ease;
        }

        .alert-error {
            background: rgba(244, 33, 46, 0.1);
            border: 1px solid rgba(244, 33, 46, 0.3);
            color: #f4212e;
        }

        .alert-success {
            background: rgba(0, 186, 124, 0.1);
            border: 1px solid rgba(0, 186, 124, 0.3);
            color: #00ba7c;
        }

        .alert i { flex-shrink: 0; font-size: 16px; margin-top: 1px; }
        .alert a { color: #FF6200; font-weight: 600; text-decoration: underline; }

        /* ===== SUCCESS STATE ===== */
        .success-state {
            text-align: center;
            padding: 8px 0;
        }

        .success-icon {
            width: 72px;
            height: 72px;
            border-radius: 50%;
            background: rgba(0, 186, 124, 0.12);
            color: #00ba7c;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 30px;
            margin: 0 auto 24px;
            animation: pop 0.4s ease-out;
        }

        @keyframes pop {
            0% { transform: scale(0); }
            70% { transform: scale(1.1); }
            100% { transform: scale(1); }
        }

        /* ===== FOOTER ===== */
        .forgot-footer {
            margin-top: 32px;
            text-align: center;
            font-size: 14px;
            color: #71767b;
            line-height: 1.6;
        }

        .forgot-footer a {
            color: #FF6200;
            font-weight: 600;
            text-decoration: none;
        }

        .forgot-footer a:hover { text-decoration: underline; }

        /* ===== RESPONSIVE ===== */
        @media (max-width: 900px) {
            .brand-panel { display: none; }
            .form-panel {
                padding: 24px;
                min-height: 100vh;
                align-items: flex-start;
                padding-top: 48px;
            }
            .mobile-logo { display: flex; }
            .forgot-header h2 { font-size: 26px; }
        }

        @media (max-width: 480px) {
            .form-panel { padding: 20px 16px; padding-top: 32px; }
            .back-link { margin-bottom: 20px; }
            .forgot-header { margin-bottom: 24px; }
            .forgot-header h2 { font-size: 22px; }
        }
    </style>
</head>
<body>

<div class="forgot-wrapper">

    <!-- ===== LEFT: BRANDING ===== -->
    <div class="brand-panel">
        <div class="brand-content">
            <div class="brand-logo-large">
                <img src="<?= SITE_URL ?>/assets/icons/logo.png" alt="BARKER">
            </div>
            <h1>Forgot your<br>password?</h1>
            <p>No worries — it happens to the best of the pack. We'll help you get back in.</p>

            <div class="brand-features">
                <div class="brand-feature">
                    <i class="fa-solid fa-shield-halved"></i>
                    <span>Secure token-based reset</span>
                </div>
                <div class="brand-feature">
                    <i class="fa-solid fa-clock"></i>
                    <span>Link valid for 1 hour</span>
                </div>
                <div class="brand-feature">
                    <i class="fa-solid fa-envelope"></i>
                    <span>Sent to your registered email</span>
                </div>
            </div>
        </div>
    </div>

    <!-- ===== RIGHT: FORM ===== -->
    <div class="form-panel">
        <div class="forgot-box">

            <div class="mobile-logo">
                <img src="<?= SITE_URL ?>/assets/icons/logo.png" alt="BARKER">
            </div>

            <a href="<?= SITE_URL ?>/pages/login.php" class="back-link">
                <i class="fa-solid fa-arrow-left"></i> Back to login
            </a>

            <?php if ($success): ?>
                <!-- ===== SUCCESS STATE ===== -->
                <div class="success-state">
                    <div class="success-icon">
                        <i class="fa-solid fa-paper-plane"></i>
                    </div>
                    <div class="forgot-header" style="margin-bottom:0">
                        <h2>Check your inbox</h2>
                        <p style="margin-top:10px"><?= $success ?></p>
                    </div>
                </div>

                <div class="forgot-footer">
                    Didn't receive anything? Check your spam folder or
                    <a href="<?= SITE_URL ?>/pages/forgot_password.php">try again</a>.
                </div>
            <?php else: ?>
                <!-- ===== FORM STATE ===== -->
                <div class="forgot-header">
                    <div class="icon-circle">
                        <i class="fa-solid fa-key"></i>
                    </div>
                    <h2>Reset your password</h2>
                    <p>Enter the email address linked to your account and we'll send you a secure reset link.</p>
                </div>

                <?php if ($error): ?>
                    <div class="alert alert-error">
                        <i class="fa-solid fa-triangle-exclamation"></i>
                        <span><?= sanitize($error) ?></span>
                    </div>
                <?php endif; ?>

                <form method="POST" id="forgotForm" autocomplete="off">
                    <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">

                    <div class="field">
                        <label class="field-label-top" for="emailInput">Adresse email</label>
                        <div class="field-input-wrap">
                            <input type="email" name="email" id="emailInput" class="field-input"
                                placeholder="ex: max@barker.ink" required autofocus
                                value="<?= sanitize($_POST['email'] ?? '') ?>">
                            <i class="fa-solid fa-envelope field-icon"></i>
                        </div>
                    </div>

                    <button type="submit" class="btn-primary" id="submitBtn">
                        <i class="fa-solid fa-paper-plane"></i> Envoyer le lien
                    </button>
                </form>

                <div class="forgot-footer">
                    Tu t'en souviens ? <a href="<?= SITE_URL ?>/pages/login.php">Se connecter</a>
                </div>
            <?php endif; ?>

        </div>
    </div>
</div>

<script>
// Loading state on submit
document.getElementById('forgotForm')?.addEventListener('submit', function() {
    const btn = document.getElementById('submitBtn');
    if (btn) {
        btn.disabled = true;
        btn.classList.add('btn-loading');
        btn.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Envoi…';
    }
});

// Auto-focus
document.addEventListener('DOMContentLoaded', function() {
    const email = document.getElementById('emailInput');
    if (email && !email.value) email.focus();
});
</script>
</body>
</html>