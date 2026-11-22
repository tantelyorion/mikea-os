<?php
// ============================================================
// BARKER API — Messagerie interne
// ============================================================
// Toute la logique est dans includes/messaging.php (base BARKER).
// ============================================================

require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$uid    = (int)$_SESSION['user_id'];
$action = $_GET['action'] ?? ($_POST['action'] ?? '');

$WRITE_ACTIONS = ['send', 'send_file', 'send_voice', 'edit', 'delete', 'read', 'open'];
if (in_array($action, $WRITE_ACTIONS, true)) {
    if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
        json_response(['error' => 'POST requis'], 405);
    }
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        json_response(['error' => 'Jeton de sécurité invalide, recharge la page.'], 403);
    }
}

try {
    switch ($action) {

        case 'list':
            json_response(['success' => true, 'convs' => dm_list_conversations($uid)]);

        case 'messages':
            $convId = (int)($_GET['conv'] ?? 0);
            if (!$convId) json_response(['error' => 'conv requis'], 400);
            $msgs = dm_fetch_messages($uid, $convId);
            dm_mark_read($uid, $convId);
            json_response(['success' => true, 'messages' => $msgs, 'meta' => dm_conversation_meta($uid, $convId)]);

        case 'poll':
            $convId  = (int)($_GET['conv'] ?? 0);
            $afterId = (int)($_GET['after_id'] ?? 0);
            if (!$convId) json_response(['error' => 'conv requis'], 400);
            $msgs = dm_fetch_messages($uid, $convId, $afterId);
            if ($msgs) dm_mark_read($uid, $convId);
            json_response(['success' => true, 'messages' => $msgs]);

        case 'send':
            $convId  = (int)($_POST['conv'] ?? 0);
            $content = (string)($_POST['content'] ?? '');
            $replyTo = (int)($_POST['reply_to'] ?? 0) ?: null;
            if (!$convId) json_response(['error' => 'conv requis'], 400);
            if (trim($content) === '') json_response(['error' => 'Message vide'], 400);
            json_response(['success' => true, 'message' => dm_send_text($uid, $convId, $content, $replyTo)]);

        case 'send_file':
            $convId  = (int)($_POST['conv'] ?? 0);
            $replyTo = (int)($_POST['reply_to'] ?? 0) ?: null;
            if (!$convId) json_response(['error' => 'conv requis'], 400);
            if (empty($_FILES['file']) || $_FILES['file']['error'] !== UPLOAD_ERR_OK) {
                json_response(['error' => 'Aucun fichier reçu (ou fichier trop lourd)'], 400);
            }
            json_response(['success' => true, 'message' => dm_send_file($uid, $convId, $_FILES['file'], $replyTo)]);

        case 'send_voice':
            $convId   = (int)($_POST['conv'] ?? 0);
            $duration = (int)($_POST['duration'] ?? 0);
            $replyTo  = (int)($_POST['reply_to'] ?? 0) ?: null;
            if (!$convId) json_response(['error' => 'conv requis'], 400);
            if (empty($_FILES['voice']) || $_FILES['voice']['error'] !== UPLOAD_ERR_OK) {
                json_response(['error' => 'Aucun enregistrement reçu'], 400);
            }
            json_response(['success' => true, 'message' => dm_send_voice($uid, $convId, $_FILES['voice'], $duration, $replyTo)]);

        case 'edit':
            $msgId   = (int)($_POST['msg_id'] ?? 0);
            $content = (string)($_POST['content'] ?? '');
            if (!$msgId) json_response(['error' => 'msg_id requis'], 400);
            if (trim($content) === '') json_response(['error' => 'Message vide'], 400);
            json_response(['success' => true, 'message' => dm_edit_message($uid, $msgId, $content)]);

        case 'delete':
            $msgId = (int)($_POST['msg_id'] ?? 0);
            if (!$msgId) json_response(['error' => 'msg_id requis'], 400);
            dm_delete_message($uid, $msgId);
            json_response(['success' => true]);

        case 'read':
            $convId = (int)($_POST['conv'] ?? 0);
            if (!$convId) json_response(['error' => 'conv requis'], 400);
            dm_mark_read($uid, $convId);
            json_response(['success' => true]);

        // Ouvre (ou crée) la conversation directe avec un utilisateur BARKER.
        case 'open':
            $toUserId = (int)($_POST['to'] ?? 0);
            if (!$toUserId) json_response(['error' => 'to requis'], 400);
            if ($toUserId === $uid) json_response(['error' => 'Impossible de te contacter toi-même'], 400);
            $convId = dm_open_conversation($uid, $toUserId);
            if (!$convId) json_response(['error' => 'Utilisateur introuvable'], 404);
            $meta = dm_conversation_meta($uid, $convId);
            json_response(['success' => true, 'conv_id' => $convId, 'name' => $meta['name'] ?? '', 'username' => $meta['username'] ?? '']);

        case 'unread_count':
            json_response(['success' => true, 'count' => dm_unread_count($uid)]);

        default:
            json_response(['error' => 'Action invalide'], 400);
    }
} catch (RuntimeException $e) {
    json_response(['error' => $e->getMessage()], 400);
} catch (InvalidArgumentException $e) {
    json_response(['error' => 'Requête invalide'], 400);
} catch (Throwable $e) {
    error_log('api/messages.php error: ' . $e->getMessage());
    json_response(['error' => 'La messagerie est momentanément indisponible'], 503);
}
