/*
 * save_data.h — Modèle de données et fonctions de lecture/écriture XML
 * pour les fichiers de sauvegarde Stardew Valley (joueur, temps, bâtiments,
 * inventaire joueur, coffres, éléments de la ferme et synchronisation SaveGameInfo).
 */

#ifndef SAVE_DATA_H
#define SAVE_DATA_H

#include "core/farm_map.h"
#include "core/items_db.h"

#include <stdbool.h>
#include <stdint.h>

#define MAX_BUILDINGS     64
#define MAX_STORAGES      32 /* 0 = Inventaire Joueur, 1..31 = Coffres trouvés */
#define MAX_STORAGE_SLOTS 36 /* Jusqu'à 36 slots par inventaire ou coffre */

/* Type d'élément dynamique posé sur la ferme (affiché sur la mini-carte) */
typedef enum {
    OVERLAY_NONE      = 0,
    OVERLAY_CHEST     = 1, /* Coffre */
    OVERLAY_SPRINKLER = 2, /* Arroseur (Sprinkler / Quality / Iridium) */
    OVERLAY_MACHINE   = 3, /* Machine (Keg, Furnace, Preserves Jar, etc.) */
    OVERLAY_TREE      = 4, /* Arbre ou arbre fruitier */
    OVERLAY_CROP      = 5, /* Terre labourée / culture (HoeDirt) */
    OVERLAY_OTHER_OBJ = 6  /* Pierre, bois, clôture, mauvaise herbe... */
} FarmOverlayType;

/* Représente un bâtiment présent (ou nouvellement ajouté/déplacé/supprimé) sur la ferme */
typedef struct {
    char building_type[32]; /* Nom XML du bâtiment (ex: "Silo", "Barn", "Junimo Hut") */
    int tile_x;             /* Position X actuelle en cases (coin haut-gauche) */
    int tile_y;             /* Position Y actuelle en cases (coin haut-gauche) */
    int orig_tile_x;        /* Position X d'origine dans le XML (pour retrouver le nœud) */
    int orig_tile_y;        /* Position Y d'origine dans le XML */
    int tiles_wide;         /* Largeur en cases */
    int tiles_high;         /* Hauteur en cases */
    int catalog_index;      /* Index dans BUILDING_CATALOG (-1 si déjà présent dans le XML) */
    bool is_new;            /* true si ajouté dans l'éditeur et pas encore écrit dans le XML */
    bool is_moved;          /* true si un bâtiment existant a été déplacé */
    bool is_deleted;        /* true si marqué pour suppression lors de la sauvegarde */
} BuildingEntry;

/* Représente un emplacement (slot) dans l'inventaire du joueur ou dans un coffre */
typedef struct {
    bool occupied;          /* true si un objet est présent dans ce slot */
    bool existed_in_xml;    /* true si ce slot correspondait à un nœud déjà présent dans le XML */
    char name[64];          /* Nom de l'objet affiché */
    int parent_sheet_idx;   /* ID de l'objet (<parentSheetIndex>, -1 pour outils/armes) */
    int stack;              /* Quantité (1..999) */
    int db_index;           /* Index dans ITEMS_DB si nouvel objet ajouté (-1 sinon) */
    bool is_modified;       /* true si ajouté ou quantité modifiée */
    bool is_cleared;        /* true si vidé/supprimé */
} StorageSlot;

/* Représente une zone de stockage : Inventaire Joueur (index 0) ou un Coffre */
typedef struct {
    bool is_player_inv;     /* true = <player>/<items>, false = <Chest>/<items> */
    char label[96];         /* Libellé affiché (ex: "Inventaire Joueur", "Coffre #1 - Farm (55,10)") */
    char location_name[64]; /* Nom du <GameLocation> contenant le coffre */
    int tile_x;             /* Coordonnée X du coffre dans la carte */
    int tile_y;             /* Coordonnée Y du coffre dans la carte */
    int max_slots;          /* Capacité de la zone (12/24/36 pour le joueur, 36 pour un coffre) */
    StorageSlot slots[MAX_STORAGE_SLOTS];
} StorageContainer;

/*
 * Représente les champs éditables d'une sauvegarde Stardew Valley en mémoire.
 */
typedef struct {
    char file_path[512];   /* Chemin complet du fichier XML chargé */
    char farmer_name[64];  /* Nom du fermier (<player><name>) */
    char farm_name[64];    /* Nom de la ferme (<player><farmName>) */

    int money;             /* Argent en pièces d'or (<player><money>) */
    int max_stamina;       /* Énergie maximale (<player><maxStamina>) */
    int max_health;        /* Santé maximale (<player><maxHealth>) */

    int farming_level;     /* Niveau d'agriculture, 0 à 10 (<farmingLevel>) */
    int mining_level;      /* Niveau de minage, 0 à 10 (<miningLevel>) */
    int foraging_level;    /* Niveau de cueillette, 0 à 10 (<foragingLevel>) */
    int fishing_level;     /* Niveau de pêche, 0 à 10 (<fishingLevel>) */
    int combat_level;      /* Niveau de combat, 0 à 10 (<combatLevel>) */

    int day_of_month;      /* Jour du mois, 1 à 28 (<dayOfMonth>) */
    int season_index;      /* Saison : 0 = spring, 1 = summer, 2 = fall, 3 = winter */
    int year;              /* Année en jeu (<year>) */

    BuildingEntry buildings[MAX_BUILDINGS]; /* Bâtiments construits sur la ferme */
    int building_count;                     /* Nombre d'entrées dans buildings[] */

    StorageContainer storages[MAX_STORAGES]; /* 0 = Inventaire Joueur, 1..N = Coffres */
    int storage_count;                       /* Nombre total de zones de stockage */

    /* Couche visuelle des objets/arbres/cultures de la ferme (FarmOverlayType) */
    uint8_t farm_overlay[FARM_HEIGHT][FARM_WIDTH];

    bool loaded_from_file; /* true si les données proviennent d'un vrai fichier XML */
    bool dirty;            /* true si des modifications non enregistrées existent */
} SaveData;

/* Initialise la structure avec des valeurs de démonstration (si aucun fichier chargé). */
void save_data_init_demo(SaveData *data);

/* Charge et parse un fichier de sauvegarde XML Stardew Valley dans data. Retourne true si succès. */
bool save_data_load_xml(SaveData *data, const char *path);

/* Sauvegarde toutes les modifications (joueur, bâtiments, objets, coffres) et synchronise SaveGameInfo. */
bool save_data_save_xml(SaveData *data);

/* Bâtiments */
bool save_data_can_place_building_ignore(const SaveData *data, int x, int y, int w, int h, int ignore_idx);
bool save_data_can_place_building(const SaveData *data, int x, int y, int w, int h);
bool save_data_add_building(SaveData *data, int catalog_index, int tile_x, int tile_y);
bool save_data_move_building(SaveData *data, int index, int new_x, int new_y);
bool save_data_delete_building(SaveData *data, int index);
int save_data_active_building_count(const SaveData *data);

/* Inventaire joueur & Coffres */
bool save_data_add_item(SaveData *data, int storage_idx, int db_index, int qty);
bool save_data_remove_item(SaveData *data, int storage_idx, int slot_idx);
int save_data_storage_used_count(const StorageContainer *storage);

/* Retourne le nom de la saison en français ("Printemps", "Ete", "Automne", "Hiver"). */
const char *save_data_season_name(int season_index);

#endif
