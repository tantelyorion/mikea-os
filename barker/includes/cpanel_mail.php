<?php
// ============================================================
// BARKER MAIL — Intégration cPanel UAPI
// ============================================================
// Ce fichier permet de créer/vérifier/supprimer automatiquement
// des comptes email @barker.ink depuis l'application BARKER,
// en utilisant l'API UAPI d'un compte cPanel classique (mutualisé,
// pas besoin de WHM ni de VPS).
//
// Doc UAPI Email::add_pop :
// https://api.docs.cpanel.net/openapi/cpanel/operation/add_pop/
// ============================================================

/**
 * Appelle un module/fonction de l'UAPI cPanel avec le token API.
 *
 * @param string $module   Ex: "Email"
 * @param string $function Ex: "add_pop"
 * @param array  $params   Paramètres de la fonction
 * @return array ['ok' => bool, 'data' => mixed, 'error' => string|null]
 */
function cpanel_uapi_call(string $module, string $function, array $params = []): array
{
    if (!function_exists('curl_init')) {
        return ['ok' => false, 'data' => null, 'error' => 'L\'extension cURL n\'est pas disponible sur ce serveur.'];
    }

    $host = rtrim(CPANEL_HOST, '/');
    $url  = $host . '/execute/' . rawurlencode($module) . '/' . rawurlencode($function);

    if (!empty($params)) {
        $url .= '?' . http_build_query($params);
    }

    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_HTTPHEADER => [
            'Authorization: cpanel ' . CPANEL_USERNAME . ':' . CPANEL_API_TOKEN,
        ],
        CURLOPT_SSL_VERIFYPEER => true,
        CURLOPT_TIMEOUT => 20,
    ]);

    $response = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $curlErr  = curl_error($ch);
    curl_close($ch);

    if ($response === false) {
        return ['ok' => false, 'data' => null, 'error' => 'Erreur de connexion à cPanel : ' . $curlErr];
    }

    $decoded = json_decode($response, true);

    if ($decoded === null) {
        return ['ok' => false, 'data' => null, 'error' => 'Réponse cPanel invalide (HTTP ' . $httpCode . ').'];
    }

    // Format UAPI : { status: 1|0, errors: [...], data: ... }
    if (isset($decoded['status']) && (int)$decoded['status'] === 1) {
        return ['ok' => true, 'data' => $decoded['data'] ?? null, 'error' => null];
    }

    $errorMsg = 'Erreur cPanel inconnue.';
    if (!empty($decoded['errors']) && is_array($decoded['errors'])) {
        $errorMsg = implode(' ', $decoded['errors']);
    }

    return ['ok' => false, 'data' => $decoded['data'] ?? null, 'error' => $errorMsg];
}

/**
 * Vérifie qu'un nom d'utilisateur respecte le format autorisé pour
 * une boîte mail cPanel (partie locale d'une adresse email).
 */
function barker_mail_valid_username(string $username): bool
{
    if ($username === '' || mb_strlen($username) > 64) {
        return false;
    }
    if (in_array(mb_strtolower($username), MAIL_RESERVED_USERNAMES, true)) {
        return false;
    }
    // lettres, chiffres, points, tirets, underscores — commence/finit par alphanumérique
    return (bool)preg_match('/^[a-zA-Z0-9]([a-zA-Z0-9._-]{0,62}[a-zA-Z0-9])?$/', $username);
}

/**
 * Liste les boîtes mail existantes sur le domaine MAIL_DOMAIN,
 * pour vérifier la disponibilité d'un nom d'utilisateur.
 */
function barker_mail_list_accounts(): array
{
    $result = cpanel_uapi_call('Email', 'list_pops', [
        'domain' => MAIL_DOMAIN,
    ]);

    if (!$result['ok'] || !is_array($result['data'])) {
        return [];
    }

    $usernames = [];
    foreach ($result['data'] as $row) {
        // Le champ "email" contient généralement "user@domaine"
        $email = $row['email'] ?? ($row['login'] ?? '');
        $local = strstr($email, '@', true);
        if ($local !== false) {
            $usernames[] = mb_strtolower($local);
        }
    }
    return $usernames;
}

/**
 * true si le nom d'utilisateur (partie avant @barker.ink) est libre.
 */
function barker_mail_username_available(string $username): bool
{
    if (!barker_mail_valid_username($username)) {
        return false;
    }
    $existing = barker_mail_list_accounts();
    return !in_array(mb_strtolower($username), $existing, true);
}

/**
 * Crée une boîte mail username@barker.ink via cPanel UAPI (Email::add_pop).
 *
 * @return array ['ok' => bool, 'error' => string|null]
 */
function barker_mail_create_account(string $username, string $password, int $quotaMb = null): array
{
    if (!barker_mail_valid_username($username)) {
        return ['ok' => false, 'error' => 'Nom d\'utilisateur Barker Mail invalide.'];
    }
    if (mb_strlen($password) < 8) {
        return ['ok' => false, 'error' => 'Le mot de passe Barker Mail doit contenir au moins 8 caractères.'];
    }

    $quotaMb = $quotaMb ?? MAIL_DEFAULT_QUOTA_MB;

    $result = cpanel_uapi_call('Email', 'add_pop', [
        'email'    => $username,
        'domain'   => MAIL_DOMAIN,
        'password' => $password,
        'quota'    => $quotaMb,
    ]);

    if (!$result['ok']) {
        return ['ok' => false, 'error' => $result['error'] ?? 'Échec de création de la boîte Barker Mail.'];
    }

    return ['ok' => true, 'error' => null];
}

/**
 * Supprime une boîte mail (utilisé en cas de rollback si l'inscription
 * échoue après la création de la boîte, ou pour la suppression de compte).
 */
function barker_mail_delete_account(string $username): array
{
    $result = cpanel_uapi_call('Email', 'delete_pop', [
        'email'  => $username,
        'domain' => MAIL_DOMAIN,
    ]);

    return ['ok' => $result['ok'], 'error' => $result['error'] ?? null];
}
