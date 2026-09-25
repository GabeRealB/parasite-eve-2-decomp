#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_shrine.h"
#include "rooms/room_common.h"

/// Scratch state of the two falling-prop tasks, stored at `Task::work`
/// (`memCalloc(0x48)` in `func_neo_ark_shrine_8017F4C8` / `_8017F688`).
/// `color` / `light` are the prop's own matrices, republished onto
/// `TmdObject::lightMtx` / `field_20` by the two spawn handlers; `speed` /
/// `delta` / `ticks` are the fall itself, stepped by `func_neo_ark_shrine_8017F578`.
typedef struct {
    /* 0x00 */ MATRIX color;
    /* 0x20 */ MATRIX light;
    /* 0x40 */ u16    speed; ///< per-frame gravity step
    /* 0x42 */ u16    delta; ///< accumulated fall distance for this frame
    /* 0x44 */ u16    ticks; ///< frames since the fall started
    /* 0x46 */ u8     pad_46[2];
} NeoArkShrineFall;

void func_neo_ark_shrine_8017E988(s32 x, s32 y, s32 variant);
void func_neo_ark_shrine_8017F80C(Task* task);
void func_neo_ark_shrine_8017F86C(Task* task);
void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2);
void func_neo_ark_shrine_80180144(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_neo_ark_shrine_80180570(GpCoord* arg0, s32 arg1, u8* rgb);
void func_neo_ark_shrine_80180DF4(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
void func_neo_ark_shrine_80181474(GpCoord* arg0, s16 arg1, u8* arg2);

extern u8  D_8007216D;
extern s16 D_80114D08;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern TaskDesc         D_neo_ark_shrine_80182404[];
extern u16              D_neo_ark_shrine_80182410[16];
extern NeoArkShrineSlot D_neo_ark_shrine_8018256C[16];
extern SVECTOR          D_neo_ark_shrine_8018268C[];
extern SVECTOR          D_neo_ark_shrine_80182694[];
extern SVECTOR          D_neo_ark_shrine_8018269C[];
extern SVECTOR          D_neo_ark_shrine_801826AC[];
extern SVECTOR          D_neo_ark_shrine_801826C4[];
extern SVECTOR          D_neo_ark_shrine_801826D4[];
extern SVECTOR          D_neo_ark_shrine_80182704[];

/// Offset of the beam's near end from the effect's parent coordinate. The far
/// end's offset follows it directly; the beam's set-up state reaches that one
/// as element 1.
extern SVECTOR D_neo_ark_shrine_80182714[];

/// Offset of the beam's far end from the effect's parent coordinate.
extern SVECTOR D_neo_ark_shrine_8018271C;

/// Moves the action-prompt cursor from the pad: for each port the task's
/// `spawnArg1` selects, integrates the analog stick and the d-pad direction
/// into the cursor's fixed-point position, clamps it to the screen, updates
/// the two prompt buttons' press / hold states, and draws the cursor icon.
void func_neo_ark_shrine_8017E528(Task* task)
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
        func_neo_ark_shrine_8017E988(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues the action-prompt cursor icon, a 16x24 textured quad, at (`x`, `y`)
/// into the head of the current OT. `variant` selects the palette, 0x3C87 when
/// it is 2 and 0x3C88 otherwise, and 0 draws nothing.
void func_neo_ark_shrine_8017E988(s32 x, s32 y, s32 variant)
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

/// Task callback of the action-prompt cursor: state 0 resets both prompt slots,
/// state 1 moves the cursor from the pad every frame after.
void func_neo_ark_shrine_8017EA70(Task* task)
{
    TaskFunc states[2] = { func_neo_ark_shrine_8017F80C, func_neo_ark_shrine_8017E528 };

    states[task->state](task);
}

/// Per-frame helper of the cap script: animates and draws the sliding-tile
/// puzzle.
void func_neo_ark_shrine_8017EAC0()
{
    func_neo_ark_shrine_8017DF7C();
}

/// Task callback of the shrine's cap script: dispatches `Task::state` through a
/// copy of the script's state table.
void func_neo_ark_shrine_8017EAE0(Task* task)
{
    TaskFuncTable16 sp;

    sp = D_neo_ark_shrine_8017D5D0;
    sp.funcs[task->state](task);
}

/// Task callback of the shrine's first falling prop: dispatches `Task::state`
/// through a copy of the prop's state table.
void func_neo_ark_shrine_8017EB54(Task* task)
{
    TaskFuncTable4 states;

    states = D_neo_ark_shrine_8017D610;
    states.funcs[task->state](task);
}

/// Task callback of the shrine's second falling prop: dispatches `Task::state`
/// through a copy of the prop's state table.
void func_neo_ark_shrine_8017EBB8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_shrine_8017D620;
    sp.funcs[task->state](task);
}

/// Hit-tests (`x`, `y`) against every rectangle of the `-1`-terminated hotspot
/// table, setting each entry's `hit` flag, and returns whether any was hit.
s32 func_neo_ark_shrine_8017EC10(OverlayHotspot* table, s16 x, s16 y)
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

/// Task callback of the descriptor at `D_neo_ark_shrine_80182404`: allocates
/// the cap script's state, sets the global mode byte, steps the task on one
/// state and clears the shrine's hotspot list.
void func_neo_ark_shrine_8017ECC4(Task* task)
{
    NeoArkShrineScript* st;
    OverlayHotspot*     hs;

    st = memCalloc(0x10, 0);
    if (st == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2          = Task_SpawnFromTable(D_neo_ark_shrine_80182404, 0, 1, 0);
    task->work               = (TaskIdMap*)st;
    Mc_SaveData.at4.loc.view = 0xB;
    /* The once-loop folds away, but flow counts its references at loop depth
       2: without it the parameter's priority (6*2/42) loses to the state
       pointer's (3*1/10) and the two swap callee-saved homes. Keeping the
       state load below the mode store is the same loop's scheduling edge. */
    do {
        task->state++;
    } while (0);
    Display_AcquireRef();
    for (hs = D_neo_ark_shrine_80182430; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

/// Cap script state 1: sets the first action prompt's `targetId` to 0x80 and its
/// `mode` to 1, zeroes its on-screen position, and steps the script on.
void func_neo_ark_shrine_8017EDAC(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: runs the shrine's
/// per-step helper, clears the prompt's highlight state, then re-spawns the
/// prompt at the coordinates the gameplay side left in `D_80114D28` with the
/// display mode this step picked, and advances the task to state 4.
void func_neo_ark_shrine_8017EDE0(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* work   = (NeoArkShrineScript*)task->work;

    func_neo_ark_shrine_8017EAC0();
    /* Without this local-alloc ranks `work` (2 refs over 6 insns) above `task`
       (3 refs over 12), which swaps their `$s1` / `$s2` homes. */
    SOFT_TOUCH_REG(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->field_E);
    task->state = 4;
}

/// Clears the action prompt's highlight state and runs the shrine's per-step
/// helper. When `func_800D4EC0` reports success, starts cap slot 2 if the
/// script's `field_C` is 0x10, and otherwise sets `field_F` and starts cap
/// slot 1. The task advances to state 2 on every path.
void func_neo_ark_shrine_8017EE44(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* work   = (NeoArkShrineScript*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_neo_ark_shrine_8017EAC0();
    /* Same `task` / `work` home swap as `func_neo_ark_shrine_8017EDE0`. */
    SOFT_TOUCH_REG(task);
    /* Each path writes `task->state` itself. That puts a second store in the
       last arm's block, so sched1 moves the `field_F` store below the
       argument setup; jump2 then merges only the `jal` and the state store. */
    if (func_800D4EC0() == 0) {
        task->state = 2;
        return;
    }
    if (work->field_C == 0x10) {
        Gp_StartCapSlot(2, 0, 0);
        task->state = 2;
        return;
    }
    work->field_F = 1;
    Gp_StartCapSlot(1, 0, 0);
    task->state = 2;
}

void func_neo_ark_shrine_8017EED4(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 0xA;
    /* Without this the scheduler hoists the `spawnArg2` load above the
       `Mc_SaveData.at4.loc.view` byte store, which then fills `taskKill`'s delay slot. */
    SOFT_BARRIER();
    taskKill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

/// Same as `func_neo_ark_shrine_8017F320`, but it latches the script's pad
/// mode on rather than off.
void func_neo_ark_shrine_8017EF68(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    Gp_SpawnPadLerp(0x12, 0x30, 0x90);
    D_neo_ark_shrine_80186868 = 1;
    prompt->mode              = 0;
    prompt->targetId          = 0;
    func_neo_ark_shrine_8017EAC0(task);
    st->timer = 0;
    task->state++;
}

/// The script step that runs while the shrine's pad is idle: it re-clears the
/// prompt, ticks the step's timer, and once the step has run 0x1E frames latches
/// the shrine's mode — 2, or 5 when flag 0xE9 is set — into `D_8007216D` and the
/// session, which makes the room rebuild its objects, and enters state 2.
///
/// The same literal is stored in both arms on purpose: `gGameSession` is read
/// per arm, and jump_optimize's cross-jumping (post-sched2) merges the arms'
/// identical `sb` pairs into the join. Written with one shared `var_v0` the
/// stores are one pair too but the constant's `li` precedes the address, the
/// merge swallows the `gGameSession` load as well, and the function comes out
/// four insns short.
void func_neo_ark_shrine_8017EFE4(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    st->timer        = st->timer + 1;
    func_neo_ark_shrine_8017EAC0(task);
    if (st->timer >= 0x1E) {
        if (GameFlag_GetNibble(0xE9) == 0) {
            D_8007216D                 = 2;
            gGameSession->at4.loc.room = 2;
        } else {
            D_8007216D                 = 5;
            gGameSession->at4.loc.room = 5;
        }
        gGameSession->roomObjsDirty = 1;
        task->state                 = 2;
    }
}

void func_neo_ark_shrine_8017F094(Task* task)
{
    NeoArkShrineScript* st;

    st                        = (NeoArkShrineScript*)task->work;
    D_neo_ark_shrine_8018686A = 1;
    func_neo_ark_shrine_8017EAC0();
    taskKill((Task*)task->spawnArg2);
    st->timer = 0;
    task->state++;
}

void func_neo_ark_shrine_8017F0F0(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;

    st = (NeoArkShrineScript*)task->work;
    func_neo_ark_shrine_8017EAC0();
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer >= 0x1EU) {
        Task_SpawnFromTable(&D_neo_ark_shrine_80182508, 1, 0, 0);
        Mc_SaveData.at4.loc.view = 0xE;
        /* Without this the scheduler hoists the `task->state` reload above the
           `Mc_SaveData.at4.loc.view` byte store to fill its load-delay slot. */
        SOFT_BARRIER();
        st->timer = 0;
        task->state++;
    }
}

void func_neo_ark_shrine_8017F178(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;
    s32                 next;

    st        = (NeoArkShrineScript*)task->work;
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer >= 0x5AU) {
        st->timer = 0;
        if (GameFlag_GetNibble(0xE9) == 0) {
            Task_SpawnFromTable(&D_neo_ark_shrine_80182508, 2, 0, 0);
            Mc_SaveData.at4.loc.view = 0xD;
            GameFlag_SetNibble(0xE9, 1);
            next = task->state + 1;
        } else {
            next = task->state + 2;
        }
        task->state = next;
    }
}

void func_neo_ark_shrine_8017F21C(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;

    st        = (NeoArkShrineScript*)task->work;
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer == 0x1E) {
        Gp_StateF0.field_20 = 1;
    }
    if (st->timer >= 0x3CU) {
        task->state++;
    }
}

void func_neo_ark_shrine_8017F274(Task* task)
{
    Gp_StateF0.field_20         = 2;
    Mc_SaveData.at4.loc.room    = 6;
    gGameSession->at4.loc.room  = 6;
    gGameSession->roomObjsDirty = 1;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 0xA;
    Task_RequestKill(task, 0);
}

/// Runs the shrine's per-step helper and restarts the script's step timer:
/// raises a pad lerp, clears the prompt's highlight state and advances the
/// task to the next state.
void func_neo_ark_shrine_8017F320(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    Gp_SpawnPadLerp(0x12, 0x30, 0x90);
    D_neo_ark_shrine_80186868 = 0;
    prompt->mode              = 0;
    prompt->targetId          = 0;
    func_neo_ark_shrine_8017EAC0(task);
    st->timer = 0;
    task->state++;
}

/// Same as `func_neo_ark_shrine_8017EFE4`, but the mode it latches is 1, or 4
/// when flag 0xE9 is set.
void func_neo_ark_shrine_8017F398(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    st->timer        = st->timer + 1;
    func_neo_ark_shrine_8017EAC0(task);
    if (st->timer >= 0x1E) {
        if (GameFlag_GetNibble(0xE9) == 0) {
            D_8007216D                 = 1;
            gGameSession->at4.loc.room = 1;
        } else {
            D_8007216D                 = 4;
            gGameSession->at4.loc.room = 4;
        }
        gGameSession->roomObjsDirty = 1;
        task->state                 = 2;
    }
}

/// Resets the shrine's 16-slot arrangement puzzle to its starting state: clears
/// the two puzzle flags, reloads the work copy of the slot layout from the
/// room's initial-layout table, and re-seeds the slot arrangement with the
/// room's starting order.
void func_neo_ark_shrine_8017F448(void)
{
    NeoArkShrineSlot* dstSlot;
    NeoArkShrineSlot* srcSlot;
    s16*              dstOrder;
    u16*              srcOrder;
    s32               i;
    u16               y;
    u16               order;

    i                         = 0;
    dstSlot                   = D_neo_ark_shrine_8018688C;
    srcSlot                   = D_neo_ark_shrine_8018256C;
    D_neo_ark_shrine_8018686A = 0;
    D_neo_ark_shrine_80186868 = 0;
    do {
        i++;
        dstSlot->x = srcSlot->x;
        y          = srcSlot->y;
        srcSlot++;
        dstSlot->y = y;
        dstSlot++;
    } while (i < 0x10);

    i        = 0;
    dstOrder = D_neo_ark_shrine_8018686C;
    srcOrder = D_neo_ark_shrine_80182410;
    do {
        order = *srcOrder;
        srcOrder++;
        i++;
        *dstOrder = order;
        dstOrder++;
    } while (i < 0x10);
}

/// Second state of the shrine's first falling prop: allocates its 0x48-byte
/// scratch block, republishes the block's light / colour matrices onto the
/// model's `TmdObject`, parks the prop at its starting position parented to the
/// room's view coordinate system, and advances the task to the falling state.
void func_neo_ark_shrine_8017F4C8(Task* task)
{
    TmdObject*        extra;
    GpCoord*          coord;
    NeoArkShrineFall* st;

    extra      = task->extra.tmd;
    coord      = extra->coords;
    st         = (NeoArkShrineFall*)memCalloc(sizeof(NeoArkShrineFall), 0);
    task->work = (TaskIdMap*)st;
    if (st == NULL) {
        taskKill(task);
        return;
    }
    extra->lightMtx   = &st->light;
    extra->flags      = 0;
    extra->colorMtx   = &st->color;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = 0x1B58;
    coord->coord.t[1] = -0xBB8;
    coord->coord.t[2] = -0x3E8;
    func_neo_ark_shrine_8017F86C(task);
    task->state++;
}

void func_neo_ark_shrine_8017F578(Task* task)
{
    NeoArkShrineFall* st;
    GpCoord*          coord;
    u16               ticks;
    u16               speed;
    u16               delta;
    s32               y;

    st        = (NeoArkShrineFall*)task->work;
    coord     = task->extra.tmd->coords;
    ticks     = st->ticks + 1;
    st->ticks = ticks;
    if ((s16)ticks == 4) {
        Gp_SpawnPadLerp(0x18, 0x40, 0xFF);
        SndEvt_EnqueueType6(0x55150009, 0, 0);
    }
    speed             = st->speed + 1;
    delta             = st->delta + speed;
    st->delta         = delta;
    st->speed         = speed;
    y                 = coord->coord.t[1] + (s16)delta;
    coord->coord.t[1] = y;
    if (y > 0) {
        coord->coord.t[1] = 0;
        task->state++;
    }
    func_neo_ark_shrine_8017F86C(task);
}

void func_neo_ark_shrine_8017F640(Task* task)
{
    func_neo_ark_shrine_8017F86C(task);
    if (D_neo_ark_shrine_8018686A == 0) {
        task->state++;
    }
}

/// Second state of the shrine's second falling prop: as `func_neo_ark_shrine_8017F4C8`,
/// but parked at the mirror position on the far side of the shrine.
void func_neo_ark_shrine_8017F688(Task* task)
{
    TmdObject*        extra;
    GpCoord*          coord;
    NeoArkShrineFall* st;

    extra      = task->extra.tmd;
    coord      = extra->coords;
    st         = (NeoArkShrineFall*)memCalloc(sizeof(NeoArkShrineFall), 0);
    task->work = (TaskIdMap*)st;
    if (st == NULL) {
        taskKill(task);
        return;
    }
    extra->lightMtx   = &st->light;
    extra->flags      = 0;
    extra->colorMtx   = &st->color;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = 0x222E;
    coord->coord.t[1] = -0xBB8;
    coord->coord.t[2] = -0x11C6;
    func_neo_ark_shrine_8017F86C(task);
    task->state++;
}

void func_neo_ark_shrine_8017F738(Task* task)
{
    NeoArkShrineFall* st;
    GpCoord*          coord;
    u16               ticks;
    u16               speed;
    u16               delta;
    s32               y;

    st        = (NeoArkShrineFall*)task->work;
    coord     = task->extra.tmd->coords;
    ticks     = st->ticks + 1;
    st->ticks = ticks;
    if ((s16)ticks == 2) {
        SndEvt_EnqueueType6(0x5515000B, 0, 0);
    }
    if ((s16)st->ticks == 0x12) {
        Gp_SpawnPadLerp(0xA, 0xA0, 0xFF);
    }
    speed             = st->speed + 2;
    delta             = st->delta + speed;
    st->delta         = delta;
    st->speed         = speed;
    y                 = coord->coord.t[1] + (s16)delta;
    coord->coord.t[1] = y;
    if (y > 0) {
        coord->coord.t[1] = 0;
        task->state++;
    }
    func_neo_ark_shrine_8017F86C(task);
}

/// Resets both action-prompt slots and steps the task on: zeroes each slot's
/// fixed-point cursor position and its buttons' hold counters, sets
/// `targetId` to 0x100, `field_E` to 0xF and `mode` to 1.
void func_neo_ark_shrine_8017F80C(Task* task)
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

/// Tail every `NeoArkShrineFall` handler runs: clears the prop's root coordinate
/// flag, rebuilds its world matrix, and republishes the translation in
/// `func_800D7A9C`'s format, lowered by 0x320 so the prop draws on the floor.
void func_neo_ark_shrine_8017F86C(Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     vec;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// On the task's first tick stores three ids (0x601DF, 0x601FB, 0x60217) into
/// the `D_80115758` / `D_8011572C` / `D_80115750` slots; then, every tick, runs
/// `func_neo_ark_shrine_8017FC14` over the positions the current camera view
/// shows, drawn from one of the room's `SVECTOR` arrays.
void func_neo_ark_shrine_8017F8DC(Task* task)
{
    if (task->state == 0) {
        D_80115758  = 0x601DF;
        D_8011572C  = 0x601FB;
        D_80115750  = 0x60217;
        task->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_neo_ark_shrine_801826D4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 3: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[8], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[9], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[10], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 5:
        case 18: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_neo_ark_shrine_8018269C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 7: {
            SVECTOR* p = D_neo_ark_shrine_8018268C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 12: {
            SVECTOR* p = D_neo_ark_shrine_80182694;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 14: {
            SVECTOR* p = D_neo_ark_shrine_801826C4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 16: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 10:
        case 17: {
            SVECTOR* p = D_neo_ark_shrine_80182704;
            func_neo_ark_shrine_8017FC14(&p[0], 0, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 0, 0x300);
            break;
        }
    }
}

/// Projects the world-space point `pos` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues one semi-transparent `POLY_FT4` sprite
/// centred on it (tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects
/// the 40-texel UV column `(s16)arg1 * 40` at v=0..0x27, and `arg2` is a signed
/// half-extent whose on-screen radius is `(s16)arg2 * 39 / otz`. All three RGB
/// channels take `0x20`, plus 0x10 on odd `animFrame` values, so the sprite
/// flickers frame to frame.
void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                idx;
    s32                u0;
    s32                u1;
    s32                sarg;
    s32                blend;
    s16                xy;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(pos);
    gte_rtps();
    ds    = &gDisplayState;
    blend = (((u8)ds->animFrame & 1) * 16) + 0x20;
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        sarg        = (s16)arg2;
        setRGB0(prim, blend, blend, blend);
        prim->u0                          = u0;
        prim->v0                          = 0;
        prim->u1                          = u1;
        prim->v1                          = 0;
        prim->u2                          = u0;
        prim->v2                          = 0x27;
        prim->u3                          = u1;
        prim->v3                          = 0x27;
        prim->code                       |= 2;
        ((RoomDraw13Scratch*)tmp)->radius = (sarg * 40 - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy                                = ((RoomDraw13Scratch*)tmp)->sx - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x2                          = xy;
        prim->x0                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sx + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x3                          = xy;
        prim->x1                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sy - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y1                          = xy;
        prim->y0                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sy + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y3                          = xy;
        prim->y2                          = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Effect task drawing a glow at its model's position: over `spawnArg1` frames
/// it grows two starbursts and a ring, flashes the screen when that ends, then
/// shrinks a two-ring billboard until it fades out and releases its work.
void func_neo_ark_shrine_8017FEA0(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         rgb[3];

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_neo_ark_shrine_80180570(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_shrine_80180570(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_shrine_80180144(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_neo_ark_shrine_80181474(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim.
void func_neo_ark_shrine_80180144(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = (u16)arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / (otz + 1)`. The RGB triple in `rgb` lights only the
/// inner vertex so each wedge fades to black.
void func_neo_ark_shrine_80180570(GpCoord* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = (u16)arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Effect task drawing a beam trail: state 0 allocates sixteen coordinates and
/// fills both eight-slot trails from the two start positions; state 1 each
/// frame records the current ends into the next slot and draws the trail, and
/// releases the effect after `spawnArg1` frames.
void func_neo_ark_shrine_80180904(Task* task)
{
    GpCoord    coord;
    GpCoord*   coords;
    GpCoord*   objCoord;
    GpCoord*   dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = (GpCoord*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GpCoord*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_neo_ark_shrine_80182714[0].vx;
                objCoord->coord.t[1] = D_neo_ark_shrine_80182714[0].vy;
                objCoord->coord.t[2] = D_neo_ark_shrine_80182714[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_neo_ark_shrine_80182714[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_neo_ark_shrine_8018271C.vx;
                coord.coord.t[1] = D_neo_ark_shrine_8018271C.vy;
                coord.coord.t[2] = D_neo_ark_shrine_8018271C.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_neo_ark_shrine_80180DF4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the beam
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade. Dropped when `gte_stflg` is negative.
void func_neo_ark_shrine_80180DF4(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GpCoord*           a;
    GpCoord*           b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                = SCRATCH_HEAD(u8) - sizeof(RoomDraw03Scratch);
        blk                = (RoomDraw03Scratch*)tmp;
        SCRATCH_HEAD(void) = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = (u16)a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = (u16)a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = (u16)a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        blk->v[1].vz = (u16)b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        blk->v[2].vz = (u16)a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// Effect task of a burst at its model's position: spawns its particle effects
/// on the first frame, then either sprays sparks in random directions or grows
/// two fading rings for seven frames, and releases its work.
void func_neo_ark_shrine_801811EC(Task* task)
{
    GpCoord*   objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_neo_ark_shrine_80180144(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_shrine_80180144(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a two-ring billboard. `arg1` is a signed half-extent; on-screen radii
/// are `(s16)arg1 * 64 / (otz + 1)` (outer) and `(s16)arg1 * 8 / (otz + 1)`
/// (inner). The RGB triple tints the inner vertex of the inner ring at full
/// brightness and the outer ring at half, so each wedge fades to a black rim.
void func_neo_ark_shrine_80181474(GpCoord* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = (u16)arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
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

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
    SCRATCH_POP_BYTES(0x1C);
}
