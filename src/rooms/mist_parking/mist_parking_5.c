#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "mist_parking_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_cutscene.h"

#define ROOM_PART declarations_mist_parking_5
#include "assets/rooms/mist/parking/declarations.inc"

/// World-coordinate capsule endpoints and the separate pulsing point-glow centre.
///
/// Positions use signed whole world-coordinate units and are borrowed only
/// during drawing. Row drawing takes three consecutive capsule pairs. Every
/// vector includes the fourth halfword read alongside Z by the GTE.
typedef struct {
    SVECTOR capsuleEndpoints[13][2]; // Thirteen capsule pairs; pair 1 is retained but unused
    SVECTOR pointCenter;             // Centre shared by the cyan diamond and disc glows
} _MistParkingGlowWorldPoints;
STATIC_ASSERT_SIZEOF(_MistParkingGlowWorldPoints, 0xD8);

/// XZ cell-row stride of the parking grid's authored cell index.
enum { MIST_PARKING_COLLISION_CELL_COUNT_Z = 4 };

/// Scene target, MIST stock profile and script delays for the variant-1 shop poses.
enum {
    MIST_PARKING_JODIE_SCENE_PLACEMENT   = 1,
    MIST_PARKING_SHOP_STOCK_PROFILE      = 0x10,
    MIST_PARKING_SHOP_POSE_DELAY_UPDATES = 20,
    MIST_PARKING_SHOP_REPLY_END_UPDATES  = 30
};

#define ROOM_PART gMistParkingJodieShopTalkScript
#include "assets/rooms/mist/parking/events.inc"
/// Authored parking glow positions in signed world-coordinate units.
///
/// Capsule drawers borrow one endpoint pair; row drawers borrow three
/// consecutive pairs. The separate centre serves
/// all point glows. Keep the unused second pair and all vector fourth halfwords.
static _MistParkingGlowWorldPoints _gMistParkingGlowWorldPoints = {
    .capsuleEndpoints = {
        { { 11430, -2690, -4750, 0 }, { 10230, -2690, -4750, 0 } },
        { { 8070, -2690, -4750, 0 }, { 6860, -2690, -4750, 0 } },
        { { 3870, -3190, -4840, 0 }, { 3870, -3190, -3690, 0 } },
        { { 3870, -3190, -1600, 0 }, { 3870, -3190, -450, 0 } },
        { { 3870, -3190, 1020, 0 }, { 3870, -3190, 2180, 0 } },
        { { -2000, -3190, -4840, 0 }, { -2000, -3190, -3690, 0 } },
        { { -2000, -3190, -1600, 0 }, { -2000, -3190, -450, 0 } },
        { { -2000, -3190, 1020, 0 }, { -2000, -3190, 2180, 0 } },
        { { -5870, -3190, -4840, 0 }, { -5870, -3190, -3690, 0 } },
        { { -5870, -3190, -1600, 0 }, { -5870, -3190, -450, 0 } },
        { { -5870, -3190, 1020, 0 }, { -5870, -3190, 2180, 0 } },
        { { -7780, -3190, -2000, 0 }, { -8940, -3190, -2000, 0 } },
        { { -7780, -3190, -5000, 0 }, { -8940, -3190, -5000, 0 } },
    },
    .pointCenter = { -7412, -1345, 773, 0 },
};

/// Texture depth, neutral modulation and opaque code flags for parking sprites.
enum {
    MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT = 1,
    MIST_PARKING_SPRITE_NEUTRAL_RGB        = 128,
    MIST_PARKING_SPRITE_OPAQUE             = 0
};

/// CAP command slots and room actions selected by the parking interaction quads.
///
/// Numbered CAP entries identify the default dialogue resource's authored slots.
/// Room-message entries instead invoke the room script or view-9 cutscene.
enum {
    MIST_PARKING_TRIGGER_CAP_COMMAND_5          = 5,
    MIST_PARKING_TRIGGER_CAP_COMMAND_6          = 6,
    MIST_PARKING_TRIGGER_CAP_COMMAND_7          = 7,
    MIST_PARKING_TRIGGER_CAP_COMMAND_9          = 9,
    MIST_PARKING_TRIGGER_CAP_COMMAND_11         = 11,
    MIST_PARKING_TRIGGER_CAP_COMMAND_12         = 12,
    MIST_PARKING_TRIGGER_CAP_COMMAND_21         = 21,
    MIST_PARKING_TRIGGER_DEPARTURE_MENU         = 15,
    MIST_PARKING_TRIGGER_SHOP_TALK              = 18,
    MIST_PARKING_TRIGGER_ROOM_SCRIPT            = 1,
    MIST_PARKING_TRIGGER_ROOM_CUTSCENE          = 8,
    MIST_PARKING_TRIGGER_SELECT_ROOM_2          = 1,
    MIST_PARKING_TRIGGER_SELECT_ROOM_1          = 2,
    MIST_PARKING_TRIGGER_SHOOTING_GALLERY_WARPS = (1 << 4) | 1 // Current-area endpoint 1; destination arrival 1
};

/// Type-1 loaded-bank request bases for paired parking footstep cues.
enum {
    MIST_PARKING_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 17,
    MIST_PARKING_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 19
};

#include "assets/rooms/mist/parking/header.inc"
#include "assets/rooms/mist/parking/geometry.inc"
#include "assets/rooms/mist/parking/cameras.inc"
#include "assets/rooms/mist/parking/sprites.inc"
#include "assets/rooms/mist/parking/view_boundaries.inc"
#include "assets/rooms/mist/parking/action_triggers.inc"
#include "assets/rooms/mist/parking/lights.inc"
#include "assets/rooms/mist/parking/placements.inc"
#include "assets/rooms/mist/parking/ambient.inc"
#include "assets/rooms/mist/parking/surfaces.inc"

s32 gShopRefillDisplayQuantityFixed = 0;

const EquipmentWeaponSupply* gShopRefillSupply = NULL;

Task* gRoomCutsceneSoundTask = NULL;

s32 D_mist_parking_8019531C = 0;

Task* gMistParkingCutsceneModelTask = NULL;

Task* gMistParkingPlayerHeadAimTask = NULL;

MistParkingPrizeAnnouncementState gMistParkingPrizeAnnouncement = { 0 };

MistParkingHeadAimHandle gMistParkingShopPlayerHeadAimHandle = { 0 };

MistParkingShopTalkState gMistParkingShopTalk = { 0, 0, 0, 0 };

RoomCutsceneRec gMistParkingCommandCutscene = { 0 };

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

/// Draws one parking-area row of three capsule glows.
///
/// Borrows three contiguous, word-aligned `worldEndpointPairs`, drawing pairs
/// 0, 1 and 2 in order. A negative GTE flag at either endpoint skips
/// that capsule; the other pairs are still drawn. The signed low halfword of
/// `radiusScale` gives each endpoint's pixel radius as that value times 64
/// divided by camera Z / 4, which must be nonzero for accepted projections.
/// `rgbNibbles` bits 8..11, 4..7 and 0..3 supply RGB channels scaled by 16;
/// odd animation frames set intensity bit 3 in each channel.
///
/// Requires the current view, an initialized scratch stack with room for one
/// `OverlayPointPairScratch`, and a current ordering table and packet arena.
/// Queues up to eighteen additive Gouraud quads plus their blend commands.
/// Retains no endpoint pointer; packets use the frame arena until GPU completion.
static inline void _mistParkingDrawGlowRow(const SVECTOR worldEndpointPairs[3][2], s32 radiusScale, s32 rgbNibbles)
{
    _glowDrawCapsule(worldEndpointPairs[0], radiusScale, rgbNibbles);
    _glowDrawCapsule(worldEndpointPairs[1], radiusScale, rgbNibbles);
    _glowDrawCapsule(worldEndpointPairs[2], radiusScale, rgbNibbles);
}

void mistParkingDrawGlowsTask(Task* unused)
{
    enum {
        MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE       = 0x200, // Pixel radius = scale * 64 / (camera Z / 4)
        MIST_PARKING_GLOW_GREY_RGB_NIBBLES           = 0x444, // Each channel is 64, with bit 3 set on odd animation frames
        MIST_PARKING_GLOW_POINT_PULSE_RATE           = 0x60,  // 4096 angle units per turn, per animation frame
        MIST_PARKING_GLOW_DIAMOND_RADIUS_SCALE       = 0x100, // Pixel half-extent = scale * 32 / (camera Z / 4)
        MIST_PARKING_GLOW_VIEW7_DIAMOND_RADIUS_SCALE = 0xE0,
        MIST_PARKING_GLOW_DISC_RADIUS_SCALE          = 0x40   // Outer pixel radius = scale * 64 / (camera Z / 4)
    };

    u8 mappedViewIndex;

    // Select by mapped camera index; logical views can use a different ordering.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[0], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 18:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[2], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 7:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[6], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[7], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[9], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[10], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawDiamond(&_gMistParkingGlowWorldPoints.pointCenter, MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_VIEW7_DIAMOND_RADIUS_SCALE);
            break;
        case 20:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 9:
            _glowDrawPulsingDisc(&_gMistParkingGlowWorldPoints.pointCenter, MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_DISC_RADIUS_SCALE);
            break;
        case 10:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[2], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 11:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[2], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 14:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[7], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 15:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[0], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 16:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[5], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[8], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[9], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[11], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[12], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 5:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[2], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 6:
        case 19:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[5], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[8], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[11], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[12], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawDiamond(&_gMistParkingGlowWorldPoints.pointCenter, MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_DIAMOND_RADIUS_SCALE);
            break;
        case 8:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
    }
}

#include "../../shared/effects/glow_draw_diamond.inc.c"

#include "../../shared/effects/glow_draw_pulsing_disc.inc.c"

#include "../../shared/effects/glow_draw_capsule.inc.c"
