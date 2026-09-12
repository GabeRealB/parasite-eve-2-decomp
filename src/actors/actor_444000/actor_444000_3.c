#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s16 D_actor_444000_80144A68;
extern s16 D_actor_444000_80144A70;
extern s32 Gp_LcgState;

extern s8         D_8007218A;
extern u8         D_80073BA9;
extern GpAnimSet* D_actor_444000_80161694[];
extern GpAnimBlk* Gp_PlayerAnimBlkTbl[];
extern u16        Gp_WeaponIdBase[];

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80134040);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013441C);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013482C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80135448);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801371E8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80137594);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013799C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80137D4C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801381B0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80138490);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80138B94);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80138FC4);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131E90);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131E9C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131EA8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013928C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80139594);

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

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131F0C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131F1C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131F30);
