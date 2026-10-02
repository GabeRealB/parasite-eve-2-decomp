#ifndef SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"

extern AnimationSet gDryfieldToiletAnimation03054;

extern AnimationSet gDryfieldToiletAnimation035A4;

extern EvsCommand D_dryfield_toilet_80180C58[31];

extern EvsCommand D_dryfield_toilet_80180F40[20];

extern WorldCollisionGrid D_dryfield_toilet_80181404;

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_toilet_8017DA3C(s32);

void func_dryfield_toilet_8017DC50(void);

void func_dryfield_toilet_8017DC70(void);

void func_dryfield_toilet_8017DC90(void);

void func_dryfield_toilet_8017DCB0(void);

void func_dryfield_toilet_8017DCD0(s32);

#endif // SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H
