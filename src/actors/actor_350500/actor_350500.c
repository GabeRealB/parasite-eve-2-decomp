#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor/actor_motion.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/walk/reversing_walker.h"

enum {
    ACTOR_350500_REVERSE_WALK_TASK_SPAWN  = 0,
    ACTOR_350500_REVERSE_WALK_TASK_UPDATE = 1,
    ACTOR_350500_REVERSE_WALK_TASK_EXIT   = 2,
};

/// Reversing walker lifecycle handlers: 0 spawn, 1 update, 2 exit.
///
/// Copied by value and indexed by `Task::state` in 0..2, without a terminator
/// or bounds check. Callbacks require a live task; update requires setup
/// work, and teardown may release both the task and its owned `Enemy`.
/// Actor-control holds suspend dispatch of all three slots.
static const TaskFuncTable3 _gActor350500ReverseWalkTaskStates = {
    .funcs = {
        [ACTOR_350500_REVERSE_WALK_TASK_SPAWN]  = _reverseWalkSpawn,
        [ACTOR_350500_REVERSE_WALK_TASK_UPDATE] = _reverseWalkUpdate,
        [ACTOR_350500_REVERSE_WALK_TASK_EXIT]   = _reverseWalkExit,
    },
};

/// Scripted walk phases indexed by `ReverseWalkWork::walk.motionStep` (0..3).
///
/// Face the target, begin moving, approach until arrival, then turn to the
/// destination yaw. Every entry is a `void(Task*)` callback; there is no terminator.
static const TaskFuncTable4 _gReverseWalkStepHandlers = { {
    _reverseWalkOrientForWalk,
    _reverseWalkBeginMove,
    _actorMotionArrive19,
    _reverseWalkTurnToYaw,
} };

/// Local +Z velocity of a forward reversing-walker step.
///
/// Signed 16.16: 32 parent-coordinate units per tick before root rotation.
/// Backward movement scales it by -0.4 before rotating into the parent frame.
static const VECTOR _gReverseWalkForwardVelocity = { 0, 0, 0x200000, 0 };

extern TmdSource gEveMaskedBody;
static s32       _actor350500ReverseWalkCommandMsg(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void      _actor350500ReverseWalkTask(Task* task);

#include "assets/characters/eve/masked_body.inc"

#include "assets/rooms/shelter/b6_corridor/eve_clips.inc"

/// Masked Eve's bank-0 body animation sets: clip 0 absent, clips 1..4 loaded.
///
/// `AnimationPlayRequest::animationId` selects the clip after `source.index`
/// selects the outer bank. Reversing-walker playback borrows the table and
/// sets for the body task's lifetime; the package must stay loaded.
#include "assets/rooms/shelter/b6_corridor/eve_bank.inc"

/// Carrier-local banks for nineteen-part playback: only bank 0, clips 1..4.
///
/// `AnimationPlayRequest::source.index` selects the bank; `animationId` selects
/// an entry of its clip table. Playback borrows that table and loaded clip data.
static AnimationSet** const _gActorMotionAnimBanks19[1] SECTION(".data") = {
    _gActor350500BodyAnimSets,
};

TaskDesc gActor350500MaskedEveReverseWalkTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor350500ReverseWalkTask, { .model = &gEveMaskedBody } };

/// Terminated message table of this carrier's nineteen-part reversing walker.
///
/// Animation, placement, drawing and walk-to use shared handlers; the
/// direction command is package-specific. The initialized task borrows
/// this table and its callbacks until teardown.
static TaskMessageEntry _gReverseWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _reverseWalkSetDrawModeMsg },
    { ACTOR_MESSAGE_WALK_TO, _reverseWalkStartWalkMsg },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor350500ReverseWalkCommandMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};
#include "../../shared/walk/reversing_walker_update.inc.c"

#include "../../shared/actor/actor_motion_arrive19.inc.c"

#include "../../shared/walk/reversing_walker_start.inc.c"

/// Runs actor 350500's reversing-walker task state while actor control is running.
///
/// Requires a live TMD task with an owned enemy in `spawnArg2.pointer` and
/// `state` in 0..2 (0 spawn, 1 update, 2 exit); the index is unchecked.
/// Spawn initializes the owned work before updates can run. Spawn failure or
/// exit can release the task, so do not access it after dispatch.
/// Any actor-control value other than running skips all three states.
static void _actor350500ReverseWalkTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = _gActor350500ReverseWalkTaskStates;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

#include "../../shared/walk/reversing_walker.inc.c"

#include "../../shared/actor/actor_motion_play19.inc.c"

#include "../../shared/actor/actor_messages_place_euler.inc.c"

#include "../../shared/walk/reversing_walker_visibility.inc.c"

/// Selects whether subsequent walk setup approaches targets forward or backward.
///
/// Requires initialized `ReverseWalkWork` and a borrowed, readable command.
/// Command 1 selects backing, 2 selects forward walking; other commands leave
/// the direction unchanged. Ignores the context, message ID and second payload.
/// Retains no command pointer and always returns 0. Existing velocity is intact.
static s32 _actor350500ReverseWalkCommandMsg(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_350500_REVERSE_WALK_COMMAND_BACKWARD = 1,
        ACTOR_350500_REVERSE_WALK_COMMAND_FORWARD  = 2,
    };
    ReverseWalkWork* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_350500_REVERSE_WALK_COMMAND_BACKWARD:
            work->walksForward = 0;
            break;
        case ACTOR_350500_REVERSE_WALK_COMMAND_FORWARD:
            work->walksForward = 1;
            break;
    }
    return 0;
}
