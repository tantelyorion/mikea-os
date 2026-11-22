<?php
require_once __DIR__ . '/config.php';
ml_require_login();

$folder = $_GET['folder'] ?? 'INBOX';
$folder = trim($folder);
if ($folder === '') $folder = 'INBOX';

$search = trim($_GET['q'] ?? '');
$page   = max(1, (int)($_GET['page'] ?? 1));

$settings = ml_get_settings();
$perPage  = $settings['page_size'] ?? MAIL_PAGE_SIZE;

$conn = ml_connect($folder);

$flashError = '';
if ($conn === false) {
    $flashError = 'Connexion à Barker Mail impossible pour le moment. Réessaie dans un instant.';
    $folders = [];
    $overview = ['messages' => [], 'total' => 0, 'page' => 1, 'pages' => 1];
} else {
    $folders  = ml_list_folders($conn);
    $overview = ml_get_overview($conn, $search, $page, $perPage);
}

// ===== Actions groupées (corbeille / suppression définitive / marquer lu) =====
if ($conn !== false && $_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = $_POST['bulk_action'] ?? '';
    $uids   = array_map('intval', $_POST['uid'] ?? []);

    if (!empty($uids) && $action !== '') {
        $shortFolder = preg_replace('/^INBOX\.?/i', '', $folder) ?: 'INBOX';
        $isTrash = (strcasecmp($shortFolder, 'Trash') === 0);

        if ($action === 'trash') {
            if ($isTrash) {
                foreach ($uids as $uid) imap_delete($conn, (string)$uid, FT_UID);
                imap_expunge($conn);
            } else {
                $trashRaw = ml_find_folder_by_shortname($conn, 'Trash');
                if ($trashRaw) {
                    foreach ($uids as $uid) {
                        @imap_mail_move($conn, (string)$uid, imap_utf7_encode($trashRaw), CP_UID);
                    }
                    imap_expunge($conn);
                }
            }
        } elseif ($action === 'delete') {
            foreach ($uids as $uid) imap_delete($conn, (string)$uid, FT_UID);
            imap_expunge($conn);
        } elseif ($action === 'read') {
            foreach ($uids as $uid) imap_setflag_full($conn, (string)$uid, '\\Seen', ST_UID);
        } elseif ($action === 'unread') {
            foreach ($uids as $uid) imap_clearflag_full($conn, (string)$uid, '\\Seen', ST_UID);
        }
    }

    header('Location: index.php?folder=' . urlencode($folder) . ($search !== '' ? '&q=' . urlencode($search) : ''));
    exit;
}

$pageTitle = ml_folder_display($folder)['label'] . ' — Barker Mail';
$activeFolder = $folder;
include __DIR__ . '/includes/chrome_top.php';
?>
    <main class="ml-main">

        <?php if ($flashError): ?>
            <div class="ml-alert ml-alert-error"><i class="fa-solid fa-triangle-exclamation"></i> <span><?= ml_safe($flashError) ?></span></div>
        <?php endif; ?>

        <div class="ml-toolbar">
            <form class="ml-search-form" method="GET" action="index.php">
                <input type="hidden" name="folder" value="<?= ml_safe($folder) ?>">
                <input type="text" name="q" class="ml-search-input" placeholder="Rechercher dans <?= ml_safe(ml_folder_display($folder)['label']) ?>…" value="<?= ml_safe($search) ?>">
                <button type="submit" class="ml-search-btn"><i class="fa-solid fa-magnifying-glass"></i></button>
            </form>
            <a href="index.php?folder=<?= urlencode($folder) ?>" class="ml-icon-btn" title="Actualiser"><i class="fa-solid fa-rotate"></i></a>
        </div>

        <?php if ($conn !== false): ?>
        <form method="POST" action="index.php?folder=<?= urlencode($folder) ?>&q=<?= urlencode($search) ?>" id="bulkForm">
            <div class="ml-bulk-bar">
                <label class="ml-select-all">
                    <input type="checkbox" id="selectAll" onclick="document.querySelectorAll('.rowcheck').forEach(c=>c.checked=this.checked)">
                    <span>Tout sélectionner</span>
                </label>
                <div class="ml-spacer"></div>
                <button type="submit" name="bulk_action" value="read" class="ml-btn" title="Marquer lu"><i class="fa-solid fa-envelope-open"></i><span class="ml-btn-label">Lu</span></button>
                <button type="submit" name="bulk_action" value="unread" class="ml-btn" title="Marquer non lu"><i class="fa-solid fa-envelope"></i><span class="ml-btn-label">Non lu</span></button>
                <button type="submit" name="bulk_action" value="trash" class="ml-btn ml-btn-danger" title="Mettre à la corbeille"><i class="fa-solid fa-trash"></i><span class="ml-btn-label">Corbeille</span></button>
                <?php if (strcasecmp(preg_replace('/^INBOX\.?/i', '', $folder) ?: 'INBOX', 'Trash') === 0): ?>
                    <button type="submit" name="bulk_action" value="delete" class="ml-btn ml-btn-danger" title="Supprimer définitivement"<?= $settings['confirm_delete'] ? " onclick=\"return confirm('Supprimer définitivement les messages sélectionnés ?');\"" : '' ?>><i class="fa-solid fa-fire"></i><span class="ml-btn-label">Supprimer</span></button>
                <?php endif; ?>
            </div>

            <?php if (empty($overview['messages'])): ?>
                <div class="ml-empty">
                    <i class="fa-solid fa-inbox"></i>
                    <?= $search !== '' ? 'Aucun résultat pour « ' . ml_safe($search) . ' ».' : 'Ce dossier est vide.' ?>
                </div>
            <?php else: ?>
                <div class="ml-list">
                    <?php foreach ($overview['messages'] as $m):
                        $fromParts = ml_extract_name_email($m['from']);
                        $fromDisplay = $fromParts['name'] !== '' ? $fromParts['name'] : $fromParts['email'];
                    ?>
                    <div class="ml-row <?= !$m['seen'] ? 'unread' : '' ?>" onclick="if(event.target.type!=='checkbox') window.location='view.php?folder=<?= urlencode($folder) ?>&uid=<?= (int)$m['uid'] ?>'">
                        <span class="ml-row-check"><input type="checkbox" class="rowcheck" name="uid[]" value="<?= (int)$m['uid'] ?>" onclick="event.stopPropagation()"></span>
                        <span class="ml-row-star <?= $m['flagged'] ? 'flagged' : '' ?>"><i class="fa-solid fa-star"></i></span>
                        <span class="ml-row-main">
                            <span class="ml-row-top">
                                <span class="ml-row-from"><?= ml_safe($fromDisplay) ?></span>
                                <span class="ml-row-date"><?= ml_time_ago($m['date']) ?></span>
                            </span>
                            <span class="ml-row-top">
                                <span class="ml-row-subject"><?= ml_safe($m['subject']) ?></span>
                                <?php if ($m['attach']): ?><span class="ml-row-attach"><i class="fa-solid fa-paperclip"></i></span><?php endif; ?>
                            </span>
                        </span>
                    </div>
                    <?php endforeach; ?>
                </div>

                <?php if ($overview['pages'] > 1): ?>
                <div class="ml-pagination">
                    <a class="<?= $page <= 1 ? 'disabled' : '' ?>" href="index.php?folder=<?= urlencode($folder) ?>&q=<?= urlencode($search) ?>&page=<?= $page - 1 ?>"><i class="fa-solid fa-chevron-left"></i> Précédent</a>
                    <span>Page <?= $page ?> / <?= $overview['pages'] ?></span>
                    <a class="<?= $page >= $overview['pages'] ? 'disabled' : '' ?>" href="index.php?folder=<?= urlencode($folder) ?>&q=<?= urlencode($search) ?>&page=<?= $page + 1 ?>">Suivant <i class="fa-solid fa-chevron-right"></i></a>
                </div>
                <?php endif; ?>
            <?php endif; ?>
        </form>
        <?php endif; ?>

    </main>
</div>
</body>
</html>
<?php if ($conn !== false) imap_close($conn); ?>
