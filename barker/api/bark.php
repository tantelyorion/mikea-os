<?php
// ============================================================
// BARKER API — Bark (post/delete/edit) - VERSION COMPLÈTE CORRIGÉE UTF-8
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

header('Content-Type: application/json');

$action = $_GET['action'] ?? '';

// ============================================================
// ACTION: POST BARK - VERSION CORRIGÉE (sans html_entity_decode)
// ============================================================
if ($action === 'post' && $_SERVER['REQUEST_METHOD'] === 'POST') {

    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        json_response(['error' => 'Invalid CSRF token'], 403);
    }

    $content = $_POST['content'] ?? '';
    $content = trim($content);
    
    // ⚠️ NE PAS utiliser html_entity_decode ici !
    // Le contenu vient déjà en UTF-8 du formulaire
    // html_entity_decode va corrompre les caractères spéciaux
    
    $isAnon = isset($_POST['is_anonymous']) && $_POST['is_anonymous'] == 'on' ? 1 : 0;

    // Validation
    if (empty($content)) {
        echo json_encode(['error' => 'Content is required']);
        exit;
    }
    
    if (mb_strlen($content) > 300) {
        echo json_encode(['error' => 'Content too long (max 300 characters)']);
        exit;
    }

    try {
        DB::get()->beginTransaction();
        
        // Insertion du bark - stockage direct sans modification
        $barkId = DB::insert(
            "INSERT INTO barks (user_id, content, is_anonymous) VALUES (?, ?, ?)",
            [$_SESSION['user_id'], $content, $isAnon]
        );

        // Gestion des médias (si présents)
        // Sécurité : le type réel du fichier est détecté côté serveur
        // (jamais via le Content-Type ou le nom fourni par le client),
        // et chaque média est automatiquement compressé après import.
        if (!empty($_FILES['media']['name'][0])) {
            $mediaDir = __DIR__ . '/../assets/uploads/media/';
            $fileCount = min(count($_FILES['media']['name']), 4);
            $maxImageBytes = 10 * 1024 * 1024;   // 10 Mo
            $maxVideoBytes = 100 * 1024 * 1024;  // 100 Mo (avant compression)

            for ($i = 0; $i < $fileCount; $i++) {
                if ($_FILES['media']['error'][$i] !== UPLOAD_ERR_OK) continue;

                $tmpName = $_FILES['media']['tmp_name'][$i];
                $prefix  = 'media_' . $barkId . '_' . $i;

                $result = barker_store_image($tmpName, $mediaDir, $prefix, $maxImageBytes);
                $mediaType = 'image';

                if (!$result) {
                    $result = barker_store_video($tmpName, $mediaDir, $prefix, $maxVideoBytes);
                    $mediaType = 'video';
                }

                if ($result) {
                    DB::execute(
                        "INSERT INTO media (bark_id, type, url) VALUES (?, ?, ?)",
                        [$barkId, $mediaType, $result['filename']]
                    );
                }
            }
        }

        // Extraire et sauvegarder les hashtags
        preg_match_all('/#([\p{L}\p{N}_]+)/u', $content, $matches);
        $hashtags = array_unique($matches[1]);
        
        foreach ($hashtags as $tag) {
            $tagLower = mb_strtolower($tag, 'UTF-8');
            
            DB::execute(
                "INSERT INTO hashtags (name, bark_count) VALUES (?, 1) 
                 ON DUPLICATE KEY UPDATE bark_count = bark_count + 1",
                [$tagLower]
            );
            
            $hashtagId = DB::fetchOne("SELECT id FROM hashtags WHERE name = ?", [$tagLower]);
            if ($hashtagId) {
                DB::execute(
                    "INSERT IGNORE INTO bark_hashtags (bark_id, hashtag_id) VALUES (?, ?)",
                    [$barkId, $hashtagId['id']]
                );
            }
        }

        DB::get()->commit();
        
        // Log de sécurité
        log_security($_SESSION['user_id'], 'bark_posted', "bark_id=$barkId");

        // Succès !
        echo json_encode(['success' => true, 'bark_id' => $barkId]);
        
    } catch (PDOException $e) {
        DB::get()->rollBack();
        error_log("Bark post error: " . $e->getMessage());
        echo json_encode(['error' => 'Database error occurred']);
    }
    exit;
}

// ============================================================
// ACTION: DELETE BARK
// ============================================================
if ($action === 'delete' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        json_response(['error' => 'Invalid CSRF token'], 403);
    }
    $barkId = (int)($_POST['bark_id'] ?? 0);
    
    if (!$barkId) {
        echo json_encode(['error' => 'Bark ID required']);
        exit;
    }
    
    try {
        DB::get()->beginTransaction();
        
        // Vérifier que le bark appartient bien à l'utilisateur
        $bark = DB::fetchOne(
            "SELECT id, poll_id FROM barks WHERE id = ? AND user_id = ? AND is_deleted = 0", 
            [$barkId, $_SESSION['user_id']]
        );
        
        if ($bark) {
            // Si le bark a un sondage associé, le supprimer aussi
            if ($bark['poll_id']) {
                DB::execute("DELETE FROM polls WHERE id = ?", [$bark['poll_id']]);
            }
            
            // Supprimer les médias associés physiquement
            $mediaFiles = DB::fetchAll("SELECT url FROM media WHERE bark_id = ?", [$barkId]);
            foreach ($mediaFiles as $media) {
                $filePath = __DIR__ . '/../assets/uploads/media/' . $media['url'];
                if (file_exists($filePath)) {
                    unlink($filePath);
                }
            }
            
            // Supprimer les entrées media de la base
            DB::execute("DELETE FROM media WHERE bark_id = ?", [$barkId]);
            
            // Supprimer les associations hashtags
            DB::execute("DELETE FROM bark_hashtags WHERE bark_id = ?", [$barkId]);
            
            // Supprimer les yaps associés
            DB::execute("DELETE FROM yaps WHERE bark_id = ?", [$barkId]);
            
            // Supprimer les paws associés
            DB::execute("DELETE FROM paws WHERE bark_id = ?", [$barkId]);
            
            // Supprimer les rebarks associés
            DB::execute("DELETE FROM rebarks WHERE bark_id = ?", [$barkId]);
            
            // Supprimer les buries associés
            DB::execute("DELETE FROM buries WHERE bark_id = ?", [$barkId]);
            
            // Supprimer le bark
            DB::execute("DELETE FROM barks WHERE id = ?", [$barkId]);
            
            DB::get()->commit();
            log_security($_SESSION['user_id'], 'bark_deleted', "bark_id=$barkId");
            echo json_encode(['success' => true]);
        } else {
            echo json_encode(['error' => 'Bark not found or not authorized']);
        }
    } catch (PDOException $e) {
        DB::get()->rollBack();
        error_log("Bark delete error: " . $e->getMessage());
        echo json_encode(['error' => 'Database error occurred']);
    }
    exit;
}

// ============================================================
// ACTION: EDIT BARK - VERSION CORRIGÉE (sans html_entity_decode)
// ============================================================
if ($action === 'edit' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        json_response(['error' => 'Invalid CSRF token'], 403);
    }
    $barkId = (int)($_POST['bark_id'] ?? 0);
    $content = $_POST['content'] ?? '';
    $content = trim($content);
    
    if (!$barkId) {
        echo json_encode(['error' => 'Bark ID required']);
        exit;
    }
    
    // ⚠️ NE PAS utiliser html_entity_decode ici non plus
    
    // Validation
    if (empty($content)) {
        echo json_encode(['error' => 'Content is required']);
        exit;
    }
    
    if (mb_strlen($content) > 300) {
        echo json_encode(['error' => 'Content too long (max 300 characters)']);
        exit;
    }
    
    try {
        DB::get()->beginTransaction();
        
        // Vérifier que le bark appartient à l'utilisateur et n'est pas supprimé
        $bark = DB::fetchOne(
            "SELECT id, content FROM barks WHERE id = ? AND user_id = ? AND is_deleted = 0",
            [$barkId, $_SESSION['user_id']]
        );
        
        if (!$bark) {
            echo json_encode(['error' => 'Bark not found or not authorized']);
            exit;
        }
        
        // Mettre à jour le contenu
        DB::execute(
            "UPDATE barks SET content = ? WHERE id = ?",
            [$content, $barkId]
        );
        
        // Mettre à jour les hashtags (supprimer les anciens et ajouter les nouveaux)
        // D'abord, récupérer les anciens hashtags pour décrémenter leurs compteurs
        $oldHashtags = DB::fetchAll(
            "SELECT h.name FROM bark_hashtags bh JOIN hashtags h ON h.id = bh.hashtag_id WHERE bh.bark_id = ?",
            [$barkId]
        );
        
        foreach ($oldHashtags as $oldTag) {
            DB::execute(
                "UPDATE hashtags SET bark_count = bark_count - 1 WHERE name = ? AND bark_count > 0",
                [$oldTag['name']]
            );
        }
        
        // Supprimer les anciennes associations
        DB::execute("DELETE FROM bark_hashtags WHERE bark_id = ?", [$barkId]);
        
        // Extraire et sauvegarder les nouveaux hashtags
        preg_match_all('/#([\p{L}\p{N}_]+)/u', $content, $matches);
        $hashtags = array_unique($matches[1]);
        
        foreach ($hashtags as $tag) {
            $tagLower = mb_strtolower($tag, 'UTF-8');
            
            DB::execute(
                "INSERT INTO hashtags (name, bark_count) VALUES (?, 1) 
                 ON DUPLICATE KEY UPDATE bark_count = bark_count + 1",
                [$tagLower]
            );
            
            $hashtagId = DB::fetchOne("SELECT id FROM hashtags WHERE name = ?", [$tagLower]);
            if ($hashtagId) {
                DB::execute(
                    "INSERT IGNORE INTO bark_hashtags (bark_id, hashtag_id) VALUES (?, ?)",
                    [$barkId, $hashtagId['id']]
                );
            }
        }
        
        DB::get()->commit();
        
        // Log de sécurité
        log_security($_SESSION['user_id'], 'bark_edited', "bark_id=$barkId");
        
        echo json_encode([
            'success' => true,
            'bark_id' => $barkId,
            'content' => $content
        ]);
        
    } catch (PDOException $e) {
        DB::get()->rollBack();
        error_log("Bark edit error: " . $e->getMessage());
        echo json_encode(['error' => 'Database error occurred']);
    }
    exit;
}

// ============================================================
// ACTION: GET BARK DETAILS
// ============================================================
if ($action === 'get' && $_SERVER['REQUEST_METHOD'] === 'GET') {
    $barkId = (int)($_GET['id'] ?? 0);
    
    if (!$barkId) {
        echo json_encode(['error' => 'Bark ID required']);
        exit;
    }
    
    try {
        $bark = DB::fetchOne("
            SELECT b.*, u.username, u.display_name, u.avatar, u.gender, u.is_verified,
                   (SELECT COUNT(*) FROM paws WHERE bark_id = b.id) AS paw_count,
                   (SELECT COUNT(*) FROM yaps WHERE bark_id = b.id) AS yap_count,
                   (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id) AS rebark_count
            FROM barks b
            JOIN users u ON u.id = b.user_id
            WHERE b.id = ? AND b.is_deleted = 0 AND u.is_banned = 0
        ", [$barkId]);
        
        if (!$bark) {
            echo json_encode(['error' => 'Bark not found']);
            exit;
        }
        
        // Récupérer les médias
        $media = DB::fetchAll("SELECT * FROM media WHERE bark_id = ?", [$barkId]);
        
        // Récupérer les hashtags
        $hashtags = DB::fetchAll("
            SELECT h.name FROM bark_hashtags bh 
            JOIN hashtags h ON h.id = bh.hashtag_id 
            WHERE bh.bark_id = ?
        ", [$barkId]);
        
        echo json_encode([
            'success' => true,
            'bark' => $bark,
            'media' => $media,
            'hashtags' => array_column($hashtags, 'name')
        ]);
        
    } catch (PDOException $e) {
        error_log("Bark get error: " . $e->getMessage());
        echo json_encode(['error' => 'Database error occurred']);
    }
    exit;
}

// ============================================================
// ACTION: GET USER BARKS
// ============================================================
if ($action === 'user_barks' && $_SERVER['REQUEST_METHOD'] === 'GET') {
    $userId = (int)($_GET['user_id'] ?? $_SESSION['user_id']);
    $limit = (int)($_GET['limit'] ?? 20);
    $offset = (int)($_GET['offset'] ?? 0);
    
    try {
        $barks = DB::fetchAll("
            SELECT b.*, u.username, u.display_name, u.avatar, u.gender, u.is_verified,
                   (SELECT COUNT(*) FROM paws WHERE bark_id = b.id) AS paw_count,
                   (SELECT COUNT(*) FROM yaps WHERE bark_id = b.id) AS yap_count,
                   (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id) AS rebark_count,
                   (SELECT COUNT(*) FROM paws WHERE bark_id = b.id AND user_id = ?) AS user_pawed,
                   (SELECT COUNT(*) FROM rebarks WHERE bark_id = b.id AND user_id = ?) AS user_rebarked
            FROM barks b
            JOIN users u ON u.id = b.user_id
            WHERE b.user_id = ? AND b.is_deleted = 0 AND u.is_banned = 0
            ORDER BY b.created_at DESC
            LIMIT ? OFFSET ?
        ", [$_SESSION['user_id'], $_SESSION['user_id'], $userId, $limit, $offset]);
        
        echo json_encode([
            'success' => true,
            'barks' => $barks,
            'count' => count($barks)
        ]);
        
    } catch (PDOException $e) {
        error_log("User barks error: " . $e->getMessage());
        echo json_encode(['error' => 'Database error occurred']);
    }
    exit;
}

// Si aucune action valide n'est trouvée
echo json_encode(['error' => 'Invalid action']);