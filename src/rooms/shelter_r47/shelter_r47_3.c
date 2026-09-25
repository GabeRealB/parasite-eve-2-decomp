#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_r47.h"

extern s16 D_80114D08;

/// Hotspot tables of the second cap script; `spawnArg1` 2 selects the second.
extern OverlayHotspot D_shelter_r47_8018739C[];
extern OverlayHotspot D_shelter_r47_801873D8[];

/// Area views of the map pages the second cap script steps through, indexed by
/// `ShelterR47State2::field_1C`.
extern u8 D_shelter_r47_801873FC[];

extern TaskDesc D_shelter_r47_8018760C;

/// World-space points the room's glow drawers mark, picked by the current view.
extern SVECTOR D_shelter_r47_80187624[];
extern SVECTOR D_shelter_r47_80187664[];

void func_shelter_r47_8018431C(Task* task);
void func_shelter_r47_801844A0(Task* task);
void func_shelter_r47_80184658(Task* task);
void func_shelter_r47_80184AE0(Task* task);
void func_shelter_r47_80184F40(s32 x, s32 y, s32 variant);
void func_shelter_r47_80185028(Task* task);
void func_shelter_r47_80185098(Task* task);
void func_shelter_r47_801851B8(Task* task);
s32  func_shelter_r47_801852A0(OverlayHotspot* table, s16 x, s16 y);
void func_shelter_r47_80185354(Task* task);
void func_shelter_r47_80185450(Task* task);
void func_shelter_r47_80185510(Task* task);
void func_shelter_r47_801855B8(Task* task);
void func_shelter_r47_801856AC(Task* task);
void func_shelter_r47_8018571C(Task* task);
void func_shelter_r47_8018585C(Task* task);
void func_shelter_r47_80185A4C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_r47_80185DEC(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_r47_80186260(SVECTOR* arg0, s32 arg1, s32 arg2);

/// State handlers of the room's second cap script, run by
/// `func_shelter_r47_80185214`.
const TaskFuncTable11 D_shelter_r47_8017D7DC = {
    {
        func_shelter_r47_8018431C,
        func_shelter_r47_80185354,
        func_shelter_r47_801844A0,
        func_shelter_r47_80185450,
        func_shelter_r47_80184658,
        func_shelter_r47_80185510,
        func_shelter_r47_801855B8,
        func_shelter_r47_801856AC,
        func_shelter_r47_8018571C,
        func_shelter_r47_80185028,
        func_shelter_r47_80185098,
    },
};

/// State-0 entry of the second cap script. It allocates the `ShelterR47State2`
/// work, spawns the companion task from `D_shelter_r47_8018760C`, picks the
/// hotspot table by `spawnArg1` (2 selects the alternate table) and clears every
/// entry's `hit`. It holds the HUD and cutscene, sets the two quads' and the
/// sprite's targets, saves the view byte and switches the view to 0x25. With
/// `spawnArg1` 1 or 2 it also opens the quads fully, starts the fade at 0xFF
/// and records the argument in `field_2A`.
///
/// The empty `do {} while (0)` statements are required for the match: their
/// loop notes stop the scheduler moving instructions across them.
void func_shelter_r47_8018431C(Task* task)
{
    ShelterR47State2* state;
    s16               spriteX;
    OverlayHotspot*   hs;
    s32               arg;
    u8                view;
    s32               level;
    s16               quadW, quadH, quad2W, quad2H;

    state = (ShelterR47State2*)memCalloc(0x30, false);
    if (state == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(&D_shelter_r47_8018760C, 0, 1, 0);
    task->work      = (void*)state;
    task->state    += 1;
    Display_AcquireRef();
    state->hotspots = task->spawnArg1 == 2 ? D_shelter_r47_801873D8 : D_shelter_r47_8018739C;
    do {
    } while (0);
    for (hs = state->hotspots; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    quadW                      = 0xE8;
    quadH                      = 0xCE;
    quad2W                     = 0x50;
    quad2H                     = 0x60;
    spriteX                    = -0x9C;
    do {
    } while (0);
    view                     = Mc_SaveData.at4.loc.view;
    state->field_20          = -0x104;
    state->field_E           = quadW;
    state->field_10          = quadH;
    state->field_16          = quad2W;
    state->field_18          = quad2H;
    state->field_1E          = spriteX;
    state->field_29          = view;
    Mc_SaveData.at4.loc.view = 0x25;
    if ((arg = task->spawnArg1) == 1 || arg == 2) {
        state->fade     = 0xFF;
        level           = (u8)state->fade;
        state->field_A  = quadW;
        state->field_C  = quadH;
        state->field_12 = quad2W;
        state->field_14 = quad2H;
        state->field_20 = spriteX;
        Fade_DrawOverlay(level, level, level, 2);
        state->field_2A = arg;
    }
}

/// Idle state of the second cap script. It counts `field_2C` down (with a
/// sound on reaching zero), runs `func_shelter_r47_801851B8`, and while no cap
/// is running and `field_2B` is clear, hit-tests the cursor against `hotspots`.
/// A confirmed hit (`buttons[0].state == 2`) latches the hotspot's `id` and
/// `promptKind` and advances to state 3; `buttons[1].state == 2` advances to
/// state 5 when `field_2A` is 0 and otherwise starts cap slot 0x25. `field_2A`
/// of 3 moves straight to state 9.
void func_shelter_r47_801844A0(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    ShelterR47State2* st     = (ShelterR47State2*)task->work;
    OverlayHotspot*   hs     = st->hotspots;

    if (st->field_2C != 0) {
        if (--st->field_2C == 0) {
            SndEvt_EnqueueType6(0x542F0003, 0, 0);
        }
    }
    func_shelter_r47_801851B8(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if ((Gp_CapBusy() != 0) || (st->field_2B != 0)) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    if (st->field_2A == 3) {
        task->state = 9;
        return;
    }
    prompt->targetId = 0x80;
    if (func_shelter_r47_801852A0(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->field_1A     = hs->id;
                    st->promptKind   = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        if (st->field_2A == 0) {
            task->state = 5;
            return;
        }
        Gp_StartCapSlot(0x25, 0, 0);
    }
}

/// State 4 of the second cap script: acts on the hotspot latched in
/// `field_1A` once `func_800D4EC0` reports non-zero. Hotspots 1 and 4 start one
/// of five cap slots picked by `field_1C`. Hotspots 2 and 3 start a cap while
/// their latch (`D_shelter_r47_8018A696` / `D_shelter_r47_8018A697`) is clear,
/// setting it, or otherwise set `field_1E` to -0x104, clear
/// `field_E`, `field_10`, `field_16` and `field_18`, play sound 0x542F0004 and
/// move to state 6. Every other path returns to state 2, except an unknown
/// hotspot id, which leaves the state unchanged.
void func_shelter_r47_80184658(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    ShelterR47State2* st     = (ShelterR47State2*)task->work;

    func_shelter_r47_801851B8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        switch (st->field_1A) {
            case 1:
                switch (st->field_1C) {
                    case 0:
                        Gp_StartCapSlot(0x1D, 0, 0);
                        break;
                    case 1:
                        Gp_StartCapSlot(0x1E, 0, 0);
                        break;
                    case 2:
                        Gp_StartCapSlot(0x1F, 0, 0);
                        break;
                    case 3:
                        Gp_StartCapSlot(0x20, 0, 0);
                        break;
                    case 4:
                        Gp_StartCapSlot(0x21, 0, 0);
                        break;
                }
                break;
            case 2:
                if (st->field_2A != 0) {
                    Gp_StartCapSlot(0x24, 0, 0);
                    task->state = 2;
                    return;
                }
                if (D_shelter_r47_8018A696 == 0) {
                    Gp_StartCapSlot(0x35, 0, 0);
                    D_shelter_r47_8018A696 = 1;
                    task->state            = 2;
                    return;
                }
                goto snd;
            case 3:
                if (st->field_2A == 0 && D_shelter_r47_8018A697 == 0) {
                    Gp_StartCapSlot(0x34, 0, 0);
                    D_shelter_r47_8018A697 = 1;
                    task->state            = 2;
                    return;
                }
            snd:
                st->field_1E = -0x104;
                st->field_E  = 0;
                st->field_10 = 0;
                st->field_16 = 0;
                st->field_18 = 0;
                task->state  = 6;
                SndEvt_EnqueueType6(0x542F0004, 0, 0);
                SndEvt_EnqueueType7(0x542F0005, 1);
                return;
            case 4:
                switch (st->field_1C) {
                    case 0:
                        Gp_StartCapSlot(0x2F, 0, 0);
                        break;
                    case 1:
                        Gp_StartCapSlot(0x30, 0, 0);
                        break;
                    case 2:
                        Gp_StartCapSlot(0x31, 0, 0);
                        break;
                    case 3:
                        Gp_StartCapSlot(0x32, 0, 0);
                        break;
                    case 4:
                        Gp_StartCapSlot(0x33, 0, 0);
                        break;
                }
                break;
            default:
                return;
        }
    }
    task->state = 2;
}

/// Outlines `rect` in (`r`, `g`, `b`) with four flat `LINE_F2` edges linked
/// into `gGpuCurrentOt[1]`.
void func_shelter_r47_8018489C(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

/// Per-frame cursor driver of the action prompt, state 1 of the two-state
/// dispatcher that runs it.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it) and then the
/// d-pad, whose four bits select one of eight headings fed to `rsin`/`rcos`,
/// into the prompt's 1/512-pixel position, clamps that to the screen,
/// classifies the confirm (0x40) and cancel (0xA0) buttons into the prompt's
/// two button slots, and hands the rounded position to `func_shelter_r47_80184F40` to
/// draw the cursor. `RoomActionPrompt::targetId` serves as the cursor speed and
/// `field_E` as the double-press window: a second press within that many frames
/// without the cursor having moved reports state 4 instead of 2.
void func_shelter_r47_80184AE0(Task* task)
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
        func_shelter_r47_80184F40(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues one 16x24 textured quad, the action prompt's cursor icon, at (`x`,
/// `y`) into the head of the current OT. `variant` selects the palette, 0x3C87
/// when it is 2 and 0x3C88 otherwise; 0 draws nothing.
void func_shelter_r47_80184F40(s32 x, s32 y, s32 variant)
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

void func_shelter_r47_80185028(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
    state->fade = 0;
    task->state++;
}

void func_shelter_r47_80185098(Task* task)
{
    ShelterR47State2* state;
    u16               fade;
    u8                level;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
    fade        = state->fade + 0x10;
    state->fade = fade;
    if ((s16)fade >= 0x100) {
        state->fade = 0xFF;
        if (task->spawnArg1 != 1) {
            Gp_MsgPlayerWeapon(1);
        }
        Gp_MsgPlayer3F3(1);
        Display_ReleaseRef();
        if (state->field_2A != 1) {
            gGameSession->eventState = 0;
        }
        gGameSession->cutsceneHold = 0;
        taskKill((Task*)task->spawnArg2);
        Task_RequestKill(task, 0);
    }
    SndEvt_EnqueueType7(0x542F0005, 1);
    level = (u8)state->fade;
    Fade_DrawOverlay(level, level, level, 2);
}

void func_shelter_r47_801851B8(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
}

/// Dispatcher of the second cap script: copies the state table
/// `D_shelter_r47_8017D7DC` to the stack and runs the entry for the task's
/// state.
void func_shelter_r47_80185214(Task* task)
{
    TaskFuncTable11 states;

    states = D_shelter_r47_8017D7DC;
    states.funcs[task->state](task);
}

s32 func_shelter_r47_801852A0(OverlayHotspot* table, s16 x, s16 y)
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

void func_shelter_r47_80185354(Task* task)
{
    ShelterR47State2* state;
    RoomActionPrompt* prompt = &D_80114D28;
    u8                level;

    state = (ShelterR47State2*)task->work;
    if (state->field_2A == 1) {
        level = state->fade;
        Fade_DrawOverlay(level, level, level, 2);
        task->state      = 8;
        prompt->targetId = 0;
        prompt->mode     = 0;
    } else if (state->field_2A == 2) {
        prompt->targetId = 0;
        prompt->mode     = 0;
        if ((s16)state->fade > 0) {
            state->fade -= 8;
            if ((s16)state->fade < 0) {
                state->fade = 0;
                Gp_StartCapSlot(0x12, 0, 0);
                task->state++;
            } else {
                level = state->fade;
                Fade_DrawOverlay(level, level, level, 2);
            }
        }
    } else {
        state->field_2C  = 6;
        prompt->targetId = 0x80;
        prompt->mode     = 1;
        task->state++;
    }
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
}

void func_shelter_r47_80185450(Task* task)
{
    ShelterR47State2* state;
    RoomActionPrompt* prompt = &D_80114D28;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_801851B8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (state->field_2A == 0) {
        if (state->field_1A == 2 && D_shelter_r47_8018A696 == 0) {
            state->promptKind = 0;
        }
        if (state->field_1A == 3 && D_shelter_r47_8018A697 == 0) {
            state->promptKind = 0;
        }
    }
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, state->promptKind);
    task->state = 4;
}

void func_shelter_r47_80185510(Task* task)
{
    ShelterR47State2* state;

    state      = (ShelterR47State2*)task->work;
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    SndEvt_EnqueueType7(0x542F0005, 1);
    Display_ReleaseRef();
    Mc_SaveData.at4.loc.view   = state->field_29;
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    taskKill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

void func_shelter_r47_801855B8(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_801851B8(task);
    if (state->field_A <= 0) {
        state->field_E  = 0xE8;
        state->field_10 = 0xCE;
        state->field_1E = -0x9C;
        switch (state->field_1A) {
            case 2:
                state->field_1C--;
                if (state->field_1C < 0) {
                    state->field_1C = 4;
                }
                Mc_SaveData.at4.loc.view = D_shelter_r47_801873FC[state->field_1C];
                break;
            case 3:
                state->field_1C++;
                if (state->field_1C >= 5) {
                    state->field_1C = 0;
                }
                Mc_SaveData.at4.loc.view = D_shelter_r47_801873FC[state->field_1C];
                break;
        }
        task->state++;
    }
}

void func_shelter_r47_801856AC(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_801851B8(task);
    if (state->field_A >= 0xB5) {
        SndEvt_EnqueueType6(0x542F0003, 0, 0);
        state->field_16 = 0x50;
        state->field_18 = 0x60;
        task->state     = 2;
    }
}

void func_shelter_r47_8018571C(Task* task)
{
    ShelterR47State2* state;
    RoomActionPrompt* prompt = &D_80114D28;
    u8                level;

    state = (ShelterR47State2*)task->work;
    if ((s16)state->fade > 0) {
        state->fade -= 8;
        if ((s16)state->fade < 0) {
            state->fade = 0;
        } else {
            level = state->fade;
            Fade_DrawOverlay(level, level, level, 2);
        }
    }
    state->field_24++;
    if (state->field_24 > 300 || gGameSession->evtSkipped != 0) {
        task->state = 9;
    }
    func_shelter_r47_801851B8(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
    }
}

/// Two-state dispatcher of the action prompt, with its handler table built on
/// the stack: state 0 runs `func_shelter_r47_8018585C` and state 1 runs
/// `func_shelter_r47_80184AE0`.
void func_shelter_r47_8018580C(Task* task)
{
    TaskFunc funcs[2] = {
        func_shelter_r47_8018585C,
        func_shelter_r47_80184AE0,
    };

    funcs[task->state](task);
}

/// State 0 of the action prompt's dispatcher: resets both prompt slots before
/// the first cursor scan (position and hold counters cleared, cursor speed
/// 0x100, double-press window 0xF, `mode` 1) and advances the task's state.
void func_shelter_r47_8018585C(Task* task)
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

void func_shelter_r47_801858BC(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 5:
            func_shelter_r47_80185DEC(&D_shelter_r47_80187624[0], 0x60, 0xA0);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[1], 0x280, 0x444);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[2], 0x280, 0x444);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[3], 0x280, 0x444);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[4], 0x280, 0x444);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[5], 0x280, 0x444);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[6], 0x100, 0x344);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[7], 0x300, 0x344);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[8], 0x280, 0x344);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[9], 0x180, 0x344);
            break;
        case 13:
            func_shelter_r47_80185A4C(&D_shelter_r47_80187664[0], 0x280, 0x344);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187664[1], 0x180, 0x344);
            break;
        case 14:
            func_shelter_r47_80185DEC(&D_shelter_r47_80187624[0], 0x60, 0xA0);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[6], 0x100, 0x344);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[8], 0x280, 0x344);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[9], 0x180, 0x344);
            break;
        case 44:
            func_shelter_r47_80186260(&D_shelter_r47_80187624[0], 0x60, 0xA0);
            func_shelter_r47_80185A4C(&D_shelter_r47_80187624[6], 0x100, 0x344);
            break;
    }
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues four gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / otz`. `arg2` packs three RGB nibbles for the inner
/// vertex, OR'd with `((u8)gDisplayState.animFrame & 1) * 8`.
void func_shelter_r47_80185A4C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1` scales
/// `gDisplayState.animFrame` into `rsin` so the lit vertex pulses as
/// `rsin(...) / 34 + 0x78` on green and blue.
void func_shelter_r47_80185DEC(SVECTOR* arg0, s32 arg1, s32 arg2)
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

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
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
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges around the projected centre. `arg2` is a signed
/// half-extent; on-screen radii are `(s16)arg2 * 64 / otz` (outer) and
/// `(s16)arg2 * 8 / otz` (inner). `arg1` scales `gDisplayState.animFrame` into
/// `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
void func_shelter_r47_80186260(SVECTOR* arg0, s32 arg1, s32 arg2)
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

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
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
