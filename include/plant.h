#ifndef PVZ_PLANT_H
#define PVZ_PLANT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

#define MAX_SELECTED_PLANTS 8
#define PLANT_UNLOCK_TABLE_COUNT PLANT_TYPE_COUNT

typedef struct {
    const char* name;
    u16         sunCost;
    s16         health;
    u16         damage;
    f32         attackInterval;   /* seconds between shots */
    RechargeSpeed recharge;
    u8          unlockLevel;      /* 0-based level index when unlocked */
    const char* seedIconPath;     /* assets/seeds/<slug>.png */
} PlantDef;

const PlantDef* Plant_GetDef(PlantType type);

PlantType Plant_GetRewardForLevel(u8 levelIndex);

void Plant_GetUnlockedForLevel(u8 levelIndex, PlantType* outList, u8* outCount);

const char* Plant_Name(PlantType type);
u16         Plant_Cost(PlantType type);
s16         Plant_MaxHealth(PlantType type);

#ifdef __cplusplus
}
#endif

#endif
