#include "rooms/neo_ark_submarine_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_submarine_tunnel_private.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"
#include "../../shared/screen_wave.h"
#include "../../shared/water_effects.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// The area-record id the event handler publishes, and the event scripts
/// `evsStartScriptWithSkip` / `evsStartScript` are handed.
extern EvsCommand D_actor_451100_80135220[];
extern EvsCommand D_actor_451100_80135FD0[];
extern EvsCommand D_actor_451100_80136108[];

/// Spawn table of the screen-wave task, and the context it is spawned with.
/// The context's mode word is written through its own symbol, which is how the
/// original reached it.
extern TaskDesc gScreenWaveTaskDesc[];

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 gScreenWaveRamp;

/// Message handlers this room's task answers, installed into pointer slot 7.
extern TaskMessageEntry D_neo_ark_submarine_tunnel_80181A50[];

/// The tunnel's own script blob and the byte recording which of its scenes has
/// already been staged.
extern EvsCommand D_neo_ark_submarine_tunnel_80181AF0[];

static void _neoArkSubmarineTunnelInitializeRoom(Task* task);
static void _neoArkSubmarineTunnelMessageTaskIdle(Task* task);

static s32 _neoArkSubmarineTunnelHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _neoArkSubmarineTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _neoArkSubmarineTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _neoArkSubmarineTunnelHandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg);

static AnimationSet _gNeoArkSubmarineTunnelAnimation03EE0;
static AnimationSet _gNeoArkSubmarineTunnelAnimation0444C;

extern AnimationPlayRequest     D_neo_ark_submarine_tunnel_80181A88;
extern AnimationPlayRequest     D_neo_ark_submarine_tunnel_80181A9C;
extern AnimationBankCopyRequest D_neo_ark_submarine_tunnel_80181A80;
static void                     _neoArkSubmarineTunnelSetEventSeen(s32 seenValue);

enum { NEO_ARK_SUBMARINE_TUNNEL_EVENT_SEEN = 1 };

TaskDesc D_neo_ark_submarine_tunnel_801810E4 = { { { TASK_BODY_NONE, 192 } }, waterRefractionTask, { .value = 0 } };

TaskDesc D_neo_ark_submarine_tunnel_801810F0 = { { { TASK_BODY_NONE, 192 } }, waterDistortBandTask, { .value = 0 } };

static AnimationPackedPose _gNeoArkSubmarineTunnelAnimation03EE0Bank1[6] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_bank1.inc"
};

static AnimationPackedRotation _gNeoArkSubmarineTunnelAnimation03EE0Bank4[64] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_bank4.inc"
};

static AnimationRecord _gNeoArkSubmarineTunnelAnimation03EE0Records[141] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_records.inc"
};

static u16 _gNeoArkSubmarineTunnelAnimation03EE0Indices[20] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_indices.inc"
};

static AnimationSet _gNeoArkSubmarineTunnelAnimation03EE0 = {
    _gNeoArkSubmarineTunnelAnimation03EE0Records,
    _gNeoArkSubmarineTunnelAnimation03EE0Indices,
    { NULL, _gNeoArkSubmarineTunnelAnimation03EE0Bank1, NULL, NULL, _gNeoArkSubmarineTunnelAnimation03EE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gNeoArkSubmarineTunnelAnimation0444CBank1[10] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_bank1.inc"
};

static AnimationPackedRotation _gNeoArkSubmarineTunnelAnimation0444CBank4[126] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_bank4.inc"
};

static AnimationRecord _gNeoArkSubmarineTunnelAnimation0444CRecords[171] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_records.inc"
};

static u16 _gNeoArkSubmarineTunnelAnimation0444CIndices[20] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_indices.inc"
};

static AnimationSet _gNeoArkSubmarineTunnelAnimation0444C = {
    _gNeoArkSubmarineTunnelAnimation0444CRecords,
    _gNeoArkSubmarineTunnelAnimation0444CIndices,
    { NULL, _gNeoArkSubmarineTunnelAnimation0444CBank1, NULL, NULL, _gNeoArkSubmarineTunnelAnimation0444CBank4, NULL, NULL, NULL },
};

TaskDesc gScreenWaveTaskDesc[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskMessageEntry D_neo_ark_submarine_tunnel_80181A50[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkSubmarineTunnelResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkSubmarineTunnelRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkSubmarineTunnelHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkSubmarineTunnelHandleCapCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_neo_ark_submarine_tunnel_80181A78[2] = {
    &_gNeoArkSubmarineTunnelAnimation0444C,
    &_gNeoArkSubmarineTunnelAnimation03EE0,
};

AnimationBankCopyRequest D_neo_ark_submarine_tunnel_80181A80 = { { .sets = D_neo_ark_submarine_tunnel_80181A78 }, ARRAY_SIZE(D_neo_ark_submarine_tunnel_80181A78) };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181A88 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181A9C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

ActorCommand D_neo_ark_submarine_tunnel_80181AB0 = { { .loc = { 5, 12 } }, 0 };

ActorCommand D_neo_ark_submarine_tunnel_80181AB4 = { { .loc = { 5, 12 } }, 1 };

ActorCommand D_neo_ark_submarine_tunnel_80181AB8 = { { .loc = { 5, 12 } }, 2 };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181ABC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_neo_ark_submarine_tunnel_80181AD0 = { { 4544, 3001, 0, 0 }, { 0, -1024, 0, 0 } };

EvsSceneKey D_neo_ark_submarine_tunnel_80181AE8 = { 5, 60, 11 };

EvsCommand D_neo_ark_submarine_tunnel_80181AF0[32] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_neo_ark_submarine_tunnel_80181AE8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_neo_ark_submarine_tunnel_80181A80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181ABC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_neo_ark_submarine_tunnel_80181AD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _screenWaveRun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181A9C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_neo_ark_submarine_tunnel_80181AB4 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181A88 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_neo_ark_submarine_tunnel_80181AB8 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181ABC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _screenWaveRun }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _neoArkSubmarineTunnelSetEventSeen }, { .value = NEO_ARK_SUBMARINE_TUNNEL_EVENT_SEEN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

/// State handlers of the room task `neoArkSubmarineTunnelRoomTask`
/// runs: `_neoArkSubmarineTunnelInitializeRoom` sets it up,
/// `_neoArkSubmarineTunnelMessageTaskIdle` runs every later tick, and
/// `taskKill` ends it.
static const TaskFuncTable3 D_neo_ark_submarine_tunnel_8017D614 = {
    { _neoArkSubmarineTunnelInitializeRoom, _neoArkSubmarineTunnelMessageTaskIdle, taskKill }
};

#include "../../shared/screen_wave.inc.c"

/// Handles trigger-driven tunnel scenes and arrival-control release.
///
/// Borrows a four-byte `DirectionActionRequest` during synchronous dispatch.
/// Action 1 advances variant-3 progress from 1 to 2 and starts its scene.
/// Action 2 starts the unseen variant-1 event or releases warp-1 arrival control;
/// action 3 starts first-entry variant-3 progress or releases warp-2 arrival
/// control. The room's entry latch suppresses repeated entry/control handling.
/// Only the unsigned action byte is read; control, argument and other callback
/// words are ignored. Retains no request pointer and always returns zero.
static s32 _neoArkSubmarineTunnelHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { ACTION_ADVANCE_PROGRESS      = 1,
           ACTION_EVENT_OR_WARP_1       = 2,
           ACTION_ENTRY_OR_WARP_2       = 3,
           PROGRESS_NOT_STARTED         = 0,
           PROGRESS_ENTERED             = 1,
           PROGRESS_ADVANCED            = 2,
           EVENT_VARIANT                = 1,
           PROGRESS_VARIANT             = 3,
           ENTRY_WARP_1                 = 1,
           ENTRY_WARP_2                 = 2,
           ENTRY_UNHANDLED              = 0,
           ENTRY_HANDLED                = 1,
           PROGRESS_SCENE_EVENT         = 0x1A,
           OBJECTIVE_AFTER_PROGRESS     = 0x35,
           SCENE_MUSIC_OVERRIDE_ENABLED = 1 };
    u8 actionId;
    u8 eventVariant;
    u8 entryWarp;

    // Advance the staged variant-3 story without changing the entry latch.
    actionId = request->actionId;
    if ((actionId == ACTION_ADVANCE_PROGRESS) && (gameFlagGetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS) == actionId) && (gGameSession->location.loc.variant == PROGRESS_VARIANT)) {
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, OBJECTIVE_AFTER_PROGRESS);
        gameFlagSetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS, PROGRESS_ADVANCED);
        gameFlagSetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE, SCENE_MUSIC_OVERRIDE_ENABLED);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = PROGRESS_SCENE_EVENT;
        evsStartScriptWithSkip(D_actor_451100_80135220, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_451100_80135FD0);
    }
    // Scenes claim the entry latch before the fallback arrival-control release.
    if ((request->actionId == ACTION_EVENT_OR_WARP_1) && (gameFlagGetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) == 0)) {
        eventVariant = gGameSession->location.loc.variant;
        if (eventVariant == EVENT_VARIANT) {
            evsStartScript(D_neo_ark_submarine_tunnel_80181AF0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            D_neo_ark_submarine_tunnel_80181DF0 = eventVariant;
        }
    }
    actionId = request->actionId;
    if ((actionId == ACTION_ENTRY_OR_WARP_2) && (D_neo_ark_submarine_tunnel_80181DF0 == ENTRY_UNHANDLED) && (gGameSession->location.loc.warp == ENTRY_WARP_2) && (gameFlagGetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS) == PROGRESS_NOT_STARTED) && (gGameSession->location.loc.variant == actionId)) {
        gameFlagSetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS, PROGRESS_ENTERED);
        evsStartScript(D_actor_451100_80136108, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        D_neo_ark_submarine_tunnel_80181DF0 = ENTRY_HANDLED;
    }
    if ((request->actionId == ACTION_EVENT_OR_WARP_1) && (D_neo_ark_submarine_tunnel_80181DF0 == ENTRY_UNHANDLED)) {
        entryWarp = gGameSession->location.loc.warp;
        if (entryWarp == ENTRY_WARP_1) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            D_neo_ark_submarine_tunnel_80181DF0 = entryWarp;
        }
    }
    if ((request->actionId == ACTION_ENTRY_OR_WARP_2) && (D_neo_ark_submarine_tunnel_80181DF0 == ENTRY_UNHANDLED) && (gGameSession->location.loc.warp == ENTRY_WARP_2)) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        D_neo_ark_submarine_tunnel_80181DF0 = ENTRY_HANDLED;
    }
    return 0;
}

/// Refuses every key-item use with the item menu's cannot-use reply.
///
/// `itemId` is the selected inventory item ID; all arguments are ignored.
static s32 _neoArkSubmarineTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a Neo Ark room transition after resolving its destination variant.
///
/// Borrows a complete request and writable reply through synchronous dispatch;
/// they may alias. Copies all eight bytes before resolving the reply's room.
/// Query mode preserves the requested selectors. Neither pointer is retained;
/// the map overlay must be loaded. Always returns 1; the task and ID are unused.
static s32 _neoArkSubmarineTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_SUBMARINE_TUNNEL_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return NEO_ARK_SUBMARINE_TUNNEL_TRANSITION_ALLOWED;
}

/// Queues tunnel CAP commands 4 and 5 only in variant 1 and while CAP is idle.
///
/// Handles `ROOM_MESSAGE_COMMAND` and returns zero for every input. The signed
/// command index is forwarded unchanged; receiver, message ID and final word
/// are unused. Keep the selected CAP resources loaded through event playback.
static s32 _neoArkSubmarineTunnelHandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg)
{
    enum { CAP_COMMAND_FIRST = 4,
           CAP_COMMAND_END   = 6,
           CAP_VARIANT       = 1 };

    if (commandIndex < CAP_COMMAND_END) {
        if (commandIndex >= CAP_COMMAND_FIRST) {
            if (gGameSession->location.loc.variant == CAP_VARIANT) {
                capSpawnEventIfIdle(commandIndex, CAP_EVENT_NO_FLAGS);
            }
        }
    }
    return 0;
}

#include "../../shared/screen_wave_run.inc.c"

/// Stores the tunnel event's seen value in the live save's game-flag nibble.
///
/// The event script passes 1 at completion. The callback accepts a full signed
/// word; the flag writer stores its low four bits (0 clear, 1 seen).
static void _neoArkSubmarineTunnelSetEventSeen(s32 seenValue)
{
    gameFlagSetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN, seenValue);
}

/// Installs the tunnel message receiver and starts its ambience script.
///
/// Called in state 0 with a live room task and initialized gameplay resources.
/// Registers the borrowed task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _neoArkSubmarineTunnelInitializeRoom(Task* task)
{
    task->msgTable = D_neo_ark_submarine_tunnel_80181A50;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_SUBMARINE_TUNNEL_AMBIENCE, 0, 0);
    task->state = task->state + 1;
}

/// Keeps the initialized room-message task idle between incoming messages.
///
/// The player-slot lookup has no result consumer, but remains part of this tick.
static void _neoArkSubmarineTunnelMessageTaskIdle(Task* task)
{
    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
}

void neoArkSubmarineTunnelRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_neo_ark_submarine_tunnel_8017D614;
    stateHandlers.funcs[task->state](task);
}
