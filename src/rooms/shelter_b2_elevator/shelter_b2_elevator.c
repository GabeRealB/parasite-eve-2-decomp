#include "rooms/shelter_b2_elevator.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
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

/// Per-task state of an elevator car: its travel, kept within 0..500.
typedef struct {
    s32 travel;
} ShelterElevatorCar;

extern s32 D_801378D0;
extern s32 D_801380F8;

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_shelter_b2_elevator_8017DFA0[];

/// The room's spawnable tasks: two elevator cars, then the exit task.
extern TaskDesc D_shelter_b2_elevator_8017DF70[];

/// The two elevator-car tasks the room entry task spawns.
extern Task* D_shelter_b2_elevator_8017EA00[];

static void func_shelter_b2_elevator_8017DB08(Task* task);

s32  func_shelter_b2_elevator_8017DA5C(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b2_elevator_8017DA64(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b2_elevator_8017DAA8(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b2_elevator_8017DAB0(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b2_elevator_8017DAB8(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b2_elevator_8017DAE0(Task*, s32, GpMessageArg, GpMessageArg);
void func_shelter_b2_elevator_8017D70C(Task*);
void func_shelter_b2_elevator_8017D888(Task*);

TmdBone D_shelter_b2_elevator_8017DB78[1] = {
#include "assets/shelter_b2_elevator_model_00790_skeleton.inc"
};

u32 D_shelter_b2_elevator_8017DB9C[1] = {
#include "assets/shelter_b2_elevator_model_00790_partVerts.inc"
};

SVECTOR D_shelter_b2_elevator_8017DBA0[21] = {
#include "assets/shelter_b2_elevator_model_00790_verts.inc"
};

u32 D_shelter_b2_elevator_8017DC48[66] = {
#include "assets/shelter_b2_elevator_model_00790_stream.inc"
};

TmdSource D_shelter_b2_elevator_8017DD50 = {
    0,
    480,
    0,
    1,
    D_shelter_b2_elevator_8017DB9C,
    D_shelter_b2_elevator_8017DBA0,
    &D_shelter_b2_elevator_8017DBA0[21],
    D_shelter_b2_elevator_8017DB78,
    D_shelter_b2_elevator_8017DC48,
};

TmdBone D_shelter_b2_elevator_8017DD74[1] = {
#include "assets/shelter_b2_elevator_model_0098C_skeleton.inc"
};

u32 D_shelter_b2_elevator_8017DD98[1] = {
#include "assets/shelter_b2_elevator_model_0098C_partVerts.inc"
};

SVECTOR D_shelter_b2_elevator_8017DD9C[21] = {
#include "assets/shelter_b2_elevator_model_0098C_verts.inc"
};

u32 D_shelter_b2_elevator_8017DE44[66] = {
#include "assets/shelter_b2_elevator_model_0098C_stream.inc"
};

TmdSource D_shelter_b2_elevator_8017DF4C = {
    0,
    480,
    0,
    1,
    D_shelter_b2_elevator_8017DD98,
    D_shelter_b2_elevator_8017DD9C,
    &D_shelter_b2_elevator_8017DD9C[21],
    D_shelter_b2_elevator_8017DD74,
    D_shelter_b2_elevator_8017DE44,
};

TaskDesc D_shelter_b2_elevator_8017DF70[4] = {
    { 1, 192, func_shelter_b2_elevator_8017D70C, { .model = &D_shelter_b2_elevator_8017DD50 } },
    { 1, 192, func_shelter_b2_elevator_8017D70C, { .model = &D_shelter_b2_elevator_8017DF4C } },
    { 0, 32, func_shelter_b2_elevator_8017D888, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_shelter_b2_elevator_8017DFA0[7] = {
    { 5102, func_shelter_b2_elevator_8017DA64 },
    { 5105, func_shelter_b2_elevator_8017DA5C },
    { 5103, func_shelter_b2_elevator_8017DAB0 },
    { 5104, func_shelter_b2_elevator_8017DAA8 },
    { 5100, func_shelter_b2_elevator_8017DAB8 },
    { 5101, func_shelter_b2_elevator_8017DAE0 },
    { 0x7FFFFFFF, NULL },
};

u8* D_shelter_b2_elevator_8017DFD8[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b2_elevator_8017DFDC[1] = {
    { { .bytes = { 3, 0 } } },
};

GpWarpRec D_shelter_b2_elevator_8017DFE0[1] = {
    { { .words = { 1024, 0x2CEA, 0, -496 } }, { 0, 0, 0, 0 }, { .words = { 1024, 0x2CEA, 0, -496 } }, { 0, 0, 0, 0 }, 0x541A0002, 0x541A0001, 0, 2, 0, 0 },
};

SVECTOR D_shelter_b2_elevator_8017E018[6] = {
#include "assets/shelter_b2_elevator_collision_00B24_normals.inc"
};

SVECTOR D_shelter_b2_elevator_8017E048[8] = {
#include "assets/shelter_b2_elevator_collision_00B24_verts.inc"
};

GpGridFace D_shelter_b2_elevator_8017E088[6] = {
#include "assets/shelter_b2_elevator_collision_00B24_faces.inc"
};

s16 D_shelter_b2_elevator_8017E0D0[8] = {
#include "assets/shelter_b2_elevator_collision_00B24_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b2_elevator_8017E0D0[i])
s16* D_shelter_b2_elevator_8017E0E0[1] = {
#include "assets/shelter_b2_elevator_collision_00B24_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b2_elevator_8017E0E4 = { NULL, D_shelter_b2_elevator_8017E018, D_shelter_b2_elevator_8017E048, D_shelter_b2_elevator_8017E088, D_shelter_b2_elevator_8017E0E0, -0x2AF8, 1450, 1, 1, 4000, 6 };

GpViewRec D_shelter_b2_elevator_8017E108[15] = {
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

GpSprtElem D_shelter_b2_elevator_8017E334[56] = {
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

GpSprtRec D_shelter_b2_elevator_8017E7BC[3] = {
    { { .empty = D_shelter_b2_elevator_8017E324 }, D_shelter_b2_elevator_8017E324, NULL },
    { { .elements = D_shelter_b2_elevator_8017E334 }, D_shelter_b2_elevator_8017E794, NULL },
    { { .empty = D_shelter_b2_elevator_8017E7AC }, D_shelter_b2_elevator_8017E7AC, NULL },
};

GpPointLight D_shelter_b2_elevator_8017E7E0[1] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2FD1, -1742, -381 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2950, 3487, 3402, { 0, 0 } }, 679, 5240 },
};

GpRoomCoordSet D_shelter_b2_elevator_8017E840 = { 0, NULL, 1, D_shelter_b2_elevator_8017E7E0, 0, NULL };

GpObj4C D_shelter_b2_elevator_8017E858[2] = {
    { NULL, NULL, NULL, { 3454, -1167, 45, 0 }, { { -10, -1520, 2263, 0 }, { 11, -1520, -2263, 0 }, { -10, 1520, 2263, 0 }, { 11, 1520, -2263, 0 } }, { -4099, 0, -20, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3311, -1152, 47, 0 }, { { 27, -1520, -2262, 0 }, { -26, -1520, 2263, 0 }, { 27, 1520, -2262, 0 }, { -26, 1520, 2263, 0 } }, { 4095, 0, 47, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 2, 3, 129, 0 },
};

GpObj4C D_shelter_b2_elevator_8017E8F0[1] = {
    { NULL, NULL, NULL, { 0x2BF0, -48, -448, 0 }, { { -336, 0, -1024, 0 }, { 336, 0, -1024, 0 }, { -336, 0, 1024, 0 }, { 336, 0, 1024, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1070, 0, 27, 18, 130, 0 },
};

GpAreaTmdRec D_shelter_b2_elevator_8017E93C[2] = {
    { 101, 429, 0, 0, { 0, 0 }, D_80137600 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_elevator_8017E954[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b2_elevator_8017E964[11] = {
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

GpRoomParamRec D_shelter_b2_elevator_8017E9C8[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b2_elevator_8017E9D0[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec* D_shelter_b2_elevator_8017E9D8[8] = {
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9D0,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
    D_shelter_b2_elevator_8017E9C8,
};

GpAreaApplyRec D_shelter_b2_elevator_8017E9F8[2] = {
    { 4, 44, 4, 1 },
    { 255, 0, 0, 0 },
};

Task* D_shelter_b2_elevator_8017EA00[2];

static __inline__ Task* ShelterElevator_SpawnTask(s32 index, s32 direction);
static void             func_shelter_b2_elevator_8017D5E8(Task* task);

/// The room entry task's first state: installs the room's message table, takes
/// pointer slot 7 and spawns the two elevator cars. Unless the byte
/// `Mc_SaveData[0].state.demoScene` is 9, it then either runs the first-visit sequence, setting
/// event nibble 0xCF, or on a later visit hides the HUD, spawns the exit task
/// and runs CAP command 3.
/// Spawn one of this room's task descriptors with its signed travel direction.
static __inline__ Task* ShelterElevator_SpawnTask(s32 index, s32 direction)
{
    return Task_SpawnFromTable(D_shelter_b2_elevator_8017DF70, index, 0, direction);
}

static void func_shelter_b2_elevator_8017D5E8(Task* task)
{
    task->msgTable = D_shelter_b2_elevator_8017DFA0;
    Game_SetPtrSlot(task, 7);
    D_shelter_b2_elevator_8017EA00[0] = ShelterElevator_SpawnTask(0, -1);
    D_shelter_b2_elevator_8017EA00[1] = ShelterElevator_SpawnTask(1, 1);
    if (Mc_SaveData[0].state.demoScene != 9) {
        if (GameFlag_GetNibble(0xCF) == 0) {
            GameFlag_SetNibble(0xCF, 1);
            func_800E8634(&D_801378D0, 0, &D_801380F8);
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

/// An elevator car's task. The first frame allocates its state and places the
/// model; every later frame adds `spawnArg1` * 10 to the travel, clamps it to
/// 0..500, sets the model's z from the travel times `spawnArg2`, and submits
/// the model, with object flag 0x80 set except in camera view 2.
void func_shelter_b2_elevator_8017D70C(Task* task)
{
    TmdObject*          obj;
    GfxCoord*           coord;
    ShelterElevatorCar* car;
    VECTOR              vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    switch (task->state) {
        case 0:
            car = memCalloc(4, 0);
            if (car == NULL) {
                taskKill(task);
                return;
            }
            task->work          = (TaskIdMap*)car;
            car->travel         = 0;
            obj->otOffset       = 0x64;
            obj->flags          = 0;
            coord->parent       = &gGfxViewCoord;
            coord->coord.t[0]   = 0x2A94;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = -0x1F4;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state++;
            break;
        case 1:
            car         = (ShelterElevatorCar*)task->work;
            car->travel = car->travel + task->spawnArg1.value * 10;
            if (car->travel < 0) {
                car->travel = 0;
            }
            if (car->travel >= 0x1F5) {
                car->travel = 0x1F4;
            }
            coord->coord.t[2] = car->travel * task->spawnArg2.value - 0x1F4;
            if (gGameSession->at4.loc.view == 2) {
                obj->flags = 0;
            } else {
                obj->flags = TMD_OBJECT_HIDDEN;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
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
                    Mc_SaveData[0].state.at4.loc.area = 9;
                    Mc_SaveData[0].state.at4.loc.warp = 3;
                    break;
                case 0xC:
                    Mc_SaveData[0].state.at4.loc.area = 0x1B;
                    Mc_SaveData[0].state.at4.loc.warp = 2;
                    break;
                case 0xD:
                    Mc_SaveData[0].state.at4.loc.area = 0x2A;
                    Mc_SaveData[0].state.at4.loc.warp = 3;
                    break;
            }
            task->state++;
            break;
        case 3:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            msg.field_5       = 0;
            msg.prefix.packed = Mc_SaveData[0].state.at4.loc.area;
            msg.field_2       = Mc_SaveData[0].state.at4.loc.warp;
            msg.field_3       = Mc_SaveData[0].state.at4.loc.room;
            msg2              = msg;
            func_map_shelter_80179A04(&msg, &msg2);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.warp = msg2.field_2;
            Mc_SaveData[0].state.at4.loc.room = msg2.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Message-table handler for message 0x13F1. Does nothing.
s32 func_shelter_b2_elevator_8017DA5C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
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
s32 func_shelter_b2_elevator_8017DAA8(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for message 0x13EF. Does nothing.
s32 func_shelter_b2_elevator_8017DAB0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for message 0x13EC: sets `spawnArg1` of both elevator
/// cars to 1.
s32 func_shelter_b2_elevator_8017DAB8(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    D_shelter_b2_elevator_8017EA00[0]->spawnArg1.value = 1;
    D_shelter_b2_elevator_8017EA00[1]->spawnArg1.value = 1;
    return 0;
}

/// Message-table handler for message 0x13ED: sets `spawnArg1` of both elevator
/// cars to -1.
s32 func_shelter_b2_elevator_8017DAE0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
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
