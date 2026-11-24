# Stardew Valley Save Editor — PS Vita

Éditeur de sauvegarde pour **Stardew Valley** conçu pour tourner sur **PlayStation Vita** (en homebrew `.vpk`), développé en **C11** avec une architecture **double cible** permettant de le compiler et de le lancer **nativement sur macOS** (à la résolution exacte de la PS Vita : **960×544**) pour un développement rapide.

---

## 1. Stack technique

| Composant | Technologie | Rôle |
|---|---|---|
| Langage | **C11** (`-Wall -Wextra -Wpedantic`) | Langage natif de VitaSDK et SDL2, rapide et faible empreinte RAM |
| Fenêtrage, rendu & inputs | **SDL2** (`SDL_Renderer`) | Fenêtre 960×544, dessin 2D, gestion clavier + manette / boutons PS Vita |
| Rendu de texte | **SDL2_ttf** | Affichage de polices TrueType (`.ttf` / `.ttc`) pour l'interface maison |
| Parsing XML | **libxml2** | Lecture et modification des fichiers de sauvegarde XML Stardew Valley (plusieurs Mo) |
| Build system | **CMake + Ninja** | Compilation rapide sur macOS et compilation croisée PS Vita via **VitaSDK** |

---

## 2. Architecture du projet

Le projet est découpé en **3 couches indépendantes**, orchestrées par `src/main.c`. Ce découpage permet de garder la logique métier et l'interface **100 % portables** entre macOS et PS Vita.

```text
                    ┌────────────────────────┐
                    │       src/main.c       │
                    │  Init / Boucle 60 FPS  │
                    └───────────┬────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌───────────────┐      ┌─────────────────┐      ┌───────────────┐
│ src/platform/ │      │    src/core/    │      │    src/ui/    │
│  Abstraction  │─────▶│  Données & XML  │◀─────│   Interface   │
│  Mac / Vita   │      │    SaveData     │      │ SDL2 + TTF UI │
└───────────────┘      └─────────────────┘      └───────────────┘
```

### Flux d'exécution à chaque frame (~60 FPS)
1. **Entrées (`platform`)** : `SDL_PollEvent` récupère un événement brut (touche clavier, bouton manette, fermeture fenêtre) et `platform_translate_event()` le traduit en action abstraite (`InputAction` : `INPUT_UP`, `INPUT_RIGHT`, `INPUT_TAB_NEXT`, `INPUT_SAVE`, etc.).
2. **Mise à jour (`ui` → `core`)** : `ui_handle_input()` déplace la sélection, change d'onglet, modifie les champs de la structure `SaveData` (en marquant `dirty = true`) ou déclenche l'écriture XML (`save_data_save_xml()`).
3. **Rendu (`ui`)** : `ui_render()` dessine l'écran complet en 960×544 via `SDL_Renderer` (bandeau titre, onglets, panneau de champs, barre d'aide et messages de statut).

### Principe du Dual-Target (macOS natif vs PS Vita)
Le fichier `CMakeLists.txt` détecte automatiquement la cible :
- **Sans toolchain Vita** (`PLATFORM_DESKTOP=1`) : compile un exécutable natif macOS lié aux librairies Homebrew (`sdl2`, `SDL2_ttf`) et au `libxml2` système.
- **Avec la toolchain VitaSDK** (`if(VITA)`, `PLATFORM_VITA=1`) : compile avec `arm-vita-eabi-gcc`, lie les stubs PS Vita (`SceDisplay_stub`, `SceCtrl_stub`, `SceGxm_stub`, etc.) et package automatiquement `stardew_save_editor.self` puis **`stardew_save_editor.vpk`**.

---

## 3. Arborescence et rôle de chaque fichier

```text
saveEditorStardewValleyPSVITA/
├── CMakeLists.txt
├── .gitignore
├── README.md
├── build/                                  # Dossier de build généré (non versionné)
├── src/
│   ├── main.c                              # Point d'entrée et boucle principale
│   ├── core/
│   │   ├── save_data.h                     # Modèle SaveData et API XML
│   │   └── save_data.c                     # Lecture/écriture XML (libxml2) et backup .bak
│   ├── platform/
│   │   ├── platform.h                      # Constantes écran, InputAction, API plateforme
│   │   └── platform.c                      # Chemins Mac/Vita et mapping clavier/manette
│   └── ui/
│       ├── ui.h                            # État de l'interface (UIState) et API UI
│       └── ui.c                            # Rendu 2D SDL2/TTF et logique des menus
└── test_saves/
    ├── sample_save.xml                     # Petite sauvegarde XML de test
    └── vita_pcse01235/
        └── EDWN_117938427/                 # Vraie sauvegarde PS Vita (Title ID PCSE01235)
            ├── EDWN_117938427              # Fichier principal de sauvegarde (~3,8 Mo XML)
            ├── EDWN_117938427_old          # Copie du jour précédent générée par le jeu
            ├── SaveGameInfo                # Métadonnées pour l'écran de chargement (~58 Ko)
            └── SaveGameInfo_old            # Copie précédente des métadonnées
```

### Détail de chaque dossier et fichier

#### Racine du projet
- **`CMakeLists.txt`** : Déclare le projet C11, liste les fichiers sources, configure les dépendances selon la plateforme (macOS via `PkgConfig` + `LibXml2`, ou PS Vita via `vita.cmake` + `vita_create_self` + `vita_create_vpk`).
- **`.gitignore`** : Exclut les fichiers générés (`build/`, `build-vita/`, `.DS_Store`, `*.vpk`, `*.self`, `*.velf`, `*.bak`).
- **`README.md`** : Documentation complète du projet.

#### `src/main.c`
Point d'entrée de l'application :
1. Initialise `libxml2`, `SDL2` (vidéo + manette) et `SDL2_ttf`.
2. Crée la fenêtre **960×544** et le renderer matériel (`SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC`).
3. Ouvre la première manette détectée (ou les contrôles PS Vita sur console).
4. Charge les polices via `ui_init()`.
5. Charge une sauvegarde XML selon l'ordre de priorité suivant :
   1. Chemin passé en argument de ligne de commande (`argv[1]`).
   2. `test_saves/vita_pcse01235/EDWN_117938427/EDWN_117938427` (vraie sauvegarde PS Vita).
   3. `test_saves/sample_save.xml` (sauvegarde minimale).
   4. Données de démonstration en mémoire (`save_data_init_demo()`).
6. Exécute la boucle principale (~60 FPS) puis libère proprement toutes les ressources à la fermeture.

#### `src/core/` — Logique métier et manipulation XML
- **`save_data.h`** : Définit la structure `SaveData` contenant :
  - Identité : `file_path`, `farmer_name`, `farm_name`.
  - Statistiques joueur : `money`, `max_stamina`, `max_health`.
  - Compétences (0 à 10) : `farming_level`, `mining_level`, `foraging_level`, `fishing_level`, `combat_level`.
  - Temps : `day_of_month` (1–28), `season_index` (`0` Printemps, `1` Été, `2` Automne, `3` Hiver), `year`.
  - État : `loaded_from_file` (vrai si chargé depuis un fichier XML) et `dirty` (vrai si modifié depuis la dernière sauvegarde).
- **`save_data.c`** :
  - `save_data_init_demo()` : Remplit `SaveData` avec des valeurs par défaut si aucun fichier n'est trouvé.
  - `save_data_load_xml()` : Parse le fichier XML avec `xmlReadFile()`, lit les balises enfants de `<SaveGame>` et `<player>` (`<name>`, `<farmName>`, `<money>`, `<maxStamina>`, `<maxHealth>`, `<farmingLevel>`, etc.).
  - `save_data_save_xml()` :
    1. Crée automatiquement une copie de sécurité `<fichier>.bak` avant toute écriture (`create_backup()`).
    2. Relit l'arbre XML complet pour **ne perdre aucune donnée non éditée** (ferme, bâtiments, coffres, PNJ, quêtes, etc.).
    3. Met à jour uniquement les balises modifiées.
    4. Réécrit le fichier en UTF-8 avec `xmlSaveFormatFileEnc()` et remet `dirty = false`.
  - `save_data_season_name()` : Convertit `season_index` en libellé français pour l'affichage.

#### `src/platform/` — Abstraction macOS / PS Vita
- **`platform.h`** : Définit `SCREEN_WIDTH` (`960`), `SCREEN_HEIGHT` (`544`) et l'énumération `InputAction`.
- **`platform.c`** :
  - `platform_get_saves_dir()` : Retourne `test_saves` sur macOS et `ux0:data/StardewSaveEditor/saves` sur PS Vita.
  - `platform_get_font_path()` : Cherche `assets/font.ttf` (ou `app0:assets/font.ttf` sur PS Vita), avec fallback sur les polices système macOS (`Arial.ttf` / `Helvetica.ttc`).
  - `platform_translate_event()` : Traduit les événements clavier (`SDL_KEYDOWN`) et manette / PS Vita (`SDL_CONTROLLERBUTTONDOWN`) vers le même enum `InputAction`.

#### `src/ui/` — Interface utilisateur maison (SDL2 + SDL2_ttf)
- **`ui.h`** : Définit `UIState` (3 tailles de police : titre `26px`, corps `20px`, petit `15px`, onglet actif, ligne sélectionnée, message de statut temporaire).
- **`ui.c`** :
  - 3 onglets : **Joueur** (Argent ±1000, Énergie max ±10, Santé max ±10), **Compétences** (5 compétences de 0 à 10), **Ferme & Temps** (Jour 1–28, Saison, Année 1–99).
  - Palette chaude inspirée de Stardew Valley (brun bois sombre, bordures dorées, texte parchemin, vert validation).
  - Affiche `*` à côté du nom de la ferme quand des modifications non sauvegardées existent, et un toast vert de 3 secondes lors de la sauvegarde.

#### `test_saves/` — Sauvegardes de test
- **`sample_save.xml`** : Mini-sauvegarde XML légère pour tester rapidement.
- **`vita_pcse01235/EDWN_117938427/`** : Véritable sauvegarde PS Vita (`PCSE01235`, issue de la base open-source *Apollo Save Database*) permettant de tester le parser `libxml2` en conditions réelles sur un fichier complet de 3,8 Mo.

---

## 4. Spécificités des sauvegardes Stardew Valley sur PS Vita

- **Title IDs PS Vita** : `PCSE01235` (US) et `PCSB01235` (EU).
- **Emplacement sur la console** : `ux0:user/00/savedata/<TITLE_ID>/`.
- **Chiffrement** : Sur PS Vita, les sauvegardes système sont chiffrées. Pour les éditer, il faut les **décrypter** (via *VitaShell* → Triangle → *Open Decrypted*, ou via *Apollo Save Tool* / *Vita Save Manager*), les copier dans `ux0:data/StardewSaveEditor/saves/`, les modifier, puis les réimporter / re-signer.
- **Structure double fichier** : Chaque partie Stardew Valley est composée de :
  - `<Nom>_<ID>` : la sauvegarde complète du monde et du joueur.
  - `SaveGameInfo` : un fichier résumé lu par l'écran de chargement du jeu (nom, ferme, argent, date). *Note : pour un support complet en jeu, il faudra synchroniser les champs modifiés dans les deux fichiers.*
- **Version du jeu** : La version officielle PS Vita est restée sur une ancienne branche (~1.02 / 1.2–1.3), donc ses sauvegardes ont une structure XML plus simple que la version PC 1.6.

---

## 5. Installation, compilation et lancement

### Sur macOS (développement natif)

#### Prérequis (Homebrew)
```bash
brew install cmake ninja pkg-config sdl2 sdl2_ttf
```
*(Sur macOS, `clang` et `libxml2` sont déjà fournis par Xcode Command Line Tools.)*

#### Compiler et lancer
```bash
# 1. Configurer le projet (une seule fois)
cmake -G Ninja -B build

# 2. Compiler
cmake --build build

# 3. Lancer (charge la sauvegarde PS Vita de test par défaut)
./build/stardew_save_editor

# Ou ouvrir un fichier XML précis :
./build/stardew_save_editor test_saves/sample_save.xml
```

### Pour PS Vita (`.vpk` via VitaSDK)

Une fois **VitaSDK** installé (`VITASDK=/usr/local/vitasdk`) et les librairies installées via `vdpm install sdl2 sdl2_ttf libxml2` :

```bash
cmake -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
cmake --build build-vita
```

Le fichier **`build-vita/stardew_save_editor.vpk`** généré peut ensuite être :
- testé sur ordinateur dans l'émulateur **Vita3K**, ou
- transféré sur la PS Vita (USB/FTP via **VitaShell**) et installé.

> **Attention pour le build PS Vita** : pense à placer un fichier de police TrueType dans `assets/font.ttf` et à l'inclure dans le `.vpk` (`FILE assets/font.ttf assets/font.ttf` dans `vita_create_vpk`), car la PS Vita ne dispose pas des polices système macOS.

---

## 6. Contrôles

| Action | Clavier (macOS) | Manette / PS Vita |
|---|---|---|
| Naviguer haut / bas | `↑` / `↓` ou `W` / `S` | D-Pad Haut / Bas |
| Diminuer / augmenter une valeur | `←` / `→` ou `A` / `D` | D-Pad Gauche / Droite |
| Onglet précédent / suivant | `Q` / `E` ou `[` / `]` | `L` / `R` |
| Sauvegarder (+ backup `.bak`) | `F5` | `Start` |
| Quitter | `Esc` | `Select` |

---

## 7. Pistes d'évolution

- **Sélecteur de sauvegarde intégré** : lister automatiquement tous les dossiers de sauvegarde présents dans `test_saves/` (Mac) ou `ux0:data/StardewSaveEditor/saves/` (Vita).
- **Synchronisation `SaveGameInfo`** : mettre à jour automatiquement `SaveGameInfo` en même temps que le fichier principal.
- **Nouveaux onglets** : inventaire (objets & quantités), relations avec les villageois (cœurs), météo du lendemain, centre communautaire.
- **Écran tactile & LiveArea** : support tactile SDL2 sur PS Vita + ajout des assets LiveArea (`icon0.png`, `bg.png`, `startup.png`, `template.xml`) pour la bulle sur l'écran d'accueil de la console.
