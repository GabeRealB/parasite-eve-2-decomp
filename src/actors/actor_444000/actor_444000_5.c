#include "common.h"

#include "actors/actor_444000.h"
#include "actors/actors_shared_80133de4.h"
#include "actors/actors_shared_80133f64.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include <psyq/abs.h>
#include <psyq/inline_c.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// GPF with `sf = 1`, which `psyq/inline_c.h` spells without the COP2 prefix
/// the retail build used. Same form as `src/pe/energyball/energyball.c`.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// `MVMVA` with `sf = 1` reading the rotation matrix and V0 -- the operation
/// `gte_RotTrans` performs -- spelled with the COP2 prefix the retail build
/// used, which `psyq/inline_c.h` omits.
#define gte_rt_real() __asm__ volatile("nop; nop; .word 0x4A480012")

extern s16 D_actor_444000_80144A68;
extern s16 D_actor_444000_80144A70;
extern s32 Gp_LcgState;

extern s8         D_8007218A;
extern u8         D_80073BA9;
extern u8         D_801153F4;
extern GpAnimSet* D_actor_444000_80161694[];
extern GpAnimBlk* Gp_PlayerAnimBlkTbl[];
extern u16        Gp_WeaponIdBase[];

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_actor_444000_80133010(Actor444000* task);
void func_actor_444000_80133C58(Actor444000* task, s16 arg1);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Per-animation reset argument, a `[?][0x2D]` table of `field_7B3` indexed by
/// the id that was playing before the switch.
extern s8 D_actor_444000_80160C5C[][0x2D];

/// Reseed every slot of the three even animation members from `field_7B3` when
/// the id it names differs from the latched `field_7B2`, then latch it. Each
/// slot also has its `field_9` seeded from `field_7B6`, and the reset argument
/// comes from the `[field_7B2][field_7B3]` transition table.
void func_actor_444000_80134040(Actor444000* arg0)
{
    Actor444000Work* work = arg0->field_1C;
    s32              i;

    if (work->field_7B2 != work->field_7B3) {
        for (i = 1; i < 8; i++) {
            work->slots0[i].field_9 = work->field_7B6;
            func_800B4114(&work->anim0, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots2[i].field_9 = work->field_7B6;
            func_800B4114(&work->anim2, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots4[i].field_9 = work->field_7B6;
            func_800B4114(&work->anim4, i, work->field_7B3, 0,
                          D_actor_444000_80160C5C[work->field_7B2][work->field_7B3]);
        }
        work->field_7B2 = work->field_7B3;
    }
}

/// Advance every animation slot of the three context pairs and write the blended
/// pose out of each pair's even member. Both members of a pair are ticked with
/// the same slot index; the odd member's `field_9` is seeded from `field_7BE`
/// and the even member's from `field_7B6 - 3`. `field_7C0` is the copy weight,
/// with `0x1000 - field_7C0` as its complement.
void func_actor_444000_801341C4(Actor444000* arg0)
{
    GpAnimPose       pose0;
    GpAnimPose       pose1;
    Actor444000Work* work     = arg0->field_1C;
    s32              blend    = work->field_7C0;
    s32              invBlend = 0x1000 - blend;
    s16              i;

    for (i = 1; i < 8; i++) {
        if (i < 11) {
            work->slots1[i].field_9 = work->field_7BE;
            work->slots0[i].field_9 = work->field_7B6 - 3;
            func_800B3448(&work->anim0, i, (s32)&pose0, 0);
            func_800B3448(&work->anim1, i, (s32)&pose1, 0);
            Gp_AnimWritePoseCopy(&work->anim0, i, &pose0, &pose1, blend, invBlend);
        } else {
            work->slots0[i].field_9 = work->field_7B6 - 3;
            Gp_AnimTickIndex(&work->anim0, i);
        }
    }

    for (i = 0; i < 4; i++) {
        work->slots3[i].field_9 = work->field_7BE;
        work->slots2[i].field_9 = work->field_7B6 - 3;
        func_800B3448(&work->anim2, i, (s32)&pose0, 0);
        func_800B3448(&work->anim3, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim2, i, &pose0, &pose1, blend, invBlend);
    }

    for (i = 0; i < 4; i++) {
        work->slots5[i].field_9 = work->field_7BE;
        work->slots4[i].field_9 = work->field_7B6 - 3;
        func_800B3448(&work->anim4, i, (s32)&pose0, 0);
        func_800B3448(&work->anim5, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim4, i, &pose0, &pose1, blend, invBlend);
    }
}

/// Per-frame animation step. `field_7B0` 1 re-seeds the block through
/// `func_actor_444000_80134040`, 2 resets every slot of the three even members
/// from `field_7B3` outright; either way the block is armed (`field_7B0` 3, the
/// frame counter and the 0x20-byte scratch at `field_7D0` cleared). Then the
/// slots are advanced: plainly while `field_7B1` is clear, otherwise through the
/// blended path, which clears `field_7B1` again once the first pair's slot 1
/// reports done. The three trailing flags run the shared reaction helpers.
void func_actor_444000_8013441C(Actor444000* arg0)
{
    Actor444000Work* work = arg0->field_1C;
    Actor444000Work* w;
    s32              i;

    if (work->field_7B0 == 1) {
        func_actor_444000_80134040(arg0);
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    } else if (work->field_7B0 == 2) {
        w = arg0->field_1C;
        for (i = 1; i < 8; i++) {
            w->slots0[i].field_9 = w->field_7B6;
            Gp_AnimResetSlot(&w->anim0, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].field_9 = w->field_7B6;
            Gp_AnimResetSlot(&w->anim2, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].field_9 = w->field_7B6;
            Gp_AnimResetSlot(&w->anim4, i, w->field_7B3);
        }
        w->field_7B2    = w->field_7B3;
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    }

    if (work->field_7BA == 2) {
        ActorsShared80133f64((Task*)arg0);
        work->field_7BA = 3;
    }

    work->field_7B4++;

    if (work->field_7B1 == 0) {
        w = arg0->field_1C;
        for (i = 1; i < 8; i++) {
            w->slots0[i].field_9 = w->field_7B6;
            Gp_AnimTickIndex(&w->anim0, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].field_9 = w->field_7B6;
            Gp_AnimTickIndex(&w->anim2, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].field_9 = w->field_7B6;
            Gp_AnimTickIndex(&w->anim4, i);
        }
    } else {
        func_actor_444000_801341C4(arg0);
        if (work->slots1[1].field_10 & 1) {
            work->field_7B1 = 0;
        }
    }

    if (work->field_EF4 != 0) {
        ActorsShared80133de4((Task*)arg0, work->field_EFE);
    }
    if (work->field_EF6 != 0) {
        func_actor_444000_80133C58(arg0, work->field_7C4);
    }
    if (work->field_EF8 != 0) {
        func_actor_444000_80133010(arg0);
    }
}

/// Spawn the hit effect for an attack that landed on `coord`: the effect kind
/// is the attack's `Gp_GetIdParam1`, and its rotation comes from the attack's
/// `Gp_GetIdParam0` - categories 2, 4, 6 and 7 get one fixed tilt, everything
/// else picks one of three at random. The rotation and `func_800FDB18`'s
/// argument block live in a 0x10-byte scratchpad frame.
void func_actor_444000_80134688(GsCOORDINATE2* coord, s32 id)
{
    Actor444000EffScratch* sc = (Actor444000EffScratch*)(SCRATCH_SP -= sizeof(Actor444000EffScratch));

    sc->eff.field_4 = 0x500;
    sc->eff.field_0 = coord;
    sc->eff.field_6 = 3;

    switch (Gp_GetIdParam0(id) & 0xFFFF) {
        case 2:
        case 4:
        case 6:
        case 7:
            sc->rot.vx = 0;
            sc->rot.vy = -0x190;
            sc->rot.vz = 0x258;
            func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
            break;
        case 0:
        case 1:
        case 3:
        case 5:
        case 8:
        case 9:
        default:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            switch ((u16)(((u32)Gp_LcgState >> 16) % 3U)) {
                case 0:
                    sc->rot.vy = 0;
                    sc->rot.vx = 0;
                    sc->rot.vz = 0x384;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
                case 1:
                    sc->rot.vx = 0x258;
                    sc->rot.vy = -0xC8;
                    sc->rot.vz = 0x2BC;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
                case 2:
                    sc->rot.vx = -0x12C;
                    sc->rot.vy = -0x320;
                    sc->rot.vz = 0x320;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
            }
            break;
    }

    SCRATCH_SP += sizeof(Actor444000EffScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013482C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80135448);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_801371E8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80137594);

/// Flight step of the seized player's model: carry it along the model's own
/// forward axis until it lands. `field_1A8` (the dispatcher's state-changed
/// flag) re-arms the step counter, the ground marker and the first display
/// node on the frame the state starts.
///
/// While the game is running (`D_801153F4` clear) the model falls 0xA a step,
/// column 2 of its coordinate is normalised into a scratchpad `SVECTOR` and
/// scaled by 0x89/0x1000 through the GTE's GPF, and that is the per-step
/// translation added to the coordinate; past step 0x29 the height is pinned to
/// -0x3E8 instead. The marker grows 0x60 a step and is drawn under the work
/// block's own coordinate, which is parented to `Gfx_ViewCoord` and tracks the
/// model. After 0x35 steps the display node is handed back and the task steps
/// on. Paused (`D_801153F4` set) only the coordinate is refreshed, and the
/// marker is skipped while the host actor sits in state 6.
///
/// Bails out -- unlinking the display node and stepping the task on -- when the
/// overlay is shutting down or the host actor has left the grab states.
void func_actor_444000_8013799C(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work;
    Actor444000Work*     host;
    GpEnemy*             owner;
    u8*                  head;
    SVECTOR*             dir;
    /// Second live alias of `dir`: the GTE operand is kept in its own register
    /// for the whole function, which is what gives this function its seventh
    /// callee-saved slot.
    SVECTOR* gteDir;

    work  = task->field_1C;
    owner = task->parent->spawnArg2;
    host  = owner->task->idMap;

    if (D_actor_444000_80144A68 == 1 || (s16)host->field_0 == 0x10 || (s16)host->field_0 == 5 ||
        (s16)host->field_0 == 0xC || (s16)host->field_0 == 0x12) {
        task->state++;
        Gp_UnlinkObj(&work->obj0);
        return;
    }

    head                       = *(u8**)G_SCRATCH_HEAD;
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    *(SVECTOR**)G_SCRATCH_HEAD = dir;
    gteDir                     = dir;

    if (work->field_1A8 != 0) {
        work->field_1AC    = 0;
        work->field_1B0    = 0x400;
        work->field_1A8    = 0;
        work->rec0.field_4 = 0;
        work->obj0.flags  |= 0x8000;
    }

    if (D_801153F4 == 0) {
        work->field_1AC++;
        task->extra->field_8->coord.t[1] += 0xA;

        Gfx_MatrixCol2(&task->extra->field_8->coord, dir);
        VectorNormalSS(dir, dir);
        gte_lddp(0x89);
        gte_ldsv(gteDir);
        gte_gpf12_real();
        gte_stsv(gteDir);

        task->extra->field_8->coord.t[0] += dir->vx;
        task->extra->field_8->coord.t[1] += dir->vy;
        if (work->field_1AC >= 0x29) {
            task->extra->field_8->coord.t[1] = -0x3E8;
        }
        task->extra->field_8->coord.t[2] += dir->vz;
        task->extra->field_8->flg         = 0;

        work->field_1B0 += 0x60;
        Gp_ClearRec18Occupied(&work->rec0);

        work->coord.sub = &Gfx_ViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = task->extra->field_8->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = task->extra->field_8->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                             Gp_State1C->field_8);

        if (work->field_1AC >= 0x35) {
            Gp_UnlinkObj(&work->obj0);
            task->state++;
            work->field_1A8 = 1;
        }
    } else {
        work->coord.sub = &Gfx_ViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = task->extra->field_8->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = task->extra->field_8->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        if (host->field_F08 != 6) {
            Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                                 Gp_State1C->field_8);
        }
    }

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(SVECTOR);
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80137D4C);

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// shrinks it uniformly to half size: `ratan2` of the rotation's Z basis gives
/// the yaw, `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix`
/// applies 0.5 on all three axes. The working matrix lives in a frame carved
/// off `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. Written as an inline so the four scratch-head accesses
/// stay absolute; see `Actor444000_RebuildRotation` in `actor_444000_4.c`.
static __inline__ void Actor444000_ShrinkRotation(GsCOORDINATE2* coord)
{
    Actor444000RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor444000RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor444000RotScratch));
    *(Actor444000RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x800;
    sc->scale.vy = 0x800;
    sc->scale.vz = 0x800;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(Actor444000RotScratch);
}

/// Death throes of the grabbing enemy: bounce the model on the floor until it
/// settles. While the model is still below the floor plane (`coord.t[1] > 0`)
/// it is snapped back to -0x32, the step counter is cleared, the impact cue is
/// enqueued with the object's own pan and half its depth, and the task steps
/// on. Otherwise the body keeps falling by `field_1AA`'s magnitude, drifts a
/// fifteenth of `vel` in x and z, has its colour refreshed from the model's
/// world position, damps the two shake terms and has its rotation rebuilt at
/// half scale.
void func_actor_444000_801381B0(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work = task->field_1C;
    GsCOORDINATE2*       coord;
    VECTOR               pos;
    s32                  sfx;
    s32                  pan;
    s32                  drop;
    s32                  bounce;

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    coord = task->extra->field_8;
    drop  = coord->coord.t[1];
    if (drop > 0) {
        coord->coord.t[1] = -0x32;
        work->field_1AC   = 0;
        sfx               = ((enemy->field_8 >> 0xC) << 8) | 0x4020000C;
        pan               = (s8)Gp_GetObjPan((GpObj38*)task->extra->field_8);
        SndEvt_EnqueueType6(sfx, pan, (s8)(Gp_GetObjDepth((GpObj38*)task->extra->field_8) / 2));
        task->state++;
        return;
    }

    bounce            = ABS(work->field_1AA);
    coord->coord.t[1] = drop + bounce;

    task->extra->field_8->coord.t[0] += work->vel.vx / 15;
    task->extra->field_8->coord.t[2] += work->vel.vz / 15;
    task->extra->field_8->flg         = 0;

    pos.vx = task->extra->field_8->workm.t[0];
    pos.vy = task->extra->field_8->workm.t[1];
    pos.vz = task->extra->field_8->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;

    Actor444000_ShrinkRotation(task->extra->field_8);
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80138490);

/// Hold state of the enemy dispatched through `D_actor_444000_80131EA8`: once
/// `field_1A8` says the take-over is armed and `field_1B2` says the player
/// animation is already installed, rebuild the overlay's own animation-set
/// table from the player's current weapon block and (re)send it as message
/// 0x3FF, flagging the model object busy. Then count the step, and after nine
/// of them cancel the animation with message 0x3F1 and step the task on.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down, cancelling a
/// still-installed animation on the way out.
void func_actor_444000_801389EC(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work;
    Task*                player;
    s32                  armed;

    work   = task->field_1C;
    player = Game_GetPtrSlot(3);
    if (D_actor_444000_80144A68 == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    if (work->field_1A8 != 0) {
        armed           = work->field_1B2;
        work->field_1AC = 0;
        if (armed != 1) {
            task->state++;
            return;
        }
        D_actor_444000_80161694[2] =
            ((Actor444000AnimTable*)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9])->sets[9];
        work->anim.field_0 = D_actor_444000_80161694;
        work->anim.field_4 = 2;
        work->anim.field_8 = armed;
        work->anim.field_C = 9;
        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        task->extra->field_C = 0x80;
    }

    if (work->field_1AC >= 9) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
        work->field_1B2 = 0;
        task->state++;
    }
    work->field_1AC++;
}

/// World position of `coord` as seen from `Gfx_ViewCoord`: `out` starts as the
/// point in `coord`'s own space and is walked up the coordinate hierarchy, one
/// `gte_rt` per level, until the view coordinate is reached. A hierarchy that
/// does not end at the view coordinate leaves `out` untouched.
static __inline__ void Actor444000_LocalToView(GsCOORDINATE2* coord, SVECTOR* out)
{
    SVECTOR acc;
    VECTOR  v;
    s32     flag;

    acc.vx = out->vx;
    acc.vy = out->vy;
    acc.vz = out->vz;

    for (;;) {
        if (coord->sub == NULL) {
            return;
        }
        if (coord != &Gfx_ViewCoord) {
            gte_SetTransMatrix(&coord->coord);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&acc);
            gte_rt_real();
            gte_stlvnl(&v);
            gte_stflg(&flag);
            acc.vx = v.vx;
            acc.vy = v.vy;
            acc.vz = v.vz;
            coord  = coord->sub;
        } else {
            out->vx = acc.vx;
            out->vy = acc.vy;
            out->vz = acc.vz;
            return;
        }
    }
}

/// Link one of the work block's display nodes: it hangs off the model's own
/// coordinate, carries `rec` as its collision-record table and sits at `pos`
/// in that coordinate's space.
static __inline__ void Actor444000_LinkWorkObj(GpObj* obj, GsCOORDINATE2* coord, GpRec18* rec,
                                               SVECTOR* pos, s32 prio, s32 kind)
{
    obj->field_8  = coord;
    obj->field_C  = rec;
    obj->field_10 = pos->vx;
    obj->field_12 = pos->vy;
    obj->field_14 = pos->vz;
    obj->field_1C = 0x100;
    obj->flags    = 1;
    Gp_LinkObj(prio, obj);
    Gp_InitRec18Table(obj->field_C, kind, 0);
}

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F0C`:
/// allocate its work block, drop the model onto the floor of the view
/// coordinate and hang the two display nodes off it.
///
/// The model is reparented to `Gfx_ViewCoord` and its translation replaced by
/// the world position of part 3 of the owning enemy's model, so the body starts
/// where that part is. `field_1AA` is a ninth of that height -- the bounce the
/// descent state adds back -- and `vel` the horizontal gap to the player, which
/// the later states spend a fifteenth at a time. The landing cue is enqueued at
/// the model's own pan and depth with the owner's id in its high half, the
/// model is spun to a random yaw, and the two nodes are linked with their
/// collision-record tables before the task's colour and light matrices are
/// pointed into the work block.
void func_actor_444000_80138B94(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work;
    GpEnemy*             owner;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    player = Game_GetPtrSlot(3);

    if (D_actor_444000_80144A68 == 1 ||
        (work = Mem_Calloc(sizeof(Actor444000GrabWork), false), task->field_1C = work, work == NULL)) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->field_8->sub = &Gfx_ViewCoord;
    task->extra->field_C      = 0;

    vec.vx = vec.vy = vec.vz = 0;
    Actor444000_LocalToView(&((TmdObject*)owner->task->extra)->field_8[3], &vec);

    task->extra->field_8->coord.t[0] = vec.vx;
    task->extra->field_8->coord.t[1] = vec.vy;
    task->extra->field_8->coord.t[2] = vec.vz;
    task->extra->field_8->flg        = 0;

    work->field_1AA = task->extra->field_8->coord.t[1] / 9;
    work->vel.vx =
        ((TmdObject*)player->extra)->field_8->coord.t[0] - task->extra->field_8->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        ((TmdObject*)player->extra)->field_8->coord.t[2] - task->extra->field_8->coord.t[2];
    work->field_1AC = 0;
    task->state++;

    sfx = ((owner->field_8 >> 0xC) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan((GpObj38*)task->extra->field_8);
    SndEvt_EnqueueType6(sfx, pan, (s8)Gp_GetObjDepth((GpObj38*)task->extra->field_8));

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 0x10) & 0x1FF, 1);

    vec.vx = vec.vy = vec.vz = 0;

    Actor444000_LinkWorkObj(&work->obj0, task->extra->field_8, &work->rec0, &vec, 3, 1);

    work->obj1.field_8  = task->extra->field_8;
    work->obj1.field_C  = &work->rec1;
    work->obj1.field_10 = 0;
    work->obj1.field_12 = 0;
    work->obj1.field_14 = 0;
    work->obj1.field_18 = 0x3000A;
    work->obj1.field_1C = 0x100;
    work->obj1.flags    = 1;
    Gp_LinkObj(2, &work->obj1);

    work->obj0.flags |= 0x8000;
    Gp_InitRec18Table(work->obj1.field_C, 3, 0);
    work->obj1.flags   |= 0x4000;
    work->obj0.field_18 = Gp_PackObjPair((GpObj50*)owner, 5);

    task->extra->field_1C = &work->lightMtx;
    task->extra->field_20 = &work->colorMtx;
}

/// Descent state that follows the hold: once the model's y has passed its apex
/// (gone negative) both display nodes get their draw flags raised and the
/// bounce height `field_1AA` is added back to y as a magnitude each step. When
/// y reaches -0x31 or above it is clamped to -0x32, the step counter is reset,
/// the landing sound is played at the model's own pan and depth, and the task
/// steps on. Collision against `rec1` -- and, in room 0x0427 past x 0x4B65 --
/// kills the horizontal velocity, whatever is left of it moves the model by a
/// ninth per step, and the model's own `workm` translation is handed to
/// `Gp_UpdateActorColor`.
void func_actor_444000_80138FC4(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work = task->field_1C;
    VECTOR               pos;
    s32                  pan;

    if (D_actor_444000_80144A68 == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    if (work->field_1A8 != 0) {
        Gp_SetLightMode((GpObj4C*)enemy, 0);
        task->extra->field_C = 2;
    }

    if (task->extra->field_8->coord.t[1] < 0) {
        work->obj0.flags                 |= 0x8000;
        work->obj1.flags                 |= 0x4000;
        task->extra->field_8->coord.t[1] += ABS(work->field_1AA);
    }

    if (task->extra->field_8->coord.t[1] >= -0x31) {
        task->extra->field_8->coord.t[1] = -0x32;
        work->field_1AC                  = 0;
        pan                              = (s8)Gp_GetObjPan((GpObj38*)task->extra->field_8);
        SndEvt_EnqueueType6(0x4020000C, pan, (s8)Gp_GetObjDepth((GpObj38*)task->extra->field_8));
        task->state++;
    }

    if (func_actor_444000_80132B14(task->extra->field_8, &work->rec1, 3) != 0) {
        work->vel.vz = 0;
        work->vel.vx = 0;
    }

    if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x04270000 &&
        task->extra->field_8->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    Gp_ClearRec18Occupied(&work->rec1);
    Gp_ClearRec18Occupied(&work->rec0);

    task->extra->field_8->coord.t[0] += work->vel.vx / 9;
    task->extra->field_8->coord.t[2] += work->vel.vz / 9;
    task->extra->field_8->flg         = 0;

    pos.vx = task->extra->field_8->workm.t[0];
    pos.vy = task->extra->field_8->workm.t[1];
    pos.vz = task->extra->field_8->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131E90);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131E9C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131EA8);

/// Settling state that follows the bounce: the step counter drives the whole
/// thing. When the dispatcher flags a state change the horizontal velocity is
/// cut to a ninth, both light modes are reset and the two display nodes drop
/// the draw flags the descent raised. Past x 0x4B65 in room 0x0427 the x
/// velocity is killed outright; for the first eight steps what is left of it
/// moves the model and is halved again each step. Steps 1, 2, 4, 8 and 20 puff
/// a `0x600A5` effect out of the model's coordinate, and 4 and 8 also switch
/// the light mode. After 0x51 steps both nodes are unlinked and the task steps
/// on; until then the two collision-record tables are wiped each step. The
/// model's own `workm` translation is handed to `Gp_UpdateActorColor`.
void func_actor_444000_8013928C(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work = task->field_1C;
    VECTOR               pos;
    s16                  step;

    if (D_actor_444000_80144A68 == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    if (work->field_1A8 != 0) {
        work->field_1AC = 0;
        work->vel.vx   /= 9;
        work->vel.vz   /= 9;
        Gp_SetLightMode((GpObj4C*)enemy, 0);
        Gp_SetLightMode((GpObj4C*)enemy, 1);
        work->obj1.flags    &= ~0x4000;
        work->obj0.flags    &= ~0x8000;
        task->extra->field_C = 2;
    }

    work->field_1AC++;

    if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x04270000 &&
        task->extra->field_8->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    if (work->field_1AC < 8) {
        task->extra->field_8->coord.t[0] += work->vel.vx;
        task->extra->field_8->coord.t[2] += work->vel.vz;
        work->vel.vx                    >>= 1;
        work->vel.vz                    >>= 1;
        task->extra->field_8->flg         = 0;
    }

    step = work->field_1AC - 1;
    switch (step) {
        case 3:
        case 7:
            Gp_SpawnEff(0x600A5, task->extra->field_8, 1, NULL);
            Gp_SetLightMode((GpObj4C*)enemy, 2);
            break;
        case 0:
        case 1:
        case 19:
            Gp_SpawnEff(0x600A5, task->extra->field_8, 1, NULL);
            break;
    }

    if (work->field_1AC >= 0x51) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        task->state++;
    }

    if (work->field_1AC < 0x51) {
        Gp_ClearRec18Occupied(&work->rec1);
        Gp_ClearRec18Occupied(&work->rec0);
    }

    pos.vx = task->extra->field_8->workm.t[0];
    pos.vy = task->extra->field_8->workm.t[1];
    pos.vz = task->extra->field_8->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80139594);

/// Ascent state that precedes the descent above: lift the model by 0x1F4 plus
/// `field_1AE` a step until it passes -0x4E20, then clamp it there, snap its
/// horizontal position back onto `work->target`, restart the step counter, pick
/// a fresh 0..0x1F bias for the next leg, flag the list object and step the task
/// on. Either way the work block's own coordinate is left tracking the model.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void func_actor_444000_80139AF8(GpEnemy* enemy, Actor444000Drop* task)
{
    Actor444000DropWork* work;
    s32                  y;

    work = task->field_1C;
    if (D_actor_444000_80144A68 == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    y                                = task->extra->field_8->coord.t[1] - 0x1F4;
    task->extra->field_8->coord.t[1] = y - work->field_1AE;
    if (task->extra->field_8->coord.t[1] < -0x4E20) {
        task->state++;
        task->extra->field_8->coord.t[0] = work->target.vx;
        task->extra->field_8->coord.t[2] = work->target.vz;
        Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
        task->extra->field_8->coord.t[1] = -0x4E20;
        work->timer                      = 0;
        work->field_1AE                  = ((u32)Gp_LcgState >> 16) & 0x1F;
        work->obj.flags                 |= 0x8000;
    }

    task->extra->field_8->flg = 0;
    work->coord.coord.t[0]    = task->extra->field_8->coord.t[0];
    work->coord.coord.t[1]    = task->extra->field_8->coord.t[1];
    work->coord.coord.t[2]    = task->extra->field_8->coord.t[2];
    work->coord.flg           = 0;
    Gp_UpdateCoord(&work->coord);
}

/// Descent state of the enemy dispatched through `D_actor_444000_80131F1C`:
/// draw the growing shadow marker on the floor under the model, then after
/// 0x14 steps start pulling the model down by `0x258 + field_1AE` a step. When
/// it reaches floor level, zero the height, restart the step counter, tell the
/// trailing `Gp_SpawnEff` effect to wind down, play the landing cue and step
/// the task on. Either way the work block's own coordinate is left tracking
/// the model. Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void func_actor_444000_80139C80(GpEnemy* enemy, Actor444000Drop* task)
{
    Actor444000DropWork* work;
    Actor444000DropCoord coord;
    MATRIX*              mtx;
    GpEnemy*             owner;
    s32                  snd;
    s32                  pan;

    work = task->field_1C;
    if (D_actor_444000_80144A68 == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    work->timer++;
    coord.c.sub          = &Gfx_ViewCoord;
    mtx                  = &coord.c.coord;
    coord.ident.m00_m01  = 0x1000;
    coord.ident.m02_m10  = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    coord.ident.m20_m21  = 0;
    mtx->m[2][2]         = 0x1000;
    Gfx_RotMatrixY(mtx, 0, 1);

    coord.c.coord.t[0] = task->extra->field_8->coord.t[0];
    coord.c.coord.t[1] = 0;
    coord.c.coord.t[2] = task->extra->field_8->coord.t[2];
    coord.c.flg        = 0;
    Gp_UpdateCoord(&coord.c);

    Gp_DrawEffGroundQuad((VECTOR3*)coord.c.workm.t, (s16)((s16)work->timer * 8 + 0x80),
                         Gp_State1C->field_8);

    if ((s16)work->timer >= 0x14) {
        task->extra->field_8->coord.t[1] =
            task->extra->field_8->coord.t[1] + (work->field_1AE + 0x258);
        if (task->extra->field_8->coord.t[1] > 0) {
            owner                            = task->parent->spawnArg2;
            task->extra->field_8->coord.t[1] = 0;
            work->timer                      = 0;
            if (work->eff != NULL) {
                work->eff->field_0->spawnArg1 = 2;
            }
            task->state++;
            snd = ((owner->field_8 >> 12) << 8) | 0x4020000C;
            pan = (s8)Gp_GetObjPan((GpObj38*)task->extra->field_8);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)task->extra->field_8));
        }
    }

    task->extra->field_8->flg = 0;
    Gp_ClearRec18Occupied(&work->rec);
    work->coord.coord.t[0] = task->extra->field_8->coord.t[0];
    work->coord.coord.t[1] = task->extra->field_8->coord.t[1];
    work->coord.coord.t[2] = task->extra->field_8->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131F0C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131F1C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131F30);
