#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/acropolis_security_room.h"
#include "rooms/room_common.h"

/// The security monitor's own hotspot table, hit-tested by
/// `func_acropolis_security_room_8017ECB4`.
extern AsrHotspot D_acropolis_security_room_80182648[];

/// The five camera ids the security monitor can display, in the order the
/// `GameFlag_GetNibble(0x2A)` nibble indexes them.
extern s16 D_acropolis_security_room_801826B4[];

extern s8  D_8007216C;
extern s16 D_80114D08;

/// States of the security-monitor task, dispatched by
/// `func_acropolis_security_room_8017ED68`. Defined in the previous unit,
/// which owns the `.rodata` it sits in.
extern const AsrMonitorStateTable AsrMonitorStates;

void func_acropolis_security_room_8017E0C4(s16 id);
void func_acropolis_security_room_8017E37C(Task* task);
void func_acropolis_security_room_8017E490(Task* task);
void func_acropolis_security_room_8017E8F0(s32 x, s32 y, s32 variant);
s32  func_acropolis_security_room_8017ECB4(AsrHotspot* table, s16 x, s16 y);
void func_acropolis_security_room_8017EDE4(Task* task);

/// Per-frame cursor driver of the security-room action prompt, run as state 1
/// of `func_acropolis_security_room_8017E9D8`. Byte-for-byte the same body as
/// `func_acropolis_security_room_8017F480` in the cap script.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it for a dead-zone
/// curve) and then the d-pad -- whose four bits select one of eight
/// 1/16-of-a-turn headings fed to `rsin`/`rcos` -- into the prompt's
/// 1/512-pixel position, clamps that to the screen, classifies the confirm
/// (0x40) and cancel (0xA0) buttons into the prompt's two button slots, and
/// finally hands the rounded position to `func_acropolis_security_room_8017E8F0`
/// to draw the cursor. `RoomActionPrompt::targetId` doubles as the cursor speed
/// here and `field_E` as the double-press window: a second press inside that
/// many frames without the cursor having moved reports state 4 instead of 2.
///
/// `step` carries the analog delta first and the d-pad heading afterwards, and
/// `idx` indexes the button slots in `u16` units so that `i` survives as the
/// loop counter.
void func_acropolis_security_room_8017E490(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28 + port;
        pad    = (PadState*)&Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * Display_State.field_10a;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * Display_State.field_10a;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * Display_State.field_10a;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * Display_State.field_10a;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * Display_State.field_10a) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * Display_State.field_10a) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons[0].state;
        heldp  = &prompt->buttons[0].heldFrames;
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                           = 0;
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed = prompt->screen.packed;
                    *statep                                              = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += Display_State.field_10a;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_acropolis_security_room_8017E8F0(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017E8F0);

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017EA28);

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
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
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
/// prompt. A hit that the player confirms (`buttons[0].state == 2`) on a raised hotspot
/// clears the prompt and runs cap command 0xE; `buttons[1].state == 2` leaves the
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
    if (func_acropolis_security_room_8017ECB4(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
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
    if (prompt->buttons[1].state == 2) {
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

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017EDE4);

/// Idle state of the security room's cap script: the same hotspot scan
/// `func_acropolis_security_room_8017EB9C` runs for the monitor, but against
/// the script's own table and with the hit recorded in the script's state
/// block instead of dispatched as a cap command. A confirmed
/// (`buttons[0].state == 2`) hit copies the hotspot's `id` and `promptKind` into the
/// state block and advances to state 3; with nothing under the cursor the
/// pending sub-step is cleared and the prompt merely highlights (`mode` 1).
/// `buttons[1].state == 2` leaves the scan by advancing to state 5.
void func_acropolis_security_room_8017EE44(Task* task)
{
    RoomActionPrompt*           prompt = &D_80114D28;
    AsrHotspot*                 hs     = D_acropolis_security_room_801826DC;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->idMap;

    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_acropolis_security_room_8017FCB0(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->variant      = hs->id;
                    st->promptKind   = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        st->field_0  = 0;
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", D_acropolis_security_room_8017D63C);
