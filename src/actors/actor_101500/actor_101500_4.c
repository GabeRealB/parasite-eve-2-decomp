#include "common.h"
#include "actors/actor_101500.h"
#include "actors/actors_shared_80134a54.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/tmd.h"

extern MATRIX* D_80073B8C;

void func_actor_101500_801337A8(Actor101500* arg0)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s16              bob;

    work  = arg0->field_1C;
    coord = arg0->field_2C->coords;
    bob   = 0;
    if ((s16)work->field_352 == 5) {
        work->field_37C++;
        if (work->field_37C >= 15) {
            work->field_37C = 0;
        }
        bob = D_actor_101500_8013BE90[work->field_37C];
    }
    work->field_31C.vx = coord->coord.t[0];
    work->field_31C.vy = coord->coord.t[1];
    work->field_31C.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_360) >> 12;
    coord->coord.t[1] += work->field_366 + bob;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_360) >> 12;
    if (coord->coord.t[1] - D_80073B8C->t[1] > 5000) {
        arg0->field_30  = 2;
        work->field_35A = 8;
        work->field_35C = 4;
        work->field_362 = 0;
    }
}

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
    effect1       = Gp_SpawnEff(0x40007, &arg0->field_2C->coords[1], 0x100, NULL);
    if (effect1 != NULL) {
        sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
        raw1        = (u16)arg0->field_20->field_8;
        model1      = (TmdObject*)effect1->field_0->extra;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->at4.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry1        = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->tpage = entry1->field_D;
        model1->clut  = entry1->field_E;
        if (model1->buffer != NULL) {
            Tmd_ProcessStream(model1);
            Tmd_ProcessStream(model1);
        }
    }

    D_80067704[0] = D_actor_101500_801364F0;
    effect2       = Gp_SpawnEff(0x40007, &arg0->field_2C->coords[1], 0x100, NULL);
    if (effect2 != NULL) {
        sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
        raw2        = (u16)arg0->field_20->field_8;
        model2      = (TmdObject*)effect2->field_0->extra;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->at4.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2        = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->field_D;
        model2->clut  = entry2->field_E;
        if (model2->buffer != NULL) {
            Tmd_ProcessStream(model2);
            Tmd_ProcessStream(model2);
        }
    }

    D_80067704[0] = D_actor_101500_801366DC;
    effect3       = Gp_SpawnEff(0x40007, &arg0->field_2C->coords[1], 0x100, NULL);
    if (effect3 != NULL) {
        sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
        raw3        = (u16)arg0->field_20->field_8;
        model3      = (TmdObject*)effect3->field_0->extra;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->at4.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3        = (GpCdRec10*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->tpage = entry3->field_D;
        model3->clut  = entry3->field_E;
        if (model3->buffer != NULL) {
            Tmd_ProcessStream(model3);
            Tmd_ProcessStream(model3);
        }
    }

    D_80067704[0] = D_actor_101500_801368B4;
    effect4       = Gp_SpawnEff(0x40007, &arg0->field_2C->coords[1], 0x100, NULL);
    if (effect4 != NULL) {
        sessionKey4 = (GpAreaKey*)&gGameSession->at4.loc;
        raw4        = (u16)arg0->field_20->field_8;
        model4      = (TmdObject*)effect4->field_0->extra;
        key.stage   = sessionKey4->stage;
        key.area    = sessionKey4->area;
        key.room    = sessionKey4->room;
        areaByte0   = gGameSession->at4.loc.view;
        index4      = raw4 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry4        = (GpCdRec10*)((index4 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model4->tpage = entry4->field_D;
        model4->clut  = entry4->field_E;
        if (model4->buffer != NULL) {
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
    coord = model->coords;
    switch (D_801153F4) {
        case 0:
            break;
        case 1:
            sub = &coord[1];
            goto update;
        case 2:
            model->flags       = 0x80;
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
                model->flags    = 0x80;
                work->field_35C = 3;
            }
            break;
        case 1:
            ActorsShared80134a54((ActorShared80134a54*)arg1);
            work->field_362++;
            if (work->field_362 == 10) {
                model->flags = 2;
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
                    model->flags |= 4;
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
    sub = arg1->field_2C->coords;
    sub = &sub[1];
update:
    pos.vx = sub->workm.t[0];
    pos.vy = sub->workm.t[1];
    pos.vz = sub->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &pos, 0, 0);
}

extern u16     D_actor_101500_8013BE08[];
extern SVECTOR D_actor_101500_8013BEB0;
extern s16     D_actor_101500_8013BEB2;
extern SVECTOR D_actor_101500_8013BEB8;

void func_actor_101500_80133EF8(Actor101500* arg0)
{
    u8*              head;
    VECTOR3*         stk;
    VECTOR3*         vec;
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s32              dy;
    s32              ady;
    u16              val;
    u16              val2;
    u16*             tbl;
    s32              off;
    s32              delay;

    head              = *(u8**)0x1F8003FC;
    stk               = (VECTOR3*)(head - 0x10);
    *(u8**)0x1F8003FC = (u8*)stk;
    vec               = stk;
    work              = arg0->field_1C;
    coord             = arg0->field_2C->coords;
    switch (work->field_35C) {
        case 0:
            off = work->field_364 + 800;
            dy  = D_actor_101500_8013BEB2 - off - coord->coord.t[1];
            ady = abs(dy);
            if (ady < 30 || --work->field_362 <= 0) {
                work->field_35C = 1;
            } else {
                work->field_366 = dy > 0 ? 30 : -30;
            }
            vec->vx                = D_actor_101500_8013BEB0.vx - coord->coord.t[0];
            vec->vy                = 0;
            vec->vz                = D_actor_101500_8013BEB0.vz - coord->coord.t[2];
            work->field_372        = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376        = 100;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
            break;
        case 1:
            work->field_366 = 0;
            work->field_360 = 0;
            work->field_36C = 0;
            if (--work->field_362 < 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                val             = D_actor_101500_8013BE08[(Gp_LcgState >> 16) & 0xF];
                work->field_352 = 6;
                work->field_35C = 2;
                work->field_35E = val;
            }
            ((VECTOR3*)(head - 0x10))->vx = D_actor_101500_8013BEB0.vx - coord->coord.t[0];
            stk->vy                       = 0;
            stk->vz                       = D_actor_101500_8013BEB0.vz - coord->coord.t[2];
            work->field_372               = ratan2((s16)((VECTOR3*)(head - 0x10))->vx, (s16)stk->vz) & 0xFFF;
            work->field_376               = 100;
            break;
        case 2:
            work->field_360  = 200;
            work->field_35E -= 200;
            if ((s16)work->field_35E < 0) {
                tbl             = D_actor_101500_8013BDE8;
                work->field_352 = 5;
                val2            = tbl[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_35C = 1;
                work->field_362 = val2;
            }
            break;
    }
    if (D_80073B8C->t[0] > D_actor_101500_8013BEB8.vx && D_80073B8C->t[2] < D_actor_101500_8013BEB8.vz) {
        work->field_35A = 3;
        delay           = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F) + 60;
        work->field_352 = 5;
        work->field_380 = 15;
        work->field_35C = 0;
        work->field_34C = 0x400F0002;
        work->field_358 = 1;
        work->field_37A = 1;
        work->field_362 = delay;
        work->field_364 = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1FF;
        Gp_ArmStateF0(1);
    }
    *(u8**)0x1F8003FC += 0x10;
}

/// Closes this unit's `.rodata` after the jump table above so
/// `actors_shared_801344f8`'s rodata starts at 0x80131E8C. Nothing reads it.
const u32 D_actor_101500_80131E88 = 0;
