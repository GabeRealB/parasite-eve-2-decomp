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
#include "gameplay/actor_presentation.h"
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
#include "gameplay/player_state.h"
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

/// Turn-to-heading task state: current unwrapped yaw and signed step.
extern s32   D_acropolis_helicopter_landing_pad_80187F74;
extern s32   D_acropolis_helicopter_landing_pad_80187F78;
extern s16   D_acropolis_helicopter_landing_pad_80187F7C;
extern Task* D_acropolis_helicopter_landing_pad_80187F80;

/// Vibration programs paired with the departure timeline's screen shakes.
extern PadScriptCmd              D_acropolis_helicopter_landing_pad_80187D40[2];
extern PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D48[2];
extern PadScriptCmd              D_acropolis_helicopter_landing_pad_80187D50[4];
extern PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D60[2];
extern PadScriptCmd              D_acropolis_helicopter_landing_pad_80187D68[4];
extern PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80187D78[2];

static void _acropolisHelicopterLandingPadSpawnScreenShake(s32 halfDurationFrames, s32 amplitudePixels);
static void _acropolisHelicopterLandingPadInitRoom(Task* task);

/// Script-task indices in the landing pad's shared descriptor table.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PREPARE_DEPARTURE = 0,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_MOVE_PLAYER       = 1,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SCREEN_SHAKE      = 2,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SHAKE_TIMELINE    = 3,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_CONFIRM_DEPARTURE = 4,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_RETURN_TO_MIST    = 5,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_TURN_PLAYER       = 6,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PLAYER_PITCH      = 7,
};

/// Encounter/departure phases consulted by the room's departure callbacks.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING            = 0,
    ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED = 2,
    ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_ACCEPTED = 3,
    ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_READY    = 4,
};

/// A debug format string nothing in the room reads.
static const char D_acropolis_helicopter_landing_pad_8017D5D0[] = "%s (%5d,%5d,%5d)";

/// State handlers of the room's script task
/// `acropolisHelicopterLandingPadRoomTask`, indexed by
/// `Task::state`: set-up, the per-frame phase tick and `taskKill`.
static const TaskFuncTable3 D_acropolis_helicopter_landing_pad_8017D5E4 = {
    { _acropolisHelicopterLandingPadInitRoom, acropolisHelicopterLandingPadUpdateEncounterPhase, taskKill },
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

/// Composes one weighted pitch pulse into the player's borrowed model coordinate.
///
/// Borrows a live, writable coordinate with an animation-supplied local matrix.
/// Weight is signed Q12 in 0..4096; pitch peaks at -96 angle units (4096 per
/// turn). Composes about local X, dirties the cache and recomposes the chain.
static inline void _acropolisHelicopterLandingPadApplyPlayerPitchPulse(GfxCoord* playerPart, s32 weight)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_MAX_ANGLE  = 96,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE = 4096,
    };

    gfxRotMatrixX(&playerPart->coord, -(weight * ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_MAX_ANGLE) / ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE, GRAPHICS_ROTATION_COMPOSE);
    playerPart->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(playerPart);
}

/// Samples one triangular shake envelope point, advancing the shared random sequence.
///
/// Borrows the shake task and re-reads its cursor for the alternating sign.
/// The cursor must be in [-halfDurationFrames, halfDurationFrames], with a
/// half-duration of 1..255 updates. Arithmetic shift by 8 gives the packed
/// signed pixel amplitude. Both signed products must fit s32; division truncates
/// toward zero before the Q16 shift. Changes the LCG but does not move the cursor.
static inline s32 _acropolisHelicopterLandingPadSampleScreenShake(Task* task, s32 packedShake, s32 halfDurationFrames)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT      = 8,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS = 16,
    };
    s32 shakeSample;
    s32 envelopeAmplitude;

    shakeSample       = halfDurationFrames - ABS(task->spawnArg1.value);
    envelopeAmplitude = shakeSample * (packedShake >> ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT);
    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    shakeSample       = (envelopeAmplitude * (s32)(gRandomLcgState >> ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS)) / halfDurationFrames >> ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS;
    if (task->spawnArg1.value & 1) {
        shakeSample = ABS(shakeSample);
    } else {
        shakeSample = -ABS(shakeSample);
    }
    return shakeSample;
}

void acropolisHelicopterLandingPadPrepareDepartureTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SETUP            = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_VIEW        = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_EXIT_ACTORS      = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_ACTOR_EXIT  = 3,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_RESTORE_VIEW     = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_LIFT       = 5,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_LOAD_RESOURCES   = 6,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_LOAD        = 7,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SPAWN_PLACEMENTS = 8,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE      = 9,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT          = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VIEW             = 18,
        ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_CHILD                 = 40,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_PLAYER_CLIP      = 9,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_DELAY_FRAMES     = 120,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SOUND_REMAINING  = 90,
        ACROPOLIS_HELICOPTER_LANDING_PAD_REVISIT_HIDDEN_ACTOR       = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ACTOR_HIDE_MODEL           = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CD_FILE_KEY_BYTES          = 4,
    };
    u8        fileKey[ACROPOLIS_HELICOPTER_LANDING_PAD_CD_FILE_KEY_BYTES];
    u8        loadArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    SVECTOR   ambientColor;
    Task*     liftTask;
    GfxCoord* liftCoord;

    /// Enqueues the stage-zero departure bundle with its image-placement offsets.
    ///
    /// Expands to a compound statement and repeatedly evaluates both arguments.
    /// They must be side-effect-free, distinct writable byte arrays of at least
    /// four bytes. Enqueue copies the consumed bytes synchronously and ignores
    /// key byte 1. Captures no local identifiers; subsequent calls retain their
    /// original order. Kept as a macro so stores address the caller's arrays directly.
#define ACROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES(fileKey, loadArgs) \
    {                                                                                 \
        enum {                                                                        \
            ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_GROUP    = 51,            \
            ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_HUNDREDS = 10,            \
            ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_X_PAGES = 3,             \
            ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_Y_ROWS  = 5,             \
        };                                                                            \
        (fileKey)[2]  = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_GROUP;        \
        (loadArgs)[0] = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_HUNDREDS;     \
        (loadArgs)[2] = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_X_PAGES;     \
        (fileKey)[3]  = 0;                                                            \
        (fileKey)[0]  = 0;                                                            \
        (loadArgs)[1] = CD_COMMAND_LOAD_DEFAULT;                                      \
        (loadArgs)[3] = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_Y_ROWS;      \
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, (fileKey), (loadArgs));                    \
        func_800ABFF8();                                                              \
        func_800AC000();                                                              \
    }

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SETUP:
            task->spawnArg1.value = 0;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            task->state += 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant >= ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                taskMessageDispatch(sceneFindPlacedActor(ACROPOLIS_HELICOPTER_LANDING_PAD_REVISIT_HIDDEN_ACTOR), ACTOR_MESSAGE_SET_MODEL_DRAW, ACROPOLIS_HELICOPTER_LANDING_PAD_ACTOR_HIDE_MODEL, 0);
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VIEW;
            ambientColor.vx                                            = 1200;
            ambientColor.vy                                            = 1200;
            ambientColor.vz                                            = 1552;
            worldCoordSetAmbientColorOverride(&ambientColor);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_VIEW:
            if (gGameSession->viewReady != 0) {
                task->state += 1;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_EXIT_ACTORS:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant < ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_EXIT_PLACED_ACTORS, 0, 0);
            }
            task->state += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_ACTOR_EXIT:
            task->state += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_RESTORE_VIEW:
            loadingRequestViewGraphicsRestore();
            task->state += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_LIFT:
            // Place in the lift's frame before attaching the player to its root.
            TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_OTHER_CHILD, ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_CHILD, &liftTask);
            TASK_MESSAGE_DISPATCH_POINTER(liftTask, ACTOR_MESSAGE_PLAY_ANIMATION, &D_acropolis_helicopter_landing_pad_80184E28, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &D_acropolis_helicopter_landing_pad_801837B0, 0);
            D_acropolis_helicopter_landing_pad_80184E3C.animationId  = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_PLAYER_CLIP;
            D_acropolis_helicopter_landing_pad_80184E3C.source.index = gPlayerStatus.weapon + 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_acropolis_helicopter_landing_pad_80184E3C, 0);
            liftCoord = liftTask->extra.tmd->coords;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, liftCoord, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &D_acropolis_helicopter_landing_pad_801837B0, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            task->state += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_LOAD_RESOURCES:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant < ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                ACROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES(fileKey, loadArgs);
            }
            task->spawnArg1.value = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_DELAY_FRAMES;
            task->state          += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_LOAD:
            task->spawnArg1.value -= 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.variant >= ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                task->state = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE;
            } else if (cdCmdIsIdle()) {
                loadingRequestViewGraphicsRestore();
                task->state += 1;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SPAWN_PLACEMENTS:
            task->spawnArg1.value -= 1;
            // The newly loaded actor variant must be spawned before the scene script.
            areaSetPlacementVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT, AREA_VARIANT_RESET_ALWAYS);
            areaSyncLocationVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
            areaSpawnPlacements(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
            D_acropolis_helicopter_landing_pad_80184D9C = ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_READY;
            task->state                                += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE:
            task->spawnArg1.value -= 1;
            if (task->spawnArg1.value <= 0) {
                evsStartScriptWithSkip(D_acropolis_helicopter_landing_pad_8018467C, EVENT_SCRIPT_HUD_KEEP,
                                       D_acropolis_helicopter_landing_pad_80184CF4);
                taskKill(task);
            }
            break;
    }
    if (task->spawnArg1.value == ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SOUND_REMAINING) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD, 3), 0, 0);
    }
}
#undef ACROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES

void acropolisHelicopterLandingPadDepartureShakeTimelineTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_FIRST_FRAME        = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_FIRST_FRAME    = 7,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_REPEAT_FRAME       = 200,
        ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_REPEAT_FRAME   = 207,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_SECOND_FRAME       = 410,
        ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_SECOND_FRAME   = 421,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_THIRD_FRAME        = 520,
        ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_THIRD_FRAME    = 535,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_TIMELINE_END_FRAME = 640,
    };

    // Screen impulses precede the corresponding controller vibration cues.
    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_REPEAT_FRAME:
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_FIRST_FRAME:
            _acropolisHelicopterLandingPadSpawnScreenShake(15, 2);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_FIRST_FRAME:
        case ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_REPEAT_FRAME:
            padScriptSpawn(D_acropolis_helicopter_landing_pad_80187D68, D_acropolis_helicopter_landing_pad_80187D78);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_SECOND_FRAME:
            _acropolisHelicopterLandingPadSpawnScreenShake(22, 3);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_SECOND_FRAME:
            padScriptSpawn(D_acropolis_helicopter_landing_pad_80187D50, D_acropolis_helicopter_landing_pad_80187D60);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_THIRD_FRAME:
            _acropolisHelicopterLandingPadSpawnScreenShake(30, 4);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_THIRD_FRAME:
            padScriptSpawn(D_acropolis_helicopter_landing_pad_80187D40, D_acropolis_helicopter_landing_pad_80187D48);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_TIMELINE_END_FRAME:
            taskKill(task);
            break;
    }
    task->state += 1;
}

void acropolisHelicopterLandingPadReturnToMistTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_PREPARE        = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WAIT_MOVIE     = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD         = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE     = 0x102,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_DIALOGUE       = 7,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SCENE_EVENT    = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WARP           = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_ROOM           = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SPRITE_VARIANT = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_MOVIE_TASK     = 0,
    };

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_PREPARE:
            // Reset Acropolis rewards and progress before the streamed interlude.
            playerStateRestoreFullHpMp();
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_PARTHENON_KEY);
            inventoryClearCollectedBit(ACROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE);
            itemSetIdentified(ACROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE, true);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_DIALOGUE);
            taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184E68, ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_MOVIE_TASK, 0, 0);
            task->state += 1;
            return;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WAIT_MOVIE:
            task->state = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD;
            return;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD:
            // The movie task holds the task list while playback owns the display.
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SCENE_EVENT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_MIST_R18;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_ROOM;
            gDisplayState.spriteVariant                                 = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SPRITE_VARIANT;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            displayReleaseMenuHold();
            taskKill(task);
            return;
    }
}

void acropolisHelicopterLandingPadTurnPlayerYawTask(Task* task)
{
    GameActor* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    s32        wrappedYaw;
    s32        turnValue; // Wrapped distance/comparison scratch, then the stepped yaw
    s32        currentDistance;
    s32        targetYaw;
    s32        currentYaw;

    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_INITIALIZE = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_UPDATE     = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP       = 256,
    };

    /// Compares absolute target distances, overwriting the wrapped-distance scratch with the result.
    ///
    /// All arguments must be side-effect-free s32 values or local lvalues.
    /// The two scratch arguments must be distinct and must not alias the inputs.
    /// Repeatedly evaluates inputs; expands to a compound statement and captures
    /// no identifiers. The caller reuses the result scratch for the next yaw.
#define ACROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES(wrapped, current, target, comparison, currentDistance) \
    {                                                                                                                 \
        (comparison) = (wrapped) - (target);                                                                          \
        if ((comparison) < 0) {                                                                                       \
            (comparison) = -(comparison);                                                                             \
        }                                                                                                             \
        (currentDistance) = (current) - (target);                                                                     \
        if ((currentDistance) < 0) {                                                                                  \
            (currentDistance) = -(currentDistance);                                                                   \
        }                                                                                                             \
        (comparison) = (comparison) < (currentDistance);                                                              \
    }

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_INITIALIZE:
            D_acropolis_helicopter_landing_pad_80187F74 = player->rotation.vy & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value < D_acropolis_helicopter_landing_pad_80187F74) {
                wrappedYaw = D_acropolis_helicopter_landing_pad_80187F74 - ACTOR_TRANSFORM_ANGLE_TURN;
            } else {
                wrappedYaw = D_acropolis_helicopter_landing_pad_80187F74 + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            targetYaw  = task->spawnArg1.value;
            currentYaw = D_acropolis_helicopter_landing_pad_80187F74;
            // Compare both unwrapped paths before choosing the turn direction.
            ACROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES(wrappedYaw, currentYaw, targetYaw, turnValue, currentDistance);
            if (turnValue) {
                D_acropolis_helicopter_landing_pad_80187F74 = wrappedYaw;
            }
            if (task->spawnArg1.value > D_acropolis_helicopter_landing_pad_80187F74) {
                D_acropolis_helicopter_landing_pad_80187F78 = ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP;
            } else {
                D_acropolis_helicopter_landing_pad_80187F78 = -ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP;
            }
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_UPDATE:
            turnValue                                   = D_acropolis_helicopter_landing_pad_80187F74 + D_acropolis_helicopter_landing_pad_80187F78;
            D_acropolis_helicopter_landing_pad_80187F74 = turnValue;
            if (D_acropolis_helicopter_landing_pad_80187F78 > 0) {
                targetYaw = task->spawnArg1.value;
                if (targetYaw < turnValue) {
                    D_acropolis_helicopter_landing_pad_80187F74 = targetYaw;
                    taskKill(task);
                }
            }
            if (D_acropolis_helicopter_landing_pad_80187F78 < 0) {
                if (D_acropolis_helicopter_landing_pad_80187F74 < task->spawnArg1.value) {
                    D_acropolis_helicopter_landing_pad_80187F74 = task->spawnArg1.value;
                    taskKill(task);
                }
            }
            // Keep the endpoint test strict: an exact arrival runs one more update.
            player->rotation.vy = D_acropolis_helicopter_landing_pad_80187F74;
            break;
    }
}
#undef ACROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES

void acropolisHelicopterLandingPadPlayerPitchPulseTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_PART        = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE  = 4096,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP = 400,
    };
    GfxCoord* playerPart;
    // These unused aggregate roles are unproven; their stack slots and stores remain in the image.
    SVECTOR unusedShortVectorBefore;
    VECTOR  unusedVectorBefore;
    VECTOR  unusedVector;
    MATRIX  unusedMatrix;
    SVECTOR unusedShortVectorAfter;

    playerPart      = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_PART];
    unusedVector.vx = -0x249;
    unusedVector.vy = 0;
    unusedVector.vz = 0xB8;

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE:
            D_acropolis_helicopter_landing_pad_80187F7C = 0;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE:
            _acropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, D_acropolis_helicopter_landing_pad_80187F7C);
            D_acropolis_helicopter_landing_pad_80187F7C += ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP;
            if (D_acropolis_helicopter_landing_pad_80187F7C > ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE) {
                D_acropolis_helicopter_landing_pad_80187F7C = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE;
                task->state                                 = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL:
            _acropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, D_acropolis_helicopter_landing_pad_80187F7C);
            D_acropolis_helicopter_landing_pad_80187F7C -= ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP;
            if (D_acropolis_helicopter_landing_pad_80187F7C < 0) {
                D_acropolis_helicopter_landing_pad_80187F7C = 0;
                task->state                                 = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE;
            }
            break;
    }
}

s32 acropolisHelicopterLandingPadResolveDeparture(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_CAP_SLOT = 9,
        ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ORDINARY      = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ROOM_HANDLED  = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_SELECTOR = -1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_CONTROL  = 30,
    };

    *reply = *request;
    if (request->areaId == GAME_AREA_ACROPOLIS_FIRE_ESCAPE) {
        if (D_acropolis_helicopter_landing_pad_80184D9C == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // This exact-entry selector matches idle sound slots; its purpose is unproven.
                sndEvtRequestScriptStop(ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_SELECTOR, ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_CONTROL);
            }
            return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ORDINARY;
        }
        if (D_acropolis_helicopter_landing_pad_80184D9C == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capStartSequenceSlot(ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            }
            return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ROOM_HANDLED;
        }
        return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ROOM_HANDLED;
    }
    return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ORDINARY;
}

s32 acropolisHelicopterLandingPadRefuseKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 acropolisHelicopterLandingPadHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_START_ENCOUNTER = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_LATCH_PLACEMENT = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_WAITING      = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_STARTED      = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER      = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_KEY_ITEM_TRIGGER       = 8,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_MUSIC_ENTRY  = 1,
    };
    u8                     actionId;
    WorldCollisionTrigger* placementTrigger;
    WorldCollisionTrigger* keyItemTrigger;

    if ((request->actionId == ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_START_ENCOUNTER) && (D_acropolis_helicopter_landing_pad_80184D9C == ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_WAITING) && (D_acropolis_helicopter_landing_pad_80184E0C != 0)) {
        // Swap the encounter triggers before normal play resumes.
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
        gStageSceneMusicEntry   = ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_MUSIC_ENTRY;
        evsStartScriptWithSkip(D_acropolis_helicopter_landing_pad_80183A34, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_helicopter_landing_pad_80183FA4);
        D_acropolis_helicopter_landing_pad_80184D9C = ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_STARTED;
        placementTrigger                            = &D_acropolis_helicopter_landing_pad_80185E7C[ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER];
        keyItemTrigger                              = &D_acropolis_helicopter_landing_pad_80185E7C[ACROPOLIS_HELICOPTER_LANDING_PAD_KEY_ITEM_TRIGGER];
        placementTrigger->flags                    |= WORLD_COLLISION_TRIGGER_ENABLED;
        keyItemTrigger->flags                      &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    actionId = request->actionId;
    if (actionId == ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_LATCH_PLACEMENT) {
        D_acropolis_helicopter_landing_pad_80187F84 = actionId;
    }
    return 0;
}

s32 acropolisHelicopterLandingPadHandleCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_COMMAND_CONFIRM_DEPARTURE = 4 };

    if ((commandId == ACROPOLIS_HELICOPTER_LANDING_PAD_COMMAND_CONFIRM_DEPARTURE) && (D_acropolis_helicopter_landing_pad_80184D9C == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED)) {
        taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_CONFIRM_DEPARTURE, 0, 0);
    }
    return 0;
}

void acropolisHelicopterLandingPadStartDepartureShakeTimeline(void)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SHAKE_TIMELINE, 0, 0);
}

void acropolisHelicopterLandingPadStartPlayerMoveToSceneMark(void)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_MOVE_PLAYER, 0, 0);
}

/// Spawns one triangular vertical shake from a duration and signed pixel amplitude.
///
/// Half-duration is 1..255 updates; the amplitude's left shift and the shake
/// task's signed products must fit s32. Authored amplitudes are 2, 3 and 4.
/// Packs the duration into bits 0..7 and the amplitude above it; retains no pointer.
static void _acropolisHelicopterLandingPadSpawnScreenShake(s32 halfDurationFrames, s32 amplitudePixels)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT = 8 };

    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SCREEN_SHAKE, 0, halfDurationFrames | (amplitudePixels << ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT));
}

void acropolisHelicopterLandingPadStartReturnToMist(void)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_RETURN_TO_MIST, 0, 0);
}

void acropolisHelicopterLandingPadPlacePlayerAfterEncounter(void)
{
    if (D_acropolis_helicopter_landing_pad_80187F84 != 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &D_acropolis_helicopter_landing_pad_80184E50, 0);
    }
}

void acropolisHelicopterLandingPadStartPlayerYawTurn(s32 targetYaw)
{
    taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_TURN_PLAYER, targetYaw, 0);
}

void acropolisHelicopterLandingPadReleaseEncounterBattle(void)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_ACTOR         = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_BATTLE_END_DELAY_FRAMES = 3,
    };

    // The resident API ignores its second argument; rewards come from the actor.
    sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_ACTOR), 0x1B);
    gSceneCombatState.signals.bytes.endDelayFrames = ACROPOLIS_HELICOPTER_LANDING_PAD_BATTLE_END_DELAY_FRAMES;
}

void acropolisHelicopterLandingPadLockAttachmentsForEncounter(void)
{
    roomEffectRequestCancelAll();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

void acropolisHelicopterLandingPadSetPlayerPitchPulseState(s32 pulseState)
{
    D_acropolis_helicopter_landing_pad_80187F80->state = pulseState;
}

void acropolisHelicopterLandingPadMovePlayerToSceneMarkTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_START         = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_WAIT          = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_APPROACH_CLIP = 12,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_ARRIVAL_CLIP  = 9,
    };
    GameActorMoveAnim moveAnim;

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_START:
            moveAnim.approachAnimId = ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_APPROACH_CLIP;
            moveAnim.arrivalAnimId  = ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_ARRIVAL_CLIP;
            TASK_MESSAGE_DISPATCH_POINTERS(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_MOVE_TO, &D_acropolis_helicopter_landing_pad_801837E0, &moveAnim);
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_WAIT:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskKill(task);
            }
            break;
    }
}

void acropolisHelicopterLandingPadScreenShakeTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_INITIALIZE    = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_UPDATE        = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_DURATION_MASK = 0xFF,
    };
    s32 packedShake;
    s32 halfDurationFrames;
    s32 shakeSample;

    packedShake        = task->spawnArg2.value;
    halfDurationFrames = packedShake & ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_DURATION_MASK;

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_INITIALIZE:
            task->spawnArg1.value = -halfDurationFrames;
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_UPDATE:
            if (halfDurationFrames < task->spawnArg1.value) {
                displaySetShakeY(0);
                taskKill(task);
            } else {
                // Grow and decay the envelope around the middle frame.
                shakeSample = _acropolisHelicopterLandingPadSampleScreenShake(task, packedShake, halfDurationFrames);
                displaySetShakeY(shakeSample);
                task->spawnArg1.value++;
            }
            break;
    }
}

void acropolisHelicopterLandingPadConfirmDepartureTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_START         = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CHECK_CHOICE  = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DELAY         = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_WAIT_HOLD     = 3,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DEPART        = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CAP_SLOT      = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CANCEL_CHOICE = 1,
    };

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capStartSequenceSlot(ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            // Fall through: starting the CAP also advances to the choice check.
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DELAY:
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_WAIT_HOLD:
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CHECK_CHOICE:
            if (capIsBusy() == 0) {
                if (D_801156A8 == ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CANCEL_CHOICE) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                    taskKill(task);
                    break;
                }
                displayAcquireMenuHold();
                task->state++;
            }
            // Advance even while CAP is busy; acceptance advances twice in this update.
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DEPART:
            D_acropolis_helicopter_landing_pad_80184D9C = ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_ACCEPTED;
            taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PREPARE_DEPARTURE, 0, 0);
            taskKill(task);
            break;
    }
}

/// Initializes the room task's message routing, encounter gates and persistent pitch task.
///
/// Requires the room resources and task bank to remain loaded. Registers the
/// task in the room slot, clears both encounter latches, starts the initial room sound script
/// and disables placement trigger 4. The retained pitch task lives until room
/// teardown; allocation failure is not checked. Advances to the room update state.
static void _acropolisHelicopterLandingPadInitRoom(Task* task)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER = 4 };
    WorldCollisionTrigger* placementTrigger;

    task->msgTable = D_acropolis_helicopter_landing_pad_80183710;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    D_acropolis_helicopter_landing_pad_80187F84 = 0;
    D_acropolis_helicopter_landing_pad_80184E0C = 0;
    task->state++;
    evsStartScript(D_acropolis_helicopter_landing_pad_80183A04, EVENT_SCRIPT_HUD_KEEP);
    D_acropolis_helicopter_landing_pad_80187F80 = taskSpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PLAYER_PITCH, 0, 0);
    placementTrigger                            = &D_acropolis_helicopter_landing_pad_80185E7C[ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER];
    placementTrigger->flags                    &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
}

void acropolisHelicopterLandingPadRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_acropolis_helicopter_landing_pad_8017D5E4;
    stateHandlers.funcs[task->state](task);
}
