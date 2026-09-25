#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/tonfa_baton.h"

/// Near vector for the baton's swing arc.
SVECTOR D_tonfa_baton_8011E0F0[1] = { { 0, 0x0080, 0, 0 } };

/// The far end of that pair, immediately after it. Both forms appear in
/// the original: one path reaches it as `D_tonfa_baton_8011E0F0[1]`, which compiles to the
/// array's address plus 8, and another names it directly, which compiles
/// to its own address - so it has to be a separate object, not element 1.
SVECTOR D_tonfa_baton_8011E0F8 = { 0, -0x0200, 0, 0 };

void func_tonfa_baton_8011DB78(Task* task);

void func_tonfa_baton_8011D1EC(Task* task)
{
    GpCoord    local;
    GpCoord*   coord;
    GpCoord*   dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;
    s32        flags;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                coord->sub        = work->parent;
                coord->coord.t[0] = D_tonfa_baton_8011E0F0[0].vx;
                coord->coord.t[1] = D_tonfa_baton_8011E0F0[0].vy;
                coord->coord.t[2] = D_tonfa_baton_8011E0F0[0].vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->state      = 1;
                vec              = &D_tonfa_baton_8011E0F0[1];
                local.sub        = coord;
                local.coord.t[0] = vec->vx;
                local.coord.t[1] = vec->vy;
                local.coord.t[2] = vec->vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                for (i = 0; i < 8; i++) {
                    dst        = &D_tonfa_baton_8012BBEC[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &D_tonfa_baton_8012BE6C[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = local.workm;
                    gte_SetRotMatrix(&local.workm);
                    gte_SetTransMatrix(&local.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                flags = 0x13;
                if (task->spawnArg1 == 0) {
                    flags = 1;
                }
                D_tonfa_baton_8012C0EC = flags;
                break;
            case 1:
                coord->flg = 0;
                Gp_UpdateCoord(coord);
                local.sub        = work->parent;
                local.coord.t[0] = D_tonfa_baton_8011E0F8.vx;
                local.coord.t[1] = D_tonfa_baton_8011E0F8.vy;
                local.coord.t[2] = D_tonfa_baton_8011E0F8.vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                dst        = &D_tonfa_baton_8012BBEC[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &D_tonfa_baton_8012BE6C[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = local.workm;
                gte_SetRotMatrix(&local.workm);
                gte_SetTransMatrix(&local.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &D_tonfa_baton_8012BBEC[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &D_tonfa_baton_8012BE6C[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_tonfa_baton_8011D6B0(work->age & 7, D_tonfa_baton_8012C0EC);
                break;
        }
        if (work->age >= 0x1F) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

void func_tonfa_baton_8011D6B0(s16 slot, s16 flags)
{
    TonfaBeamScratch* blk;
    GpCoord*          a;
    GpCoord*          b;
    POLY_G4*          prim;
    s32               i;
    s32               j;
    s32               i0;
    s32               i1;
    s32               hi;
    s32               lo;
    s32               fade;

    SCRATCH_PUSH_BYTES(sizeof(TonfaBeamScratch));
    blk = SCRATCH_HEAD(TonfaBeamScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &D_tonfa_baton_8012BBEC[i0];
        blk->v[0].vx = (u16)a->workm.t[0];
        blk->v[0].vy = (u16)a->workm.t[1];
        b            = &D_tonfa_baton_8012BE6C[i0];
        blk->v[0].vz = (u16)a->workm.t[2];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        a            = &D_tonfa_baton_8012BBEC[i1];
        blk->v[1].vz = (u16)b->workm.t[2];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        b            = &D_tonfa_baton_8012BE6C[i1];
        blk->v[2].vz = (u16)a->workm.t[2];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade = 0x40 - i * 9;
            hi   = fade & 0xFF;
            lo   = (fade - 9) & 0xFF;
            setRGB0(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB1(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB2(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            setRGB3(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    SCRATCH_POP_BYTES(sizeof(TonfaBeamScratch));
}

void func_tonfa_baton_8011DA48(Task* arg0)
{
    TmdObject* extra;
    s32*       ptr;

    extra              = arg0->extra.tmd;
    ptr                = extra->coords;
    arg0->state        = arg0->state + 1;
    arg0->exitCallback = func_tonfa_baton_8011DB78;
    *ptr               = 0;
    extra->flags       = 0;
}

void func_tonfa_baton_8011DA74(Task* arg0)
{
    TmdObject* extra;
    GpCoord*   coord;
    GameActor* actor;
    s32        mode;

    extra        = arg0->extra.tmd;
    coord        = extra->coords;
    actor        = gameGetPtrSlot(3)->work;
    coord->flg   = 0;
    extra->flags = (gameGetPtrSlot(3))->extra.tmd->flags;

    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0x60;
    coord->coord.t[2] = 0;

    if (*(u32*)&actor->field_954 != 0x40000) {
        arg0->spawnArg1 = 0;
    }

    mode = arg0->spawnArg1 & 0xF;
    switch (mode) {
        case 0:
            if (coord->param.rot.vz > 0) {
                coord->param.rot.vz = coord->param.rot.vz - 0x100;
            }
            break;
        case 1:
            if (coord->param.rot.vz < 0x800) {
                coord->param.rot.vz = coord->param.rot.vz + 0x1C0;
            }
            break;
    }
    Gfx_RotMatrixZ(&coord->coord, coord->param.rot.vz, 1);
}

void func_tonfa_baton_8011DB6C(Task* arg0)
{
    arg0->state = 3;
}

/// Exit callback: kills the task.
void func_tonfa_baton_8011DB78(Task* task)
{
    taskKill(task);
}

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_tonfa_baton_8011DB98(Task* arg0)
{
    TaskFunc states[4] = {
        func_tonfa_baton_8011DA48,
        func_tonfa_baton_8011DA74,
        func_tonfa_baton_8011DB6C,
        func_tonfa_baton_8011DB78,
    };

    states[arg0->state](arg0);
}

/// Per-frame swing state machine for the tonfa baton, and the only weapon here
/// that moves the player: while `field_973` is set the third column of the
/// weapon coordinate is scaled by 1/84 and added to the coordinate's
/// translation, which is what carries the lunge. Case 0 arms the swing (8-tick
/// wind-up) and queues the ready animation. Cases 1 and 2 run the wind-up: on
/// the tick it expires the weapon becomes solid, the swing report plays and the
/// trail effect is parented to the weapon task; pressing again during the
/// window (`field_966 & 0xA`) upgrades to the second swing, which case 2 turns
/// into the follow-through, otherwise the state falls back to the 10-tick
/// recovery of case 5. Case 3 is the follow-through: it re-arms the hitbox
/// three ticks in and parks in case 4, whose 9 ticks clear the hit flag again.
/// Cases 1/2 and 4 also play the connect sound once per swing when
/// `Gp_CountRec18Hi` reports a hit.
void func_tonfa_baton_8011DBFC(Task* arg0)
{
    GameActor*  actor;
    GpCoord*    coord;
    TonfaSwing* swing;
    GpEffWork*  eff;
    s32         delay;
    s32         step;
    s32         fade;
    s32         swinging;

    swinging = 0;
    actor    = arg0->work;
    SCRATCH_PUSH_BYTES(0x18);
    swing = SCRATCH_HEAD(TonfaSwing);
    switch (actor->field_95E) {
        case 0:
            actor->field_956 = 4;
            actor->field_95E = 1;
            actor->field_954 = 0;
            actor->field_958 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_934 = 8;
            actor->field_93E = 0;
            func_80106518(0x13);
            actor->field_124 = 0x21317;
            actor->field_12A = (actor->field_12A & 0xF7FF) | 0x400;
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 3);
            break;
        case 1:
        case 2:
            delay = actor->field_934;
            if (delay == 0) {
                actor->field_973 = 1;
                swinging         = 1;
                if (actor->field_966 & 0xA) {
                    actor->field_95E = 2;
                }
            } else {
                delay--;
                actor->field_934 = delay;
                if (delay == 0) {
                    actor->field_91C->spawnArg1 = 1;
                    actor->field_12A           |= 0x8000;
                    func_80106238(arg0, 0, 0);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20130001, 0);
                    eff = Gp_SpawnEff(0x6003A,
                                      actor->field_91C->extra.tmd->coords,
                                      0, NULL);
                    if (eff != NULL) {
                        Task_Reparent(actor->field_91C, eff->task);
                    }
                }
            }
            if (actor->field_93E != 1 && Gp_CountRec18Hi(actor->field_32C, 0x30000) != 0) {
                actor->field_93E = 1;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20130003, 0);
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                actor->field_12A &= 0x7FFF;
                if (actor->field_95E == 2) {
                    actor->field_95E = 3;
                    actor->field_934 = 0xC;
                    func_80106518(0x13);
                    actor->field_124 = 0x21315;
                    eff              = Gp_SpawnEff(
                        0x6003A, actor->field_91C->extra.tmd->coords, 1,
                        NULL);
                    if (eff != NULL) {
                        Task_Reparent(actor->field_91C, eff->task);
                    }
                    Gp_AnimResetChildSlots(arg0, 0xB);
                } else {
                    actor->field_95E            = 5;
                    actor->field_934            = 0xA;
                    actor->field_91C->spawnArg1 = 0;
                    Gp_AnimResetChildSlots(arg0, 0xE);
                }
            }
            break;
        case 3:
            if (actor->field_934 != 0) {
                actor->field_973 = 1;
                swinging         = 1;
                step             = actor->field_934 - 1;
                actor->field_934 = step;
                if (step == 3) {
                    actor->field_12A |= 0x8000;
                    func_80106238(arg0, 0, 1);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20130002, 0);
                } else if (step == 0) {
                    actor->field_95E            = 4;
                    actor->field_934            = 9;
                    actor->field_91C->spawnArg1 = 0;
                }
            }
            /* fallthrough */
        case 4:
            if (actor->field_95E == 4) {
                fade = actor->field_934;
                fade--;
                actor->field_934 = fade;
                if (fade == 0) {
                    actor->field_12A &= 0x7FFF;
                }
            }
            if (actor->field_93E != 2 && Gp_CountRec18Hi(actor->field_32C, 0x30000) != 0) {
                actor->field_93E = 2;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20130004, 0);
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
        case 5:
            if (actor->field_934 != 0) {
                actor->field_973 = 1;
                swinging         = 1;
                actor->field_934 = actor->field_934 - 1;
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    coord = arg0->extra.tmd->coords;
    Gfx_MatrixCol2(&coord->coord, &swing->dir);
    swing->vx          = (s16)(swing->dir.vx / 84) * swinging;
    swing->vy          = (s16)(swing->dir.vy / 84) * swinging;
    swing->vz          = (s16)(swing->dir.vz / 84) * swinging;
    coord->coord.t[0] += swing->vx;
    coord->coord.t[1] += swing->vy;
    coord->coord.t[2] += swing->vz;
    SCRATCH_POP_BYTES(0x18);
}
