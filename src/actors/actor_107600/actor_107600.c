#include "common.h"

#include "main/mem.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/session.h"

#include "gameplay/gameplay.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

#include "actors/actor_107600.h"

/* The controller task this actor is reparented to is the Mist shooting
 * gallery's, so the counter at +0xE of its work block is that room's. */
#include "rooms/mist_shooting_gallery.h"

#include <psyq/inline_c.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_gpl12_real() __asm__ volatile("nop; nop; .word 0x4BA8003E")

/* Leading-rodata state table. A local initializer would emit the pool at this
 * function's .rodata instead of at D_actor_107600_80131E24. */
extern const TaskFuncTable4 D_actor_107600_80131E24;

/* Second table out of the same leading-rodata block, run by
 * `func_actor_107600_801348A0`. */
extern const TaskFuncTable4 D_actor_107600_80131E74;

/* Third table out of that block, run by `func_actor_107600_80132CD4` and
 * holding one entry per `Actor107600Work.field_144` phase. */
extern const TaskFuncTable3 D_actor_107600_80131E34;

/* Fourth table out of that block, run by `func_actor_107600_80133024` on
 * `Actor107600Work.field_158`. */
extern const TaskFuncTable10 D_actor_107600_80131E84;

/* Eight effect offsets `func_actor_107600_80133024` cycles through from
 * `Actor107600Work.field_16A`. */
extern Actor107600Pair D_actor_107600_80135730[];

/* Table `func_actor_107600_80132DF0` spawns from, indexed with `arg1 + 1`; it
 * is the trailing animation/data blob, not the leading rodata. */
extern TaskDesc D_actor_107600_80134F94;

/* The pair-source record the spawn state hangs off the enemy's `GpEnemy.field_50`
 * (a zeroed pointer to `D_actor_107600_8013571C`, 0x32 and 0xFF000000) and the
 * 16-entry HP table it indexes with the spawn variant. Both are trailing-blob
 * data, after the collision tables. */
/* Pair-source record the spawn state hangs off `GpEnemy.field_50`. */
extern GpPairSrcE D_actor_107600_80134F84;
extern GpPairSrcE D_actor_107600_80135720;
extern u16        D_actor_107600_80135750[];

/* Remaining-enemy count, and the gallery controller task the room overlay
 * publishes (its `Task::idMap` is the `MistShootingGalleryWork`). */
extern s16   D_80073BA0;
extern Task* D_8018E0C4;

/// Spawn state of the `D_actor_107600_80131E24` table. The target is dropped
/// (and the gallery's live count given back) when the player is within 0x400 on
/// XZ unless `Task::spawnArg1` bit 0x40000000 forces it, when byte 0 of
/// `spawnArg1` is the 0xFF marker, or when the work block cannot be allocated.
/// Otherwise binds the work block's matrices, records the spawn position, and
/// spawns the child from `func_actor_107600_80132DF0`.
void func_actor_107600_80131F10(Task* arg0)
{
    TmdObject*       obj;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   target;
    void**           scratch;
    u8*              head;
    VECTOR*          block;
    Actor107600Work* work;

    obj       = arg0->extra;
    enemy     = arg0->spawnArg2;
    coord     = obj->field_8;
    target    = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = target->coord.t[0] - coord->coord.t[0];
    *scratch  = block;
    block->vz = target->coord.t[2] - coord->coord.t[2];
    if ((!(arg0->spawnArg1 & 0x40000000) && func_80103D8C(block->vx, block->vz) < 0x401) || (u8)arg0->spawnArg1 == 0xFF) {
    fail:
        if ((arg0->spawnArg1 & 0xF000) != 0x2000) {
            ((MistShootingGalleryWork*)arg0->parent->idMap)->field_0E--;
        }
        *(u8**)G_SCRATCH_HEAD += 0x10;
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    work        = Mem_Calloc(0x14C, false);
    arg0->idMap = (TaskIdMap*)work;
    if (work == NULL) {
        goto fail;
    }
    arg0->exitCallback = func_actor_107600_80132AC0;
    work->field_146    = (u8)((u32)arg0->spawnArg1 >> 16);
    work->field_144    = (s32)(arg0->spawnArg1 & 0xF000) >> 12;
    obj->field_1C      = &work->matrix_20;
    obj->field_20      = &work->matrix_0;
    enemy->field_50    = &D_actor_107600_80134F84;
    coord->sub         = &Gfx_ViewCoord;
    enemy->field_4     = &((TmdObject*)arg0->extra)->field_8->coord;
    enemy->field_48    = 0;
    if (work->field_144 != 2) {
        /* retail passes a 0 the resident definition ignores */
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
    }
    work->field_48 = coord->coord.t[0];
    work->field_4A = coord->coord.t[1];
    work->field_4C = coord->coord.t[2];
    work->yaw      = -0x400;
    if (work->field_144 == 1) {
        work->roll += 0x800;
    }
    arg0->state++;
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    func_actor_107600_80132DF0(enemy, arg0->spawnArg1 & 0xF,
                               work->field_144 | (((u32)arg0->spawnArg1 >> 16) & 0x2000));
    *scratch = (u8*)*scratch + 0x10;
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80132160);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80132514);

void func_actor_107600_801328CC(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_107600_80131E24;
    sp.funcs[arg0->state](arg0);
}

/// Update state of the `D_actor_107600_80131E24` table, switched on the scene
/// mode `D_801153F4`. Mode 0 runs the `field_13E` sub-state, copies the yaw and
/// roll onto the model root, rebuilds its rotation and scales `coord.m[1][1]`
/// by the `field_14B` percent; modes 0 and 1 then refresh the colour and show
/// the model, and mode 2 hides it.
void func_actor_107600_80132930(Task* arg0)
{
    TmdObject*       ext      = arg0->extra;
    GpCoordPose*     coord    = (GpCoordPose*)ext->field_8;
    Actor107600Work* work     = (Actor107600Work*)arg0->idMap;
    TaskFunc         funcs[2] = { (TaskFunc)func_actor_107600_80132CB8, func_actor_107600_80132CD4 };
    TmdObject*       obj;

    obj = ext;
    switch (D_801153F4) {
        case 0:
            funcs[(s16)work->field_13E](arg0);
            coord->field_46 = work->yaw;
            coord->field_48 = work->roll;
            coord->flg      = 0;
            func_actor_107600_80132B7C(arg0);
            coord->coord.m[1][1] = work->field_14B * (coord->coord.m[1][1] / 100);
        case 1:
            func_actor_107600_80132B0C(arg0);
            obj->field_C &= ~0x80;
            break;
        case 2:
            ext->field_C |= 0x80;
            break;
    }
}

/// State 2 of the `D_actor_107600_80131E24` table: drops this instance from the
/// spawning gallery's live-target count unless it was spawned already counted
/// (phase 2, the `0x200D` cursor target), then advances to the exit state.
void func_actor_107600_80132A7C(Task* arg0)
{
    Task*            parent;
    Actor107600Work* work;

    parent = arg0->parent;
    work   = (Actor107600Work*)arg0->idMap;
    if (work->field_144 != 2) {
        ((MistShootingGalleryWork*)parent->idMap)->field_0E--;
    }
    arg0->state++;
}

/// Enemy exit callback: releases the shared state slot unless the work block
/// has already reached phase 2, then hands the enemy back for destruction.
void func_actor_107600_80132AC0(Task* arg0)
{
    Actor107600Work* work = (Actor107600Work*)arg0->idMap;

    if (work->field_144 != 2) {
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    }
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off `G_SCRATCH_HEAD` and hands it to
/// `Gp_UpdateActorColor` for the enemy in `Task::spawnArg2` with no blend
/// parameters. Same shape as `func_actor_107600_801349E0`, a different callee.
void func_actor_107600_80132B0C(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;
    void*          obj;

    obj       = arg0->spawnArg2;
    coord     = ((TmdObject*)arg0->extra)->field_8;
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(obj, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Rebuilds the model root's rotation from the work block's three angles: wrap
/// each to 12 bits, build the rotation in a scratch matrix carved off
/// `G_SCRATCH_HEAD`, then copy its 3x3 into the part's `GsCOORDINATE2::coord`.
/// The same body as `ActorsShared80139948`, with the copy left as a call to
/// `func_actor_107600_80132C4C`.
void func_actor_107600_80132B7C(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          m;

    work->pitch              &= 0xFFF;
    work->yaw                &= 0xFFF;
    work->roll               &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ((s16)work->roll, m);
    RotMatrixX((s16)work->pitch, m);
    func_8004BFF8((s16)work->yaw, m);
    func_actor_107600_80132C4C(m, &coord->coord);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
}

/// Copies the 3x3 rotation of the scratch matrix `func_actor_107600_80132B7C`
/// just built into the part's `GsCOORDINATE2::coord`, leaving the translation
/// row of the destination alone.
void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

void func_actor_107600_80132CB8(Actor107600* arg0)
{
    Actor107600Work* work = arg0->field_1C;

    work->field_13E++;
}

/// Runs the `D_actor_107600_80131E34` entry for the work block's `field_144`
/// phase through the same stack-copied table idiom as
/// `func_actor_107600_801328CC`, then advances the model's yaw by 0x20 once the
/// spawn flag at `field_14A` says this instance is rotating.
void func_actor_107600_80132CD4(Task* arg0)
{
    TaskFuncTable3   sp;
    Actor107600Work* work = (Actor107600Work*)arg0->idMap;

    sp = D_actor_107600_80131E34;
    sp.funcs[work->field_144](arg0);
    if (work->field_14A != 0) {
        work->yaw += 0x20;
    }
}

/// Phase in which the actor waits for its parent's first child to raise bit
/// 0x80 of `Task::spawnArg1`: the first pass zeroes the model root's Y
/// translation and resets the `field_14B` scale to 100, then each frame with
/// the bit set shrinks it by 8 until it reaches 0 and the task state advances.
void func_actor_107600_80132D54(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    GpEnemy*         enemy = arg0->spawnArg2;

    switch (work->field_140.step) {
        case 0:
            work->field_140.step++;
            work->field_14B   = 100;
            coord->coord.t[1] = 0;
        case 1:
            if (enemy->task->firstChild->spawnArg1 & 0x80) {
                if (work->field_14B > 0) {
                    work->field_14B -= 8;
                    return;
                }
                work->field_14B = 0;
                arg0->state++;
            }
            break;
    }
}

/// Spawns the next instance of this actor's own `D_actor_107600_80134F94`
/// table (`arg1 + 1` is the index) and adopts it as a child of `arg0`: the new
/// task is reparented and its root coordinate's `sub` link is pointed at
/// `arg0`'s own root coordinate, `Task::spawnArg1` is packed from the two
/// arguments, and the spawned model takes `arg1`'s texture page - dropping the
/// CLUT row to 0 once `arg1` reaches 10 - before the stream is processed twice
/// (one half-buffer per call) and the enemy's light is set to 0x900.
void func_actor_107600_80132DF0(GpEnemy* arg0, s32 arg1, s32 arg2)
{
    GpEnemy*    enemy;
    GpCoordExt* coord;
    TmdObject*  obj;

    enemy = Gp_SpawnEnemyFromTable(&D_actor_107600_80134F94, arg1 + 1, arg2, arg0);
    if (enemy != NULL) {
        Task_Reparent(arg0->task, enemy->task);
        coord                  = (GpCoordExt*)((TmdObject*)enemy->task->extra)->field_8;
        coord->sub             = ((TmdObject*)arg0->task->extra)->field_8;
        enemy->task->spawnArg1 = arg1 | (arg2 << 16);
        obj                    = (TmdObject*)enemy->task->extra;
        obj->field_24          = 0;
        if (arg1 < 10) {
            obj->field_25 = 2;
        } else {
            obj->field_25 = 0;
        }
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
        enemy->field_A = 0x900;
    }
}

/// Spawn state: allocates the work block, hangs the two matrices off the
/// display object's `field_1C` / `field_20`, puts the actor's own light on the
/// enemy and links its node in. Both failure paths - the 0xFF "already dead"
/// marker in byte 0 of `Task::spawnArg1` and a failed allocation - destroy the
/// enemy and return before the exit callback is installed. The variant is the
/// low nibble of `spawnArg1`'s high halfword: it selects the HP from the
/// 16-entry table and its model root later, in `func_actor_107600_80134958`.
void func_actor_107600_80132ED0(Task* arg0)
{
    Actor107600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    u16              hp;
    u32              variant;

    obj     = arg0->extra;
    variant = *(u8*)&arg0->spawnArg1;
    enemy   = arg0->spawnArg2;
    coord   = obj->field_8;
    if (variant == 0xFF || (work = (Actor107600Work*)Mem_Calloc(0x16C, false), arg0->idMap = (TaskIdMap*)work, work == NULL)) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    arg0->exitCallback    = func_actor_107600_80134920;
    work->field_162       = ((u32)arg0->spawnArg1 >> 16) & 0xF;
    obj->field_1C         = &work->matrix_20;
    obj->field_20         = &work->matrix_0;
    enemy->field_50       = &D_actor_107600_80135720;
    enemy->field_54       = (s32)work->rec18;
    work->field_140.coord = ((TmdObject*)arg0->extra)->field_8;
    work->field_144       = 0x140;
    work->field_146       = 2;
    hp                    = D_actor_107600_80135750[arg0->spawnArg1 & 0xF];
    enemy->field_42       = hp;
    enemy->field_40       = hp;
    func_actor_107600_80134958(arg0);
    Gp_LinkNode(&enemy->node);
    enemy->field_4      = &coord->workm;
    enemy->field_1C.vy  = -0x244;
    enemy->field_48     = 0;
    enemy->field_1C.vx  = 0;
    enemy->field_1C.vz  = 0;
    enemy->field_18     = coord;
    enemy->node.field_4 = 1;
    func_actor_107600_80134E5C(coord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    arg0->state += 1;
}

/// Per-frame update switched on the scene mode `D_801153F4`, like
/// `func_actor_107600_80132930`. Mode 0 runs the `field_158` state out of
/// `D_actor_107600_80131E84`, then (below state 7) takes hits, clears the collision records and enters state 9 once the enemy's
/// HP is gone. Afterwards publishes the enemy's slot mask to the gallery and
/// emits one effect per `field_16B` hit, the last one upward when dead.
void func_actor_107600_80133024(Task* arg0)
{
    TaskFuncTable10  sp;
    GpEnemy*         enemy;
    TmdObject*       ext;
    TmdObject*       obj;
    Actor107600Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         v;
    s32              i;

    enemy                  = arg0->spawnArg2;
    ext                    = arg0->extra;
    work                   = (Actor107600Work*)arg0->idMap;
    coord                  = ext->field_8;
    obj                    = ext;
    sp                     = D_actor_107600_80131E84;
    *(s32*)G_SCRATCH_HEAD -= 8;
    v                      = *(SVECTOR**)G_SCRATCH_HEAD;
    switch (D_801153F4) {
        case 0:
            sp.funcs[work->field_158](arg0);
            if (work->field_158 < 7) {
                if (work->field_150 == 0) {
                    func_actor_107600_80133DC4(arg0);
                } else {
                    work->field_150--;
                }
                Gp_ClearRec18Occupied(work->rec18);
                if (enemy->field_40 <= 0) {
                    func_actor_107600_80134B98((Actor107600*)arg0, 9);
                }
            }
        case 1:
            func_actor_107600_801349E0(arg0);
            obj->field_C &= ~0x80;
            break;
        case 2:
            obj->field_C |= 0x80;
            break;
    }
    if (work->field_162 != 2) {
        ((MistShootingGalleryWork*)D_8018E0C4->idMap)->field_1D = Gp_NodeSlotMask(&enemy->node);
    }
    coord->flg = 0;
    func_actor_107600_80134A50(arg0);
    func_actor_107600_80134EF4(arg0);
    for (i = 0; i < work->field_16B; i++) {
        if (i == work->field_16B - 1 && enemy->field_40 <= 0) {
            v->vx = 0;
            v->vy = -0xE0;
            v->vz = 0;
            func_actor_107600_80133FA8(coord, v);
        } else {
            v->vx = D_actor_107600_80135730[(work->field_16A + i) & 7].vx;
            v->vy = D_actor_107600_80135730[(work->field_16A + i) & 7].vy;
            v->vz = 0;
            func_actor_107600_80134248(coord, v);
        }
    }
    *(s32*)G_SCRATCH_HEAD += 8;
}

/// Sub-state machine in `field_15A`: once `Task::spawnArg1` bit 0x10 is set,
/// grows the `field_168`/`field_169` scales by 0x20 up to 100, plays a cue and
/// eases `field_50` down, alternates `field_50` for four frames and raises bit
/// 0x20. From then on, while bit 0x20000000 is set, `field_166` counts frames:
/// at 120 it switches the light mode, at 210 it spawns an effect on the
/// `Game_GetPtrSlot(3)` actor's fifth coordinate and updates that actor.
void func_actor_107600_801332D4(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GpEnemy*         enemy = arg0->spawnArg2;
    GpActorWork*     player;
    GameActor*       actor;
    s32              pan;
    s32              flags;
    s16              v;

    if ((s16)func_actor_107600_80134BAC((Actor107600*)arg0) != 0) {
        return;
    }
    switch (work->field_15A) {
        case 0:
            if (!(arg0->spawnArg1 & 0x10)) {
                return;
            }
            work->field_15A++;
        case 1:
            if (work->field_168 < 100) {
                work->field_168 += 0x20;
                return;
            }
            work->field_15A++;
        case 2:
            if (work->field_169 < 100) {
                work->field_169 += 0x20;
                return;
            }
            {
                GpObj38* o = (GpObj38*)((TmdObject*)arg0->extra)->field_8;
                s32      p;
                work->field_15A++;
                p = (s8)Gp_GetObjPan(o);
                SndEvt_EnqueueType6(0x51140007, p, (s8)Gp_GetObjDepth(o));
            }
        case 3: {
            u16 w = work->field_50;
            if ((u16)(w - 1) < 0x400) {
                work->field_50 = w - ((0x420 - (s16)w) >> 2);
                return;
            }
        }
            work->field_154 = 0;
            work->field_15A++;
            return;
        case 4:
            v               = work->field_154 + 1;
            work->field_154 = v;
            if (v & 1) {
                work->field_50 = ((v << 16) >> 13) - 0x38;
            } else {
                work->field_50 = 0;
                if ((s16)work->field_154 >= 4) {
                    work->field_15A++;
                    arg0->spawnArg1 |= 0x20;
                    Gp_SetLightMode((GpObj4C*)enemy, 0);
                    enemy->node.field_4 = 4;
                    work->obj.flags    |= 0x8000;
                }
            }
        case 5:
            flags = arg0->spawnArg1;
            if (flags & 0x40) {
                func_actor_107600_80134B98((Actor107600*)arg0, 7);
                return;
            }
            if (!(flags & 0x20000000)) {
                return;
            }
            work->field_166++;
            if ((s16)work->field_166 == 120) {
                GpObj38* o = (GpObj38*)((TmdObject*)arg0->extra)->field_8;
                s32      p;
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                p = (s8)Gp_GetObjPan(o);
                SndEvt_EnqueueType6(0x51140013, p, (s8)Gp_GetObjDepth(o));
            } else if ((s16)work->field_166 == 210) {
                GpObj38* c;
                s32      p;
                player          = Game_GetPtrSlot(3);
                c               = (GpObj38*)&player->extra->field_8[4];
                actor           = player->actor;
                work->field_166 = 0;
                Gp_SetLightMode((GpObj4C*)enemy, 0);
                Gp_SpawnEff(0x601BD, (GsCOORDINATE2*)c, 0, NULL);
                p = (s8)Gp_GetObjPan(c);
                SndEvt_EnqueueType6(0x5114000E, p, (s8)Gp_GetObjDepth(c));
                if (actor->field_954 != 1) {
                    if (D_80073BA0 < 11) {
                        ((MistShootingGalleryWork*)D_8018E0C4->idMap)->field_22 = 1;
                        actor->field_96E                                        = 0;
                    } else {
                        actor->field_96E = 10;
                    }
                    actor->field_96C = 1;
                    actor->field_972 = 5;
                    func_8010A9D0(player);
                    pan = (s8)Gp_GetObjPan(c);
                    SndEvt_EnqueueType6(6, pan, (s8)Gp_GetObjDepth(c));
                }
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80133668);

/// Death sequence sub-state machine in `field_15A`: unlinks the enemy node and
/// waits seven frames, plays the death cue, ramps `field_50` up to 0x400, then
/// shrinks the `field_169`/`field_168` scales by 0x20 until both are <= 20 and
/// raises bit 0x80 of `Task::spawnArg1`.
void func_actor_107600_801337FC(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GpEnemy*         enemy = arg0->spawnArg2;
    GpObj38*         obj;
    s32              pan;

    switch (work->field_15A) {
        case 0:
            work->field_15A++;
            arg0->spawnArg1 |= 0x40;
            work->field_154  = 7;
            Gp_UnlinkNode(&enemy->node);
            enemy->field_54  = 0;
            work->obj.flags &= 0x7FFF;
        case 1:
            work->field_154--;
            if ((s16)work->field_154 <= 0) {
                obj = (GpObj38*)((TmdObject*)arg0->extra)->field_8;
                work->field_15A++;
                work->field_16B = 0;
                work->field_15C = 0;
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                pan = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(0x51140009, pan, (s8)Gp_GetObjDepth(obj));
            }
            break;
        case 2:
            if ((s16)work->field_50 < 0x400) {
                work->field_50 += 0x80;
                return;
            }
            work->field_50 = 0x400;
            work->field_15A++;
        case 3:
            if (work->field_169 > 20) {
                work->field_169 -= 0x20;
                return;
            }
            if (work->field_168 > 20) {
                work->field_168 -= 0x20;
                return;
            }
            work->field_15A++;
            arg0->spawnArg1 |= 0x80;
            break;
        case 4:
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_801339A4);

/// Hit handler: for each collision record tagged 0x2xxxx, stores the hit
/// position, applies `Gp_ComputeDamage` to the enemy's HP, plays the hit sound
/// for the first eight hits and picks a light/heavy reaction in `field_15E`.
void func_actor_107600_80133DC4(Task* arg0)
{
    Actor107600Work* work;
    GpEnemy*         enemy;
    GpObj38*         obj;
    s32              i;
    s16              damage;
    s32              pan;

    work                   = (Actor107600Work*)arg0->idMap;
    enemy                  = arg0->spawnArg2;
    *(s32*)G_SCRATCH_HEAD -= 8;
    work->field_156        = 0;
    if (Gp_FindRec18(work->obj.field_C, 0) != 0) {
        for (i = 0; i < 8; i++) {
            if ((work->rec18[i].field_4 & 0xFFFF0000) == 0x20000) {
                work->field_156                        = 1;
                ((Actor107600HitPos*)&work->pitch)->vx = work->rec18[i].field_10;
                ((Actor107600HitPos*)&work->pitch)->vy = work->rec18[i].field_12;
                ((Actor107600HitPos*)&work->pitch)->vz = work->rec18[i].field_14;
                func_actor_107600_80134D9C(arg0);
                damage          = Gp_ComputeDamage(work->rec18[i].field_4, work->field_14C, 0, 0);
                work->field_150 = Gp_GetIdParam2(work->rec18[i].field_4);
                work->field_160 = damage;
                func_800DA6E8(&enemy->node, damage, 0);
                enemy->field_40 -= damage;
                if (enemy->field_40 <= 0) {
                    enemy->field_40 = 0;
                }
                if (damage > 0) {
                    if (work->field_16B < 8) {
                        obj = (GpObj38*)((TmdObject*)arg0->extra)->field_8;
                        work->field_16B++;
                        pan = (s8)Gp_GetObjPan(obj);
                        SndEvt_EnqueueType6(0x51140008, pan, (s8)Gp_GetObjDepth(obj));
                    }
                    if (damage >= 0x14) {
                        work->field_15E = 2;
                    } else {
                        work->field_15E = 1;
                    }
                } else {
                    work->field_156 = 0;
                }
            }
        }
    }
    Gp_ClearRec18Occupied(work->rec18);
    *(s32*)G_SCRATCH_HEAD += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80133FA8);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80134248);

/// Mode 1 collapses each column of `m` to one weighted value plus a
/// `Display_State.field_14`-driven sine pulse; mode 2 clears the 3x3 part.
void func_actor_107600_801344E8(void* arg0, MATRIX* m, s32 mode)
{
    s32 i;
    s16 v;

    switch (mode) {
        case 0:
            break;
        case 1:
            for (i = 0; i < 3; i++) {
                v          = (m->m[0][i] * 7 + m->m[1][i] * 6 + m->m[2][i] * 3) / 33;
                v         += (s16)(rsin(Display_State.field_14 * 198) + 0x1000);
                m->m[0][i] = v;
                m->m[1][i] = v;
                m->m[2][i] = v;
            }
            break;
        case 2:
            m->m[0][0] = 0;
            m->m[0][1] = 0;
            m->m[0][2] = 0;
            m->m[1][0] = 0;
            m->m[1][1] = 0;
            m->m[1][2] = 0;
            m->m[2][0] = 0;
            m->m[2][1] = 0;
            m->m[2][2] = 0;
            break;
    }
}

/// Recolours the model's light matrix from the `field_4E` mode pair, blending
/// the two remaps by `field_4F` while it counts down; a copy of
/// `Gp_UpdateActorColor`.
void func_actor_107600_80134608(GpEnemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3)
{
    TmdObject*      extra;
    MATRIX*         colorMtx;
    s32             mode;
    u8*             head;
    GpColorScratch* block;
    SVECTOR*        col0;
    SVECTOR*        col1;
    GpMtxCol*       src;
    GpMtxCol*       dst;
    s32             i;
    s32             w0;
    s32             w1;

    extra    = (TmdObject*)arg0->task->extra;
    colorMtx = extra->field_20;
    mode     = arg0->field_4E & 3;
    if ((!(extra->field_C & 0x80) && (extra->field_18 != NULL)) || (Game_Session->field_65 != 1)) {
        {
            void**                   scratch;
            register GpColorScratch* tmp asm("v0");

            scratch  = (void**)G_SCRATCH_HEAD;
            head     = *scratch;
            tmp      = (GpColorScratch*)(head - 0x30);
            block    = tmp;
            *scratch = tmp;
        }
        func_800D7A9C(extra, arg1, 0, 3);
        if ((s8)arg0->field_4F <= 0) {
            func_actor_107600_801344E8(arg0, colorMtx, mode);
        } else {
            block->mtx.m[0][0] = colorMtx->m[0][0];
            block->mtx.m[0][1] = colorMtx->m[0][1];
            block->mtx.m[0][2] = colorMtx->m[0][2];
            block->mtx.m[1][0] = colorMtx->m[1][0];
            block->mtx.m[1][1] = colorMtx->m[1][1];
            block->mtx.m[1][2] = colorMtx->m[1][2];
            block->mtx.m[2][0] = colorMtx->m[2][0];
            block->mtx.m[2][1] = colorMtx->m[2][1];
            block->mtx.m[2][2] = colorMtx->m[2][2];
            func_actor_107600_801344E8(arg0, colorMtx, mode);
            func_actor_107600_801344E8(arg0, &block->mtx, (arg0->field_4E >> 2) & 3);
            i    = 0;
            col0 = (SVECTOR*)(head - 0x10);
            col1 = (SVECTOR*)(head - 8);
            src  = (GpMtxCol*)colorMtx;
            w0   = (s8)arg0->field_4F << 8;
            dst  = (GpMtxCol*)block;
            w1   = 0x1000 - w0;
            do {
                block->col0.vx = src->x;
                TOUCH_REG(src);
                block->col0.vy = src->y;
                TOUCH_REG(src);
                block->col0.vz = src->z;
                block->col1.vx = dst->x;
                TOUCH_REG(dst);
                block->col1.vy = dst->y;
                TOUCH_REG(dst);
                block->col1.vz = dst->z;
                gte_lddp(w1);
                gte_ldsv(col0);
                gte_gpf12_real();
                gte_lddp(w0);
                gte_ldsv(col1);
                gte_gpl12_real();
                gte_stsv(col0);
                src->x = block->col0.vx;
                dst    = (GpMtxCol*)&dst->_0;
                src->y = block->col0.vy;
                i++;
                src->z = block->col0.vz;
                src    = (GpMtxCol*)&src->_0;
            } while (i < 3);
            if (D_801153F4 == 0) {
                arg0->field_4F--;
            }
        }
        *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
    }
}

void func_actor_107600_801348A0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_107600_80131E74;
    sp.funcs[arg0->state](arg0);
}

void func_actor_107600_80134904(Task* arg0)
{
    TmdObject* obj = arg0->extra;

    obj->field_C |= 0x80;
}

void func_actor_107600_80134920(Task* arg0)
{
    Gp_UnlinkObj(&((Actor107600Work*)arg0->idMap)->obj);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Links this actor's display node the way `func_8010C980` does for the
/// gameplay objects: the node's collision table is the `GpRec18` run at
/// `work->rec18` (count 8), and its `field_1C` payload is the spawn variant's
/// height, 0x220 for the `field_162 == 1` variant and 0x190 otherwise.
void func_actor_107600_80134958(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    GpRec18*         rec   = work->rec18;

    work->obj.field_8  = coord;
    work->obj.field_C  = rec;
    work->obj.field_10 = 0;
    work->obj.field_12 = -0x250;
    work->obj.field_14 = 0;
    work->obj.field_18 = 0x3004C;
    work->obj.field_1C = (work->field_162 == 1) ? 0x220 : 0x190;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
}

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off `G_SCRATCH_HEAD` and hands it to
/// `func_actor_107600_80134608` with no blend parameters.
void func_actor_107600_801349E0(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;
    void*          obj;

    obj       = arg0->spawnArg2;
    coord     = ((TmdObject*)arg0->extra)->field_8;
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    func_actor_107600_80134608(obj, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Builds a second rotation from the work block's angle trio at +0x50: wrap
/// each to 12 bits, lay an unscaled `MATRIX` down at the scratchpad head, apply
/// `Gfx_RotMatrixZ`/`X`/`Y` to it in that order, copy its 3x3 into the model's
/// `GsCOORDINATE2::coord` through `func_actor_107600_80134B2C`, and hand the
/// scratch block back.
void func_actor_107600_80134A50(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          m;

    work->field_50           &= 0xFFF;
    work->field_52           &= 0xFFF;
    work->field_54           &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    Gfx_RotMatrixZ(m, (s16)work->field_54, 0);
    Gfx_RotMatrixX(m, (s16)work->field_50, 0);
    Gfx_RotMatrixY(m, (s16)work->field_52, 0);
    func_actor_107600_80134B2C(m, &coord->coord);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80134B2C);

void func_actor_107600_80134B98(Actor107600* arg0, s16 arg1)
{
    Actor107600Work* work = arg0->field_1C;

    work->field_158 = arg1;
    work->field_15A = 0;
}
