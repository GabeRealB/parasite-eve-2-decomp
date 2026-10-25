#include "rooms/shelter_b2_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00300.h"
#include "actors/actor_01100.h"
#include "actors/actor_02100.h"
#include "actors/actor_02600.h"
#include "actors/actor_05700.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
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
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
#include "../../shared/room/shelter_elevator.h"

/// Task descriptor `_roomEventGate` spawns when a
/// gated event fires.

/// Copy of the message that fired a gated event, kept for the task
/// `roomEventTask` to warp from.

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `roomEventTask` plays.
extern RoomEventReq gRoomEventReq;

/// Event task: plays the recorded request's cap command and voice lines, then
/// copies the recorded message's area, warp and room into the save location,
/// spawns task 0x11 and ends.

extern RoomEventActiveBytes gRoomEventActive;

static void _shelterB2ElevatorHallInitRoomTask(Task* task);
static void _shelterB2ElevatorHallIdleRoomTask(Task* task);

static s32 _shelterB2ElevatorHallResolveRoomTransition(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB2ElevatorHallRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterB2ElevatorHallIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB2ElevatorHallIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32 _shelterB2ElevatorHallHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg);

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Spawns the hall's elevator floor-selection and ride-wait task at index zero.
///
/// Bodyless, priority `TASK_PRIORITY_SCENE`, with no descriptor terminator. The first spawn word
/// is the resolved sound-script ID to wait for; the second is unused. Requires
/// the hall's floor-selection CAP command and loaded map/session resources.
/// Spawning reads this recipe synchronously; its callback must remain loaded
/// until the task finishes or requests the selected hall's reload.
static TaskDesc _gShelterB2ElevatorHallElevatorTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = shelterElevatorTask,
    .data     = { .value = 0 },
};

/// Five room-message handlers and the receiver table's end marker.
///
/// RESOLVE borrows eight-byte request/reply records (which may alias) and
/// returns 0 intercepted/refused, 1 accepted/bypassed or 2 eligible for a door
/// event. USE_KEY_ITEM takes an item ID and refuses it with zero. ROOM_ACTION
/// borrows a DirectionActionRequest; COMMAND takes two integer words; both
/// are ignored with zero. SOUND takes an integer cue (1 starts the elevator
/// script) and an unused word, returning zero. Unsupported IDs return zero;
/// never send TASK_MESSAGE_TABLE_END. The room task borrows this table and
/// its callbacks for the receiver's lifetime.
static TaskMessageEntry _gShelterB2ElevatorHallRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB2ElevatorHallResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB2ElevatorHallRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2ElevatorHallIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB2ElevatorHallIgnoreRoomCommand },
    { ROOM_MESSAGE_SOUND, _shelterB2ElevatorHallHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space endpoint pairs for the hall's lamp glows.
///
/// Each even index begins a lamp; the following point ends it. Camera draws
/// use overlapping ranges of this table. Coordinates are signed game units.
static SVECTOR _gShelterB2ElevatorHallLampEndpoints[26] = {
    { -8291, -620, -1755, 0 },
    { -7626, -620, -1755, 0 },
    { -5333, -620, -1755, 0 },
    { -4665, -620, -1755, 0 },
    { -1331, -620, -1755, 0 },
    { -663, -620, -1755, 0 },
    { 1669, -620, -1755, 0 },
    { 2328, -620, -1755, 0 },
    { 5664, -620, -1755, 0 },
    { 6336, -620, -1755, 0 },
    { 8672, -620, -1755, 0 },
    { 9336, -620, -1755, 0 },
    { -1331, -620, 1757, 0 },
    { -663, -620, 1757, 0 },
    { 1669, -620, 1757, 0 },
    { 2328, -620, 1757, 0 },
    { -9760, -620, -235, 0 },
    { -9760, -620, 368, 0 },
    { 0x280E, -620, 2564, 0 },
    { 0x280E, -620, 3170, 0 },
    { 0x2749, -2342, 3789, 0 },
    { 0x2749, -2342, 4279, 0 },
    { 0x2735, -2342, -255, 0 },
    { 0x2735, -2342, -744, 0 },
    { -5622, -2399, 1652, 0 },
    { -5249, -2399, 1652, 0 },
};

/// World-space centre of the south walkway door's blue unlocked indicator.
///
/// Coordinates are signed game units, borrowed only during the glow draw.
static SVECTOR _gShelterB2ElevatorHallUnlockedDoorIndicatorPoint = { 9936, -1250, 3234, 0 };

/// World-space centre of the south walkway door's red locked indicator.
///
/// Coordinates are signed game units, borrowed only during the glow draw.
static SVECTOR _gShelterB2ElevatorHallLockedDoorIndicatorPoint = { 9901, -1195, 3234, 0 };

/// Initializes this overlay's three halo tint rows.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxShade` and
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` =
/// `[ROOM_VISUAL_EFFECTS_HALO_SHADE_COUNT]`; supplies exactly three elements.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    { .rShift = 0, .gShift = 1, .bShift = 2 },         \
    { .rShift = 2, .gShift = 1, .bShift = 0 },         \
    { .rShift = 0, .gShift = 2, .bShift = 1 },         \
}
/// Selects the element type of this overlay's three-row halo tint array.
///
/// Expands to the `RoomFxShade` type token; requires
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` =
/// `[ROOM_VISUAL_EFFECTS_HALO_SHADE_COUNT]` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` containing three RGB-shift rows.
/// Read indices are 0..2. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxShade
/// Declares the three-row halo tint array.
///
/// Supplies an array bound in `RoomFxShade` elements; requires
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be `RoomFxShade`. Bind around
/// `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND [ROOM_VISUAL_EFFECTS_HALO_SHADE_COUNT]
#include "../../shared/room/room_visual_effects_halo_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawTintedDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

u8* gShelterB2ElevatorHallViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB2ElevatorHallRoomViewCounts[SHELTER_B2_ELEVATOR_HALL_ROOM_COUNT] = {
    ARRAY_SIZE(gShelterB2ElevatorHallViewCameras),
};

/// Hall endpoint slots and the south walkway door's map-marker nibble.
enum {
    SHELTER_B2_ELEVATOR_HALL_WARP_MAIN_CORRIDOR     = 1,
    SHELTER_B2_ELEVATOR_HALL_WARP_ELEVATOR          = 2,
    SHELTER_B2_ELEVATOR_HALL_WARP_SOUTH_WALKWAY     = 3,
    SHELTER_B2_ELEVATOR_HALL_SOUTH_WALKWAY_MAP_FLAG = 457,
};

DirectionWarpEntry gShelterB2ElevatorHallWarpEntries[SHELTER_B2_ELEVATOR_HALL_WARP_ENTRY_COUNT] = {
    [SHELTER_B2_ELEVATOR_HALL_WARP_MAIN_CORRIDOR - 1] = {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -5465, 0, 1040 },
        .unknown_10     = { 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -5465, 0, 1040 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 6),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 5),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_SHELTER_1C7,
    },
    [SHELTER_B2_ELEVATOR_HALL_WARP_ELEVATOR - 1] = {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9400, 0, -600 },
        .unknown_10     = { 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9400, 0, -600 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_SHELTER_1BB,
    },
    [SHELTER_B2_ELEVATOR_HALL_WARP_SOUTH_WALKWAY - 1] = {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9484, 0, 3743 },
        .unknown_10     = { 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9484, 0, 3743 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 4),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 3),
        .blockedSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 8),
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = SHELTER_B2_ELEVATOR_HALL_SOUTH_WALKWAY_MAP_FLAG,
    },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gShelterB2ElevatorHallRoomCollisionNormals[7] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_normals.inc"
};

/// Wall and floor mesh vertices in signed room game coordinates.
static SVECTOR _gShelterB2ElevatorHallRoomCollisionVerts[48] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_verts.inc"
};

/// Indexed wall quads (class 0) and floor quads (classes 1 and 2).
static WorldCollisionGridFace _gShelterB2ElevatorHallRoomCollisionFaces[28] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_faces.inc"
};

/// Packed cell face-index lists, each ending at `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gShelterB2ElevatorHallRoomCollisionCells[124] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB2ElevatorHallRoomCollisionCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..123
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B2_ELEVATOR_HALL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB2ElevatorHallRoomCollisionCells[(halfwordOffset)])
/// Face-list starts for the 5-by-2 grid, indexed by X times 2 plus Z.
static s16* _gShelterB2ElevatorHallRoomCollisionTable[10] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_table.inc"
};
#undef SHELTER_B2_ELEVATOR_HALL_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB2ElevatorHallRoomCollision = {
    .viewCoord   = NULL,
    .normals     = _gShelterB2ElevatorHallRoomCollisionNormals,
    .vertices    = _gShelterB2ElevatorHallRoomCollisionVerts,
    .faces       = _gShelterB2ElevatorHallRoomCollisionFaces,
    .cellFaceIds = _gShelterB2ElevatorHallRoomCollisionTable,
    .xBias       = 9350,
    .zBias       = 1481,
    .cellCountX  = 5,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB2ElevatorHallRoomCollisionFaces),
};

ViewCamera gShelterB2ElevatorHallViewCameras[SHELTER_B2_ELEVATOR_HALL_VIEW_COUNT] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -20, 0x7530, -890 } }, 358 },
    { { { { 1273, 0, 3893 }, { 27, 4095, -8 }, { -3893, 28, 1273 } }, { 2681, 1099, 981 } }, 230 },
    { { { { 1056, 0, 3957 }, { 303, 4083, -81 }, { -3945, 314, 1053 } }, { -2887, 1466, 974 } }, 230 },
    { { { { 1008, 0, -3969 }, { -238, 4088, -60 }, { 3962, 246, 1006 } }, { 2609, 1438, 888 } }, 230 },
    { { { { 1006, 0, -3970 }, { -325, 4082, -82 }, { 3957, 335, 1003 } }, { -1476, 1537, 851 } }, 230 },
    { { { { 3845, 0, -1410 }, { -107, 4084, -293 }, { 1406, 312, 3834 } }, { -4795, 1541, 1484 } }, 230 },
    { { { { -1552, 0, -3790 }, { -270, 4085, 110 }, { 3780, 292, -1548 } }, { -8355, 1552, -250 } }, 257 },
};

/// Empty sprite batches for view 1, retaining decoded background strips.
///
/// The zero-count background record precedes the list terminator. This list
/// also occupies the view's unused source slot and is never read as sprites.
static SpriteBatch _gShelterB2ElevatorHallView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for view 2, retaining decoded background strips.
///
/// The zero-count background record precedes the list terminator. This list
/// also occupies the view's unused source slot and is never read as sprites.
static SpriteBatch _gShelterB2ElevatorHallView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for view 3, retaining decoded background strips.
///
/// The zero-count background record precedes the list terminator. This list
/// also occupies the view's unused source slot and is never read as sprites.
static SpriteBatch _gShelterB2ElevatorHallView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Encoded room-sprite atlas addresses and neutral opaque colour modulation.
enum {
    SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE     = getTPage(1, 0, 960, 0),
    SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT             = getClut(0, 255),
    SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR    = 128,
    SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED = 0,
};

/// Ten foreground texture rectangles and sorting depths for view 4.
///
/// Positions are pixels relative to the view centre; sizes and UVs are texels.
/// Cached packets snapshot geometry while continuing to read these depths.
static SpriteSource _gShelterB2ElevatorHallView4SpriteSources[10] = {
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 32, 48 } }, -96, -8, 1410, { .fields = { 96, 200 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 32, 40 } }, -96, -48, 1362, { .fields = { 72, 0 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 24, 56 } }, -64, 0, 1605, { .fields = { 104, 0 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 24, 48 } }, -64, -48, 1559, { .fields = { 96, 104 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 16, 48 } }, -40, -48, 1616, { .fields = { 112, 56 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 16, 48 } }, -40, 0, 1672, { .fields = { 112, 152 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 8, 48 } }, -24, -8, 1660, { .fields = { 120, 104 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 8, 40 } }, -24, -48, 1646, { .fields = { 96, 40 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 8, 40 } }, -16, -48, 1815, { .fields = { 104, 56 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 8, 40 } }, -16, -8, 1799, { .fields = { 104, 152 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
};

/// View 4's background selection, complete foreground range and list terminator.
///
/// The zero-count first record keeps decoded image strips behind the sprites.
static SpriteBatch _gShelterB2ElevatorHallView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB2ElevatorHallView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's depth-limited clip rectangle, followed by its list terminator.
///
/// Clips scene drawing to (147, 70, 103, 94) draw-buffer pixels until the
/// full-screen restore at unscaled sorting depth 1850.
static SpriteDrawArea _gShelterB2ElevatorHallView4DrawAreas[2] = {
    { { 147, 70, 103, 94 }, 1850 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Twelve foreground texture rectangles and sorting depths for view 5.
///
/// Positions are pixels relative to the view centre; sizes and UVs are texels.
/// Cached packets snapshot geometry while continuing to read these depths.
static SpriteSource _gShelterB2ElevatorHallView5SpriteSources[12] = {
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 16, 48 } }, -88, 72, 822, { .fields = { 104, 160 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 48, 56 } }, -160, -120, 645, { .fields = { 80, 0 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 40, 56 } }, -112, -120, 780, { .fields = { 88, 56 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 40, 40 } }, -112, -64, 809, { .fields = { 40, 208 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, -24, 825, { .fields = { 88, 112 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -24, 654, { .fields = { 80, 208 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 48, 40 } }, -160, -64, 651, { .fields = { 24, 104 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 24, 658, { .fields = { 56, 160 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 72, 667, { .fields = { 40, 56 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 40, 48 } }, -112, 24, 835, { .fields = { 40, 0 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 8, 48 } }, -112, 72, 834, { .fields = { 120, 160 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
    { SHELTER_B2_ELEVATOR_HALL_SPRITE_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_HALL_SPRITE_CLUT, { .fields = { 16, 48 } }, -104, 72, 822, { .fields = { 72, 104 } }, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_NEUTRAL_COLOR, SHELTER_B2_ELEVATOR_HALL_SPRITE_OPAQUE_MODULATED },
};

/// View 5's background selection, complete foreground range and list terminator.
///
/// The zero-count first record keeps decoded image strips behind the sprites.
static SpriteBatch _gShelterB2ElevatorHallView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB2ElevatorHallView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for view 6, retaining decoded background strips.
///
/// The zero-count background record precedes the list terminator. This list
/// also occupies the view's unused source slot and is never read as sprites.
static SpriteBatch _gShelterB2ElevatorHallView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for view 7, retaining decoded background strips.
///
/// The zero-count background record precedes the list terminator. This list
/// also occupies the view's unused source slot and is never read as sprites.
static SpriteBatch _gShelterB2ElevatorHallView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB2ElevatorHallSpriteViews[7] = {
    { { .empty = _gShelterB2ElevatorHallView1SpriteBatches }, _gShelterB2ElevatorHallView1SpriteBatches, NULL },
    { { .empty = _gShelterB2ElevatorHallView2SpriteBatches }, _gShelterB2ElevatorHallView2SpriteBatches, NULL },
    { { .empty = _gShelterB2ElevatorHallView3SpriteBatches }, _gShelterB2ElevatorHallView3SpriteBatches, NULL },
    { { .elements = _gShelterB2ElevatorHallView4SpriteSources }, _gShelterB2ElevatorHallView4SpriteBatches, _gShelterB2ElevatorHallView4DrawAreas },
    { { .elements = _gShelterB2ElevatorHallView5SpriteSources }, _gShelterB2ElevatorHallView5SpriteBatches, NULL },
    { { .empty = _gShelterB2ElevatorHallView6SpriteBatches }, _gShelterB2ElevatorHallView6SpriteBatches, NULL },
    { { .empty = _gShelterB2ElevatorHallView7SpriteBatches }, _gShelterB2ElevatorHallView7SpriteBatches, NULL },
};

/// Fourteen authored point lights contributing in every hall view.
///
/// Positions and falloff radii use world units; RGB intensities have twelve
/// fractional bits. Coordinate composition and attenuation keep entries writable.
static WorldCoordPointLight _gShelterB2ElevatorHallPointLights[14] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -681, -659, -1106 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -681, -659, 1137 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1648, -659, 1137 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1047, -659, -1192 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6077, -659, -1034 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8608, -659, -855 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8600, -659, 2166 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4834, -659, 2957 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7805, -6652, 1436 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3279, 3368, 3448 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2330, -6652, -263 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3267, 3308, 3588 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5813, -5652, -97 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3366, 3466, 3708 }, { 0, 0 } }, 2000, 0x2801 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8513, -659, 35 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7513, -659, -1106 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4617, -659, -1134 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
};

WorldCoordRoomLights gShelterB2ElevatorHallRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB2ElevatorHallPointLights),
    .pointLights           = _gShelterB2ElevatorHallPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB2ElevatorHallViewBoundaryTriggers[SHELTER_B2_ELEVATOR_HALL_VIEW_BOUNDARY_TRIGGER_COUNT] = {
    { NULL, NULL, NULL, { -4678, 64, -197, 0 }, { { 115, -2016, 2533, 0 }, { -124, -2016, -2543, 0 }, { 115, 2016, 2533, 0 }, { -124, 2016, -2543, 0 } }, { -4101, 0, 192, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4865, 0, -144, 0 }, { { -141, -2016, -2631, 0 }, { 126, -2016, 2616, 0 }, { -141, 2016, -2631, 0 }, { 126, 2016, 2616, 0 } }, { 4090, 0, -209, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 64, 0, 0, 0 }, { { -5, -2016, -2627, 0 }, { 5, -2016, 2627, 0 }, { -5, 2016, -2627, 0 }, { 5, 2016, 2627, 0 } }, { 4095, 0, -8, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 256, 0, 0, 0 }, { { -5, -2016, 2541, 0 }, { 5, -2016, -2541, 0 }, { -5, 2016, 2541, 0 }, { 5, 2016, -2541, 0 } }, { -4106, 0, -9, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4256, 0, 0, 0 }, { { -259, -2016, 2523, 0 }, { 249, -2016, -2533, 0 }, { -259, 2016, 2523, 0 }, { 249, 2016, -2533, 0 } }, { -4084, 0, -412, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4065, 0, 0, 0 }, { { 249, -2016, -2620, 0 }, { -257, -2016, 2609, 0 }, { 249, 2016, -2620, 0 }, { -257, 2016, 2609, 0 } }, { 4076, 0, 394, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7680, 0, 2320, 0 }, { { -3429, -2016, -371, 0 }, { 3429, -2016, 371, 0 }, { -3429, 2016, -371, 0 }, { 3429, 2016, 371, 0 } }, { 441, 0, -4083, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7808, 0, 2097, 0 }, { { 3515, -2016, 381, 0 }, { -3515, -2016, -381, 0 }, { 3515, 2016, 381, 0 }, { -3515, 2016, -381, 0 } }, { -443, 0, 4080, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder gShelterB2ElevatorHallSightOccluders[SHELTER_B2_ELEVATOR_HALL_SIGHT_OCCLUDER_COUNT] = {
    { NULL, NULL, { 2560, -1344, 3856, 0 }, { { -1664, 2368, 2096, 0 }, { 1664, 2368, -2096, 0 }, { -1664, -2368, 2096, 0 }, { 1664, -2368, -2096, 0 } }, { 3208, 0, 2546, 0 }, 3565, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionTrigger gShelterB2ElevatorHallWarpTriggers[SHELTER_B2_ELEVATOR_HALL_WARP_TRIGGER_COUNT] = {
    { NULL, NULL, NULL, { -5632, -48, 1072, 0 }, { { -832, 0, -464, 0 }, { 833, 0, -464, 0 }, { -832, 0, 464, 0 }, { 833, 0, 464, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -ONE, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, (SHELTER_B2_ELEVATOR_HALL_WARP_MAIN_CORRIDOR << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9712, -48, -464, 0 }, { { 384, 0, -624, 0 }, { 384, 0, 624, 0 }, { -384, 0, -624, 0 }, { -384, 0, 624, 0 } }, { 0, 4104, 0, 0 }, { -ONE, 0, 0, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_ELEVATOR, (SHELTER_B2_ELEVATOR_HALL_WARP_ELEVATOR << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9648, -48, 4272, 0 }, { { 384, 0, -752, 0 }, { 384, 0, 752, 0 }, { -384, 0, -752, 0 }, { -384, 0, 752, 0 } }, { 0, 4113, 0, 0 }, { -ONE, 0, 0, 0 }, 844, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, (SHELTER_B2_ELEVATOR_HALL_WARP_SOUTH_WALKWAY << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resource keys; each also equals its actor package's decimal file number here.
///
/// Entry keys join placements to recipes; file-group selectors independently
/// choose slot 1 (base 10) or slot 2 (base 20).
enum {
    SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT         = 26,
    SHELTER_B2_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER  = 3,
    SHELTER_B2_ELEVATOR_HALL_RESOURCE_LARGE_MOSSBACK = 49,
    SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER        = 21,
    SHELTER_B2_ELEVATOR_HALL_RESOURCE_LAUNCHER_ROOK  = 57,
};

/// Slot-1 Maggot package and body recipe for placement variant 1, then the end marker.
///
/// Loads actor_102600; recipe index 0 borrows its body descriptor while loaded.
static AreaResource _gShelterB2ElevatorHallVariant1Resources[2] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Brain Stinger package and body recipe for placement variant 2, then the end marker.
///
/// Loads actor_100300; recipe index 0 borrows its three-entry task table while loaded.
static AreaResource _gShelterB2ElevatorHallVariant2Resources[2] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER, SHELTER_B2_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor100300Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 large Mossback package and body recipe for placement variant 3, then the end marker.
///
/// Loads actor_104900; recipe index 0 borrows the Mossback task table at its
/// slot-1 address. Resource key 49 selects the large model's parameters and scale.
static AreaResource _gShelterB2ElevatorHallVariant3Resources[2] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LARGE_MOSSBACK, SHELTER_B2_ELEVATOR_HALL_RESOURCE_LARGE_MOSSBACK, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor101100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Watcher and slot-2 launcher Rook GOLEM packages for variant 11, then the end marker.
///
/// Loads actor_102100 and actor_205700; each recipe selects body index 0 and
/// borrows its loaded package's descriptor storage.
static AreaResource _gShelterB2ElevatorHallVariant11Resources[3] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LAUNCHER_ROOK, SHELTER_B2_ELEVATOR_HALL_RESOURCE_LAUNCHER_ROOK, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor05700RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Watcher and slot-2 launcher Rook GOLEM packages for variant 21, then the end marker.
///
/// Loads actor_102100 and actor_205700; each recipe selects body index 0 and
/// borrows its loaded package's descriptor storage independently of variant 11.
static AreaResource _gShelterB2ElevatorHallVariant21Resources[3] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LAUNCHER_ROOK, SHELTER_B2_ELEVATOR_HALL_RESOURCE_LAUNCHER_ROOK, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor05700RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor-specific placement selectors, interpreted after matching each resource key.
enum {
    SHELTER_B2_ELEVATOR_HALL_MAGGOT_MODE_DORMANT       = 0,
    SHELTER_B2_ELEVATOR_HALL_STINGER_PATROL_SET_0      = 0,
    SHELTER_B2_ELEVATOR_HALL_STINGER_PATROL_SET_1      = 1,
    SHELTER_B2_ELEVATOR_HALL_MOSSBACK_ACTION_AWARENESS = 2,
    SHELTER_B2_ELEVATOR_HALL_WATCHER_STATIONARY_BEAM_3 = 3,
    SHELTER_B2_ELEVATOR_HALL_ROOK_MODE_IDLE            = 0,
    SHELTER_B2_ELEVATOR_HALL_ROOK_MODE_PATROL          = 1,
};

/// Seven dormant ground Maggots for variant 1, followed by the placement end marker.
///
/// World-unit XYZ and 4096-unit yaw seed their roots. Each uses timing row 0,
/// the base file, texture page offset 0 and CLUT row offset 2. Actors borrow
/// these records until teardown, within this overlay's lifetime.
static AreaPlacement _gShelterB2ElevatorHallVariant1Placements[8] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, 0, SHELTER_B2_ELEVATOR_HALL_MAGGOT_MODE_DORMANT, 7500, 0, 4150, 1800, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, 0, SHELTER_B2_ELEVATOR_HALL_MAGGOT_MODE_DORMANT, 7000, 0, 1950, 1300, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, 0, SHELTER_B2_ELEVATOR_HALL_MAGGOT_MODE_DORMANT, 5300, 0, 3600, 1700, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, 0, SHELTER_B2_ELEVATOR_HALL_MAGGOT_MODE_DORMANT, 5400, 0, 1700, 1500, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, 0, SHELTER_B2_ELEVATOR_HALL_MAGGOT_MODE_DORMANT, 1850, 0, 350, 1250, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, 0, SHELTER_B2_ELEVATOR_HALL_MAGGOT_MODE_DORMANT, 300, 0, 600, 1200, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_MAGGOT, 0, SHELTER_B2_ELEVATOR_HALL_MAGGOT_MODE_DORMANT, 300, 0, -800, 900, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two Brain Stingers using the hall's patrol sets 0 and 1 for variant 2, then the end marker.
///
/// World-unit XYZ and 4096-unit yaw seed their roots; base-file placements use
/// texture page offset 0 and CLUT row offset 2. Actors borrow the records until
/// teardown, within this overlay's lifetime.
static AreaPlacement _gShelterB2ElevatorHallVariant2Placements[3] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER, 0, SHELTER_B2_ELEVATOR_HALL_STINGER_PATROL_SET_0, 6500, 0, 3300, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_BRAIN_STINGER, 0, SHELTER_B2_ELEVATOR_HALL_STINGER_PATROL_SET_1, -4000, 0, 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Three large Mossbacks with action-sensitive awareness for variant 3, then the end marker.
///
/// Variant 2 becomes spawn payload 0x20000: player actions or nearby running
/// provoke awareness. World-unit XYZ and 4096-unit yaw seed the roots;
/// base-file placements use texture page offset 0 and CLUT row offset 2.
/// Actors borrow these records until teardown, within this overlay's lifetime.
static AreaPlacement _gShelterB2ElevatorHallVariant3Placements[4] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LARGE_MOSSBACK, SHELTER_B2_ELEVATOR_HALL_MOSSBACK_ACTION_AWARENESS, 0, -7800, 0, -200, 1200, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LARGE_MOSSBACK, SHELTER_B2_ELEVATOR_HALL_MOSSBACK_ACTION_AWARENESS, 0, 250, 0, 600, 1600, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LARGE_MOSSBACK, SHELTER_B2_ELEVATOR_HALL_MOSSBACK_ACTION_AWARENESS, 0, 4350, 0, 500, 1800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Four stationary beam-3 Watchers and one patrolling launcher Rook GOLEM for variant 11.
///
/// Watchers use sight row 0 (3000 units); the Rook walks 3000-unit patrol legs.
/// XYZ are world units and yaw uses 4096 per turn. Watcher page/CLUT offsets
/// are 0/2, Rook offsets 2/4; all use base files. The end marker follows five
/// live records, which their actors borrow until teardown.
static AreaPlacement _gShelterB2ElevatorHallVariant11Placements[6] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, SHELTER_B2_ELEVATOR_HALL_WATCHER_STATIONARY_BEAM_3, 0, 7500, -1800, -1400, 0, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, SHELTER_B2_ELEVATOR_HALL_WATCHER_STATIONARY_BEAM_3, 0, 5500, -1800, -1400, 0, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, SHELTER_B2_ELEVATOR_HALL_WATCHER_STATIONARY_BEAM_3, 0, 7500, -1800, 4900, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, SHELTER_B2_ELEVATOR_HALL_WATCHER_STATIONARY_BEAM_3, 0, 5500, -1800, 4900, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LAUNCHER_ROOK, 3, SHELTER_B2_ELEVATOR_HALL_ROOK_MODE_PATROL, 6500, 0, 3000, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two stationary beam-3 Watchers and two launcher Rook GOLEMs for variant 21.
///
/// Watchers use sight row 7 (8000 units); one Rook walks 9000-unit patrol legs,
/// the other starts idle. XYZ are world units and yaw uses 4096 per turn.
/// Watcher page/CLUT offsets are 0/2, Rook offsets 2/4; all use base files.
/// The end marker follows four live records, borrowed until actor teardown.
static AreaPlacement _gShelterB2ElevatorHallVariant21Placements[5] = {
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, SHELTER_B2_ELEVATOR_HALL_WATCHER_STATIONARY_BEAM_3, 0, 8500, -1800, 4900, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 7 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_WATCHER, SHELTER_B2_ELEVATOR_HALL_WATCHER_STATIONARY_BEAM_3, 0, 6500, -1800, 4900, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 7 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LAUNCHER_ROOK, 9, SHELTER_B2_ELEVATOR_HALL_ROOK_MODE_PATROL, -2000, 0, 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 2, 4, 0 },
    { SHELTER_B2_ELEVATOR_HALL_RESOURCE_LAUNCHER_ROOK, 0, SHELTER_B2_ELEVATOR_HALL_ROOK_MODE_IDLE, 9600, 0, -500, ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB2ElevatorHallAreaVariants[SHELTER_B2_ELEVATOR_HALL_AREA_VARIANT_COUNT] = {
    [1]  = { .placements = _gShelterB2ElevatorHallVariant1Placements, .resources = _gShelterB2ElevatorHallVariant1Resources },
    [2]  = { .placements = _gShelterB2ElevatorHallVariant2Placements, .resources = _gShelterB2ElevatorHallVariant2Resources },
    [3]  = { .placements = _gShelterB2ElevatorHallVariant3Placements, .resources = _gShelterB2ElevatorHallVariant3Resources },
    [11] = { .placements = _gShelterB2ElevatorHallVariant11Placements, .resources = _gShelterB2ElevatorHallVariant11Resources },
    [21] = { .placements = _gShelterB2ElevatorHallVariant21Placements, .resources = _gShelterB2ElevatorHallVariant21Resources },
};

/// Cue-2 script bases in the loaded room's type-1 footstep bank.
enum {
    SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B,
    SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// Walk, run and scripted-jump cue bases for floor surface class 1.
///
/// Animation cue 1 adds one to the base; companion playback adds another 100.
/// Surface records borrow these requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gShelterB2ElevatorHallSurfaceClass1FootstepSounds = {
    .walk         = SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_1_WALK_CUE_BASE,
    .run          = SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_1_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_1_WALK_CUE_BASE,
};

/// Walk, run and scripted-jump cue bases for floor surface class 2.
///
/// Animation cue 1 adds one to the base; companion playback adds another 100.
/// Surface records borrow these requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gShelterB2ElevatorHallSurfaceClass2FootstepSounds = {
    .walk         = SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_2_WALK_CUE_BASE,
    .run          = SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_2_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B2_ELEVATOR_HALL_SURFACE_CLASS_2_WALK_CUE_BASE,
};

/// Blocking, impact-enabled, pushback-applying policy without footsteps for classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gShelterB2ElevatorHallSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking, impact-enabled, pushback-applying policy and footsteps for floor class 1.
static WorldCollisionSurfaceProperties _gShelterB2ElevatorHallSurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB2ElevatorHallSurfaceClass1FootstepSounds,
};

/// Blocking, impact-enabled, pushback-applying policy and footsteps for floor class 2.
static WorldCollisionSurfaceProperties _gShelterB2ElevatorHallSurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB2ElevatorHallSurfaceClass2FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB2ElevatorHallSurfacePropertiesByClass[8] = {
    &_gShelterB2ElevatorHallSilentSurfaceProperties,
    &_gShelterB2ElevatorHallSurfaceClass1Properties,
    &_gShelterB2ElevatorHallSurfaceClass2Properties,
    &_gShelterB2ElevatorHallSilentSurfaceProperties,
    &_gShelterB2ElevatorHallSilentSurfaceProperties,
    &_gShelterB2ElevatorHallSilentSurfaceProperties,
    &_gShelterB2ElevatorHallSilentSurfaceProperties,
    &_gShelterB2ElevatorHallSilentSurfaceProperties,
};

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 253, 152, 217 } };

RoomEventReq gRoomEventReq;

#include "../../shared/room/room_event.inc.c"

#include "../../shared/room/shelter_elevator_task.inc.c"

/// Resolves an elevator-hall departure and gates its door or elevator event.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows a complete eight-byte request and
/// writable reply, which may alias. Copies the record before selecting its
/// Shelter room variant. Corridor and south-walkway departures return the
/// door gate's 0 refused, 1 bypassed or 2 eligible result; the latter requires
/// Bowman's Card and identifies it when an event is requested. Elevator
/// departures return 0 and, on execution, either play the disabled notice or
/// start the ride. Other destinations return 1. Queries suppress playback and
/// flag writes, while the gate still clears its event-start latch. Keep the
/// room, map overlay and event resources loaded through deferred execution.
static s32 _shelterB2ElevatorHallResolveRoomTransition(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B2_ELEVATOR_HALL_TRANSITION_INTERCEPTED   = 0,
        SHELTER_B2_ELEVATOR_HALL_TRANSITION_ACCEPTED      = 1,
        SHELTER_B2_ELEVATOR_HALL_CAP_CORRIDOR_DOOR        = 1,
        SHELTER_B2_ELEVATOR_HALL_CAP_WALKWAY_DOOR         = 3,
        SHELTER_B2_ELEVATOR_HALL_CAP_WALKWAY_CARD_MISSING = 2,
        SHELTER_B2_ELEVATOR_HALL_CAP_ELEVATOR_DISABLED    = 4,
        SHELTER_B2_ELEVATOR_HALL_CAP_ELEVATOR_RIDE        = 5,
        SHELTER_B2_ELEVATOR_HALL_EVENT_FLAG_REFUSED       = 2,
        SHELTER_B2_ELEVATOR_HALL_CARD_IDENTIFIED          = 1,
        SHELTER_B2_ELEVATOR_HALL_NO_COLLECTION_REQUIRED   = 0,
        SHELTER_B2_ELEVATOR_HALL_COLLECTION_BIT_MASK      = 0x7F,
        SHELTER_B2_ELEVATOR_HALL_CORRIDOR_FIRST_SOUND     = 7,
        SHELTER_B2_ELEVATOR_HALL_CORRIDOR_SECOND_SOUND    = 5,
        SHELTER_B2_ELEVATOR_HALL_WALKWAY_FIRST_SOUND      = 9,
        SHELTER_B2_ELEVATOR_HALL_WALKWAY_SECOND_SOUND     = 3,
        SHELTER_B2_ELEVATOR_HALL_RIDE_SOUND               = 1,
    };

    RoomEventReq doorEvent;
    s32          gateResult;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
        doorEvent.capCmd        = SHELTER_B2_ELEVATOR_HALL_CAP_CORRIDOR_DOOR;
        doorEvent.missingCapCmd = SHELTER_B2_ELEVATOR_HALL_CAP_CORRIDOR_DOOR;
        doorEvent.firstSnd      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, SHELTER_B2_ELEVATOR_HALL_CORRIDOR_FIRST_SOUND);
        doorEvent.secondSnd     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, SHELTER_B2_ELEVATOR_HALL_CORRIDOR_SECOND_SOUND);
        doorEvent.flagId        = GAME_FLAG_B2_CORRIDOR_ELEVATOR_HALL_UNLOCKED;
        doorEvent.collectedBit  = SHELTER_B2_ELEVATOR_HALL_NO_COLLECTION_REQUIRED;
        return _roomEventGate(&doorEvent, reply);
    }
    if (request->areaId == GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY) {
        doorEvent.capCmd        = SHELTER_B2_ELEVATOR_HALL_CAP_WALKWAY_DOOR;
        doorEvent.missingCapCmd = SHELTER_B2_ELEVATOR_HALL_CAP_WALKWAY_CARD_MISSING;
        doorEvent.firstSnd      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, SHELTER_B2_ELEVATOR_HALL_WALKWAY_FIRST_SOUND);
        doorEvent.secondSnd     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, SHELTER_B2_ELEVATOR_HALL_WALKWAY_SECOND_SOUND);
        doorEvent.flagId        = GAME_FLAG_B2_HALL_SOUTH_WALKWAY_DOOR_UNLOCKED;
        doorEvent.collectedBit  = INVENTORY_COLLECTION_ID_BOWMANS_CARD & SHELTER_B2_ELEVATOR_HALL_COLLECTION_BIT_MASK;
        gateResult              = _roomEventGate(&doorEvent, reply);
        if (gRoomEventActive.eventStarted != 0) {
            itemSetIdentified(INVENTORY_COLLECTION_ID_BOWMANS_CARD, SHELTER_B2_ELEVATOR_HALL_CARD_IDENTIFIED);
        }
        return gateResult;
    }
    // Elevator travel is handled by its ride task rather than a door transition.
    if (request->areaId == GAME_AREA_SHELTER_B2_ELEVATOR) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, SHELTER_B2_ELEVATOR_HALL_EVENT_FLAG_REFUSED);
                capRunCommandWithTransition(SHELTER_B2_ELEVATOR_HALL_CAP_ELEVATOR_DISABLED);
            }
            return SHELTER_B2_ELEVATOR_HALL_TRANSITION_INTERCEPTED;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommand(SHELTER_B2_ELEVATOR_HALL_CAP_ELEVATOR_RIDE, CAP_PLAYBACK_IN_PLACE);
            taskSpawnFromTable(&_gShelterB2ElevatorHallElevatorTaskDesc, 0, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, SHELTER_B2_ELEVATOR_HALL_RIDE_SOUND), 0);
        }
        return SHELTER_B2_ELEVATOR_HALL_TRANSITION_INTERCEPTED;
    }
    return SHELTER_B2_ELEVATOR_HALL_TRANSITION_ACCEPTED;
}

/// Refuses every key-item use request (message 0x13F1), returning zero.
static s32 _shelterB2ElevatorHallRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores room commands and their argument words, returning zero.
static s32 _shelterB2ElevatorHallIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores the borrowed room-action request, returning zero without reading it.
static s32 _shelterB2ElevatorHallIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Starts the elevator sound script for cue 1; other cues do nothing, and all return zero.
static s32 _shelterB2ElevatorHallHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg)
{
    enum { ELEVATOR_SOUND_CUE    = 1,
           ELEVATOR_SOUND_SCRIPT = 0x541B0001 };

    if (cueId == ELEVATOR_SOUND_CUE) {
        sndEvtRequestScriptStart(ELEVATOR_SOUND_SCRIPT, 0, 0);
    }
    return 0;
}

/// Room receiver states: 0 registers the receiver, 1 idles, 2 releases the task.
///
/// Contains three TaskFunc callbacks with no terminator; dispatch requires
/// state 0..2. The receiver copies the complete table before calling one entry.
/// Keep the hall overlay loaded while its task or installed messages are live.
static const TaskFuncTable3 _gShelterB2ElevatorHallRoomStates = {
    {
        _shelterB2ElevatorHallInitRoomTask,
        _shelterB2ElevatorHallIdleRoomTask,
        taskKill,
    },
};

/// Installs the room message handlers and registers the task as the current room receiver.
///
/// Called in state 0; advances to the idle state without allocating work or a body.
static void _shelterB2ElevatorHallInitRoomTask(Task* task)
{
    task->msgTable = _gShelterB2ElevatorHallRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room receiver alive in state 1 while messages perform its work.
static void _shelterB2ElevatorHallIdleRoomTask(Task* task)
{
}

void shelterB2ElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB2ElevatorHallRoomStates;
    states.funcs[task->state](task);
}

/// Draws the south walkway door's blue unlocked or red locked status glow.
///
/// A nonzero unlock nibble selects the blue lamp's world point; zero selects
/// the red lamp's. The room glow task calls this for mapped views 5 and 6.
/// Requires composed view matrices, an initialized scratch stack and the
/// current frame's primitive arena and depth ordering table.
static inline void _shelterB2ElevatorHallDrawDoorIndicator(void)
{
    enum {
        DOOR_INDICATOR_RADIUS_SCALE  = 0x100,                                        // Outer pixels = scale * 64 / (camera Z / 4 + 1)
        DOOR_INDICATOR_FLICKER_SHIFT = 5,                                            // Odd animation frames add 32 to each RGB byte
        DOOR_UNLOCKED_COLOR          = (DOOR_INDICATOR_FLICKER_SHIFT << 12) | 0x04C, // Base RGB bytes 0, 64, 192
        DOOR_LOCKED_COLOR            = (DOOR_INDICATOR_FLICKER_SHIFT << 12) | 0xC40, // Base RGB bytes 192, 64, 0
    };

    if (gameFlagGetNibble(GAME_FLAG_B2_HALL_SOUTH_WALKWAY_DOOR_UNLOCKED) != 0) {
        _glowDrawTintedDisc(&_gShelterB2ElevatorHallUnlockedDoorIndicatorPoint, DOOR_INDICATOR_RADIUS_SCALE, DOOR_UNLOCKED_COLOR);
    } else {
        _glowDrawTintedDisc(&_gShelterB2ElevatorHallLockedDoorIndicatorPoint, DOOR_INDICATOR_RADIUS_SCALE, DOOR_LOCKED_COLOR);
    }
}

void shelterB2ElevatorHallDrawGlowsTask(Task* task)
{
    enum { GLOWS_INITIALIZE,
           GLOWS_DRAW,
           MAPPED_VIEW_MASK = 0xFF,
           LAMP_RADIUS      = 0x180,
           ACCENT_RADIUS    = 0x200,
           LAMP_COLOR       = 0x111, // RGB nibbles, each scaled by 16
           ACCENT_COLOR     = 0x412 };

    // Select this loaded room's effect implementations before any dependent spawn.
    if (task->state == GLOWS_INITIALIZE) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B2_ELEVATOR_HALL_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B2_ELEVATOR_HALL_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B2_ELEVATOR_HALL_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B2_ELEVATOR_HALL_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B2_ELEVATOR_HALL_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B2_ELEVATOR_HALL_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B2_ELEVATOR_HALL_SPARK_BURST;
        task->state               = GLOWS_DRAW;
    }

    // Draw only the world-space lamps visible from the mapped camera.
    switch (viewGetMappedIndex() & MAPPED_VIEW_MASK) {
        case 2: {
            const SVECTOR* lampEndpoints;
            lampEndpoints = _gShelterB2ElevatorHallLampEndpoints;
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[2], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[16], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[24], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
        case 3: {
            const SVECTOR* lampEndpoints;
            lampEndpoints = &_gShelterB2ElevatorHallLampEndpoints[4];
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[8], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[12], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[20], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
        case 4: {
            const SVECTOR* lampEndpoints;
            lampEndpoints = &_gShelterB2ElevatorHallLampEndpoints[6];
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[8], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[12], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[16], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
        case 5: {
            const SVECTOR* lampEndpoints;
            _shelterB2ElevatorHallDrawDoorIndicator();
            lampEndpoints = &_gShelterB2ElevatorHallLampEndpoints[18];
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[2], ACCENT_RADIUS, ACCENT_COLOR);
            _glowDrawCapsule(&lampEndpoints[4], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
        case 6: {
            const SVECTOR* lampEndpoints;
            _shelterB2ElevatorHallDrawDoorIndicator();
            lampEndpoints = &_gShelterB2ElevatorHallLampEndpoints[18];
            _glowDrawCapsule(&lampEndpoints[0], LAMP_RADIUS, LAMP_COLOR);
            _glowDrawCapsule(&lampEndpoints[2], ACCENT_RADIUS, ACCENT_COLOR);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/glow_draw_tinted_disc.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB2ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB2ElevatorHallRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB2ElevatorHallRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
