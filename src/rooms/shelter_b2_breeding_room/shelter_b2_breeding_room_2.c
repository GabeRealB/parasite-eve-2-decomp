#include "rooms/shelter_b2_breeding_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b2_breeding_room_private.h"

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

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the
/// GTE flag word. Defined before `glow_draw.h`, which otherwise selects
/// `GlowCentreScratch`.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../shared/glow_draw.h"

#define D_shelter_b2_breeding_room_80180470 (D_shelter_b2_breeding_room_80180450 + 4)
#define D_shelter_b2_breeding_room_80180480 (D_shelter_b2_breeding_room_80180450 + 6)
#define D_shelter_b2_breeding_room_801804C0 (D_shelter_b2_breeding_room_80180450 + 14)

// Indexed views below share one contiguous table.

SVECTOR D_shelter_b2_breeding_room_80180450[32] = {
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

/// Selects the breeding room's implementations of the three Amoeba effects.
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
            _glowDrawCapsule(&D_shelter_b2_breeding_room_80180470[0], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREEN);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180470[24], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180470[25], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180470[26], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180470[27], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            break;
        case 3:
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[0], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[1], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_breeding_room_80180450[2], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[12], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[13], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[16], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180450[17], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            break;
        case 4:
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[0], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[1], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[4], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[5], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_801804C0[7], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            break;
        case 5:
            _glowDrawCapsule(&D_shelter_b2_breeding_room_80180480[0], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[8], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[9], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[12], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[13], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[14], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[15], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHT_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[16], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[17], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            break;
        case 6:
            _glowDrawCapsule(&D_shelter_b2_breeding_room_80180480[0], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            _glowDrawCapsule(&D_shelter_b2_breeding_room_80180480[2], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            _glowDrawCapsule(&D_shelter_b2_breeding_room_80180480[4], SHELTER_B2_BREEDING_ROOM_GLOW_SMALL_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREEN);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[16], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[17], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_BRIGHTEST_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[18], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[19], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[20], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY);
            glowDrawDisc(&D_shelter_b2_breeding_room_80180480[21], SHELTER_B2_BREEDING_ROOM_GLOW_LARGE_RADIUS_SCALE, SHELTER_B2_BREEDING_ROOM_GLOW_DIM_GREY);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"
