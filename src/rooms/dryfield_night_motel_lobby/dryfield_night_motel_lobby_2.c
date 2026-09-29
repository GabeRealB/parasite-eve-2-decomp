#include "types.h"

#include "main/task_types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
Task* D_dryfield_night_motel_lobby_801844D0;

s32 D_dryfield_night_motel_lobby_801844D4;

/// The seven digits of the lobby keypad code as entered so far, most recent
/// first at index 0; `0xA` marks a slot the player has not filled. The room's
/// init resets all seven to `0xA`,
/// `func_dryfield_night_motel_lobby_80180440` shifts a new digit in at index 0
/// (its own count of digits entered is bounded by 7) and
/// `func_dryfield_night_motel_lobby_80180734` tests the filled slots against
/// the code. The datum's eighth byte is padding before the cap script.
u8 D_dryfield_night_motel_lobby_801844D8[7];

#include "rooms/dryfield_night_motel_lobby.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "dryfield_night_motel_lobby_private.h"

#include "gameplay/action_prompt.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#define D_dryfield_night_motel_lobby_801828E8 (D_dryfield_night_motel_lobby_801828E0 + 1)

/// Task descriptor of the examine child task `func_dryfield_night_motel_lobby_80180E98`
/// spawns.
extern TaskDesc D_dryfield_night_motel_lobby_80182814[];

extern GpAreaApplyRec D_dryfield_night_motel_lobby_801844AC[];

/// World-space points of the markers `func_dryfield_night_motel_lobby_801812F8`
/// draws; the second name is the same run from its second entry.

static s16  func_dryfield_night_motel_lobby_80180734(void);
static void func_dryfield_night_motel_lobby_80180C20(s32 x, s32 y, s32 variant);
static void func_dryfield_night_motel_lobby_80181298(Task* task);
static void func_dryfield_night_motel_lobby_80181404(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_dryfield_night_motel_lobby_80181878(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_dryfield_night_motel_lobby_80182200(SVECTOR* arg0, s32 arg1, s32 arg2);

static void func_dryfield_night_motel_lobby_80180E98(Task* task);
static void func_dryfield_night_motel_lobby_80180FA4(Task* task);
static void func_dryfield_night_motel_lobby_80180FD8(Task* task);
static void func_dryfield_night_motel_lobby_8018103C(Task* task);
static void func_dryfield_night_motel_lobby_801810AC(Task* arg0);
static void func_dryfield_night_motel_lobby_80181138(Task* arg0);
static void func_dryfield_night_motel_lobby_8018119C(Task* arg0);
static void func_dryfield_night_motel_lobby_801811E0(Task* arg0);
static void func_dryfield_night_motel_lobby_80181218(Task* arg0);
static void func_dryfield_night_motel_lobby_8018122C(Task* arg0);

/// The eleven states of the room's examine task, run by
/// `func_dryfield_night_motel_lobby_80180D58`.
static const TaskFuncTable11 D_dryfield_night_motel_lobby_8017D6B0 = {
    {
        func_dryfield_night_motel_lobby_80180E98,
        func_dryfield_night_motel_lobby_80180FA4,
        func_dryfield_night_motel_lobby_8017FE90,
        func_dryfield_night_motel_lobby_80180FD8,
        func_dryfield_night_motel_lobby_8018103C,
        func_dryfield_night_motel_lobby_801810AC,
        func_dryfield_night_motel_lobby_80181138,
        func_dryfield_night_motel_lobby_8018119C,
        func_dryfield_night_motel_lobby_801811E0,
        func_dryfield_night_motel_lobby_80181218,
        func_dryfield_night_motel_lobby_8018122C,
    },
};

// Indexed views below share one contiguous table.
extern GpGridParams   D_dryfield_night_motel_lobby_80182DB4[1];
extern GpObj4C        D_dryfield_night_motel_lobby_80184034[4];
extern GpObj4C        D_dryfield_night_motel_lobby_80184164[8];
extern GpRoomBoundVec D_dryfield_night_motel_lobby_8018441C[8];
extern GpRoomCoordSet D_dryfield_night_motel_lobby_8018401C[1];

void func_dryfield_night_motel_lobby_80180D08(Task*);
void func_dryfield_night_motel_lobby_80180D58(Task*);

TaskDesc D_dryfield_night_motel_lobby_80182814[1] = {
    { 0, 192, func_dryfield_night_motel_lobby_80180D08, { .model = NULL } },
};

OverlayHotspot D_dryfield_night_motel_lobby_80182820[15] = {
    { -48, -62, 20, 22, 7, 0, 0 },
    { -20, -62, 20, 22, 8, 0, 0 },
    { 8, -62, 20, 22, 9, 0, 0 },
    { -48, -34, 20, 22, 4, 0, 0 },
    { -20, -34, 20, 22, 5, 0, 0 },
    { 8, -34, 20, 22, 6, 0, 0 },
    { -48, -6, 20, 22, 1, 0, 0 },
    { -20, -6, 20, 22, 2, 0, 0 },
    { 8, -6, 20, 22, 3, 0, 0 },
    { -48, 22, 20, 22, 0, 0, 0 },
    { -20, 22, 20, 22, 10, 0, 0 },
    { 8, 22, 20, 22, 11, 0, 0 },
    { -20, 50, 50, 22, 12, 0, 0 },
    { 42, 50, 50, 22, 13, 0, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

TaskDesc D_dryfield_night_motel_lobby_801828D4 = { 0, 192, func_dryfield_night_motel_lobby_80180D58, { .model = NULL } };

SVECTOR D_dryfield_night_motel_lobby_801828E0[5] = {
    { 4430, -1130, 2380, 0 },
    { 3880, -1280, 2740, 0 },
    { 4500, -2570, 1500, 0 },
    { 1500, -2570, 1500, 0 },
    { 1500, -2570, 4500, 0 },
};

GpRoomObjRec D_dryfield_night_motel_lobby_80182908[1] = {
    { D_dryfield_night_motel_lobby_80182DB4, D_dryfield_night_motel_lobby_80184034, D_dryfield_night_motel_lobby_80184164, NULL },
};

GpRoomCoordRec D_dryfield_night_motel_lobby_80182918[1] = {
    { D_dryfield_night_motel_lobby_8018401C, D_dryfield_night_motel_lobby_8018441C },
};

u8* D_dryfield_night_motel_lobby_80182920[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_motel_lobby_80182924[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_dryfield_night_motel_lobby_80182928[1] = {
    { { .words = { 0, 2940, 0, 316 } }, { 0, 0, 0, 0 }, { .words = { 0, 2940, 0, 316 } }, { 0, 0, 0, 0 }, 0x53110002, 0x53110001, 0, 2, 0, 481 },
};

SVECTOR D_dryfield_night_motel_lobby_80182960[9] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_normals.inc"
};

SVECTOR D_dryfield_night_motel_lobby_801829A8[53] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_verts.inc"
};

GpGridFace D_dryfield_night_motel_lobby_80182B50[33] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_faces.inc"
};

s16 D_dryfield_night_motel_lobby_80182CDC[100] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_motel_lobby_80182CDC[i])
s16* D_dryfield_night_motel_lobby_80182DA4[4] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_motel_lobby_80182DB4[1] = {
    { NULL, D_dryfield_night_motel_lobby_80182960, D_dryfield_night_motel_lobby_801829A8, D_dryfield_night_motel_lobby_80182B50, D_dryfield_night_motel_lobby_80182DA4, 0, 0, 2, 2, 4000, 33 },
};

GpViewRec D_dryfield_night_motel_lobby_80182DD8[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3000, 0x61A8, -3000 } }, 680 },
    { { { { -3992, 0, -915 }, { -291, 3882, 1272 }, { 867, 1305, -3784 } }, { -1790, 2360, -5690 } }, 230 },
    { { { { 4037, 0, -691 }, { -204, 3912, -1195 }, { 660, 1212, 3856 } }, { -1790, 2360, -340 } }, 230 },
    { { { { 3142, 0, -2627 }, { -1618, 3226, -1936 }, { 2069, 2523, 2475 } }, { -3510, 2960, -1660 } }, 230 },
    { { { { -3408, 0, 2272 }, { 1819, 2454, 2728 }, { -1361, 3279, -2041 } }, { -4640, 1620, -2570 } }, 289 },
    { { { { 0, 0, 4095 }, { 4045, 643, 0 }, { -643, 4045, 0 } }, { -4105, 1572, -4340 } }, 541 },
    { { { { 2330, 0, 3368 }, { 2690, 2464, -1861 }, { -2026, 3271, 1402 } }, { -4571, 1891, -3974 } }, 541 },
    { { { { 2330, 0, 3368 }, { 2690, 2464, -1861 }, { -2026, 3271, 1402 } }, { -4592, 2112, -3856 } }, 541 },
};

GpSprtCmd D_dryfield_night_motel_lobby_80182EF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_lobby_80182F08[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, -24, 1089, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 32, 1167, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -8, 1050, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -8, 1050, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -8, 1050, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -16, 1023, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -8, 950, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -8, 1014, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 1107, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -8, 1014, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 8, 1125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, -16, 1129, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 0, 1146, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 16, 1179, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -32, 1079, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -16, 1090, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 0, 1163, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 16, 1175, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 24, 740, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 48, 1137, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 1050, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 16, 1087, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 40, 1112, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 0, 1050, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 24, 1087, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 48, 1087, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 0, 1000, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 24, 1000, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 48, 1025, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 72, 1025, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 0, 920, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 64, 937, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 88, 937, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 8, 842, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 24, 825, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 40, 837, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 64, 862, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 88, 862, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 16, 763, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 32, 750, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 48, 775, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 72, 775, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 96, 775, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 32, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 725, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 64, 725, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 725, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 16, 876, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 32, 906, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 48, 912, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_motel_lobby_801832F0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 32, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_lobby_80183310[52] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -16, 1103, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 0, 986, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 994, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 1414, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 32, 1310, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 40, 1230, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 48, 1150, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 48, 687, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 650, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 64, 1041, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 72, 976, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 80, 925, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 791, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 753, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 700, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -24, 1130, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -16, 1237, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -16, 1066, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -8, 1052, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 1240, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 0, 1146, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 0, 1033, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, 8, 1076, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 8, 1000, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 8, 922, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, 8, 750, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, 16, 1008, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, 16, 946, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 16, 875, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 24, 1253, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 24, 1159, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 24, 889, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 104, 32, 846, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 40, 797, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 40, 775, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, 48, 750, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 48, 675, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 56, 725, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 56, 700, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 64, 898, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 64, 806, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 64, 662, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 80, 891, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 80, 675, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 96, 770, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -16, 1226, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 24, 825, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 24, 750, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 40, 725, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 712, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 32, 750, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 32, 737, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_motel_lobby_80183720[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 52, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_lobby_80183738[71] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -8, 837, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -8, 818, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 0, 593, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 8, 887, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 8, 687, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 24, 637, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 32, 750, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 40, 600, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 40, 500, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 550, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 525, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -24, 775, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -24, 837, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -24, 837, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -8, 725, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 8, 712, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 24, 680, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 48, 860, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -8, 775, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 8, 850, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 32, 957, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -8, 837, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 16, 650, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 32, 655, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 48, 602, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 72, 775, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 88, 781, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 104, 782, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 32, 612, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 48, 575, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 64, 569, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 64, 537, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 72, 512, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 487, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 72, 525, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 88, 518, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 48, 487, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 64, 550, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 32, 500, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 48, 550, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 72, 650, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 750, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 40, 600, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 64, 700, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 48, 942, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 32, 962, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 72, 853, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 793, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 500, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, 96, 503, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 80, 512, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 96, 503, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 16, 525, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 88, 518, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -40, 80, 534, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 80, 716, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, 48, 606, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 88, 518, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 550, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 32, 650, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 56, 825, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 80, 825, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 8, 675, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 32, 800, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 64, 825, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 8, 750, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 24, 825, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 40, 825, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 0, 887, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 900, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 16, 975, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_motel_lobby_80183CC4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 71, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_lobby_80183CDC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_lobby_80183CEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_lobby_80183CFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_lobby_80183D0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_motel_lobby_80183D1C[8] = {
    { { .empty = D_dryfield_night_motel_lobby_80182EF8 }, D_dryfield_night_motel_lobby_80182EF8, NULL },
    { { .elements = D_dryfield_night_motel_lobby_80182F08 }, D_dryfield_night_motel_lobby_801832F0, NULL },
    { { .elements = D_dryfield_night_motel_lobby_80183310 }, D_dryfield_night_motel_lobby_80183720, NULL },
    { { .elements = D_dryfield_night_motel_lobby_80183738 }, D_dryfield_night_motel_lobby_80183CC4, NULL },
    { { .empty = D_dryfield_night_motel_lobby_80183CDC }, D_dryfield_night_motel_lobby_80183CDC, NULL },
    { { .empty = D_dryfield_night_motel_lobby_80183CEC }, D_dryfield_night_motel_lobby_80183CEC, NULL },
    { { .empty = D_dryfield_night_motel_lobby_80183CFC }, D_dryfield_night_motel_lobby_80183CFC, NULL },
    { { .empty = D_dryfield_night_motel_lobby_80183D0C }, D_dryfield_night_motel_lobby_80183D0C, NULL },
};

GpPointLight D_dryfield_night_motel_lobby_80183D7C[7] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -2100, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 2949, 2457, { 0, 0 } }, 1500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4500, -2100, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3600, 3314, 2501, { 0, 0 } }, 1500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -2100, 4500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 2949, 2457, { 0, 0 } }, 1500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3885, -1300, 2740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 3604, 3276, { 0, 0 } }, 500, 1000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 85, -2050, 1040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 2949, 2457, { 0, 0 } }, 750, 1000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 85, -2050, 4920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 2949, 2457, { 0, 0 } }, 750, 1000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4500, -2100, 4500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 409, 696, 860, { 0, 0 } }, 100, 1500 },
};

GpRoomCoordSet D_dryfield_night_motel_lobby_8018401C[1] = {
    { 0, NULL, 7, D_dryfield_night_motel_lobby_80183D7C, 0, NULL },
};

GpObj4C D_dryfield_night_motel_lobby_80184034[4] = {
    { NULL, NULL, NULL, { 2241, -896, 3011, 0 }, { { -1888, 1024, 0, 0 }, { 1888, 1024, 0, 0 }, { -1888, -1024, 0, 0 }, { 1888, -1024, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2141, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 2144, -864, 3392, 0 }, { { 1888, 1024, 0, 0 }, { -1888, 1024, 0, 0 }, { 1888, -1024, 0, 0 }, { -1888, -1024, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 2141, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 4000, -896, 5264, 0 }, { { 0, 1024, 848, 0 }, { 0, 1024, -848, 0 }, { 0, -1024, 848, 0 }, { 0, -1024, -848, 0 } }, { 4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 1324, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 4095, -896, 5248, 0 }, { { 2, 1024, -848, 0 }, { -1, 1024, 848, 0 }, { 2, -1024, -848, 0 }, { -1, -1024, 848, 0 } }, { -4103, 0, -10, 0 }, { 0, 0, 4096, 0 }, 1324, 0, 3, 4, 129, 0 },
};

GpObj4C D_dryfield_night_motel_lobby_80184164[8] = {
    { NULL, NULL, NULL, { 2992, -48, 320, 0 }, { { -848, 0, -256, 0 }, { 848, 0, -256, 0 }, { -848, 0, 256, 0 }, { 848, 0, 256, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 884, 0, 15, 18, 2, 0 },
    { NULL, NULL, NULL, { 5872, -64, 3104, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 523, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 4656, -64, 2280, 0 }, { { -1200, 0, -744, 0 }, { 816, 0, -712, 0 }, { -432, 0, 728, 0 }, { 816, 0, 728, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 1408, 2, 3, 255, 4, 0 },
    { NULL, NULL, NULL, { 4016, -64, 3504, 0 }, { { -816, 0, -464, 0 }, { 816, 0, -464, 0 }, { -816, 0, 464, 0 }, { 816, 0, 464, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 938, 2, 4, 0, 4, 0 },
    { NULL, NULL, NULL, { 5808, -64, 976, 0 }, { { -320, 0, -880, 0 }, { 320, 0, -880, 0 }, { -320, 0, 880, 0 }, { 320, 0, 880, 0 } }, { 0, 4119, 0, 0 }, { -4096, 0, 0, 0 }, 936, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 5312, -64, 4928, 0 }, { { -320, 0, -624, 0 }, { 320, 0, -624, 0 }, { -320, 0, 624, 0 }, { 320, 0, 624, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 701, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 5888, -64, 3984, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 4497, -64, 4464, 0 }, { { -608, 0, -496, 0 }, { 608, 0, -496, 0 }, { -608, 0, 496, 0 }, { 608, 0, 496, 0 } }, { 0, 4105, 0, 0 }, { 4091, 0, -201, 0 }, 783, 5, 1, 0, 130, 0 },
};

GpAreaVariant D_dryfield_night_motel_lobby_801843C4[11] = { 0 };

GpRoomBoundVec D_dryfield_night_motel_lobby_8018441C[8] = {
    { 7, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 287, 398, 383, 354 },
    { 516, 781, 953, 703 },
    { 459, 656, 722, 590 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

s32 D_dryfield_night_motel_lobby_8018445C[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

s32 D_dryfield_night_motel_lobby_80184468[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

GpRoomParamRec D_dryfield_night_motel_lobby_80184474[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_lobby_8018445C },
};

GpRoomParamRec D_dryfield_night_motel_lobby_8018447C[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_lobby_8018445C },
};

GpRoomParamRec D_dryfield_night_motel_lobby_80184484[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_lobby_80184468 },
};

GpRoomParamRec* D_dryfield_night_motel_lobby_8018448C[8] = {
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_8018447C,
    D_dryfield_night_motel_lobby_80184484,
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_80184474,
};

GpAreaApplyRec D_dryfield_night_motel_lobby_801844AC[8] = {
    { 3, 1, 2, 0 },
    { 3, 3, 2, 1 },
    { 3, 5, 4, 1 },
    { 3, 9, 4, 1 },
    { 3, 26, 4, 17 },
    { 3, 26, 7, 33 },
    { 3, 29, 3, 1 },
    { 255, 0, 0, 0 },
};

Task* D_dryfield_night_motel_lobby_801844CC = NULL;

RoomCutsceneRec D_dryfield_night_motel_lobby_801844E0;

static void func_dryfield_night_motel_lobby_801807C0(Task* task);

void func_dryfield_night_motel_lobby_801802A8(Task* task)
{
    DnmlExamineWork* work = (DnmlExamineWork*)task->work;
    POLY_FT4*        p;
    s32              i;
    u8               digit;
    u8               u;

    if (work->field_6 == 0) {
        for (i = 0; i < 7; i++) {
            D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
        }
    }
    if (work->field_7 == 0) {
        for (i = 0; i < 7; i++) {
            digit          = D_dryfield_night_motel_lobby_801844D8[i];
            p              = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(p + 1);
            setPolyFT4(p);
            if (digit == 0xA) {
                p->u0 = 0;
                p->v0 = 0x18;
                p->u1 = 0x18;
                p->v1 = 0x18;
                p->u2 = 0;
                p->v2 = 0x30;
                p->u3 = 0x18;
                p->v3 = 0x30;
            } else {
                u     = digit * 0x18;
                p->u0 = u;
                p->v0 = 0;
                p->u1 = u + 0x18;
                p->v1 = 0;
                p->u2 = u;
                p->v2 = 0x18;
                p->u3 = u + 0x18;
                p->v3 = 0x18;
            }
            setShadeTex(p, 1);
            p->tpage = 0xE;
            p->clut  = 0x4000;
            p->x0    = 0x3C - i * 0x18;
            p->y0    = -0x60;
            p->x1    = 0x54 - i * 0x18;
            p->y1    = -0x60;
            p->x2    = 0x3C - i * 0x18;
            p->y2    = -0x48;
            p->x3    = 0x54 - i * 0x18;
            p->y3    = -0x48;
            addPrim(&gGpuCurrentOt[10], p);
        }
    } else {
        D_dryfield_night_motel_lobby_801844D8[0] = 0;
    }
}

void func_dryfield_night_motel_lobby_80180440(Task* task, s16 key)
{
    DnmlExamineWork* work = (DnmlExamineWork*)task->work;
    s32              i;

    switch (key) {
        case 0:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                if (D_dryfield_night_motel_lobby_801844D8[0] != 0 || D_dryfield_night_motel_lobby_801844D8[1] != 0xA) {
                    D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                    D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                    D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                    D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                    D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                    D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                    D_dryfield_night_motel_lobby_801844D8[0] = key;
                    work->field_2++;
                }
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                D_dryfield_night_motel_lobby_801844D8[work->field_2] = 0xA;
                D_dryfield_night_motel_lobby_801844D8[6]             = D_dryfield_night_motel_lobby_801844D8[5];
                D_dryfield_night_motel_lobby_801844D8[5]             = D_dryfield_night_motel_lobby_801844D8[4];
                D_dryfield_night_motel_lobby_801844D8[4]             = D_dryfield_night_motel_lobby_801844D8[3];
                D_dryfield_night_motel_lobby_801844D8[3]             = D_dryfield_night_motel_lobby_801844D8[2];
                D_dryfield_night_motel_lobby_801844D8[2]             = D_dryfield_night_motel_lobby_801844D8[1];
                D_dryfield_night_motel_lobby_801844D8[1]             = D_dryfield_night_motel_lobby_801844D8[0];
                D_dryfield_night_motel_lobby_801844D8[0]             = key;
                work->field_2++;
            }
            break;
        case 10:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                if (D_dryfield_night_motel_lobby_801844D8[0] != 0 || D_dryfield_night_motel_lobby_801844D8[1] != 0xA) {
                    D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                    D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                    D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                    D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                    D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                    D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                    D_dryfield_night_motel_lobby_801844D8[0] = 0;
                    work->field_2++;
                    if (work->field_2 < 7) {
                        D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                        D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                        D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                        D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                        D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                        D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                        D_dryfield_night_motel_lobby_801844D8[0] = 0;
                        work->field_2++;
                    }
                }
            }
            break;
        case 11:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            work->field_2 = 0;
            work->field_7 = 1;
            for (i = 0; i < 7; i++) {
                D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
            }
            break;
        case 12:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            work->field_2 = 0;
            work->field_7 = 1;
            for (i = 0; i < 7; i++) {
                D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
            }
            break;
        case 13:
            if (func_dryfield_night_motel_lobby_80180734() != 0) {
                work->field_8 = 1;
            } else {
                SndEvt_EnqueueType6(0x53110009, 0, 0);
            }
            break;
    }
}

/// Whether the keypad holds the lobby's code: exactly four digits, the three
/// older slots still `0xA`, and those four reading `3 0 3 3` in the order they
/// were typed.
static s16 func_dryfield_night_motel_lobby_80180734(void)
{
    u8* p = D_dryfield_night_motel_lobby_801844D8;

    if (p[6] != 0xA) {
        return 0;
    }
    if (p[5] != p[6]) {
        return 0;
    }
    if (p[4] != p[5]) {
        return 0;
    }
    if (p[3] != 3) {
        return 0;
    }
    if (p[2] != 0) {
        return 0;
    }
    /* Compares the third digit with the first rather than against a repeated
       literal: the earlier test leaves that load live, and re-testing it is
       what keeps it in one register instead of a fresh `addiu`. */
    if (p[1] != p[3]) {
        return 0;
    }
    return p[0] == 3;
}

/// Moves the action-prompt cursor of each pad `task->spawnArg1.value` selects (1:
/// port 0, 2: port 1, otherwise both) from its analog stick and d-pad, clamps
/// it to the screen, updates the press state of its two buttons and draws it.
static void func_dryfield_night_motel_lobby_801807C0(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1.value) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28[port];
        pad    = &Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons.halfwords[0];
        heldp  = &prompt->buttons.halfwords[1];
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                                          = 0;
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos = prompt->screen.packed;
                    *statep                                                             = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_dryfield_night_motel_lobby_80180C20(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues the action-prompt cursor icon, a textured quad, at (`x`, `y`) into
/// the head of the current OT. `variant` is the prompt's mode: 2 selects
/// palette 0x3C87, any other non-zero value 0x3C88, and 0 draws nothing.
static void func_dryfield_night_motel_lobby_80180C20(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

void func_dryfield_night_motel_lobby_80180D08(Task* task)
{
    TaskFunc states[2] = { func_dryfield_night_motel_lobby_80181298, func_dryfield_night_motel_lobby_801807C0 };

    states[task->state](task);
}

/// Runs the examine task's current state: the eleven handlers of
/// `D_dryfield_night_motel_lobby_8017D6B0` are copied onto the stack and the
/// one `Task::state` names is called.
void func_dryfield_night_motel_lobby_80180D58(Task* task)
{
    TaskFuncTable11 states;

    states = D_dryfield_night_motel_lobby_8017D6B0;
    states.funcs[task->state](task);
}

s32 func_dryfield_night_motel_lobby_80180DE4(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// States of the examine task, in the order `D_dryfield_night_motel_lobby_8017D6B0`
/// lists them.
static void func_dryfield_night_motel_lobby_80180E98(Task* task)
{
    DnmlExamineWork* work;
    OverlayHotspot*  hs;
    u8*              p;
    u8               empty;
    s32              i;

    work = memCalloc(0xA, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer           = Task_SpawnFromTable(D_dryfield_night_motel_lobby_80182814, 0, 1, 0);
    task->work                        = (TaskIdMap*)work;
    Mc_SaveData[0].state.at4.loc.view = 6;
    /* The once-loop folds away, but `flow` counts its references at loop depth
       2: without it the state load is scheduled above the mode store. */
    do {
        task->state++;
    } while (0);
    Display_AcquireRef();
    for (hs = D_dryfield_night_motel_lobby_80182820; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    /* The fill value has to reach the store through a register and the pointer
       has to be built from the index: a literal store, or a `&code[6]` folded
       into the symbol, compiles to a different loop. */
    empty = 0xA;
    i     = 6;
    p     = &D_dryfield_night_motel_lobby_801844D8[i];
    for (; i >= 0; i--) {
        *p-- = empty;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

/// Sets the first action prompt to mode 1 with target id 0x80, clears its
/// screen position, and steps the task on one state.
static void func_dryfield_night_motel_lobby_80180FA4(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Re-spawns the action prompt over the examine cursor: clears the highlight
/// state the prompt was left in, then hands the prompt's own coordinates and
/// this room's display mode back to `func_800D4E78`, which parks them in the
/// gameplay-side globals the prompt's display task reads.
static void func_dryfield_night_motel_lobby_80180FD8(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->work;

    func_dryfield_night_motel_lobby_801802A8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Confirms the action prompt the script's current step put up: drops the
/// highlight state, then, while `func_800D4EC0` still reports a prompt on
/// screen, flags the step busy in `promptBusy` (which the cursor draw in
/// `func_dryfield_night_motel_lobby_801802A8` gates its confirm on) and starts
/// cap slot 9. Advances the task to state 2 either way.
static void func_dryfield_night_motel_lobby_8018103C(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->work;

    func_dryfield_night_motel_lobby_801802A8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        work->promptBusy = 1;
        Gp_StartCapSlot(9, 0, 0);
    }
    task->state = 2;
}

static void func_dryfield_night_motel_lobby_801810AC(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    Mc_SaveData[0].state.at4.loc.view = 4;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill((Task*)arg0->spawnArg2.pointer);
    Task_RequestKill(arg0, 0);
}

static void func_dryfield_night_motel_lobby_80181138(Task* arg0)
{
    Gp_ApplyAreaRecs(D_dryfield_night_motel_lobby_801844AC);
    gGameSession->eventState = 1;
    taskKill(arg0->spawnArg2.pointer);
    GameFlag_SetNibble(0x74, 1);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_dryfield_night_motel_lobby_8018119C(Task* arg0)
{
    SndEvt_EnqueueType6(0x53110008, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_dryfield_night_motel_lobby_801811E0(Task* arg0)
{
    Gp_RunCapCmd1(8);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_dryfield_night_motel_lobby_80181218(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

static void func_dryfield_night_motel_lobby_8018122C(Task* arg0)
{
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    Mc_SaveData[0].state.at4.loc.view = 4;
    Task_RequestKill(arg0, 0);
}

/// Resets both action prompts - cursor position cleared, target id 0x100,
/// `field_E` 0xF, both buttons' held counts cleared, mode 1 - and steps the
/// task on one state.
static void func_dryfield_night_motel_lobby_80181298(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0                     = 0;
        prompt->field_4                     = 0;
        prompt->targetId                    = 0x100;
        prompt->field_E                     = 0xF;
        prompt->buttons.slots[0].heldFrames = 0;
        prompt->buttons.slots[1].heldFrames = 0;
        prompt->mode                        = 1;
    }
    task->state = task->state + 1;
}

void func_dryfield_night_motel_lobby_801812F8(Task* unused)
{
    switch (gGameSession->at4.loc.view) {
        case 2:
            func_dryfield_night_motel_lobby_80181404(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x60);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[1], 2, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[2], 1, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[3], 1, 0x300);
            break;
        case 3:
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E8[0], 2, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E8[3], 1, 0x300);
            break;
        case 4:
            func_dryfield_night_motel_lobby_80181404(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x60);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[1], 2, 0x300);
            break;
        case 5:
            func_dryfield_night_motel_lobby_80181878(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x30);
            break;
    }
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1` scales
/// `gDisplayState.animFrame` into `rsin` so the lit vertex pulses as
/// `rsin(...) / 34 + 0x78` on green and blue.
static void func_dryfield_night_motel_lobby_80181404(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        block   = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a gouraud glow around the projected
/// centre: eight wedges at the outer radius in half the pulse colour, eight at
/// half that radius in the full colour, and four cross wedges reaching from
/// the inner radius outwards. `arg2` is a signed half-extent; the outer radius
/// is `(s16)arg2 * 64 / otz` and the inner `(s16)arg2 * 8 / otz`. `arg1`
/// scales `gDisplayState.animFrame` into `rsin`, so the centre colour pulses as
/// `rsin(...) / 34 + 0x78` on green and blue.
static void func_dryfield_night_motel_lobby_80181878(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                pulse;
    s32                color;
    s32                half;
    s32                size;
    s32                ang;
    s32                t;
    s32                t2;
    s32                u;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
    gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        color = half;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Projects the point `arg0` through `gGfxViewCoord.workm` and, when the GTE flag
/// is non-negative, queues one semi-transparent `POLY_FT4` sprite centred on it:
/// UV column `(s16)arg1 * 40`, on-screen half-extent `(s16)arg2 * 39 / otz`, and
/// an RGB that alternates between 0x20 and 0x30 with `animFrame`. It reserves
/// 0x20 bytes of scratch but releases only 0x10 on exit.
static void func_dryfield_night_motel_lobby_80182200(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                idx;
    s32                blend;
    s16                xy;

    block = SCRATCH_PUSH_BYTES(0x20);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        blend       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x20;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        setUVWH(prim, idx * 40, 0, 0x27, 0x27);
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        xy            = block->sx - (u16)block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = block->sx + (u16)block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = block->sy - (u16)block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = block->sy + (u16)block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}
