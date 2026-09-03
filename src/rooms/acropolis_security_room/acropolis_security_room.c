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

extern s8  D_8007216C;
extern s16 D_80114D08;

s32  func_acropolis_security_room_8017ECB4(AsrHotspot* table, s16 x, s16 y);
void func_acropolis_security_room_8017E490(Task* task);
void func_acropolis_security_room_8017EDE4(Task* task);

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
/// confirmed (`field_14` 2) scans the table for the raised entry and hands its
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
    if (func_acropolis_security_room_8017ECB4(hs, prompt->screenX, prompt->screenY) != 0) {
        prompt->mode = 2;
        if ((prompt->field_14 == 2) && (hs->id != -1)) {
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
    if (prompt->field_1C == 2) {
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

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017DE80);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E0C4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E37C);

void func_acropolis_security_room_8017DB30(Task* task);
void func_acropolis_security_room_8017DC7C(Task* task);
void func_acropolis_security_room_8017EA28(Task* task);
void func_acropolis_security_room_8017EA5C(Task* task);
void func_acropolis_security_room_8017EADC(Task* task);
void func_acropolis_security_room_8017EB9C(Task* task);

/// States of the security-monitor task, dispatched by
/// `func_acropolis_security_room_8017ED68`: set up the work block, run the
/// camera list, redraw the panel, confirm a camera, and leave the monitor.
static const AsrMonitorStateTable AsrMonitorStates = {
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

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E490);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E8F0);

/// Two-state dispatcher whose handler table is built on the stack rather than
/// read from `.data`: state 0 runs `func_acropolis_security_room_8017EDE4` and
/// state 1 runs `func_acropolis_security_room_8017E490`.
void func_acropolis_security_room_8017E9D8(Task* task)
{
    TaskFunc funcs[2] = {
        func_acropolis_security_room_8017EDE4,
        func_acropolis_security_room_8017E490,
    };

    funcs[task->state](task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EA28);

/// Confirms the camera the player picked on the security monitor: clears the
/// action prompt, redraws the panel for the selected camera plus its cursor
/// overlay, spawns the prompt at the panel's coordinates and advances the task.
void func_acropolis_security_room_8017EA5C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    AsrMonitorWork*   work   = (AsrMonitorWork*)task->idMap;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    func_800D4E78(prompt->screenX, prompt->screenY, work->promptKind);
    task->state = 4;
}

/// Leaves the security monitor: records which camera was on screen as the
/// `0x2A` nibble (index into `D_acropolis_security_room_801826B4`, 0 if the id
/// is not in the table), restores the room's normal display state and kills the
/// monitor task along with the child task it spawned.
void func_acropolis_security_room_8017EADC(Task* task)
{
    AsrMonitorWork* work;
    s16*            camera;
    s32             index;
    s32             cameraId;

    index      = 0;
    camera     = D_acropolis_security_room_801826B4;
    work       = (AsrMonitorWork*)task->idMap;
    D_80114D08 = 0xA;
    cameraId   = (s16)work->cameraId;
loop:
    if (cameraId != *camera) {
        index  += 1;
        camera += 1;
        if (index >= 5) {
            GameFlag_SetNibble(0x2A, 0);
            goto done;
        }
        goto loop;
    }
    GameFlag_SetNibble(0x2A, index);
done:
    D_8007216C = 4;
    Display_ReleaseRef();
    Game_Session->field_66 = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_1  = 0;
    Task_Kill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

/// Idle state of the security monitor: hit-tests the action cursor against the
/// monitor's hotspot table and mirrors the result into the room's action
/// prompt. A hit that the player confirms (`field_14 == 2`) on a raised hotspot
/// clears the prompt and runs cap command 0xE; `field_1C == 2` leaves the
/// monitor by advancing to state 5.
void func_acropolis_security_room_8017EB9C(Task* task)
{
    RoomActionPrompt* prompt  = &D_80114D28;
    AsrHotspot*       hotspot = D_acropolis_security_room_80182648;

    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_acropolis_security_room_8017ECB4(hotspot, prompt->screenX, prompt->screenY) != 0) {
        prompt->mode = 2;
        if (prompt->field_14 == 2) {
            for (; hotspot->id != -1; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    Gp_RunCapCmd(0xE, 0);
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->field_1C == 2) {
        task->state = 5;
    }
}

/// Hit-tests the action cursor at (`x`, `y`) against the 0xFFFF-terminated
/// hotspot table `table`, raising `hit` on every entry whose rectangle
/// contains the point and clearing it on every other one. Returns non-zero if
/// any entry was hit, so `func_acropolis_security_room_8017EB9C` can tell
/// "cursor is over something" from "cursor is over nothing" without rescanning
/// the table. Same body as `func_acropolis_security_room_8017FCB0`.
s32 func_acropolis_security_room_8017ECB4(AsrHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// Runs the security monitor's current state. The seven handlers are copied
/// onto the stack first, so the call goes through a local table rather than
/// through `.rodata`.
void func_acropolis_security_room_8017ED68(Task* task)
{
    TaskFuncTable7 sp;

    sp = AsrMonitorStates.states;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EDE4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EE44);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D63C);
