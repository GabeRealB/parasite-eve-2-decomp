#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/dryfield_night_motel_lobby.h"
#include "rooms/room_common.h"

extern s16 D_80114D08;

/// Task descriptor of the examine child task `func_dryfield_night_motel_lobby_80180E98`
/// spawns.
extern TaskDesc D_dryfield_night_motel_lobby_80182814[];

extern GpAreaApplyRec D_dryfield_night_motel_lobby_801844AC;

/// World-space points of the markers `func_dryfield_night_motel_lobby_801812F8`
/// draws; the second name is the same run from its second entry.
extern SVECTOR D_dryfield_night_motel_lobby_801828E0[];
extern SVECTOR D_dryfield_night_motel_lobby_801828E8[];

/// The seven digits of the lobby keypad code as entered so far, most recent
/// first at index 0; `0xA` marks a slot the player has not filled. The room's
/// init resets all seven to `0xA`,
/// `func_dryfield_night_motel_lobby_80180440` shifts a new digit in at index 0
/// (its own count of digits entered is bounded by 7) and
/// `func_dryfield_night_motel_lobby_80180734` tests the filled slots against
/// the code. The datum's eighth byte is padding before the cap script.
extern u8 D_dryfield_night_motel_lobby_801844D8[7];

s16  func_dryfield_night_motel_lobby_80180734(void);
void func_dryfield_night_motel_lobby_80180C20(s32 x, s32 y, s32 variant);
void func_dryfield_night_motel_lobby_80181298(Task* task);
void func_dryfield_night_motel_lobby_80181404(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_motel_lobby_80181878(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_motel_lobby_80182200(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_dryfield_night_motel_lobby_801802A8(Task* task)
{
    DnmlExamineWork* work = (DnmlExamineWork*)task->work;
    POLY_FT4*        p;
    s32              i;
    u8               digit;
    u8               u;

    if (work->field_6 == 0) {
        for (i = 0; i < 7; i++) {
            D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
        }
    }
    if (work->field_7 == 0) {
        for (i = 0; i < 7; i++) {
            digit          = D_dryfield_night_motel_lobby_801844D8[i];
            p              = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(p + 1);
            setPolyFT4(p);
            if (digit == 0xA) {
                p->u0 = 0;
                p->v0 = 0x18;
                p->u1 = 0x18;
                p->v1 = 0x18;
                p->u2 = 0;
                p->v2 = 0x30;
                p->u3 = 0x18;
                p->v3 = 0x30;
            } else {
                u     = digit * 0x18;
                p->u0 = u;
                p->v0 = 0;
                p->u1 = u + 0x18;
                p->v1 = 0;
                p->u2 = u;
                p->v2 = 0x18;
                p->u3 = u + 0x18;
                p->v3 = 0x18;
            }
            setShadeTex(p, 1);
            p->tpage = 0xE;
            p->clut  = 0x4000;
            p->x0    = 0x3C - i * 0x18;
            p->y0    = -0x60;
            p->x1    = 0x54 - i * 0x18;
            p->y1    = -0x60;
            p->x2    = 0x3C - i * 0x18;
            p->y2    = -0x48;
            p->x3    = 0x54 - i * 0x18;
            p->y3    = -0x48;
            addPrim(&gGpuCurrentOt[10], p);
        }
    } else {
        D_dryfield_night_motel_lobby_801844D8[0] = 0;
    }
}

void func_dryfield_night_motel_lobby_80180440(Task* task, s16 key)
{
    DnmlExamineWork* work = (DnmlExamineWork*)task->work;
    s32              i;

    switch (key) {
        case 0:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                if (D_dryfield_night_motel_lobby_801844D8[0] != 0 || D_dryfield_night_motel_lobby_801844D8[1] != 0xA) {
                    D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                    D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                    D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                    D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                    D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                    D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                    D_dryfield_night_motel_lobby_801844D8[0] = key;
                    work->field_2++;
                }
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                D_dryfield_night_motel_lobby_801844D8[work->field_2] = 0xA;
                D_dryfield_night_motel_lobby_801844D8[6]             = D_dryfield_night_motel_lobby_801844D8[5];
                D_dryfield_night_motel_lobby_801844D8[5]             = D_dryfield_night_motel_lobby_801844D8[4];
                D_dryfield_night_motel_lobby_801844D8[4]             = D_dryfield_night_motel_lobby_801844D8[3];
                D_dryfield_night_motel_lobby_801844D8[3]             = D_dryfield_night_motel_lobby_801844D8[2];
                D_dryfield_night_motel_lobby_801844D8[2]             = D_dryfield_night_motel_lobby_801844D8[1];
                D_dryfield_night_motel_lobby_801844D8[1]             = D_dryfield_night_motel_lobby_801844D8[0];
                D_dryfield_night_motel_lobby_801844D8[0]             = key;
                work->field_2++;
            }
            break;
        case 10:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                if (D_dryfield_night_motel_lobby_801844D8[0] != 0 || D_dryfield_night_motel_lobby_801844D8[1] != 0xA) {
                    D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                    D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                    D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                    D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                    D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                    D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                    D_dryfield_night_motel_lobby_801844D8[0] = 0;
                    work->field_2++;
                    if (work->field_2 < 7) {
                        D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                        D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                        D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                        D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                        D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                        D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                        D_dryfield_night_motel_lobby_801844D8[0] = 0;
                        work->field_2++;
                    }
                }
            }
            break;
        case 11:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            work->field_2 = 0;
            work->field_7 = 1;
            for (i = 0; i < 7; i++) {
                D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
            }
            break;
        case 12:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            work->field_2 = 0;
            work->field_7 = 1;
            for (i = 0; i < 7; i++) {
                D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
            }
            break;
        case 13:
            if (func_dryfield_night_motel_lobby_80180734() != 0) {
                work->field_8 = 1;
            } else {
                SndEvt_EnqueueType6(0x53110009, 0, 0);
            }
            break;
    }
}

/// Whether the keypad holds the lobby's code: exactly four digits, the three
/// older slots still `0xA`, and those four reading `3 0 3 3` in the order they
/// were typed.
s16 func_dryfield_night_motel_lobby_80180734(void)
{
    u8* p = D_dryfield_night_motel_lobby_801844D8;

    if (p[6] != 0xA) {
        return 0;
    }
    if (p[5] != p[6]) {
        return 0;
    }
    if (p[4] != p[5]) {
        return 0;
    }
    if (p[3] != 3) {
        return 0;
    }
    if (p[2] != 0) {
        return 0;
    }
    /* Compares the third digit with the first rather than against a repeated
       literal: the earlier test leaves that load live, and re-testing it is
       what keeps it in one register instead of a fresh `addiu`. */
    if (p[1] != p[3]) {
        return 0;
    }
    return p[0] == 3;
}

/// Moves the action-prompt cursor of each pad `task->spawnArg1` selects (1:
/// port 0, 2: port 1, otherwise both) from its analog stick and d-pad, clamps
/// it to the screen, updates the press state of its two buttons and draws it.
void func_dryfield_night_motel_lobby_801807C0(Task* task)
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
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
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
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
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
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_dryfield_night_motel_lobby_80180C20(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues the action-prompt cursor icon, a textured quad, at (`x`, `y`) into
/// the head of the current OT. `variant` is the prompt's mode: 2 selects
/// palette 0x3C87, any other non-zero value 0x3C88, and 0 draws nothing.
void func_dryfield_night_motel_lobby_80180C20(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

void func_dryfield_night_motel_lobby_80180D08(Task* task)
{
    TaskFunc states[2] = { func_dryfield_night_motel_lobby_80181298, func_dryfield_night_motel_lobby_801807C0 };

    states[task->state](task);
}

/// Runs the examine task's current state: the eleven handlers of
/// `D_dryfield_night_motel_lobby_8017D6B0` are copied onto the stack and the
/// one `Task::state` names is called.
void func_dryfield_night_motel_lobby_80180D58(Task* task)
{
    TaskFuncTable11 states;

    states = D_dryfield_night_motel_lobby_8017D6B0;
    states.funcs[task->state](task);
}

s32 func_dryfield_night_motel_lobby_80180DE4(OverlayHotspot* table, s16 x, s16 y)
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

void func_dryfield_night_motel_lobby_80180E98(Task* task)
{
    DnmlExamineWork* work;
    OverlayHotspot*  hs;
    u8*              p;
    u8               empty;
    s32              i;

    work = memCalloc(0xA, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2          = Task_SpawnFromTable(D_dryfield_night_motel_lobby_80182814, 0, 1, 0);
    task->work               = (TaskIdMap*)work;
    Mc_SaveData.at4.loc.view = 6;
    /* The once-loop folds away, but `flow` counts its references at loop depth
       2: without it the state load is scheduled above the mode store. */
    do {
        task->state++;
    } while (0);
    Display_AcquireRef();
    for (hs = D_dryfield_night_motel_lobby_80182820; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    /* The fill value has to reach the store through a register and the pointer
       has to be built from the index: a literal store, or a `&code[6]` folded
       into the symbol, compiles to a different loop. */
    empty = 0xA;
    i     = 6;
    p     = &D_dryfield_night_motel_lobby_801844D8[i];
    for (; i >= 0; i--) {
        *p-- = empty;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

/// Sets the first action prompt to mode 1 with target id 0x80, clears its
/// screen position, and steps the task on one state.
void func_dryfield_night_motel_lobby_80180FA4(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Re-spawns the action prompt over the examine cursor: clears the highlight
/// state the prompt was left in, then hands the prompt's own coordinates and
/// this room's display mode back to `func_800D4E78`, which parks them in the
/// gameplay-side globals the prompt's display task reads.
void func_dryfield_night_motel_lobby_80180FD8(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->work;

    func_dryfield_night_motel_lobby_801802A8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Confirms the action prompt the script's current step put up: drops the
/// highlight state, then, while `func_800D4EC0` still reports a prompt on
/// screen, flags the step busy in `promptBusy` (which the cursor draw in
/// `func_dryfield_night_motel_lobby_801802A8` gates its confirm on) and starts
/// cap slot 9. Advances the task to state 2 either way.
void func_dryfield_night_motel_lobby_8018103C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->work;

    func_dryfield_night_motel_lobby_801802A8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        work->promptBusy = 1;
        Gp_StartCapSlot(9, 0, 0);
    }
    task->state = 2;
}

void func_dryfield_night_motel_lobby_801810AC(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 4;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    SOFT_BARRIER();
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}

void func_dryfield_night_motel_lobby_80181138(Task* arg0)
{
    Gp_ApplyAreaRecs(&D_dryfield_night_motel_lobby_801844AC);
    gGameSession->eventState = 1;
    taskKill(arg0->spawnArg2);
    GameFlag_SetNibble(0x74, 1);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_night_motel_lobby_8018119C(Task* arg0)
{
    SndEvt_EnqueueType6(0x53110008, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_night_motel_lobby_801811E0(Task* arg0)
{
    Gp_RunCapCmd1(8);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_night_motel_lobby_80181218(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void func_dryfield_night_motel_lobby_8018122C(Task* arg0)
{
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 4;
    Task_RequestKill(arg0, 0);
}

/// Resets both action prompts - cursor position cleared, target id 0x100,
/// `field_E` 0xF, both buttons' held counts cleared, mode 1 - and steps the
/// task on one state.
void func_dryfield_night_motel_lobby_80181298(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}

void func_dryfield_night_motel_lobby_801812F8(void)
{
    switch (gGameSession->at4.loc.view) {
        case 2:
            func_dryfield_night_motel_lobby_80181404(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x60);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[1], 2, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[2], 1, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[3], 1, 0x300);
            break;
        case 3:
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E8[0], 2, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E8[3], 1, 0x300);
            break;
        case 4:
            func_dryfield_night_motel_lobby_80181404(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x60);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[1], 2, 0x300);
            break;
        case 5:
            func_dryfield_night_motel_lobby_80181878(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x30);
            break;
    }
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1` scales
/// `gDisplayState.animFrame` into `rsin` so the lit vertex pulses as
/// `rsin(...) / 34 + 0x78` on green and blue.
void func_dryfield_night_motel_lobby_80181404(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
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
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues a gouraud glow around the projected
/// centre: eight wedges at the outer radius in half the pulse colour, eight at
/// half that radius in the full colour, and four cross wedges reaching from
/// the inner radius outwards. `arg2` is a signed half-extent; the outer radius
/// is `(s16)arg2 * 64 / otz` and the inner `(s16)arg2 * 8 / otz`. `arg1`
/// scales `gDisplayState.animFrame` into `rsin`, so the centre colour pulses as
/// `rsin(...) / 34 + 0x78` on green and blue.
void func_dryfield_night_motel_lobby_80181878(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                pulse;
    s32                color;
    s32                half;
    s32                size;
    s32                ang;
    s32                t;
    s32                t2;
    s32                u;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
    gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
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
    SCRATCH_POP_BYTES(0x14);
}

/// Projects the point `arg0` through `Gfx_ViewWorldMtx` and, when the GTE flag
/// is non-negative, queues one semi-transparent `POLY_FT4` sprite centred on it:
/// UV column `(s16)arg1 * 40`, on-screen half-extent `(s16)arg2 * 39 / otz`, and
/// an RGB that alternates between 0x20 and 0x30 with `animFrame`. It reserves
/// 0x20 bytes of scratch but releases only 0x10 on exit.
void func_dryfield_night_motel_lobby_80182200(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x20;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x20))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x20))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x20))->otz;
        xy       = ((RoomDraw13Scratch*)tmp)->sx - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sx + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x20))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}
