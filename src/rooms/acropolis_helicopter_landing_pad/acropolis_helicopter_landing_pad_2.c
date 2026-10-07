#include "rooms/acropolis_helicopter_landing_pad.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "acropolis_helicopter_landing_pad_private.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
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
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

/// Main-executable byte with no module header yet; `+ 1` seeds the slot-3
/// msg 0x3E8 record's `field_0` in `func_acropolis_helicopter_landing_pad_8017DA9C`.

/// Turn-to-heading task state: current unwrapped yaw and signed step.
extern s32   D_acropolis_helicopter_landing_pad_80187F74;
extern s32   D_acropolis_helicopter_landing_pad_80187F78;
extern s16   D_acropolis_helicopter_landing_pad_80187F7C;
extern Task* D_acropolis_helicopter_landing_pad_80187F80;

/// Three `Gp_SpawnScript18` argument pairs used by the state timeline in
/// `func_acropolis_helicopter_landing_pad_8017DE78`, one pair per phase.
extern PadScriptCmd              D_acropolis_helicopter_landing_pad_80187D40[2];
extern PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D48[2];
extern PadScriptCmd              D_acropolis_helicopter_landing_pad_80187D50[4];
extern PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D60[2];
extern PadScriptCmd              D_acropolis_helicopter_landing_pad_80187D68[4];
extern PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D78[2];

static void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1);
static void func_acropolis_helicopter_landing_pad_8017EA6C(Task* task);

/// A debug format string nothing in the room reads.
static const char D_acropolis_helicopter_landing_pad_8017D5D0[] = "%s (%5d,%5d,%5d)";

/// State handlers of the room's script task
/// `func_acropolis_helicopter_landing_pad_8017EB00`, indexed by
/// `Task::state`: set-up, the per-frame phase tick and `taskKill`.
static const TaskFuncTable3 D_acropolis_helicopter_landing_pad_8017D5E4 = {
    { func_acropolis_helicopter_landing_pad_8017EA6C, func_acropolis_helicopter_landing_pad_8017D9BC, taskKill },
};

extern SpriteDrawArea D_acropolis_helicopter_landing_pad_80186C8C[2];
extern SpriteDrawArea D_acropolis_helicopter_landing_pad_80186DD4[2];
extern SpriteDrawArea D_acropolis_helicopter_landing_pad_80186EC8[2];
extern SpriteDrawArea D_acropolis_helicopter_landing_pad_80187148[2];
extern SpriteDrawArea D_acropolis_helicopter_landing_pad_8018723C[2];
extern SpriteDrawArea D_acropolis_helicopter_landing_pad_80187358[2];
extern SpriteDrawArea D_acropolis_helicopter_landing_pad_801875B0[2];
extern SpriteDrawArea D_acropolis_helicopter_landing_pad_80187630[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80186B00[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80186C64[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80186C7C[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80186D04[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80186DBC[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80186EB0[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187120[5];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187224[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187340[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187588[5];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801875C4[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801875D4[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801875E4[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801875F4[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187618[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187644[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187690[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801876F8[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187710[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187720[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801877A8[3];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801877C0[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801877D0[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801877E0[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_801877F0[2];
extern SpriteBatch    D_acropolis_helicopter_landing_pad_80187800[2];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80186B10[17];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80186CA0[5];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80186D1C[8];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80186DE8[10];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80186EDC[29];
extern SpriteSource   D_acropolis_helicopter_landing_pad_8018715C[10];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80187250[12];
extern SpriteSource   D_acropolis_helicopter_landing_pad_8018736C[27];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80187604[1];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80187654[3];
extern SpriteSource   D_acropolis_helicopter_landing_pad_801876A8[4];
extern SpriteSource   D_acropolis_helicopter_landing_pad_80187730[6];

WorldCollisionTrigger D_acropolis_helicopter_landing_pad_80185E7C[9] = {
    { NULL, NULL, NULL, { -6528, -32, -6208, 0 }, { { -384, 0, -192, 0 }, { 384, 0, -192, 0 }, { -384, 0, 192, 0 }, { 384, 0, 192, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 429, WORLD_COLLISION_TRIGGER_ACTION_CALLBACK | 0x100, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5274, -131, -1840, 0 }, { { -800, 0, -688, 0 }, { 800, 0, -688, 0 }, { -800, 0, 688, 0 }, { 800, 0, 688, 0 } }, { 0, 4107, 0, 0 }, { 201, 0, -4091, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1055, -128, -6657, 0 }, { { 73, 0, -1674, 0 }, { 1007, 0, -1340, 0 }, { -1006, 0, 1339, 0 }, { -73, 0, 1673, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1673, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -6650, 472, -5301, 0 }, { { -496, 0, -240, 0 }, { 496, 0, -240, 0 }, { -496, 0, 240, 0 }, { 496, 0, 240, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 550, WORLD_COLLISION_TRIGGER_ACTION_FACING | WORLD_COLLISION_TRIGGER_AUTOMATIC, 51, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6160, -128, -1824, 0 }, { { -560, 0, -848, 0 }, { 560, 0, -848, 0 }, { -560, 0, 848, 0 }, { 560, 0, 848, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4091, 0 }, 1015, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 5648, -32, 1184, 0 }, { { -224, 0, -1344, 0 }, { 1440, 0, -1344, 0 }, { -224, 0, 1344, 0 }, { 1440, 0, 1344, 0 } }, { 0, 4117, 0, 0 }, { 4091, 0, 201, 0 }, 1966, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -800, -64, 5568, 0 }, { { 1760, 0, -864, 0 }, { 1760, 0, 1760, 0 }, { -1760, 0, -864, 0 }, { -1760, 0, 1760, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, 4095, 0 }, 2482, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5536, -64, 544, 0 }, { { 864, 0, 1472, 0 }, { -1984, 0, 1472, 0 }, { 864, 0, -1472, 0 }, { -1984, 0, -1472, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, -201, 0 }, 2468, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6144, -64, -1824, 0 }, { { -560, 0, -848, 0 }, { 560, 0, -848, 0 }, { -560, 0, 848, 0 }, { 560, 0, 848, 0 } }, { 0, 4098, 0, 0 }, { -4052, 0, -601, 0 }, 1015, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_acropolis_helicopter_landing_pad_80186128[2] = {
    { NULL, NULL, { -256, -944, -240, 0 }, { { -5632, -1968, -5616, 0 }, { 5632, -1968, 5616, 0 }, { -5632, 1968, -5616, 0 }, { 5632, 1968, 5616, 0 } }, { 2897, 0, -2906, 0 }, 8192, 1, 0 },
    { NULL, NULL, { 0, -896, 0, 0 }, { { 5616, -1920, -5632, 0 }, { -5616, -1920, 5632, 0 }, { 5616, 1920, -5632, 0 }, { -5616, 1920, 5632, 0 } }, { 2900, 0, 2892, 0 }, 8175, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_acropolis_helicopter_landing_pad_801861A0[2] = {
    { 27, 109, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, D_actor_510900_80167A18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_helicopter_landing_pad_801861B8[4] = {
    { 27, 110, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, D_actor_511000_80155070 },
    { 144, 110, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, D_actor_511000_801472E8 },
    { 254, 110, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, D_actor_511000_80139924 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_helicopter_landing_pad_801861E8[13] = {
    { NULL, NULL },
    { D_map_akropolis_8017BD2C, D_acropolis_helicopter_landing_pad_801861A0 },
    { D_map_akropolis_8017BD4C, D_acropolis_helicopter_landing_pad_801861B8 },
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

/// The landing pad's directional model light, contributing in every room view.
///
/// The local translation is a direction vector, normalized when shading;
/// RGB intensities have 12 fractional bits. The loaded room owns this array.
/// Coordinate updates attach the view parent and refresh the composed matrix;
/// shading overwrites attenuation, so the records must remain writable.
static WorldCoordLight _gAcropolisHelicopterLandingPadDirectionalLights[] = {
    {
        .transform = {
            .lighting = {
                .composeStamp = GRAPHICS_COORD_DIRTY,
                .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 10, -10, -10 } },
                .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                .unknown_46   = { 0, 0, 0, 0 },
                .attenuation  = 0,
                .parent       = NULL,
            },
        },
        .color      = { .r = 1556, .g = 1638, .b = 1802 },
        .unknown_56 = { 0, 0 },
    },
};

/// The landing pad's 22 point lights for model shading and spatial light queries.
///
/// Every entry accepts every room view. Positions and falloff radii use
/// integer world units; RGB intensities have 12 fractional bits (`ONE` is 1.0).
/// The loaded room overlay owns these writable records: coordinate updates
/// attach the view parent and compose transforms, and queries overwrite
/// attenuation. Borrowed pointers must not survive unloading the overlay.
static WorldCoordPointLight _gAcropolisHelicopterLandingPadPointLights[] = {
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3900, -4205, -1800 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 2048, .g = 2129, .b = 2211 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, -4205, -6000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 2048, .g = 2129, .b = 2211 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -5000, -1000, -6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 2000, -1000, 6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -2000, -1000, 6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -5000, -1000, 6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6100, -1000, 5000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6100, -1000, 2000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 6100, -1010, 500 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = ONE, .g = 3686, .b = 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -1300, -1010, 6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = ONE, .g = 3686, .b = 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6100, -1010, -100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = ONE, .g = 3686, .b = 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -5800, -1610, -1400 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3686, .g = 3686, .b = 3686 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 800,
        .outer = 1300,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -2200, -3770, -6800 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 1556, .g = 1556, .b = 1638 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -2000, -1000, -6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 2000, -1000, -6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5000, -1000, -6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 6100, -1000, -5000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 6100, -1000, -2000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 6100, -1000, 2000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 6100, -1000, 5000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5000, -1000, 6100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 3112, .g = 3194, .b = 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -1200, -4205, 0 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, 0, 0 } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .unknown_46   = { 0, 0, 0, 0 },
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color      = { .r = 2048, .g = 2129, .b = 2211 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 2000,
    },
};

WorldCoordRoomLights D_acropolis_helicopter_landing_pad_80186AE8[1] = {
    { ARRAY_SIZE(_gAcropolisHelicopterLandingPadDirectionalLights), _gAcropolisHelicopterLandingPadDirectionalLights, ARRAY_SIZE(_gAcropolisHelicopterLandingPadPointLights), _gAcropolisHelicopterLandingPadPointLights, 0, NULL },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80186B00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_helicopter_landing_pad_80186B10[17] = {
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -104, -16, 721, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -96, 0, 658, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -88, 24, 605, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -80, 40, 568, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -80, 56, 544, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -72, 64, 528, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -72, 72, 513, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -64, 80, 499, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -64, 88, 486, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -56, 96, 474, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -56, 104, 457, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 64 } }, 104, -120, 550, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 56 } }, 80, -56, 562, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 24 } }, 64, 0, 594, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 120, 48 } }, 40, 24, 750, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 24 } }, 64, 72, 750, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 24 } }, 88, 96, 604, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80186C64[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80186C7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_80186C8C[2] = {
    { { 4, 4, 133, 235 }, 1500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_acropolis_helicopter_landing_pad_80186CA0[5] = {
    { 142, 0x3FC0, { .fields = { 16, 112 } }, 0, -104, 2125, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 144 } }, -16, -112, 1750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 208 } }, -48, -120, 1500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 240 } }, -88, -120, 1125, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 240 } }, -160, -120, 500, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80186D04[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_helicopter_landing_pad_80186D1C[8] = {
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -160, 0, 500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 80 } }, -104, 16, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 96 } }, -56, 24, 500, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -8, 32, 500, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 104 } }, 40, 16, 500, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 64, 0, 500, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 88, -16, 500, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, -32, 500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80186DBC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_80186DD4[2] = {
    { { 267, 0, 51, 239 }, 3625 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_acropolis_helicopter_landing_pad_80186DE8[10] = {
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, 56, 500, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -128, 48, 550, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -96, 40, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -56, 32, 625, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -24, 24, 625, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 8, 16, 650, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 40, 8, 675, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 80, 0, 700, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 112, -8, 750, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, -16, 875, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80186EB0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_80186EC8[2] = {
    { { 109, 118, 0, 0 }, 1750 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_acropolis_helicopter_landing_pad_80186EDC[29] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -24, 500, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -144, -16, 420, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -128, -8, 417, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -112, 0, 400, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -88, 8, 375, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, -56, 24, 375, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 8, 16, 375, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, 32, 8, 400, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 64, 0, 412, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, 96, -8, 425, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, 128, -16, 437, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -16, 450, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -72, 3000, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -144, -80, 2500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -80, 2250, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -160, -88, 2875, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -112, -88, 2875, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -64, -88, 2875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -16, -88, 2875, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, 32, -88, 2875, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, 80, -88, 2875, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, 128, -88, 2875, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 48, 48 } }, -160, -40, 2875, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -112, -40, 2875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -64, -40, 2875, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 48, 48 } }, -16, -40, 2875, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, 32, -40, 2875, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 48, 48 } }, 80, -40, 2875, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 32, 48 } }, 128, -40, 2875, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187120[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 2, 0 } },
    { 12, 3, 0, 0, { 1, 0 } },
    { 15, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_80187148[2] = {
    { { 1, 0, 46, 239 }, 2750 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_acropolis_helicopter_landing_pad_8018715C[10] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 0, 900, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 8, 900, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -104, 8, 900, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -72, 8, 875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -40, 16, 875, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -8, 16, 875, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 16, 850, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 56, 24, 825, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 88, 24, 800, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 120, 24, 775, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187224[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_8018723C[2] = {
    { { 53, 23, 0, 0 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_acropolis_helicopter_landing_pad_80187250[12] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -104, -64, 3000, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -88, -72, 2500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -128, -32, 700, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -104, 0, 625, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -120, 64, 500, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -88, 16, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -64, 48, 500, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -16, 40, 500, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, 24, 32, 500, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 72, 24, 50, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 112, 16, 500, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, 16, 500, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187340[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_80187358[2] = {
    { { 1, 0, 63, 239 }, 3500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_acropolis_helicopter_landing_pad_8018736C[27] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -64, 550, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -40, 550, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, -16, 525, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 8, 500, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, 24, 500, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -64, 48, 450, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 80, 425, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, 96, 400, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 16, 88, 375, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 40, 80, 375, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, 72, 375, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 80, 64, 400, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, 56, 400, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 120, 48, 425, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -64, -120, 2130, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -48, -120, 1812, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, -32, -120, 1687, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 136 } }, -16, -120, 1450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, 64, 250, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 32 } }, 96, 32, 250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -24, 250, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, 128, -120, 325, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, 96, 80, 250, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 48, 875, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 40, 64, 800, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 80, 750, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, 96, 625, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187588[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 9, 0, 0, { 2, 0 } },
    { 23, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_801875B0[2] = {
    { { 3, 0, 99, 239 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801875C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801875D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801875E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801875F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_helicopter_landing_pad_80187604[1] = {
    { 143, 0x3FC0, { .fields = { 16, 136 } }, -16, -16, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187618[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_80187630[2] = {
    { { 2, 1, 149, 238 }, 1125 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187644[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_helicopter_landing_pad_80187654[3] = {
    { 143, 0x3FC0, { .fields = { 104, 136 } }, -160, -24, 400, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 120, 56 } }, -56, 56, 375, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 56 } }, 64, 64, 390, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187690[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_helicopter_landing_pad_801876A8[4] = {
    { 143, 0x3FC0, { .fields = { 120, 40 } }, -160, 40, 675, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 168, 40 } }, -160, 80, 450, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 152, 32 } }, 8, 88, 450, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 120, 40 } }, 40, 48, 675, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801876F8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187710[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187720[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_helicopter_landing_pad_80187730[6] = {
    { 143, 0x3FC0, { .fields = { 72, 32 } }, -160, 88, 250, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -88, 80, 250, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -32, 72, 250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 16, 64, 250, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, 72, 56, 250, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 128, 48, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801877A8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801877C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801877D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801877E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_801877F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_helicopter_landing_pad_80187800[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_80187810[2] = {
    { { 2, 1, 149, 238 }, 1125 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteView D_acropolis_helicopter_landing_pad_80187824[27] = {
    { { .empty = D_acropolis_helicopter_landing_pad_80186B00 }, D_acropolis_helicopter_landing_pad_80186B00, NULL },
    { { .elements = D_acropolis_helicopter_landing_pad_80186B10 }, D_acropolis_helicopter_landing_pad_80186C64, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_80186C7C }, D_acropolis_helicopter_landing_pad_80186C7C, D_acropolis_helicopter_landing_pad_80186C8C },
    { { .elements = D_acropolis_helicopter_landing_pad_80186CA0 }, D_acropolis_helicopter_landing_pad_80186D04, NULL },
    { { .elements = D_acropolis_helicopter_landing_pad_80186D1C }, D_acropolis_helicopter_landing_pad_80186DBC, D_acropolis_helicopter_landing_pad_80186DD4 },
    { { .elements = D_acropolis_helicopter_landing_pad_80186DE8 }, D_acropolis_helicopter_landing_pad_80186EB0, D_acropolis_helicopter_landing_pad_80186EC8 },
    { { .elements = D_acropolis_helicopter_landing_pad_80186EDC }, D_acropolis_helicopter_landing_pad_80187120, D_acropolis_helicopter_landing_pad_80187148 },
    { { .elements = D_acropolis_helicopter_landing_pad_8018715C }, D_acropolis_helicopter_landing_pad_80187224, D_acropolis_helicopter_landing_pad_8018723C },
    { { .elements = D_acropolis_helicopter_landing_pad_80187250 }, D_acropolis_helicopter_landing_pad_80187340, D_acropolis_helicopter_landing_pad_80187358 },
    { { .elements = D_acropolis_helicopter_landing_pad_8018736C }, D_acropolis_helicopter_landing_pad_80187588, D_acropolis_helicopter_landing_pad_801875B0 },
    { { .empty = D_acropolis_helicopter_landing_pad_801875C4 }, D_acropolis_helicopter_landing_pad_801875C4, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_801875D4 }, D_acropolis_helicopter_landing_pad_801875D4, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_801875E4 }, D_acropolis_helicopter_landing_pad_801875E4, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_801875F4 }, D_acropolis_helicopter_landing_pad_801875F4, NULL },
    { { .elements = D_acropolis_helicopter_landing_pad_80187604 }, D_acropolis_helicopter_landing_pad_80187618, D_acropolis_helicopter_landing_pad_80187630 },
    { { .empty = D_acropolis_helicopter_landing_pad_80187644 }, D_acropolis_helicopter_landing_pad_80187644, NULL },
    { { .elements = D_acropolis_helicopter_landing_pad_80187654 }, D_acropolis_helicopter_landing_pad_80187690, NULL },
    { { .elements = D_acropolis_helicopter_landing_pad_801876A8 }, D_acropolis_helicopter_landing_pad_801876F8, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_80187710 }, D_acropolis_helicopter_landing_pad_80187710, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_80187720 }, D_acropolis_helicopter_landing_pad_80187720, NULL },
    { { .elements = D_acropolis_helicopter_landing_pad_80187730 }, D_acropolis_helicopter_landing_pad_801877A8, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_801877C0 }, D_acropolis_helicopter_landing_pad_801877C0, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_801877D0 }, D_acropolis_helicopter_landing_pad_801877D0, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_801877E0 }, D_acropolis_helicopter_landing_pad_801877E0, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_801877F0 }, D_acropolis_helicopter_landing_pad_801877F0, NULL },
    { { .empty = D_acropolis_helicopter_landing_pad_80187800 }, D_acropolis_helicopter_landing_pad_80187800, NULL },
    { { .elements = D_acropolis_helicopter_landing_pad_80187604 }, D_acropolis_helicopter_landing_pad_80187618, D_acropolis_helicopter_landing_pad_80187630 },
};

ViewCamera D_acropolis_helicopter_landing_pad_80187968[27] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1500, 0x7530, 2500 } }, 432 },
    { { { { 2020, 0, 3563 }, { 2282, 3145, -1294 }, { -2735, 2624, 1551 } }, { 4724, 2583, 7606 } }, 257 },
    { { { { 1448, 0, 3831 }, { 929, 3973, -351 }, { -3716, 994, 1405 } }, { 111, 1700, 7565 } }, 257 },
    { { { { 683, 0, -4038 }, { -993, 3970, -168 }, { 3914, 1007, 662 } }, { 3525, 1700, 7067 } }, 257 },
    { { { { 3492, 0, 2140 }, { 389, 4027, -636 }, { -2104, 745, 3434 } }, { -8514, 1700, 9737 } }, 257 },
    { { { { 2030, 0, 3557 }, { 1121, 3887, -640 }, { -3375, 1291, 1927 } }, { -0x2868, 2310, 2157 } }, 257 },
    { { { { -3041, 0, 2743 }, { 488, 4030, 541 }, { -2699, 729, -2993 } }, { -8208, 1268, -8598 } }, 230 },
    { { { { -3740, 0, -1668 }, { -184, 4070, 413 }, { 1658, 452, -3717 } }, { 3618, 1700, -0x2D82 } }, 230 },
    { { { { -1924, 0, -3615 }, { -627, 4033, 333 }, { 3560, 710, -1895 } }, { 9705, 1727, -8007 } }, 257 },
    { { { { 3730, 0, -1690 }, { -618, 3812, -1364 }, { 1573, 1497, 3472 } }, { 7825, 2067, 3862 } }, 230 },
    { { { { 1608, 0, 3767 }, { 2218, 3309, -947 }, { -3044, 2412, 1299 } }, { -3535, 9746, 3169 } }, 230 },
    { { { { 1216, 0, -3911 }, { -2605, 3055, -810 }, { 2917, 2728, 907 } }, { 5463, 0x28FA, 2359 } }, 230 },
    { { { { 1538, 0, -3795 }, { -2650, 2931, -1074 }, { 2717, 2860, 1101 } }, { 3937, 6355, 1314 } }, 257 },
    { { { { 3507, 0, -2116 }, { -1885, 1858, -3125 }, { 960, 3650, 1591 } }, { -5011, 8020, -3917 } }, 230 },
    { { { { 836, 0, 4009 }, { -1269, 3885, 264 }, { -3803, -1297, 793 } }, { 292, 673, 6887 } }, 207 },
    { { { { 1453, 0, -3829 }, { -771, 4012, -292 }, { 3750, 824, 1423 } }, { -1784, 1471, 7126 } }, 207 },
    { { { { 1155, 0, 3929 }, { -1737, 3673, 510 }, { -3524, -1811, 1035 } }, { 1062, 2444, 7359 } }, 207 },
    { { { { -564, 0, 4056 }, { 503, 4064, 70 }, { -4025, 508, -559 } }, { 2521, 4108, 1483 } }, 230 },
    { { { { 2575, 0, -3185 }, { -930, 3917, -752 }, { 3046, 1196, 2462 } }, { 3838, 4675, 2020 } }, 188 },
    { { { { 2586, 0, -3176 }, { -638, 4012, -520 }, { 3111, 824, 2533 } }, { 2361, 4678, 1361 } }, 188 },
    { { { { 3554, 0, 2035 }, { -1127, 3411, 1967 }, { -1695, -2267, 2959 } }, { -870, 2992, 6656 } }, 207 },
    { { { { 3950, 0, 1082 }, { 504, 3625, -1839 }, { -958, 1906, 3496 } }, { -270, 4973, 6269 } }, 243 },
    { { { { -4092, 0, 162 }, { 123, 2664, 3108 }, { -105, 3110, -2662 } }, { 839, 3802, 9054 } }, 230 },
    { { { { 2856, 0, -2935 }, { 2818, 1144, 2742 }, { 820, -3932, 798 } }, { 2576, 5537, 2556 } }, 230 },
    { { { { 522, 0, -4062 }, { -2349, 3341, -302 }, { 3314, 2368, 426 } }, { 8857, 2945, 2306 } }, 230 },
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1000, 0x7530, -2000 } }, 432 },
    { { { { 836, 0, 4009 }, { -1269, 3885, 264 }, { -3803, -1297, 793 } }, { 292, 673, 6887 } }, 207 },
};

PadScriptCmd D_acropolis_helicopter_landing_pad_80187D34[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 5), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D3C = { 120, 70, 20, 1 };

PadScriptCmd D_acropolis_helicopter_landing_pad_80187D40[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D48[2] = {
    { 0, 0, 20, 0 },
    { 255, 220, 25, 1 },
};

PadScriptCmd D_acropolis_helicopter_landing_pad_80187D50[4] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 10), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D60[2] = {
    { 0, 0, 1, 0 },
    { 220, 180, 25, 1 },
};

PadScriptCmd D_acropolis_helicopter_landing_pad_80187D68[4] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 10), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D78[2] = {
    { 0, 0, 1, 0 },
    { 150, 100, 25, 1 },
};

WorldCollisionFootstepSounds D_acropolis_helicopter_landing_pad_80187D80 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionFootstepSounds D_acropolis_helicopter_landing_pad_80187D8C = {
    0x1000000D,
    0x1000000F,
    0x1000000D,
};

WorldCollisionSurfaceProperties D_acropolis_helicopter_landing_pad_80187D98[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_helicopter_landing_pad_80187DA0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_helicopter_landing_pad_80187D8C },
};

WorldCollisionSurfaceProperties D_acropolis_helicopter_landing_pad_80187DA8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_helicopter_landing_pad_80187D80 },
};

WorldCollisionSurfaceProperties D_acropolis_helicopter_landing_pad_80187DB0[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_helicopter_landing_pad_80187D80 },
};

WorldCollisionSurfaceProperties D_acropolis_helicopter_landing_pad_80187DB8[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_helicopter_landing_pad_80187D80 },
};

WorldCollisionSurfaceProperties D_acropolis_helicopter_landing_pad_80187DC0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_acropolis_helicopter_landing_pad_80187DC8[8] = {
    D_acropolis_helicopter_landing_pad_80187D98,
    D_acropolis_helicopter_landing_pad_80187D98,
    D_acropolis_helicopter_landing_pad_80187DA0,
    D_acropolis_helicopter_landing_pad_80187DA8,
    D_acropolis_helicopter_landing_pad_80187DB0,
    D_acropolis_helicopter_landing_pad_80187DB8,
    D_acropolis_helicopter_landing_pad_80187DC0,
    D_acropolis_helicopter_landing_pad_80187D98,
};

static TmdBone _gAcropolisHelicopterLandingPadModel0A8E8Skeleton[1] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_skeleton.inc"
};

static u32 _gAcropolisHelicopterLandingPadModel0A8E8PartVerts[1] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_partVerts.inc"
};

static SVECTOR _gAcropolisHelicopterLandingPadModel0A8E8Verts[13] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_verts.inc"
};

static SVECTOR _gAcropolisHelicopterLandingPadModel0A8E8Normals[6] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_normals.inc"
};

static u32 _gAcropolisHelicopterLandingPadModel0A8E8Stream[42] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_stream.inc"
};

TmdSource gAcropolisHelicopterLandingPadModel0A8E8 = {
    0,
    312,
    0,
    1,
    _gAcropolisHelicopterLandingPadModel0A8E8PartVerts,
    _gAcropolisHelicopterLandingPadModel0A8E8Verts,
    _gAcropolisHelicopterLandingPadModel0A8E8Normals,
    _gAcropolisHelicopterLandingPadModel0A8E8Skeleton,
    _gAcropolisHelicopterLandingPadModel0A8E8Stream,
};

s32 D_acropolis_helicopter_landing_pad_80187F74 = 0;

s32 D_acropolis_helicopter_landing_pad_80187F78 = 0;

s16 D_acropolis_helicopter_landing_pad_80187F7C = 0;

Task* D_acropolis_helicopter_landing_pad_80187F80 = NULL;

s32 D_acropolis_helicopter_landing_pad_80187F84 = 0;

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

RoomEventMsg D_acropolis_helicopter_landing_pad_80187F90 = { 0, 0, 0, 0, 0, 0 };

/// Room state-machine task. State 0 resets the player weapon, posts 0x7D5 to
/// slot-4 entry 1 on a second-or-later visit (`gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant`), stamps
/// the save location with 0x12 and sets the override vector. States 1-4 wait
/// for `gGameSession->viewReady`, post 0x7D9 to slot 4 on a first visit, then
/// call `func_800A99B4`. State 5 asks the scene task to look up child 0x28,
/// plays that child's animation from a zeroed request, pushes the slot-3
/// weapon record with `field_4 = 9`
/// and hands the enemy's coordinate to slot 3 (0x3F5). State 6 queues CD
/// command 0x21 on a first visit and arms a 0x78 frame countdown; state 7
/// waits for the CD to go idle (or skips to 9 on a later visit); state 8
/// registers the area object and spawns the area; state 9 starts the exit
/// script pair and kills the task. Every frame, hitting countdown 0x5A queues
/// sound event 0x51100003.
void func_acropolis_helicopter_landing_pad_8017DA9C(Task* task)
{
    u8      param1[8];
    u8      param2[8];
    SVECTOR vec;
    Task*   spawned;
    void*   coord;

    switch (task->state) {
        case 0:
            task->spawnArg1.value = 0;
            Gp_MsgPlayerWeapon(0);
            task->state += 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant >= 2) {
                taskMessageDispatch(Gp_LookupSlot4(1), ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x12;
            vec.vx                                                     = 0x4B0;
            vec.vy                                                     = 0x4B0;
            vec.vz                                                     = 0x610;
            worldCoordSetAmbientColorOverride(&vec);
            break;
        case 1:
            if (gGameSession->viewReady != 0) {
                task->state += 1;
            }
            break;
        case 2:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant < 2) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_EXIT_PLACED_ACTORS, 0, 0);
            }
            task->state += 1;
            break;
        case 3:
            task->state += 1;
            break;
        case 4:
            func_800A99B4();
            task->state += 1;
            break;
        case 5:
            TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_OTHER_CHILD, 0x28, &spawned);
            TASK_MESSAGE_DISPATCH_POINTER(spawned, 0x7D3, &D_acropolis_helicopter_landing_pad_80184E28, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &D_acropolis_helicopter_landing_pad_801837B0, 0);
            D_acropolis_helicopter_landing_pad_80184E3C.animationId  = 9;
            D_acropolis_helicopter_landing_pad_80184E3C.source.index = gPlayerStatus.weapon + 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_acropolis_helicopter_landing_pad_80184E3C, 0);
            coord = spawned->extra.tmd->coords;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F5, coord, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &D_acropolis_helicopter_landing_pad_801837B0, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            task->state += 1;
            break;
        case 6:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant < 2) {
                param1[2] = 0x33;
                param2[0] = 0xA;
                param2[2] = 3;
                param1[3] = 0;
                param1[0] = 0;
                param2[1] = 0;
                param2[3] = 5;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
                func_800ABFF8();
                func_800AC000();
            }
            task->spawnArg1.value = 0x78;
            task->state          += 1;
            break;
        case 7:
            task->spawnArg1.value -= 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant >= 2) {
                task->state = 9;
            } else if (cdCmdIsIdle()) {
                func_800A99B4();
                task->state += 1;
            }
            break;
        case 8:
            task->spawnArg1.value -= 1;
            areaSetPlacementVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, 2, AREA_VARIANT_RESET_ALWAYS);
            areaSyncLocationVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
            Gp_SpawnArea(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
            D_acropolis_helicopter_landing_pad_80184D9C = 4;
            task->state                                += 1;
            break;
        case 9:
            task->spawnArg1.value -= 1;
            if (task->spawnArg1.value <= 0) {
                func_800E8634(D_acropolis_helicopter_landing_pad_8018467C, 1,
                              D_acropolis_helicopter_landing_pad_80184CF4);
                taskKill(task);
            }
            break;
    }
    if (task->spawnArg1.value == 0x5A) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD, 3), 0, 0);
    }
}

/// Frame-counted timeline task: three phases, each spawning the enemy task
/// (`func_..._8017E618`) and, a few frames later, a script-18 pair; kills
/// itself at frame 0x280.
void func_acropolis_helicopter_landing_pad_8017DE78(Task* task)
{
    switch (task->state) {
        case 0xC8:
        case 0x0:
            func_acropolis_helicopter_landing_pad_8017E618(0xF, 2);
            break;
        case 0x7:
        case 0xCF:
            Gp_SpawnScript18(D_acropolis_helicopter_landing_pad_80187D68, D_acropolis_helicopter_landing_pad_80187D78);
            break;
        case 0x19A:
            func_acropolis_helicopter_landing_pad_8017E618(0x16, 3);
            break;
        case 0x1A5:
            Gp_SpawnScript18(D_acropolis_helicopter_landing_pad_80187D50, D_acropolis_helicopter_landing_pad_80187D60);
            break;
        case 0x208:
            func_acropolis_helicopter_landing_pad_8017E618(0x1E, 4);
            break;
        case 0x217:
            Gp_SpawnScript18(D_acropolis_helicopter_landing_pad_80187D40, D_acropolis_helicopter_landing_pad_80187D48);
            break;
        case 0x280:
            taskKill(task);
            break;
    }
    task->state += 1;
}

void func_acropolis_helicopter_landing_pad_8017DFCC(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_FillPlayerHpMp();
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_PARTHENON_KEY);
            inventoryClearCollectedBit(0x102);
            Gp_SetItemSeenBit(0x102, 1);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 7);
            taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184E68, 0, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_MIST_R18;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(0, 0x11, 0, 0);
            displayReleaseMenuHold();
            taskKill(arg0);
            return;
    }
}

/// Turn-to-heading task: rotates the player actor's yaw
/// (`GameActor.rotation.vy`, masked to 12 bits) to `spawnArg1` in `0x100` steps
/// along the shorter direction. State 0 picks the unwrapped start angle
/// (`yaw`, or `yaw +/- 0x1000` when that is closer to the target) and the
/// step sign; state 1 steps, clamps onto the target and kills the task.
///
/// `tmp` carries three unrelated values (the first abs distance, the
/// "wrapped is closer" flag and state 1's new yaw), and `target` is re-read
/// in state 1: both are what puts the `slt` result and the state-1 sum in
/// the same registers as the original.
void func_acropolis_helicopter_landing_pad_8017E0F8(Task* arg0)
{
    GameActor* actor = (GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    s32        wrapped;
    s32        tmp;
    s32        dist;
    s32        target;
    s32        cur;

    switch (arg0->state) {
        case 0:
            D_acropolis_helicopter_landing_pad_80187F74 = actor->rotation.vy & 0xFFF;
            if (arg0->spawnArg1.value < D_acropolis_helicopter_landing_pad_80187F74) {
                wrapped = D_acropolis_helicopter_landing_pad_80187F74 - 0x1000;
            } else {
                wrapped = D_acropolis_helicopter_landing_pad_80187F74 + 0x1000;
            }
            target = arg0->spawnArg1.value;
            cur    = D_acropolis_helicopter_landing_pad_80187F74;
            tmp    = wrapped - target;
            if (tmp < 0) {
                tmp = -tmp;
            }
            dist = cur - target;
            if (dist < 0) {
                dist = -dist;
            }
            tmp = tmp < dist;
            if (tmp) {
                D_acropolis_helicopter_landing_pad_80187F74 = wrapped;
            }
            if (arg0->spawnArg1.value > D_acropolis_helicopter_landing_pad_80187F74) {
                D_acropolis_helicopter_landing_pad_80187F78 = 0x100;
            } else {
                D_acropolis_helicopter_landing_pad_80187F78 = -0x100;
            }
            arg0->state++;
            break;
        case 1:
            tmp                                         = D_acropolis_helicopter_landing_pad_80187F74 + D_acropolis_helicopter_landing_pad_80187F78;
            D_acropolis_helicopter_landing_pad_80187F74 = tmp;
            if (D_acropolis_helicopter_landing_pad_80187F78 > 0) {
                target = arg0->spawnArg1.value;
                if (target < tmp) {
                    D_acropolis_helicopter_landing_pad_80187F74 = target;
                    taskKill(arg0);
                }
            }
            if (D_acropolis_helicopter_landing_pad_80187F78 < 0) {
                if (D_acropolis_helicopter_landing_pad_80187F74 < arg0->spawnArg1.value) {
                    D_acropolis_helicopter_landing_pad_80187F74 = arg0->spawnArg1.value;
                    taskKill(arg0);
                }
            }
            actor->rotation.vy = D_acropolis_helicopter_landing_pad_80187F74;
            break;
    }
}

/// Helipad rotor / lift task: swings the player model's coord part 4 about X
/// by `-angle * 0x60 / 0x1000` and updates it. State 1 ramps
/// `D_acropolis_helicopter_landing_pad_80187F7C` up to 0x1000 (then state 2),
/// state 2 ramps it back to 0 (then state 0), state 0 resets it.
///
/// The extra locals are dead: the original wrote `dir` and reserved the
/// other aggregates (0x50 bytes of frame) for code that no longer runs, and
/// the constant stores survive in the binary.
void func_acropolis_helicopter_landing_pad_8017E270(Task* task)
{
    GfxCoord* coord;
    SVECTOR   unusedA;
    VECTOR    unusedB;
    VECTOR    dir;
    MATRIX    unusedM;
    SVECTOR   unusedC;

    coord  = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[4];
    dir.vx = -0x249;
    dir.vy = 0;
    dir.vz = 0xB8;

    switch (task->state) {
        case 0:
            D_acropolis_helicopter_landing_pad_80187F7C = 0;
            break;
        case 1:
            gfxRotMatrixX(&coord->coord, -(D_acropolis_helicopter_landing_pad_80187F7C * 0x60) / 0x1000, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            D_acropolis_helicopter_landing_pad_80187F7C += 0x190;
            if (D_acropolis_helicopter_landing_pad_80187F7C > 0x1000) {
                D_acropolis_helicopter_landing_pad_80187F7C = 0x1000;
                task->state                                 = 2;
            }
            break;
        case 2:
            gfxRotMatrixX(&coord->coord, -(D_acropolis_helicopter_landing_pad_80187F7C * 0x60) / 0x1000, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            D_acropolis_helicopter_landing_pad_80187F7C -= 0x190;
            if (D_acropolis_helicopter_landing_pad_80187F7C < 0) {
                D_acropolis_helicopter_landing_pad_80187F7C = 0;
                task->state                                 = 0;
            }
            break;
    }
}

/// Message 0x13EE handler: copies the requested `RoomEventMsg` to `dst`. For a
/// warp into stage 0xF it consults the room's phase
/// (`D_acropolis_helicopter_landing_pad_80184D9C`): phase 0 queues sound
/// event 0x1E and refuses the warp (returns 1); phase 2 starts cap slot 9
/// first. `queryOnly` set skips the side effect either way.
s32 func_acropolis_helicopter_landing_pad_8017E3F0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    if (src->areaId == GAME_AREA_ACROPOLIS_FIRE_ESCAPE) {
        if (D_acropolis_helicopter_landing_pad_80184D9C == 0) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                sndEvtRequestScriptStop(-1, 0x1E);
            }
            return 1;
        }
        if (D_acropolis_helicopter_landing_pad_80184D9C == 2) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_StartCapSlot(9, 1, 0);
            }
            return 0;
        }
        return 0;
    }
    return 1;
}

/// Does nothing and returns 0.
s32 func_acropolis_helicopter_landing_pad_8017E49C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// `DIRECTION_MESSAGE_ROOM_ACTION` handler. On action 0, once the room session flag
/// `D_acropolis_helicopter_landing_pad_80184E0C` is up and the phase is
/// still 0, starts the helicopter sequence: flags the session, loads the
/// bank pair, moves to phase 1, enables trigger 4 and disables trigger 8. Action 1 latches `D_acropolis_helicopter_landing_pad_80187F84`.
s32 func_acropolis_helicopter_landing_pad_8017E4A4(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    u8                     actionId;
    WorldCollisionTrigger* obj;
    WorldCollisionTrigger* obj2;

    if ((((const DirectionActionRequest*)firstArg)->actionId == 0) && (D_acropolis_helicopter_landing_pad_80184D9C == 0) && (D_acropolis_helicopter_landing_pad_80184E0C != 0)) {
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
        gStageSceneMusicEntry   = 1;
        func_800E8634(D_acropolis_helicopter_landing_pad_80183A34, 0, D_acropolis_helicopter_landing_pad_80183FA4);
        D_acropolis_helicopter_landing_pad_80184D9C = 1;
        obj                                         = (D_acropolis_helicopter_landing_pad_80185E7C + 4);
        obj2                                        = obj + 4;
        obj->flags                                 |= WORLD_COLLISION_TRIGGER_ENABLED;
        obj2->flags                                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    actionId = ((const DirectionActionRequest*)firstArg)->actionId;
    if (actionId == 1) {
        D_acropolis_helicopter_landing_pad_80187F84 = actionId;
    }
    return 0;
}

s32 func_acropolis_helicopter_landing_pad_8017E570(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if ((arg2 == 4) && (D_acropolis_helicopter_landing_pad_80184D9C == 2)) {
        taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 4, 0, 0);
    }
    return 0;
}

/// Spawns entry 3 of the room's task table.
void func_acropolis_helicopter_landing_pad_8017E5B8(void)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 3, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E5E8(void)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 1, 0, 0);
}

static void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 2, 0, arg0 | (arg1 << 8));
}

void func_acropolis_helicopter_landing_pad_8017E64C(void)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 5, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E67C(void)
{
    if (D_acropolis_helicopter_landing_pad_80187F84 != 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &D_acropolis_helicopter_landing_pad_80184E50, 0);
    }
}

void func_acropolis_helicopter_landing_pad_8017E6C0(s32 arg0)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 6, arg0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E6F0(void)
{
    sceneReleaseBattleRefWithRewards(Gp_LookupSlot4(0), 0x1B);
    gSceneCombatState.signals.bytes.endDelayFrames = 3;
}

/// Requests effect cancellation with `roomEffectRequestCancelAll` and sets bit 0 of
/// `Gp_StateC08.flags`.
void func_acropolis_helicopter_landing_pad_8017E724(void)
{
    roomEffectRequestCancelAll();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

void func_acropolis_helicopter_landing_pad_8017E75C(s32 arg0)
{
    D_acropolis_helicopter_landing_pad_80187F80->state = arg0;
}

void func_acropolis_helicopter_landing_pad_8017E76C(Task* task)
{
    GameActorMoveAnim moveAnim;

    switch (task->state) {
        case 0:
            moveAnim.approachAnimId = 0xC;
            moveAnim.arrivalAnimId  = 9;
            Gp_DispatchMsgPtrs(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_MOVE_TO, &D_acropolis_helicopter_landing_pad_801837E0, &moveAnim);
            task->state++;
            break;
        case 1:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskKill(task);
            }
            break;
    }
}

void func_acropolis_helicopter_landing_pad_8017E81C(Task* arg0)
{
    s32 packed;
    s32 lo;
    s32 scaled;
    s32 val;

    packed = arg0->spawnArg2.value;
    lo     = packed & 0xFF;

    switch (arg0->state) {
        case 0:
            arg0->spawnArg1.value = -lo;
            arg0->state++;
            break;
        case 1:
            if (lo < arg0->spawnArg1.value) {
                displaySetShakeY(0);
                taskKill(arg0);
            } else {
                val             = lo - ABS(arg0->spawnArg1.value);
                scaled          = val * (packed >> 8);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                val             = (scaled * (s32)(gRandomLcgState >> 16)) / lo >> 16;
                if (arg0->spawnArg1.value & 1) {
                    val = ABS(val);
                } else {
                    val = -ABS(val);
                }
                displaySetShakeY(val);
                arg0->spawnArg1.value++;
            }
            break;
    }
}

void func_acropolis_helicopter_landing_pad_8017E974(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(4, 1, 0);
        case 2:
        case 3:
            task->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                if (D_801156A8 == 1) {
                    Gp_MsgPlayerWeapon(1);
                    taskKill(task);
                    break;
                }
                Display_AcquireRef();
                task->state++;
            }
            task->state++;
            break;
        case 4:
            D_acropolis_helicopter_landing_pad_80184D9C = 3;
            taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 0, 0, 0);
            taskKill(task);
            break;
    }
}

static void func_acropolis_helicopter_landing_pad_8017EA6C(Task* task)
{
    task->msgTable = D_acropolis_helicopter_landing_pad_80183710;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    D_acropolis_helicopter_landing_pad_80187F84 = 0;
    D_acropolis_helicopter_landing_pad_80184E0C = 0;
    task->state++;
    func_800E8614(D_acropolis_helicopter_landing_pad_80183A04, 1);
    D_acropolis_helicopter_landing_pad_80187F80                 = taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 7, 0, 0);
    (D_acropolis_helicopter_landing_pad_80185E7C + 4)[0].flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
}

/// The room's script task: runs the state handler
/// `D_acropolis_helicopter_landing_pad_8017D5E4` names for `Task::state`,
/// through a copy of the table taken onto the stack.
void func_acropolis_helicopter_landing_pad_8017EB00(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_helicopter_landing_pad_8017D5E4;
    sp.funcs[task->state](task);
}
