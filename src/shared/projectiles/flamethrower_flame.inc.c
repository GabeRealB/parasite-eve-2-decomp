/* The Pyke flame library; see flamethrower_flame.h. The carrier exports `_flamethrowerFlameTask` under its own name with
   DEFINE_ALIAS. */

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_collision_types.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/task_types.h"

#include "flamethrower_flame.h"

#ifndef FLAMETHROWER_FLAME_REDRAW_UPDATES_COORD
/// Selects whether nonrunning flame redraws refresh the composed transform.
///
/// A carrier may define this preprocessor integer to 0 or 1 before including
/// the fragment. The default 0 (M4A1 Pyke) draws from the existing `workm`;
/// 1 composes the coordinate through its parents before drawing. This applies
/// to every nonzero `effectControl` below `ROOM_EFFECT_CONTROL_CANCEL_MIN`,
/// including `ROOM_EFFECT_CONTROL_HIDDEN`. Neither policy advances the local
/// transform, age, size or spin. The fragment undefines the binding at its end.
#define FLAMETHROWER_FLAME_REDRAW_UPDATES_COORD 0
#endif

#ifndef FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE
/// Selects the splash depth increment's position relative to the GTE store.
///
/// Preprocessor integer 0 or 1, supplied before this fragment is included.
/// The default 0 (actor_800100) increments the stored `SZ3 >> 2` depth by one,
/// before ordering-table quantization. Value 1 (m4a1_pyke) retains an increment
/// of the uninitialized stack word before the GTE store overwrites it; the
/// queued depth is then unbiased. The fragment undefines the binding at its end.
#define FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE 0
#elif (FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE != 0) && (FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE != 1)
#error "FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE must be 0 or 1"
#endif

/// Draws the animated, additive flame at the weapon nozzle as a billboard.
///
/// Borrows the three s32 world coordinates for this call, narrowing to s16.
/// `animationFrame` wraps modulo six 32-texel cells. `sizeScale` controls the
/// screen half-side in pixels as sizeScale * 31 / (SZ3 / 4 + 1); it does not
/// modulate colour. A negative GTE FLAG rejects the projection. Requires space
/// for one scratch block and one POLY_FT4 in the frame arena; the packet must
/// remain live through GPU drawing.
static void _flamethrowerFlameDrawNozzle(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale)
{
    enum { FRAME_COUNT = 6,
           CELL_SHIFT  = 5,
           UV_SPAN     = (1 << CELL_SHIFT) - 1,
           TEXTURE_TOP = 0x98 };

    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             projectionPoint;
    s16                  screenX;
    s16                  screenY;
    u16                  textureFrameIndex;
    s32                  textureLeft;
    s32                  textureRight;
    u16                  worldZ;

    // Project the nozzle centre, then size the square from its biased depth.
    scratchEnd                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (scratchEnd - 1)->worldPoint.vx           = (u16)worldPosition->vx;
    scratch                                   = scratchEnd - 1;
    scratch->worldPoint.vy                    = (u16)worldPosition->vy;
    worldZ                                    = (u16)worldPosition->vz;
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZ;
    projectionPoint                           = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(projectionPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        scratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
        quad->clut  = getClut(208, 268);
        quad->v0    = TEXTURE_TOP;
        quad->v1    = TEXTURE_TOP;
        quad->v2    = TEXTURE_TOP + UV_SPAN;
        quad->v3    = TEXTURE_TOP + UV_SPAN;
        // Keep the wrapped frame in a halfword before forming byte UVs.
        textureFrameIndex     = animationFrame % FRAME_COUNT;
        textureLeft           = textureFrameIndex << CELL_SHIFT;
        textureRight          = textureLeft + UV_SPAN;
        quad->u0              = textureLeft;
        quad->u1              = textureRight;
        quad->u2              = textureLeft;
        quad->u3              = textureRight;
        scratch->screenExtent = (sizeScale * UV_SPAN) / scratch->depth;
        screenX               = scratch->screenX - (u16)scratch->screenExtent;
        quad->x2              = screenX;
        quad->x0              = screenX;
        screenX               = scratch->screenX + (u16)scratch->screenExtent;
        quad->x3              = screenX;
        quad->x1              = screenX;
        screenY               = scratch->screenY - (u16)scratch->screenExtent;
        quad->y1              = screenY;
        quad->y0              = screenY;
        screenY               = scratch->screenY + (u16)scratch->screenExtent;
        quad->y3              = screenY;
        quad->y2              = screenY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Advances a Pyke flame by one parent-frame displacement and refreshes its composed transform.
///
/// `flameWork->move` supplies signed 16-bit XYZ displacements in integer game
/// units for one running tick, already in `flameCoord`'s parent frame. Both
/// pointers must be non-NULL and remain live for this call; neither is retained.
/// The coordinate and its ancestors must stay live and writable in an acyclic
/// parent chain. Local rotation is preserved; `workm` is composed through the complete
/// chain for drawing and collision, clobbering the GTE working registers.
static inline void _flamethrowerFlameAdvanceCoord(GfxCoord* flameCoord, const EffectWork* flameWork)
{
    flameCoord->coord.t[0]  += flameWork->move.vx;
    flameCoord->coord.t[1]  += flameWork->move.vy;
    flameCoord->coord.t[2]  += flameWork->move.vz;
    flameCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(flameCoord);
}

/// Updates one Pyke flame's colliding flight and spreading drift after a geometry hit.
///
/// Requires a counted effect with an owned coordinate body and an `EffectWork`
/// in `spawnArg2.pointer`, initially age/state zero and work NULL. The low u16
/// of `spawnArg1.value` supplies launch speed in game units per running tick
/// (the carriers pass 0x40..0x180) and the initial size numerator before a 0x180
/// bias. Launch follows the coordinate's rotated positive Y axis, with 0..63
/// units of speed jitter. `move` then holds signed-halfword displacements in
/// the coordinate's parent frame; `scale` sizes the billboard and `angle` uses
/// 4096 units per turn. The collision radius is fixed at half the initial size.
///
/// Owns a primary-heap `FlamethrowerFlameBody` in `work` after initialization. An enemy-body
/// contact ends flight; a geometry hit unlinks its sphere and starts drift
/// without resetting age. Expiry is checked at age 21, after drawing; the tick
/// that enters drift returns before that check. Allocation failure retries
/// initialization with age zero. Nonzero control below cancellation redraws
/// without aging; `FLAMETHROWER_FLAME_REDRAW_UPDATES_COORD` selects cache refresh then.
/// Cancellation or expiry releases the task, coordinate body and owned work blocks.
static void _flamethrowerFlameTask(Task* task)
{
    enum {
        FLAMETHROWER_FLAME_STATE_INIT            = 0,
        FLAMETHROWER_FLAME_STATE_FLIGHT          = 1,
        FLAMETHROWER_FLAME_STATE_DRIFT           = 2,
        FLAMETHROWER_FLAME_LIFETIME_TICKS        = 21,
        FLAMETHROWER_FLAME_INITIAL_SIZE_BIAS     = 0x180,
        FLAMETHROWER_FLAME_FLIGHT_SIZE_STEP      = 0x10,
        FLAMETHROWER_FLAME_DRIFT_SIZE_STEP       = 0x40,
        FLAMETHROWER_FLAME_FLIGHT_Y_ACCELERATION = 8,
        FLAMETHROWER_FLAME_LAUNCH_JITTER_MASK    = 0x3F,
        FLAMETHROWER_FLAME_SPIN_ANGLE_MASK       = ONE - 1,
        FLAMETHROWER_FLAME_DRIFT_TRIG_SHIFT      = 8
    };

    GfxCoord               groundCoord;
    SVECTOR                flightEnd;
    SVECTOR                flightStart;
    GfxCoord*              coord;
    EffectWork*            work;
    FlamethrowerFlameBody* flame;
    s32                    effectControl;
    u32                    launchRandom;
    u32                    spinRandom;
    u32                    splashRandom;
    u32                    driftRandom;

    flame         = task->work;
    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (task->state != FLAMETHROWER_FLAME_STATE_INIT) {
            worldCollisionUnlinkBody(&flame->body);
        }
        effectKillTask(work, task);
        return;
    }
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
#if FLAMETHROWER_FLAME_REDRAW_UPDATES_COORD
        actorRenderComposeCoord(coord);
#endif
        _flamethrowerFlameDrawBlob(GRAPHICS_MATRIX_TRANSLATION(&coord->workm),
                                   (work->age >> 1) + 1, work->scale, work->angle);
        return;
    }
    work->age = work->age + 1;
    switch (task->state) {
        case FLAMETHROWER_FLAME_STATE_INIT:
            flame = memCalloc(sizeof(*flame), false);
            if (flame == NULL) {
                work->age = 0;
                return;
            }
            task->exitCallback = _flamethrowerFlameRelease;
            // Rotate the launch displacement into the coordinate's parent frame.
            work->move.vx   = 0;
            launchRandom    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = launchRandom;
            work->move.vy   = (u16)task->spawnArg1.value - ((launchRandom >> 16) & FLAMETHROWER_FLAME_LAUNCH_JITTER_MASK);
            work->move.vz   = 0;
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            work->scale                  = (u16)task->spawnArg1.value + FLAMETHROWER_FLAME_INITIAL_SIZE_BIAS;
            spinRandom                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle                  = (spinRandom >> 16) & FLAMETHROWER_FLAME_SPIN_ANGLE_MASK;
            task->state                  = FLAMETHROWER_FLAME_STATE_FLIGHT;
            task->work                   = flame;
            flame->body.coord            = coord;
            flame->body.context.contacts = flame->contacts;
            flame->body.key              = FLAMETHROWER_FLAME_ATTACK_KEY;
            flame->body.radius           = work->scale >> 1;
            gRandomLcgState              = spinRandom;
            flame->body.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &flame->body);
            // The allocation already zeroed the entry; LAST terminates the table.
            flame->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
            flame->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            /* fallthrough */
        case FLAMETHROWER_FLAME_STATE_FLIGHT:
            // Keep composed endpoints for the geometry probe; contact tests use the sphere.
            work->scale    = work->scale + FLAMETHROWER_FLAME_FLIGHT_SIZE_STEP;
            work->move.vy  = work->move.vy + FLAMETHROWER_FLAME_FLIGHT_Y_ACCELERATION;
            flightStart.vx = coord->workm.t[0];
            flightStart.vy = coord->workm.t[1];
            flightStart.vz = coord->workm.t[2];
            _flamethrowerFlameAdvanceCoord(coord, work);
            flightEnd.vx = coord->workm.t[0];
            flightEnd.vy = coord->workm.t[1];
            flightEnd.vz = coord->workm.t[2];
            _flamethrowerFlameDrawBlob(GRAPHICS_MATRIX_TRANSLATION(&coord->workm),
                                       (work->age >> 1) + 1, work->scale,
                                       work->angle);
            splashRandom    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = splashRandom;
            if ((u16)((splashRandom >> 16) % 3) == 0 && gRoomEffectState->groundTraceEnabled != 0 &&
                worldCollisionProjectGroundCoord(coord, &groundCoord) == 1) {
                // Only projected XYZ feed the splash; the temporary's rotation stays unspecified.
                _flamethrowerFlameDrawSplash(GRAPHICS_MATRIX_TRANSLATION(&groundCoord.workm), (s16)((work->scale * 2) / 3));
            }
            if (worldCollisionCountContactsByKind(flame->body.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&flame->body);
                effectKillTask(work, task);
                return;
            }
            if (worldCollisionProbeGridSegment(&flightEnd, &flightStart, NULL, NULL) == 1) {
                // Drift no longer takes contacts; unsigned trig scaling precedes s16 truncation.
                worldCollisionUnlinkBody(&flame->body);
                task->state     = FLAMETHROWER_FLAME_STATE_DRIFT;
                work->move.vx   = (u32)rcos(work->angle) >> FLAMETHROWER_FLAME_DRIFT_TRIG_SHIFT;
                work->move.vy   = (u32)rsin(work->angle) >> FLAMETHROWER_FLAME_DRIFT_TRIG_SHIFT;
                driftRandom     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = driftRandom;
                work->move.vz   = (u32)rsin((driftRandom >> 16) & FLAMETHROWER_FLAME_SPIN_ANGLE_MASK) >> FLAMETHROWER_FLAME_DRIFT_TRIG_SHIFT;
                return;
            }
            if (work->age >= FLAMETHROWER_FLAME_LIFETIME_TICKS) {
                worldCollisionUnlinkBody(&flame->body);
                effectKillTask(work, task);
                return;
            }
            worldCollisionClearContacts(flame->contacts);
            return;
        case FLAMETHROWER_FLAME_STATE_DRIFT:
            work->scale = work->scale + FLAMETHROWER_FLAME_DRIFT_SIZE_STEP;
            _flamethrowerFlameAdvanceCoord(coord, work);
            _flamethrowerFlameDrawBlob(GRAPHICS_MATRIX_TRANSLATION(&coord->workm),
                                       (work->age >> 1) + 1, work->scale,
                                       work->angle);
            if (work->age >= FLAMETHROWER_FLAME_LIFETIME_TICKS) {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Applies an effect-atlas frame's palette and UV rectangle to a flying-flame quad.
///
/// Borrows a writable `quad` and one `gEffectSpriteAtlasFrames` entry for this
/// call. U/V origins and `EFFECT_SPRITE_ATLAS_UV_SPAN` count page-relative
/// texels; the inclusive corners cover a 40-by-40 cell in top-left, top-right,
/// bottom-left, bottom-right order. Palette X/Y count VRAM words/scanlines.
/// Writes the CLUT and eight UV bytes. The caller owns packet initialization,
/// geometry, texture-page selection and submission to the ordering table.
static inline void _flamethrowerFlameSetBlobTexture(POLY_FT4* quad, const EffectSpriteTextureFrame* textureFrame)
{
    quad->clut = getClut(textureFrame->clutX, textureFrame->clutY);
    quad->u0   = textureFrame->u;
    quad->v0   = textureFrame->v;
    quad->u1   = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->v1   = textureFrame->v;
    quad->u2   = textureFrame->u;
    quad->v2   = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->u3   = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->v3   = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
}

/// Draws the flying flame as a spinning, additive billboard at a world point.
///
/// Borrows the three s32 coordinates for this call and narrows each to s16.
/// `animationFrame` wraps modulo the twelve-frame `gEffectSpriteAtlasFrames`.
/// `sizeScale` is a sizing numerator: the screen half-diagonal in pixels is
/// sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN / (SZ3 / 4 + 1). `spinAngle` uses
/// 4096 units per turn. A negative GTE FLAG rejects the projection.
/// Reserves/releases one scratch block and appends one POLY_FT4 on acceptance;
/// the caller must provide scratch and frame-packet space through GPU drawing.
static void _flamethrowerFlameDrawBlob(const VECTOR3* worldPosition, u16 animationFrame, u16 sizeScale, s16 spinAngle)
{
    enum { QUARTER_TURN       = ONE / 4,
           TRIG_FRACTION_BITS = 12 };

    EffectShapeScratch*             scratchEnd;
    EffectShapeScratch*             scratch;
    EffectShapeScratch*             projectionScratch;
    POLY_FT4*                       quad;
    const EffectSpriteTextureFrame* textureFrame;
    u16                             textureFrameIndex;
    s32                             cornerAngle;
    u16                             worldZ;

    // Project the borrowed world point before allocating a GPU packet.
    scratchEnd                               = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (scratchEnd - 1)->worldPoint.vx          = (u16)worldPosition->vx;
    scratch                                  = scratchEnd - 1;
    scratch->worldPoint.vy                   = (u16)worldPosition->vy;
    worldZ                                   = (u16)worldPosition->vz;
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = scratch;
    scratch->worldPoint.vz                   = worldZ;
    projectionScratch                        = scratch;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    textureFrameIndex = animationFrame % ARRAY_SIZE(gEffectSpriteAtlasFrames);
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        scratch->depth = scratch->depth + 1;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage  = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
        textureFrame = &gEffectSpriteAtlasFrames[textureFrameIndex];
        _flamethrowerFlameSetBlobTexture(quad, textureFrame);
        // Reuse the inclusive texel span to size the billboard's half-diagonal.
        cornerAngle              = spinAngle;
        scratch->extent.corner.x = (((sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN) / scratch->depth) * rsin(cornerAngle)) >> TRIG_FRACTION_BITS;
        scratch->extent.corner.y = (((sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN) / scratch->depth) * rcos(cornerAngle)) >> TRIG_FRACTION_BITS;
        quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;
        cornerAngle              = cornerAngle + QUARTER_TURN;
        quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;
        scratch->extent.corner.x = (((sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN) / scratch->depth) * rsin(cornerAngle)) >> TRIG_FRACTION_BITS;
        scratch->extent.corner.y = (((sizeScale * EFFECT_SPRITE_ATLAS_UV_SPAN) / scratch->depth) * rcos(cornerAngle)) >> TRIG_FRACTION_BITS;
        quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;
        quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws an additive textured ground splash below a flying flame.
///
/// `halfSize` is a signed 16-bit local half-side in game coordinate units,
/// promoted by the caller to s32. Rotates its X/Z corners by the view
/// coordinate's work matrix, then adds the three
/// borrowed s32 coordinates of `worldPosition`, narrowing each result to s16.
/// The position need only live for this call. The final projection's negative
/// GTE FLAG rejects the quad. Requires one scratch block and one POLY_FT4 in
/// the frame arena, whose packet must remain live through GPU drawing.
static void _flamethrowerFlameDrawSplash(const VECTOR3* worldPosition, s32 halfSize)
{
    enum { TEXTURE_LEFT  = 0xE0,
           TEXTURE_TOP   = 0xC8,
           UV_SPAN       = 31,
           TEXTURE_SHADE = 0x40 }; // Half-intensity modulation (0x80 is neutral).

    EffectGroundQuadScratch*    scratchHead;
    EffectGroundQuadScratch*    scratch;
    POLY_FT4*                   quad;
    const EffectUnitQuadCorner* unitCorners;
    s32                         cornerIndex;
    s32                         projectionFlags;
    s32                         orderingDepth;

    scratchHead                                   = SCRATCH_STACK_CURSOR(EffectGroundQuadScratch) - 1;
    SCRATCH_STACK_CURSOR(EffectGroundQuadScratch) = scratchHead;
    scratch                                       = scratchHead;
    gte_SetTransMatrix(&GsWSMATRIX);
    cornerIndex = 0;
    unitCorners = gEffectUnitQuadCorners;
    do {
        scratch->vertices[cornerIndex].vx = (u16)unitCorners[cornerIndex].axis0Sign * halfSize;
        scratch->vertices[cornerIndex].vy = 0;
        scratch->vertices[cornerIndex].vz = (u16)unitCorners[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&scratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&scratch->vertices[cornerIndex]);
        (u16) scratch->vertices[cornerIndex].vx = (u16)scratch->vertices[cornerIndex].vx + (u16)worldPosition->vx;
        (u16) scratch->vertices[cornerIndex].vy = (u16)scratch->vertices[cornerIndex].vy + (u16)worldPosition->vy;
        (u16) scratch->vertices[cornerIndex].vz = (u16)scratch->vertices[cornerIndex].vz + (u16)worldPosition->vz;
        cornerIndex++;
    } while (cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners));

    // Project the first corner separately, then the remaining three together.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&scratch->screenCorners[0]);
    gte_ldv3(&scratch->vertices[1], &scratch->vertices[2], &scratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&scratch->screenCorners[1], &scratch->screenCorners[2], &scratch->screenCorners[3]);
    gte_stflg(&projectionFlags);
    if (projectionFlags >= 0) {
#if FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE
        // The original pre-store stack increment is discarded by the GTE store.
        orderingDepth++;
#endif
        gte_stszotz(&orderingDepth);
#if !FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE
        orderingDepth++;
#endif
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setRGB0(quad, TEXTURE_SHADE, TEXTURE_SHADE, TEXTURE_SHADE);
        quad->tpage = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
        quad->clut  = getClut(240, 268);
        setUV4(quad, TEXTURE_LEFT, TEXTURE_TOP, TEXTURE_LEFT + UV_SPAN, TEXTURE_TOP,
               TEXTURE_LEFT, TEXTURE_TOP + UV_SPAN, TEXTURE_LEFT + UV_SPAN, TEXTURE_TOP + UV_SPAN);
        quad->x0 = scratch->screenCorners[0].vx;
        quad->y0 = scratch->screenCorners[0].vy;
        quad->x1 = scratch->screenCorners[1].vx;
        quad->y1 = scratch->screenCorners[1].vy;
        quad->x2 = scratch->screenCorners[2].vx;
        quad->y2 = scratch->screenCorners[2].vy;
        quad->x3 = scratch->screenCorners[3].vx;
        quad->y3 = scratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)orderingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectGroundQuadScratch);
}

/// Tears down a counted flying flame after unlinking its collision sphere.
///
/// `task` owns a `FlamethrowerFlameBody` in `work` (or NULL) and a
/// separate `EffectWork` in `spawnArg2.pointer`. Default task teardown frees
/// the collision block and releases the coordinate body and task. Call once;
/// neither allocation nor the task may be used after teardown.
static void _flamethrowerFlameRelease(Task* task)
{
    FlamethrowerFlameBody* flame      = task->work;
    EffectWork*            effectWork = task->spawnArg2.pointer;

    if (flame != NULL) {
        worldCollisionUnlinkBody(&flame->body);
    }
    effectKillTask(effectWork, task);
}

#undef FLAMETHROWER_FLAME_REDRAW_UPDATES_COORD
#undef FLAMETHROWER_FLAME_SPLASH_INCREMENT_BEFORE_DEPTH_STORE
