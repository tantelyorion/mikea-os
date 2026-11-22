-- ============================================================
-- BARKER MAIL — Migration base de données
-- À exécuter UNE FOIS dans phpMyAdmin (ou via SSH/mysql) sur la
-- base gasyamis_barker, avant de mettre en ligne le nouveau
-- register.php.
-- ============================================================

ALTER TABLE `users`
    ADD COLUMN `mail_username` VARCHAR(64) DEFAULT NULL COMMENT 'Partie locale de l addresse Barker Mail (avant @barker.ink), si compte cree' AFTER `email`,
    ADD COLUMN `has_barker_mail` TINYINT(1) DEFAULT 0 COMMENT '1 si une boite Barker Mail a ete creee pour ce compte' AFTER `mail_username`;

ALTER TABLE `users`
    ADD UNIQUE KEY `idx_mail_username` (`mail_username`);
