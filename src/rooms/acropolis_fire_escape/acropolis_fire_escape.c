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

static void func_acropolis_fire_escape_8017FE50(Task* task);
static void func_acropolis_fire_escape_8017FECC(Task* task);

s32 func_acropolis_fire_escape_8017F9F8(Task*, s32, s32, s32);
s32 func_acropolis_fire_escape_8017FD98(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_acropolis_fire_escape_8017FE40(Task*, s32, s32, s32);
s32 func_acropolis_fire_escape_8017FE48(Task*, s32, s32, s32);

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
    { 5105, func_acropolis_fire_escape_8017FE40 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_fire_escape_8017F9F8 },
    { ROOM_MESSAGE_SOUND, func_acropolis_fire_escape_8017FE48 },
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
    Telephone_MenuTask(task);
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
            Gp_RunCapCmd1(0xB);
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
        Gp_SpawnIfCapIdle(3, 1);
        func_800E3FAC(0xA2, 7);
    }
    if (event == 1) {
        slot = Gp_LookupSlot4(0);
        cap  = 1;
        if (slot != NULL) {
            result = taskMessageDispatch(slot, ACTOR_MESSAGE_IS_PRESENT, 0, 0);
            cap    = 9;
            if (result == 0) {
                cap = 1;
            }
        }
        Gp_SpawnIfCapIdle(cap, 1);
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
        SndEvt_EnqueueType7(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0x1E);
    } else {
        SndEvt_EnqueueTypeA(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0, (s8)(((0x64 - vol) * 127) / 100));
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
        SndEvt_EnqueueType7(SOUND_ACROPOLIS_FIRE_ESCAPE_AMBIENCE, 0xF);
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

/// The `0x13F1` message handler of `D_acropolis_fire_escape_80181D3C`: ignores
/// the message.
s32 func_acropolis_fire_escape_8017FE40(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The `0x13F2` message handler of `D_acropolis_fire_escape_80181D3C`: ignores
/// the message.
s32 func_acropolis_fire_escape_8017FE48(Task* task, s32 msgId, s32 arg2, s32 arg3)
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

    slot = Gp_LookupSlot4(0);
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
            Gp_SpawnEff(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLICKER_LIGHT, coord, 0x42000, &work->move);
            task->state = task->state + 1;
            break;
        case 1:
            if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                if (gGameSession->location.loc.view == 3) {
                    work->move.vx = 0x48F;
                    work->move.vy = -0x391;
                    work->move.vz = 0x686;
                    Gp_SpawnEff(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, coord, 0x60E, &work->move);
                }
                if (gGameSession->location.loc.view == 8) {
                    work->move.vx = 0x48F;
                    work->move.vy = -0x391;
                    work->move.vz = 0x686;
                    Gp_SpawnEff(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, coord, 0x8000030E, &work->move);
                }
                if (gGameSession->location.loc.view == 6) {
                    work->move.vx = -0xC1F;
                    work->move.vy = -0xD10;
                    work->move.vz = 0x8E0;
                    Gp_SpawnEff(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, coord, 0x10408, &work->move);
                }
                if (gGameSession->location.loc.view == 9) {
                    work->move.vx = -0xC1F;
                    work->move.vy = -0xD10;
                    work->move.vz = 0x8E0;
                    Gp_SpawnEff(EFFECT_ACROPOLIS_FIRE_ESCAPE_FLARE, coord, 0x80010208, &work->move);
                }
            }
            break;
    }
}

/// Draws a flickering glow at the task's coordinate while the scene is not in
/// a cutscene and the camera is on views 2, 3 or 7. The coordinate is
/// projected through `GsWSMATRIX` and nothing is drawn unless its biased depth
/// stays beyond 0x10. Every 32 frames `index` picks one of four flicker
/// modes, which set the brightness `scale` each frame: random 0/0x10/0x20/0x30,
/// the same but changing only on odd frames, a steady 0x30, or random 0/0x10.
/// Rising from 0x10 or less to 0x20 or more plays sound event 0x510F0006 panned
/// to the coordinate. The glow is eight `POLY_G4` wedges around the projected
/// point, taken two steps at a time from the circle table, each lit at the
/// centre and black at the rim; it is drawn three times, at the full radius
/// with half the brightness, at half the radius with the brightness, and at an
/// eighth of the radius with four times the brightness (wrapping in a byte).
/// Every wedge takes the semi-transparent tpage of `gpuSetPrimitiveBlendMode`.
void func_acropolis_fire_escape_80180154(Task* task)
{
    EffectWork*           work;
    GfxCoord*             coord;
    RoomGlowRadiiScratch* block;
    POLY_G4*              prim;
    s32                   play;
    s32                   i;
    u16                   level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    play  = 0;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN && ((0x46 >> (gGameSession->location.loc.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        block              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
        block->worldPos.vx = coord->workm.t[0];
        block->worldPos.vy = coord->workm.t[1];
        block->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPos);
        gte_rtps();
        gte_stsxy(&block->screenPos);
        gte_stszotz(&block->otz);
        block->otz -= 0x20;
        if (block->otz > 0x10) {
            if (!(gDisplayState.animFrame & 0x1F)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->index     = (gRandomLcgState >> 16) & 3;
            }
            level = work->scale;
            if (work->scale < 0x11) {
                play = 1;
            }
            switch (work->index) {
                case 0:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->scale     = (gRandomLcgState >> 16) & 0x30;
                    break;
                case 1:
                    if (gDisplayState.animFrame & 1) {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        level           = (gRandomLcgState >> 16) & 0x30;
                    }
                    work->scale = level;
                    break;
                case 2:
                    work->scale = 0x30;
                    break;
                case 3:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->scale     = (gRandomLcgState >> 16) & 0x10;
                    break;
            }
            if (play && work->scale >= 0x20) {
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_FIRE_ESCAPE_LIGHT_FLICKER, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            block->outerRadius = (((task->spawnArg1.value >> 8) & 0xFF) * 0x600) / block->otz;
            block->innerRadius = (((task->spawnArg1.value >> 8) & 0xFF) * 0xC0) / block->otz;
            for (i = 0; i < 0x10; i += 2) {
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, work->scale >> 1, work->scale >> 1, work->scale >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->y0 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                prim->x1 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 12);
                prim->y1 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 12);
                prim->x2 = block->screenPos.vx;
                prim->y2 = block->screenPos.vy;
                prim->x3 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 12);
                prim->y3 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 12);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, work->scale, work->scale, work->scale);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 13);
                prim->y1 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 13);
                prim->x2 = block->screenPos.vx;
                prim->y2 = block->screenPos.vy;
                prim->x3 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 13);
                prim->y3 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 13);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (u8)work->scale * 4, (u8)work->scale * 4, (u8)work->scale * 4);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 15);
                prim->y0 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i]) >> 15);
                prim->x1 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 15);
                prim->y1 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 15);
                prim->x2 = block->screenPos.vx;
                prim->y2 = block->screenPos.vy;
                prim->x3 = block->screenPos.vx + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 15);
                prim->y3 = block->screenPos.vy + ((block->outerRadius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 15);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    }
}

/// Draws a pulsing flare at the task's coordinate, projected through
/// `GsWSMATRIX`, and nothing when its depth is 0x10 or less. The brightness is
/// a triangle wave of `gDisplayState.animFrame` times the low byte of `spawnArg1`; bit 16
/// tints it cyan instead of red, and bits 8-15 size it, divided by the depth.
/// A negative `spawnArg1` draws a radial glow of `POLY_G4` wedges with four
/// longer rays; otherwise a flat diamond of two `POLY_G4`s, with two crossed
/// `LINE_G3` streaks when bit 28 is set. Every primitive takes the
/// semi-transparent tpage of `gpuSetPrimitiveBlendMode`. Finally `spawnArg2` goes to
/// `effectKillTask`.
void func_acropolis_fire_escape_80180B20(Task* task)
{
    RoomGlowRadiiScratch* blk;
    POLY_G4*              prim;
    LINE_G3*              line;
    GfxCoord*             coord;
    void*                 mem;
    s32                   i;
    s32                   pulse;
    s32                   level;
    s32                   height;
    s16                   amp;
    s16                   flip;
    s32                   ampSi;
    s32                   ampHalf;
    u8                    red;
    u8                    cyan;
    s32                   z;
    s32                   shift;
    u32                   otByteOffset;
    u32                   tag;
    u_long*               ot;

    coord = task->extra.coordBody->coord;
    mem   = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    blk              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    blk->worldPos.vx = (u16)coord->workm.t[0];
    blk->worldPos.vy = (u16)coord->workm.t[1];
    blk->worldPos.vz = (u16)coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->worldPos);
    gte_rtps();
    gte_stsxy(&blk->screenPos);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        pulse  = gDisplayState.animFrame;
        pulse *= task->spawnArg1.value & 0xFF;
        flip   = (task->spawnArg1.value >> 16) & 1;
        if (pulse & 0x80) {
            level = ~pulse & 0x7F;
        } else {
            level = pulse & 0x7F;
        }
        amp   = level * 2;
        level = task->spawnArg1.value;
        if (level < 0) {
            height           = (level >> 8) & 0xFF;
            blk->outerRadius = (height << 10) / blk->otz;
            blk->innerRadius = (((task->spawnArg1.value >> 8) & 0xFF) << 7) / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                ampSi          = amp;
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (ampSi * (flip ^ 1)) >> 1, (flip * ampSi) >> 1, (flip * ampSi) >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->screenPos.vx + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->y0 = blk->screenPos.vy + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                prim->x1 = blk->screenPos.vx + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 12);
                prim->y1 = blk->screenPos.vy + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 12);
                prim->x2 = blk->screenPos.vx;
                prim->y2 = blk->screenPos.vy;
                prim->x3 = blk->screenPos.vx + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 12);
                prim->y3 = blk->screenPos.vy + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 12);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, blk->otz);

                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, ampSi * (flip ^ 1), flip * ampSi, flip * ampSi);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->screenPos.vx + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = blk->screenPos.vy + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = blk->screenPos.vx + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 13);
                prim->y1 = blk->screenPos.vy + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 13);
                prim->x2 = blk->screenPos.vx;
                prim->y2 = blk->screenPos.vy;
                prim->x3 = blk->screenPos.vx + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 13);
                prim->y3 = blk->screenPos.vy + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 13);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, blk->otz);
            }
            ampHalf = amp >> 1;
            for (i = 2; i < 0x10; i += 8) {
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    red  = ampHalf * (flip ^ 1);
                    cyan = flip * ampHalf;
                    setRGB2(prim, red, cyan, cyan);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0     = blk->screenPos.vx + ((blk->innerRadius * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                    prim->y0     = blk->screenPos.vy + ((blk->innerRadius * D_acropolis_fire_escape_80181D7C[i - 4]) >> 12);
                    prim->x1     = blk->screenPos.vx + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 11);
                    prim->y1     = blk->screenPos.vy + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i]) >> 11);
                    prim->x2     = blk->screenPos.vx;
                    prim->y2     = blk->screenPos.vy;
                    prim->x3     = blk->screenPos.vx + ((blk->innerRadius * D_acropolis_fire_escape_80181D7C[i + 8]) >> 12);
                    prim->y3     = blk->screenPos.vy + ((blk->innerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                    shift        = gDisplayState.otDepthShift;
                    otByteOffset = (((u32)blk->otz << shift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK;
                    __asm__("" : "+r"(otByteOffset) : "r"(shift), "m"(gDisplayState.otDepthShift));
                    setaddr(prim, getaddr(((u_long*)((otByteOffset) + (uintptr)gGpuCurrentOt))));
                    ot  = ((u_long*)((((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + (uintptr)gGpuCurrentOt));
                    tag = (*ot & GPU_DMA_PACKET_LENGTH_MASK) | ((u32)prim & GPU_DMA_LINK_ADDRESS_MASK);
                    *ot = tag;
                    z   = blk->otz;
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG_USE(z, tag);
                    SOFT_TOUCH_REG_USE(prim, z);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, z);

                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, cyan, cyan);
                } while (0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->screenPos.vx + ((blk->innerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = blk->screenPos.vy + ((blk->innerRadius * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = blk->screenPos.vx + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 8]) >> 12);
                prim->y1 = blk->screenPos.vy + ((blk->outerRadius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->x2 = blk->screenPos.vx;
                prim->y2 = blk->screenPos.vy;
                prim->x3 = blk->screenPos.vx + ((blk->innerRadius * D_acropolis_fire_escape_80181D7C[i + 0xC]) >> 13);
                prim->y3 = blk->screenPos.vy + ((blk->innerRadius * D_acropolis_fire_escape_80181D7C[i + 8]) >> 13);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                z = blk->otz;
                __asm__("" : "+r"(z) : "r"(red), "r"(&D_acropolis_fire_escape_80181D7C[i]));
                __asm__("" : "+r"(prim) : "r"(z), "r"(cyan));
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, z);
            }
        } else {
            blk->outerRadius = (((level >> 8) & 0xFF) << 9) / blk->otz;
            blk->innerRadius = (((task->spawnArg1.value >> 8) & 0xFF) << 9) / blk->otz;
            for (i = 0; i < 2; i++) {
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, amp * (flip ^ 1), flip * amp, flip * amp);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->screenPos.vx - blk->outerRadius;
                prim->x1 = prim->x2 = blk->screenPos.vx;
                prim->x3            = blk->screenPos.vx + blk->outerRadius;
                prim->y0 = prim->y2 = prim->y3 = blk->screenPos.vy;
                prim->y1                       = (blk->screenPos.vy - blk->innerRadius) + blk->innerRadius * (i + i);
                addPrim((&gGpuCurrentOt[((u32)blk->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF]),
                        prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, blk->otz);
            }
            if (task->spawnArg1.value & 0x10000000) {
                for (i = 0; i < 2; i++) {
                    line           = gGpuPrimCursor;
                    gGpuPrimCursor = line + 1;
                    setLineG3(line);
                    setRGB0(line, 0, 0, 0);
                    setRGB1(line, amp * (flip ^ 1), flip * amp, flip * amp);
                    setRGB2(line, 0, 0, 0);
                    line->x0 = blk->screenPos.vx + blk->outerRadius * (i * 3 - 1);
                    line->y0 = blk->screenPos.vy - blk->innerRadius * (i + 1);
                    line->x1 = blk->screenPos.vx;
                    line->y1 = blk->screenPos.vy;
                    line->x2 = blk->screenPos.vx - blk->outerRadius * (i * 3 - 1);
                    line->y2 = blk->screenPos.vy + blk->innerRadius * (i + 1);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, blk->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    effectKillTask(mem, task);
}
