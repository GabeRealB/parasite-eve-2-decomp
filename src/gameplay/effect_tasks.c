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

/// Flag in `_EffectCriticalHitStyle::color` that adds four radial spikes, each
/// at a random angle within its own quarter turn, to the ring.
#define EFFECT_CRITICAL_HIT_STYLE_SPIKES 0x1000

/// Packs the channel weights of `_EffectCriticalHitStyle::color`, each 0 to 15.
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

extern _EffectCriticalHitStyle D_8011291C[];

extern _EffectImpactFlashFrame Gp_EffSprRecs[];

extern u32 D_80111EF4[1];

extern SVECTOR D_80111EF8[6];

extern SVECTOR D_80111F28[4];

extern u32 D_80111F48[32];

extern u32 D_80112010[1];

extern SVECTOR D_80112014[6];

extern SVECTOR D_80112044[4];

extern u32 D_80112064[32];

extern u32 D_8011212C[1];

extern SVECTOR D_80112130[6];

extern SVECTOR D_80112160[4];

extern u32 D_80112180[32];

extern u32 D_80112248[1];

extern SVECTOR D_8011224C[6];

extern SVECTOR D_8011227C[4];

extern u32 D_8011229C[32];

extern u32 D_80112364[1];

extern SVECTOR D_80112368[8];

extern SVECTOR D_801123A8[10];

extern u32 D_801123F8[48];

extern SVECTOR D_801124DC[34];

extern SVECTOR D_801125EC[34];

extern SVECTOR D_801126FC[34];

extern SVECTOR D_8011280C[34];

static void _effectDrawMuzzleFlash(const GfxCoord* coord, s32 size, s32 angle);

static void _effectDrawImpactSparkFlash(const GfxCoord* coord, u16 frame, s16 size, s16 angle);

static void _effectDrawCriticalHitBurst(const GfxCoord* coord, s16 radius, s16 brightness, u16 color);

/// Places the four corners of one sixteenth-turn critical-hit ring segment.
///
/// segmentAngle is 0..3840 in 4096-unit turns; returns the angle plus 256.
/// scratch supplies the projected centre and signed integer pixel radii in
/// extent.burst. Corners 0/1 follow the outer radius and 2/3 the inner radius,
/// in GPU strip order. Q12 sine/cosine products shift arithmetically; each
/// resulting pixel coordinate narrows to the packet's signed halfword.
/// Borrows both pointers, writes only quad's XY fields and consumes no arena
/// or scratch-stack storage. Packet colours and its DMA tag are untouched.
static __inline__ s32 _effectSetCriticalHitRingCorners(POLY_G4* quad, const EffectShapeScratch* scratch, s32 segmentAngle)
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
static __inline__ void _effectProjectGroundShadow(EffectQuadScratch* quadScratch)
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

/// Chooses a sideways ejection direction using the spawning coordinate's local rotation.
///
/// Consumes three consecutive LCG values and writes work->move. The caller
/// normalizes it after selecting its launch profile; work->parent must be
/// live with the intended launch rotation in its local matrix.
static __inline__ void _effectChooseThrownModelDirection(EffectWork* work)
{
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vz   = (gRandomLcgState >> 16) & 0x7F;
    gte_SetRotMatrix(&work->parent->coord);
    gte_ldv0(&work->move);
    gte_rtv0();
    gte_stsv(&work->move);
}

/// Projects the bouncing spark's staged view position onto its screen centre.
///
/// scratch is a live block with XYZ initialized in signed halfword coordinate
/// units. Writes screenX/Y and projectionFlags even on rejection, leaving
/// depth untouched and SZ3 ready to read before another GTE transform.
static __inline__ void _effectProjectBouncingSpark(EffectShapeScratch* scratch)
{
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
}

/// Places the bouncing spark's two opposite corner pairs around its screen centre.
///
/// scratch has a positive biased depth and projected centre; work supplies
/// the size numerator and angle in 4096-unit turns. The half-diagonal uses
/// the 15-texel UV span.
/// Reuses scratch's corner words for each pair, retaining the unsigned
/// halfword narrowing before addition to the packet's signed pixel fields.
static __inline__ void _effectSetBouncingSparkCorners(POLY_FT4* quad, EffectShapeScratch* scratch, const EffectWork* work)
{
    enum {
        EFFECT_BOUNCING_SPARK_TRIG_FRACTION_BITS = 12,
    };

    scratch->extent.corner.x = (((work->scale * EFFECT_BOUNCING_SPARK_UV_SPAN) / scratch->depth) * rsin(work->angle)) >> EFFECT_BOUNCING_SPARK_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((work->scale * EFFECT_BOUNCING_SPARK_UV_SPAN) / scratch->depth) * rcos(work->angle)) >> EFFECT_BOUNCING_SPARK_TRIG_FRACTION_BITS;
    quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;
    quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;
    quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;
    quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;
    scratch->extent.corner.x = (((work->scale * EFFECT_BOUNCING_SPARK_UV_SPAN) / scratch->depth) * rsin(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_BOUNCING_SPARK_TRIG_FRACTION_BITS;
    scratch->extent.corner.y = (((work->scale * EFFECT_BOUNCING_SPARK_UV_SPAN) / scratch->depth) * rcos(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_BOUNCING_SPARK_TRIG_FRACTION_BITS;
    quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;
    quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;
    quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;
    quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;
}

EffectUnitQuadCorner D_80111E38[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};
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
u16 Gp_QuadClutX[6] = {
    32,
    48,
    192,
    208,
    224,
    240,
};
u16 D_80111EB4[6] = {
    80,
    176,
    256,
    272,
    288,
    304,
};
u16 Gp_FadeQuadColors[8] = {
    0x2CCC,
    4924,
    9155,
    9068,
    0x2C63,
    7267,
    9020,
    7222,
};

void Gp_EffCtlTask2B(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    s32                            temp;
    s32                            idx;
    s32                            t2;
    s32                            rng;
    s32                            count;

    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    lightSlot = gWorldCoordTransientPointLights;
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                                               = arg0->spawnArg1.halves.high;
                mem->index                                         = temp;
                arg0->spawnArg1.value                              = (u8)arg0->spawnArg1.value;
                slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
                slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
                t2                                                 = coord->coord.t[2];
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                slot->head.color.r                                 = 0xC00;
                slot->head.color.g                                 = 0xC00;
                slot->head.color.b                                 = 0xC00;
                slot->inner                                        = 0xFA0;
                slot->outer                                        = 0x12C0;
                slot->head.transform.coord.coord.t[2]              = t2;
                coord->parent                                      = mem->parent;
                coord->coord.t[0]                                  = D_801124DC[arg0->spawnArg1.value].vx;
                coord->coord.t[1]                                  = D_801124DC[arg0->spawnArg1.value].vy;
                coord->coord.t[2]                                  = D_801124DC[arg0->spawnArg1.value].vz;
                coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                switch (arg0->spawnArg1.value) {
                    case 1:
                    default:
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rng;
                        Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, (((u32)rng >> 16) & 0x1FF) | 0x200, 0);
                        idx         = arg0->spawnArg1.value;
                        arg0->state = 1;
                        Gp_SpawnEff(EFFECT_BULLET_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 4;
                        lightSlot->framesLeft = 4;
                        break;
                    case 2:
                    case 3:
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rng;
                        Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx         = arg0->spawnArg1.value;
                        arg0->state = 1;
                        Gp_SpawnEff(EFFECT_BULLET_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 4;
                        lightSlot->framesLeft = 4;
                        break;
                    case 30:
                    case 31:
                    case 32:
                        arg0->state     = 2;
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rng;
                        Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx = arg0->spawnArg1.value;
                        Gp_SpawnEff(EFFECT_BULLET_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 2;
                        lightSlot->framesLeft = 2;
                        break;
                    case 5:
                        idx         = arg0->spawnArg1.value;
                        arg0->state = 1;
                        Gp_SpawnEff(EFFECT_P229_SHELL_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 4;
                        lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
                        break;
                    case 33:
                        mem->index      = 1;
                        arg0->state     = 1;
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rng;
                        Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx = arg0->spawnArg1.value;
                        Gp_SpawnEff(EFFECT_P229_SHELL_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 4;
                        lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
                        break;
                }
                if (mem->index == 0) {
                    gRoomEffectState->burstRequest = true;
                }
                break;
            case 1:
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) | 0x200, 0);
                arg0->state++;
                break;
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6A(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    s32                            t2;

    lightSlot = gWorldCoordTransientPointLights;
    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
                slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
                t2                                                 = coord->coord.t[2];
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                slot->head.color.b                                 = 0xC00;
                slot->head.color.g                                 = 0xC00;
                slot->head.color.r                                 = 0xC00;
                slot->inner                                        = 0xFA0;
                slot->outer                                        = 0x12C0;
                gWorldCoordTransientPointLights->framesLeft        = 4;
                slot->head.transform.coord.coord.t[2]              = t2;
                coord->parent                                      = mem->parent;
                coord->coord.t[0]                                  = D_801124DC[arg0->spawnArg1.value].vx;
                coord->coord.t[1]                                  = D_801124DC[arg0->spawnArg1.value].vy;
                coord->coord.t[2]                                  = D_801124DC[arg0->spawnArg1.value].vz;
                coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
                mem->move.vx    = 0;
                mem->move.vy    = 0;
                mem->move.vz    = -(mem->scale >> 1);
                Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x600, &mem->move);
                arg0->state                    = 1;
                gRoomEffectState->burstRequest = true;
                break;
            case 1:
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x11280,
                            &mem->move);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x21280,
                            &mem->move);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x31280,
                            &mem->move);
                arg0->state++;
                break;
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        if (mem->age >= 5) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6B(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    RoomEffectState*               effectState;
    s32                            temp;
    s32                            idx;
    s32                            t2;
    s32                            count;

    lightSlot   = gWorldCoordTransientPointLights;
    slot        = &lightSlot->light;
    mem         = arg0->spawnArg2.pointer;
    coord       = arg0->extra.coordBody->coord;
    effectState = gRoomEffectState;
    if (effectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        if (arg0->state == 0) {
            temp                                               = arg0->spawnArg1.halves.high;
            mem->index                                         = temp;
            arg0->spawnArg1.value                              = (u8)arg0->spawnArg1.value;
            slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
            slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
            t2                                                 = coord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            slot->head.color.r                                 = 0xC00;
            slot->head.color.g                                 = 0xC00;
            slot->head.color.b                                 = 0xC00;
            slot->inner                                        = 0xFA0;
            slot->outer                                        = 0x12C0;
            slot->head.transform.coord.coord.t[2]              = t2;
            coord->parent                                      = mem->parent;
            coord->coord.t[0]                                  = D_801124DC[arg0->spawnArg1.value].vx;
            coord->coord.t[1]                                  = D_801124DC[arg0->spawnArg1.value].vy;
            coord->coord.t[2]                                  = D_801124DC[arg0->spawnArg1.value].vz;
            coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
            mem->move.vx    = 0;
            mem->move.vy    = 0;
            mem->move.vz    = -(mem->scale >> 1);
            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x380, &mem->move);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
            idx         = arg0->spawnArg1.value;
            arg0->state = 1;
            Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH_MODEL, coord, idx, &D_801125EC[idx]);
            if (arg0->spawnArg1.value == 0x11) {
                mem->scale                                  = 1;
                gWorldCoordTransientPointLights->framesLeft = 1;
            } else {
                mem->scale                                  = 4;
                gWorldCoordTransientPointLights->framesLeft = 4;
            }
            if (mem->index == 0) {
                gRoomEffectState->burstRequest = true;
            }
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            effectKillTask(mem, arg0);
        }
    }
}

void func_800ED42C(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    SVECTOR*                       vec;
    s32                            temp;
    s32                            t2;
    s32                            count;
    s32                            i;

    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    lightSlot = gWorldCoordTransientPointLights;
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                                               = arg0->spawnArg1.halves.high;
                mem->index                                         = temp;
                arg0->spawnArg1.value                              = (u8)arg0->spawnArg1.value;
                slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
                slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
                t2                                                 = coord->coord.t[2];
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                slot->head.color.r                                 = 0xE00;
                slot->head.color.g                                 = 0xA00;
                slot->head.color.b                                 = 0xA00;
                slot->inner                                        = 0xFA0;
                slot->outer                                        = 0x12C0;
                slot->head.transform.coord.coord.t[2]              = t2;
                coord->parent                                      = mem->parent;
                coord->coord.t[0]                                  = D_801124DC[arg0->spawnArg1.value].vx;
                coord->coord.t[1]                                  = D_801124DC[arg0->spawnArg1.value].vy;
                coord->coord.t[2]                                  = D_801124DC[arg0->spawnArg1.value].vz;
                coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                switch (arg0->spawnArg1.value) {
                    case 1:
                    default:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 18);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x200, &mem->move);
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_SHOTGUN_SPARK_LINE, coord, 0, 0);
                            }
                            mem->scale = 0x18;
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 17);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x380, &mem->move);
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                _effectDrawMuzzleFlash(coord, (s16)(mem->scale + 0x280),
                                                       (gRandomLcgState >> 16) & 0xFFF);
                            }
                            mem->scale = 4;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_SPARK_THROWN, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        arg0->state = 1;
                        break;
                    case 15:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 18);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x200, &mem->move);
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_SHOTGUN_SPARK_LINE, coord, 0, 0);
                            }
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 17);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x380, &mem->move);
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                _effectDrawMuzzleFlash(coord, (s16)(mem->scale + 0x280),
                                                       (gRandomLcgState >> 16) & 0xFFF);
                            }
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_SPARK_THROWN, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        Gp_SpawnEff(EFFECT_SHOTGUN_SHELL_CASING, coord, arg0->spawnArg1.value, &D_801125EC[arg0->spawnArg1.value]);
                        arg0->state = 2;
                        mem->scale  = 4;
                        break;
                    case 23:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = (s32)((u16)mem->scale << 16) >> 18;
                            gte_SetTransMatrix(&GsWSMATRIX);
                            gte_SetRotMatrix(&coord->coord);
                            vec = &mem->move;
                            gte_ldv0(vec);
                            gte_rtv0();
                            gte_stsv(vec);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x200, vec);
                            i = 0;
                            gfxRotMatrixX(&coord->coord, 0x400, i);
                            coord->composeStamp = GRAPHICS_COORD_DIRTY;
                            for (; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_SHOTGUN_SPARK_LINE, coord, 0, 0);
                            }
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = (s32)((u16)mem->scale << 16) >> 17;
                            gte_SetTransMatrix(&GsWSMATRIX);
                            gte_SetRotMatrix(&coord->coord);
                            vec = &mem->move;
                            gte_ldv0(vec);
                            gte_rtv0();
                            gte_stsv(vec);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x380, vec);
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                _effectDrawMuzzleFlash(coord, (s16)(mem->scale + 0x280),
                                                       (gRandomLcgState >> 16) & 0xFFF);
                            }
                            gfxRotMatrixX(&coord->coord, 0x400, GRAPHICS_ROTATION_COMPOSE);
                            coord->composeStamp = GRAPHICS_COORD_DIRTY;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_SPARK_THROWN, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        Gp_SpawnEff(EFFECT_SHOTGUN_SHELL_CASING, mem->parent, arg0->spawnArg1.value, &D_801125EC[arg0->spawnArg1.value]);
                        arg0->state = 2;
                        mem->scale  = 4;
                }
                lightSlot->framesLeft          = 4;
                gRoomEffectState->burstRequest = true;
                break;
            case 1:
                if (mem->age == mem->scale) {
                    Gp_SpawnEff(EFFECT_SHOTGUN_SHELL_CASING, coord, arg0->spawnArg1.value, &D_801125EC[arg0->spawnArg1.value]);
                    arg0->state = 2;
                }
                break;
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6C(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    s32                            temp;
    s32                            idx;
    s32                            t2;
    s32                            rng;
    s32                            rng2;
    s32                            count;
    s32                            i;

    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    lightSlot = gWorldCoordTransientPointLights;
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                                               = arg0->spawnArg1.halves.high;
                mem->index                                         = temp;
                arg0->spawnArg1.value                              = (u8)arg0->spawnArg1.value;
                slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
                slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
                t2                                                 = coord->coord.t[2];
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                slot->head.color.r                                 = 0xC00;
                slot->head.color.g                                 = 0xC00;
                slot->head.color.b                                 = 0xC00;
                slot->inner                                        = 0xFA0;
                slot->outer                                        = 0x12C0;
                slot->head.transform.coord.coord.t[2]              = t2;
                coord->parent                                      = mem->parent;
                coord->coord.t[0]                                  = D_801126FC[arg0->spawnArg1.value].vx;
                coord->coord.t[1]                                  = D_801126FC[arg0->spawnArg1.value].vy;
                coord->coord.t[2]                                  = D_801126FC[arg0->spawnArg1.value].vz;
                coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                idx             = ((u32)rng >> 16) & 0x1FF;
                rng2            = rng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                mem->scale      = idx;
                gRandomLcgState = rng2;
                _effectDrawMuzzleFlash(coord, idx | 0x400, ((u32)rng2 >> 16) & 0xFFF);
                for (i = 0; i < 4; i++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK_THROWN, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x21380, 0);
                }
                switch (arg0->spawnArg1.value) {
                    case 1:
                    default:
                        arg0->state = 1;
                        mem->scale  = 0x10;
                        break;
                    case 12:
                        arg0->state = 2;
                        mem->scale  = 4;
                        break;
                    case 27:
                        mem->angle  = 0xA;
                        arg0->state = 1;
                        mem->scale  = 0x18;
                        break;
                }
                lightSlot->framesLeft = 4;
                if (mem->index == 0) {
                    gRoomEffectState->burstRequest = true;
                }
                break;
            case 1:
                if (mem->age == mem->scale) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0xFF) + 0x12180,
                                &D_8011280C[arg0->spawnArg1.value]);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0xFF) + 0x22180,
                                &D_8011280C[arg0->spawnArg1.value]);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0xFF) + 0x32180,
                                &D_8011280C[arg0->spawnArg1.value]);
                    Gp_SpawnEff(EFFECT_091, coord, arg0->spawnArg1.value + mem->angle,
                                &D_8011280C[arg0->spawnArg1.value]);
                    arg0->state = 2;
                }
                break;
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            effectKillTask(mem, arg0);
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
            randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
            randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = (gRandomLcgState >> 16) % 3 + 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ((gRandomLcgState >> 16) & 0x1FF) + 0x200;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            task->state     = EFFECT_DRAW_TASK_ACTIVE;
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
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
            randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
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
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            work->angle     = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 1) + 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = (gRandomLcgState >> 14) & 0x7C;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
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
                work->angle     = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
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
                work->scale     = 0x114;
                work->angle     = EFFECT_THROWN_MODEL_MIN_BLINK_AGE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = 0xFF80 - ((gRandomLcgState >> 16) & 0x3F);
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
                work->scale     = 0xBF;
                work->angle     = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0xFFA0 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = -((gRandomLcgState >> 16) & 0x7F);
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
                work->scale     = 0x60;
                work->angle     = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = (((gRandomLcgState >> 16) & 0x1F) + 0x10);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_MM1:
                work->scale     = 0x80;
                work->angle     = EFFECT_THROWN_MODEL_MEDIUM_BLINK_AGE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = 0xFFC0 - ((gRandomLcgState >> 16) & 0x3F);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
                break;
            case EFFECT_THROWN_MODEL_PROFILE_M4A1_GRENADE_EJECTION:
                work->scale     = 0x60;
                work->angle     = EFFECT_THROWN_MODEL_LONG_BLINK_AGE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = 0xFFF0 - ((gRandomLcgState >> 16) & 0x1F);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
                break;
        }
        VectorNormalSS(&work->move, &work->move);
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->pos.vx        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->pos.vy        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->pos.vz        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
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
            goto release;
        }
    }
    return;
release:
    effectKillTask(work, task);
}

void Gp_EffCtlTask6E(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         rng;

    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value & 0xF0000) {
            mem->scale = (u32)arg0->spawnArg1.value >> 16;
        } else {
            mem->scale = 0xC;
        }
        arg0->spawnArg1.value = (u16)arg0->spawnArg1.value;
        coord->parent         = mem->parent;
        coord->coord.t[0]     = D_801125EC[arg0->spawnArg1.value].vx;
        coord->coord.t[1]     = D_801125EC[arg0->spawnArg1.value].vy;
        coord->coord.t[2]     = D_801125EC[arg0->spawnArg1.value].vz;
        coord->coord.t[0]    += mem->pos.vx;
        coord->coord.t[1]    += mem->pos.vy;
        coord->coord.t[2]    += mem->pos.vz;
        coord->composeStamp   = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        arg0->state     = 1;
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) | 0x11200, 0);
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) | 0x21200, 0);
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) | 0x31200, 0);
    }
    Gp_SpawnEff(EFFECT_091, coord, arg0->spawnArg1.value, 0);
    mem->age++;
    if (mem->age > mem->scale - 1) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffCtlTask6D(Task* arg0)
{
    GfxCoord* coord;
    MATRIX*   m;
    void*     mem;
    s32       i;
    s32       one;

    i                    = 0;
    one                  = ONE;
    coord                = arg0->extra.coordBody->coord;
    mem                  = arg0->spawnArg2.pointer;
    m                    = &coord->coord;
    *(s32*)&coord->coord = one;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = one;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = one;
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    for (; i < 6; i++) {
        Gp_SpawnEff(EFFECT_BULLET_CASING, coord, 9, 0);
    }

    effectKillTask(mem, arg0);
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
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ((gRandomLcgState >> 16) & 0xFF) + 0x100;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x40 - ((gRandomLcgState >> 16) & 0x7F);

            gte_SetRotMatrix(&work->parent->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = (gRandomLcgState >> 16) % 3 + 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = ((gRandomLcgState >> 16) & 1) + 1;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                tile);
        gpuSetPrimitiveBlendMode(tile, GPU_BLEND_ADD, scratch->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_EffectPixelSparkScratch);
    work->age++;
    if (work->age >= EFFECT_PIXEL_SPARK_TICKS) {
        effectKillTask(work, task);
    }
}

void Gp_EffCtlTask3B(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s32         i;
    s32         rng;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = ((u32)rng >> 16) & 0xFFF;
            gRandomLcgState = rng;
            if (arg0->spawnArg1.value & 0xFFF) {
                mem->angle = arg0->spawnArg1.halves.low & 0xFFF;
            } else {
                mem->angle = 0x200;
            }
            for (i = 0; i < 6; i++) {
                Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 1, 0);
            }
            arg0->state = 1;
        }
        _effectDrawImpactSparkFlash(coord, mem->age, mem->angle, mem->scale);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age < 4) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void Gp_EffSprTask5C(Task* arg0)
{
    EffectShapeScratch*             head;
    EffectShapeScratch*             block;
    EffectShapeScratch*             projectionScratch;
    GfxCoord*                       coord;
    EffectWork*                     mem;
    POLY_FT4*                       prim;
    const EffectSpriteTextureFrame* textureFrame;
    s16                             flag;
    s16                             scale;
    s16                             step;
    s32                             rng;
    s32                             i;
    s32                             n;
    s32                             t2;
    s32                             tmp;
    u16                             vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            scale = 0x200;
            if (arg0->spawnArg1.value & 0xFFF) {
                scale = arg0->spawnArg1.halves.low & 0xFFF;
            }
            mem->scale      = scale;
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->angle      = ((u32)rng >> 16) & 0xFFF;
            gRandomLcgState = rng;
            if (arg0->spawnArg1.value & 0xF000) {
                step = (arg0->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 2;
            }
            mem->period = step;
            mem->step   = (s32)((u16)mem->scale << 16) >> 23;
            tmp         = arg0->spawnArg1.signedBytes[3];
            mem->index  = tmp & 0xF;
            if (mem->index != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gte_lddp(mem->scale << 3);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
                gte_lddp(mem->index << 12);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            } else if (!(arg0->spawnArg1.value & 0xF0000000)) {
                n = gDisplayState.animFrame & 3;
                i = 0;
                if (n != 0) {
                    do {
                        Gp_SpawnEff(EFFECT_EXPLOSION, coord, ((s32)((u16)mem->scale << 16) >> 17) | 0x02001000, 0);
                        i += 1;
                    } while (i < n);
                }
                n = gDisplayState.animFrame % 3;
                i = 0;
                if (n > 0) {
                    do {
                        Gp_SpawnEff(EFFECT_EXPLOSION, coord, ((s32)((u16)mem->scale << 16) >> 17) | 0x01002000, 0);
                        i += 1;
                    } while (i < n);
                }
            }
            arg0->state = 1;
        }
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
        block                                    = head - 1;
        block->worldPoint.vy                     = (u16)coord->workm.t[1];
        vz                                       = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        block->worldPoint.vz                     = vz;
        projectionScratch                        = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            block->depth   = block->depth + 1;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            textureFrame = &gEffectSpriteAtlasFrames[mem->age / mem->period];
            prim->code  |= 3;
            prim->tpage  = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
            prim->clut   = getClut(textureFrame->clutX, textureFrame->clutY);
            prim->u0     = textureFrame->u;
            prim->v0     = textureFrame->v;
            prim->u1     = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
            prim->v1     = textureFrame->v;
            prim->u2     = textureFrame->u;
            prim->v2     = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
            prim->u3     = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
            prim->v3     = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
            // Reuse the inclusive texel span to size the billboard's half-diagonal.
            block->extent.corner.x = (((mem->scale * EFFECT_SPRITE_ATLAS_UV_SPAN) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * EFFECT_SPRITE_ATLAS_UV_SPAN) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * EFFECT_SPRITE_ATLAS_UV_SPAN) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * EFFECT_SPRITE_ATLAS_UV_SPAN) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        t2                  = coord->coord.t[2] + mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = t2;
        mem->scale         += mem->step;
        mem->age++;
        if (mem->age <= mem->period * ARRAY_SIZE(gEffectSpriteAtlasFrames) - 1) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void func_800F289C(Task* arg0)
{
    EffectShapeScratch* block;
    EffectWork*         mem;
    GfxCoord*           coord;
    POLY_FT4*           prim;
    s16                 flag;
    s16                 scale;
    s16                 mode;
    s32                 tmp;
    s32                 i;
    s32                 n;
    s32                 mask;
    s32                 step;
    s32                 step2;
    s32                 mask2;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            scale = 0x200;
            if (arg0->spawnArg1.value & 0xFFF) {
                scale = arg0->spawnArg1.halves.low & 0xFFF;
            }
            mem->scale      = scale;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
            if (arg0->spawnArg1.value & 0xF000) {
                mem->period = (arg0->spawnArg1.value >> 12) & 0xF;
            } else {
                mem->period = 1;
            }
            if (arg0->spawnArg1.value & 0xFF0000) {
                mem->step = (arg0->spawnArg1.value >> 16) & 0xFF;
            } else {
                mem->step = mem->scale >> 8;
            }
            tmp        = arg0->spawnArg1.signedBytes[3];
            mode       = tmp & 0xF;
            mem->index = mode;
            if (mode != 0) {
                if (mode == 1) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 8 - ((gRandomLcgState >> 16) & 0xF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = ((gRandomLcgState >> 16) & 0xF) * 3;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 8 - ((gRandomLcgState >> 16) & 0xF);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                    gte_lddp((mem->scale << 3));
                    gte_ldsv(&mem->move);
                    gte_gpf12();
                    gte_stsv(&mem->move);
                    gte_lddp((mem->index << 11));
                    gte_ldsv(&mem->move);
                    gte_gpf12();
                    gte_stsv(&mem->move);
                }
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            } else {
                switch (mem->step) {
                    case 1:
                        mem->move.vx    = 0;
                        mem->move.vz    = 0;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = -((gRandomLcgState >> 16) & 0xF) - 0x20;
                        break;
                    case 2:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = -((gRandomLcgState >> 0x10) & 0x1F) - 0x10;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vz    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        break;
                    case 3:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vz    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        gte_lddp((mem->scale << 2));
                        gte_ldsv(&mem->move);
                        gte_gpf12();
                        gte_stsv(&mem->move);
                        break;
                }
            }
            if (arg0->spawnArg1.value & 0x30000000) {
                n = gDisplayState.animFrame & 3;
                for (i = 0; i < n; i++) {
                    step = mem->scale - (mem->scale >> 2);
                    mask = (arg0->spawnArg1.value & 0xC0000000) | 0x6002000;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, step | mask, 0);
                }
                n = gDisplayState.animFrame % 3;
                for (i = 0; i < n; i++) {
                    step2 = mem->scale - (mem->scale >> 2);
                    mask2 = (arg0->spawnArg1.value & 0xC0000000) | 0x4003000;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, step2 | mask2, 0);
                }
            }
            arg0->state = 1;
        }
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
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            if (arg0->spawnArg1.value & 0xC0000000) {
                prim->tpage = ((((u32)arg0->spawnArg1.value >> 30) - 1) & 3) << 5 | 8;
            } else {
                prim->tpage = 0x28;
            }
            prim->clut             = 0x4253;
            prim->u0               = (mem->age / mem->period) << 5;
            prim->v0               = 0x18;
            prim->u1               = ((mem->age / mem->period) << 5) + 0x1F;
            prim->v1               = 0x18;
            prim->u2               = (mem->age / mem->period) << 5;
            prim->v2               = 0x37;
            prim->u3               = ((mem->age / mem->period) << 5) + 0x1F;
            prim->v3               = 0x37;
            block->extent.corner.x = (((mem->scale * 0x1F) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x1F) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + block->extent.corner.x;
            prim->x3               = block->screenX - block->extent.corner.x;
            prim->y0               = block->screenY - block->extent.corner.y;
            prim->y3               = block->screenY + block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * 0x1F) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x1F) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + block->extent.corner.x;
            prim->x2               = block->screenX - block->extent.corner.x;
            prim->y1               = block->screenY - block->extent.corner.y;
            prim->y2               = block->screenY + block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        coord->coord.t[2]  += mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->age++;
        if (mem->age <= mem->period * 8 - 1) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void Gp_EffSprTask76(Task* arg0)
{
    EffectShapeScratch* block;
    GfxCoord*           coord;
    EffectWork*         mem;
    POLY_FT4*           prim;
    u16                 uvSpan;
    s16                 scale;
    s32                 rng;

    coord = arg0->extra.coordBody->coord;
    block = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    mem   = arg0->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
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
        block->depth   = block->depth + 1;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        uvSpan = Gp_EffSprRecs[mem->age].uvSpan;
        if (arg0->state == 0) {
            if (arg0->spawnArg1.value & 0xFFF) {
                scale = arg0->spawnArg1.halves.low & 0xFFF;
            } else {
                scale = 0x200;
            }
            mem->scale      = scale;
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng;
            mem->angle      = ((u32)rng >> 16) & 0xFFF;
            arg0->state     = 1;
        }
        prim->code            |= 3;
        prim->tpage            = getTPage(0, 1, Gp_EffSprRecs[mem->age].tpageX, 0);
        prim->clut             = getClut(Gp_EffSprRecs[mem->age].clutX, Gp_EffSprRecs[mem->age].clutY);
        prim->u0               = Gp_EffSprRecs[mem->age].u;
        prim->v0               = Gp_EffSprRecs[mem->age].v;
        prim->u1               = Gp_EffSprRecs[mem->age].u + uvSpan;
        prim->v1               = Gp_EffSprRecs[mem->age].v;
        prim->u2               = Gp_EffSprRecs[mem->age].u;
        prim->v2               = Gp_EffSprRecs[mem->age].v + uvSpan;
        prim->u3               = Gp_EffSprRecs[mem->age].u + uvSpan;
        prim->v3               = Gp_EffSprRecs[mem->age].v + uvSpan;
        block->extent.corner.x = ((((s16)uvSpan * mem->scale) / block->depth) * rsin(mem->angle)) >> 12;
        block->extent.corner.y = ((((s16)uvSpan * mem->scale) / block->depth) * rcos(mem->angle)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        block->extent.corner.x = ((((s16)uvSpan * mem->scale) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
        block->extent.corner.y = ((((s16)uvSpan * mem->scale) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    mem->age++;
    if (mem->age >= 4) {
        effectKillTask(mem, arg0);
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
    s32                 shadeByteSource;
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
        randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->angle     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
        gRandomLcgState = randomState;
        if (task->spawnArg1.value & EFFECT_BOUNCING_SPARK_PERIOD_MASK) {
            ticksPerFrame = (task->spawnArg1.value >> EFFECT_BOUNCING_SPARK_PERIOD_SHIFT) & EFFECT_BOUNCING_SPARK_PERIOD_MAX;
        } else {
            ticksPerFrame = 1;
        }
        work->period    = ticksPerFrame;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->step      = 0x100 - ((gRandomLcgState >> 16) & 0x1F0);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vx   = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vz   = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
        work->index++;
    }
    actorRenderComposeCoord(coord);
    // Project the current position before moving the spark for its next tick.
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    _effectProjectBouncingSpark(block);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth   = block->depth + 1;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_ADDITIVE_TEXTURED_QUAD & ~2);
        if (work->age >= EFFECT_BOUNCING_SPARK_FADE_AGE) {
            shade = (EFFECT_BOUNCING_SPARK_TICKS - work->age) * EFFECT_BOUNCING_SPARK_CELL_SIZE;
            // Keep a separate word copy for the glow's byte conversion.
            __asm__ volatile("" : "=r"(shadeByteSource) : "0"(shade));
            glowBrightness = (u8)shadeByteSource;
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
        _effectSetBouncingSparkCorners(quad, block, work);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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

void func_800F4308(Task* arg0)
{
    u8                             rgb[3];
    EffectWork*                    mem;
    GfxCoord*                      coord;
    GfxCoord*                      lightCoord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    ModelObjectCoordBody*          body;
    SVECTOR*                       vec;
    s16                            flag;
    s32                            scale11;
    s32                            scale12;
    s32                            count;
    s32                            cond;
    s32                            condInc;
    s32                            i;
    s32                            t2_10;
    s32                            t2_11;
    s32                            t2_12;
    s32                            rng;
    s32                            tmp;

    lightSlot  = &gWorldCoordTransientPointLights[1];
    slot       = &lightSlot->light;
    lightCoord = &slot->head.transform.coord;
    body       = arg0->extra.coordBody;
    mem        = arg0->spawnArg2.pointer;
    flag       = gRoomEffectState->effectControl;
    coord      = body->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        cond = flag < ROOM_EFFECT_CONTROL_CANCEL_MIN;
        goto release;
    }
    actorRenderComposeCoord(coord);
    mem->age = mem->age + 1;
    switch (arg0->spawnArg1.value) {
        case 10:
            switch (arg0->state) {
                case 0:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    vec             = &mem->move;
                    Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x600, vec);
                    rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_BOUNCING_SPARK, coord, (((u32)rng >> 16) & 0x3F) | 0x100, vec);
                    rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_BOUNCING_SPARK, coord, (((u32)rng >> 16) & 0x3F) | 0x100, vec);
                    arg0->state++;
                    break;
                case 1:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0x1FF) | 0xD0000400,
                                &mem->move);
                    if (mem->age >= 7) {
                        arg0->state++;
                    }
                    break;
                case 2:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0xFF) | 0x82003400,
                                &mem->move);
                    if (mem->age >= 0xB) {
                        arg0->state++;
                    }
                    break;
            }
            lightSlot->framesLeft    = 0x10;
            count                    = mem->age;
            slot->outer              = 0x2580;
            slot->head.color.r       = 0x1000;
            slot->head.color.g       = 0xC00;
            slot->head.color.b       = 0x800;
            slot->inner              = (0x898 - (count * 0x64)) * 4;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            t2_10                    = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            lightCoord->coord.t[2]   = t2_10;
            cond                     = mem->age < 0x15;
            goto release;
        case 11:
            switch (arg0->state) {
                case 0:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    Gp_SpawnEff(EFFECT_IMPACT_FLASH, coord, 0x500, &mem->move);
                    condInc = mem->age < 2;
                    goto maybe11;
                case 1:
                    i = 0;
                    do {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0x1FF) | 0x82004400,
                                    &mem->move);
                        i += 1;
                    } while (i < 2);
                    condInc = mem->age < 9;
                    goto maybe11;
                case 2:
                    i = 0;
                    do {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0xFF) | 0xD0000400,
                                    &mem->move);
                        i += 1;
                    } while (i < 2);
                    condInc = mem->age < 0xD;
                maybe11:
                    if (condInc != 0) {
                        break;
                    }
                    arg0->state += 1;
                    break;
            }
            if (mem->scale++ < 8) {
                tmp     = -0x80 - (mem->scale << 4);
                rgb[0]  = tmp;
                rgb[1]  = (rgb[0] * 3) >> 2;
                rgb[2]  = (rgb[0] * 2) / 3;
                scale11 = (mem->scale << 6) + 0x40;
                effectDrawOuterGlowBand(coord, (s16)scale11, (s16)scale11, rgb);
            }
            if (mem->age < 4) {
                i = 0;
                do {
                    Gp_SpawnEff(EFFECT_SPARK_STREAK, coord, 0, 0);
                    i += 1;
                } while (i < 3);
            }
            lightSlot->framesLeft    = 0x10;
            count                    = mem->age;
            slot->outer              = 0x2580;
            slot->head.color.r       = 0xC00;
            slot->head.color.g       = 0xC00;
            slot->head.color.b       = 0x800;
            slot->inner              = (0x898 - (count * 0x64)) * 4;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            t2_11                    = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            lightCoord->coord.t[2]   = t2_11;
            cond                     = mem->age < 0x15;
            goto release;
        case 12:
            if (arg0->state != 0) {
                if (arg0->state == 1) {
                    goto case12_1;
                }
                goto skip12;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            Gp_SpawnEff(EFFECT_IMPACT_FLASH, coord, 0x500, &mem->move);
            condInc = mem->age < 2;
            goto maybe12;
        case12_1:
            i = 0;
            do {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0x1FF) | 0x82004400,
                            &mem->move);
                i += 1;
            } while (i < 2);
            condInc = mem->age < 9;
        maybe12:
            if (condInc == 0) {
                arg0->state += 1;
            }
        skip12:
            if (mem->scale++ < 8) {
                rgb[2]  = (-0x80 - (mem->scale << 4)) * 2;
                rgb[1]  = (rgb[2] & 0xE0) >> 2;
                rgb[0]  = rgb[1];
                scale12 = (mem->scale << 6) + 0x40;
                effectDrawOuterGlowBand(coord, (s16)scale12, (s16)scale12, rgb);
                if (gDisplayState.animFrame & 1) {
                    rgb[2] = ~(mem->scale * 0x1F);
                    rgb[1] = rgb[2] >> 2;
                    rgb[0] = rgb[1];
                    effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                }
            }
            lightSlot->framesLeft    = 0x10;
            count                    = mem->age;
            slot->outer              = 0x2580;
            slot->head.color.r       = 0x800;
            slot->head.color.g       = 0xC00;
            slot->head.color.b       = 0x1000;
            slot->inner              = (0x898 - (count * 0x64)) * 4;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            t2_12                    = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            lightCoord->coord.t[2]   = t2_12;
            cond                     = mem->age < 0x15;
            goto release;
        default:
            return;
    }
release:
    if (cond == 0) {
        effectKillTask(mem, arg0);
    }
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
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        fadeRate        = 2;
        if (((gRandomLcgState >> 16) & 3) != 0) {
            fadeRate = 1;
        }
        randomState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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

void Gp_EffPolyTask9C(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
            if (arg0->state == 0) {
                mem->scale  = 0x10;
                mem->angle  = 0x20;
                mem->period = D_8011291C[arg0->spawnArg1.value].color;
                mem->step   = D_8011291C[arg0->spawnArg1.value].radiusStep;
                arg0->state++;
            }
            actorRenderComposeCoord(coord);
            mem->scale -= 2;
            mem->angle += mem->step;
            _effectDrawCriticalHitBurst(coord, mem->angle, mem->scale, mem->period);
            mem->age++;
            if (mem->age < 8) {
                return;
            }
        } else {
            return;
        }
    }
    effectKillTask(mem, arg0);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
                randomState = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tri                           = gGpuPrimCursor;
                gGpuPrimCursor                = tri + 1;
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
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK, GRAPHICS_ROTATION_REPLACE);
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
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * (u16)work->scale;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * (u16)work->scale;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
    work->age++;
    if (work->angle < work->age) {
        effectKillTask(work, task);
    }
}

void Gp_EffSprTask54(Task* arg0)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    s16                 count;
    s16                 step;
    u16                 vz;
    EffectWork*         mem;
    GfxCoord*           coord;
    POLY_FT4*           prim;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }

    actorRenderComposeCoord(coord);
    if (arg0->state == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->scale      = arg0->spawnArg1.halves.low & 0xFFF;
        mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
        if (arg0->spawnArg1.value & 0xF000) {
            count = (arg0->spawnArg1.value >> 12) & 0xF;
        } else {
            count = 1;
        }
        mem->period     = count;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = -((gRandomLcgState >> 16) & 0xF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
        if (arg0->spawnArg1.value < 0) {
            if (mem->angle & 1) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, coord, ((mem->scale * 3) >> 2) + 0x3000, NULL);
            }
            if (!(mem->angle & 3)) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, coord, ((mem->scale * 3) >> 2) + 0x3000, NULL);
            }
        }
        arg0->state = 1;
    }

    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
    block                                    = head - 1;
    block->worldPoint.vy                     = (u16)coord->workm.t[1];
    vz                                       = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
    block->worldPoint.vz                     = vz;
    projectionScratch                        = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->r0    = 0x68;
        prim->g0    = 0x70;
        prim->b0    = 0x38;
        prim->tpage = 0x28;
        setSemiTrans(prim, 1);
        prim->clut = 0x4253;
        prim->u0   = (mem->age / mem->period) << 5;
        prim->v0   = 0x18;
        prim->u1   = ((mem->age / mem->period) << 5) + 0x1F;
        prim->v1   = 0x18;
        prim->u2   = (mem->age / mem->period) << 5;
        prim->v2   = 0x37;
        prim->u3   = ((mem->age / mem->period) << 5) + 0x1F;
        prim->v3   = 0x37;

        block->extent.corner.x = ((((s32)mem->scale * 31) / block->depth) * rsin(mem->angle)) >> 12;
        block->extent.corner.y = ((((s32)mem->scale * 31) / block->depth) * rcos(mem->angle)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        block->extent.corner.x = ((((s32)mem->scale * 31) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
        block->extent.corner.y = ((((s32)mem->scale * 31) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        coord->coord.t[2]  += mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        step                = mem->age + 1;
        mem->age            = step;
        if (step > (mem->period * 8) - 1) {
            effectKillTask(mem, arg0);
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
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfSize;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfSize;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
        for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
            quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfSize;
            quadScratch->vertices[cornerIndex].vy = 0;
            quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfSize;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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
                if ((s16)worldCollisionProjectGroundPoint(MATRIX_TRANS(&coord->workm), &groundPoint) != 0) {
                    effectDrawGroundShadow(&groundPoint, EFFECT_PLAYER_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
                }
            }
        }
    }
}

TmdBone D_80111ED0[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_80111EF4[1] = {
#include "assets/gameplay_effect_80111fc8_part_vertices.inc"
};
SVECTOR D_80111EF8[6] = {
#include "assets/gameplay_effect_80111fc8_vertices.inc"
};
SVECTOR D_80111F28[4] = {
#include "assets/gameplay_effect_80111fc8_normals.inc"
};
u32 D_80111F48[32] = {
#include "assets/gameplay_effect_80111fc8_packets.inc"
};
TmdSource D_80111FC8    = { 0, 196, 0, 1, D_80111EF4, D_80111EF8, D_80111F28, D_80111ED0, D_80111F48 };
TmdBone   D_80111FEC[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_80112010[1] = {
#include "assets/gameplay_effect_80111fc8_part_vertices.inc"
};
SVECTOR D_80112014[6] = {
#include "assets/gameplay_effect_801120e4_vertices.inc"
};
SVECTOR D_80112044[4] = {
#include "assets/gameplay_effect_801120e4_normals.inc"
};
u32 D_80112064[32] = {
#include "assets/gameplay_effect_801120e4_packets.inc"
};
TmdSource D_801120E4    = { 0, 196, 0, 1, D_80112010, D_80112014, D_80112044, D_80111FEC, D_80112064 };
TmdBone   D_80112108[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_8011212C[1] = {
#include "assets/gameplay_effect_80111fc8_part_vertices.inc"
};
SVECTOR D_80112130[6] = {
#include "assets/gameplay_effect_80112200_vertices.inc"
};
SVECTOR D_80112160[4] = {
#include "assets/gameplay_effect_80112200_normals.inc"
};
u32 D_80112180[32] = {
#include "assets/gameplay_effect_80112200_packets.inc"
};
TmdSource D_80112200    = { 0, 196, 0, 1, D_8011212C, D_80112130, D_80112160, D_80112108, D_80112180 };
TmdBone   D_80112224[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_80112248[1] = {
#include "assets/gameplay_effect_80111fc8_part_vertices.inc"
};
SVECTOR D_8011224C[6] = {
#include "assets/gameplay_effect_8011231c_vertices.inc"
};
SVECTOR D_8011227C[4] = {
#include "assets/gameplay_effect_8011231c_normals.inc"
};
u32 D_8011229C[32] = {
#include "assets/gameplay_effect_8011231c_packets.inc"
};
TmdSource D_8011231C    = { 0, 196, 0, 1, D_80112248, D_8011224C, D_8011227C, D_80112224, D_8011229C };
TmdBone   D_80112340[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_80112364[1] = {
#include "assets/gameplay_effect_801124b8_part_vertices.inc"
};
SVECTOR D_80112368[8] = {
#include "assets/gameplay_effect_801124b8_vertices.inc"
};
SVECTOR D_801123A8[10] = {
#include "assets/gameplay_effect_801124b8_normals.inc"
};
u32 D_801123F8[48] = {
#include "assets/gameplay_effect_801124b8_packets.inc"
};
TmdSource D_801124B8     = { 0, 312, 0, 1, D_80112364, D_80112368, D_801123A8, D_80112340, D_801123F8 };
SVECTOR   D_801124DC[34] = {
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
SVECTOR D_801125EC[34] = {
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
SVECTOR D_801126FC[34] = {
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
SVECTOR D_8011280C[34] = {
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
_EffectCriticalHitStyle D_8011291C[6] = {
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 15, 7), 24 },
    { EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 7, 7), 32 },
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 15, 15), 24 },
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 15, 7), 24 },
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 7, 15), 24 },
    { EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 7, 15), 24 },
};
_EffectImpactFlashFrame Gp_EffSprRecs[4] = {
    { 55, 112, 200, 176, 266, 576 },
    { 31, 192, 56, 192, 266, 512 },
    { 55, 168, 200, 192, 266, 576 },
    { 31, 224, 56, 192, 266, 512 },
};
