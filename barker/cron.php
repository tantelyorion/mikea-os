#!/usr/bin/env php
<?php
// ============================================================
// BARKER — Cron / Maintenance Script
// Run every hour via: php /path/to/barker/cron.php
// Crontab: 0 * * * * php /var/www/html/barker/cron.php >> /var/log/barker_cron.log 2>&1
// ============================================================

define('BARKER_CRON', true);
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/includes/db.php';

$log = function(string $msg) {
    echo '[' . date('Y-m-d H:i:s') . '] ' . $msg . PHP_EOL;
};

$log('BARKER cron started');

// ── 1. Messagerie : retirer les fichiers de messages supprimés (orphelins) ──
$msgBase = __DIR__ . '/assets/uploads/messages/';
$orphanMsgFiles = 0;
$known = [];
foreach (DB::fetchAll("SELECT file_path FROM dm_messages WHERE file_path IS NOT NULL") as $r) {
    $known[basename($r['file_path'])] = true;
}
foreach (['media', 'documents', 'voice'] as $sub) {
    foreach (glob($msgBase . $sub . '/*') ?: [] as $f) {
        // Marge de 1 h pour ne pas supprimer un upload en cours d'enregistrement
        if (is_file($f) && !isset($known[basename($f)]) && filemtime($f) < time() - 3600) {
            @unlink($f);
            $orphanMsgFiles++;
        }
    }
}
$log("💬 Orphaned message files removed: $orphanMsgFiles");

// ── 2. Clean up old security logs (keep 90 days) ─────────────
$oldLogs = DB::execute(
    "DELETE FROM security_logs WHERE created_at < DATE_SUB(NOW(), INTERVAL 90 DAY)"
);
$log("🛡  Old security logs deleted: $oldLogs");

// ── 3. Clean up old read notifications (keep 30 days) ────────
$oldNotifs = DB::execute(
    "DELETE FROM notifications WHERE is_read=1 AND created_at < DATE_SUB(NOW(), INTERVAL 30 DAY)"
);
$log("🔔 Old notifications deleted: $oldNotifs");

// ── 4. Update hashtag bark counts ────────────────────────────
DB::execute("
    UPDATE hashtags h
    SET h.bark_count = (
        SELECT COUNT(*)
        FROM bark_hashtags bh
        JOIN barks b ON b.id = bh.bark_id
        WHERE bh.hashtag_id = h.id
          AND b.is_deleted = 0
    )
");
$log("🏷  Hashtag counts updated");

// ── 5. Remove orphaned hashtags ───────────────────────────────
$orphans = DB::execute(
    "DELETE FROM hashtags WHERE bark_count = 0"
);
$log("🏷  Orphan hashtags removed: $orphans");

// ── 6. Clean orphaned media files ────────────────────────────
$mediaDir = __DIR__ . '/assets/uploads/media/';
if (is_dir($mediaDir)) {
    $dbFiles = DB::fetchAll("SELECT url FROM media");
    $dbSet   = array_flip(array_column($dbFiles, 'url'));
    $fsFiles = glob($mediaDir . '*');
    $removed = 0;
    foreach ($fsFiles as $file) {
        $name = basename($file);
        if (!isset($dbSet[$name])) {
            @unlink($file);
            $removed++;
        }
    }
    $log("🖼  Orphaned media files removed: $removed");
}

// ── 7. Stats summary ─────────────────────────────────────────
$stats = DB::fetchOne("
    SELECT
        (SELECT COUNT(*) FROM users WHERE is_banned=0)  AS users,
        (SELECT COUNT(*) FROM barks WHERE is_deleted=0) AS barks,
        (SELECT COUNT(*) FROM paws)                      AS paws,
        (SELECT COALESCE(SUM(amount),0) FROM bones WHERE status='completed') AS total_bones,
        (SELECT COUNT(*) FROM reports WHERE status='pending') AS pending_reports
");

$log("📊 Stats: {$stats['users']} users | {$stats['barks']} barks | {$stats['paws']} paws | €{$stats['total_bones']} donated | {$stats['pending_reports']} pending reports");

$log('✅ BARKER cron completed');
echo PHP_EOL;
