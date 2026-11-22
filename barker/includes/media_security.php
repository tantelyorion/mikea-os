<?php
// ============================================================
// BARKER — Sécurité des uploads + Compression média
// ============================================================
// Objectifs :
//  1) Ne JAMAIS faire confiance au type MIME ni à l'extension envoyés
//     par le navigateur (facilement falsifiables) : on détecte le
//     vrai type du fichier côté serveur (finfo + getimagesize).
//  2) Générer un nom de fichier aléatoire côté serveur (jamais basé
//     sur le nom fourni par le client) pour empêcher tout upload de
//     script déguisé (ex: shell.php renommé en photo.jpg).
//  3) Compresser automatiquement les images et vidéos importées pour
//     réduire leur poids tout en gardant une qualité correcte.
// ============================================================

// Tables MIME réel -> extension autorisée
const BARKER_IMAGE_MIME_MAP = [
    'image/jpeg' => 'jpg',
    'image/png'  => 'png',
    'image/webp' => 'webp',
    'image/gif'  => 'gif',
];

const BARKER_VIDEO_MIME_MAP = [
    'video/mp4'        => 'mp4',
    'video/webm'       => 'webm',
    'video/quicktime'  => 'mov',
    'video/x-matroska' => 'mkv',
];

const BARKER_DOCUMENT_MIME_MAP = [
    'application/pdf'    => 'pdf',
    'application/msword' => 'doc',
    'application/vnd.openxmlformats-officedocument.wordprocessingml.document' => 'docx',
    'application/vnd.ms-excel' => 'xls',
    'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet' => 'xlsx',
    'application/vnd.ms-powerpoint' => 'ppt',
    'application/vnd.openxmlformats-officedocument.presentationml.presentation' => 'pptx',
    'text/plain' => 'txt',
    'application/zip' => 'zip',
];

/**
 * Détecte le vrai type MIME d'un fichier déjà uploadé (tmp_name) en
 * inspectant son contenu, pas son nom ni son Content-Type déclaré.
 */
function barker_detect_mime(string $tmpPath): ?string
{
    if (!is_file($tmpPath)) return null;
    if (function_exists('finfo_open')) {
        $finfo = finfo_open(FILEINFO_MIME_TYPE);
        $mime  = finfo_file($finfo, $tmpPath) ?: null;
        finfo_close($finfo);
        return $mime;
    }
    // Repli si fileinfo n'est pas dispo (rare)
    return function_exists('mime_content_type') ? (mime_content_type($tmpPath) ?: null) : null;
}

/**
 * Génère un nom de fichier aléatoire et sûr, jamais dérivé du nom client.
 */
function barker_random_filename(string $prefix, string $ext): string
{
    return preg_replace('/[^a-z0-9_]/i', '', $prefix) . '_' . bin2hex(random_bytes(12)) . '.' . $ext;
}

/**
 * Stocke une image uploadée en toute sécurité : vérifie le vrai type,
 * vérifie que c'est une image valide (getimagesize), génère un nom
 * aléatoire, déplace le fichier puis le compresse.
 *
 * @return array{filename:string, mime:string}|null
 */
function barker_store_image(string $tmpName, string $destDir, string $prefix, int $maxBytes, ?string &$error = null): ?array
{
    if (!is_uploaded_file($tmpName)) { $error = 'upload_invalide'; return null; }

    $size = filesize($tmpName);
    if ($size === false || $size > $maxBytes) { $error = 'fichier_trop_lourd'; return null; }

    $mime = barker_detect_mime($tmpName);
    if (!$mime || !isset(BARKER_IMAGE_MIME_MAP[$mime])) { $error = 'type_non_autorise'; return null; }

    // Double vérification : le fichier doit être une image décodable réellement
    $info = @getimagesize($tmpName);
    if ($info === false) { $error = 'image_corrompue'; return null; }

    $ext      = BARKER_IMAGE_MIME_MAP[$mime];
    $filename = barker_random_filename($prefix, $ext);

    if (!is_dir($destDir)) mkdir($destDir, 0755, true);
    $dest = rtrim($destDir, '/') . '/' . $filename;

    if (!move_uploaded_file($tmpName, $dest)) { $error = 'echec_deplacement'; return null; }

    // Compression best-effort : si ça échoue, on garde l'original tel quel
    barker_compress_image($dest, $mime);

    return ['filename' => $filename, 'mime' => $mime];
}

/**
 * Stocke une vidéo uploadée en toute sécurité, puis tente une
 * compression via ffmpeg si disponible sur le serveur.
 *
 * @return array{filename:string, mime:string}|null
 */
function barker_store_video(string $tmpName, string $destDir, string $prefix, int $maxBytes, ?string &$error = null): ?array
{
    if (!is_uploaded_file($tmpName)) { $error = 'upload_invalide'; return null; }

    $size = filesize($tmpName);
    if ($size === false || $size > $maxBytes) { $error = 'fichier_trop_lourd'; return null; }

    $mime = barker_detect_mime($tmpName);
    if (!$mime || !isset(BARKER_VIDEO_MIME_MAP[$mime])) { $error = 'type_non_autorise'; return null; }

    $ext      = BARKER_VIDEO_MIME_MAP[$mime];
    $filename = barker_random_filename($prefix, $ext);

    if (!is_dir($destDir)) mkdir($destDir, 0755, true);
    $dest = rtrim($destDir, '/') . '/' . $filename;

    if (!move_uploaded_file($tmpName, $dest)) { $error = 'echec_deplacement'; return null; }

    // Compression best-effort (ffmpeg). Si absent du serveur, on garde
    // la vidéo d'origine : jamais bloquant pour l'utilisateur.
    $newFilename = barker_compress_video($dest, $filename);

    return ['filename' => $newFilename, 'mime' => $mime];
}

/**
 * Stocke un document (PDF, Word, Excel, etc.) en toute sécurité.
 * Whitelist stricte : tout le reste (scripts, exécutables...) est rejeté.
 *
 * @return array{filename:string, original_name:string, mime:string}|null
 */
function barker_store_document(string $tmpName, string $originalName, string $destDir, int $maxBytes, ?string &$error = null): ?array
{
    if (!is_uploaded_file($tmpName)) { $error = 'upload_invalide'; return null; }

    $size = filesize($tmpName);
    if ($size === false || $size > $maxBytes) { $error = 'fichier_trop_lourd'; return null; }

    $mime = barker_detect_mime($tmpName);
    if (!$mime || !isset(BARKER_DOCUMENT_MIME_MAP[$mime])) { $error = 'type_non_autorise'; return null; }

    $ext      = BARKER_DOCUMENT_MIME_MAP[$mime];
    $filename = barker_random_filename('doc', $ext);

    if (!is_dir($destDir)) mkdir($destDir, 0755, true);
    $dest = rtrim($destDir, '/') . '/' . $filename;

    if (!move_uploaded_file($tmpName, $dest)) { $error = 'echec_deplacement'; return null; }

    return ['filename' => $filename, 'original_name' => $originalName, 'mime' => $mime];
}

/**
 * Compresse une image sur place avec GD : redimensionne si trop grande
 * et réencode avec un niveau de qualité raisonnable. Ne casse jamais
 * le fichier : si GD échoue, l'original est conservé tel quel.
 */
function barker_compress_image(string $path, string $mime, int $maxWidth = 1920, int $maxHeight = 1920, int $quality = 82): void
{
    if (!extension_loaded('gd')) return;

    $info = @getimagesize($path);
    if (!$info) return;
    [$width, $height] = $info;
    if ($width <= 0 || $height <= 0) return;

    try {
        switch ($mime) {
            case 'image/jpeg':
                $src = @imagecreatefromjpeg($path);
                break;
            case 'image/png':
                $src = @imagecreatefrompng($path);
                break;
            case 'image/webp':
                $src = function_exists('imagecreatefromwebp') ? @imagecreatefromwebp($path) : false;
                break;
            case 'image/gif':
                // Les GIF animés perdraient leur animation en repassant par GD :
                // on ne touche pas aux GIF pour ne jamais casser une animation.
                return;
            default:
                return;
        }
        if (!$src) return;

        // Redimensionnement uniquement si l'image dépasse les limites
        $ratio     = min(1, $maxWidth / $width, $maxHeight / $height);
        $newWidth  = max(1, (int) round($width * $ratio));
        $newHeight = max(1, (int) round($height * $ratio));

        if ($ratio < 1) {
            $resized = imagecreatetruecolor($newWidth, $newHeight);
            if ($mime === 'image/png' || $mime === 'image/webp') {
                imagealphablending($resized, false);
                imagesavealpha($resized, true);
            }
            imagecopyresampled($resized, $src, 0, 0, 0, 0, $newWidth, $newHeight, $width, $height);
            imagedestroy($src);
            $src = $resized;
        }

        switch ($mime) {
            case 'image/jpeg':
                imagejpeg($src, $path, $quality);
                break;
            case 'image/png':
                // PNG : échelle de compression 0-9 (pas de "qualité" comme le JPEG)
                imagepng($src, $path, 6);
                break;
            case 'image/webp':
                if (function_exists('imagewebp')) imagewebp($src, $path, $quality);
                break;
        }
        imagedestroy($src);
    } catch (\Throwable $e) {
        error_log('barker_compress_image: ' . $e->getMessage());
        // On garde le fichier original déjà stocké, pas d'échec utilisateur
    }
}

/**
 * Vérifie si ffmpeg est disponible et exécutable sur ce serveur.
 * De nombreux hébergeurs mutualisés désactivent shell_exec : dans ce
 * cas la fonction retourne false et la vidéo d'origine est conservée.
 */
function barker_ffmpeg_available(): bool
{
    static $available = null;
    if ($available !== null) return $available;

    $disabled = array_map('trim', explode(',', (string) ini_get('disable_functions')));
    if (!function_exists('shell_exec') || in_array('shell_exec', $disabled, true)) {
        return $available = false;
    }

    $which = @shell_exec('command -v ffmpeg 2>/dev/null');
    return $available = !empty(trim((string) $which));
}

/**
 * Compresse une vidéo via ffmpeg (H.264 + AAC, dimensions plafonnées).
 * Standardise vers un conteneur .mp4 pour garantir une lecture large
 * (y compris pour des sources .mov/.mkv). Si ffmpeg est indisponible,
 * ou si la compression échoue, on garde la vidéo d'origine intacte et
 * on renvoie le nom de fichier inchangé.
 *
 * @return string Le nom de fichier final (peut différer de l'original si converti en .mp4)
 */
function barker_compress_video(string $path, string $filename, int $maxWidth = 1280, int $crf = 28): string
{
    if (!barker_ffmpeg_available()) return $filename;

    $dir        = dirname($path);
    $baseName   = pathinfo($filename, PATHINFO_FILENAME);
    $targetName = $baseName . '.mp4';
    $targetPath = $dir . '/' . $targetName;
    $tmpOutput  = $dir . '/' . $baseName . '_tmp.mp4';

    $src = escapeshellarg($path);
    $out = escapeshellarg($tmpOutput);

    // - scale : ne réduit que si la vidéo dépasse maxWidth (jamais d'agrandissement)
    // - crf 28 + preset veryfast : bon compromis poids/qualité, rapide sur mutualisé
    // - +faststart : lecture en streaming progressif dans le navigateur
    $cmd = "ffmpeg -y -i $src -vf \"scale='min($maxWidth,iw)':-2\" "
         . "-c:v libx264 -crf $crf -preset veryfast -movflags +faststart "
         . "-c:a aac -b:a 128k $out 2>&1";

    @shell_exec($cmd);

    if (!is_file($tmpOutput) || filesize($tmpOutput) < 1024) {
        @unlink($tmpOutput);
        return $filename; // échec silencieux : on garde l'original
    }

    // On ne remplace que si la compression a réellement réduit le poids
    if (filesize($tmpOutput) < filesize($path)) {
        @unlink($path);
        rename($tmpOutput, $targetPath);
        return $targetName;
    }

    @unlink($tmpOutput);
    return $filename;
}
