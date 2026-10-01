#ifndef SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"

#include "main/task_types.h"

extern AnimationSet D_dryfield_night_garage_80182140;

extern AnimationSet D_dryfield_night_garage_80182514;

extern AnimationSet D_dryfield_night_garage_80182904;

extern AnimationSet D_dryfield_night_garage_80182C70;

extern TaskDesc D_dryfield_night_garage_80182C98[2];

extern s32 D_dryfield_night_garage_80182DE0;

extern s32 D_dryfield_night_garage_80182DE4;

extern EvsCommand D_dryfield_night_garage_80182DF8[40];

extern EvsCommand D_dryfield_night_garage_801831B8[19];

extern GpGridParams D_dryfield_night_garage_80183DD4;

extern GpObj4C D_dryfield_night_garage_80186D7C[16];

extern s32 Shop_Data_80187628;

extern GpItemMap* Shop_Data_8018762C;

/// Returns the task of the room work object whose id is the current area and
/// stage with `arg0` in bits 12 and up, or NULL when there is none.
Task* func_dryfield_night_garage_80180A64(s32 arg0);

void func_dryfield_night_garage_801807E4(Task* arg0);

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_night_garage_80180924(void);

void func_dryfield_night_garage_80180944(void);

void func_dryfield_night_garage_80180964(void);

void func_dryfield_night_garage_80180984(void);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H
