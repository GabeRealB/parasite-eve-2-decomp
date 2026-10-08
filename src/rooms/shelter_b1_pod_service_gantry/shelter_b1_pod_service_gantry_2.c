#include "rooms/shelter_b1_pod_service_gantry.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b1_pod_service_gantry_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_flags.h"
#include "gameplay/effects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
/// Empty presence flag: this room supplies `_waterDrawSpinU16` and
/// `_waterDrawTileU16`.
///
/// Defined, with no replacement list, immediately before `water_effects.h`.
/// `defined()` is the only test. That header prototypes the shared drawers
/// only when `WATER_SHARED_U16_DRAWERS` is set
/// and this flag is not. This file prototypes and defines both drawers and
/// includes `water_drift_task_u16.inc.c` instead of the shared bodies. The
/// spin sprite uses texture page 0x2C. The tile grid starts at v 0 with clut
/// 0x4393. Both drawers take a `u16` index and an `s16` scale; the spin
/// drawer also takes an `s16` angle.
#define WATER_OWN_U16_DRAWERS
#include "../../shared/water_effects.h"
/// Binds the gantry's exported `void (Task*)` callback for the rising-sprite instance.
///
/// Supply a function identifier before the first inclusion of `effect_sprite.h`.
/// Keep it defined through `effect_sprite_rise.inc.c`, which undefines it.
/// Gameplay imports this package's compiled callback for effect slot 0x1C6.
#define EFFECT_SPRITE_RISE_TASK shelterB1PodServiceGantryEffectSpriteRiseTask
#include "../../shared/effect_sprite.h"

static void _waterDrawSpinU16(const GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTileU16(const GfxCoord* coord, u16 textureCell, s16 radiusScale);

/// Scratch-stack workspace for the room's spinning water sprite.
///
/// One perspective transform of `worldPoint` supplies the screen centre, the
/// GTE status and the SZ3 / 4 depth. The depth divides the sprite's size onto
/// the screen and orders its primitive, so it must be nonzero once accepted.
/// `cornerOffsetX` and `cornerOffsetY` hold the signed pixel displacement to
/// one pair of opposite corners, then are reused a quarter turn later for the
/// other pair; only their low halfwords reach the GPU packet.
///
/// `screenX` and `screenY` keep the raw 16-bit encodings of the signed GTE
/// pixel coordinates. They are adjacent so one GTE word store fills both.
///
/// The status word sits between the depth and the corner offsets.
/// `EffectBillboardScratch` keeps it after the offsets and `EffectShapeScratch`
/// puts the world point first, so this drawer uses neither. Reserve one
/// complete, word-aligned block and release it after drawing; no pointer into
/// it survives release.
typedef struct {
    s32     depth;           // SZ3 / 4; divisor for the corner offsets and depth for sorting
    s32     projectionFlags; // GTE FLAG word; a negative value rejects the projection
    s32     cornerOffsetX;   // Signed horizontal displacement from the centre to a corner, in pixels
    s32     cornerOffsetY;   // Signed vertical displacement from the centre to a corner, in pixels
    SVECTOR worldPoint;      // World position, with each translation component narrowed to s16
    u16     screenX;         // Raw projected centre X; first half of the GTE screen-position word
    u16     screenY;         // Raw projected centre Y; second half of the GTE screen-position word
} _ShelterB1PodServiceGantrySpinScratch;
STATIC_ASSERT_SIZEOF(_ShelterB1PodServiceGantrySpinScratch, 0x1C);

extern s32 D_actor_560800_801752EC;
extern s8  D_shelter_b1_pod_service_gantry_8018256C[];

static void _shelterB1PodServiceGantryDrawBankedDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _shelterB1PodServiceGantryDrawAlternateDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);

s32 D_shelter_b1_pod_service_gantry_8018250C[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_shelter_b1_pod_service_gantry_80182518[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_shelter_b1_pod_service_gantry_80182520[8] = {
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
};

AreaApplyRec D_shelter_b1_pod_service_gantry_80182540[11] = {
    { 4, 17, 22, 0 },
    { 4, 25, 1, 0 },
    { 4, 27, 1, 0 },
    { 4, 28, 1, 0 },
    { 4, 29, 1, 0 },
    { 4, 30, 1, 0 },
    { 4, 32, 1, 0 },
    { 4, 33, 1, 0 },
    { 4, 34, 1, 0 },
    { 4, 35, 22, 0 },
    { 255, 0, 0, 0 },
};

s8 D_shelter_b1_pod_service_gantry_8018256C[8] = { 0 };

/// Writes the signed pixel offset to one rotated drift-sprite corner.
///
/// `projection` borrows a live workspace with positive `depth`; only
/// `extent.corner` changes and no pointer is retained. The signed half-diagonal
/// is `sizeFactor * 47 / depth` pixels, truncated toward zero before rotation.
/// Products must fit s32; the Q12 arithmetic shift rounds negative products down.
///
/// `cornerAngle` is absolute, in 4096 units per turn. X points right and Y up:
/// with a positive half-diagonal, zero points up and a quarter turn points right.
/// The drawer subtracts Y from the centre and uses opposite signs for each pair
/// of corners, repeating a quarter turn later for the other pair.
static __inline__ void _shelterB1PodServiceGantryRotateDriftCorner(EffectShapeScratch* projection, s16 sizeFactor, s32 cornerAngle)
{
    enum {
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PERSPECTIVE_SCALE  = 47, // Multiplier in the depth-divided half-diagonal numerator
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_TRIG_FRACTION_BITS = 12  // Fractional bits in rsin/rcos samples; 4096 represents 1.0
    };
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                  = rsin(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.x = (halfDiagonalPixels * trigSample) >> SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_TRIG_FRACTION_BITS;
    trigSample                  = rcos(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.y = (halfDiagonalPixels * trigSample) >> SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_TRIG_FRACTION_BITS;
}

/// Binds the pod service gantry's exported `void (Task*)` callback for this drift instance.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Used only as the definition name; no arguments, captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_TASK shelterB1PodServiceGantrySpriteDriftTask
/// Binds the twelve-frame drift drawer: `(const GfxCoord*, u16 frameAndPalette, s16 size, s16 angle)`.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Arguments are evaluated once; the coordinate is borrowed, size is a perspective
/// numerator, and angle uses 4096 units per turn. No captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_DRAW_BANKED _shelterB1PodServiceGantryDrawBankedDriftSprite
/// Binds the ten-frame drift drawer with the same coordinate, packed-frame, size and angle contract.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Arguments are evaluated once; the coordinate is borrowed, size is a perspective
/// numerator, and angle uses 4096 units per turn. No captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE _shelterB1PodServiceGantryDrawAlternateDriftSprite
#include "../../shared/effect_sprite_drift.inc.c"

/// Draws a rotating twelve-cell sprite with a per-cell or alternate palette.
///
/// `coord->workm` must be composed for `GsWSMATRIX`; translation is narrowed
/// to s16. Bits 0..11 of `frameAndPalette` hold cell 0..11; selectors 0/1 in
/// bits 12..15 use per-cell CLUT rows 270/271, and 2..15 use CLUT 0x428F.
/// The five-column sheet has 48-texel cells at V=112 on texture page 0x2B.
///
/// `size * 47 / depth` gives the signed pixel half-diagonal before Q12
/// rotation; `angle` uses 4096 units per turn. Accepted SZ3/4 depth must be
/// nonzero. A nonnegative GTE FLAG queues an additive raw-texture FT4. One
/// complete scratch block is reserved, cleared and released on every path;
/// the initialized scratch stack and primitive cursor must have capacity.
static void _shelterB1PodServiceGantryDrawBankedDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        // Signed row offsets wrap to GPU UV bytes; last - first is 47 modulo 256.
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FIRST_TEXEL_ROW   = 112,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_LAST_TEXEL_ROW    = 159,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CLUT_ROW_SHIFT    = 6,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PALETTE_ROW_COUNT = 2,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FRAME_MASK        = 0xFFF,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PALETTE_SHIFT     = 12,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELLS_PER_ROW     = 5,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELL_PITCH_TEXELS = 48,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_UV_SPAN_TEXELS    = 47,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_QUARTER_TURN      = 0x400,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PACKET_WORDS      = sizeof(POLY_FT4) / sizeof(u32) - 1,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PACKET_CODE       = 0x2F,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_TEXTURE_PAGE      = 0x2B,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_ALTERNATE_CLUT    = 0x428F,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FIRST_PALETTE_ROW = 0x10E,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CLUT_COLUMN_MASK  = 0x3F
    };
    void**              cursorSlot;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    u16                 paletteSelector;
    u32                 frameOrAngle;
    u16                 cellColumn;
    u16                 cellRow;
    s32                 cellU;
    s32                 cellV;
    s32                 cornerAngle;

    frameOrAngle    = frameAndPalette;
    frameOrAngle   &= SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FRAME_MASK;
    paletteSelector = frameAndPalette >> SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PALETTE_SHIFT;
    cursorSlot      = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd      = *cursorSlot;
    *cursorSlot     = scratchEnd - 1;
    projection      = scratchEnd - 1;
    memFillBytes(projection, 0, sizeof(*projection));
    (scratchEnd - 1)->worldPoint.vx = (u16)coord->workm.t[0];
    projection->worldPoint.vy       = (u16)coord->workm.t[1];
    projection->worldPoint.vz       = (u16)coord->workm.t[2];
    // Project the composed centre; the quad rotates only in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PACKET_WORDS);
        setcode(quad, SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PACKET_CODE);
        quad->tpage = SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_TEXTURE_PAGE;
        if (paletteSelector >= SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PALETTE_ROW_COUNT) {
            quad->clut = SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_ALTERNATE_CLUT;
        } else {
            quad->clut = ((paletteSelector + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FIRST_PALETTE_ROW) << SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CLUT_ROW_SHIFT) | (frameOrAngle & SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CLUT_COLUMN_MASK);
        }
        cellColumn  = (u16)frameOrAngle % SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELLS_PER_ROW;
        cellRow     = (u16)frameOrAngle / SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELLS_PER_ROW;
        cornerAngle = angle;
        cellU       = cellColumn * SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELL_PITCH_TEXELS;
        cellV       = cellRow * SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELL_PITCH_TEXELS;
        setUV4(quad, cellU, cellV + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FIRST_TEXEL_ROW, cellU + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_UV_SPAN_TEXELS, cellV + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FIRST_TEXEL_ROW, cellU, cellV + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_LAST_TEXEL_ROW, cellU + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_UV_SPAN_TEXELS, cellV + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_LAST_TEXEL_ROW);
        _shelterB1PodServiceGantryRotateDriftCorner(projection, size, cornerAngle);
        quad->x0     = projection->screenX + (u16)projection->extent.corner.x;
        quad->x3     = projection->screenX - (u16)projection->extent.corner.x;
        quad->y0     = projection->screenY - (u16)projection->extent.corner.y;
        frameOrAngle = cornerAngle + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_QUARTER_TURN;
        quad->y3     = projection->screenY + (u16)projection->extent.corner.y;
        _shelterB1PodServiceGantryRotateDriftCorner(projection, size, frameOrAngle);
        quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
        quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
        quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
        quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
        // Sort by projection depth; the GPU consumes a complete textured-quad packet.
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws a rotating ten-cell sprite with a base or alternate palette.
///
/// `coord->workm` must be composed for `GsWSMATRIX`; translation is narrowed
/// to s16. Bits 0..11 of `frameAndPalette` hold cell 0..9; any nonzero
/// selector in bits 12..15 uses CLUT 0x428F instead of 0x43D0. The five-column
/// sheet has 48-texel cells starting at V=128 on texture page 0x2C.
///
/// `size * 47 / depth` gives the signed pixel half-diagonal before Q12
/// rotation; `angle` uses 4096 units per turn. Accepted SZ3/4 depth must be
/// nonzero. A nonnegative GTE FLAG queues an additive raw-texture FT4. One
/// complete scratch block is reserved, cleared and released on every path;
/// the initialized scratch stack and primitive cursor must have capacity.
static void _shelterB1PodServiceGantryDrawAlternateDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        // Signed row offsets wrap to GPU UV bytes; last - first is 47 modulo 256.
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FIRST_TEXEL_ROW   = -128,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_LAST_TEXEL_ROW    = -81,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FRAME_MASK        = 0xFFF,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PALETTE_SHIFT     = 12,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELLS_PER_ROW     = 5,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELL_PITCH_TEXELS = 48,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_UV_SPAN_TEXELS    = 47,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_QUARTER_TURN      = 0x400,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PACKET_WORDS      = sizeof(POLY_FT4) / sizeof(u32) - 1,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PACKET_CODE       = 0x2F,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_TEXTURE_PAGE      = 0x2C,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_ALTERNATE_CLUT    = 0x428F,
        SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_BASE_CLUT         = 0x43D0
    };
    void**              cursorSlot;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    u16                 paletteSelector;
    u16                 cellColumn;
    u16                 cellRow;
    s32                 cellU;
    s32                 cellV;
    s32                 cornerAngle;
    s32                 perpendicularAngle;

    paletteSelector  = frameAndPalette >> SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PALETTE_SHIFT;
    frameAndPalette &= SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FRAME_MASK;
    cursorSlot       = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd       = *cursorSlot;
    *cursorSlot      = scratchEnd - 1;
    projection       = scratchEnd - 1;
    memFillBytes(projection, 0, sizeof(*projection));
    (scratchEnd - 1)->worldPoint.vx = (u16)coord->workm.t[0];
    projection->worldPoint.vy       = (u16)coord->workm.t[1];
    projection->worldPoint.vz       = (u16)coord->workm.t[2];
    // Project the composed centre; the quad rotates only in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PACKET_WORDS);
        setcode(quad, SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_PACKET_CODE);
        quad->tpage = SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_TEXTURE_PAGE;
        quad->clut  = paletteSelector ? SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_ALTERNATE_CLUT : SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_BASE_CLUT;
        cellColumn  = frameAndPalette % SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELLS_PER_ROW;
        cellRow     = frameAndPalette / SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELLS_PER_ROW;
        cornerAngle = angle;
        cellU       = cellColumn * SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELL_PITCH_TEXELS;
        cellV       = cellRow * SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_CELL_PITCH_TEXELS;
        setUV4(quad, cellU, cellV + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FIRST_TEXEL_ROW, cellU + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_UV_SPAN_TEXELS, cellV + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_FIRST_TEXEL_ROW, cellU, cellV + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_LAST_TEXEL_ROW, cellU + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_UV_SPAN_TEXELS, cellV + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_LAST_TEXEL_ROW);
        _shelterB1PodServiceGantryRotateDriftCorner(projection, size, cornerAngle);
        quad->x0           = projection->screenX + (u16)projection->extent.corner.x;
        quad->x3           = projection->screenX - (u16)projection->extent.corner.x;
        quad->y0           = projection->screenY - (u16)projection->extent.corner.y;
        perpendicularAngle = cornerAngle + SHELTER_B1_POD_SERVICE_GANTRY_DRIFT_QUARTER_TURN;
        quad->y3           = projection->screenY + (u16)projection->extent.corner.y;
        _shelterB1PodServiceGantryRotateDriftCorner(projection, size, perpendicularAngle);
        quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
        quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
        quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
        quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
        // Sort by projection depth; the GPU consumes a complete textured-quad packet.
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#include "../../shared/water_drift_task_u16.inc.c"

void shelterB1PodServiceGantryWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

/// Writes the signed screen offset from a water sprite's centre to one corner.
///
/// `projection` borrows a live, word-aligned scratch block with positive
/// `depth` equal to SZ3 / 4. Only `cornerOffsetX` and `cornerOffsetY` change;
/// the caller keeps ownership and no pointer is retained. The signed pixel
/// half-diagonal is `radiusScale * 31 / depth`, truncated toward zero before
/// rotation. Each Q12 product must fit s32; the arithmetic shift rounds down.
///
/// `cornerAngle` uses 4096 units per turn, with X right and Y up: for a
/// positive half-diagonal, zero points up and a quarter turn points right.
/// It stays s32 so adding a quarter turn does not narrow the bearing to s16.
static inline void _shelterB1PodServiceGantryComputeWaterCornerOffset(_ShelterB1PodServiceGantrySpinScratch* projection, s16 radiusScale, s32 cornerAngle)
{
    enum {
        WATER_SPIN_U16_PERSPECTIVE_SCALE  = 31,
        WATER_SPIN_U16_TRIG_FRACTION_BITS = 12
    };
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                = rsin(cornerAngle);
    halfDiagonalPixels        = (radiusScale * WATER_SPIN_U16_PERSPECTIVE_SCALE) / projection->depth;
    projection->cornerOffsetX = (halfDiagonalPixels * trigSample) >> WATER_SPIN_U16_TRIG_FRACTION_BITS;
    trigSample                = rcos(cornerAngle);
    halfDiagonalPixels        = (radiusScale * WATER_SPIN_U16_PERSPECTIVE_SCALE) / projection->depth;
    projection->cornerOffsetY = (halfDiagonalPixels * trigSample) >> WATER_SPIN_U16_TRIG_FRACTION_BITS;
}

/// Draws one rotated, camera-facing cell of the eight-frame water-drift strip.
///
/// `coord` is borrowed with `workm` already composed in the input space of
/// `GsWSMATRIX`; drift tasks normally supply view-space translations. Each
/// translation keeps its low 16 bits as a signed GTE coordinate.
/// `textureColumn` is unsigned. Frame 0..7 selects a 32-texel square at
/// V=224..255; UVs wrap to bytes without a bounds check. The 4-bit texture page
/// is at VRAM (768, 0), with its 16-colour palette at (304, 271).
///
/// `radiusScale * 31 / depth` gives the signed half-diagonal in pixels before
/// rotation. Drift tasks supply scale 0..4095. `spinAngle` uses 4096 units per
/// turn. With positive scale, zero puts the first corner above the centre,
/// a quarter turn to its right. Signed division truncates toward zero; the Q12 products round down
/// and must fit s32. Accepted projections require positive SZ3/4 depth; there
/// is no extra check.
///
/// A nonnegative GTE FLAG queues one raw-texture, additive semi-transparent
/// `POLY_FT4`; the primitive cursor must have space for the complete packet.
/// One word-aligned `_ShelterB1PodServiceGantrySpinScratch` is reserved on the
/// initialized scratch stack and released on every path. No pointer is
/// retained; GTE state changes.
static void _waterDrawSpinU16(const GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle)
{
    enum {
        WATER_SPIN_U16_CELL_SHIFT      = 5,
        WATER_SPIN_U16_UV_SPAN_TEXELS  = 31,
        WATER_SPIN_U16_FIRST_TEXEL_ROW = 224,
        WATER_SPIN_U16_LAST_TEXEL_ROW  = 255,
        WATER_SPIN_U16_QUARTER_TURN    = 0x400
    };
    void**                                 scratchCursor;
    _ShelterB1PodServiceGantrySpinScratch* scratchEnd;
    _ShelterB1PodServiceGantrySpinScratch* projection;
    s32*                                   depth;
    POLY_FT4*                              quad;
    s32                                    cellU;
    s32                                    lastU;
    s32                                    firstV;
    s32                                    cornerAngle;
    s32                                    perpendicularAngle;

    // Project the composed centre before constructing the screen-space quad.
    scratchCursor  = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd     = *scratchCursor;
    projection     = scratchEnd - 1;
    *scratchCursor = projection;
    // The GTE writes SZ3 / 4 through this pointer once the projection is accepted.
    depth = &projection->depth;
    memFillBytes(projection, 0, sizeof(*projection));
    projection->worldPoint.vx = (u16)coord->workm.t[0];
    projection->worldPoint.vy = (u16)coord->workm.t[1];
    projection->worldPoint.vz = (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&(scratchEnd - 1)->worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(depth);
        quad           = gGpuPrimCursor;
        cornerAngle    = spinAngle;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setShadeTex(quad, 1);
        setSemiTrans(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 768, 0);
        quad->clut  = getClut(304, 271);
        cellU       = textureColumn << WATER_SPIN_U16_CELL_SHIFT;
        firstV      = WATER_SPIN_U16_FIRST_TEXEL_ROW;
        lastU       = cellU + WATER_SPIN_U16_UV_SPAN_TEXELS;
        setUV4(quad, cellU, firstV, lastU, firstV, cellU, WATER_SPIN_U16_LAST_TEXEL_ROW, lastU, WATER_SPIN_U16_LAST_TEXEL_ROW);
        // Opposite corners share an offset; the second pair is a quarter turn away.
        _shelterB1PodServiceGantryComputeWaterCornerOffset(projection, radiusScale, cornerAngle);
        quad->x0           = projection->screenX + (u16)projection->cornerOffsetX;
        quad->x3           = projection->screenX - (u16)projection->cornerOffsetX;
        quad->y0           = projection->screenY - (u16)projection->cornerOffsetY;
        perpendicularAngle = cornerAngle + WATER_SPIN_U16_QUARTER_TURN;
        quad->y3           = projection->screenY + (u16)projection->cornerOffsetY;
        _shelterB1PodServiceGantryComputeWaterCornerOffset(projection, radiusScale, perpendicularAngle);
        quad->x1 = projection->screenX + (u16)projection->cornerOffsetX;
        quad->x2 = projection->screenX - (u16)projection->cornerOffsetX;
        quad->y1 = projection->screenY - (u16)projection->cornerOffsetY;
        quad->y2 = projection->screenY + (u16)projection->cornerOffsetY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)*depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_POP_AT(scratchCursor, _ShelterB1PodServiceGantrySpinScratch);
}

#include "../../shared/water_tile_u16_corners.inc.c"

/// Draws one upright, camera-facing cell of the eight-frame water-spray grid.
///
/// `coord` is borrowed with `workm` already composed in the input space of
/// `GsWSMATRIX`; drift tasks normally supply view-space translations. Each
/// translation keeps its low 16 bits as a signed GTE coordinate. `textureCell`
/// is unsigned. Its low three bits choose a 56-texel square from four columns
/// and two rows, starting at V=0. The 4-bit texture page is at VRAM (704, 0),
/// with its 16-colour palette at (304, 270).
///
/// `radiusScale * 55 / depth` gives the signed horizontal half-width `r` in
/// pixels. Drift tasks supply scale 0..4095. Signed division truncates toward
/// zero. The top edge is centre Y - r - (r >> 1), the bottom centre Y + (r >>
/// 1), placing the centre about a quarter of the height above the bottom edge.
/// Accepted projections require positive SZ3/4 depth; there is no extra check.
///
/// A nonnegative GTE FLAG queues one raw-texture, additive semi-transparent
/// `POLY_FT4`; the primitive cursor must have space for the complete packet.
/// One word-aligned `EffectCentreScratch` is reserved on the initialized
/// scratch stack and released on every path. No pointer is retained; GTE state
/// changes.
static void _waterDrawTileU16(const GfxCoord* coord, u16 textureCell, s16 radiusScale)
{
    enum {
        WATER_TILE_U16_COLUMNS           = 4,
        WATER_TILE_U16_CELLS             = 8,
        WATER_TILE_U16_ROW_SHIFT         = 2,
        WATER_TILE_U16_CELL_TEXELS       = 56,
        WATER_TILE_U16_UV_SPAN_TEXELS    = WATER_TILE_U16_CELL_TEXELS - 1,
        WATER_TILE_U16_FIRST_TEXEL_ROW   = 0,
        WATER_TILE_U16_PERSPECTIVE_SCALE = 55,
        WATER_TILE_U16_PACKET_CODE       = 0x2F
    };
    void**               scratchCursor;
    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    SVECTOR*             projectionPoint;
    u32                  cellIndex;
    s32                  firstU;
    s32                  firstV;
    s32                  lastU;
    s32                  lastV;

    // Project the composed centre before constructing the screen-space quad.
    scratchCursor  = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd     = *scratchCursor;
    projection     = scratchEnd - 1;
    *scratchCursor = projection;
    memFillBytes(projection, 0, sizeof(*projection));
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    projectionPoint           = &projection->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(projectionPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, WATER_TILE_U16_PACKET_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(304, 270);
        // Wrap the animation frame over the eight-cell texture grid.
        cellIndex = textureCell;
        firstU    = (cellIndex & (WATER_TILE_U16_COLUMNS - 1)) * WATER_TILE_U16_CELL_TEXELS;
        firstV    = ((cellIndex & (WATER_TILE_U16_CELLS - 1)) >> WATER_TILE_U16_ROW_SHIFT) * WATER_TILE_U16_CELL_TEXELS + WATER_TILE_U16_FIRST_TEXEL_ROW;
        lastU     = firstU + WATER_TILE_U16_UV_SPAN_TEXELS;
        lastV     = firstV + WATER_TILE_U16_UV_SPAN_TEXELS;
        setUV4(quad, firstU, firstV, lastU, firstV, firstU, lastV, lastU, lastV);
        projection->screenExtent = (radiusScale * WATER_TILE_U16_PERSPECTIVE_SCALE) / projection->depth;
        _waterSetUprightSpriteCorners(quad, projection);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_POP_AT(scratchCursor, EffectCentreScratch);
}

void shelterB1PodServiceGantryDrawChainGlow(const GfxCoord* coord, s32 chainNumber, s32 radiusScale, s16 packedColor)
{
    enum {
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_LOCAL_Y            = -196,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_PERSPECTIVE_SCALE  = 64,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_NIBBLE_BITS        = 4,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_CHANNEL_MASK       = 0xF,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_CHANNEL_BYTE_MASK  = 0xF0,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_FLICKER_SHIFT      = 12,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_TRIG_FRACTION_BITS = 12,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_EIGHTH_TURN        = 0x200,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_QUARTER_TURN       = 0x400,
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_FULL_TURN          = 0x1000,
    };
    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* projection;
    POLY_G4*             wedge;
    s32                  startAngle;
    s32                  midpointAngle;
    s32                  endAngle;
    s32                  flickerIncrement;
    s32                  packedColorValue;
    u16                  colorBits;
    u32                  channels;
    s32                  green;
    u8                   red;
    s32                  screenRadius;
    s16                  blue;

    // Reserve and clear the local-point projection workspace.
    scratchEnd                 = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    SCRATCH_STACK_CURSOR(void) = scratchEnd - 1;
    projection                 = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    packedColorValue           = packedColor;
    colorBits                  = packedColorValue;
    memFillBytes(projection, 0, sizeof(*projection));
    scratchEnd[-1].worldPoint.vx = 0;
    projection->worldPoint.vy    = SHELTER_B1_POD_SERVICE_GANTRY_GLOW_LOCAL_Y;
    projection->worldPoint.vz    = 0;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&projection->worldPoint);
    gte_rtv0();
    gte_stsv(&projection->worldPoint);
    scratchEnd[-1].worldPoint.vx = (u16)scratchEnd[-1].worldPoint.vx + (u16)coord->workm.t[0];
    projection->worldPoint.vy    = (u16)projection->worldPoint.vy + (u16)coord->workm.t[1];
    projection->worldPoint.vz    = (u16)projection->worldPoint.vz + (u16)coord->workm.t[2];

    // Project the halfword world point; rejected projections emit no packets.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].screenX);
    gte_stflg(&scratchEnd[-1].projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&scratchEnd[-1].depth);
        // Size the projected disc and expand RGB with its alternating increment.
        screenRadius             = ((s16)radiusScale * SHELTER_B1_POD_SERVICE_GANTRY_GLOW_PERSPECTIVE_SCALE) / projection->depth;
        startAngle               = 0;
        flickerIncrement         = (D_actor_560800_801752EC + (u8)D_shelter_b1_pod_service_gantry_8018256C[chainNumber & (ARRAY_SIZE(D_shelter_b1_pod_service_gantry_8018256C) - 1)]) & 1;
        channels                 = colorBits;
        flickerIncrement       <<= channels >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_FLICKER_SHIFT;
        red                      = flickerIncrement + ((channels >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_NIBBLE_BITS) & SHELTER_B1_POD_SERVICE_GANTRY_GLOW_CHANNEL_BYTE_MASK);
        green                    = flickerIncrement + (channels & SHELTER_B1_POD_SERVICE_GANTRY_GLOW_CHANNEL_BYTE_MASK);
        blue                     = flickerIncrement + ((packedColor & SHELTER_B1_POD_SERVICE_GANTRY_GLOW_CHANNEL_MASK) << SHELTER_B1_POD_SERVICE_GANTRY_GLOW_NIBBLE_BITS);
        projection->screenExtent = screenRadius;
        // Four quarter wedges share the lit centre and leave their rim black.
        /// Queues one additive quarter wedge and advances the angle by a quarter turn.
        ///
        /// Expands to several statements; invoke only in the braced loop below.
        /// Captures projection, wedge, the three angle locals and RGB locals, the
        /// frame packet cursor and current ordering table. Reads projection and
        /// angles repeatedly, writes wedge/angles and advances the packet cursor.
        /// Requires quad and blend-mode packet capacity; retains no scratch pointer.
#define SHELTER_B1_POD_SERVICE_GANTRY_DRAW_GLOW_WEDGE()                                                                                                    \
    wedge          = gGpuPrimCursor;                                                                                                                       \
    gGpuPrimCursor = wedge + 1;                                                                                                                            \
    setPolyG4(wedge);                                                                                                                                      \
    setRGB0(wedge, 0, 0, 0);                                                                                                                               \
    setRGB1(wedge, 0, 0, 0);                                                                                                                               \
    setRGB2(wedge, red, green, blue);                                                                                                                      \
    setRGB3(wedge, 0, 0, 0);                                                                                                                               \
    wedge->x0     = projection->screenX + ((projection->screenExtent * rsin(startAngle)) >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_TRIG_FRACTION_BITS);        \
    midpointAngle = startAngle + SHELTER_B1_POD_SERVICE_GANTRY_GLOW_EIGHTH_TURN;                                                                           \
    wedge->y0     = projection->screenY + ((projection->screenExtent * rcos(startAngle)) >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_TRIG_FRACTION_BITS);        \
    wedge->x1     = projection->screenX + ((projection->screenExtent * rsin(midpointAngle)) >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_TRIG_FRACTION_BITS);     \
    wedge->y1     = projection->screenY + ((projection->screenExtent * rcos(midpointAngle)) >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_TRIG_FRACTION_BITS);     \
    endAngle      = startAngle + SHELTER_B1_POD_SERVICE_GANTRY_GLOW_QUARTER_TURN;                                                                          \
    wedge->x2     = projection->screenX;                                                                                                                   \
    wedge->y2     = projection->screenY;                                                                                                                   \
    wedge->x3     = projection->screenX + ((projection->screenExtent * rsin(endAngle)) >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_TRIG_FRACTION_BITS);          \
    wedge->y3     = projection->screenY + ((projection->screenExtent * rcos(endAngle)) >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_TRIG_FRACTION_BITS);          \
    startAngle    = endAngle;                                                                                                                              \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            wedge);                                                                                                                                        \
    gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, projection->depth);

        do {
            SHELTER_B1_POD_SERVICE_GANTRY_DRAW_GLOW_WEDGE();
        } while (startAngle < SHELTER_B1_POD_SERVICE_GANTRY_GLOW_FULL_TURN);
#undef SHELTER_B1_POD_SERVICE_GANTRY_DRAW_GLOW_WEDGE
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#include "../../shared/effect_sprite_rise.inc.c"

/// Initializes a persistent alternating-glow phase for each of the room's eight chains.
///
/// Consumes eight draws from `gRandomLcgState`, advancing with unsigned 32-bit
/// wraparound and storing state bits 16..23 in each byte. The drawer indexes
/// the slots by chain number modulo eight and adds each byte's low bit to the
/// actor's tick parity. These phases persist until another initialization,
/// which replaces all eight bytes and consumes eight more draws.
static inline void _shelterB1PodServiceGantrySeedGlowFlicker(void)
{
    enum {
        SHELTER_B1_POD_SERVICE_GANTRY_GLOW_RANDOM_BYTE_SHIFT = 16
    };
    s32 flickerSlot;

    for (flickerSlot = 0; flickerSlot < ARRAY_SIZE(D_shelter_b1_pod_service_gantry_8018256C); flickerSlot++) {
        D_shelter_b1_pod_service_gantry_8018256C[flickerSlot] = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> SHELTER_B1_POD_SERVICE_GANTRY_GLOW_RANDOM_BYTE_SHIFT;
    }
}

void shelterB1PodServiceGantryInitEffectsTask(Task* task)
{
    enum {
        SHELTER_B1_POD_SERVICE_GANTRY_EFFECTS_STATE_NEW  = 0,
        SHELTER_B1_POD_SERVICE_GANTRY_EFFECTS_STATE_IDLE = 1
    };
    if (task->state == SHELTER_B1_POD_SERVICE_GANTRY_EFFECTS_STATE_NEW) {
        _shelterB1PodServiceGantrySeedGlowFlicker();
        gRoomEffectWaterSprayId = EFFECT_SHELTER_B1_POD_SERVICE_GANTRY_WATER_SPRAY;
        task->state             = SHELTER_B1_POD_SERVICE_GANTRY_EFFECTS_STATE_IDLE;
    }
}
