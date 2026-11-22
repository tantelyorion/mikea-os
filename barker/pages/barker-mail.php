<?php
// ============================================================
// BARKER - pages/barker-mail.php
// ============================================================
// Point d'entrée du lien "Barker Mail" du popup "My Apps".
//  - Compte qui a déjà une boîte Barker Mail  → redirection directe vers le webmail.
//  - Compte qui n'en a pas encore             → on le lui dit, et on lui propose
//    d'en créer une ici même, SANS déconnexion : la boîte créée est liée
//    immédiatement au compte BARKER de la session en cours
//    (users.mail_username + users.has_barker_mail).
// ============================================================

require_once dirname(__DIR__) . '/includes/init.php';
require_once dirname(__DIR__) . '/includes/directadmin_mail.php';
require_login();

$currentUser = current_user();
$myId        = (int)$currentUser['id'];

// Déjà une boîte : on ouvre le webmail directement.
if (!empty($currentUser['has_barker_mail'])) {
    redirect(MAIL_WEBMAIL_URL);
}

$error   = '';
$created = null; // adresse complète une fois la boîte créée

// Nom proposé par défaut : le nom d'utilisateur BARKER, nettoyé.
$suggested = mb_strtolower(preg_replace('/[^a-zA-Z0-9._-]/', '', (string)$currentUser['username']));
$postedUsername = $suggested;

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        $error = 'Jeton de sécurité invalide, recharge la page et réessaie.';
    } else {
        $mailUsername   = mb_strtolower(trim($_POST['mail_username'] ?? ''));
        $mailPassword   = $_POST['mail_password'] ?? '';
        $mailPassword2  = $_POST['mail_password2'] ?? '';
        $postedUsername = $mailUsername;

        if (!barker_mail_valid_username($mailUsername)) {
            $error = 'Nom invalide : utilise des lettres, chiffres, point, tiret ou underscore.';
        } elseif (strlen($mailPassword) < 8) {
            $error = 'Le mot de passe de la boîte doit contenir au moins 8 caractères.';
        } elseif ($mailPassword !== $mailPassword2) {
            $error = 'Les deux mots de passe ne correspondent pas.';
        } elseif (DB::fetchOne("SELECT id FROM users WHERE mail_username = ?", [$mailUsername])) {
            $error = 'Cette adresse est déjà prise. Choisis un autre nom.';
        } elseif (!barker_mail_username_available($mailUsername)) {
            $error = 'L\'adresse ' . htmlspecialchars($mailUsername) . '@' . MAIL_DOMAIN . ' n\'est pas disponible. Choisis un autre nom.';
        } else {
            $res = barker_mail_create_account($mailUsername, $mailPassword);
            if (!$res['ok']) {
                log_security($myId, 'barker_mail_create_failed', $res['error'] ?? 'Erreur inconnue');
                $error = 'Impossible de créer la boîte pour le moment (' . htmlspecialchars($res['error'] ?? 'erreur inconnue') . ').';
            } else {
                // Liaison au compte BARKER de la session en cours. En cas d'échec
                // de l'écriture, on supprime la boîte pour ne pas laisser d'orpheline.
                try {
                    DB::execute("UPDATE users SET mail_username = ?, has_barker_mail = 1 WHERE id = ?", [$mailUsername, $myId]);
                    log_security($myId, 'barker_mail_created', 'Boîte Barker Mail créée après inscription : ' . $mailUsername . '@' . MAIL_DOMAIN);
                    $created = $mailUsername . '@' . MAIL_DOMAIN;
                } catch (Throwable $e) {
                    barker_mail_delete_account($mailUsername);
                    error_log('barker-mail.php link failed: ' . $e->getMessage());
                    $error = 'La boîte a été créée mais n\'a pas pu être liée à ton compte. Réessaie.';
                }
            }
        }
    }
}

$pageTitle = 'Barker Mail';
include __DIR__ . '/../includes/header.php';
?>

<main class="main-feed">
    <div class="bm-card">
        <?php if ($created): ?>
            <div class="bm-icon ok"><i class="fa-solid fa-circle-check"></i></div>
            <h1>Ta boîte Barker Mail est prête</h1>
            <p class="bm-address"><?= htmlspecialchars($created) ?></p>
            <p class="bm-text">Elle est maintenant liée à ton compte BARKER. Tu restes connecté ici.</p>
            <a class="bm-btn" href="<?= htmlspecialchars(MAIL_WEBMAIL_URL) ?>" target="_blank" rel="noopener">
                <i class="fa-solid fa-envelope-open"></i> Ouvrir ma boîte mail
            </a>
        <?php else: ?>
            <div class="bm-icon"><i class="fa-solid fa-envelope"></i></div>
            <h1>Tu n'as pas encore de Barker Mail</h1>
            <p class="bm-text">Crée ton adresse <strong>@<?= htmlspecialchars(MAIL_DOMAIN) ?></strong> en quelques secondes. Elle sera liée à ton compte BARKER actuel — pas besoin de te reconnecter.</p>

            <?php if ($error): ?>
                <div class="bm-error"><i class="fa-solid fa-triangle-exclamation"></i> <?= $error ?></div>
            <?php endif; ?>

            <form method="post" class="bm-form" autocomplete="off">
                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">

                <label for="mail_username">Nom de ton adresse</label>
                <div class="bm-address-input">
                    <input type="text" id="mail_username" name="mail_username" value="<?= htmlspecialchars($postedUsername) ?>" required maxlength="64" autocapitalize="off" spellcheck="false">
                    <span>@<?= htmlspecialchars(MAIL_DOMAIN) ?></span>
                </div>

                <label for="mail_password">Mot de passe de la boîte</label>
                <input type="password" id="mail_password" name="mail_password" required minlength="8" autocomplete="new-password">

                <label for="mail_password2">Confirmer le mot de passe</label>
                <input type="password" id="mail_password2" name="mail_password2" required minlength="8" autocomplete="new-password">

                <p class="bm-hint">Ce mot de passe sert uniquement à ouvrir ta boîte mail (il peut être différent de celui de ton compte BARKER).</p>

                <button type="submit" class="bm-btn"><i class="fa-solid fa-plus"></i> Créer mon Barker Mail</button>
            </form>
        <?php endif; ?>
    </div>
</main>

<style>
.bm-card { max-width: 460px; margin: 40px auto; padding: 28px 24px; background: var(--bg-card); border: 1px solid var(--border); border-radius: 14px; text-align: center; }
.bm-icon { font-size: 40px; color: var(--orange); margin-bottom: 10px; }
.bm-icon.ok { color: #2ecc71; }
.bm-card h1 { font-size: 1.25rem; margin: 0 0 10px; color: var(--text-primary); }
.bm-text { color: var(--text-secondary); font-size: .9rem; line-height: 1.45; margin: 0 0 18px; }
.bm-address { font-size: 1.1rem; font-weight: 700; color: var(--orange); margin: 6px 0 12px; word-break: break-all; }
.bm-error { background: rgba(244,33,46,.1); border: 1px solid rgba(244,33,46,.4); color: #f4212e; padding: 10px 12px; border-radius: 10px; font-size: .85rem; margin-bottom: 14px; text-align: left; }
.bm-form { text-align: left; }
.bm-form label { display: block; font-size: .8rem; font-weight: 600; color: var(--text-secondary); margin: 12px 0 5px; }
.bm-form input[type=password], .bm-address-input { width: 100%; box-sizing: border-box; background: var(--bg-input); border: 1px solid var(--border); border-radius: 10px; color: var(--text-primary); }
.bm-form input[type=password] { padding: 11px 14px; font-size: .9rem; outline: none; }
.bm-address-input { display: flex; align-items: center; padding-right: 12px; }
.bm-address-input input { flex: 1; min-width: 0; background: none; border: none; outline: none; color: var(--text-primary); padding: 11px 6px 11px 14px; font-size: .9rem; }
.bm-address-input span { color: var(--text-muted); font-size: .85rem; white-space: nowrap; }
.bm-form input:focus, .bm-address-input:focus-within { border-color: var(--orange); }
.bm-hint { font-size: .75rem; color: var(--text-muted); margin: 10px 0 16px; }
.bm-btn { display: inline-flex; align-items: center; justify-content: center; gap: 8px; width: 100%; box-sizing: border-box; padding: 12px 20px; background: var(--orange); color: #fff; border: none; border-radius: 999px; font-weight: 600; font-size: .92rem; cursor: pointer; text-decoration: none; }
.bm-btn:hover { background: var(--orange-hover); }
</style>

<?php include __DIR__ . '/../includes/footer.php'; ?>
