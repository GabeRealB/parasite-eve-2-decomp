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

static void _roomCutsceneSoundTask(Task* task);

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

static void _acropolisFireEscapeInitializeRoomTask(Task* task);
static void _acropolisFireEscapeDisableAbsentActorInteraction(Task* unusedTask);

static s32 _acropolisFireEscapeHandleRoomCommand(Task* unusedTask, s32 messageId, s32 command, s32 unusedArg);
static s32 _acropolisFireEscapeResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _acropolisFireEscapeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _acropolisFireEscapeIgnoreSoundMessage(Task* task, s32 messageId, s32 soundCommand, s32 unusedArg);

static void _acropolisFireEscapeAmbienceTask(Task* task);

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
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_acropolis_fire_escape_80181D3C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisFireEscapeResolveRoomTransition },
    { ACROPOLIS_FIRE_ESCAPE_MESSAGE_USE_KEY_ITEM, _acropolisFireEscapeRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _acropolisFireEscapeHandleRoomCommand },
    { ROOM_MESSAGE_SOUND, _acropolisFireEscapeIgnoreSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_acropolis_fire_escape_80181D64[2] = {
    { { { TASK_BODY_NONE, 32 } }, _acropolisFireEscapeAmbienceTask, { .value = 0 } },
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

void acropolisFireEscapeTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// States of the room's message task, run by `acropolisFireEscapeRoomTask`:
/// install the message table and spawn the ambient-sound task, run the
/// per-frame check, die.
static const TaskFuncTable3 D_acropolis_fire_escape_8017D6A4 = {
    {
        _acropolisFireEscapeInitializeRoomTask,
        _acropolisFireEscapeDisableAbsentActorInteraction,
        taskKill,
    },
};

/// Starts the repeat fire-escape scene using the room's persistent playback record.
///
/// The record and scene resources must remain loaded and unchanged until the
/// runner finishes; its follow-up command advances the room dialogue.
static inline void _acropolisFireEscapeStartRepeatScene(void)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_SCENE_VIEW              = 9,
        ACROPOLIS_FIRE_ESCAPE_SCENE_CAP_SLOT          = 1,
        ACROPOLIS_FIRE_ESCAPE_SCENE_CAP_FILE          = 1,
        ACROPOLIS_FIRE_ESCAPE_SCENE_PLAY              = 0,
        ACROPOLIS_FIRE_ESCAPE_SCENE_TASK              = 0,
        ACROPOLIS_FIRE_ESCAPE_SCENE_FOLLOW_UP_COMMAND = 3,
        ACROPOLIS_FIRE_ESCAPE_SCENE_START_SOUND       = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, 1),
        ACROPOLIS_FIRE_ESCAPE_SCENE_END_SOUND         = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, 4),
        ACROPOLIS_FIRE_ESCAPE_SCENE_PLAYBACK_SOUND    = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, 7),
        ACROPOLIS_FIRE_ESCAPE_SCENE_AFTER_SOUND       = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FIRE_ESCAPE, 8),
    };

    D_acropolis_fire_escape_80183048.view            = ACROPOLIS_FIRE_ESCAPE_SCENE_VIEW;
    D_acropolis_fire_escape_80183048.capSlot         = ACROPOLIS_FIRE_ESCAPE_SCENE_CAP_SLOT;
    D_acropolis_fire_escape_80183048.capFile         = ACROPOLIS_FIRE_ESCAPE_SCENE_CAP_FILE;
    D_acropolis_fire_escape_80183048.skipScene       = ACROPOLIS_FIRE_ESCAPE_SCENE_PLAY;
    D_acropolis_fire_escape_80183048.startSound      = ACROPOLIS_FIRE_ESCAPE_SCENE_START_SOUND;
    D_acropolis_fire_escape_80183048.endSound        = ACROPOLIS_FIRE_ESCAPE_SCENE_END_SOUND;
    D_acropolis_fire_escape_80183048.sceneSound      = ACROPOLIS_FIRE_ESCAPE_SCENE_PLAYBACK_SOUND;
    D_acropolis_fire_escape_80183048.afterSceneSound = ACROPOLIS_FIRE_ESCAPE_SCENE_AFTER_SOUND;
    taskSpawnFromTable(gRoomCutsceneTaskDescs, ACROPOLIS_FIRE_ESCAPE_SCENE_TASK, ACROPOLIS_FIRE_ESCAPE_SCENE_FOLLOW_UP_COMMAND, &D_acropolis_fire_escape_80183048);
}

/// Handles the fire escape's scene, story-progress and placed-actor dialogue commands.
///
/// `ROOM_MESSAGE_COMMAND` supplies integer command 4 for the first/repeat scene,
/// 3 for dialogue progress 6 and objective 7, or 1 for actor-dependent dialogue.
/// Other commands do nothing. Requires loaded room/CAP resources and live save
/// state; repeated scenes borrow the persistent record through completion.
/// Receiver, message ID and second payload are ignored; always returns zero.
static s32 _acropolisFireEscapeHandleRoomCommand(Task* unusedTask, s32 messageId, s32 command, s32 unusedArg)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_COMMAND_SCENE             = 4,
        ACROPOLIS_FIRE_ESCAPE_COMMAND_ADVANCE_DIALOGUE  = 3,
        ACROPOLIS_FIRE_ESCAPE_COMMAND_ACTOR_DIALOGUE    = 1,
        ACROPOLIS_FIRE_ESCAPE_SCENE_UNSEEN              = 0,
        ACROPOLIS_FIRE_ESCAPE_SCENE_SEEN                = 1,
        ACROPOLIS_FIRE_ESCAPE_FIRST_SCENE_CAP_COMMAND   = 11,
        ACROPOLIS_FIRE_ESCAPE_DIALOGUE_PROGRESS         = 6,
        ACROPOLIS_FIRE_ESCAPE_FOLLOW_UP_RESET           = 0,
        ACROPOLIS_FIRE_ESCAPE_DIALOGUE_CAP_COMMAND      = 3,
        ACROPOLIS_FIRE_ESCAPE_DIALOGUE_OBJECTIVE        = 7,
        ACROPOLIS_FIRE_ESCAPE_DIALOGUE_ACTOR_PLACEMENT  = 0,
        ACROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND  = 1,
        ACROPOLIS_FIRE_ESCAPE_ACTOR_PRESENT_CAP_COMMAND = 9,
    };
    Task* actorTask;
    s32   capCommand;
    s32   actorPresent;

    if (command == ACROPOLIS_FIRE_ESCAPE_COMMAND_SCENE) {
        if (gameFlagGetNibble(GAME_FLAG_FIRE_ESCAPE_FIRST_SCENE) == ACROPOLIS_FIRE_ESCAPE_SCENE_UNSEEN) {
            gameFlagSetNibble(GAME_FLAG_FIRE_ESCAPE_FIRST_SCENE, ACROPOLIS_FIRE_ESCAPE_SCENE_SEEN);
            capRunCommandWithTransition(ACROPOLIS_FIRE_ESCAPE_FIRST_SCENE_CAP_COMMAND);
            return 0;
        }
        _acropolisFireEscapeStartRepeatScene();
    }
    if (command == ACROPOLIS_FIRE_ESCAPE_COMMAND_ADVANCE_DIALOGUE) {
        // Advancing dialogue rearms its follow-up without lowering later progress.
        if (gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) < ACROPOLIS_FIRE_ESCAPE_DIALOGUE_PROGRESS) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, ACROPOLIS_FIRE_ESCAPE_FOLLOW_UP_RESET);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ACROPOLIS_FIRE_ESCAPE_DIALOGUE_PROGRESS);
        }
        capSpawnEventIfIdle(ACROPOLIS_FIRE_ESCAPE_DIALOGUE_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_FIRE_ESCAPE_DIALOGUE_OBJECTIVE);
    }
    if (command == ACROPOLIS_FIRE_ESCAPE_COMMAND_ACTOR_DIALOGUE) {
        actorTask  = sceneFindPlacedActor(ACROPOLIS_FIRE_ESCAPE_DIALOGUE_ACTOR_PLACEMENT);
        capCommand = ACROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND;
        if (actorTask != NULL) {
            // Retain the original zero output payload; this actor also writes through it.
            actorPresent = taskMessageDispatch(actorTask, ACTOR_MESSAGE_IS_PRESENT, 0, 0);
            capCommand   = ACROPOLIS_FIRE_ESCAPE_ACTOR_PRESENT_CAP_COMMAND;
            if (actorPresent == 0) {
                capCommand = ACROPOLIS_FIRE_ESCAPE_ACTOR_ABSENT_CAP_COMMAND;
            }
        }
        capSpawnEventIfIdle(capCommand, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}

/// Requests view-dependent ambience and the view-8 story music transition.
///
/// State 0 resets the room's requested-level latch; state 1 updates it each
/// tick. Logical views 2/3, 4/5 and 8 request levels 30%, 15% and 100%; others
/// request silence. Changes start, mix or fade the sound over 30 audio updates.
/// The latch records requests even if the sound queue rejects them. In view 8,
/// music event 5 becomes 7 once, queuing ordinary area music and clearing the
/// session's flow options. Requires live session/save and loaded room sound data.
static void _acropolisFireEscapeAmbienceTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_INIT            = 0,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_UPDATE          = 1,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT          = 0,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT    = 100,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS23_PERCENT = 30,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS45_PERCENT = 15,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX = 127,
        ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FADE_TICKS      = 30,
        ACROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT        = 5,
        ACROPOLIS_FIRE_ESCAPE_VIEW8_MUSIC_EVENT        = 7,
        ACROPOLIS_FIRE_ESCAPE_MUSIC_REQUEST_ORDINARY   = 0,
    };
    s32 levelPercent;
    s32 previousLevelPercent;

    switch (task->state) {
        case ACROPOLIS_FIRE_ESCAPE_AMBIENCE_INIT:
            D_acropolis_fire_escape_80183040 = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT;
            task->state                      = task->state + 1;
            return;
        case ACROPOLIS_FIRE_ESCAPE_AMBIENCE_UPDATE:
            break;
        default:
            return;
    }

    switch (gGameSession->location.loc.view) {
        case 8:
            levelPercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT;
            // Commit the music column before queuing its asynchronous loader.
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ACROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_FIRE_ESCAPE_VIEW8_MUSIC_EVENT;
                gStageMusicParams.fadeOutTicks                      = 1;
                gStageMusicParams.field_2                           = 1;
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, ACROPOLIS_FIRE_ESCAPE_MUSIC_REQUEST_ORDINARY, 0);
                gGameSession->flowFlags = 0;
            }
            break;
        case 2:
        case 3:
            levelPercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS23_PERCENT;
            break;
        case 4:
        case 5:
            levelPercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_VIEWS45_PERCENT;
            break;
        default:
            levelPercent = ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT;
            break;
    }

    previousLevelPercent = D_acropolis_fire_escape_80183040;
    if (levelPercent == previousLevelPercent) {
        return;
    }
    if (previousLevelPercent == ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT - levelPercent) * ACROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX) / ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT));
    } else if (levelPercent == ACROPOLIS_FIRE_ESCAPE_AMBIENCE_SILENT) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FADE_TICKS);
    } else {
        sndEvtRequestScriptMix(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT - levelPercent) * ACROPOLIS_FIRE_ESCAPE_AMBIENCE_ATTENUATION_MAX) / ACROPOLIS_FIRE_ESCAPE_AMBIENCE_FULL_PERCENT));
    }
    D_acropolis_fire_escape_80183040 = levelPercent;
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Resolves departure from the fire escape and fades its ambience on execution.
///
/// Borrows complete request/reply records, which may be the same object.
/// Queries only copy the request. Execution fades ambience over 15 audio
/// updates and selects bridge room 2 at progress 3, room 1 otherwise.
/// Always returns 1 to permit the resolved transition; receiver and ID are unused.
static s32 _acropolisFireEscapeResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_DEPARTURE_FADE_TICKS    = 15,
        ACROPOLIS_FIRE_ESCAPE_BRIDGE_PROGRESS_CHANGED = 3,
        ACROPOLIS_FIRE_ESCAPE_BRIDGE_ROOM_INITIAL     = 1,
        ACROPOLIS_FIRE_ESCAPE_BRIDGE_ROOM_CHANGED     = 2,
        ACROPOLIS_FIRE_ESCAPE_TRANSITION_ALLOWED      = 1
    };

    *reply = *request;
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, ACROPOLIS_FIRE_ESCAPE_DEPARTURE_FADE_TICKS);
    }
    if (request->areaId == GAME_AREA_ACROPOLIS_BRIDGE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == ACROPOLIS_FIRE_ESCAPE_BRIDGE_PROGRESS_CHANGED) {
            reply->room = ACROPOLIS_FIRE_ESCAPE_BRIDGE_ROOM_CHANGED;
        } else {
            reply->room = ACROPOLIS_FIRE_ESCAPE_BRIDGE_ROOM_INITIAL;
        }
    }
    return ACROPOLIS_FIRE_ESCAPE_TRANSITION_ALLOWED;
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

/// Publishes the room receiver and starts its view-dependent ambience.
///
/// State 0 requires a live bodyless task and loaded room/gameplay resources.
/// Saved scene event 5 selects load-only mode for area music. Advances
/// to the interaction-check state even if the ambience task fails to spawn.
static void _acropolisFireEscapeInitializeRoomTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_INITIAL_AMBIENCE_TASK = 0,
        ACROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT     = 5,
    };

    task->msgTable = D_acropolis_fire_escape_80181D3C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_acropolis_fire_escape_80181D64, ACROPOLIS_FIRE_ESCAPE_INITIAL_AMBIENCE_TASK, 0, 0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == ACROPOLIS_FIRE_ESCAPE_ENTRY_MUSIC_EVENT) {
        gGameSession->flowFlags = GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY;
    }
    task->state = task->state + 1;
}

/// Disables the placed actor's interaction when the actor is absent.
///
/// Idle room-state callback; the receiver is unused. Queries placement 0 with
/// `ACTOR_MESSAGE_IS_PRESENT` and clears the trigger's ENABLED bit on a zero
/// reply or missing actor. Never re-enables it. The presence query retains the
/// original zero second payload, although this room's actor writes through it.
static void _acropolisFireEscapeDisableAbsentActorInteraction(Task* unusedTask)
{
    enum { ACROPOLIS_FIRE_ESCAPE_ACTOR_INTERACTION_TRIGGER = 5 };
    Task* actorTask;

    actorTask = sceneFindPlacedActor(0);
    if (actorTask == NULL || taskMessageDispatch(actorTask, ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) {
        WorldCollisionTrigger* interaction = &D_acropolis_fire_escape_8018252C[ACROPOLIS_FIRE_ESCAPE_ACTOR_INTERACTION_TRIGGER];
        interaction->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
}

void acropolisFireEscapeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_acropolis_fire_escape_8017D6A4;
    stateHandlers.funcs[task->state](task);
}

/// Places a flare from the emitter's reusable local-offset storage.
///
/// Spawning copies XYZ into the new coordinate before this storage is reused;
/// the new work also retains the pointer. The one-frame flare never reads it.
static inline void _acropolisFireEscapeSpawnFlare(EffectWork* emitterWork, GfxCoord* emitterCoord,
                                                  s16 offsetX, s16 offsetY, s16 offsetZ, s32 options)
{
    emitterWork->move.vx = offsetX;
    emitterWork->move.vy = offsetY;
    emitterWork->move.vz = offsetZ;
    effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, emitterCoord, options, &emitterWork->move);
}

void acropolisFireEscapeLightEmitterTask(Task* task)
{
    enum {
        ACROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_INITIALIZE = 0,
        ACROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_EMIT       = 1,
        // Radius byte 32; the lamp does not read the retained upper bits.
        ACROPOLIS_FIRE_ESCAPE_FLICKER_SPAWN_OPTIONS = 0x42000,
        ACROPOLIS_FIRE_ESCAPE_RED_DIAMOND_VIEW      = 3,
        ACROPOLIS_FIRE_ESCAPE_RED_RADIAL_VIEW       = 8,
        ACROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_VIEW     = 6,
        ACROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_VIEW      = 9,
        ACROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE        = 14,
        ACROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE       = 8,
        ACROPOLIS_FIRE_ESCAPE_RED_DIAMOND_RADIUS    = 6,
        ACROPOLIS_FIRE_ESCAPE_RED_RADIAL_RADIUS     = 3,
        ACROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_RADIUS   = 4,
        ACROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_RADIUS    = 2,
    };
    EffectWork* emitterWork;
    GfxCoord*   emitterCoord;

    emitterWork  = task->spawnArg2.pointer;
    emitterCoord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_INITIALIZE:
            // The persistent lamp snapshots this local placement once.
            emitterWork->move.vx = 2904;
            emitterWork->move.vy = -2082;
            emitterWork->move.vz = -229;
            effectSpawn(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLICKER_LIGHT, emitterCoord, ACROPOLIS_FIRE_ESCAPE_FLICKER_SPAWN_OPTIONS, &emitterWork->move);
            task->state = task->state + 1;
            break;
        case ACROPOLIS_FIRE_ESCAPE_LIGHT_EMITTER_EMIT:
            // Actor pause/hide modes still emit; a cancellation update suppresses emission.
            if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                if (gGameSession->location.loc.view == ACROPOLIS_FIRE_ESCAPE_RED_DIAMOND_VIEW) {
                    _acropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, 1167, -913, 1670,
                                                   (ACROPOLIS_FIRE_ESCAPE_RED_DIAMOND_RADIUS << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | ACROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE);
                }
                if (gGameSession->location.loc.view == ACROPOLIS_FIRE_ESCAPE_RED_RADIAL_VIEW) {
                    _acropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, 1167, -913, 1670,
                                                   ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL | (ACROPOLIS_FIRE_ESCAPE_RED_RADIAL_RADIUS << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | ACROPOLIS_FIRE_ESCAPE_RED_PULSE_RATE);
                }
                if (gGameSession->location.loc.view == ACROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_VIEW) {
                    _acropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, -3103, -3344, 2272,
                                                   ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN | (ACROPOLIS_FIRE_ESCAPE_CYAN_DIAMOND_RADIUS << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | ACROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE);
                }
                if (gGameSession->location.loc.view == ACROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_VIEW) {
                    _acropolisFireEscapeSpawnFlare(emitterWork, emitterCoord, -3103, -3344, 2272,
                                                   ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL | ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN | (ACROPOLIS_FIRE_ESCAPE_CYAN_RADIAL_RADIUS << ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT) | ACROPOLIS_FIRE_ESCAPE_CYAN_PULSE_RATE);
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
