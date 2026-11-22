<?php
require_once __DIR__ . '/config.php';
ml_logout();
header('Location: login.php');
exit;
