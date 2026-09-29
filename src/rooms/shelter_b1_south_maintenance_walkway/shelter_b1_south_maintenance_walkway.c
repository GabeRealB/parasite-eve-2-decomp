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

/// Per-colour channel shifts for the glowing disc, indexed by the spawn
/// argument.
extern RoomHaloShade D_shelter_b1_south_maintenance_walkway_801823E8[];

/// Spawn payload of the task 0x31 the event task may start.
extern RoomFadeStorage  D_shelter_b1_south_maintenance_walkway_80183634;
extern RoomEventMsg     D_shelter_b1_south_maintenance_walkway_8018363C;
extern RoomLatchedEvent D_shelter_b1_south_maintenance_walkway_80183648;

static void func_shelter_b1_south_maintenance_walkway_8017DC88(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_south_maintenance_walkway_8017E404(SVECTOR* arg0, s16 arg1);
static void func_shelter_b1_south_maintenance_walkway_8017EA04(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b1_south_maintenance_walkway_8017EE30(GpCoord* arg0, s16 arg1, u8* rgb);
static void func_shelter_b1_south_maintenance_walkway_8017F6B4(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_b1_south_maintenance_walkway_8017FD34(GpCoord* arg0, s16 arg1, u8* arg2);
static void func_shelter_b1_south_maintenance_walkway_80180E70(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b1_south_maintenance_walkway_801810F4(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b1_south_maintenance_walkway_80181518(GpCoord* arg0, s32 arg1, u8* rgb);
static void func_shelter_b1_south_maintenance_walkway_80181A58(GpCoord* coord, s16 size);
static void func_shelter_b1_south_maintenance_walkway_80181F84(GpCoord* arg0, s32 arg1);

extern GpGridParams   D_shelter_b1_south_maintenance_walkway_801827B8[1];
extern GpObj3A        D_shelter_b1_south_maintenance_walkway_80183274[1];
extern GpObj4C        D_shelter_b1_south_maintenance_walkway_801830AC[6];
extern GpObj4C        D_shelter_b1_south_maintenance_walkway_801832B0[2];
extern GpRoomCoordSet D_shelter_b1_south_maintenance_walkway_80183094[1];
s32                   func_shelter_b1_south_maintenance_walkway_8017D790(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                   func_shelter_b1_south_maintenance_walkway_8017D9D0(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_shelter_b1_south_maintenance_walkway_8017D9D8(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_shelter_b1_south_maintenance_walkway_8017D9E0(Task*, s32, GpMessageArg, GpMessageArg);
void                  func_shelter_b1_south_maintenance_walkway_8017D5F8(Task*);

TaskDesc D_shelter_b1_south_maintenance_walkway_801822FC = { 0, 32, func_shelter_b1_south_maintenance_walkway_8017D5F8, { .model = NULL } };

GpMsgEntry D_shelter_b1_south_maintenance_walkway_80182308[5] = {
    { 5102, func_shelter_b1_south_maintenance_walkway_8017D790 },
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

SVECTOR D_shelter_b1_south_maintenance_walkway_801823D8[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

RoomHaloShade D_shelter_b1_south_maintenance_walkway_801823E8[2] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

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
    { 0, 4096, 0, 0 },
    { 2613, -3154, 0, 0 },
    { -2704, -3077, 0, 0 },
    { 0, 862, -4004, 0 },
    { 0, 0, -4096, 0 },
    { 0, 774, 4022, 0 },
    { 0, 0, 4096, 0 },
    { 0, -3154, -2613, 0 },
    { 0, -3077, 2704, 0 },
    { -4022, 774, 0, 0 },
    { -4096, 0, 0, 0 },
    { 4004, 862, 0, 0 },
    { 4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
};

SVECTOR D_shelter_b1_south_maintenance_walkway_801824F4[38] = {
    { 1090, -2600, -3120, 0 },
    { 2930, -2600, -4900, 0 },
    { 2930, -2600, 4930, 0 },
    { 1090, -2600, 4930, 0 },
    { 1160, -1010, -3190, 0 },
    { 810, -1300, -2840, 0 },
    { 810, -1300, 4930, 0 },
    { 1160, -1010, 4930, 0 },
    { 2850, -1010, 4930, 0 },
    { 3180, -1300, 4930, 0 },
    { 3180, -1300, -5150, 0 },
    { 2850, -1010, -4820, 0 },
    { -2870, -2600, -3120, 0 },
    { -2870, -2600, -4900, 0 },
    { -2870, -1300, -2840, 0 },
    { 1160, 0, -3190, 0 },
    { -2870, 0, -3190, 0 },
    { -2870, -1010, -3190, 0 },
    { -2870, -1300, -5150, 0 },
    { 2850, 0, -4820, 0 },
    { -2870, -1010, -4820, 0 },
    { -2870, 0, -4820, 0 },
    { 2850, 0, 4930, 0 },
    { 1160, 0, 4930, 0 },
    { 2935, 0, -4902, 0 },
    { 1065, 0, -3098, 0 },
    { 1065, 0, 4937, 0 },
    { 2935, 0, 4937, 0 },
    { -2872, 0, -4902, 0 },
    { -2872, 0, -3098, 0 },
    { -2870, -2690, -2700, 0 },
    { -2870, 150, -2700, 0 },
    { -2870, 150, -5300, 0 },
    { -2870, -2690, -5300, 0 },
    { 3330, -2690, 4930, 0 },
    { 3330, 150, 4930, 0 },
    { 730, 150, 4930, 0 },
    { 730, -2690, 4930, 0 },
};

GpGridFace D_shelter_b1_south_maintenance_walkway_80182624[18] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 9, 10, 8, 11 }, 2, 0 },
    { { 0, 12, 1, 13 }, 0, 0 },
    { { 5, 14, 0, 12 }, 3, 0 },
    { { 15, 16, 4, 17 }, 4, 0 },
    { { 18, 10, 13, 1 }, 5, 0 },
    { { 11, 20, 19, 21 }, 6, 0 },
    { { 4, 17, 5, 14 }, 7, 0 },
    { { 10, 18, 11, 20 }, 8, 0 },
    { { 10, 9, 1, 2 }, 9, 0 },
    { { 8, 11, 22, 19 }, 10, 0 },
    { { 6, 5, 3, 0 }, 11, 0 },
    { { 4, 7, 15, 23 }, 12, 0 },
    { { 25, 26, 24, 27 }, 13, 1 },
    { { 24, 28, 25, 29 }, 13, 1 },
    { { 31, 32, 30, 33 }, 12, 0 },
    { { 35, 36, 34, 37 }, 4, 0 },
};

s16 D_shelter_b1_south_maintenance_walkway_801826FC[18] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    -1,
};

s16 D_shelter_b1_south_maintenance_walkway_80182720[15] = {
    0,
    1,
    2,
    3,
    4,
    5,
    8,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    -1,
};

s16 D_shelter_b1_south_maintenance_walkway_80182740[10] = {
    0,
    1,
    2,
    10,
    11,
    12,
    13,
    14,
    17,
    -1,
};

s16 D_shelter_b1_south_maintenance_walkway_80182754[17] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    -1,
};

s16 D_shelter_b1_south_maintenance_walkway_80182778[9] = {
    0,
    1,
    2,
    10,
    11,
    12,
    13,
    14,
    -1,
};

s16 D_shelter_b1_south_maintenance_walkway_8018278C[10] = {
    0,
    1,
    2,
    10,
    11,
    12,
    13,
    14,
    17,
    -1,
};

s16* D_shelter_b1_south_maintenance_walkway_801827A0[6] = {
    D_shelter_b1_south_maintenance_walkway_801826FC,
    D_shelter_b1_south_maintenance_walkway_80182720,
    D_shelter_b1_south_maintenance_walkway_80182740,
    D_shelter_b1_south_maintenance_walkway_80182754,
    D_shelter_b1_south_maintenance_walkway_80182778,
    D_shelter_b1_south_maintenance_walkway_8018278C,
};

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

GpSprtCmd D_shelter_b1_south_maintenance_walkway_80182890[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_south_maintenance_walkway_801828A0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_south_maintenance_walkway_801828B0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
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

GpSprtCmd D_shelter_b1_south_maintenance_walkway_80182DE8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 57, 0, 0, { 1, 0 } },
    { 57, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_south_maintenance_walkway_80182E08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b1_south_maintenance_walkway_80182E18[5] = {
    { { .empty = D_shelter_b1_south_maintenance_walkway_80182890 }, D_shelter_b1_south_maintenance_walkway_80182890, NULL },
    { { .empty = D_shelter_b1_south_maintenance_walkway_801828A0 }, D_shelter_b1_south_maintenance_walkway_801828A0, NULL },
    { { .empty = D_shelter_b1_south_maintenance_walkway_801828B0 }, D_shelter_b1_south_maintenance_walkway_801828B0, NULL },
    { { .elements = D_shelter_b1_south_maintenance_walkway_801828C0 }, D_shelter_b1_south_maintenance_walkway_80182DE8, NULL },
    { { .empty = D_shelter_b1_south_maintenance_walkway_80182E08 }, D_shelter_b1_south_maintenance_walkway_80182E08, NULL },
};

GpPointLight D_shelter_b1_south_maintenance_walkway_80182E54[6] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1622, -223, -3869 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2555, 2518, 2578, { 0, 0 } }, 1550, 2671 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 524, -224, -3732 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2429, 2417, 2416, { 0, 0 } }, 1899, 2620 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2187, -303, -4148 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1526, 1617, 1737, { 0, 0 } }, 1500, 3411 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1972, -223, 2267 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2568, 2676, 2615, { 0, 0 } }, 1961, 3743 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1987, -223, -206 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2549, 2539, 2597, { 0, 0 } }, 1701, 2461 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2025, -223, -1515 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2588, 2641, 2677, { 0, 0 } }, 1923, 3843 },
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

GpAreaPlace D_shelter_b1_south_maintenance_walkway_801833D8[9] = {
    { 21, 0, 0, 2700, -2000, 5000, 2048, 0, 0, 2, 6 },
    { 21, 0, 0, 1300, -2000, 5000, 2048, 0, 0, 2, 6 },
    { 7, 0, 0, 2300, 0, -2500, -200, 0, 2, 4, 4 },
    { 7, 0, 0, 1700, 0, -2500, 200, 0, 2, 4, 4 },
    { 7, 0, 0, 2300, 0, -2200, 0, 0, 2, 4, 4 },
    { 7, 0, 0, 1700, 0, -2200, -500, 0, 2, 4, 4 },
    { 7, 0, 0, 2300, 0, -1900, 2600, 0, 2, 4, 4 },
    { 7, 0, 0, 1700, 0, -1900, 1600, 0, 2, 4, 4 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_south_maintenance_walkway_80183468[6] = {
    { 11, 0, 0, 2000, 0, 800, 0, 0, 0, 2, 0 },
    { 11, 0, 0, 2400, 0, -4000, 3072, 0, 0, 2, 0 },
    { 24, 0, 0, 2550, 0, 0, 3600, 0, 2, 4, 0 },
    { 24, 0, 0, 1750, 0, -600, 3072, 0, 2, 4, 0 },
    { 24, 0, 0, 1550, 0, -1600, 1024, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_south_maintenance_walkway_801834C8[9] = {
    { 24, 0, 1, 2000, 0, -800, 0, 0, 0, 2, 0 },
    { 24, 0, 0, 1500, 0, 2400, 450, 0, 0, 2, 0 },
    { 24, 0, 0, 2350, 0, 400, 3400, 0, 0, 2, 0 },
    { 24, 0, 0, 900, 0, -4500, 3100, 0, 0, 2, 0 },
    { 24, 0, 0, 750, 0, -3500, 2750, 0, 0, 2, 0 },
    { 26, 0, 0, 2500, 0, -4500, 3400, 0, 2, 4, 0 },
    { 26, 0, 0, 2450, 0, -3400, 0, 0, 2, 4, 0 },
    { 26, 0, 0, 1500, 0, -3300, 1600, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_south_maintenance_walkway_80183558[4] = {
    { 21, 4, 0, 2700, -2000, 5000, 2048, 0, 0, 2, 6 },
    { 21, 4, 0, 1300, -2000, 5000, 2048, 0, 0, 2, 6 },
    { 20, 0, 0, 2000, 0, 3500, 2048, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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
        if (dst->field_5 == 0) {
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
            gDisplayState.roomVariant         = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b1_south_maintenance_walkway_8018363C.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b1_south_maintenance_walkway_8018363C.field_2;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_b1_south_maintenance_walkway_8018363C.field_3;
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
    if (in->prefix.packed == 9) {
        event.capCmd   = 1;
        event.stageSnd = 0x540A0001;
        event.flagId   = 0x14B;
        event.fade     = 0;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(out, &event);
    }
    if (in->prefix.packed == 0xB) {
        event.capCmd   = 2;
        event.stageSnd = 0x540A0003;
        event.flagId   = 0x14C;
        event.fade     = 0;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9D8(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9E0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
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

    switch (gGameSession->at4.loc.view) {
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

        scratch  = (void**)G_SCRATCH_HEAD;
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw25Scratch);
}

/// A flash that swells and then fades. For as many ticks as the spawn argument
/// it brightens and grows two discs and a ring at its frame, all tinted
/// (level, level / 4, level / 2); at full brightness it draws a fade quad, then
/// draws a two-ring billboard that dims by 0x10 a tick and releases its work
/// block once the level falls to 0x10. It pauses while the room's event state
/// is set and releases the block when that state reaches 4.
void func_shelter_b1_south_maintenance_walkway_8017E760(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_shelter_b1_south_maintenance_walkway_8017EE30(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b1_south_maintenance_walkway_8017EE30(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b1_south_maintenance_walkway_8017EA04(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_shelter_b1_south_maintenance_walkway_8017FD34(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows.
static void func_shelter_b1_south_maintenance_walkway_8017EA04(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
static void func_shelter_b1_south_maintenance_walkway_8017EE30(GpCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// A twin trail. The first tick allocates sixteen coordinate frames, eight for
/// each trail, and seeds them all from the two points offset from the anchor,
/// so both trails start collapsed. Each later tick re-places the two points,
/// records them in the next slot of each ring of eight and draws the trails
/// between the rings as a beam. The work block is released once the tick count
/// reaches the spawn argument. It idles while the room's event state is 2 or
/// more.
void func_shelter_b1_south_maintenance_walkway_8017F1C4(Task* task)
{
    GpCoord    coord;
    GpCoord*   coords;
    GpCoord*   objCoord;
    GpCoord*   dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = (GpCoord*)task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GpCoord*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_shelter_b1_south_maintenance_walkway_801823D8[0].vx;
                objCoord->coord.t[1] = D_shelter_b1_south_maintenance_walkway_801823D8[0].vy;
                objCoord->coord.t[2] = D_shelter_b1_south_maintenance_walkway_801823D8[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b1_south_maintenance_walkway_801823D8[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub = work->parent;
                {
                    SVECTOR* edge    = &D_shelter_b1_south_maintenance_walkway_801823D8[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.flg = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_shelter_b1_south_maintenance_walkway_8017F6B4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the beam between two rings of eight coordinate frames as seven
/// gouraud quads, walking back from slot `arg2`, each quad joining two adjacent
/// slots of both rings and dimmer the older it is. `arg3` packs the colour as
/// three multipliers, at bits 8, 4 and 0. A quad whose projection overflows is
/// skipped.
static void func_shelter_b1_south_maintenance_walkway_8017F6B4(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GpCoord*           a;
    GpCoord*           b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// A spark burst. The first tick spawns its flash effect; then, for a non-zero
/// spawn argument, it sprays randomly jittered sparks each tick, and for zero
/// it draws a fixed ring and one widening by 0x30 a tick, both dimming by 0x20
/// a tick. Either way it releases its work block after seven ticks. It pauses
/// while the room's event state is set and releases the block when that state
/// reaches 4.
void func_shelter_b1_south_maintenance_walkway_8017FAAC(Task* task)
{
    GpCoord*   objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_shelter_b1_south_maintenance_walkway_8017EA04(objCoord, 0x100, 0x100, rgb);
            func_shelter_b1_south_maintenance_walkway_8017EA04(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Queues a star-shaped glow at the projected world position of `arg0`: a
/// disc of radius `arg1` scaled by depth, shaded half `arg2` at the centre, an
/// inner disc of half that radius at full `arg2`, and four thin rays at right
/// angles, alternately reaching the radius and twice it, all fading to black
/// at the rim. Nothing is drawn when the projection overflows.
static void func_shelter_b1_south_maintenance_walkway_8017FD34(GpCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// A glowing disc attached to its parent at the work block's position. In
/// state 1 the disc grows, and every fourth tick the task spawns the effect
/// `D_80115730` names at a random joint of the player's model and adopts it
/// as a child task; state 2 adds a flickering half-bright second disc; state 3
/// drifts the disc away while it fades inside an expanding ring, then releases
/// the work block. The spawn argument picks the disc's colour shifts. It
/// pauses while the room's event state is set and releases the block when that
/// state reaches 4.
void func_shelter_b1_south_maintenance_walkway_801806F4(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->sub                       = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->flg                       = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].b;
            func_shelter_b1_south_maintenance_walkway_80181518(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].b;
            func_shelter_b1_south_maintenance_walkway_80181518(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b1_south_maintenance_walkway_80181518(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1.value].b;
            func_shelter_b1_south_maintenance_walkway_80181518(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_shelter_b1_south_maintenance_walkway_801810F4(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// A spark that flies to another frame. The first tick takes the offset from
/// its own frame to the target frame the spawn argument names, in its own
/// axes, and keeps 0xCC/0x1000 of it as its step. Each later tick moves it by
/// that step and, every other tick, draws it as a textured square at the next
/// animation frame; it releases its work block after 20 ticks. It pauses while
/// the room's event state is set and releases the block when that state
/// reaches 4.
void func_shelter_b1_south_maintenance_walkway_80180C4C(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    GpCoord*   target;
    VECTOR     delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.tmd->coords;
    target = task->spawnArg1.pointer;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += work->pos.vx;
                coord->coord.t[1] += work->pos.vy;
                coord->coord.t[2] += work->pos.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_shelter_b1_south_maintenance_walkway_80180E70(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
static void func_shelter_b1_south_maintenance_walkway_80180E70(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex                                     = arg1;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = arg0->workm.t[1];
    vz                                      = arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        setRGB0(prim, arg3, arg3, arg3);
        setUVWH(prim, t + 0x60, 0, 0x17, 0x17);
        sarg        = (s16)arg2;
        t           = sarg * 24;
        block->step = (t - sarg) / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows. The
/// same drawing as `func_shelter_b1_south_maintenance_walkway_8017EA04`, with
/// its scratch block laid out differently.
static void func_shelter_b1_south_maintenance_walkway_801810F4(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows. The
/// same drawing as `func_shelter_b1_south_maintenance_walkway_8017EE30`, with
/// its scratch block laid out differently.
static void func_shelter_b1_south_maintenance_walkway_80181518(GpCoord* arg0, s32 arg1, u8* rgb)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// A burst in orange. Each tick draws a disc and a glow at a growing size
/// while a wider, dimmer ring expands and fades behind them; once that ring is
/// gone the main level falls 0x18 a tick and the work block is released. It
/// pauses while the room's event state is set and releases the block when that
/// state reaches 4.
void func_shelter_b1_south_maintenance_walkway_801818AC(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b1_south_maintenance_walkway_80181518(coord, (s16)(step * 2), rgb);
        func_shelter_b1_south_maintenance_walkway_80181A58(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b1_south_maintenance_walkway_801810F4(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_shelter_b1_south_maintenance_walkway_80181A58(GpCoord* coord, s16 size)
{
    GpCoord        ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = shifted >> 0x11;
    light->head.b               = shifted >> 0x12;
    light->head.u.at.local.t[0] = coord->coord.t[0];
    light->head.u.at.local.t[1] = coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    Gp_LcgState                 = random;
    block                       = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx               = coord->workm.t[0];
    block->vec.vy               = coord->workm.t[1];
    block->vec.vz               = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b1_south_maintenance_walkway_80181F84(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b1_south_maintenance_walkway_80181F84(GpCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}
