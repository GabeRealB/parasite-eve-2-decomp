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
#include "rooms/rooms_shared_8017ecb4.h"
#include "rooms/shelter_r47.h"

/// Scratch state of the room's first cap script: the task family whose state
/// table is `D_shelter_r47_8017D6C8` (dispatcher `func_shelter_r47_80182B18`).
/// `memCalloc(0x54)` in its state-0 entry `func_shelter_r47_8018138C`, stored
/// at `Task::work`.
typedef struct {
    /* 0x00 */ u8  pad_0[0x18];
    /* 0x18 */ s16 field_18[5]; ///< one-bit toggles, committed to game flags 0xAC, 0xD5, 0xAE, 0xD6 and 0xD2 when the script ends
    /* 0x22 */ u8  pad_22[0x12];
    /* 0x34 */ union {
        u16 word;        ///< the high byte gates whether the action prompt keeps its kind
        u8  low;         ///< selects the `D_shelter_r47_80186FAC` byte published as the area view
    } field_34;
    /* 0x36 */ u16 fade; ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    /* 0x38 */ u8  pad_38[2];
    /* 0x3A */ s16 field_3A;
    /* 0x3C */ s16 field_3C;
    /* 0x3E */ s16 field_3E;
    /* 0x40 */ s16 field_40;
    /* 0x42 */ s16 field_42; ///< counter gating the move to state 3
    /* 0x44 */ s16 step;     ///< sub-step selected by the running cap event
    /* 0x46 */ u8  pad_46[2];
    /* 0x48 */ s16 field_48;
    /* 0x4A */ s8  promptKind; ///< display mode forwarded to `func_800D4E78`
    /* 0x4B */ u8  pad_4B[3];
    /* 0x4E */ u8  field_4E;   ///< low byte of `Mc_SaveData.at4.loc.view` saved on entry
    /* 0x4F */ s8  field_4F;   ///< selects which toggle in `field_18` a step flips
    /* 0x50 */ u8  pad_50;
    /* 0x51 */ s8  field_51;
    /* 0x52 */ s8  field_52; ///< mirrors toggle 3 of `field_18`
    /* 0x53 */ u8  pad_53;
} ShelterR47State;
STATIC_ASSERT_SIZEOF(ShelterR47State, 0x54);

/// Menu input lock, counted down by `Gp_TickMenuLock`.
extern s16       Gp_MenuLockDelay;
extern s16       D_80114D08;
extern GpAreaKey D_8007216C;
extern u8        D_shelter_r47_80186FAC[];
extern u8        D_shelter_r47_80186FAD;
extern s16       D_shelter_r47_801875EC[];
extern s16       D_shelter_r47_801875F8[][2];

s32  func_shelter_r47_8018097C(Task* task);
s32  func_shelter_r47_80180C48(Task* task);
void func_shelter_r47_80181914(Task* task, s32 arg1);

void func_shelter_r47_801832E4(s16 step);
void func_shelter_r47_801832EC(Task* task);
void func_shelter_r47_8018337C(Task* task);
void func_shelter_r47_801833DC(Task* task, s16 arg1);
void func_shelter_r47_80183484(Task* task);

extern SVECTOR D_shelter_r47_80187624[];
extern SVECTOR D_shelter_r47_80187664[];

extern u8 D_shelter_r47_8018A694;
extern u8 D_shelter_r47_8018A695;
extern u8 D_shelter_r47_8018A696;
extern u8 D_shelter_r47_8018A697;

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
    if (state->field_51 == 0 && (state->field_34.word >> 8) == 0 && D_shelter_r47_8018A695 == 0) {
        state->promptKind = 0;
    }
    if (((s16)state->field_34.word >> 8) == 1 && D_shelter_r47_8018A694 == 0) {
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
        D_8007216C.view = D_shelter_r47_80186FAC[state->field_34.low];
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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80183234);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80183284);

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
    GameFlag_SetNibble(0xAC, state->field_18[0]);
    GameFlag_SetNibble(0xD5, state->field_18[1]);
    GameFlag_SetNibble(0xAE, state->field_18[2]);
    GameFlag_SetNibble(0xD6, state->field_18[3]);
    GameFlag_SetNibble(0xD2, state->field_18[4]);
}

/// Flips toggle `arg1` of `field_18`. Toggle 1 also publishes the area view
/// (0x12 or 0x24), and toggle 3 is mirrored into `field_52`.
void func_shelter_r47_801833DC(Task* task, s16 arg1)
{
    ShelterR47State* state;

    state                 = (ShelterR47State*)task->work;
    state->field_18[arg1] = (state->field_18[arg1] + 1) & 1;
    switch (arg1) {
        case 0:
        case 2:
        case 4:
            break;
        case 1:
            if (!(state->field_18[1] & 1)) {
                D_shelter_r47_80186FAD = 0x12;
                D_8007216C.view        = 0x12;
            } else {
                D_shelter_r47_80186FAD = 0x24;
                D_8007216C.view        = 0x24;
            }
            break;
        case 3:
            if (!(state->field_18[3] & 1)) {
                state->field_52 = 0;
            } else {
                state->field_52 = 1;
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80183484);

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
