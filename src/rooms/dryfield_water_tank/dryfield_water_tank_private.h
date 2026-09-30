#ifndef SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, GpCmdArg*);
        void (*call1)(Task*, s32, GpXformArg*);
        void (*call2)(Task*, s32, s32);
    } handler;
} DryfieldWaterTankMessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldWaterTankMessageEntry, 8);

extern TaskDesc D_dryfield_water_tank_80184DF4[2];

extern s32 D_dryfield_water_tank_801868BC;

extern s32 D_dryfield_water_tank_801868C0;

extern s32 D_dryfield_water_tank_801868C4;

extern s32 D_dryfield_water_tank_801868C8;

extern u16 D_dryfield_water_tank_801868CC[10];

extern Task* D_dryfield_water_tank_80188D50;

extern GpEvsCmd D_dryfield_water_tank_8017F114[11];

extern GpEvsCmd D_dryfield_water_tank_8017F21C[11];

extern GpMsgEntry D_dryfield_water_tank_8017F324[5];

extern TaskDesc D_dryfield_water_tank_8017F34C[2];

extern GpXformArg D_dryfield_water_tank_8017FD60[2];

extern u16 D_dryfield_water_tank_8017FDA8[12];

extern GpEvsCmd D_dryfield_water_tank_8017FDC0[11];

extern GpEvsCmd D_dryfield_water_tank_8017FEC8[8];

extern TaskDesc D_dryfield_water_tank_8017FF88[2];

extern TaskDesc D_dryfield_water_tank_80180794;

extern AnimationSet D_dryfield_water_tank_80180A7C;

extern AnimationSet D_dryfield_water_tank_80180D3C;

extern AnimationSet D_dryfield_water_tank_80181020;

extern AnimationSet D_dryfield_water_tank_80181274;

extern AnimationSet D_dryfield_water_tank_801815C0;

extern AnimationSet D_dryfield_water_tank_80181D7C;

extern AnimationSet D_dryfield_water_tank_80182060;

extern AnimationSet D_dryfield_water_tank_80182258;

extern AnimationSet D_dryfield_water_tank_801825AC;

extern AnimationSet D_dryfield_water_tank_801827A4;

extern AnimationSet D_dryfield_water_tank_80182CC4;

extern AnimationSet D_dryfield_water_tank_80182FA4;

extern AnimationSet D_dryfield_water_tank_80183378;

extern AnimationSet D_dryfield_water_tank_80183768;

extern AnimationSet D_dryfield_water_tank_80183AD4;

extern AnimationSet D_dryfield_water_tank_80183E00;

extern AnimationSet D_dryfield_water_tank_80184228;

extern AnimationSet D_dryfield_water_tank_80184508;

extern DryfieldWaterTankMessageEntry D_dryfield_water_tank_8017FD90[3];

/// Toggle the room's cutscene-“watched” state over two of the area's sprite
/// commands: `arg0 != 0` hides the first and shows the second by setting and
/// clearing their `SpriteBatch::hidden`, `arg0 == 0` does the opposite. No-op unless `GameSession.at4.loc.stage` is 2, i.e. only for the stage
/// whose sprite table has a record for the current room.
/// `func_dryfield_water_tank_8017DB48` passes the game-flag `0x55` nibble
/// through it, one way per value.
void func_dryfield_water_tank_8017EFF4(s32 arg0);

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tank_8017DD20(Task*);

void func_dryfield_water_tank_8017DEA4(Task*);

void func_dryfield_water_tank_8017E194(s16);

void func_dryfield_water_tank_8017E1B4(void);

void func_dryfield_water_tank_8017E220(Task*);

void func_dryfield_water_tank_8017EC38(u32);

void func_dryfield_water_tank_8017EC6C(Task*);

void func_dryfield_water_tank_8017ED30(Task*);

void func_dryfield_water_tank_8017EDF4(Task*);

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_water_tank_8017D618(Task*);

s32 func_dryfield_water_tank_8017D7BC(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_dryfield_water_tank_8017D7C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_water_tank_8017D7EC(Task*, s32, GpMsg13EF*, GpMessageArg);

s32 func_dryfield_water_tank_8017D910(Task*, s32, s32, GpMessageArg);

void func_dryfield_water_tank_8017D948(Task*);

void func_dryfield_water_tank_8017E0B4(Task*, s32, s32);

void func_dryfield_water_tank_8017E0E8(Task*, s32, GpXformArg*);

void func_dryfield_water_tank_8017E174(Task*, s32, GpCmdArg*);

#endif // SRC_ROOMS_DRYFIELD_WATER_TANK_DRYFIELD_WATER_TANK_PRIVATE_H
