#include "common.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s16            D_80114D08;
extern s32            D_acropolis_bridge_801917A8;
extern TaskFuncTable9 D_acropolis_bridge_8017D614;

void func_acropolis_bridge_8017E60C(s32 arg0, s32 arg1);
void func_acropolis_bridge_8017ED38(Task* task);

/// Work block this room's script tasks keep at `Task::idMap`. `field_4` is the
/// script step handed to `func_acropolis_bridge_8017E60C` and `promptKind` the
/// display mode forwarded to `func_800D4E78`.
typedef struct AcropolisBridgePromptWork {
    /* 0x00 */ byte pad_0[0x4];
    /* 0x04 */ s16  field_4;
    /* 0x06 */ byte pad_6[0x2];
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ byte pad_C[0x2];
    /* 0x0E */ s8   promptKind;
    /* 0x0F */ s8   promptBusy;
} AcropolisBridgePromptWork;

/// Payload this file passes as `Gp_DispatchMsg`'s `arg2` for message 0x7DA,
/// the same record `func_acropolis_bridge_8017DC68` sends.
typedef struct AcropolisBridgeMsg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} AcropolisBridgeMsg7DA;

/// One entry of the bridge's -1-terminated hotspot table
/// (`D_acropolis_bridge_8018983C`). `x` / `y` / `w` / `h` are the screen
/// rectangle `func_acropolis_bridge_8017F6D4` tests the action cursor against;
/// on a hit it raises `hit` on that entry and clears it on every other.
typedef struct AcropolisBridgeHotspot {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 w;
    /* 0x6 */ s16 h;
    /* 0x8 */ s16 id; // list terminator is -1
    /* 0xA */ u8  promptKind;
    /* 0xB */ s8  hit;
} AcropolisBridgeHotspot;

extern AcropolisBridgeHotspot D_acropolis_bridge_8018983C[];

s32 func_acropolis_bridge_8017F6D4(AcropolisBridgeHotspot* table, s16 x, s16 y);

/// Two-state dispatcher of this room's prompt script task: builds the handler
/// table on the stack and tails into the entry named by `Task::state`.
void func_acropolis_bridge_8017F280(Task* task)
{
    TaskFunc states[2] = { Room_Util04, func_acropolis_bridge_8017ED38 };

    states[task->state](task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F2D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F358);

/// Arms the action prompt for a fresh script step: parks the cursor at the top
/// left with the highlight mode on and the cursor speed at 0x80, tears down any
/// prompt still up, then advances the task to its next state.
void func_acropolis_bridge_8017F404(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    func_acropolis_bridge_8017E60C(0xFFF, 0);
    task->state++;
}

/// Spawns the action prompt for the script's current step: closes the previous
/// prompt, clears the highlight state, then re-spawns the prompt at the
/// coordinates the gameplay side left in `D_80114D28` with this step's display
/// mode, and advances the task to state 4.
void func_acropolis_bridge_8017F460(Task* task)
{
    RoomActionPrompt*          prompt = &D_80114D28;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->idMap;

    func_acropolis_bridge_8017E60C(work->field_4, 0);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Closes the prompt the script's current step put up, clears the highlight
/// state, and advances the task to state 2. If `func_800D4EC0` still reports a
/// prompt on screen, the step is flagged busy in `promptBusy` (which the
/// hotspot scan in `func_acropolis_bridge_8017E1D0` gates on) and cap slot 9 is
/// started.
void func_acropolis_bridge_8017F4CC(Task* task)
{
    RoomActionPrompt*          prompt = &D_80114D28;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->idMap;

    func_acropolis_bridge_8017E60C(work->field_4, 0);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        work->promptBusy = 1;
        Gp_StartCapSlot(9, 0, 0);
    }
    task->state = 2;
}

/// Waits ten frames on the prompt the script's current step put up, then closes
/// it. Step 0x561 is the one the room answers with message 0x7DA before its
/// confirmation sound and state 6; every other step just clears the step's
/// counters, plays the cancel sound and goes to state 7. Either way the prompt
/// is torn down and the cursor is re-hit-tested against the room's hotspot
/// table, so `mode` reports whether it ended up over one.
void func_acropolis_bridge_8017F544(Task* task)
{
    RoomActionPrompt*          prompt = &D_80114D28;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->idMap;
    AcropolisBridgeHotspot*    hs     = D_acropolis_bridge_8018983C;

    if (work->field_A < 0xA) {
        work->field_A++;
        return;
    }

    if (work->field_4 != 0x561) {
        SndEvt_EnqueueType6(0x510E0004, 0, 0);
        work->field_8 = 0;
        work->field_A = 0;
        task->state   = 7;
    } else {
        AcropolisBridgeMsg7DA msg = { 1, 0xE, 2 };

        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        SndEvt_EnqueueType6(0x510E0009, 0, 0);
        task->state = 6;
    }
    func_acropolis_bridge_8017E60C(work->field_4, 0);
    if (func_acropolis_bridge_8017F6D4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
    } else {
        prompt->mode = 1;
    }
}

void func_acropolis_bridge_8017F658(Task* task)
{
    Display_ReleaseRef();
    func_acropolis_bridge_8017E60C(0xFFF, 0);
    Task_Kill((Task*)task->spawnArg2);
    Task_RequestKill(task, D_acropolis_bridge_801917A8);
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    D_80114D08             = 0xA;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F6D4);

/// Nine-state dispatcher of this room's script task: copies the handler table
/// out of the overlay's rodata onto the stack and tails into the entry named by
/// `Task::state`.
void func_acropolis_bridge_8017F788(Task* task)
{
    TaskFuncTable9 states;

    states = D_acropolis_bridge_8017D614;
    states.funcs[task->state](task);
}
