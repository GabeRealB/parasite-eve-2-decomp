#ifndef SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"

extern AnimationSet D_dryfield_toilet_80180614;

extern AnimationSet D_dryfield_toilet_80180B64;

extern GpEvsCmd D_dryfield_toilet_80180C58[31];

extern GpEvsCmd D_dryfield_toilet_80180F40[20];

extern GpGridParams D_dryfield_toilet_80181404;

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_toilet_8017DA3C(s32);

void func_dryfield_toilet_8017DC50(void);

void func_dryfield_toilet_8017DC70(void);

void func_dryfield_toilet_8017DC90(void);

void func_dryfield_toilet_8017DCB0(void);

void func_dryfield_toilet_8017DCD0(s32);

#endif // SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H
