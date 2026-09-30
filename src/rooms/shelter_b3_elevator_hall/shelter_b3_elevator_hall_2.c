#include "rooms/shelter_b3_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "shelter_b3_elevator_hall_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#define ROOM_EVENT_ACTIVE gRoomEventActive[0]
#include "../../shared/room_events.h"

extern TaskDesc D_80142604;

#include "../../shared/room_visual_effects_disc_data.inc.c"

u8* D_shelter_b3_elevator_hall_80182B54[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b3_elevator_hall_80182B58[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_shelter_b3_elevator_hall_80182B5C[3] = {
    { { .words = { 2048, -6047, 0, 1500 } }, { 0, 0, 0, 0 }, { .words = { 3328, -5000, 0, -200 } }, { 0, 0, 0, 0 }, 0x542A0004, 0x542A0003, 0x542A0005, 2, 0, 459 },
    { { .words = { 3072, 6749, 0, -300 } }, { 0, 0, 0, 0 }, { .words = { 2304, 6400, 0, 500 } }, { 0, 0, 0, 0 }, 0, 0, 0, 6, 0, 0 },
    { { .words = { 3072, -4500, 0, 920 } }, { 0, 0, 0, 0 }, { .words = { 3072, -4500, 0, 920 } }, { 0, 0, 0, 0 }, 0x542A0002, 0, 0, 2, 0, 443 },
};

SVECTOR D_shelter_b3_elevator_hall_80182C04[9] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_normals.inc"
};

SVECTOR D_shelter_b3_elevator_hall_80182C4C[103] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_verts.inc"
};

GpGridFace D_shelter_b3_elevator_hall_80182F84[68] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_faces.inc"
};

s16 D_shelter_b3_elevator_hall_801832B4[236] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b3_elevator_hall_801832B4[i])
s16* D_shelter_b3_elevator_hall_8018348C[15] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b3_elevator_hall_801834C8 = { NULL, D_shelter_b3_elevator_hall_80182C04, D_shelter_b3_elevator_hall_80182C4C, D_shelter_b3_elevator_hall_80182F84, D_shelter_b3_elevator_hall_8018348C, 6978, 3041, 5, 3, 4000, 68 };

GpViewRec D_shelter_b3_elevator_hall_801834EC[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -1000, 0x7530, 0 } }, 447 },
    { { { { 3799, 0, -1531 }, { -696, 3648, -1726 }, { 1363, 1861, 3383 } }, { 6733, 2229, 2999 } }, 207 },
    { { { { 713, 0, 4033 }, { 233, 4089, -41 }, { -4026, 237, 712 } }, { -1171, 1416, 2568 } }, 230 },
    { { { { 867, 0, -4003 }, { -112, 4094, -24 }, { 4001, 114, 866 } }, { 4190, 1251, 2463 } }, 230 },
    { { { { 745, 0, -4027 }, { 54, 4095, 10 }, { 4027, -55, 745 } }, { -876, 1106, 2428 } }, 230 },
    { { { { 3923, 0, -1177 }, { -117, 4075, -392 }, { 1171, 409, 3903 } }, { -5282, 1071, 2998 } }, 207 },
    { { { { 1294, 0, -3885 }, { -479, 4064, -159 }, { 3856, 505, 1285 } }, { 6220, 1640, -296 } }, 257 },
};

SpriteBatch D_shelter_b3_elevator_hall_801835E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_elevator_hall_801835F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_elevator_hall_80183608[29] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -96, 1399, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -56, 1443, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -32, 1440, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -8, 1446, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 16, 1451, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 40, 1445, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 32, 1360, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 8, 1356, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -16, 1306, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -40, 1339, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -64, 1331, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -96, 1322, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, -96, 1252, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -64, 1287, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -40, 1260, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -16, 1264, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 8, 1268, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 32, 1268, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, 88, -96, 1250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 88, -16, 1250, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 40, -16, 1134, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 40, -96, 1202, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 64, -96, 829, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 64, -16, 917, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, 104, 40, 687, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 80, 40, 752, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 64, 40, 915, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 48, 40, 1051, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, 48, 1125, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_elevator_hall_8018384C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 1, 0 } },
    { 24, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_elevator_hall_8018386C[2] = {
    { { 101, 12, 82, 183 }, 1408 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

SpriteSource D_shelter_b3_elevator_hall_80183880[13] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 0, 0, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -16, 2214, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -32, 2208, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 16, 2214, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -32, 8, 1443, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -32, -24, 1439, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -32, -56, 1403, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -8, 16, 1911, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -8, -16, 2022, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -8, -48, 2026, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 8, -40, 2202, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 8, -16, 2225, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 8, 8, 2316, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_elevator_hall_80183984[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_elevator_hall_801839A4[2] = {
    { { 139, 0, 180, 235 }, 2208 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

SpriteSource D_shelter_b3_elevator_hall_801839B8[24] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 48, 1064, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, -8, 1070, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -56, 1072, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 56 } }, -136, 64, 666, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 64 } }, -136, 0, 703, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 56 } }, -136, -56, 754, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 64 } }, -136, -120, 740, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -96, 64, 751, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 64 } }, -96, 0, 789, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 56 } }, -96, -56, 790, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 64 } }, -96, -120, 791, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -72, -120, 910, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -72, -64, 908, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -72, -8, 907, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -72, 48, 890, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -56, -72, 986, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -56, -120, 996, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -40, -120, 1038, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -40, -72, 1002, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -40, -24, 1005, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -40, 32, 1063, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -32, 48, 1060, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -56, -16, 974, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -56, 32, 987, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_elevator_hall_80183B98[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_elevator_hall_80183BB8[2] = {
    { { 125, 25, 111, 173 }, 1055 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

SpriteSource D_shelter_b3_elevator_hall_80183BCC[73] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 72, 0, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 80, 72, 607, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 72, 658, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 64, 64, 664, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, 64, 756, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 48, 56, 767, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 48, 784, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 24, 40, 894, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 32, 997, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -8, 24, 1087, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, 32, 1045, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 40, 963, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 48, 864, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 56, 833, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, -72, 768, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -40, 695, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -72, 706, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -72, 653, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -40, 657, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -16, 693, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 8, 697, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 32, 665, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 32, 708, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 8, 665, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -16, 662, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -40, 662, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -72, 646, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, 24, 625, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, -40, 625, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -72, 625, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -72, 625, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 112, -40, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, 24, 625, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 24, 625, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 24, 625, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, 24, 650, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -16, 625, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -72, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -72, 650, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -24, 650, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 40, 704, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 48, 673, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 40, 696, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 48, 640, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 56, 621, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 48, 653, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 56, 592, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 64, 566, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 56, 585, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 64, 548, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 64, 536, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 56, 40, 672, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 56, 628, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -80, -24, 1025, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, -24, 1025, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 16 } }, -72, 0, 1025, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -24, -24, 1025, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -16, -24, 1037, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -16, 0, 1037, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 24, -16, 1050, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 16, 8, 1050, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 40, -8, 1050, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 56, 0, 1050, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 48, 24, 1050, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 40, 48 } }, -112, -104, 1250, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 48, 48 } }, -160, -104, 1250, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 40 } }, -80, -56, 1250, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 32, 40 } }, -112, -56, 1250, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 48, 40 } }, -160, -56, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 40, 40 } }, -112, -16, 1250, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 48, 40 } }, -160, -16, 1250, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 40, 32 } }, -112, 24, 1250, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 48, 32 } }, -160, 24, 1250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_elevator_hall_80184180[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 4, 0 } },
    { 14, 26, 0, 0, { 3, 0 } },
    { 40, 13, 0, 0, { 2, 0 } },
    { 53, 11, 0, 0, { 1, 0 } },
    { 64, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_elevator_hall_801841B8[2] = {
    { { 0, 1, 226, 239 }, 690 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

SpriteBatch D_shelter_b3_elevator_hall_801841CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b3_elevator_hall_801841DC[7] = {
    { { .empty = D_shelter_b3_elevator_hall_801835E8 }, D_shelter_b3_elevator_hall_801835E8, NULL },
    { { .empty = D_shelter_b3_elevator_hall_801835F8 }, D_shelter_b3_elevator_hall_801835F8, NULL },
    { { .elements = D_shelter_b3_elevator_hall_80183608 }, D_shelter_b3_elevator_hall_8018384C, D_shelter_b3_elevator_hall_8018386C },
    { { .elements = D_shelter_b3_elevator_hall_80183880 }, D_shelter_b3_elevator_hall_80183984, D_shelter_b3_elevator_hall_801839A4 },
    { { .elements = D_shelter_b3_elevator_hall_801839B8 }, D_shelter_b3_elevator_hall_80183B98, D_shelter_b3_elevator_hall_80183BB8 },
    { { .elements = D_shelter_b3_elevator_hall_80183BCC }, D_shelter_b3_elevator_hall_80184180, D_shelter_b3_elevator_hall_801841B8 },
    { { .empty = D_shelter_b3_elevator_hall_801841CC }, D_shelter_b3_elevator_hall_801841CC, NULL },
};

WorldCoordPointLight D_shelter_b3_elevator_hall_80184230[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2088, -2723, -1583 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3528, 3488, 3508 }, { 0, 0 } }, 2000, 5702 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1644, -2723, -1583 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 6201 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7160, -2723, -2914 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 2000, 7581 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5792, -2723, -1451 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 7181 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3591, -2723, -1583 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 4000 },
};

GpRoomCoordSet D_shelter_b3_elevator_hall_80184410 = { 0, NULL, 5, D_shelter_b3_elevator_hall_80184230, 0, NULL };

GpObj4C D_shelter_b3_elevator_hall_80184428[8] = {
    { NULL, NULL, NULL, { 3519, 128, -2001, 0 }, { { 0, -4304, 2096, 0 }, { 0, -4304, -2096, 0 }, { 0, 4304, 2096, 0 }, { 0, 4304, -2096, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 3360, 0, -2161, 0 }, { { 0, -4304, -1664, 0 }, { 0, -4304, 1664, 0 }, { 0, 4304, -1664, 0 }, { 0, 4304, 1664, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -1664, 0, -1888, 0 }, { { 0, -4304, -1664, 0 }, { 0, -4304, 1664, 0 }, { 0, 4304, -1664, 0 }, { 0, 4304, 1664, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1536, 0, -1856, 0 }, { { 0, -4304, 2096, 0 }, { 0, -4304, -2096, 0 }, { 0, 4304, 2096, 0 }, { 0, 4304, -2096, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -6176, 0, -1313, 0 }, { { 2304, -4304, 432, 0 }, { -2304, -4304, -432, 0 }, { 2304, 4304, 432, 0 }, { -2304, 4304, -432, 0 } }, { -758, 0, 4038, 0 }, { 0, 0, 4096, 0 }, 4884, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -6192, 0, -1104, 0 }, { { -2128, -4304, -432, 0 }, { 2128, -4304, 432, 0 }, { -2128, 4304, -432, 0 }, { 2128, 4304, 432, 0 } }, { 816, 0, -4025, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 6272, 0, -1088, 0 }, { { -2128, -4304, 464, 0 }, { 2128, -4304, -464, 0 }, { -2128, 4304, 464, 0 }, { 2128, 4304, -464, 0 } }, { -878, 0, -4025, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 6208, 0, -1216, 0 }, { { 2128, -4304, -464, 0 }, { -2128, -4304, 464, 0 }, { 2128, 4304, -464, 0 }, { -2128, 4304, 464, 0 } }, { 877, 0, 4024, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 6, 5, 129, 0 },
};

GpAreaTmdRec D_shelter_b3_elevator_hall_80184688[2] = {
    { 44, 44, 0, 0, { 0, 0 }, &D_80142604 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b3_elevator_hall_801846A0[3] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b3_elevator_hall_801846C4[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_elevator_hall_801846DC[3] = {
    { 44, 0, 0, 3800, 0, -2000, 2400, 0, 0, 2, 0 },
    { 44, 0, 0, -1500, 0, -2300, 1750, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b3_elevator_hall_8018470C[4] = {
    { 24, 0, 0, -6500, 0, -800, 500, 0, 0, 2, 0 },
    { 24, 0, 0, 5500, 0, -500, 300, 0, 0, 2, 0 },
    { 49, 2, 0, 800, 0, -1500, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b3_elevator_hall_8018474C[3] = {
    { 3, 0, 0, -5000, 0, -2000, 1024, 0, 0, 2, 0 },
    { 3, 0, 1, 6000, 0, -2000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b3_elevator_hall_8018477C[12] = {
    { NULL, NULL },
    { D_shelter_b3_elevator_hall_801846DC, D_shelter_b3_elevator_hall_80184688 },
    { D_shelter_b3_elevator_hall_8018470C, D_shelter_b3_elevator_hall_801846A0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b3_elevator_hall_8018474C, D_shelter_b3_elevator_hall_801846C4 },
};

GpObj4C D_shelter_b3_elevator_hall_801847DC[4] = {
    { NULL, NULL, NULL, { -6096, 0, 1696, 0 }, { { -752, 0, -352, 0 }, { 752, 0, -352, 0 }, { -752, 0, 352, 0 }, { 752, 0, 352, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 829, 0, 41, 19, 2, 0 },
    { NULL, NULL, NULL, { 6848, -48, 176, 0 }, { { 176, 0, -624, 0 }, { 176, 0, 625, 0 }, { -176, 0, -624, 0 }, { -176, 0, 625, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 646, 257, 21, 64, 2, 0 },
    { NULL, NULL, NULL, { 7392, -128, 0, 0 }, { { 0, 2992, -1024, 0 }, { 0, 2992, 1024, 0 }, { 0, -2992, -1024, 0 }, { 0, -2992, 1024, 0 } }, { -4104, 0, 0, 0 }, { -4096, 0, 0, 0 }, 3156, 0x8000, 43, 35, 2, 0 },
    { NULL, NULL, NULL, { -4799, -48, 1136, 0 }, { { 432, 0, -720, 0 }, { 432, 0, 720, 0 }, { -432, 0, -720, 0 }, { -432, 0, 720, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 839, 0, 26, 49, 130, 0 },
};

GpObj3A D_shelter_b3_elevator_hall_8018490C[2] = {
    { NULL, NULL, { -2176, -688, 672, 0 }, { { -2208, -4272, -1632, 0 }, { 2208, -4272, 1632, 0 }, { -2208, 4272, -1632, 0 }, { 2208, 4272, 1632, 0 } }, { 2434, 0, -3295, 0 }, { -52, 19 }, 1, 0 },
    { NULL, NULL, { 2719, -608, 703, 0 }, { { -2419, -4272, 1651, 0 }, { 2420, -4272, -1650, 0 }, { -2419, 4272, 1651, 0 }, { 2420, 4272, -1650, 0 } }, { -2311, 0, -3388, 0 }, { 50, 20 }, 129, 0 },
};

s32 D_shelter_b3_elevator_hall_80184984[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

s32 D_shelter_b3_elevator_hall_80184990[3] = {
    0x10000041,
    0x10000043,
    0x10000055,
};

s32 D_shelter_b3_elevator_hall_8018499C[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849A8[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849B0[1] = {
    { 0, 0, 1, 0, D_shelter_b3_elevator_hall_80184984 },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849B8[1] = {
    { 0, 0, 1, 0, D_shelter_b3_elevator_hall_80184990 },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849C0[1] = {
    { 0, 0, 1, 0, D_shelter_b3_elevator_hall_8018499C },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849C8[1] = {
    { 0, 1, 0, 0, D_shelter_b3_elevator_hall_80184990 },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849D0[1] = {
    { 0, 0, 1, 0, D_shelter_b3_elevator_hall_80184990 },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849D8[1] = {
    { 0, 1, 0, 0, D_shelter_b3_elevator_hall_80184990 },
};

GpRoomParamRec* D_shelter_b3_elevator_hall_801849E0[8] = {
    D_shelter_b3_elevator_hall_801849A8,
    D_shelter_b3_elevator_hall_801849B0,
    D_shelter_b3_elevator_hall_801849B8,
    D_shelter_b3_elevator_hall_801849C0,
    D_shelter_b3_elevator_hall_801849C8,
    D_shelter_b3_elevator_hall_801849D0,
    D_shelter_b3_elevator_hall_801849D8,
    D_shelter_b3_elevator_hall_801849A8,
};

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive[4] = {
    0,
    18,
    230,
    216,
};

RoomEventReq gRoomEventReq = { 0 }; /// A glowing disc anchored to its parent at the work block's position. In
#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b3_elevator_hall_80180E18(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b3_elevator_hall_80181370(Task* task)
{
    RoomFx_FlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b3_elevator_hall_80181FD0(Task* arg0)
{
    RoomFx_OrangeBurst2Task(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
