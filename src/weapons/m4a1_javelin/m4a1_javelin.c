#include "weapons/m4a1_javelin.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "m4a1_javelin_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "types.h"
/// Read-only XYZ translation words from the launch sprite's `GfxCoord::workm.t`.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const long
#define SPRITE_QUAD_POS(p, i)            ((p)[i])
/// Signed texture-frame index for the eight-frame launch sprite.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

/// Scratch-stack block for projecting the two ends of one flare line.
///
/// The ends are world points, transformed one after the other through
/// `GsWSMATRIX` with one perspective transform each. Every transform writes
/// that end's screen position as one screen-XY word and replaces `flag` with
/// the GTE flag word; a negative flag word means that transform reported an
/// error, and the line is dropped. Otherwise that end's depth, SZ3 / 4, is
/// stored and raised by one. The line is sorted and blended at the first
/// end's depth alone; the second end's depth is stored but not read back.
///
/// Reserve one complete block and release it in scratch-stack order after
/// drawing; no pointer into it survives release.
typedef struct {
    s32     otz0; // Ordering-table depth of the first end, plus one; where the line is sorted and blended
    s32     otz1; // Ordering-table depth of the second end, plus one; never read back
    s32     flag; // GTE flag word of the latest transform; negative means that transform failed
    DVECTOR sxy0; // Projected screen position of the first end, in pixels; one GTE screen-XY word
    DVECTOR sxy1; // Projected screen position of the second end, in pixels; one GTE screen-XY word
} _M4a1JavelinLineScratch;
STATIC_ASSERT_SIZEOF(_M4a1JavelinLineScratch, 0x14);

/// Segment controls, angular units and perspective thickness used by the beam drawers.
enum {
    M4A1_JAVELIN_SEGMENT_CAP_NEAR_END  = 1,
    M4A1_JAVELIN_SEGMENT_REFRESH_ANGLE = 2,
    M4A1_JAVELIN_FULL_TURN             = 0x1000,
    M4A1_JAVELIN_HALF_TURN             = 0x800,
    M4A1_JAVELIN_QUARTER_TURN          = 0x400,
    M4A1_JAVELIN_EIGHTH_TURN           = 0x200,
    M4A1_JAVELIN_TRIG_FRACTION_BITS    = 12,
    M4A1_JAVELIN_RADIUS_DEPTH_PRODUCT  = 0x4000
};

static void _m4a1JavelinDrawBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444);
static void _m4a1JavelinDrawGroundBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444);
static void _m4a1JavelinDrawMuzzleFlareLine(const SVECTOR* muzzlePoint, const SVECTOR* ringPoint, u16 brightness);

/// Fixed local offset the guide beam's coordinate hangs at.
static SVECTOR D_m4a1_javelin_8011FA90 = { 0, 0x200, 0x20, 0 };

/// `(0, 0x800, 0)`: the probe offset `worldCollisionProbeGridSegment` traces each beam segment
/// against, rotated into world space by `gGfxViewCoord.workm` first.
static SVECTOR D_m4a1_javelin_8011FA98 = { 0, 0x800, 0, 0 };

/// Per-segment `flags` for `_m4a1JavelinDrawBeamSegment`, walked from the far
/// end (`[5]`, bit 1: retake the beam angle) to the muzzle (`[0]`, bit 0: cap
/// the near end).
static u16 D_m4a1_javelin_8011FAA0[6] = { 1, 0, 0, 0, 0, 2 };

/// The four RGB444 beam colours `EffectWork::step` fades through.
static u16 D_m4a1_javelin_8011FAAC[4] = { 0x12, 0x124, 0x248, 0x36C };

static void _m4a1JavelinSetTrackedImpactPoint(const long* worldTranslation);
void        func_m4a1_javelin_8011F5D4(Task* arg0);

/// Allocates and initializes a Gouraud quad for a beam cap or connecting strip.
///
/// `quad` must be a simple POLY_G4* local lvalue, assigned once and read
/// repeatedly. Each colour argument is evaluated once; supply zero for the
/// second centre of a cap. The macro captures and advances `gGpuPrimCursor`
/// by one packet. Use as a standalone statement inside a braced control body;
/// it expands to a scoped block. Callers fill positions and link the packet
/// before submission.
#define M4A1_JAVELIN_ALLOCATE_BEAM_QUAD(quad, red, green, blue, secondRed, secondGreen, secondBlue) \
    {                                                                                               \
        (quad)         = gGpuPrimCursor;                                                            \
        gGpuPrimCursor = (quad) + 1;                                                                \
        setPolyG4(quad);                                                                            \
        setRGB0(quad, 0, 0, 0);                                                                     \
        setRGB1(quad, 0, 0, 0);                                                                     \
        setRGB2(quad, red, green, blue);                                                            \
        setRGB3(quad, secondRed, secondGreen, secondBlue);                                          \
    }

void m4a1JavelinGuideBeamTask(Task* task)
{
    enum {
        M4A1_JAVELIN_GUIDE_INIT             = 0,
        M4A1_JAVELIN_GUIDE_MUZZLE_FLARE     = 1,
        M4A1_JAVELIN_GUIDE_BEAM             = 2,
        M4A1_JAVELIN_BEAM_SEGMENT_COUNT     = ARRAY_SIZE(D_m4a1_javelin_8011FAA0),
        M4A1_JAVELIN_BEAM_LENGTH            = 8000,
        M4A1_JAVELIN_FLARE_INITIAL_RADIUS   = 0x600,
        M4A1_JAVELIN_FLARE_RADIUS_DECREMENT = 0xF0,
        M4A1_JAVELIN_FLARE_FORWARD_STEP     = 0xC0,
        M4A1_JAVELIN_FLARE_BRIGHTNESS_STEP  = 0x20,
        M4A1_JAVELIN_FLARE_MAX_BRIGHTNESS   = 0xC0,
        M4A1_JAVELIN_GUIDE_HOLD_TICKS       = 32,
        M4A1_JAVELIN_LIGHT_LIFETIME_TICKS   = 4,
        M4A1_JAVELIN_BEAM_BASE_RGB444       = 0x36C,
        M4A1_JAVELIN_PROBE_RAISE            = 0x100,
        // Actor mode occupies the low halfword, normal firing state 4 the high.
        M4A1_JAVELIN_PLAYER_FIRING_MODE_STATE = (4 << 16) | GAME_ACTOR_MODE_NORMAL
    };
    EffectWork*                    work;
    GfxCoord*                      coord;
    GameActor*                     actor;
    WorldCoordTransientPointLight* transientLight;
    GfxCoord*                      lightCoord;
    WorldCoordPointLight*          pointLight;
    SVECTOR                        nearPoint;
    SVECTOR                        farPoint;
    SVECTOR                        nearGroundPoint;
    SVECTOR                        farGroundPoint;
    s32                            segmentIndex;
    s32                            flareAngle;
    s32                            previousHitIndex;
    s32                            redIntensity;
    u16                            blueIntensity;

    actor          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    transientLight = &gWorldCoordTransientPointLights[1];
    pointLight     = &transientLight->light;
    lightCoord     = &transientLight->light.head.transform.coord;
    work           = task->spawnArg2.pointer;
    coord          = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    work->age = work->age + 1;
    switch (task->state) {
        case M4A1_JAVELIN_GUIDE_INIT:
            // Attach the beam to the weapon and reset its shared target/directions.
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = D_m4a1_javelin_8011FA90.vx;
            coord->coord.t[1]   = D_m4a1_javelin_8011FA90.vy;
            coord->coord.t[2]   = D_m4a1_javelin_8011FA90.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state                = M4A1_JAVELIN_GUIDE_MUZZLE_FLARE;
            work->move.vy              = M4A1_JAVELIN_BEAM_LENGTH;
            work->move.vx              = 0;
            work->move.vz              = 0;
            D_m4a1_javelin_8012EB68.vx = 0;
            D_m4a1_javelin_8012EB68.vy = 0;
            D_m4a1_javelin_8012EB68.vz = 0;
            D_m4a1_javelin_8012EB70    = 0;
            work->period               = M4A1_JAVELIN_FLARE_INITIAL_RADIUS;
            work->step                 = ARRAY_SIZE(D_m4a1_javelin_8011FAAC) - 1;
            D_m4a1_javelin_8012EB62    = 0;
            D_m4a1_javelin_8012EB60    = 0;
            /* fallthrough */
        case M4A1_JAVELIN_GUIDE_MUZZLE_FLARE:
            // Move the narrowing flare ring forward while its spokes brighten.
            actorRenderComposeCoord(coord);
            transientLight->framesLeft = M4A1_JAVELIN_LIGHT_LIFETIME_TICKS;
            pointLight->inner          = 0x100;
            pointLight->outer          = 0x1000;
            redIntensity               = pointLight->head.color.r >> 1;
            pointLight->head.color.r   = redIntensity;
            pointLight->head.color.g   = redIntensity >> 2;
            gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            pointLight->head.color.b   = ((gRandomLcgState >> 16) & 0x700) + 0x400;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightCoord->coord);
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->scale == M4A1_JAVELIN_FLARE_MAX_BRIGHTNESS) {
                task->state = M4A1_JAVELIN_GUIDE_BEAM;
            } else {
                work->scale  = work->scale + M4A1_JAVELIN_FLARE_BRIGHTNESS_STEP;
                work->angle  = work->angle + M4A1_JAVELIN_FLARE_FORWARD_STEP;
                work->period = work->period - M4A1_JAVELIN_FLARE_RADIUS_DECREMENT;
            }
            nearPoint.vx = coord->workm.t[0];
            nearPoint.vy = coord->workm.t[1];
            nearPoint.vz = coord->workm.t[2];
            for (flareAngle = 0; flareAngle < M4A1_JAVELIN_FULL_TURN; flareAngle += M4A1_JAVELIN_EIGHTH_TURN) {
                farPoint.vx = (work->period * rsin(flareAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS;
                farPoint.vy = work->angle;
                farPoint.vz = (work->period * rcos(flareAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&farPoint);
                gte_rtv0();
                gte_stsv(&farPoint);
                farPoint.vx = farPoint.vx + nearPoint.vx;
                farPoint.vy = farPoint.vy + nearPoint.vy;
                farPoint.vz = farPoint.vz + nearPoint.vz;
                _m4a1JavelinDrawMuzzleFlareLine(&nearPoint, &farPoint, work->scale);
            }
            return;
        case M4A1_JAVELIN_GUIDE_BEAM:
            // Walk from the tracked/default far end back toward the muzzle.
            actorRenderComposeCoord(coord);
            transientLight->framesLeft = M4A1_JAVELIN_LIGHT_LIFETIME_TICKS;
            pointLight->inner          = 0x400;
            pointLight->outer          = 0x4000;
            gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            blueIntensity              = ((gRandomLcgState >> 16) & 0x700) + 0x800;
            pointLight->head.color.b   = blueIntensity;
            pointLight->head.color.r   = blueIntensity >> 1;
            pointLight->head.color.g   = blueIntensity >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightCoord->coord);
            D_m4a1_javelin_8012EB64  = 0;
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            D_m4a1_javelin_8012EB66  = 0;
            if (D_m4a1_javelin_8012EB70 != 0) {
                nearPoint.vx = coord->workm.t[0];
                nearPoint.vy = coord->workm.t[1];
                nearPoint.vz = coord->workm.t[2];
                farPoint.vx  = D_m4a1_javelin_8012EB68.vx;
                farPoint.vy  = D_m4a1_javelin_8012EB68.vy;
                farPoint.vz  = D_m4a1_javelin_8012EB68.vz;
            } else {
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&farPoint);
                nearPoint.vx = coord->workm.t[0];
                nearPoint.vy = coord->workm.t[1];
                nearPoint.vz = coord->workm.t[2];
                farPoint.vx  = farPoint.vx + nearPoint.vx;
                farPoint.vy  = farPoint.vy + nearPoint.vy;
                farPoint.vz  = farPoint.vz + nearPoint.vz;
            }
            work->pos.vx = (nearPoint.vx - farPoint.vx) / M4A1_JAVELIN_BEAM_SEGMENT_COUNT;
            work->pos.vy = (nearPoint.vy - farPoint.vy) / M4A1_JAVELIN_BEAM_SEGMENT_COUNT;
            work->pos.vz = (nearPoint.vz - farPoint.vz) / M4A1_JAVELIN_BEAM_SEGMENT_COUNT;
            nearPoint.vx = farPoint.vx;
            nearPoint.vy = farPoint.vy;
            nearPoint.vz = farPoint.vz;
            if (gRoomEffectState->groundTraceEnabled != 0) {
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&D_m4a1_javelin_8011FA98);
                gte_rtv0();
                gte_stsv(&farGroundPoint);
                farGroundPoint.vx = farGroundPoint.vx + farPoint.vx;
                farGroundPoint.vy = farGroundPoint.vy + farPoint.vy;
                farGroundPoint.vz = farGroundPoint.vz + farPoint.vz;
                farPoint.vy       = farPoint.vy - M4A1_JAVELIN_PROBE_RAISE;
                if (worldCollisionProbeGridSegment(&farGroundPoint, &farPoint, &farGroundPoint, NULL) == 1) {
                    previousHitIndex = M4A1_JAVELIN_BEAM_SEGMENT_COUNT;
                } else {
                    previousHitIndex = M4A1_JAVELIN_BEAM_SEGMENT_COUNT - 1;
                }
                farPoint.vy = farPoint.vy + M4A1_JAVELIN_PROBE_RAISE;
                for (segmentIndex = M4A1_JAVELIN_BEAM_SEGMENT_COUNT - 1; segmentIndex >= 0; segmentIndex--) {
                    nearPoint.vx = nearPoint.vx + work->pos.vx;
                    nearPoint.vy = nearPoint.vy + work->pos.vy;
                    nearPoint.vz = nearPoint.vz + work->pos.vz;
                    _m4a1JavelinDrawBeamSegment(&nearPoint, &farPoint, D_m4a1_javelin_8011FAA0[segmentIndex],
                                                D_m4a1_javelin_8011FAAC[work->step]);
                    gte_SetRotMatrix(&gGfxViewCoord.workm);
                    gte_ldv0(&D_m4a1_javelin_8011FA98);
                    gte_rtv0();
                    gte_stsv(&nearGroundPoint);
                    nearGroundPoint.vx = nearGroundPoint.vx + nearPoint.vx;
                    nearGroundPoint.vy = nearGroundPoint.vy + nearPoint.vy;
                    nearGroundPoint.vz = nearGroundPoint.vz + nearPoint.vz;
                    nearPoint.vy       = nearPoint.vy - M4A1_JAVELIN_PROBE_RAISE;
                    if (worldCollisionProbeGridSegment(&nearGroundPoint, &nearPoint, &nearGroundPoint, NULL) == 1) {
                        if (segmentIndex < previousHitIndex) {
                            _m4a1JavelinDrawGroundBeamSegment(&nearGroundPoint, &farGroundPoint, D_m4a1_javelin_8011FAA0[segmentIndex],
                                                              D_m4a1_javelin_8011FAAC[work->step >> 1]);
                        }
                        previousHitIndex = segmentIndex;
                    } else {
                        previousHitIndex = segmentIndex - 1;
                    }
                    nearPoint.vy      = nearPoint.vy + M4A1_JAVELIN_PROBE_RAISE;
                    farGroundPoint.vx = nearGroundPoint.vx;
                    farGroundPoint.vy = nearGroundPoint.vy;
                    farGroundPoint.vz = nearGroundPoint.vz;
                    farPoint.vx       = nearPoint.vx;
                    farPoint.vy       = nearPoint.vy;
                    farPoint.vz       = nearPoint.vz;
                }
            } else {
                for (segmentIndex = M4A1_JAVELIN_BEAM_SEGMENT_COUNT - 1; segmentIndex >= 0; segmentIndex--) {
                    nearPoint.vx = nearPoint.vx + work->pos.vx;
                    nearPoint.vy = nearPoint.vy + work->pos.vy;
                    nearPoint.vz = nearPoint.vz + work->pos.vz;
                    _m4a1JavelinDrawBeamSegment(&nearPoint, &farPoint, D_m4a1_javelin_8011FAA0[segmentIndex], M4A1_JAVELIN_BEAM_BASE_RGB444);
                    farPoint.vx = nearPoint.vx;
                    farPoint.vy = nearPoint.vy;
                    farPoint.vz = nearPoint.vz;
                }
            }
            // Once the hold expires, consume one colour step on every active tick.
            if (work->age >= M4A1_JAVELIN_GUIDE_HOLD_TICKS + 1) {
                work->step = work->step - 1;
                if (work->step < 0) {
                    effectKillTask(work, task);
                }
            } else if (*(s32*)&actor->mode != M4A1_JAVELIN_PLAYER_FIRING_MODE_STATE) {
                work->age = work->age + M4A1_JAVELIN_GUIDE_HOLD_TICKS;
            }
            break;
    }
}

/// Draws one additive guide-beam segment between two world points.
///
/// Points are read-only, word-aligned SVECTORs in game coordinate units.
/// `rgb444` packs 0x0RGB; an alternating 16-unit byte bias brightens the line,
/// and the surrounding Gouraud wedges use two thirds of that brightness.
/// `segmentFlags` selects the near-end cap and direction refresh with
/// `M4A1_JAVELIN_SEGMENT_*`. Refresh also draws the far-end cap; otherwise
/// segments share the cached screen direction (4096 angle units per turn).
/// A failed endpoint projection emits nothing and requests a direction refresh
/// on the next visible segment. Uses and releases one scratch block; packets
/// remain in the current primitive heap and ordering table until GPU completion.
static void _m4a1JavelinDrawBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444)
{
    OverlayPointPairScratch* scratch;
    LINE_F2*                 line;
    POLY_G4*                 quad;
    s32                      wedgeAngle;
    s32                      farCornerAngle;
    s32                      nearCornerAngle;
    s32                      stripAngle;
    s32                      flickerBias;
    u32                      expandedRgb444;
    u8                       red;
    u8                       green;
    u8                       blue;
    u16                      beamAngle;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(OverlayPointPairScratch);

    // Project both endpoints before emitting GPU packets.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(nearPoint);
    gte_rtps();
    gte_stsxy(&scratch->sx0);
    gte_stflg(&scratch->flag);
    if (scratch->flag >= 0) {
        gte_stszotz(&scratch->otz0);
        scratch->otz0++;
        gte_ldv0(farPoint);
        gte_rtps();
        gte_stsxy(&scratch->sx1);
        gte_stflg(&scratch->flag);
        if (scratch->flag >= 0) {
            gte_stszotz(&scratch->otz1);
            expandedRgb444 = rgb444;
            red            = (expandedRgb444 >> 4) & 0xF0;
            green          = expandedRgb444 & 0xF0;
            blue           = (rgb444 & 0xF) * 0x10;
            scratch->otz1++;
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineF2(line);
            flickerBias = ((u8)gDisplayState.animFrame & 1) * 0x10;
            red         = red + flickerBias;
            green       = green + flickerBias;
            blue        = blue + flickerBias;
            setRGB0(line, red, green, blue);
            line->x0 = scratch->sx0;
            line->y0 = scratch->sy0;
            line->x1 = scratch->sx1;
            line->y1 = scratch->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, scratch->otz0);
            scratch->radius0 = M4A1_JAVELIN_RADIUS_DEPTH_PRODUCT / scratch->otz0;
            scratch->radius1 = M4A1_JAVELIN_RADIUS_DEPTH_PRODUCT / scratch->otz1;
            red              = red * 2 / 3;
            green            = green * 2 / 3;
            blue             = blue * 2 / 3;
            // Share the strip direction and cap its terminal segments.
            if ((segmentFlags & M4A1_JAVELIN_SEGMENT_REFRESH_ANGLE) || D_m4a1_javelin_8012EB64 != 0) {
                beamAngle               = ratan2(line->y1 - line->y0, line->x0 - line->x1);
                D_m4a1_javelin_8012EB60 = beamAngle;
                D_m4a1_javelin_8012EB64 = 0;
                for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + M4A1_JAVELIN_HALF_TURN; wedgeAngle += M4A1_JAVELIN_QUARTER_TURN) {
                    M4A1_JAVELIN_ALLOCATE_BEAM_QUAD(quad, red, green, blue, 0, 0, 0);
                    farCornerAngle = wedgeAngle + M4A1_JAVELIN_HALF_TURN;
                    quad->x0       = line->x1 + ((scratch->radius1 * rsin(farCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    quad->y0       = line->y1 + ((scratch->radius1 * rcos(farCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    farCornerAngle = wedgeAngle + (M4A1_JAVELIN_HALF_TURN + M4A1_JAVELIN_EIGHTH_TURN);
                    quad->x1       = line->x1 + ((scratch->radius1 * rsin(farCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    quad->y1       = line->y1 + ((scratch->radius1 * rcos(farCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    quad->x2       = line->x1;
                    quad->y2       = line->y1;
                    farCornerAngle = wedgeAngle + (M4A1_JAVELIN_HALF_TURN + M4A1_JAVELIN_QUARTER_TURN);
                    quad->x3       = line->x1 + ((scratch->radius1 * rsin(farCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    quad->y3       = line->y1 + ((scratch->radius1 * rcos(farCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz1);
                }
            } else {
                beamAngle = D_m4a1_javelin_8012EB60;
            }
            if (segmentFlags & M4A1_JAVELIN_SEGMENT_CAP_NEAR_END) {
                for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + M4A1_JAVELIN_HALF_TURN; wedgeAngle += M4A1_JAVELIN_QUARTER_TURN) {
                    M4A1_JAVELIN_ALLOCATE_BEAM_QUAD(quad, red, green, blue, 0, 0, 0);
                    quad->x0        = line->x0 + ((scratch->radius0 * rsin(wedgeAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    quad->y0        = line->y0 + ((scratch->radius0 * rcos(wedgeAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    nearCornerAngle = wedgeAngle + M4A1_JAVELIN_EIGHTH_TURN;
                    quad->x1        = line->x0 + ((scratch->radius0 * rsin(nearCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    quad->y1        = line->y0 + ((scratch->radius0 * rcos(nearCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    quad->x2        = line->x0;
                    quad->y2        = line->y0;
                    nearCornerAngle = wedgeAngle + M4A1_JAVELIN_QUARTER_TURN;
                    quad->x3        = line->x0 + ((scratch->radius0 * rsin(nearCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    quad->y3        = line->y0 + ((scratch->radius0 * rcos(nearCornerAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);
                }
            }
            for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + M4A1_JAVELIN_HALF_TURN; wedgeAngle += M4A1_JAVELIN_QUARTER_TURN) {
                M4A1_JAVELIN_ALLOCATE_BEAM_QUAD(quad, red, green, blue, red, green, blue);
                stripAngle = (s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2);
                quad->x0   = line->x0 + ((scratch->radius0 * rsin(stripAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                quad->y0   = line->y0 + ((scratch->radius0 * rcos(stripAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                quad->x1   = line->x1 + ((scratch->radius1 * rsin(stripAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                quad->y1   = line->y1 + ((scratch->radius1 * rcos(stripAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
                quad->x2   = line->x0;
                quad->y2   = line->y0;
                quad->x3   = line->x1;
                quad->y3   = line->y1;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);
            }
        } else {
            D_m4a1_javelin_8012EB64 = 1;
        }
    } else {
        D_m4a1_javelin_8012EB64 = 1;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(OverlayPointPairScratch));
}

/// Draws the additive ground glow between adjacent beam collision points.
///
/// Inputs and `segmentFlags` follow `_m4a1JavelinDrawBeamSegment`; the caller
/// supplies two successful neighbouring probes in world coordinate units.
/// The 0x0RGB colour receives an alternating 8-unit bias and is halved.
/// Direction caching and projection-failure recovery are independent of the
/// airborne beam. Thickness scales inversely with each endpoint's depth.
/// Reserves/releases one scratch block and appends packets to the current heap.
static void _m4a1JavelinDrawGroundBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444)
{
    OverlayPointPairScratch* scratch;
    LINE_F2*                 line;
    POLY_G4*                 quad;
    u32                      flickerBias;
    u32                      expandedRgb444;
    u8                       red;
    u8                       green;
    u8                       blue;
    u16                      beamAngle;
    s32                      wedgeAngle;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(OverlayPointPairScratch);

    // Project both endpoints before emitting GPU packets.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(nearPoint);
    gte_rtps();
    gte_stsxy(&scratch->sx0);
    gte_stflg(&scratch->flag);
    if (scratch->flag < 0) {
        goto fail;
    }
    gte_stszotz(&scratch->otz0);
    scratch->otz0++;
    gte_ldv0(farPoint);
    gte_rtps();
    gte_stsxy(&scratch->sx1);
    gte_stflg(&scratch->flag);
    if (scratch->flag < 0) {
        goto fail;
    }
    gte_stszotz(&scratch->otz1);

    scratch->otz1++;
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    flickerBias    = (gDisplayState.animFrame & 1) * 8;
    expandedRgb444 = rgb444 & 0xFFFF;
    red            = (((expandedRgb444 >> 4) & 0xF0) + flickerBias) >> 1;
    green          = ((expandedRgb444 & 0xF0) + flickerBias) >> 1;
    blue           = (((rgb444 & 0xF) << 4) + flickerBias) >> 1;
    setRGB0(line, red, green, blue);
    line->x0 = scratch->sx0;
    line->y0 = scratch->sy0;
    line->x1 = scratch->sx1;
    line->y1 = scratch->sy1;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            line);
    gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, scratch->otz0);
    scratch->radius0 = M4A1_JAVELIN_RADIUS_DEPTH_PRODUCT / scratch->otz0;
    scratch->radius1 = M4A1_JAVELIN_RADIUS_DEPTH_PRODUCT / scratch->otz1;

    // Share the strip direction and cap its terminal segments.
    if ((segmentFlags & M4A1_JAVELIN_SEGMENT_REFRESH_ANGLE) || D_m4a1_javelin_8012EB66 != 0) {
        beamAngle               = ratan2(line->y1 - line->y0, line->x0 - line->x1);
        D_m4a1_javelin_8012EB62 = beamAngle;
        D_m4a1_javelin_8012EB66 = 0;
        for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + M4A1_JAVELIN_HALF_TURN; wedgeAngle += M4A1_JAVELIN_QUARTER_TURN) {
            M4A1_JAVELIN_ALLOCATE_BEAM_QUAD(quad, red, green, blue, 0, 0, 0);
            quad->x0 = line->x1 + ((scratch->radius1 * rsin(wedgeAngle + M4A1_JAVELIN_HALF_TURN)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->y0 = line->y1 + ((scratch->radius1 * rcos(wedgeAngle + M4A1_JAVELIN_HALF_TURN)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->x1 = line->x1 + ((scratch->radius1 * rsin(wedgeAngle + (M4A1_JAVELIN_HALF_TURN + M4A1_JAVELIN_EIGHTH_TURN))) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->y1 = line->y1 + ((scratch->radius1 * rcos(wedgeAngle + (M4A1_JAVELIN_HALF_TURN + M4A1_JAVELIN_EIGHTH_TURN))) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->x2 = line->x1;
            quad->y2 = line->y1;
            quad->x3 = line->x1 + ((scratch->radius1 * rsin(wedgeAngle + (M4A1_JAVELIN_HALF_TURN + M4A1_JAVELIN_QUARTER_TURN))) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->y3 = line->y1 + ((scratch->radius1 * rcos(wedgeAngle + (M4A1_JAVELIN_HALF_TURN + M4A1_JAVELIN_QUARTER_TURN))) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz1);
        }
    } else {
        beamAngle = D_m4a1_javelin_8012EB62;
    }

    if (segmentFlags & M4A1_JAVELIN_SEGMENT_CAP_NEAR_END) {
        for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + M4A1_JAVELIN_HALF_TURN; wedgeAngle += M4A1_JAVELIN_QUARTER_TURN) {
            M4A1_JAVELIN_ALLOCATE_BEAM_QUAD(quad, red, green, blue, 0, 0, 0);
            quad->x0 = line->x0 + ((scratch->radius0 * rsin(wedgeAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->y0 = line->y0 + ((scratch->radius0 * rcos(wedgeAngle)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->x1 = line->x0 + ((scratch->radius0 * rsin(wedgeAngle + M4A1_JAVELIN_EIGHTH_TURN)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->y1 = line->y0 + ((scratch->radius0 * rcos(wedgeAngle + M4A1_JAVELIN_EIGHTH_TURN)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->x2 = line->x0;
            quad->y2 = line->y0;
            quad->x3 = line->x0 + ((scratch->radius0 * rsin(wedgeAngle + M4A1_JAVELIN_QUARTER_TURN)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            quad->y3 = line->y0 + ((scratch->radius0 * rcos(wedgeAngle + M4A1_JAVELIN_QUARTER_TURN)) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);
        }
    }

    for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + M4A1_JAVELIN_HALF_TURN; wedgeAngle += M4A1_JAVELIN_QUARTER_TURN) {
        M4A1_JAVELIN_ALLOCATE_BEAM_QUAD(quad, red, green, blue, red, green, blue);
        quad->x0 = line->x0 + ((scratch->radius0 * rsin((s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2))) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
        quad->y0 = line->y0 + ((scratch->radius0 * rcos((s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2))) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
        quad->x1 = line->x1 + ((scratch->radius1 * rsin((s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2))) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
        quad->y1 = line->y1 + ((scratch->radius1 * rcos((s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2))) >> M4A1_JAVELIN_TRIG_FRACTION_BITS);
        quad->x2 = line->x0;
        quad->y2 = line->y0;
        quad->x3 = line->x1;
        quad->y3 = line->y1;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);
    }
    goto done;

fail:
    D_m4a1_javelin_8012EB66 = 1;
done:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(OverlayPointPairScratch));
}

#undef M4A1_JAVELIN_ALLOCATE_BEAM_QUAD

/// Draws one additive muzzle-flare spoke from the muzzle to its world-space ring.
///
/// Inputs are read-only, word-aligned SVECTORs in game coordinate units.
/// `brightness` is the blue byte at the muzzle (caller supplies 32..192);
/// green/red use one half/quarter, and the ring end is black. The spoke is
/// sorted at the muzzle depth and omitted if either projection fails.
/// Reserves/releases one scratch block and appends a Gouraud line to the heap.
static void _m4a1JavelinDrawMuzzleFlareLine(const SVECTOR* muzzlePoint, const SVECTOR* ringPoint, u16 brightness)
{
    _M4a1JavelinLineScratch* scratch;
    LINE_G2*                 line;
    s32*                     muzzleDepth;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(_M4a1JavelinLineScratch);
    muzzleDepth = &scratch->otz0;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(muzzlePoint);
    gte_rtps();
    gte_stsxy(&scratch->sxy0);
    gte_stflg(&scratch->flag);
    if (scratch->flag >= 0) {
        gte_stszotz(muzzleDepth);
        scratch->otz0++;
        gte_ldv0(ringPoint);
        gte_rtps();
        gte_stsxy(&scratch->sxy1);
        gte_stflg(&scratch->flag);
        if (scratch->flag >= 0) {
            gte_stszotz(&scratch->otz1);
            scratch->otz1++;
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG2(line);
            setRGB0(line, brightness >> 2, brightness >> 1, brightness);
            setRGB1(line, 0, 0, 0);
            line->x0 = scratch->sxy0.vx;
            line->y0 = scratch->sxy0.vy;
            line->x1 = scratch->sxy1.vx;
            line->y1 = scratch->sxy1.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, scratch->otz0);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_M4a1JavelinLineScratch);
}

/// Contact-flash palette: VRAM X=48 words, Y=267 scanlines.
#define SPRITE_QUAD_CLUT getClut(48, 267)
/// Texel width and horizontal stride of each of the launch flash's eight texture frames.
#define SPRITE_QUAD_CELL_WIDTH 32
/// Inclusive top texel row of the launch-flash strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x18
#define SPRITE_QUAD_V1    0x37
/// Perspective-sizing multiplier for Javelin's launch flash.
///
/// Uses the cell's inclusive 31-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

/// Sets or clears the guide beam's tracked world-space impact point.
///
/// `worldTranslation` is NULL to invalidate the point without clearing its
/// stale coordinates, or at least three readable signed translation words
/// (X, Y, Z). Each is narrowed to its low signed halfword. Values are copied;
/// the pointer is never retained, so a scratch coordinate is a valid source.
static void _m4a1JavelinSetTrackedImpactPoint(const long* worldTranslation)
{
    if (worldTranslation == NULL) {
        D_m4a1_javelin_8012EB70 = 0;
        return;
    }
    D_m4a1_javelin_8012EB68.vx = worldTranslation[0];
    D_m4a1_javelin_8012EB68.vy = worldTranslation[1];
    D_m4a1_javelin_8012EB70    = 1;
    D_m4a1_javelin_8012EB68.vz = worldTranslation[2];
}

void m4a1JavelinContactFlashTask(Task* task)
{
    enum {
        M4A1_JAVELIN_CONTACT_FLASH_INIT        = 0,
        M4A1_JAVELIN_CONTACT_FLASH_PLAYING     = 1,
        M4A1_JAVELIN_CONTACT_FLASH_FRAME_COUNT = 8,
        M4A1_JAVELIN_CONTACT_FLASH_SIZE        = 512
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }

    work->age++;
    if (task->state == M4A1_JAVELIN_CONTACT_FLASH_INIT) {
        work->scale     = M4A1_JAVELIN_CONTACT_FLASH_SIZE;
        gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->angle     = (gRandomLcgState >> 16) & (M4A1_JAVELIN_FULL_TURN - 1);
        task->state     = M4A1_JAVELIN_CONTACT_FLASH_PLAYING;
    }
    // Age counts active ticks from one; the texture strip is indexed from zero.
    spriteQuadDraw(coord->workm.t, work->age - 1, work->scale, work->angle);
    if (work->age == M4A1_JAVELIN_CONTACT_FLASH_FRAME_COUNT) {
        effectKillTask(work, task);
    }
}

/// Per-frame firing state machine for the M4A1 javelin launcher, the sibling of
/// `func_m4a1_grenade_8011D1EC`. State 0 arms the shot and raises the weapon
/// (clip 8 instead of 1 when it was already up), state 1 waits for that clip.
/// State 2 branches on `field_97F`: a held trigger (bit 0) drops into the
/// three-round burst of state 3, a tap (bit 1) fires the single 0x101 javelin
/// of state 5, and anything else falls straight into the burst. State 3 counts
/// `field_934` down to each round, spending one javelin, playing `0x201D0004`
/// and spawning the muzzle flash; on the frame `field_934` reaches 2 it drops
/// the aim lock and spawns the 0x6003B impact marker, which state 4 also does
/// before parking in state 7. States 5 and 6 run the flight timer and feed the
/// tracked point to `_m4a1JavelinSetTrackedImpactPoint` (or clear it when nothing is
/// in range) so the guide line is drawn. State 7 runs the recoil timer down and
/// hands back to `func_80106550` once `playerActorIsSlotAdvancingLinearly` is done or the timer has
/// run out.
///
/// `gPlayerStatus.weaponSlotItem` is the low byte `func_801061F0` packs into
/// `GameActor::collisionBodies[GAME_ACTOR_BODY_WEAPON].key`. Reading it through the struct rather than as a bare
/// `extern u8` at 0x80073BAA is what keeps GCC from hoisting the `lbu` above
/// the `actor->` stores: a scalar global and a struct field do not alias, so
/// the scheduler is free to move the load, and the block comes out reordered.
void func_m4a1_javelin_8011F5D4(Task* arg0)
{
    GameActor*  actor;
    GfxCoord*   coord;
    GfxCoord*   spot;
    EffectWork* eff;
    s32         anim;
    s32         delay;
    s32         tick;
    u16         count;

    SCRATCH_STACK_RESERVE_BYTES(0x58);
    coord        = arg0->extra.tmd->coords;
    actor        = arg0->work;
    spot         = SCRATCH_STACK_CURSOR(GfxCoord);
    spot->parent = NULL;

    switch (actor->statePhase) {
        case 0:
            anim                                                  = 1;
            actor->state                                          = 4;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += anim;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
            }
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            actor->rumblePosted = 0;
            if (actor->attackButton & 1) {
                actor->statePhase                                     = 3;
                actor->attackCancelTicks                              = 9;
                actor->turnRateIndex                                  = 0;
                actor->stateTimer                                     = 0;
                actor->actionValue                                    = 3;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | 0x21D00;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
                func_80106238(arg0, 0, 1);
            } else if (actor->attackButton & 2) {
                actor->statePhase                                     = 5;
                actor->turnRateIndex                                  = 2;
                actor->attackCancelTicks                              = 0x1C;
                actor->stateTimer                                     = 6;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = 0x21D1F;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                func_80106238(arg0, 0, 0);
                Gp_ConsumeSlotQty(0x9C, 0x101);
                eff = Gp_SpawnEff(EFFECT_JAVELIN_GUIDE_BEAM,
                                  actor->equipmentTasks[1]->extra.tmd->coords,
                                  0x1D, NULL);
                if (eff != NULL) {
                    taskReparent(actor->equipmentTasks[1], eff->task);
                }
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x201D0005, 1);
                playerActorPlayChildSlotsWithBlend(arg0, 0xB, 0, 3);
                break;
            }
            /* fallthrough */
        case 3:
            count = actor->actionValue;
            if (actor->actionValue != 0) {
                delay = actor->stateTimer;
                if (delay == 0) {
                    actor->actionValue                                    = count - 1;
                    actor->stateTimer                                     = 3;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ConsumeSlotQty(0x9C, 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x201D0004, 1);
                    Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                0x1D, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                } else {
                    delay--;
                    actor->stateTimer = delay;
                    if (delay == 2) {
                        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                        if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                            Gp_SpawnEff(EFFECT_IMPACT_SPARK, spot, 0, NULL);
                            Gp_PlayObjSfx(spot, 0x17, 1);
                        }
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->statePhase                                     = 7;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_SpawnEff(EFFECT_IMPACT_SPARK, spot, 0, NULL);
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            break;
        case 5:
        case 6:
            tick              = actor->stateTimer - 1;
            actor->stateTimer = tick;
            if (tick == 0) {
                if (actor->statePhase == 5) {
                    actor->statePhase                                     = 6;
                    actor->stateTimer                                     = 0x1C;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    actor->statePhase                                     = 7;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
            }
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                _m4a1JavelinSetTrackedImpactPoint(spot->workm.t);
                eff = Gp_SpawnEff(EFFECT_M4A1_JAVELIN_CONTACT_FLASH, spot, 0, NULL);
                if (eff != NULL) {
                    taskReparent(actor->equipmentTasks[1], eff->task);
                }
            } else {
                _m4a1JavelinSetTrackedImpactPoint(NULL);
            }
            break;
        case 7:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x58);
}

static TmdBone _gM4a1JavelinModel02D7CSkeleton[1] = {
#include "assets/m4a1_javelin_model_02D7C_skeleton.inc"
};

static u32 _gM4a1JavelinModel02D7CPartVerts[1] = {
#include "assets/m4a1_javelin_model_02D7C_partVerts.inc"
};

static SVECTOR _gM4a1JavelinModel02D7CVerts[74] = {
#include "assets/m4a1_javelin_model_02D7C_verts.inc"
};

static SVECTOR _gM4a1JavelinModel02D7CNormals[66] = {
#include "assets/m4a1_javelin_model_02D7C_normals.inc"
};

static u32 _gM4a1JavelinModel02D7CStream[504] = {
#include "assets/m4a1_javelin_model_02D7C_stream.inc"
};

TmdSource D_m4a1_javelin_8012071C = {
    0,
    3668,
    0,
    1,
    _gM4a1JavelinModel02D7CPartVerts,
    _gM4a1JavelinModel02D7CVerts,
    _gM4a1JavelinModel02D7CNormals,
    _gM4a1JavelinModel02D7CSkeleton,
    _gM4a1JavelinModel02D7CStream,
};

static AnimationPackedPose _gM4a1JavelinAnimation03710Bank1[2] = {
#include "assets/m4a1_javelin_animation_03710_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation03710Bank4[8] = {
#include "assets/m4a1_javelin_animation_03710_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation03710Records[76] = {
#include "assets/m4a1_javelin_animation_03710_records.inc"
};

static u16 _gM4a1JavelinAnimation03710Indices[20] = {
#include "assets/m4a1_javelin_animation_03710_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation03710 = {
    _gM4a1JavelinAnimation03710Records,
    _gM4a1JavelinAnimation03710Indices,
    { NULL, _gM4a1JavelinAnimation03710Bank1, NULL, NULL, _gM4a1JavelinAnimation03710Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation03DB4Bank1[12] = {
#include "assets/m4a1_javelin_animation_03DB4_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation03DB4Bank4[151] = {
#include "assets/m4a1_javelin_animation_03DB4_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation03DB4Records[218] = {
#include "assets/m4a1_javelin_animation_03DB4_records.inc"
};

static u16 _gM4a1JavelinAnimation03DB4Indices[20] = {
#include "assets/m4a1_javelin_animation_03DB4_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation03DB4 = {
    _gM4a1JavelinAnimation03DB4Records,
    _gM4a1JavelinAnimation03DB4Indices,
    { NULL, _gM4a1JavelinAnimation03DB4Bank1, NULL, NULL, _gM4a1JavelinAnimation03DB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation04614Bank1[19] = {
#include "assets/m4a1_javelin_animation_04614_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation04614Bank4[169] = {
#include "assets/m4a1_javelin_animation_04614_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation04614Records[290] = {
#include "assets/m4a1_javelin_animation_04614_records.inc"
};

static u16 _gM4a1JavelinAnimation04614Indices[20] = {
#include "assets/m4a1_javelin_animation_04614_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation04614 = {
    _gM4a1JavelinAnimation04614Records,
    _gM4a1JavelinAnimation04614Indices,
    { NULL, _gM4a1JavelinAnimation04614Bank1, NULL, NULL, _gM4a1JavelinAnimation04614Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation04E78Bank1[19] = {
#include "assets/m4a1_javelin_animation_04E78_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation04E78Bank4[170] = {
#include "assets/m4a1_javelin_animation_04E78_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation04E78Records[290] = {
#include "assets/m4a1_javelin_animation_04E78_records.inc"
};

static u16 _gM4a1JavelinAnimation04E78Indices[20] = {
#include "assets/m4a1_javelin_animation_04E78_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation04E78 = {
    _gM4a1JavelinAnimation04E78Records,
    _gM4a1JavelinAnimation04E78Indices,
    { NULL, _gM4a1JavelinAnimation04E78Bank1, NULL, NULL, _gM4a1JavelinAnimation04E78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0518CBank1[3] = {
#include "assets/m4a1_javelin_animation_0518C_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0518CBank4[69] = {
#include "assets/m4a1_javelin_animation_0518C_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0518CRecords[99] = {
#include "assets/m4a1_javelin_animation_0518C_records.inc"
};

static u16 _gM4a1JavelinAnimation0518CIndices[20] = {
#include "assets/m4a1_javelin_animation_0518C_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0518C = {
    _gM4a1JavelinAnimation0518CRecords,
    _gM4a1JavelinAnimation0518CIndices,
    { NULL, _gM4a1JavelinAnimation0518CBank1, NULL, NULL, _gM4a1JavelinAnimation0518CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation058E8Bank1[14] = {
#include "assets/m4a1_javelin_animation_058E8_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation058E8Bank4[156] = {
#include "assets/m4a1_javelin_animation_058E8_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation058E8Records[253] = {
#include "assets/m4a1_javelin_animation_058E8_records.inc"
};

static u16 _gM4a1JavelinAnimation058E8Indices[20] = {
#include "assets/m4a1_javelin_animation_058E8_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation058E8 = {
    _gM4a1JavelinAnimation058E8Records,
    _gM4a1JavelinAnimation058E8Indices,
    { NULL, _gM4a1JavelinAnimation058E8Bank1, NULL, NULL, _gM4a1JavelinAnimation058E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation06070Bank1[16] = {
#include "assets/m4a1_javelin_animation_06070_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation06070Bank4[167] = {
#include "assets/m4a1_javelin_animation_06070_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation06070Records[247] = {
#include "assets/m4a1_javelin_animation_06070_records.inc"
};

static u16 _gM4a1JavelinAnimation06070Indices[20] = {
#include "assets/m4a1_javelin_animation_06070_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation06070 = {
    _gM4a1JavelinAnimation06070Records,
    _gM4a1JavelinAnimation06070Indices,
    { NULL, _gM4a1JavelinAnimation06070Bank1, NULL, NULL, _gM4a1JavelinAnimation06070Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation06344Bank1[6] = {
#include "assets/m4a1_javelin_animation_06344_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation06344Bank4[52] = {
#include "assets/m4a1_javelin_animation_06344_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation06344Records[91] = {
#include "assets/m4a1_javelin_animation_06344_records.inc"
};

static u16 _gM4a1JavelinAnimation06344Indices[20] = {
#include "assets/m4a1_javelin_animation_06344_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation06344 = {
    _gM4a1JavelinAnimation06344Records,
    _gM4a1JavelinAnimation06344Indices,
    { NULL, _gM4a1JavelinAnimation06344Bank1, NULL, NULL, _gM4a1JavelinAnimation06344Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation066D4Bank1[7] = {
#include "assets/m4a1_javelin_animation_066D4_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation066D4Bank4[73] = {
#include "assets/m4a1_javelin_animation_066D4_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation066D4Records[114] = {
#include "assets/m4a1_javelin_animation_066D4_records.inc"
};

static u16 _gM4a1JavelinAnimation066D4Indices[20] = {
#include "assets/m4a1_javelin_animation_066D4_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation066D4 = {
    _gM4a1JavelinAnimation066D4Records,
    _gM4a1JavelinAnimation066D4Indices,
    { NULL, _gM4a1JavelinAnimation066D4Bank1, NULL, NULL, _gM4a1JavelinAnimation066D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation06B5CBank1[9] = {
#include "assets/m4a1_javelin_animation_06B5C_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation06B5CBank4[104] = {
#include "assets/m4a1_javelin_animation_06B5C_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation06B5CRecords[139] = {
#include "assets/m4a1_javelin_animation_06B5C_records.inc"
};

static u16 _gM4a1JavelinAnimation06B5CIndices[20] = {
#include "assets/m4a1_javelin_animation_06B5C_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation06B5C = {
    _gM4a1JavelinAnimation06B5CRecords,
    _gM4a1JavelinAnimation06B5CIndices,
    { NULL, _gM4a1JavelinAnimation06B5CBank1, NULL, NULL, _gM4a1JavelinAnimation06B5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation06D58Bank1[3] = {
#include "assets/m4a1_javelin_animation_06D58_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation06D58Bank4[22] = {
#include "assets/m4a1_javelin_animation_06D58_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation06D58Records[76] = {
#include "assets/m4a1_javelin_animation_06D58_records.inc"
};

static u16 _gM4a1JavelinAnimation06D58Indices[20] = {
#include "assets/m4a1_javelin_animation_06D58_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation06D58 = {
    _gM4a1JavelinAnimation06D58Records,
    _gM4a1JavelinAnimation06D58Indices,
    { NULL, _gM4a1JavelinAnimation06D58Bank1, NULL, NULL, _gM4a1JavelinAnimation06D58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation07030Bank1[6] = {
#include "assets/m4a1_javelin_animation_07030_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation07030Bank4[57] = {
#include "assets/m4a1_javelin_animation_07030_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation07030Records[87] = {
#include "assets/m4a1_javelin_animation_07030_records.inc"
};

static u16 _gM4a1JavelinAnimation07030Indices[20] = {
#include "assets/m4a1_javelin_animation_07030_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation07030 = {
    _gM4a1JavelinAnimation07030Records,
    _gM4a1JavelinAnimation07030Indices,
    { NULL, _gM4a1JavelinAnimation07030Bank1, NULL, NULL, _gM4a1JavelinAnimation07030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation072D4Bank1[4] = {
#include "assets/m4a1_javelin_animation_072D4_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation072D4Bank4[55] = {
#include "assets/m4a1_javelin_animation_072D4_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation072D4Records[82] = {
#include "assets/m4a1_javelin_animation_072D4_records.inc"
};

static u16 _gM4a1JavelinAnimation072D4Indices[20] = {
#include "assets/m4a1_javelin_animation_072D4_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation072D4 = {
    _gM4a1JavelinAnimation072D4Records,
    _gM4a1JavelinAnimation072D4Indices,
    { NULL, _gM4a1JavelinAnimation072D4Bank1, NULL, NULL, _gM4a1JavelinAnimation072D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation074D4Bank1[3] = {
#include "assets/m4a1_javelin_animation_074D4_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation074D4Bank4[23] = {
#include "assets/m4a1_javelin_animation_074D4_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation074D4Records[76] = {
#include "assets/m4a1_javelin_animation_074D4_records.inc"
};

static u16 _gM4a1JavelinAnimation074D4Indices[20] = {
#include "assets/m4a1_javelin_animation_074D4_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation074D4 = {
    _gM4a1JavelinAnimation074D4Records,
    _gM4a1JavelinAnimation074D4Indices,
    { NULL, _gM4a1JavelinAnimation074D4Bank1, NULL, NULL, _gM4a1JavelinAnimation074D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation07828Bank1[8] = {
#include "assets/m4a1_javelin_animation_07828_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation07828Bank4[68] = {
#include "assets/m4a1_javelin_animation_07828_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation07828Records[101] = {
#include "assets/m4a1_javelin_animation_07828_records.inc"
};

static u16 _gM4a1JavelinAnimation07828Indices[20] = {
#include "assets/m4a1_javelin_animation_07828_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation07828 = {
    _gM4a1JavelinAnimation07828Records,
    _gM4a1JavelinAnimation07828Indices,
    { NULL, _gM4a1JavelinAnimation07828Bank1, NULL, NULL, _gM4a1JavelinAnimation07828Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation07ADCBank1[5] = {
#include "assets/m4a1_javelin_animation_07ADC_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation07ADCBank4[55] = {
#include "assets/m4a1_javelin_animation_07ADC_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation07ADCRecords[83] = {
#include "assets/m4a1_javelin_animation_07ADC_records.inc"
};

static u16 _gM4a1JavelinAnimation07ADCIndices[20] = {
#include "assets/m4a1_javelin_animation_07ADC_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation07ADC = {
    _gM4a1JavelinAnimation07ADCRecords,
    _gM4a1JavelinAnimation07ADCIndices,
    { NULL, _gM4a1JavelinAnimation07ADCBank1, NULL, NULL, _gM4a1JavelinAnimation07ADCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation07DFCBank1[6] = {
#include "assets/m4a1_javelin_animation_07DFC_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation07DFCBank4[66] = {
#include "assets/m4a1_javelin_animation_07DFC_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation07DFCRecords[96] = {
#include "assets/m4a1_javelin_animation_07DFC_records.inc"
};

static u16 _gM4a1JavelinAnimation07DFCIndices[20] = {
#include "assets/m4a1_javelin_animation_07DFC_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation07DFC = {
    _gM4a1JavelinAnimation07DFCRecords,
    _gM4a1JavelinAnimation07DFCIndices,
    { NULL, _gM4a1JavelinAnimation07DFCBank1, NULL, NULL, _gM4a1JavelinAnimation07DFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation085C0Bank1[18] = {
#include "assets/m4a1_javelin_animation_085C0_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation085C0Bank4[184] = {
#include "assets/m4a1_javelin_animation_085C0_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation085C0Records[239] = {
#include "assets/m4a1_javelin_animation_085C0_records.inc"
};

static u16 _gM4a1JavelinAnimation085C0Indices[20] = {
#include "assets/m4a1_javelin_animation_085C0_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation085C0 = {
    _gM4a1JavelinAnimation085C0Records,
    _gM4a1JavelinAnimation085C0Indices,
    { NULL, _gM4a1JavelinAnimation085C0Bank1, NULL, NULL, _gM4a1JavelinAnimation085C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation09858Bank1[29] = {
#include "assets/m4a1_javelin_animation_09858_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation09858Bank4[450] = {
#include "assets/m4a1_javelin_animation_09858_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation09858Records[633] = {
#include "assets/m4a1_javelin_animation_09858_records.inc"
};

static u16 _gM4a1JavelinAnimation09858Indices[20] = {
#include "assets/m4a1_javelin_animation_09858_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation09858 = {
    _gM4a1JavelinAnimation09858Records,
    _gM4a1JavelinAnimation09858Indices,
    { NULL, _gM4a1JavelinAnimation09858Bank1, NULL, NULL, _gM4a1JavelinAnimation09858Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0A3D0Bank1[12] = {
#include "assets/m4a1_javelin_animation_0A3D0_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0A3D0Bank4[266] = {
#include "assets/m4a1_javelin_animation_0A3D0_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0A3D0Records[412] = {
#include "assets/m4a1_javelin_animation_0A3D0_records.inc"
};

static u16 _gM4a1JavelinAnimation0A3D0Indices[20] = {
#include "assets/m4a1_javelin_animation_0A3D0_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0A3D0 = {
    _gM4a1JavelinAnimation0A3D0Records,
    _gM4a1JavelinAnimation0A3D0Indices,
    { NULL, _gM4a1JavelinAnimation0A3D0Bank1, NULL, NULL, _gM4a1JavelinAnimation0A3D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0AAECBank1[9] = {
#include "assets/m4a1_javelin_animation_0AAEC_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0AAECBank4[144] = {
#include "assets/m4a1_javelin_animation_0AAEC_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0AAECRecords[264] = {
#include "assets/m4a1_javelin_animation_0AAEC_records.inc"
};

static u16 _gM4a1JavelinAnimation0AAECIndices[20] = {
#include "assets/m4a1_javelin_animation_0AAEC_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0AAEC = {
    _gM4a1JavelinAnimation0AAECRecords,
    _gM4a1JavelinAnimation0AAECIndices,
    { NULL, _gM4a1JavelinAnimation0AAECBank1, NULL, NULL, _gM4a1JavelinAnimation0AAECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0AF6CBank1[6] = {
#include "assets/m4a1_javelin_animation_0AF6C_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0AF6CBank4[107] = {
#include "assets/m4a1_javelin_animation_0AF6C_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0AF6CRecords[143] = {
#include "assets/m4a1_javelin_animation_0AF6C_records.inc"
};

static u16 _gM4a1JavelinAnimation0AF6CIndices[20] = {
#include "assets/m4a1_javelin_animation_0AF6C_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0AF6C = {
    _gM4a1JavelinAnimation0AF6CRecords,
    _gM4a1JavelinAnimation0AF6CIndices,
    { NULL, _gM4a1JavelinAnimation0AF6CBank1, NULL, NULL, _gM4a1JavelinAnimation0AF6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0B144Bank1[3] = {
#include "assets/m4a1_javelin_animation_0B144_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0B144Bank4[32] = {
#include "assets/m4a1_javelin_animation_0B144_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0B144Records[57] = {
#include "assets/m4a1_javelin_animation_0B144_records.inc"
};

static u16 _gM4a1JavelinAnimation0B144Indices[20] = {
#include "assets/m4a1_javelin_animation_0B144_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0B144 = {
    _gM4a1JavelinAnimation0B144Records,
    _gM4a1JavelinAnimation0B144Indices,
    { NULL, _gM4a1JavelinAnimation0B144Bank1, NULL, NULL, _gM4a1JavelinAnimation0B144Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0B6A8Bank1[11] = {
#include "assets/m4a1_javelin_animation_0B6A8_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0B6A8Bank4[125] = {
#include "assets/m4a1_javelin_animation_0B6A8_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0B6A8Records[167] = {
#include "assets/m4a1_javelin_animation_0B6A8_records.inc"
};

static u16 _gM4a1JavelinAnimation0B6A8Indices[20] = {
#include "assets/m4a1_javelin_animation_0B6A8_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0B6A8 = {
    _gM4a1JavelinAnimation0B6A8Records,
    _gM4a1JavelinAnimation0B6A8Indices,
    { NULL, _gM4a1JavelinAnimation0B6A8Bank1, NULL, NULL, _gM4a1JavelinAnimation0B6A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0B89CBank1[3] = {
#include "assets/m4a1_javelin_animation_0B89C_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0B89CBank4[20] = {
#include "assets/m4a1_javelin_animation_0B89C_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0B89CRecords[76] = {
#include "assets/m4a1_javelin_animation_0B89C_records.inc"
};

static u16 _gM4a1JavelinAnimation0B89CIndices[20] = {
#include "assets/m4a1_javelin_animation_0B89C_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0B89C = {
    _gM4a1JavelinAnimation0B89CRecords,
    _gM4a1JavelinAnimation0B89CIndices,
    { NULL, _gM4a1JavelinAnimation0B89CBank1, NULL, NULL, _gM4a1JavelinAnimation0B89CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0BD1CBank1[8] = {
#include "assets/m4a1_javelin_animation_0BD1C_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0BD1CBank4[105] = {
#include "assets/m4a1_javelin_animation_0BD1C_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0BD1CRecords[139] = {
#include "assets/m4a1_javelin_animation_0BD1C_records.inc"
};

static u16 _gM4a1JavelinAnimation0BD1CIndices[20] = {
#include "assets/m4a1_javelin_animation_0BD1C_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0BD1C = {
    _gM4a1JavelinAnimation0BD1CRecords,
    _gM4a1JavelinAnimation0BD1CIndices,
    { NULL, _gM4a1JavelinAnimation0BD1CBank1, NULL, NULL, _gM4a1JavelinAnimation0BD1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0BEF8Bank1[2] = {
#include "assets/m4a1_javelin_animation_0BEF8_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0BEF8Bank4[17] = {
#include "assets/m4a1_javelin_animation_0BEF8_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0BEF8Records[76] = {
#include "assets/m4a1_javelin_animation_0BEF8_records.inc"
};

static u16 _gM4a1JavelinAnimation0BEF8Indices[20] = {
#include "assets/m4a1_javelin_animation_0BEF8_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0BEF8 = {
    _gM4a1JavelinAnimation0BEF8Records,
    _gM4a1JavelinAnimation0BEF8Indices,
    { NULL, _gM4a1JavelinAnimation0BEF8Bank1, NULL, NULL, _gM4a1JavelinAnimation0BEF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0C69CBank1[13] = {
#include "assets/m4a1_javelin_animation_0C69C_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0C69CBank4[175] = {
#include "assets/m4a1_javelin_animation_0C69C_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0C69CRecords[255] = {
#include "assets/m4a1_javelin_animation_0C69C_records.inc"
};

static u16 _gM4a1JavelinAnimation0C69CIndices[20] = {
#include "assets/m4a1_javelin_animation_0C69C_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0C69C = {
    _gM4a1JavelinAnimation0C69CRecords,
    _gM4a1JavelinAnimation0C69CIndices,
    { NULL, _gM4a1JavelinAnimation0C69CBank1, NULL, NULL, _gM4a1JavelinAnimation0C69CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0D144Bank1[19] = {
#include "assets/m4a1_javelin_animation_0D144_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0D144Bank4[269] = {
#include "assets/m4a1_javelin_animation_0D144_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0D144Records[336] = {
#include "assets/m4a1_javelin_animation_0D144_records.inc"
};

static u16 _gM4a1JavelinAnimation0D144Indices[20] = {
#include "assets/m4a1_javelin_animation_0D144_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0D144 = {
    _gM4a1JavelinAnimation0D144Records,
    _gM4a1JavelinAnimation0D144Indices,
    { NULL, _gM4a1JavelinAnimation0D144Bank1, NULL, NULL, _gM4a1JavelinAnimation0D144Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0E5F4Bank1[35] = {
#include "assets/m4a1_javelin_animation_0E5F4_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0E5F4Bank4[567] = {
#include "assets/m4a1_javelin_animation_0E5F4_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0E5F4Records[632] = {
#include "assets/m4a1_javelin_animation_0E5F4_records.inc"
};

static u16 _gM4a1JavelinAnimation0E5F4Indices[20] = {
#include "assets/m4a1_javelin_animation_0E5F4_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0E5F4 = {
    _gM4a1JavelinAnimation0E5F4Records,
    _gM4a1JavelinAnimation0E5F4Indices,
    { NULL, _gM4a1JavelinAnimation0E5F4Bank1, NULL, NULL, _gM4a1JavelinAnimation0E5F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0EADCBank1[8] = {
#include "assets/m4a1_javelin_animation_0EADC_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0EADCBank4[111] = {
#include "assets/m4a1_javelin_animation_0EADC_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0EADCRecords[159] = {
#include "assets/m4a1_javelin_animation_0EADC_records.inc"
};

static u16 _gM4a1JavelinAnimation0EADCIndices[20] = {
#include "assets/m4a1_javelin_animation_0EADC_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0EADC = {
    _gM4a1JavelinAnimation0EADCRecords,
    _gM4a1JavelinAnimation0EADCIndices,
    { NULL, _gM4a1JavelinAnimation0EADCBank1, NULL, NULL, _gM4a1JavelinAnimation0EADCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0EE3CBank1[6] = {
#include "assets/m4a1_javelin_animation_0EE3C_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0EE3CBank4[65] = {
#include "assets/m4a1_javelin_animation_0EE3C_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0EE3CRecords[113] = {
#include "assets/m4a1_javelin_animation_0EE3C_records.inc"
};

static u16 _gM4a1JavelinAnimation0EE3CIndices[20] = {
#include "assets/m4a1_javelin_animation_0EE3C_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0EE3C = {
    _gM4a1JavelinAnimation0EE3CRecords,
    _gM4a1JavelinAnimation0EE3CIndices,
    { NULL, _gM4a1JavelinAnimation0EE3CBank1, NULL, NULL, _gM4a1JavelinAnimation0EE3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0F4FCBank1[12] = {
#include "assets/m4a1_javelin_animation_0F4FC_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0F4FCBank4[168] = {
#include "assets/m4a1_javelin_animation_0F4FC_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0F4FCRecords[208] = {
#include "assets/m4a1_javelin_animation_0F4FC_records.inc"
};

static u16 _gM4a1JavelinAnimation0F4FCIndices[20] = {
#include "assets/m4a1_javelin_animation_0F4FC_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0F4FC = {
    _gM4a1JavelinAnimation0F4FCRecords,
    _gM4a1JavelinAnimation0F4FCIndices,
    { NULL, _gM4a1JavelinAnimation0F4FCBank1, NULL, NULL, _gM4a1JavelinAnimation0F4FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation0FE0CBank1[16] = {
#include "assets/m4a1_javelin_animation_0FE0C_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation0FE0CBank4[234] = {
#include "assets/m4a1_javelin_animation_0FE0C_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation0FE0CRecords[278] = {
#include "assets/m4a1_javelin_animation_0FE0C_records.inc"
};

static u16 _gM4a1JavelinAnimation0FE0CIndices[20] = {
#include "assets/m4a1_javelin_animation_0FE0C_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation0FE0C = {
    _gM4a1JavelinAnimation0FE0CRecords,
    _gM4a1JavelinAnimation0FE0CIndices,
    { NULL, _gM4a1JavelinAnimation0FE0CBank1, NULL, NULL, _gM4a1JavelinAnimation0FE0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation106C4Bank1[18] = {
#include "assets/m4a1_javelin_animation_106C4_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation106C4Bank4[201] = {
#include "assets/m4a1_javelin_animation_106C4_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation106C4Records[283] = {
#include "assets/m4a1_javelin_animation_106C4_records.inc"
};

static u16 _gM4a1JavelinAnimation106C4Indices[20] = {
#include "assets/m4a1_javelin_animation_106C4_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation106C4 = {
    _gM4a1JavelinAnimation106C4Records,
    _gM4a1JavelinAnimation106C4Indices,
    { NULL, _gM4a1JavelinAnimation106C4Bank1, NULL, NULL, _gM4a1JavelinAnimation106C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation10E64Bank1[16] = {
#include "assets/m4a1_javelin_animation_10E64_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation10E64Bank4[157] = {
#include "assets/m4a1_javelin_animation_10E64_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation10E64Records[263] = {
#include "assets/m4a1_javelin_animation_10E64_records.inc"
};

static u16 _gM4a1JavelinAnimation10E64Indices[20] = {
#include "assets/m4a1_javelin_animation_10E64_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation10E64 = {
    _gM4a1JavelinAnimation10E64Records,
    _gM4a1JavelinAnimation10E64Indices,
    { NULL, _gM4a1JavelinAnimation10E64Bank1, NULL, NULL, _gM4a1JavelinAnimation10E64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1JavelinAnimation11838Bank1[25] = {
#include "assets/m4a1_javelin_animation_11838_bank1.inc"
};

static AnimationPackedRotation _gM4a1JavelinAnimation11838Bank4[223] = {
#include "assets/m4a1_javelin_animation_11838_bank4.inc"
};

static AnimationRecord _gM4a1JavelinAnimation11838Records[311] = {
#include "assets/m4a1_javelin_animation_11838_records.inc"
};

static u16 _gM4a1JavelinAnimation11838Indices[20] = {
#include "assets/m4a1_javelin_animation_11838_indices.inc"
};

static AnimationSet _gM4a1JavelinAnimation11838 = {
    _gM4a1JavelinAnimation11838Records,
    _gM4a1JavelinAnimation11838Indices,
    { NULL, _gM4a1JavelinAnimation11838Bank1, NULL, NULL, _gM4a1JavelinAnimation11838Bank4, NULL, NULL, NULL },
};

AnimationBank D_m4a1_javelin_8012EA20 = { { {
    NULL,
    &_gM4a1JavelinAnimation03710,
    &_gM4a1JavelinAnimation106C4,
    &_gM4a1JavelinAnimation10E64,
    &_gM4a1JavelinAnimation11838,
    &_gM4a1JavelinAnimation04614,
    &_gM4a1JavelinAnimation04E78,
    &_gM4a1JavelinAnimation0F4FC,
    &_gM4a1JavelinAnimation0FE0C,
    &_gM4a1JavelinAnimation0BEF8,
    &_gM4a1JavelinAnimation0EADC,
    &_gM4a1JavelinAnimation0EE3C,
    &_gM4a1JavelinAnimation0D144,
    &_gM4a1JavelinAnimation0C69C,
    &_gM4a1JavelinAnimation0E5F4,
    &_gM4a1JavelinAnimation0E5F4,
    &_gM4a1JavelinAnimation07ADC,
    &_gM4a1JavelinAnimation07DFC,
    &_gM4a1JavelinAnimation085C0,
    &_gM4a1JavelinAnimation03DB4,
    &_gM4a1JavelinAnimation0E5F4,
    &_gM4a1JavelinAnimation03710,
    &_gM4a1JavelinAnimation03710,
    &_gM4a1JavelinAnimation09858,
    &_gM4a1JavelinAnimation0AAEC,
    &_gM4a1JavelinAnimation0A3D0,
    &_gM4a1JavelinAnimation06B5C,
    &_gM4a1JavelinAnimation06D58,
    &_gM4a1JavelinAnimation07030,
    &_gM4a1JavelinAnimation072D4,
    &_gM4a1JavelinAnimation074D4,
    &_gM4a1JavelinAnimation07828,
    &_gM4a1JavelinAnimation0AF6C,
    &_gM4a1JavelinAnimation0B144,
    &_gM4a1JavelinAnimation0AF6C,
    &_gM4a1JavelinAnimation0B144,
    &_gM4a1JavelinAnimation058E8,
    &_gM4a1JavelinAnimation06070,
    &_gM4a1JavelinAnimation066D4,
    &_gM4a1JavelinAnimation06344,
    &_gM4a1JavelinAnimation0518C,
    &_gM4a1JavelinAnimation03710,
    &_gM4a1JavelinAnimation0B6A8,
    &_gM4a1JavelinAnimation0B89C,
    &_gM4a1JavelinAnimation0BD1C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

/// A word between the bank and the work state that nothing refers to. Its
/// value is not zero and its purpose is not established.
u32 D_m4a1_javelin_8012EB5C = 0x102232DD;

u16     D_m4a1_javelin_8012EB60 = 0;
u16     D_m4a1_javelin_8012EB62 = 0;
s16     D_m4a1_javelin_8012EB64 = 0;
s16     D_m4a1_javelin_8012EB66 = 0;
SVECTOR D_m4a1_javelin_8012EB68 = { 0, 0, 0, 0 };
s32     D_m4a1_javelin_8012EB70 = 0;
