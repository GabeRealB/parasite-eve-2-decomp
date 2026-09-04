#include "common.h"

#include "gameplay/3688.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s16 D_80114D08;
extern s32 D_acropolis_bridge_801917A8;

void func_acropolis_bridge_8017E60C(s32 arg0, s32 arg1);

/// Work block this room's script tasks keep at `Task::idMap`. `field_4` is the
/// script step handed to `func_acropolis_bridge_8017E60C` and `promptKind` the
/// display mode forwarded to `func_800D4E78`.
typedef struct AcropolisBridgePromptWork {
    /* 0x00 */ byte pad_0[0x4];
    /* 0x04 */ s16  field_4;
    /* 0x06 */ byte pad_6[0x8];
    /* 0x0E */ s8   promptKind;
} AcropolisBridgePromptWork;

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F280);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F2D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F358);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F404);

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F4CC);

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_7", func_acropolis_bridge_8017F788);
