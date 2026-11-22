<?php
require_once dirname(__DIR__, 2) . '/includes/init.php';
require_once dirname(__DIR__, 2) . '/includes/papay.php';

require_login();
$user = current_user();
$error = '';
$success = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        $error = 'Invalid CSRF token.';
    } else {
        $action = $_POST['action'] ?? '';
        
        if ($action === 'link_papay') {
            $papayUsername = trim($_POST['papay_username'] ?? '');
            if (empty($papayUsername)) {
                $error = 'Veuillez entrer votre nom d\'utilisateur PAPAY.';
            } else {
                if (link_papay_account($user['id'], $papayUsername)) {
                    $success = 'Compte PAPAY lié avec succès !';
                } else {
                    $error = 'Compte PAPAY introuvable.';
                }
            }
        } elseif ($action === 'create_papay') {
            $papayPassword = $_POST['papay_password'] ?? '';
            $papayPhone = $_POST['papay_phone'] ?? '';
            
            if (empty($papayPassword) || strlen($papayPassword) < 8) {
                $error = 'Mot de passe PAPAY (min. 8 caractères) requis.';
            } else {
                $papayUserId = create_papay_account(
                    $user['id'],
                    $user['username'],
                    $user['email'],
                    $user['display_name'],
                    $papayPhone
                );
                
                if ($papayUserId) {
                    $success = 'Compte PAPAY créé avec succès !';
                } else {
                    $error = 'Erreur lors de la création du compte PAPAY.';
                }
            }
        }
    }
}

$hasPapay = user_has_papay_account($user['id']);
$papayInfo = get_papay_user_info($user['id']);
?>

<!DOCTYPE html>
<html lang="fr" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lier PAPAY - BARKER</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css">
    <link rel="stylesheet" href="<?= SITE_URL ?>/css/style.css">
</head>
<body>
    <!-- Votre template header -->
    
    <div class="container" style="max-width:600px;margin:40px auto;">
        <h1><i class="fa-solid fa-wallet"></i> Intégration PAPAY</h1>
        
        <?php if ($hasPapay && $papayInfo): ?>
            <div class="alert alert-success">
                <i class="fa-solid fa-check-circle"></i> 
                Votre compte Barker est lié à PAPAY 
                (ID: <?= htmlspecialchars($papayInfo['user_id'] ?? '') ?>)
            </div>
            
            <div class="papay-info card" style="padding:20px;background:var(--bg-secondary);border-radius:8px;">
                <h3>Informations PAPAY</h3>
                <p><strong>Utilisateur:</strong> <?= htmlspecialchars($papayInfo['username'] ?? '') ?></p>
                <p><strong>Email:</strong> <?= htmlspecialchars($papayInfo['email'] ?? '') ?></p>
                <p><strong>Solde:</strong> <?= htmlspecialchars($papayInfo['balance'] ?? '0') ?> Ar</p>
                <a href="<?= PAPAY_BASE_URL ?>" class="btn btn-primary" target="_blank">
                    <i class="fa-solid fa-arrow-up-right-from-square"></i> Accéder à PAPAY
                </a>
            </div>
        <?php else: ?>
            <div class="alert alert-info">
                <i class="fa-solid fa-info-circle"></i>
                Vous n'avez pas encore de compte PAPAY associé.
            </div>
            
            <!-- Option 1: Créer un compte PAPAY -->
            <div class="card" style="padding:20px;background:var(--bg-secondary);border-radius:8px;margin-bottom:20px;">
                <h3><i class="fa-solid fa-plus-circle"></i> Créer un compte PAPAY</h3>
                <form method="POST">
                    <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                    <input type="hidden" name="action" value="create_papay">
                    
                    <div class="form-group">
                        <label>Mot de passe PAPAY *</label>
                        <input type="password" name="papay_password" class="form-control" placeholder="Min. 8 caractères" required minlength="8">
                    </div>
                    <div class="form-group">
                        <label>Téléphone</label>
                        <input type="tel" name="papay_phone" class="form-control" placeholder="+261 xx xxx xx">
                    </div>
                    <button type="submit" class="btn btn-primary">
                        <i class="fa-solid fa-wallet"></i> Créer mon compte PAPAY
                    </button>
                </form>
            </div>
            
            <!-- Option 2: Lier un compte existant -->
            <div class="card" style="padding:20px;background:var(--bg-secondary);border-radius:8px;">
                <h3><i class="fa-solid fa-link"></i> Lier un compte PAPAY existant</h3>
                <form method="POST">
                    <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                    <input type="hidden" name="action" value="link_papay">
                    
                    <div class="form-group">
                        <label>Nom d'utilisateur PAPAY *</label>
                        <input type="text" name="papay_username" class="form-control" placeholder="Votre pseudo sur PAPAY.info" required>
                        <div class="form-hint">Entrez votre nom d'utilisateur exact sur PAPAY.info</div>
                    </div>
                    <button type="submit" class="btn btn-secondary">
                        <i class="fa-solid fa-link"></i> Lier mon compte
                    </button>
                </form>
            </div>
        <?php endif; ?>
    </div>
    
    <!-- Votre template footer -->
</body>
</html>