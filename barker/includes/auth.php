<?php
// ============================================================
// BARKER — Auth & Session helpers - VERSION CORRIGÉE UTF-8
// ============================================================

function session_start_safe(): void {
    if (session_status() === PHP_SESSION_NONE) {
        ini_set('session.cookie_lifetime', SESSION_LIFETIME);
        ini_set('session.gc_maxlifetime', SESSION_LIFETIME);
        session_set_cookie_params([
            'lifetime' => SESSION_LIFETIME,
            'path'     => '/',
            'secure'   => (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off')
                          || (($_SERVER['HTTP_X_FORWARDED_PROTO'] ?? '') === 'https' && defined('TRUST_PROXY_HEADERS') && TRUST_PROXY_HEADERS),
            'httponly' => true,
            'samesite' => 'Lax',
        ]);
        session_start();
    }
}

function is_logged_in(): bool {
    return isset($_SESSION['user_id']) && !empty($_SESSION['user_id']);
}

function current_user(): ?array {
    if (!is_logged_in()) return null;
    static $user = null;
    if ($user === null) {
        $user = DB::fetchOne(
            "SELECT * FROM users WHERE id = ? AND is_banned = 0",
            [$_SESSION['user_id']]
        );
    }
    return $user ?: null;
}

function require_login(): void {
    if (!is_logged_in()) {
        header('Location: ' . SITE_URL . '/pages/login.php');
        exit;
    }
}

function login_user(array $user): void {
    // Régénérer l'id AVANT d'écrire la session (anti fixation de session)
    session_regenerate_id(true);
    $_SESSION['user_id']   = (int)$user['id'];
    $_SESSION['username']  = $user['username'];
    $_SESSION['theme']     = $user['theme'] ?? 'dark';
}

function logout_user(): void {
    $_SESSION = [];
    session_destroy();
}

function hash_password(string $password): string {
    return password_hash($password, PASSWORD_BCRYPT, ['cost' => BCRYPT_COST]);
}

function verify_password(string $password, string $hash): bool {
    return password_verify($password, $hash);
}

function get_user_avatar(?array $user): string {
    if ($user && !empty($user['avatar'])) {
        return SITE_URL . '/assets/uploads/avatars/' . htmlspecialchars($user['avatar']);
    }
    $gender = $user['gender'] ?? 'other';
    return match($gender) {
        'male'   => SITE_URL . '/assets/default_avatars/male.png',
        'female' => SITE_URL . '/assets/default_avatars/female.png',
        default  => SITE_URL . '/assets/default_avatars/other.png',
    };
}

function get_user_cover(?array $user): string {
    if ($user && !empty($user['cover_photo'])) {
        return SITE_URL . '/assets/uploads/covers/' . htmlspecialchars($user['cover_photo']);
    }
    return SITE_URL . '/assets/covers/default_cover.jpg';
}

function get_display_name(array $bark, array $user): string {
    if ($bark['is_anonymous']) return '<i class="fa-solid fa-glasses"></i> Anonymous';
    return htmlspecialchars($user['display_name']);
}

function log_security(int $userId = null, string $action = '', string $details = ''): void {
    if ($userId !== null) {
        $userExists = DB::fetchOne("SELECT id FROM users WHERE id = ?", [$userId]);
        if (!$userExists) {
            $userId = null;
        }
    }
    
    DB::execute(
        "INSERT INTO security_logs (user_id, ip, action, details) VALUES (?,?,?,?)",
        [$userId, get_client_ip(), $action, $details]
    );
}

function get_client_ip(): string {
    // Les en-têtes X-Forwarded-For / CF-Connecting-IP sont falsifiables par
    // n'importe quel client : on ne les lit que derrière un proxy de confiance
    // déclaré (define('TRUST_PROXY_HEADERS', true) dans config.local.php).
    $keys = (defined('TRUST_PROXY_HEADERS') && TRUST_PROXY_HEADERS)
        ? ['HTTP_CF_CONNECTING_IP', 'HTTP_X_FORWARDED_FOR', 'HTTP_X_REAL_IP', 'REMOTE_ADDR']
        : ['REMOTE_ADDR'];
    foreach ($keys as $k) {
        if (!empty($_SERVER[$k])) {
            $ip = trim(explode(',', $_SERVER[$k])[0]);
            if (filter_var($ip, FILTER_VALIDATE_IP)) return $ip;
        }
    }
    return '0.0.0.0';
}

function csrf_token(): string {
    if (empty($_SESSION['csrf_token'])) {
        $_SESSION['csrf_token'] = bin2hex(random_bytes(32));
    }
    return $_SESSION['csrf_token'];
}

function verify_csrf(string $token): bool {
    return isset($_SESSION['csrf_token']) && hash_equals($_SESSION['csrf_token'], $token);
}

function sanitize(string $input): string {
    // NE PAS utiliser htmlspecialchars sur l'input brut
    // Garder les caractères UTF-8 intacts
    return trim($input);
}

/**
 * Fonction pour afficher du texte en toute sécurité dans HTML
 */
function safe_output(string $text): string {
    return htmlspecialchars($text, ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML5, 'UTF-8');
}

function time_ago(string $datetime): string {
    $diff = time() - strtotime($datetime);
    if ($diff < 60)     return $diff . 's';
    if ($diff < 3600)   return floor($diff/60) . 'm';
    if ($diff < 86400)  return floor($diff/3600) . 'h';
    if ($diff < 604800) return floor($diff/86400) . 'd';
    return date('d M Y', strtotime($datetime));
}

function extract_hashtags(string $content): array {
    preg_match_all('/#([\p{L}\p{N}_]+)/u', $content, $matches);
    return array_unique($matches[1]);
}

/**
 * Fonction corrigée pour linkify_content - NE DOUBLE PAS ENCODER
 * Gère correctement les caractères spéciaux, émojis, chinois, arabe, russe
 * Le contenu en entrée doit déjà être encodé HTML
 */
function linkify_content(string $content): string {
    // Le contenu est déjà encodé en HTML ici
    
    if (empty($content)) return '';
    
    // URLs
    $content = preg_replace(
        '/(https?:\/\/[^\s<>"\'\)\[]+)/i',
        '<a href="$1" target="_blank" rel="noopener noreferrer" class="bark-link">$1</a>',
        $content
    );
    
    // Hashtags (supporte les caractères Unicode dans les hashtags)
    $content = preg_replace_callback(
        '/#([\p{L}\p{N}_]+)/u',
        function($matches) {
            return '<a href="' . SITE_URL . '/pages/hashtag.php?tag=' . urlencode($matches[1]) . '" class="bark-hashtag">#' . $matches[1] . '</a>';
        },
        $content
    );
    
    // Mentions (supporte les caractères Unicode)
    $content = preg_replace_callback(
        '/@([\p{L}\p{N}_]+)/u',
        function($matches) {
            return '<a href="' . SITE_URL . '/pages/profile.php?u=' . urlencode($matches[1]) . '" class="bark-mention">@' . $matches[1] . '</a>';
        },
        $content
    );
    
    // Retourne le contenu avec les liens, sans encoder le reste
    return nl2br($content);
}

/**
 * Fonction pour afficher le contenu d'un bark en toute sécurité
 * À utiliser dans les templates PHP
 * CORRIGÉE : encode d'abord en HTML, puis applique les liens
 */
function display_bark_content(string $content): string {
    // Si le contenu est vide
    if (empty($content)) return '';
    
    // ⭐ Encoder pour le HTML d'abord
    $encoded = htmlspecialchars($content, ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML5, 'UTF-8');
    
    // Puis appliquer les liens, hashtags, mentions
    return linkify_content($encoded);
}

/**
 * Récupérer les détails complets d'un poll pour l'affichage dans les citations
 */
function getPollDetailsForQuote($pollId) {
    if (!$pollId) return null;
    
    $poll = DB::fetchOne("
        SELECT p.*
        FROM polls p
        WHERE p.id = ?
    ", [$pollId]);
    
    if (!$poll) return null;
    
    $options = DB::fetchAll("
        SELECT * FROM poll_options 
        WHERE poll_id = ? 
        ORDER BY option_order ASC
    ", [$pollId]);
    
    return [
        'poll' => $poll,
        'options' => $options,
        'total_votes' => (int)($poll['total_votes'] ?? 0)
    ];
}