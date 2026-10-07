#include "rooms/acropolis_fire_escape.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stage_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"
#include "../../shared/glow_draw.h"

extern WorldCollisionTrigger D_acropolis_fire_escape_8018252C[12];

extern UiObjectDesc D_800611E4;

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row captions of the play-data panel, one per row.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
static u8 Telephone_Data_80181A70[];

/// Suffix appended after a percentage.
static u8 Telephone_Data_80181A78[];

/// Help strings handed to the UI holder while the cursor rests on a row of the
/// play-data panel, one per row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// Lists of the play-data panel, the usage panel and the telephone menu; the
/// row descriptor both panels spawn; and the play-data and usage panels the
/// telephone menu's rows open.
static UiList       Telephone_Data_80181C44;
static UiList       Telephone_Data_80181C6C;
static UiObjectDesc Telephone_Data_80181C90;
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;
static UiList       Telephone_Data_80181CF4;

/// Task table the cutscene (entry 0) and its sound task (entry 1) are spawned
/// from.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// Message table of the room's message task.
extern TaskMessageEntry D_acropolis_fire_escape_80181D3C[];

/// Task table holding the room's ambient-sound task.
extern TaskDesc D_acropolis_fire_escape_80181D64[];

/// Circle table the glow and flare tasks read their wedge corners from, in
/// 4.12 fixed point: entry `i` is a corner's y and entry `i + 4` its x.
extern s16 D_acropolis_fire_escape_80181D7C[];

/// Object whose `field_4A` bit 0x40 the message task clears each frame while
/// the slot-4 task does not answer message 0x7D6.

/// Level the ambient-sound task last set for sound event 0x510F0005.
extern s32 D_acropolis_fire_escape_80183040;

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* gRoomCutsceneSoundTask;

/// Parameters of the cutscene the 0x13F0 message handler starts.
extern RoomCutsceneRec D_acropolis_fire_escape_80183048;

#define TELEPHONE_TITLE_BYTES "Telephone\0\1\0"
#include "../../shared/telephone.h"

/// Key-item request handled by this room; the payload is an item ID.
enum { ACROPOLIS_FIRE_ESCAPE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Initializes a glow quad with RGB at vertex 2 and a black rim elsewhere.
///
/// `quad` must be a stable, side-effect-free pointer to a writable POLY_G4;
/// SDK setters evaluate it repeatedly. Each colour expression is evaluated
/// once and narrows to a byte on assignment. This void expression sequences
/// the header and rim stores before reading the centre colour. Geometry and
/// queueing remain the caller's responsibility; no identifiers are captured.
#define ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, red, green, blue) \
    ((void)(setPolyG4((quad)),                                       \
            setRGB0((quad), 0, 0, 0),                                \
            setRGB1((quad), 0, 0, 0),                                \
            setRGB2((quad), (red), (green), (blue)),                 \
            setRGB3((quad), 0, 0, 0)))

static void func_acropolis_fire_escape_8017FE50(Task* task);
static void func_acropolis_fire_escape_8017FECC(Task* task);

s32        func_acropolis_fire_escape_8017F9F8(Task*, s32, s32, s32);
s32        func_acropolis_fire_escape_8017FD98(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _acropolisFireEscapeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _acropolisFireEscapeIgnoreSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 unusedArg);

void func_acropolis_fire_escape_8017FB40(Task*);

extern WorldCollisionGrid     D_acropolis_fire_escape_801822A8[1];
extern WorldCollisionOccluder D_acropolis_fire_escape_801828BC[2];
extern WorldCollisionTrigger  D_acropolis_fire_escape_801822CC[8];
extern WorldCoordRoomLights   D_acropolis_fire_escape_80182B54[1];

extern SpriteDrawArea D_acropolis_fire_escape_80182D44[2];
extern SpriteDrawArea D_acropolis_fire_escape_80182DF4[2];
extern SpriteBatch    D_acropolis_fire_escape_80182B6C[2];
extern SpriteBatch    D_acropolis_fire_escape_80182B7C[2];
extern SpriteBatch    D_acropolis_fire_escape_80182C90[3];
extern SpriteBatch    D_acropolis_fire_escape_80182CA8[2];
extern SpriteBatch    D_acropolis_fire_escape_80182D1C[3];
extern SpriteBatch    D_acropolis_fire_escape_80182D34[2];
extern SpriteBatch    D_acropolis_fire_escape_80182DBC[3];
extern SpriteBatch    D_acropolis_fire_escape_80182DD4[2];
extern SpriteBatch    D_acropolis_fire_escape_80182DE4[2];
extern SpriteSource   D_acropolis_fire_escape_80182B8C[13];
extern SpriteSource   D_acropolis_fire_escape_80182CB8[5];
extern SpriteSource   D_acropolis_fire_escape_80182D58[5];

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_acropolis_fire_escape_80181D3C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_fire_escape_8017FD98 },
    { ACROPOLIS_FIRE_ESCAPE_MESSAGE_USE_KEY_ITEM, _acropolisFireEscapeRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, func_acropolis_fire_escape_8017F9F8 },
    { ROOM_MESSAGE_SOUND, _acropolisFireEscapeIgnoreSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_acropolis_fire_escape_80181D64[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_fire_escape_8017FB40, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s16 D_acropolis_fire_escape_80181D7C[24] = {
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    3784,
    4096,
    3784,
    2896,
    1567,
    0,
    -1567,
    -2896,
    -3784,
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    0,
};

WorldCollisionRoomResources D_acropolis_fire_escape_80181DAC[1] = {
    { D_acropolis_fire_escape_801822A8, D_acropolis_fire_escape_801822CC, D_acropolis_fire_escape_8018252C, D_acropolis_fire_escape_801828BC },
};

u8* D_acropolis_fire_escape_80181DBC[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_fire_escape_80181DC0[1] = { 10 };

WorldCoordRoomLighting D_acropolis_fire_escape_80181DC4[1] = {
    { D_acropolis_fire_escape_80182B54, NULL },
};

DirectionWarpEntry D_acropolis_fire_escape_80181DCC[3] = {
    { { { .word = 3072 }, 2628, 1, -183 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 2628, 1, -183 }, { 0, 0, 0, 0 }, 0x510F0003, 0x510F0002, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 490 },
    { { { .word = 0 }, -4294, -1799, 1240 }, { 0, 0, 0, 0 }, { { .word = 0 }, -4294, -1799, 1240 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -4294, -2450, 340 }, { 0, 0, 0, 0 }, { { .word = 0 }, -4294, -1800, 1240 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

SVECTOR gAcropolisFireEscapeCollision04CE8Normals[10] = {
#include "assets/acropolis_fire_escape_collision_04CE8_normals.inc"
};

SVECTOR gAcropolisFireEscapeCollision04CE8Verts[55] = {
#include "assets/acropolis_fire_escape_collision_04CE8_verts.inc"
};

WorldCollisionGridFace gAcropolisFireEscapeCollision04CE8Faces[23] = {
#include "assets/acropolis_fire_escape_collision_04CE8_faces.inc"
};

static s16 _gAcropolisFireEscapeCollision04CE8Cells[122] = {
#include "assets/acropolis_fire_escape_collision_04CE8_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisFireEscapeCollision04CE8Cells[i])
static s16* _gAcropolisFireEscapeCollision04CE8Table[9] = {
#include "assets/acropolis_fire_escape_collision_04CE8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_fire_escape_801822A8[1] = {
    { NULL, gAcropolisFireEscapeCollision04CE8Normals, gAcropolisFireEscapeCollision04CE8Verts, gAcropolisFireEscapeCollision04CE8Faces, _gAcropolisFireEscapeCollision04CE8Table, 5000, 3800, 3, 3, 4000, 23 },
};

WorldCollisionTrigger D_acropolis_fire_escape_801822CC[8] = {
    { NULL, NULL, NULL, { 2010, -2256, 1376, 0 }, { { -1024, -2352, 0, 0 }, { 1024, -2352, 0, 0 }, { -1024, 2352, 0, 0 }, { 1024, 2352, 0, 0 } }, { 0, 0, -4106, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2016, -2272, 839, 0 }, { { -1024, 2352, 0, 0 }, { 1024, 2352, 0, 0 }, { -1024, -2352, 0, 0 }, { 1024, -2352, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1862, -2272, 3879, 0 }, { { -1055, -2352, -1086, 0 }, { 1055, -2352, 1086, 0 }, { -1055, 2352, -1086, 0 }, { 1055, 2352, 1086, 0 } }, { 2940, 0, -2859, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1927, -2272, 3609, 0 }, { { 1181, -2352, 1169, 0 }, { -1180, -2352, -1168, 0 }, { 1181, 2352, 1169, 0 }, { -1180, 2352, -1168, 0 } }, { -2882, 0, 2910, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2465, -2336, 3935, 0 }, { { -8, -2352, -1240, 0 }, { 8, -2352, 1240, 0 }, { -8, 2352, -1240, 0 }, { 8, 2352, 1240, 0 } }, { 4107, 0, -28, 0 }, { 0, 0, 4096, 0 }, 2648, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2016, -2304, 3936, 0 }, { { 0, -2352, 1024, 0 }, { 0, -2352, -1024, 0 }, { 0, 2352, 1024, 0 }, { 0, 2352, -1024, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4066, -2304, 3234, 0 }, { { 1380, -2352, -319, 0 }, { -1387, -2352, 308, 0 }, { 1380, 2352, -319, 0 }, { -1387, 2352, 308, 0 } }, { 907, 0, 4005, 0 }, { 0, 0, 4096, 0 }, 2745, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3859, -2176, 3301, 0 }, { { -1314, -2352, 291, 0 }, { 1303, -2352, -308, 0 }, { -1314, 2352, 291, 0 }, { 1303, 2352, -308, 0 } }, { -915, 0, -3994, 0 }, { 0, 0, 4096, 0 }, 2697, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_fire_escape_8018252C[12] = {
    { NULL, NULL, NULL, { -338, -32, 4144, 0 }, { { -110, 0, -848, 0 }, { 110, 0, -848, 0 }, { -110, 0, 848, 0 }, { 110, 0, 848, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 853, WORLD_COLLISION_TRIGGER_ACTION_FACING, 9, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3095, -1856, 4095, 0 }, { { -128, 0, -768, 0 }, { 128, 0, -768, 0 }, { -128, 0, 768, 0 }, { 128, 0, 768, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 9, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4486, -1856, 1184, 0 }, { { 384, 0, -176, 0 }, { 384, 0, 176, 0 }, { -384, 0, -176, 0 }, { -384, 0, 176, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 422, WORLD_COLLISION_TRIGGER_ACTION_FACING, 26, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4320, -2384, 544, 0 }, { { -592, 1552, 0, 0 }, { 592, 1552, 0, 0 }, { -592, -1552, 0, 0 }, { 592, -1552, 0, 0 } }, { 0, 0, 4099, 0 }, { 0, 0, 4096, 0 }, 1659, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 16, 51, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2544, -32, 0, 0 }, { { -320, 0, -768, 0 }, { 320, 0, -768, 0 }, { -320, 0, 768, 0 }, { 320, 0, 768, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 832, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1440, -96, -1680, 0 }, { { -480, 0, 1216, 0 }, { -480, 0, -320, 0 }, { 1568, 0, 1216, 0 }, { 1568, 0, -320, 0 } }, { 0, 4095, 0, 0 }, { -201, 0, 4091, 0 }, 1982, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1312, -96, 1856, 0 }, { { -416, 0, 480, 0 }, { -416, 0, -480, 0 }, { 416, 0, 480, 0 }, { 416, 0, -480, 0 } }, { 0, 4102, 0, 0 }, { 4095, 0, 0, 0 }, 633, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3392, -1856, 2080, 0 }, { { -592, 0, 720, 0 }, { -592, 0, -720, 0 }, { 592, 0, 720, 0 }, { 592, 0, -720, 0 } }, { 0, 4095, 0, 0 }, { -4077, 0, 401, 0 }, 931, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4288, -2544, 160, 0 }, { { 768, 0, -304, 0 }, { 768, 0, 304, 0 }, { -768, 0, -304, 0 }, { -768, 0, 304, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 824, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -1835, 3984, 0 }, { { -270, 0, -832, 0 }, { 270, 0, -832, 0 }, { -270, 0, 832, 0 }, { 270, 0, 832, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1248, -64, 224, 0 }, { { -416, 0, 480, 0 }, { -416, 0, -480, 0 }, { 416, 0, 480, 0 }, { 416, 0, -480, 0 } }, { 0, 4102, 0, 0 }, { 4095, 0, 0, 0 }, 633, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2016, -64, -1712, 0 }, { { -686, 0, -384, 0 }, { 686, 0, -384, 0 }, { -686, 0, 384, 0 }, { 686, 0, 384, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 783, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_acropolis_fire_escape_801828BC[2] = {
    { NULL, NULL, { -944, -2320, 2624, 0 }, { { -1808, -3344, 0, 0 }, { 1808, -3344, 0, 0 }, { -1808, 3344, 0, 0 }, { 1808, 3344, 0, 0 } }, { 0, 0, -4096, 0 }, 3797, 1, 0 },
    { NULL, NULL, { -864, -2272, -448, 0 }, { { 0, -3296, 3088, 0 }, { 0, -3296, -3088, 0 }, { 0, 3296, 3088, 0 }, { 0, 3296, -3088, 0 } }, { -4101, 0, 0, 0 }, 4492, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_acropolis_fire_escape_80182934[2] = {
    { 10, 115, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_311500_80169338 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_fire_escape_8018294C[5] = {
    { NULL, NULL },
    { D_map_akropolis_8017BD0C, D_acropolis_fire_escape_80182934 },
    { D_map_akropolis_8017BD0C, D_acropolis_fire_escape_80182934 },
    { D_map_akropolis_8017BD0C, D_acropolis_fire_escape_80182934 },
    { NULL, NULL },
};

/// The fire escape's five white point lights, contributing in every room view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits, initially `ONE` in each channel. The loaded room overlay
/// owns these writable records: coordinate updates parent and compose their
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// must not survive unloading the overlay.
static WorldCoordPointLight _gAcropolisFireEscapePointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2020, -2380, 663 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 760,
        .outer = 4206,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2020, -2380, 2820 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -10, -2380, 3860 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3870, -4220, 3850 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3870, -4220, 1740 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 500,
        .outer = 2000,
    },
};

WorldCoordRoomLights D_acropolis_fire_escape_80182B54[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisFireEscapePointLights), _gAcropolisFireEscapePointLights, 0, NULL },
};

SpriteBatch D_acropolis_fire_escape_80182B6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fire_escape_80182B7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fire_escape_80182B8C[13] = {
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, -104, 1025, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, -48, 1025, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, 8, 1025, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -48, -120, 1025, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -48, -48, 1025, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -48, 8, 1025, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -72, -120, 1025, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -72, -48, 1025, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -72, 24, 1025, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -96, -120, 1025, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -96, -32, 1025, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -96, 56, 1025, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 240 } }, -160, -120, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fire_escape_80182C90[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fire_escape_80182CA8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fire_escape_80182CB8[5] = {
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -32, -120, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -48, -120, 650, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 240 } }, -72, -120, 625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 240 } }, -112, -120, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 240 } }, -160, -120, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fire_escape_80182D1C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fire_escape_80182D34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_fire_escape_80182D44[2] = {
    { { 103, 0, 215, 239 }, 1000 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_acropolis_fire_escape_80182D58[5] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 112, 490, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 516, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 160, 112 } }, -88, -120, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 176, 88 } }, -96, -8, 500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 200, 40 } }, -104, 80, 500, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fire_escape_80182DBC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fire_escape_80182DD4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fire_escape_80182DE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_fire_escape_80182DF4[2] = {
    { { 19, 11, 0, 0 }, 250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_acropolis_fire_escape_80182E08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_fire_escape_80182E18[10] = {
    { { .empty = D_acropolis_fire_escape_80182B6C }, D_acropolis_fire_escape_80182B6C, NULL },
    { { .empty = D_acropolis_fire_escape_80182B7C }, D_acropolis_fire_escape_80182B7C, NULL },
    { { .elements = D_acropolis_fire_escape_80182B8C }, D_acropolis_fire_escape_80182C90, NULL },
    { { .empty = D_acropolis_fire_escape_80182CA8 }, D_acropolis_fire_escape_80182CA8, NULL },
    { { .elements = D_acropolis_fire_escape_80182CB8 }, D_acropolis_fire_escape_80182D1C, NULL },
    { { .empty = D_acropolis_fire_escape_80182D34 }, D_acropolis_fire_escape_80182D34, D_acropolis_fire_escape_80182D44 },
    { { .elements = D_acropolis_fire_escape_80182D58 }, D_acropolis_fire_escape_80182DBC, NULL },
    { { .empty = D_acropolis_fire_escape_80182DD4 }, D_acropolis_fire_escape_80182DD4, NULL },
    { { .empty = D_acropolis_fire_escape_80182DE4 }, D_acropolis_fire_escape_80182DE4, D_acropolis_fire_escape_80182DF4 },
    { { .empty = D_acropolis_fire_escape_80182DE4 }, D_acropolis_fire_escape_80182DE4, NULL },
};

ViewCamera D_acropolis_fire_escape_80182E90[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1200, 0x7530, -1600 } }, 680 },
    { { { { -4039, 0, 676 }, { 308, 3645, 1842 }, { -602, 1868, -3595 } }, { -2490, 2330, -2698 } }, 230 },
    { { { { 4020, 0, 784 }, { 142, 4028, -728 }, { -771, 742, 3953 } }, { -2414, 1819, 1108 } }, 230 },
    { { { { -519, 0, 4062 }, { -431, 4072, -55 }, { -4039, -434, -517 } }, { -3951, 691, -4228 } }, 257 },
    { { { { -1415, 0, 3843 }, { -298, 4083, -109 }, { -3832, -317, -1411 } }, { 390, 2571, -4428 } }, 230 },
    { { { { -3980, 0, -964 }, { 41, 4092, -169 }, { 963, -174, -3977 } }, { 4348, 2874, -5793 } }, 257 },
    { { { { 4058, 0, -556 }, { 73, 4060, 532 }, { 551, -537, 4022 } }, { -1702, 511, 4673 } }, 230 },
    { { { { 1699, 0, 3726 }, { 2331, 3195, -1063 }, { -2906, 2562, 1325 } }, { -1417, 1157, -1423 } }, 230 },
    { { { { -1538, 0, -3796 }, { -2236, 3309, 906 }, { 3067, 2413, -1242 } }, { 3307, 3439, -2448 } }, 230 },
};

WorldCollisionFootstepSounds D_acropolis_fire_escape_80182FD4 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionFootstepSounds D_acropolis_fire_escape_80182FE0 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionFootstepSounds D_acropolis_fire_escape_80182FEC = {
    0x1000000D,
    0x1000000F,
    0x1000000D,
};

WorldCollisionSurfaceProperties D_acropolis_fire_escape_80182FF8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_fire_escape_80182FEC },
};

WorldCollisionSurfaceProperties D_acropolis_fire_escape_80183000[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_fire_escape_80182FD4 },
};

WorldCollisionSurfaceProperties D_acropolis_fire_escape_80183008[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_fire_escape_80182FEC },
};

WorldCollisionSurfaceProperties D_acropolis_fire_escape_80183010[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_fire_escape_80182FE0 },
};

WorldCollisionSurfaceProperties D_acropolis_fire_escape_80183018[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_fire_escape_80182FEC },
};

WorldCollisionSurfaceProperties* D_acropolis_fire_escape_80183020[8] = {
    D_acropolis_fire_escape_80182FF8,
    D_acropolis_fire_escape_80183000,
    D_acropolis_fire_escape_80183008,
    D_acropolis_fire_escape_80183010,
    D_acropolis_fire_escape_80182FF8,
    D_acropolis_fire_escape_80182FF8,
    D_acropolis_fire_escape_80183018,
    D_acropolis_fire_escape_80182FF8,
};

s32 D_acropolis_fire_escape_80183040;

Task* gRoomCutsceneSoundTask;

RoomCutsceneRec D_acropolis_fire_escape_80183048;

#include "../../shared/telephone.inc.c"

void func_acropolis_fire_escape_8017EA68(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// States of the room's message task, run by `func_acropolis_fire_escape_8017FF24`:
/// install the message table and spawn the ambient-sound task, run the
/// per-frame check, die.
static const TaskFuncTable3 D_acropolis_fire_escape_8017D6A4 = {
    {
        func_acropolis_fire_escape_8017FE50,
        func_acropolis_fire_escape_8017FECC,
        taskKill,
    },
};

/// The `0x13F0` message handler of `D_acropolis_fire_escape_80181D3C`.
/// Event 4 runs CAP command 0xB the first time (setting game flag 0x16A), and
/// afterwards starts the room's cutscene in view 9 with CAP slot and file 1.
/// Event 3 raises flag 0x155 to at least 6 and starts CAP 3. Event 1 starts
/// CAP 9 when the slot-4 task answers message 0x7D6, CAP 1 otherwise.
s32 func_acropolis_fire_escape_8017F9F8(Task* task, s32 msgId, s32 event, s32 arg3)
{
    Task* slot;
    s32   cap;
    s32   result;

    if (event == 4) {
        if (gameFlagGetNibble(GAME_FLAG_FIRE_ESCAPE_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_FIRE_ESCAPE_FIRST_SCENE, 1);
            capRunCommandWithTransition(0xB);
            return 0;
        }
        D_acropolis_fire_escape_80183048.view            = 9;
        D_acropolis_fire_escape_80183048.capSlot         = 1;
        D_acropolis_fire_escape_80183048.capFile         = 1;
        D_acropolis_fire_escape_80183048.skipScene       = 0;
        D_acropolis_fire_escape_80183048.startSound      = 0x510F0001;
        D_acropolis_fire_escape_80183048.endSound        = 0x510F0004;
        D_acropolis_fire_escape_80183048.sceneSound      = 0x510F0007;
        D_acropolis_fire_escape_80183048.afterSceneSound = 0x510F0008;
        taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 3, &D_acropolis_fire_escape_80183048);
    }
    if (event == 3) {
        if (gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) < 6) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 6);
        }
        capSpawnEventIfIdle(3, CAP_EVENT_PAUSE_ACTORS);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 7);
    }
    if (event == 1) {
        slot = sceneFindPlacedActor(0);
        cap  = 1;
        if (slot != NULL) {
            result = taskMessageDispatch(slot, ACTOR_MESSAGE_IS_PRESENT, 0, 0);
            cap    = 9;
            if (result == 0) {
                cap = 1;
            }
        }
        capSpawnEventIfIdle(cap, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}

/// Task body of the room's ambient sound. Each frame it picks a level for the
/// looping sound event 0x510F0005 from the current view (views 2-5 and 8 hear
/// it, the rest silence it) and, when the level changes, starts the sound,
/// fades it out or retunes it. Entering view 8 while the save's scene event is
/// 5 also advances it to 7 and spawns `Stage_MusicTaskDesc`.
void func_acropolis_fire_escape_8017FB40(Task* task)
{
    s32 vol;
    s32 prev;

    switch (task->state) {
        case 0:
            D_acropolis_fire_escape_80183040 = 0;
            task->state                      = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    switch (gGameSession->location.loc.view) {
        case 8:
            vol = 0x64;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == 5) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 7;
                gStageMusicParams.fadeOutTicks                      = 1;
                gStageMusicParams.field_2                           = 1;
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, 0, 0);
                gGameSession->flowFlags = 0;
            }
            break;
        case 2:
        case 3:
            vol = 0x1E;
            break;
        case 4:
        case 5:
            vol = 0xF;
            break;
        default:
            vol = 0;
            break;
    }

    prev = D_acropolis_fire_escape_80183040;
    if (vol == prev) {
        return;
    }
    if (prev == 0) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((0x64 - vol) * 127) / 100));
    } else if (vol == 0) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0x1E);
    } else {
        sndEvtRequestScriptMix(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((0x64 - vol) * 127) / 100));
    }
    D_acropolis_fire_escape_80183040 = vol;
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// The `0x13EE` message handler of `D_acropolis_fire_escape_80181D3C`: copies
/// the incoming save location onto the outgoing one, fades the ambient sound
/// out when `queryOnly` is 0, and for location 0xE with `queryOnly` 0 sets the
/// outgoing `room` to 2 when game flag 2 is 3, to 1 otherwise.
s32 func_acropolis_fire_escape_8017FD98(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    if (src->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0xF);
    }
    if (src->areaId == GAME_AREA_ACROPOLIS_BRIDGE && src->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 3) {
            dst->room = 2;
        } else {
            dst->room = 1;
        }
    }
    return 1;
}

/// Refuses every key-item use in this room, returning zero without consuming it.
///
/// The receiver, message ID, item ID and unused second argument are ignored.
static s32 _acropolisFireEscapeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_FIRE_ESCAPE_KEY_ITEM_USE_REFUSED = 0 };

    return ACROPOLIS_FIRE_ESCAPE_KEY_ITEM_USE_REFUSED;
}

/// Ignores room sound commands and returns zero without starting a sound.
static s32 _acropolisFireEscapeIgnoreSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 unusedArg)
{
    return 0;
}

/// First state of the room's message task: installs the message table, takes
/// pointer slot 7, spawns the ambient-sound task and, when `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent` is 5,
/// sets the session's flow flags to 8.
static void func_acropolis_fire_escape_8017FE50(Task* task)
{
    task->msgTable = D_acropolis_fire_escape_80181D3C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_acropolis_fire_escape_80181D64, 0, 0, 0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == 5) {
        gGameSession->flowFlags = GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY;
    }
    task->state = task->state + 1;
}

/// Per-frame state of the room's message task: clears bit 0x40 of
/// `D_acropolis_fire_escape_8018252C[5].field_4A` unless the slot-4 task exists
/// and answers message 0x7D6.
static void func_acropolis_fire_escape_8017FECC(Task* task)
{
    Task* slot;

    slot = sceneFindPlacedActor(0);
    if (slot == NULL || taskMessageDispatch(slot, ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) {
        {
            WorldCollisionTrigger* object = &D_acropolis_fire_escape_8018252C[5];
            object->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
        }
    }
}

/// Runs the room's message task: copies the state table onto the stack and
/// calls the entry for the task's current state.
void func_acropolis_fire_escape_8017FF24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_fire_escape_8017D6A4;
    sp.funcs[task->state](task);
}

/// Task body of the room's effect emitter. On its first frame it spawns effect
/// 0x6008C at a fixed offset from the task's coordinate; every later frame
/// outside a cutscene it spawns effect 0x6004F at the offset and with the
/// parameters of the current view, for views 3, 6, 8 and 9.
void func_acropolis_fire_escape_8017FF7C(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case 0:
            work->move.vx = 0xB58;
            work->move.vy = -0x822;
            work->move.vz = -0xE5;
            effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLICKER_LIGHT, coord, 0x42000, &work->move);
            task->state = task->state + 1;
            break;
        case 1:
            if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                if (gGameSession->location.loc.view == 3) {
                    work->move.vx = 0x48F;
                    work->move.vy = -0x391;
                    work->move.vz = 0x686;
                    effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, coord, (6 << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | 14, &work->move);
                }
                if (gGameSession->location.loc.view == 8) {
                    work->move.vx = 0x48F;
                    work->move.vy = -0x391;
                    work->move.vz = 0x686;
                    effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, coord, ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL | (3 << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | 14, &work->move);
                }
                if (gGameSession->location.loc.view == 6) {
                    work->move.vx = -0xC1F;
                    work->move.vy = -0xD10;
                    work->move.vz = 0x8E0;
                    effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, coord, ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN | (4 << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | 8, &work->move);
                }
                if (gGameSession->location.loc.view == 9) {
                    work->move.vx = -0xC1F;
                    work->move.vy = -0xD10;
                    work->move.vz = 0x8E0;
                    effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, coord, ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL | ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN | (2 << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | 8, &work->move);
                }
            }
            break;
    }
}

/// Queues an initialized fire-escape glow quad with additive blending.
///
/// `quad` is a word-aligned frame-arena packet; its DMA link and
/// semitransparency bit are updated. `sortingDepth` points to a stable signed
/// camera-Z/4 word, after any bias, already clipped to at least `GLOW_MIN_DEPTH`.
/// Unsigned depth scaling by `gDisplayState.otDepthShift` (0..3) selects a
/// wrapped tag in 0..1023. The current table must contain all 1024 depth tags,
/// and the frame arena must have `sizeof(DR_TPAGE)` bytes available.
/// The depth pointer is not retained. The quad and the added draw-mode packet
/// must remain live until GPU completion; the draw mode persists until replaced.
static inline void _acropolisFireEscapeQueueGlow(POLY_G4* quad, const s32* sortingDepth)
{
    enum { ACROPOLIS_FIRE_ESCAPE_GLOW_DEPTH_TO_TAG_INDEX_SHIFT = 4 };

    addPrim(&gGpuCurrentOt[((u32)*sortingDepth << gDisplayState.otDepthShift) >> ACROPOLIS_FIRE_ESCAPE_GLOW_DEPTH_TO_TAG_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt))],
            quad);
    // Prepending the draw mode after the quad makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, *sortingDepth);
}

void acropolisFireEscapeFlickerLightTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_FLICKER_VIEW_MASK           = (1 << (2 - 1)) | (1 << (3 - 1)) | (1 << (7 - 1)),
        ACROPOLIS_FIRE_ESCAPE_FLICKER_DEPTH_BIAS          = 32,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_MODE_FRAME_MASK     = 31,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_MODE_MASK           = 3,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT        = 8,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK         = 0xFF,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_DIM_MAX             = 16,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_SOUND_MIN           = 32,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK     = 0x30,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_LOW_BRIGHTNESS_MASK = 0x10,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM              = 0,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM_ODD_FRAMES   = 1,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_STEADY              = 2,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_DIM_RANDOM          = 3,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SCALE        = 0x600,
        ACROPOLIS_FIRE_ESCAPE_FLICKER_INNER_RADIUS_SCALE  = 0xC0,
        ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS           = 16,
    };

    EffectWork*           effectWork;
    GfxCoord*             coord;
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    s32                   wasDim;
    s32                   cornerIndex;
    u16                   brightness;

    effectWork = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    wasDim     = 0;
    // The installed room has 1-based view IDs; only views 2, 3 and 7 show the lamp.
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN && ((ACROPOLIS_FIRE_ESCAPE_FLICKER_VIEW_MASK >> (gGameSession->location.loc.view - 1)) & 1)) {
        // Project the lamp and pull its sorting depth forward before sizing it.
        actorRenderComposeCoord(coord);
        projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
        projection->worldPos.vx = coord->workm.t[0];
        projection->worldPos.vy = coord->workm.t[1];
        projection->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projection->worldPos);
        gte_rtps();
        gte_stsxy(&projection->screenPos);
        gte_stszotz(&projection->otz);
        projection->otz -= ACROPOLIS_FIRE_ESCAPE_FLICKER_DEPTH_BIAS;
        if (projection->otz >= GLOW_MIN_DEPTH) {
            if (!(gDisplayState.animFrame & ACROPOLIS_FIRE_ESCAPE_FLICKER_MODE_FRAME_MASK)) {
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectWork->index = (gRandomLcgState >> 16) & ACROPOLIS_FIRE_ESCAPE_FLICKER_MODE_MASK;
            }
            // Choose flicker brightness, then sound only a dim-to-bright transition.
            brightness = effectWork->scale;
            if (effectWork->scale <= ACROPOLIS_FIRE_ESCAPE_FLICKER_DIM_MAX) {
                wasDim = 1;
            }
            switch (effectWork->index) {
                case ACROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM:
                    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectWork->scale = (gRandomLcgState >> 16) & ACROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    break;
                case ACROPOLIS_FIRE_ESCAPE_FLICKER_RANDOM_ODD_FRAMES:
                    if (gDisplayState.animFrame & 1) {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        brightness      = (gRandomLcgState >> 16) & ACROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    }
                    effectWork->scale = brightness;
                    break;
                case ACROPOLIS_FIRE_ESCAPE_FLICKER_STEADY:
                    effectWork->scale = ACROPOLIS_FIRE_ESCAPE_FLICKER_BRIGHTNESS_MASK;
                    break;
                case ACROPOLIS_FIRE_ESCAPE_FLICKER_DIM_RANDOM:
                    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectWork->scale = (gRandomLcgState >> 16) & ACROPOLIS_FIRE_ESCAPE_FLICKER_LOW_BRIGHTNESS_MASK;
                    break;
            }
            if (wasDim && effectWork->scale >= ACROPOLIS_FIRE_ESCAPE_FLICKER_SOUND_MIN) {
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_FIRE_ESCAPE_LIGHT_FLICKER, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            projection->outerRadius = (((task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK) * ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SCALE) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLICKER_RADIUS_MASK) * ACROPOLIS_FIRE_ESCAPE_FLICKER_INNER_RADIUS_SCALE) / projection->otz;
            // Eight wedges in each of three concentric intensity layers.
            for (cornerIndex = 0; cornerIndex < ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; cornerIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, effectWork->scale >> 1, effectWork->scale >> 1, effectWork->scale >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 2]) >> GLOW_TRIG_SHIFT);
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, effectWork->scale, effectWork->scale, effectWork->scale);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, (u8)effectWork->scale * 4, (u8)effectWork->scale * 4, (u8)effectWork->scale * 4);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 4]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex]) >> (GLOW_TRIG_SHIFT + 3));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 5]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 1]) >> (GLOW_TRIG_SHIFT + 3));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 6]) >> (GLOW_TRIG_SHIFT + 3));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[cornerIndex + 2]) >> (GLOW_TRIG_SHIFT + 3));
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    }
}

void acropolisFireEscapeFlareTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_FALLING_BIT    = 0x80,
        ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK     = 0x7F,
        ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL_RADIUS_SHIFT  = 10,
        ACROPOLIS_FIRE_ESCAPE_FLARE_INNER_RADIUS_SHIFT   = 7,
        ACROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT = 9,
        ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS          = 16,
    };

    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    GfxCoord*             coord;
    EffectWork*           effectWork;
    s32                   partIndex;
    s32                   pulsePhase;
    s32                   pulseOrOptions; // Folded pulse level, then the packed spawn options
    s32                   radiusScale;
    s16                   intensity;
    s16                   cyan;
    s16                   halfIntensity;

    coord      = task->extra.coordBody->coord;
    effectWork = task->spawnArg2.pointer;
    // Each invocation projects and draws one frame before retiring its counted work.
    actorRenderComposeCoord(coord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = (u16)coord->workm.t[0];
    projection->worldPos.vy = (u16)coord->workm.t[1];
    projection->worldPos.vz = (u16)coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= GLOW_MIN_DEPTH) {
        // Fold the low-byte phase into a 0..127 triangle wave and select red or cyan.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_RATE_MASK;
        cyan        = (task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN_SHIFT) & 1;
        if (pulsePhase & ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_FALLING_BIT) {
            pulseOrOptions = ~pulsePhase & ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK;
        } else {
            pulseOrOptions = pulsePhase & ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_LEVEL_MASK;
        }
        intensity      = pulseOrOptions * 2;
        pulseOrOptions = task->spawnArg1.value;
        if (pulseOrOptions < 0) {
            radiusScale             = (pulseOrOptions >> ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK;
            projection->outerRadius = (radiusScale << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << ACROPOLIS_FIRE_ESCAPE_FLARE_INNER_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; partIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, (intensity * (cyan ^ 1)) >> 1, (cyan * intensity) >> 1, (cyan * intensity) >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 2]) >> GLOW_TRIG_SHIFT);
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
            // Overlay four half-bright rays on the concentric disc.
            halfIntensity = intensity >> 1;
            for (partIndex = 2; partIndex < ACROPOLIS_FIRE_ESCAPE_GLOW_CIRCLE_STEPS; partIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_fire_escape_80181D7C[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_fire_escape_80181D7C[partIndex - 4]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 4]) >> (GLOW_TRIG_SHIFT - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex]) >> (GLOW_TRIG_SHIFT - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, halfIntensity * (cyan ^ 1), cyan * halfIntensity, cyan * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_fire_escape_80181D7C[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 0xC]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_fire_escape_80181D7C[partIndex + 8]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((pulseOrOptions >> ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << ACROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (((task->spawnArg1.value >> ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) & ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK) << ACROPOLIS_FIRE_ESCAPE_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < 2; partIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD(quad, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->innerRadius) + projection->innerRadius * (partIndex + partIndex);
                _acropolisFireEscapeQueueGlow(quad, &projection->otz);
            }
            // Retained packet reuse: the lines are initialized but the last quad is linked again.
            if (task->spawnArg1.value & ACROPOLIS_FIRE_ESCAPE_FLARE_STREAK_PACKETS) {
                for (partIndex = 0; partIndex < 2; partIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    setLineG3(streak);
                    setRGB0(streak, 0, 0, 0);
                    setRGB1(streak, intensity * (cyan ^ 1), cyan * intensity, cyan * intensity);
                    setRGB2(streak, 0, 0, 0);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (partIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->innerRadius * (partIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (partIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->innerRadius * (partIndex + 1);
                    _acropolisFireEscapeQueueGlow(quad, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    effectKillTask(effectWork, task);
}

#undef ACROPOLIS_FIRE_ESCAPE_INIT_GLOW_QUAD
