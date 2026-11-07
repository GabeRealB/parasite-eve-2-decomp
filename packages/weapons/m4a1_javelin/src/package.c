#include "m4a1_javelin.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/effects.h>
#include <gameplay/hud_sprites.h>
#include <gameplay/items.h>
#include <gameplay/light.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>
#include <gameplay/world_collision_types.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/mc_types.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/session_types.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>
#include <engine/sound_ids.h>

#include <effects/glow/glow.h>

#include <weapons/weapon/weapon.h>
#include <weapons/weapon/sounds.h>
#include <weapons/weapon/weapon_index.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 8)
PE2_OPTION_DEF(BURST_ROUNDS, int, 3)
PE2_OPTION_DEF(BURST_CANCEL_FRAMES, int, 9)
PE2_OPTION_DEF(SECONDARY_CANCEL_FRAMES, int, 0x1C)
PE2_OPTION_DEF(BURST_DELAY_FRAMES, int, 3)
PE2_OPTION_DEF(RECOVERY_COOLDOWN_FRAMES, int, 0xC)
PE2_OPTION_DEF(BEAM_DELAY_FRAMES, int, 6)
PE2_OPTION_DEF(BEAM_ACTIVE_FRAMES, int, 0x1C)
PE2_OPTION_DEF(BURST_IMPACT_FRAMES_LEFT, int, 2)
/// The guide beam and muzzle flare (`m4a1JavelinGuideBeamTask`): lengths in game units, times in dispatch ticks.
PE2_OPTION_DEF(BEAM_LENGTH, int, 8000)
PE2_OPTION_DEF(FLARE_INITIAL_RADIUS, int, 0x600)
PE2_OPTION_DEF(FLARE_RADIUS_DECREMENT, int, 0xF0)
PE2_OPTION_DEF(FLARE_FORWARD_STEP, int, 0xC0)
PE2_OPTION_DEF(FLARE_BRIGHTNESS_STEP, int, 0x20)
PE2_OPTION_DEF(FLARE_MAX_BRIGHTNESS, int, 0xC0)
PE2_OPTION_DEF(GUIDE_HOLD_FRAMES, int, 32)
PE2_OPTION_DEF(LIGHT_LIFETIME_FRAMES, int, 4)
PE2_OPTION_DEF(BEAM_BASE_RGB444, int, 0x36C)
/// The beam's screen radius times its depth.
PE2_OPTION_DEF(RADIUS_DEPTH_PRODUCT, int, 0x4000)
PE2_OPTION_DEF(PROBE_RAISE, int, 0x100)
/// The contact flash (`m4a1JavelinContactFlashTask`); size in game units.
PE2_OPTION_DEF(CONTACT_FLASH_SIZE, int, 512)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gM4a1JavelinWeaponModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gM4a1JavelinPlayerAnimationBank)

/// A sound of the M4A1 Javelin's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M4A1_JAVELIN, (slot))

/// The M4A1 Javelin's own slots.
enum {
    SOUND_BEAM = 5,
};

/// Attack tuning and ids; times in dispatch ticks.
enum {
    RIFLE_KEY_BASE            = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_M4A1_JAVELIN << 8),
    BEAM_KEY                  = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_M4A1_JAVELIN << 8) | 0x1F,
    SECONDARY_TURN_RATE_INDEX = 2,
};

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
#include <effects/sprite_quad/sprite_quad.h>
#include <weapons/attack/attack.h>

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
} _LineScratch;
STATIC_ASSERT_SIZEOF(_LineScratch, 0x14);

/// Segment controls, angular units and perspective thickness used by the beam drawers.
enum {
    SEGMENT_CAP_NEAR_END  = 1,
    SEGMENT_REFRESH_ANGLE = 2,
    FULL_TURN             = 0x1000,
    HALF_TURN             = 0x800,
    QUARTER_TURN          = 0x400,
    EIGHTH_TURN           = 0x200,
    TRIG_FRACTION_BITS    = 12
};

static void _drawBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444);
static void _drawGroundBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444);
static void _drawMuzzleFlareLine(const SVECTOR* muzzlePoint, const SVECTOR* ringPoint, u16 brightness);

/// Guide-beam origin in the weapon model's local coordinate units.
static SVECTOR _gGuideBeamOffset = { 0, 0x200, 0x20, 0 };

/// Ground-probe displacement along view +Y, in game coordinate units.
///
/// Rotated into world space before adding it to each guide-beam sample.
static SVECTOR _gGroundProbeOffset = { 0, 0x800, 0, 0 };

/// Controls for the six beam and ground-glow segments, indexed from the muzzle.
///
/// Drawing visits indices 5..0: the far segment refreshes the direction and
/// caps its far end; the muzzle segment caps its near end.
static u16 _gBeamSegmentFlags[] = {
    SEGMENT_CAP_NEAR_END, 0, 0, 0, 0, SEGMENT_REFRESH_ANGLE
};

/// Guide-beam retirement colours packed as 0x0RGB, darkest to brightest.
///
/// Beam retirement visits indices 3..0; ground glow uses that index >> 1,
/// selecting only entries 1 and 0. Without ground tracing the beam retains
/// its fixed bright colour instead of reading this ramp.
static u16 _gBeamFadeColours[] = { 0x12, 0x124, 0x248, 0x36C };

/// The model and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(bank)

/// Cached screen direction shared by visible segments of the airborne guide beam.
///
/// Stores the low halfword of a signed angle, in 4096 units per turn. Drawing
/// interprets it as s16; a refresh replaces it before constructing a far cap.
static u16 _gBeamScreenAngle;

/// Cached ground-glow screen direction, independent of the airborne beam's angle.
///
/// Stores the low halfword of a signed angle, in 4096 units per turn, interpreted
/// as s16 by the ground-strip drawer.
static u16 _gGroundBeamScreenAngle;

/// Requests an airborne-beam direction refresh after an endpoint projection fails.
///
/// Boolean halfword (0 clear, 1 pending). Cleared at the start of each beam tick;
/// within the tick, the next accepted segment refreshes the angle and far cap.
static s16 _gBeamAngleRefreshPending;

/// Requests a ground-glow direction refresh after an endpoint projection fails.
///
/// Boolean halfword (0 clear, 1 pending), independent of the airborne request.
/// Cleared at each beam tick and consumed by the next accepted ground segment.
static s16 _gGroundBeamAngleRefreshPending;

/// Copied world-space contact endpoint of the current guide beam, in game units.
///
/// XYZ are narrowed to signed halfwords. Coordinates are meaningful only while
/// `_gTrackedImpactValid` is true; invalidation retains the stale point.
static SVECTOR _gTrackedImpactPoint;

/// Whether the guide beam should end at its copied contact rather than its full reach.
///
/// Cleared on guide initialization or a missing contact; set when the contact's
/// XYZ translation is copied. It owns no pointer into the contact's scratch storage.
static bool _gTrackedImpactValid;

static void _setTrackedImpactPoint(const long* worldTranslation);

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
#define ALLOCATE_BEAM_QUAD(quad, red, green, blue, secondRed, secondGreen, secondBlue) \
    {                                                                                  \
        (quad)         = gGpuPrimCursor;                                               \
        gGpuPrimCursor = (quad) + 1;                                                   \
        setPolyG4(quad);                                                               \
        setRGB0(quad, 0, 0, 0);                                                        \
        setRGB1(quad, 0, 0, 0);                                                        \
        setRGB2(quad, (red), (green), (blue));                                         \
        setRGB3(quad, (secondRed), (secondGreen), (secondBlue));                       \
    }

void m4a1JavelinGuideBeamTask(Task* task)
{
    enum {
        GUIDE_INIT         = 0,
        GUIDE_MUZZLE_FLARE = 1,
        GUIDE_BEAM         = 2,
        BEAM_SEGMENT_COUNT = ARRAY_SIZE(_gBeamSegmentFlags),
        // Actor mode occupies the low halfword, normal firing state 4 the high.
        PLAYER_FIRING_MODE_STATE = (PLAYER_ACTOR_STATE_ATTACK << 16) | GAME_ACTOR_MODE_NORMAL
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
        case GUIDE_INIT:
            // Attach the beam to the weapon and reset its shared target/directions.
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = _gGuideBeamOffset.vx;
            coord->coord.t[1]   = _gGuideBeamOffset.vy;
            coord->coord.t[2]   = _gGuideBeamOffset.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state             = GUIDE_MUZZLE_FLARE;
            work->move.vy           = PE2_OPTION(BEAM_LENGTH);
            work->move.vx           = 0;
            work->move.vz           = 0;
            _gTrackedImpactPoint.vx = 0;
            _gTrackedImpactPoint.vy = 0;
            _gTrackedImpactPoint.vz = 0;
            _gTrackedImpactValid    = false;
            work->period            = PE2_OPTION(FLARE_INITIAL_RADIUS);
            work->step              = ARRAY_SIZE(_gBeamFadeColours) - 1;
            _gGroundBeamScreenAngle = 0;
            _gBeamScreenAngle       = 0;
            /* fallthrough */
        case GUIDE_MUZZLE_FLARE:
            // Move the narrowing flare ring forward while its spokes brighten.
            actorRenderComposeCoord(coord);
            transientLight->framesLeft = PE2_OPTION(LIGHT_LIFETIME_FRAMES);
            pointLight->inner          = 0x100;
            pointLight->outer          = 0x1000;
            redIntensity               = pointLight->head.color.r >> 1;
            pointLight->head.color.r   = redIntensity;
            pointLight->head.color.g   = redIntensity >> 2;
            RANDOM_LCG_ADVANCE();
            pointLight->head.color.b = (RANDOM_LCG_DRAW() & 0x700) + 0x400;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightCoord->coord);
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->scale == PE2_OPTION(FLARE_MAX_BRIGHTNESS)) {
                task->state = GUIDE_BEAM;
            } else {
                work->scale  = work->scale + PE2_OPTION(FLARE_BRIGHTNESS_STEP);
                work->angle  = work->angle + PE2_OPTION(FLARE_FORWARD_STEP);
                work->period = work->period - PE2_OPTION(FLARE_RADIUS_DECREMENT);
            }
            nearPoint.vx = coord->workm.t[0];
            nearPoint.vy = coord->workm.t[1];
            nearPoint.vz = coord->workm.t[2];
            for (flareAngle = 0; flareAngle < FULL_TURN; flareAngle += EIGHTH_TURN) {
                farPoint.vx = (work->period * rsin(flareAngle)) >> TRIG_FRACTION_BITS;
                farPoint.vy = work->angle;
                farPoint.vz = (work->period * rcos(flareAngle)) >> TRIG_FRACTION_BITS;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&farPoint);
                gte_rtv0();
                gte_stsv(&farPoint);
                farPoint.vx = farPoint.vx + nearPoint.vx;
                farPoint.vy = farPoint.vy + nearPoint.vy;
                farPoint.vz = farPoint.vz + nearPoint.vz;
                _drawMuzzleFlareLine(&nearPoint, &farPoint, work->scale);
            }
            return;
        case GUIDE_BEAM:
            // Walk from the tracked/default far end back toward the muzzle.
            actorRenderComposeCoord(coord);
            transientLight->framesLeft = PE2_OPTION(LIGHT_LIFETIME_FRAMES);
            pointLight->inner          = 0x400;
            pointLight->outer          = 0x4000;
            RANDOM_LCG_ADVANCE();
            blueIntensity            = (RANDOM_LCG_DRAW() & 0x700) + 0x800;
            pointLight->head.color.b = blueIntensity;
            pointLight->head.color.r = blueIntensity >> 1;
            pointLight->head.color.g = blueIntensity >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightCoord->coord);
            _gBeamAngleRefreshPending       = false;
            lightCoord->composeStamp        = GRAPHICS_COORD_DIRTY;
            _gGroundBeamAngleRefreshPending = false;
            if (_gTrackedImpactValid != false) {
                nearPoint.vx = coord->workm.t[0];
                nearPoint.vy = coord->workm.t[1];
                nearPoint.vz = coord->workm.t[2];
                farPoint.vx  = _gTrackedImpactPoint.vx;
                farPoint.vy  = _gTrackedImpactPoint.vy;
                farPoint.vz  = _gTrackedImpactPoint.vz;
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
            work->pos.vx = (nearPoint.vx - farPoint.vx) / BEAM_SEGMENT_COUNT;
            work->pos.vy = (nearPoint.vy - farPoint.vy) / BEAM_SEGMENT_COUNT;
            work->pos.vz = (nearPoint.vz - farPoint.vz) / BEAM_SEGMENT_COUNT;
            nearPoint.vx = farPoint.vx;
            nearPoint.vy = farPoint.vy;
            nearPoint.vz = farPoint.vz;
            if (gRoomEffectState->groundTraceEnabled != 0) {
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&_gGroundProbeOffset);
                gte_rtv0();
                gte_stsv(&farGroundPoint);
                farGroundPoint.vx = farGroundPoint.vx + farPoint.vx;
                farGroundPoint.vy = farGroundPoint.vy + farPoint.vy;
                farGroundPoint.vz = farGroundPoint.vz + farPoint.vz;
                farPoint.vy       = farPoint.vy - PE2_OPTION(PROBE_RAISE);
                if (worldCollisionProbeGridSegment(&farGroundPoint, &farPoint, &farGroundPoint, NULL) == 1) {
                    previousHitIndex = BEAM_SEGMENT_COUNT;
                } else {
                    previousHitIndex = BEAM_SEGMENT_COUNT - 1;
                }
                farPoint.vy = farPoint.vy + PE2_OPTION(PROBE_RAISE);
                for (segmentIndex = BEAM_SEGMENT_COUNT - 1; segmentIndex >= 0; segmentIndex--) {
                    nearPoint.vx = nearPoint.vx + work->pos.vx;
                    nearPoint.vy = nearPoint.vy + work->pos.vy;
                    nearPoint.vz = nearPoint.vz + work->pos.vz;
                    _drawBeamSegment(&nearPoint, &farPoint, _gBeamSegmentFlags[segmentIndex],
                                     _gBeamFadeColours[work->step]);
                    gte_SetRotMatrix(&gGfxViewCoord.workm);
                    gte_ldv0(&_gGroundProbeOffset);
                    gte_rtv0();
                    gte_stsv(&nearGroundPoint);
                    nearGroundPoint.vx = nearGroundPoint.vx + nearPoint.vx;
                    nearGroundPoint.vy = nearGroundPoint.vy + nearPoint.vy;
                    nearGroundPoint.vz = nearGroundPoint.vz + nearPoint.vz;
                    nearPoint.vy       = nearPoint.vy - PE2_OPTION(PROBE_RAISE);
                    if (worldCollisionProbeGridSegment(&nearGroundPoint, &nearPoint, &nearGroundPoint, NULL) == 1) {
                        if (segmentIndex < previousHitIndex) {
                            _drawGroundBeamSegment(&nearGroundPoint, &farGroundPoint, _gBeamSegmentFlags[segmentIndex],
                                                   _gBeamFadeColours[work->step >> 1]);
                        }
                        previousHitIndex = segmentIndex;
                    } else {
                        previousHitIndex = segmentIndex - 1;
                    }
                    nearPoint.vy      = nearPoint.vy + PE2_OPTION(PROBE_RAISE);
                    farGroundPoint.vx = nearGroundPoint.vx;
                    farGroundPoint.vy = nearGroundPoint.vy;
                    farGroundPoint.vz = nearGroundPoint.vz;
                    farPoint.vx       = nearPoint.vx;
                    farPoint.vy       = nearPoint.vy;
                    farPoint.vz       = nearPoint.vz;
                }
            } else {
                for (segmentIndex = BEAM_SEGMENT_COUNT - 1; segmentIndex >= 0; segmentIndex--) {
                    nearPoint.vx = nearPoint.vx + work->pos.vx;
                    nearPoint.vy = nearPoint.vy + work->pos.vy;
                    nearPoint.vz = nearPoint.vz + work->pos.vz;
                    _drawBeamSegment(&nearPoint, &farPoint, _gBeamSegmentFlags[segmentIndex], PE2_OPTION(BEAM_BASE_RGB444));
                    farPoint.vx = nearPoint.vx;
                    farPoint.vy = nearPoint.vy;
                    farPoint.vz = nearPoint.vz;
                }
            }
            // Once the hold expires, consume one colour step on every active tick.
            if (work->age >= PE2_OPTION(GUIDE_HOLD_FRAMES) + 1) {
                work->step = work->step - 1;
                if (work->step < 0) {
                    effectKillTask(work, task);
                }
            } else if (*(s32*)&actor->mode != PLAYER_FIRING_MODE_STATE) {
                work->age = work->age + PE2_OPTION(GUIDE_HOLD_FRAMES);
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
/// `SEGMENT_*`. Refresh also draws the far-end cap; otherwise
/// segments share the cached screen direction (4096 angle units per turn).
/// A failed endpoint projection emits nothing and requests a direction refresh
/// on the next visible segment. Uses and releases one scratch block; packets
/// remain in the current primitive heap and ordering table until GPU completion.
static void _drawBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444)
{
    GlowFlaggedPointPairScratch* scratch;
    LINE_F2*                     line;
    POLY_G4*                     quad;
    s32                          wedgeAngle;
    s32                          farCornerAngle;
    s32                          nearCornerAngle;
    s32                          stripAngle;
    s32                          flickerBias;
    u32                          expandedRgb444;
    u8                           red;
    u8                           green;
    u8                           blue;
    u16                          beamAngle;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GlowFlaggedPointPairScratch);

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
            scratch->radius0 = PE2_OPTION(RADIUS_DEPTH_PRODUCT) / scratch->otz0;
            scratch->radius1 = PE2_OPTION(RADIUS_DEPTH_PRODUCT) / scratch->otz1;
            red              = red * 2 / 3;
            green            = green * 2 / 3;
            blue             = blue * 2 / 3;
            // Share the strip direction and cap its terminal segments.
            if ((segmentFlags & SEGMENT_REFRESH_ANGLE) || _gBeamAngleRefreshPending != false) {
                beamAngle                 = ratan2(line->y1 - line->y0, line->x0 - line->x1);
                _gBeamScreenAngle         = beamAngle;
                _gBeamAngleRefreshPending = false;
                for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + HALF_TURN; wedgeAngle += QUARTER_TURN) {
                    ALLOCATE_BEAM_QUAD(quad, red, green, blue, 0, 0, 0);
                    farCornerAngle = wedgeAngle + HALF_TURN;
                    quad->x0       = line->x1 + ((scratch->radius1 * rsin(farCornerAngle)) >> TRIG_FRACTION_BITS);
                    quad->y0       = line->y1 + ((scratch->radius1 * rcos(farCornerAngle)) >> TRIG_FRACTION_BITS);
                    farCornerAngle = wedgeAngle + (HALF_TURN + EIGHTH_TURN);
                    quad->x1       = line->x1 + ((scratch->radius1 * rsin(farCornerAngle)) >> TRIG_FRACTION_BITS);
                    quad->y1       = line->y1 + ((scratch->radius1 * rcos(farCornerAngle)) >> TRIG_FRACTION_BITS);
                    quad->x2       = line->x1;
                    quad->y2       = line->y1;
                    farCornerAngle = wedgeAngle + (HALF_TURN + QUARTER_TURN);
                    quad->x3       = line->x1 + ((scratch->radius1 * rsin(farCornerAngle)) >> TRIG_FRACTION_BITS);
                    quad->y3       = line->y1 + ((scratch->radius1 * rcos(farCornerAngle)) >> TRIG_FRACTION_BITS);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz1),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz1);
                }
            } else {
                beamAngle = _gBeamScreenAngle;
            }
            if (segmentFlags & SEGMENT_CAP_NEAR_END) {
                for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + HALF_TURN; wedgeAngle += QUARTER_TURN) {
                    ALLOCATE_BEAM_QUAD(quad, red, green, blue, 0, 0, 0);
                    quad->x0        = line->x0 + ((scratch->radius0 * rsin(wedgeAngle)) >> TRIG_FRACTION_BITS);
                    quad->y0        = line->y0 + ((scratch->radius0 * rcos(wedgeAngle)) >> TRIG_FRACTION_BITS);
                    nearCornerAngle = wedgeAngle + EIGHTH_TURN;
                    quad->x1        = line->x0 + ((scratch->radius0 * rsin(nearCornerAngle)) >> TRIG_FRACTION_BITS);
                    quad->y1        = line->y0 + ((scratch->radius0 * rcos(nearCornerAngle)) >> TRIG_FRACTION_BITS);
                    quad->x2        = line->x0;
                    quad->y2        = line->y0;
                    nearCornerAngle = wedgeAngle + QUARTER_TURN;
                    quad->x3        = line->x0 + ((scratch->radius0 * rsin(nearCornerAngle)) >> TRIG_FRACTION_BITS);
                    quad->y3        = line->y0 + ((scratch->radius0 * rcos(nearCornerAngle)) >> TRIG_FRACTION_BITS);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);
                }
            }
            for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + HALF_TURN; wedgeAngle += QUARTER_TURN) {
                ALLOCATE_BEAM_QUAD(quad, red, green, blue, red, green, blue);
                stripAngle = (s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2);
                quad->x0   = line->x0 + ((scratch->radius0 * rsin(stripAngle)) >> TRIG_FRACTION_BITS);
                quad->y0   = line->y0 + ((scratch->radius0 * rcos(stripAngle)) >> TRIG_FRACTION_BITS);
                quad->x1   = line->x1 + ((scratch->radius1 * rsin(stripAngle)) >> TRIG_FRACTION_BITS);
                quad->y1   = line->y1 + ((scratch->radius1 * rcos(stripAngle)) >> TRIG_FRACTION_BITS);
                quad->x2   = line->x0;
                quad->y2   = line->y0;
                quad->x3   = line->x1;
                quad->y3   = line->y1;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);
            }
        } else {
            _gBeamAngleRefreshPending = true;
        }
    } else {
        _gBeamAngleRefreshPending = true;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GlowFlaggedPointPairScratch));
}

/// Draws the additive ground glow between adjacent beam collision points.
///
/// Inputs and `segmentFlags` follow `_drawBeamSegment`; the caller
/// supplies two successful neighbouring probes in world coordinate units.
/// The 0x0RGB colour receives an alternating 8-unit bias and is halved.
/// Direction caching and projection-failure recovery are independent of the
/// airborne beam. Thickness scales inversely with each endpoint's depth.
/// Reserves/releases one scratch block and appends packets to the current heap.
static void _drawGroundBeamSegment(const SVECTOR* nearPoint, const SVECTOR* farPoint, u16 segmentFlags, u16 rgb444)
{
    GlowFlaggedPointPairScratch* scratch;
    LINE_F2*                     line;
    POLY_G4*                     quad;
    u32                          flickerBias;
    u32                          expandedRgb444;
    u8                           red;
    u8                           green;
    u8                           blue;
    u16                          beamAngle;
    s32                          wedgeAngle;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GlowFlaggedPointPairScratch);

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
            scratch->radius0 = PE2_OPTION(RADIUS_DEPTH_PRODUCT) / scratch->otz0;
            scratch->radius1 = PE2_OPTION(RADIUS_DEPTH_PRODUCT) / scratch->otz1;

            // Share the strip direction and cap its terminal segments.
            if ((segmentFlags & SEGMENT_REFRESH_ANGLE) || _gGroundBeamAngleRefreshPending != false) {
                beamAngle                       = ratan2(line->y1 - line->y0, line->x0 - line->x1);
                _gGroundBeamScreenAngle         = beamAngle;
                _gGroundBeamAngleRefreshPending = false;
                for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + HALF_TURN; wedgeAngle += QUARTER_TURN) {
                    ALLOCATE_BEAM_QUAD(quad, red, green, blue, 0, 0, 0);
                    quad->x0 = line->x1 + ((scratch->radius1 * rsin(wedgeAngle + HALF_TURN)) >> TRIG_FRACTION_BITS);
                    quad->y0 = line->y1 + ((scratch->radius1 * rcos(wedgeAngle + HALF_TURN)) >> TRIG_FRACTION_BITS);
                    quad->x1 = line->x1 + ((scratch->radius1 * rsin(wedgeAngle + (HALF_TURN + EIGHTH_TURN))) >> TRIG_FRACTION_BITS);
                    quad->y1 = line->y1 + ((scratch->radius1 * rcos(wedgeAngle + (HALF_TURN + EIGHTH_TURN))) >> TRIG_FRACTION_BITS);
                    quad->x2 = line->x1;
                    quad->y2 = line->y1;
                    quad->x3 = line->x1 + ((scratch->radius1 * rsin(wedgeAngle + (HALF_TURN + QUARTER_TURN))) >> TRIG_FRACTION_BITS);
                    quad->y3 = line->y1 + ((scratch->radius1 * rcos(wedgeAngle + (HALF_TURN + QUARTER_TURN))) >> TRIG_FRACTION_BITS);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz1), quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz1);
                }
            } else {
                beamAngle = _gGroundBeamScreenAngle;
            }

            if (segmentFlags & SEGMENT_CAP_NEAR_END) {
                for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + HALF_TURN; wedgeAngle += QUARTER_TURN) {
                    ALLOCATE_BEAM_QUAD(quad, red, green, blue, 0, 0, 0);
                    quad->x0 = line->x0 + ((scratch->radius0 * rsin(wedgeAngle)) >> TRIG_FRACTION_BITS);
                    quad->y0 = line->y0 + ((scratch->radius0 * rcos(wedgeAngle)) >> TRIG_FRACTION_BITS);
                    quad->x1 = line->x0 + ((scratch->radius0 * rsin(wedgeAngle + EIGHTH_TURN)) >> TRIG_FRACTION_BITS);
                    quad->y1 = line->y0 + ((scratch->radius0 * rcos(wedgeAngle + EIGHTH_TURN)) >> TRIG_FRACTION_BITS);
                    quad->x2 = line->x0;
                    quad->y2 = line->y0;
                    quad->x3 = line->x0 + ((scratch->radius0 * rsin(wedgeAngle + QUARTER_TURN)) >> TRIG_FRACTION_BITS);
                    quad->y3 = line->y0 + ((scratch->radius0 * rcos(wedgeAngle + QUARTER_TURN)) >> TRIG_FRACTION_BITS);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0), quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);
                }
            }

            for (wedgeAngle = (s16)beamAngle; wedgeAngle < (s16)beamAngle + HALF_TURN; wedgeAngle += QUARTER_TURN) {
                ALLOCATE_BEAM_QUAD(quad, red, green, blue, red, green, blue);
                quad->x0 = line->x0 + ((scratch->radius0 * rsin((s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2))) >> TRIG_FRACTION_BITS);
                quad->y0 = line->y0 + ((scratch->radius0 * rcos((s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2))) >> TRIG_FRACTION_BITS);
                quad->x1 = line->x1 + ((scratch->radius1 * rsin((s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2))) >> TRIG_FRACTION_BITS);
                quad->y1 = line->y1 + ((scratch->radius1 * rcos((s16)beamAngle + ((wedgeAngle - (s16)beamAngle) * 2))) >> TRIG_FRACTION_BITS);
                quad->x2 = line->x0;
                quad->y2 = line->y0;
                quad->x3 = line->x1;
                quad->y3 = line->y1;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0), quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);
            }
        } else {
            _gGroundBeamAngleRefreshPending = true;
        }
    } else {
        _gGroundBeamAngleRefreshPending = true;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GlowFlaggedPointPairScratch));
}

#undef ALLOCATE_BEAM_QUAD

/// Draws one additive muzzle-flare spoke from the muzzle to its world-space ring.
///
/// Inputs are read-only, word-aligned SVECTORs in game coordinate units.
/// `brightness` is the blue byte at the muzzle (caller supplies 32..192);
/// green/red use one half/quarter, and the ring end is black. The spoke is
/// sorted at the muzzle depth and omitted if either projection fails.
/// Reserves/releases one scratch block and appends a Gouraud line to the heap.
static void _drawMuzzleFlareLine(const SVECTOR* muzzlePoint, const SVECTOR* ringPoint, u16 brightness)
{
    _LineScratch* scratch;
    LINE_G2*      line;
    s32*          muzzleDepth;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(_LineScratch);
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
    SCRATCH_STACK_RELEASE_BLOCK(_LineScratch);
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
#include <effects/sprite_quad/draw.inc.c>

/// Sets or clears the guide beam's tracked world-space impact point.
///
/// `worldTranslation` is NULL to invalidate the point without clearing its
/// stale coordinates, or at least three readable signed translation words
/// (X, Y, Z). Each is narrowed to its low signed halfword. Values are copied;
/// the pointer is never retained, so a scratch coordinate is a valid source.
static void _setTrackedImpactPoint(const long* worldTranslation)
{
    if (worldTranslation == NULL) {
        _gTrackedImpactValid = false;
        return;
    }
    _gTrackedImpactPoint.vx = worldTranslation[0];
    _gTrackedImpactPoint.vy = worldTranslation[1];
    _gTrackedImpactValid    = true;
    _gTrackedImpactPoint.vz = worldTranslation[2];
}

void m4a1JavelinContactFlashTask(Task* task)
{
    enum {
        CONTACT_FLASH_INIT        = 0,
        CONTACT_FLASH_PLAYING     = 1,
        CONTACT_FLASH_FRAME_COUNT = 8,
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
    if (task->state == CONTACT_FLASH_INIT) {
        work->scale = PE2_OPTION(CONTACT_FLASH_SIZE);
        RANDOM_LCG_ADVANCE();
        work->angle = RANDOM_LCG_DRAW() & (FULL_TURN - 1);
        task->state = CONTACT_FLASH_PLAYING;
    }
    // Age counts active ticks from one; the texture strip is indexed from zero.
    spriteQuadDraw(coord->workm.t, work->age - 1, work->scale, work->angle);
    if (work->age == CONTACT_FLASH_FRAME_COUNT) {
        effectKillTask(work, task);
    }
}

void m4a1JavelinAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE        = 0,
        PHASE_WAIT_READY     = 1,
        PHASE_SELECT_ATTACK  = 2,
        PHASE_BURST          = 3,
        PHASE_IMPACT         = 4,
        PHASE_BEAM_DELAY     = 5,
        PHASE_BEAM_ACTIVE    = 6,
        PHASE_RECOVER        = 7,
        ATTACK_SCRATCH_BYTES = 0x58,
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
    SCRATCH_STACK_RESERVE_BYTES(ATTACK_SCRATCH_BYTES);
    rootCoord           = playerTask->extra.tmd->coords;
    actor               = playerTask->work;
    impactCoord         = SCRATCH_STACK_CURSOR(GfxCoord);
    impactCoord->parent = NULL;

    switch (actor->statePhase) {
        case PHASE_PREPARE:
            readyBlendFrames                                      = PE2_OPTION(READY_BLEND_FRAMES);
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += readyBlendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PE2_OPTION(MOVING_READY_BLEND_FRAMES);
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PHASE_SELECT_ATTACK:
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = PHASE_BURST;
                actor->attackCancelTicks                              = PE2_OPTION(BURST_CANCEL_FRAMES);
                actor->turnRateIndex                                  = 0;
                actor->stateTimer                                     = 0;
                actor->actionValue                                    = PE2_OPTION(BURST_ROUNDS);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | RIFLE_KEY_BASE;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase                                     = PHASE_BEAM_DELAY;
                actor->turnRateIndex                                  = SECONDARY_TURN_RATE_INDEX;
                actor->attackCancelTicks                              = PE2_OPTION(SECONDARY_CANCEL_FRAMES);
                actor->stateTimer                                     = PE2_OPTION(BEAM_DELAY_FRAMES);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = BEAM_KEY;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_JAVELIN), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                effectWork = effectSpawn(EFFECT_JAVELIN_GUIDE_BEAM,
                                         actor->equipmentTasks[1]->extra.tmd->coords,
                                         0x1D, NULL);
                if (effectWork != NULL) {
                    taskReparent(actor->equipmentTasks[1], effectWork->task);
                }
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_BEAM), 1);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 0, 3);
                break;
            }
            /* fallthrough */
        case PHASE_BURST:
            roundsLeft = actor->actionValue;
            if (actor->actionValue != 0) {
                burstDelay = actor->stateTimer;
                if (burstDelay == 0) {
                    actor->actionValue                                    = roundsLeft - 1;
                    actor->stateTimer                                     = PE2_OPTION(BURST_DELAY_FRAMES);
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_JAVELIN), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX_M4A1_JAVELIN, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
                } else {
                    burstDelay--;
                    actor->stateTimer = burstDelay;
                    if (burstDelay == PE2_OPTION(BURST_IMPACT_FRAMES_LEFT)) {
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
        case PHASE_IMPACT:
            actor->statePhase                                     = PHASE_RECOVER;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                effectSpawn(EFFECT_IMPACT_SPARK, impactCoord, 0, NULL);
                worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
            }
            break;
        case PHASE_BEAM_DELAY:
        case PHASE_BEAM_ACTIVE:
            beamTicksLeft     = actor->stateTimer - 1;
            actor->stateTimer = beamTicksLeft;
            if (beamTicksLeft == 0) {
                if (actor->statePhase == PHASE_BEAM_DELAY) {
                    actor->statePhase                                     = PHASE_BEAM_ACTIVE;
                    actor->stateTimer                                     = PE2_OPTION(BEAM_ACTIVE_FRAMES);
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    actor->statePhase                                     = PHASE_RECOVER;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
            }
            // The beam tracks this frame's contact; no contact clears the cached point.
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
                _setTrackedImpactPoint(impactCoord->workm.t);
                effectWork = effectSpawn(EFFECT_M4A1_JAVELIN_CONTACT_FLASH, impactCoord, 0, NULL);
                if (effectWork != NULL) {
                    taskReparent(actor->equipmentTasks[1], effectWork->task);
                }
            } else {
                _setTrackedImpactPoint(NULL);
            }
            break;
        case PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = PE2_OPTION(RECOVERY_COOLDOWN_FRAMES);
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(ATTACK_SCRATCH_BYTES);
}
