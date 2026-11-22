# Correctifs d'affichage / exécution sous QEMU

## Démarrage
- **BSS jamais remis à zéro** (`kernel/arch/x86_64/entry.asm`) : `objcopy -O binary` n'inclut pas le BSS ;
  les variables statiques (curseur, états des pilotes, drapeaux…) contenaient des valeurs aléatoires.
  Le BSS est maintenant effacé avant la mise en place de la pile.
- **Tampon de chargement du noyau** (`boot/loader/stage2.asm`) : déplacé de 0x10000 à 0x20000 pour ne plus
  chevaucher la fin de stage2 (0x8000–0x14000) ; la copie reste sous 0xA0000.

## Exceptions CPU
- `isr_handler` figeait la machine sans rien afficher. Un **écran d'erreur lisible** (nom, vecteur, code d'erreur,
  RIP, CR2, registres) s'affiche maintenant, en mode graphique comme en mode texte
  (`kernel/interrupt/handlers.c`, `kernel/cpu/isr_stubs.asm`).

## Multitâche
- `thread_yield()` pouvait être interrompu par le minuteur en plein changement de contexte : corruption des piles.
  Les interruptions sont masquées pendant la commutation puis restaurées.
- Pile des nouveaux threads correctement alignée (rsp ≡ 8 mod 16 à l'entrée).

## Pilote graphique (`kernel/drivers/graphics/graphics.c`)
- Masques couleur VBE nuls (certains BIOS) → écran noir : repli sur le format 8:8:8.
- `gfx_fill_rect` réécrit (clipping unique, couleur empaquetée une fois) ; défilement par blocs de 8 octets ;
  voie rapide pour le verre dépoli en 32 bits. Résultat : bien moins de lenteur et de scintillement.

## Interface
- Curseur souris non redessiné s'il n'a pas bougé (fin du clignotement) ; position initiale au centre.
- Purge du tampon PS/2 avant l'initialisation de la souris.
- Boucle du bureau en veille (`hlt`) au lieu d'une attente active à 100 % CPU ; horloge mise à jour chaque seconde.
- Ombres de fenêtres plus douces (2 couches).

## Build
- Le Makefile recompile maintenant `boot.bin`/`stage2.bin` quand les `.asm` changent et suit les dépendances des `.h`.
- Détection du backend audio QEMU (`pa`, `alsa`, `sdl`, sinon `none`) : QEMU ne plante plus sans PulseAudio.
- `-vga std -m 256M` ajoutés ; vérification que `kernel.bin` ≤ 900 secteurs.
- `tools/check_toolchain.sh` accepte `gcc/ld/objcopy` standard.

## Réseau, sécurité et JARVIS (nouveau)
- **PCI réel** (`kernel/drivers/pci`) : énumération du bus, BAR, bus-mastering. Commande `lspci`.
- **Ethernet Intel e1000** (`kernel/drivers/net/e1000.c`) : la carte par défaut de QEMU, VirtualBox, VMware.
  Mode polling, DMA par anneaux RX/TX.
- **Pile réseau** (`net/`) : Ethernet, ARP, IPv4, ICMP. Commandes `netinfo`, `ping <ip>`, `arp [flush]`.
  Réseau QEMU par défaut : 10.0.2.15, passerelle 10.0.2.2 (essayez `ping 10.0.2.2`).
- **Sécurité par défaut** : trafic entrant non sollicité refusé, mode furtif (ping entrant ignoré),
  **détection d'usurpation ARP** (une entrée existante n'est jamais écrasée en silence), journal `seclog`,
  commande `firewall [stealth on|off]`.
- **JARVIS** (`kernel/ai/jarvis.c`) : moteur d'intentions local français/anglais
  (`jarvis état du réseau`, `jarvis ping 10.0.2.2`, `jarvis journal de sécurité`…). Pour les questions
  ouvertes, pont série vers un **modèle local** Ollama : `python3 tools/jarvis_bridge.py` puis `make run-ai`,
  et dans l'OS `jarvis ask <question>`. La réponse du modèle est affichée, **jamais exécutée**.
- `radio` / `wifi` / `bluetooth` : état honnête des radios et de la localisation.

### Internet (nouveau)
- **Abstraction de carte réseau** (`net/net.c`) : la pile ne dépend plus d'un pilote précis.
- **Realtek RTL8139** (`kernel/drivers/net/rtl8139.c`) en plus de l'Intel e1000 : `make run QEMU_NIC=rtl8139`.
- **UDP, DHCP, DNS, TCP, HTTP** (`net/tcpip.c`) : commandes `dhcp`, `nslookup <nom>`, `http <hôte> [/chemin]`.
  Sécurité : aucun port ouvert, seules les réponses à nos propres requêtes sont acceptées (identifiants, ports,
  numéros de séquence vérifiés). Pas de TLS : HTTP seulement.
- Testé sur l'hôte contre un serveur simulé (DHCP → DNS → TCP → HTTP, sommes de contrôle vérifiées).

### JARVIS ↔ Ollama direct (nouveau)
- `jarvis ask <question>` (ou toute phrase non reconnue) interroge **Ollama sur votre PC par TCP**
  (`10.0.2.2:11434` = l'hôte vu depuis QEMU), sans pont ni internet. Prérequis : `ollama serve` et
  `ollama pull llama3.2:3b`. Changer de modèle : `jarvis model <nom>`.
- Secours : le pont série `tools/jarvis_bridge.py` (`make run-ai`).
- La destination est toujours l'hôte local ; la réponse est affichée, jamais exécutée.

### Cryptographie (nouveau, socle de TLS)
- `crypto/` : **SHA-256, HMAC-SHA256, AES-128-GCM, X25519**, sans dépendance. Comparaison en temps constant ;
  AES-GCM authentifie **avant** de déchiffrer (un message falsifié ne produit aucune sortie).
- Vérifié : vecteurs officiels FIPS/RFC 4231/NIST/RFC 7748 + 115 comparaisons aléatoires contre
  Python/OpenSSL (0 écart). Dans l'OS : `selftest` (tests) et `sha256 <texte>`.
- **Noyau** : le chargeur lit maintenant 1000 secteurs (500 Ko) au lieu de 900.

### HTTPS / TLS 1.3 (nouveau)
- `https <hôte> [/chemin] [pin=<empreinte>]` : client **TLS 1.3** (AES-128-GCM, X25519), signatures serveur
  **RSA-PSS (1024-4096 bits) et ECDSA P-256**, vérification du `CertificateVerify` et du `Finished`.
- **Identité par empreinte de clé** (comme SSH), faute de magasin d'autorités : avec `pin=` la connexion est refusée
  si la clé diffère ; sans `pin`, première confiance (avertissement affiché) puis refus si l'empreinte change.
- Aléa : RDRAND (QEMU `-cpu max`, ajouté au Makefile) ; sinon gigue d'horloge (avertissement).
- **Testé contre un vrai serveur OpenSSL TLS 1.3** : 12/12 (RSA-2048/4096, ECDSA, mauvaise empreinte refusée,
  octets falsifiés refusés, Ed25519 refusé proprement). Voir `tools/tests/`.
- Limites : pas de validation de chaîne/expiration/nom d'hôte, pas de P-384/Ed25519, pas de reprise de session.

### Ce qui n'est PAS fait (et pourquoi)
- **WiFi / Bluetooth / GPS** : aucun pilote réel. Une carte WiFi exige un firmware propriétaire + pile 802.11 +
  WPA2/3 ; le Bluetooth passe en général par USB (pile xHCI/EHCI + HCI absentes) ; pas de récepteur GNSS.
  Chacun est un chantier de plusieurs mois ; `radio` indique l'état réel de la machine.
- **Validation de chaîne de certificats** (autorités de confiance, expiration, nom d'hôte) : non faite ; remplacée par l'épinglage de clé.
- **USB** (clavier/souris/Bluetooth USB) : pile xHCI non écrite.
- **IA embarquée dans le noyau** : un vrai LLM ne peut pas tourner dans ce noyau (SSE/FPU désactivés, pas de
  mémoire ni de CPU suffisants). D'où le pont vers un modèle local sur l'hôte.

> Non testé sous QEMU dans mon environnement (ni `nasm` ni `qemu` disponibles) : le code C compile sans
> avertissement et passe l'édition de liens (symboles asm exclus). Testez avec `make clean && make run`.

## Interface : texte lisse (etape 1) et noyau allege (etape 2)
- **Texte anti-aliase** (`kernel/drivers/graphics/graphics.c`, `assets/fonts/font_ui.h`) : polices 4 bits/pixel generees par
  `tools/gen_font.py` (Poppins pour l'interface, DejaVu Sans Mono pour la grille 16 px), rendu entier (pas de SSE), UTF-8
  (accents francais). `gfx_draw_char()` utilise automatiquement la police lissee sur la grille de cellules (scale 2) ;
  `gfx_text_width()` / `gfx_draw_text_ui()` / `gfx_draw_text_ui_vcenter()` pour la mise en page proportionnelle.
  Branche dans les barres de titre, boutons, champs, menu, horloge, Launchpad et ecran de connexion.
- **Assets sur disque** (`kernel/assets/`, `tools/pack_assets.py`, `assets/assets.pak`) : fonds d'ecran (169 Ko) et sons (88 Ko)
  ne sont plus compiles dans le noyau (qui passe de ~451 Ko a ~233 Ko sur 512 Ko possibles). Ils sont lus a la demande depuis
  le disque de boot, secteur 1100, zone copiee par `install`. Pack absent/corrompu : repli silencieux (fond en degrade, bips).
  Pour ajouter un fond : deposer un `.rgb` dans `assets/wallpapers/`, le declarer dans `tools/pack_assets.py` et
  `gui/assets/wallpaper_images.c`, puis `python3 tools/pack_assets.py`.
- Verifie : tout le C compile sans warning avec les flags du Makefile ; test hote `tools/tests/assets_host_test.c`
  (6 fonds + 2 sons identiques octet par octet, repli sur pack absent/corrompu/entree hors zone). **Non teste sous QEMU.**

## Boite a outils de widgets (etape 3a)
- `gui/ui.h`, `gui/ui.c` : widgets en PIXELS, "mode immediat" (carte, etiquette, bouton normal/principal/danger,
  bascule, ligne de liste, champ de texte, barre de progression, separateur) + decoupe de zones (`ui_cut_top`...).
  Coins arrondis lisses (`gfx_fill_rounded_rect_aa`), police proportionnelle, couleurs du theme (clair/sombre).
- Aucune application existante n'est encore migree : la grille de cellules reste en place. Prochaine etape : migrer
  Reglages, puis le gestionnaire de fichiers, vers `ui.h`.
- Verifie : compilation sans warning ; test hote `tools/tests/ui_host_test.c` (cycle de clic : maintien, relachement
  dedans/dehors, glisser, bascule) ; rendu clair/sombre controle visuellement. **Non teste sous QEMU.**

## Reglages migre vers les widgets (etape 3b)
- `apps/settings/settings.c` (boucle d'evenements : cadre de fenetre, glisser, agrandir, focus, dock, clavier -- inchanges)
  + `apps/settings/settings_view.c/.h` (mise en page et dessin en pixels avec `gui/ui.h`, sans dependance noyau).
- Nouvelle disposition : cartes Session (avatar + role), Apparence (bascules Mode sombre / Sons), Fond d'ecran (6 vignettes
  cliquables + bouton Motif), Systeme (disque, memoire, date/heure avec jauges), pied avec Deconnexion. Fenetre 44 cellules
  de large (bornee a l'ecran), Entree ferme toujours la fenetre.
- Repeint partiel au survol/clic (sans redessiner le fond d'ecran) ; les cartes sont repeintes sur un fond opaque connu
  (`ui_card_clear`) afin que les bords lisses ne "durcissent" pas au fil des repeints (verifie : 5 repeints = memes pixels).
- Disque/memoire/heure rafraichis au plus toutes les 3 s.
- Verifie : compilation sans warning ; simulation d'edition de liens (aucun symbole manquant hors asm) ; test hote
  `tools/tests/settings_view_host_test.c`. **Non teste sous QEMU.**

## Double buffering (etape 4)
- `kernel/drivers/graphics/graphics.c` dessine desormais dans un TAMPON EN RAM (32 Mio, jusqu'a 16 Mio : 1920x1080 ou
  2560x1440 en 32 bits) et copie vers la memoire video seulement les lignes modifiees (`gfx_present`). Plus de
  scintillement sur les redessins complets ; les melanges alpha lisent la RAM et non plus la memoire video (bien plus rapide).
- Le tampon n'est active que si le CMOS (registres 0x34/0x35) annonce assez de RAM ; sinon ecriture directe comme avant.
  Message au demarrage : `[graphics] Mode graphique actif (double buffering)` ou `(ecriture directe)`.
- Trois garanties contre un "ecran qui ne s'affiche jamais" :
  1. `gui_draw_cursor()` (appelee par toutes les boucles graphiques) appelle `gfx_present()` en fin d'image ;
  2. filet de securite `gfx_tick()` dans le timer (100 Hz) : si plus rien n'est dessine depuis un tick et que l'etat n'est pas
     affiche, il copie tout l'ecran ;
  3. `gfx_set_direct()` en tete d'`isr_handler()` : sur panique du noyau on affiche tout et on repasse en ecriture directe.
- Verifie : compilation sans warning ; test hote `tools/tests/double_buffer_host_test.c` (7 scenarios, dont "la memoire video
  n'est pas touchee pendant le dessin" et "rendu identique au mode direct, 3 078 400 octets"). **Non teste sous QEMU.**
- Limite connue : le marquage des lignes modifiees n'est pas atomique entre plusieurs fils qui dessineraient en meme temps
  (une seule fenetre a le focus et dessine a la fois, donc non observe en pratique).

## Agent IA et barre Mikea (etape 5) -- voir docs/AGENT.md
- `kernel/ai/agent.c/.h` : cerveau pur (sans dependance noyau) : registre ferme d'actions, planificateur hors ligne
  (francais/anglais, accents, mots composes), lecture tolerante du JSON d'un modele, 4 niveaux de permission,
  `agent_execute(call, confirmed)` = SEUL point d'execution (refuse seul une action sensible non confirmee), journal,
  pile d'annulation.
- `kernel/ai/agent_builtin.c/.h` : 11 actions derriere une interface "plateforme" (aucun appel noyau direct).
  `delete_file` passe par la corbeille. Aucune suppression definitive.
- `kernel/ai/agent_kernel.c/.h` : plateforme reelle + choix du cerveau (regles, puis modele local, puis repli hors ligne).
- `kernel/ai/jarvis.c` : client Ollama generalise (`jarvis_llm_generate`) avec sortie contrainte par schema JSON (`format`),
  tampons agrandis, `\n` correctement encode, nom de modele jusqu'a 95 caracteres.
- `apps/mikea_bar/` : barre Mikea (vue + boucle). Ouverte par Espace (bureau) ou clic sur "MikeaOS" (barre de menu).
- Verifie : compilation sans warning ; aucun symbole non resolu hors asm ; `tools/tests/agent_host_test.c` (174 verifications,
  dont arguments pieges et confirmations contournees) ; deux fuzzers sous ASan/UBSan (700 000 entrees, 0 plantage) ;
  `ollama_request_host_test.c` (requete HTTP reelle : JSON valide, Content-Length exact) ;
  `mikea_bar_view_host_test.c` (clics par etat, idempotence, 4 resolutions). **Non teste sous QEMU.**
