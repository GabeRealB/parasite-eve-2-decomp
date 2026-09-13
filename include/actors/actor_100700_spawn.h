#ifndef ACTOR_100700_SPAWN_H
#define ACTOR_100700_SPAWN_H

#include "actors/actor_100700.h"

/// The 0x2F4-byte allocation used by Actor00700_Fn01FE0.
typedef struct Actor00700SpawnWork {
    /* 0x000 */ u8      field_0[0x14];
    /* 0x014 */ u8      field_14[0xA0];
    /* 0x0B4 */ u8      field_B4[0x40];
    /* 0x0F4 */ MATRIX  field_F4;
    /* 0x114 */ MATRIX  field_114;
    /* 0x134 */ u8      field_134[8];
    /* 0x13C */ void*   field_13C;
    /* 0x140 */ void*   field_140;
    /* 0x144 */ u16     field_144;
    /* 0x146 */ u16     field_146;
    /* 0x148 */ u16     field_148;
    /* 0x14A */ u8      pad_14A[0x2];
    /* 0x14C */ s32     field_14C;
    /* 0x150 */ u16     field_150;
    /* 0x152 */ u16     field_152;
    /* 0x154 */ GpRec18 field_154;
    /* 0x16C */ u8      field_16C[8];
    /* 0x174 */ void*   field_174;
    /* 0x178 */ void*   field_178;
    /* 0x17C */ u16     field_17C;
    /* 0x17E */ u16     field_17E;
    /* 0x180 */ u16     field_180;
    /* 0x182 */ u8      pad_182[0x2];
    /* 0x184 */ s32     field_184;
    /* 0x188 */ u16     field_188;
    /* 0x18A */ u16     field_18A;
    /* 0x18C */ GpRec18 field_18C[4];
    /* 0x1EC */ u8      field_1EC[8];
    /* 0x1F4 */ void*   field_1F4;
    /* 0x1F8 */ void*   field_1F8;
    /* 0x1FC */ u16     field_1FC;
    /* 0x1FE */ u16     field_1FE;
    /* 0x200 */ u16     field_200;
    /* 0x202 */ u8      pad_202[0x2];
    /* 0x204 */ s32     field_204;
    /* 0x208 */ u16     field_208;
    /* 0x20A */ u16     field_20A;
    /* 0x20C */ GpRec18 field_20C;
    /* 0x224 */ void*   field_224;
    /* 0x228 */ u16     field_228;
    /* 0x22A */ u16     field_22A;
    /* 0x22C */ u8      pad_22C[0x80];
    /* 0x2AC */ s32     field_2AC;
    /* 0x2B0 */ s32     field_2B0;
    /* 0x2B4 */ s32     field_2B4;
    /* 0x2B8 */ u8      pad_2B8[0x1E];
    /* 0x2D6 */ u16     field_2D6;
    /* 0x2D8 */ u8      pad_2D8[0x4];
    /* 0x2DC */ u16     field_2DC;
    /* 0x2DE */ u8      pad_2DE[0x16];
} Actor00700SpawnWork;
STATIC_ASSERT_SIZEOF(Actor00700SpawnWork, 0x2F4);

typedef struct Actor00700SpawnData {
    /* 0x000 */ u32 field_0;
    /* 0x004 */ u16 field_4;
} Actor00700SpawnData;
extern Actor00700SpawnData Actor00700_D07588;
extern u32                 Actor00700_D07584;
extern u32                 Actor00700_D075B4;

#endif
