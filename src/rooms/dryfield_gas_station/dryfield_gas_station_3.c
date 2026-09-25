#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_gas_station.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Work block for the gas-station cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_gas_station_801807E0` and hung off `Task::work` (0x1C).
///
/// `owner` is the slot-3 game pointer (`gameGetPtrSlot(3)`) the task dispatches
/// its messages to, and `playerEffActive` is the flag guarding
/// `Gp_KillPlayerEffs` / `Gp_SpawnWeaponEff`. `field_4` is the script command
/// `func_dryfield_gas_station_801803C0` carries out and clears once it is done,
/// `field_6` the step within a multi-frame command (both written together by
/// `func_dryfield_gas_station_80180B2C`), and `field_8` the frame counter of the
/// command that walks the owner across the forecourt.
typedef struct DgsWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ u16   field_4;
    /* 0x06 */ u16   field_6;
    /* 0x08 */ u16   field_8;
    /* 0x0A */ byte  pad_A[0x2];
    /* 0x0C */ u16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DgsWork;
STATIC_ASSERT_SIZEOF(DgsWork, 0x10);

extern u8         D_80071075;
extern s8         D_80114C12;
extern s32        D_dryfield_gas_station_80182E30;
extern GpXformArg D_dryfield_gas_station_80182E44[];
extern GpXformArg D_dryfield_gas_station_80182E5C;
extern GpXformArg D_dryfield_gas_station_80182E74;
extern s32        D_dryfield_gas_station_80182E8C;
extern s32        D_dryfield_gas_station_8018303C;
extern TaskDesc   D_dryfield_gas_station_8018312C[];
extern SVECTOR    D_dryfield_gas_station_80183144;

/// The cutscene task `func_dryfield_gas_station_801807E0` publishes once its
/// `DgsWork` block is set up, so the room's script helpers can reach it.
extern Task* D_dryfield_gas_station_80184BD4;

/// Carries out the script command in `DgsWork::field_4`, then clears it (the
/// multi-frame commands return early until they finish). 1 places the owner at
/// the first of three 0x3E9 placements and plays two sounds; 2 and 3 hand the
/// owner a `D_dryfield_gas_station_80182E30` script record as msg 0x3F4, 2 also
/// sending msg 0x3FD and 3 placing the owner at the second placement first.
/// 4 walks the owner from the first placement to the third over 30 frames with
/// msg 0x3FE before its closing 0x3F4; 5 is `func_dryfield_gas_station_80180A60`
/// written out again; 6 spawns entry 1 of `D_dryfield_gas_station_8018312C`,
/// waits a frame and turns the display back on.
void func_dryfield_gas_station_801803C0(Task* task)
{
    DgsWork* work;
    DgsWork* cur;
    DgsWork* eff;
    Task*    shared;
    union {
        GpAnimArg rec;
        GpMoveArg move;
    } msg;
    GpAnimArg  script;
    GpAnimArg* rec;
    u16        step;

    work = (DgsWork*)task->work;
    switch (work->field_4) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E44[0], 0);
            SndEvt_EnqueueType6(0x52010011, 0, 0);
            SndEvt_EnqueueType6(0x52010012, 0, 0);
            break;
        case 2:
            cur = (DgsWork*)task->work;
            if (cur->owner != NULL) {
                msg.rec.animBlock.ptr = &D_dryfield_gas_station_80182E30;
                msg.rec.field_4       = 1;
                msg.rec.field_8       = 0;
                msg.rec.field_C       = 0;
                msg.rec.field_10      = 0;
                Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
            }
            Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
            break;
        case 3:
            SndEvt_EnqueueType6(0x52010013, 0, 0);
            Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E5C, 0);
            cur = (DgsWork*)task->work;
            if (cur->owner != NULL) {
                msg.rec.animBlock.ptr = &D_dryfield_gas_station_80182E30;
                msg.rec.field_4       = 2;
                msg.rec.field_8       = 1;
                msg.rec.field_C       = 0x1E;
                msg.rec.field_10      = 0;
                Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
            }
            break;
        case 4:
            step = work->field_6;
            switch (step) {
                case 0:
                    cur = (DgsWork*)task->work;
                    if (cur->owner != NULL) {
                        msg.rec.animBlock.ptr = &D_dryfield_gas_station_80182E30;
                        msg.rec.field_4       = 3;
                        msg.rec.field_8       = 0;
                        msg.rec.field_C       = 0;
                        msg.rec.field_10      = 0;
                        Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
                    }
                    Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
                    Gp_DispatchMsg((Task*)work->owner, 0x3FC, 0, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    return;
                case 1:
                    msg.move.x        = (D_dryfield_gas_station_80182E44[2].pos.vx - D_dryfield_gas_station_80182E44[0].pos.vx) / 30;
                    msg.move.y        = 0;
                    msg.move.z        = (D_dryfield_gas_station_80182E44[2].pos.vz - D_dryfield_gas_station_80182E44[0].pos.vz) / 30;
                    msg.move.field_10 = 0;
                    Gp_DispatchMsg((Task*)work->owner, 0x3FE, (s32)&msg.move, 0);
                    work->field_8++;
                    if (work->field_8 < 31) {
                        return;
                    }
                    // Taken before the owner check, the record's address is in
                    // $a2 early enough that the two register-valued fields are
                    // stored through it; the constant ones still go off $sp.
                    rec = &script;
                    cur = (DgsWork*)task->work;
                    if (cur->owner != NULL) {
                        script.animBlock.ptr = &D_dryfield_gas_station_80182E30;
                        script.field_4       = 0;
                        rec->field_8         = step;
                        rec->field_C         = 0xF;
                        script.field_10      = 0;
                        Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)rec, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 5:
            shared = D_dryfield_gas_station_80184BD4;
            eff    = (DgsWork*)shared->work;
            if (eff->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                eff->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            Gp_DispatchMsg((Task*)eff->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E74, 0);
            cur = (DgsWork*)shared->work;
            if (cur->owner != NULL) {
                msg.rec.animBlock.ptr = &D_dryfield_gas_station_80182E30;
                msg.rec.field_4       = 0;
                msg.rec.field_8       = 0;
                msg.rec.field_C       = 0;
                msg.rec.field_10      = 0;
                Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
            }
            SndEvt_EnqueueType7(0x52010011, 0x3C);
            SetDispMask(1);
            break;
        case 6:
            switch (work->field_6) {
                case 0:
                    Task_SpawnFromTable(D_dryfield_gas_station_8018312C, 1, 0x1E, 0);
                case 1:
                    work->field_6++;
                    return;
                case 2:
                    SetDispMask(1);
                    break;
            }
            break;
    }
    work->field_4 = 0;
}

/// Spawns the gas station's cutscene owner. State 0 refuses to run twice (a
/// `D_80114C12` of 1 and a live `D_80071075` both mean the cutscene is already
/// up), otherwise it parks the freshly zeroed 0x10-byte `DgsWork` block in
/// `Task::work`, fills `owner` from pointer slot 3 and republishes this task as
/// `D_dryfield_gas_station_80184BD4` so the room's script helpers can reach that block.
/// Two kills: a failed `Mem_Malloc` kills the task outright, and state 1 kills
/// it once the session has torn down (`gGameSession->eventState`). Between the two
/// it hands slot 3 the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4 -- only when a previous state 0 already found an owner, since the
/// reloaded `work` is dereferenced unconditionally.
void func_dryfield_gas_station_801807E0(Task* task)
{
    DgsWork*  work;
    DgsWork*  work2;
    GpAnimArg script;

    switch (task->state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                work       = Mem_Malloc(0x10, false);
                task->work = (TaskIdMap*)work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner                     = gameGetPtrSlot(3);
                    D_dryfield_gas_station_80184BD4 = task;
                }
                work2 = (DgsWork*)task->work;
                if (work2->owner != 0) {
                    script.animBlock.ptr = &D_dryfield_gas_station_80182E30;
                    script.field_4       = 0;
                    script.field_8       = 0;
                    script.field_C       = 0;
                    script.field_10      = 0;
                    Gp_DispatchMsg((Task*)work2->owner, 0x3F4, (s32)&script, 0);
                }
                func_800E3FAC(0xA2, 9);
                func_800E8634((s32)&D_dryfield_gas_station_80182E8C, 0,
                              (s32)&D_dryfield_gas_station_8018303C);
                task->state = task->state + 1;
                return;
            }
            return;

        case 1:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
                return;
            }
            func_dryfield_gas_station_801803C0(task);
            break;
    }
}

/// Latches the player-effect flag and kills the effects once. The 1 is loaded
/// before the branch and stored in the `jal` delay slot.
void func_dryfield_gas_station_80180944(void)
{
    DgsWork* work = (DgsWork*)D_dryfield_gas_station_80184BD4->work;
    if (work->playerEffActive == 0) {
        work->playerEffActive = 1;
        Gp_KillPlayerEffs();
    }
}

/// Fade task: on its first tick it allocates the 8-byte fade block and seeds
/// its three channels to 0xFF, then every frame it draws the fade overlay (the
/// red channel standing in for blue) and steps each channel down by
/// `Task::spawnArg1`, killing itself once red has gone negative.
void func_dryfield_gas_station_80180984(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if (fade->r >= 0) {
                return;
            }
            taskKill(arg0);
            break;
    }
}

/// Tells slot 3 that the cutscene is opening: it ends the weapon effect the
/// player may still be carrying (flag at `DgsWork::playerEffActive`), echoes the
/// equipped weapon back with msg 0x3E9 and, once the cutscene task has an owner,
/// hands that owner the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4. The record is a `GpAnimArg` built on the stack, only its first field
/// (the script pointer) set.
void func_dryfield_gas_station_80180A60(void)
{
    Task*     task;
    DgsWork*  work;
    DgsWork*  work2;
    GpAnimArg script;

    task = D_dryfield_gas_station_80184BD4;
    work = (DgsWork*)task->work;
    if (work->playerEffActive != 0) {
        Gp_SpawnWeaponEff();
        work->playerEffActive = 0;
        Gp_MsgPlayerWeapon(0);
    }
    Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E74, 0);
    work2 = (DgsWork*)task->work;
    if (work2->owner != 0) {
        script.animBlock.ptr = &D_dryfield_gas_station_80182E30;
        script.field_4       = 0;
        script.field_8       = 0;
        script.field_C       = 0;
        script.field_10      = 0;
        Gp_DispatchMsg((Task*)work2->owner, 0x3F4, (s32)&script, 0);
    }
    SndEvt_EnqueueType7(0x52010011, 0x3C);
    SetDispMask(1);
}

/// Hands the cutscene task the script command `arg0` to carry out, starting
/// it from its first step.
void func_dryfield_gas_station_80180B2C(s16 arg0)
{
    DgsWork* work = (DgsWork*)D_dryfield_gas_station_80184BD4->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}

/// Draws a pulsing glow at `arg1` in `arg0`'s space. `arg1` is rotated by
/// `arg0`'s `workm` and offset by its translation, then projected through
/// `GsWSMATRIX` with a single `RTPS` into a 0x14-byte `G_SCRATCH_HEAD` block;
/// nothing is drawn when its `otz` is 16 or less. `arg3` is a signed
/// half-extent, so the on-screen half width is `(s16)arg3 * 32 / otz`; two
/// gouraud `POLY_G4` wedges and two `LINE_G3` diagonals cross the projected
/// centre, whose green and blue pulse as `rsin(animFrame * arg2) / 34 + 0x78`.
void func_dryfield_gas_station_80180B4C(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    LINE_G3*          line;
    s32               i;
    s32               color;
    s32               pulse;
    s32               twice;
    s32               t;
    s32               t2;

    Gp_UpdateCoord(arg0);
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomShaftScratch*)(head - 0x14);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomShaftScratch*)(head - 0x14))->vec);
    block->vec.vx = (u16)block->vec.vx + (u16)arg0->workm.t[0];
    block->vec.vy = (u16)block->vec.vy + (u16)arg0->workm.t[1];
    block->vec.vz = (u16)block->vec.vz + (u16)arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomShaftScratch*)(head - 0x14))->vec);
    gte_rtps();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        pulse            = rsin(gDisplayState.animFrame * (s16)arg2);
        i                = 0;
        block->halfWidth = ((s16)arg3 << 5) / ((RoomShaftScratch*)(head - 0x14))->otz;
        color            = pulse / 34 + 0x78;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->halfWidth;
            prim->x1 = prim->x2 = block->sx;
            prim->x3            = block->sx + (u16)block->halfWidth;
            prim->y0 = prim->y2 = prim->y3 = block->sy;
            twice                          = i << 1;
            prim->y1                       = (block->sy - (u16)block->halfWidth) + block->halfWidth * twice;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, color, color);
            setRGB2(line, 0, 0, 0);
            t        = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->halfWidth * t);
            line->y0 = block->sy - (block->halfWidth * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->halfWidth * t);
            line->y2 = block->sy + (block->halfWidth * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Draws a pulsing cyan glow at `data` in `coord`'s space: the point is
/// projected through `GsWSMATRIX`, and nothing is drawn when its `otz` is 16 or
/// less. Around the projected centre it lays a fan of gouraud `POLY_G4`
/// wedges of radius `rOuter`, each paired with a brighter one of half that
/// radius, then four quads reaching out from `rInner` towards `rOuter`.
/// The centre vertex's intensity is `rsin(animFrame * arg2) / 34 + 0x78`,
/// halved on the outer wedges and on the four quads.
void func_dryfield_gas_station_80181058(GpCoord* coord, SVECTOR* data, s32 arg2, s32 arg3)
{
    u8*              head;
    RoomGlowScratch* block;
    POLY_G4*         prim;
    s32              pulse;
    s32              color;
    s32              half;
    s32              size;
    s32              ang;
    s32              t;
    s32              t2;
    s32              u;

    Gp_UpdateCoord(coord);
    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x18);
        block   = (RoomGlowScratch*)tmp;
    }

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(data);
    gte_rtv0();
    gte_stsv(&((RoomGlowScratch*)(head - 0x18))->vec);
    block->vec.vx += coord->workm.t[0];
    block->vec.vy += coord->workm.t[1];
    block->vec.vz += coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomGlowScratch*)(head - 0x18))->vec);
    gte_rtps();
    gte_stsxy(&((RoomGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&block->otz);
    if (((RoomGlowScratch*)(head - 0x18))->otz > 16) {
        pulse         = rsin(gDisplayState.animFrame * (s16)arg2);
        ang           = 0;
        size          = (s16)arg3;
        block->rOuter = (size * 64) / ((RoomGlowScratch*)(head - 0x18))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomGlowScratch*)(head - 0x18))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        color = half;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame effect: draws the gas station's shaft with the task's own model
/// coordinate, then advances the room's `Gp_State1C`. `Task::extra` is the
/// task's `TmdObject`, so `field_8` is the coordinate both draws share. The
/// stage-visit byte `gGameSession->at4.loc.view` is used as a bit index: bits 4, 6,
/// 11 and 12 (`0x1850`) select `func_dryfield_gas_station_80180B4C` with the wide half-extent 0x80,
/// and any other non-zero bit selects `func_dryfield_gas_station_80181058`
/// with 0x40.
void func_dryfield_gas_station_80181A78(Task* arg0)
{
    s32      mask;
    GpCoord* coord;

    mask  = 1 << gGameSession->at4.loc.view;
    coord = arg0->extra.tmd->coords;
    if (mask & 0x1850) {
        func_dryfield_gas_station_80180B4C(coord, &D_dryfield_gas_station_80183144, 0x60, 0x80);
    } else if (mask != 0) {
        func_dryfield_gas_station_80181058(coord, &D_dryfield_gas_station_80183144, 0x60, 0x40);
    }
    Gp_State1C->roomEffectMode = 2;
}
