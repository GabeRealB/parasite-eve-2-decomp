#include "rooms/akropolis_square.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../../../shared/room/room_cutscene.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// The scene sub-task while it runs, NULL otherwise.
extern Task* gRoomCutsceneSoundTask;

/// Defines the reflection scale at the shared implementation's include position.
///
/// 1 lets `planar_reflection.inc.c` include `planar_reflection_rodata.inc.c`.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 1
#include "../../../../shared/room/planar_reflection.h"

#include "../../../../shared/room/telephone.h"

static void _akropolisSquareInitializeRoomTask(Task* task);
static void _akropolisSquareIdleRoomTask(Task* unusedTask);

static s32  _akropolisSquareResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _akropolisSquareHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _akropolisSquareHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32  _akropolisSquareRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _akropolisSquareHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg);
static s32  _akropolisSquareSetTelephoneLightMode(Task* unusedTask, s32 unusedMessageId, s32 glowMode, s32 unusedSecondArg);
static void _akropolisSquareTelephoneRingTask(Task* task);
static void _akropolisSquareScrollingBackdropTask(Task* task);
static void _akropolisSquareCapViewSequenceTask(Task* task);
static void _akropolisSquareControlScrollingBackdrop(s32 action);

static WorldCollisionGrid         _gAkropolisSquareCollisionGrid;
static WorldCollisionTrigger      _gAkropolisSquareViewBoundaryTriggers[16];
static WorldCollisionTrigger      _gAkropolisSquareActionTriggers[26];
static WorldCoordRoomAmbientEntry _gAkropolisSquareAmbientTable[16];
static WorldCoordRoomLights       _gAkropolisSquareRoomLights;

#include "../../../../shared/room/planar_reflection_data.inc.c"

/// Bodyless reflection spawn recipes, indexed as player (0) or attachment (1).
///
/// There is no terminator. Spawning copies a descriptor; the callbacks and
/// borrowed player geometry must remain live through the reflection's lifetime.
static TaskDesc _gAkropolisSquareReflectionTaskDescs[2] = {
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_REFLECTION_TASK_PRIORITY } }, akropolisSquarePlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_REFLECTION_TASK_PRIORITY } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return _gAkropolisSquareReflectionTaskDescs;
}

#include "../../../../shared/room/telephone_data.inc.c"

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Room-transition, interaction, key-item, command and sound-cue dispatch.
///
/// The room task borrows this terminated list while the overlay is loaded.
/// Payload types belong to the selected message; unsupported IDs return zero.
static TaskMessageEntry _gAkropolisSquareRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisSquareResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisSquareHandleRoomAction },
    { AKROPOLIS_SQUARE_MESSAGE_USE_KEY_ITEM, _akropolisSquareRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _akropolisSquareHandleRoomCommand },
    { ROOM_MESSAGE_SOUND, _akropolisSquareHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Player weapon-bank clip for the arrival presentation, restarting without grid collision.
///
/// EVS copies this request and replaces source.index with the equipped weapon's
/// bank. Reset playback ignores blendFrames; the clip's visual role is unproven.
static AnimationPlayRequest _gAkropolisSquareArrivalWeaponAnimation = {
    .source               = { .index = 1 },
    .animationId          = AKROPOLIS_SQUARE_ARRIVAL_ANIMATION_ID,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Bodyless telephone-ring (0), CAP/view (1) and scrolling-backdrop (2) spawn recipes.
///
/// No terminator; callers must select 0..2. The middle recipe has no current
/// spawner. Spawn arguments are zero in the live telephone and backdrop callers.
static TaskDesc _gAkropolisSquareSequenceTaskDescs[3] = {
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_SEQUENCE_TASK_PRIORITY } }, _akropolisSquareTelephoneRingTask, { .value = 0 } },
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_SEQUENCE_TASK_PRIORITY } }, _akropolisSquareCapViewSequenceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, AKROPOLIS_SQUARE_BACKDROP_TASK_PRIORITY } }, _akropolisSquareScrollingBackdropTask, { .value = 0 } },
};

/// Pending telephone call (0 none, 1 ringing and not yet answered), shared by the singleton room sequences.
///
/// Executed exits use it to select follow-up dialogue without clearing it.
/// Cutscene handoff consumes it as skipScene; ending the ringing away from its
/// follow-up views clears it. Overlay loading supplies the initial zero.
static s32 _gAkropolisSquareTelephoneCallPending = 0;

/// Arrival-event latch (0 unstarted, 1 started), reset only by loading the overlay.
static s32 _gAkropolisSquareArrivalEventStarted = 0;

/// Actions of the scrolling backdrop, sent by the arrival scene's script.
enum {
    AKROPOLIS_SQUARE_BACKDROP_ACTION_START = 0,
    AKROPOLIS_SQUARE_BACKDROP_ACTION_STOP  = 1,
};

/// Telephone scene: the phone's light in ringing mode, camera 14 while the grinning stranger
/// by the phone runs off, the scene's sound cue, then the previous view.
///
/// EVS borrows this END-terminated program and its skip program while running.
/// The waits are interpreter ticks; task code separately holds/releases the player.
static EvsCommand _gAkropolisSquareTelephoneSceneScript[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM_EFFECT }, { .value = 0 }, { .value = AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE }, { .value = AKROPOLIS_SQUARE_TELEPHONE_LIGHT_RINGING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = AKROPOLIS_SQUARE_TELEPHONE_EVENT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_TELEPHONE_SCENE) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = AKROPOLIS_SQUARE_TELEPHONE_EVENT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip completion for the telephone scene, restoring its previous view under a fade.
///
/// END-terminated, borrowed by EVS. Zero fade operands select average blending
/// and the interpreter's default seven-tick ramp; the eight-tick waits settle it.
static EvsCommand _gAkropolisSquareTelephoneSceneSkipScript[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = AKROPOLIS_SQUARE_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = AKROPOLIS_SQUARE_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Warp-7 arrival presentation: CAP 10, a weapon clip and the scrolling backdrop.
///
/// END-terminated and borrowed through playback. Starts at view 15; the backdrop
/// task finishes at view 13. The last message resumes normal player control.
static EvsCommand _gAkropolisSquareArrivalEventScript[8] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = AKROPOLIS_SQUARE_ARRIVAL_EVENT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = AKROPOLIS_SQUARE_ARRIVAL_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAkropolisSquareArrivalWeaponAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _akropolisSquareControlScrollingBackdrop }, { .value = AKROPOLIS_SQUARE_BACKDROP_ACTION_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = AKROPOLIS_SQUARE_ARRIVAL_EVENT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip completion for the arrival presentation, stopping the backdrop at view 13.
///
/// END-terminated and borrowed by EVS. Fade waits settle the default ramp;
/// cleanup aborts CAP and restores player drawing before player control resumes.
static EvsCommand _gAkropolisSquareArrivalSkipScript[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = AKROPOLIS_SQUARE_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = AKROPOLIS_SQUARE_ARRIVAL_SKIP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _akropolisSquareControlScrollingBackdrop }, { .value = AKROPOLIS_SQUARE_BACKDROP_ACTION_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = AKROPOLIS_SQUARE_SKIP_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Sound requests indexed by room cue 0..8, with no terminator or bounds check.
///
/// Cues 1 and 5..8 select this area's scripts. Cues 0 and 2..4 forward the
/// sound system's no-op request; this table and its reader retain signed words.
static s32 _gAkropolisSquareSoundCueScripts[9] = {
    SOUND_SCRIPT_REQUEST_NO_OP,
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_START),
    SOUND_SCRIPT_REQUEST_NO_OP,
    SOUND_SCRIPT_REQUEST_NO_OP,
    SOUND_SCRIPT_REQUEST_NO_OP,
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_SCRIPT_CUE_5),
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_SCENE),
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_END),
    AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_SCRIPT_CUE_8),
};

/// Terminated room-effect dispatch, accepting a signed-word telephone-light mode.
///
/// The effect task borrows the list and callback while this overlay is loaded.
/// Its final null callback also supplies the first glow ray's zero halfword;
/// retain its adjacency to `_gAkropolisSquareGlowSineSamples`.
static TaskMessageEntry _gAkropolisSquareRoomEffectMessages[2] = {
    { AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE, _akropolisSquareSetTelephoneLightMode },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Q12 sine samples at sixteenth-turn intervals, starting at minus a quarter turn.
///
/// Samples 0..22 cover the circular fan and rays; the final stored zero has no
/// reader. The first ray also reads index -2, the zero callback halfword in the
/// preceding `_gAkropolisSquareRoomEffectMessages`; its intent is unproven.
static s16 _gAkropolisSquareGlowSineSamples[24] = {
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    3784,
    4096,
    3784,
    2896,
    1567,
    0,
    -1567,
    -2896,
    -3784,
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    0,
};

/// Telephone light preset: 0 idle, 1 ringing - faster, larger glows with front streaks.
///
/// Room-effect initialization resets it. Messages preserve any signed word;
/// other values retain the packed spawn-word arithmetic rather than a bool test.
static s32 _gAkropolisSquareTelephoneLightMode = AKROPOLIS_SQUARE_TELEPHONE_LIGHT_IDLE;

WorldCollisionRoomResources gAkropolisSquareRoomCollisionResources[1] = {
    {
        .grid                 = &_gAkropolisSquareCollisionGrid,
        .viewBoundaryTriggers = _gAkropolisSquareViewBoundaryTriggers,
        .actionTriggers       = _gAkropolisSquareActionTriggers,
        .occluders            = NULL,
    },
};

u8* gAkropolisSquareViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gAkropolisSquareViewCounts[1] = { ARRAY_SIZE(gAkropolisSquareSpriteViews) };

WorldCoordRoomLighting gAkropolisSquareRoomLightingTable[1] = {
    { .lights = &_gAkropolisSquareRoomLights, .ambientTable = _gAkropolisSquareAmbientTable },
};

DirectionWarpEntry gAkropolisSquareWarpEntries[7] = {
    { { { .word = 1024 }, -6510, -2136, 511 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6510, -2136, 511 }, { 0, 0, 0, 0 }, AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_ARRIVAL), AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_DEPARTURE), AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_BLOCKED), 13, DIRECTION_WARP_FLAG_NONE, EVENT_FLAG_MAP_DOOR_AKROPOLIS_SQUARE_WEST_ELEVATOR },
    { { { .word = 3072 }, 6513, -2138, 140 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 6513, -2138, 140 }, { 0, 0, 0, 0 }, AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_ARRIVAL), AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_DEPARTURE), AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_BLOCKED), 6, DIRECTION_WARP_FLAG_NONE, EVENT_FLAG_MAP_DOOR_AKROPOLIS_SQUARE_EAST_ELEVATOR },
    { { { .word = 256 }, -4227, -1535, -3725 }, { 0, 0, 0, 0 }, { { .word = 256 }, -4227, -1535, -3725 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3840 }, 4199, -1535, -3916 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 4199, -1535, -3916 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 256 }, -4703, -1000, -4644 }, { 0, 0, 0, 0 }, { { .word = 256 }, -4419, -1200, -4340 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3840 }, 4905, -1000, -4576 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 4481, -1240, -4416 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -6510, -2136, 511 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6510, -2136, 511 }, { 0, 0, 0, 0 }, AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_ARRIVAL), AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_DEPARTURE), AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_ELEVATOR_BLOCKED), 15, DIRECTION_WARP_FLAG_NONE, EVENT_FLAG_MAP_DOOR_AKROPOLIS_SQUARE_WEST_ELEVATOR },
};

/// Room-space collision normals, with ONE (4096) for unit length.
static SVECTOR _gAkropolisSquareCollisionGridNormals[] = {
#include "assets/akropolis_square_collision_07BDC_normals.inc"
};

/// Collision vertices in signed room game coordinates.
static SVECTOR _gAkropolisSquareCollisionGridVerts[] = {
#include "assets/akropolis_square_collision_07BDC_verts.inc"
};

/// Indexed collision quads: 90 in surface class 0 and 25 in class 4.
static WorldCollisionGridFace _gAkropolisSquareCollisionGridFaces[] = {
#include "assets/akropolis_square_collision_07BDC_faces.inc"
};

/// Packed cell face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gAkropolisSquareCollisionGridCells[] = {
#include "assets/akropolis_square_collision_07BDC_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gAkropolisSquareCollisionGridCells` (506 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define AKROPOLIS_SQUARE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gAkropolisSquareCollisionGridCells[(halfwordOffset)])

/// X-major 4-by-4 directory of face lists in `_gAkropolisSquareCollisionGridCells`.
static s16* _gAkropolisSquareCollisionGridTable[] = {
#include "assets/akropolis_square_collision_07BDC_table.inc"
};
#undef AKROPOLIS_SQUARE_CELL_FACE_LIST_AT_OFFSET

/// The square's collision mesh and its 4-by-4 spatial index in the room XZ plane.
///
/// Cells have 4000-unit sides and origin (-7000, -8000); the directory index is
/// x * 4 + z. Room setup borrows this mutable record and binds its view coordinate.
/// The record, mesh pools and face lists stay live while this overlay is loaded.
static WorldCollisionGrid _gAkropolisSquareCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAkropolisSquareCollisionGridNormals,
    .vertices    = _gAkropolisSquareCollisionGridVerts,
    .faces       = _gAkropolisSquareCollisionGridFaces,
    .cellFaceIds = _gAkropolisSquareCollisionGridTable,
    .xBias       = 7000,
    .zBias       = 8000,
    .cellCountX  = 4,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAkropolisSquareCollisionGridFaces),
};

/// Sixteen directed camera boundaries, including the final LAST-marked quad.
///
/// Source/destination views are 3..8 and 13. Origins and vertices use room game
/// units; plane normals use Q12. Room setup binds coordinates, links and enables
/// every record. Unlink before unloading; hit and list links are runtime state.
static WorldCollisionTrigger _gAkropolisSquareViewBoundaryTriggers[16] = {
    { NULL, NULL, NULL, { -2050, -3681, 2655, 0 }, { { -1080, 2717, 3957, 0 }, { 1070, 2724, -3953, 0 }, { -1074, -2723, 3949, 0 }, { 1075, -2716, -3960, 0 } }, { 3952, 2, 1074, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2253, -3648, -4067, 0 }, { { -1346, 2813, 704, 0 }, { 1332, 2628, -777, 0 }, { -1339, -2627, 707, 0 }, { 1336, -2812, -780, 0 } }, { 1986, 4, 3590, 0 }, { 0, 4096, 0, 0 }, 3197, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1682, -3744, -3572, 0 }, { { 1535, 2813, 886, 0 }, { -1565, 2628, -890, 0 }, { 1535, -2627, 883, 0 }, { -1563, -2812, -896, 0 } }, { 2045, 1, -3574, 0 }, { 0, 4096, 0, 0 }, 3328, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2267, -3584, -3843, 0 }, { { 1497, 2813, -816, 0 }, { -1503, 2628, 766, 0 }, { 1486, -2627, -817, 0 }, { -1510, -2812, 762, 0 } }, { -1917, 3, -3632, 0 }, { 0, 4096, 0, 0 }, 3278, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4159, -3648, -770, 0 }, { { 3327, 2718, 116, 0 }, { -3328, 2723, -122, 0 }, { 3321, -2722, 116, 0 }, { -3335, -2717, -124, 0 } }, { 146, -1, -4110, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4222, -3648, -1250, 0 }, { { -3337, 2718, -120, 0 }, { 3322, 2723, 111, 0 }, { -3331, -2722, -117, 0 }, { 3329, -2717, 111, 0 } }, { -143, 1, 4111, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1635, -3712, 3102, 0 }, { { 763, 2718, -3250, 0 }, { -772, 2723, 3235, 0 }, { 761, -2722, -3245, 0 }, { -772, -2717, 3241, 0 } }, { -4005, 0, -949, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1660, -3616, 2814, 0 }, { { 531, 2718, 3289, 0 }, { -533, 2723, -3287, 0 }, { 531, -2722, 3285, 0 }, { -535, -2717, -3293, 0 } }, { 4060, 0, -658, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2269, -3680, 2750, 0 }, { { -697, 2718, -3264, 0 }, { 688, 2723, 3252, 0 }, { -696, -2722, -3259, 0 }, { 690, -2717, 3258, 0 } }, { -4025, 0, 855, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4579, -2464, -2338, 0 }, { { -4100, 2717, 117, 0 }, { 4094, 2724, -120, 0 }, { -4094, -2723, 119, 0 }, { 4100, -2716, -116, 0 } }, { 118, 1, 4094, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 8, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4675, -2432, -2050, 0 }, { { 4101, 2717, -116, 0 }, { -4094, 2724, 120, 0 }, { 4094, -2723, -119, 0 }, { -4100, -2716, 117, 0 } }, { -119, 2, -4095, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 4, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5648, -3456, 1024, 0 }, { { -859, 2717, 326, 0 }, { 850, 2724, -323, 0 }, { -848, -2723, 324, 0 }, { 859, -2716, -324, 0 } }, { 1460, 0, 3847, 0 }, { 0, 4096, 0, 0 }, 2862, 0, 8, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5249, -3488, -211, 0 }, { { 406, 2717, 983, 0 }, { -403, 2724, -972, 0 }, { 404, -2723, 973, 0 }, { -406, -2716, -981, 0 } }, { 3791, 1, -1570, 0 }, { 0, 4096, 0, 0 }, 2918, 0, 8, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5459, -3552, 1054, 0 }, { { 955, 2717, -367, 0 }, { -946, 2724, 365, 0 }, { 946, -2723, -364, 0 }, { -954, -2716, 368, 0 } }, { -1478, 0, -3835, 0 }, { 0, 4096, 0, 0 }, 2896, 0, 13, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5154, -3488, -306, 0 }, { { -482, 2717, -1106, 0 }, { 480, 2724, 1096, 0 }, { -480, -2723, -1096, 0 }, { 482, -2716, 1105, 0 } }, { -3758, 1, 1640, 0 }, { 0, 4096, 0, 0 }, 2974, 0, 13, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1696, -3648, -3826, 0 }, { { -1574, 2813, -843, 0 }, { 1560, 2628, 827, 0 }, { -1574, -2627, -836, 0 }, { 1560, -2812, 836, 0 } }, { -1936, 4, 3632, 0 }, { 0, 4096, 0, 0 }, 3318, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Twenty-six square interactions: warps, stair facing, CAP and room commands.
///
/// The final included quad has LAST set. Geometry uses room units and Q12
/// normals; control selects the byte payload interpretation. Room setup binds
/// and links the mutable records; hit latches persist until the list is cleared.
static WorldCollisionTrigger _gAkropolisSquareActionTriggers[26] = {
    { NULL, NULL, NULL, { -6416, -2240, 0, 0 }, { { -432, 0, -1088, 0 }, { 432, 0, -1088, 0 }, { -432, 0, 1088, 0 }, { 432, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_AKROPOLIS_WEST_ELEVATOR_HALL, AKROPOLIS_SQUARE_TRIGGER_WEST_ELEVATOR_WARPS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6320, -2237, -64, 0 }, { { -480, 0, -1024, 0 }, { 480, 0, -1024, 0 }, { -480, 0, 1024, 0 }, { 480, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_AKROPOLIS_EAST_ELEVATOR_HALL, AKROPOLIS_SQUARE_TRIGGER_EAST_ELEVATOR_WARPS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4161, -1664, -3873, 0 }, { { 489, 0, -473, 0 }, { 672, 0, 40, 0 }, { -671, 0, -39, 0 }, { -488, 0, 474, 0 } }, { 0, 4096, 0, 0 }, { 1189, 0, 3920, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_FACING | AKROPOLIS_SQUARE_TRIGGER_STAIR_DESCEND, 70, 144, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4286, -1667, -3969, 0 }, { { 704, 0, -39, 0 }, { 521, 0, 473, 0 }, { -519, 0, -473, 0 }, { -703, 0, 40, 0 } }, { 0, 4105, 0, 0 }, { -1380, 0, 3857, 0 }, 704, WORLD_COLLISION_TRIGGER_ACTION_FACING | AKROPOLIS_SQUARE_TRIGGER_STAIR_DESCEND, 70, 112, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4528, -2242, -1728, 0 }, { { 2128, 0, -176, 0 }, { 2128, 0, 176, 0 }, { -2128, 0, -176, 0 }, { -2128, 0, 176, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 2126, WORLD_COLLISION_TRIGGER_ACTION_FACING | AKROPOLIS_SQUARE_TRIGGER_STAIR_DESCEND, 67, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4753, -1632, -2481, 0 }, { { 2188, 0, -166, 0 }, { 2197, 0, 186, 0 }, { -2196, 0, -185, 0 }, { -2187, 0, 167, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2202, WORLD_COLLISION_TRIGGER_ACTION_FACING, 67, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4560, -2243, -1765, 0 }, { { 2064, 0, -176, 0 }, { 2064, 0, 176, 0 }, { -2064, 0, -176, 0 }, { -2064, 0, 176, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 2063, WORLD_COLLISION_TRIGGER_ACTION_FACING | AKROPOLIS_SQUARE_TRIGGER_STAIR_DESCEND, 67, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4574, -1638, -2432, 0 }, { { 2064, 0, -224, 0 }, { 2064, 0, 224, 0 }, { -2064, 0, -224, 0 }, { -2064, 0, 224, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 2063, WORLD_COLLISION_TRIGGER_ACTION_FACING, 67, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4433, -1376, -4385, 0 }, { { -1197, 1024, 433, 0 }, { 1198, 1024, -433, 0 }, { -1197, -1024, 433, 0 }, { 1198, -1024, -433, 0 } }, { 1399, 0, 3869, 0 }, { 1567, 0, 3784, 0 }, 1634, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_AKROPOLIS_PATIO, AKROPOLIS_SQUARE_TRIGGER_PATIO_WARPS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4494, -1376, -4418, 0 }, { { -1141, 1024, -417, 0 }, { 1142, 1024, 418, 0 }, { -1141, -1024, -417, 0 }, { 1142, -1024, 418, 0 } }, { -1410, 0, 3848, 0 }, { -1257, 0, 3898, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_AKROPOLIS_FORKED_ROAD, AKROPOLIS_SQUARE_TRIGGER_FORKED_ROAD_WARPS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 52, -1680, -3974, 0 }, { { 1496, 0, -1411, 0 }, { 1110, 0, 439, 0 }, { -1444, 0, -1415, 0 }, { -1127, 0, 436, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 168, -2272, -80, 0 }, { { 888, 0, 192, 0 }, { 1112, 0, 1601, 0 }, { -839, 0, 192, 0 }, { -1159, 0, 1601, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, 4091, 0 }, 1974, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6272, -2328, 2400, 0 }, { { -912, 0, -736, 0 }, { 912, 0, -736, 0 }, { -912, 0, 736, 0 }, { 912, 0, 736, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4464, -1200, -4609, 0 }, { { 888, 0, -537, 0 }, { 1040, 0, -152, 0 }, { -1040, 0, 153, 0 }, { -888, 0, 538, 0 } }, { 0, 4103, 0, 0 }, { -1380, 0, -3857, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_FACING | WORLD_COLLISION_TRIGGER_AUTOMATIC, 67, 16, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4689, -1152, -4801, 0 }, { { -1054, 0, -113, 0 }, { -858, 0, -621, 0 }, { 858, 0, 621, 0 }, { 1054, 0, 113, 0 } }, { 0, 4102, 0, 0 }, { 1567, 0, -3784, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_FACING | WORLD_COLLISION_TRIGGER_AUTOMATIC, 67, 240, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3568, -2304, 3600, 0 }, { { 1680, 0, -1312, 0 }, { 1680, 0, 1473, 0 }, { -1679, 0, -1472, 0 }, { -1679, 0, 1313, 0 } }, { 0, 4108, 0, 0 }, { -201, 0, 4091, 0 }, 2231, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -1552, -2272, -448, 0 }, { { -1376, 0, -1088, 0 }, { 768, 0, -1088, 0 }, { -1120, 0, 1184, 0 }, { 768, 0, 1376, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1750, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1536, -2272, -640, 0 }, { { -1952, 0, -672, 0 }, { 1248, 0, -672, 0 }, { -1280, 0, 1568, 0 }, { 1248, 0, 1408, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 2063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -769, -1696, -6753, 0 }, { { -943, 0, 302, 0 }, { 1265, 0, -16, 0 }, { -560, 0, 1105, 0 }, { 1040, 0, 787, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5577, -2272, -1575, 0 }, { { -824, 0, -504, 0 }, { 1288, 0, -504, 0 }, { -856, 0, 1000, 0 }, { 1288, 0, 1480, 0 } }, { 0, 4103, 0, 0 }, { 4096, 0, 0, 0 }, 1958, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1104, -2304, 5136, 0 }, { { -992, 0, -848, 0 }, { 672, 0, -848, 0 }, { -992, 0, 80, 0 }, { 672, 0, 80, 0 } }, { 0, 4107, 0, 0 }, { 4017, 0, -799, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 13, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1824, -2240, 3801, 0 }, { { 3504, 0, -2168, 0 }, { 3504, 0, 2249, 0 }, { -3503, 0, -1720, 0 }, { -3503, 0, 1641, 0 } }, { 0, 4102, 0, 0 }, { -201, 0, 4091, 0 }, 4159, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5281, -1640, -2401, 0 }, { { -991, 0, -568, 0 }, { 623, 0, -972, 0 }, { -696, 0, 744, 0 }, { 1066, 0, 797, 0 } }, { 0, 4117, 0, 0 }, { 3513, 0, -2106, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -128, -1664, -3296, 0 }, { { 7440, 0, -176, 0 }, { 7440, 0, 176, 0 }, { -7440, 0, -176, 0 }, { -7440, 0, 176, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 7437, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 14, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -1664, -4784, 0 }, { { 1248, 0, -640, 0 }, { 1248, 0, 641, 0 }, { -1247, 0, -640, 0 }, { -1247, 0, 641, 0 } }, { 0, 4102, 0, 0 }, { 201, 0, -4091, 0 }, 1402, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 192, -2252, -1664, 0 }, { { 7248, 0, -304, 0 }, { 7248, 0, 272, 0 }, { -7248, 0, -272, 0 }, { -7248, 0, 304, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 7240, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 16, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Grinning Stranger resource, entry 19, followed by END.
///
/// The matching placement is map-owned. Actor file number 118 uses base group
/// 10 and task descriptor 0; loading and placement borrow both overlays' tables.
static AreaResource _gAkropolisSquareVariant1Resources[2] = {
    {
        .entryId        = 19,
        .fileNumber     = 118,
        .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor111800GrinningStrangerTaskDesc,
    },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gAkropolisSquareAreaVariants[3] = {
    { NULL, NULL },
    { gMapAkropolisSquareVariant1Placements, _gAkropolisSquareVariant1Resources },
    { NULL, NULL },
};

/// The square's 16 point lights, contributing to model lighting in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits (`ONE` is full intensity). The loaded room overlay owns
/// these writable records: room updates parent and compose their transforms,
/// and lighting queries overwrite attenuation. Borrowed pointers must not
/// survive unloading the overlay.
static WorldCoordPointLight _gAkropolisSquarePointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3005, -4865, 2030 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3031, 2949, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10,
        .outer = 8250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5380, -2055, -3345 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2375, 2293, 2211 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 3500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5360, -2055, -3345 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2375, 2293, 2211 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 3500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4395, -1475, -4140 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1884, 1802, 1720 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 750,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4405, -1475, -4140 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1884, 1802, 1720 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 750,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5104, -900, -5740 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2703, 2621, 2539 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5037, -900, -5783 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2703, 2621, 2539 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6430, -4865, 5 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3276, 3194, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 650,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6445, -4865, 5 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3276, 3194, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 650,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3108, -4865, 2030 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3031, 2949, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10,
        .outer = 8250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1960, -4265, -5722 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2949, 2949, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1970, -4265, -5722 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3031, 3031, 2949 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4925, -1925, -2165 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2211, 2129, 2048 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5090, -1925, -2165 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2211, 2129, 2048 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2869, -1925, -2165 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2211, 2129, 2048 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1250,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2805, -1925, -2165 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2211, 2129, 2048 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 1250,
    },
};

/// The square's sixteen point lights, with no directional or cone lights.
///
/// The room-lighting table borrows this collection. Its light array stays mutable
/// for coordinate composition and attenuation queries while the overlay is loaded.
static WorldCoordRoomLights _gAkropolisSquareRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gAkropolisSquarePointLights),
    .pointLights           = _gAkropolisSquarePointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Minimum ambient colours for the square's fifteen logical views.
///
/// Entry zero gives the following view count; entries 1..15 each supply RGB
/// minima of 16 (one 8-bit colour level) and retained luma 16. Lighting borrows
/// this table read-only; it stays live with the room overlay.
static WorldCoordRoomAmbientEntry _gAkropolisSquareAmbientTable[16] = {
    { .viewCount = ARRAY_SIZE(_gAkropolisSquareAmbientTable) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Empty foreground list for logical view 1, retaining decoded background strips.
///
/// Keep both records: the zero-count header is skipped before the end marker
/// is tested. The view also retains this list address in its empty source slot.
static SpriteBatch _gAkropolisSquareView1SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Empty foreground list for logical view 2, retaining decoded background strips.
///
/// Keep both records: the zero-count header is skipped before the end marker
/// is tested. The view also retains this list address in its empty source slot.
static SpriteBatch _gAkropolisSquareView2SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 73 foreground texture rectangles for logical view 3.
///
/// Positions and sizes use pixels relative to the view centre; depth is the
/// unscaled ordering-table sorting value. RGB 128 is neutral modulation and
/// code flags are zero. The view and its batches borrow these source elements
/// while the room overlay is loaded.
static SpriteSource _gAkropolisSquareView3SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -80, 2750, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 2750, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 2750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 0, 2875, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 8, 2450, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 0, 2450, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 8, 2450, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 40, 1550, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, 32, 1575, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 32, 1600, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 32, 1625, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 24, 1650, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 24, 1675, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 16, 1700, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 16, 1725, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 40, 1750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 48, 625, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 48, 625, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 56, 625, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 48, 625, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -104, 88, 625, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -80, 64, 625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 88, 625, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 88, 500, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 56, 500, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, 64, 625, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -24, 88, 625, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 56, 625, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 72, 625, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, 72, 625, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, 72, 625, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 88, 64, 625, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 120, 64, 625, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 64, 625, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 104, 625, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 104, 625, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 56, 625, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 40, 1650, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 40, 1650, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 16, 1750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 16, 1725, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 32, 1700, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 32, 1675, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 2475, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 24, 2425, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -32, 24, 2325, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -8, 32, 2312, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, 32, 2312, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 24, 2375, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 32, 2425, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -8, 2500, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 16, 2500, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 16, 2450, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -16, 2450, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, 16, 2425, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 16, 2400, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, -16, 2425, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -24, 2400, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -16, 2425, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 16, 2425, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 16, 2450, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -8, 2500, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -16, 2450, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 16, 2500, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -24, 2450, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 2450, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -64, 2450, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -64, 2450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -112, 2450, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -88, 2450, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -88, 2450, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -120, 2450, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -120, 2450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

/// Foreground sprite ranges for logical view 3, retaining decoded background strips.
///
/// The zero-count first record selects the background; the last record ends
/// the list. Source indices and counts are in elements. Flags remain mutable
/// while this overlay is loaded; the two serialized bytes retain unproven roles.
static SpriteBatch _gAkropolisSquareView3SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 4, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 4, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 5, 0 } },
    { .firstSprite = 7, .spriteCount = 9, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 4, 0 } },
    { .firstSprite = 16, .spriteCount = 11, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 6, 0 } },
    { .firstSprite = 27, .spriteCount = 10, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 37, .spriteCount = 6, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 7, 0 } },
    { .firstSprite = 43, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = 50, .spriteCount = 14, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 8, 0 } },
    { .firstSprite = 64, .spriteCount = ARRAY_SIZE(_gAkropolisSquareView3SpriteSources) - 64, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 41 foreground texture rectangles for logical view 4.
///
/// Positions and sizes use pixels relative to the view centre; depth is the
/// unscaled ordering-table sorting value. RGB 128 is neutral modulation and
/// code flags are zero. The view and its batches borrow these source elements
/// while the room overlay is loaded.
static SpriteSource _gAkropolisSquareView4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 64, 1337, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 1325, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 1175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 64, 1180, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 48, 1180, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 64, 1212, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 40, 1212, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 40, 1225, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 64, 1225, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 32, 1337, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 32, 1325, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 24, 1375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 64, 1375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 112, 875, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 104, 875, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 875, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 96, 875, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 875, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -40, 88, 875, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 72, 875, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 80, 875, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 88, 875, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 48, 1556, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 112, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 104, 1337, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 104, 1300, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 104, 1300, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 104, 1250, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 88, 1325, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 88, 1312, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 88, 1350, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 88, 1325, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 72, 1412, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 72, 1406, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 72, 1356, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 64, 1506, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 1562, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 64, 1530, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 56, 1562, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 48, 1706, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 40, 1712, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
};

/// Foreground sprite ranges for logical view 4, retaining decoded background strips.
///
/// The zero-count first record selects the background; the last record ends
/// the list. Source indices and counts are in elements. Flags remain mutable
/// while this overlay is loaded; the two serialized bytes retain unproven roles.
static SpriteBatch _gAkropolisSquareView4SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 13, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 13, .spriteCount = 9, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 22, .spriteCount = ARRAY_SIZE(_gAkropolisSquareView4SpriteSources) - 22, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 31 foreground texture rectangles for logical view 5.
///
/// Positions and sizes use pixels relative to the view centre; depth is the
/// unscaled ordering-table sorting value. RGB 128 is neutral modulation and
/// code flags are zero. The view and its batches borrow these source elements
/// while the room overlay is loaded.
static SpriteSource _gAkropolisSquareView5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 80, 1118, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 80, 1106, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 80, 1112, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 64, 1112, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 80, 1115, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 56, 1115, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 80, 1117, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, 48, 1117, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, 40, 1118, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 40, 1120, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 80, 1120, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 24, 1120, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 112, 937, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 72, 937, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 88, 937, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 104, 937, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 96, 1275, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 104, 1200, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 104, 1200, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 104, 1200, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 104, 1200, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 88, 1300, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 88, 1300, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 88, 1300, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 80, 1300, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 1350, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 80, 1350, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 72, 1400, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 1375, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 64, 1450, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 1450, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
};

/// Foreground sprite ranges for logical view 5, retaining decoded background strips.
///
/// The zero-count first record selects the background; the last record ends
/// the list. Source indices and counts are in elements. Flags remain mutable
/// while this overlay is loaded; the two serialized bytes retain unproven roles.
static SpriteBatch _gAkropolisSquareView5SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 12, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 12, .spriteCount = 4, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 16, .spriteCount = ARRAY_SIZE(_gAkropolisSquareView5SpriteSources) - 16, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 91 foreground texture rectangles for logical view 6.
///
/// Positions and sizes use pixels relative to the view centre; depth is the
/// unscaled ordering-table sorting value. RGB 128 is neutral modulation and
/// code flags are zero. The view and its batches borrow these source elements
/// while the room overlay is loaded.
static SpriteSource _gAkropolisSquareView6SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 1487, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 40, 1500, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 72, 1450, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 40, 1450, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 24, 1450, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 40, 1487, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 72, 1487, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 16, 1500, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 1500, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 24, 1500, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 48, 1350, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 1350, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, 32, 1475, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 32, 1500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 24, 1500, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 56, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 48, 1400, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 40, 1425, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 72, 1350, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -104, 56, 1362, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -72, 56, 1362, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -40, 56, 1362, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 48, 1375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 48, 1375, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 1375, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 88, 1275, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 88, 1275, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 72, 1275, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 72, 1275, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 72, 1275, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 56, 1275, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -128, 64, 1275, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 64, 1275, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, 40, 1275, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 104, 775, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -48, 104, 775, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 104, 775, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, 104, 775, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 88, 800, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 88, 812, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 88, 812, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 88, 812, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 72, 862, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 72, 862, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 72, 862, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -48, 64, 875, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, 64, 875, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 875, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 48, 1225, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 120, 72, 1212, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 56, 1212, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 56, 1212, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 40, 1225, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 120, 48, 1225, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 48, 3000, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 16, 56, 2875, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 48, 56, 2875, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 80, 56, 2875, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 56, 2875, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, 56, 2875, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 16, 40, 3000, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 40, 3000, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 40, 3000, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 40, 3000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 32, 3125, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 48, 32, 3125, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 32, 3125, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 24, 3250, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 3250, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 2762, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 2762, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, -48, 2762, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, 0, 2762, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -40, 2762, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, -48, 2750, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, -48, 2762, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, -40, 2925, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 24, 2925, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, 0, 2925, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -40, 2925, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, -48, 2925, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 0, 2925, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 8, 88, 987, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 88, 987, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 88, 987, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 8, 72, 1000, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 72, 1000, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 72, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 56, 1062, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 56, 1062, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 64, 1062, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
};

/// Foreground sprite ranges for logical view 6, retaining decoded background strips.
///
/// The zero-count first record selects the background; the last record ends
/// the list. Source indices and counts are in elements. Flags remain mutable
/// while this overlay is loaded; the two serialized bytes retain unproven roles.
static SpriteBatch _gAkropolisSquareView6SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 6, 0 } },
    { .firstSprite = 7, .spriteCount = 8, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 15, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 8, 0 } },
    { .firstSprite = 18, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 25, .spriteCount = 9, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 5, 0 } },
    { .firstSprite = 34, .spriteCount = 14, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 48, .spriteCount = 6, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 9, 0 } },
    { .firstSprite = 54, .spriteCount = 15, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 4, 0 } },
    { .firstSprite = 69, .spriteCount = 13, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 7, 0 } },
    { .firstSprite = 82, .spriteCount = ARRAY_SIZE(_gAkropolisSquareView6SpriteSources) - 82, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 94 foreground texture rectangles for logical view 7.
///
/// Positions and sizes use pixels relative to the view centre; depth is the
/// unscaled ordering-table sorting value. RGB 128 is neutral modulation and
/// code flags are zero. The view and its batches borrow these source elements
/// while the room overlay is loaded.
static SpriteSource _gAkropolisSquareView7SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 2050, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 0, 2050, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -32, 2050, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -72, 2050, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -72, 2050, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 8, 2050, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 136, -24, 2050, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -48, 2050, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 2062, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 24, 2025, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 40, 1912, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 32, 2025, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 64, 2025, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 32, 2025, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 16, 2025, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 24, 2037, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 32, 2062, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 32, 2050, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 2050, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 16, 2050, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 32, 2037, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 64, 2037, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 32, 2025, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 64, 2025, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 64, 2025, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 64, 1200, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 24, 1200, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -16, 1200, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -56, 1200, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -96, 1200, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, -104, 1200, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, -88, 1200, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 40, 1987, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 56, 1987, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 8, 1987, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -24, 1987, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -56, 1987, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -48, 1987, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 16, 2150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, -16, 2150, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -32, 2150, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 8, 2250, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -24, 2250, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 24, 2125, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 24, 2125, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 2125, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 32, 2125, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 40, 2250, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 32, 2250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 48, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 2125, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 32, 2125, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 40, 2125, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 48, 2112, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 48, 2112, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 2112, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 40, 2112, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 32, 2112, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 2100, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 2100, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 40, 2100, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 1550, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 1550, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 56, 1600, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 48, 1600, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 40, 1700, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1750, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 48, 1600, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 88, 1250, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 64, 1312, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 56, 1312, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -144, 72, 1300, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 72, 1300, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 1312, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 802, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 88, 780, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -104, 96, 775, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -72, 96, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -32, 96, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 750, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 80, 787, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -72, 80, 780, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -32, 80, 780, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 72, 795, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 72, 795, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 48, 1950, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, 40, 1950, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 1950, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 88, 72, 1300, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 120, 72, 1300, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 64, 1305, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 56, 1305, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 64, 1305, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 56, 1305, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
};

/// Foreground sprite ranges for logical view 7, retaining decoded background strips.
///
/// The zero-count first record selects the background; the last record ends
/// the list. Source indices and counts are in elements. Flags remain mutable
/// while this overlay is loaded; the two serialized bytes retain unproven roles.
static SpriteBatch _gAkropolisSquareView7SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 8, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 13, 0 } },
    { .firstSprite = 8, .spriteCount = 17, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 25, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 9, 0 } },
    { .firstSprite = 32, .spriteCount = 6, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = 38, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 11, 0 } },
    { .firstSprite = 41, .spriteCount = 2, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 43, .spriteCount = 4, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 12, 0 } },
    { .firstSprite = 47, .spriteCount = 2, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 49, .spriteCount = 4, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 8, 0 } },
    { .firstSprite = 53, .spriteCount = 5, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 6, 0 } },
    { .firstSprite = 58, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 15, 0 } },
    { .firstSprite = 61, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 7, 0 } },
    { .firstSprite = 68, .spriteCount = 6, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 10, 0 } },
    { .firstSprite = 74, .spriteCount = 11, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 4, 0 } },
    { .firstSprite = 85, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 14, 0 } },
    { .firstSprite = 88, .spriteCount = ARRAY_SIZE(_gAkropolisSquareView7SpriteSources) - 88, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 5, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 53 foreground texture rectangles for logical view 8.
///
/// Positions and sizes use pixels relative to the view centre; depth is the
/// unscaled ordering-table sorting value. RGB 128 is neutral modulation and
/// code flags are zero. The view and its batches borrow these source elements
/// while the room overlay is loaded.
static SpriteSource _gAkropolisSquareView8SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 48, 1125, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 80, 1075, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 1125, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 80, 1075, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 16, 1125, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 40, 1125, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 80, 1075, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 1075, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 40, 1125, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 24, 2185, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 24, 2185, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 32, 2185, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 24, 2185, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 32, 2185, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 24, 2225, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 24, 2225, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 16, 2225, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 16, 2225, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 104, 1250, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 80, 1250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 1250, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 64, 1400, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 56, 1400, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 80, 1250, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 56, 1400, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 64, 1400, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 72, 1393, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 72, 56, 1395, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 48, 1417, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 1200, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 1125, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 64, 1162, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 40, 1175, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 32, 1187, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 32, 1200, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, 40, 1350, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, 0, 1350, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -40, 1350, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, -72, 1350, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -144, -96, 1350, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 2375, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 8, 2375, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -32, 2375, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, -56, 2375, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -56, 2375, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, 8, 3375, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -24, 3375, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, -40, 3375, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 0, 3375, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 3375, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 16, 3375, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -16, 3375, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -40, 3375, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
};

/// Foreground sprite ranges for logical view 8, retaining decoded background strips.
///
/// The zero-count first record selects the background; the last record ends
/// the list. Source indices and counts are in elements. Flags remain mutable
/// while this overlay is loaded; the two serialized bytes retain unproven roles.
static SpriteBatch _gAkropolisSquareView8SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 9, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 9, .spriteCount = 5, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 6, 0 } },
    { .firstSprite = 14, .spriteCount = 4, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 18, .spriteCount = 8, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 7, 0 } },
    { .firstSprite = 26, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 5, 0 } },
    { .firstSprite = 29, .spriteCount = 6, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 8, 0 } },
    { .firstSprite = 35, .spriteCount = 5, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 4, 0 } },
    { .firstSprite = 40, .spriteCount = 5, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 9, 0 } },
    { .firstSprite = 45, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 48, .spriteCount = 2, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 10, 0 } },
    { .firstSprite = 50, .spriteCount = ARRAY_SIZE(_gAkropolisSquareView8SpriteSources) - 50, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Empty foreground list for logical view 9, retaining decoded background strips.
///
/// Keep both records: the zero-count header is skipped before the end marker
/// is tested. The view also retains this list address in its empty source slot.
static SpriteBatch _gAkropolisSquareView9SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Empty foreground list for logical view 10, retaining decoded background strips.
///
/// Keep both records: the zero-count header is skipped before the end marker
/// is tested. The view also retains this list address in its empty source slot.
static SpriteBatch _gAkropolisSquareView10SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Empty foreground list for logical view 11, retaining decoded background strips.
///
/// Keep both records: the zero-count header is skipped before the end marker
/// is tested. The view also retains this list address in its empty source slot.
static SpriteBatch _gAkropolisSquareView11SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Empty foreground list for logical view 12, retaining decoded background strips.
///
/// Keep both records: the zero-count header is skipped before the end marker
/// is tested. The view also retains this list address in its empty source slot.
static SpriteBatch _gAkropolisSquareView12SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Empty foreground for view 13, selecting decoded background strips.
///
/// Keep the zero-count header and END record. The view retains this list's
/// address in sources.empty; no SpriteSource elements are read through it.
static SpriteBatch _gAkropolisSquareView13SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Empty foreground for view 14, selecting decoded background strips.
///
/// Keep the zero-count header and END record. The view retains this list's
/// address in sources.empty; no SpriteSource elements are read through it.
static SpriteBatch _gAkropolisSquareView14SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Empty foreground for view 15, selecting decoded background strips.
///
/// Keep the zero-count header and END record. The view retains this list's
/// address in sources.empty; no SpriteSource elements are read through it.
static SpriteBatch _gAkropolisSquareView15SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteView gAkropolisSquareSpriteViews[15] = {
    { { .empty = _gAkropolisSquareView1SpriteBatches }, _gAkropolisSquareView1SpriteBatches, NULL },
    { { .empty = _gAkropolisSquareView2SpriteBatches }, _gAkropolisSquareView2SpriteBatches, NULL },
    { { .elements = _gAkropolisSquareView3SpriteSources }, _gAkropolisSquareView3SpriteBatches, NULL },
    { { .elements = _gAkropolisSquareView4SpriteSources }, _gAkropolisSquareView4SpriteBatches, NULL },
    { { .elements = _gAkropolisSquareView5SpriteSources }, _gAkropolisSquareView5SpriteBatches, NULL },
    { { .elements = _gAkropolisSquareView6SpriteSources }, _gAkropolisSquareView6SpriteBatches, NULL },
    { { .elements = _gAkropolisSquareView7SpriteSources }, _gAkropolisSquareView7SpriteBatches, NULL },
    { { .elements = _gAkropolisSquareView8SpriteSources }, _gAkropolisSquareView8SpriteBatches, NULL },
    { { .empty = _gAkropolisSquareView9SpriteBatches }, _gAkropolisSquareView9SpriteBatches, NULL },
    { { .empty = _gAkropolisSquareView10SpriteBatches }, _gAkropolisSquareView10SpriteBatches, NULL },
    { { .empty = _gAkropolisSquareView11SpriteBatches }, _gAkropolisSquareView11SpriteBatches, NULL },
    { { .empty = _gAkropolisSquareView12SpriteBatches }, _gAkropolisSquareView12SpriteBatches, NULL },
    { { .empty = _gAkropolisSquareView13SpriteBatches }, _gAkropolisSquareView13SpriteBatches, NULL },
    { { .empty = _gAkropolisSquareView14SpriteBatches }, _gAkropolisSquareView14SpriteBatches, NULL },
    { { .empty = _gAkropolisSquareView15SpriteBatches }, _gAkropolisSquareView15SpriteBatches, NULL },
};

ViewCamera gAkropolisSquareViewCameras[15] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x704E, 100 } }, 348 },
    { { { { 3936, 0, 1132 }, { 14, 4095, -48 }, { -1132, 50, 3936 } }, { -3740, 3950, 0x2FEE } }, 195 },
    { { { { 4080, 0, 358 }, { 94, 3950, -1079 }, { -345, 1083, 3934 } }, { -370, 4950, 0x2828 } }, 207 },
    { { { { 3737, 0, 1674 }, { 531, 3884, -1185 }, { -1588, 1299, 3544 } }, { 1990, 4870, 9170 } }, 263 },
    { { { { 3750, 0, -1647 }, { -542, 3867, -1235 }, { 1555, 1349, 3540 } }, { -1930, 4800, 8370 } }, 246 },
    { { { { -1334, 0, -3872 }, { -103, 4094, 35 }, { 3871, 109, -1334 } }, { 2600, 3765, -4090 } }, 246 },
    { { { { -920, 0, -3991 }, { 300, 4084, -69 }, { 3979, -308, -917 } }, { 6160, 3515, -3630 } }, 207 },
    { { { { 4041, 0, 667 }, { 0, 4095, -3 }, { -667, 3, 4041 } }, { 3090, 3725, 5820 } }, 235 },
    { { { { -872, 0, -4002 }, { -1083, 3942, 236 }, { 3852, 1109, -839 } }, { -5555, 4045, -2590 } }, 269 },
    { { { { -2704, 0, -3076 }, { -109, 4093, 96 }, { 3074, 145, -2702 } }, { 1780, 3230, -1710 } }, 230 },
    { { { { -4018, 0, -791 }, { -243, 3897, 1236 }, { 753, 1260, -3823 } }, { 130, 2975, -880 } }, 257 },
    { { { { 3944, 0, 1103 }, { 808, 2787, -2890 }, { -750, 3001, 2684 } }, { -1310, 3273, 5158 } }, 329 },
    { { { { -704, 0, 4035 }, { 189, 4091, 33 }, { -4030, 192, -703 } }, { 1800, 3413, -820 } }, 257 },
    { { { { 1064, 0, -3955 }, { -2268, 3355, -610 }, { 3239, 2349, 872 } }, { -4075, 5389, -1590 } }, 269 },
    { { { { 1034, 0, -3963 }, { 258, 4087, 67 }, { 3954, -266, 1031 } }, { 7815, 3515, 1095 } }, 418 },
};

/// Footstep cue bases for surface class 4; scripted jumps reuse walking.
///
/// Cue 2 uses the base, cue 1 adds 1 and companion playback adds 100.
/// The surface record borrows this storage while the room overlay is loaded.
static WorldCollisionFootstepSounds _gAkropolisSquareFootstepSounds = {
    .walk         = AKROPOLIS_SQUARE_FOOTSTEP_WALK,
    .run          = AKROPOLIS_SQUARE_FOOTSTEP_RUN,
    .scriptedJump = AKROPOLIS_SQUARE_FOOTSTEP_WALK,
};

/// Shared policy for classes 0..3 and 5..7: solid, impact-enabled and without footstep cues.
///
/// Grid contacts apply pushback. The first byte's role is unproven and retains
/// zero. The surface-class directory borrows this record while the overlay is loaded.
static WorldCollisionSurfaceProperties _gAkropolisSquareDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 4's solid, impact-enabled surface policy with paired footstep cues.
///
/// Grid contacts apply pushback. The first byte's role is unproven and retains
/// zero. The surface-class directory borrows this record while the overlay is loaded.
static WorldCollisionSurfaceProperties _gAkropolisSquareClass4SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAkropolisSquareFootstepSounds,
};

WorldCollisionSurfaceProperties* gAkropolisSquareSurfacePropertiesByClass[8] = {
    &_gAkropolisSquareDefaultSurfaceProperties,
    &_gAkropolisSquareDefaultSurfaceProperties,
    &_gAkropolisSquareDefaultSurfaceProperties,
    &_gAkropolisSquareDefaultSurfaceProperties,
    &_gAkropolisSquareClass4SurfaceProperties,
    &_gAkropolisSquareDefaultSurfaceProperties,
    &_gAkropolisSquareDefaultSurfaceProperties,
    &_gAkropolisSquareDefaultSurfaceProperties,
};

AreaApplyRec gAkropolisSquarePostCutsceneAreaUpdates[4] = {
    { .stage = GAME_STAGE_AKROPOLIS, .area = GAME_AREA_AKROPOLIS_PATIO, .variant = AKROPOLIS_SQUARE_POST_CUTSCENE_PATIO_VARIANT, .policy = AREA_APPLY_MODE_ALWAYS | AKROPOLIS_SQUARE_POST_CUTSCENE_MAP_MARK },
    { .stage = GAME_STAGE_AKROPOLIS, .area = GAME_AREA_AKROPOLIS_CAFETERIA, .variant = AKROPOLIS_SQUARE_POST_CUTSCENE_CAFETERIA_VARIANT, .policy = AREA_APPLY_MODE_ALWAYS | AKROPOLIS_SQUARE_POST_CUTSCENE_MAP_MARK },
    { .stage = GAME_STAGE_AKROPOLIS, .area = GAME_AREA_MIST_PARKING, .variant = AKROPOLIS_SQUARE_POST_CUTSCENE_PARKING_VARIANT, .policy = AREA_APPLY_MODE_ALWAYS | AKROPOLIS_SQUARE_POST_CUTSCENE_MAP_MARK },
    { .stage = AREA_APPLY_END },
};

/// Callback ticks elapsed since the last ring, reset before each repeat interval.
///
/// The singleton ring task increments it in its two repeat states and
/// rings again at 121. Startup and repeated rings reset it to zero.
static s32 _gAkropolisSquareTelephoneRingFrames = 0;

/// Borrowed singleton handle of the arrival scene's scrolling backdrop.
///
/// Start replaces this handle, assuming spawning succeeds. Stop requires a
/// live task; both explicit stop and the backdrop's self-teardown leave the
/// pointer unchanged. The next start overwrites it; overlay load supplies NULL.
static Task* _gAkropolisSquareBackdropTask = NULL;

/// Singleton backdrop's horizontal strip anchor in pixels, panning from -320 to zero.
static s32 _gAkropolisSquareBackdropScrollX = 0;

/// Elapsed callback ticks in the backdrop's 46-tick delay or 31-tick hold phase.
///
/// Reset at task initialization and on reaching the final scroll position;
/// the same signed word is reused because those phases cannot overlap.
static s32 _gAkropolisSquareBackdropPhaseFrames = 0;

Task* gRoomCutsceneSoundTask = NULL;

/// Persistent record for the square's view-9, CAP-file-1 cutscene and its sound cues.
///
/// The room fills rec before handing it to the asynchronous runner; do not
/// overwrite or unload it until completion. capTPageX/Y remain zero, selecting
/// the default CAP texture origin. The eight trailing bytes have no proven role.
static RoomCutsceneRecStorage _gAkropolisSquareCutsceneRec = { 0 };

/// The ringing telephone, at (6570, -3990, 2270), in whole room-coordinate units.
///
/// Each ring binds the current view parent and composes the cached transform
/// before pan/depth queries; those queries borrow it only during the call.
static GfxCoord _gAkropolisSquareTelephoneCoord = { 0 };

#include "../../../../shared/room/planar_reflection.inc.c"

void akropolisSquarePlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\xDC\xDD"
#include "../../../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void akropolisSquareTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../../../shared/room/telephone_panels.inc.c"

#include "../../../../shared/room/room_cutscene_task.inc.c"

/// Three room-task states: initialize (0), idle arrival checks (1), and teardown (2).
///
/// Copied by value before dispatch with Task::state in 0..2; no terminator.
/// Keep this table at its interleaved rodata position.
static const TaskFuncTable3 _gAkropolisSquareRoomStates = {
    {
        _akropolisSquareInitializeRoomTask,
        _akropolisSquareIdleRoomTask,
        taskKill,
    },
};

/// Resolves square exits and settles a pending telephone call on departure.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows an eight-byte request and writable reply,
/// which may alias; copies the whole record before resolution. Queries avoid
/// flag/CAP effects. Forked-road exit selects room 2 after lock release; the
/// elevator exits return 0 and run CAP command 1 once main progress reaches 2.
/// Executed patio exits select room 1, 2 or 3 from story progress. Returns
/// 1 for other exits. The retained location stores have no transition effect.
static s32 _akropolisSquareResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    // These unused aggregate stores are present in the original instruction stream.
    GameLocationKey retainedLocation;
    u16             destinationArea;

    retainedLocation.stage = GAME_STAGE_AKROPOLIS;
    retainedLocation.area  = GAME_AREA_AKROPOLIS_CAFETERIA;
    *reply                 = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_FORKED_ROAD) {
        if ((_gAkropolisSquareTelephoneCallPending != 0) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_SQUARE_TELEPHONE_MISSED_DIALOGUE);
        }
        if (request->areaId == GAME_AREA_AKROPOLIS_FORKED_ROAD) {
            if (gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 1) {
                reply->room = 2;
            }
        }
        return 1;
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_EAST_ELEVATOR_HALL) {
        if ((_gAkropolisSquareTelephoneCallPending != 0) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_SQUARE_TELEPHONE_MISSED_DIALOGUE);
        }
        if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PROGRESS) < 2) {
            return 1;
        }
        if ((gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PROGRESS) == 2) || (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PROGRESS) >= 3)) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // Mark the map and show the blocked-exit caption, retaining the exit.
                do {
                    gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
                    capRunCommandWithTransition(AKROPOLIS_SQUARE_ELEVATOR_BLOCKED_CAP_COMMAND);
                } while (0);
            }
            return 0;
        }
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_WEST_ELEVATOR_HALL) {
        if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PROGRESS) < 2) {
            return 1;
        }
        if ((gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PROGRESS) == 2) || (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PROGRESS) >= 3)) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // Mark the map and show the blocked-exit caption, retaining the exit.
                do {
                    gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
                    capRunCommandWithTransition(AKROPOLIS_SQUARE_ELEVATOR_BLOCKED_CAP_COMMAND);
                } while (0);
            }
            return 0;
        }
    }
    destinationArea = request->areaId;
    if (destinationArea == GAME_AREA_AKROPOLIS_PATIO) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_PROGRESS) < 2) {
                if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_OPENING_PROGRESS) < 2) {
                    reply->room = 1;
                } else {
                    reply->room = 2;
                }
            } else {
                reply->room = destinationArea;
            }
        }
    }
    return 1;
}
/// Publishes the square cutscene record and starts its controller.
///
/// Consumes the pending-call latch as the skip choice and normalizes
/// arrival warp 7 to 1. The controller borrows persistent overlay storage
/// through playback and runs CAP command 2 afterwards.
static inline void _akropolisSquareStartRoomCutscene(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == AKROPOLIS_SQUARE_CUTSCENE_ARRIVAL_WARP) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = AKROPOLIS_SQUARE_CUTSCENE_RESUME_WARP;
    }
    _gAkropolisSquareCutsceneRec.rec.view            = AKROPOLIS_SQUARE_CUTSCENE_VIEW;
    _gAkropolisSquareCutsceneRec.rec.capSlot         = AKROPOLIS_SQUARE_CUTSCENE_CAP_SLOT;
    _gAkropolisSquareCutsceneRec.rec.capFile         = AKROPOLIS_SQUARE_CUTSCENE_CAP_FILE;
    _gAkropolisSquareCutsceneRec.rec.startSound      = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_START);
    _gAkropolisSquareCutsceneRec.rec.endSound        = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_END);
    _gAkropolisSquareCutsceneRec.rec.sceneSound      = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_SCENE);
    _gAkropolisSquareCutsceneRec.rec.afterSceneSound = AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_CALL_AFTER);
    _gAkropolisSquareCutsceneRec.rec.skipScene       = _gAkropolisSquareTelephoneCallPending;
    _gAkropolisSquareTelephoneCallPending            = 0;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, AKROPOLIS_SQUARE_CUTSCENE_TASK_ENTRY, AKROPOLIS_SQUARE_CUTSCENE_FOLLOWUP, &_gAkropolisSquareCutsceneRec.rec);
}

/// Starts the square cutscene or a one-time CAP hint from an integer room command.
///
/// Command 2 consumes the pending-call latch as the cutscene skip choice,
/// normalizes arrival warp 7 to 1 and spawns the room-cutscene controller.
/// Commands 14 and 16 latch one-time hints; command 16 selects CAP 16 or 17
/// from the saved button layout. The cutscene record is persistent borrowed
/// storage; keep the overlay loaded through playback. Always returns 0.
static s32 _akropolisSquareHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    s32 hintCommand;

    if (command == AKROPOLIS_SQUARE_ROOM_COMMAND_CUTSCENE) {
        _akropolisSquareStartRoomCutscene();
    }
    if ((command == AKROPOLIS_SQUARE_ROOM_COMMAND_HINT_14) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_HINT_SEEN) == 0)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_HINT_SEEN, 1);
        capSpawnEventIfIdle(AKROPOLIS_SQUARE_ROOM_COMMAND_HINT_14, CAP_EVENT_PAUSE_ACTORS);
    }
    if ((command == AKROPOLIS_SQUARE_ROOM_COMMAND_CONTROLS_HINT) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_STAIRS_HINT_SEEN) == 0)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_STAIRS_HINT_SEEN, 1);
        hintCommand = AKROPOLIS_SQUARE_CONTROLS_HINT_ALTERNATE_COMMAND;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout != 1) {
            hintCommand = AKROPOLIS_SQUARE_ROOM_COMMAND_CONTROLS_HINT;
        }
        capSpawnEventIfIdle(hintCommand, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}
/// Positions and starts one ring, optionally restarting its repeat timer.
///
/// A nonzero `resetRepeatFrames` resets the elapsed callback-tick counter;
/// zero preserves it for the first ring. Places the origin at (6570, -3990,
/// 2270) in whole world-coordinate units, composes its view transform and
/// requests the ring sound. Pan is -16..15 and depth -128..127, narrowed to signed
/// bytes for audio. Requires live square coordinates and the current view;
/// changes GTE state and retains no new borrowed storage.
static inline void _akropolisSquareRingTelephone(s32 resetRepeatFrames)
{
    s32 soundPan;

    _gAkropolisSquareTelephoneCoord.coord.t[0] = AKROPOLIS_SQUARE_TELEPHONE_X;
    _gAkropolisSquareTelephoneCoord.coord.t[1] = AKROPOLIS_SQUARE_TELEPHONE_Y;
    _gAkropolisSquareTelephoneCoord.coord.t[2] = AKROPOLIS_SQUARE_TELEPHONE_Z;
    if (resetRepeatFrames) {
        _gAkropolisSquareTelephoneRingFrames = 0;
    }
    _gAkropolisSquareTelephoneCoord.parent = &gGfxViewCoord;
    actorRenderComposeCoord(&_gAkropolisSquareTelephoneCoord);
    soundPan = worldCoordGetOriginAudioPan(&_gAkropolisSquareTelephoneCoord);
    sndEvtRequestScriptStart(AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_TELEPHONE_RING), (s8)soundPan,
                             (s8)worldCoordGetOriginAudioDepth(&_gAkropolisSquareTelephoneCoord));
}

/// Runs the telephone scene, then rings until the call is answered or left.
///
/// State 0 starts the event with a skip script and holds the player; states
/// 1 and 2 advance one tick each, and state 3 sounds the first ring. State 4
/// repeats every 121 ticks until EVS becomes idle, then releases the player.
/// State 5 keeps ringing while the saved view is 5..7 or 9 and the
/// follow-up remains armed; otherwise it returns the phone's light to idle, stops
/// the ringing and kills this task. Requires the square overlay and resources.
static void _akropolisSquareTelephoneRingTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_TELEPHONE_START          = 0,
        AKROPOLIS_SQUARE_TELEPHONE_STARTUP_TICK_1 = 1,
        AKROPOLIS_SQUARE_TELEPHONE_STARTUP_TICK_2 = 2,
        AKROPOLIS_SQUARE_TELEPHONE_FIRST_RING     = 3,
        AKROPOLIS_SQUARE_TELEPHONE_WAIT_EVENT     = 4,
        AKROPOLIS_SQUARE_TELEPHONE_WAIT_HANDOFF   = 5,
    };
    s32 repeatFrames;

    switch (task->state) {
        case AKROPOLIS_SQUARE_TELEPHONE_START:
            // Hold the player while the opening camera and telephone scene script runs.
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            _gAkropolisSquareTelephoneCallPending = 1;
            _gAkropolisSquareTelephoneRingFrames  = 0;
            evsStartScriptWithSkip(_gAkropolisSquareTelephoneSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisSquareTelephoneSceneSkipScript);
            task->state += 1;
            return;

        case AKROPOLIS_SQUARE_TELEPHONE_FIRST_RING:
            _akropolisSquareRingTelephone(0);
            task->state += 1;
            return;

        case AKROPOLIS_SQUARE_TELEPHONE_WAIT_EVENT:
            repeatFrames                         = _gAkropolisSquareTelephoneRingFrames + 1;
            _gAkropolisSquareTelephoneRingFrames = repeatFrames;
            if (repeatFrames >= AKROPOLIS_SQUARE_TELEPHONE_RING_FRAMES) {
                _akropolisSquareRingTelephone(1);
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            /* fallthrough */

        case AKROPOLIS_SQUARE_TELEPHONE_STARTUP_TICK_1:
        case AKROPOLIS_SQUARE_TELEPHONE_STARTUP_TICK_2:
            task->state += 1;
            return;

        case AKROPOLIS_SQUARE_TELEPHONE_WAIT_HANDOFF:
            // Stop ringing after leaving views 5..7/9 or once the call is answered.
            if (((u32)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view - AKROPOLIS_SQUARE_TELEPHONE_FIRST_FOLLOWUP_VIEW) >= (u32)AKROPOLIS_SQUARE_TELEPHONE_FOLLOWUP_VIEW_COUNT &&
                 gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != AKROPOLIS_SQUARE_TELEPHONE_SCENE_VIEW) ||
                _gAkropolisSquareTelephoneCallPending == 0) {
                if (_gAkropolisSquareTelephoneCallPending != 0) {
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                    gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_SQUARE_TELEPHONE_MISSED_DIALOGUE);
                    _gAkropolisSquareTelephoneCallPending = 0;
                }
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE, 0, 0);
                sndEvtRequestScriptStop(AKROPOLIS_SQUARE_SOUND(AKROPOLIS_SQUARE_SOUND_TELEPHONE_RING), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(task);
                return;
            }
            repeatFrames                         = _gAkropolisSquareTelephoneRingFrames + 1;
            _gAkropolisSquareTelephoneRingFrames = repeatFrames;
            if (repeatFrames >= AKROPOLIS_SQUARE_TELEPHONE_RING_FRAMES) {
                _akropolisSquareRingTelephone(1);
            }
            break;
    }
}
/// Pans a 640-by-240 backdrop right by 320 pixels, then selects view 13.
///
/// States 0..4 initialize, delay 46 ticks, scroll one pixel per tick, hold 31
/// ticks and finish. Every tick draws, including the one that kills the task.
/// The image is three raw-color 8-bit sprite strips of widths 256, 256 and
/// 128, using VRAM X 448/576/704, Y 256 and the CLUT at (0, 255).
/// Requires an initialized frame arena with room for three `SPRT` and three
/// `DR_TPAGE` packets; queued packets must survive GPU drawing. Position and
/// timing are room singletons, so only one such task may run at a time.
static void _akropolisSquareScrollingBackdropTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_BACKDROP_START          = 0,
        AKROPOLIS_SQUARE_BACKDROP_DELAY          = 1,
        AKROPOLIS_SQUARE_BACKDROP_SCROLL         = 2,
        AKROPOLIS_SQUARE_BACKDROP_HOLD           = 3,
        AKROPOLIS_SQUARE_BACKDROP_FINISH         = 4,
        AKROPOLIS_SQUARE_BACKDROP_STRIP_COUNT    = 3,
        AKROPOLIS_SQUARE_BACKDROP_ORDERING_DEPTH = 4,
        AKROPOLIS_SQUARE_BACKDROP_TEXTURE_8_BIT  = 1,
    };
    SPRT*     sprite;
    DR_TPAGE* drawMode;
    s32       screenX;
    s32       stripIndex;
    s32       texturePageX;
    s32       delayFrames;
    s32       holdFrames;
    s32       scrollX;

    switch (task->state) {
        case AKROPOLIS_SQUARE_BACKDROP_START:
            _gAkropolisSquareBackdropScrollX     = -0x140;
            _gAkropolisSquareBackdropPhaseFrames = 0;
            task->state                         += 1;
            break;

        case AKROPOLIS_SQUARE_BACKDROP_DELAY:
            delayFrames                          = _gAkropolisSquareBackdropPhaseFrames + 1;
            _gAkropolisSquareBackdropPhaseFrames = delayFrames;
            if (delayFrames >= AKROPOLIS_SQUARE_BACKDROP_DELAY_FRAMES) {
                task->state += 1;
            }
            break;

        case AKROPOLIS_SQUARE_BACKDROP_SCROLL:
            scrollX                          = _gAkropolisSquareBackdropScrollX + 1;
            _gAkropolisSquareBackdropScrollX = scrollX;
            if (scrollX >= 0) {
                _gAkropolisSquareBackdropPhaseFrames = 0;
                task->state                         += 1;
            }
            break;

        case AKROPOLIS_SQUARE_BACKDROP_HOLD:
            holdFrames                           = _gAkropolisSquareBackdropPhaseFrames + 1;
            _gAkropolisSquareBackdropPhaseFrames = holdFrames;
            if (holdFrames >= AKROPOLIS_SQUARE_BACKDROP_HOLD_FRAMES) {
                task->state += 1;
            }
            break;

        case AKROPOLIS_SQUARE_BACKDROP_FINISH:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SQUARE_BACKDROP_FINISH_VIEW;
            taskKill(task);
            break;
    }

    // Draw even on the final tick; prepend each texture page before its sprite.
    screenX = _gAkropolisSquareBackdropScrollX;
    for (stripIndex = 0; stripIndex < AKROPOLIS_SQUARE_BACKDROP_STRIP_COUNT; stripIndex++) {
        texturePageX   = 0x1C0 + stripIndex * 0x80;
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setSprt(sprite);
        setShadeTex(sprite, 1);
        sprite->x0 = screenX - 0xA0;
        sprite->y0 = -0x78;
        sprite->u0 = 0;
        sprite->v0 = 0;
        if (stripIndex == AKROPOLIS_SQUARE_BACKDROP_STRIP_COUNT - 1) {
            sprite->w = 0x80;
            sprite->h = 0xF0;
        } else {
            sprite->w = 0x100;
            sprite->h = 0xF0;
        }
        sprite->clut = GetClut(0, 0xFF);
        addPrim(&gGpuCurrentOt[AKROPOLIS_SQUARE_BACKDROP_ORDERING_DEPTH], sprite);

        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setDrawTPage(drawMode, 0, 1, GetTPage(AKROPOLIS_SQUARE_BACKDROP_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, texturePageX, 0x100));
        addPrim(&gGpuCurrentOt[AKROPOLIS_SQUARE_BACKDROP_ORDERING_DEPTH], drawMode);

        screenX += 0x100;
    }
}

#include "../../../../shared/room/room_cutscene_sound_task.inc.c"

/// Starts CAP event 5 for room action 0, returning 0 for every action.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a live request through dispatch;
/// only its action ID is read. Other IDs are ignored, as are the other arguments.
static s32 _akropolisSquareHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    if (request->actionId == AKROPOLIS_SQUARE_ACTION_START_EVENT) {
        capSpawnEventIfIdle(AKROPOLIS_SQUARE_ACTION_CAP_EVENT, CAP_EVENT_NO_FLAGS);
    }
    return 0;
}

/// Refuses every key-item-use request with result 0 and no side effects.
///
/// Handles `AKROPOLIS_SQUARE_MESSAGE_USE_KEY_ITEM` on the room task. The item
/// ID and second argument are ignored; no payload storage is accessed.
static s32 _akropolisSquareRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return 0;
}

/// Starts the square's sound script selected by a zero-based cue index.
///
/// Handles `ROOM_MESSAGE_SOUND`. `cueIndex` must be in 0..8; no bounds check
/// is performed. Entries 0, 2, 3 and 4 pass a zero script ID through unchanged.
/// Playback is centered with neutral depth; returns 0 and ignores other arguments.
static s32 _akropolisSquareHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg)
{
    sndEvtRequestScriptStart(_gAkropolisSquareSoundCueScripts[cueIndex], 0, 0);
    return 0;
}

/// Runs the square's CAP-command-5 view sequence and marks its completion.
///
/// Bodyless states 0..2 select view 7; alternate states 3..7 select view 8
/// after two delay ticks. Each route starts CAP command 5 and ends by setting
/// `EVENT_FLAG_MAP_AKROPOLIS_AVAILABLE` and killing the task. Entry 1 of the room task table holds
/// this callback, but no current code spawns that entry. CAP/view resources
/// must be live for the selected route.
static void _akropolisSquareCapViewSequenceTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_CAP_VIEW_FIRST_START    = 0,
        AKROPOLIS_SQUARE_CAP_VIEW_FIRST_SELECT   = 1,
        AKROPOLIS_SQUARE_CAP_VIEW_FIRST_FINISH   = 2,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_START   = 3,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_DELAY_1 = 4,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_DELAY_2 = 5,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_SELECT  = 6,
        AKROPOLIS_SQUARE_CAP_VIEW_SECOND_FINISH  = 7,
    };
    switch (task->state) {
        case AKROPOLIS_SQUARE_CAP_VIEW_FIRST_START:
            capRunCommandWithTransition(AKROPOLIS_SQUARE_CAP_VIEW_COMMAND);
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_FIRST_SELECT:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SQUARE_CAP_VIEW_FIRST;
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_START:
            capRunCommandWithTransition(AKROPOLIS_SQUARE_CAP_VIEW_COMMAND);
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_DELAY_1:
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_DELAY_2:
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_SELECT:
            capRunCommandWithTransition(AKROPOLIS_SQUARE_CAP_VIEW_COMMAND);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_SQUARE_CAP_VIEW_SECOND;
            task->state++;
            return;
        case AKROPOLIS_SQUARE_CAP_VIEW_FIRST_FINISH:
        case AKROPOLIS_SQUARE_CAP_VIEW_SECOND_FINISH:
            gameFlagSetNibble(EVENT_FLAG_MAP_AKROPOLIS_AVAILABLE, 1);
            taskKill(task);
            return;
    }
}

/// Starts or stops the square arrival scene's scrolling-backdrop task.
///
/// Script action 0 spawns and remembers the singleton; action 1 kills that
/// task and leaves the pointer unchanged. Other actions do nothing. Stop
/// requires a successful start whose task is still live; repeated starts can
/// replace the remembered task without tearing down its predecessor.
static void _akropolisSquareControlScrollingBackdrop(s32 action)
{
    switch (action) {
        case AKROPOLIS_SQUARE_BACKDROP_ACTION_START:
            _gAkropolisSquareBackdropTask = taskSpawnFromTable(_gAkropolisSquareSequenceTaskDescs, AKROPOLIS_SQUARE_BACKDROP_TASK_INDEX, 0, 0);
            return;
        case AKROPOLIS_SQUARE_BACKDROP_ACTION_STOP:
            taskKill(_gAkropolisSquareBackdropTask);
            return;
    }
}

/// Installs the square's room message handlers and registers the live room task.
///
/// Called in state 0; the table is borrowed from this loaded overlay. Advances
/// to the idle state without creating a body or work block.
static void _akropolisSquareInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisSquareRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state += 1;
}

/// Starts the warp-7 arrival event once per overlay load.
///
/// Called in the room task's idle state; leaves the state unchanged. Arms scene
/// music event 2 and starts the event/skip scripts with HUD hiding and restoration.
/// The task itself is unused; room save state and overlay resources must be live.
static void _akropolisSquareIdleRoomTask(Task* unusedTask)
{
    // Removing this unused reservation changes the original 40-byte call frame.
    char retainedStackSpace[0x10];

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == AKROPOLIS_SQUARE_CUTSCENE_ARRIVAL_WARP && _gAkropolisSquareArrivalEventStarted == 0) {
        _gAkropolisSquareArrivalEventStarted                = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = AKROPOLIS_SQUARE_ARRIVAL_SCENE_EVENT;
        evsStartScriptWithSkip(_gAkropolisSquareArrivalEventScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisSquareArrivalSkipScript);
    }
}

void akropolisSquareRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAkropolisSquareRoomStates;
    stateHandlers.funcs[task->state](task);
}

s32 akropolisSquareStartTelephoneScene(s32 unused)
{
    enum {
        AKROPOLIS_SQUARE_TELEPHONE_NEWLY_ARMED   = 0,
        AKROPOLIS_SQUARE_TELEPHONE_ALREADY_ARMED = 1,
    };

    GameLocationKey key;

    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_TELEPHONE_SCENE_SEEN) == 0) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SQUARE_TELEPHONE_SCENE_SEEN, 1);
        key.stage = GAME_STAGE_AKROPOLIS;
        key.area  = GAME_AREA_AKROPOLIS_SQUARE;
        areaSetPlacementVariant(&key, AKROPOLIS_SQUARE_TELEPHONE_PLACEMENT_VARIANT, AREA_VARIANT_RESET_ALWAYS);
        gGameSession->eventState = 1;
        taskSpawnFromTable(_gAkropolisSquareSequenceTaskDescs, AKROPOLIS_SQUARE_TELEPHONE_TASK_INDEX, 0, 0);
        return AKROPOLIS_SQUARE_TELEPHONE_NEWLY_ARMED;
    }
    return AKROPOLIS_SQUARE_TELEPHONE_ALREADY_ARMED;
}

void akropolisSquareRoomEffectTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_ROOM_EFFECT_INITIAL = 0,
        AKROPOLIS_SQUARE_ROOM_EFFECT_ACTIVE  = 1,
    };
    EffectWork* work;
    GfxCoord*   roomCoord;

    roomCoord = task->extra.coordBody->coord;
    work      = task->spawnArg2.pointer;
    switch (task->state) {
        case AKROPOLIS_SQUARE_ROOM_EFFECT_INITIAL:
            // Install the effect receiver before starting both reflection modes.
            task->msgTable = _gAkropolisSquareRoomEffectMessages;
            gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
            _gAkropolisSquareTelephoneLightMode = AKROPOLIS_SQUARE_TELEPHONE_LIGHT_IDLE;
            taskSpawn(AKROPOLIS_SQUARE_REFLECTION_TASK_BANK, AKROPOLIS_SQUARE_REFLECTION_TASK_SLOT, AKROPOLIS_SQUARE_REFLECTION_FLOOR, 0);
            taskSpawn(AKROPOLIS_SQUARE_REFLECTION_TASK_BANK, AKROPOLIS_SQUARE_REFLECTION_TASK_SLOT, AKROPOLIS_SQUARE_REFLECTION_PLANE, 0);
            task->state++;
            return;
        case AKROPOLIS_SQUARE_ROOM_EFFECT_ACTIVE:
            // Each view of the phone uses its own packed pulse/radius/colour/shape preset.
            if ((AKROPOLIS_SQUARE_TELEPHONE_LIGHT_VIEW_MASK >> (gGameSession->location.view - 1)) & 1) {
                work->move.vx = AKROPOLIS_SQUARE_TELEPHONE_X;
                work->move.vy = AKROPOLIS_SQUARE_TELEPHONE_Y;
                work->move.vz = AKROPOLIS_SQUARE_TELEPHONE_Z;
                effectSpawn(EFFECT_AKROPOLIS_SQUARE_TELEPHONE_LIGHT, roomCoord, _gAkropolisSquareTelephoneLightMode * AKROPOLIS_SQUARE_TELEPHONE_LIGHT_FRONT_MODE_DELTA + AKROPOLIS_SQUARE_TELEPHONE_LIGHT_FRONT_BASE,
                            &work->move);
            }
            if (gGameSession->location.view == AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_VIEW) {
                work->move.vx = 0x18D2;
                work->move.vy = -0x100B;
                work->move.vz = 0x8AB;
                effectSpawn(EFFECT_AKROPOLIS_SQUARE_TELEPHONE_LIGHT, roomCoord, _gAkropolisSquareTelephoneLightMode * AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_MODE_DELTA + AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_BASE,
                            &work->move);
            }
            if (gGameSession->location.view == AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_VIEW) {
                work->move.vx = 0x19AA;
                work->move.vy = -0xF96;
                work->move.vz = 0x8E8;
                effectSpawn(EFFECT_AKROPOLIS_SQUARE_TELEPHONE_LIGHT, roomCoord, _gAkropolisSquareTelephoneLightMode * AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_MODE_DELTA + AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_BASE,
                            &work->move);
            }
            return;
    }
}

/// Queues a telephone-light primitive with additive blending at its projected depth.
///
/// `primitive` is an initialized, writable `POLY_G4` or `LINE_G3` in the frame
/// arena. `sortingDepth` borrows a readable signed word holding SZ3 / 4 before
/// display scaling; the caller rejects depths below 17. The scaled depth wraps
/// to tag 0..1023 in the current table. Keep the depth word separate from packet
/// storage; no depth pointer is retained.
/// Requires word-aligned space for one `DR_TPAGE` at `gGpuPrimCursor`, consumed
/// without a capacity check. Both packets must live until GPU drawing completes.
/// The dithered additive draw mode persists until another command replaces it.
static inline void _akropolisSquareQueueTelephoneLight(void* primitive, const s32* sortingDepth)
{
    // Sixteen depth units per tag, expressed as four-byte table offsets.
    enum { AKROPOLIS_SQUARE_GLOW_DEPTH_TO_BYTE_OFFSET_SHIFT = 2 };

    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                ((((u32)*sortingDepth << gDisplayState.otDepthShift) >> AKROPOLIS_SQUARE_GLOW_DEPTH_TO_BYTE_OFFSET_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            primitive);
    // Prepending the draw mode after the primitive makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, *sortingDepth);
}

void akropolisSquareTelephoneLightTask(Task* task)
{
    enum {
        AKROPOLIS_SQUARE_GLOW_MIN_DEPTH            = 17,
        AKROPOLIS_SQUARE_GLOW_PULSE_RATE_MASK      = 0xFF,
        AKROPOLIS_SQUARE_GLOW_RADIUS_SHIFT         = 8,
        AKROPOLIS_SQUARE_GLOW_RADIUS_MASK          = 0xFF,
        AKROPOLIS_SQUARE_GLOW_CYAN_SHIFT           = 16,
        AKROPOLIS_SQUARE_GLOW_STREAKS              = 0x10000000,
        AKROPOLIS_SQUARE_GLOW_PULSE_FALLING        = 0x80,
        AKROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK      = 0x7F,
        AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS   = 12,
        AKROPOLIS_SQUARE_GLOW_FAN_RADIUS_FACTOR    = 0x600,
        AKROPOLIS_SQUARE_GLOW_RAY_RADIUS_FACTOR    = 0xC0,
        AKROPOLIS_SQUARE_GLOW_DIAMOND_RADIUS_SHIFT = 9
    };
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    GfxCoord*             effectCoord;
    void*                 effectWork;
    s32                   segmentIndex;
    s32                   pulsePhase;
    s32                   glowValue;
    s32                   radiusScale;
    s16                   intensity;
    s16                   cyan;
    s16                   halfIntensity;

    // Project the composed effect center after narrowing to game coordinates.
    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = (u16)effectCoord->workm.t[0];
    projection->worldPos.vy = (u16)effectCoord->workm.t[1];
    projection->worldPos.vz = (u16)effectCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= AKROPOLIS_SQUARE_GLOW_MIN_DEPTH) {
        // Fold the frame phase into a triangular red or cyan brightness ramp.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & AKROPOLIS_SQUARE_GLOW_PULSE_RATE_MASK;
        cyan        = (task->spawnArg1.value >> AKROPOLIS_SQUARE_GLOW_CYAN_SHIFT) & 1;
        if (pulsePhase & AKROPOLIS_SQUARE_GLOW_PULSE_FALLING) {
            glowValue = ~pulsePhase & AKROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK;
        } else {
            glowValue = pulsePhase & AKROPOLIS_SQUARE_GLOW_PULSE_RAMP_MASK;
        }
        intensity = glowValue * 2;
        glowValue = task->spawnArg1.value;
        // The sign bit selects the round fan; the other form is a flat diamond.
        if (glowValue < 0) {
            radiusScale             = (glowValue >> AKROPOLIS_SQUARE_GLOW_RADIUS_SHIFT) & AKROPOLIS_SQUARE_GLOW_RADIUS_MASK;
            projection->outerRadius = (radiusScale * AKROPOLIS_SQUARE_GLOW_FAN_RADIUS_FACTOR) / projection->otz;
            projection->innerRadius = (radiusScale * AKROPOLIS_SQUARE_GLOW_RAY_RADIUS_FACTOR) / projection->otz;
            for (segmentIndex = 0; segmentIndex < 0x10; segmentIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, (intensity * (cyan ^ 1)) >> 1, (cyan * intensity) >> 1, (cyan * intensity) >> 1);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 5]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 1]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 6]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 2]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                _akropolisSquareQueueTelephoneLight(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 5]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 1]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 6]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 2]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                _akropolisSquareQueueTelephoneLight(quad, &projection->otz);
            }
            // Add two opposing pairs of long rays over the circular fan.
            halfIntensity = intensity >> 1;
            for (segmentIndex = 2; segmentIndex < 0x10; segmentIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                // The first ray samples the preceding message terminator's zero callback halfword.
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex - 4]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 8]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                _akropolisSquareQueueTelephoneLight(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 8]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 4]) >> AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 0xC]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * _gAkropolisSquareGlowSineSamples[segmentIndex + 8]) >> (AKROPOLIS_SQUARE_GLOW_TRIG_FRACTION_BITS + 1));
                _akropolisSquareQueueTelephoneLight(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((glowValue >> AKROPOLIS_SQUARE_GLOW_RADIUS_SHIFT) & AKROPOLIS_SQUARE_GLOW_RADIUS_MASK) << AKROPOLIS_SQUARE_GLOW_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (segmentIndex = 0; segmentIndex < 2; segmentIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, 0, 0, 0);
                setRGB1(quad, 0, 0, 0);
                setRGB2(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                setRGB3(quad, 0, 0, 0);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->outerRadius) + projection->outerRadius * (segmentIndex + segmentIndex);
                addPrim((&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF]),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz);
            }
            if (task->spawnArg1.value & AKROPOLIS_SQUARE_GLOW_STREAKS) {
                for (segmentIndex = 0; segmentIndex < 2; segmentIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    setLineG3(streak);
                    setRGB0(streak, 0, 0, 0);
                    setRGB1(streak, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                    setRGB2(streak, 0, 0, 0);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (segmentIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->outerRadius * (segmentIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (segmentIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->outerRadius * (segmentIndex + 1);
                    _akropolisSquareQueueTelephoneLight(streak, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    // This is a one-frame counted effect, including when projection rejects it.
    effectKillTask(effectWork, task);
}

/// Selects the variant used to construct subsequent telephone-light spawn parameters.
///
/// Handles `AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE`. Scripts send 1
/// during the telephone scene and 0 afterwards; any word is stored unchanged. Requires
/// the square overlay and its live room effect. Returns 0; other arguments
/// are ignored, and the payload is an integer rather than an object address.
static s32 _akropolisSquareSetTelephoneLightMode(Task* unusedTask, s32 unusedMessageId, s32 glowMode, s32 unusedSecondArg)
{
    _gAkropolisSquareTelephoneLightMode = glowMode;
    return 0;
}

/// Unused empty function retained in the overlay image; its original role is unproven.
static void _akropolisSquareNoOp(void)
{
}
