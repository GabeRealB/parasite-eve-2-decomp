#include "pe/inferno.h"

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
#include "main/mem.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Vertices on each rim of one inferno fan band.
///
/// The texture row has one cell per vertex, so a segment index and its frame
/// wrap with this count. `INFERNO_FAN_SEGMENT_YAW` is the angle between
/// adjacent vertices, in the frame's 4096-per-turn units. Six steps close the
/// band four units short of a full turn.
#define INFERNO_FAN_SEGMENT_COUNT 6
#define INFERNO_FAN_SEGMENT_YAW   0x2AA

/// The two bands of one inferno fan, in shape-table and texture-phase order.
///
/// `INFERNO_FAN_RISING_BAND` is lifted by `EffectWork::period` plus the
/// shape's lift and has base radius 0x100. `INFERNO_FAN_CONSTANT_LIFT_BAND`
/// uses the shape's lift alone and has base radius 0x200.
/// `INFERNO_FAN_BAND_COUNT` is how many of those columns the fan has.
#define INFERNO_FAN_RISING_BAND        0
#define INFERNO_FAN_CONSTANT_LIFT_BAND 1
#define INFERNO_FAN_BAND_COUNT         2

/// Per-segment texture-cell phase for both bands of one inferno fan.
///
/// The fan task allocates one as its `Task::work`. Each byte is the high
/// half of an LCG draw. A drawer adds `EffectWork::age` and reduces modulo
/// `INFERNO_FAN_SEGMENT_COUNT`; the residue selects that segment's cell on
/// the six-cell texture row.
///
/// `band` names the two columns. `byBand` is those same bytes in band-major
/// order, addressed by the drawer's `bandIndex`.
/// `byte` is that order flattened, so the fill can write segment `i` of both
/// columns from one pointer and stay inside one array. `byte[i]` is segment
/// `i` of `risingBand`; `byte[i + INFERNO_FAN_SEGMENT_COUNT]` is that segment
/// of `constantLiftBand`.
typedef union {
    struct {
        u8 risingBand[INFERNO_FAN_SEGMENT_COUNT];                 // Cell phase of the band whose lift is period + lift
        u8 constantLiftBand[INFERNO_FAN_SEGMENT_COUNT];           // Cell phase of the band whose lift is the shape's lift alone
    } band;
    u8 byBand[INFERNO_FAN_BAND_COUNT][INFERNO_FAN_SEGMENT_COUNT]; // [0] risingBand, [1] constantLiftBand
    u8 byte[INFERNO_FAN_BAND_COUNT * INFERNO_FAN_SEGMENT_COUNT];  // risingBand, then constantLiftBand
} _InfernoFanTexturePhase;
STATIC_ASSERT_SIZEOF(_InfernoFanTexturePhase, 0xC);

/// Scratch-stack workspace for one band of the inferno ground fan.
///
/// A drawer places one vertex of each rim at every `INFERNO_FAN_SEGMENT_YAW`
/// in the effect coordinate's local frame, rotates it by that coordinate's
/// `workm` and adds its translation. `topRing` is the wider rim, displaced by
/// the band's lift along local -Y. `bottomRing` is the narrower rim and stays
/// in the local XZ plane. Quad `i` takes vertices 0 and 1 from `topRing[i]`
/// and `topRing[i + 1]`, and vertices 2 and 3 from `bottomRing[i]` and
/// `bottomRing[i + 1]`.
///
/// `sxy0`..`sxy3` are those vertices' screen positions as packed words. X is
/// the low half and Y is the arithmetic shift of the high half, so the words
/// stay signed. Ordering depth and the GTE flag are the drawer's locals, and
/// pointers into the block end at its release.
typedef struct {
    SVECTOR topRing[INFERNO_FAN_SEGMENT_COUNT];    // Wider rim, lifted along local -Y; quad vertices 0 and 1
    SVECTOR bottomRing[INFERNO_FAN_SEGMENT_COUNT]; // Narrower rim in the local XZ plane; quad vertices 2 and 3
    s32     sxy0;                                  // Packed screen position of the current quad's vertex 0
    s32     sxy1;                                  // Packed screen position of vertex 1
    s32     sxy2;                                  // Packed screen position of vertex 2
    s32     sxy3;                                  // Packed screen position of vertex 3
} _InfernoFanScratch;
STATIC_ASSERT_SIZEOF(_InfernoFanScratch, 0x70);

/// The two fan shapes the inferno wall sweeps through.
static EffectBandShape D_inferno_801304E4[] = {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
};

/// The `sndEvtRequestScriptStart` id the inferno cast plays, indexed by
/// the cast's level, `Gp_StateC08.attachId % 10 - 1`.
/// The same index also picks the state `func_inferno_8012EF88` advances to,
/// which is why the three ids and the three state chains run in step.
static s32 D_inferno_801304F0[] = { 0xE0100001, 0xE0130001, 0xE00D0001 };

/// Q12 trigonometry and the six 40-by-40 cells of the fan's additive 4-bit texture.
enum {
    INFERNO_FAN_TRIG_FRACTION_BITS = 12,
    INFERNO_FAN_TEXTURE_DEPTH_4BIT = 0,
    INFERNO_FAN_TEXTURE_PAGE_X     = 640, // VRAM words
    INFERNO_FAN_TEXTURE_PAGE_Y     = 0,   // VRAM rows
    INFERNO_FAN_PALETTE_X          = 32,  // VRAM words
    INFERNO_FAN_PALETTE_Y          = 266, // VRAM rows
    INFERNO_FAN_TEXTURE_CELL_SIZE  = 40,  // Texels per cell edge
    INFERNO_FAN_TEXTURE_V          = 96   // First row within the texture page
};

static void _infernoDrawScreenWash(s16 intensity);
static void _infernoDrawRisingFanBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex, const _InfernoFanTexturePhase* texturePhase);
static void _infernoDrawConstantLiftFanBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex, const _InfernoFanTexturePhase* texturePhase);

/// Builds both six-vertex fan rims in the caller's live scratch block.
///
/// Expands to multiple statements; use only unconditionally in a drawer body.
/// Arguments must be side-effect-free values and are read repeatedly:
/// `scratch` is an `_InfernoFanScratch*`, `coord` is a composed `GfxCoord*`,
/// radii are signed 16-bit coordinate distances, and lift is the unsigned
/// 16-bit displacement along local -Y. Captures the caller's `s32`
/// `segmentIndex` and `yaw`, its `SVECTOR* bottomVertex`, and `GsWSMATRIX`.
#define INFERNO_BUILD_FAN_RIMS(scratch, coord, baseRadius, liftedRadius, lift)                                                      \
    gte_SetTransMatrix(&GsWSMATRIX);                                                                                                \
    /* Wider lifted rim, then the narrower rim in the local XZ plane. */                                                            \
    for (segmentIndex = 0; segmentIndex < INFERNO_FAN_SEGMENT_COUNT; segmentIndex++) {                                              \
        yaw                                 = segmentIndex * INFERNO_FAN_SEGMENT_YAW;                                               \
        (scratch)->topRing[segmentIndex].vx = (rsin(yaw) * (liftedRadius)) >> INFERNO_FAN_TRIG_FRACTION_BITS;                       \
        (scratch)->topRing[segmentIndex].vy = -(lift);                                                                              \
        (scratch)->topRing[segmentIndex].vz = (rcos(yaw) * (liftedRadius)) >> INFERNO_FAN_TRIG_FRACTION_BITS;                       \
        gte_SetRotMatrix(&(coord)->workm);                                                                                          \
        gte_ldv0(&(scratch)->topRing[segmentIndex]);                                                                                \
        gte_rtv0();                                                                                                                 \
        gte_stsv(&(scratch)->topRing[segmentIndex]);                                                                                \
        (scratch)->topRing[segmentIndex].vx    = (u16)(scratch)->topRing[segmentIndex].vx + (u16)(coord)->workm.t[0];               \
        (scratch)->topRing[segmentIndex].vy    = (u16)(scratch)->topRing[segmentIndex].vy + (u16)(coord)->workm.t[1];               \
        (scratch)->topRing[segmentIndex].vz    = (u16)(scratch)->topRing[segmentIndex].vz + (u16)(coord)->workm.t[2];               \
        (scratch)->bottomRing[segmentIndex].vx = (rsin(yaw) * (baseRadius)) >> INFERNO_FAN_TRIG_FRACTION_BITS;                      \
        /* Address the lower rim within the full workspace byte extent. */                                                          \
        bottomVertex     = (SVECTOR*)((u8*)(scratch) + segmentIndex * sizeof(SVECTOR) + OFFSET_OF(_InfernoFanScratch, bottomRing)); \
        bottomVertex->vy = 0;                                                                                                       \
        bottomVertex->vz = (rcos(yaw) * (baseRadius)) >> INFERNO_FAN_TRIG_FRACTION_BITS;                                            \
        gte_SetRotMatrix(&(coord)->workm);                                                                                          \
        gte_ldv0(&(scratch)->bottomRing[segmentIndex]);                                                                             \
        gte_rtv0();                                                                                                                 \
        gte_stsv(&(scratch)->bottomRing[segmentIndex]);                                                                             \
        (scratch)->bottomRing[segmentIndex].vx = (u16)(scratch)->bottomRing[segmentIndex].vx + (u16)(coord)->workm.t[0];            \
        bottomVertex->vy                       = (u16)bottomVertex->vy + (u16)(coord)->workm.t[1];                                  \
        bottomVertex->vz                       = (u16)bottomVertex->vz + (u16)(coord)->workm.t[2];                                  \
    }

/// Runs one frame of the inferno cast: a state machine driven by
/// `Task::state`, with the chain it takes chosen in state 0 from
/// `Gp_StateC08.attachId % 10 - 1` (the combo counter), which also picks the
/// roar from `D_inferno_801304F0` and lands the task on state 1, 5 or 9.
/// State 1 spawns the two ignition effects, state 5 fans six flames around a
/// 0x400 step, state 9 the ground burst; states 10 and 11 fade the effect
/// brightness scalar (`EffectWork::angle`) down and back up and each fire one ring of
/// flames on their own tick, and state 12 fades out and releases. Every state
/// updates the effect coordinate first, and any state releases immediately if
/// the player is dying (`Gp_StateC08.effectPhase`) or parasite-energy effects are
/// cancelled (`gRoomEffectState->peEffectControl`).
void func_inferno_8012EF88(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         i;
    s32         pan;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(mem, arg0);
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->scale = 0x200;
            mem->angle = 0xFF;
            pan        = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_inferno_801304F0[(u16)(Gp_StateC08.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            arg0->state = ((u16)(Gp_StateC08.attachId % 10) - 1) * 4 + 1;
            return;
        case 1:
            Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 3, NULL);
            Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 5, NULL);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            Gp_SpawnPadLerp(0x10, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 5:
            i = 0x200;
            _infernoDrawScreenWash(mem->angle);
            Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 3, NULL);
            mem->scale = 0x600;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x400;
                Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 4, &mem->move);
            } while (i < 0x1200);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            Gp_SpawnPadLerp(0x14, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 9:
            Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 0, NULL);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xA;
            return;
        case 10:
            _infernoDrawScreenWash(mem->angle);
            mem->angle = mem->angle - 0x10;
            if (mem->age != 0xC) {
                return;
            }
            mem->scale = 0x600;
            i          = 0x155;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x2AA;
                Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 1, &mem->move);
            } while (i < 0x1151);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xB;
            return;
        case 11:
            _infernoDrawScreenWash(mem->angle);
            if (mem->angle < 0xF0) {
                mem->angle = mem->angle + 0x10;
            }
            if (mem->age != 0x18) {
                return;
            }
            mem->scale = 0x900;
            i          = 0;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x2AA;
                Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 2, &mem->move);
            } while (i < 0xFFC);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            Gp_SpawnPadLerp(0x18, 0xFF, 8);
            arg0->state = 0xC;
            mem->angle  = 0xFF;
            return;
        case 12:
            _infernoDrawScreenWash(mem->angle);
            if (mem->angle >= 9) {
                mem->angle = mem->angle - 8;
                return;
            }
            break;
        default:
            return;
    }
    effectKillTask(mem, arg0);
}

/// Draws the Inferno cast's additive red-amber wash over the 320-by-240 view.
///
/// `intensity` is the red channel (callers use 0..255); green and blue are
/// its arithmetic right shifts by one and two, with channels stored as bytes.
/// Applies the display's vertical offset and queues the blend command before
/// the quad at the same sorting depth. Requires frame-arena space for one
/// `POLY_F4` and one `DR_TPAGE`; their storage lives until GPU drawing completes.
static void _infernoDrawScreenWash(s16 intensity)
{
    enum {
        INFERNO_SCREEN_HALF_WIDTH         = 160, // Pixels from the centred screen origin
        INFERNO_SCREEN_HALF_HEIGHT        = 120,
        INFERNO_SCREEN_WASH_SORTING_DEPTH = 0x30 // Unscaled depth passed to both OT insertions
    };
    POLY_F4*      quad;
    DisplayState* display;
    s32           left;
    s32           right;
    s32           top;
    s32           bottom;
    s32           sortingDepth;

    display      = &gDisplayState;
    left         = -INFERNO_SCREEN_HALF_WIDTH;
    right        = INFERNO_SCREEN_HALF_WIDTH;
    top          = -INFERNO_SCREEN_HALF_HEIGHT;
    bottom       = INFERNO_SCREEN_HALF_HEIGHT;
    sortingDepth = INFERNO_SCREEN_WASH_SORTING_DEPTH;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setRGB0(quad, intensity, intensity >> 1, intensity >> 2);
    quad->x0 = left;
    quad->y0 = top - display->vramYOffset;
    quad->x1 = right;
    quad->y1 = top - display->vramYOffset;
    quad->x2 = left;
    quad->y2 = bottom - display->vramYOffset;
    quad->x3 = right;
    quad->y3 = bottom - display->vramYOffset;
    // Prepending the draw-mode packet makes additive blending active before the wash.
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sortingDepth << display->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, sortingDepth);
}

/// Fades and expands both Inferno fan bands for one frame, then draws them.
///
/// `work->scale` is RGB intensity; the caller must keep it in 0..255 after
/// subtracting `brightnessDecay`, since there is no clamping. `radiusGrowth`
/// expands both rims of both bands. `risingLiftGrowth` raises only the rising
/// band's upper rim along local -Y; `upperRimSpreadGrowth` further expands
/// both upper rims. Growth arguments are distances per frame in the effect's
/// coordinate units. All four updates use signed 32-bit arithmetic and narrow
/// to the work's signed halfwords before either band is drawn.
///
/// Borrows live work and initialized phases for both bands, using the existing
/// composed `coord->workm`. The caller advances the nonnegative `work->age`
/// used for texture animation. Draws the rising band before the fixed-lift
/// band, needing frame-arena capacity for up to twelve `POLY_FT4` packets and
/// aligned scratch-stack space for one `_InfernoFanScratch` at a time. No input
/// pointers are retained; queued packets live until GPU drawing completes.
static inline void _infernoFadeAndDrawFanBands(EffectWork* work, const GfxCoord* coord, const _InfernoFanTexturePhase* texturePhase,
                                               s32 brightnessDecay, s32 radiusGrowth, s32 risingLiftGrowth, s32 upperRimSpreadGrowth)
{
    work->scale  = work->scale - brightnessDecay;
    work->angle  = work->angle + radiusGrowth;
    work->period = work->period + risingLiftGrowth;
    work->step   = work->step + upperRimSpreadGrowth;
    _infernoDrawRisingFanBand(work, coord, INFERNO_FAN_RISING_BAND, texturePhase);
    _infernoDrawConstantLiftFanBand(work, coord, INFERNO_FAN_CONSTANT_LIFT_BAND, texturePhase);
}

void infernoFlameFanTask(Task* task)
{
    enum {
        INFERNO_FAN_STATE_INITIALIZE          = 0,
        INFERNO_FAN_STATE_GROW_THEN_FADE      = 1,
        INFERNO_FAN_STATE_STATIONARY_BURST    = 2,
        INFERNO_FAN_STATE_DRIFTING_BURST      = 3,
        INFERNO_FAN_STATE_WIDE_IGNITION       = 4,
        INFERNO_FAN_STATE_SHALLOW_RING        = 5,
        INFERNO_FAN_STATE_FAST_IGNITION       = 6,
        INFERNO_FAN_INITIAL_BRIGHTNESS        = 0x80,
        INFERNO_FAN_RISING_LIFT_LIMIT         = 0xC00,
        INFERNO_FAN_DRIFT_SCALE_Q12           = 0x80,
        INFERNO_FAN_SLOW_BRIGHTNESS_DECAY     = 4,
        INFERNO_FAN_BURST_BRIGHTNESS_DECAY    = 8,
        INFERNO_FAN_IGNITION_BRIGHTNESS_DECAY = 6,
        INFERNO_FAN_RANDOM_PHASE_SHIFT        = 16
    };
    EffectWork*              work;
    GfxCoord*                coord;
    _InfernoFanTexturePhase* texturePhase;
    u8*                      segmentPhases;
    s32                      segmentIndex;
    u32                      randomDraw;

    texturePhase = task->work;
    work         = task->spawnArg2.pointer;
    coord        = task->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(work, task);
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    work->age = work->age + 1;
    switch (task->state) {
        case INFERNO_FAN_STATE_INITIALIZE:
            texturePhase = memCalloc(sizeof(*texturePhase), false);
            if (texturePhase == NULL) {
                work->age = 0;
                return;
            }
            task->work  = texturePhase;
            work->scale = INFERNO_FAN_INITIAL_BRIGHTNESS;
            // Seed each segment in both bands, preserving the interleaved random draws.
            segmentIndex = 0;
            do {
                segmentPhases    = &texturePhase->byte[segmentIndex];
                randomDraw       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = randomDraw;
                segmentPhases[0] = randomDraw >> INFERNO_FAN_RANDOM_PHASE_SHIFT;
                segmentIndex++;
                randomDraw                               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState                          = randomDraw;
                segmentPhases[INFERNO_FAN_SEGMENT_COUNT] = randomDraw >> INFERNO_FAN_RANDOM_PHASE_SHIFT;
            } while (segmentIndex < INFERNO_FAN_SEGMENT_COUNT);
            task->state = task->spawnArg1.value + INFERNO_FAN_STATE_GROW_THEN_FADE;
            // Cache a 1/32-offset step rotated into the coordinate's parent frame.
            gte_lddp(INFERNO_FAN_DRIFT_SCALE_Q12);
            gte_ldsv(&work->pos);
            gte_gpf12();
            gte_stsv(&work->move);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            return;
        case INFERNO_FAN_STATE_GROW_THEN_FADE:
            // The centered fan reaches its lift limit before brightness starts fading.
            if (work->scale >= INFERNO_FAN_SLOW_BRIGHTNESS_DECAY + 1) {
                if (work->period < INFERNO_FAN_RISING_LIFT_LIMIT) {
                    work->period = work->period + 0xC0;
                } else {
                    work->scale = work->scale - INFERNO_FAN_SLOW_BRIGHTNESS_DECAY;
                }
                work->angle = work->angle + 0x20;
                work->step  = work->step + 0x18;
                _infernoDrawRisingFanBand(work, coord, INFERNO_FAN_RISING_BAND, texturePhase);
                _infernoDrawConstantLiftFanBand(work, coord, INFERNO_FAN_CONSTANT_LIFT_BAND, texturePhase);
                return;
            }
            break;
        case INFERNO_FAN_STATE_STATIONARY_BURST:
            if (work->scale >= INFERNO_FAN_BURST_BRIGHTNESS_DECAY + 1) {
                _infernoFadeAndDrawFanBands(work, coord, texturePhase, INFERNO_FAN_BURST_BRIGHTNESS_DECAY, 0x20, 0xC0, 0x18);
                return;
            }
            break;
        case INFERNO_FAN_STATE_DRIFTING_BURST:
            // Leave the composed matrix at this frame's old position; refresh next frame.
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->scale >= INFERNO_FAN_BURST_BRIGHTNESS_DECAY + 1) {
                _infernoFadeAndDrawFanBands(work, coord, texturePhase, INFERNO_FAN_BURST_BRIGHTNESS_DECAY, 0x20, 0xC0, 0x18);
                return;
            }
            break;
        case INFERNO_FAN_STATE_WIDE_IGNITION:
            if (work->scale >= INFERNO_FAN_IGNITION_BRIGHTNESS_DECAY + 1) {
                _infernoFadeAndDrawFanBands(work, coord, texturePhase, INFERNO_FAN_IGNITION_BRIGHTNESS_DECAY, 0x40, 0xC0, 0x10);
                return;
            }
            break;
        case INFERNO_FAN_STATE_SHALLOW_RING:
            if (work->scale >= INFERNO_FAN_IGNITION_BRIGHTNESS_DECAY + 1) {
                _infernoFadeAndDrawFanBands(work, coord, texturePhase, INFERNO_FAN_IGNITION_BRIGHTNESS_DECAY, 0x40, 0x40, 0x18);
                return;
            }
            break;
        case INFERNO_FAN_STATE_FAST_IGNITION:
            if (work->scale >= INFERNO_FAN_IGNITION_BRIGHTNESS_DECAY + 1) {
                _infernoFadeAndDrawFanBands(work, coord, texturePhase, INFERNO_FAN_IGNITION_BRIGHTNESS_DECAY, 0x80, 0x20, 0x20);
                return;
            }
            break;
        default:
            return;
    }
    effectKillTask(work, task);
}

/// Draws the Inferno fan band whose upper rim rises with its animated height.
///
/// `bandIndex` must select a shape and phase column (0..1); the fan task uses
/// `INFERNO_FAN_RISING_BAND`. `work->angle` is radial growth, `step` is extra
/// upper-rim spread, `period` is added to the shape's lift, and `scale` is
/// byte-stored texture brightness. Radii narrow to signed 16 bits and lift
/// wraps to unsigned 16 bits before placement along local -Y. Distances use
/// the coordinate frame's units, and `coord->workm` must be composed.
///
/// `texturePhase` supplies six cell phases for each band; adding the task's
/// nonnegative frame age selects one of six texture cells. Borrows 0x70 bytes
/// of aligned scratch-stack storage and releases it before returning. Requires
/// frame-arena space for up to six `POLY_FT4` packets, retained until drawing.
static void _infernoDrawRisingFanBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex, const _InfernoFanTexturePhase* texturePhase)
{
    _InfernoFanScratch*    scratch;
    const EffectBandShape* shape;
    const EffectBandShape* shapes;
    SVECTOR*               bottomVertex;
    POLY_FT4*              quad;
    s32                    projectionFlags;
    s32                    sortingDepth;
    s32                    segmentIndex;
    s32                    nextSegmentIndex;
    s32                    yaw;
    s32                    textureU;
    s16                    baseRadius;
    s16                    liftedRadius;
    u16                    lift;
    u16                    textureCell;

    shapes       = D_inferno_801304E4;
    shape        = &shapes[bandIndex];
    lift         = work->period + shape->lift;
    baseRadius   = work->angle + shape->baseRadius;
    liftedRadius = shape->spread + (baseRadius + work->step);
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_InfernoFanScratch);
    INFERNO_BUILD_FAN_RIMS(scratch, coord, baseRadius, liftedRadius, lift);
    // Project adjacent rim pairs, then queue each valid textured segment.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < INFERNO_FAN_SEGMENT_COUNT; segmentIndex++) {
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtps();
        textureCell = (texturePhase->byBand[bandIndex][segmentIndex] + work->age) % INFERNO_FAN_SEGMENT_COUNT;
        gte_stsxy(&scratch->sxy0);
        nextSegmentIndex = segmentIndex + 1;
        gte_ldv3(&scratch->topRing[nextSegmentIndex % INFERNO_FAN_SEGMENT_COUNT], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegmentIndex % INFERNO_FAN_SEGMENT_COUNT]);
        gte_rtpt();
        gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
        gte_stflg(&projectionFlags);
        if (projectionFlags >= 0) {
            gte_stszotz(&sortingDepth);
            sortingDepth++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setRGB0(quad, work->scale, work->scale, work->scale);
            quad->tpage = getTPage(INFERNO_FAN_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, INFERNO_FAN_TEXTURE_PAGE_X, INFERNO_FAN_TEXTURE_PAGE_Y);
            quad->clut  = getClut(INFERNO_FAN_PALETTE_X, INFERNO_FAN_PALETTE_Y);
            textureU    = textureCell * INFERNO_FAN_TEXTURE_CELL_SIZE;
            setUV4(quad, textureU, INFERNO_FAN_TEXTURE_V, textureU + INFERNO_FAN_TEXTURE_CELL_SIZE - 1, INFERNO_FAN_TEXTURE_V,
                   textureU, INFERNO_FAN_TEXTURE_V + INFERNO_FAN_TEXTURE_CELL_SIZE - 1,
                   textureU + INFERNO_FAN_TEXTURE_CELL_SIZE - 1, INFERNO_FAN_TEXTURE_V + INFERNO_FAN_TEXTURE_CELL_SIZE - 1);
            quad->x0 = scratch->sxy0;
            quad->y0 = scratch->sxy0 >> 16;
            quad->x1 = scratch->sxy1;
            quad->y1 = scratch->sxy1 >> 16;
            quad->x2 = scratch->sxy2;
            quad->y2 = scratch->sxy2 >> 16;
            quad->x3 = scratch->sxy3;
            quad->y3 = scratch->sxy3 >> 16;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sortingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_InfernoFanScratch);
}

/// Draws the Inferno fan band whose upper rim stays at the shape's fixed lift.
///
/// `bandIndex` must select a shape and phase column (0..1); the fan task uses
/// `INFERNO_FAN_CONSTANT_LIFT_BAND`. Uses the radii, brightness, texture phases,
/// composed coordinate, scratch stack and frame arena described for
/// `_infernoDrawRisingFanBand`, with no contribution from `work->period`.
static void _infernoDrawConstantLiftFanBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex, const _InfernoFanTexturePhase* texturePhase)
{
    _InfernoFanScratch*    scratch;
    const EffectBandShape* shape;
    const EffectBandShape* shapes;
    SVECTOR*               bottomVertex;
    POLY_FT4*              quad;
    s32                    projectionFlags;
    s32                    sortingDepth;
    s32                    segmentIndex;
    s32                    nextSegmentIndex;
    s32                    yaw;
    s32                    textureU;
    s16                    baseRadius;
    s16                    liftedRadius;
    u16                    lift;
    u16                    textureCell;

    shapes       = D_inferno_801304E4;
    shape        = &shapes[bandIndex];
    baseRadius   = work->angle + shape->baseRadius;
    liftedRadius = shape->spread + (baseRadius + work->step);
    lift         = shape->lift;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_InfernoFanScratch);
    INFERNO_BUILD_FAN_RIMS(scratch, coord, baseRadius, liftedRadius, lift);
    // Project adjacent rim pairs, then queue each valid textured segment.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < INFERNO_FAN_SEGMENT_COUNT; segmentIndex++) {
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtps();
        textureCell = (texturePhase->byBand[bandIndex][segmentIndex] + work->age) % INFERNO_FAN_SEGMENT_COUNT;
        gte_stsxy(&scratch->sxy0);
        nextSegmentIndex = segmentIndex + 1;
        gte_ldv3(&scratch->topRing[nextSegmentIndex % INFERNO_FAN_SEGMENT_COUNT], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegmentIndex % INFERNO_FAN_SEGMENT_COUNT]);
        gte_rtpt();
        gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
        gte_stflg(&projectionFlags);
        if (projectionFlags >= 0) {
            gte_stszotz(&sortingDepth);
            sortingDepth++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setRGB0(quad, work->scale, work->scale, work->scale);
            quad->tpage = getTPage(INFERNO_FAN_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, INFERNO_FAN_TEXTURE_PAGE_X, INFERNO_FAN_TEXTURE_PAGE_Y);
            quad->clut  = getClut(INFERNO_FAN_PALETTE_X, INFERNO_FAN_PALETTE_Y);
            textureU    = textureCell * INFERNO_FAN_TEXTURE_CELL_SIZE;
            setUV4(quad, textureU, INFERNO_FAN_TEXTURE_V, textureU + INFERNO_FAN_TEXTURE_CELL_SIZE - 1, INFERNO_FAN_TEXTURE_V,
                   textureU, INFERNO_FAN_TEXTURE_V + INFERNO_FAN_TEXTURE_CELL_SIZE - 1,
                   textureU + INFERNO_FAN_TEXTURE_CELL_SIZE - 1, INFERNO_FAN_TEXTURE_V + INFERNO_FAN_TEXTURE_CELL_SIZE - 1);
            quad->x0 = scratch->sxy0;
            quad->y0 = scratch->sxy0 >> 16;
            quad->x1 = scratch->sxy1;
            quad->y1 = scratch->sxy1 >> 16;
            quad->x2 = scratch->sxy2;
            quad->y2 = scratch->sxy2 >> 16;
            quad->x3 = scratch->sxy3;
            quad->y3 = scratch->sxy3 >> 16;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sortingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_InfernoFanScratch);
}

#undef INFERNO_BUILD_FAN_RIMS
