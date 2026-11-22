<?php
// ============================================================
// BARKER — Messagerie interne (conversations directes 1-à-1)
// ============================================================
// BARKER possède sa PROPRE messagerie, stockée dans SA base de
// données (tables dm_conversations / dm_messages — voir
// sql/migration_messaging_native.sql). Aucune dépendance envers
// une autre application.
//
// Une conversation directe est unique par paire d'utilisateurs :
// user_a est toujours le plus petit id, user_b le plus grand.
// ============================================================

const DM_MAX_CHARS      = 4000;
const DM_MAX_FILE_BYTES = 20 * 1024 * 1024; // 20 Mo
const DM_MAX_VOICE_SECS = 600;

// Types MIME autorisés pour les messages vocaux (MediaRecorder selon navigateur).
// finfo détecte souvent un .webm audio comme "video/webm" : on l'accepte ici.
const DM_VOICE_MIME_MAP = [
    'audio/webm'      => 'webm',
    'video/webm'      => 'webm',
    'audio/ogg'       => 'ogg',
    'application/ogg' => 'ogg',
    'audio/mp4'       => 'm4a',
    'video/mp4'       => 'm4a',
    'audio/x-m4a'     => 'm4a',
    'audio/mpeg'      => 'mp3',
    'audio/wav'       => 'wav',
    'audio/x-wav'     => 'wav',
];

/** Dossier + URL de stockage des pièces jointes de messages. */
function dm_storage(string $sub): array {
    $dir = rtrim(UPLOAD_DIR, '/') . '/messages/' . $sub . '/';
    if (!is_dir($dir)) @mkdir($dir, 0755, true);
    return [$dir, rtrim(UPLOAD_URL, '/') . '/messages/' . $sub . '/'];
}

/** URL d'avatar d'un utilisateur (colonnes avatar + gender de la table users). */
function dm_avatar_url(?string $avatar, ?string $gender = null): string {
    if (!empty($avatar)) {
        return SITE_URL . '/assets/uploads/avatars/' . rawurlencode($avatar);
    }
    return match ($gender) {
        'male'   => AVATAR_MALE,
        'female' => AVATAR_FEMALE,
        default  => AVATAR_OTHER,
    };
}

/** Une conversation existe-t-elle et $uid y participe-t-il ? Retourne la ligne ou null. */
function dm_get_conversation_for(int $uid, int $convId): ?array {
    return DB::fetchOne(
        "SELECT * FROM dm_conversations WHERE id = ? AND (user_a = ? OR user_b = ?)",
        [$convId, $uid, $uid]
    );
}

function dm_other_user_id(array $conv, int $uid): int {
    return (int)$conv['user_a'] === $uid ? (int)$conv['user_b'] : (int)$conv['user_a'];
}

/**
 * Ouvre (ou crée) la conversation directe entre deux utilisateurs.
 * Retourne l'id de la conversation, ou null si l'autre compte est
 * introuvable / banni / identique.
 */
function dm_open_conversation(int $uid, int $otherId): ?int {
    if ($uid === $otherId || $otherId <= 0) return null;

    $other = DB::fetchOne("SELECT id FROM users WHERE id = ? AND is_banned = 0", [$otherId]);
    if (!$other) return null;

    $a = min($uid, $otherId);
    $b = max($uid, $otherId);

    // INSERT IGNORE + clé unique (user_a, user_b) : sans risque de doublon
    // même si deux requêtes arrivent en même temps.
    DB::execute("INSERT IGNORE INTO dm_conversations (user_a, user_b) VALUES (?, ?)", [$a, $b]);
    $row = DB::fetchOne("SELECT id FROM dm_conversations WHERE user_a = ? AND user_b = ?", [$a, $b]);
    return $row ? (int)$row['id'] : null;
}

/** Liste des conversations de l'utilisateur, la plus récente en premier. */
function dm_list_conversations(int $uid): array {
    $rows = DB::fetchAll(
        "SELECT c.id, c.last_message_at,
                u.id AS contact_id, u.username, u.display_name, u.avatar, u.gender,
                (SELECT COUNT(*) FROM dm_messages m
                  WHERE m.conversation_id = c.id AND m.sender_id <> ?
                    AND m.read_at IS NULL AND m.is_deleted = 0) AS unread,
                (SELECT m2.id FROM dm_messages m2
                  WHERE m2.conversation_id = c.id AND m2.is_deleted = 0
                  ORDER BY m2.id DESC LIMIT 1) AS last_id
         FROM dm_conversations c
         JOIN users u ON u.id = IF(c.user_a = ?, c.user_b, c.user_a)
         WHERE (c.user_a = ? OR c.user_b = ?) AND u.is_banned = 0
         ORDER BY COALESCE(c.last_message_at, c.created_at) DESC
         LIMIT 200",
        [$uid, $uid, $uid, $uid]
    );

    $out = [];
    foreach ($rows as $r) {
        $last = null;
        if (!empty($r['last_id'])) {
            $last = DB::fetchOne("SELECT content, type, sent_at FROM dm_messages WHERE id = ?", [$r['last_id']]);
        }
        $preview = null;
        if ($last) {
            $preview = match ($last['type']) {
                'image' => '📷 Photo',
                'video' => '🎬 Vidéo',
                'voice' => '🎤 Message vocal',
                'file'  => '📎 Fichier',
                default => mb_substr((string)$last['content'], 0, 80),
            };
        }
        $out[] = [
            'id'         => (int)$r['id'],
            'type'       => 'direct',
            'name'       => $r['display_name'] ?: $r['username'],
            'username'   => $r['username'],
            'avatar_url' => dm_avatar_url($r['avatar'], $r['gender']),
            'contact_id' => (int)$r['contact_id'],
            'last_msg'   => $preview,
            'last_type'  => $last['type'] ?? null,
            'last_time'  => $last['sent_at'] ?? null,
            'unread'     => (int)$r['unread'],
        ];
    }
    return $out;
}

/** Infos d'en-tête de la conversation (nom, avatar) — utile pour ouvrir une conv. */
function dm_conversation_meta(int $uid, int $convId): ?array {
    $conv = dm_get_conversation_for($uid, $convId);
    if (!$conv) return null;
    $u = DB::fetchOne(
        "SELECT id, username, display_name, avatar, gender FROM users WHERE id = ?",
        [dm_other_user_id($conv, $uid)]
    );
    if (!$u) return null;
    return [
        'id'         => (int)$conv['id'],
        'contact_id' => (int)$u['id'],
        'name'       => $u['display_name'] ?: $u['username'],
        'username'   => $u['username'],
        'avatar_url' => dm_avatar_url($u['avatar'], $u['gender']),
    ];
}

/** Ajoute les champs calculés attendus par l'interface. */
function dm_decorate_message(array $m, int $uid): array {
    $m['id']              = (int)$m['id'];
    $m['sender_id']       = (int)$m['sender_id'];
    $m['file_url']        = !empty($m['file_path']) ? rtrim(UPLOAD_URL, '/') . '/messages/' . ltrim($m['file_path'], '/') : null;
    $m['sender_avatar_url'] = dm_avatar_url($m['sender_avatar'] ?? null, $m['sender_gender'] ?? null);
    $m['mine']            = ($m['sender_id'] === $uid);
    // Un message supprimé ne doit plus rien divulguer.
    if ((int)$m['is_deleted'] === 1) {
        $m['content'] = null; $m['file_url'] = null; $m['file_name'] = null;
    }
    unset($m['file_path'], $m['sender_gender']);
    return $m;
}

/**
 * Messages d'une conversation (tous, ou ceux postérieurs à $afterId pour
 * le polling). Vérifie que $uid participe à la conversation.
 */
function dm_fetch_messages(int $uid, int $convId, int $afterId = 0): array {
    if (!dm_get_conversation_for($uid, $convId)) throw new RuntimeException('Conversation introuvable');

    $params = [$convId];
    $where  = "m.conversation_id = ?";
    if ($afterId > 0) { $where .= " AND m.id > ?"; $params[] = $afterId; }

    // On renvoie les 300 DERNIERS messages, remis dans l'ordre chronologique.
    $rows = DB::fetchAll(
        "SELECT * FROM (
            SELECT m.id, m.conversation_id, m.sender_id, m.type, m.content, m.file_path, m.file_name,
                   m.file_size, m.duration, m.reply_to, m.sent_at, m.is_deleted, m.is_edited, m.edited_at,
                   u.display_name AS sender_name, u.avatar AS sender_avatar, u.gender AS sender_gender,
                   rm.content AS reply_content, rm.type AS reply_type, rm.is_deleted AS reply_deleted,
                   ru.display_name AS reply_sender
            FROM dm_messages m
            JOIN users u ON u.id = m.sender_id
            LEFT JOIN dm_messages rm ON rm.id = m.reply_to AND rm.conversation_id = m.conversation_id
            LEFT JOIN users ru ON ru.id = rm.sender_id
            WHERE $where
            ORDER BY m.id DESC
            LIMIT 300
         ) t ORDER BY id ASC",
        $params
    );

    foreach ($rows as &$r) {
        if (!empty($r['reply_deleted'])) $r['reply_content'] = null;
        unset($r['reply_deleted']);
        $r = dm_decorate_message($r, $uid);
    }
    return $rows;
}

function dm_fetch_one(int $uid, int $convId, int $msgId): ?array {
    foreach (dm_fetch_messages($uid, $convId, $msgId - 1) as $m) {
        if ($m['id'] === $msgId) return $m;
    }
    return null;
}

/** Vérifie que $uid peut écrire dans la conversation (l'autre compte doit exister et ne pas être banni). */
function dm_assert_can_send(int $uid, int $convId): array {
    $conv = dm_get_conversation_for($uid, $convId);
    if (!$conv) throw new RuntimeException('Conversation introuvable');
    $other = DB::fetchOne("SELECT is_banned FROM users WHERE id = ?", [dm_other_user_id($conv, $uid)]);
    if (!$other || (int)$other['is_banned'] === 1) {
        throw new RuntimeException("Ce compte n'est plus disponible");
    }
    return $conv;
}

/** Limite anti-spam : 30 messages / minute / utilisateur. */
function dm_rate_limit(int $uid): void {
    $row = DB::fetchOne(
        "SELECT COUNT(*) AS c FROM dm_messages WHERE sender_id = ? AND sent_at > DATE_SUB(NOW(), INTERVAL 1 MINUTE)",
        [$uid]
    );
    if ((int)($row['c'] ?? 0) >= 30) {
        throw new RuntimeException('Tu envoies trop de messages, attends un instant.');
    }
}

/** Valide que le message auquel on répond appartient bien à la même conversation. */
function dm_valid_reply_to(int $convId, ?int $replyTo): ?int {
    if (!$replyTo) return null;
    $r = DB::fetchOne("SELECT id FROM dm_messages WHERE id = ? AND conversation_id = ?", [$replyTo, $convId]);
    return $r ? (int)$r['id'] : null;
}

function dm_insert_message(int $uid, array $conv, string $type, ?string $content, ?string $filePath, ?string $fileName, ?int $fileSize, ?int $duration, ?int $replyTo): array {
    dm_rate_limit($uid);
    $convId  = (int)$conv['id'];
    $replyTo = dm_valid_reply_to($convId, $replyTo);

    $id = DB::insert(
        "INSERT INTO dm_messages (conversation_id, sender_id, type, content, file_path, file_name, file_size, duration, reply_to)
         VALUES (?,?,?,?,?,?,?,?,?)",
        [$convId, $uid, $type, $content, $filePath, $fileName, $fileSize, $duration, $replyTo]
    );
    DB::execute("UPDATE dm_conversations SET last_message_at = NOW() WHERE id = ?", [$convId]);

    $msg = dm_fetch_one($uid, $convId, $id);
    return $msg ?: ['id' => $id, 'conversation_id' => $convId, 'sender_id' => $uid, 'type' => $type, 'content' => $content, 'mine' => true];
}

function dm_send_text(int $uid, int $convId, string $content, ?int $replyTo = null): array {
    $content = trim($content);
    if ($content === '') throw new InvalidArgumentException('empty_message');
    if (mb_strlen($content) > DM_MAX_CHARS) throw new RuntimeException('Message trop long (' . DM_MAX_CHARS . ' caractères max).');
    $conv = dm_assert_can_send($uid, $convId);
    return dm_insert_message($uid, $conv, 'text', $content, null, null, null, null, $replyTo);
}

/** Fichier : image, vidéo ou document (whitelist stricte, voir media_security.php). */
function dm_send_file(int $uid, int $convId, array $file, ?int $replyTo = null): array {
    $conv = dm_assert_can_send($uid, $convId);
    $tmp  = $file['tmp_name'];
    $orig = mb_substr(basename((string)$file['name']), 0, 200);
    $mime = barker_detect_mime($tmp);
    if (!$mime) throw new RuntimeException('Type de fichier non reconnu.');

    $err = null; $res = null; $type = 'file'; $sub = 'documents';
    if (isset(BARKER_IMAGE_MIME_MAP[$mime])) {
        [$dir] = dm_storage('media');
        $res = barker_store_image($tmp, $dir, 'dm', DM_MAX_FILE_BYTES, $err);
        $type = 'image'; $sub = 'media';
    } elseif (isset(BARKER_VIDEO_MIME_MAP[$mime])) {
        [$dir] = dm_storage('media');
        $res = barker_store_video($tmp, $dir, 'dm', DM_MAX_FILE_BYTES, $err);
        $type = 'video'; $sub = 'media';
        if ($res) {
            // Compression best-effort (ffmpeg si disponible) ; le nom final peut changer.
            $res['filename'] = barker_compress_video($dir . $res['filename'], $res['filename']);
        }
    } else {
        [$dir] = dm_storage('documents');
        $res = barker_store_document($tmp, $orig, $dir, DM_MAX_FILE_BYTES, $err);
    }

    if (!$res) {
        $msg = match ($err) {
            'fichier_trop_lourd' => 'Fichier trop lourd (20 Mo max).',
            'type_non_autorise'  => "Ce type de fichier n'est pas autorisé.",
            'image_corrompue'    => 'Image illisible ou corrompue.',
            default              => "Échec de l'envoi du fichier.",
        };
        throw new RuntimeException($msg);
    }

    $path = $sub . '/' . $res['filename'];
    $size = @filesize(rtrim($dir, '/') . '/' . $res['filename']) ?: null;
    return dm_insert_message($uid, $conv, $type, null, $path, $orig, $size, null, $replyTo);
}

/** Message vocal enregistré dans le navigateur (MediaRecorder). */
function dm_send_voice(int $uid, int $convId, array $file, int $duration, ?int $replyTo = null): array {
    $conv = dm_assert_can_send($uid, $convId);
    $tmp  = $file['tmp_name'];

    if (!is_uploaded_file($tmp)) throw new RuntimeException('Upload invalide.');
    $size = filesize($tmp);
    if ($size === false || $size < 200)       throw new RuntimeException('Enregistrement vide.');
    if ($size > DM_MAX_FILE_BYTES)            throw new RuntimeException('Enregistrement trop lourd.');

    $mime = barker_detect_mime($tmp);
    if (!$mime || !isset(DM_VOICE_MIME_MAP[$mime])) throw new RuntimeException("Format audio non pris en charge.");

    [$dir] = dm_storage('voice');
    $name  = barker_random_filename('vc', DM_VOICE_MIME_MAP[$mime]);
    if (!move_uploaded_file($tmp, $dir . $name)) throw new RuntimeException("Échec de l'enregistrement du vocal.");

    $duration = max(0, min(DM_MAX_VOICE_SECS, $duration));
    return dm_insert_message($uid, $conv, 'voice', null, 'voice/' . $name, $name, (int)$size, $duration, $replyTo);
}

/** Modifier un message texte dont on est l'auteur. */
function dm_edit_message(int $uid, int $msgId, string $content): array {
    $content = trim($content);
    if ($content === '') throw new InvalidArgumentException('empty_message');
    if (mb_strlen($content) > DM_MAX_CHARS) throw new RuntimeException('Message trop long.');

    $m = DB::fetchOne("SELECT id, conversation_id, type FROM dm_messages WHERE id = ? AND sender_id = ? AND is_deleted = 0", [$msgId, $uid]);
    if (!$m) throw new RuntimeException('Message introuvable.');
    if ($m['type'] !== 'text') throw new RuntimeException('Seuls les messages texte peuvent être modifiés.');

    DB::execute("UPDATE dm_messages SET content = ?, is_edited = 1, edited_at = NOW() WHERE id = ?", [$content, $msgId]);
    return dm_fetch_one($uid, (int)$m['conversation_id'], $msgId) ?? ['id' => $msgId, 'content' => $content, 'is_edited' => 1];
}

/** Supprimer (soft-delete) un message dont on est l'auteur + effacer le fichier joint. */
function dm_delete_message(int $uid, int $msgId): bool {
    $m = DB::fetchOne("SELECT id, file_path FROM dm_messages WHERE id = ? AND sender_id = ? AND is_deleted = 0", [$msgId, $uid]);
    if (!$m) throw new RuntimeException('Message introuvable.');

    DB::execute("UPDATE dm_messages SET is_deleted = 1, content = NULL, file_path = NULL, file_name = NULL WHERE id = ?", [$msgId]);

    if (!empty($m['file_path'])) {
        $full = rtrim(UPLOAD_DIR, '/') . '/messages/' . ltrim($m['file_path'], '/');
        $real = realpath($full);
        $base = realpath(rtrim(UPLOAD_DIR, '/') . '/messages');
        // Garde-fou : on ne supprime que ce qui est réellement dans /messages
        if ($real && $base && str_starts_with($real, $base . DIRECTORY_SEPARATOR)) @unlink($real);
    }
    return true;
}

/** Marque comme lus les messages reçus dans la conversation. */
function dm_mark_read(int $uid, int $convId): void {
    if (!dm_get_conversation_for($uid, $convId)) return;
    DB::execute(
        "UPDATE dm_messages SET read_at = NOW() WHERE conversation_id = ? AND sender_id <> ? AND read_at IS NULL",
        [$convId, $uid]
    );
}

/** Nombre total de messages non lus (pastille du menu). */
function dm_unread_count(int $uid): int {
    $row = DB::fetchOne(
        "SELECT COUNT(*) AS c
         FROM dm_messages m
         JOIN dm_conversations c ON c.id = m.conversation_id
         WHERE (c.user_a = ? OR c.user_b = ?) AND m.sender_id <> ? AND m.read_at IS NULL AND m.is_deleted = 0",
        [$uid, $uid, $uid]
    );
    return (int)($row['c'] ?? 0);
}
