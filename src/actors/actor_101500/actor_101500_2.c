#include "common.h"
#include "actors/actor_101500.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500_2", func_actor_101500_801337A8);

extern u8    D_actor_101500_801363AC[];
extern u8    D_actor_101500_801364F0[];
extern u8    D_actor_101500_801366DC[];
extern u8    D_actor_101500_801368B4[];
extern void* D_80067704[1];

void func_actor_101500_801338D0(Actor101500* arg0)
{
    GpAreaKey  key;
    u8         areaByte0;
    u32        raw1, index1;
    GpEffWork* effect1;
    TmdObject* model1;
    GpCdRec10* entry1;
    GpAreaKey* sessionKey1;
    u32        raw2, index2;
    GpEffWork* effect2;
    TmdObject* model2;
    GpCdRec10* entry2;
    GpAreaKey* sessionKey2;
    u32        raw3, index3;
    GpEffWork* effect3;
    TmdObject* model3;
    GpCdRec10* entry3;
    GpAreaKey* sessionKey3;
    u32        raw4, index4;
    GpEffWork* effect4;
    TmdObject* model4;
    GpCdRec10* entry4;
    GpAreaKey* sessionKey4;

    D_80067704[0] = D_actor_101500_801363AC;
    effect1       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x100, NULL);
    if (effect1 != NULL) {
        sessionKey1 = (GpAreaKey*)&Game_Session->field_4;
        raw1        = (u16)arg0->field_20->field_8;
        model1      = (TmdObject*)effect1->field_0->extra;
        key.field_3 = sessionKey1->field_3;
        key.field_2 = sessionKey1->field_2;
        key.field_1 = sessionKey1->field_1;
        areaByte0   = Game_Session->field_4;
        index1      = raw1 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry1           = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->field_24 = entry1->field_D;
        model1->field_25 = entry1->field_E;
        if (model1->field_18 != NULL) {
            Tmd_ProcessStream(model1);
            Tmd_ProcessStream(model1);
        }
    }

    D_80067704[0] = D_actor_101500_801364F0;
    effect2       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x100, NULL);
    if (effect2 != NULL) {
        sessionKey2 = (GpAreaKey*)&Game_Session->field_4;
        raw2        = (u16)arg0->field_20->field_8;
        model2      = (TmdObject*)effect2->field_0->extra;
        key.field_3 = sessionKey2->field_3;
        key.field_2 = sessionKey2->field_2;
        key.field_1 = sessionKey2->field_1;
        areaByte0   = Game_Session->field_4;
        index2      = raw2 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2           = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->field_24 = entry2->field_D;
        model2->field_25 = entry2->field_E;
        if (model2->field_18 != NULL) {
            Tmd_ProcessStream(model2);
            Tmd_ProcessStream(model2);
        }
    }

    D_80067704[0] = D_actor_101500_801366DC;
    effect3       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x100, NULL);
    if (effect3 != NULL) {
        sessionKey3 = (GpAreaKey*)&Game_Session->field_4;
        raw3        = (u16)arg0->field_20->field_8;
        model3      = (TmdObject*)effect3->field_0->extra;
        key.field_3 = sessionKey3->field_3;
        key.field_2 = sessionKey3->field_2;
        key.field_1 = sessionKey3->field_1;
        areaByte0   = Game_Session->field_4;
        index3      = raw3 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3           = (GpCdRec10*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->field_24 = entry3->field_D;
        model3->field_25 = entry3->field_E;
        if (model3->field_18 != NULL) {
            Tmd_ProcessStream(model3);
            Tmd_ProcessStream(model3);
        }
    }

    D_80067704[0] = D_actor_101500_801368B4;
    effect4       = Gp_SpawnEff(0x40007, &arg0->field_2C->field_8[1], 0x100, NULL);
    if (effect4 != NULL) {
        sessionKey4 = (GpAreaKey*)&Game_Session->field_4;
        raw4        = (u16)arg0->field_20->field_8;
        model4      = (TmdObject*)effect4->field_0->extra;
        key.field_3 = sessionKey4->field_3;
        key.field_2 = sessionKey4->field_2;
        key.field_1 = sessionKey4->field_1;
        areaByte0   = Game_Session->field_4;
        index4      = raw4 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry4           = (GpCdRec10*)((index4 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model4->field_24 = entry4->field_D;
        model4->field_25 = entry4->field_E;
        if (model4->field_18 != NULL) {
            Tmd_ProcessStream(model4);
            Tmd_ProcessStream(model4);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500_2", func_actor_101500_80133C10);

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500_2", func_actor_101500_80133EF8);
