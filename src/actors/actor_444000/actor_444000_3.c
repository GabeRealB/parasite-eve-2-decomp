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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801341C4);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80139EE4);

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F30`:
/// allocate its `Actor444000SpinnerWork`, parent the model object to the world
/// coordinate, give it a random orientation off `Gp_LcgState`, point it at its
/// own light and colour matrices and step the task on. Bails to
/// `Gp_DestroyEnemy` when the overlay is shutting down or the allocation fails.
void func_actor_444000_8013A1C4(GpEnemy* enemy, Actor444000Spinner* task)
{
    Actor444000SpinnerWork* work;

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    work           = Mem_Calloc(0xA0, 0);
    task->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->field_8->sub = &Gfx_ViewCoord;
    task->extra->field_C      = 0;

    switch ((u16)task->spawnArg1) {
        case 0:
            work->spin = 0x14;
            break;
        case 1:
            work->spin = 0x28;
            break;
        case 2:
            work->spin = 0x50;
            break;
        default:
            work->spin = 0x50;
            break;
    }

    task->field_24 = NULL;
    work->field_98 = 0;
    work->field_96 = 0;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixZ(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixX(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);

    task->extra->field_1C     = &work->lightMtx;
    task->extra->field_20     = &work->colorMtx;
    task->extra->field_8->flg = 0;
    Gp_UpdateCoord(task->extra->field_8);
    func_800D7A9C(task->extra, (VECTOR*)task->extra->field_8->workm.t, 0, 3);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013A3AC);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131F0C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131F1C);

INCLUDE_RODATA("actors/nonmatchings/actor_444000/actor_444000_3", D_actor_444000_80131F30);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013A77C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013A958);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013ACD0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013AFF8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013C060);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013C4B0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013CA60);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013D128);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013D810);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013D96C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013E058);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013EC84);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013FB74);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801404C0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80140BBC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80140E28);

void func_actor_444000_8014105C(Actor444000* arg0)
{
    Actor444000Work* work;
    GpEnemy*         obj;
    TmdObject*       tmd;
    s32              state;
    s32              id;
    s32              pan;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        tmd               = (TmdObject*)arg0->extra;
        obj               = arg0->field_20;
        obj->node.field_4 = 8;
        tmd->field_C      = 0;
        state             = work->field_7B3;
        work->field_EF4   = 0;
        work->field_EF6   = 0;
        work->field_EFA   = 1;
        if (state != 0xD) {
            work->field_7B0 = 1;
            work->field_7B3 = 0xD;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        id  = (((u16)obj->field_8 >> 12) << 8) | 0x40200004;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        SndEvt_EnqueueType7((((u16)obj->field_8 >> 12) << 8) | 0x4020000D, 1);
        return;
    }
    SCRATCH_SP -= 0xC;
    if (D_actor_444000_80144A70 >= 0x191) {
        work->field_7A4         = 0;
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    if (work->slots0[1].field_10 & 1) {
        work->field_0 = 0xA;
    }
    SCRATCH_SP += 0xC;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801411C8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80141618);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80141DFC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80142254);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801423C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80142F28);
