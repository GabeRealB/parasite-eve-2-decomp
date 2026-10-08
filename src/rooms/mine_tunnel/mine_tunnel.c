#include "rooms/mine_tunnel.h"

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_targets.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "../../shared/room_variants.h"

/// Event script started with `evsStartScript`.
extern EvsCommand D_mine_tunnel_8017E024[];

/// The room's message table: 0x13EE is handled by `_mineTunnelResolveRoomTransition`,
/// 0x13F1 by `_mineTunnelRejectKeyItemUse`, 0x13EF by `_mineTunnelHandleDirectionAction`
/// and 0x13F0 by `_mineTunnelHandleCommand`.
extern TaskMessageEntry D_mine_tunnel_8017DFC4[];

static s32 _mineTunnelRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _mineTunnelResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _mineTunnelHandleCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32 _mineTunnelHandleDirectionAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

static AnimationSet _gMineTunnelAnimation009DC;

extern AnimationPlayRequest     D_mine_tunnel_8017DFFC;
extern AnimationBankCopyRequest D_mine_tunnel_8017DFF4;
static void                     _mineTunnelSetEnemyWave(s32 wavePhase);

/// Scripted enemy activation phases selected by this room.
enum {
    MINE_TUNNEL_ENEMY_WAVE_ENTRANCE = 1,
    MINE_TUNNEL_ENEMY_WAVE_ENGAGE   = 2,
};

static AnimationPackedPose _gMineTunnelAnimation009DCBank1[10] = {
#include "assets/mine_tunnel_animation_009DC_bank1.inc"
};

static AnimationPackedRotation _gMineTunnelAnimation009DCBank4[95] = {
#include "assets/mine_tunnel_animation_009DC_bank4.inc"
};

static AnimationRecord _gMineTunnelAnimation009DCRecords[139] = {
#include "assets/mine_tunnel_animation_009DC_records.inc"
};

static u16 _gMineTunnelAnimation009DCIndices[20] = {
#include "assets/mine_tunnel_animation_009DC_indices.inc"
};

static AnimationSet _gMineTunnelAnimation009DC = {
    _gMineTunnelAnimation009DCRecords,
    _gMineTunnelAnimation009DCIndices,
    { NULL, _gMineTunnelAnimation009DCBank1, NULL, NULL, _gMineTunnelAnimation009DCBank4, NULL, NULL, NULL },
};

TaskMessageEntry D_mine_tunnel_8017DFC4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mineTunnelResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mineTunnelRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mineTunnelHandleDirectionAction },
    { ROOM_MESSAGE_COMMAND, _mineTunnelHandleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_mine_tunnel_8017DFEC[2] = {
    &_gMineTunnelAnimation009DC,
    NULL,
};

AnimationBankCopyRequest D_mine_tunnel_8017DFF4 = { { .sets = D_mine_tunnel_8017DFEC }, ARRAY_SIZE(D_mine_tunnel_8017DFEC) };

AnimationPlayRequest D_mine_tunnel_8017DFFC = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_mine_tunnel_8017E010 = { { .index = 1 }, 7, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_mine_tunnel_8017E024[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_tunnel_8017DFF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineTunnelSetEnemyWave }, { .value = MINE_TUNNEL_ENEMY_WAVE_ENTRANCE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_tunnel_8017DFFC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static void _mineTunnelInitializeRoomTask(Task* task);
static void _mineTunnelIdleRoomTask(Task* unusedTask);

/// Refuses key-item use in the mine tunnel without consuming the selected item.
///
/// `ROOM_MESSAGE_USE_KEY_ITEM` carries the collected item ID and a zero second
/// word. All arguments are unused; the reply makes the item menu show refusal.
static s32 _mineTunnelRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a mine-tunnel departure through the Mine/Shelter room-variant rules.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`, borrowing a readable eight-byte request
/// and writable reply, which may alias. Copies the complete record before
/// resolving it; queries preserve the copied destination. The map_shelter
/// overlay must be loaded, and destination selectors must be valid for that
/// stage. Neither pointer is retained. Always permits ordinary departure.
static s32 _mineTunnelResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Plays the tunnel's progress-dependent CAP reply to room command 2.
///
/// Passage progress below 2 selects command 2, otherwise command 3. Other
/// commands do nothing. Requires loaded CAP commands; receiver, message ID and
/// second word are ignored. Always returns zero.
static s32 _mineTunnelHandleCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        MINE_TUNNEL_COMMAND_PASSAGE_REPLY  = 2,
        MINE_TUNNEL_PASSAGE_PROGRESS_READY = 2,
        MINE_TUNNEL_PASSAGE_REPLY_BEFORE   = 2,
        MINE_TUNNEL_PASSAGE_REPLY_AFTER    = 3
    };

    if (commandId == MINE_TUNNEL_COMMAND_PASSAGE_REPLY) {
        capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) >= MINE_TUNNEL_PASSAGE_PROGRESS_READY ? MINE_TUNNEL_PASSAGE_REPLY_AFTER : MINE_TUNNEL_PASSAGE_REPLY_BEFORE);
    }
    return 0;
}

/// Starts the tunnel's one-time entrance encounter for direction action 1.
///
/// Only variant 1 with an unseen event starts the script. Marks the event
/// before holding player control and requesting playback. Borrows a four-byte
/// direction request through synchronous dispatch; control and argument are
/// ignored, as are task, message ID and the zero second word. Returns zero.
static s32 _mineTunnelHandleDirectionAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        MINE_TUNNEL_ACTION_ENTRANCE_ENCOUNTER = 1,
        MINE_TUNNEL_EVENT_UNSEEN              = 0,
        MINE_TUNNEL_EVENT_SEEN                = 1
    };
    u8 actionId;

    actionId = request->actionId;
    if ((actionId == MINE_TUNNEL_ACTION_ENTRANCE_ENCOUNTER) && (gGameSession->location.loc.variant == actionId) && (gameFlagGetNibble(GAME_FLAG_MINE_TUNNEL_EVENT_SEEN) == MINE_TUNNEL_EVENT_UNSEEN)) {
        gameFlagSetNibble(GAME_FLAG_MINE_TUNNEL_EVENT_SEEN, MINE_TUNNEL_EVENT_SEEN);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        evsStartScript(D_mine_tunnel_8017E024, EVENT_SCRIPT_HUD_KEEP);
    }
    return 0;
}

/// Selects the tunnel's scripted enemy activation phase.
///
/// The event script selects entrance phase 1; later entries select engagement
/// phase 2. Accepts the event callback's raw signed word and stores its low
/// signed byte in scene combat state, without acquiring a battle reference.
static void _mineTunnelSetEnemyWave(s32 wavePhase)
{
    gSceneCombatState.actor01600Wave = wavePhase;
}

/// Registers the tunnel room and restores its previously started enemy wave.
///
/// State 0 publishes the message receiver, restores engagement in variant 1
/// when the entrance event flag is 1, selects countdown-music entry 1 and
/// advances to idle. Requires the room and its message table to stay loaded.
static void _mineTunnelInitializeRoomTask(Task* task)
{
    enum {
        MINE_TUNNEL_ENCOUNTER_VARIANT     = 1,
        MINE_TUNNEL_EVENT_STARTED         = 1,
        MINE_TUNNEL_COUNTDOWN_MUSIC_ENTRY = 1
    };

    task->msgTable = D_mine_tunnel_8017DFC4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == MINE_TUNNEL_ENCOUNTER_VARIANT) && (gameFlagGetNibble(GAME_FLAG_MINE_TUNNEL_EVENT_SEEN) == MINE_TUNNEL_EVENT_STARTED)) {
        _mineTunnelSetEnemyWave(MINE_TUNNEL_ENEMY_WAVE_ENGAGE);
    }
    task->state           = task->state + 1;
    gStageSceneMusicEntry = MINE_TUNNEL_COUNTDOWN_MUSIC_ENTRY;
}

/// Keeps the mine tunnel's registered room task alive in state 1 for messages.
static void _mineTunnelIdleRoomTask(Task* unusedTask)
{
}

/// The room event task's three states: install the message table, idle, and
/// kill.
static const TaskFuncTable3 D_mine_tunnel_8017D5C4 = {
    {
        _mineTunnelInitializeRoomTask,
        _mineTunnelIdleRoomTask,
        taskKill,
    },
};

void mineTunnelRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_mine_tunnel_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
