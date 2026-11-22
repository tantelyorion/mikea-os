<?php
// ============================================================
// BARKER MAIL — Fonctions IMAP / SMTP
// ============================================================

function ml_safe(?string $s): string
{
    return htmlspecialchars($s ?? '', ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML5, 'UTF-8');
}

function ml_decode_header(?string $raw): string
{
    if ($raw === null || $raw === '') return '';
    $parts = imap_mime_header_decode($raw);
    $out = '';
    foreach ($parts as $p) {
        $charset = ($p->charset === 'default') ? 'UTF-8' : $p->charset;
        $text = @iconv($charset, 'UTF-8//IGNORE', $p->text);
        $out .= ($text !== false) ? $text : $p->text;
    }
    return $out;
}

function ml_format_bytes(int $bytes): string
{
    if ($bytes < 1024) return $bytes . ' o';
    if ($bytes < 1024 * 1024) return round($bytes / 1024, 1) . ' Ko';
    if ($bytes < 1024 * 1024 * 1024) return round($bytes / (1024 * 1024), 1) . ' Mo';
    return round($bytes / (1024 * 1024 * 1024), 1) . ' Go';
}

function ml_time_ago($timestamp): string
{
    if (!$timestamp) return '';
    $diff = time() - $timestamp;
    if ($diff < 60) return 'à l\'instant';
    if ($diff < 3600) return floor($diff / 60) . ' min';
    if ($diff < 86400) return floor($diff / 3600) . ' h';
    if ($diff < 604800) return floor($diff / 86400) . ' j';
    return date('d M Y', $timestamp);
}

// ============================================================
// Session / Auth
// ============================================================
function ml_is_logged_in(): bool
{
    return !empty($_SESSION['ml_email']) && !empty($_SESSION['ml_password']);
}

function ml_require_login(): void
{
    if (!ml_is_logged_in()) {
        header('Location: login.php');
        exit;
    }
}

function ml_current_email(): string
{
    return $_SESSION['ml_email'] ?? '';
}

function ml_logout(): void
{
    $_SESSION = [];
    if (ini_get('session.use_cookies')) {
        $p = session_get_cookie_params();
        setcookie(session_name(), '', time() - 42000, $p['path'], $p['domain'], $p['secure'], $p['httponly']);
    }
    session_destroy();
}

// ============================================================
// Connexion IMAP
// ============================================================
function ml_imap_flags(): string
{
    $enc  = BARKERMAIL_IMAP_ENCRYPTION === 'tls' ? 'tls' : 'ssl';
    $flag = '/imap/' . $enc;
    if (defined('BARKERMAIL_IMAP_NOVALIDATE') && BARKERMAIL_IMAP_NOVALIDATE) {
        $flag .= '/novalidate-cert';
    }
    return $flag;
}

function ml_mailbox_string(string $folder = 'INBOX'): string
{
    $root = '{' . BARKERMAIL_IMAP_HOST . ':' . BARKERMAIL_IMAP_PORT . ml_imap_flags() . '}';
    return $root . imap_utf7_encode($folder);
}

/**
 * Tente une connexion IMAP avec les identifiants donnés. Utilisé au login.
 * Retourne la ressource IMAP en cas de succès, ou false sinon.
 */
function ml_try_login(string $email, string $password)
{
    $mailbox = ml_mailbox_string('INBOX');
    $conn = @imap_open($mailbox, $email, $password, 0, 1, [
        'DISABLE_AUTHENTICATOR' => 'GSSAPI',
    ]);
    return $conn ?: false;
}

/**
 * Connexion IMAP pour la session en cours, sur le dossier donné.
 */
function ml_connect(string $folder = 'INBOX')
{
    if (!ml_is_logged_in()) return false;
    $mailbox = ml_mailbox_string($folder);
    $conn = @imap_open($mailbox, $_SESSION['ml_email'], $_SESSION['ml_password'], 0, 1, [
        'DISABLE_AUTHENTICATOR' => 'GSSAPI',
    ]);
    return $conn ?: false;
}

// ============================================================
// Dossiers
// ============================================================
function ml_folder_display(string $rawName): array
{
    // rawName ex: "INBOX", "INBOX.Sent", "INBOX.Trash", "INBOX.Drafts", "INBOX.Junk"
    $short = preg_replace('/^INBOX\.?/i', '', $rawName);
    if ($short === '') $short = 'INBOX';

    $map = [
        'INBOX'  => ['label' => 'Boîte de réception', 'icon' => 'fa-inbox'],
        'Sent'   => ['label' => 'Envoyés',            'icon' => 'fa-paper-plane'],
        'Drafts' => ['label' => 'Brouillons',          'icon' => 'fa-file-pen'],
        'Trash'  => ['label' => 'Corbeille',            'icon' => 'fa-trash'],
        'Junk'   => ['label' => 'Indésirables',        'icon' => 'fa-triangle-exclamation'],
        'Spam'   => ['label' => 'Indésirables',        'icon' => 'fa-triangle-exclamation'],
    ];

    if (isset($map[$short])) {
        return $map[$short];
    }
    return ['label' => $short, 'icon' => 'fa-folder'];
}

function ml_list_folders($conn): array
{
    $root = '{' . BARKERMAIL_IMAP_HOST . ':' . BARKERMAIL_IMAP_PORT . ml_imap_flags() . '}';
    $list = @imap_list($conn, $root, '*');
    if (!$list) return [['raw' => 'INBOX'] + ml_folder_display('INBOX')];

    $folders = [];
    foreach ($list as $full) {
        $raw = str_replace($root, '', $full);
        $raw = imap_utf7_decode($raw);
        $folders[] = ['raw' => $raw] + ml_folder_display($raw);
    }

    // INBOX en premier, puis Sent/Drafts/Trash/Junk, puis le reste
    $order = ['INBOX' => 0, 'Sent' => 1, 'Drafts' => 2, 'Trash' => 3, 'Junk' => 4, 'Spam' => 4];
    usort($folders, function ($a, $b) use ($order) {
        $sa = preg_replace('/^INBOX\.?/i', '', $a['raw']) ?: 'INBOX';
        $sb = preg_replace('/^INBOX\.?/i', '', $b['raw']) ?: 'INBOX';
        $oa = $order[$sa] ?? 99;
        $ob = $order[$sb] ?? 99;
        return $oa <=> $ob ?: strcmp($sa, $sb);
    });

    return $folders;
}

function ml_find_folder_by_shortname($conn, string $shortName): ?string
{
    foreach (ml_list_folders($conn) as $f) {
        $short = preg_replace('/^INBOX\.?/i', '', $f['raw']) ?: 'INBOX';
        if (strcasecmp($short, $shortName) === 0) return $f['raw'];
    }
    return null;
}

// ============================================================
// Liste des messages
// ============================================================
function ml_get_overview($conn, string $search = '', int $page = 1, int $perPage = MAIL_PAGE_SIZE): array
{
    if ($search !== '') {
        $criteria = 'TEXT "' . str_replace('"', '', $search) . '"';
        $uids = imap_search($conn, $criteria, SE_UID) ?: [];
    } else {
        $uids = imap_search($conn, 'ALL', SE_UID) ?: [];
    }

    // Tri du plus récent au plus ancien
    rsort($uids);

    $total = count($uids);
    $offset = ($page - 1) * $perPage;
    $pageUids = array_slice($uids, $offset, $perPage);

    $messages = [];
    foreach ($pageUids as $uid) {
        $overview = imap_fetch_overview($conn, (string)$uid, FT_UID);
        if (empty($overview)) continue;
        $o = $overview[0];
        $messages[] = [
            'uid'      => $uid,
            'subject'  => ml_decode_header($o->subject ?? '(sans objet)'),
            'from'     => ml_decode_header($o->from ?? ''),
            'to'       => ml_decode_header($o->to ?? ''),
            'date'     => isset($o->udate) ? (int)$o->udate : (isset($o->date) ? strtotime($o->date) : 0),
            'seen'     => !empty($o->seen),
            'answered' => !empty($o->answered),
            'flagged'  => !empty($o->flagged),
            'size'     => (int)($o->size ?? 0),
            'attach'   => (bool)($o->hasattachment ?? false),
        ];
    }

    return [
        'messages' => $messages,
        'total'    => $total,
        'page'     => $page,
        'pages'    => max(1, (int)ceil($total / $perPage)),
    ];
}

// ============================================================
// Lecture d'un message
// ============================================================
function ml_extract_name_email(string $header): array
{
    // "Jean Dupont <jean@barker.ink>" -> ['name' => 'Jean Dupont', 'email' => 'jean@barker.ink']
    if (preg_match('/^(.*)<(.+)>$/', trim($header), $m)) {
        return ['name' => trim($m[1], " \t\"'"), 'email' => trim($m[2])];
    }
    return ['name' => '', 'email' => trim($header)];
}

function ml_get_body_and_attachments($conn, $uid, $structure, $partNumber = '', &$html = '', &$plain = '', &$attachments = [])
{
    if (!isset($structure->parts) || !is_array($structure->parts)) {
        // Message simple (une seule partie)
        $body = imap_fetchbody($conn, $uid, $partNumber === '' ? '1' : $partNumber, FT_UID | FT_PEEK);
        $body = ml_decode_part($body, $structure->encoding ?? 0);
        if (($structure->subtype ?? '') === 'HTML') {
            $html = $body;
        } else {
            $plain = $body;
        }
        return;
    }

    foreach ($structure->parts as $i => $part) {
        $num = ($partNumber === '') ? (string)($i + 1) : $partNumber . '.' . ($i + 1);

        $disposition = strtoupper($part->ifdparameters ? ($part->dparameters[0]->value ?? '') : '');
        $isAttachment = false;
        $filename = '';

        if (!empty($part->ifdparameters)) {
            foreach ($part->dparameters as $p) {
                if (strcasecmp($p->attribute, 'filename') === 0) $filename = $p->value;
            }
        }
        if (!empty($part->ifparameters)) {
            foreach ($part->parameters as $p) {
                if (strcasecmp($p->attribute, 'name') === 0 && $filename === '') $filename = $p->value;
            }
        }

        $disp = '';
        if (!empty($part->ifdisposition)) $disp = strtoupper($part->disposition);

        if ($disp === 'ATTACHMENT' || ($filename !== '' && $disp !== 'INLINE' && $part->type != 1)) {
            $attachments[] = [
                'part'     => $num,
                'filename' => $filename !== '' ? ml_decode_header($filename) : ('fichier_' . $num),
                'size'     => $part->bytes ?? 0,
            ];
            continue;
        }

        if ($part->type == 1 && isset($part->parts)) {
            // multipart imbriqué (ex: multipart/alternative dans multipart/mixed)
            ml_get_body_and_attachments($conn, $uid, $part, $num, $html, $plain, $attachments);
            continue;
        }

        if ($part->type == 0) { // TEXT
            $body = imap_fetchbody($conn, $uid, $num, FT_UID | FT_PEEK);
            $body = ml_decode_part($body, $part->encoding ?? 0);
            $charset = '';
            if (!empty($part->ifparameters)) {
                foreach ($part->parameters as $p) {
                    if (strcasecmp($p->attribute, 'charset') === 0) $charset = $p->value;
                }
            }
            if ($charset && strtoupper($charset) !== 'UTF-8') {
                $converted = @iconv($charset, 'UTF-8//IGNORE', $body);
                if ($converted !== false) $body = $converted;
            }
            if (strtoupper($part->subtype) === 'HTML') {
                $html = $body;
            } else {
                $plain = ($plain === '') ? $body : $plain . "\n" . $body;
            }
        } elseif ($filename !== '') {
            // Pièce jointe inline (image intégrée, etc.) sans disposition explicite
            $attachments[] = [
                'part'     => $num,
                'filename' => ml_decode_header($filename),
                'size'     => $part->bytes ?? 0,
            ];
        }
    }
}

function ml_decode_part(string $body, int $encoding): string
{
    switch ($encoding) {
        case 3: // BASE64
            return base64_decode($body);
        case 4: // QUOTED-PRINTABLE
            return quoted_printable_decode($body);
        default:
            return $body;
    }
}

function ml_sanitize_html_body(string $html): string
{
    // Nettoyage basique : retire scripts/styles dangereux avant affichage.
    $html = preg_replace('#<script\b[^>]*>.*?</script>#is', '', $html);
    $html = preg_replace('#<iframe\b[^>]*>.*?</iframe>#is', '', $html);
    $html = preg_replace('#\son\w+\s*=\s*"[^"]*"#i', '', $html);
    $html = preg_replace("#\son\w+\s*=\s*'[^']*'#i", '', $html);
    return $html;
}

// ============================================================
// Paramètres utilisateur (stockés par boîte mail, hors base BARKER)
// ============================================================
function ml_settings_dir(): string
{
    $dir = __DIR__ . '/../data';
    if (!is_dir($dir)) @mkdir($dir, 0770, true);
    return $dir;
}

function ml_settings_path(string $email): string
{
    return ml_settings_dir() . '/settings_' . sha1(strtolower(trim($email))) . '.json';
}

function ml_default_settings(): array
{
    return [
        'display_name'   => '',
        'signature'      => '',
        'theme'          => 'dark',
        'page_size'      => defined('MAIL_PAGE_SIZE') ? MAIL_PAGE_SIZE : 25,
        'confirm_delete' => true,
    ];
}

function ml_get_settings(?string $email = null): array
{
    $email = $email ?? ml_current_email();
    if ($email === '') return ml_default_settings();
    $path = ml_settings_path($email);
    if (!is_file($path)) return ml_default_settings();
    $raw = @file_get_contents($path);
    $data = json_decode((string)$raw, true);
    if (!is_array($data)) return ml_default_settings();
    return array_merge(ml_default_settings(), $data);
}

function ml_save_settings(string $email, array $settings): bool
{
    if ($email === '') return false;
    $merged = array_merge(ml_get_settings($email), $settings);
    $merged['page_size']      = max(10, min(100, (int)$merged['page_size']));
    $merged['theme']          = ($merged['theme'] === 'light') ? 'light' : 'dark';
    $merged['display_name']   = trim((string)$merged['display_name']);
    $merged['signature']      = (string)$merged['signature'];
    $merged['confirm_delete'] = (bool)$merged['confirm_delete'];
    $path = ml_settings_path($email);
    return @file_put_contents($path, json_encode($merged, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT), LOCK_EX) !== false;
}

function ml_toggle_theme(string $email): string
{
    $settings = ml_get_settings($email);
    $settings['theme'] = ($settings['theme'] === 'light') ? 'dark' : 'light';
    ml_save_settings($email, $settings);
    return $settings['theme'];
}

function ml_display_name(): string
{
    $settings = ml_get_settings();
    return $settings['display_name'] !== '' ? $settings['display_name'] : ml_current_email();
}

// ============================================================
// Envoi (SMTP via PHPMailer)
// ============================================================
function ml_send_mail(string $to, string $subject, string $bodyHtml, array $attachments = [], string $cc = '', string $bcc = '', string $fromName = ''): array
{
    require_once dirname(__DIR__, 2) . '/includes/PHPMailer/src/Exception.php';
    require_once dirname(__DIR__, 2) . '/includes/PHPMailer/src/PHPMailer.php';
    require_once dirname(__DIR__, 2) . '/includes/PHPMailer/src/SMTP.php';

    $mail = new \PHPMailer\PHPMailer\PHPMailer(true);

    try {
        $mail->isSMTP();
        $mail->Host       = BARKERMAIL_SMTP_HOST;
        $mail->SMTPAuth   = true;
        $mail->Username   = $_SESSION['ml_email'];
        $mail->Password   = $_SESSION['ml_password'];
        $mail->SMTPSecure = BARKERMAIL_SMTP_ENCRYPTION === 'tls'
            ? \PHPMailer\PHPMailer\PHPMailer::ENCRYPTION_STARTTLS
            : \PHPMailer\PHPMailer\PHPMailer::ENCRYPTION_SMTPS;
        $mail->Port       = BARKERMAIL_SMTP_PORT;
        $mail->CharSet    = 'UTF-8';

        $mail->setFrom($_SESSION['ml_email'], $fromName !== '' ? $fromName : $_SESSION['ml_email']);
        foreach (array_filter(array_map('trim', explode(',', $to))) as $addr) {
            $mail->addAddress($addr);
        }
        foreach (array_filter(array_map('trim', explode(',', $cc))) as $addr) {
            $mail->addCC($addr);
        }
        foreach (array_filter(array_map('trim', explode(',', $bcc))) as $addr) {
            $mail->addBCC($addr);
        }

        foreach ($attachments as $att) {
            if (!empty($att['tmp_name']) && is_uploaded_file($att['tmp_name'])) {
                $mail->addAttachment($att['tmp_name'], $att['name']);
            }
        }

        $mail->isHTML(true);
        $mail->Subject = $subject;
        $mail->Body    = $bodyHtml;
        $mail->AltBody = trim(strip_tags(str_replace(['<br>', '<br/>', '<br />'], "\n", $bodyHtml)));

        $mail->send();
        return ['ok' => true, 'error' => null];
    } catch (\Throwable $e) {
        return ['ok' => false, 'error' => $mail->ErrorInfo ?: $e->getMessage()];
    }
}
