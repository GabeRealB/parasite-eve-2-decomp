#ifndef ACTOR_400100_DAMAGE_H
#define ACTOR_400100_DAMAGE_H

#include "actors/actor_400100.h"

void Actor00100_Fn03340(Actor00100*, s16, s32);

typedef struct Actor00100DamageWork {
    s16     field_0;
    s16     field_2;
    u8      pad_4[0x2];
    s16     field_6;
    u8      pad_8[0x822];
    s16     field_82A;
    u8      pad_82C[0xA];
    s16     field_836;
    s16     field_838;
    u8      pad_83A[0x6];
    s16     field_840;
    u8      pad_842[0x2];
    s16     field_844;
    u8      pad_846[0xBE];
    u8      field_904;
    u8      field_905;
    s16     field_906;
    u8      pad_908[0x20];
    GpRec18 primaryHits[5];
    u8      pad_9A0[0x20];
    GpRec18 secondaryHits[5];
    u8      pad_A38[0x1A8];
    s16     field_BE0;
    u16     field_BE2;
    s16     field_BE4;
    u8      pad_BE6[0x44];
    s16     field_C2A;
} Actor00100DamageWork;
typedef struct Actor00100DamageScratch {
    s32 field_0;
    s32 field_4;
    s32 field_8;
    u8  pad_C[0x4];
    s16 field_10;
    s16 field_12;
    s16 field_14;
    u8  pad_16[0x2];
    s16 field_18;
    s16 field_1A;
    s16 field_1C;
    u8  pad_1E[0x2];
    s32 field_20;
    s32 field_24;
    s32 field_28;
    s16 field_2C;
    s16 field_2E;
} Actor00100DamageScratch;

static __inline__ s32 Actor00100_FindDamageHit(GpRec18* records, SVECTOR* pos)
{
    s16 i;
    for (i = 0; i < 5; i++) {
        if (!records[i].field_4)
            break;
        if ((records[i].field_4 & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].field_8;
            pos->vy = records[i].field_A;
            pos->vz = records[i].field_C;
            return records[i].field_4;
        }
    }
    return 0;
}

static __inline__ void Actor00100_SetHitState(Actor00100DamageWork* work)
{
    s32 state = work->field_0;
    if (state == 4 || state == 7 || state == 0x21 || state == 0x14 || state == 0xB || state == 0x11 || (state == 0x24 && work->field_6 < 10))
        work->field_0 = 7;
    else
        work->field_0 = 0x14;
}

#endif
