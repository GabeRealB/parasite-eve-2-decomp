#ifndef ACTOR_207200_H
#define ACTOR_207200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// 0x38-byte `GpObj` list node; five of them live in `Actor207200Work` and are
/// unlinked one by one when the actor tears down.
typedef struct Actor207200Obj {
    /* 0x00 */ byte pad_0[0x38];
} Actor207200Obj;

typedef struct Actor207200Work {
    /* 0x000 */ byte           pad_0[0x1DC];
    /* 0x1DC */ Actor207200Obj field_1DC;
    /* 0x214 */ Actor207200Obj field_214;
    /* 0x24C */ byte           pad_24C[0x78];
    /* 0x2C4 */ Actor207200Obj field_2C4;
    /* 0x2FC */ byte           pad_2FC[0x78];
    /* 0x374 */ Actor207200Obj field_374;
    /* 0x3AC */ Actor207200Obj field_3AC;
} Actor207200Work;

/// Owning context. The leading part holds the `Gp_UnlinkNode` list entry at
/// +0x10, as for the gameplay `GpEnemy`.
typedef struct Actor207200Ctx {
    /* 0x00 */ byte pad_0[0x10];
    /* 0x10 */ byte node[0x44];
    /* 0x54 */ s32  field_54;
} Actor207200Ctx;

typedef struct Actor207200 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor207200Work* field_1C;
    /* 0x20 */ Actor207200Ctx*  field_20;
} Actor207200;

void func_actor_207200_8014DB4C(Actor207200* arg0);

#endif
