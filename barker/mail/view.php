<?php
require_once __DIR__ . '/config.php';
ml_require_login();

$folder = trim($_GET['folder'] ?? 'INBOX');
if ($folder === '') $folder = 'INBOX';
$uid = (int)($_GET['uid'] ?? 0);

$conn = ml_connect($folder);

if ($conn === false) {
    $pageTitle = 'Barker Mail';
    $activeFolder = $folder;
    $folders = [];
    include __DIR__ . '/includes/chrome_top.php';
    echo '<main class="ml-main"><div class="ml-alert ml-alert-error"><i class="fa-solid fa-triangle-exclamation"></i> Connexion à Barker Mail impossible.</div></main></div></body></html>';
    exit;
}

$folders  = ml_list_folders($conn);
$settings = ml_get_settings();

// ===== Actions rapides (POST) =====
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = $_POST['action'] ?? '';
    $shortFolder = preg_replace('/^INBOX\.?/i', '', $folder) ?: 'INBOX';

    if ($action === 'trash') {
        if (strcasecmp($shortFolder, 'Trash') === 0) {
            imap_delete($conn, (string)$uid, FT_UID);
        } else {
            $trashRaw = ml_find_folder_by_shortname($conn, 'Trash');
            if ($trashRaw) @imap_mail_move($conn, (string)$uid, imap_utf7_encode($trashRaw), CP_UID);
        }
        imap_expunge($conn);
        imap_close($conn);
        header('Location: index.php?folder=' . urlencode($folder));
        exit;
    } elseif ($action === 'unread') {
        imap_clearflag_full($conn, (string)$uid, '\\Seen', ST_UID);
        imap_close($conn);
        header('Location: index.php?folder=' . urlencode($folder));
        exit;
    } elseif ($action === 'flag') {
        imap_setflag_full($conn, (string)$uid, '\\Flagged', ST_UID);
        header('Location: view.php?folder=' . urlencode($folder) . '&uid=' . $uid);
        exit;
    } elseif ($action === 'unflag') {
        imap_clearflag_full($conn, (string)$uid, '\\Flagged', ST_UID);
        header('Location: view.php?folder=' . urlencode($folder) . '&uid=' . $uid);
        exit;
    }
}

$msgNum = imap_msgno($conn, $uid);
if (!$msgNum) {
    $pageTitle = 'Message introuvable — Barker Mail';
    $activeFolder = $folder;
    include __DIR__ . '/includes/chrome_top.php';
    echo '<main class="ml-main"><div class="ml-empty"><i class="fa-solid fa-circle-exclamation"></i> Ce message n\'existe plus.</div></main></div></body></html>';
    imap_close($conn);
    exit;
}

$headerInfo = imap_headerinfo($conn, $msgNum);
$structure  = imap_fetchstructure($conn, $uid, FT_UID);

$html = '';
$plain = '';
$attachments = [];
ml_get_body_and_attachments($conn, $uid, $structure, '', $html, $plain, $attachments);

// Marquer comme lu
imap_setflag_full($conn, (string)$uid, '\\Seen', ST_UID);

$subject = ml_decode_header(isset($headerInfo->subject) ? imap_utf8($headerInfo->subject) : '(sans objet)');
$fromRaw = '';
if (!empty($headerInfo->from)) {
    $o = $headerInfo->from[0];
    $fromRaw = (isset($o->personal) ? ml_decode_header(imap_utf8($o->personal)) . ' ' : '') . '<' . ($o->mailbox ?? '') . '@' . ($o->host ?? '') . '>';
}
$fromParts = ml_extract_name_email($fromRaw);

$toDisplay = [];
if (!empty($headerInfo->to)) {
    foreach ($headerInfo->to as $o) {
        $toDisplay[] = ($o->mailbox ?? '') . '@' . ($o->host ?? '');
    }
}

$isFlagged = !empty($headerInfo->Flagged) && trim($headerInfo->Flagged) === 'F';
$dateTs = isset($headerInfo->udate) ? (int)$headerInfo->udate : strtotime($headerInfo->date ?? 'now');

$bodyToShow = $html !== '' ? ml_sanitize_html_body($html) : nl2br(ml_safe($plain));

$pageTitle = $subject . ' — Barker Mail';
$activeFolder = $folder;
include __DIR__ . '/includes/chrome_top.php';
?>
    <main class="ml-main">
        <div class="ml-toolbar">
            <a href="index.php?folder=<?= urlencode($folder) ?>" class="ml-btn"><i class="fa-solid fa-arrow-left"></i> Retour</a>
        </div>

        <div class="ml-msg-card">
            <h1 class="ml-msg-subject"><?= ml_safe($subject) ?></h1>

            <div class="ml-msg-meta">
                <div>
                    <div class="ml-msg-from-name"><?= ml_safe($fromParts['name'] !== '' ? $fromParts['name'] : $fromParts['email']) ?></div>
                    <div class="ml-msg-from-email">&lt;<?= ml_safe($fromParts['email']) ?>&gt;</div>
                    <?php if (!empty($toDisplay)): ?>
                        <div class="ml-msg-to">À : <?= ml_safe(implode(', ', $toDisplay)) ?></div>
                    <?php endif; ?>
                </div>
                <div class="ml-msg-date"><?= $dateTs ? date('d M Y à H:i', $dateTs) : '' ?></div>
            </div>

            <div class="ml-msg-body"><?= $bodyToShow ?></div>

            <?php if (!empty($attachments)): ?>
            <div class="ml-attachments">
                <div class="ml-attachments-title">Pièces jointes (<?= count($attachments) ?>)</div>
                <?php foreach ($attachments as $att): ?>
                    <a class="ml-attachment" href="download.php?folder=<?= urlencode($folder) ?>&uid=<?= $uid ?>&part=<?= urlencode($att['part']) ?>&name=<?= urlencode($att['filename']) ?>">
                        <i class="fa-solid fa-paperclip"></i>
                        <span><?= ml_safe($att['filename']) ?></span>
                        <span style="color:var(--ml-text-muted);">(<?= ml_format_bytes((int)$att['size']) ?>)</span>
                    </a>
                <?php endforeach; ?>
            </div>
            <?php endif; ?>

            <div class="ml-msg-actions">
                <a class="ml-btn ml-btn-primary" href="compose.php?mode=reply&folder=<?= urlencode($folder) ?>&uid=<?= $uid ?>"><i class="fa-solid fa-reply"></i> Répondre</a>
                <a class="ml-btn" href="compose.php?mode=replyall&folder=<?= urlencode($folder) ?>&uid=<?= $uid ?>"><i class="fa-solid fa-reply-all"></i> Répondre à tous</a>
                <a class="ml-btn" href="compose.php?mode=forward&folder=<?= urlencode($folder) ?>&uid=<?= $uid ?>"><i class="fa-solid fa-share"></i> Transférer</a>

                <form method="POST" class="ml-inline-form">
                    <input type="hidden" name="action" value="<?= $isFlagged ? 'unflag' : 'flag' ?>">
                    <button type="submit" class="ml-btn"><i class="fa-solid fa-star"></i> <?= $isFlagged ? 'Retirer le marqueur' : 'Marquer' ?></button>
                </form>
                <form method="POST" class="ml-inline-form">
                    <input type="hidden" name="action" value="unread">
                    <button type="submit" class="ml-btn"><i class="fa-solid fa-envelope"></i> Non lu</button>
                </form>
                <form method="POST" class="ml-inline-form"<?= $settings['confirm_delete'] ? " onsubmit=\"return confirm('Mettre ce message à la corbeille ?');\"" : '' ?>>
                    <input type="hidden" name="action" value="trash">
                    <button type="submit" class="ml-btn ml-btn-danger"><i class="fa-solid fa-trash"></i> Corbeille</button>
                </form>
            </div>
        </div>
    </main>
</div>
</body>
</html>
<?php imap_close($conn); ?>
