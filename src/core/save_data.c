/*
 * save_data.c — Lecture, modification et sauvegarde des fichiers XML Stardew Valley
 * à l'aide de libxml2 (joueur, temps, bâtiments, débris, overlay mini-carte et SaveGameInfo).
 */

#include "core/save_data.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Valeurs exactes attendues par Stardew Valley dans la balise <currentSeason> */
static const char *SEASONS[4] = { "spring", "summer", "fall", "winter" };

/*
 * Recherche un élément enfant direct nommé `name` sous le nœud XML `parent`.
 */
static xmlNodePtr find_child(xmlNodePtr parent, const char *name) {
    if (!parent) {
        return NULL;
    }
    for (xmlNodePtr cur = parent->children; cur != NULL; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, (const xmlChar *)name) == 0) {
            return cur;
        }
    }
    return NULL;
}

/*
 * Lit le contenu texte d'une balise enfant `name` sous `parent` et le copie dans `dst`.
 */
static void read_string_node(xmlNodePtr parent, const char *name, char *dst, size_t dst_size) {
    xmlNodePtr node = find_child(parent, name);
    if (!node) {
        return;
    }
    xmlChar *content = xmlNodeGetContent(node);
    if (content) {
        snprintf(dst, dst_size, "%s", (const char *)content);
        xmlFree(content);
    }
}

/*
 * Lit le contenu entier d'une balise enfant `name` sous `parent` et le stocke dans `*dst`.
 */
static void read_int_node(xmlNodePtr parent, const char *name, int *dst) {
    xmlNodePtr node = find_child(parent, name);
    if (!node) {
        return;
    }
    xmlChar *content = xmlNodeGetContent(node);
    if (content) {
        *dst = atoi((const char *)content);
        xmlFree(content);
    }
}

/*
 * Remplace la valeur entière d'une balise enfant `name` sous `parent` dans l'arbre XML.
 */
static void write_int_node(xmlNodePtr parent, const char *name, int value) {
    xmlNodePtr node = find_child(parent, name);
    if (!node) {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    xmlNodeSetContent(node, (const xmlChar *)buf);
}

/*
 * Remplace la valeur texte d'une balise enfant `name` sous `parent` dans l'arbre XML.
 */
static void write_string_node(xmlNodePtr parent, const char *name, const char *value) {
    xmlNodePtr node = find_child(parent, name);
    if (!node) {
        return;
    }
    xmlNodeSetContent(node, (const xmlChar *)value);
}

/*
 * Ajoute un nœud enfant `<name>value</name>` (texte) sous `parent`.
 */
static xmlNodePtr add_text_child(xmlNodePtr parent, const char *name, const char *value) {
    return xmlNewTextChild(parent, NULL, (const xmlChar *)name, (const xmlChar *)value);
}

/*
 * Ajoute un nœud enfant `<name>value</name>` (entier) sous `parent`.
 */
static xmlNodePtr add_int_child(xmlNodePtr parent, const char *name, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return add_text_child(parent, name, buf);
}

/*
 * Ajoute un nœud `<tag><X>x</X><Y>y</Y></tag>` (Point / Vector2 XML Stardew Valley).
 */
static void add_point_child(xmlNodePtr parent, const char *tag, int x, int y) {
    xmlNodePtr pt = xmlNewChild(parent, NULL, (const xmlChar *)tag, NULL);
    add_int_child(pt, "X", x);
    add_int_child(pt, "Y", y);
}

/*
 * Ajoute le nœud `<color>` blanc par défaut utilisé par Stardew Valley.
 */
static void add_white_color_child(xmlNodePtr parent, const char *tag) {
    xmlNodePtr col = xmlNewChild(parent, NULL, (const xmlChar *)tag, NULL);
    add_int_child(col, "R", 255);
    add_int_child(col, "G", 255);
    add_int_child(col, "B", 255);
    add_int_child(col, "A", 255);
    add_text_child(col, "PackedValue", "4294967295");
}

/*
 * Recherche le nœud `<GameLocation>` correspondant à la ferme (`<name>Farm</name>`).
 */
static xmlNodePtr find_farm_location(xmlNodePtr root) {
    xmlNodePtr locations = find_child(root, "locations");
    if (!locations) {
        return NULL;
    }
    for (xmlNodePtr loc = locations->children; loc != NULL; loc = loc->next) {
        if (loc->type == XML_ELEMENT_NODE) {
            char loc_name[64] = "";
            read_string_node(loc, "name", loc_name, sizeof(loc_name));
            if (strcmp(loc_name, "Farm") == 0) {
                return loc;
            }
        }
    }
    return NULL;
}

/*
 * Supprime dans un dictionnaire XML de la ferme (`<objects>` ou `<terrainFeatures>`)
 * tous les `<item>` dont la position `(X, Y)` tombe dans le rectangle `[x, x+w[` x `[y, y+h[`.
 */
static void clear_area_in_dict(xmlNodePtr dict_node, int x, int y, int w, int h) {
    if (!dict_node) {
        return;
    }

    xmlNodePtr cur = dict_node->children;
    while (cur != NULL) {
        xmlNodePtr next = cur->next;

        if (cur->type == XML_ELEMENT_NODE) {
            xmlNodePtr key = find_child(cur, "key");
            xmlNodePtr vec = find_child(key, "Vector2");
            if (vec) {
                int ix = -1;
                int iy = -1;
                read_int_node(vec, "X", &ix);
                read_int_node(vec, "Y", &iy);

                if (ix >= x && ix < x + w && iy >= y && iy < y + h) {
                    xmlUnlinkNode(cur);
                    xmlFreeNode(cur);
                }
            }
        }

        cur = next;
    }
}

/*
 * Efface la couche visuelle `farm_overlay` sous le rectangle `(x, y, w, h)` en mémoire.
 */
static void clear_overlay_rect(SaveData *data, int x, int y, int w, int h) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int gx = x + dx;
            int gy = y + dy;
            if (gx >= 0 && gx < FARM_WIDTH && gy >= 0 && gy < FARM_HEIGHT) {
                data->farm_overlay[gy][gx] = OVERLAY_NONE;
            }
        }
    }
}

/*
 * Remplit `data->farm_overlay` à partir de `<objects>` et `<terrainFeatures>` de la ferme
 * pour afficher coffres, arroseurs, machines, arbres et cultures sur la mini-carte.
 */
static void load_farm_overlay(SaveData *data, xmlNodePtr farm) {
    memset(data->farm_overlay, OVERLAY_NONE, sizeof(data->farm_overlay));
    if (!farm) {
        return;
    }

    /* 1. Cultures et arbres dans <terrainFeatures> */
    xmlNodePtr terrain = find_child(farm, "terrainFeatures");
    if (terrain) {
        for (xmlNodePtr cur = terrain->children; cur != NULL; cur = cur->next) {
            if (cur->type != XML_ELEMENT_NODE) {
                continue;
            }
            xmlNodePtr vec = find_child(find_child(cur, "key"), "Vector2");
            xmlNodePtr tf = find_child(find_child(cur, "value"), "TerrainFeature");
            if (!vec || !tf) {
                continue;
            }
            int ix = -1, iy = -1;
            read_int_node(vec, "X", &ix);
            read_int_node(vec, "Y", &iy);
            if (ix < 0 || ix >= FARM_WIDTH || iy < 0 || iy >= FARM_HEIGHT) {
                continue;
            }

            xmlChar *xsi_type = xmlGetProp(tf, (const xmlChar *)"type");
            if (xsi_type) {
                if (strcmp((const char *)xsi_type, "Tree") == 0 ||
                    strcmp((const char *)xsi_type, "FruitTree") == 0) {
                    data->farm_overlay[iy][ix] = OVERLAY_TREE;
                } else if (strcmp((const char *)xsi_type, "HoeDirt") == 0) {
                    data->farm_overlay[iy][ix] = OVERLAY_CROP;
                }
                xmlFree(xsi_type);
            }
        }
    }

    /* 2. Coffres, arroseurs, machines et autres objets dans <objects> (prioritaire visuellement) */
    xmlNodePtr objects = find_child(farm, "objects");
    if (objects) {
        for (xmlNodePtr cur = objects->children; cur != NULL; cur = cur->next) {
            if (cur->type != XML_ELEMENT_NODE) {
                continue;
            }
            xmlNodePtr vec = find_child(find_child(cur, "key"), "Vector2");
            xmlNodePtr obj = find_child(find_child(cur, "value"), "Object");
            if (!vec || !obj) {
                continue;
            }
            int ix = -1, iy = -1;
            read_int_node(vec, "X", &ix);
            read_int_node(vec, "Y", &iy);
            if (ix < 0 || ix >= FARM_WIDTH || iy < 0 || iy >= FARM_HEIGHT) {
                continue;
            }

            char name[64] = "";
            read_string_node(obj, "name", name, sizeof(name));

            if (strcmp(name, "Chest") == 0) {
                data->farm_overlay[iy][ix] = OVERLAY_CHEST;
            } else if (strstr(name, "Sprinkler") != NULL) {
                data->farm_overlay[iy][ix] = OVERLAY_SPRINKLER;
            } else if (strcmp(name, "Keg") == 0 || strcmp(name, "Preserves Jar") == 0 ||
                       strcmp(name, "Furnace") == 0 || strcmp(name, "Mayonnaise Machine") == 0 ||
                       strcmp(name, "Cheese Press") == 0 || strcmp(name, "Bee House") == 0 ||
                       strcmp(name, "Seed Maker") == 0 || strcmp(name, "Crystalarium") == 0 ||
                       strcmp(name, "Lightning Rod") == 0 || strcmp(name, "Oil Maker") == 0) {
                data->farm_overlay[iy][ix] = OVERLAY_MACHINE;
            } else if (data->farm_overlay[iy][ix] == OVERLAY_NONE) {
                data->farm_overlay[iy][ix] = OVERLAY_OTHER_OBJ;
            }
        }
    }
}

/*
 * Recherche dans `<buildings>` le nœud `<Building>` correspondant à `(orig_x, orig_y)` et `type`.
 */
static xmlNodePtr find_building_node(xmlNodePtr buildings_node, const BuildingEntry *entry) {
    if (!buildings_node) {
        return NULL;
    }
    for (xmlNodePtr b = buildings_node->children; b != NULL; b = b->next) {
        if (b->type != XML_ELEMENT_NODE) {
            continue;
        }
        int bx = -1, by = -1;
        char btype[32] = "";
        read_int_node(b, "tileX", &bx);
        read_int_node(b, "tileY", &by);
        read_string_node(b, "buildingType", btype, sizeof(btype));
        if (bx == entry->orig_tile_x && by == entry->orig_tile_y &&
            strcmp(btype, entry->building_type) == 0) {
            return b;
        }
    }
    return NULL;
}

/*
 * Construit un nœud XML `<Building>` compatible Stardew Valley PS Vita (1.2)
 * et l'ajoute dans `<buildings>`.
 */
static void append_building_xml(xmlNodePtr root, xmlNodePtr buildings_node, const BuildingEntry *entry) {
    if (entry->catalog_index < 0 || entry->catalog_index >= BUILDING_CATALOG_COUNT) {
        return;
    }
    const BuildingBlueprint *bp = &BUILDING_CATALOG[entry->catalog_index];

    xmlNsPtr xsi_ns = xmlSearchNs(root->doc, root, (const xmlChar *)"xsi");

    xmlNodePtr b = xmlNewChild(buildings_node, NULL, (const xmlChar *)"Building", NULL);
    if (bp->xsi_type != NULL) {
        if (xsi_ns) {
            xmlNewNsProp(b, xsi_ns, (const xmlChar *)"type", (const xmlChar *)bp->xsi_type);
        } else {
            xmlNewProp(b, (const xmlChar *)"xsi:type", (const xmlChar *)bp->xsi_type);
        }
    }

    char indoors_unique[64];
    snprintf(indoors_unique, sizeof(indoors_unique), "%s%d",
             bp->indoors_name, entry->tile_x * 2000 + entry->tile_y);

    add_int_child(b, "tileX", entry->tile_x);
    add_int_child(b, "tileY", entry->tile_y);
    add_int_child(b, "tilesWide", bp->tiles_wide);
    add_int_child(b, "tilesHigh", bp->tiles_high);
    add_int_child(b, "maxOccupants", bp->max_occupants);
    add_int_child(b, "currentOccupants", 0);
    add_int_child(b, "daysOfConstructionLeft", 0);
    add_int_child(b, "daysUntilUpgrade", 0);
    add_text_child(b, "buildingType", bp->xml_type);
    add_text_child(b, "nameOfIndoors", indoors_unique);
    add_text_child(b, "baseNameOfIndoors", bp->indoors_name);
    add_text_child(b, "nameOfIndoorsWithoutUnique", bp->indoors_name);

    if (bp->indoors_type != NULL) {
        xmlNodePtr ind = xmlNewChild(b, NULL, (const xmlChar *)"indoors", NULL);
        if (xsi_ns) {
            xmlNewNsProp(ind, xsi_ns, (const xmlChar *)"type", (const xmlChar *)bp->indoors_type);
        } else {
            xmlNewProp(ind, (const xmlChar *)"xsi:type", (const xmlChar *)bp->indoors_type);
        }
        xmlNewChild(ind, NULL, (const xmlChar *)"characters", NULL);
        xmlNewChild(ind, NULL, (const xmlChar *)"objects", NULL);
        xmlNewChild(ind, NULL, (const xmlChar *)"terrainFeatures", NULL);
        xmlNewChild(ind, NULL, (const xmlChar *)"debris", NULL);
        add_text_child(ind, "name", bp->indoors_name);
        add_white_color_child(ind, "waterColor");
        add_text_child(ind, "isFarm", "true");
        add_text_child(ind, "isOutdoors", "false");
        add_text_child(ind, "isStructure", "true");
        add_text_child(ind, "ignoreDebrisWeather", "false");
        add_text_child(ind, "ignoreOutdoorLighting", "false");
        add_text_child(ind, "ignoreLights", "false");
        add_text_child(ind, "treatAsOutdoors", "false");
        add_int_child(ind, "numberOfSpawnedObjectsOnMap", 0);

        if (strcmp(bp->indoors_type, "AnimalHouse") == 0) {
            xmlNewChild(ind, NULL, (const xmlChar *)"animals", NULL);
            add_int_child(ind, "animalLimit", bp->max_occupants);
            xmlNewChild(ind, NULL, (const xmlChar *)"animalsThatLiveHere", NULL);
            add_point_child(ind, "incubatingEgg", -1, -1);
        }
    }

    add_point_child(b, "humanDoor", bp->human_door_x, bp->human_door_y);
    add_point_child(b, "animalDoor", bp->animal_door_x, bp->animal_door_y);
    add_white_color_child(b, "color");
    add_text_child(b, "animalDoorOpen", "false");
    add_text_child(b, "magical", bp->magical ? "true" : "false");
    add_text_child(b, "owner", "-6666666");

    if (bp->xsi_type != NULL && strcmp(bp->xsi_type, "JunimoHut") == 0) {
        add_text_child(b, "noHarvest", "false");
    }
}

/*
 * Convertit le nom anglais de saison en index entier (0 à 3).
 */
static int season_from_string(const char *s) {
    for (int i = 0; i < 4; i++) {
        if (strcmp(s, SEASONS[i]) == 0) {
            return i;
        }
    }
    return 0;
}

/*
 * Crée une copie de sécurité `<path>.bak` avant toute écriture.
 */
static bool create_backup(const char *path) {
    char bak_path[520];
    snprintf(bak_path, sizeof(bak_path), "%s.bak", path);

    FILE *src = fopen(path, "rb");
    if (!src) {
        return false;
    }
    FILE *dst = fopen(bak_path, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }

    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, n, dst);
    }

    fclose(src);
    fclose(dst);
    return true;
}

/*
 * Synchronise le fichier `SaveGameInfo` situé dans le même dossier que `data->file_path`
 * afin que l'écran de chargement du jeu affiche l'argent, les niveaux et la date à jour.
 */
static void sync_save_game_info(const SaveData *data) {
    const char *slash = strrchr(data->file_path, '/');
    if (!slash) {
        return;
    }

    char info_path[540];
    size_t dir_len = (size_t)(slash - data->file_path);
    if (dir_len + 16 >= sizeof(info_path)) {
        return;
    }
    memcpy(info_path, data->file_path, dir_len);
    snprintf(info_path + dir_len, sizeof(info_path) - dir_len, "/SaveGameInfo");

    FILE *check = fopen(info_path, "rb");
    if (!check) {
        return;
    }
    fclose(check);

    create_backup(info_path);

    xmlDocPtr doc = xmlReadFile(info_path, NULL, 0);
    if (!doc) {
        return;
    }

    xmlNodePtr farmer = xmlDocGetRootElement(doc);
    if (farmer) {
        write_int_node(farmer, "money", data->money);
        write_int_node(farmer, "maxStamina", data->max_stamina);
        write_int_node(farmer, "maxHealth", data->max_health);
        write_int_node(farmer, "farmingLevel", data->farming_level);
        write_int_node(farmer, "miningLevel", data->mining_level);
        write_int_node(farmer, "foragingLevel", data->foraging_level);
        write_int_node(farmer, "fishingLevel", data->fishing_level);
        write_int_node(farmer, "combatLevel", data->combat_level);
        write_int_node(farmer, "dayOfMonthForSaveGame", data->day_of_month);
        write_int_node(farmer, "seasonForSaveGame", data->season_index % 4);
        write_int_node(farmer, "yearForSaveGame", data->year);
    }

    xmlSaveFormatFileEnc(info_path, doc, "UTF-8", 1);
    xmlFreeDoc(doc);
}

/*
 * Initialise `data` avec des valeurs d'exemple (mode démo).
 */
void save_data_init_demo(SaveData *data) {
    memset(data, 0, sizeof(*data));
    snprintf(data->farmer_name, sizeof(data->farmer_name), "Alexis");
    snprintf(data->farm_name, sizeof(data->farm_name), "Pelican");
    data->money = 5000;
    data->max_stamina = 270;
    data->max_health = 100;
    data->farming_level = 3;
    data->mining_level = 2;
    data->foraging_level = 2;
    data->fishing_level = 1;
    data->combat_level = 1;
    data->day_of_month = 12;
    data->season_index = 0;
    data->year = 1;
    data->building_count = 0;
    data->loaded_from_file = false;
    data->dirty = false;
}

/*
 * Ouvre et parse le fichier XML situé à `path`, puis extrait joueur, temps,
 * bâtiments et couche visuelle de la ferme (coffres, arroseurs, arbres, cultures).
 */
bool save_data_load_xml(SaveData *data, const char *path) {
    xmlDocPtr doc = xmlReadFile(path, NULL, 0);
    if (!doc) {
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        xmlFreeDoc(doc);
        return false;
    }

    save_data_init_demo(data);
    snprintf(data->file_path, sizeof(data->file_path), "%s", path);

    xmlNodePtr player = find_child(root, "player");
    if (player) {
        read_string_node(player, "name", data->farmer_name, sizeof(data->farmer_name));
        read_string_node(player, "farmName", data->farm_name, sizeof(data->farm_name));
        read_int_node(player, "money", &data->money);
        read_int_node(player, "maxStamina", &data->max_stamina);
        read_int_node(player, "maxHealth", &data->max_health);
        read_int_node(player, "farmingLevel", &data->farming_level);
        read_int_node(player, "miningLevel", &data->mining_level);
        read_int_node(player, "foragingLevel", &data->foraging_level);
        read_int_node(player, "fishingLevel", &data->fishing_level);
        read_int_node(player, "combatLevel", &data->combat_level);
    }

    read_int_node(root, "dayOfMonth", &data->day_of_month);
    read_int_node(root, "year", &data->year);

    char season_buf[32] = "spring";
    read_string_node(root, "currentSeason", season_buf, sizeof(season_buf));
    data->season_index = season_from_string(season_buf);

    xmlNodePtr farm = find_farm_location(root);
    load_farm_overlay(data, farm);

    xmlNodePtr buildings = find_child(farm, "buildings");
    if (buildings) {
        for (xmlNodePtr b = buildings->children; b != NULL; b = b->next) {
            if (b->type != XML_ELEMENT_NODE || data->building_count >= MAX_BUILDINGS) {
                continue;
            }
            BuildingEntry *entry = &data->buildings[data->building_count];
            memset(entry, 0, sizeof(*entry));
            read_string_node(b, "buildingType", entry->building_type, sizeof(entry->building_type));
            read_int_node(b, "tileX", &entry->tile_x);
            read_int_node(b, "tileY", &entry->tile_y);
            entry->orig_tile_x = entry->tile_x;
            entry->orig_tile_y = entry->tile_y;
            read_int_node(b, "tilesWide", &entry->tiles_wide);
            read_int_node(b, "tilesHigh", &entry->tiles_high);
            entry->catalog_index = -1;
            entry->is_new = false;
            entry->is_moved = false;
            entry->is_deleted = false;
            data->building_count++;
        }
    }

    data->loaded_from_file = true;
    data->dirty = false;

    xmlFreeDoc(doc);
    return true;
}

/*
 * Vérifie si un bâtiment de taille (w, h) peut être posé en (x, y) sans collision,
 * en ignorant éventuellement le bâtiment d'index `ignore_idx` (utile pour déplacer un bâtiment).
 */
bool save_data_can_place_building_ignore(const SaveData *data, int x, int y, int w, int h, int ignore_idx) {
    if (!farm_map_terrain_free(x, y, w, h)) {
        return false;
    }
    for (int i = 0; i < data->building_count; i++) {
        if (i == ignore_idx || data->buildings[i].is_deleted) {
            continue;
        }
        const BuildingEntry *b = &data->buildings[i];
        bool overlap_x = (x < b->tile_x + b->tiles_wide) && (x + w > b->tile_x);
        bool overlap_y = (y < b->tile_y + b->tiles_high) && (y + h > b->tile_y);
        if (overlap_x && overlap_y) {
            return false;
        }
    }
    return true;
}

/*
 * Vérifie si un nouveau bâtiment de taille (w, h) peut être posé en (x, y).
 */
bool save_data_can_place_building(const SaveData *data, int x, int y, int w, int h) {
    return save_data_can_place_building_ignore(data, x, y, w, h, -1);
}

/*
 * Ajoute un bâtiment du catalogue (`BUILDING_CATALOG[catalog_index]`) en (tile_x, tile_y).
 */
bool save_data_add_building(SaveData *data, int catalog_index, int tile_x, int tile_y) {
    if (catalog_index < 0 || catalog_index >= BUILDING_CATALOG_COUNT) {
        return false;
    }
    if (data->building_count >= MAX_BUILDINGS) {
        return false;
    }
    const BuildingBlueprint *bp = &BUILDING_CATALOG[catalog_index];
    if (!save_data_can_place_building(data, tile_x, tile_y, bp->tiles_wide, bp->tiles_high)) {
        return false;
    }

    BuildingEntry *entry = &data->buildings[data->building_count];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->building_type, sizeof(entry->building_type), "%s", bp->xml_type);
    entry->tile_x = tile_x;
    entry->tile_y = tile_y;
    entry->orig_tile_x = tile_x;
    entry->orig_tile_y = tile_y;
    entry->tiles_wide = bp->tiles_wide;
    entry->tiles_high = bp->tiles_high;
    entry->catalog_index = catalog_index;
    entry->is_new = true;
    entry->is_moved = false;
    entry->is_deleted = false;

    clear_overlay_rect(data, tile_x, tile_y, bp->tiles_wide, bp->tiles_high);
    data->building_count++;
    data->dirty = true;
    return true;
}

/*
 * Déplace le bâtiment `index` vers `(new_x, new_y)` si la destination est valide.
 */
bool save_data_move_building(SaveData *data, int index, int new_x, int new_y) {
    if (index < 0 || index >= data->building_count || data->buildings[index].is_deleted) {
        return false;
    }
    BuildingEntry *b = &data->buildings[index];
    if (!save_data_can_place_building_ignore(data, new_x, new_y, b->tiles_wide, b->tiles_high, index)) {
        return false;
    }

    b->tile_x = new_x;
    b->tile_y = new_y;
    if (!b->is_new) {
        b->is_moved = true;
    }
    clear_overlay_rect(data, new_x, new_y, b->tiles_wide, b->tiles_high);
    data->dirty = true;
    return true;
}

/*
 * Marque un bâtiment existant pour suppression (ou le retire immédiatement s'il était nouveau).
 */
bool save_data_delete_building(SaveData *data, int index) {
    if (index < 0 || index >= data->building_count || data->buildings[index].is_deleted) {
        return false;
    }
    if (data->buildings[index].is_new) {
        /* Jamais écrit dans le XML : on peut le retirer directement du tableau */
        for (int i = index; i < data->building_count - 1; i++) {
            data->buildings[i] = data->buildings[i + 1];
        }
        data->building_count--;
    } else {
        data->buildings[index].is_deleted = true;
    }
    data->dirty = true;
    return true;
}

/*
 * Retourne le nombre de bâtiments actifs (non supprimés) sur la ferme.
 */
int save_data_active_building_count(const SaveData *data) {
    int count = 0;
    for (int i = 0; i < data->building_count; i++) {
        if (!data->buildings[i].is_deleted) {
            count++;
        }
    }
    return count;
}

/*
 * Écrit toutes les modifications dans le fichier XML principal (+ backup .bak)
 * et synchronise le fichier `SaveGameInfo` adjacent.
 */
bool save_data_save_xml(SaveData *data) {
    if (!data->loaded_from_file || data->file_path[0] == '\0') {
        return false;
    }

    create_backup(data->file_path);

    xmlDocPtr doc = xmlReadFile(data->file_path, NULL, 0);
    if (!doc) {
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr player = find_child(root, "player");
    if (player) {
        write_int_node(player, "money", data->money);
        write_int_node(player, "maxStamina", data->max_stamina);
        write_int_node(player, "maxHealth", data->max_health);
        write_int_node(player, "farmingLevel", data->farming_level);
        write_int_node(player, "miningLevel", data->mining_level);
        write_int_node(player, "foragingLevel", data->foraging_level);
        write_int_node(player, "fishingLevel", data->fishing_level);
        write_int_node(player, "combatLevel", data->combat_level);
    }

    write_int_node(root, "dayOfMonth", data->day_of_month);
    write_int_node(root, "year", data->year);
    write_string_node(root, "currentSeason", SEASONS[data->season_index % 4]);

    /* Applique suppressions, déplacements et ajouts sur <locations>/Farm/<buildings> */
    xmlNodePtr farm = find_farm_location(root);
    if (farm) {
        xmlNodePtr buildings = find_child(farm, "buildings");
        xmlNodePtr objects = find_child(farm, "objects");
        xmlNodePtr terrain = find_child(farm, "terrainFeatures");
        if (!buildings) {
            buildings = xmlNewChild(farm, NULL, (const xmlChar *)"buildings", NULL);
        }

        for (int i = 0; i < data->building_count; i++) {
            BuildingEntry *b = &data->buildings[i];
            if (b->is_deleted) {
                xmlNodePtr node = find_building_node(buildings, b);
                if (node) {
                    xmlUnlinkNode(node);
                    xmlFreeNode(node);
                }
            } else if (b->is_moved) {
                xmlNodePtr node = find_building_node(buildings, b);
                if (node) {
                    clear_area_in_dict(objects, b->tile_x, b->tile_y, b->tiles_wide, b->tiles_high);
                    clear_area_in_dict(terrain, b->tile_x, b->tile_y, b->tiles_wide, b->tiles_high);
                    write_int_node(node, "tileX", b->tile_x);
                    write_int_node(node, "tileY", b->tile_y);
                }
            } else if (b->is_new) {
                clear_area_in_dict(objects, b->tile_x, b->tile_y, b->tiles_wide, b->tiles_high);
                clear_area_in_dict(terrain, b->tile_x, b->tile_y, b->tiles_wide, b->tiles_high);
                append_building_xml(root, buildings, b);
            }
        }
    }

    int res = xmlSaveFormatFileEnc(data->file_path, doc, "UTF-8", 1);
    xmlFreeDoc(doc);

    if (res >= 0) {
        /* Nettoie le tableau en mémoire après écriture réussie */
        int write_idx = 0;
        for (int i = 0; i < data->building_count; i++) {
            if (!data->buildings[i].is_deleted) {
                BuildingEntry b = data->buildings[i];
                b.is_new = false;
                b.is_moved = false;
                b.orig_tile_x = b.tile_x;
                b.orig_tile_y = b.tile_y;
                data->buildings[write_idx++] = b;
            }
        }
        data->building_count = write_idx;
        data->dirty = false;

        /* Met à jour SaveGameInfo à côté de la sauvegarde */
        sync_save_game_info(data);
        return true;
    }
    return false;
}

/*
 * Retourne le nom lisible en français correspondant à l'index de saison (0 à 3).
 */
const char *save_data_season_name(int season_index) {
    switch (season_index % 4) {
        case 0: return "Printemps";
        case 1: return "Ete";
        case 2: return "Automne";
        case 3: return "Hiver";
        default: return "Printemps";
    }
}
