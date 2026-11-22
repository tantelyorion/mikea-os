<?php
// ============================================================
// BARKER — Create Poll Page - VERSION COMPLÈTE CORRIGÉE
// ============================================================
require_once __DIR__ . '/../includes/init.php';
require_login();

$pageTitle = __('poll_create');
$currentUser = current_user();

include __DIR__ . '/../includes/header.php';
?>

<style>
.poll-type-selector {
    display: flex;
    gap: 12px;
    margin-bottom: 24px;
    flex-wrap: wrap;
}

.poll-type-card {
    flex: 1;
    min-width: 120px;
    padding: 16px;
    border: 2px solid var(--border);
    border-radius: 12px;
    background: var(--bg-card);
    cursor: pointer;
    text-align: center;
    transition: all 0.2s ease;
}

.poll-type-card:hover {
    border-color: var(--orange);
    transform: translateY(-2px);
}

.poll-type-card.selected {
    border-color: var(--orange);
    background: var(--orange-faint);
}

.poll-type-card i {
    font-size: 1.8rem;
    margin-bottom: 8px;
    display: block;
    color: var(--orange);
}

.poll-type-card h4 {
    margin: 8px 0 4px;
    font-size: 1rem;
}

.poll-type-card p {
    font-size: 0.75rem;
    color: var(--text-muted);
    margin: 0;
}

.options-container {
    margin: 16px 0;
}

.option-row {
    display: flex;
    gap: 8px;
    margin-bottom: 8px;
    align-items: center;
}

.option-row input[type="text"] {
    flex: 1;
}

.option-row .remove-option {
    background: transparent;
    border: none;
    color: var(--text-muted);
    cursor: pointer;
    padding: 8px;
    border-radius: 50%;
}

.option-row .remove-option:hover {
    background: var(--bg-card2);
    color: #FF3B30;
}

.add-option-btn {
    margin-top: 8px;
}

.rating-preview {
    display: flex;
    gap: 8px;
    justify-content: center;
    padding: 16px;
    background: var(--bg-card2);
    border-radius: 12px;
}

.rating-star {
    font-size: 2rem;
    cursor: pointer;
    color: var(--text-muted);
    transition: all 0.15s ease;
}

.rating-star:hover,
.rating-star.selected {
    color: #FFD700;
}

.duel-container {
    margin: 16px 0;
}

.duel-options-wrapper {
    display: flex;
    gap: 16px;
    justify-content: center;
    flex-wrap: wrap;
}

.duel-option-card {
    flex: 1;
    min-width: 250px;
    text-align: center;
    padding: 16px;
    background: var(--bg-card2);
    border-radius: 12px;
}

.duel-image-preview {
    min-height: 140px;
    cursor: pointer;
    margin-bottom: 12px;
}

.duel-image-preview .image-placeholder {
    background: var(--bg-card);
    border-radius: 12px;
    padding: 30px;
    text-align: center;
    border: 2px dashed var(--border);
    transition: all 0.2s ease;
}

.duel-image-preview .image-placeholder:hover {
    border-color: var(--orange);
    background: var(--orange-faint);
}

.duel-image-preview img {
    width: 100%;
    max-height: 140px;
    object-fit: cover;
    border-radius: 12px;
    border: 2px solid var(--orange);
}

.duel-option-card input[type="text"] {
    margin: 12px 0;
}

.image-upload-btn {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 6px 12px;
    background: var(--orange-faint);
    border-radius: 20px;
    cursor: pointer;
    font-size: 0.75rem;
    transition: all 0.2s;
    margin: 4px;
}

.image-upload-btn:hover {
    background: var(--orange);
    color: white;
}

.remove-image-btn {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 6px 12px;
    background: transparent;
    border: 1px solid #FF3B30;
    border-radius: 20px;
    cursor: pointer;
    font-size: 0.75rem;
    color: #FF3B30;
    transition: all 0.2s;
    margin: 4px;
}

.remove-image-btn:hover {
    background: #FF3B30;
    color: white;
}

.vs-badge {
    text-align: center;
    margin: -10px 0;
    position: relative;
    z-index: 1;
}

.vs-badge span {
    background: var(--orange);
    color: white;
    padding: 4px 16px;
    border-radius: 30px;
    font-weight: 700;
    font-size: 1.1rem;
}

.toggle-switch {
    position: relative;
    display: inline-block;
    width: 44px;
    height: 24px;
}

.toggle-switch input {
    opacity: 0;
    width: 0;
    height: 0;
}

.toggle-slider {
    position: absolute;
    cursor: pointer;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background-color: #ccc;
    transition: 0.3s;
    border-radius: 24px;
}

.toggle-slider:before {
    position: absolute;
    content: "";
    height: 18px;
    width: 18px;
    left: 3px;
    bottom: 3px;
    background-color: white;
    transition: 0.3s;
    border-radius: 50%;
}

input:checked + .toggle-slider {
    background-color: var(--orange);
}

input:checked + .toggle-slider:before {
    transform: translateX(20px);
}

.form-group {
    margin-bottom: 16px;
}

.form-label {
    display: block;
    margin-bottom: 8px;
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
}

.form-control:focus {
    outline: none;
    border-color: var(--orange);
}

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

.btn-sm {
    padding: 6px 12px;
    font-size: 0.8rem;
}

.compose-box {
    display: flex;
    gap: 12px;
    padding: 16px;
    background: var(--bg-card);
    border-radius: 16px;
    margin-bottom: 20px;
}

.compose-avatar {
    width: 44px;
    height: 44px;
    border-radius: 50%;
    object-fit: cover;
}

.compose-body {
    flex: 1;
}

.compose-textarea {
    width: 100%;
    padding: 12px;
    border: 1px solid var(--border);
    border-radius: 12px;
    background: var(--bg-card2);
    color: var(--text-primary);
    font-size: 0.95rem;
    resize: vertical;
    font-family: inherit;
}

.compose-textarea:focus {
    outline: none;
    border-color: var(--orange);
}

.text-muted {
    color: var(--text-muted);
    font-size: 0.8rem;
}

.rating-media-upload {
    margin: 16px 0;
    padding: 16px;
    background: var(--bg-card2);
    border-radius: 12px;
    text-align: center;
}

.rating-media-preview {
    margin-top: 12px;
    max-width: 100%;
    border-radius: 12px;
    overflow: hidden;
}

.rating-media-preview img,
.rating-media-preview video {
    max-width: 100%;
    max-height: 200px;
    object-fit: cover;
    border-radius: 8px;
}

.rating-media-label {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 10px 20px;
    background: var(--orange-faint);
    border-radius: 30px;
    cursor: pointer;
    font-size: 0.9rem;
    transition: all 0.2s;
}

.rating-media-label:hover {
    background: var(--orange);
    color: white;
}

.remove-media-btn {
    margin-top: 8px;
    background: transparent;
    border: none;
    color: #FF3B30;
    cursor: pointer;
    font-size: 0.8rem;
}
</style>

<main class="main-feed">
    <div class="feed-header">
        <h1 class="feed-header-title">📊 <?= __('poll_create') ?></h1>
        <p class="text-muted"><?= __('poll_create_description') ?></p>
    </div>

    <div class="compose-box">
        <img src="<?= get_user_avatar($currentUser) ?>" class="compose-avatar" alt="avatar">
        
        <div class="compose-body">
            <form id="pollForm" enctype="multipart/form-data">
                <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
                
                <!-- Bark content (optional) -->
                <textarea name="bark_content" id="barkContent" 
                    placeholder="<?= __('poll_bark_content_placeholder') ?>"
                    class="compose-textarea" maxlength="300" rows="2"
                    style="margin-bottom:16px"></textarea>
                
                <!-- Poll question -->
                <input type="text" name="question" id="pollQuestion" 
                    placeholder="<?= __('poll_question_placeholder') ?> *" 
                    class="form-control" maxlength="300" required
                    style="margin-bottom:16px; font-weight:600; font-size:1.1rem">
                
                <!-- Poll type selector -->
                <div class="poll-type-selector">
                    <div class="poll-type-card" data-type="single">
                        <i class="fa-solid fa-check-circle"></i>
                        <h4><?= __('poll_type_single') ?></h4>
                        <p><?= __('poll_type_single_desc') ?></p>
                    </div>
                    <div class="poll-type-card" data-type="multiple">
                        <i class="fa-solid fa-check-double"></i>
                        <h4><?= __('poll_type_multiple') ?></h4>
                        <p><?= __('poll_type_multiple_desc') ?></p>
                    </div>
                    <div class="poll-type-card" data-type="rating">
                        <i class="fa-solid fa-star"></i>
                        <h4><?= __('poll_type_rating') ?></h4>
                        <p><?= __('poll_type_rating_desc') ?></p>
                    </div>
                    <div class="poll-type-card" data-type="duel">
                        <i class="fa-solid fa-fist-raised"></i>
                        <h4><?= __('poll_type_duel') ?></h4>
                        <p><?= __('poll_type_duel_desc') ?></p>
                    </div>
                </div>
                
                <!-- Options container (dynamic) -->
                <div id="optionsContainer" class="options-container" style="display:none"></div>
                
                <!-- Duel container (dynamic) -->
                <div id="duelContainer" style="display:none"></div>
                
                <!-- Rating container (dynamic) -->
                <div id="ratingContainer" style="display:none">
                    <div class="rating-media-upload">
                        <label class="rating-media-label">
                            <i class="fa-solid fa-image"></i>
                            <?= __('poll_add_media_optional') ?>
                            <input type="file" name="rating_media" id="ratingMedia" accept="image/*,video/*" style="display:none" onchange="previewRatingMedia(this)">
                        </label>
                        <div id="ratingMediaPreview" class="rating-media-preview" style="display:none"></div>
                        <button type="button" class="remove-media-btn" id="removeRatingMedia" style="display:none" onclick="removeRatingMedia()">
                            <i class="fa-solid fa-trash"></i> <?= __('poll_remove_media') ?>
                        </button>
                    </div>
                    <div class="rating-preview">
                        <?php for ($i = 1; $i <= 5; $i++): ?>
                            <i class="fa-regular fa-star rating-star" data-value="<?= $i ?>"></i>
                        <?php endfor; ?>
                    </div>
                    <p class="text-muted" style="font-size:0.8rem; text-align:center; margin-top:8px">
                        <?= __('poll_rating_description') ?>
                    </p>
                </div>
                
                <!-- Poll settings -->
                <div style="margin: 16px 0; padding-top: 16px; border-top: 1px solid var(--border)">
                    <div class="form-group">
                        <label class="form-label"><i class="fa-solid fa-clock"></i> <?= __('poll_duration') ?></label>
                        <select name="expires_days" class="form-control">
                            <option value="0"><?= __('poll_no_expiration') ?></option>
                            <option value="1">1 <?= __('poll_day') ?></option>
                            <option value="3">3 <?= __('poll_days') ?></option>
                            <option value="7">7 <?= __('poll_days') ?></option>
                            <option value="14">14 <?= __('poll_days') ?></option>
                            <option value="30">30 <?= __('poll_days') ?></option>
                        </select>
                    </div>
                    
                    <div class="form-group">
                        <label style="display:flex; align-items:center; gap:12px; cursor:pointer">
                            <span class="toggle-switch">
                                <input type="checkbox" name="allow_anonymous_vote" value="1">
                                <span class="toggle-slider"></span>
                            </span>
                            <span><?= __('poll_allow_anonymous_vote') ?></span>
                        </label>
                    </div>
                    
                    <div class="form-group">
                        <label style="display:flex; align-items:center; gap:12px; cursor:pointer">
                            <span class="toggle-switch">
                                <input type="checkbox" name="is_anonymous" id="anonPollToggle">
                                <span class="toggle-slider"></span>
                            </span>
                            <span><i class="fa-solid fa-user-secret"></i> <?= __('poll_post_anonymously') ?></span>
                        </label>
                    </div>
                </div>
                
                <div style="display:flex; gap:12px; justify-content:flex-end">
                    <button type="button" class="btn btn-outline" onclick="history.back()"><?= __('modal_cancel') ?></button>
                    <button type="submit" class="btn btn-primary">
                        <i class="fa-solid fa-chart-simple"></i> <?= __('poll_create_button') ?>
                    </button>
                </div>
            </form>
        </div>
    </div>
</main>

<script>
let currentPollType = 'single';
let ratingMediaFile = null;
let duelImage1File = null;
let duelImage2File = null;

// ============================================================
// RATING MEDIA FUNCTIONS
// ============================================================
function previewRatingMedia(input) {
    const previewDiv = document.getElementById('ratingMediaPreview');
    const removeBtn = document.getElementById('removeRatingMedia');
    
    if (input.files && input.files[0]) {
        ratingMediaFile = input.files[0];
        const reader = new FileReader();
        
        reader.onload = function(e) {
            previewDiv.style.display = 'block';
            const fileType = ratingMediaFile.type;
            
            if (fileType.startsWith('image/')) {
                previewDiv.innerHTML = `<img src="${e.target.result}" alt="Preview" style="max-width:100%; max-height:200px; border-radius:8px">`;
            } else if (fileType.startsWith('video/')) {
                previewDiv.innerHTML = `<video src="${e.target.result}" controls style="max-width:100%; max-height:200px"></video>`;
            }
            
            removeBtn.style.display = 'inline-block';
        };
        
        reader.readAsDataURL(ratingMediaFile);
    }
}

function removeRatingMedia() {
    ratingMediaFile = null;
    document.getElementById('ratingMedia').value = '';
    document.getElementById('ratingMediaPreview').style.display = 'none';
    document.getElementById('ratingMediaPreview').innerHTML = '';
    document.getElementById('removeRatingMedia').style.display = 'none';
}

// ============================================================
// POLL TYPE SELECTOR
// ============================================================
document.querySelectorAll('.poll-type-card').forEach(card => {
    card.addEventListener('click', () => {
        document.querySelectorAll('.poll-type-card').forEach(c => c.classList.remove('selected'));
        card.classList.add('selected');
        currentPollType = card.dataset.type;
        
        document.getElementById('optionsContainer').style.display = 'none';
        document.getElementById('duelContainer').style.display = 'none';
        document.getElementById('ratingContainer').style.display = 'none';
        
        if (currentPollType === 'rating') {
            document.getElementById('ratingContainer').style.display = 'block';
            initRatingPreview();
        } else if (currentPollType === 'duel') {
            document.getElementById('duelContainer').style.display = 'block';
            initDuelOptions();
        } else {
            document.getElementById('optionsContainer').style.display = 'block';
            initOptions(2);
        }
    });
});

// ============================================================
// STANDARD OPTIONS (SINGLE/MULTIPLE)
// ============================================================
function initOptions(minOptions = 2) {
    const container = document.getElementById('optionsContainer');
    container.innerHTML = `
        <label class="form-label"><?= __('poll_options') ?></label>
        <div id="optionsList"></div>
        <button type="button" class="add-option-btn btn btn-outline btn-sm" onclick="addOption()">
            <i class="fa-solid fa-plus"></i> <?= __('poll_add_option') ?>
        </button>
    `;
    
    for (let i = 0; i < minOptions; i++) {
        addOption(i === minOptions - 1);
    }
}

function addOption(focus = true) {
    const list = document.getElementById('optionsList');
    const optionCount = list.children.length;
    const maxOptions = 10;
    
    if (optionCount >= maxOptions) {
        showToast(`<?= __('poll_max_options') ?> ${maxOptions}`, 'error');
        return;
    }
    
    const div = document.createElement('div');
    div.className = 'option-row';
    div.innerHTML = `
        <input type="text" name="options[]" class="form-control" placeholder="<?= __('poll_option_placeholder') ?> ${optionCount + 1}" maxlength="200">
        <button type="button" class="remove-option" onclick="this.closest('.option-row').remove()">
            <i class="fa-solid fa-xmark"></i>
        </button>
    `;
    list.appendChild(div);
    
    if (focus) {
        div.querySelector('input').focus();
    }
}

// ============================================================
// RATING POLL
// ============================================================
function initRatingPreview() {
    document.querySelectorAll('.rating-star').forEach(star => {
        star.addEventListener('mouseenter', function() {
            const val = parseInt(this.dataset.value);
            document.querySelectorAll('.rating-star').forEach((s, idx) => {
                if (idx < val) {
                    s.className = 'fa-solid fa-star rating-star';
                } else {
                    s.className = 'fa-regular fa-star rating-star';
                }
            });
        });
        
        star.addEventListener('mouseleave', function() {
            document.querySelectorAll('.rating-star').forEach(s => {
                s.className = 'fa-regular fa-star rating-star';
            });
        });
        
        star.addEventListener('click', function() {
            const val = parseInt(this.dataset.value);
            document.querySelectorAll('.rating-star').forEach((s, idx) => {
                if (idx < val) {
                    s.className = 'fa-solid fa-star rating-star selected';
                } else {
                    s.className = 'fa-regular fa-star rating-star';
                    s.classList.remove('selected');
                }
            });
        });
    });
}

// ============================================================
// DUEL POLL (VS) - VERSION CORRIGÉE AVEC IMAGES DISTINCTES
// ============================================================
function initDuelOptions() {
    const container = document.getElementById('duelContainer');
    container.innerHTML = `
        <div class="duel-container">
            <label class="form-label">⚔️ <?= __('poll_duel_options') ?></label>
            
            <div class="duel-options-wrapper">
                <!-- Option 1 -->
                <div class="duel-option-card">
                    <div class="duel-image-preview" id="duelImagePreview1" onclick="document.getElementById('duelImageInput1').click()">
                        <div class="image-placeholder">
                            <i class="fa-solid fa-image" style="font-size: 2rem; opacity: 0.5;"></i>
                            <p style="font-size: 0.7rem; margin-top: 8px;"><?= __('poll_click_add_image') ?></p>
                        </div>
                    </div>
                    <input type="file" id="duelImageInput1" name="option_image_1" accept="image/jpeg,image/png,image/gif,image/webp" style="display: none;" onchange="previewDuelImage(this, 1)">
                    <input type="text" id="duelOption1" class="form-control" placeholder="<?= __('poll_option1_placeholder') ?>" maxlength="200" required>
                    <div>
                        <button type="button" class="image-upload-btn" onclick="document.getElementById('duelImageInput1').click()">
                            <i class="fa-solid fa-camera"></i> <?= __('poll_add_image') ?>
                        </button>
                        <button type="button" id="removeImage1" class="remove-image-btn" style="display:none" onclick="removeDuelImage(1)">
                            <i class="fa-solid fa-trash"></i> <?= __('poll_remove') ?>
                        </button>
                    </div>
                </div>
                
                <!-- VS Badge -->
                <div class="vs-badge">
                    <span>VS</span>
                </div>
                
                <!-- Option 2 -->
                <div class="duel-option-card">
                    <div class="duel-image-preview" id="duelImagePreview2" onclick="document.getElementById('duelImageInput2').click()">
                        <div class="image-placeholder">
                            <i class="fa-solid fa-image" style="font-size: 2rem; opacity: 0.5;"></i>
                            <p style="font-size: 0.7rem; margin-top: 8px;"><?= __('poll_click_add_image') ?></p>
                        </div>
                    </div>
                    <input type="file" id="duelImageInput2" name="option_image_2" accept="image/jpeg,image/png,image/gif,image/webp" style="display: none;" onchange="previewDuelImage(this, 2)">
                    <input type="text" id="duelOption2" class="form-control" placeholder="<?= __('poll_option2_placeholder') ?>" maxlength="200" required>
                    <div>
                        <button type="button" class="image-upload-btn" onclick="document.getElementById('duelImageInput2').click()">
                            <i class="fa-solid fa-camera"></i> <?= __('poll_add_image') ?>
                        </button>
                        <button type="button" id="removeImage2" class="remove-image-btn" style="display:none" onclick="removeDuelImage(2)">
                            <i class="fa-solid fa-trash"></i> <?= __('poll_remove') ?>
                        </button>
                    </div>
                </div>
            </div>
            
            <p class="text-muted" style="font-size:0.75rem; text-align:center; margin-top:16px">
                ⚡ <?= __('poll_duel_exactly_two') ?>
            </p>
        </div>
    `;
    
    // Reset variables
    duelImage1File = null;
    duelImage2File = null;
}

function previewDuelImage(input, optionNum) {
    if (input.files && input.files[0]) {
        const file = input.files[0];
        
        // Validate file type
        const allowedTypes = ['image/jpeg', 'image/png', 'image/gif', 'image/webp'];
        if (!allowedTypes.includes(file.type)) {
            showToast('<?= __('error_invalid_image_format') ?>', 'error');
            input.value = '';
            return;
        }
        
        // Validate file size (max 5MB)
        if (file.size > 5 * 1024 * 1024) {
            showToast('<?= __('error_image_too_large') ?>', 'error');
            input.value = '';
            return;
        }
        
        const reader = new FileReader();
        
        reader.onload = function(e) {
            const previewDiv = document.getElementById(`duelImagePreview${optionNum}`);
            const removeBtn = document.getElementById(`removeImage${optionNum}`);
            
            previewDiv.innerHTML = `
                <img src="${e.target.result}" alt="Preview" style="width:100%; max-height:140px; object-fit:cover; border-radius:12px; border:2px solid var(--orange); cursor:pointer">
                <p style="font-size:0.7rem; margin-top:4px; color:var(--text-muted)"><?= __('poll_click_to_change') ?></p>
            `;
            
            if (removeBtn) removeBtn.style.display = 'inline-block';
            
            // Stocker le fichier pour l'envoi - DISTINCT pour chaque option
            if (optionNum === 1) {
                duelImage1File = file;
            } else {
                duelImage2File = file;
            }
        };
        
        reader.readAsDataURL(file);
    }
}

function removeDuelImage(optionNum) {
    if (optionNum === 1) {
        duelImage1File = null;
        document.getElementById('duelImageInput1').value = '';
        document.getElementById('duelImagePreview1').innerHTML = `
            <div class="image-placeholder" onclick="document.getElementById('duelImageInput1').click()">
                <i class="fa-solid fa-image" style="font-size: 2rem; opacity: 0.5;"></i>
                <p style="font-size: 0.7rem; margin-top: 8px;"><?= __('poll_click_add_image') ?></p>
            </div>
        `;
        document.getElementById('removeImage1').style.display = 'none';
    } else {
        duelImage2File = null;
        document.getElementById('duelImageInput2').value = '';
        document.getElementById('duelImagePreview2').innerHTML = `
            <div class="image-placeholder" onclick="document.getElementById('duelImageInput2').click()">
                <i class="fa-solid fa-image" style="font-size: 2rem; opacity: 0.5;"></i>
                <p style="font-size: 0.7rem; margin-top: 8px;"><?= __('poll_click_add_image') ?></p>
            </div>
        `;
        document.getElementById('removeImage2').style.display = 'none';
    }
}

// ============================================================
// FORM SUBMISSION - VERSION CORRIGÉE
// ============================================================
document.getElementById('pollForm').addEventListener('submit', async function(e) {
    e.preventDefault();
    
    const question = document.getElementById('pollQuestion').value.trim();
    if (!question) {
        showToast('<?= __('poll_question_required') ?>', 'error');
        return;
    }
    
    const formData = new FormData();
    formData.append('csrf_token', getCsrfToken());
    formData.append('poll_type', currentPollType);
    formData.append('question', question);
    formData.append('bark_content', document.getElementById('barkContent').value);
    formData.append('expires_days', document.querySelector('[name="expires_days"]').value);
    formData.append('allow_anonymous_vote', document.querySelector('[name="allow_anonymous_vote"]')?.checked ? '1' : '0');
    formData.append('is_anonymous', document.getElementById('anonPollToggle')?.checked ? 'on' : 'off');
    
    // ============================================================
    // DUEL POLL - VERSION CORRIGÉE AVEC IMAGES DISTINCTES
    // ============================================================
    if (currentPollType === 'duel') {
        const option1 = document.getElementById('duelOption1').value.trim();
        const option2 = document.getElementById('duelOption2').value.trim();
        
        if (!option1 || !option2) {
            showToast('<?= __('poll_duel_both_required') ?>', 'error');
            return;
        }
        
        formData.append('options[]', option1);
        formData.append('options[]', option2);
        
        // Ajouter les images avec des noms de champs DISTINCTS
        // option_image_1 pour la première option
        if (duelImage1File) {
            formData.append('option_image_1', duelImage1File);
        }
        
        // option_image_2 pour la deuxième option
        if (duelImage2File) {
            formData.append('option_image_2', duelImage2File);
        }
        
    // ============================================================
    // RATING POLL
    // ============================================================
    } else if (currentPollType === 'rating') {
        if (ratingMediaFile) {
            formData.append('rating_media', ratingMediaFile);
        }
        
    // ============================================================
    // STANDARD POLLS (SINGLE/MULTIPLE)
    // ============================================================
    } else {
        const optionInputs = document.querySelectorAll('#optionsList input[name="options[]"]');
        const options = [];
        optionInputs.forEach(input => {
            if (input.value.trim()) {
                options.push(input.value.trim());
            }
        });
        
        if (options.length < 2) {
            showToast('<?= __('poll_min_two_options') ?>', 'error');
            return;
        }
        
        options.forEach(opt => {
            formData.append('options[]', opt);
        });
    }
    
    const submitBtn = this.querySelector('button[type="submit"]');
    const originalText = submitBtn.innerHTML;
    submitBtn.disabled = true;
    submitBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> <?= __('poll_creating') ?>...';
    
    try {
        const res = await fetch(`${SITE_URL}/api/poll.php?action=create`, {
            method: 'POST',
            body: formData
        });
        
        const text = await res.text();
        
        let data;
        try {
            data = JSON.parse(text);
        } catch (e) {
            console.error('JSON parse error:', e, text);
            showToast('<?= __('error_server') ?>: ' + text.substring(0, 100), 'error');
            submitBtn.disabled = false;
            submitBtn.innerHTML = originalText;
            return;
        }
        
        if (data.success) {
            showToast('<?= __('poll_created_success') ?>', 'success');
            setTimeout(() => {
                window.location.href = `${SITE_URL}/pages/bark.php?id=${data.bark_id}`;
            }, 1000);
        } else {
            showToast(data.error || '<?= __('poll_create_error') ?>', 'error');
            submitBtn.disabled = false;
            submitBtn.innerHTML = originalText;
        }
    } catch (error) {
        console.error('Poll creation error:', error);
        showToast('<?= __('error_network') ?>', 'error');
        submitBtn.disabled = false;
        submitBtn.innerHTML = originalText;
    }
});

// Initialize default poll type
document.querySelector('.poll-type-card[data-type="single"]').click();
</script>

<?php include __DIR__ . '/../includes/footer.php'; ?>