<?php
// ============================================================
// BARKER — Language Manager - VERSION COMPLÈTE
// ============================================================

// Liste des langues supportées - COMPLÈTE avec toutes les nouvelles langues
$GLOBALS['availableLanguages'] = [
    // Langues existantes (14)
    'en' => ['name' => 'English', 'flag' => '🇬🇧', 'direction' => 'ltr'],
    'zh' => ['name' => '中文', 'flag' => '🇨🇳', 'direction' => 'ltr'],
    'es' => ['name' => 'Español', 'flag' => '🇪🇸', 'direction' => 'ltr'],
    'fr' => ['name' => 'Français', 'flag' => '🇫🇷', 'direction' => 'ltr'],
    'de' => ['name' => 'Deutsch', 'flag' => '🇩🇪', 'direction' => 'ltr'],
    'pt' => ['name' => 'Português', 'flag' => '🇵🇹', 'direction' => 'ltr'],
    'hi' => ['name' => 'हिन्दी', 'flag' => '🇮🇳', 'direction' => 'ltr'],
    'ru' => ['name' => 'Русский', 'flag' => '🇷🇺', 'direction' => 'ltr'],
    'ko' => ['name' => '한국어', 'flag' => '🇰🇷', 'direction' => 'ltr'],
    'mg' => ['name' => 'Malagasy', 'flag' => '🇲🇬', 'direction' => 'ltr'],
    'ar' => ['name' => 'العربية', 'flag' => '🇸🇦', 'direction' => 'rtl'],
    'iw' => ['name' => 'עברית', 'flag' => '🇮🇱', 'direction' => 'rtl'],
    'sw' => ['name' => 'Kiswahili', 'flag' => '🇹🇿', 'direction' => 'ltr'],
    'zu' => ['name' => 'isiZulu', 'flag' => '🇿🇦', 'direction' => 'ltr'],
    
    // NOUVELLES LANGUES (13)
    'ja' => ['name' => '日本語', 'flag' => '🇯🇵', 'direction' => 'ltr'],
    'si' => ['name' => 'සිංහල', 'flag' => '🇱🇰', 'direction' => 'ltr'],
    'th' => ['name' => 'ไทย', 'flag' => '🇹🇭', 'direction' => 'ltr'],
    'id' => ['name' => 'Bahasa Indonesia', 'flag' => '🇮🇩', 'direction' => 'ltr'],
    'ln' => ['name' => 'Lingala', 'flag' => '🇨🇩', 'direction' => 'ltr'],
    'mi' => ['name' => 'Māori', 'flag' => '🇳🇿', 'direction' => 'ltr'],
    'it' => ['name' => 'Italiano', 'flag' => '🇮🇹', 'direction' => 'ltr'],
    'el' => ['name' => 'Ελληνικά', 'flag' => '🇬🇷', 'direction' => 'ltr'],
    'tr' => ['name' => 'Türkçe', 'flag' => '🇹🇷', 'direction' => 'ltr'],
    'mn' => ['name' => 'Монгол хэл', 'flag' => '🇲🇳', 'direction' => 'ltr'],
    'vi' => ['name' => 'Tiếng Việt', 'flag' => '🇻🇳', 'direction' => 'ltr'],
    'am' => ['name' => 'አማርኛ', 'flag' => '🇪🇹', 'direction' => 'ltr'],
    'et' => ['name' => '🌌 Extraterrestrial', 'flag' => '👽', 'direction' => 'ltr'],
];

// Obtenir la langue actuelle de l'utilisateur
function get_current_language(): string {
    global $availableLanguages;
    
    // Priorité 1: Utilisateur connecté avec langue en BDD
    if (is_logged_in()) {
        $user = current_user();
        if ($user && !empty($user['language']) && isset($availableLanguages[$user['language']])) {
            return $user['language'];
        }
    }
    
    // Priorité 2: Session
    if (isset($_SESSION['language']) && isset($availableLanguages[$_SESSION['language']])) {
        return $_SESSION['language'];
    }
    
    // Priorité 3: Détection navigateur
    if (isset($_SERVER['HTTP_ACCEPT_LANGUAGE'])) {
        $browserLang = substr($_SERVER['HTTP_ACCEPT_LANGUAGE'], 0, 2);
        if (isset($availableLanguages[$browserLang])) {
            return $browserLang;
        }
    }
    
    // Par défaut: Anglais
    return 'en';
}

// Changer la langue de l'utilisateur
function set_language(string $langCode): bool {
    global $availableLanguages;
    
    if (!isset($availableLanguages[$langCode])) {
        return false;
    }
    
    $_SESSION['language'] = $langCode;
    
    if (is_logged_in()) {
        DB::execute(
            "UPDATE users SET language = ? WHERE id = ?",
            [$langCode, $_SESSION['user_id']]
        );
    }
    
    return true;
}

// Fonction de traduction
function __(string $key, array $params = []): string {
    static $translations = null;
    static $currentLang = null;
    
    $lang = get_current_language();
    
    // Recharger seulement si la langue a changé
    if ($translations === null || $currentLang !== $lang) {
        $currentLang = $lang;
        $langFile = __DIR__ . "/../locales/{$lang}.php";
        
        if (file_exists($langFile)) {
            $translations = include $langFile;
            if (!is_array($translations)) {
                $translations = [];
            }
        } else {
            // Fallback à l'anglais
            $fallbackFile = __DIR__ . "/../locales/en.php";
            if (file_exists($fallbackFile)) {
                $translations = include $fallbackFile;
            } else {
                $translations = [];
            }
        }
    }
    
    $text = $translations[$key] ?? $key;
    
    if (!empty($params)) {
        foreach ($params as $paramKey => $value) {
            $text = str_replace('{' . $paramKey . '}', $value, $text);
            // Support pour {s} pluriel
            $text = str_replace('{s}', $value > 1 ? 's' : '', $text);
        }
    }
    
    return $text;
}

// Obtenir la direction du texte (RTL pour arabe/hébreu)
function get_text_direction(): string {
    $lang = get_current_language();
    global $availableLanguages;
    
    return $availableLanguages[$lang]['direction'] ?? 'ltr';
}

// Obtenir la liste des langues disponibles
function get_available_languages(): array {
    global $availableLanguages;
    return $availableLanguages;
}