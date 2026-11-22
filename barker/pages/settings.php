<?php
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$currentUser = current_user();
$success = '';
$error   = '';
$tab     = $_GET['tab'] ?? 'profile';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        $error = 'Invalid CSRF token.';
    } else {
        $action = $_POST['action'] ?? '';

        if ($action === 'update_profile') {
            $display_name = trim($_POST['display_name'] ?? '');
            $bio          = trim($_POST['bio'] ?? '');
            $gender       = $_POST['gender'] ?? $currentUser['gender'];

            if (empty($display_name)) {
                $error = 'Display name is required.';
            } else {
                // Handle avatar upload — vrai type détecté côté serveur,
                // nom aléatoire, puis compression automatique.
                $avatarFile = null;
                if (!empty($_FILES['avatar']['name'])) {
                    $uploadError = null;
                    $result = barker_store_image($_FILES['avatar']['tmp_name'], __DIR__ . '/../assets/uploads/avatars/', 'avatar_' . $currentUser['id'], 5 * 1024 * 1024, $uploadError);
                    if ($result) {
                        $avatarFile = $result['filename'];
                    } else {
                        $error = 'Avatar invalide (jpg, png, gif, webp — 5 Mo max).';
                    }
                }

                // Handle cover upload
                $coverFile = null;
                if (!empty($_FILES['cover']['name']) && !$error) {
                    $uploadError = null;
                    $result = barker_store_image($_FILES['cover']['tmp_name'], __DIR__ . '/../assets/uploads/covers/', 'cover_' . $currentUser['id'], 10 * 1024 * 1024, $uploadError);
                    if ($result) {
                        $coverFile = $result['filename'];
                    } else {
                        $error = 'Image de couverture invalide (jpg, png, gif, webp — 10 Mo max).';
                    }
                }

                if (!$error) {
                    $sql = "UPDATE users SET display_name=?, bio=?, gender=?";
                    $params = [$display_name, $bio, $gender];
                    if ($avatarFile) { $sql .= ", avatar=?"; $params[] = $avatarFile; }
                    if ($coverFile)  { $sql .= ", cover_photo=?"; $params[] = $coverFile; }
                    $sql .= " WHERE id=?";
                    $params[] = $currentUser['id'];
                    DB::execute($sql, $params);
                    $success = __('success_profile_updated');
                    $currentUser = current_user(); // refresh
                }
            }
        }

        if ($action === 'change_password') {
            $old = $_POST['old_password'] ?? '';
            $new = $_POST['new_password'] ?? '';
            $confirm = $_POST['confirm_password'] ?? '';

            if (!verify_password($old, $currentUser['password'])) {
                $error = 'Current password is incorrect.';
            } elseif (strlen($new) < 8) {
                $error = 'New password must be at least 8 characters.';
            } elseif ($new !== $confirm) {
                $error = 'Passwords do not match.';
            } else {
                DB::execute(
                    "UPDATE users SET password=? WHERE id=?",
                    [hash_password($new), $currentUser['id']]
                );
                log_security($currentUser['id'], 'password_changed', '');
                $success = __('success_password_changed');
            }
        }

        if ($action === 'update_language') {
            $newLang = $_POST['language'] ?? '';
            if (set_language($newLang)) {
                $success = __('success_language_updated');
                $currentUser = current_user(); // refresh
                // Recharger la page pour appliquer la nouvelle langue
                echo '<meta http-equiv="refresh" content="1;url=' . SITE_URL . '/pages/settings.php?tab=language">';
            } else {
                $error = 'Invalid language selection.';
            }
        }
    }
}

$pageTitle = __('settings_title');
include __DIR__ . '/../includes/header.php';
?>

<main class="main-feed">
    <div class="feed-header">
        <h1 class="feed-header-title">⚙️ <?= __('settings_title') ?></h1>
    </div>

    <!-- Tabs -->
    <div class="feed-tabs">
        <a href="?tab=profile" class="feed-tab <?= $tab==='profile'?'active':'' ?>" style="text-decoration:none">
            <i class="fa-solid fa-user"></i> <?= __('settings_profile') ?>
        </a>
        <a href="?tab=security" class="feed-tab <?= $tab==='security'?'active':'' ?>" style="text-decoration:none">
            <i class="fa-solid fa-lock"></i> <?= __('settings_security') ?>
        </a>
        <a href="?tab=language" class="feed-tab <?= $tab==='language'?'active':'' ?>" style="text-decoration:none">
            <i class="fa-solid fa-globe"></i> <?= __('settings_language') ?>
        </a>
        <a href="?tab=account" class="feed-tab <?= $tab==='account'?'active':'' ?>" style="text-decoration:none">
            <i class="fa-solid fa-trash"></i> <?= __('settings_account') ?>
        </a>
    </div>

    <div style="padding:24px">
        <?php if ($success): ?>
            <div class="alert alert-success"><i class="fa-solid fa-check"></i> <?= sanitize($success) ?></div>
        <?php endif; ?>
        <?php if ($error): ?>
            <div class="alert alert-error"><i class="fa-solid fa-triangle-exclamation"></i> <?= sanitize($error) ?></div>
        <?php endif; ?>

        <?php if ($tab === 'profile'): ?>
        <!-- Profile Settings -->
        <form method="POST" enctype="multipart/form-data">
            <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
            <input type="hidden" name="action" value="update_profile">

            <!-- Cover photo -->
            <div class="form-group">
                <label class="form-label"><?= __('settings_cover_photo') ?></label>
                <div style="position:relative;border-radius:12px;overflow:hidden;height:140px;background:var(--bg-card2);margin-bottom:8px">
                    <img src="<?= get_user_cover($currentUser) ?>" id="coverPreview"
                         style="width:100%;height:100%;object-fit:cover" alt="Cover">
                    <label style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:rgba(0,0,0,0.4);cursor:pointer;opacity:0;transition:opacity 0.2s"
                           onmouseover="this.style.opacity=1" onmouseout="this.style.opacity=0">
                        <span style="color:#fff;font-weight:700"><i class="fa-solid fa-camera"></i> <?= __('settings_cover_photo') ?></span>
                        <input type="file" name="cover" accept="image/*" style="display:none"
                               onchange="previewFile(this,'coverPreview')">
                    </label>
                </div>
            </div>

            <!-- Avatar -->
            <div class="form-group">
                <label class="form-label"><?= __('settings_avatar') ?></label>
                <div style="display:flex;align-items:center;gap:16px">
                    <div style="position:relative">
                        <img src="<?= get_user_avatar($currentUser) ?>" id="avatarPreview"
                             style="width:80px;height:80px;border-radius:50%;object-fit:cover;border:3px solid var(--orange)">
                        <label style="position:absolute;inset:0;border-radius:50%;display:flex;align-items:center;justify-content:center;background:rgba(0,0,0,0.5);cursor:pointer;opacity:0;transition:opacity 0.2s"
                               onmouseover="this.style.opacity=1" onmouseout="this.style.opacity=0">
                            <i class="fa-solid fa-camera" style="color:#fff"></i>
                            <input type="file" name="avatar" accept="image/*" style="display:none"
                                   onchange="previewFile(this,'avatarPreview')">
                        </label>
                    </div>
                    <div style="font-size:0.85rem;color:var(--text-muted)">
                        Click to change your avatar<br>Max 5MB · JPG, PNG, GIF, WEBP
                    </div>
                </div>
            </div>

            <div class="form-group">
                <label class="form-label"><?= __('settings_display_name') ?> *</label>
                <input type="text" name="display_name" class="form-control"
                    value="<?= htmlspecialchars($currentUser['display_name']) ?>" maxlength="100" required>
            </div>

            <div class="form-group">
                <label class="form-label"><?= __('auth_bio') ?></label>
                <textarea name="bio" class="form-control" rows="3" maxlength="300"
                    placeholder="<?= __('placeholder_bio') ?>"><?= htmlspecialchars($currentUser['bio'] ?? '') ?></textarea>
            </div>

            <div class="form-group">
                <label class="form-label"><?= __('auth_gender') ?></label>
                <div class="gender-options">
                    <?php foreach (['male'=>['fa-mars','Homme'],'female'=>['fa-venus','Femme'],'other'=>['fa-genderless','Autre']] as $val => [$icon, $label]): ?>
                    <label class="gender-opt <?= $currentUser['gender']===$val?'selected':'' ?>" onclick="selectGender('<?= $val ?>',this)">
                        <input type="radio" name="gender" value="<?= $val ?>" <?= $currentUser['gender']===$val?'checked':'' ?>>
                        <i class="fa-solid <?= $icon ?> gender-icon"></i>
                        <?= __($label) ?>
                    </label>
                    <?php endforeach; ?>
                </div>
            </div>

            <button type="submit" class="btn btn-primary">
                <i class="fa-solid fa-floppy-disk"></i> <?= __('settings_save') ?>
            </button>
        </form>

        <?php elseif ($tab === 'security'): ?>
        <!-- Security Settings -->
        <form method="POST">
            <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
            <input type="hidden" name="action" value="change_password">

            <h3 style="margin-bottom:16px;font-size:1rem"><?= __('settings_security') ?></h3>

            <div class="form-group">
                <label class="form-label"><?= __('settings_current_password') ?></label>
                <input type="password" name="old_password" class="form-control" placeholder="••••••••" required>
            </div>
            <div class="form-group">
                <label class="form-label"><?= __('settings_new_password') ?></label>
                <input type="password" name="new_password" class="form-control" placeholder="Min. 8 characters" required minlength="8">
            </div>
            <div class="form-group">
                <label class="form-label"><?= __('settings_confirm_password') ?></label>
                <input type="password" name="confirm_password" class="form-control" placeholder="Repeat new password" required>
            </div>

            <button type="submit" class="btn btn-primary">
                <i class="fa-solid fa-lock"></i> <?= __('settings_update_password') ?>
            </button>
        </form>

        <?php elseif ($tab === 'language'): ?>
        <!-- Language Settings -->
        <form method="POST">
            <input type="hidden" name="csrf_token" value="<?= csrf_token() ?>">
            <input type="hidden" name="action" value="update_language">

            <h3 style="margin-bottom:16px;font-size:1rem">
                <i class="fa-solid fa-globe"></i> <?= __('settings_language') ?>
            </h3>

            <div class="form-group">
                <label class="form-label"><?= __('settings_language') ?></label>
                <select name="language" class="form-control" style="font-size:1rem; padding:12px">
                    <?php 
                    global $availableLanguages;
                    foreach ($availableLanguages as $code => $info): 
                        $selected = (get_current_language() === $code) ? 'selected' : '';
                    ?>
                        <option value="<?= $code ?>" <?= $selected ?>>
                            <?= $info['flag'] ?> <?= $info['name'] ?>
                        </option>
                    <?php endforeach; ?>
                </select>
                <div class="form-hint" style="margin-top:8px">
                    <i class="fa-solid fa-info-circle"></i> <?= __('settings_language') ?> affects the entire interface.
                    Click save to apply the language change.
                </div>
            </div>

            <button type="submit" class="btn btn-primary" style="margin-top: 24px;">
                <i class="fa-solid fa-floppy-disk"></i> <?= __('settings_save') ?>
            </button>
        </form>

        <?php elseif ($tab === 'account'): ?>
        <!-- Account Actions -->
        <div style="margin-bottom:24px">
            <h3 style="margin-bottom:8px"><?= __('nav_logout') ?></h3>
            <p style="color:var(--text-secondary);font-size:0.9rem;margin-bottom:12px"><?= __('nav_logout') ?> from your account on this device.</p>
            <a href="<?= SITE_URL ?>/pages/logout.php" class="btn btn-outline">
                <i class="fa-solid fa-right-from-bracket"></i> <?= __('nav_logout') ?>
            </a>
        </div>

        <div class="divider"></div>

        <div>
            <h3 style="margin-bottom:8px;color:#FF3B30"><?= __('settings_delete_account') ?></h3>
            <p style="color:var(--text-secondary);font-size:0.9rem;margin-bottom:12px">
                <?= __('settings_delete_warning') ?>
            </p>
            <button class="btn btn-danger" onclick="confirmDelete()">
                <i class="fa-solid fa-trash"></i> <?= __('settings_delete_account') ?>
            </button>
        </div>
        <?php endif; ?>
    </div>
</main>

<aside class="sidebar-right">
    <div class="widget">
        <div class="widget-title"><?= __('settings_language') ?></div>
        <div style="font-size:0.85rem;color:var(--text-secondary)">
            <p>🌍 <?= __('settings_language') ?>: <strong><?= $availableLanguages[get_current_language()]['name'] ?? 'English' ?></strong></p>
            <p style="margin-top:8px">
                <i class="fa-solid fa-info-circle"></i> 
                <?= __('settings_language') ?> affects the entire interface.
            </p>
        </div>
    </div>
</aside>

<script>
function previewFile(input, imgId) {
    if (input.files && input.files[0]) {
        const reader = new FileReader();
        reader.onload = e => document.getElementById(imgId).src = e.target.result;
        reader.readAsDataURL(input.files[0]);
    }
}

function selectGender(val, el) {
    document.querySelectorAll('.gender-opt').forEach(o => o.classList.remove('selected'));
    el.classList.add('selected');
    el.querySelector('input').checked = true;
}

function confirmDelete() {
    if (confirm('<?= addslashes(__('settings_delete_account')) ?> - <?= addslashes('Are you absolutely sure? This CANNOT be undone.') ?>')) {
        if (confirm('<?= addslashes('Last warning: Delete your account permanently?') ?>')) {
            window.location.href = '<?= SITE_URL ?>/pages/delete_account.php?confirm=1';
        }
    }
}
</script>

<style>
/* Additional styles for language selector */
select.form-control {
    cursor: pointer;
    background: var(--bg-card2);
}

select.form-control option {
    padding: 10px;
    font-size: 0.95rem;
}

/* RTL Support for language selector */
[dir="rtl"] .gender-options {
    direction: rtl;
}

[dir="rtl"] .feed-tabs a {
    direction: ltr;
}

/* Gender options styling */
.gender-options {
    display: flex;
    flex-wrap: wrap;
    gap: 12px;
    margin-top: 8px;
}

.gender-opt {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 8px 16px;
    background: var(--bg-card2);
    border-radius: 40px;
    cursor: pointer;
    transition: all 0.2s ease;
    border: 2px solid transparent;
}

.gender-opt.selected {
    border-color: var(--orange);
    background: rgba(255, 107, 53, 0.1);
}

.gender-opt input {
    display: none;
}

.gender-icon {
    font-size: 1.2rem;
}
</style>

<?php include __DIR__ . '/../includes/footer.php'; ?>