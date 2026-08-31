#ifndef ACTOR_102100_H
#define ACTOR_102100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `field_8` is the actor's `GsCOORDINATE2` array: entry 0 is the actor's own
/// coordinate, entry 3 the alternate sight origin used by `Actor02100_Fn00DCC`.
typedef struct Actor02100Obj2C {
    /* 0x0 */ byte           pad_0[8];
    /* 0x8 */ GsCOORDINATE2* field_8;
    /* 0xC */ s16            field_C;
} Actor02100Obj2C;

/// Overlay-local view of the spawn parameter block (`Task::spawnArg2`).
/// `field_3C` points at the per-enemy parameter record whose byte 0xF holds
/// the sight-range index (low 3 bits) into `Actor02100_D03E00`.
typedef struct Actor02100Params {
    /* 0x0 */ byte pad_0[0xF];
    /* 0xF */ u8   field_F;
} Actor02100Params;

typedef struct Actor02100Spawn {
    /* 0x00 */ byte              pad_0[0x3C];
    /* 0x3C */ Actor02100Params* field_3C;
} Actor02100Spawn;

/// 0x20-byte scratch block taken from `G_SCRATCH_HEAD` by
/// `Actor02100_Fn00DCC`: the world-space delta between the two coordinates,
/// then the two endpoints as `SVECTOR`s for the line-of-sight test.
typedef struct Actor02100Sight {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR from;
    /* 0x18 */ SVECTOR to;
} Actor02100Sight;
STATIC_ASSERT_SIZEOF(Actor02100Sight, 0x20);

typedef struct Actor02100Work {
    /* 0x000 */ byte  pad_0[0x40];
    /* 0x040 */ byte  field_40[0x38];
    /* 0x078 */ byte  field_78[0x50];
    /* 0x0C8 */ byte  field_C8[0x50];
    /* 0x118 */ s16   field_118;
    /* 0x11A */ s16   field_11A;
    /* 0x11C */ s16   field_11C;
    /* 0x11E */ byte  pad_11E[2];
    /* 0x120 */ s16   field_120;
    /* 0x122 */ s16   field_122;
    /* 0x124 */ s16   field_124;
    /* 0x126 */ byte  pad_126[0x1A];
    /* 0x140 */ void* field_140;
    /* 0x144 */ byte  pad_144[0x20];
    /* 0x164 */ s32   field_164;
    /* 0x168 */ s32   field_168;
    /* 0x16C */ byte  pad_16C[6];
    /* 0x172 */ s16   field_172;
    /* 0x174 */ s16   field_174;
    /* 0x176 */ byte  pad_176[2];
    /* 0x178 */ s16   field_178;
    /* 0x17A */ s16   field_17A;
    /* 0x17C */ s16   field_17C;
    /* 0x17E */ s16   field_17E;
    /* 0x180 */ s16   field_180;
    /* 0x182 */ byte  pad_182[4];
    /* 0x186 */ s16   field_186;
    /* 0x188 */ s16   field_188;
    /* 0x18A */ byte  pad_18A[0x12];
} Actor02100Work;
STATIC_ASSERT_SIZEOF(Actor02100Work, 0x19C);

typedef struct Actor02100 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor02100Work*  field_1C;
    /* 0x20 */ Actor02100Spawn* field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor02100Obj2C* field_2C;
    /* 0x30 */ s32              field_30;
} Actor02100;
STATIC_ASSERT_SIZEOF(Actor02100, 0x34);

/// `Gp_UnlinkNode` list entry at +0x10 of `Actor02100Ctx`. `field_4` is the
/// flag byte the per-state handlers write (`Actor02100_Fn031C4`).
typedef struct Actor02100Node {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ u8   field_4;
    /* 0x5 */ byte pad_5[3];
} Actor02100Node;
STATIC_ASSERT_SIZEOF(Actor02100Node, 0x8);

typedef struct Actor02100Ctx {
    /* 0x00 */ byte           pad_0[0x10];
    /* 0x10 */ Actor02100Node node;
    /* 0x18 */ byte           pad_18[0x3C];
    /* 0x54 */ s32            field_54;
} Actor02100Ctx;
STATIC_ASSERT_SIZEOF(Actor02100Ctx, 0x58);

extern s16 Actor02100_D03E00[];

void Actor02100_Fn031C4(Actor02100Ctx* arg0, Actor02100* arg1);
void Actor02100_Fn032E4(Actor02100* arg0);
void Actor02100_Fn035D4(Actor02100Ctx* arg0, Actor02100* arg1);

#endif
