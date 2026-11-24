/*
 * save_data.h — Modèle de données et fonctions de lecture/écriture XML
 * pour les fichiers de sauvegarde Stardew Valley.
 */

#ifndef SAVE_DATA_H
#define SAVE_DATA_H

#include <stdbool.h>

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

    bool loaded_from_file; /* true si les données proviennent d'un vrai fichier XML */
    bool dirty;            /* true si des modifications non enregistrées existent */

    
} SaveData;

/* Initialise la structure avec des valeurs de démonstration (si aucun fichier chargé). */
void save_data_init_demo(SaveData *data);

/* Charge et parse un fichier de sauvegarde XML Stardew Valley dans data. Retourne true si succès. */
bool save_data_load_xml(SaveData *data, const char *path);

/* Sauvegarde les modifications dans le fichier XML d'origine (en créant d'abord un backup .bak). */
bool save_data_save_xml(SaveData *data);

/* Retourne le nom de la saison en français ("Printemps", "Ete", "Automne", "Hiver"). */
const char *save_data_season_name(int season_index);

#endif
