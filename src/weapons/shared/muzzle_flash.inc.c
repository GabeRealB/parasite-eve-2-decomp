

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/gtemac.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/task_types.h"

#include "overlay.h"

#include "muzzle_flash.h"

/* Part of the muzzle flash library; see muzzle_flash.h. The carrier exports `_muzzleFlashTask` under its own
   name with DEFINE_ALIAS. */

/// Activates a transient white point light at the effect's initial position.
///
/// `spawnCoord` supplies its local translation in the light's parent frame;
/// the flash calls this while both coordinates are parented to the view,
/// before attaching the effect to the weapon's muzzle. The slot must already
/// be initialized with that borrowed parent. Full Q12 white intensity extends
/// to 4000 world units and falls to zero at 4800; the slot expires after four
/// unpaused light updates. Placement invalidates its cached transform for the
/// next composition pass. The slot is shared storage that another effect can
/// overwrite; no pointer is retained by this helper.
static inline void _muzzleFlashInitializeLight(const GfxCoord* spawnCoord, WorldCoordTransientPointLight* transientLight)
{
    enum {
        MUZZLE_FLASH_LIGHT_INNER_RADIUS = 0xFA0,
        MUZZLE_FLASH_LIGHT_OUTER_RADIUS = 0x12C0,
        MUZZLE_FLASH_LIGHT_FRAME_COUNT  = 4
    };
    WorldCoordPointLight* pointLight = &transientLight->light;

    pointLight->head.transform.coord.coord.t[0]   = spawnCoord->coord.t[0];
    pointLight->head.transform.coord.coord.t[1]   = spawnCoord->coord.t[1];
    pointLight->head.transform.coord.coord.t[2]   = spawnCoord->coord.t[2];
    pointLight->head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    pointLight->head.color.r                      = ONE;
    pointLight->head.color.g                      = ONE;
    pointLight->head.color.b                      = ONE;
    pointLight->inner                             = MUZZLE_FLASH_LIGHT_INNER_RADIUS;
    pointLight->outer                             = MUZZLE_FLASH_LIGHT_OUTER_RADIUS;
    transientLight->framesLeft                    = MUZZLE_FLASH_LIGHT_FRAME_COUNT;
}

/// Advances the seven-update flash, its additive screen tint and its point light.
///
/// `task` owns the live EffectWork in spawnArg2.pointer and a coordinate body.
/// Its borrowed spawn coordinate and ancestors must survive the flash. The
/// first visible update attaches the body at the carrier's muzzle offset,
/// initializes size 1536..2559, spin 0..4095 and brightness 192, and retains
/// one random streak angle in each quadrant in the carrier's shared buffer;
/// initializing another flash replaces the angles all live flashes read.
/// Later visible updates halve size and brightness; EffectWork.scale, angle
/// and period hold those values.
/// Hidden effects suspend this task completely, including age and cleanup.
/// Each update draws before freeing the work and task at age seven. Transient
/// light slot zero is shared and may be overwritten by another effect; its
/// four-frame expiry is maintained separately from the task's lifetime.
static void _muzzleFlashTask(Task* task)
{
    enum {
        MUZZLE_FLASH_STATE_INITIALIZE   = 0,
        MUZZLE_FLASH_STATE_FADE         = 1,
        MUZZLE_FLASH_LIGHT_SLOT         = 0,
        MUZZLE_FLASH_LIGHT_RADIUS_STEP  = 0x190,
        MUZZLE_FLASH_FRAME_COUNT        = 7,
        MUZZLE_FLASH_INITIAL_BRIGHTNESS = 0xC0,
        MUZZLE_FLASH_MIN_WORLD_SIZE     = 0x600,
        MUZZLE_FLASH_QUADRANT_SHIFT     = 10 // 1024 angle units per streak quadrant
    };
    EffectWork*                    work;
    GfxCoord*                      muzzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    u8                             tintRgb[3];
    s32                            streakIndex;

    work           = task->spawnArg2.pointer;
    muzzleCoord    = task->extra.coordBody->coord;
    transientLight = &gWorldCoordTransientPointLights[MUZZLE_FLASH_LIGHT_SLOT];
    pointLight     = &transientLight->light;

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }

    work->age++;
    switch (task->state) {
        case MUZZLE_FLASH_STATE_INITIALIZE:
            // Start the light at the spawn position before applying the muzzle offset.
            _muzzleFlashInitializeLight(muzzleCoord, transientLight);

            // Attach and compose once; normal coordinate updates refresh it thereafter.
            muzzleCoord->parent       = work->parent;
            muzzleCoord->coord.t[0]   = _gMuzzleFlashOffset.vx;
            muzzleCoord->coord.t[1]   = _gMuzzleFlashOffset.vy;
            muzzleCoord->coord.t[2]   = _gMuzzleFlashOffset.vz;
            muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(muzzleCoord);

            work->period = MUZZLE_FLASH_INITIAL_BRIGHTNESS;
            RANDOM_LCG_ADVANCE();
            work->scale = (RANDOM_LCG_DRAW() & (MUZZLE_FLASH_ANGLE_QUARTER_TURN - 1)) + MUZZLE_FLASH_MIN_WORLD_SIZE;
            RANDOM_LCG_ADVANCE();
            work->angle = RANDOM_LCG_DRAW() & MUZZLE_FLASH_ANGLE_MASK;
            task->state = MUZZLE_FLASH_STATE_FADE;
            for (streakIndex = 0; streakIndex < ARRAY_SIZE(_gMuzzleFlashAngles); streakIndex++) {
                RANDOM_LCG_ADVANCE();
                _gMuzzleFlashAngles[streakIndex] = ((streakIndex & (ARRAY_SIZE(_gMuzzleFlashAngles) - 1)) << MUZZLE_FLASH_QUADRANT_SHIFT) + (RANDOM_LCG_DRAW() & (MUZZLE_FLASH_ANGLE_QUARTER_TURN - 1));
            }
            break;
        case MUZZLE_FLASH_STATE_FADE:
            work->scale  = work->scale >> 1;
            work->period = work->period >> 1;
            break;
    }

    _muzzleFlashDrawCore(muzzleCoord, work->scale, work->angle);
    // One byte supplies all tint channels; preserve the single shared load.
    tintRgb[0] = tintRgb[1] = tintRgb[2] = work->period;
    effectDrawScreenTint(tintRgb, GPU_BLEND_ADD);
    for (streakIndex = 0; streakIndex < ARRAY_SIZE(_gMuzzleFlashAngles); streakIndex++) {
        _muzzleFlashDrawStreak(muzzleCoord, _gMuzzleFlashAngles[streakIndex], work->period);
    }
    // Shrink only the full-strength radius; the zero-contribution radius stays fixed.
    if (pointLight->inner >= MUZZLE_FLASH_LIGHT_RADIUS_STEP + 1) {
        pointLight->inner -= MUZZLE_FLASH_LIGHT_RADIUS_STEP;
    }
    if (work->age >= MUZZLE_FLASH_FRAME_COUNT) {
        effectKillTask(work, task);
    }
}

/// Sets the pixel offsets for one opposite-corner pair of the muzzle-flash core.
///
/// `spriteScratch->otz` must contain the projected centre's SZ3 / 4 depth,
/// at least `MUZZLE_FLASH_MIN_DEPTH`. `worldSize` is the nonnegative size
/// scale supplied by the flash task (0..2559). Integer division first gives
/// a pixel radius of worldSize * 55 / otz; Q12 sine/cosine then produce signed
/// offsets without narrowing them to the packet's 16-bit coordinates.
/// `cornerAngle` uses 4096 units per turn: zero points up, a quarter turn
/// right. The caller adds/subtracts `cornerDx` along X and subtracts/adds
/// `cornerDy` along screen Y, then repeats a quarter turn later for the other
/// pair. Only those two scratch words are written; no address is retained.
static inline void _muzzleFlashSetCoreCornerOffsets(OverlaySpriteScratch* spriteScratch, s16 worldSize, s32 cornerAngle)
{
    enum { MUZZLE_FLASH_CORE_PROJECTION_SCALE = 55 };
    spriteScratch->cornerDx = (((worldSize * MUZZLE_FLASH_CORE_PROJECTION_SCALE) / spriteScratch->otz) * rsin(cornerAngle)) >> MUZZLE_FLASH_TRIG_FRACTION_BITS;
    spriteScratch->cornerDy = (((worldSize * MUZZLE_FLASH_CORE_PROJECTION_SCALE) / spriteScratch->otz) * rcos(cornerAngle)) >> MUZZLE_FLASH_TRIG_FRACTION_BITS;
}

/// Draws the additive textured core as a spinning camera-facing square.
///
/// `muzzleCoord->workm` must be freshly composed through the view chain; its
/// translation supplies the centre, narrowed to signed 16-bit coordinates.
/// `worldSize` is a nonnegative game-coordinate radius scale (the task starts
/// at 1536..2559); the screen corner radius is worldSize * 55 / (SZ3 / 4).
/// `spinAngle` uses 4096 units per turn. The raw texture supplies all colour.
/// Requires initialized projection settings, a free scratch block and space
/// for one POLY_FT4 packet. The packet is consumed even below the depth cutoff.
static void _muzzleFlashDrawCore(const GfxCoord* muzzleCoord, s16 worldSize, s16 spinAngle)
{
    enum {
        MUZZLE_FLASH_CORE_TEXTURE_LEFT     = 0x70,
        MUZZLE_FLASH_CORE_TEXTURE_TOP      = 0xC8,
        MUZZLE_FLASH_CORE_TEXTURE_RIGHT    = 0xA7,
        MUZZLE_FLASH_CORE_TEXTURE_BOTTOM   = 0xFF,
        MUZZLE_FLASH_CORE_SEMI_TRANSPARENT = 0x02,
        MUZZLE_FLASH_CORE_RAW_TEXTURE      = 0x01
    };
    OverlaySpriteScratch* block;
    POLY_FT4*             quad;
    s32                   cornerAngle;

    // Project the composed muzzle centre; the quad itself stays in screen space.
    block              = SCRATCH_STACK_RESERVE_BLOCK(OverlaySpriteScratch);
    block->worldPos.vx = muzzleCoord->workm.t[0];
    block->worldPos.vy = muzzleCoord->workm.t[1];
    block->worldPos.vz = muzzleCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= MUZZLE_FLASH_MIN_DEPTH) {
        cornerAngle = spinAngle;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 0x240, 0);
        quad->clut  = getClut(0xB0, 0x10A);
        setUV4(quad, MUZZLE_FLASH_CORE_TEXTURE_LEFT, MUZZLE_FLASH_CORE_TEXTURE_TOP,
               MUZZLE_FLASH_CORE_TEXTURE_RIGHT, MUZZLE_FLASH_CORE_TEXTURE_TOP,
               MUZZLE_FLASH_CORE_TEXTURE_LEFT, MUZZLE_FLASH_CORE_TEXTURE_BOTTOM,
               MUZZLE_FLASH_CORE_TEXTURE_RIGHT, MUZZLE_FLASH_CORE_TEXTURE_BOTTOM);
        setcode(quad, getcode(quad) | MUZZLE_FLASH_CORE_SEMI_TRANSPARENT | MUZZLE_FLASH_CORE_RAW_TEXTURE);

        // Opposite corner pairs are a quarter turn apart around the projected centre.
        _muzzleFlashSetCoreCornerOffsets(block, worldSize, cornerAngle);
        quad->x0     = block->screenPos.vx + block->cornerDx;
        quad->x3     = block->screenPos.vx - block->cornerDx;
        quad->y0     = block->screenPos.vy - block->cornerDy;
        cornerAngle += MUZZLE_FLASH_ANGLE_QUARTER_TURN;
        quad->y3     = block->screenPos.vy + block->cornerDy;
        _muzzleFlashSetCoreCornerOffsets(block, worldSize, cornerAngle);
        quad->x1 = block->screenPos.vx + block->cornerDx;
        quad->x2 = block->screenPos.vx - block->cornerDx;
        quad->y1 = block->screenPos.vy - block->cornerDy;
        quad->y2 = block->screenPos.vy + block->cornerDy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlaySpriteScratch);
}

/// Draws one additive blue-white streak in the muzzle's coordinate frame.
///
/// `muzzleCoord->workm` must be freshly composed through the view chain.
/// Angles use 4096 units per turn. Three base corners have radius 256 around
/// `directionAngle`, at offsets -192, 0 and +192; the tip has radius 1536 and
/// local Z -512. The muzzle transform rotates that Z offset with the weapon.
/// `brightness` is 0..255; only the centre base corner is lit, with half
/// brightness in red/green and full brightness in blue. Positions narrow to
/// signed 16-bit coordinates before projection. Requires initialized projection
/// settings, one free scratch block and packet space for POLY_G4 plus its blend
/// command. A rejected depth still consumes the quad packet.
static void _muzzleFlashDrawStreak(const GfxCoord* muzzleCoord, s16 directionAngle, s16 brightness)
{
    enum {
        MUZZLE_FLASH_STREAK_BASE_ANGLE_OFFSET = 0xC0,
        MUZZLE_FLASH_STREAK_TIP_RADIUS        = 0x600,
        MUZZLE_FLASH_STREAK_TIP_LOCAL_Z       = -0x200,
        MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT   = 4 // 4096-amplitude trig to a radius of 256
    };
    EffectQuadCornersScratch* block;
    POLY_G4*                  quad;
    const MATRIX*             muzzleMatrix;
    s32                       cornerAngle;
    s32                       firstBaseAngle;
    s32                       tipRadius;
    s32                       tipLocalZ;

    // Keep the tip dimensions in locals at their first use: the target uses mult.
    tipLocalZ = MUZZLE_FLASH_STREAK_TIP_LOCAL_Z;
    block     = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    cornerAngle = directionAngle;

    // Build local corners, then rotate and add translations modulo 16 bits.
    // Unsigned addition preserves wrapping even for a full-width translation.
    firstBaseAngle        = cornerAngle - MUZZLE_FLASH_STREAK_BASE_ANGLE_OFFSET;
    block->vertices[0].vx = (u32)rsin(firstBaseAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[0].vy = (u32)rcos(firstBaseAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[0].vz = 0;
    muzzleMatrix          = &muzzleCoord->workm;
    gte_ApplyMatrixSV(muzzleMatrix, &block->vertices[0], &block->vertices[0]);
    block->vertices[0].vx += (u32)muzzleCoord->workm.t[0];
    block->vertices[0].vy += (u32)muzzleCoord->workm.t[1];
    block->vertices[0].vz += (u32)muzzleCoord->workm.t[2];

    tipRadius             = MUZZLE_FLASH_STREAK_TIP_RADIUS;
    block->vertices[1].vx = (rsin(cornerAngle) * tipRadius) >> MUZZLE_FLASH_TRIG_FRACTION_BITS;
    block->vertices[1].vy = (rcos(cornerAngle) * tipRadius) >> MUZZLE_FLASH_TRIG_FRACTION_BITS;
    block->vertices[1].vz = tipLocalZ;
    gte_ApplyMatrixSV(muzzleMatrix, &block->vertices[1], &block->vertices[1]);
    block->vertices[1].vx += (u32)muzzleCoord->workm.t[0];
    block->vertices[1].vy += (u32)muzzleCoord->workm.t[1];
    block->vertices[1].vz += (u32)muzzleCoord->workm.t[2];

    block->vertices[2].vx = (u32)rsin(cornerAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[2].vy = (u32)rcos(cornerAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[2].vz = 0;
    gte_ApplyMatrixSV(muzzleMatrix, &block->vertices[2], &block->vertices[2]);
    block->vertices[2].vx += (u32)muzzleCoord->workm.t[0];
    cornerAngle            = cornerAngle + MUZZLE_FLASH_STREAK_BASE_ANGLE_OFFSET;
    block->vertices[2].vy += (u32)muzzleCoord->workm.t[1];
    block->vertices[2].vz += (u32)muzzleCoord->workm.t[2];

    block->vertices[3].vx = (u32)rsin(cornerAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[3].vy = (u32)rcos(cornerAngle) >> MUZZLE_FLASH_STREAK_BASE_TRIG_SHIFT;
    block->vertices[3].vz = 0;
    gte_ApplyMatrixSV(muzzleMatrix, &block->vertices[3], &block->vertices[3]);
    block->vertices[3].vx += (u32)muzzleCoord->workm.t[0];
    block->vertices[3].vy += (u32)muzzleCoord->workm.t[1];
    block->vertices[3].vz += (u32)muzzleCoord->workm.t[2];

    // Project corner 0 separately, then corners 1..3 into the packet.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vertices[0]);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyG4(quad);
    gte_stsxy(&quad->x0);
    gte_ldv3(&block->vertices[1], &block->vertices[2],
             &block->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&block->depth);
    if (block->depth >= MUZZLE_FLASH_MIN_DEPTH) {
        setRGB0(quad, 0, 0, 0);
        setRGB1(quad, 0, 0, 0);
        setRGB2(quad, brightness >> 1, brightness >> 1, brightness);
        setRGB3(quad, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
}
