#include "rooms/dryfield_night_water_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_01100.h"
#include "actors/actor_146000.h"
#include "actors/actor_400600.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room.h"
#include "../../shared/effects/glow_draw.h"

/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/effects/water_effects.h"
#include "../../shared/room/room_events.h"
#include "../../shared/room/room_variants.h"
#include "../../shared/water_hole.h"

// Tracked model coordinates and packed water-spray recipe. Size uses perspective
// units, launch speed uses game-coordinate units per running update.
enum {
    DRYFIELD_NIGHT_WATER_HOLE_SPLASH_FIRST_PART  = 14,
    DRYFIELD_NIGHT_WATER_HOLE_SPLASH_PART_STRIDE = 3,
    DRYFIELD_NIGHT_WATER_HOLE_SPLASH_ROLL_MASK   = 0x1FF,
    DRYFIELD_NIGHT_WATER_HOLE_RIPPLE_ODDS_BIAS   = 32,
    DRYFIELD_NIGHT_WATER_HOLE_RIPPLE_HALF_SIDE   = 64,
    DRYFIELD_NIGHT_WATER_HOLE_SPRAY_SIZE         = 384,
    DRYFIELD_NIGHT_WATER_HOLE_SPRAY_CELL_UPDATES = 2,
    DRYFIELD_NIGHT_WATER_HOLE_SPRAY_LAUNCH_SPEED = 32,
    DRYFIELD_NIGHT_WATER_HOLE_SPRAY_UPWARD_BURST = 1,
    DRYFIELD_NIGHT_WATER_HOLE_SPRAY_SPAWN_ARG    = DRYFIELD_NIGHT_WATER_HOLE_SPRAY_SIZE |
                                                (DRYFIELD_NIGHT_WATER_HOLE_SPRAY_CELL_UPDATES << 12) |
                                                (DRYFIELD_NIGHT_WATER_HOLE_SPRAY_LAUNCH_SPEED << 16) |
                                                (DRYFIELD_NIGHT_WATER_HOLE_SPRAY_UPWARD_BURST << 24),
};

static s32  _roomVariantResolveWaterHole(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static void _waterHoleWaterTask(Task* task);

static s32 _roomVariantResolveShelter(RoomEventMsg* request, RoomEventMsg* reply);

/// One surface-class replacement the room applies after its default records load.
///
/// The room's list of these ends at the first entry whose `properties` is NULL.
/// Applying an entry stores `properties` in the room's slot of the active
/// stage's surface-property table and refreshes that class's cached
/// `suppressPushback` flag, so later lookups of the class see the replacement.
/// The records are this overlay's own and are borrowed only while it is loaded.
typedef struct {
    WorldCollisionSurfaceProperties* properties;   // Replacement record; NULL ends the list
    s32                              surfaceClass; // Surface class it replaces (0..7)
} _DryfieldNightWaterHoleSurfaceOverride;
STATIC_ASSERT_SIZEOF(_DryfieldNightWaterHoleSurfaceOverride, 0x8);

/// The staged event descriptor, read by the room's event task.
extern RoomDeparture gRoomDeparture;

static void _dryfieldNightWaterHoleRoomIdle(Task* task);
static void _dryfieldNightWaterHoleApplySurfaceOverrides(const _DryfieldNightWaterHoleSurfaceOverride* overrides);

static WorldCollisionGrid         _gDryfieldNightWaterHoleCollisionGrid;
static WorldCollisionOccluder     _gDryfieldNightWaterHoleOccluders[2];
static WorldCollisionTrigger      _gDryfieldNightWaterHoleViewBoundaryTriggers[12];
static WorldCollisionTrigger      _gDryfieldNightWaterHoleSwitchOnActionTriggers[9];
static WorldCollisionTrigger      _gDryfieldNightWaterHoleSwitchOffActionTriggers[8];
static WorldCoordRoomAmbientEntry _gDryfieldNightWaterHoleSwitchOnAmbientByView[12];
static WorldCoordRoomAmbientEntry _gDryfieldNightWaterHoleSwitchOffAmbientByView[12];
static WorldCoordRoomLights       _gDryfieldNightWaterHoleSwitchOnRoomLights;
static WorldCoordRoomLights       _gDryfieldNightWaterHoleSwitchOffRoomLights;

static s32 _dryfieldNightWaterHoleRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightWaterHoleCommandMessage(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldNightWaterHoleRoomActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

// Room-local script choices, task priorities and logical-view extent.
enum {
    DRYFIELD_NIGHT_WATER_HOLE_PLAYER_IDLE_CLIP             = 1,
    DRYFIELD_NIGHT_WATER_HOLE_CAP_ARRIVAL                  = 5,
    DRYFIELD_NIGHT_WATER_HOLE_CAP_LOGICAL_VIEW_IDS         = 1,
    DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_BATTLE_DELAY_FRAMES  = 3,
    DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL2_BATTLE_VIEW         = 7,
    DRYFIELD_NIGHT_WATER_HOLE_LOGICAL_VIEW_COUNT           = 11,
    DRYFIELD_NIGHT_WATER_HOLE_STALKER_FIRST_WALL_ENTRANCE  = 0,
    DRYFIELD_NIGHT_WATER_HOLE_STALKER_SECOND_WALL_ENTRANCE = 1,
    DRYFIELD_NIGHT_WATER_HOLE_STALKER_FIRST_FLOOR_ARRIVAL  = 2,
    DRYFIELD_NIGHT_WATER_HOLE_STALKER_SECOND_FLOOR_ARRIVAL = 3,
    DRYFIELD_NIGHT_WATER_HOLE_STAIR_DESCEND                = 0x100,
    DRYFIELD_NIGHT_WATER_HOLE_STAIR_SURFACE_CLASS          = 1,
    DRYFIELD_NIGHT_WATER_HOLE_STAIR_DESCENT_STEPS          = 2,
    DRYFIELD_NIGHT_WATER_HOLE_STAIR_ASCENT_STEPS           = 5,
    DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_YAW_UNITS            = 16,
    DRYFIELD_NIGHT_WATER_HOLE_DRIVEWAY_WARP_SLOTS          = (1 << 4) | 2,
    DRYFIELD_NIGHT_WATER_HOLE_UNDERPASS_WARP_SLOTS         = (2 << 4) | 2,
    DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_ARRIVAL1_ACTION      = 1,
    DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_ARRIVAL2_ACTION      = 2,
    DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_SHELTER_DOOR_COMMAND = 2,
    DRYFIELD_NIGHT_WATER_HOLE_CAP_SWITCH_ON_INTERACTION    = 6,
    DRYFIELD_NIGHT_WATER_HOLE_UNDERPASS_MAP_FLAG           = 462,
    DRYFIELD_NIGHT_WATER_HOLE_WARP1_SOUND                  = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 3),
    DRYFIELD_NIGHT_WATER_HOLE_WARP2_ARRIVAL_SOUND          = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 2),
    DRYFIELD_NIGHT_WATER_HOLE_WARP2_DEPARTURE_SOUND        = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 1),
    DRYFIELD_NIGHT_WATER_HOLE_WARP3_ARRIVAL_SOUND          = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 8),
};

static AnimationPackedPose _gDryfieldNightWaterHoleAnimation03004Bank1[6] = {
#include "assets/dryfield_night_water_hole_animation_03004_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightWaterHoleAnimation03004Bank4[64] = {
#include "assets/dryfield_night_water_hole_animation_03004_bank4.inc"
};

static AnimationRecord _gDryfieldNightWaterHoleAnimation03004Records[141] = {
#include "assets/dryfield_night_water_hole_animation_03004_records.inc"
};

static u16 _gDryfieldNightWaterHoleAnimation03004Indices[20] = {
#include "assets/dryfield_night_water_hole_animation_03004_indices.inc"
};

static AnimationSet _gDryfieldNightWaterHoleAnimation03004 = {
    _gDryfieldNightWaterHoleAnimation03004Records,
    _gDryfieldNightWaterHoleAnimation03004Indices,
    { NULL, _gDryfieldNightWaterHoleAnimation03004Bank1, NULL, NULL, _gDryfieldNightWaterHoleAnimation03004Bank4, NULL, NULL, NULL },
};

/// Bodyless priority-32 controller for a resolved departure through the Shelter door.
///
/// Borrows the staged `gRoomDeparture` until the departure has been issued.
static TaskDesc _gDryfieldNightWaterHoleShelterDepartureTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventDepartureTask, { .value = 0 } };

/// Room-transition, key-item, trigger-action and Shelter-door message dispatch.
///
/// The initialized room task borrows this table while loaded. The final entry
/// rejects unsupported messages; the four live handlers consume their payloads synchronously.
static TaskMessageEntry _gDryfieldNightWaterHoleRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveWaterHole },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightWaterHoleRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightWaterHoleRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightWaterHoleCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// The one player clip appended to the equipped animation bank for the arrival scene.
///
/// Becomes clip `ANIMATION_BANK_BASE_SET_COUNT`; its data stays borrowed during playback.
static AnimationSet* _gDryfieldNightWaterHoleArrivalAnimationSets[1] = {
    &_gDryfieldNightWaterHoleAnimation03004,
};

/// Copies the arrival clip pointer into the loaded player's first extension slot.
///
/// The one-word source is borrowed through dispatch; the copied clip remains room-owned.
static AnimationBankCopyRequest _gDryfieldNightWaterHoleArrivalAnimationBankExtension = { _gDryfieldNightWaterHoleArrivalAnimationSets, ARRAY_SIZE(_gDryfieldNightWaterHoleArrivalAnimationSets) };

/// Restarts the room's appended player clip with world collision enabled.
///
/// The script resolves the equipped bank in a copy of this request before dispatch.
/// The clip's visual action is unproven; playback borrows the loaded room animation.
static AnimationPlayRequest _gDryfieldNightWaterHoleArrivalPlayerAnimation = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Holds the player in the equipped bank's idle clip before the arrival scene.
///
/// Restarts without blending and enables world collision; the script replaces
/// the initial bank index in its stack copy before dispatch.
static AnimationPlayRequest _gDryfieldNightWaterHoleArrivalPlayerIdleAnimation = { { .index = 1 }, DRYFIELD_NIGHT_WATER_HOLE_PLAYER_IDLE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Starts placement zero's first Zebra Stalker wall entrance during arrival action 1.
///
/// Borrowed through dispatch; the receiver latches the action and ignores the context.
static ActorCommand _gDryfieldNightWaterHoleStalkerFirstWallEntranceCommand = { { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE } }, DRYFIELD_NIGHT_WATER_HOLE_STALKER_FIRST_WALL_ENTRANCE };

/// Starts placement zero's second Zebra Stalker wall entrance during arrival action 2.
///
/// Borrowed through dispatch; the receiver latches the action and ignores the context.
static ActorCommand _gDryfieldNightWaterHoleStalkerSecondWallEntranceCommand = { { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE } }, DRYFIELD_NIGHT_WATER_HOLE_STALKER_SECOND_WALL_ENTRANCE };

/// Starts the Zebra Stalker on the first floor position after the arrival event.
///
/// Used for arrival IDs other than 2. The receiver latches command 2, then
/// places at (11000, 0, -1000) with yaw 3072 and enters ordinary behavior.
static ActorCommand _gDryfieldNightWaterHoleStalkerFirstFloorArrivalCommand = { { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE } }, DRYFIELD_NIGHT_WATER_HOLE_STALKER_FIRST_FLOOR_ARRIVAL };

/// Starts the Zebra Stalker on the second floor position after the arrival event.
///
/// Used for arrival ID 2. The receiver latches command 3, then places at
/// (15000, 0, -3000) with yaw 1024 and enters ordinary behavior.
static ActorCommand _gDryfieldNightWaterHoleStalkerSecondFloorArrivalCommand = { { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE } }, DRYFIELD_NIGHT_WATER_HOLE_STALKER_SECOND_FLOOR_ARRIVAL };

/// Arrival-action-1 player turn toward +X, a quarter-turn in the root's parent frame.
///
/// The turn handler copies only yaw; the zero position is unused.
static ActorTransform _gDryfieldNightWaterHoleArrivalPlayerTurn = { { 0, 0, 0, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// First-visit arrival-action-2 sequence with the second Stalker wall entrance.
///
/// Plays CAP command 5 and the appended player clip, then waits for CAP cues,
/// delays battle engagement and selects logical view 7 before restoring control.
/// The interpreter borrows this terminated array and its payloads while the room stays loaded.
static EvsCommand _gDryfieldNightWaterHoleArrival2Script[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_WATER_HOLE_CAP_ARRIVAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightWaterHoleArrivalAnimationBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightWaterHoleArrivalPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = DRYFIELD_NIGHT_WATER_HOLE_CAP_LOGICAL_VIEW_IDS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightWaterHoleArrivalPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldNightWaterHoleStalkerSecondWallEntranceCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_BATTLE_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL2_BATTLE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// First-visit arrival-action-1 sequence with the first Stalker wall entrance.
///
/// Plays CAP command 5 and the appended player clip, turns the player to +X,
/// then engages battle and restores control. CAP cues order the scene steps.
/// The interpreter borrows this terminated array and its payloads while the room stays loaded.
static EvsCommand _gDryfieldNightWaterHoleArrival1Script[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_WATER_HOLE_CAP_ARRIVAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightWaterHoleArrivalAnimationBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightWaterHoleArrivalPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = DRYFIELD_NIGHT_WATER_HOLE_CAP_LOGICAL_VIEW_IDS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightWaterHoleArrivalPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldNightWaterHoleStalkerFirstWallEntranceCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gDryfieldNightWaterHoleArrivalPlayerTurn } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_BATTLE_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Bodyless priority-192 water drawer, spawned while the Shelter route is closed.
static TaskDesc _gDryfieldNightWaterHoleWaterTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _waterHoleWaterTask, { .value = 0 } };

/// Two world-space water patches at the shared session height, followed by a Y=-1 sentinel.
///
/// Each frame draws 64 columns in each of two strips per patch. At most 256
/// accepted quads and their draw modes consume 0x3000 bytes of the borrowed actor arena.
static WaterHoleSurface _gWaterHoleSurfaces[3] = {
    { 4000, -2000, 8000, 2000, WATER_HOLE_SURFACE_HEIGHT },
    { 10000, -4000, 13000, 2000, WATER_HOLE_SURFACE_HEIGHT },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// Two world-space light-shaft endpoint pairs for logical views 3/4.
///
/// XYZ uses signed game-coordinate units. The splash/light task draws both
/// pairs while underpass switch 1 is on; each draw borrows exactly two points.
static const SVECTOR _gDryfieldNightWaterHoleShaftGroup1Endpoints[2][2] SECTION(".data") = {
    {
        { 9500, -1835, -100, 0 },
        { 0x2904, -1835, -100, 0 },
    },
    {
        { 9500, -1750, -75, 0 },
        { 0x2904, -1750, -75, 0 },
    },
};

/// Two world-space light-shaft endpoint pairs for logical views 4/6/9/11.
///
/// XYZ uses signed game-coordinate units. The splash/light task draws both
/// pairs while underpass switch 1 is on; each draw borrows exactly two points.
static const SVECTOR _gDryfieldNightWaterHoleShaftGroup2Endpoints[2][2] SECTION(".data") = {
    {
        { 0x37DC, -1835, -3900, 0 },
        { 0x3BC4, -1835, -3900, 0 },
    },
    {
        { 0x37DC, -1750, -3925, 0 },
        { 0x3BC4, -1750, -3925, 0 },
    },
};

/// Two world-space light-shaft endpoint pairs for logical view 7.
///
/// XYZ uses signed game-coordinate units. The splash/light task draws both
/// pairs while underpass switch 1 is on; each draw borrows exactly two points.
static const SVECTOR _gDryfieldNightWaterHoleShaftGroup3Endpoints[2][2] SECTION(".data") = {
    {
        { 0x477C, -1835, -2120, 0 },
        { 0x4B64, -1835, -2120, 0 },
    },
    {
        { 0x477C, -1750, -2095, 0 },
        { 0x4B64, -1750, -2095, 0 },
    },
};

/// Previous composed XYZ samples for player splash coordinates 14 and 17.
///
/// One signed-halfword sample per part; inactive frames retain the last sample.
/// The singleton emitter initializes XYZ from the cached transform, then updates
/// them after each splash test. The fourth halfword is never read or written.
static SVECTOR _gDryfieldNightWaterHolePreviousSplashPartPositions[2] = { 0 };

WorldCollisionRoomResources gDryfieldNightWaterHoleRoomCollisionResources[4] = {
    { &_gDryfieldNightWaterHoleCollisionGrid, _gDryfieldNightWaterHoleViewBoundaryTriggers, _gDryfieldNightWaterHoleSwitchOnActionTriggers, _gDryfieldNightWaterHoleOccluders },
    { &_gDryfieldNightWaterHoleCollisionGrid, _gDryfieldNightWaterHoleViewBoundaryTriggers, _gDryfieldNightWaterHoleSwitchOffActionTriggers, _gDryfieldNightWaterHoleOccluders },
    { &_gDryfieldNightWaterHoleCollisionGrid, _gDryfieldNightWaterHoleViewBoundaryTriggers, _gDryfieldNightWaterHoleSwitchOnActionTriggers, _gDryfieldNightWaterHoleOccluders },
    { &_gDryfieldNightWaterHoleCollisionGrid, _gDryfieldNightWaterHoleViewBoundaryTriggers, _gDryfieldNightWaterHoleSwitchOffActionTriggers, _gDryfieldNightWaterHoleOccluders },
};

WorldCoordRoomLighting gDryfieldNightWaterHoleRoomLighting[4] = {
    { &_gDryfieldNightWaterHoleSwitchOnRoomLights, _gDryfieldNightWaterHoleSwitchOnAmbientByView },
    { &_gDryfieldNightWaterHoleSwitchOffRoomLights, _gDryfieldNightWaterHoleSwitchOffAmbientByView },
    { &_gDryfieldNightWaterHoleSwitchOnRoomLights, _gDryfieldNightWaterHoleSwitchOnAmbientByView },
    { &_gDryfieldNightWaterHoleSwitchOffRoomLights, _gDryfieldNightWaterHoleSwitchOffAmbientByView },
};

/// Room 1's logical-view map while underpass switch 1 is on.
///
/// Logical views 1..11 index bytes 0..10, yielding 1-based camera/image/sprite
/// indices. The final zero byte is retained storage outside the view count.
static u8 _gDryfieldNightWaterHoleRoom1ViewMap[12] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    21,
    22,
    23,
    0,
};

/// Room 2's logical-view map while underpass switch 1 is off.
///
/// Logical views 1..11 index bytes 0..10, yielding 1-based camera/image/sprite
/// indices. The final zero byte is retained storage outside the view count.
static u8 _gDryfieldNightWaterHoleRoom2ViewMap[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    24,
    25,
    26,
    0,
};

/// Room 3's logical-view map while underpass switch 1 is on.
///
/// Logical views 1..11 index bytes 0..10, yielding 1-based camera/image/sprite
/// indices. The final zero byte is retained storage outside the view count.
/// Rooms 3/4 substitute logical views 7/8; the story role of that choice is unproven.
static u8 _gDryfieldNightWaterHoleRoom3ViewMap[12] = {
    1,
    2,
    3,
    4,
    5,
    6,
    10,
    9,
    21,
    22,
    23,
    0,
};

/// Room 4's logical-view map while underpass switch 1 is off.
///
/// Logical views 1..11 index bytes 0..10, yielding 1-based camera/image/sprite
/// indices. The final zero byte is retained storage outside the view count.
/// Rooms 3/4 substitute logical views 7/8; the story role of that choice is unproven.
static u8 _gDryfieldNightWaterHoleRoom4ViewMap[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    20,
    19,
    24,
    25,
    26,
    0,
};

u8* gDryfieldNightWaterHoleViewMaps[4] = {
    _gDryfieldNightWaterHoleRoom1ViewMap,
    _gDryfieldNightWaterHoleRoom2ViewMap,
    _gDryfieldNightWaterHoleRoom3ViewMap,
    _gDryfieldNightWaterHoleRoom4ViewMap,
};

ViewCount gDryfieldNightWaterHoleViewCounts[4] = {
    DRYFIELD_NIGHT_WATER_HOLE_LOGICAL_VIEW_COUNT,
    DRYFIELD_NIGHT_WATER_HOLE_LOGICAL_VIEW_COUNT,
    DRYFIELD_NIGHT_WATER_HOLE_LOGICAL_VIEW_COUNT,
    DRYFIELD_NIGHT_WATER_HOLE_LOGICAL_VIEW_COUNT,
};

DirectionWarpEntry gDryfieldNightWaterHoleWarpEntries[3] = {
    { { { .word = 0 }, 7400, 0, -1300 }, { 0, 0, 0, 0 }, { { .word = 0 }, 7400, 0, -1300 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_WATER_HOLE_WARP1_SOUND, DRYFIELD_NIGHT_WATER_HOLE_WARP1_SOUND, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 0x57C0, -534, -3076 }, { 0, 0, 0, 0 }, { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 0x57C0, -534, -3076 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_WATER_HOLE_WARP2_ARRIVAL_SOUND, DRYFIELD_NIGHT_WATER_HOLE_WARP2_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_WATER_HOLE_UNDERPASS_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 4680, 0, -954 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 4680, 0, -954 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_WATER_HOLE_WARP3_ARRIVAL_SOUND, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_WATER },
};

enum {
    DRYFIELD_NIGHT_WATER_HOLE_GRID_CELL_COUNT_X = 6,
    DRYFIELD_NIGHT_WATER_HOLE_GRID_CELL_COUNT_Z = 2,
};

/// Room-space collision normals, using ONE (4096) for unit length.
static SVECTOR _gDryfieldNightWaterHoleCollisionGridNormals[10] = {
#include "assets/dryfield_night_water_hole_collision_03990_normals.inc"
};

/// Collision mesh vertices in signed integer room-coordinate units.
static SVECTOR _gDryfieldNightWaterHoleCollisionGridVerts[54] = {
#include "assets/dryfield_night_water_hole_collision_03990_verts.inc"
};

/// Indexed collision quads and their room-local surface classes.
static WorldCollisionGridFace _gDryfieldNightWaterHoleCollisionGridFaces[23] = {
#include "assets/dryfield_night_water_hole_collision_03990_faces.inc"
};

/// Twelve packed face-index lists, terminated by WORLD_COLLISION_GRID_CELL_END.
///
/// The cell table selects each list's start in halfwords. Retained zeros
/// between some lists are outside the traversed face indices.
static s16 _gDryfieldNightWaterHoleCollisionGridCells[92] = {
#include "assets/dryfield_night_water_hole_collision_03990_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightWaterHoleCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..91 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_WATER_HOLE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightWaterHoleCollisionGridCells[(halfwordOffset)])
/// X-major cell-list starts, indexed by x * cellCountZ + z.
static s16* _gDryfieldNightWaterHoleCollisionGridTable[DRYFIELD_NIGHT_WATER_HOLE_GRID_CELL_COUNT_X * DRYFIELD_NIGHT_WATER_HOLE_GRID_CELL_COUNT_Z] = {
#include "assets/dryfield_night_water_hole_collision_03990_table.inc"
};
#undef DRYFIELD_NIGHT_WATER_HOLE_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh shared by the night water hole's four room selectors.
///
/// The 6-by-2 square-cell index has origin (4000, -5000) in room X/Z and
/// 4000 game units per cell. Setup binds viewCoord to the live view before
/// publishing the grid; this record and its pools must stay loaded while active.
static WorldCollisionGrid _gDryfieldNightWaterHoleCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightWaterHoleCollisionGridNormals,
    .vertices    = _gDryfieldNightWaterHoleCollisionGridVerts,
    .faces       = _gDryfieldNightWaterHoleCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightWaterHoleCollisionGridTable,
    .xBias       = -4000,
    .zBias       = 5000,
    .cellCountX  = DRYFIELD_NIGHT_WATER_HOLE_GRID_CELL_COUNT_X,
    .cellCountZ  = DRYFIELD_NIGHT_WATER_HOLE_GRID_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightWaterHoleCollisionGridFaces),
};

ViewCamera gDryfieldNightWaterHoleViewCameras[DRYFIELD_NIGHT_WATER_HOLE_MAPPED_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -0x34B7, 0x78AD, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -0x34B7, 0x8C35, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -3186, 0, -2573 }, { -25, 4095, 31 }, { 2573, 40, -3186 } }, { -8630, 1300, -20 } }, 246 },
    { { { { -2620, 0, 3148 }, { 219, 4086, 182 }, { -3140, 285, -2613 } }, { -9960, 1360, 430 } }, 246 },
    { { { { -2395, 0, -3322 }, { -237, 4085, 171 }, { 3313, 293, -2389 } }, { -9120, 1360, 430 } }, 246 },
    { { { { -3590, 0, -1971 }, { -90, 4091, 164 }, { 1968, 188, -3586 } }, { -7990, 1450, -1820 } }, 541 },
    { { { { -1831, 0, 3663 }, { 153, 4092, 76 }, { -3660, 171, -1830 } }, { -0x2C2E, 1460, 130 } }, 541 },
    { { { { -3154, 0, -2612 }, { -258, 4075, 311 }, { 2599, 404, -3139 } }, { -7920, 1660, -1120 } }, 541 },
};

/// Mapped view 1's image-only batch list.
///
/// The zero-count leading record preserves decoded image strips; the terminal
/// record selects no source sprites. Its address is also retained in the
/// view's empty source slot and is never dereferenced as a source array.
static SpriteBatch _gDryfieldNightWaterHoleView1Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 2's image-only batch list.
///
/// The zero-count leading record preserves decoded image strips; the terminal
/// record selects no source sprites. Its address is also retained in the
/// view's empty source slot and is never dereferenced as a source array.
static SpriteBatch _gDryfieldNightWaterHoleView2Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

// Room foreground textures use 8-bit pages at VRAM Y zero and one CLUT row.
enum {
    DRYFIELD_NIGHT_WATER_HOLE_SPRITE_TEXTURE_8_BIT = 1,
    DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960      = getTPage(DRYFIELD_NIGHT_WATER_HOLE_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896      = getTPage(DRYFIELD_NIGHT_WATER_HOLE_SPRITE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT          = getClut(0, 255),
    DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR = 128,
};

/// The 21 foreground sprite rectangles for mapped view 3.
///
/// Positions and sizes are pixels relative to the view centre; depth is an
/// unscaled ordering value. The view's batch ranges cover the entire array.
/// Packet allocation snapshots texture/geometry into both frame buffers;
/// cached drawing still reads these overlay-owned mutable source depths.
static SpriteSource _gDryfieldNightWaterHoleView3Sources[21] = {
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 64, 40 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 3's foreground ranges over source elements 0..7, 8..15 and 16..20.
///
/// The leading zero count preserves decoded image strips; SPRITE_BATCH_END
/// ends the list. Hidden/excluded flags start clear; serialized field_6 bytes
/// are retained without assigning them an unproven rendering role.
static SpriteBatch _gDryfieldNightWaterHoleView3Batches[5] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 8 },
    { .firstSprite = 8, .spriteCount = 8, .field_6 = { 2, 0 } },
    { .firstSprite = 16, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterHoleView3Sources) - 16, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 33 foreground sprite rectangles for mapped view 4.
///
/// Positions and sizes are pixels relative to the view centre; depth is an
/// unscaled ordering value. The view's batch ranges cover the entire array.
/// Packet allocation snapshots texture/geometry into both frame buffers;
/// cached drawing still reads these overlay-owned mutable source depths.
static SpriteSource _gDryfieldNightWaterHoleView4Sources[33] = {
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 4's foreground ranges over source elements 0..14 and 15..32.
///
/// The leading zero count preserves decoded image strips; SPRITE_BATCH_END
/// ends the list. Hidden/excluded flags start clear; serialized field_6 bytes
/// are retained without assigning them an unproven rendering role.
static SpriteBatch _gDryfieldNightWaterHoleView4Batches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 15, .field_6 = { 1, 0 } },
    { .firstSprite = 15, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterHoleView4Sources) - 15 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 18 foreground sprite rectangles for mapped view 5.
///
/// Positions and sizes are pixels relative to the view centre; depth is an
/// unscaled ordering value. The view's batch ranges cover the entire array.
/// Packet allocation snapshots texture/geometry into both frame buffers;
/// cached drawing still reads these overlay-owned mutable source depths.
static SpriteSource _gDryfieldNightWaterHoleView5Sources[18] = {
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 128 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 5's foreground ranges over source elements 0..17.
///
/// The leading zero count preserves decoded image strips; SPRITE_BATCH_END
/// ends the list. Hidden/excluded flags start clear; serialized field_6 bytes
/// are retained without assigning them an unproven rendering role.
static SpriteBatch _gDryfieldNightWaterHoleView5Batches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterHoleView5Sources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 19 foreground sprite rectangles for mapped view 6.
///
/// Positions and sizes are pixels relative to the view centre; depth is an
/// unscaled ordering value. The view's batch ranges cover the entire array.
/// Packet allocation snapshots texture/geometry into both frame buffers;
/// cached drawing still reads these overlay-owned mutable source depths.
static SpriteSource _gDryfieldNightWaterHoleView6Sources[19] = {
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 72, 40 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 88, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 6's foreground ranges over source elements 0..8, 9..16 and 17..18.
///
/// The leading zero count preserves decoded image strips; SPRITE_BATCH_END
/// ends the list. Hidden/excluded flags start clear; serialized field_6 bytes
/// are retained without assigning them an unproven rendering role.
static SpriteBatch _gDryfieldNightWaterHoleView6Batches[5] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 9, .field_6 = { 1, 0 } },
    { .firstSprite = 9, .spriteCount = 8, .field_6 = { 2, 0 } },
    { .firstSprite = 17, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterHoleView6Sources) - 17 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 7's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView7Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 8's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView8Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 9's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView9Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 10's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView10Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 11's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView11Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 12's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView12Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 21 foreground sprite rectangles for mapped view 13.
///
/// Positions are pixels relative to the drawing origin; dimensions are pixels.
/// Texture coordinates are texels and depth is an unscaled OT sorting value.
/// Batches cover the complete array. Allocation snapshots texture and geometry
/// into both frame buffers; cached linking still borrows these source depths.
static SpriteSource _gDryfieldNightWaterHoleView13Sources[21] = {
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 40 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 48, 128 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 56, 72 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 13's foreground ranges over source elements 0..7, 8..15 and 16..20.
///
/// A zero-count first record retains decoded image strips; SPRITE_BATCH_END
/// ends the list. All ranges start visible and included in cached packets.
/// The serialized field_6 bytes have no proven role and are retained intact.
static SpriteBatch _gDryfieldNightWaterHoleView13Batches[5] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 8, .field_6 = { 2, 0 } },
    { .firstSprite = 8, .spriteCount = 8 },
    { .firstSprite = 16, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterHoleView13Sources) - 16, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 33 foreground sprite rectangles for mapped view 14.
///
/// Positions are pixels relative to the drawing origin; dimensions are pixels.
/// Texture coordinates are texels and depth is an unscaled OT sorting value.
/// Batches cover the complete array. Allocation snapshots texture and geometry
/// into both frame buffers; cached linking still borrows these source depths.
static SpriteSource _gDryfieldNightWaterHoleView14Sources[33] = {
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 40, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 32, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 16, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 0, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 48, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_896, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 168 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 14's foreground ranges over source elements 0..14 and 15..32.
///
/// A zero-count first record retains decoded image strips; SPRITE_BATCH_END
/// ends the list. All ranges start visible and included in cached packets.
/// The serialized field_6 bytes have no proven role and are retained intact.
static SpriteBatch _gDryfieldNightWaterHoleView14Batches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 15, .field_6 = { 1, 0 } },
    { .firstSprite = 15, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterHoleView14Sources) - 15 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 18 foreground sprite rectangles for mapped view 15.
///
/// Positions are pixels relative to the drawing origin; dimensions are pixels.
/// Texture coordinates are texels and depth is an unscaled OT sorting value.
/// Batches cover the complete array. Allocation snapshots texture and geometry
/// into both frame buffers; cached linking still borrows these source depths.
static SpriteSource _gDryfieldNightWaterHoleView15Sources[18] = {
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 15's foreground ranges over source elements 0..17.
///
/// A zero-count first record retains decoded image strips; SPRITE_BATCH_END
/// ends the list. All ranges start visible and included in cached packets.
static SpriteBatch _gDryfieldNightWaterHoleView15Batches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterHoleView15Sources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 19 foreground sprite rectangles for mapped view 16.
///
/// Positions are pixels relative to the drawing origin; dimensions are pixels.
/// Texture coordinates are texels and depth is an unscaled OT sorting value.
/// Batches cover the complete array. Allocation snapshots texture and geometry
/// into both frame buffers; cached linking still borrows these source depths.
static SpriteSource _gDryfieldNightWaterHoleView16Sources[19] = {
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 72, 80 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 120 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_WATER_HOLE_SPRITE_PAGE_960, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_CLUT, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 16's foreground ranges over source elements 0..7, 8..16 and 17..18.
///
/// A zero-count first record retains decoded image strips; SPRITE_BATCH_END
/// ends the list. All ranges start visible and included in cached packets.
/// The serialized field_6 bytes have no proven role and are retained intact.
static SpriteBatch _gDryfieldNightWaterHoleView16Batches[5] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 8, .field_6 = { 1, 0 } },
    { .firstSprite = 8, .spriteCount = 9, .field_6 = { 2, 0 } },
    { .firstSprite = 17, .spriteCount = ARRAY_SIZE(_gDryfieldNightWaterHoleView16Sources) - 17 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 17's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView17Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 18's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView18Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 19's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView19Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 20's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView20Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 21's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView21Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 22's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView22Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 23's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView23Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 24's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView24Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 25's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView25Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 26's image-only batch list.
///
/// The zero-count first record retains decoded image strips; the second ends
/// the list. Its retained address in sources.empty supplies no source elements.
static SpriteBatch _gDryfieldNightWaterHoleView26Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gDryfieldNightWaterHoleSpriteViews[DRYFIELD_NIGHT_WATER_HOLE_MAPPED_VIEW_COUNT] = {
    { .sources = { .empty = _gDryfieldNightWaterHoleView1Batches }, .batches = _gDryfieldNightWaterHoleView1Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView2Batches }, .batches = _gDryfieldNightWaterHoleView2Batches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterHoleView3Sources }, .batches = _gDryfieldNightWaterHoleView3Batches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterHoleView4Sources }, .batches = _gDryfieldNightWaterHoleView4Batches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterHoleView5Sources }, .batches = _gDryfieldNightWaterHoleView5Batches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterHoleView6Sources }, .batches = _gDryfieldNightWaterHoleView6Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView7Batches }, .batches = _gDryfieldNightWaterHoleView7Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView8Batches }, .batches = _gDryfieldNightWaterHoleView8Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView9Batches }, .batches = _gDryfieldNightWaterHoleView9Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView10Batches }, .batches = _gDryfieldNightWaterHoleView10Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView11Batches }, .batches = _gDryfieldNightWaterHoleView11Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView12Batches }, .batches = _gDryfieldNightWaterHoleView12Batches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterHoleView13Sources }, .batches = _gDryfieldNightWaterHoleView13Batches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterHoleView14Sources }, .batches = _gDryfieldNightWaterHoleView14Batches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterHoleView15Sources }, .batches = _gDryfieldNightWaterHoleView15Batches, .drawAreas = NULL },
    { .sources = { .elements = _gDryfieldNightWaterHoleView16Sources }, .batches = _gDryfieldNightWaterHoleView16Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView17Batches }, .batches = _gDryfieldNightWaterHoleView17Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView18Batches }, .batches = _gDryfieldNightWaterHoleView18Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView19Batches }, .batches = _gDryfieldNightWaterHoleView19Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView20Batches }, .batches = _gDryfieldNightWaterHoleView20Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView21Batches }, .batches = _gDryfieldNightWaterHoleView21Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView22Batches }, .batches = _gDryfieldNightWaterHoleView22Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView23Batches }, .batches = _gDryfieldNightWaterHoleView23Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView24Batches }, .batches = _gDryfieldNightWaterHoleView24Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView25Batches }, .batches = _gDryfieldNightWaterHoleView25Batches, .drawAreas = NULL },
    { .sources = { .empty = _gDryfieldNightWaterHoleView26Batches }, .batches = _gDryfieldNightWaterHoleView26Batches, .drawAreas = NULL },
};

/// Twelve directed logical-view boundaries linking views 2 through 8 in all four rooms.
///
/// Each adjacent pair has one trigger in each direction. Positions/radii use
/// room units and normals use Q12. Setup mutates links and coordinates through
/// the final LAST-marked entry; storage must remain loaded until unlinked.
static WorldCollisionTrigger _gDryfieldNightWaterHoleViewBoundaryTriggers[12] = {
    { NULL, NULL, NULL, { 6101, -1152, -1034, 0 }, { { 199, -2176, -1004, 0 }, { -200, -2176, 1004, 0 }, { 199, 2176, -1004, 0 }, { -200, 2176, 1004, 0 } }, { 4021, 0, 797, 0 }, { 0, 0, ONE, 0 }, 2401, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6304, -1104, -1024, 0 }, { { -154, -2128, 1004, 0 }, { 144, -2128, -1022, 0 }, { -154, 2128, 1004, 0 }, { 144, 2128, -1022, 0 } }, { -4056, 0, -597, 0 }, { 0, 0, ONE, 0 }, 2360, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9499, -1248, -1060, 0 }, { { -345, -2272, -963, 0 }, { 345, -2272, 963, 0 }, { -345, 2272, -963, 0 }, { 345, 2272, 963, 0 } }, { 3869, 0, -1388, 0 }, { 0, 0, ONE, 0 }, 2482, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9632, -1200, -1026, 0 }, { { 340, -2224, 960, 0 }, { -350, -2224, -966, 0 }, { 340, 2224, 960, 0 }, { -350, 2224, -966, 0 } }, { -3865, 0, 1383, 0 }, { 0, 0, ONE, 0 }, 2442, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A9F, -1088, -1922, 0 }, { { 919, -2112, 432, 0 }, { -929, -2112, -443, 0 }, { 919, 2112, 432, 0 }, { -929, 2112, -443, 0 } }, { -1765, 0, 3723, 0 }, { 0, 0, ONE, 0 }, 2346, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A7E, -1168, -1858, 0 }, { { -927, -2192, -439, 0 }, { 923, -2192, 434, 0 }, { -927, 2192, -439, 0 }, { 923, 2192, 434, 0 } }, { 1748, 0, -3710, 0 }, { 0, 0, ONE, 0 }, 2415, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x319F, -1248, -2945, 0 }, { { 435, -2272, -931, 0 }, { -441, -2272, 919, 0 }, { 435, 2272, -931, 0 }, { -441, 2272, 919, 0 } }, { 3717, 0, 1758, 0 }, { 0, 0, ONE, 0 }, 2482, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x323F, -1136, -2976, 0 }, { { -441, -2160, 919, 0 }, { 435, -2160, -931, 0 }, { -441, 2160, 919, 0 }, { 435, 2160, -931, 0 } }, { -3706, 0, -1755, 0 }, { 0, 0, ONE, 0 }, 2387, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x415F, -1200, -2912, 0 }, { { 198, -2224, 1001, 0 }, { -202, -2224, -1006, 0 }, { 198, 2224, 1001, 0 }, { -202, 2224, -1006, 0 } }, { -4027, 0, 801, 0 }, { 0, 0, ONE, 0 }, 2442, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40FF, -1328, -2977, 0 }, { { -205, -2352, -1008, 0 }, { 195, -2352, 999, 0 }, { -205, 2352, -1008, 0 }, { 195, 2352, 999, 0 } }, { 4021, 0, -803, 0 }, { 0, 0, ONE, 0 }, 2560, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x531F, -1376, -2978, 0 }, { { 0, -2400, 1024, 0 }, { 0, -2400, -1023, 0 }, { 0, 2400, 1024, 0 }, { 0, 2400, -1023, 0 } }, { -4116, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2598, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x523F, -1408, -2978, 0 }, { { 51, -2432, -1022, 0 }, { -50, -2432, 1022, 0 }, { 51, 2432, -1022, 0 }, { -50, 2432, 1022, 0 } }, { 4093, 0, 200, 0 }, { 0, 0, ONE, 0 }, 2635, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Switch-on interaction regions for rooms 1 and 3, including CAP command 6.
///
/// Covers driveway/underpass warps, the stair flight, Shelter-door command 2
/// and automatic arrival actions 1/2. The final LAST-marked trigger is included.
/// Links, coordinates and hit latches are mutable and borrowed while loaded.
static WorldCollisionTrigger _gDryfieldNightWaterHoleSwitchOnActionTriggers[9] = {
    { NULL, NULL, NULL, { 7376, -48, -1616, 0 }, { { -719, 0, -336, 0 }, { 720, 0, -336, 0 }, { -719, 0, 336, 0 }, { 720, 0, 336, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, ONE, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY, DRYFIELD_NIGHT_WATER_HOLE_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x58B0, -1280, -2992, 0 }, { { 0, 1344, -1199, 0 }, { 0, 1344, 1200, 0 }, { 0, -1344, -1199, 0 }, { 0, -1344, 1200, 0 } }, { -4101, 0, 0, 0 }, { -ONE, 0, 0, 0 }, 1801, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, DRYFIELD_NIGHT_WATER_HOLE_UNDERPASS_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5860, -403, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, ONE, 0, 0 }, { ONE, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING | DRYFIELD_NIGHT_WATER_HOLE_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, (DRYFIELD_NIGHT_WATER_HOLE_STAIR_SURFACE_CLASS << 4) | DRYFIELD_NIGHT_WATER_HOLE_STAIR_DESCENT_STEPS, 3 * ACTOR_TRANSFORM_ANGLE_TURN / (4 * DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_YAW_UNITS), WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING, (DRYFIELD_NIGHT_WATER_HOLE_STAIR_SURFACE_CLASS << 4) | DRYFIELD_NIGHT_WATER_HOLE_STAIR_ASCENT_STEPS, ACTOR_TRANSFORM_ANGLE_TURN / (4 * DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_YAW_UNITS), WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_SHELTER_DOOR_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x34B0, -64, -2961, 0 }, { { 398, 0, -1136, 0 }, { 718, 0, -951, 0 }, { -718, 0, 952, 0 }, { -398, 0, 1137, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, ONE, 0 }, 1200, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_ARRIVAL1_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4100, -64, -5409, 0 }, { { -838, 0, -1072, 0 }, { 491, 0, -1269, 0 }, { -491, 0, 1270, 0 }, { 838, 0, 1073, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1360, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_ARRIVAL2_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 7376, -64, -2176, 0 }, { { -959, 0, 176, 0 }, { 960, 0, 176, 0 }, { -959, 0, 848, 0 }, { 960, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, ONE, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY, DRYFIELD_NIGHT_WATER_HOLE_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40A0, -64, -4416, 0 }, { { -1007, 0, 464, 0 }, { 944, 0, 464, 0 }, { -1007, 0, 1392, 0 }, { 944, 0, 1392, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, ONE, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WATER_HOLE_CAP_SWITCH_ON_INTERACTION, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Switch-off interaction regions for rooms 2 and 4, without the CAP-command-6 region.
///
/// Covers driveway/underpass warps, the stair flight, Shelter-door command 2
/// and automatic arrival actions 1/2. The larger driveway region is the included
/// LAST entry. Links, coordinates and hit latches are borrowed while loaded.
static WorldCollisionTrigger _gDryfieldNightWaterHoleSwitchOffActionTriggers[8] = {
    { NULL, NULL, NULL, { 7376, -48, -1616, 0 }, { { -719, 0, -336, 0 }, { 720, 0, -336, 0 }, { -719, 0, 336, 0 }, { 720, 0, 336, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, ONE, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY, DRYFIELD_NIGHT_WATER_HOLE_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x58B0, -1280, -2992, 0 }, { { 0, 1344, -1199, 0 }, { 0, 1344, 1200, 0 }, { 0, -1344, -1199, 0 }, { 0, -1344, 1200, 0 } }, { -4101, 0, 0, 0 }, { -ONE, 0, 0, 0 }, 1801, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, DRYFIELD_NIGHT_WATER_HOLE_UNDERPASS_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5860, -403, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, ONE, 0, 0 }, { ONE, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING | DRYFIELD_NIGHT_WATER_HOLE_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, (DRYFIELD_NIGHT_WATER_HOLE_STAIR_SURFACE_CLASS << 4) | DRYFIELD_NIGHT_WATER_HOLE_STAIR_DESCENT_STEPS, 3 * ACTOR_TRANSFORM_ANGLE_TURN / (4 * DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_YAW_UNITS), WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING, (DRYFIELD_NIGHT_WATER_HOLE_STAIR_SURFACE_CLASS << 4) | DRYFIELD_NIGHT_WATER_HOLE_STAIR_ASCENT_STEPS, ACTOR_TRANSFORM_ANGLE_TURN / (4 * DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_YAW_UNITS), WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_SHELTER_DOOR_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x34B0, -64, -2961, 0 }, { { 398, 0, -1136, 0 }, { 718, 0, -951, 0 }, { -718, 0, 952, 0 }, { -398, 0, 1137, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, ONE, 0 }, 1200, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_ARRIVAL1_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4100, -64, -5409, 0 }, { { -838, 0, -1072, 0 }, { 491, 0, -1269, 0 }, { -491, 0, 1270, 0 }, { 838, 0, 1073, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1360, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_WATER_HOLE_TRIGGER_ARRIVAL2_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 7376, -64, -2240, 0 }, { { -959, 0, 208, 0 }, { 960, 0, 208, 0 }, { -959, 0, 976, 0 }, { 960, 0, 976, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, ONE, 0 }, 1366, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY, DRYFIELD_NIGHT_WATER_HOLE_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two room-space line-of-sight blocking quads shared by all four room selectors.
///
/// Normals use Q12 and positions use game units. Setup mutates list links and
/// enables both entries, including the LAST-marked second; keep storage live until unlinked.
static WorldCollisionOccluder _gDryfieldNightWaterHoleOccluders[2] = {
    { NULL, NULL, { 8544, -1520, -3664, 0 }, { { -1536, -2224, -1840, 0 }, { -1536, 2224, -1840, 0 }, { 1536, -2224, 1840, 0 }, { 1536, 2224, 1840, 0 } }, { -3150, 0, 2629, 0 }, 3268, 1, 0 },
    { NULL, NULL, { 0x34E0, -1568, -176, 0 }, { { -1504, -2224, -1936, 0 }, { -1504, 2224, -1936, 0 }, { 1504, -2224, 1936, 0 }, { 1504, 2224, 1936, 0 } }, { -3237, 0, 2514, 0 }, 3308, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// The 6 authored point lights for rooms 1 and 3, with underpass switch 1 on.
///
/// Positions and inner/outer falloff radii use world-coordinate units; RGB
/// uses Q12 intensity. All logical views admit these lights. Runtime binding
/// parents and composes the mutable transforms; shading overwrites attenuation.
static WorldCoordPointLight _gDryfieldNightWaterHoleSwitchOnPointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9250, -1650, -600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3604, 2867 }, { 0, 0 } }, 1500, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x374A, -1650, -3400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3604, 2867 }, { 0, 0 } }, 1500, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x499C, -1650, -2600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3604, 2867 }, { 0, 0 } }, 1500, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x63C9, -4206, -2304 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 3000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -1290, -663 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1441, 1146 }, { 0, 0 } }, 1000, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2B64, -1277, -2296 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1441, 1146 }, { 0, 0 } }, 1000, 2700 },
};

// Full cone opening in 4096 angle units per turn.
enum { DRYFIELD_NIGHT_WATER_HOLE_CONE_OPENING = 113 };

/// The downward cone light for rooms 1 and 3, with underpass switch 1 on.
///
/// Position and falloff radii use world units; RGB and the axis use Q12.
/// Every logical view admits it. Runtime binding builds the cone orientation
/// and composes its mutable transform; shading overwrites attenuation.
static WorldCoordSpotLight _gDryfieldNightWaterHoleSwitchOnConeLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7394, -3968, -1496 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3604, 2867 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 3000, 6000, DRYFIELD_NIGHT_WATER_HOLE_CONE_OPENING },
};

/// Authored lighting while underpass switch 1 is on, shared by rooms 1 and 3.
///
/// Six point lights and one downward cone; no directional lights. The collection
/// borrows this overlay's mutable light arrays until the room is unloaded.
static WorldCoordRoomLights _gDryfieldNightWaterHoleSwitchOnRoomLights = {
    .pointLightCount = ARRAY_SIZE(_gDryfieldNightWaterHoleSwitchOnPointLights),
    .pointLights     = _gDryfieldNightWaterHoleSwitchOnPointLights,
    .coneLightCount  = ARRAY_SIZE(_gDryfieldNightWaterHoleSwitchOnConeLights),
    .coneLights      = _gDryfieldNightWaterHoleSwitchOnConeLights,
};

/// The 7 authored point lights for rooms 2 and 4, with underpass switch 1 off.
///
/// Positions and inner/outer falloff radii use world-coordinate units; RGB
/// uses Q12 intensity. All logical views admit these lights. Runtime binding
/// parents and composes the mutable transforms; shading overwrites attenuation.
static WorldCoordPointLight _gDryfieldNightWaterHoleSwitchOffPointLights[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8650, -1650, -1100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3A06, -1650, -3100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x48A2, -1650, -3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x5FE1, -4206, -2344 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2621, 1638 }, { 0, 0 } }, 0, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5500, -1650, -1003 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2A9C, -1277, -2296 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x5208, -1277, -2996 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
};

/// The downward cone light for rooms 2 and 4, with underpass switch 1 off.
///
/// Position and falloff radii use world units; RGB and the axis use Q12.
/// Every logical view admits it. Runtime binding builds the cone orientation
/// and composes its mutable transform; shading overwrites attenuation.
static WorldCoordSpotLight _gDryfieldNightWaterHoleSwitchOffConeLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7444, -3447, -1496 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3014, 2621 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 2500, 5000, DRYFIELD_NIGHT_WATER_HOLE_CONE_OPENING },
};

/// Authored lighting while underpass switch 1 is off, shared by rooms 2 and 4.
///
/// Seven point lights and one downward cone; no directional lights. The collection
/// borrows this overlay's mutable light arrays until the room is unloaded.
static WorldCoordRoomLights _gDryfieldNightWaterHoleSwitchOffRoomLights = {
    .pointLightCount = ARRAY_SIZE(_gDryfieldNightWaterHoleSwitchOffPointLights),
    .pointLights     = _gDryfieldNightWaterHoleSwitchOffPointLights,
    .coneLightCount  = ARRAY_SIZE(_gDryfieldNightWaterHoleSwitchOffConeLights),
    .coneLights      = _gDryfieldNightWaterHoleSwitchOffConeLights,
};

/// Variant 1's Zebra Stalker resource and terminal entry.
///
/// Placement zero selects the water-entrance spawn; file group base 40/file 6
/// supplies its package and task descriptor, borrowed during loading and spawning.
static AreaResource _gDryfieldNightWaterHoleVariant1Resources[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor400600ZebraStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's actor-101100 resource shared by its two placements, then a terminator.
///
/// File group base 10/file 11 supplies the package; descriptor zero is borrowed
/// during loading and spawning. Both placements match entry ID 11.
static AreaResource _gDryfieldNightWaterHoleVariant2Resources[2] = {
    { 11, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor101100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 10's Water Hole scene-controller resource and terminal entry.
///
/// File group base 10/file 460 is loaded although this variant's placement list
/// is empty. Room initialization starts the ending controller explicitly.
static AreaResource _gDryfieldNightWaterHoleVariant10Resources[2] = {
    { 101, 460, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor146000WaterHoleSceneTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 21's Zebra Stalker resource shared by three ordinary placements, then a terminator.
///
/// File group base 40/file 6 and descriptor zero serve every entry-ID-6 placement.
/// The table and descriptor are borrowed during loading and spawning.
static AreaResource _gDryfieldNightWaterHoleVariant21Resources[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor400600ZebraStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightWaterHoleAreaVariants[DRYFIELD_NIGHT_WATER_HOLE_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldFullWaterHoleVariant1Placements, .resources = _gDryfieldNightWaterHoleVariant1Resources },
    [2]                    = { .placements = gMapDryfieldFullWaterHoleVariant2Placements, .resources = _gDryfieldNightWaterHoleVariant2Resources },
    [10]                   = { .placements = gMapDryfieldFullWaterHoleVariant10Placements, .resources = _gDryfieldNightWaterHoleVariant10Resources },
    [21]                   = { .placements = gMapDryfieldFullWaterHoleVariant21Placements, .resources = _gDryfieldNightWaterHoleVariant21Resources },
};

/// Ambient minima by logical view for rooms 1 and 3, with switch 1 on.
///
/// Entry zero counts 11 following views; entries 1..11 contain RGB minima and
/// retained luma, in 16 units per 8-bit channel level. Views 6 and 7 have raised
/// minima. The logical view selects this table before camera/image remapping.
static WorldCoordRoomAmbientEntry _gDryfieldNightWaterHoleSwitchOnAmbientByView[12] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightWaterHoleSwitchOnAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 509, 508, 509, 508 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Ambient minima by logical view for rooms 2 and 4, with switch 1 off.
///
/// Entry zero counts 11 following views; entries 1..11 contain RGB minima and
/// retained luma, in 16 units per 8-bit channel level. View 3 has raised
/// minima. The logical view selects this table before camera/image remapping.
static WorldCoordRoomAmbientEntry _gDryfieldNightWaterHoleSwitchOffAmbientByView[12] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightWaterHoleSwitchOffAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 118, 117, 118, 117 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

enum {
    DRYFIELD_NIGHT_WATER_HOLE_CLASSES_1_AND_2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 37,
    DRYFIELD_NIGHT_WATER_HOLE_CLASSES_1_AND_2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 39,
    DRYFIELD_NIGHT_WATER_HOLE_CLASSES_1_AND_2_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 41,
    DRYFIELD_NIGHT_WATER_HOLE_CLASS_6_WALK_SOUND         = SOUND_SCRIPT_REQUEST_TYPE_1 | 61,
    DRYFIELD_NIGHT_WATER_HOLE_CLASS_6_RUN_SOUND          = SOUND_SCRIPT_REQUEST_TYPE_1 | 63,
};

/// Footstep bases for the default policies of surface classes 1 and 2.
///
/// Walk/run/scripted jump use entries 37/39/41 in the loaded type-1 bank.
/// Cue 1 adds one; companion cues add 100. Surface records borrow this storage
/// only while the overlay is loaded; the physical material is unproven.
static WorldCollisionFootstepSounds _gDryfieldNightWaterHoleClasses1And2Footsteps = {
    .walk         = DRYFIELD_NIGHT_WATER_HOLE_CLASSES_1_AND_2_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_WATER_HOLE_CLASSES_1_AND_2_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_WATER_HOLE_CLASSES_1_AND_2_JUMP_SOUND,
};

/// Footstep bases for class 6 and the Shelter-route-open replacements of classes 1/2/4.
///
/// Walk and scripted jump use entry 61, running entry 63 in the loaded type-1
/// bank. Cue 1 adds one and companion cues add 100. Records borrow this storage
/// only while the overlay is loaded; the physical material is unproven.
static WorldCollisionFootstepSounds _gDryfieldNightWaterHoleClass6Footsteps = {
    .walk         = DRYFIELD_NIGHT_WATER_HOLE_CLASS_6_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_WATER_HOLE_CLASS_6_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_WATER_HOLE_CLASS_6_WALK_SOUND,
};

/// Blocking surface policy without footstep cues, shared by classes 0, 4, 5 and 7.
///
/// Blocks probes, permits weapon impacts and applies pushback. The record is
/// mutable overlay-owned storage borrowed by the active surface-class table.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterHoleDefaultSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1 blocking policy with footstep cues, replaced when the Shelter route opens.
///
/// Blocks probes, permits weapon impacts and applies pushback. The record is
/// mutable overlay-owned storage borrowed by the active surface-class table.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterHoleClass1Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterHoleClasses1And2Footsteps,
};

/// Class 2 blocking policy with footstep cues, replaced when the Shelter route opens.
///
/// Blocks probes, permits weapon impacts and applies pushback. The record is
/// mutable overlay-owned storage borrowed by the active surface-class table.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterHoleClass2Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterHoleClasses1And2Footsteps,
};

/// Class 3 blocking policy without footstep cues.
///
/// Blocks probes, permits weapon impacts and applies pushback. The record is
/// mutable overlay-owned storage borrowed by the active surface-class table.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterHoleClass3Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 6 blocking policy with walk/run/jump footstep bases 61/63/61.
///
/// Blocks probes, permits weapon impacts and applies pushback. The record is
/// mutable overlay-owned storage borrowed by the active surface-class table.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterHoleClass6Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterHoleClass6Footsteps,
};

/// Shelter-route-open blocking policy for classes 1 and 4, with footstep bases 61/63/61.
///
/// Allows weapon impacts and pushback. The active class table borrows this
/// mutable record until replaced or unloaded; the physical material is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterHoleShelterRouteOpenClasses1And4Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterHoleClass6Footsteps,
};

/// Shelter-route-open blocking policy for class 2, with footstep bases 61/63/61.
///
/// Allows weapon impacts and pushback. The active class table borrows this
/// separate mutable record until replaced or unloaded; the physical material is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightWaterHoleShelterRouteOpenClass2Surface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightWaterHoleClass6Footsteps,
};

/// Surface replacements installed when the water-hole Shelter route has opened.
///
/// Classes 4 and 1 share a replacement; class 2 uses its separate record.
/// All enable impacts and pushback with footstep bases 61/63/61. The final
/// NULL properties pointer terminates the list. Applying it also refreshes
/// the live pushback cache; property records remain borrowed from this overlay.
static const _DryfieldNightWaterHoleSurfaceOverride _gDryfieldNightWaterHoleShelterRouteOpenSurfaceOverrides[4] SECTION(".data") = {
    { &_gDryfieldNightWaterHoleShelterRouteOpenClasses1And4Surface, 4 },
    { &_gDryfieldNightWaterHoleShelterRouteOpenClasses1And4Surface, 1 },
    { &_gDryfieldNightWaterHoleShelterRouteOpenClass2Surface, 2 },
    { NULL, 0 },
};

WorldCollisionSurfaceProperties* gDryfieldNightWaterHoleSurfacePropertiesByClass[8] = {
    &_gDryfieldNightWaterHoleDefaultSurface,
    &_gDryfieldNightWaterHoleClass1Surface,
    &_gDryfieldNightWaterHoleClass2Surface,
    &_gDryfieldNightWaterHoleClass3Surface,
    &_gDryfieldNightWaterHoleDefaultSurface,
    &_gDryfieldNightWaterHoleDefaultSurface,
    &_gDryfieldNightWaterHoleClass6Surface,
    &_gDryfieldNightWaterHoleDefaultSurface,
};

enum { DRYFIELD_NIGHT_WATER_HOLE_SCENE_ELEVATOR_VARIANT   = 1,
       DRYFIELD_NIGHT_WATER_HOLE_SCENE_SEWER_VARIANT      = 4,
       DRYFIELD_NIGHT_WATER_HOLE_SCENE_WATER_TANK_VARIANT = 10,
       DRYFIELD_NIGHT_WATER_HOLE_SCENE_MAP_MARK           = 1 };

const AreaApplyRec gDryfieldNightWaterHoleSceneAreaUpdates[4] SECTION(".data") = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, DRYFIELD_NIGHT_WATER_HOLE_SCENE_ELEVATOR_VARIANT, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_WATER_HOLE_SCENE_MAP_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, DRYFIELD_NIGHT_WATER_HOLE_SCENE_SEWER_VARIANT, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_WATER_HOLE_SCENE_MAP_MARK },
    { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, DRYFIELD_NIGHT_WATER_HOLE_SCENE_WATER_TANK_VARIANT, AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_WATER_HOLE_SCENE_MAP_MARK },
    { AREA_APPLY_END, 0, 0, 0 },
};

/// Byte cursor for interleaved POLY_G4 and DR_MODE packets in the current actor-arena half.
///
/// Reset on every draw to buffer 2 without a companion, otherwise buffer 1,
/// offset by the current ordering-table half (0 or 1) times 0xC000 bytes.
/// Packet casts select views of word-aligned storage; GPU use outlives this cursor.
static u8* _gWaterHolePrimCursor = NULL;

/// Signed-halfword running-actor tick counter controlling the water seam's phase.
///
/// Paused actors retain it. Each increment scrolls -16 angle units (4096 per
/// turn); narrowing retains the low 16 bits, so the sine repeats every 256 ticks.
static s16 _gWaterHoleWaveScroll = 0;

/// Retained, unreferenced halfword containing -12288; its role is unproven.
static s16 D_dryfield_night_water_hole_8018362E = -0x3000;

RoomDeparture gRoomDeparture;

static void _dryfieldNightWaterHoleInitializeRoom(Task* task);

#include "../../shared/room/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

#include "../../shared/room/room_event_departure_task.inc.c"

static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

/// Registers the room receiver and restores its progress-dependent scenery and actors.
///
/// State zero starts the water task while the Shelter route is closed, or
/// installs surface overrides after it opens. Arrival variant 1 cues placement
/// zero when its arrival event is already latched, selecting the command by
/// arrival ID. Ending variant 10 with a companion present latches its ending,
/// selects objective 37 and starts the ending controller once. Advances to idle
/// state 1; requires loaded room/actor tables and the live scene receiver.
static void _dryfieldNightWaterHoleInitializeRoom(Task* task)
{
    enum {
        DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_VARIANT   = 1,
        DRYFIELD_NIGHT_WATER_HOLE_ALTERNATE_ARRIVAL = 2,
        DRYFIELD_NIGHT_WATER_HOLE_ENDING_VARIANT    = 10,
        DRYFIELD_NIGHT_WATER_HOLE_ENDING_STARTED    = 2,
        DRYFIELD_NIGHT_WATER_HOLE_ENDING_OBJECTIVE  = 0x25,
        DRYFIELD_NIGHT_WATER_HOLE_ENDING_TASK_INDEX = 1,
    };

    task->msgTable = _gDryfieldNightWaterHoleRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) == 0) {
        taskSpawnFromTable(&_gDryfieldNightWaterHoleWaterTaskDesc, 0, 0, 0);
    } else {
        _dryfieldNightWaterHoleApplySurfaceOverrides(_gDryfieldNightWaterHoleShelterRouteOpenSurfaceOverrides);
    }
    if (gGameSession->location.variant == DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_VARIANT && sceneFindPlacedActor(0) != 0 && gameFlagGetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT) != 0) {
        if (gGameSession->location.warp == DRYFIELD_NIGHT_WATER_HOLE_ALTERNATE_ARRIVAL) {
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &_gDryfieldNightWaterHoleStalkerSecondFloorArrivalCommand, 0);
        } else {
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &_gDryfieldNightWaterHoleStalkerFirstFloorArrivalCommand, 0);
        }
    }
    if (gGameSession->location.variant == DRYFIELD_NIGHT_WATER_HOLE_ENDING_VARIANT && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0 && gameFlagGetNibble(GAME_FLAG_0CF) == 0) {
        gameFlagSetNibble(GAME_FLAG_0CF, DRYFIELD_NIGHT_WATER_HOLE_ENDING_STARTED);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_WATER_HOLE_ENDING_OBJECTIVE);
        taskSpawnFromTable(gActor146000WaterHoleSceneTasks, DRYFIELD_NIGHT_WATER_HOLE_ENDING_TASK_INDEX, 0, 0);
    }
    task->state = task->state + 1;
}

/// Initialization, message-wait and teardown handlers indexed by room-task state 0..2.
///
/// Dispatched from a complete 12-byte stack copy; callback order is fixed.
static const TaskFuncTable3 _gDryfieldNightWaterHoleRoomStateHandlers = {
    { _dryfieldNightWaterHoleInitializeRoom, _dryfieldNightWaterHoleRoomIdle, taskKill },
};

/// Refuses every key-item use in this room without consuming the item.
///
/// Message `ROOM_MESSAGE_USE_KEY_ITEM` carries the inventory
/// item ID in `itemId`; the receiver and second argument are unused. Returns
/// zero so the item menu displays its refusal notice.
static s32 _dryfieldNightWaterHoleRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/water_hole_door_msg.inc.c"

#include "../../shared/room/room_variants_resolve_departure.inc.c"

/// Handles the Shelter-door command by staging a departure or reporting it locked.
///
/// `ROOM_MESSAGE_COMMAND` command 2 resolves the open route's initialized area,
/// arrival and room selectors through the Shelter resolver, publishes the whole
/// departure and spawns its controller after holding player control. The facing
/// is 3072 units out of 4096 per turn. The singleton departure must remain intact
/// until consumed. A closed route plays CAP command 2, marks water on the map
/// and cues the locked-door sound. Other commands do nothing. Returns zero;
/// receiver, message ID and second payload are unused.
static s32 _dryfieldNightWaterHoleCommandMessage(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_WATER_HOLE_COMMAND_SHELTER_DOOR = 2,
        DRYFIELD_NIGHT_WATER_HOLE_SHELTER_ROOM         = 1,
        DRYFIELD_NIGHT_WATER_HOLE_SHELTER_ARRIVAL      = 3,
        DRYFIELD_NIGHT_WATER_HOLE_SHELTER_SOUND        = 0x53200007,
        DRYFIELD_NIGHT_WATER_HOLE_SHELTER_FACING       = 0xC00,
        DRYFIELD_NIGHT_WATER_HOLE_CAP_LOCKED_DOOR      = 2,
        DRYFIELD_NIGHT_WATER_HOLE_MAP_WATER_MARK       = 2,
    };

    RoomDeparture departure;

    if (commandId == DRYFIELD_NIGHT_WATER_HOLE_COMMAND_SHELTER_DOOR) {
        if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) != 0) {
            RoomVariantResolver resolve = _roomVariantResolveShelter;

            departure.stage     = GAME_STAGE_MINE_SHELTER;
            departure.area      = GAME_AREA_SHELTER_B4_WATER_SUPPLY;
            departure.roomState = DRYFIELD_NIGHT_WATER_HOLE_SHELTER_ROOM;
            departure.warp      = DRYFIELD_NIGHT_WATER_HOLE_SHELTER_ARRIVAL;
            departure.sndEvent  = DRYFIELD_NIGHT_WATER_HOLE_SHELTER_SOUND;
            departure.facing    = DRYFIELD_NIGHT_WATER_HOLE_SHELTER_FACING;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            // Resolve only initialized destination selectors; the request aliases its reply.
            _roomVariantResolveDeparture(&departure, resolve);
            gRoomDeparture = departure;
            taskSpawnFromTable(&_gDryfieldNightWaterHoleShelterDepartureTaskDesc, 0, 0, 0);
        } else {
            capRunCommandWithTransition(DRYFIELD_NIGHT_WATER_HOLE_CAP_LOCKED_DOOR);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_WATER, DRYFIELD_NIGHT_WATER_HOLE_MAP_WATER_MARK);
            sndEvtRequestScriptStart(SOUND_NIGHT_WATER_HOLE_LOCKED, 0, 0);
        }
    }
    return 0;
}

/// Starts one of two arrival scripts once on the water hole's arrival variant.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request through the call.
/// In variant 1, action 2 or action 1 selects its arrival script if the arrival
/// event is clear, latching the event before starting the script. No pointer is
/// retained. Returns zero. Receiver,
/// message ID and the zero second payload are unused.
static s32 _dryfieldNightWaterHoleRoomActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_WATER_HOLE_ACTION_ARRIVAL_1 = 1,
        DRYFIELD_NIGHT_WATER_HOLE_ACTION_ARRIVAL_2 = 2,
        DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_VARIANT  = 1,
    };

    u8 actionId;

    if ((request->actionId == DRYFIELD_NIGHT_WATER_HOLE_ACTION_ARRIVAL_2) && (gameFlagGetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT) == 0) && (gGameSession->location.variant == DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_VARIANT)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT, 1);
        evsStartScript(_gDryfieldNightWaterHoleArrival2Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    actionId = request->actionId;
    if ((actionId == DRYFIELD_NIGHT_WATER_HOLE_ACTION_ARRIVAL_1) && (gameFlagGetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT) == 0) && (gGameSession->location.variant == actionId)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT, 1);
        evsStartScript(_gDryfieldNightWaterHoleArrival1Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Keeps the initialized room task idle until its state changes externally.
static void _dryfieldNightWaterHoleRoomIdle(Task* task)
{
    // Retain the idle state's unused 16-byte stack frame.
    u8 unusedStackBytes[16];
}

void dryfieldNightWaterHoleRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldNightWaterHoleRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Installs surface-class replacements and refreshes their live pushback policy.
///
/// Borrows a list ended by a NULL properties pointer; each preceding class
/// must be 0..7. Requires the current stage/area's writable property table.
/// Replacement records remain borrowed until that room table is replaced or
/// unloaded. The cache widens each record's suppression byte to s32: zero
/// enables pushback, nonzero suppresses it. Does not change grid geometry.
static void _dryfieldNightWaterHoleApplySurfaceOverrides(const _DryfieldNightWaterHoleSurfaceOverride* overrides)
{
    GameLocationKey*                  location;
    s32                               overrideIndex;
    WorldCollisionSurfaceProperties** surfaceProperties;

    location = &gGameSession->location;
    for (overrideIndex = 0; overrides[overrideIndex].properties != NULL; overrideIndex++) {
        surfaceProperties                                                                = gWorldCollisionSurfacePropertiesTables[location->stage - 1][location->area - 1];
        surfaceProperties[overrides[overrideIndex].surfaceClass]                         = overrides[overrideIndex].properties;
        gWorldCollisionSurfacePushbackSuppression[overrides[overrideIndex].surfaceClass] = surfaceProperties[overrides[overrideIndex].surfaceClass]->suppressPushback;
    }
}

#include "../../shared/water_hole_draw_surfaces.inc.c"

#include "../../shared/water_hole_water_task.inc.c"

#include "../../shared/water_hole_water_start.inc.c"

void dryfieldNightWaterHoleSplashAndLightShaftsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_WATER_HOLE_SPLASH_INITIALIZE   = 0,
        DRYFIELD_NIGHT_WATER_HOLE_SPLASH_ACTIVE       = 1,
        DRYFIELD_NIGHT_WATER_HOLE_SPLASH_SAMPLE_COUNT = ARRAY_SIZE(_gDryfieldNightWaterHolePreviousSplashPartPositions),
        DRYFIELD_NIGHT_WATER_HOLE_SHAFT_GROUP_1_VIEWS = (1 << 3) | (1 << 4),
        DRYFIELD_NIGHT_WATER_HOLE_SHAFT_GROUP_2_VIEWS = (1 << 4) | (1 << 6) | (1 << 9) | (1 << 11),
        DRYFIELD_NIGHT_WATER_HOLE_SHAFT_GROUP_3_VIEWS = 1 << 7,
        DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE  = 0x100,
    };

    Task*       playerTask;
    s32         viewMask;
    EffectWork* work;
    GfxCoord*   playerRoot;
    GfxCoord*   trackedPart;
    GfxCoord*   surfaceParent;
    GfxCoord    surfaceCoord;
    s32         sampleIndex;
    u32         randomRoll;

/// Emits ripple then spray for one part and advances its signed-halfword history.
///
/// Captures playerTask, work, trackedPart, surfaceParent, surfaceCoord and
/// randomRoll from this function. sampleIndex must be a side-effect-free index
/// 0 or 1 and is evaluated repeatedly. The caller supplies a live view parent
/// and a model containing coordinates 14/17. Consumes two ordered LCG draws,
/// including on failed spawns, and leaves the resulting odds in work->angle.
/// Expands to a braced statement block; the child uses its copied coordinate.
#define DRYFIELD_NIGHT_WATER_HOLE_EMIT_PART_SPLASHES(sampleIndex)                                                                                                        \
    {                                                                                                                                                                    \
        trackedPart = &playerTask->extra.tmd->coords[DRYFIELD_NIGHT_WATER_HOLE_SPLASH_FIRST_PART + (sampleIndex) * DRYFIELD_NIGHT_WATER_HOLE_SPLASH_PART_STRIDE];        \
        actorRenderComposeCoord(trackedPart);                                                                                                                            \
        /* Movement sets signed-halfword odds; ripple alone adds the bias. */                                                                                            \
        work->angle = ABS(_gDryfieldNightWaterHolePreviousSplashPartPositions[(sampleIndex)].vx - trackedPart->workm.t[0]) +                                             \
                      ABS(_gDryfieldNightWaterHolePreviousSplashPartPositions[(sampleIndex)].vy - trackedPart->workm.t[1]) +                                             \
                      ABS(_gDryfieldNightWaterHolePreviousSplashPartPositions[(sampleIndex)].vz - trackedPart->workm.t[2]) + DRYFIELD_NIGHT_WATER_HOLE_RIPPLE_ODDS_BIAS; \
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &trackedPart->workm, &surfaceCoord.coord);                                                                        \
        surfaceCoord.parent       = surfaceParent;                                                                                                                       \
        surfaceCoord.coord.t[1]   = gGameSession->waterY;                                                                                                                \
        surfaceCoord.composeStamp = GRAPHICS_COORD_DIRTY;                                                                                                                \
        actorRenderComposeCoord(&surfaceCoord);                                                                                                                          \
        randomRoll = (RANDOM_LCG_ADVANCE());                                                                                                                             \
        if ((s32)((randomRoll >> 16) & DRYFIELD_NIGHT_WATER_HOLE_SPLASH_ROLL_MASK) < work->angle) {                                                                      \
            effectSpawn(gRoomEffectWaterRippleId, &surfaceCoord, DRYFIELD_NIGHT_WATER_HOLE_RIPPLE_HALF_SIDE, 0);                                                         \
        }                                                                                                                                                                \
        work->angle -= DRYFIELD_NIGHT_WATER_HOLE_RIPPLE_ODDS_BIAS;                                                                                                       \
        randomRoll   = (RANDOM_LCG_ADVANCE());                                                                                                                           \
        if ((s32)((randomRoll >> 16) & DRYFIELD_NIGHT_WATER_HOLE_SPLASH_ROLL_MASK) < work->angle) {                                                                      \
            effectSpawn(gRoomEffectWaterSprayId, &surfaceCoord, DRYFIELD_NIGHT_WATER_HOLE_SPRAY_SPAWN_ARG, 0);                                                           \
        }                                                                                                                                                                \
        _gDryfieldNightWaterHolePreviousSplashPartPositions[(sampleIndex)].vx = trackedPart->workm.t[0];                                                                 \
        _gDryfieldNightWaterHolePreviousSplashPartPositions[(sampleIndex)].vy = trackedPart->workm.t[1];                                                                 \
        _gDryfieldNightWaterHolePreviousSplashPartPositions[(sampleIndex)].vz = trackedPart->workm.t[2];                                                                 \
    }

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work       = task->spawnArg2.pointer;
    viewMask   = 1 << gGameSession->location.view;
    playerRoot = playerTask->extra.tmd->coords;
    switch (task->state) {
        case DRYFIELD_NIGHT_WATER_HOLE_SPLASH_INITIALIZE:
            // Start history from cached part positions, without an initial movement impulse.
            if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) == 0) {
                gRoomEffectWaterRippleId = EFFECT_DRYFIELD_NIGHT_WATER_HOLE_WATER_RIPPLE;
                gRoomEffectWaterSprayId  = EFFECT_DRYFIELD_NIGHT_WATER_HOLE_WATER_SPRAY;
            }
            task->state = DRYFIELD_NIGHT_WATER_HOLE_SPLASH_ACTIVE;
            for (sampleIndex = 0; sampleIndex < DRYFIELD_NIGHT_WATER_HOLE_SPLASH_SAMPLE_COUNT; sampleIndex++) {
                trackedPart                                                         = &playerTask->extra.tmd->coords[DRYFIELD_NIGHT_WATER_HOLE_SPLASH_FIRST_PART + sampleIndex * DRYFIELD_NIGHT_WATER_HOLE_SPLASH_PART_STRIDE];
                _gDryfieldNightWaterHolePreviousSplashPartPositions[sampleIndex].vx = trackedPart->workm.t[0];
                _gDryfieldNightWaterHolePreviousSplashPartPositions[sampleIndex].vy = trackedPart->workm.t[1];
                _gDryfieldNightWaterHolePreviousSplashPartPositions[sampleIndex].vz = trackedPart->workm.t[2];
            }
            break;
        case DRYFIELD_NIGHT_WATER_HOLE_SPLASH_ACTIVE:
            // Paused or dry frames keep history frozen until wading resumes.
            if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING &&
                gGameSession->waterY < playerRoot->coord.t[1]) {
                surfaceParent = &gGfxViewCoord;
                for (sampleIndex = 0; sampleIndex < DRYFIELD_NIGHT_WATER_HOLE_SPLASH_SAMPLE_COUNT; sampleIndex++) {
                    DRYFIELD_NIGHT_WATER_HOLE_EMIT_PART_SPLASHES(sampleIndex);
                }
            }
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 1) {
                if (viewMask & DRYFIELD_NIGHT_WATER_HOLE_SHAFT_GROUP_1_VIEWS) {
                    _glowDrawShaft(_gDryfieldNightWaterHoleShaftGroup1Endpoints[0], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                    _glowDrawShaft(_gDryfieldNightWaterHoleShaftGroup1Endpoints[1], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                }
                if (viewMask & DRYFIELD_NIGHT_WATER_HOLE_SHAFT_GROUP_2_VIEWS) {
                    _glowDrawShaft(_gDryfieldNightWaterHoleShaftGroup2Endpoints[0], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                    _glowDrawShaft(_gDryfieldNightWaterHoleShaftGroup2Endpoints[1], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                }
                if (viewMask & DRYFIELD_NIGHT_WATER_HOLE_SHAFT_GROUP_3_VIEWS) {
                    _glowDrawShaft(_gDryfieldNightWaterHoleShaftGroup3Endpoints[0], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                    _glowDrawShaft(_gDryfieldNightWaterHoleShaftGroup3Endpoints[1], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                }
            }
            break;
    }
}

#undef DRYFIELD_NIGHT_WATER_HOLE_EMIT_PART_SPLASHES

#include "../../shared/effects/glow_draw_shaft.inc.c"

#include "../../shared/effects/water_ripple_task.inc.c"

void dryfieldNightWaterHoleWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task_u16.inc.c"

void dryfieldNightWaterHoleWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/effects/water_spin_u16.inc.c"
