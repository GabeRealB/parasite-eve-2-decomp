#include "pe/metabolism.h"

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
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Visual tuning of the metabolism cast for one Parasite Energy level.
///
/// The cast is a fan of glow wedges, two rings and up to three arcs that
/// brighten and widen around the caster while it throws off sparks. It selects
/// its row with the level digit of the attachment id, less one, which it keeps
/// in `EffectWork::index`.
///
/// The brightness is the cast's `EffectWork::scale`: the green channel of the
/// drawing, which red and blue are shifted down from. The radius is its
/// `EffectWork::angle`: the extent of the fan and the arcs, twice the radius
/// of the rings, and the distance from the caster each spark is spawned at.
typedef struct {
    s16 wedgeCount;      // Glow wedges in the fan, one random angle each; the angle table holds 16
    s16 brightnessLimit; // Brightness the growth stops at; below it the cast gains 0x10 a frame
    s16 radiusStep;      // Radius gained per frame, while the cast grows and while it fades
    s16 radiusLimit;     // Radius that ends the growth; also the sprite radius each spark is spawned with
} _MetabolismLevelTuning;
STATIC_ASSERT_SIZEOF(_MetabolismLevelTuning, 8);

/// Per-level tuning for the metabolism drain, one row per PE level 1-3,
/// weakest first.
static _MetabolismLevelTuning D_metabolism_8012FB54[] = {
    { 0x0008, 0x0080, 0x0020, 0x0400 },
    { 0x000C, 0x00B0, 0x0030, 0x0500 },
    { 0x0010, 0x00E0, 0x0040, 0x0600 },
};

/// The `sndEvtRequestScriptStart` id for each `D_metabolism_8012FB54` row.
static s32 D_metabolism_8012FB6C[] = { 0xE01F0001, 0xE0220001, 0xE0250001 };

/// Scratch angles for the fan, one per wedge: `(i << 10)` plus a 10-bit
/// random offset, seeded by state 0 and drawn by `_metabolismDrawFanWedge`.
static s16 D_metabolism_8012FB78[16];

static void _metabolismDrawFanWedge(const GfxCoord* coord, s16 radius, s16 bearing, s16 brightness);

/// Draws the metabolism cast's doubled disc and concentric outer glow bands.
///
/// Borrows a composed centre coordinate and the cast's work for this call.
/// `angle` is the radius in game-coordinate units, `scale` is green brightness
/// (red / 4, blue / 2), `age` selects the extra band on odd ticks, and a
/// nonzero `index` selects the additional outer band for PE levels 2 and 3.
/// RGB arithmetic narrows to bytes after each operation. The drawers borrow
/// scratch storage and append packets to the unchecked frame primitive arena.
static inline void _metabolismDrawGlow(const GfxCoord* coord, const EffectWork* castWork)
{
    enum {
        METABOLISM_GLOW_BAND_WIDTH        = 0x80,
        METABOLISM_GLOW_OUTER_BAND_OFFSET = 0x200,
    };
    u8 rgb[3];

    rgb[0] = castWork->scale >> 2;
    rgb[1] = castWork->scale;
    rgb[2] = castWork->scale >> 1;
    effectDrawGouraudDisc(coord, castWork->angle >> 1, rgb);
    effectDrawGouraudDisc(coord, castWork->angle >> 1, rgb);
    rgb[0] >>= 1;
    rgb[1] >>= 1;
    rgb[2] >>= 1;
    effectDrawOuterGlowBand(coord, castWork->angle, METABOLISM_GLOW_BAND_WIDTH, rgb);
    if (castWork->age & 1) {
        rgb[1] >>= 1;
        rgb[2] <<= 1;
        effectDrawOuterGlowBand(coord, METABOLISM_GLOW_BAND_WIDTH, castWork->angle, rgb);
    }
    if (castWork->index != 0) {
        rgb[0] >>= 1;
        rgb[1] >>= 1;
        rgb[2] >>= 1;
        effectDrawOuterGlowBand(coord, (s16)(castWork->angle + METABOLISM_GLOW_OUTER_BAND_OFFSET), METABOLISM_GLOW_BAND_WIDTH, rgb);
    }
}

/// Runs one frame of the metabolism cast. Cancel (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD`
/// or `gRoomEffectState->peEffectControl >= 4`) releases the work block. State 0 parents the
/// coordinate to the player with an identity rotation lifted 0x400 above it,
/// picks the intensity row from the combo counter, seeds one random angle per
/// fan wedge into `D_metabolism_8012FB78`, and plays the combo-indexed cue.
/// State 1 grows brightness and radius, and each frame spins the coordinate to
/// three random yaws, rotating `EffectWork.move` through the new frame and
/// then overwriting it with the `angle` circle at `step`, to parent
/// three `0x60013` sparks; it hands over to state 2 once the radius reaches
/// the row's `radiusLimit`. State 2 shrinks brightness by 0x10 a frame and drops
/// to state 3 - release - below 0x11. States 1 and 2 both draw the fan wedges,
/// two rings and two or three arcs, each arc on a colour halved again from the
/// last.
void func_metabolism_8012EF34(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    EffectWork*       spawned;
    s32               pan;
    s32               bright;
    s32               i;
    s32               temp_lo;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(mem, arg0);
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            rot                 = (GfxRotationWords*)&coord->coord;
            coord->parent       = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            rot->m00M01         = ONE;
            rot->m02M10         = 0;
            rot->m11M12         = ONE;
            rot->m20M21         = 0;
            rot->m22            = ONE;
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = -0x400;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            arg0->state = 1;
            mem->index  = (Gp_StateC08.attachId % 10) - 1;
            mem->angle  = 0x80;
            {
                s32 rng;

                for (i = 0; i < D_metabolism_8012FB54[mem->index].wedgeCount; i++) {
                    rng                      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_metabolism_8012FB78[i] = (i << 10) + (((u32)rng >> 16) & 0x3FF);
                    gRandomLcgState          = rng;
                }
            }
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            pan                = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_metabolism_8012FB6C[mem->index], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            /* fallthrough */
        case 1:
            actorRenderComposeCoord(coord);
            bright = mem->scale;
            if (bright < D_metabolism_8012FB54[mem->index].brightnessLimit) {
                bright += 0x10;
            }
            mem->scale = bright;
            mem->angle = mem->angle + D_metabolism_8012FB54[mem->index].radiusStep;
            {
                s32 rng;
                s32 rng2;

                for (i = 0; i < 3; i++) {
                    rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    rng2            = rng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = rng;
                    mem->step       = ((u32)rng >> 16) & 0xFFF;
                    gRandomLcgState = rng2;
                    gfxRotMatrixY(&coord->coord, ((u32)rng2 >> 16) & 0xFFF, 0);
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&mem->move);
                    gte_rtv0();
                    gte_stsv(&mem->move);
                    mem->move.vx = (rcos(mem->step) * mem->angle) >> 12;
                    temp_lo      = rsin(mem->step) * mem->angle;
                    mem->move.vz = 0;
                    mem->move.vy = temp_lo >> 12;
                    spawned      = Gp_SpawnEff(EFFECT_METABOLISM_SPARKLE, coord,
                                               (s32)D_metabolism_8012FB54[mem->index].radiusLimit,
                                               &mem->move);
                    if (spawned != NULL) {
                        taskReparent(arg0, spawned->task);
                    }
                }
            }
            if (mem->angle >= D_metabolism_8012FB54[mem->index].radiusLimit) {
                arg0->state = 2;
            }
            for (i = 0; i < D_metabolism_8012FB54[mem->index].wedgeCount; i++) {
                _metabolismDrawFanWedge(coord, mem->angle, D_metabolism_8012FB78[i],
                                        mem->scale);
            }
            _metabolismDrawGlow(coord, mem);
            return;
        case 2:
            actorRenderComposeCoord(coord);
            for (i = 0; i < D_metabolism_8012FB54[mem->index].wedgeCount; i++) {
                _metabolismDrawFanWedge(coord, mem->angle, D_metabolism_8012FB78[i],
                                        mem->scale);
            }
            mem->scale = mem->scale - 0x10;
            mem->angle = mem->angle + D_metabolism_8012FB54[mem->index].radiusStep;
            if (mem->scale < 0x11) {
                arg0->state = 3;
            }
            _metabolismDrawGlow(coord, mem);
            return;
        case 3:
            effectKillTask(mem, arg0);
            return;
    }
}

/// Moves the sparkle along its parent's Y axis and refreshes its cached transform.
///
/// `deltaY` is a signed displacement in parent-coordinate units; the sum must
/// fit s32. The writable coordinate and its parent chain must remain live.
static inline void _metabolismMoveSparkleCoord(GfxCoord* coord, s16 deltaY)
{
    s32 nextY;

    nextY               = coord->coord.t[1] + deltaY;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
}

void metabolismSparkleTask(Task* task)
{
    enum {
        METABOLISM_SPARKLE_STATE_INITIALIZE   = 0,
        METABOLISM_SPARKLE_STATE_SPINNING     = 1,
        METABOLISM_SPARKLE_STATE_FADING       = 2,
        METABOLISM_SPARKLE_Y_STEP             = 8,
        METABOLISM_SPARKLE_SIZE_MASK          = 0xFFF,
        METABOLISM_SPARKLE_ROTATION_MASK      = 0xFFF,
        METABOLISM_SPARKLE_PALETTE_SHIFT      = 12,
        METABOLISM_SPARKLE_SPINNING_PALETTE   = 1,
        METABOLISM_SPARKLE_FADING_PALETTE     = 3,
        METABOLISM_SPARKLE_INITIAL_BRIGHTNESS = 0xC0,
        METABOLISM_SPARKLE_BRIGHTNESS_STEP    = 0x18,
        METABOLISM_SPARKLE_FRAME_COUNT        = 8,
        METABOLISM_SPARKLE_FADING_ROLL_COUNT  = 3,
        METABOLISM_SPARKLE_ALWAYS_SPIN_LEVELS = 2,
    };
    EffectWork* work;
    GfxCoord*   coord;
    u16         peLevel;
    u16         variantRoll;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    switch (task->state) {
        case METABOLISM_SPARKLE_STATE_INITIALIZE:
            work->move.vx = 0;
            work->move.vy = METABOLISM_SPARKLE_Y_STEP;
            work->move.vz = 0;
            work->angle   = task->spawnArg1.value & METABOLISM_SPARKLE_SIZE_MASK;
            // Only PE level 3 can select the fading variant; lower levels skip that random draw.
            peLevel = Gp_StateC08.attachId % 10U;
            if (peLevel - 1 < METABOLISM_SPARKLE_ALWAYS_SPIN_LEVELS ||
                (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT,
                 variantRoll     = (gRandomLcgState >> 16) % (u32)METABOLISM_SPARKLE_FADING_ROLL_COUNT, variantRoll != 0)) {
                task->state     = METABOLISM_SPARKLE_STATE_SPINNING;
                work->period    = METABOLISM_SPARKLE_SPINNING_PALETTE << METABOLISM_SPARKLE_PALETTE_SHIFT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = (gRandomLcgState >> 16) & METABOLISM_SPARKLE_ROTATION_MASK;
            } else {
                task->state  = METABOLISM_SPARKLE_STATE_FADING;
                work->scale  = METABOLISM_SPARKLE_INITIAL_BRIGHTNESS;
                work->period = METABOLISM_SPARKLE_FADING_PALETTE << METABOLISM_SPARKLE_PALETTE_SHIFT;
            }
            return;
        case METABOLISM_SPARKLE_STATE_SPINNING:
            _metabolismMoveSparkleCoord(coord, work->move.vy);
            // Advance on even ages, draw on odd ages, and retire at frame eight.
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            if (work->index < METABOLISM_SPARKLE_FRAME_COUNT) {
                if (work->age & 1) {
                    effectDrawSpinningBillboard(coord, work->index, work->angle,
                                                work->scale | work->period);
                    return;
                }
            } else {
                effectKillTask(work, task);
                return;
            }
            break;
        case METABOLISM_SPARKLE_STATE_FADING:
            _metabolismMoveSparkleCoord(coord, work->move.vy);
            if (!(work->age & 1)) {
                work->index = work->index + 1;
            }
            if (work->index < METABOLISM_SPARKLE_FRAME_COUNT) {
                if (work->age & 1) {
                    effectDrawModulatedBillboard(coord, work->index, work->angle,
                                                 work->scale | work->period);
                    work->scale = work->scale - METABOLISM_SPARKLE_BRIGHTNESS_STEP;
                    return;
                }
            } else {
                effectKillTask(work, task);
                return;
            }
            break;
    }
}

/// Reserves and projects the centre of a metabolism fan wedge.
///
/// `cursorSlot` points at the initialized scratch-stack cursor. Returns one
/// live complete block; the caller releases it through the same cursor slot.
/// Borrows the composed coordinate and `GsWSMATRIX`, and clobbers GTE registers.
static inline EffectCentreScratch* _metabolismProjectFanCentre(const GfxCoord* coord, void** cursorSlot)
{
    EffectCentreScratch* scratch;

    // Project the composed centre once, narrowing its translation to signed halfwords.
    scratch                = SCRATCH_PUSH_AT(cursorSlot, EffectCentreScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    return scratch;
}

/// Draws one additive Gouraud wedge of the metabolism cast's screen-space fan.
///
/// `coord` supplies a composed translation in the input space of `GsWSMATRIX`;
/// its rotation is unused and XYZ narrows to signed 16-bit coordinate units.
/// The rim radius in pixels is `radius * 128 / (SZ3 / 4 + 1)`, with signed
/// arithmetic and division toward zero. `bearing` uses 4096 units per turn,
/// counterclockwise from screen-down; the rim corners are at bearing +/- 32.
/// `brightness` supplies green (0..255), with red / 2 and blue alternating
/// between full and half brightness on display-frame parity. The rim is black.
/// A negative GTE FLAG rejects the triangle. Reserves/releases one complete
/// scratch block and appends a triangle/blend-command pair to the unchecked
/// frame arena; inputs must stay clear of both, and packets live through GPU drawing.
static void _metabolismDrawFanWedge(const GfxCoord* coord, s16 radius, s16 bearing, s16 brightness)
{
    enum {
        METABOLISM_FAN_PERSPECTIVE_SCALE  = 128,
        METABOLISM_FAN_HALF_ANGLE         = 0x20,
        METABOLISM_FAN_TRIG_FRACTION_BITS = 12,
    };
    void**               cursorSlot;
    EffectCentreScratch* scratch;
    POLY_G3*             triangle;
    s32                  rightRimAngle;
    s32                  leftRimAngle;

    cursorSlot = SCRATCH_HEAD_ADDR;
    scratch    = _metabolismProjectFanCentre(coord, cursorSlot);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth++;
        triangle       = gGpuPrimCursor;
        gGpuPrimCursor = triangle + 1;
        setPolyG3(triangle);
        // Fade from the coloured centre to a black rim spanning 64 angle units.
        setRGB0(triangle, brightness >> 1, brightness, brightness >> (gDisplayState.animFrame & 1));
        setRGB1(triangle, 0, 0, 0);
        setRGB2(triangle, 0, 0, 0);
        scratch->screenExtent = (radius * METABOLISM_FAN_PERSPECTIVE_SCALE) / scratch->depth;
        rightRimAngle         = bearing;
        leftRimAngle          = rightRimAngle - METABOLISM_FAN_HALF_ANGLE;
        triangle->x0          = scratch->screenX;
        triangle->y0          = scratch->screenY;
        triangle->x1          = scratch->screenX + ((scratch->screenExtent * rsin(leftRimAngle)) >> METABOLISM_FAN_TRIG_FRACTION_BITS);
        triangle->y1          = scratch->screenY + ((scratch->screenExtent * rcos(leftRimAngle)) >> METABOLISM_FAN_TRIG_FRACTION_BITS);
        rightRimAngle        += METABOLISM_FAN_HALF_ANGLE;
        triangle->x2          = scratch->screenX + ((scratch->screenExtent * rsin(rightRimAngle)) >> METABOLISM_FAN_TRIG_FRACTION_BITS);
        triangle->y2          = scratch->screenY + ((scratch->screenExtent * rcos(rightRimAngle)) >> METABOLISM_FAN_TRIG_FRACTION_BITS);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                triangle);
        gpuSetPrimitiveBlendMode(triangle, GPU_BLEND_ADD, scratch->depth);
    }
    SCRATCH_POP_AT(cursorSlot, EffectCentreScratch);
}
