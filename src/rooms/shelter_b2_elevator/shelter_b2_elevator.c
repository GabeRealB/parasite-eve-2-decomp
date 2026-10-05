#include "rooms/shelter_b2_elevator.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
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

/// World Z of both elevator-door leaves when the door is shut: the line the
/// two leaves meet on.
#define SHELTER_B2_ELEVATOR_DOOR_CLOSED_Z (-500)

/// World units a door leaf slides per frame while the door opens or closes.
#define SHELTER_B2_ELEVATOR_DOOR_LEAF_SPEED 10

/// Travel of a fully retracted door leaf, in world units: one leaf's width.
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

extern EvsCommand D_actor_142900_801378D0[];
extern EvsCommand D_actor_142900_801380F8[];

/// The room's message table, installed on the room entry task.
extern TaskMessageEntry D_shelter_b2_elevator_8017DFA0[];

/// The room's spawnable tasks: the two door leaves, then the exit task.
extern TaskDesc D_shelter_b2_elevator_8017DF70[];

/// The two door-leaf tasks the room entry task spawns.
extern Task* D_shelter_b2_elevator_8017EA00[];

static void func_shelter_b2_elevator_8017DB08(Task* task);

s32  func_shelter_b2_elevator_8017DA5C(Task*, s32, s32, s32);
s32  func_shelter_b2_elevator_8017DA64(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b2_elevator_8017DAA8(Task*, s32, s32, s32);
s32  func_shelter_b2_elevator_8017DAB0(Task*, s32, s32, s32);
s32  func_shelter_b2_elevator_8017DAB8(Task*, s32, s32, s32);
s32  func_shelter_b2_elevator_8017DAE0(Task*, s32, s32, s32);
void func_shelter_b2_elevator_8017D70C(Task*);
void func_shelter_b2_elevator_8017D888(Task*);

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

TaskDesc D_shelter_b2_elevator_8017DF70[4] = {
    { { { TASK_BODY_TMD, 192 } }, func_shelter_b2_elevator_8017D70C, { .model = &_gShelterB2ElevatorModel00688 } },
    { { { TASK_BODY_TMD, 192 } }, func_shelter_b2_elevator_8017D70C, { .model = &_gShelterB2ElevatorModel00884 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b2_elevator_8017D888, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_shelter_b2_elevator_8017DFA0[7] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_elevator_8017DA64 },
    { 5105, func_shelter_b2_elevator_8017DA5C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b2_elevator_8017DAB0 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b2_elevator_8017DAA8 },
    { 5100, func_shelter_b2_elevator_8017DAB8 },
    { 5101, func_shelter_b2_elevator_8017DAE0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* D_shelter_b2_elevator_8017DFD8[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b2_elevator_8017DFDC[1] = { 3 };

DirectionWarpEntry D_shelter_b2_elevator_8017DFE0[1] = {
    { { { .word = 1024 }, 0x2CEA, 0, -496 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 0x2CEA, 0, -496 }, { 0, 0, 0, 0 }, 0x541A0002, 0x541A0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB2ElevatorCollision00B24Normals[6] = {
#include "assets/shelter_b2_elevator_collision_00B24_normals.inc"
};

static SVECTOR _gShelterB2ElevatorCollision00B24Verts[8] = {
#include "assets/shelter_b2_elevator_collision_00B24_verts.inc"
};

static WorldCollisionGridFace _gShelterB2ElevatorCollision00B24Faces[6] = {
#include "assets/shelter_b2_elevator_collision_00B24_faces.inc"
};

static s16 _gShelterB2ElevatorCollision00B24Cells[8] = {
#include "assets/shelter_b2_elevator_collision_00B24_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2ElevatorCollision00B24Cells[i])
static s16* _gShelterB2ElevatorCollision00B24Table[1] = {
#include "assets/shelter_b2_elevator_collision_00B24_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_elevator_8017E0E4 = { NULL, _gShelterB2ElevatorCollision00B24Normals, _gShelterB2ElevatorCollision00B24Verts, _gShelterB2ElevatorCollision00B24Faces, _gShelterB2ElevatorCollision00B24Table, -0x2AF8, 1450, 1, 1, 4000, 6 };

ViewCamera D_shelter_b2_elevator_8017E108[15] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2EE0, 0x7530, 450 } }, 2748 },
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

SpriteBatch D_shelter_b2_elevator_8017E324[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_elevator_8017E334[56] = {
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 104, -24, 0, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -48, 0, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -64, 0, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, -80, 0, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, -8, -120, 0, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -16, -120, 0, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 32, -120, 0, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 40, -120, 0, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -120, 0, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, -64, 0, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -112, -64, 0, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, -56, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -48, 0, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, -32, 0, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -16, 0, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -16, 0, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -16, 0, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, -120, 461, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 56, -120, 519, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 104, -80, 484, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 96, -64, 524, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 88, -48, 592, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 72, -24, 638, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -32, 756, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 40, -16, 698, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, -40, 730, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, -56, 683, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -48, 704, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, -64, 535, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -88, 529, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -80, 540, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -32, -40, 782, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, -64, 727, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -56, -120, 550, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, -16, 0, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, -24, 707, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -32, 685, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, -40, 774, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, -48, 691, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -48, 640, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -56, -64, 745, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -80, -64, 673, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, -80, 569, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -80, 531, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -96, 490, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -88, 552, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -96, 558, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, -96, 548, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, -120, 472, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -120, 524, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -120, 566, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -120, 555, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -88, 589, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -88, 598, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -80, 560, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -80, 581, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_elevator_8017E794[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 56, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_8017E7AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b2_elevator_8017E7BC[3] = {
    { { .empty = D_shelter_b2_elevator_8017E324 }, D_shelter_b2_elevator_8017E324, NULL },
    { { .elements = D_shelter_b2_elevator_8017E334 }, D_shelter_b2_elevator_8017E794, NULL },
    { { .empty = D_shelter_b2_elevator_8017E7AC }, D_shelter_b2_elevator_8017E7AC, NULL },
};

WorldCoordPointLight D_shelter_b2_elevator_8017E7E0[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2FD1, -1742, -381 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2950, 3487, 3402 }, { 0, 0 } }, 679, 5240 },
};

WorldCoordRoomLights D_shelter_b2_elevator_8017E840 = { 0, NULL, ARRAY_SIZE(D_shelter_b2_elevator_8017E7E0), D_shelter_b2_elevator_8017E7E0, 0, NULL };

WorldCollisionTrigger D_shelter_b2_elevator_8017E858[2] = {
    { NULL, NULL, NULL, { 3454, -1167, 45, 0 }, { { -10, -1520, 2263, 0 }, { 11, -1520, -2263, 0 }, { -10, 1520, 2263, 0 }, { 11, 1520, -2263, 0 } }, { -4099, 0, -20, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3311, -1152, 47, 0 }, { { 27, -1520, -2262, 0 }, { -26, -1520, 2263, 0 }, { 27, 1520, -2262, 0 }, { -26, 1520, 2263, 0 } }, { 4095, 0, 47, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_elevator_8017E8F0[1] = {
    { NULL, NULL, NULL, { 0x2BF0, -48, -448, 0 }, { { -336, 0, -1024, 0 }, { 336, 0, -1024, 0 }, { -336, 0, 1024, 0 }, { 336, 0, 1024, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_WARP, 27, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_elevator_8017E93C[2] = {
    { 101, 429, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_142900_80137600 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_elevator_8017E954[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_elevator_8017E964[11] = {
    { NULL, NULL },
    { D_shelter_b2_elevator_8017E954, D_shelter_b2_elevator_8017E93C },
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

s32 D_shelter_b2_elevator_8017E9BC[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b2_elevator_8017E9C8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_elevator_8017E9D0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_shelter_b2_elevator_8017E9D8[8] = {
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9D0,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
};

AreaApplyRec D_shelter_b2_elevator_8017E9F8[2] = {
    { 4, 44, 4, 1 },
    { 255, 0, 0, 0 },
};

Task* D_shelter_b2_elevator_8017EA00[2];

static __inline__ Task* ShelterElevator_SpawnTask(s32 index, s32 direction);
static void             func_shelter_b2_elevator_8017D5E8(Task* task);

/// The room entry task's first state: installs the room's message table, takes
/// pointer slot 7 and spawns the two door leaves. Unless the byte
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene` is 9, it then either runs the first-visit sequence, setting
/// event nibble 0xCF, or on a later visit hides the HUD, spawns the exit task
/// and runs CAP command 3.
/// Spawn one of this room's task descriptors with its signed travel direction.
static __inline__ Task* ShelterElevator_SpawnTask(s32 index, s32 direction)
{
    return taskSpawnFromTable(D_shelter_b2_elevator_8017DF70, index, 0, direction);
}

static void func_shelter_b2_elevator_8017D5E8(Task* task)
{
    task->msgTable = D_shelter_b2_elevator_8017DFA0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    D_shelter_b2_elevator_8017EA00[0] = ShelterElevator_SpawnTask(0, -1);
    D_shelter_b2_elevator_8017EA00[1] = ShelterElevator_SpawnTask(1, 1);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        if (gameFlagGetNibble(GAME_FLAG_0CF) == 0) {
            gameFlagSetNibble(GAME_FLAG_0CF, 1);
            func_800E8634(D_actor_142900_801378D0, 0, D_actor_142900_801380F8);
            func_800E3FAC(0xA2, 0x24);
        } else {
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            ShelterElevator_SpawnTask(2, 0);
            Gp_RunCapCmd(3, 0);
        }
    }
    task->state++;
}

/// The task of one leaf of the elevator's sliding door. The first frame
/// allocates its `_ShelterB2ElevatorDoorLeafWork` and places the model shut;
/// every later frame slides the leaf by `spawnArg1` steps, keeps its travel
/// within the opening, offsets the model along Z to the side `spawnArg2`
/// names, and submits the model, with object flag 0x80 set except in camera
/// view 2.
void func_shelter_b2_elevator_8017D70C(Task* task)
{
    TmdObject*                      obj;
    GfxCoord*                       coord;
    _ShelterB2ElevatorDoorLeafWork* work;
    VECTOR                          vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    switch (task->state) {
        case 0:
            work = memCalloc(sizeof(_ShelterB2ElevatorDoorLeafWork), 0);
            if (work == NULL) {
                taskKill(task);
                return;
            }
            task->work          = work;
            work->travel        = 0;
            obj->otOffset       = 0x64;
            obj->flags          = 0;
            coord->parent       = &gGfxViewCoord;
            coord->coord.t[0]   = 0x2A94;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = SHELTER_B2_ELEVATOR_DOOR_CLOSED_Z;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state++;
            break;
        case 1:
            work          = task->work;
            work->travel += task->spawnArg1.value * SHELTER_B2_ELEVATOR_DOOR_LEAF_SPEED;
            if (work->travel < 0) {
                work->travel = 0;
            }
            if (work->travel > SHELTER_B2_ELEVATOR_DOOR_LEAF_TRAVEL_MAX) {
                work->travel = SHELTER_B2_ELEVATOR_DOOR_LEAF_TRAVEL_MAX;
            }
            coord->coord.t[2] = (work->travel * task->spawnArg2.value) + SHELTER_B2_ELEVATOR_DOOR_CLOSED_Z;
            if (gGameSession->location.loc.view == 2) {
                obj->flags = 0;
            } else {
                obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            func_800D7A9C(obj, &vec, 0, 3);
            break;
    }
}

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_shelter_b2_elevator_8017D5C4 = {
    { func_shelter_b2_elevator_8017D5E8, func_shelter_b2_elevator_8017DB08, taskKill },
};

/// The exit task. After 21 frames and once the CAP script is idle, it sets the
/// destination area and warp from the event key the script chose (0xB, 0xC or
/// 0xD), then resolves the destination through `func_map_shelter_80179A04`, spawns task
/// 0x11 and ends.
void func_shelter_b2_elevator_8017D888(Task* task)
{
    RoomEventMsg msg;
    RoomEventMsg msg2;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            if (task->killCountdown >= 0x15) {
                task->state++;
            }
            task->killCountdown = task->killCountdown + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            switch (Gp_GetCapEventKey()) {
                case 0xB:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B1_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
                    break;
                case 0xC:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B2_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 2;
                    break;
                case 0xD:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B3_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
                    break;
            }
            task->state++;
            break;
        case 3:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0);
            msg.queryOnly = ROOM_EVENT_EXECUTE;
            msg.areaId    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area;
            msg.warp      = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp;
            msg.room      = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room;
            msg2          = msg;
            func_map_shelter_80179A04(&msg, &msg2);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = msg2.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = msg2.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Message-table handler for message 0x13F1. Does nothing.
s32 func_shelter_b2_elevator_8017DA5C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message-table handler for message 0x13EE: copies the incoming record onto
/// the outgoing one and passes both to `func_map_shelter_80179A04`. Always returns 1.
s32 func_shelter_b2_elevator_8017DA64(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

/// Message-table handler for message 0x13F0. Does nothing.
s32 func_shelter_b2_elevator_8017DAA8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message-table handler for message 0x13EF. Does nothing.
s32 func_shelter_b2_elevator_8017DAB0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message-table handler for message 0x13EC: sets `spawnArg1` of both door
/// leaves to 1, opening the door.
s32 func_shelter_b2_elevator_8017DAB8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    D_shelter_b2_elevator_8017EA00[0]->spawnArg1.value = 1;
    D_shelter_b2_elevator_8017EA00[1]->spawnArg1.value = 1;
    return 0;
}

/// Message-table handler for message 0x13ED: sets `spawnArg1` of both door
/// leaves to -1, closing the door.
s32 func_shelter_b2_elevator_8017DAE0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    D_shelter_b2_elevator_8017EA00[0]->spawnArg1.value = -1;
    D_shelter_b2_elevator_8017EA00[1]->spawnArg1.value = -1;
    return 0;
}

/// The room entry task's idle state.
static void func_shelter_b2_elevator_8017DB08(Task* task)
{
    char pad[0x10];
}

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_shelter_b2_elevator_8017DB18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_elevator_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_b2_elevator_8017DB70(Task* unused)
{
}
