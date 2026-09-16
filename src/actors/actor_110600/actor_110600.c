#include "common.h"

#include "actors/actor_110600.h"
#include "actors/actors_shared_8013411c.h"
#include "actors/actors_shared_80135a60.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801322CC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132470);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132654);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801327EC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132958);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132A84);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132D54);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132FE0);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80133550);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80133778);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80133A94);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// rescales it uniformly: `ratan2` of the rotation's Z basis gives the yaw,
/// `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// `scale` on all three axes. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. Written as an inline so the four scratch-head accesses
/// stay absolute; see `Actor444000_ShrinkRotation` in `actor_444000_5.c`.
static __inline__ void Actor110600_ScaleRotation(Task* task, s16 scale)
{
    ActorShared80135a60Scratch* blk;
    GsCOORDINATE2*              coord;
    u8*                         head;
    s16                         ang;
    u16                         m22;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    coord                                         = ((TmdObject*)task->extra)->field_8;
    blk                                           = (ActorShared80135a60Scratch*)(head - 0x34);
    *(ActorShared80135a60Scratch**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]                    = *(u16*)&((ActorShared80135a60Scratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]                    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]                    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]                    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]                    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]                    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]                    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]                    = *(u16*)&blk->m.m[2][1];
    m22                                     = *(u16*)&blk->m.m[2][2];
    coord->flg                              = 0;
    coord->coord.m[2][2]                    = m22;
    ((TmdObject*)task->extra)->field_8->flg = 0;
    *(u8**)G_SCRATCH_HEAD                   = *(u8**)G_SCRATCH_HEAD + 0x34;
}

/// Placement opcode: drops the model's root coordinate onto `placement` (the
/// three longs become its translation, the Euler angles go through
/// `Gfx_RotMatrixX` / `Y` / `Z`), then rebuilds and rescales that coordinate
/// from the actor's own heading and caches the resulting yaw in the work
/// block's `field_8`. Same placement as `ActorsShared80169f74`, with the
/// rescale of `ActorsShared80135a60` folded in behind it.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    Actor110600Work* work;

    work = (Actor110600Work*)task->idMap;

    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    Actor110600_ScaleRotation(task, (s16)work->field_B7C);
    work->field_8 = ratan2(-((TmdObject*)task->extra)->field_8->coord.m[2][0],
                           ((TmdObject*)task->extra)->field_8->coord.m[2][2]);
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134040);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801341A4);

/// Per-tick animation pass: for each clip id 1..0x12, the first ten (`i < 0xB`)
/// write the two clip ids into their slot's `field_9` and tick the primary and
/// blend contexts through `func_800B3448`, then hand both poses to
/// `Gp_AnimWritePoseCopy` with `weight` at 0x8A0 and its complement; the rest
/// only rewrite the primary slot and `Gp_AnimTickIndex` it. Same body as
/// `func_actor_403000_801336B4`, which walks 24 slots instead of 19.
void func_actor_110600_80134438(Actor110600* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor110600AnimWork* work;

    work   = (Actor110600AnimWork*)arg0->field_1C;
    weight = work->field_8A0;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_89E;
            work->slots[i].field_9      = (u8)(work->field_896 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_896 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134564);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134728);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134AB4);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135194);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135454);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135A18);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135B84);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135E20);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80136210);

/// Timer stage that walks between the two long `field_892` values. Entering on
/// a live actor re-arms it: clear the model object, take 0x8000 off
/// `field_A90.flags` and put 0x4000 on `field_950.flags`, tag the enemy's link
/// node, set the stage timer to 0x18 and `field_896` from `field_898`, then
/// re-arm the walker block at 0xB28 for a fresh patrol (`field_5C` cleared,
/// `field_5E` reloaded from `field_B86`, `field_60` = 8) with `field_B90` /
/// `field_B82` / `field_8A4` / `field_8A2` cleared. Every tick after that steps
/// the walker and the model, then retimes: at 0x18 a draw of `Gp_LcgState`
/// whose seventh bit is clear drops it to 0xE, and at 0xE the `field_5C` bit 0
/// the walker sets on arrival — or on hitting something — puts it back to 0x18.
/// Both retimes re-enter state 1 (`field_88C`) and tick once more.
void func_actor_110600_80136888(Actor110600* arg0)
{
    Actor110600Work*   work;
    Actor110600Walker* walker;
    GpEnemy*           enemy;
    TmdObject*         obj;
    u32                rng;
    u16                ramp;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4   = 8;
        work->field_88C       = 1;
        work->field_892       = 0x18;
        work->field_896       = work->field_898;
        ramp                  = work->field_B86;
        walker                = (Actor110600Walker*)((u8*)work + 0xB28);
        work->field_B90       = 0;
        walker->field_5C      = 0;
        walker->field_5E      = ramp;
        walker->field_60      = 8;
        work->field_B82       = 0;
        work->field_8A4       = 0;
        work->field_8A2       = 0;
    }
    func_actor_110600_80133A94((Actor110600Walker*)((u8*)work + 0xB28));
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 0x18) {
        if (work->field_5C & 2) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng;
            if (!((rng >> 16) & 7)) {
                work->field_892 = 0xE;
                work->field_88C = 1;
                func_actor_110600_80134728(arg0);
            }
        }
    }
    if ((work->field_892 == 0xE) && (work->field_5C & 1)) {
        work->field_892 = 0x18;
        work->field_88C = 1;
        func_actor_110600_80134728(arg0);
    }
}

/// The walker's handoff stage. Entering on a live actor re-arms it: clear the
/// model object, take 0x8000 off `field_A90.flags` and put 0x4000 on
/// `field_950.flags`, tag the enemy's link node, park the stage timer at 0x1D
/// with `field_896` at 0x10, then re-arm the walker block at 0xB28 to run its
/// patrol out (`field_5C` = 0xFFFE, `field_5E` reloaded from `field_B86`,
/// `field_60` = 2) with `field_B90` / `field_B82` / `field_8A4` / `field_8A2`
/// cleared. Every tick after that steps the walker and the model; at 0x1D the
/// `field_5C` bit 0 the walker sets on arrival moves the stage to 0x1E and
/// re-seeds the walker block, and at 0x1E that same bit picks what the actor
/// does next: 0xB while the enemy's `field_40` is still positive, 0xC once it
/// has run out.
void func_actor_110600_801369D8(Actor110600* arg0)
{
    Actor110600Work*   work;
    Actor110600Walker* walker;
    Actor110600Walker* walker2;
    GpEnemy*           enemy;
    u16                ramp;
    u16                ramp2;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C = 0;
        work->field_A90.flags   = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags   = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4     = 8;
        work->field_88C         = 2;
        work->field_892         = 0x1D;
        work->field_896         = 0x10;
        ramp                    = work->field_B86;
        walker                  = (Actor110600Walker*)((u8*)work + 0xB28);
        work->field_B90         = 0;
        walker->field_5C        = 0xFFFE;
        walker->field_5E        = ramp;
        walker->field_60        = 2;
        work->field_B82         = 0;
        work->field_8A4         = 0;
        work->field_8A2         = 0;
    }
    walker2 = (Actor110600Walker*)((u8*)work + 0xB28);
    func_actor_110600_80133A94(walker2);
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 0x1D) {
        if (work->field_5C & 1) {
            ramp2             = work->field_B86;
            work->field_892   = 0x1E;
            work->field_88C   = 2;
            walker2->field_5C = 0;
            walker2->field_60 = 2;
            walker2->field_5E = ramp2;
            return;
        }
    }
    if ((work->field_892 == 0x1E) && (work->field_5C & 1)) {
        if (enemy->field_40 > 0) {
            work->field_0 = 0xB;
        } else {
            work->field_0 = 0xC;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80136B20);

/// Re-dresses a live actor: take the model object out of draw, drop bit 0x8000
/// of `field_A90.flags` and bit 0x4000 of `field_950.flags`, tag the enemy's
/// link node, clear the `field_B82` / `field_8A4` / `field_8A2` timers and hand
/// the model the 0x80 texture page, then spawn five effects off its part
/// coordinates 6, 8, 10, 11 and 15 (`Gp_SpawnEff` bank 0xA0005, buffer sizes
/// 0x200 / 0x200 / 0x200 / 0x300 / 0x300). Each spawned model object takes its
/// texture page and CLUT from the nested area record the actor's own area key
/// resolves to, and is streamed twice once its aux buffer exists.
void func_actor_110600_80136ECC(Actor110600* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GpAreaKey        key;
    u8               areaByte0;
    u32              raw1, index1;
    GpEffWork*       effect1;
    TmdObject*       model1;
    GpCdRec10*       entry1;
    GpAreaKey*       sessionKey1;
    u32              raw2, index2;
    GpEffWork*       effect2;
    TmdObject*       model2;
    GpCdRec10*       entry2;
    GpAreaKey*       sessionKey2;
    u32              raw3, index3;
    GpEffWork*       effect3;
    TmdObject*       model3;
    GpCdRec10*       entry3;
    GpAreaKey*       sessionKey3;
    u32              raw4, index4;
    GpEffWork*       effect4;
    TmdObject*       model4;
    GpCdRec10*       entry4;
    GpAreaKey*       sessionKey4;
    u32              raw5, index5;
    GpEffWork*       effect5;
    TmdObject*       model5;
    GpCdRec10*       entry5;
    GpAreaKey*       sessionKey5;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy                  = arg0->field_20;
        obj                    = arg0->field_2C;
        obj->field_C           = 0;
        work->field_A90.flags &= 0x7FFF;
        work->field_950.flags &= 0xBFFF;
        enemy->node.field_4    = 1;
        work->field_B82        = 0;
        work->field_8A4        = 0;
        work->field_8A2        = 0;
        obj->field_C           = 0x80;

        effect1 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[6], 0x200, NULL);
        if (effect1 != NULL) {
            sessionKey1 = (GpAreaKey*)&Game_Session->field_4;
            raw1        = enemy->field_8;
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

        effect2 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[8], 0x200, NULL);
        if (effect2 != NULL) {
            sessionKey2 = (GpAreaKey*)&Game_Session->field_4;
            raw2        = enemy->field_8;
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

        effect3 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[10], 0x200, NULL);
        if (effect3 != NULL) {
            sessionKey3 = (GpAreaKey*)&Game_Session->field_4;
            raw3        = enemy->field_8;
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

        effect4 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[11], 0x300, NULL);
        if (effect4 != NULL) {
            sessionKey4 = (GpAreaKey*)&Game_Session->field_4;
            raw4        = enemy->field_8;
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

        effect5 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[15], 0x300, NULL);
        if (effect5 != NULL) {
            sessionKey5 = (GpAreaKey*)&Game_Session->field_4;
            raw5        = enemy->field_8;
            model5      = (TmdObject*)effect5->field_0->extra;
            key.field_3 = sessionKey5->field_3;
            key.field_2 = sessionKey5->field_2;
            key.field_1 = sessionKey5->field_1;
            areaByte0   = Game_Session->field_4;
            index5      = raw5 >> 12;
            key.field_0 = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry5           = (GpCdRec10*)((index5 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model5->field_24 = entry5->field_D;
            model5->field_25 = entry5->field_E;
            if (model5->field_18 != NULL) {
                Tmd_ProcessStream(model5);
                Tmd_ProcessStream(model5);
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801372CC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137684);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801377FC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137980);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137AF4);

/// Death stage machine, entering on a live actor: take the model out of draw,
/// drop bit 0x8000 of `field_A90.flags` and set 0x4000 of `field_950.flags`,
/// tag the enemy's link node, arm `field_892` / `field_88C` and the `field_896`
/// timer, tick once and clear both `field_BE0` and the `field_BE2` stage. Stage
/// 0 idles on that timer — once the pose `field_4E` reaches 4 it parks
/// `field_896` at -0x10 and steps to stage 1. Stage 1 is the shrink tail:
/// halves `field_896` each tick, parking at -0xC when the halving lands on the
/// stage value and bouncing -1 back to 8, and after 0x35 ticks parks
/// `field_896` / `field_898` at 0x38 and moves the actor to state 3. Every
/// stage-1 tick also adds 0x27 to `field_BE4`.
void func_actor_110600_80137DB0(Actor110600* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    s16              step;
    s32              state;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy                 = arg0->field_20;
        obj                   = arg0->field_2C;
        obj->field_C          = 0;
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4   = 8;
        work->field_892       = 0xC;
        work->field_88C       = 2;
        work->field_B82       = 0;
        work->field_8A4       = 0;
        work->field_896       = 6;
        func_actor_110600_80134728(arg0);
        work->field_BE0 = 0;
        work->field_BE2 = 0;
    }
    state           = work->field_BE2;
    work->field_88E = 0;
    switch (state) {
        case 0:
            func_actor_110600_80134728(arg0);
            if ((work->field_4E & 0x3FF) == 4) {
                work->field_896 = -0x10;
                work->field_BE2 = (s16)((u16)work->field_BE2 + 1);
                return;
            }
            return;
        case 1:
            step            = (s16)work->field_896 / 2;
            work->field_896 = step;
            work->field_BE0++;
            if (work->field_896 == state) {
                work->field_896 = -0xC;
            }
            if (work->field_896 == -1) {
                work->field_896 = 8;
            }
            func_actor_110600_80134728(arg0);
            if (work->field_BE0 >= 0x35) {
                work->field_896 = 0x38;
                work->field_898 = 0x38;
                work->field_0   = 3;
            }
            work->field_BE4 += 0x27;
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137F2C);

void func_actor_110600_80138394(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_110600/actor_110600", ActorsShared80135df4Table);

/// The `0x7D3` handler of the display-opcode table `D_actor_110600_80148624`:
/// maps the requested state onto the work block's `field_892` (0x22..0x28) and
/// parks the actor in state 0x11 with `field_2` cleared. States 0 and 4 also
/// stamp the enemy's occupancy tag and re-save its pose; state 0 writes its own
/// `field_892` ahead of those calls, so it skips the store the other four share,
/// which is the tail the compiler merged out of the four `break`s.
///
/// The table GCC emits for this switch is what pins the package's
/// `rodata_head`: it lands at 0x18C, 8-aligned only if this unit's `.rodata`
/// starts at 0x4 rather than 0x0 — the package id ahead of it is prepended, not
/// compiled — and behind the id it picks up `.align 3`'s 4-byte pad instead.
s32 func_actor_110600_8013839C(Actor110600* arg0, s32 arg1, Actor110600Msg7D3* arg2)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    s32              state;

    state = arg2->field_4;
    work  = arg0->field_1C;
    enemy = arg0->field_20;
    switch (state) {
        case 0:
            work->field_892 = 0x22;
            enemy->field_4B = 1;
            Gp_SaveEnemyPose(enemy);
            break;
        case 1:
            work->field_892 = 0x23;
            break;
        case 2:
            work->field_892 = 0x24;
            break;
        case 3:
            work->field_892 = 0x25;
            break;
        case 4:
            enemy->field_4B = 1;
            Gp_SaveEnemyPose(enemy);
            work->field_892 = 0x28;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// Display-object handler, the same shape as `ActorsShared8013d268` one overlay
/// over: `arg2` selects the mode and `GpEnemy.field_4B` -- the occupancy tag
/// `Gp_SaveEnemyPose` writes -- decides whether mode 1 shows the object again.
/// Mode 0 hides it (bit 0x80 of `TmdObject.field_C`) and reinstates its buffers;
/// 1 hides it and restarts the work block's `field_0` while the tag reads 4, and
/// otherwise clears `field_C` and reinstates the buffers; 2 only sets bit 0x4;
/// 3 clears `field_C`, restarts `field_0` and then sets bit 0x4. `arg1` is
/// unused; it exists because the dispatch passes three arguments.
s32 func_actor_110600_80138448(Actor110600* arg0, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor110600Work* work;
    GpEnemy*         enemy;

    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    work  = arg0->field_1C;
    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            if (enemy->field_4B == 0) {
                obj->field_C = 0;
                Tmd_AllocBuffers(obj);
            } else if (enemy->field_4B == 4) {
                obj->field_C  = 0x80;
                work->field_0 = 0;
            } else {
                obj->field_C = 0;
                Tmd_AllocBuffers(obj);
            }
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
            if (enemy->field_4B == 4) {
                obj->field_C = 0x80;
            } else {
                obj->field_C = 0;
            }
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}

s32 func_actor_110600_80138538(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor110600Work*)arg0->idMap;
    if (enemy->field_40 > 0) {
        return 1;
    }
    work->field_BE4 = 0;
    enemy->field_4C = 0;
    work->field_BE6 = 0;
    return 0;
}
