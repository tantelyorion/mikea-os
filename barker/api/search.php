<?php
// ============================================================
// BARKER API — Search autocomplete
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

$q = trim($_GET['q'] ?? '');

if (mb_strlen($q) < 2) {
    json_response(['users' => [], 'hashtags' => []]);
}

$like = '%' . $q . '%';

// Users
$users = DB::fetchAll("
    SELECT id, username, display_name, avatar, gender, is_verified
    FROM users
    WHERE is_banned=0
      AND (username LIKE ? OR display_name LIKE ?)
    LIMIT 5
", [$like, $like]);

$usersFormatted = array_map(function($u) {
    return [
        'id'           => $u['id'],
        'username'     => $u['username'],
        'display_name' => $u['display_name'],
        'avatar'       => get_user_avatar($u),
        'is_verified'  => (bool)$u['is_verified'],
        'url'          => SITE_URL . '/pages/profile.php?u=' . urlencode($u['username']),
    ];
}, $users);

// Hashtags
$hashtags = DB::fetchAll("
    SELECT name, bark_count FROM hashtags
    WHERE name LIKE ?
    ORDER BY bark_count DESC
    LIMIT 5
", [$like]);

json_response([
    'users'    => $usersFormatted,
    'hashtags' => $hashtags,
]);
