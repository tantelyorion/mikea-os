<?php
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

if ($_SERVER['REQUEST_METHOD'] !== 'POST') json_response(['error'=>'POST only'],405);

$body  = json_decode(file_get_contents('php://input'), true);
$theme = $body['theme'] ?? 'dark';

if (!in_array($theme, ['dark','light'])) json_response(['error'=>'Invalid theme'],400);

$_SESSION['theme'] = $theme;
DB::execute("UPDATE users SET theme=? WHERE id=?", [$theme, $_SESSION['user_id']]);

json_response(['success'=>true,'theme'=>$theme]);
