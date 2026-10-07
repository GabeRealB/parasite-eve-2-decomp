#include "rooms/shelter_b1_pod_service_gantry.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "shelter_b1_pod_service_gantry_private.h"

#include "rooms/shelter_b1_pod_service_gantry_light_types.h"

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

extern TaskDesc D_actor_160900_8013FB50[];
extern TaskDesc D_actor_560800_8016EA28[];
extern TaskDesc D_actor_560800_801718F0[];

/// The room's message table, published in `Task::msgTable`.
extern TaskMessageEntry D_shelter_b1_pod_service_gantry_8017FAF4[];

static void func_shelter_b1_pod_service_gantry_8017D628(Task* task);
static void func_shelter_b1_pod_service_gantry_8017D81C(Task* arg0);

/// The room task's three states: set-up, the step sequence below, and exit.
static const TaskFuncTable3 D_shelter_b1_pod_service_gantry_8017D5C4 = {
    { func_shelter_b1_pod_service_gantry_8017D81C, func_shelter_b1_pod_service_gantry_8017D628, taskKill },
};

s32 func_shelter_b1_pod_service_gantry_8017D7C0(Task*, s32, s32, s32);
s32 func_shelter_b1_pod_service_gantry_8017D7C8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_pod_service_gantry_8017D80C(Task*, s32, s32, s32);
s32 func_shelter_b1_pod_service_gantry_8017D814(Task*, s32, s32, s32);

extern WorldCoordPointLight                      D_shelter_b1_pod_service_gantry_80181DC8[9];
extern ShelterB1PodServiceGantrySpotLightStorage D_shelter_b1_pod_service_gantry_80182128;

TaskMessageEntry D_shelter_b1_pod_service_gantry_8017FAF4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_pod_service_gantry_8017D7C8 },
    { 5105, func_shelter_b1_pod_service_gantry_8017D7C0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_pod_service_gantry_8017D814 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_pod_service_gantry_8017D80C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* D_shelter_b1_pod_service_gantry_8017FB1C[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_pod_service_gantry_8017FB20[1] = { 46 };

DirectionWarpEntry D_shelter_b1_pod_service_gantry_8017FB24[1] = {
    { { { .word = 1024 }, 2535, 0, 3220 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 2535, 0, 3220 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

AreaResource D_shelter_b1_pod_service_gantry_8017FB5C[5] = {
    { 34, 608, AREA_RESOURCE_FILE_GROUP_BASE_50, 1, { 0, 0 }, D_actor_560800_801718F0 },
    { 101, 608, AREA_RESOURCE_FILE_GROUP_BASE_60, 1, { 0, 0 }, D_actor_560800_801718F0 },
    { 131, 608, AREA_RESOURCE_FILE_GROUP_BASE_60, 1, { 0, 0 }, D_actor_560800_801718F0 },
    { 59, 608, AREA_RESOURCE_FILE_GROUP_BASE_60, 1, { 0, 0 }, D_actor_560800_801718F0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_pod_service_gantry_8017FB98[6] = {
    { 34, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0 },
    { 131, 0, 0, 0, 0, 0, 0, 1, 2, 8, 0 },
    { 101, 0, 0, 0, 0, 0, 0, 2, 4, 5, 0 },
    { 59, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0 },
    { 101, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_pod_service_gantry_8017FBF8[11] = {
    { NULL, NULL },
    { D_shelter_b1_pod_service_gantry_8017FB98, D_shelter_b1_pod_service_gantry_8017FB5C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

static SVECTOR _gShelterB1PodServiceGantryCollision02C04Normals[13] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_normals.inc"
};

static SVECTOR _gShelterB1PodServiceGantryCollision02C04Verts[66] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_verts.inc"
};

static WorldCollisionGridFace _gShelterB1PodServiceGantryCollision02C04Faces[30] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_faces.inc"
};

static s16 _gShelterB1PodServiceGantryCollision02C04Cells[172] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1PodServiceGantryCollision02C04Cells[i])
static s16* _gShelterB1PodServiceGantryCollision02C04Table[15] = {
#include "assets/shelter_b1_pod_service_gantry_collision_02C04_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_pod_service_gantry_801801C4 = { NULL, _gShelterB1PodServiceGantryCollision02C04Normals, _gShelterB1PodServiceGantryCollision02C04Verts, _gShelterB1PodServiceGantryCollision02C04Faces, _gShelterB1PodServiceGantryCollision02C04Table, 1003, -1010, 5, 3, 4000, 30 };

ViewCamera D_shelter_b1_pod_service_gantry_801801E8[46] = {
    { { { { 1403, 0, 3848 }, { 2757, 2857, -1005 }, { -2684, 2934, 978 } }, { -0x2DC3, 4293, -2180 } }, 207 },
    { { { { 693, 0, 4036 }, { 392, 4076, -67 }, { -4017, 398, 690 } }, { -7410, 1510, -2610 } }, 289 },
    { { { { 1395, 0, -3850 }, { -456, 4067, -165 }, { 3823, 485, 1386 } }, { -2490, 1520, -2540 } }, 207 },
    { { { { -3621, 0, 1914 }, { 765, 3754, 1447 }, { -1754, 1637, -3319 } }, { -0x3D54, 3550, -8260 } }, 289 },
    { { { { 3965, 0, 1025 }, { -3, 4095, 14 }, { -1025, -15, 3965 } }, { -0x3F7A, 2500, -1160 } }, 289 },
    { { { { -4017, 0, -797 }, { -627, 2530, 3159 }, { 492, 3221, -2481 } }, { -6717, 2637, -5072 } }, 329 },
    { { { { 4089, 0, -229 }, { 137, 3281, 2447 }, { 183, -2450, 3276 } }, { -6550, 170, -1790 } }, 143 },
    { { { { -4096, 0, 0 }, { 0, 2262, 3414 }, { 0, 3414, -2262 } }, { -7000, 6570, -9340 } }, 289 },
    { { { { -4095, 0, 0 }, { 0, 2763, 3023 }, { 0, 3023, -2763 } }, { -7000, 2930, -6100 } }, 289 },
    { { { { 3908, 0, 1224 }, { 276, 3990, -882 }, { -1193, 924, 3807 } }, { -8490, 2150, -770 } }, 289 },
    { { { { -1888, 0, 3634 }, { -233, 4087, -121 }, { -3627, -263, -1884 } }, { -9400, 1470, -5110 } }, 541 },
    { { { { 4095, 0, 0 }, { 0, 3947, 1094 }, { 0, -1094, 3947 } }, { -7000, 1530, -4260 } }, 289 },
    { { { { -2903, 0, -2888 }, { -254, 4080, 256 }, { 2877, 361, -2892 } }, { -0x389A, 2490, -8990 } }, 289 },
    { { { { 0, 0, 4096 }, { 1366, 3861, 0 }, { -3861, 1366, 0 } }, { -0x4538, 2880, -6500 } }, 289 },
    { { { { 4052, 0, -595 }, { 68, 4068, 468 }, { 591, -473, 4025 } }, { -0x3A98, 2050, -5600 } }, 541 },
    { { { { 2083, 0, 3526 }, { 535, 4048, -316 }, { -3485, 621, 2058 } }, { -390, 1520, 3400 } }, 282 },
    { { { { 664, 0, 4041 }, { 178, 4091, -29 }, { -4037, 181, 664 } }, { -1310, 1370, 2360 } }, 275 },
    { { { { 862, 0, 4004 }, { 1204, 3906, -259 }, { -3818, 1231, 822 } }, { -6593, 1887, 2833 } }, 246 },
    { { { { -3915, 0, -1201 }, { -30, 4094, 99 }, { 1201, 103, -3914 } }, { -7057, 873, -0x39AE } }, 269 },
    { { { { -458, 0, 4070 }, { 2704, 3060, 304 }, { -3041, 2722, -342 } }, { -5900, 5070, -8460 } }, 289 },
    { { { { 0, 0, -4096 }, { -343, 4081, 0 }, { 4081, 343, 0 } }, { 1531, 1760, -3200 } }, 853 },
    { { { { -4046, 0, -632 }, { 58, 4078, -373 }, { 629, -377, -4029 } }, { -8150, 1150, -5100 } }, 289 },
    { { { { -416, 0, 4074 }, { -259, 4087, -26 }, { -4066, -260, -415 } }, { -9288, 1352, -3496 } }, 541 },
    { { { { -331, 0, -4082 }, { 653, 4043, -53 }, { 4029, -655, -327 } }, { -3800, 800, -3800 } }, 680 },
    { { { { 2885, 0, 2906 }, { 102, 4093, -101 }, { -2905, 143, 2883 } }, { -7132, 1446, -1740 } }, 447 },
    { { { { 3776, 0, 1585 }, { 432, 3940, -1031 }, { -1525, 1118, 3633 } }, { -8348, 1907, -27 } }, 369 },
    { { { { 115, 0, -4094 }, { 87, 4095, 2 }, { 4093, -87, 115 } }, { -6000, 1508, -3300 } }, 447 },
    { { { { 2175, 0, 3470 }, { 1288, 3803, -807 }, { -3222, 1520, 2020 } }, { -9356, 2096, -1713 } }, 447 },
    { { { { -1119, 0, -3940 }, { 554, 4055, -157 }, { 3900, -576, -1108 } }, { -5100, 1200, -3750 } }, 312 },
    { { { { -394, 0, -4076 }, { 954, 3982, -92 }, { 3963, -958, -383 } }, { -4850, 800, -3750 } }, 380 },
    { { { { 3594, 0, -1963 }, { -1509, 2619, -2763 }, { 1255, 3148, 2299 } }, { -5820, 4480, -190 } }, 447 },
    { { { { 3745, 0, 1658 }, { 62, 4093, -140 }, { -1657, 153, 3742 } }, { -8710, 1080, 430 } }, 348 },
    { { { { -995, 0, 3973 }, { 105, 4094, 26 }, { -3971, 108, -995 } }, { -0x2C69, 998, -3892 } }, 358 },
    { { { { 2031, 0, 3556 }, { 1299, 3812, -742 }, { -3310, 1496, 1890 } }, { -7162, 1426, -2106 } }, 447 },
    { { { { -3914, 0, 1205 }, { 1172, 961, 3805 }, { -283, 3981, -918 } }, { -7360, 9500, -6160 } }, 257 },
    { { { { -3916, 0, 1198 }, { 1165, 960, 3807 }, { -281, 3981, -918 } }, { -7364, 9503, -6166 } }, 257 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { 606, 0, 4050 }, { 30, 4095, -4 }, { -4050, 30, 606 } }, { -9523, 1028, -2600 } }, 447 },
    { { { { 1999, 0, 3574 }, { 187, 4090, -104 }, { -3569, 214, 1996 } }, { -9422, 777, -982 } }, 329 },
    { { { { 2809, 0, 2980 }, { 1285, 3695, -1212 }, { -2688, 1766, 2535 } }, { -0x3412, 2770, -6570 } }, 289 },
    { { { { 1178, 0, -3922 }, { 529, 4058, 158 }, { 3887, -552, 1167 } }, { -1433, 500, -1642 } }, 380 },
    { { { { 1676, 0, -3737 }, { -1100, 3914, -493 }, { 3571, 1205, 1601 } }, { -7948, 590, -2568 } }, 329 },
};

SpriteSource D_shelter_b1_pod_service_gantry_80180860[23] = {
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 128, 48, 386, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 104, 56, 386, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, 64, 381, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 72, 376, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 80, 391, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, 88, 386, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 380, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 104, 376, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, 40, 1250, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, 40, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, 56, 1250, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 72, 1250, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 96, 1250, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 16, 32, 1250, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -40, 56, 1250, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 16, 56, 1250, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -32, 72, 1250, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 16, 72, 1250, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -24, 96, 1250, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 48, 8, 1250, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 32, 1250, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 56, 56, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -48, 24, 1250, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80180A2C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_service_gantry_80180A4C[8] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 0, 1300, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -104, 0, 1237, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -104, -48, 1237, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -104, -96, 1237, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -48, 1300, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -96, 1300, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -40, -96, 1350, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 0, 1300, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80180AEC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_service_gantry_80180B04[12] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 0, 2300, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -40, 2025, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -56, 2300, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 48, -56, 2125, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -56, 2015, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, -40, 2300, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 737, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, 72, 737, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 737, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 24, 737, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, 0, 737, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 8, 737, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80180BF4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b1_pod_service_gantry_80180C14[2] = {
    { { 195, 0, 125, 239 }, 2500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b1_pod_service_gantry_80180C28[82] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 80, 1075, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, 88, 1050, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -80, 88, 1050, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -16, 88, 1056, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 16, 88, 1056, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 48, 88, 1062, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 80, 88, 1081, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 112, 88, 1081, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 1081, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -80, 72, 1045, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -48, 72, 1050, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 64, 1075, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -16, 72, 1056, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 16, 64, 1057, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 48, 56, 1087, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 80, 56, 1112, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 72, 32, 1187, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 32, 1187, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 112, 56, 1112, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 144, 64, 1112, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 112, 48, 1237, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, 88, 1056, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, 8, 1050, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -80, 24, 1037, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -112, 40, 1000, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -112, 64, 1000, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 88, 1000, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 64, 962, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 88, 962, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, 40, 962, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 32, 950, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -136, 32, 962, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -80, 40, 1037, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -80, 64, 1037, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, 40, 1050, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -48, 64, 1050, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -8, 975, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 8, 962, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -48, 0, 937, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 64, 1450, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -80, 1450, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -32, 1450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 112, 16, 1450, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -80, 1450, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -32, 1450, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 16, 1450, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, -72, 1445, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -32, 1445, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 16, 1445, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 48, 64, 1445, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, 56, 1075, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 56, 1175, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 80, 1175, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, 88, 1175, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 72, 1050, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 8, 64, 1062, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 56, 1081, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 56, 1081, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 48, 1081, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 48, 1345, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 16, 56, 1317, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, 64, 1295, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, 72, 1292, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 0, 64, 1305, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 24, 56, 1330, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 48, 48, 1375, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 48, 1367, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 48, 1387, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, 56, 1330, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -16, 64, 1305, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -32, 72, 1300, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -8, 64, 1320, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, 56, 1362, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 56, 1375, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, 48, 1425, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 48, 1431, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 0, 56, 1382, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 64, 1362, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, 72, 1357, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 64, 1375, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 8, 56, 1412, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 1436, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181290[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 6, 0 } },
    { 22, 14, 0, 0, { 1, 0 } },
    { 36, 3, 0, 0, { 4, 0 } },
    { 39, 11, 0, 0, { 0, 0 } },
    { 50, 9, 0, 0, { 5, 0 } },
    { 59, 7, 0, 0, { 3, 0 } },
    { 66, 8, 0, 0, { 7, 0 } },
    { 74, 8, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_service_gantry_801812E0[15] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 32, 925, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 104, 700, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 72, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 40, 875, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 32, 925, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 112, 32, 1000, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 56, 32, 1000, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 32, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -56, 32, 1000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 48, 1000, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -40, 72, 1000, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 48, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 48, 1000, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 72, 1000, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 72, 1000, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_8018140C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_8018142C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_8018143C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_service_gantry_8018144C[10] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -152, -56, 1950, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -112, -56, 1950, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -72, -56, 1900, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -32, -48, 1800, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, -48, 1800, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 32, -56, 1900, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 72, -56, 1950, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 112, -56, 1950, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -16, 1800, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -16, 1800, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181514[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_service_gantry_8018152C[24] = {
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -64, 0, 812, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -112, -8, 800, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -8, 800, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -24, 800, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 16, 0, 812, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 64, -8, 800, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 112, -8, 800, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -24, 800, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -72, 56, 634, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -120, 56, 656, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -152, 56, 827, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 32, 649, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 24, 56, 634, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 72, 56, 656, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 120, 56, 673, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 32, 650, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -64, 0, 570, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -64, 32, 630, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -64, 64, 701, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 96, 801, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 96, 802, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 24, 64, 701, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 32, 631, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 0, 570, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_8018170C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 4, 4, 0, 0, { 3, 0 } },
    { 8, 4, 0, 0, { 2, 0 } },
    { 12, 4, 0, 0, { 4, 0 } },
    { 16, 8, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181744[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_service_gantry_80181754[2] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 88, 104, 520, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 120, 104, 511, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_8018177C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181794[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801817A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_service_gantry_801817B4[14] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 212, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 72, 212, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -64, 72, 212, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -16, 72, 212, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 32, 72, 212, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 80, 72, 212, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 72, 212, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 80, 32, 212, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 32, 48, 212, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 24, 212, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -16, 48, 212, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -64, 48, 212, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 40, 212, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -112, 48, 212, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801818CC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801818E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801818F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181904[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181914[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181924[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181934[2] = {
    { 0, 0, 1, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181944[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181954[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181964[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181974[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181984[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181994[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801819A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801819B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801819C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801819D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801819E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_801819F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A64[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A74[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A84[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181A94[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181AA4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181AB4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_pod_service_gantry_80181AC4[9] = {
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 0, 902, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 72, 915, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -128, 96, 875, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, -72, 96, 875, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 120, -96, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, -24, 625, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, 24, 625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 56 } }, 72, 64, 625, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, 0, 96, 875, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181B78[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_pod_service_gantry_80181B90[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_pod_service_gantry_80181BA0[46] = {
    { { .elements = D_shelter_b1_pod_service_gantry_80180860 }, D_shelter_b1_pod_service_gantry_80180A2C, NULL },
    { { .elements = D_shelter_b1_pod_service_gantry_80180A4C }, D_shelter_b1_pod_service_gantry_80180AEC, NULL },
    { { .elements = D_shelter_b1_pod_service_gantry_80180B04 }, D_shelter_b1_pod_service_gantry_80180BF4, D_shelter_b1_pod_service_gantry_80180C14 },
    { { .elements = D_shelter_b1_pod_service_gantry_80180C28 }, D_shelter_b1_pod_service_gantry_80181290, NULL },
    { { .elements = D_shelter_b1_pod_service_gantry_801812E0 }, D_shelter_b1_pod_service_gantry_8018140C, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_8018142C }, D_shelter_b1_pod_service_gantry_8018142C, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_8018143C }, D_shelter_b1_pod_service_gantry_8018143C, NULL },
    { { .elements = D_shelter_b1_pod_service_gantry_8018144C }, D_shelter_b1_pod_service_gantry_80181514, NULL },
    { { .elements = D_shelter_b1_pod_service_gantry_8018152C }, D_shelter_b1_pod_service_gantry_8018170C, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181744 }, D_shelter_b1_pod_service_gantry_80181744, NULL },
    { { .elements = D_shelter_b1_pod_service_gantry_80181754 }, D_shelter_b1_pod_service_gantry_8018177C, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181794 }, D_shelter_b1_pod_service_gantry_80181794, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801817A4 }, D_shelter_b1_pod_service_gantry_801817A4, NULL },
    { { .elements = D_shelter_b1_pod_service_gantry_801817B4 }, D_shelter_b1_pod_service_gantry_801818CC, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801818E4 }, D_shelter_b1_pod_service_gantry_801818E4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801818F4 }, D_shelter_b1_pod_service_gantry_801818F4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181904 }, D_shelter_b1_pod_service_gantry_80181904, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181914 }, D_shelter_b1_pod_service_gantry_80181914, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181924 }, D_shelter_b1_pod_service_gantry_80181924, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181934 }, D_shelter_b1_pod_service_gantry_80181934, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181944 }, D_shelter_b1_pod_service_gantry_80181944, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181954 }, D_shelter_b1_pod_service_gantry_80181954, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181964 }, D_shelter_b1_pod_service_gantry_80181964, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181974 }, D_shelter_b1_pod_service_gantry_80181974, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181984 }, D_shelter_b1_pod_service_gantry_80181984, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181994 }, D_shelter_b1_pod_service_gantry_80181994, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801819A4 }, D_shelter_b1_pod_service_gantry_801819A4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801819B4 }, D_shelter_b1_pod_service_gantry_801819B4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801819C4 }, D_shelter_b1_pod_service_gantry_801819C4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801819D4 }, D_shelter_b1_pod_service_gantry_801819D4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801819E4 }, D_shelter_b1_pod_service_gantry_801819E4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_801819F4 }, D_shelter_b1_pod_service_gantry_801819F4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A04 }, D_shelter_b1_pod_service_gantry_80181A04, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A14 }, D_shelter_b1_pod_service_gantry_80181A14, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A24 }, D_shelter_b1_pod_service_gantry_80181A24, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A34 }, D_shelter_b1_pod_service_gantry_80181A34, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A44 }, D_shelter_b1_pod_service_gantry_80181A44, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A54 }, D_shelter_b1_pod_service_gantry_80181A54, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A64 }, D_shelter_b1_pod_service_gantry_80181A64, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A74 }, D_shelter_b1_pod_service_gantry_80181A74, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A84 }, D_shelter_b1_pod_service_gantry_80181A84, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181A94 }, D_shelter_b1_pod_service_gantry_80181A94, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181AA4 }, D_shelter_b1_pod_service_gantry_80181AA4, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181AB4 }, D_shelter_b1_pod_service_gantry_80181AB4, NULL },
    { { .elements = D_shelter_b1_pod_service_gantry_80181AC4 }, D_shelter_b1_pod_service_gantry_80181B78, NULL },
    { { .empty = D_shelter_b1_pod_service_gantry_80181B90 }, D_shelter_b1_pod_service_gantry_80181B90, NULL },
};

WorldCoordPointLight D_shelter_b1_pod_service_gantry_80181DC8[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x310C, -2576, 7191 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1756, 3216, 2480 }, { 0, 0 } }, 100, 200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x309F, -2482, 7439 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1019, 3392, 1019 }, { 0, 0 } }, 100, 461 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x38B7, -3643, 7891 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2170, 2209, 2195 }, { 0, 0 } }, 1878, 3562 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4133, -2789, 3232 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2983, 2975, 2968 }, { 0, 0 } }, 1970, 6109 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8491, -3002, 3193 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2615, 2612, 2602 }, { 0, 0 } }, 3836, 6579 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x33FB, -2300, 0x2736 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2435, 3392, 2909 }, { 0, 0 } }, 300, 800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4074, -2305, 6109 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2059, 4119, 2889 }, { 0, 0 } }, 401, 855 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41F1, -1881, 7349 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1756, 3216, 2480 }, { 0, 0 } }, 100, 200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x35EA, -1128, 2778 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1944, 1895, 1866 }, { 0, 0 } }, 5201, 9559 },
};

ShelterB1PodServiceGantrySpotLightStorage D_shelter_b1_pod_service_gantry_80182128 = {
    .coneLights = {
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -4003, -2, 873 }, { 869, 266, 4000 }, { -59, 4087, -261 } }, { 3412, -0x5063, 9454 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2764, 2764, 2425 }, { 0, 0 } }, { 872, 3993, -260, 0 }, 0x4E20, 0x7530, 0x2C71 },
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -4092, 0, -222 }, { 215, -909, -3996 }, { -50, -3996, 908 } }, { 8054, 5038, 1813 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 226, 226, 198 }, { 0, 0 } }, { -221, -3988, 907, 0 }, 10, 0x4E20, 0x2C71 },
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

WorldCoordRoomLights D_shelter_b1_pod_service_gantry_801824F4 = { 0, NULL, ARRAY_SIZE(D_shelter_b1_pod_service_gantry_80181DC8), D_shelter_b1_pod_service_gantry_80181DC8, ARRAY_SIZE(D_shelter_b1_pod_service_gantry_80182128.coneLights), D_shelter_b1_pod_service_gantry_80182128.coneLights };

static void func_shelter_b1_pod_service_gantry_8017D628(Task* task)
{
    u8                              param1[4];
    u8                              param2[4];
    s32                             poll;
    _ShelterB1PodServiceGantryWork* work = task->work;

    switch (work->step) {
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_START_FIRST_SCENE:
            work->sceneTask = taskSpawnFromTable(D_actor_560800_801718F0, 0, 0, 0);
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_AWAIT_FIRST_SCENE:
            if (taskPollKill(work->sceneTask, &poll) == 0) {
                break;
            }
            work->sceneTask = taskSpawnFromTable(D_actor_560800_8016EA28, 0, 0, 0);
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_LOAD_SECOND_SCENE:
            param1[2] = 0x10;
            param1[3] = 0;
            param1[0] = 0;
            param2[0] = 9;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_START_SECOND_SCENE:
            if (cdCmdIsIdle() == 0) {
                break;
            }
            work->sceneTask = taskSpawnFromTable(D_actor_160900_8013FB50, 0, 0, 0);
            Gp_ApplyAreaRecs(D_shelter_b1_pod_service_gantry_80182540);
            gameFlagSetNibble(GAME_FLAG_118, 1);
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_AWAIT_SECOND_SCENE:
            if (taskPollKill(work->sceneTask, &poll) == 0) {
                break;
            }
            gGameSession->unknown_138                                   = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_MINE_SHELTER;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 2;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(0, 0x11, 0, 0);
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_PAUSE:
            work->step++;
            break;
        case SHELTER_B1_POD_SERVICE_GANTRY_STEP_LEAVING:
            break;
    }
}

s32 func_shelter_b1_pod_service_gantry_8017D7C0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler of message 0x13EE in the room's message table: copies the incoming
/// record onto the outgoing one, passes both to `mapShelterRoomVariantResolve` and returns 1.
s32 func_shelter_b1_pod_service_gantry_8017D7C8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    return 1;
}

s32 func_shelter_b1_pod_service_gantry_8017D80C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_pod_service_gantry_8017D814(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

static void func_shelter_b1_pod_service_gantry_8017D81C(Task* arg0)
{
    _ShelterB1PodServiceGantryWork* work;

    arg0->msgTable = D_shelter_b1_pod_service_gantry_8017FAF4;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    work       = memMalloc(sizeof(*work), false);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    memFillBytes(work, 0U, sizeof(*work));
    SetDispMask(0);
    arg0->state += 1;
}

/// The room task: copies its three-state table
/// `D_shelter_b1_pod_service_gantry_8017D5C4` onto the stack and calls the
/// entry for the task's current state.
void func_shelter_b1_pod_service_gantry_8017D89C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_pod_service_gantry_8017D5C4;
    sp.funcs[task->state](task);
}
