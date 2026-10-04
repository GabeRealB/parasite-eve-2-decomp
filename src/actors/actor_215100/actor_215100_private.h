#ifndef SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H
#define SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/evs.h"

#include "main/task_types.h"

extern TaskDesc D_actor_215100_8014CF6C[2];

extern s32 D_actor_215100_8014D038;

extern s32 D_actor_215100_8014D03C;

extern s32 D_actor_215100_8014D044;

extern AnimationSet gActor215100Animation034E4;

extern AnimationSet gActor215100Animation03754;

extern AnimationSet gActor215100Animation039AC;

extern AnimationSet gActor215100Animation03B48;

extern AnimationSet gActor215100Animation03D98;

extern AnimationSet gActor215100Animation03FF0;

extern AnimationSet gActor215100Animation042F4;

extern EvsCommand D_actor_215100_8014EB98[3];

extern EvsCommand D_actor_215100_8014EBE0[18];

extern EvsCommand D_actor_215100_8014ED90[9];

extern EvsCommand D_actor_215100_8014EE68[13];

extern EvsCommand D_actor_215100_8014EFA0[8];

extern EvsCommand D_actor_215100_8014F060[9];

extern EvsCommand D_actor_215100_8014F138[6];

extern s32 D_actor_215100_8015E670;

void func_actor_215100_80149F2C(Task* task);

// Callbacks referenced by the overlay's shared data tables.
void func_actor_215100_8014A5C0(Task*);

void func_actor_215100_8014A7C4(Task*);

#endif // SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H
