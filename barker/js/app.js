// ============================================================
// BARKER — Main JavaScript - VERSION CORRIGÉE
// ============================================================

// Vérifier que SITE_URL est défini
if (typeof SITE_URL === 'undefined') {
    console.error('SITE_URL is not defined! Check footer.php');
    window.SITE_URL = window.location.origin + '/barker2';
}

console.log('BARKER JS - SITE_URL:', SITE_URL);

// ============================================================
// UTILITAIRES CORRIGÉS
// ============================================================

// Récupérer le token CSRF
function getCsrfToken() {
    const tokenInput = document.querySelector('input[name="csrf_token"]');
    return tokenInput ? tokenInput.value : '';
}

/**
 * Décoder les entités HTML correctement - CORRIGÉ
 * Gère tous les caractères spéciaux, émojis, chinois, arabe, russe
 */
function decodeHtmlEntities(text) {
    if (!text) return '';
    try {
        // Utiliser un élément DOM pour décoder
        const textarea = document.createElement('textarea');
        textarea.innerHTML = text;
        return textarea.value;
    } catch (e) {
        console.error('Decode error:', e);
        return text;
    }
}

/**
 * Encoder les entités HTML pour l'insertion dans le DOM - Sécurité
 */
function encodeHtmlEntities(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Surligner le texte recherché
function highlightText(text, query) {
    if (!text) return '';
    if (!query || query.length < 2) return encodeHtmlEntities(text);
    try {
        const escapedQuery = query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        const regex = new RegExp('(' + escapedQuery + ')', 'gi');
        const encodedText = encodeHtmlEntities(text);
        return encodedText.replace(regex, '<span class="search-highlight">$1</span>');
    } catch (e) {
        return encodeHtmlEntities(text);
    }
}

// Toast notifications
function showToast(message, type = 'success') {
    let container = document.getElementById('toastContainer');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toastContainer';
        container.style.cssText = `
            position:fixed; bottom:24px; right:24px; z-index:9999;
            display:flex; flex-direction:column; gap:8px;
        `;
        document.body.appendChild(container);
    }

    const borderColor = type === 'error' ? '#FF3B30' : type === 'success' ? '#2ECC71' : '#FF9F1C';
    const iconClass = type === 'error' ? 'fa-circle-xmark' : type === 'success' ? 'fa-circle-check' : 'fa-circle-info';

    const toast = document.createElement('div');
    toast.style.cssText = `
        background:var(--bg-card, #1e1e1e); 
        border:1px solid var(--border, #333);
        padding:12px 18px; 
        border-radius:12px;
        box-shadow:0 4px 12px rgba(0,0,0,0.3); 
        font-size:0.9rem;
        display:flex; 
        align-items:center; 
        gap:8px;
        animation:fadeIn 0.3s ease; 
        max-width:300px;
        border-left: 4px solid ${borderColor};
        color: var(--text-primary, #fff);
    `;

    const icon = document.createElement('i');
    icon.className = `fa-solid ${iconClass}`;
    icon.style.color = borderColor;
    icon.style.flexShrink = '0';

    const text = document.createElement('span');
    text.textContent = message; // toujours du texte brut, jamais de HTML (anti-XSS)

    toast.appendChild(icon);
    toast.appendChild(text);
    container.appendChild(toast);
    
    setTimeout(() => { 
        toast.style.opacity = '0';
        toast.style.transition = 'opacity 0.3s ease';
        setTimeout(() => toast.remove(), 300); 
    }, 3000);
}

// Media Viewer
function openMediaViewer(src) {
    const overlay = document.createElement('div');
    overlay.style.cssText = `
        position:fixed; inset:0; background:rgba(0,0,0,0.95); 
        z-index:9999; display:flex; align-items:center; 
        justify-content:center; cursor:pointer;
    `;
    
    if (src.match(/\.(mp4|webm|mov)$/i)) {
        const video = document.createElement('video');
        video.src = src;
        video.controls = true;
        video.autoplay = true;
        video.style.cssText = 'max-width:90vw; max-height:90vh; border-radius:8px';
        overlay.appendChild(video);
    } else {
        const img = document.createElement('img');
        img.src = src;
        img.style.cssText = 'max-width:90vw; max-height:90vh; object-fit:contain; border-radius:8px';
        overlay.appendChild(img);
    }
    
    overlay.onclick = () => overlay.remove();
    document.body.appendChild(overlay);
    
    const escHandler = (e) => {
        if (e.key === 'Escape') {
            overlay.remove();
            document.removeEventListener('keydown', escHandler);
        }
    };
    document.addEventListener('keydown', escHandler);
}

// ============================================================
// THEME
// ============================================================

function toggleTheme() {
    const html = document.documentElement;
    const curr = html.getAttribute('data-theme');
    const next = curr === 'dark' ? 'light' : 'dark';
    html.setAttribute('data-theme', next);

    const icon = document.getElementById('themeIcon');
    const label = document.getElementById('themeLabel');
    if (icon) {
        icon.className = next === 'dark' ? 'fa-solid fa-sun' : 'fa-solid fa-moon';
    }
    if (label) label.textContent = next === 'dark' ? 'Light mode' : 'Dark mode';

    fetch(`${SITE_URL}/api/theme.php`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({theme: next})
    }).catch(err => console.error('Theme update failed:', err));
}

// ============================================================
// BARK MODAL
// ============================================================

function openBarkModal() {
    const modal = document.getElementById('barkModal');
    if (!modal) return;
    
    modal.classList.add('active');
    document.getElementById('barkTextarea')?.focus();
    
    const anonCheckbox = document.getElementById('anonToggle');
    if (anonCheckbox) {
        anonCheckbox.checked = false;
        const anonLabel = anonCheckbox.closest('.compose-anon-toggle');
        if (anonLabel) anonLabel.classList.remove('anon-active');
    }
}

function openBarkModalWithAnon() {
    const modal = document.getElementById('barkModal');
    if (!modal) return;
    
    modal.classList.add('active');
    document.getElementById('barkTextarea')?.focus();
    
    const anonCheckbox = document.getElementById('anonToggle');
    if (anonCheckbox) {
        anonCheckbox.checked = true;
        const anonLabel = anonCheckbox.closest('.compose-anon-toggle');
        if (anonLabel) {
            anonLabel.classList.add('anon-active');
            anonLabel.style.animation = 'pulse 0.5s ease';
            setTimeout(() => anonLabel.style.animation = '', 500);
        }
    }
    
    showToast('Barking anonymously', 'info');
}

function closeBarkModal() {
    const modal = document.getElementById('barkModal');
    if (!modal) return;
    
    modal.classList.remove('active');
    document.getElementById('barkForm')?.reset();
    
    const preview = document.getElementById('mediaPreviewModal');
    if (preview) preview.innerHTML = '';
    
    updateCharCount();
    
    const anonCheckbox = document.getElementById('anonToggle');
    if (anonCheckbox) {
        anonCheckbox.checked = false;
        const anonLabel = anonCheckbox.closest('.compose-anon-toggle');
        if (anonLabel) anonLabel.classList.remove('anon-active');
    }
}

function updateCharCount() {
    const ta = document.getElementById('barkTextarea');
    const el = document.getElementById('charCount');
    if (!ta || !el) return;
    
    const remaining = 300 - ta.value.length;
    el.textContent = remaining;
    el.className = 'compose-char-count';
    if (remaining <= 50) el.classList.add('warn');
    if (remaining <= 10) el.classList.add('danger');
}

function previewMedia(input) {
    const container = document.getElementById('mediaPreviewModal');
    if (!container) return;
    
    container.innerHTML = '';
    const files = Array.from(input.files).slice(0, 4);
    
    files.forEach((file) => {
        const wrap = document.createElement('div');
        wrap.className = 'media-preview-item';
        
        if (file.type.startsWith('image/')) {
            const img = document.createElement('img');
            img.src = URL.createObjectURL(file);
            wrap.appendChild(img);
        } else if (file.type.startsWith('video/')) {
            const vid = document.createElement('video');
            vid.src = URL.createObjectURL(file);
            vid.controls = true;
            wrap.appendChild(vid);
        }
        
        const btn = document.createElement('button');
        btn.className = 'media-preview-remove';
        btn.innerHTML = '<i class="fa-solid fa-xmark"></i>';
        btn.onclick = () => wrap.remove();
        wrap.appendChild(btn);
        container.appendChild(wrap);
    });
}

// ============================================================
// MY APPS MODAL
// ============================================================

function openMyAppsModal() {
    const modal = document.getElementById('myAppsModal');
    if (modal) modal.classList.add('active');
}

function closeMyAppsModal() {
    const modal = document.getElementById('myAppsModal');
    if (modal) modal.classList.remove('active');
}

// ============================================================
// SEARCH MODAL
// ============================================================

let searchTimeout = null;
let currentFilter = 'all';
let currentQuery = '';

function openSearchModal() {
    const modal = document.getElementById('searchModal');
    if (modal) {
        modal.classList.add('active');
        document.getElementById('modalSearchInput')?.focus();
    }
}

function closeSearchModal() {
    const modal = document.getElementById('searchModal');
    if (!modal) return;
    
    modal.classList.remove('active');
    
    const searchInput = document.getElementById('modalSearchInput');
    if (searchInput) searchInput.value = '';
    
    const resultsContainer = document.getElementById('searchResults');
    if (resultsContainer) {
        resultsContainer.innerHTML = `
            <div style="text-align:center; padding:40px; color:var(--text-muted)">
                <i class="fa-solid fa-search" style="font-size:2rem; margin-bottom:12px; opacity:0.5"></i>
                <p>Start typing to search...</p>
            </div>
        `;
    }
    
    const viewAllDiv = document.getElementById('viewAllResults');
    if (viewAllDiv) viewAllDiv.style.display = 'none';
    
    currentQuery = '';
}

function setSearchFilter(filter) {
    currentFilter = filter;
    
    document.querySelectorAll('.search-filter-btn').forEach(btn => {
        btn.classList.remove('active');
        if (btn.dataset.filter === filter) btn.classList.add('active');
    });
    
    if (currentQuery.length >= 2) performSearch(currentQuery);
}

function performSearch(query) {
    const resultsContainer = document.getElementById('searchResults');
    const viewAllDiv = document.getElementById('viewAllResults');
    const viewAllLink = document.getElementById('viewAllLink');
    
    resultsContainer.innerHTML = `
        <div style="text-align:center; padding:20px">
            <i class="fa-solid fa-spinner fa-spin"></i> Searching...
        </div>
    `;
    
    viewAllLink.href = `${SITE_URL}/pages/search.php?q=${encodeURIComponent(query)}&tab=${currentFilter}`;
    viewAllDiv.style.display = 'block';
    
    fetch(`${SITE_URL}/api/search.php?q=${encodeURIComponent(query)}`)
        .then(res => res.json())
        .then(data => displaySearchResults(data, query))
        .catch(err => {
            console.error('Search error:', err);
            resultsContainer.innerHTML = `
                <div style="text-align:center; padding:20px; color:var(--text-muted)">
                    <p>Error searching. Please try again.</p>
                </div>
            `;
        });
}

function displaySearchResults(data, query) {
    const container = document.getElementById('searchResults');
    container.innerHTML = '';
    
    let hasResults = false;
    
    // Utilisateurs
    if ((currentFilter === 'all' || currentFilter === 'users') && data.users?.length > 0) {
        hasResults = true;
        
        const header = document.createElement('div');
        header.style.cssText = 'padding:8px 0; font-size:0.8rem; font-weight:700; color:var(--text-muted); text-transform:uppercase; letter-spacing:1px';
        header.textContent = 'Users';
        container.appendChild(header);
        
        data.users.slice(0, 5).forEach(user => {
            const item = document.createElement('a');
            item.href = user.url;
            item.className = 'search-result-item';
            item.innerHTML = `
                <img src="${user.avatar}" class="search-result-avatar" alt="${encodeHtmlEntities(user.display_name)}">
                <div class="search-result-info">
                    <div class="search-result-name">
                        ${highlightText(user.display_name, query)}
                        ${user.is_verified ? '<span class="verified-badge">✓</span>' : ''}
                    </div>
                    <div class="search-result-handle">@${highlightText(user.username, query)}</div>
                </div>
                <i class="fa-solid fa-chevron-right" style="color:var(--text-muted)"></i>
            `;
            container.appendChild(item);
        });
    }
    
    // Hashtags
    if (currentFilter === 'all' && data.hashtags?.length > 0) {
        const header = document.createElement('div');
        header.style.cssText = 'padding:16px 0 8px; font-size:0.8rem; font-weight:700; color:var(--text-muted); text-transform:uppercase; letter-spacing:1px';
        header.textContent = 'Hashtags';
        container.appendChild(header);
        
        data.hashtags.slice(0, 5).forEach(tag => {
            const item = document.createElement('a');
            item.href = `${SITE_URL}/pages/hashtag.php?tag=${encodeURIComponent(tag.name)}`;
            item.className = 'search-result-item';
            item.innerHTML = `
                <div style="width:44px; height:44px; display:flex; align-items:center; justify-content:center; background:var(--orange-faint); border-radius:50%; color:var(--orange); font-size:1.2rem">#</div>
                <div class="search-result-info">
                    <div class="search-result-name" style="color:var(--orange)">${highlightText('#' + tag.name, query)}</div>
                    <div class="search-result-handle">${tag.bark_count} barks</div>
                </div>
                <i class="fa-solid fa-chevron-right" style="color:var(--text-muted)"></i>
            `;
            container.appendChild(item);
            hasResults = true;
        });
    }
    
    if (!hasResults) {
        container.innerHTML = `
            <div style="text-align:center; padding:40px; color:var(--text-muted)">
                <i class="fa-solid fa-search" style="font-size:2rem; margin-bottom:12px; opacity:0.5"></i>
                <p>No results for "${encodeHtmlEntities(query)}"</p>
                <p style="font-size:0.85rem; margin-top:8px">Try different keywords</p>
            </div>
        `;
    }
}

// ============================================================
// ACTIONS (Paw, Rebark, Yap, Howl, Bone, Report, Follow, Delete)
// ============================================================

async function togglePaw(barkId, btn) {
    const wasActive = btn.classList.contains('active');
    btn.classList.toggle('active');
    const countEl = btn.querySelector('.action-count');
    countEl.textContent = parseInt(countEl.textContent) + (btn.classList.contains('active') ? 1 : -1);
    btn.classList.add('paw-anim');
    setTimeout(() => btn.classList.remove('paw-anim'), 300);

    try {
        const fd = new FormData();
        fd.append('bark_id', barkId);
        fd.append('csrf_token', getCsrfToken());
        
        const res = await fetch(`${SITE_URL}/api/paw.php`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (!data.success) {
            btn.classList.toggle('active');
            countEl.textContent = parseInt(countEl.textContent) + (wasActive ? 1 : -1);
            if (data.error) showToast(data.error, 'error');
        }
    } catch (error) {
        console.error('Paw error:', error);
        btn.classList.toggle('active');
        countEl.textContent = parseInt(countEl.textContent) + (wasActive ? 1 : -1);
        showToast('Network error', 'error');
    }
}

async function toggleRebark(barkId, btn) {
    const fd = new FormData();
    fd.append('bark_id', barkId);
    fd.append('csrf_token', getCsrfToken());
    fd.append('is_anonymous', 0);

    try {
        const res = await fetch(`${SITE_URL}/api/rebark.php`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            btn.classList.toggle('active');
            const cnt = btn.querySelector('.action-count');
            cnt.textContent = parseInt(cnt.textContent) + (btn.classList.contains('active') ? 1 : -1);
            showToast(data.rebarked ? 'Rebarked!' : 'Rebark removed', 'success');
        } else {
            showToast(data.error || 'Error', 'error');
        }
    } catch (error) {
        console.error('Rebark error:', error);
        showToast('Network error', 'error');
    }
}

function openYap(barkId, previewHtml) {
    document.getElementById('yapBarkId').value = barkId;
    // Décoder correctement le preview HTML
    const decodedPreview = decodeHtmlEntities(previewHtml);
    document.getElementById('yapOriginalBark').innerHTML = decodedPreview;
    document.getElementById('yapContent').value = '';
    document.getElementById('yapModal').classList.add('active');
    setTimeout(() => document.getElementById('yapContent').focus(), 100);
}

function closeYapModal() {
    document.getElementById('yapModal')?.classList.remove('active');
}

async function submitYap() {
    const barkId = document.getElementById('yapBarkId').value;
    const content = document.getElementById('yapContent').value.trim();
    const anon = document.getElementById('yapAnon').checked ? 1 : 0;

    if (!content) return showToast('Write something first!', 'error');

    const fd = new FormData();
    fd.append('bark_id', barkId);
    fd.append('content', content);
    fd.append('is_anonymous', anon);
    fd.append('csrf_token', getCsrfToken());

    try {
        const res = await fetch(`${SITE_URL}/api/yap.php`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            closeYapModal();
            showToast('Yap sent!', 'success');
            setTimeout(() => location.reload(), 800);
        } else {
            showToast(data.error || 'Error', 'error');
        }
    } catch (error) {
        console.error('Yap error:', error);
        showToast('Network error', 'error');
    }
}

function openHowl(barkId, previewHtml) {
    document.getElementById('howlBarkId').value = barkId;
    // Décoder correctement le preview HTML
    const decodedPreview = decodeHtmlEntities(previewHtml);
    document.getElementById('howlQuotedBark').innerHTML = decodedPreview;
    document.getElementById('howlContent').value = '';
    document.getElementById('howlModal').classList.add('active');
    setTimeout(() => document.getElementById('howlContent').focus(), 100);
}

function closeHowlModal() {
    document.getElementById('howlModal')?.classList.remove('active');
}

async function submitHowl() {
    const barkId = document.getElementById('howlBarkId').value;
    const content = document.getElementById('howlContent').value.trim();
    const anon = document.getElementById('howlAnon').checked ? 1 : 0;

    if (!content) return showToast('Add your howl first!', 'error');

    const fd = new FormData();
    fd.append('bark_id', barkId);
    fd.append('content', content);
    fd.append('is_anonymous', anon);
    fd.append('csrf_token', getCsrfToken());

    try {
        const res = await fetch(`${SITE_URL}/api/howl.php`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            closeHowlModal();
            showToast('<i class="fa-solid fa-wifi sound"></i> Howl posted!', 'success');
            setTimeout(() => location.reload(), 800);
        } else {
            showToast(data.error || 'Error', 'error');
        }
    } catch (error) {
        console.error('Howl error:', error);
        showToast('Network error', 'error');
    }
}

let selectedBoneAmount = null;

function openBoneModal(barkId, receiverId, receiverName) {
    document.getElementById('boneBarkId').value = barkId;
    document.getElementById('boneReceiverId').value = receiverId;
    document.getElementById('boneReceiverName').textContent = decodeHtmlEntities(receiverName);
    document.getElementById('customBoneAmount').value = '';
    document.querySelectorAll('.bone-amount-btn').forEach(b => b.classList.remove('selected'));
    selectedBoneAmount = null;
    document.getElementById('boneModal').classList.add('active');
}

function closeBoneModal() {
    document.getElementById('boneModal')?.classList.remove('active');
}

function selectBoneAmount(amount, btn) {
    selectedBoneAmount = amount;
    document.getElementById('customBoneAmount').value = '';
    document.querySelectorAll('.bone-amount-btn').forEach(b => b.classList.remove('selected'));
    btn.classList.add('selected');
}

function clearBonePreset() {
    selectedBoneAmount = null;
    document.querySelectorAll('.bone-amount-btn').forEach(b => b.classList.remove('selected'));
}

async function sendBone() {
    const amount = selectedBoneAmount || parseFloat(document.getElementById('customBoneAmount').value);
    const barkId = document.getElementById('boneBarkId').value;
    const recvId = document.getElementById('boneReceiverId').value;
    const message = document.getElementById('boneMessage').value;

    if (!amount || amount < 0.50) return showToast('Minimum donation is €0.50', 'error');

    const fd = new FormData();
    fd.append('bark_id', barkId);
    fd.append('receiver_id', recvId);
    fd.append('amount', amount);
    fd.append('message', message);
    fd.append('csrf_token', getCsrfToken());

    try {
        const res = await fetch(`${SITE_URL}/api/bone.php`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            closeBoneModal();
            showToast(`Bone sent! €${amount.toFixed(2)} donated`, 'success');
            setTimeout(() => location.reload(), 800);
        } else {
            showToast(data.error || 'Error sending bone', 'error');
        }
    } catch (error) {
        console.error('Bone error:', error);
        showToast('Network error', 'error');
    }
}

function openReport(barkId) {
    document.getElementById('reportBarkId').value = barkId;
    document.getElementById('reportModal').classList.add('active');
}

function closeReportModal() {
    document.getElementById('reportModal')?.classList.remove('active');
}

async function submitReport() {
    const barkId = document.getElementById('reportBarkId').value;
    const reason = document.getElementById('reportReason').value;
    const details = document.getElementById('reportDetails').value;

    const fd = new FormData();
    fd.append('bark_id', barkId);
    fd.append('reason', reason);
    fd.append('details', details);
    fd.append('csrf_token', getCsrfToken());

    try {
        const res = await fetch(`${SITE_URL}/api/report.php`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            closeReportModal();
            showToast('🚨 Report submitted', 'success');
        } else {
            showToast(data.error || 'Error', 'error');
        }
    } catch (error) {
        console.error('Report error:', error);
        showToast('Network error', 'error');
    }
}

async function toggleFollow(userId, btn) {
    const fd = new FormData();
    fd.append('user_id', userId);
    fd.append('csrf_token', getCsrfToken());

    try {
        const res = await fetch(`${SITE_URL}/api/follow.php`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            btn.textContent = data.following ? 'Following' : 'Follow';
            btn.classList.toggle('following', data.following);
            showToast(data.following ? 'Followed!' : 'Unfollowed', 'success');
        } else {
            showToast(data.error || 'Error', 'error');
        }
    } catch (error) {
        console.error('Follow error:', error);
        showToast('Network error', 'error');
    }
}

async function deleteBark(barkId) {
    if (!confirm('Delete this bark? This cannot be undone.')) return;
    
    const fd = new FormData();
    fd.append('bark_id', barkId);
    fd.append('csrf_token', getCsrfToken());

    try {
        const res = await fetch(`${SITE_URL}/api/bark.php?action=delete`, {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            const card = document.querySelector(`[data-bark-id="${barkId}"]`);
            if (card) {
                card.style.transition = 'all 0.3s ease';
                card.style.opacity = '0';
                card.style.height = card.offsetHeight + 'px';
                setTimeout(() => {
                    card.style.height = '0';
                    card.style.padding = '0';
                    card.style.margin = '0';
                }, 200);
                setTimeout(() => card.remove(), 400);
            }
            showToast('Bark deleted', 'success');
        } else {
            showToast(data.error || 'Error deleting bark', 'error');
        }
    } catch (error) {
        console.error('Delete error:', error);
        showToast('Network error', 'error');
    }
}

// ============================================================
// NOTIFICATIONS POLLING
// ============================================================

function pollNotifications() {
    fetch(`${SITE_URL}/api/notifications.php?action=count`, {
        credentials: 'same-origin'
    })
        .then(r => r.json())
        .then(data => {
            updateBadge('notifications', data.notifications || 0);
            updateBadge('messages', data.messages || 0);
        })
        .catch(err => console.error('Poll error:', err));
}

function updateBadge(type, count) {
    document.querySelectorAll('.nav-badge').forEach(b => {
        const parent = b.closest('.nav-item');
        if (!parent) return;
        const href = parent.getAttribute('href') || '';
        
        if (type === 'notifications' && href.includes('notifications')) {
            b.textContent = count > 99 ? '99+' : count;
            b.style.display = count > 0 ? 'inline-block' : 'none';
        }
        if (type === 'messages' && href.includes('messages')) {
            b.textContent = count > 99 ? '99+' : count;
            b.style.display = count > 0 ? 'inline-block' : 'none';
        }
    });
}

// ============================================================
// INITIALISATION
// ============================================================

document.addEventListener('DOMContentLoaded', function() {
    console.log('BARKER JS - DOM chargé !');
    
    // Compteur de caractères
    const textarea = document.getElementById('barkTextarea');
    if (textarea) textarea.addEventListener('input', updateCharCount);
    
    // Submit Bark
    document.getElementById('barkForm')?.addEventListener('submit', async function(e) {
        e.preventDefault();
        
        const btn = this.querySelector('.compose-submit');
        const originalText = btn.textContent;
        btn.disabled = true;
        btn.textContent = 'Barking...';

        const fd = new FormData(this);
        
        try {
            const res = await fetch(`${SITE_URL}/api/bark.php?action=post`, {
                method: 'POST', 
                body: fd,
                credentials: 'same-origin'
            });
            const data = await res.json();
            
            if (data.success) {
                closeBarkModal();
                showToast('Bark posted!', 'success');
                setTimeout(() => location.reload(), 800);
            } else {
                showToast(data.error || 'Failed to bark', 'error');
                btn.disabled = false;
                btn.textContent = originalText;
            }
        } catch (error) {
            console.error('Bark error:', error);
            showToast('Network error', 'error');
            btn.disabled = false;
            btn.textContent = originalText;
        }
    });
    
    // Fermer les modals avec le bouton close
    document.querySelectorAll('.modal-close').forEach(btn => {
        btn.addEventListener('click', function() {
            this.closest('.modal-overlay')?.classList.remove('active');
        });
    });
    
    // Search modal input
    const modalSearchInput = document.getElementById('modalSearchInput');
    if (modalSearchInput) {
        modalSearchInput.addEventListener('input', function() {
            const query = this.value.trim();
            currentQuery = query;
            
            clearTimeout(searchTimeout);
            
            if (query.length < 2) {
                document.getElementById('searchResults').innerHTML = `
                    <div style="text-align:center; padding:40px; color:var(--text-muted)">
                        <i class="fa-solid fa-search" style="font-size:2rem; margin-bottom:12px; opacity:0.5"></i>
                        <p>Type at least 2 characters to search...</p>
                    </div>
                `;
                document.getElementById('viewAllResults').style.display = 'none';
                return;
            }
            
            searchTimeout = setTimeout(() => performSearch(query), 300);
        });
        
        modalSearchInput.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' && this.value.trim().length >= 2) {
                const query = this.value.trim();
                window.location.href = `${SITE_URL}/pages/search.php?q=${encodeURIComponent(query)}&tab=${currentFilter}`;
            }
        });
    }
    
    // Auto-expand textareas
    document.querySelectorAll('textarea').forEach(ta => {
        ta.addEventListener('input', function() {
            this.style.height = 'auto';
            this.style.height = this.scrollHeight + 'px';
        });
    });
    
    // Polling notifications
    if (document.querySelector('.sidebar-left')) {
        pollNotifications();
        setInterval(pollNotifications, 30000);
    }
});

// Dropdown menus
document.addEventListener('click', function(e) {
    const trigger = e.target.closest('[data-dropdown]');
    if (trigger) {
        e.preventDefault();
        e.stopPropagation();
        const targetId = trigger.getAttribute('data-dropdown');
        const dd = document.getElementById(targetId);
        if (dd) {
            document.querySelectorAll('.dropdown.open').forEach(d => {
                if (d !== dd) d.classList.remove('open');
            });
            dd.classList.toggle('open');
        }
    } else {
        document.querySelectorAll('.dropdown.open').forEach(d => d.classList.remove('open'));
    }
});

// Close modals on overlay click
document.querySelectorAll('.modal-overlay').forEach(overlay => {
    overlay.addEventListener('click', e => {
        if (e.target === overlay) overlay.classList.remove('active');
    });
});

// Keyboard shortcuts
document.addEventListener('keydown', e => {
    if (e.key === 'Escape') {
        document.querySelectorAll('.modal-overlay.active').forEach(m => m.classList.remove('active'));
        document.querySelectorAll('.dropdown.open').forEach(d => d.classList.remove('open'));
    }
    
    // Ctrl+K ou / pour recherche
    if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
        e.preventDefault();
        openSearchModal();
    }
    
    if (e.key === '/' && !['INPUT','TEXTAREA','SELECT'].includes(document.activeElement.tagName)) {
        e.preventDefault();
        openSearchModal();
    }
    
    // N pour nouveau bark
    if (e.key === 'n' && !e.ctrlKey && !e.metaKey && !e.shiftKey &&
        !['INPUT','TEXTAREA','SELECT'].includes(document.activeElement.tagName)) {
        e.preventDefault();
        openBarkModal();
    }
    
    // A pour bark anonyme
    if (e.key === 'a' && !e.ctrlKey && !e.metaKey && !e.shiftKey &&
        !['INPUT','TEXTAREA','SELECT'].includes(document.activeElement.tagName)) {
        e.preventDefault();
        openBarkModalWithAnon();
    }
});

console.log('BARKER JS loaded successfully !');

// ── Bury (Bookmark) ──────────────────────────────────────────
async function toggleBury(barkId, btn) {
    const wasActive = btn.classList.contains('active');
    btn.classList.toggle('active');
    
    try {
        const fd = new FormData();
        fd.append('bark_id', barkId);
        fd.append('csrf_token', getCsrfToken());
        
        const res = await fetch(SITE_URL + '/api/bury.php', {
            method: 'POST', 
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            showToast(data.buried ? 'Bark buried!' : 'Bury removed', 'success');
        } else {
            btn.classList.toggle('active');
            showToast(data.error || 'Error', 'error');
        }
    } catch (error) {
        console.error('Bury error:', error);
        btn.classList.toggle('active');
        showToast('Network error', 'error');
    }
}

// ── Edit Bark Modal ──────────────────────────────────────────
let editingBarkId = null;

function openEditBarkModal(barkId, currentContent) {
    editingBarkId = barkId;
    
    // Décoder le contenu HTML correctement
    const decodedContent = decodeHtmlEntities(currentContent);
    
    document.getElementById('editBarkId').value = barkId;
    document.getElementById('editBarkContent').value = decodedContent;
    document.getElementById('editCharCount').textContent = 300 - decodedContent.length;
    document.getElementById('editBarkModal').classList.add('active');
    
    setTimeout(() => document.getElementById('editBarkContent').focus(), 100);
}

function closeEditBarkModal() {
    document.getElementById('editBarkModal')?.classList.remove('active');
    editingBarkId = null;
}

function updateEditCharCount() {
    const ta = document.getElementById('editBarkContent');
    const el = document.getElementById('editCharCount');
    if (!ta || !el) return;
    
    const remaining = 300 - ta.value.length;
    el.textContent = remaining;
    el.className = 'compose-char-count';
    if (remaining <= 50) el.classList.add('warn');
    if (remaining <= 10) el.classList.add('danger');
}

async function submitEditBark() {
    const content = document.getElementById('editBarkContent').value.trim();
    
    if (!content) {
        showToast('Content is required', 'error');
        return;
    }
    
    if (content.length > 300) {
        showToast('Content too long', 'error');
        return;
    }
    
    const fd = new FormData();
    fd.append('bark_id', editingBarkId);
    fd.append('content', content);
    fd.append('csrf_token', getCsrfToken());
    
    try {
        const res = await fetch(SITE_URL + '/api/bark.php?action=edit', {
            method: 'POST',
            body: fd,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            closeEditBarkModal();
            showToast('✏️ Bark updated!', 'success');
            setTimeout(() => location.reload(), 800);
        } else {
            showToast(data.error || 'Error updating bark', 'error');
        }
    } catch (error) {
        console.error('Edit error:', error);
        showToast('Network error', 'error');
    }
}

// ============================================================
// POLL SYSTEM
// ============================================================

async function loadPoll(pollId, container) {
    if (container.hasAttribute('data-loaded') && container.getAttribute('data-loaded') === 'true') {
        console.log('Poll already loaded, skipping:', pollId);
        return;
    }
    
    if (container.innerHTML.trim() !== '' && container.querySelector('.poll-card')) {
        console.log('Poll already has content, skipping:', pollId);
        container.setAttribute('data-loaded', 'true');
        return;
    }
    
    try {
        const res = await fetch(`${SITE_URL}/api/poll.php?action=get&poll_id=${pollId}`, {
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            container.setAttribute('data-loaded', 'true');
            renderPoll(data, container);
        } else {
            container.innerHTML = `<div class="poll-error">Failed to load poll</div>`;
        }
    } catch (error) {
        console.error('Poll load error:', error);
        container.innerHTML = `<div class="poll-error">Error loading poll</div>`;
    }
}

function renderPoll(data, container) {
    const poll = data.poll;
    const options = data.options;
    const userVoted = data.user_voted;
    const isExpired = data.is_expired;
    
    let html = `
        <div class="poll-card">
            <div class="poll-question">
                <i class="fa-solid fa-chart-simple"></i>
                ${encodeHtmlEntities(poll.question)}
            </div>
    `;
    
    if (isExpired) {
        html += `<div class="poll-expired-badge"><i class="fa-solid fa-clock"></i> Poll closed</div>`;
    }
    
    if (poll.poll_type === 'rating') {
        html += renderRatingPoll(data, userVoted, isExpired);
    } else if (poll.poll_type === 'duel') {
        html += renderDuelPoll(data, userVoted, isExpired);
    } else {
        html += renderStandardPoll(data, userVoted, isExpired);
    }
    
    html += `
            <div class="poll-footer">
                <span class="poll-vote-count">${data.total_votes} vote${data.total_votes !== 1 ? 's' : ''}</span>
            </div>
        </div>
    `;
    
    if (container.innerHTML.trim() === '' || !container.querySelector('.poll-card')) {
        container.innerHTML = html;
    }
    
    if (!isExpired && !userVoted) {
        attachPollEventListeners(data.poll.id, poll.poll_type, container);
    }
}

function renderStandardPoll(data, userVoted, isExpired) {
    const poll = data.poll;
    const options = data.options;
    const isMultiple = poll.poll_type === 'multiple';
    
    let html = `<div class="poll-options ${isMultiple ? 'poll-multiple' : ''}">`;
    
    options.forEach(opt => {
        const checked = userVoted && data.user_choice === opt.id ? 'checked' : '';
        const percentage = opt.percentage;
        
        if (userVoted || isExpired) {
            html += `
                <div class="poll-option-result">
                    <div class="poll-option-text">
                        ${encodeHtmlEntities(opt.option_text)}
                        <span class="poll-option-percent">${percentage}%</span>
                    </div>
                    <div class="poll-progress-bar">
                        <div class="poll-progress-fill" style="width: ${percentage}%"></div>
                    </div>
                    <div class="poll-option-votes">${opt.vote_count} vote${opt.vote_count !== 1 ? 's' : ''}</div>
                </div>
            `;
        } else {
            html += `
                <label class="poll-option ${isMultiple ? 'poll-checkbox' : 'poll-radio'}">
                    <input type="${isMultiple ? 'checkbox' : 'radio'}" 
                           name="poll_option" 
                           value="${opt.id}"
                           ${isMultiple ? '' : 'data-poll-id="' + data.poll.id + '"'}>
                    <span class="poll-option-text">${encodeHtmlEntities(opt.option_text)}</span>
                </label>
            `;
        }
    });
    
    if (!userVoted && !isExpired) {
        html += `
            <button class="poll-vote-btn btn btn-primary btn-sm" onclick="submitPollVote(${data.poll.id}, '${poll.poll_type}')">
                <i class="fa-solid fa-check"></i> Vote
            </button>
        `;
    }
    
    html += `</div>`;
    return html;
}

function renderRatingPoll(data, userVoted, isExpired) {
    const poll = data.poll;
    const options = data.options;
    
    let html = `<div class="poll-rating-container">`;
    
    if (poll.media_url && poll.media_type) {
        html += `
            <div class="rating-media-container">
                <div class="rating-media" onclick="openMediaViewer('${SITE_URL}/assets/uploads/polls/${encodeHtmlEntities(poll.media_url)}')">
                    ${poll.media_type === 'image' 
                        ? `<img src="${SITE_URL}/assets/uploads/polls/${encodeHtmlEntities(poll.media_url)}" alt="Rating media" style="max-width:100%; max-height:250px; border-radius:12px; object-fit:cover">`
                        : `<video src="${SITE_URL}/assets/uploads/polls/${encodeHtmlEntities(poll.media_url)}" controls preload="metadata" style="max-width:100%; max-height:250px; border-radius:12px" onclick="event.stopPropagation()"></video>`
                    }
                </div>
                <div class="rating-media-caption">
                    <i class="fa-solid fa-camera"></i> Click to view full size
                </div>
            </div>
        `;
    }
    
    if (userVoted || isExpired) {
        html += `<div class="rating-results">`;
        options.forEach(opt => {
            const percentage = opt.percentage;
            const stars = parseInt(opt.option_text);
            html += `
                <div class="rating-row">
                    <div class="rating-stars-label">
                        ${'★'.repeat(stars)}${'☆'.repeat(5-stars)}
                    </div>
                    <div class="poll-progress-bar" style="flex:1">
                        <div class="poll-progress-fill" style="width: ${percentage}%"></div>
                    </div>
                    <div class="rating-percent">${percentage}%</div>
                </div>
            `;
        });
        html += `<div class="average-rating">
            <i class="fa-solid fa-star"></i> Average: ${calculateAverageRating(options)}/5 ★ (${data.total_votes} vote${data.total_votes !== 1 ? 's' : ''})
        </div>`;
        html += `</div>`;
    } else {
        html += `
            <div class="rating-vote">
                <div class="rating-stars-input" id="ratingStars-${poll.id}">
                    ${[1,2,3,4,5].map(i => `
                        <i class="fa-regular fa-star rating-star-select" data-value="${i}" data-poll-id="${poll.id}"></i>
                    `).join('')}
                </div>
                <button class="poll-vote-btn btn btn-primary btn-sm" onclick="submitRatingVote(${poll.id})">
                    <i class="fa-solid fa-check"></i> Rate this
                </button>
            </div>
        `;
    }
    
    html += `</div>`;
    return html;
}

function renderDuelPoll(data, userVoted, isExpired) {
    const poll = data.poll;
    const options = data.options;
    
    let html = `<div class="poll-duel-container">`;
    
    if (userVoted || isExpired) {
        html += `<div class="duel-results">`;
        options.forEach(opt => {
            const percentage = opt.percentage;
            html += `
                <div class="duel-option-result">
                    ${opt.image_url ? `<img src="${SITE_URL}/assets/uploads/polls/${encodeHtmlEntities(opt.image_url)}" class="duel-result-image">` : ''}
                    <div class="duel-option-text">${encodeHtmlEntities(opt.option_text)}</div>
                    <div class="duel-percentage">${percentage}%</div>
                    <div class="poll-progress-bar">
                        <div class="poll-progress-fill" style="width: ${percentage}%"></div>
                    </div>
                    <div class="duel-votes">${opt.vote_count} votes</div>
                </div>
            `;
        });
        html += `</div>`;
    } else {
        html += `<div class="duel-vote">`;
        options.forEach(opt => {
            html += `
                <div class="duel-option-vote" onclick="selectDuelOption(${poll.id}, ${opt.id})" data-opt-id="${opt.id}">
                    ${opt.image_url ? `<img src="${SITE_URL}/assets/uploads/polls/${encodeHtmlEntities(opt.image_url)}" class="duel-vote-image">` : ''}
                    <div class="duel-option-text">${encodeHtmlEntities(opt.option_text)}</div>
                </div>
            `;
        });
        html += `</div>`;
        html += `<input type="hidden" id="selectedDuelOption-${poll.id}" value="">`;
        html += `<button class="poll-vote-btn btn btn-primary btn-sm" onclick="submitDuelVote(${poll.id})">
            <i class="fa-solid fa-fist-raised"></i> Fight!
        </button>`;
    }
    
    html += `</div>`;
    return html;
}

function calculateAverageRating(options) {
    let total = 0;
    let votes = 0;
    options.forEach(opt => {
        const stars = parseInt(opt.option_text);
        total += stars * opt.vote_count;
        votes += opt.vote_count;
    });
    if (votes === 0) return 0;
    return (total / votes).toFixed(1);
}

function attachPollEventListeners(pollId, pollType, container) {
    if (pollType === 'single') {
        const radios = container.querySelectorAll('input[type="radio"]');
        radios.forEach(radio => {
            radio.addEventListener('change', () => {
                submitPollVote(pollId, pollType);
            });
        });
    }
}

async function submitPollVote(pollId, pollType) {
    const container = document.getElementById(`poll-${pollId}`);
    const formData = new FormData();
    formData.append('poll_id', pollId);
    formData.append('csrf_token', getCsrfToken());
    
    if (pollType === 'multiple') {
        const checked = container.querySelectorAll('input[type="checkbox"]:checked');
        const optionIds = Array.from(checked).map(cb => cb.value);
        if (optionIds.length === 0) {
            showToast('Select at least one option', 'error');
            return;
        }
        optionIds.forEach(id => formData.append('option_ids[]', id));
    } else {
        const selected = container.querySelector('input[type="radio"]:checked');
        if (!selected) {
            showToast('Select an option', 'error');
            return;
        }
        formData.append('option_id', selected.value);
    }
    
    try {
        const res = await fetch(`${SITE_URL}/api/poll.php?action=vote`, {
            method: 'POST',
            body: formData,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            showToast('Vote recorded!', 'success');
            loadPoll(pollId, container);
        } else {
            showToast(data.error || 'Error voting', 'error');
        }
    } catch (error) {
        console.error('Vote error:', error);
        showToast('Network error', 'error');
    }
}

async function submitRatingVote(pollId) {
    const selectedStar = document.querySelector(`#ratingStars-${pollId} .rating-star-select.selected`);
    if (!selectedStar) {
        showToast('Select a rating', 'error');
        return;
    }
    
    const ratingValue = selectedStar.dataset.value;
    const formData = new FormData();
    formData.append('poll_id', pollId);
    formData.append('rating_value', ratingValue);
    formData.append('csrf_token', getCsrfToken());
    
    try {
        const res = await fetch(`${SITE_URL}/api/poll.php?action=vote`, {
            method: 'POST',
            body: formData,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            showToast(`Rated ${ratingValue} stars!`, 'success');
            const container = document.getElementById(`poll-${pollId}`);
            loadPoll(pollId, container);
        } else {
            showToast(data.error || 'Error voting', 'error');
        }
    } catch (error) {
        console.error('Rating vote error:', error);
        showToast('Network error', 'error');
    }
}

let selectedDuelOption = null;

function selectDuelOption(pollId, optionId) {
    document.querySelectorAll(`#poll-${pollId} .duel-option-vote`).forEach(opt => {
        opt.classList.remove('selected');
    });
    const selected = document.querySelector(`#poll-${pollId} .duel-option-vote[data-opt-id="${optionId}"]`);
    if (selected) {
        selected.classList.add('selected');
        selectedDuelOption = optionId;
    }
}

async function submitDuelVote(pollId) {
    if (!selectedDuelOption) {
        showToast('Choose your champion!', 'error');
        return;
    }
    
    const formData = new FormData();
    formData.append('poll_id', pollId);
    formData.append('option_id', selectedDuelOption);
    formData.append('csrf_token', getCsrfToken());
    
    try {
        const res = await fetch(`${SITE_URL}/api/poll.php?action=vote`, {
            method: 'POST',
            body: formData,
            credentials: 'same-origin'
        });
        const data = await res.json();
        
        if (data.success) {
            showToast('Vote cast!', 'success');
            const container = document.getElementById(`poll-${pollId}`);
            loadPoll(pollId, container);
        } else {
            showToast(data.error || 'Error voting', 'error');
        }
    } catch (error) {
        console.error('Duel vote error:', error);
        showToast('Network error', 'error');
    }
}

// Star rating hover effect
document.addEventListener('DOMContentLoaded', function() {
    document.addEventListener('mouseover', function(e) {
        if (e.target.classList && e.target.classList.contains('rating-star-select')) {
            const val = parseInt(e.target.dataset.value);
            const container = e.target.parentElement;
            const stars = container.querySelectorAll('.rating-star-select');
            stars.forEach((star, idx) => {
                if (idx < val) {
                    star.className = 'fa-solid fa-star rating-star-select';
                } else {
                    star.className = 'fa-regular fa-star rating-star-select';
                }
            });
        }
    });
    
    document.addEventListener('mouseout', function(e) {
        if (e.target.classList && e.target.classList.contains('rating-star-select')) {
            const container = e.target.parentElement;
            const stars = container.querySelectorAll('.rating-star-select');
            stars.forEach(star => {
                if (!star.classList.contains('selected')) {
                    star.className = 'fa-regular fa-star rating-star-select';
                } else {
                    star.className = 'fa-solid fa-star rating-star-select selected';
                }
            });
        }
    });
    
    document.addEventListener('click', function(e) {
        if (e.target.classList && e.target.classList.contains('rating-star-select')) {
            const container = e.target.parentElement;
            const val = parseInt(e.target.dataset.value);
            const stars = container.querySelectorAll('.rating-star-select');
            stars.forEach((star, idx) => {
                star.classList.remove('selected');
                if (idx < val) {
                    star.className = 'fa-solid fa-star rating-star-select selected';
                } else {
                    star.className = 'fa-regular fa-star rating-star-select';
                }
            });
        }
    });
    
    document.querySelectorAll('.poll-container').forEach(container => {
        const pollId = container.dataset.pollId;
        if (pollId && (!container.hasAttribute('data-loaded') || container.getAttribute('data-loaded') !== 'true')) {
            if (container.innerHTML.trim() === '' || !container.querySelector('.poll-card')) {
                loadPoll(pollId, container);
            } else {
                container.setAttribute('data-loaded', 'true');
            }
        }
    });
});

function encodeHtmlEntities(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}