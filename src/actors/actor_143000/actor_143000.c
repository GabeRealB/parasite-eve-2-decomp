#include "common.h"

#include <psyq/memory.h>
#include <psyq/rand.h>

#include "actors/actor_143000.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/3A34.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/strings.h"
#include "rooms/room_common.h"

/// Work block of the actor's callback task. `promptKind` is the picked hotspot's
/// prompt display mode, copied from its `Actor143000Rect::field_A` by
/// `func_actor_143000_801325F0` and handed to `func_800D4E78` when
/// `func_actor_143000_80133698` re-spawns the prompt.
typedef struct Actor143000Work {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ u16  field_2;
    /* 0x04 */ s16  field_4;
    /* 0x06 */ s8   promptKind;
    /* 0x07 */ s8   field_7;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s8   field_12;
    /* 0x13 */ s8   field_13;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s16  field_16;
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
} Actor143000Work;
STATIC_ASSERT_SIZEOF(Actor143000Work, 0x1C);

/// One hotspot of the `D_actor_143000_80134580` list: a screen rect
/// `func_actor_143000_80133AE8` hit-tests the prompt cursor against. The list
/// ends on an entry whose `field_8` is -1.
typedef struct Actor143000Rect {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 w;
    /* 0x6 */ s16 h;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s8  field_A;
    /// Set while the prompt cursor is inside the rect; cleared on every entry
    /// when `func_actor_143000_801324C8` starts the actor.
    /* 0xB */ s8 field_B;
} Actor143000Rect;
STATIC_ASSERT_SIZEOF(Actor143000Rect, 0xC);

/// Spawn argument of `func_actor_143000_80133CF0`, the task that captures
/// successive horizontal image strips.
typedef struct Actor143000CaptureArgs {
    /* 0x0 */ u16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ u16 w;
    /* 0x6 */ s16 h;
    /* 0x8 */ s32 total;
    /* 0xC */ s32 count;
} Actor143000CaptureArgs;
STATIC_ASSERT_SIZEOF(Actor143000CaptureArgs, 0x10);

extern s8              D_8007218B;
extern s16             D_80114D08;
extern TaskDesc        D_actor_143000_80134558;
extern u8              D_actor_143000_80134570[];
extern Actor143000Rect D_actor_143000_80134580[];
extern char*           D_actor_143000_801345F8[];
extern TaskDesc        D_actor_143000_801350B0;
extern s32             D_actor_143000_80135C00;
extern s32             D_actor_143000_80135C04;
extern u8              D_actor_143000_80135C0C;
extern char            D_actor_143000_80135C20[];

void func_actor_143000_801323E0(s32 x, s32 y, s32 variant);
void func_actor_143000_80132A04(Task* arg0);
void func_actor_143000_80133664(Task* task);
void func_actor_143000_80133698(Task* task);
void func_actor_143000_801336E8(Task* arg0);
void func_actor_143000_80133800(Task* arg0);
void func_actor_143000_801338C8(Task* arg0);
void func_actor_143000_801338E0(Task* arg0);
void func_actor_143000_801339CC(Task* arg0);
void func_actor_143000_80133AC0(Task* arg0);
s32  func_actor_143000_80133AE8(Actor143000Rect* p, s16 x, s16 y);
void func_actor_143000_80133C2C(void);
void func_actor_143000_80133C90(Task* task);

/// Per-frame cursor driver of the action prompt, run as state 1 of the prompt
/// task that `func_actor_143000_80133578` dispatches.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it for a dead-zone
/// curve) and then the d-pad -- whose four bits select one of eight
/// 1/16-of-a-turn headings fed to `rsin`/`rcos` -- into the prompt's
/// 1/512-pixel position, clamps that to the screen, classifies the confirm
/// (0x40) and cancel (0xA0) buttons into the prompt's two button slots, and
/// finally hands the rounded position to `func_actor_143000_801323E0`
/// to draw the cursor. `RoomActionPrompt::targetId` doubles as the cursor speed
/// here and `field_E` as the double-press window: a second press inside that
/// many frames without the cursor having moved reports state 4 instead of 2.
///
/// `step` carries the analog delta first and the d-pad heading afterwards, and
/// `idx` indexes the button slots in `u16` units so that `i` survives as the
/// loop counter.
void func_actor_143000_80131F80(Task* task)
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
        func_actor_143000_801323E0(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues one 16x24 textured quad -- the action prompt cursor icon -- at
/// (`x`, `y`) into the head of the current OT. `variant` is the prompt's
/// `mode`: 0 draws nothing, 2 (a hotspot under the cursor) uses palette
/// 0x3C87 and anything else 0x3C88.
void func_actor_143000_801323E0(s32 x, s32 y, s32 variant)
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

void func_actor_143000_801324C8(Task* arg0)
{
    Actor143000Work* work;
    Actor143000Rect* p;
    u8               temp_a0;

    p    = D_actor_143000_80134580;
    work = memCalloc(0x1CU, false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->spawnArg2          = Task_SpawnFromTable(&D_actor_143000_80134558, 0, 1, 0);
    arg0->work               = (TaskIdMap*)work;
    temp_a0                  = Mc_SaveData.at4.loc.view;
    Mc_SaveData.at4.loc.view = 0xB;
    D_actor_143000_80135C0C  = temp_a0;
    arg0->state             += 1;
    work->field_4            = 0;
    Display_AcquireRef();
    if (p->field_8 != -1) {
        do {
            p->field_B = 0;
            p++;
        } while (p->field_8 != -1);
    }
    work->field_7              = 0;
    work->field_12             = 1;
    work->field_13             = 0;
    work->field_16             = 0xA00;
    work->field_C              = 0;
    work->field_10             = 0;
    work->field_18             = 0x10;
    work->field_1A             = 0;
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    Gp_MsgPlayerWeapon(0);
    Gp_MsgPlayer3F3(0);
}

void func_actor_143000_801325F0(Task* arg0)
{
    Actor143000Work*  work;
    u8                u;
    Actor143000Rect*  p;
    POLY_FT4*         prim;
    RoomActionPrompt* prompt;
    s16               dx;
    s16               dy;
    s16               x;
    s16               y;
    s16               w;
    s16               h;
    u8                v;
    u8                uw;
    u8                vh;

    work                     = arg0->work;
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    p                        = D_actor_143000_80134580;
    Gp_StateF0.field_4       = 2;
    prompt                   = &D_80114D28;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (D_8007218B == 9) {
        func_actor_143000_80133C2C();
    }
    work->field_2 = 0;
    if (func_actor_143000_80133AE8(p, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; p->field_8 != -1; p++) {
                if (p->field_B != 0) {
                    if (work->field_7 != 0 && p->field_8 == 5) {
                        SndEvt_EnqueueType6(0x541F0013, 0, 0);
                        prompt->mode     = 0;
                        prompt->targetId = 0;
                        work->field_8    = prompt->screen.xy.x;
                        work->field_A    = prompt->screen.xy.y;
                        arg0->state      = 8;
                        return;
                    }
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->field_2    = p->field_8;
                    work->promptKind = p->field_A;
                    arg0->state      = 3;
                    return;
                }
            }
        }
        for (p = D_actor_143000_80134580; p->field_8 != -1; p++) {
            if (p->field_B != 0) {
                if (p->field_8 != 3) {
                    if (p->field_8 == 5) {
                        prim           = (POLY_FT4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        SetPolyFT4(prim);
                        setShadeTex(prim, 1);
                        x  = (s16)(prompt->screen.xy.x - p->x) / 16 * 16;
                        y  = (s16)(prompt->screen.xy.y - p->y) / 16 * 16;
                        dx = p->x;
                        dy = p->y;
                        u  = x;
                        v  = y + 0x70;
                        x += dx;
                        y += dy;
                        setXYWH(prim, x, y, 16, 16);
                        setUVWH(prim, u, v, 16, 16);
                        prim->tpage = 0x16;
                        prim->clut  = 0x3DC1;
                        addPrim(&gGpuCurrentOt[0x3FE], prim);
                    }
                } else {
                    prim           = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    SetPolyFT4(prim);
                    setShadeTex(prim, 1);
                    u  = 0x30;
                    x  = p->x;
                    w  = p->w;
                    y  = p->y;
                    h  = p->h;
                    uw = p->w;
                    vh = p->h;
                    setXY4(prim, x, y, x + w, y, x, y + h, x + w, y + h);
                    setUV4(prim, u, 0xB8, uw + 0x30, 0xB8, u, vh - 0x48, uw + 0x30, vh - 0x48);
                    prim->tpage = 0x16;
                    prim->clut  = 0x3DC1;
                    addPrim(&gGpuCurrentOt[0x3FE], prim);
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        arg0->state = 5;
    }
}

/// The three rows of the code keypad, bottom row first;
/// `D_actor_143000_801345F8` lists them top row first.
const char D_actor_143000_80131E54[] = "NOPQRSTUVWXYZ";
const char D_actor_143000_80131E64[] = "ABCDEFGHIJKLM";
const char D_actor_143000_80131E74[] = "0123456789-# ";

/// State table of the actor's callback, `func_actor_143000_801335C8`, which
/// copies it onto its stack and indexes it with `Task::state`.
const TaskFuncTable11 D_actor_143000_80131E84 = { {
    func_actor_143000_801324C8,
    func_actor_143000_80133664,
    func_actor_143000_801325F0,
    func_actor_143000_80133698,
    func_actor_143000_801336E8,
    func_actor_143000_80133800,
    func_actor_143000_801338C8,
    func_actor_143000_80132A04,
    func_actor_143000_801338E0,
    func_actor_143000_801339CC,
    func_actor_143000_80133AC0,
} };

/// The codes `func_actor_143000_80132A04` accepts; the second only while
/// `D_8007218B` is non-zero.
const char D_actor_143000_80131EB0[] = "A3EILM2S2Y";
const char D_actor_143000_80131EBC[] = "YSD";

void func_actor_143000_80132A04(Task* arg0)
{
    s32              var_s2;
    Actor143000Work* temp_s0;

    COMPILER_BARRIER();
    temp_s0 = arg0->work;
    var_s2  = 0;
    if (arg0->killCountdown == 0) {
        if ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EB0) == 0) || ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EBC) == 0) && (D_8007218B != 0))) {
            var_s2 = 1;
        }
        temp_s0->field_C = var_s2;
    }
    if (temp_s0->field_C != 0) {
        switch (arg0->killCountdown) {
            case 0:
                temp_s0->field_12 = 2;
                break;
            case 0x3C:
                temp_s0->field_12 = 3;
                temp_s0->field_14 = 0x7C;
                break;
            case 0x46:
                temp_s0->field_14 = 0x83;
                break;
            case 0x50:
                temp_s0->field_14 = 0x8A;
                break;
            case 0x5A:
                temp_s0->field_14 = 0;
                break;
            case 0x78:
                SndEvt_EnqueueType6(0x541F0011, 0, 0);
                temp_s0->field_13 = 1;
                break;
            case 0x96:
                temp_s0->field_12 = 4;
                temp_s0->field_13 = 0;
                break;
            case 0xF0:
                temp_s0->field_12 = 5;
                break;
            case 0x14A:
                arg0->state                     = 0xA;
                D_actor_143000_80135C08.field_0 = 0;
                D_actor_143000_80135C08.field_1 = 0;
                D_actor_143000_80135C08.field_2 = 0xF;
                arg0->killCountdown             = 0xF;
                Task_Spawn(1, 0x31, 0, (s32)&D_actor_143000_80135C08);
                break;
        }
    } else {
        switch (arg0->killCountdown) {
            case 0:
                temp_s0->field_12 = 2;
                break;
            case 0x3C:
                temp_s0->field_12 = 3;
                temp_s0->field_14 = 0x7C;
                break;
            case 0x46:
                temp_s0->field_14 = 0x83;
                break;
            case 0x50:
                temp_s0->field_14 = 0x8A;
                break;
            case 0x5A:
                temp_s0->field_14 = 0;
                break;
            case 0x78:
                SndEvt_EnqueueType6(0x541F0012, 0, 0);
                temp_s0->field_13 = 2;
                break;
            case 0x96:
                temp_s0->field_12 = 6;
                temp_s0->field_14 = 0x7C;
                temp_s0->field_13 = 0;
                break;
            case 0xA0:
                temp_s0->field_14 = 0x83;
                break;
            case 0xAA:
                temp_s0->field_14 = 0x8A;
                break;
            case 0xB4:
                temp_s0->field_14 = 0;
                break;
            case 0xD2:
                temp_s0->field_13 = 2;
                break;
            case 0xF0:
                temp_s0->field_12 = 7;
                temp_s0->field_13 = 0;
                break;
            case 0x14A:
                temp_s0->field_12 = 1;
                temp_s0->field_10 = 0;
                arg0->state       = 2;
                break;
        }
    }
    arg0->killCountdown = (s16)((u16)arg0->killCountdown + 1);
}

void func_actor_143000_80132D10(Task* arg0)
{
    Actor143000Work* work;
    POLY_FT4*        prim;
    s32              i;
    s16              y;
    s16              x1;
    s16              sx;
    u8               sv;
    s16              sy;
    s16              y1;
    u8               u;
    u8               v;
    u8               v1;
    u8               u1;
    s16              clut;

    x1                                      = -0x48;
    work                                    = arg0->work;
    D_actor_143000_80135C20[work->field_10] = 0;
    D_actor_143000_80135C00++;
    y = 0x10;
    for (i = 0; i < work->field_10; i++) {
        y1             = y + 8;
        u              = 0x58;
        v              = 0xB8;
        u1             = u + 8;
        v1             = v + 8;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        SetPolyFT4(prim);
        setXY4(prim, x1, y, x1 + 8, y, x1, y1, x1 + 8, y1);
        setUV4(prim, u, v, u1, v, u, v1, u1, v1);
        prim->tpage = 0x16;
        prim->clut  = 0x3DC5;
        setShadeTex(prim, 1);
        addPrim(&gGpuCurrentOt[0x3FE], prim);
        x1 += 8;
    }
    if (work->field_10 != 0x14 && arg0->state != 7) {
        u              = 0x60;
        v              = 0xB8;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        SetPolyFT4(prim);
        setXYWH(prim, x1, y, 8, 8);
        setUVWH(prim, u, v, 8, 8);
        prim->tpage = 0x16;
        prim->clut  = 0x3DC6;
        setShadeTex(prim, 1);
        if (D_actor_143000_80135C00 & 0x10) {
            addPrim(&gGpuCurrentOt[0x3FE], prim);
        }
    }
    if (work->field_12 != 0) {
        y1 = 0xFE;
        if (work->field_14 != 0) {
            y1 = work->field_14;
        }
        sx             = -0x78;
        sy             = -0x48;
        sv             = (work->field_12 - 1) * 0x10;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        SetPolyFT4(prim);
        setXYWH(prim, sx, sy, y1, 0x10);
        setUVWH(prim, 0, sv, y1, 0x10);
        prim->tpage = 0x16;
        prim->clut  = 0x3DC0;
        setShadeTex(prim, 1);
        addPrim(&gGpuCurrentOt[0x3FE], prim);
    }
    if (work->field_13 != 0) {
        sy = sx = -0x28;
        x1      = 0x18;
        y1      = -0x10;
        if (work->field_13 == 1) {
            u    = 0x70;
            v    = 0xA0;
            clut = 0x3DC3;
        } else {
            u    = 0x30;
            v    = 0xA0;
            clut = 0x3DC4;
        }
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        SetPolyFT4(prim);
        setXY4(prim, sx, sy, x1, sy, sx, y1, x1, y1);
        setUVWH(prim, u, v, 0x40, 0x18);
        prim->tpage = 0x16;
        prim->clut  = clut;
        setShadeTex(prim, 1);
        addPrim(&gGpuCurrentOt[0x3FE], prim);
    }
    sy             = -0x60;
    sx             = work->field_16 >> 4;
    y1             = sy + 0x18;
    x1             = sx + 0x30;
    sv             = D_actor_143000_80134570[(D_actor_143000_80135C04 / 16) % 16] * 0x18 - 0x60;
    v1             = sv + 0x18;
    clut           = 0x3DC7;
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    SetPolyFT4(prim);
    setXY4(prim, sx, sy, x1, sy, sx, y1, x1, y1);
    setUV4(prim, 0, sv, 0x30, sv, 0, v1, 0x30, v1);
    prim->tpage = 0x16;
    prim->clut  = clut;
    setShadeTex(prim, 1);
    addPrim(&gGpuCurrentOt[0x3FE], prim);
    D_actor_143000_80135C04 += work->field_18;
    if (arg0->state != 7 && arg0->state != 0xA) {
        work->field_16 -= work->field_18;
        if (work->field_16 < -0xD00) {
            work->field_16 = 0xA00;
        }
    }
    work->field_18 -= 4;
    if (work->field_18 < 0x10) {
        work->field_18 = 0x10;
    }
}

/// Outlines the hotspot rect `rect` in (`r`, `g`, `b`) with four flat
/// `LINE_F2` edges linked into `gGpuCurrentOt[1]`. Only reached while
/// `Mc_SaveData.demoScene` or `D_8007218B` is 9, to show the hotspot rects.
void func_actor_143000_80133334(Actor143000Rect* rect, u8 r, u8 g, u8 b)
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

/// Callback of the action-prompt task that `func_actor_143000_801324C8` spawns
/// from `D_actor_143000_80134558`: a two-state dispatcher whose handler table
/// is built on the stack. State 0, `func_actor_143000_80133C90`, resets both
/// prompt slots; state 1, `func_actor_143000_80131F80`, drives the cursor every
/// frame from then on.
void func_actor_143000_80133578(Task* task)
{
    TaskFunc funcs[2] = {
        func_actor_143000_80133C90,
        func_actor_143000_80131F80,
    };

    funcs[task->state](task);
}

void func_actor_143000_801335C8(Task* arg0)
{
    TaskFuncTable11 fns;

    fns = D_actor_143000_80131E84;
    fns.funcs[arg0->state](arg0);
    func_actor_143000_80132D10(arg0);
}

/// State 1 of the actor's callback: arms the first action-prompt slot with
/// target id 0x80, marks it highlighted (`mode` 1), clears its screen position
/// and steps the task on to state 2.
void func_actor_143000_80133664(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// State 3 of the actor's callback, entered once a hotspot is picked: clears
/// the prompt's highlight and target, re-spawns the prompt at its current
/// screen position with the picked hotspot's `promptKind`, and moves the task
/// to state 4.
void func_actor_143000_80133698(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    Actor143000Work*  work   = (Actor143000Work*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

void func_actor_143000_801336E8(Task* arg0)
{
    Actor143000Work*  work   = arg0->work;
    RoomActionPrompt* prompt = &D_80114D28;
    s32               cmd;

    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        switch ((s16)(work->field_2 - 1)) {
            case 0:
                cmd = 8;
                goto run;
            case 1:
                cmd = 7;
                goto run;
            case 3:
                cmd = 9;
            run:
                Gp_RunCapCmd(cmd, 0);
                arg0->state = 2;
                break;
            case 2:
                SndEvt_EnqueueType6(0x541F0010, 0, 0);
                arg0->state         = 7;
                arg0->killCountdown = 0;
                break;
            case 4:
                work->field_7 = 1;
                Gp_RunCapCmd(0xA, 0);
                if (GameFlag_GetNibble(0xD0) == 1) {
                    arg0->killCountdown = 0xA;
                    arg0->state         = 9;
                } else {
                    arg0->state = 2;
                }
                break;
            default:
                arg0->state = 2;
                break;
        }
    } else if (work->field_4 != 0) {
        arg0->state = 6;
    } else {
        arg0->state = 2;
    }
}

void func_actor_143000_80133800(Task* arg0)
{
    Actor143000Work* work = (Actor143000Work*)arg0->work;

    Display_ReleaseRef();
    gGameSession->cutsceneHold = 0;
    if (work->field_C == 0) {
        D_80114D08               = 0xA;
        gGameSession->eventState = 0;
        gGameSession->hideHud    = 0;
        Gp_StateF0.field_4       = 0;
        Mc_SaveData.at4.loc.view = D_actor_143000_80135C0C;
        Gp_MsgPlayer3F3(1);
    } else {
        Task_SpawnFromTable(&D_actor_143000_801350B0, 1, 0, (s32)&D_actor_143000_80135C08);
    }
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, work->field_C);
}

void func_actor_143000_801338C8(Task* arg0)
{
    ((Actor143000Work*)arg0->work)->field_4 = 0;
    arg0->state                             = 2;
}

void func_actor_143000_801338E0(Task* arg0)
{
    Actor143000Work* work = arg0->work;
    s32              col  = (work->field_8 + 0x80) / 16;
    s32              row  = (work->field_A - 0x20) / 16;
    char*            key;

    if ((u32)col < 13) {
        if (row >= 0) {
            if (row < 3) {
                key = D_actor_143000_801345F8[row] + col;
                if ((s8)*key == '#') {
                    work->field_10 = 0;
                } else if ((s8)*key == '-') {
                    if (work->field_10 > 0) {
                        work->field_10--;
                    }
                } else if (work->field_10 < 20) {
                    D_actor_143000_80135C20[work->field_10] = *key;
                    work->field_10++;
                }
                work->field_18 = 0x30;
            }
        }
    }
    arg0->state = 2;
}

void func_actor_143000_801339CC(Task* arg0)
{
    Actor143000Work* work = arg0->work;
    u32              count;

    if (Gp_CapBusy() == 0) {
        count               = (u16)arg0->killCountdown - 1;
        arg0->killCountdown = count;
        if ((s16)count <= 0) {
            arg0->killCountdown = (rand() * 8 >> 15) + 8;
            work->field_10++;
            SndEvt_EnqueueType6(0x541F0013, 0, 0);
            memcpy(D_actor_143000_80135C20, D_actor_143000_80131EB0, 11);
            count = work->field_10;
            if (count >= 0xA) {
                arg0->state = 2;
            }
        }
    }
}

void func_actor_143000_80133AC0(Task* arg0)
{
    u16 count = (u16)arg0->killCountdown - 1;

    arg0->killCountdown = count;
    if ((s16)count <= 0) {
        arg0->state = 5;
    }
}

s32 func_actor_143000_80133AE8(Actor143000Rect* p, s16 x, s16 y)
{
    s32 result = 0;

    if (p->field_8 != -1) {
        do {
            if (x >= p->x && x < p->x + p->w && y >= p->y && y < p->y + p->h) {
                if (Mc_SaveData.demoScene == 9) {
                    func_actor_143000_80133334(p, 0, 0, 0);
                }
                p->field_B = 1;
                if (result == 0) {
                    result = p->field_8;
                }
            } else {
                if (Mc_SaveData.demoScene == 9) {
                    func_actor_143000_80133334(p, 0xFF, 0, 0);
                }
                p->field_B = 0;
            }
            p++;
        } while (p->field_8 != -1);
    }
    return result;
}

void func_actor_143000_80133C2C(void)
{
    Actor143000Rect* p = D_actor_143000_80134580;

    if (p->field_8 != -1) {
        do {
            func_actor_143000_80133334(p, 0, 0xFF, 0);
            p++;
        } while (p->field_8 != -1);
    }
}

/// State 0 of the action-prompt task: resets both prompt slots before the
/// first cursor frame -- clears the position accumulators and the two
/// buttons' held-frame counters, parks the target id (the cursor speed) at
/// 0x100 and `field_E` (the double-press window) at 0xF, marks the slot
/// highlighted -- and steps the task on one state.
void func_actor_143000_80133C90(Task* task)
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

void func_actor_143000_80133CF0(Task* arg0)
{
    Actor143000CaptureArgs* p = arg0->spawnArg2;
    RECT                    r;
    RECT                    r2;
    s32                     n;
    s32                     offset;
    RECT*                   rp;
    s32                     bottom;
    Task*                   killTask = arg0;

    SOFT_TOUCH_REG(killTask);
    if (gGameSession->at4.loc.view == 0xE) {
        switch (arg0->state) {
            case 0:
                arg0->killCountdown = 6;
                p->count            = 0;
                arg0->state++;
                return;
            case 1:
                if (--arg0->killCountdown > 0) {
                    return;
                }
                arg0->killCountdown = 6;
                r.x                 = p->x;
                r.w                 = p->w;
                r.y                 = p->y + p->h * p->count / p->total;
                n                   = p->count + 1;
                rp                  = &r2;
                SOFT_TOUCH_REG_USE(rp, n);
                p->count = n;
                bottom   = p->y + p->h * n / p->total;
                offset   = r.y * 640;
                r.h      = bottom - r.y;
                SOFT_USE_REG(offset);
                r2    = r;
                r2.x  = 0x1C0;
                rp->w = 0x140;
                r2.y += 0x100;
                StoreImage(rp, (u32*)((u8*)Fs_ImgBuffers + offset));
                killTask = arg0;
                if (p->count >= p->total) {
                    goto kill;
                }
                break;
        }
    } else {
        goto kill;
    }
    return;
kill:
    taskKill(killTask);
}
