#include "rooms/shelter_b1_sleeping_quarters.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b1_sleeping_quarters_private.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
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
#include "main/task_types.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

/// World-space endpoints of the green beam visible in mapped views 2 and 3.
static SVECTOR _gShelterB1SleepingQuartersGreenBeamEndpoints[2] = {
    { -350, -2200, -580, 0 },
    { -350, -2200, -1440, 0 },
};

/// World-space red disc position visible in mapped views 8 and 9.
///
/// The drawer borrows only the centre; the following eight zero bytes have
/// no established consumer or role.
static struct {
    SVECTOR centre;       // Red disc centre in signed world-coordinate units.
    byte    unknown_8[8]; // Retained zero bytes; role unproven.
} _gShelterB1SleepingQuartersRedDiscPosition = {
    { 11800, -2350, 6330, 0 },
    { 0 },
};

/// World-space endpoints of eight white beams shared across mapped views.
///
/// Consecutive pairs form beams. Views 3, 4, 5, 7 and 8 select subsets;
/// every draw borrows two signed world-coordinate points within this table.
static SVECTOR _gShelterB1SleepingQuartersWhiteBeamEndpoints[16] = {
    { 1760, -2930, 4850, 0 },
    { 2600, -2930, 4850, 0 },
    { 1760, -2930, 2130, 0 },
    { 2600, -2930, 2130, 0 },
    { 1760, -2930, -1610, 0 },
    { 2600, -2930, -1610, 0 },
    { 4910, -2930, 4710, 0 },
    { 4910, -2930, 3880, 0 },
    { 8680, -2930, 4020, 0 },
    { 8680, -2930, 4850, 0 },
    { 0x2C24, -2930, 4850, 0 },
    { 0x2C24, -2930, 4030, 0 },
    { 7150, -2290, 4950, 0 },
    { 7840, -2290, 4950, 0 },
    { 7150, -2290, 3350, 0 },
    { 7840, -2290, 3350, 0 },
};

/// World-space endpoints of two white beams visible in mapped view 6.
///
/// Consecutive pairs form beams; positions are borrowed only during drawing.
static SVECTOR _gShelterB1SleepingQuartersView6BeamEndpoints[4] = {
    { 5080, -1880, -50, 0 },
    { 5080, -1880, 350, 0 },
    { 6420, -1880, -50, 0 },
    { 6420, -1880, 350, 0 },
};

/// World-space endpoints of four white beams visible in mapped view 10.
///
/// Consecutive pairs form beams; positions are borrowed only during drawing.
static SVECTOR _gShelterB1SleepingQuartersView10BeamEndpoints[8] = {
    { 8130, -2660, -30, 0 },
    { 7740, -2660, -30, 0 },
    { 9540, -2660, -30, 0 },
    { 9080, -2660, -30, 0 },
    { 0x2AC6, -2660, -30, 0 },
    { 0x2904, -2660, -30, 0 },
    { 0x3048, -2660, -30, 0 },
    { 0x2E7C, -2660, -30, 0 },
};

static void _glowDrawBeam(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

/// Draws two flickering additive beams with a shared radius scale, screen angle and tint.
///
/// `beamEndpoints` supplies four word-aligned world-space points, borrowed for
/// the call: points 0..1 form the first beam and points 2..3 form the second.
/// Each beam is skipped if its second end's depth is below 17; its first end's
/// depth is clamped to 16. Depth is camera Z / 4, and each end's pixel radius
/// is `radiusScale * 64 / depth`. `screenAngle` sets the cap and side orientation
/// in screen space, in 4096 units per turn with zero pointing down.
/// `colorFactors` packs a signed red multiplier in bits 8..15 and one-bit green
/// and blue multipliers in bits 4 and 0; the other bits are ignored. These
/// multiply an intensity of 32 or 40 on alternating frames; colour bytes wrap.
///
/// Requires the current view matrix, an initialized scratch stack with room for
/// one `GlowPointPairScratch`, and a current ordering table and packet arena.
/// Each accepted beam queues six Gouraud quads plus additive blend commands;
/// queued packet storage must remain live until GPU completion.
static inline void _shelterB1SleepingQuartersDrawBeamPair(const SVECTOR beamEndpoints[4], s16 radiusScale, s16 screenAngle, u16 colorFactors)
{
    _glowDrawBeam(&beamEndpoints[0], radiusScale, screenAngle, colorFactors);
    _glowDrawBeam(&beamEndpoints[2], radiusScale, screenAngle, colorFactors);
}

void shelterB1SleepingQuartersDrawViewLightsTask(Task* task)
{
    enum {
        LIGHTS_INITIALIZE,
        LIGHTS_DRAW,
        // Pixel radius is scale * 64 / (camera Z / 4).
        BEAM_RADIUS_SCALE = 0x200,
        DISC_RADIUS_SCALE = 0x300,
        // Red factor in bits 8..15; green and blue factors in bits 4 and 0.
        LIGHT_WHITE     = 0x111,
        LIGHT_GREEN     = 0x010,
        LIGHT_RED       = 0x100,
        VIEW_INDEX_MASK = 0xFF,
    };

    // Select the room's particle effects before drawing its persistent lights.
    if (task->state == LIGHTS_INITIALIZE) {
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_SLEEPING_QUARTERS_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_SLEEPING_QUARTERS_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_SLEEPING_QUARTERS_ORANGE_BURST_2;
        task->state               = LIGHTS_DRAW;
    }

    // Camera views select world-space endpoint pairs and a separate disc centre.
    // Views 4, 5 and 7 share endpoint pairs from the same table.
    switch (viewGetMappedIndex() & VIEW_INDEX_MASK) {
        case 3:
            _glowDrawBeam(&_gShelterB1SleepingQuartersWhiteBeamEndpoints[4], BEAM_RADIUS_SCALE, 0, LIGHT_WHITE);
            // Fall through: view 3 also contains view 2's green beam.
        case 2:
            _glowDrawBeam(_gShelterB1SleepingQuartersGreenBeamEndpoints, BEAM_RADIUS_SCALE, 0, LIGHT_GREEN);
            break;
        case 4: {
            const SVECTOR* lightPoints;
            lightPoints = _gShelterB1SleepingQuartersWhiteBeamEndpoints;
            _shelterB1SleepingQuartersDrawBeamPair(lightPoints, BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[6], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, LIGHT_WHITE);
            break;
        }
        case 5: {
            const SVECTOR* lightPoints;
            lightPoints = &_gShelterB1SleepingQuartersWhiteBeamEndpoints[6];
            _glowDrawBeam(&lightPoints[0], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[6], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[8], BEAM_RADIUS_SCALE, 0, LIGHT_WHITE);
            break;
        }
        case 6: {
            const SVECTOR* lightPoints;
            lightPoints = _gShelterB1SleepingQuartersView6BeamEndpoints;
            _shelterB1SleepingQuartersDrawBeamPair(lightPoints, BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, LIGHT_WHITE);
            break;
        }
        case 7: {
            const SVECTOR* lightPoints;
            lightPoints = _gShelterB1SleepingQuartersWhiteBeamEndpoints;
            _glowDrawBeam(&lightPoints[0], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[2], BEAM_RADIUS_SCALE, 0, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[8], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[12], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[14], BEAM_RADIUS_SCALE, 0, LIGHT_WHITE);
            break;
        }
        case 8:
            _glowDrawBeam(&_gShelterB1SleepingQuartersWhiteBeamEndpoints[10], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            // Fall through: view 8 also contains view 9's red glow disc.
        case 9:
            _glowDrawBitDisc(&_gShelterB1SleepingQuartersRedDiscPosition.centre, DISC_RADIUS_SCALE, LIGHT_RED);
            break;
        case 10: {
            const SVECTOR* lightPoints;
            lightPoints = _gShelterB1SleepingQuartersView10BeamEndpoints;
            _shelterB1SleepingQuartersDrawBeamPair(&lightPoints[0], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _shelterB1SleepingQuartersDrawBeamPair(&lightPoints[4], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_beam.inc.c"

#include "../../shared/effects/glow_draw_bit_disc.inc.c"
