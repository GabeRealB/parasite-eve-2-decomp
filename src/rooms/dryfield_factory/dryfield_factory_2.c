#include "common.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/dryfield_factory.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

/// The room script task's work block as the prompt-spawning state reads it:
/// `promptKind` is the display mode forwarded to `func_800D4E78`, read signed.
typedef struct RoomUtil21Work {
    /* 0x00 */ byte pad_0[0xE];
    /* 0x0E */ s8   promptKind;
} RoomUtil21Work;

extern s16 D_80114D08;

extern TaskDesc       D_dryfield_factory_80186E88[];
extern GpMsgEntry     D_dryfield_factory_80186EA0[];
extern OverlayHotspot D_dryfield_factory_80186EB0[];
extern SVECTOR        D_dryfield_factory_80186EF8;
extern SVECTOR        D_dryfield_factory_80186F00;
extern SVECTOR        D_dryfield_factory_80186F08;

void func_dryfield_factory_80180A4C(Task* task);
void func_dryfield_factory_80181538(s32 x, s32 y, s32 variant);
s32  func_dryfield_factory_80181778(OverlayHotspot* table, s16 x, s16 y);
void func_dryfield_factory_8018182C(Task* task);
void func_dryfield_factory_80181938(Task* task);
void func_dryfield_factory_8018196C(Task* task);
void func_dryfield_factory_801819BC(Task* task);
void func_dryfield_factory_80181A24(Task* task);
void func_dryfield_factory_80181AB8(Task* task);
void func_dryfield_factory_80181BB4(Task* task);

/// State handlers of the room's script task, run by
/// `func_dryfield_factory_8018169C`: set-up, prompt arming, the idle hotspot
/// scan, prompt spawning, the prompt state, the exit and the wait for the
/// message handler's trigger.
const TaskFuncTable7 D_dryfield_factory_8017D678 = {
    {
        func_dryfield_factory_8018182C,
        func_dryfield_factory_80181938,
        func_dryfield_factory_80180A4C,
        func_dryfield_factory_8018196C,
        func_dryfield_factory_801819BC,
        func_dryfield_factory_80181A24,
        func_dryfield_factory_80181AB8,
    },
};

/// Idle state of the room's script task. It counts down the delay the prompt
/// state armed and, once that is spent and no cap is playing, hit-tests the
/// room's hotspots under the cursor: a confirmed hit copies the hotspot's `id`
/// and `promptKind` into the work block and advances to state 3, and the
/// cancel button leaves for state 5.
void func_dryfield_factory_80180A4C(Task* task)
{
    RoomActionPrompt*       prompt = &D_80114D28;
    OverlayHotspot*         hs     = D_dryfield_factory_80186EB0;
    NightFactoryScriptWork* st     = (NightFactoryScriptWork*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (st->field_8 != 0) {
        st->field_8 = st->field_8 - 1;
    }
    if ((Gp_CapBusy() != 0) || (st->field_8 != 0)) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_dryfield_factory_80181778(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->field_C      = hs->id;
                    st->field_E      = hs->promptKind;
                    task->state      = 3;
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

/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left edge of the rectangle spanning
/// (`x`, `y`) to (`x + w`, `y + h`) -- each linked into `gGpuCurrentOt[1]`.
void func_dryfield_factory_80180BA4(RoomRect* rect, u8 r, u8 g, u8 b)
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

void func_dryfield_factory_80180DE8(Task* task, s16 step)
{
    s32 id;
    s32 state;

    if (GameFlag_GetNibble(0x48) != 0) {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                if (!(GameFlag_GetNibble(0x49) & 2)) {
                    GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) | 2);
                    if (GameFlag_GetNibble(0x47) == 0) {
                        Mc_SaveData.at4.loc.view = 0x12;
                    } else {
                        Mc_SaveData.at4.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    Gp_StartCapSlot(8, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                if (GameFlag_GetNibble(0x49) & 2) {
                    GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) & ~2);
                    if (GameFlag_GetNibble(0x47) == 0) {
                        Mc_SaveData.at4.loc.view = 0x12;
                    } else {
                        Mc_SaveData.at4.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    Gp_StartCapSlot(9, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) ^ 1);
                if (GameFlag_GetNibble(0x47) == 0) {
                    Mc_SaveData.at4.loc.view = 0x12;
                } else {
                    Mc_SaveData.at4.loc.view = 0x13;
                }
                state       = 6;
                task->state = state;
                break;
            case 3:
                Gp_StartCapSlot(6, 0, 1);
                state       = 2;
                task->state = state;
                break;
            case 4:
                Gp_StartCapSlot(7, 0, 0);
                state       = 2;
                task->state = state;
                break;
        }
    } else {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(8, 0, 0);
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(9, 0, 0);
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(0xA, 0, 0);
                break;
            case 3:
                Gp_StartCapSlot(6, 0, 0);
                break;
            case 4:
                Gp_StartCapSlot(7, 0, 0);
                break;
        }
        task->state = 2;
    }
}

void func_dryfield_factory_801810D8(Task* task)
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
        pad    = &Pad_States[port];
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
        func_dryfield_factory_80181538(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues one 16x24 textured quad -- the room's on-screen action prompt icon --
/// at (`x`, `y`) into the head of the current OT. `variant` selects the palette,
/// 0x3C87 when it is 2 and 0x3C88 otherwise, and 0 draws nothing at all.
void func_dryfield_factory_80181538(s32 x, s32 y, s32 variant)
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

/// Sets the skip-link byte on the second sprite command of view 9 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.loc.stage` is 2.
void func_dryfield_factory_80181620(s32 arg0)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][8].field_4;
        if (!(arg0 & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}

/// Runs the room script task's current state. The seven handlers are copied
/// onto the stack first, so the call goes through a local table rather than
/// through `.rodata`.
void func_dryfield_factory_8018169C(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_factory_8017D678;
    sp.funcs[task->state](task);
}

void func_dryfield_factory_80181718(Task* task)
{
    TaskFunc states[2] = { func_dryfield_factory_80181BB4, func_dryfield_factory_801810D8 };

    states[task->state](task);
}

/// Message 0x13F3 handler of the room's script task: raises the one-shot
/// trigger `field_A` in its work block, which the script's wait state consumes.
void func_dryfield_factory_80181768(Task* task)
{
    ((NightFactoryScriptWork*)task->work)->field_A = 1;
}

s32 func_dryfield_factory_80181778(OverlayHotspot* table, s16 x, s16 y)
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

/// State 0 of the room's script task: allocates its work block, spawns the
/// child task, publishes the script's message table, picks the global mode
/// byte from game flag 0x48, advances, and clears the hotspot hits while
/// holding the HUD for the cutscene.
void func_dryfield_factory_8018182C(Task* task)
{
    NightFactoryScriptWork* work;
    OverlayHotspot*         hs;

    work = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(D_dryfield_factory_80186E88, 0, 1, 0);
    task->work      = (TaskIdMap*)work;
    task->msgTable  = D_dryfield_factory_80186EA0;
    if (GameFlag_GetNibble(0x48) == 0) {
        Mc_SaveData.at4.loc.view = 0xC;
    } else {
        Mc_SaveData.at4.loc.view = 5;
    }
    task->state++;
    Display_AcquireRef();
    for (hs = D_dryfield_factory_80186EB0; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    work->field_8              = 0;
}

/// Arms the action prompt for a hotspot and steps the caller's script on one
/// state: marks the prompt as highlighted (`mode` 1) for the fixed target id
/// 0x80 and resets the on-screen position, which `func_800D4E78` fills in again
/// when the prompt is actually spawned.
void func_dryfield_factory_80181938(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with the display mode this state picked.
void func_dryfield_factory_8018196C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    RoomUtil21Work*   work   = (RoomUtil21Work*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Prompt state of the room's script task: clears the cursor highlight and,
/// while `func_800D4EC0` still reports a prompt on screen, runs the cap step
/// the work block names; otherwise it returns to the idle state 2. Either way
/// it re-arms the idle state's delay.
void func_dryfield_factory_801819BC(Task* task)
{
    NightFactoryScriptWork* work = (NightFactoryScriptWork*)task->work;

    D_80114D28.mode     = 0;
    D_80114D28.targetId = 0;
    if (func_800D4EC0() != 0) {
        func_dryfield_factory_80180DE8(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}

void func_dryfield_factory_80181A24(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgAlly3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 3;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}

/// Wait state of the room's script task: once the one-shot trigger `field_A`
/// has been raised by the script's message handler, picks the global mode
/// byte from game flag 0x48, re-arms the idle delay, consumes the trigger and
/// returns the script to its idle state 2.
void func_dryfield_factory_80181AB8(Task* task)
{
    RoomActionPrompt*       prompt;
    NightFactoryScriptWork* work;

    prompt           = &D_80114D28;
    work             = (NightFactoryScriptWork*)task->work;
    prompt->targetId = 0;
    prompt->mode     = 0;
    if (work->field_A != 0) {
        if (GameFlag_GetNibble(0x48) == 0) {
            Mc_SaveData.at4.loc.view = 0xC;
        } else {
            Mc_SaveData.at4.loc.view = 5;
        }
        work->field_8 = 0xA;
        work->field_A = 0;
        task->state   = 2;
    }
}

/// Sets the skip-link byte on the second sprite command of view 11 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.loc.stage` is 2.
void func_dryfield_factory_80181B38(s32 arg0)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][10].field_4;
        if (!(arg0 & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}

/// Resets both action-prompt slots before a script's first cursor scan and steps
/// the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1).
void func_dryfield_factory_80181BB4(Task* task)
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

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges, tinted. `arg1` is a
/// signed half-extent; on-screen radii are `(s16)arg1 * 64 / otz` (outer) and
/// `(s16)arg1 * 8 / otz` (inner). `arg2` packs the tint into four nibbles,
/// `[shift][r][g][b]`: each colour nibble is scaled to 8 bits by `<< 4`, and
/// bit 0 of `gDisplayState.animFrame` is added to all three channels shifted
/// left by the top nibble, so the disc flickers on alternating frames. The
/// outer ring draws at half brightness first and full brightness second; the
/// inner cross uses the halved colour throughout.
void func_dryfield_factory_80181C14(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                packed;
    s32                blend;
    s32                size;
    s32                otz;
    s32                rOuter;
    s32                rInner;
    s32                ang;
    s32                t;
    s32                t2;
    s32                r;
    s32                g;
    s32                b;
    s32                rh;
    s32                gh;
    s32                bh;

    {
        void** scratch;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        block   = (RoomDraw05Scratch*)(*scratch = head - 0x14);
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        size          = (s16)arg1;
        otz           = block->otz + 1;
        rOuter        = (size * 64) / otz;
        block->otz    = otz;
        ds            = &gDisplayState;
        blend         = ds->animFrame;
        block->rOuter = rOuter;
        rInner        = (size * 8) / block->otz;
        packed        = arg2 << 16;
        blend         = blend & 1;
        blend         = blend << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = rInner;
        ang           = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            rh = (u8)r >> 1;
            gh = (u8)g >> 1;
            bh = (u8)b >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rh, gh, bh);
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
            setRGB2(prim, r, g, b);
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

        r   = (u8)rh;
        g   = (u8)gh;
        b   = (u8)bh;
        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang - 0x400)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ang - 0x400)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x400)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x400)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang + 0x400)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang + 0x400)) >> 11);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x800)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x800)) >> 12);
            ang     += 0x800;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Per-frame effect: draws up to three glowing discs at fixed points in the
/// room. The draw set is selected by the stage-visit byte
/// `gGameSession->at4.loc.view` taken as a bit index, and each group also gates on a
/// story flag, so a disc only appears on the visits and after the event that
/// the flag records.
void func_dryfield_factory_801825F0(Task* task)
{
    s32 state;

    state = 1 << gGameSession->at4.loc.view;
    if (GameFlag_GetNibble(0x48) != 0 && (state & 0x15068) != 0) {
        func_dryfield_factory_80181C14(&D_dryfield_factory_80186EF8, 0x100, 0x3660);
    }
    if (state & 0xF26C4) {
        if (GameFlag_GetNibble(0x4A) == 1) {
            func_dryfield_factory_80181C14(&D_dryfield_factory_80186F00, 0x80, 0x5A00);
        } else if (GameFlag_GetNibble(0x4A) == 2) {
            func_dryfield_factory_80181C14(&D_dryfield_factory_80186F08, 0x80, 0x50A0);
        }
    }
}
