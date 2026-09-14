#include "common.h"

#include "actors/actor_215100.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"

/// Imports from the 0x80180000 overlay; no header names them yet.
void            func_80180390(s32 arg0);
void            func_801811C0(s16 arg0);
void            func_801848B4(void);
void            func_80184954(void);
extern TaskDesc D_80185384;
extern TaskDesc D_801856B8;
extern s8       D_8007216C;
extern u8       D_80071085;
extern Task*    D_8018E0C4;
extern TaskDesc D_actor_215100_8014CF6C;
extern TaskDesc D_actor_215100_8014E13C;
extern TaskDesc D_actor_215100_801544FC;
extern TaskDesc D_actor_215100_80154508;
extern Task*    D_actor_215100_8015E64C;
extern s32      D_actor_215100_8014D038;
extern s32      D_actor_215100_8014D03C;
extern s32      D_actor_215100_80153ED4;
extern s32      D_actor_215100_80153FDC;
extern s32      D_actor_215100_801543E4;
/// Caption script table, and the script currently being played back with the
/// entry it is up to.
extern Actor215100Caption** D_actor_215100_8015E650;
extern Actor215100Caption*  D_actor_215100_8015E658;
extern s16                  D_actor_215100_8015E65C;
extern s16                  D_actor_215100_8015E65E;
extern s16                  D_actor_215100_8015E660;
extern s16                  D_actor_215100_8015E662;
extern s16                  D_actor_215100_8015E664;
extern s16                  D_actor_215100_8015E666;
extern s8                   D_actor_215100_8015E66C;
extern s32                  D_actor_215100_8015E670;
extern s16                  D_actor_215100_801544EC;
extern s16                  D_actor_215100_801544EE;
extern Actor215100CharRec   D_actor_215100_8015E678;
extern u8                   D_801153F4;
extern u8                   D_80115690;
void                        func_actor_215100_8014B0D4(void);
void                        func_actor_215100_8014B1B0(s32 arg0);
s32                         func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2);
s16                         func_actor_215100_8014BDFC(s32 arg0);
s16                         func_actor_215100_8014C06C(s32 arg0);
s16                         func_actor_215100_8014C298(s32 arg0);
s32                         func_actor_215100_8014C418(s32 arg0);
void                        func_actor_215100_8014B3C8(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void                        func_actor_215100_8014BEE8(void);

INCLUDE_RODATA("actors/nonmatchings/actor_215100/actor_215100", D_actor_215100_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_80149F2C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014A398);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014A5C0);

/// Watches the caption system while the actor waits to be talked to: state 0
/// polls `Gp_CapBusy` / `Gp_GetCapEventKey`, and on key 2 hands the scene task
/// `D_8018E0C4` its exit and steps to state 1, while any other key kills the
/// task outright. State 1 starts the caption playback and steps to state 2,
/// which commits the ending: it flags the save-slot session, plays the sound,
/// clears the actor's own 0x97B, drops the story flag the sibling
/// `func_actor_215100_8014A908` sets, and releases the display reference.
void func_actor_215100_8014A7C4(Task* arg0)
{
    GameActor* actor;

    actor = (GameActor*)((Task*)Game_GetPtrSlot(3))->idMap;
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_GetCapEventKey() == 2) {
                Task_CallExit(D_8018E0C4);
                arg0->state++;
            } else {
                Task_Kill(arg0);
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(0);
            D_8007216C = 8;
            func_801811C0(0);
            arg0->state++;
            break;
        case 2:
            Game_Session->field_126 = 1;
            Game_Session->field_69 |= 0x80;
            SndEvt_EnqueueType2(0, 0x1E);
            actor->field_97B        = 0;
            D_actor_215100_8014D038 = 0;
            Gp_MsgPlayerWeapon(1);
            Gp_StateC08.field_6 &= 0xFD;
            if (D_80071085 != 0) {
                Display_ReleaseRef();
            }
            Task_Kill(arg0);
            break;
    }
}

void func_actor_215100_8014A908(void)
{
    D_actor_215100_8014D038 = 0;
    if (D_actor_215100_8015E670 < 3) {
        D_8007216C = 8;
        func_801811C0(0);
    } else {
        func_80180390(1);
        D_actor_215100_8014D03C = 1;
    }
    if (D_actor_215100_8015E670 < 4) {
        Gp_StateC08.field_6 &= 0xFD;
    }
    SndEvt_EnqueueType2(0, 0x1E);
}

void func_actor_215100_8014A9A0(void)
{
    if (D_actor_215100_8015E670 == 5) {
        D_actor_215100_8014D038 = 0;
        func_80180390(1);
        D_actor_215100_8014D03C = 1;
        Game_Session->field_126 = 1;
        SndEvt_EnqueueType2(0, 0x1E);
        Game_Session->field_69 |= 0x80;
    }
    if (D_actor_215100_8015E670 < 3) {
        Gp_RunCapCmd(0x1D, 3);
        Task_SpawnFromTable(&D_actor_215100_8014CF6C, 1, 0, 0);
    }
}

/// Hands the actor off to its caption script, or starts one, depending on
/// whether the script for the current story flag has already run.
///
/// The `else` arm is a `do { } while (0)` whose `break` is the "already
/// committed" exit. It is not vestigial: the loop notes it emits make `reorg`
/// mark that branch's label as leaving a loop, so the delay-slot pass predicts
/// it not-taken and fills its slot from the fall-through rather than from the
/// shared `return 2` tail. Without the loop the branch reaches the same label
/// by a copied `li v0,2`, one instruction longer.
s32 func_actor_215100_8014AA54(Actor215100CharRec* arg0)
{
    if (D_actor_215100_8014D038 != 0) {
        if (arg0->field_5 != 0) {
            return 2;
        }
        D_actor_215100_8015E678 = *arg0;
        Gp_MsgPlayerWeapon(0);
        D_801153F4 = 1;
        Gp_RunCapCmd(0x14, 0);
        D_80115690 = 1;
        Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 1, 0);
    } else {
        do {
            if (GameFlag_GetNibble(0xED) == 0) {
                return 1;
            }
            if (arg0->field_5 != 0) {
                break;
            }
            D_actor_215100_8015E678 = *arg0;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(0x17);
            Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 2, 0);
        } while (0);
    }
    return 2;
}

void func_actor_215100_8014AB6C(void)
{
    if (D_actor_215100_8014D038 != 0) {
        func_80184954();
        return;
    }
    Gp_SpawnIfCapIdle(0x11, 1);
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014ABAC);

void func_actor_215100_8014AD50(Task* arg0)
{
    if (arg0->killCountdown % 48 == 0) {
        Task_SpawnFromTable(&D_801856B8, 1, 0, 0);
    }
    arg0->killCountdown = arg0->killCountdown + 1;
}

void func_actor_215100_8014ADD8(void)
{
    Task_SpawnFromTable(&D_80185384, 0, 0, 0);
}

void func_actor_215100_8014AE08(s32 arg0)
{
    if (arg0 != 0) {
        func_801848B4();
    }
}

void func_actor_215100_8014AE2C(s32 arg0)
{
    if (arg0 != 0) {
        D_actor_215100_8015E64C = Task_SpawnFromTable(&D_actor_215100_8014E13C, 2, 0, 0);
        return;
    }
    if (D_actor_215100_8015E64C != NULL) {
        Task_Kill(D_actor_215100_8015E64C);
        D_actor_215100_8015E64C = NULL;
    }
}

void func_actor_215100_8014AE90(s16 arg0)
{
    func_801811C0(arg0);
}

void func_actor_215100_8014AEB4(s16 arg0)
{
    Game_Session->field_52 = arg0;
}

void func_actor_215100_8014AEC4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x300, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_215100_8014AF0C(void)
{
    switch (GameFlag_GetNibble(0xF5)) {
        case 0:
            GameFlag_SetNibble(0xF5, 1);
            func_800E8614((s32)&D_actor_215100_80153ED4, 0);
            break;
        case 1:
            func_800E8614((s32)&D_actor_215100_80153FDC, 0);
            GameFlag_SetNibble(0xF5, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_215100_801543E4, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014AFAC);

void func_actor_215100_8014B0D4(void)
{
    if ((D_actor_215100_8015E658 != NULL) &&
        (D_actor_215100_8015E658[D_actor_215100_8015E662].field_8 != -1) &&
        (Gp_CapBusy() == 0)) {
        func_actor_215100_8014B3C8(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8, 0x80, 1,
                                   D_actor_215100_8015E658[D_actor_215100_8015E662].field_0 |
                                       ((D_actor_215100_8015E658[D_actor_215100_8015E662].field_1 & 0x10) * 0x10));
        if (!(D_actor_215100_8015E658[D_actor_215100_8015E662].field_4 & 1)) {
            func_actor_215100_8014BEE8();
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014B1B0);

/// Starts playing the caption script `arg0` picks out of
/// `D_actor_215100_8015E650`, keyed on `arg1`, and parks its per-line metrics in
/// the globals `func_actor_215100_8014B0D4` reads. Returns 1 when there is no
/// such script, 0 once it is playing; `arg2` is the line delay.
s32 func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2)
{
    Actor215100Caption* caption;
    s16                 entry;

    caption                 = D_actor_215100_8015E650[arg0];
    D_actor_215100_8015E658 = caption;
    if (caption == NULL) {
        return 1;
    }
    D_actor_215100_8015E666 = arg1;
    entry                   = func_actor_215100_8014C418(1);
    D_actor_215100_8015E662 = entry;
    D_actor_215100_8015E660 = arg2;
    D_actor_215100_8015E65C = func_actor_215100_8014C06C(D_actor_215100_8015E658[entry].field_8);
    D_actor_215100_8015E65E = func_actor_215100_8014BDFC(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8);
    D_actor_215100_8015E664 = func_actor_215100_8014C298(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8);
    D_actor_215100_8015E66C = 0x1E;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014B3C8);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014BDFC);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014BEE8);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C06C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C17C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C298);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C360);

s32 func_actor_215100_8014C418(s32 arg0)
{
    s32                 flag;
    s32                 id;
    s32                 base;
    Actor215100Caption* p;

    flag = -1;
    id   = D_actor_215100_8015E666;
    base = (s32)D_actor_215100_8015E658;
    p    = (Actor215100Caption*)(arg0 * sizeof(Actor215100Caption) + base);
loop:
    if (p->field_8 == flag) {
        goto done;
    }
    if (p->field_5 == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

void func_actor_215100_8014C46C(Task* task)
{
    s32 remaining;

    remaining       = task->spawnArg1 - 1;
    task->spawnArg1 = remaining;
    if (remaining <= 0) {
        Task_Kill(task);
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C4A8(Task* task)
{
    s32 remaining;
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            remaining       = task->spawnArg1 - 1;
            task->spawnArg1 = remaining;
            if ((remaining <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0)) {
                Task_Kill(task);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_actor_215100_801544FC, 0, arg2, 0);
}

void func_actor_215100_8014C58C(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_actor_215100_80154508, arg2, 0, 0);
}

void func_actor_215100_8014C5E0(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                   = 0;
    D_actor_215100_801544EC = arg0;
    D_actor_215100_801544EE = arg1;
    for (i = 0; i < 0x32; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg2) {
                func_actor_215100_8014B1B0(D_8006C338[i].field_4);
                break;
            }
            count++;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", ActorsShared80131e24Sub0);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C874);
