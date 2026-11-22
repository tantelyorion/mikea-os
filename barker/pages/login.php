<?php
require_once dirname(__DIR__) . '/includes/init.php';

if (is_logged_in()) redirect(SITE_URL . '/index.php');

$error = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        $error = 'Invalid CSRF token. Please reload and try again.';
    } else {
        $login    = trim($_POST['login'] ?? '');
        $loginNormalized = preg_replace('/[\s\-.()]/', '', $login); // pour comparer proprement un numéro de téléphone saisi avec espaces
        $password = $_POST['password'] ?? '';

        // Anti force-brute : 10 échecs max / 15 min pour une même IP
        $recentFails = DB::fetchOne(
            "SELECT COUNT(*) AS c FROM security_logs WHERE action = 'login_failed' AND ip = ? AND created_at > DATE_SUB(NOW(), INTERVAL 15 MINUTE)",
            [get_client_ip()]
        );
        if ((int)($recentFails['c'] ?? 0) >= 10) {
            $error = 'Trop de tentatives. Réessaie dans quelques minutes.';
        } elseif (empty($login) || empty($password)) {
            $error = 'Please fill in all fields.';
        } else {
            $user = DB::fetchOne(
                "SELECT * FROM users WHERE (email = ? OR username = ? OR phone = ? OR phone = ?) AND is_banned = 0",
                [$login, $login, $login, $loginNormalized]
            );

            if ($user && verify_password($password, $user['password'])) {
                login_user($user);
                log_security($user['id'], 'login', 'Login successful');
                redirect(SITE_URL . '/index.php');
            } else {
                $error = 'Invalid credentials.';
                log_security(null, 'login_failed', 'Tentative échouée');
            }
        }
    }
}

$pageTitle = 'Login';
?>
<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
    <title>Login — BARKER</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:opsz,wght@14..32,300;14..32,400;14..32,500;14..32,600;14..32,700;14..32,800&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            background: #000000;
            min-height: 100vh;
            display: flex;
            color: #e7e9ea;
            -webkit-font-smoothing: antialiased;
        }

        /* ===== SPLIT LAYOUT ===== */
        .login-wrapper {
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
            text-align: left;
        }

        .brand-logo-large {
            width: 72px;
            height: 72px;
            border-radius: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            margin-bottom: 40px;
            overflow: hidden;
        }

        .brand-logo-large img {
            width: 100%;
            height: 100%;
            object-fit: contain;
            filter: drop-shadow(0 8px 24px rgba(255, 98, 0, 0.4));
        }

        .brand-content h1 {
            font-size: 56px;
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
            font-weight: 400;
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
            width: 36px;
            height: 36px;
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

        .login-box {
            width: 100%;
            max-width: 420px;
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

        .mobile-logo img {
            height: 48px;
            width: auto;
        }

        .login-box h2 {
            font-size: 32px;
            font-weight: 800;
            color: #ffffff;
            margin-bottom: 8px;
            letter-spacing: -0.8px;
        }

        .login-box .subtitle {
            color: #71767b;
            font-size: 15px;
            margin-bottom: 32px;
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

        .field-input:focus ~ .field-icon {
            color: #FF6200;
        }

        /* Password toggle */
        .password-toggle {
            position: absolute;
            right: 12px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            cursor: pointer;
            color: #5a5e63;
            font-size: 15px;
            padding: 8px;
            border-radius: 8px;
            transition: color 0.2s, background 0.2s;
            z-index: 2;
        }

        .password-toggle:hover {
            color: #FF6200;
            background: rgba(255, 98, 0, 0.08);
        }

        /* ===== FORGOT LINK ===== */
        .forgot-row {
            display: flex;
            justify-content: flex-end;
            margin-top: -10px;
            margin-bottom: 24px;
        }

        .forgot-row a {
            color: #a1a5aa;
            font-size: 13px;
            text-decoration: none;
            transition: color 0.2s;
        }

        .forgot-row a:hover {
            color: #FF6200;
            text-decoration: underline;
        }

        /* ===== PRIMARY BUTTON ===== */
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
            letter-spacing: 0.1px;
        }

        .btn-primary:hover {
            background: #e65800;
        }

        .btn-primary:active {
            transform: scale(0.98);
            background: #cc4e00;
        }

        /* ===== DIVIDER ===== */
        .divider {
            display: flex;
            align-items: center;
            gap: 12px;
            margin: 28px 0;
        }

        .divider::before,
        .divider::after {
            content: '';
            flex: 1;
            height: 1px;
            background: #2f3336;
        }

        .divider span {
            color: #71767b;
            font-size: 13px;
            font-weight: 400;
        }

        /* ===== SECONDARY BUTTON ===== */
        .btn-secondary {
            width: 100%;
            height: 52px;
            padding: 0 24px;
            font-size: 15px;
            font-weight: 700;
            font-family: inherit;
            color: #FF6200;
            background: transparent;
            border: 1px solid #FF6200;
            border-radius: 9999px;
            cursor: pointer;
            transition: background 0.2s, transform 0.1s;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            text-decoration: none;
            letter-spacing: 0.1px;
        }

        .btn-secondary:hover {
            background: rgba(255, 98, 0, 0.1);
        }

        .btn-secondary:active {
            transform: scale(0.98);
        }

        /* ===== ALERT ===== */
        .alert {
            padding: 14px 16px;
            border-radius: 12px;
            margin-bottom: 24px;
            font-size: 14px;
            display: flex;
            align-items: center;
            gap: 10px;
            animation: shake 0.4s ease-in-out;
            line-height: 1.4;
        }

        @keyframes shake {
            0%, 100% { transform: translateX(0); }
            25% { transform: translateX(-5px); }
            75% { transform: translateX(5px); }
        }

        .alert-error {
            background: rgba(244, 33, 46, 0.1);
            border: 1px solid rgba(244, 33, 46, 0.3);
            color: #f4212e;
        }

        .alert i {
            flex-shrink: 0;
            font-size: 16px;
        }

        /* ===== FOOTER ===== */
        .login-footer {
            margin-top: 32px;
            text-align: center;
            color: #71767b;
            font-size: 13px;
            line-height: 1.6;
        }

        .login-footer a {
            color: #71767b;
            text-decoration: none;
            margin: 0 4px;
        }

        .login-footer a:hover {
            text-decoration: underline;
        }

        /* ===== LOADING ===== */
        .btn-loading {
            pointer-events: none;
            opacity: 0.8;
        }

        .btn-loading i {
            animation: spin 0.8s linear infinite;
        }

        @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }

        /* ===== RESPONSIVE ===== */
        @media (max-width: 900px) {
            .brand-panel {
                display: none;
            }

            .form-panel {
                padding: 24px;
                min-height: 100vh;
            }

            .mobile-logo {
                display: flex;
            }

            .login-box h2 {
                font-size: 26px;
                text-align: center;
            }

            .login-box .subtitle {
                text-align: center;
            }
        }

        @media (max-width: 480px) {
            .form-panel {
                padding: 20px 16px;
                align-items: flex-start;
                padding-top: 48px;
            }

            .login-box h2 {
                font-size: 22px;
            }
        }
    </style>
</head>
<body>
<div class="login-wrapper">

    <!-- ===== LEFT: BRANDING ===== -->
    <div class="brand-panel">
        <div class="brand-content">
            <div class="brand-logo-large">
                <img src="<?= SITE_URL ?>/assets/icons/logo.png" alt="BARKER">
            </div>
            <h1>See what's<br>happening now</h1>
            <p>Join the pack. Share moments, connect with friends, and let your voice be heard.</p>

            <div class="brand-features">
                <div class="brand-feature">
                    <i class="fa-solid fa-bolt"></i>
                    <span>Real-time updates from your pack</span>
                </div>
                <div class="brand-feature">
                    <i class="fa-solid fa-users"></i>
                    <span>Connect with people who matter</span>
                </div>
                <div class="brand-feature">
                    <i class="fa-solid fa-shield-halved"></i>
                    <span>Safe, secure & private</span>
                </div>
            </div>
        </div>
    </div>

    <!-- ===== RIGHT: LOGIN FORM ===== -->
    <div class="form-panel">
        <div class="login-box">

            <div class="mobile-logo">
                <img src="<?= SITE_URL ?>/assets/icons/logo.png" alt="BARKER">
            </div>

            <h2>Sign in to BARKER</h2>
            <p class="subtitle">Welcome back — no leash required.</p>

            <?php if ($error): ?>
                <div class="alert alert-error">
                    <i class="fa-solid fa-circle-exclamation"></i>
                    <span><?= sanitize($error) ?></span>
                </div>
            <?php endif; ?>

            <form method="POST" action="" id="loginForm" autocomplete="off">
                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">

                <!-- Login field -->
                <div class="field">
                    <label class="field-label-top" for="loginInput">Email ou nom d'utilisateur</label>
                    <div class="field-input-wrap">
                        <input type="text" name="login" id="loginInput" class="field-input"
                            placeholder="ex: max_du_13 ou max@barker.ink"
                            value="<?= sanitize($_POST['login'] ?? '') ?>"
                            required autocomplete="username">
                        <i class="fa-solid fa-at field-icon"></i>
                    </div>
                </div>

                <!-- Password field -->
                <div class="field">
                    <label class="field-label-top" for="passwordInput">Mot de passe</label>
                    <div class="field-input-wrap">
                        <input type="password" name="password" id="passwordInput"
                            class="field-input" placeholder="Ton mot de passe"
                            required autocomplete="current-password">
                        <i class="fa-solid fa-lock field-icon"></i>
                        <button type="button" class="password-toggle" onclick="togglePassword()" aria-label="Afficher le mot de passe">
                            <i class="fa-solid fa-eye" id="eyeIcon"></i>
                        </button>
                    </div>
                </div>

                <div class="forgot-row">
                    <a href="<?= SITE_URL ?>/pages/forgot_password.php">Mot de passe oublié ?</a>
                </div>

                <button type="submit" class="btn-primary" id="submitBtn">
                    Se connecter
                </button>
            </form>

            <div class="divider">
                <span>ou</span>
            </div>

            <a href="<?= SITE_URL ?>/pages/register.php" class="btn-secondary">
                <i class="fa-solid fa-user-plus"></i>
                Créer un compte
            </a>

            <div class="login-footer">
                <a href="terms.php">Conditions d'utilisation</a> · <a href="privacy.php">Politique de confidentialité</a>
            </div>
        </div>
    </div>
</div>

<script>
function togglePassword() {
    const input = document.getElementById('passwordInput');
    const icon  = document.getElementById('eyeIcon');
    if (input.type === 'password') {
        input.type = 'text';
        icon.className = 'fa-solid fa-eye-slash';
    } else {
        input.type = 'password';
        icon.className = 'fa-solid fa-eye';
    }
}

// Loading state on submit
document.getElementById('loginForm').addEventListener('submit', function() {
    const btn = document.getElementById('submitBtn');
    if (btn) {
        btn.classList.add('btn-loading');
        btn.innerHTML = '<i class="fa-solid fa-spinner"></i> Connexion…';
        setTimeout(() => {
            if (btn.classList.contains('btn-loading')) {
                btn.classList.remove('btn-loading');
                btn.innerHTML = 'Se connecter';
            }
        }, 10000);
    }
});

// Auto-focus login field
document.addEventListener('DOMContentLoaded', function() {
    const loginField = document.getElementById('loginInput');
    if (loginField && !loginField.value) loginField.focus();
});
</script>
</body>
</html>