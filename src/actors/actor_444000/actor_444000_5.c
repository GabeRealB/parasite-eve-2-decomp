#include "common.h"

#include "actors/actor_444000.h"
#include "actors/actor_444000_view.h"
#include "actors/actors_shared_80133de4.h"
#include "actors/actors_shared_80133f64.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include <psyq/abs.h>
#include <psyq/inline_c.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// GPF with `sf = 1`, which `psyq/inline_c.h` spells without the COP2 prefix
/// the retail build used. Same form as `src/pe/energyball/energyball.c`.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s16 D_actor_444000_80144A68;
extern s32 D_actor_444000_80144A74;
extern s32 D_actor_444000_80144A7C;
extern s32 D_actor_444000_80144A6C;
extern s16 D_actor_444000_80144A70;
extern s32 Gp_LcgState;

extern MATRIX* D_80073B8C;
extern s8      D_8007218A;
extern u8      D_80073BA9;
extern u8      D_801153F4;
/// Global freeze flag: 1 while the game is halted, which stops the run below
/// from advancing the model.
extern u8         D_80072729;
extern GpAnimSet* D_actor_444000_80161694[];

/// Which of the three drop-point groups the falling enemies use this round,
/// rerolled off `Gp_LcgState` whenever a spawn arrives with `spawnArg1` 0.
extern u8 D_actor_444000_80161690;
/// Per-`spawnArg1` offset from the host model to the point the enemy is stood
/// up at when it is spawned.
extern SVECTOR D_actor_444000_80161704[];
/// The drop points themselves: `vz` is added to the ring x coordinate and `vx`
/// (less 0x189C) becomes the z coordinate.
extern SVECTOR D_actor_444000_80161744[];
/// `[group][spawnArg1]` index into `D_actor_444000_80161744`.
extern u8 D_actor_444000_801617C4[][8];
/// Reply buffer the hold state below hands message 0x3F8.
extern Actor444000Msg3F8 D_actor_444000_80161898;
extern GpAnimBlk*        Gp_PlayerAnimBlkTbl[];
extern u16               Gp_WeaponIdBase[];

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

/// Walk `coord` a fixed 0x32/0x1000 of its own forward axis (column 2 of its
/// rotation, normalised and GPF-scaled) and flag it for rebuild. The direction
/// vector lives in an `SVECTOR` carved off `G_SCRATCH_HEAD` and handed straight
/// back; written as an inline so those scratch-head accesses stay absolute, the
/// same reason as `Actor444000_ShrinkRotation` above.
static __inline__ void Actor444000_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head                       = *(u8**)G_SCRATCH_HEAD;
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    *(SVECTOR**)G_SCRATCH_HEAD = dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x32);
    gte_ldsv(dir);
    gte_gpf12_real();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(SVECTOR);
}

/// The run-out / turn / run-back pass, stepped by `Actor444000Work::field_F08`.
///
/// A reset request re-arms the block: the model's flag word and the enemy's
/// link state are cleared, both colour steps are switched on, animation 2 is
/// requested and the scratch matrix is seeded with an identity rotation.
///
/// Every step publishes the yaw from the model's own facing to the player in
/// `field_7C4`, wrapped into +/-0x800, and -- unless the game is frozen --
/// walks the model forward along that facing. State 0 runs out to x 0x1770,
/// state 1 turns the model 0xD a step until it has swung the full half turn
/// (its rotation is rebuilt from the running `angle` rather than spun in
/// place), and states 2 to 5 run it back through -0x1387, -0x251B and -0x32C7.
/// Past -0x4203 the task hands over to state 0x10 and tells the player task
/// (slot 7) message 0x13F4.
void func_actor_444000_8013482C(Actor444000* task)
{
    Actor444000RunScratch* sc;
    Actor444000RunMat*     mat;
    TmdObject*             tmd;
    Actor444000Work*       work;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         model;
    GsCOORDINATE2*         facing;
    u8*                    head;
    s16                    ang;
    s32                    frame;

    head        = (u8*)SCRATCH_SP;
    SCRATCH_SP -= sizeof(Actor444000RunScratch);
    sc          = (Actor444000RunScratch*)SCRATCH_SP;

    work  = task->field_1C;
    enemy = task->field_20;

    if (work->field_4 != 0) {
        tmd                 = (TmdObject*)task->extra;
        enemy->node.field_4 = 0;
        tmd->flags          = 0;
        work->field_EF4     = 1;
        work->field_7B3     = 2;
        work->field_EF6     = 1;
        work->field_7B0     = 1;
        work->field_EFA     = 0;
        work->field_EFE     = 0;
        mat                 = &((Actor444000RunScratch*)(head - sizeof(Actor444000RunScratch)))->m;
        mat->ident.m00_m01  = 0x1000;
        mat->ident.m02_m10  = 0;
        mat->ident.m11_m12  = 0x1000;
        mat->ident.m20_m21  = 0;
        mat->ident.m22      = 0x1000;
    }

    func_actor_444000_8013441C(task);

    frame = work->slots0[2].field_2 & 0x3FF;
    if (frame == 0x12 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
        id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords) / 2));
    }

    frame = work->slots0[2].field_2 & 0x3FF;
    if (frame == 0x18 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
        id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords) / 2));
    }

    work->field_7D8 = work->slots0[2].field_2 & 0x3FF;

    model      = ((TmdObject*)task->extra)->coords;
    sc->dir.vx = Player_Status.coordMtx->t[0] - model->coord.t[0];
    sc->dir.vy = Player_Status.coordMtx->t[1] - model->coord.t[1];
    sc->dir.vz = Player_Status.coordMtx->t[2] - model->coord.t[2];

    facing = ((TmdObject*)task->extra)->coords;
    ang    = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);

    if (ang < 0) {
    wrapUp:
        if (ang < -0x800) {
            ang += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (ang > 0x800) {
            ang -= 0x1000;
            goto wrapDown;
        }
    }

    work->field_7C4 = ang;

    switch (work->field_F08) {
        case 0: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[0] >= 0x1770) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 1:
            coord = ((TmdObject*)task->extra)->coords;
            if (coord->coord.t[0] < 0x2134) {
                if (D_80072729 != 1) {
                    Actor444000_StepForward(coord);
                }
            } else {
                sc->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0xD;
                sc->m.mat = ((TmdObject*)task->extra)->coords->coord;

                Gfx_MatrixCol2(&sc->m.mat, &sc->dir);
                VectorNormalSS(&sc->dir, &sc->dir);
                gte_lddp(0xBEA);
                gte_ldsv(&sc->dir);
                gte_gpf12_real();
                gte_stsv(&sc->dir);

                sc->m.mat.t[0] += sc->dir.vx;
                sc->m.mat.t[1] += sc->dir.vy;
                sc->m.mat.t[2] += sc->dir.vz;

                Gfx_RotMatrixY(&sc->m.mat, sc->angle, 1);
                ((TmdObject*)task->extra)->coords->coord = sc->m.mat;

                Gfx_MatrixCol2(&sc->m.mat, &sc->dir);
                VectorNormalSS(&sc->dir, &sc->dir);
                gte_lddp(-0xBB8);
                gte_ldsv(&sc->dir);
                gte_gpf12_real();
                gte_stsv(&sc->dir);

                ((TmdObject*)task->extra)->coords->coord.t[0] += sc->dir.vx;
                ((TmdObject*)task->extra)->coords->coord.t[1] += sc->dir.vy;
                ((TmdObject*)task->extra)->coords->coord.t[2] += sc->dir.vz;
                ((TmdObject*)task->extra)->coords->flg         = 0;

                if (0x800 - ABS(sc->angle) < 0xD) {
                    sc->angle = 0x800;
                    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x800, 1);
                    work->field_F08++;
                }
            }
            break;

        case 2: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[2] < -0x1387) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 3: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[2] < -0x251B) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 4: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[2] < -0x32C7) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 5: {
            s32            paused = D_80072729;
            GsCOORDINATE2* c      = ((TmdObject*)task->extra)->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            ((TmdObject*)task->extra)->coords->flg = 0;
            if (((TmdObject*)task->extra)->coords->coord.t[2] < -0x4203) {
                work->field_0 = 0x10;
                work->field_F08++;
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, 0, 0);
            }
            break;
    }

    SCRATCH_SP += sizeof(Actor444000RunScratch);
}

/// Rebuild `coord`'s rotation around the yaw it already faces, left at full
/// width but scaled by `y` vertically -- the squash the death sequence retracts
/// each body with. The same shape as `Actor444000_ScaleRotation` below, except
/// the vertical scale arrives as an `s16`, which is what puts its sign
/// extension at the `scale.vy` store rather than at the call site. The working
/// matrix lives in a frame carved off `G_SCRATCH_HEAD`, handed back once the
/// rotation has been copied onto the coordinate.
static __inline__ void Actor444000_SquashRotation(GsCOORDINATE2* coord, s16 y)
{
    Actor444000RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor444000RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor444000RotScratch));
    *(Actor444000RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = y;
    sc->scale.vz = 0x1000;
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

/// State 0x12, the death sequence: the boss collapses, each of its escort
/// bodies is cut loose from the model hierarchy and then squashed flat in its
/// own window of the sub-state counter.
///
/// A reset request re-arms the block on animation 0x12, clears the enemy's link
/// state, the model's flag word and the four counters, marks the session
/// (`gGameSession::at4.loc.place` 3) and plays the death cue at half depth.
///
/// The rest of the tick splits on bit 0x100 of the second animation slot --
/// whether the collapse animation is still running or has finished.
///
/// While it runs, `D_actor_444000_80144A70` is walked down 0xC8 a step until it
/// is under 0x191, four one-shot cues fire on frames 0x33, 0x3D, 0x4E and 0x71
/// of the fourth slot, and sub-states 0x14, 0x82, 0x14A and 0x1DC each hand one
/// body over: 0x14 switches the host and escort 3 to light mode 1, while the
/// other three reparent escort 2, 4 and 3's model to `Gfx_ViewCoord`. That
/// reparenting is why both halves of the part's placement have to be resolved
/// by hand -- `Actor444000_AccumulateRotation` for the rotation it had up the
/// chain and `Actor444000_LocalToView` for its origin -- the same pair
/// `func_actor_444000_80137594` uses. Past each of those sub-states the body
/// sinks toward the host's own height 0x1E a step, clamped there, and squashes
/// from 0x1000 to nothing over 0x28 steps, throwing effect 0x60196 at one of
/// three offsets every fifth step and raising flag 0x80 on the last one.
///
/// Once the animation has finished, escort 0, escort 1 and the host model are
/// squashed over their own windows (0..0x28, 0x14..0x3C and 0xD..0x85), the
/// host throws one of five effects around itself every third step of its
/// window, escort 3 halves its height over the 0x5A steps from 0xA1 -- the step
/// that ends it also raises the arena floor's last eight grid corners -- and
/// step 0xA0 enqueues the collapse cue.
void func_actor_444000_80135448(Actor444000* task)
{
    Actor444000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    MATRIX           mat;
    SVECTOR          pos;
    SVECTOR*         verts;
    s32              frame;
    s32              step;

    work  = task->field_1C;
    enemy = task->field_20;

    if (work->field_4 != 0) {
        s32 id;
        s32 pan;

        tmd                 = (TmdObject*)task->extra;
        enemy->node.field_4 = 0;
        tmd->flags          = 0;
        work->field_7B3     = 0x12;
        work->field_EF4     = 0;
        work->field_EF6     = 0;
        work->field_EFA     = 0;
        work->field_7B0     = 1;
        work->field_EFE     = 0;

        func_actor_444000_8013441C(task);

        gGameSession->at4.loc.place = 3;
        id                          = (((u16)enemy->field_8 >> 12) << 8) | 0x54280007;
        pan                         = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords) / 2));
        return;
    }

    if (work->slots0[1].field_10 & 1) {
        work->field_6 = 0;
        func_actor_444000_8013441C(task);
    }

    if (!(work->slots0[1].field_10 & 0x100)) {
        if (D_actor_444000_80144A70 >= 0x191) {
            D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
        }

        func_actor_444000_8013441C(task);

        frame = work->slots0[3].field_2 & 0x3FF;
        if (frame == 0x33 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200013;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords));
        }

        frame = work->slots0[3].field_2 & 0x3FF;
        if (frame == 0x3D && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200003;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords));
        }

        frame = work->slots0[3].field_2 & 0x3FF;
        if (frame == 0x4E && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200014;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords));
        }

        frame = work->slots0[3].field_2 & 0x3FF;
        if (frame == 0x71 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200015;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords));
        }

        work->field_7A8 = work->slots0[3].field_2 & 0x3FF;

        switch (work->field_6) {
            case 0x14:
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                Gp_SetLightMode((GpObj4C*)work->field_ECC[3], 1);
                break;

            case 0x82:
                Actor444000_AccumulateRotation(&((TmdObject*)task->extra)->coords[4], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                Actor444000_LocalToView(&((TmdObject*)task->extra)->coords[4], &pos);

                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->sub        = &Gfx_ViewCoord;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord      = mat;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[0] = pos.vx;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] = pos.vy;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[2] = pos.vz;
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->flg        = 0;
                break;

            case 0x1DC:
                Actor444000_AccumulateRotation(&((TmdObject*)task->extra)->coords[3], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                Actor444000_LocalToView(&((TmdObject*)task->extra)->coords[3], &pos);

                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->sub        = &Gfx_ViewCoord;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord      = mat;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[0] = pos.vx;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] = pos.vy;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[2] = pos.vz;
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->flg        = 0;
                break;

            case 0x14A:
                Actor444000_AccumulateRotation(&((TmdObject*)task->extra)->coords[4], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                Actor444000_LocalToView(&((TmdObject*)task->extra)->coords[4], &pos);

                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->sub        = &Gfx_ViewCoord;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord      = mat;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[0] = pos.vx;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] = pos.vy;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[2] = pos.vz;
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->flg        = 0;
                work->field_EF8                                                   = 0;
                break;
        }

        if (work->field_6 >= 0x83) {
            if (((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] <
                ((TmdObject*)task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] +=
                    (work->field_6 - 0x82) * 0x1E;
            } else if (((TmdObject*)task->extra)->coords->coord.t[1] <
                       ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] =
                    ((TmdObject*)task->extra)->coords->coord.t[1];
                Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords, 0x13401800,
                            NULL);
            }

            step = work->field_6 - 0x82;
            if (step < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[2]->task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[2]->task->extra)->flags = 2;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                ((TmdObject*)work->field_ECC[2]->task->extra)->flags = 0x80;
                Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[2]->task->extra)->coords, 0x13401800,
                            NULL);
            }
        }

        if (work->field_6 >= 0x14B) {
            if (((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] <
                ((TmdObject*)task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] +=
                    (work->field_6 - 0x14A) * 0x1E;
            } else if (((TmdObject*)task->extra)->coords->coord.t[1] <
                       ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] =
                    ((TmdObject*)task->extra)->coords->coord.t[1];
                Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords, 0x13401800,
                            NULL);
            }

            step = work->field_6 - 0x14A;
            if (step < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[4]->task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[4]->task->extra)->flags = 2;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                ((TmdObject*)work->field_ECC[4]->task->extra)->flags = 0x80;
                Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[4]->task->extra)->coords, 0x13401800,
                            NULL);
            }
        }

        if (work->field_6 >= 0x1DD) {
            if (((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] <
                ((TmdObject*)task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] +=
                    (work->field_6 - 0x1DC) * 0x1E;
            } else if (((TmdObject*)task->extra)->coords->coord.t[1] <
                       ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1]) {
                ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] =
                    ((TmdObject*)task->extra)->coords->coord.t[1];
            }

            step = work->field_6 - 0x1DC;
            if (step < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[3]->task->extra)->flags = 2;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, ((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                ((TmdObject*)work->field_ECC[3]->task->extra)->flags = 0x80;
            }
        }
    } else {
        switch (work->field_6) {
            case 0x28:
                break;
            case 0x78:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
        }

        if (work->field_6 > 0) {
            if (work->field_6 < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[0]->task->extra)->coords,
                                           (s16)(0x1000 - ((work->field_6 * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[0]->task->extra)->flags = 2;
            } else if (work->field_6 == 0x28) {
                ((TmdObject*)work->field_ECC[0]->task->extra)->flags = 0x80;
            }
        }

        if (work->field_6 >= 0x15) {
            step = work->field_6 - 0x14;
            if (step < 0x28) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[1]->task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                ((TmdObject*)work->field_ECC[1]->task->extra)->flags = 2;
            } else if (step == 0x28) {
                ((TmdObject*)work->field_ECC[1]->task->extra)->flags = 0x80;
            }
        }

        if (work->field_6 >= 0xE) {
            step = work->field_6 - 0xD;
            if (step < 0x78) {
                Actor444000_SquashRotation(((TmdObject*)task->extra)->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 120)));
                ((TmdObject*)task->extra)->flags = 2;
            } else if (step == 0x78) {
                ((TmdObject*)task->extra)->flags = 0x80;
            }
        }

        if ((s16)((s16)(u16)work->field_6 % 3) == 0 && (s16)(u16)work->field_6 - 0xD < 0x78) {
            switch ((s16)((s16)((s16)(u16)work->field_6 / 3) % 5)) {
                case 0:
                    pos.vz = 0;
                    pos.vy = 0;
                    pos.vx = 0;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x14101900, &pos);
                    break;
                case 1:
                    pos.vx = 0x960;
                    pos.vy = 0;
                    pos.vz = -0x960;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x13201800, &pos);
                    break;
                case 2:
                    pos.vx = -0x9C4;
                    pos.vy = 0;
                    pos.vz = 0x9C4;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x131C1800, &pos);
                    break;
                case 3:
                    pos.vx = -0x6A4;
                    pos.vy = 0;
                    pos.vz = 0x640;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x14101800, &pos);
                    break;
                case 4:
                    pos.vx = 0x6A4;
                    pos.vy = 0;
                    pos.vz = -0x640;
                    Gp_SpawnEff(0x60196, ((TmdObject*)task->extra)->coords, 0x14301800, &pos);
                    break;
            }
        }

        if (work->field_6 >= 0xA1) {
            step = work->field_6 - 0xA0;
            if (step < 0x5A) {
                Actor444000_SquashRotation(((TmdObject*)work->field_ECC[3]->task->extra)->coords,
                                           (s16)(0x800 - ((step * 0x800) / 90)));
            } else if (step == 0x5A) {
                ((TmdObject*)task->extra)->flags = 0x80;

                verts        = Gp_GridParams->field_8;
                verts[24].vy = 0x1F4;
                verts[25].vy = 0x1F4;
                verts[26].vy = 0x320;
                verts[27].vy = 0x320;
                verts[28].vy = 0x1F4;
                verts[29].vy = 0x1F4;
                verts[30].vy = 0x320;
                verts[31].vy = 0x320;
            }
        }

        if (work->field_6 == 0xA0) {
            SndEvt_EnqueueType7((((u16)enemy->field_8 >> 12) << 8) | 0x54280007, 1);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_801371E8);

/// Link one of the work block's display nodes: it hangs off the model's own
/// coordinate, carries `rec` as its collision-record table and sits at `pos`
/// in that coordinate's space with `field1C` as its extent.
static __inline__ void Actor444000_LinkWorkObj(GsCOORDINATE2* coord, GpObj* obj, GpRec18* rec,
                                               SVECTOR* pos, s16 field1C, s32 prio, s32 kind)
{
    obj->field_8  = coord;
    obj->field_C  = rec;
    obj->field_10 = pos->vx;
    obj->field_12 = pos->vy;
    obj->field_14 = pos->vz;
    obj->field_1C = field1C;
    obj->flags    = 1;
    Gp_LinkObj(prio, obj);
    Gp_InitRec18Table(obj->field_C, kind, 0);
}

/// Spawn state of the enemy dispatched through `D_actor_444000_80131EA8`:
/// allocate its work block and stand the model up where the host's first
/// escort is, in view space.
///
/// The model is reparented to `Gfx_ViewCoord`, so both halves of that escort's
/// part 1 have to be resolved by hand: `Actor444000_AccumulateRotation` walks
/// the part's coordinate chain up to the view coordinate for the rotation and
/// `Actor444000_LocalToView` carries its origin along the same chain for the
/// translation. The model is then turned a quarter turn, its single display
/// node is linked with a 0x394 extent, and that node is paired with the owning
/// enemy so collisions against it reach this task.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down, the
/// host actor has left the grab states, or the work block cannot be allocated.
void func_actor_444000_80137594(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work;
    GpEnemy*             owner;
    Actor444000Work*     host;
    SVECTOR              pos;
    SVECTOR              vec;

    owner = task->parent->spawnArg2;
    host  = owner->task->work;

    if (D_actor_444000_80144A68 == 1 || (s16)host->field_0 == 0x10 || (s16)host->field_0 == 5 ||
        (s16)host->field_0 == 0xC || (s16)host->field_0 == 0x12 ||
        (work = Mem_Calloc(sizeof(Actor444000GrabWork), false), task->field_1C = work, work == NULL)) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    work->field_1AC          = 0;
    task->extra->coords->sub = &Gfx_ViewCoord;
    task->extra->flags       = 0;

    Actor444000_AccumulateRotation(&((TmdObject*)host->field_ECC[0]->task->extra)->coords[1],
                                   &task->extra->coords->coord);

    vec.vx = vec.vy = vec.vz = 0;
    Actor444000_LocalToView(&((TmdObject*)host->field_ECC[0]->task->extra)->coords[1], &vec);

    task->extra->coords->coord.t[0] = vec.vx;
    task->extra->coords->coord.t[1] = vec.vy;
    task->extra->coords->coord.t[2] = vec.vz;
    task->extra->coords->flg        = 0;

    Gfx_RotMatrixY(&task->extra->coords->coord, 0x80, 0);
    Gp_UpdateCoord(task->extra->coords);

    pos.vx = pos.vy = pos.vz = 0;
    Actor444000_LinkWorkObj(task->extra->coords, &work->obj0, &work->rec0, &pos, 0x394, 3, 1);

    work->obj0.flags   &= 0x7FFF;
    work->obj0.field_18 = Gp_PackObjPair((GpObj50*)owner, 2);
    work->field_1A8     = 1;
    task->state++;
}

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
    host  = owner->task->work;

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
        task->extra->coords->coord.t[1] += 0xA;

        Gfx_MatrixCol2(&task->extra->coords->coord, dir);
        VectorNormalSS(dir, dir);
        gte_lddp(0x89);
        gte_ldsv(gteDir);
        gte_gpf12_real();
        gte_stsv(gteDir);

        task->extra->coords->coord.t[0] += dir->vx;
        task->extra->coords->coord.t[1] += dir->vy;
        if (work->field_1AC >= 0x29) {
            task->extra->coords->coord.t[1] = -0x3E8;
        }
        task->extra->coords->coord.t[2] += dir->vz;
        task->extra->coords->flg         = 0;

        work->field_1B0 += 0x60;
        Gp_ClearRec18Occupied(&work->rec0);

        work->coord.sub = &Gfx_ViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = task->extra->coords->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = task->extra->coords->coord.t[2];
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
        work->coord.coord.t[0] = task->extra->coords->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = task->extra->coords->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        if (host->field_F08 != 6) {
            Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                                 Gp_State1C->field_8);
        }
    }

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(SVECTOR);
}

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

/// Entry state of the enemy dispatched through `D_actor_444000_80131F0C`:
/// allocate its work block and drop the model onto the floor of the view
/// coordinate, under escort 1 of the host actor.
///
/// The model is reparented to `Gfx_ViewCoord`, its texture page cleared and its
/// CLUT row set to 2, and -- once the stream buffers exist -- processed twice
/// before the spawn cue is enqueued at the model's own pan and half its depth
/// with the owner's id in its high half. The task's light and colour matrices
/// are pointed into the work block, the translation is replaced by the world
/// position of part 1 of escort 1's model, and `field_1AA` is a fifteenth of
/// that height. `vel` is the horizontal gap to the player, which the later
/// states spend a fifteenth at a time. The rotation is finally rebuilt at half
/// scale around the yaw the model already faces.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void func_actor_444000_80137D4C(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work;
    GpEnemy*             owner;
    Actor444000Work*     host;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    host   = owner->task->work;
    player = Game_GetPtrSlot(3);

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    work           = Mem_Calloc(sizeof(Actor444000GrabWork), false);
    task->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->coords->sub = &Gfx_ViewCoord;
    task->extra->flags       = 0;
    task->extra->tpage       = 0;
    task->extra->clut        = 2;

    if (task->extra->buffer != NULL) {
        Tmd_ProcessStream(task->extra);
        Tmd_ProcessStream(task->extra);
        sfx = ((owner->field_8 >> 0xC) << 8) | 0x4020001C;
        pan = (s8)Gp_GetObjPan((GpObj38*)task->extra->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(Gp_GetObjDepth((GpObj38*)task->extra->coords) / 2));
    }

    task->extra->lightMtx = &work->lightMtx;
    task->extra->colorMtx = &work->colorMtx;

    vec.vx = vec.vy = vec.vz = 0;
    Actor444000_LocalToView(&((TmdObject*)host->field_ECC[1]->task->extra)->coords[1], &vec);

    task->extra->coords->coord.t[0] = vec.vx;
    task->extra->coords->coord.t[1] = vec.vy;
    task->extra->coords->coord.t[2] = vec.vz;
    task->extra->coords->flg        = 0;

    work->field_1AA = task->extra->coords->coord.t[1] / 15;
    work->vel.vx =
        ((TmdObject*)player->extra)->coords->coord.t[0] - task->extra->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        ((TmdObject*)player->extra)->coords->coord.t[2] - task->extra->coords->coord.t[2];
    work->field_1AC = 0;
    work->field_1B2 = 0;

    Actor444000_ShrinkRotation(task->extra->coords);
    task->state++;
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

    coord = task->extra->coords;
    drop  = coord->coord.t[1];
    if (drop > 0) {
        coord->coord.t[1] = -0x32;
        work->field_1AC   = 0;
        sfx               = ((enemy->field_8 >> 0xC) << 8) | 0x4020000C;
        pan               = (s8)Gp_GetObjPan((GpObj38*)task->extra->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(Gp_GetObjDepth((GpObj38*)task->extra->coords) / 2));
        task->state++;
        return;
    }

    bounce            = ABS(work->field_1AA);
    coord->coord.t[1] = drop + bounce;

    task->extra->coords->coord.t[0] += work->vel.vx / 15;
    task->extra->coords->coord.t[2] += work->vel.vz / 15;
    task->extra->coords->flg         = 0;

    pos.vx = task->extra->coords->workm.t[0];
    pos.vy = task->extra->coords->workm.t[1];
    pos.vz = task->extra->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;

    Actor444000_ShrinkRotation(task->extra->coords);
}

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// rescales it: `ratan2` of the rotation's Z basis gives the yaw,
/// `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// `xz` on both horizontal axes and `y` on the vertical one. The working
/// matrix lives in a frame carved off `G_SCRATCH_HEAD`, which is handed back
/// once the rotation has been copied onto the coordinate. Written as an inline
/// so the four scratch-head accesses stay absolute, like
/// `Actor444000_ShrinkRotation` above.
static __inline__ void Actor444000_ScaleRotation(GsCOORDINATE2* coord, s16 xz, s32 y)
{
    Actor444000RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor444000RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor444000RotScratch));
    *(Actor444000RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = xz;
    sc->scale.vy = y;
    sc->scale.vz = xz;
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

/// Squared-distance test on a horizontal gap, run out of a `VECTOR3` carved
/// off `G_SCRATCH_HEAD`: the vector holds the gap's x and z beside a 1000-unit
/// reach, each component is squared in place, and the result says whether the
/// gap is longer than the reach.
static __inline__ s32 Actor444000_OutOfReach(SVECTOR* gap)
{
    VECTOR3* v;

    v                          = (VECTOR3*)(*(u8**)G_SCRATCH_HEAD - sizeof(VECTOR3));
    *(VECTOR3**)G_SCRATCH_HEAD = v;
    v->vx                      = gap->vx;
    v->vy                      = gap->vz;
    v->vz                      = 1000;
    v->vx                      = v->vx * v->vx;
    v->vy                      = v->vy * v->vy;
    v->vz                      = v->vz * v->vz;
    *(u8**)G_SCRATCH_HEAD      = *(u8**)G_SCRATCH_HEAD + sizeof(VECTOR3);

    return v->vx + v->vy >= v->vz;
}

/// Rise state of the enemy dispatched through `D_actor_444000_80131EA8`: for
/// the first nine steps the model is stretched taller and thinner each step --
/// horizontally `step * 400 + 0x800` and vertically `0x800 / step` -- around
/// the yaw it already faces. On step 7 it is squashed to 0x17A0 wide at normal
/// height, and if the player is within 1000 units horizontally, is not in mode
/// 2, still has HP and answers the 0x3F8 query, the overlay's own animation-set
/// table is sent as message 0x3FF and the take-over is latched in `field_1B2`.
/// The task steps on once the count passes ten with no animation installed,
/// once the latched animation has been released, or after 200 steps. Every
/// step refreshes the model's colour from its world position and damps the two
/// shake terms. Bails to `Gp_DestroyEnemy` when the overlay is shutting down,
/// cancelling a still-installed animation on the way out.
void func_actor_444000_80138490(GpEnemy* enemy, Actor444000Grab* task)
{
    Actor444000GrabWork* work;
    Task*                player;
    GameActor*           actor;
    PlayerStatus*        cfg;
    SVECTOR              gap;
    VECTOR               pos;
    s16                  step;
    s16                  scale;
    s32                  shrink;

    work   = task->field_1C;
    player = Game_GetPtrSlot(3);
    actor  = (GameActor*)player->work;
    cfg    = &Player_Status;

    if (D_actor_444000_80144A68 == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    step = ++work->field_1AC;
    if (step < 10) {
        scale  = step * 0x190 + 0x800;
        shrink = 0x800 / step;
        Actor444000_ScaleRotation(task->extra->coords, scale, shrink);
    }

    if (work->field_1AC == 7) {
        Actor444000_ScaleRotation(task->extra->coords, 0x17A0, 0x800);

        gap.vx = task->extra->coords->coord.t[0] -
                 ((TmdObject*)player->extra)->coords->coord.t[0];
        gap.vy = 0;
        gap.vz = task->extra->coords->coord.t[2] -
                 ((TmdObject*)player->extra)->coords->coord.t[2];

        if (Actor444000_OutOfReach(&gap) == 0 && actor->field_954 != 2 &&
            cfg->hp > 0) {
            D_actor_444000_80161898.field_14 = 0x28;
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&D_actor_444000_80161898, 0) == 0) {
                D_actor_444000_80144A6C = 1;
                work->anim.field_0      = D_actor_444000_80161694;
                work->anim.field_4      = 1;
                work->anim.field_8      = 0;
                work->anim.field_C      = 3;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                work->field_1B2 = 1;
            }
        }
    }

    if (work->field_1AC >= 11 && work->field_1B2 == 0) {
        task->state++;
    } else if (work->field_1B2 == 1 && D_actor_444000_80144A6C == 0) {
        task->state++;
    } else if (work->field_1AC >= 0xC9) {
        D_actor_444000_80144A6C = 0;
        task->state++;
    }

    pos.vx = task->extra->coords->workm.t[0];
    pos.vy = task->extra->coords->workm.t[1];
    pos.vz = task->extra->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;
}

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
        task->extra->flags = 0x80;
    }

    if (work->field_1AC >= 9) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
        work->field_1B2 = 0;
        task->state++;
    }
    work->field_1AC++;
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

    task->extra->coords->sub = &Gfx_ViewCoord;
    task->extra->flags       = 0;

    vec.vx = vec.vy = vec.vz = 0;
    Actor444000_LocalToView(&((TmdObject*)owner->task->extra)->coords[3], &vec);

    task->extra->coords->coord.t[0] = vec.vx;
    task->extra->coords->coord.t[1] = vec.vy;
    task->extra->coords->coord.t[2] = vec.vz;
    task->extra->coords->flg        = 0;

    work->field_1AA = task->extra->coords->coord.t[1] / 9;
    work->vel.vx =
        ((TmdObject*)player->extra)->coords->coord.t[0] - task->extra->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        ((TmdObject*)player->extra)->coords->coord.t[2] - task->extra->coords->coord.t[2];
    work->field_1AC = 0;
    task->state++;

    sfx = ((owner->field_8 >> 0xC) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan((GpObj38*)task->extra->coords);
    SndEvt_EnqueueType6(sfx, pan, (s8)Gp_GetObjDepth((GpObj38*)task->extra->coords));

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&task->extra->coords->coord, ((u32)Gp_LcgState >> 0x10) & 0x1FF, 1);

    vec.vx = vec.vy = vec.vz = 0;

    Actor444000_LinkWorkObj(task->extra->coords, &work->obj0, &work->rec0, &vec, 0x100, 3, 1);

    work->obj1.field_8  = task->extra->coords;
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

    task->extra->lightMtx = &work->lightMtx;
    task->extra->colorMtx = &work->colorMtx;
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
        task->extra->flags = 2;
    }

    if (task->extra->coords->coord.t[1] < 0) {
        work->obj0.flags                |= 0x8000;
        work->obj1.flags                |= 0x4000;
        task->extra->coords->coord.t[1] += ABS(work->field_1AA);
    }

    if (task->extra->coords->coord.t[1] >= -0x31) {
        task->extra->coords->coord.t[1] = -0x32;
        work->field_1AC                 = 0;
        pan                             = (s8)Gp_GetObjPan((GpObj38*)task->extra->coords);
        SndEvt_EnqueueType6(0x4020000C, pan, (s8)Gp_GetObjDepth((GpObj38*)task->extra->coords));
        task->state++;
    }

    if (func_actor_444000_80132B14(task->extra->coords, &work->rec1, 3) != 0) {
        work->vel.vz = 0;
        work->vel.vx = 0;
    }

    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x04270000 &&
        task->extra->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    Gp_ClearRec18Occupied(&work->rec1);
    Gp_ClearRec18Occupied(&work->rec0);

    task->extra->coords->coord.t[0] += work->vel.vx / 9;
    task->extra->coords->coord.t[2] += work->vel.vz / 9;
    task->extra->coords->flg         = 0;

    pos.vx = task->extra->coords->workm.t[0];
    pos.vy = task->extra->coords->workm.t[1];
    pos.vz = task->extra->coords->workm.t[2];
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
        work->obj1.flags  &= ~0x4000;
        work->obj0.flags  &= ~0x8000;
        task->extra->flags = 2;
    }

    work->field_1AC++;

    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x04270000 &&
        task->extra->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    if (work->field_1AC < 8) {
        task->extra->coords->coord.t[0] += work->vel.vx;
        task->extra->coords->coord.t[2] += work->vel.vz;
        work->vel.vx                   >>= 1;
        work->vel.vz                   >>= 1;
        task->extra->coords->flg         = 0;
    }

    step = work->field_1AC - 1;
    switch (step) {
        case 3:
        case 7:
            Gp_SpawnEff(0x600A5, task->extra->coords, 1, NULL);
            Gp_SetLightMode((GpObj4C*)enemy, 2);
            break;
        case 0:
        case 1:
        case 19:
            Gp_SpawnEff(0x600A5, task->extra->coords, 1, NULL);
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

    pos.vx = task->extra->coords->workm.t[0];
    pos.vy = task->extra->coords->workm.t[1];
    pos.vz = task->extra->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

/// Horizontal gap from `coord` to the camera target `D_80073B8C`, as an
/// `SVECTOR` the caller supplies.
static __inline__ void Actor444000_GapToCamera(GsCOORDINATE2* coord, SVECTOR* out)
{
    out->vx = D_80073B8C->t[0] - coord->coord.t[0];
    out->vy = D_80073B8C->t[1] - coord->coord.t[1];
    out->vz = D_80073B8C->t[2] - coord->coord.t[2];
}

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F1C`:
/// allocate its work block and pick the point it will be dropped on.
///
/// A spawn with `spawnArg1` 0 rerolls the drop-point group in
/// `D_actor_444000_80161690`, mapping the two low bits of the LCG onto group
/// 1, 1, 2 and 0. `work->target` is then the host model's position pushed out
/// by 0x1B58, 0x2710 or 0x32C8 -- whichever ring the host is on, measured
/// against the camera target -- plus the `[group][spawnArg1]` entry of
/// `D_actor_444000_80161744`, with a 0..0x7F jitter on z. `spawnArg1` 4 drops
/// on the player instead. The model itself is stood up beside the host at the
/// `D_actor_444000_80161704` offset, its work coordinate is parented to
/// `Gfx_ViewCoord` with an identity rotation and carries the single display
/// node, and the spawn cue is enqueued at the model's own pan and depth with
/// the owner's id in its high half. The trailing `Gp_SpawnEff` effect becomes
/// this task's parent so it dies with it.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void func_actor_444000_80139594(GpEnemy* enemy, Actor444000Drop* task)
{
    Actor444000DropWork* work;
    GpEnemy*             owner;
    Task*                parent;
    Task*                player;
    Actor444000Matrix*   mtx;
    SVECTOR              vec;
    s32                  dist;
    s32                  rnd;
    s32                  snd;
    s32                  pan;

    player = Game_GetPtrSlot(3);
    owner  = task->parent->spawnArg2;
    parent = task->parent;

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }
    work           = Mem_Calloc(sizeof(Actor444000DropWork), false);
    task->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->coords->sub = &Gfx_ViewCoord;
    work->field_1AA          = 0;

    Actor444000_GapToCamera(task->extra->coords, &vec);

    if ((u16)task->spawnArg1 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        rnd         = ((u32)Gp_LcgState >> 16) & 3;
        switch (rnd) {
            case 0:
            case 1:
                D_actor_444000_80161690 = 1;
                break;
            case 2:
                D_actor_444000_80161690 = rnd;
                break;
            case 3:
                D_actor_444000_80161690 = 0;
                break;
            default:
                D_actor_444000_80161690 = 0;
                break;
        }
    }

    Actor444000_GapToCamera(((TmdObject*)parent->extra)->coords, &vec);
    dist  = vec.vx * vec.vx;
    dist += vec.vy * vec.vy;
    dist += vec.vz * vec.vz;
    dist  = SquareRoot0(dist);

    if (dist < 0x1F40) {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x1B58;
    } else if (dist < 0x2AF8) {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x2710;
    } else {
        work->target.vx = ((TmdObject*)parent->extra)->coords->coord.t[0] + 0x32C8;
    }

    work->target.vx +=
        D_actor_444000_80161744[D_actor_444000_801617C4[D_actor_444000_80161690]
                                                       [(u16)task->spawnArg1]]
            .vz;
    work->target.vy = 0;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->target.vz = D_actor_444000_80161744[D_actor_444000_801617C4[D_actor_444000_80161690]
                                                                     [(u16)task->spawnArg1]]
                          .vx -
                      0x189C;
    work->target.vz = (((u32)Gp_LcgState >> 16) & 0x7F) + work->target.vz;

    if ((u16)task->spawnArg1 == 4) {
        work->target.vx = ((TmdObject*)player->extra)->coords->coord.t[0];
        work->target.vy = 0;
        work->target.vz = ((TmdObject*)player->extra)->coords->coord.t[2];
    }

    work->timer     = 0;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_1AE = ((u32)Gp_LcgState >> 16) & 8;

    vec.vx = D_actor_444000_80161704[(u16)task->spawnArg1].vx;
    vec.vy = D_actor_444000_80161704[(u16)task->spawnArg1].vy;
    vec.vz = D_actor_444000_80161704[(u16)task->spawnArg1].vz;

    task->extra->coords->coord.t[0] = vec.vx + ((TmdObject*)parent->extra)->coords->coord.t[0];
    task->extra->coords->coord.t[1] = vec.vy;
    task->extra->coords->coord.t[2] = vec.vz + ((TmdObject*)parent->extra)->coords->coord.t[2];

    work->obj.field_18 = Gp_PackObjPair((GpObj50*)owner, 1);

    vec.vx = 0;
    vec.vy = 0;
    vec.vz = 0;

    work->coord.sub    = &Gfx_ViewCoord;
    mtx                = (Actor444000Matrix*)&work->coord.coord;
    mtx->ident.m00_m01 = 0x1000;
    mtx->ident.m02_m10 = 0;
    mtx->ident.m11_m12 = 0x1000;
    mtx->ident.m20_m21 = 0;
    mtx->ident.m22     = 0x1000;
    Gfx_RotMatrixY(&mtx->mat, 0, 1);

    Actor444000_LinkWorkObj(&work->coord, &work->obj, &work->rec, &vec, 0x100, 3, 1);
    work->obj.flags &= 0x7FFF;

    snd = ((owner->field_8 >> 12) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan((GpObj38*)task->extra->coords);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)task->extra->coords));

    work->eff = Gp_SpawnEff(0x6019B, task->extra->coords, 0, NULL);
    if (work->eff != NULL) {
        Task_Reparent((Task*)task, work->eff->field_0);
    }
    task->state++;
}

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

    y                               = task->extra->coords->coord.t[1] - 0x1F4;
    task->extra->coords->coord.t[1] = y - work->field_1AE;
    if (task->extra->coords->coord.t[1] < -0x4E20) {
        task->state++;
        task->extra->coords->coord.t[0] = work->target.vx;
        task->extra->coords->coord.t[2] = work->target.vz;
        Gp_LcgState                     = Gp_LcgState * 5 + 0x71357911;
        task->extra->coords->coord.t[1] = -0x4E20;
        work->timer                     = 0;
        work->field_1AE                 = ((u32)Gp_LcgState >> 16) & 0x1F;
        work->obj.flags                |= 0x8000;
    }

    task->extra->coords->flg = 0;
    work->coord.coord.t[0]   = task->extra->coords->coord.t[0];
    work->coord.coord.t[1]   = task->extra->coords->coord.t[1];
    work->coord.coord.t[2]   = task->extra->coords->coord.t[2];
    work->coord.flg          = 0;
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

    coord.c.coord.t[0] = task->extra->coords->coord.t[0];
    coord.c.coord.t[1] = 0;
    coord.c.coord.t[2] = task->extra->coords->coord.t[2];
    coord.c.flg        = 0;
    Gp_UpdateCoord(&coord.c);

    Gp_DrawEffGroundQuad((VECTOR3*)coord.c.workm.t, (s16)((s16)work->timer * 8 + 0x80),
                         Gp_State1C->field_8);

    if ((s16)work->timer >= 0x14) {
        task->extra->coords->coord.t[1] =
            task->extra->coords->coord.t[1] + (work->field_1AE + 0x258);
        if (task->extra->coords->coord.t[1] > 0) {
            owner                           = task->parent->spawnArg2;
            task->extra->coords->coord.t[1] = 0;
            work->timer                     = 0;
            if (work->eff != NULL) {
                work->eff->field_0->spawnArg1 = 2;
            }
            task->state++;
            snd = ((owner->field_8 >> 12) << 8) | 0x4020000C;
            pan = (s8)Gp_GetObjPan((GpObj38*)task->extra->coords);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)task->extra->coords));
        }
    }

    task->extra->coords->flg = 0;
    Gp_ClearRec18Occupied(&work->rec);
    work->coord.coord.t[0] = task->extra->coords->coord.t[0];
    work->coord.coord.t[1] = task->extra->coords->coord.t[1];
    work->coord.coord.t[2] = task->extra->coords->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131F0C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131F1C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_5", D_actor_444000_80131F30);
