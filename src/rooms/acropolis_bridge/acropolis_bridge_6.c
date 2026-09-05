#include "common.h"

#include "gameplay/D4.h"
#include "main/display.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Slides one of three mutually exclusive bridge sprites in view 9 by
/// `(dx, dy)` and makes it the visible one. Each state owns three consecutive
/// `GpSprtElem` entries, which move together, and one of the three
/// `GpSprtCmd` slots; `Gp_LinkViewSprts` treats a nonzero `field_4` as "skip
/// OT-linking", so the selected command gets 0 and the other two get 1. A
/// state outside 0..2 moves nothing and hides all three.
void func_acropolis_bridge_8017EB4C(s32 state, s8 dx, s8 dy)
{
    GameSession*      g    = Game_Session;
    GameSessionFrom4* sess = (GameSessionFrom4*)&g->field_4;
    GpSprtRec*        rec;
    GpSprtElem*       el;
    GpSprtCmd*        cmd;
    s32               mode;

    rec  = Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1];
    cmd  = rec[9].field_4;
    el   = rec[9].field_0;
    mode = state & 0xFF;

    if (mode == 0) {
        el[0].x0      += dx;
        el[0].y0      += dy;
        el[1].x0      += dx;
        el[1].y0      += dy;
        el[2].x0      += dx;
        el[2].y0      += dy;
        cmd[1].field_4 = 0;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    } else if (mode == 1) {
        el[3].x0      += dx;
        el[3].y0      += dy;
        el[4].x0      += dx;
        el[4].y0      += dy;
        el[5].x0      += dx;
        el[5].y0      += dy;
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 0;
        cmd[3].field_4 = 1;
    } else if (mode == 2) {
        el[6].x0      += dx;
        el[6].y0      += dy;
        el[7].x0      += dx;
        el[7].y0      += dy;
        el[8].x0      += dx;
        el[8].y0      += dy;
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 0;
    } else {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    }
}

/// Per-frame cursor driver of this room's action prompt, run as state 1 of
/// `func_acropolis_bridge_8017F280`. Byte-for-byte the same body as
/// `func_acropolis_security_room_8017E490`.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it for a dead-zone
/// curve) and then the d-pad -- whose four bits select one of eight
/// 1/16-of-a-turn headings fed to `rsin`/`rcos` -- into the prompt's
/// 1/512-pixel position, clamps that to the screen, classifies the confirm
/// (0x40) and cancel (0xA0) buttons into the prompt's two button slots, and
/// finally hands the rounded position to `Room_Draw36` to draw the cursor.
/// `RoomActionPrompt::targetId` doubles as the cursor speed here and `field_E`
/// as the double-press window: a second press inside that many frames without
/// the cursor having moved reports state 4 instead of 2.
///
/// `step` carries the analog delta first and the d-pad heading afterwards, and
/// `idx` indexes the button slots in `u16` units so that `i` survives as the
/// loop counter.
void func_acropolis_bridge_8017ED38(Task* task)
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
        Room_Draw36(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}
