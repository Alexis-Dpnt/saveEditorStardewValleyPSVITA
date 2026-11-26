/*
 * save_data.c — Lecture, modification et sauvegarde des fichiers XML Stardew Valley
 * à l'aide de libxml2 (joueur, temps, bâtiments, inventaire, coffres, débris,
 * overlay mini-carte et synchronisation SaveGameInfo).
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
 * Recherche le nœud `<GameLocation>` nommé `target_name` (ex: "Farm", "FarmHouse").
 */
static xmlNodePtr find_location_by_name(xmlNodePtr root, const char *target_name) {
    xmlNodePtr locations = find_child(root, "locations");
    if (!locations) {
        return NULL;
    }
    for (xmlNodePtr loc = locations->children; loc != NULL; loc = loc->next) {
        if (loc->type == XML_ELEMENT_NODE) {
            char loc_name[64] = "";
            read_string_node(loc, "name", loc_name, sizeof(loc_name));
            if (strcmp(loc_name, target_name) == 0) {
                return loc;
            }
        }
    }
    return NULL;
}

static xmlNodePtr find_farm_location(xmlNodePtr root) {
    return find_location_by_name(root, "Farm");
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
 * Remplit `data->farm_overlay` à partir de `<objects>` et `<terrainFeatures>` de la ferme.
 */
static void load_farm_overlay(SaveData *data, xmlNodePtr farm) {
    memset(data->farm_overlay, OVERLAY_NONE, sizeof(data->farm_overlay));
    if (!farm) {
        return;
    }

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
 * Lit les éléments `<Item>` d'un nœud `<items>` (inventaire joueur ou coffre)
 * dans `storage->slots[]`.
 */
static void load_items_list(StorageContainer *storage, xmlNodePtr items_node) {
    if (!items_node) {
        return;
    }
    int slot = 0;
    for (xmlNodePtr cur = items_node->children; cur != NULL && slot < storage->max_slots; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) {
            continue;
        }
        StorageSlot *st = &storage->slots[slot];
        memset(st, 0, sizeof(*st));
        st->existed_in_xml = true;
        st->parent_sheet_idx = -1;
        st->db_index = -1;

        /* Un slot vide dans l'inventaire porte l'attribut xsi:nil="true" */
        xmlChar *nil_attr = xmlGetProp(cur, (const xmlChar *)"nil");
        bool is_nil = (nil_attr != NULL && strcmp((const char *)nil_attr, "true") == 0);
        if (nil_attr) {
            xmlFree(nil_attr);
        }

        if (!is_nil) {
            char name[64] = "";
            read_string_node(cur, "Name", name, sizeof(name));
            if (name[0] == '\0') {
                read_string_node(cur, "name", name, sizeof(name));
            }
            if (name[0] == '\0') {
                snprintf(name, sizeof(name), "Objet");
            }
            snprintf(st->name, sizeof(st->name), "%s", name);

            st->stack = 1;
            if (find_child(cur, "Stack")) {
                read_int_node(cur, "Stack", &st->stack);
            } else if (find_child(cur, "stack")) {
                read_int_node(cur, "stack", &st->stack);
            }
            if (st->stack <= 0) {
                st->stack = 1;
            }

            /* Seuls les <Item xsi:type="Object"> simples possèdent un parentSheetIndex empilable */
            xmlChar *xsi_type = xmlGetProp(cur, (const xmlChar *)"type");
            if (xsi_type) {
                if (strcmp((const char *)xsi_type, "Object") == 0) {
                    read_int_node(cur, "parentSheetIndex", &st->parent_sheet_idx);
                }
                xmlFree(xsi_type);
            }

            st->occupied = true;
        }
        slot++;
    }
}

/*
 * Charge l'inventaire du joueur (index 0) puis tous les coffres présents sur les cartes
 * (Farm en priorité, puis FarmHouse et les autres lieux) dans `data->storages[]`.
 */
static void load_all_storages(SaveData *data, xmlNodePtr root, xmlNodePtr player) {
    data->storage_count = 0;

    /* Index 0 : toujours l'inventaire du joueur */
    StorageContainer *pinv = &data->storages[0];
    memset(pinv, 0, sizeof(*pinv));
    pinv->is_player_inv = true;
    snprintf(pinv->label, sizeof(pinv->label), "Inventaire Joueur");
    pinv->max_slots = MAX_STORAGE_SLOTS;
    if (player) {
        int max_items = MAX_STORAGE_SLOTS;
        read_int_node(player, "maxItems", &max_items);
        if (max_items > 0 && max_items <= MAX_STORAGE_SLOTS) {
            pinv->max_slots = max_items;
        }
        load_items_list(pinv, find_child(player, "items"));
    }
    data->storage_count = 1;

    /* Index 1..N : tous les coffres (<Object xsi:type="Chest">) trouvés dans <locations> */
    xmlNodePtr locations = find_child(root, "locations");
    if (!locations) {
        return;
    }

    int chest_num = 1;
    for (xmlNodePtr loc = locations->children; loc != NULL && data->storage_count < MAX_STORAGES; loc = loc->next) {
        if (loc->type != XML_ELEMENT_NODE) {
            continue;
        }
        char loc_name[64] = "";
        read_string_node(loc, "name", loc_name, sizeof(loc_name));

        xmlNodePtr objects = find_child(loc, "objects");
        if (!objects) {
            continue;
        }

        for (xmlNodePtr cur = objects->children; cur != NULL && data->storage_count < MAX_STORAGES; cur = cur->next) {
            if (cur->type != XML_ELEMENT_NODE) {
                continue;
            }
            xmlNodePtr vec = find_child(find_child(cur, "key"), "Vector2");
            xmlNodePtr obj = find_child(find_child(cur, "value"), "Object");
            if (!vec || !obj) {
                continue;
            }

            xmlChar *xsi_type = xmlGetProp(obj, (const xmlChar *)"type");
            bool is_chest = (xsi_type != NULL && strcmp((const char *)xsi_type, "Chest") == 0);
            if (xsi_type) {
                xmlFree(xsi_type);
            }
            if (!is_chest) {
                continue;
            }

            int cx = 0, cy = 0;
            read_int_node(vec, "X", &cx);
            read_int_node(vec, "Y", &cy);

            StorageContainer *chest = &data->storages[data->storage_count];
            memset(chest, 0, sizeof(*chest));
            chest->is_player_inv = false;
            snprintf(chest->location_name, sizeof(chest->location_name), "%s", loc_name);
            chest->tile_x = cx;
            chest->tile_y = cy;
            chest->max_slots = MAX_STORAGE_SLOTS;
            snprintf(chest->label, sizeof(chest->label), "Coffre #%d - %s (%d, %d)",
                     chest_num, loc_name, cx, cy);

            load_items_list(chest, find_child(obj, "items"));
            data->storage_count++;
            chest_num++;
        }
    }
}

/*
 * Crée un nœud XML `<Item xsi:nil="true" />` (slot vide dans l'inventaire joueur).
 */
static xmlNodePtr create_nil_item_node(xmlNodePtr root) {
    xmlNsPtr xsi_ns = xmlSearchNs(root->doc, root, (const xmlChar *)"xsi");
    xmlNodePtr item = xmlNewNode(NULL, (const xmlChar *)"Item");
    if (xsi_ns) {
        xmlNewNsProp(item, xsi_ns, (const xmlChar *)"nil", (const xmlChar *)"true");
    } else {
        xmlNewProp(item, (const xmlChar *)"xsi:nil", (const xmlChar *)"true");
    }
    return item;
}

/*
 * Crée un nœud XML `<Item xsi:type="Object">` complet et compatible Stardew Valley PS Vita 1.2
 * à partir d'une entrée de `ITEMS_DB[db_index]` et d'une quantité `stack`.
 */
static xmlNodePtr create_object_item_node(xmlNodePtr root, int db_index, int stack) {
    const ItemDef *def = &ITEMS_DB[db_index];
    xmlNsPtr xsi_ns = xmlSearchNs(root->doc, root, (const xmlChar *)"xsi");

    xmlNodePtr item = xmlNewNode(NULL, (const xmlChar *)"Item");
    if (xsi_ns) {
        xmlNewNsProp(item, xsi_ns, (const xmlChar *)"type", (const xmlChar *)"Object");
    } else {
        xmlNewProp(item, (const xmlChar *)"xsi:type", (const xmlChar *)"Object");
    }

    add_int_child(item, "specialVariable", 0);
    add_int_child(item, "category", def->xml_category);
    add_text_child(item, "specialItem", "false");
    add_text_child(item, "hasBeenInInventory", "true");
    add_text_child(item, "Name", def->name);
    add_int_child(item, "Stack", stack);
    add_point_child(item, "tileLocation", 0, 0);
    add_int_child(item, "parentSheetIndex", def->id);
    add_int_child(item, "owner", 0);
    add_text_child(item, "name", def->name);
    add_text_child(item, "type", def->xml_type);
    add_text_child(item, "canBeSetDown", "true");
    add_text_child(item, "canBeGrabbed", "true");
    add_text_child(item, "isHoedirt", "false");
    add_text_child(item, "isSpawnedObject", "false");
    add_text_child(item, "questItem", "false");
    add_text_child(item, "isOn", "true");
    add_int_child(item, "fragility", 0);
    add_int_child(item, "price", def->price);
    add_int_child(item, "edibility", def->edibility);
    add_int_child(item, "stack", stack);
    add_int_child(item, "quality", 0);
    add_text_child(item, "bigCraftable", "false");
    add_text_child(item, "setOutdoors", "false");
    add_text_child(item, "setIndoors", "false");
    add_text_child(item, "readyForHarvest", "false");
    add_text_child(item, "showNextIndex", "false");
    add_text_child(item, "flipped", "false");
    add_text_child(item, "hasBeenPickedUpByFarmer", "true");
    add_text_child(item, "isRecipe", "false");
    add_text_child(item, "isLamp", "false");
    add_int_child(item, "minutesUntilReady", 0);

    xmlNodePtr bb = xmlNewChild(item, NULL, (const xmlChar *)"boundingBox", NULL);
    add_int_child(bb, "X", 0);
    add_int_child(bb, "Y", 0);
    add_int_child(bb, "Width", 64);
    add_int_child(bb, "Height", 64);
    add_point_child(bb, "Location", 0, 0);

    add_point_child(item, "scale", 0, 0);
    return item;
}

/*
 * Retrouve le nœud `<items>` d'un coffre précis dans `<locations>`.
 */
static xmlNodePtr find_chest_items_node(xmlNodePtr root, const StorageContainer *chest) {
    xmlNodePtr loc = find_location_by_name(root, chest->location_name);
    xmlNodePtr objects = find_child(loc, "objects");
    if (!objects) {
        return NULL;
    }

    for (xmlNodePtr cur = objects->children; cur != NULL; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) {
            continue;
        }
        xmlNodePtr vec = find_child(find_child(cur, "key"), "Vector2");
        xmlNodePtr obj = find_child(find_child(cur, "value"), "Object");
        if (!vec || !obj) {
            continue;
        }
        int cx = -1, cy = -1;
        read_int_node(vec, "X", &cx);
        read_int_node(vec, "Y", &cy);
        if (cx == chest->tile_x && cy == chest->tile_y) {
            xmlChar *xsi_type = xmlGetProp(obj, (const xmlChar *)"type");
            bool is_chest = (xsi_type != NULL && strcmp((const char *)xsi_type, "Chest") == 0);
            if (xsi_type) {
                xmlFree(xsi_type);
            }
            if (is_chest) {
                xmlNodePtr items = find_child(obj, "items");
                if (!items) {
                    items = xmlNewChild(obj, NULL, (const xmlChar *)"items", NULL);
                }
                return items;
            }
        }
    }
    return NULL;
}

/*
 * Applique en XML les ajouts, modifications de quantité et suppressions d'un stockage
 * (inventaire joueur ou coffre) sans altérer les objets existants non modifiés.
 */
static void save_storage_to_xml(xmlNodePtr root, xmlNodePtr items_node, const StorageContainer *st) {
    if (!items_node) {
        return;
    }

    /* Collecte les nœuds <Item> existants par index */
    xmlNodePtr existing[MAX_STORAGE_SLOTS];
    int existing_count = 0;
    for (xmlNodePtr cur = items_node->children; cur != NULL && existing_count < MAX_STORAGE_SLOTS; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE) {
            existing[existing_count++] = cur;
        }
    }

    for (int s = 0; s < st->max_slots; s++) {
        const StorageSlot *slot = &st->slots[s];

        if (s < existing_count) {
            xmlNodePtr old_node = existing[s];
            if (slot->is_cleared) {
                if (st->is_player_inv) {
                    /* Dans l'inventaire joueur, un slot vidé devient <Item xsi:nil="true" /> */
                    xmlNodePtr nil_node = create_nil_item_node(root);
                    xmlReplaceNode(old_node, nil_node);
                    xmlFreeNode(old_node);
                } else {
                    /* Dans un coffre, un slot vidé est simplement retiré de la liste */
                    xmlUnlinkNode(old_node);
                    xmlFreeNode(old_node);
                }
            } else if (slot->is_modified) {
                if (slot->db_index >= 0 && slot->db_index < ITEMS_DB_COUNT) {
                    xmlNodePtr new_node = create_object_item_node(root, slot->db_index, slot->stack);
                    xmlReplaceNode(old_node, new_node);
                    xmlFreeNode(old_node);
                } else {
                    /* Empilage sur un objet déjà existant : on met à jour Stack et stack */
                    write_int_node(old_node, "Stack", slot->stack);
                    write_int_node(old_node, "stack", slot->stack);
                }
            }
        } else {
            /* Au-delà du nombre de nœuds existants */
            if (slot->occupied && slot->is_modified &&
                slot->db_index >= 0 && slot->db_index < ITEMS_DB_COUNT) {
                xmlNodePtr new_node = create_object_item_node(root, slot->db_index, slot->stack);
                xmlAddChild(items_node, new_node);
            } else if (st->is_player_inv && s >= existing_count && !slot->occupied) {
                /* Ne rajoute pas de slots nil au-delà de la taille déjà présente dans le XML */
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
 * Synchronise le fichier `SaveGameInfo` situé dans le même dossier que `data->file_path`.
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

    data->storage_count = 1;
    data->storages[0].is_player_inv = true;
    snprintf(data->storages[0].label, sizeof(data->storages[0].label), "Inventaire Joueur");
    data->storages[0].max_slots = MAX_STORAGE_SLOTS;

    data->loaded_from_file = false;
    data->dirty = false;
}

/*
 * Ouvre et parse le fichier XML situé à `path`, puis extrait joueur, temps,
 * bâtiments, inventaire, coffres et couche visuelle de la ferme.
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
    load_all_storages(data, root, player);

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

bool save_data_can_place_building(const SaveData *data, int x, int y, int w, int h) {
    return save_data_can_place_building_ignore(data, x, y, w, h, -1);
}

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

bool save_data_delete_building(SaveData *data, int index) {
    if (index < 0 || index >= data->building_count || data->buildings[index].is_deleted) {
        return false;
    }
    if (data->buildings[index].is_new) {
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
 * Ajoute `qty` exemplaires de `ITEMS_DB[db_index]` dans la zone de stockage `storage_idx`.
 * Empile sur un slot existant du même ID s'il reste de la place, sinon utilise le premier slot vide.
 */
bool save_data_add_item(SaveData *data, int storage_idx, int db_index, int qty) {
    if (storage_idx < 0 || storage_idx >= data->storage_count) {
        return false;
    }
    if (db_index < 0 || db_index >= ITEMS_DB_COUNT || qty <= 0) {
        return false;
    }
    if (qty > 999) {
        qty = 999;
    }

    StorageContainer *st = &data->storages[storage_idx];
    const ItemDef *def = &ITEMS_DB[db_index];

    /* 1. Tente d'empiler sur un slot contenant déjà le même objet (< 999) */
    for (int i = 0; i < st->max_slots; i++) {
        StorageSlot *slot = &st->slots[i];
        if (slot->occupied && slot->parent_sheet_idx == def->id && slot->stack < 999) {
            int new_stack = slot->stack + qty;
            if (new_stack > 999) {
                new_stack = 999;
            }
            slot->stack = new_stack;
            slot->is_modified = true;
            slot->is_cleared = false;
            data->dirty = true;
            return true;
        }
    }

    /* 2. Sinon, place dans le premier slot libre */
    for (int i = 0; i < st->max_slots; i++) {
        StorageSlot *slot = &st->slots[i];
        if (!slot->occupied) {
            slot->occupied = true;
            snprintf(slot->name, sizeof(slot->name), "%s", def->name);
            slot->parent_sheet_idx = def->id;
            slot->stack = qty;
            slot->db_index = db_index;
            slot->is_modified = true;
            slot->is_cleared = false;
            data->dirty = true;
            return true;
        }
    }

    return false; /* Stockage plein */
}

/*
 * Vide le slot `slot_idx` du stockage `storage_idx`.
 */
bool save_data_remove_item(SaveData *data, int storage_idx, int slot_idx) {
    if (storage_idx < 0 || storage_idx >= data->storage_count) {
        return false;
    }
    StorageContainer *st = &data->storages[storage_idx];
    if (slot_idx < 0 || slot_idx >= st->max_slots) {
        return false;
    }
    StorageSlot *slot = &st->slots[slot_idx];
    if (!slot->occupied) {
        return false;
    }

    slot->occupied = false;
    slot->name[0] = '\0';
    slot->parent_sheet_idx = -1;
    slot->stack = 0;
    slot->db_index = -1;
    slot->is_modified = false;
    slot->is_cleared = slot->existed_in_xml;
    data->dirty = true;
    return true;
}

/*
 * Retourne le nombre de slots occupés dans un stockage.
 */
int save_data_storage_used_count(const StorageContainer *storage) {
    int used = 0;
    for (int i = 0; i < storage->max_slots; i++) {
        if (storage->slots[i].occupied) {
            used++;
        }
    }
    return used;
}

/*
 * Écrit toutes les modifications dans le fichier XML principal (+ backup .bak)
 * et synchronise `SaveGameInfo`.
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

    /* 1. Applique les modifications sur l'inventaire du joueur et les coffres */
    for (int i = 0; i < data->storage_count; i++) {
        const StorageContainer *st = &data->storages[i];
        bool any_change = false;
        for (int s = 0; s < st->max_slots; s++) {
            if (st->slots[s].is_modified || st->slots[s].is_cleared) {
                any_change = true;
                break;
            }
        }
        if (!any_change) {
            continue;
        }
        if (st->is_player_inv && player) {
            xmlNodePtr pitems = find_child(player, "items");
            if (!pitems) {
                pitems = xmlNewChild(player, NULL, (const xmlChar *)"items", NULL);
            }
            save_storage_to_xml(root, pitems, st);
        } else if (!st->is_player_inv) {
            xmlNodePtr citems = find_chest_items_node(root, st);
            save_storage_to_xml(root, citems, st);
        }
    }

    /* 2. Applique suppressions, déplacements et ajouts sur <locations>/Farm/<buildings> */
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
        /* Recharge proprement l'état depuis le fichier fraîchement écrit pour synchroniser les flags */
        char saved_path[512];
        snprintf(saved_path, sizeof(saved_path), "%s", data->file_path);
        sync_save_game_info(data);
        save_data_load_xml(data, saved_path);
        return true;
    }
    return false;
}

const char *save_data_season_name(int season_index) {
    switch (season_index % 4) {
        case 0: return "Printemps";
        case 1: return "Ete";
        case 2: return "Automne";
        case 3: return "Hiver";
        default: return "Printemps";
    }
}
