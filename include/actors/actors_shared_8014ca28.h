#ifndef ACTORS_SHARED_8014CA28_H
#define ACTORS_SHARED_8014CA28_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// 0x2E4-byte work block the enemy's spawn function allocates with `Mem_Calloc`
/// and parks in the task's `Task::idMap` slot (that slot is not a `TaskIdMap`
/// here). It embeds four `GpObj` list nodes, each followed by the `GpRec18`
/// table `Gp_InitRec18Table` zeroes and the node's `field_C` points at;
/// `ActorsShared8014ca28` hands all four back to `Gp_UnlinkObj`.
typedef struct ActorShared8014ca28Work {
    /* 0x000 */ byte    pad_0[0xFC];
    /* 0x0FC */ GpObj   field_FC;
    /* 0x11C */ GpRec18 field_11C[1];
    /* 0x134 */ GpObj   field_134;
    /* 0x154 */ GpRec18 field_154[4];
    /* 0x1B4 */ GpObj   field_1B4;
    /* 0x1D4 */ GpRec18 field_1D4[1];
    /* 0x1EC */ GpObj   field_1EC;
    /* 0x20C */ GpRec18 field_20C[1];
    /* 0x224 */ byte    pad_224[0xC0];
} ActorShared8014ca28Work;
STATIC_ASSERT_SIZEOF(ActorShared8014ca28Work, 0x2E4);

void ActorsShared8014ca28(Task* task);

#endif
