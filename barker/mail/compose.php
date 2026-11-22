<?php
require_once __DIR__ . '/config.php';
ml_require_login();

$mode   = $_GET['mode'] ?? 'new';
$srcFolder = trim($_GET['folder'] ?? 'INBOX');
$srcUid = (int)($_GET['uid'] ?? 0);

$conn = ml_connect($srcFolder !== '' ? $srcFolder : 'INBOX');
$folders = ($conn !== false) ? ml_list_folders($conn) : [];
$settings = ml_get_settings();
$displayName = ml_display_name();

$toValue = '';
$ccValue = '';
$subjectValue = '';
$bodyValue = '';
$error = '';
$success = '';

// ===== Pré-remplissage (réponse / transfert) =====
if ($conn !== false && $srcUid > 0 && in_array($mode, ['reply', 'replyall', 'forward'], true)) {
    $msgNum = imap_msgno($conn, $srcUid);
    if ($msgNum) {
        $headerInfo = imap_headerinfo($conn, $msgNum);
        $structure  = imap_fetchstructure($conn, $srcUid, FT_UID);
        $html = ''; $plain = ''; $atts = [];
        ml_get_body_and_attachments($conn, $srcUid, $structure, '', $html, $plain, $atts);

        $origSubject = ml_decode_header(isset($headerInfo->subject) ? imap_utf8($headerInfo->subject) : '');
        $fromEmail = '';
        $fromName  = '';
        if (!empty($headerInfo->from)) {
            $o = $headerInfo->from[0];
            $fromEmail = ($o->mailbox ?? '') . '@' . ($o->host ?? '');
            $fromName  = isset($o->personal) ? ml_decode_header(imap_utf8($o->personal)) : $fromEmail;
        }
        $dateTs = isset($headerInfo->udate) ? (int)$headerInfo->udate : strtotime($headerInfo->date ?? 'now');
        $quoted = $html !== '' ? $html : nl2br(ml_safe($plain));

        if ($mode === 'reply' || $mode === 'replyall') {
            $toValue = $fromEmail;
            if ($mode === 'replyall' && !empty($headerInfo->to)) {
                $others = [];
                foreach ($headerInfo->to as $o) {
                    $addr = ($o->mailbox ?? '') . '@' . ($o->host ?? '');
                    if (strcasecmp($addr, ml_current_email()) !== 0) $others[] = $addr;
                }
                if (!empty($others)) $ccValue = implode(', ', $others);
            }
            $subjectValue = (stripos($origSubject, 're:') === 0) ? $origSubject : 'Re: ' . $origSubject;
            $bodyValue = '<br><br><div style="border-left:2px solid #333639;padding-left:12px;color:#71767b;">'
                . 'Le ' . ($dateTs ? date('d M Y à H:i', $dateTs) : '') . ', ' . ml_safe($fromName) . ' a écrit :<br>'
                . $quoted . '</div>';
        } elseif ($mode === 'forward') {
            $subjectValue = (stripos($origSubject, 'fwd:') === 0) ? $origSubject : 'Fwd: ' . $origSubject;
            $bodyValue = '<br><br><div style="border-left:2px solid #333639;padding-left:12px;color:#71767b;">'
                . '---------- Message transféré ----------<br>'
                . 'De : ' . ml_safe($fromName) . ' &lt;' . ml_safe($fromEmail) . '&gt;<br>'
                . 'Date : ' . ($dateTs ? date('d M Y à H:i', $dateTs) : '') . '<br>'
                . 'Objet : ' . ml_safe($origSubject) . '<br><br>'
                . $quoted . '</div>';
        }
    }
}

// ===== Envoi =====
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $to      = trim($_POST['to'] ?? '');
    $cc      = trim($_POST['cc'] ?? '');
    $bcc     = trim($_POST['bcc'] ?? '');
    $subject = trim($_POST['subject'] ?? '(sans objet)');
    $bodyHtml = $_POST['body'] ?? '';
    $bodyHtml = nl2br(ml_safe($bodyHtml)); // le champ est un textarea texte brut, on le convertit proprement

    $toValue = $to; $ccValue = $cc; $subjectValue = $subject;

    if (empty($to) || !filter_var(trim(explode(',', $to)[0]), FILTER_VALIDATE_EMAIL)) {
        $error = 'Merci d\'indiquer au moins un destinataire valide.';
    } else {
        $attachments = [];
        if (!empty($_FILES['attachments']['name'][0] ?? '')) {
            foreach ($_FILES['attachments']['name'] as $i => $fname) {
                if ($_FILES['attachments']['error'][$i] === UPLOAD_ERR_OK) {
                    $attachments[] = [
                        'tmp_name' => $_FILES['attachments']['tmp_name'][$i],
                        'name'     => $fname,
                    ];
                }
            }
        }

        $result = ml_send_mail($to, $subject, $bodyHtml, $attachments, $cc, $bcc, $displayName);

        if ($result['ok']) {
            // Copie dans le dossier "Envoyés"
            if ($conn !== false) {
                $sentRaw = ml_find_folder_by_shortname($conn, 'Sent');
                if ($sentRaw) {
                    $headerLines = "To: {$to}\r\nSubject: {$subject}\r\n";
                    if ($cc !== '') $headerLines .= "Cc: {$cc}\r\n";
                    $headerLines .= "MIME-Version: 1.0\r\nContent-Type: text/html; charset=UTF-8\r\nDate: " . date('r') . "\r\n\r\n";
                    @imap_append($conn, ml_mailbox_string($sentRaw), $headerLines . $bodyHtml, "\\Seen");
                }
            }
            $success = 'Message envoyé !';
            $toValue = $ccValue = $subjectValue = $bodyValue = '';
        } else {
            $error = 'Échec de l\'envoi : ' . ($result['error'] ?? 'erreur inconnue');
            $bodyValue = $_POST['body'] ?? '';
        }
    }
}

// ===== Signature automatique (uniquement à l'affichage initial, pas après un envoi raté) =====
$textareaBody = strip_tags(str_replace(['<br>', '<br/>', '<br />'], "\n", $bodyValue));
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    $sig = trim($settings['signature']);
    if ($sig !== '') {
        $sigBlock = "-- \n" . $sig;
        $textareaBody = ($textareaBody !== '')
            ? "\n\n" . $sigBlock . "\n\n" . $textareaBody
            : "\n\n" . $sigBlock;
    }
}

$modeLabels = ['reply' => 'Répondre', 'replyall' => 'Répondre à tous', 'forward' => 'Transférer', 'new' => 'Nouveau message'];
$pageTitle = ($modeLabels[$mode] ?? 'Nouveau message') . ' — Barker Mail';
$activeFolder = $srcFolder;
include __DIR__ . '/includes/chrome_top.php';
?>
    <main class="ml-main">
        <div class="ml-toolbar">
            <a href="<?= $srcUid > 0 ? 'view.php?folder=' . urlencode($srcFolder) . '&uid=' . $srcUid : 'index.php' ?>" class="ml-btn"><i class="fa-solid fa-arrow-left"></i> Retour</a>
        </div>

        <?php if ($error): ?>
            <div class="ml-alert ml-alert-error"><i class="fa-solid fa-triangle-exclamation"></i> <span><?= ml_safe($error) ?></span></div>
        <?php endif; ?>
        <?php if ($success): ?>
            <div class="ml-alert ml-alert-success"><i class="fa-solid fa-check-circle"></i> <span><?= ml_safe($success) ?></span></div>
        <?php endif; ?>

        <div class="ml-compose-card">
            <form method="POST" action="compose.php?mode=<?= ml_safe($mode) ?>&folder=<?= urlencode($srcFolder) ?>&uid=<?= $srcUid ?>" enctype="multipart/form-data">
                <div class="ml-field">
                    <label for="to">À</label>
                    <input type="text" id="to" name="to" placeholder="destinataire@exemple.com, autre@exemple.com" value="<?= ml_safe($toValue) ?>" required>
                </div>

                <span class="ml-cc-toggle" onclick="document.getElementById('ccFields').classList.toggle('active')">Cc / Cci</span>

                <div class="ml-cc-fields <?= $ccValue !== '' ? 'active' : '' ?>" id="ccFields">
                    <div class="ml-field">
                        <label for="cc">Cc</label>
                        <input type="text" id="cc" name="cc" value="<?= ml_safe($ccValue) ?>">
                    </div>
                    <div class="ml-field">
                        <label for="bcc">Cci</label>
                        <input type="text" id="bcc" name="bcc">
                    </div>
                </div>

                <div class="ml-field">
                    <label for="subject">Objet</label>
                    <input type="text" id="subject" name="subject" value="<?= ml_safe($subjectValue) ?>">
                </div>

                <div class="ml-field">
                    <label for="body">Message</label>
                    <textarea id="body" name="body"><?= ml_safe($textareaBody) ?></textarea>
                </div>

                <div class="ml-field">
                    <label for="attachments">Pièces jointes</label>
                    <label for="attachments" class="ml-file-drop">
                        <i class="fa-solid fa-paperclip"></i>
                        <span>Cliquer pour choisir un ou plusieurs fichiers</span>
                    </label>
                    <input type="file" id="attachments" name="attachments[]" class="ml-file-input" multiple hidden>
                    <div class="ml-file-list" id="attachmentsList"></div>
                </div>

                <button type="submit" class="ml-btn ml-btn-primary"><i class="fa-solid fa-paper-plane"></i> Envoyer</button>
            </form>
        </div>
    </main>
</div>
</body>
</html>
<?php if ($conn !== false) imap_close($conn); ?>
