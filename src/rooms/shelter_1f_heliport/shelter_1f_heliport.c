#include "rooms/shelter_1f_heliport.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
#include "../../shared/room_events.h"
#include "../../shared/follow_collision.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_shelter_1f_heliport_80182CB0[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_shelter_1f_heliport_80182CB0_value __asm__("D_shelter_1f_heliport_80182CB0");

extern void func_actor_161500_80131FBC(void);
extern void func_actor_161500_80132038(void);
extern void func_actor_161500_80132110(void);
extern void func_actor_161500_8013230C(void);
extern void func_actor_161500_801322A0(void);
extern void func_actor_260400_80149E38(void);
extern void func_actor_260500_80149E80(void);
extern void func_actor_260500_80149EBC(void);
extern void func_actor_260400_80149FA4(void);

extern TaskDesc D_actor_161500_80136CDC;

/// The 0xFFFF-terminated item id lists `func_shelter_1f_heliport_8017D730`
/// chooses from, and the one it returns when no case matches.
static u16 Shop_Data_801815F8[];
static u16 Shop_Data_80181600[];
static u16 Shop_Data_80181608[];
static u16 Shop_Data_80181610[];
static u16 Shop_Data_80181620[];
static u16 Shop_Data_80181630[];
static u16 Shop_Data_80181640[];
static u16 Shop_Data_80181648[];
static u16 Shop_Data_80181658[];
static u16 Shop_Data_80181668[];
static u16 Shop_Data_80181678[];
static u16 Shop_Data_80181680[];
static u16 Shop_Data_80181694[];
static u16 Shop_Data_801816AC[];
static u16 Shop_Data_801816C0[];
static u16 Shop_Data_801816C8[];
static u16 Shop_Data_801816D8[];
static u16 Shop_Data_801816F0[];
static u16 Shop_Data_80181704[];
static u16 Shop_Data_8018170C[];
static u16 Shop_Data_80181720[];
static u16 Shop_Data_8018173C[];
static u16 Shop_Data_8018174C[];
static u16 Shop_Data_80181758[];
static u16 Shop_Data_80181770[];
static u16 Shop_Data_8018178C[];
static u16 Shop_Data_801817A0[];
static u16 Shop_Data_801817A8[];
static u16 Shop_Data_801817BC[];
static u16 Shop_Data_801817DC[];
static u16 Shop_Data_801817EC[];
static u16 Shop_Data_801817F8[];
static u16 Shop_Data_80181810[];
static u16 Shop_Data_80181814[];
static u16 Shop_Data_80181818[];
static u16 Shop_Data_80181820[];
static u16 Shop_Data_80181830[];
static u16 Shop_Data_80181838[];
static u16 Shop_Data_80181840[];
static u16 Shop_Data_80181848[];
static u16 Shop_Data_80181854[];
static u16 Shop_Data_8018185C[];
static u16 Shop_Data_80181868[];
static u16 Shop_Data_80181870[];
static u16 Shop_Data_8018187C[];
static u16 Shop_Data_80181888[];
static u16 Shop_Data_80181890[];
static u16 Shop_Data_80181898[];
static u16 Shop_Data_801818A4[];
static u16 Shop_Data_801818B0[];
static u16 Shop_Data_801818B8[];
static u16 Shop_Data_801818C4[];
static u16 Shop_Data_801818D0[];
static u16 Shop_Data_801818DC[];
static u16 Shop_Data_801818E0[];
static u16 Shop_Data_801818EC[];
static u16 Shop_Data_801818F8[];
static u16 Shop_Data_80181904[];
static u16 Shop_Data_8018190C[];
static u16 Shop_Data_80181918[];
static u16 Shop_Data_80181924[];
static u16 Shop_Data_80181930[];
static u16 Shop_Data_80181938[];
static u16 Shop_Data_80181944[];
static u16 Shop_Data_80181AD4[];

/// Messages and labels of the shop's panels.
static u8 Shop_Data_801819F0[];
static u8 Shop_Data_80181A04[];
static u8 Shop_Data_80181A0C[];
static u8 Shop_Data_80181A1C[];
static u8 Shop_Data_80181A20[];
static u8 Shop_Data_80181A5C[];
static u8 Shop_Data_80181A64[];
static u8 Shop_Data_80181A70[];
static u8 Shop_Data_80181A78[];
static u8 Shop_Data_80181A80[];
static u8 Shop_Data_80181A94[];
static u8 Shop_Data_80181AA4[];
static u8 Shop_Data_80181AC4[];
static u8 Shop_Data_80181AD0[];

/// The shop's tier ladder.
static ShopTier Shop_Data_80181950[SHOP_TIER_COUNT];

/// The item id the shop list's cursor last rested on.
static s32 Shop_Data_801819EC;

/// Row handlers, lists and panel descriptors of the shop's panels.
static UiListRowCallback Shop_Data_80181AD8[];
static UiList            Shop_Data_80181AE0;
static UiList            Shop_Data_80181B0C;
static UiObjectDesc      Shop_Data_80181B30;
static UiObjectDesc      Shop_Data_80181B4C;
static UiObjectDesc      Shop_Data_80181B68;
static UiObjectDesc      Shop_Data_80181B84;
static UiObjectDesc      Shop_Data_80181BA0;
static UiObjectDesc      Shop_Data_80181BD8;
static UiObjectDesc      Shop_Data_80181BF4;
static UiObjectDesc      Shop_Data_80181C10;

/// Descriptors of the room's event task and of its cap-script task.
extern TaskDesc D_shelter_1f_heliport_80181194;
extern TaskDesc D_shelter_1f_heliport_801811C8;

/// Message handlers the room's controller task installs in pointer slot 7.
extern TaskMessageEntry D_shelter_1f_heliport_801811A0[];

extern u8 D_shelter_1f_heliport_801811D4[][4];

/// Offset `func_shelter_1f_heliport_801802AC` hands the mesh rebuild; only its
/// `vy` is ever set.
extern SVECTOR D_shelter_1f_heliport_80181204;

/// The mesh's pristine source and the working copy rebuilt from it.
extern WorldCollisionGrid gFollowCollisionSource;
extern WorldCollisionGrid gFollowCollisionGrid;

/// Work pair of the charge panel `func_shelter_1f_heliport_8017F2D4`: the
/// animated quantity in 24.8 fixed point, and the item map of the slot being
/// charged.
static s32                    Shop_Data_80187628;
static EquipmentWeaponSupply* Shop_Data_8018762C;

/// The event the message handler latched for the room's event task: the spawn
/// argument of its helper task 0x31, the message, the flag saying one was
/// latched, and the event's parameters.
extern RoomFadeStorage  gRoomEventFade;
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void func_shelter_1f_heliport_80180658(Task* task);
static void func_shelter_1f_heliport_80180748(Task* task);
static void func_shelter_1f_heliport_801807C0(void);

#define SHOP_CHARGE_TITLE_BYTES "Charge\0" \
                                "2"
#include "../../shared/shop.h"
#include "../../shared/cap_dialogue.h"

extern WorldCollisionGrid         gFollowCollisionGrid;
extern WorldCollisionTrigger      D_shelter_1f_heliport_80182178[12];
extern WorldCollisionTrigger      D_shelter_1f_heliport_80182508[21];
extern WorldCoordRoomAmbientEntry D_shelter_1f_heliport_80182B44[13];
extern WorldCoordRoomLights       D_shelter_1f_heliport_80182160[1];
s32                               func_shelter_1f_heliport_801800A0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                               func_shelter_1f_heliport_80180334(Task*, s32, s32, s32);
s32                               func_shelter_1f_heliport_8018041C(Task*, s32, s32, s32);
s32                               func_shelter_1f_heliport_801804BC(Task*, s32, RoomEventMsg*, s32);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_shelter_1f_heliport_80181188 = { { { TASK_BODY_NONE, 192 } }, Shop_SessionTask, { .value = 0 } };

TaskDesc D_shelter_1f_heliport_80181194 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_1f_heliport_801811A0[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_heliport_801800A0 },
    { 5105, func_shelter_1f_heliport_80180334 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_1f_heliport_801804BC },
    { ROOM_MESSAGE_COMMAND, func_shelter_1f_heliport_8018041C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_1f_heliport_801811C8 = { { { TASK_BODY_NONE, 32 } }, capDialogueLoopTask, { .value = 0 } };

u8 D_shelter_1f_heliport_801811D4[12][4] = {
    { 2, 2, 2, 2 },
    { 1, 1, 1, 1 },
    { 2, 2, 1, 1 },
    { 1, 1, 2, 2 },
    { 1, 1, 2, 2 },
    { 2, 2, 1, 1 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 2, 2, 1, 1 },
};

SVECTOR D_shelter_1f_heliport_80181204 = { 0 };

static SVECTOR _gShelter1fHeliportCollision03CECNormals[4] = {
#include "assets/shelter_1f_heliport_collision_03CEC_normals.inc"
};

static SVECTOR _gShelter1fHeliportCollision03CECVerts[8] = {
#include "assets/shelter_1f_heliport_collision_03CEC_verts.inc"
};

static WorldCollisionGridFace _gShelter1fHeliportCollision03CECFaces[4] = {
#include "assets/shelter_1f_heliport_collision_03CEC_faces.inc"
};

static s16 _gShelter1fHeliportCollision03CECCells[6] = {
#include "assets/shelter_1f_heliport_collision_03CEC_cells.inc"
};

#define GRID_CELL(i) (&_gShelter1fHeliportCollision03CECCells[i])
static s16* _gShelter1fHeliportCollision03CECTable[1] = {
#include "assets/shelter_1f_heliport_collision_03CEC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFollowCollisionSource = { NULL, _gShelter1fHeliportCollision03CECNormals, _gShelter1fHeliportCollision03CECVerts, _gShelter1fHeliportCollision03CECFaces, _gShelter1fHeliportCollision03CECTable, 131, 227, 1, 1, 4000, 4 };

WorldCollisionRoomResources D_shelter_1f_heliport_801812D0[1] = {
    { &gFollowCollisionGrid, D_shelter_1f_heliport_80182178, D_shelter_1f_heliport_80182508, NULL },
};

WorldCoordRoomLighting D_shelter_1f_heliport_801812E0[1] = {
    { D_shelter_1f_heliport_80182160, D_shelter_1f_heliport_80182B44 },
};

u8* D_shelter_1f_heliport_801812E8[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_1f_heliport_801812EC[1] = { 12 };

DirectionWarpEntry D_shelter_1f_heliport_801812F0[2] = {
    { { { .word = 3072 }, 9920, 0, 3450 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2792, 0, 2880 }, { 0, 0, 0, 0 }, 0x55040002, 0x55040001, DIRECTION_WARP_SOUND_NONE, 11, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 440, 0, 4340 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2792, 0, 2880 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelter1fHeliportCollision043B4Normals[12] = {
#include "assets/shelter_1f_heliport_collision_043B4_normals.inc"
};

static SVECTOR _gShelter1fHeliportCollision043B4Verts[75] = {
#include "assets/shelter_1f_heliport_collision_043B4_verts.inc"
};

static WorldCollisionGridFace _gShelter1fHeliportCollision043B4Faces[40] = {
#include "assets/shelter_1f_heliport_collision_043B4_faces.inc"
};

static s16 _gShelter1fHeliportCollision043B4Cells[172] = {
#include "assets/shelter_1f_heliport_collision_043B4_cells.inc"
};

#define GRID_CELL(i) (&_gShelter1fHeliportCollision043B4Cells[i])
static s16* _gShelter1fHeliportCollision043B4Table[9] = {
#include "assets/shelter_1f_heliport_collision_043B4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFollowCollisionGrid = { NULL, _gShelter1fHeliportCollision043B4Normals, _gShelter1fHeliportCollision043B4Verts, _gShelter1fHeliportCollision043B4Faces, _gShelter1fHeliportCollision043B4Table, 0, 2800, 3, 3, 4000, 40 };

ViewCamera D_shelter_1f_heliport_80181998[12] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x3CE8, -3400 } }, 235 },
    { { { { 338, 0, -4082 }, { -81, 4095, -6 }, { 4081, 81, 337 } }, { -550, 1690, -2890 } }, 230 },
    { { { { 604, 0, 4051 }, { 79, 4095, -11 }, { -4050, 80, 604 } }, { -7570, 1700, -2750 } }, 230 },
    { { { { 3910, 0, 1218 }, { -32, 4094, 105 }, { -1217, -110, 3909 } }, { -2572, 1124, -1708 } }, 269 },
    { { { { -4049, 0, 618 }, { -54, 4079, -359 }, { -615, -363, -4033 } }, { -8235, 928, -4313 } }, 289 },
    { { { { 576, -68, 4054 }, { -143, 4092, 89 }, { -4052, -154, 573 } }, { -3633, 810, -4286 } }, 289 },
    { { { { 3673, -67, 1810 }, { -125, 4073, 407 }, { -1807, -420, 3651 } }, { -1386, 1256, -5618 } }, 289 },
    { { { { 2691, -68, 3086 }, { -5, 4094, 95 }, { -3087, -66, 2690 } }, { -2397, 1279, -5079 } }, 320 },
    { { { { -2704, -68, 3075 }, { -153, 4092, -44 }, { -3071, -144, -2705 } }, { -2303, 1279, -7476 } }, 348 },
    { { { { -4043, -68, 647 }, { -23, 4085, 285 }, { -650, 278, -4034 } }, { -1300, 1420, -7060 } }, 380 },
    { { { { 381, -42, -4077 }, { -3172, 2570, -323 }, { 2562, 3188, 206 } }, { -5360, 5890, -3200 } }, 230 },
    { { { { -1836, -49, 3660 }, { 2496, 2978, 1292 }, { -2677, 2810, -1305 } }, { -3140, 1450, -1920 } }, 329 },
};

SpriteBatch D_shelter_1f_heliport_80181B48[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_1f_heliport_80181B58[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 72, 1239, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -40, 1539, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -16, 1500, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 8, 1500, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -48, 150, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -24, 1500, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 0, 1500, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 24, 1500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -56, 1350, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -24, 1375, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 1375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -64, 1175, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 40, 1375, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -32, 1175, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 0, 1200, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 32, 1200, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 64, 1193, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -72, 1100, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -32, 1100, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 1100, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 48, 1100, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_heliport_80181CFC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181D14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181D24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181D34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181D44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181D54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181D64[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181D74[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181D84[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_1f_heliport_80181D94[13] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 1100, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 1075, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 80, 1175, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 1100, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 1050, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 104, 1050, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 88, 1025, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 72, 1000, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 64, 900, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 104, 1150, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 104, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 88, 1150, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 88, 1175, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_heliport_80181E98[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_heliport_80181EB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_1f_heliport_80181EC0[12] = {
    { { .empty = D_shelter_1f_heliport_80181B48 }, D_shelter_1f_heliport_80181B48, NULL },
    { { .elements = D_shelter_1f_heliport_80181B58 }, D_shelter_1f_heliport_80181CFC, NULL },
    { { .empty = D_shelter_1f_heliport_80181D14 }, D_shelter_1f_heliport_80181D14, NULL },
    { { .empty = D_shelter_1f_heliport_80181D24 }, D_shelter_1f_heliport_80181D24, NULL },
    { { .empty = D_shelter_1f_heliport_80181D34 }, D_shelter_1f_heliport_80181D34, NULL },
    { { .empty = D_shelter_1f_heliport_80181D44 }, D_shelter_1f_heliport_80181D44, NULL },
    { { .empty = D_shelter_1f_heliport_80181D54 }, D_shelter_1f_heliport_80181D54, NULL },
    { { .empty = D_shelter_1f_heliport_80181D64 }, D_shelter_1f_heliport_80181D64, NULL },
    { { .empty = D_shelter_1f_heliport_80181D74 }, D_shelter_1f_heliport_80181D74, NULL },
    { { .empty = D_shelter_1f_heliport_80181D84 }, D_shelter_1f_heliport_80181D84, NULL },
    { { .elements = D_shelter_1f_heliport_80181D94 }, D_shelter_1f_heliport_80181E98, NULL },
    { { .empty = D_shelter_1f_heliport_80181EB0 }, D_shelter_1f_heliport_80181EB0, NULL },
};

WorldCoordPointLight D_shelter_1f_heliport_80181F50[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5450, -4000, 3600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, 4500, 5000 },
};

WorldCoordSpotLight D_shelter_1f_heliport_80181FB0[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 700, -1250, -1510 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, { 2469, 809, 3165, 0 }, 0x4E20, 0x4E20, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 450, -1500, 9740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, { 2574, 780, -3089, 0 }, 0x3A98, 0x3A98, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7900, -1750, 6890 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, { 2537, 992, -3058, 0 }, 0x4E20, 0x4E20, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7550, -1500, -6360 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, { 2627, 0, 3142, 0 }, 0x4E20, 0x4E20, 887 },
};

WorldCoordRoomLights D_shelter_1f_heliport_80182160[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_1f_heliport_80181F50), D_shelter_1f_heliport_80181F50, ARRAY_SIZE(D_shelter_1f_heliport_80181FB0), D_shelter_1f_heliport_80181FB0 },
};

WorldCollisionTrigger D_shelter_1f_heliport_80182178[12] = {
    { NULL, NULL, NULL, { 4017, -3248, 3516, 0 }, { { 131, -3712, -3671, 0 }, { -135, -3712, 3667, 0 }, { 131, 3712, -3671, 0 }, { -135, 3712, 3667, 0 } }, { 4104, 0, 148, 0 }, { 0, 0, 4096, 0 }, 5221, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4122, -3200, 3577, 0 }, { { -135, -3712, 3667, 0 }, { 131, -3712, -3670, 0 }, { -135, 3712, 3667, 0 }, { 131, 3712, -3670, 0 } }, { -4104, 0, -150, 0 }, { 0, 0, 4096, 0 }, 5221, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 94, -3232, 5853, 0 }, { { 1452, -3712, -676, 0 }, { -1451, -3712, 677, 0 }, { 1452, 3712, -676, 0 }, { -1451, 3712, 677, 0 } }, { 1733, 0, 3719, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 110, -3232, 6014, 0 }, { { -1468, -3712, 669, 0 }, { 1468, -3712, -669, 0 }, { -1468, 3712, 669, 0 }, { 1468, 3712, -669, 0 } }, { -1702, 0, -3733, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6829, -3296, 606, 0 }, { { 1337, -3712, 569, 0 }, { -1337, -3712, -569, 0 }, { 1337, 3712, 569, 0 }, { -1337, 3712, -569, 0 } }, { -1611, 0, 3782, 0 }, { 0, 0, 4096, 0 }, 3982, 0, 11, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6813, -3168, 766, 0 }, { { -1306, -3712, -569, 0 }, { 1307, -3712, 570, 0 }, { -1306, 3712, -569, 0 }, { 1307, 3712, 570, 0 } }, { 1642, 0, -3771, 0 }, { 0, 0, 4096, 0 }, 3974, 0, 5, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9152, -3232, 575, 0 }, { { -1160, -3712, 828, 0 }, { 1160, -3712, -827, 0 }, { -1160, 3712, 828, 0 }, { 1160, 3712, -827, 0 } }, { -2388, 0, -3347, 0 }, { 0, 0, 4096, 0 }, 3974, 0, 5, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9183, -3200, 350, 0 }, { { 1154, -3712, -836, 0 }, { -1153, -3712, 836, 0 }, { 1154, 3712, -836, 0 }, { -1153, 3712, 836, 0 } }, { 2411, 0, 3327, 0 }, { 0, 0, 4096, 0 }, 3974, 0, 11, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2847, -3200, 5856, 0 }, { { 1449, -3712, 711, 0 }, { -1448, -3712, -710, 0 }, { 1449, 3712, 711, 0 }, { -1448, 3712, -710, 0 } }, { -1807, 0, 3682, 0 }, { 0, 0, 4096, 0 }, 4047, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2846, -3265, 6046, 0 }, { { -1448, -3712, -710, 0 }, { 1449, -3712, 711, 0 }, { -1448, 3712, -710, 0 }, { 1449, 3712, 711, 0 } }, { 1806, 0, -3684, 0 }, { 0, 0, 4096, 0 }, 4047, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6653, -2944, 3869, 0 }, { { 134, -3712, -3670, 0 }, { -133, -3712, 3670, 0 }, { 134, 3712, -3670, 0 }, { -133, 3712, 3670, 0 } }, { 4105, 0, 149, 0 }, { 0, 0, 4096, 0 }, 5221, 0, 11, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6782, -2912, 4031, 0 }, { { -178, -3712, 3669, 0 }, { 178, -3712, -3668, 0 }, { -178, 3712, 3669, 0 }, { 178, 3712, -3668, 0 } }, { -4092, 0, -199, 0 }, { 0, 0, 4096, 0 }, 5221, 0, 2, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_1f_heliport_80182508[21] = {
    { NULL, NULL, NULL, { 416, -48, 4704, 0 }, { { -448, 0, -608, 0 }, { 448, 0, -608, 0 }, { -448, 0, 608, 0 }, { 448, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 754, WORLD_COLLISION_TRIGGER_ACTION_WARP, 28, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6960, -64, -1168, 0 }, { { -336, 0, -688, 0 }, { 1424, 0, -688, 0 }, { -336, 0, 1392, 0 }, { 1424, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1991, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 832, -64, 3040, 0 }, { { -1008, 0, -848, 0 }, { 1104, 0, -848, 0 }, { -1008, 0, 1104, 0 }, { 1104, 0, 1104, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1557, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2880, -64, 5056, 0 }, { { -1552, 0, -1328, 0 }, { 752, 0, -1328, 0 }, { -1552, 0, 816, 0 }, { 752, 0, 816, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 2039, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, 6944, 0 }, { { -624, 0, -1168, 0 }, { 1296, 0, -1168, 0 }, { -624, 0, 400, 0 }, { 1296, 0, 400, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1740, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1968, -64, 7760, 0 }, { { -1120, 0, -288, 0 }, { 1120, 0, -288, 0 }, { -1120, 0, 288, 0 }, { 1120, 0, 288, 0 } }, { 0, 4114, 0, 0 }, { 600, 0, -4053, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 33, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -64, 1504, 0 }, { { -688, 0, -496, 0 }, { 1264, 0, -496, 0 }, { -688, 0, 1168, 0 }, { 1264, 0, 1168, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_CAP, 34, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3344, -64, 5232, 0 }, { { -512, 0, -736, 0 }, { 512, 0, -736, 0 }, { -512, 0, 736, 0 }, { 512, 0, 736, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 896, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4416, -64, 5232, 0 }, { { -512, 0, -752, 0 }, { 512, 0, -752, 0 }, { -512, 0, 752, 0 }, { 512, 0, 752, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 909, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3872, -64, 4432, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3888, -64, 5920, 0 }, { { -1024, 0, -1904, 0 }, { 1024, 0, -1904, 0 }, { -1024, 0, -688, 0 }, { 1024, 0, -688, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2157, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2880, -64, 4992, 0 }, { { 0, 0, -1120, 0 }, { 896, 0, -1120, 0 }, { 0, 0, 960, 0 }, { 896, 0, 960, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1431, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, 4928, 0 }, { { -512, 0, -1120, 0 }, { 640, 0, -1120, 0 }, { -512, 0, 960, 0 }, { 640, 0, 960, 0 } }, { 0, 4108, 0, 0 }, { 4096, 0, 0, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3168, -64, 3872, 0 }, { { -512, 0, -192, 0 }, { 1920, 0, -192, 0 }, { -512, 0, 960, 0 }, { 1920, 0, 960, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, -4091, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9759, -64, 1519, 0 }, { { -648, 0, -163, 0 }, { 97, 0, -661, 0 }, { -96, 0, 662, 0 }, { 649, 0, 164, 0 } }, { 0, 4106, 0, 0 }, { -3703, 0, 1751, 0 }, 668, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5616, -64, 5296, 0 }, { { -3280, 0, -368, 0 }, { 3280, 0, -368, 0 }, { -3280, 0, 368, 0 }, { 3280, 0, 368, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 3298, WORLD_COLLISION_TRIGGER_ACTION_CAP, 36, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2784, -64, 6592, 0 }, { { -448, 0, -1376, 0 }, { 448, 0, -1376, 0 }, { -448, 0, 1376, 0 }, { 448, 0, 1376, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_CAP, 36, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5040, -64, 1216, 0 }, { { -2080, 0, -368, 0 }, { 2080, 0, -368, 0 }, { -2080, 0, 368, 0 }, { 2080, 0, 368, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 2111, WORLD_COLLISION_TRIGGER_ACTION_CAP, 38, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8208, -64, -2592, 0 }, { { -816, 0, -368, 0 }, { 816, 0, -368, 0 }, { -816, 0, 368, 0 }, { 816, 0, 368, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_CAP, 39, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2880, -64, 3200, 0 }, { { -448, 0, -1376, 0 }, { 448, 0, -1376, 0 }, { -448, 0, 1376, 0 }, { 448, 0, 1376, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_CAP, 42, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3904, -64, 6080, 0 }, { { -1248, 0, -2624, 0 }, { 1248, 0, -2624, 0 }, { -1248, 0, -832, 0 }, { 1248, 0, -832, 0 } }, { 0, 4117, 0, 0 }, { 4096, 0, 0, 0 }, 2896, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_1f_heliport_80182B44[13] = {
    { .viewCount = ARRAY_SIZE(D_shelter_1f_heliport_80182B44) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1838, 1819, 1496, 1785 } },
    { .color = { 1854, 1818, 1567, 1800 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

AreaResource D_shelter_1f_heliport_80182BAC[3] = {
    { 116, 615, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gStrideWalkTasks },
    { 115, 605, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_260500_80159DB0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_1f_heliport_80182BD0[3] = {
    { 116, 615, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gStrideWalkTasks },
    { 144, 604, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_260400_80154C18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_1f_heliport_80182BF4[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AF30, D_shelter_1f_heliport_80182BAC },
    { D_map_neo_ark_8017AF80, D_shelter_1f_heliport_80182BD0 },
    { NULL, NULL },
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

WorldCollisionFootstepSounds D_shelter_1f_heliport_80182C5C = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_shelter_1f_heliport_80182C68[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_1f_heliport_80182C70[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_1f_heliport_80182C5C },
};

WorldCollisionSurfaceProperties* D_shelter_1f_heliport_80182C78[8] = {
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C70,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
};

static s32 Shop_Data_80187628 = 0;

static EquipmentWeaponSupply* Shop_Data_8018762C = NULL;

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_shelter_1f_heliport_80182CB0[4] = {
    0,
    2,
    68,
    -32,
};

RoomLatchedEvent gRoomEventLatched = { 0 };

static inline s32     Shop_AddItemCount(s32 item, s32 count);
static __inline__ s32 _shelter1fHeliportStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/room_event_staged_task.inc.c"

/// State handlers of the room's controller task: installing its message
/// table, a per-frame state that runs `func_shelter_1f_heliport_801807C0`,
/// and the kill.
static const TaskFuncTable3 D_shelter_1f_heliport_8017D710 = {
    {
        func_shelter_1f_heliport_80180658,
        func_shelter_1f_heliport_80180748,
        taskKill,
    },
};

static __inline__ s32 _shelter1fHeliportStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_heliport_80182CB0_value = 0;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, 1);
            }
            taskSpawnFromTable(&D_shelter_1f_heliport_80181194, 0, 0, 0);
            D_shelter_1f_heliport_80182CB0_value = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_heliport_801800A0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    RoomLatchedEvent event;

    *dst = *src;
    mapNeoArkResolveRoomVariant(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_1F_TENT && src->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_1, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestScriptStop(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_2, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
    if (src->areaId == GAME_AREA_SHELTER_1F_BULWARK) {
        if (gameFlagGetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS) == 0 && gGameSession->location.loc.variant == 1) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_RunCapCmd1(0x2B);
            }
            return 2;
        }
        event.capCmd   = 0x29;
        event.stageSnd = 0x55040001;
        event.flagId   = 0;
        event.fade     = 1;
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            sndEvtRequestScriptStop(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_1, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            sndEvtRequestScriptStop(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_2, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        }
        return _shelter1fHeliportStartEvent(dst, &event);
    }
    return 1;
}

void func_shelter_1f_heliport_801802AC(s32 arg0)
{
    Task* task;
    Task* slotA;

    task  = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    slotA = task;
    if (task == NULL) {
        task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
    if (slotA != NULL && gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE) == 1) {
        D_shelter_1f_heliport_80181204.vy = 0;
    } else {
        D_shelter_1f_heliport_80181204.vy = 0x2710;
    }
    followCollisionRebuild(task->extra.tmd->coords, &D_shelter_1f_heliport_80181204);
}

s32 func_shelter_1f_heliport_80180334(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    WorldCollisionTrigger* node;
    s32                    found;

    if (arg2 == 0x124 && gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE) == 1 && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        found = 0;
        node  = Gp_PendingObj4C;
        while (node != NULL) {
            if (node->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && node->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && node->hit != 0) {
                found = 1;
                break;
            }
            node  = node->next;
            found = 0;
        }

        if (found != 0) {
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 9);
            taskSpawnFromTableOnDefaultList(&D_actor_161500_80136CDC, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

s32 func_shelter_1f_heliport_8018041C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 need;

    switch (arg2) {
        case 0x21:
            Gp_MsgPlayerWeapon(0);
            taskSpawnFromTable(&D_shelter_1f_heliport_801811C8, 0, 0x21, 0);
            break;
        case 0x22:
            need = 1;
            if (gGameSession->location.loc.variant == 1) {
                need = 2;
            }
            Gp_SpawnIfCapIdle(gameFlagGetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_B) >= need ? 0x22 : 0x25, 0);
            break;
    }
    return 0;
}

s32 func_shelter_1f_heliport_801804BC(Task* arg0, s32 arg1, RoomEventMsg* in, s32 arg3)
{
    switch (in->warp) {
        case 1:
            if (gGameSession->location.loc.variant == 1) {
                func_actor_260500_80149EBC();
            }
            if (gGameSession->location.loc.variant == 2) {
                func_actor_260400_80149E38();
            }
            break;
        case 2:
            func_actor_161500_80132038();
            break;
        case 3:
            func_actor_161500_80131FBC();
            break;
        case 4:
            func_actor_161500_80132110();
            break;
        case 5:
            func_actor_161500_801322A0();
            break;
    }
    return 0;
}

#include "../../shared/cap_dialogue_loop.inc.c"

static void func_shelter_1f_heliport_80180658(Task* arg0)
{
    arg0->msgTable = D_shelter_1f_heliport_801811A0;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 1) {
        func_actor_260500_80149E80();
    }
    if (gGameSession->location.loc.variant == 2) {
        func_actor_260400_80149FA4();
    }
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        func_actor_161500_8013230C();
    }
    func_shelter_1f_heliport_801802AC(0);
    func_shelter_1f_heliport_801807C0();
    gpuResetAndInvalidateModelBuffers();
    tmdResetAuxHeapAndRestoreBuffers();
    sndEvtRequestScriptStart(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_1, 0, 0);
    sndEvtRequestScriptStart(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_2, 0, 0);
    arg0->state = arg0->state + 1;
}

static void func_shelter_1f_heliport_80180748(Task* task)
{
    func_shelter_1f_heliport_801807C0();
}

/// Runs the task's current state through its three-entry state table, copied
/// onto the stack before the call.
void func_shelter_1f_heliport_80180768(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_heliport_8017D710;
    sp.funcs[task->state](task);
}

static void func_shelter_1f_heliport_801807C0(void)
{
    s32 i;
    s32 idx = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;

    if (gGameSession->location.loc.variant < 3 && idx < 12) {
        if (D_shelter_1f_heliport_801811D4[idx][0] != 0) {
            for (i = 0; i < 4; i++) {
                Gp_MsgSlot4Chain(i, D_shelter_1f_heliport_801811D4[idx][i]);
            }
        }
    }
}

#include "../../shared/follow_collision_rebuild.inc.c"

void shelter1fHeliportNoOpEffectTask(Task* unusedTask)
{
}
