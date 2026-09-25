#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actors_shared_80136574.h"
#include "actors/actors_shared_8013391c.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

/// Work block of the task this enemy hangs off -- the part spawn reaches it as
/// `task->parent->work`, and the part teardown reads the sound id `field_31C`
/// from it. `field_334` is the enemy's sub-state index: it selects the spawn
/// position in `D_actor_105300_80133A20` and also which of the two per-enemy
/// death flags the spawn raises.
///
/// The pose half is what the per-frame handlers drive: `field_32C` is the
/// animation sub-state `func_actor_105300_8013222C` dispatches on, `field_328`
/// the row of the clip table that state walks, `field_32A` the countdown the
/// LCG reseeds and `field_320` / `field_324` the pose the pose tick
/// `func_actor_105300_80133610` queues and the frames it has counted for it.
/// `field_2FC` is the local coordinate matrix that handler copies into the
/// model's own coordinate each frame. `field_320` / `field_324` are unsigned
/// here: every signed read of them casts at the use.
typedef struct Actor05300Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[10];
    /* 0x1A4 */ GpAnimPose poses[10];
    /* 0x244 */ MATRIX     field_244;
    /* 0x264 */ MATRIX     field_264;
    /* 0x284 */ GpObj      node0;
    /* 0x2A4 */ GpObj      node1;
    /* 0x2C4 */ GpRec18    rec18[2];
    /* 0x2F4 */ GpEffArg   field_2F4;
    /* 0x2FC */ MATRIX     field_2FC;
    /* 0x31C */ s32        field_31C;
    /* 0x320 */ u16        field_320;
    /* 0x322 */ s16        field_322;
    /* 0x324 */ u16        field_324;
    /* 0x326 */ u16        field_326;
    /* 0x328 */ u16        field_328;
    /* 0x32A */ u16        field_32A;
    /* 0x32C */ u16        field_32C;
    /* 0x32E */ u16        field_32E;
    /* 0x330 */ u16        field_330;
    /* 0x332 */ s16        field_332;
    /* 0x334 */ s16        field_334;
    /* 0x336 */ s16        field_336;
    /* 0x338 */ s16        field_338;
    /* 0x33A */ s16        field_33A;
    /* 0x33C */ s16        field_33C;
    /* 0x33E */ s16        field_33E;
} Actor05300Work;
STATIC_ASSERT_SIZEOF(Actor05300Work, 0x340);

/// One row of the two clip/scale tables (`D_actor_105300_8013D3E0` for sub-
/// state 0, `D_actor_105300_8013D3EC` for 1) the animation schedule walks by
/// `Actor05300Work::field_328`. A zero `field_0` advances the row; a non-zero
/// one ends the clip and reseeds the countdown, so each table's last row is
/// its terminator. `field_2` is the scale that row hands
/// `func_actor_105300_801336D4`.
typedef struct Actor05300Clip {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ u16 field_2;
} Actor05300Clip;
STATIC_ASSERT_SIZEOF(Actor05300Clip, 0x4);

/// 0x48-byte part object the spawn allocates with `memCalloc` and parks in
/// `Task::work`. It leads with the `GpObj` list node linked into
/// `Gp_ObjLists[2]` -- and the one the part teardown hands back to
/// `Gp_UnlinkObj` -- so `obj.ctx.recs` is the single-entry `GpRec18` collision
/// table at 0x20. `field_38` holds the same coordinate `obj.coord` points at,
/// and `field_46` is the sub-state the teardown reads back to pick its death
/// flag.
typedef struct Actor05300Part {
    /* 0x00 */ GpObj    obj;
    /* 0x20 */ GpRec18  rec18[1];
    /* 0x38 */ GpEffArg field_38; // record this part's death effect is spawned with
    /* 0x40 */ s16      field_40;
    /* 0x42 */ u16      field_42;
    /* 0x44 */ s16      field_44;
    /* 0x46 */ s16      field_46;
} Actor05300Part;
STATIC_ASSERT_SIZEOF(Actor05300Part, 0x48);

/// The 0x18-byte block the hit handler pushes on the scratchpad stack at
/// `0x1F8003FC`: the player-to-enemy delta and the effect offset it hands
/// `Gp_SpawnEff` / `func_800FDB18`.
typedef struct Actor05300Scratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR ofs;
} Actor05300Scratch;
STATIC_ASSERT_SIZEOF(Actor05300Scratch, 0x18);

/// Spawn position copied into a coordinate's translation, one entry per
/// `Actor05300Work::field_334` sub-state.
typedef struct Actor05300SpawnPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor05300SpawnPos;
STATIC_ASSERT_SIZEOF(Actor05300SpawnPos, 0x6);

/// One row of the per-area sound table `D_actor_105300_8013D3C4`, indexed by
/// `GameSession::at4.loc.view`. `field_0` and `field_2` are the two s8 parameters
/// `SndEvt_EnqueueTypeA` is handed with the work block's sound id.
typedef struct Actor05300SndRow {
    /* 0x0 */ s8 field_0;
    /* 0x1 */ s8 pad_1;
    /* 0x2 */ s8 field_2;
    /* 0x3 */ s8 pad_3;
} Actor05300SndRow;
STATIC_ASSERT_SIZEOF(Actor05300SndRow, 0x4);

extern GpPairSrcE         D_actor_105300_8013D3A0;
extern Actor05300SpawnPos D_actor_105300_80133A20[2];
extern Actor05300Clip     D_actor_105300_8013D3E0[];
extern Actor05300SndRow   D_actor_105300_8013D3C4[];
extern u32                D_actor_105300_8013D3BC;
extern u8                 D_801153F4;
extern s32                D_actor_105300_8013D3B0[];
extern SVECTOR            D_actor_105300_80133A40[];
extern s32                D_actor_105300_8013D3B4;
extern GpMsgEntry         D_actor_105300_80133A00[];
extern SVECTOR            D_actor_105300_80133A30[2];
extern GpPairSrcE         D_actor_105300_8013D390;
extern u16                D_actor_105300_8013D394[];
extern u32                D_actor_105300_8013D3C0;
extern GpAnimSet*         D_actor_105300_8013D414[];
extern TaskDesc           D_actor_105300_8013D3FC[2];

void func_8017E524(s32 arg0);
void func_8017FD88(s32 arg0);

extern s8 D_80115416;

void func_800B4114(Actor05300Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern Actor05300Clip D_actor_105300_8013D3EC[];
extern s16            D_actor_105300_80133A18[];
extern s16            D_actor_105300_80133A2C[];

void func_actor_105300_80133530(Task* arg0);
void func_actor_105300_801335B8(Task* arg0);
void func_actor_105300_80133610(Task* arg0);
void func_actor_105300_801336D4(Task* arg0, MATRIX* arg1, s16 arg2, s32 arg3);
void func_actor_105300_80133838(GpEnemy* arg0, Task* arg1);

/// Hit handler of the main body, the first step of the tick. After the
/// cooldown `field_332` has run out, each of the two contact records the
/// player's attack claimed deals damage by distance: a tenth of it while the
/// part object is alive (`field_336` clear), in which case the body cannot
/// drop below 1 hit point, otherwise the full amount, quadrupled on a critical
/// roll. A surviving body enters its hit reaction (idle state 1, pose 2); a
/// killed one moves the task to its death handler, waiting in death state 3
/// with pose 3. An attack id differing from the previous record's spawns its
/// hit effect; every record restarts the cooldown from the id's parameter 2
/// and plays the hit sound.
void func_actor_105300_80131E3C(Task* arg0)
{
    Actor05300Scratch* scr;
    Actor05300Work*    work;
    GpEnemy*           enemy;
    GsCOORDINATE2*     coord;
    s32                damage;
    s32                lastId;
    s32                val;
    s32                snd;
    s32                i;

    scr    = --*(Actor05300Scratch**)0x1F8003FC;
    coord  = ((TmdObject*)arg0->extra)->coords;
    work   = arg0->work;
    enemy  = arg0->spawnArg2;
    lastId = 0;
    if (work->field_332 != 0) {
        work->field_332--;
        if ((work->field_332 << 0x10) <= 0) {
            work->field_332 = 0;
        }
        if (work->field_332 != 0) {
            goto end;
        }
    }
    for (i = 0; i < 2; i++) {
        if ((work->rec18[i].key & 0xFFFF0000) != 0x20000) {
            continue;
        }
        scr->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
        scr->delta.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
        scr->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        damage        = Gp_ComputeDamage(work->rec18[i].key, SquareRoot0(scr->delta.vx * scr->delta.vx + scr->delta.vy * scr->delta.vy + scr->delta.vz * scr->delta.vz), 0, 0);
        if (work->field_336 == 0) {
            damage /= 10;
        } else if (Gp_RollEnemyChance(enemy, work->rec18[i].key, 0) != 0) {
            damage     *= 4;
            scr->ofs.vx = D_actor_105300_80133A40[work->field_334].vx;
            scr->ofs.vy = D_actor_105300_80133A40[work->field_334].vy;
            scr->ofs.vz = D_actor_105300_80133A40[work->field_334].vz;
            Gp_SpawnEff(0x6009C, coord, 0, &scr->ofs);
        }
        func_800DA6E8(&enemy->node, damage, 0);
        func_800E2C78(enemy, work->rec18[i].key, damage, 0);
        enemy->hp -= damage;
        if (enemy->hp <= 0) {
            if (work->field_336 == 0) {
                enemy->hp = 1;
            } else {
                arg0->state     = 2;
                work->field_32E = 3;
                work->field_330 = 2;
                work->field_338 = 0;
                work->field_320 = 3;
            }
        } else {
            work->field_32C = 1;
            work->field_328 = 0;
            work->field_320 = 2;
        }
        if (lastId != work->rec18[i].key) {
            lastId      = work->rec18[i].key;
            val         = Gp_GetIdParam1(lastId) & 0xFFFF;
            scr->ofs.vx = D_actor_105300_80133A40[work->field_334].vx;
            scr->ofs.vy = D_actor_105300_80133A40[work->field_334].vy;
            scr->ofs.vz = D_actor_105300_80133A40[work->field_334].vz;
            if (val == 3) {
                Gp_SpawnEff(0x6007F, coord, work->field_2F4.spawnArgLo | (work->field_2F4.spawnArgHi << 16), &scr->ofs);
            } else {
                func_800FDB18((u16)val, coord, &scr->ofs, &work->field_2F4);
            }
        }
        val = Gp_GetIdParam2(work->rec18[i].key);
        if (val > 0) {
            work->field_332 = val;
        }
        snd = D_actor_105300_8013D3B0[2] | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8);
        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
end:
    Gp_ClearRec18Occupied(work->rec18);
    *(Actor05300Scratch**)0x1F8003FC += 1;
}

/// Idle schedule of the enemy, one of the steps the tick handler
/// `func_actor_105300_80133468` runs each frame. The sub-state (`field_32C`)
/// picks what it does: state 0 walks `D_actor_105300_8013D3E0` once the
/// countdown `field_32A` has run out, and on that table's terminator row
/// resets the row index, reseeds the countdown from the gameplay LCG and plays
/// the sound id `D_actor_105300_8013D3BC` with the placement number in the
/// high nibble of `GpEnemy::placeKey`; state 1 (entered on a hit) walks
/// `D_actor_105300_8013D3EC` and moves to state 2 on its terminator; state 2
/// returns to pose 1 and state 0 once the pose has run 0x23 frames past its
/// entry of `D_actor_105300_80133A18`. The row's `field_2` is the scale
/// `func_actor_105300_801336D4` applies to the saved coordinate matrix
/// `field_2FC`, 0x1000 when no row was read, and while the session's
/// `viewReady` is 1 the per-view row of `D_actor_105300_8013D3C4` is enqueued
/// with the work block's sound id.
void func_actor_105300_8013222C(Task* arg0)
{
    Actor05300Work* work;
    GsCOORDINATE2*  coord;
    u16             scale;
    s32             pan;
    s32             sndId;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    scale = 0x1000;
    switch ((s16)work->field_32C) {
        case 0:
            if ((s16)work->field_32A <= 0) {
                scale = D_actor_105300_8013D3E0[(s16)work->field_328].field_2;
                if (D_actor_105300_8013D3E0[(s16)work->field_328].field_0 != 0) {
                    work->field_328 = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_32A = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
                    sndId           = D_actor_105300_8013D3BC |
                            ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8);
                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sndId, pan, (s8)gpGetObjDepth(coord));
                } else {
                    work->field_328 = work->field_328 + 1;
                }
            } else {
                work->field_32A = work->field_32A - 1;
            }
            break;
        case 1:
            scale = D_actor_105300_8013D3EC[(s16)work->field_328].field_2;
            if (D_actor_105300_8013D3EC[(s16)work->field_328].field_0 != 0) {
                work->field_328 = 0;
                work->field_32C = 2;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_32A = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
            } else {
                work->field_328 = work->field_328 + 1;
            }
            break;
        case 2:
            if ((s16)work->field_324 >= D_actor_105300_80133A18[(s16)work->field_320] + 0x23) {
                work->field_320 = 1;
                work->field_32C = 0;
            }
            break;
    }
    func_actor_105300_801336D4(arg0, &work->field_2FC, scale, 1);
    if (gGameSession->viewReady == 1) {
        SndEvt_EnqueueTypeA(work->field_31C, D_actor_105300_8013D3C4[gGameSession->at4.loc.view].field_0,
                            D_actor_105300_8013D3C4[gGameSession->at4.loc.view].field_2);
    }
}

/// Death handler of the main task (its state 2). Death state `field_32E` 3 is
/// the wait the hit handler enters when the enemy dies: effects are spawned
/// every fourth frame until message bit 1 moves it to 0. State 0 drops the
/// enemy's lock-on node and both collision objects and starts the sequence;
/// state 1 runs it for 0x78 frames, shrinking the model towards an eighth of
/// its scale while flickering through `D_actor_105300_8013D3EC`, spawning the
/// same two randomly offset effects every fourth frame, setting bit 1 of the
/// model's `field_C` at frame 0x14, spawning effect 0x600A5 at 0x1E and
/// switching the light mode at 0x6E, then ends in state 2. Independently,
/// `field_330` 0 calls `Gp_ReleaseStateF0Add` once with this sub-state's entry
/// of `D_actor_105300_80133A2C` (message bit 2 clears the hold value 2). The
/// pose and colour are ticked every frame, and the enemy is destroyed once the
/// sequence has ended and the release has run.
void func_actor_105300_8013246C(GpEnemy* arg0, Task* arg1)
{
    SVECTOR         ofs;
    VECTOR          pos;
    TmdObject*      obj;
    Actor05300Work* work;
    Actor05300Work* anim;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  tmp;
    Actor05300Clip* clip;
    u16             scale;
    s32             r;
    s8              flag;
    s32             x;
    s32             z;
    s32             x2;
    s32             z2;
    s32             i;
    s32             value;

    obj   = arg1->extra;
    work  = arg1->work;
    coord = obj->coords;
    scale = 0x1000;
    switch (D_801153F4) {
        case 1:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
            return;
        case 2:
            obj->flags = 0x80;
            return;
        case 0:
            break;
    }
    if ((work->field_33A & 1) && (s16)work->field_32E == 3) {
        work->field_32E = 0;
    }
    if ((work->field_33A & 2) && (s16)work->field_330 == 2) {
        work->field_330 = 0;
    }
    switch ((s16)work->field_32E) {
        case 0:
            work->field_326 = 0x1000;
            work->field_2FC = coord->coord;
            arg0->recs      = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->node0);
            Gp_UnlinkObj(&work->node1);
            Gp_SetLightMode(arg0, 1);
            if (work->field_334 == 0) {
                work->field_328 = 0;
                work->field_32C = 0;
                work->field_32E = 1;
                r               = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = r;
            } else {
                work->field_328 = 0;
                work->field_32C = 0;
                work->field_32E = 1;
                r               = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = r;
            }
            flag            = 1;
            work->field_32A = (((u32)r >> 16) & 0xF) + 0xA;
            D_80115416      = flag;
            break;
        case 1:
            if ((s16)work->field_326 > 0x200) {
                work->field_326 -= 0x20;
            }
            switch ((s16)work->field_32C) {
                case 0:
                    work->field_32A--;
                    if ((s16)work->field_32A <= 0) {
                        work->field_32C = 1;
                    }
                    scale = work->field_326;
                    break;
                case 1:
                    clip  = &D_actor_105300_8013D3EC[(s16)work->field_32A];
                    scale = ((s16)work->field_326 * (s16)clip->field_2) >> 12;
                    if (clip->field_0 != 0) {
                        work->field_32C = 0;
                        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                        work->field_32A = ((Gp_LcgState >> 16) & 0xF) + 0xA;
                    } else {
                        work->field_32A++;
                    }
                    break;
            }
            func_actor_105300_801336D4(arg1, &work->field_2FC, scale, 0);
            if (!(work->field_328 & 3)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                x           = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    x = -x;
                }
                ofs.vx      = x;
                ofs.vy      = -0x9C4;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                z           = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    z = -z;
                }
                ofs.vz = z;
                Gp_SpawnEff(0x600E0, coord, 0x400, &ofs);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                x2          = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    x2 = -x2;
                }
                ofs.vx      = x2;
                ofs.vy      = -0x960;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                z2          = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    z2 = -z2;
                }
                ofs.vz = z2;
                Gp_SpawnEff(0x60070, coord, 0x30011600, &ofs);
            }
            work->field_328++;
            if ((s16)work->field_328 == 0x14) {
                obj->flags |= 2;
            }
            if ((s16)work->field_328 == 0x1E) {
                Gp_SpawnEff(0x600A5, coord, 5, NULL);
            }
            if ((s16)work->field_328 == 0x6E) {
                Gp_SetLightMode(arg0, 2);
            }
            if ((s16)work->field_328 >= 0x78) {
                work->field_32E = 2;
            }
            tmp    = ((TmdObject*)arg1->extra)->coords;
            pos.vx = tmp->workm.t[0];
            pos.vy = tmp->workm.t[1];
            pos.vz = tmp->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
            break;
        case 2:
            break;
        case 3:
            arg0->node.flags = 1;
            work->field_328++;
            if (!(work->field_328 & 3)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                x           = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    x = -x;
                }
                ofs.vx      = x;
                ofs.vy      = -0x9C4;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                z           = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    z = -z;
                }
                ofs.vz = z;
                Gp_SpawnEff(0x600E0, coord, 0x400, &ofs);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                x2          = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    x2 = -x2;
                }
                ofs.vx      = x2;
                ofs.vy      = -0x960;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                z2          = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    z2 = -z2;
                }
                ofs.vz = z2;
                Gp_SpawnEff(0x60070, coord, 0x30011600, &ofs);
            }
            break;
    }
    if ((s16)work->field_330 == 0) {
        Gp_ReleaseStateF0Add(arg1, D_actor_105300_80133A2C[work->field_334]);
        work->field_330 = 1;
        Gp_ClearAreaFlag4((GpAreaKey*)&gGameSession->at4);
    }
    anim = arg1->work;
    i    = 1;
    if ((s16)anim->field_320 != anim->field_322) {
        anim->field_322 = anim->field_320;
        anim->field_324 = 0;
        value           = D_actor_105300_80133A18[(s16)anim->field_320];
        for (; i < 10; i++) {
            func_800B4114(anim, i, (s16)anim->field_320, 0, value);
        }
    } else {
        TOUCH_REG(i);
        anim->field_324 += i;
        do {
            Gp_AnimTickIndex(&anim->anim, i);
            i++;
        } while (i < 10);
    }
    tmp    = ((TmdObject*)arg1->extra)->coords;
    pos.vx = tmp->workm.t[0];
    pos.vy = tmp->workm.t[1];
    pos.vz = tmp->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
    if ((s16)work->field_32E == 2 && (s16)work->field_330 == 1) {
        Gp_DestroyEnemy(arg0, arg1);
    }
}

/// Spawn state of the enemy: allocates the 0x48-byte part object, seeds its
/// coordinate's translation from the sub-state's entry in
/// `D_actor_105300_80133A20`, links it into `Gp_ObjLists[2]`, and raises one of
/// the two per-enemy death flags. A failed allocation tears the enemy down
/// instead and leaves the task on this handler; otherwise the task moves to the
/// tick handler (`state` 1).
void func_actor_105300_80132BAC(GpEnemy* arg0, Task* arg1)
{
    TmdObject*      obj;
    Actor05300Work* work;
    Actor05300Part* part;
    GsCOORDINATE2*  coord;
    GpRec18*        rec18;
    s32             flag;
    u16             type;

    obj   = arg1->extra;
    coord = obj->coords;
    work  = (Actor05300Work*)arg1->parent->work;
    part  = memCalloc(0x48, 0);
    if (part == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work        = (TaskIdMap*)part;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = D_actor_105300_80133A20[work->field_334].x;
    coord->coord.t[1] = D_actor_105300_80133A20[work->field_334].y;
    coord->coord.t[2] = D_actor_105300_80133A20[work->field_334].z;
    coord->flg        = 0;
    arg0->field_4     = &coord->coord;
    arg0->field_48    = 0;
    Gp_LinkNode(&arg0->node);
    rec18                     = part->rec18;
    arg0->coord               = coord;
    arg0->bodyPos.vx          = 0;
    arg0->bodyPos.vy          = 0;
    arg0->bodyPos.vz          = 0;
    arg0->param               = &D_actor_105300_8013D3A0;
    arg0->recs                = rec18;
    arg0->hp                  = D_actor_105300_8013D3A0.hpMax;
    part->field_38.spawnArgLo = 0x500;
    part->field_38.coord      = coord;
    part->field_38.spawnArgHi = 2;
    part->obj.coord           = coord;
    part->obj.ctx.recs        = rec18;
    part->obj.pos.vx          = 0;
    part->obj.pos.vy          = 0;
    part->obj.pos.vz          = 0;
    part->obj.key             = ((Actor05300Work*)arg1->parent->work)->node0.key;
    part->obj.radius          = 0xC8;
    part->obj.flags           = 1;
    Gp_LinkObj(2, &part->obj);
    Gp_InitRec18Table(rec18, 1, 0);
    part->obj.flags |= 0x8000;
    type             = (u16)work->field_334;
    part->field_46   = type;
    if ((type << 0x10) == 0) {
        func_8017FD88(1);
        flag = 0x147;
    } else {
        func_8017E524(1);
        flag = 0x148;
    }
    GameFlag_SetNibble(flag, 0);
    arg1->state = 1;
}

void func_actor_105300_80132DAC(GpEnemy* arg0, Task* arg1)
{
    VECTOR*         vec;
    Actor05300Part* part;
    GsCOORDINATE2*  coord;
    s32             damage;
    s32             snd;
    s32             hitTime;

    coord = ((TmdObject*)arg1->extra)->coords;
    part  = (Actor05300Part*)arg1->work;
    switch (D_801153F4) {
        case 1:
            return;
        case 0:
            arg0->node.flags = 8;
            break;
        case 2:
            arg0->node.flags = 1;
            return;
    }
    vec = --*(VECTOR**)0x1F8003FC;
    if (part->field_40 != 0) {
        part->field_40--;
        if (part->field_40 <= 0) {
            part->field_40 = 0;
        }
    }
    if (part->field_44 != 0) {
        part->field_44--;
    }
    if (part->field_40 == 0 && (part->rec18[0].key & 0xFFFF0000) == 0x20000) {
        if (part->rec18[0].key & 0x8000) {
            func_800DA6E8(&arg0->node, 0, 0);
        } else {
            vec->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            vec->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            damage  = Gp_ComputeDamage(part->rec18[0].key, SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz), 0, 0);
            if (Gp_RollEnemyChance(arg0, part->rec18[0].key, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, coord, 0, NULL);
            }
            func_800DA6E8(&arg0->node, damage, 0);
            arg0->hp -= damage;
            if (arg0->hp <= 0) {
                arg1->state                                      = 2;
                part->field_42                                   = 0;
                ((Actor05300Work*)arg1->parent->work)->field_336 = 1;
                Gp_SpawnEff(0x6005C, coord, 0x10002400, NULL);
                Gp_SpawnEff(0x60070, coord, 0x32FF1400, NULL);
                snd  = D_actor_105300_8013D3B4;
                snd |= (arg0->placeKey >> 12) << 8;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else if (damage > 0) {
                if (part->field_44 == 0) {
                    if ((Gp_GetIdParam0(part->rec18[0].key) & 0xFFFF) == 7) {
                        func_800FDB18(3, coord, NULL, &part->field_38);
                    }
                    func_800FDB18(7, coord, NULL, &part->field_38);
                    part->field_44 = 10;
                }
                hitTime = Gp_GetIdParam2(part->rec18[0].key);
                if (hitTime > 0) {
                    part->field_40 = hitTime;
                }
                snd  = D_actor_105300_8013D3B0[0];
                snd |= (arg0->placeKey >> 12) << 8;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
    }
    Gp_ClearRec18Occupied(part->rec18);
    *(VECTOR**)0x1F8003FC += 1;
}

void func_actor_105300_8013310C(GpEnemy* arg0, Task* arg1)
{
    Actor05300Work* work;
    TmdObject*      obj;
    GsCOORDINATE2*  coord;
    GpAreaKey*      sessionKey;
    GpAreaRec*      rec;
    GpAreaPlace*    place;
    TmdObject*      model;
    GpEnemy*        spawned;
    GpAreaKey       key;
    u16             idx;
    s32             sound;
    s32             i;

    obj   = arg1->extra;
    coord = obj->coords;
    work  = memCalloc(0x340, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_264;
    obj->colorMtx  = &work->field_244;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = coord;
    arg0->bodyPos.vx           = D_actor_105300_80133A30[0].vx;
    arg0->bodyPos.vy           = D_actor_105300_80133A30[0].vy;
    arg0->bodyPos.vz           = D_actor_105300_80133A30[0].vz;
    arg0->param                = &D_actor_105300_8013D390;
    arg0->recs                 = work->rec18;
    arg0->hp                   = D_actor_105300_8013D390.hpMax;
    work->field_2F4.coord      = coord;
    work->field_2F4.spawnArgLo = 0x500;
    work->field_2F4.spawnArgHi = 3;
    func_800B3F84(&work->anim, D_actor_105300_8013D414, obj,
                  work->poses, work->slots);
    for (i = 1; i < 0xA; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_326      = 0x1000;
    work->field_334      = 0;
    work->field_2FC      = coord->coord;
    work->field_338      = 1;
    work->field_33C      = D_actor_105300_8013D394[0];
    work->node0.coord    = coord;
    work->node0.ctx.recs = work->rec18;
    work->node0.pos.vx   = 0;
    work->node0.pos.vy   = 0;
    work->node0.pos.vz   = 0;
    work->node0.key      = 0x30035;
    work->node0.radius   = 0x5DC;
    work->node0.flags    = 1;
    Gp_LinkObj(2, &work->node0);
    Gp_InitRec18Table(work->rec18, 2, 0);
    work->node0.flags   |= 0x8000;
    work->node1.coord    = coord;
    work->node1.ctx.recs = work->rec18;
    work->node1.pos.vx   = D_actor_105300_80133A30[0].vx;
    work->node1.pos.vy   = D_actor_105300_80133A30[0].vy;
    work->node1.pos.vz   = D_actor_105300_80133A30[0].vz;
    work->node1.key      = 0x30035;
    work->node1.radius   = 0x12C;
    work->node1.flags    = 1;
    Gp_LinkObj(2, &work->node1);
    work->node1.flags |= 0x8000;
    spawned            = Gp_SpawnEnemyFromTable(D_actor_105300_8013D3FC, 1, 0, arg0);
    model              = spawned->task->extra;
    idx                = arg0->placeKey >> 12;
    sessionKey         = &gGameSession->at4.loc;
    key.stage          = sessionKey->stage;
    key.area           = sessionKey->area;
    key.room           = sessionKey->room;
    key.view           = sessionKey->view;
    Gp_SyncAreaKeyIndex(&key);
    rec          = Gp_GetNestedAreaRec(&key);
    place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    sound           = D_actor_105300_8013D3C0 | ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8);
    work->field_31C = sound;
    SndEvt_EnqueueType6(sound, D_actor_105300_8013D3C4[gGameSession->at4.loc.view].field_0,
                        D_actor_105300_8013D3C4[gGameSession->at4.loc.view].field_2);
    arg1->msgTable = D_actor_105300_80133A00;
    arg1->state    = 1;
}

/// Tick handler of the main task (its state 1), switched on the gameplay mode
/// `D_801153F4`. Mode 1 only updates the colour; mode 2 sets the model's
/// `field_C` to 0x80 and the lock-on node's flags to 1 and stops there. Any
/// other mode runs the frame - mode 0 first clearing the model's `field_C` and
/// setting the node's flags to 8: the hit handler, the idle schedule, the pose
/// tick, the model's coordinate refresh, the colour update and the
/// regeneration step.
void func_actor_105300_80133468(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2* temp_s1;
    TmdObject*     temp_a1;
    s32            state;
    s32            one;

    temp_a1 = arg1->extra;
    temp_s1 = temp_a1->coords;
    state   = D_801153F4;
    one     = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    temp_a1->flags   = 0;
    arg0->node.flags = 8;
    goto default_body;
case1:
    func_actor_105300_801335B8(arg1);
    return;
case2:
    temp_a1->flags   = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    func_actor_105300_80131E3C(arg1);
    func_actor_105300_8013222C(arg1);
    func_actor_105300_80133610(arg1);
    temp_s1->flg = 0;
    Gp_UpdateCoord(temp_s1);
    func_actor_105300_801335B8(arg1);
    func_actor_105300_80133530(arg1);
}

/// Regeneration step: while the part object is alive (`field_336` clear)
/// and the enemy's hit points are below the ceiling `field_33C`, one point
/// comes back every five frames (`field_33E` counts them down), reported
/// through the lock-on node as a damage of -1.
void func_actor_105300_80133530(Task* arg0)
{
    Actor05300Work* work;
    GpEnemy*        enemy;
    s16             timer;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if ((work->field_336 == 0) && (enemy->hp < work->field_33C)) {
        timer           = work->field_33E - 1;
        work->field_33E = timer;
        if ((timer << 0x10) <= 0) {
            enemy->hp = enemy->hp + 1;
            func_800DA6E8(&enemy->node, -1, 0);
            work->field_33E = 5;
        }
    }
}

/// Hands the model's world position (its coordinate's `workm` translation) to
/// `Gp_UpdateActorColor` for the enemy, with no blend parameters.
void func_actor_105300_801335B8(Task* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

/// Pose tick. When the pose asked for (`field_320`) differs from the one the
/// animation slots were last queued for (`field_322`), slots 1-9 are re-queued
/// with it and its entry of `D_actor_105300_80133A18`, and the frame count
/// `field_324` restarts; otherwise every slot is ticked and the count advances
/// by one.
void func_actor_105300_80133610(Task* arg0)
{
    Actor05300Work* work;
    s32             i;
    s32             value;

    work = arg0->work;
    i    = 1;
    if ((s16)work->field_320 != work->field_322) {
        work->field_322 = work->field_320;
        work->field_324 = 0;
        value           = D_actor_105300_80133A18[(s16)work->field_320];
        for (; i < 10; i++) {
            func_800B4114(work, i, (s16)work->field_320, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_324 += i;
        do {
            Gp_AnimTickIndex(&work->anim, i);
            i++;
        } while (i < 10);
    }
}

/// Sets the model's coordinate to `arg1` scaled by `arg2` and marks it for
/// recomputation. The scale is built in a 0x30-byte block borrowed from the
/// scratchpad: an identity rotation is written word-wise and `ScaleMatrix`
/// scales it, on all three axes when `arg3` is non-zero and on Y alone when it
/// is zero.
///
/// The scratchpad head is written twice, from two separate computations of
/// `head - 0x30`. CSE cannot substitute a value that holds no register, so the
/// store keeps the block-local `$v1` while `blk` - which crosses both calls -
/// is copied into `$s0` by `reload_cse_regs`. Folding the two into one
/// variable allocates `blk`'s register for the store as well and loses the
/// copy, the delay-slot fill and the frame layout.
void func_actor_105300_801336D4(Task* arg0, MATRIX* arg1, s16 arg2, s32 arg3)
{
    void*                       head;
    ActorShared80136574Scratch* blk;
    GsCOORDINATE2*              coord;

    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - 0x30;
    blk                     = (ActorShared80136574Scratch*)((u8*)head - 0x30);
    coord                   = ((TmdObject*)arg0->extra)->coords;

    if (arg3 == 0) {
        blk->scale.vx = 0x1000;
        blk->scale.vy = arg2;
        blk->scale.vz = 0x1000;
    } else {
        blk->scale.vx = arg2;
        blk->scale.vy = arg2;
        blk->scale.vz = arg2;
    }

    coord->coord = *arg1;

    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;

    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->mat.mat);
    coord->flg              = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
}

/// State handlers of the part task, indexed by `Task::state`: spawn, per-frame
/// hit reaction and teardown.
const GpEnemyTaskFuncTable3 D_actor_105300_80131E24 = {
    {
        func_actor_105300_80132BAC,
        func_actor_105300_80132DAC,
        func_actor_105300_80133838,
    },
};

void func_actor_105300_801337DC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105300_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Teardown handler of the part task (its state 2), ticking only while the
/// gameplay mode `D_801153F4` is 0. The first tick unlinks the enemy's lock-on
/// node and the part's collision object, drops the enemy's `recs`, sends the
/// main task's sound id `field_31C` a type-7 event, and undoes what the spawn
/// did for this sub-state (chosen by `field_46`): the same room call with 0
/// instead of 1, and game flag 0x147 or 0x148 set to 1 where the spawn set it
/// to 0. The enemy is destroyed once the counter `field_42` reaches 0x3D.
void func_actor_105300_80133838(GpEnemy* arg0, Task* arg1)
{
    Actor05300Part* part;
    Actor05300Work* parentWork;
    u16             timer;

    part       = (Actor05300Part*)arg1->work;
    parentWork = (Actor05300Work*)arg1->parent->work;
    if (D_801153F4 == 0) {
        timer          = part->field_42 + 1;
        part->field_42 = timer;
        if ((s16)timer == 1) {
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            arg0->recs = 0;
            SndEvt_EnqueueType7(parentWork->field_31C, 1);
            if (part->field_46 == 0) {
                func_8017FD88(0);
                GameFlag_SetNibble(0x147, 1);
            } else {
                func_8017E524(0);
                GameFlag_SetNibble(0x148, 1);
            }
        }
        if ((s16)part->field_42 >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
    }
}

/// Message handler in the main task's message table: ORs the bit the payload's
/// selector names into the work block's `field_33A` (1, 2 or both for
/// selector 3; 0 is a no-op). Bit 1 releases the death handler from its wait,
/// bit 2 lets it run its `Gp_ReleaseStateF0Add` call.
s32 func_actor_105300_8013391C(Task* task, s32 msgId, ActorsShared8013391cMsg* msg)
{
    Actor05300Work* work;

    work = (Actor05300Work*)task->work;
    switch (msg->field_2) {
        case 0:
            break;
        case 1:
            work->field_33A |= 1;
            break;
        case 2:
            work->field_33A |= 2;
            break;
        case 3:
            work->field_33A |= 3;
            break;
    }
    return 0;
}

s16 Actor05300_Fn01B70(Task* arg0)
{
    return ((Actor05300Work*)arg0->work)->field_338;
}

/// State handlers of the main task, indexed by `Task::state`: spawn, per-frame
/// tick and death.
const GpEnemyTaskFuncTable3 D_actor_105300_80131E30 = {
    {
        func_actor_105300_8013310C,
        func_actor_105300_80133468,
        func_actor_105300_8013246C,
    },
};

void func_actor_105300_801339A4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105300_80131E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
