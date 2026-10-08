#include "pe/antibody.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"
/// Signed texture-frame counter for both antibody sprite instances.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

/// Visual tuning of the antibody cast for one Parasite Energy level.
///
/// The cast is a set of rings, an arc and a fan of glow wedges that grow
/// around the caster while motes spawned on a circle close on its centre. The
/// cast task and the mote task both select their row with the level digit of
/// the attachment id, less one, which they keep in `EffectWork::index`.
///
/// `scaleLimit` and `scaleStep` are in the units of the cast's
/// `EffectWork::scale`, which is at once the brightness of the drawing (its
/// low byte is the red and green channel) and, multiplied up, the radius of
/// each ring. The mote sizes are the size argument of the mote's sprite quad.
typedef struct {
    s16 wedgeCount;         // Glow wedges fanned around the ring; the yaw table holds 16
    s16 scaleLimit;         // Scale that ends the growth; the fading rings are drawn at this scale
    s16 scaleStep;          // Scale gained per frame of growth; at level 3 the fading arc and wedges keep gaining it
    s16 moteSpawnSize;      // Size a mote is spawned with
    s16 moteRerollSizeBase; // Low end of the 0x200-wide range a mote's size is re-rolled in; doubled for the larger sprite
    s16 moteSpawnRadius;    // Radius of the horizontal circle each burst of four motes is spawned on
    s16 moteSpawnInterval;  // Frames between mote bursts during the first 0x14 frames of the cast
} _AntibodyLevelTuning;
STATIC_ASSERT_SIZEOF(_AntibodyLevelTuning, 0xE);

/// Per-level tuning for the antibody motes, one row per PE level 1-3,
/// weakest first.
static _AntibodyLevelTuning D_antibody_80130BD4[] = {
    { 0x0008, 0x0090, 0x0005, 0x0200, 0x0080, 0x0600, 0x0008 },
    { 0x000C, 0x00C0, 0x0006, 0x0300, 0x0100, 0x0700, 0x0006 },
    { 0x0010, 0x00F0, 0x0007, 0x0400, 0x0180, 0x0800, 0x0004 },
};

/// The `sndEvtRequestScriptStart` id for each `D_antibody_80130BD4` row, played
/// once when `antibodyCastTask` seeds the cast.
static s32 D_antibody_80130C00[] = { 0xE0290001, 0xE02C0001, 0xE02F0001 };

/// Antibody mote instance of `spriteQuadDraw`.
///
/// `pos` is the effect coordinate, `frame` selects the mote cell, `size` is
/// the perspective numerator and `angle` is the spin in 4096 units per turn.
static void spriteQuadDrawMote(const GfxCoord* pos, s16 frame, s16 size, s16 angle);
static void _antibodyDrawMoteStrip(const GfxCoord* coord, s16 textureFrame, s16 widthScale);

/// Sixteen wedge yaws, refilled once per cast by `antibodyCastTask`.
/// Entry `i` is `i * (0x1000 / wedgeCount)` plus a 9-bit `gRandomLcgState` draw;
/// states 1 and 2 pass one yaw per frame to `glowDrawWedge`.
static s16 D_antibody_80130C0C[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/// Sets the Antibody cast's parent-space Y for drawing or mote placement.
///
/// `localY` is an absolute signed game-coordinate position in the parent's
/// space. The cast draws at -1024 and restores 0 before spawning motes.
/// Invalidates the coordinate's cache and composes through its full parent
/// chain so the new origin is immediately available in `castCoord->workm`.
///
/// Borrows a non-NULL writable coordinate and its live, writable, acyclic
/// parent chain for this call. Composition changes GTE working registers.
static inline void _antibodySetCastHeight(GfxCoord* castCoord, s32 localY)
{
    castCoord->coord.t[1]   = localY;
    castCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(castCoord);
}

void antibodyCastTask(Task* task)
{
    enum {
        ANTIBODY_CAST_STATE_INITIALIZE      = 0,
        ANTIBODY_CAST_STATE_GROWING         = 1,
        ANTIBODY_CAST_STATE_FADING          = 2,
        ANTIBODY_CAST_LEVEL_ID_RADIX        = 10,
        ANTIBODY_CAST_LEVEL_ONE_ROW         = 0,
        ANTIBODY_CAST_LEVEL_THREE_ROW       = 2,
        ANTIBODY_CAST_MOTE_BURST_PHASE      = 1,
        ANTIBODY_CAST_MOTE_BURST_END_AGE    = 20,
        ANTIBODY_CAST_FADE_STEP             = 16,
        ANTIBODY_CAST_RING_LOCAL_Y          = -1024,
        ANTIBODY_CAST_OUTER_BAND_HEIGHT     = 128,
        ANTIBODY_CAST_FULL_TURN             = 4096,
        ANTIBODY_CAST_MOTES_PER_BURST       = 4,
        ANTIBODY_CAST_QUARTER_TURN          = ANTIBODY_CAST_FULL_TURN / ANTIBODY_CAST_MOTES_PER_BURST,
        ANTIBODY_CAST_WEDGE_YAW_JITTER_MASK = 511,
        ANTIBODY_CAST_TRIG_SHIFT            = 12,
    };
    EffectWork*      work;
    GfxCoord*        coord;
    AttachmentState* attachmentState;
    s32              wedgeIndex;
    u8               rgb[3];

    attachmentState = &Gp_StateC08;
    work            = task->spawnArg2.pointer;
    coord           = task->extra.coordBody->coord;
    if ((attachmentState->effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case ANTIBODY_CAST_STATE_INITIALIZE: {

                // Seed the level-dependent wedges and apply the spell before growth starts.
                coord->parent = work->parent;
                gfxSetRotIdentity(&coord->coord);
                coord->coord.t[2]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[0]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                gRoomEffectState->peFxFlags &= (u16)~ROOM_EFFECT_PE_ANTIBODY_AURA;
                attachmentState->flags      |= ATTACHMENT_FLAG_APPLY_STATS;
                task->state                  = ANTIBODY_CAST_STATE_GROWING;
                work->index                  = (Gp_StateC08.attachId % ANTIBODY_CAST_LEVEL_ID_RADIX) - 1;
                wedgeIndex                   = 0;
                if (D_antibody_80130BD4[work->index].wedgeCount > 0) {
                    do {
                        s16* wedgeYaws;
                        s32  baseYaw;
                        s32  yawRng;

                        wedgeYaws             = D_antibody_80130C0C;
                        baseYaw               = wedgeIndex * (ANTIBODY_CAST_FULL_TURN / D_antibody_80130BD4[work->index].wedgeCount);
                        yawRng                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        wedgeYaws[wedgeIndex] = baseYaw + (((u32)yawRng >> 16) & ANTIBODY_CAST_WEDGE_YAW_JITTER_MASK);
                        gRandomLcgState       = yawRng;
                    } while (++wedgeIndex < D_antibody_80130BD4[work->index].wedgeCount);
                }
                {
                    s32 pan;

                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(D_antibody_80130C00[work->index], pan,
                                             (s8)worldCoordGetOriginAudioDepth(coord));
                }
                return;
            }
            case ANTIBODY_CAST_STATE_GROWING: {
                _AntibodyLevelTuning* tuningTable;
                _AntibodyLevelTuning* wedgeTuningTable;

                EffectWork* moteWork;
                s32         moteRng;
                s32         burstBaseYaw;
                s16         moteYaw;
                s16*        wedgeYawCursor;
                s16         wedgeCount;

                // Draw the growing rings above the caster, then spawn motes at its origin.
                tuningTable = D_antibody_80130BD4;
                work->scale = work->scale + tuningTable[work->index].scaleStep;
                rgb[0]      = (u8)work->scale;
                rgb[1]      = (u8)work->scale;
                rgb[2]      = work->scale >> 1;
                _antibodySetCastHeight(coord, ANTIBODY_CAST_RING_LOCAL_Y);
                effectDrawGouraudDisc(coord, (s16)(work->scale * 4), rgb);
                effectDrawGouraudDisc(coord, (s16)(work->scale * 8), rgb);
                effectDrawGouraudDisc(coord, (s16)(work->scale * 0xC), rgb);
                if (work->index != ANTIBODY_CAST_LEVEL_ONE_ROW) {
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    effectDrawOuterGlowBand(coord, (s16)(work->scale * 8), ANTIBODY_CAST_OUTER_BAND_HEIGHT, rgb);
                }
                wedgeIndex = 0;
                wedgeCount = tuningTable[work->index].wedgeCount;
                if (wedgeCount > 0) {
                    wedgeTuningTable = tuningTable;

                    wedgeYawCursor = D_antibody_80130C0C;
                    do {
                        glowDrawWedge(coord, (s16)(work->scale * 6), *wedgeYawCursor, rgb);
                        wedgeYawCursor += 1;
                    } while (++wedgeIndex < wedgeTuningTable[work->index].wedgeCount);
                }
                _antibodySetCastHeight(coord, 0);
                if (work->age < ANTIBODY_CAST_MOTE_BURST_END_AGE) {
                    if ((work->age % D_antibody_80130BD4[work->index].moteSpawnInterval) == ANTIBODY_CAST_MOTE_BURST_PHASE) {
                        burstBaseYaw = 0;
                        do {
                            moteRng         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            moteYaw         = burstBaseYaw + (((u32)moteRng >> 16) & (ANTIBODY_CAST_QUARTER_TURN - 1));
                            gRandomLcgState = moteRng;
                            work->angle     = moteYaw;
                            work->move.vx =
                                (D_antibody_80130BD4[work->index].moteSpawnRadius * rsin(moteYaw)) >> ANTIBODY_CAST_TRIG_SHIFT;
                            work->move.vz = (D_antibody_80130BD4[work->index].moteSpawnRadius *
                                             rcos(work->angle)) >>
                                            ANTIBODY_CAST_TRIG_SHIFT;
                            moteWork = effectSpawn(EFFECT_ANTIBODY_MOTE, coord, 0, &work->move);
                            if (moteWork != NULL) {
                                taskReparent(task, moteWork->task);
                            }
                            burstBaseYaw += ANTIBODY_CAST_QUARTER_TURN;
                        } while (burstBaseYaw < ANTIBODY_CAST_FULL_TURN);
                    }
                }
                if (work->scale > D_antibody_80130BD4[work->index].scaleLimit) {
                    effectSpawn((EFFECT_ANTIBODY_AURA | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                    work->period = work->scale;
                    task->state  = ANTIBODY_CAST_STATE_FADING;
                }
                return;
            }
            case ANTIBODY_CAST_STATE_FADING: {
                _AntibodyLevelTuning* tuningTable;
                _AntibodyLevelTuning* wedgeTuningTable;

                s16* wedgeYawCursor;
                s16  wedgeCount;

                // Disc radii stay capped; level three keeps expanding the outer band and wedges.
                if (work->scale < ANTIBODY_CAST_FADE_STEP + 1) {
                    break;
                }
                work->scale = work->scale - ANTIBODY_CAST_FADE_STEP;
                rgb[0]      = (u8)work->scale;
                rgb[1]      = (u8)work->scale;
                rgb[2]      = work->scale >> 1;
                _antibodySetCastHeight(coord, ANTIBODY_CAST_RING_LOCAL_Y);
                tuningTable = D_antibody_80130BD4;
                effectDrawGouraudDisc(coord, (s16)(tuningTable[work->index].scaleLimit * 4), rgb);
                effectDrawGouraudDisc(coord, (s16)(tuningTable[work->index].scaleLimit * 8), rgb);
                effectDrawGouraudDisc(coord, (s16)(tuningTable[work->index].scaleLimit * 0xC), rgb);
                if (work->index != ANTIBODY_CAST_LEVEL_ONE_ROW) {
                    if (work->index == ANTIBODY_CAST_LEVEL_THREE_ROW) {
                        work->period = work->period + tuningTable[work->index].scaleStep;
                    }
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    effectDrawOuterGlowBand(coord, (s16)(work->period * 8), ANTIBODY_CAST_OUTER_BAND_HEIGHT, rgb);
                }
                wedgeIndex = 0;
                wedgeCount = D_antibody_80130BD4[work->index].wedgeCount;
                if (wedgeCount > 0) {
                    wedgeTuningTable = D_antibody_80130BD4;

                    wedgeYawCursor = D_antibody_80130C0C;
                    do {
                        glowDrawWedge(coord, (s16)(work->period * 6), *wedgeYawCursor, rgb);
                        wedgeYawCursor += 1;
                    } while (++wedgeIndex < wedgeTuningTable[work->index].wedgeCount);
                }
                _antibodySetCastHeight(coord, 0);
                return;
            }
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

void antibodyMoteTask(Task* task)
{
    enum {
        ANTIBODY_MOTE_INITIALIZE       = 0,
        ANTIBODY_MOTE_CONVERGE         = 1,
        ANTIBODY_MOTE_RISE             = 2,
        ANTIBODY_MOTE_PLAYER_FLASH     = 3,
        ANTIBODY_MOTE_INWARD_FRAMES    = 16,
        ANTIBODY_MOTE_LAST_FRAME       = 21,
        ANTIBODY_MOTE_RISE_STEP        = -128,
        ANTIBODY_MOTE_FLASH_MASK       = 15,    // One roll in sixteen freezes an inward mote
        ANTIBODY_MOTE_REROLL_MASK      = 7,     // One roll in eight changes size and spin
        ANTIBODY_MOTE_SIZE_JITTER_MASK = 0x1FF, // Add 0..511 to the level's size base
        ANTIBODY_MOTE_ANGLE_MASK       = 0xFFF  // 4096 angle units per turn
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         seedRng;
    s32         flashRng;
    s16         levelIndex;

    /// Occasionally rerolls the current mote's size and spin in shared random draw order.
    ///
    /// Captures this function's `work`, the level table, random state and masks.
    /// `sizeMultiplier` must be the signed integer constant 1 (mote) or 2
    /// (player-linked flash); it is evaluated once, only when the check succeeds.
    /// Consumes one check draw, then separate size and angle draws on success.
    /// Local random values belong to each expansion; store size before drawing
    /// the angle. A single statement with no control flow into its caller;
    /// defined only for this function and undefined immediately after its body.
#define ANTIBODY_MOTE_REROLL_APPEARANCE(sizeMultiplier)                                                \
    do {                                                                                               \
        s32 rerollRng;                                                                                 \
        s32 sizeRng;                                                                                   \
        s32 angleRng;                                                                                  \
                                                                                                       \
        rerollRng       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;              \
        gRandomLcgState = rerollRng;                                                                   \
        if ((((u32)rerollRng >> 16) & ANTIBODY_MOTE_REROLL_MASK) == 0) {                               \
            sizeRng         = rerollRng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                \
            gRandomLcgState = sizeRng;                                                                 \
            work->scale     = D_antibody_80130BD4[work->index].moteRerollSizeBase * (sizeMultiplier) + \
                          (((u32)sizeRng >> 16) & ANTIBODY_MOTE_SIZE_JITTER_MASK);                     \
            angleRng        = sizeRng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                  \
            gRandomLcgState = angleRng;                                                                \
            work->angle     = ((u32)angleRng >> 16) & ANTIBODY_MOTE_ANGLE_MASK;                        \
        }                                                                                              \
    } while (0)

    work                = task->spawnArg2.pointer;
    coord               = task->extra.coordBody->coord;
    work->age           = work->age + 1;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (task->state) {
        case ANTIBODY_MOTE_INITIALIZE:
            // Keep the parent's rotation; the mote's local transform starts at its spawn offset.
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);

            coord->coord.t[0] = work->pos.vx;
            coord->coord.t[1] = work->pos.vy;
            coord->coord.t[2] = work->pos.vz;

            // Q12 scaling derives a constant inward step; each signed component truncates in the GTE.
            gte_lddp(ONE / ANTIBODY_MOTE_INWARD_FRAMES);
            gte_ldsv(&work->pos);
            gte_gpf12();
            gte_stsv(&work->move);

            task->state     = ANTIBODY_MOTE_CONVERGE;
            seedRng         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = seedRng;
            levelIndex      = Gp_StateC08.attachId % 10 - 1;
            work->index     = levelIndex;
            work->scale     = D_antibody_80130BD4[levelIndex].moteSpawnSize;
            work->angle     = ((u32)seedRng >> 16) & ANTIBODY_MOTE_ANGLE_MASK;
            /* fallthrough */
        case ANTIBODY_MOTE_CONVERGE:
            ANTIBODY_MOTE_REROLL_APPEARANCE(1);
            coord->coord.t[0]  -= work->move.vx;
            coord->coord.t[1]  -= work->move.vy;
            coord->coord.t[2]  -= work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            spriteQuadDrawMote(coord, work->age, work->scale, work->angle);
            if (work->age >= ANTIBODY_MOTE_INWARD_FRAMES) {
                work->move.vy = ANTIBODY_MOTE_RISE_STEP;
                task->state   = ANTIBODY_MOTE_RISE;
                return;
            }
            flashRng        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = flashRng;
            if ((((u32)flashRng >> 16) & ANTIBODY_MOTE_FLASH_MASK) == 0) {
                task->state = ANTIBODY_MOTE_PLAYER_FLASH;
            }
            return;
        case ANTIBODY_MOTE_RISE:
            ANTIBODY_MOTE_REROLL_APPEARANCE(1);
            coord->coord.t[1]  += work->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            spriteQuadDrawMote(coord, work->age, work->scale, work->angle);
            // Both final phases emit their last frame before retiring the counted effect.
            if (work->age >= ANTIBODY_MOTE_LAST_FRAME) {
                effectKillTask(work, task);
            }
            break;
        case ANTIBODY_MOTE_PLAYER_FLASH:
            // Freeze the local position, then draw the larger sprite and its link to the player.
            ANTIBODY_MOTE_REROLL_APPEARANCE(2);
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, work->age, work->scale, work->angle);
            _antibodyDrawMoteStrip(coord, work->age, work->scale);
            if (work->age >= ANTIBODY_MOTE_LAST_FRAME) {
                effectKillTask(work, task);
            }
            break;
    }
}

#undef ANTIBODY_MOTE_REROLL_APPEARANCE

/// Defines the mote instance. The next inclusion is unbound and becomes `spriteQuadDraw`.
#define SPRITE_QUAD_FUNC spriteQuadDrawMote
/// Packed additive mote texture page: 4-bit indexed texels at VRAM X=576 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 576, 0)
/// Mote palette: VRAM X=96 words, Y=267 scanlines.
#define SPRITE_QUAD_CLUT getClut(96, 267)
/// Texel width and horizontal stride of each cell in the mote's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the mote texture row, repeated as the effect ages.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the mote strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x50
#define SPRITE_QUAD_V1    0x77
/// Perspective-sizing multiplier for the antibody mote.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

/// Larger antibody sprite palette: VRAM X=144 words, Y=267 scanlines.
#define SPRITE_QUAD_CLUT getClut(144, 267)
/// Texel width and horizontal stride of each cell in the larger sprite's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the larger antibody sprite's repeating texture row.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the larger sprite strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for the larger antibody sprite.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

/// Queues an additive textured strip from an Antibody mote to the player's second model coordinate.
///
/// `coord` is borrowed read-only; its cached translation and the player's
/// `coords[1].workm.t` must already be composed in `GsWSMATRIX` input space.
/// Each component is narrowed to s16 before projection. The player task must
/// own a live model with at least two coordinates. A negative GTE FLAG after
/// either projection rejects the strip. Its sizing and sorting depth is the
/// start's SZ3 / 4 plus one; the end contributes no depth.
///
/// `textureFrame` repeats modulo four over two columns of 128 texels and two
/// rows of 24. `widthScale` is a signed perspective numerator: the corner
/// offset length is `widthScale * 23 / depth` pixels before Q12 rotation.
/// Opposite corners use the screen segment's angle and then a quarter turn;
/// GPU coordinate fields keep the low 16 bits of each sum.
///
/// Requires one free `EffectStripScratch` block and space for one `POLY_FT4`
/// in the current frame's primitive arena and ordering table. The scratch
/// block is released on every path; a queued packet lasts through GPU use.
/// Overwrites the GTE matrix, vector and projection registers.
static void _antibodyDrawMoteStrip(const GfxCoord* coord, s16 textureFrame, s16 widthScale)
{
    enum {
        ANTIBODY_STRIP_PLAYER_PART  = 1,
        ANTIBODY_STRIP_DEPTH_BIAS   = 1,
        ANTIBODY_STRIP_CELL_WIDTH   = 128,
        ANTIBODY_STRIP_CELL_HEIGHT  = 24,
        ANTIBODY_STRIP_COLUMN_MASK  = 1,
        ANTIBODY_STRIP_FRAME_MASK   = 3,
        ANTIBODY_STRIP_TOP_V        = 208,   // Second row begins at V=232
        ANTIBODY_STRIP_QUARTER_TURN = 0x400, // 4096 angle units per turn
        ANTIBODY_STRIP_TRIG_SHIFT   = 12     // rsin/rcos: 4096 represents 1.0
    };
    EffectStripScratch* scratch;
    POLY_FT4*           quad;
    const GfxCoord*     playerCoord;
    s32                 uLeft;
    s32                 uRight;
    s32                 vTop;
    s32                 vBottom;
    s16                 screenAngle;

    /// Projects one endpoint with the already-loaded GTE matrices, leaving SZ3 available.
    ///
    /// Inputs and outputs are word-aligned; the complete SVECTOR is readable.
    /// Writes signed screen pixels and the complete FLAG word for rejection.
    /// Each pointer argument is evaluated once, in order; captures no caller
    /// identifiers. Expands to several statements, so invoke in a braced block.
    /// Defined only around this drawer and undefined immediately after its body.
#define ANTIBODY_STRIP_PROJECT_POINT(worldPoint, screenPoint, projectionFlags) \
    gte_ldv0((worldPoint));                                                    \
    gte_rtps();                                                                \
    gte_stsxy((screenPoint));                                                  \
    gte_stflg((projectionFlags))

    // Stage both cached translations' low halves before loading the camera matrices.
    playerCoord            = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[ANTIBODY_STRIP_PLAYER_PART];
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectStripScratch);
    scratch->worldStart.vx = coord->workm.t[0];
    scratch->worldStart.vy = coord->workm.t[1];
    scratch->worldStart.vz = coord->workm.t[2];
    scratch->worldEnd.vx   = playerCoord->workm.t[0];
    scratch->worldEnd.vy   = playerCoord->workm.t[1];
    scratch->worldEnd.vz   = playerCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    ANTIBODY_STRIP_PROJECT_POINT(&scratch->worldStart, &scratch->screenStart, &scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth += ANTIBODY_STRIP_DEPTH_BIAS;
        ANTIBODY_STRIP_PROJECT_POINT(&scratch->worldEnd, &scratch->screenEnd, &scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            // Raw texture colour with additive blending for texels whose colour has bit 15 set.
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setShadeTex(quad, 1);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
            quad->clut  = getClut(128, 267);
            uLeft       = (textureFrame & ANTIBODY_STRIP_COLUMN_MASK) * ANTIBODY_STRIP_CELL_WIDTH;
            uRight      = uLeft + (ANTIBODY_STRIP_CELL_WIDTH - 1);
            // Subtract wrapped V origins; packet bytes retain inclusive endpoints.
            vTop    = ((textureFrame & ANTIBODY_STRIP_FRAME_MASK) >> 1) * ANTIBODY_STRIP_CELL_HEIGHT - (256 - ANTIBODY_STRIP_TOP_V);
            vBottom = ((textureFrame & ANTIBODY_STRIP_FRAME_MASK) >> 1) * ANTIBODY_STRIP_CELL_HEIGHT -
                      (256 - ANTIBODY_STRIP_TOP_V - (ANTIBODY_STRIP_CELL_HEIGHT - 1));
            setUV4(quad, uLeft, vTop, uRight, vTop, uLeft, vBottom, uRight, vBottom);
            // Resolve perpendicular and longitudinal offsets against the projected segment.
            screenAngle            = ratan2(scratch->screenEnd.vy - scratch->screenStart.vy, scratch->screenEnd.vx - scratch->screenStart.vx);
            scratch->cornerOffsetX = (((widthScale * (ANTIBODY_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle)) >> ANTIBODY_STRIP_TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (ANTIBODY_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle)) >> ANTIBODY_STRIP_TRIG_SHIFT;
            quad->x0               = scratch->screenStart.vx + scratch->cornerOffsetX;
            quad->x3               = scratch->screenEnd.vx - scratch->cornerOffsetX;
            quad->y0               = scratch->screenStart.vy - scratch->cornerOffsetY;
            quad->y3               = scratch->screenEnd.vy + scratch->cornerOffsetY;
            scratch->cornerOffsetX = (((widthScale * (ANTIBODY_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle + ANTIBODY_STRIP_QUARTER_TURN)) >> ANTIBODY_STRIP_TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (ANTIBODY_STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle + ANTIBODY_STRIP_QUARTER_TURN)) >> ANTIBODY_STRIP_TRIG_SHIFT;
            quad->x1               = scratch->screenEnd.vx + scratch->cornerOffsetX;
            quad->x2               = scratch->screenStart.vx - scratch->cornerOffsetX;
            quad->y1               = scratch->screenEnd.vy - scratch->cornerOffsetY;
            quad->y2               = scratch->screenStart.vy + scratch->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectStripScratch);
}

#undef ANTIBODY_STRIP_PROJECT_POINT

#include "../../shared/glow_draw_wedge.inc.c"
