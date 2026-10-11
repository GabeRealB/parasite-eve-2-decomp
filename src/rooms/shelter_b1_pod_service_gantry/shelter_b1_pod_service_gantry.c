#include "rooms/shelter_b1_pod_service_gantry.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "shelter_b1_pod_service_gantry_private.h"

#include "rooms/shelter_b1_pod_service_gantry_light_types.h"

#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "actors/actor_160900.h"
#include "actors/actor_560800.h"

#include "mapui/map_shelter.h"

/// Steps of the room task's running state, held in
/// `_ShelterB1PodServiceGantryWork::step`. Each advances to the next; the last
/// one idles while the area transition takes the room down.
enum {
    SHELTER_B1_POD_SERVICE_GANTRY_STEP_START_FIRST_SCENE  = 0, // Spawn the first scene task
    SHELTER_B1_POD_SERVICE_GANTRY_STEP_AWAIT_FIRST_SCENE  = 1, // Reap it once it asks to stop, then spawn its follow-up task
    SHELTER_B1_POD_SERVICE_GANTRY_STEP_PAUSE              = 2, // One frame with nothing to do
    SHELTER_B1_POD_SERVICE_GANTRY_STEP_LOAD_SECOND_SCENE  = 3, // Queue the load of the second scene's actor package
    SHELTER_B1_POD_SERVICE_GANTRY_STEP_START_SECOND_SCENE = 4, // Once the CD queue is idle, spawn the second scene task
    SHELTER_B1_POD_SERVICE_GANTRY_STEP_AWAIT_SECOND_SCENE = 5, // Reap it once it asks to stop, then leave for the pod access tunnel
    SHELTER_B1_POD_SERVICE_GANTRY_STEP_LEAVING            = 6, // Nothing further; the transition is under way
};

/// Work block of the room task, which plays two scenes back to back and then
/// leaves for the pod access tunnel. The sequence starts unconditionally on
/// the task's first running frame.
///
/// The task's first state allocates it zeroed and the default teardown frees
/// it. The scene tasks are spawned unparented, so this pointer is the only
/// link to the one being waited for.
typedef struct {
    Task* sceneTask; // Task the current step spawned; borrowed, and stale once that task has been reaped
    s16   step;      // Position in the sequence (SHELTER_B1_POD_SERVICE_GANTRY_STEP_*)
} _ShelterB1PodServiceGantryWork;
STATIC_ASSERT_SIZEOF(_ShelterB1PodServiceGantryWork, 8);

/// The room's message table, published in `Task::msgTable`.
extern TaskMessageEntry D_shelter_b1_pod_service_gantry_8017FAF4[];

static void _shelterB1PodServiceGantryRunSceneSequence(Task* task);
static void _shelterB1PodServiceGantryInitRoomState(Task* task);

/// Room-controller states 0..2: initialize scene work, run the two-scene sequence, teardown.
///
/// The controller copies all three callbacks before unchecked state dispatch.
/// No terminator; keep the room overlay loaded while the controller is live.
static const TaskFuncTable3 _gShelterB1PodServiceGantryStateHandlers = {
    { _shelterB1PodServiceGantryInitRoomState, _shelterB1PodServiceGantryRunSceneSequence, taskKill },
};

static s32 _shelterB1PodServiceGantryRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static s32 _shelterB1PodServiceGantryResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB1PodServiceGantryIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg);
static s32 _shelterB1PodServiceGantryIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

TaskMessageEntry D_shelter_b1_pod_service_gantry_8017FAF4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1PodServiceGantryResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1PodServiceGantryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1PodServiceGantryIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1PodServiceGantryIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* gShelterB1PodServiceGantryRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB1PodServiceGantryRoomViewCounts[1] = { 46 };

DirectionWarpEntry gShelterB1PodServiceGantryWarpEntries[1] = {
    {
        .player         = { { .word = 1024 }, 2535, 0, 3220 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, 2535, 0, 3220 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Placement-entry keys, decimal file numbers and table selectors for these layouts.
enum {
    SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_NO9           = 34,
    SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_KYLE          = 101,
    SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_EVE           = 131,
    SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_CHAIN_CARRIER = 59,
    SHELTER_B1_POD_SERVICE_GANTRY_FILE_CAST              = 608,
    SHELTER_B1_POD_SERVICE_GANTRY_DISCARD_TASK           = 1,
};

/// Cast-loading recipes for the default gantry placement variant (1).
///
/// Key 34 loads file 560800; keys 101, 131 and 59 select additional texture
/// files 660801..660804. No placement requests the base-60 file 660800.
/// Every entry spawns bodyless discard descriptor 1: the cutscene controller
/// creates the cast separately, while placements supply texture relocations.
/// Paired with `_gShelterB1PodServiceGantryDefaultPlacements`; AREA_PLACEMENT_END ends the list.
/// Retain room recipes, placements and actor descriptors through loading,
/// spawning and model-flag updates.
static AreaResource _gShelterB1PodServiceGantryDefaultResources[5] = {
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_NO9, .fileNumber = SHELTER_B1_POD_SERVICE_GANTRY_FILE_CAST, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50, .taskIndex = SHELTER_B1_POD_SERVICE_GANTRY_DISCARD_TASK, .taskTable = gActor560800CutsceneTaskDescriptors },
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_KYLE, .fileNumber = SHELTER_B1_POD_SERVICE_GANTRY_FILE_CAST, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = SHELTER_B1_POD_SERVICE_GANTRY_DISCARD_TASK, .taskTable = gActor560800CutsceneTaskDescriptors },
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_EVE, .fileNumber = SHELTER_B1_POD_SERVICE_GANTRY_FILE_CAST, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = SHELTER_B1_POD_SERVICE_GANTRY_DISCARD_TASK, .taskTable = gActor560800CutsceneTaskDescriptors },
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_CHAIN_CARRIER, .fileNumber = SHELTER_B1_POD_SERVICE_GANTRY_FILE_CAST, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = SHELTER_B1_POD_SERVICE_GANTRY_DISCARD_TASK, .taskTable = gActor560800CutsceneTaskDescriptors },
    { .entryId = AREA_PLACEMENT_END },
};

/// Default gantry layout (variant 1): five cast-loading placeholders.
///
/// The base No. 9 file and Eve/Kyle/chain-carrier/Kyle additional files
/// use low file-ID components 0, 1, 2, 3 and 4. Coordinates and yaw are zero;
/// the cutscene places its own cast. Signed page offsets and CLUT rows select
/// each loaded texture location. Descriptor 1 immediately discards the tasks.
/// Keys match `_gShelterB1PodServiceGantryDefaultResources`; AREA_PLACEMENT_END ends the list.
/// Texture-page offsets count 64-word VRAM columns and CLUT offsets count rows.
/// Retain this room storage through loading and cast texture binding.
static AreaPlacement _gShelterB1PodServiceGantryDefaultPlacements[6] = {
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_NO9, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = -1, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_EVE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 1, .texturePageOffset = 2, .clutRowOffset = 8, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_KYLE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 2, .texturePageOffset = 4, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_CHAIN_CARRIER, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 3, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
    { .entryId = SHELTER_B1_POD_SERVICE_GANTRY_RESOURCE_KYLE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 4, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB1PodServiceGantryAreaVariants[SHELTER_B1_POD_SERVICE_GANTRY_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = _gShelterB1PodServiceGantryDefaultPlacements, .resources = _gShelterB1PodServiceGantryDefaultResources },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gShelterB1PodServiceGantryRoomCollisionNormals[13] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_normals.inc"
};

/// Collision vertices in signed room-coordinate game units.
static SVECTOR _gShelterB1PodServiceGantryRoomCollisionVerts[66] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_verts.inc"
};

/// Indexed collision faces selecting the room's vertex, normal and surface pools.
static WorldCollisionGridFace _gShelterB1PodServiceGantryRoomCollisionFaces[30] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_faces.inc"
};

/// Face-index lists for nonempty grid cells, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB1PodServiceGantryRoomCollisionCells[172] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_cells.inc"
};

// The generated table supplies zero-based element offsets into the cell-list pool.
#define GRID_CELL(i) (&_gShelterB1PodServiceGantryRoomCollisionCells[i])
/// X-major cell-list pointers: five X rows of three Z cells; NULL marks an empty cell.
static s16* _gShelterB1PodServiceGantryRoomCollisionTable[15] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gShelterB1PodServiceGantryRoomCollision = {
    .viewCoord   = NULL,
    .normals     = _gShelterB1PodServiceGantryRoomCollisionNormals,
    .vertices    = _gShelterB1PodServiceGantryRoomCollisionVerts,
    .faces       = _gShelterB1PodServiceGantryRoomCollisionFaces,
    .cellFaceIds = _gShelterB1PodServiceGantryRoomCollisionTable,
    .xBias       = 1003,
    .zBias       = -1010,
    .cellCountX  = 5,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB1PodServiceGantryRoomCollisionFaces),
};

ViewCamera gShelterB1PodServiceGantryViewCameras[46] = {
    { .transform = { .m = { { 1403, 0, 3848 }, { 2757, 2857, -1005 }, { -2684, 2934, 978 } }, .t = { -0x2DC3, 4293, -2180 } }, .screenDistance = 207 },
    { .transform = { .m = { { 693, 0, 4036 }, { 392, 4076, -67 }, { -4017, 398, 690 } }, .t = { -7410, 1510, -2610 } }, .screenDistance = 289 },
    { .transform = { .m = { { 1395, 0, -3850 }, { -456, 4067, -165 }, { 3823, 485, 1386 } }, .t = { -2490, 1520, -2540 } }, .screenDistance = 207 },
    { .transform = { .m = { { -3621, 0, 1914 }, { 765, 3754, 1447 }, { -1754, 1637, -3319 } }, .t = { -0x3D54, 3550, -8260 } }, .screenDistance = 289 },
    { .transform = { .m = { { 3965, 0, 1025 }, { -3, 4095, 14 }, { -1025, -15, 3965 } }, .t = { -0x3F7A, 2500, -1160 } }, .screenDistance = 289 },
    { .transform = { .m = { { -4017, 0, -797 }, { -627, 2530, 3159 }, { 492, 3221, -2481 } }, .t = { -6717, 2637, -5072 } }, .screenDistance = 329 },
    { .transform = { .m = { { 4089, 0, -229 }, { 137, 3281, 2447 }, { 183, -2450, 3276 } }, .t = { -6550, 170, -1790 } }, .screenDistance = 143 },
    { .transform = { .m = { { -ONE, 0, 0 }, { 0, 2262, 3414 }, { 0, 3414, -2262 } }, .t = { -7000, 6570, -9340 } }, .screenDistance = 289 },
    { .transform = { .m = { { -4095, 0, 0 }, { 0, 2763, 3023 }, { 0, 3023, -2763 } }, .t = { -7000, 2930, -6100 } }, .screenDistance = 289 },
    { .transform = { .m = { { 3908, 0, 1224 }, { 276, 3990, -882 }, { -1193, 924, 3807 } }, .t = { -8490, 2150, -770 } }, .screenDistance = 289 },
    { .transform = { .m = { { -1888, 0, 3634 }, { -233, 4087, -121 }, { -3627, -263, -1884 } }, .t = { -9400, 1470, -5110 } }, .screenDistance = 541 },
    { .transform = { .m = { { 4095, 0, 0 }, { 0, 3947, 1094 }, { 0, -1094, 3947 } }, .t = { -7000, 1530, -4260 } }, .screenDistance = 289 },
    { .transform = { .m = { { -2903, 0, -2888 }, { -254, 4080, 256 }, { 2877, 361, -2892 } }, .t = { -0x389A, 2490, -8990 } }, .screenDistance = 289 },
    { .transform = { .m = { { 0, 0, ONE }, { 1366, 3861, 0 }, { -3861, 1366, 0 } }, .t = { -0x4538, 2880, -6500 } }, .screenDistance = 289 },
    { .transform = { .m = { { 4052, 0, -595 }, { 68, 4068, 468 }, { 591, -473, 4025 } }, .t = { -0x3A98, 2050, -5600 } }, .screenDistance = 541 },
    { .transform = { .m = { { 2083, 0, 3526 }, { 535, 4048, -316 }, { -3485, 621, 2058 } }, .t = { -390, 1520, 3400 } }, .screenDistance = 282 },
    { .transform = { .m = { { 664, 0, 4041 }, { 178, 4091, -29 }, { -4037, 181, 664 } }, .t = { -1310, 1370, 2360 } }, .screenDistance = 275 },
    { .transform = { .m = { { 862, 0, 4004 }, { 1204, 3906, -259 }, { -3818, 1231, 822 } }, .t = { -6593, 1887, 2833 } }, .screenDistance = 246 },
    { .transform = { .m = { { -3915, 0, -1201 }, { -30, 4094, 99 }, { 1201, 103, -3914 } }, .t = { -7057, 873, -0x39AE } }, .screenDistance = 269 },
    { .transform = { .m = { { -458, 0, 4070 }, { 2704, 3060, 304 }, { -3041, 2722, -342 } }, .t = { -5900, 5070, -8460 } }, .screenDistance = 289 },
    { .transform = { .m = { { 0, 0, -ONE }, { -343, 4081, 0 }, { 4081, 343, 0 } }, .t = { 1531, 1760, -3200 } }, .screenDistance = 853 },
    { .transform = { .m = { { -4046, 0, -632 }, { 58, 4078, -373 }, { 629, -377, -4029 } }, .t = { -8150, 1150, -5100 } }, .screenDistance = 289 },
    { .transform = { .m = { { -416, 0, 4074 }, { -259, 4087, -26 }, { -4066, -260, -415 } }, .t = { -9288, 1352, -3496 } }, .screenDistance = 541 },
    { .transform = { .m = { { -331, 0, -4082 }, { 653, 4043, -53 }, { 4029, -655, -327 } }, .t = { -3800, 800, -3800 } }, .screenDistance = 680 },
    { .transform = { .m = { { 2885, 0, 2906 }, { 102, 4093, -101 }, { -2905, 143, 2883 } }, .t = { -7132, 1446, -1740 } }, .screenDistance = 447 },
    { .transform = { .m = { { 3776, 0, 1585 }, { 432, 3940, -1031 }, { -1525, 1118, 3633 } }, .t = { -8348, 1907, -27 } }, .screenDistance = 369 },
    { .transform = { .m = { { 115, 0, -4094 }, { 87, 4095, 2 }, { 4093, -87, 115 } }, .t = { -6000, 1508, -3300 } }, .screenDistance = 447 },
    { .transform = { .m = { { 2175, 0, 3470 }, { 1288, 3803, -807 }, { -3222, 1520, 2020 } }, .t = { -9356, 2096, -1713 } }, .screenDistance = 447 },
    { .transform = { .m = { { -1119, 0, -3940 }, { 554, 4055, -157 }, { 3900, -576, -1108 } }, .t = { -5100, 1200, -3750 } }, .screenDistance = 312 },
    { .transform = { .m = { { -394, 0, -4076 }, { 954, 3982, -92 }, { 3963, -958, -383 } }, .t = { -4850, 800, -3750 } }, .screenDistance = 380 },
    { .transform = { .m = { { 3594, 0, -1963 }, { -1509, 2619, -2763 }, { 1255, 3148, 2299 } }, .t = { -5820, 4480, -190 } }, .screenDistance = 447 },
    { .transform = { .m = { { 3745, 0, 1658 }, { 62, 4093, -140 }, { -1657, 153, 3742 } }, .t = { -8710, 1080, 430 } }, .screenDistance = 348 },
    { .transform = { .m = { { -995, 0, 3973 }, { 105, 4094, 26 }, { -3971, 108, -995 } }, .t = { -0x2C69, 998, -3892 } }, .screenDistance = 358 },
    { .transform = { .m = { { 2031, 0, 3556 }, { 1299, 3812, -742 }, { -3310, 1496, 1890 } }, .t = { -7162, 1426, -2106 } }, .screenDistance = 447 },
    { .transform = { .m = { { -3914, 0, 1205 }, { 1172, 961, 3805 }, { -283, 3981, -918 } }, .t = { -7360, 9500, -6160 } }, .screenDistance = 257 },
    { .transform = { .m = { { -3916, 0, 1198 }, { 1165, 960, 3807 }, { -281, 3981, -918 } }, .t = { -7364, 9503, -6166 } }, .screenDistance = 257 },
    { .transform = { .m = { { -ONE, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, .t = { -7000, 9483, -6180 } }, .screenDistance = 418 },
    { .transform = { .m = { { -ONE, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, .t = { -7000, 9483, -6180 } }, .screenDistance = 418 },
    { .transform = { .m = { { -ONE, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, .t = { -7000, 9483, -6180 } }, .screenDistance = 418 },
    { .transform = { .m = { { -ONE, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, .t = { -7000, 9483, -6180 } }, .screenDistance = 418 },
    { .transform = { .m = { { -ONE, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, .t = { -7000, 9483, -6180 } }, .screenDistance = 418 },
    { .transform = { .m = { { 606, 0, 4050 }, { 30, 4095, -4 }, { -4050, 30, 606 } }, .t = { -9523, 1028, -2600 } }, .screenDistance = 447 },
    { .transform = { .m = { { 1999, 0, 3574 }, { 187, 4090, -104 }, { -3569, 214, 1996 } }, .t = { -9422, 777, -982 } }, .screenDistance = 329 },
    { .transform = { .m = { { 2809, 0, 2980 }, { 1285, 3695, -1212 }, { -2688, 1766, 2535 } }, .t = { -0x3412, 2770, -6570 } }, .screenDistance = 289 },
    { .transform = { .m = { { 1178, 0, -3922 }, { 529, 4058, 158 }, { 3887, -552, 1167 } }, .t = { -1433, 500, -1642 } }, .screenDistance = 380 },
    { .transform = { .m = { { 1676, 0, -3737 }, { -1100, 3914, -493 }, { 3571, 1205, 1601 } }, .t = { -7948, 590, -2568 } }, .screenDistance = 329 },
};

// Room sprite atlas: 8-bit texture pages at Y=0, palette at (0, 255).
enum {
    SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896 = getTPage(1, 0, 896, 0),
    SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960 = getTPage(1, 0, 960, 0),
    SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT              = getClut(0, 255),
    SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR     = 128,
};

/// Texture rectangles and sorting depths for pod service gantry view 1.
///
/// This view's batches select all 23 elements. Positions and dimensions
/// are pixels relative to the view centre; depths are unscaled OT sorting
/// values. The room owns these writable sources while its overlay is loaded.
static SpriteSource _gShelterB1PodServiceGantryView1Sources[23] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 72 } }, 128, 48, 386, { .fields = { 96, 72 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 64 } }, 104, 56, 386, { .fields = { 96, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 56 } }, 88, 64, 381, { .fields = { 104, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 48 } }, 64, 72, 376, { .fields = { 80, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 40 } }, 48, 80, 391, { .fields = { 88, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 32 } }, 24, 88, 386, { .fields = { 72, 32 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 0, 96, 380, { .fields = { 48, 32 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 104, 376, { .fields = { 72, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 48 } }, -72, 40, 1250, { .fields = { 112, 208 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 72 } }, -56, 40, 1250, { .fields = { 120, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 64 } }, -48, 56, 1250, { .fields = { 120, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 48 } }, -40, 72, 1250, { .fields = { 104, 200 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 24 } }, -32, 96, 1250, { .fields = { 72, 168 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 24 } }, 16, 32, 1250, { .fields = { 32, 168 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 56, 16 } }, -40, 56, 1250, { .fields = { 8, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 16 } }, 16, 56, 1250, { .fields = { 32, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -32, 72, 1250, { .fields = { 48, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 24 } }, 16, 72, 1250, { .fields = { 64, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -24, 96, 1250, { .fields = { 48, 120 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 24 } }, 48, 8, 1250, { .fields = { 48, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 24 } }, 56, 32, 1250, { .fields = { 56, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 32 } }, 56, 56, 1250, { .fields = { 56, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 64, 32 } }, -48, 24, 1250, { .fields = { 32, 64 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for pod service gantry view 1.
///
/// The zero-count first batch selects decoded background strips.
/// Two sprite ranges cover all 23 source elements; ranges start visible
/// and included in cached drawing. The final two bytes retain unproven
/// serialized values. Storage remains writable while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView1Batches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 8, .field_6 = { 1, 0 } },
    { .firstSprite = 8, .spriteCount = 15, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for pod service gantry view 2.
///
/// This view's batches select all 8 elements. Positions and dimensions
/// are pixels relative to the view centre; depths are unscaled OT sorting
/// values. The room owns these writable sources while its overlay is loaded.
static SpriteSource _gShelterB1PodServiceGantryView2Sources[8] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, -48, 0, 1300, { .fields = { 120, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, -104, 0, 1237, { .fields = { 80, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, -104, -48, 1237, { .fields = { 80, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, -104, -96, 1237, { .fields = { 80, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -48, 1300, { .fields = { 112, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -96, 1300, { .fields = { 112, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 32 } }, -40, -96, 1350, { .fields = { 64, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 48 } }, -56, 0, 1300, { .fields = { 104, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for pod service gantry view 2.
///
/// The zero-count first batch selects decoded background strips.
/// One sprite range covers all 8 source elements; ranges start visible
/// and included in cached drawing. The final two bytes retain unproven
/// serialized values. Storage remains writable while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView2Batches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB1PodServiceGantryView2Sources), .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for pod service gantry view 3.
///
/// This view's batches select all 12 elements. Positions and dimensions
/// are pixels relative to the view centre; depths are unscaled OT sorting
/// values. The room owns these writable sources while its overlay is loaded.
static SpriteSource _gShelterB1PodServiceGantryView3Sources[12] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 0, 2300, { .fields = { 104, 88 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, -40, 2025, { .fields = { 112, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, -56, 2300, { .fields = { 96, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 16 } }, 48, -56, 2125, { .fields = { 88, 72 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, -56, 2015, { .fields = { 96, 56 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 40 } }, 32, -40, 2300, { .fields = { 112, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 72, 737, { .fields = { 80, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 40 } }, 80, 72, 737, { .fields = { 80, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 24, 737, { .fields = { 80, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 48 } }, 96, 24, 737, { .fields = { 112, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 88, 0, 737, { .fields = { 104, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 8, 737, { .fields = { 96, 40 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for pod service gantry view 3.
///
/// The zero-count first batch selects decoded background strips.
/// Two sprite ranges cover all 12 source elements; ranges start visible
/// and included in cached drawing. The final two bytes retain unproven
/// serialized values. Storage remains writable while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView3Batches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 6, .field_6 = { 1, 0 } },
    { .firstSprite = 6, .spriteCount = 6, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Depth-sorted clipping for pod service gantry view 3.
///
/// Clips scene drawing to (195, 0, 125, 239) in draw-buffer pixels, then
/// restores the full screen at unscaled OT depth 2500. The terminal record
/// emits no command. The room owns this writable list while loaded.
static SpriteDrawArea _gShelterB1PodServiceGantryView3DrawAreas[2] = {
    { .clipRect = { 195, 0, 125, 239 }, .restoreDepth = 2500 },
    { .restoreDepth = SPRITE_DRAW_AREA_END },
};

/// Texture rectangles and sorting depths for pod service gantry view 4.
///
/// This view's batches select all 82 elements. Positions and dimensions
/// are pixels relative to the view centre; depths are unscaled OT sorting
/// values. The room owns these writable sources while its overlay is loaded.
static SpriteSource _gShelterB1PodServiceGantryView4Sources[82] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 80, 1075, { .fields = { 72, 64 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -112, 88, 1050, { .fields = { 16, 224 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -80, 88, 1050, { .fields = { 0, 32 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -16, 88, 1056, { .fields = { 0, 64 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 16, 88, 1056, { .fields = { 16, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 48, 88, 1062, { .fields = { 16, 160 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 80, 88, 1081, { .fields = { 16, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 112, 88, 1081, { .fields = { 112, 160 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 32 } }, 144, 88, 1081, { .fields = { 0, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, -80, 72, 1045, { .fields = { 48, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, -48, 72, 1050, { .fields = { 112, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 8 } }, -40, 64, 1075, { .fields = { 56, 136 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, -16, 72, 1056, { .fields = { 80, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 24 } }, 16, 64, 1057, { .fields = { 56, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 48, 56, 1087, { .fields = { 112, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 80, 56, 1112, { .fields = { 48, 168 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 24 } }, 72, 32, 1187, { .fields = { 88, 176 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 24 } }, 80, 32, 1187, { .fields = { 56, 168 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 112, 56, 1112, { .fields = { 48, 136 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 24 } }, 144, 64, 1112, { .fields = { 72, 120 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 48, 1237, { .fields = { 56, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -48, 88, 1056, { .fields = { 48, 200 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -48, 8, 1050, { .fields = { 16, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, -80, 24, 1037, { .fields = { 40, 64 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 24 } }, -112, 40, 1000, { .fields = { 56, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 24 } }, -112, 64, 1000, { .fields = { 72, 72 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, 88, 1000, { .fields = { 56, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 64, 962, { .fields = { 48, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 16 } }, -120, 88, 962, { .fields = { 96, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 24 } }, -128, 40, 962, { .fields = { 88, 120 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 32, 950, { .fields = { 104, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 8 } }, -136, 32, 962, { .fields = { 48, 120 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 24 } }, -80, 40, 1037, { .fields = { 48, 24 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, -80, 64, 1037, { .fields = { 48, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, 40, 1050, { .fields = { 72, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, 64, 1050, { .fields = { 72, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 48 } }, -80, -8, 975, { .fields = { 112, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 24 } }, -64, 8, 962, { .fields = { 64, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 24 } }, -48, 0, 937, { .fields = { 112, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, 64, 1450, { .fields = { 88, 208 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -80, 1450, { .fields = { 80, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -32, 1450, { .fields = { 80, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 32 } }, 112, 16, 1450, { .fields = { 0, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -80, 1450, { .fields = { 80, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -32, 1450, { .fields = { 80, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, 16, 1450, { .fields = { 32, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 40 } }, 32, -72, 1445, { .fields = { 48, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -32, 1445, { .fields = { 80, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, 16, 1445, { .fields = { 48, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 8 } }, 48, 64, 1445, { .fields = { 56, 80 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 56, 1075, { .fields = { 64, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 56, 1175, { .fields = { 64, 224 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 80, 1175, { .fields = { 64, 200 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 8, 88, 1175, { .fields = { 64, 208 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, -16, 72, 1050, { .fields = { 88, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 8, 64, 1062, { .fields = { 88, 152 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 32, 56, 1081, { .fields = { 80, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 56, 56, 1081, { .fields = { 80, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 8 } }, 56, 48, 1081, { .fields = { 56, 88 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 48, 1345, { .fields = { 64, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 16, 56, 1317, { .fields = { 88, 200 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, -8, 64, 1295, { .fields = { 120, 152 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, -24, 72, 1292, { .fields = { 104, 24 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 0, 64, 1305, { .fields = { 104, 72 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 24, 56, 1330, { .fields = { 96, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, 48, 1375, { .fields = { 104, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 48, 1367, { .fields = { 96, 184 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 48, 1387, { .fields = { 88, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 8, 56, 1330, { .fields = { 104, 176 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, -16, 64, 1305, { .fields = { 104, 184 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, -32, 72, 1300, { .fields = { 104, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, -8, 64, 1320, { .fields = { 88, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 16, 56, 1362, { .fields = { 80, 24 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, 40, 56, 1375, { .fields = { 80, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 48, 1425, { .fields = { 104, 120 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 48, 1431, { .fields = { 96, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, 0, 56, 1382, { .fields = { 96, 176 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, -24, 64, 1362, { .fields = { 112, 152 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, -40, 72, 1357, { .fields = { 104, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, -16, 64, 1375, { .fields = { 120, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X896, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, 8, 56, 1412, { .fields = { 56, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 56, 1436, { .fields = { 32, 32 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for pod service gantry view 4.
///
/// The zero-count first batch selects decoded background strips.
/// Eight sprite ranges cover all 82 source elements; ranges start visible
/// and included in cached drawing. The final two bytes retain unproven
/// serialized values. Storage remains writable while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView4Batches[10] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 22, .field_6 = { 6, 0 } },
    { .firstSprite = 22, .spriteCount = 14, .field_6 = { 1, 0 } },
    { .firstSprite = 36, .spriteCount = 3, .field_6 = { 4, 0 } },
    { .firstSprite = 39, .spriteCount = 11, .field_6 = { 0, 0 } },
    { .firstSprite = 50, .spriteCount = 9, .field_6 = { 5, 0 } },
    { .firstSprite = 59, .spriteCount = 7, .field_6 = { 3, 0 } },
    { .firstSprite = 66, .spriteCount = 8, .field_6 = { 7, 0 } },
    { .firstSprite = 74, .spriteCount = 8, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for pod service gantry view 5.
///
/// This view's batches select all 15 elements. Positions and dimensions
/// are pixels relative to the view centre; depths are unscaled OT sorting
/// values. The room owns these writable sources while its overlay is loaded.
static SpriteSource _gShelterB1PodServiceGantryView5Sources[15] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 8, 8 } }, -48, 32, 925, { .fields = { 120, 248 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 104, 700, { .fields = { 112, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 48 } }, -96, 72, 750, { .fields = { 112, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 48 } }, -80, 40, 875, { .fields = { 112, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 24 } }, -64, 32, 925, { .fields = { 112, 160 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 32, 1000, { .fields = { 64, 32 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 56, 16 } }, 56, 32, 1000, { .fields = { 56, 16 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, 32, 1000, { .fields = { 80, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 16 } }, -56, 32, 1000, { .fields = { 72, 80 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 24 } }, -40, 48, 1000, { .fields = { 104, 208 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 32 } }, -40, 72, 1000, { .fields = { 104, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, -16, 48, 1000, { .fields = { 80, 64 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 24 } }, 16, 48, 1000, { .fields = { 112, 184 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, -16, 72, 1000, { .fields = { 80, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 32 } }, 16, 72, 1000, { .fields = { 112, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for pod service gantry view 5.
///
/// The zero-count first batch selects decoded background strips.
/// Two sprite ranges cover all 15 source elements; ranges start visible
/// and included in cached drawing. The final two bytes retain unproven
/// serialized values. Storage remains writable while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView5Batches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 5, .field_6 = { 1, 0 } },
    { .firstSprite = 5, .spriteCount = 10, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Sprite drawing batches for pod service gantry view 6.
///
/// The zero-count first batch selects decoded background strips.
/// The next record ends the list. Its retained source-slot address supplies
/// no source elements. The room owns this writable list while loaded.
static SpriteBatch _gShelterB1PodServiceGantryView6Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Sprite drawing batches for pod service gantry view 7.
///
/// The zero-count first batch selects decoded background strips.
/// The next record ends the list. Its retained source-slot address supplies
/// no source elements. The room owns this writable list while loaded.
static SpriteBatch _gShelterB1PodServiceGantryView7Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for pod service gantry view 8.
///
/// This view's batches select all 10 elements. Positions and dimensions
/// are pixels relative to the view centre; depths are unscaled OT sorting
/// values. The room owns these writable sources while its overlay is loaded.
static SpriteSource _gShelterB1PodServiceGantryView8Sources[10] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 40 } }, -152, -56, 1950, { .fields = { 88, 120 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 40 } }, -112, -56, 1950, { .fields = { 88, 160 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 40 } }, -72, -56, 1900, { .fields = { 88, 200 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -32, -48, 1800, { .fields = { 56, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 0, -48, 1800, { .fields = { 56, 32 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 40 } }, 32, -56, 1900, { .fields = { 88, 80 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 40 } }, 72, -56, 1950, { .fields = { 88, 40 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 40 } }, 112, -56, 1950, { .fields = { 88, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, -24, -16, 1800, { .fields = { 96, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, -16, 1800, { .fields = { 112, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for pod service gantry view 8.
///
/// The zero-count first batch selects decoded background strips.
/// One sprite range covers all 10 source elements; ranges start visible
/// and included in cached drawing. The final two bytes retain unproven
/// serialized values. Storage remains writable while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView8Batches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB1PodServiceGantryView8Sources), .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for pod service gantry view 9.
///
/// This view's batches select all 24 elements. Positions and dimensions
/// are pixels relative to the view centre; depths are unscaled OT sorting
/// values. The room owns these writable sources while its overlay is loaded.
static SpriteSource _gShelterB1PodServiceGantryView9Sources[24] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 16 } }, -64, 0, 812, { .fields = { 40, 200 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -112, -8, 800, { .fields = { 48, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, -8, 800, { .fields = { 40, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, -24, 800, { .fields = { 104, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 16 } }, 16, 0, 812, { .fields = { 56, 240 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, 64, -8, 800, { .fields = { 40, 176 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, 112, -8, 800, { .fields = { 48, 104 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, -24, 800, { .fields = { 32, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -72, 56, 634, { .fields = { 40, 152 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -120, 56, 656, { .fields = { 80, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, -152, 56, 827, { .fields = { 48, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 24 } }, -160, 32, 649, { .fields = { 88, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, 24, 56, 634, { .fields = { 48, 32 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, 72, 56, 656, { .fields = { 48, 80 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, 120, 56, 673, { .fields = { 24, 56 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 24 } }, 120, 32, 650, { .fields = { 56, 56 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -64, 0, 570, { .fields = { 96, 64 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -64, 32, 630, { .fields = { 96, 32 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 32 } }, -64, 64, 701, { .fields = { 88, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 96, 801, { .fields = { 64, 24 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 8 } }, 24, 96, 802, { .fields = { 56, 232 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 32 } }, 24, 64, 701, { .fields = { 88, 160 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 32, 32, 631, { .fields = { 96, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, 32, 0, 570, { .fields = { 96, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for pod service gantry view 9.
///
/// The zero-count first batch selects decoded background strips.
/// Five sprite ranges cover all 24 source elements; ranges start visible
/// and included in cached drawing. The final two bytes retain unproven
/// serialized values. Storage remains writable while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView9Batches[7] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 4, .field_6 = { 0, 0 } },
    { .firstSprite = 4, .spriteCount = 4, .field_6 = { 3, 0 } },
    { .firstSprite = 8, .spriteCount = 4, .field_6 = { 2, 0 } },
    { .firstSprite = 12, .spriteCount = 4, .field_6 = { 4, 0 } },
    { .firstSprite = 16, .spriteCount = 8, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Sprite drawing batches for pod service gantry view 10.
///
/// The zero-count first batch selects decoded background strips.
/// The next record ends the list. Its retained source-slot address supplies
/// no source elements. The room owns this writable list while loaded.
static SpriteBatch _gShelterB1PodServiceGantryView10Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for pod service gantry view 11.
///
/// This view's batches select all 2 elements. Positions and dimensions
/// are pixels relative to the view centre; depths are unscaled OT sorting
/// values. The room owns these writable sources while its overlay is loaded.
static SpriteSource _gShelterB1PodServiceGantryView11Sources[2] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, 88, 104, 520, { .fields = { 96, 16 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 16 } }, 120, 104, 511, { .fields = { 96, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for pod service gantry view 11.
///
/// The zero-count first batch selects decoded background strips.
/// One sprite range covers all 2 source elements; ranges start visible
/// and included in cached drawing. The final two bytes retain unproven
/// serialized values. Storage remains writable while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView11Batches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB1PodServiceGantryView11Sources), .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Sprite drawing batches for pod service gantry view 12.
///
/// The zero-count first batch selects decoded background strips.
/// The next record ends the list. Its retained source-slot address supplies
/// no source elements. The room owns this writable list while loaded.
static SpriteBatch _gShelterB1PodServiceGantryView12Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Sprite drawing batches for pod service gantry view 13.
///
/// The zero-count first batch selects decoded background strips.
/// The next record ends the list. Its retained source-slot address supplies
/// no source elements. The room owns this writable list while loaded.
static SpriteBatch _gShelterB1PodServiceGantryView13Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for pod service gantry view 14.
///
/// All 14 elements are drawn over decoded background strips. Positions are
/// pixels relative to the view centre; dimensions are pixel extents and depth
/// is an unscaled OT sorting value. The 8-bit textures share one page and
/// palette. RGB starts at neutral modulation and source command flags are clear.
/// The room owns this writable array; it remains valid only while its overlay
/// is loaded.
static SpriteSource _gShelterB1PodServiceGantryView14Sources[14] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 72, 212, { .fields = { 80, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, -112, 72, 212, { .fields = { 80, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, -64, 72, 212, { .fields = { 80, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, -16, 72, 212, { .fields = { 80, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 32, 72, 212, { .fields = { 80, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 48 } }, 80, 72, 212, { .fields = { 32, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 48 } }, 128, 72, 212, { .fields = { 48, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 40 } }, 80, 32, 212, { .fields = { 32, 144 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, 32, 48, 212, { .fields = { 32, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 48 } }, 128, 24, 212, { .fields = { 48, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -16, 48, 212, { .fields = { 0, 48 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -64, 48, 212, { .fields = { 0, 96 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 32 } }, -160, 40, 212, { .fields = { 32, 184 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 24 } }, -112, 48, 212, { .fields = { 0, 72 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Background selection and sprite range for pod service gantry view 14.
///
/// The zero-count first record requests decoded image strips; the second
/// draws the complete source array and starts visible in cached drawing.
/// The final record ends the list. The last two bytes of each record are
/// retained at zero; their purpose is unproven. The room owns this writable
/// list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView14Batches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB1PodServiceGantryView14Sources), .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 15.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView15Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 16.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView16Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 17.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView17Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 18.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView18Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 19.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView19Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 20.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. The first record's retained hidden flag affects no sprites.
/// `SpriteView.sources.empty` retains this writable room-owned list's address,
/// valid only while the overlay is loaded.
static SpriteBatch _gShelterB1PodServiceGantryView20Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = true },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 21.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView21Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 22.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView22Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 23.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView23Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 24.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView24Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 25.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView25Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 26.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView26Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 27.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView27Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 28.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView28Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 29.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView29Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 30.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView30Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 31.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView31Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 32.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView32Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 33.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView33Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 34.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView34Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 35.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView35Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 36.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView36Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 37.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView37Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 38.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView38Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 39.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView39Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 40.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView40Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 41.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView41Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 42.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView42Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 43.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView43Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for pod service gantry view 44.
///
/// Zero first count selects decoded image strips; the next record ends the
/// list. `SpriteView.sources.empty` retains its address without source sprites.
/// The room owns this writable list for its overlay lifetime.
static SpriteBatch _gShelterB1PodServiceGantryView44Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Nine foreground texture rectangles for mapped gantry view 45.
///
/// Positions and sizes are pixels; depth is the unscaled OT sorting value.
/// The renderer borrows this writable array while the room is loaded.
static SpriteSource _gShelterB1PodServiceGantryView45Sources[9] = {
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, 0, 902, { .fields = { 96, 224 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 32, 48 } }, -160, 72, 915, { .fields = { 96, 176 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 56, 24 } }, -128, 96, 875, { .fields = { 40, 216 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 72, 24 } }, -72, 96, 875, { .fields = { 24, 192 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 40, 72 } }, 120, -96, 625, { .fields = { 88, 0 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 24, 48 } }, 136, -24, 625, { .fields = { 104, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 48, 40 } }, 112, 24, 625, { .fields = { 56, 128 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 88, 56 } }, 72, 64, 625, { .fields = { 40, 72 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_TEXTURE_PAGE_X960, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_CLUT, { .fields = { 72, 24 } }, 0, 96, 875, { .fields = { 24, 168 } }, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, SHELTER_B1_POD_SERVICE_GANTRY_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Drawing batches retaining the background and selecting all nine view-45 sprites.
///
/// The zero-count header selects decoded image strips. The range borrows the
/// full source array; the terminal record ends this room-owned writable list.
static SpriteBatch _gShelterB1PodServiceGantryView45Batches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB1PodServiceGantryView45Sources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Background-only drawing batches for mapped gantry view 46.
///
/// The zero-count header selects decoded image strips; the next record ends
/// the list. The source slot retains this list's address without sprite elements.
static SpriteBatch _gShelterB1PodServiceGantryView46Batches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gShelterB1PodServiceGantrySpriteViews[46] = {
    { { .elements = _gShelterB1PodServiceGantryView1Sources }, _gShelterB1PodServiceGantryView1Batches, NULL },
    { { .elements = _gShelterB1PodServiceGantryView2Sources }, _gShelterB1PodServiceGantryView2Batches, NULL },
    { { .elements = _gShelterB1PodServiceGantryView3Sources }, _gShelterB1PodServiceGantryView3Batches, _gShelterB1PodServiceGantryView3DrawAreas },
    { { .elements = _gShelterB1PodServiceGantryView4Sources }, _gShelterB1PodServiceGantryView4Batches, NULL },
    { { .elements = _gShelterB1PodServiceGantryView5Sources }, _gShelterB1PodServiceGantryView5Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView6Batches }, _gShelterB1PodServiceGantryView6Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView7Batches }, _gShelterB1PodServiceGantryView7Batches, NULL },
    { { .elements = _gShelterB1PodServiceGantryView8Sources }, _gShelterB1PodServiceGantryView8Batches, NULL },
    { { .elements = _gShelterB1PodServiceGantryView9Sources }, _gShelterB1PodServiceGantryView9Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView10Batches }, _gShelterB1PodServiceGantryView10Batches, NULL },
    { { .elements = _gShelterB1PodServiceGantryView11Sources }, _gShelterB1PodServiceGantryView11Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView12Batches }, _gShelterB1PodServiceGantryView12Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView13Batches }, _gShelterB1PodServiceGantryView13Batches, NULL },
    { { .elements = _gShelterB1PodServiceGantryView14Sources }, _gShelterB1PodServiceGantryView14Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView15Batches }, _gShelterB1PodServiceGantryView15Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView16Batches }, _gShelterB1PodServiceGantryView16Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView17Batches }, _gShelterB1PodServiceGantryView17Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView18Batches }, _gShelterB1PodServiceGantryView18Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView19Batches }, _gShelterB1PodServiceGantryView19Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView20Batches }, _gShelterB1PodServiceGantryView20Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView21Batches }, _gShelterB1PodServiceGantryView21Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView22Batches }, _gShelterB1PodServiceGantryView22Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView23Batches }, _gShelterB1PodServiceGantryView23Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView24Batches }, _gShelterB1PodServiceGantryView24Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView25Batches }, _gShelterB1PodServiceGantryView25Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView26Batches }, _gShelterB1PodServiceGantryView26Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView27Batches }, _gShelterB1PodServiceGantryView27Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView28Batches }, _gShelterB1PodServiceGantryView28Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView29Batches }, _gShelterB1PodServiceGantryView29Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView30Batches }, _gShelterB1PodServiceGantryView30Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView31Batches }, _gShelterB1PodServiceGantryView31Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView32Batches }, _gShelterB1PodServiceGantryView32Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView33Batches }, _gShelterB1PodServiceGantryView33Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView34Batches }, _gShelterB1PodServiceGantryView34Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView35Batches }, _gShelterB1PodServiceGantryView35Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView36Batches }, _gShelterB1PodServiceGantryView36Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView37Batches }, _gShelterB1PodServiceGantryView37Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView38Batches }, _gShelterB1PodServiceGantryView38Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView39Batches }, _gShelterB1PodServiceGantryView39Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView40Batches }, _gShelterB1PodServiceGantryView40Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView41Batches }, _gShelterB1PodServiceGantryView41Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView42Batches }, _gShelterB1PodServiceGantryView42Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView43Batches }, _gShelterB1PodServiceGantryView43Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView44Batches }, _gShelterB1PodServiceGantryView44Batches, NULL },
    { { .elements = _gShelterB1PodServiceGantryView45Sources }, _gShelterB1PodServiceGantryView45Batches, NULL },
    { { .empty = _gShelterB1PodServiceGantryView46Batches }, _gShelterB1PodServiceGantryView46Batches, NULL },
};

/// Nine room-owned point lights contributing in every gantry view.
///
/// Positions and falloff radii use world-coordinate game units; RGB uses
/// 12 fractional bits. Gameplay updates transforms and attenuation in place.
/// The collection borrows all nine records for the room overlay's lifetime.
static WorldCoordPointLight _gShelterB1PodServiceGantryPointLights[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x310C, -2576, 7191 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1756, 3216, 2480 }, { 0, 0 } }, 100, 200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x309F, -2482, 7439 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1019, 3392, 1019 }, { 0, 0 } }, 100, 461 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x38B7, -3643, 7891 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2170, 2209, 2195 }, { 0, 0 } }, 1878, 3562 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4133, -2789, 3232 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2983, 2975, 2968 }, { 0, 0 } }, 1970, 6109 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8491, -3002, 3193 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2615, 2612, 2602 }, { 0, 0 } }, 3836, 6579 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x33FB, -2300, 0x2736 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2435, 3392, 2909 }, { 0, 0 } }, 300, 800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4074, -2305, 6109 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2059, 4119, 2889 }, { 0, 0 } }, 401, 855 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x41F1, -1881, 7349 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1756, 3216, 2480 }, { 0, 0 } }, 100, 200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x35EA, -1128, 2778 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1944, 1895, 1866 }, { 0, 0 } }, 5201, 9559 },
};

/// Two live cone lights followed by 756 bytes of unproven purpose and boundaries.
///
/// The collection borrows only `coneLights`, both admitted in every view.
/// Gameplay updates their transforms and attenuation while the overlay is
/// loaded. The following byte storage is outside the published light count.
static ShelterB1PodServiceGantrySpotLightStorage _gShelterB1PodServiceGantrySpotLightStorage = {
    .coneLights = {
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -4003, -2, 873 }, { 869, 266, 4000 }, { -59, 4087, -261 } }, { 3412, -0x5063, 9454 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2764, 2764, 2425 }, { 0, 0 } }, { 872, 3993, -260, 0 }, 0x4E20, 0x7530, 0x2C71 },
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -4092, 0, -222 }, { 215, -909, -3996 }, { -50, -3996, 908 } }, { 8054, 5038, 1813 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 226, 226, 198 }, { 0, 0 } }, { -221, -3988, 907, 0 }, 10, 0x4E20, 0x2C71 },
    },
    .unknown_D8 = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x09,
        0x00,
        0x00,
        0x00,
        0x58,
        0xD8,
        0x18,
        0x80,
        0x02,
        0x00,
        0x00,
        0x00,
        0xB8,
        0xDB,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x99,
        0xF4,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xD8,
        0xF8,
        0x00,
        0x00,
        0xB0,
        0xF1,
        0x00,
        0x00,
        0x28,
        0x07,
        0x00,
        0x00,
        0xB0,
        0xF1,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xF0,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB5,
        0xF3,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8E,
        0xFC,
        0x00,
        0x00,
        0x60,
        0xF0,
        0x00,
        0x00,
        0x69,
        0xF0,
        0x00,
        0x00,
        0x67,
        0xFC,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x70,
        0x17,
        0x00,
        0x00,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x70,
        0x17,
        0xB8,
        0xF2,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x70,
        0x17,
        0xB8,
        0xF2,
        0xDC,
        0x05,
        0x00,
        0x00,
        0x70,
        0x17,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x40,
        0x1F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x40,
        0x1F,
        0xB8,
        0xF2,
        0xDC,
        0x05,
        0x00,
        0x00,
        0x36,
        0x31,
        0x00,
        0x00,
        0x1C,
        0x25,
        0x00,
        0x00,
        0x36,
        0x31,
        0x00,
        0x00,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x36,
        0x31,
        0xB8,
        0xF2,
        0x00,
        0x00,
        0x00,
        0x00,
        0x36,
        0x31,
        0xB8,
        0xF2,
        0x1C,
        0x25,
        0x00,
        0x00,
        0x40,
        0x38,
        0x00,
        0x00,
        0x7C,
        0x15,
        0x00,
        0x00,
        0x40,
        0x38,
        0x00,
        0x00,
        0x7C,
        0x15,
        0x00,
        0x00,
        0x40,
        0x38,
        0xB8,
        0xF2,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x3E,
        0xB8,
        0xF2,
        0xC7,
        0x24,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x40,
        0x1F,
        0x00,
        0x00,
        0x94,
        0x11,
        0x00,
        0x00,
        0x40,
        0x1F,
        0x00,
        0x00,
        0x94,
        0x11,
        0x00,
        0x00,
        0x28,
        0x23,
        0xB8,
        0xF2,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x94,
        0x11,
        0x00,
        0x00,
        0x70,
        0x17,
        0xB8,
        0xF2,
        0x94,
        0x11,
        0x00,
        0x00,
        0xA8,
        0x2F,
        0x00,
        0x00,
        0xF8,
        0x11,
        0x00,
        0x00,
        0x9C,
        0xFF,
        0x00,
        0x00,
        0x78,
        0x05,
        0x00,
        0x00,
        0x9C,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x44,
        0x11,
        0x00,
        0x00,
        0xD4,
        0x30,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xD4,
        0x30,
        0x18,
        0xFC,
        0x04,
        0x15,
        0x00,
        0x00,
        0xA4,
        0x38,
        0x00,
        0x00,
        0x44,
        0x11,
        0x00,
        0x00,
        0xD4,
        0x30,
        0x00,
        0x00,
        0x44,
        0x11,
        0x00,
        0x00,
        0xA4,
        0x38,
        0x50,
        0xFB,
        0x00,
        0x00,
        0x00,
        0x00,
        0xD4,
        0x30,
        0x50,
        0xFB,
        0x04,
        0x15,
        0x00,
        0x00,
        0x74,
        0x40,
        0x00,
        0x00,
        0x7C,
        0x15,
        0x00,
        0x00,
        0x74,
        0x40,
        0x00,
        0x00,
        0x7C,
        0x15,
        0x00,
        0x00,
        0x8C,
        0x3C,
        0xB8,
        0xF2,
        0x1C,
        0x25,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA4,
        0x38,
        0x00,
        0x00,
        0x78,
        0x05,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x09,
        0x00,
        0x0A,
        0x00,
        0x08,
        0x00,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x0A,
        0x00,
        0x0C,
        0x00,
        0x0B,
        0x00,
        0x0D,
        0x00,
        0x03,
        0x00,
        0x00,
        0x00,
        0x0D,
        0x00,
        0x0C,
        0x00,
        0x0E,
        0x00,
        0x00,
        0x00,
        0x02,
        0x00,
        0x00,
        0x00,
        0x11,
        0x00,
        0x09,
        0x00,
        0x10,
        0x00,
        0x08,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x1A,
        0x00,
        0x1D,
        0x00,
        0x05,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x1E,
        0x00,
        0x21,
        0x00,
        0x06,
        0x00,
        0x00,
        0x00,
        0x23,
        0x00,
        0x24,
        0x00,
        0x22,
        0x00,
        0x00,
        0x00,
        0x07,
        0x00,
        0x00,
        0x00,
        0x11,
        0x00,
        0x10,
        0x00,
        0x26,
        0x00,
        0x27,
        0x00,
    },
};

WorldCoordRoomLights gShelterB1PodServiceGantryRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB1PodServiceGantryPointLights),
    .pointLights           = _gShelterB1PodServiceGantryPointLights,
    .coneLightCount        = ARRAY_SIZE(_gShelterB1PodServiceGantrySpotLightStorage.coneLights),
    .coneLights            = _gShelterB1PodServiceGantrySpotLightStorage.coneLights,
};

/// Plays the gantry scene and movie, replaces the actor package and leaves after the next scene.
///
/// Requires the zeroed work owned by the initialized room task. Polling reaps
/// each scene controller after its stop request; the returned scene result is
/// ignored. The movie launcher transfers execution to the display list and
/// kills itself, so its stored pointer is never polled. The room sequence
/// resumes after movie restoration, before loading actor_160900 into actor
/// slot 1 previously occupied by actor_560800. The final step commits the pod
/// access tunnel destination and waits for the area reload to tear down the room.
static void _shelterB1PodServiceGantryRunSceneSequence(Task* task)
{
    /// Queues stage-zero actor file 160900 using this sequence's byte blocks.
    ///
    /// Captures writable `fileKey` (four bytes) and `loadArgs` (the four-byte
    /// CD argument extent). Key byte 1 is ignored; every argument byte is set.
    /// Enqueue copies both blocks immediately. Expands to a standalone statement
    /// list; use within a braced switch case or branch.
#define SHELTER_B1_POD_SERVICE_GANTRY_QUEUE_SECOND_SCENE()       \
    fileKey[CD_FILE_KEY_GROUP]     = SECOND_SCENE_FILE_GROUP;    \
    fileKey[CD_FILE_KEY_STAGE]     = SECOND_SCENE_CDF_STAGE;     \
    fileKey[CD_FILE_KEY_INDEX]     = SECOND_SCENE_FILE_INDEX;    \
    loadArgs[CD_LOAD_ARG_HUNDREDS] = SECOND_SCENE_FILE_HUNDREDS; \
    loadArgs[CD_LOAD_ARG_MODE]     = CD_COMMAND_LOAD_DEFAULT;    \
    loadArgs[CD_LOAD_ARG_IMAGE_X]  = 0;                          \
    loadArgs[CD_LOAD_ARG_IMAGE_Y]  = 0;                          \
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs)

    enum { CD_FILE_KEY_BYTES          = 4,
           CD_FILE_KEY_INDEX          = 0,
           CD_FILE_KEY_GROUP          = 2,
           CD_FILE_KEY_STAGE          = 3,
           CD_LOAD_ARG_HUNDREDS       = 0,
           CD_LOAD_ARG_MODE           = 1,
           CD_LOAD_ARG_IMAGE_X        = 2,
           CD_LOAD_ARG_IMAGE_Y        = 3,
           SECOND_SCENE_FILE_GROUP    = 16,
           SECOND_SCENE_FILE_HUNDREDS = 9,
           SECOND_SCENE_FILE_INDEX    = 0,
           SECOND_SCENE_CDF_STAGE     = 0,
           GANTRY_RETURN_PENDING      = 1,
           EXIT_WARP                  = 2,
           EXIT_ROOM                  = 1,
           SPRITE_RESOURCE_VARIANT    = 1 };
    u8                              fileKey[CD_FILE_KEY_BYTES];
    u8                              loadArgs[sizeof(gCdCmdQueue.entries[0].args)];
    s32                             ignoredSceneResult;
    _ShelterB1PodServiceGantryWork* work = task->work;

    switch (work->step) {
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_START_FIRST_SCENE:
            work->sceneTask = taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_CUTSCENE, 0, 0);
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_AWAIT_FIRST_SCENE:
            if (taskPollKill(work->sceneTask, &ignoredSceneResult) == 0) {
                break;
            }
            work->sceneTask = taskSpawnFromTable(gActor560800MovieTaskDescriptors, ACTOR_560800_MOVIE_TASK_LAUNCH, 0, 0);
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_LOAD_SECOND_SCENE:
            // Queue stage-zero file 160900 after movie playback restores the game loop.
            // Enqueue ignores file-key byte 1 and copies all four load-argument bytes.
            SHELTER_B1_POD_SERVICE_GANTRY_QUEUE_SECOND_SCENE();
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_START_SECOND_SCENE:
            if (cdCmdIsIdle() == 0) {
                break;
            }
            work->sceneTask = taskSpawnFromTable(gActor160900TaskDescriptors, 0, 0, 0);
            areaApplySavedUpdates(gShelterB1PodServiceGantrySecondSceneAreaUpdates);
            gameFlagSetNibble(GAME_FLAG_118, GANTRY_RETURN_PENDING);
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_AWAIT_SECOND_SCENE:
            if (taskPollKill(work->sceneTask, &ignoredSceneResult) == 0) {
                break;
            }
            // Reload into the pod access tunnel after the second scene has been reaped.
            gGameSession->unknown_138                                   = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_MINE_SHELTER;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = EXIT_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = EXIT_ROOM;
            gDisplayState.spriteVariant                                 = SPRITE_RESOURCE_VARIANT;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            // Also advance the step when the transition request falls through here.
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_PAUSE:
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_LEAVING:
            break;
    }
}

#undef SHELTER_B1_POD_SERVICE_GANTRY_QUEUE_SECOND_SCENE

/// Refuses key-item use at the gantry without consuming the selected item.
///
/// Handles ROOM_MESSAGE_USE_KEY_ITEM. All arguments are unused; the reply
/// selects the inventory's refusal notice.
static s32 _shelterB1PodServiceGantryRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition and resolves its Mine/Shelter destination from progress.
///
/// Handles ROOM_EVENT_MESSAGE_RESOLVE with a readable complete eight-byte
/// request and writable reply; both may be the same record. Copies the request
/// before resolving the reply's room. Queries preserve its destination. The map
/// overlay must be loaded; neither pointer is retained. Always returns 1.
static s32 _shelterB1PodServiceGantryResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B1_POD_SERVICE_GANTRY_ROOM_EVENT_ACCEPTED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_B1_POD_SERVICE_GANTRY_ROOM_EVENT_ACCEPTED;
}

/// Ignores gantry CAP room commands and returns zero without side effects.
///
/// ROOM_MESSAGE_COMMAND carries an integer selector and argument. All four
/// parameters are unused; the sender discards the result.
static s32 _shelterB1PodServiceGantryIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg)
{
    enum { SHELTER_B1_POD_SERVICE_GANTRY_COMMAND_IGNORED = 0 };

    return SHELTER_B1_POD_SERVICE_GANTRY_COMMAND_IGNORED;
}

/// Ignores gantry direction-trigger actions and returns zero without side effects.
///
/// DIRECTION_MESSAGE_ROOM_ACTION borrows a request and carries a zero second
/// payload. The request is neither read nor retained, and the sender ignores
/// the result. The task and message ID are also unused.
static s32 _shelterB1PodServiceGantryIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum { SHELTER_B1_POD_SERVICE_GANTRY_ROOM_ACTION_IGNORED = 0 };

    return SHELTER_B1_POD_SERVICE_GANTRY_ROOM_ACTION_IGNORED;
}

/// Publishes the gantry room receiver and allocates its zeroed scene-sequence work.
///
/// Requires state zero. The task owns the primary-heap allocation until teardown.
/// Allocation failure kills the receiver; success hides the display and advances
/// to the running state, whose step zero starts the first scene.
static void _shelterB1PodServiceGantryInitRoomState(Task* task)
{
    _ShelterB1PodServiceGantryWork* work;

    task->msgTable = D_shelter_b1_pod_service_gantry_8017FAF4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    memFillBytes(work, 0U, sizeof(*work));
    // Keep the display hidden until the opening scene takes control.
    SetDispMask(0);
    task->state += 1;
}

void shelterB1PodServiceGantryRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterB1PodServiceGantryStateHandlers;
    stateHandlers.funcs[task->state](task);
}
