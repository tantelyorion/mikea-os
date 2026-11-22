<?php
// ============================================================
// BARKER - Register Page (inscription en plusieurs étapes,
// avec création optionnelle d'une boîte Barker Mail @barker.ink)
// ============================================================

require_once dirname(__DIR__) . '/includes/init.php';
require_once dirname(__DIR__) . '/includes/directadmin_mail.php';
// require_once dirname(__DIR__) . '/includes/papay.php'; // 🔒 Désactivé — PAPAY non disponible juridiquement

if (is_logged_in()) redirect(SITE_URL . '/index.php');

$error   = '';
$papayOption = 'no'; // 🔒 Forcé à 'no' tant que PAPAY n'est pas disponible

$postedUsername           = '';
$postedDisplayName        = '';
$postedGender              = 'other';
$postedBio                 = '';
$postedPhone               = '';
$postedCountryIso          = 'MG';
$postedCountryDial         = '261';
$postedWantMail            = 'yes';
$postedMailUsernameChoice  = 'same';
$postedMailUsernameCustom  = '';
$postedMailPasswordChoice  = 'same';
$postedExternalEmail       = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        $error = 'Jeton de sécurité invalide. Merci de recharger la page et réessayer.';
    } else {
        $username     = trim($_POST['username'] ?? '');
        $display_name = trim($_POST['display_name'] ?? '');
        $password     = $_POST['password'] ?? '';
        $confirm      = $_POST['confirm_password'] ?? '';
        $gender       = $_POST['gender'] ?? 'other';
        $bio          = trim($_POST['bio'] ?? '');
        $terms        = isset($_POST['terms']) && $_POST['terms'] === 'accepted';

        $phone        = trim($_POST['phone'] ?? '');
        $countryIso   = strtoupper(trim($_POST['country_iso'] ?? ''));
        $countryDial  = trim($_POST['country_dial'] ?? '');

        $wantBarkerMail     = (($_POST['want_barker_mail'] ?? 'yes') === 'no') ? 'no' : 'yes';
        $mailUsernameChoice = (($_POST['mail_username_choice'] ?? 'same') === 'custom') ? 'custom' : 'same';
        $mailUsernameCustom = trim($_POST['mail_username_custom'] ?? '');
        $mailPasswordChoice = (($_POST['mail_password_choice'] ?? 'same') === 'custom') ? 'custom' : 'same';
        $mailPasswordCustom = $_POST['mail_password_custom'] ?? '';
        $externalEmail      = trim($_POST['external_email'] ?? '');

        $postedUsername          = $username;
        $postedDisplayName       = $display_name;
        $postedGender             = $gender;
        $postedBio                = $bio;
        $postedPhone              = $phone;
        $postedCountryIso         = $countryIso ?: 'MG';
        $postedCountryDial        = $countryDial ?: '261';
        $postedWantMail           = $wantBarkerMail;
        $postedMailUsernameChoice = $mailUsernameChoice;
        $postedMailUsernameCustom = $mailUsernameCustom;
        $postedMailPasswordChoice = $mailPasswordChoice;
        $postedExternalEmail      = $externalEmail;

        // ===== Validation de base =====
        if (empty($username) || empty($display_name) || empty($password)) {
            $error = 'Tous les champs obligatoires doivent être remplis.';
        } elseif (!preg_match('/^[a-zA-Z0-9_]{3,50}$/', $username)) {
            $error = 'Nom d\'utilisateur : 3 à 50 caractères, lettres/chiffres/underscore uniquement.';
        } elseif (strlen($password) < 8) {
            $error = 'Le mot de passe doit contenir au moins 8 caractères.';
        } elseif ($password !== $confirm) {
            $error = 'Les mots de passe ne correspondent pas.';
        } elseif (!in_array($gender, ['male', 'female', 'other'], true)) {
            $error = 'Genre invalide.';
        } elseif ($phone !== '' && !preg_match('/^\+\d{6,15}$/', $phone)) {
            $error = 'Numéro de téléphone invalide. Sélectionne ton pays et saisis ton numéro, ou laisse le champ vide.';
        } elseif (!$terms) {
            $error = 'Vous devez accepter les Conditions et la Politique de confidentialité.';
        }

        // ===== Unicité du téléphone (facultatif, mais unique s'il est renseigné) =====
        if (empty($error) && $phone !== '') {
            if (DB::fetchOne("SELECT id FROM users WHERE phone = ?", [$phone])) {
                $error = 'Ce numéro de téléphone est déjà associé à un compte BARKER.';
            }
        }

        $finalEmail        = '';
        $mailUsername       = null;
        $mailPasswordToUse  = null;

        // ===== Résolution de l'email (Barker Mail ou externe) =====
        if (empty($error)) {
            if ($wantBarkerMail === 'yes') {
                $mailUsername = ($mailUsernameChoice === 'custom' && $mailUsernameCustom !== '')
                    ? mb_strtolower($mailUsernameCustom)
                    : mb_strtolower($username);

                if (!barker_mail_valid_username($mailUsername)) {
                    $error = 'Le nom d\'utilisateur Barker Mail choisi n\'est pas valide (lettres, chiffres, points, tirets — 1 à 64 caractères).';
                } elseif ($mailPasswordChoice === 'custom' && strlen($mailPasswordCustom) < 8) {
                    $error = 'Le mot de passe Barker Mail doit contenir au moins 8 caractères.';
                } else {
                    $mailPasswordToUse = ($mailPasswordChoice === 'custom' && strlen($mailPasswordCustom) >= 8)
                        ? $mailPasswordCustom
                        : $password;
                    $finalEmail = $mailUsername . '@' . MAIL_DOMAIN;
                }
            } else {
                if (empty($externalEmail) || !filter_var($externalEmail, FILTER_VALIDATE_EMAIL)) {
                    $error = 'Merci d\'indiquer une adresse email valide.';
                } else {
                    $finalEmail = $externalEmail;
                }
            }
        }

        // ===== Unicité username / email =====
        if (empty($error)) {
            $existing = DB::fetchOne(
                "SELECT id FROM users WHERE username = ? OR email = ?",
                [$username, $finalEmail]
            );
            if ($existing) {
                $error = 'Ce nom d\'utilisateur ou cette adresse email est déjà utilisé.';
            }
        }

        // ===== Disponibilité de la boîte Barker Mail côté DirectAdmin =====
        if (empty($error) && $wantBarkerMail === 'yes') {
            if (!barker_mail_username_available($mailUsername)) {
                $error = 'L\'adresse ' . htmlspecialchars($mailUsername) . '@' . MAIL_DOMAIN . ' n\'est pas disponible. Choisissez un autre nom.';
            }
        }

        // ===== Création du compte =====
        if (empty($error)) {
            $hashed = hash_password($password);
            $ip     = get_client_ip();
            $userId = 0;

            try {
                $userId = DB::insert(
                    "INSERT INTO users (username, display_name, email, phone, country_iso, country_dial, password, gender, bio, ip_register, terms_accepted_at, mail_username, has_barker_mail)
                     VALUES (?,?,?,?,?,?,?,?,?,?, NOW(), ?, 0)",
                    [
                        $username,
                        $display_name,
                        $finalEmail,
                        $phone !== '' ? $phone : null,
                        $phone !== '' ? ($countryIso ?: null) : null,
                        $phone !== '' ? ($countryDial ?: null) : null,
                        $hashed,
                        $gender,
                        $bio,
                        $ip,
                        $wantBarkerMail === 'yes' ? $mailUsername : null,
                    ]
                );
            } catch (PDOException $e) {
                error_log('BARKER register: insert users failed: ' . $e->getMessage());
                $error = 'Ce nom d\'utilisateur, cette adresse email ou ce numéro est déjà utilisé.';
            }

            if (empty($error)) {
                log_security($userId, 'register', 'Nouveau compte BARKER créé');

                if ($wantBarkerMail === 'yes') {
                    $mailResult = barker_mail_create_account($mailUsername, $mailPasswordToUse);

                    if (!$mailResult['ok']) {
                        // Annule le compte : pas de compte sans la boîte mail demandée
                        DB::execute("DELETE FROM users WHERE id = ?", [$userId]);
                        log_security(null, 'barker_mail_create_failed', $mailResult['error'] ?? 'Erreur inconnue');
                        $error = 'Impossible de créer votre boîte Barker Mail (' . htmlspecialchars($mailResult['error'] ?? 'erreur inconnue') . '). Réessayez, ou choisissez une adresse email externe à l\'étape 2.';
                    } else {
                        DB::execute("UPDATE users SET has_barker_mail = 1 WHERE id = ?", [$userId]);
                        log_security($userId, 'barker_mail_created', 'Boîte Barker Mail créée : ' . $mailUsername . '@' . MAIL_DOMAIN);
                    }
                }
            }
        }

        if (empty($error)) {
            $user = DB::fetchOne("SELECT * FROM users WHERE id = ?", [$userId]);
            login_user($user);

            $_SESSION['flash_success'] = ($wantBarkerMail === 'yes')
                ? 'Compte BARKER créé avec succès ! Votre adresse Barker Mail ' . htmlspecialchars($mailUsername) . '@' . MAIL_DOMAIN . ' est prête — connectez-vous sur ' . MAIL_WEBMAIL_URL . '.'
                : 'Compte BARKER créé avec succès !';

            redirect(SITE_URL . '/index.php');
        }
    }
}
?>
<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
    <title>Join BARKER</title>
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

        .register-wrapper { display: flex; width: 100%; min-height: 100vh; }

        /* ===== LEFT — Branding ===== */
        .brand-panel {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 60px 40px;
            position: sticky;
            top: 0;
            height: 100vh;
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
        .brand-content { max-width: 480px; position: relative; z-index: 1; }
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
        .brand-content p { font-size: 18px; color: #71767b; line-height: 1.5; max-width: 380px; }
        .brand-features { margin-top: 48px; display: flex; flex-direction: column; gap: 20px; }
        .brand-feature { display: flex; align-items: center; gap: 14px; color: #e7e9ea; font-size: 15px; font-weight: 500; }
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

        /* ===== RIGHT — Form ===== */
        .form-panel { flex: 1; display: flex; justify-content: center; padding: 40px; background: #000000; overflow-y: auto; }
        .register-box { width: 100%; max-width: 520px; animation: fadeIn 0.6s ease-out; padding-bottom: 60px; }

        @keyframes fadeIn { from { opacity: 0; transform: translateY(20px); } to { opacity: 1; transform: translateY(0); } }

        .mobile-logo { display: none; justify-content: center; margin-bottom: 32px; }
        .mobile-logo img { height: 48px; width: auto; }

        .register-box h2 { font-size: 32px; font-weight: 800; color: #ffffff; margin-bottom: 8px; letter-spacing: -0.8px; }
        .register-box .subtitle { color: #71767b; font-size: 15px; margin-bottom: 28px; }

        /* ===== PROGRESS ===== */
        .progress-bar { display: flex; align-items: center; gap: 8px; margin-bottom: 12px; }
        .progress-seg { flex: 1; height: 4px; border-radius: 4px; background: #2f3336; transition: background 0.25s; }
        .progress-seg.done, .progress-seg.active { background: #FF6200; }
        .progress-labels { display: flex; justify-content: space-between; margin-bottom: 32px; font-size: 12px; color: #71767b; }
        .progress-labels span.current { color: #FF6200; font-weight: 700; }

        /* ===== STEPS ===== */
        .step { display: none; animation: fadeIn 0.35s ease-out; }
        .step.active { display: block; }

        .step-nav { display: flex; gap: 12px; margin-top: 28px; }
        .btn-back {
            flex: 0 0 auto;
            height: 52px;
            padding: 0 22px;
            font-size: 15px;
            font-weight: 700;
            font-family: inherit;
            color: #e7e9ea;
            background: transparent;
            border: 1px solid #333639;
            border-radius: 9999px;
            cursor: pointer;
            transition: border-color 0.2s, color 0.2s;
            display: inline-flex;
            align-items: center;
            justify-content: center;
        }
        .btn-back:hover { border-color: #555a5e; color: #ffffff; }

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
        .field-label-top .opt {
            color: #71767b;
            font-weight: 400;
            font-size: 12px;
        }

        .field-input-wrap {
            position: relative;
        }

        .field-input,
        .field-textarea {
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

        .field-textarea {
            padding: 14px 16px;
            resize: vertical;
            min-height: 96px;
            line-height: 1.5;
        }

        .field-input::placeholder,
        .field-textarea::placeholder {
            color: #4a4d52;
            font-size: 14px;
        }

        .field-input:hover,
        .field-textarea:hover {
            border-color: #3f4347;
            background: #111;
        }

        .field-input:focus,
        .field-textarea:focus {
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

        .field-hint {
            font-size: 12px;
            color: #71767b;
            margin-top: 8px;
            padding-left: 2px;
            line-height: 1.4;
        }

        /* ===== TÉLÉPHONE + PAYS (facultatif) ===== */
        .phone-field-row {
            display: flex;
            gap: 10px;
            align-items: stretch;
        }
        #countrySelectWrap { position: relative; flex-shrink: 0; }
        .country-select-btn {
            height: 52px;
            min-height: 52px;
            padding: 0 12px;
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 12px;
            color: #e7e9ea;
            font-size: 15px;
            font-family: inherit;
            cursor: pointer;
            display: flex;
            align-items: center;
            gap: 6px;
            white-space: nowrap;
        }
        .country-select-btn:hover, .country-select-btn:focus {
            border-color: #FF6200;
            outline: none;
        }
        .phone-local-wrapper { flex: 1; position: relative; display: flex; align-items: center; min-width: 0; }
        .phone-local-wrapper .field-icon { left: 16px; }
        .phone-local-wrapper input { width: 100%; }
        .country-dropdown {
            display: none;
            position: absolute;
            top: calc(100% + 6px);
            left: 0;
            width: 280px;
            max-width: 90vw;
            background: #16181c;
            border: 1px solid #2f3336;
            border-radius: 12px;
            box-shadow: 0 12px 32px rgba(0,0,0,.5);
            z-index: 50;
            flex-direction: column;
            overflow: hidden;
        }
        .country-search-wrap { padding: 10px; border-bottom: 1px solid #2f3336; }
        .country-search-wrap input {
            width: 100%;
            padding: 10px 12px;
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 8px;
            color: #e7e9ea;
            font-size: 14px;
            outline: none;
        }
        .country-search-wrap input:focus { border-color: #FF6200; }
        .country-list-items { overflow-y: auto; max-height: 260px; }
        .country-item {
            display: flex;
            align-items: center;
            gap: 10px;
            padding: 10px 14px;
            cursor: pointer;
            font-size: 13.5px;
        }
        .country-item:hover { background: #1e2124; }
        .country-item-flag { font-size: 18px; }
        .country-item-name { flex: 1; color: #d3d5d8; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
        .country-item-dial { color: #71767b; font-size: 12.5px; }
        @media (max-width: 480px) {
            .phone-field-row { flex-wrap: wrap; }
            #countrySelectWrap { flex: 1 0 100%; }
            .country-select-btn { width: 100%; justify-content: space-between; }
            .country-dropdown { width: 100%; }
        }

        .field-suffix {
            position: absolute;
            right: 16px;
            top: 50%;
            transform: translateY(-50%);
            color: #71767b;
            font-size: 14px;
            font-weight: 600;
            pointer-events: none;
        }

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

        .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }
        @media (max-width: 520px) { .grid-2 { grid-template-columns: 1fr; } }

        .section-title {
            font-size: 12px;
            font-weight: 700;
            color: #71767b;
            text-transform: uppercase;
            letter-spacing: 1.4px;
            margin: 34px 0 18px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .section-title:first-child { margin-top: 0; }
        .section-title i { color: #FF6200; font-size: 13px; }
        .section-title::after { content: ''; flex: 1; height: 1px; background: #2f3336; }

        /* ===== GENDER PILLS ===== */
        .gender-options { display: flex; gap: 10px; flex-wrap: wrap; }
        .gender-opt {
            flex: 1; min-width: 100px;
            display: flex; flex-direction: column;
            align-items: center; justify-content: center;
            gap: 6px;
            padding: 14px 12px;
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 12px;
            cursor: pointer;
            transition: all 0.2s;
            color: #a1a5aa;
            font-size: 13px;
            font-weight: 500;
            text-align: center;
        }
        .gender-opt:hover { border-color: #3f4347; color: #e7e9ea; background: #111; }
        .gender-opt.selected {
            border-color: #FF6200;
            background: rgba(255, 98, 0, 0.1);
            color: #FF6200;
        }
        .gender-opt input { display: none; }
        .gender-icon { font-size: 22px; line-height: 1; }

        /* ===== CHOICE CARDS (Barker Mail) ===== */
        .choice-opts { display: flex; flex-direction: column; gap: 10px; margin-bottom: 8px; }
        .choice-opt {
            display: flex; align-items: flex-start; gap: 14px;
            padding: 16px;
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 14px;
            cursor: pointer;
            transition: all 0.2s;
        }
        .choice-opt:hover { border-color: #3f4347; background: #111; }
        .choice-opt.selected {
            border-color: #FF6200;
            background: rgba(255, 98, 0, 0.08);
        }
        .choice-opt input { display: none; }
        .choice-radio {
            width: 20px; height: 20px;
            border-radius: 50%;
            border: 2px solid #555a5e;
            flex-shrink: 0;
            margin-top: 2px;
            position: relative;
            transition: border-color 0.2s;
        }
        .choice-opt.selected .choice-radio { border-color: #FF6200; }
        .choice-opt.selected .choice-radio::after {
            content: '';
            position: absolute;
            inset: 3px;
            border-radius: 50%;
            background: #FF6200;
        }
        .choice-opt-content { display: flex; flex-direction: column; gap: 3px; }
        .choice-opt-label { font-size: 15px; font-weight: 600; color: #e7e9ea; }
        .choice-opt-hint { font-size: 13px; color: #71767b; line-height: 1.45; }

        .subfields {
            display: none;
            margin-top: 12px;
            padding: 18px;
            background: rgba(255, 98, 0, 0.03);
            border: 1px dashed #333639;
            border-radius: 14px;
        }
        .subfields.active { display: block; animation: fadeIn 0.3s ease-out; }

        .subfields .mini-title {
            font-size: 12px;
            font-weight: 700;
            color: #a1a5aa;
            text-transform: uppercase;
            letter-spacing: 0.8px;
            margin-bottom: 12px;
            display: block;
        }
        .subfields .mini-title:not(:first-child) { margin-top: 20px; }

        .mini-choice { display: flex; gap: 8px; margin-bottom: 14px; flex-wrap: wrap; }
        .mini-choice label {
            display: flex; align-items: center; gap: 8px;
            font-size: 13px;
            color: #e7e9ea;
            padding: 9px 14px;
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 9999px;
            cursor: pointer;
            transition: all 0.2s;
            user-select: none;
        }
        .mini-choice label:hover { border-color: #3f4347; background: #111; }
        .mini-choice label:has(input:checked) {
            border-color: #FF6200;
            background: rgba(255, 98, 0, 0.1);
            color: #FF6200;
        }
        .mini-choice input { margin: 0; accent-color: #FF6200; cursor: pointer; }

        /* ===== TERMS ===== */
        .terms-group {
            margin: 24px 0;
            padding: 16px;
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 14px;
            transition: border-color 0.2s;
        }
        .terms-group:hover { border-color: #3f4347; }
        .terms-group.error {
            border-color: #f4212e;
            animation: shake 0.4s ease;
        }
        .terms-group label {
            display: flex;
            align-items: flex-start;
            gap: 12px;
            cursor: pointer;
        }
        .terms-group input[type="checkbox"] {
            margin-top: 2px;
            width: 18px; height: 18px;
            flex-shrink: 0;
            accent-color: #FF6200;
            cursor: pointer;
        }
        .terms-text { font-size: 14px; line-height: 1.5; color: #e7e9ea; }
        .terms-text a { color: #FF6200; font-weight: 600; text-decoration: none; }
        .terms-text a:hover { text-decoration: underline; }
        .required-star { color: #f4212e; font-weight: 700; }

        /* ===== RECAP ===== */
        .recap-card {
            background: #0d0d0d;
            border: 1px solid #2f3336;
            border-radius: 14px;
            padding: 18px;
            margin-bottom: 24px;
        }
        .recap-row {
            display: flex;
            justify-content: space-between;
            gap: 12px;
            padding: 12px 0;
            border-bottom: 1px solid #2f3336;
            font-size: 14px;
        }
        .recap-row:last-child { border-bottom: none; padding-bottom: 0; }
        .recap-row:first-child { padding-top: 0; }
        .recap-row .recap-label { color: #a1a5aa; flex-shrink: 0; }
        .recap-row .recap-value {
            color: #e7e9ea;
            font-weight: 600;
            text-align: right;
            word-break: break-word;
        }
        .recap-value.brand { color: #FF6200; }

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
            animation: shake 0.4s ease-in-out;
            line-height: 1.45;
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
        .alert-success {
            background: rgba(0, 186, 124, 0.1);
            border: 1px solid rgba(0, 186, 124, 0.3);
            color: #00ba7c;
        }
        .alert i { flex-shrink: 0; font-size: 16px; margin-top: 1px; }

        /* ===== DIVIDER ===== */
        .divider {
            display: flex;
            align-items: center;
            gap: 12px;
            margin: 32px 0 24px;
        }
        .divider::before, .divider::after {
            content: '';
            flex: 1;
            height: 1px;
            background: #2f3336;
        }
        .divider span { color: #71767b; font-size: 13px; }

        /* ===== FOOTER ===== */
        .register-footer {
            text-align: center;
            color: #71767b;
            font-size: 14px;
        }
        .register-footer a {
            color: #FF6200;
            font-weight: 600;
            text-decoration: none;
        }
        .register-footer a:hover { text-decoration: underline; }

        /* ===== RESPONSIVE ===== */
        @media (max-width: 900px) {
            .brand-panel { display: none; }
            .form-panel { padding: 24px; min-height: 100vh; align-items: flex-start; }
            .mobile-logo { display: flex; }
            .register-box h2 { font-size: 26px; text-align: center; }
            .register-box .subtitle { text-align: center; }
        }
        @media (max-width: 480px) {
            .form-panel { padding: 20px 16px; padding-top: 32px; }
            .gender-opt { min-width: 80px; padding: 12px 8px; font-size: 12px; }
            .register-box h2 { font-size: 22px; }
        }

        .btn-loading { pointer-events: none; opacity: 0.8; }
        .btn-loading i { animation: spin 0.8s linear infinite; }
        @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }
    </style>
</head>
<body>

<div class="register-wrapper">

    <!-- ===== LEFT: BRANDING ===== -->
    <div class="brand-panel">
        <div class="brand-content">
            <div class="brand-logo-large">
                <img src="<?= SITE_URL ?>/assets/icons/logo.png" alt="BARKER">
            </div>
            <h1>Join the pack.</h1>
            <p>Crée ton compte BARKER, connecte-toi avec ta meute, partage tes moments — et si tu veux, ouvre en même temps ta boîte mail @barker.ink.</p>

            <div class="brand-features">
                <div class="brand-feature">
                    <i class="fa-solid fa-bolt"></i>
                    <span>L'actu de ta meute en temps réel</span>
                </div>
                <div class="brand-feature">
                    <i class="fa-solid fa-envelope"></i>
                    <span>Une adresse @barker.ink avec Barker Mail</span>
                </div>
                <div class="brand-feature">
                    <i class="fa-solid fa-shield-halved"></i>
                    <span>Sûr, sécurisé & privé</span>
                </div>
            </div>
        </div>
    </div>

    <!-- ===== RIGHT: FORM ===== -->
    <div class="form-panel">
        <div class="register-box">

            <div class="mobile-logo">
                <img src="<?= SITE_URL ?>/assets/icons/logo.png" alt="BARKER">
            </div>

            <h2>Crée ton compte</h2>
            <p class="subtitle">Rejoins BARKER en quelques étapes.</p>

            <?php if ($error): ?>
                <div class="alert alert-error">
                    <i class="fa-solid fa-triangle-exclamation"></i>
                    <span><?= sanitize($error) ?></span>
                </div>
            <?php endif; ?>

            <?php if (isset($_SESSION['flash_success'])): ?>
                <div class="alert alert-success">
                    <i class="fa-solid fa-check-circle"></i>
                    <span><?= $_SESSION['flash_success'] ?></span>
                </div>
                <?php unset($_SESSION['flash_success']); ?>
            <?php endif; ?>

            <!-- ===== PROGRESS ===== -->
            <div class="progress-bar">
                <div class="progress-seg active" data-seg="1"></div>
                <div class="progress-seg" data-seg="2"></div>
                <div class="progress-seg" data-seg="3"></div>
            </div>
            <div class="progress-labels">
                <span data-label="1" class="current">1. Profil</span>
                <span data-label="2">2. Barker Mail</span>
                <span data-label="3">3. Finalisation</span>
            </div>

            <form method="POST" action="" id="registerForm" autocomplete="off">
                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">

                <!-- ============================================ -->
                <!-- STEP 1 — PROFIL & SÉCURITÉ                    -->
                <!-- ============================================ -->
                <div class="step active" data-step="1">

                    <div class="section-title"><i class="fa-solid fa-user"></i> Infos de base</div>

                    <div class="grid-2">
                        <div class="field">
                            <label class="field-label-top" for="usernameInput">Nom d'utilisateur</label>
                            <div class="field-input-wrap">
                                <input type="text" name="username" id="usernameInput" class="field-input"
                                    placeholder="ex: max_du_13" maxlength="50"
                                    value="<?= sanitize($postedUsername) ?>" required>
                                <i class="fa-solid fa-at field-icon"></i>
                            </div>
                            <div class="field-hint">3-50 caractères, lettres/chiffres/underscore</div>
                        </div>
                        <div class="field">
                            <label class="field-label-top" for="displayNameInput">Nom affiché</label>
                            <div class="field-input-wrap">
                                <input type="text" name="display_name" id="displayNameInput" class="field-input"
                                    placeholder="ex: Max" maxlength="100"
                                    value="<?= sanitize($postedDisplayName) ?>" required>
                                <i class="fa-solid fa-id-card field-icon"></i>
                            </div>
                        </div>
                    </div>

                    <div class="section-title"><i class="fa-solid fa-comment-dots"></i> Numéro de téléphone</div>
                    <div class="field">
                        <label class="field-label-top" for="phoneLocalInput">Numéro de téléphone</label>
                        <div class="phone-field-row">
                            <div id="countrySelectWrap">
                                <button type="button" class="country-select-btn" id="countrySelectBtn"></button>
                                <div class="country-dropdown" id="countryDropdown">
                                    <div class="country-search-wrap">
                                        <input type="text" id="countrySearchInput" placeholder="Rechercher un pays…" autocomplete="off">
                                    </div>
                                    <div class="country-list-items" id="countryListItems"></div>
                                </div>
                            </div>
                            <div class="phone-local-wrapper">
                                <i class="fa-solid fa-phone field-icon"></i>
                                <input type="tel" id="phoneLocalInput" class="field-input" placeholder="Numéro de téléphone (facultatif)" autocomplete="tel">
                            </div>
                        </div>
                        <input type="hidden" name="phone" id="phoneHiddenInput" value="<?= sanitize($postedPhone) ?>">
                        <input type="hidden" name="country_iso" id="countryIsoInput" value="<?= sanitize($postedCountryIso) ?>">
                        <input type="hidden" name="country_dial" id="countryDialInput" value="<?= sanitize($postedCountryDial) ?>">
                        <div class="field-hint">Facultatif. S'il est renseigné, il doit être unique.</div>
                    </div>

                    <div class="section-title"><i class="fa-solid fa-lock"></i> Sécurité</div>

                    <div class="grid-2">
                        <div class="field">
                            <label class="field-label-top" for="passwordInput">Mot de passe</label>
                            <div class="field-input-wrap">
                                <input type="password" name="password" id="passwordInput" class="field-input"
                                    placeholder="8 caractères min." required minlength="8">
                                <i class="fa-solid fa-key field-icon"></i>
                                <button type="button" class="password-toggle" onclick="togglePwd('passwordInput','eye1')" aria-label="Afficher">
                                    <i class="fa-solid fa-eye" id="eye1"></i>
                                </button>
                            </div>
                        </div>
                        <div class="field">
                            <label class="field-label-top" for="confirmInput">Confirmation</label>
                            <div class="field-input-wrap">
                                <input type="password" name="confirm_password" id="confirmInput" class="field-input"
                                    placeholder="Retaper le mot de passe" required minlength="8">
                                <i class="fa-solid fa-key field-icon"></i>
                                <button type="button" class="password-toggle" onclick="togglePwd('confirmInput','eye2')" aria-label="Afficher">
                                    <i class="fa-solid fa-eye" id="eye2"></i>
                                </button>
                            </div>
                        </div>
                    </div>

                    <div class="section-title"><i class="fa-solid fa-paw"></i> Profil</div>

                    <div class="field">
                        <label class="field-label-top">Je m'identifie comme</label>
                        <div class="gender-options">
                            <label class="gender-opt <?= ($postedGender === 'male') ? 'selected' : '' ?>" onclick="selectGender('male', this)">
                                <input type="radio" name="gender" value="male" <?= ($postedGender === 'male') ? 'checked' : '' ?>>
                                <span class="gender-icon">🧔</span> Homme
                            </label>
                            <label class="gender-opt <?= ($postedGender === 'female') ? 'selected' : '' ?>" onclick="selectGender('female', this)">
                                <input type="radio" name="gender" value="female" <?= ($postedGender === 'female') ? 'checked' : '' ?>>
                                <span class="gender-icon">👩</span> Femme
                            </label>
                            <label class="gender-opt <?= ($postedGender === 'other') ? 'selected' : '' ?>" onclick="selectGender('other', this)">
                                <input type="radio" name="gender" value="other" <?= ($postedGender === 'other') ? 'checked' : '' ?>>
                                <span class="gender-icon">🌈</span> Autre
                            </label>
                        </div>
                    </div>

                    <div class="field">
                        <label class="field-label-top" for="bioInput">Bio <span class="opt">(facultatif)</span></label>
                        <textarea name="bio" id="bioInput" class="field-textarea"
                            placeholder="Parle un peu de toi à la meute…" maxlength="300"><?= sanitize($postedBio) ?></textarea>
                        <div class="field-hint">300 caractères max</div>
                    </div>

                    <div class="step-nav">
                        <button type="button" class="btn-primary" onclick="goToStep(2)">
                            Continuer <i class="fa-solid fa-arrow-right"></i>
                        </button>
                    </div>
                </div>

                <!-- ============================================ -->
                <!-- STEP 2 — BARKER MAIL                          -->
                <!-- ============================================ -->
                <div class="step" data-step="2">

                    <div class="section-title"><i class="fa-solid fa-envelope"></i> Ta boîte Barker Mail</div>
                    <p style="font-size: 13px; color: #a1a5aa; margin-bottom: 16px; padding-left: 2px; line-height: 1.5;">
                        Barker Mail est notre messagerie intégrée (@barker.ink). Tu peux en ouvrir une automatiquement, ou utiliser une adresse email externe.
                    </p>

                    <div class="choice-opts">
                        <label class="choice-opt <?= ($postedWantMail === 'yes') ? 'selected' : '' ?>" onclick="selectWantMail('yes', this)">
                            <input type="radio" name="want_barker_mail" value="yes" <?= ($postedWantMail === 'yes') ? 'checked' : '' ?>>
                            <span class="choice-radio"></span>
                            <span class="choice-opt-content">
                                <span class="choice-opt-label">✨ Créer ma boîte Barker Mail</span>
                                <span class="choice-opt-hint">Une adresse @barker.ink est créée automatiquement pour toi</span>
                            </span>
                        </label>
                        <label class="choice-opt <?= ($postedWantMail === 'no') ? 'selected' : '' ?>" onclick="selectWantMail('no', this)">
                            <input type="radio" name="want_barker_mail" value="no" <?= ($postedWantMail === 'no') ? 'checked' : '' ?>>
                            <span class="choice-radio"></span>
                            <span class="choice-opt-content">
                                <span class="choice-opt-label">Utiliser une adresse email externe</span>
                                <span class="choice-opt-hint">Gmail, Yahoo… — tu pourras créer une boîte Barker Mail plus tard</span>
                            </span>
                        </label>
                    </div>

                    <!-- Sous-champs : création Barker Mail -->
                    <div id="barkerMailFields" class="subfields <?= ($postedWantMail === 'yes') ? 'active' : '' ?>">
                        <span class="mini-title">Nom d'utilisateur de la boîte</span>
                        <div class="mini-choice">
                            <label>
                                <input type="radio" name="mail_username_choice" value="same" onchange="toggleMailUsernameCustom()" <?= ($postedMailUsernameChoice === 'same') ? 'checked' : '' ?>>
                                <span>Utiliser mon nom d'utilisateur Barker</span>
                            </label>
                            <label>
                                <input type="radio" name="mail_username_choice" value="custom" onchange="toggleMailUsernameCustom()" <?= ($postedMailUsernameChoice === 'custom') ? 'checked' : '' ?>>
                                <span>Choisir un autre nom</span>
                            </label>
                        </div>
                        <div class="field" id="mailUsernameCustomField" style="<?= ($postedMailUsernameChoice === 'custom') ? '' : 'display:none;' ?> margin-bottom: 0;">
                            <label class="field-label-top" for="mailUsernameCustomInput">Nom souhaité</label>
                            <div class="field-input-wrap">
                                <input type="text" name="mail_username_custom" id="mailUsernameCustomInput" class="field-input"
                                    placeholder="ex: max.du.13" maxlength="64" value="<?= sanitize($postedMailUsernameCustom) ?>">
                                <i class="fa-solid fa-envelope field-icon"></i>
                                <span class="field-suffix">@<?= MAIL_DOMAIN ?></span>
                            </div>
                        </div>

                        <span class="mini-title">Mot de passe de la boîte mail</span>
                        <div class="mini-choice">
                            <label>
                                <input type="radio" name="mail_password_choice" value="same" onchange="toggleMailPasswordCustom()" <?= ($postedMailPasswordChoice === 'same') ? 'checked' : '' ?>>
                                <span>Même mot de passe que mon compte Barker</span>
                            </label>
                            <label>
                                <input type="radio" name="mail_password_choice" value="custom" onchange="toggleMailPasswordCustom()" <?= ($postedMailPasswordChoice === 'custom') ? 'checked' : '' ?>>
                                <span>Définir un mot de passe différent</span>
                            </label>
                        </div>
                        <div class="field" id="mailPasswordCustomField" style="<?= ($postedMailPasswordChoice === 'custom') ? '' : 'display:none;' ?> margin-bottom: 0;">
                            <label class="field-label-top" for="mailPasswordCustomInput">Mot de passe Barker Mail</label>
                            <div class="field-input-wrap">
                                <input type="password" name="mail_password_custom" id="mailPasswordCustomInput" class="field-input"
                                    placeholder="8 caractères min." minlength="8">
                                <i class="fa-solid fa-key field-icon"></i>
                                <button type="button" class="password-toggle" onclick="togglePwd('mailPasswordCustomInput','eye3')" aria-label="Afficher">
                                    <i class="fa-solid fa-eye" id="eye3"></i>
                                </button>
                            </div>
                        </div>
                    </div>

                    <!-- Sous-champ : email externe -->
                    <div id="externalMailField" class="subfields <?= ($postedWantMail === 'no') ? 'active' : '' ?>">
                        <div class="field" style="margin-bottom: 0;">
                            <label class="field-label-top" for="externalEmailInput">Adresse email</label>
                            <div class="field-input-wrap">
                                <input type="email" name="external_email" id="externalEmailInput" class="field-input"
                                    placeholder="ex: max@gmail.com" value="<?= sanitize($postedExternalEmail) ?>">
                                <i class="fa-solid fa-envelope field-icon"></i>
                            </div>
                        </div>
                    </div>

                    <div class="step-nav">
                        <button type="button" class="btn-back" onclick="goToStep(1)" aria-label="Retour">
                            <i class="fa-solid fa-arrow-left"></i>
                        </button>
                        <button type="button" class="btn-primary" onclick="goToStep(3)">
                            Continuer <i class="fa-solid fa-arrow-right"></i>
                        </button>
                    </div>
                </div>

                <!-- ============================================ -->
                <!-- STEP 3 — RÉCAP & FINALISATION                 -->
                <!-- ============================================ -->
                <div class="step" data-step="3">

                    <div class="section-title"><i class="fa-solid fa-clipboard-check"></i> Vérifie tout ça</div>

                    <div class="recap-card">
                        <div class="recap-row">
                            <span class="recap-label">Nom d'utilisateur</span>
                            <span class="recap-value" id="recapUsername">—</span>
                        </div>
                        <div class="recap-row">
                            <span class="recap-label">Nom affiché</span>
                            <span class="recap-value" id="recapDisplayName">—</span>
                        </div>
                        <div class="recap-row">
                            <span class="recap-label">Adresse email</span>
                            <span class="recap-value brand" id="recapEmail">—</span>
                        </div>
                    </div>

                    <?php /* 🔒 PAPAY SECTION — DÉSACTIVÉE — Réservé pour PAPAY quand disponible juridiquement */ ?>

                    <div class="terms-group" id="termsGroup">
                        <label>
                            <input type="checkbox" name="terms" value="accepted" required id="termsCheckbox"
                                <?= (isset($_POST['terms']) && $_POST['terms'] === 'accepted') ? 'checked' : '' ?>>
                            <span class="terms-text">
                                J'ai lu et j'accepte les
                                <a href="<?= SITE_URL ?>/pages/terms.php" target="_blank">Conditions d'utilisation</a>
                                et la
                                <a href="<?= SITE_URL ?>/pages/privacy.php" target="_blank">Politique de confidentialité</a>
                                <span class="required-star">*</span>
                            </span>
                        </label>
                    </div>

                    <div class="step-nav">
                        <button type="button" class="btn-back" onclick="goToStep(2)" aria-label="Retour">
                            <i class="fa-solid fa-arrow-left"></i>
                        </button>
                        <button type="submit" class="btn-primary" id="submitBtn" style="flex:1;">
                            <i class="fa-solid fa-paw"></i> Créer mon compte
                        </button>
                    </div>
                </div>

            </form>

            <div class="divider"><span>ou</span></div>

            <div class="register-footer">
                Déjà un compte ? <a href="<?= SITE_URL ?>/pages/login.php">Se connecter</a>
            </div>

        </div>
    </div>
</div>

<script src="<?= SITE_URL ?>/js/country-select.js?v=<?= @filemtime(dirname(__DIR__) . '/js/country-select.js') ?: time() ?>"></script>
<script>
const MAIL_DOMAIN = <?= json_encode(MAIL_DOMAIN) ?>;
let currentStep = 1;

// ============================================================
// Téléphone / pays (facultatif) — compose le numéro complet
// (+<indicatif><local sans 0 initial>) dans le champ caché,
// et renseigne le pays choisi. _selectedCountry vient de country-select.js.
// ============================================================
function composePhoneHidden() {
    const localInput = document.getElementById('phoneLocalInput');
    let local = (localInput.value || '').trim().replace(/[\s\-.()]/g, '');
    if (local.startsWith('0')) local = local.substring(1);
    document.getElementById('phoneHiddenInput').value = local ? '+' + _selectedCountry.dial + local : '';
    document.getElementById('countryIsoInput').value = _selectedCountry.code;
    document.getElementById('countryDialInput').value = _selectedCountry.dial;
}

// ============================================================
// Navigation entre étapes
// ============================================================
function goToStep(step) {
    if (step > currentStep && !validateStep(currentStep)) {
        return;
    }
    if (step === 3) updateRecap();

    document.querySelectorAll('.step').forEach(s => s.classList.remove('active'));
    document.querySelector('.step[data-step="' + step + '"]').classList.add('active');

    document.querySelectorAll('.progress-seg').forEach(seg => {
        const segNum = parseInt(seg.dataset.seg, 10);
        seg.classList.toggle('done', segNum < step);
        seg.classList.toggle('active', segNum === step);
    });
    document.querySelectorAll('.progress-labels span').forEach(lbl => {
        lbl.classList.toggle('current', parseInt(lbl.dataset.label, 10) === step);
    });

    currentStep = step;
    window.scrollTo({ top: 0, behavior: 'smooth' });

    // Focus intelligent sur le premier champ de l'étape
    setTimeout(() => {
        const firstInput = document.querySelector('.step[data-step="' + step + '"] .field-input, .step[data-step="' + step + '"] .field-textarea');
        if (firstInput) firstInput.focus();
    }, 300);
}

function validateStep(step) {
    if (step === 1) {
        const username = document.getElementById('usernameInput');
        const displayName = document.getElementById('displayNameInput');
        const password = document.getElementById('passwordInput');
        const confirm = document.getElementById('confirmInput');

        if (!username.value || !/^[a-zA-Z0-9_]{3,50}$/.test(username.value)) {
            username.focus(); username.style.borderColor = '#f4212e';
            alert("Nom d'utilisateur invalide (3-50 caractères, lettres/chiffres/underscore).");
            return false;
        }
        if (!displayName.value.trim()) {
            displayName.focus();
            alert('Merci de renseigner un nom affiché.');
            return false;
        }
        if (!password.value || password.value.length < 8) {
            password.focus(); password.style.borderColor = '#f4212e';
            alert('Le mot de passe doit contenir au moins 8 caractères.');
            return false;
        }
        if (password.value !== confirm.value) {
            confirm.focus(); confirm.style.borderColor = '#f4212e';
            alert('Les mots de passe ne correspondent pas.');
            return false;
        }

        // Le téléphone est facultatif : on le compose seulement s'il est saisi.
        composePhoneHidden();
    }

    if (step === 2) {
        const wantMail = document.querySelector('input[name="want_barker_mail"]:checked').value;
        if (wantMail === 'yes') {
            const choice = document.querySelector('input[name="mail_username_choice"]:checked').value;
            if (choice === 'custom') {
                const custom = document.getElementById('mailUsernameCustomInput');
                if (!custom.value.trim()) {
                    custom.focus(); custom.style.borderColor = '#f4212e';
                    alert("Merci de choisir un nom d'utilisateur pour ta boîte Barker Mail.");
                    return false;
                }
            }
            const pwdChoice = document.querySelector('input[name="mail_password_choice"]:checked').value;
            if (pwdChoice === 'custom') {
                const pwd = document.getElementById('mailPasswordCustomInput');
                if (!pwd.value || pwd.value.length < 8) {
                    pwd.focus(); pwd.style.borderColor = '#f4212e';
                    alert('Le mot de passe Barker Mail doit contenir au moins 8 caractères.');
                    return false;
                }
            }
        } else {
            const email = document.getElementById('externalEmailInput');
            if (!email.value || !email.checkValidity()) {
                email.focus(); email.style.borderColor = '#f4212e';
                alert('Merci de renseigner une adresse email valide.');
                return false;
            }
        }
    }

    return true;
}

function updateRecap() {
    document.getElementById('recapUsername').textContent = '@' + (document.getElementById('usernameInput').value || '—');
    document.getElementById('recapDisplayName').textContent = document.getElementById('displayNameInput').value || '—';

    const wantMail = document.querySelector('input[name="want_barker_mail"]:checked').value;
    let emailText;
    if (wantMail === 'yes') {
        const choice = document.querySelector('input[name="mail_username_choice"]:checked').value;
        const localPart = (choice === 'custom' && document.getElementById('mailUsernameCustomInput').value.trim())
            ? document.getElementById('mailUsernameCustomInput').value.trim()
            : (document.getElementById('usernameInput').value || '—');
        emailText = localPart + '@' + MAIL_DOMAIN + ' (Barker Mail)';
    } else {
        emailText = document.getElementById('externalEmailInput').value || '—';
    }
    document.getElementById('recapEmail').textContent = emailText;
}

// ============================================================
// Password toggle
// ============================================================
function togglePwd(inputId, iconId) {
    const input = document.getElementById(inputId);
    const icon  = document.getElementById(iconId);
    if (input.type === 'password') {
        input.type = 'text';
        icon.className = 'fa-solid fa-eye-slash';
    } else {
        input.type = 'password';
        icon.className = 'fa-solid fa-eye';
    }
}

// ============================================================
// Gender selection
// ============================================================
function selectGender(val, el) {
    document.querySelectorAll('.gender-opt').forEach(o => o.classList.remove('selected'));
    el.classList.add('selected');
    el.querySelector('input').checked = true;
}

// ============================================================
// Barker Mail — choix créer / externe
// ============================================================
function selectWantMail(val, el) {
    document.querySelectorAll('.choice-opt').forEach(o => o.classList.remove('selected'));
    el.classList.add('selected');
    el.querySelector('input').checked = true;

    const barkerFields = document.getElementById('barkerMailFields');
    const externalField = document.getElementById('externalMailField');

    if (val === 'yes') {
        barkerFields.classList.add('active');
        externalField.classList.remove('active');
        document.getElementById('externalEmailInput').required = false;
    } else {
        barkerFields.classList.remove('active');
        externalField.classList.add('active');
        document.getElementById('externalEmailInput').required = true;
    }
}

function toggleMailUsernameCustom() {
    const choice = document.querySelector('input[name="mail_username_choice"]:checked').value;
    document.getElementById('mailUsernameCustomField').style.display = (choice === 'custom') ? '' : 'none';
}

function toggleMailPasswordCustom() {
    const choice = document.querySelector('input[name="mail_password_choice"]:checked').value;
    document.getElementById('mailPasswordCustomField').style.display = (choice === 'custom') ? '' : 'none';
}

// ============================================================
// Form validation (dernier filet avant envoi)
// ============================================================
document.getElementById('registerForm').addEventListener('submit', function(e) {
    if (!validateStep(1) || !validateStep(2)) {
        e.preventDefault();
        return false;
    }

    const termsCheckbox = document.getElementById('termsCheckbox');
    const termsGroup    = document.getElementById('termsGroup');

    if (!termsCheckbox.checked) {
        e.preventDefault();
        termsGroup.classList.add('error');
        termsGroup.scrollIntoView({ behavior: 'smooth', block: 'center' });
        return false;
    }

    termsGroup.classList.remove('error');

    const btn = document.getElementById('submitBtn');
    btn.classList.add('btn-loading');
    btn.disabled = true;
    btn.innerHTML = '<i class="fa-solid fa-spinner"></i> Création du compte…';

    return true;
});

document.getElementById('termsCheckbox').addEventListener('change', function() {
    const termsGroup = document.getElementById('termsGroup');
    if (this.checked) termsGroup.classList.remove('error');
});

// ============================================================
// Init on load
// ============================================================
document.addEventListener('DOMContentLoaded', function() {
    const selectedGender = document.querySelector('input[name="gender"]:checked');
    if (selectedGender) {
        const parent = selectedGender.closest('.gender-opt');
        if (parent) selectGender(selectedGender.value, parent);
    }

    const selectedWantMail = document.querySelector('input[name="want_barker_mail"]:checked');
    if (selectedWantMail) {
        const parent = selectedWantMail.closest('.choice-opt');
        if (parent) selectWantMail(selectedWantMail.value, parent);
    }

    const firstInput = document.getElementById('usernameInput');
    if (firstInput && !firstInput.value) firstInput.focus();
});
</script>
</body>
</html>