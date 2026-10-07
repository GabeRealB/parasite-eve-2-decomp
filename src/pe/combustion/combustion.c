#include "pe/combustion.h"

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
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/pyro_flame.h"
#include "../../shared/sprite_quad.h"

/// Reach and duration of the combustion flame lines at one Parasite Energy
/// level.
///
/// The cast runs two flame emitters, each laying one flame per frame along a
/// line that leaves the caster to one side of its facing and descends as it
/// goes. An emitter and every flame it lays select a row by the level digit of
/// the spell being cast (`AttachmentState::attachId % 10 - 1`), so a higher
/// level lays more flames along a longer, shallower line and burns for longer.
///
/// The steps are distances along the emitter coordinate's own axes. The frame
/// counts are compared with `EffectWork::age`, which is 1 on an emitter's
/// first frame.
typedef struct {
    s16 flameDropStep;     // Added each frame to the Y of the offset the next flame is laid at; positive is down
    s16 flameReachStep;    // Added each frame to the Z of that offset, which starts 0x200 out
    s16 narrowFlameFrames; // Frames an emitter lays narrow flames for. The flame of the frame after is drawn wide and ends the line, and the spell's stats are applied on that frame
    s16 emitterFrames;     // Frames an emitter lives; also the length of the controller vibration that fades out under it
} _CombustionLevelTuning;
STATIC_ASSERT_SIZEOF(_CombustionLevelTuning, 0x8);

/// Strip lengths shared by the drawers and the ember task's stopping tests.
enum {
    COMBUSTION_SMALL_FLAME_FRAME_COUNT = 6,
    COMBUSTION_EMBER_FRAME_COUNT       = 8,
};

/// Added to SZ3 / 4 before perspective sizing and ordering to avoid a zero divisor.
enum { COMBUSTION_SPRITE_DEPTH_BIAS = 1 };

static void _combustionDrawSmallFlame(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor);
static void _combustionDrawEmber(const GfxCoord* coord, s32 animationFrame, s16 sizeFactor);
static void _combustionDrawLargeFlame(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor);

/// Per-level tuning for the combustion flame, one row per PE level 1-3,
/// weakest first.
static _CombustionLevelTuning D_combustion_80130980[] = {
    { 0x0060, 0x0120, 0x0007, 0x0015 },
    { 0x0055, 0x0187, 0x0008, 0x0017 },
    { 0x004C, 0x01F3, 0x0009, 0x0019 },
};

/// The `sndEvtRequestScriptStart` id for each `D_combustion_80130980` row.
static s32 D_combustion_80130998[] = { 0xE00C0002, 0xE00F0002, 0xE0120002 };

/// The effect coordinate's world Y at ignition, saved by
/// `func_combustion_8012EF34` before it re-bases the coordinate on the player.
static s32 D_combustion_801309A4 = 0;

/// Burns the player: parents an effect coordinate to the player model, plays
/// the ignition sound and fades the screen, then spawns a flame every frame
/// while drifting the flame overlay by the `D_combustion_80130980` row for the
/// current intensity. State 1 spawns, state 2 (past `narrowFlameFrames`) only
/// unwinds the yaw the ignition applied, and either state ends as soon as the
/// player is dying (`Gp_StateC08.effectPhase`), parasite-energy effects are
/// cancelled (`gRoomEffectState->peEffectControl`) or the row's
/// `emitterFrames` tick is reached.
void func_combustion_8012EF34(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    EffectWork*       spawned;
    s32               pan;
    u8                rgb[3];

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            if (arg0->spawnArg1.value == 0) {
                arg0->spawnArg1.value = 1;
            }
            D_combustion_801309A4 = coord->workm.t[1];
            rot                   = (GfxRotationWords*)&coord->coord;
            coord->parent         = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            rot->m00M01           = ONE;
            rot->m11M12           = ONE;
            rot->m22              = ONE;
            rot->m02M10           = 0;
            rot->m20M21           = 0;
            coord->coord.t[0]     = 0;
            coord->coord.t[1]     = -0x400;
            coord->coord.t[2]     = 0;
            gfxRotMatrixY(&coord->coord, arg0->spawnArg1.value << 9, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            mem->move.vz = 0x200;
            pan          = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_combustion_80130998[(u16)(Gp_StateC08.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            arg0->state = 1;
            mem->index  = Gp_StateC08.attachId % 10 - 1;
            padScriptSpawnVariableMotorRamp(D_combustion_80130980[mem->index].emitterFrames, 0xFF, 8);
            /* fallthrough */
        case 1:
            actorRenderComposeCoord(coord);
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                effectKillTask(mem, arg0);
                return;
            }
            mem->move.vy = mem->move.vy + D_combustion_80130980[mem->index].flameDropStep;
            mem->move.vz = mem->move.vz + D_combustion_80130980[mem->index].flameReachStep;
            spawned      = effectSpawn((EFFECT_COMBUSTION_FLAME | EFFECT_SPAWN_UNLIMITED), coord, (s32)(mem->age), &mem->move);
            if (spawned != NULL) {
                taskReparent(arg0, spawned->task);
            }
            if (D_combustion_80130980[mem->index].narrowFlameFrames < mem->age) {
                Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
                arg0->state        = 2;
                return;
            }
            return;
        case 2:
            actorRenderComposeCoord(coord);
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) ||
                (mem->age > D_combustion_80130980[mem->index].emitterFrames)) {
                effectKillTask(mem, arg0);
                return;
            }
            gfxRotMatrixY(&coord->coord, -(arg0->spawnArg1.value * 80), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}

/// One flame of the combustion burn. State 0 re-bases the effect coordinate on
/// the `EffectWork.parent` parent with an identity rotation and the work
/// block's `pos` offset, seeds the phase `age` from
/// `gRandomLcgState`, the radius `scale` from `spawnArg1` and the intensity
/// `index` from `Gp_StateC08.attachId % 10 - 1`, then splits: `spawnArg1`
/// past the `D_combustion_80130980` row's `narrowFlameFrames` runs the wide
/// state 2, anything smaller the narrow state 1. Both states redraw every frame -
/// `index < 2` picks the small draw helper, otherwise the large one - and
/// one frame in four spawn a trailing ember that adopts this task as its
/// parent. Either state releases the effect once the player is dying
/// (`Gp_StateC08.effectPhase`), parasite-energy effects are cancelled
/// (`gRoomEffectState->peEffectControl`) or the flame has lived
/// 0x21 frames.
void func_combustion_8012F2BC(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    EffectWork*       spawned;
    s32               rng;
    s32               spawnRng1;
    s32               spawnRng1b;
    s32               spawnRng2;
    s32               spawnRng2b;
    s32               last;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            rot           = (GfxRotationWords*)&coord->coord;
            coord->parent = mem->parent;
            rot->m00M01   = ONE;
            rot->m02M10   = 0;
            rot->m11M12   = ONE;
            rot->m20M21   = 0;
            rot->m22      = ONE;

            coord->coord.t[0]   = mem->pos.vx;
            coord->coord.t[1]   = mem->pos.vy;
            coord->coord.t[2]   = mem->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);

            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->age        = ((u32)rng >> 16) & 0xF;
            mem->scale      = arg0->spawnArg1.value * 32 + 512;
            mem->index      = Gp_StateC08.attachId % 10 - 1;
            last            = D_combustion_80130980[mem->index].narrowFlameFrames;
            gRandomLcgState = rng;
            if (last < arg0->spawnArg1.value) {
                arg0->state = 2;
                return;
            }
            arg0->state = 1;
            return;
        case 1:
            actorRenderComposeCoord(coord);
            if (mem->index < 2) {
                _combustionDrawSmallFlame(coord, mem->age, mem->scale);
            } else {
                _combustionDrawLargeFlame(coord, mem->age, mem->scale);
            }
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) || (mem->age >= 0x21)) {
                effectKillTask(mem, arg0);
                return;
            }
            spawnRng1       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = spawnRng1;
            if ((((u32)spawnRng1 >> 16) & 3) == 0) {
                spawnRng1b      = spawnRng1 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = spawnRng1b;
                spawned         = effectSpawn(EFFECT_COMBUSTION_EMBER, coord, ((u32)spawnRng1b >> 16) & 1, 0);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            return;
        case 2:
            actorRenderComposeCoord(coord);
            if (mem->index < 2) {
                spriteQuadDrawFlicker(coord, mem->age, mem->scale * 3 / 2, 0);
            } else {
                spriteQuadDrawFlicker(coord, mem->age, mem->scale * 4, 0);
            }
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) || (mem->age >= 0x21)) {
                effectKillTask(mem, arg0);
                return;
            }
            spawnRng2       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = spawnRng2;
            if ((((u32)spawnRng2 >> 16) & 3) == 0) {
                spawnRng2b      = spawnRng2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = spawnRng2b;
                spawned         = effectSpawn(EFFECT_COMBUSTION_EMBER, coord, ((u32)spawnRng2b >> 16) & 1, 0);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            return;
    }
}

/// Writes the screen corners of an axis-aligned square billboard.
///
/// Borrows a writable `quad` and read-only `scratch` with initialized
/// `screenX`, `screenY` and `screenExtent`. The extent is the signed pixel
/// half-size, already perspective-scaled by the caller. Only the packet's
/// X/Y fields change: vertices 0/1 form the minus-Y pair, 2/3 the plus-Y pair,
/// with even vertices on minus-X and odd vertices on plus-X.
///
/// Centre/extent arithmetic keeps the low 16 bits, interpreted as signed GPU
/// pixel coordinates; a negative extent reverses the corner pairs. Neither
/// pointer is retained, and ownership stays with the caller.
static inline void _combustionSetBillboardCorners(POLY_FT4* quad, const EffectCentreScratch* scratch)
{
    s16 cornerX;
    s16 cornerY;

    cornerX  = scratch->screenX - (u16)scratch->screenExtent;
    quad->x2 = cornerX;
    quad->x0 = cornerX;
    cornerX  = scratch->screenX + (u16)scratch->screenExtent;
    quad->x3 = cornerX;
    quad->x1 = cornerX;
    cornerY  = scratch->screenY - (u16)scratch->screenExtent;
    quad->y1 = cornerY;
    quad->y0 = cornerY;
    cornerY  = scratch->screenY + (u16)scratch->screenExtent;
    quad->y3 = cornerY;
    quad->y2 = cornerY;
}

/// Draws an additive square billboard from Combustion's six-cell small-flame strip.
///
/// `coord` must have a composed world transform; its translation is narrowed to
/// signed 16-bit world coordinates. Nonnegative `animationFrame` wraps modulo
/// six. `sizeFactor * 31 / (SZ3 / 4 + 1)` gives the pixel half-extent, truncated
/// toward zero. A negative GTE FLAG word discards the projection.
///
/// Requires an initialized scratch stack with one word-aligned
/// `EffectCentreScratch` block free, and primitive space for one `POLY_FT4`.
/// Scratch is released before return; a linked packet lives until GPU completion.
static void _combustionDrawSmallFlame(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor)
{
    // UV coordinates and the perspective numerator use the texel span of one cell.
    enum {
        COMBUSTION_SMALL_FLAME_CELL_WIDTH = 32,
        COMBUSTION_SMALL_FLAME_UV_SPAN    = COMBUSTION_SMALL_FLAME_CELL_WIDTH - 1,
        COMBUSTION_SMALL_FLAME_TOP_V      = 0x98,
        COMBUSTION_SMALL_FLAME_BOTTOM_V   = COMBUSTION_SMALL_FLAME_TOP_V + COMBUSTION_SMALL_FLAME_UV_SPAN,
    };
    EffectCentreScratch* scratchTop;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    s32                  leftU;
    s32                  rightU;
    u16                  worldZ;

    // Narrow the composed centre into the GTE vector and project it.
    scratchTop                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (scratchTop - 1)->worldPoint.vx           = (u16)coord->workm.t[0];
    scratch                                   = scratchTop - 1;
    scratch->worldPoint.vy                    = (u16)coord->workm.t[1];
    worldZ                                    = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZ;
    worldPoint                                = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchTop - 1)->screenX);
    gte_stflg(&(scratchTop - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchTop - 1)->depth);
        scratch->depth += COMBUSTION_SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 576, 0);
        quad->clut  = getClut(32, 266);
        quad->v0    = COMBUSTION_SMALL_FLAME_TOP_V;
        quad->v1    = COMBUSTION_SMALL_FLAME_TOP_V;
        quad->v2    = COMBUSTION_SMALL_FLAME_BOTTOM_V;
        quad->v3    = COMBUSTION_SMALL_FLAME_BOTTOM_V;
        leftU       = (s16)(animationFrame % COMBUSTION_SMALL_FLAME_FRAME_COUNT) * COMBUSTION_SMALL_FLAME_CELL_WIDTH;
        rightU      = leftU + COMBUSTION_SMALL_FLAME_UV_SPAN;
        quad->u1    = rightU;
        quad->u3    = rightU;
        quad->u0    = leftU;
        quad->u2    = leftU;
        // Size the axis-aligned quad in pixels using the biased projection depth.
        scratch->screenExtent = (sizeFactor * COMBUSTION_SMALL_FLAME_UV_SPAN) / scratch->depth;
        _combustionSetBillboardCorners(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Moves an ember in parent-space Y and refreshes its composed transform.
///
/// `riseStep` is a signed displacement in game coordinate units per task tick,
/// added to the local-to-parent translation, independently of the ember's
/// own rotation. Combustion chooses -randomByte - 64 * levelIndex, with
/// `levelIndex` in 0..2, giving -383..0. The translation sum must fit s32.
///
/// Borrows a writable task-owned `coord` and its live, acyclic parent chain.
/// Clears the composition stamp before recomposing, so the cached transform
/// is ready for drawing on return. Ownership stays with the task.
static inline void _combustionRiseEmber(GfxCoord* coord, s16 riseStep)
{
    s32 nextY;

    nextY               = coord->coord.t[1] + riseStep;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
}

void combustionEmberTask(Task* task)
{
    enum {
        COMBUSTION_EMBER_STATE_INIT        = 0,
        COMBUSTION_EMBER_STATE_PYRO_FLAME  = 1,
        COMBUSTION_EMBER_STATE_EMBER       = 2,
        COMBUSTION_EMBER_STATE_SMALL_FLAME = 3,
        COMBUSTION_EMBER_RANDOM_RISE_MASK  = 0xFF,
        COMBUSTION_EMBER_LEVEL_RISE_SHIFT  = 6,
        COMBUSTION_EMBER_LEVEL_SIZE_SHIFT  = 8,
        COMBUSTION_EMBER_BASE_SIZE_FACTOR  = 0x300,
        COMBUSTION_EMBER_LARGE_LEVEL_INDEX = 2,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         riseRng;
    s32         angleRng;
    s16         riseStep;
    s16         levelIndex;
    s16         animationFrame;
    s32         riseLevelIndex;
    s32         randomRiseStep;
    s32         sizeLevelIndex;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    switch (task->state) {
        case COMBUSTION_EMBER_STATE_INIT:
            // Retain the PE level, fixed orientation, rise speed and size for this ember.
            levelIndex      = (Gp_StateC08.attachId % 10U) - 1;
            riseRng         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            angleRng        = riseRng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = ((u32)angleRng >> 16) & (PYRO_FLAME_FULL_TURN - 1);
            randomRiseStep  = ((u32)riseRng >> 16) & COMBUSTION_EMBER_RANDOM_RISE_MASK;
            work->step      = levelIndex;
            gRandomLcgState = riseRng;
            riseLevelIndex  = work->step;
            work->move.vy   = -randomRiseStep - (riseLevelIndex << COMBUSTION_EMBER_LEVEL_RISE_SHIFT);
            gRandomLcgState = angleRng;
            task->state     = task->spawnArg1.value + COMBUSTION_EMBER_STATE_PYRO_FLAME;
            sizeLevelIndex  = work->step;
            work->angle     = (sizeLevelIndex << COMBUSTION_EMBER_LEVEL_SIZE_SHIFT) + COMBUSTION_EMBER_BASE_SIZE_FACTOR;
            if (work->step >= COMBUSTION_EMBER_LARGE_LEVEL_INDEX) {
                gRandomLcgState = angleRng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                task->state    += (gRandomLcgState >> 16) & 1;
            }
            // The first tick always uses pyro-flame frame zero, regardless of the selected state.
            /* fallthrough */
        case COMBUSTION_EMBER_STATE_PYRO_FLAME:
            riseStep = work->move.vy;
            _combustionRiseEmber(coord, riseStep);
            // Advance on even ages and flash the retained cell on odd ages.
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            animationFrame = work->index;
            if (animationFrame < PYRO_FLAME_FRAME_COUNT) {
                if (work->age & 1) {
                    _pyroFlameDrawSprite(coord, animationFrame, work->angle, work->scale);
                    return;
                }
            } else {
                effectKillTask(work, task);
                return;
            }
            break;
        case COMBUSTION_EMBER_STATE_EMBER:
            riseStep = work->move.vy;
            _combustionRiseEmber(coord, riseStep);
            animationFrame = work->index + 1;
            work->index    = animationFrame;
            if (animationFrame < COMBUSTION_EMBER_FRAME_COUNT) {
                _combustionDrawEmber(coord, animationFrame, work->angle);
                return;
            }
            effectKillTask(work, task);
            return;
        case COMBUSTION_EMBER_STATE_SMALL_FLAME:
            riseStep = work->move.vy;
            _combustionRiseEmber(coord, riseStep);
            animationFrame = work->index + 1;
            work->index    = animationFrame;
            if (animationFrame < COMBUSTION_SMALL_FLAME_FRAME_COUNT) {
                _combustionDrawSmallFlame(coord, animationFrame, work->angle);
                return;
            }
            effectKillTask(work, task);
            return;
    }
}

#include "../../shared/pyro_flame_draw_sprite.inc.c"

/// Draws an additive square billboard from Combustion's eight-cell ember strip.
///
/// Uses the composed world translation, narrowed to signed 16-bit coordinates.
/// The low three bits of `animationFrame` select the cell. The pixel half-extent
/// is `sizeFactor * 23 / (SZ3 / 4 + 1)`, truncated toward zero; a negative GTE
/// FLAG word discards the projection.
///
/// Requires and releases one word-aligned `EffectCentreScratch` block on an
/// initialized scratch stack. Primitive space must hold one `POLY_FT4` packet,
/// which remains live until GPU completion if linked into the ordering table.
static void _combustionDrawEmber(const GfxCoord* coord, s32 animationFrame, s16 sizeFactor)
{
    enum {
        COMBUSTION_EMBER_CELL_WIDTH = 24,
        COMBUSTION_EMBER_UV_SPAN    = COMBUSTION_EMBER_CELL_WIDTH - 1,
        COMBUSTION_EMBER_TOP_V      = 0xA0,
        COMBUSTION_EMBER_BOTTOM_V   = COMBUSTION_EMBER_TOP_V + COMBUSTION_EMBER_UV_SPAN,
    };
    EffectCentreScratch* scratchTop;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    s32                  leftU;
    u16                  worldZ;

    // Narrow the composed centre into the GTE vector and project it.
    scratchTop                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (scratchTop - 1)->worldPoint.vx           = (u16)coord->workm.t[0];
    scratch                                   = scratchTop - 1;
    scratch->worldPoint.vy                    = (u16)coord->workm.t[1];
    worldZ                                    = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZ;
    worldPoint                                = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchTop - 1)->screenX);
    gte_stflg(&(scratchTop - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchTop - 1)->depth);
        scratch->depth += COMBUSTION_SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
        quad->clut  = getClut(208, 268);
        leftU       = (animationFrame & (COMBUSTION_EMBER_FRAME_COUNT - 1)) * COMBUSTION_EMBER_CELL_WIDTH;
        setUV4(quad, leftU, COMBUSTION_EMBER_TOP_V, leftU + COMBUSTION_EMBER_UV_SPAN, COMBUSTION_EMBER_TOP_V, leftU, COMBUSTION_EMBER_BOTTOM_V, leftU + COMBUSTION_EMBER_UV_SPAN, COMBUSTION_EMBER_BOTTOM_V);
        // Size the axis-aligned quad in pixels using the biased projection depth.
        scratch->screenExtent = (sizeFactor * COMBUSTION_EMBER_UV_SPAN) / scratch->depth;
        _combustionSetBillboardCorners(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#define SPRITE_QUAD_SCALE 55
#define SPRITE_QUAD_ODD_LOOK(p)   \
    setRGB0(p, 0xC0, 0x70, 0x40); \
    SPRITE_QUAD_CORE_CELL(p);     \
    setSemiTrans(p, 1)
#define SPRITE_QUAD_EVEN_LOOK(p) \
    setcode(p, 0x2F);            \
    SPRITE_QUAD_RIM_CELL(p)
#include "../../shared/sprite_quad_draw_flicker.inc.c"

/// Draws an additive square billboard from Combustion's twelve-cell large-flame sheet.
///
/// Uses the composed world translation, narrowed to signed 16-bit coordinates.
/// Nonnegative `animationFrame` wraps modulo twelve over two rows of six cells;
/// each cell selects its own palette. The pixel half-extent is
/// `sizeFactor * 39 / (SZ3 / 4 + 1)`, truncated toward zero. A negative GTE FLAG
/// word discards the projection.
///
/// Requires and releases one word-aligned `EffectCentreScratch` block on an
/// initialized scratch stack. Primitive space must hold one `POLY_FT4` packet,
/// which remains live until GPU completion if linked into the ordering table.
static void _combustionDrawLargeFlame(const GfxCoord* coord, s16 animationFrame, s16 sizeFactor)
{
    enum {
        COMBUSTION_LARGE_FLAME_COLUMNS     = 6,
        COMBUSTION_LARGE_FLAME_FRAME_COUNT = 12,
        COMBUSTION_LARGE_FLAME_CELL_WIDTH  = 40,
        COMBUSTION_LARGE_FLAME_UV_SPAN     = COMBUSTION_LARGE_FLAME_CELL_WIDTH - 1,
        // Signed origins wrap to byte UVs starting at V=0x88.
        COMBUSTION_LARGE_FLAME_TOP_V    = -0x78,
        COMBUSTION_LARGE_FLAME_BOTTOM_V = COMBUSTION_LARGE_FLAME_TOP_V + COMBUSTION_LARGE_FLAME_UV_SPAN,
    };
    EffectCentreScratch* scratchTop;
    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    SVECTOR*             worldPoint;
    s32                  wrappedFrame;
    u32                  cellIndex;
    u16                  textureColumn;
    u16                  textureRow;
    s32                  leftU;
    s32                  rightU;
    s32                  topV;
    s32                  bottomV;
    u16                  worldZ;

    // Narrow the composed centre into the GTE vector and project it.
    scratchTop                                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    (scratchTop - 1)->worldPoint.vx           = (u16)coord->workm.t[0];
    scratch                                   = scratchTop - 1;
    scratch->worldPoint.vy                    = (u16)coord->workm.t[1];
    worldZ                                    = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratch;
    scratch->worldPoint.vz                    = worldZ;
    worldPoint                                = &scratch->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(worldPoint);
    gte_rtps();
    wrappedFrame = animationFrame;
    wrappedFrame = wrappedFrame % COMBUSTION_LARGE_FLAME_FRAME_COUNT;
    gte_stsxy(&(scratchTop - 1)->screenX);
    gte_stflg(&(scratchTop - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchTop - 1)->depth);
        scratch->depth += COMBUSTION_SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage   = getTPage(0, GPU_BLEND_ADD, 640, 0);
        cellIndex     = (u16)wrappedFrame;
        quad->clut    = getClut(cellIndex << 4, 268);
        textureColumn = cellIndex % COMBUSTION_LARGE_FLAME_COLUMNS;
        textureRow    = cellIndex / COMBUSTION_LARGE_FLAME_COLUMNS;
        leftU         = textureColumn * COMBUSTION_LARGE_FLAME_CELL_WIDTH;
        rightU        = leftU + COMBUSTION_LARGE_FLAME_UV_SPAN;
        topV          = textureRow * COMBUSTION_LARGE_FLAME_CELL_WIDTH + COMBUSTION_LARGE_FLAME_TOP_V;
        bottomV       = textureRow * COMBUSTION_LARGE_FLAME_CELL_WIDTH + COMBUSTION_LARGE_FLAME_BOTTOM_V;
        setUV4(quad, leftU, topV, rightU, topV, leftU, bottomV, rightU, bottomV);
        // Size the axis-aligned quad in pixels using the biased projection depth.
        scratch->screenExtent = (sizeFactor * COMBUSTION_LARGE_FLAME_UV_SPAN) / scratch->depth;
        _combustionSetBillboardCorners(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void func_combustion_801308E0(Task* arg0)
{
    GfxCoord* coord;

    if (arg0->state != 0) {
        effectKillTask(arg0->spawnArg2.pointer, arg0);
        return;
    }
    coord = arg0->extra.coordBody->coord;
    actorRenderComposeCoord(coord);
    effectSpawn((EFFECT_COMBUSTION_FLAME_EMITTER | EFFECT_SPAWN_UNLIMITED), coord, 1, 0);
    effectSpawn((EFFECT_COMBUSTION_FLAME_EMITTER | EFFECT_SPAWN_UNLIMITED), coord, -1, 0);
    arg0->state = arg0->state + 1;
}
