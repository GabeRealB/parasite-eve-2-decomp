#include "rooms/acropolis_helicopter_landing_pad.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "acropolis_helicopter_landing_pad_private.h"

#include "actors/actor_511000.h"
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

/// A debug format string nothing in the room reads.
static const char D_acropolis_helicopter_landing_pad_8017D5D0[] = "%s (%5d,%5d,%5d)";

/// State handlers of the room's script task
/// `acropolisHelicopterLandingPadRoomTask`, indexed by
/// `Task::state`: set-up, the per-frame phase tick and `taskKill`.
static const TaskFuncTable3 D_acropolis_helicopter_landing_pad_8017D5E4 = {
    { _acropolisHelicopterLandingPadInitRoom, acropolisHelicopterLandingPadUpdateEncounterPhase, taskKill },
};

/// Packed selectors carried by the landing pad's exit, stair and room-action regions.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_CALLBACK_CONTROL    = WORLD_COLLISION_TRIGGER_ACTION_CALLBACK | (1 << 8),
    ACROPOLIS_HELICOPTER_LANDING_PAD_TRIGGER_START_ENCOUNTER  = 0,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TRIGGER_ENABLE_PLACEMENT = 1,
    ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_SURFACE_AND_STEPS  = (3 << 4) | 3,
    ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_FACING             = ACTOR_TRANSFORM_ANGLE_HALF_TURN / 16,
};

WorldCollisionTrigger gAcropolisHelicopterLandingPadActionTriggers[9] = {
    {
        .origin       = { -6528, -32, -6208, 0 },
        .vertices     = { { -384, 0, -192, 0 }, { 384, 0, -192, 0 }, { -384, 0, 192, 0 }, { 384, 0, 192, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { 0, 0, -ONE, 0 },
        .radius       = 429,
        .control      = ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_CALLBACK_CONTROL,
        .parameter0   = 0,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { -5274, -131, -1840, 0 },
        .vertices     = { { -800, 0, -688, 0 }, { 800, 0, -688, 0 }, { -800, 0, 688, 0 }, { 800, 0, 688, 0 } },
        .normal       = { 0, 4107, 0, 0 },
        .facingNormal = { 201, 0, -4091, 0 },
        .radius       = 1047,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 4,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 1055, -128, -6657, 0 },
        .vertices     = { { 73, 0, -1674, 0 }, { 1007, 0, -1340, 0 }, { -1006, 0, 1339, 0 }, { -73, 0, 1673, 0 } },
        .normal       = { 0, 4103, 0, 0 },
        .facingNormal = { 0, 0, -ONE, 0 },
        .radius       = 1673,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC,
        .parameter0   = ACROPOLIS_HELICOPTER_LANDING_PAD_TRIGGER_START_ENCOUNTER,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_QUAD,
    },
    {
        .origin       = { -6650, 472, -5301, 0 },
        .vertices     = { { -496, 0, -240, 0 }, { 496, 0, -240, 0 }, { -496, 0, 240, 0 }, { 496, 0, 240, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 550,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_FACING | WORLD_COLLISION_TRIGGER_AUTOMATIC,
        .parameter0   = ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_SURFACE_AND_STEPS,
        .parameter1   = ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_FACING,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { -6160, -128, -1824, 0 },
        .vertices     = { { -560, 0, -848, 0 }, { 560, 0, -848, 0 }, { -560, 0, 848, 0 }, { 560, 0, 848, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { 201, 0, -4091, 0 },
        .radius       = 1015,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC,
        .parameter0   = ACROPOLIS_HELICOPTER_LANDING_PAD_TRIGGER_ENABLE_PLACEMENT,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_QUAD,
    },
    {
        .origin       = { 5648, -32, 1184, 0 },
        .vertices     = { { -224, 0, -1344, 0 }, { 1440, 0, -1344, 0 }, { -224, 0, 1344, 0 }, { 1440, 0, 1344, 0 } },
        .normal       = { 0, 4117, 0, 0 },
        .facingNormal = { 4091, 0, 201, 0 },
        .radius       = 1966,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 6,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD,
    },
    {
        .origin       = { -800, -64, 5568, 0 },
        .vertices     = { { 1760, 0, -864, 0 }, { 1760, 0, 1760, 0 }, { -1760, 0, -864, 0 }, { -1760, 0, 1760, 0 } },
        .normal       = { 0, 4111, 0, 0 },
        .facingNormal = { 0, 0, 4095, 0 },
        .radius       = 2482,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 7,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD,
    },
    {
        .origin       = { -5536, -64, 544, 0 },
        .vertices     = { { 864, 0, 1472, 0 }, { -1984, 0, 1472, 0 }, { 864, 0, -1472, 0 }, { -1984, 0, -1472, 0 } },
        .normal       = { 0, 4101, 0, 0 },
        .facingNormal = { -4091, 0, -201, 0 },
        .radius       = 2468,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 8,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD,
    },
    {
        .origin       = { -6144, -64, -1824, 0 },
        .vertices     = { { -560, 0, -848, 0 }, { 560, 0, -848, 0 }, { -560, 0, 848, 0 }, { 560, 0, 848, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { -4052, 0, -601, 0 },
        .radius       = 1015,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = 10,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
    },
};

WorldCollisionOccluder gAcropolisHelicopterLandingPadSightOccluders[2] = {
    {
        .origin   = { -256, -944, -240, 0 },
        .vertices = { { -5632, -1968, -5616, 0 }, { 5632, -1968, 5616, 0 }, { -5632, 1968, -5616, 0 }, { 5632, 1968, 5616, 0 } },
        .normal   = { 2897, 0, -2906, 0 },
        .radius   = 8192,
        .flags    = 1,
    },
    {
        .origin   = { 0, -896, 0, 0 },
        .vertices = { { 5616, -1920, -5632, 0 }, { -5616, -1920, 5632, 0 }, { 5616, 1920, -5632, 0 }, { -5616, 1920, 5632, 0 } },
        .normal   = { 2900, 0, 2892, 0 },
        .radius   = 8175,
        .flags    = 1 | WORLD_COLLISION_OCCLUDER_LAST,
    },
};

/// Placement-entry keys used by this room's resource recipes.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_NO9_RESOURCE_ENTRY        = 27,
    ACROPOLIS_HELICOPTER_LANDING_PAD_RUPERT_RESOURCE_ENTRY     = 144,
    ACROPOLIS_HELICOPTER_LANDING_PAD_HELICOPTER_RESOURCE_ENTRY = 254,
};

/// No. 9 GOLEM's loading and body-spawn recipe for landing-pad variant 1.
///
/// Entry 27 loads base-group-50 file 109 and spawns descriptor 0.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gAcropolisHelicopterLandingPadVariant1Resources[2] = {
    { .entryId = ACROPOLIS_HELICOPTER_LANDING_PAD_NO9_RESOURCE_ENTRY, .fileNumber = 109, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50, .taskIndex = 0, .taskTable = gActor510900TaskDescriptors },
    { .entryId = AREA_PLACEMENT_END },
};

/// No. 9, Rupert and helicopter recipes for landing-pad variant 2.
///
/// All use file 110: No. 9 selects base group 50, Rupert and the helicopter
/// select base group 60. Each spawns descriptor 0; all three map placements
/// have fileIdLow=0, so both base-60 recipes participate in base loading.
/// The final record is `AREA_PLACEMENT_END`. Keep this room, its map-owned
/// placements and the selected actor overlays loaded during loading, spawning
/// and model-flag updates.
static AreaResource _gAcropolisHelicopterLandingPadVariant2Resources[4] = {
    { .entryId = ACROPOLIS_HELICOPTER_LANDING_PAD_NO9_RESOURCE_ENTRY, .fileNumber = 110, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50, .taskIndex = 0, .taskTable = gActor511000No9GolemTaskDescriptors },
    { .entryId = ACROPOLIS_HELICOPTER_LANDING_PAD_RUPERT_RESOURCE_ENTRY, .fileNumber = 110, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = 0, .taskTable = gActor511000RupertTaskDescriptors },
    { .entryId = ACROPOLIS_HELICOPTER_LANDING_PAD_HELICOPTER_RESOURCE_ENTRY, .fileNumber = 110, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = 0, .taskTable = gActor511000HelicopterTaskDescriptors },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant D_acropolis_helicopter_landing_pad_801861E8[13] = {
    { NULL, NULL },
    { gMapAkropolisHelicopterLandingPadVariant1Placements, _gAcropolisHelicopterLandingPadVariant1Resources },
    { gMapAkropolisHelicopterLandingPadVariant2Placements, _gAcropolisHelicopterLandingPadVariant2Resources },
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

WorldCoordRoomLights gAcropolisHelicopterLandingPadRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gAcropolisHelicopterLandingPadDirectionalLights),
    .directionalLights     = _gAcropolisHelicopterLandingPadDirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gAcropolisHelicopterLandingPadPointLights),
    .pointLights           = _gAcropolisHelicopterLandingPadPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Keeps decoded background strips in mapped view 1 without source sprites.
///
/// The zero-count first record selects image strips; the following record ends
/// cached-sprite traversal. The view retains this address in its empty source slot.
static SpriteBatch _gAcropolisHelicopterLandingPadView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 17 depth-sorted foreground texture rectangles in mapped view 2.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. The loaded overlay owns these mutable sources.
static SpriteSource _gAcropolisHelicopterLandingPadView2SpriteSources[] = {
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

/// Adds all view-2 source sprites over decoded background strips, then ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisHelicopterLandingPadView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Keeps decoded background strips in mapped view 3 without source sprites.
///
/// The zero-count selector precedes the terminator; its retained empty source
/// address is never dereferenced as sprites. View clipping is independent.
static SpriteBatch _gAcropolisHelicopterLandingPadView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clips mapped view 3 to (4,4,133,235) pixels until sorting depth 1500.
///
/// Coordinates are from the draw buffer's upper-left corner. The final record
/// ends the list; clipping also applies to scene primitives, without source sprites.
static SpriteDrawArea _gAcropolisHelicopterLandingPadView3DrawAreas[] = {
    { { 4, 4, 133, 235 }, 1500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// The five depth-sorted foreground texture rectangles in mapped view 4.
///
/// Positions and sizes are view-centred pixels; depth is an unscaled sorting
/// value. These mutable sources remain live with the room overlay.
static SpriteSource _gAcropolisHelicopterLandingPadView4SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 16, 112 } }, 0, -104, 2125, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 144 } }, -16, -112, 1750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 208 } }, -48, -120, 1500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 240 } }, -88, -120, 1125, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 240 } }, -160, -120, 500, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

/// Adds all view-4 source sprites over decoded background strips, then ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisHelicopterLandingPadView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The eight foreground texture rectangles at sorting depth 500 in mapped view 5.
///
/// Positions and sizes are view-centred pixels. The room overlay owns these
/// mutable sources, independently of the view's scene clipping commands.
static SpriteSource _gAcropolisHelicopterLandingPadView5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -160, 0, 500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 80 } }, -104, 16, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 96 } }, -56, 24, 500, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -8, 32, 500, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 104 } }, 40, 16, 500, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 64, 0, 500, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 88, -16, 500, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, -32, 500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
};

/// Adds all view-5 source sprites over decoded background strips, then ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisHelicopterLandingPadView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clips mapped view 5 to (267,0,51,239) pixels until sorting depth 3625.
///
/// The draw-buffer rectangle also clips scene primitives. The second record
/// terminates the list without emitting commands.
static SpriteDrawArea _gAcropolisHelicopterLandingPadView5DrawAreas[] = {
    { { 267, 0, 51, 239 }, 3625 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// The ten depth-sorted foreground texture rectangles in mapped view 6.
///
/// Positions and sizes are view-centred pixels; depth is unscaled. These
/// mutable sources remain live with the room overlay.
static SpriteSource _gAcropolisHelicopterLandingPadView6SpriteSources[] = {
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

/// Adds all view-6 source sprites over decoded background strips, then ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisHelicopterLandingPadView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Applies mapped view 6's zero-size clip at (109,118) until sorting depth 1750.
///
/// Coordinates are draw-buffer pixels. The authored zero dimensions are
/// retained; the second record terminates the list.
static SpriteDrawArea _gAcropolisHelicopterLandingPadView6DrawAreas[] = {
    { { 109, 118, 0, 0 }, 1750 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// The 29 texture rectangles forming mapped view 7's three source ranges.
///
/// The ranges contain 12 foreground rectangles, three deeper rectangles and
/// fourteen tiles at depth 2875. Positions and sizes are view-centred pixels;
/// depth is unscaled. Sources remain mutable and owned by the loaded overlay.
static SpriteSource _gAcropolisHelicopterLandingPadView7SpriteSources[] = {
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

/// Adds view 7's source ranges [0,12), [12,15) and [15,29) over background strips.
///
/// The zero-count selector and final terminator are both required. The first
/// two ranges retain serialized bytes 2 and 1 whose role is unproven; current
/// packet processing reads neither byte.
static SpriteBatch _gAcropolisHelicopterLandingPadView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 2, 0 } },
    { 12, 3, 0, 0, { 1, 0 } },
    { 15, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clips mapped view 7 to (1,0,46,239) pixels until sorting depth 2750.
///
/// Coordinates are draw-buffer pixels; the second record terminates the list.
static SpriteDrawArea _gAcropolisHelicopterLandingPadView7DrawAreas[] = {
    { { 1, 0, 46, 239 }, 2750 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// The ten depth-sorted foreground texture rectangles in mapped view 8.
///
/// Positions and sizes are view-centred pixels; depth is unscaled. The room
/// overlay owns these mutable sources for the view's complete lifetime.
static SpriteSource _gAcropolisHelicopterLandingPadView8SpriteSources[] = {
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

/// Adds all view-8 source sprites over decoded background strips, then ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisHelicopterLandingPadView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Applies mapped view 8's zero-size clip at (53,23) until sorting depth 2250.
///
/// Coordinates are draw-buffer pixels. The authored zero dimensions are
/// retained; the second record terminates the list.
static SpriteDrawArea _gAcropolisHelicopterLandingPadView8DrawAreas[] = {
    { { 53, 23, 0, 0 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// The twelve depth-sorted foreground texture rectangles in mapped view 9.
///
/// Positions and sizes are view-centred pixels; depth is unscaled, including
/// the authored depth 50 at source 9. The loaded overlay owns these mutable sources.
static SpriteSource _gAcropolisHelicopterLandingPadView9SpriteSources[] = {
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

/// Adds all view-9 source sprites over decoded background strips, then ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisHelicopterLandingPadView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clips mapped view 9 to (1,0,63,239) pixels until sorting depth 3500.
///
/// Coordinates are draw-buffer pixels; the second record terminates the list.
static SpriteDrawArea _gAcropolisHelicopterLandingPadView9DrawAreas[] = {
    { { 1, 0, 63, 239 }, 3500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// The 27 foreground texture rectangles for mapped view 10.
///
/// Positions are pixels relative to the view centre; depths are unscaled sorting values.
/// Batches select source elements; these writable records live with the room overlay.
static SpriteSource _gAcropolisHelicopterLandingPadView10SpriteSources[27] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -104, -64, 550, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -96, -40, 550, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -88, -16, 525, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -80, 8, 500, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -72, 24, 500, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -64, 48, 450, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -56, 80, 425, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 24 } }, -48, 96, 400, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 16, 88, 375, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 40, 80, 375, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 64, 72, 375, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 80, 64, 400, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 104, 56, 400, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 120, 48, 425, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, -64, -120, 2130, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 112 } }, -48, -120, 1812, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 128 } }, -32, -120, 1687, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 136 } }, -16, -120, 1450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, 96, 64, 250, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 32 } }, 96, 32, 250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 56 } }, 112, -24, 250, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, 128, -120, 325, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 40 } }, 96, 80, 250, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 24, 48, 875, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 40, 64, 800, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 56, 80, 750, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 72, 96, 625, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
};

/// Adds view 10's 27 source sprites in three ranges over decoded background strips.
///
/// Ranges are [0,14), [14,23) and [23,27); the final record ends the list.
/// The nonzero serialized bytes in the first two ranges have no observed readers.
static SpriteBatch _gAcropolisHelicopterLandingPadView10SpriteBatches[5] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 14, .field_6 = { 1, 0 } },
    { .firstSprite = 14, .spriteCount = 9, .field_6 = { 2, 0 } },
    { .firstSprite = 23, .spriteCount = ARRAY_SIZE(_gAcropolisHelicopterLandingPadView10SpriteSources) - 23 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Clips mapped view 10 to (3, 0, 99, 239) pixels until sorting depth 2250.
///
/// Coordinates are draw-buffer pixels; the second record terminates the list.
static SpriteDrawArea _gAcropolisHelicopterLandingPadView10DrawAreas[2] = {
    { .clipRect = { 3, 0, 99, 239 }, .restoreDepth = 2250 },
    { .restoreDepth = SPRITE_DRAW_AREA_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 11.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView11SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 12.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView12SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 13.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView13SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 14.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView14SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The foreground texture rectangle shared by mapped views 15 and 27.
///
/// Positions are pixels relative to the view centre; depths are unscaled sorting values.
/// Batches select source elements; these writable records live with the room overlay.
static SpriteSource _gAcropolisHelicopterLandingPadViews15And27SpriteSources[1] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 136 } }, -16, -16, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

/// Adds the shared source sprite over decoded background strips in mapped views 15 and 27.
///
/// Counts are source elements; the final record terminates the list.
static SpriteBatch _gAcropolisHelicopterLandingPadViews15And27SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .spriteCount = ARRAY_SIZE(_gAcropolisHelicopterLandingPadViews15And27SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Clips mapped views 15 and 27 to (2, 1, 149, 238) pixels until sorting depth 1125.
///
/// Coordinates are draw-buffer pixels; the second record terminates the list.
static SpriteDrawArea _gAcropolisHelicopterLandingPadViews15And27DrawAreas[2] = {
    { .clipRect = { 2, 1, 149, 238 }, .restoreDepth = 1125 },
    { .restoreDepth = SPRITE_DRAW_AREA_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 16.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView16SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 3 foreground texture rectangles for mapped view 17.
///
/// Positions are pixels relative to the view centre; depths are unscaled sorting values.
/// Batches select source elements; these writable records live with the room overlay.
static SpriteSource _gAcropolisHelicopterLandingPadView17SpriteSources[3] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 104, 136 } }, -160, -24, 400, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 120, 56 } }, -56, 56, 375, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 56 } }, 64, 64, 390, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
};

/// Adds all source sprites over decoded background strips in mapped view 17.
///
/// Counts are source elements; the final record terminates the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView17SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .spriteCount = ARRAY_SIZE(_gAcropolisHelicopterLandingPadView17SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 4 foreground texture rectangles for mapped view 18.
///
/// Positions are pixels relative to the view centre; depths are unscaled sorting values.
/// Batches select source elements; these writable records live with the room overlay.
static SpriteSource _gAcropolisHelicopterLandingPadView18SpriteSources[4] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 120, 40 } }, -160, 40, 675, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 168, 40 } }, -160, 80, 450, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 152, 32 } }, 8, 88, 450, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 120, 40 } }, 40, 48, 675, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
};

/// Adds all source sprites over decoded background strips in mapped view 18.
///
/// Counts are source elements; the final record terminates the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView18SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .spriteCount = ARRAY_SIZE(_gAcropolisHelicopterLandingPadView18SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 19.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView19SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 20.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView20SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// The 6 foreground texture rectangles for mapped view 21.
///
/// Positions are pixels relative to the view centre; depths are unscaled sorting values.
/// Batches select source elements; these writable records live with the room overlay.
static SpriteSource _gAcropolisHelicopterLandingPadView21SpriteSources[6] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 32 } }, -160, 88, 250, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -88, 80, 250, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -32, 72, 250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 56 } }, 16, 64, 250, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 64 } }, 72, 56, 250, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, 128, 48, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

/// Adds all source sprites over decoded background strips in mapped view 21.
///
/// Counts are source elements; the final record terminates the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView21SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .spriteCount = ARRAY_SIZE(_gAcropolisHelicopterLandingPadView21SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 22.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView22SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 23.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView23SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 24.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView24SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 25.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView25SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Keeps decoded background strips without adding source sprites in mapped view 26.
///
/// The zero first count selects image strips; the second record ends the list.
static SpriteBatch _gAcropolisHelicopterLandingPadView26SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteDrawArea D_acropolis_helicopter_landing_pad_80187810[2] = {
    { { 2, 1, 149, 238 }, 1125 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteView gAcropolisHelicopterLandingPadSpriteViews[27] = {
    { { .empty = _gAcropolisHelicopterLandingPadView1SpriteBatches }, _gAcropolisHelicopterLandingPadView1SpriteBatches, NULL },
    { { .elements = _gAcropolisHelicopterLandingPadView2SpriteSources }, _gAcropolisHelicopterLandingPadView2SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView3SpriteBatches }, _gAcropolisHelicopterLandingPadView3SpriteBatches, _gAcropolisHelicopterLandingPadView3DrawAreas },
    { { .elements = _gAcropolisHelicopterLandingPadView4SpriteSources }, _gAcropolisHelicopterLandingPadView4SpriteBatches, NULL },
    { { .elements = _gAcropolisHelicopterLandingPadView5SpriteSources }, _gAcropolisHelicopterLandingPadView5SpriteBatches, _gAcropolisHelicopterLandingPadView5DrawAreas },
    { { .elements = _gAcropolisHelicopterLandingPadView6SpriteSources }, _gAcropolisHelicopterLandingPadView6SpriteBatches, _gAcropolisHelicopterLandingPadView6DrawAreas },
    { { .elements = _gAcropolisHelicopterLandingPadView7SpriteSources }, _gAcropolisHelicopterLandingPadView7SpriteBatches, _gAcropolisHelicopterLandingPadView7DrawAreas },
    { { .elements = _gAcropolisHelicopterLandingPadView8SpriteSources }, _gAcropolisHelicopterLandingPadView8SpriteBatches, _gAcropolisHelicopterLandingPadView8DrawAreas },
    { { .elements = _gAcropolisHelicopterLandingPadView9SpriteSources }, _gAcropolisHelicopterLandingPadView9SpriteBatches, _gAcropolisHelicopterLandingPadView9DrawAreas },
    { { .elements = _gAcropolisHelicopterLandingPadView10SpriteSources }, _gAcropolisHelicopterLandingPadView10SpriteBatches, _gAcropolisHelicopterLandingPadView10DrawAreas },
    { { .empty = _gAcropolisHelicopterLandingPadView11SpriteBatches }, _gAcropolisHelicopterLandingPadView11SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView12SpriteBatches }, _gAcropolisHelicopterLandingPadView12SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView13SpriteBatches }, _gAcropolisHelicopterLandingPadView13SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView14SpriteBatches }, _gAcropolisHelicopterLandingPadView14SpriteBatches, NULL },
    { { .elements = _gAcropolisHelicopterLandingPadViews15And27SpriteSources }, _gAcropolisHelicopterLandingPadViews15And27SpriteBatches, _gAcropolisHelicopterLandingPadViews15And27DrawAreas },
    { { .empty = _gAcropolisHelicopterLandingPadView16SpriteBatches }, _gAcropolisHelicopterLandingPadView16SpriteBatches, NULL },
    { { .elements = _gAcropolisHelicopterLandingPadView17SpriteSources }, _gAcropolisHelicopterLandingPadView17SpriteBatches, NULL },
    { { .elements = _gAcropolisHelicopterLandingPadView18SpriteSources }, _gAcropolisHelicopterLandingPadView18SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView19SpriteBatches }, _gAcropolisHelicopterLandingPadView19SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView20SpriteBatches }, _gAcropolisHelicopterLandingPadView20SpriteBatches, NULL },
    { { .elements = _gAcropolisHelicopterLandingPadView21SpriteSources }, _gAcropolisHelicopterLandingPadView21SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView22SpriteBatches }, _gAcropolisHelicopterLandingPadView22SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView23SpriteBatches }, _gAcropolisHelicopterLandingPadView23SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView24SpriteBatches }, _gAcropolisHelicopterLandingPadView24SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView25SpriteBatches }, _gAcropolisHelicopterLandingPadView25SpriteBatches, NULL },
    { { .empty = _gAcropolisHelicopterLandingPadView26SpriteBatches }, _gAcropolisHelicopterLandingPadView26SpriteBatches, NULL },
    { { .elements = _gAcropolisHelicopterLandingPadViews15And27SpriteSources }, _gAcropolisHelicopterLandingPadViews15And27SpriteBatches, _gAcropolisHelicopterLandingPadViews15And27DrawAreas },
};

ViewCamera gAcropolisHelicopterLandingPadViewCameras[27] = {
    { .transform = { .m = { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, .t = { 1500, 30000, 2500 } }, .screenDistance = 432 },
    { .transform = { .m = { { 2020, 0, 3563 }, { 2282, 3145, -1294 }, { -2735, 2624, 1551 } }, .t = { 4724, 2583, 7606 } }, .screenDistance = 257 },
    { .transform = { .m = { { 1448, 0, 3831 }, { 929, 3973, -351 }, { -3716, 994, 1405 } }, .t = { 111, 1700, 7565 } }, .screenDistance = 257 },
    { .transform = { .m = { { 683, 0, -4038 }, { -993, 3970, -168 }, { 3914, 1007, 662 } }, .t = { 3525, 1700, 7067 } }, .screenDistance = 257 },
    { .transform = { .m = { { 3492, 0, 2140 }, { 389, 4027, -636 }, { -2104, 745, 3434 } }, .t = { -8514, 1700, 9737 } }, .screenDistance = 257 },
    { .transform = { .m = { { 2030, 0, 3557 }, { 1121, 3887, -640 }, { -3375, 1291, 1927 } }, .t = { -10344, 2310, 2157 } }, .screenDistance = 257 },
    { .transform = { .m = { { -3041, 0, 2743 }, { 488, 4030, 541 }, { -2699, 729, -2993 } }, .t = { -8208, 1268, -8598 } }, .screenDistance = 230 },
    { .transform = { .m = { { -3740, 0, -1668 }, { -184, 4070, 413 }, { 1658, 452, -3717 } }, .t = { 3618, 1700, -11650 } }, .screenDistance = 230 },
    { .transform = { .m = { { -1924, 0, -3615 }, { -627, 4033, 333 }, { 3560, 710, -1895 } }, .t = { 9705, 1727, -8007 } }, .screenDistance = 257 },
    { .transform = { .m = { { 3730, 0, -1690 }, { -618, 3812, -1364 }, { 1573, 1497, 3472 } }, .t = { 7825, 2067, 3862 } }, .screenDistance = 230 },
    { .transform = { .m = { { 1608, 0, 3767 }, { 2218, 3309, -947 }, { -3044, 2412, 1299 } }, .t = { -3535, 9746, 3169 } }, .screenDistance = 230 },
    { .transform = { .m = { { 1216, 0, -3911 }, { -2605, 3055, -810 }, { 2917, 2728, 907 } }, .t = { 5463, 10490, 2359 } }, .screenDistance = 230 },
    { .transform = { .m = { { 1538, 0, -3795 }, { -2650, 2931, -1074 }, { 2717, 2860, 1101 } }, .t = { 3937, 6355, 1314 } }, .screenDistance = 257 },
    { .transform = { .m = { { 3507, 0, -2116 }, { -1885, 1858, -3125 }, { 960, 3650, 1591 } }, .t = { -5011, 8020, -3917 } }, .screenDistance = 230 },
    { .transform = { .m = { { 836, 0, 4009 }, { -1269, 3885, 264 }, { -3803, -1297, 793 } }, .t = { 292, 673, 6887 } }, .screenDistance = 207 },
    { .transform = { .m = { { 1453, 0, -3829 }, { -771, 4012, -292 }, { 3750, 824, 1423 } }, .t = { -1784, 1471, 7126 } }, .screenDistance = 207 },
    { .transform = { .m = { { 1155, 0, 3929 }, { -1737, 3673, 510 }, { -3524, -1811, 1035 } }, .t = { 1062, 2444, 7359 } }, .screenDistance = 207 },
    { .transform = { .m = { { -564, 0, 4056 }, { 503, 4064, 70 }, { -4025, 508, -559 } }, .t = { 2521, 4108, 1483 } }, .screenDistance = 230 },
    { .transform = { .m = { { 2575, 0, -3185 }, { -930, 3917, -752 }, { 3046, 1196, 2462 } }, .t = { 3838, 4675, 2020 } }, .screenDistance = 188 },
    { .transform = { .m = { { 2586, 0, -3176 }, { -638, 4012, -520 }, { 3111, 824, 2533 } }, .t = { 2361, 4678, 1361 } }, .screenDistance = 188 },
    { .transform = { .m = { { 3554, 0, 2035 }, { -1127, 3411, 1967 }, { -1695, -2267, 2959 } }, .t = { -870, 2992, 6656 } }, .screenDistance = 207 },
    { .transform = { .m = { { 3950, 0, 1082 }, { 504, 3625, -1839 }, { -958, 1906, 3496 } }, .t = { -270, 4973, 6269 } }, .screenDistance = 243 },
    { .transform = { .m = { { -4092, 0, 162 }, { 123, 2664, 3108 }, { -105, 3110, -2662 } }, .t = { 839, 3802, 9054 } }, .screenDistance = 230 },
    { .transform = { .m = { { 2856, 0, -2935 }, { 2818, 1144, 2742 }, { 820, -3932, 798 } }, .t = { 2576, 5537, 2556 } }, .screenDistance = 230 },
    { .transform = { .m = { { 522, 0, -4062 }, { -2349, 3341, -302 }, { 3314, 2368, 426 } }, .t = { 8857, 2945, 2306 } }, .screenDistance = 230 },
    { .transform = { .m = { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, .t = { 1000, 30000, -2000 } }, .screenDistance = 432 },
    { .transform = { .m = { { 836, 0, 4009 }, { -1269, 3885, 264 }, { -3803, -1297, 793 } }, .t = { 292, 673, 6887 } }, .screenDistance = 207 },
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

/// Cue-2 bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_WALK_SOUND          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_RUN_SOUND           = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_SCRIPTED_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
    ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_WALK_SOUND          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0D,
    ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_RUN_SOUND           = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0F,
};

/// Paired footstep scripts for the lower floor and surface classes 4 and 5.
///
/// Walk, run and scripted-jump modes use distinct cue bases in the loaded
/// type-1 bank. Cue 1 adds one and companion playback adds 100. Surface records
/// borrow these bases for the room overlay's lifetime without modifying them.
static WorldCollisionFootstepSounds _gAcropolisHelicopterLandingPadFloorFootstepSounds = {
    .walk         = ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_WALK_SOUND,
    .run          = ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_RUN_SOUND,
    .scriptedJump = ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_SCRIPTED_JUMP_SOUND,
};

/// Paired footstep scripts for the upper floor and scripted fire-escape stairs.
///
/// Walking and scripted jumps share the same cue base; running has its own.
/// Cue 1 adds one and companion playback adds 100 in the loaded type-1 bank.
/// Surface class 2 borrows this record while the room overlay remains loaded.
static WorldCollisionFootstepSounds _gAcropolisHelicopterLandingPadStairFootstepSounds = {
    .walk         = ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_WALK_SOUND,
    .run          = ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_RUN_SOUND,
    .scriptedJump = ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_WALK_SOUND,
};

/// Silent solid collision policy shared by surface classes 0, 1 and 7.
///
/// Blocks probes, allows weapon impacts and applies pushback. No footstep
/// table is selected; the unproven first byte retains its authored zero.
static WorldCollisionSurfaceProperties _gAcropolisHelicopterLandingPadSilentSolidSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Solid upper-floor policy also selected explicitly during the exit stair sequence.
///
/// Surface class 2 blocks probes, allows weapon impacts, applies pushback and
/// borrows the stair footstep bases for the loaded overlay's lifetime.
static WorldCollisionSurfaceProperties _gAcropolisHelicopterLandingPadStairSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisHelicopterLandingPadStairFootstepSounds,
};

/// Solid lower-floor policy for surface class 3, with floor footstep cues.
///
/// Blocks probes, allows weapon impacts and applies pushback. The floor cue
/// bases remain borrowed while the room overlay is loaded.
static WorldCollisionSurfaceProperties _gAcropolisHelicopterLandingPadFloorSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisHelicopterLandingPadFloorFootstepSounds,
};

/// Probe-transparent perimeter policy for surface class 4.
///
/// Applies pushback and retains floor footstep cues while probes pass and
/// weapon impacts are suppressed. The room overlay owns this single record.
static WorldCollisionSurfaceProperties _gAcropolisHelicopterLandingPadPerimeterSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisHelicopterLandingPadFloorFootstepSounds,
};

/// Surface class 5's probe-transparent policy with pushback and floor footsteps.
///
/// Weapon impacts are suppressed. This distinct authored record is selected
/// by class 5; the geometry or material associated with that class is unproven.
static WorldCollisionSurfaceProperties _gAcropolisHelicopterLandingPadSurface5Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisHelicopterLandingPadFloorFootstepSounds,
};

/// Silent outer-boundary policy for surface class 6.
///
/// Blocks probes and applies pushback while suppressing weapon impacts;
/// no footstep table is selected. The room overlay owns this single record.
static WorldCollisionSurfaceProperties _gAcropolisHelicopterLandingPadOuterBoundarySurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gAcropolisHelicopterLandingPadSurfacePropertiesByClass[8] = {
    &_gAcropolisHelicopterLandingPadSilentSolidSurface,
    &_gAcropolisHelicopterLandingPadSilentSolidSurface,
    &_gAcropolisHelicopterLandingPadStairSurface,
    &_gAcropolisHelicopterLandingPadFloorSurface,
    &_gAcropolisHelicopterLandingPadPerimeterSurface,
    &_gAcropolisHelicopterLandingPadSurface5Properties,
    &_gAcropolisHelicopterLandingPadOuterBoundarySurface,
    &_gAcropolisHelicopterLandingPadSilentSolidSurface,
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

/// Current unwrapped yaw of the singleton player-turn task.
///
/// Angles use 4096 units per turn. Initialization selects the shorter path,
/// possibly outside 0..4095; updates clamp at the target before narrowing to the player's yaw.
static s32 _gAcropolisHelicopterLandingPadTurnYaw = 0;

/// Signed yaw increment of the singleton player-turn task, in angle units per update.
///
/// Initialization chooses +256 or -256, with 4096 units per turn.
/// The value persists through task completion and is replaced by the next turn.
static s32 _gAcropolisHelicopterLandingPadTurnStep = 0;

/// Q12 weight of the persistent player model-part pitch pulse.
///
/// Idle resets it to zero; rise and fall step by 400 and clamp to 0..4096.
/// The signed halfword permits the falling update to test a negative intermediate.
static s16 _gAcropolisHelicopterLandingPadPitchWeight = 0;

Task* D_acropolis_helicopter_landing_pad_80187F80 = NULL;

s32 gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled = 0;

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
            TASK_MESSAGE_DISPATCH_POINTER(liftTask, ACTOR_MESSAGE_PLAY_ANIMATION, &gAcropolisHelicopterLandingPadLiftRaiseRequest, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAcropolisHelicopterLandingPadLiftPlayerPlacement, 0);
            gAcropolisHelicopterLandingPadLiftPlayerAnimationRequest.animationId  = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_PLAYER_CLIP;
            gAcropolisHelicopterLandingPadLiftPlayerAnimationRequest.source.index = gPlayerStatus.weapon + 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &gAcropolisHelicopterLandingPadLiftPlayerAnimationRequest, 0);
            liftCoord = liftTask->extra.tmd->coords;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, liftCoord, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAcropolisHelicopterLandingPadLiftPlayerPlacement, 0);
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
            gAcropolisHelicopterLandingPadEncounterPhase = ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_READY;
            task->state                                 += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE:
            task->spawnArg1.value -= 1;
            if (task->spawnArg1.value <= 0) {
                evsStartScriptWithSkip(gAcropolisHelicopterLandingPadDepartureScript, EVENT_SCRIPT_HUD_KEEP,
                                       gAcropolisHelicopterLandingPadDepartureSkipScript);
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
            taskSpawnFromTable(gAcropolisHelicopterLandingPadMovieTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_MOVIE_TASK, 0, 0);
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
            _gAcropolisHelicopterLandingPadTurnYaw = player->rotation.vy & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value < _gAcropolisHelicopterLandingPadTurnYaw) {
                wrappedYaw = _gAcropolisHelicopterLandingPadTurnYaw - ACTOR_TRANSFORM_ANGLE_TURN;
            } else {
                wrappedYaw = _gAcropolisHelicopterLandingPadTurnYaw + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            targetYaw  = task->spawnArg1.value;
            currentYaw = _gAcropolisHelicopterLandingPadTurnYaw;
            // Compare both unwrapped paths before choosing the turn direction.
            ACROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES(wrappedYaw, currentYaw, targetYaw, turnValue, currentDistance);
            if (turnValue) {
                _gAcropolisHelicopterLandingPadTurnYaw = wrappedYaw;
            }
            if (task->spawnArg1.value > _gAcropolisHelicopterLandingPadTurnYaw) {
                _gAcropolisHelicopterLandingPadTurnStep = ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP;
            } else {
                _gAcropolisHelicopterLandingPadTurnStep = -ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP;
            }
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_UPDATE:
            turnValue                              = _gAcropolisHelicopterLandingPadTurnYaw + _gAcropolisHelicopterLandingPadTurnStep;
            _gAcropolisHelicopterLandingPadTurnYaw = turnValue;
            if (_gAcropolisHelicopterLandingPadTurnStep > 0) {
                targetYaw = task->spawnArg1.value;
                if (targetYaw < turnValue) {
                    _gAcropolisHelicopterLandingPadTurnYaw = targetYaw;
                    taskKill(task);
                }
            }
            if (_gAcropolisHelicopterLandingPadTurnStep < 0) {
                if (_gAcropolisHelicopterLandingPadTurnYaw < task->spawnArg1.value) {
                    _gAcropolisHelicopterLandingPadTurnYaw = task->spawnArg1.value;
                    taskKill(task);
                }
            }
            // Keep the endpoint test strict: an exact arrival runs one more update.
            player->rotation.vy = _gAcropolisHelicopterLandingPadTurnYaw;
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
            _gAcropolisHelicopterLandingPadPitchWeight = 0;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE:
            _acropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, _gAcropolisHelicopterLandingPadPitchWeight);
            _gAcropolisHelicopterLandingPadPitchWeight += ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP;
            if (_gAcropolisHelicopterLandingPadPitchWeight > ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE) {
                _gAcropolisHelicopterLandingPadPitchWeight = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE;
                task->state                                = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL:
            _acropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, _gAcropolisHelicopterLandingPadPitchWeight);
            _gAcropolisHelicopterLandingPadPitchWeight -= ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP;
            if (_gAcropolisHelicopterLandingPadPitchWeight < 0) {
                _gAcropolisHelicopterLandingPadPitchWeight = 0;
                task->state                                = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE;
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
        if (gAcropolisHelicopterLandingPadEncounterPhase == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // This exact-entry selector matches idle sound slots; its purpose is unproven.
                sndEvtRequestScriptStop(ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_SELECTOR, ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_CONTROL);
            }
            return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ORDINARY;
        }
        if (gAcropolisHelicopterLandingPadEncounterPhase == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED) {
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
        ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_START_ENCOUNTER  = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_LATCH_PLACEMENT  = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_WAITING       = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_STARTED       = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER       = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CAP_INTERACTION_TRIGGER = 8,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_MUSIC_ENTRY   = 1,
    };
    u8                     actionId;
    WorldCollisionTrigger* placementTrigger;
    WorldCollisionTrigger* capInteractionTrigger;

    if ((request->actionId == ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_START_ENCOUNTER) && (gAcropolisHelicopterLandingPadEncounterPhase == ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_WAITING) && (gAcropolisHelicopterLandingPadEncounterReady != 0)) {
        // Swap the encounter triggers before normal play resumes.
        gGameSession->flowFlags   = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
        gStageCountdownMusicEntry = ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_MUSIC_ENTRY;
        evsStartScriptWithSkip(gAcropolisHelicopterLandingPadEncounterIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gAcropolisHelicopterLandingPadEncounterIntroSkipScript);
        gAcropolisHelicopterLandingPadEncounterPhase = ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_STARTED;
        placementTrigger                             = &gAcropolisHelicopterLandingPadActionTriggers[ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER];
        capInteractionTrigger                        = &gAcropolisHelicopterLandingPadActionTriggers[ACROPOLIS_HELICOPTER_LANDING_PAD_CAP_INTERACTION_TRIGGER];
        placementTrigger->flags                     |= WORLD_COLLISION_TRIGGER_ENABLED;
        capInteractionTrigger->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    actionId = request->actionId;
    if (actionId == ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_LATCH_PLACEMENT) {
        gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled = actionId;
    }
    return 0;
}

s32 acropolisHelicopterLandingPadHandleCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_COMMAND_CONFIRM_DEPARTURE = 4 };

    if ((commandId == ACROPOLIS_HELICOPTER_LANDING_PAD_COMMAND_CONFIRM_DEPARTURE) && (gAcropolisHelicopterLandingPadEncounterPhase == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED)) {
        taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_CONFIRM_DEPARTURE, 0, 0);
    }
    return 0;
}

void acropolisHelicopterLandingPadStartDepartureShakeTimeline(void)
{
    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SHAKE_TIMELINE, 0, 0);
}

void acropolisHelicopterLandingPadStartPlayerMoveToSceneMark(void)
{
    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_MOVE_PLAYER, 0, 0);
}

/// Spawns one triangular vertical shake from a duration and signed pixel amplitude.
///
/// Half-duration is 1..255 updates; the amplitude's left shift and the shake
/// task's signed products must fit s32. Authored amplitudes are 2, 3 and 4.
/// Packs the duration into bits 0..7 and the amplitude above it; retains no pointer.
static void _acropolisHelicopterLandingPadSpawnScreenShake(s32 halfDurationFrames, s32 amplitudePixels)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT = 8 };

    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SCREEN_SHAKE, 0, halfDurationFrames | (amplitudePixels << ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT));
}

void acropolisHelicopterLandingPadStartReturnToMist(void)
{
    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_RETURN_TO_MIST, 0, 0);
}

void acropolisHelicopterLandingPadPlacePlayerAfterEncounter(void)
{
    if (gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled != 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAcropolisHelicopterLandingPadPostEncounterPlayerPlacement, 0);
    }
}

void acropolisHelicopterLandingPadStartPlayerYawTurn(s32 targetYaw)
{
    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_TURN_PLAYER, targetYaw, 0);
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
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
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
            TASK_MESSAGE_DISPATCH_POINTERS(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_MOVE_TO, &gAcropolisHelicopterLandingPadSceneMarkStorage.destination, &moveAnim);
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
            gAcropolisHelicopterLandingPadEncounterPhase = ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_ACCEPTED;
            taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PREPARE_DEPARTURE, 0, 0);
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

    task->msgTable = gAcropolisHelicopterLandingPadRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled = 0;
    gAcropolisHelicopterLandingPadEncounterReady                = 0;
    task->state++;
    evsStartScript(gAcropolisHelicopterLandingPadAmbientSoundScript, EVENT_SCRIPT_HUD_KEEP);
    D_acropolis_helicopter_landing_pad_80187F80 = taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PLAYER_PITCH, 0, 0);
    placementTrigger                            = &gAcropolisHelicopterLandingPadActionTriggers[ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER];
    placementTrigger->flags                    &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
}

void acropolisHelicopterLandingPadRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_acropolis_helicopter_landing_pad_8017D5E4;
    stateHandlers.funcs[task->state](task);
}
