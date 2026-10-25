#include "stages/akropolis/rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "mist_parking_private.h"

#include "gameplay/inventory.h"
#include "gameplay/room_effects.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../../../shared/effects/glow_draw.h"
#include "../../../../shared/room/room_cutscene.h"

#define ROOM_PART declarations_mist_parking_glows
#include "assets/rooms/mist/parking/declarations.inc"

/// World-coordinate capsule endpoints and the separate pulsing point-glow center.
typedef struct {
    SVECTOR capsuleEndpoints[13][2]; // Thirteen capsule pairs; pair 1 is retained but unused
    SVECTOR pointCenter;             // Centre shared by the cyan diamond and disc glows
} _GlowWorldPoints;
STATIC_ASSERT_SIZEOF(_GlowWorldPoints, 0xD8);

/// Scene target, MIST stock profile and script delays for the variant-1 shop poses.
enum {
    JODIE_SCENE_PLACEMENT   = 1,
    SHOP_STOCK_PROFILE      = 0x10,
    SHOP_POSE_DELAY_UPDATES = 20,
    SHOP_REPLY_END_UPDATES  = 30
};

#define ROOM_PART gMistParkingJodieShopTalkScript
#include "assets/rooms/mist/parking/events.inc"
/// Authored parking glow positions in signed world-coordinate units.
static _GlowWorldPoints _gMistParkingGlowWorldPoints = {
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

s32 gMistParkingDialogChoice = 0;

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
static inline void _mistParkingDrawGlowRow(const SVECTOR worldEndpointPairs[3][2], s32 radiusScale, s32 rgbNibbles)
{
    _glowDrawCapsule(worldEndpointPairs[0], radiusScale, rgbNibbles);
    _glowDrawCapsule(worldEndpointPairs[1], radiusScale, rgbNibbles);
    _glowDrawCapsule(worldEndpointPairs[2], radiusScale, rgbNibbles);
}

void mistParkingDrawGlowsTask(Task* unused)
{
    enum {
        CAPSULE_RADIUS_SCALE       = 0x200, // Pixel radius = scale * 64 / (camera Z / 4)
        GREY_RGB_NIBBLES           = 0x444, // Each channel is 64, with bit 3 set on odd animation frames
        POINT_PULSE_RATE           = 0x60,  // 4096 angle units per turn, per animation frame
        DIAMOND_RADIUS_SCALE       = 0x100, // Pixel half-extent = scale * 32 / (camera Z / 4)
        VIEW7_DIAMOND_RADIUS_SCALE = 0xE0,
        DISC_RADIUS_SCALE          = 0x40   // Outer pixel radius = scale * 64 / (camera Z / 4)
    };

    u8 mappedViewIndex;

    // Select by mapped camera index; logical views can use a different ordering.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[0], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 18:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[2], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 7:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[6], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[7], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[9], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[10], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawDiamond(&_gMistParkingGlowWorldPoints.pointCenter, POINT_PULSE_RATE, VIEW7_DIAMOND_RADIUS_SCALE);
            break;
        case 20:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 9:
            _glowDrawPulsingDisc(&_gMistParkingGlowWorldPoints.pointCenter, POINT_PULSE_RATE, DISC_RADIUS_SCALE);
            break;
        case 10:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[2], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 11:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[2], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 14:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[7], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 15:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[0], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 16:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[5], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[8], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[9], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[11], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[12], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 5:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[2], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
        case 6:
        case 19:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[5], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[8], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[11], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[12], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawDiamond(&_gMistParkingGlowWorldPoints.pointCenter, POINT_PULSE_RATE, DIAMOND_RADIUS_SCALE);
            break;
        case 8:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], CAPSULE_RADIUS_SCALE, GREY_RGB_NIBBLES);
            break;
    }
}

#include "../../../../shared/effects/glow_draw_diamond.inc.c"

#include "../../../../shared/effects/glow_draw_pulsing_disc.inc.c"

#include "../../../../shared/effects/glow_draw_capsule.inc.c"
