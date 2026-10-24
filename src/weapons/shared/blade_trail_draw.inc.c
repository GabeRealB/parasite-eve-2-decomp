/* Part of the blade trail library; see blade_trail.h. */

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"
#include "types.h"

#include "gameplay/room_effects.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/scratch.h"

#include "blade_trail.h"

// The carrier defines the two endpoint rings, `static GfxCoord _gBladeTrailFirstHistory[8]` and
// `_gBladeTrailSecondHistory[8]`, and declares them before this inclusion. The first ring supplies corners
// 0 and 2; the second supplies 1 and 3.
// Slots are paired samples, newest at age & 7, seeded before drawing and
// recomposed into the current view. The drawer reads only workm.t, narrowing
// game-coordinate units to signed 16 bits; it retains no coordinate pointers.

STATIC_ASSERT(ARRAY_SIZE(_gBladeTrailFirstHistory) == 8, blade_trail_first_endpoint_has_eight_samples);
STATIC_ASSERT(ARRAY_SIZE(_gBladeTrailFirstHistory) == ARRAY_SIZE(_gBladeTrailSecondHistory), blade_trail_endpoint_histories_are_paired);

/// Colours the newer and older edges of a blade-trail quad with one packed tint.
///
/// `quad` borrows one writable `POLY_G4`: vertices 0/1 are the newer first/second
/// endpoints and vertices 2/3 the older first/second endpoints. Only their RGB
/// bytes are written.
/// Brightness arguments are integer factors in 0..255. Tint multipliers occupy
/// bits 8..9 (red), 4..5 (green) and 0..1 (blue), with other bits zero.
/// Red uses the complete signed high part of `packedTint`, without masking.
/// Each product is narrowed to its low eight bits without saturation.
static inline void _bladeTrailTintQuad(POLY_G4* quad, s32 newerBrightness, s32 olderBrightness, s16 packedTint)
{
    enum {
        BLADE_TRAIL_TINT_RED_SHIFT    = 8,
        BLADE_TRAIL_TINT_GREEN_SHIFT  = 4,
        BLADE_TRAIL_TINT_CHANNEL_MASK = 3
    };
    s32 redMultiplier;
    s32 greenMultiplier;
    s32 blueMultiplier;

    redMultiplier   = packedTint >> BLADE_TRAIL_TINT_RED_SHIFT;
    greenMultiplier = (packedTint >> BLADE_TRAIL_TINT_GREEN_SHIFT) & BLADE_TRAIL_TINT_CHANNEL_MASK;
    blueMultiplier  = packedTint & BLADE_TRAIL_TINT_CHANNEL_MASK;
    setRGB0(quad, newerBrightness * redMultiplier, newerBrightness * greenMultiplier, newerBrightness * blueMultiplier);
    setRGB1(quad, newerBrightness * redMultiplier, newerBrightness * greenMultiplier, newerBrightness * blueMultiplier);
    setRGB2(quad, olderBrightness * redMultiplier, olderBrightness * greenMultiplier, olderBrightness * blueMultiplier);
    setRGB3(quad, olderBrightness * redMultiplier, olderBrightness * greenMultiplier, olderBrightness * blueMultiplier);
}

/// Queues a fading additive ribbon through eight recorded blade poses.
///
/// `newestSlot` identifies the newest endpoint pair in 0..7; indexing wraps
/// modulo eight. Both rings must have initialized, composed `workm` view-space
/// translations in coordinate units. Their low signed 16 bits are projected
/// through `GsWSMATRIX`. The coordinates are borrowed read-only for this call.
/// `packedTint` supplies RGB multipliers in 0..3 at bits 8, 4 and 0, with all
/// other bits zero. Red uses a signed shift without masking the high part.
///
/// The seven quads fade from brightness 64 to 1 in steps of 9. Only the GTE
/// FLAG from the last three corners rejects a quad; corner 0's flags are
/// overwritten. Sorting and additive blend commands use the older second
/// endpoint's SZ3 / 4, scaled and wrapped by the current ordering-table configuration.
///
/// Requires an initialized scratch stack with one free `BladeTrailScratch`
/// block and a word-aligned frame arena with room for seven `POLY_G4` packets
/// and up to seven `DR_TPAGE` commands. A quad consumes arena space even when
/// rejected. Scratch is released before return; queued packets live until GPU
/// drawing finishes. Overwrites the GTE matrix and projection registers.
static void _bladeTrailDraw(s16 newestSlot, s16 packedTint)
{
    enum {
        BLADE_TRAIL_INITIAL_BRIGHTNESS   = 64,
        BLADE_TRAIL_FADE_STEP            = 9,
        BLADE_TRAIL_BRIGHTNESS_BYTE_MASK = 0xFF
    };
    BladeTrailScratch* scratch;
    const GfxCoord*    firstEndpointCoord;
    const GfxCoord*    secondEndpointCoord;
    POLY_G4*           quad;
    s32                segment;
    s32                unwrappedSlot;
    s32                newerSlot;
    s32                olderSlot;
    s32                newerBrightness;
    s32                olderBrightness;
    s32                brightness;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(BladeTrailScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segment = 0; segment < ARRAY_SIZE(_gBladeTrailFirstHistory) - 1; segment++) {
        // Join successive recorded poses, retaining the low signed coordinate halves.
        unwrappedSlot               = newestSlot - segment;
        newerSlot                   = unwrappedSlot & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1);
        olderSlot                   = (unwrappedSlot - 1) & (ARRAY_SIZE(_gBladeTrailSecondHistory) - 1);
        firstEndpointCoord          = &_gBladeTrailFirstHistory[newerSlot];
        scratch->worldCorners[0].vx = firstEndpointCoord->workm.t[0];
        scratch->worldCorners[0].vy = firstEndpointCoord->workm.t[1];
        secondEndpointCoord         = &_gBladeTrailSecondHistory[newerSlot];
        scratch->worldCorners[0].vz = firstEndpointCoord->workm.t[2];
        scratch->worldCorners[1].vx = secondEndpointCoord->workm.t[0];
        scratch->worldCorners[1].vy = secondEndpointCoord->workm.t[1];
        firstEndpointCoord          = &_gBladeTrailFirstHistory[olderSlot];
        scratch->worldCorners[1].vz = secondEndpointCoord->workm.t[2];
        scratch->worldCorners[2].vx = firstEndpointCoord->workm.t[0];
        scratch->worldCorners[2].vy = firstEndpointCoord->workm.t[1];
        secondEndpointCoord         = &_gBladeTrailSecondHistory[olderSlot];
        scratch->worldCorners[2].vz = firstEndpointCoord->workm.t[2];
        scratch->worldCorners[3].vx = secondEndpointCoord->workm.t[0];
        scratch->worldCorners[3].vy = secondEndpointCoord->workm.t[1];
        scratch->worldCorners[3].vz = secondEndpointCoord->workm.t[2];
        // Corner 0 is projected alone. The flag word belongs to the transform of the other three.
        gte_ldv0(&scratch->worldCorners[0]);
        gte_rtps();
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        gte_stsxy(&quad->x0);
        gte_ldv3(&scratch->worldCorners[1], &scratch->worldCorners[2], &scratch->worldCorners[3]);
        gte_rtpt();
        gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            // Fade each edge and queue its additive mode at the older second endpoint's depth.
            gte_stszotz(&scratch->otz);
            brightness      = BLADE_TRAIL_INITIAL_BRIGHTNESS - segment * BLADE_TRAIL_FADE_STEP;
            newerBrightness = brightness & BLADE_TRAIL_BRIGHTNESS_BYTE_MASK;
            olderBrightness = (brightness - BLADE_TRAIL_FADE_STEP) & BLADE_TRAIL_BRIGHTNESS_BYTE_MASK;
            _bladeTrailTintQuad(quad, newerBrightness, olderBrightness, packedTint);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(BladeTrailScratch);
}
