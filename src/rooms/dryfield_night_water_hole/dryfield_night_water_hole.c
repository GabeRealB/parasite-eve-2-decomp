#include "rooms/dryfield_night_water_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

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
#include "../../shared/glow_draw.h"

/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"
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

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.

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

/// Descriptor the room's event task is spawned from, index 0 of the table
/// `_dryfieldNightWaterHoleCommandMessage` hands `taskSpawnFromTable`. Its
/// callback is that same task, `roomEventDepartureTask`.
extern TaskDesc D_dryfield_night_water_hole_801805EC;
/// The room's message table, the `TaskMessageEntry` list the room task publishes in
/// `Task::msgTable` for `taskMessageDispatch` to walk: 0x13EE, 0x13F1, 0x13EF and
/// 0x13F0.
extern TaskMessageEntry D_dryfield_night_water_hole_801805F8[];
/// The two four-byte records this room hands the slot-4 task as the message
/// 0x7DB payload, picked by `gGameSession::location.loc.warp`. They are the last two of
/// the four-record run at 0x80180654, which differ only in the halfword at 0x2.
extern s32 D_dryfield_night_water_hole_8018065C;
extern s32 D_dryfield_night_water_hole_80180660;
/// The records message 0x13EF passes to `evsStartScript` on the first visit
/// through sub-id 1, for `field_2` 2 and 1 respectively.
extern EvsCommand D_dryfield_night_water_hole_8018067C[];
extern EvsCommand D_dryfield_night_water_hole_801807FC[];
/// Descriptor of the room's water task, spawned while progress nibble 0xB8 is
/// still clear. Its callback is `_waterHoleWaterTask`.
extern TaskDesc D_dryfield_night_water_hole_80180964[];
/// Point pairs of the glowing beams the splash task draws, one table per group
/// of views.
extern SVECTOR D_dryfield_night_water_hole_80180994[];
extern SVECTOR D_dryfield_night_water_hole_801809B4[];
extern SVECTOR D_dryfield_night_water_hole_801809D4[];
/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_dryfield_night_water_hole_801809F4[];
/// Override list applied once nibble 0xB8 is set.
extern _DryfieldNightWaterHoleSurfaceOverride D_dryfield_night_water_hole_801835D8[];
/// Cursor into the primitive area the room's water surface is written to,
/// reset each frame to the half of that area belonging to the ordering table
/// being built.
/// Frame counter the water surface's wave is phased by.
/// The staged event descriptor, read by the room's event task.
extern RoomDeparture gRoomDeparture;

static void _dryfieldNightWaterHoleRoomIdle(Task* task);
static void _dryfieldNightWaterHoleApplySurfaceOverrides(const _DryfieldNightWaterHoleSurfaceOverride* overrides);

extern WorldCollisionGrid         D_dryfield_night_water_hole_80180F50[1];
extern WorldCollisionOccluder     D_dryfield_night_water_hole_80182D58[2];
extern WorldCollisionTrigger      D_dryfield_night_water_hole_801824BC[12];
extern WorldCollisionTrigger      D_dryfield_night_water_hole_8018284C[9];
extern WorldCollisionTrigger      D_dryfield_night_water_hole_80182AF8[8];
static WorldCoordRoomAmbientEntry _gDryfieldNightWaterHoleSwitchOnAmbientByView[12];
static WorldCoordRoomAmbientEntry _gDryfieldNightWaterHoleSwitchOffAmbientByView[12];
static WorldCoordRoomLights       _gDryfieldNightWaterHoleSwitchOnRoomLights;
static WorldCoordRoomLights       _gDryfieldNightWaterHoleSwitchOffRoomLights;

extern AnimationSet* D_dryfield_night_water_hole_80180620[1];

static s32 _dryfieldNightWaterHoleRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightWaterHoleCommandMessage(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldNightWaterHoleRoomActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

enum {
    DRYFIELD_NIGHT_WATER_HOLE_MESSAGE_USE_KEY_ITEM = 0x13F1,
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

TaskDesc D_dryfield_night_water_hole_801805EC = { { { TASK_BODY_NONE, 32 } }, roomEventDepartureTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_water_hole_801805F8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveWaterHole },
    { DRYFIELD_NIGHT_WATER_HOLE_MESSAGE_USE_KEY_ITEM, _dryfieldNightWaterHoleRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightWaterHoleRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightWaterHoleCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_dryfield_night_water_hole_80180620[1] = {
    &_gDryfieldNightWaterHoleAnimation03004,
};

AnimationBankCopyRequest D_dryfield_night_water_hole_80180624 = { { .sets = D_dryfield_night_water_hole_80180620 }, ARRAY_SIZE(D_dryfield_night_water_hole_80180620) };

AnimationPlayRequest D_dryfield_night_water_hole_8018062C = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_water_hole_80180640 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_dryfield_night_water_hole_80180654 = { { .loc = { 3, 32 } }, 0 };

ActorCommand D_dryfield_night_water_hole_80180658 = { { .loc = { 3, 32 } }, 1 };

s32 D_dryfield_night_water_hole_8018065C = 0x22003;

s32 D_dryfield_night_water_hole_80180660 = 0x32003;

ActorTransform D_dryfield_night_water_hole_80180664 = { { 0, 0, 0, 0 }, { 0, 1024, 0, 0 } };

EvsCommand D_dryfield_night_water_hole_8018067C[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_water_hole_80180624 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_hole_80180640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_hole_8018062C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_night_water_hole_80180658 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_water_hole_801807FC[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_water_hole_80180624 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_hole_80180640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_hole_8018062C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_night_water_hole_80180654 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_dryfield_night_water_hole_80180664 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_night_water_hole_80180964[1] = {
    { { { TASK_BODY_NONE, 192 } }, _waterHoleWaterTask, { .value = 0 } },
};

WaterHoleSurface gWaterHoleSurfaces[3] = {
    { 4000, -2000, 8000, 2000, WATER_HOLE_SURFACE_HEIGHT },
    { 10000, -4000, 13000, 2000, WATER_HOLE_SURFACE_HEIGHT },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

SVECTOR D_dryfield_night_water_hole_80180994[4] = {
    { 9500, -1835, -100, 0 },
    { 0x2904, -1835, -100, 0 },
    { 9500, -1750, -75, 0 },
    { 0x2904, -1750, -75, 0 },
};

SVECTOR D_dryfield_night_water_hole_801809B4[4] = {
    { 0x37DC, -1835, -3900, 0 },
    { 0x3BC4, -1835, -3900, 0 },
    { 0x37DC, -1750, -3925, 0 },
    { 0x3BC4, -1750, -3925, 0 },
};

SVECTOR D_dryfield_night_water_hole_801809D4[4] = {
    { 0x477C, -1835, -2120, 0 },
    { 0x4B64, -1835, -2120, 0 },
    { 0x477C, -1750, -2095, 0 },
    { 0x4B64, -1750, -2095, 0 },
};

SVECTOR D_dryfield_night_water_hole_801809F4[2] = { 0 };

WorldCollisionRoomResources D_dryfield_night_water_hole_80180A04[4] = {
    { D_dryfield_night_water_hole_80180F50, D_dryfield_night_water_hole_801824BC, D_dryfield_night_water_hole_8018284C, D_dryfield_night_water_hole_80182D58 },
    { D_dryfield_night_water_hole_80180F50, D_dryfield_night_water_hole_801824BC, D_dryfield_night_water_hole_80182AF8, D_dryfield_night_water_hole_80182D58 },
    { D_dryfield_night_water_hole_80180F50, D_dryfield_night_water_hole_801824BC, D_dryfield_night_water_hole_8018284C, D_dryfield_night_water_hole_80182D58 },
    { D_dryfield_night_water_hole_80180F50, D_dryfield_night_water_hole_801824BC, D_dryfield_night_water_hole_80182AF8, D_dryfield_night_water_hole_80182D58 },
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

ViewCount D_dryfield_night_water_hole_80180AA4[4] = { 11, 11, 11, 11 };

DirectionWarpEntry D_dryfield_night_water_hole_80180AAC[3] = {
    { { { .word = 0 }, 7400, 0, -1300 }, { 0, 0, 0, 0 }, { { .word = 0 }, 7400, 0, -1300 }, { 0, 0, 0, 0 }, 0x53200003, 0x53200003, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x57C0, -534, -3076 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x57C0, -534, -3076 }, { 0, 0, 0, 0 }, 0x53200002, 0x53200001, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, 462 },
    { { { .word = 1024 }, 4680, 0, -954 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 4680, 0, -954 }, { 0, 0, 0, 0 }, 0x53200008, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_WATER },
};

static SVECTOR _gDryfieldNightWaterHoleCollision03990Normals[10] = {
#include "assets/dryfield_night_water_hole_collision_03990_normals.inc"
};

static SVECTOR _gDryfieldNightWaterHoleCollision03990Verts[54] = {
#include "assets/dryfield_night_water_hole_collision_03990_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightWaterHoleCollision03990Faces[23] = {
#include "assets/dryfield_night_water_hole_collision_03990_faces.inc"
};

static s16 _gDryfieldNightWaterHoleCollision03990Cells[92] = {
#include "assets/dryfield_night_water_hole_collision_03990_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightWaterHoleCollision03990Cells[i])
static s16* _gDryfieldNightWaterHoleCollision03990Table[12] = {
#include "assets/dryfield_night_water_hole_collision_03990_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_water_hole_80180F50[1] = {
    { NULL, _gDryfieldNightWaterHoleCollision03990Normals, _gDryfieldNightWaterHoleCollision03990Verts, _gDryfieldNightWaterHoleCollision03990Faces, _gDryfieldNightWaterHoleCollision03990Table, -4000, 5000, 6, 2, 4000, 23 },
};

ViewCamera D_dryfield_night_water_hole_80180F74[26] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x34B7, 0x78AD, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x34B7, 0x8C35, 3935 } }, 329 },
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

SpriteBatch D_dryfield_night_water_hole_80181AE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181AF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181B00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181B10[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181B20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80181B30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80181B40[21] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_80181CE4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 2, 0 } },
    { 8, 8, 0, 0, { 0, 0 } },
    { 16, 5, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80181D0C[33] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_80181FA0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80181FC0[18] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_80182128[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_water_hole_80182140[19] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_water_hole_801822BC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 9, 0, 0, { 2, 0 } },
    { 17, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_801822E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_801822F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182304[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182314[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182324[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182334[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182344[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182354[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182364[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_water_hole_80182374[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_water_hole_80182384[26] = {
    { { .empty = _gDryfieldNightWaterHoleView1Batches }, _gDryfieldNightWaterHoleView1Batches, NULL },
    { { .empty = _gDryfieldNightWaterHoleView2Batches }, _gDryfieldNightWaterHoleView2Batches, NULL },
    { { .elements = _gDryfieldNightWaterHoleView3Sources }, _gDryfieldNightWaterHoleView3Batches, NULL },
    { { .elements = _gDryfieldNightWaterHoleView4Sources }, _gDryfieldNightWaterHoleView4Batches, NULL },
    { { .elements = _gDryfieldNightWaterHoleView5Sources }, _gDryfieldNightWaterHoleView5Batches, NULL },
    { { .elements = _gDryfieldNightWaterHoleView6Sources }, _gDryfieldNightWaterHoleView6Batches, NULL },
    { { .empty = D_dryfield_night_water_hole_80181AE0 }, D_dryfield_night_water_hole_80181AE0, NULL },
    { { .empty = D_dryfield_night_water_hole_80181AF0 }, D_dryfield_night_water_hole_80181AF0, NULL },
    { { .empty = D_dryfield_night_water_hole_80181B00 }, D_dryfield_night_water_hole_80181B00, NULL },
    { { .empty = D_dryfield_night_water_hole_80181B10 }, D_dryfield_night_water_hole_80181B10, NULL },
    { { .empty = D_dryfield_night_water_hole_80181B20 }, D_dryfield_night_water_hole_80181B20, NULL },
    { { .empty = D_dryfield_night_water_hole_80181B30 }, D_dryfield_night_water_hole_80181B30, NULL },
    { { .elements = D_dryfield_night_water_hole_80181B40 }, D_dryfield_night_water_hole_80181CE4, NULL },
    { { .elements = D_dryfield_night_water_hole_80181D0C }, D_dryfield_night_water_hole_80181FA0, NULL },
    { { .elements = D_dryfield_night_water_hole_80181FC0 }, D_dryfield_night_water_hole_80182128, NULL },
    { { .elements = D_dryfield_night_water_hole_80182140 }, D_dryfield_night_water_hole_801822BC, NULL },
    { { .empty = D_dryfield_night_water_hole_801822E4 }, D_dryfield_night_water_hole_801822E4, NULL },
    { { .empty = D_dryfield_night_water_hole_801822F4 }, D_dryfield_night_water_hole_801822F4, NULL },
    { { .empty = D_dryfield_night_water_hole_80182304 }, D_dryfield_night_water_hole_80182304, NULL },
    { { .empty = D_dryfield_night_water_hole_80182314 }, D_dryfield_night_water_hole_80182314, NULL },
    { { .empty = D_dryfield_night_water_hole_80182324 }, D_dryfield_night_water_hole_80182324, NULL },
    { { .empty = D_dryfield_night_water_hole_80182334 }, D_dryfield_night_water_hole_80182334, NULL },
    { { .empty = D_dryfield_night_water_hole_80182344 }, D_dryfield_night_water_hole_80182344, NULL },
    { { .empty = D_dryfield_night_water_hole_80182354 }, D_dryfield_night_water_hole_80182354, NULL },
    { { .empty = D_dryfield_night_water_hole_80182364 }, D_dryfield_night_water_hole_80182364, NULL },
    { { .empty = D_dryfield_night_water_hole_80182374 }, D_dryfield_night_water_hole_80182374, NULL },
};

WorldCollisionTrigger D_dryfield_night_water_hole_801824BC[12] = {
    { NULL, NULL, NULL, { 6101, -1152, -1034, 0 }, { { 199, -2176, -1004, 0 }, { -200, -2176, 1004, 0 }, { 199, 2176, -1004, 0 }, { -200, 2176, 1004, 0 } }, { 4021, 0, 797, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6304, -1104, -1024, 0 }, { { -154, -2128, 1004, 0 }, { 144, -2128, -1022, 0 }, { -154, 2128, 1004, 0 }, { 144, 2128, -1022, 0 } }, { -4056, 0, -597, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9499, -1248, -1060, 0 }, { { -345, -2272, -963, 0 }, { 345, -2272, 963, 0 }, { -345, 2272, -963, 0 }, { 345, 2272, 963, 0 } }, { 3869, 0, -1388, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9632, -1200, -1026, 0 }, { { 340, -2224, 960, 0 }, { -350, -2224, -966, 0 }, { 340, 2224, 960, 0 }, { -350, 2224, -966, 0 } }, { -3865, 0, 1383, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A9F, -1088, -1922, 0 }, { { 919, -2112, 432, 0 }, { -929, -2112, -443, 0 }, { 919, 2112, 432, 0 }, { -929, 2112, -443, 0 } }, { -1765, 0, 3723, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A7E, -1168, -1858, 0 }, { { -927, -2192, -439, 0 }, { 923, -2192, 434, 0 }, { -927, 2192, -439, 0 }, { 923, 2192, 434, 0 } }, { 1748, 0, -3710, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x319F, -1248, -2945, 0 }, { { 435, -2272, -931, 0 }, { -441, -2272, 919, 0 }, { 435, 2272, -931, 0 }, { -441, 2272, 919, 0 } }, { 3717, 0, 1758, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x323F, -1136, -2976, 0 }, { { -441, -2160, 919, 0 }, { 435, -2160, -931, 0 }, { -441, 2160, 919, 0 }, { 435, 2160, -931, 0 } }, { -3706, 0, -1755, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x415F, -1200, -2912, 0 }, { { 198, -2224, 1001, 0 }, { -202, -2224, -1006, 0 }, { 198, 2224, 1001, 0 }, { -202, 2224, -1006, 0 } }, { -4027, 0, 801, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40FF, -1328, -2977, 0 }, { { -205, -2352, -1008, 0 }, { 195, -2352, 999, 0 }, { -205, 2352, -1008, 0 }, { 195, 2352, 999, 0 } }, { 4021, 0, -803, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x531F, -1376, -2978, 0 }, { { 0, -2400, 1024, 0 }, { 0, -2400, -1023, 0 }, { 0, 2400, 1024, 0 }, { 0, 2400, -1023, 0 } }, { -4116, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x523F, -1408, -2978, 0 }, { { 51, -2432, -1022, 0 }, { -50, -2432, 1022, 0 }, { 51, 2432, -1022, 0 }, { -50, 2432, 1022, 0 } }, { 4093, 0, 200, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_water_hole_8018284C[9] = {
    { NULL, NULL, NULL, { 7376, -48, -1616, 0 }, { { -719, 0, -336, 0 }, { 720, 0, -336, 0 }, { -719, 0, 336, 0 }, { 720, 0, 336, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x58B0, -1280, -2992, 0 }, { { 0, 1344, -1199, 0 }, { 0, 1344, 1200, 0 }, { 0, -1344, -1199, 0 }, { 0, -1344, 1200, 0 } }, { -4101, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1801, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 38, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5860, -403, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 18, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING, 21, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x34B0, -64, -2961, 0 }, { { 398, 0, -1136, 0 }, { 718, 0, -951, 0 }, { -718, 0, 952, 0 }, { -398, 0, 1137, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1200, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4100, -64, -5409, 0 }, { { -838, 0, -1072, 0 }, { 491, 0, -1269, 0 }, { -491, 0, 1270, 0 }, { 838, 0, 1073, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1360, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 7376, -64, -2176, 0 }, { { -959, 0, 176, 0 }, { 960, 0, 176, 0 }, { -959, 0, 848, 0 }, { 960, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40A0, -64, -4416, 0 }, { { -1007, 0, 464, 0 }, { 944, 0, 464, 0 }, { -1007, 0, 1392, 0 }, { 944, 0, 1392, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_water_hole_80182AF8[8] = {
    { NULL, NULL, NULL, { 7376, -48, -1616, 0 }, { { -719, 0, -336, 0 }, { 720, 0, -336, 0 }, { -719, 0, 336, 0 }, { 720, 0, 336, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x58B0, -1280, -2992, 0 }, { { 0, 1344, -1199, 0 }, { 0, 1344, 1200, 0 }, { 0, -1344, -1199, 0 }, { 0, -1344, 1200, 0 } }, { -4101, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1801, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 38, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5860, -403, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 18, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING, 21, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x34B0, -64, -2961, 0 }, { { 398, 0, -1136, 0 }, { 718, 0, -951, 0 }, { -718, 0, 952, 0 }, { -398, 0, 1137, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1200, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4100, -64, -5409, 0 }, { { -838, 0, -1072, 0 }, { 491, 0, -1269, 0 }, { -491, 0, 1270, 0 }, { 838, 0, 1073, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1360, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 7376, -64, -2240, 0 }, { { -959, 0, 208, 0 }, { 960, 0, 208, 0 }, { -959, 0, 976, 0 }, { 960, 0, 976, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 1366, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_night_water_hole_80182D58[2] = {
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

AreaResource D_dryfield_night_water_hole_801833B8[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor400600ZebraStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_water_hole_801833D0[2] = {
    { 11, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor101100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_water_hole_801833E8[2] = {
    { 101, 460, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor146000WaterHoleSceneTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_water_hole_80183400[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor400600ZebraStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_water_hole_80183418[22] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017CCE8, D_dryfield_night_water_hole_801833B8 },
    { D_map_dryfield_full_8017CD08, D_dryfield_night_water_hole_801833D0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017CD38, D_dryfield_night_water_hole_801833E8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017CD48, D_dryfield_night_water_hole_80183400 },
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

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835C8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gDryfieldNightWaterHoleClass6Footsteps },
};

WorldCollisionSurfaceProperties D_dryfield_night_water_hole_801835D0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gDryfieldNightWaterHoleClass6Footsteps },
};

_DryfieldNightWaterHoleSurfaceOverride D_dryfield_night_water_hole_801835D8[4] = {
    { D_dryfield_night_water_hole_801835C8, 4 },
    { D_dryfield_night_water_hole_801835C8, 1 },
    { D_dryfield_night_water_hole_801835D0, 2 },
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

AreaApplyRec D_dryfield_night_water_hole_80183618[4] = {
    { 4, 42, 1, 1 },
    { 4, 44, 4, 1 },
    { 3, 21, 10, 1 },
    { 255, 0, 0, 0 },
};

u8* gWaterHolePrimCursor = NULL;

s16 gWaterHoleWaveScroll = 0;

/// Not read by this room's code; what it belongs to is unknown.
s16 D_dryfield_night_water_hole_8018362E = -0x3000;

RoomDeparture gRoomDeparture;

static void _dryfieldNightWaterHoleInitializeRoom(Task* task);

#include "../../shared/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

#include "../../shared/room_event_departure_task.inc.c"

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

    task->msgTable = D_dryfield_night_water_hole_801805F8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) == 0) {
        taskSpawnFromTable(D_dryfield_night_water_hole_80180964, 0, 0, 0);
    } else {
        _dryfieldNightWaterHoleApplySurfaceOverrides(D_dryfield_night_water_hole_801835D8);
    }
    if (gGameSession->location.loc.variant == DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_VARIANT && sceneFindPlacedActor(0) != 0 && gameFlagGetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT) != 0) {
        if (gGameSession->location.loc.warp == DRYFIELD_NIGHT_WATER_HOLE_ALTERNATE_ARRIVAL) {
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_water_hole_80180660, 0);
        } else {
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_water_hole_8018065C, 0);
        }
    }
    if (gGameSession->location.loc.variant == DRYFIELD_NIGHT_WATER_HOLE_ENDING_VARIANT && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0 && gameFlagGetNibble(GAME_FLAG_0CF) == 0) {
        gameFlagSetNibble(GAME_FLAG_0CF, DRYFIELD_NIGHT_WATER_HOLE_ENDING_STARTED);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_WATER_HOLE_ENDING_OBJECTIVE);
        taskSpawnFromTable(gActor146000WaterHoleSceneTasks, DRYFIELD_NIGHT_WATER_HOLE_ENDING_TASK_INDEX, 0, 0);
    }
    task->state = task->state + 1;
}

/// The room task's three states, run from a stack copy by
/// `dryfieldNightWaterHoleRoomTask`: the entry tick, the idle state,
/// then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_water_hole_8017D688 = {
    { _dryfieldNightWaterHoleInitializeRoom, _dryfieldNightWaterHoleRoomIdle, taskKill },
};

/// Refuses every key-item use in this room without consuming the item.
///
/// Message `DRYFIELD_NIGHT_WATER_HOLE_MESSAGE_USE_KEY_ITEM` carries the inventory
/// item ID in `itemId`; the receiver and second argument are unused. Returns
/// zero so the item menu displays its refusal notice.
static s32 _dryfieldNightWaterHoleRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    enum { DRYFIELD_NIGHT_WATER_HOLE_KEY_ITEM_REFUSED = 0 };

    return DRYFIELD_NIGHT_WATER_HOLE_KEY_ITEM_REFUSED;
}

#include "../../shared/water_hole_door_msg.inc.c"

/// Resolves a departure's initialized destination selectors through a stage resolver.
///
/// The resolver must accept one aliased request/reply and read only area,
/// arrival, room and execution choice. Other message bytes remain uninitialized.
/// Borrows both pointers for this call and preserves stage, facing and sound.
static inline void _roomVariantResolveDeparture(RoomDeparture* departure, RoomVariantResolver resolveVariant)
{
    RoomEventMsg request;

    request.areaId    = departure->area;
    request.warp      = departure->warp;
    request.room      = departure->room;
    request.queryOnly = ROOM_EVENT_EXECUTE;
    resolveVariant(&request, &request);
    departure->area = request.areaId;
    departure->warp = request.warp;
    departure->room = request.room;
}

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

            departure.stage    = GAME_STAGE_MINE_SHELTER;
            departure.area     = GAME_AREA_SHELTER_B4_WATER_SUPPLY;
            departure.room     = DRYFIELD_NIGHT_WATER_HOLE_SHELTER_ROOM;
            departure.warp     = DRYFIELD_NIGHT_WATER_HOLE_SHELTER_ARRIVAL;
            departure.sndEvent = DRYFIELD_NIGHT_WATER_HOLE_SHELTER_SOUND;
            departure.facing   = DRYFIELD_NIGHT_WATER_HOLE_SHELTER_FACING;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            // Resolve only initialized destination selectors; the request aliases its reply.
            _roomVariantResolveDeparture(&departure, resolve);
            gRoomDeparture = departure;
            taskSpawnFromTable(&D_dryfield_night_water_hole_801805EC, 0, 0, 0);
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

    if ((request->actionId == DRYFIELD_NIGHT_WATER_HOLE_ACTION_ARRIVAL_2) && (gameFlagGetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT) == 0) && (gGameSession->location.loc.variant == DRYFIELD_NIGHT_WATER_HOLE_ARRIVAL_VARIANT)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT, 1);
        evsStartScript(D_dryfield_night_water_hole_8018067C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    actionId = request->actionId;
    if ((actionId == DRYFIELD_NIGHT_WATER_HOLE_ACTION_ARRIVAL_1) && (gameFlagGetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT) == 0) && (gGameSession->location.loc.variant == actionId)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_WATER_HOLE_ARRIVAL_EVENT, 1);
        evsStartScript(D_dryfield_night_water_hole_801807FC, EVENT_SCRIPT_HUD_HIDE_RESTORE);
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

    stateHandlers = D_dryfield_night_water_hole_8017D688;
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

    location = &gGameSession->location.loc;
    for (overrideIndex = 0; overrides[overrideIndex].properties != NULL; overrideIndex++) {
        surfaceProperties                                                                = Gp_RoomParamTables[location->stage - 1][location->area - 1];
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
        DRYFIELD_NIGHT_WATER_HOLE_SPLASH_SAMPLE_COUNT = ARRAY_SIZE(D_dryfield_night_water_hole_801809F4),
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
#define DRYFIELD_NIGHT_WATER_HOLE_EMIT_PART_SPLASHES(sampleIndex)                                                                                                 \
    {                                                                                                                                                             \
        trackedPart = &playerTask->extra.tmd->coords[DRYFIELD_NIGHT_WATER_HOLE_SPLASH_FIRST_PART + (sampleIndex) * DRYFIELD_NIGHT_WATER_HOLE_SPLASH_PART_STRIDE]; \
        actorRenderComposeCoord(trackedPart);                                                                                                                     \
        /* Movement sets signed-halfword odds; ripple alone adds the bias. */                                                                                     \
        work->angle = ABS(D_dryfield_night_water_hole_801809F4[(sampleIndex)].vx - trackedPart->workm.t[0]) +                                                     \
                      ABS(D_dryfield_night_water_hole_801809F4[(sampleIndex)].vy - trackedPart->workm.t[1]) +                                                     \
                      ABS(D_dryfield_night_water_hole_801809F4[(sampleIndex)].vz - trackedPart->workm.t[2]) + DRYFIELD_NIGHT_WATER_HOLE_RIPPLE_ODDS_BIAS;         \
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &trackedPart->workm, &surfaceCoord.coord);                                                                 \
        surfaceCoord.parent       = surfaceParent;                                                                                                                \
        surfaceCoord.coord.t[1]   = gGameSession->waterY;                                                                                                         \
        surfaceCoord.composeStamp = GRAPHICS_COORD_DIRTY;                                                                                                         \
        actorRenderComposeCoord(&surfaceCoord);                                                                                                                   \
        randomRoll = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT);                                                          \
        if ((s32)((randomRoll >> 16) & DRYFIELD_NIGHT_WATER_HOLE_SPLASH_ROLL_MASK) < work->angle) {                                                               \
            effectSpawn(gRoomEffectWaterRippleId, &surfaceCoord, DRYFIELD_NIGHT_WATER_HOLE_RIPPLE_HALF_SIDE, 0);                                                  \
        }                                                                                                                                                         \
        work->angle -= DRYFIELD_NIGHT_WATER_HOLE_RIPPLE_ODDS_BIAS;                                                                                                \
        randomRoll   = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT);                                                        \
        if ((s32)((randomRoll >> 16) & DRYFIELD_NIGHT_WATER_HOLE_SPLASH_ROLL_MASK) < work->angle) {                                                               \
            effectSpawn(gRoomEffectWaterSprayId, &surfaceCoord, DRYFIELD_NIGHT_WATER_HOLE_SPRAY_SPAWN_ARG, 0);                                                    \
        }                                                                                                                                                         \
        D_dryfield_night_water_hole_801809F4[(sampleIndex)].vx = trackedPart->workm.t[0];                                                                         \
        D_dryfield_night_water_hole_801809F4[(sampleIndex)].vy = trackedPart->workm.t[1];                                                                         \
        D_dryfield_night_water_hole_801809F4[(sampleIndex)].vz = trackedPart->workm.t[2];                                                                         \
    }

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work       = task->spawnArg2.pointer;
    viewMask   = 1 << gGameSession->location.loc.view;
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
                trackedPart                                          = &playerTask->extra.tmd->coords[DRYFIELD_NIGHT_WATER_HOLE_SPLASH_FIRST_PART + sampleIndex * DRYFIELD_NIGHT_WATER_HOLE_SPLASH_PART_STRIDE];
                D_dryfield_night_water_hole_801809F4[sampleIndex].vx = trackedPart->workm.t[0];
                D_dryfield_night_water_hole_801809F4[sampleIndex].vy = trackedPart->workm.t[1];
                D_dryfield_night_water_hole_801809F4[sampleIndex].vz = trackedPart->workm.t[2];
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
                    _glowDrawShaft(&D_dryfield_night_water_hole_80180994[0], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                    _glowDrawShaft(&D_dryfield_night_water_hole_80180994[2], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                }
                if (viewMask & DRYFIELD_NIGHT_WATER_HOLE_SHAFT_GROUP_2_VIEWS) {
                    _glowDrawShaft(&D_dryfield_night_water_hole_801809B4[0], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                    _glowDrawShaft(&D_dryfield_night_water_hole_801809B4[2], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                }
                if (viewMask & DRYFIELD_NIGHT_WATER_HOLE_SHAFT_GROUP_3_VIEWS) {
                    _glowDrawShaft(&D_dryfield_night_water_hole_801809D4[0], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                    _glowDrawShaft(&D_dryfield_night_water_hole_801809D4[2], DRYFIELD_NIGHT_WATER_HOLE_SHAFT_RADIUS_SCALE);
                }
            }
            break;
    }
}

#undef DRYFIELD_NIGHT_WATER_HOLE_EMIT_PART_SPLASHES

#include "../../shared/glow_draw_shaft.inc.c"

#include "../../shared/water_ripple_task.inc.c"

void dryfieldNightWaterHoleWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16.inc.c"

void dryfieldNightWaterHoleWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"
