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
/// 0x13F1 by `_mineTunnelRejectKeyItemUse`, 0x13EF by `func_mine_tunnel_8017D670`
/// and 0x13F0 by `func_mine_tunnel_8017D630`.
extern TaskMessageEntry D_mine_tunnel_8017DFC4[];

static s32 _mineTunnelRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _mineTunnelResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
s32        func_mine_tunnel_8017D630(Task*, s32, s32, s32);
s32        func_mine_tunnel_8017D670(Task*, s32, RoomEventMsg*, s32);

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
    { DIRECTION_MESSAGE_ROOM_ACTION, func_mine_tunnel_8017D670 },
    { ROOM_MESSAGE_COMMAND, func_mine_tunnel_8017D630 },
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

static void func_mine_tunnel_8017D6EC(Task* arg0);
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

s32 func_mine_tunnel_8017D630(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) >= 2 ? 3 : 2);
    }
    return 0;
}

s32 func_mine_tunnel_8017D670(Task* arg0, s32 arg1, RoomEventMsg* msg, s32 arg3)
{
    u8 temp_v1;

    temp_v1 = msg->warp;
    if ((temp_v1 == 1) && (gGameSession->location.loc.variant == temp_v1) && (gameFlagGetNibble(GAME_FLAG_MINE_TUNNEL_EVENT_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_MINE_TUNNEL_EVENT_SEEN, 1);
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

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and - when the session is at place 1
/// and flag 0xA1 is 1 - calls `_mineTunnelSetEnemyWave` with 2. Then sets
/// scene music entry 1 and advances to state 1.
static void func_mine_tunnel_8017D6EC(Task* arg0)
{
    arg0->msgTable = D_mine_tunnel_8017DFC4;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == 1) && (gameFlagGetNibble(GAME_FLAG_MINE_TUNNEL_EVENT_SEEN) == 1)) {
        _mineTunnelSetEnemyWave(MINE_TUNNEL_ENEMY_WAVE_ENGAGE);
    }
    arg0->state           = (s32)(arg0->state + 1);
    gStageSceneMusicEntry = 1;
}

/// Keeps the mine tunnel's registered room task alive in state 1 for messages.
static void _mineTunnelIdleRoomTask(Task* unusedTask)
{
}

/// The room event task's three states: install the message table, idle, and
/// kill.
static const TaskFuncTable3 D_mine_tunnel_8017D5C4 = {
    {
        func_mine_tunnel_8017D6EC,
        _mineTunnelIdleRoomTask,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_mine_tunnel_8017D77C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}
