#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Scratch state of the room's first cap script: the task family whose state
/// table is `D_shelter_r47_8017D6C8` (dispatcher `func_shelter_r47_80182B18`).
/// `memCalloc(0x54)` in its state-0 entry `func_shelter_r47_8018138C`, stored
/// at `Task::work`.
typedef struct {
    /* 0x00 */ u8  pad_0[0x18];
    /* 0x18 */ s16 field_18; ///< committed to game flag 0xAC when the script ends
    /* 0x1A */ s16 field_1A; ///< committed to game flag 0xD5 when the script ends
    /* 0x1C */ s16 field_1C; ///< committed to game flag 0xAE when the script ends
    /* 0x1E */ s16 field_1E; ///< committed to game flag 0xD6 when the script ends
    /* 0x20 */ s16 field_20; ///< committed to game flag 0xD2 when the script ends
    /* 0x22 */ u8  pad_22[0x12];
    /* 0x34 */ u8  field_34; ///< selects the `D_shelter_r47_80186FAC` byte published as the area view
    /* 0x35 */ u8  pad_35;
    /* 0x36 */ u16 fade;     ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    /* 0x38 */ u8  pad_38[2];
    /* 0x3A */ s16 field_3A;
    /* 0x3C */ s16 field_3C;
    /* 0x3E */ s16 field_3E;
    /* 0x40 */ s16 field_40;
    /* 0x42 */ s16 field_42; ///< counter gating the move to state 3
    /* 0x44 */ s16 step;     ///< sub-step selected by the running cap event
    /* 0x46 */ u8  pad_46[2];
    /* 0x48 */ s16 field_48;
    /* 0x4A */ u8  pad_4A[4];
    /* 0x4E */ u8  field_4E; ///< low byte of `Mc_SaveData.at4.loc.view` saved on entry
    /* 0x4F */ s8  field_4F; ///< selects which of the five halves from `field_18` a step toggles
    /* 0x50 */ u8  pad_50;
    /* 0x51 */ s8  field_51;
    /* 0x52 */ u8  pad_52[2];
} ShelterR47State;
STATIC_ASSERT_SIZEOF(ShelterR47State, 0x54);

/// Scratch state of the room's second cap script: the task family whose state
/// table is `D_shelter_r47_8017D7DC` (dispatcher `func_shelter_r47_80185214`).
/// `memCalloc(0x30)` in its state-0 entry `func_shelter_r47_8018431C`, stored
/// at `Task::work`.
typedef struct {
    /* 0x00 */ u8  pad_0[0xA];
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  pad_C[0xA];
    /* 0x16 */ s16 field_16;
    /* 0x18 */ s16 field_18;
    /* 0x1A */ u8  pad_1A[2];
    /* 0x1C */ s16 field_1C;
    /* 0x1E */ u8  pad_1E[4];
    /* 0x22 */ u16 fade;     ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    /* 0x24 */ u8  pad_24[5];
    /* 0x29 */ u8  field_29; ///< low byte of `Mc_SaveData.at4.loc.view` saved on entry
    /* 0x2A */ s8  field_2A;
    /* 0x2B */ u8  pad_2B[5];
} ShelterR47State2;
STATIC_ASSERT_SIZEOF(ShelterR47State2, 0x30);

/// Menu input lock, counted down by `Gp_TickMenuLock`.
extern s16       Gp_MenuLockDelay;
extern s16       D_80114D08;
extern GpAreaKey D_8007216C;
extern u8        D_shelter_r47_80186FAC[];

s32 func_shelter_r47_80180C48(Task* task);

void func_shelter_r47_801832EC(Task* task);
void func_shelter_r47_8018337C(Task* task);
void func_shelter_r47_801833DC(Task* task, s16 arg1);

extern SVECTOR D_shelter_r47_80187624[];
extern SVECTOR D_shelter_r47_80187664[];

extern u8 D_shelter_r47_8018A694;
extern u8 D_shelter_r47_8018A695;
extern u8 D_shelter_r47_8018A696;
extern u8 D_shelter_r47_8018A697;

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80182B9C);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80182CA4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80182DAC);

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
        D_8007216C.view = D_shelter_r47_80186FAC[state->field_34];
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
    GameFlag_SetNibble(0xAC, state->field_18);
    GameFlag_SetNibble(0xD5, state->field_1A);
    GameFlag_SetNibble(0xAE, state->field_1C);
    GameFlag_SetNibble(0xD6, state->field_1E);
    GameFlag_SetNibble(0xD2, state->field_20);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_801833DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80183484);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80183B84);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80183FF4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80184124);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_4", D_shelter_r47_8017D7DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_8018431C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_801844A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80184658);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_8018489C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80184AE0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80184F40);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_4", func_shelter_r47_80185214);
