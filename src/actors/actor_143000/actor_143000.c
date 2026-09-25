#include "common.h"

#include "actors/actor_143000.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/strings.h"
#include "rooms/room_common.h"
extern TaskDesc D_actor_143000_801350C8;

extern s32              D_80070F6C;
extern s8               D_8007218B;
extern u8               D_801153F4;
extern GpAreaApplyRec   D_80186488;
extern GpAreaApplyRec   D_8018649C;
extern char             D_actor_143000_80131EB0[];
extern char             D_actor_143000_80131EBC[];
extern TaskDesc         D_actor_143000_80134558;
extern u8               D_actor_143000_80134570[];
extern Actor143000Rect  D_actor_143000_80134580[];
extern s32              D_actor_143000_801351B0;
extern s32              D_actor_143000_80135870;
extern s32              D_actor_143000_80135A20;
extern s32              D_actor_143000_80135AE0;
extern s32              D_actor_143000_80135C00;
extern s32              D_actor_143000_80135C04;
extern Actor143000Spawn D_actor_143000_80135C08;
extern u8               D_actor_143000_80135C0C;
extern s32              D_actor_143000_80135C14;
extern s32              D_actor_143000_80135C18;
extern s32              D_actor_143000_80135C1C;
extern char             D_actor_143000_80135C20[];
extern u8               D_actor_143000_80135C38[];
void                    func_actor_143000_801323E0(s32 x, s32 y, s32 variant);

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

void func_actor_143000_801325F0(Actor143000* arg0)
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

    work                     = arg0->field_1C;
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    p                        = D_actor_143000_80134580;
    D_801153F4               = 2;
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
                        arg0->field_30   = 8;
                        return;
                    }
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->field_2    = p->field_8;
                    work->promptKind = p->field_A;
                    arg0->field_30   = 3;
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
        arg0->field_30 = 5;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131E84);

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131EB0);

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_actor_143000_80131EBC\n"
        "D_actor_143000_80131EBC:\n"
        "\t.asciz \"YSD\"\n"
        "\t.align 2\n"
        "\t.asciz \"\"\n"
        "\t.align 2\n"
        ".section .text\n");
#endif

void func_actor_143000_80132A04(Actor143000* arg0)
{
    s32              var_s2;
    Actor143000Work* temp_s0;

    COMPILER_BARRIER();
    temp_s0 = arg0->field_1C;
    var_s2  = 0;
    if (arg0->field_2A == 0) {
        if ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EB0) == 0) || ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EBC) == 0) && (D_8007218B != 0))) {
            var_s2 = 1;
        }
        temp_s0->field_C = var_s2;
    }
    if (temp_s0->field_C != 0) {
        switch (arg0->field_2A) {
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
                arg0->field_30                  = 0xA;
                D_actor_143000_80135C08.field_0 = 0;
                D_actor_143000_80135C08.field_1 = 0;
                D_actor_143000_80135C08.field_2 = 0xF;
                arg0->field_2A                  = 0xF;
                Task_Spawn(1, 0x31, 0, (s32)&D_actor_143000_80135C08);
                break;
        }
    } else {
        switch (arg0->field_2A) {
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
                arg0->field_30    = 2;
                break;
        }
    }
    arg0->field_2A = (s16)((u16)arg0->field_2A + 1);
}

void func_actor_143000_80132D10(Actor143000* arg0)
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
    work                                    = arg0->field_1C;
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
    if (work->field_10 != 0x14 && arg0->field_30 != 7) {
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
    if (arg0->field_30 != 7 && arg0->field_30 != 0xA) {
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
