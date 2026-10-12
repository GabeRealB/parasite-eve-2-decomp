#include "weapons/m4a1_javelin.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

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
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

#include "overlay.h"
#include "types.h"

static u16     _gM4a1JavelinBeamScreenAngle;
static u16     _gM4a1JavelinGroundBeamScreenAngle;
static s16     _gM4a1JavelinBeamAngleRefreshPending;
static s16     _gM4a1JavelinGroundBeamAngleRefreshPending;
static SVECTOR _gM4a1JavelinTrackedImpactPoint;
static bool    _gM4a1JavelinTrackedImpactValid;

/// Read-only XYZ translation words from the launch sprite's `GfxCoord::workm.t`.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const long
/// Reads one cached XYZ translation word for Javelin's sprite drawer.
///
/// `source` borrows three readable signed 32-bit words from `GfxCoord::workm.t`;
/// `componentIndex` is 0 (X), 1 (Y) or 2 (Z), in elements. Values are integer
/// game coordinates in `GsWSMATRIX` input space; the drawer projects their low
/// 16 bits. Each argument is evaluated once, no caller identifiers are captured,
/// and the source is neither modified nor retained. Bind alongside the source
/// type before the header; both remain bound through the drawer inclusion.
#define SPRITE_QUAD_TRANSLATION_COMPONENT(source, componentIndex) ((source)[(componentIndex)])
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

/// Guide-beam origin in the weapon model's local coordinate units.
static SVECTOR _gM4a1JavelinGuideBeamOffset = { 0, 0x200, 0x20, 0 };

/// Ground-probe displacement along view +Y, in game coordinate units.
///
/// Rotated into world space before adding it to each guide-beam sample.
static const SVECTOR _gM4a1JavelinGroundProbeOffset SECTION(".data") = { 0, 0x800, 0, 0 };

/// Controls for the six beam and ground-glow segments, indexed from the muzzle.
///
/// Drawing visits indices 5..0: the far segment refreshes the direction and
/// caps its far end; the muzzle segment caps its near end.
static u16 _gM4a1JavelinBeamSegmentFlags[] = {
    M4A1_JAVELIN_SEGMENT_CAP_NEAR_END, 0, 0, 0, 0, M4A1_JAVELIN_SEGMENT_REFRESH_ANGLE
};

/// Guide-beam retirement colours packed as 0x0RGB, darkest to brightest.
///
/// Beam retirement visits indices 3..0; ground glow uses that index >> 1,
/// selecting only entries 1 and 0. Without ground tracing the beam retains
/// its fixed bright colour instead of reading this ramp.
static const u16 _gM4a1JavelinBeamFadeColours[] SECTION(".data") = { 0x12, 0x124, 0x248, 0x36C };

static void _m4a1JavelinSetTrackedImpactPoint(const long* worldTranslation);

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
        M4A1_JAVELIN_BEAM_SEGMENT_COUNT     = ARRAY_SIZE(_gM4a1JavelinBeamSegmentFlags),
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
            coord->coord.t[0]   = _gM4a1JavelinGuideBeamOffset.vx;
            coord->coord.t[1]   = _gM4a1JavelinGuideBeamOffset.vy;
            coord->coord.t[2]   = _gM4a1JavelinGuideBeamOffset.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state                        = M4A1_JAVELIN_GUIDE_MUZZLE_FLARE;
            work->move.vy                      = M4A1_JAVELIN_BEAM_LENGTH;
            work->move.vx                      = 0;
            work->move.vz                      = 0;
            _gM4a1JavelinTrackedImpactPoint.vx = 0;
            _gM4a1JavelinTrackedImpactPoint.vy = 0;
            _gM4a1JavelinTrackedImpactPoint.vz = 0;
            _gM4a1JavelinTrackedImpactValid    = false;
            work->period                       = M4A1_JAVELIN_FLARE_INITIAL_RADIUS;
            work->step                         = ARRAY_SIZE(_gM4a1JavelinBeamFadeColours) - 1;
            _gM4a1JavelinGroundBeamScreenAngle = 0;
            _gM4a1JavelinBeamScreenAngle       = 0;
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
            _gM4a1JavelinBeamAngleRefreshPending       = false;
            lightCoord->composeStamp                   = GRAPHICS_COORD_DIRTY;
            _gM4a1JavelinGroundBeamAngleRefreshPending = false;
            if (_gM4a1JavelinTrackedImpactValid != false) {
                nearPoint.vx = coord->workm.t[0];
                nearPoint.vy = coord->workm.t[1];
                nearPoint.vz = coord->workm.t[2];
                farPoint.vx  = _gM4a1JavelinTrackedImpactPoint.vx;
                farPoint.vy  = _gM4a1JavelinTrackedImpactPoint.vy;
                farPoint.vz  = _gM4a1JavelinTrackedImpactPoint.vz;
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
                gte_ldv0(&_gM4a1JavelinGroundProbeOffset);
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
                    _m4a1JavelinDrawBeamSegment(&nearPoint, &farPoint, _gM4a1JavelinBeamSegmentFlags[segmentIndex],
                                                _gM4a1JavelinBeamFadeColours[work->step]);
                    gte_SetRotMatrix(&gGfxViewCoord.workm);
                    gte_ldv0(&_gM4a1JavelinGroundProbeOffset);
                    gte_rtv0();
                    gte_stsv(&nearGroundPoint);
                    nearGroundPoint.vx = nearGroundPoint.vx + nearPoint.vx;
                    nearGroundPoint.vy = nearGroundPoint.vy + nearPoint.vy;
                    nearGroundPoint.vz = nearGroundPoint.vz + nearPoint.vz;
                    nearPoint.vy       = nearPoint.vy - M4A1_JAVELIN_PROBE_RAISE;
                    if (worldCollisionProbeGridSegment(&nearGroundPoint, &nearPoint, &nearGroundPoint, NULL) == 1) {
                        if (segmentIndex < previousHitIndex) {
                            _m4a1JavelinDrawGroundBeamSegment(&nearGroundPoint, &farGroundPoint, _gM4a1JavelinBeamSegmentFlags[segmentIndex],
                                                              _gM4a1JavelinBeamFadeColours[work->step >> 1]);
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
                    _m4a1JavelinDrawBeamSegment(&nearPoint, &farPoint, _gM4a1JavelinBeamSegmentFlags[segmentIndex], M4A1_JAVELIN_BEAM_BASE_RGB444);
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
            if ((segmentFlags & M4A1_JAVELIN_SEGMENT_REFRESH_ANGLE) || _gM4a1JavelinBeamAngleRefreshPending != false) {
                beamAngle                            = ratan2(line->y1 - line->y0, line->x0 - line->x1);
                _gM4a1JavelinBeamScreenAngle         = beamAngle;
                _gM4a1JavelinBeamAngleRefreshPending = false;
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
                beamAngle = _gM4a1JavelinBeamScreenAngle;
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
            _gM4a1JavelinBeamAngleRefreshPending = true;
        }
    } else {
        _gM4a1JavelinBeamAngleRefreshPending = true;
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
    if (scratch->flag >= 0) {
        gte_stszotz(&scratch->otz0);
        scratch->otz0++;
        gte_ldv0(farPoint);
        gte_rtps();
        gte_stsxy(&scratch->sx1);
        gte_stflg(&scratch->flag);
        if (scratch->flag >= 0) {
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
            if ((segmentFlags & M4A1_JAVELIN_SEGMENT_REFRESH_ANGLE) || _gM4a1JavelinGroundBeamAngleRefreshPending != false) {
                beamAngle                                  = ratan2(line->y1 - line->y0, line->x0 - line->x1);
                _gM4a1JavelinGroundBeamScreenAngle         = beamAngle;
                _gM4a1JavelinGroundBeamAngleRefreshPending = false;
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
                beamAngle = _gM4a1JavelinGroundBeamScreenAngle;
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
        } else {
            _gM4a1JavelinGroundBeamAngleRefreshPending = true;
        }
    } else {
        _gM4a1JavelinGroundBeamAngleRefreshPending = true;
    }
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
/// Inclusive bottom texel row of the eight-frame flash strip's square cells, relative to its texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
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
        _gM4a1JavelinTrackedImpactValid = false;
        return;
    }
    _gM4a1JavelinTrackedImpactPoint.vx = worldTranslation[0];
    _gM4a1JavelinTrackedImpactPoint.vy = worldTranslation[1];
    _gM4a1JavelinTrackedImpactValid    = true;
    _gM4a1JavelinTrackedImpactPoint.vz = worldTranslation[2];
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

void m4a1JavelinAttackState(Task* playerTask)
{
    enum {
        M4A1_JAVELIN_PHASE_PREPARE             = 0,
        M4A1_JAVELIN_PHASE_WAIT_READY          = 1,
        M4A1_JAVELIN_PHASE_SELECT_ATTACK       = 2,
        M4A1_JAVELIN_PHASE_BURST               = 3,
        M4A1_JAVELIN_PHASE_IMPACT              = 4,
        M4A1_JAVELIN_PHASE_BEAM_DELAY          = 5,
        M4A1_JAVELIN_PHASE_BEAM_ACTIVE         = 6,
        M4A1_JAVELIN_PHASE_RECOVER             = 7,
        M4A1_JAVELIN_PLAYER_ATTACK_STATE       = 4,
        M4A1_JAVELIN_ANIMATION_READY           = 9,
        M4A1_JAVELIN_ANIMATION_PRIMARY         = 0xA,
        M4A1_JAVELIN_ANIMATION_SECONDARY       = 0xB,
        M4A1_JAVELIN_READY_BLEND_FRAMES        = 1,
        M4A1_JAVELIN_MOVING_READY_BLEND_FRAMES = 8,
        M4A1_JAVELIN_IMPACT_SOUND              = SOUND_COMMON(0x17),
        M4A1_JAVELIN_BURST_ROUNDS              = 3,
        M4A1_JAVELIN_BURST_CANCEL_FRAMES       = 9,
        M4A1_JAVELIN_SECONDARY_CANCEL_FRAMES   = 0x1C,
        M4A1_JAVELIN_BURST_DELAY_FRAMES        = 3,
        M4A1_JAVELIN_RECOVERY_COOLDOWN_FRAMES  = 0xC,
        M4A1_JAVELIN_BEAM_DELAY_FRAMES         = 6,
        M4A1_JAVELIN_BEAM_ACTIVE_FRAMES        = 0x1C,
        M4A1_JAVELIN_ATTACK_SCRATCH_BYTES      = 0x58,
        M4A1_JAVELIN_RIFLE_KEY_BASE            = WORLD_COLLISION_CONTACT_ATTACK | (29 << 8),
        M4A1_JAVELIN_BEAM_KEY                  = WORLD_COLLISION_CONTACT_ATTACK | (29 << 8) | 0x1F,
        M4A1_JAVELIN_WEAPON_ID                 = 29,
        M4A1_JAVELIN_PRIMARY_SOUND             = SOUND_WEAPON(29, 4),
        M4A1_JAVELIN_BEAM_SOUND                = SOUND_WEAPON(29, 5),
        M4A1_JAVELIN_BURST_IMPACT_TICKS_LEFT   = 2,
        M4A1_JAVELIN_SECONDARY_TURN_RATE_INDEX = 2,
    };
    GameActor*  actor;
    GfxCoord*   rootCoord;
    GfxCoord*   impactCoord;
    EffectWork* effectWork;
    s32         readyBlendFrames;
    s32         burstDelay;
    s32         beamTicksLeft;
    u16         roundsLeft;

    // Retain the full reservation; the trailing eight bytes have no recovered accesses.
    SCRATCH_STACK_RESERVE_BYTES(M4A1_JAVELIN_ATTACK_SCRATCH_BYTES);
    rootCoord           = playerTask->extra.tmd->coords;
    actor               = playerTask->work;
    impactCoord         = SCRATCH_STACK_CURSOR(GfxCoord);
    impactCoord->parent = NULL;

    switch (actor->statePhase) {
        case M4A1_JAVELIN_PHASE_PREPARE:
            readyBlendFrames                                      = M4A1_JAVELIN_READY_BLEND_FRAMES;
            actor->state                                          = M4A1_JAVELIN_PLAYER_ATTACK_STATE;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += readyBlendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M4A1_JAVELIN_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M4A1_JAVELIN_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M4A1_JAVELIN_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M4A1_JAVELIN_PHASE_SELECT_ATTACK:
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = M4A1_JAVELIN_PHASE_BURST;
                actor->attackCancelTicks                              = M4A1_JAVELIN_BURST_CANCEL_FRAMES;
                actor->turnRateIndex                                  = 0;
                actor->stateTimer                                     = 0;
                actor->actionValue                                    = M4A1_JAVELIN_BURST_ROUNDS;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | M4A1_JAVELIN_RIFLE_KEY_BASE;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase                                     = M4A1_JAVELIN_PHASE_BEAM_DELAY;
                actor->turnRateIndex                                  = M4A1_JAVELIN_SECONDARY_TURN_RATE_INDEX;
                actor->attackCancelTicks                              = M4A1_JAVELIN_SECONDARY_CANCEL_FRAMES;
                actor->stateTimer                                     = M4A1_JAVELIN_BEAM_DELAY_FRAMES;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = M4A1_JAVELIN_BEAM_KEY;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(M4A1_JAVELIN_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                effectWork = effectSpawn(EFFECT_JAVELIN_GUIDE_BEAM,
                                         actor->equipmentTasks[1]->extra.tmd->coords,
                                         0x1D, NULL);
                if (effectWork != NULL) {
                    taskReparent(actor->equipmentTasks[1], effectWork->task);
                }
                worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_JAVELIN_BEAM_SOUND, 1);
                playerActorPlayChildSlotsWithBlend(playerTask, M4A1_JAVELIN_ANIMATION_SECONDARY, 0, 3);
                break;
            }
            /* fallthrough */
        case M4A1_JAVELIN_PHASE_BURST:
            roundsLeft = actor->actionValue;
            if (actor->actionValue != 0) {
                burstDelay = actor->stateTimer;
                if (burstDelay == 0) {
                    actor->actionValue                                    = roundsLeft - 1;
                    actor->stateTimer                                     = M4A1_JAVELIN_BURST_DELAY_FRAMES;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(M4A1_JAVELIN_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_JAVELIN_PRIMARY_SOUND, 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                M4A1_JAVELIN_WEAPON_ID, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, M4A1_JAVELIN_ANIMATION_PRIMARY, 0, 2);
                } else {
                    burstDelay--;
                    actor->stateTimer = burstDelay;
                    if (burstDelay == M4A1_JAVELIN_BURST_IMPACT_TICKS_LEFT) {
                        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                        if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                            effectSpawn(EFFECT_IMPACT_SPARK, impactCoord, 0, NULL);
                            worldCoordPlaySound(impactCoord, M4A1_JAVELIN_IMPACT_SOUND, 1);
                        }
                    }
                }
                break;
            }
            /* fallthrough */
        case M4A1_JAVELIN_PHASE_IMPACT:
            actor->statePhase                                     = M4A1_JAVELIN_PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                effectSpawn(EFFECT_IMPACT_SPARK, impactCoord, 0, NULL);
                worldCoordPlaySound(impactCoord, M4A1_JAVELIN_IMPACT_SOUND, 1);
            }
            break;
        case M4A1_JAVELIN_PHASE_BEAM_DELAY:
        case M4A1_JAVELIN_PHASE_BEAM_ACTIVE:
            beamTicksLeft     = actor->stateTimer - 1;
            actor->stateTimer = beamTicksLeft;
            if (beamTicksLeft == 0) {
                if (actor->statePhase == M4A1_JAVELIN_PHASE_BEAM_DELAY) {
                    actor->statePhase                                     = M4A1_JAVELIN_PHASE_BEAM_ACTIVE;
                    actor->stateTimer                                     = M4A1_JAVELIN_BEAM_ACTIVE_FRAMES;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    actor->statePhase                                     = M4A1_JAVELIN_PHASE_RECOVER;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
            }
            // The beam tracks this frame's contact; no contact clears the cached point.
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                _m4a1JavelinSetTrackedImpactPoint(impactCoord->workm.t);
                effectWork = effectSpawn(EFFECT_M4A1_JAVELIN_CONTACT_FLASH, impactCoord, 0, NULL);
                if (effectWork != NULL) {
                    taskReparent(actor->equipmentTasks[1], effectWork->task);
                }
            } else {
                _m4a1JavelinSetTrackedImpactPoint(NULL);
            }
            break;
        case M4A1_JAVELIN_PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_JAVELIN_RECOVERY_COOLDOWN_FRAMES;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(M4A1_JAVELIN_ATTACK_SCRATCH_BYTES);
}

/// Identity initial pose and self-parented root of the rigid weapon model.
static const TmdBone _gM4a1JavelinWeaponModelSkeleton[] SECTION(".data") = {
#include "assets/m4a1_javelin_model_02D7C_skeleton.inc"
};

/// Number of part-local vertices belonging to the weapon model's single part.
static const u32 _gM4a1JavelinWeaponModelPartVerts[] SECTION(".data") = {
#include "assets/m4a1_javelin_model_02D7C_partVerts.inc"
};

/// Weapon-model vertices in integer part-local coordinate units.
static SVECTOR _gM4a1JavelinWeaponModelVerts[] = {
#include "assets/m4a1_javelin_model_02D7C_verts.inc"
};

/// Weapon-model lighting normals in part-local axes, with 4096 representing one.
static SVECTOR _gM4a1JavelinWeaponModelNormals[] = {
#include "assets/m4a1_javelin_model_02D7C_normals.inc"
};

/// Writable draw stream for two textured Gouraud triangles and 69 quads.
///
/// Geometry references are byte offsets into the vertex and normal arrays;
/// model creation replaces the draw-handler words in place.
static u32 _gM4a1JavelinWeaponModelStream[] = {
#include "assets/m4a1_javelin_model_02D7C_stream.inc"
};

TmdSource gM4a1JavelinWeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = 2 * sizeof(POLY_GT3) + 69 * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM4a1JavelinWeaponModelSkeleton),
    .partVertexCounts    = _gM4a1JavelinWeaponModelPartVerts,
    .verts               = _gM4a1JavelinWeaponModelVerts,
    .normals             = _gM4a1JavelinWeaponModelNormals,
    .skeleton            = _gM4a1JavelinWeaponModelSkeleton,
    .stream              = _gM4a1JavelinWeaponModelStream,
};

/// Translation/rotation poses for the standing-idle clip's encoding-1 tracks.
static const AnimationPackedPose _gM4a1JavelinIdleAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_03710_bank1.inc"
};

/// Packed rotation poses for the standing-idle clip's encoding-4 tracks.
static const AnimationPackedRotation _gM4a1JavelinIdleAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_03710_bank4.inc"
};

/// Keyframes and playback controls for the nineteen-part standing-idle clip.
static const AnimationRecord _gM4a1JavelinIdleAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_03710_records.inc"
};

/// Absolute record starts for the idle clip's nineteen model-part tracks.
///
/// The exported final halfword is retained but is outside those track indices.
static const u16 _gM4a1JavelinIdleAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_03710_indices.inc"
};

/// Standing-idle clip for the player while the M4A1 Javelin bank is loaded.
///
/// Native bank entry 1; entries 21, 22 and 41 alias this same clip. Playback
/// borrows nineteen part tracks and read-only pose banks 1 and 4.
static AnimationSet _gM4a1JavelinIdleAnimation = {
    .records           = _gM4a1JavelinIdleAnimationRecords,
    .trackStartIndices = _gM4a1JavelinIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1JavelinIdleAnimationBank1, NULL, NULL, _gM4a1JavelinIdleAnimationBank4, NULL, NULL, NULL },
};

/// Translation/rotation poses for the unarmed-walk clip's encoding-1 tracks.
static const AnimationPackedPose _gM4a1JavelinUnarmedWalkAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_03DB4_bank1.inc"
};

/// Packed rotation poses for the unarmed-walk clip's encoding-4 tracks.
static const AnimationPackedRotation _gM4a1JavelinUnarmedWalkAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_03DB4_bank4.inc"
};

/// Keyframes and playback controls for the nineteen-part unarmed-walk clip.
static const AnimationRecord _gM4a1JavelinUnarmedWalkAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_03DB4_records.inc"
};

/// Absolute record starts for the unarmed-walk clip's nineteen model-part tracks.
///
/// The exported final halfword is retained but is outside those track indices.
static const u16 _gM4a1JavelinUnarmedWalkAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_03DB4_indices.inc"
};

/// Forward-walk clip used when the player's held weapon model is absent.
///
/// Native M4A1 Javelin bank entry 19; borrows nineteen part tracks and
/// read-only pose banks 1 and 4 while this weapon package stays loaded.
static AnimationSet _gM4a1JavelinUnarmedWalkAnimation = {
    .records           = _gM4a1JavelinUnarmedWalkAnimationRecords,
    .trackStartIndices = _gM4a1JavelinUnarmedWalkAnimationIndices,
    .poseBanks         = { NULL, _gM4a1JavelinUnarmedWalkAnimationBank1, NULL, NULL, _gM4a1JavelinUnarmedWalkAnimationBank4, NULL, NULL, NULL },
};

/// Translation/rotation poses for the negative-turn clip's encoding-1 tracks.
static const AnimationPackedPose _gM4a1JavelinNegativeTurnAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_04614_bank1.inc"
};

/// Packed rotation poses for the negative-turn clip's encoding-4 tracks.
static const AnimationPackedRotation _gM4a1JavelinNegativeTurnAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_04614_bank4.inc"
};

/// Keyframes and playback controls for the nineteen-part negative-turn clip.
static const AnimationRecord _gM4a1JavelinNegativeTurnAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_04614_records.inc"
};

/// Absolute record starts for the negative-turn clip's nineteen model-part tracks.
///
/// The exported final halfword is retained but is outside those track indices.
static const u16 _gM4a1JavelinNegativeTurnAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_04614_indices.inc"
};

/// Stationary negative-turn clip for the player with the M4A1 Javelin equipped.
///
/// Native bank entry 5, selected for a negative turn sign; borrows nineteen
/// part tracks and read-only pose banks 1 and 4 while the package stays loaded.
static AnimationSet _gM4a1JavelinNegativeTurnAnimation = {
    .records           = _gM4a1JavelinNegativeTurnAnimationRecords,
    .trackStartIndices = _gM4a1JavelinNegativeTurnAnimationIndices,
    .poseBanks         = { NULL, _gM4a1JavelinNegativeTurnAnimationBank1, NULL, NULL, _gM4a1JavelinNegativeTurnAnimationBank4, NULL, NULL, NULL },
};

/// Pose encodings used as bank indices by these player-rig clips.
enum {
    M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Positive-turn translation/rotation poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinPositiveTurnAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_04E78_bank1.inc"
};

/// Positive-turn rotation-only poses, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinPositiveTurnAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_04E78_bank4.inc"
};

/// Keyframes and loop controls for the nineteen-part positive-turn clip.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gM4a1JavelinPositiveTurnAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_04E78_records.inc"
};

/// Absolute record starts for the positive-turn clip's nineteen model-part tracks.
///
/// The exported final halfword is retained outside the track-index domain; its role is unproven.
static const u16 _gM4a1JavelinPositiveTurnAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_04E78_indices.inc"
};

/// Looping positive-turn clip for the player with the M4A1 Javelin equipped.
///
/// Native bank entry 6 serves stationary turns and scripted positive-yaw turns.
/// Its nineteen tracks borrow read-only, word-aligned pose banks while the package is loaded.
static AnimationSet _gM4a1JavelinPositiveTurnAnimation = {
    .records           = _gM4a1JavelinPositiveTurnAnimationRecords,
    .trackStartIndices = _gM4a1JavelinPositiveTurnAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinPositiveTurnAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinPositiveTurnAnimationBank4,
    },
};

/// Item-use translation/rotation poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinItemUseAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0518C_bank1.inc"
};

/// Item-use rotation-only poses, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinItemUseAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0518C_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part item-use clip.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gM4a1JavelinItemUseAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0518C_records.inc"
};

/// Absolute record starts for the item-use clip's nineteen model-part tracks.
///
/// The exported final halfword is retained outside the track-index domain; its role is unproven.
static const u16 _gM4a1JavelinItemUseAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0518C_indices.inc"
};

/// Item-use presentation clip for the player with the M4A1 Javelin equipped.
///
/// Native bank entry 40 stops at its final pose before aim locomotion resumes.
/// Its nineteen tracks borrow read-only, word-aligned pose banks while the package is loaded.
static AnimationSet _gM4a1JavelinItemUseAnimation = {
    .records           = _gM4a1JavelinItemUseAnimationRecords,
    .trackStartIndices = _gM4a1JavelinItemUseAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinItemUseAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinItemUseAnimationBank4,
    },
};

/// Stair-ascent translation/rotation poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinStairAscentAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_058E8_bank1.inc"
};

/// Stair-ascent rotation-only poses, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinStairAscentAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_058E8_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for the nineteen-part stair-ascent clip.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gM4a1JavelinStairAscentAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_058E8_records.inc"
};

/// Absolute record starts for the stair-ascent clip's nineteen model-part tracks.
///
/// The exported final halfword is retained outside the track-index domain; its role is unproven.
static const u16 _gM4a1JavelinStairAscentAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_058E8_indices.inc"
};

/// Looping stair-ascent clip for the player with the M4A1 Javelin equipped.
///
/// Native bank entry 36 supplies footstep cues that gate each scripted upward stride.
/// Its nineteen tracks borrow read-only, word-aligned pose banks while the package is loaded.
static AnimationSet _gM4a1JavelinStairAscentAnimation = {
    .records           = _gM4a1JavelinStairAscentAnimationRecords,
    .trackStartIndices = _gM4a1JavelinStairAscentAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinStairAscentAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinStairAscentAnimationBank4,
    },
};

/// Stair-descent translation/rotation poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinStairDescentAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06070_bank1.inc"
};

/// Stair-descent rotation-only poses, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinStairDescentAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06070_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for the nineteen-part stair-descent clip.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gM4a1JavelinStairDescentAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06070_records.inc"
};

/// Absolute record starts for the stair-descent clip's nineteen model-part tracks.
///
/// The exported final halfword is retained outside the track-index domain; its role is unproven.
static const u16 _gM4a1JavelinStairDescentAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06070_indices.inc"
};

/// Looping stair-descent clip for the player with the M4A1 Javelin equipped.
///
/// Native bank entry 37 supplies footstep cues that gate each scripted downward stride.
/// Its nineteen tracks borrow read-only, word-aligned pose banks while the package is loaded.
static AnimationSet _gM4a1JavelinStairDescentAnimation = {
    .records           = _gM4a1JavelinStairDescentAnimationRecords,
    .trackStartIndices = _gM4a1JavelinStairDescentAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinStairDescentAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinStairDescentAnimationBank4,
    },
};

/// Odd-ascent finish translation/rotation poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinStairAscentFinishOddAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06344_bank1.inc"
};

/// Odd-ascent finish rotation-only poses, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinStairAscentFinishOddAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06344_bank4.inc"
};

/// Keyframes, footstep cues and stop controls for the nineteen-part odd-ascent finish.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gM4a1JavelinStairAscentFinishOddAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06344_records.inc"
};

/// Absolute record starts for the odd-ascent finish's nineteen model-part tracks.
///
/// The exported final halfword is retained outside the track-index domain; its role is unproven.
static const u16 _gM4a1JavelinStairAscentFinishOddAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06344_indices.inc"
};

/// Finishes an odd-length stair ascent with the M4A1 Javelin equipped.
///
/// Native bank entry 39 is selected from the requested stair count's saved parity.
/// Its nineteen tracks stop at their final poses and borrow read-only, word-aligned
/// pose banks while the package is loaded.
static AnimationSet _gM4a1JavelinStairAscentFinishOddAnimation = {
    .records           = _gM4a1JavelinStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM4a1JavelinStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinStairAscentFinishOddAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinStairAscentFinishOddAnimationBank4,
    },
};

/// Even-ascent finish translation/rotation poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinStairAscentFinishEvenAnimationBank1[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_066D4_bank1.inc"
};

/// Even-ascent finish rotation-only poses, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinStairAscentFinishEvenAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_066D4_bank4.inc"
};

/// Keyframes, footstep cues and stop controls for the nineteen-part even-ascent finish.
///
/// Pose offsets count four-byte words; keyframe durations count normal-rate frames.
static const AnimationRecord _gM4a1JavelinStairAscentFinishEvenAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_066D4_records.inc"
};

/// Absolute record starts for the even-ascent finish's nineteen model-part tracks.
///
/// The exported final halfword is retained outside the track-index domain; its role is unproven.
static const u16 _gM4a1JavelinStairAscentFinishEvenAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_066D4_indices.inc"
};

/// Finishes an even-length stair ascent with the M4A1 Javelin equipped.
///
/// Native bank entry 38 is selected from the requested stair count's saved parity.
/// Its nineteen tracks stop at their final poses and borrow read-only, word-aligned
/// pose banks while the package is loaded.
static AnimationSet _gM4a1JavelinStairAscentFinishEvenAnimation = {
    .records           = _gM4a1JavelinStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM4a1JavelinStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinStairAscentFinishEvenAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinStairAscentFinishEvenAnimationBank4,
    },
};

/// Family-0 Parasite Energy release poses for player tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyReleaseFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_06B5C_bank1.inc"
};

/// Family-0 Parasite Energy release rotations for player tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyReleaseFamily0AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06B5C_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part family-0 release clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track ends with a stop control. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyReleaseFamily0AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06B5C_records.inc"
};

/// Absolute record starts for the family-0 release clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyReleaseFamily0AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06B5C_indices.inc"
};

/// Parasite Energy family 0 release clip with the M4A1 Javelin equipped.
///
/// Native bank entry 26. Entered with a six-frame blend when the attachment
/// begins release.
/// Its nineteen tracks stop at their final poses before charge playback.
/// Family 0 selects spell IDs 300..600 whose tens digit is not 3, or item IDs
/// at least 601 whose tens digit is 1.
/// The borrowed arrays and word-aligned pose banks remain live while the
/// package is loaded.
static AnimationSet _gM4a1JavelinParasiteEnergyReleaseFamily0Animation = {
    .records           = _gM4a1JavelinParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyReleaseFamily0AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyReleaseFamily0AnimationBank4,
    },
};

/// Family-0 Parasite Energy charge poses for player tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyChargeFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_06D58_bank1.inc"
};

/// Family-0 Parasite Energy charge rotations for player tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyChargeFamily0AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06D58_bank4.inc"
};

/// Keyframes and loop/stop controls for the nineteen-part family-0 charge clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track jumps to its first keyframe before its retained stop
/// control. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyChargeFamily0AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06D58_records.inc"
};

/// Absolute record starts for the family-0 charge clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyChargeFamily0AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_06D58_indices.inc"
};

/// Parasite Energy family 0 charge clip with the M4A1 Javelin equipped.
///
/// Native bank entry 27. Reset after the release clip; replaced by the cast
/// clip when the countdown reaches zero. Its nineteen tracks loop over two
/// one-frame keyframes while the cast countdown runs.
/// Family 0 selects spell IDs 300..600 whose tens digit is not 3, or item IDs
/// at least 601 whose tens digit is 1.
/// The borrowed arrays and word-aligned pose banks remain live while the
/// package is loaded.
static AnimationSet _gM4a1JavelinParasiteEnergyChargeFamily0Animation = {
    .records           = _gM4a1JavelinParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyChargeFamily0AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyChargeFamily0AnimationBank4,
    },
};

/// Family-0 Parasite Energy cast poses for player tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyCastFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_07030_bank1.inc"
};

/// Family-0 Parasite Energy cast rotations for player tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyCastFamily0AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07030_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part family-0 cast clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track ends with a stop control. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyCastFamily0AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07030_records.inc"
};

/// Absolute record starts for the family-0 cast clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyCastFamily0AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07030_indices.inc"
};

/// Parasite Energy family 0 cast clip with the M4A1 Javelin equipped.
///
/// Native bank entry 28. Reset when the attachment's cast countdown reaches zero.
/// Its nineteen tracks stop at their final poses before normal control resumes.
/// Family 0 selects spell IDs 300..600 whose tens digit is not 3, or item IDs
/// at least 601 whose tens digit is 1.
/// The borrowed arrays and word-aligned pose banks remain live while the
/// package is loaded.
static AnimationSet _gM4a1JavelinParasiteEnergyCastFamily0Animation = {
    .records           = _gM4a1JavelinParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyCastFamily0AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyCastFamily0AnimationBank4,
    },
};

/// Family-1 Parasite Energy release poses for player tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyReleaseFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_072D4_bank1.inc"
};

/// Family-1 Parasite Energy release rotations for player tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyReleaseFamily1AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_072D4_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part family-1 release clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track ends with a stop control. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyReleaseFamily1AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_072D4_records.inc"
};

/// Absolute record starts for the family-1 release clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyReleaseFamily1AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_072D4_indices.inc"
};

/// Parasite Energy family 1 release clip with the M4A1 Javelin equipped.
///
/// Native bank entry 29. Entered with a six-frame blend when the attachment
/// begins release.
/// Its nineteen tracks stop at their final poses before charge playback.
/// Family 1 selects spell IDs below 300 whose tens digit is not 3, or item IDs
/// at least 601 whose tens digit is not 1.
/// The borrowed arrays and word-aligned pose banks remain live while the
/// package is loaded.
static AnimationSet _gM4a1JavelinParasiteEnergyReleaseFamily1Animation = {
    .records           = _gM4a1JavelinParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyReleaseFamily1AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyReleaseFamily1AnimationBank4,
    },
};

/// Family-1 Parasite Energy charge poses for player tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyChargeFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_074D4_bank1.inc"
};

/// Family-1 Parasite Energy charge rotations for player tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyChargeFamily1AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_074D4_bank4.inc"
};

/// Keyframes and loop/stop controls for the nineteen-part family-1 charge clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track jumps to its first keyframe before its retained stop
/// control. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyChargeFamily1AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_074D4_records.inc"
};

/// Absolute record starts for the family-1 charge clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyChargeFamily1AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_074D4_indices.inc"
};

/// Parasite Energy family 1 charge clip with the M4A1 Javelin equipped.
///
/// Native bank entry 30. Reset after the release clip; replaced by the cast
/// clip when the countdown reaches zero. Its nineteen tracks loop over two
/// one-frame keyframes while the cast countdown runs.
/// Family 1 selects spell IDs below 300 whose tens digit is not 3, or item IDs
/// at least 601 whose tens digit is not 1.
/// The borrowed arrays and word-aligned pose banks remain live while the
/// package is loaded.
static AnimationSet _gM4a1JavelinParasiteEnergyChargeFamily1Animation = {
    .records           = _gM4a1JavelinParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyChargeFamily1AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyChargeFamily1AnimationBank4,
    },
};

/// Family-1 Parasite Energy cast poses for player tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyCastFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_07828_bank1.inc"
};

/// Family-1 Parasite Energy cast rotations for player tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyCastFamily1AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07828_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part family-1 cast clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track ends with a stop control. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyCastFamily1AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07828_records.inc"
};

/// Absolute record starts for the family-1 cast clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyCastFamily1AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07828_indices.inc"
};

/// Parasite Energy family 1 cast clip with the M4A1 Javelin equipped.
///
/// Native bank entry 31. Reset when the attachment's cast countdown reaches zero.
/// Its nineteen tracks stop at their final poses before normal control resumes.
/// Family 1 selects spell IDs below 300 whose tens digit is not 3, or item IDs
/// at least 601 whose tens digit is not 1.
/// The borrowed arrays and word-aligned pose banks remain live while the
/// package is loaded.
static AnimationSet _gM4a1JavelinParasiteEnergyCastFamily1Animation = {
    .records           = _gM4a1JavelinParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyCastFamily1AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyCastFamily1AnimationBank4,
    },
};

/// Translation/rotation poses for the part-4 hit reaction clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinPart4HitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_07ADC_bank1.inc"
};

/// Packed rotations for the part-4 hit reaction clip's tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit
/// steps of a 4096-unit turn. Record offsets count four-byte words.
static const AnimationPackedRotation _gM4a1JavelinPart4HitReactionAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07ADC_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-track part-4 hit reaction clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track ends with a stop control.
/// No cue flags are set.
static const AnimationRecord _gM4a1JavelinPart4HitReactionAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07ADC_records.inc"
};

/// Absolute record starts for the part-4 hit reaction clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinPart4HitReactionAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07ADC_indices.inc"
};

/// Player part-4 hit reaction with the M4A1 Javelin equipped.
///
/// Native bank entry 16, selected for hit region 1 (the model-part-4
/// collision sphere) with a three-frame blend across child slots 1..18.
/// Each track stops at its final pose before damage recovery.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinPart4HitReactionAnimation = {
    .records           = _gM4a1JavelinPart4HitReactionAnimationRecords,
    .trackStartIndices = _gM4a1JavelinPart4HitReactionAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinPart4HitReactionAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinPart4HitReactionAnimationBank4,
    },
};

/// Translation/rotation poses for the other-body hit reaction clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinOtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_07DFC_bank1.inc"
};

/// Packed rotations for the other-body hit reaction clip's tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit
/// steps of a 4096-unit turn. Record offsets count four-byte words.
static const AnimationPackedRotation _gM4a1JavelinOtherBodyHitReactionAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07DFC_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-track other-body hit reaction clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track ends with a stop control.
/// No cue flags are set.
static const AnimationRecord _gM4a1JavelinOtherBodyHitReactionAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07DFC_records.inc"
};

/// Absolute record starts for the other-body hit reaction clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinOtherBodyHitReactionAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_07DFC_indices.inc"
};

/// Player hit reaction for other body regions with the M4A1 Javelin equipped.
///
/// Native bank entry 17, selected for a pending hit outside the model-part-4
/// region with a three-frame blend across child slots 1..18.
/// Each track stops at its final pose before damage recovery.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinOtherBodyHitReactionAnimation = {
    .records           = _gM4a1JavelinOtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gM4a1JavelinOtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinOtherBodyHitReactionAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinOtherBodyHitReactionAnimationBank4,
    },
};

/// Translation/rotation poses for the stopped pose clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinStoppedPoseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_085C0_bank1.inc"
};

/// Packed rotations for the stopped pose clip's tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit
/// steps of a 4096-unit turn. Record offsets count four-byte words.
static const AnimationPackedRotation _gM4a1JavelinStoppedPoseAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_085C0_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-track stopped pose clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Each track ends with a stop control.
/// No cue flags are set.
static const AnimationRecord _gM4a1JavelinStoppedPoseAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_085C0_records.inc"
};

/// Absolute record starts for the stopped pose clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinStoppedPoseAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_085C0_indices.inc"
};

/// Player stopped damage-mode clip with the M4A1 Javelin equipped.
///
/// Native bank entry 18, used at zero HP and for forced stopped poses.
/// Child slots 1..18 are reset or blended for the requested frame count;
/// all tracks stop at their final poses. The stopped state has no automatic
/// damage recovery. Selecting this clip does not itself change HP.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinStoppedPoseAnimation = {
    .records           = _gM4a1JavelinStoppedPoseAnimationRecords,
    .trackStartIndices = _gM4a1JavelinStoppedPoseAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinStoppedPoseAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinStoppedPoseAnimationBank4,
    },
};

/// Translation/rotation poses for the high-health idle clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinHighHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_09858_bank1.inc"
};

/// Packed rotations for the high-health idle clip's tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit
/// steps of a 4096-unit turn. Record offsets count four-byte words.
static const AnimationPackedRotation _gM4a1JavelinHighHealthIdleAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_09858_bank4.inc"
};

/// Keyframes and loop controls for the nineteen-track high-health idle clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Jumps target absolute record indices within the same track.
/// A stop record follows each loop jump but is not reached by forward playback.
/// No cue flags are set.
static const AnimationRecord _gM4a1JavelinHighHealthIdleAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_09858_records.inc"
};

/// Absolute record starts for the high-health idle clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinHighHealthIdleAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_09858_indices.inc"
};

/// Player idle clip above half HP with the M4A1 Javelin equipped.
///
/// Native bank entry 23, selected after 300 direction-free idle ticks when
/// HP is greater than half maximum HP. Child slots 1..18 blend for five
/// normal-rate frames. Every track loops over a trailing keyframe sequence.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinHighHealthIdleAnimation = {
    .records           = _gM4a1JavelinHighHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1JavelinHighHealthIdleAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinHighHealthIdleAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinHighHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for the low-health idle clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinLowHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0A3D0_bank1.inc"
};

/// Packed rotations for the low-health idle clip's tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit
/// steps of a 4096-unit turn. Record offsets count four-byte words.
static const AnimationPackedRotation _gM4a1JavelinLowHealthIdleAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0A3D0_bank4.inc"
};

/// Keyframes and loop controls for the nineteen-track low-health idle clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Jumps target absolute record indices within the same track.
/// A stop record follows each loop jump but is not reached by forward playback.
/// No cue flags are set.
static const AnimationRecord _gM4a1JavelinLowHealthIdleAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0A3D0_records.inc"
};

/// Absolute record starts for the low-health idle clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinLowHealthIdleAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0A3D0_indices.inc"
};

/// Player low-health idle and paralysis clip with the M4A1 Javelin equipped.
///
/// Native bank entry 25, selected after 300 direction-free idle ticks when
/// HP is at most one quarter of maximum HP, with a five-frame blend.
/// Paralysis also selects it, with a six-frame blend. Both paths update
/// child slots 1..18; every track loops over a trailing keyframe sequence.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinLowHealthIdleAnimation = {
    .records           = _gM4a1JavelinLowHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1JavelinLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinLowHealthIdleAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinLowHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for the mid-health idle clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 1/4096 turns. Pose offsets
/// count four-byte words, three per pose; the bank must remain word-aligned.
static const AnimationPackedPose _gM4a1JavelinMidHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0AAEC_bank1.inc"
};

/// Packed rotations for the mid-health idle clip's tracks 2..18.
///
/// Signed Euler components are packed in 11/10/11 bits, in eight-angle-unit
/// steps of a 4096-unit turn. Record offsets count four-byte words.
static const AnimationPackedRotation _gM4a1JavelinMidHealthIdleAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0AAEC_bank4.inc"
};

/// Keyframes and loop controls for the nineteen-track mid-health idle clip.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words. Jumps target absolute record indices within the same track.
/// A stop record follows each loop jump but is not reached by forward playback.
/// No cue flags are set.
static const AnimationRecord _gM4a1JavelinMidHealthIdleAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0AAEC_records.inc"
};

/// Absolute record starts for the mid-health idle clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinMidHealthIdleAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0AAEC_indices.inc"
};

/// Player idle clip between quarter and half HP with the M4A1 Javelin equipped.
///
/// Native bank entry 24, selected after 300 direction-free idle ticks when
/// HP is above one quarter and at most half maximum HP. Child slots 1..18
/// blend for five normal-rate frames. Every track loops over a trailing
/// keyframe sequence.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinMidHealthIdleAnimation = {
    .records           = _gM4a1JavelinMidHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1JavelinMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinMidHealthIdleAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinMidHealthIdleAnimationBank4,
    },
};

/// Event-entry poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinEventEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0AF6C_bank1.inc"
};

/// Event-entry rotations for tracks 2..18, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinEventEntryAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0AF6C_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part event-entry clip.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Every track stops at its final pose. No cue flags are set.
static const AnimationRecord _gM4a1JavelinEventEntryAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0AF6C_records.inc"
};

/// Absolute record starts for the event-entry clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinEventEntryAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0AF6C_indices.inc"
};

/// Player presentation entering an event with the M4A1 Javelin equipped.
///
/// Native bank entries 32 and 34 share this clip. Scripted presentation with
/// selector zero starts child slots 1..18 without a timed blend, then stops
/// at the event pose while the event runs.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinEventEntryAnimation = {
    .records           = _gM4a1JavelinEventEntryAnimationRecords,
    .trackStartIndices = _gM4a1JavelinEventEntryAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinEventEntryAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinEventEntryAnimationBank4,
    },
};

/// Event-exit poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinEventExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0B144_bank1.inc"
};

/// Event-exit rotations for tracks 2..18, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinEventExitAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B144_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part event-exit clip.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Every track stops at its final pose. No cue flags are set.
static const AnimationRecord _gM4a1JavelinEventExitAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B144_records.inc"
};

/// Absolute record starts for the event-exit clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinEventExitAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B144_indices.inc"
};

/// Player presentation leaving an event with the M4A1 Javelin equipped.
///
/// Native bank entries 33 and 35 share this clip. Scripted presentation with
/// a nonzero selector starts child slots 1..18 without a timed blend; event
/// tasks wait for completion before restoring player control.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinEventExitAnimation = {
    .records           = _gM4a1JavelinEventExitAnimationRecords,
    .trackStartIndices = _gM4a1JavelinEventExitAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinEventExitAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinEventExitAnimationBank4,
    },
};

/// Family-2 Parasite Energy release poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0B6A8_bank1.inc"
};

/// Family-2 Parasite Energy release rotations for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyReleaseFamily2AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B6A8_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part family-2 release clip.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Every track stops at its final pose. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyReleaseFamily2AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B6A8_records.inc"
};

/// Absolute record starts for the family-2 release clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyReleaseFamily2AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B6A8_indices.inc"
};

/// Parasite Energy family 2 release clip with the M4A1 Javelin equipped.
///
/// Native bank entry 42, entered with a six-frame blend when release begins.
/// The nineteen tracks stop at their final poses before charge playback.
/// Family 2 selects attachment IDs below 601 whose decimal tens digit is 3.
/// The borrowed arrays and word-aligned pose banks must remain loaded while
/// any playback slot refers to them.
static AnimationSet _gM4a1JavelinParasiteEnergyReleaseFamily2Animation = {
    .records           = _gM4a1JavelinParasiteEnergyReleaseFamily2AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyReleaseFamily2AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyReleaseFamily2AnimationBank4,
    },
};

/// Family-2 Parasite Energy charge poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0B89C_bank1.inc"
};

/// Family-2 Parasite Energy charge rotations for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyChargeFamily2AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B89C_bank4.inc"
};

/// Keyframes and loop/stop controls for the nineteen-part family-2 charge clip.
///
/// Every track loops over two one-frame keyframes before its retained stop
/// control. Pose offsets count four-byte words; jumps are absolute record
/// indices. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyChargeFamily2AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B89C_records.inc"
};

/// Absolute record starts for the family-2 charge clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyChargeFamily2AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0B89C_indices.inc"
};

/// Parasite Energy family 2 charge clip with the M4A1 Javelin equipped.
///
/// Native bank entry 43, reset after release and replaced by the cast clip
/// when the attachment countdown reaches zero. Its nineteen tracks loop.
/// Family 2 selects attachment IDs below 601 whose decimal tens digit is 3.
/// The borrowed arrays and word-aligned pose banks must remain loaded while
/// any playback slot refers to them.
static AnimationSet _gM4a1JavelinParasiteEnergyChargeFamily2Animation = {
    .records           = _gM4a1JavelinParasiteEnergyChargeFamily2AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyChargeFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyChargeFamily2AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyChargeFamily2AnimationBank4,
    },
};

/// Family-2 Parasite Energy cast poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinParasiteEnergyCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0BD1C_bank1.inc"
};

/// Family-2 Parasite Energy cast rotations for tracks 2..18, in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinParasiteEnergyCastFamily2AnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0BD1C_bank4.inc"
};

/// Keyframes and stop controls for the nineteen-part family-2 cast clip.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Every track stops at its final pose. No cue flags are set.
static const AnimationRecord _gM4a1JavelinParasiteEnergyCastFamily2AnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0BD1C_records.inc"
};

/// Absolute record starts for the family-2 cast clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinParasiteEnergyCastFamily2AnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0BD1C_indices.inc"
};

/// Parasite Energy family 2 cast clip with the M4A1 Javelin equipped.
///
/// Native bank entry 44, reset when the attachment countdown reaches zero.
/// The nineteen tracks stop at their final poses before control resumes.
/// Family 2 selects attachment IDs below 601 whose decimal tens digit is 3.
/// The borrowed arrays and word-aligned pose banks must remain loaded while
/// any playback slot refers to them.
static AnimationSet _gM4a1JavelinParasiteEnergyCastFamily2Animation = {
    .records           = _gM4a1JavelinParasiteEnergyCastFamily2AnimationRecords,
    .trackStartIndices = _gM4a1JavelinParasiteEnergyCastFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinParasiteEnergyCastFamily2AnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinParasiteEnergyCastFamily2AnimationBank4,
    },
};

/// Fixed aim-hold poses for tracks 0 and 1, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1JavelinAimHoldAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0BEF8_bank1.inc"
};

/// Fixed aim-hold rotations for tracks 2..18, packed in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinAimHoldAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0BEF8_bank4.inc"
};

/// Keyframes and loop/stop controls for the nineteen-part aim-hold clip.
///
/// Every track loops over two one-frame keyframes selecting the same pose,
/// before its retained stop control. Pose offsets count four-byte words;
/// jumps are absolute record indices. No cue flags are set.
static const AnimationRecord _gM4a1JavelinAimHoldAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0BEF8_records.inc"
};

/// Absolute record starts for the aim-hold clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinAimHoldAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0BEF8_indices.inc"
};

/// Stationary aim-hold and attack-ready clip with the M4A1 Javelin equipped.
///
/// Native bank entry 9. Aim entry blends child slots 1..18 over five frames;
/// preparing either attack blends for one frame, or eight while moving or
/// turning. All nineteen tracks loop at a fixed pose until replaced.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinAimHoldAnimation = {
    .records           = _gM4a1JavelinAimHoldAnimationRecords,
    .trackStartIndices = _gM4a1JavelinAimHoldAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinAimHoldAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinAimHoldAnimationBank4,
    },
};

/// Translation/rotation poses for the backward/turn aim clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinAimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0C69C_bank1.inc"
};

/// Packed rotations for the backward/turn aim clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinAimBackwardOrTurnAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0C69C_bank4.inc"
};

/// Keyframes and loop/stop controls for backward or turning aim locomotion.
///
/// Child tracks loop over 32 normal-rate frames; track 1 marks paired footsteps.
/// Pose offsets count four-byte words; jumps use absolute record indices.
static const AnimationRecord _gM4a1JavelinAimBackwardOrTurnAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0C69C_records.inc"
};

/// Absolute record starts for the backward/turn aim clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinAimBackwardOrTurnAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0C69C_indices.inc"
};

/// Backward-motion and stationary-turn aim clip with the M4A1 Javelin equipped.
///
/// Native bank entry 13, selected while backing up or turning at rest.
/// Child tracks loop until another action replaces the clip.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinAimBackwardOrTurnAnimation = {
    .records           = _gM4a1JavelinAimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gM4a1JavelinAimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinAimBackwardOrTurnAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinAimBackwardOrTurnAnimationBank4,
    },
};

/// Translation/rotation poses for the forward aim clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinAimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0D144_bank1.inc"
};

/// Packed rotations for the forward aim clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinAimForwardAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0D144_bank4.inc"
};

/// Keyframes and loop/stop controls for forward aim locomotion.
///
/// Child tracks loop over 19 normal-rate frames; track 1 marks paired footsteps.
/// Pose offsets count four-byte words; jumps use absolute record indices.
static const AnimationRecord _gM4a1JavelinAimForwardAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0D144_records.inc"
};

/// Absolute record starts for the forward aim clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinAimForwardAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0D144_indices.inc"
};

/// Forward-motion aim clip with the M4A1 Javelin equipped.
///
/// Native bank entry 12, selected while moving forward in aim mode.
/// Child tracks loop until another action replaces the clip.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinAimForwardAnimation = {
    .records           = _gM4a1JavelinAimForwardAnimationRecords,
    .trackStartIndices = _gM4a1JavelinAimForwardAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinAimForwardAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinAimForwardAnimationBank4,
    },
};

/// Translation/rotation poses for the reload clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0E5F4_bank1.inc"
};

/// Packed rotations for the reload clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinReloadAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0E5F4_bank4.inc"
};

/// Keyframes and stop controls for the M4A1 Javelin reload clip.
///
/// Every track stops at its final pose; durations count normal-rate frames.
/// Track 1 carries two records with both cue flags set for reload sounds
/// and ammunition application. Pose offsets count four-byte words.
static const AnimationRecord _gM4a1JavelinReloadAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0E5F4_records.inc"
};

/// Absolute record starts for the reload clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinReloadAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0E5F4_indices.inc"
};

/// Reload clip shared by the M4A1 Javelin primary and secondary loads.
///
/// Native bank entries 14 and 15, also reused at entry 20 after battle.
/// Reload entry blends child tracks for three frames; the second paired
/// cue on track 1 applies the selected ammunition before playback ends.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinReloadAnimation = {
    .records           = _gM4a1JavelinReloadAnimationRecords,
    .trackStartIndices = _gM4a1JavelinReloadAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinReloadAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinReloadAnimationBank4,
    },
};

/// Translation/rotation poses for the rifle-shot clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinRifleShotAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0EADC_bank1.inc"
};

/// Packed rotations for the rifle-shot clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinRifleShotAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0EADC_bank4.inc"
};

/// Keyframes and stop controls for one M4A1 Javelin rifle shot.
///
/// Every track stops at its final pose; durations count normal-rate frames.
/// Pose offsets count four-byte words. No cue flags are set.
static const AnimationRecord _gM4a1JavelinRifleShotAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0EADC_records.inc"
};

/// Absolute record starts for the rifle-shot clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinRifleShotAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0EADC_indices.inc"
};

/// Rifle-shot clip restarted for each round of the M4A1 Javelin primary attack.
///
/// Native bank entry 10, blended into child tracks over two frames for
/// each fired round. Every track stops at its final pose.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinRifleShotAnimation = {
    .records           = _gM4a1JavelinRifleShotAnimationRecords,
    .trackStartIndices = _gM4a1JavelinRifleShotAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinRifleShotAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinRifleShotAnimationBank4,
    },
};

/// Translation/rotation poses for the beam-fire clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinBeamFireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0EE3C_bank1.inc"
};

/// Packed rotations for the beam-fire clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinBeamFireAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0EE3C_bank4.inc"
};

/// Keyframes and stop controls for the M4A1 Javelin beam attack.
///
/// Every track stops at its final pose; durations count normal-rate frames.
/// Pose offsets count four-byte words. No cue flags are set.
static const AnimationRecord _gM4a1JavelinBeamFireAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0EE3C_records.inc"
};

/// Absolute record starts for the beam-fire clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinBeamFireAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0EE3C_indices.inc"
};

/// Beam-fire clip for the M4A1 Javelin secondary attack.
///
/// Native bank entry 11, blended into child tracks over three frames when
/// the beam attack starts. Every track stops at its final pose.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinBeamFireAnimation = {
    .records           = _gM4a1JavelinBeamFireAnimationRecords,
    .trackStartIndices = _gM4a1JavelinBeamFireAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinBeamFireAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinBeamFireAnimationBank4,
    },
};

/// Translation/rotation poses for the aim-entry clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinAimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0F4FC_bank1.inc"
};

/// Packed rotations for the aim-entry clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinAimEntryAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0F4FC_bank4.inc"
};

/// Keyframes and stop controls for entering aim with the M4A1 Javelin.
///
/// Every track stops at its final pose; durations count normal-rate frames.
/// Pose offsets count four-byte words. No cue flags are set.
static const AnimationRecord _gM4a1JavelinAimEntryAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0F4FC_records.inc"
};

/// Absolute record starts for the aim-entry clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinAimEntryAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0F4FC_indices.inc"
};

/// Aim-entry clip for raising the equipped M4A1 Javelin into its ready pose.
///
/// Native bank entry 7. Aim entry restarts or blends the child tracks;
/// completion blends them into the aim-hold clip over five frames.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinAimEntryAnimation = {
    .records           = _gM4a1JavelinAimEntryAnimationRecords,
    .trackStartIndices = _gM4a1JavelinAimEntryAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinAimEntryAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinAimEntryAnimationBank4,
    },
};

/// Translation/rotation poses for the aim-exit clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinAimExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_0FE0C_bank1.inc"
};

/// Packed rotations for the aim-exit clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinAimExitAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0FE0C_bank4.inc"
};

/// Keyframes and stop controls for leaving aim with the M4A1 Javelin.
///
/// Every track stops at its final pose; durations count normal-rate frames.
/// Pose offsets count four-byte words. No cue flags are set.
static const AnimationRecord _gM4a1JavelinAimExitAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0FE0C_records.inc"
};

/// Absolute record starts for the aim-exit clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinAimExitAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_0FE0C_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM4a1JavelinAimExitAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m4a1JavelinAimExitTrackCount);

/// Aim-exit clip for lowering the equipped M4A1 Javelin out of its ready pose.
///
/// Native bank entry 8, blended into child tracks over six frames when aim ends,
/// including after a scripted attack. Every track stops before locomotion resumes.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinAimExitAnimation = {
    .records           = _gM4a1JavelinAimExitAnimationRecords,
    .trackStartIndices = _gM4a1JavelinAimExitAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinAimExitAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinAimExitAnimationBank4,
    },
};

/// Translation/rotation poses for the armed forward-walk clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_106C4_bank1.inc"
};

/// Packed rotations for the armed forward-walk clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinWalkAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_106C4_bank4.inc"
};

/// Keyframes and loop controls for walking forward with the M4A1 Javelin.
///
/// Each track jumps within itself before its retained stop control.
/// Track 1 carries the paired footstep cues; durations count normal-rate frames
/// and pose offsets count four-byte words.
static const AnimationRecord _gM4a1JavelinWalkAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_106C4_records.inc"
};

/// Absolute record starts for the armed forward-walk clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinWalkAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_106C4_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM4a1JavelinWalkAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m4a1JavelinWalkTrackCount);

/// Looping forward-walk clip for the player holding the M4A1 Javelin.
///
/// Native bank entry 2, selected for ordinary forward walking with a held
/// weapon model. Track 1's cues time footsteps and optional ground dust.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinWalkAnimation = {
    .records           = _gM4a1JavelinWalkAnimationRecords,
    .trackStartIndices = _gM4a1JavelinWalkAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinWalkAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinWalkAnimationBank4,
    },
};

/// Translation/rotation poses for the backward-walk clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinBackwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_10E64_bank1.inc"
};

/// Packed rotations for the backward-walk clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinBackwardWalkAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_10E64_bank4.inc"
};

/// Keyframes and loop controls for walking backward with the M4A1 Javelin.
///
/// Each track jumps within itself before its retained stop control.
/// Track 1 carries the paired footstep cues; durations count normal-rate frames
/// and pose offsets count four-byte words.
static const AnimationRecord _gM4a1JavelinBackwardWalkAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_10E64_records.inc"
};

/// Absolute record starts for the backward-walk clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinBackwardWalkAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_10E64_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM4a1JavelinBackwardWalkAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m4a1JavelinBackwardWalkTrackCount);

/// Looping backward-walk clip for the player using the M4A1 Javelin bank.
///
/// Native bank entry 3, selected for backward movement outside aim.
/// Track 1's cues time footsteps and optional ground dust.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinBackwardWalkAnimation = {
    .records           = _gM4a1JavelinBackwardWalkAnimationRecords,
    .trackStartIndices = _gM4a1JavelinBackwardWalkAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinBackwardWalkAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinBackwardWalkAnimationBank4,
    },
};

/// Translation/rotation poses for the running clip's tracks 0 and 1.
///
/// Translation uses model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gM4a1JavelinRunAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_javelin_animation_11838_bank1.inc"
};

/// Packed rotations for the running clip's tracks 2..18.
///
/// Signed 11/10/11-bit X/Y/Z components in eight-angle-unit steps.
static const AnimationPackedRotation _gM4a1JavelinRunAnimationBank4[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_11838_bank4.inc"
};

/// Keyframes and loop controls for running with the M4A1 Javelin bank.
///
/// Each track jumps within itself before its retained stop control.
/// Track 1 carries the paired footstep cues; durations count normal-rate frames
/// and pose offsets count four-byte words.
static const AnimationRecord _gM4a1JavelinRunAnimationRecords[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_11838_records.inc"
};

/// Absolute record starts for the running clip's tracks 0..18.
///
/// The exported final zero halfword is retained outside the playback track domain.
static const u16 _gM4a1JavelinRunAnimationIndices[] SECTION(".data") = {
#include "assets/m4a1_javelin_animation_11838_indices.inc"
};
STATIC_ASSERT(ARRAY_SIZE(_gM4a1JavelinRunAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, m4a1JavelinRunTrackCount);

/// Looping run clip for the player using the M4A1 Javelin bank.
///
/// Native bank entry 4, selected for forward running outside aim according to
/// the saved movement mode and run input. Track 1's cues time running footsteps
/// and optional ground dust.
/// The nineteen-track arrays and word-aligned pose banks are borrowed and
/// must remain loaded while any playback slot refers to them.
static AnimationSet _gM4a1JavelinRunAnimation = {
    .records           = _gM4a1JavelinRunAnimationRecords,
    .trackStartIndices = _gM4a1JavelinRunAnimationIndices,
    .poseBanks         = {
        [M4A1_JAVELIN_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1JavelinRunAnimationBank1,
        [M4A1_JAVELIN_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1JavelinRunAnimationBank4,
    },
};

AnimationBank gM4a1JavelinPlayerAnimationBank = {
    .table = { .sets = {
                   [1] = &_gM4a1JavelinIdleAnimation,
                   &_gM4a1JavelinWalkAnimation,
                   &_gM4a1JavelinBackwardWalkAnimation,
                   &_gM4a1JavelinRunAnimation,
                   &_gM4a1JavelinNegativeTurnAnimation,
                   &_gM4a1JavelinPositiveTurnAnimation,
                   &_gM4a1JavelinAimEntryAnimation,
                   &_gM4a1JavelinAimExitAnimation,
                   &_gM4a1JavelinAimHoldAnimation,
                   &_gM4a1JavelinRifleShotAnimation,
                   &_gM4a1JavelinBeamFireAnimation,
                   &_gM4a1JavelinAimForwardAnimation,
                   &_gM4a1JavelinAimBackwardOrTurnAnimation,
                   &_gM4a1JavelinReloadAnimation,
                   &_gM4a1JavelinReloadAnimation,
                   &_gM4a1JavelinPart4HitReactionAnimation,
                   &_gM4a1JavelinOtherBodyHitReactionAnimation,
                   &_gM4a1JavelinStoppedPoseAnimation,
                   &_gM4a1JavelinUnarmedWalkAnimation,
                   &_gM4a1JavelinReloadAnimation,
                   &_gM4a1JavelinIdleAnimation,
                   &_gM4a1JavelinIdleAnimation,
                   &_gM4a1JavelinHighHealthIdleAnimation,
                   &_gM4a1JavelinMidHealthIdleAnimation,
                   &_gM4a1JavelinLowHealthIdleAnimation,
                   &_gM4a1JavelinParasiteEnergyReleaseFamily0Animation,
                   &_gM4a1JavelinParasiteEnergyChargeFamily0Animation,
                   &_gM4a1JavelinParasiteEnergyCastFamily0Animation,
                   &_gM4a1JavelinParasiteEnergyReleaseFamily1Animation,
                   &_gM4a1JavelinParasiteEnergyChargeFamily1Animation,
                   &_gM4a1JavelinParasiteEnergyCastFamily1Animation,
                   &_gM4a1JavelinEventEntryAnimation,
                   &_gM4a1JavelinEventExitAnimation,
                   &_gM4a1JavelinEventEntryAnimation,
                   &_gM4a1JavelinEventExitAnimation,
                   &_gM4a1JavelinStairAscentAnimation,
                   &_gM4a1JavelinStairDescentAnimation,
                   &_gM4a1JavelinStairAscentFinishEvenAnimation,
                   &_gM4a1JavelinStairAscentFinishOddAnimation,
                   &_gM4a1JavelinItemUseAnimation,
                   &_gM4a1JavelinIdleAnimation,
                   &_gM4a1JavelinParasiteEnergyReleaseFamily2Animation,
                   &_gM4a1JavelinParasiteEnergyChargeFamily2Animation,
                   &_gM4a1JavelinParasiteEnergyCastFamily2Animation,
               } },
};

/// Unreferenced four-byte value following the player animation bank; role unproven.
static u32 D_m4a1_javelin_8012EB5C = 0x102232DD;

/// Cached screen direction shared by visible segments of the airborne guide beam.
///
/// Stores the low halfword of a signed angle, in 4096 units per turn. Drawing
/// interprets it as s16; a refresh replaces it before constructing a far cap.
static u16 _gM4a1JavelinBeamScreenAngle = 0;

/// Cached ground-glow screen direction, independent of the airborne beam's angle.
///
/// Stores the low halfword of a signed angle, in 4096 units per turn, interpreted
/// as s16 by the ground-strip drawer.
static u16 _gM4a1JavelinGroundBeamScreenAngle = 0;

/// Requests an airborne-beam direction refresh after an endpoint projection fails.
///
/// Boolean halfword (0 clear, 1 pending). Cleared at the start of each beam tick;
/// within the tick, the next accepted segment refreshes the angle and far cap.
static s16 _gM4a1JavelinBeamAngleRefreshPending = false;

/// Requests a ground-glow direction refresh after an endpoint projection fails.
///
/// Boolean halfword (0 clear, 1 pending), independent of the airborne request.
/// Cleared at each beam tick and consumed by the next accepted ground segment.
static s16 _gM4a1JavelinGroundBeamAngleRefreshPending = false;

/// Copied world-space contact endpoint of the current guide beam, in game units.
///
/// XYZ are narrowed to signed halfwords. Coordinates are meaningful only while
/// `_gM4a1JavelinTrackedImpactValid` is true; invalidation retains the stale point.
static SVECTOR _gM4a1JavelinTrackedImpactPoint = { 0, 0, 0, 0 };

/// Whether the guide beam should end at its copied contact rather than its full reach.
///
/// Cleared on guide initialization or a missing contact; set when the contact's
/// XYZ translation is copied. It owns no pointer into the contact's scratch storage.
static bool _gM4a1JavelinTrackedImpactValid = false;
