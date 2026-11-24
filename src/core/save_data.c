#include "core/save_data.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SEASONS[4] = { "spring", "summer", "fall", "winter" };

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

static void write_int_node(xmlNodePtr parent, const char *name, int value) {
    xmlNodePtr node = find_child(parent, name);
    if (!node) {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    xmlNodeSetContent(node, (const xmlChar *)buf);
}

static void write_string_node(xmlNodePtr parent, const char *name, const char *value) {
    xmlNodePtr node = find_child(parent, name);
    if (!node) {
        return;
    }
    xmlNodeSetContent(node, (const xmlChar *)value);
}

static int season_from_string(const char *s) {
    for (int i = 0; i < 4; i++) {
        if (strcmp(s, SEASONS[i]) == 0) {
            return i;
        }
    }
    return 0;
}

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
    data->loaded_from_file = false;
    data->dirty = false;
}

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

    data->loaded_from_file = true;
    data->dirty = false;

    xmlFreeDoc(doc);
    return true;
}

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

    int res = xmlSaveFormatFileEnc(data->file_path, doc, "UTF-8", 1);
    xmlFreeDoc(doc);

    if (res >= 0) {
        data->dirty = false;
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
