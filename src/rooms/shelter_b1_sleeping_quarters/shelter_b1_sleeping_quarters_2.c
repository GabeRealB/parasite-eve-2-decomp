#include "rooms/shelter_b1_sleeping_quarters.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b1_sleeping_quarters_private.h"

#include "actors/task_tables.h"

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
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

extern SVECTOR D_shelter_b1_sleeping_quarters_8018054C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018055C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018056C[];
extern SVECTOR D_shelter_b1_sleeping_quarters_801805EC[];
extern SVECTOR D_shelter_b1_sleeping_quarters_8018060C[];

// Existing slice names select endpoints within one shared table, in SVECTOR elements.
#define D_shelter_b1_sleeping_quarters_8018058C (D_shelter_b1_sleeping_quarters_8018056C + 4)
#define D_shelter_b1_sleeping_quarters_8018059C (D_shelter_b1_sleeping_quarters_8018056C + 6)
#define D_shelter_b1_sleeping_quarters_801805BC (D_shelter_b1_sleeping_quarters_8018056C + 10)

SVECTOR D_shelter_b1_sleeping_quarters_8018054C[2] = {
    { -350, -2200, -580, 0 },
    { -350, -2200, -1440, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_8018055C[2] = {
    { 0x2E18, -2350, 6330, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_8018056C[16] = {
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

SVECTOR D_shelter_b1_sleeping_quarters_801805EC[4] = {
    { 5080, -1880, -50, 0 },
    { 5080, -1880, 350, 0 },
    { 6420, -1880, -50, 0 },
    { 6420, -1880, 350, 0 },
};

SVECTOR D_shelter_b1_sleeping_quarters_8018060C[8] = {
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

/// Draws two adjacent beams with the same scale, screen angle and colour.
///
/// Borrows four world points as two consecutive endpoint pairs. The arguments
/// use the fixed-angle beam drawer's radius, 4096-unit turn and colour-factor units.
static inline void _shelterB1SleepingQuartersDrawBeamPair(const SVECTOR lightPoints[4], s32 radiusScale, s32 startAngle, s32 packedColor)
{
    _glowDrawBeam(&lightPoints[0], radiusScale, startAngle, packedColor);
    _glowDrawBeam(&lightPoints[2], radiusScale, startAngle, packedColor);
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
            _glowDrawBeam(D_shelter_b1_sleeping_quarters_8018058C, BEAM_RADIUS_SCALE, 0, LIGHT_WHITE);
            // Fall through: view 3 also contains view 2's green beam.
        case 2:
            _glowDrawBeam(D_shelter_b1_sleeping_quarters_8018054C, BEAM_RADIUS_SCALE, 0, LIGHT_GREEN);
            break;
        case 4: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_sleeping_quarters_8018056C;
            _shelterB1SleepingQuartersDrawBeamPair(lightPoints, BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[6], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, LIGHT_WHITE);
            break;
        }
        case 5: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_sleeping_quarters_8018059C;
            _glowDrawBeam(&lightPoints[0], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[6], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[8], BEAM_RADIUS_SCALE, 0, LIGHT_WHITE);
            break;
        }
        case 6: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_sleeping_quarters_801805EC;
            _shelterB1SleepingQuartersDrawBeamPair(lightPoints, BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, LIGHT_WHITE);
            break;
        }
        case 7: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_sleeping_quarters_8018056C;
            _glowDrawBeam(&lightPoints[0], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[2], BEAM_RADIUS_SCALE, 0, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[8], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[12], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _glowDrawBeam(&lightPoints[14], BEAM_RADIUS_SCALE, 0, LIGHT_WHITE);
            break;
        }
        case 8:
            _glowDrawBeam(D_shelter_b1_sleeping_quarters_801805BC, BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            // Fall through: view 8 also contains view 9's red glow disc.
        case 9:
            _glowDrawBitDisc(D_shelter_b1_sleeping_quarters_8018055C, DISC_RADIUS_SCALE, LIGHT_RED);
            break;
        case 10: {
            const SVECTOR* lightPoints;
            lightPoints = D_shelter_b1_sleeping_quarters_8018060C;
            _shelterB1SleepingQuartersDrawBeamPair(&lightPoints[0], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            _shelterB1SleepingQuartersDrawBeamPair(&lightPoints[4], BEAM_RADIUS_SCALE, GLOW_HALF_TURN, LIGHT_WHITE);
            break;
        }
    }
}

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_bit_disc.inc.c"
