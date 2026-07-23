#include "plant.h"
#include "constants.h"
#include <string.h>

static const char* s_plantNames[PLANT_TYPE_COUNT] = {
    "Peashooter",    "Sunflower",     "Cherry Bomb",   "Wall-nut",
    "Potato Mine",   "Snow Pea",      "Chomper",       "Repeater",
    "Puff-shroom",   "Sun-shroom",    "Fume-shroom",   "Grave Buster",
    "Hypno-shroom",  "Scaredy-shroom","Ice-shroom",    "Doom-shroom",
    "Lily Pad",      "Squash",        "Threepeater",   "Tangle Kelp",
    "Jalapeno",      "Spikeweed",     "Torchwood",     "Tall-nut",
    "Sea-shroom",    "Plantern",      "Cactus",        "Blover",
    "Split Pea",     "Starfruit",     "Pumpkin",       "Magnet-shroom",
    "Cabbage-pult",  "Flower Pot",    "Kernel-pult",   "Coffee Bean",
    "Garlic",        "Umbrella Leaf", "Marigold",      "Melon-pult",
    "Gatling Pea",   "Twin Sunflower","Gloom-shroom",  "Cattail",
    "Winter Melon",  "Gold Magnet",   "Spikerock",     "Cob Cannon",
    "Imitater",
};

static const PlantDef s_plantDefs[PLANT_TYPE_COUNT] = {
    /* name             cost  hp   dmg  atkIntvl recharge   unlock slug used for path */
    {"Peashooter",      100,  300, 20,  1.425f, RECHARGE_FAST,       0},
    {"Sunflower",        50,  300, 0,   24.25f, RECHARGE_FAST,       1},
    {"Cherry Bomb",     150,  300, 1800,1.2f,  RECHARGE_VERY_SLOW,   2},
    {"Wall-nut",         50,  4000,0,   0,     RECHARGE_SLOW,        3},
    {"Potato Mine",      25,  300, 1800,15.0f, RECHARGE_SLOW,        4},
    {"Snow Pea",        175,  300, 20,  1.425f,RECHARGE_FAST,        5},
    {"Chomper",         150,  300, 40,  42.0f, RECHARGE_FAST,        6},
    {"Repeater",        200,  300, 20,  1.425f,RECHARGE_FAST,        7},
    {"Puff-shroom",       0,  300, 20,  1.425f,RECHARGE_FAST,        8},
    {"Sun-shroom",       25,  300, 0,   24.25f,RECHARGE_FAST,        9},
    {"Fume-shroom",      75,  300, 20,  1.425f,RECHARGE_FAST,       10},
    {"Grave Buster",     75,  300, 0,   4.5f,  RECHARGE_FAST,       11},
    {"Hypno-shroom",     75,  300, 0,   0,     RECHARGE_SLOW,       12},
    {"Scaredy-shroom",   25,  300, 20,  1.425f,RECHARGE_FAST,       13},
    {"Ice-shroom",       75,  300, 20,  1.0f,  RECHARGE_VERY_SLOW,  14},
    {"Doom-shroom",     125,  300, 1800,1.0f,  RECHARGE_VERY_SLOW,  15},
    {"Lily Pad",         25,  300, 0,   0,     RECHARGE_FAST,       16},
    {"Squash",           50,  300, 1800,0,     RECHARGE_SLOW,       17},
    {"Threepeater",     325,  300, 20,  1.425f,RECHARGE_FAST,       18},
    {"Tangle Kelp",      25,  300, 0,   0,     RECHARGE_SLOW,       19},
    {"Jalapeno",        125,  300, 1800,1.0f,  RECHARGE_VERY_SLOW,  20},
    {"Spikeweed",       100,  300, 20,  1.0f,  RECHARGE_FAST,       21},
    {"Torchwood",       175,  300, 0,   0,     RECHARGE_FAST,       22},
    {"Tall-nut",        125,  8000,0,   0,     RECHARGE_SLOW,       23},
    {"Sea-shroom",        0,  300, 20,  1.425f,RECHARGE_SLOW,       24},
    {"Plantern",         25,  300, 0,   0,     RECHARGE_SLOW,       25},
    {"Cactus",          125,  300, 20,  1.425f,RECHARGE_FAST,       26},
    {"Blover",          100,  300, 0,   0,     RECHARGE_FAST,       27},
    {"Split Pea",       125,  300, 20,  1.425f,RECHARGE_FAST,       28},
    {"Starfruit",       125,  300, 20,  1.425f,RECHARGE_FAST,       29},
    {"Pumpkin",         125,  4000,0,   0,     RECHARGE_SLOW,       30},
    {"Magnet-shroom",   100,  300, 0,   15.0f, RECHARGE_FAST,       31},
    {"Cabbage-pult",    100,  300, 40,  2.925f,RECHARGE_FAST,       32},
    {"Flower Pot",       25,  300, 0,   0,     RECHARGE_FAST,       33},
    {"Kernel-pult",     100,  300, 20,  2.925f,RECHARGE_FAST,       34},
    {"Coffee Bean",      75,  300, 0,   0,     RECHARGE_FAST,       35},
    {"Garlic",           50,  400, 0,   0,     RECHARGE_FAST,       36},
    {"Umbrella Leaf",   100,  300, 0,   0,     RECHARGE_FAST,       37},
    {"Marigold",         50,  300, 0,   24.25f,RECHARGE_SLOW,       38},
    {"Melon-pult",      300,  300, 80,  2.925f,RECHARGE_FAST,       39},
    {"Gatling Pea",     450,  300, 20,  1.425f,RECHARGE_VERY_SLOW,  40},
    {"Twin Sunflower",  200,  300, 0,   24.25f,RECHARGE_VERY_SLOW,  41},
    {"Gloom-shroom",    225,  300, 20,  1.925f,RECHARGE_VERY_SLOW,  42},
    {"Cattail",         250,  300, 20,  1.425f,RECHARGE_VERY_SLOW,  43},
    {"Winter Melon",    500,  300, 80,  2.925f,RECHARGE_VERY_SLOW,  44},
    {"Gold Magnet",     150,  300, 0,   10.0f, RECHARGE_VERY_SLOW,  45},
    {"Spikerock",       225,  450, 20,  1.0f,  RECHARGE_VERY_SLOW,  46},
    {"Cob Cannon",      700,  300, 1800,34.75f,RECHARGE_VERY_SLOW,  47},
    {"Imitater",          0,  300, 0,   0,     RECHARGE_VERY_SLOW,  48},
};

/* Plant reward per level: returns the plant unlocked by completing this level,
 * or PLANT_NONE if no reward (bonus / conveyor levels). */
PlantType Plant_GetRewardForLevel(u8 levelIndex) {
    u8 levelInWorld = levelIndex % LEVELS_PER_WORLD;
    if (levelInWorld == 4 || levelInWorld == 9)
        return PLANT_NONE;
    for (int i = 0; i < PLANT_TYPE_COUNT; i++) {
        if (s_plantDefs[i].unlockLevel == levelIndex)
            return (PlantType)i;
    }
    return PLANT_NONE;
}

const PlantDef* Plant_GetDef(PlantType type) {
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return NULL;
    return &s_plantDefs[type];
}

void Plant_GetUnlockedForLevel(u8 levelIndex, PlantType* outList, u8* outCount) {
    u8 count = 0;
    for (int i = 0; i < PLANT_TYPE_COUNT && count < PLANT_TYPE_COUNT; i++) {
        if (s_plantDefs[i].unlockLevel <= levelIndex) {
            outList[count++] = (PlantType)i;
        }
    }
    *outCount = count;
}

const char* Plant_Name(PlantType type) {
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return "?";
    return s_plantNames[type];
}

u16 Plant_Cost(PlantType type) {
    const PlantDef* d = Plant_GetDef(type);
    return d ? d->sunCost : 0;
}

s16 Plant_MaxHealth(PlantType type) {
    const PlantDef* d = Plant_GetDef(type);
    return d ? d->health : 100;
}
