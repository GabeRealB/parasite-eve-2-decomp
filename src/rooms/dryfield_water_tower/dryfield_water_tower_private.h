#ifndef DRYFIELD_WATER_TOWER_PRIVATE_H
#define DRYFIELD_WATER_TOWER_PRIVATE_H

#include "gameplay/message.h"

#include "main/task_types.h"

#include "common.h"

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    u32 value;
    u8 retained[4];
} DryfieldWaterTowerStorage768C;
STATIC_ASSERT_SIZEOF(DryfieldWaterTowerStorage768C, 8);

extern DryfieldWaterTowerStorage768C D_dryfield_water_tower_8018768C;

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tower_8017D948(Task *);

typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task *, s32, RoomEventMsg *, RoomEventMsg *);
        s32 (*call2)(Task *, s32, s32, s32);
        s32 (*call3)(s32, s32, s32);
    } handler;
} DryfieldWaterTowerMessageEntry;

extern DryfieldWaterTowerMessageEntry D_dryfield_water_tower_801803A0[7];

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tower_8017D7D8(Task *);
s32 func_dryfield_water_tower_8017DAF8(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_water_tower_8017DC64(s32, s32, s32);
s32 func_dryfield_water_tower_8017DCFC(void);
s32 func_dryfield_water_tower_8017DD04(Task *, s32, s32, s32);
s32 func_dryfield_water_tower_8017DD3C(void);
s32 func_dryfield_water_tower_8017DD44(Task *, s32, s32, s32);

#endif // DRYFIELD_WATER_TOWER_PRIVATE_H
