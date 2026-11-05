#include "antibody.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/attachment_state.h>
#include <gameplay/attachments.h>
#include <gameplay/effects.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/gfx.h>
#include <engine/random.h>
#include <engine/gfx_types.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <effects/glow/glow.h>
/// Signed texture-frame counter for both antibody sprite instances.
#define SPRITE_QUAD_FRAME_T s16
#include <effects/sprite_quad/sprite_quad.h>
#include <pe/effects/strip/strip.h>

PE2_EDITOR_LABEL("Rings")
/// Height of the rings above the caster, in its local units (up is negative).
PE2_OPTION_DEF(RING_Y, int, -1024)
/// The three rings' radii, as multiples of the cast's scale.
PE2_OPTION_DEF(RING_1_RADIUS, int, 4)
PE2_OPTION_DEF(RING_2_RADIUS, int, 8)
PE2_OPTION_DEF(RING_3_RADIUS, int, 12)
/// The outer glow band drawn from level 2: its radius as a multiple of the scale, its height, and how much dimmer it is
/// than the rings, as a shift of their color.
PE2_OPTION_DEF(BAND_RADIUS, int, 8)
PE2_OPTION_DEF(BAND_HEIGHT, int, 128)
PE2_OPTION_DEF(BAND_DIM_SHIFT, int, 1)
/// The glow wedges' length as a multiple of the scale, and the range of the random yaw added to each, a power of two in
/// a 4096-unit turn.
PE2_OPTION_DEF(WEDGE_LENGTH, int, 6)
PE2_OPTION_DEF(WEDGE_JITTER, int, 512)
/// How much dimmer blue is than red and green, as a shift: the cast is yellow.
PE2_OPTION_DEF(BLUE_SHIFT, int, 1)
/// Brightness lost each frame while the cast fades.
PE2_OPTION_DEF(FADE_STEP, int, 16)

PE2_EDITOR_LABEL("Motes")
/// Motes spawned in each burst, a power of two each given its share of the turn, and the frames from the cast's start
/// during which bursts come.
PE2_OPTION_DEF(MOTES_PER_BURST, int, 4)
PE2_OPTION_DEF(MOTE_BURST_FRAMES, int, 20)
/// Frames a mote takes to close on the caster, and the frame it goes on, after rising since then.
PE2_OPTION_DEF(MOTE_INWARD_FRAMES, int, 16)
PE2_OPTION_DEF(MOTE_LAST_FRAME, int, 21)
/// Local units a mote rises each frame.
PE2_OPTION_DEF(MOTE_RISE_SPEED, int, 128)
/// One frame in this many, a closing mote stops, drawn larger with a strip to the player; and one frame in this many, a
/// mote changes size and spin. Powers of two.
PE2_OPTION_DEF(MOTE_STOP_ODDS, int, 16)
PE2_OPTION_DEF(MOTE_CHANGE_ODDS, int, 8)
/// The range added to a level's size when a mote changes, a power of two, and how many times larger a stopped mote is.
PE2_OPTION_DEF(MOTE_SIZE_JITTER, int, 512)
PE2_OPTION_DEF(STOPPED_MOTE_SCALE, int, 2)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(WEDGE_JITTER)) && PE2_OPTION(WEDGE_JITTER) <= 4096, wedge_jitter_is_a_mask);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(MOTES_PER_BURST)) && PE2_OPTION(MOTES_PER_BURST) <= 4096, motes_share_the_turn);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(MOTE_STOP_ODDS)) && POWER_OF_TWO(PE2_OPTION(MOTE_CHANGE_ODDS)), mote_odds_are_masks);
STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(MOTE_SIZE_JITTER)), mote_size_jitter_is_a_mask);
STATIC_ASSERT(PE2_OPTION(MOTE_INWARD_FRAMES) > 0 && PE2_OPTION(MOTE_LAST_FRAME) > PE2_OPTION(MOTE_INWARD_FRAMES), motes_rise_after_closing_in);

/// The cast's tuning at one Parasite Energy level, by the level digit of the attachment id less one. A scale is at once
/// the drawing's brightness (its low byte the red and green) and, multiplied up, each ring's radius.
typedef struct {
    s16 wedges;          // Glow wedges fanned around the rings
    s16 scaleLimit;      // The scale the growth ends at, which the fading rings keep
    s16 scaleStep;       // Scale gained each frame of growth; at level 3 the fading band and wedges keep gaining it
    s16 moteSize;        // Size a mote is spawned with
    s16 moteChangedSize; // The least size a mote changes to, before its jitter; times STOPPED_MOTE_SCALE when stopped
    s16 moteRadius;      // Radius of the circle each burst is spawned on
    s16 moteInterval;    // Frames between bursts
} _LevelTuning;
PE2_TYPE(_LevelTuning)

PE2_EDITOR_LABEL("Levels")
/// The cast at levels 1 to 3.
PE2_OPTION_DEF(LEVELS, _LevelTuning[3], {
    { 8, 0x0090, 5, 0x0200, 0x0080, 0x0600, 8 },
    { 12, 0x00C0, 6, 0x0300, 0x0100, 0x0700, 6 },
    { 16, 0x00F0, 7, 0x0400, 0x0180, 0x0800, 4 },
})
PE2_EDITOR_LABEL("")

/// The most wedges any level fans, which the yaw table holds.
enum {
    MAX_CAST_WEDGES =
        LARGER(LARGER(PE2_OPTION_AT(LEVELS, 0, wedges), PE2_OPTION_AT(LEVELS, 1, wedges)), PE2_OPTION_AT(LEVELS, 2, wedges))
};
STATIC_ASSERT(PE2_OPTION_AT(LEVELS, 0, wedges) > 0 && PE2_OPTION_AT(LEVELS, 1, wedges) > 0 && PE2_OPTION_AT(LEVELS, 2, wedges) > 0,
              every_level_fans_wedges);
STATIC_ASSERT(PE2_OPTION_AT(LEVELS, 0, moteInterval) > 0 && PE2_OPTION_AT(LEVELS, 1, moteInterval) > 0 &&
                  PE2_OPTION_AT(LEVELS, 2, moteInterval) > 0,
              every_level_bursts);

static _LevelTuning _gLevelTuning[] = PE2_OPTION(LEVELS);

/// The cue each level's cast plays.
static s32 _gCastSoundScripts[ARRAY_SIZE(_gLevelTuning)] = { 0xE0290001, 0xE02C0001, 0xE02F0001 };

/// The mote's copy of `spriteQuadDraw`.
static void spriteQuadDrawMote(const GfxCoord* pos, s16 frame, s16 size, s16 angle);
static void _drawMoteStrip(const GfxCoord* coord, s16 textureFrame, s16 widthScale);

/// The wedges' yaws, spaced evenly with a little jitter when a cast starts.
static s16 _gWedgeYaws[MAX_CAST_WEDGES] = { 0 };

/// Moves the cast's coordinate to a height above its parent and composes it: up to draw the rings, back to 0 to spawn
/// motes.
static inline void _setCastHeight(GfxCoord* castCoord, s32 localY)
{
    castCoord->coord.t[1]   = localY;
    castCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(castCoord);
}

void antibodyCastTask(Task* task)
{
    enum {
        CAST_STATE_INITIALIZE = 0,
        CAST_STATE_GROWING    = 1,
        CAST_STATE_FADING     = 2,
        CAST_LEVEL_ID_RADIX   = 10,
        CAST_LEVEL_ONE_ROW    = 0,
        CAST_LEVEL_THREE_ROW  = 2,
        CAST_MOTE_BURST_PHASE = 1,
        CAST_FULL_TURN        = 4096,
        CAST_BURST_SPACING    = CAST_FULL_TURN / PE2_OPTION(MOTES_PER_BURST),
        CAST_TRIG_SHIFT       = 12,
    };
    EffectWork*      work;
    GfxCoord*        coord;
    AttachmentState* attachmentState;
    s32              wedgeIndex;
    u8               rgb[3];

    attachmentState = &gAttachmentState;
    work            = task->spawnArg2.pointer;
    coord           = task->extra.coordBody->coord;
    if ((attachmentState->effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        work->age = work->age + 1;
        switch (task->state) {
            case CAST_STATE_INITIALIZE: {

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
                task->state                  = CAST_STATE_GROWING;
                work->index                  = (gAttachmentState.attachId % CAST_LEVEL_ID_RADIX) - 1;
                wedgeIndex                   = 0;
                if (_gLevelTuning[work->index].wedges > 0) {
                    do {
                        s16* wedgeYaws;
                        s32  baseYaw;
                        s32  yawRng;

                        wedgeYaws             = _gWedgeYaws;
                        baseYaw               = wedgeIndex * (CAST_FULL_TURN / _gLevelTuning[work->index].wedges);
                        yawRng                = RANDOM_LCG_NEXT(gRandomLcgState);
                        wedgeYaws[wedgeIndex] = baseYaw + (((u32)yawRng >> 16) & (PE2_OPTION(WEDGE_JITTER) - 1));
                        gRandomLcgState       = yawRng;
                    } while (++wedgeIndex < _gLevelTuning[work->index].wedges);
                }
                {
                    s32 pan;

                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(_gCastSoundScripts[work->index], pan,
                                             (s8)worldCoordGetOriginAudioDepth(coord));
                }
                return;
            }
            case CAST_STATE_GROWING: {
                _LevelTuning* tuningTable;
                _LevelTuning* wedgeTuningTable;

                EffectWork* moteWork;
                s32         moteRng;
                s32         burstBaseYaw;
                s16         moteYaw;
                s16*        wedgeYawCursor;
                s16         wedgeCount;

                // Draw the growing rings above the caster, then spawn motes at its origin.
                tuningTable = _gLevelTuning;
                work->scale = work->scale + tuningTable[work->index].scaleStep;
                rgb[0]      = (u8)work->scale;
                rgb[1]      = (u8)work->scale;
                rgb[2]      = work->scale >> PE2_OPTION(BLUE_SHIFT);
                _setCastHeight(coord, PE2_OPTION(RING_Y));
                effectDrawGouraudDisc(coord, (s16)(work->scale * PE2_OPTION(RING_1_RADIUS)), rgb);
                effectDrawGouraudDisc(coord, (s16)(work->scale * PE2_OPTION(RING_2_RADIUS)), rgb);
                effectDrawGouraudDisc(coord, (s16)(work->scale * PE2_OPTION(RING_3_RADIUS)), rgb);
                if (work->index != CAST_LEVEL_ONE_ROW) {
                    rgb[0] >>= PE2_OPTION(BAND_DIM_SHIFT);
                    rgb[1] >>= PE2_OPTION(BAND_DIM_SHIFT);
                    rgb[2] >>= PE2_OPTION(BAND_DIM_SHIFT);
                    effectDrawOuterGlowBand(coord, (s16)(work->scale * PE2_OPTION(BAND_RADIUS)), PE2_OPTION(BAND_HEIGHT), rgb);
                }
                wedgeIndex = 0;
                wedgeCount = tuningTable[work->index].wedges;
                if (wedgeCount > 0) {
                    wedgeTuningTable = tuningTable;

                    wedgeYawCursor = _gWedgeYaws;
                    do {
                        glowDrawWedge(coord, (s16)(work->scale * PE2_OPTION(WEDGE_LENGTH)), *wedgeYawCursor, rgb);
                        wedgeYawCursor += 1;
                    } while (++wedgeIndex < wedgeTuningTable[work->index].wedges);
                }
                _setCastHeight(coord, 0);
                if (work->age < PE2_OPTION(MOTE_BURST_FRAMES)) {
                    if ((work->age % _gLevelTuning[work->index].moteInterval) == CAST_MOTE_BURST_PHASE) {
                        burstBaseYaw = 0;
                        do {
                            moteRng         = RANDOM_LCG_NEXT(gRandomLcgState);
                            moteYaw         = burstBaseYaw + (((u32)moteRng >> 16) & (CAST_BURST_SPACING - 1));
                            gRandomLcgState = moteRng;
                            work->angle     = moteYaw;
                            work->move.vx =
                                (_gLevelTuning[work->index].moteRadius * rsin(moteYaw)) >> CAST_TRIG_SHIFT;
                            work->move.vz = (_gLevelTuning[work->index].moteRadius *
                                             rcos(work->angle)) >>
                                            CAST_TRIG_SHIFT;
                            moteWork = effectSpawn(EFFECT_ANTIBODY_MOTE, coord, 0, &work->move);
                            if (moteWork != NULL) {
                                taskReparent(task, moteWork->task);
                            }
                            burstBaseYaw += CAST_BURST_SPACING;
                        } while (burstBaseYaw < CAST_FULL_TURN);
                    }
                }
                if (work->scale > _gLevelTuning[work->index].scaleLimit) {
                    effectSpawn((EFFECT_ANTIBODY_AURA | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                    work->period = work->scale;
                    task->state  = CAST_STATE_FADING;
                }
                return;
            }
            case CAST_STATE_FADING: {
                _LevelTuning* tuningTable;
                _LevelTuning* wedgeTuningTable;

                s16* wedgeYawCursor;
                s16  wedgeCount;

                // Disc radii stay capped; level three keeps expanding the outer band and wedges.
                if (work->scale < PE2_OPTION(FADE_STEP) + 1) {
                    break;
                }
                work->scale = work->scale - PE2_OPTION(FADE_STEP);
                rgb[0]      = (u8)work->scale;
                rgb[1]      = (u8)work->scale;
                rgb[2]      = work->scale >> PE2_OPTION(BLUE_SHIFT);
                _setCastHeight(coord, PE2_OPTION(RING_Y));
                tuningTable = _gLevelTuning;
                effectDrawGouraudDisc(coord, (s16)(tuningTable[work->index].scaleLimit * PE2_OPTION(RING_1_RADIUS)), rgb);
                effectDrawGouraudDisc(coord, (s16)(tuningTable[work->index].scaleLimit * PE2_OPTION(RING_2_RADIUS)), rgb);
                effectDrawGouraudDisc(coord, (s16)(tuningTable[work->index].scaleLimit * PE2_OPTION(RING_3_RADIUS)), rgb);
                if (work->index != CAST_LEVEL_ONE_ROW) {
                    if (work->index == CAST_LEVEL_THREE_ROW) {
                        work->period = work->period + tuningTable[work->index].scaleStep;
                    }
                    rgb[0] >>= PE2_OPTION(BAND_DIM_SHIFT);
                    rgb[1] >>= PE2_OPTION(BAND_DIM_SHIFT);
                    rgb[2] >>= PE2_OPTION(BAND_DIM_SHIFT);
                    effectDrawOuterGlowBand(coord, (s16)(work->period * PE2_OPTION(BAND_RADIUS)), PE2_OPTION(BAND_HEIGHT), rgb);
                }
                wedgeIndex = 0;
                wedgeCount = _gLevelTuning[work->index].wedges;
                if (wedgeCount > 0) {
                    wedgeTuningTable = _gLevelTuning;

                    wedgeYawCursor = _gWedgeYaws;
                    do {
                        glowDrawWedge(coord, (s16)(work->period * PE2_OPTION(WEDGE_LENGTH)), *wedgeYawCursor, rgb);
                        wedgeYawCursor += 1;
                    } while (++wedgeIndex < wedgeTuningTable[work->index].wedges);
                }
                _setCastHeight(coord, 0);
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
        MOTE_INITIALIZE   = 0,
        MOTE_CONVERGE     = 1,
        MOTE_RISE         = 2,
        MOTE_PLAYER_FLASH = 3,
        MOTE_ANGLE_MASK   = 0xFFF // 4096 angle units per turn
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         seedRng;
    s32         flashRng;
    s16         levelIndex;

    /// Now and then gives a mote a new size, the level's base times `sizeMultiplier` plus a jitter, and spin.
#define MOTE_REROLL_APPEARANCE(work, sizeMultiplier)                                            \
    do {                                                                                        \
        s32 rerollRng;                                                                          \
        s32 sizeRng;                                                                            \
        s32 angleRng;                                                                           \
                                                                                                \
        rerollRng       = RANDOM_LCG_NEXT(gRandomLcgState);                                     \
        gRandomLcgState = rerollRng;                                                            \
        if ((((u32)rerollRng >> 16) & (PE2_OPTION(MOTE_CHANGE_ODDS) - 1)) == 0) {               \
            sizeRng         = RANDOM_LCG_NEXT(rerollRng);                                       \
            gRandomLcgState = sizeRng;                                                          \
            (work)->scale   = _gLevelTuning[(work)->index].moteChangedSize * (sizeMultiplier) + \
                            (((u32)sizeRng >> 16) & (PE2_OPTION(MOTE_SIZE_JITTER) - 1));        \
            angleRng        = RANDOM_LCG_NEXT(sizeRng);                                         \
            gRandomLcgState = angleRng;                                                         \
            (work)->angle   = ((u32)angleRng >> 16) & MOTE_ANGLE_MASK;                          \
        }                                                                                       \
    } while (0)

    work                = task->spawnArg2.pointer;
    coord               = task->extra.coordBody->coord;
    work->age           = work->age + 1;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (task->state) {
        case MOTE_INITIALIZE:
            // Keep the parent's rotation; the mote's local transform starts at its spawn offset.
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);

            coord->coord.t[0] = work->pos.vx;
            coord->coord.t[1] = work->pos.vy;
            coord->coord.t[2] = work->pos.vz;

            // Q12 scaling derives a constant inward step; each signed component truncates in the GTE.
            gte_lddp(ONE / PE2_OPTION(MOTE_INWARD_FRAMES));
            gte_ldsv(&work->pos);
            gte_gpf12();
            gte_stsv(&work->move);

            task->state     = MOTE_CONVERGE;
            seedRng         = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = seedRng;
            levelIndex      = gAttachmentState.attachId % 10 - 1;
            work->index     = levelIndex;
            work->scale     = _gLevelTuning[levelIndex].moteSize;
            work->angle     = ((u32)seedRng >> 16) & MOTE_ANGLE_MASK;
            /* fallthrough */
        case MOTE_CONVERGE:
            MOTE_REROLL_APPEARANCE(work, 1);
            coord->coord.t[0]  -= work->move.vx;
            coord->coord.t[1]  -= work->move.vy;
            coord->coord.t[2]  -= work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            spriteQuadDrawMote(coord, work->age, work->scale, work->angle);
            if (work->age >= PE2_OPTION(MOTE_INWARD_FRAMES)) {
                work->move.vy = -PE2_OPTION(MOTE_RISE_SPEED);
                task->state   = MOTE_RISE;
                return;
            }
            flashRng        = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = flashRng;
            if ((((u32)flashRng >> 16) & (PE2_OPTION(MOTE_STOP_ODDS) - 1)) == 0) {
                task->state = MOTE_PLAYER_FLASH;
            }
            return;
        case MOTE_RISE:
            MOTE_REROLL_APPEARANCE(work, 1);
            coord->coord.t[1]  += work->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            spriteQuadDrawMote(coord, work->age, work->scale, work->angle);
            // Both final phases emit their last frame before retiring the counted effect.
            if (work->age >= PE2_OPTION(MOTE_LAST_FRAME)) {
                effectKillTask(work, task);
            }
            break;
        case MOTE_PLAYER_FLASH:
            // Freeze the local position, then draw the larger sprite and its link to the player.
            MOTE_REROLL_APPEARANCE(work, PE2_OPTION(STOPPED_MOTE_SCALE));
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, work->age, work->scale, work->angle);
            _drawMoteStrip(coord, work->age, work->scale);
            if (work->age >= PE2_OPTION(MOTE_LAST_FRAME)) {
                effectKillTask(work, task);
            }
            break;
    }
}

#undef MOTE_REROLL_APPEARANCE

/// The mote's sprite; the next copy is `spriteQuadDraw`, the larger one a stopped mote draws.
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
#define SPRITE_QUAD_TOP_V 0x50
/// Inclusive bottom texel row of the mote strip's square cells, relative to its texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
/// Perspective-sizing multiplier: the cell's inclusive span.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include <effects/sprite_quad/draw.inc.c>

/// Larger antibody sprite palette: VRAM X=144 words, Y=267 scanlines.
#define SPRITE_QUAD_CLUT getClut(144, 267)
/// Texel width and horizontal stride of each cell in the larger sprite's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the larger antibody sprite's repeating texture row.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the larger sprite strip, relative to its texture page.
#define SPRITE_QUAD_TOP_V 0x38
/// Inclusive bottom texel row of the larger sprite's square cells, relative to its texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
/// Perspective-sizing multiplier: the cell's inclusive span.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include <effects/sprite_quad/draw.inc.c>

/// Draws an additive strip from a stopped mote to the player's second model coordinate, sorted at the mote's depth;
/// either end failing to project drops it. `textureFrame` cycles its four cells, `widthScale` sets its width.
static void _drawMoteStrip(const GfxCoord* coord, s16 textureFrame, s16 widthScale)
{
    enum {
        STRIP_PLAYER_PART  = 1,
        STRIP_DEPTH_BIAS   = 1,
        STRIP_CELL_WIDTH   = 128,
        STRIP_CELL_HEIGHT  = 24,
        STRIP_COLUMN_MASK  = 1,
        STRIP_FRAME_MASK   = 3,
        STRIP_TOP_V        = 208,   // Second row begins at V=232
        STRIP_QUARTER_TURN = 0x400, // 4096 angle units per turn
        STRIP_TRIG_SHIFT   = 12     // rsin/rcos: 4096 represents 1.0
    };
    EffectStripScratch* scratch;
    POLY_FT4*           quad;
    const GfxCoord*     playerCoord;
    s32                 uLeft;
    s32                 uRight;
    s32                 vTop;
    s32                 vBottom;
    s16                 screenAngle;

    // Stage both cached translations' low halves before loading the camera matrices.
    playerCoord            = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[STRIP_PLAYER_PART];
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectStripScratch);
    scratch->worldStart.vx = coord->workm.t[0];
    scratch->worldStart.vy = coord->workm.t[1];
    scratch->worldStart.vz = coord->workm.t[2];
    scratch->worldEnd.vx   = playerCoord->workm.t[0];
    scratch->worldEnd.vy   = playerCoord->workm.t[1];
    scratch->worldEnd.vz   = playerCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    _effectStripProjectPoint(scratch, false);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth += STRIP_DEPTH_BIAS;
        _effectStripProjectPoint(scratch, true);
        if (scratch->projectionFlags >= 0) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            // Raw texture colour with additive blending for texels whose colour has bit 15 set.
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setShadeTex(quad, 1);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
            quad->clut  = getClut(128, 267);
            uLeft       = (textureFrame & STRIP_COLUMN_MASK) * STRIP_CELL_WIDTH;
            uRight      = uLeft + (STRIP_CELL_WIDTH - 1);
            // Subtract wrapped V origins; packet bytes retain inclusive endpoints.
            vTop    = ((textureFrame & STRIP_FRAME_MASK) >> 1) * STRIP_CELL_HEIGHT - (256 - STRIP_TOP_V);
            vBottom = ((textureFrame & STRIP_FRAME_MASK) >> 1) * STRIP_CELL_HEIGHT -
                      (256 - STRIP_TOP_V - (STRIP_CELL_HEIGHT - 1));
            setUV4(quad, uLeft, vTop, uRight, vTop, uLeft, vBottom, uRight, vBottom);
            // Resolve perpendicular and longitudinal offsets against the projected segment.
            screenAngle            = ratan2(scratch->screenEnd.vy - scratch->screenStart.vy, scratch->screenEnd.vx - scratch->screenStart.vx);
            scratch->cornerOffsetX = (((widthScale * (STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle)) >> STRIP_TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle)) >> STRIP_TRIG_SHIFT;
            quad->x0               = scratch->screenStart.vx + scratch->cornerOffsetX;
            quad->x3               = scratch->screenEnd.vx - scratch->cornerOffsetX;
            quad->y0               = scratch->screenStart.vy - scratch->cornerOffsetY;
            quad->y3               = scratch->screenEnd.vy + scratch->cornerOffsetY;
            scratch->cornerOffsetX = (((widthScale * (STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rsin(screenAngle + STRIP_QUARTER_TURN)) >> STRIP_TRIG_SHIFT;
            scratch->cornerOffsetY = (((widthScale * (STRIP_CELL_HEIGHT - 1)) / scratch->depth) * rcos(screenAngle + STRIP_QUARTER_TURN)) >> STRIP_TRIG_SHIFT;
            quad->x1               = scratch->screenEnd.vx + scratch->cornerOffsetX;
            quad->x2               = scratch->screenStart.vx - scratch->cornerOffsetX;
            quad->y1               = scratch->screenEnd.vy - scratch->cornerOffsetY;
            quad->y2               = scratch->screenStart.vy + scratch->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectStripScratch);
}

#include <effects/glow/wedge.inc.c>
