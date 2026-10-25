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

#include "actors/actor_451100.h"

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
// The wave task and its persistent storage occupy separate translation units.
/// Binds the scripted-wave renderer to this overlay's writable borrowed-context slot.
///
/// The slot is defined in neo_ark_submarine_tunnel_2.c and declared in the
/// private header. The renderer borrows `gScreenWaveSpawnCtx`, which event
/// requests steer directly. Keep this binding through the per-frame fragment.
#define SCREEN_WAVE_ACTIVE_CONTEXT (gScreenWaveContext)
/// Writable column waves for the tunnel's scripted horizontal screen ripple.
///
/// Binds the per-frame renderer to `gScreenWaveColumnWaves`, the
/// `ScreenWaveOscillator[13]` defined in neo_ark_submarine_tunnel_2.c.
/// Only entries 0..10 are seeded, advanced and sampled; entries 11..12 are
/// outside the mesh. Keep the binding through the per-frame fragment and the
/// overlay loaded through the wave's final drawing tick. Only one wave may
/// use this storage at a time.
#define SCREEN_WAVE_COLUMN_OSCILLATORS (gScreenWaveColumnWaves)
/// Writable row waves for the tunnel's scripted vertical screen ripple.
///
/// Binds the per-frame renderer to the overlay-private `gScreenWaveRowWaves`,
/// a `ScreenWaveOscillator[32]` defined in neo_ark_submarine_tunnel_2.c.
/// Entries 0..29 control the horizontal mesh edges below the pinned top edge;
/// entries 30..31 are outside the active mesh. Every drawing tick advances
/// the active phases, including while actors are frozen.
/// Keep this binding through the per-frame fragment and the overlay loaded
/// through the final drawing tick. Only one wave may use this storage at a time.
#define SCREEN_WAVE_ROW_OSCILLATORS (gScreenWaveRowWaves)
#include "../../shared/screen/screen_wave.h"
#include "../../shared/effects/water_effects.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

static s32 _gScreenWaveAmplitude;

static void _neoArkSubmarineTunnelInitializeRoom(Task* task);
static void _neoArkSubmarineTunnelMessageTaskIdle(Task* task);

static s32 _neoArkSubmarineTunnelHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _neoArkSubmarineTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _neoArkSubmarineTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _neoArkSubmarineTunnelHandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg);

static AnimationSet _gNeoArkSubmarineTunnelAnimation03EE0;
static AnimationSet _gNeoArkSubmarineTunnelAnimation0444C;

static void _neoArkSubmarineTunnelSetEventSeen(s32 seenValue);

enum { NEO_ARK_SUBMARINE_TUNNEL_EVENT_SEEN = 1 };

TaskDesc gNeoArkSubmarineTunnelWaterRefractionTaskDesc[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, waterRefractionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, waterDistortBandTask, { .value = 0 } },
};

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

/// Bodyless screen-wave recipe in slot 0, followed by TASK_DESC_END.
///
/// Runs at priority `TASK_PRIORITY_CONTROL` and borrows a ScreenWaveCtx through spawnArg2.
/// Only one wave may be active; its context and this carrier's code/storage
/// must remain live through the final draw. The spawner copies the recipe.
static TaskDesc _gScreenWaveTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Current sine-displacement strength, recomputed as frame * scale / span.
///
/// Strength 32 corresponds to one pixel at the sine peak. The seed tick resets
/// the initial 256 to zero; the context must have a positive span while drawing.
static s32 _gScreenWaveAmplitude = 256;

/// Tunnel room-message routes for transitions, key-item refusal, actions and CAP commands.
///
/// Four native-payload handlers followed by TASK_MESSAGE_TABLE_END. The room
/// controller borrows the table and callback code until teardown.
static const TaskMessageEntry _gNeoArkSubmarineTunnelRoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkSubmarineTunnelResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkSubmarineTunnelRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkSubmarineTunnelHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkSubmarineTunnelHandleCapCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Two player encounter clips copied into weapon-bank extension slots 47/48.
///
/// This is a fixed two-word transfer, not a terminated table. The scene plays
/// the second clip before the first; the copied pointers and clip data remain
/// borrowed until the player returns to its ordinary animation bank.
static AnimationSet* _gNeoArkSubmarineTunnelEncounterAnimationSets[2] = {
    &_gNeoArkSubmarineTunnelAnimation0444C,
    &_gNeoArkSubmarineTunnelAnimation03EE0,
};

/// Copies both encounter clip pointers into the player's equipped-bank extension.
///
/// Exactly two words populate slots 47/48. The script borrows this request
/// through dispatch; copied pointers and room clip data remain live through playback.
static AnimationBankCopyRequest _gNeoArkSubmarineTunnelEncounterAnimationBankCopy = {
    .sets  = _gNeoArkSubmarineTunnelEncounterAnimationSets,
    .count = ARRAY_SIZE(_gNeoArkSubmarineTunnelEncounterAnimationSets),
};

enum {
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_REQUEST_BANK      = 1,
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_PLAYER_CLIP_47    = ANIMATION_BANK_BASE_SET_COUNT,
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_PLAYER_CLIP_48    = ANIMATION_BANK_BASE_SET_COUNT + 1,
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_BLEND_FRAMES      = 8,
    NEO_ARK_SUBMARINE_TUNNEL_DIVER_COMMAND_SWIM          = 1,
    NEO_ARK_SUBMARINE_TUNNEL_DIVER_COMMAND_PATROL        = 2,
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_PLAYER_RESET_CLIP = 1,
};

/// Later player clip request for the Diver encounter, interpolating extension clip 47.
///
/// Eight normal-rate blend frames; grid collision is disabled. The script copies
/// the request and resolves the equipped bank, ignoring the stored seed.
/// The borrowed extension clips must remain loaded through playback.
static AnimationPlayRequest _gNeoArkSubmarineTunnelEncounterPlayerClip47Request = {
    { .index = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_REQUEST_BANK },
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_PLAYER_CLIP_47,
    ANIMATION_BLEND_INTERPOLATE,
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_BLEND_FRAMES,
    ANIMATION_WORLD_COLLISION_DISABLE,
};

/// First player clip request for the Diver encounter, interpolating extension clip 48.
///
/// Eight normal-rate blend frames; grid collision is disabled. The script copies
/// the request and resolves the equipped bank, ignoring the stored seed.
/// The borrowed extension clips must remain loaded through playback.
static AnimationPlayRequest _gNeoArkSubmarineTunnelEncounterPlayerClip48Request = {
    { .index = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_REQUEST_BANK },
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_PLAYER_CLIP_48,
    ANIMATION_BLEND_INTERPOLATE,
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_BLEND_FRAMES,
    ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced command 0 in the tunnel actor-command namespace; intended action unproven.
static ActorCommand D_neo_ark_submarine_tunnel_80181AB0 = { { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL } }, 0 };

/// Releases the Bog Divers' tunnel-introduction wait into their timed forward swim.
///
/// The encounter script broadcasts this room-context command synchronously.
/// Divers latch command 1 without inspecting context; payload storage is not retained.
static ActorCommand _gNeoArkSubmarineTunnelDiverSwimCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL } },
    .command = NEO_ARK_SUBMARINE_TUNNEL_DIVER_COMMAND_SWIM,
};

/// Releases the Bog Divers from the introduction into their waypoint patrol.
///
/// Broadcast synchronously in the tunnel namespace. Divers latch command 2
/// without reading context and retain no pointer to this record.
static ActorCommand _gNeoArkSubmarineTunnelDiverPatrolCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL } },
    .command = NEO_ARK_SUBMARINE_TUNNEL_DIVER_COMMAND_PATROL,
};

/// Resets player clip 1 before and after the tunnel's Diver encounter clips.
///
/// No blend frames; grid collision stays disabled. The script copies this
/// request and resolves the equipped weapon bank before synchronous dispatch.
static AnimationPlayRequest _gNeoArkSubmarineTunnelEncounterPlayerResetRequest = {
    .source               = { .index = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_REQUEST_BANK },
    .animationId          = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_PLAYER_RESET_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player placement during the Diver introduction, in room-coordinate units.
///
/// Position is (4544, 3001, 0), facing -X at -1024 units of yaw (4096 per turn).
/// The player copies position and rotation during synchronous script dispatch.
static ActorTransform _gNeoArkSubmarineTunnelEncounterPlayerPlacement = { { 4544, 3001, 0, 0 }, { 0, -1024, 0, 0 } };

/// Stream selection key for the tunnel's Diver introduction: group 5, stream 60, qualifier 11.
///
/// Group is a stream-table selector, not a stage ID. The script and scene audio
/// borrow this key until the selected scene is finished.
static EvsSceneKey _gNeoArkSubmarineTunnelDiverEncounterSceneKey = { 5, 60, 11 };

enum {
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_CAP_COMMAND = 1,
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_RESUME_VIEW = 2,
    NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_FADE_FRAMES = 30,
};

/// Thirty-one instructions and END for the tunnel's first Diver encounter.
///
/// Variant 1/action 2 starts it while the seen flag is clear. It installs player
/// clips, coordinates CAP/audio, water distortion and Diver swim/patrol cues,
/// restores play and records the seen flag. Script and referenced room records
/// stay loaded until completion; waits are measured in task frames or CAP cues.
static EvsCommand _gNeoArkSubmarineTunnelDiverEncounterScript[32] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gNeoArkSubmarineTunnelDiverEncounterSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gNeoArkSubmarineTunnelEncounterAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gNeoArkSubmarineTunnelEncounterPlayerResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gNeoArkSubmarineTunnelEncounterPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _screenWaveRun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gNeoArkSubmarineTunnelEncounterPlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gNeoArkSubmarineTunnelDiverSwimCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_SET_RATE }, { .value = ANIMATION_RATE_ONE / 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gNeoArkSubmarineTunnelEncounterPlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_RESUME_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gNeoArkSubmarineTunnelDiverPatrolCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gNeoArkSubmarineTunnelEncounterPlayerResetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _screenWaveRun }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = NEO_ARK_SUBMARINE_TUNNEL_ENCOUNTER_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _neoArkSubmarineTunnelSetEventSeen }, { .value = NEO_ARK_SUBMARINE_TUNNEL_EVENT_SEEN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/effects/water.inc.c"

/// Room-controller states 0 initialize, 1 idle, 2 teardown.
///
/// The controller copies all three callbacks before unchecked state dispatch.
/// There is no terminator; keep the room overlay loaded while it is live.
static const TaskFuncTable3 _gNeoArkSubmarineTunnelRoomStateHandlers = {
    { _neoArkSubmarineTunnelInitializeRoom, _neoArkSubmarineTunnelMessageTaskIdle, taskKill }
};

#include "../../shared/screen/screen_wave.inc.c"

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
    if ((actionId == ACTION_ADVANCE_PROGRESS) && (gameFlagGetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS) == actionId) && (gGameSession->location.variant == PROGRESS_VARIANT)) {
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, OBJECTIVE_AFTER_PROGRESS);
        gameFlagSetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS, PROGRESS_ADVANCED);
        gameFlagSetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE, SCENE_MUSIC_OVERRIDE_ENABLED);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = PROGRESS_SCENE_EVENT;
        evsStartScriptWithSkip(gActor451100SubmarineTunnelSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor451100SubmarineTunnelSceneSkipScript);
    }
    // Scenes claim the entry latch before the fallback arrival-control release.
    if ((request->actionId == ACTION_EVENT_OR_WARP_1) && (gameFlagGetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) == 0)) {
        eventVariant = gGameSession->location.variant;
        if (eventVariant == EVENT_VARIANT) {
            evsStartScript(_gNeoArkSubmarineTunnelDiverEncounterScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gNeoArkSubmarineTunnelEntryActionHandled = eventVariant;
        }
    }
    actionId = request->actionId;
    if ((actionId == ACTION_ENTRY_OR_WARP_2) && (gNeoArkSubmarineTunnelEntryActionHandled == ENTRY_UNHANDLED) && (gGameSession->location.warp == ENTRY_WARP_2) && (gameFlagGetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS) == PROGRESS_NOT_STARTED) && (gGameSession->location.variant == actionId)) {
        gameFlagSetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS, PROGRESS_ENTERED);
        evsStartScript(gActor451100SubmarineTunnelEntryScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        gNeoArkSubmarineTunnelEntryActionHandled = ENTRY_HANDLED;
    }
    if ((request->actionId == ACTION_EVENT_OR_WARP_1) && (gNeoArkSubmarineTunnelEntryActionHandled == ENTRY_UNHANDLED)) {
        entryWarp = gGameSession->location.warp;
        if (entryWarp == ENTRY_WARP_1) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            gNeoArkSubmarineTunnelEntryActionHandled = entryWarp;
        }
    }
    if ((request->actionId == ACTION_ENTRY_OR_WARP_2) && (gNeoArkSubmarineTunnelEntryActionHandled == ENTRY_UNHANDLED) && (gGameSession->location.warp == ENTRY_WARP_2)) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        gNeoArkSubmarineTunnelEntryActionHandled = ENTRY_HANDLED;
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
            if (gGameSession->location.variant == CAP_VARIANT) {
                capSpawnEventIfIdle(commandIndex, CAP_EVENT_NO_FLAGS);
            }
        }
    }
    return 0;
}

#include "../../shared/screen/screen_wave_run.inc.c"

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
    task->msgTable = _gNeoArkSubmarineTunnelRoomMessages;
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

    stateHandlers = _gNeoArkSubmarineTunnelRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
