#ifndef SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H
#define SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/evs.h"

#include "main/task_types.h"

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    s32 value;
    u8  retained[4];
} Actor215100StorageE670;
STATIC_ASSERT_SIZEOF(Actor215100StorageE670, 8);

extern TaskDesc D_actor_215100_8014CF6C[2];

extern s32 D_actor_215100_8014D038;

extern s32 D_actor_215100_8014D03C;

extern s32 D_actor_215100_8014D044;

extern GpAnimSet D_actor_215100_8014D304;

extern GpAnimSet D_actor_215100_8014D574;

extern GpAnimSet D_actor_215100_8014D7CC;

extern GpAnimSet D_actor_215100_8014D968;

extern GpAnimSet D_actor_215100_8014DBB8;

extern GpAnimSet D_actor_215100_8014DE10;

extern GpAnimSet D_actor_215100_8014E114;

extern GpEvsCmd D_actor_215100_8014EB98[3];

extern GpEvsCmd D_actor_215100_8014EBE0[18];

extern GpEvsCmd D_actor_215100_8014ED90[9];

extern GpEvsCmd D_actor_215100_8014EE68[13];

extern GpEvsCmd D_actor_215100_8014EFA0[8];

extern GpEvsCmd D_actor_215100_8014F060[9];

extern GpEvsCmd D_actor_215100_8014F138[6];

extern Actor215100StorageE670 D_actor_215100_8015E670;

void func_actor_215100_80149F2C(Task* task);

// Callbacks referenced by the overlay's shared data tables.
void func_actor_215100_8014A5C0(Task*);

void func_actor_215100_8014A7C4(Task*);

#endif // SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H
