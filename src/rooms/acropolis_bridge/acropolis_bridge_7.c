#include "common.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
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
    /* 0x06 */ byte pad_6[0x8];
    /* 0x0E */ s8   promptKind;
    /* 0x0F */ s8   promptBusy;
} AcropolisBridgePromptWork;

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F544);

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
