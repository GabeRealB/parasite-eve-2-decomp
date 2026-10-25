#include "rooms/shelter_b2_breeding_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b2_breeding_room_private.h"

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

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the
/// GTE flag word. Defined before `glow_draw.h`, which otherwise selects
/// `GlowCentreScratch`.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../shared/effects/glow_draw.h"

/// World-space glow centres and capsule endpoints selected by mapped views 2..6.
///
/// Points 0..1 and 12..31 are disc centres; consecutive pairs in 2..11 form
/// capsules. All 32 signed world-coordinate points are borrowed only while drawing.
static SVECTOR _gShelterB2BreedingRoomGlowPoints[32] = {
    { 3450, -2450, -2000, 0 },
    { 3000, -2450, -2620, 0 },
    { 880, -2450, -2740, 0 },
    { 2410, -2450, -2740, 0 },
    { -260, -2240, -1360, 0 },
    { -260, -2240, -660, 0 },
    { 7470, -2160, 5560, 0 },
    { 8200, -2160, 5560, 0 },
    { 8990, -2160, 5560, 0 },
    { 9720, -2160, 5560, 0 },
    { 12130, -1430, 6540, 0 },
    { 12480, -1430, 6540, 0 },
    { 1410, -2350, 90, 0 },
    { 1410, -2350, 1890, 0 },
    { 1410, -2350, 3140, 0 },
    { 1410, -2350, 4760, 0 },
    { 2650, -2350, 90, 0 },
    { 2650, -2350, 1890, 0 },
    { 2650, -2350, 3140, 0 },
    { 2650, -2350, 4760, 0 },
    { 4340, -2350, 3830, 0 },
    { 4340, -2350, 5060, 0 },
    { 6780, -2350, 3830, 0 },
    { 6780, -2350, 5060, 0 },
    { 9190, -2350, 3830, 0 },
    { 9190, -2350, 5060, 0 },
    { 11510, -2350, 3830, 0 },
    { 11510, -2350, 5060, 0 },
    { 1210, -2350, -1890, 0 },
    { 2450, -2350, -1890, 0 },
    { 1400, -2350, 620, 0 },
    { 2650, -2350, 620, 0 },
};

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Installs this room's effect IDs for the Amoeba's charge glow, sparks and projectile burst.
///
/// The charge glow emits sparks from player joints; the burst marks the
/// projectile's end. Call after the room-effect controller initializes its
/// slots and before these effects are spawned. The selected bank-6 IDs remain
/// installed until another room overwrites them or the controller resets them.
/// Their task handlers require this room's overlay to remain loaded.
static inline void _shelterB2BreedingRoomInstallAmoebaEffects(void)
{
    gRoomEffectGlowDiscId     = EFFECT_SHELTER_B2_BREEDING_ROOM_GLOW_DISC;
    gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B2_BREEDING_ROOM_FLYING_SPARK;
    gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B2_BREEDING_ROOM_ORANGE_BURST_2;
}

void shelterB2BreedingRoomDrawGlowsTask(Task* task)
{
    enum {
        SHELTER_B2_BREEDING_ROOM_GLOW_STATE_INIT      = 0,
        SHELTER_B2_BREEDING_ROOM_GLOW_STATE_DRAW      = 1,
        SHELTER_B2_BREEDING_ROOM_GLOW_VIEW_INDEX_MASK = 0xFF,
        // Pixel radius = scale * 64 / (camera Z / 4).
        SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE = 0x100,
        SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE = 0x200,
        // Packed RGB nibbles; each channel is multiplied by 16 before flicker.
        SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY       = 0x111,
        SHELTER_B2_BREEDING_ROOM_GLOW_GREY           = 0x222,
        SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY    = 0x333,
        SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY = 0x444,
        SHELTER_B2_BREEDING_ROOM_GLOW_GREEN          = 0x142,
    };

    if (task->state == SHELTER_B2_BREEDING_ROOM_GLOW_STATE_INIT) {
        _shelterB2BreedingRoomInstallAmoebaEffects();
        task->state = SHELTER_B2_BREEDING_ROOM_GLOW_STATE_DRAW;
    }

    // Select fixed world points by mapped camera index, rather than logical view.
    switch (viewGetMappedIndex() & SHELTER_B2_BREEDING_ROOM_GLOW_VIEW_INDEX_MASK) {
        case 2:
            _glowDrawCapsule(&_gShelterB2BreedingRoomGlowPoints[4], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREEN);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[28], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[29], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[30], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[31], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            break;
        case 3:
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[0], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[1], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            _glowDrawCapsule(&_gShelterB2BreedingRoomGlowPoints[2], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[12], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[13], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[16], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[17], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            break;
        case 4:
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[14], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[15], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[18], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[19], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[21], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            break;
        case 5:
            _glowDrawCapsule(&_gShelterB2BreedingRoomGlowPoints[6], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[14], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[15], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[18], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[19], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[20], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[21], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[22], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[23], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            break;
        case 6:
            _glowDrawCapsule(&_gShelterB2BreedingRoomGlowPoints[6], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            _glowDrawCapsule(&_gShelterB2BreedingRoomGlowPoints[8], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            _glowDrawCapsule(&_gShelterB2BreedingRoomGlowPoints[10], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREEN);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[22], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[23], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[24], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[25], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[26], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY);
            glowDrawDisc(&_gShelterB2BreedingRoomGlowPoints[27], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY);
            break;
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/glow_draw_disc.inc.c"
