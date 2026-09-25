#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/light.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/rooms_shared_8017d830.h"
#include "rooms/acropolis_helicopter_landing_pad.h"

/// 0x20 scratch block `func_acropolis_helicopter_landing_pad_80180A64` takes
/// from `G_SCRATCH_HEAD` for one spark line. `a` / `b` are the two random
/// endpoints, rotated by the coord's `workm` and offset by its translation;
/// `otz` is `SZ3 >> 2` of the second `RTPS`, `flag` the GTE flag word (bit 31
/// rejects the line), and `x0..y1` the two projected screen points.
typedef struct AhlpSparkScratch {
    /* 0x00 */ SVECTOR a;
    /* 0x08 */ SVECTOR b;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s32     flag;
    /* 0x18 */ u16     x0;
    /* 0x1A */ u16     y0;
    /* 0x1C */ u16     x1;
    /* 0x1E */ u16     y1;
} AhlpSparkScratch;
STATIC_ASSERT_SIZEOF(AhlpSparkScratch, 0x20);

/// 0x1C scratch block `func_acropolis_helicopter_landing_pad_80181064` takes
/// from `G_SCRATCH_HEAD` for one lens-flare sprite. `pos` is the coord's
/// world translation, `otz` is `SZ3 >> 2` of the `RTPS`, `flag` the GTE flag
/// word (bit 31 rejects the sprite), `sx` / `sy` the projected centre and
/// `dx` / `dy` the rotated half-extents of the quad's two diagonals.
typedef struct AhlpFlareScratch {
    /* 0x00 */ SVECTOR pos;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ u16     sx;
    /* 0x1A */ u16     sy;
} AhlpFlareScratch;
STATIC_ASSERT_SIZEOF(AhlpFlareScratch, 0x1C);

extern SVECTOR   D_acropolis_helicopter_landing_pad_80184E80[12];
extern s32       D_acropolis_helicopter_landing_pad_80184EE0[12];
extern GpSaveLoc D_acropolis_helicopter_landing_pad_80187F90;

void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0);
void func_acropolis_helicopter_landing_pad_8017EE2C(Task* arg0);
void func_acropolis_helicopter_landing_pad_8017F010(SVECTOR* pos, s16 index, s32 level);
void func_acropolis_helicopter_landing_pad_80180664(GpCoord* coord);
void func_acropolis_helicopter_landing_pad_80180A64(GpCoord* coord);

/// The whole-unit delta of the last step
/// `func_acropolis_helicopter_landing_pad_801819C0` applied.
extern SVECTOR D_acropolis_helicopter_landing_pad_80187F88;

void func_acropolis_helicopter_landing_pad_8017EB58(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slot         = Stream_FindSlot(key.raw.data, 0, 0);
    slotParam[0] = slot;
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
        return;
    }
    Mem_Set(Fs_ImgBuffers, 0, 0x25800);
    SetDispMask(1);
    taskKill(task);
    Display_ResetHeapWrapper();
}

void func_acropolis_helicopter_landing_pad_8017ED00(Task* arg0)
{
    Display_SpawnWithOt(D_acropolis_helicopter_landing_pad_80184E68, 1, 0, 0);
    gDisplayState.at100.flags.flipMode = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

/// Asks the slot-7 task to warp to stage 0xF, room 3 (message 0x13EE with the
/// room's `GpSaveLoc`); advances on success, otherwise kills the task.
void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0)
{
    Task* slot = gameGetPtrSlot(7);

    D_acropolis_helicopter_landing_pad_80187F90.field_4 = 1;
    D_acropolis_helicopter_landing_pad_80187F90.field_3 = 1;
    *(u16*)&D_acropolis_helicopter_landing_pad_80187F90 = 0xF;
    D_acropolis_helicopter_landing_pad_80187F90.field_2 = 3;
    D_acropolis_helicopter_landing_pad_80187F90.field_5 = 0;
    if (Gp_DispatchMsg(slot, 0x13EE, (s32)&D_acropolis_helicopter_landing_pad_80187F90,
                       (s32)&D_acropolis_helicopter_landing_pad_80187F90) != 0) {
        arg0->state += 1;
    } else {
        taskKill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EDD4(Task* arg0)
{
    GpXformArg msg;
    Task*      slot;

    slot       = gameGetPtrSlot(3);
    msg.rot.vx = 0;
    msg.rot.vy = 0;
    msg.rot.vz = 0;
    Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
    arg0->state = arg0->state + 1;
}

/// Task state step: advances the state once msg 0x3F0 to slot 3 returns 0.
void func_acropolis_helicopter_landing_pad_8017EE2C(Task* arg0)
{
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_acropolis_helicopter_landing_pad_8017EE80(Task* arg0)
{
    GpFacingArg args;
    Task*       slot;

    slot         = gameGetPtrSlot(3);
    args.field_0 = 1;
    args.field_4 = 3;
    Gp_DispatchMsg(slot, 0x3EF, (s32)&args, 0);
    arg0->state = arg0->state + 1;
}

void func_acropolis_helicopter_landing_pad_8017EEDC(Task* arg0)
{
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        Mc_SaveData.at4.loc.area = D_acropolis_helicopter_landing_pad_80187F90.field_0;
        Mc_SaveData.at4.loc.warp = D_acropolis_helicopter_landing_pad_80187F90.field_2;
        Mc_SaveData.at4.loc.room = D_acropolis_helicopter_landing_pad_80187F90.field_3;
        Task_Spawn(0, 0x11, 0, 0);
        taskKill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EF60(void)
{
    Task_Spawn(2, 0xF, 0, 0);
}

/// Five-state dispatcher of the room's intro task; the handler table is built
/// on the stack. Marks the player actor's `field_930` as 2 before every step.
void func_acropolis_helicopter_landing_pad_8017EF8C(Task* arg0)
{
    GameActor* actor     = (GameActor*)(gameGetPtrSlot(3))->work;
    TaskFunc   states[5] = {
        func_acropolis_helicopter_landing_pad_8017ED50,
        func_acropolis_helicopter_landing_pad_8017EDD4,
        func_acropolis_helicopter_landing_pad_8017EE2C,
        func_acropolis_helicopter_landing_pad_8017EE80,
        func_acropolis_helicopter_landing_pad_8017EEDC,
    };

    actor->field_930 = 2;
    states[arg0->state](arg0);
}

/// Draws one helipad floodlight glow. Light `index` owns transient light slot
/// `6 + (index & 1)`; the light is skipped while `Gp_State1C->eventState` is
/// non-zero (switching the slot off once it reaches 4) and unless the
/// current view's bit is set in the light's
/// `D_acropolis_helicopter_landing_pad_80184EE0` mask. Otherwise `pos` is
/// projected through `gGfxViewCoord.workm` into a `G_SCRATCH_HEAD` block and,
/// when the GTE flag word is clean, the record is refreshed and two rings of
/// flat-shaded `POLY_G4` fans are linked into the OT at the light's `otz`: 16
/// wedges of the outer radius (a dim `level >> 1` layer under a `level` one)
/// and four inner-radius blades whose intensity is `level >> 1`.
void func_acropolis_helicopter_landing_pad_8017F010(SVECTOR* pos, s16 index, s32 level)
{
    GpCoord64*         light;
    GpPointLight*      work;
    void**             scratch;
    u8*                head;
    RoomDraw05Scratch* blk;
    POLY_G4*           prim;
    s32                a;
    s32                b;
    s32                c;
    s32                d;
    s16                lvl;
    s32                half;
    s32                mask;

    lvl   = level;
    light = &Gp_RoomCoords[6 + (index & 1)];
    work  = &light->data.light;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            light->framesLeft = 0;
        }
    } else {
        mask = D_acropolis_helicopter_landing_pad_80184EE0[index] & (1 << ((Gp_GetViewIndex() & 0xFF) - 1));
        if (mask == 0) {
            return;
        }
        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        *scratch = head - 0x14;
        blk      = (RoomDraw05Scratch*)(head - 0x14);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(pos);
        gte_rtps();
        gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
        gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            light->framesLeft          = 2;
            work->inner                = 0x640;
            work->outer                = 0x3200;
            work->head.r               = level * 16;
            work->head.g               = 0;
            work->head.b               = 0;
            work->head.u.at.local.t[0] = pos->vx;
            work->head.u.at.local.t[1] = pos->vy;
            work->head.u.at.local.t[2] = pos->vz;
            light->data.coord.flg      = 0;
            blk->rOuter                = 0xC000 / ((RoomDraw05Scratch*)(head - 0x14))->otz;
            blk->rInner                = 0x1800 / ((RoomDraw05Scratch*)(head - 0x14))->otz;

            for (a = 0; a < 0x1000; a += 0x200) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                half = lvl >> 1;
                setRGB2(prim, half, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * rsin(a)) >> 12);
                prim->y0 = blk->sy + ((blk->rOuter * rcos(a)) >> 12);
                b        = a + 0x100;
                prim->x1 = blk->sx + ((blk->rOuter * rsin(b)) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * rcos(b)) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                c        = a + 0x200;
                prim->x3 = blk->sx + ((blk->rOuter * rsin(c)) >> 12);
                prim->y3 = blk->sy + ((blk->rOuter * rcos(c)) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * rsin(a)) >> 13);
                prim->y0 = blk->sy + ((blk->rOuter * rcos(a)) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * rsin(b)) >> 13);
                prim->y1 = blk->sy + ((blk->rOuter * rcos(b)) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * rsin(c)) >> 13);
                prim->y3 = blk->sy + ((blk->rOuter * rcos(c)) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }

            lvl = half;
            for (a = 0x200; a < 0x1000; a += 0x800) {
                d              = a - 0x400;
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rInner * rsin(d)) >> 13);
                prim->y0 = blk->sy + ((blk->rInner * rcos(d)) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * rsin(a)) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * rcos(a)) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                d        = a + 0x400;
                prim->x3 = blk->sx + ((blk->rInner * rsin(d)) >> 13);
                prim->y3 = blk->sy + ((blk->rInner * rcos(d)) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rInner * rsin(a)) >> 12);
                prim->y0 = blk->sy + ((blk->rInner * rcos(a)) >> 12);
                prim->x1 = blk->sx + ((blk->rOuter * rsin(d)) >> 11);
                prim->y1 = blk->sy + ((blk->rOuter * rcos(d)) >> 11);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                d        = a + 0x800;
                prim->x3 = blk->sx + ((blk->rInner * rsin(d)) >> 12);
                prim->y3 = blk->sy + ((blk->rInner * rcos(d)) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
        SCRATCH_POP_BYTES(0x14);
    }
}

/// Draws one helipad ember / spark sprite. `spawnArg1` non-zero spawns the
/// bright variant (`scale` 0x300..0x3FF, no drift beyond a fixed -0x18 on
/// Y, `step` 2..5 with `period` up to 0x3F); zero spawns the dim one
/// (`scale` 0x100..0x1FF, random 3D drift, `step` / `period` 1..4).
/// The sprite lives `step * 6` frames counted in `age`. Each frame
/// the coord's translation is projected through `GsWSMATRIX` into a
/// semi-transparent `POLY_FT4` (tpage 0x2B, clut 0x4383, one of the 32x32
/// cells on row 0x28) whose corners are the projected centre plus / minus
/// `scale * 31 / otz` rotated by `angle` and `angle + 0x400`. A
/// bright sprite (`spawnArg1 == 1`) flickers a random green / blue-white tint
/// on 1-in-4 LCG rolls and, before its last two frames, fires a 0x600E0
/// effect on 1-in-16. While `Gp_State1C->eventState` is 0 the coord drifts,
/// `scale` grows by `period` and the frame counter advances until it
/// expires, which releases the state-1C memory; `field_4 >= 4` releases it at
/// once and 2..3 idles.
void func_acropolis_helicopter_landing_pad_8017FA30(Task* arg0)
{
    GpEffWork*        mem;
    GpCoord*          coord;
    void**            scratch;
    u8*               head;
    AhlpFlareScratch* blk;
    POLY_FT4*         prim;
    u32               tmp;
    s16               n;

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            if (arg0->spawnArg1 != 0) {
                mem->move.vx = 0;
                mem->move.vy = -0x18;
                mem->move.vz = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->scale   = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x300;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->period  = ((u32)Gp_LcgState >> 16) & 0x3F;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->step    = (((u32)Gp_LcgState >> 16) & 3) + 2;
            } else {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->scale   = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x100;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->period  = (((u32)Gp_LcgState >> 16) & 3) + 1;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->step    = (((u32)Gp_LcgState >> 16) & 3) + 1;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 7) - 4;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = ~(((u32)Gp_LcgState >> 16) & 0xF);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = (((u32)Gp_LcgState >> 16) & 7) - 4;
            }
            arg0->state++;
        }
        scratch     = (void**)G_SCRATCH_HEAD;
        head        = *scratch;
        *scratch    = head - 0x1C;
        blk         = (AhlpFlareScratch*)(head - 0x1C);
        blk->pos.vx = coord->workm.t[0];
        blk->pos.vy = coord->workm.t[1];
        blk->pos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->pos);
        gte_rtps();
        gte_stsxy(&((AhlpFlareScratch*)(head - 0x1C))->sx);
        gte_stflg(&((AhlpFlareScratch*)(head - 0x1C))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&((AhlpFlareScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            if (arg0->spawnArg1 == 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
                    setRGB0(prim, tmp >> 1, tmp, 0xFF);
                } else {
                    prim->code |= 1;
                }
                if (mem->age < mem->step * 6 - 2) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && Gp_State1C->eventState == 0) {
                        Gp_SpawnEff(0x600E0, coord, 0x100, NULL);
                    }
                }
            } else {
                prim->code = 0x2D;
            }
            prim->tpage = 0x2B;
            prim->clut  = 0x4383;
            prim->code |= 2;
            prim->u0    = (mem->age / mem->step + 1) * 0x20;
            prim->v0    = 0x28;
            prim->u1    = (mem->age / mem->step + 1) * 0x20 + 0x1F;
            prim->v1    = 0x28;
            prim->u2    = (mem->age / mem->step + 1) * 0x20;
            prim->v2    = 0x47;
            prim->u3    = (mem->age / mem->step + 1) * 0x20 + 0x1F;
            prim->v3    = 0x47;
            blk->dx     = ((mem->scale * 0x1F / blk->otz) * rsin(mem->angle)) >> 12;
            blk->dy     = ((mem->scale * 0x1F / blk->otz) * rcos(mem->angle)) >> 12;
            prim->x0    = blk->sx + (u16)blk->dx;
            prim->x3    = blk->sx - (u16)blk->dx;
            prim->y0    = blk->sy - (u16)blk->dy;
            prim->y3    = blk->sy + (u16)blk->dy;
            blk->dx     = ((mem->scale * 0x1F / blk->otz) * rsin(mem->angle + 0x400)) >> 12;
            blk->dy     = ((mem->scale * 0x1F / blk->otz) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1    = blk->sx + (u16)blk->dx;
            prim->x2    = blk->sx - (u16)blk->dx;
            prim->y1    = blk->sy - (u16)blk->dy;
            prim->y2    = blk->sy + (u16)blk->dy;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        }
        SCRATCH_POP_BYTES(0x1C);
        if (Gp_State1C->eventState == 0) {
            coord->coord.t[0] += mem->move.vx;
            coord->coord.t[1] += mem->move.vy;
            coord->coord.t[2] += mem->move.vz;
            coord->flg         = 0;
            mem->scale        += mem->period;
            n                  = mem->age + 1;
            mem->age           = n;
            if (n > mem->step * 6 - 1) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
        }
    }
}

/// Effect task for the helipad floodlights anchored to `Gp_RoomCoords[4]` and
/// `[5]`. On first run it parents the coord to the work's `parent` and
/// positions it from `pos`. State 0 rolls 0-3 spawns of
/// `func_acropolis_helicopter_landing_pad_80180664`, a 1-in-4 roll of
/// `func_acropolis_helicopter_landing_pad_80180A64`, and claims slot 4 as a
/// light (refcount 4). State 1 (also reached by fallthrough) rearms
/// `scale` on a 1-in-4 roll every 8th frame; when armed it plays sound
/// `0x51100001` panned at the coord, spawns one 0x6003B and six 0x600A4
/// effects reparented under this task, and claims slot 5 as a light. State 2
/// releases the state-1C memory, the only step taken while
/// `Gp_State1C->eventState` is set.
void func_acropolis_helicopter_landing_pad_801802E0(Task* arg0)
{
    GpEffWork*    mem;
    GpCoord*      coord;
    GpCoord64*    base;
    GpPointLight* slot;
    GpEffWork*    eff;
    s32           i;
    s32           n;
    s32           pan;

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (arg0->state == 2) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (mem->index == 0) {
        coord->sub        = mem->parent;
        coord->coord.t[0] = mem->pos.vx;
        coord->coord.t[1] = mem->pos.vy;
        coord->coord.t[2] = mem->pos.vz;
        coord->flg        = 0;
        Gp_UpdateCoord(coord);
        mem->scale = 1;
        mem->index++;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            n           = ((u32)Gp_LcgState >> 16) & 3;
            for (i = 0; i < n; i++) {
                func_acropolis_helicopter_landing_pad_80180664(coord);
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                if (Gp_State1C->eventState == 0) {
                    func_acropolis_helicopter_landing_pad_80180A64(coord);
                }
            }
            base             = &Gp_RoomCoords[4];
            slot             = &base->data.light;
            base->framesLeft = 4;
            slot->inner      = 0x15E0;
            slot->outer      = 0x1900;
            slot->head.r     = 0x800;
            slot->head.g     = 0x800;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            slot->head.b     = (((u32)Gp_LcgState >> 16) & 0x700) + 0x900;
            Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &base->data.coord.coord);
            base->data.coord.flg = 0;
            /* fallthrough */
        case 1:
            if ((gDisplayState.animFrame & 7) == 0) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    mem->scale = 1;
                }
            }
            if (mem->scale != 0) {
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(0x51100001, pan, (s8)gpGetObjDepth(coord));
                mem->scale = 0;
                eff        = Gp_SpawnEff(0x6003B, coord, 0x200, NULL);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->task);
                }
                for (i = 0; i < 6; i++) {
                    eff = Gp_SpawnEff(0x600A4, coord, 1, NULL);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->task);
                    }
                }
                base             = &Gp_RoomCoords[5];
                slot             = &base->data.light;
                base->framesLeft = 4;
                slot->inner      = 0xFA0;
                slot->outer      = 0x12C0;
                slot->head.r     = 0xC00;
                slot->head.g     = 0xC00;
                slot->head.b     = 0x600;
                Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &base->data.coord.coord);
                base->data.coord.flg = 0;
            }
            break;
        case 2:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Draws one random spark line off the floodlight coord, the same shape as
/// `func_acropolis_helicopter_landing_pad_80180A64` with a different box:
/// `a` is rolled 64 wide and 128 tall hanging 0xC0..0x41 below the coord,
/// `b` is centred (128 wide, 255 tall via an LCG modulo). Both are rotated by
/// the coord's `workm`, offset by its translation and projected through
/// `GsWSMATRIX` into a semi-transparent `LINE_F2` whose green is an LCG byte
/// and red half of it. Nothing is queued when the GTE flag word is negative.
void func_acropolis_helicopter_landing_pad_80180664(GpCoord* coord)
{
    void**            scratch;
    u8*               head;
    AhlpSparkScratch* blk;
    LINE_F2*          prim;
    SVECTOR*          vec;
    u32               tmp;
    u16               lvl;

    Gp_UpdateCoord(coord);
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    *scratch    = head - 0x20;
    blk         = (AhlpSparkScratch*)(head - 0x20);
    vec         = &blk->a;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vy   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0xC0;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    __asm__("" : "+r"(vec));
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(vec);
    gte_rtv0();
    gte_stsv(vec);
    blk->a.vx   = (u16)blk->a.vx + (u16)coord->workm.t[0];
    blk->a.vy   = (u16)blk->a.vy + (u16)coord->workm.t[1];
    blk->a.vz   = (u16)blk->a.vz + (u16)coord->workm.t[2];
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vx   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0x40;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vy   = (((u32)Gp_LcgState >> 16) % 0xFF) - 0x80;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vz   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0x40;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtv0();
    gte_stsv(&((AhlpSparkScratch*)(head - 0x20))->b);
    blk->b.vx = (u16)blk->b.vx + (u16)coord->workm.t[0];
    blk->b.vy = (u16)blk->b.vy + (u16)coord->workm.t[1];
    blk->b.vz = (u16)blk->b.vz + (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x0);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtps();
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
    lvl         = tmp;
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x1);
    gte_stflg(&((AhlpSparkScratch*)(head - 0x20))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&((AhlpSparkScratch*)(head - 0x20))->otz);
        prim           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineF2(prim);
        setRGB0(prim, tmp >> 1, lvl, 0xFF);
        prim->x0 = blk->x0;
        prim->y0 = blk->y0;
        prim->x1 = blk->x1;
        prim->y1 = blk->y1;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    SCRATCH_POP_BYTES(0x20);
}

/// Draws one random spark line off the floodlight coord: two endpoints are
/// rolled from the LCG (`a` in a 64x128x64 box, `b` in 64x256x64), rotated by
/// the coord's `workm` and offset by its translation, then projected through
/// `GsWSMATRIX` into a semi-transparent `LINE_F2` whose green is an LCG byte
/// and red half of it. Nothing is queued when the GTE flag word is negative.
void func_acropolis_helicopter_landing_pad_80180A64(GpCoord* coord)
{
    void**            scratch;
    u8*               head;
    AhlpSparkScratch* blk;
    LINE_F2*          prim;
    SVECTOR*          vec;
    u32               tmp;
    u16               lvl;

    Gp_UpdateCoord(coord);
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    *scratch    = head - 0x20;
    blk         = (AhlpSparkScratch*)(head - 0x20);
    vec         = &blk->a;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vy   = ((u32)Gp_LcgState >> 16) & 0x7F;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    __asm__("" : "+r"(vec));
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(vec);
    gte_rtv0();
    gte_stsv(vec);
    blk->a.vx   = (u16)blk->a.vx + (u16)coord->workm.t[0];
    blk->a.vy   = (u16)blk->a.vy + (u16)coord->workm.t[1];
    blk->a.vz   = (u16)blk->a.vz + (u16)coord->workm.t[2];
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vy   = ((u32)Gp_LcgState >> 16) & 0xFF;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtv0();
    gte_stsv(&((AhlpSparkScratch*)(head - 0x20))->b);
    blk->b.vx = (u16)blk->b.vx + (u16)coord->workm.t[0];
    blk->b.vy = (u16)blk->b.vy + (u16)coord->workm.t[1];
    blk->b.vz = (u16)blk->b.vz + (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x0);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtps();
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
    lvl         = tmp;
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x1);
    gte_stflg(&((AhlpSparkScratch*)(head - 0x20))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&((AhlpSparkScratch*)(head - 0x20))->otz);
        prim           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineF2(prim);
        setRGB0(prim, tmp >> 1, lvl, 0xFF);
        prim->x0 = blk->x0;
        prim->y0 = blk->y0;
        prim->x1 = blk->x1;
        prim->y1 = blk->y1;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    SCRATCH_POP_BYTES(0x20);
}

/// Effect task for the helipad beacon anchored to `Gp_RoomCoords[4]`. State 0
/// spawns two 0x6005E effects, takes the slot (refcount 4) and seeds its
/// light parameters from the coord and an LCG draw; state 1 spawns two more
/// with arg 0; state 2 fires a 0x6005A effect on 1-in-16 LCG rolls every
/// 64th frame; state 3 releases the state-1C memory. Idle while
/// `Gp_State1C->eventState` is set.
void func_acropolis_helicopter_landing_pad_80180E40(Task* arg0)
{
    GpEffWork*    mem;
    GpCoord*      coord;
    GpCoord64*    base;
    GpPointLight* slot;

    base  = &Gp_RoomCoords[4];
    slot  = &base->data.light;
    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if (arg0->state == 3) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (Gp_State1C->eventState != 0 && arg0->state < 3) {
        return;
    }
    Gp_UpdateCoord(coord);
    switch (arg0->state) {
        case 0:
            Gp_SpawnEff(0x6005E, coord, 1, NULL);
            Gp_SpawnEff(0x6005E, coord, 1, NULL);
            base->framesLeft              = 4;
            slot->inner                   = 0x1900;
            slot->outer                   = 0x1C20;
            slot->head.r                  = 0x800;
            slot->head.g                  = 0x800;
            Gp_LcgState                   = Gp_LcgState * 5 + 0x71357911;
            slot->head.b                  = (((u32)Gp_LcgState >> 16) & 0x700) + 0x900;
            slot->head.u.coord.coord.t[0] = coord->coord.t[0];
            slot->head.u.coord.coord.t[1] = coord->coord.t[1];
            slot->head.u.coord.coord.t[2] = coord->coord.t[2];
            base->data.coord.flg          = 0;
            break;
        case 1:
            Gp_SpawnEff(0x6005E, coord, 0, NULL);
            Gp_SpawnEff(0x6005E, coord, 0, NULL);
            break;
        case 2:
            if (gDisplayState.animFrame & 0x40) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                    Gp_SpawnEff(0x6005A, coord, 2, NULL);
                }
            }
            break;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Effect task for one helipad lens flare. State 0 seeds the `GpEffWork`
/// from the LCG: a 0x200..0x3FF radius (`scale`), a 12-bit angle
/// (`angle`), a 1..4 lifetime scale (`step`, the flare lives
/// `step * 6` frames counted in `age`) and a per-frame drift
/// (`move` / `move.vy` / `move.vz`). Each frame the coord's translation
/// is projected through `GsWSMATRIX` into a semi-transparent `POLY_FT4`
/// (tpage 0x2B, clut 0x4384, one of `step` 40x40 cells on row 0x48)
/// whose four corners are the projected centre plus / minus
/// `scale * 39 / otz` rotated by `angle` and `angle + 0x400`. The
/// last eight frames fade to grey; before that a spawned flare
/// (`spawnArg1`) flickers a random green / blue-white tint on 1-in-4 LCG rolls
/// and fires a 0x600E0 effect on 1-in-16, and every flare fires 0x6005A on
/// 1-in-16. While `Gp_State1C->eventState` is 0 the coord drifts and the frame
/// counter advances until it expires, which releases the state-1C memory;
/// `field_4 >= 4` releases it at once and 2..3 idles.
void func_acropolis_helicopter_landing_pad_80181064(Task* arg0)
{
    GpEffWork*        mem;
    GpCoord*          coord;
    void**            scratch;
    u8*               head;
    AhlpFlareScratch* blk;
    POLY_FT4*         prim;
    s32               span;
    s32               n;
    s32               lvl;
    u8                tmp;

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->angle   = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->step    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = -(((u32)Gp_LcgState >> 16) & 0x1F) - 0x40;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            arg0->state++;
        }
        scratch     = (void**)G_SCRATCH_HEAD;
        head        = *scratch;
        *scratch    = head - 0x1C;
        blk         = (AhlpFlareScratch*)(head - 0x1C);
        blk->pos.vx = coord->workm.t[0];
        blk->pos.vy = coord->workm.t[1];
        blk->pos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->pos);
        gte_rtps();
        gte_stsxy(&((AhlpFlareScratch*)(head - 0x1C))->sx);
        gte_stflg(&((AhlpFlareScratch*)(head - 0x1C))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&((AhlpFlareScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            span = mem->step * 6;
            n    = mem->age;
            if (span - 8 < n) {
                lvl = (span - n + 1) * 16;
                setRGB0(prim, lvl, lvl, lvl);
            } else {
                if (arg0->spawnArg1 != 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        tmp         = (u32)Gp_LcgState >> 16;
                        setRGB0(prim, tmp >> 1, tmp, 0xFF);
                    } else {
                        prim->code |= 1;
                    }
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && Gp_State1C->eventState == 0) {
                        Gp_SpawnEff(0x600E0, coord, 0x100, NULL);
                    }
                } else {
                    prim->code = 0x2D;
                }
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && Gp_State1C->eventState == 0) {
                    Gp_SpawnEff(0x6005A, coord, 2 - arg0->spawnArg1, NULL);
                }
            }
            prim->tpage = 0x2B;
            prim->code |= 2;
            prim->clut  = 0x4384;
            prim->u0    = (mem->age / mem->step) * 0x28;
            prim->v0    = 0x48;
            prim->u1    = (mem->age / mem->step) * 0x28 + 0x27;
            prim->v1    = 0x48;
            prim->u2    = (mem->age / mem->step) * 0x28;
            prim->v2    = 0x6F;
            prim->u3    = (mem->age / mem->step) * 0x28 + 0x27;
            prim->v3    = 0x6F;
            blk->dx     = ((mem->scale * 0x27 / blk->otz) * rsin(mem->angle)) >> 12;
            blk->dy     = ((mem->scale * 0x27 / blk->otz) * rcos(mem->angle)) >> 12;
            prim->x0    = blk->sx + (u16)blk->dx;
            prim->x3    = blk->sx - (u16)blk->dx;
            prim->y0    = blk->sy - (u16)blk->dy;
            prim->y3    = blk->sy + (u16)blk->dy;
            blk->dx     = ((mem->scale * 0x27 / blk->otz) * rsin(mem->angle + 0x400)) >> 12;
            blk->dy     = ((mem->scale * 0x27 / blk->otz) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1    = blk->sx + (u16)blk->dx;
            prim->x2    = blk->sx - (u16)blk->dx;
            prim->y1    = blk->sy - (u16)blk->dy;
            prim->y2    = blk->sy + (u16)blk->dy;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        }
        SCRATCH_POP_BYTES(0x1C);
        if (Gp_State1C->eventState == 0) {
            coord->coord.t[0] += mem->move.vx;
            coord->coord.t[1] += mem->move.vy;
            coord->coord.t[2] += mem->move.vz;
            coord->flg         = 0;
            mem->age++;
            if (mem->age > mem->step * 6 - 1) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
        }
    }
}

/// Per-frame driver of the twelve helipad lights. Flags `Gp_State1C->groundShade`
/// while view 0x12 is active, folds the frame counter `gDisplayState.animFrame * 4` into a
/// 0..0xFE triangle wave kept in the effect work's `scale` (the low two bits
/// are dropped on the rising half so the ramp steps in fours), then runs
/// `func_acropolis_helicopter_landing_pad_8017F010` once per light position.
void func_acropolis_helicopter_landing_pad_801818F0(Task* arg0)
{
    GpEffWork* work = (GpEffWork*)arg0->spawnArg2;
    SVECTOR*   pos;
    s32        i;
    s32        v;
    s32        level;

    if ((Gp_GetViewIndex() & 0xFF) == 0x12) {
        Gp_State1C->groundShade = -1;
    } else {
        Gp_State1C->groundShade = 0;
    }

    v           = gDisplayState.animFrame << 2;
    work->scale = v;
    if (v & 0x80) {
        level = 0x7F - (v & 0x7F);
    } else {
        level = v & 0x7C;
    }
    work->scale = level * 2;

    i   = 0;
    pos = D_acropolis_helicopter_landing_pad_80184E80;
    for (; i < 12; i++) {
        func_acropolis_helicopter_landing_pad_8017F010(pos++, i, work->scale);
    }
}

/// Moves a coordinate frame by the 16.16 delta `func_800E0C10` computes for
/// `rec`: adds its integer part to X and Z, rounds a fractional remainder of
/// X or Z one unit away from zero, and keeps the applied delta in
/// `D_acropolis_helicopter_landing_pad_80187F88`. Returns 1 when the delta's
/// X or Z is non-zero. Works in a 0x14 block from `G_SCRATCH_HEAD`.
s32 func_acropolis_helicopter_landing_pad_801819C0(GpCoord* coord, GpRec18* rec, s16 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                             += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]                             += s->delta.vz.h.hi;
        D_acropolis_helicopter_landing_pad_80187F88.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_acropolis_helicopter_landing_pad_80187F88.vy = s->delta.vy.w >> 16;
        D_acropolis_helicopter_landing_pad_80187F88.vz = s->delta.vz.w >> 16;
        val                                            = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_helicopter_landing_pad_80187F88.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_helicopter_landing_pad_80187F88.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_helicopter_landing_pad_80187F88.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_helicopter_landing_pad_80187F88.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Pushes a coordinate frame away from the records of a `GpRec18` table.
/// Takes the frame's world position and the world point one unit ahead of
/// it and computes each record's bearing relative to that facing; a zero
/// `key` ends the table and a record whose kind is not 0x10000 or 0x30000
/// does not count. For each counting record whose bearing lies within 0x400
/// of every other counting record's, it moves X and Z `push` units away
/// along that bearing. Returns 1 when a push was applied, and 0 at once
/// while `gGameSession->viewReady` is 1.
s32 func_acropolis_helicopter_landing_pad_80181B64(GpCoord* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                  scratch;
    void**                  tail;
    u8*                     head;
    OverlayBisectorScratch* st;
    u16                     vz;
    s16                     d;
    s16                     dz;
    s32                     t;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(OverlayBisectorScratch);
        st  = (OverlayBisectorScratch*)tmp;
    }
    st->eye.vx = (u16)coord->coord.t[0];
    st->eye.vy = (u16)coord->coord.t[1];
    vz         = (u16)coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    overlayToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            dz               = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            dz               = (u16)st->aim.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail = (void**)G_SCRATCH_HEAD;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}

/// Task step of an item-pickup model: hides the mesh with flag 4 when the
/// item's 2-bit flag reads 2, otherwise resets its flags and draw offset and
/// allocates its TMD buffers. The view index is fetched and ignored.
void func_acropolis_helicopter_landing_pad_801822B0(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags |= 4;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
