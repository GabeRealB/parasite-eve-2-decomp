#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_r47.h"

/// One entry of a map-marker table: the area it stands for and the screen
/// position of its marker. A table ends at the entry whose `stage` is 0xFF.
typedef struct {
    s16 stage;
    u16 area;
    s16 x;
    s16 y;
} ShelterR47MapMark;

extern GpAreaKey D_8007216C;
extern s16       D_80114D08;

/// Menu input lock, counted down by `Gp_TickMenuLock`.
extern s16 Gp_MenuLockDelay;

/// Area views published for the first cap script's five selections, indexed by
/// the selection; entry 1 is switched between views 0x12 and 0x24 by the low bit
/// of game flag 0xD5.
extern u8 D_shelter_r47_80186FAC[5];

/// Byte sequences selected by `ShelterR47State::step` and walked by
/// `field_48`; `0xFF` ends a sequence.
extern u8* D_shelter_r47_80187374[];

/// Marker tables indexed by `ShelterR47State2::field_1C`; the second is used
/// while game-flag nibble 0xDF is 1.
extern ShelterR47MapMark* D_shelter_r47_801875C4[];
extern ShelterR47MapMark* D_shelter_r47_801875D8[];

extern s16 D_shelter_r47_801875EC[];
extern s16 D_shelter_r47_801875F8[][2];

/// Latches set the first time selection kinds 0 and 1 play their one-off cap
/// events, so later selections skip them.
extern u8 D_shelter_r47_8018A694;
extern u8 D_shelter_r47_8018A695;

void func_shelter_r47_801816CC(Task* task);
void func_shelter_r47_80181F14(Task* task, s16 y);
void func_shelter_r47_801820C0(s16 arg0);
void func_shelter_r47_80182348(Task* task);
void func_shelter_r47_80182470(Task* task);
void func_shelter_r47_801828D0(s32 x, s32 y, s32 variant);
s16  func_shelter_r47_801829B8(Task* task, s16 arg1);
void func_shelter_r47_80182C78(Task* task);
void func_shelter_r47_80182CA4(Task* task);
void func_shelter_r47_80182DAC(Task* task);
void func_shelter_r47_80182E78(Task* task);
void func_shelter_r47_80182F18(Task* task);
void func_shelter_r47_80182FDC(Task* task);
void func_shelter_r47_80183068(Task* task);
void func_shelter_r47_801830B8(Task* task);
void func_shelter_r47_80183170(Task* task);
void func_shelter_r47_801831C8(Task* task);
void func_shelter_r47_80183284(Task* task);
void func_shelter_r47_801832E4(s16 step);
void func_shelter_r47_801832EC(Task* task);
void func_shelter_r47_8018337C(Task* task);
void func_shelter_r47_801833DC(Task* task, s16 arg1);
void func_shelter_r47_80183484(Task* task);

/// State handlers of the room's first cap script, run by
/// `func_shelter_r47_80182B18`.
const TaskFuncTable14 D_shelter_r47_8017D6C8 = {
    {
        func_shelter_r47_8018138C,
        func_shelter_r47_80182C78,
        func_shelter_r47_80182CA4,
        func_shelter_r47_80181568,
        func_shelter_r47_80182DAC,
        func_shelter_r47_801816CC,
        func_shelter_r47_80182E78,
        func_shelter_r47_80182FDC,
        func_shelter_r47_80182F18,
        func_shelter_r47_80183068,
        func_shelter_r47_801830B8,
        func_shelter_r47_80183170,
        func_shelter_r47_801831C8,
        func_shelter_r47_80182348,
    },
};

/// Acts on `selection`, the hotspot id stored by `func_shelter_r47_80181568`,
/// when `func_800D4EC0` returns nonzero: the id's high byte picks the kind.
/// Kind 0 accepts a new low byte into `field_4F` (checked by
/// `func_shelter_r47_801829B8` while `field_51` is set, and the first time
/// gated by a one-off cap event otherwise), clears `field_3A`..`field_40` and
/// moves to state 7. Kind 1 moves to state 9 after its one-off event, kind 2
/// starts the cap event for the current `step`, and kinds 3 and 4 start their
/// own events. Every other outcome returns to state 3.
void func_shelter_r47_801816CC(Task* task)
{
    ShelterR47State*  work;
    ShelterR47State*  w;
    RoomActionPrompt* prompt;
    u32               kind;

    prompt = &D_80114D28;
    work   = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        kind = (u16)work->selection.id >> 8;
        if (kind == 0) {
            if (work->field_4F != (work->selection.id & 0xFF)) {
                if (work->field_51 != 0) {
                    if (func_shelter_r47_801829B8(task, work->selection.id & 0xFF) == 0) {
                        task->state = 3;
                        return;
                    }
                    work->field_50 = work->field_4F;
                    work->field_4F = work->selection.id;
                    w              = (ShelterR47State*)task->work;
                    w->field_3A    = 0;
                    w->field_3C    = 0;
                    w->field_3E    = 0;
                    w->field_40    = 0;
                    task->state    = 7;
                    return;
                }
                if (D_shelter_r47_8018A695 == 0) {
                    Gp_StartCapSlot(0x2E, 0, 0);
                    D_shelter_r47_8018A695 = 1;
                    task->state            = 3;
                    return;
                }
                work->field_50 = work->field_4F;
                work->field_4F = work->selection.id;
                w              = (ShelterR47State*)task->work;
                w->field_3A    = 0;
                w->field_3C    = 0;
                w->field_3E    = 0;
                w->field_40    = 0;
                task->state    = 7;
                return;
            }
            if (work->field_51 != 0) {
                Gp_StartCapSlot(0xF, 0, 0);
            }
            task->state = 3;
            return;
        }
        if (kind == 1) {
            if (D_shelter_r47_8018A694 == 0) {
                Gp_StartCapSlot(0x2C, 0, 0);
                D_shelter_r47_8018A694 = kind;
                task->state            = 3;
                return;
            }
            task->state = 9;
            return;
        }
        if (kind == 2) {
            switch (work->step) {
                case 0:
                    Gp_StartCapSlot(0x15, 0, 0);
                    break;
                case 1:
                    Gp_StartCapSlot(0x16, 0, 0);
                    break;
                case 2:
                    Gp_StartCapSlot(0x17, 0, 0);
                    break;
                case 3:
                    Gp_StartCapSlot(0x18, 0, 0);
                    break;
                case 4:
                    Gp_StartCapSlot(0x1A, 0, 0);
                    break;
                case 5:
                    Gp_StartCapSlot(0x19, 0, 0);
                    break;
                case 6:
                    Gp_StartCapSlot(0x1B, 0, 0);
                    break;
                case 7:
                    Gp_StartCapSlot(0x1C, 0, 0);
                    break;
                case 8:
                    Gp_StartCapSlot(0x22, 0, 0);
                    break;
                case 9:
                    Gp_StartCapSlot(0x23, 0, 0);
                    break;
            }
            task->state = 3;
            return;
        }
        if (kind == 3) {
            Gp_StartCapSlot(0x2B, 0, 0);
            task->state = 3;
            return;
        }
        if (kind == 4) {
            Gp_StartCapSlot(0x2D, 0, 0);
            task->state = 3;
            return;
        }
        return;
    }
    task->state = 3;
}

/// Sets the task's `step` to `st` and draws sprite `id` at (`field_30`,
/// `field_32`).
#define SHELTER_R47_DRAW_STEP(id, st)                          \
    {                                                          \
        s16 x_                               = work->field_30; \
        s16 y_                               = work->field_32; \
        ((ShelterR47State*)task->work)->step = (st);           \
        func_shelter_r47_80180F38(x_, y_, (id));               \
    }

/// Per-frame draw of the cap script's selection screen. While `D_8007216C` is
/// 0x14 it scrolls the background by `field_46`. Every positioned sprite is
/// eased a quarter of the way toward its target each frame. `arg1` picks the
/// layout: the entry drawn is `field_4F` when it is 0 and `field_50` otherwise,
/// that entry's toggle selects `step`, and the five rows
/// in `field_0`/`field_C` either all settle at one column or fan out, with the
/// selected row marked.
void func_shelter_r47_80181914(Task* task, s16 arg1)
{
    ShelterR47State* work;
    s32              i;
    s16              id;
    s16              nx;
    s32              x;
    s32              ny;
    s16              y;
    s8               c;
    s16              sel;

    work = (ShelterR47State*)task->work;
    if (D_8007216C.view == 0x14) {
        if (work->field_52 & 1) {
            work->field_46--;
            if (work->field_46 < 0) {
                work->field_46 = 0;
            }
        } else {
            work->field_46++;
            if (work->field_46 > 0x140) {
                work->field_46 = 0x140;
            }
        }
        func_shelter_r47_801820C0(work->field_46);
    }
    if (work->field_42 > 0) {
        work->field_42--;
    }
    work->field_28 += (-0x98 - work->field_28) >> 2;
    func_shelter_r47_80180F38(work->field_28, work->field_2A, 0);
    id = 1;
    if (arg1 == 0) {
        work->field_2E += (0x48 - work->field_2E) >> 2;
        if (work->field_42 == 0) {
            func_shelter_r47_80180F38(work->field_2C, work->field_2E, id);
        } else {
            func_shelter_r47_80180F38(work->field_2C, work->field_2E, 2);
        }
        work->field_32 += (0x58 - work->field_32) >> 2;
        func_shelter_r47_80181F14(task, work->field_32);
        switch (work->field_4F) {
            case 0:
                if (work->toggles[0] == 0) {
                    SHELTER_R47_DRAW_STEP(3, 0);
                } else {
                    SHELTER_R47_DRAW_STEP(4, 1);
                }
                break;
            case 1:
                if (work->toggles[1] == 0) {
                    SHELTER_R47_DRAW_STEP(5, 2);
                } else {
                    SHELTER_R47_DRAW_STEP(6, 3);
                }
                break;
            case 2:
                if (work->toggles[2] == 0) {
                    SHELTER_R47_DRAW_STEP(7, 4);
                } else {
                    SHELTER_R47_DRAW_STEP(8, 5);
                }
                break;
            case 3:
                if (work->toggles[3] == 0) {
                    SHELTER_R47_DRAW_STEP(9, 6);
                } else {
                    SHELTER_R47_DRAW_STEP(10, 7);
                }
                break;
            case 4:
                if (work->toggles[4] == 0) {
                    SHELTER_R47_DRAW_STEP(11, 8);
                } else {
                    SHELTER_R47_DRAW_STEP(12, 9);
                }
                break;
        }
    } else {
        work->field_2E += (0x80 - work->field_2E) >> 2;
        func_shelter_r47_80180F38(work->field_2C, work->field_2E, id);
        work->field_32 += (0x90 - work->field_32) >> 2;
        func_shelter_r47_80181F14(task, work->field_32);
        switch (work->field_50) {
            case 0:
                if (work->toggles[0] == 0) {
                    SHELTER_R47_DRAW_STEP(3, 0);
                } else {
                    SHELTER_R47_DRAW_STEP(4, 1);
                }
                break;
            case 1:
                if (work->toggles[1] == 0) {
                    SHELTER_R47_DRAW_STEP(5, 2);
                } else {
                    SHELTER_R47_DRAW_STEP(6, 3);
                }
                break;
            case 2:
                if (work->toggles[2] == 0) {
                    SHELTER_R47_DRAW_STEP(7, 4);
                } else {
                    SHELTER_R47_DRAW_STEP(8, 5);
                }
                break;
            case 3:
                if (work->toggles[3] == 0) {
                    SHELTER_R47_DRAW_STEP(9, 6);
                } else {
                    SHELTER_R47_DRAW_STEP(10, 7);
                }
                break;
            case 4:
                if (work->toggles[4] == 0) {
                    SHELTER_R47_DRAW_STEP(11, 8);
                } else {
                    SHELTER_R47_DRAW_STEP(12, 9);
                }
                break;
        }
    }
    if (arg1 == 0) {
        for (i = 0; i < 5; i++) {
            nx               = work->field_0[i] + ((0x78 - work->field_0[i]) >> 2);
            work->field_0[i] = nx;
            if (work->field_4F == i) {
                ny = work->field_C[i];
                func_shelter_r47_80180F38((s16)(nx + 0x18), ny, 0x14);
                func_shelter_r47_80180F38(nx, ny, 0x12);
            } else {
                func_shelter_r47_80180F38(nx, work->field_C[i], 0x12);
            }
        }
    } else {
        for (i = 0; i < 5; i++) {
            if (work->field_4F == i) {
                nx               = work->field_0[i] + ((0x78 - work->field_0[i]) >> 2);
                ny               = work->field_C[i];
                work->field_0[i] = nx;
                func_shelter_r47_80180F38((s16)(nx + 0x18), ny, 0x14);
                func_shelter_r47_80180F38(nx, ny, 0x13);
            } else {
                switch (i) {
                    case 0:
                        work->field_0[i] += (0xAA - work->field_0[i]) >> 2;
                        break;
                    case 1:
                        work->field_0[i] += (0xBE - work->field_0[i]) >> 2;
                        break;
                    case 2:
                        work->field_0[i] += (0xD2 - work->field_0[i]) >> 2;
                        break;
                    case 3:
                        work->field_0[i] += (0xE6 - work->field_0[i]) >> 2;
                        break;
                    case 4:
                        work->field_0[i] += (0xFA - work->field_0[i]) >> 2;
                        break;
                }
                func_shelter_r47_80180F38(work->field_0[i], work->field_C[i], 0x12);
            }
        }
    }
    if (arg1 == 0) {
        c = work->field_4F;
        x = work->field_24;
        y = work->field_26;
    } else {
        c = work->field_50;
        x = work->field_24;
        y = work->field_26;
    }
    work->field_24 += (0x7E - x) >> 2;
    sel             = c;
    if ((u16)sel < 5) {
        func_shelter_r47_80180F38(work->field_24, y, (s16)(sel + 0xD));
    }
}

/// Draws the current byte of the `step` sequence at row `y` through
/// `func_shelter_r47_80180F38`, with a textured quad whose left edge follows
/// the byte's value, advancing `field_48` on odd animation frames. At the
/// terminator it redraws the previous byte for eight frames out of every
/// sixteen instead.
void func_shelter_r47_80181F14(Task* task, s16 y)
{
    ShelterR47State* work;
    POLY_FT4*        poly;
    u8*              p;
    s32              c;

    work = (ShelterR47State*)task->work;
    p    = D_shelter_r47_80187374[work->step] + work->field_48;
    c    = *p;
    if (c != 0xFF) {
        func_shelter_r47_80180F38(c - 0x9D, y, 0x14);
        poly           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(poly + 1);
        setPolyFT4(poly);
        setUVWH(poly, 0x48, 0xB9, 0x2C, 0xE);
        poly->tpage = 0xD;
        poly->clut  = 0x3FC3;
        setXY4(poly, c - 0x96, y + 1, 0x69, y + 1, c - 0x96, y + 0xF, 0x69, y + 0xF);
        poly->code |= 1;
        addPrim(&gGpuCurrentOt[10], poly);
        if (gDisplayState.animFrame & 1) {
            work->field_48++;
        }
    } else {
        c = p[-1];
        if ((u32)(gDisplayState.animFrame & 0xF) < 8) {
            func_shelter_r47_80180F38(c - 0x9D, y, 0x14);
        }
    }
}

void func_shelter_r47_801820C0(s16 arg0)
{
    GpTpageSprt* p;
    SPRT*        sprt;

    p              = (GpTpageSprt*)gGpuPrimCursor;
    sprt           = &p->sprt;
    gGpuPrimCursor = (u8*)(p + 1);
    setlen(&p->tpage, 1);
    setlen(&p->sprt, 4);
    p->tpage.code[0] = 0xE1000096;
    setcode(&p->sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x4000;
    sprt->x0    = -0xA0 - arg0;
    sprt->y0    = -0x78;
    sprt->u0    = 0;
    sprt->v0    = 0;
    sprt->w     = 0x100;
    sprt->h     = 0xF0;
    sprt->code |= 1;
    addPrim(&gGpuCurrentOt[12], p);

    p              = (GpTpageSprt*)gGpuPrimCursor;
    sprt           = &p->sprt;
    gGpuPrimCursor = (u8*)(p + 1);
    setlen(&p->tpage, 1);
    setlen(&p->sprt, 4);
    p->tpage.code[0] = 0xE1000098;
    setcode(&p->sprt, 0x64);
    MargePrim(p, sprt);
    sprt->x0    = 0x60 - arg0;
    sprt->clut  = 0x4000;
    sprt->y0    = -0x78;
    sprt->u0    = 0;
    sprt->v0    = 0;
    sprt->w     = 0x80;
    sprt->h     = 0xF0;
    sprt->code |= 1;
    addPrim(&gGpuCurrentOt[12], p);

    p              = (GpTpageSprt*)gGpuPrimCursor;
    sprt           = &p->sprt;
    gGpuPrimCursor = (u8*)(p + 1);
    setlen(&p->tpage, 1);
    setlen(&p->sprt, 4);
    p->tpage.code[0] = 0xE100008E;
    setcode(&p->sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x4040;
    sprt->x0    = 0xE0 - arg0;
    sprt->y0    = -0x78;
    sprt->u0    = 0;
    sprt->v0    = 0;
    sprt->w     = 0x100;
    sprt->h     = 0xF0;
    sprt->code |= 1;
    addPrim(&gGpuCurrentOt[12], p);
}

void func_shelter_r47_80182348(Task* task)
{
    ShelterR47State* state;
    ShelterR47State* done;
    u16              fade;
    u8               level;

    state = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    fade        = state->fade + 0x10;
    state->fade = fade;
    if ((s16)fade >= 0x100) {
        state->fade = 0xFF;
        done        = (ShelterR47State*)task->work;
        GameFlag_SetNibble(0xAC, done->toggles[0]);
        GameFlag_SetNibble(0xD5, done->toggles[1]);
        GameFlag_SetNibble(0xAE, done->toggles[2]);
        GameFlag_SetNibble(0xD6, done->toggles[3]);
        GameFlag_SetNibble(0xD2, done->toggles[4]);
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        Gp_MenuLockDelay = 8;
        D_80114D08       = 0xA;
        Display_ReleaseRef();
        gGameSession->eventState   = 0;
        gGameSession->hideHud      = 0;
        gGameSession->cutsceneHold = 0;
        taskKill((Task*)task->spawnArg2);
        Task_RequestKill(task, 0);
    }
    level = (u8)state->fade;
    Fade_DrawOverlay(level, level, level, 2);
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
/// two button slots, and hands the rounded position to `func_shelter_r47_801828D0` to
/// draw the cursor. `RoomActionPrompt::targetId` serves as the cursor speed and
/// `field_E` as the double-press window: a second press within that many frames
/// without the cursor having moved reports state 4 instead of 2.
void func_shelter_r47_80182470(Task* task)
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
        func_shelter_r47_801828D0(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues one 16x24 textured quad, the action prompt's cursor icon, at (`x`,
/// `y`) into the head of the current OT. `variant` selects the palette, 0x3C87
/// when it is 2 and 0x3C88 otherwise; 0 draws nothing.
void func_shelter_r47_801828D0(s32 x, s32 y, s32 variant)
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

s16 func_shelter_r47_801829B8(Task* task, s16 arg1)
{
    ShelterR47State* state;
    s8               step;

    state = (ShelterR47State*)task->work;
    step  = state->field_51;
    switch (step) {
        case 1:
            if (arg1 != step) {
                Gp_StartCapSlot(0x10, 0, 1);
                return 0;
            }
            state->field_51 = 2;
            return 1;
        case 2:
            if (arg1 != step) {
                Gp_StartCapSlot(0x10, 0, 2);
                return 0;
            }
            state->field_51 = 3;
            return 1;
        case 3:
            if (arg1 != step) {
                Gp_StartCapSlot(0x10, 0, 3);
                return 0;
            }
            state->field_51 = 4;
            return 1;
    }
    return 0;
}

/// Loads the script's working copies of game flags 0xAC, 0xD5, 0xAE, 0xD6 and
/// 0xD2, and sets `D_shelter_r47_80186FAC[1]` from the low bit of flag 0xD5.
void func_shelter_r47_80182AA0(Task* task)
{
    ShelterR47State* state = (ShelterR47State*)task->work;

    state->toggles[0] = GameFlag_GetNibble(0xAC);
    state->toggles[1] = GameFlag_GetNibble(0xD5);
    if (!(state->toggles[1] & 1)) {
        D_shelter_r47_80186FAC[1] = 0x12;
    } else {
        D_shelter_r47_80186FAC[1] = 0x24;
    }
    state->toggles[2] = GameFlag_GetNibble(0xAE);
    state->toggles[3] = GameFlag_GetNibble(0xD6);
    state->toggles[4] = GameFlag_GetNibble(0xD2);
}

void func_shelter_r47_80182B18(Task* task)
{
    TaskFuncTable14 states;

    states = D_shelter_r47_8017D6C8;
    states.funcs[task->state](task);
}

/// Hit-tests the point (`x`, `y`) against every entry of a hotspot table up to
/// its -1 terminator, raising `hit` on each entry whose rectangle contains the
/// point (edges inclusive) and clearing it on the rest. Entry 0x101 is never
/// raised while the task's `field_4F` is 1. Returns 1 if any entry was raised.
s32 func_shelter_r47_80182B9C(Task* task, RoomHotspot* table, s16 x, s16 y)
{
    ShelterR47State* work;
    s32              hit;

    work = (ShelterR47State*)task->work;
    hit  = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y) &&
            ((work->field_4F != 1) || (table->id != 0x101))) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// Zeroes the action prompt's target id, mode and screen position, and steps
/// the caller's script on one state.
void func_shelter_r47_80182C78(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0;
    prompt->mode        = 0;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

void func_shelter_r47_80182CA4(Task* task)
{
    ShelterR47State* state;
    s32              flag;
    s32              value;

    state = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    if ((s16)func_shelter_r47_8018097C(task) != 0) {
        if (state->field_51 == 1) {
            Gp_StartCapSlot(0xA, 0, 0);
        }
        if (state->field_51 == 0) {
            func_shelter_r47_801832E4(state->step);
        }
        switch (((ShelterR47State*)task->work)->step) {
            case 0:
                flag  = 0x1C6;
                value = 2;
                break;
            case 1:
                flag  = 0x1C6;
                value = 0;
                break;
            case 4:
                flag  = 0x1C4;
                value = 2;
                break;
            case 5:
                flag  = 0x1C4;
                value = 0;
                break;
            default:
                task->state++;
                return;
        }
        GameFlag_SetNibble(flag, value);
        task->state++;
    }
}

/// Clears the action prompt's mode and target, drops the prompt kind to 0 when
/// its gating flags are clear, shows the prompt, and moves the script to state 5.
void func_shelter_r47_80182DAC(Task* task)
{
    ShelterR47State*  state;
    RoomActionPrompt* prompt = &D_80114D28;

    state = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (state->field_51 == 0 && (state->selection.raw >> 8) == 0 && D_shelter_r47_8018A695 == 0) {
        state->promptKind = 0;
    }
    if (((s16)state->selection.raw >> 8) == 1 && D_shelter_r47_8018A694 == 0) {
        state->promptKind = 0;
    }
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, state->promptKind);
    task->state = 5;
}

void func_shelter_r47_80182E78(Task* task)
{
    ShelterR47State* state;

    state      = (ShelterR47State*)task->work;
    D_80114D08 = 0xA;
    func_shelter_r47_8018337C(task);
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    D_8007216C.view            = state->field_4E;
    /* Keeps the `spawnArg2` load below the `D_8007216C` store, so that it
       does not fill `taskKill`'s delay slot. */
    SOFT_BARRIER();
    taskKill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

void func_shelter_r47_80182F18(Task* task)
{
    s16 step;
    s32 flag;
    s32 value;

    func_shelter_r47_80181914(task, 0);
    if ((s16)func_shelter_r47_8018097C(task) != 0) {
        func_shelter_r47_801832EC(task);
        step = ((ShelterR47State*)task->work)->step;
        switch (step) {
            case 0:
                flag  = 0x1C6;
                value = 2;
                break;
            case 1:
                flag  = 0x1C6;
                value = 0;
                break;
            case 4:
                flag  = 0x1C4;
                value = 2;
                break;
            case 5:
                flag  = 0x1C4;
                value = 0;
                break;
            default:
                task->state = 3;
                return;
        }
        GameFlag_SetNibble(flag, value);
        task->state = 3;
    }
}

void func_shelter_r47_80182FDC(Task* task)
{
    ShelterR47State* state;
    ShelterR47State* work;

    state = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 1);
    if ((s16)func_shelter_r47_80180C48(task) != 0) {
        D_8007216C.view = D_shelter_r47_80186FAC[state->selection.index];
        state->field_48 = 0;
        work            = (ShelterR47State*)task->work;
        work->field_3A  = 0xFF;
        work->field_3C  = 0xFF;
        work->field_3E  = 0xFF;
        work->field_40  = 0xFF;
        task->state++;
    }
}

void func_shelter_r47_80183068(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    state->field_48 = 0;
    state->field_42 = 0x10;
    task->state++;
}

void func_shelter_r47_801830B8(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->work;
    func_shelter_r47_801833DC(task, state->field_4F);
    func_shelter_r47_80181914(task, 0);
    switch (state->step) {
        case 0:
            GameFlag_SetNibble(0x1C6, 2);
            break;
        case 1:
            GameFlag_SetNibble(0x1C6, 0);
            break;
        case 4:
            GameFlag_SetNibble(0x1C4, 2);
            break;
        case 5:
            GameFlag_SetNibble(0x1C4, 0);
            break;
        case 2:
        case 3:
        case 6:
        case 7:
        case 8:
        case 9:
            break;
    }
    task->state++;
}

void func_shelter_r47_80183170(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    if ((state->field_42 == 0) && (Gp_CapBusy() == 0)) {
        task->state = 3;
    }
}

void func_shelter_r47_801831C8(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    state->fade = 0;
    task->state++;
}

void func_shelter_r47_80183210(void)
{
    D_shelter_r47_8018A694 = 0;
    D_shelter_r47_8018A695 = 0;
    D_shelter_r47_8018A696 = 0;
    D_shelter_r47_8018A697 = 0;
}

/// Two-state dispatcher of the action prompt, with its handler table built on
/// the stack: state 0 runs `func_shelter_r47_80183284` and state 1 runs
/// `func_shelter_r47_80182470`.
void func_shelter_r47_80183234(Task* task)
{
    TaskFunc funcs[2] = {
        func_shelter_r47_80183284,
        func_shelter_r47_80182470,
    };

    funcs[task->state](task);
}

/// State 0 of the action prompt's dispatcher: resets both prompt slots before
/// the first cursor scan (position and hold counters cleared, cursor speed
/// 0x100, double-press window 0xF, `mode` 1) and advances the task's state.
void func_shelter_r47_80183284(Task* task)
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

void func_shelter_r47_801832E4(s16 step)
{
}

void func_shelter_r47_801832EC(Task* task)
{
    ShelterR47State* state = (ShelterR47State*)task->work;

    switch (state->field_51) {
        case 0:
            func_shelter_r47_801832E4(state->step);
            break;
        case 2:
            Gp_StartCapSlot(0xB, 0, 0);
            break;
        case 3:
            Gp_StartCapSlot(0xC, 0, 0);
            break;
        case 4:
            Gp_StartCapSlot(0xD, 0, 0);
            break;
    }
}

void func_shelter_r47_8018337C(Task* task)
{
    ShelterR47State* state;

    state = (ShelterR47State*)task->work;
    GameFlag_SetNibble(0xAC, state->toggles[0]);
    GameFlag_SetNibble(0xD5, state->toggles[1]);
    GameFlag_SetNibble(0xAE, state->toggles[2]);
    GameFlag_SetNibble(0xD6, state->toggles[3]);
    GameFlag_SetNibble(0xD2, state->toggles[4]);
}

/// Flips toggle `arg1`. Toggle 1 also publishes the area view
/// (0x12 or 0x24), and toggle 3 is mirrored into `field_52`.
void func_shelter_r47_801833DC(Task* task, s16 arg1)
{
    ShelterR47State* state;

    state                = (ShelterR47State*)task->work;
    state->toggles[arg1] = (state->toggles[arg1] + 1) & 1;
    switch (arg1) {
        case 0:
        case 2:
        case 4:
            break;
        case 1:
            if (!(state->toggles[1] & 1)) {
                D_shelter_r47_80186FAC[1] = 0x12;
                D_8007216C.view           = 0x12;
            } else {
                D_shelter_r47_80186FAC[1] = 0x24;
                D_8007216C.view           = 0x24;
            }
            break;
        case 3:
            if (!(state->toggles[3] & 1)) {
                state->field_52 = 0;
            } else {
                state->field_52 = 1;
            }
            break;
    }
}

/// Draws the map overlay of the room's second cap script, brightening each
/// quad by 0x30 over the last. While `field_1C` is not 3 it draws one marker
/// per entry of the `field_1C` marker table whose area object has 0x4 set and
/// 0x2 clear in `GpAreaObj::field_1`, after a fixed marker when `field_1C` is
/// 0, `field_2A` is not 1 and collected bit 0x12D is set. When `field_1C` is 3
/// it first moves `field_2A` from 2 to 3 and starts cap slot 0x13, then draws
/// the same markers if game-flag nibble 0xDF is 1, and otherwise the
/// `field_A` x `field_C` map quad.
void func_shelter_r47_80183484(Task* task)
{
    ShelterR47State2*  state;
    u16                stage;
    ShelterR47MapMark* mark;
    GpAreaRec*         tbl;
    GpAreaObj*         obj;
    POLY_FT4*          p;
    GpAreaKey          key;
    s32                flag;
    s16                bit;
    s16                visible;
    u8                 shade;

    state = (ShelterR47State2*)task->work;
    shade = (u8)state->field_26 * 4;
    if (GameFlag_GetNibble(0xDF) == 1) {
        mark = D_shelter_r47_801875D8[state->field_1C];
    } else {
        mark = D_shelter_r47_801875C4[state->field_1C];
    }
    if (state->field_1C != 3) {
        if (state->field_2A != 1 && state->field_1C == 0 && Gp_HasCollectedBit(0x12D) != 0) {
            shade         += 0x30;
            p              = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(p + 1);
            setPolyFT4(p);
            setUV4(p, 0x58, 0x20, 0x60, 0x20, 0x58, 0x28, 0x60, 0x28);
            p->tpage = 0x2F;
            p->clut  = 0x3FC7;
            setRGB0(p, shade, shade, shade);
            setSemiTrans(p, 1);
            setXY4(p, 0x78, -0x4B, 0x80, -0x4B, 0x78, -0x43, 0x80, -0x43);
            addPrim(&gGpuCurrentOt[10], p);
        }
        stage = mark->stage;
        while (mark->stage != 0xFF) {
            u16 area;

            area      = mark->area;
            key.stage = stage;
            key.room  = 1;
            key.view  = 2;
            key.area  = area;
            tbl       = Gp_AreaTables[key.stage];
            if (tbl != NULL) {
                obj = tbl[key.area].field_4;
                if (obj != NULL) {
                    bit  = obj->field_1 & 4;
                    flag = bit != 0;
                } else {
                    flag = 0;
                }
            } else {
                flag = 0;
            }
            if (flag == 1) {
                if (Gp_GetAreaFlag2(&key) == flag) {
                    SOFT_BARRIER();
                    visible = 0;
                } else {
                    visible = 1;
                }
            } else {
                visible = 0;
            }
            if (visible) {
                p              = (POLY_FT4*)gGpuPrimCursor;
                shade         += 0x30;
                gGpuPrimCursor = (u8*)(p + 1);
                setPolyFT4(p);
                setUV4(p, 0x50, 0x20, 0x58, 0x20, 0x50, 0x28, 0x58, 0x28);
                setRGB0(p, shade, shade, shade);
                p->tpage = 0x2F;
                p->clut  = 0x3FC6;
                setSemiTrans(p, 1);
                setXY4(p, mark->x - 4, mark->y - 4, mark->x + 4, mark->y - 4, mark->x - 4, mark->y + 4,
                       mark->x + 4, mark->y + 4);
                addPrim(&gGpuCurrentOt[10], p);
            }
            mark++;
            stage = mark->stage;
        }
    } else {
        if (state->field_2A == 2) {
            state->field_2A = 3;
            Gp_StartCapSlot(0x13, 0, 0);
        }
        if (GameFlag_GetNibble(0xDF) == 1) {
            stage = mark->stage;
            while (mark->stage != 0xFF) {
                u16 area;

                area      = mark->area;
                key.stage = stage;
                key.room  = 1;
                key.view  = 2;
                key.area  = area;
                tbl       = Gp_AreaTables[key.stage];
                if (tbl != NULL) {
                    obj = tbl[key.area].field_4;
                    if (obj != NULL) {
                        bit  = obj->field_1 & 4;
                        flag = bit != 0;
                    } else {
                        flag = 0;
                    }
                } else {
                    flag = 0;
                }
                if (flag == 1) {
                    if (Gp_GetAreaFlag2(&key) == flag) {
                        SOFT_BARRIER();
                        visible = 0;
                    } else {
                        visible = 1;
                    }
                } else {
                    visible = 0;
                }
                if (visible) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    shade         += 0x30;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    setUV4(p, 0x50, 0x20, 0x58, 0x20, 0x50, 0x28, 0x58, 0x28);
                    setRGB0(p, shade, shade, shade);
                    p->tpage = 0x2F;
                    p->clut  = 0x3FC6;
                    setSemiTrans(p, 1);
                    setXY4(p, mark->x - 4, mark->y - 4, mark->x + 4, mark->y - 4, mark->x - 4, mark->y + 4,
                           mark->x + 4, mark->y + 4);
                    addPrim(&gGpuCurrentOt[10], p);
                }
                mark++;
                stage = mark->stage;
            }
        } else {
            if (shade == 0) {
                SndEvt_EnqueueType6(0x542F0005, 0, 0);
            }
            p              = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(p + 1);
            setPolyFT4(p);
            setUV4(p, 0, 0, 0xE8, 0, 0, 0xCE, 0xE8, 0xCE);
            p->tpage = 0x36;
            p->clut  = 0x4000;
            setRGB0(p, shade, shade, shade);
            setSemiTrans(p, 1);
            setXY4(p, -0x4D, -0x67, state->field_A - 0x4D, -0x67, -0x4D, state->field_C - 0x67,
                   state->field_A - 0x4D, state->field_C - 0x67);
            addPrim(&gGpuCurrentOt[11], p);
        }
    }
}

void func_shelter_r47_80183B84(Task* task)
{
    ShelterR47State2* state;
    POLY_FT4*         p;

    state           = (ShelterR47State2*)task->work;
    state->field_A += (state->field_E - state->field_A) >> 2;
    state->field_C += (state->field_10 - state->field_C) >> 2;
    if (state->field_A >= 0xE5) {
        state->field_A = 0xE8;
        state->field_C = 0xCE;
        func_shelter_r47_80183484(task);
        state->field_2B = 0;
        state->field_26++;
    } else {
        state->field_26 = 0;
        state->field_2B = 1;
    }

    p              = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setPolyFT4(p);
    setUV4(p, 0, 0, 0xE8, 0, 0, 0xCE, 0xE8, 0xCE);
    p->tpage = 0x2D;
    p->clut  = 0x3FC0;
    p->code |= 3;
    setXY4(p, -0x4D, -0x67, state->field_A - 0x4D, -0x67, -0x4D, state->field_C - 0x67,
           state->field_A - 0x4D, state->field_C - 0x67);
    addPrim(&gGpuCurrentOt[12], p);

    p                = (POLY_FT4*)gGpuPrimCursor;
    state->field_12 += (state->field_16 - state->field_12) >> 2;
    state->field_14 += (state->field_18 - state->field_14) >> 2;
    gGpuPrimCursor   = (u8*)(p + 1);
    setPolyFT4(p);
    setUV4(p, 0, 0, 0x50, 0, 0, 0x60, 0x50, 0x60);
    p->tpage = 0x2E;
    p->clut  = 0x3FC1;
    p->code |= 3;
    setXY4(p, -0x9C, -0x5B, state->field_12 - 0x9C, -0x5B, -0x9C, state->field_14 - 0x5B,
           state->field_12 - 0x9C, state->field_14 - 0x5B);
    addPrim(&gGpuCurrentOt[11], p);
}

void func_shelter_r47_80183E24(void)
{
    GpTpageSprt* p;
    SPRT*        sprt;

    p              = (GpTpageSprt*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setlen(&p->tpage, 1);
    setlen(&p->sprt, 4);
    p->tpage.code[0] = 0xE100002F;
    setcode(&p->sprt, 0x64);
    sprt = &p->sprt;
    MargePrim(p, sprt);
    sprt->clut        = 0x3FC4;
    sprt->x0          = -0x96;
    sprt->y0          = 0x3F;
    sprt->u0          = 0x50;
    sprt->w           = 0x38;
    sprt->v0          = 0;
    sprt->h           = 0x10;
    sprt->code       |= 3;
    p->tpage.tag      = (p->tpage.tag & 0xFF000000) | (gGpuCurrentOt[11] & 0xFFFFFF);
    gGpuCurrentOt[11] = (gGpuCurrentOt[11] & 0xFF000000) | ((u32)p & 0xFFFFFF);
}

void func_shelter_r47_80183F0C(void)
{
    GpTpageSprt* p;
    SPRT*        sprt;

    p              = (GpTpageSprt*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setlen(&p->tpage, 1);
    setlen(&p->sprt, 4);
    p->tpage.code[0] = 0xE100002F;
    setcode(&p->sprt, 0x64);
    sprt = &p->sprt;
    MargePrim(p, sprt);
    sprt->clut        = 0x3FC5;
    sprt->x0          = -0x90;
    sprt->y0          = 0x50;
    sprt->u0          = 0x50;
    sprt->v0          = 0x10;
    sprt->w           = 0x38;
    sprt->h           = 0x10;
    sprt->code       |= 3;
    p->tpage.tag      = (p->tpage.tag & 0xFF000000) | (gGpuCurrentOt[11] & 0xFFFFFF);
    gGpuCurrentOt[11] = (gGpuCurrentOt[11] & 0xFF000000) | ((u32)p & 0xFFFFFF);
}

void func_shelter_r47_80183FF4(Task* task, s16 arg1)
{
    GpTpageSprt*      p;
    SPRT*             sprt;
    ShelterR47State2* state;

    p                = (GpTpageSprt*)gGpuPrimCursor;
    state            = (ShelterR47State2*)task->work;
    sprt             = &p->sprt;
    gGpuPrimCursor   = (u8*)(p + 1);
    state->field_20 += (state->field_1E - state->field_20) >> 2;
    setlen(&p->tpage, 1);
    setlen(&p->sprt, 4);
    p->tpage.code[0] = 0xE100002F;
    setcode(&p->sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut        = 0x3FC2;
    sprt->code       |= 3;
    sprt->x0          = state->field_20;
    sprt->y0          = -0x67;
    sprt->u0          = 0;
    sprt->v0          = D_shelter_r47_801875EC[arg1];
    sprt->w           = 0x50;
    sprt->h           = 0xA;
    p->tpage.tag      = (p->tpage.tag & 0xFF000000) | (gGpuCurrentOt[11] & 0xFFFFFF);
    gGpuCurrentOt[11] = (gGpuCurrentOt[11] & 0xFF000000) | ((u32)p & 0xFFFFFF);
}

void func_shelter_r47_80184124(Task* task, s16 arg1)
{
    GpTpageSprt*      p;
    SPRT*             sprt;
    ShelterR47State2* state;

    p              = (GpTpageSprt*)gGpuPrimCursor;
    state          = (ShelterR47State2*)task->work;
    sprt           = &p->sprt;
    gGpuPrimCursor = (u8*)(p + 1);
    setlen(&p->tpage, 1);
    setlen(&p->sprt, 4);
    p->tpage.code[0] = 0xE100002F;
    setcode(&p->sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x3FC3;
    sprt->code |= 3;
    sprt->x0    = state->field_20;
    sprt->y0    = 0x35;
    sprt->u0    = 0;
    sprt->v0    = D_shelter_r47_801875F8[arg1][0] + 0x38;
    sprt->w     = 0x50;
    sprt->h     = 8;
    addPrim(&gGpuCurrentOt[11], p);

    p                = (GpTpageSprt*)gGpuPrimCursor;
    sprt             = &p->sprt;
    gGpuPrimCursor   = (u8*)(p + 1);
    p->tpage.code[0] = 0xE100002F;
    setlen(&p->tpage, 1);
    setlen(&p->sprt, 4);
    setcode(&p->sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x3FC3;
    sprt->code |= 3;
    sprt->x0    = state->field_20;
    sprt->y0    = 0x60;
    sprt->u0    = 0;
    sprt->v0    = D_shelter_r47_801875F8[arg1][1] + 0x38;
    sprt->w     = 0x50;
    sprt->h     = 8;
    addPrim(&gGpuCurrentOt[11], p);
}
