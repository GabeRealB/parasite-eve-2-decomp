#include "rooms/dryfield_night_motel_loft.h"

#include "common.h"

#include "dryfield_night_motel_loft_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "../../shared/room_variants.h"

extern WorldCollisionTrigger D_dryfield_night_motel_loft_801803F4[14];

/// The room's 0x7DB payload buffer.
extern ActorCommand D_dryfield_night_motel_loft_8018092C;

extern AreaResource D_dryfield_night_motel_loft_8018081C[2];
extern AreaResource D_dryfield_night_motel_loft_80180834[3];
extern AreaResource D_dryfield_night_motel_loft_80180858[2];
extern AreaResource D_dryfield_night_motel_loft_80180870[2];

ActorTransform D_dryfield_night_motel_loft_8017FB84[2] = {
    { { 0, 0, 0xFFFF, 0 }, { 0, 0, 0, 0 } },
    { { 0xFFFF, 0, 0, 0 }, { -1, 0, 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_loft_8017FBB4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_loft_8017FBC4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_loft_8017FBD4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_motel_loft_8017FBE4[14] = {
    { { .empty = D_dryfield_night_motel_loft_8017F2D0 }, D_dryfield_night_motel_loft_8017F2D0, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017F2E0 }, D_dryfield_night_motel_loft_8017F2E0, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F2F0 }, D_dryfield_night_motel_loft_8017F390, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F3A8 }, D_dryfield_night_motel_loft_8017F4AC, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F4C4 }, D_dryfield_night_motel_loft_8017F654, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F66C }, D_dryfield_night_motel_loft_8017F770, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F788 }, D_dryfield_night_motel_loft_8017F97C, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F9AC }, D_dryfield_night_motel_loft_8017FB64, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017F2E0 }, D_dryfield_night_motel_loft_8017F2E0, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F2F0 }, D_dryfield_night_motel_loft_8017F390, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F788 }, D_dryfield_night_motel_loft_8017F97C, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017FBB4 }, D_dryfield_night_motel_loft_8017FBB4, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017FBC4 }, D_dryfield_night_motel_loft_8017FBC4, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017FBD4 }, D_dryfield_night_motel_loft_8017FBD4, NULL },
};

/// Point lights shared by both nighttime motel loft rooms in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. The loaded overlay owns the mutable array: coordinate
/// updates parent and compose its transforms, and lighting queries overwrite
/// attenuation. Borrowed pointers must not outlive the overlay.
static WorldCoordPointLight _gDryfieldNightMotelLoftPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4605, -1890, 1498 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2867, 2703, 2293 },
        },
        .inner = 2081,
        .outer = 4561,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4638, -1878, 1027 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2867, 2703, 2293 },
        },
        .inner = 1441,
        .outer = 2901,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2362, -1878, 5 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1638, 2048, 2867 },
        },
        .inner = 1000,
        .outer = 2000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4144, -2651, 5 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1638, 2048, 2867 },
        },
        .inner = 1000,
        .outer = 2000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8, -1878, -1245 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2867, 2703, 2293 },
        },
        .inner = 1841,
        .outer = 4096,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4603, -1878, -1241 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2867, 2703, 2293 },
        },
        .inner = 1381,
        .outer = 3701,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -14, -1878, 1500 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2867, 2703, 2293 },
        },
        .inner = 1562,
        .outer = 3501,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5195, -1878, -1241 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2867, 2703, 2293 },
        },
        .inner = 2284,
        .outer = 5083,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 50000, -25000, -10000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 0, 163, 819 },
        },
        .inner = 300000,
        .outer = 400000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7, -1878, 5 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1000, 1000, 1000 },
        },
        .inner = 4096,
        .outer = 7772,
    },
};

WorldCoordRoomLights D_dryfield_night_motel_loft_8018004C[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldNightMotelLoftPointLights), _gDryfieldNightMotelLoftPointLights, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_motel_loft_80180064[12] = {
    { NULL, NULL, NULL, { 3007, -1040, -417, 0 }, { { -4, -1264, -2500, 0 }, { -4, 1264, -2500, 0 }, { -3, -1264, 2489, 0 }, { -3, 1264, 2489, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2782, -1088, -354, 0 }, { { -3, -1264, 2492, 0 }, { -3, 1264, 2492, 0 }, { -3, -1264, -2498, 0 }, { -3, 1264, -2498, 0 } }, { 4104, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 928, -1024, 1199, 0 }, { { 687, -1264, 1236, 0 }, { 687, 1264, 1236, 0 }, { -687, -1264, -1235, 0 }, { -687, 1264, -1235, 0 } }, { 3587, 0, -1998, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 3, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 991, -1056, 959, 0 }, { { -685, -1264, -1237, 0 }, { -685, 1264, -1237, 0 }, { 686, -1264, 1237, 0 }, { 686, 1264, 1237, 0 } }, { -3582, 0, 1984, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 7, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1217, -1120, -1473, 0 }, { { 37, -1264, -1420, 0 }, { 37, 1264, -1420, 0 }, { -59, -1264, 1406, 0 }, { -59, 1264, 1406, 0 } }, { -4094, 0, -141, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1506, -1088, -1665, 0 }, { { 16, -1264, 1410, 0 }, { 16, 1264, 1410, 0 }, { -28, -1264, -1417, 0 }, { -28, 1264, -1417, 0 } }, { 4102, 0, -66, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4929, -1056, -1248, 0 }, { { 275, -1264, 1373, 0 }, { 275, 1264, 1373, 0 }, { -311, -1264, -1393, 0 }, { -311, 1264, -1393, 0 } }, { 4015, 0, -852, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4577, -1056, -1312, 0 }, { { -243, -1264, -1408, 0 }, { -243, 1264, -1408, 0 }, { 213, -1264, 1383, 0 }, { 213, 1264, 1383, 0 } }, { -4042, 0, 659, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5187, -1056, 703, 0 }, { { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 }, { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 } }, { -1044, 0, -3962, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5282, -1024, 512, 0 }, { { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 }, { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 } }, { 1044, 0, 3971, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1761, -1088, 1472, 0 }, { { -17, -1264, -1414, 0 }, { -17, 1264, -1414, 0 }, { 17, -1264, 1414, 0 }, { 17, 1264, 1414, 0 } }, { -4096, 0, 46, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2177, -1056, 1471, 0 }, { { 15, -1264, 1415, 0 }, { 15, 1264, 1415, 0 }, { -14, -1264, -1414, 0 }, { -14, 1264, -1414, 0 } }, { 4095, 0, -43, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_motel_loft_801803F4[14] = {
    { NULL, NULL, NULL, { 4320, -56, -2096, 0 }, { { -1056, 0, -464, 0 }, { 1056, 0, -464, 0 }, { -1056, 0, 464, 0 }, { 1056, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6240, -64, 0, 0 }, { { 128, 0, -896, 0 }, { 1280, 0, -896, 0 }, { 128, 0, 768, 0 }, { 1280, 0, 768, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1562, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4624, -64, -32, 0 }, { { -528, 0, -464, 0 }, { 528, 0, -464, 0 }, { -528, 0, 464, 0 }, { 528, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5088, -64, -1664, 0 }, { { -656, 0, -464, 0 }, { 656, 0, -464, 0 }, { -656, 0, 464, 0 }, { 656, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 199, 0, 4089, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4736, -64, 1200, 0 }, { { -496, 0, -608, 0 }, { 496, 0, -608, 0 }, { -496, 0, 608, 0 }, { 496, 0, 608, 0 } }, { 0, 4105, 0, 0 }, { -4076, 0, 400, 0 }, 783, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4768, -64, -352, 0 }, { { -496, 0, -832, 0 }, { 496, 0, -832, 0 }, { -496, 0, 832, 0 }, { 496, 0, 832, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, -1, 0 }, 968, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4736, -64, -1856, 0 }, { { -496, 0, -608, 0 }, { 496, 0, -608, 0 }, { -496, 0, 608, 0 }, { 496, 0, 608, 0 } }, { 0, 4105, 0, 0 }, { -4076, 0, -403, 0 }, 783, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -496, -64, 784, 0 }, { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { -2, 0, 4095, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3488, -64, 800, 0 }, { { -1424, 0, -400, 0 }, { 1424, 0, -400, 0 }, { -1424, 0, 400, 0 }, { 1424, 0, 400, 0 } }, { 0, 4108, 0, 0 }, { -2, 0, 4095, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -512, -64, -864, 0 }, { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 1, 0, -4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3456, -64, -832, 0 }, { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 1, 0, -4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1392, -64, 1632, 0 }, { { -944, 0, -400, 0 }, { 944, 0, -400, 0 }, { -944, 0, 400, 0 }, { 944, 0, 400, 0 } }, { 0, 4099, 0, 0 }, { 1, 0, -4096, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4416, -64, 1632, 0 }, { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 1, 0, -4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2672, -64, -1664, 0 }, { { -1712, 0, -400, 0 }, { 1712, 0, -400, 0 }, { -1712, 0, 400, 0 }, { 1712, 0, 400, 0 } }, { 0, 4113, 0, 0 }, { -1, 0, 4096, 0 }, 1755, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_night_motel_loft_8018081C[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_motel_loft_80180834[3] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400600_80151B10 },
    { 16, 16, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_301600_801745DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_motel_loft_80180858[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_motel_loft_80180870[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_motel_loft_80180888[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017CB58, D_dryfield_night_motel_loft_8018081C },
    { NULL, NULL },
    { D_map_dryfield_full_8017CB98, D_dryfield_night_motel_loft_80180834 },
    { D_map_dryfield_full_8017CBF8, D_dryfield_night_motel_loft_80180858 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017CC78, D_dryfield_night_motel_loft_80180870 },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_dryfield_night_motel_loft_801808F0 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_loft_801808FC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_loft_80180904[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_motel_loft_801808F0 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_motel_loft_8018090C[8] = {
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_80180904,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
};

ActorCommand D_dryfield_night_motel_loft_8018092C = { 0 };

static void _dryfieldNightMotelLoftInitializeRoom(Task* task);
static void _dryfieldNightMotelLoftUpdateRoom(Task* task);

// Saved object slot shared by the Jerry Can placement and its collision barrier.
enum {
    DRYFIELD_NIGHT_MOTEL_LOFT_JERRY_CAN_OBJECT = 0xA,
    DRYFIELD_NIGHT_MOTEL_LOFT_OBJECT_REMOVED   = 2
};

s32 dryfieldNightMotelLoftRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#define ROOM_VARIANT_MOTEL_BALCONY_MSG roomVariantMotelBalconyMsg
#include "../../shared/room_variants_motel_balcony.inc.c"

s32 dryfieldNightMotelLoftCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_LOFT_COMMAND_CAPTION_SCENE = 3,
    };

    if (commandId == DRYFIELD_NIGHT_MOTEL_LOFT_COMMAND_CAPTION_SCENE) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(D_dryfield_night_motel_loft_8017EB4C, 0, 0, 0);
    }
    return 0;
}

s32 dryfieldNightMotelLoftIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

s32 dryfieldNightMotelLoftPlaySoundCue(Task* task, s32 messageId, s32 cueKey, s32 unusedSecondArg)
{
    enum { DRYFIELD_NIGHT_MOTEL_LOFT_SOUND_CUE = 5 };

    if (cueKey == DRYFIELD_NIGHT_MOTEL_LOFT_SOUND_CUE) {
        sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, DRYFIELD_NIGHT_MOTEL_LOFT_SOUND_CUE), 0, 0);
    }
    return 0;
}

void dryfieldNightMotelLoftCaptionSceneTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_LOFT_SCENE_START         = 0,
        DRYFIELD_NIGHT_MOTEL_LOFT_SCENE_WAIT_CAP      = 1,
        DRYFIELD_NIGHT_MOTEL_LOFT_SCENE_RESTORE       = 2,
        DRYFIELD_NIGHT_MOTEL_LOFT_CAP_FIRST_SCENE     = 3,
        DRYFIELD_NIGHT_MOTEL_LOFT_CAP_REPEAT_SCENE    = 18,
        DRYFIELD_NIGHT_MOTEL_LOFT_SCENE_COMPLETED_KEY = 31,
        DRYFIELD_NIGHT_MOTEL_LOFT_CHOICE_ROW_STEP     = 5,
    };

    switch (task->state) {
        case DRYFIELD_NIGHT_MOTEL_LOFT_SCENE_START:
            // Keep actor updates paused while the caption scene owns player control.
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_SCENE_DONE) != 0 ? DRYFIELD_NIGHT_MOTEL_LOFT_CAP_REPEAT_SCENE : DRYFIELD_NIGHT_MOTEL_LOFT_CAP_FIRST_SCENE, CAP_PLAYBACK_IN_PLACE);
            D_80115680  = DRYFIELD_NIGHT_MOTEL_LOFT_CHOICE_ROW_STEP;
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_MOTEL_LOFT_SCENE_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
                return;
            }
            return;
        case DRYFIELD_NIGHT_MOTEL_LOFT_SCENE_RESTORE:
            // Only the completion choice makes later visits use the repeat scene.
            if (capGetVariantKey() == DRYFIELD_NIGHT_MOTEL_LOFT_SCENE_COMPLETED_KEY) {
                gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_SCENE_DONE, 1);
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            break;
    }
}

void dryfieldNightMotelLoftSetCurrentRoom(u8 roomId)
{
    gGameSession->location.loc.room                            = roomId;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = roomId;
}

/// Publishes the room controller and restores its encounter and pickup collision.
///
/// State 0 of `dryfieldNightMotelLoftRoomTask`. Requires the loaded room's
/// tables and live session/save state. If the encounter was already triggered,
/// command 1 restores placement 0's Stalker to its running floor state.
/// Advances to state 1 after rebuilding the Jerry Can barrier.
static void _dryfieldNightMotelLoftInitializeRoom(Task* task)
{
    enum { DRYFIELD_NIGHT_MOTEL_LOFT_RESTORE_STALKER_COMMAND = 1 };

    task->msgTable = D_dryfield_night_motel_loft_8017EB1C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (sceneFindPlacedActor(0) != 0 && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN) != 0) {
        D_dryfield_night_motel_loft_8018092C.command = DRYFIELD_NIGHT_MOTEL_LOFT_RESTORE_STALKER_COMMAND;
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_motel_loft_8018092C, 0);
    }
    dryfieldNightMotelLoftRebuildJerryCanCollision(areaGetCurrentObjectState(DRYFIELD_NIGHT_MOTEL_LOFT_JERRY_CAN_OBJECT) == DRYFIELD_NIGHT_MOTEL_LOFT_OBJECT_REMOVED);
    task->state = task->state + 1;
}

/// Keeps the Jerry Can barrier current and starts the Stalker encounter once.
///
/// State 1 of `dryfieldNightMotelLoftRoomTask`. Collection removes the barrier
/// and disables the pickup interaction. The event starts only while placement
/// 0 is live, and its saved latch prevents another start on later updates.
static void _dryfieldNightMotelLoftUpdateRoom(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_OBJECTIVE = 0x15,
        DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_MUSIC_KEY = 3
    };

    dryfieldNightMotelLoftRebuildJerryCanCollision(areaGetCurrentObjectState(DRYFIELD_NIGHT_MOTEL_LOFT_JERRY_CAN_OBJECT) == DRYFIELD_NIGHT_MOTEL_LOFT_OBJECT_REMOVED);
    if (areaGetCurrentObjectState(DRYFIELD_NIGHT_MOTEL_LOFT_JERRY_CAN_OBJECT) == DRYFIELD_NIGHT_MOTEL_LOFT_OBJECT_REMOVED) {
        WorldCollisionTrigger* pickupTrigger = &D_dryfield_night_motel_loft_801803F4[1];
        pickupTrigger->flags                &= ~WORLD_COLLISION_TRIGGER_ENABLED;
    }
    // Latch before launching the script, which changes rooms and commands the Stalker.
    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_JERRY_CAN) && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN) == 0 && sceneFindPlacedActor(0)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN, 1);
        evsStartScript(D_dryfield_night_motel_loft_8017EB78, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_OBJECTIVE);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_MUSIC_KEY;
    }
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_motel_loft_8017D5C4 = {
    { _dryfieldNightMotelLoftInitializeRoom, _dryfieldNightMotelLoftUpdateRoom, taskKill },
};

void dryfieldNightMotelLoftRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_night_motel_loft_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
