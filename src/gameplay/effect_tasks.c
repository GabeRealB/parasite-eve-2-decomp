#include "gameplay/effect_tasks.h"

#include <psyq/sys/types.h>
#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/display.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"

/// GPU packet commands, angle units and spawn fields used by these drawers.
///
/// The textured-quad packet length excludes its one-word DMA tag.
enum {
    EFFECT_DRAW_ADDITIVE_TEXTURED_QUAD     = 0x2E,
    EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD = 0x2F,
    EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS = sizeof(POLY_FT4) / sizeof(u32) - 1,
    EFFECT_DRAW_FULL_TURN                  = 0x1000,
    EFFECT_DRAW_ANGLE_MASK                 = EFFECT_DRAW_FULL_TURN - 1,
    EFFECT_DRAW_SIZE_MASK                  = 0xFFF,
    EFFECT_DRAW_QUARTER_TURN               = EFFECT_DRAW_FULL_TURN / 4,
    EFFECT_DRAW_THREE_QUARTER_TURN         = EFFECT_DRAW_FULL_TURN * 3 / 4,
    EFFECT_DRAW_TASK_NEW                   = 0,
    EFFECT_DRAW_TASK_ACTIVE                = 1,
    EFFECT_CRITICAL_HIT_RING_ANGLE_STEP    = EFFECT_DRAW_FULL_TURN / 16,
    EFFECT_MUZZLE_SPARK_PERIOD_MASK        = 0xF0000,
    EFFECT_MUZZLE_SPARK_RANDOM_DRIFT       = 0x1000,
};

/// Texture cell dimensions and task lifetimes, in texels and ticks respectively.
/// UV span is cell size minus one; UV endpoints are inclusive.
enum {
    EFFECT_MUZZLE_FLARE_CELL_SIZE            = 40,
    EFFECT_MUZZLE_FLARE_TEXTURE_V            = 216,
    EFFECT_MUZZLE_FLARE_FRAME_COUNT          = 2,
    EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE   = 32,
    EFFECT_ADDITIVE_MUZZLE_FLARE_TEXTURE_V   = 56,
    EFFECT_MUZZLE_SPARK_CELL_SIZE            = 24,
    EFFECT_MUZZLE_SPARK_TEXTURE_U            = 48,
    EFFECT_MUZZLE_SPARK_FRAME_COUNT          = 4,
    EFFECT_THROWN_MUZZLE_SPARK_CELL_SIZE     = 32,
    EFFECT_THROWN_MUZZLE_SPARK_TEXTURE_V     = 24,
    EFFECT_THROWN_MUZZLE_SPARK_FRAME_COUNT   = 8,
    EFFECT_IMPACT_SPARK_CELL_SIZE            = 24,
    EFFECT_MUZZLE_FLASH_UV_SPAN              = 55,
    EFFECT_SHOTGUN_SPARK_LINE_TICKS          = 4,
    EFFECT_PIXEL_SPARK_TICKS                 = 8,
    EFFECT_RED_GROUND_GLOW_TICKS             = 1024,
    EFFECT_RED_GROUND_GLOW_DEFAULT_HALF_SIZE = 1024,
    EFFECT_RED_GROUND_GLOW_DEPTH_BIAS        = 128,
    EFFECT_BOUNCING_SPARK_CELL_SIZE          = 16,
    EFFECT_BOUNCING_SPARK_UV_SPAN            = EFFECT_BOUNCING_SPARK_CELL_SIZE - 1,
};

/// Shared settings for weapon-flash profiles, size jitter and transient lighting.
///
/// Profiles index 34-entry local-offset tables. Light radii use world units;
/// intensity uses Q12. The inner radius shrinks with a fixed outer radius.
enum {
    EFFECT_WEAPON_FLASH_STATE_WAIT         = 2,
    EFFECT_WEAPON_FLASH_PROFILE_P08        = 1,
    EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK   = 0x1FF,
    EFFECT_WEAPON_FLASH_LIGHT_INTENSITY    = ONE * 3 / 4,
    EFFECT_WEAPON_FLASH_LIGHT_INNER_RADIUS = 4000,
    EFFECT_WEAPON_FLASH_LIGHT_OUTER_RADIUS = 4800,
    EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP  = 400,
    EFFECT_WEAPON_FLASH_LIGHT_TICKS        = 4,
    EFFECT_WEAPON_FLASH_LAST_AGE           = 4,
    EFFECT_RIFLE_FLASH_SIZE                = 896,
};

/// Packed critical-hit style bit enabling four additive radial spikes.
///
/// Bit 12 is separate from the three RGB weight nibbles. The spikes share the
/// ring's brightness-scaled colour and fade to black at their outer corners.
#define EFFECT_CRITICAL_HIT_STYLE_SPIKES 0x1000

/// Packs critical-hit RGB brightness weights into bits 8..11, 4..7 and 0..3.
///
/// Inputs are integers in 0..15, evaluated once each without masking or casts.
/// Returns an integer constant expression for style-table initialization;
/// bit 12 remains clear for `EFFECT_CRITICAL_HIT_STYLE_SPIKES`. The drawer
/// multiplies each weight by the current brightness to obtain an 8-bit channel.
#define EFFECT_CRITICAL_HIT_STYLE_COLOR(red, green, blue) (((red) << 8) | ((green) << 4) | (blue))

/// One look of the `EFFECT_CRITICAL_HIT` burst, selected by the effect's spawn
/// argument.
///
/// The burst is an additive ring that grows for eight frames while it fades
/// from its colour to black. Each colour channel is its weight multiplied by a
/// brightness that falls from 14 to 0. The ring's radius starts at 0x20 and is
/// in units of 256 / depth pixels at its outer edge; the inner edge sits at
/// half that distance.
typedef struct {
    u16 color;      // `EFFECT_CRITICAL_HIT_STYLE_COLOR` channel weights, with `EFFECT_CRITICAL_HIT_STYLE_SPIKES` where the burst has spikes
    u16 radiusStep; // amount the ring's radius grows each frame
} _EffectCriticalHitStyle;
STATIC_ASSERT_SIZEOF(_EffectCriticalHitStyle, 4);

/// One frame of the `EFFECT_IMPACT_FLASH` animation: a square texture cell with
/// the texture page and palette it is drawn from.
///
/// The flash shows one frame per tick, and consecutive frames can come from
/// different texture pages. `u` and `v` are texels within the page at `tpageX`;
/// the record holds no page Y, and its pages sit on the top row of VRAM. The
/// page and palette coordinates are unencoded VRAM coordinates for `getTPage`
/// and `getClut`.
///
/// `uvSpan` is the distance between the cell's outermost texels, not its side,
/// so a cell ends at `u + uvSpan` and `v + uvSpan`, which must stay within the
/// page (0..255). It is also the distance from the drawn quad's centre to each
/// corner, in the units the effect's scale and the perspective divide act on,
/// so a larger cell is drawn larger.
///
/// Texture coordinates are eight bits wide when drawn. The high byte of `u` and
/// `v` is zero in every frame and has no proven role of its own.
typedef struct {
    u16 uvSpan; // Texels from the cell's first column or row to its last: its side minus one
    u16 u;      // Left texture column in texels (0..255)
    u16 v;      // Top texture row in texels (0..255)
    u16 clutX;  // Palette X in VRAM words, aligned to 16 words
    u16 clutY;  // Palette Y in VRAM scanlines
    u16 tpageX; // Texture page X in VRAM words, aligned to 64 words
} _EffectImpactFlashFrame;
STATIC_ASSERT_SIZEOF(_EffectImpactFlashFrame, 0xC);

/// Scratch-stack workspace for projecting one `EFFECT_PIXEL_SPARK` onto its tile.
///
/// The spark's task stages its coordinate's world position, narrowed to signed
/// 16-bit coordinate units, and one perspective transform through the
/// world-to-screen matrix supplies the screen position, the GTE status and the
/// depth. A rejected projection queues no tile and leaves `depth` unwritten.
///
/// Reserve one complete, word-aligned block on the scratch stack and release
/// it in reverse order after drawing. The block is not cleared; the SVECTOR's
/// unused fourth halfword has no established value. No pointer into this
/// workspace may survive its release.
typedef struct {
    SVECTOR worldPoint;      // Spark position in world coordinates, narrowed to s16
    s32     depth;           // SZ3 / 4 plus one, used for ordering-table placement and blend setup
    s32     projectionFlags; // GTE FLAG word; bit 31 makes it negative and rejects the projection
    DVECTOR screenPoint;     // Projected position in pixels, the tile's top-left corner; one GTE word store fills both halves
} _EffectPixelSparkScratch;
STATIC_ASSERT_SIZEOF(_EffectPixelSparkScratch, 0x14);

static const _EffectCriticalHitStyle _gEffectCriticalHitStyles[6];

static const _EffectImpactFlashFrame _gEffectImpactFlashFrames[4];

static SVECTOR _gEffectWeaponMuzzleOffsets[34];

static const SVECTOR _gEffectWeaponEmissionOffsets[34];

static const SVECTOR _gEffectGrenadeMuzzleOffsets[34];

static SVECTOR _gEffectGrenadeEmissionOffsets[34];

static void _effectDrawMuzzleFlash(const GfxCoord* coord, s32 size, s32 angle);

static void _effectDrawImpactSparkFlash(const GfxCoord* coord, u16 frame, s16 size, s16 angle);

static void _effectDrawCriticalHitBurst(const GfxCoord* coord, s16 radius, s16 brightness, u16 color);

/// Sets an explosion quad's palette and 40-by-40 atlas texture rectangle.
///
/// `textureFrame` borrows one `gEffectSpriteAtlasFrames` entry; `quad` must
/// be writable. U/V are page-relative texels, with inclusive endpoints 39
/// texels beyond the origin, in top-left, top-right, bottom-left, bottom-right
/// order. Palette X/Y are VRAM words/scanlines, encoded into the packet's CLUT.
/// The caller selects the texture page, blend command and geometry.
static inline void _effectSetExplosionTexture(POLY_FT4* quad, const EffectSpriteTextureFrame* textureFrame)
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

/// Sets the shotgun flash light at the effect's initial position.
///
/// Borrow `spawnCoord` before attaching it to the weapon muzzle. Its local
/// translation and the initialized `pointLight` must share the view parent.
/// Sets Q12 RGB intensity (7/8, 5/8, 5/8), full-strength radius 4000 and outer
/// radius 4800 in game coordinate units, and marks placement dirty.
/// The caller enables the shared transient slot and manages its lifetime.
static inline void _effectInitializeShotgunLight(const GfxCoord* spawnCoord, WorldCoordPointLight* pointLight)
{
    enum { EFFECT_SHOTGUN_FLASH_RED_INTENSITY   = ONE * 7 / 8,
           EFFECT_SHOTGUN_FLASH_OTHER_INTENSITY = ONE * 5 / 8 };
    s32 spawnZ;
    pointLight->head.transform.coord.coord.t[0]   = spawnCoord->coord.t[0];
    pointLight->head.transform.coord.coord.t[1]   = spawnCoord->coord.t[1];
    spawnZ                                        = spawnCoord->coord.t[2];
    pointLight->head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    pointLight->head.color.r                      = EFFECT_SHOTGUN_FLASH_RED_INTENSITY;
    pointLight->head.color.g                      = EFFECT_SHOTGUN_FLASH_OTHER_INTENSITY;
    pointLight->head.color.b                      = EFFECT_SHOTGUN_FLASH_OTHER_INTENSITY;
    pointLight->inner                             = EFFECT_WEAPON_FLASH_LIGHT_INNER_RADIUS;
    pointLight->outer                             = EFFECT_WEAPON_FLASH_LIGHT_OUTER_RADIUS;
    pointLight->head.transform.coord.coord.t[2]   = spawnZ;
}

/// Refreshes a grenade burst's point light while its full-strength radius contracts.
///
/// All pointers are borrowed. `pointLight` must be `&transientLight->light`
/// and `lightCoord` must be `&pointLight->head.transform.coord`. The initialized
/// light and `blastCoord` must share the view parent. `ageCounter` supplies a
/// readable signed-halfword frame count, 1..21 for the burst's live updates.
/// RGB inputs use Q12. Sets inner radius to 8800 - 400 * age and outer radius
/// to 9600 in game coordinate units, copies local translation, marks placement
/// dirty and refreshes expiry to sixteen unpaused transient-light updates.
static inline void _effectRefreshGrenadeBurstLight(WorldCoordTransientPointLight* transientLight, WorldCoordPointLight* pointLight, GfxCoord* lightCoord, const GfxCoord* blastCoord, const s16* ageCounter, s16 redIntensity, s16 greenIntensity, s16 blueIntensity)
{
    enum {
        EFFECT_GRENADE_BURST_LIGHT_TICKS    = 16,
        EFFECT_GRENADE_BURST_OUTER_RADIUS   = 9600,
        EFFECT_GRENADE_BURST_RADIUS_QUARTER = 2200,
        EFFECT_GRENADE_BURST_RADIUS_STEP    = 100,
    };
    s32 age;
    s32 spawnZ;
    transientLight->framesLeft = EFFECT_GRENADE_BURST_LIGHT_TICKS;
    age                        = *ageCounter;
    pointLight->outer          = EFFECT_GRENADE_BURST_OUTER_RADIUS;
    pointLight->head.color.r   = redIntensity;
    pointLight->head.color.g   = greenIntensity;
    pointLight->head.color.b   = blueIntensity;
    pointLight->inner          = (EFFECT_GRENADE_BURST_RADIUS_QUARTER - (age * EFFECT_GRENADE_BURST_RADIUS_STEP)) * 4;
    lightCoord->coord.t[0]     = blastCoord->coord.t[0];
    lightCoord->coord.t[1]     = blastCoord->coord.t[1];
    spawnZ                     = blastCoord->coord.t[2];
    lightCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    lightCoord->coord.t[2]     = spawnZ;
}

/// Places the four corners of one sixteenth-turn critical-hit ring segment.
///
/// segmentAngle is 0..3840 in 4096-unit turns; returns the angle plus 256.
/// scratch supplies the projected centre and signed integer pixel radii in
/// extent.burst. Corners 0/1 follow the outer radius and 2/3 the inner radius,
/// in GPU strip order. Q12 sine/cosine products shift arithmetically; each
/// resulting pixel coordinate narrows to the packet's signed halfword.
/// Borrows both pointers, writes only quad's XY fields and consumes no arena
/// or scratch-stack storage. Packet colours and its DMA tag are untouched.
static inline s32 _effectSetCriticalHitRingCorners(POLY_G4* quad, const EffectShapeScratch* scratch, s32 segmentAngle)
{
    enum { EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS = 12 };
    s32 nextSegmentAngle;

    quad->x0         = scratch->screenX + ((scratch->extent.burst.outer * rsin(segmentAngle)) >> EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS);
    nextSegmentAngle = segmentAngle + EFFECT_CRITICAL_HIT_RING_ANGLE_STEP;
    quad->y0         = scratch->screenY + ((scratch->extent.burst.outer * rcos(segmentAngle)) >> EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS);
    quad->x1         = scratch->screenX + ((scratch->extent.burst.outer * rsin(nextSegmentAngle)) >> EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS);
    quad->y1         = scratch->screenY + ((scratch->extent.burst.outer * rcos(nextSegmentAngle)) >> EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS);
    quad->x2         = scratch->screenX + ((scratch->extent.burst.inner * rsin(segmentAngle)) >> EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS);
    quad->y2         = scratch->screenY + ((scratch->extent.burst.inner * rcos(segmentAngle)) >> EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS);
    quad->x3         = scratch->screenX + ((scratch->extent.burst.inner * rsin(nextSegmentAngle)) >> EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS);
    quad->y3         = scratch->screenY + ((scratch->extent.burst.inner * rcos(nextSegmentAngle)) >> EFFECT_CRITICAL_HIT_TRIG_FRACTION_BITS);
    return nextSegmentAngle;
}

/// Projects the ground-shadow quad from world coordinates to screen pixels.
///
/// quadScratch is a live, word-aligned block with all four vertices' XYZ
/// components initialized in signed 16-bit world coordinates, in GPU quad
/// strip order. The caller
/// supplies the GTE projection settings and `GsWSMATRIX`'s translation; this
/// loads that matrix's rotation. The block is borrowed only for this call.
///
/// Writes all four `screenCorners` and the triple transform's `projectionFlags`,
/// even on rejection. Corner 0's single-transform flags are not accumulated;
/// the caller rejects a negative final FLAG. Leaves the depth member untouched
/// and corner 3's depth in GTE SZ3, to be read before another depth-changing
/// GTE command.
static inline void _effectProjectGroundShadow(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&GsWSMATRIX);
    // Save corner 0 before the triple transform replaces the screen FIFO.
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Seeds and rotates a randomized sideways model-ejection vector.
///
/// Before rotation, XYZ range over 96..159, -111..16 and 0..127, respectively.
/// Uses exactly three consecutive shared LCG draws. `work` must be writable
/// and its borrowed `parent` live with the intended local Q12 rotation.
/// Only `move`'s XYZ are written; its fourth halfword is untouched. The result
/// is not a unit vector: the caller normalizes it to Q12 before applying speed.
/// Changes the GTE rotation and working registers; no translation is applied.
static inline void _effectChooseThrownModelDirection(EffectWork* work)
{
    RANDOM_LCG_ADVANCE();
    work->move.vx = (RANDOM_LCG_DRAW() & 0x3F) + 0x60;
    RANDOM_LCG_ADVANCE();
    work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x7F);
    RANDOM_LCG_ADVANCE();
    work->move.vz = RANDOM_LCG_DRAW() & 0x7F;
    gte_SetRotMatrix(&work->parent->coord);
    gte_ldv0(&work->move);
    gte_rtv0();
    gte_stsv(&work->move);
}

/// Projects a staged effect centre through the current screen matrix.
///
/// `scratch` must be a live, word-aligned `EffectShapeScratch` with XYZ staged
/// in `worldPoint` in `GsWSMATRIX`'s input space, in signed 16-bit coordinate
/// units. These task coordinates compose through the view before staging.
/// Requires the current GTE projection settings. Loads the screen rotation
/// and translation and writes raw `screenX`/`screenY` halfwords with one word
/// store, then `projectionFlags`, even when FLAG bit 31 rejects the point.
/// Leaves `depth` and `extent` untouched and SZ3 ready for the caller's depth
/// read before another depth-changing GTE command. Borrows the block only
/// for this call and does not allocate or queue a GPU packet.
static inline void _effectProjectShapeCentre(EffectShapeScratch* scratch)
{
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
}

/// Fractional bits returned by the sine and cosine tables used for sprite rotation.
enum { EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS = 12 };

/// Places a rotated sprite quad around its projected centre.
///
/// `quad`, `scratch` and `work` are side-effect-free pointer expressions to
/// POLY_FT4, EffectShapeScratch and const EffectWork storage, respectively.
/// `uvSpan` is a side-effect-free inclusive texture-cell span in 1..55 texels;
/// work->scale supplies size in 0..4095 and work->angle rotation in 4096-unit
/// turns. Arguments are evaluated repeatedly and no caller locals are captured.
/// Scratch must contain a projected centre and positive depth (SZ3 / 4 + 1).
/// The half-diagonal is size * uvSpan / depth pixels, divided before Q12
/// rotation. Writes all quad XY fields and reuses extent.corner for the two
/// opposite pairs, leaving the quarter-turn pair on return. Packet coordinates
/// narrow to signed 16 bits. All storage is borrowed for this statement.
/// Expands to several statements; use only within a braced block. Keeping
/// the sequence in its caller lets the compiler fold constant spans and
/// schedule packet submission between the final coordinate stores.
#define EFFECT_SET_ROTATED_SPRITE_CORNERS(quad, scratch, work, uvSpan)                                                                                                           \
    (scratch)->extent.corner.x = ((((uvSpan) * (work)->scale) / (scratch)->depth) * rsin((work)->angle)) >> EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS;                            \
    (scratch)->extent.corner.y = ((((uvSpan) * (work)->scale) / (scratch)->depth) * rcos((work)->angle)) >> EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS;                            \
    (quad)->x0                 = (scratch)->screenX + (scratch)->extent.corner.x;                                                                                                \
    (quad)->x3                 = (scratch)->screenX - (scratch)->extent.corner.x;                                                                                                \
    (quad)->y0                 = (scratch)->screenY - (scratch)->extent.corner.y;                                                                                                \
    (quad)->y3                 = (scratch)->screenY + (scratch)->extent.corner.y;                                                                                                \
    (scratch)->extent.corner.x = ((((uvSpan) * (work)->scale) / (scratch)->depth) * rsin((work)->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS; \
    (scratch)->extent.corner.y = ((((uvSpan) * (work)->scale) / (scratch)->depth) * rcos((work)->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS; \
    (quad)->x1                 = (scratch)->screenX + (scratch)->extent.corner.x;                                                                                                \
    (quad)->x2                 = (scratch)->screenX - (scratch)->extent.corner.x;                                                                                                \
    (quad)->y1                 = (scratch)->screenY - (scratch)->extent.corner.y;                                                                                                \
    (quad)->y2                 = (scratch)->screenY + (scratch)->extent.corner.y;

/// Places a rotated puff's two opposite-corner pairs around its screen centre.
///
/// Arguments must be side-effect-free POLY_FT4*, EffectShapeScratch*,
/// const EffectWork* and inclusive texel-span expressions, respectively.
/// work->scale is a nonnegative signed-halfword size and angle a 4096-unit
/// rotation; uvSpan is 1..39. Scratch holds a projected centre and positive
/// depth, with the caller's bias already applied. The half-diagonal is
/// size * span / depth pixels before Q12 rotation. Writes quad XY, narrowing
/// to signed halfwords, and scratch corner offsets; leaves the quarter-turn
/// pair there. Borrows storage and queues no packet. Arguments occur
/// repeatedly; no caller locals are captured. Use as a standalone compound statement inside braces. A constant
/// span stays visible to multiplication folding at each use. Signed word
/// intermediates must fit before the final packet-coordinate truncation.
#define EFFECT_SET_PUFF_CORNERS(quad, scratch, work, uvSpan)                                                                                                                         \
    {                                                                                                                                                                                \
        (scratch)->extent.corner.x = ((((work)->scale * (uvSpan)) / (scratch)->depth) * rsin((work)->angle)) >> EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS;                            \
        (scratch)->extent.corner.y = ((((work)->scale * (uvSpan)) / (scratch)->depth) * rcos((work)->angle)) >> EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS;                            \
        (quad)->x0                 = (scratch)->screenX + (scratch)->extent.corner.x;                                                                                                \
        (quad)->x3                 = (scratch)->screenX - (scratch)->extent.corner.x;                                                                                                \
        (quad)->y0                 = (scratch)->screenY - (scratch)->extent.corner.y;                                                                                                \
        (quad)->y3                 = (scratch)->screenY + (scratch)->extent.corner.y;                                                                                                \
        (scratch)->extent.corner.x = ((((work)->scale * (uvSpan)) / (scratch)->depth) * rsin((work)->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS; \
        (scratch)->extent.corner.y = ((((work)->scale * (uvSpan)) / (scratch)->depth) * rcos((work)->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_ROTATED_SPRITE_TRIG_FRACTION_BITS; \
        (quad)->x1                 = (scratch)->screenX + (scratch)->extent.corner.x;                                                                                                \
        (quad)->x2                 = (scratch)->screenX - (scratch)->extent.corner.x;                                                                                                \
        (quad)->y1                 = (scratch)->screenY - (scratch)->extent.corner.y;                                                                                                \
        (quad)->y2                 = (scratch)->screenY + (scratch)->extent.corner.y;                                                                                                \
    }

/// Texture-cell side in texels, shared by the dust animation and corner sizing.
enum { EFFECT_DUST_PUFF_CELL_SIZE = 32 };

/// Places a dust puff's two pairs of opposite corners around its projected centre.
///
/// Borrows a writable POLY_FT4, a live word-aligned projection block and work.
/// scratch->depth must be the accepted RTPS's positive, unmodified SZ3 / 4;
/// work->scale is size in 0..4095 and angle a rotation in 4096 units per turn.
/// The half-diagonal is size * 31 / depth pixels before Q12 rotation. Corner
/// offsets narrow to the packet's signed halfword after addition to the raw
/// screen coordinates.
/// Writes only quad XY and scratch extent.corner, leaving the quarter-turn
/// offsets there on return. Neither reserves storage nor queues a packet;
/// the pointers need remain live only for this call.
static inline void _effectSetDustPuffCorners(POLY_FT4* quad, EffectShapeScratch* scratch, const EffectWork* work)
{
    EFFECT_SET_PUFF_CORNERS(quad, scratch, work, EFFECT_DUST_PUFF_CELL_SIZE - 1);
}

/// Initializes the shared handgun point light at the effect's spawn position.
///
/// Borrows a live spawnCoord and the point-light member of an initialized
/// transient slot. Their translations share the view-parent frame; placement
/// is sampled before the muzzle is attached. Retains the light's parent and
/// rotation. Writes three-quarter Q12 white,
/// full-strength radius 4000 and outer radius 4800 in world units, invalidating
/// composition. Leaves expiry unchanged for the weapon-profile branch.
static inline void _effectInitializeHandgunLight(const GfxCoord* spawnCoord, WorldCoordPointLight* pointLight)
{
    s32 spawnZ;
    pointLight->head.transform.coord.coord.t[0]   = spawnCoord->coord.t[0];
    pointLight->head.transform.coord.coord.t[1]   = spawnCoord->coord.t[1];
    spawnZ                                        = spawnCoord->coord.t[2];
    pointLight->head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    pointLight->head.color.r                      = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
    pointLight->head.color.g                      = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
    pointLight->head.color.b                      = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
    pointLight->inner                             = EFFECT_WEAPON_FLASH_LIGHT_INNER_RADIUS;
    pointLight->outer                             = EFFECT_WEAPON_FLASH_LIGHT_OUTER_RADIUS;
    pointLight->head.transform.coord.coord.t[2]   = spawnZ;
}

/// Attaches the reload emitter at its profile offset plus copied spawn XYZ.
///
/// ejectionCoord and work are side-effect-free GfxCoord* and const EffectWork*
/// expressions; weaponProfile is a side-effect-free 0..33 table index. Each
/// argument is evaluated repeatedly, preserving separate index reads between
/// translation stores. Borrows all storage and the work's live parent, retains
/// rotation and invalidates composition without composing. Offsets use signed
/// parent-local coordinate units. No caller locals are captured. Use as one
/// statement; the parent remains borrowed for the emitter's lifetime.
#define EFFECT_ATTACH_RELOAD_EMITTER(ejectionCoord, work, weaponProfile)                   \
    {                                                                                      \
        (ejectionCoord)->parent       = (work)->parent;                                    \
        (ejectionCoord)->coord.t[0]   = _gEffectWeaponEmissionOffsets[(weaponProfile)].vx; \
        (ejectionCoord)->coord.t[1]   = _gEffectWeaponEmissionOffsets[(weaponProfile)].vy; \
        (ejectionCoord)->coord.t[2]   = _gEffectWeaponEmissionOffsets[(weaponProfile)].vz; \
        (ejectionCoord)->coord.t[0]  += (work)->pos.vx;                                    \
        (ejectionCoord)->coord.t[1]  += (work)->pos.vy;                                    \
        (ejectionCoord)->coord.t[2]  += (work)->pos.vz;                                    \
        (ejectionCoord)->composeStamp = GRAPHICS_COORD_DIRTY;                              \
    }

// Preserve the initialized-data placement of the read-only metadata tables.
const EffectUnitQuadCorner gEffectUnitQuadCorners[4] __attribute__((section(".data"))) = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};
/// The twelve explosion frames of the resident effect atlas (rooms/akropolis/west_elevator_hall/effect_atlas, cells
/// `explosion_*`), each with its own palette on row 265 (its `explosion_*` variants).
// Preserve the initialized-data placement of this shared read-only table.
const EffectSpriteTextureFrame gEffectSpriteAtlasFrames[12] __attribute__((section(".data"))) = {
    { 0, 0, 0, 0, 112, 265 },
    { EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 128, 265 },
    { 2 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 144, 265 },
    { 3 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 160, 265 },
    { 4 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 176, 265 },
    { 5 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 192, 265 },
    { 0, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 208, 265 },
    { EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 224, 265 },
    { 2 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 240, 265 },
    { 3 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 256, 265 },
    { 4 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 272, 265 },
    { 5 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 288, 265 },
};
const u16 gEffectBillboardClutX[6] __attribute__((section(".data"))) = {
    32,
    48,
    192,
    208,
    224,
    240,
};
const u16 gEffectModulatedBillboardClutX[6] __attribute__((section(".data"))) = {
    80,
    176,
    256,
    272,
    288,
    304,
};
enum { EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT = 12 };
const u16 gEffectStatusScreenTints[8] __attribute__((section(".data"))) = {
    (GPU_BLEND_SUBTRACT << EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT) | 0xCCC, // Darkness
    (GPU_BLEND_ADD << EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT) | 0x33C,      // Paralysis
    (GPU_BLEND_SUBTRACT << EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT) | 0x3C3, // Poison
    (GPU_BLEND_SUBTRACT << EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT) | 0x36C, // Reaction 4
    (GPU_BLEND_SUBTRACT << EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT) | 0xC63, // Silence
    (GPU_BLEND_ADD << EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT) | 0xC63,      // Timed status 0x20; identity unproven
    (GPU_BLEND_SUBTRACT << EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT) | 0x33C, // Confusion
    (GPU_BLEND_ADD << EFFECT_STATUS_SCREEN_TINT_BLEND_SHIFT) | 0xC36,      // Berserker
};

void effectControlTask2B(Task* task)
{
    enum {
        EFFECT_WEAPON_FLASH_PROFILE_M93R            = 2,
        EFFECT_WEAPON_FLASH_PROFILE_M950            = 3,
        EFFECT_WEAPON_FLASH_PROFILE_P229            = 5,
        EFFECT_WEAPON_FLASH_PROFILE_MP5A5           = 30,
        EFFECT_WEAPON_FLASH_PROFILE_MP5A5_UPGRADE_1 = 31,
        EFFECT_WEAPON_FLASH_PROFILE_MP5A5_UPGRADE_2 = 32,
        EFFECT_WEAPON_FLASH_PROFILE_NPC_PISTOL      = 33,
        EFFECT_SMG_FLASH_LAST_AGE                   = 2,
        EFFECT_WEAPON_FLASH_SMALL_SIZE              = 512,
        EFFECT_WEAPON_FLASH_MEDIUM_SIZE             = 768,
    };
    EffectWork*                    work;
    GfxCoord*                      muzzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    s32                            burstSuppressed;
    s32                            weaponProfile;
    s32                            randomState;
    s32                            age;

    work           = task->spawnArg2.pointer;
    muzzleCoord    = task->extra.coordBody->coord;
    transientLight = gWorldCoordTransientPointLights;
    pointLight     = &transientLight->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case EFFECT_DRAW_TASK_NEW:
                burstSuppressed       = task->spawnArg1.halves.high;
                work->index           = burstSuppressed;
                task->spawnArg1.value = (u8)task->spawnArg1.value;
                // Light the spawn position before attaching the effect to the weapon.
                _effectInitializeHandgunLight(muzzleCoord, pointLight);
                muzzleCoord->parent       = work->parent;
                muzzleCoord->coord.t[0]   = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vx;
                muzzleCoord->coord.t[1]   = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vy;
                muzzleCoord->coord.t[2]   = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vz;
                muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(muzzleCoord);
                switch (task->spawnArg1.value) {
                    case EFFECT_WEAPON_FLASH_PROFILE_P08:
                    default:
                        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
                        gRandomLcgState = randomState;
                        effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, (((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) | EFFECT_WEAPON_FLASH_SMALL_SIZE, 0);
                        weaponProfile = task->spawnArg1.value;
                        task->state   = EFFECT_DRAW_TASK_ACTIVE;
                        effectSpawn(EFFECT_BULLET_CASING, muzzleCoord, weaponProfile, &_gEffectWeaponEmissionOffsets[weaponProfile]);
                        work->scale                = EFFECT_WEAPON_FLASH_LAST_AGE;
                        transientLight->framesLeft = EFFECT_WEAPON_FLASH_LIGHT_TICKS;
                        break;
                    case EFFECT_WEAPON_FLASH_PROFILE_M93R:
                    case EFFECT_WEAPON_FLASH_PROFILE_M950:
                        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
                        gRandomLcgState = randomState;
                        effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, (((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_WEAPON_FLASH_MEDIUM_SIZE, 0);
                        weaponProfile = task->spawnArg1.value;
                        task->state   = EFFECT_DRAW_TASK_ACTIVE;
                        effectSpawn(EFFECT_BULLET_CASING, muzzleCoord, weaponProfile, &_gEffectWeaponEmissionOffsets[weaponProfile]);
                        work->scale                = EFFECT_WEAPON_FLASH_LAST_AGE;
                        transientLight->framesLeft = EFFECT_WEAPON_FLASH_LIGHT_TICKS;
                        break;
                    case EFFECT_WEAPON_FLASH_PROFILE_MP5A5:
                    case EFFECT_WEAPON_FLASH_PROFILE_MP5A5_UPGRADE_1:
                    case EFFECT_WEAPON_FLASH_PROFILE_MP5A5_UPGRADE_2:
                        task->state     = EFFECT_WEAPON_FLASH_STATE_WAIT;
                        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
                        gRandomLcgState = randomState;
                        effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, (((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_WEAPON_FLASH_MEDIUM_SIZE, 0);
                        weaponProfile = task->spawnArg1.value;
                        effectSpawn(EFFECT_BULLET_CASING, muzzleCoord, weaponProfile, &_gEffectWeaponEmissionOffsets[weaponProfile]);
                        work->scale                = EFFECT_SMG_FLASH_LAST_AGE;
                        transientLight->framesLeft = EFFECT_SMG_FLASH_LAST_AGE;
                        break;
                    case EFFECT_WEAPON_FLASH_PROFILE_P229:
                        weaponProfile = task->spawnArg1.value;
                        task->state   = EFFECT_DRAW_TASK_ACTIVE;
                        effectSpawn(EFFECT_P229_SHELL_CASING, muzzleCoord, weaponProfile, &_gEffectWeaponEmissionOffsets[weaponProfile]);
                        work->scale                = EFFECT_WEAPON_FLASH_LAST_AGE;
                        transientLight->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
                        break;
                    case EFFECT_WEAPON_FLASH_PROFILE_NPC_PISTOL:
                        work->index     = 1;
                        task->state     = EFFECT_DRAW_TASK_ACTIVE;
                        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
                        gRandomLcgState = randomState;
                        effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, (((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_WEAPON_FLASH_MEDIUM_SIZE, 0);
                        weaponProfile = task->spawnArg1.value;
                        effectSpawn(EFFECT_P229_SHELL_CASING, muzzleCoord, weaponProfile, &_gEffectWeaponEmissionOffsets[weaponProfile]);
                        work->scale                = EFFECT_WEAPON_FLASH_LAST_AGE;
                        transientLight->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
                        break;
                }
                if (work->index == 0) {
                    gRoomEffectState->burstRequest = true;
                }
                break;
            case EFFECT_DRAW_TASK_ACTIVE:
                randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState = randomState;
                effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) | EFFECT_WEAPON_FLASH_SMALL_SIZE, 0);
                task->state++;
                break;
        }
        // Shrink the full-strength radius; retain the fixed outer falloff boundary.
        if (pointLight->inner >= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP + 1) {
            pointLight->inner -= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP;
        }
        age = work->age;
        if (work->scale < age) {
            effectKillTask(work, task);
        }
    }
}

void effectControlTask6A(Task* task)
{
    enum {
        EFFECT_ACTOR_FLASH_SIZE             = 1536,
        EFFECT_ACTOR_FLASH_SHORT_SPARK_ARG  = (1 << 16) | EFFECT_MUZZLE_SPARK_RANDOM_DRIFT | 640,
        EFFECT_ACTOR_FLASH_MEDIUM_SPARK_ARG = (2 << 16) | EFFECT_MUZZLE_SPARK_RANDOM_DRIFT | 640,
        EFFECT_ACTOR_FLASH_LONG_SPARK_ARG   = (3 << 16) | EFFECT_MUZZLE_SPARK_RANDOM_DRIFT | 640,
    };
    EffectWork*                    work;
    GfxCoord*                      muzzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    s32                            spawnZ;

    transientLight = gWorldCoordTransientPointLights;
    work           = task->spawnArg2.pointer;
    muzzleCoord    = task->extra.coordBody->coord;
    pointLight     = &transientLight->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case EFFECT_DRAW_TASK_NEW:
                // Light the spawn position before attaching the effect to the weapon.
                pointLight->head.transform.coord.coord.t[0]             = muzzleCoord->coord.t[0];
                pointLight->head.transform.coord.coord.t[1]             = muzzleCoord->coord.t[1];
                spawnZ                                                  = muzzleCoord->coord.t[2];
                transientLight->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                pointLight->head.color.b                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
                pointLight->head.color.g                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
                pointLight->head.color.r                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
                pointLight->inner                                       = EFFECT_WEAPON_FLASH_LIGHT_INNER_RADIUS;
                pointLight->outer                                       = EFFECT_WEAPON_FLASH_LIGHT_OUTER_RADIUS;
                gWorldCoordTransientPointLights->framesLeft             = EFFECT_WEAPON_FLASH_LIGHT_TICKS;
                pointLight->head.transform.coord.coord.t[2]             = spawnZ;
                muzzleCoord->parent                                     = work->parent;
                muzzleCoord->coord.t[0]                                 = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vx;
                muzzleCoord->coord.t[1]                                 = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vy;
                muzzleCoord->coord.t[2]                                 = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vz;
                muzzleCoord->composeStamp                               = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(muzzleCoord);
                RANDOM_LCG_ADVANCE();
                work->scale   = RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK;
                work->move.vx = 0;
                work->move.vy = 0;
                work->move.vz = -(work->scale >> 1);
                effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, work->scale + EFFECT_ACTOR_FLASH_SIZE, &work->move);
                task->state                    = EFFECT_DRAW_TASK_ACTIVE;
                gRoomEffectState->burstRequest = true;
                break;
            case EFFECT_DRAW_TASK_ACTIVE:
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_ACTOR_FLASH_SHORT_SPARK_ARG,
                            &work->move);
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_ACTOR_FLASH_MEDIUM_SPARK_ARG,
                            &work->move);
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_ACTOR_FLASH_LONG_SPARK_ARG,
                            &work->move);
                task->state++;
                break;
        }
        // Shrink the full-strength radius; retain the fixed outer falloff boundary.
        if (pointLight->inner >= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP + 1) {
            pointLight->inner -= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP;
        }
        if (work->age >= EFFECT_WEAPON_FLASH_LAST_AGE + 1) {
            effectKillTask(work, task);
        }
    }
}

void effectControlTask6B(Task* task)
{
    enum {
        EFFECT_WEAPON_FLASH_PROFILE_M249 = 17,
        EFFECT_M249_FLASH_LAST_AGE       = 1,
    };
    EffectWork*                    work;
    GfxCoord*                      muzzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    RoomEffectState*               effectState;
    s32                            burstSuppressed;
    s32                            weaponProfile;
    s32                            spawnZ;
    s32                            age;

    transientLight = gWorldCoordTransientPointLights;
    pointLight     = &transientLight->light;
    work           = task->spawnArg2.pointer;
    muzzleCoord    = task->extra.coordBody->coord;
    effectState    = gRoomEffectState;
    if (effectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            burstSuppressed       = task->spawnArg1.halves.high;
            work->index           = burstSuppressed;
            task->spawnArg1.value = (u8)task->spawnArg1.value;
            // Light the spawn position before attaching the effect to the weapon.
            pointLight->head.transform.coord.coord.t[0]             = muzzleCoord->coord.t[0];
            pointLight->head.transform.coord.coord.t[1]             = muzzleCoord->coord.t[1];
            spawnZ                                                  = muzzleCoord->coord.t[2];
            transientLight->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            pointLight->head.color.r                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
            pointLight->head.color.g                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
            pointLight->head.color.b                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
            pointLight->inner                                       = EFFECT_WEAPON_FLASH_LIGHT_INNER_RADIUS;
            pointLight->outer                                       = EFFECT_WEAPON_FLASH_LIGHT_OUTER_RADIUS;
            pointLight->head.transform.coord.coord.t[2]             = spawnZ;
            muzzleCoord->parent                                     = work->parent;
            muzzleCoord->coord.t[0]                                 = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vx;
            muzzleCoord->coord.t[1]                                 = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vy;
            muzzleCoord->coord.t[2]                                 = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vz;
            muzzleCoord->composeStamp                               = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(muzzleCoord);
            RANDOM_LCG_ADVANCE();
            work->scale   = RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK;
            work->move.vx = 0;
            work->move.vy = 0;
            work->move.vz = -(work->scale >> 1);
            effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, work->scale + EFFECT_RIFLE_FLASH_SIZE, &work->move);
            RANDOM_LCG_ADVANCE();
            effectSpawn(EFFECT_MUZZLE_FLARE_ADDITIVE, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_RIFLE_FLASH_SIZE, 0);
            weaponProfile = task->spawnArg1.value;
            task->state   = EFFECT_DRAW_TASK_ACTIVE;
            effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH_MODEL, muzzleCoord, weaponProfile, &_gEffectWeaponEmissionOffsets[weaponProfile]);
            if (task->spawnArg1.value == EFFECT_WEAPON_FLASH_PROFILE_M249) {
                work->scale                                 = EFFECT_M249_FLASH_LAST_AGE;
                gWorldCoordTransientPointLights->framesLeft = EFFECT_M249_FLASH_LAST_AGE;
            } else {
                work->scale                                 = EFFECT_WEAPON_FLASH_LAST_AGE;
                gWorldCoordTransientPointLights->framesLeft = EFFECT_WEAPON_FLASH_LIGHT_TICKS;
            }
            if (work->index == 0) {
                gRoomEffectState->burstRequest = true;
            }
        }
        // Shrink the full-strength radius; retain the fixed outer falloff boundary.
        if (pointLight->inner >= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP + 1) {
            pointLight->inner -= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP;
        }
        age = work->age;
        if (work->scale < age) {
            effectKillTask(work, task);
        }
    }
}

void effectControlTaskA1(Task* task)
{
    enum {
        EFFECT_SHOTGUN_FLASH_PROFILE_AS12       = 15,
        EFFECT_SHOTGUN_FLASH_PROFILE_GUNBLADE   = 23,
        EFFECT_SHOTGUN_FLASH_STATE_EJECTED      = 2,
        EFFECT_SHOTGUN_FLASH_BUCKSHOT_LAST_AGE  = 24,
        EFFECT_SHOTGUN_FLASH_PELLET_COUNT       = 12,
        EFFECT_SHOTGUN_FLASH_THROWN_SPARK_COUNT = 4,
        EFFECT_SHOTGUN_FLASH_BUCKSHOT_SIZE      = 512,
        EFFECT_SHOTGUN_FLASH_SLUG_SIZE          = 640,
        EFFECT_SHOTGUN_FLASH_SPARK_ARG          = (2 << 16) | 768,
    };
    EffectWork*                    work;
    GfxCoord*                      muzzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    SVECTOR*                       flareOffset;
    s32                            ammunitionIndex;
    s32                            age;
    s32                            sparkIndex;

/// Emits the shotgun's twelve pixel sparks followed by twelve line sparks.
///
/// muzzleCoord is a side-effect-free live GfxCoord* and sparkIndex a writable
/// integer local initialized to zero. Arguments occur repeatedly; the counter
/// ends at twelve and is reset between loops. Children use zero payload and
/// offset, consume no draws here, and own independent work. Uses the local
/// EFFECT_SHOTGUN_FLASH_PELLET_COUNT constant; captures no caller variable.
/// Both loops form one statement. The input counter controls the first loop;
/// a nonzero value would emit fewer pixel sparks while still emitting twelve lines.
#define EFFECT_SPAWN_SHOTGUN_PELLET_SPARKS(muzzleCoord, sparkIndex)                                \
    do {                                                                                           \
        for (; (sparkIndex) < EFFECT_SHOTGUN_FLASH_PELLET_COUNT; (sparkIndex)++) {                 \
            effectSpawn(EFFECT_PIXEL_SPARK, (muzzleCoord), 0, 0);                                  \
        }                                                                                          \
        for ((sparkIndex) = 0; (sparkIndex) < EFFECT_SHOTGUN_FLASH_PELLET_COUNT; (sparkIndex)++) { \
            effectSpawn(EFFECT_SHOTGUN_SPARK_LINE, (muzzleCoord), 0, 0);                           \
        }                                                                                          \
    } while (0)

    work           = task->spawnArg2.pointer;
    muzzleCoord    = task->extra.coordBody->coord;
    transientLight = gWorldCoordTransientPointLights;
    pointLight     = &transientLight->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case EFFECT_DRAW_TASK_NEW:
                ammunitionIndex       = task->spawnArg1.halves.high;
                work->index           = ammunitionIndex;
                task->spawnArg1.value = (u8)task->spawnArg1.value;
                // Light the spawn position before moving to the weapon muzzle.
                _effectInitializeShotgunLight(muzzleCoord, pointLight);
                muzzleCoord->parent       = work->parent;
                muzzleCoord->coord.t[0]   = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vx;
                muzzleCoord->coord.t[1]   = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vy;
                muzzleCoord->coord.t[2]   = _gEffectWeaponMuzzleOffsets[task->spawnArg1.value].vz;
                muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(muzzleCoord);
                switch (task->spawnArg1.value) {
                    case EFFECT_WEAPON_FLASH_PROFILE_P08:
                    default:
                        RANDOM_LCG_ADVANCE();
                        work->scale = RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK;
                        if (work->index == SHOTGUN_ROUND_BUCKSHOT) {
                            work->move.vx = 0;
                            work->move.vy = 0;
                            work->move.vz = -((s32)((u16)work->scale << 16) >> 18);
                            effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, work->scale + EFFECT_SHOTGUN_FLASH_BUCKSHOT_SIZE, &work->move);
                            sparkIndex = 0;
                            EFFECT_SPAWN_SHOTGUN_PELLET_SPARKS(muzzleCoord, sparkIndex);
                            work->scale = EFFECT_SHOTGUN_FLASH_BUCKSHOT_LAST_AGE;
                        } else {
                            work->move.vx = 0;
                            work->move.vy = 0;
                            work->move.vz = -((s32)((u16)work->scale << 16) >> 17);
                            effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, work->scale + EFFECT_RIFLE_FLASH_SIZE, &work->move);
                            RANDOM_LCG_ADVANCE();
                            effectSpawn(EFFECT_MUZZLE_FLARE_ADDITIVE, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_RIFLE_FLASH_SIZE, 0);
                            if (work->index == SHOTGUN_ROUND_RIFLED_SLUG) {
                                RANDOM_LCG_ADVANCE();
                                _effectDrawMuzzleFlash(muzzleCoord, (s16)(work->scale + EFFECT_SHOTGUN_FLASH_SLUG_SIZE),
                                                       RANDOM_LCG_DRAW() & EFFECT_DRAW_ANGLE_MASK);
                            }
                            work->scale = EFFECT_WEAPON_FLASH_LAST_AGE;
                        }
                        RANDOM_LCG_ADVANCE();
                        effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_SHOTGUN_FLASH_SPARK_ARG, 0);
                        for (sparkIndex = 0; sparkIndex < EFFECT_SHOTGUN_FLASH_THROWN_SPARK_COUNT; sparkIndex++) {
                            RANDOM_LCG_ADVANCE();
                            effectSpawn(EFFECT_MUZZLE_SPARK_THROWN, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) | EFFECT_SHOTGUN_FLASH_BUCKSHOT_SIZE, 0);
                        }
                        task->state = EFFECT_DRAW_TASK_ACTIVE;
                        break;
                    case EFFECT_SHOTGUN_FLASH_PROFILE_AS12:
                        RANDOM_LCG_ADVANCE();
                        work->scale = RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK;
                        if (work->index == SHOTGUN_ROUND_BUCKSHOT) {
                            work->move.vx = 0;
                            work->move.vy = 0;
                            work->move.vz = -((s32)((u16)work->scale << 16) >> 18);
                            effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, work->scale + EFFECT_SHOTGUN_FLASH_BUCKSHOT_SIZE, &work->move);
                            sparkIndex = 0;
                            EFFECT_SPAWN_SHOTGUN_PELLET_SPARKS(muzzleCoord, sparkIndex);
                        } else {
                            work->move.vx = 0;
                            work->move.vy = 0;
                            work->move.vz = -((s32)((u16)work->scale << 16) >> 17);
                            effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, work->scale + EFFECT_RIFLE_FLASH_SIZE, &work->move);
                            RANDOM_LCG_ADVANCE();
                            effectSpawn(EFFECT_MUZZLE_FLARE_ADDITIVE, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_RIFLE_FLASH_SIZE, 0);
                            if (work->index == SHOTGUN_ROUND_RIFLED_SLUG) {
                                RANDOM_LCG_ADVANCE();
                                _effectDrawMuzzleFlash(muzzleCoord, (s16)(work->scale + EFFECT_SHOTGUN_FLASH_SLUG_SIZE),
                                                       RANDOM_LCG_DRAW() & EFFECT_DRAW_ANGLE_MASK);
                            }
                        }
                        RANDOM_LCG_ADVANCE();
                        effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_SHOTGUN_FLASH_SPARK_ARG, 0);
                        for (sparkIndex = 0; sparkIndex < EFFECT_SHOTGUN_FLASH_THROWN_SPARK_COUNT; sparkIndex++) {
                            RANDOM_LCG_ADVANCE();
                            effectSpawn(EFFECT_MUZZLE_SPARK_THROWN, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) | EFFECT_SHOTGUN_FLASH_BUCKSHOT_SIZE, 0);
                        }
                        effectSpawn(EFFECT_SHOTGUN_SHELL_CASING, muzzleCoord, task->spawnArg1.value, &_gEffectWeaponEmissionOffsets[task->spawnArg1.value]);
                        task->state = EFFECT_SHOTGUN_FLASH_STATE_EJECTED;
                        work->scale = EFFECT_WEAPON_FLASH_LAST_AGE;
                        break;
                    case EFFECT_SHOTGUN_FLASH_PROFILE_GUNBLADE:
                        RANDOM_LCG_ADVANCE();
                        work->scale = RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK;
                        if (work->index == SHOTGUN_ROUND_BUCKSHOT) {
                            work->move.vx = 0;
                            work->move.vy = 0;
                            work->move.vz = (s32)((u16)work->scale << 16) >> 18;
                            gte_SetTransMatrix(&GsWSMATRIX);
                            gte_SetRotMatrix(&muzzleCoord->coord);
                            flareOffset = &work->move;
                            gte_ldv0(flareOffset);
                            gte_rtv0();
                            gte_stsv(flareOffset);
                            effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, work->scale + EFFECT_SHOTGUN_FLASH_BUCKSHOT_SIZE, flareOffset);
                            sparkIndex = 0;
                            gfxRotMatrixX(&muzzleCoord->coord, EFFECT_DRAW_QUARTER_TURN, GRAPHICS_ROTATION_COMPOSE);
                            muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                            EFFECT_SPAWN_SHOTGUN_PELLET_SPARKS(muzzleCoord, sparkIndex);
                        } else {
                            work->move.vx = 0;
                            work->move.vy = 0;
                            work->move.vz = (s32)((u16)work->scale << 16) >> 17;
                            gte_SetTransMatrix(&GsWSMATRIX);
                            gte_SetRotMatrix(&muzzleCoord->coord);
                            flareOffset = &work->move;
                            gte_ldv0(flareOffset);
                            gte_rtv0();
                            gte_stsv(flareOffset);
                            effectSpawn(EFFECT_MUZZLE_FLARE, muzzleCoord, work->scale + EFFECT_RIFLE_FLASH_SIZE, flareOffset);
                            RANDOM_LCG_ADVANCE();
                            effectSpawn(EFFECT_MUZZLE_FLARE_ADDITIVE, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_RIFLE_FLASH_SIZE, 0);
                            if (work->index == SHOTGUN_ROUND_RIFLED_SLUG) {
                                RANDOM_LCG_ADVANCE();
                                _effectDrawMuzzleFlash(muzzleCoord, (s16)(work->scale + EFFECT_SHOTGUN_FLASH_SLUG_SIZE),
                                                       RANDOM_LCG_DRAW() & EFFECT_DRAW_ANGLE_MASK);
                            }
                            gfxRotMatrixX(&muzzleCoord->coord, EFFECT_DRAW_QUARTER_TURN, GRAPHICS_ROTATION_COMPOSE);
                            muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                        }
                        RANDOM_LCG_ADVANCE();
                        effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_SHOTGUN_FLASH_SPARK_ARG, 0);
                        for (sparkIndex = 0; sparkIndex < EFFECT_SHOTGUN_FLASH_THROWN_SPARK_COUNT; sparkIndex++) {
                            RANDOM_LCG_ADVANCE();
                            effectSpawn(EFFECT_MUZZLE_SPARK_THROWN, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) | EFFECT_SHOTGUN_FLASH_BUCKSHOT_SIZE, 0);
                        }
                        effectSpawn(EFFECT_SHOTGUN_SHELL_CASING, work->parent, task->spawnArg1.value, &_gEffectWeaponEmissionOffsets[task->spawnArg1.value]);
                        task->state = EFFECT_SHOTGUN_FLASH_STATE_EJECTED;
                        work->scale = EFFECT_WEAPON_FLASH_LAST_AGE;
                }
                transientLight->framesLeft     = EFFECT_WEAPON_FLASH_LIGHT_TICKS;
                gRoomEffectState->burstRequest = true;
                break;
            case EFFECT_DRAW_TASK_ACTIVE:
                if (work->age == work->scale) {
                    effectSpawn(EFFECT_SHOTGUN_SHELL_CASING, muzzleCoord, task->spawnArg1.value, &_gEffectWeaponEmissionOffsets[task->spawnArg1.value]);
                    task->state = EFFECT_SHOTGUN_FLASH_STATE_EJECTED;
                }
                break;
        }
        // Contract full-strength lighting while keeping the outer reach fixed.
        if (pointLight->inner > EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP) {
            pointLight->inner -= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP;
        }
        age = work->age;
        if (work->scale < age) {
            effectKillTask(work, task);
        }
    }

#undef EFFECT_SPAWN_SHOTGUN_PELLET_SPARKS
}

void effectControlTask6C(Task* task)
{
    // Delayed spark arguments retain bit 13, unused by `effectSpriteTask35`.
    enum {
        EFFECT_WEAPON_FLASH_PROFILE_MM1          = 12,
        EFFECT_WEAPON_FLASH_PROFILE_M4A1_GRENADE = 27,
        EFFECT_GRENADE_FLASH_SIZE                = 1024,
        EFFECT_GRENADE_FLASH_SPARK_PAIR_COUNT    = 4,
        EFFECT_GRENADE_FLASH_EJECTION_AGE        = 16,
        EFFECT_M4A1_GRENADE_FLASH_EJECTION_AGE   = 24,
        EFFECT_M4A1_GRENADE_FLASH_EJECTION_BIAS  = 10,
        EFFECT_GRENADE_FLASH_INITIAL_SPARK_ARG   = (2 << 16) | EFFECT_MUZZLE_SPARK_RANDOM_DRIFT | 896,
        EFFECT_GRENADE_FLASH_SHORT_SPARK_ARG     = (1 << 16) | 0x2000 | 384,
        EFFECT_GRENADE_FLASH_MEDIUM_SPARK_ARG    = (2 << 16) | 0x2000 | 384,
        EFFECT_GRENADE_FLASH_LONG_SPARK_ARG      = (3 << 16) | 0x2000 | 384,
    };
    EffectWork*                    work;
    GfxCoord*                      muzzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    s32                            burstSuppressed;
    s32                            flareSizeJitter;
    s32                            spawnZ;
    s32                            randomState;
    s32                            angleRandomState;
    s32                            age;
    s32                            sparkIndex;

    work           = task->spawnArg2.pointer;
    muzzleCoord    = task->extra.coordBody->coord;
    transientLight = gWorldCoordTransientPointLights;
    pointLight     = &transientLight->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case EFFECT_DRAW_TASK_NEW:
                burstSuppressed       = task->spawnArg1.halves.high;
                work->index           = burstSuppressed;
                task->spawnArg1.value = (u8)task->spawnArg1.value;
                // Light the spawn position before attaching the effect to the weapon.
                pointLight->head.transform.coord.coord.t[0]             = muzzleCoord->coord.t[0];
                pointLight->head.transform.coord.coord.t[1]             = muzzleCoord->coord.t[1];
                spawnZ                                                  = muzzleCoord->coord.t[2];
                transientLight->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                pointLight->head.color.r                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
                pointLight->head.color.g                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
                pointLight->head.color.b                                = EFFECT_WEAPON_FLASH_LIGHT_INTENSITY;
                pointLight->inner                                       = EFFECT_WEAPON_FLASH_LIGHT_INNER_RADIUS;
                pointLight->outer                                       = EFFECT_WEAPON_FLASH_LIGHT_OUTER_RADIUS;
                pointLight->head.transform.coord.coord.t[2]             = spawnZ;
                muzzleCoord->parent                                     = work->parent;
                muzzleCoord->coord.t[0]                                 = _gEffectGrenadeMuzzleOffsets[task->spawnArg1.value].vx;
                muzzleCoord->coord.t[1]                                 = _gEffectGrenadeMuzzleOffsets[task->spawnArg1.value].vy;
                muzzleCoord->coord.t[2]                                 = _gEffectGrenadeMuzzleOffsets[task->spawnArg1.value].vz;
                muzzleCoord->composeStamp                               = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(muzzleCoord);
                randomState      = RANDOM_LCG_NEXT(gRandomLcgState);
                flareSizeJitter  = ((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK;
                angleRandomState = RANDOM_LCG_NEXT(randomState);
                gRandomLcgState  = randomState;
                work->scale      = flareSizeJitter;
                gRandomLcgState  = angleRandomState;
                _effectDrawMuzzleFlash(muzzleCoord, flareSizeJitter | EFFECT_GRENADE_FLASH_SIZE, ((u32)angleRandomState >> 16) & EFFECT_DRAW_ANGLE_MASK);
                for (sparkIndex = 0; sparkIndex < EFFECT_GRENADE_FLASH_SPARK_PAIR_COUNT; sparkIndex++) {
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_MUZZLE_SPARK_THROWN, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_RIFLE_FLASH_SIZE, 0);
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) + EFFECT_GRENADE_FLASH_INITIAL_SPARK_ARG, 0);
                }
                switch (task->spawnArg1.value) {
                    case EFFECT_WEAPON_FLASH_PROFILE_P08:
                    default:
                        task->state = EFFECT_DRAW_TASK_ACTIVE;
                        work->scale = EFFECT_GRENADE_FLASH_EJECTION_AGE;
                        break;
                    case EFFECT_WEAPON_FLASH_PROFILE_MM1:
                        task->state = EFFECT_WEAPON_FLASH_STATE_WAIT;
                        work->scale = EFFECT_WEAPON_FLASH_LAST_AGE;
                        break;
                    case EFFECT_WEAPON_FLASH_PROFILE_M4A1_GRENADE:
                        work->angle = EFFECT_M4A1_GRENADE_FLASH_EJECTION_BIAS;
                        task->state = EFFECT_DRAW_TASK_ACTIVE;
                        work->scale = EFFECT_M4A1_GRENADE_FLASH_EJECTION_AGE;
                        break;
                }
                transientLight->framesLeft = EFFECT_WEAPON_FLASH_LIGHT_TICKS;
                if (work->index == 0) {
                    gRoomEffectState->burstRequest = true;
                }
                break;
            case EFFECT_DRAW_TASK_ACTIVE:
                if (work->age == work->scale) {
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & 0xFF) + EFFECT_GRENADE_FLASH_SHORT_SPARK_ARG,
                                &_gEffectGrenadeEmissionOffsets[task->spawnArg1.value]);
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & 0xFF) + EFFECT_GRENADE_FLASH_MEDIUM_SPARK_ARG,
                                &_gEffectGrenadeEmissionOffsets[task->spawnArg1.value]);
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_MUZZLE_SPARK, muzzleCoord, (RANDOM_LCG_DRAW() & 0xFF) + EFFECT_GRENADE_FLASH_LONG_SPARK_ARG,
                                &_gEffectGrenadeEmissionOffsets[task->spawnArg1.value]);
                    effectSpawn(EFFECT_091, muzzleCoord, task->spawnArg1.value + work->angle,
                                &_gEffectGrenadeEmissionOffsets[task->spawnArg1.value]);
                    task->state = EFFECT_WEAPON_FLASH_STATE_WAIT;
                }
                break;
        }
        // Shrink the full-strength radius; retain the fixed outer falloff boundary.
        if (pointLight->inner >= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP + 1) {
            pointLight->inner -= EFFECT_WEAPON_FLASH_LIGHT_RADIUS_STEP;
        }
        age = work->age;
        if (work->scale < age) {
            effectKillTask(work, task);
        }
    }
}

void effectSpriteTask34(Task* task)
{
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s16                 effectControl;
    s32                 randomState;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        if (work->age == 0) {
            randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
            work->scale     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            gRandomLcgState = randomState;
            work->angle     = task->spawnArg1.halves.low;
        }
        // Project the two texture frames around the composed muzzle position.
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = (u16)coord->workm.t[0];
        block->worldPoint.vy = (u16)coord->workm.t[1];
        block->worldPoint.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            block->depth   = block->depth + 1;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
            quad->tpage            = getTPage(0, GPU_BLEND_ADD, 640, 0);
            quad->clut             = getClut(0, 269);
            quad->u0               = work->age * EFFECT_MUZZLE_FLARE_CELL_SIZE;
            quad->v0               = EFFECT_MUZZLE_FLARE_TEXTURE_V;
            quad->u1               = work->age * EFFECT_MUZZLE_FLARE_CELL_SIZE + (EFFECT_MUZZLE_FLARE_CELL_SIZE - 1);
            quad->v1               = EFFECT_MUZZLE_FLARE_TEXTURE_V;
            quad->u2               = work->age * EFFECT_MUZZLE_FLARE_CELL_SIZE;
            quad->v2               = (EFFECT_MUZZLE_FLARE_TEXTURE_V + EFFECT_MUZZLE_FLARE_CELL_SIZE - 1);
            quad->u3               = work->age * EFFECT_MUZZLE_FLARE_CELL_SIZE + (EFFECT_MUZZLE_FLARE_CELL_SIZE - 1);
            quad->v3               = (EFFECT_MUZZLE_FLARE_TEXTURE_V + EFFECT_MUZZLE_FLARE_CELL_SIZE - 1);
            block->extent.corner.x = (((work->angle * 23) / block->depth) * rsin(work->scale)) >> 12;
            block->extent.corner.y = (((work->angle * 23) / block->depth) * rcos(work->scale)) >> 12;
            quad->x0               = block->screenX + (u16)block->extent.corner.x;
            quad->x3               = block->screenX - (u16)block->extent.corner.x;
            quad->y0               = block->screenY - (u16)block->extent.corner.y;
            quad->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((work->angle * 23) / block->depth) * rsin(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> 12;
            block->extent.corner.y = (((work->angle * 23) / block->depth) * rcos(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> 12;
            quad->x1               = block->screenX + (u16)block->extent.corner.x;
            quad->x2               = block->screenX - (u16)block->extent.corner.x;
            quad->y1               = block->screenY - (u16)block->extent.corner.y;
            quad->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        work->age++;
        if (work->age < EFFECT_MUZZLE_FLARE_FRAME_COUNT) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(work, task);
}

void effectSpriteTask72(Task* task)
{
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s16                 effectControl;
    s32                 randomState;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
            work->scale     = (((u32)randomState >> 16) & 0x800) - 0x200;
            gRandomLcgState = randomState;
            work->angle     = task->spawnArg1.halves.low;
            task->state     = EFFECT_DRAW_TASK_ACTIVE;
        }
        // The two possible orientations are opposite diagonals of the flash.
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = (u16)coord->workm.t[0];
        block->worldPoint.vy = (u16)coord->workm.t[1];
        block->worldPoint.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            block->depth   = block->depth + 1;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
            quad->tpage            = getTPage(0, GPU_BLEND_ADD, 512, 0);
            quad->clut             = getClut(128, 266);
            quad->u0               = work->age * EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE;
            quad->v0               = EFFECT_ADDITIVE_MUZZLE_FLARE_TEXTURE_V;
            quad->u1               = work->age * EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE + (EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE - 1);
            quad->v1               = EFFECT_ADDITIVE_MUZZLE_FLARE_TEXTURE_V;
            quad->u2               = work->age * EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE;
            quad->v2               = (EFFECT_ADDITIVE_MUZZLE_FLARE_TEXTURE_V + EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE - 1);
            quad->u3               = work->age * EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE + (EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE - 1);
            quad->v3               = (EFFECT_ADDITIVE_MUZZLE_FLARE_TEXTURE_V + EFFECT_ADDITIVE_MUZZLE_FLARE_CELL_SIZE - 1);
            block->extent.corner.x = (((work->angle * 31) / block->depth) * rsin(work->scale)) >> 12;
            block->extent.corner.y = (((work->angle * 31) / block->depth) * rcos(work->scale)) >> 12;
            quad->x0               = block->screenX + (u16)block->extent.corner.x;
            quad->x3               = block->screenX - (u16)block->extent.corner.x;
            quad->y0               = block->screenY - (u16)block->extent.corner.y;
            quad->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((work->angle * 31) / block->depth) * rsin(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> 12;
            block->extent.corner.y = (((work->angle * 31) / block->depth) * rcos(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> 12;
            quad->x1               = block->screenX + (u16)block->extent.corner.x;
            quad->x2               = block->screenX - (u16)block->extent.corner.x;
            quad->y1               = block->screenY - (u16)block->extent.corner.y;
            quad->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        work->age++;
        if (work->age < EFFECT_MUZZLE_FLARE_FRAME_COUNT) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(work, task);
}

void effectLineTaskA3(Task* task)
{
    EffectWork*        work;
    GfxCoord*          coord;
    EffectLineScratch* block;
    LINE_G2*           line;
    s16                effectControl;
    s16                brightness;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            RANDOM_LCG_ADVANCE();
            work->scale = RANDOM_LCG_DRAW() % 3 + 1;
            RANDOM_LCG_ADVANCE();
            work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
            RANDOM_LCG_ADVANCE();
            work->move.vy = (RANDOM_LCG_DRAW() & 0x1FF) + 0x200;
            RANDOM_LCG_ADVANCE();
            work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
            task->state   = EFFECT_DRAW_TASK_ACTIVE;
        }
        block                  = SCRATCH_STACK_RESERVE_BLOCK(EffectLineScratch);
        block->endpoints[0].vx = coord->workm.t[0];
        block->endpoints[0].vy = coord->workm.t[1];
        block->endpoints[0].vz = coord->workm.t[2];
        // Rotate the launch direction into the view frame and lengthen it with age.
        gte_SetRotMatrix(&work->parent->coord);
        gte_ldv0(&work->move);
        gte_rtv0();
        gte_stsv(&block->endpoints[1]);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&block->endpoints[1]);
        gte_rtv0();
        gte_stsv(&block->endpoints[1]);
        gte_lddp((work->age << 11) + ONE);
        gte_ldsv(&block->endpoints[1]);
        gte_gpf12();
        gte_stsv(&block->endpoints[1]);
        block->endpoints[1].vx += block->endpoints[0].vx;
        block->endpoints[1].vy += block->endpoints[0].vy;
        block->endpoints[1].vz += block->endpoints[0].vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->endpoints[0]);
        gte_rtps();
        gte_stsxy(&block->screenEndpoints[0]);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_ldv0(&block->endpoints[1]);
            gte_rtps();
            gte_stsxy(&block->screenEndpoints[1]);
            gte_stflg(&block->projectionFlags);
            if (block->projectionFlags >= 0) {
                gte_stszotz(&block->depth);
                block->depth   = block->depth + 1;
                line           = gGpuPrimCursor;
                gGpuPrimCursor = line + 1;
                setLineG2(line);
                brightness = 0xFF - (work->age << 6);
                line->r0   = 0;
                line->g0   = 0;
                line->b0   = 0;
                line->r1   = brightness;
                line->g1   = brightness >> work->scale;
                line->b1   = brightness >> 3;
                line->x0   = block->screenEndpoints[0].vx;
                line->y0   = block->screenEndpoints[0].vy;
                line->x1   = block->screenEndpoints[1].vx;
                line->y1   = block->screenEndpoints[1].vy;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                        line);
                gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->depth);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectLineScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        work->age++;
        if (work->age < EFFECT_SHOTGUN_SPARK_LINE_TICKS) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(work, task);
}

/// Draws the rotated 56-texel muzzle flash used by grenade and shotgun fire.
///
/// coord must already be composed. size and angle are narrowed to s16;
/// angle uses 4096 units per turn. size * 55 / (SZ3 / 4) is the screen
/// half-diagonal before rotation. The raw texture is drawn additively.
static void _effectDrawMuzzleFlash(const GfxCoord* coord, s32 size, s32 angle)
{
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s32                 perpendicularAngle;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = (u16)coord->workm.t[0];
    block->worldPoint.vy = (u16)coord->workm.t[1];
    block->worldPoint.vz = (u16)coord->workm.t[2];
    // Project the centre; the texture rotates in screen space after the divide.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);
        quad->clut  = getClut(176, 266);
        setUVWH(quad, 0x70, 0xC8, EFFECT_MUZZLE_FLASH_UV_SPAN, EFFECT_MUZZLE_FLASH_UV_SPAN);
        block->extent.corner.x = ((((s16)size * EFFECT_MUZZLE_FLASH_UV_SPAN) / block->depth) * rsin((s16)angle)) >> 12;
        block->extent.corner.y = ((((s16)size * EFFECT_MUZZLE_FLASH_UV_SPAN) / block->depth) * rcos((s16)angle)) >> 12;
        quad->x0               = block->screenX + (u16)block->extent.corner.x;
        quad->x3               = block->screenX - (u16)block->extent.corner.x;
        quad->y0               = block->screenY - (u16)block->extent.corner.y;
        quad->y3               = block->screenY + (u16)block->extent.corner.y;
        perpendicularAngle     = (s16)angle + EFFECT_DRAW_QUARTER_TURN;
        block->extent.corner.x = ((((s16)size * EFFECT_MUZZLE_FLASH_UV_SPAN) / block->depth) * rsin(perpendicularAngle)) >> 12;
        block->extent.corner.y = ((((s16)size * EFFECT_MUZZLE_FLASH_UV_SPAN) / block->depth) * rcos(perpendicularAngle)) >> 12;
        quad->x1               = block->screenX + (u16)block->extent.corner.x;
        quad->x2               = block->screenX - (u16)block->extent.corner.x;
        quad->y1               = block->screenY - (u16)block->extent.corner.y;
        quad->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void effectSpriteTask35(Task* task)
{
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s16                 effectControl;
    s16                 framePeriod;
    s32                 randomState;
    s32                 nextZ;
    s32                 textureFrame;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
            work->scale     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            gRandomLcgState = randomState;
            work->angle     = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            if (task->spawnArg1.value & EFFECT_MUZZLE_SPARK_PERIOD_MASK) {
                framePeriod = (task->spawnArg1.value >> 16) & 0xF;
            } else {
                framePeriod = 1;
            }
            work->period = framePeriod;
            if (task->spawnArg1.value & EFFECT_MUZZLE_SPARK_RANDOM_DRIFT) {
                RANDOM_LCG_ADVANCE();
                work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vz = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
            }
            task->state = EFFECT_DRAW_TASK_ACTIVE;
        }
        actorRenderComposeCoord(coord);
        // Each atlas frame lasts period ticks; preserve the four divisions.
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = (u16)coord->workm.t[0];
        block->worldPoint.vy = (u16)coord->workm.t[1];
        block->worldPoint.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
            quad->tpage            = getTPage(0, GPU_BLEND_ADD, 512, 0);
            quad->clut             = getClut(32, 265);
            textureFrame           = work->age / work->period;
            quad->v0               = 0;
            quad->u0               = textureFrame * EFFECT_MUZZLE_SPARK_CELL_SIZE + EFFECT_MUZZLE_SPARK_TEXTURE_U;
            textureFrame           = work->age / work->period;
            quad->v1               = 0;
            quad->u1               = textureFrame * EFFECT_MUZZLE_SPARK_CELL_SIZE + (EFFECT_MUZZLE_SPARK_TEXTURE_U + EFFECT_MUZZLE_SPARK_CELL_SIZE - 1);
            textureFrame           = work->age / work->period;
            quad->v2               = (EFFECT_MUZZLE_SPARK_CELL_SIZE - 1);
            quad->u2               = textureFrame * EFFECT_MUZZLE_SPARK_CELL_SIZE + EFFECT_MUZZLE_SPARK_TEXTURE_U;
            textureFrame           = work->age / work->period;
            quad->v3               = (EFFECT_MUZZLE_SPARK_CELL_SIZE - 1);
            quad->u3               = textureFrame * EFFECT_MUZZLE_SPARK_CELL_SIZE + (EFFECT_MUZZLE_SPARK_TEXTURE_U + EFFECT_MUZZLE_SPARK_CELL_SIZE - 1);
            block->extent.corner.x = (((work->angle * 23) / block->depth) * rsin(work->scale)) >> 12;
            block->extent.corner.y = (((work->angle * 23) / block->depth) * rcos(work->scale)) >> 12;
            quad->x0               = block->screenX + (u16)block->extent.corner.x;
            quad->x3               = block->screenX - (u16)block->extent.corner.x;
            quad->y0               = block->screenY - (u16)block->extent.corner.y;
            quad->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((work->angle * 23) / block->depth) * rsin(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> 12;
            block->extent.corner.y = (((work->angle * 23) / block->depth) * rcos(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> 12;
            quad->x1               = block->screenX + (u16)block->extent.corner.x;
            quad->x2               = block->screenX - (u16)block->extent.corner.x;
            quad->y1               = block->screenY - (u16)block->extent.corner.y;
            quad->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        nextZ               = coord->coord.t[2] + work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = nextZ;
        work->age++;
        if (work->age <= work->period * EFFECT_MUZZLE_SPARK_FRAME_COUNT - 1) {
            return;
        }
    }
    effectKillTask(work, task);
}

void effectSpriteTask6F(Task* task)
{
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s16                 effectControl;
    s32                 nextZ;
    s32                 textureFrame;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        actorRenderComposeCoord(coord);
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            RANDOM_LCG_ADVANCE();
            work->scale = RANDOM_LCG_DRAW() & EFFECT_DRAW_ANGLE_MASK;
            work->angle = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            RANDOM_LCG_ADVANCE();
            work->period = (RANDOM_LCG_DRAW() & 1) + 1;
            RANDOM_LCG_ADVANCE();
            work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
            RANDOM_LCG_ADVANCE();
            work->move.vy = (gRandomLcgState >> 14) & 0x7C;
            RANDOM_LCG_ADVANCE();
            work->move.vz = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
            // Carry the random launch displacement into the effect's view-relative frame.
            gte_SetRotMatrix(&work->parent->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            task->state = EFFECT_DRAW_TASK_ACTIVE;
        }
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = (u16)coord->workm.t[0];
        block->worldPoint.vy = (u16)coord->workm.t[1];
        block->worldPoint.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
            quad->tpage            = getTPage(0, GPU_BLEND_ADD, 512, 0);
            quad->clut             = getClut(304, 265);
            textureFrame           = work->age / work->period;
            quad->v0               = EFFECT_THROWN_MUZZLE_SPARK_TEXTURE_V;
            quad->u0               = textureFrame * EFFECT_THROWN_MUZZLE_SPARK_CELL_SIZE;
            textureFrame           = work->age / work->period;
            quad->v1               = EFFECT_THROWN_MUZZLE_SPARK_TEXTURE_V;
            quad->u1               = textureFrame * EFFECT_THROWN_MUZZLE_SPARK_CELL_SIZE + 0x1F;
            textureFrame           = work->age / work->period;
            quad->v2               = (EFFECT_THROWN_MUZZLE_SPARK_TEXTURE_V + EFFECT_THROWN_MUZZLE_SPARK_CELL_SIZE - 1);
            quad->u2               = textureFrame * EFFECT_THROWN_MUZZLE_SPARK_CELL_SIZE;
            textureFrame           = work->age / work->period;
            quad->v3               = (EFFECT_THROWN_MUZZLE_SPARK_TEXTURE_V + EFFECT_THROWN_MUZZLE_SPARK_CELL_SIZE - 1);
            quad->u3               = textureFrame * EFFECT_THROWN_MUZZLE_SPARK_CELL_SIZE + 0x1F;
            block->extent.corner.x = (((work->angle * 31) / block->depth) * rsin(work->scale)) >> 12;
            block->extent.corner.y = (((work->angle * 31) / block->depth) * rcos(work->scale)) >> 12;
            quad->x0               = block->screenX + (u16)block->extent.corner.x;
            quad->x3               = block->screenX - (u16)block->extent.corner.x;
            quad->y0               = block->screenY - (u16)block->extent.corner.y;
            quad->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((work->angle * 31) / block->depth) * rsin(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> 12;
            block->extent.corner.y = (((work->angle * 31) / block->depth) * rcos(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> 12;
            quad->x1               = block->screenX + (u16)block->extent.corner.x;
            quad->x2               = block->screenX - (u16)block->extent.corner.x;
            quad->y1               = block->screenY - (u16)block->extent.corner.y;
            quad->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        nextZ               = coord->coord.t[2] + work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = nextZ;
        work->age++;
        if (work->age <= work->period * EFFECT_THROWN_MUZZLE_SPARK_FRAME_COUNT - 1) {
            return;
        }
    }
    effectKillTask(work, task);
}

void effectThrownModelTask(Task* task)
{
    // Profiles follow weapon indices, with separate NPC and grenade-ejection keys.
    enum {
        EFFECT_THROWN_MODEL_PROFILE_P08                   = 1,
        EFFECT_THROWN_MODEL_PROFILE_M93R                  = 2,
        EFFECT_THROWN_MODEL_PROFILE_M950                  = 3,
        EFFECT_THROWN_MODEL_PROFILE_P229                  = 5,
        EFFECT_THROWN_MODEL_PROFILE_MONGOOSE              = 9,
        EFFECT_THROWN_MODEL_PROFILE_GRENADE_PISTOL        = 11,
        EFFECT_THROWN_MODEL_PROFILE_MM1                   = 12,
        EFFECT_THROWN_MODEL_PROFILE_PA3                   = 13,
        EFFECT_THROWN_MODEL_PROFILE_SP12                  = 14,
        EFFECT_THROWN_MODEL_PROFILE_AS12                  = 15,
        EFFECT_THROWN_MODEL_PROFILE_M4A1                  = 16,
        EFFECT_THROWN_MODEL_PROFILE_M249                  = 17,
        EFFECT_THROWN_MODEL_PROFILE_M4A1_UPGRADE_1        = 20,
        EFFECT_THROWN_MODEL_PROFILE_M4A1_UPGRADE_2        = 21,
        EFFECT_THROWN_MODEL_PROFILE_GUNBLADE              = 23,
        EFFECT_THROWN_MODEL_PROFILE_M4A1_HAMMER           = 25,
        EFFECT_THROWN_MODEL_PROFILE_M4A1_BAYONET          = 26,
        EFFECT_THROWN_MODEL_PROFILE_M4A1_GRENADE          = 27,
        EFFECT_THROWN_MODEL_PROFILE_M4A1_PYKE             = 28,
        EFFECT_THROWN_MODEL_PROFILE_M4A1_JAVELIN          = 29,
        EFFECT_THROWN_MODEL_PROFILE_MP5A5                 = 30,
        EFFECT_THROWN_MODEL_PROFILE_MP5A5_UPGRADE_1       = 31,
        EFFECT_THROWN_MODEL_PROFILE_MP5A5_UPGRADE_2       = 32,
        EFFECT_THROWN_MODEL_PROFILE_NPC_PISTOL            = 33,
        EFFECT_THROWN_MODEL_PROFILE_M4A1_GRENADE_EJECTION = 37,
        EFFECT_THROWN_MODEL_LONG_BLINK_AGE                = 20,
        EFFECT_THROWN_MODEL_MEDIUM_BLINK_AGE              = 15,
        EFFECT_THROWN_MODEL_SHORT_BLINK_AGE               = 10,
        EFFECT_THROWN_MODEL_MIN_BLINK_AGE                 = 5,
        EFFECT_THROWN_MODEL_GRAVITY                       = 384,
    };
    SVECTOR     displacement;
    SVECTOR     probeEnd;
    SVECTOR     probeStartOrNormal;
    VECTOR      rotatedLaunchVector;
    VECTOR      launchVector;
    TmdObject*  model;
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    direction;
    s16         effectControl;

    model         = task->extra.tmd;
    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = model->coords;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    actorRenderComposeCoord(coord);
    // Select the launch direction, speed and blink age, then seed the spin.
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        switch (task->spawnArg1.value) {
            case EFFECT_THROWN_MODEL_PROFILE_P08:
            default:
                work->scale = 0xD4;
                work->angle = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                _effectChooseThrownModelDirection(work);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_M93R:
                work->scale = 0x100;
                work->angle = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                _effectChooseThrownModelDirection(work);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_M950:
                work->scale = 0x114;
                work->angle = EFFECT_THROWN_MODEL_MEDIUM_BLINK_AGE;
                _effectChooseThrownModelDirection(work);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_MP5A5:
            case EFFECT_THROWN_MODEL_PROFILE_MP5A5_UPGRADE_1:
            case EFFECT_THROWN_MODEL_PROFILE_MP5A5_UPGRADE_2:
                work->scale = 0x114;
                work->angle = EFFECT_THROWN_MODEL_SHORT_BLINK_AGE;
                _effectChooseThrownModelDirection(work);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_P229:
            case EFFECT_THROWN_MODEL_PROFILE_NPC_PISTOL:
                work->scale = 0xD4;
                work->angle = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                _effectChooseThrownModelDirection(work);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_MONGOOSE:
                work->angle = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                RANDOM_LCG_ADVANCE();
                work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vy = (RANDOM_LCG_DRAW() & 0x3F) + 0x40;
                RANDOM_LCG_ADVANCE();
                work->move.vz = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->scale = (RANDOM_LCG_DRAW() & 0x3F) + 0x40;
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_M4A1:
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_UPGRADE_1:
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_UPGRADE_2:
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_HAMMER:
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_BAYONET:
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_GRENADE:
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_PYKE:
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_JAVELIN:
                work->scale = 0x114;
                work->angle = EFFECT_THROWN_MODEL_MEDIUM_BLINK_AGE;
                _effectChooseThrownModelDirection(work);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_M249:
                work->scale = 0x114;
                work->angle = EFFECT_THROWN_MODEL_MIN_BLINK_AGE;
                RANDOM_LCG_ADVANCE();
                work->move.vx = (RANDOM_LCG_DRAW() & 0x3F) + 0x40;
                RANDOM_LCG_ADVANCE();
                work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vz = 0xFF80 - (RANDOM_LCG_DRAW() & 0x3F);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_PA3:
            case EFFECT_THROWN_MODEL_PROFILE_SP12:
            case EFFECT_THROWN_MODEL_PROFILE_AS12:
                work->scale = 0xBF;
                work->angle = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                _effectChooseThrownModelDirection(work);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_GUNBLADE:
                work->scale = 0xBF;
                work->angle = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                RANDOM_LCG_ADVANCE();
                work->move.vx = 0xFFA0 - (RANDOM_LCG_DRAW() & 0x3F);
                RANDOM_LCG_ADVANCE();
                work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x7F);
                RANDOM_LCG_ADVANCE();
                work->move.vz = -(RANDOM_LCG_DRAW() & 0x7F);
                memset(&launchVector, 0, sizeof(launchVector));
                launchVector.vx     = work->move.vx;
                launchVector.vy     = work->move.vy;
                launchVector.vz     = work->move.vz;
                rotatedLaunchVector = launchVector;
                ApplyTransposeMatrixLV(&coord->coord, &rotatedLaunchVector, &rotatedLaunchVector);
                work->move.vx = rotatedLaunchVector.vx;
                work->move.vy = rotatedLaunchVector.vy;
                work->move.vz = rotatedLaunchVector.vz;
                break;
            case EFFECT_THROWN_MODEL_PROFILE_GRENADE_PISTOL:
                work->scale = 0x60;
                work->angle = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                RANDOM_LCG_ADVANCE();
                work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vz = ((RANDOM_LCG_DRAW() & 0x1F) + 0x10);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_MM1:
                work->scale = 0x80;
                work->angle = EFFECT_THROWN_MODEL_MEDIUM_BLINK_AGE;
                RANDOM_LCG_ADVANCE();
                work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vz = 0xFFC0 - (RANDOM_LCG_DRAW() & 0x3F);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_GRENADE_EJECTION:
                work->scale = 0x60;
                work->angle = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                RANDOM_LCG_ADVANCE();
                work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vz = 0xFFF0 - (RANDOM_LCG_DRAW() & 0x1F);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
                break;
        }
        VectorNormalSS(&work->move, &work->move);
        RANDOM_LCG_ADVANCE();
        work->pos.vx = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
        RANDOM_LCG_ADVANCE();
        work->pos.vy = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
        RANDOM_LCG_ADVANCE();
        work->pos.vz        = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        task->state         = EFFECT_DRAW_TASK_ACTIVE;
        gfxRotMatrixX(&coord->coord, EFFECT_DRAW_FULL_TURN / 2, GRAPHICS_ROTATION_COMPOSE);
        return;
    }
    // Probe the attempted displacement in view space before accepting a rebound.
    gfxRotMatrixXYZ(&coord->coord, &work->pos, GRAPHICS_ROTATION_COMPOSE);
    MatrixNormal(&coord->coord, &coord->coord);
    gte_lddp(work->scale);
    direction = &work->move;
    gte_ldsv(direction);
    gte_gpf12();
    gte_stsv(&displacement);
    coord->coord.t[0]  += displacement.vx;
    coord->coord.t[1]  += displacement.vy;
    coord->coord.t[2]  += displacement.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&displacement);
    gte_rtv0();
    gte_stsv(&probeEnd);
    probeStartOrNormal.vx = (u16)coord->workm.t[0];
    probeStartOrNormal.vy = (u16)coord->workm.t[1];
    probeStartOrNormal.vz = (u16)coord->workm.t[2];
    probeEnd.vx          += probeStartOrNormal.vx;
    probeEnd.vy          += probeStartOrNormal.vy;
    probeEnd.vz          += probeStartOrNormal.vz;
    if (worldCollisionProbeGridSegment(&probeEnd, &probeStartOrNormal, &probeEnd, &probeStartOrNormal) == 1) {
        // The probe replaces its start vector with the room-space Q12 normal.
        coord->coord.t[0] -= displacement.vx;
        coord->coord.t[1] -= displacement.vy;
        coord->coord.t[2] -= displacement.vz;
        work->move.vx      = (probeStartOrNormal.vx >> 1) + (work->move.vx >> 1);
        work->move.vy      = probeStartOrNormal.vy + (work->move.vy >> 1);
        work->move.vz      = (probeStartOrNormal.vz >> 1) + (work->move.vz >> 1);
        VectorNormalSS(direction, direction);
        work->scale = (work->scale * 2) / 3;
        gte_lddp(work->scale);
        gte_ldsv(direction);
        gte_gpf12();
        gte_stsv(&displacement);
        coord->coord.t[0] += displacement.vx;
        coord->coord.t[1] += displacement.vy;
        coord->coord.t[2] += displacement.vz;
    } else {
        work->move.vy += EFFECT_THROWN_MODEL_GRAVITY;
    }
    // Blink on alternate display frames before releasing the piece.
    work->age++;
    if (work->angle < work->age) {
        model->flags = (gDisplayState.animFrame & 1) ? model->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW : model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if (work->angle * 2 < work->age) {
            effectKillTask(work, task);
        }
    }
}

void effectControlTask6E(Task* task)
{
    enum {
        EFFECT_RELOAD_SHORT_SPARK_ARG    = (1 << 16) | EFFECT_MUZZLE_SPARK_RANDOM_DRIFT | 512,
        EFFECT_RELOAD_MEDIUM_SPARK_ARG   = (2 << 16) | EFFECT_MUZZLE_SPARK_RANDOM_DRIFT | 512,
        EFFECT_RELOAD_LONG_SPARK_ARG     = (3 << 16) | EFFECT_MUZZLE_SPARK_RANDOM_DRIFT | 512,
        EFFECT_RELOAD_DURATION_GATE_MASK = 0xF0000,
        EFFECT_RELOAD_DEFAULT_TICKS      = 12,
    };
    EffectWork* work;
    GfxCoord*   ejectionCoord;
    s32         randomState;

    ejectionCoord = task->extra.coordBody->coord;
    work          = task->spawnArg2.pointer;
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        if (task->spawnArg1.value & EFFECT_RELOAD_DURATION_GATE_MASK) {
            work->scale = (u32)task->spawnArg1.value >> 16;
        } else {
            work->scale = EFFECT_RELOAD_DEFAULT_TICKS;
        }
        task->spawnArg1.value = (u16)task->spawnArg1.value;
        EFFECT_ATTACH_RELOAD_EMITTER(ejectionCoord, work, task->spawnArg1.value);
        actorRenderComposeCoord(ejectionCoord);
        task->state     = EFFECT_DRAW_TASK_ACTIVE;
        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomState;
        effectSpawn(EFFECT_MUZZLE_SPARK, ejectionCoord, (((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) | EFFECT_RELOAD_SHORT_SPARK_ARG, 0);
        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomState;
        effectSpawn(EFFECT_MUZZLE_SPARK, ejectionCoord, (((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) | EFFECT_RELOAD_MEDIUM_SPARK_ARG, 0);
        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomState;
        effectSpawn(EFFECT_MUZZLE_SPARK, ejectionCoord, (((u32)randomState >> 16) & EFFECT_WEAPON_FLASH_SIZE_JITTER_MASK) | EFFECT_RELOAD_LONG_SPARK_ARG, 0);
    }
    // The initial sparks are independent; emit one model piece on every update.
    effectSpawn(EFFECT_091, ejectionCoord, task->spawnArg1.value, 0);
    work->age++;
    if (work->age > work->scale - 1) {
        effectKillTask(work, task);
    }
}

#undef EFFECT_ATTACH_RELOAD_EMITTER

void effectControlTask6D(Task* task)
{
    enum {
        EFFECT_WEAPON_FLASH_PROFILE_MONGOOSE = 9,
        EFFECT_RELOAD_CASING_COUNT           = 6,
    };
    GfxCoord*   dropCoord;
    EffectWork* work;
    s32         casingIndex;

    casingIndex = 0;
    dropCoord   = task->extra.coordBody->coord;
    work        = task->spawnArg2.pointer;
    // Drop in the view-parent frame, retaining the spawn translation.
    gfxSetRotIdentity(&dropCoord->coord);
    dropCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(dropCoord);

    for (; casingIndex < EFFECT_RELOAD_CASING_COUNT; casingIndex++) {
        effectSpawn(EFFECT_BULLET_CASING, dropCoord, EFFECT_WEAPON_FLASH_PROFILE_MONGOOSE, 0);
    }

    effectKillTask(work, task);
}

void effectTileTaskA4(Task* task)
{
    EffectWork*               work;
    GfxCoord*                 coord;
    _EffectPixelSparkScratch* scratch;
    TILE*                     tile;
    s16                       brightness;

    coord   = task->extra.coordBody->coord;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_EffectPixelSparkScratch);
    work    = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        if (task->spawnArg1.value != 0) {
            // This mode retains the zeroed size and tint shift supplied by spawn.
            RANDOM_LCG_ADVANCE();
            work->move.vx = 0x20 - (RANDOM_LCG_DRAW() & 0x3F);
            RANDOM_LCG_ADVANCE();
            work->move.vy = 0x20 - (RANDOM_LCG_DRAW() & 0x3F);
            RANDOM_LCG_ADVANCE();
            work->move.vz = 0x20 - (RANDOM_LCG_DRAW() & 0x3F);
        } else {
            RANDOM_LCG_ADVANCE();
            work->move.vx = 0x40 - (RANDOM_LCG_DRAW() & 0x7F);
            RANDOM_LCG_ADVANCE();
            work->move.vy = (RANDOM_LCG_DRAW() & 0xFF) + 0x100;
            RANDOM_LCG_ADVANCE();
            work->move.vz = 0x40 - (RANDOM_LCG_DRAW() & 0x7F);

            gte_SetRotMatrix(&work->parent->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);

            RANDOM_LCG_ADVANCE();
            work->scale = RANDOM_LCG_DRAW() % 3 + 1;
            RANDOM_LCG_ADVANCE();
            work->angle = (RANDOM_LCG_DRAW() & 1) + 1;
        }
        task->state = EFFECT_DRAW_TASK_ACTIVE;
    }
    // Motion affects the next composition; this tile uses the current work matrix.
    coord->coord.t[0]     += work->move.vx;
    coord->coord.t[1]     += work->move.vy;
    coord->coord.t[2]     += work->move.vz;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    scratch->worldPoint.vx = (u16)coord->workm.t[0];
    scratch->worldPoint.vy = (u16)coord->workm.t[1];
    scratch->worldPoint.vz = (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenPoint);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth = scratch->depth + 1;
        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        setTile(tile);
        brightness = 0xFF - (u16)work->age * 0x10;
        tile->w    = work->angle;
        tile->h    = work->angle;
        tile->r0   = brightness;
        tile->g0   = brightness >> work->scale;
        tile->b0   = brightness >> 3;
        tile->x0   = scratch->screenPoint.vx;
        tile->y0   = scratch->screenPoint.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                tile);
        gpuSetPrimitiveBlendMode(tile, GPU_BLEND_ADD, scratch->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_EffectPixelSparkScratch);
    work->age++;
    if (work->age >= EFFECT_PIXEL_SPARK_TICKS) {
        effectKillTask(work, task);
    }
}

void effectControlTask3B(Task* task)
{
    enum {
        EFFECT_IMPACT_SPARK_DEFAULT_SIZE    = 512,
        EFFECT_IMPACT_SPARK_TILE_COUNT      = 6,
        EFFECT_IMPACT_SPARK_TILE_DRIFT_MODE = 1,
        EFFECT_IMPACT_SPARK_FRAME_COUNT     = 4,
    };
    EffectWork* work;
    GfxCoord*   impactCoord;
    s16         effectControl;
    s32         sparkIndex;
    s32         randomState;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    impactCoord   = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(impactCoord);
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
            work->scale     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            gRandomLcgState = randomState;
            if (task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK) {
                work->angle = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            } else {
                work->angle = EFFECT_IMPACT_SPARK_DEFAULT_SIZE;
            }
            for (sparkIndex = 0; sparkIndex < EFFECT_IMPACT_SPARK_TILE_COUNT; sparkIndex++) {
                effectSpawn(EFFECT_PIXEL_SPARK, impactCoord, EFFECT_IMPACT_SPARK_TILE_DRIFT_MODE, 0);
            }
            task->state = EFFECT_DRAW_TASK_ACTIVE;
        }
        _effectDrawImpactSparkFlash(impactCoord, work->age, work->angle, work->scale);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        work->age++;
        if (work->age < EFFECT_IMPACT_SPARK_FRAME_COUNT) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(work, task);
}

/// Draws one of the four rotated 24-texel impact-flash frames.
///
/// coord must already be composed and frame must be 0..3. size * 23 / depth
/// is the screen half-diagonal before rotation; depth is SZ3 / 4 + 1.
/// angle uses 4096 units per turn. The raw texture is drawn additively.
static void _effectDrawImpactSparkFlash(const GfxCoord* coord, u16 frame, s16 size, s16 angle)
{
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s32                 frameUOffset;
    s32                 perpendicularAngle;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = (u16)coord->workm.t[0];
    block->worldPoint.vy = (u16)coord->workm.t[1];
    block->worldPoint.vz = (u16)coord->workm.t[2];
    // Project the centre; the texture rotates in screen space after the divide.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth   = block->depth + 1;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
        quad->tpage            = getTPage(0, GPU_BLEND_ADD, 512, 0);
        quad->clut             = getClut(16, 265);
        frameUOffset           = frame * EFFECT_IMPACT_SPARK_CELL_SIZE;
        quad->u0               = frameUOffset - 0x70;
        quad->u2               = frameUOffset - 0x70;
        quad->v0               = 0;
        quad->u1               = frameUOffset - 0x59;
        quad->v1               = 0;
        quad->v2               = (EFFECT_IMPACT_SPARK_CELL_SIZE - 1);
        quad->u3               = frameUOffset - 0x59;
        quad->v3               = (EFFECT_IMPACT_SPARK_CELL_SIZE - 1);
        block->extent.corner.x = (((size * 23) / block->depth) * rsin(angle)) >> 12;
        block->extent.corner.y = (((size * 23) / block->depth) * rcos(angle)) >> 12;
        quad->x0               = block->screenX + (u16)block->extent.corner.x;
        quad->x3               = block->screenX - (u16)block->extent.corner.x;
        quad->y0               = block->screenY - (u16)block->extent.corner.y;
        quad->y3               = block->screenY + (u16)block->extent.corner.y;
        perpendicularAngle     = angle + EFFECT_DRAW_QUARTER_TURN;
        block->extent.corner.x = (((size * 23) / block->depth) * rsin(perpendicularAngle)) >> 12;
        block->extent.corner.y = (((size * 23) / block->depth) * rcos(perpendicularAngle)) >> 12;
        quad->x1               = block->screenX + (u16)block->extent.corner.x;
        quad->x2               = block->screenX - (u16)block->extent.corner.x;
        quad->y1               = block->screenY - (u16)block->extent.corner.y;
        quad->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void effectSpriteTask5C(Task* task)
{
    enum {
        EFFECT_EXPLOSION_DEFAULT_SIZE   = 512,
        EFFECT_EXPLOSION_DEFAULT_PERIOD = 2,
        EFFECT_EXPLOSION_PERIOD_MASK    = 0xF000,
        EFFECT_EXPLOSION_PERIOD_SHIFT   = 12,
        EFFECT_EXPLOSION_MAX_PERIOD     = 15,
        EFFECT_EXPLOSION_DIRECTION_MASK = 0xF,
        EFFECT_EXPLOSION_OPTION_MASK    = 0xF0000000,
        EFFECT_EXPLOSION_FAST_CHILD_ARG = (2 << 24) | (1 << 12),
        EFFECT_EXPLOSION_SLOW_CHILD_ARG = (1 << 24) | (2 << 12),
        EFFECT_EXPLOSION_RAW_BLEND_BITS = 3,
    };
    EffectShapeScratch*             scratch;
    GfxCoord*                       explosionCoord;
    EffectWork*                     work;
    POLY_FT4*                       quad;
    const EffectSpriteTextureFrame* textureFrame;
    s16                             effectControl;
    s16                             size;
    s16                             framePeriod;
    s32                             randomState;
    s32                             childIndex;
    s32                             childCount;
    s32                             nextZ;
    s32                             spawnOptions;

    work           = task->spawnArg2.pointer;
    effectControl  = gRoomEffectState->effectControl;
    explosionCoord = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        actorRenderComposeCoord(explosionCoord);
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            size = EFFECT_EXPLOSION_DEFAULT_SIZE;
            if (task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK) {
                size = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            }
            work->scale     = size;
            randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
            work->angle     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            gRandomLcgState = randomState;
            if (task->spawnArg1.value & EFFECT_EXPLOSION_PERIOD_MASK) {
                framePeriod = (task->spawnArg1.value >> EFFECT_EXPLOSION_PERIOD_SHIFT) & EFFECT_EXPLOSION_MAX_PERIOD;
            } else {
                framePeriod = EFFECT_EXPLOSION_DEFAULT_PERIOD;
            }
            work->period = framePeriod;
            work->step   = (s32)((u16)work->scale << 16) >> 23;
            spawnOptions = task->spawnArg1.signedBytes[3];
            work->index  = spawnOptions & EFFECT_EXPLOSION_DIRECTION_MASK;
            if (work->index != 0) {
                RANDOM_LCG_ADVANCE();
                work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vy = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                work->move.vz = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                gte_lddp(work->scale << 3);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&work->move);
                gte_lddp(work->index << 12);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&work->move);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            } else if (!(task->spawnArg1.value & EFFECT_EXPLOSION_OPTION_MASK)) {
                childCount = gDisplayState.animFrame & 3;
                childIndex = 0;
                if (childCount != 0) {
                    do {
                        effectSpawn(EFFECT_EXPLOSION, explosionCoord, ((s32)((u16)work->scale << 16) >> 17) | EFFECT_EXPLOSION_FAST_CHILD_ARG, 0);
                        childIndex += 1;
                    } while (childIndex < childCount);
                }
                childCount = gDisplayState.animFrame % 3;
                childIndex = 0;
                if (childCount > 0) {
                    do {
                        effectSpawn(EFFECT_EXPLOSION, explosionCoord, ((s32)((u16)work->scale << 16) >> 17) | EFFECT_EXPLOSION_SLOW_CHILD_ARG, 0);
                        childIndex += 1;
                    } while (childIndex < childCount);
                }
            }
            task->state = EFFECT_DRAW_TASK_ACTIVE;
        }
        // Project the composed centre; bias depth before billboard sizing.
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = explosionCoord->workm.t[0];
        scratch->worldPoint.vy = explosionCoord->workm.t[1];
        scratch->worldPoint.vz = explosionCoord->workm.t[2];
        _effectProjectShapeCentre(scratch);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            scratch->depth = scratch->depth + 1;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            textureFrame = &gEffectSpriteAtlasFrames[work->age / work->period];
            quad->code  |= EFFECT_EXPLOSION_RAW_BLEND_BITS;
            quad->tpage  = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
            _effectSetExplosionTexture(quad, textureFrame);
            // Size the rotated billboard from its growing size and biased depth.
            EFFECT_SET_PUFF_CORNERS(quad, scratch, work, EFFECT_SPRITE_ATLAS_UV_SPAN);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        // Motion, growth and age advance only after a running draw.
        explosionCoord->coord.t[0]  += work->move.vx;
        explosionCoord->coord.t[1]  += work->move.vy;
        nextZ                        = explosionCoord->coord.t[2] + work->move.vz;
        explosionCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        explosionCoord->coord.t[2]   = nextZ;
        work->scale                 += work->step;
        work->age++;
        if (work->age <= work->period * ARRAY_SIZE(gEffectSpriteAtlasFrames) - 1) {
            return;
        }
    }
    effectKillTask(work, task);
}

void effectSpriteTask70(Task* task)
{
    enum {
        EFFECT_SMOKE_PUFF_DEFAULT_SIZE    = 512,
        EFFECT_SMOKE_PUFF_PERIOD_MASK     = 0xF000,
        EFFECT_SMOKE_PUFF_PERIOD_SHIFT    = 12,
        EFFECT_SMOKE_PUFF_MAX_PERIOD      = 15,
        EFFECT_SMOKE_PUFF_MOTION_MASK     = 0xFF0000,
        EFFECT_SMOKE_PUFF_MOTION_SHIFT    = 16,
        EFFECT_SMOKE_PUFF_MAX_MOTION      = 255,
        EFFECT_SMOKE_PUFF_DIRECTION_MASK  = 0xF,
        EFFECT_SMOKE_PUFF_PARENT_RISE     = 1,
        EFFECT_SMOKE_PUFF_VIEW_RISE       = 1,
        EFFECT_SMOKE_PUFF_VIEW_DRIFT_RISE = 2,
        EFFECT_SMOKE_PUFF_VIEW_DRIFT      = 3,
        EFFECT_SMOKE_PUFF_CHILDREN        = 0x30000000,
        EFFECT_SMOKE_PUFF_BLEND_MASK      = 0xC0000000,
        EFFECT_SMOKE_PUFF_BLEND_SHIFT     = 30,
        EFFECT_SMOKE_PUFF_FAST_CHILD_ARG  = (6 << 24) | (2 << 12),
        EFFECT_SMOKE_PUFF_SLOW_CHILD_ARG  = (4 << 24) | (3 << 12),
        EFFECT_SMOKE_PUFF_CELL_SIZE       = 32,
        EFFECT_SMOKE_PUFF_UV_SPAN         = EFFECT_SMOKE_PUFF_CELL_SIZE - 1,
        EFFECT_SMOKE_PUFF_CELL_SHIFT      = 5,
        EFFECT_SMOKE_PUFF_TEXTURE_V       = 24,
        EFFECT_SMOKE_PUFF_FRAME_COUNT     = 8,
    };
    EffectShapeScratch* scratch;
    EffectWork*         work;
    GfxCoord*           puffCoord;
    POLY_FT4*           quad;
    s16                 effectControl;
    s16                 size;
    s16                 directionMode;
    s32                 spawnOptions;
    s32                 childIndex;
    s32                 childCount;
    s32                 childOptions;
    s32                 childSize;
    s32                 secondaryChildSize;
    s32                 secondaryChildOptions;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    puffCoord     = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        actorRenderComposeCoord(puffCoord);
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            size = EFFECT_SMOKE_PUFF_DEFAULT_SIZE;
            if (task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK) {
                size = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            }
            work->scale = size;
            RANDOM_LCG_ADVANCE();
            work->angle = RANDOM_LCG_DRAW() & EFFECT_DRAW_ANGLE_MASK;
            if (task->spawnArg1.value & EFFECT_SMOKE_PUFF_PERIOD_MASK) {
                work->period = (task->spawnArg1.value >> EFFECT_SMOKE_PUFF_PERIOD_SHIFT) & EFFECT_SMOKE_PUFF_MAX_PERIOD;
            } else {
                work->period = 1;
            }
            if (task->spawnArg1.value & EFFECT_SMOKE_PUFF_MOTION_MASK) {
                work->step = (task->spawnArg1.value >> EFFECT_SMOKE_PUFF_MOTION_SHIFT) & EFFECT_SMOKE_PUFF_MAX_MOTION;
            } else {
                work->step = work->scale >> 8;
            }
            spawnOptions  = task->spawnArg1.signedBytes[3];
            directionMode = spawnOptions & EFFECT_SMOKE_PUFF_DIRECTION_MASK;
            work->index   = directionMode;
            // Nonzero modes choose a displacement in the borrowed parent basis.
            if (directionMode != 0) {
                if (directionMode == EFFECT_SMOKE_PUFF_PARENT_RISE) {
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = 8 - (RANDOM_LCG_DRAW() & 0xF);
                    RANDOM_LCG_ADVANCE();
                    work->move.vy = (RANDOM_LCG_DRAW() & 0xF) * 3;
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = 8 - (RANDOM_LCG_DRAW() & 0xF);
                } else {
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                    RANDOM_LCG_ADVANCE();
                    work->move.vy = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                    gte_lddp((work->scale << 3));
                    gte_ldsv(&work->move);
                    gte_gpf12();
                    gte_stsv(&work->move);
                    gte_lddp((work->index << 11));
                    gte_ldsv(&work->move);
                    gte_gpf12();
                    gte_stsv(&work->move);
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            } else {
                switch (work->step) {
                    case EFFECT_SMOKE_PUFF_VIEW_RISE:
                        work->move.vx = 0;
                        work->move.vz = 0;
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = -(RANDOM_LCG_DRAW() & 0xF) - 0x20;
                        break;
                    case EFFECT_SMOKE_PUFF_VIEW_DRIFT_RISE:
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = -((gRandomLcgState >> 0x10) & 0x1F) - 0x10;
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        break;
                    case EFFECT_SMOKE_PUFF_VIEW_DRIFT:
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        gte_lddp((work->scale << 2));
                        gte_ldsv(&work->move);
                        gte_gpf12();
                        gte_stsv(&work->move);
                        break;
                }
            }
            if (task->spawnArg1.value & EFFECT_SMOKE_PUFF_CHILDREN) {
                childCount = gDisplayState.animFrame & 3;
                for (childIndex = 0; childIndex < childCount; childIndex++) {
                    childSize    = work->scale - (work->scale >> 2);
                    childOptions = (task->spawnArg1.value & EFFECT_SMOKE_PUFF_BLEND_MASK) | EFFECT_SMOKE_PUFF_FAST_CHILD_ARG;
                    effectSpawn(EFFECT_SMOKE_PUFF, puffCoord, childSize | childOptions, 0);
                }
                childCount = gDisplayState.animFrame % 3;
                for (childIndex = 0; childIndex < childCount; childIndex++) {
                    secondaryChildSize    = work->scale - (work->scale >> 2);
                    secondaryChildOptions = (task->spawnArg1.value & EFFECT_SMOKE_PUFF_BLEND_MASK) | EFFECT_SMOKE_PUFF_SLOW_CHILD_ARG;
                    effectSpawn(EFFECT_SMOKE_PUFF, puffCoord, secondaryChildSize | secondaryChildOptions, 0);
                }
            }
            task->state = EFFECT_DRAW_TASK_ACTIVE;
        }
        // Draw the current centre; the next running update commits drift.
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = puffCoord->workm.t[0];
        scratch->worldPoint.vy = puffCoord->workm.t[1];
        scratch->worldPoint.vz = puffCoord->workm.t[2];
        _effectProjectShapeCentre(scratch);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            scratch->depth++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
            if (task->spawnArg1.value & EFFECT_SMOKE_PUFF_BLEND_MASK) {
                quad->tpage = getTPage(0, ((((u32)task->spawnArg1.value >> EFFECT_SMOKE_PUFF_BLEND_SHIFT) - 1) & 3), 512, 0);
            } else {
                quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
            }
            quad->clut = getClut(304, 265);
            quad->u0   = (work->age / work->period) << EFFECT_SMOKE_PUFF_CELL_SHIFT;
            quad->v0   = EFFECT_SMOKE_PUFF_TEXTURE_V;
            quad->u1   = ((work->age / work->period) << EFFECT_SMOKE_PUFF_CELL_SHIFT) + EFFECT_SMOKE_PUFF_UV_SPAN;
            quad->v1   = EFFECT_SMOKE_PUFF_TEXTURE_V;
            quad->u2   = (work->age / work->period) << EFFECT_SMOKE_PUFF_CELL_SHIFT;
            quad->v2   = EFFECT_SMOKE_PUFF_TEXTURE_V + EFFECT_SMOKE_PUFF_UV_SPAN;
            quad->u3   = ((work->age / work->period) << EFFECT_SMOKE_PUFF_CELL_SHIFT) + EFFECT_SMOKE_PUFF_UV_SPAN;
            quad->v3   = EFFECT_SMOKE_PUFF_TEXTURE_V + EFFECT_SMOKE_PUFF_UV_SPAN;
            EFFECT_SET_ROTATED_SPRITE_CORNERS(quad, scratch, work, EFFECT_SMOKE_PUFF_UV_SPAN);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        puffCoord->coord.t[0]  += work->move.vx;
        puffCoord->coord.t[1]  += work->move.vy;
        puffCoord->coord.t[2]  += work->move.vz;
        puffCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->age++;
        if (work->age <= work->period * EFFECT_SMOKE_PUFF_FRAME_COUNT - 1) {
            return;
        }
    }
    effectKillTask(work, task);
}

void effectSpriteTask76(Task* task)
{
    enum {
        EFFECT_IMPACT_FLASH_DEFAULT_SIZE = 512,
        EFFECT_IMPACT_FLASH_PACKET_FLAGS = 3, // Raw texture and semitransparency
    };
    EffectShapeScratch* scratch;
    GfxCoord*           coord;
    EffectWork*         work;
    POLY_FT4*           quad;
    s16                 uvSpan;
    s16                 size;
    s32                 randomState;

    coord   = task->extra.coordBody->coord;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    work    = task->spawnArg2.pointer;
    // Stage the composed view position before attempting this frame.
    actorRenderComposeCoord(coord);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    _effectProjectShapeCentre(scratch);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth = scratch->depth + 1;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD & ~EFFECT_IMPACT_FLASH_PACKET_FLAGS);
        uvSpan = _gEffectImpactFlashFrames[work->age].uvSpan;
        // Initialize size and rotation only after an accepted projection.
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            if (task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK) {
                size = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            } else {
                size = EFFECT_IMPACT_FLASH_DEFAULT_SIZE;
            }
            work->scale     = size;
            randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = randomState;
            work->angle     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            task->state     = EFFECT_DRAW_TASK_ACTIVE;
        }
        quad->code |= EFFECT_IMPACT_FLASH_PACKET_FLAGS;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, _gEffectImpactFlashFrames[work->age].tpageX, 0);
        quad->clut  = getClut(_gEffectImpactFlashFrames[work->age].clutX, _gEffectImpactFlashFrames[work->age].clutY);
        quad->u0    = _gEffectImpactFlashFrames[work->age].u;
        quad->v0    = _gEffectImpactFlashFrames[work->age].v;
        quad->u1    = _gEffectImpactFlashFrames[work->age].u + uvSpan;
        quad->v1    = _gEffectImpactFlashFrames[work->age].v;
        quad->u2    = _gEffectImpactFlashFrames[work->age].u;
        quad->v2    = _gEffectImpactFlashFrames[work->age].v + uvSpan;
        quad->u3    = _gEffectImpactFlashFrames[work->age].u + uvSpan;
        quad->v3    = _gEffectImpactFlashFrames[work->age].v + uvSpan;
        EFFECT_SET_ROTATED_SPRITE_CORNERS(quad, scratch, work, uvSpan);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    // Offscreen frames still expire, independently of ordinary effect control.
    work->age++;
    if (work->age >= (s32)ARRAY_SIZE(_gEffectImpactFlashFrames)) {
        effectKillTask(work, task);
    }
}

void effectSpriteTask7C(Task* task)
{
    enum {
        EFFECT_BOUNCING_SPARK_DEFAULT_SIZE           = 512,
        EFFECT_BOUNCING_SPARK_PERIOD_MASK            = 0xF000,
        EFFECT_BOUNCING_SPARK_PERIOD_SHIFT           = 12,
        EFFECT_BOUNCING_SPARK_PERIOD_MAX             = 15,
        EFFECT_BOUNCING_SPARK_FRAME_COUNT            = 6,
        EFFECT_BOUNCING_SPARK_TEXTURE_V              = 88,
        EFFECT_BOUNCING_SPARK_TEXTURE_PAGE           = getTPage(0, GPU_BLEND_ADD, 512, 0),
        EFFECT_BOUNCING_SPARK_CLUT                   = getClut(160, 266),
        EFFECT_BOUNCING_SPARK_UNMODULATED_BRIGHTNESS = 128,
        EFFECT_BOUNCING_SPARK_FADE_AGE               = 24,
        EFFECT_BOUNCING_SPARK_FADE_SHIFT             = 4,
        EFFECT_BOUNCING_SPARK_TICKS                  = 31,
        EFFECT_BOUNCING_SPARK_GRAVITY                = 5,
    };
    GfxCoord            groundCoord;
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           quad;
    s16                 effectControl;
    s32                 randomState;
    s16                 size;
    s16                 ticksPerFrame;
    s32                 shade;
    s32                 fade;
    u32                 glowBrightness;

    work           = task->spawnArg2.pointer;
    effectControl  = gRoomEffectState->effectControl;
    coord          = task->extra.coordBody->coord;
    glowBrightness = EFFECT_BOUNCING_SPARK_UNMODULATED_BRIGHTNESS;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    if (work->index == 0) {
        size = EFFECT_BOUNCING_SPARK_DEFAULT_SIZE;
        if (task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK) {
            size = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
        }
        work->scale     = size;
        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
        work->angle     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
        gRandomLcgState = randomState;
        if (task->spawnArg1.value & EFFECT_BOUNCING_SPARK_PERIOD_MASK) {
            ticksPerFrame = (task->spawnArg1.value >> EFFECT_BOUNCING_SPARK_PERIOD_SHIFT) & EFFECT_BOUNCING_SPARK_PERIOD_MAX;
        } else {
            ticksPerFrame = 1;
        }
        work->period = ticksPerFrame;
        RANDOM_LCG_ADVANCE();
        work->step = 0x100 - (RANDOM_LCG_DRAW() & 0x1F0);
        RANDOM_LCG_ADVANCE();
        work->move.vx = 0x40 - (RANDOM_LCG_DRAW() & 0x7F);
        RANDOM_LCG_ADVANCE();
        work->move.vy = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
        RANDOM_LCG_ADVANCE();
        work->move.vz = 0x40 - (RANDOM_LCG_DRAW() & 0x7F);
        work->index++;
    }
    actorRenderComposeCoord(coord);
    // Project the current position before moving the spark for its next tick.
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    _effectProjectShapeCentre(block);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth   = block->depth + 1;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_ADDITIVE_TEXTURED_QUAD & ~2);
        fade = work->age;
        if (fade >= EFFECT_BOUNCING_SPARK_FADE_AGE) {
            shade = (EFFECT_BOUNCING_SPARK_TICKS - fade) << EFFECT_BOUNCING_SPARK_FADE_SHIFT;
            // Fitted: the image copies the shade into the register that held
            // the age and converts that copy to the glow byte, so the copy
            // and the conversion were in different blocks, and whatever
            // separated them left no instruction. What was tested, and
            // whether the two sides differed in the source, is unknown; this
            // test is a stand-in. That the copy is the age's own variable is
            // read off its register alone.
            if (shade > 0) {
                fade = shade;
            } else {
                fade = shade;
            }
            glowBrightness = (u8)fade;
            quad->r0       = shade;
            quad->g0       = shade;
            quad->b0       = shade;
        } else {
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD & ~2);
        }
        quad->tpage = EFFECT_BOUNCING_SPARK_TEXTURE_PAGE;
        setSemiTrans(quad, true);
        quad->clut = EFFECT_BOUNCING_SPARK_CLUT;
        quad->u0   = ((work->age / work->period) % EFFECT_BOUNCING_SPARK_FRAME_COUNT) * EFFECT_BOUNCING_SPARK_CELL_SIZE;
        quad->v0   = EFFECT_BOUNCING_SPARK_TEXTURE_V;
        quad->u1   = ((work->age / work->period) % EFFECT_BOUNCING_SPARK_FRAME_COUNT) * EFFECT_BOUNCING_SPARK_CELL_SIZE + (EFFECT_BOUNCING_SPARK_CELL_SIZE - 1);
        quad->v1   = EFFECT_BOUNCING_SPARK_TEXTURE_V;
        quad->u2   = ((work->age / work->period) % EFFECT_BOUNCING_SPARK_FRAME_COUNT) * EFFECT_BOUNCING_SPARK_CELL_SIZE;
        quad->v2   = EFFECT_BOUNCING_SPARK_TEXTURE_V + EFFECT_BOUNCING_SPARK_CELL_SIZE - 1;
        quad->u3   = ((work->age / work->period) % EFFECT_BOUNCING_SPARK_FRAME_COUNT) * EFFECT_BOUNCING_SPARK_CELL_SIZE + (EFFECT_BOUNCING_SPARK_CELL_SIZE - 1);
        quad->v3   = EFFECT_BOUNCING_SPARK_TEXTURE_V + EFFECT_BOUNCING_SPARK_CELL_SIZE - 1;
        EFFECT_SET_ROTATED_SPRITE_CORNERS(quad, block, work, EFFECT_BOUNCING_SPARK_UV_SPAN);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    work->move.vy += EFFECT_BOUNCING_SPARK_GRAVITY;
    work->angle   += work->step;
    work->age++;
    if (work->age >= EFFECT_BOUNCING_SPARK_TICKS) {
    release:
        effectKillTask(work, task);
        return;
    }
    // Glow only on hits; the retained bounce test also reads the output after misses.
    if (worldCollisionProjectGroundCoord(coord, &groundCoord) == 1) {
        effectDrawGroundGlow(&groundCoord, work->scale >> 1, glowBrightness);
    }
    if (coord->coord.t[1] > groundCoord.coord.t[1]) {
        coord->coord.t[1] -= work->move.vy * 2;
        work->move.vy      = -(work->move.vy >> 1);
        work->move.vx      = work->move.vx >> 1;
        work->move.vz      = work->move.vz >> 1;
    }
}

#undef EFFECT_SET_PUFF_CORNERS
#undef EFFECT_SET_ROTATED_SPRITE_CORNERS

void effectControlTask71(Task* task)
{
    enum {
        EFFECT_GRENADE_BURST_STATE_FLASH                  = 0,
        EFFECT_GRENADE_BURST_STATE_SMOKE                  = 1,
        EFFECT_GRENADE_BURST_STATE_LATE_SMOKE             = 2,
        EFFECT_GRENADE_BURST_LIGHT_SLOT                   = 1,
        EFFECT_GRENADE_BURST_LAST_AGE                     = 21,
        EFFECT_GRENADE_BURST_GLOW_TICKS                   = 8,
        EFFECT_GRENADE_BURST_GLOW_SIZE_STEP               = 64,
        EFFECT_GRENADE_BURST_BAND_COLOR_BIAS              = 128,
        EFFECT_GRENADE_BURST_BAND_COLOR_SHIFT             = 4,
        EFFECT_GRENADE_BURST_BLUE_GREEN_MASK              = 0xE0,
        EFFECT_GRENADE_BURST_TINT_COLOR_STEP              = 31,
        EFFECT_GRENADE_BURST_GLOW_SIZE_BIAS               = 64,
        EFFECT_GRENADE_BURST_IMPACT_SIZE                  = 1280,
        EFFECT_GRENADE_BURST_EXPLOSION_SIZE               = 1536,
        EFFECT_GRENADE_BURST_SPARK_SIZE                   = 256,
        EFFECT_GRENADE_BURST_SPARK_JITTER_MASK            = 63,
        EFFECT_GRENADE_BURST_SMOKE_JITTER_MASK            = 511,
        EFFECT_GRENADE_BURST_LATE_JITTER_MASK             = 255,
        EFFECT_GRENADE_BURST_FAST_SMOKE_ARG               = (3U << 30) | (1 << 28) | 1024,
        EFFECT_GRENADE_BURST_SLOW_SMOKE_ARG               = (2U << 30) | (2 << 24) | (3 << 12) | 1024,
        EFFECT_GRENADE_BURST_PAIR_SMOKE_ARG               = (2U << 30) | (2 << 24) | (4 << 12) | 1024,
        EFFECT_GRENADE_BURST_FRAGMENTATION_SMOKE_LAST_AGE = 7,
        EFFECT_GRENADE_BURST_FRAGMENTATION_LATE_LAST_AGE  = 11,
        EFFECT_GRENADE_BURST_AIRBURST_SMOKE_LAST_AGE      = 9,
        EFFECT_GRENADE_BURST_AIRBURST_LATE_LAST_AGE       = 13,
        EFFECT_GRENADE_BURST_FLASH_LAST_AGE               = 2,
        EFFECT_GRENADE_BURST_SMOKE_PAIR_COUNT             = 2,
        EFFECT_GRENADE_BURST_STREAK_COUNT                 = 3,
        EFFECT_GRENADE_BURST_STREAK_AGE_LIMIT             = 4,
    };
    u8                             glowRgb[3];
    EffectWork*                    work;
    GfxCoord*                      blastCoord;
    GfxCoord*                      lightCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    ModelObjectCoordBody*          coordBody;
    SVECTOR*                       spawnOffset;
    s16                            effectControl;
    s32                            warmGlowRadius;
    s32                            blueGlowRadius;
    s32                            particleIndex;
    s32                            randomState;
    s32                            redBrightnessEncoding;

/// Chooses the local displacement of a grenade burst's next child effect.
///
/// Consumes three consecutive draws from `gRandomLcgState`, one per axis.
/// Writes XYZ in -255..256 units of the blast coordinate's local frame;
/// the vector's fourth halfword is untouched. The caller supplies writable
/// storage and spawns each child before choosing another displacement.
/// `offset` must be a side-effect-free writable `SVECTOR*` expression; it is
/// evaluated three times. No caller locals are captured. The expansion is a
/// braced block; invoke only within a braced caller block.
#define EFFECT_CHOOSE_GRENADE_BURST_OFFSET(offset)                                                                  \
    {                                                                                                               \
        enum { EFFECT_GRENADE_BURST_OFFSET_CENTRE = 256,                                                            \
               EFFECT_GRENADE_BURST_OFFSET_MASK   = 511 };                                                            \
        RANDOM_LCG_ADVANCE();                                                                                       \
        (offset)->vx = EFFECT_GRENADE_BURST_OFFSET_CENTRE - (RANDOM_LCG_DRAW() & EFFECT_GRENADE_BURST_OFFSET_MASK); \
        RANDOM_LCG_ADVANCE();                                                                                       \
        (offset)->vy = EFFECT_GRENADE_BURST_OFFSET_CENTRE - (RANDOM_LCG_DRAW() & EFFECT_GRENADE_BURST_OFFSET_MASK); \
        RANDOM_LCG_ADVANCE();                                                                                       \
        (offset)->vz = EFFECT_GRENADE_BURST_OFFSET_CENTRE - (RANDOM_LCG_DRAW() & EFFECT_GRENADE_BURST_OFFSET_MASK); \
    }

    transientLight = &gWorldCoordTransientPointLights[EFFECT_GRENADE_BURST_LIGHT_SLOT];
    pointLight     = &transientLight->light;
    lightCoord     = &pointLight->head.transform.coord;
    coordBody      = task->extra.coordBody;
    work           = task->spawnArg2.pointer;
    effectControl  = gRoomEffectState->effectControl;
    blastCoord     = coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    actorRenderComposeCoord(blastCoord);
    work->age = work->age + 1;
    // The ammunition recipe stages child offsets in the blast coordinate frame.
    switch (task->spawnArg1.value) {
        case GRENADE_ROUND_FRAGMENTATION:
            switch (task->state) {
                case EFFECT_GRENADE_BURST_STATE_FLASH:
                    EFFECT_CHOOSE_GRENADE_BURST_OFFSET(&work->move);
                    spawnOffset = &work->move;
                    effectSpawn(EFFECT_EXPLOSION, blastCoord, EFFECT_GRENADE_BURST_EXPLOSION_SIZE, spawnOffset);
                    randomState = RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_BOUNCING_SPARK, blastCoord, (((u32)randomState >> 16) & EFFECT_GRENADE_BURST_SPARK_JITTER_MASK) | EFFECT_GRENADE_BURST_SPARK_SIZE, spawnOffset);
                    randomState = RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_BOUNCING_SPARK, blastCoord, (((u32)randomState >> 16) & EFFECT_GRENADE_BURST_SPARK_JITTER_MASK) | EFFECT_GRENADE_BURST_SPARK_SIZE, spawnOffset);
                    task->state++;
                    break;
                case EFFECT_GRENADE_BURST_STATE_SMOKE:
                    EFFECT_CHOOSE_GRENADE_BURST_OFFSET(&work->move);
                    randomState = RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_SMOKE_PUFF, blastCoord, (((u32)randomState >> 16) & EFFECT_GRENADE_BURST_SMOKE_JITTER_MASK) | EFFECT_GRENADE_BURST_FAST_SMOKE_ARG,
                                &work->move);
                    if (work->age >= EFFECT_GRENADE_BURST_FRAGMENTATION_SMOKE_LAST_AGE) {
                        task->state++;
                    }
                    break;
                case EFFECT_GRENADE_BURST_STATE_LATE_SMOKE:
                    EFFECT_CHOOSE_GRENADE_BURST_OFFSET(&work->move);
                    randomState = RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_SMOKE_PUFF, blastCoord, (((u32)randomState >> 16) & EFFECT_GRENADE_BURST_LATE_JITTER_MASK) | EFFECT_GRENADE_BURST_SLOW_SMOKE_ARG,
                                &work->move);
                    if (work->age >= EFFECT_GRENADE_BURST_FRAGMENTATION_LATE_LAST_AGE) {
                        task->state++;
                    }
                    break;
            }
            _effectRefreshGrenadeBurstLight(transientLight, pointLight, lightCoord, blastCoord, &work->age, ONE, EFFECT_WEAPON_FLASH_LIGHT_INTENSITY, ONE / 2);
            if (work->age >= EFFECT_GRENADE_BURST_LAST_AGE) {
                effectKillTask(work, task);
            }
            break;
        case GRENADE_ROUND_AIRBURST:
            switch (task->state) {
                case EFFECT_GRENADE_BURST_STATE_FLASH:
                    EFFECT_CHOOSE_GRENADE_BURST_OFFSET(&work->move);
                    effectSpawn(EFFECT_IMPACT_FLASH, blastCoord, EFFECT_GRENADE_BURST_IMPACT_SIZE, &work->move);
                    if (work->age >= EFFECT_GRENADE_BURST_FLASH_LAST_AGE) {
                        task->state++;
                    }
                    break;
                case EFFECT_GRENADE_BURST_STATE_SMOKE:
                    particleIndex = 0;
                    do {
                        EFFECT_CHOOSE_GRENADE_BURST_OFFSET(&work->move);
                        randomState = RANDOM_LCG_ADVANCE();
                        effectSpawn(EFFECT_SMOKE_PUFF, blastCoord, (((u32)randomState >> 16) & EFFECT_GRENADE_BURST_SMOKE_JITTER_MASK) | EFFECT_GRENADE_BURST_PAIR_SMOKE_ARG,
                                    &work->move);
                        particleIndex += 1;
                    } while (particleIndex < EFFECT_GRENADE_BURST_SMOKE_PAIR_COUNT);
                    if (work->age >= EFFECT_GRENADE_BURST_AIRBURST_SMOKE_LAST_AGE) {
                        task->state++;
                    }
                    break;
                case EFFECT_GRENADE_BURST_STATE_LATE_SMOKE:
                    particleIndex = 0;
                    do {
                        EFFECT_CHOOSE_GRENADE_BURST_OFFSET(&work->move);
                        randomState = RANDOM_LCG_ADVANCE();
                        effectSpawn(EFFECT_SMOKE_PUFF, blastCoord, (((u32)randomState >> 16) & EFFECT_GRENADE_BURST_LATE_JITTER_MASK) | EFFECT_GRENADE_BURST_FAST_SMOKE_ARG,
                                    &work->move);
                        particleIndex += 1;
                    } while (particleIndex < EFFECT_GRENADE_BURST_SMOKE_PAIR_COUNT);
                    if (work->age >= EFFECT_GRENADE_BURST_AIRBURST_LATE_LAST_AGE) {
                        task->state++;
                    }
                    break;
            }
            // Brightness wraps into RGB bytes before the band is queued.
            if (work->scale++ < EFFECT_GRENADE_BURST_GLOW_TICKS) {
                redBrightnessEncoding = -EFFECT_GRENADE_BURST_BAND_COLOR_BIAS - (work->scale << EFFECT_GRENADE_BURST_BAND_COLOR_SHIFT);
                glowRgb[0]            = redBrightnessEncoding;
                glowRgb[1]            = (glowRgb[0] * 3) >> 2;
                glowRgb[2]            = (glowRgb[0] * 2) / 3;
                warmGlowRadius        = (work->scale * EFFECT_GRENADE_BURST_GLOW_SIZE_STEP) + EFFECT_GRENADE_BURST_GLOW_SIZE_BIAS;
                effectDrawOuterGlowBand(blastCoord, (s16)warmGlowRadius, (s16)warmGlowRadius, glowRgb);
            }
            if (work->age < EFFECT_GRENADE_BURST_STREAK_AGE_LIMIT) {
                particleIndex = 0;
                do {
                    effectSpawn(EFFECT_SPARK_STREAK, blastCoord, 0, 0);
                    particleIndex += 1;
                } while (particleIndex < EFFECT_GRENADE_BURST_STREAK_COUNT);
            }
            _effectRefreshGrenadeBurstLight(transientLight, pointLight, lightCoord, blastCoord, &work->age, EFFECT_WEAPON_FLASH_LIGHT_INTENSITY, EFFECT_WEAPON_FLASH_LIGHT_INTENSITY, ONE / 2);
            if (work->age >= EFFECT_GRENADE_BURST_LAST_AGE) {
                effectKillTask(work, task);
            }
            break;
        case GRENADE_ROUND_RIOT:
            switch (task->state) {
                case EFFECT_GRENADE_BURST_STATE_FLASH:
                    EFFECT_CHOOSE_GRENADE_BURST_OFFSET(&work->move);
                    effectSpawn(EFFECT_IMPACT_FLASH, blastCoord, EFFECT_GRENADE_BURST_IMPACT_SIZE, &work->move);
                    if (work->age >= EFFECT_GRENADE_BURST_FLASH_LAST_AGE) {
                        task->state++;
                    }
                    break;
                case EFFECT_GRENADE_BURST_STATE_SMOKE:
                    particleIndex = 0;
                    do {
                        EFFECT_CHOOSE_GRENADE_BURST_OFFSET(&work->move);
                        randomState = RANDOM_LCG_ADVANCE();
                        effectSpawn(EFFECT_SMOKE_PUFF, blastCoord, (((u32)randomState >> 16) & EFFECT_GRENADE_BURST_SMOKE_JITTER_MASK) | EFFECT_GRENADE_BURST_PAIR_SMOKE_ARG,
                                    &work->move);
                        particleIndex += 1;
                    } while (particleIndex < EFFECT_GRENADE_BURST_SMOKE_PAIR_COUNT);
                    if (work->age >= EFFECT_GRENADE_BURST_AIRBURST_SMOKE_LAST_AGE) {
                        task->state++;
                    }
                    break;
            }
            // Brightness wraps into RGB bytes before the band is queued.
            if (work->scale++ < EFFECT_GRENADE_BURST_GLOW_TICKS) {
                glowRgb[2]     = (-EFFECT_GRENADE_BURST_BAND_COLOR_BIAS - (work->scale << EFFECT_GRENADE_BURST_BAND_COLOR_SHIFT)) * 2;
                glowRgb[1]     = (glowRgb[2] & EFFECT_GRENADE_BURST_BLUE_GREEN_MASK) >> 2;
                glowRgb[0]     = glowRgb[1];
                blueGlowRadius = (work->scale * EFFECT_GRENADE_BURST_GLOW_SIZE_STEP) + EFFECT_GRENADE_BURST_GLOW_SIZE_BIAS;
                effectDrawOuterGlowBand(blastCoord, (s16)blueGlowRadius, (s16)blueGlowRadius, glowRgb);
                if (gDisplayState.animFrame & 1) {
                    glowRgb[2] = ~(work->scale * EFFECT_GRENADE_BURST_TINT_COLOR_STEP);
                    glowRgb[1] = glowRgb[2] >> 2;
                    glowRgb[0] = glowRgb[1];
                    effectDrawScreenTint(glowRgb, GPU_BLEND_ADD);
                }
            }
            _effectRefreshGrenadeBurstLight(transientLight, pointLight, lightCoord, blastCoord, &work->age, ONE / 2, EFFECT_WEAPON_FLASH_LIGHT_INTENSITY, ONE);
            if (work->age >= EFFECT_GRENADE_BURST_LAST_AGE) {
                effectKillTask(work, task);
            }
            break;
    }

#undef EFFECT_CHOOSE_GRENADE_BURST_OFFSET
}

void effectLineTask92(Task* task)
{
    EffectLineScratch* block;
    EffectWork*        work;
    GfxCoord*          coord;
    LINE_F2*           line;
    s16                fadeRate;
    s32                randomState;
    s16                brightness;

    block = SCRATCH_STACK_RESERVE_BLOCK(EffectLineScratch);
    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    if (work->age == 0) {
        RANDOM_LCG_ADVANCE();
        work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
        RANDOM_LCG_ADVANCE();
        work->move.vy = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
        RANDOM_LCG_ADVANCE();
        work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
        RANDOM_LCG_ADVANCE();
        fadeRate = 2;
        if ((RANDOM_LCG_DRAW() & 3) != 0) {
            fadeRate = 1;
        }
        randomState = RANDOM_LCG_NEXT(gRandomLcgState);
        work->angle = (((u32)randomState >> 16) & 1) + 1;
        work->scale = fadeRate;
        gfxSetRotIdentity(&coord->coord);
        work->pos.vx    = (u16)coord->workm.t[0];
        work->pos.vy    = (u16)coord->workm.t[1];
        work->pos.vz    = (u16)coord->workm.t[2];
        gRandomLcgState = randomState;
    }
    // Advance next tick's translation; draw from the already composed position.
    coord->coord.t[0]     += work->move.vx;
    coord->coord.t[1]     += work->move.vy;
    coord->coord.t[2]     += work->move.vz;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    block->endpoints[0].vx = coord->workm.t[0];
    block->endpoints[0].vy = coord->workm.t[1];
    block->endpoints[0].vz = coord->workm.t[2];
    block->endpoints[1].vx = work->pos.vx;
    block->endpoints[1].vy = work->pos.vy;
    block->endpoints[1].vz = work->pos.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->endpoints[0]);
    gte_rtps();
    gte_stsxy(&block->screenEndpoints[0]);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_ldv0(&block->endpoints[1]);
        gte_rtps();
        gte_stsxy(&block->screenEndpoints[1]);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            line           = gGpuPrimCursor;
            block->depth   = block->depth + 1;
            gGpuPrimCursor = line + 1;
            setLineF2(line);
            brightness = 0xFF - (work->age << (6 - work->scale));
            if (task->spawnArg1.value != 0) {
                line->r0 = brightness >> 3;
                line->g0 = brightness >> work->angle;
                line->b0 = brightness;
            } else {
                line->r0 = brightness;
                line->g0 = brightness >> work->angle;
                line->b0 = brightness >> 3;
            }
            line->x0 = block->screenEndpoints[0].vx;
            line->y0 = block->screenEndpoints[0].vy;
            line->x1 = block->screenEndpoints[1].vx;
            line->y1 = block->screenEndpoints[1].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->depth);
            // Cache an endpoint only when both projections succeeded.
            work->pos.vx = (u16)coord->workm.t[0];
            work->pos.vy = (u16)coord->workm.t[1];
            work->pos.vz = (u16)coord->workm.t[2];
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectLineScratch);
    work->age++;
    if (work->age > work->scale * 8 - 1) {
        effectKillTask(work, task);
    }
}

void effectPolyTask9C(Task* task)
{
    enum {
        EFFECT_CRITICAL_HIT_INITIAL_BRIGHTNESS = 16,
        EFFECT_CRITICAL_HIT_INITIAL_RADIUS     = 32,
        EFFECT_CRITICAL_HIT_BRIGHTNESS_STEP    = 2,
        EFFECT_CRITICAL_HIT_TICKS              = 8,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
            if (task->state == EFFECT_DRAW_TASK_NEW) {
                work->scale  = EFFECT_CRITICAL_HIT_INITIAL_BRIGHTNESS;
                work->angle  = EFFECT_CRITICAL_HIT_INITIAL_RADIUS;
                work->period = _gEffectCriticalHitStyles[task->spawnArg1.value].color;
                work->step   = _gEffectCriticalHitStyles[task->spawnArg1.value].radiusStep;
                task->state++;
            }
            actorRenderComposeCoord(coord);
            // Advance before drawing: brightness runs from 14 down to zero.
            work->scale -= EFFECT_CRITICAL_HIT_BRIGHTNESS_STEP;
            work->angle += work->step;
            _effectDrawCriticalHitBurst(coord, work->angle, work->scale, work->period);
            work->age++;
            if (work->age < EFFECT_CRITICAL_HIT_TICKS) {
                return;
            }
        } else {
            return;
        }
    }
    effectKillTask(work, task);
}

/// Draws the additive critical-hit ring and optional four radial spikes.
///
/// The composed coordinate supplies the world centre. Outer radius is
/// radius * 256 / (SZ3 / 4 + 1) pixels; the inner radius is half of it.
/// Each RGB nibble in color weights brightness (the task supplies 0..14),
/// and EFFECT_CRITICAL_HIT_STYLE_SPIKES enables the randomized spikes.
static void _effectDrawCriticalHitBurst(const GfxCoord* coord, s16 radius, s16 brightness, u16 color)
{
    EffectShapeScratch* block;
    POLY_G4*            quad;
    POLY_G3*            tri;
    s32                 segmentAngle;
    s32                 nextSegmentAngle;
    s32                 randomState;
    s32                 spikeAngle;
    u8                  red;
    u8                  green;
    u8                  blue;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        // Sweep the outer and inner radii into a shaded ring.
        block->extent.burst.outer = (radius << 8) / block->depth;
        block->extent.burst.inner = (radius << 7) / block->depth;
        red                       = brightness * ((color >> 8) & 0xF);
        green                     = brightness * ((color >> 4) & 0xF);
        blue                      = brightness * (color & 0xF);
        segmentAngle              = 0;
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            quad->r0         = red;
            quad->r1         = red;
            quad->g0         = green;
            quad->b0         = blue;
            quad->g1         = green;
            quad->b1         = blue;
            quad->r2         = 0;
            quad->g2         = 0;
            quad->b2         = 0;
            quad->r3         = 0;
            quad->g3         = 0;
            quad->b3         = 0;
            nextSegmentAngle = _effectSetCriticalHitRingCorners(quad, block, segmentAngle);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                    quad);
            segmentAngle = nextSegmentAngle;
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->depth);
        } while (segmentAngle < EFFECT_DRAW_FULL_TURN);
        if (color & EFFECT_CRITICAL_HIT_STYLE_SPIKES) {
            // Reuse the sizing words for the optional radial spikes.
            block->extent.spike.radius    = 0x12000 / block->depth;
            block->extent.spike.halfWidth = 0x2400 / block->depth;
            segmentAngle                  = 0;
            do {
                randomState    = RANDOM_LCG_ADVANCE();
                tri            = gGpuPrimCursor;
                gGpuPrimCursor = tri + 1;
                setPolyG3(tri);
                tri->r0    = red;
                tri->g0    = green;
                tri->b0    = blue;
                tri->r1    = 0;
                tri->g1    = 0;
                tri->b1    = 0;
                tri->r2    = 0;
                tri->g2    = 0;
                tri->b2    = 0;
                spikeAngle = segmentAngle + (((u32)randomState >> 16) & 0x300);
                tri->x0    = block->screenX;
                tri->y0    = block->screenY;
                tri->x1    = block->screenX + ((block->extent.spike.radius * rsin(spikeAngle)) >> 12) +
                          ((block->extent.spike.halfWidth * rsin(spikeAngle + EFFECT_DRAW_THREE_QUARTER_TURN)) >> 12);
                tri->y1 = block->screenY + ((block->extent.spike.radius * rcos(spikeAngle)) >> 12) +
                          ((block->extent.spike.halfWidth * rcos(spikeAngle + EFFECT_DRAW_THREE_QUARTER_TURN)) >> 12);
                tri->x2 = block->screenX + ((block->extent.spike.radius * rsin(spikeAngle)) >> 12) +
                          ((block->extent.spike.halfWidth * rsin(spikeAngle + EFFECT_DRAW_QUARTER_TURN)) >> 12);
                tri->y2 = block->screenY + ((block->extent.spike.radius * rcos(spikeAngle)) >> 12) +
                          ((block->extent.spike.halfWidth * rcos(spikeAngle + EFFECT_DRAW_QUARTER_TURN)) >> 12);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                        tri);
                segmentAngle += EFFECT_DRAW_QUARTER_TURN;
                gpuSetPrimitiveBlendMode(tri, GPU_BLEND_ADD, block->depth);
            } while (segmentAngle < EFFECT_DRAW_FULL_TURN);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void effectSpriteTask9E(Task* task)
{
    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    EffectWork*        work;
    GfxCoord*          coord;
    POLY_FT4*          quad;
    s32                halfSize;
    s32                shade;
    u8                 wrappedBrightness;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        RANDOM_LCG_ADVANCE();
        gfxRotMatrixY(&coord->coord, RANDOM_LCG_DRAW() & EFFECT_DRAW_ANGLE_MASK, GRAPHICS_ROTATION_REPLACE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK) {
            halfSize = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
        } else {
            halfSize = EFFECT_RED_GROUND_GLOW_DEFAULT_HALF_SIZE;
        }
        work->scale = halfSize;
        work->angle = EFFECT_RED_GROUND_GLOW_TICKS - 1;
        task->state = EFFECT_DRAW_TASK_ACTIVE;
    }
    actorRenderComposeCoord(coord);

    // Build the square in the effect coordinate's horizontal plane.
    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis0Sign * (u16)work->scale;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis1Sign * (u16)work->scale;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += coord->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += coord->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += coord->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    shade             = -0x80 - (work->age >> 3);
    wrappedBrightness = shade;
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth += EFFECT_RED_GROUND_GLOW_DEPTH_BIAS;
        quad                = gGpuPrimCursor;
        gGpuPrimCursor      = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_ADDITIVE_TEXTURED_QUAD);
        quad->g0    = wrappedBrightness >> 2;
        quad->b0    = wrappedBrightness >> 2;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);
        quad->r0    = shade;
        quad->clut  = getClut(192, 266);
        quad->u0    = 0xA8;
        quad->v0    = 0xC8;
        quad->u1    = 0xDF;
        quad->v1    = 0xC8;
        quad->u2    = 0xA8;
        quad->v2    = 0xFF;
        quad->u3    = 0xDF;
        quad->v3    = 0xFF;
        quad->x0    = quadScratch->screenCorners[0].vx;
        quad->y0    = quadScratch->screenCorners[0].vy;
        quad->x1    = quadScratch->screenCorners[1].vx;
        quad->y1    = quadScratch->screenCorners[1].vy;
        quad->x2    = quadScratch->screenCorners[2].vx;
        quad->y2    = quadScratch->screenCorners[2].vy;
        quad->x3    = quadScratch->screenCorners[3].vx;
        quad->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
    work->age++;
    if (work->angle < work->age) {
        effectKillTask(work, task);
    }
}

void effectSpriteTask54(Task* task)
{
    enum {
        EFFECT_DUST_PUFF_PERIOD_MASK      = 0xF000,
        EFFECT_DUST_PUFF_PERIOD_SHIFT     = 12,
        EFFECT_DUST_PUFF_MAX_PERIOD       = 15,
        EFFECT_DUST_PUFF_CHILD_PERIOD_ARG = 3 << EFFECT_DUST_PUFF_PERIOD_SHIFT,
        EFFECT_DUST_PUFF_TEXTURE_V        = 24,
        EFFECT_DUST_PUFF_FRAME_COUNT      = 8,
    };
    EffectShapeScratch* block;
    s16                 framePeriod;
    s16                 nextAge;
    EffectWork*         work;
    GfxCoord*           puffCoord;
    POLY_FT4*           quad;

    work      = task->spawnArg2.pointer;
    puffCoord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    actorRenderComposeCoord(puffCoord);
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        RANDOM_LCG_ADVANCE();
        work->scale = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
        work->angle = RANDOM_LCG_DRAW() & EFFECT_DRAW_ANGLE_MASK;
        if (task->spawnArg1.value & EFFECT_DUST_PUFF_PERIOD_MASK) {
            framePeriod = (task->spawnArg1.value >> EFFECT_DUST_PUFF_PERIOD_SHIFT) & EFFECT_DUST_PUFF_MAX_PERIOD;
        } else {
            framePeriod = 1;
        }
        work->period = framePeriod;
        RANDOM_LCG_ADVANCE();
        work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
        RANDOM_LCG_ADVANCE();
        work->move.vy = -(RANDOM_LCG_DRAW() & 0xF);
        RANDOM_LCG_ADVANCE();
        work->move.vz = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
        if (task->spawnArg1.value < 0) {
            if (work->angle & 1) {
                effectSpawn(EFFECT_DUST_PUFF, puffCoord, ((work->scale * 3) >> 2) + EFFECT_DUST_PUFF_CHILD_PERIOD_ARG, NULL);
            }
            if (!(work->angle & 3)) {
                effectSpawn(EFFECT_DUST_PUFF, puffCoord, ((work->scale * 3) >> 2) + EFFECT_DUST_PUFF_CHILD_PERIOD_ARG, NULL);
            }
        }
        task->state = EFFECT_DRAW_TASK_ACTIVE;
    }

    // Project the drifting centre with the unmodified SZ3 / 4 depth.
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = (u16)puffCoord->workm.t[0];
    block->worldPoint.vy = (u16)puffCoord->workm.t[1];
    block->worldPoint.vz = (u16)puffCoord->workm.t[2];
    _effectProjectShapeCentre(block);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        quad->r0    = 0x68;
        quad->g0    = 0x70;
        quad->b0    = 0x38;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
        setSemiTrans(quad, true);
        quad->clut = getClut(304, 265);
        quad->u0   = (work->age / work->period) * EFFECT_DUST_PUFF_CELL_SIZE;
        quad->v0   = EFFECT_DUST_PUFF_TEXTURE_V;
        quad->u1   = ((work->age / work->period) * EFFECT_DUST_PUFF_CELL_SIZE) + (EFFECT_DUST_PUFF_CELL_SIZE - 1);
        quad->v1   = EFFECT_DUST_PUFF_TEXTURE_V;
        quad->u2   = (work->age / work->period) * EFFECT_DUST_PUFF_CELL_SIZE;
        quad->v2   = (EFFECT_DUST_PUFF_TEXTURE_V + EFFECT_DUST_PUFF_CELL_SIZE - 1);
        quad->u3   = ((work->age / work->period) * EFFECT_DUST_PUFF_CELL_SIZE) + (EFFECT_DUST_PUFF_CELL_SIZE - 1);
        quad->v3   = (EFFECT_DUST_PUFF_TEXTURE_V + EFFECT_DUST_PUFF_CELL_SIZE - 1);

        _effectSetDustPuffCorners(quad, block, work);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    // Draw while paused; commit displacement and age only when running.
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        puffCoord->coord.t[0]  += work->move.vx;
        puffCoord->coord.t[1]  += work->move.vy;
        puffCoord->coord.t[2]  += work->move.vz;
        puffCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        nextAge                 = work->age + 1;
        work->age               = nextAge;
        if (nextAge > (work->period * EFFECT_DUST_PUFF_FRAME_COUNT) - 1) {
            effectKillTask(work, task);
        }
    }
}

void effectDrawGroundGlow(const GfxCoord* coord, s32 halfSize, u32 brightness)
{
    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          quad;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis0Sign * halfSize;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += coord->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += coord->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += coord->workm.t[2];
    }

    // Project one corner, then the remaining three with the GTE triple transform.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        quad->r0    = brightness >> 1;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);
        quad->clut  = getClut(240, 268);
        quad->u0    = 0xE0;
        quad->v0    = 0xC8;
        quad->v1    = 0xC8;
        quad->u2    = 0xE0;
        quad->g0    = brightness;
        quad->b0    = brightness;
        quad->u1    = 0xFF;
        quad->v2    = 0xE7;
        quad->u3    = 0xFF;
        quad->v3    = 0xE7;
        setSemiTrans(quad, 1);
        quad->x0 = quadScratch->screenCorners[0].vx;
        quad->y0 = quadScratch->screenCorners[0].vy;
        quad->x1 = quadScratch->screenCorners[1].vx;
        quad->y1 = quadScratch->screenCorners[1].vy;
        quad->x2 = quadScratch->screenCorners[2].vx;
        quad->y2 = quadScratch->screenCorners[2].vy;
        quad->x3 = quadScratch->screenCorners[3].vx;
        quad->y3 = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

void effectDrawGroundShadow(const VECTOR3* centre, s32 halfSize, s16 shade)
{
    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          quad;

    if (shade >= 0 && gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
        gte_SetTransMatrix(&GsWSMATRIX);
        for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners); cornerIndex++) {
            quadScratch->vertices[cornerIndex].vx = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis0Sign * halfSize;
            quadScratch->vertices[cornerIndex].vy = 0;
            quadScratch->vertices[cornerIndex].vz = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis1Sign * halfSize;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&quadScratch->vertices[cornerIndex]);
            gte_rtv0();
            gte_stsv(&quadScratch->vertices[cornerIndex]);
            quadScratch->vertices[cornerIndex].vx += centre->vx;
            quadScratch->vertices[cornerIndex].vy += centre->vy;
            quadScratch->vertices[cornerIndex].vz += centre->vz;
        }

        _effectProjectGroundShadow(quadScratch);
        if (quadScratch->projectionFlags >= 0) {
            gte_stszotz(&quadScratch->depth);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            if (shade == 0) {
                setShadeTex(quad, 1);
            } else {
                quad->r0 = shade;
                quad->g0 = shade;
                quad->b0 = shade;
            }
            quad->tpage = getTPage(0, GPU_BLEND_SUBTRACT, 512, 0);
            quad->clut  = getClut(48, 266);
            quad->u0    = 0xC0;
            quad->v0    = 0x98;
            quad->v1    = 0x98;
            quad->u2    = 0xC0;
            quad->u1    = 0xF7;
            quad->v2    = 0xCF;
            quad->u3    = 0xF7;
            quad->v3    = 0xCF;
            setSemiTrans(quad, 1);
            quad->x0 = quadScratch->screenCorners[0].vx;
            quad->y0 = quadScratch->screenCorners[0].vy;
            quad->x1 = quadScratch->screenCorners[1].vx;
            quad->y1 = quadScratch->screenCorners[1].vy;
            quad->x2 = quadScratch->screenCorners[2].vx;
            quad->y2 = quadScratch->screenCorners[2].vy;
            quad->x3 = quadScratch->screenCorners[3].vx;
            quad->y3 = quadScratch->screenCorners[3].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->depth),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
    }
}

void effectSpriteTask53(Task* task)
{
    enum { EFFECT_PLAYER_GROUND_SHADOW_HALF_SIZE = 448 };
    VECTOR3   groundPoint;
    Task*     playerTask;
    GfxCoord* coord;
    GfxCoord* playerCoords;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    coord      = task->extra.coordBody->coord;
    // Wait for the player before borrowing its first model-part coordinate.
    if (playerTask != NULL) {
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            playerCoords        = playerTask->extra.tmd->coords;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->parent       = playerCoords + 1;
            actorRenderComposeCoord(coord);
            task->state = EFFECT_DRAW_TASK_ACTIVE;
        } else if (gRoomEffectState->groundShadowShade >= 0) {
            if (!(playerTask->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                actorRenderComposeCoord(coord);
                if ((s16)worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&coord->workm), &groundPoint) != 0) {
                    effectDrawGroundShadow(&groundPoint, EFFECT_PLAYER_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
                }
            }
        }
    }
}

// Embedded thrown-model sources start unresolved and use only directly transformed packets.
enum { EFFECT_MODEL_HANDLERS_UNRESOLVED = 0,
       EFFECT_MODEL_PRETRANSFORM_BYTES  = 0 };

/// Initial one-part identity transform for the bullet casing model.
static const TmdBone _gEffectBulletCasingModelRestPose[1] __attribute__((section(".data"))) = {
#include "assets/bullet_casing_skeleton.inc"
};
/// Vertex count of the bullet casing model's sole part.
static const u32 _gEffectBulletCasingModelPartVertexCounts[1] __attribute__((section(".data"))) = {
#include "assets/bullet_casing_partVerts.inc"
};
/// Part-local signed integer vertices of the bullet casing model.
static SVECTOR _gEffectBulletCasingModelVertices[6] = {
#include "assets/bullet_casing_verts.inc"
};
/// Q12 lighting normals of the bullet casing model.
static SVECTOR _gEffectBulletCasingModelNormals[4] = {
#include "assets/bullet_casing_normals.inc"
};
/// Bullet casing TMD command words, with writable draw-handler slots.
static u32 _gEffectBulletCasingModelPackets[32] = {
#include "assets/bullet_casing_stream.inc"
};
TmdSource gEffectBulletCasingModel = { 0, sizeof(POLY_GT3) + 3 * sizeof(POLY_GT4), 0, ARRAY_SIZE(_gEffectBulletCasingModelRestPose), _gEffectBulletCasingModelPartVertexCounts, _gEffectBulletCasingModelVertices, _gEffectBulletCasingModelNormals, _gEffectBulletCasingModelRestPose, _gEffectBulletCasingModelPackets };
/// Initial one-part identity transform for the rifle casing model.
static const TmdBone _gEffectRifleCasingModelRestPose[1] __attribute__((section(".data"))) = {
#include "assets/rifle_casing_skeleton.inc"
};
/// Vertex count of the rifle casing model's sole part.
static const u32 _gEffectRifleCasingModelPartVertexCounts[1] __attribute__((section(".data"))) = {
#include "assets/rifle_casing_partVerts.inc"
};
/// Part-local signed integer vertices of the rifle casing model.
static SVECTOR _gEffectRifleCasingModelVertices[6] = {
#include "assets/rifle_casing_verts.inc"
};
/// Q12 lighting normals of the rifle casing model.
static SVECTOR _gEffectRifleCasingModelNormals[4] = {
#include "assets/rifle_casing_normals.inc"
};
/// Rifle casing TMD command words, with writable draw-handler slots.
static u32 _gEffectRifleCasingModelPackets[32] = {
#include "assets/rifle_casing_stream.inc"
};
TmdSource gEffectRifleCasingModel = {
    .handlersResolved    = EFFECT_MODEL_HANDLERS_UNRESOLVED,
    .bufferHalfBytes     = sizeof(POLY_GT3) + 3 * sizeof(POLY_GT4),
    .preXformRegionBytes = EFFECT_MODEL_PRETRANSFORM_BYTES,
    .partCount           = ARRAY_SIZE(_gEffectRifleCasingModelRestPose),
    .partVertexCounts    = _gEffectRifleCasingModelPartVertexCounts,
    .verts               = _gEffectRifleCasingModelVertices,
    .normals             = _gEffectRifleCasingModelNormals,
    .skeleton            = _gEffectRifleCasingModelRestPose,
    .stream              = _gEffectRifleCasingModelPackets,
};
/// Initial one-part identity transform for the P229 casing model.
static const TmdBone _gEffectP229CasingModelRestPose[1] __attribute__((section(".data"))) = {
#include "assets/p229_casing_skeleton.inc"
};
/// Vertex count of the P229 casing model's sole part.
static const u32 _gEffectP229CasingModelPartVertexCounts[1] __attribute__((section(".data"))) = {
#include "assets/p229_casing_partVerts.inc"
};
/// Part-local signed integer vertices of the P229 casing model.
static SVECTOR _gEffectP229CasingModelVertices[6] = {
#include "assets/p229_casing_verts.inc"
};
/// Q12 lighting normals of the P229 casing model.
static SVECTOR _gEffectP229CasingModelNormals[4] = {
#include "assets/p229_casing_normals.inc"
};
/// P229 casing TMD command words, with writable draw-handler slots.
static u32 _gEffectP229CasingModelPackets[32] = {
#include "assets/p229_casing_stream.inc"
};
TmdSource gEffectP229CasingModel = {
    .handlersResolved    = EFFECT_MODEL_HANDLERS_UNRESOLVED,
    .bufferHalfBytes     = sizeof(POLY_GT3) + 3 * sizeof(POLY_GT4),
    .preXformRegionBytes = EFFECT_MODEL_PRETRANSFORM_BYTES,
    .partCount           = ARRAY_SIZE(_gEffectP229CasingModelRestPose),
    .partVertexCounts    = _gEffectP229CasingModelPartVertexCounts,
    .verts               = _gEffectP229CasingModelVertices,
    .normals             = _gEffectP229CasingModelNormals,
    .skeleton            = _gEffectP229CasingModelRestPose,
    .stream              = _gEffectP229CasingModelPackets,
};
/// Initial one-part identity transform for the shotgun shell casing model.
static const TmdBone _gEffectShotgunShellCasingModelRestPose[1] __attribute__((section(".data"))) = {
#include "assets/shotgun_shell_casing_skeleton.inc"
};
/// Vertex count of the shotgun shell casing model's sole part.
static const u32 _gEffectShotgunShellCasingModelPartVertexCounts[1] __attribute__((section(".data"))) = {
#include "assets/shotgun_shell_casing_partVerts.inc"
};
/// Part-local signed integer vertices of the shotgun shell casing model.
static SVECTOR _gEffectShotgunShellCasingModelVertices[6] = {
#include "assets/shotgun_shell_casing_verts.inc"
};
/// Q12 lighting normals of the shotgun shell casing model.
static SVECTOR _gEffectShotgunShellCasingModelNormals[4] = {
#include "assets/shotgun_shell_casing_normals.inc"
};
/// Shotgun shell casing TMD command words, with writable draw-handler slots.
static u32 _gEffectShotgunShellCasingModelPackets[32] = {
#include "assets/shotgun_shell_casing_stream.inc"
};
TmdSource gEffectShotgunShellCasingModel = {
    .handlersResolved    = EFFECT_MODEL_HANDLERS_UNRESOLVED,
    .bufferHalfBytes     = sizeof(POLY_GT3) + 3 * sizeof(POLY_GT4),
    .preXformRegionBytes = EFFECT_MODEL_PRETRANSFORM_BYTES,
    .partCount           = ARRAY_SIZE(_gEffectShotgunShellCasingModelRestPose),
    .partVertexCounts    = _gEffectShotgunShellCasingModelPartVertexCounts,
    .verts               = _gEffectShotgunShellCasingModelVertices,
    .normals             = _gEffectShotgunShellCasingModelNormals,
    .skeleton            = _gEffectShotgunShellCasingModelRestPose,
    .stream              = _gEffectShotgunShellCasingModelPackets,
};
/// Initial one-part identity transform for the slot-0x91 thrown model.
static const TmdBone _gEffectThrownModel91RestPose[1] __attribute__((section(".data"))) = {
#include "assets/thrown_model_91_skeleton.inc"
};
/// Vertex count of the slot-0x91 thrown model's sole part.
static const u32 _gEffectThrownModel91PartVertexCounts[1] __attribute__((section(".data"))) = {
#include "assets/thrown_model_91_partVerts.inc"
};
/// Part-local integer vertices of the slot-0x91 box; physical identity unproven.
static SVECTOR _gEffectThrownModel91Vertices[8] = {
#include "assets/thrown_model_91_verts.inc"
};
/// Q12 lighting normals of the slot-0x91 box, including its two cap normals.
static SVECTOR _gEffectThrownModel91Normals[10] = {
#include "assets/thrown_model_91_normals.inc"
};
/// Six-quad TMD command words for slot 0x91, with writable draw-handler slots.
static u32 _gEffectThrownModel91Packets[48] = {
#include "assets/thrown_model_91_stream.inc"
};
TmdSource gEffectThrownModel91 = {
    .handlersResolved    = EFFECT_MODEL_HANDLERS_UNRESOLVED,
    .bufferHalfBytes     = 6 * sizeof(POLY_GT4),
    .preXformRegionBytes = EFFECT_MODEL_PRETRANSFORM_BYTES,
    .partCount           = ARRAY_SIZE(_gEffectThrownModel91RestPose),
    .partVertexCounts    = _gEffectThrownModel91PartVertexCounts,
    .verts               = _gEffectThrownModel91Vertices,
    .normals             = _gEffectThrownModel91Normals,
    .skeleton            = _gEffectThrownModel91RestPose,
    .stream              = _gEffectThrownModel91Packets,
};
/// Parent-local muzzle placements for the weapon-flash profiles 0..33.
///
/// Controllers copy signed XYZ in game-coordinate units into their muzzle
/// coordinate before composition. The parent is the controller's borrowed
/// weapon/model coordinate; the fourth halfword is unused. The table stays
/// in initialized gameplay data and no controller modifies its entries.
static SVECTOR _gEffectWeaponMuzzleOffsets[34] = {
    { 0, 0, 0, 0 },
    { 0, 384, 96, 0 },
    { 0, 448, 96, 0 },
    { 0, 512, 96, 0 },
    { 0, 384, 96, 0 },
    { 0, 448, 128, 0 },
    { 0, 288, 128, 0 },
    { 0, 288, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 480, 128, 0 },
    { 0, 0, 0, 0 },
    { 0, 448, 128, 0 },
    { 0, 640, 128, 0 },
    { 0, 560, 96, 0 },
    { 0, 672, 96, 0 },
    { 0, 752, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 864, 144, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 32, 768, 0 },
    { 0, 0, 0, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 576, 128, 0 },
    { 0, 576, 128, 0 },
    { 0, 576, 128, 0 },
    { 0, 448, 128, 0 },
};
/// Weapon emission offsets indexed by weapon-effect profile 0..33.
///
/// Signed halfword XYZ use the spawning controller's local coordinate units.
/// Casing and rifle-flash spawns copy these into their placement while retaining
/// a borrowed pointer. Reload emitters instead add them to copied spawn XYZ in
/// the weapon parent's axes. The fourth halfword is zero and unused. Storage
/// remains valid with gameplay for all spawned tasks.
static const SVECTOR _gEffectWeaponEmissionOffsets[34] __attribute__((section(".data"))) = {
    { 0, 0, 0, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 320, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 256, 0 },
    { 0, 384, 0, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 448, 0 },
    { 0, 0, 576, 0 },
    { 0, 0, 480, 0 },
    { 0, 64, 576, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 480, 0 },
    { 0, 0, 480, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 448, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 480, 0 },
};
/// Grenade-flash muzzle placements indexed by weapon-flash profile 0..33.
///
/// Signed halfword XYZ use the weapon parent's local game-coordinate units;
/// the controller reparents its coordinate before applying these translations.
/// The fourth halfword is zero and unused. These are distinct from the ordinary
/// weapon muzzle placements, including the MM1 and M4A1 grenade profiles.
static const SVECTOR _gEffectGrenadeMuzzleOffsets[34] __attribute__((section(".data"))) = {
    { 0, 0, 0, 0 },
    { 0, 384, 96, 0 },
    { 0, 448, 96, 0 },
    { 0, 512, 96, 0 },
    { 0, 384, 96, 0 },
    { 0, 448, 128, 0 },
    { 0, 288, 128, 0 },
    { 0, 288, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 480, 128, 0 },
    { 0, 0, 0, 0 },
    { 0, 448, 128, 0 },
    { 0, 640, 128, 0 },
    { 0, 560, 96, 0 },
    { 0, 672, 96, 0 },
    { 0, 752, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 864, 144, 0 },
    { 0, 0, 0, 0 },
    { 0, 768, 0, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 576, 64, 0 },
    { 0, 576, 64, 0 },
    { 0, 576, 64, 0 },
    { 0, 640, 96, 0 },
};
/// Delayed grenade-flash emission offsets indexed by weapon-flash profile 0..33.
///
/// Signed halfword XYZ use the controller muzzle's local game-coordinate units,
/// after its parent-local muzzle translation has been installed. Child effects
/// copy placement and borrow the entry until teardown. The fourth halfword is
/// zero and unused. The effect-spawn interface takes writable SVECTOR storage.
static SVECTOR _gEffectGrenadeEmissionOffsets[34] = {
    { 0, 0, 0, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 320, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 256, 0 },
    { 0, 384, 0, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 448, 0 },
    { 0, 0, 576, 0 },
    { 0, 0, 512, 0 },
    { 0, 64, 576, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 384, 0 },
    { 0, 448, 128, 0 },
};
/// Colour, spikes and radius growth for critical-hit style selectors 0..5.
///
/// The selector is unchecked. Radius growth is per tick, with the outer screen
/// radius given by radius * 256 / depth pixels.
static const _EffectCriticalHitStyle _gEffectCriticalHitStyles[6] __attribute__((section(".data"))) = {
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 15, 7), 24 }, // 0 yellow, spiked
    { EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 7, 7), 32 },                                     // 1 light red, plain, faster growth
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 15, 15), 24 }, // 2 cyan, spiked
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 15, 7), 24 },  // 3 green, spiked
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 7, 15), 24 }, // 4 magenta, spiked
    { EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 7, 15), 24 },                                     // 5 blue, plain
};
/// Texture cells, pages and palettes of the four-tick additive impact flash.
///
/// One entry is drawn per age 0..3. Spans 55/31 cover inclusive 56/32-texel
/// square cells and also scale the billboard.
static const _EffectImpactFlashFrame _gEffectImpactFlashFrames[4] __attribute__((section(".data"))) = {
    { 55, 112, 200, 176, 266, 576 },
    { 31, 192, 56, 192, 266, 512 },
    { 55, 168, 200, 192, 266, 576 },
    { 31, 224, 56, 192, 266, 512 },
};
