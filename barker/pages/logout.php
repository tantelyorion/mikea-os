<?php
require_once dirname(__DIR__) . '/includes/init.php';

if (is_logged_in()) {
    log_security($_SESSION['user_id'], 'logout', '');
    logout_user();
}

redirect(SITE_URL . '/pages/login.php');
