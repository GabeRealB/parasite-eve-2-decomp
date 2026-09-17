#ifndef ACTORS_SHARED_8014DF20_H
#define ACTORS_SHARED_8014DF20_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/1BC.h"

/// 0x2B0-byte work block the enemy's spawn function allocates with `Mem_Calloc`
/// and parks in the task's `Task::idMap` slot (that slot is not a `TaskIdMap`
/// here). It embeds three `GpObj` list nodes; the first points its `field_C` at
/// the `GpActorD4Rec` that follows it, the other two point straight at their
/// own `GpRec18` table, and `Gp_InitRec18Table` zeroes each table.
/// `ActorsShared8014df20` hands all three nodes back to `Gp_UnlinkObj`.
typedef struct ActorShared8014df20Work {
    /* 0x000 */ GpAnimCtx    context;
    /* 0x014 */ GpAnimSlot   slots[3];
    /* 0x08C */ byte         field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX       field_BC;       // colour matrix, TmdObject::field_20
    /* 0x0DC */ MATRIX       field_DC;       // light matrix, TmdObject::field_1C
    /* 0x0FC */ GpObj        field_FC;
    /* 0x11C */ GpActorD4Rec field_11C;
    /* 0x134 */ GpRec18      field_134[1];
    /* 0x14C */ GpObj        field_14C;
    /* 0x16C */ GpRec18      field_16C[1];
    /* 0x184 */ GpObj        field_184;
    /* 0x1A4 */ GpRec18      field_1A4[4];
    /* 0x204 */ byte         pad_204[0x88];
    /* 0x28C */ s16          field_28C;
    /* 0x28E */ s16          field_28E;
    /* 0x290 */ byte         pad_290[0x14];
    /* 0x2A4 */ s16          field_2A4;
    /* 0x2A6 */ s16          field_2A6;
    /* 0x2A8 */ s16          field_2A8;
    /* 0x2AA */ byte         pad_2AA[2];
    /* 0x2AC */ s16          field_2AC;
    /* 0x2AE */ byte         pad_2AE[2];
} ActorShared8014df20Work;
STATIC_ASSERT_SIZEOF(ActorShared8014df20Work, 0x2B0);

void ActorsShared8014df20(Task* task);

#endif
