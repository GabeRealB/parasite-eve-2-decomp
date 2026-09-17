#include "common.h"
#include "actors/actor_101500.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/tmd.h"

/// Spawn handler: allocates the work area, binds the model and collision
/// objects, and seeds the pose from the spawn variant in `GpAreaPlace.field_1`.
void func_actor_101500_80131EB4(GpEnemy* arg0, Actor101500* arg1)
{
    Actor101500Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    GpRec18*         records;
    u32              draw;
    s32              i;
    s32              r;

    obj   = arg1->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x384U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, (Task*)arg1);
        return;
    }
    arg1->field_1C = work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_1C  = &work->field_1BC;
    obj->field_20  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18     = &arg1->field_2C->field_8[2];
    arg0->node.field_4 = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &D_actor_101500_8013BDD8;
    arg0->field_54     = (s32)work->field_1FC;
    arg0->field_40     = D_actor_101500_8013BDD8.field_4;
    work->field_314    = coord;
    work->field_318    = 0x300;
    work->field_31A    = 1;
    place              = arg0->field_3C;
    switch (work->field_382 = place->field_1) {
        case 0:
            work->field_36E = ((GpAreaPlace*)arg0->field_3C)->field_2 & 1;
            work->field_370 = (((GpAreaPlace*)arg0->field_3C)->field_2 >> 1) & 1;
            switch (work->field_36E) {
                case 0:
                    work->field_352 = 1;
                    work->field_354 = 1;
                    work->field_34C = 0;
                    break;
                case 1:
                    work->field_352 = 2;
                    work->field_354 = 2;
                    work->field_34C = 0;
                    break;
            }
            work->field_362 = 0;
            draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->field_364    = ((draw >> 16) & 0x1F) + 1;
            work->field_372    = (ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) + 0x800) & 0xFFF;
            break;
        case 1:
            work->field_370 = 1;
            work->field_352 = 5;
            work->field_354 = 5;
            work->field_35A = 9;
            work->field_35C = 0;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_362 = ((Gp_LcgState >> 16) & 0x3F) + 0x3C;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_364 = (Gp_LcgState >> 16) & 0x1FF;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            break;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    func_800B3F84((GpAnimCtx*)work, D_actor_101500_8013BE34, (GpAnimObj*)obj, work->field_12C,
                  (GpAnimSlot*)work->field_14);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, (s16)work->field_352);
    }
    if (work->field_382 == 0) {
        draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        r                  = (draw >> 16) & 0x3F;
        for (i = 1; i < 7; i++) {
            func_800B4114(work, i, work->field_352, 0, r);
        }
    }
    work->field_1DC.field_8  = &arg1->field_2C->field_8[2];
    work->field_1DC.field_C  = work->field_1FC;
    work->field_1DC.field_10 = 0;
    work->field_1DC.field_12 = 0;
    work->field_1DC.field_14 = 0;
    work->field_1DC.field_18 = 0x3000F;
    work->field_1DC.field_1C = 0x12C;
    work->field_1DC.flags    = 1;
    Gp_LinkObj(2, &work->field_1DC);
    Gp_InitRec18Table(work->field_1FC, 3, 0);
    work->field_244.field_C  = work->field_264;
    work->field_244.field_8  = coord;
    work->field_244.field_10 = 0;
    work->field_1DC.flags   |= 0x8000;
    if (work->field_36E == 0) {
        work->field_244.field_12 = 0;
        work->field_244.field_14 = -0x12C;
    } else {
        work->field_244.field_12 = 0x12C;
        work->field_244.field_14 = 0;
    }
    work->field_244.field_18 = 0x3000F;
    work->field_244.field_1C = 0x12C;
    work->field_244.flags    = 1;
    Gp_LinkObj(2, &work->field_244);
    Gp_InitRec18Table(work->field_264, 5, 0);
    records                  = work->field_2FC;
    work->field_2DC.field_8  = coord;
    work->field_2DC.field_C  = records;
    work->field_2DC.field_10 = 0;
    work->field_2DC.field_12 = 0;
    work->field_2DC.field_14 = 0x190;
    work->field_244.flags   |= 0x4200;
    work->field_2DC.field_18 = Gp_PackPair(&D_actor_101500_8013BDD4, 0);
    work->field_2DC.field_1C = 0x12C;
    work->field_2DC.flags    = 1;
    Gp_LinkObj(3, &work->field_2DC);
    Gp_InitRec18Table(records, 1, 0);
    work->field_2DC.flags &= 0x7FFF;
    arg1->field_30         = 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_101500/actor_101500", D_actor_101500_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_101500/actor_101500", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500", func_actor_101500_8013230C);

void func_actor_101500_8013291C(Actor101500* actor, s32 damage)
{
    GpEnemy*         enemy;
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s32              id;

    enemy            = actor->field_20;
    work             = actor->field_1C;
    coord            = actor->field_2C->field_8;
    enemy->field_40 -= damage;
    if (enemy->field_40 <= 0) {
        work->field_378 = 1;
    }
    id = ((actor->field_20->field_8 >> 12) << 8) | 0x400F0004;
    SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    if (enemy->field_40 <= (D_actor_101500_8013BDDC * 60) / 100) {
        work->field_358 = 2;
        if (work->field_35A != 5) {
            work->field_35A          = 4;
            work->field_244.field_12 = -300;
            work->field_244.field_14 = 0;
        }
    } else {
        work->field_35A = 7;
        switch (work->field_358) {
            case 0:
                if (work->field_36E == 0) {
                    work->field_352 = 11;
                    work->field_354 = 1;
                } else {
                    work->field_352 = 12;
                    work->field_354 = 2;
                }
                break;
            case 1:
                work->field_352 = 13;
                work->field_354 = 6;
                break;
            case 2:
                work->field_352 = 13;
                work->field_354 = 14;
                break;
        }
        work->field_34C = 0;
        work->field_356 = 0;
    }
    Gp_SetStateF0Byte3(2);
}

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500", func_actor_101500_80132AC4);
