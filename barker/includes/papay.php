<?php
/**
 * BARKER - Integration PAPAY
 * Fonctions pour lier les comptes Barker avec PAPAY
 */

// ============================================================
// PROTECTION : Empêche le rechargement du fichier
// (corrige le warning "Constant already defined")
// ============================================================
if (defined('PAPAY_INTEGRATION_LOADED')) {
    return;
}
define('PAPAY_INTEGRATION_LOADED', true);

// ============================================================
// CONSTANTES (protection supplémentaire au cas où)
// ============================================================
if (!defined('PAPAY_API_URL')) {
    define('PAPAY_API_URL', 'https://papay.info/api/index.php');
}

if (!defined('PAPAY_BASE_URL')) {
    define('PAPAY_BASE_URL', 'https://papay.info');
}

// ============================================================
// FONCTIONS
// ============================================================

/**
 * Vérifier si un utilisateur Barker a un compte PAPAY associé
 */
if (!function_exists('user_has_papay_account')) {
    function user_has_papay_account(int $userId): bool {
        $user = DB::fetchOne(
            "SELECT papay_user_id FROM users WHERE id = ?",
            [$userId]
        );
        return !empty($user['papay_user_id']);
    }
}

/**
 * Créer un compte PAPAY pour un utilisateur Barker
 *
 * ⚠️ ATTENTION : La signature a été mise à jour pour correspondre
 * à l'appel dans register.php qui passe un mot de passe.
 */
if (!function_exists('create_papay_account')) {
    function create_papay_account(
        int $userId,
        string $username,
        string $email,
        string $fullName,
        string $phone = '',
        string $password = ''
    ): ?int {
        // Appel API à PAPAY pour créer le compte
        $data = [
            'username'  => $username,
            'email'     => $email,
            'full_name' => $fullName,
            'phone'     => $phone,
            'password'  => $password,
            'action'    => 'create_account'
        ];

        $result = call_papay_api('create_user', $data);

        if ($result && isset($result['user_id'])) {
            DB::execute(
                "UPDATE users SET papay_user_id = ?, papay_account_created = 1, papay_email = ? WHERE id = ?",
                [$result['user_id'], $email, $userId]
            );

            log_security($userId, 'papay_account_created', "Compte PAPAY créé pour l'utilisateur");
            return (int) $result['user_id'];
        }

        return null;
    }
}

/**
 * Lier un compte PAPAY existant à un compte Barker
 */
if (!function_exists('link_papay_account')) {
    function link_papay_account(int $userId, string $papayUsername): bool {
        $result = call_papay_api('get_user', ['username' => $papayUsername]);

        if ($result && isset($result['user_id'])) {
            DB::execute(
                "UPDATE users SET papay_user_id = ?, papay_email = ?, papay_linked_at = NOW() WHERE id = ?",
                [$result['user_id'], $result['email'] ?? '', $userId]
            );

            log_security($userId, 'papay_account_linked', "Compte PAPAY lié: " . $papayUsername);
            return true;
        }

        return false;
    }
}

/**
 * Appel à l'API PAPAY
 */
if (!function_exists('call_papay_api')) {
    function call_papay_api(string $action, array $data): ?array {
        $apiKey    = defined('PAPAY_API_KEY')    ? PAPAY_API_KEY    : 'VOTRE_API_KEY';
        $apiSecret = defined('PAPAY_API_SECRET') ? PAPAY_API_SECRET : 'VOTRE_API_SECRET';

        $ch = curl_init();
        curl_setopt_array($ch, [
            CURLOPT_URL            => PAPAY_API_URL . '?action=' . $action,
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_POST           => true,
            CURLOPT_POSTFIELDS     => json_encode($data),
            CURLOPT_HTTPHEADER     => [
                'Content-Type: application/json',
                'X-API-KEY: ' . $apiKey,
                'X-API-SECRET: ' . $apiSecret
            ],
            CURLOPT_TIMEOUT        => 30,
        ]);

        $response = curl_exec($ch);
        $error    = curl_error($ch);
        curl_close($ch);

        if ($error) {
            error_log("PAPAY API Error: $error");
            return null;
        }

        return json_decode($response, true);
    }
}

/**
 * Obtenir les informations PAPAY d'un utilisateur Barker
 */
if (!function_exists('get_papay_user_info')) {
    function get_papay_user_info(int $userId): ?array {
        $user = DB::fetchOne(
            "SELECT papay_user_id, papay_email, papay_linked_at, papay_account_created
             FROM users WHERE id = ?",
            [$userId]
        );

        if (!$user || empty($user['papay_user_id'])) {
            return null;
        }

        return call_papay_api('get_user', ['user_id' => $user['papay_user_id']]);
    }
}