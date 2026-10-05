#include "types.h"

#include "main/task_types.h"
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_cutscene.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
Task* gRoomCutsceneSoundTask;

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

extern AreaApplyRec D_dryfield_night_motel_lobby_801844AC[];

/// World-space points of the markers `func_dryfield_night_motel_lobby_801812F8`
/// draws; the second name is the same run from its second entry.

static s16  func_dryfield_night_motel_lobby_80180734(void);
static void _dryfieldNightMotelLobbyDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

static void func_dryfield_night_motel_lobby_80180E98(Task* task);
static void func_dryfield_night_motel_lobby_80180FA4(Task* task);
static void func_dryfield_night_motel_lobby_80180FD8(Task* task);
static void func_dryfield_night_motel_lobby_8018103C(Task* task);
static void func_dryfield_night_motel_lobby_80181138(Task* arg0);
static void func_dryfield_night_motel_lobby_8018119C(Task* arg0);
static void func_dryfield_night_motel_lobby_801811E0(Task* arg0);
static void _dryfieldNightMotelLobbyCashRegisterExitDelay(Task* task);
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
        actionPromptEventEnd,
        func_dryfield_night_motel_lobby_80181138,
        func_dryfield_night_motel_lobby_8018119C,
        func_dryfield_night_motel_lobby_801811E0,
        _dryfieldNightMotelLobbyCashRegisterExitDelay,
        func_dryfield_night_motel_lobby_8018122C,
    },
};

// Indexed views below share one contiguous table.
extern WorldCollisionGrid         D_dryfield_night_motel_lobby_80182DB4[1];
extern WorldCollisionTrigger      D_dryfield_night_motel_lobby_80184034[4];
extern WorldCollisionTrigger      D_dryfield_night_motel_lobby_80184164[8];
extern WorldCoordRoomAmbientEntry D_dryfield_night_motel_lobby_8018441C[8];
extern WorldCoordRoomLights       D_dryfield_night_motel_lobby_8018401C[1];

void func_dryfield_night_motel_lobby_80180D08(Task*);
void func_dryfield_night_motel_lobby_80180D58(Task*);

TaskDesc D_dryfield_night_motel_lobby_80182814[1] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_motel_lobby_80180D08, { .value = 0 } },
};

ActionPromptHotspot D_dryfield_night_motel_lobby_80182820[15] = {
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
    { -20, 22, 20, 22, DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_DOUBLE_ZERO, 0, 0 },
    { 8, 22, 20, 22, DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_HASH, 0, 0 },
    { -20, 50, 50, 22, DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_CLEAR, 0, 0 },
    { 42, 50, 50, 22, DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_TOTAL, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

TaskDesc D_dryfield_night_motel_lobby_801828D4 = { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_motel_lobby_80180D58, { .value = 0 } };

SVECTOR D_dryfield_night_motel_lobby_801828E0[5] = {
    { 4430, -1130, 2380, 0 },
    { 3880, -1280, 2740, 0 },
    { 4500, -2570, 1500, 0 },
    { 1500, -2570, 1500, 0 },
    { 1500, -2570, 4500, 0 },
};

WorldCollisionRoomResources D_dryfield_night_motel_lobby_80182908[1] = {
    { D_dryfield_night_motel_lobby_80182DB4, D_dryfield_night_motel_lobby_80184034, D_dryfield_night_motel_lobby_80184164, NULL },
};

WorldCoordRoomLighting D_dryfield_night_motel_lobby_80182918[1] = {
    { D_dryfield_night_motel_lobby_8018401C, D_dryfield_night_motel_lobby_8018441C },
};

u8* D_dryfield_night_motel_lobby_80182920[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_night_motel_lobby_80182924[1] = { 7 };

DirectionWarpEntry D_dryfield_night_motel_lobby_80182928[1] = {
    { { { .word = 0 }, 2940, 0, 316 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2940, 0, 316 }, { 0, 0, 0, 0 }, 0x53110002, 0x53110001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 481 },
};

static SVECTOR _gDryfieldNightMotelLobbyCollision057F4Normals[9] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_normals.inc"
};

static SVECTOR _gDryfieldNightMotelLobbyCollision057F4Verts[53] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightMotelLobbyCollision057F4Faces[33] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_faces.inc"
};

static s16 _gDryfieldNightMotelLobbyCollision057F4Cells[100] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightMotelLobbyCollision057F4Cells[i])
static s16* _gDryfieldNightMotelLobbyCollision057F4Table[4] = {
#include "assets/dryfield_night_motel_lobby_collision_057F4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_motel_lobby_80182DB4[1] = {
    { NULL, _gDryfieldNightMotelLobbyCollision057F4Normals, _gDryfieldNightMotelLobbyCollision057F4Verts, _gDryfieldNightMotelLobbyCollision057F4Faces, _gDryfieldNightMotelLobbyCollision057F4Table, 0, 0, 2, 2, 4000, 33 },
};

ViewCamera D_dryfield_night_motel_lobby_80182DD8[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3000, 0x61A8, -3000 } }, 680 },
    { { { { -3992, 0, -915 }, { -291, 3882, 1272 }, { 867, 1305, -3784 } }, { -1790, 2360, -5690 } }, 230 },
    { { { { 4037, 0, -691 }, { -204, 3912, -1195 }, { 660, 1212, 3856 } }, { -1790, 2360, -340 } }, 230 },
    { { { { 3142, 0, -2627 }, { -1618, 3226, -1936 }, { 2069, 2523, 2475 } }, { -3510, 2960, -1660 } }, 230 },
    { { { { -3408, 0, 2272 }, { 1819, 2454, 2728 }, { -1361, 3279, -2041 } }, { -4640, 1620, -2570 } }, 289 },
    { { { { 0, 0, 4095 }, { 4045, 643, 0 }, { -643, 4045, 0 } }, { -4105, 1572, -4340 } }, 541 },
    { { { { 2330, 0, 3368 }, { 2690, 2464, -1861 }, { -2026, 3271, 1402 } }, { -4571, 1891, -3974 } }, 541 },
    { { { { 2330, 0, 3368 }, { 2690, 2464, -1861 }, { -2026, 3271, 1402 } }, { -4592, 2112, -3856 } }, 541 },
};

SpriteBatch D_dryfield_night_motel_lobby_80182EF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_lobby_80182F08[50] = {
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

SpriteBatch D_dryfield_night_motel_lobby_801832F0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_lobby_80183310[52] = {
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

SpriteBatch D_dryfield_night_motel_lobby_80183720[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 52, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_lobby_80183738[71] = {
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

SpriteBatch D_dryfield_night_motel_lobby_80183CC4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 71, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_lobby_80183CDC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_lobby_80183CEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_lobby_80183CFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_lobby_80183D0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_motel_lobby_80183D1C[8] = {
    { { .empty = D_dryfield_night_motel_lobby_80182EF8 }, D_dryfield_night_motel_lobby_80182EF8, NULL },
    { { .elements = D_dryfield_night_motel_lobby_80182F08 }, D_dryfield_night_motel_lobby_801832F0, NULL },
    { { .elements = D_dryfield_night_motel_lobby_80183310 }, D_dryfield_night_motel_lobby_80183720, NULL },
    { { .elements = D_dryfield_night_motel_lobby_80183738 }, D_dryfield_night_motel_lobby_80183CC4, NULL },
    { { .empty = D_dryfield_night_motel_lobby_80183CDC }, D_dryfield_night_motel_lobby_80183CDC, NULL },
    { { .empty = D_dryfield_night_motel_lobby_80183CEC }, D_dryfield_night_motel_lobby_80183CEC, NULL },
    { { .empty = D_dryfield_night_motel_lobby_80183CFC }, D_dryfield_night_motel_lobby_80183CFC, NULL },
    { { .empty = D_dryfield_night_motel_lobby_80183D0C }, D_dryfield_night_motel_lobby_80183D0C, NULL },
};

/// Seven authored point lights contributing in every view of the nighttime motel lobby.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits (`ONE` is full intensity). The loaded room overlay owns
/// these writable records: coordinate updates parent and compose their
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// must not survive unloading the overlay.
static WorldCoordPointLight _gDryfieldNightMotelLobbyPointLights[] = {
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 1500, -2100, 1500 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 1500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 4500, -2100, 1500 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 3600, 3314, 2501 },
        },
        .inner = 1500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 1500, -2100, 4500 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 1500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 3885, -1300, 2740 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 3686, 3604, 3276 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 85, -2050, 1040 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 750,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 85, -2050, 4920 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 750,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 4500, -2100, 4500 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 409, 696, 860 },
        },
        .inner = 100,
        .outer = 1500,
    },
};

WorldCoordRoomLights D_dryfield_night_motel_lobby_8018401C[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldNightMotelLobbyPointLights), _gDryfieldNightMotelLobbyPointLights, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_motel_lobby_80184034[4] = {
    { NULL, NULL, NULL, { 2241, -896, 3011, 0 }, { { -1888, 1024, 0, 0 }, { 1888, 1024, 0, 0 }, { -1888, -1024, 0, 0 }, { 1888, -1024, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2141, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2144, -864, 3392, 0 }, { { 1888, 1024, 0, 0 }, { -1888, 1024, 0, 0 }, { 1888, -1024, 0, 0 }, { -1888, -1024, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 2141, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4000, -896, 5264, 0 }, { { 0, 1024, 848, 0 }, { 0, 1024, -848, 0 }, { 0, -1024, 848, 0 }, { 0, -1024, -848, 0 } }, { 4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 1324, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4095, -896, 5248, 0 }, { { 2, 1024, -848, 0 }, { -1, 1024, 848, 0 }, { 2, -1024, -848, 0 }, { -1, -1024, 848, 0 } }, { -4103, 0, -10, 0 }, { 0, 0, 4096, 0 }, 1324, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_motel_lobby_80184164[8] = {
    { NULL, NULL, NULL, { 2992, -48, 320, 0 }, { { -848, 0, -256, 0 }, { 848, 0, -256, 0 }, { -848, 0, 256, 0 }, { 848, 0, 256, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 884, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5872, -64, 3104, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4656, -64, 2280, 0 }, { { -1200, 0, -744, 0 }, { 816, 0, -712, 0 }, { -432, 0, 728, 0 }, { 816, 0, 728, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 1408, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4016, -64, 3504, 0 }, { { -816, 0, -464, 0 }, { 816, 0, -464, 0 }, { -816, 0, 464, 0 }, { 816, 0, 464, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 938, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5808, -64, 976, 0 }, { { -320, 0, -880, 0 }, { 320, 0, -880, 0 }, { -320, 0, 880, 0 }, { 320, 0, 880, 0 } }, { 0, 4119, 0, 0 }, { -4096, 0, 0, 0 }, 936, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5312, -64, 4928, 0 }, { { -320, 0, -624, 0 }, { 320, 0, -624, 0 }, { -320, 0, 624, 0 }, { 320, 0, 624, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5888, -64, 3984, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4497, -64, 4464, 0 }, { { -608, 0, -496, 0 }, { 608, 0, -496, 0 }, { -608, 0, 496, 0 }, { 608, 0, 496, 0 } }, { 0, 4105, 0, 0 }, { 4091, 0, -201, 0 }, 783, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaVariant D_dryfield_night_motel_lobby_801843C4[11] = { 0 };

WorldCoordRoomAmbientEntry D_dryfield_night_motel_lobby_8018441C[8] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_motel_lobby_8018441C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 287, 398, 383, 354 } },
    { .color = { 516, 781, 953, 703 } },
    { .color = { 459, 656, 722, 590 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_night_motel_lobby_8018445C = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionFootstepSounds D_dryfield_night_motel_lobby_80184468 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_lobby_80184474[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_motel_lobby_8018445C },
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_lobby_8018447C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_motel_lobby_8018445C },
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_lobby_80184484[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_motel_lobby_80184468 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_motel_lobby_8018448C[8] = {
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_8018447C,
    D_dryfield_night_motel_lobby_80184484,
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_80184474,
    D_dryfield_night_motel_lobby_80184474,
};

AreaApplyRec D_dryfield_night_motel_lobby_801844AC[8] = {
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

static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

void func_dryfield_night_motel_lobby_801802A8(Task* task)
{
    DryfieldNightMotelLobbyCashRegisterWork* work = task->work;
    POLY_FT4*                                p;
    s32                                      i;
    u8                                       digit;
    u8                                       u;

    if (work->entryOpen == 0) {
        for (i = 0; i < 7; i++) {
            D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
        }
    }
    if (work->entryCleared == 0) {
        for (i = 0; i < 7; i++) {
            digit          = D_dryfield_night_motel_lobby_801844D8[i];
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
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
    DryfieldNightMotelLobbyCashRegisterWork* work = task->work;
    s32                                      i;

    switch (key) {
        case 0:
            sndEvtRequestScriptStart(SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_PRESS, 0, 0);
            if (work->digitCount < 7) {
                if (D_dryfield_night_motel_lobby_801844D8[0] != 0 || D_dryfield_night_motel_lobby_801844D8[1] != 0xA) {
                    D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                    D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                    D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                    D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                    D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                    D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                    D_dryfield_night_motel_lobby_801844D8[0] = key;
                    work->digitCount++;
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
            sndEvtRequestScriptStart(SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_PRESS, 0, 0);
            if (work->digitCount < 7) {
                D_dryfield_night_motel_lobby_801844D8[work->digitCount] = 0xA;
                D_dryfield_night_motel_lobby_801844D8[6]                = D_dryfield_night_motel_lobby_801844D8[5];
                D_dryfield_night_motel_lobby_801844D8[5]                = D_dryfield_night_motel_lobby_801844D8[4];
                D_dryfield_night_motel_lobby_801844D8[4]                = D_dryfield_night_motel_lobby_801844D8[3];
                D_dryfield_night_motel_lobby_801844D8[3]                = D_dryfield_night_motel_lobby_801844D8[2];
                D_dryfield_night_motel_lobby_801844D8[2]                = D_dryfield_night_motel_lobby_801844D8[1];
                D_dryfield_night_motel_lobby_801844D8[1]                = D_dryfield_night_motel_lobby_801844D8[0];
                D_dryfield_night_motel_lobby_801844D8[0]                = key;
                work->digitCount++;
            }
            break;
        case DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_DOUBLE_ZERO:
            sndEvtRequestScriptStart(SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_PRESS, 0, 0);
            if (work->digitCount < 7) {
                if (D_dryfield_night_motel_lobby_801844D8[0] != 0 || D_dryfield_night_motel_lobby_801844D8[1] != 0xA) {
                    D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                    D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                    D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                    D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                    D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                    D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                    D_dryfield_night_motel_lobby_801844D8[0] = 0;
                    work->digitCount++;
                    if (work->digitCount < 7) {
                        D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                        D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                        D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                        D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                        D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                        D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                        D_dryfield_night_motel_lobby_801844D8[0] = 0;
                        work->digitCount++;
                    }
                }
            }
            break;
        case DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_HASH:
            sndEvtRequestScriptStart(SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_PRESS, 0, 0);
            work->digitCount   = 0;
            work->entryCleared = 1;
            for (i = 0; i < 7; i++) {
                D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
            }
            break;
        case DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_CLEAR:
            sndEvtRequestScriptStart(SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_PRESS, 0, 0);
            work->digitCount   = 0;
            work->entryCleared = 1;
            for (i = 0; i < 7; i++) {
                D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
            }
            break;
        case DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_TOTAL:
            if (func_dryfield_night_motel_lobby_80180734() != 0) {
                work->codeAccepted = 1;
            } else {
                sndEvtRequestScriptStart(SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_ERROR, 0, 0);
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

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

void func_dryfield_night_motel_lobby_80180D08(Task* task)
{
    TaskFunc states[2] = { actionPromptReset, actionPromptMoveCursors };

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

#include "../../shared/action_prompt_hit_test.inc.c"

/// States of the examine task, in the order `D_dryfield_night_motel_lobby_8017D6B0`
/// lists them.
static void func_dryfield_night_motel_lobby_80180E98(Task* task)
{
    DryfieldNightMotelLobbyCashRegisterWork* work;
    ActionPromptHotspot*                     hs;
    u8*                                      p;
    u8                                       empty;
    s32                                      i;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = taskSpawnFromTable(D_dryfield_night_motel_lobby_80182814, 0, 1, 0);
    task->work                                                 = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
    /* The once-loop folds away, but `flow` counts its references at loop depth
       2: without it the state load is scheduled above the mode store. */
    do {
        task->state++;
    } while (0);
    Display_AcquireRef();
    for (hs = D_dryfield_night_motel_lobby_80182820; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
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

/// Arms the first action prompt at `ACTION_PROMPT_SPEED_AIM` with the idle
/// cursor, clears its screen position, and steps the task on one state.
static void func_dryfield_night_motel_lobby_80180FA4(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
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
    ActionPrompt*                            prompt = D_80114D28;
    DryfieldNightMotelLobbyCashRegisterWork* work   = task->work;

    func_dryfield_night_motel_lobby_801802A8(task);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Acts on the answer to the examine prompt: drops the highlight state, then,
/// when `func_800D4EC0` reports the prompt was accepted, marks the register
/// `examined` (from which point the scan treats a confirm as a key press) and
/// starts cap slot 9. Returns the task to state 2 either way.
static void func_dryfield_night_motel_lobby_8018103C(Task* task)
{
    ActionPrompt*                            prompt = D_80114D28;
    DryfieldNightMotelLobbyCashRegisterWork* work   = task->work;

    func_dryfield_night_motel_lobby_801802A8(task);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (func_800D4EC0() != 0) {
        work->examined = 1;
        Gp_StartCapSlot(9, 0, 0);
    }
    task->state = 2;
}

#include "../../shared/action_prompt_event_end.inc.c"

static void func_dryfield_night_motel_lobby_80181138(Task* arg0)
{
    Gp_ApplyAreaRecs(D_dryfield_night_motel_lobby_801844AC);
    gGameSession->eventState = 1;
    taskKill(arg0->spawnArg2.pointer);
    gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_LOBBY_EVENT_SEEN, 1);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_dryfield_night_motel_lobby_8018119C(Task* arg0)
{
    sndEvtRequestScriptStart(SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_ACCEPT, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_dryfield_night_motel_lobby_801811E0(Task* arg0)
{
    Gp_RunCapCmd1(8);
    arg0->state = (s32)(arg0->state + 1);
}

/// Advances the cash-register task through one idle update before restoring play.
///
/// Called in state 9 after the completion CAP command request; state 10
/// restores player control, the HUD and the normal lobby view on the next update.
static void _dryfieldNightMotelLobbyCashRegisterExitDelay(Task* task)
{
    task->state = task->state + 1;
}

static void func_dryfield_night_motel_lobby_8018122C(Task* arg0)
{
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    displayReleaseMenuHold();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 4;
    Task_RequestKill(arg0, 0);
}

#include "../../shared/action_prompt_reset.inc.c"

void func_dryfield_night_motel_lobby_801812F8(Task* unused)
{
    switch (gGameSession->location.loc.view) {
        case 2:
            _glowDrawDiamond(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x60);
            _dryfieldNightMotelLobbyDrawFlare(&D_dryfield_night_motel_lobby_801828E0[1], 2, 0x300);
            _dryfieldNightMotelLobbyDrawFlare(&D_dryfield_night_motel_lobby_801828E0[2], 1, 0x300);
            _dryfieldNightMotelLobbyDrawFlare(&D_dryfield_night_motel_lobby_801828E0[3], 1, 0x300);
            break;
        case 3:
            _dryfieldNightMotelLobbyDrawFlare(&D_dryfield_night_motel_lobby_801828E8[0], 2, 0x300);
            _dryfieldNightMotelLobbyDrawFlare(&D_dryfield_night_motel_lobby_801828E8[3], 1, 0x300);
            break;
        case 4:
            _glowDrawDiamond(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x60);
            _dryfieldNightMotelLobbyDrawFlare(&D_dryfield_night_motel_lobby_801828E0[1], 2, 0x300);
            break;
        case 5:
            _glowDrawPulsingDisc(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x30);
            break;
    }
}

#include "../../shared/glow_draw_diamond.inc.c"

#include "../../shared/glow_draw_pulsing_disc.inc.c"

/// Sets a lobby flare quad's square around its projected light centre.
///
/// Borrows the writable `flare` packet and read-only `projection` for this call.
/// `sx`, `sy` and `radius` are pixels; radius is the square's half-extent.
/// Vertices 0, 1, 2 and 3 become top-left, top-right, bottom-left and bottom-right,
/// with coordinates wrapping to the packet's signed 16 bits.
/// The caller supplies the accepted projection, header, texture, colour and linkage.
static inline void _dryfieldNightMotelLobbySetFlareBounds(POLY_FT4* flare, const GlowCentreScratch* projection)
{
    flare->x0 = flare->x2 = projection->sx - projection->radius;
    flare->x1 = flare->x3 = projection->sx + projection->radius;
    flare->y0 = flare->y1 = projection->sy - projection->radius;
    flare->y2 = flare->y3 = projection->sy + projection->radius;
}

/// Draws a flickering textured flare at a lobby light's world position.
///
/// Borrows `worldPoint` during the call and queues one semitransparent quad in
/// the current frame, with RGB intensity 32 or 48 on alternating frames.
/// `textureIndex` uses its signed low halfword for a 40-texel column and its
/// low six bits for the palette offset; lobby callers select columns 1 and 2.
/// The signed low halfword of `radiusScale` gives a pixel half-extent of
/// `radiusScale * 39 / depth`, where depth is camera Z / 4 and must be nonzero.
/// The view, packet arena and depth ordering table must be ready for drawing.
///
/// Requires 32 free scratch bytes, uses the lower 16 for the projection and
/// releases those 16 on either path. The upper 16 remain reserved until the
/// frame's scratch reset; their role is unproven.
static void _dryfieldNightMotelLobbyDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale)
{
    enum { DRYFIELD_NIGHT_MOTEL_LOBBY_FLARE_RETAINED_SCRATCH_BYTES = 0x10 };
    GlowCentreScratch* projection;
    POLY_FT4*          flare;
    s32                columnIndex;
    s32                intensity;

    projection = SCRATCH_STACK_RESERVE_BYTES(sizeof(*projection) + DRYFIELD_NIGHT_MOTEL_LOBBY_FLARE_RETAINED_SCRATCH_BYTES);

    // Reject projection errors before reserving a GPU packet.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&projection->sx);
    gte_stflg(&projection->flag);
    if (projection->flag >= 0) {
        gte_stszotz(&projection->otz);
        flare          = gGpuPrimCursor;
        gGpuPrimCursor = flare + 1;
        setPolyFT4(flare);
        columnIndex  = (s16)textureIndex;
        intensity    = (((u8)gDisplayState.animFrame & 1) * (1 << GLOW_BRIGHT_FLICKER_SHIFT)) + GLOW_FLICKER_BASE_INTENSITY;
        flare->tpage = GLOW_FLARE_TEXTURE_PAGE;
        flare->clut  = (columnIndex & GLOW_FLARE_PALETTE_OFFSET_MASK) | GLOW_FLARE_PALETTE_BASE;
        setUVWH(flare, columnIndex * GLOW_FLARE_CELL_STRIDE, 0, GLOW_FLARE_CELL_LAST_TEXEL, GLOW_FLARE_CELL_LAST_TEXEL);
        setRGB0(flare, intensity, intensity, intensity);
        setSemiTrans(flare, 1);

        // Perspective-size the square and sort it at the light's projected depth.
        projection->radius = ((s16)radiusScale * GLOW_FLARE_CELL_LAST_TEXEL) / projection->otz;
        _dryfieldNightMotelLobbySetFlareBounds(flare, projection);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                flare);
    }
    // Preserve the extra reservation above the projection, including on rejection.
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreScratch);
}
