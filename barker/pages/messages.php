<?php
// ============================================================
// BARKER - pages/messages.php
// ============================================================
// Messagerie interne de BARKER : texte, fichiers, messages vocaux,
// réponses, édition/suppression. API : api/messages.php (base BARKER).
// ============================================================

require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$currentUser  = current_user();
$myId         = (int)$currentUser['id'];
$openToUserId  = isset($_GET['to']) ? (int)$_GET['to'] : 0;

$pageTitle = __('nav_messages') ?? 'Messages';
include __DIR__ . '/../includes/header.php';
?>

<main class="main-feed messages-page">
    <div class="msg-shell" id="msgShell">
        <section class="msg-list-pane" id="msgListPane">
            <div class="msg-list-header">
                <h1><i class="fa-solid fa-comment-dots"></i> <?= htmlspecialchars($pageTitle) ?></h1>
            </div>
            <div class="msg-list-scroll" id="msgConvList">
                <div class="msg-empty-state"><i class="fa-solid fa-spinner fa-spin"></i></div>
            </div>
        </section>

        <section class="msg-chat-pane" id="msgChatPane">
            <div class="msg-chat-placeholder" id="msgChatPlaceholder">
                <i class="fa-solid fa-comments"></i>
                <p>Choisis une conversation pour commencer à discuter.</p>
            </div>

            <div class="msg-chat-active" id="msgChatActive" style="display:none;">
                <div class="msg-chat-header">
                    <button class="msg-back-btn" id="msgBackBtn" title="Retour"><i class="fa-solid fa-arrow-left"></i></button>
                    <img class="msg-chat-avatar" id="msgChatAvatar" src="" alt="">
                    <div class="msg-chat-title">
                        <span class="msg-chat-name" id="msgChatName"></span>
                        <span class="msg-chat-status" id="msgChatStatus"></span>
                    </div>
                </div>

                <div class="msg-thread" id="msgThread"></div>

                <div class="msg-reply-preview" id="msgReplyPreview" style="display:none;">
                    <div class="msg-reply-preview-body">
                        <span class="msg-reply-preview-label">Réponse à</span>
                        <span class="msg-reply-preview-text" id="msgReplyPreviewText"></span>
                    </div>
                    <button type="button" id="msgReplyCancel"><i class="fa-solid fa-xmark"></i></button>
                </div>

                <form class="msg-composer" id="msgComposer" autocomplete="off">
                    <input type="file" id="msgFileInput" hidden>
                    <button type="button" class="msg-icon-btn" id="msgAttachBtn" title="Joindre un fichier"><i class="fa-solid fa-paperclip"></i></button>
                    <input type="text" id="msgInput" placeholder="Écris un message…" maxlength="4000">
                    <button type="button" class="msg-icon-btn msg-mic-btn" id="msgMicBtn" title="Message vocal"><i class="fa-solid fa-microphone"></i></button>
                    <button type="submit" class="msg-send-btn" id="msgSendBtn" title="Envoyer"><i class="fa-solid fa-paper-plane"></i></button>
                </form>
                <div class="msg-recording-bar" id="msgRecordingBar" style="display:none;">
                    <span class="msg-recording-dot"></span>
                    <span id="msgRecordingTime">0:00</span>
                    <span class="msg-recording-hint">Enregistrement…</span>
                    <button type="button" id="msgRecordingCancel"><i class="fa-solid fa-trash"></i></button>
                    <button type="button" id="msgRecordingSend"><i class="fa-solid fa-paper-plane"></i></button>
                </div>
            </div>
        </section>
    </div>

    <!-- ── Menu contextuel message (édition / suppression / réponse) ── -->
    <div class="msg-context-menu" id="msgContextMenu" style="display:none;">
        <button data-act="reply"><i class="fa-solid fa-reply"></i> Répondre</button>
        <button data-act="edit" id="msgCtxEdit"><i class="fa-solid fa-pen"></i> Modifier</button>
        <button data-act="delete" id="msgCtxDelete"><i class="fa-solid fa-trash"></i> Supprimer</button>
    </div>
</main>

<style>
.messages-page { padding: 0 !important; max-width: none !important; }
.msg-shell {
    display: flex;
    height: calc(100vh - 0px);
    min-height: 520px;
    max-width: 100%;
    min-width: 0;
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 12px;
    overflow: hidden;
    margin: 12px;
    position: relative;
}
@media (max-width: 900px) { .msg-shell { margin: 0; border-radius: 0; border: none; height: calc(100vh - 56px); } }

.msg-list-pane { width: 340px; min-width: 260px; max-width: 40%; display: flex; flex-direction: column; border-right: 1px solid var(--border); background: var(--bg-card); }
.msg-list-header { padding: 16px 18px; border-bottom: 1px solid var(--border); }
.msg-list-header h1 { font-size: 1.15rem; margin: 0; color: var(--text-primary); }
.msg-list-header h1 i { color: var(--orange); margin-right: 8px; }
.msg-list-scroll { flex: 1; overflow-y: auto; }

.msg-conv-item { display: flex; align-items: center; gap: 12px; padding: 12px 16px; cursor: pointer; border-bottom: 1px solid var(--border); transition: background .15s ease; }
.msg-conv-item:hover, .msg-conv-item.active { background: var(--bg-card2); }
.msg-conv-avatar { width: 48px; height: 48px; border-radius: 50%; object-fit: cover; flex-shrink: 0; background: var(--bg-input); }
.msg-conv-body { flex: 1; min-width: 0; }
.msg-conv-top { display: flex; justify-content: space-between; align-items: baseline; gap: 8px; }
.msg-conv-name { font-weight: 600; color: var(--text-primary); font-size: .95rem; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.msg-conv-time { font-size: .72rem; color: var(--text-muted); flex-shrink: 0; }
.msg-conv-preview { display: flex; justify-content: space-between; align-items: center; gap: 8px; margin-top: 2px; }
.msg-conv-preview span { font-size: .82rem; color: var(--text-secondary); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.msg-conv-badge { background: var(--orange); color: #fff; font-size: .68rem; font-weight: 700; min-width: 18px; height: 18px; border-radius: 999px; display: flex; align-items: center; justify-content: center; padding: 0 5px; flex-shrink: 0; }
.msg-empty-state { display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100%; color: var(--text-muted); gap: 10px; padding: 30px; text-align: center; font-size: .9rem; }
.msg-empty-state i { font-size: 26px; color: var(--orange); }

.msg-chat-pane { flex: 1; display: flex; flex-direction: column; min-width: 0; }
.msg-chat-placeholder { flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center; color: var(--text-muted); gap: 12px; }
.msg-chat-placeholder i { font-size: 40px; color: var(--orange); opacity: .6; }
.msg-chat-active { flex: 1; display: flex; flex-direction: column; min-height: 0; min-width: 0; }

.msg-chat-header { display: flex; align-items: center; gap: 12px; padding: 12px 16px; border-bottom: 1px solid var(--border); background: var(--bg-card); }
.msg-back-btn { display: none; background: none; border: none; color: var(--text-primary); font-size: 18px; cursor: pointer; padding: 4px; }
.msg-chat-avatar { width: 40px; height: 40px; border-radius: 50%; object-fit: cover; background: var(--bg-input); }
.msg-chat-title { display: flex; flex-direction: column; min-width: 0; }
.msg-chat-name { font-weight: 700; color: var(--text-primary); font-size: .95rem; }
.msg-chat-status { font-size: .75rem; color: var(--text-muted); }
.msg-chat-status.online { color: #2ecc71; }

.msg-thread { flex: 1; overflow-y: auto; padding: 16px; display: flex; flex-direction: column; gap: 8px; background: var(--bg); }
.msg-bubble-row { display: flex; position: relative; }
.msg-bubble-row.mine { justify-content: flex-end; }
.msg-bubble {
    max-width: 72%; padding: 9px 13px; border-radius: 16px; font-size: .9rem; line-height: 1.35;
    background: var(--bg-card2); color: var(--text-primary); word-wrap: break-word; box-shadow: var(--shadow-sm); cursor: default;
}
.msg-bubble-row.mine .msg-bubble { background: var(--orange); color: #fff; border-bottom-right-radius: 4px; }
.msg-bubble-row:not(.mine) .msg-bubble { border-bottom-left-radius: 4px; }
.msg-bubble-time { display: block; font-size: .65rem; opacity: .7; margin-top: 4px; text-align: right; }
.msg-bubble-edited { font-style: italic; opacity: .65; font-size: .68rem; margin-left: 4px; }
.msg-bubble.deleted { font-style: italic; opacity: .6; }
.msg-bubble img.msg-media { max-width: 220px; border-radius: 10px; display: block; margin-bottom: 4px; cursor: pointer; }
.msg-bubble video.msg-media { max-width: 240px; border-radius: 10px; display: block; margin-bottom: 4px; }
.msg-bubble audio.msg-audio { width: 220px; }
.msg-file-chip { display: flex; align-items: center; gap: 8px; }
.msg-file-chip i { font-size: 20px; }
.msg-file-chip a { color: inherit; text-decoration: underline; }
.msg-reply-quote { border-left: 3px solid var(--orange); padding: 4px 8px; margin-bottom: 6px; font-size: .78rem; opacity: .8; border-radius: 4px; background: rgba(0,0,0,.12); }
.msg-bubble-row.mine .msg-reply-quote { background: rgba(255,255,255,.15); }

.msg-reply-preview { display: flex; align-items: center; justify-content: space-between; padding: 8px 16px; background: var(--bg-card2); border-top: 1px solid var(--border); }
.msg-reply-preview-label { color: var(--orange); font-weight: 600; font-size: .78rem; margin-right: 6px; }
.msg-reply-preview-text { font-size: .82rem; color: var(--text-secondary); }
.msg-reply-preview button { background: none; border: none; color: var(--text-muted); cursor: pointer; font-size: 16px; }

.msg-composer { display: flex; align-items: center; gap: 8px; padding: 12px 16px; border-top: 1px solid var(--border); background: var(--bg-card); min-width: 0; }
.msg-composer input[type=text] { flex: 1; min-width: 0; background: var(--bg-input); border: 1px solid var(--border); border-radius: 999px; padding: 10px 16px; color: var(--text-primary); font-size: .9rem; outline: none; }
.msg-composer input[type=text]:focus { border-color: var(--orange); }
.msg-icon-btn { width: 38px; height: 38px; border-radius: 50%; border: none; background: var(--bg-input); color: var(--text-secondary); cursor: pointer; flex-shrink: 0; display: flex; align-items: center; justify-content: center; }
.msg-icon-btn:hover { color: var(--orange); }
.msg-mic-btn.recording { color: #fff; background: #e0245e; }
.msg-send-btn { width: 40px; height: 40px; border-radius: 50%; border: none; background: var(--orange); color: #fff; cursor: pointer; flex-shrink: 0; display: flex; align-items: center; justify-content: center; }
.msg-send-btn:hover { background: var(--orange-hover); }

.msg-recording-bar { display: flex; align-items: center; gap: 10px; padding: 10px 16px; background: var(--bg-card); border-top: 1px solid var(--border); font-size: .85rem; color: var(--text-secondary); min-width: 0; }
.msg-recording-dot { width: 10px; height: 10px; border-radius: 50%; background: #e0245e; animation: msgPulse 1s infinite; }
@keyframes msgPulse { 0%,100% { opacity: 1; } 50% { opacity: .3; } }
.msg-recording-hint { flex: 1; }
.msg-recording-bar button { background: none; border: none; color: var(--text-secondary); font-size: 16px; cursor: pointer; padding: 6px; }
#msgRecordingSend { color: var(--orange); }

.msg-modal-overlay { position: fixed; inset: 0; background: rgba(0,0,0,.6); display: flex; align-items: center; justify-content: center; z-index: 500; padding: 16px; }
.msg-modal { background: var(--bg-card); border: 1px solid var(--border); border-radius: 14px; padding: 24px; max-width: 380px; width: 100%; }
.msg-modal h3 { margin: 0 0 10px; color: var(--text-primary); font-size: 1.05rem; }
.msg-modal h3 i { color: var(--orange); margin-right: 6px; }
.msg-modal p { color: var(--text-secondary); font-size: .85rem; margin: 0 0 16px; line-height: 1.4; }
.msg-modal-error { color: #f4212e; font-size: .8rem; min-height: 18px; margin-top: 8px; }
.msg-modal-submit { width: 100%; margin-top: 10px; padding: 11px; border: none; border-radius: 999px; background: var(--orange); color: #fff; font-weight: 600; cursor: pointer; }
.msg-modal-submit:hover { background: var(--orange-hover); }

.msg-context-menu { position: fixed; z-index: 600; background: var(--bg-card2); border: 1px solid var(--border); border-radius: 10px; overflow: hidden; box-shadow: var(--shadow); min-width: 160px; }
.msg-context-menu button { display: flex; align-items: center; gap: 8px; width: 100%; padding: 10px 14px; background: none; border: none; color: var(--text-primary); font-size: .85rem; cursor: pointer; text-align: left; }
.msg-context-menu button:hover { background: var(--bg-input); }
.msg-context-menu button[data-act="delete"] { color: #f4212e; }

/* ---- Sélecteur pays (repris du même style que register.php) ---- */

@media (max-width: 900px) {
    .msg-list-pane { width: 100%; max-width: none; border-right: none; }
    .msg-chat-pane { display: none; }
    .msg-back-btn { display: inline-flex; }
    .msg-shell.chat-open .msg-list-pane { display: none; }
    .msg-shell.chat-open .msg-chat-pane { display: flex; }
}
</style>

<script>
(function () {
    const API = '<?= SITE_URL ?>/api/messages.php';
    const OPEN_TO = <?= (int)$openToUserId ?>;

    const shell = document.getElementById('msgShell');
    const listEl = document.getElementById('msgConvList');
    const placeholder = document.getElementById('msgChatPlaceholder');
    const activeBox = document.getElementById('msgChatActive');
    const thread = document.getElementById('msgThread');
    const composer = document.getElementById('msgComposer');
    const input = document.getElementById('msgInput');
    const backBtn = document.getElementById('msgBackBtn');
    const attachBtn = document.getElementById('msgAttachBtn');
    const fileInput = document.getElementById('msgFileInput');
    const micBtn = document.getElementById('msgMicBtn');
    const recordingBar = document.getElementById('msgRecordingBar');
    const recordingTime = document.getElementById('msgRecordingTime');
    const replyPreview = document.getElementById('msgReplyPreview');
    const replyPreviewText = document.getElementById('msgReplyPreviewText');
    const ctxMenu = document.getElementById('msgContextMenu');

    let currentConvId = null;
    let lastMsgId = 0;
    let pollTimer = null;
    let replyToId = null;
    let ctxMessageId = null;
    let convsCache = [];

    // ---------- Utilitaires ----------
    function esc(s) { const d = document.createElement('div'); d.textContent = s || ''; return d.innerHTML; }
    function timeShort(iso) {
        if (!iso) return '';
        const d = new Date(iso.replace(' ', 'T'));
        const now = new Date();
        const sameDay = d.toDateString() === now.toDateString();
        return sameDay ? d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : d.toLocaleDateString([], { day: '2-digit', month: '2-digit' });
    }

    async function api(action, params, method) {
        method = method || 'GET';
        let url = API + '?action=' + encodeURIComponent(action);
        let opts = { method, headers: {} };
        if (method === 'GET') {
            for (const k in (params || {})) url += '&' + k + '=' + encodeURIComponent(params[k]);
            const res = await fetch(url);
            return res.json();
        }
        const body = new URLSearchParams(params || {});
        body.append('csrf_token', getCsrfToken());
        const res = await fetch(url, { method: 'POST', body });
        return res.json();
    }

    async function apiUpload(action, params, fileField, file) {
        const fd = new FormData();
        fd.append('action', action);
        fd.append('csrf_token', getCsrfToken());
        for (const k in (params || {})) fd.append(k, params[k]);
        fd.append(fileField, file);
        const res = await fetch(API + '?action=' + encodeURIComponent(action), { method: 'POST', body: fd });
        return res.json();
    }

    // ---------- Liste des conversations ----------
    function renderConvList(convs) {
        convsCache = convs;
        if (!convs.length) {
            listEl.innerHTML = '<div class="msg-empty-state"><i class="fa-regular fa-comment"></i><span>Aucune conversation pour l’instant.<br>Envoie un message depuis un profil BARKER pour démarrer.</span></div>';
            return;
        }
        listEl.innerHTML = convs.map(c => `
            <div class="msg-conv-item${c.id === currentConvId ? ' active' : ''}" data-conv="${c.id}">
                <img class="msg-conv-avatar" src="${esc(c.avatar_url)}" alt="">
                <div class="msg-conv-body">
                    <div class="msg-conv-top">
                        <span class="msg-conv-name">${esc(c.name || 'Utilisateur')}</span>
                        <span class="msg-conv-time">${timeShort(c.last_time)}</span>
                    </div>
                    <div class="msg-conv-preview">
                        <span>${esc(c.last_msg || 'Démarrez la conversation')}</span>
                        ${c.unread ? `<span class="msg-conv-badge">${c.unread > 9 ? '9+' : c.unread}</span>` : ''}
                    </div>
                </div>
            </div>
        `).join('');
        listEl.querySelectorAll('.msg-conv-item').forEach(el => {
            el.addEventListener('click', () => openConversation(parseInt(el.dataset.conv, 10)));
        });
    }

    async function refreshList() {
        const r = await api('list', { tab: 'chats' });
        if (r.success) renderConvList(r.convs);
    }

    // ---------- Rendu des messages ----------
    function bubbleInner(m) {
        let replyHtml = '';
        if (m.reply_to && m.reply_content !== undefined && m.reply_content !== null) {
            replyHtml = `<div class="msg-reply-quote"><strong>${esc(m.reply_sender || '')}</strong> — ${esc((m.reply_content || '').slice(0, 80))}</div>`;
        }
        if (m.is_deleted == 1) {
            return replyHtml + `<div class="msg-bubble deleted"><i class="fa-solid fa-ban"></i> Message supprimé</div>`;
        }
        let body = '';
        switch (m.type) {
            case 'image':
                body = `<img class="msg-media" src="${esc(m.file_url)}" onclick="window.open('${esc(m.file_url)}','_blank')">`;
                break;
            case 'video':
                body = `<video class="msg-media" src="${esc(m.file_url)}" controls></video>`;
                break;
            case 'voice':
                body = `<audio class="msg-audio" src="${esc(m.file_url)}" controls></audio>`;
                break;
            case 'file':
                body = `<div class="msg-file-chip"><i class="fa-solid fa-file"></i><a href="${esc(m.file_url)}" target="_blank" rel="noopener">${esc(m.file_name || 'Fichier')}</a></div>`;
                break;
            default:
                body = esc(m.content);
        }
        const edited = m.is_edited == 1 ? '<span class="msg-bubble-edited">(modifié)</span>' : '';
        return replyHtml + body + `<span class="msg-bubble-time">${timeShort(m.sent_at)}${edited}</span>`;
    }

    function appendMessage(m) {
        // Anti-doublon : l'envoi direct (réponse immédiate de 'send') et le
        // polling en arrière-plan peuvent tous les deux ramener le MÊME
        // message si le timing tombe mal (le poll était déjà parti avant
        // que lastMsgId soit mis à jour). On ignore silencieusement si ce
        // message est déjà affiché.
        if (thread.querySelector(`.msg-bubble-row[data-msg-id="${m.id}"]`)) {
            lastMsgId = Math.max(lastMsgId, parseInt(m.id, 10));
            return;
        }
        const row = document.createElement('div');
        row.className = 'msg-bubble-row' + (m.mine ? ' mine' : '');
        row.dataset.msgId = m.id;
        row.dataset.mine = m.mine ? '1' : '0';
        row.dataset.type = m.type;
        row.dataset.content = m.content || '';
        const bubble = document.createElement('div');
        bubble.className = 'msg-bubble';
        bubble.innerHTML = bubbleInner(m);
        row.appendChild(bubble);
        if (m.mine) {
            bubble.addEventListener('contextmenu', (e) => { e.preventDefault(); openContextMenu(e, m.id, m.type); });
            let pressTimer;
            bubble.addEventListener('touchstart', () => { pressTimer = setTimeout(() => openContextMenu(null, m.id, m.type), 500); });
            bubble.addEventListener('touchend', () => clearTimeout(pressTimer));
        } else {
            bubble.addEventListener('dblclick', () => startReply(m.id, m.content, m.type));
        }
        thread.appendChild(row);
        lastMsgId = Math.max(lastMsgId, parseInt(m.id, 10));
    }

    function renderMessages(msgs, append) {
        if (!append) thread.innerHTML = '';
        msgs.forEach(appendMessage);
        thread.scrollTop = thread.scrollHeight;
    }

    function updateMessageInPlace(m) {
        const row = thread.querySelector(`.msg-bubble-row[data-msg-id="${m.id}"]`);
        if (!row) return;
        row.querySelector('.msg-bubble').innerHTML = bubbleInner(m);
    }

    // ---------- Ouverture d'une conversation ----------
    async function openConversation(convId) {
        currentConvId = convId;
        lastMsgId = 0;
        replyToId = null;
        replyPreview.style.display = 'none';
        placeholder.style.display = 'none';
        activeBox.style.display = 'flex';
        shell.classList.add('chat-open');

        const meta = convsCache.find(c => c.id === convId);
        if (meta) {
            document.getElementById('msgChatAvatar').src = meta.avatar_url;
            document.getElementById('msgChatName').textContent = meta.name || 'Utilisateur';
        }
        listEl.querySelectorAll('.msg-conv-item').forEach(el => el.classList.toggle('active', parseInt(el.dataset.conv, 10) === convId));

        const r = await api('messages', { conv: convId });
        if (r.success) {
            if (!meta && r.meta) {
                document.getElementById('msgChatAvatar').src = r.meta.avatar_url;
                document.getElementById('msgChatName').textContent = r.meta.name || 'Utilisateur';
            }
            renderMessages(r.messages, false);
        } else if (r.error) {
            alert(r.error);
        }

        clearInterval(pollTimer);
        pollTimer = setInterval(pollNewMessages, 3000);
        input.focus();
    }

    async function pollNewMessages() {
        if (!currentConvId) return;
        const r = await api('poll', { conv: currentConvId, after_id: lastMsgId });
        if (r.success && r.messages.length) renderMessages(r.messages, true);
    }

    // ---------- Réponse à un message ----------
    function startReply(msgId, content, type) {
        replyToId = msgId;
        replyPreview.style.display = 'flex';
        const label = { image: '📷 Photo', video: '🎥 Vidéo', voice: '🎤 Message vocal', file: '📎 Fichier' }[type] || (content || '').slice(0, 60);
        replyPreviewText.textContent = label;
        input.focus();
    }
    document.getElementById('msgReplyCancel').addEventListener('click', () => {
        replyToId = null;
        replyPreview.style.display = 'none';
    });

    // ---------- Menu contextuel (modifier / supprimer / répondre) ----------
    function openContextMenu(e, msgId, type) {
        ctxMessageId = msgId;
        document.getElementById('msgCtxEdit').style.display = (type === 'text') ? 'flex' : 'none';
        ctxMenu.style.display = 'block';
        const x = e ? e.clientX : window.innerWidth / 2 - 80;
        const y = e ? e.clientY : window.innerHeight / 2 - 60;
        ctxMenu.style.left = Math.min(x, window.innerWidth - 180) + 'px';
        ctxMenu.style.top = Math.min(y, window.innerHeight - 140) + 'px';
    }
    document.addEventListener('click', (e) => { if (!ctxMenu.contains(e.target)) ctxMenu.style.display = 'none'; });

    ctxMenu.querySelectorAll('button').forEach(btn => {
        btn.addEventListener('click', async () => {
            const act = btn.dataset.act;
            const row = thread.querySelector(`.msg-bubble-row[data-msg-id="${ctxMessageId}"]`);
            ctxMenu.style.display = 'none';
            if (!row) return;

            if (act === 'reply') {
                startReply(ctxMessageId, row.dataset.content, row.dataset.type);
            } else if (act === 'delete') {
                if (!confirm('Supprimer ce message ?')) return;
                const r = await api('delete', { msg_id: ctxMessageId }, 'POST');
                if (r.success) updateMessageInPlace({ id: ctxMessageId, is_deleted: 1 });
            } else if (act === 'edit') {
                const current = row.dataset.content;
                const next = prompt('Modifier le message :', current);
                if (next === null || next.trim() === '' || next === current) return;
                const r = await api('edit', { msg_id: ctxMessageId, content: next }, 'POST');
                if (r.success) { row.dataset.content = next; updateMessageInPlace(r.message); }
                else if (r.error) alert(r.error);
            }
        });
    });

    // ---------- Envoi texte ----------
    let sendingMessage = false;
    composer.addEventListener('submit', async (e) => {
        e.preventDefault();
        if (sendingMessage) return; // évite un double envoi (double-clic/double-Enter)
        const content = input.value.trim();
        if (!content || !currentConvId) return;
        sendingMessage = true;
        input.value = '';
        const params = { conv: currentConvId, content };
        if (replyToId) params.reply_to = replyToId;
        try {
            const r = await api('send', params, 'POST');
            if (r.success) { appendMessage(r.message); thread.scrollTop = thread.scrollHeight; refreshList(); }
            else if (r.error) alert(r.error);
        } finally {
            sendingMessage = false;
        }
        replyToId = null;
        replyPreview.style.display = 'none';
    });

    // ---------- Pièce jointe ----------
    attachBtn.addEventListener('click', () => fileInput.click());
    fileInput.addEventListener('change', async () => {
        const file = fileInput.files[0];
        fileInput.value = '';
        if (!file || !currentConvId) return;
        const params = { conv: currentConvId };
        if (replyToId) params.reply_to = replyToId;
        const r = await apiUpload('send_file', params, 'file', file);
        if (r.success) { appendMessage(r.message); thread.scrollTop = thread.scrollHeight; refreshList(); }
        else if (r.error) alert(r.error);
        replyToId = null; replyPreview.style.display = 'none';
    });

    // ---------- Message vocal (MediaRecorder) ----------
    let mediaRecorder = null, recordedChunks = [], recordStart = 0, recordTimerInt = null;

    micBtn.addEventListener('click', async () => {
        if (!currentConvId) return;
        if (!navigator.mediaDevices || !window.MediaRecorder) {
            alert("L'enregistrement vocal n'est pas supporté par ce navigateur.");
            return;
        }
        try {
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            recordedChunks = [];
            mediaRecorder = new MediaRecorder(stream);
            mediaRecorder.ondataavailable = (e) => { if (e.data.size > 0) recordedChunks.push(e.data); };
            mediaRecorder.start();
            recordStart = Date.now();
            micBtn.classList.add('recording');
            composer.style.display = 'none';
            recordingBar.style.display = 'flex';
            recordTimerInt = setInterval(() => {
                const s = Math.floor((Date.now() - recordStart) / 1000);
                recordingTime.textContent = Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0');
            }, 250);
        } catch (err) {
            alert("Impossible d'accéder au micro.");
        }
    });

    function stopRecording() {
        clearInterval(recordTimerInt);
        micBtn.classList.remove('recording');
        composer.style.display = 'flex';
        recordingBar.style.display = 'none';
        if (mediaRecorder && mediaRecorder.state !== 'inactive') mediaRecorder.stop();
        if (mediaRecorder) mediaRecorder.stream.getTracks().forEach(t => t.stop());
    }

    document.getElementById('msgRecordingCancel').addEventListener('click', () => {
        if (mediaRecorder) mediaRecorder.onstop = null;
        stopRecording();
        recordedChunks = [];
    });

    document.getElementById('msgRecordingSend').addEventListener('click', () => {
        const duration = Math.round((Date.now() - recordStart) / 1000);
        if (!currentConvId) { stopRecording(); return; }
        if (mediaRecorder) {
            mediaRecorder.onstop = async () => {
                const blob = new Blob(recordedChunks, { type: 'audio/webm' });
                const params = { conv: currentConvId, duration };
                if (replyToId) params.reply_to = replyToId;
                const r = await apiUpload('send_voice', params, 'voice', blob);
                if (r.success) { appendMessage(r.message); thread.scrollTop = thread.scrollHeight; refreshList(); }
                else if (r.error) alert(r.error);
                replyToId = null; replyPreview.style.display = 'none';
            };
        }
        stopRecording();
    });

    // ---------- Retour (mobile) ----------
    backBtn.addEventListener('click', () => {
        shell.classList.remove('chat-open');
        clearInterval(pollTimer);
        currentConvId = null;
    });

    setInterval(refreshList, 8000);

    // ---------- Initialisation ----------
    async function init() {
        await refreshList();

        if (OPEN_TO) {
            const r = await api('open', { to: OPEN_TO }, 'POST');
            if (r.success) {
                await refreshList();
                openConversation(r.conv_id);
            } else if (r.error) {
                alert(r.error);
            }
        }
    }
    init();
})();
</script>

<?php include __DIR__ . '/../includes/footer.php'; ?>
