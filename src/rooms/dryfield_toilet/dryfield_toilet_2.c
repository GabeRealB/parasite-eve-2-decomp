#include "rooms/dryfield_toilet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_toilet_private.h"

#include "actors/actor_01200.h"
#include "actors/actor_02400.h"
#include "actors/actor_323300.h"
#include "actors/actor_401000.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"
#include "../../shared/room/room_visual_effects.h"

static WorldCollisionOccluder _gDryfieldToiletSightOccluders[1];
static WorldCollisionTrigger  _gDryfieldToiletViewBoundaryTriggers[6];
static WorldCollisionTrigger  _gDryfieldToiletActionTriggers[9];
static WorldCoordRoomLights   _gDryfieldToiletRoomLights;

/// Player clip ids and the serialized word count used by this encounter.
enum {
    DRYFIELD_TOILET_PLAYER_IDLE_CLIP                 = 1,
    DRYFIELD_TOILET_PLAYER_EVENT_CLIP48              = ANIMATION_BANK_BASE_SET_COUNT + 1,
    DRYFIELD_TOILET_PLAYER_EVENT_CLIP49              = ANIMATION_BANK_BASE_SET_COUNT + 2,
    DRYFIELD_TOILET_PLAYER_SKIP_CLIP                 = 9,
    DRYFIELD_TOILET_PLAYER_ANIMATION_COPY_WORD_COUNT = 4,
};

/// Commands in the stage-2/area-16 actor namespace and placed actor selectors.
enum {
    DRYFIELD_TOILET_WOMAN_PLACEMENT_INDEX          = 0,
    DRYFIELD_TOILET_ODD_STRANGER_PLACEMENT_INDEX   = 1,
    DRYFIELD_TOILET_ODD_STRANGER_COMMAND1          = 1,
    DRYFIELD_TOILET_ODD_STRANGER_COMMAND_AMBUSH    = 2,
    DRYFIELD_TOILET_WOMAN_COMMAND_SPAWN_STRANGER   = 10,
    DRYFIELD_TOILET_WOMAN_COMMAND_RELEASE_STRANGER = 11,
    DRYFIELD_TOILET_WOMAN_COMMAND_TURN             = 12,
    DRYFIELD_TOILET_WOMAN_COMMAND_SPRAY            = 13,
    DRYFIELD_TOILET_WOMAN_DRAW_HIDE_RELEASE        = 2,
};

/// The door's one-based warp slots and saved map-marker nibble.
enum {
    DRYFIELD_TOILET_DOOR_WARP_SLOT                = 1,
    DRYFIELD_TOILET_PARKING_LOT_ARRIVAL_WARP_SLOT = 3,
    DRYFIELD_TOILET_DOOR_MAP_FLAG                 = 480,
};

/// Number of model normals blended, excluding the unconsumed serialized vector.
enum { DRYFIELD_TOILET_LESSER_STRANGER_MORPH_NORMAL_COUNT = 1604 };

static SVECTOR _gDryfieldToiletLesserStrangerMorphSavedVertices[326];
static SVECTOR _gDryfieldToiletLesserStrangerMorphSavedNormals[DRYFIELD_TOILET_LESSER_STRANGER_MORPH_NORMAL_COUNT];

/// Player encounter clips, extended ids 47..49; 47 is empty.
///
/// Both encounter paths copy four entries, one past these three: id 50
/// receives the following copy request's first word, this list's address, and
/// is never played. Playback borrows the room's clips while the encounter plays.
static AnimationSet* _gDryfieldToiletPlayerEncounterSets[3] = { NULL, &gDryfieldToiletAnimation03054, &gDryfieldToiletAnimation035A4 };

/// Installs the encounter clips; reads four entries from a three-entry list.
static AnimationBankCopyRequest _gDryfieldToiletPlayerAnimationBankCopy = { _gDryfieldToiletPlayerEncounterSets, DRYFIELD_TOILET_PLAYER_ANIMATION_COPY_WORD_COUNT };

/// Unreferenced bank-1 idle request, resetting clip 1 with world collision disabled.
static AnimationPlayRequest _gDryfieldToiletUnusedPlayerIdleAnimation = { { .index = 1 }, DRYFIELD_TOILET_PLAYER_IDLE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// First player encounter clip, with a twenty-frame blend and world collision disabled.
///
/// The script replaces source.index in a copy with the equipped weapon bank.
/// Clip 48 uses the installed extension; its physical motion is unproven.
static AnimationPlayRequest _gDryfieldToiletPlayerEventAnimation48 = { { .index = 1 }, DRYFIELD_TOILET_PLAYER_EVENT_CLIP48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

/// Second player encounter clip, with a twenty-frame blend and world collision disabled.
///
/// The script replaces source.index in a copy with the equipped weapon bank.
/// Clip 49 uses the installed extension; its physical motion is unproven.
static AnimationPlayRequest _gDryfieldToiletPlayerEventAnimation49 = { { .index = 1 }, DRYFIELD_TOILET_PLAYER_EVENT_CLIP49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

/// Skip-path player clip 9, with a ten-frame blend and world collision disabled.
///
/// The script resolves the equipped weapon bank in a copy of this request.
/// Clip 9 belongs to that bank's base clips; its physical motion is unproven.
static AnimationPlayRequest _gDryfieldToiletPlayerSkipAnimation9 = { { .index = 1 }, DRYFIELD_TOILET_PLAYER_SKIP_CLIP, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player's encounter-end placement in room coordinates, with 4096 angle units per turn.
static ActorTransform _gDryfieldToiletPlayerBattlePlacement = { { -1664, 0, 135, 0 }, { 0, -2047, 0, 0 } };

/// Initial Odd Stranger command 1, which only caches its bytes in the toilet namespace.
///
/// The receiver handles toilet commands 0 and 2; command 1 leaves its state intact.
static ActorCommand _gDryfieldToiletOddStrangerCommand1 = { { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET } }, DRYFIELD_TOILET_ODD_STRANGER_COMMAND1 };

/// Places the Odd Stranger at its ambush position and enters its ambush state.
static ActorCommand _gDryfieldToiletOddStrangerAmbushCommand = { { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET } }, DRYFIELD_TOILET_ODD_STRANGER_COMMAND_AMBUSH };

/// Spawns the woman's temporary Lesser Stranger and disables her sound retriggers.
static ActorCommand _gDryfieldToiletSpawnLesserStrangerCommand = { { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET } }, DRYFIELD_TOILET_WOMAN_COMMAND_SPAWN_STRANGER };

/// Releases the woman's temporary Lesser Stranger and stops her looping toilet sound.
static ActorCommand _gDryfieldToiletReleaseLesserStrangerCommand = { { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET } }, DRYFIELD_TOILET_WOMAN_COMMAND_RELEASE_STRANGER };

/// Unreferenced woman command presets: turn to a supplied placement or emit spray.
///
/// These are stored presets, not a terminated command list. No script sends
/// either record; a turn would also require a placement in its second payload.
static ActorCommand _gDryfieldToiletUnusedWomanCommands[2] = {
    { { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET } }, DRYFIELD_TOILET_WOMAN_COMMAND_TURN },
    { { .loc = { GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET } }, DRYFIELD_TOILET_WOMAN_COMMAND_SPRAY },
};

/// Unreferenced placements sharing a room position and differing by a quarter-turn yaw.
///
/// Coordinates use room units and angles use 4096 units per turn. Their
/// intended actor and association with the unused commands are unproven.
static ActorTransform _gDryfieldToiletUnusedActorPlacements[2] = {
    { { -1664, 0, -1222, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { { -1664, 0, -1222, 0 }, { 0, 0, 0, 0 } },
};

/// Encounter stream key: group 2, stream 33, qualifier 11 (second qualifier zero).
static EvsSceneKey _gDryfieldToiletEncounterSceneKey = { .group = 2, .streamId = 33, .subId = 11 };

EvsCommand gDryfieldToiletEncounterScript[31] = {
    // Stage the stream and replace the equipped bank's encounter clips.
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gDryfieldToiletEncounterSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldToiletStageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldToiletPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldToiletPlayerEventAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_ODD_STRANGER_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldToiletOddStrangerCommand1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_WOMAN_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldToiletPlayerEventAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Substitute the transforming actor and play the timed encounter views.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_WOMAN_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldToiletSpawnLesserStrangerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_WOMAN_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_TOILET_WOMAN_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 6) }, { .value = SOUND_SCRIPT_STOP_KEEP_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldToiletStartScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 132 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Commit the battle setup before returning player control.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldToiletEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_WOMAN_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldToiletReleaseLesserStrangerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldToiletPlayerBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_ODD_STRANGER_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldToiletOddStrangerAmbushCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldToiletMoveEventCollision }, { .value = DRYFIELD_TOILET_EVENT_COLLISION_MOVE_ASIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldToiletFinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDryfieldToiletEncounterSkipScript[20] = {
    // Cancel playback behind the fade and apply the encounter's final placements.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldToiletCancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_WOMAN_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = DRYFIELD_TOILET_WOMAN_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_WOMAN_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldToiletReleaseLesserStrangerCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldToiletPlayerBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldToiletPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldToiletPlayerSkipAnimation9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_TOILET_ODD_STRANGER_PLACEMENT_INDEX }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldToiletOddStrangerAmbushCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldToiletMoveEventCollision }, { .value = DRYFIELD_TOILET_EVENT_COLLISION_MOVE_ASIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Reload view 3 and finish the fade before engaging the battle.
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldToiletEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

WorldCollisionRoomResources gDryfieldToiletRoomCollisionResources[1] = {
    {
        .grid                 = &gDryfieldToiletRoomCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldToiletViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldToiletActionTriggers,
        .occluders            = _gDryfieldToiletSightOccluders,
    },
};

u8* gDryfieldToiletRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldToiletRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldToiletViewCameras) };

WorldCoordRoomLighting gDryfieldToiletRoomLighting[1] = {
    { &_gDryfieldToiletRoomLights, NULL },
};

DirectionWarpEntry gDryfieldToiletWarpEntries[1] = {
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 2 }, -1660, 0, 1653 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 2 }, -1660, 0, 1653 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_TOILET_DOOR_MAP_FLAG,
    },
};

/// Seven room-grid normals in Q12 units; normal zero belongs to the event obstacle.
static SVECTOR _gDryfieldToiletRoomCollisionGridNormals[7] = {
#include "assets/dryfield_toilet_collision_03E44_normals.inc"
};

/// Thirty-eight room-grid vertices; the event obstacle occupies vertices 0..3.
static SVECTOR _gDryfieldToiletRoomCollisionGridVerts[38] = {
#include "assets/dryfield_toilet_collision_03E44_verts.inc"
};

/// Seventeen indexed room faces; the initially empty first face is restored for the event.
static WorldCollisionGridFace _gDryfieldToiletRoomCollisionGridFaces[17] = {
#include "assets/dryfield_toilet_collision_03E44_faces.inc"
};

/// Serialized signed face-index lists for both cells, terminated by -1.
///
/// The final stored zero follows both terminated lists and is not traversed.
static s16 _gDryfieldToiletRoomCollisionGridCells[34] = {
#include "assets/dryfield_toilet_collision_03E44_cells.inc"
};

// The generated initializer supplies a constant index into the signed cell lists.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldToiletRoomCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..33 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_TOILET_ROOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldToiletRoomCollisionGridCells[(halfwordOffset)])
/// X-major cell-list pointers for the grid's one-by-two XZ partition.
static s16* _gDryfieldToiletRoomCollisionGridTable[2] = {
#include "assets/dryfield_toilet_collision_03E44_table.inc"
};
#undef DRYFIELD_TOILET_ROOM_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gDryfieldToiletRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldToiletRoomCollisionGridNormals,
    .vertices    = _gDryfieldToiletRoomCollisionGridVerts,
    .faces       = _gDryfieldToiletRoomCollisionGridFaces,
    .cellFaceIds = _gDryfieldToiletRoomCollisionGridTable,
    .xBias       = 2970,
    .zBias       = 2300,
    .cellCountX  = 1,
    .cellCountZ  = ARRAY_SIZE(_gDryfieldToiletRoomCollisionGridTable),
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldToiletRoomCollisionGridFaces),
};

ViewCamera gDryfieldToiletViewCameras[11] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x7530, 0 } }, 1524 },
    { { { { 4000, 0, -880 }, { -628, 2868, -2855 }, { 616, 2924, 2801 } }, { 2049, 2467, 57 } }, 230 },
    { { { { -3969, 0, -1008 }, { -446, 3672, 1757 }, { 904, 1813, -3559 } }, { 2094, 1926, -1868 } }, 230 },
    { { { { -4005, 0, 858 }, { 107, 4063, 502 }, { -851, 513, -3973 } }, { -515, 1179, -1469 } }, 207 },
    { { { { 3989, 0, 927 }, { -344, 3803, 1481 }, { -861, -1521, 3704 } }, { -488, 203, 1520 } }, 207 },
    { { { { 863, 0, -4003 }, { -3600, 1791, -776 }, { 1751, 3683, 377 } }, { -1314, 2467, 1776 } }, 207 },
    { { { { -152, 0, -4093 }, { -3445, 2211, 128 }, { 2210, 3447, -82 } }, { -1153, 2360, -518 } }, 207 },
    { { { { -977, 0, -3977 }, { -3279, 2317, 805 }, { 2250, 3377, -552 } }, { -1153, 2360, -1708 } }, 207 },
    { { { { -4095, 0, -66 }, { -26, 3759, 1626 }, { 60, 1626, -3758 } }, { 1655, 1744, -488 } }, 230 },
    { { { { -4003, 0, 865 }, { -160, 4025, -742 }, { -850, -759, -3934 } }, { 1455, 734, -688 } }, 329 },
    { { { { -3984, 0, 947 }, { 393, 3725, 1656 }, { -862, 1702, -3624 } }, { 1635, 1684, 31 } }, 338 },
};

/// Empty batch list for mapped view 1, retaining decoded image strips.
static SpriteBatch _gDryfieldToiletView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped view 2; rectangles use drawing-origin pixels.
static SpriteSource _gDryfieldToiletView2SpriteSources[45] = {
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 325, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 80, 392, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -88, 96, 350, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -80, 72, 300, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 72, 325, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 32, -120, 687, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 24, -120, 700, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -48, 716, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -120, 413, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 48, 478, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 88, 500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -56, 435, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, -120, 374, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, -56, 379, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 0, 388, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 48, 426, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 88, 443, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -120, 342, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 128, -56, 348, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 0, 345, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 48, 389, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 397, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -120, 315, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -56, 330, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 0, 337, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 48, 351, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 370, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 56, -120, 575, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -24, 663, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -120, 537, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -24, 618, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -16, 582, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -120, 509, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 80, -8, 551, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 88, 16, 667, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -32, 710, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 48, -120, 582, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 40, -120, 583, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 40, -40, 757, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 24, 465, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 0, 452, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -56, 453, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -120, 448, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -120, 484, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, -72, 472, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
};

/// Draw batches for mapped view 2, ending at SPRITE_BATCH_END.
static SpriteBatch _gDryfieldToiletView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, ARRAY_SIZE(_gDryfieldToiletView2SpriteSources) - 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped view 3; rectangles use drawing-origin pixels.
static SpriteSource _gDryfieldToiletView3SpriteSources[47] = {
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 40, 0, 525, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 24, 16, 400, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 56, 650, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, 72, 625, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, -24, 800, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -40, 825, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 40, 16, 450, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, 40, 40, 375, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 40, 64, 300, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, 40, 80, 300, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 56, -16, 600, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, -40, 825, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -40, -120, 691, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -32, -96, 766, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -48, -120, 659, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -56, -120, 611, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -64, -120, 569, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -72, -120, 533, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -80, -120, 501, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -88, -120, 472, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 40, 761, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 40, 715, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 40, 660, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 40, 610, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 40, 565, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 40, 528, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 40, 494, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, -8, 442, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, -8, 376, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -8, 327, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, -64, 325, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -136, -64, 369, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -112, -64, 425, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, -120, 315, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -136, -120, 356, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -112, -120, 409, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 40, 334, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -136, 40, 390, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, 40, 442, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 64, 338, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, 64, 377, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -120, 64, 415, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 64, 450, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 336, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 88, 366, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 88, 403, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 88, 432, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
};

/// Draw batches for mapped view 3, ending at SPRITE_BATCH_END.
static SpriteBatch _gDryfieldToiletView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldToiletView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped view 4; rectangles use drawing-origin pixels.
static SpriteSource _gDryfieldToiletView4SpriteSources[13] = {
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 56, -32, 575, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, 64, -104, 513, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, 72, -104, 495, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, 80, -112, 457, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 208 } }, 88, -112, 445, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 96, -120, 421, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 104, -120, 389, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 112, -120, 364, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 120, -120, 341, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 128, -120, 316, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 136, -120, 276, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 144, -120, 263, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, 152, -120, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

/// Draw batches for mapped view 4, ending at SPRITE_BATCH_END.
static SpriteBatch _gDryfieldToiletView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldToiletView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped view 5; rectangles use drawing-origin pixels.
static SpriteSource _gDryfieldToiletView5SpriteSources[39] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 112, 402, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, 48, 765, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, 40, 600, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -96, 24, 500, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -160, 8, 327, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -128, 16, 402, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 96, 397, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -56, 700, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, -8, 650, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -64, 612, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -8, 587, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -80, 616, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -88, 577, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -16, 510, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, -16, 550, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -104, 600, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -40, 500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 525, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -40, 475, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -56, 425, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -120, 481, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -120, 362, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 64 } }, -160, -56, 337, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, -56, 362, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -56, 400, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, -120, 375, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -112, -120, 425, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -8, 56, 850, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 836, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 880, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -8, -24, 956, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -24, 1020, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, -24, 883, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, 0, 650, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -24, 24, 662, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, -24, 821, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, 16, 685, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -16, 24, 902, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -16, 0, 8400, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

/// Draw batches for mapped view 5, ending at SPRITE_BATCH_END.
static SpriteBatch _gDryfieldToiletView5SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 3, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 2, 0 } },
    { 27, ARRAY_SIZE(_gDryfieldToiletView5SpriteSources) - 27, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 5 clipping rectangle in draw-buffer pixels, restored at depth 875.
static SpriteDrawArea _gDryfieldToiletView5DrawAreas[2] = {
    { { 150, 0, 168, 239 }, 875 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Empty batch list for mapped view 6, retaining decoded image strips.
static SpriteBatch _gDryfieldToiletView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty batch list for mapped view 7, retaining decoded image strips.
static SpriteBatch _gDryfieldToiletView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty batch list for mapped view 8, retaining decoded image strips.
static SpriteBatch _gDryfieldToiletView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty batch list for mapped view 9, retaining decoded image strips.
static SpriteBatch _gDryfieldToiletView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty batch list for mapped view 10, retaining decoded image strips.
static SpriteBatch _gDryfieldToiletView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty batch list for mapped view 11, retaining decoded image strips.
static SpriteBatch _gDryfieldToiletView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldToiletSpriteViews[11] = {
    { { .empty = _gDryfieldToiletView1SpriteBatches }, _gDryfieldToiletView1SpriteBatches, NULL },
    { { .elements = _gDryfieldToiletView2SpriteSources }, _gDryfieldToiletView2SpriteBatches, NULL },
    { { .elements = _gDryfieldToiletView3SpriteSources }, _gDryfieldToiletView3SpriteBatches, NULL },
    { { .elements = _gDryfieldToiletView4SpriteSources }, _gDryfieldToiletView4SpriteBatches, NULL },
    { { .elements = _gDryfieldToiletView5SpriteSources }, _gDryfieldToiletView5SpriteBatches, _gDryfieldToiletView5DrawAreas },
    { { .empty = _gDryfieldToiletView6SpriteBatches }, _gDryfieldToiletView6SpriteBatches, NULL },
    { { .empty = _gDryfieldToiletView7SpriteBatches }, _gDryfieldToiletView7SpriteBatches, NULL },
    { { .empty = _gDryfieldToiletView8SpriteBatches }, _gDryfieldToiletView8SpriteBatches, NULL },
    { { .empty = _gDryfieldToiletView9SpriteBatches }, _gDryfieldToiletView9SpriteBatches, NULL },
    { { .empty = _gDryfieldToiletView10SpriteBatches }, _gDryfieldToiletView10SpriteBatches, NULL },
    { { .empty = _gDryfieldToiletView11SpriteBatches }, _gDryfieldToiletView11SpriteBatches, NULL },
};

/// Directed view boundaries joining 2/3, 3/4 and 4/5, including the final marked record.
///
/// Room setup binds and links these mutable quads; positions/radii use room
/// units and normals use Q12. Source and destination are 1-based view slots.
static WorldCollisionTrigger _gDryfieldToiletViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { 127, -992, 6, 0 }, { { -1024, -2016, 0, 0 }, { 1025, -2016, 0, 0 }, { -1024, 2016, 0, 0 }, { 1025, 2016, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 2260, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 128, -1056, -96, 0 }, { { 1024, -2080, 0, 0 }, { -1024, -2080, 0, 0 }, { 1024, 2080, 0, 0 }, { -1024, 2080, 0, 0 } }, { 0, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1184, -1088, -1568, 0 }, { { -157, -2112, -1018, 0 }, { 145, -2112, 1008, 0 }, { -157, 2112, -1018, 0 }, { 145, 2112, 1008, 0 } }, { 4052, 0, -606, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1120, -800, -1600, 0 }, { { 149, -1824, 1011, 0 }, { -152, -1824, -1014, 0 }, { 149, 1824, 1011, 0 }, { -152, 1824, -1014, 0 } }, { -4051, 0, 601, 0 }, { 0, 0, 4096, 0 }, 2079, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1857, -736, 799, 0 }, { { -1004, -1760, 199, 0 }, { 1005, -1760, -199, 0 }, { -1004, 1760, 199, 0 }, { 1005, 1760, -199, 0 } }, { -799, 0, -4019, 0 }, { 0, 0, 4096, 0 }, 2035, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1857, -1072, 638, 0 }, { { 1016, -2096, -53, 0 }, { -1027, -2096, 45, 0 }, { 1016, 2096, -53, 0 }, { -1027, 2096, 45, 0 } }, { 196, 0, 4115, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Door warp, CAP interactions and the once-only encounter region for this room.
///
/// Nine mutable room-unit quads end with LAST on the final included record.
/// The door requests area 15 through local warp 1 and destination warp 3;
/// the encounter region sends room action 1. Other records select CAP commands.
static WorldCollisionTrigger _gDryfieldToiletActionTriggers[9] = {
    { NULL, NULL, NULL, { -1808, -48, 1728, 0 }, { { -656, 0, -256, 0 }, { 656, 0, -256, 0 }, { -656, 0, 256, 0 }, { 656, 0, 256, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 704, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_PARKING_LOT, (DRYFIELD_TOILET_DOOR_WARP_SLOT << 4) | DRYFIELD_TOILET_PARKING_LOT_ARRIVAL_WARP_SLOT, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 729, -64, -1472, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 765, -64, -480, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 764, -64, 448, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 768, -64, 1408, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1824, -64, -1296, 0 }, { { -432, 0, -112, 0 }, { 848, 0, -112, 0 }, { -432, 0, 1232, 0 }, { 848, 0, 1232, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -400, -64, 544, 0 }, { { -512, 0, -1472, 0 }, { 512, 0, -1472, 0 }, { -512, 0, 1472, 0 }, { 512, 0, 1472, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 1557, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1088, -64, -480, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1856, -64, -1344, 0 }, { { -320, 0, -704, 0 }, { 320, 0, -704, 0 }, { -320, 0, 704, 0 }, { 320, 0, 704, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 773, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// One mutable room-space quad blocking actor line of sight, with the final-record flag.
///
/// Geometry uses room units and a Q12 normal. Low flag bit 0 is preserved;
/// its role is unproven. The quad must remain loaded until it is unlinked.
static WorldCollisionOccluder _gDryfieldToiletSightOccluders[1] = {
    { NULL, NULL, { -864, -1488, 624, 0 }, { { 0, 1904, -1520, 0 }, { 0, -1904, -1520, 0 }, { 0, 1904, 1520, 0 }, { 0, -1904, 1520, 0 } }, { 4109, 0, 0, 0 }, 2428, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Four all-view point lights, with world-unit positions/radii and Q12 RGB intensities.
static WorldCoordPointLight _gDryfieldToiletPointLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -810, -1260, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 0x186A0, 0x186A0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2290, -1470, -510 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -310, -1470, -1790 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 110, -1470, 1770 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 2000 },
};

/// Single room light collection, with point sources only.
static WorldCoordRoomLights _gDryfieldToiletRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gDryfieldToiletPointLights),
    _gDryfieldToiletPointLights,
    0,
    NULL,
};

/// Variant-1 resources for placed woman entry 9 and Odd Stranger entry 10.
///
/// Loads actor_323300 and actor_401000 and borrows their spawn descriptors.
/// The woman descriptor also exposes its temporary Lesser Stranger in slot 1.
/// AREA_PLACEMENT_END terminates the table; keep resources live through actor use.
static AreaResource _gDryfieldToiletEncounterResources[3] = {
    { 10, 10, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401000StrangerTaskDesc },
    { 9, 233, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, gActor323300WomanStrangerTaskDescriptors },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-2 resource for entry 12, shared by seven Bone Suckler placements.
///
/// Loads actor_101200 and uses its body descriptor. Includes AREA_PLACEMENT_END.
static AreaResource _gDryfieldToiletBoneSucklerResources[2] = {
    { 12, 12, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101200BoneSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant-7 resource for entry 24, shared by three Amoeba placements.
///
/// Loads actor_102400 and uses descriptor 0, the body rather than its fireball.
/// Includes AREA_PLACEMENT_END; keep the borrowed descriptor table loaded.
static AreaResource _gDryfieldToiletAmoebaResources[2] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldToiletAreaVariants[DRYFIELD_TOILET_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldToiletEncounterPlacements, .resources = _gDryfieldToiletEncounterResources },
    [2]                    = { .placements = gMapDryfieldToiletBoneSucklerPlacements, .resources = _gDryfieldToiletBoneSucklerResources },
    [7]                    = { .placements = gMapDryfieldToiletAmoebaPlacements, .resources = _gDryfieldToiletAmoebaResources },
};

/// Full-ramp XYZ displacement of each Lesser Stranger vertex in model-local units.
static SVECTOR _gDryfieldToiletLesserStrangerMorphVertexDeltas[325] = {
#include "assets/dryfield_toilet_morph_053C0.inc"
};

/// Full-ramp Lesser Stranger normal targets, with Q12 XYZ components.
///
/// Only the first 1604 serialized vectors are blended. The final
/// (0, 0, 0, 1604) record is retained; its purpose is unproven.
static SVECTOR _gDryfieldToiletLesserStrangerMorphTargetNormals[1605] = {
#include "assets/dryfield_toilet_morph_05DE8.inc"
};

ModelMorph gDryfieldToiletLesserStrangerMorph = {
    .vertexDeltas     = _gDryfieldToiletLesserStrangerMorphVertexDeltas,
    .targetNormals    = _gDryfieldToiletLesserStrangerMorphTargetNormals,
    .savedVertices    = _gDryfieldToiletLesserStrangerMorphSavedVertices,
    .savedNormals     = _gDryfieldToiletLesserStrangerMorphSavedNormals,
    .savedVertexCount = ARRAY_SIZE(_gDryfieldToiletLesserStrangerMorphVertexDeltas),
    .normalCount      = DRYFIELD_TOILET_LESSER_STRANGER_MORPH_NORMAL_COUNT,
    .firstVertex      = 0,
    .deltaCount       = ARRAY_SIZE(_gDryfieldToiletLesserStrangerMorphVertexDeltas),
};

/// Cue-2 script entries in the loaded type-1 bank; cue 1 uses the next entry.
enum {
    DRYFIELD_TOILET_FOOTSTEP_WALK_ENTRY = 1,
    DRYFIELD_TOILET_FOOTSTEP_RUN_ENTRY  = 3,
};

/// Paired walk and run cue bases, with scripted jumps using the walk pair.
static WorldCollisionFootstepSounds _gDryfieldToiletFootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_TOILET_FOOTSTEP_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_TOILET_FOOTSTEP_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_TOILET_FOOTSTEP_WALK_ENTRY,
};

/// Blocking, weapon-impact surface for classes 0 and 3..7, without footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldToiletSilentBlockingSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class-1 blocking, weapon-impact surface with the room's footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldToiletFootstepBlockingSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldToiletFootstepSounds,
};

/// Class-2 probe-pass surface with footstep cues, pushback and no weapon impacts.
static WorldCollisionSurfaceProperties _gDryfieldToiletFootstepPassThroughSurface = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldToiletFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldToiletSurfaceClasses[8] = {
    &_gDryfieldToiletSilentBlockingSurface,
    &_gDryfieldToiletFootstepBlockingSurface,
    &_gDryfieldToiletFootstepPassThroughSurface,
    &_gDryfieldToiletSilentBlockingSurface,
    &_gDryfieldToiletSilentBlockingSurface,
    &_gDryfieldToiletSilentBlockingSurface,
    &_gDryfieldToiletSilentBlockingSurface,
    &_gDryfieldToiletSilentBlockingSurface,
};

/// Rest-vertex XYZ snapshot shared by the transformation actor.
///
/// Only entries 0..324 are written or blended. The final stored vector's role
/// is unproven; it is retained without treating it as a model vertex.
static SVECTOR _gDryfieldToiletLesserStrangerMorphSavedVertices[326] = { 0 };

/// Rest-normal XYZ snapshot, initialized before blending and shared by the actor.
static SVECTOR _gDryfieldToiletLesserStrangerMorphSavedNormals[DRYFIELD_TOILET_LESSER_STRANGER_MORPH_NORMAL_COUNT] = { 0 };

/// Normalizes a spray offset to Q12, choosing a random direction for a zero offset.
///
/// Borrows writable effect work. Reads the signed XYZ offset in the emitter's
/// parent frame and writes `move` at 4096 units per unit length. A zero offset
/// consumes three LCG draws and replaces `pos` with components in [-2048,2047];
/// that random vector can itself be zero, so no nonzero direction is guaranteed.
/// The emitter has already copied its placement before this mutation.
static inline void _dryfieldToiletNormalizeSprayDirection(EffectWork* work)
{
    enum { SPRAY_RANDOM_DIRECTION_MASK = 0xFFF,
           SPRAY_RANDOM_DIRECTION_BIAS = 0x800 };

    if ((work->pos.vx | work->pos.vy | work->pos.vz) == 0) {
        RANDOM_LCG_ADVANCE();
        work->pos.vx = (RANDOM_LCG_DRAW() & SPRAY_RANDOM_DIRECTION_MASK) - SPRAY_RANDOM_DIRECTION_BIAS;
        RANDOM_LCG_ADVANCE();
        work->pos.vy = (RANDOM_LCG_DRAW() & SPRAY_RANDOM_DIRECTION_MASK) - SPRAY_RANDOM_DIRECTION_BIAS;
        RANDOM_LCG_ADVANCE();
        work->pos.vz = (RANDOM_LCG_DRAW() & SPRAY_RANDOM_DIRECTION_MASK) - SPRAY_RANDOM_DIRECTION_BIAS;
    }
    // The offset also supplies the spray direction, normalized to Q12.
    VectorNormalSS(&work->pos, &work->move);
}

void dryfieldToiletSprayEmitterTask(Task* task)
{
    enum { SPRAY_ATTACH                 = 0,
           SPRAY_EMIT                   = 1,
           SPRAY_INITIAL_SPEED          = 48,
           SPRAY_PUFF_SIZE              = 0x180,
           SPRAY_PUFF_FRAME_PERIOD      = 1 << 12,
           SPRAY_PUFF_UNINTERPRETED_BIT = 1 << 16,
           SPRAY_PUFF_PARAMETERS        = SPRAY_PUFF_SIZE | SPRAY_PUFF_FRAME_PERIOD | SPRAY_PUFF_UNINTERPRETED_BIT };

    EffectWork* work;
    GfxCoord*   coord;
    EffectWork* puffWork;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
            return;
        }
        if (task->state == SPRAY_ATTACH) {
            coord->parent       = work->parent;
            coord->coord.t[0]   = work->pos.vx;
            coord->coord.t[1]   = work->pos.vy;
            coord->coord.t[2]   = work->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = SPRAY_EMIT;
            work->scale         = SPRAY_INITIAL_SPEED;
            work->angle         = task->spawnArg1.value;
            _dryfieldToiletNormalizeSprayDirection(work);
        }
        // Children inherit the live attachment; their velocity decreases by age.
        actorRenderComposeCoord(coord);
        puffWork = effectSpawn(EFFECT_DRYFIELD_TOILET_JET_PUFF, coord, SPRAY_PUFF_PARAMETERS, 0);
        if (puffWork != NULL) {
            gte_lddp(work->scale - work->age);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&puffWork->move);
        }
        work->age++;
        if (work->age < work->angle) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// Sets the screen corners of a jet puff's rotated square around its projected centre.
///
/// `projection` must be a live scratch block with a screen centre and positive
/// depth (SZ3 / 4). The signed halfword sources supply size and an angle in 4096
/// units per turn; this puff initializes both to 0..4095. Size times the cell's
/// 31-texel UV span divided by depth gives the half-diagonal in pixels before
/// rotation.
/// Each component rereads the sources and truncates the signed division before
/// its Q12 trig product. The second diagonal leaves its offsets in `projection`;
/// screen coordinates narrow to packet halfwords. No pointers are retained.
static inline void _dryfieldToiletSetJetPuffCorners(POLY_FT4* quad, OverlaySpriteScratch* projection, const s16* sizeSource, const s16* angleSource)
{
    enum { PUFF_SIZE_PROJECTION_SCALE = 31 }; // UV span of the 32-texel puff cell

    // Place opposite corners on each of two perpendicular screen-space diagonals.
    projection->cornerDx = (((*sizeSource * PUFF_SIZE_PROJECTION_SCALE) / projection->otz) * rsin(*angleSource)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS;
    projection->cornerDy = (((*sizeSource * PUFF_SIZE_PROJECTION_SCALE) / projection->otz) * rcos(*angleSource)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS;
    quad->x0             = projection->screenPos.vx + projection->cornerDx;
    quad->x3             = projection->screenPos.vx - projection->cornerDx;
    quad->y0             = projection->screenPos.vy - projection->cornerDy;
    quad->y3             = projection->screenPos.vy + projection->cornerDy;
    projection->cornerDx = (((*sizeSource * PUFF_SIZE_PROJECTION_SCALE) / projection->otz) * rsin(*angleSource + ROOM_VISUAL_EFFECTS_FULL_TURN / 4)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS;
    projection->cornerDy = (((*sizeSource * PUFF_SIZE_PROJECTION_SCALE) / projection->otz) * rcos(*angleSource + ROOM_VISUAL_EFFECTS_FULL_TURN / 4)) >> ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS;
    quad->x1             = projection->screenPos.vx + projection->cornerDx;
    quad->x2             = projection->screenPos.vx - projection->cornerDx;
    quad->y1             = projection->screenPos.vy - projection->cornerDy;
    quad->y2             = projection->screenPos.vy + projection->cornerDy;
}

void dryfieldToiletJetPuffTask(Task* task)
{
    enum {
        PUFF_INITIALIZE,
        PUFF_STATIONARY,
        PUFF_DRIFTING,
        PUFF_SIZE_MASK               = 0xFFF,
        PUFF_PERIOD_MASK             = 0xF000,
        PUFF_PERIOD_SHIFT            = 12,
        PUFF_RANDOM_MOVE_FLAG        = 0x100000,
        PUFF_SCALE_MOVE_FLAG         = 0x01000000,
        PUFF_RANDOM_MOVE_MASK        = 0x1F,
        PUFF_RANDOM_MOVE_BIAS        = 16,
        PUFF_FALL_ACCELERATION       = 3,
        PUFF_MIN_DEPTH               = 17,
        PUFF_TEXTURE_FRAME_COUNT     = 6,
        PUFF_TEXTURE_CELL_SHIFT      = 5,
        PUFF_TEXTURE_ROW             = 0x40,
        PUFF_TEXTURE_CELL_LAST       = 31,
        PUFF_TEXTURE_PAGE            = 0x2B,
        PUFF_TEXTURE_PALETTE         = 0x43C0,
        PUFF_RAW_TEXTURE_BLEND_FLAGS = 3
    };

    EffectWork*           work;
    GfxCoord*             coord;
    OverlaySpriteScratch* scratchEnd;
    OverlaySpriteScratch* projection;
    POLY_FT4*             quad;
    s32                   randomBits;
    s16                   framePeriod;
    SVECTOR*              velocity;
    u16                   worldX;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    // Project the centre and reserve its packet before advancing the effect.
    actorRenderComposeCoord(coord);
    scratchEnd = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
    worldX     = coord->workm.t[0];
    SCRATCH_STACK_RESERVE_BLOCK(OverlaySpriteScratch);
    projection              = SCRATCH_STACK_CURSOR(OverlaySpriteScratch);
    projection->worldPos.vx = worldX;
    projection->worldPos.vy = coord->workm.t[1];
    projection->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&(scratchEnd - 1)->worldPos);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&(scratchEnd - 1)->screenPos);
    gte_stszotz(&projection->otz);
    if ((scratchEnd - 1)->otz >= PUFF_MIN_DEPTH) {
        if (task->state == PUFF_INITIALIZE) {
            randomBits  = RANDOM_LCG_NEXT(gRandomLcgState);
            work->scale = task->spawnArg1.value & PUFF_SIZE_MASK;
            work->angle = ((u32)randomBits >> 16) & (ROOM_VISUAL_EFFECTS_FULL_TURN - 1);
            // Keep the signed halfword: nibble values 8..15 decode as negative.
            framePeriod     = task->spawnArg1.value & PUFF_PERIOD_MASK;
            gRandomLcgState = randomBits;
            if (framePeriod != 0) {
                framePeriod = framePeriod >> PUFF_PERIOD_SHIFT;
            } else {
                framePeriod = 1;
            }
            work->period = framePeriod;
            if (task->spawnArg1.value & PUFF_RANDOM_MOVE_FLAG) {
                RANDOM_LCG_ADVANCE();
                work->move.vx = (RANDOM_LCG_DRAW() & PUFF_RANDOM_MOVE_MASK) - PUFF_RANDOM_MOVE_BIAS;
                RANDOM_LCG_ADVANCE();
                work->move.vy = (RANDOM_LCG_DRAW() & PUFF_RANDOM_MOVE_MASK) - PUFF_RANDOM_MOVE_BIAS;
                RANDOM_LCG_ADVANCE();
                work->move.vz = (RANDOM_LCG_DRAW() & PUFF_RANDOM_MOVE_MASK) - PUFF_RANDOM_MOVE_BIAS;
            }
            if (task->spawnArg1.value & PUFF_SCALE_MOVE_FLAG) {
                gte_lddp(work->scale << 2);
                velocity = &work->move;
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            }
            task->state = PUFF_STATIONARY;
            if (work->move.vx | work->move.vy | work->move.vz) {
                task->state = PUFF_DRIFTING;
            }
        }
        quad->tpage = PUFF_TEXTURE_PAGE;
        quad->clut  = PUFF_TEXTURE_PALETTE;
        quad->code |= PUFF_RAW_TEXTURE_BLEND_FLAGS;
        setUVWH(quad, (work->age / work->period) << PUFF_TEXTURE_CELL_SHIFT, PUFF_TEXTURE_ROW,
                PUFF_TEXTURE_CELL_LAST, PUFF_TEXTURE_CELL_LAST);
        _dryfieldToiletSetJetPuffCorners(quad, projection, &work->scale, &work->angle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
    // Pausing freezes age and drift after drawing; cancellation also draws once.
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
            return;
        }
        if (task->state == PUFF_DRIFTING) {
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->move.vy      += PUFF_FALL_ACCELERATION;
        }
        work->age++;
        if (work->age <= work->period * PUFF_TEXTURE_FRAME_COUNT - 1) {
            return;
        }
    }
    effectKillTask(work, task);
}

void dryfieldToiletConfigureEffectsTask(Task* task)
{
    enum { EFFECTS_UNREGISTERED,
           EFFECTS_REGISTERED };

    if (task->state == EFFECTS_UNREGISTERED) {
        gRoomEffectGlowDiscId     = EFFECT_DRYFIELD_TOILET_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_DRYFIELD_TOILET_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_DRYFIELD_TOILET_ORANGE_BURST_2;
        task->state               = EFFECTS_REGISTERED;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

void dryfieldToiletRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void dryfieldToiletFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room/room_visual_effects_burst.inc.c"

void dryfieldToiletFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
