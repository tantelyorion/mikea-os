# Agent IA de Mikea OS

Tape **Espace** sur le bureau (ou clique sur « MikeaOS » dans la barre de menu) : la **barre Mikea** s'ouvre.
Écris une demande en français ou en anglais, Mikea la transforme en **une action précise** du système.

```
 « ouvre la calculatrice »        → ouvre l'application
 « passe en mode sombre »         → change le thème            (annulable)
 « crée un fichier notes.txt avec le texte bonjour »  → demande confirmation
 « supprime notes.txt »           → confirmation forte, va à la CORBEILLE (jamais effacé)
 « annule »                       → défait la dernière action
 « aide »                         → liste d'exemples
```

## Comment ça marche

```
 saisie ─► agent_kernel_plan()
             ├─ 1. règles hors ligne (agent_plan_rules)  : instantané, sans réseau
             └─ 2. sinon : modèle local (Ollama), sortie CONTRAINTE par un schéma JSON
                           │
                           ▼
                  agent_parse_json()   ← le modèle ne peut citer qu'une action du registre
                           │
                           ▼
                  agent_decide()       ← validation des arguments + niveau de permission
                           │
           ┌───────────────┴────────────────┐
        RUN (seule)                CONFIRM / CONFIRM_STRONG → bouton « Valider »
           └───────────────┬────────────────┘
                           ▼
                  agent_execute(call, confirmed)   ← SEUL point d'exécution
```

Une règle et un modèle passent **exactement** par les mêmes garde-fous.

## Garde-fous

| Garde-fou | Effet |
|---|---|
| Liste fermée d'actions | un nom inventé (« rm », « shell », « shutdown »…) est refusé |
| Arguments validés | noms de fichiers `[A-Za-z0-9._-]`, sans `/`, `..`, point initial, ≤ 31 car. ; valeurs d'une liste connue |
| 4 niveaux de permission | lecture et réglages réversibles : seuls ; écriture : confirmation ; destructif : confirmation forte |
| `confirmed` obligatoire | `agent_execute` **refuse lui-même** une action sensible non confirmée, quel que soit l'appelant |
| Entrée ne valide jamais le destructif | il faut cliquer sur le bouton |
| Jamais de suppression définitive | `delete_file` passe par `file_trash()` (récupérable) |
| Pas de shell | l'agent n'appelle que des fonctions typées ; l'installeur n'est pas ouvrable |
| Droits de l'utilisateur | les fonctions de fichiers vérifient `current_user_can()` : l'agent ne peut pas plus que toi |
| Annulation et journal | pile de 8 annulations, journal des 16 dernières actions |

## Actions (kernel/ai/agent_builtin.c)

| Action | Permission | Annulable |
|---|---|---|
| `open_app` | réversible | – |
| `set_theme` / `set_sound` / `set_wallpaper` | réversible | oui |
| `system_info` / `list_files` / `read_file` | lecture | – |
| `create_file` / `restore_file` | écriture (confirmation) | oui |
| `delete_file` / `logout` | destructif (confirmation forte) | `delete_file` oui |

## Ajouter une action (3 étapes)

1. **Plateforme** : ajouter un pointeur de fonction dans `agent_platform` (`agent_builtin.h`) et l'implémenter dans
   `agent_kernel.c` (c'est là, et seulement là, qu'on appelle le noyau).
2. **Action** : ajouter une entrée dans le tableau `BUILTIN` de `agent_builtin.c` : nom, aide pour le modèle (courte, ASCII),
   permission, texte de confirmation, `validate`, `run` et `undo` si possible.
3. **(facultatif) Règle hors ligne** : ajouter les mots-clés dans `agent_plan_rules()` (`agent.c`).

Le schéma JSON et la consigne du modèle sont **générés depuis le registre** : rien d'autre à modifier.
Puis lancer `tools/tests/agent_host_test.c` (et les deux fuzzers) : ils attaquent les garde-fous.

## Modèle local

Le modèle tourne **sur ton PC** (Ollama, aucune donnée ne quitte la machine) ; Mikea OS lui parle en HTTP via la passerelle
QEMU (`10.0.2.2:11434`). Le schéma passe dans le champ `format` d'`/api/generate` : le décodage est **contraint**, le modèle ne
peut produire que du JSON conforme.

- Changer de modèle dans Mikea OS : `jarvis model <nom>` (le nom peut faire jusqu'à 95 caractères).
- Pour un agent fiable, viser environ **4 milliards de paramètres** (en dessous, les appels d'outils multi-tours deviennent
  peu fiables). Préférer une variante **« instruct » non-thinking** : un modèle qui écrit des balises `<think>` avant sa
  réponse gêne la lecture du JSON. Exemple : Qwen3-4B-Instruct-2507 (~2,5 Go en Q4_K_M). Vérifier le nom exact dans la
  bibliothèque Ollama, ou tirer un GGUF depuis Hugging Face : `ollama run hf.co/<depot>:Q4_K_M`.
- Sans modèle joignable, les règles continuent de marcher hors ligne.

## Limites connues

- L'appel au modèle **bloque** la fenêtre (« Mikea réfléchit… ») : à déplacer dans un fil de travail.
- Une action par demande (pas d'enchaînement).
- Le clavier PS/2 ne tape que de l'ASCII : on écrit « systeme », les règles comprennent aussi « système ».
- Le modèle tourne sur l'hôte, pas encore **dans** Mikea OS (voir la feuille de route : ring 3, grosse RAM, SSE/AVX).
