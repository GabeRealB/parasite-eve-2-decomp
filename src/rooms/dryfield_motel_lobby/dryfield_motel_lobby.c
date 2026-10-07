#include "rooms/dryfield_motel_lobby.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
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
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "rooms/room_common.h"

extern UiObjectDesc D_800611E4;

/// Labels the four menu-entry handlers `func_dryfield_motel_lobby_8017F094` to
/// `func_dryfield_motel_lobby_8017F308` draw: "Save", "Play Data", "Weapon
/// Data" and "PE Data".
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row labels of the play-data statistics panel, one per row
/// `func_dryfield_motel_lobby_8017D650` draws.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// The " times" suffix appended to that panel's count rows.
static u8 Telephone_Data_80181A70[];

/// The "%" suffix appended to the percentages the play-data panels print.
static u8 Telephone_Data_80181A78[];

/// Help texts of the panel's nine rows, handed to the UI holder for the
/// selected row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// The "Play Data" panel's row list.
static UiList Telephone_Data_80181C44;

/// The usage panel's row list.
static UiList Telephone_Data_80181C6C;

/// UI descriptor the "Play Data" panel and the usage panel spawn when they
/// first open.
static UiObjectDesc Telephone_Data_80181C90;

/// UI descriptors the "Play Data" entry and the two usage entries open.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

static const char Telephone_Data_8017D638[];

/// The telephone menu's entry list.
static UiList Telephone_Data_80181CF4;

/// The room's message table, which `func_dryfield_motel_lobby_8017F44C`
/// installs on the room task.
extern TaskMessageEntry D_dryfield_motel_lobby_8017F810[];

#define TELEPHONE_TITLE_BYTES "Telephone\0\xAD\x1A"
#include "../../shared/telephone.h"

static void func_dryfield_motel_lobby_8017F44C(Task* task);
static void func_dryfield_motel_lobby_8017F490(Task* task);

extern WorldCollisionGrid    D_dryfield_motel_lobby_8017FBE4[1];
extern WorldCollisionTrigger D_dryfield_motel_lobby_80180AEC[4];
extern WorldCollisionTrigger D_dryfield_motel_lobby_80180C1C[7];
extern WorldCoordRoomLights  D_dryfield_motel_lobby_80181010[1];
s32                          func_dryfield_motel_lobby_8017F40C(Task*, s32, s32, s32);
s32                          func_dryfield_motel_lobby_8017F414(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                          func_dryfield_motel_lobby_8017F43C(Task*, s32, s32, s32);
s32                          func_dryfield_motel_lobby_8017F444(Task*, s32, s32, s32);

#include "../../shared/telephone_data.inc.c"

TaskMessageEntry D_dryfield_motel_lobby_8017F810[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_motel_lobby_8017F414 },
    { 5105, func_dryfield_motel_lobby_8017F40C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_motel_lobby_8017F444 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_motel_lobby_8017F43C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources D_dryfield_motel_lobby_8017F838[1] = {
    { D_dryfield_motel_lobby_8017FBE4, D_dryfield_motel_lobby_80180AEC, D_dryfield_motel_lobby_80180C1C, NULL },
};

u8* D_dryfield_motel_lobby_8017F848[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_motel_lobby_8017F84C[1] = { 5 };

WorldCoordRoomLighting D_dryfield_motel_lobby_8017F850[1] = {
    { D_dryfield_motel_lobby_80181010, NULL },
};

DirectionWarpEntry D_dryfield_motel_lobby_8017F858[1] = {
    { { { .word = 0 }, 2940, 0, 316 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2940, 0, 316 }, { 0, 0, 0, 0 }, 0x52110002, 0x52110001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 481 },
};

static SVECTOR _gDryfieldMotelLobbyCollision02624Normals[9] = {
#include "assets/dryfield_motel_lobby_collision_02624_normals.inc"
};

static SVECTOR _gDryfieldMotelLobbyCollision02624Verts[42] = {
#include "assets/dryfield_motel_lobby_collision_02624_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelLobbyCollision02624Faces[25] = {
#include "assets/dryfield_motel_lobby_collision_02624_faces.inc"
};

static s16 _gDryfieldMotelLobbyCollision02624Cells[64] = {
#include "assets/dryfield_motel_lobby_collision_02624_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelLobbyCollision02624Cells[i])
static s16* _gDryfieldMotelLobbyCollision02624Table[4] = {
#include "assets/dryfield_motel_lobby_collision_02624_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_motel_lobby_8017FBE4[1] = {
    { NULL, _gDryfieldMotelLobbyCollision02624Normals, _gDryfieldMotelLobbyCollision02624Verts, _gDryfieldMotelLobbyCollision02624Faces, _gDryfieldMotelLobbyCollision02624Table, 0, 0, 2, 2, 4000, 25 },
};

ViewCamera D_dryfield_motel_lobby_8017FC08[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3000, 0x61A8, -3000 } }, 680 },
    { { { { -3992, 0, -915 }, { -291, 3882, 1272 }, { 867, 1305, -3784 } }, { -1790, 2360, -5690 } }, 230 },
    { { { { 4037, 0, -691 }, { -204, 3912, -1195 }, { 660, 1212, 3856 } }, { -1790, 2360, -340 } }, 230 },
    { { { { 3142, 0, -2627 }, { -1618, 3226, -1936 }, { 2069, 2523, 2475 } }, { -3510, 2960, -1660 } }, 230 },
    { { { { -3394, 0, 2291 }, { 1829, 2466, 2710 }, { -1379, 3270, -2044 } }, { -4690, 1730, -2640 } }, 289 },
};

SpriteBatch D_dryfield_motel_lobby_8017FCBC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_lobby_8017FCCC[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, -24, 1089, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 32, 1167, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -8, 1114, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -8, 1114, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -8, 1064, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -16, 1023, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -8, 950, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -8, 1014, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 1107, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -8, 1014, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 8, 1139, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, -16, 1129, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 0, 1146, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 16, 1179, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -32, 1079, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -16, 1090, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 0, 1163, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 16, 1175, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 24, 740, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 48, 1187, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 1112, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 16, 1162, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 40, 1175, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 0, 1075, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
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

SpriteBatch D_dryfield_motel_lobby_801800B4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_lobby_801800D4[52] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -16, 1103, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 0, 986, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 994, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 1414, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 32, 1310, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 40, 1230, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 48, 1150, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 48, 687, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 650, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 64, 1041, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 72, 976, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 80, 925, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 791, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 753, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 700, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -24, 1130, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -16, 1237, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -16, 1066, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -8, 1052, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 1240, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 0, 1146, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 0, 1033, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, 8, 1076, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 8, 1000, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 8, 922, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, 8, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, 16, 1008, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, 16, 946, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 16, 875, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 24, 1253, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 24, 1159, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 24, 889, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 104, 32, 846, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 40, 797, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 40, 775, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, 48, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 48, 675, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 56, 725, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 56, 700, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 64, 898, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 64, 806, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 64, 662, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 80, 891, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 80, 675, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 96, 770, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -16, 1226, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 24, 825, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 24, 750, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 40, 725, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 678, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 32, 750, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 32, 700, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_lobby_801804E4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 52, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_lobby_801804FC[71] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -8, 837, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -8, 818, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 0, 593, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 8, 887, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 8, 730, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 24, 670, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 32, 750, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 40, 612, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 40, 620, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 574, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 558, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -24, 862, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -24, 837, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, -24, 837, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -8, 806, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 8, 738, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 24, 680, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 48, 860, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -8, 806, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 8, 850, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 32, 957, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -8, 837, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 16, 708, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 32, 655, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 48, 602, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 72, 775, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 88, 781, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 104, 782, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 32, 655, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 48, 609, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 64, 569, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 64, 606, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 72, 551, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 586, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 72, 551, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 88, 518, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 48, 609, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 64, 569, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 32, 655, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 48, 609, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 72, 700, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 750, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 40, 631, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 64, 750, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 48, 942, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 32, 962, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 72, 853, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 793, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 530, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, 96, 503, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 80, 534, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 96, 503, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 16, 697, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 88, 518, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -40, 80, 534, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 80, 716, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, 48, 606, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 88, 518, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 722, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 32, 655, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 56, 825, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 80, 825, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 8, 725, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 32, 800, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 64, 825, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 8, 750, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 24, 825, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 40, 825, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 0, 887, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 900, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 16, 975, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_lobby_80180A88[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 71, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_lobby_80180AA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_motel_lobby_80180AB0[5] = {
    { { .empty = D_dryfield_motel_lobby_8017FCBC }, D_dryfield_motel_lobby_8017FCBC, NULL },
    { { .elements = D_dryfield_motel_lobby_8017FCCC }, D_dryfield_motel_lobby_801800B4, NULL },
    { { .elements = D_dryfield_motel_lobby_801800D4 }, D_dryfield_motel_lobby_801804E4, NULL },
    { { .elements = D_dryfield_motel_lobby_801804FC }, D_dryfield_motel_lobby_80180A88, NULL },
    { { .empty = D_dryfield_motel_lobby_80180AA0 }, D_dryfield_motel_lobby_80180AA0, NULL },
};

WorldCollisionTrigger D_dryfield_motel_lobby_80180AEC[4] = {
    { NULL, NULL, NULL, { 2241, -896, 3011, 0 }, { { -1888, 1024, 0, 0 }, { 1888, 1024, 0, 0 }, { -1888, -1024, 0, 0 }, { 1888, -1024, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2141, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2144, -864, 3392, 0 }, { { 1888, 1024, 0, 0 }, { -1888, 1024, 0, 0 }, { 1888, -1024, 0, 0 }, { -1888, -1024, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 2141, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4000, -896, 5264, 0 }, { { 0, 1024, 848, 0 }, { 0, 1024, -848, 0 }, { 0, -1024, 848, 0 }, { 0, -1024, -848, 0 } }, { 4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 1324, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4095, -896, 5248, 0 }, { { 2, 1024, -848, 0 }, { -1, 1024, 848, 0 }, { 2, -1024, -848, 0 }, { -1, -1024, 848, 0 } }, { -4103, 0, -10, 0 }, { 0, 0, 4096, 0 }, 1324, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_motel_lobby_80180C1C[7] = {
    { NULL, NULL, NULL, { 2992, -48, 320, 0 }, { { -848, 0, -256, 0 }, { 848, 0, -256, 0 }, { -848, 0, 256, 0 }, { 848, 0, 256, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 884, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5872, -64, 3104, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4656, -64, 2280, 0 }, { { -1200, 0, -744, 0 }, { 816, 0, -712, 0 }, { -432, 0, 728, 0 }, { 816, 0, 728, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 1408, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4016, -64, 3456, 0 }, { { -816, 0, -640, 0 }, { 816, 0, -640, 0 }, { -816, 0, 640, 0 }, { 816, 0, 640, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5808, -64, 976, 0 }, { { -320, 0, -880, 0 }, { 320, 0, -880, 0 }, { -320, 0, 880, 0 }, { 320, 0, 880, 0 } }, { 0, 4119, 0, 0 }, { -4096, 0, 0, 0 }, 936, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5312, -64, 4928, 0 }, { { -320, 0, -624, 0 }, { 320, 0, -624, 0 }, { -320, 0, 624, 0 }, { 320, 0, 624, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5888, -64, 3984, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Point lights contributing in every view of the Dryfield motel lobby.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits (`ONE` is full intensity). The room-light collection
/// borrows this writable array while the overlay is loaded. Coordinate updates
/// set its parents and composed matrices; lighting queries replace attenuation.
static WorldCoordPointLight _gDryfieldMotelLobbyPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -2500, 1200 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3768, 3768, 3522 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -1610, 704 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 2000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1610, 704 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 2000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -1610, 704 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 2000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -2500, 5450 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2949, 2949, 2703 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 6000,
    },
};

WorldCoordRoomLights D_dryfield_motel_lobby_80181010[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldMotelLobbyPointLights), _gDryfieldMotelLobbyPointLights, 0, NULL },
};

WorldCollisionFootstepSounds D_dryfield_motel_lobby_80181028 = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionSurfaceProperties D_dryfield_motel_lobby_80181034[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_motel_lobby_8018103C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_lobby_80181028 },
};

WorldCollisionSurfaceProperties* D_dryfield_motel_lobby_80181044[8] = {
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_8018103C,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
    D_dryfield_motel_lobby_80181034,
};

#include "../../shared/telephone.inc.c"

void func_dryfield_motel_lobby_8017E9E8(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

s32 func_dryfield_motel_lobby_8017F40C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Location-message handler of the room's message table: copies the requested
/// location onto the outgoing record and answers 1.
s32 func_dryfield_motel_lobby_8017F414(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_motel_lobby_8017F43C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_motel_lobby_8017F444(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// First state of the room task: installs the room's message table, stores the
/// task in session pointer slot 7 and advances.
static void func_dryfield_motel_lobby_8017F44C(Task* task)
{
    task->msgTable = D_dryfield_motel_lobby_8017F810;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Per-frame state of the room task: the room has nothing to update.
static void func_dryfield_motel_lobby_8017F490(Task* task)
{
}

/// The room task's three states: set-up, the (empty) per-frame state, and the
/// kill.
static const TaskFuncTable3 D_dryfield_motel_lobby_8017D644 = {
    {
        func_dryfield_motel_lobby_8017F44C,
        func_dryfield_motel_lobby_8017F490,
        taskKill,
    },
};

/// The room task's update: copies the state table and runs the current state.
void func_dryfield_motel_lobby_8017F498(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_lobby_8017D644;
    sp.funcs[task->state](task);
}
