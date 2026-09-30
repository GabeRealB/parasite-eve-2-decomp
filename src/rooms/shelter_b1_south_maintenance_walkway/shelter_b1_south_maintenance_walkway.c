#include "rooms/shelter_b1_south_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
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

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_shelter_b1_south_maintenance_walkway_80183644[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_shelter_b1_south_maintenance_walkway_80183644_value __asm__("D_shelter_b1_south_maintenance_walkway_80183644");

/// Descriptor of the event task the message handler spawns.
extern TaskDesc D_shelter_b1_south_maintenance_walkway_801822FC;

/// The room's message table.
extern GpMsgEntry D_shelter_b1_south_maintenance_walkway_80182308[];

/// Points the room task draws its glows and discs at, depending on the view:
/// ten pairs of glow end points followed by the centre of the red disc.
extern SVECTOR D_shelter_b1_south_maintenance_walkway_80182330[];

/// The two points of the twin trail, as offsets from its anchor frame. The
/// second is also reached under its own name.

/// Spawn payload of the task 0x31 the event task may start.
extern RoomFadeStorage  D_shelter_b1_south_maintenance_walkway_80183634;
extern RoomEventMsg     D_shelter_b1_south_maintenance_walkway_8018363C;
extern RoomLatchedEvent D_shelter_b1_south_maintenance_walkway_80183648;

static void func_shelter_b1_south_maintenance_walkway_8017DC88(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_south_maintenance_walkway_8017E404(SVECTOR* arg0, s16 arg1);

extern GpGridParams   D_shelter_b1_south_maintenance_walkway_801827B8[1];
extern GpObj3A        D_shelter_b1_south_maintenance_walkway_80183274[1];
extern GpObj4C        D_shelter_b1_south_maintenance_walkway_801830AC[6];
extern GpObj4C        D_shelter_b1_south_maintenance_walkway_801832B0[2];
extern GpRoomCoordSet D_shelter_b1_south_maintenance_walkway_80183094[1];
s32                   func_shelter_b1_south_maintenance_walkway_8017D790(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                   func_shelter_b1_south_maintenance_walkway_8017D9D0(Task*, s32, TaskMessageArg, TaskMessageArg);
s32                   func_shelter_b1_south_maintenance_walkway_8017D9D8(Task*, s32, TaskMessageArg, TaskMessageArg);
s32                   func_shelter_b1_south_maintenance_walkway_8017D9E0(Task*, s32, TaskMessageArg, TaskMessageArg);
void                  func_shelter_b1_south_maintenance_walkway_8017D5F8(Task*);

TaskDesc D_shelter_b1_south_maintenance_walkway_801822FC = { 0, 32, func_shelter_b1_south_maintenance_walkway_8017D5F8, { .model = NULL } };

GpMsgEntry D_shelter_b1_south_maintenance_walkway_80182308[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_south_maintenance_walkway_8017D790 },
    { 5105, func_shelter_b1_south_maintenance_walkway_8017D9D0 },
    { 5103, func_shelter_b1_south_maintenance_walkway_8017D9E0 },
    { 5104, func_shelter_b1_south_maintenance_walkway_8017D9D8 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_shelter_b1_south_maintenance_walkway_80182330[21] = {
    { 894, -196, 3102, 0 },
    { 894, -196, 2264, 0 },
    { 894, -196, 404, 0 },
    { 894, -196, -385, 0 },
    { 894, -196, -2032, 0 },
    { 894, -196, -2645, 0 },
    { 3102, -196, 3102, 0 },
    { 3102, -196, 2264, 0 },
    { 3102, -196, 404, 0 },
    { 3102, -196, -385, 0 },
    { 3102, -196, -2032, 0 },
    { 3102, -196, -2645, 0 },
    { 650, -196, -2895, 0 },
    { -31, -196, -2895, 0 },
    { 600, -196, -5113, 0 },
    { -45, -196, -5113, 0 },
    { -1556, -196, -2895, 0 },
    { -2498, -196, -2895, 0 },
    { -1556, -196, -5113, 0 },
    { -2498, -196, -5113, 0 },
    { 763, -1283, -2117, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

#include "../../shared/room_visual_effects_disc_data.inc.c"

GpRoomCoordRec D_shelter_b1_south_maintenance_walkway_801823F4[1] = {
    { D_shelter_b1_south_maintenance_walkway_80183094, NULL },
};

GpRoomObjRec D_shelter_b1_south_maintenance_walkway_801823FC[1] = {
    { D_shelter_b1_south_maintenance_walkway_801827B8, D_shelter_b1_south_maintenance_walkway_801830AC, D_shelter_b1_south_maintenance_walkway_801832B0, D_shelter_b1_south_maintenance_walkway_80183274 },
};

u8* D_shelter_b1_south_maintenance_walkway_8018240C[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b1_south_maintenance_walkway_80182410[1] = {
    { { .bytes = { 5, 0 } } },
};

GpWarpRec D_shelter_b1_south_maintenance_walkway_80182414[2] = {
    { { .words = { 1024, -2357, 0, -3990 } }, { 0, 0, 0, 0 }, { .words = { 1024, -2357, 0, -3990 } }, { 0, 0, 0, 0 }, 0x540A0002, 0x540A0001, 0, 5, 0, 0 },
    { { .words = { 2048, 2048, 0, 4600 } }, { 0, 0, 0, 0 }, { .words = { 2048, 2048, 0, 4600 } }, { 0, 0, 0, 0 }, 0x540A0004, 0x540A0003, 0, 2, 0, 430 },
};

SVECTOR D_shelter_b1_south_maintenance_walkway_80182484[14] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_normals.inc"
};

SVECTOR D_shelter_b1_south_maintenance_walkway_801824F4[38] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_verts.inc"
};

GpGridFace D_shelter_b1_south_maintenance_walkway_80182624[18] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_faces.inc"
};

s16 D_shelter_b1_south_maintenance_walkway_801826FC[82] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_south_maintenance_walkway_801826FC[i])
s16* D_shelter_b1_south_maintenance_walkway_801827A0[6] = {
#include "assets/shelter_b1_south_maintenance_walkway_collision_051F8_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_south_maintenance_walkway_801827B8[1] = {
    { NULL, D_shelter_b1_south_maintenance_walkway_80182484, D_shelter_b1_south_maintenance_walkway_801824F4, D_shelter_b1_south_maintenance_walkway_80182624, D_shelter_b1_south_maintenance_walkway_801827A0, 2872, 5300, 2, 3, 4000, 18 },
};

GpViewRec D_shelter_b1_south_maintenance_walkway_801827DC[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x35D2, 0 } }, 235 },
    { { { { 3964, 0, 1030 }, { 222, 3999, -857 }, { -1006, 885, 3870 } }, { -2760, 1734, -200 } }, 235 },
    { { { { 4064, 0, 509 }, { 89, 4032, -715 }, { -501, 721, 4000 } }, { -2760, 1952, 4026 } }, 235 },
    { { { { -3922, 0, 1178 }, { 415, 3832, 1384 }, { -1102, 1445, -3670 } }, { -2962, 1986, -667 } }, 235 },
    { { { { 1594, 0, 3772 }, { 1049, 3934, -443 }, { -3623, 1139, 1531 } }, { -2935, 1592, 4947 } }, 235 },
};

SpriteBatch D_shelter_b1_south_maintenance_walkway_80182890[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_south_maintenance_walkway_801828A0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_south_maintenance_walkway_801828B0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_south_maintenance_walkway_801828C0[66] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -120, 753, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, 16, 936, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, -8, 920, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, -32, 936, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, -56, 861, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, -80, 800, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, -120, 703, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 32, -120, 776, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -120, 703, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 8, 1122, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 8, 1050, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 1063, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -104, 1490, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -104, 745, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -96, 857, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -40, 925, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -40, 925, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -32, 935, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -32, 925, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -24, 951, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -24, 920, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -48, 915, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -48, 893, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -56, 903, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -56, 925, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -64, 893, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -64, 887, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -72, 939, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -72, 877, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -80, 914, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -80, 768, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -88, 854, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -88, 777, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -96, 837, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -104, 821, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -104, 727, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -96, 631, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -16, 997, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -16, 936, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, -8, 1014, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -8, 925, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 0, 1002, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 0, 925, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 8, 1009, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 8, 1000, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 8, 1000, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 16, 1037, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 16, 1000, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 16, 1000, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 24, 1075, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 1050, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 24, 939, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 24, 1050, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 32, 1050, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 32, 1050, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 32, 1050, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 32, 1050, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, 128, 32, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 104, 32, 829, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 88, 32, 863, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 72, 32, 864, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 88, 56, 812, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 72, 56, 812, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 56, 56, 853, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 56, 32, 925, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, 32, 963, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_south_maintenance_walkway_80182DE8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 57, 0, 0, { 1, 0 } },
    { 57, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_south_maintenance_walkway_80182E08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b1_south_maintenance_walkway_80182E18[5] = {
    { { .empty = D_shelter_b1_south_maintenance_walkway_80182890 }, D_shelter_b1_south_maintenance_walkway_80182890, NULL },
    { { .empty = D_shelter_b1_south_maintenance_walkway_801828A0 }, D_shelter_b1_south_maintenance_walkway_801828A0, NULL },
    { { .empty = D_shelter_b1_south_maintenance_walkway_801828B0 }, D_shelter_b1_south_maintenance_walkway_801828B0, NULL },
    { { .elements = D_shelter_b1_south_maintenance_walkway_801828C0 }, D_shelter_b1_south_maintenance_walkway_80182DE8, NULL },
    { { .empty = D_shelter_b1_south_maintenance_walkway_80182E08 }, D_shelter_b1_south_maintenance_walkway_80182E08, NULL },
};

GpPointLight D_shelter_b1_south_maintenance_walkway_80182E54[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1622, -223, -3869 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2555, 2518, 2578 }, { 0, 0 } }, 1550, 2671 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 524, -224, -3732 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2429, 2417, 2416 }, { 0, 0 } }, 1899, 2620 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2187, -303, -4148 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1526, 1617, 1737 }, { 0, 0 } }, 1500, 3411 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1972, -223, 2267 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2568, 2676, 2615 }, { 0, 0 } }, 1961, 3743 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1987, -223, -206 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2549, 2539, 2597 }, { 0, 0 } }, 1701, 2461 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2025, -223, -1515 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2588, 2641, 2677 }, { 0, 0 } }, 1923, 3843 },
};

GpRoomCoordSet D_shelter_b1_south_maintenance_walkway_80183094[1] = {
    { 0, NULL, 6, D_shelter_b1_south_maintenance_walkway_80182E54, 0, NULL },
};

GpObj4C D_shelter_b1_south_maintenance_walkway_801830AC[6] = {
    { NULL, NULL, NULL, { 2080, -1584, 2560, 0 }, { { -1380, -1904, -226, 0 }, { 1363, -1904, 207, 0 }, { -1380, 1904, -226, 0 }, { 1363, 1904, 207, 0 } }, { 639, 0, -4061, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 2048, -1600, 2401, 0 }, { { 1408, -1904, 152, 0 }, { -1416, -1904, -160, 0 }, { 1408, 1904, 152, 0 }, { -1416, 1904, -160, 0 } }, { -453, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 2112, -1504, -1233, 0 }, { { 1717, -1904, 24, 0 }, { -1729, -1904, -41, 0 }, { 1717, 1904, 24, 0 }, { -1729, 1904, -41, 0 } }, { -79, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 2176, -1505, -1072, 0 }, { { -1720, -1904, -82, 0 }, { 1700, -1904, 41, 0 }, { -1720, 1904, -82, 0 }, { 1700, 1904, 41, 0 } }, { 146, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 1008, -1568, -4241, 0 }, { { 112, -1904, -1309, 0 }, { -114, -1904, 1306, 0 }, { 112, 1904, -1309, 0 }, { -114, 1904, 1306, 0 } }, { 4099, 0, 354, 0 }, { 0, 0, 4096, 0 }, 2304, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 1120, -1600, -4176, 0 }, { { -100, -1904, 1323, 0 }, { 98, -1904, -1325, 0 }, { -100, 1904, 1323, 0 }, { 98, 1904, -1325, 0 } }, { -4084, 0, -307, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, 129, 0 },
};

GpObj3A D_shelter_b1_south_maintenance_walkway_80183274[1] = {
    { NULL, NULL, { -1504, -1344, -1424, 0 }, { { -2080, -2368, 1200, 0 }, { 2080, -2368, -1200, 0 }, { -2080, 2368, 1200, 0 }, { 2080, 2368, -1200, 0 } }, { -2053, 0, -3558, 0 }, { 39, 13 }, 129, 0 },
};

GpObj4C D_shelter_b1_south_maintenance_walkway_801832B0[2] = {
    { NULL, NULL, NULL, { -2512, -48, -4032, 0 }, { { -432, 0, -1024, 0 }, { 432, 0, -1024, 0 }, { -432, 0, 1024, 0 }, { 432, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1108, 0, 9, 20, 2, 0 },
    { NULL, NULL, NULL, { 1984, -48, 4480, 0 }, { { -1024, 0, 432, 0 }, { -1024, 0, -432, 0 }, { 1024, 0, 432, 0 }, { 1024, 0, -432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1108, 0, 11, 33, 130, 0 },
};

GpAreaTmdRec D_shelter_b1_south_maintenance_walkway_80183348[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 7, 7, 1, 0, { 0, 0 }, D_80150C80 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_south_maintenance_walkway_8018336C[3] = {
    { 11, 11, 0, 0, { 0, 0 }, D_80147400 },
    { 24, 24, 1, 0, { 0, 0 }, D_8014E47C },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_south_maintenance_walkway_80183390[3] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 26, 26, 1, 0, { 0, 0 }, D_801528D4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_south_maintenance_walkway_801833B4[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 20, 20, 1, 0, { 0, 0 }, D_8015FDF0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_south_maintenance_walkway_801833D8[9] = {
    { 21, 0, 0, 2700, -2000, 5000, 2048, 0, 0, 2, 6 },
    { 21, 0, 0, 1300, -2000, 5000, 2048, 0, 0, 2, 6 },
    { 7, 0, 0, 2300, 0, -2500, -200, 0, 2, 4, 4 },
    { 7, 0, 0, 1700, 0, -2500, 200, 0, 2, 4, 4 },
    { 7, 0, 0, 2300, 0, -2200, 0, 0, 2, 4, 4 },
    { 7, 0, 0, 1700, 0, -2200, -500, 0, 2, 4, 4 },
    { 7, 0, 0, 2300, 0, -1900, 2600, 0, 2, 4, 4 },
    { 7, 0, 0, 1700, 0, -1900, 1600, 0, 2, 4, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_south_maintenance_walkway_80183468[6] = {
    { 11, 0, 0, 2000, 0, 800, 0, 0, 0, 2, 0 },
    { 11, 0, 0, 2400, 0, -4000, 3072, 0, 0, 2, 0 },
    { 24, 0, 0, 2550, 0, 0, 3600, 0, 2, 4, 0 },
    { 24, 0, 0, 1750, 0, -600, 3072, 0, 2, 4, 0 },
    { 24, 0, 0, 1550, 0, -1600, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_south_maintenance_walkway_801834C8[9] = {
    { 24, 0, 1, 2000, 0, -800, 0, 0, 0, 2, 0 },
    { 24, 0, 0, 1500, 0, 2400, 450, 0, 0, 2, 0 },
    { 24, 0, 0, 2350, 0, 400, 3400, 0, 0, 2, 0 },
    { 24, 0, 0, 900, 0, -4500, 3100, 0, 0, 2, 0 },
    { 24, 0, 0, 750, 0, -3500, 2750, 0, 0, 2, 0 },
    { 26, 0, 0, 2500, 0, -4500, 3400, 0, 2, 4, 0 },
    { 26, 0, 0, 2450, 0, -3400, 0, 0, 2, 4, 0 },
    { 26, 0, 0, 1500, 0, -3300, 1600, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_south_maintenance_walkway_80183558[4] = {
    { 21, 4, 0, 2700, -2000, 5000, 2048, 0, 0, 2, 6 },
    { 21, 4, 0, 1300, -2000, 5000, 2048, 0, 0, 2, 6 },
    { 20, 0, 0, 2000, 0, 3500, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b1_south_maintenance_walkway_80183598[12] = {
    { NULL, NULL },
    { D_shelter_b1_south_maintenance_walkway_801833D8, D_shelter_b1_south_maintenance_walkway_80183348 },
    { D_shelter_b1_south_maintenance_walkway_80183468, D_shelter_b1_south_maintenance_walkway_8018336C },
    { D_shelter_b1_south_maintenance_walkway_801834C8, D_shelter_b1_south_maintenance_walkway_80183390 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_south_maintenance_walkway_80183558, D_shelter_b1_south_maintenance_walkway_801833B4 },
};

s32 D_shelter_b1_south_maintenance_walkway_801835F8[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_shelter_b1_south_maintenance_walkway_80183604[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b1_south_maintenance_walkway_8018360C[1] = {
    { 0, 0, 1, 0, D_shelter_b1_south_maintenance_walkway_801835F8 },
};

GpRoomParamRec* D_shelter_b1_south_maintenance_walkway_80183614[8] = {
    D_shelter_b1_south_maintenance_walkway_80183604,
    D_shelter_b1_south_maintenance_walkway_8018360C,
    D_shelter_b1_south_maintenance_walkway_80183604,
    D_shelter_b1_south_maintenance_walkway_80183604,
    D_shelter_b1_south_maintenance_walkway_80183604,
    D_shelter_b1_south_maintenance_walkway_80183604,
    D_shelter_b1_south_maintenance_walkway_80183604,
    D_shelter_b1_south_maintenance_walkway_80183604,
};

RoomFadeStorage D_shelter_b1_south_maintenance_walkway_80183634 = { 0 };

RoomEventMsg D_shelter_b1_south_maintenance_walkway_8018363C = { 0 };

s8 D_shelter_b1_south_maintenance_walkway_80183644[4] = {
    0,
    26,
    67,
    -36,
};

RoomLatchedEvent D_shelter_b1_south_maintenance_walkway_80183648;

static __inline__ s32 _shelterB1SouthMaintenanceWalkwayStartEvent(
    RoomEventMsg* dst, RoomLatchedEvent* event);
static void func_shelter_b1_south_maintenance_walkway_8017D9E8(Task* task);
static void func_shelter_b1_south_maintenance_walkway_8017DA2C(Task* task);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB1SouthMaintenanceWalkwayStartEvent(
    RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_south_maintenance_walkway_80183644_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b1_south_maintenance_walkway_8018363C = *dst;
            D_shelter_b1_south_maintenance_walkway_80183648 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_south_maintenance_walkway_801822FC, 0, 0, 0);
            D_shelter_b1_south_maintenance_walkway_80183644_value = 1;
        }
        return 2;
    }
    return 1;
}

/// The event task the room's message handler spawns. It runs the latched
/// event's CAP command and waits for it to finish, starting task 0x31 when the
/// event asks for it; then plays the event's stage sound (if any) and waits
/// for the voice to end. Finally it commits the latched message's area, warp
/// and room as the save location, respawns the player task as type 0x11 and
/// ends.
void func_shelter_b1_south_maintenance_walkway_8017D5F8(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b1_south_maintenance_walkway_80183648.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b1_south_maintenance_walkway_80183648.fade != 0) {
                    D_shelter_b1_south_maintenance_walkway_80183634.fade.field_0 = 0;
                    D_shelter_b1_south_maintenance_walkway_80183634.fade.field_1 = 0;
                    D_shelter_b1_south_maintenance_walkway_80183634.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_b1_south_maintenance_walkway_80183634.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b1_south_maintenance_walkway_80183648.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b1_south_maintenance_walkway_80183648.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b1_south_maintenance_walkway_80183648.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b1_south_maintenance_walkway_8018363C.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b1_south_maintenance_walkway_8018363C.warp;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_b1_south_maintenance_walkway_8018363C.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Messages 9 and 0xB start the room's event, each with its
/// own parameters and flag; any other message answers 1.
s32 func_shelter_b1_south_maintenance_walkway_8017D790(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == 9) {
        event.capCmd   = 1;
        event.stageSnd = 0x540A0001;
        event.flagId   = 0x14B;
        event.fade     = 0;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(out, &event);
    }
    if (in->areaId == 0xB) {
        event.capCmd   = 2;
        event.stageSnd = 0x540A0003;
        event.flagId   = 0x14C;
        event.fade     = 0;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9D0(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9D8(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9E0(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Installs the room's message table on `task`, publishes the task in pointer
/// slot 7 and steps it to its next state.
static void func_shelter_b1_south_maintenance_walkway_8017D9E8(Task* task)
{
    task->msgTable = D_shelter_b1_south_maintenance_walkway_80182308;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The room task's idle state: does nothing.
static void func_shelter_b1_south_maintenance_walkway_8017DA2C(Task* task)
{
}

/// The room task's three states: set-up, idle and exit.
static const TaskFuncTable3 D_shelter_b1_south_maintenance_walkway_8017D5D8 = {
    { func_shelter_b1_south_maintenance_walkway_8017D9E8, func_shelter_b1_south_maintenance_walkway_8017DA2C, taskKill },
};

/// The room task. Runs the handler for its current state from the room's
/// three-entry state table, copied to the stack first.
void func_shelter_b1_south_maintenance_walkway_8017DA34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_south_maintenance_walkway_8017D5D8;
    sp.funcs[task->state](task);
}

void func_shelter_b1_south_maintenance_walkway_8017DA8C(Task* task)
{
    if (task->state == 0) {
        D_80115758  = 0x601C9;
        D_8011572C  = 0x601E5;
        D_80115750  = 0x60201;
        D_80115734  = 0x6021C;
        D_80115730  = 0x6021B;
        D_80115754  = 0x6021D;
        task->state = 1;
    }

    switch (gGameSession->location.loc.view) {
        case 2:
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[0], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[6], 0x200, 0x400);
            break;
        case 3:
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[0], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[2], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[6], 0x200, 0x400);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[8], 0x200, 0x400);
            break;
        case 4:
            func_shelter_b1_south_maintenance_walkway_8017E404(&D_shelter_b1_south_maintenance_walkway_80182330[20], 0x200);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[4], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[10], 0x200, -0x400);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[14], 0x200, 0x800);
            break;
        case 5:
            func_shelter_b1_south_maintenance_walkway_8017E404(&D_shelter_b1_south_maintenance_walkway_80182330[20], 0x200);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[4], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[12], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[14], 0x200, -0x400);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[16], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&D_shelter_b1_south_maintenance_walkway_80182330[18], 0x200, -0x400);
            break;
    }
}

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, of radius `arg1` scaled by that end's
/// depth and turned by the angle `arg2`, joined by quads. The brightness
/// alternates between 0x20 and 0x28 on successive frames. Nothing is drawn when
/// the second point lies nearer than OTZ 0x11.
static void func_shelter_b1_south_maintenance_walkway_8017DC88(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        base      = (s16)arg2;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = rgb;
            p->g2    = rgb;
            prim->b2 = rgb;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Projects `arg0` through `gGfxViewCoord.workm` and, when its OTZ is above 0x10,
/// queues four gouraud `POLY_G4` wedges forming a red disc around it, of radius
/// `arg1 * 64 / otz`. The centre's red level alternates between 0x20 and 0x28
/// on odd and even frames.
static void func_shelter_b1_south_maintenance_walkway_8017E404(SVECTOR* arg0, s16 arg1)
{
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                radius;

    block = SCRATCH_PUSH(RoomDraw25Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        radius        = (arg1 * 64) / block->otz;
        rgb           = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        ang           = 0;
        block->radius = radius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw25Scratch);
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b1_south_maintenance_walkway_8017E760(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b1_south_maintenance_walkway_8017F1C4(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_south_maintenance_walkway_8017FAAC(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b1_south_maintenance_walkway_801806F4(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b1_south_maintenance_walkway_80180C4C(Task* task)
{
    RoomFx_FlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b1_south_maintenance_walkway_801818AC(Task* arg0)
{
    RoomFx_OrangeBurst2Task(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
