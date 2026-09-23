#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Scratch state of the room's first cap script: the task family whose state
/// table is `RoomsShared8017d8d0States` (dispatcher `func_shelter_r47_80182B18`).
/// `memCalloc(0x54)` in its state-0 entry `func_shelter_r47_8018138C`, stored
/// at `Task::work`.
typedef struct {
    /* 0x00 */ u8  pad_0[0x18];
    /* 0x18 */ s16 field_18;  ///< committed to game flag 0xAC when the script ends
    /* 0x1A */ s16 field_1A;  ///< committed to game flag 0xD5 when the script ends
    /* 0x1C */ s16 field_1C;  ///< committed to game flag 0xAE when the script ends
    /* 0x1E */ s16 field_1E;  ///< committed to game flag 0xD6 when the script ends
    /* 0x20 */ s16 field_20;  ///< committed to game flag 0xD2 when the script ends
    /* 0x22 */ u8  pad_22[0x12];
    /* 0x34 */ s16 selection; ///< `id` of the hotspot the player confirmed
    /* 0x36 */ u16 fade;      ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    /* 0x38 */ u8  pad_38[2];
    /* 0x3A */ s16 field_3A;
    /* 0x3C */ s16 field_3C;
    /* 0x3E */ s16 field_3E;
    /* 0x40 */ s16 field_40;
    /* 0x42 */ s16 field_42; ///< counter gating the move to state 3
    /* 0x44 */ s16 step;     ///< sub-step selected by the running cap event
    /* 0x46 */ u8  pad_46[2];
    /* 0x48 */ s16 field_48;
    /* 0x4A */ u8  promptKind; ///< `promptKind` of the hotspot the player confirmed
    /* 0x4B */ u8  pad_4B[4];
    /* 0x4F */ s8  field_4F;   ///< low byte of the last `selection` accepted with kind 0
    /* 0x50 */ s8  field_50;   ///< previous `field_4F`, kept when a new one is accepted
    /* 0x51 */ s8  field_51;
    /* 0x52 */ u8  pad_52[2];
} ShelterR47State;
STATIC_ASSERT_SIZEOF(ShelterR47State, 0x54);

/// Menu input lock, counted down by `Gp_TickMenuLock`.
extern s16 Gp_MenuLockDelay;
extern s16 D_80114D08;

void func_shelter_r47_80180F38(s32 arg0, s16 arg1, s32 arg2);
void func_shelter_r47_80181914(Task* task, s32 arg1);
s16  func_shelter_r47_801829B8(Task* task, s16 arg1);

extern u8 D_shelter_r47_80186FAD;

/// Latches set the first time selection kinds 0 and 1 play their one-off cap
/// events, so later selections skip them.
extern u8 D_shelter_r47_8018A694;
extern u8 D_shelter_r47_8018A695;

/// Byte sequences selected by `ShelterR47State::step` and walked by
/// `field_48`; `0xFF` ends a sequence.
extern u8* D_shelter_r47_80187374[];

/// Acts on `selection`, the hotspot id stored by `func_shelter_r47_80181568`,
/// when `func_800D4EC0` returns nonzero: the id's high byte picks the kind. Kind 0 accepts a new low byte into `field_4F` (checked by
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
        kind = (u16)work->selection >> 8;
        if (kind == 0) {
            if (work->field_4F != (work->selection & 0xFF)) {
                if (work->field_51 != 0) {
                    if (func_shelter_r47_801829B8(task, work->selection & 0xFF) == 0) {
                        task->state = 3;
                        return;
                    }
                    work->field_50 = work->field_4F;
                    work->field_4F = work->selection;
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
                work->field_4F = work->selection;
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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181914);

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
        GameFlag_SetNibble(0xAC, done->field_18);
        GameFlag_SetNibble(0xD5, done->field_1A);
        GameFlag_SetNibble(0xAE, done->field_1C);
        GameFlag_SetNibble(0xD6, done->field_1E);
        GameFlag_SetNibble(0xD2, done->field_20);
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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182470);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801828D0);

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
/// 0xD2, and sets `D_shelter_r47_80186FAD` from the low bit of flag 0xD5.
void func_shelter_r47_80182AA0(Task* task)
{
    ShelterR47State* state = (ShelterR47State*)task->work;

    state->field_18 = GameFlag_GetNibble(0xAC);
    state->field_1A = GameFlag_GetNibble(0xD5);
    if (!(state->field_1A & 1)) {
        D_shelter_r47_80186FAD = 0x12;
    } else {
        D_shelter_r47_80186FAD = 0x24;
    }
    state->field_1C = GameFlag_GetNibble(0xAE);
    state->field_1E = GameFlag_GetNibble(0xD6);
    state->field_20 = GameFlag_GetNibble(0xD2);
}
