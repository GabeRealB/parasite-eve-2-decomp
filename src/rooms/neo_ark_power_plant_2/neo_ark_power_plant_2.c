#include "rooms/neo_ark_power_plant_2.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_02100.h"
#include "actors/actor_05700.h"
#include "actors/actor_105300.h"
#include "actors/actor_403900.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the
/// GTE flag word. Defined before `glow_draw.h`, which otherwise selects
/// `GlowCentreScratch`.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../shared/effects/glow_draw.h"

static WorldCollisionOccluder           _gNeoArkPowerPlant2Occluders[1];
static const WorldCoordRoomAmbientEntry _gNeoArkPowerPlant2AmbientTable[10];

static void _neoArkPowerPlant2InitializeRoom(Task* task);
static void _neoArkPowerPlant2UpdateRoom(Task* unusedTask);

/// Room-controller callbacks indexed by task state: 0 initialize, 1 update, 2 kill.
static const TaskFuncTable3 _gNeoArkPowerPlant2RoomStateHandlers = {
    { _neoArkPowerPlant2InitializeRoom, _neoArkPowerPlant2UpdateRoom, taskKill },
};

static WorldCollisionGrid    _gNeoArkPowerPlant2CollisionGrid;
static WorldCollisionTrigger _gNeoArkPowerPlant2ViewBoundaryTriggers[8];
static WorldCollisionTrigger _gNeoArkPowerPlant2ActionTriggers[8];
static WorldCoordRoomLights  _gNeoArkPowerPlant2RoomLights;

static s32  _neoArkPowerPlant2RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _neoArkPowerPlant2ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkPowerPlant2HandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg);
static s32  _neoArkPowerPlant2IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _neoArkPowerPlant2PrepareGeneratorClearScene(void);
static void _neoArkPowerPlant2StopSkippedSceneVibration(void);

static AnimationPackedPose _gNeoArkPowerPlant2Animation02A4CBank1[3] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_bank1.inc"
};

static AnimationPackedRotation _gNeoArkPowerPlant2Animation02A4CBank4[28] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_bank4.inc"
};

static AnimationRecord _gNeoArkPowerPlant2Animation02A4CRecords[88] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_records.inc"
};

static u16 _gNeoArkPowerPlant2Animation02A4CIndices[20] = {
#include "assets/neo_ark_power_plant_2_animation_02A4C_indices.inc"
};

static AnimationSet _gNeoArkPowerPlant2Animation02A4C = {
    _gNeoArkPowerPlant2Animation02A4CRecords,
    _gNeoArkPowerPlant2Animation02A4CIndices,
    { NULL, _gNeoArkPowerPlant2Animation02A4CBank1, NULL, NULL, _gNeoArkPowerPlant2Animation02A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gNeoArkPowerPlant2Animation02C10Bank1[2] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_bank1.inc"
};

static AnimationPackedRotation _gNeoArkPowerPlant2Animation02C10Bank4[22] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_bank4.inc"
};

static AnimationRecord _gNeoArkPowerPlant2Animation02C10Records[65] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_records.inc"
};

static u16 _gNeoArkPowerPlant2Animation02C10Indices[20] = {
#include "assets/neo_ark_power_plant_2_animation_02C10_indices.inc"
};

static AnimationSet _gNeoArkPowerPlant2Animation02C10 = {
    _gNeoArkPowerPlant2Animation02C10Records,
    _gNeoArkPowerPlant2Animation02C10Indices,
    { NULL, _gNeoArkPowerPlant2Animation02C10Bank1, NULL, NULL, _gNeoArkPowerPlant2Animation02C10Bank4, NULL, NULL, NULL },
};

/// Room-controller message routes, ending at TASK_MESSAGE_TABLE_END.
///
/// Resolves departures, rejects key items, ignores room actions and selects
/// progress-dependent CAP dialogue. The live room task borrows this table.
static TaskMessageEntry _gNeoArkPowerPlant2RoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkPowerPlant2ResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkPowerPlant2RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkPowerPlant2IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkPowerPlant2HandleCapCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Generator command bits sent by this room's clear scene.
enum {
    NEO_ARK_POWER_PLANT_2_GENERATOR_RELEASE_DEATH       = 1,
    NEO_ARK_POWER_PLANT_2_GENERATOR_RELEASE_BATTLE_EXIT = 2,
};

/// Equipped-weapon idle request used at the start of the generator-clear scene.
///
/// Clip 1 blends over 20 frames with world collision enabled. The interpreter
/// resolves the equipped bank in a copy, so the stored bank selector is unused.
static const AnimationPlayRequest _gNeoArkPowerPlant2GeneratorClearWeaponIdle __attribute__((section(".data"))) = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Releases the defeated generator's death-animation wait at the CAP cue.
static ActorCommand _gNeoArkPowerPlant2GeneratorReleaseDeathCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2 } },
    .command = NEO_ARK_POWER_PLANT_2_GENERATOR_RELEASE_DEATH,
};

/// Releases the generator's battle-exit wait after the scene returns to view 4.
static ActorCommand _gNeoArkPowerPlant2GeneratorReleaseBattleCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2 } },
    .command = NEO_ARK_POWER_PLANT_2_GENERATOR_RELEASE_BATTLE_EXIT,
};

/// Releases both generator waits when the clear scene is skipped.
static ActorCommand _gNeoArkPowerPlant2GeneratorReleaseBothCommand = {
    .context = { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2 } },
    .command = NEO_ARK_POWER_PLANT_2_GENERATOR_RELEASE_DEATH | NEO_ARK_POWER_PLANT_2_GENERATOR_RELEASE_BATTLE_EXIT,
};

/// Player animation sets for the generator-clear scene: empty slot 0, clips 1 and 2.
///
/// Installed as a direct bank by both requests below. The player borrows the
/// table and packed clip data until scripted control restores its weapon bank.
static AnimationSet* _gNeoArkPowerPlant2GeneratorClearAnimationSets[] = {
    NULL,
    &_gNeoArkPowerPlant2Animation02A4C,
    &_gNeoArkPowerPlant2Animation02C10,
};

/// Installs and resets the first player clip in the generator-clear scene.
///
/// World collision stays enabled; the installed animation data remains borrowed
/// from this overlay until the script restores ordinary player control.
static AnimationPlayRequest _gNeoArkPowerPlant2GeneratorClearPlayerAnimation1 = {
    .source               = { .sets = _gNeoArkPowerPlant2GeneratorClearAnimationSets },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Installs and resets the second player clip at the next generator-clear CAP cue.
///
/// Uses the same borrowed direct bank and enabled world collision as clip 1.
static AnimationPlayRequest _gNeoArkPowerPlant2GeneratorClearPlayerAnimation2 = {
    .source               = { .sets = _gNeoArkPowerPlant2GeneratorClearAnimationSets },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player placement shared by normal and skipped generator-clear presentation.
///
/// Position is in world units; Euler angles use 4096 units per turn. Placement
/// consumes the complete transform synchronously while this overlay is loaded.
static ActorTransform _gNeoArkPowerPlant2GeneratorClearPlayerTransform = {
    .pos = { 4000, -5000, -1540, 0 },
    .rot = { 0, -2275, 0, 0 },
};

/// Two-motor vibration loop for the generator-clear scene, ending with both lanes stopped.
///
/// Both lanes visit steps 0..4. Binary vibration uses segment 1 and a one-frame
/// gap; variable vibration uses segment 0 and a five-frame gap. Their loop
/// counters are 40 and 15. The vibration task borrows both arrays until teardown.
static PadScriptCmd _gNeoArkPowerPlant2GeneratorClearVibrationCommands[] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 5) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 40), PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 15) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Generator-clear motor segments: four-frame intensity ramp and two-frame binary hold.
///
/// Variable intensity ramps from 236 to 77 on the 0..255 scale. The binary
/// lane ignores intensity. The retained fourth byte's role is unproven.
static PadScriptVibrationSegment _gNeoArkPowerPlant2GeneratorClearVibrationSegments[] = {
    { .startIntensity = 236, .endIntensity = 77, .durationFrames = 4, .field_3 = 1 },
    { .startIntensity = 0, .endIntensity = 0, .durationFrames = 2, .field_3 = 0 },
};

/// Room-bank sound-script slots started together during generator clearance.
enum {
    NEO_ARK_POWER_PLANT_2_GENERATOR_CLEAR_SOUND_6 = 0x55100006,
    NEO_ARK_POWER_PLANT_2_GENERATOR_CLEAR_SOUND_7 = 0x55100007,
};

/// CAP-synchronized generator-clear presentation, ending at EVENT_SCRIPT_OPCODE_END.
///
/// Cancels effects, starts CAP command 1, hides and replaces the player animation,
/// runs sound and vibration, and releases generator death and battle exit at
/// separate cues. Returns to view 4 and restores player control. The interpreter
/// and player borrow this overlay's records and animation data through completion.
static EvsCommand _gNeoArkPowerPlant2GeneratorClearScript[] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _neoArkPowerPlant2PrepareGeneratorClearScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gNeoArkPowerPlant2GeneratorClearWeaponIdle }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = NEO_ARK_POWER_PLANT_2_GENERATOR_CLEAR_SOUND_6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = NEO_ARK_POWER_PLANT_2_GENERATOR_CLEAR_SOUND_7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = _gNeoArkPowerPlant2GeneratorClearVibrationCommands }, { .vibrationSegments = _gNeoArkPowerPlant2GeneratorClearVibrationSegments }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gNeoArkPowerPlant2GeneratorReleaseDeathCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gNeoArkPowerPlant2GeneratorClearPlayerTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_INSTALL_AND_PLAY }, { .message = { .animation = &_gNeoArkPowerPlant2GeneratorClearPlayerAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_INSTALL_AND_PLAY }, { .message = { .animation = &_gNeoArkPowerPlant2GeneratorClearPlayerAnimation2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gNeoArkPowerPlant2GeneratorReleaseBattleCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path that restores the clear scene's final placement, view and generator releases.
///
/// Fades around scene cleanup, stops vibration, selects view 4, releases both
/// generator waits and ends player scripting. Ends at EVENT_SCRIPT_OPCODE_END;
/// the interpreter borrows this overlay's commands until completion.
static EvsCommand _gNeoArkPowerPlant2GeneratorClearSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gNeoArkPowerPlant2GeneratorClearPlayerTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _neoArkPowerPlant2StopSkippedSceneVibration }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gNeoArkPowerPlant2GeneratorReleaseBothCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Fixed world-space position of view 8's transient blue point light, in game units.
///
/// Coordinates are copied into the lighting slot each eligible frame.
static SVECTOR _gNeoArkPowerPlant2View8BlueLightPosition = { 4000, -7000, -4400, 0 };

/// Retained, unused copy of the generator-clear player position, in world game units.
///
/// Matches the position used by both normal and skipped clear presentation.
/// No light, glow, placement or script consumer selects this separate vector.
static const SVECTOR _gNeoArkPowerPlant2UnusedGeneratorClearPlayerPosition SECTION(".data") = { .vx = 4000, .vy = -5000, .vz = -1540, .pad = 0 };

/// Fixed world point of view 6's generator glow and damaged-support flashes, in game units.
///
/// Flash tasks retain this emitter address; keep the room overlay loaded until
/// their teardown. Glow projection only borrows it for the draw call.
static SVECTOR _gNeoArkPowerPlant2GeneratorGlowPosition = { 6140, -6085, -8500, 0 };

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gNeoArkPowerPlant2CollisionRooms[1] = {
    { .grid                 = &_gNeoArkPowerPlant2CollisionGrid,
      .viewBoundaryTriggers = _gNeoArkPowerPlant2ViewBoundaryTriggers,
      .actionTriggers       = _gNeoArkPowerPlant2ActionTriggers,
      .occluders            = _gNeoArkPowerPlant2Occluders },
};

WorldCoordRoomLighting gNeoArkPowerPlant2RoomLighting[1] = {
    { .lights = &_gNeoArkPowerPlant2RoomLights, .ambientTable = _gNeoArkPowerPlant2AmbientTable },
};

u8* gNeoArkPowerPlant2ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkPowerPlant2ViewCounts[2] = { ARRAY_SIZE(gNeoArkPowerPlant2ViewCameras), 0 };

DirectionWarpEntry gNeoArkPowerPlant2WarpEntries[1] = {
    { { { .word = 3072 }, 4000, -5000, -400 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4000, -5000, -400 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-axis collision normals, with 4096 representing unit length.
static SVECTOR _gNeoArkPowerPlant2CollisionGridNormals[] = {
#include "assets/neo_ark_power_plant_2_collision_03804_normals.inc"
};

/// Collision mesh vertices in signed room-coordinate units.
static SVECTOR _gNeoArkPowerPlant2CollisionGridVerts[] = {
#include "assets/neo_ark_power_plant_2_collision_03804_verts.inc"
};

/// Indexed triangles and quads using this room's surface classes 0, 3, 4 and 5.
static WorldCollisionGridFace _gNeoArkPowerPlant2CollisionGridFaces[] = {
#include "assets/neo_ark_power_plant_2_collision_03804_faces.inc"
};

/// Serialized face-index lists for the grid cells, each ending at -1.
static s16 _gNeoArkPowerPlant2CollisionGridCells[] = {
#include "assets/neo_ark_power_plant_2_collision_03804_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkPowerPlant2CollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_POWER_PLANT_2_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkPowerPlant2CollisionGridCells[(halfwordOffset)])
/// Twelve cell-list pointers in X-major order, four Z cells per X row.
static s16* _gNeoArkPowerPlant2CollisionGridTable[] = {
#include "assets/neo_ark_power_plant_2_collision_03804_table.inc"
};
#undef NEO_ARK_POWER_PLANT_2_CELL_FACE_LIST_AT_OFFSET

enum { NEO_ARK_POWER_PLANT_2_COLLISION_GRID_Z_CELL_COUNT = 4 };

/// The plant's collision mesh and 3-by-4 XZ cell index.
///
/// Cells are 4000 room units wide, with origin (-100, -12100). Activation
/// binds the mutable descriptor to the view; its mesh and cell lists remain
/// borrowed from this room overlay until collision teardown.
static WorldCollisionGrid _gNeoArkPowerPlant2CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkPowerPlant2CollisionGridNormals,
    .vertices    = _gNeoArkPowerPlant2CollisionGridVerts,
    .faces       = _gNeoArkPowerPlant2CollisionGridFaces,
    .cellFaceIds = _gNeoArkPowerPlant2CollisionGridTable,
    .xBias       = 100,
    .zBias       = 12100,
    .cellCountX  = ARRAY_SIZE(_gNeoArkPowerPlant2CollisionGridTable) / NEO_ARK_POWER_PLANT_2_COLLISION_GRID_Z_CELL_COUNT,
    .cellCountZ  = NEO_ARK_POWER_PLANT_2_COLLISION_GRID_Z_CELL_COUNT,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkPowerPlant2CollisionGridFaces),
};

ViewCamera gNeoArkPowerPlant2ViewCameras[NEO_ARK_POWER_PLANT_2_VIEW_COUNT] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x55F0, 6000 } }, 289 },
    { { { { -1159, 0, -3928 }, { -2285, 3331, 674 }, { 3195, 2382, -943 } }, { -791, 8408, 1169 } }, 230 },
    { { { { -875, 0, 4001 }, { 1486, 3802, 325 }, { -3714, 1522, -812 } }, { -6791, 7658, 1319 } }, 257 },
    { { { { -1124, 0, 3938 }, { 333, 4081, 95 }, { -3924, 346, -1120 } }, { -8841, 6358, 1319 } }, 230 },
    { { { { 3952, 0, 1075 }, { -23, 4095, 85 }, { -1075, -88, 3951 } }, { -8341, 6098, 8139 } }, 257 },
    { { { { -4002, 0, 868 }, { -45, 4090, -211 }, { -867, -216, -3997 } }, { -8441, 5798, 2039 } }, 289 },
    { { { { -2378, 0, 3334 }, { 2574, 2603, 1836 }, { -2119, 3162, -1511 } }, { -8341, 7898, 6139 } }, 257 },
    { { { { 3869, 0, 1342 }, { -153, 4069, 441 }, { -1334, -467, 3844 } }, { -4702, 6286, 3621 } }, 680 },
    { { { { -4095, 0, -13 }, { 4, 3863, -1361 }, { 12, -1361, -3862 } }, { -3992, 5736, 1231 } }, 230 },
};

/// Neutral per-channel texture modulation; 128 represents unity.
enum { NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB = 128 };

/// View 1's background-only batch list, with no source sprite elements.
///
/// The first zero count keeps decoded image strips; SPRITE_BATCH_END stops
/// traversal. The source slot retains this list's address without dereferencing it.
static SpriteBatch _gNeoArkPowerPlant2View1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 64 depth-sorted image rectangles in screen-relative pixels.
///
/// Texture pages use 8-bit indices and the CLUT at (0, 255); RGB 128 is
/// neutral modulation. The view's batches select ranges in source elements,
/// and the room overlay owns their mutable geometry and sorting depths.
static SpriteSource _gNeoArkPowerPlant2View2SpriteSources[] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 128, -48, 1319, { .fields = { 120, 56 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 72, -40, 1325, { .fields = { 104, 104 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 120, -40, 1325, { .fields = { 112, 232 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 48, -32, 1300, { .fields = { 96, 248 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, -32, 1300, { .fields = { 88, 112 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, -24, 1146, { .fields = { 88, 128 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, -16, 1111, { .fields = { 88, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 104, -8, 1082, { .fields = { 96, 240 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 72, 0, 1070, { .fields = { 80, 224 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 152, 0, 1033, { .fields = { 56, 48 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 144, 0, 1047, { .fields = { 56, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 120, 8, 1084, { .fields = { 40, 144 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 96, 8, 1062, { .fields = { 48, 192 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 72, 8, 1080, { .fields = { 96, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 64, 0, 1172, { .fields = { 80, 128 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 56, -8, 1242, { .fields = { 88, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 48, -16, 1205, { .fields = { 120, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 40, 0, 1232, { .fields = { 40, 184 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 48, -24, 1200, { .fields = { 48, 184 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 64, -24, 1225, { .fields = { 96, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 56, -16, 1086, { .fields = { 120, 80 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 72, -16, 1111, { .fields = { 104, 72 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, -8, 1032, { .fields = { 0, 136 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, 80, -8, 1092, { .fields = { 112, 40 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, -40, 1573, { .fields = { 56, 88 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 144, -120, 1298, { .fields = { 72, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 128, -120, 1395, { .fields = { 80, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 120, -120, 1622, { .fields = { 96, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 112, -104, 1644, { .fields = { 72, 176 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 56, -32, 1163, { .fields = { 64, 144 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 48, -32, 1191, { .fields = { 64, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 40, -32, 1534, { .fields = { 96, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 32, -24, 1514, { .fields = { 104, 80 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 88, 32, 1035, { .fields = { 40, 96 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 88, -64, 1500, { .fields = { 16, 112 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 80, -56, 1512, { .fields = { 72, 224 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, -48, 1542, { .fields = { 56, 240 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 112, -56, 1450, { .fields = { 24, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 128, -56, 1450, { .fields = { 8, 176 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 96 } }, 120, 0, 1061, { .fields = { 88, 104 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 64, -32, 1450, { .fields = { 24, 224 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 56, -40, 1475, { .fields = { 72, 128 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 48, -40, 1500, { .fields = { 88, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, -32, 1525, { .fields = { 80, 240 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -112, 48, 1097, { .fields = { 24, 80 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, 48, 1097, { .fields = { 8, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -80, 48, 1097, { .fields = { 8, 80 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, 40, 1097, { .fields = { 120, 48 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -64, 48, 1097, { .fields = { 112, 88 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 56, 32 } }, -112, 40, 1163, { .fields = { 112, 144 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -112, 32, 1225, { .fields = { 0, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -48, 56, 794, { .fields = { 72, 56 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -80, -72, 2000, { .fields = { 64, 240 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -72, -80, 1475, { .fields = { 120, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -64, -40, 1175, { .fields = { 72, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -64, 16, 925, { .fields = { 104, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -56, 16, 800, { .fields = { 112, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, 8, 1000, { .fields = { 72, 248 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, 40, 1056, { .fields = { 48, 240 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 88, 0, 1000, { .fields = { 8, 40 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 80, 24, 1025, { .fields = { 48, 80 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 128, 24, 925, { .fields = { 80, 184 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 136, 32, 875, { .fields = { 64, 48 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 144, 40, 875, { .fields = { 48, 40 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
};

/// View 2's 8 source ranges between the background selector and end marker.
///
/// The zero-count first record keeps decoded background strips; the terminal
/// record contributes no sprites. Ranges cover the view's complete source array.
static SpriteBatch _gNeoArkPowerPlant2View2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 6, 0 } },
    { 24, 16, 0, 0, { 1, 0 } },
    { 40, 4, 0, 0, { 4, 0 } },
    { 44, 5, 0, 0, { 0, 0 } },
    { 49, 1, 0, 0, { 5, 0 } },
    { 50, 1, 0, 0, { 3, 0 } },
    { 51, 6, 0, 0, { 7, 0 } },
    { 57, ARRAY_SIZE(_gNeoArkPowerPlant2View2SpriteSources) - 57, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 70 depth-sorted image rectangles in screen-relative pixels.
///
/// Texture pages use 8-bit indices and the CLUT at (0, 255); RGB 128 is
/// neutral modulation. The view's batches select ranges in source elements,
/// and the room overlay owns their mutable geometry and sorting depths.
static SpriteSource _gNeoArkPowerPlant2View3SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -88, 96, 864, { .fields = { 88, 48 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 48, 746, { .fields = { 88, 192 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -160, 64, 806, { .fields = { 104, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -144, 48, 750, { .fields = { 96, 176 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -128, 32, 810, { .fields = { 96, 88 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -120, 24, 852, { .fields = { 96, 48 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -112, 16, 888, { .fields = { 104, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -104, 16, 942, { .fields = { 96, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -96, 8, 1007, { .fields = { 112, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -88, 0, 1029, { .fields = { 120, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -136, 40, 779, { .fields = { 120, 232 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -136, 64, 843, { .fields = { 104, 176 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -112, 64, 879, { .fields = { 104, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -96, 64, 874, { .fields = { 120, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -88, 64, 971, { .fields = { 96, 152 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -80, 48, 1049, { .fields = { 96, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -160, 16, 936, { .fields = { 48, 16 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, -160, 8, 998, { .fields = { 40, 208 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 24, 911, { .fields = { 64, 216 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -160, 32, 845, { .fields = { 64, 56 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 40, 765, { .fields = { 72, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 8 } }, -160, 0, 1044, { .fields = { 24, 8 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, -8, 1093, { .fields = { 80, 224 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -104, 0, 1172, { .fields = { 104, 56 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -96, 0, 1162, { .fields = { 96, 232 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 16, 1011, { .fields = { 104, 168 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 16, 1011, { .fields = { 112, 56 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 24, 949, { .fields = { 88, 32 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 32, 899, { .fields = { 88, 40 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 32, 899, { .fields = { 88, 104 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 32, 899, { .fields = { 88, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 40, 838, { .fields = { 88, 72 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 40, 997, { .fields = { 88, 96 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 48, 788, { .fields = { 88, 88 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 48, 784, { .fields = { 88, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 48, 788, { .fields = { 88, 160 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 56, 754, { .fields = { 88, 136 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 56, 754, { .fields = { 88, 128 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 56, 929, { .fields = { 88, 112 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 64, 718, { .fields = { 88, 80 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 64, 798, { .fields = { 88, 152 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 72, 686, { .fields = { 88, 144 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 72, 686, { .fields = { 88, 168 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 72, 807, { .fields = { 88, 176 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 80, 657, { .fields = { 88, 184 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 80, 657, { .fields = { 88, 248 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 80, 702, { .fields = { 80, 88 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 88, 631, { .fields = { 88, 232 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 88, 631, { .fields = { 88, 240 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 96, 606, { .fields = { 80, 24 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 96, 561, { .fields = { 80, 40 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 104, 584, { .fields = { 80, 48 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 104, 584, { .fields = { 80, 32 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 112, 559, { .fields = { 80, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 112, 559, { .fields = { 80, 72 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 32, 859, { .fields = { 80, 80 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 40, 812, { .fields = { 80, 96 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 48, 772, { .fields = { 80, 104 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 56, 743, { .fields = { 80, 112 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 64, 728, { .fields = { 80, 136 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 72, 715, { .fields = { 88, 24 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 72, 718, { .fields = { 80, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 80, 702, { .fields = { 80, 128 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 80, 700, { .fields = { 80, 152 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 88, 690, { .fields = { 80, 160 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 88, 690, { .fields = { 80, 168 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 96, 678, { .fields = { 80, 192 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 96, 678, { .fields = { 80, 144 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 104, 667, { .fields = { 80, 176 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 112, 656, { .fields = { 80, 184 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
};

/// View 3's 4 source ranges between the background selector and end marker.
///
/// The zero-count first record keeps decoded background strips; the terminal
/// record contributes no sprites. Ranges cover the view's complete source array.
static SpriteBatch _gNeoArkPowerPlant2View3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 3, 0 } },
    { 23, 2, 0, 0, { 0, 0 } },
    { 25, 30, 0, 0, { 2, 0 } },
    { 55, ARRAY_SIZE(_gNeoArkPowerPlant2View3SpriteSources) - 55, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 30 depth-sorted image rectangles in screen-relative pixels.
///
/// Texture pages use 8-bit indices and the CLUT at (0, 255); RGB 128 is
/// neutral modulation. The view's batches select ranges in source elements,
/// and the room overlay owns their mutable geometry and sorting depths.
static SpriteSource _gNeoArkPowerPlant2View4SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -144, -8, 1375, { .fields = { 24, 168 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -96, -8, 1375, { .fields = { 16, 160 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -96, 0, 1200, { .fields = { 16, 144 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -144, 0, 1200, { .fields = { 24, 192 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -136, 8, 1150, { .fields = { 24, 184 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -88, 8, 1150, { .fields = { 24, 176 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, -48, -8, 1375, { .fields = { 96, 144 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 72 } }, -88, -8, 1375, { .fields = { 88, 72 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 72 } }, -144, -16, 1375, { .fields = { 72, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -56, 0, 1151, { .fields = { 80, 224 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -64, 0, 950, { .fields = { 72, 168 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -72, 0, 950, { .fields = { 64, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -80, 0, 925, { .fields = { 64, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -88, 0, 900, { .fields = { 64, 32 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -96, 8, 875, { .fields = { 120, 216 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -104, 8, 875, { .fields = { 112, 216 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -112, 24, 875, { .fields = { 64, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, 112, 250, { .fields = { 64, 152 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 72, 0, 1000, { .fields = { 80, 128 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 80, 0, 750, { .fields = { 80, 176 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 80, 48, 850, { .fields = { 96, 216 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 88, 16, 0x9E59, { .fields = { 72, 120 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 88, 64, 675, { .fields = { 88, 144 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 96, 32, 500, { .fields = { 80, 72 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 96, 88, 525, { .fields = { 72, 208 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 104, 48, 375, { .fields = { 72, 72 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 104, 96, 425, { .fields = { 64, 96 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 112, 64, 250, { .fields = { 88, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 120, 80, 250, { .fields = { 104, 216 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 128, 104, 250, { .fields = { 72, 240 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
};

/// View 4's 4 source ranges between the background selector and end marker.
///
/// The zero-count first record keeps decoded background strips; the terminal
/// record contributes no sprites. Ranges cover the view's complete source array.
static SpriteBatch _gNeoArkPowerPlant2View4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 3, 0, 0, { 0, 0 } },
    { 9, 8, 0, 0, { 2, 0 } },
    { 17, ARRAY_SIZE(_gNeoArkPowerPlant2View4SpriteSources) - 17, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 28 depth-sorted image rectangles in screen-relative pixels.
///
/// Texture pages use 8-bit indices and the CLUT at (0, 255); RGB 128 is
/// neutral modulation. The view's batches select ranges in source elements,
/// and the room overlay owns their mutable geometry and sorting depths.
static SpriteSource _gNeoArkPowerPlant2View5SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 0, 1350, { .fields = { 24, 40 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -160, 8, 1350, { .fields = { 0, 248 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 16, 1350, { .fields = { 16, 240 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -120, 16, 1350, { .fields = { 40, 48 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, -160, 24, 1350, { .fields = { 96, 104 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -72, 48, 1350, { .fields = { 64, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -64, 48, 1350, { .fields = { 96, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, 48, 1350, { .fields = { 24, 168 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -40, 48, 1350, { .fields = { 8, 168 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -128, 24, 1350, { .fields = { 8, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 56 } }, -128, 64, 1350, { .fields = { 56, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -160, -120, 575, { .fields = { 120, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -152, -96, 575, { .fields = { 80, 160 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -160, 0, 775, { .fields = { 72, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, -160, 56, 775, { .fields = { 96, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -136, 0, 850, { .fields = { 80, 56 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -136, 56, 850, { .fields = { 112, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -120, 8, 875, { .fields = { 80, 112 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -120, 56, 875, { .fields = { 88, 112 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -112, 24, 950, { .fields = { 112, 64 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -112, 64, 950, { .fields = { 88, 160 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -104, 32, 1075, { .fields = { 8, 136 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -104, 64, 1075, { .fields = { 16, 104 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -88, 56, 1100, { .fields = { 24, 136 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -112, 8, 2000, { .fields = { 0, 200 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 48 } }, 40, 8, 2000, { .fields = { 16, 56 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -8, 8, 2000, { .fields = { 32, 152 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -56, 8, 2000, { .fields = { 32, 104 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
};

/// View 5's 3 source ranges between the background selector and end marker.
///
/// The zero-count first record keeps decoded background strips; the terminal
/// record contributes no sprites. Ranges cover the view's complete source array.
static SpriteBatch _gNeoArkPowerPlant2View5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 13, 0, 0, { 2, 0 } },
    { 24, ARRAY_SIZE(_gNeoArkPowerPlant2View5SpriteSources) - 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's 8 depth-sorted image rectangles in screen-relative pixels.
///
/// Texture pages use 8-bit indices and the CLUT at (0, 255); RGB 128 is
/// neutral modulation. The view's batches select ranges in source elements,
/// and the room overlay owns their mutable geometry and sorting depths.
static SpriteSource _gNeoArkPowerPlant2View6SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 152, -8, 869, { .fields = { 112, 192 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 144, -8, 903, { .fields = { 120, 192 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 136, -8, 939, { .fields = { 104, 192 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 64, -40, 1575, { .fields = { 88, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 48, -40, 1625, { .fields = { 112, 96 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 32, -40, 1700, { .fields = { 112, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 16, -40, 2000, { .fields = { 96, 96 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, 8, -40, 2000, { .fields = { 104, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
};

/// View 6's 2 source ranges between the background selector and end marker.
///
/// The zero-count first record keeps decoded background strips; the terminal
/// record contributes no sprites. Ranges cover the view's complete source array.
/// Batch 2 selects five Life Support sprites whose visibility can change.
static SpriteBatch _gNeoArkPowerPlant2View6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, ARRAY_SIZE(_gNeoArkPowerPlant2View6SpriteSources) - 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's background-only batch list, with no source sprite elements.
///
/// The first zero count keeps decoded image strips; SPRITE_BATCH_END stops
/// traversal. The source slot retains this list's address without dereferencing it.
static SpriteBatch _gNeoArkPowerPlant2View7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's background-only batch list, with no source sprites.
///
/// Zero count keeps decoded image strips; SPRITE_BATCH_END ends traversal.
/// The source slot retains this list's address without reading it as sprites.
static SpriteBatch _gNeoArkPowerPlant2View8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Four opaque texture rectangles layered over view 9's decoded background.
///
/// Positions and dimensions are pixels relative to the view centre; UV is in
/// texels and depth is the unscaled ordering value. RGB 128 is neutral.
/// The sprite renderer borrows these mutable records while the room is loaded.
static SpriteSource _gNeoArkPowerPlant2View9SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 80, 48 } }, -160, 72, 525, { .fields = { 48, 160 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { 143, 0x3FC0, { .fields = { 80, 48 } }, 80, 72, 525, { .fields = { 48, 208 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { 143, 0x3FC0, { .fields = { 80, 80 } }, -80, 40, 475, { .fields = { 48, 0 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
    { 143, 0x3FC0, { .fields = { 80, 80 } }, 0, 40, 475, { .fields = { 48, 80 } }, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, NEO_ARK_POWER_PLANT_2_SPRITE_NEUTRAL_RGB, 0 },
};

/// View 9's background strip selector and complete four-source sprite range.
///
/// Zero count keeps decoded image strips. The next range selects every source
/// rectangle; SPRITE_BATCH_END ends traversal. Records remain room-owned.
static SpriteBatch _gNeoArkPowerPlant2View9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkPowerPlant2View9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkPowerPlant2SpriteViews[NEO_ARK_POWER_PLANT_2_VIEW_COUNT] = {
    { { .empty = _gNeoArkPowerPlant2View1SpriteBatches }, _gNeoArkPowerPlant2View1SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant2View2SpriteSources }, _gNeoArkPowerPlant2View2SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant2View3SpriteSources }, _gNeoArkPowerPlant2View3SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant2View4SpriteSources }, _gNeoArkPowerPlant2View4SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant2View5SpriteSources }, _gNeoArkPowerPlant2View5SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant2View6SpriteSources }, _gNeoArkPowerPlant2View6SpriteBatches, NULL },
    { { .empty = _gNeoArkPowerPlant2View7SpriteBatches }, _gNeoArkPowerPlant2View7SpriteBatches, NULL },
    { { .empty = _gNeoArkPowerPlant2View8SpriteBatches }, _gNeoArkPowerPlant2View8SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant2View9SpriteSources }, _gNeoArkPowerPlant2View9SpriteBatches, NULL },
};

/// Twenty-one authored point lights admitted in every logical view.
///
/// Placements and falloff distances use world units; RGB intensity uses Q12.
/// Gameplay parents and composes the coordinates and overwrites attenuation,
/// so these room-owned records stay mutable for the overlay's lifetime.
static WorldCoordPointLight _gNeoArkPowerPlant2PointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 990, -7530, -0x2710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 990, -7530, -7350 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 990, -7530, -4650 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 990, -7530, -2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7980, -7500, -650 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7980, -7500, -2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7980, -7500, -4650 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7980, -7500, -7350 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7980, -7500, -0x2710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 1966, 1884 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6340, -7990, -2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4510, -5320, -0x2710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 1966, 1884 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5770, -7990, -7380 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5770, -7990, -4670 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4840, -6520, -2600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 1966, 1884 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3280, -7990, -4670 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3280, -7990, -7380 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3280, -7990, -2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3280, -7990, 700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5770, -7990, 730 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7980, -7500, 730 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 990, -7530, 700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3194, 3112 }, { 0, 0 } }, 1000, 4000 },
};

/// The plant's authored model-light collection, containing only point lights.
///
/// Gameplay parents and composes the borrowed light coordinates and updates
/// attenuation. The record and its mutable array must stay loaded while used.
static WorldCoordRoomLights _gNeoArkPowerPlant2RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkPowerPlant2PointLights),
    .pointLights           = _gNeoArkPowerPlant2PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Eight directed boundaries connecting ordinary views 2 through 6 in both directions.
///
/// Origins, corner offsets and radii use game coordinates; normals use Q12.
/// The last boundary includes LAST. Room setup binds, links and enables these
/// mutable records until collision teardown; views 1, 7, 8 and 9 have no boundary here.
static WorldCollisionTrigger _gNeoArkPowerPlant2ViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 3040, -7040, 128, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3135, -7104, 96, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4096, -7104, -2272, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3967, -7040, -2240, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6304, -7104, -2336, 0 }, { { 3, -2256, -1209, 0 }, { -2, -2256, 1209, 0 }, { 3, 2256, -1209, 0 }, { -2, 2256, 1209, 0 } }, { 4099, 0, 7, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6382, -7104, -2272, 0 }, { { -2, -2256, 1209, 0 }, { 3, -2256, -1209, 0 }, { -2, 2256, 1209, 0 }, { 3, 2256, -1209, 0 } }, { -4102, 0, -10, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7887, -7072, -4785, 0 }, { { 2024, -2256, 142, 0 }, { -2023, -2256, -142, 0 }, { 2024, 2256, 142, 0 }, { -2023, 2256, -142, 0 } }, { -288, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7839, -7040, -4705, 0 }, { { -2024, -2256, -139, 0 }, { 2024, -2256, 140, 0 }, { -2024, 2256, -139, 0 }, { 2024, 2256, 140, 0 } }, { 281, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Descending stair direction in the second action quad's control word.
enum { NEO_ARK_POWER_PLANT_2_STAIR_DESCEND = 0x100 };

/// Eight room interactions: paired stairs, automatic room/warp actions and CAP checks.
///
/// Warp parameters pack departure/arrival slots 1 and 2. CAP IDs 2 and 3 route
/// through the room message handler. Game-coordinate quads and Q12 normals are
/// bound and linked at room setup; the last record includes LAST. Storage stays
/// mutable and room-owned until collision teardown.
static WorldCollisionTrigger _gNeoArkPowerPlant2ActionTriggers[8] = {
    { NULL, NULL, NULL, { 3744, -64, -1616, 0 }, { { -320, 0, -496, 0 }, { 320, 0, -496, 0 }, { -320, 0, 496, 0 }, { 320, 0, 496, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 590, WORLD_COLLISION_TRIGGER_ACTION_FACING, 5, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4064, -5056, -480, 0 }, { { -336, 0, -496, 0 }, { 336, 0, -496, 0 }, { -336, 0, 496, 0 }, { 336, 0, 496, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 596, WORLD_COLLISION_TRIGGER_ACTION_FACING | NEO_ARK_POWER_PLANT_2_STAIR_DESCEND, 37, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4607, -769, -1600, 0 }, { { -16, -1391, -496, 0 }, { 17, 1392, -496, 0 }, { -16, -1391, 496, 0 }, { 17, 1392, 496, 0 } }, { -4101, 42, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4927, -4673, -480, 0 }, { { -17, 1392, 496, 0 }, { 16, -1391, 496, 0 }, { -17, 1392, -496, 0 }, { 16, -1391, -496, 0 } }, { -4101, -49, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 33, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 256, -48, -1120, 0 }, { { -320, 0, -496, 0 }, { 320, 0, -496, 0 }, { -320, 0, 496, 0 }, { 320, 0, 496, 0 } }, { 0, 4113, 0, 0 }, { 4096, 0, 0, 0 }, 590, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6720, -5152, -7600, 0 }, { { -320, 0, -896, 0 }, { 320, 0, -896, 0 }, { -320, 0, 896, 0 }, { 320, 0, 896, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4288, -5152, -2160, 0 }, { { -2720, 0, -368, 0 }, { 2720, 0, -368, 0 }, { -2720, 0, 368, 0 }, { 2720, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 2733, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6864, -5120, -4256, 0 }, { { -400, 0, -2352, 0 }, { 400, 0, -2352, 0 }, { -400, 0, 2352, 0 }, { 400, 0, 2352, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 2374, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Beta Generator and Watcher resource recipes, ending at AREA_PLACEMENT_END.
///
/// Placement IDs 53 and 21 select decimal actor files 105300 and 202100.
/// The loader borrows the resource list and loaded actor task descriptors.
static AreaResource _gNeoArkPowerPlant2GeneratorResources[] = {
    { 53, 53, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, GENERATOR_BODY_TASK_INDEX, { 0, 0 }, gActor105300GeneratorTasks },
    { 21, 21, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor202100WatcherTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 11's Rook GOLEM and Watcher resource recipes, ending at AREA_PLACEMENT_END.
///
/// Placement IDs 57 and 21 select decimal actor files 105700 and 202100.
/// The loader borrows this room list and the loaded actors' task descriptors.
static AreaResource _gNeoArkPowerPlant2RookResources[] = {
    { 57, 57, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor05700RookGolemTasks },
    { 21, 21, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor202100WatcherTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 17's Bishop GOLEM resource recipe, ending at AREA_PLACEMENT_END.
///
/// Placement ID 39 selects decimal actor file 403900 and its first task recipe.
/// The room list and loaded actor descriptor stay borrowed through spawning.
static AreaResource _gNeoArkPowerPlant2BishopResources[] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor403900GolemTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gNeoArkPowerPlant2AreaVariants[NEO_ARK_POWER_PLANT_2_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkPowerPlant2Variant1Placements, .resources = _gNeoArkPowerPlant2GeneratorResources },
    [11]                   = { .placements = gMapNeoArkPowerPlant2Variant11Placements, .resources = _gNeoArkPowerPlant2RookResources },
    [17]                   = { .placements = gMapNeoArkPowerPlant2Variant17Placements, .resources = _gNeoArkPowerPlant2BishopResources },
};

/// The plant's single room-space line-of-sight blocking quad.
///
/// Origin, corner offsets and radius use game units; the normal uses Q12.
/// LAST includes this record before stopping setup. The low flag bit's role
/// is unproven. Linking mutates the room-owned node until collision teardown.
static WorldCollisionOccluder _gNeoArkPowerPlant2Occluders[1] = {
    { NULL, NULL, { 6064, -6976, -6944, 0 }, { { 208, 2944, 1760, 0 }, { -208, 2944, -1760, 0 }, { 208, -2944, 1760, 0 }, { -208, -2944, -1760, 0 } }, { 4068, 0, -481, 0 }, 3434, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

// Keep read-only lighting data in the original room-data section.
/// Ambient minima for logical views 1..9, preceded by the view-count header.
///
/// RGB uses 16 units per colour level; view 8 has higher minima than the other
/// views. The fourth colour halfword retains authored luma and is not queried.
static const WorldCoordRoomAmbientEntry _gNeoArkPowerPlant2AmbientTable[10] __attribute__((section(".data"))) = {
    { .viewCount = ARRAY_SIZE(_gNeoArkPowerPlant2AmbientTable) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1639, 1232, 1229, 1384 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Cue-2 sound-script requests; cue 1 adds one to the nonzero base.
enum {
    NEO_ARK_POWER_PLANT_2_SURFACE_2_WALK_SOUND = 0x10000015,
    NEO_ARK_POWER_PLANT_2_SURFACE_2_RUN_SOUND  = 0x10000017,
    NEO_ARK_POWER_PLANT_2_SURFACE_2_JUMP_SOUND = 0x10000019,
    NEO_ARK_POWER_PLANT_2_SURFACE_4_WALK_SOUND = 0x10000059,
    NEO_ARK_POWER_PLANT_2_SURFACE_4_RUN_SOUND  = 0x1000005B,
    NEO_ARK_POWER_PLANT_2_SURFACE_5_WALK_SOUND = 0x1000005D,
    NEO_ARK_POWER_PLANT_2_SURFACE_5_RUN_SOUND  = 0x1000005F,
};

// Keep read-only cue tables in the original room-data section.
/// Walking, running and scripted-jump cue bases for surface class 4.
///
/// Uses scripts 0x59/0x5A for walking and jumping, 0x5B/0x5C for running,
/// in the currently loaded type-1 sound bank. Surface records borrow this data.
static const WorldCollisionFootstepSounds _gNeoArkPowerPlant2Surface4FootstepSounds __attribute__((section(".data"))) = {
    .walk         = NEO_ARK_POWER_PLANT_2_SURFACE_4_WALK_SOUND,
    .run          = NEO_ARK_POWER_PLANT_2_SURFACE_4_RUN_SOUND,
    .scriptedJump = NEO_ARK_POWER_PLANT_2_SURFACE_4_WALK_SOUND,
};

/// Walking, running and scripted-jump cue bases for stair surface class 2.
///
/// Uses scripts 0x15/0x16, 0x17/0x18 and 0x19/0x1A respectively in the
/// currently loaded type-1 bank. The descent selects this class independently
/// of the grid faces; surface records borrow the room-owned data.
static const WorldCollisionFootstepSounds _gNeoArkPowerPlant2Surface2FootstepSounds __attribute__((section(".data"))) = {
    .walk         = NEO_ARK_POWER_PLANT_2_SURFACE_2_WALK_SOUND,
    .run          = NEO_ARK_POWER_PLANT_2_SURFACE_2_RUN_SOUND,
    .scriptedJump = NEO_ARK_POWER_PLANT_2_SURFACE_2_JUMP_SOUND,
};

/// Walking, running and scripted-jump cue bases for surface class 5.
///
/// Uses scripts 0x5D/0x5E for walking and jumping, 0x5F/0x60 for running,
/// in the currently loaded type-1 sound bank. Surface records borrow this data.
static const WorldCollisionFootstepSounds _gNeoArkPowerPlant2Surface5FootstepSounds __attribute__((section(".data"))) = {
    .walk         = NEO_ARK_POWER_PLANT_2_SURFACE_5_WALK_SOUND,
    .run          = NEO_ARK_POWER_PLANT_2_SURFACE_5_RUN_SOUND,
    .scriptedJump = NEO_ARK_POWER_PLANT_2_SURFACE_5_WALK_SOUND,
};

/// Blocking collision policy without footstep cues for classes 0, 1, 6 and 7.
///
/// Enables weapon impacts and pushback; the surface directory borrows this record.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant2DefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking collision policy and stair footstep cues for surface class 2.
///
/// Enables weapon impacts and pushback. The descending stair sets this class;
/// the room's grid faces do not use it. Both record and cues are room-owned.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant2Surface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkPowerPlant2Surface2FootstepSounds,
};

/// Probe-passable surface class 3, with weapon impacts disabled and no footstep cues.
///
/// Grid contacts still apply pushback; the surface directory borrows this record.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant2Surface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking surface class 4 with weapon impacts, pushback and its own footstep cues.
///
/// The surface directory borrows this record and its room-owned cue table.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant2Surface4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkPowerPlant2Surface4FootstepSounds,
};

/// Blocking surface class 5 with weapon impacts, pushback and its own footstep cues.
///
/// The surface directory borrows this record and its room-owned cue table.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant2Surface5Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkPowerPlant2Surface5FootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkPowerPlant2SurfaceProperties[NEO_ARK_POWER_PLANT_2_SURFACE_CLASS_COUNT] = {
    &_gNeoArkPowerPlant2DefaultSurfaceProperties,
    &_gNeoArkPowerPlant2DefaultSurfaceProperties,
    &_gNeoArkPowerPlant2Surface2Properties,
    &_gNeoArkPowerPlant2Surface3Properties,
    &_gNeoArkPowerPlant2Surface4Properties,
    &_gNeoArkPowerPlant2Surface5Properties,
    &_gNeoArkPowerPlant2DefaultSurfaceProperties,
    &_gNeoArkPowerPlant2DefaultSurfaceProperties,
};

/// Low policy nibble requesting a saved map mark after an area-layout update.
enum { NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK = 1 };

/// Saved-area layouts and map marks committed after the Beta Generator is defeated.
///
/// Eight updates precede AREA_APPLY_END. Shrine layouts differ between replay/
/// Scavenger and Bounty/Nightmare modes; the B1 control-room update clears its
/// map mark. Applying the list also removes saved enemy poses in accepted areas.
/// The synchronous updater borrows these records only for the call.
static const AreaApplyRec _gNeoArkPowerPlant2GeneratorClearAreaUpdates[] __attribute__((section(".data"))) = {
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL, 11, AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 4, AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 4, AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 4, AREA_APPLY_MODE_REPLAY_SCAVENGER | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 3, AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 3, AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM, 11, AREA_APPLY_MODE_ALWAYS },
    { AREA_APPLY_END, 0, 0, 0 },
};

/// Additional submarine-tunnel, island and gallery layouts after generator clearance.
///
/// Applied only when GAME_FLAG_0F3 is nonzero; that flag's story meaning is
/// unproven. All three updates set layout 2 and a map mark in every save mode,
/// remove saved enemy poses, then end at AREA_APPLY_END. Borrowed synchronously.
static const AreaApplyRec _gNeoArkPowerPlant2GeneratorClearExtraAreaUpdates[] __attribute__((section(".data"))) = {
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, 2, AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 2, AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 2, AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_2_AREA_UPDATE_MARK },
    { AREA_APPLY_END, 0, 0, 0 },
};

/// Rejects key-item use in this room, returning the menu's unusable result.
///
/// Reads no payload, consumes no item and retains no arguments.
static s32 _neoArkPowerPlant2RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a Neo Ark destination room for a Power Plant 2 transition request.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Copies the complete eight-byte request
/// into writable reply storage before resolving its room from game progress.
/// Query mode preserves the copied record. The pointers may alias and are
/// borrowed only for this call; the Neo Ark map overlay must be loaded.
/// Always returns 1 to allow the transition; task and message ID are unused.
static s32 _neoArkPowerPlant2ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_POWER_PLANT_2_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return NEO_ARK_POWER_PLANT_2_TRANSITION_ALLOWED;
}

/// Selects Power Plant 2 CAP dialogue from plant clearance and generator battle state.
///
/// Room command 2 selects CAP 2 or 5 by clearance. Command 3 selects CAP 7
/// after battle completion, otherwise CAP 6 for a downed part or CAP 3.
/// Other commands do nothing. Uses display-transition playback and returns zero;
/// only `commandIndex` is read. Keep the plant CAP resources loaded through playback.
static s32 _neoArkPowerPlant2HandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg)
{
    enum { COMMAND_CHECK_PLANT     = 2,
           COMMAND_CHECK_GENERATOR = 3,
           CAP_PLANT_UNCLEARED     = 2,
           CAP_PLANT_CLEARED       = 5,
           CAP_GENERATOR_ACTIVE    = 3,
           CAP_GENERATOR_PART_DOWN = 6,
           CAP_BATTLE_FINISHED     = 7 };
    s32 capCommand;

    switch (commandIndex) {
        case COMMAND_CHECK_PLANT:
            if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
                capCommand = CAP_PLANT_UNCLEARED;
            } else {
                capCommand = CAP_PLANT_CLEARED;
            }
            capRunCommandWithTransition(capCommand);
            break;
        case COMMAND_CHECK_GENERATOR:
            capCommand = CAP_BATTLE_FINISHED;
            if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_FINISHED) {
                capCommand = gameFlagGetNibble(GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN) != 0 ? CAP_GENERATOR_PART_DOWN : CAP_GENERATOR_ACTIVE;
            }
            capRunCommandWithTransition(capCommand);
            break;
    }
    return 0;
}

/// Ignores room-action requests and returns zero.
///
/// The request is borrowed during synchronous dispatch; no payload is read or retained.
static s32 _neoArkPowerPlant2IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Cancels active effects and blocks new ability requests for the generator-clear scene.
///
/// Called at the scene script's start with live room-effect and attachment state.
/// Cancellation is deferred to the effect update; the attachment event lock
/// remains set for the ordinary attachment controller to release.
static void _neoArkPowerPlant2PrepareGeneratorClearScene(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Stops scene vibration when the generator-clear event is skipped.
///
/// Requests vibration-script and motor-task teardown and clears port 0's pending
/// vibration requests. Requires a live game session; teardown can be deferred
/// while actor updates are frozen.
static void _neoArkPowerPlant2StopSkippedSceneVibration(void)
{
    padScriptHalt();
}

/// Installs the power plant message receiver and selects its music options.
///
/// Called in state 0 with a live room task and initialized gameplay resources.
/// Registers the borrowed task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
/// Variant 1 suppresses ending music; other variants preserve the flow flags.
static void _neoArkPowerPlant2InitializeRoom(Task* task)
{
    enum { NEO_ARK_POWER_PLANT_2_SKIP_ENDING_MUSIC_VARIANT = 1 };
    u8 variant;

    task->msgTable = _gNeoArkPowerPlant2RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    // Keep the selected variant as the byte written to the music flags.
    variant = gGameSession->location.variant;
    if (variant == NEO_ARK_POWER_PLANT_2_SKIP_ENDING_MUSIC_VARIANT) {
        gGameSession->flowFlags = variant;
    }
    task->state = task->state + 1;
}

/// Starts the generator-clear scene once its actor is gone and presentation is ready.
///
/// Room state 1 checks placement 0's presence reply while the plant is uncleared.
/// Waits for the ability wheel and display transition, then commits clearance,
/// elevator/map/story updates and starts the clear script with its skip script.
/// Requires live scene, session and save state; the task argument is unused.
static void _neoArkPowerPlant2UpdateRoom(Task* unusedTask)
{
    enum { GENERATOR_PLACEMENT         = 0,
           GENERATOR_CLEAR_SCENE_EVENT = 0x17,
           OBJECTIVE_AFTER_CLEAR       = 0x2E,
           DIALOGUE_AFTER_CLEAR        = 7 };
    Task* generatorTask;

    // Commit progress before admitting the clear scene; presentation may defer it.
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
        generatorTask = sceneFindPlacedActor(GENERATOR_PLACEMENT);
        if ((generatorTask != NULL) && (taskMessageDispatch(generatorTask, ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) && (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) &&
            (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED, 1);
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_EVE_ELEVATOR_UNLOCKED, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_POWER_PLANT_2, 0);
            areaApplySavedUpdates(_gNeoArkPowerPlant2GeneratorClearAreaUpdates);
            if (gameFlagGetNibble(GAME_FLAG_0F3) != 0) {
                areaApplySavedUpdates(_gNeoArkPowerPlant2GeneratorClearExtraAreaUpdates);
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = GENERATOR_CLEAR_SCENE_EVENT;
            gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, OBJECTIVE_AFTER_CLEAR);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, DIALOGUE_AFTER_CLEAR);
            evsStartScriptWithSkip(_gNeoArkPowerPlant2GeneratorClearScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gNeoArkPowerPlant2GeneratorClearSkipScript);
        }
    }
}

void neoArkPowerPlant2RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gNeoArkPowerPlant2RoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Emits a random generator flash while effects run and the plant remains uncleared.
///
/// Advances the unsigned 32-bit LCG once per eligible call and emits when the
/// upper-word low three bits are zero. The caller gates the damaged generator
/// and active view. The flash retains the room's emitter-position storage;
/// keep that storage and the room resources loaded through the effect's lifetime.
static inline void _neoArkPowerPlant2EmitGeneratorFlash(void)
{
    enum { GENERATOR_FLASH_MASK      = 7,
           GENERATOR_FLASH_BASE_SIZE = 0x400 };
    u32 randomWord;

    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
        randomWord      = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomWord;
        if (((randomWord >> 16) & GENERATOR_FLASH_MASK) == 0) {
            effectSpawn(EFFECT_FLASH_BURST, NULL, GENERATOR_FLASH_BASE_SIZE, &_gNeoArkPowerPlant2GeneratorGlowPosition);
        }
    }
}

void neoArkPowerPlant2UpdateViewLightingTask(Task* task)
{
    enum { EFFECTS_UNREGISTERED        = 0,
           EFFECTS_REGISTERED          = 1,
           GENERATOR_VIEW              = 6,
           BLUE_LIGHT_VIEW             = 8,
           GENERATOR_GLOW_RADIUS_SCALE = 0x300,
           GENERATOR_GLOW_RGB444       = 0x334,
           BLUE_LIGHT_SLOT             = 4,
           BLUE_LIGHT_LIFETIME_FRAMES  = 4,
           BLUE_LIGHT_INNER_RADIUS     = 0x400,
           BLUE_LIGHT_OUTER_RADIUS     = 0x4000,
           BLUE_LIGHT_RANDOM_Q12_MASK  = 0x700,
           BLUE_LIGHT_BASE_Q12         = 0x800 };
    u32                            randomWord;
    u16                            blueIntensityQ12;
    WorldCoordPointLight*          pointLight;
    WorldCoordTransientPointLight* lightSlot;

    if (task->state == EFFECTS_UNREGISTERED) {
        gRoomEffectFlashId      = EFFECT_NEO_ARK_POWER_PLANT_2_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_NEO_ARK_POWER_PLANT_2_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_NEO_ARK_POWER_PLANT_2_SPARK_BURST;
        task->state             = EFFECTS_REGISTERED;
    }
    switch ((u8)viewGetMappedIndex()) {
        case GENERATOR_VIEW:
            // Consume one random draw only while the damaged generator can flash.
            if (gameFlagGetNibble(GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN) != 0) {
                _neoArkPowerPlant2EmitGeneratorFlash();
            } else {
                glowDrawDisc(&_gNeoArkPowerPlant2GeneratorGlowPosition, GENERATOR_GLOW_RADIUS_SCALE, GENERATOR_GLOW_RGB444);
            }
            break;
        case BLUE_LIGHT_VIEW:
            // Refresh a retained shared slot; leaving the view lets its countdown expire.
            lightSlot                                          = &gWorldCoordTransientPointLights[BLUE_LIGHT_SLOT];
            lightSlot->framesLeft                              = BLUE_LIGHT_LIFETIME_FRAMES;
            pointLight                                         = &lightSlot->light;
            pointLight->inner                                  = BLUE_LIGHT_INNER_RADIUS;
            pointLight->outer                                  = BLUE_LIGHT_OUTER_RADIUS;
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            randomWord                                         = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState                                    = randomWord;
            blueIntensityQ12                                   = ((randomWord >> 16) & BLUE_LIGHT_RANDOM_Q12_MASK) + BLUE_LIGHT_BASE_Q12;
            pointLight->head.color.b                           = blueIntensityQ12;
            pointLight->head.color.r                           = blueIntensityQ12 >> 1;
            pointLight->head.color.g                           = blueIntensityQ12 >> 1;
            pointLight->head.transform.lighting.local.t[0]     = _gNeoArkPowerPlant2View8BlueLightPosition.vx;
            pointLight->head.transform.lighting.local.t[1]     = _gNeoArkPowerPlant2View8BlueLightPosition.vy;
            pointLight->head.transform.lighting.local.t[2]     = _gNeoArkPowerPlant2View8BlueLightPosition.vz;
            break;
    }
}

#include "../../shared/effects/glow_draw_disc.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void neoArkPowerPlant2RoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void neoArkPowerPlant2RoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void neoArkPowerPlant2RoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"

    void neoArkPowerPlant2SetView6SpritesHidden(u8 hidden)
    {
        enum { VIEW_INDEX      = 5,
               BATCH_INDEX     = 2,
               SPRITES_VISIBLE = 0,
               SPRITES_HIDDEN  = 1 };

        const GameLocationKey* location = &gGameSession->location;
        SpriteBatch*           view6Batches;

        view6Batches = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1][VIEW_INDEX].batches;
        if (hidden == SPRITES_VISIBLE) {
            view6Batches[BATCH_INDEX].hidden = SPRITES_VISIBLE;
        } else if (hidden == SPRITES_HIDDEN) {
            view6Batches[BATCH_INDEX].hidden = SPRITES_HIDDEN;
        }
    }
