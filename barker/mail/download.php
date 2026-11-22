<?php
require_once __DIR__ . '/config.php';
ml_require_login();

$folder = trim($_GET['folder'] ?? 'INBOX');
if ($folder === '') $folder = 'INBOX';
$uid  = (int)($_GET['uid'] ?? 0);
$part = trim($_GET['part'] ?? '');
$name = trim($_GET['name'] ?? 'fichier');

if ($uid <= 0 || $part === '') {
    http_response_code(400);
    die('Requête invalide.');
}

$conn = ml_connect($folder);
if ($conn === false) {
    http_response_code(500);
    die('Connexion à Barker Mail impossible.');
}

$structure = imap_fetchstructure($conn, $uid, FT_UID);
$body = imap_fetchbody($conn, $uid, $part, FT_UID);

// Déterminer l'encodage de cette partie précise pour bien décoder les octets
$targetPart = $structure;
if (!empty($structure->parts)) {
    $segments = explode('.', $part);
    $node = $structure;
    foreach ($segments as $seg) {
        $idx = ((int)$seg) - 1;
        if (isset($node->parts[$idx])) {
            $node = $node->parts[$idx];
        }
    }
    $targetPart = $node;
}

$encoding = $targetPart->encoding ?? 0;
$data = ml_decode_part($body, $encoding);

$mime = 'application/octet-stream';
if (isset($targetPart->type, $targetPart->subtype)) {
    $types = [0 => 'text', 1 => 'multipart', 2 => 'message', 3 => 'application', 4 => 'audio', 5 => 'image', 6 => 'video', 7 => 'other'];
    $mime = ($types[$targetPart->type] ?? 'application') . '/' . strtolower($targetPart->subtype);
}

imap_close($conn);

header('Content-Type: ' . $mime);
header('Content-Length: ' . strlen($data));
header('Content-Disposition: attachment; filename="' . str_replace('"', '', $name) . '"');
header('X-Content-Type-Options: nosniff');
echo $data;
