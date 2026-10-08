#include "pe/energyshot.h"

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
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"

/// Visual tuning of the energy shot cast for one Parasite Energy level.
///
/// The cast draws three rings and a fan of glow wedges on a plane raised above
/// the coordinate it is attached to, and up to three beams, each a flared
/// sleeve of textured quads rising from that coordinate. They grow together
/// while the cast sheds a spark each frame, then fade. The cast task selects
/// its row with the level digit of the attachment id, less one, which it keeps
/// in `EffectWork::index`.
///
/// `scaleLimit` and `scaleStep` are in the units of the cast's
/// `EffectWork::scale`, which is at once the brightness of the drawing (its
/// low byte is the red and blue channel, half of it the green) and, multiplied
/// up, the radius of each ring, wedge and beam. `ringHeight` is a distance
/// along the coordinate's Y axis.
typedef struct {
    s16 wedgeCount; // Glow wedges fanned around the rings; the yaw table holds 16
    s16 scaleLimit; // Scale that ends the growth; the fading rings are drawn at this scale
    s16 scaleStep;  // Scale gained per frame of growth; at level 3 the fading wedges and beams keep gaining it
    u16 ringHeight; // Height of the ring and wedge plane. The main beam rises to 0x100 short of it; from level 2 a second rises to twice it, and level 3 adds a third to half of it. Also the size of each spark the cast sheds
} _EnergyshotLevelTuning;
STATIC_ASSERT_SIZEOF(_EnergyshotLevelTuning, 8);

/// Per-level tuning for the energy shot: rows are PE levels 1-3.
static _EnergyshotLevelTuning D_energyshot_801300E4[] = {
    { 0x0008, 0x0090, 0x0005, 0x0400 },
    { 0x000C, 0x00C0, 0x0006, 0x0500 },
    { 0x0010, 0x00F0, 0x0007, 0x0600 },
};

/// The `sndEvtRequestScriptStart` id for each `D_energyshot_801300E4` row.
static s32 D_energyshot_801300FC[] = { 0xE02A0001, 0xE02D0001, 0xE0300001 };

static void _energyshotDrawBeamBand(const GfxCoord* coord, s16 radiusGrowth, s16 height, const u8* rgb);

/// Sixteen per-vertex texture-frame offsets, refilled once per cast by
/// `func_energyshot_8012EF34` and consumed by the GTE pass in
/// `_energyshotDrawBeamBand`, where each is added to `gDisplayState.animFrame`
/// and reduced mod 6 to pick one of the six 0x28-wide frames of the beam
/// texture.
static s16 D_energyshot_80130108[16];
/// Sixteen wedge yaws, refilled once per cast by `func_energyshot_8012EF34`
/// from `gRandomLcgState`. Entry `i` is `i * (0x1000 / wedgeCount)` plus a 9-bit LCG
/// draw. States 1 and 2 pass one yaw per frame to `glowDrawWedge`.
static s16 D_energyshot_80130128[16];

/// Energy shot PE. `Task::spawnArg2` is the `EffectWork` block; `Task::extra`
/// reaches the coordinate. Cancel (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD` or
/// `gRoomEffectState->peEffectControl >= 4`) releases the work block.
///
/// State 0 parents the coordinate, seeds 16 texture-frame offsets and 16 wedge
/// yaws from `gRandomLcgState`, and plays the combo-indexed cue. State 1 grows
/// brightness / radius, draws three rings plus `wedgeCount` wedges and the beam,
/// and parents a `0x600F4` spark; once brightness exceeds the row cap it
/// advances to state 2, which shrinks brightness until it drops below 0x11.
void func_energyshot_8012EF34(Task* arg0)
{
    EffectWork*      mem;
    GfxCoord*        coord;
    AttachmentState* state;
    s32              i;
    u8               rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((state->effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0: {
                RoomEffectState* effectState;
                s16              count;
                u16              level;

                coord->parent = mem->parent;
                gfxSetRotIdentity(&coord->coord);
                coord->coord.t[2]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[0]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                state->flags             |= ATTACHMENT_FLAG_APPLY_STATS;
                effectState               = gRoomEffectState;
                effectState->burstRequest = false;
                effectState->peFxFlags   &= (u16)~ROOM_EFFECT_PE_ENERGY_SHOT_AURA;
                arg0->state               = 1;
                mem->index                = (Gp_StateC08.attachId % 10) - 1;
                i                         = 0;
                {
                    s16* frames;

                    frames = D_energyshot_80130108;
                    do {
                        s32 rng;

                        i              += 1;
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        *frames         = ((u32)rng >> 16) & 0xFF;
                        frames         += 1;
                        gRandomLcgState = rng;
                    } while (i < 0x10);
                }
                i = 0;
                {
                    _EnergyshotLevelTuning* tbl;

                    tbl   = D_energyshot_801300E4;
                    count = tbl[mem->index].wedgeCount;
                    level = mem->index;
                    if (count > 0) {
                        do {
                            s32 lo;
                            s32 rng;

                            lo                       = i * (0x1000 / D_energyshot_801300E4[(s16)level].wedgeCount);
                            rng                      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            D_energyshot_80130128[i] = lo + (((u32)rng >> 16) & 0x1FF);
                            i                       += 1;
                            gRandomLcgState          = rng;
                            count                    = D_energyshot_801300E4[mem->index].wedgeCount;
                            level                    = mem->index;
                        } while (i < count);
                    }
                }
                {
                    s32 pan;

                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(D_energyshot_801300FC[mem->index], pan,
                                             (s8)worldCoordGetOriginAudioDepth(coord));
                }
                return;
            }
            case 1: {
                _EnergyshotLevelTuning* table;
                _EnergyshotLevelTuning* t2;
                s32                     rng;
                s16                     ang;
                s16*                    p;
                s16                     count;

                table               = D_energyshot_801300E4;
                mem->scale          = mem->scale + table[mem->index].scaleStep;
                rgb[0]              = (u8)mem->scale;
                rgb[1]              = mem->scale >> 1;
                rgb[2]              = (u8)mem->scale;
                coord->coord.t[1]   = -(s16)table[mem->index].ringHeight;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                effectDrawGouraudDisc(coord, (s16)(mem->scale * 4), rgb);
                effectDrawGouraudDisc(coord, (s16)(mem->scale * 8), rgb);
                effectDrawGouraudDisc(coord, (s16)(mem->scale * 0xC), rgb);
                i     = 0;
                count = table[mem->index].wedgeCount;
                if (count > 0) {
                    t2 = table;
                    p  = D_energyshot_80130128;
                    do {
                        glowDrawWedge(coord, (s16)(mem->scale * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].wedgeCount);
                }
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (mem->index != 0) {
                    if (mem->index == 2) {
                        _energyshotDrawBeamBand(coord, (s16)(mem->scale * 8),
                                                (s16)D_energyshot_801300E4[2].ringHeight >> 1, rgb);
                    }
                    _energyshotDrawBeamBand(
                        coord, (s16)(mem->scale * 4),
                        D_energyshot_801300E4[mem->index].ringHeight * 2, rgb);
                }
                _energyshotDrawBeamBand(
                    coord, (s16)(mem->scale * 6),
                    D_energyshot_801300E4[mem->index].ringHeight - 0x100, rgb);
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                ang             = ((u32)rng >> 16) & 0xFFF;
                gRandomLcgState = rng;
                mem->angle      = ang;
                mem->move.vx    = (u32)(rsin(ang) * mem->scale * 3) >> 11;
                mem->move.vz    = (u32)(rcos(mem->angle) * mem->scale * 3) >> 11;
                effectSpawn(EFFECT_RISING_ENERGY_SPARK, coord,
                            (s16)D_energyshot_801300E4[mem->index].ringHeight | 0x8000,
                            &mem->move);
                if (D_energyshot_801300E4[mem->index].scaleLimit < mem->scale) {
                    effectSpawn((EFFECT_ENERGY_SHOT_AURA | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                    mem->period = mem->scale;
                    arg0->state = 2;
                }
                return;
            }
            case 2: {
                _EnergyshotLevelTuning* table;
                _EnergyshotLevelTuning* t2;
                s16*                    p;
                s16                     count;

                if (mem->scale < 0x11) {
                    effectKillTask(mem, arg0);
                    return;
                }
                mem->scale          = mem->scale - 0x10;
                rgb[0]              = (u8)mem->scale;
                rgb[1]              = mem->scale >> 1;
                rgb[2]              = (u8)mem->scale;
                table               = D_energyshot_801300E4;
                coord->coord.t[1]   = -(s16)table[mem->index].ringHeight;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                effectDrawGouraudDisc(coord, (s16)(table[mem->index].scaleLimit * 4), rgb);
                effectDrawGouraudDisc(coord, (s16)(table[mem->index].scaleLimit * 8), rgb);
                effectDrawGouraudDisc(coord, (s16)(table[mem->index].scaleLimit * 0xC), rgb);
                i     = 0;
                count = table[mem->index].wedgeCount;
                if (count > 0) {
                    t2 = table;
                    p  = D_energyshot_80130128;
                    do {
                        glowDrawWedge(coord, (s16)(mem->period * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].wedgeCount);
                }
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (mem->index != 0) {
                    if (mem->index == 2) {
                        mem->period =
                            mem->period + D_energyshot_801300E4[2].scaleStep;
                        _energyshotDrawBeamBand(
                            coord, (s16)(mem->period * 8),
                            (s16)D_energyshot_801300E4[mem->index].ringHeight >> 1,
                            rgb);
                    }
                    _energyshotDrawBeamBand(
                        coord, (s16)(mem->period * 4),
                        D_energyshot_801300E4[mem->index].ringHeight * 2, rgb);
                }
                _energyshotDrawBeamBand(
                    coord, (s16)(mem->period * 6),
                    D_energyshot_801300E4[mem->index].ringHeight - 0x100, rgb);
                return;
            }
        }
        return;
    }
    effectKillTask(mem, arg0);
}

#include "../../shared/glow_draw_wedge.inc.c"

/// Projects one band quad and selects its six-frame texture cell.
///
/// Borrows complete live scratch storage with both rings initialized and
/// segmentIndex in 0..15. The caller has installed the projection matrices.
/// Saves corner zero before RTPT advances the screen FIFO; only the final
/// RTPT FLAG is retained, and the last corner's SZ3 remains for the caller.
/// Texture phase lookup stays between RTPS and saving the first corner.
/// All input arguments are side-effect-free locals, used repeatedly; textureFrame
/// is a writable s16 lvalue. Captures display animation frame and the package phase table.
/// The scoped next-segment temporary is private to the expansion.
#define ENERGYSHOT_PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame)                                                               \
    {                                                                                                                                      \
        enum { ENERGYSHOT_BAND_FRAME_COUNT = 6 };                                                                                          \
        s32 nextSegmentIndex;                                                                                                              \
                                                                                                                                           \
        gte_ldv0(&(scratch)->topRing[(segmentIndex)]);                                                                                     \
        gte_rtps();                                                                                                                        \
        (textureFrame) = (u32)(D_energyshot_80130108[(segmentIndex)] + gDisplayState.animFrame) % ENERGYSHOT_BAND_FRAME_COUNT;             \
        gte_stsxy(&(scratch)->sxy0);                                                                                                       \
        nextSegmentIndex = ((segmentIndex) + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);                                                         \
        gte_ldv3(&(scratch)->topRing[nextSegmentIndex], &(scratch)->bottomRing[(segmentIndex)], &(scratch)->bottomRing[nextSegmentIndex]); \
        gte_rtpt();                                                                                                                        \
        gte_stsxy3(&(scratch)->sxy1, &(scratch)->sxy2, &(scratch)->sxy3);                                                                  \
        gte_stflg(&(scratch)->projectionFlags);                                                                                            \
    }

/// Draws one textured tapered Energy Shot beam band between local-XZ rims.
///
/// Borrows the composed `coord` and three RGB bytes. Signed `radiusGrowth`
/// and `height` are coordinate units: the top rim has radius growth + 1024
/// at Y = -height, and the bottom rim has radius growth / 2 + 256 at Y = 0.
/// Radii narrow to signed halfwords before Q12 sine/cosine products; rotated
/// and translated vertices also retain their low halfwords. Six 40-texel
/// frames use per-segment jitter plus the display frame. The final RTPT FLAG
/// rejects a segment; sorting uses its last corner's SZ3 / 4 + 1.
/// Reserves/releases one complete scratch block and appends at most sixteen
/// additive modulated `POLY_FT4` packets. Inputs must stay clear of scratch
/// and the unchecked primitive arena; queued packets live through drawing.
static void _energyshotDrawBeamBand(const GfxCoord* coord, s16 radiusGrowth, s16 height, const u8* rgb)
{
    enum {
        ENERGYSHOT_BAND_ANGLE_STEP         = 256,
        ENERGYSHOT_BAND_TRIG_FRACTION_BITS = 12,
        ENERGYSHOT_BAND_CELL_WIDTH         = 40,
        ENERGYSHOT_BAND_TOP_V              = 0x60,
        ENERGYSHOT_BAND_UV_SPAN            = 39,
        ENERGYSHOT_BAND_TOP_RADIUS_BASE    = 1024,
        ENERGYSHOT_BAND_BOTTOM_RADIUS_BASE = 256,
    };
    EffectBandScratch* scratch;
    SVECTOR*           bottomVertex;
    POLY_FT4*          quad;
    s32                segmentIndex;
    s32                rimAngle;
    s32                textureU;
    s16                textureFrame;
    s16                topRadius;
    s16                bottomRadius;

    bottomRadius = radiusGrowth / 2 + ENERGYSHOT_BAND_BOTTOM_RADIUS_BASE;
    topRadius    = radiusGrowth + ENERGYSHOT_BAND_TOP_RADIUS_BASE;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Build both rims in local XZ, then transform their narrowed vertices.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        rimAngle                          = segmentIndex * ENERGYSHOT_BAND_ANGLE_STEP;
        scratch->topRing[segmentIndex].vx = (rsin(rimAngle) * topRadius) >> ENERGYSHOT_BAND_TRIG_FRACTION_BITS;
        scratch->topRing[segmentIndex].vy = -height;
        scratch->topRing[segmentIndex].vz = (rcos(rimAngle) * topRadius) >> ENERGYSHOT_BAND_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx    = (u16)scratch->topRing[segmentIndex].vx + (u16)coord->workm.t[0];
        scratch->topRing[segmentIndex].vy    = (u16)scratch->topRing[segmentIndex].vy + (u16)coord->workm.t[1];
        scratch->topRing[segmentIndex].vz    = (u16)scratch->topRing[segmentIndex].vz + (u16)coord->workm.t[2];
        scratch->bottomRing[segmentIndex].vx = (rsin(rimAngle) * bottomRadius) >> ENERGYSHOT_BAND_TRIG_FRACTION_BITS;
        // Address the lower rim through the complete scratch block's byte view.
        bottomVertex     = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        bottomVertex->vy = 0;
        bottomVertex->vz = (rcos(rimAngle) * bottomRadius) >> ENERGYSHOT_BAND_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->bottomRing[segmentIndex]);
        scratch->bottomRing[segmentIndex].vx = (u16)scratch->bottomRing[segmentIndex].vx + (u16)coord->workm.t[0];
        bottomVertex->vy                     = (u16)bottomVertex->vy + (u16)coord->workm.t[1];
        bottomVertex->vz                     = (u16)bottomVertex->vz + (u16)coord->workm.t[2];
    }
    // Project sixteen wrapped segments; only accepted quads consume packets.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        ENERGYSHOT_PROJECT_BAND_SEGMENT(scratch, segmentIndex, textureFrame);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            scratch->otz++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setRGB0(quad, rgb[0], rgb[1], rgb[2]);
            setSemiTrans(quad, true);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            quad->clut  = getClut(16, 267);
            textureU    = textureFrame * ENERGYSHOT_BAND_CELL_WIDTH;
            setUV4(quad, textureU, ENERGYSHOT_BAND_TOP_V, textureU + ENERGYSHOT_BAND_UV_SPAN, ENERGYSHOT_BAND_TOP_V, textureU, ENERGYSHOT_BAND_TOP_V + ENERGYSHOT_BAND_UV_SPAN, textureU + ENERGYSHOT_BAND_UV_SPAN, ENERGYSHOT_BAND_TOP_V + ENERGYSHOT_BAND_UV_SPAN);
            quad->x0 = (u16)scratch->sxy0.vx;
            quad->y0 = (u16)scratch->sxy0.vy;
            quad->x1 = (u16)scratch->sxy1.vx;
            quad->y1 = (u16)scratch->sxy1.vy;
            quad->x2 = (u16)scratch->sxy2.vx;
            quad->y2 = (u16)scratch->sxy2.vy;
            quad->x3 = (u16)scratch->sxy3.vx;
            quad->y3 = (u16)scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}
#undef ENERGYSHOT_PROJECT_BAND_SEGMENT

/// Initializes an Energy Shot billboard's fixed upward velocity and screen rotation.
///
/// Borrows writable `work` for this call. Sets `move` to (0, -79..-16, 0)
/// in parent-coordinate units per callback tick and `scale` to an angle in
/// 0..4095 (4096 units per turn). The angle's clear upper four bits select
/// palette zero when passed to `effectDrawSpinningBillboard`.
///
/// Advances `gRandomLcgState` twice, choosing velocity before rotation.
/// Animation counters and ownership remain with the caller; no pointer is retained.
static inline void _energyshotInitRisingBillboard(EffectWork* work)
{
    enum {
        ENERGYSHOT_BILLBOARD_Y_VELOCITY_BASE = -16,
        ENERGYSHOT_BILLBOARD_Y_JITTER_MASK   = 0x3F,
        ENERGYSHOT_BILLBOARD_ROTATION_MASK   = 0xFFF,
    };

    work->move.vx   = 0;
    work->move.vz   = 0;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    // Interpret the unsigned subtraction's low halfword as a signed Y displacement.
    work->move.vy   = (s16)(ENERGYSHOT_BILLBOARD_Y_VELOCITY_BASE -
                          ((gRandomLcgState >> 16) & ENERGYSHOT_BILLBOARD_Y_JITTER_MASK));
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->scale     = (gRandomLcgState >> 16) & ENERGYSHOT_BILLBOARD_ROTATION_MASK;
}

void energyshotRisingBillboardTask(Task* task)
{
    enum {
        ENERGYSHOT_BILLBOARD_STATE_INITIALIZE = 0,
        ENERGYSHOT_BILLBOARD_STATE_ANIMATE    = 1,
        ENERGYSHOT_BILLBOARD_TICKS_PER_FRAME  = 4,
        ENERGYSHOT_BILLBOARD_FRAME_COUNT      = 8,
        ENERGYSHOT_BILLBOARD_SIZE             = 0x400,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         nextY;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    work->age = work->age + 1;
    if (task->state == ENERGYSHOT_BILLBOARD_STATE_INITIALIZE) {
        _energyshotInitRisingBillboard(work);
        task->state = ENERGYSHOT_BILLBOARD_STATE_ANIMATE;
    }

    // Initialization falls through to movement and drawing on the first tick.
    nextY               = coord->coord.t[1] + work->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
    if ((work->age & (ENERGYSHOT_BILLBOARD_TICKS_PER_FRAME - 1)) == 0) {
        work->index = work->index + 1;
    }
    if (work->index < ENERGYSHOT_BILLBOARD_FRAME_COUNT) {
        effectDrawSpinningBillboard(coord, work->index, ENERGYSHOT_BILLBOARD_SIZE, work->scale);
        return;
    }
    effectKillTask(work, task);
}
