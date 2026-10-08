#include "rooms/dryfield_back_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/back_street.h"

/// The room's message table, installed on the room entry task.
extern TaskMessageEntry D_dryfield_back_street_8017F964[];
extern TaskDesc         D_dryfield_back_street_8017F98C[];

/// Volume last asked of the back street's ambience, or 0 when none is playing.
/// Written by `func_dryfield_back_street_8017D5D0` and cleared by state 0 of the
/// same task.
extern s32 D_dryfield_back_street_80181054;

void       func_dryfield_back_street_8017D5D0(Task*);
static s32 _dryfieldBackStreetRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32 _dryfieldBackStreetIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 executionMode);
static s32 _dryfieldBackStreetIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg);

enum { DRYFIELD_BACK_STREET_MESSAGE_USE_KEY_ITEM = 0x13F1 };

extern WorldCollisionGrid    D_dryfield_back_street_80180284[1];
extern WorldCollisionTrigger D_dryfield_back_street_801804C0[6];
extern WorldCollisionTrigger D_dryfield_back_street_80180688[11];
extern WorldCoordRoomLights  D_dryfield_back_street_80180FF8[1];

extern TaskDesc Actor00100_D1BA84;

TaskMessageEntry D_dryfield_back_street_8017F964[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, backStreetEventMsg },
    { DRYFIELD_BACK_STREET_MESSAGE_USE_KEY_ITEM, _dryfieldBackStreetRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldBackStreetIgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldBackStreetIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_back_street_8017F98C[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_back_street_8017D5D0, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_dryfield_back_street_8017F9B4[1] = {
    { D_dryfield_back_street_80180284, D_dryfield_back_street_801804C0, D_dryfield_back_street_80180688, NULL },
};

u8* D_dryfield_back_street_8017F9C4[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_back_street_8017F9C8[1] = { 5 };

WorldCoordRoomLighting D_dryfield_back_street_8017F9CC[1] = {
    { D_dryfield_back_street_80180FF8, NULL },
};

DirectionWarpEntry D_dryfield_back_street_8017F9D4[4] = {
    { { { .word = 1024 }, -9597, 0, 4667 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -8696, 0, 5479 }, { 0, 0, 0, 0 }, 0x52050002, 0x52050001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 470 },
    { { { .word = 2048 }, -7539, 0, 5536 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -8028, 0, 5175 }, { 0, 0, 0, 0 }, 0x52050004, 0x52050003, 0x52050005, 2, DIRECTION_WARP_FLAG_NONE, 469 },
    { { { .word = 2048 }, -1423, 0, 5449 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -2346, 0, 5223 }, { 0, 0, 0, 0 }, 0x52050002, 0x52050001, 0x52050005, 3, DIRECTION_WARP_FLAG_NONE, 468 },
    { { { .word = 2048 }, 9463, 2, 5532 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 9800, 2, 4668 }, { 0, 0, 0, 0 }, 0x52050002, 0x52050001, 0x52050005, 5, DIRECTION_WARP_FLAG_NONE, 467 },
};

static SVECTOR _gDryfieldBackStreetCollision02CC4Normals[23] = {
#include "assets/dryfield_back_street_collision_02CC4_normals.inc"
};

static SVECTOR _gDryfieldBackStreetCollision02CC4Verts[88] = {
#include "assets/dryfield_back_street_collision_02CC4_verts.inc"
};

static WorldCollisionGridFace _gDryfieldBackStreetCollision02CC4Faces[39] = {
#include "assets/dryfield_back_street_collision_02CC4_faces.inc"
};

static s16 _gDryfieldBackStreetCollision02CC4Cells[266] = {
#include "assets/dryfield_back_street_collision_02CC4_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldBackStreetCollision02CC4Cells[i])
static s16* _gDryfieldBackStreetCollision02CC4Table[28] = {
#include "assets/dryfield_back_street_collision_02CC4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_back_street_80180284[1] = {
    { NULL, _gDryfieldBackStreetCollision02CC4Normals, _gDryfieldBackStreetCollision02CC4Verts, _gDryfieldBackStreetCollision02CC4Faces, _gDryfieldBackStreetCollision02CC4Table, 0x2AFE, 100, 7, 4, 4000, 39 },
};

ViewCamera D_dryfield_back_street_801802A8[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -850, 0x7148, -4850 } }, 329 },
    { { { { 696, 0, 4036 }, { -1070, 3949, 184 }, { -3892, -1085, 671 } }, { 4016, 385, -4128 } }, 230 },
    { { { { 681, 0, 4038 }, { 402, 4075, -67 }, { -4018, 408, 677 } }, { -2416, 1408, -4201 } }, 257 },
    { { { { 615, 0, -4049 }, { -873, 3999, -132 }, { 3954, 883, 600 } }, { 2448, 1600, -4230 } }, 230 },
    { { { { 173, 0, -4092 }, { -1464, 3824, -62 }, { 3821, 1465, 162 } }, { -2046, 2454, -4167 } }, 257 },
};

SpriteBatch D_dryfield_back_street_8018035C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_back_street_8018036C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_back_street_8018037C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_back_street_8018038C[5] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 1295, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -72, 1258, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 152 } }, 80, -72, 1125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, 96, -88, 1022, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 216 } }, 120, -96, 785, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_back_street_801803F0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_back_street_80180408[5] = {
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -8, 1663, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -16, 1680, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -16, 1714, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -24, 1678, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -16, 1669, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_back_street_8018046C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_back_street_80180484[5] = {
    { { .empty = D_dryfield_back_street_8018035C }, D_dryfield_back_street_8018035C, NULL },
    { { .empty = D_dryfield_back_street_8018036C }, D_dryfield_back_street_8018036C, NULL },
    { { .empty = D_dryfield_back_street_8018037C }, D_dryfield_back_street_8018037C, NULL },
    { { .elements = D_dryfield_back_street_8018038C }, D_dryfield_back_street_801803F0, NULL },
    { { .elements = D_dryfield_back_street_80180408 }, D_dryfield_back_street_8018046C, NULL },
};

WorldCollisionTrigger D_dryfield_back_street_801804C0[6] = {
    { NULL, NULL, NULL, { -6272, -3296, 4752, 0 }, { { 0, -4320, -1584, 0 }, { 0, -4320, 1584, 0 }, { 0, 4320, -1584, 0 }, { 0, 4320, 1584, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5888, -3504, 4672, 0 }, { { 0, -4528, 1584, 0 }, { 0, -4528, -1584, 0 }, { 0, 4528, 1584, 0 }, { 0, 4528, -1584, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -192, -4160, 4576, 0 }, { { -236, -5184, 1562, 0 }, { 228, -5184, -1572, 0 }, { -236, 5184, 1562, 0 }, { 228, 5184, -1572, 0 } }, { -4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 5418, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -320, -4128, 4608, 0 }, { { 229, -5152, -1571, 0 }, { -235, -5152, 1563, 0 }, { 229, 5152, -1571, 0 }, { -235, 5152, 1563, 0 } }, { 4053, 0, 600, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4256, -4096, 4704, 0 }, { { 0, -5120, -1584, 0 }, { 0, -5120, 1584, 0 }, { 0, 5120, -1584, 0 }, { 0, 5120, 1584, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5345, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4416, -4352, 4736, 0 }, { { 0, -5376, 1584, 0 }, { 0, -5376, -1584, 0 }, { 0, 5376, 1584, 0 }, { 0, 5376, -1584, 0 } }, { -4126, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_back_street_80180688[11] = {
    { NULL, NULL, NULL, { -9648, -55, 4496, 0 }, { { -400, 0, -560, 0 }, { 400, 0, -560, 0 }, { -400, 0, 560, 0 }, { 400, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7872, -55, 5776, 0 }, { { 832, 0, -352, 0 }, { 832, 0, 352, 0 }, { -832, 0, -352, 0 }, { -832, 0, 352, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 902, WORLD_COLLISION_TRIGGER_ACTION_WARP, 6, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1616, -48, 5744, 0 }, { { 688, 0, -352, 0 }, { 688, 0, 352, 0 }, { -688, 0, -352, 0 }, { -688, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 770, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9424, -48, 5712, 0 }, { { 784, 0, -384, 0 }, { 784, 0, 384, 0 }, { -784, 0, -384, 0 }, { -784, 0, 384, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2CA0, -64, 5024, 0 }, { { -1072, 0, -416, 0 }, { 1072, 0, -416, 0 }, { -1072, 0, 416, 0 }, { 1072, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6113, -64, 5569, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8288, -64, 2144, 0 }, { { -1696, 0, -208, 0 }, { 1760, 0, -208, 0 }, { -800, 0, 1008, 0 }, { 736, 0, 1008, 0 } }, { 0, 4104, 0, 0 }, { 201, 0, -4092, 0 }, 1768, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7456, -64, 5696, 0 }, { { -1248, 0, -368, 0 }, { 1248, 0, -368, 0 }, { -1248, 0, 368, 0 }, { 1248, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 200, 0, -4093, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4576, -64, 5536, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 5664, 0 }, { { 1056, 0, -384, 0 }, { 1056, 0, 384, 0 }, { -1056, 0, -384, 0 }, { -1056, 0, 384, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2790, -64, 5520, 0 }, { { -224, 0, -560, 0 }, { 448, 0, -560, 0 }, { -448, 0, 560, 0 }, { 224, 0, 560, 0 } }, { 0, 4107, 0, 0 }, { -4076, 0, 401, 0 }, 715, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_back_street_801809CC[2] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_back_street_801809E4[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 56, 56, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205600_801602C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_back_street_80180A08[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_back_street_80180A20[3] = {
    { 15, 0, 0, 6900, -2000, 6050, 0, 0, 0, 2, 0 },
    { 15, 0, 1, 8000, -2800, 3600, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_dryfield_back_street_80180A50[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AEA4, D_dryfield_back_street_801809CC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017AED4, D_dryfield_back_street_801809E4 },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_back_street_80180A20, D_dryfield_back_street_80180A08 },
    { NULL, NULL },
    { NULL, NULL },
};

/// Authored white point lights for model shading in every daytime Back Street view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits (`ONE` is full strength). The room overlay owns this
/// writable storage: coordinate updates set its view parent and cached
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// expire when the room overlay unloads.
static WorldCoordPointLight _gDryfieldBackStreetPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2985, 5488 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2990, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3000, -2985, 5062 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6020, -2985, 5065 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9000, -2985, 5488 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2980, -2985, 5488 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5990, -3500, 5452 } },
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
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2941, -3500, 5065 } },
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
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6000, -2985, 5955 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 5000,
    },
};

WorldCoordRoomLights D_dryfield_back_street_80180FF8[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldBackStreetPointLights), _gDryfieldBackStreetPointLights, 0, NULL },
};

WorldCollisionFootstepSounds D_dryfield_back_street_80181010 = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionSurfaceProperties D_dryfield_back_street_8018101C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_back_street_80181024[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_back_street_80181010 },
};

WorldCollisionSurfaceProperties D_dryfield_back_street_8018102C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_back_street_80181010 },
};

WorldCollisionSurfaceProperties* D_dryfield_back_street_80181034[8] = {
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_80181024,
    D_dryfield_back_street_8018102C,
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_8018101C,
};

s32 D_dryfield_back_street_80181054 = 0;

static void func_dryfield_back_street_8017D8B4(Task* task);

/// Back street ambience: state 0 clears the recorded volume and advances, state
/// 1 maps the current camera view to a target volume and stereo pan - 0x1E/+4,
/// 0x32/-8 and 0x64/-0xC for views 3/4/5, 0 and centre elsewhere - and, whenever
/// the volume differs from the recorded one, enqueues the matching fade event:
/// type 6 to start the track, type 7 to stop it, type A to retune it, then
/// records the new volume.
void func_dryfield_back_street_8017D5D0(Task* task)
{
    s32 vol;
    s32 pan;

    switch (task->state) {
        case 0:
            D_dryfield_back_street_80181054 = 0;
            task->state                     = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    switch (viewGetMappedIndex()) {
        case 3:
            vol = 0x1E;
            pan = 4;
            break;
        case 4:
            vol = 0x32;
            pan = -8;
            break;
        case 5:
            vol = 0x64;
            pan = -0xC;
            break;
        default:
            pan = 0;
            vol = 0;
            break;
    }

    if (vol == D_dryfield_back_street_80181054) {
        return;
    }
    if (D_dryfield_back_street_80181054 == 0) {
        sndEvtRequestScriptStart(SOUND_BACK_STREET_AMBIENCE, pan, (s8)(((0x64 - vol) * 0x7F) / 100));
    } else if (vol == 0) {
        sndEvtRequestScriptStop(SOUND_BACK_STREET_AMBIENCE, 0x1E);
    } else {
        sndEvtRequestScriptMix(SOUND_BACK_STREET_AMBIENCE, pan, (s8)(((0x64 - vol) * 0x7F) / 100));
    }
    D_dryfield_back_street_80181054 = vol;
}

#include "../../shared/back_street_event_msg.inc.c"

/// Refuses every key-item use in Back Street, returning the menu's unusable result.
///
/// The item ID and zero second payload are ignored; no item is consumed.
static s32 _dryfieldBackStreetRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    enum { DRYFIELD_BACK_STREET_KEY_ITEM_UNUSABLE = 0 };

    return DRYFIELD_BACK_STREET_KEY_ITEM_UNUSABLE;
}

/// Ignores room commands and their execution modes, returning zero.
static s32 _dryfieldBackStreetIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 executionMode)
{
    return 0;
}

/// Ignores room-action requests and returns zero.
///
/// The request is borrowed for synchronous dispatch; no payload is read or retained.
static s32 _dryfieldBackStreetIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    return 0;
}

/// The room entry task's first state: installs the room's message table, hands
/// the task to pointer slot 7, spawns the tasks of
/// `D_dryfield_back_street_8017F98C` (the ambience task) and moves on to the
/// next state.
static void func_dryfield_back_street_8017D8B4(Task* task)
{
    task->msgTable = D_dryfield_back_street_8017F964;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_dryfield_back_street_8017F98C, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

/// Keeps the initialized room task available for messages until its state changes.
static void _dryfieldBackStreetIdleState(Task* task)
{
}

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_dryfield_back_street_8017D5C4 = {
    { func_dryfield_back_street_8017D8B4, _dryfieldBackStreetIdleState, taskKill },
};

void dryfieldBackStreetRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_dryfield_back_street_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

void dryfieldBackStreetConfigureEffectsTask(Task* task)
{
    enum { DRYFIELD_BACK_STREET_EFFECTS_INITIALIZE,
           DRYFIELD_BACK_STREET_EFFECTS_ACTIVE };

    if (task->state == DRYFIELD_BACK_STREET_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_DRYFIELD_BACK_STREET_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_DRYFIELD_BACK_STREET_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_DRYFIELD_BACK_STREET_SPARK_BURST;
        task->state             = DRYFIELD_BACK_STREET_EFFECTS_ACTIVE;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void dryfieldBackStreetRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void dryfieldBackStreetRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_dryfield_back_street_8017ED1C(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
