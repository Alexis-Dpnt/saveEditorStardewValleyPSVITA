# Stardew Valley Save Editor — PS Vita

![C11](https://img.shields.io/badge/C-11-blue)
![SDL2](https://img.shields.io/badge/UI-SDL2%20%2B%20SDL2__ttf-orange)
![Platform](https://img.shields.io/badge/Platform-PS%20Vita%20%7C%20macOS-green)

Éditeur de sauvegarde natif pour **Stardew Valley (PS Vita — v1.2)** écrit en **C11** avec **SDL2**, **SDL2_ttf** et **libxml2**.  
Grâce à une architecture **double cible (macOS natif + PS Vita `.vpk`)**, le même code compile en 1 seconde sur macOS à la résolution exacte de la console (**960×544**) et en homebrew `.vpk` via **VitaSDK**.

---

## Fonctionnalités

- **Sélecteur de sauvegardes intégré** : scanne automatiquement `test_saves/` (macOS) ou `ux0:data/StardewSaveEditor/saves/` (PS Vita) pour charger n'importe quelle partie.
- **Édition du joueur & du temps** :
  - **Joueur** : argent (`po`), énergie maximale, santé maximale.
  - **Compétences** : Agriculture, Minage, Cueillette, Pêche, Combat (`0` à `10`).
  - **Ferme & Temps** : jour (`1–28`), saison (`Printemps`, `Été`, `Automne`, `Hiver`), année.
- **Constructeur & gestionnaire de bâtiments avec mini-carte `80×65`** :
  - Grille exacte de la Ferme Standard extraite des fichiers du jeu PS Vita (`Farm.xnb`).
  - Affichage en temps réel des éléments dynamiques de la sauvegarde : **coffres**, **arroseurs**, **machines**, **arbres**, **cultures** et **bâtiments**.
  - **Ajout** des **16 bâtiments compatibles PS Vita 1.2** (`Silo`, `Well`, `Stable`, `Mill`, `Shed`, `Coop`/`Big`/`Deluxe`, `Barn`/`Big`/`Deluxe`, `Slime Hutch`, `Junimo Hut`, `Earth Obelisk`, `Water Obelisk`, `Gold Clock`) avec détection des collisions.
  - **Déplacement** et **suppression** des bâtiments existants.
  - **Nettoyage automatique des débris** (`<objects>` et `<terrainFeatures>`) sous chaque bâtiment ajouté ou déplacé.
- **Sauvegarde sécurisée** :
  - Backup automatique `.bak` avant chaque écriture.
  - Préservation intégrale de l'arbre XML (ferme, PNJ, quêtes, inventaire…).
  - **Synchronisation automatique de `SaveGameInfo`** pour que l'écran de chargement du jeu reflète les nouvelles valeurs.

---

## Architecture

Le projet est structuré en **3 couches découplées**, orchestrées par `src/main.c` :

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
│  Mac / Vita   │      │  Map & SaveData │      │ SDL2 + TTF UI │
└───────────────┘      └─────────────────┘      └───────────────┘
```

### Arborescence

```text
saveEditorStardewValleyPSVITA/
├── CMakeLists.txt                       # Build dual-target : macOS natif + PS Vita (.vpk)
├── README.md
├── assets/
│   └── font.ttf                         # Police embarquée dans le .vpk (app0:assets/font.ttf)
├── sce_sys/                             # Assets LiveArea PS Vita (PNG 8-bit indexés)
│   ├── icon0.png                        # Icône de la bulle (128×128)
│   └── livearea/contents/
│       ├── bg.png                       # Fond LiveArea (840×500)
│       ├── startup.png                  # Bouton de lancement (280×158)
│       └── template.xml
├── src/
│   ├── main.c                           # Point d'entrée, init SDL2/TTF/libxml2, boucle 60 FPS
│   ├── core/
│   │   ├── farm_map.h                   # Grille 80×65 Ferme Standard + catalogue 16 bâtiments
│   │   ├── save_data.h / save_data.c    # Lecture/écriture XML, bâtiments, débris, SaveGameInfo
│   ├── platform/
│   │   └── platform.h / platform.c      # Chemins Mac/Vita, scanner opendir/readdir, inputs
│   └── ui/
│       └── ui.h / ui.c                  # Interface 5 onglets + mini-carte interactive 960×544
└── test_saves/                          # Sauvegardes XML de test locales
```

---

## Compilation & Lancement

### 1. Sur macOS (développement natif)

#### Prérequis
```bash
brew install cmake ninja pkg-config sdl2 sdl2_ttf
```
*(Clang et `libxml2` sont fournis par Xcode Command Line Tools.)*

#### Compiler et exécuter
```bash
cmake -G Ninja -B build
cmake --build build
./build/stardew_save_editor
```
Il est également possible de passer un fichier de sauvegarde directement en argument :
```bash
./build/stardew_save_editor chemin/vers/MaSauvegarde
```

### 2. Pour PS Vita (`.vpk` via VitaSDK)

#### Prérequis
Installer [VitaSDK](https://vitasdk.org/) (ex : dans `~/vitasdk`) puis les dépendances cibles :
```bash
vdpm install sdl2 sdl2_ttf libxml2
```

#### Compiler le `.vpk`
```bash
export VITASDK=$HOME/vitasdk
export PATH=$VITASDK/bin:$PATH

cmake -G Ninja -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
cmake --build build-vita
```
Le package prêt à installer est généré dans **`build-vita/stardew_save_editor.vpk`**.

---

## Installation & Utilisation sur PS Vita

1. **Installer l'homebrew** : transférer `build-vita/stardew_save_editor.vpk` sur la PS Vita (USB/FTP via **VitaShell**) et l'installer.
2. **Exporter sa sauvegarde décryptée** :
   - Dans **VitaShell**, aller sur `ux0:user/00/savedata/PCSB01226` *(EU)* ou `PCSE01235` *(US)*.
   - Appuyer sur **Triangle → `Open decrypted`**.
   - Copier le dossier de la partie (ex : `Alexis_455215412/`) dans **`ux0:data/StardewSaveEditor/saves/`**.
3. **Éditer** : lancer **Stardew Save Editor** depuis le LiveArea, sélectionner la partie dans l'onglet **Sauvegardes**, faire ses modifications et sauvegarder (**Start**).
4. **Réimporter** : dans **VitaShell**, recopier le dossier modifié depuis `ux0:data/StardewSaveEditor/saves/` vers `ux0:user/00/savedata/<TITLE_ID>/` ouvert via **Triangle → `Open decrypted`** (VitaShell re-chiffre automatiquement la sauvegarde pour la console).

---

## Contrôles

| Action | Clavier (macOS) | Manette / PS Vita |
|---|---|---|
| Naviguer / Déplacer le curseur | `↑` `↓` `←` `→` ou `W` `A` `S` `D` | D-Pad |
| Changer d'onglet | `Q` / `E` ou `[` / `]` | `L` / `R` |
| Valider / Charger / Poser / Déplacer | `Entrée` ou `Espace` | `Croix` |
| Annuler / Retour | `Backspace` | `Rond` |
| Basculer **Ajouter ↔ Existants** *(onglet Bâtiments)* | `Tab` | `Carré` |
| Supprimer le bâtiment sélectionné | `X` ou `Suppr` | `Triangle` |
| Enregistrer (+ backup `.bak` + `SaveGameInfo`) | `F5` | `Start` |
| Quitter | `Esc` | `Select` |
