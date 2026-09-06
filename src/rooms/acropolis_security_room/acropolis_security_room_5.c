#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/acropolis_security_room.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix with no translation
/// vector added. Same reason as above for spelling out the word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
/// `mvmva 1, 0, 0, 0, 0`: rotate V0 by the rotation matrix and add the
/// translation vector. Same reason as above for spelling out the word.
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")
/// `gpf 1`: scale IR1..3 by IR0. Same reason as above for spelling out the word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
void func_acropolis_security_room_8017FD64(s32 flags);
void func_acropolis_security_room_8017F480(Task* task);
void func_acropolis_security_room_80180308(Task* task);
/// The two `TaskDesc`s this room's script spawns from: index 0 is
/// `func_acropolis_security_room_80180368`, index 1 is
/// `func_acropolis_security_room_801804CC`.
extern TaskDesc D_acropolis_security_room_80182700[];
/// The single-entry `TaskDesc` table the script spawns its child task from:
/// `func_acropolis_security_room_8017F9C8`.
extern TaskDesc D_acropolis_security_room_801826C0[];
/// The script's message table, parked in `Task::field_24`.
extern GpMsgEntry D_acropolis_security_room_801826CC[];
extern s8         D_8007216C;
extern s16        D_80114D08;
/// 0xFF-terminated area-record lists applied as the script ends.
extern GpAreaApplyRec D_acropolis_security_room_80184F50[];
extern GpAreaApplyRec D_acropolis_security_room_80184F78[];
extern GpAreaApplyRec D_acropolis_security_room_80184F7C[];
extern GpAreaApplyRec D_acropolis_security_room_80184F80[];
/// Scratch state of the security-room ambience task, stored at `Task::idMap`.
/// `func_acropolis_security_room_80180368` allocates it with `Mem_Calloc(4, 0)`,
/// so the size below is the allocation and not a guess.
typedef struct {
    /* 0x0 */ u16  fadeStarted; // the looping ambience has already been faded out
    /* 0x2 */ byte pad_2[0x2];
} AsrAmbienceState;
/// The 0x100-entry RGB555 palette every monitor-screen CLUT is blended
/// towards: the "off" colours of the four security-camera feeds.
extern u16 D_acropolis_security_room_80182718[];
/// The four lit palettes, one per camera feed, blended against
/// `D_acropolis_security_room_80182718` by the feed's own brightness.
extern u16 D_acropolis_security_room_80182918[];
extern u16 D_acropolis_security_room_80182B18[];
extern u16 D_acropolis_security_room_80182D18[];
extern u16 D_acropolis_security_room_80182F18[];
/// The four blend results, uploaded to VRAM by
/// `D_acropolis_security_room_80183918`.
extern u16 D_acropolis_security_room_80183118[];
extern u16 D_acropolis_security_room_80183318[];
extern u16 D_acropolis_security_room_80183518[];
extern u16 D_acropolis_security_room_80183718[];
/// The upload records for the four blended CLUTs above.
extern GpImgRec D_acropolis_security_room_80183918[];
/// Camera-lit bitmask for each value of `GameFlag_GetNibble(9)`; bit N is set
/// while feed N is showing something.
extern u16 D_acropolis_security_room_80183968[];
/// Spawn position and effect id of the flash each newly lit feed plays,
/// indexed by feed.
extern SVECTOR D_acropolis_security_room_80183998[];
extern s16     D_acropolis_security_room_801839B8[];

void func_acropolis_security_room_8017F1BC(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(9);
    if ((flag == 0) || (flag == 2)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(3, 1, 0);
        } else if (step == 1) {
            Gp_ClearCollectedBit(0x104);
            SndEvt_EnqueueType6(0x51060001, 0, 0);
            GameFlag_SetNibble(9, GameFlag_GetNibble(9) | 1);
            GameFlag_SetNibble(1, 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
            st->field_0 = 0;
            task->state = 6;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F80);
            Task_Kill((Task*)task->spawnArg2);
            return;
        } else {
            Gp_StartCapSlot(3, 1, 2);
        }
    } else if ((flag == 1) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(3, 1, 1);
        } else {
            Gp_StartCapSlot(3, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

void func_acropolis_security_room_8017F300(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(9);
    if ((flag == 0) || (flag == 1)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(4, 1, 0);
        } else if (step == 2) {
            Gp_ClearCollectedBit(0x103);
            SndEvt_EnqueueType6(0x51060001, 0, 0);
            GameFlag_SetNibble(9, GameFlag_GetNibble(9) | 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
            st->field_0           = 0;
            task->state           = 0xA;
            Game_Session->field_1 = 1;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F50);
            if (GameFlag_GetNibble(3) < 3) {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F78);
            } else {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F7C);
            }
            Task_Kill((Task*)task->spawnArg2);
            return;
        } else {
            Gp_StartCapSlot(4, 1, 2);
            task->state = 2;
            return;
        }
    } else if ((flag == 2) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(4, 1, 1);
        } else {
            Gp_StartCapSlot(4, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

/// Per-frame cursor driver of the security-room action prompt, run as state 1
/// of `func_acropolis_security_room_8017F9C8`.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it for a dead-zone
/// curve) and then the d-pad -- whose four bits select one of eight
/// 1/16-of-a-turn headings fed to `rsin`/`rcos` -- into the prompt's
/// 1/512-pixel position, clamps that to the screen, classifies the confirm
/// (0x40) and cancel (0xA0) buttons into the prompt's two button slots, and
/// finally hands the rounded position to `func_acropolis_security_room_8017F8E0`
/// to draw the cursor. `RoomActionPrompt::targetId` doubles as the cursor speed
/// here and `field_E` as the double-press window: a second press inside that
/// many frames without the cursor having moved reports state 4 instead of 2.
///
/// `step` carries the analog delta first and the d-pad heading afterwards, and
/// `idx` indexes the button slots in `u16` units so that `i` survives as the
/// loop counter.
void func_acropolis_security_room_8017F480(Task* task)
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
        func_acropolis_security_room_8017F8E0(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues the security-room's 16x24 cursor/highlight quad at (`x`, `y`) into
/// the current OT. `variant` picks the palette -- 0x3C87 when it is 2, and
/// 0x3C88 otherwise -- and 0 draws nothing at all.
void func_acropolis_security_room_8017F8E0(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);

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

    addPrim(Gpu_CurrentOt, prim);
}

/// Task callback of the descriptor at `D_acropolis_security_room_801826C0`:
/// a two-state dispatcher whose handler table is built on the stack rather
/// than read from `.data`, so state 0 runs
/// `func_acropolis_security_room_80180308` and state 1 runs
/// `func_acropolis_security_room_8017F480`.
void func_acropolis_security_room_8017F9C8(Task* task)
{
    TaskFunc funcs[2] = {
        func_acropolis_security_room_80180308,
        func_acropolis_security_room_8017F480,
    };

    funcs[task->state](task);
}

/// State 0 of the security-room cap script: allocates the 0x10 state block into
/// `Task::idMap`, spawns the script's child task, publishes the message table
/// and the current pair-flag nibble, takes a display reference and clears every
/// hotspot's `hit` flag before the first cursor scan. A failed allocation kills
/// the task instead.
void func_acropolis_security_room_8017FA18(Task* task)
{
    AcropolisSecurityRoomState* st;
    AsrHotspot*                 hs;

    st = Mem_Calloc(sizeof(AcropolisSecurityRoomState), 0);
    if (st == NULL) {
        Task_Kill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(D_acropolis_security_room_801826C0, 0, 1, 0);
    task->field_24  = D_acropolis_security_room_801826CC;
    task->idMap     = (TaskIdMap*)st;
    D_8007216C      = 6;
    SOFT_BARRIER();
    task->state++;
    st->field_0 = 0;
    st->frames  = 0;
    func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
    Game_Session->field_66 = 1;
    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    Display_AcquireRef();
    for (hs = D_acropolis_security_room_801826DC; hs->id != -1; hs++) {
        hs->hit = 0;
    }
}

/// Arms the action prompt for the script's hotspot and steps the caller on one
/// state: highlights (`mode` 1) the fixed target id 0x80 and clears the prompt's
/// on-screen position, which `func_800D4E78` fills in again when the prompt is
/// actually spawned. Same body as the shared `Room_Util14`, which this overlay
/// cannot link because it carries a second copy at `func_acropolis_security_room_8017EA28`.
void func_acropolis_security_room_8017FB20(Task* task)
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
void func_acropolis_security_room_8017FB54(Task* task)
{
    RoomActionPrompt*           prompt = &D_80114D28;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->idMap;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, st->promptKind);
    task->state = 4;
}

/// Runs one step of whichever cap script this task family is driving, once the
/// gameplay side reports the action prompt has been dismissed: clears the
/// prompt, then hands the task to `func_acropolis_security_room_8017F1BC` or
/// `func_acropolis_security_room_8017F300` and resets the sub-step. While
/// `func_800D4EC0` still reports a prompt up and no sub-step is pending, the
/// task instead parks on state 2. `variant` is never written in this overlay --
/// the state block is calloc'd -- so the `func_acropolis_security_room_8017F300`
/// arm is the one this room actually takes.
void func_acropolis_security_room_8017FBA4(Task* task)
{
    RoomActionPrompt*           prompt = &D_80114D28;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->idMap;

    prompt->mode     = 0;
    prompt->targetId = 0;
    if ((func_800D4EC0() != 0) || (st->field_0 != 0)) {
        if (st->variant == 1) {
            func_acropolis_security_room_8017F1BC(task);
            st->field_0 = 0;
            return;
        }
        func_acropolis_security_room_8017F300(task);
        st->field_0 = 0;
        return;
    }
    task->state = 2;
}

void func_acropolis_security_room_8017FC30(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    D_8007216C             = 3;
    Display_ReleaseRef();
    Task_Kill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

/// Hit-tests the action cursor at (`x`, `y`) against the 0xFFFF-terminated
/// hotspot table `table`, raising `hit` on every entry whose rectangle
/// contains the point and clearing it on every other one. Returns non-zero if
/// any entry was hit, so the caller can tell "cursor is over something" from
/// "cursor is over nothing" without rescanning the table.
s32 func_acropolis_security_room_8017FCB0(AsrHotspot* table, s16 x, s16 y)
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

/// Repaints the two security-monitor sprites for the current state of game
/// flag nibble 9, whose low two bits say which of the two shutters has been
/// opened. The nibble selects, for each of the two sprite commands of view 6
/// in this room's sprite record, whether `Gp_LinkViewSprts` skips linking it
/// (`field_4` non-zero) or draws it.
void func_acropolis_security_room_8017FD64(s32 flags)
{
    GameSession*      g    = Game_Session;
    GameSessionFrom4* sess = (GameSessionFrom4*)&g->field_4;
    GpSprtCmd*        cmd;

    cmd = Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1][5].field_4;
    switch (flags & 0xFF) {
        case 0:
            cmd[1].field_4 = 1;
            cmd[2].field_4 = 1;
            break;
        case 1:
            cmd[1].field_4 = 0;
            cmd[2].field_4 = 1;
            break;
        case 2:
            cmd[1].field_4 = 1;
            cmd[2].field_4 = 0;
            break;
        case 3:
            cmd[1].field_4 = 0;
            cmd[2].field_4 = 0;
            break;
    }
}

/// `GpMsgEntry` handler for message 0x13F1, the "can this key item be used
/// here?" query `Gp_UseKeyItemRow` sends to slot 7. `item` is the key item the
/// player highlighted; the three ids this room accepts each select a sub-step
/// of the cap script, recorded in the state block's `field_0` for
/// `func_acropolis_security_room_8017FA18` to pick up. Any other item stores 0
/// and answers 0, which is the "cannot use that now" reply.
s32 func_acropolis_security_room_8017FE24(Task* task, s32 msgId, s32 item, s32 arg3)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;

    if (item == 0x101) {
        st->field_0 = 3;
        return 1;
    }
    if (item == 0x103) {
        st->field_0 = 2;
        return 1;
    }
    if (item == 0x104) {
        st->field_0 = 1;
        return 1;
    }
    st->field_0 = 0;
    return 0;
}

/// Fades the screen to white over 0x40 frames, then steps the caller on one
/// state: `frames` doubles as the fade level here, rising by 4 a frame and
/// driving `Fade_DrawOverlay`'s three colour channels together. At the halfway
/// point (0x80) the door chime is queued; once the level passes 0xFF the
/// counter is reset for the next state and `D_8007216C` is set to 0x10.
void func_acropolis_security_room_8017FE6C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;
    u8                          level;

    level = st->frames;
    Fade_DrawOverlay(level, level, level, 2);
    st->frames = st->frames + 4;
    if (st->frames == 0x80) {
        SndEvt_EnqueueType6(0x51060002, 0, 0);
    }
    if (st->frames >= 0x100) {
        st->frames = 0;
        D_8007216C = 0x10;
        /* Without the barrier GCC hoists the `lw` of `task->state` above the
         * byte store, dropping the load-delay `nop`. */
        SOFT_BARRIER();
        task->state = task->state + 1;
    }
}

void func_acropolis_security_room_8017FF0C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;

    if (st->frames == 1) {
        st->child   = Task_SpawnFromTable(D_acropolis_security_room_80182700, 0, 0, 0);
        task->state = task->state + 1;
    }
    st->frames = st->frames + 1;
}

void func_acropolis_security_room_8017FF84(Task* task)
{
    s32 killArg;

    if (Task_PollKill(((AcropolisSecurityRoomState*)task->idMap)->child, &killArg) != 0) {
        task->state = task->state + 1;
    }
}

void func_acropolis_security_room_8017FFD0(Task* arg0)
{
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayer3F3(0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_acropolis_security_room_80180010(Task* task)
{
    D_8007216C = 3;
    /* Without the barrier GCC hoists the `lw` of `task->state` above the byte
     * store, dropping the load-delay `nop` and making the body one instruction
     * short. */
    SOFT_BARRIER();
    task->state = task->state + 1;
}

void func_acropolis_security_room_80180030(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    Game_Session->field_1  = 0;
    func_800E9BDC(0, 0xF9FF);
    Task_RequestKill(task, 0);
}

void func_acropolis_security_room_801800A4(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;
    s32                         level;
    s16                         frames;

    GameFlag_SetNibble(0x1EE, 0);
    level = (u8)st->frames;
    Fade_DrawOverlay(level, level, level, 2);
    frames     = st->frames + 4;
    st->frames = frames;
    if ((u16)frames == 0x80) {
        SndEvt_EnqueueType6(0x51060002, 0, 0);
    }
    if (st->frames >= 0x100) {
        st->frames = 0;
        D_8007216C = 0xE;
        /* Same load-delay shape as `func_acropolis_security_room_80180010`:
         * without the barrier GCC hoists the `lw` of `task->state` above the
         * byte store and drops the delay `nop`. */
        SOFT_BARRIER();
        task->state = task->state + 1;
    }
}

void func_acropolis_security_room_8018014C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;

    if (st->frames == 1) {
        st->child   = Task_SpawnFromTable(D_acropolis_security_room_80182700, 1, 0, 0);
        task->state = task->state + 1;
    }
    st->frames = st->frames + 1;
}

void func_acropolis_security_room_801801C4(Task* task)
{
    s32 killArg;

    if (Task_PollKill(((AcropolisSecurityRoomState*)task->idMap)->child, &killArg) != 0) {
        Gp_MsgPlayer3F3(1);
        task->state = task->state + 1;
    }
}

void func_acropolis_security_room_80180218(Task* task)
{
    D_80114D08 = 0xA;
    D_8007216C = 3;
    Display_ReleaseRef();
    func_800E9BDC(0, 0xF9FF);
    Task_RequestKill(task, 0);
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    Game_Session->field_1  = 0;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_5", D_acropolis_security_room_8017D6AC);
