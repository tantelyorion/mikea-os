<?php
// ============================================================
// BARKER API — Poll Management - VERSION COMPLÈTE CORRIGÉE
// ============================================================
require_once dirname(__DIR__) . '/includes/init.php';
require_login();

// Désactiver l'affichage des erreurs pour l'API
error_reporting(0);
ini_set('display_errors', 0);

// Nettoyer tout buffer existant
if (ob_get_level()) ob_clean();
header('Content-Type: application/json');
header('X-Content-Type-Options: nosniff');

$action = $_GET['action'] ?? '';

// ============================================================
// ACTION: CREATE POLL - VERSION CORRIGÉE
// ============================================================
if ($action === 'create' && $_SERVER['REQUEST_METHOD'] === 'POST') {

    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        json_response(['error' => 'Invalid CSRF token'], 403);
    }

    $pollType = $_POST['poll_type'] ?? 'single';
    $question = trim($_POST['question'] ?? '');
    $options = $_POST['options'] ?? [];
    $expiresDays = (int)($_POST['expires_days'] ?? 0);
    $allowAnonymousVote = isset($_POST['allow_anonymous_vote']) ? 1 : 0;
    $content = trim($_POST['bark_content'] ?? '');
    $isAnon = isset($_POST['is_anonymous']) && $_POST['is_anonymous'] == 'on' ? 1 : 0;
    
    // Validation
    if (empty($question)) {
        echo json_encode(['error' => 'Question is required']);
        exit;
    }
    
    if (mb_strlen($question) > 300) {
        echo json_encode(['error' => 'Question too long (max 300 chars)']);
        exit;
    }
    
    // Validation selon type de sondage
    if ($pollType === 'rating') {
        $options = [];
    } elseif ($pollType === 'duel') {
        $options = array_filter($options, function($opt) {
            return !empty(trim($opt));
        });
        $options = array_values($options);
        
        if (count($options) !== 2) {
            echo json_encode(['error' => 'Duel polls require exactly 2 options, got ' . count($options)]);
            exit;
        }
    } else {
        $options = array_filter($options, function($opt) {
            return !empty(trim($opt));
        });
        $options = array_values($options);
        
        if (count($options) < 2) {
            echo json_encode(['error' => 'Minimum 2 options required']);
            exit;
        }
        if (count($options) > 10) {
            echo json_encode(['error' => 'Maximum 10 options allowed']);
            exit;
        }
    }
    
    // ============================================================
    // GESTION DES IMAGES POUR RATING
    // ============================================================
    $ratingMedia = null;
    $ratingMediaType = null;
    
    if ($pollType === 'rating' && isset($_FILES['rating_media'])) {
        $uploadDir = __DIR__ . '/../assets/uploads/polls/';
        $file = $_FILES['rating_media'];

        if ($file['error'] === UPLOAD_ERR_OK && !empty($file['name'])) {
            $maxImageBytes = 10 * 1024 * 1024;   // 10 Mo
            $maxVideoBytes = 100 * 1024 * 1024;  // 100 Mo (avant compression)

            $result = barker_store_image($file['tmp_name'], $uploadDir, 'rating', $maxImageBytes);
            if ($result) {
                $ratingMedia = $result['filename'];
                $ratingMediaType = 'image';
            } else {
                $result = barker_store_video($file['tmp_name'], $uploadDir, 'rating', $maxVideoBytes);
                if ($result) {
                    $ratingMedia = $result['filename'];
                    $ratingMediaType = 'video';
                }
            }
        }
    }
    
    // ============================================================
    // GESTION DES IMAGES POUR DUEL
    // Champs distincts : option_image_1 et option_image_2.
    // Important : si une image est fournie mais échoue (trop lourde,
    // format refusé, ou rejetée par php.ini avant même d'arriver ici),
    // on bloque la création avec une erreur claire plutôt que de créer
    // un Duel silencieusement amputé d'une image — c'est ce qui rendait
    // le Duel inutilisable (une seule image affichée sur les deux).
    // ============================================================
    $duelImages = [];
    if ($pollType === 'duel') {
        $uploadDir = __DIR__ . '/../assets/uploads/polls/';
        $maxImageBytes = 5 * 1024 * 1024; // 5 Mo

        $phpUploadErrors = [
            UPLOAD_ERR_INI_SIZE   => 'the file exceeds the server upload size limit',
            UPLOAD_ERR_FORM_SIZE  => 'the file exceeds the form upload size limit',
            UPLOAD_ERR_PARTIAL    => 'the file was only partially uploaded (connection interrupted)',
            UPLOAD_ERR_NO_TMP_DIR => 'server is missing a temporary folder',
            UPLOAD_ERR_CANT_WRITE => 'server failed to write the file to disk',
            UPLOAD_ERR_EXTENSION  => 'a server extension blocked the upload',
        ];

        foreach ([1 => 'option_image_1', 2 => 'option_image_2'] as $optionNum => $fieldName) {
            if (empty($_FILES[$fieldName]) || empty($_FILES[$fieldName]['name'])) {
                continue; // pas d'image fournie pour cette option, c'est autorisé
            }

            $fileError = $_FILES[$fieldName]['error'];
            if ($fileError !== UPLOAD_ERR_OK) {
                $reason = $phpUploadErrors[$fileError] ?? 'unknown upload error';
                echo json_encode(['error' => "Image for option $optionNum failed to upload ($reason). Please try a smaller image."]);
                exit;
            }

            $uploadErr = null;
            $result = barker_store_image($_FILES[$fieldName]['tmp_name'], $uploadDir, 'duel_opt' . $optionNum, $maxImageBytes, $uploadErr);
            if (!$result) {
                echo json_encode(['error' => "Image for option $optionNum was rejected (" . ($uploadErr ?? 'invalid_image') . "). Please use a JPG, PNG, GIF or WebP under 5MB."]);
                exit;
            }

            $duelImages[$optionNum - 1] = $result['filename'];
        }
    }
    
    try {
        DB::get()->beginTransaction();
        
        // 1. Créer le bark
        $barkId = DB::insert(
            "INSERT INTO barks (user_id, content, is_anonymous, bark_type) VALUES (?, ?, ?, 'normal')",
            [$_SESSION['user_id'], $content, $isAnon]
        );
        
        // 2. Créer le sondage
        $expiresAt = null;
        if ($expiresDays > 0) {
            $expiresAt = date('Y-m-d H:i:s', strtotime("+$expiresDays days"));
        }
        
        $pollId = DB::insert(
            "INSERT INTO polls (bark_id, user_id, poll_type, question, expires_at, allow_anonymous_vote, media_url, media_type) 
             VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            [$barkId, $_SESSION['user_id'], $pollType, $question, $expiresAt, $allowAnonymousVote, $ratingMedia, $ratingMediaType]
        );
        
        // 3. Lier le sondage au bark
        DB::execute("UPDATE barks SET poll_id = ? WHERE id = ?", [$pollId, $barkId]);
        
        // ============================================================
        // 4. CRÉER LES OPTIONS
        // ============================================================
        if ($pollType === 'rating') {
            for ($i = 1; $i <= 5; $i++) {
                DB::insert(
                    "INSERT INTO poll_options (poll_id, option_text, option_order) VALUES (?, ?, ?)",
                    [$pollId, $i . ' étoile' . ($i > 1 ? 's' : ''), $i]
                );
            }
        } elseif ($pollType === 'duel') {
            // Option 1 (index 0) - avec son image propre
            $option1Text = isset($options[0]) ? trim($options[0]) : '';
            $image1 = isset($duelImages[0]) ? $duelImages[0] : null;
            DB::insert(
                "INSERT INTO poll_options (poll_id, option_text, option_order, image_url) VALUES (?, ?, ?, ?)",
                [$pollId, $option1Text, 0, $image1]
            );
            
            // Option 2 (index 1) - avec son image propre
            $option2Text = isset($options[1]) ? trim($options[1]) : '';
            $image2 = isset($duelImages[1]) ? $duelImages[1] : null;
            DB::insert(
                "INSERT INTO poll_options (poll_id, option_text, option_order, image_url) VALUES (?, ?, ?, ?)",
                [$pollId, $option2Text, 1, $image2]
            );
        } else {
            foreach ($options as $idx => $optText) {
                DB::insert(
                    "INSERT INTO poll_options (poll_id, option_text, option_order) VALUES (?, ?, ?)",
                    [$pollId, trim($optText), $idx]
                );
            }
        }
        
        // 5. Extraire hashtags
        $fullContent = $content . ' ' . $question;
        preg_match_all('/#([\p{L}\p{N}_]+)/u', $fullContent, $matches);
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
        
        echo json_encode([
            'success' => true, 
            'bark_id' => $barkId, 
            'poll_id' => $pollId,
            'media_url' => $ratingMedia,
            'media_type' => $ratingMediaType
        ]);
        
    } catch (PDOException $e) {
        DB::get()->rollBack();
        error_log("Poll creation PDO error: " . $e->getMessage());
        error_log('Poll creation PDO error (detail): ' . $e->getMessage());
        echo json_encode(['error' => 'Database error']);
    } catch (Exception $e) {
        DB::get()->rollBack();
        error_log("Poll creation error: " . $e->getMessage());
        echo json_encode(['error' => 'An error occurred while creating the poll']);
    }
    exit;
}

// ============================================================
// ACTION: GET POLL DATA
// ============================================================
if ($action === 'get' && $_SERVER['REQUEST_METHOD'] === 'GET') {
    $pollId = (int)($_GET['poll_id'] ?? 0);
    
    if (!$pollId) {
        echo json_encode(['error' => 'Poll ID required']);
        exit;
    }
    
    $poll = DB::fetchOne("
        SELECT p.*, u.username, u.display_name, u.avatar, u.gender,
               b.content as bark_content, b.is_anonymous as bark_is_anonymous
        FROM polls p
        JOIN users u ON u.id = p.user_id
        JOIN barks b ON b.id = p.bark_id
        WHERE p.id = ? AND b.is_deleted = 0
    ", [$pollId]);
    
    if (!$poll) {
        echo json_encode(['error' => 'Poll not found']);
        exit;
    }
    
    $options = DB::fetchAll("
        SELECT * FROM poll_options 
        WHERE poll_id = ? 
        ORDER BY option_order ASC
    ", [$pollId]);
    
    $userVote = null;
    if (is_logged_in()) {
        $userVote = DB::fetchOne("
            SELECT option_id, rating_value FROM poll_votes 
            WHERE poll_id = ? AND user_id = ?
            LIMIT 1
        ", [$pollId, $_SESSION['user_id']]);
    } else {
        $userVote = DB::fetchOne("
            SELECT option_id, rating_value FROM poll_votes 
            WHERE poll_id = ? AND ip_address = ?
            LIMIT 1
        ", [$pollId, get_client_ip()]);
    }
    
    $userVoted = !is_null($userVote);
    $userChoice = $userVote ? $userVote['option_id'] : null;
    $userRating = $userVote ? $userVote['rating_value'] : null;
    
    $totalVotes = (int)$poll['total_votes'];
    foreach ($options as &$opt) {
        $opt['percentage'] = $totalVotes > 0 ? round(($opt['vote_count'] / $totalVotes) * 100) : 0;
    }
    unset($opt); // casse la référence (bonne pratique, évite toute corruption si ce code est réutilisé plus loin)
    
    $isExpired = false;
    if ($poll['expires_at'] && strtotime($poll['expires_at']) < time()) {
        $isExpired = true;
    }
    
    echo json_encode([
        'success' => true,
        'poll' => $poll,
        'options' => $options,
        'user_voted' => $userVoted,
        'user_choice' => $userChoice,
        'user_rating' => $userRating,
        'total_votes' => $totalVotes,
        'is_expired' => $isExpired
    ]);
    exit;
}

// ============================================================
// ACTION: VOTE
// ============================================================
if ($action === 'vote' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        json_response(['error' => 'Invalid CSRF token'], 403);
    }
    $pollId = (int)($_POST['poll_id'] ?? 0);
    $optionIds = $_POST['option_ids'] ?? [];
    $singleOptionId = (int)($_POST['option_id'] ?? 0);
    $ratingValue = (int)($_POST['rating_value'] ?? 0);
    
    if (!$pollId) {
        echo json_encode(['error' => 'Poll ID required']);
        exit;
    }
    
    $poll = DB::fetchOne("SELECT * FROM polls WHERE id = ?", [$pollId]);
    if (!$poll) {
        echo json_encode(['error' => 'Poll not found']);
        exit;
    }
    
    if ($poll['expires_at'] && strtotime($poll['expires_at']) < time()) {
        echo json_encode(['error' => 'This poll has expired']);
        exit;
    }
    
    $userId = is_logged_in() ? $_SESSION['user_id'] : null;
    $ipAddress = get_client_ip();
    
    // Vérifier si l'utilisateur a déjà voté
    $existingVote = null;
    if ($userId) {
        $existingVote = DB::fetchOne(
            "SELECT id FROM poll_votes WHERE poll_id = ? AND user_id = ?",
            [$pollId, $userId]
        );
    } else {
        $existingVote = DB::fetchOne(
            "SELECT id FROM poll_votes WHERE poll_id = ? AND ip_address = ? AND user_id IS NULL",
            [$pollId, $ipAddress]
        );
    }
    
    if ($existingVote && !$poll['allow_anonymous_vote']) {
        echo json_encode(['error' => 'You have already voted on this poll']);
        exit;
    }
    
    try {
        DB::get()->beginTransaction();
        
        if ($poll['poll_type'] === 'rating') {
            if ($ratingValue < 1 || $ratingValue > 5) {
                echo json_encode(['error' => 'Invalid rating value']);
                exit;
            }
            
            if ($existingVote) {
                $oldVote = DB::fetchOne("SELECT option_id FROM poll_votes WHERE id = ?", [$existingVote['id']]);
                if ($oldVote) {
                    DB::execute("UPDATE poll_options SET vote_count = vote_count - 1 WHERE id = ?", [$oldVote['option_id']]);
                }
                DB::execute("DELETE FROM poll_votes WHERE id = ?", [$existingVote['id']]);
            } else {
                DB::execute("UPDATE polls SET total_votes = total_votes + 1 WHERE id = ?", [$pollId]);
            }
            
            $option = DB::fetchOne("SELECT id FROM poll_options WHERE poll_id = ? AND option_order = ?", [$pollId, $ratingValue]);
            
            if ($option) {
                DB::execute("UPDATE poll_options SET vote_count = vote_count + 1 WHERE id = ?", [$option['id']]);
                DB::insert(
                    "INSERT INTO poll_votes (poll_id, option_id, user_id, rating_value, ip_address) VALUES (?, ?, ?, ?, ?)",
                    [$pollId, $option['id'], $userId, $ratingValue, $ipAddress]
                );
            }
            
        } elseif ($poll['poll_type'] === 'multiple') {
            if (empty($optionIds) || !is_array($optionIds)) {
                echo json_encode(['error' => 'Select at least one option']);
                exit;
            }
            
            if ($existingVote) {
                $oldOptions = DB::fetchAll("SELECT option_id FROM poll_votes WHERE poll_id = ? AND user_id = ?", [$pollId, $userId]);
                foreach ($oldOptions as $old) {
                    DB::execute("UPDATE poll_options SET vote_count = vote_count - 1 WHERE id = ?", [$old['option_id']]);
                }
                DB::execute("DELETE FROM poll_votes WHERE poll_id = ? AND user_id = ?", [$pollId, $userId]);
            } else {
                DB::execute("UPDATE polls SET total_votes = total_votes + 1 WHERE id = ?", [$pollId]);
            }
            
            foreach ($optionIds as $optId) {
                DB::execute("UPDATE poll_options SET vote_count = vote_count + 1 WHERE id = ?", [$optId]);
                DB::insert(
                    "INSERT INTO poll_votes (poll_id, option_id, user_id, ip_address) VALUES (?, ?, ?, ?)",
                    [$pollId, $optId, $userId, $ipAddress]
                );
            }
            
        } elseif ($poll['poll_type'] === 'duel') {
            if (!$singleOptionId) {
                echo json_encode(['error' => 'Select an option']);
                exit;
            }
            
            if ($existingVote) {
                $oldVote = DB::fetchOne("SELECT option_id FROM poll_votes WHERE id = ?", [$existingVote['id']]);
                if ($oldVote) {
                    DB::execute("UPDATE poll_options SET vote_count = vote_count - 1 WHERE id = ?", [$oldVote['option_id']]);
                }
                DB::execute("DELETE FROM poll_votes WHERE id = ?", [$existingVote['id']]);
            } else {
                DB::execute("UPDATE polls SET total_votes = total_votes + 1 WHERE id = ?", [$pollId]);
            }
            
            DB::execute("UPDATE poll_options SET vote_count = vote_count + 1 WHERE id = ?", [$singleOptionId]);
            DB::insert(
                "INSERT INTO poll_votes (poll_id, option_id, user_id, ip_address) VALUES (?, ?, ?, ?)",
                [$pollId, $singleOptionId, $userId, $ipAddress]
            );
            
        } else {
            // Single choice
            if (!$singleOptionId) {
                echo json_encode(['error' => 'Select an option']);
                exit;
            }
            
            if ($existingVote) {
                $oldVote = DB::fetchOne("SELECT option_id FROM poll_votes WHERE id = ?", [$existingVote['id']]);
                if ($oldVote) {
                    DB::execute("UPDATE poll_options SET vote_count = vote_count - 1 WHERE id = ?", [$oldVote['option_id']]);
                }
                DB::execute("DELETE FROM poll_votes WHERE id = ?", [$existingVote['id']]);
            } else {
                DB::execute("UPDATE polls SET total_votes = total_votes + 1 WHERE id = ?", [$pollId]);
            }
            
            DB::execute("UPDATE poll_options SET vote_count = vote_count + 1 WHERE id = ?", [$singleOptionId]);
            DB::insert(
                "INSERT INTO poll_votes (poll_id, option_id, user_id, ip_address) VALUES (?, ?, ?, ?)",
                [$pollId, $singleOptionId, $userId, $ipAddress]
            );
        }
        
        DB::get()->commit();
        
        $updatedOptions = DB::fetchAll("SELECT * FROM poll_options WHERE poll_id = ?", [$pollId]);
        $totalVotes = DB::fetchOne("SELECT total_votes FROM polls WHERE id = ?", [$pollId]);
        
        foreach ($updatedOptions as &$opt) {
            $opt['percentage'] = $totalVotes['total_votes'] > 0 ? round(($opt['vote_count'] / $totalVotes['total_votes']) * 100) : 0;
        }
        unset($opt); // casse la référence (bonne pratique)
        
        echo json_encode([
            'success' => true,
            'options' => $updatedOptions,
            'total_votes' => (int)$totalVotes['total_votes']
        ]);
        
    } catch (PDOException $e) {
        DB::get()->rollBack();
        error_log("Poll vote PDO error: " . $e->getMessage());
        echo json_encode(['error' => 'Database error occurred']);
    } catch (Exception $e) {
        DB::get()->rollBack();
        error_log("Poll vote error: " . $e->getMessage());
        echo json_encode(['error' => 'An error occurred while voting']);
    }
    exit;
}

// ============================================================
// ACTION: DELETE POLL
// ============================================================
if ($action === 'delete' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!verify_csrf($_POST['csrf_token'] ?? '')) {
        json_response(['error' => 'Invalid CSRF token'], 403);
    }
    $pollId = (int)($_POST['poll_id'] ?? 0);
    
    if (!$pollId) {
        echo json_encode(['error' => 'Poll ID required']);
        exit;
    }
    
    $poll = DB::fetchOne("SELECT id, bark_id FROM polls WHERE id = ? AND user_id = ?", [$pollId, $_SESSION['user_id']]);
    
    if (!$poll) {
        echo json_encode(['error' => 'Poll not found or not authorized']);
        exit;
    }
    
    try {
        DB::get()->beginTransaction();
        
        DB::execute("DELETE FROM polls WHERE id = ?", [$pollId]);
        DB::execute("UPDATE barks SET poll_id = NULL WHERE id = ?", [$poll['bark_id']]);
        
        DB::get()->commit();
        
        echo json_encode(['success' => true]);
        
    } catch (PDOException $e) {
        DB::get()->rollBack();
        error_log("Poll delete error: " . $e->getMessage());
        echo json_encode(['error' => 'Database error occurred']);
    }
    exit;
}

// ============================================================
// ACTION: GET USER POLLS
// ============================================================
if ($action === 'user_polls' && $_SERVER['REQUEST_METHOD'] === 'GET') {
    $userId = (int)($_GET['user_id'] ?? $_SESSION['user_id']);
    $limit = (int)($_GET['limit'] ?? 10);
    $offset = (int)($_GET['offset'] ?? 0);
    
    $polls = DB::fetchAll("
        SELECT p.*, b.content as bark_content, b.created_at as bark_created_at
        FROM polls p
        JOIN barks b ON b.id = p.bark_id
        WHERE p.user_id = ? AND b.is_deleted = 0
        ORDER BY b.created_at DESC
        LIMIT ? OFFSET ?
    ", [$userId, $limit, $offset]);
    
    foreach ($polls as &$poll) {
        $options = DB::fetchAll("
            SELECT * FROM poll_options 
            WHERE poll_id = ? 
            ORDER BY option_order ASC
        ", [$poll['id']]);
        
        $poll['options'] = $options;
        $poll['is_expired'] = $poll['expires_at'] && strtotime($poll['expires_at']) < time();
    }
    
    echo json_encode([
        'success' => true,
        'polls' => $polls,
        'count' => count($polls)
    ]);
    exit;
}

// Si aucune action valide n'est trouvée
echo json_encode(['error' => 'Invalid action']);