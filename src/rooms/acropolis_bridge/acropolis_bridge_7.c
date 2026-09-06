#include "common.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/acropolis_bridge.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ed38.h"

extern s16            D_80114D08;
extern TaskFuncTable9 D_acropolis_bridge_8017D614;

s32 RoomsShared8017ecb4(RoomHotspot* table, s16 x, s16 y);

/// Two-state dispatcher of this room's prompt script task: builds the handler
/// table on the stack and tails into the entry named by `Task::state`.
void func_acropolis_bridge_8017F280(Task* task)
{
    TaskFunc states[2] = { Room_Util04, RoomsShared8017ed38 };

    states[task->state](task);
}

/// Repaints the two bridge sprites that game flag nibble 0x10 governs: one
/// sprite command in view 2 of this room's sprite record and one in view 5.
/// `Gp_LinkViewSprts` reads `field_4` to decide whether to skip OT-linking a
/// command's prims, so a zero nibble draws both and a non-zero one hides them.
void func_acropolis_bridge_8017F2D0(s32 flags)
{
    GameSession*      g    = Game_Session;
    GameSessionFrom4* sess = (GameSessionFrom4*)&g->field_4;
    GpSprtRec*        rec;
    GpSprtCmd*        cmd;

    rec = Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1];

    cmd = rec[1].field_4;
    if ((flags & 0xFF) == 0) {
        cmd[11].field_4 = 0;
    } else {
        cmd[11].field_4 = 1;
    }

    cmd = rec[4].field_4;
    if ((flags & 0xFF) == 0) {
        cmd[16].field_4 = 0;
    } else {
        cmd[16].field_4 = 1;
    }
}

/// Picks which of three mutually exclusive bridge sprites view 9 of this room
/// draws. `Gp_LinkViewSprts` treats a nonzero `field_4` as "skip OT-linking",
/// so the selected command gets 0 and the other two get 1; a state outside
/// 0..2 hides all three.
void func_acropolis_bridge_8017F358(s32 state)
{
    GameSession*      g    = Game_Session;
    GameSessionFrom4* sess = (GameSessionFrom4*)&g->field_4;
    GpSprtRec*        rec;
    GpSprtCmd*        cmd;
    s32               mode;

    rec  = Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1];
    cmd  = rec[9].field_4;
    mode = state & 0xFF;

    if (mode == 0) {
        cmd[1].field_4 = 0;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    } else if (mode == 1) {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 0;
        cmd[3].field_4 = 1;
    } else if (mode == 2) {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 0;
    } else {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    }
}

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
    RoomHotspot*               hs     = D_acropolis_bridge_8018983C;

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
    if (RoomsShared8017ecb4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
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
