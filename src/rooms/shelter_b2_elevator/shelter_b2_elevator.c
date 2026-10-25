#include "rooms/shelter_b2_elevator.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor_142900.h"
#include "actors/actor_142900.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

/// Room-coordinate Z translation of both door-leaf roots at zero travel (-500).
#define SHELTER_B2_ELEVATOR_DOOR_CLOSED_Z (-500)

/// Whole room-coordinate units added to leaf travel per door-task tick (10).
///
/// The motion request supplies -1, 0 or 1; the leaf's side sign is applied only
/// when travel is converted to root Z.
#define SHELTER_B2_ELEVATOR_DOOR_LEAF_SPEED 10

/// Maximum door-leaf travel from its shut position, in room-coordinate units (500).
///
/// Travel clamps to 0..500 before the side sign and shut-root Z are applied.
#define SHELTER_B2_ELEVATOR_DOOR_LEAF_TRAVEL_MAX 500

/// Work block of one leaf of the elevator's sliding door, held in `Task::work`.
///
/// The door is two mirrored leaves, each its own task with its own block. The
/// task's `spawnArg1` selects opening (1), closing (-1) or rest (0), and its
/// `spawnArg2` the side the leaf retracts to (-1 or 1 along Z).
typedef struct {
    s32 travel; // World units slid from the shut position, 0 to `SHELTER_B2_ELEVATOR_DOOR_LEAF_TRAVEL_MAX`
} _ShelterB2ElevatorDoorLeafWork;
STATIC_ASSERT_SIZEOF(_ShelterB2ElevatorDoorLeafWork, 4);

/// Borrowed door-leaf task handles in negative-Z, positive-Z order.
///
/// Room initialization stores both spawn results without checking failure.
/// Door messages require both tasks to be live. Task teardown owns their bodies
/// and work and does not clear these handles; stop using them before room unload.
static Task* _gShelterB2ElevatorDoorLeafTasks[2];

static void _shelterB2ElevatorRoomIdleState(Task* unusedTask);

static s32  _shelterB2ElevatorRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _shelterB2ElevatorResolveRoomEventMessage(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB2ElevatorIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _shelterB2ElevatorIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* unusedRequest, s32 unusedArg);
static s32  _shelterB2ElevatorOpenDoorMessage(Task* task, s32 messageId, s32 unusedArg1, s32 unusedArg2);
static s32  _shelterB2ElevatorCloseDoorMessage(Task* task, s32 messageId, s32 unusedArg1, s32 unusedArg2);
static void _shelterB2ElevatorDoorLeafTask(Task* task);
static void _shelterB2ElevatorRoomInitState(Task* task);
static void _shelterB2ElevatorExitTask(Task* task);

/// Signed travel requests stored in a door leaf's first spawn argument.
enum {
    SHELTER_B2_ELEVATOR_DOOR_CLOSE = -1,
    SHELTER_B2_ELEVATOR_DOOR_REST  = 0,
    SHELTER_B2_ELEVATOR_DOOR_OPEN  = 1,
};

/// Live descriptor indices; the terminating record is not spawnable.
enum {
    SHELTER_B2_ELEVATOR_TASK_NEGATIVE_Z_LEAF = 0,
    SHELTER_B2_ELEVATOR_TASK_POSITIVE_Z_LEAF = 1,
    SHELTER_B2_ELEVATOR_TASK_EXIT            = 2,
};

/// View slots used by the arrival endpoint and the two directed boundaries.
enum {
    SHELTER_B2_ELEVATOR_ENTRY_VIEW = 2,
    SHELTER_B2_ELEVATOR_SCENE_VIEW = 3,
};

/// Sound requests stored in the elevator's warp endpoint.
enum {
    SHELTER_B2_ELEVATOR_WARP_ARRIVAL_SOUND   = 0x541A0002,
    SHELTER_B2_ELEVATOR_WARP_DEPARTURE_SOUND = 0x541A0001,
};

/// Departure slot 1 in this room and arrival slot 2 in the B2 elevator hall.
enum { SHELTER_B2_ELEVATOR_HALL_WARP_KEY = (1 << 4) | 2 };

/// Saved upper-sewer layout and map-mark policy after the first elevator scene.
enum {
    SHELTER_B2_ELEVATOR_UPPER_SEWER_LAYOUT = 4,
    SHELTER_B2_ELEVATOR_SET_MAP_MARK       = 1,
};

/// Encoded 8-bit texture pages and palette used by view 2's sprite rectangles.
enum {
    SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE  = 142,    // VRAM X = 896, Y = 0
    SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE = 143,    // VRAM X = 960, Y = 0
    SHELTER_B2_ELEVATOR_SPRITE_CLUT               = 0x3FC0, // VRAM X = 0, Y = 255
};

static TmdBone _gShelterB2ElevatorModel00688Skeleton[1] = {
#include "assets/shelter_b2_elevator_model_00688_skeleton.inc"
};

static u32 _gShelterB2ElevatorModel00688PartVerts[1] = {
#include "assets/shelter_b2_elevator_model_00688_partVerts.inc"
};

static SVECTOR _gShelterB2ElevatorModel00688Verts[21] = {
#include "assets/shelter_b2_elevator_model_00688_verts.inc"
};

static u32 _gShelterB2ElevatorModel00688Stream[66] = {
#include "assets/shelter_b2_elevator_model_00688_stream.inc"
};

static TmdSource _gShelterB2ElevatorModel00688 = {
    0,
    480,
    0,
    1,
    _gShelterB2ElevatorModel00688PartVerts,
    _gShelterB2ElevatorModel00688Verts,
    &_gShelterB2ElevatorModel00688Verts[21],
    _gShelterB2ElevatorModel00688Skeleton,
    _gShelterB2ElevatorModel00688Stream,
};

static TmdBone _gShelterB2ElevatorModel00884Skeleton[1] = {
#include "assets/shelter_b2_elevator_model_00884_skeleton.inc"
};

static u32 _gShelterB2ElevatorModel00884PartVerts[1] = {
#include "assets/shelter_b2_elevator_model_00884_partVerts.inc"
};

static SVECTOR _gShelterB2ElevatorModel00884Verts[21] = {
#include "assets/shelter_b2_elevator_model_00884_verts.inc"
};

static u32 _gShelterB2ElevatorModel00884Stream[66] = {
#include "assets/shelter_b2_elevator_model_00884_stream.inc"
};

static TmdSource _gShelterB2ElevatorModel00884 = {
    0,
    480,
    0,
    1,
    _gShelterB2ElevatorModel00884PartVerts,
    _gShelterB2ElevatorModel00884Verts,
    &_gShelterB2ElevatorModel00884Verts[21],
    _gShelterB2ElevatorModel00884Skeleton,
    _gShelterB2ElevatorModel00884Stream,
};

/// Spawn recipes for the negative-Z leaf, positive-Z leaf and exit controller.
///
/// Indices 0..2 are live; index 3 is `TASK_DESC_END`. Leaf models borrow room
/// geometry for their task lifetimes. The bodyless exit controller has priority
/// 32, ahead of the priority-192 leaves on the ascending execution list.
static TaskDesc _gShelterB2ElevatorTaskDescriptors[4] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _shelterB2ElevatorDoorLeafTask, { .model = &_gShelterB2ElevatorModel00688 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _shelterB2ElevatorDoorLeafTask, { .model = &_gShelterB2ElevatorModel00884 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB2ElevatorExitTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// The room receiver's six message handlers followed by `TASK_MESSAGE_TABLE_END`.
///
/// Resolves room transitions, refuses key items, ignores room actions/commands
/// and controls both door leaves. The receiver borrows this table while the
/// room is loaded; door messages require live leaf handles.
static TaskMessageEntry _gShelterB2ElevatorRoomMessages[7] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB2ElevatorResolveRoomEventMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB2ElevatorRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2ElevatorIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelterB2ElevatorIgnoreCommandMessage },
    { SHELTER_B2_ELEVATOR_MESSAGE_OPEN_DOOR, _shelterB2ElevatorOpenDoorMessage },
    { SHELTER_B2_ELEVATOR_MESSAGE_CLOSE_DOOR, _shelterB2ElevatorCloseDoorMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* gShelterB2ElevatorViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB2ElevatorViewCounts[1] = { ARRAY_SIZE(gShelterB2ElevatorSpriteViews) };

DirectionWarpEntry gShelterB2ElevatorWarps[1] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 0x2CEA, 0, -496 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 0x2CEA, 0, -496 }, { 0, 0, 0, 0 }, SHELTER_B2_ELEVATOR_WARP_ARRIVAL_SOUND, SHELTER_B2_ELEVATOR_WARP_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, SHELTER_B2_ELEVATOR_ENTRY_VIEW, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-axis unit normals for the cab's collision box, with ONE per unit.
static SVECTOR _gShelterB2ElevatorCollisionGridNormals[6] = {
#include "assets/shelter_b2_elevator_collision_00B24_normals.inc"
};

/// Eight corners of the cab's collision box in whole room-coordinate units.
static SVECTOR _gShelterB2ElevatorCollisionGridVerts[8] = {
#include "assets/shelter_b2_elevator_collision_00B24_verts.inc"
};

/// Six quads indexing the collision grid's corner and normal pools.
static WorldCollisionGridFace _gShelterB2ElevatorCollisionGridFaces[6] = {
#include "assets/shelter_b2_elevator_collision_00B24_faces.inc"
};

/// Face-index storage for the grid cell, including its -1 list end.
///
/// The final word follows the end marker and has no established interpretation.
static s16 _gShelterB2ElevatorCollisionGridCells[8] = {
#include "assets/shelter_b2_elevator_collision_00B24_cells.inc"
};

/// Resolves a generated halfword offset to the elevator cell's face-index list.
///
/// Captures `_gShelterB2ElevatorCollisionGridCells`, returning a borrowed `s16*`.
/// `halfwordOffset` is evaluated once; this initializer requires the constant 0
/// selecting the six face indices followed by `WORLD_COLLISION_GRID_CELL_END`.
/// The grid's `cell_face_list_macro` manifest entry selects this binding for its
/// table include. Keep the pool live while the grid is active.
#define SHELTER_B2_ELEVATOR_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB2ElevatorCollisionGridCells[(halfwordOffset)])

/// X-major directory of the cab grid's single face-index list.
static s16* _gShelterB2ElevatorCollisionGridTable[1] = {
#include "assets/shelter_b2_elevator_collision_00B24_table.inc"
};
#undef SHELTER_B2_ELEVATOR_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB2ElevatorCollisionGrid = { NULL, _gShelterB2ElevatorCollisionGridNormals, _gShelterB2ElevatorCollisionGridVerts, _gShelterB2ElevatorCollisionGridFaces, _gShelterB2ElevatorCollisionGridTable, -0x2AF8, 1450, 1, 1, 4000, ARRAY_SIZE(_gShelterB2ElevatorCollisionGridFaces) };

ViewCamera gShelterB2ElevatorViewCameras[15] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -0x2EE0, 0x7530, 450 } }, 2748 },
    { { { { -496, 0, 4065 }, { 3586, 1929, 438 }, { -1914, 3613, -233 } }, { -0x32B8, 2954, 417 } }, 207 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
    { { { { -758, 0, 4025 }, { 230, 4089, 43 }, { -4018, 234, -757 } }, { -0x32AC, 1398, 78 } }, 289 },
};

/// View 1's empty sprite list; the zero first count keeps decoded image strips.
static SpriteBatch _gShelterB2ElevatorView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's depth-sorted textured rectangles in screen pixels and unscaled OT depth.
///
/// The complete source array is selected by its one live batch, after the empty
/// background selector. Texture pages 142/143 use the view image's CLUT 0x3FC0;
/// RGB 128 is neutral modulation and code flags 0 select opaque emission.
static SpriteSource _gShelterB2ElevatorView2SpriteSources[56] = {
    { SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 56, 24 } }, 104, -24, 0, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 40, 24 } }, 120, -48, 0, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, -64, 0, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 152, -80, 0, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 40, 120 } }, -8, -120, 0, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 80 } }, -16, -120, 0, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 88 } }, 32, -120, 0, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 64 } }, 40, -120, 0, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 32 } }, 48, -120, 0, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 64 } }, -120, -64, 0, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 64 } }, -112, -64, 0, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 56 } }, -104, -56, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 48 } }, -96, -48, 0, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 32 } }, -88, -32, 0, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, -16, 0, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -16, -16, 0, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 16 } }, 32, -16, 0, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 48, 40 } }, 112, -120, 461, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 56, 40 } }, 56, -120, 519, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 48, 16 } }, 104, -80, 484, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 40, 16 } }, 96, -64, 524, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 32, 24 } }, 88, -48, 592, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 32, 24 } }, 72, -24, 638, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 16 } }, 32, -32, 756, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 32, 16 } }, 40, -16, 698, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, -40, 730, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, 40, -56, 683, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, 72, -48, 704, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 16 } }, 72, -64, 535, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 32 } }, 48, -88, 529, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, -80, 540, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -32, -40, 782, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -40, -64, 727, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 40, 32 } }, -56, -120, 550, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 40, 16 } }, -56, -16, 0, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -72, -24, 707, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, -32, 685, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -56, -40, 774, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -72, -48, 691, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 16 } }, -88, -48, 640, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -56, -64, 745, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_LEFT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -80, -64, 673, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 32 } }, -96, -80, 569, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, -80, 531, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 32 } }, -120, -96, 490, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -64, -88, 552, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 32 } }, -80, -96, 558, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 16 } }, -104, -96, 548, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -120, -120, 472, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 24, 24 } }, -96, -120, 524, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 32 } }, -64, -120, 566, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 8, 24 } }, -72, -120, 555, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, -88, 589, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 24 } }, -32, -88, 598, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, -80, 560, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { SHELTER_B2_ELEVATOR_SPRITE_RIGHT_TEXTURE_PAGE, SHELTER_B2_ELEVATOR_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, -80, 581, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
};

/// View 2's background selector, complete source range and batch-list end.
static SpriteBatch _gShelterB2ElevatorView2SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB2ElevatorView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite list; the zero first count keeps decoded image strips.
static SpriteBatch _gShelterB2ElevatorView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB2ElevatorSpriteViews[3] = {
    { { .empty = _gShelterB2ElevatorView1SpriteBatches }, _gShelterB2ElevatorView1SpriteBatches, NULL },
    { { .elements = _gShelterB2ElevatorView2SpriteSources }, _gShelterB2ElevatorView2SpriteBatches, NULL },
    { { .empty = _gShelterB2ElevatorView3SpriteBatches }, _gShelterB2ElevatorView3SpriteBatches, NULL },
};

/// The cab's point light, in world units with 12-fractional-bit RGB intensity.
///
/// Contributes in every view, at full strength within 679 units and falling to
/// zero at 5240 units. Room setup binds its parent; lighting queries reuse its
/// composition cache and attenuation, so the record remains mutable.
static WorldCoordPointLight _gShelterB2ElevatorPointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2FD1, -1742, -381 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2950, 3487, 3402 }, { 0, 0 } }, 679, 5240 },
};

WorldCoordRoomLights gShelterB2ElevatorRoomLights = { 0, NULL, ARRAY_SIZE(_gShelterB2ElevatorPointLights), _gShelterB2ElevatorPointLights, 0, NULL };

WorldCollisionTrigger gShelterB2ElevatorViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { 3454, -1167, 45, 0 }, { { -10, -1520, 2263, 0 }, { 11, -1520, -2263, 0 }, { -10, 1520, 2263, 0 }, { 11, 1520, -2263, 0 } }, { -4099, 0, -20, 0 }, { 0, 0, ONE, 0 }, 2721, 0, SHELTER_B2_ELEVATOR_SCENE_VIEW, SHELTER_B2_ELEVATOR_ENTRY_VIEW, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3311, -1152, 47, 0 }, { { 27, -1520, -2262, 0 }, { -26, -1520, 2263, 0 }, { 27, 1520, -2262, 0 }, { -26, 1520, 2263, 0 } }, { 4095, 0, 47, 0 }, { 0, 0, ONE, 0 }, 2721, 0, SHELTER_B2_ELEVATOR_ENTRY_VIEW, SHELTER_B2_ELEVATOR_SCENE_VIEW, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger gShelterB2ElevatorActionTriggers[1] = {
    { NULL, NULL, NULL, { 0x2BF0, -48, -448, 0 }, { { -336, 0, -1024, 0 }, { 336, 0, -1024, 0 }, { -336, 0, 1024, 0 }, { 336, 0, 1024, 0 } }, { 0, 4100, 0, 0 }, { ONE, 0, 0, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, SHELTER_B2_ELEVATOR_HALL_WARP_KEY, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant-1 loading recipe for the B2 elevator scene library.
///
/// Entry 101 loads file 142900 (base 10 plus decimal file number 429).
/// The paired placement list contains only `AREA_PLACEMENT_END`, so loading
/// queues the library but area spawning creates no actor. Descriptor zero is
/// retained for the resource recipe. The final entry ends the resource scan;
/// keep the room and actor library loaded while their records are borrowed.
static AreaResource _gShelterB2ElevatorVariant1Resources[2] = {
    { .entryId = 101, .fileNumber = 429, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskTable = gActor142900TaskTable },
    { .entryId = AREA_PLACEMENT_END },
};

/// Empty placement list for variant 1, whose resource list still loads the scene actor.
static AreaPlacement _gShelterB2ElevatorVariant1Placements[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB2ElevatorAreaVariants[SHELTER_B2_ELEVATOR_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = _gShelterB2ElevatorVariant1Placements, .resources = _gShelterB2ElevatorVariant1Resources },
};

/// Three retained type-1 sound requests with no established reader or record type.
static s32 D_shelter_b2_elevator_8017E9BC[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

/// Silent blocking surface policy shared by classes 0 and 2..7.
static WorldCollisionSurfaceProperties _gShelterB2ElevatorDefaultSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// The floor's class-1 policy, with the same blocking behavior and no footstep sounds.
static WorldCollisionSurfaceProperties _gShelterB2ElevatorSurfaceClass1 = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

WorldCollisionSurfaceProperties* gShelterB2ElevatorSurfacePropertiesByClass[8] = {
    &_gShelterB2ElevatorDefaultSurface,
    &_gShelterB2ElevatorSurfaceClass1,
    &_gShelterB2ElevatorDefaultSurface,
    &_gShelterB2ElevatorDefaultSurface,
    &_gShelterB2ElevatorDefaultSurface,
    &_gShelterB2ElevatorDefaultSurface,
    &_gShelterB2ElevatorDefaultSurface,
    &_gShelterB2ElevatorDefaultSurface,
};

AreaApplyRec gShelterB2ElevatorSceneSavedAreaUpdates[2] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, SHELTER_B2_ELEVATOR_UPPER_SEWER_LAYOUT, AREA_APPLY_MODE_ALWAYS | SHELTER_B2_ELEVATOR_SET_MAP_MARK },
    { AREA_APPLY_END, 0, 0, 0 },
};

/// Spawns a door leaf at rest or the room's exit task on the selected task list.
///
/// `descriptorIndex` is 0 or 1 for a leaf, or 2 for the exit task; it is unchecked.
/// `doorSide` is the leaf's signed Z direction (-1 or 1), ignored by the exit
/// task. The new task owns its body and work; returns NULL on spawn failure.
/// Keep this room's callback code and model geometry loaded while tasks use them.
static inline Task* _shelterB2ElevatorSpawnTask(s32 descriptorIndex, s32 doorSide)
{
    return taskSpawnFromTable(_gShelterB2ElevatorTaskDescriptors, descriptorIndex, SHELTER_B2_ELEVATOR_DOOR_REST, doorSide);
}

/// Initializes the elevator's room receiver, door leaves and entry sequence.
///
/// Runs in state 0 and advances to idle. Publishes the receiver in
/// `GAME_TASK_SLOT_ROOM` and retains both leaf spawn results without checking
/// failure. Demo 9 skips story playback; other entries run the first-visit
/// script when the elevator-access flag is clear, otherwise hold the HUD/event
/// state and request an exit task with CAP command 3. Keep this room and its
/// actor scripts loaded while their tasks run. The task body and payloads are unused.
static void _shelterB2ElevatorRoomInitState(Task* task)
{
    enum {
        SHELTER_B2_ELEVATOR_SETUP_ONLY_DEMO        = 9,
        SHELTER_B2_ELEVATOR_FIRST_VISIT_RECORDED   = 1,
        SHELTER_B2_ELEVATOR_OBJECTIVE_AFTER_ENTRY  = 0x24,
        SHELTER_B2_ELEVATOR_CAP_SELECT_DESTINATION = 3,
        SHELTER_B2_ELEVATOR_HUD_HIDDEN             = 1,
        SHELTER_B2_ELEVATOR_EVENT_ACTIVE           = 1,
    };

    task->msgTable = _gShelterB2ElevatorRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gShelterB2ElevatorDoorLeafTasks[SHELTER_B2_ELEVATOR_TASK_NEGATIVE_Z_LEAF] = _shelterB2ElevatorSpawnTask(SHELTER_B2_ELEVATOR_TASK_NEGATIVE_Z_LEAF, -1);
    _gShelterB2ElevatorDoorLeafTasks[SHELTER_B2_ELEVATOR_TASK_POSITIVE_Z_LEAF] = _shelterB2ElevatorSpawnTask(SHELTER_B2_ELEVATOR_TASK_POSITIVE_Z_LEAF, 1);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != SHELTER_B2_ELEVATOR_SETUP_ONLY_DEMO) {
        if (gameFlagGetNibble(GAME_FLAG_0CF) == 0) {
            gameFlagSetNibble(GAME_FLAG_0CF, SHELTER_B2_ELEVATOR_FIRST_VISIT_RECORDED);
            evsStartScriptWithSkip(gActor142900ElevatorFirstVisitScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor142900ElevatorFirstVisitSkipScript);
            gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, SHELTER_B2_ELEVATOR_OBJECTIVE_AFTER_ENTRY);
        } else {
            gGameSession->hideHud    = SHELTER_B2_ELEVATOR_HUD_HIDDEN;
            gGameSession->eventState = SHELTER_B2_ELEVATOR_EVENT_ACTIVE;
            _shelterB2ElevatorSpawnTask(SHELTER_B2_ELEVATOR_TASK_EXIT, 0);
            capRunCommand(SHELTER_B2_ELEVATOR_CAP_SELECT_DESTINATION, CAP_PLAYBACK_IN_PLACE);
        }
    }
    task->state++;
}

/// Steps a leaf's bounded travel and places it on its signed side of the doorway.
///
/// Borrows the live leaf task, its travel storage and its model's root coordinate.
/// `travel` is 0..500 world units; `spawnArg1.value` requests closing (-1), rest
/// (0) or opening (1) at 10 units per tick, and `spawnArg2.value` is the Z side
/// (-1 or 1). Clamping leaves the request intact at either end of the travel.
/// Only travel and root Z change; the caller must dirty the coordinate cache.
static inline void _shelterB2ElevatorSlideDoorLeaf(Task* task, _ShelterB2ElevatorDoorLeafWork* leafWork, GfxCoord* modelCoord)
{
    leafWork->travel += task->spawnArg1.value * SHELTER_B2_ELEVATOR_DOOR_LEAF_SPEED;
    if (leafWork->travel < 0) {
        leafWork->travel = 0;
    }
    if (leafWork->travel > SHELTER_B2_ELEVATOR_DOOR_LEAF_TRAVEL_MAX) {
        leafWork->travel = SHELTER_B2_ELEVATOR_DOOR_LEAF_TRAVEL_MAX;
    }
    modelCoord->coord.t[2] = (leafWork->travel * task->spawnArg2.value) + SHELTER_B2_ELEVATOR_DOOR_CLOSED_Z;
}

/// Maintains one mirrored leaf of the elevator's sliding door.
///
/// Requires a live TMD body. State 0 places it shut and allocates task-owned
/// travel storage; failure kills the task. State 1 slides in whole world units
/// and refreshes lighting, drawing only in the doorway view. `spawnArg1` selects
/// opening (1), closing (-1) or rest (0); `spawnArg2` selects the Z side (-1 or 1).
/// The view coordinate and room geometry must remain loaded until task teardown.
static void _shelterB2ElevatorDoorLeafTask(Task* task)
{
    enum {
        SHELTER_B2_ELEVATOR_DOOR_LEAF_INIT   = 0,
        SHELTER_B2_ELEVATOR_DOOR_LEAF_UPDATE = 1,
        SHELTER_B2_ELEVATOR_DOORWAY_VIEW     = 2,
        SHELTER_B2_ELEVATOR_DOOR_X           = 10900,
        SHELTER_B2_ELEVATOR_DOOR_OT_OFFSET   = 100,
        SHELTER_B2_ELEVATOR_DOOR_LIGHT_COUNT = 3,
    };

    TmdObject*                      model;
    GfxCoord*                       modelCoord;
    _ShelterB2ElevatorDoorLeafWork* leafWork;
    VECTOR3                         worldPosition;

    model      = task->extra.tmd;
    modelCoord = model->coords;
    switch (task->state) {
        case SHELTER_B2_ELEVATOR_DOOR_LEAF_INIT:
            leafWork = memCalloc(sizeof(*leafWork), 0);
            if (leafWork == NULL) {
                taskKill(task);
                return;
            }
            task->work               = leafWork;
            leafWork->travel         = 0;
            model->otOffset          = SHELTER_B2_ELEVATOR_DOOR_OT_OFFSET;
            model->flags             = 0;
            modelCoord->parent       = &gGfxViewCoord;
            modelCoord->coord.t[0]   = SHELTER_B2_ELEVATOR_DOOR_X;
            modelCoord->coord.t[1]   = 0;
            modelCoord->coord.t[2]   = SHELTER_B2_ELEVATOR_DOOR_CLOSED_Z;
            modelCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state++;
            break;
        case SHELTER_B2_ELEVATOR_DOOR_LEAF_UPDATE:
            // Opposite side signs retract the two leaves from the same shut Z.
            leafWork = task->work;
            _shelterB2ElevatorSlideDoorLeaf(task, leafWork, modelCoord);
            if (gGameSession->location.view == SHELTER_B2_ELEVATOR_DOORWAY_VIEW) {
                model->flags = 0;
            } else {
                model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            // Lighting samples the composed position, including the view parent.
            modelCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(modelCoord);
            worldPosition.vx = modelCoord->workm.t[0];
            worldPosition.vy = modelCoord->workm.t[1];
            worldPosition.vz = modelCoord->workm.t[2];
            worldCoordSetModelLighting(model, &worldPosition, 0, SHELTER_B2_ELEVATOR_DOOR_LIGHT_COUNT);
            break;
    }
}

/// Room-controller states 0..2: initialize, idle, release the task.
///
/// Copied as a twelve-byte callback value before unchecked state dispatch;
/// contains no terminal entry. Callback code must stay loaded through the call.
static const TaskFuncTable3 _gShelterB2ElevatorRoomStates = {
    { _shelterB2ElevatorRoomInitState, _shelterB2ElevatorRoomIdleState, taskKill },
};

/// Resolves the saved elevator destination and requests a captured-frame reload.
///
/// Requires the live save, display state, sound queue and loaded Shelter map overlay.
/// Stops all nonambient scripts without fading, resolves the saved destination
/// and selects sprite variant 1. Only area, warp, room and the execute selector
/// are initialized in the eight-byte messages; the resolver reads those fields
/// and changes only room. Saved area is retained and warp/room are written back.
/// Selectors must identify valid destination records. Queues a captured-frame
/// reload without waiting for completion or checking spawn admission.
static inline void _shelterB2ElevatorRequestExitReload(void)
{
    enum { SHELTER_B2_ELEVATOR_EXIT_SPRITE_VARIANT = 1 };
    RoomEventMsg transitionRequest;
    RoomEventMsg resolvedDestination;

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    transitionRequest.queryOnly = ROOM_EVENT_EXECUTE;
    transitionRequest.areaId    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area;
    transitionRequest.warp      = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp;
    transitionRequest.roomState = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState;
    resolvedDestination         = transitionRequest;
    mapShelterRoomVariantResolve(&transitionRequest, &resolvedDestination);
    gDisplayState.spriteVariant                                 = SHELTER_B2_ELEVATOR_EXIT_SPRITE_VARIANT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = resolvedDestination.warp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = resolvedDestination.roomState;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Transfers from the elevator to the floor chosen by its CAP dialogue.
///
/// Start in state 0 with a zero countdown. Holds player control until the
/// pre-increment countdown reaches 21, then waits for CAP to become idle.
/// Keys 0xB/0xC/0xD select B1/B2/B3; other keys retain the saved destination.
/// After two further state advances, resolves the saved room variant and
/// requests a captured-frame reload before killing this task. Body and payloads
/// are unused; the room and map overlay must remain loaded until the request.
static void _shelterB2ElevatorExitTask(Task* task)
{
    enum {
        SHELTER_B2_ELEVATOR_EXIT_HOLD          = 0,
        SHELTER_B2_ELEVATOR_EXIT_WAIT_CAP      = 1,
        SHELTER_B2_ELEVATOR_EXIT_SELECT_FLOOR  = 2,
        SHELTER_B2_ELEVATOR_EXIT_DELAY         = 3,
        SHELTER_B2_ELEVATOR_EXIT_RELOAD        = 4,
        SHELTER_B2_ELEVATOR_EXIT_HOLD_TICKS    = 21,
        SHELTER_B2_ELEVATOR_CAP_DESTINATION_B1 = 0xB,
        SHELTER_B2_ELEVATOR_CAP_DESTINATION_B2 = 0xC,
        SHELTER_B2_ELEVATOR_CAP_DESTINATION_B3 = 0xD,
        SHELTER_B2_ELEVATOR_B1_ARRIVAL_WARP    = 3,
        SHELTER_B2_ELEVATOR_B2_ARRIVAL_WARP    = 2,
        SHELTER_B2_ELEVATOR_B3_ARRIVAL_WARP    = 3,
    };

    switch (task->state) {
        case SHELTER_B2_ELEVATOR_EXIT_HOLD:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            if (task->killCountdown >= SHELTER_B2_ELEVATOR_EXIT_HOLD_TICKS) {
                task->state++;
            }
            task->killCountdown = task->killCountdown + 1;
            break;
        case SHELTER_B2_ELEVATOR_EXIT_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B2_ELEVATOR_EXIT_SELECT_FLOOR:
            switch (capGetVariantKey()) {
                case SHELTER_B2_ELEVATOR_CAP_DESTINATION_B1:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area = GAME_AREA_SHELTER_B1_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = SHELTER_B2_ELEVATOR_B1_ARRIVAL_WARP;
                    break;
                case SHELTER_B2_ELEVATOR_CAP_DESTINATION_B2:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area = GAME_AREA_SHELTER_B2_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = SHELTER_B2_ELEVATOR_B2_ARRIVAL_WARP;
                    break;
                case SHELTER_B2_ELEVATOR_CAP_DESTINATION_B3:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area = GAME_AREA_SHELTER_B3_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = SHELTER_B2_ELEVATOR_B3_ARRIVAL_WARP;
                    break;
            }
            task->state++;
            break;
        case SHELTER_B2_ELEVATOR_EXIT_DELAY:
            task->state++;
            break;
        case SHELTER_B2_ELEVATOR_EXIT_RELOAD:
            // Only the destination selectors are used; retain the partial message initialization.
            _shelterB2ElevatorRequestExitReload();
            taskKill(task);
            break;
    }
}

/// Refuses every inventory key-item request, returning 0 without consuming it.
static s32 _shelterB2ElevatorRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition after resolving its destination from Shelter progress.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows a complete request and writable reply;
/// they may alias. Copies all eight bytes before resolving the reply's room.
/// Query requests preserve that copy. The map overlay must be loaded; neither
/// pointer is retained. Returns 1 even when the destination is unchanged.
static s32 _shelterB2ElevatorResolveRoomEventMessage(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B2_ELEVATOR_ROOM_EVENT_ACCEPTED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_B2_ELEVATOR_ROOM_EVENT_ACCEPTED;
}

/// Ignores `ROOM_MESSAGE_COMMAND` and returns 0 without changing room state.
static s32 _shelterB2ElevatorIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION` and its borrowed request, returning 0.
static s32 _shelterB2ElevatorIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* unusedRequest, s32 unusedArg)
{
    return 0;
}

/// Requests both door leaves to open on subsequent updates and returns 0.
///
/// Both tasks in `_gShelterB2ElevatorDoorLeafTasks` must be live; their initial
/// travel storage is allocated by their own first updates. Payloads are ignored.
static s32 _shelterB2ElevatorOpenDoorMessage(Task* task, s32 messageId, s32 unusedArg1, s32 unusedArg2)
{
    _gShelterB2ElevatorDoorLeafTasks[SHELTER_B2_ELEVATOR_TASK_NEGATIVE_Z_LEAF]->spawnArg1.value = SHELTER_B2_ELEVATOR_DOOR_OPEN;
    _gShelterB2ElevatorDoorLeafTasks[SHELTER_B2_ELEVATOR_TASK_POSITIVE_Z_LEAF]->spawnArg1.value = SHELTER_B2_ELEVATOR_DOOR_OPEN;
    return 0;
}

/// Requests both door leaves to close on subsequent updates and returns 0.
///
/// Both tasks in `_gShelterB2ElevatorDoorLeafTasks` must be live. Payloads are ignored.
static s32 _shelterB2ElevatorCloseDoorMessage(Task* task, s32 messageId, s32 unusedArg1, s32 unusedArg2)
{
    _gShelterB2ElevatorDoorLeafTasks[SHELTER_B2_ELEVATOR_TASK_NEGATIVE_Z_LEAF]->spawnArg1.value = SHELTER_B2_ELEVATOR_DOOR_CLOSE;
    _gShelterB2ElevatorDoorLeafTasks[SHELTER_B2_ELEVATOR_TASK_POSITIVE_Z_LEAF]->spawnArg1.value = SHELTER_B2_ELEVATOR_DOOR_CLOSE;
    return 0;
}

/// Keeps the initialized room task idle without changing its state or resources.
static void _shelterB2ElevatorRoomIdleState(Task* unusedTask)
{
    // Preserve the original 16-byte idle frame; no stack bytes are accessed.
    char unusedFrame[0x10];
}

void shelterB2ElevatorRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterB2ElevatorRoomStates;
    stateHandlers.funcs[task->state](task);
}

void shelterB2ElevatorEffectNoopTask(Task* unusedTask)
{
}
