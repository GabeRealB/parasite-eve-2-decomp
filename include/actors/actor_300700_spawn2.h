#ifndef ACTOR_300700_SPAWN2_H
#define ACTOR_300700_SPAWN2_H

#include "gameplay/3A34.h"
#include "main/session.h"

/// The 0x39C-byte allocation `func_actor_300700_80163510` makes with
/// `Mem_Calloc` and stores in `Task::work`, then fills with the four `GpObj`
/// render nodes (`Gp_LinkObj`, shapes 3/2/2/3) and their `GpRec18` tables.
///
/// This is the work block the overlay's second enemy variant runs on - the
/// state table `D_actor_300700_80161E30` (`func_actor_300700_80164D3C` and
/// friends), which reaches it as `Actor300700Work` and only ever touches the
/// fields from 0x37A up. Those trailing fields, plus the animation context
/// `func_800B3F84` fills in, are laid out identically in both views; the
/// 0x1DC..0x333 run - the four list nodes and their record tables - is owned
/// only here, so `Actor300700Work` carries it as padding.
typedef struct Actor300700Spawn2Work {
    /* 0x000 */ byte           pad_0[0xC];
    /* 0x00C */ s16            field_C;
    /* 0x00E */ byte           pad_E[6];
    /* 0x014 */ byte           field_14[0x118]; // anim slots, func_800B3F84 arg4
    /* 0x12C */ byte           field_12C[0x50]; // pose buffer, func_800B3F84 arg3
    /* 0x17C */ MATRIX         field_17C;
    /* 0x19C */ MATRIX         field_19C;       // TmdObject color matrix
    /* 0x1BC */ MATRIX         field_1BC;       // TmdObject light matrix
    /* 0x1DC */ GpObj          obj1;
    /* 0x1FC */ GpRec18        rec1[1];
    /* 0x214 */ GpObj          obj2;
    /* 0x234 */ GpRec18        rec2[3];
    /* 0x27C */ GpObj          obj3;
    /* 0x29C */ GpRec18        rec3[4];
    /* 0x2FC */ GpObj          obj4;
    /* 0x31C */ GpRec18        rec4[1];
    /* 0x334 */ GsCOORDINATE2* field_334;
    /* 0x338 */ s16            field_338;
    /* 0x33A */ s16            field_33A;
    /* 0x33C */ byte           pad_33C[0x42];
    /* 0x37E */ s16            field_37E; // current state id, `Actor300700Work`
    /* 0x380 */ s16            field_380; // last applied state id
} Actor300700Spawn2Work;

#endif
