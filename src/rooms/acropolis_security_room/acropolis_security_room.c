#include "common.h"

#include <psyq/stdio.h>

#include "gameplay/1A8.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/acropolis_security_room.h"
#include "rooms/room_common.h"

extern Task* D_acropolis_security_room_801855A8;
extern Task* D_acropolis_security_room_801855AC;

extern GpMsgEntry D_acropolis_security_room_801825DC[];
extern TaskDesc   D_acropolis_security_room_80182618;
extern TaskDesc   D_acropolis_security_room_8018263C;

/// The five camera ids the security monitor can display, in the order the
/// `GameFlag_GetNibble(0x2A)` nibble indexes them (see
/// `func_acropolis_security_room_8017D9DC`, which seeds `AsrMonitorWork::cameraId`
/// from this table).
extern s16 D_acropolis_security_room_801826B4[];

/// The security monitor's own hotspot table, hit-tested by
/// `func_acropolis_security_room_8017ECB4`.
extern AsrHotspot D_acropolis_security_room_80182648[];

extern s8 D_8007216C;

s32 func_acropolis_security_room_8017ECB4(AsrHotspot* table, s16 x, s16 y);

s32 func_acropolis_security_room_8017D6DC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 ret;

    if (arg0 == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(arg0, arg1, arg2, arg3);
    }
    return ret;
}

s32 func_acropolis_security_room_8017D708(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Task_SpawnFromTable(&D_acropolis_security_room_80182618, 1, 0, 0);
    }
    return 0;
}

void func_acropolis_security_room_8017D740(Task* arg0, s32 arg1, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0) {
        Task_SpawnFromTable(&D_acropolis_security_room_80182618, 0, 0, 0);
    }
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D5C4);

void func_acropolis_security_room_8017D77C(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf("monitor\n");
            D_acropolis_security_room_801855A8 = Task_Spawn(2, 9, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855A8, &sp10) != 0) {
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Task_Kill(arg0);
            }
            return;
    }
}

/* "power supply\n" plus the two bytes of alignment padding the original
 * object left in .rodata, so the block stays 16 bytes of the same content. */
static const char PowerSupplyMsg[16] = "power supply\n\0@\021";

void func_acropolis_security_room_8017D834(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf(PowerSupplyMsg);
            D_acropolis_security_room_801855AC = Task_Spawn(2, 0xA, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(2);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855AC, &sp10) != 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 1, 0);
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                D_acropolis_security_room_801855AC = NULL;
                Task_Kill(arg0);
            }
            return;
    }
}

void func_acropolis_security_room_8017D930(Task* arg0)
{
    arg0->field_24 = D_acropolis_security_room_801825DC;
    Game_SetPtrSlot(arg0, 7);
    arg0->state                        = arg0->state + 1;
    D_acropolis_security_room_801855AC = NULL;
}

void func_acropolis_security_room_8017D97C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017D984);

/// Entry state of the security-monitor task: allocates the `AsrMonitorWork`
/// block into the `Task::idMap` slot, spawns the monitor's companion task,
/// seeds `cameraId` from the `GameFlag_GetNibble(0x2A)` camera table, and picks
/// the next state from the `GameFlag_GetNibble(1)` progress nibble (state+1 and
/// prompt kind 8 before chapter 3, state 6 and prompt kind 5 after). Finally it
/// clears every hotspot's `hit` flag so the first hit test starts clean.
void func_acropolis_security_room_8017D9DC(Task* task)
{
    AsrMonitorWork* work;
    AsrHotspot*     hs;
    s16             flag;
    s32             state;
    s16             stateElse;

    work = (AsrMonitorWork*)Mem_Calloc(sizeof(AsrMonitorWork), 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->spawnArg2  = Task_SpawnFromTable(&D_acropolis_security_room_8018263C, 0, 1, 0);
    task->idMap      = (TaskIdMap*)work;
    work->blinkTimer = 0;
    stateElse        = 6;
    flag             = GameFlag_GetNibble(0x2A);
    if ((u16)flag < 5) {
        work->cameraId = D_acropolis_security_room_801826B4[flag];
    } else {
        work->cameraId = D_acropolis_security_room_801826B4[0];
    }
    if (GameFlag_GetNibble(1) < 3) {
        D_8007216C = 8;
        /* Without this the scheduler hoists the `task->state` load above the
           `D_8007216C` byte store to fill its load-delay slot. */
        SOFT_BARRIER();
        state = task->state;
        state++;
    } else {
        D_8007216C = 5;
        state      = stateElse;
    }
    task->state = state;
    Display_AcquireRef();
    Game_Session->field_68 = 1;
    Game_Session->field_66 = 1;
    Game_Session->field_1  = 1;
    hs                     = D_acropolis_security_room_80182648;
    if (hs->id != -1) {
        do {
            hs->hit = 0;
            hs++;
        } while (hs->id != -1);
    }
}

/// Runs the hotspot-hit state of the security monitor: redraws the panel and
/// cursor, then hit-tests the action cursor against the room's hotspot table.
/// A miss leaves the prompt highlighted (`mode` 1); a hit with the prompt
/// confirmed (`buttons[0].state` 2) scans the table for the raised entry and hands its
/// `id` / `promptKind` to the work block, advancing to state 3. Otherwise the
/// task advances to state 5 once the prompt has been dismissed.
void func_acropolis_security_room_8017DB30(Task* task)
{
    AsrMonitorWork*   work;
    AsrHotspot*       hs;
    RoomActionPrompt* prompt;

    hs     = D_acropolis_security_room_80182648;
    prompt = &D_80114D28;
    work   = (AsrMonitorWork*)task->idMap;
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_acropolis_security_room_8017ECB4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if ((prompt->buttons[0].state == 2) && (hs->id != -1)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->selection  = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 3;
                    return;
                }
                hs++;
            } while (hs->id != -1);
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

/// Runs the camera-list state of the security monitor: mirrors the highlighted
/// row into `Mc_SaveData.field_4` (with a click), scrolls the panel by a page
/// when the row is one of the two 0x8000/0x8001 scroll commands, and fires the
/// two one-shot cap sequences the room gates on the `0xA` game-flag nibble.
/// Then redraws the panel plus cursor overlay and advances to state 2.
void func_acropolis_security_room_8017DC7C(Task* task)
{
    AsrMonitorWork* work;
    McSaveData*     save;
    s32             sfx;
    s16             sel;
    u16             usel;

    work                = (AsrMonitorWork*)task->idMap;
    D_80114D28.mode     = 0;
    D_80114D28.targetId = 0;
    if (func_800D4EC0() != 0) {
        sel = work->selection;
        if (sel >= 0) {
            save = &Mc_SaveData;
            if (save->field_4 != sel) {
                save->field_4 = work->selection;
                SndEvt_EnqueueType6(0x51060003, 0, 0);
                if ((work->selection == 0xA) && !(GameFlag_GetNibble(0xA) & 2)) {
                    work->field_7 = 1;
                }
            }
        }
        usel = work->selection;
        if (usel == 0x8000) {
            if (((s16)work->cameraId + 0x3E) < 0xFE) {
                work->cameraId += 0x3E;
                sfx             = 0x51060006;
                goto play;
            }
        } else if (usel == 0x8001) {
            if (((s16)work->cameraId - 0x3E) > 0) {
                work->cameraId -= 0x3E;
                sfx             = 0x51060007;
            play:
                SndEvt_EnqueueType6(sfx, 0, 0);
            }
        }
        if (((u8)D_8007216C == 0xB) && ((s16)work->cameraId != 4) && !(GameFlag_GetNibble(0xA) & 1)) {
            Gp_StartCapSlot(0xD, 0, 0);
            GameFlag_SetNibble(0xA, GameFlag_GetNibble(0xA) | 1);
        }
        if (((u8)D_8007216C == 0xA) && ((s16)work->cameraId != 4) && (work->field_7 != 0) &&
            (work->field_8 == 0) && (GameFlag_GetNibble(0x102) == 0)) {
            Gp_StartCapSlot(0xC, 0, 0);
            work->field_8 = 1;
        }
    }
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    task->state = 2;
}

/// Outlines `rect` on screen in the colour (`r`, `g`, `b`) with four
/// unconnected flat lines -- top, right, bottom and left edge of the rectangle
/// spanning (`x`, `y`) to (`x + w`, `y + h`) -- each linked into
/// `Gpu_CurrentOt[3]`. Nothing in the overlay calls it; it is the debug box
/// drawer for the hotspot rectangles.
void func_acropolis_security_room_8017DE80(AsrRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 3, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 3, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 3, line);

    line           = (LINE_F2*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(Gpu_CurrentOt + 3, line);
}

/// Washes the security-monitor panel with the grey level `id` -- the work
/// block's `cameraId` biased by -0x7F -- as a semi-transparent `POLY_F4`
/// covering (-0x66, -0x5F) to (0x6C, 0x3C) in `Gpu_CurrentOt[0xC]`, followed by
/// the drawing-mode packet that restores the panel's texture page. A negative
/// `id` uses its magnitude and the other semi-transparency rate (0xE100004A
/// rather than 0xE100002A), which is what makes the "no signal" panel read
/// differently from a live camera. The strip below the panel (y 0x3C to 0x38)
/// is then blacked out with an opaque quad in `Gpu_CurrentOt[0xB]`.
void func_acropolis_security_room_8017E0C4(s16 id)
{
    POLY_F4* poly;
    DR_MODE* dr;
    u16      c;

    if (id >= 0) {
        c              = id & 0x7F;
        poly           = (POLY_F4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
        setlen(poly, 5);
        setcode(poly, 0x2A);
        poly->r0 = c;
        poly->g0 = c;
        poly->b0 = c;
        poly->x0 = -0x66;
        poly->y0 = -0x5F;
        poly->x1 = 0x6C;
        poly->y1 = -0x5F;
        poly->x2 = -0x66;
        poly->y2 = 0x3C;
        poly->x3 = 0x6C;
        poly->y3 = 0x3C;
        addPrim(Gpu_CurrentOt + 0xC, poly);

        dr             = (DR_MODE*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(dr + 1);
        setlen(dr, 1);
        dr->code[0] = 0xE100002A;
        addPrim(Gpu_CurrentOt + 0xC, dr);
    } else {
        c              = (~id + 1) & 0xFF;
        poly           = (POLY_F4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
        setlen(poly, 5);
        setcode(poly, 0x2A);
        poly->r0 = c;
        poly->g0 = c;
        poly->b0 = c;
        poly->x0 = -0x66;
        poly->y0 = -0x5F;
        poly->x1 = 0x6C;
        poly->y1 = -0x5F;
        poly->x2 = -0x66;
        poly->y2 = 0x3C;
        poly->x3 = 0x6C;
        poly->y3 = 0x3C;
        addPrim(Gpu_CurrentOt + 0xC, poly);

        dr             = (DR_MODE*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(dr + 1);
        setlen(dr, 1);
        dr->code[0] = 0xE100004A;
        addPrim(Gpu_CurrentOt + 0xC, dr);
    }

    poly           = (POLY_F4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
    setlen(poly, 5);
    setcode(poly, 0x28);
    poly->r0 = 0;
    poly->g0 = 0;
    poly->b0 = 0;
    poly->x0 = -0x66;
    poly->y0 = 0x3C;
    poly->x1 = 0x6C;
    poly->y1 = 0x3C;
    poly->x2 = -0x66;
    poly->y2 = 0x38;
    poly->x3 = 0x6C;
    poly->y3 = 0x38;
    addPrim(Gpu_CurrentOt + 0xB, poly);

    dr             = (DR_MODE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(dr + 1);
    setlen(dr, 1);
    dr->code[0] = 0xE100000A;
    addPrim(Gpu_CurrentOt + 0xB, dr);
}

/// Draws the blinking cursor overlay on top of the monitor panel: a 0x6C-wide
/// grey `TILE` whose top edge and height both track `AsrMonitorWork::blinkTimer`,
/// followed by the drawing-mode packet that restores the panel's texture page.
/// The timer wraps at 0x97, which is what makes the bar sweep and restart.
void func_acropolis_security_room_8017E37C(Task* task)
{
    AsrMonitorWork* work;
    TILE*           tile;
    DR_MODE*        dr;
    s16             y;

    tile           = (TILE*)Gpu_PrimCursor;
    work           = (AsrMonitorWork*)task->idMap;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x42);
    tile->r0 = 0x60;
    tile->g0 = 0x60;
    tile->b0 = 0x60;
    tile->x0 = -0x66;
    tile->w  = 0x6C;
    y        = work->blinkTimer - 0x5F;
    tile->h  = y;
    tile->y0 = y;
    addPrim(Gpu_CurrentOt + 0xE, tile);
    dr             = (DR_MODE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(dr + 1);
    setlen(dr, 1);
    dr->code[0] = 0xE100000A;
    addPrim(Gpu_CurrentOt + 0xE, dr);
    work->blinkTimer++;
    if (work->blinkTimer >= 0x97) {
        work->blinkTimer = 0;
    }
}

void func_acropolis_security_room_8017DB30(Task* task);
void func_acropolis_security_room_8017DC7C(Task* task);
void func_acropolis_security_room_8017EA28(Task* task);
void func_acropolis_security_room_8017EA5C(Task* task);
void func_acropolis_security_room_8017EADC(Task* task);
void func_acropolis_security_room_8017EB9C(Task* task);

/// States of the security-monitor task, dispatched by
/// `func_acropolis_security_room_8017ED68`: set up the work block, run the
/// camera list, redraw the panel, confirm a camera, and leave the monitor.
const AsrMonitorStateTable AsrMonitorStates = {
    { {
        func_acropolis_security_room_8017D9DC,
        func_acropolis_security_room_8017EA28,
        func_acropolis_security_room_8017DB30,
        func_acropolis_security_room_8017EA5C,
        func_acropolis_security_room_8017DC7C,
        func_acropolis_security_room_8017EADC,
        func_acropolis_security_room_8017EB9C,
    } },
    NULL,
};
