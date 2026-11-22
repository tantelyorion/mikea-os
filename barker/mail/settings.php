<?php
require_once __DIR__ . '/config.php';
ml_require_login();

$email = ml_current_email();
$settings = ml_get_settings($email);
$success = '';
$error = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $newSettings = [
        'display_name'   => trim($_POST['display_name'] ?? ''),
        'signature'      => trim($_POST['signature'] ?? ''),
        'theme'          => ($_POST['theme'] ?? 'dark') === 'light' ? 'light' : 'dark',
        'page_size'      => (int)($_POST['page_size'] ?? MAIL_PAGE_SIZE),
        'confirm_delete' => isset($_POST['confirm_delete']),
    ];

    if (ml_save_settings($email, $newSettings)) {
        $settings = ml_get_settings($email);
        $success = 'Paramètres enregistrés.';
    } else {
        $error = "Impossible d'enregistrer les paramètres pour le moment. Réessaie dans un instant.";
    }
}

$conn = ml_connect('INBOX');
$folders = ($conn !== false) ? ml_list_folders($conn) : [];
if ($conn !== false) imap_close($conn);

$pageTitle = 'Paramètres — Barker Mail';
$activeFolder = '';
include __DIR__ . '/includes/chrome_top.php';
?>
    <main class="ml-main">
        <div class="ml-toolbar">
            <a href="index.php" class="ml-btn"><i class="fa-solid fa-arrow-left"></i> Retour</a>
        </div>

        <?php if ($error): ?>
            <div class="ml-alert ml-alert-error"><i class="fa-solid fa-triangle-exclamation"></i> <span><?= ml_safe($error) ?></span></div>
        <?php endif; ?>
        <?php if ($success): ?>
            <div class="ml-alert ml-alert-success"><i class="fa-solid fa-check-circle"></i> <span><?= ml_safe($success) ?></span></div>
        <?php endif; ?>

        <div class="ml-settings-card">
            <h1 class="ml-settings-title"><i class="fa-solid fa-gear"></i> Paramètres</h1>
            <p class="ml-settings-sub">Connecté en tant que <strong><?= ml_safe($email) ?></strong></p>

            <form method="POST" action="settings.php" class="ml-settings-form">

                <section class="ml-settings-section">
                    <h2>Identité</h2>
                    <div class="ml-field">
                        <label for="display_name">Nom affiché</label>
                        <input type="text" id="display_name" name="display_name" placeholder="<?= ml_safe($email) ?>" value="<?= ml_safe($settings['display_name']) ?>">
                        <p class="ml-field-hint">C'est le nom que tes destinataires verront comme expéditeur.</p>
                    </div>
                    <div class="ml-field">
                        <label for="signature">Signature</label>
                        <textarea id="signature" name="signature" class="ml-signature-input" placeholder="Ajoutée automatiquement en bas de tes nouveaux messages."><?= ml_safe($settings['signature']) ?></textarea>
                    </div>
                </section>

                <section class="ml-settings-section">
                    <h2>Apparence</h2>
                    <div class="ml-theme-choice">
                        <label class="ml-radio-card <?= $settings['theme'] === 'dark' ? 'checked' : '' ?>">
                            <input type="radio" name="theme" value="dark" <?= $settings['theme'] === 'dark' ? 'checked' : '' ?>>
                            <i class="fa-solid fa-moon"></i>
                            <span>Sombre</span>
                        </label>
                        <label class="ml-radio-card <?= $settings['theme'] === 'light' ? 'checked' : '' ?>">
                            <input type="radio" name="theme" value="light" <?= $settings['theme'] === 'light' ? 'checked' : '' ?>>
                            <i class="fa-solid fa-sun"></i>
                            <span>Clair</span>
                        </label>
                    </div>
                </section>

                <section class="ml-settings-section">
                    <h2>Lecture des messages</h2>
                    <div class="ml-field">
                        <label for="page_size">Messages par page</label>
                        <select id="page_size" name="page_size">
                            <?php foreach ([10, 25, 50, 100] as $n): ?>
                                <option value="<?= $n ?>" <?= (int)$settings['page_size'] === $n ? 'selected' : '' ?>><?= $n ?></option>
                            <?php endforeach; ?>
                        </select>
                    </div>
                    <label class="ml-checkbox-row">
                        <input type="checkbox" name="confirm_delete" <?= $settings['confirm_delete'] ? 'checked' : '' ?>>
                        <span>Demander une confirmation avant une suppression définitive</span>
                    </label>
                </section>

                <div class="ml-settings-actions">
                    <button type="submit" class="ml-btn ml-btn-primary"><i class="fa-solid fa-check"></i> Enregistrer</button>
                </div>
            </form>
        </div>
    </main>
</div>
</body>
</html>
<?php ?>
