<?php
// ============================================
// facture.php - Gestion complète des factures
// AVEC DEMANDE DE REMISE ET VALIDATION
// IMPRESSION A4 PORTRAIT OPTIMISÉE (SANS GASPILLAGE)
// ============================================

require_once 'includes/config.php';
require_once 'includes/functions.php';
requireLogin();
requirePageAccess();
$db = getDB();

$action = $_GET['action'] ?? 'list';
$msg = '';
$error = '';
$currentUser = getCurrentUser();

// Logo de l'hôtel
$logoHotel = getParametreValeur('hotel_logo', '');
if (!empty($logoHotel) && !file_exists(__DIR__ . '/' . $logoHotel)) {
    $logoHotel = '';
}

// ============================================
// BUGFIX (12/09/2026) : le numéro de facture était construit comme
// PREFIXE-AAAAMMJJ-XXXX (date du jour + id réservation), donc
// STRICTEMENT IDENTIQUE si on générait une 2e facture de consommation
// pour la même réservation le même jour (ex : de nouvelles commandes
// terminées après une première facturation). MySQL rejetait alors
// l'insertion (numero_facture est UNIQUE) avec une erreur d'intégrité,
// remontée en HTTP 500 côté navigateur. Cette fonction garantit un
// numéro unique en ajoutant -2, -3... si la base est déjà prise
// (voir migration_11_elargir_numero_facture.sql pour la place
// nécessaire en base).
// ============================================
// BUGFIX (17/09/2026) : l'heure de check-in/check-out affichée sur la
// facture était toujours "00:00:00", car elle était lue sur
// reservations.date_arrivee / date_depart — des colonnes DATE qui ne
// contiennent qu'une date prévue de séjour, jamais une heure réelle.
// On affiche maintenant les vraies heures de check-in/check-out
// (reservations.checkin_le / checkout_le, voir migration_14), et on ne
// prétend pas connaître une heure quand le check-in/check-out n'a pas
// encore eu lieu (on affiche alors juste la date prévue, sans heure,
// avec la mention "prévu").
// ============================================
function afficherPeriodeSejour(?string $dateArrivee, ?string $dateDepart, ?string $checkinLe, ?string $checkoutLe): string {
    if (!$dateArrivee || !$dateDepart) {
        return '';
    }
    $arrivee = $checkinLe
        ? date('d/m/Y H:i', strtotime($checkinLe))
        : date('d/m/Y', strtotime($dateArrivee)) . ' (prévu)';
    $depart = $checkoutLe
        ? date('d/m/Y H:i', strtotime($checkoutLe))
        : date('d/m/Y', strtotime($dateDepart)) . ' (prévu)';
    return 'Du <strong>' . htmlspecialchars($arrivee) . '</strong> au <strong>' . htmlspecialchars($depart) . '</strong>';
}

function genererNumeroFactureUnique(PDO $db, string $base): string {
    $check = $db->prepare('SELECT 1 FROM factures WHERE numero_facture = ? LIMIT 1');
    $numero = $base;
    $suffixe = 2;
    while (true) {
        $check->execute([$numero]);
        if (!$check->fetch()) {
            return $numero;
        }
        $numero = $base . '-' . $suffixe;
        $suffixe++;
    }
}

// ============================================
// 1. GENERER UNE FACTURE
// ============================================
if ($action === 'generer' && isset($_GET['reservation_id']) && $_SERVER['REQUEST_METHOD'] === 'POST') {
    verifyCsrfToken();
    $reservation_id = intval($_GET['reservation_id']);

    // BUGFIX (12/09/2026) : ce bloc n'avait aucun try/catch — la moindre
    // erreur SQL (comme le bug de concaténation TVA corrigé le même jour)
    // remontait donc en page HTTP 500 vide, sans aucun message exploitable
    // ni pour l'utilisateur ni pour le diagnostic. Le try/catch ci-dessous
    // affiche désormais le vrai message d'erreur sur facture.php au lieu
    // d'un écran 500 muet, si un problème survient à l'avenir.
    try {
    
    $res = $db->prepare("
        SELECT r.*, c.nom, c.prenom, c.email, c.telephone, c.adresse,
               ch.numero as chambre_numero, ch.type as chambre_type, ch.prix_nuit
        FROM reservations r
        JOIN clients c ON r.client_id = c.id
        LEFT JOIN chambres ch ON r.chambre_id = ch.id
        WHERE r.id = ?
    ");
    $res->execute([$reservation_id]);
    $reservation = $res->fetch();
    
    if (!$reservation) {
        header('Location: facture.php?error=' . urlencode('Réservation non trouvée'));
        exit;
    }

    // BUGFIX (audit 12/09/2026) : cette vérification bloquait TOUTE nouvelle
    // facturation dès qu'une facture existait déjà pour la réservation — même
    // une facture de blanchisserie ou une facture de consommation antérieure
    // n'ayant rien à voir avec l'hébergement. Résultat : toute commande
    // restaurant / room-service / petit-déjeuner devenue "terminée" APRÈS la
    // toute première facture de la réservation n'était plus jamais reprise
    // dans une facture — elle disparaissait purement et simplement, sans
    // jamais apparaître dans le détail d'aucune facture. On ne doit bloquer
    // que la RE-création de la facture d'HÉBERGEMENT (pour ne pas la
    // dupliquer si on clique deux fois) ; les nouvelles consommations,
    // elles, doivent toujours pouvoir être facturées, quel que soit le
    // nombre de factures déjà existantes pour cette réservation.
    $dejaFactureHotel = $db->prepare("SELECT id FROM factures WHERE reservation_id = ? AND type_facture = 'hotel' LIMIT 1");
    $dejaFactureHotel->execute([$reservation_id]);
    $dejaFactureHotel = $dejaFactureHotel->fetch();

    $commandes = $db->prepare("
        SELECT * FROM commandes 
        WHERE reservation_id = ? AND statut = 'termine'
        AND id NOT IN (SELECT commande_id FROM facture_lignes WHERE commande_id IS NOT NULL)
    ");
    $commandes->execute([$reservation_id]);
    $commandes = $commandes->fetchAll();
    
    $nuits = (strtotime($reservation['date_depart']) - strtotime($reservation['date_arrivee'])) / 86400;
    $facturesCreees = [];

    // --- Facture Hébergement ---
    // BUGFIX : ne (re)créer la facture d'hébergement que si aucune n'existe
    // déjà pour cette réservation (sinon elle serait dupliquée à chaque clic).
    if (!$dejaFactureHotel && $reservation['prix_total'] > 0) {
        // CORRECTIF (TVA hébergement incluse) : le prix de la chambre
        // (reservations.prix_total, basé sur chambres.prix_nuit) est un prix
        // TTC — la TVA à TVA_TAUX est déjà comprise dedans, le client ne
        // paie jamais "prix chambre + TVA en plus" pour l'hébergement.
        // Avant ce correctif, le code faisait $sousTotal + $tva, ce qui
        // facturait la TVA une seconde fois par-dessus un prix qui la
        // contenait déjà. On extrait maintenant la TVA du prix TTC au lieu
        // de l'ajouter : $sousTotalHT = TTC / (1 + taux), $tva = TTC - HT.
        // La vignette et la taxe de séjour restent des suppléments distincts,
        // non soumis à la TVA, payés en plus du prix de la chambre.
        //
        // BUGFIX (17/09/2026 - tarif chambre non à jour en facturation) :
        // ce montant venait auparavant TOUJOURS de reservations.prix_total,
        // qui est figé au tarif de la chambre tel qu'il était au moment de
        // LA CRÉATION de la réservation. Si le prix de la chambre est
        // ensuite modifié dans Chambres > Modifier (ce qui ne met à jour
        // que chambres.prix_nuit, pas les réservations déjà créées), toute
        // réservation créée avant ce changement continuait à être facturée
        // à l'ancien tarif, sans lien avec le prix actuellement affiché sur
        // la fiche de la chambre — parfois avec un écart de plusieurs
        // dizaines de %, selon l'ampleur du changement de tarif. Certaines
        // réservations semblaient "correctes" (celles créées ou déplacées
        // après le changement de tarif) et d'autres non, d'où l'impression
        // que seules certaines chambres étaient concernées.
        // On recalcule donc maintenant le montant dû à partir du tarif
        // ACTUEL de la chambre (chambres.prix_nuit, déjà chargé ci-dessus
        // via la jointure sur `ch`), avec la même formule que celle utilisée
        // à la création/au déplacement d'une réservation (reservations.php,
        // planning.php). Exception : les réservations importées d'une
        // plateforme externe (source_plateforme != 'direct') gardent leur
        // prix_total tel quel, car ce montant vient de l'OTA et n'a aucun
        // rapport avec le tarif local de la chambre.
        $sourcePlateforme = $reservation['source_plateforme'] ?? 'direct';
        if ($sourcePlateforme === 'direct' && isset($reservation['prix_nuit']) && $reservation['prix_nuit'] !== null) {
            $prixNuitActuel = (float) $reservation['prix_nuit'];
            if (($reservation['type_sejour'] ?? 'nuit') === 'day_use') {
                $pctRow = $db->query("SELECT valeur FROM parametres WHERE cle = 'day_use_pourcentage'")->fetch();
                $dayUsePourcentage = $pctRow ? floatval($pctRow['valeur']) : 50;
                $sousTotalTTC = round($prixNuitActuel * ($dayUsePourcentage / 100), 2);
            } else {
                $sousTotalTTC = round($prixNuitActuel * $nuits, 2);
            }
        } else {
            $sousTotalTTC = $reservation['prix_total'];
        }
        $nbNuitsFacturables = max(1, $nuits);
        $tauxTva = TVA_TAUX;
        $montantVignette = 1500;
        $montantTaxeSejour = 1000;

        $tva = round($sousTotalTTC * $tauxTva / (1 + $tauxTva), 2);
        $sousTotal = round($sousTotalTTC - $tva, 2); // prix chambre HT (hors TVA), pour affichage/compta
        $vignette = $montantVignette * $nbNuitsFacturables;
        $taxeSejour = $montantTaxeSejour * $nbNuitsFacturables;
        $total = $sousTotalTTC + $vignette + $taxeSejour; // TVA déjà incluse dans $sousTotalTTC, ne pas la rajouter
        $numFactureHotel = genererNumeroFactureUnique($db, 'FACT-H-' . date('Ymd') . '-' . str_pad($reservation_id, 4, '0', STR_PAD_LEFT));

        $depotDejaVerse = min((float) $reservation['montant_paye'], $total);
        $resteInitial = round($total - $depotDejaVerse, 2);
        $statutInitial = $resteInitial <= 0.01 ? 'payee' : ($depotDejaVerse > 0 ? 'partielle' : 'en_attente');

        $stmt = $db->prepare("
            INSERT INTO factures (numero_facture, reservation_id, client_id, chambre_id, type_facture, date_facture, 
                                  sous_total, tva, total_ttc, montant_paye, reste_a_payer, statut, mode_paiement, operateur_mobile_money)
            VALUES (?, ?, ?, ?, 'hotel', NOW(), ?, ?, ?, ?, ?, ?, ?, ?)
        ");
        $stmt->execute([
            $numFactureHotel, 
            $reservation_id, 
            $reservation['client_id'], 
            $reservation['chambre_id'],
            $sousTotal, 
            $tva, 
            $total, 
            $depotDejaVerse,
            $resteInitial,
            $statutInitial,
            $reservation['mode_paiement'] ?? 'especes',
            $reservation['operateur_mobile_money'] ?? null
        ]);
        $factureHotelId = $db->lastInsertId();

        if ($depotDejaVerse > 0) {
            $db->prepare("INSERT INTO facture_paiements (facture_id, montant, mode_paiement, operateur_mobile_money, notes) VALUES (?, ?, ?, ?, ?)")
               ->execute([$factureHotelId, $depotDejaVerse, $reservation['mode_paiement'] ?? 'especes', $reservation['operateur_mobile_money'] ?? null, 'Acompte versé à la réservation']);
        }

        $descriptionSejour = 'Séjour du ' . date('d/m/Y', strtotime($reservation['date_arrivee'])) . ' au ' . date('d/m/Y', strtotime($reservation['date_depart']));
        $db->prepare("INSERT INTO facture_lignes (facture_id, type_ligne, description, quantite, prix_unitaire, total) 
                      VALUES (?, 'sejour', ?, ?, ?, ?)")
           ->execute([$factureHotelId, $descriptionSejour, $nbNuitsFacturables, $nuits > 0 ? $sousTotal / $nuits : $sousTotal, $sousTotal]);

        // BUGFIX (12/09/2026) : la concaténation PHP " . round($tauxTva * 100) . "
        // était écrite À L'INTÉRIEUR de la chaîne SQL entre guillemets doubles,
        // donc jamais évaluée par PHP — elle partait telle quelle (littéralement
        // avec un point) dans la requête envoyée à MySQL, qui la refusait avec
        // une erreur de syntaxe (exception PDO non interceptée ici) => HTTP 500
        // à CHAQUE génération d'une facture d'hébergement (donc à chaque
        // nouvelle réservation facturée, comme la réservation 23). Le libellé
        // "TVA (20%)" doit être construit en PHP puis passé en paramètre lié.
        $db->prepare("INSERT INTO facture_lignes (facture_id, type_ligne, description, quantite, prix_unitaire, total) 
                      VALUES (?, 'taxe', ?, 1, ?, ?)")
           ->execute([$factureHotelId, 'TVA (' . round($tauxTva * 100) . '%) incluse', $tva, $tva]);

        $db->prepare("INSERT INTO facture_lignes (facture_id, type_ligne, description, quantite, prix_unitaire, total) 
                      VALUES (?, 'taxe', 'Vignette', ?, ?, ?)")
           ->execute([$factureHotelId, $nbNuitsFacturables, $montantVignette, $vignette]);

        $db->prepare("INSERT INTO facture_lignes (facture_id, type_ligne, description, quantite, prix_unitaire, total) 
                      VALUES (?, 'taxe', 'Taxe de séjour', ?, ?, ?)")
           ->execute([$factureHotelId, $nbNuitsFacturables, $montantTaxeSejour, $taxeSejour]);

        $facturesCreees['hotel'] = $factureHotelId;
    }

    // --- Facture Consommation (restaurant/bar/room service) ---
    // Les consommations (restaurant, bar, room-service, petit-déjeuner...)
    // sont facturées à prix direct : pas de TVA, pas de taxe de séjour, pas
    // de vignette. Seule l'hébergement (facture hôtel, plus bas) porte ces
    // suppléments. On aligne ainsi ce bloc sur vente_comptoir.php,
    // facture_comptoir.php et blanchisserie.php, qui ne facturent plus non
    // plus de TVA sur les consommations (révision du 21/09/2026).
    $lignesConso = [];
    $totalConso = 0;
    
    if (!empty($commandes)) {
        foreach ($commandes as $cmd) {
            // BUGFIX (audit 12/09/2026) : la table `commande_lignes` n'a NI
            // colonne `description` NI colonne `total` (voir database.sql :
            // elle a `menu_id` + `sous_total`). Le code lisait donc toujours
            // $ligne['description'] et $ligne['total'] comme NULL → repli
            // silencieux sur `?? 0`, si bien que $totalConso valait TOUJOURS
            // 0 et qu'aucune facture de consommation n'était jamais créée
            // (le bloc plus bas ne s'exécute que si $totalConso > 0). C'est
            // la cause principale pour laquelle le petit-déjeuner et les
            // autres commandes restaurant/room-service liées à un séjour
            // n'apparaissaient jamais dans aucune facture. On rejoint
            // maintenant la table `menu` (comme le fait déjà correctement
            // facture_comptoir.php) pour récupérer le nom du plat, et on lit
            // le bon nom de colonne `sous_total`.
            $lignesCmd = $db->prepare("
                SELECT cl.*, m.nom AS menu_nom
                FROM commande_lignes cl
                JOIN menu m ON cl.menu_id = m.id
                WHERE cl.commande_id = ?
            ");
            $lignesCmd->execute([$cmd['id']]);
            $lignes = $lignesCmd->fetchAll();
            foreach ($lignes as $ligne) {
                $lignesConso[] = [
                    'description' => $ligne['menu_nom'] ?? ('Commande #' . $cmd['numero_commande']),
                    'quantite' => $ligne['quantite'] ?? 1,
                    'prix_unitaire' => $ligne['prix_unitaire'] ?? 0,
                    'total' => $ligne['sous_total'] ?? 0,
                    'commande_id' => $cmd['id']
                ];
                $totalConso += $ligne['sous_total'] ?? 0;
            }
        }
    }

    if ($totalConso > 0) {
        $numFactureConso = genererNumeroFactureUnique($db, 'FACT-C-' . date('Ymd') . '-' . str_pad($reservation_id, 4, '0', STR_PAD_LEFT));
        $tvaConso = 0;
        $totalConsoTtc = $totalConso;

        // BUGFIX (mise à jour séparation Hébergement/Consommations) : cette
        // facture ne contient QUE des lignes de consommation (jamais de ligne
        // "séjour"), elle doit donc être enregistrée avec type_facture =
        // 'consommation' et non 'complet' — sinon elle disparaît à tort de la
        // nouvelle liste "Factures consommations" (factures.php), qui filtre
        // uniquement sur ce champ.
        $stmt = $db->prepare("
            INSERT INTO factures (numero_facture, reservation_id, client_id, chambre_id, type_facture, date_facture, 
                                  sous_total, tva, total_ttc, reste_a_payer, statut, mode_paiement, operateur_mobile_money)
            VALUES (?, ?, ?, ?, 'consommation', NOW(), ?, ?, ?, ?, 'en_attente', ?, ?)
        ");
        $stmt->execute([
            $numFactureConso, 
            $reservation_id, 
            $reservation['client_id'], 
            $reservation['chambre_id'],
            $totalConso, 
            $tvaConso,
            $totalConsoTtc, 
            $totalConsoTtc,
            $reservation['mode_paiement'] ?? 'especes',
            $reservation['operateur_mobile_money'] ?? null
        ]);
        $factureConsoId = $db->lastInsertId();

        foreach ($lignesConso as $ligne) {
            $db->prepare("INSERT INTO facture_lignes (facture_id, type_ligne, description, quantite, prix_unitaire, total, commande_id) 
                          VALUES (?, 'consommation', ?, ?, ?, ?, ?)")
               ->execute([
                   $factureConsoId, 
                   $ligne['description'], 
                   $ligne['quantite'], 
                   $ligne['prix_unitaire'], 
                   $ligne['total'], 
                   $ligne['commande_id'] ?? null
               ]);
        }

        $facturesCreees['consommation'] = $factureConsoId;
    }

    if (empty($facturesCreees)) {
        // Rien de NOUVEAU à facturer (aucune commande fraîchement terminée).
        // Si une facture d'hébergement existe déjà, on la montre plutôt que
        // d'afficher une erreur — sinon on prévient qu'il n'y a rien à faire.
        if ($dejaFactureHotel) {
            header('Location: facture.php?action=view&id=' . $dejaFactureHotel['id'] . '&msg=' . urlencode('info:Aucune nouvelle consommation à facturer pour cette réservation'));
        } else {
            header('Location: facture.php?error=' . urlencode('Rien à facturer pour cette réservation'));
        }
        exit;
    }

    $factureAAfficher = $facturesCreees['hotel'] ?? $facturesCreees['consommation'];
    header('Location: facture.php?action=view&id=' . $factureAAfficher . '&msg=' . urlencode('success:Facture(s) générée(s) avec succès'));
    exit;
    } catch (Exception $e) {
        if ($db->inTransaction()) {
            $db->rollBack();
        }
        header('Location: facture.php?error=' . urlencode('Erreur lors de la génération de la facture : ' . $e->getMessage()));
        exit;
    }
}

// ============================================
// 2. ENREGISTRER UN PAIEMENT
// ============================================
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['enregistrer_paiement'])) {
    verifyCsrfToken();
    $facture_id = intval($_POST['facture_id']);
    $montant = floatval($_POST['montant']);
    $mode_paiement = $_POST['mode_paiement'] ?? 'especes';
    $operateur_mobile_money = $_POST['operateur_mobile_money'] ?? null;
    $reference = trim($_POST['reference'] ?? '');
    $notes = trim($_POST['notes'] ?? '');

    // NOUVEAU (17/09/2026 - encaissement direct depuis les listes de
    // factures) : le paiement peut maintenant être enregistré depuis une
    // fenêtre modale sur factures.php ou sur la liste de facture.php
    // (action=list), sans quitter la page. Dans ce cas, un champ caché
    // "retour" contient l'URL de la page appelante, pour y revenir après
    // l'enregistrement au lieu d'atterrir systématiquement sur la fiche
    // facture (action=view). Par sécurité (éviter une redirection ouverte
    // vers un site externe forgée dans le formulaire), seule une URL
    // relative commençant par "factures.php" ou "facture.php" est
    // acceptée ; toute autre valeur retombe sur le comportement par
    // défaut (retour sur la fiche facture).
    $retour = $_POST['retour'] ?? '';
    $retourOk = is_string($retour)
        && $retour !== ''
        && preg_match('/^(factures\.php|facture\.php)(\?[^\s"\'<>]*)?$/', $retour) === 1;
    $urlRetourDefaut = 'facture.php?action=view&id=' . $facture_id;
    $urlRetour = $retourOk ? $retour : $urlRetourDefaut;
    $sep = (strpos($urlRetour, '?') !== false) ? '&' : '?';

    if ($mode_paiement === 'mobile_money' && !$operateur_mobile_money) {
        header('Location: ' . $urlRetour . $sep . 'msg=' . urlencode('danger:Veuillez préciser l\'opérateur Mobile Money'));
        exit;
    }

    $facture = $db->prepare("SELECT reservation_id, reste_a_payer FROM factures WHERE id = ?");
    $facture->execute([$facture_id]);
    $factureData = $facture->fetch();

    if (!$factureData) {
        header('Location: facture.php?error=' . urlencode('Facture non trouvée'));
        exit;
    }

    if ($montant <= 0) {
        header('Location: ' . $urlRetour . $sep . 'msg=' . urlencode('danger:Le montant doit être supérieur à 0'));
        exit;
    }

    if ($montant > $factureData['reste_a_payer'] + 0.01) {
        header('Location: ' . $urlRetour . $sep . 'msg=' . urlencode('danger:Le montant dépasse le reste à payer'));
        exit;
    }

    try {
        if ($factureData['reservation_id']) {
            enregistrerPaiementReservation($factureData['reservation_id'], $montant, $mode_paiement, $reference, $notes, $facture_id, $operateur_mobile_money);
        } else {
            $db->beginTransaction();
            $db->prepare("INSERT INTO facture_paiements (facture_id, montant, mode_paiement, operateur_mobile_money, reference, notes) VALUES (?, ?, ?, ?, ?, ?)")
               ->execute([$facture_id, $montant, $mode_paiement, $mode_paiement === 'mobile_money' ? $operateur_mobile_money : null, $reference, $notes]);
            $fd = $db->prepare("SELECT montant_paye, total_ttc FROM factures WHERE id = ?");
            $fd->execute([$facture_id]);
            $fdData = $fd->fetch();
            $nouveau_paye = $fdData['montant_paye'] + $montant;
            $reste = max(0, $fdData['total_ttc'] - $nouveau_paye);
            $statut = $reste <= 0.01 ? 'payee' : 'partielle';
            $db->prepare("UPDATE factures SET montant_paye = ?, reste_a_payer = ?, statut = ? WHERE id = ?")
               ->execute([$nouveau_paye, $reste, $statut, $facture_id]);
            if ($mode_paiement === 'especes') {
                $session = getSessionCaisseOuverte();
                if ($session) {
                    enregistrerMouvementCaisse($session['id'], 'encaissement', $montant, 'especes', 'Paiement facture ' . $facture_id, ['facture_id' => $facture_id]);
                }
            }
            $db->commit();
        }
    } catch (Exception $e) {
        if ($db->inTransaction()) {
            $db->rollBack();
        }
        header('Location: ' . $urlRetour . $sep . 'msg=' . urlencode('danger:' . $e->getMessage()));
        exit;
    }

    $db->prepare("INSERT INTO logs_activite (user_id, action, table_name, record_id, details) 
                   VALUES (?, 'paiement', 'factures', ?, ?)")
       ->execute([$_SESSION['user_id'], $facture_id, "Paiement de " . formatMontant($montant) . " enregistré"]);
    
    header('Location: ' . $urlRetour . $sep . 'msg=' . urlencode('success:Paiement enregistré avec succès'));
    exit;
}

// ============================================
// 3. DEMANDER UNE REMISE
// ============================================
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['demander_remise'])) {
    verifyCsrfToken();
    $facture_id = intval($_POST['facture_id']);
    $montant_demande = floatval($_POST['montant_demande']);
    $motif = trim($_POST['motif_demande'] ?? '');
    
    if ($motif === '' || $montant_demande <= 0) {
        header('Location: facture.php?action=view&id=' . $facture_id . '&msg=' . urlencode('danger:Veuillez saisir un motif et un montant valide'));
        exit;
    }
    
    $facture = $db->prepare("SELECT reste_a_payer, client_id, reservation_id FROM factures WHERE id = ?");
    $facture->execute([$facture_id]);
    $f = $facture->fetch();
    
    if (!$f) {
        header('Location: facture.php?error=' . urlencode('Facture non trouvée'));
        exit;
    }
    
    if ($montant_demande > $f['reste_a_payer']) {
        header('Location: facture.php?action=view&id=' . $facture_id . '&msg=' . urlencode('danger:Le montant demandé ne peut pas dépasser le reste à payer'));
        exit;
    }
    
    $check = $db->prepare("SELECT id FROM demandes_remise WHERE facture_id = ? AND statut = 'en_attente'");
    $check->execute([$facture_id]);
    if ($check->fetch()) {
        header('Location: facture.php?action=view&id=' . $facture_id . '&msg=' . urlencode('danger:Une demande de remise est déjà en attente pour cette facture'));
        exit;
    }
    
    $stmt = $db->prepare("INSERT INTO demandes_remise (facture_id, client_id, reservation_id, montant_demande, motif, demande_par) 
                           VALUES (?, ?, ?, ?, ?, ?)");
    $stmt->execute([$facture_id, $f['client_id'], $f['reservation_id'], $montant_demande, $motif, $_SESSION['user_id']]);
    
    $db->prepare("INSERT INTO logs_activite (user_id, action, table_name, record_id, details) 
                   VALUES (?, 'demande_remise', 'factures', ?, ?)")
       ->execute([$_SESSION['user_id'], $facture_id, "Demande de remise de " . formatMontant($montant_demande) . " - Motif: " . $motif]);
    
    header('Location: facture.php?action=view&id=' . $facture_id . '&msg=' . urlencode('success:Demande de remise envoyée pour validation'));
    exit;
}

// ============================================
// 4. VALIDER UNE REMISE
// ============================================
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['valider_remise'])) {
    verifyCsrfToken();
    $currentUser = getCurrentUser();
    if (!$currentUser || !in_array($currentUser['role'], ['admin', 'manager'], true)) {
        header('Location: facture.php?action=view&id=' . intval($_POST['facture_id']) . '&msg=' . urlencode('danger:Seul un manager ou administrateur peut valider une remise'));
        exit;
    }
    
    $demande_id = intval($_POST['demande_id']);
    $action_remise = $_POST['action_remise'] ?? 'accepter';
    $facture_id = intval($_POST['facture_id']);
    
    $demande = $db->prepare("SELECT * FROM demandes_remise WHERE id = ?");
    $demande->execute([$demande_id]);
    $d = $demande->fetch();
    
    if (!$d) {
        header('Location: facture.php?error=' . urlencode('Demande non trouvée'));
        exit;
    }
    
    if ($d['statut'] !== 'en_attente') {
        header('Location: facture.php?action=view&id=' . $facture_id . '&msg=' . urlencode('danger:Cette demande a déjà été traitée'));
        exit;
    }
    
    if ($action_remise === 'accepter') {
        $db->prepare("UPDATE factures SET 
                       remise = remise + ?, 
                       reste_a_payer = reste_a_payer - ?,
                       remise_acceptee = 1,
                       remise_motif = CONCAT(COALESCE(remise_motif,''), CHAR(10), 'Remise accordée: ', ?, ' (', ?, ')')
                       WHERE id = ?")
           ->execute([
               $d['montant_demande'], 
               $d['montant_demande'], 
               formatMontant($d['montant_demande']), 
               $d['motif'], 
               $facture_id
           ]);
        
        $facture = $db->prepare("SELECT reste_a_payer FROM factures WHERE id = ?");
        $facture->execute([$facture_id]);
        $reste = $facture->fetchColumn();
        
        if ($reste <= 0) {
            $db->prepare("UPDATE factures SET statut = 'payee' WHERE id = ?")->execute([$facture_id]);
        }
        
        $statut = 'acceptee';
        $msg = 'Remise accordée avec succès';
        $logAction = 'remise_acceptee';
    } else {
        $statut = 'refusee';
        $msg = 'Demande de remise refusée';
        $logAction = 'remise_refusee';
    }
    
    $db->prepare("UPDATE demandes_remise SET statut = ?, traite_par = ?, date_traitement = NOW() WHERE id = ?")
       ->execute([$statut, $_SESSION['user_id'], $demande_id]);
    
    $db->prepare("INSERT INTO logs_activite (user_id, action, table_name, record_id, details) 
                   VALUES (?, ?, 'demandes_remise', ?, ?)")
       ->execute([
           $_SESSION['user_id'], 
           $logAction, 
           $demande_id, 
           json_encode([
               'action' => $action_remise, 
               'montant' => $d['montant_demande'], 
               'motif' => $d['motif'],
               'traite_par' => $currentUser['prenom'] . ' ' . $currentUser['nom']
           ])
       ]);
    
    header('Location: facture.php?action=view&id=' . $facture_id . '&msg=' . urlencode('success:' . $msg));
    exit;
}

// ============================================
// 5. RECHERCHE DE RÉSERVATIONS (API)
// ============================================
if ($action === 'search_reservations' && isset($_GET['q'])) {
    header('Content-Type: application/json');
    $search = '%' . $_GET['q'] . '%';
    $stmt = $db->prepare("
        SELECT DISTINCT r.id, r.numero_reservation, r.date_arrivee, r.date_depart,
               c.nom, c.prenom, c.email, c.telephone,
               ch.numero as chambre_numero
        FROM reservations r
        JOIN clients c ON r.client_id = c.id
        LEFT JOIN chambres ch ON r.chambre_id = ch.id
        WHERE r.statut IN ('terminee', 'check_out', 'check_in')
          AND r.id NOT IN (SELECT reservation_id FROM factures WHERE reservation_id IS NOT NULL)
          AND (c.nom LIKE ? OR c.prenom LIKE ? OR c.email LIKE ? OR c.telephone LIKE ? OR r.numero_reservation LIKE ?)
        ORDER BY r.date_depart DESC
        LIMIT 20
    ");
    $stmt->execute([$search, $search, $search, $search, $search]);
    echo json_encode($stmt->fetchAll());
    exit;
}

// ============================================
// 6. SUPPRIMER UNE FACTURE
// ============================================
if ($action === 'delete' && isset($_GET['id']) && $_SERVER['REQUEST_METHOD'] === 'POST') {
    verifyCsrfToken();
    $currentUser = getCurrentUser();
    if (!$currentUser || $currentUser['role'] !== 'admin') {
        header('Location: facture.php?msg=' . urlencode('danger:Seul un administrateur peut supprimer une facture'));
        exit;
    }
    
    $facture_id = intval($_GET['id']);
    
    $facture = $db->prepare("SELECT statut, montant_paye FROM factures WHERE id = ?");
    $facture->execute([$facture_id]);
    $f = $facture->fetch();
    
    if ($f['montant_paye'] > 0) {
        header('Location: facture.php?msg=' . urlencode('danger:Impossible de supprimer une facture qui a déjà des paiements'));
        exit;
    }
    
    $db->prepare("DELETE FROM factures WHERE id = ?")->execute([$facture_id]);
    
    $db->prepare("INSERT INTO logs_activite (user_id, action, table_name, record_id, details) 
                   VALUES (?, 'delete', 'factures', ?, 'Facture supprimée')")
       ->execute([$_SESSION['user_id'], $facture_id]);
    
    header('Location: facture.php?msg=' . urlencode('success:Facture supprimée avec succès'));
    exit;
}

// ============================================
// 7. VUE DÉTAILLÉE D'UNE FACTURE
// ============================================
if ($action === 'view' && isset($_GET['id'])) {
    $facture_id = intval($_GET['id']);
    
    $facture = $db->prepare("
        SELECT f.*, c.nom, c.prenom, c.email, c.telephone, c.adresse,
               r.numero_reservation, r.date_arrivee, r.date_depart, r.checkin_le, r.checkout_le,
               ch.numero as chambre_numero, ch.type as chambre_type
        FROM factures f
        JOIN clients c ON f.client_id = c.id
        LEFT JOIN reservations r ON f.reservation_id = r.id
        LEFT JOIN chambres ch ON f.chambre_id = ch.id
        WHERE f.id = ?
    ");
    $facture->execute([$facture_id]);
    $facture = $facture->fetch();
    
    if (!$facture) {
        header('Location: facture.php?error=' . urlencode('Facture non trouvée'));
        exit;
    }
    
    $lignes = $db->prepare("SELECT * FROM facture_lignes WHERE facture_id = ? ORDER BY id");
    $lignes->execute([$facture_id]);
    $lignes = $lignes->fetchAll();
    
    $paiements = $db->prepare("SELECT * FROM facture_paiements WHERE facture_id = ? ORDER BY date_paiement DESC");
    $paiements->execute([$facture_id]);
    $paiements = $paiements->fetchAll();
    
    $demandes = $db->prepare("
        SELECT d.*, u.nom, u.prenom, u.role,
               t.nom as traiteur_nom, t.prenom as traiteur_prenom
        FROM demandes_remise d
        LEFT JOIN utilisateurs u ON d.demande_par = u.id
        LEFT JOIN utilisateurs t ON d.traite_par = t.id
        WHERE d.facture_id = ?
        ORDER BY d.date_demande DESC
    ");
    $demandes->execute([$facture_id]);
    $demandes = $demandes->fetchAll();
    
    $facturesLiees = [];
    if (!empty($facture['reservation_id'])) {
        $liees = $db->prepare("
            SELECT id, numero_facture, type_facture, total_ttc, montant_paye, reste_a_payer, statut, remise
            FROM factures 
            WHERE reservation_id = ? AND id != ?
        ");
        $liees->execute([$facture['reservation_id'], $facture_id]);
        $facturesLiees = $liees->fetchAll();
    }
    
    $typeFactureLabels = [
        'hotel' => 'FACTURE HÉBERGEMENT',
        'restaurant' => 'FACTURE RESTAURATION',
        'bar' => 'FACTURE BAR',
        'cafeteria' => 'FACTURE CAFÉTÉRIA',
        'blanchisserie' => 'FACTURE BLANCHISSERIE',
        'complet' => 'FACTURE COMPLETE',
        'consommation' => 'FACTURE CONSOMMATION',
    ];
    $titreFacture = $typeFactureLabels[$facture['type_facture']] ?? 'FACTURE';
}

// ============================================
// 8. VUE FACTURE COMBINÉE
// ============================================
if ($action === 'combinee' && isset($_GET['reservation_id'])) {
    $reservation_id_combinee = intval($_GET['reservation_id']);

    $resInfo = $db->prepare("
        SELECT r.numero_reservation, r.date_arrivee, r.date_depart, r.checkin_le, r.checkout_le,
               c.nom, c.prenom, c.email, c.telephone, c.adresse, 
               ch.numero as chambre_numero
        FROM reservations r
        JOIN clients c ON r.client_id = c.id
        LEFT JOIN chambres ch ON r.chambre_id = ch.id
        WHERE r.id = ?
    ");
    $resInfo->execute([$reservation_id_combinee]);
    $resInfoCombinee = $resInfo->fetch();

    $facturesListe = $db->prepare("SELECT * FROM factures WHERE reservation_id = ? ORDER BY type_facture");
    $facturesListe->execute([$reservation_id_combinee]);
    $facturesListe = $facturesListe->fetchAll();

    $facturesCombinees = [];
    foreach ($facturesListe as $f) {
        $lignesF = $db->prepare("SELECT * FROM facture_lignes WHERE facture_id = ?");
        $lignesF->execute([$f['id']]);
        $facturesCombinees[] = ['facture' => $f, 'lignes' => $lignesF->fetchAll()];
    }

    if (empty($facturesCombinees)) {
        header('Location: facture.php?error=' . urlencode('Aucune facture trouvée pour cette réservation'));
        exit;
    }
}

// ============================================
// 9. LISTE DES FACTURES
// ============================================
$search = $_GET['search'] ?? '';
$statut = $_GET['statut'] ?? '';
$page = max(1, intval($_GET['page'] ?? 1));
$perPage = 20;
$offset = ($page - 1) * $perPage;

$where = '1=1';
$params = [];

if ($search) {
    $where .= ' AND (c.nom LIKE ? OR c.prenom LIKE ? OR f.numero_facture LIKE ? OR r.numero_reservation LIKE ?)';
    $searchTerm = '%' . $search . '%';
    $params[] = $searchTerm;
    $params[] = $searchTerm;
    $params[] = $searchTerm;
    $params[] = $searchTerm;
}

if ($statut) {
    $where .= ' AND f.statut = ?';
    $params[] = $statut;
}

$countStmt = $db->prepare("
    SELECT COUNT(*) FROM factures f
    JOIN clients c ON f.client_id = c.id
    LEFT JOIN reservations r ON f.reservation_id = r.id
    WHERE $where
");
$countStmt->execute($params);
$totalFactures = $countStmt->fetchColumn();
$totalPages = ceil($totalFactures / $perPage);

$stmt = $db->prepare("
    SELECT f.*, c.nom, c.prenom, c.email, c.telephone, 
           r.numero_reservation, r.date_arrivee, r.date_depart,
           ch.numero as chambre_numero
    FROM factures f
    JOIN clients c ON f.client_id = c.id
    LEFT JOIN reservations r ON f.reservation_id = r.id
    LEFT JOIN chambres ch ON f.chambre_id = ch.id
    WHERE $where
    ORDER BY f.date_facture DESC, f.id DESC
    LIMIT ? OFFSET ?
");
$params[] = $perPage;
$params[] = $offset;
$stmt->execute($params);
$factures = $stmt->fetchAll();

$stats = [
    'total' => $db->query("SELECT COUNT(*) FROM factures")->fetchColumn(),
    'impaye' => $db->query("SELECT COALESCE(SUM(reste_a_payer),0) FROM factures WHERE statut IN ('en_attente', 'partielle')")->fetchColumn(),
    'encaissement' => $db->query("SELECT COALESCE(SUM(montant_paye),0) FROM factures")->fetchColumn(),
    'remises' => $db->query("SELECT COALESCE(SUM(remise),0) FROM factures")->fetchColumn()
];

$pageTitle = $action === 'view' ? 'Facture' : 'Factures';
include 'includes/header.php';
?>

<!-- ============================================ -->
<!-- STYLES CSS COMPLETS -->
<!-- ============================================ -->
<style>
/* ============================================
   STYLES COMPLETS POUR FACTURES (ÉCRAN)
============================================ */
.facture-container {
    max-width: 900px;
    margin: 0 auto;
}

.facture-card {
    background: linear-gradient(135deg, var(--panel), var(--panel-light));
    border: 1px solid var(--border);
    border-radius: 16px;
    overflow: hidden;
    box-shadow: var(--shadow);
}

.facture-header {
    padding: 25px 30px;
    background: rgba(217,179,108,0.1);
    border-bottom: 1px solid var(--border);
    text-align: center;
}

.facture-header h2 {
    color: var(--gold);
    margin: 0;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
    font-size: 28px;
}

.facture-header .subtitle {
    color: var(--text-muted);
    font-size: 14px;
    margin-top: 5px;
}

.facture-logo {
    display: block;
    max-height: 70px;
    max-width: 220px;
    margin: 0 auto 12px;
    object-fit: contain;
}

.facture-header .facture-title {
    margin-top: 15px;
    font-size: 18px;
    font-weight: 700;
    color: var(--gold);
}

.facture-body {
    padding: 30px;
}

.facture-infos {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 25px;
    margin-bottom: 25px;
    padding-bottom: 20px;
    border-bottom: 1px solid var(--border);
}

.facture-infos .info-group {
    font-size: 14px;
}

.facture-infos .info-group strong {
    display: block;
    color: var(--gold);
    margin-bottom: 5px;
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.05em;
}

.facture-infos .info-group .info-value {
    color: var(--text);
    line-height: 1.6;
}

.facture-lignes {
    margin-bottom: 20px;
}

.facture-lignes table {
    width: 100%;
    border-collapse: collapse;
}

.facture-lignes th {
    padding: 10px 12px;
    text-align: left;
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
    border-bottom: 2px solid var(--border);
}

.facture-lignes td {
    padding: 10px 12px;
    border-bottom: 1px solid rgba(255,255,255,0.03);
    font-size: 13px;
}

.facture-lignes .text-right {
    text-align: right;
}

.facture-groupe-titre {
    margin: 18px 0 10px;
    color: var(--gold);
    font-size: 14px;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    font-weight: 600;
}

.facture-total {
    text-align: right;
    margin-top: 25px;
    padding-top: 20px;
    border-top: 2px solid var(--border);
}

.total-line {
    display: flex;
    justify-content: flex-end;
    gap: 30px;
    margin: 4px 0;
    font-size: 14px;
}

.total-line .label {
    color: var(--text-muted);
}

.total-line .amount {
    font-weight: 600;
    min-width: 120px;
    text-align: right;
}

.total-grand {
    font-size: 22px;
    font-weight: 700;
    color: var(--gold);
    margin-top: 8px;
    padding-top: 8px;
    border-top: 1px solid var(--border);
}

.total-grand .amount {
    font-size: 24px;
}

.remise-line {
    color: #E74C3C;
}

.paiement-section {
    margin-top: 25px;
    padding: 20px;
    background: rgba(255,255,255,0.03);
    border-radius: 12px;
}

.paiement-section h4 {
    color: var(--gold);
    margin-bottom: 15px;
}

.paiement-history {
    margin-top: 15px;
}

.paiement-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 8px 12px;
    border-bottom: 1px solid rgba(255,255,255,0.05);
    font-size: 13px;
}

.paiement-item .date {
    color: var(--text-dim);
    font-size: 11px;
}

.paiement-item .amount {
    font-weight: 600;
}

/* Remise section */
.remise-section {
    margin-top: 25px;
    padding-top: 20px;
    border-top: 1px dashed var(--border);
}

.remise-section h4 {
    color: var(--gold);
    margin-bottom: 15px;
}

.badge-remise {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    padding: 3px 10px;
    border-radius: 20px;
    font-size: 11px;
    font-weight: 500;
}

.badge-remise.en_attente {
    background: rgba(243,156,18,0.2);
    color: #F39C12;
}

.badge-remise.acceptee {
    background: rgba(46,204,113,0.2);
    color: #2ECC71;
}

.badge-remise.refusee {
    background: rgba(231,76,60,0.2);
    color: #E74C3C;
}

/* Badge status */
.badge-status {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    padding: 4px 12px;
    border-radius: 20px;
    font-size: 11px;
    font-weight: 500;
}

.badge-en_attente {
    background: rgba(243,156,18,0.2);
    color: #F39C12;
}

.badge-partielle {
    background: rgba(52,152,219,0.2);
    color: #3498DB;
}

.badge-payee {
    background: rgba(46,204,113,0.2);
    color: #2ECC71;
}

.badge-annulee {
    background: rgba(231,76,60,0.2);
    color: #E74C3C;
}

/* Buttons */
.btn-print {
    background: transparent;
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 8px 16px;
    cursor: pointer;
    transition: all 0.2s;
    color: var(--text-muted);
}

.btn-print:hover {
    border-color: var(--gold);
    color: var(--gold);
}

.btn-action {
    background: transparent;
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 4px 10px;
    cursor: pointer;
    transition: all 0.2s;
    font-size: 12px;
}

.btn-action:hover {
    border-color: var(--gold);
    color: var(--gold);
}

.btn-action-success {
    border-color: rgba(46,204,113,0.3);
    color: #2ECC71;
}

.btn-action-success:hover {
    background: rgba(46,204,113,0.1);
    border-color: #2ECC71;
}

.btn-action-danger {
    border-color: rgba(231,76,60,0.3);
    color: #E74C3C;
}

.btn-action-danger:hover {
    background: rgba(231,76,60,0.1);
    border-color: #E74C3C;
}

/* Alertes */
.alert {
    padding: 12px 20px;
    border-radius: 10px;
    margin-bottom: 15px;
    font-size: 14px;
}

.alert-success {
    background: rgba(46,204,113,0.15);
    color: #2ECC71;
    border: 1px solid rgba(46,204,113,0.3);
}

.alert-danger {
    background: rgba(231,76,60,0.15);
    color: #E74C3C;
    border: 1px solid rgba(231,76,60,0.3);
}

.alert i {
    margin-right: 8px;
}

.page-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 25px;
    flex-wrap: wrap;
    gap: 15px;
}

.page-header h1 {
    margin: 0;
    font-size: 24px;
}

.page-header p {
    margin: 4px 0 0;
    color: var(--text-muted);
    font-size: 14px;
}

/* ============================================
   RESPONSIVE (ÉCRAN)
   ============================================ */
@media (max-width: 768px) {
    .facture-infos {
        grid-template-columns: 1fr;
        gap: 15px;
    }
    .facture-container {
        padding: 0 10px;
    }
    .facture-body {
        padding: 20px;
    }
    .page-header {
        flex-direction: column;
        align-items: flex-start;
    }
}

/* ============================================
   IMPRESSION - A4 PORTRAIT OPTIMISÉE
   ------------------------------------------------------------
   Objectif : ZÉRO gaspillage de papier.
   - Marges @page normales (type document A4 standard)
   - Cellules de tableau à padding raisonnable
   - Groupes de lignes collés les uns aux autres
   - Pas de saut de page forcé (sauf entre factures combinées)
   - Logo affiché à l'impression A4/PDF ; masqué en impression texte
     (ticket thermique), avec les icônes, qui ne sont pas prévus pour
     ce type d'imprimante (voir règles "print-mode-texte" plus bas)
   ============================================ */
@media print {
    /* --- FORMAT PAGE ---
       CORRECTIF (21/09/2026 - texte encore coupé malgré 15mm) : 15mm
       suffit sur un aperçu PDF, mais reste trop juste sur certaines
       imprimantes physiques dont la zone non imprimable réelle dépasse
       ce chiffre. On passe à 20mm sur tous les côtés — une marge
       "normale" de document, qui absorbe cette variation d'imprimante à
       imprimante sans dépendre du matériel exact utilisé. */
    @page {
        size: A4 portrait;
        margin: 20mm;
    }

    /* --- RESET GLOBAL : on tue tous les espacements par défaut ---
       BUGFIX (17/09/2026 - texte trop petit à l'impression) : la taille
       de base était figée à 10px, ce qui rendait la facture difficile à
       lire une fois imprimée sur A4 (texte "petit" par rapport à un
       document habituel). On agrandit légèrement l'ensemble des tailles
       de police ci-dessous (base + tous les éléments détaillés plus bas). */
    html, body {
        margin: 0 !important;
        padding: 0 !important;
        background: #fff !important;
        font-size: 12px !important;
        line-height: 1.25 !important;
    }

    /* Cacher tout ce qui n'est pas la facture */
    body * { visibility: hidden; }
    .facture-container,
    .facture-container * { visibility: visible; }

    .facture-container {
        position: absolute;
        left: 0;
        top: 0;
        width: 100%;
        max-width: 100%;
        margin: 0 !important;
        padding: 0 !important;
        /* BUGFIX (17/09/2026) : filet de sécurité contre les coupures de
           texte sur les côtés — un long mot non sécable (numéro de
           facture, email, adresse) ne doit jamais pouvoir élargir un
           élément au-delà de la largeur imprimable de la page. */
        overflow-wrap: break-word;
        word-break: break-word;
    }

    /* --- CARTE : plus aucune marge/padding externe --- */
    .facture-card {
        background: #fff !important;
        border: none !important;
        border-radius: 0 !important;
        box-shadow: none !important;
        margin: 0 !important;
        padding: 0 !important;
    }

    /* En vue simple : éviter la coupure en 2 pages */
    .facture-card:not(.facture-combinee-item) {
        break-inside: avoid;
        page-break-inside: avoid;
    }

    /* En vue combinée : enchaîner sans coupure interne */
    .facture-combinee-item {
        break-inside: avoid;
        page-break-inside: avoid;
        margin: 0 !important;
        padding: 0 !important;
    }
    .facture-combinee-item + .facture-combinee-item {
        border-top: 1px dashed #999 !important;
        padding-top: 4px !important;
        margin-top: 4px !important;
    }

    /* --- HEADER ---
       CORRECTIF (21/09/2026) : compression ramenée à un niveau plus
       raisonnable (l'ancien réglage, quasi à zéro, participait aussi à
       l'effet de texte "collé"/coupé sur les bords). */
    .facture-header {
        background: none !important;
        padding: 0 0 6px 0 !important;
        margin: 0 0 6px 0 !important;
        border-bottom: 1px solid #ccc !important;
        text-align: center;
    }
    .facture-header h2 {
        font-size: 16px !important;
        margin: 0 !important;
        padding: 0 !important;
    }
    .facture-header .subtitle {
        font-size: 10.5px !important;
        margin: 2px 0 0 0 !important;
        padding: 0 !important;
    }
    .facture-header .facture-title {
        font-size: 13.5px !important;
        margin: 3px 0 0 0 !important;
        padding: 0 !important;
    }
    .facture-header p {
        margin: 2px 0 0 0 !important;
        font-size: 10.5px !important;
    }
    /* BUGFIX (17/09/2026 - logo absent du PDF/A4) : le logo de l'hôtel
       était masqué à l'impression dans tous les cas (pour gagner de la
       place sur les petits tickets thermiques), y compris sur
       l'impression normale A4/PDF où il doit au contraire apparaître.
       Il est donc affiché par défaut ici ; seul le mode "impression
       texte" (ticket thermique, voir plus bas) le masque explicitement. */
    /* CORRECTIF (21/09/2026 - logo trop petit à l'impression) : 40px de
       hauteur (largeur libre) rendait le logo minuscule et peu lisible sur
       le document imprimé. Passage à un cadre fixe 100×100px, comme
       demandé — object-fit:contain pour que l'image s'inscrive dans ce
       cadre sans être déformée si elle n'est pas carrée. */
    .facture-logo {
        display: block !important;
        width: 100px !important;
        height: 100px !important;
        object-fit: contain !important;
        margin: 0 auto 8px auto !important;
    }

    /* --- BODY : aucun padding --- */
    .facture-body {
        padding: 0 !important;
        margin: 0 !important;
    }

    /* --- INFOS CLIENT/SÉJOUR : 2 colonnes, un peu plus aérées --- */
    .facture-infos {
        display: grid !important;
        grid-template-columns: 1fr 1fr !important;
        gap: 5px 12px !important;
        margin: 0 0 5px 0 !important;
        padding: 0 0 5px 0 !important;
        border-bottom: 1px solid #ccc !important;
        font-size: 11px !important;
        line-height: 1.35 !important;
    }
    .facture-infos .info-group strong {
        font-size: 11px !important;
        margin: 0 0 1px 0 !important;
        display: inline-block;
    }
    .facture-infos .info-group .info-value {
        font-size: 11px !important;
        line-height: 1.35 !important;
    }
    .facture-infos br { line-height: 1.15 !important; }

    /* --- GROUPES : titre compact --- */
    .facture-groupe-titre {
        font-size: 11px !important;
        margin: 3px 0 2px 0 !important;
        padding: 0 !important;
        text-transform: uppercase;
        letter-spacing: 0.02em;
    }

    /* --- TABLEAUX : cellules respirent un peu plus, largeurs de colonnes
       fixées explicitement pour que "Description" ne soit pas écrasée par
       un partage à parts égales entre les 4 colonnes (table-layout:fixed
       sans largeur = colonnes ~égales par défaut, donc du texte souvent
       renvoyé à la ligne / à l'étroit même quand il tiendrait sur une
       ligne avec plus de place). --- */
    .facture-lignes { margin: 0 !important; padding: 0 !important; }
    .facture-lignes table {
        width: 100% !important;
        max-width: 100% !important;
        table-layout: fixed !important;
        border-collapse: collapse !important;
        margin: 0 0 3px 0 !important;
        page-break-inside: auto;
    }
    .facture-lignes th:nth-child(1), .facture-lignes td:nth-child(1) { width: 46% !important; }
    .facture-lignes th:nth-child(2), .facture-lignes td:nth-child(2) { width: 12% !important; }
    .facture-lignes th:nth-child(3), .facture-lignes td:nth-child(3) { width: 21% !important; }
    .facture-lignes th:nth-child(4), .facture-lignes td:nth-child(4) { width: 21% !important; }
    .facture-lignes th,
    .facture-lignes td {
        padding: 3px 6px !important;
        font-size: 11px !important;
        line-height: 1.35 !important;
        border: none !important;
        overflow-wrap: break-word !important;
        word-break: break-word !important;
    }
    .facture-lignes th {
        border-bottom: 1px solid #999 !important;
        font-size: 10px !important;
        text-transform: uppercase;
    }
    .facture-lignes tr[style*="border-top"] td {
        border-top: 1px solid #ccc !important;
    }

    /* --- TOTAUX --- */
    .facture-total {
        margin: 3px 0 0 0 !important;
        padding: 3px 0 0 0 !important;
        border-top: 1px solid #999 !important;
    }
    .total-line {
        display: flex !important;
        justify-content: flex-end !important;
        gap: 12px !important;
        margin: 0 !important;
        padding: 0 !important;
        font-size: 11px !important;
        line-height: 1.35 !important;
    }
    .total-line .amount { min-width: 70px !important; }
    .total-grand {
        font-size: 14px !important;
        margin: 3px 0 0 0 !important;
        padding: 3px 0 0 0 !important;
        border-top: 1px solid #999 !important;
    }
    .total-grand .amount { font-size: 15px !important; }

    /* --- Montant en lettres --- */
    .facture-body > p {
        margin: 3px 0 0 0 !important;
        padding: 0 !important;
        font-size: 10px !important;
        line-height: 1.35 !important;
    }

    /* --- Remise accordée : bandeau --- */
    .facture-body > div[style*="rgba(46,204,113"] {
        margin: 3px 0 0 0 !important;
        padding: 3px 6px !important;
        font-size: 10.5px !important;
    }

    /* --- Pied de facture (boutons, actions) : masqué --- */
    .no-print { display: none !important; }

    /* ============================================
       MODE TEXTE : impression ticket / rapide
       → TOUT en GRAS et en NOIR pur
       → PAS de logo, PAS d'icônes (imprimante thermique,
         souvent incapable de rendre correctement webfonts/images)
       ============================================ */
    body.print-mode-texte .facture-card,
    body.print-mode-texte .facture-card * {
        font-family: 'Courier New', Courier, monospace !important;
        font-weight: 700 !important;            /* TOUT en gras */
        color: #000 !important;                 /* TOUT en noir */
        background: transparent !important;
        background-color: transparent !important;
        background-image: none !important;
        border-color: #000 !important;
        box-shadow: none !important;
        text-shadow: none !important;
        -webkit-print-color-adjust: exact;
        print-color-adjust: exact;
    }

    /* BUGFIX (17/09/2026 - logo/icônes gênants sur ticket thermique) :
       le logo (image) et les icônes FontAwesome (webfont) ne s'impriment
       pas correctement sur une petite imprimante thermique texte (rendu
       flou, carrés vides, ou gaspillage de largeur de ticket). On les
       masque explicitement en mode "impression texte", contrairement au
       mode A4/PDF ci-dessus où le logo doit au contraire être visible. */
    body.print-mode-texte .facture-logo,
    body.print-mode-texte .fa-solid,
    body.print-mode-texte .fa-regular,
    body.print-mode-texte .fa-brands,
    body.print-mode-texte [class*="fa-"] {
        display: none !important;
    }

    /* Les libellés "label" et cellules d'en-tête encore plus marqués */
    body.print-mode-texte .facture-header h2,
    body.print-mode-texte .facture-header .facture-title,
    body.print-mode-texte .facture-groupe-titre,
    body.print-mode-texte .total-grand,
    body.print-mode-texte .total-grand .amount {
        font-weight: 900 !important;
        color: #000 !important;
    }

    /* Tableaux : traits noirs visibles */
    body.print-mode-texte .facture-lignes th {
        border-bottom: 1px solid #000 !important;
        color: #000 !important;
        font-weight: 900 !important;
    }
    body.print-mode-texte .facture-lignes td {
        border: none !important;
        color: #000 !important;
        font-weight: 700 !important;
    }
    body.print-mode-texte .facture-lignes tr[style*="border-top"] td {
        border-top: 1px solid #000 !important;
    }
    body.print-mode-texte .facture-total,
    body.print-mode-texte .total-line {
        border-color: #000 !important;
        color: #000 !important;
    }
    body.print-mode-texte .facture-header {
        border-bottom: 1px solid #000 !important;
    }
    body.print-mode-texte .facture-infos {
        border-bottom: 1px solid #000 !important;
    }

    /* Neutraliser TOUTES les couleurs inline (rouge, vert, or...) */
    body.print-mode-texte [style*="color"] {
        color: #000 !important;
    }
}
</style>

<?php
// ============================================
// AFFICHAGE - VUE DÉTAILLÉE D'UNE FACTURE
// ============================================
if ($action === 'view' && isset($facture)):
?>

<div class="facture-container">
    <!-- Messages -->
    <?php if (isset($_GET['msg'])): ?>
        <?php [$type, $message] = explode(':', $_GET['msg'], 2); ?>
        <div class="alert alert-<?= $type ?> no-print"><i class="fa-solid fa-circle-info"></i> <?= htmlspecialchars($message) ?></div>
    <?php endif; ?>

    <!-- FACTURE PRINCIPALE -->
    <div class="facture-card">
        <div class="facture-header">
            <?php if (!empty($logoHotel)): ?>
            <img src="<?= htmlspecialchars($logoHotel) ?>" alt="Logo <?= htmlspecialchars(HOTEL_NOM) ?>" class="facture-logo">
            <?php endif; ?>
            <h2><i class="fa-solid fa-hotel"></i> <?= HOTEL_NOM ?></h2>
            <p class="subtitle"><?= HOTEL_ADRESSE ?> — Tél : <?= HOTEL_TELEPHONE ?></p>
            <p class="subtitle">NIF : <?= HOTEL_NIF ?> — STAT : <?= HOTEL_STAT ?></p>
            <div class="facture-title">
                <?= $titreFacture ?> N° <?= $facture['numero_facture'] ?>
            </div>
            <p style="margin-top: 5px; font-size: 13px; color: var(--text-dim);">
                Date: <?= date('d/m/Y H:i:s', strtotime($facture['date_facture'])) ?>
                <?php if ($facture['date_echeance']): ?>
                • Échéance: <?= date('d/m/Y H:i:s', strtotime($facture['date_echeance'])) ?>
                <?php endif; ?>
            </p>
        </div>
        
        <div class="facture-body">
            <!-- Infos client et séjour -->
            <div class="facture-infos">
                <div class="info-group">
                    <strong><i class="fa-solid fa-user"></i> Client</strong>
                    <div class="info-value">
                        <?= htmlspecialchars($facture['prenom'] . ' ' . $facture['nom']) ?><br>
                        <?php if ($facture['email']): ?>
                        <span style="font-size: 12px; color: var(--text-dim);">📧 <?= htmlspecialchars($facture['email']) ?></span><br>
                        <?php endif; ?>
                        <?php if ($facture['telephone']): ?>
                        <span style="font-size: 12px; color: var(--text-dim);">📱 <?= htmlspecialchars($facture['telephone']) ?></span><br>
                        <?php endif; ?>
                        <?php if ($facture['adresse']): ?>
                        <span style="font-size: 12px; color: var(--text-dim);">📍 <?= nl2br(htmlspecialchars($facture['adresse'])) ?></span>
                        <?php endif; ?>
                    </div>
                </div>
                <div class="info-group">
                    <strong><i class="fa-solid fa-calendar"></i> Séjour</strong>
                    <div class="info-value">
                        <?php if ($facture['numero_reservation']): ?>
                        Réservation: <strong><?= htmlspecialchars($facture['numero_reservation']) ?></strong><br>
                        <?php endif; ?>
                        <?php if ($facture['chambre_numero']): ?>
                        <span style="color: var(--gold);">Client hébergé</span> — Chambre N° <strong><?= htmlspecialchars($facture['chambre_numero']) ?></strong> (<?= ucfirst($facture['chambre_type'] ?? '') ?>)<br>
                        <?php endif; ?>
                        <?= afficherPeriodeSejour($facture['date_arrivee'], $facture['date_depart'], $facture['checkin_le'] ?? null, $facture['checkout_le'] ?? null) ?>
                    </div>
                </div>
            </div>

            <!-- Lignes de facture -->
            <div class="facture-lignes">
                <?php
                // Regrouper les lignes par catégorie
                $lignesSejour = array_filter($lignes, fn($l) => $l['type_ligne'] === 'sejour');
                $lignesConso  = array_filter($lignes, fn($l) => $l['type_ligne'] === 'consommation' || $l['type_ligne'] === 'restaurant' || $l['type_ligne'] === 'bar' || $l['type_ligne'] === 'service' || $l['type_ligne'] === 'blanchisserie');
                $lignesTaxe   = array_filter($lignes, fn($l) => $l['type_ligne'] === 'taxe');
                $lignesAutres = array_filter($lignes, fn($l) => !in_array($l['type_ligne'], ['sejour', 'consommation', 'restaurant', 'bar', 'service', 'blanchisserie', 'taxe']));
                
                $totalSejour = array_sum(array_column($lignesSejour, 'total'));
                $totalConso  = array_sum(array_column($lignesConso, 'total'));
                $totalTaxe   = array_sum(array_column($lignesTaxe, 'total'));
                $totalAutres = array_sum(array_column($lignesAutres, 'total'));
                
                // BUGFIX (impression) : la ligne d'en-tête (Description/Qté/Prix unit./Total)
                // ne doit s'afficher QUE pour le tout premier groupe, sinon chaque
                // nouveau groupe recrée une ligne d'en-tête inutile qui gaspille
                // verticalement ~15px à l'impression. On passe donc $afficherEntete
                // à false pour tous les groupes suivants.
                function renderLignesFactureV2($titre, $icone, $lignesGroupe, $totalGroupe, $afficherEntete = false) {
                    if (empty($lignesGroupe)) return;
                    echo '<div class="facture-groupe-titre"><i class="fa-solid ' . $icone . '"></i> ' . htmlspecialchars($titre) . '</div>';
                    echo '<table>';
                    if ($afficherEntete) {
                        echo '<tr><th style="text-align:left;">Description</th><th style="text-align:center;">Qté</th><th style="text-align:right;">Prix unit.</th><th style="text-align:right;">Total</th></tr>';
                    }
                    foreach ($lignesGroupe as $ligne) {
                        echo '<tr>';
                        echo '<td>' . htmlspecialchars($ligne['description']) . '</td>';
                        echo '<td style="text-align:center;">' . $ligne['quantite'] . '</td>';
                        echo '<td style="text-align:right;">' . formatMontant($ligne['prix_unitaire']) . '</td>';
                        echo '<td style="text-align:right; font-weight:600;">' . formatMontant($ligne['total']) . '</td>';
                        echo '</tr>';
                    }
                    echo '<tr style="border-top: 1px solid var(--border);">';
                    echo '<td colspan="3" style="text-align:right; font-weight:600;">Total ' . htmlspecialchars($titre) . '</td>';
                    echo '<td style="text-align:right; font-weight:600; color:var(--gold);">' . formatMontant($totalGroupe) . '</td>';
                    echo '</tr>';
                    echo '</table>';
                }
                
                // Hébergement (avec en-tête, car premier groupe)
                renderLignesFactureV2('Hébergement', 'fa-bed', $lignesSejour, $totalSejour, true);
                
                // Consommations (sans en-tête)
                renderLignesFactureV2('Consommations', 'fa-utensils', $lignesConso, $totalConso, false);
                
                // Taxes (sans en-tête)
                renderLignesFactureV2('Taxes', 'fa-file-invoice', $lignesTaxe, $totalTaxe, false);
                
                // Autres (sans en-tête)
                renderLignesFactureV2('Autres', 'fa-receipt', $lignesAutres, $totalAutres, false);
                ?>
            </div>

            <!-- Totaux -->
            <div class="facture-total">
                <?php if ($totalSejour > 0): ?>
                <div class="total-line">
                    <span class="label">Sous-total Hébergement</span>
                    <span class="amount"><?= formatMontant($totalSejour) ?></span>
                </div>
                <?php endif; ?>
                <?php if ($totalConso > 0): ?>
                <div class="total-line">
                    <span class="label">Total Consommations</span>
                    <span class="amount"><?= formatMontant($totalConso) ?></span>
                </div>
                <?php endif; ?>
                <?php if ($totalTaxe > 0): ?>
                <div class="total-line">
                    <span class="label">Total Taxes</span>
                    <span class="amount"><?= formatMontant($totalTaxe) ?></span>
                </div>
                <?php endif; ?>
                <?php if ($totalAutres > 0): ?>
                <div class="total-line">
                    <span class="label">Total Autres</span>
                    <span class="amount"><?= formatMontant($totalAutres) ?></span>
                </div>
                <?php endif; ?>
                <?php if (($facture['remise'] ?? 0) > 0): ?>
                <div class="total-line remise-line">
                    <span class="label"><i class="fa-solid fa-tags"></i> Remise / Solde annulé</span>
                    <span class="amount">- <?= formatMontant($facture['remise']) ?></span>
                </div>
                <?php endif; ?>
                <div class="total-line total-grand">
                    <span class="label">TOTAL TTC</span>
                    <span class="amount"><?= formatMontant($facture['total_ttc']) ?></span>
                </div>
                <div class="total-line">
                    <span class="label">Déjà payé</span>
                    <span class="amount"><?= formatMontant($facture['montant_paye']) ?></span>
                </div>
                <div class="total-line" style="color: <?= $facture['reste_a_payer'] > 0 ? '#E74C3C' : '#2ECC71' ?>; font-size: 18px; font-weight: 700; margin-top: 8px; padding-top: 8px; border-top: 1px solid var(--border);">
                    <span class="label">Reste à payer</span>
                    <span class="amount"><?= formatMontant($facture['reste_a_payer']) ?></span>
                </div>
            </div>

            <!-- Montant total en toutes lettres -->
            <p style="margin-top: 12px; font-size: 12px; font-style: italic; color: var(--text-dim);">
                Arrêtée la présente facture à la somme de : <strong><?= montantEnLettres($facture['total_ttc']) ?></strong>.
            </p>

            <!-- Remise -->
            <?php if (!empty($facture['remise_motif']) && strpos($facture['remise_motif'], 'Remise accordée') !== false): ?>
            <div style="margin-top: 15px; padding: 12px 16px; background: rgba(46,204,113,0.08); border-radius: 8px; border-left: 3px solid #2ECC71;">
                <p style="margin: 0; font-size: 13px; color: #2ECC71;">
                    <i class="fa-solid fa-check-circle"></i> 
                    <strong>Remise accordée</strong>
                </p>
                <div style="font-size: 12px; color: var(--text-dim); margin-top: 4px; white-space: pre-wrap;">
                    <?= nl2br(htmlspecialchars($facture['remise_motif'])) ?>
                </div>
            </div>
            <?php endif; ?>

            <!-- SECTION DEMANDE DE REMISE -->
            <div class="remise-section no-print">
                <h4><i class="fa-solid fa-hand-holding-dollar"></i> Demande de remise</h4>
                
                <?php if (!empty($demandes)): ?>
                    <?php foreach ($demandes as $d): ?>
                    <div style="padding: 12px 16px; border-radius: 8px; margin-bottom: 8px; 
                         <?= $d['statut'] == 'acceptee' ? 'background:rgba(46,204,113,0.1);border-left:3px solid #2ECC71;' : 
                            ($d['statut'] == 'refusee' ? 'background:rgba(231,76,60,0.1);border-left:3px solid #E74C3C;' : 
                            'background:rgba(243,156,18,0.1);border-left:3px solid #F39C12;') ?>">
                        <div style="display: flex; justify-content: space-between; align-items: flex-start; flex-wrap: wrap; gap: 10px;">
                            <div>
                                <div style="font-weight: 600; font-size: 15px;">
                                    <?= formatMontant($d['montant_demande']) ?>
                                    <span class="badge-remise <?= $d['statut'] ?>">
                                        <?= $d['statut'] == 'acceptee' ? '✅ Acceptée' : ($d['statut'] == 'refusee' ? '❌ Refusée' : '⏳ En attente') ?>
                                    </span>
                                </div>
                                <div style="font-size: 13px; color: var(--text); margin-top: 4px;">
                                    <?= htmlspecialchars($d['motif']) ?>
                                </div>
                                <div style="font-size: 11px; color: var(--text-dim); margin-top: 4px;">
                                    Demandé par: <?= htmlspecialchars($d['prenom'] . ' ' . $d['nom']) ?> (<?= ucfirst($d['role']) ?>) 
                                    le <?= date('d/m/Y H:i:s', strtotime($d['date_demande'])) ?>
                                    <?php if ($d['date_traitement']): ?>
                                    • Traité le <?= date('d/m/Y H:i:s', strtotime($d['date_traitement'])) ?>
                                    <?php endif; ?>
                                </div>
                            </div>
                            <?php if ($d['statut'] == 'en_attente' && $currentUser && in_array($currentUser['role'], ['admin', 'manager'], true)): ?>
                            <div style="display: flex; gap: 8px;">
                                <form method="POST" style="display: inline;">
                                    <?= csrf_field() ?>
                                    <input type="hidden" name="demande_id" value="<?= $d['id'] ?>">
                                    <input type="hidden" name="facture_id" value="<?= $facture_id ?>">
                                    <button type="submit" name="valider_remise" value="accepter" class="btn-action btn-action-success" onclick="return confirm('Accepter cette remise de <?= formatMontant($d['montant_demande']) ?> ?')">
                                        ✅ Accepter
                                    </button>
                                    <button type="submit" name="valider_remise" value="refuser" class="btn-action btn-action-danger" onclick="return confirm('Refuser cette demande de remise ?')">
                                        ❌ Refuser
                                    </button>
                                </form>
                            </div>
                            <?php endif; ?>
                        </div>
                    </div>
                    <?php endforeach; ?>
                <?php else: ?>
                    <div style="color: var(--text-dim); font-size: 13px; padding: 10px 0;">
                        <i class="fa-solid fa-info-circle"></i> Aucune demande de remise pour cette facture
                    </div>
                <?php endif; ?>
                
                <?php if ($facture['reste_a_payer'] > 0 && !$facture['remise_acceptee']): ?>
                <div style="margin-top: 15px; padding: 15px; background: rgba(217,179,108,0.05); border-radius: 12px; border: 1px solid var(--border);">
                    <p style="font-size: 13px; color: var(--text-dim); margin-bottom: 12px;">
                        <i class="fa-solid fa-pen"></i> Demander une remise sur le solde restant de 
                        <strong style="color: var(--gold);"><?= formatMontant($facture['reste_a_payer']) ?></strong>
                    </p>
                    <form method="POST">
                        <?= csrf_field() ?>
                        <input type="hidden" name="facture_id" value="<?= $facture_id ?>">
                        <div style="display: grid; grid-template-columns: 1fr 2fr; gap: 12px;">
                            <div class="form-group" style="margin-bottom: 0;">
                                <label style="font-size: 12px;">Montant</label>
                                <input type="number" name="montant_demande" class="form-control" 
                                       step="1" max="<?= $facture['reste_a_payer'] ?>" 
                                       placeholder="0" required>
                            </div>
                            <div class="form-group" style="margin-bottom: 0;">
                                <label style="font-size: 12px;">Motif</label>
                                <input type="text" name="motif_demande" class="form-control" 
                                       placeholder="Raison de la demande..." required>
                            </div>
                        </div>
                        <button type="submit" name="demander_remise" class="btn btn-outline" 
                                style="margin-top: 10px; border-color: #F39C12; color: #F39C12;">
                            <i class="fa-solid fa-paper-plane"></i> Demander une remise
                        </button>
                    </form>
                </div>
                <?php endif; ?>
            </div>

            <!-- SECTION PAIEMENTS -->
            <?php if ($facture['reste_a_payer'] > 0): ?>
            <div class="paiement-section no-print" id="paiement">
                <h4><i class="fa-solid fa-money-bill"></i> Enregistrer un paiement</h4>
                <form method="POST">
                    <?= csrf_field() ?>
                    <input type="hidden" name="facture_id" value="<?= $facture_id ?>">
                    <div style="display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 12px;">
                        <div class="form-group" style="margin-bottom: 0;">
                            <label style="font-size: 12px;">Montant</label>
                            <input type="number" name="montant" class="form-control" step="1" max="<?= $facture['reste_a_payer'] ?>" required>
                        </div>
                        <div class="form-group" style="margin-bottom: 0;">
                            <label style="font-size: 12px;">Mode paiement</label>
                            <select name="mode_paiement" class="form-control" id="facturePaiementMode" data-mm-target="facturePaiementMode_operateur">
                                <?= optionsModePaiement() ?>
                            </select>
                        </div>
                        <div class="form-group" style="margin-bottom: 0;">
                            <label style="font-size: 12px;">Référence</label>
                            <input type="text" name="reference" class="form-control" placeholder="N° chèque, transaction...">
                        </div>
                    </div>
                    <div class="form-group operateur-mm-group" id="facturePaiementMode_operateur" style="display:none; margin-top: 12px;">
                        <label style="font-size: 12px;">Opérateur Mobile Money</label>
                        <select name="operateur_mobile_money" class="form-control"><?= optionsOperateurMobileMoney() ?></select>
                    </div>
                    <button type="submit" name="enregistrer_paiement" class="btn btn-gold" style="margin-top: 12px;">
                        <i class="fa-solid fa-check"></i> Enregistrer le paiement
                    </button>
                </form>
            </div>
            <?php endif; ?>

            <!-- Historique des paiements -->
            <?php if (!empty($paiements)): ?>
            <div class="paiement-section" style="margin-top: 15px;">
                <h4><i class="fa-solid fa-clock-rotate-left"></i> Historique des paiements</h4>
                <div class="paiement-history">
                    <?php foreach ($paiements as $p): ?>
                    <div class="paiement-item">
                        <div>
                            <span style="font-weight: 500;"><?= formatMontant($p['montant']) ?></span>
                            <span style="font-size: 12px; color: var(--text-dim); margin-left: 10px;">
                                <?= afficherModePaiement($p['mode_paiement'], $p['operateur_mobile_money'] ?? null) ?>
                                <?= $p['reference'] ? " - Réf: {$p['reference']}" : '' ?>
                            </span>
                        </div>
                        <span class="date"><?= date('d/m/Y H:i:s', strtotime($p['date_paiement'])) ?></span>
                    </div>
                    <?php endforeach; ?>
                </div>
            </div>
            <?php endif; ?>

            <!-- Actions -->
            <div class="no-print" style="margin-top: 25px; padding-top: 20px; border-top: 1px solid var(--border); display: flex; gap: 10px; flex-wrap: wrap; justify-content: center;">
                <button class="btn-print" onclick="imprimerFacture('pdf')">
                    <i class="fa-solid fa-file-pdf"></i> Imprimer / PDF (A4)
                </button>
                <button class="btn-print btn-outline" onclick="imprimerFacture('texte')">
                    <i class="fa-solid fa-align-left"></i> Impression simple (texte)
                </button>
                <?php if (!empty($facturesLiees)): ?>
                <a href="facture.php?action=combinee&reservation_id=<?= $facture['reservation_id'] ?>" class="btn btn-gold">
                    <i class="fa-solid fa-layer-group"></i> Voir combinée
                </a>
                <?php endif; ?>
                <a href="facture.php" class="btn btn-outline">
                    <i class="fa-solid fa-list"></i> Retour à la liste
                </a>
            </div>
        </div>
    </div>

</div>

<?php
// ============================================
// AFFICHAGE - VUE FACTURE COMBINÉE
// ============================================
elseif ($action === 'combinee' && !empty($facturesCombinees)):
?>

<div class="facture-container">
    <?php
    $totalGeneralTTC = 0;
    $totalGeneralPaye = 0;
    $totalGeneralReste = 0;
    ?>
    <!-- BUGFIX (12/09/2026) : le logo/nom/adresse de l'hôtel ET les infos
         client/séjour étaient auparavant réaffichés en entier dans CHAQUE
         carte de facture de la boucle (donc dupliqués autant de fois qu'il
         y avait de factures combinées — hébergement + consommation = déjà
         2x les mêmes infos), gaspillant de la place et poussant le document
         sur 2 pages à l'impression. Ces infos, identiques pour tout le
         document combiné, ne sont désormais affichées qu'UNE seule fois,
         avant la boucle. -->
    <div class="facture-card facture-combinee-item">
        <div class="facture-header">
            <?php if (!empty($logoHotel)): ?>
            <img src="<?= htmlspecialchars($logoHotel) ?>" alt="Logo <?= htmlspecialchars(HOTEL_NOM) ?>" class="facture-logo">
            <?php endif; ?>
            <h2><i class="fa-solid fa-hotel"></i> <?= HOTEL_NOM ?></h2>
            <p><?= HOTEL_ADRESSE ?> — Tél : <?= HOTEL_TELEPHONE ?></p>
            <p>NIF : <?= HOTEL_NIF ?> — STAT : <?= HOTEL_STAT ?></p>
            <div class="facture-title">Facture combinée — Séjour N° <?= htmlspecialchars($resInfoCombinee['numero_reservation'] ?? '-') ?></div>
        </div>
        <div class="facture-body">
            <div class="facture-infos">
                <div class="info-group">
                    <strong>Client</strong>
                    <div class="info-value">
                        <?= htmlspecialchars($resInfoCombinee['prenom'] . ' ' . $resInfoCombinee['nom']) ?><br>
                        <?= htmlspecialchars($resInfoCombinee['email'] ?? '') ?><br>
                        <?= htmlspecialchars($resInfoCombinee['telephone'] ?? '') ?>
                    </div>
                </div>
                <div class="info-group">
                    <strong>Séjour</strong>
                    <div class="info-value">
                        <?= $resInfoCombinee['chambre_numero'] ? 'Client hébergé — Chambre N° ' . htmlspecialchars($resInfoCombinee['chambre_numero']) : 'Chambre -' ?><br>
                        Réservation N° <?= htmlspecialchars($resInfoCombinee['numero_reservation'] ?? '-') ?><br>
                        <?= afficherPeriodeSejour($resInfoCombinee['date_arrivee'], $resInfoCombinee['date_depart'], $resInfoCombinee['checkin_le'] ?? null, $resInfoCombinee['checkout_le'] ?? null) ?>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <?php foreach ($facturesCombinees as $bloc): $f = $bloc['facture']; $lignesF = $bloc['lignes'];
        $totalGeneralTTC += $f['total_ttc'];
        $totalGeneralPaye += $f['montant_paye'];
        $totalGeneralReste += $f['reste_a_payer'];
    ?>
    <div class="facture-card facture-combinee-item">
        <div class="facture-body">
            <div class="facture-groupe-titre" style="display:flex; justify-content:space-between; align-items:center; color:var(--gold); font-weight:700;">
                <span><?= $typeFactureLabels[$f['type_facture']] ?? 'FACTURE' ?> N° <?= htmlspecialchars($f['numero_facture']) ?></span>
                <span style="font-size:12px; font-weight:400; color:var(--text-dim);">Date: <?= date('d/m/Y H:i:s', strtotime($f['date_facture'])) ?></span>
            </div>

            <div class="facture-lignes">
                <table>
                    <thead>
                        <tr>
                            <th style="text-align:left;">Description</th>
                            <th style="text-align:center;">Qté</th>
                            <th style="text-align:right;">Prix unit.</th>
                            <th style="text-align:right;">Total</th>
                        </tr>
                    </thead>
                    <tbody>
                        <?php foreach ($lignesF as $ligne): ?>
                        <tr>
                            <td><?= htmlspecialchars($ligne['description']) ?></td>
                            <td style="text-align:center;"><?= $ligne['quantite'] ?></td>
                            <td style="text-align:right;"><?= formatMontant($ligne['prix_unitaire']) ?></td>
                            <td style="text-align:right; font-weight:600;"><?= formatMontant($ligne['total']) ?></td>
                        </tr>
                        <?php endforeach; ?>
                    </tbody>
                </table>
            </div>

            <div class="facture-total">
                <?php
                // BUGFIX (retour hôtel 09/2026) : ce bloc n'affichait que
                // "Sous-total" + "TVA", en silence sur tout le reste (vignette,
                // taxe de séjour...) qui compose pourtant, avec eux, le Total
                // TTC affiché juste en dessous et les lignes détaillées
                // au-dessus. Résultat : Sous-total + TVA ≠ Total TTC à l'oeil
                // nu, ce qui donnait l'impression d'une erreur de calcul.
                // On additionne maintenant TOUTES les lignes de type "taxe"
                // (TVA, vignette, taxe de séjour...) pour que le récapitulatif
                // reprenne exactement la même décomposition que les lignes
                // détaillées, et que la somme affichée corresponde toujours au
                // Total TTC.
                $totalTaxesF = 0;
                foreach ($lignesF as $lg) {
                    if ($lg['type_ligne'] === 'taxe') { $totalTaxesF += $lg['total']; }
                }
                ?>
                <div class="total-line">
                    <span class="label">Sous-total</span>
                    <span class="amount"><?= formatMontant($f['sous_total']) ?></span>
                </div>
                <?php if ($totalTaxesF > 0): ?>
                <div class="total-line">
                    <span class="label">Taxes (TVA, vignette, taxe de séjour...)</span>
                    <span class="amount"><?= formatMontant($totalTaxesF) ?></span>
                </div>
                <?php endif; ?>
                <?php if (($f['remise'] ?? 0) > 0): ?>
                <div class="total-line remise-line">
                    <span class="label">Remise</span>
                    <span class="amount">- <?= formatMontant($f['remise']) ?></span>
                </div>
                <?php endif; ?>
                <div class="total-line total-grand">
                    <span class="label">TOTAL TTC</span>
                    <span class="amount"><?= formatMontant($f['total_ttc']) ?></span>
                </div>
                <div class="total-line">
                    <span class="label">Déjà payé</span>
                    <span class="amount"><?= formatMontant($f['montant_paye']) ?></span>
                </div>
                <div class="total-line" style="color: <?= $f['reste_a_payer'] > 0 ? '#E74C3C' : '#2ECC71' ?>; font-weight: 700;">
                    <span class="label">Reste à payer</span>
                    <span class="amount"><?= formatMontant($f['reste_a_payer']) ?></span>
                </div>
                <?php if ($f['reste_a_payer'] > 0): ?>
                <!-- Ajout (18/09/2026) : jusqu'ici, la vue combinée affichait le
                     reste à payer de chaque facture sans aucun moyen de
                     l'encaisser depuis cette page — il fallait revenir à la
                     liste ou ouvrir la facture séparément. -->
                <div class="no-print" style="text-align:right; margin-top:8px;">
                    <button type="button" class="btn btn-gold" style="padding:6px 16px; font-size:13px;"
                            onclick="ouvrirPaiementFacture(<?= (int)$f['id'] ?>, '<?= htmlspecialchars($f['numero_facture'], ENT_QUOTES) ?>', <?= (float)$f['reste_a_payer'] ?>)">
                        <i class="fa-solid fa-money-bill"></i> Encaisser cette facture
                    </button>
                </div>
                <?php endif; ?>
            </div>
        </div>
    </div>
    <?php endforeach; ?>

    <div class="facture-card">
        <div class="facture-body">
            <div class="facture-total" style="text-align: right;">
                <h3 style="color: var(--gold); margin-bottom: 12px;">Récapitulatif combiné</h3>
                <div class="total-line total-grand">
                    <span class="label">Total général TTC</span>
                    <span class="amount"><?= formatMontant($totalGeneralTTC) ?></span>
                </div>
                <div class="total-line">
                    <span class="label">Total déjà payé</span>
                    <span class="amount"><?= formatMontant($totalGeneralPaye) ?></span>
                </div>
                <div class="total-line" style="color: <?= $totalGeneralReste > 0 ? '#E74C3C' : '#2ECC71' ?>; font-size: 18px; font-weight: 700; margin-top: 8px; padding-top: 8px; border-top: 1px solid var(--border);">
                    <span class="label">Total reste à payer</span>
                    <span class="amount"><?= formatMontant($totalGeneralReste) ?></span>
                </div>
                <p style="margin-top: 12px; font-size: 12px; font-style: italic; color: var(--text-dim);">
                    Arrêté le présent document à la somme totale de : <strong><?= montantEnLettres($totalGeneralTTC) ?></strong>.
                </p>
            </div>
            <div class="no-print" style="margin-top: 20px; padding-top: 20px; border-top: 1px solid var(--border); display: flex; gap: 10px; flex-wrap: wrap; justify-content: center;">
                <button class="btn-print" onclick="imprimerFacture('pdf')">
                    <i class="fa-solid fa-file-pdf"></i> Imprimer / PDF (A4, 1 page)
                </button>
                <button class="btn-print btn-outline" onclick="imprimerFacture('texte')">
                    <i class="fa-solid fa-align-left"></i> Impression simple (texte)
                </button>
                <a href="facture.php" class="btn btn-outline">
                    <i class="fa-solid fa-list"></i> Retour à la liste
                </a>
            </div>
        </div>
    </div>
</div>

<?php
// Modal d'encaissement centralisé (18/09/2026) : voir
// includes/modal_paiement_facture.php. Retour sur cette même vue combinée
// (même séjour) après le paiement.
$retourUrlPaiement = 'facture.php?action=combinee&reservation_id=' . (int)$reservation_id_combinee;
include 'includes/modal_paiement_facture.php';
?>

<?php
// ============================================
// AFFICHAGE - LISTE DES FACTURES
// ============================================
else:
?>

<div class="page-header">
    <div>
        <h1><i class="fa-solid fa-file-invoice" style="color:var(--gold);margin-right:12px;"></i>Factures — Toutes (Hébergement + Consommations)</h1>
        <p><?= number_format($totalFactures, 0, ',', ' ') ?> facture(s) - <?= formatMontant($stats['encaissement']) ?> encaissé</p>
    </div>
</div>

<!-- Navigation entre les deux vues de facturation -->
<div style="display:flex; gap:10px; margin-bottom:20px;">
    <span class="btn-gold" style="border-radius:30px; padding:8px 20px; cursor:default; font-size:13px;">
        <i class="fa-solid fa-list"></i> Toutes les factures
    </span>
    <a href="factures.php" class="btn-outline" style="border-radius:30px; padding:8px 20px; font-size:13px; text-decoration:none;">
        <i class="fa-solid fa-receipt"></i> Factures consommations uniquement
    </a>
</div>

<?php if (isset($_GET['msg'])): ?>
    <?php [$type, $message] = explode(':', $_GET['msg'], 2); ?>
    <div class="alert alert-<?= $type ?>"><i class="fa-solid fa-circle-info"></i> <?= htmlspecialchars($message) ?></div>
<?php endif; ?>

<?php if (isset($_GET['error'])): ?>
    <div class="alert alert-danger"><i class="fa-solid fa-circle-exclamation"></i> <?= htmlspecialchars($_GET['error']) ?></div>
<?php endif; ?>

<!-- Statistiques -->
<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 15px; margin-bottom: 25px;">
    <div style="background: linear-gradient(135deg, var(--panel), var(--panel-light)); border: 1px solid var(--border); border-radius: 12px; padding: 15px 20px; text-align: center;">
        <div style="font-size: 24px; font-weight: 700; color: var(--gold);"><?= number_format($stats['total'], 0, ',', ' ') ?></div>
        <div style="font-size: 11px; color: var(--text-muted); margin-top: 4px;">Factures émises</div>
    </div>
    <div style="background: linear-gradient(135deg, var(--panel), var(--panel-light)); border: 1px solid var(--border); border-radius: 12px; padding: 15px 20px; text-align: center;">
        <div style="font-size: 24px; font-weight: 700; color: #E74C3C;"><?= formatMontant($stats['impaye']) ?></div>
        <div style="font-size: 11px; color: var(--text-muted); margin-top: 4px;">Montant impayé</div>
    </div>
    <div style="background: linear-gradient(135deg, var(--panel), var(--panel-light)); border: 1px solid var(--border); border-radius: 12px; padding: 15px 20px; text-align: center;">
        <div style="font-size: 24px; font-weight: 700; color: #2ECC71;"><?= formatMontant($stats['encaissement']) ?></div>
        <div style="font-size: 11px; color: var(--text-muted); margin-top: 4px;">Total encaissé</div>
    </div>
    <div style="background: linear-gradient(135deg, var(--panel), var(--panel-light)); border: 1px solid var(--border); border-radius: 12px; padding: 15px 20px; text-align: center;">
        <div style="font-size: 24px; font-weight: 700; color: #F39C12;"><?= formatMontant($stats['remises']) ?></div>
        <div style="font-size: 11px; color: var(--text-muted); margin-top: 4px;">Remises accordées</div>
    </div>
</div>

<!-- Barre de recherche -->
<div style="display: flex; gap: 10px; margin-bottom: 25px; flex-wrap: wrap;">
    <input type="text" id="searchInput" style="flex:2; padding:12px 18px; background:rgba(255,255,255,0.03); border:1px solid var(--border); border-radius:50px; color:var(--text); font-size:14px;" 
           placeholder="🔍 Rechercher par N° facture, client, réservation..."
           value="<?= htmlspecialchars($search) ?>">
    <select id="statutSelect" style="padding:12px 18px; background:rgba(255,255,255,0.03); border:1px solid var(--border); border-radius:50px; color:var(--text); font-size:14px; min-width:150px;">
        <option value="">Tous les statuts</option>
        <option value="en_attente" <?= $statut == 'en_attente' ? 'selected' : '' ?>>⏳ En attente</option>
        <option value="partielle" <?= $statut == 'partielle' ? 'selected' : '' ?>>🔄 Partielle</option>
        <option value="payee" <?= $statut == 'payee' ? 'selected' : '' ?>>✅ Payée</option>
        <option value="annulee" <?= $statut == 'annulee' ? 'selected' : '' ?>>❌ Annulée</option>
        <option value="en_retard" <?= $statut == 'en_retard' ? 'selected' : '' ?>>⚠️ En retard</option>
    </select>
    <button class="btn-gold" onclick="applyFilters()" style="border-radius:50px; padding:0 25px;">
        <i class="fa-solid fa-magnifying-glass"></i> Filtrer
    </button>
    <button class="btn-outline" onclick="resetFilters()" style="border-radius:50px; padding:0 20px;">
        <i class="fa-solid fa-rotate-left"></i>
    </button>
</div>

<!-- Tableau -->
<div style="background: linear-gradient(135deg, var(--panel), var(--panel-light)); border: 1px solid var(--border); border-radius: 12px; overflow: hidden;">
    <table style="width:100%; border-collapse:collapse;">
        <thead>
            <tr>
                <th style="padding:14px 16px; text-align:left; font-size:11px; text-transform:uppercase; color:var(--gold); border-bottom:1px solid var(--border); background:rgba(217,179,108,0.05);">N° Facture</th>
                <th style="padding:14px 16px; text-align:left; font-size:11px; text-transform:uppercase; color:var(--gold); border-bottom:1px solid var(--border); background:rgba(217,179,108,0.05);">Client</th>
                <th style="padding:14px 16px; text-align:left; font-size:11px; text-transform:uppercase; color:var(--gold); border-bottom:1px solid var(--border); background:rgba(217,179,108,0.05);">Réservation</th>
                <th style="padding:14px 16px; text-align:left; font-size:11px; text-transform:uppercase; color:var(--gold); border-bottom:1px solid var(--border); background:rgba(217,179,108,0.05);">Date</th>
                <th style="padding:14px 16px; text-align:right; font-size:11px; text-transform:uppercase; color:var(--gold); border-bottom:1px solid var(--border); background:rgba(217,179,108,0.05);">Total TTC</th>
                <th style="padding:14px 16px; text-align:right; font-size:11px; text-transform:uppercase; color:var(--gold); border-bottom:1px solid var(--border); background:rgba(217,179,108,0.05);">Reste</th>
                <th style="padding:14px 16px; text-align:left; font-size:11px; text-transform:uppercase; color:var(--gold); border-bottom:1px solid var(--border); background:rgba(217,179,108,0.05);">Statut</th>
                <th style="padding:14px 16px; text-align:center; font-size:11px; text-transform:uppercase; color:var(--gold); border-bottom:1px solid var(--border); background:rgba(217,179,108,0.05);">Actions</th>
            </tr>
        </thead>
        <tbody>
            <?php if (empty($factures)): ?>
            <tr>
                <td colspan="8" style="text-align:center; padding:60px; color:var(--text-dim);">
                    <i class="fa-solid fa-file-invoice" style="font-size:48px; display:block; margin-bottom:15px; opacity:0.5;"></i>
                    <p>Aucune facture trouvée</p>
                    <button class="btn-gold" onclick="openReservationSearch()" style="margin-top:10px; padding:8px 20px; border-radius:30px;">
                        <i class="fa-solid fa-plus"></i> Créer une facture
                    </button>
                </td>
            </tr>
            <?php else: ?>
            <?php foreach ($factures as $f): ?>
            <tr>
                <td style="padding:12px 16px; border-bottom:1px solid rgba(255,255,255,0.03);">
                    <strong style="color:var(--gold); font-family:monospace; font-size:13px;">
                        <?= htmlspecialchars($f['numero_facture']) ?>
                    </strong>
                    <div style="font-size:10px; color:var(--text-dim);">
                        <?php
                        $typeLabels = [
                            'hotel' => '🏨 Hébergement',
                            'restaurant' => '🍽️ Restauration',
                            'bar' => '🍸 Bar',
                            'complet' => '📄 Complet',
                            'consommation' => '🍽️ Consommation'
                        ];
                        echo $typeLabels[$f['type_facture']] ?? 'Facture';
                        ?>
                    </div>
                </td>
                <td style="padding:12px 16px; border-bottom:1px solid rgba(255,255,255,0.03);">
                    <div style="font-weight:500;"><?= htmlspecialchars($f['prenom'] . ' ' . $f['nom']) ?></div>
                    <div style="font-size:11px; color:var(--text-dim);"><?= htmlspecialchars($f['email'] ?? '') ?></div>
                </td>
                <td style="padding:12px 16px; border-bottom:1px solid rgba(255,255,255,0.03);">
                    <?= $f['numero_reservation'] ?? '-' ?>
                    <div style="font-size:10px; color:var(--text-dim);"><?= $f['chambre_numero'] ? 'Ch ' . $f['chambre_numero'] : '' ?></div>
                </td>
                <td style="padding:12px 16px; border-bottom:1px solid rgba(255,255,255,0.03); font-size:13px;">
                    <?= date('d/m/Y H:i', strtotime($f['date_facture'])) ?>
                </td>
                <td style="padding:12px 16px; border-bottom:1px solid rgba(255,255,255,0.03); text-align:right; font-weight:600;">
                    <?= formatMontant($f['total_ttc']) ?>
                </td>
                <td style="padding:12px 16px; border-bottom:1px solid rgba(255,255,255,0.03); text-align:right; font-weight:600; <?= $f['reste_a_payer'] > 0 ? 'color:#E74C3C;' : 'color:#2ECC71;' ?>">
                    <?= formatMontant($f['reste_a_payer']) ?>
                </td>
                <td style="padding:12px 16px; border-bottom:1px solid rgba(255,255,255,0.03);">
                    <span class="badge-status badge-<?= $f['statut'] ?>">
                        <?= $f['statut'] == 'payee' ? '✅' : ($f['statut'] == 'partielle' ? '⏳' : '📄') ?>
                        <?= ucfirst(str_replace('_', ' ', $f['statut'])) ?>
                    </span>
                </td>
                <td style="padding:12px 16px; border-bottom:1px solid rgba(255,255,255,0.03); text-align:center;">
                    <div style="display:flex; gap:6px; justify-content:center;">
                        <a href="facture.php?action=view&id=<?= $f['id'] ?>" class="btn-action" title="Voir">
                            <i class="fa-solid fa-eye"></i>
                        </a>
                        <?php if ($f['reste_a_payer'] > 0): ?>
                        <button type="button" class="btn-action btn-action-success" title="Paiement"
                                onclick="ouvrirPaiementFacture(<?= (int)$f['id'] ?>, '<?= htmlspecialchars($f['numero_facture'], ENT_QUOTES) ?>', <?= (float)$f['reste_a_payer'] ?>)">
                            <i class="fa-solid fa-money-bill"></i>
                        </button>
                        <?php endif; ?>
                        <?php if ($currentUser && $currentUser['role'] === 'admin' && $f['montant_paye'] == 0): ?>
                        <!-- BUGFIX (architecture facturation) : ce formulaire n'avait pas
                             d'attribut action, il soumettait donc vers l'URL courante
                             (sans action=delete), ce qui ne supprimait jamais rien. -->
                        <form method="POST" action="facture.php?action=delete&id=<?= $f['id'] ?>" style="display:inline;" onsubmit="return confirm('Supprimer cette facture ?')">
                            <?= csrf_field() ?>
                            <button type="submit" class="btn-action btn-action-danger" title="Supprimer">
                                <i class="fa-solid fa-trash"></i>
                            </button>
                        </form>
                        <?php endif; ?>
                    </div>
                </td>
            </tr>
            <?php endforeach; ?>
            <?php endif; ?>
        </tbody>
    </table>
</div>

<!-- Pagination -->
<?php if ($totalPages > 1): ?>
<div style="display:flex; justify-content:center; gap:8px; padding:20px; border-top:1px solid var(--border);">
    <?php if ($page > 1): ?>
    <a href="?page=<?= $page-1 ?>&search=<?= urlencode($search) ?>&statut=<?= urlencode($statut) ?>" style="padding:8px 14px; background:rgba(255,255,255,0.03); border:1px solid var(--border); border-radius:8px; color:var(--text); text-decoration:none;">
        <i class="fa-solid fa-chevron-left"></i> Précédent
    </a>
    <?php endif; ?>
    
    <span style="padding:8px 14px; color:var(--text-dim);">Page <?= $page ?> / <?= $totalPages ?></span>
    
    <?php if ($page < $totalPages): ?>
    <a href="?page=<?= $page+1 ?>&search=<?= urlencode($search) ?>&statut=<?= urlencode($statut) ?>" style="padding:8px 14px; background:rgba(255,255,255,0.03); border:1px solid var(--border); border-radius:8px; color:var(--text); text-decoration:none;">
        Suivant <i class="fa-solid fa-chevron-right"></i>
    </a>
    <?php endif; ?>
</div>
<?php endif; ?>

<!-- Bouton flottant -->
<button class="fab-create-facture" onclick="openReservationSearch()" title="Créer une facture" style="position:fixed; bottom:30px; right:30px; width:60px; height:60px; border-radius:50%; background:linear-gradient(135deg, var(--gold), var(--gold-dark)); border:none; color:var(--noir); font-size:24px; cursor:pointer; box-shadow:0 4px 20px rgba(0,0,0,0.3); z-index:100;">
    <i class="fa-solid fa-plus"></i>
</button>

<!-- Modal recherche réservation -->
<div id="reservationSearchModal" style="position:fixed; top:0; left:0; right:0; bottom:0; background:rgba(0,0,0,0.85); backdrop-filter:blur(5px); z-index:1000; display:none; align-items:center; justify-content:center;">
    <div style="background:linear-gradient(135deg, var(--panel), var(--panel-light)); border:1px solid var(--border); border-radius:24px; width:90%; max-width:700px; max-height:85vh; display:flex; flex-direction:column; overflow:hidden;">
        <div style="padding:20px 25px; border-bottom:1px solid var(--border); display:flex; justify-content:space-between; align-items:center;">
            <h3 style="margin:0; color:var(--gold); font-size:20px;"><i class="fa-solid fa-hotel"></i> Créer une facture</h3>
            <button onclick="closeReservationSearch()" style="background:rgba(255,255,255,0.03); border:none; width:36px; height:36px; border-radius:50%; color:var(--text-muted); font-size:18px; cursor:pointer;">
                <i class="fa-solid fa-xmark"></i>
            </button>
        </div>
        <div style="padding:25px; overflow-y:auto; flex:1;">
            <input type="text" id="reservationSearchInput" style="width:100%; padding:14px 18px; background:rgba(255,255,255,0.03); border:1px solid var(--border); border-radius:60px; color:var(--text); font-size:15px; margin-bottom:20px;" 
                   placeholder="🔍 Rechercher par nom, prénom, email, téléphone ou N° réservation...">
            <div id="reservationResults" style="max-height:450px; overflow-y:auto;">
                <div style="text-align:center; padding:50px 20px; color:var(--text-dim);">
                    <i class="fa-solid fa-search" style="font-size:48px; margin-bottom:15px; opacity:0.5;"></i>
                    <p>Tapez au moins 2 caractères pour rechercher</p>
                </div>
            </div>
        </div>
    </div>
</div>

<?php
// Modal d'encaissement centralisé (18/09/2026) : voir
// includes/modal_paiement_facture.php. Retour sur cette même liste après
// le paiement.
$retourUrlPaiement = 'facture.php' . (!empty($_SERVER['QUERY_STRING']) ? '?' . $_SERVER['QUERY_STRING'] : '');
include 'includes/modal_paiement_facture.php';
?>

<?php endif; ?>

<!-- ============================================ -->
<!-- JAVASCRIPT COMPLET -->
<!-- ============================================ -->
<script>
const FACTURE_CSRF_TOKEN = "<?= csrfToken() ?>";

// ============================================
// CHOIX D'IMPRESSION : "PDF A4" (mise en page complète, 1 page)
// ou "texte simple" (noir & blanc, sans styles, pour impression rapide)
// ============================================
function imprimerFacture(mode) {
    document.body.classList.remove('print-mode-texte');
    if (mode === 'texte') {
        document.body.classList.add('print-mode-texte');
    }
    window.print();
}

// ============================================
// RECHERCHE DE RÉSERVATIONS
// ============================================
let searchTimeoutFacture;
const searchInputRes = document.getElementById('reservationSearchInput');
const resultsDiv = document.getElementById('reservationResults');

if (searchInputRes) {
    searchInputRes.addEventListener('input', function() {
        clearTimeout(searchTimeoutFacture);
        const query = this.value.trim();
        
        if (query.length < 2) {
            resultsDiv.innerHTML = '<div style="text-align:center; padding:50px 20px; color:var(--text-dim);"><i class="fa-solid fa-search" style="font-size:48px; margin-bottom:15px; opacity:0.5;"></i><p>Tapez au moins 2 caractères pour rechercher</p></div>';
            return;
        }
        
        searchTimeoutFacture = setTimeout(() => {
            fetch(`facture.php?action=search_reservations&q=${encodeURIComponent(query)}`)
                .then(r => r.json())
                .then(data => {
                    if (data.length > 0) {
                        resultsDiv.innerHTML = data.map(res => `
                            <div style="padding:15px; border-bottom:1px solid var(--border); cursor:pointer; transition:all 0.2s; display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:10px;" 
                                 onclick="selectReservation(${res.id})"
                                 onmouseover="this.style.background='rgba(217,179,108,0.1)'; this.style.borderRadius='12px';"
                                 onmouseout="this.style.background='transparent'; this.style.borderRadius='0';">
                                <div style="flex:1;">
                                    <div style="font-weight:600; color:var(--gold); font-size:15px;">
                                        <i class="fa-solid fa-user"></i> ${escapeHtmlFacture(res.prenom)} ${escapeHtmlFacture(res.nom)}
                                    </div>
                                    <div style="font-size:12px; color:var(--text-dim); margin-top:4px;">
                                        <i class="fa-solid fa-calendar"></i> ${res.date_arrivee} → ${res.date_depart}
                                    </div>
                                    <div style="font-size:11px; color:var(--text-muted); margin-top:2px;">
                                        <i class="fa-solid fa-bed"></i> Chambre ${res.chambre_numero || '?'} • 
                                        <i class="fa-solid fa-tag"></i> ${res.numero_reservation}
                                        ${res.email ? ' • <i class="fa-solid fa-envelope"></i> ' + escapeHtmlFacture(res.email) : ''}
                                        ${res.telephone ? ' • <i class="fa-solid fa-phone"></i> ' + escapeHtmlFacture(res.telephone) : ''}
                                    </div>
                                </div>
                                <button style="background:var(--gold); border:none; padding:8px 20px; border-radius:30px; color:var(--noir); font-weight:500; cursor:pointer;">Sélectionner</button>
                            </div>
                        `).join('');
                    } else {
                        resultsDiv.innerHTML = '<div style="text-align:center; padding:50px 20px; color:var(--text-dim);"><i class="fa-solid fa-circle-info" style="font-size:48px; margin-bottom:15px; opacity:0.5;"></i><p>Aucune réservation trouvée</p></div>';
                    }
                })
                .catch(error => {
                    console.error('Erreur:', error);
                    resultsDiv.innerHTML = '<div style="text-align:center; padding:50px 20px; color:#E74C3C;"><i class="fa-solid fa-triangle-exclamation" style="font-size:48px; margin-bottom:15px;"></i><p>Erreur de recherche</p></div>';
                });
        }, 400);
    });
}

function escapeHtmlFacture(text) {
    if (!text) return '';
    return text.replace(/[&<>]/g, function(m) {
        if (m === '&') return '&amp;';
        if (m === '<') return '&lt;';
        if (m === '>') return '&gt;';
        return m;
    });
}

function selectReservation(id) {
    const form = document.createElement('form');
    form.method = 'POST';
    form.action = 'facture.php?action=generer';
    const input = document.createElement('input');
    input.type = 'hidden';
    input.name = 'csrf_token';
    input.value = FACTURE_CSRF_TOKEN;
    form.appendChild(input);
    const input2 = document.createElement('input');
    input2.type = 'hidden';
    input2.name = 'reservation_id';
    input2.value = id;
    form.appendChild(input2);
    document.body.appendChild(form);
    form.submit();
}

function openReservationSearch() {
    document.getElementById('reservationSearchModal').style.display = 'flex';
    setTimeout(() => {
        document.getElementById('reservationSearchInput').focus();
    }, 100);
}

function closeReservationSearch() {
    document.getElementById('reservationSearchModal').style.display = 'none';
}

// Fermer le modal au clic extérieur
document.getElementById('reservationSearchModal')?.addEventListener('click', function(e) {
    if (e.target === this) {
        closeReservationSearch();
    }
});

// ============================================
// FILTRES
// ============================================
function applyFilters() {
    let url = 'facture.php?';
    let search = document.getElementById('searchInput')?.value || '';
    let statut = document.getElementById('statutSelect')?.value || '';
    if (search) url += 'search=' + encodeURIComponent(search) + '&';
    if (statut) url += 'statut=' + encodeURIComponent(statut);
    window.location.href = url || 'facture.php';
}

function resetFilters() {
    window.location.href = 'facture.php';
}

document.getElementById('searchInput')?.addEventListener('keypress', function(e) {
    if (e.key === 'Enter') applyFilters();
});
</script>

<?php include 'includes/footer.php'; ?>