#ifndef SRC_ROOMS_DRYFIELD_WATER_TOWER_DRYFIELD_WATER_TOWER_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WATER_TOWER_DRYFIELD_WATER_TOWER_PRIVATE_H

#include "common.h"

#include "gameplay/message.h"

#include "main/task_types.h"

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    u32 value;
    u8  retained[4];
} DryfieldWaterTowerStorage768C;
STATIC_ASSERT_SIZEOF(DryfieldWaterTowerStorage768C, 8);

typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call2)(Task*, s32, s32, s32);
        s32 (*call3)(s32, s32, s32);
    } handler;
} DryfieldWaterTowerMessageEntry;

/// The room's task table at 0x80182384: entry 0 is the cap script
/// `func_dryfield_water_tower_8017F128`, which the room's entry task spawns,
/// entry 1 the prop `func_dryfield_water_tower_8017E764` and entry 2 the prop
/// `func_dryfield_water_tower_8017E1DC`, the two the cap script spawns.
extern TaskDesc D_dryfield_water_tower_80182384[];

extern Task* D_dryfield_water_tower_801876A4;

extern Task* D_dryfield_water_tower_801876AC;

extern TaskDesc D_dryfield_water_tower_80180394;

extern TaskDesc D_dryfield_water_tower_801803D8[2];

extern DryfieldWaterTowerStorage768C D_dryfield_water_tower_8018768C;

extern DryfieldWaterTowerMessageEntry D_dryfield_water_tower_801803A0[7];

/// Sets the current view's skip-OT-link byte: a zero low byte skips the view's
/// sprites, non-zero draws them.
void func_dryfield_water_tower_801802D8(u8 arg0);

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tower_8017D948(Task*);

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tower_8017D7D8(Task*);

s32 func_dryfield_water_tower_8017DAF8(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_water_tower_8017DC64(s32, s32, s32);

s32 func_dryfield_water_tower_8017DCFC(void);

s32 func_dryfield_water_tower_8017DD04(Task*, s32, s32, s32);

s32 func_dryfield_water_tower_8017DD3C(void);

s32 func_dryfield_water_tower_8017DD44(Task*, s32, s32, s32);

#endif // SRC_ROOMS_DRYFIELD_WATER_TOWER_DRYFIELD_WATER_TOWER_PRIVATE_H
