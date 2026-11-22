<?php
// ============================================================
// BARKER MAIL — Intégration DirectAdmin
// ============================================================
// Création / vérification / suppression des boîtes @barker.ink
// via l'API Legacy DirectAdmin CMD_API_POP.
//
// L'authentification utilise HTTP Basic Auth avec :
//   - DIRECTADMIN_USERNAME : utilisateur DirectAdmin
//   - DIRECTADMIN_LOGIN_KEY : Login Key dédié à l'API
//
// La documentation DirectAdmin recommande les Login Keys pour les
// appels API et indique que CMD_API_POP permet de gérer les comptes
// email au niveau utilisateur.
// ============================================================

/**
 * Appelle une commande API Legacy DirectAdmin.
 *
 * @param string $command Ex: CMD_API_POP
 * @param array  $params  Paramètres POST
 * @return array ['ok' => bool, 'data' => mixed, 'error' => string|null]
 */
function directadmin_api_call(string $command, array $params = []): array
{
    if (!function_exists('curl_init')) {
        return ['ok' => false, 'data' => null, 'error' => 'L\'extension cURL n\'est pas disponible sur ce serveur.'];
    }

    $host = rtrim(DIRECTADMIN_HOST, '/');
    $url = $host . '/' . ltrim($command, '/');

    // Le mode JSON de l'API Legacy donne une réponse JSON exploitable.
    $params['json'] = 'yes';

    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_POST => true,
        CURLOPT_POSTFIELDS => http_build_query($params),
        CURLOPT_USERPWD => DIRECTADMIN_USERNAME . ':' . DIRECTADMIN_LOGIN_KEY,
        CURLOPT_HTTPAUTH => CURLAUTH_BASIC,
        CURLOPT_HTTPHEADER => [
            'Accept: application/json',
            'Content-Type: application/x-www-form-urlencoded',
        ],
        CURLOPT_SSL_VERIFYPEER => (bool) DIRECTADMIN_VERIFY_SSL,
        CURLOPT_SSL_VERIFYHOST => DIRECTADMIN_VERIFY_SSL ? 2 : 0,
        CURLOPT_CONNECTTIMEOUT => 10,
        CURLOPT_TIMEOUT => 30,
    ]);

    $response = curl_exec($ch);
    $httpCode = (int) curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $curlErr = curl_error($ch);
    curl_close($ch);

    if ($response === false) {
        return ['ok' => false, 'data' => null, 'error' => 'Erreur de connexion à DirectAdmin : ' . $curlErr];
    }

    $response = trim($response);
    if ($response === '') {
        return ['ok' => false, 'data' => null, 'error' => 'DirectAdmin a renvoyé une réponse vide (HTTP ' . $httpCode . ').'];
    }

    $decoded = json_decode($response, true);

    // DirectAdmin peut répondre en JSON avec json=yes.
    if (is_array($decoded)) {
        $errorValue = $decoded['error'] ?? 0;
        $error = filter_var($errorValue, FILTER_VALIDATE_BOOLEAN, FILTER_NULL_ON_FAILURE);
        $isError = ($error === true || (string)$errorValue === '1' || (int)$errorValue === 1);

        if (!$isError) {
            return ['ok' => true, 'data' => $decoded, 'error' => null];
        }

        $message = $decoded['text'] ?? $decoded['details'] ?? 'Erreur DirectAdmin inconnue.';
        return ['ok' => false, 'data' => $decoded, 'error' => (string)$message];
    }

    // Fallback : certaines versions/skins peuvent renvoyer du URL-encoded.
    $parsed = [];
    parse_str($response, $parsed);
    if (!empty($parsed)) {
        $errorValue = $parsed['error'] ?? 0;
        $isError = ((string)$errorValue === '1' || (int)$errorValue === 1);
        if (!$isError) {
            return ['ok' => true, 'data' => $parsed, 'error' => null];
        }
        $message = $parsed['text'] ?? $parsed['details'] ?? 'Erreur DirectAdmin inconnue.';
        return ['ok' => false, 'data' => $parsed, 'error' => (string)$message];
    }

    return ['ok' => false, 'data' => null, 'error' => 'Réponse DirectAdmin invalide (HTTP ' . $httpCode . ').'];
}

/**
 * Vérifie le format d'un nom d'utilisateur de boîte mail.
 */
function barker_mail_valid_username(string $username): bool
{
    if ($username === '' || mb_strlen($username) > 64) {
        return false;
    }
    if (in_array(mb_strtolower($username), MAIL_RESERVED_USERNAMES, true)) {
        return false;
    }
    return (bool)preg_match('/^[a-zA-Z0-9]([a-zA-Z0-9._-]{0,62}[a-zA-Z0-9])?$/', $username);
}

/**
 * Extrait une liste d'adresses depuis les différentes formes de réponse
 * CMD_API_POP rencontrées sur DirectAdmin.
 */
function barker_mail_extract_usernames($data): array
{
    $usernames = [];

    if (!is_array($data)) {
        return $usernames;
    }

    // Réponse JSON moderne/structurée : list / list[] / accounts.
    $rows = $data['list'] ?? $data['accounts'] ?? null;
    if (is_array($rows)) {
        foreach ($rows as $key => $row) {
            if (is_string($row)) {
                $email = $row;
            } elseif (is_array($row)) {
                $email = $row['email'] ?? $row['address'] ?? $row['login'] ?? $row['user'] ?? '';
            } else {
                $email = '';
            }
            if ($email !== '' && strpos($email, '@') !== false) {
                [$local, $domain] = explode('@', $email, 2);
                if (strcasecmp($domain, MAIL_DOMAIN) === 0) {
                    $usernames[] = mb_strtolower($local);
                }
            } elseif (is_string($key) && preg_match('/^[a-zA-Z0-9._-]+$/', $key)) {
                $usernames[] = mb_strtolower($key);
            }
        }
    }

    // Certaines réponses utilisent user0=user, user1=user, etc.
    foreach ($data as $key => $value) {
        if (preg_match('/^(?:user|email)\d*$/i', (string)$key) && is_string($value)) {
            $value = trim($value);
            if (strpos($value, '@') !== false) {
                [$local, $domain] = explode('@', $value, 2);
                if (strcasecmp($domain, MAIL_DOMAIN) === 0) {
                    $usernames[] = mb_strtolower($local);
                }
            } elseif (preg_match('/^[a-zA-Z0-9._-]+$/', $value)) {
                $usernames[] = mb_strtolower($value);
            }
        }
    }

    return array_values(array_unique($usernames));
}

/**
 * Liste les boîtes mail existantes sur MAIL_DOMAIN.
 */
function barker_mail_list_accounts(): array
{
    $result = directadmin_api_call('CMD_API_POP', [
        'action' => 'list',
        'domain' => MAIL_DOMAIN,
    ]);

    if (!$result['ok']) {
        return [];
    }

    return barker_mail_extract_usernames($result['data']);
}

/**
 * true si le nom d'utilisateur est libre.
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
 * Crée une boîte username@barker.ink via DirectAdmin CMD_API_POP.
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

    $result = directadmin_api_call('CMD_API_POP', [
        'action'  => 'create',
        'domain'  => MAIL_DOMAIN,
        'user'    => $username,
        'passwd'  => $password,
        'passwd2' => $password,
        'quota'   => max(0, $quotaMb),
    ]);

    if (!$result['ok']) {
        return ['ok' => false, 'error' => $result['error'] ?? 'Échec de création de la boîte Barker Mail.'];
    }

    return ['ok' => true, 'error' => null];
}

/**
 * Supprime une boîte mail. Utilisé pour les rollbacks.
 */
function barker_mail_delete_account(string $username): array
{
    if (!barker_mail_valid_username($username)) {
        return ['ok' => false, 'error' => 'Nom d\'utilisateur Barker Mail invalide.'];
    }

    $result = directadmin_api_call('CMD_API_POP', [
        'action' => 'delete',
        'domain' => MAIL_DOMAIN,
        'user'   => $username,
    ]);

    return ['ok' => $result['ok'], 'error' => $result['error'] ?? null];
}
