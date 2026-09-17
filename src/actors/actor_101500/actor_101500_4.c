#include "common.h"
#include "actors/actor_101500.h"
#include "actors/actors_shared_80134a54.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500_4", func_actor_101500_801337A8);

extern u8    D_actor_101500_801363AC[];
extern u8    D_actor_101500_801364F0[];
extern u8    D_actor_101500_801366DC[];
extern u8    D_actor_101500_801368B4[];
extern void* D_80067704[1];
extern u8    D_801153F4;

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

/// Per-frame handler for the death sequence. Scene mode 1 only refreshes the
/// actor colour and mode 2 hides the model. Otherwise `field_35C` steps: state 0
/// saves the model matrix and unlinks the actor, 1 runs `ActorsShared80134a54`
/// and spawns an effect at frame 15, 3 frees the model's buffers once
/// `field_37E` passes 1 and 4 unlinks on its first frame; 1, 3 and 4 move to
/// 2 once `field_362` runs out, and 2 destroys the enemy.
void func_actor_101500_80133C10(GpEnemy* arg0, Actor101500* arg1)
{
    VECTOR           pos;
    Actor101500Work* work;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   sub;

    model = arg1->field_2C;
    work  = arg1->field_1C;
    coord = model->field_8;
    switch (D_801153F4) {
        case 0:
            break;
        case 1:
            sub = &coord[1];
            goto update;
        case 2:
            model->field_C     = 0x80;
            arg0->node.field_4 = 1;
            return;
    }
    switch (work->field_35C) {
        case 0:
            work->field_368 = 0x1000;
            work->field_32C = coord->coord;
            arg0->field_54  = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->field_1DC);
            Gp_UnlinkObj(&work->field_244);
            Gp_UnlinkObj(&work->field_2DC);
            Gp_SetLightMode((GpObj4C*)arg0, 1);
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xF);
            work->field_362 = 0;
            work->field_35C = 1;
            if (work->field_37E != 0) {
                model->field_C  = 0x80;
                work->field_35C = 3;
            }
            break;
        case 1:
            ActorsShared80134a54((ActorShared80134a54*)arg1);
            work->field_362++;
            if (work->field_362 == 10) {
                model->field_C = 2;
            }
            if (work->field_362 == 15) {
                Gp_SpawnEff(0x600A5, coord, 2, NULL);
            }
            if (work->field_362 >= 60) {
                work->field_35C = 2;
            }
            break;
        case 2:
            Gp_DestroyEnemy(arg0, (Task*)arg1);
            return;
        case 3:
            if (work->field_37E != 0) {
                if (work->field_37E >= 2) {
                    work->field_37E = 0;
                    Tmd_FreeBuffers(model);
                    model->field_C |= 4;
                    func_actor_101500_801338D0(arg1);
                } else {
                    work->field_37E++;
                }
            }
            work->field_362++;
            if (work->field_362 >= 60) {
                work->field_35C = 2;
            }
            break;
        case 4:
            if (work->field_362 == 0) {
                Gp_UnlinkNode(&arg0->node);
                Gp_UnlinkObj(&work->field_1DC);
                Gp_UnlinkObj(&work->field_244);
                Gp_UnlinkObj(&work->field_2DC);
                Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xF);
            }
            work->field_362++;
            if (work->field_362 >= 61) {
                work->field_35C = 2;
            }
            break;
    }
    sub = arg1->field_2C->field_8;
    sub = &sub[1];
update:
    pos.vx = sub->workm.t[0];
    pos.vy = sub->workm.t[1];
    pos.vz = sub->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &pos, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500_4", func_actor_101500_80133EF8);

/// Closes this unit's `.rodata` after the jump table above so
/// `actors_shared_801344f8`'s rodata starts at 0x80131E8C. Nothing reads it.
const u32 D_actor_101500_80131E88 = 0;
