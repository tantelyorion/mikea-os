<?php
// ============================================================
// BARKER — Footer Template (Version améliorée)
// ============================================================
$currentUser = current_user();
?>

<?php if (is_logged_in()): ?>
</div><!-- /.app-layout -->

<!-- ── Global: New Bark Modal ─────────────────────────────── -->
<div class="modal-overlay" id="barkModal">
    <div class="modal" style="max-width:560px">
        <div class="modal-header">
            <span class="modal-title">New Bark</span>
            <button class="modal-close" onclick="closeBarkModal()"><i class="fa-solid fa-xmark"></i></button>
        </div>
        <form id="barkForm" enctype="multipart/form-data">
            <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
            <div style="display:flex; gap:12px; align-items:flex-start">
                <?php if ($currentUser): ?>
                <img src="<?= get_user_avatar($currentUser) ?>" style="width:44px;height:44px;border-radius:50%;object-fit:cover;border:2px solid var(--border);flex-shrink:0" alt="">
                <?php endif; ?>
                <div style="flex:1">
                    <textarea name="content" id="barkTextarea"
                        placeholder="What are you barking about?"
                        class="compose-textarea" maxlength="300"
                        style="width:100%;min-height:100px"></textarea>
                    <div class="media-preview" id="mediaPreviewModal"></div>
                </div>
            </div>

            <div style="border-top:1px solid var(--border);margin-top:12px;padding-top:12px;display:flex;align-items:center;gap:10px;flex-wrap:wrap">
                <label class="compose-opt-btn" title="Add image/video" style="cursor:pointer">
                    <i class="fa-solid fa-image"></i>
                    <input type="file" name="media[]" id="mediaInput" accept="image/*,video/*"
                           multiple style="display:none" onchange="previewMedia(this)">
                </label>

                <label class="compose-anon-toggle" title="Post anonymously">
                    <label class="toggle-switch">
                        <input type="checkbox" name="is_anonymous" id="anonToggle">
                        <span class="toggle-slider"></span>
                    </label>
                    <i class="fa-solid fa-user-secret"></i> Anonymous
                </label>

                <span class="compose-char-count" id="charCount">300</span>

                <button type="submit" class="compose-submit" style="margin-left:auto"><i class="fa-solid fa-paw"></i> Bark</button>
            </div>
        </form>
    </div>
</div>

<!-- ── Global: Search Modal ───────────────────────────────── -->
<div class="modal-overlay" id="searchModal">
    <div class="modal" style="max-width:600px">
        <div class="modal-header">
            <span class="modal-title">🔍 Search BARKER</span>
            <button class="modal-close" onclick="closeSearchModal()">
                <i class="fa-solid fa-xmark"></i>
            </button>
        </div>
        
        <div style="padding:16px 0">
            <!-- Search input -->
            <div class="search-box" style="margin-bottom:16px">
                <i class="fa-solid fa-magnifying-glass"></i>
                <input type="text" class="search-input" id="modalSearchInput" 
                       placeholder="Search barks, users, #hashtags..." autocomplete="off" autofocus>
            </div>
            
            <!-- Search filters -->
            <div class="search-filters">
                <button class="search-filter-btn active" data-filter="all" onclick="setSearchFilter('all')">
                    <i class="fa-solid fa-globe"></i> All
                </button>
                <button class="search-filter-btn" data-filter="users" onclick="setSearchFilter('users')">
                    <i class="fa-solid fa-user"></i> Users
                </button>
                <button class="search-filter-btn" data-filter="barks" onclick="setSearchFilter('barks')">
                    <i class="fa-solid fa-comment"></i> Barks
                </button>
            </div>
            
            <!-- Search results container -->
            <div id="searchResults" class="search-results-container">
                <div class="search-placeholder">
                    <i class="fa-solid fa-search"></i>
                    <p>Start typing to search...</p>
                    <span class="search-shortcut">Press <kbd>/</kbd> to focus</span>
                </div>
            </div>
            
            <!-- View all results link -->
            <div id="viewAllResults" class="view-all-results" style="display:none">
                <a href="#" id="viewAllLink" class="btn btn-outline btn-full">
                    View all results <i class="fa-solid fa-arrow-right"></i>
                </a>
            </div>
        </div>
    </div>
</div>

<!-- ── Global: My Apps Modal ──────────────────────────────── -->
<div class="modal-overlay" id="myAppsModal">
    <div class="modal" style="max-width:420px">
        <div class="modal-header">
            <span class="modal-title"><i class="fa-solid fa-grip"></i> My App</span>
            <button class="modal-close" onclick="document.getElementById('myAppsModal').classList.remove('active')"><i class="fa-solid fa-xmark"></i></button>
        </div>

        <div class="my-apps-grid">
            <a href="<?= !empty($currentUser['has_barker_mail']) ? MAIL_WEBMAIL_URL : SITE_URL . '/pages/barker-mail.php' ?>" class="my-apps-item" target="_blank" rel="noopener">
                <img src="<?= SITE_URL ?>/assets/icons/barkermail.png" alt="Barker Mail" class="my-apps-icon">
                <span class="my-apps-name">Barker Mail</span>
            </a>
            <a href="https://hafatra.xyz" class="my-apps-item" target="_blank" rel="noopener">
                <img src="<?= SITE_URL ?>/assets/icons/hafatra.png" alt="Hafatra" class="my-apps-icon">
                <span class="my-apps-name">Hafatra</span>
            </a>
            <a href="https://enigram.xyz" class="my-apps-item" target="_blank" rel="noopener">
                <img src="<?= SITE_URL ?>/assets/icons/enigram.png" alt="Enigram" class="my-apps-icon">
                <span class="my-apps-name">Enigram</span>
            </a>
            <a href="https://tsmf.hafatra.xyz" class="my-apps-item" target="_blank" rel="noopener">
                <img src="<?= SITE_URL ?>/assets/icons/tsmf.png" alt="TSMF" class="my-apps-icon">
                <span class="my-apps-name">TSMF</span>
            </a>
            <a href="https://akory.enigram.xyz" class="my-apps-item" target="_blank" rel="noopener">
                <img src="<?= SITE_URL ?>/assets/icons/akory.png" alt="Akory" class="my-apps-icon">
                <span class="my-apps-name">Akory</span>
            </a>
            <a href="https://vibezone.cfd" class="my-apps-item" target="_blank" rel="noopener">
                <img src="<?= SITE_URL ?>/assets/icons/vibezone.png" alt="Vibezone" class="my-apps-icon">
                <span class="my-apps-name">Vibezone</span>
            </a>
        </div>
    </div>
</div>

<!-- ── Global: Bone (Donation) Modal ──────────────────────── -->
<div class="modal-overlay" id="boneModal">
    <div class="modal" style="max-width:420px">
        <div class="modal-header">
            <span class="modal-title"><i class="fa-solid fa-bone"></i> Send a Bone</span>
            <button class="modal-close" onclick="closeBoneModal()"><i class="fa-solid fa-xmark"></i></button>
        </div>
        <p style="color:var(--text-secondary);font-size:0.9rem;margin-bottom:16px">
            Support <strong id="boneReceiverName">this creator</strong> directly 💰
        </p>

        <div class="bone-amounts">
            <?php foreach ([1, 2, 5, 10, 20, 50] as $amt): ?>
            <button class="bone-amount-btn" onclick="selectBoneAmount(<?= $amt ?>, this)">
                €<?= $amt ?>
            </button>
            <?php endforeach; ?>
        </div>

        <div class="form-group">
            <label class="form-label">Custom amount (€)</label>
            <input type="number" class="form-control" id="customBoneAmount"
                   min="0.50" max="500" step="0.50" placeholder="e.g. 3.50"
                   oninput="clearBonePreset()">
        </div>

        <div class="form-group">
            <label class="form-label">Message (optional)</label>
            <input type="text" class="form-control" id="boneMessage"
                   placeholder="Keep it up!" maxlength="200">
        </div>

        <input type="hidden" id="boneBarkId">
        <input type="hidden" id="boneReceiverId">

        <button class="btn btn-primary btn-full" onclick="sendBone()">
            <i class="fa-solid fa-bone"></i> Send Bone
        </button>
    </div>
</div>

<!-- ── Global: Report Modal ───────────────────────────────── -->
<div class="modal-overlay" id="reportModal">
    <div class="modal" style="max-width:400px">
        <div class="modal-header">
            <span class="modal-title">🚨 Report</span>
            <button class="modal-close" onclick="closeReportModal()">
                <i class="fa-solid fa-xmark"></i>
            </button>
        </div>
        <div class="form-group">
            <label class="form-label">Reason</label>
            <select class="form-control" id="reportReason">
                <option value="spam">Spam</option>
                <option value="harassment">Harassment</option>
                <option value="hate">Hate speech</option>
                <option value="misinformation">Misinformation</option>
                <option value="other">Other</option>
            </select>
        </div>
        <div class="form-group">
            <label class="form-label">Details (optional)</label>
            <textarea class="form-control" id="reportDetails" rows="3" placeholder="Describe the issue..."></textarea>
        </div>
        <input type="hidden" id="reportBarkId">
        <button class="btn btn-danger btn-full" onclick="submitReport()">
            <i class="fa-solid fa-flag"></i> Submit Report
        </button>
    </div>
</div>

<!-- ── Global: Yap (Reply) Modal ──────────────────────────── -->
<div class="modal-overlay" id="yapModal">
    <div class="modal" style="max-width:500px">
        <div class="modal-header">
            <span class="modal-title"><i class="fa-solid fa-comment"></i> Yap — Reply</span>
            <button class="modal-close" onclick="closeYapModal()">
                <i class="fa-solid fa-xmark"></i>
            </button>
        </div>
        <div id="yapOriginalBark" class="quoted-bark-preview"></div>
        <input type="hidden" id="yapBarkId">
        <textarea class="form-control" id="yapContent" rows="3" placeholder="Your yap..." maxlength="300"
            style="margin-bottom:12px"></textarea>
        <div style="display:flex;align-items:center;gap:12px">
            <label class="compose-anon-toggle">
                <label class="toggle-switch">
                    <input type="checkbox" id="yapAnon">
                    <span class="toggle-slider"></span>
                </label>
                <i class="fa-solid fa-user-secret"></i> Anonymous
            </label>
            <button class="btn btn-primary" style="margin-left:auto" onclick="submitYap()">
                <i class="fa-solid fa-comment"></i> Yap!
            </button>
        </div>
    </div>
</div>

<!-- ── Global: Howl (Quote) Modal ─────────────────────────── -->
<div class="modal-overlay" id="howlModal">
    <div class="modal" style="max-width:500px">
        <div class="modal-header">
            <span class="modal-title"><i class="fa-solid fa-wifi sound"></i> Howl — Quote</span>
            <button class="modal-close" onclick="closeHowlModal()">
                <i class="fa-solid fa-xmark"></i>
            </button>
        </div>
        <textarea class="form-control" id="howlContent" rows="3"
            placeholder="Add your howl..." maxlength="300" style="margin-bottom:12px"></textarea>
        <div id="howlQuotedBark" class="quoted-bark-preview"></div>
        <input type="hidden" id="howlBarkId">
        <div style="display:flex;align-items:center;gap:12px">
            <label class="compose-anon-toggle">
                <label class="toggle-switch">
                    <input type="checkbox" id="howlAnon">
                    <span class="toggle-slider"></span>
                </label>
                <i class="fa-solid fa-user-secret"></i> Anonymous
            </label>
            <button class="btn btn-primary" style="margin-left:auto" onclick="submitHowl()">
                <i class="fa-brands fa-wolf-pack-battalion"></i> Howl!
            </button>
        </div>
    </div>
</div>

<!-- ── Global: Edit Bark Modal ────────────────────────────── -->
<div class="modal-overlay" id="editBarkModal">
    <div class="modal" style="max-width:560px">
        <div class="modal-header">
            <span class="modal-title">✏️ Edit Bark</span>
            <button class="modal-close" onclick="closeEditBarkModal()">
                <i class="fa-solid fa-xmark"></i>
            </button>
        </div>
        
        <input type="hidden" id="editBarkId">
        
        <div style="margin-bottom:16px">
            <textarea id="editBarkContent" class="compose-textarea" 
                placeholder="Edit your bark..." maxlength="300"
                style="width:100%; min-height:120px" 
                oninput="updateEditCharCount()"></textarea>
            <div style="display:flex; justify-content:space-between; margin-top:8px">
                <span class="compose-char-count" id="editCharCount">300</span>
            </div>
        </div>
        
        <div style="display:flex; gap:12px; justify-content:flex-end">
            <button class="btn btn-outline" onclick="closeEditBarkModal()">Cancel</button>
            <button class="btn btn-primary" onclick="submitEditBark()">
                <i class="fa-solid fa-check"></i> Save Changes
            </button>
        </div>
    </div>
</div>


<?php endif; ?>

<!-- ── Toast Container ─────────────────────────────────────── -->
<div id="toastContainer"></div>

<!-- ── Scripts ─────────────────────────────────────────────── -->
<script>
// Définir SITE_URL globalement
const SITE_URL = '<?= SITE_URL ?>';
console.log('BARKER - SITE_URL:', SITE_URL);
</script>
<script src="<?= SITE_URL ?>/js/app.js"></script>
<?php if (isset($extraJs)) echo $extraJs; ?>

<!-- ── Styles additionnels ─────────────────────────────────── -->
<style>
/* === Search Modal Styles === */
.search-filters {
    display: flex;
    gap: 8px;
    margin-bottom: 16px;
    border-bottom: 1px solid var(--border);
    padding-bottom: 12px;
}

.search-filter-btn {
    padding: 8px 16px;
    border-radius: 20px;
    border: 1px solid var(--border);
    background: transparent;
    color: var(--text-secondary);
    cursor: pointer;
    font-size: 0.85rem;
    font-weight: 500;
    transition: all 0.15s ease;
}

.search-filter-btn:hover {
    background: var(--bg-card2);
}

.search-filter-btn.active {
    background: var(--orange);
    color: white;
    border-color: var(--orange);
}

.search-results-container {
    max-height: 400px;
    overflow-y: auto;
    min-height: 200px;
}

.search-placeholder {
    text-align: center;
    padding: 40px;
    color: var(--text-muted);
}

.search-placeholder i {
    font-size: 2.5rem;
    margin-bottom: 12px;
    opacity: 0.5;
}

.search-placeholder p {
    margin-bottom: 8px;
}

.search-shortcut {
    font-size: 0.8rem;
    opacity: 0.7;
}

.search-shortcut kbd {
    background: var(--bg-card2);
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 2px 6px;
    font-family: monospace;
}

.search-result-item {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px;
    border-radius: 8px;
    cursor: pointer;
    transition: background 0.15s ease;
    text-decoration: none;
    color: var(--text-primary);
    border-bottom: 1px solid var(--border);
}

.search-result-item:last-child {
    border-bottom: none;
}

.search-result-item:hover {
    background: var(--bg-card2);
}

.search-result-avatar {
    width: 44px;
    height: 44px;
    border-radius: 50%;
    object-fit: cover;
    border: 2px solid var(--border);
    flex-shrink: 0;
}

.search-result-info {
    flex: 1;
    min-width: 0;
}

.search-result-name {
    font-weight: 700;
    display: flex;
    align-items: center;
    gap: 4px;
}

.search-result-handle {
    font-size: 0.8rem;
    color: var(--text-muted);
}

.search-result-bark {
    padding: 12px;
    border-radius: 8px;
    cursor: pointer;
    transition: background 0.15s ease;
    border-bottom: 1px solid var(--border);
}

.search-result-bark:hover {
    background: var(--bg-card2);
}

.search-highlight {
    background: var(--orange-faint);
    color: var(--orange);
    padding: 2px 4px;
    border-radius: 4px;
    font-weight: 600;
}

.view-all-results {
    margin-top: 16px;
    padding-top: 12px;
    border-top: 1px solid var(--border);
}

/* === Media Preview Styles === */
.media-preview {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 8px;
    margin-top: 12px;
}

.media-preview-item {
    position: relative;
    border-radius: 12px;
    overflow: hidden;
    background: var(--bg-card2);
    aspect-ratio: 1;
}

.media-preview-item img,
.media-preview-item video {
    width: 100%;
    height: 100%;
    object-fit: cover;
}

.media-preview-remove {
    position: absolute;
    top: 4px;
    right: 4px;
    width: 24px;
    height: 24px;
    border-radius: 50%;
    background: rgba(0,0,0,0.7);
    color: white;
    border: none;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 12px;
}

.media-preview-remove:hover {
    background: rgba(255,59,48,0.9);
}

/* === Quoted Bark Preview === */
.quoted-bark-preview {
    margin-bottom: 16px;
    padding: 12px;
    background: var(--bg-card2);
    border-radius: 12px;
    border: 1px solid var(--border);
    font-size: 0.9rem;
    max-height: 100px;
    overflow-y: auto;
}

/* === Toast Container === */
#toastContainer {
    position: fixed;
    bottom: 24px;
    right: 24px;
    z-index: 9999;
    display: flex;
    flex-direction: column;
    gap: 8px;
}

/* === Modal Improvements === */
.modal-overlay {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: rgba(0, 0, 0, 0.7);
    backdrop-filter: blur(4px);
    z-index: 1000;
    display: none;
    align-items: center;
    justify-content: center;
    animation: fadeIn 0.2s ease;
}

.modal-overlay.active {
    display: flex;
}

.modal {
    background: var(--bg-card);
    border-radius: 16px;
    padding: 20px;
    width: 90%;
    max-width: 500px;
    max-height: 90vh;
    overflow-y: auto;
    box-shadow: 0 10px 40px rgba(0, 0, 0, 0.3);
    animation: slideUp 0.3s ease;
}

@keyframes fadeIn {
    from { opacity: 0; }
    to { opacity: 1; }
}

@keyframes slideUp {
    from {
        opacity: 0;
        transform: translateY(20px);
    }
    to {
        opacity: 1;
        transform: translateY(0);
    }
}

.modal-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 16px;
    padding-bottom: 12px;
    border-bottom: 1px solid var(--border);
}

.modal-title {
    font-size: 1.2rem;
    font-weight: 700;
    color: var(--text-primary);
}

.modal-close {
    background: transparent;
    border: none;
    color: var(--text-muted);
    cursor: pointer;
    font-size: 1.2rem;
    padding: 4px 8px;
    border-radius: 50%;
    transition: all 0.15s ease;
}

.modal-close:hover {
    background: var(--bg-card2);
    color: var(--text-primary);
}

/* === Form Elements === */
.form-group {
    margin-bottom: 16px;
}

.form-label {
    display: block;
    margin-bottom: 6px;
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--text-secondary);
}

.form-control {
    width: 100%;
    padding: 10px 12px;
    border: 1px solid var(--border);
    border-radius: 8px;
    background: var(--bg-card2);
    color: var(--text-primary);
    font-size: 0.9rem;
    transition: border-color 0.15s ease;
}

.form-control:focus {
    outline: none;
    border-color: var(--orange);
}

/* === Buttons === */
.btn {
    padding: 10px 16px;
    border-radius: 20px;
    border: none;
    cursor: pointer;
    font-size: 0.9rem;
    font-weight: 600;
    transition: all 0.15s ease;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
}

.btn-primary {
    background: var(--orange);
    color: white;
}

.btn-primary:hover {
    opacity: 0.9;
    transform: scale(1.02);
}

.btn-outline {
    background: transparent;
    border: 1px solid var(--border);
    color: var(--text-primary);
}

.btn-outline:hover {
    background: var(--bg-card2);
}

.btn-danger {
    background: #FF3B30;
    color: white;
}

.btn-danger:hover {
    background: #e0352b;
}

.btn-full {
    width: 100%;
}

.btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
}

/* === Bone Amount Buttons === */
.bone-amounts {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 8px;
    margin-bottom: 16px;
}

.bone-amount-btn {
    padding: 10px;
    border: 1px solid var(--border);
    border-radius: 8px;
    background: var(--bg-card2);
    color: var(--text-primary);
    cursor: pointer;
    font-weight: 600;
    transition: all 0.15s ease;
}

.bone-amount-btn:hover {
    background: var(--orange-faint);
    border-color: var(--orange);
}

.bone-amount-btn.selected {
    background: var(--orange);
    color: white;
    border-color: var(--orange);
}

/* === Compose Options === */
.compose-opt-btn {
    background: transparent;
    border: none;
    cursor: pointer;
    color: var(--orange);
    font-size: 1.1rem;
    padding: 8px;
    border-radius: 50%;
    transition: all 0.2s;
    display: inline-flex;
    align-items: center;
    justify-content: center;
}

.compose-opt-btn:hover {
    background: var(--orange-faint);
}

/* === Scrollbar Customization === */
.search-results-container::-webkit-scrollbar,
.modal::-webkit-scrollbar {
    width: 6px;
}

.search-results-container::-webkit-scrollbar-track,
.modal::-webkit-scrollbar-track {
    background: transparent;
}

.search-results-container::-webkit-scrollbar-thumb,
.modal::-webkit-scrollbar-thumb {
    background: var(--border);
    border-radius: 3px;
}

.search-results-container::-webkit-scrollbar-thumb:hover,
.modal::-webkit-scrollbar-thumb:hover {
    background: var(--text-muted);
}

/* === Loading Spinner === */
.fa-spinner {
    animation: spin 1s linear infinite;
}

@keyframes spin {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
}

/* === Responsive === */
@media (max-width: 768px) {
    .modal {
        width: 95%;
        padding: 16px;
    }
    
    .bone-amounts {
        grid-template-columns: repeat(2, 1fr);
    }
    
    .search-filters {
        flex-wrap: wrap;
    }
    
    .media-preview {
        grid-template-columns: repeat(2, 1fr);
    }
}
</style>

</body>
</html>