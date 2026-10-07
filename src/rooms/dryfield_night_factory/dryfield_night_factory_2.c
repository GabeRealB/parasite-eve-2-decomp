#include "rooms/dryfield_night_factory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "dryfield_night_factory_private.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
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
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/dryfield_factory.h"

#include "rooms/room_common.h"
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
/// Selects the nighttime factory instance for shared room declarations and code.
///
/// Keep this binding through all factory implementation fragments.
#define DRYFIELD_TIME DRYFIELD_NIGHT
#include "../../shared/factory_lift.h"

static void _actionPromptResetDefault(Task* task);

extern TaskDesc gRoomEventTaskDesc;

/// The world-space points the room's three glow discs are drawn at.

s32  factoryIgnoreMessage(Task*, s32, s32, s32);
void factoryPanelRun(Task*);
void factoryPromptTask(Task*);
void factoryPanelTrigger(Task*, s32, s32, s32);

static WorldCollisionGridFace _gDryfieldNightFactoryCollision0A630Faces[72];
static SVECTOR                _gDryfieldNightFactoryCollision0A630Normals[28];
static SVECTOR                _gDryfieldNightFactoryCollision0A630Verts[170];
static s16*                   _gDryfieldNightFactoryCollision0A630Table[8];

static TmdBone _gDryfieldNightFactoryDryfieldFactoryModel06604Skeleton[1] = {
#include "assets/dryfield_factory_model_06604_skeleton.inc"
};

static u32 _gDryfieldNightFactoryDryfieldFactoryModel06604PartVerts[1] = {
#include "assets/dryfield_factory_model_06604_partVerts.inc"
};

static SVECTOR _gDryfieldNightFactoryDryfieldFactoryModel06604Verts[306] = {
#include "assets/dryfield_factory_model_06604_verts.inc"
};

static SVECTOR _gDryfieldNightFactoryDryfieldFactoryModel06604Normals[353] = {
#include "assets/dryfield_factory_model_06604_normals.inc"
};

static u32 _gDryfieldNightFactoryDryfieldFactoryModel06604Stream[2811] = {
#include "assets/dryfield_factory_model_06604_stream.inc"
};

static TmdSource _gDryfieldNightFactoryDryfieldFactoryModel06604 = {
    0,
    19284,
    0,
    1,
    _gDryfieldNightFactoryDryfieldFactoryModel06604PartVerts,
    _gDryfieldNightFactoryDryfieldFactoryModel06604Verts,
    _gDryfieldNightFactoryDryfieldFactoryModel06604Normals,
    _gDryfieldNightFactoryDryfieldFactoryModel06604Skeleton,
    _gDryfieldNightFactoryDryfieldFactoryModel06604Stream,
};

static TmdBone _gDryfieldNightFactoryDryfieldFactoryModel093E4Skeleton[1] = {
#include "assets/dryfield_factory_model_093E4_skeleton.inc"
};

static u32 _gDryfieldNightFactoryDryfieldFactoryModel093E4PartVerts[1] = {
#include "assets/dryfield_factory_model_093E4_partVerts.inc"
};

static SVECTOR _gDryfieldNightFactoryDryfieldFactoryModel093E4Verts[25] = {
#include "assets/dryfield_factory_model_093E4_verts.inc"
};

static SVECTOR _gDryfieldNightFactoryDryfieldFactoryModel093E4Normals[28] = {
#include "assets/dryfield_factory_model_093E4_normals.inc"
};

static u32 _gDryfieldNightFactoryDryfieldFactoryModel093E4Stream[139] = {
#include "assets/dryfield_factory_model_093E4_stream.inc"
};

static TmdSource _gDryfieldNightFactoryDryfieldFactoryModel093E4 = {
    0,
    856,
    0,
    1,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4PartVerts,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4Verts,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4Normals,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4Skeleton,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4Stream,
};

static SVECTOR _gDryfieldNightFactoryCollision09660Normals[2] = {
#include "assets/dryfield_night_factory_collision_09660_normals.inc"
};

static SVECTOR _gDryfieldNightFactoryCollision09660Verts[8] = {
#include "assets/dryfield_night_factory_collision_09660_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightFactoryCollision09660Faces[2] = {
#include "assets/dryfield_night_factory_collision_09660_faces.inc"
};

static s16 _gDryfieldNightFactoryCollision09660Cells[4] = {
#include "assets/dryfield_night_factory_collision_09660_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightFactoryCollision09660Cells[i])
static s16* _gDryfieldNightFactoryCollision09660Table[1] = {
#include "assets/dryfield_night_factory_collision_09660_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryBarrierTemplate = { NULL, _gDryfieldNightFactoryCollision09660Normals, _gDryfieldNightFactoryCollision09660Verts, _gDryfieldNightFactoryCollision09660Faces, _gDryfieldNightFactoryCollision09660Table, -4464, -3949, 1, 1, 4000, 2 };

static SVECTOR _gDryfieldNightFactoryCollision09730Normals[4] = {
#include "assets/dryfield_night_factory_collision_09730_normals.inc"
};

static SVECTOR _gDryfieldNightFactoryCollision09730Verts[8] = {
#include "assets/dryfield_night_factory_collision_09730_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightFactoryCollision09730Faces[4] = {
#include "assets/dryfield_night_factory_collision_09730_faces.inc"
};

static s16 _gDryfieldNightFactoryCollision09730Cells[10] = {
#include "assets/dryfield_night_factory_collision_09730_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightFactoryCollision09730Cells[i])
static s16* _gDryfieldNightFactoryCollision09730Table[2] = {
#include "assets/dryfield_night_factory_collision_09730_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryLiftTemplate = { NULL, _gDryfieldNightFactoryCollision09730Normals, _gDryfieldNightFactoryCollision09730Verts, _gDryfieldNightFactoryCollision09730Faces, _gDryfieldNightFactoryCollision09730Table, 750, 2191, 1, 2, 4000, 4 };

static SVECTOR _gDryfieldNightFactoryCollision097FCNormals[4] = {
#include "assets/dryfield_night_factory_collision_097FC_normals.inc"
};

static SVECTOR _gDryfieldNightFactoryCollision097FCVerts[8] = {
#include "assets/dryfield_night_factory_collision_097FC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightFactoryCollision097FCFaces[4] = {
#include "assets/dryfield_night_factory_collision_097FC_faces.inc"
};

static s16 _gDryfieldNightFactoryCollision097FCCells[8] = {
#include "assets/dryfield_night_factory_collision_097FC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightFactoryCollision097FCCells[i])
static s16* _gDryfieldNightFactoryCollision097FCTable[2] = {
#include "assets/dryfield_night_factory_collision_097FC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryLiftTurnedTemplate = { NULL, _gDryfieldNightFactoryCollision097FCNormals, _gDryfieldNightFactoryCollision097FCVerts, _gDryfieldNightFactoryCollision097FCFaces, _gDryfieldNightFactoryCollision097FCTable, 750, 1950, 1, 2, 4000, 4 };

TaskDesc gFactoryNightSpawnTable[8] = {
    { { { TASK_BODY_NONE, 192 } }, factoryPowerScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryLampScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryCapScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryWhiteoutScene, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, factoryLiftRun, { .model = &_gDryfieldNightFactoryDryfieldFactoryModel06604 } },
    { { { TASK_BODY_COORD, 192 } }, factoryBarrierCollision, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryHatchScene, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, factoryHatchRun, { .model = &_gDryfieldNightFactoryDryfieldFactoryModel093E4 } },
};

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gFactoryPanelSessionDesc[2] = {
    { { { TASK_BODY_NONE, 32 } }, factoryPanelSpawn, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry gFactoryMsgTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, factoryResolveWarp },
    { 5105, factoryIgnoreMessage },
    { ROOM_MESSAGE_COMMAND, factoryCommand },
    { ROOM_MESSAGE_SOUND, factorySoundCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, factoryRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gFactoryPromptDesc[1] = {
    { { { TASK_BODY_NONE, 192 } }, factoryPromptTask, { .value = 0 } },
};

TaskDesc gFactoryNightPanelDesc[1] = {
    { { { TASK_BODY_NONE, 192 } }, factoryPanelRun, { .value = 0 } },
};

TaskMessageEntry gFactoryPanelMsgTable[2] = {
    { FACTORY_PANEL_MESSAGE_MOVE_SETTLED, factoryPanelTrigger },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActionPromptHotspot gFactoryPanelHotspots[6] = {
    { -68, -63, 16, 16, 0, 1, 0 },
    { -27, -63, 16, 16, 1, 1, 0 },
    { 13, -63, 16, 16, 2, 1, 0 },
    { 46, -80, 34, 32, 3, 0, 0 },
    { -38, 0, 72, 48, 4, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

SVECTOR gFactoryGlowPos48 = { 395, -1630, 846, 0 };

SVECTOR gFactoryGlowPos4A1 = { 5910, -1308, 5649, 0 };

SVECTOR gFactoryGlowPos4A2 = { 5910, -1404, 5649, 0 };

u8* D_dryfield_night_factory_80186F1C[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

WorldCoordRoomLighting D_dryfield_night_factory_80186F24[2] = {
    { D_dryfield_night_factory_80189C88, D_dryfield_night_factory_8018A0C8 },
    { D_dryfield_night_factory_80189C88, D_dryfield_night_factory_8018A0C8 },
};

WorldCollisionRoomResources D_dryfield_night_factory_80186F34[2] = {
    { &gFactoryNightGrid, D_dryfield_night_factory_80189CA0, D_dryfield_night_factory_8018A168, NULL },
    { &gFactoryNightGrid, D_dryfield_night_factory_80189CA0, D_dryfield_night_factory_8018A168, NULL },
};

ViewCount D_dryfield_night_factory_80186F54[2] = { 19, 19 };

DirectionWarpEntry D_dryfield_night_factory_80186F58[3] = {
    { { { .word = 3072 }, 5178, 0, 1454 }, { 0, 0, 0, 0 }, { { .word = 768 }, 3952, 0, 1200 }, { 0, 0, 0, 0 }, 0x53170002, 0x53170001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 474 },
    { { { .word = 1024 }, 642, 0, 7493 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 1100, 0, 7060 }, { 0, 0, 0, 0 }, 0x53170004, 0x53170003, 0x53170005, 7, DIRECTION_WARP_FLAG_NONE, 475 },
    { { { .word = 3072 }, 5445, 1, 6866 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 1100, 0, 7060 }, { 0, 0, 0, 0 }, 0x53170014, 0x53170006, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_FADE_DEPARTURE, 473 },
};

static SVECTOR _gDryfieldNightFactoryCollision0A630Normals[28] = {
#include "assets/dryfield_night_factory_collision_0A630_normals.inc"
};

static SVECTOR _gDryfieldNightFactoryCollision0A630Verts[170] = {
#include "assets/dryfield_night_factory_collision_0A630_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightFactoryCollision0A630Faces[72] = {
#include "assets/dryfield_night_factory_collision_0A630_faces.inc"
};

static s16 _gDryfieldNightFactoryCollision0A630Cells[288] = {
#include "assets/dryfield_night_factory_collision_0A630_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightFactoryCollision0A630Cells[i])
static s16* _gDryfieldNightFactoryCollision0A630Table[8] = {
#include "assets/dryfield_night_factory_collision_0A630_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryNightGrid = { NULL, _gDryfieldNightFactoryCollision0A630Normals, _gDryfieldNightFactoryCollision0A630Verts, _gDryfieldNightFactoryCollision0A630Faces, _gDryfieldNightFactoryCollision0A630Table, 444, 222, 2, 4, 4000, 72 };

ViewCamera D_dryfield_night_factory_80187C14[19] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3057, 0x44F4, -6028 } }, 240 },
    { { { { 1147, 0, -3931 }, { -130, 4093, -37 }, { 3929, 135, 1147 } }, { -214, 1283, -1020 } }, 246 },
    { { { { 981, 0, 3976 }, { 62, 4095, -15 }, { -3976, 64, 981 } }, { -5724, 1309, -1020 } }, 246 },
    { { { { -3926, 0, -1165 }, { -458, 3765, 1545 }, { 1070, 1612, -3609 } }, { -847, 2531, -7624 } }, 257 },
    { { { { 0, 0, 4096 }, { 23, 4095, 0 }, { -4095, 23, 0 } }, { -2274, 1463, -666 } }, 680 },
    { { { { -4095, 0, 24 }, { 12, 3554, 2035 }, { -21, 2035, -3554 } }, { -4773, 2743, -9126 } }, 230 },
    { { { { -3993, 0, -911 }, { -270, 3911, 1183 }, { 870, 1214, -3813 } }, { -1048, 2819, -0x2BC8 } }, 246 },
    { { { { -1035, 0, 3962 }, { 929, 3981, 242 }, { -3852, 960, -1006 } }, { -5890, 2089, -0x284C } }, 240 },
    { { { { -489, 0, -4066 }, { -641, 4044, 77 }, { 4015, 646, -483 } }, { -337, 1837, -9758 } }, 240 },
    { { { { 619, 0, -4048 }, { -727, 4029, -111 }, { 3982, 736, 608 } }, { -4642, 1552, -5446 } }, 257 },
    { { { { 1741, 0, -3707 }, { 404, 4071, 190 }, { 3685, -446, 1731 } }, { -4747, 1512, -9880 } }, 282 },
    { { { { 0, 0, 4096 }, { 23, 4095, 0 }, { -4095, 23, 0 } }, { -2274, 1463, -666 } }, 680 },
    { { { { 1147, 0, -3931 }, { -130, 4093, -37 }, { 3929, 135, 1147 } }, { -214, 1283, -1020 } }, 246 },
    { { { { 981, 0, 3976 }, { 62, 4095, -15 }, { -3976, 64, 981 } }, { -5724, 1309, -1020 } }, 246 },
    { { { { -3926, 0, -1165 }, { -458, 3765, 1545 }, { 1070, 1612, -3609 } }, { -847, 2531, -7624 } }, 257 },
    { { { { -4095, 0, 24 }, { 12, 3554, 2035 }, { -21, 2035, -3554 } }, { -4773, 2743, -9126 } }, 230 },
    { { { { -3993, 0, -911 }, { -270, 3911, 1183 }, { 870, 1214, -3813 } }, { -1048, 2819, -0x2BC8 } }, 246 },
    { { { { -3738, 0, -1672 }, { -680, 3741, 1520 }, { 1528, 1665, -3415 } }, { -2135, 3083, -0x2DB6 } }, 263 },
    { { { { -3738, 0, -1672 }, { -680, 3741, 1520 }, { 1528, 1665, -3415 } }, { -2135, 3083, -0x2DB6 } }, 263 },
};

SpriteBatch D_dryfield_night_factory_80187EC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80187ED0[13] = {
    { 143, 0x3FC0, { .fields = { 48, 80 } }, 104, 40, 300, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, 40, 700, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 48 } }, -48, -32, 0x3847, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, 16, 1058, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 16, 1021, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, 32, 0x3318, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -8, 16, 0x2749, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -24, 8, 1150, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, 8, 1140, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -160, -120, 1037, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -136, -120, 1125, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 200 } }, -112, -120, 1162, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -96, -120, 1212, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80187FD4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 5, 0, 0, { 0, 0 } },
    { 7, 2, 0, 0, { 2, 0 } },
    { 9, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188004[12] = {
    { 143, 0x3FC0, { .fields = { 72, 240 } }, -160, -120, 50, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 88 } }, 72, 32, 0, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 200 } }, 120, -120, 2000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 200 } }, 64, -120, 2000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, 96, -120, 2000, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 56, 1150, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 40, 1157, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 24, 1155, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 8, 1153, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -8, 1147, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -24, 1151, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -40, 1150, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_801880F4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 1, 0, 0, { 2, 0 } },
    { 2, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_8018811C[82] = {
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 8, 1041, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 32, 1103, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 56, 0, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 8, 0, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 32, 978, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 48, 965, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -128, 8, 1130, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, 8, 1106, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -56, 8, 0, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -48, 24, 1031, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -48, 40, 937, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 136, 8 } }, -152, 64, 0, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -128, 24, 1075, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -128, 40, 1086, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -88, 24, 1063, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 40, 1004, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -72, 1487, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -48, 1475, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -24, 1488, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -128, -24, 1455, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -48, 1450, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -72, 1475, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -96, -72, 0, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -96, -40, 1455, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -24, 0, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -24, 1500, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -24, 1450, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -160, -80, 1025, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -104, -80, 954, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -48, -80, 908, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, 16, -80, 839, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 80, -80, 795, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 80, -64, 820, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 128, -32, 0, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 0, 0, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 80, -32, 883, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 0, 979, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 72, -16, 873, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -64, 0, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 16, 986, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 32, 0, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 0, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 64, 32, 985, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 48, 986, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 136 } }, 16, -64, 0, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -64, 1014, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, -64, 999, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -64, 1053, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -64, 0, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -40, 1092, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -104, -40, 1040, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -48, -40, 996, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -8, 1049, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, -104, -8, 1107, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -144, -16, 1150, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -16, 0, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 8, 0, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1040, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 8, 1077, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 0, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -160, 40, 0, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, 16, 1044, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, 16, 1064, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -40, 40, 1039, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 1078, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 24 } }, -160, 48, 0, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -40, 56, 0, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -40, 844, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 0, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 908, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 24, 1284, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 0, 1350, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -40, 0, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -40, 1347, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -8, 1255, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 8, 1250, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, -16, 1237, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, -40, 1230, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, 16, 1083, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 987, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -64, 884, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -16, -120, 792, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188784[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 16, 11, 0, 0, { 3, 0 } },
    { 27, 40, 0, 0, { 2, 0 } },
    { 67, 11, 0, 0, { 4, 0 } },
    { 78, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_801887BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_801887CC[32] = {
    { 142, 0x3FC0, { .fields = { 96, 32 } }, -160, 88, 425, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -64, 96, 425, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 128, 40 } }, 8, -24, 1416, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 112, -88, 1750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -48, -56, 1550, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, -88, 1432, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -16, -48, 1684, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 0, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -48, 1750, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -16, -64, 1704, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -120, 1143, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -96, 1197, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -72, 1244, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -48, 1327, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -24, 1301, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -24, 1386, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -48, 1294, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -72, 1229, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -96, 1178, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -120, 1123, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -120, 1123, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -96, 1243, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -72, 1192, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -48, 1302, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -24, 1368, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -56, 1351, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -88, 1216, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -120, 1083, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, -24, 1383, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 32 } }, -64, 88, 625, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 96, 56 } }, -160, 64, 500, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 24 } }, -64, 64, 0, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188A4C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 4, 0 } },
    { 3, 7, 0, 0, { 1, 0 } },
    { 10, 15, 0, 0, { 3, 0 } },
    { 25, 4, 0, 0, { 2, 0 } },
    { 29, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188A84[39] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 96, 500, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, -48, 2377, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, -32, 2425, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, -32, 2450, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -80, -16, 2375, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, -16, 2375, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -16, 2475, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 56, -32, 2250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 8, 1935, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 0, 1935, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 0, 1935, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, -8, 1935, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -64, -88, 1833, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -32, -88, 1791, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -8, -88, 1791, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 8, -88, 1740, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -64, -24, 1982, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -32, -24, 1911, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -24, 1911, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 8, -24, 1848, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -88, 1710, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 56, -88, 1675, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -56, 1675, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -56, 1675, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -16, 1780, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -16, 1780, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -88, 1638, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -40, 1719, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -8, 1792, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 40, 1200, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 56, 1200, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 72, 1200, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 88, 1200, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -160, 104, 1200, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 48, 1200, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 64, 1200, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 80, 1200, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 96, 1200, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -128, 64, 1200, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188D90[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 3, 0 } },
    { 1, 7, 0, 0, { 0, 0 } },
    { 8, 4, 0, 0, { 5, 0 } },
    { 12, 14, 0, 0, { 1, 0 } },
    { 26, 3, 0, 0, { 4, 0 } },
    { 29, 10, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188DD0[12] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -128, -120, 125, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, -72, 125, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -120, -64, 125, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -24, 125, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, -8, 125, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, 0, 125, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 24, 125, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, 56, 125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 88, 125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -144, 64, 125, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 80, 125, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -136, 96, 125, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188EC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188ED8[4] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, 16, 1062, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, 16, 1062, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 16, 1062, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 32, 16, 1062, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188F28[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188F90[78] = {
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 8, 0, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 8, 0, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 136, 8 } }, -152, 64, 0, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 56, 0, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 32, 1103, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -128, 40, 1086, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 40, 1004, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -48, 40, 937, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 48, 965, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -16, 32, 978, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -48, 24, 1031, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 24, 1063, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, 8, 1106, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -128, 24, 1075, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 8, 1041, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -128, 8, 1130, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -24, 0, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -96, -72, 0, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -24, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -96, -40, 1450, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -24, 1450, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -128, -24, 1450, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -24, 1488, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -48, 1475, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -48, 1450, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -72, 1475, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -72, 1475, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 136 } }, 16, -64, 0, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -64, 0, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 0, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 24 } }, -160, 48, 0, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -160, 40, 0, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 0, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 8, 0, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -16, 0, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 32, 0, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 0, 0, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 128, -32, 0, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 48, 986, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 64, 32, 985, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 16, 986, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 0, 979, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 72, -16, 873, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 80, -32, 883, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 80, -64, 884, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 80, -80, 795, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, 16, -80, 839, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -48, -80, 908, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -104, -80, 954, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -160, -80, 1025, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -64, 0, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -64, 1053, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, -64, 999, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -64, 1014, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -48, -40, 996, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -104, -40, 1040, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -40, 1092, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -144, -16, 1150, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1040, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 8, 1077, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, -8, 1107, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -8, 1049, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, 16, 1064, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, 16, 1044, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 1078, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -40, 40, 1039, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -40, 56, 0, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 24, 0, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 72, -40, 1250, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 72, -16, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, 8, 1250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -8, 1255, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -40, 1347, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 0, 1350, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -40, 0, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 1250, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -40, 1250, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 0, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_801895A8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 3, 0 } },
    { 16, 11, 0, 0, { 0, 0 } },
    { 27, 40, 0, 0, { 2, 0 } },
    { 67, 11, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_801895D8[32] = {
    { 141, 0x3FC0, { .fields = { 72, 24 } }, -72, 96, 757, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 32 } }, -160, 88, 667, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 112 } }, -160, -24, 0, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 120 } }, -72, -24, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 0, -24, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 128, 40 } }, 8, -24, 1416, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 104 } }, 8, 16, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 112, -88, 1750, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 24 } }, -16, -88, 0, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -48, -88, 1432, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -48, -56, 1550, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, -48, 1750, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -16, -64, 1704, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 0, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, -48, 1684, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, -64, 0, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 72 } }, 48, -88, 0, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -24, 1368, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -48, 1302, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -72, 1192, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -96, 1243, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -120, 1123, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -120, 1123, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -96, 1178, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -72, 1229, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 64, -48, 1294, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -24, 1386, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -24, 1301, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -48, 1311, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -72, 1244, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -96, 1197, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -120, 1143, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80189858[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 10, 0, 0, { 2, 0 } },
    { 17, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80189880[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80189890[17] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 40, 1300, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 48, 1750, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 64, 1750, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 8, 1325, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 8, 1325, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 16, 1312, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 16, 1320, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 24, 1322, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 1315, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 32, 1300, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 32, 1362, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 40, 1375, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 1312, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 40, 1275, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1282, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1287, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 216 } }, -112, -120, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_801899E4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 10, 0, 0, { 0, 0 } },
    { 13, 3, 0, 0, { 2, 0 } },
    { 16, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// State handlers of the room entry task: set-up, an empty tick and
/// `taskKill`.
static const TaskFuncTable3 _gFactoryEntryStates = {
    { factoryRoomInit, factoryEntryIdle, taskKill },
};

#include "../../shared/factory_room_init.inc.c"

#include "../../shared/factory_resolve_warp.inc.c"

#include "../../shared/factory_panel_spawn.inc.c"

#include "../../shared/factory_ignore_message.inc.c"

#include "../../shared/factory_command.inc.c"

#include "../../shared/factory_sound_command.inc.c"

#include "../../shared/factory_room_action.inc.c"

#include "../../shared/factory_entry_idle.inc.c"

#include "../../shared/factory_entry_task.inc.c"

#include "../../shared/factory_panel_idle.inc.c"

/// State handlers of the room's script task, run by
/// `factoryPanelRun`: set-up, prompt arming, the idle
/// hotspot scan, prompt spawning, the prompt state, the exit and the wait for
/// the message handler's trigger.
static const TaskFuncTable7 _gFactoryPanelStates = {
    {
        factoryPanelInit,
        factoryPanelArmPrompt,
        factoryPanelIdle,
        factoryPanelOpenPrompt,
        factoryPanelPrompt,
        factoryPanelExit,
        factoryPanelWaitMove,
    },
};

#include "../../shared/action_prompt_outline_rect.inc.c"

#include "../../shared/factory_panel_run_step.inc.c"

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

#include "../../shared/factory_show_view9_sprite.inc.c"

#include "../../shared/factory_panel_run.inc.c"

#include "../../shared/factory_prompt_task.inc.c"

#include "../../shared/factory_panel_trigger.inc.c"

#include "../../shared/action_prompt_hit_test.inc.c"

#include "../../shared/factory_panel_init.inc.c"

#include "../../shared/factory_panel_arm_prompt.inc.c"

#include "../../shared/factory_panel_open_prompt.inc.c"

#include "../../shared/factory_panel_prompt.inc.c"

#include "../../shared/factory_panel_exit.inc.c"

#include "../../shared/factory_panel_wait_move.inc.c"

#include "../../shared/factory_show_view11_sprite.inc.c"

#include "../../shared/action_prompt_reset.inc.c"

#include "../../shared/glow_draw_tinted_disc.inc.c"

#include "../../shared/factory_draw_glows.inc.c"
