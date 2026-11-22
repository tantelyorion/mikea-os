<?php
require_once __DIR__ . '/config.php';

if (ml_is_logged_in()) {
    header('Location: index.php');
    exit;
}

$error = '';
$postedEmail = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $email    = trim($_POST['email'] ?? '');
    $password = $_POST['password'] ?? '';
    $postedEmail = $email;

    if (empty($email) || empty($password)) {
        $error = 'Merci de remplir tous les champs.';
    } elseif (!filter_var($email, FILTER_VALIDATE_EMAIL)) {
        $error = 'Adresse email invalide.';
    } else {
        $conn = ml_try_login($email, $password);
        if ($conn === false) {
            $error = 'Adresse ou mot de passe incorrect, ou boîte introuvable sur ' . BARKERMAIL_IMAP_HOST . '.';
        } else {
            imap_close($conn);
            session_regenerate_id(true);
            $_SESSION['ml_email']    = $email;
            $_SESSION['ml_password'] = $password;
            header('Location: index.php');
            exit;
        }
    }
}
?>
<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
    <title>Connexion — Barker Mail</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:opsz,wght@14..32,300;14..32,400;14..32,500;14..32,600;14..32,700;14..32;800&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="css/mail.css">
    <style>
        /* ============================================================
           BARKER MAIL — Login (v7 — final, sans orange dans les champs)
           ============================================================ */
        * { margin: 0; padding: 0; box-sizing: border-box; }

        html, body {
            height: 100%;
        }

        body {
            font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            background: #000000;
            min-height: 100vh;
            color: #e7e9ea;
            -webkit-font-smoothing: antialiased;
        }

        /* ===== SPLIT LAYOUT ===== */
        .ml-login-wrap {
            display: flex;
            width: 100%;
            min-height: 100vh;
            background: #000;
        }

        /* ============ LEFT — Branding ============ */
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
            width: 72px;
            height: 72px;
            border-radius: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            margin-bottom: 32px;
            font-size: 34px;
            color: #FF6200;
            background: rgba(255, 98, 0, 0.1);
            border: 1px solid rgba(255, 98, 0, 0.25);
            box-shadow: 0 8px 32px rgba(255, 98, 0, 0.25);
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

        /* ============ RIGHT — Form ============ */
        .form-panel {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 40px;
            background: #000000;
        }

        .ml-login-card {
            width: 100%;
            max-width: 420px;
            animation: fadeIn 0.6s ease-out;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }

        /* Mobile logo */
        .mobile-logo {
            display: none;
            justify-content: center;
            align-items: center;
            gap: 10px;
            margin-bottom: 28px;
            font-size: 22px;
            font-weight: 800;
            color: #FF6200;
            letter-spacing: -0.5px;
        }
        .mobile-logo i { font-size: 24px; }

        /* ===== HEADER ===== */
        .ml-login-header {
            margin-bottom: 32px;
        }

        .ml-login-header h2 {
            font-size: 30px;
            font-weight: 800;
            color: #ffffff;
            margin-bottom: 8px;
            letter-spacing: -0.8px;
            line-height: 1.15;
        }

        .ml-login-sub {
            color: #71767b;
            font-size: 15px;
            line-height: 1.5;
            margin: 0;
        }

        .ml-login-sub strong {
            color: #FF6200;
            font-weight: 600;
        }

        /* ============ FIELDS (v7 — neutres, sans orange interne) ============ */
        .ml-field {
            margin-bottom: 20px;
        }

        .ml-field label {
            display: block;
            font-size: 13px;
            font-weight: 600;
            color: #a1a5aa;
            margin-bottom: 8px;
            padding-left: 2px;
            letter-spacing: 0.1px;
        }

        /* Wrapper = conteneur flex horizontal : icône | input | (toggle) */
        .ml-field-input-wrap {
            display: flex;
            align-items: center;
            width: 100%;
            height: 52px;
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 12px;
            padding: 0 14px 0 14px;
            transition: border-color 0.2s, background 0.2s;
        }

        .ml-field-input-wrap:hover {
            border-color: #3f4347;
            background: #111;
        }

        /* Focus : bordure grise claire, aucun orange, aucun halo */
        .ml-field-input-wrap:focus-within {
            border-color: #5a5e63;
            background: #141414;
            box-shadow: none;
        }

        /* Icon — reste grise en toutes circonstances */
        .ml-field-icon {
            flex-shrink: 0;
            width: 20px;
            height: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #5a5e63;
            font-size: 15px;
            line-height: 1;
            transition: color 0.2s;
            margin-right: 0;
            pointer-events: none;
        }

        /* Input — padding pour éloigner le texte de l'icône */
        .ml-field input {
            flex: 1 1 auto;
            min-width: 0;
            width: 100%;
            height: 100%;
            padding: 0 8px 0 16px;
            margin: 0;
            font-size: 15px;
            font-family: inherit;
            font-weight: 400;
            color: #e7e9ea;
            background: transparent;
            border: none;
            outline: none;
            line-height: 1;
            box-shadow: none;
            -webkit-appearance: none;
            appearance: none;
        }

        .ml-field input::placeholder {
            color: #4a4d52;
            font-size: 15px;
            line-height: 1;
            opacity: 1;
        }

        /* Password toggle */
        .ml-pwd-toggle {
            flex-shrink: 0;
            background: none;
            border: none;
            cursor: pointer;
            color: #5a5e63;
            font-size: 15px;
            padding: 0;
            margin-left: 4px;
            border-radius: 8px;
            transition: color 0.2s, background 0.2s;
            display: flex;
            align-items: center;
            justify-content: center;
            width: 36px;
            height: 36px;
        }

        .ml-pwd-toggle:hover {
            color: #e7e9ea;
            background: rgba(255, 255, 255, 0.06);
        }

        /* Autofill styling */
        .ml-field input:-webkit-autofill,
        .ml-field input:-webkit-autofill:hover,
        .ml-field input:-webkit-autofill:focus {
            -webkit-text-fill-color: #e7e9ea;
            -webkit-box-shadow: 0 0 0 1000px #141414 inset;
            transition: background-color 5000s ease-in-out 0s;
            caret-color: #e7e9ea;
        }

        /* ============ BUTTON ============ */
        .ml-btn {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            width: 100%;
            height: 52px;
            padding: 0 24px;
            font-size: 16px;
            font-weight: 700;
            font-family: inherit;
            border: none;
            border-radius: 9999px;
            cursor: pointer;
            transition: background 0.2s, transform 0.1s, box-shadow 0.2s;
            text-decoration: none;
            letter-spacing: 0.1px;
            margin-top: 8px;
        }

        .ml-btn-primary {
            background: #FF6200;
            color: #ffffff;
            box-shadow: 0 4px 12px rgba(255, 98, 0, 0.25);
        }

        .ml-btn-primary:hover {
            background: #e65800;
            box-shadow: 0 6px 16px rgba(255, 98, 0, 0.35);
        }

        .ml-btn-primary:active {
            transform: scale(0.98);
            background: #cc4e00;
        }

        .ml-btn-primary:disabled {
            opacity: 0.7;
            cursor: not-allowed;
        }

        /* ============ ALERT ============ */
        .ml-alert {
            padding: 14px 16px;
            border-radius: 12px;
            margin-bottom: 20px;
            font-size: 14px;
            display: flex;
            align-items: flex-start;
            gap: 10px;
            line-height: 1.5;
            animation: shake 0.4s ease-in-out;
        }

        @keyframes shake {
            0%, 100% { transform: translateX(0); }
            25% { transform: translateX(-5px); }
            75% { transform: translateX(5px); }
        }

        .ml-alert-error {
            background: rgba(244, 33, 46, 0.1);
            border: 1px solid rgba(244, 33, 46, 0.3);
            color: #f4212e;
        }

        .ml-alert-error i {
            flex-shrink: 0;
            font-size: 16px;
            margin-top: 1px;
        }

        /* ============ FOOTER ============ */
        .ml-login-footer {
            text-align: center;
            margin-top: 28px;
            font-size: 13px;
            color: #71767b;
            line-height: 1.6;
        }

        .ml-login-footer a {
            color: #FF6200;
            font-weight: 600;
            text-decoration: none;
        }

        .ml-login-footer a:hover {
            text-decoration: underline;
        }

        /* ============ LOADING ============ */
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

        /* ============ RESPONSIVE ============ */
        @media (max-width: 900px) {
            .brand-panel { display: none; }

            .form-panel {
                padding: 32px 24px;
                min-height: 100vh;
                align-items: center;
            }

            .mobile-logo { display: flex; }

            .ml-login-header h2 {
                font-size: 26px;
                text-align: center;
            }

            .ml-login-sub {
                text-align: center;
            }
        }

        @media (max-width: 480px) {
            .form-panel {
                padding: 24px 16px;
                align-items: flex-start;
                padding-top: 40px;
            }

            .ml-login-header h2 { font-size: 22px; }
            .ml-field { margin-bottom: 18px; }
            .ml-field-input-wrap { height: 50px; padding: 0 12px; }
            .ml-field-icon { width: 18px; height: 18px; font-size: 14px; }
            .ml-field input { padding: 0 6px 0 14px; }
            .ml-pwd-toggle { width: 32px; height: 32px; margin-left: 2px; }
            .ml-btn { height: 50px; font-size: 15px; }
        }
    </style>
</head>
<body>

<div class="ml-login-wrap">

    <!-- ===== LEFT: BRANDING ===== -->
    <div class="brand-panel">
        <div class="brand-content">
            <div class="brand-logo-large">
                <i class="fa-solid fa-envelope-open-text"></i>
            </div>
            <h1>Barker<br>Mail</h1>
            <p>Ta messagerie intégrée. Simple, rapide, et connectée à ton compte BARKER.</p>

            <div class="brand-features">
                <div class="brand-feature">
                    <i class="fa-solid fa-bolt"></i>
                    <span>Accès instantané à ta boîte</span>
                </div>
                <div class="brand-feature">
                    <i class="fa-solid fa-shield-halved"></i>
                    <span>Connexion sécurisée IMAP</span>
                </div>
                <div class="brand-feature">
                    <i class="fa-solid fa-at"></i>
                    <span>Une adresse @<?= ml_safe(MAIL_DOMAIN) ?></span>
                </div>
            </div>
        </div>
    </div>

    <!-- ===== RIGHT: FORM ===== -->
    <div class="form-panel">
        <div class="ml-login-card">

            <div class="mobile-logo">
                <i class="fa-solid fa-envelope-open-text"></i>
                Barker Mail
            </div>

            <div class="ml-login-header">
                <h2>Connexion</h2>
                <p class="ml-login-sub">
                    Connecte-toi avec ton adresse <strong>@<?= ml_safe(MAIL_DOMAIN) ?></strong>
                </p>
            </div>

            <?php if ($error): ?>
                <div class="ml-alert ml-alert-error">
                    <i class="fa-solid fa-triangle-exclamation"></i>
                    <span><?= ml_safe($error) ?></span>
                </div>
            <?php endif; ?>

            <form method="POST" action="" id="mailLoginForm" autocomplete="off">

                <!-- Email field -->
                <div class="ml-field">
                    <label for="email">Adresse email</label>
                    <div class="ml-field-input-wrap">
                        <i class="fa-solid fa-at ml-field-icon"></i>
                        <input type="email" id="email" name="email"
                               placeholder="ex: max@<?= ml_safe(MAIL_DOMAIN) ?>"
                               value="<?= ml_safe($postedEmail) ?>" required
                               autocomplete="username"
                               spellcheck="false"
                               autocapitalize="off"
                               autocorrect="off">
                    </div>
                </div>

                <!-- Password field -->
                <div class="ml-field">
                    <label for="password">Mot de passe</label>
                    <div class="ml-field-input-wrap">
                        <i class="fa-solid fa-lock ml-field-icon"></i>
                        <input type="password" id="password" name="password"
                               placeholder="Ton mot de passe"
                               required autocomplete="current-password">
                        <button type="button" class="ml-pwd-toggle" onclick="toggleMailPwd()" aria-label="Afficher le mot de passe">
                            <i class="fa-solid fa-eye" id="mailEyeIcon"></i>
                        </button>
                    </div>
                </div>

                <button type="submit" class="ml-btn ml-btn-primary" id="mailSubmitBtn">
                    <i class="fa-solid fa-right-to-bracket"></i> Se connecter
                </button>
            </form>

            <p class="ml-login-footer">
                Pas encore de boîte Barker Mail ?<br>
                <a href="<?= SITE_URL ?>/pages/register.php">Crée un compte BARKER</a>
            </p>
        </div>
    </div>
</div>

<script>
function toggleMailPwd() {
    const input = document.getElementById('password');
    const icon  = document.getElementById('mailEyeIcon');
    if (input.type === 'password') {
        input.type = 'text';
        icon.className = 'fa-solid fa-eye-slash';
    } else {
        input.type = 'password';
        icon.className = 'fa-solid fa-eye';
    }
}

document.getElementById('mailLoginForm')?.addEventListener('submit', function() {
    const btn = document.getElementById('mailSubmitBtn');
    if (btn) {
        btn.classList.add('btn-loading');
        btn.disabled = true;
        btn.innerHTML = '<i class="fa-solid fa-spinner"></i> Connexion…';
    }
});
</script>
</body>
</html>