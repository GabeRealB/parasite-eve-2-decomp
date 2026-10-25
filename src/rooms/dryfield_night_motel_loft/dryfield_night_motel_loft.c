#include "rooms/dryfield_night_motel_loft.h"

#include "common.h"

#include "dryfield_night_motel_loft_private.h"

#include "actors/actor_01600.h"
#include "actors/actor_400600.h"

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
#include "main/sound_types.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "../../shared/room/room_variants.h"

/// Unreferenced 48-byte object; its interpretation as actor transforms is unproven.
static ActorTransform D_dryfield_night_motel_loft_8017FB84[2] = {
    { { 0, 0, 0xFFFF, 0 }, { 0, 0, 0, 0 } },
    { { 0xFFFF, 0, 0, 0 }, { -1, 0, 0, 0 } },
};

/// Mapped view 12's background-only sprite list.
///
/// A zero-count selector precedes `SPRITE_BATCH_END`, retaining decoded image
/// strips. The same address occupies `sources.empty` and supplies no sprites.
/// Keep this distinct authored storage live while the room overlay is loaded.
static SpriteBatch _gDryfieldNightMotelLoftView12SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Mapped view 13's background-only sprite list.
///
/// A zero-count selector precedes `SPRITE_BATCH_END`, retaining decoded image
/// strips. The same address occupies `sources.empty` and supplies no sprites.
/// Keep this distinct authored storage live while the room overlay is loaded.
static SpriteBatch _gDryfieldNightMotelLoftView13SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Mapped view 14's background-only sprite list.
///
/// A zero-count selector precedes `SPRITE_BATCH_END`, retaining decoded image
/// strips. The same address occupies `sources.empty` and supplies no sprites.
/// Keep this distinct authored storage live while the room overlay is loaded.
static SpriteBatch _gDryfieldNightMotelLoftView14SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteView gDryfieldNightMotelLoftSpriteViews[14] = {
    { { .empty = gDryfieldNightMotelLoftView1SpriteBatches }, gDryfieldNightMotelLoftView1SpriteBatches, NULL },
    { { .empty = gDryfieldNightMotelLoftView2SpriteBatches }, gDryfieldNightMotelLoftView2SpriteBatches, NULL },
    { { .elements = gDryfieldNightMotelLoftView3SpriteSources }, gDryfieldNightMotelLoftView3SpriteBatches, NULL },
    { { .elements = gDryfieldNightMotelLoftView4SpriteSources }, gDryfieldNightMotelLoftView4SpriteBatches, NULL },
    { { .elements = gDryfieldNightMotelLoftView5SpriteSources }, gDryfieldNightMotelLoftView5SpriteBatches, NULL },
    { { .elements = gDryfieldNightMotelLoftView6SpriteSources }, gDryfieldNightMotelLoftView6SpriteBatches, NULL },
    { { .elements = gDryfieldNightMotelLoftView7SpriteSources }, gDryfieldNightMotelLoftView7SpriteBatches, NULL },
    { { .elements = gDryfieldNightMotelLoftView8SpriteSources }, gDryfieldNightMotelLoftView8SpriteBatches, NULL },
    { { .empty = gDryfieldNightMotelLoftView2SpriteBatches }, gDryfieldNightMotelLoftView2SpriteBatches, NULL },
    { { .elements = gDryfieldNightMotelLoftView3SpriteSources }, gDryfieldNightMotelLoftView3SpriteBatches, NULL },
    { { .elements = gDryfieldNightMotelLoftView7SpriteSources }, gDryfieldNightMotelLoftView7SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelLoftView12SpriteBatches }, _gDryfieldNightMotelLoftView12SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelLoftView13SpriteBatches }, _gDryfieldNightMotelLoftView13SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelLoftView14SpriteBatches }, _gDryfieldNightMotelLoftView14SpriteBatches, NULL },
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

WorldCoordRoomLights gDryfieldNightMotelLoftRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMotelLoftPointLights),
    .pointLights           = _gDryfieldNightMotelLoftPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

enum {
    DRYFIELD_NIGHT_MOTEL_LOFT_JERRY_CAN_TRIGGER             = 1,
    DRYFIELD_NIGHT_MOTEL_LOFT_JERRY_CAN_CAP_COMMAND         = 2,
    DRYFIELD_NIGHT_MOTEL_LOFT_TRIGGER_CAPTION_SCENE_COMMAND = 3,
    DRYFIELD_NIGHT_MOTEL_LOFT_BALCONY_WARP_SLOTS            = 0x14, // Departure slot 1, balcony arrival slot 4
};

WorldCollisionTrigger gDryfieldNightMotelLoftViewBoundaryTriggers[12] = {
    { .origin = { 3007, -1040, -417, 0 }, .vertices = { { -4, -1264, -2500, 0 }, { -4, 1264, -2500, 0 }, { -3, -1264, 2489, 0 }, { -3, 1264, 2489, 0 } }, .normal = { -4106, 0, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2792, .control = 0, .parameter0 = 3, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 2782, -1088, -354, 0 }, .vertices = { { -3, -1264, 2492, 0 }, { -3, 1264, 2492, 0 }, { -3, -1264, -2498, 0 }, { -3, 1264, -2498, 0 } }, .normal = { 4104, 0, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 2792, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 928, -1024, 1199, 0 }, .vertices = { { 687, -1264, 1236, 0 }, { 687, 1264, 1236, 0 }, { -687, -1264, -1235, 0 }, { -687, 1264, -1235, 0 } }, .normal = { 3587, 0, -1998, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 3, .parameter1 = 7, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { 991, -1056, 959, 0 }, .vertices = { { -685, -1264, -1237, 0 }, { -685, 1264, -1237, 0 }, { 686, -1264, 1237, 0 }, { 686, 1264, 1237, 0 } }, .normal = { -3582, 0, 1984, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 7, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { -1217, -1120, -1473, 0 }, .vertices = { { 37, -1264, -1420, 0 }, { 37, 1264, -1420, 0 }, { -59, -1264, 1406, 0 }, { -59, 1264, 1406, 0 } }, .normal = { -4094, 0, -141, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 4, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { -1506, -1088, -1665, 0 }, .vertices = { { 16, -1264, 1410, 0 }, { 16, 1264, 1410, 0 }, { -28, -1264, -1417, 0 }, { -28, 1264, -1417, 0 } }, .normal = { 4102, 0, -66, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 3, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { -4929, -1056, -1248, 0 }, .vertices = { { 275, -1264, 1373, 0 }, { 275, 1264, 1373, 0 }, { -311, -1264, -1393, 0 }, { -311, 1264, -1393, 0 } }, .normal = { 4015, 0, -852, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 4, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { -4577, -1056, -1312, 0 }, .vertices = { { -243, -1264, -1408, 0 }, { -243, 1264, -1408, 0 }, { 213, -1264, 1383, 0 }, { 213, 1264, 1383, 0 } }, .normal = { -4042, 0, 659, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 5, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { -5187, -1056, 703, 0 }, .vertices = { { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 }, { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 } }, .normal = { -1044, 0, -3962, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 5, .parameter1 = 6, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { -5282, -1024, 512, 0 }, .vertices = { { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 }, { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 } }, .normal = { 1044, 0, 3971, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 6, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { -1761, -1088, 1472, 0 }, .vertices = { { -17, -1264, -1414, 0 }, { -17, 1264, -1414, 0 }, { 17, -1264, 1414, 0 }, { 17, 1264, 1414, 0 } }, .normal = { -ONE, 0, 46, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 6, .parameter1 = 7, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin = { -2177, -1056, 1471, 0 }, .vertices = { { 15, -1264, 1415, 0 }, { 15, 1264, 1415, 0 }, { -14, -1264, -1414, 0 }, { -14, 1264, -1414, 0 } }, .normal = { 4095, 0, -43, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1894, .control = 0, .parameter0 = 7, .parameter1 = 6, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST },
};

WorldCollisionTrigger gDryfieldNightMotelLoftActionTriggers[14] = {
    { .origin = { 4320, -56, -2096, 0 }, .vertices = { { -1056, 0, -464, 0 }, { 1056, 0, -464, 0 }, { -1056, 0, 464, 0 }, { 1056, 0, 464, 0 } }, .normal = { 0, ONE, 0, 0 }, .facingNormal = { 0, 0, ONE, 0 }, .radius = 1152, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, .parameter1 = DRYFIELD_NIGHT_MOTEL_LOFT_BALCONY_WARP_SLOTS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -6240, -64, 0, 0 }, .vertices = { { 128, 0, -896, 0 }, { 1280, 0, -896, 0 }, { 128, 0, 768, 0 }, { 1280, 0, 768, 0 } }, .normal = { 0, 4104, 0, 0 }, .facingNormal = { -ONE, 0, 0, 0 }, .radius = 1562, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = DRYFIELD_NIGHT_MOTEL_LOFT_JERRY_CAN_CAP_COMMAND, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD },
    { .origin = { -4624, -64, -32, 0 }, .vertices = { { -528, 0, -464, 0 }, { 528, 0, -464, 0 }, { -528, 0, 464, 0 }, { 528, 0, 464, 0 } }, .normal = { 0, ONE, 0, 0 }, .facingNormal = { -ONE, 0, 0, 0 }, .radius = 701, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 1, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -5088, -64, -1664, 0 }, .vertices = { { -656, 0, -464, 0 }, { 656, 0, -464, 0 }, { -656, 0, 464, 0 }, { 656, 0, 464, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 199, 0, 4089, 0 }, .radius = 801, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = DRYFIELD_NIGHT_MOTEL_LOFT_TRIGGER_CAPTION_SCENE_COMMAND, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4736, -64, 1200, 0 }, .vertices = { { -496, 0, -608, 0 }, { 496, 0, -608, 0 }, { -496, 0, 608, 0 }, { 496, 0, 608, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { -4076, 0, 400, 0 }, .radius = 783, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 7, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4768, -64, -352, 0 }, .vertices = { { -496, 0, -832, 0 }, { 496, 0, -832, 0 }, { -496, 0, 832, 0 }, { 496, 0, 832, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { -ONE, 0, -1, 0 }, .radius = 968, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 8, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4736, -64, -1856, 0 }, .vertices = { { -496, 0, -608, 0 }, { 496, 0, -608, 0 }, { -496, 0, 608, 0 }, { 496, 0, 608, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { -4076, 0, -403, 0 }, .radius = 783, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 9, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -496, -64, 784, 0 }, .vertices = { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { -2, 0, 4095, 0 }, .radius = 1492, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 13, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -3488, -64, 800, 0 }, .vertices = { { -1424, 0, -400, 0 }, { 1424, 0, -400, 0 }, { -1424, 0, 400, 0 }, { 1424, 0, 400, 0 } }, .normal = { 0, 4108, 0, 0 }, .facingNormal = { -2, 0, 4095, 0 }, .radius = 1476, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 12, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -512, -64, -864, 0 }, .vertices = { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 1, 0, -ONE, 0 }, .radius = 1492, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 10, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -3456, -64, -832, 0 }, .vertices = { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 1, 0, -ONE, 0 }, .radius = 1492, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 11, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -1392, -64, 1632, 0 }, .vertices = { { -944, 0, -400, 0 }, { 944, 0, -400, 0 }, { -944, 0, 400, 0 }, { 944, 0, 400, 0 } }, .normal = { 0, 4099, 0, 0 }, .facingNormal = { 1, 0, -ONE, 0 }, .radius = 1024, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 16, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -4416, -64, 1632, 0 }, .vertices = { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 1, 0, -ONE, 0 }, .radius = 1492, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 15, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { -2672, -64, -1664, 0 }, .vertices = { { -1712, 0, -400, 0 }, { 1712, 0, -400, 0 }, { -1712, 0, 400, 0 }, { 1712, 0, 400, 0 } }, .normal = { 0, 4113, 0, 0 }, .facingNormal = { -1, 0, ONE, 0 }, .radius = 1755, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 14, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST },
};

// Placement entry keys and decimal file numbers are separate loader domains.
enum {
    DRYFIELD_NIGHT_MOTEL_LOFT_RESOURCE_ZEBRA_STALKER    = 6,
    DRYFIELD_NIGHT_MOTEL_LOFT_RESOURCE_SCAVENGER        = 16,
    DRYFIELD_NIGHT_MOTEL_LOFT_ZEBRA_STALKER_FILE_NUMBER = 6,
    DRYFIELD_NIGHT_MOTEL_LOFT_SCAVENGER_FILE_NUMBER     = 16,
};

/// Variant 1's Zebra Stalker recipe, followed by the resource-list terminator.
///
/// Entry 6 loads file 400600 and borrows its one task descriptor at index zero.
/// The map's three placements share this recipe; storage lives with the room.
static AreaResource _gDryfieldNightMotelLoftVariant1Resources[] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_LOFT_RESOURCE_ZEBRA_STALKER, .fileNumber = DRYFIELD_NIGHT_MOTEL_LOFT_ZEBRA_STALKER_FILE_NUMBER, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's Zebra Stalker and Scavenger recipes, terminated by entry 255.
///
/// Entry keys 6/16 select files 400600/301600 and their single task descriptors.
/// One Stalker and four map-owned Scavenger placements borrow these room recipes.
static AreaResource _gDryfieldNightMotelLoftVariant3Resources[] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_LOFT_RESOURCE_ZEBRA_STALKER, .fileNumber = DRYFIELD_NIGHT_MOTEL_LOFT_ZEBRA_STALKER_FILE_NUMBER, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = DRYFIELD_NIGHT_MOTEL_LOFT_RESOURCE_SCAVENGER, .fileNumber = DRYFIELD_NIGHT_MOTEL_LOFT_SCAVENGER_FILE_NUMBER, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = &gActor301600ScavengerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 4's file-101600 Scavenger recipe and resource-list terminator.
///
/// Entry 16 serves seven map-owned posed placements, borrowing descriptor zero.
/// The room owns this list while the actor overlay owns the descriptor and code.
static AreaResource _gDryfieldNightMotelLoftVariant4Resources[] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_LOFT_RESOURCE_SCAVENGER, .fileNumber = DRYFIELD_NIGHT_MOTEL_LOFT_SCAVENGER_FILE_NUMBER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's file-101600 Scavenger recipe and resource-list terminator.
///
/// Entry 16 serves six map-owned placements, borrowing descriptor zero.
/// This distinct authored list has the same recipe as variant 4.
static AreaResource _gDryfieldNightMotelLoftVariant11Resources[] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_LOFT_RESOURCE_SCAVENGER, .fileNumber = DRYFIELD_NIGHT_MOTEL_LOFT_SCAVENGER_FILE_NUMBER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightMotelLoftAreaVariants[13] = {
    { NULL, NULL },
    { gMapDryfieldFullMotelLoftVariant1Placements, _gDryfieldNightMotelLoftVariant1Resources },
    { NULL, NULL },
    { gMapDryfieldFullMotelLoftVariant3Placements, _gDryfieldNightMotelLoftVariant3Resources },
    { gMapDryfieldFullMotelLoftVariant4Placements, _gDryfieldNightMotelLoftVariant4Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { gMapDryfieldFullMotelLoftVariant11Placements, _gDryfieldNightMotelLoftVariant11Resources },
    { NULL, NULL },
};

/// Cue-2 sound requests in the loaded type-1 bank; cue 1 selects the next entry.
enum {
    DRYFIELD_NIGHT_MOTEL_LOFT_SURFACE_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_MOTEL_LOFT_SURFACE_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37
};

/// Footstep cue bases for surface class 1.
///
/// Walking and scripted jumps share one pair; running uses the other.
/// Cue 1 adds one to each base, and companion playback adds 100.
/// Requests borrow the loaded type-1 bank while the room is active.
static WorldCollisionFootstepSounds _gDryfieldNightMotelLoftSurface1FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_MOTEL_LOFT_SURFACE_1_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_MOTEL_LOFT_SURFACE_1_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_LOFT_SURFACE_1_WALK_CUE_BASE,
};

/// Collision policy for surface classes 0 and 2..7: probes block, impacts are enabled and pushback applies.
///
/// No footstep cues; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelLoftDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for surface class 1: probes block, impacts are enabled and pushback applies.
///
/// Borrows room-owned walking, running and scripted-jump cue bases while loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelLoftSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelLoftSurface1FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightMotelLoftSurfacePropertiesByClass[8] = {
    &_gDryfieldNightMotelLoftDefaultSurfaceProperties,
    &_gDryfieldNightMotelLoftSurface1Properties,
    &_gDryfieldNightMotelLoftDefaultSurfaceProperties,
    &_gDryfieldNightMotelLoftDefaultSurfaceProperties,
    &_gDryfieldNightMotelLoftDefaultSurfaceProperties,
    &_gDryfieldNightMotelLoftDefaultSurfaceProperties,
    &_gDryfieldNightMotelLoftDefaultSurfaceProperties,
    &_gDryfieldNightMotelLoftDefaultSurfaceProperties,
};

/// Restores placement zero's Stalker at its floor start when the encounter was seen.
///
/// Initialization writes command 1 before synchronous dispatch. The Stalker
/// ignores the zero context; this room-owned message is not retained by it.
static ActorCommand _gDryfieldNightMotelLoftRestoreStalkerCommand = { 0 };

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

/// Names the balcony destination resolver used by the loft's room-message table.
///
/// `dryfield_night_motel_loft_private.h` declares its prototype and external
/// linkage for the second translation unit. Bind one function identifier for
/// the following fragment, which undefines the binding after its definition.
#define ROOM_VARIANT_MOTEL_BALCONY_RESOLVE_HANDLER roomVariantMotelBalconyMsg
#include "../../shared/room/room_variants_motel_balcony.inc.c"

s32 dryfieldNightMotelLoftCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_LOFT_COMMAND_CAPTION_SCENE = 3,
    };

    if (commandId == DRYFIELD_NIGHT_MOTEL_LOFT_COMMAND_CAPTION_SCENE) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(&gDryfieldNightMotelLoftCaptionSceneTaskDesc, 0, 0, 0);
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
            gCapChoiceRowStride = DRYFIELD_NIGHT_MOTEL_LOFT_CHOICE_ROW_STEP;
            task->state         = task->state + 1;
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
    gGameSession->location.roomState                            = roomId;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = roomId;
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

    task->msgTable = gDryfieldNightMotelLoftMessageHandlers;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (sceneFindPlacedActor(0) != 0 && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN) != 0) {
        _gDryfieldNightMotelLoftRestoreStalkerCommand.command = DRYFIELD_NIGHT_MOTEL_LOFT_RESTORE_STALKER_COMMAND;
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &_gDryfieldNightMotelLoftRestoreStalkerCommand, 0);
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
        WorldCollisionTrigger* pickupTrigger = &gDryfieldNightMotelLoftActionTriggers[DRYFIELD_NIGHT_MOTEL_LOFT_JERRY_CAN_TRIGGER];
        pickupTrigger->flags                &= ~WORLD_COLLISION_TRIGGER_ENABLED;
    }
    // Latch before launching the script, which changes rooms and commands the Stalker.
    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_JERRY_CAN) && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN) == 0 && sceneFindPlacedActor(0)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_LOFT_EVENT_SEEN, 1);
        evsStartScript(gDryfieldNightMotelLoftStalkerEncounterScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_OBJECTIVE);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_MUSIC_KEY;
    }
}

/// Room-controller handlers indexed by Task.state (0 initialize, 1 update, 2 kill).
///
/// The controller copies all three callbacks before dispatch; the index must
/// stay in 0..2. Keep the table and callback code loaded for the task's lifetime.
static const TaskFuncTable3 _gDryfieldNightMotelLoftRoomStates = {
    { _dryfieldNightMotelLoftInitializeRoom, _dryfieldNightMotelLoftUpdateRoom, taskKill },
};

void dryfieldNightMotelLoftRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldNightMotelLoftRoomStates;
    stateHandlers.funcs[task->state](task);
}
