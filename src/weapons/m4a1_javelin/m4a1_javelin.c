#include "weapons/m4a1_javelin.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
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
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"
#include "weapons/sounds.h"

#include "overlay.h"
#include "types.h"

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
#include "../../shared/effects/sprite_quad.h"
#include "../shared/attack.h"

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
    M4A1_JAVELIN_TRIG_FRACTION_BITS    = 12
};

static void _m4a1JavelinDrawBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444);
static void _m4a1JavelinDrawGroundBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444);
static void _m4a1JavelinDrawMuzzleFlareLine(const SVECTOR* muzzlePoint, const SVECTOR* ringPoint, u16 brightness);

/// Guide-beam origin in the weapon model's local coordinate units.
static SVECTOR _gM4a1JavelinGuideBeamOffset = { 0, 0x200, 0x20, 0 };

/// Ground-probe displacement along view +Y, in game coordinate units.
///
/// Rotated into world space before adding it to each guide-beam sample.
static SVECTOR _gM4a1JavelinGroundProbeOffset = { 0, 0x800, 0, 0 };

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
static u16 _gM4a1JavelinBeamFadeColours[] = { 0x12, 0x124, 0x248, 0x36C };

/// The model `gM4a1JavelinWeaponModel`: generated from its asset (weapons/m4a1_javelin/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m4a1_javelin/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/m4a1_javelin/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/m4a1_javelin/clips.inc"
#include "assets/weapons/m4a1_javelin/bank.inc"

/// Cached screen direction shared by visible segments of the airborne guide beam.
///
/// Stores the low halfword of a signed angle, in 4096 units per turn. Drawing
/// interprets it as s16; a refresh replaces it before constructing a far cap.
static u16 _gM4a1JavelinBeamScreenAngle;

/// Cached ground-glow screen direction, independent of the airborne beam's angle.
///
/// Stores the low halfword of a signed angle, in 4096 units per turn, interpreted
/// as s16 by the ground-strip drawer.
static u16 _gM4a1JavelinGroundBeamScreenAngle;

/// Requests an airborne-beam direction refresh after an endpoint projection fails.
///
/// Boolean halfword (0 clear, 1 pending). Cleared at the start of each beam tick;
/// within the tick, the next accepted segment refreshes the angle and far cap.
static s16 _gM4a1JavelinBeamAngleRefreshPending;

/// Requests a ground-glow direction refresh after an endpoint projection fails.
///
/// Boolean halfword (0 clear, 1 pending), independent of the airborne request.
/// Cleared at each beam tick and consumed by the next accepted ground segment.
static s16 _gM4a1JavelinGroundBeamAngleRefreshPending;

/// Copied world-space contact endpoint of the current guide beam, in game units.
///
/// XYZ are narrowed to signed halfwords. Coordinates are meaningful only while
/// `_gM4a1JavelinTrackedImpactValid` is true; invalidation retains the stale point.
static SVECTOR _gM4a1JavelinTrackedImpactPoint;

/// Whether the guide beam should end at its copied contact rather than its full reach.
///
/// Cleared on guide initialization or a missing contact; set when the contact's
/// XYZ translation is copied. It owns no pointer into the contact's scratch storage.
static bool _gM4a1JavelinTrackedImpactValid;

static void _m4a1JavelinSetTrackedImpactPoint(const long* worldTranslation);

/// Reserves a Gouraud beam quad with black edges and supplied centre colours.
///
/// `quad` is a simple writable POLY_G4* lvalue, assigned then read repeatedly.
/// Each colour expression is evaluated once and narrowed to a byte, in RGB2
/// then RGB3 order. Caps use black for RGB3; strips colour both centres.
/// Requires word-aligned frame-arena space for sizeof(POLY_G4); captures and
/// advances `gGpuPrimCursor` without checking its end. Sets length, command
/// and four colours only. The caller supplies XY, ordering-table linkage and
/// blending before GPU submission; the packet lives until the GPU finishes.
/// Captures no caller locals. Expands to a block with no control transfer;
/// invoke as a standalone statement inside a braced control body.
///
/// A macro, not an inline function returning the quad: as one, its arguments
/// are copied into fresh registers and both beam drawers no longer match.
#define M4A1_JAVELIN_ALLOCATE_BEAM_QUAD(quad, red, green, blue, secondRed, secondGreen, secondBlue) \
    {                                                                                               \
        (quad)         = gGpuPrimCursor;                                                            \
        gGpuPrimCursor = (quad) + 1;                                                                \
        setPolyG4(quad);                                                                            \
        setRGB0(quad, 0, 0, 0);                                                                     \
        setRGB1(quad, 0, 0, 0);                                                                     \
        setRGB2(quad, (red), (green), (blue));                                                      \
        setRGB3(quad, (secondRed), (secondGreen), (secondBlue));                                    \
    }

void m4a1JavelinGuideBeamTask(Task* task)
{
    enum {
        M4A1_JAVELIN_GUIDE_INIT         = 0,
        M4A1_JAVELIN_GUIDE_MUZZLE_FLARE = 1,
        M4A1_JAVELIN_GUIDE_BEAM         = 2,
        M4A1_JAVELIN_BEAM_SEGMENT_COUNT = ARRAY_SIZE(_gM4a1JavelinBeamSegmentFlags),
        // Actor mode occupies the low halfword, normal firing state 4 the high.
        M4A1_JAVELIN_PLAYER_FIRING_MODE_STATE = (PLAYER_ACTOR_STATE_ATTACK << 16) | GAME_ACTOR_MODE_NORMAL
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
            RANDOM_LCG_ADVANCE();
            pointLight->head.color.b = (RANDOM_LCG_DRAW() & 0x700) + 0x400;
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
            RANDOM_LCG_ADVANCE();
            blueIntensity            = (RANDOM_LCG_DRAW() & 0x700) + 0x800;
            pointLight->head.color.b = blueIntensity;
            pointLight->head.color.r = blueIntensity >> 1;
            pointLight->head.color.g = blueIntensity >> 1;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0), line);
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz1),
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0),
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
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0),
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0),
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz1), quad);
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0), quad);
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
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0), quad);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0), line);
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
#include "../../shared/effects/sprite_quad_draw.inc.c"

#include "assets/engine/animation/player_schema.h"

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
        work->scale = M4A1_JAVELIN_CONTACT_FLASH_SIZE;
        RANDOM_LCG_ADVANCE();
        work->angle = RANDOM_LCG_DRAW() & (M4A1_JAVELIN_FULL_TURN - 1);
        task->state = M4A1_JAVELIN_CONTACT_FLASH_PLAYING;
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
        M4A1_JAVELIN_PHASE_PREPARE        = 0,
        M4A1_JAVELIN_PHASE_WAIT_READY     = 1,
        M4A1_JAVELIN_PHASE_SELECT_ATTACK  = 2,
        M4A1_JAVELIN_PHASE_BURST          = 3,
        M4A1_JAVELIN_PHASE_IMPACT         = 4,
        M4A1_JAVELIN_PHASE_BEAM_DELAY     = 5,
        M4A1_JAVELIN_PHASE_BEAM_ACTIVE    = 6,
        M4A1_JAVELIN_PHASE_RECOVER        = 7,
        M4A1_JAVELIN_ATTACK_SCRATCH_BYTES = 0x58,
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
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += readyBlendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M4A1_JAVELIN_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
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
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_JAVELIN), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                effectWork = effectSpawn(EFFECT_JAVELIN_GUIDE_BEAM,
                                         actor->equipmentTasks[1]->extra.tmd->coords,
                                         0x1D, NULL);
                if (effectWork != NULL) {
                    taskReparent(actor->equipmentTasks[1], effectWork->task);
                }
                worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_JAVELIN_SOUND(M4A1_JAVELIN_SOUND_BEAM), 1);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 0, 3);
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
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_JAVELIN), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, M4A1_JAVELIN_SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX_M4A1_JAVELIN, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
                } else {
                    burstDelay--;
                    actor->stateTimer = burstDelay;
                    if (burstDelay == M4A1_JAVELIN_BURST_IMPACT_TICKS_LEFT) {
                        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                        if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                            effectSpawn(EFFECT_IMPACT_SPARK, impactCoord, 0, NULL);
                            worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
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
                worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
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
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_JAVELIN_RECOVERY_COOLDOWN_FRAMES;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(M4A1_JAVELIN_ATTACK_SCRATCH_BYTES);
}
