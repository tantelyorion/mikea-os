-- ============================================================
-- BARKER — Messagerie interne native + correctifs
-- À exécuter UNE FOIS sur la base de BARKER (phpMyAdmin).
-- Sans risque de doublon : CREATE TABLE IF NOT EXISTS.
-- ============================================================

CREATE TABLE IF NOT EXISTS `dm_conversations` (
  `id`              INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `user_a`          INT UNSIGNED NOT NULL COMMENT 'Plus petit id utilisateur de la paire',
  `user_b`          INT UNSIGNED NOT NULL COMMENT 'Plus grand id utilisateur de la paire',
  `created_at`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_message_at` DATETIME DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_dm_pair` (`user_a`, `user_b`),
  KEY `idx_dm_user_b` (`user_b`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `dm_messages` (
  `id`              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `conversation_id` INT UNSIGNED NOT NULL,
  `sender_id`       INT UNSIGNED NOT NULL,
  `type`            ENUM('text','image','video','voice','file') NOT NULL DEFAULT 'text',
  `content`         TEXT DEFAULT NULL,
  `file_path`       VARCHAR(255) DEFAULT NULL COMMENT 'Relatif à assets/uploads/messages/',
  `file_name`       VARCHAR(255) DEFAULT NULL,
  `file_size`       INT UNSIGNED DEFAULT NULL,
  `duration`        SMALLINT UNSIGNED DEFAULT NULL COMMENT 'Secondes (vocaux)',
  `reply_to`        BIGINT UNSIGNED DEFAULT NULL,
  `sent_at`         DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `is_deleted`      TINYINT(1) NOT NULL DEFAULT 0,
  `is_edited`       TINYINT(1) NOT NULL DEFAULT 0,
  `edited_at`       DATETIME DEFAULT NULL,
  `read_at`         DATETIME DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_dm_conv` (`conversation_id`, `id`),
  KEY `idx_dm_unread` (`conversation_id`, `sender_id`, `read_at`),
  KEY `idx_dm_sender_time` (`sender_id`, `sent_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Correctif sécurité : l'accès admin ne dépend plus de "is_verified"
-- (n'importe quel compte vérifié était administrateur).
ALTER TABLE `users`
  ADD COLUMN `is_admin` TINYINT(1) NOT NULL DEFAULT 0;

-- Conserve l'accès de vos admins actuels, puis retirez ceux en trop :
--   UPDATE users SET is_admin = 0 WHERE username <> 'votre_pseudo';
UPDATE `users` SET `is_admin` = 1 WHERE `is_verified` = 1;
