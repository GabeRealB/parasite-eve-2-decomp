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
/// Empty presence flag: this room supplies `waterDrawSpinU16` and
/// `waterDrawTileU16`.
///
/// Defined, with no replacement list, immediately before `water_effects.h`.
/// `defined()` is the only test. That header prototypes the shared drawers,
/// whose parameters are `s32`, only when `WATER_SHARED_U16_DRAWERS` is set
/// and this flag is not. This file prototypes and defines both drawers and
/// includes `water_drift_task_u16.inc.c` instead of the shared bodies. The
/// spin sprite uses texture page 0x2C. The tile grid starts at v 0 with clut
/// 0x4393. Both drawers take a `u16` index and an `s16` scale; the spin
/// drawer also takes an `s16` angle.
#define WATER_OWN_U16_DRAWERS
#include "../../shared/water_effects.h"
// Exported instance: another image refers to this package's copy by name.
#define effectSpriteRiseTask shelterB1PodServiceGantryEffectSpriteRiseTask
#include "../../shared/effect_sprite.h"

void waterDrawSpinU16(GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle);
void waterDrawTileU16(GfxCoord* arg0, u16 arg1, s16 arg2);

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

extern s32 D_801752EC;
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

void func_shelter_b1_pod_service_gantry_8017E880(Task* task)
{
    waterDriftTaskU16(task);
}

/// Draws a spinning, semi-transparent raw-texture quad around the projected coordinate.
///
/// `coord->workm` must already be current in the space `GsWSMATRIX` projects.
/// `textureColumn` selects a 32-texel column on texture row 0xE0..0xFF;
/// UV stores retain only their low byte. The projected radius is
/// `radiusScale * 31 / depth`, with a nonzero depth required. `spinAngle` uses
/// 0x1000 units per turn. The scratch block lives only during this draw.
void waterDrawSpinU16(GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle)
{
    void**                                          scratch;
    _ShelterB1PodServiceGantrySpinScratch*          head;
    _ShelterB1PodServiceGantrySpinScratch*          block;
    register _ShelterB1PodServiceGantrySpinScratch* depthBlock asm("s0");
    POLY_FT4*                                       prim;
    s32                                             u0;
    s32                                             u1;
    s32                                             v;
    s32                                             angle;
    s32                                             quarterTurnAngle;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    block    = head - 1;
    *scratch = block;
    memFillBytes(block, 0, sizeof(*block));
    block->worldPoint.vx = (u16)coord->workm.t[0];
    block->worldPoint.vy = (u16)coord->workm.t[1];
    block->worldPoint.vz = (u16)coord->workm.t[2];
    // Reuse the saved coordinate register after capturing the position.
    depthBlock = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&(head - 1)->worldPoint);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&depthBlock->depth);
        prim           = gGpuPrimCursor;
        angle          = spinAngle;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setShadeTex(prim, 1);
        setSemiTrans(prim, 1);
        prim->tpage = 0x2C;
        prim->clut  = 0x43D3;
        u0          = textureColumn << 5;
        v           = 0xE0;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, v, u1, v, u0, 0xFF, u1, 0xFF);
        block->cornerOffsetX = (((radiusScale * 31) / (head - 1)->depth) * rsin(angle)) >> 12;
        block->cornerOffsetY = (((radiusScale * 31) / (head - 1)->depth) * rcos(angle)) >> 12;
        prim->x0             = block->screenX + (u16)block->cornerOffsetX;
        prim->x3             = block->screenX - (u16)block->cornerOffsetX;
        prim->y0             = block->screenY - (u16)block->cornerOffsetY;
        quarterTurnAngle     = angle + 0x400;
        prim->y3             = block->screenY + (u16)block->cornerOffsetY;
        block->cornerOffsetX = (((radiusScale * 31) / (head - 1)->depth) * rsin(quarterTurnAngle)) >> 12;
        block->cornerOffsetY = (((radiusScale * 31) / (head - 1)->depth) * rcos(quarterTurnAngle)) >> 12;
        prim->x1             = block->screenX + (u16)block->cornerOffsetX;
        prim->x2             = block->screenX - (u16)block->cornerOffsetX;
        prim->y1             = block->screenY - (u16)block->cornerOffsetY;
        prim->y2             = block->screenY + (u16)block->cornerOffsetY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(head - 1)->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_AT(scratch, _ShelterB1PodServiceGantrySpinScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` into a
/// 0x18-byte scratch block zeroed with `memFillBytes` and, when the GTE flag is
/// non-negative, queues one shade-tex `POLY_FT4` (tpage 0x2B, clut 0x4393)
/// with a 56-texel UV tile picked by `arg1` and an on-screen radius of
/// `arg2 * 55 / depth`.
void waterDrawTileU16(GfxCoord* arg0, u16 arg1, s16 arg2)
{
    void**               scratch;
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    u32                  cell;
    s32                  tex;
    s32                  v0;
    s32                  u1;
    s32                  v1;
    s16                  xy;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    block    = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    *scratch = block;
    memFillBytes(block, 0, sizeof(*block));
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    vec                  = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x4393;
        cell        = arg1;
        tex         = (cell & 3) * 0x38;
        v0          = ((cell & 7) >> 2) * 0x38;
        u1          = tex + 0x37;
        v1          = v0 + 0x37;
        setUV4(prim, tex, v0, u1, v0, tex, v1, u1, v1);
        block->screenExtent = (arg2 * 55) / block->depth;
        xy                  = block->screenX - block->screenExtent;
        prim->x0 = prim->x2 = xy;
        xy                  = block->screenX + block->screenExtent;
        prim->x1 = prim->x3 = xy;
        xy                  = block->screenY - block->screenExtent - (block->screenExtent >> 1);
        prim->y0 = prim->y1 = xy;
        xy                  = block->screenY + (block->screenExtent >> 1);
        prim->y2 = prim->y3 = xy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(EffectCentreScratch));
}

/// Draws a glowing disc at the point (0, -0xC4, 0) in `arg0`'s local frame:
/// the point is rotated by `workm`, offset by its translation and projected
/// through `GsWSMATRIX` into a zeroed scratch block popped from
/// the scratch stack. When the GTE flag is non-negative, four Gouraud
/// `POLY_G4` quarter-wedges of radius `arg2 * 64 / depth` are queued, each lit
/// at the centre vertex and black on the rim. `arg3` packs the centre colour
/// as three 4-bit channels (red in bits 8-11, green 4-7, blue 0-3); a one-bit
/// flicker, taken from the global at 0x801752EC plus the per-slot byte
/// `arg1 & 7` of this room's random table, is shifted left by `arg3`'s top
/// nibble and added to every channel.
void func_shelter_b1_pod_service_gantry_8017F450(GfxCoord* arg0, s32 arg1, s32 arg2, s16 arg3)
{
    u8*                  head;
    EffectCentreScratch* block;
    POLY_G4*             prim;
    s32                  ang;
    s32                  t;
    s32                  t2;
    s32                  blend;
    s32                  color;
    u16                  color16;
    u32                  c;
    s32                  green;
    u8                   red;

    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - sizeof(EffectCentreScratch);
    block                      = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    color                      = arg3;
    color16                    = color;
    memFillBytes(block, 0, sizeof(*block));
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = 0;
    block->worldPoint.vy                                                        = -0xC4;
    block->worldPoint.vz                                                        = 0;
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(&block->worldPoint);
    gte_rtv0();
    gte_stsv(&block->worldPoint);
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = (u16)((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx + (u16)arg0->workm.t[0];
    block->worldPoint.vy                                                        = (u16)block->worldPoint.vy + (u16)arg0->workm.t[1];
    block->worldPoint.vz                                                        = (u16)block->worldPoint.vz + (u16)arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        arg2                = ((s16)arg2 * 64) / block->depth;
        ang                 = 0;
        blend               = (D_801752EC + (u8)D_shelter_b1_pod_service_gantry_8018256C[arg1 & 7]) & 1;
        c                   = color16;
        blend             <<= c >> 12;
        red                 = blend + ((c >> 4) & 0xF0);
        green               = blend + (c & 0xF0);
        arg3                = blend + ((arg3 & 0xF) << 4);
        block->screenExtent = arg2;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, red, green, arg3);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->screenExtent * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->screenY + ((block->screenExtent * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->screenExtent * rsin(t)) >> 12);
            prim->y1 = block->screenY + ((block->screenExtent * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->screenExtent * rsin(t2)) >> 12);
            prim->y3 = block->screenY + ((block->screenExtent * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#include "../../shared/effect_sprite_rise.inc.c"

void func_shelter_b1_pod_service_gantry_8017FA7C(Task* arg0)
{
    s32 i;

    if (arg0->state == 0) {
        for (i = 0; i < 8; i++) {
            D_shelter_b1_pod_service_gantry_8018256C[i] = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
        }
        gRoomEffectWaterSprayId = EFFECT_SHELTER_B1_POD_SERVICE_GANTRY_WATER_SPRAY;
        arg0->state             = 1;
    }
}
