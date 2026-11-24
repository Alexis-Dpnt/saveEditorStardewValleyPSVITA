#ifndef SAVE_DATA_H
#define SAVE_DATA_H

#include <stdbool.h>

typedef struct {
    char file_path[512];
    char farmer_name[64];
    char farm_name[64];

    int money;
    int max_stamina;
    int max_health;

    int farming_level;
    int mining_level;
    int foraging_level;
    int fishing_level;
    int combat_level;

    int day_of_month;
    int season_index; /* 0: spring, 1: summer, 2: fall, 3: winter */
    int year;

    bool loaded_from_file;
    bool dirty;
} SaveData;

void save_data_init_demo(SaveData *data);
bool save_data_load_xml(SaveData *data, const char *path);
bool save_data_save_xml(SaveData *data);
const char *save_data_season_name(int season_index);

#endif
