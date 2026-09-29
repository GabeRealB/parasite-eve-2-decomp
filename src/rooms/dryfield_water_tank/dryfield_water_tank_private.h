#ifndef DRYFIELD_WATER_TANK_PRIVATE_H
#define DRYFIELD_WATER_TANK_PRIVATE_H

#include "gameplay/direction.h"

#include "gameplay/message.h"

#include "main/task_types.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tank_8017DD20(Task *);
void func_dryfield_water_tank_8017DEA4(Task *);
void func_dryfield_water_tank_8017E194(s16);
void func_dryfield_water_tank_8017E1B4(void);
void func_dryfield_water_tank_8017E220(Task *);
void func_dryfield_water_tank_8017EC38(u32);
void func_dryfield_water_tank_8017EC6C(Task *);
void func_dryfield_water_tank_8017ED30(Task *);
void func_dryfield_water_tank_8017EDF4(Task *);

typedef struct {
    s32 id;
    union {
        void (*call0)(Task *, s32, GpCmdArg *);
        void (*call1)(Task *, s32, GpXformArg *);
        void (*call2)(Task *, s32, s32);
    } handler;
} DryfieldWaterTankMessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldWaterTankMessageEntry, 8);

extern DryfieldWaterTankMessageEntry D_dryfield_water_tank_8017FD90[3];

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tank_8017D618(Task *);
s32 func_dryfield_water_tank_8017D7BC(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_water_tank_8017D7C4(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_dryfield_water_tank_8017D7EC(Task *, s32, GpMsg13EF *, GpMessageArg);
s32 func_dryfield_water_tank_8017D910(Task *, s32, s32, GpMessageArg);
void func_dryfield_water_tank_8017D948(Task *);
void func_dryfield_water_tank_8017E0B4(Task *, s32, s32);
void func_dryfield_water_tank_8017E0E8(Task *, s32, GpXformArg *);
void func_dryfield_water_tank_8017E174(Task *, s32, GpCmdArg *);

#endif // DRYFIELD_WATER_TANK_PRIVATE_H
