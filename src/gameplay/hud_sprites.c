#include "gameplay/hud_sprites.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/display.h"
#include "ending.h"
#include "gameplay/enemy.h"
#include "hud.h"
#include "hud_sprites.h"
#include "items.h"
#include "linked_actors.h"
#include "gameplay/loading.h"
#include "loading.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"
#include "world_targets.h"

#include "gameplay/damage.h"
#include "gameplay/room_effects.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include <psyq/rand.h>

/// Radar projection uses Q12 zoom and eight fractional bits per screen pixel.
/// Palette constants describe the sixteen-entry RGB555 ability footprint.
enum {
    HUD_RADAR_NORMAL_ZOOM_Q12          = 0x1555,
    HUD_RADAR_MOTION_DETECTOR_ZOOM_Q12 = 0xAAA,
    HUD_RADAR_POSITION_FRACTION_BITS   = 8,
    HUD_RADAR_PIXEL_ROUNDING           = 1 << (HUD_RADAR_POSITION_FRACTION_BITS - 1),
    HUD_RADAR_RANGE_FIXED              = 0x1300,
    HUD_RADAR_RANGE_SQUARED_MAX        = HUD_RADAR_RANGE_FIXED * HUD_RADAR_RANGE_FIXED - 1,
    HUD_RADAR_PALETTE_RADIUS_BIAS      = 4,
    HUD_RADAR_PALETTE_ACTIVE           = 0x9E06,
    HUD_RADAR_PALETTE_EDGE             = 0x8D03,
    HUD_RADAR_PALETTE_INACTIVE         = 0,
    HUD_RADAR_RANGE_PALETTE_X_WORDS    = 0x20,
    HUD_RADAR_RANGE_PALETTE_Y_ROWS     = 0xF2,
    HUD_RADAR_TPAGE_COMMAND            = 0xE1000000 | 0x200 | 0x3E,
    HUD_RADAR_DETECTOR_SPRITE_CODE     = 0x65,
    HUD_RADAR_RANGE_SPRITE_CODE        = 0x67,
    HUD_RADAR_TEXTURE_CLUT             = 0x3C0C,
    HUD_RADAR_RANGE_CLUT               = 0x3C82,
    HUD_RADAR_TEXTURE_PAGE             = 0x1E,
    HUD_RADAR_MAIN_TAG                 = -2,
    HUD_RADAR_RANGE_TAG                = -3,
    HUD_RADAR_TPAGE_PAYLOAD_WORDS      = sizeof(DR_TPAGE) / sizeof(u32) - 1,
    HUD_RADAR_SPRITE_PAYLOAD_WORDS     = sizeof(SPRT) / sizeof(u32) - 1
};

const char D_800938AC[8] = "????\0&!K";

/// The nine rotation coefficients of a view `MATRIX`, assigned as one value.
///
/// Laid out as `MATRIX::m`: row-major signed coefficients with 12 fractional
/// bits (`ONE` is 1.0). Applying a `ViewCamera` assigns this through both
/// matrices' `m`, so exactly these 18 bytes move; the alignment bytes before
/// `MATRIX::t` and the translation, which belongs to a different coordinate
/// node, are not part of the value. Halfword alignment is all it requires.
typedef struct {
    s16 m[3][3]; // World-to-camera rotation, row-major
} _ViewRotation;
STATIC_ASSERT_SIZEOF(_ViewRotation, 0x12);

/// Scratch-stack block in which the locked-on enemy's HP readout is placed for one frame.
///
/// The position starts as the anchor the readout is heading for. For a newly
/// locked enemy it stays there; otherwise it is replaced by the position kept
/// in `HudTargetHpReadout` advanced by one step toward the anchor. The readout
/// is drawn at the result, which is then stored back for the next frame.
/// Coordinates are pixels from the screen center, Y increasing downward.
///
/// Reserve the complete record on the scratch stack; none of its members
/// survive the matching release. The leading bytes are reserved with the block
/// and left untouched, so what the block was laid out to hold there is unproven.
typedef struct {
    byte unknown_0[0x14]; // Reserved with the block and never accessed; role unproven
    s16  x;               // Horizontal position: the anchor, then where the readout is drawn
    s16  y;               // Vertical position: the anchor, then where the readout is drawn
    s16  stepX;           // Horizontal distance left to the anchor, then an eighth of it, rounded down
    s16  stepY;           // Vertical distance left to the anchor, then an eighth of it, rounded down
} _HudTargetHpReadoutScratch;
STATIC_ASSERT_SIZEOF(_HudTargetHpReadoutScratch, 0x1C);

/// Stack workspace for one HUD hit-point readout.
///
/// `hudDrawHpReadout` keeps a single 48-byte slot, the size of a `UiObject`.
/// Separate locals would not share it. The function stores a zero content
/// origin, ordering-table index -3 and the initial panel state through
/// `uiObject`, then uses the same bytes for the amount text and the frame.
/// It does not read those panel fields back. The "HP" label is a separate
/// request, not part of this slot.
///
/// When the maximum is known, `value` holds the current amount: `digits`
/// receives the decimal text and `request` draws it. The conversion writes at
/// most ten bytes: nine digits and a terminator. The request follows those
/// sixteen bytes, clear of the text. When the maximum is negative,
/// `hiddenAmount` draws the stand-in string from the bytes `digits` occupies.
/// `frame.rect` is the outer rectangle passed to `uiDrawRectFrame` after
/// that text, on both paths, and it starts at the same byte as `value.request`.
typedef union {
    UiObject uiObject;            // Content origin, ordering-table index and initial panel state
    struct {
        u8          digits[0x10]; // Decimal text of the current amount, NUL-terminated
        TextDrawReq request;      // Placement and style for `digits`
    } value;
    TextDrawReq hiddenAmount;     // Stand-in string when the maximum is negative
    struct {
        u8   amountBytes[0x10];   // Same bytes as `value.digits` and `hiddenAmount`; not read here
        RECT rect;                // Outer rectangle in draw-environment pixels
    } frame;
} _HudHpReadoutScratch;
STATIC_ASSERT_SIZEOF(_HudHpReadoutScratch, 0x30);
STATIC_ASSERT(OFFSET_OF(_HudHpReadoutScratch, value.request) == 0x10, hud_hp_readout_value_request);
STATIC_ASSERT(OFFSET_OF(_HudHpReadoutScratch, frame.rect) == 0x10, hud_hp_readout_frame);

/// Scratch-stack block for placing one tracked target in the player's frame.
///
/// The player's frame is the first coordinate of the player's model: its
/// origin is the player's world position, and `playerInverseRotation` turns
/// world axes into the player's when that rotation is orthonormal. `position`
/// is one target's point in whichever space the work has reached. The
/// per-frame refresh takes it from `Enemy::bodyPos`, local to the enemy's
/// coordinate, through world space into the player's frame, and stores the
/// result as `Enemy::playerRelPos`. The radar starts from that stored value
/// flattened onto the ground plane, scales it by the radar zoom, and rounds it
/// to the blip's pixel offset from the radar center.
///
/// Both users reserve the complete record on the scratch stack, although the
/// radar touches only `position`; none of its members survive the matching
/// release. The bytes between the two members are reserved with the block and
/// left untouched, so what the block was laid out to hold there is unproven.
typedef struct {
    MATRIX  playerInverseRotation; // Transpose of the player's world rotation, 12 fractional bits (`ONE` is 1.0); translation unused
    byte    unknown_20[0x20];      // Reserved with the block and never accessed; role unproven
    SVECTOR position;              // One target's X, Y, Z in game units, or radar pixels once rounded; fourth word unused
} _WorldTargetPlayerFrameScratch;
STATIC_ASSERT_SIZEOF(_WorldTargetPlayerFrameScratch, 0x48);

/// Scratch workspace for expressing a transform relative to a reference frame.
///
/// Both input transforms share a containing frame. The reference rotation's
/// transpose multiplies the target rotation and rotates their origin difference;
/// this inverts the reference rotation when it is orthonormal.
///
/// Requires an initialized scratch stack with room for one 48-byte reservation,
/// released after the conversion. Only the matrix rotation and vector XYZ are
/// initialized; the matrix translation and vector's fourth word are unused.
typedef struct {
    MATRIX transposedRotation; // Reference rotation transposed, scaled by ONE (4096); t unused
    VECTOR originDelta;        // Target origin minus reference origin in the containing frame, in signed game units
} _GfxRelativeTransformScratch;
STATIC_ASSERT_SIZEOF(_GfxRelativeTransformScratch, 0x30);

u16 D_80114BB0[16];

RECT D_80114BD0;

ScreenFade D_80114BD8;

/// Starts the persistent death-screen fade using the session's signed frame count.
///
/// The fade owns no allocation for its borrowed record. No return is requested,
/// so it holds the covered screen until the area transition tears down its task.
static inline void _playClockQueueDeathFade(const GameSession* session)
{
    enum { PLAY_CLOCK_DEATH_FADE_TASK_BANK = 1,
           PLAY_CLOCK_DEATH_FADE_TASK_TYPE = 0x31 };
    ScreenFade* fade;

    fade             = &D_80114BD8;
    fade->blend      = SCREEN_FADE_SUBTRACT;
    fade->phase      = SCREEN_FADE_RUNNING;
    fade->rampFrames = session->deathFadeFrames;
    taskSpawn(PLAY_CLOCK_DEATH_FADE_TASK_BANK, PLAY_CLOCK_DEATH_FADE_TASK_TYPE, 0, fade);
}

enum {
    VIEW_CAMERA_TASK_BANK         = 0,
    VIEW_CAMERA_TASK_TYPE         = 0xF,
    VIEW_CACHED_PACKETS_TASK_TYPE = 0x17
};

/// Queues the camera preceding a mapped cursor and optional cached sprite packets.
///
/// cursor is one record past the selected camera in a live area array.
/// Camera dispatch borrows it; packet dispatch instead resolves the live view.
/// Both task allocations may fail independently, and neither result is retained.
static inline void _viewQueueCameraCursorAndPackets(const ViewCamera* cursor, s32 packetListMode)
{
    taskSpawn(VIEW_CAMERA_TASK_BANK, VIEW_CAMERA_TASK_TYPE, 0, cursor - 1);
    if (packetListMode == VIEW_PACKET_LIST_SELECTED) {
        taskSpawn(VIEW_CAMERA_TASK_BANK, VIEW_CACHED_PACKETS_TASK_TYPE, 0, 0);
    }
    if (packetListMode == VIEW_PACKET_LIST_DEFAULT) {
        taskSpawnOnDefaultList(VIEW_CAMERA_TASK_BANK, VIEW_CACHED_PACKETS_TASK_TYPE, 0, 0);
    }
}

/// Tests whether the live shooting-gallery session uses training ability levels.
///
/// Borrows a live player record and checks its resource variant only in the
/// training stage/area. Returns 0 or 1 and retains no pointer.
static __inline__ s32 _attachmentUsesTrainingLevels(const PlayerStatus* player)
{
    enum { ATTACHMENT_TRAINING_RESOURCE_VARIANT = 4 };
    s32 training;

    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0, 0)) {
        training = 0;
    } else {
        training = player->resourceVariant == ATTACHMENT_TRAINING_RESOURCE_VARIANT;
    }
    return training;
}

static void _hudDrawTargetHpReadout(Enemy* enemy, HudTargetHpReadout* readout);

static inline void _worldTargetRotatePosition(const MATRIX* rotation, SVECTOR* position);

static void _gameDebugStartInputReplay(void);

static s32 _sceneIsBattleEndDelayClear(void);

/// Tests the engaged-battle holds and the independent battle-end delay.
///
/// Borrows the live combat record and returns 0 or 1 without changing it.
/// A resumed phase's holds alone do not pass; a nonzero end delay passes in
/// every phase. Holds count outstanding references, while the delay counts frames.
static __inline__ s32 _sceneHasBattleHoldOrEndDelay(const SceneCombatState* combat)
{
    s32 hasHoldOrDelay;

    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
        hasHoldOrDelay = 1;
    } else {
        hasHoldOrDelay = 0;
    }
    return hasHoldOrDelay;
}

static s32 _attachmentMakeTextId(s32 abilityIndex, s32 level);

static s32 _attachmentStepLearnedSpell(s32 abilityIndex, s32 steps);

static void _attachmentEnqueueBattleSoundLoad(u8 fileIndex);

static s32 _attachmentIsBattleSoundLoadReady(void);

static s32 _hudCanSwitchCategory(s32 ignoreSwapLock);

static s32 func_800A7F2C(s32 value);

static __inline__ void _gfxCoordToReference(GfxCoord* coord, GfxCoord* reference, GfxCoord* outCoord);

/// Writes the translation component of a reference-relative transform.
///
/// The input origins share a containing frame. Computes
/// `out->t = scratch->transposedRotation.m * (target->t - reference->t)`
/// with GTE long-vector arithmetic, preserving signed 32-bit coordinate units.
/// The caller must supply the reference rotation's transpose in
/// `scratch->transposedRotation`, with `ONE` (4096) representing 1.0.
/// The transpose gives an inverse frame conversion for orthonormal rotations.
///
/// All pointers must be word-aligned and the caller-owned workspace disjoint
/// from the matrices. Overwrites `scratch->originDelta` XYZ; its fourth word
/// is unused. Saves all three differences before storing the three `out->t`
/// words, so `out` may equal either input matrix. Leaves the output rotation
/// and alignment bytes untouched. Changes GTE rotation and arithmetic state;
/// retains no pointers and does not reserve or release the workspace.
static __inline__ void _gfxWriteRelativeTranslation(const MATRIX* reference, const MATRIX* target, MATRIX* out,
                                                    _GfxRelativeTransformScratch* scratch)
{
    scratch->originDelta.vx = target->t[0] - reference->t[0];
    scratch->originDelta.vy = target->t[1] - reference->t[1];
    scratch->originDelta.vz = target->t[2] - reference->t[2];
    // The SDK output view writes XYZ only, without a VECTOR's fourth word.
    ApplyMatrixLV(&scratch->transposedRotation, &scratch->originDelta, (VECTOR*)out->t);
}

static void _viewSetFromCoord(GfxCoord* cameraCoord, const VECTOR* offset);

static s32 _viewQueueCoord(GfxCoord* cameraCoord, const VECTOR* offset);

static void _viewResetTransform(void);

static void _viewQueueDefaultTransform(void);

/// Installs a camera record in the active view chain and resets projection.
///
/// Copies the ONE-scaled world-to-camera rotation and negated world origin
/// into their separate nodes, clears the outer XYZ offset, and invalidates
/// all three composition caches. Parent links and the remaining matrix
/// components stay intact. Projection uses the low 16 bits of
/// `camera->screenDistance` in pixels, with a screen offset of (0, 0).
///
/// Requires an initialized view chain and a readable, word-aligned camera
/// disjoint from its nodes. Borrows the record only for the call, retains no
/// pointer, and changes GTE projection state without composing the view.
static __inline__ void _viewWriteCameraState(const ViewCamera* camera)
{
    GfxCoord*      viewOffset;
    _ViewRotation* viewRotation;
    VECTOR3*       viewTranslation;

    viewRotation    = (_ViewRotation*)gGfxViewRotCoord.coord.m;
    viewTranslation = MATRIX_TRANS(&gGfxViewCoord.coord);
    viewOffset      = &Gfx_ViewOffsetCoord;

    // Copy only coefficients and XYZ into separate nodes; leave matrix alignment bytes intact.
    *viewRotation    = *(const _ViewRotation*)camera->transform.m;
    *viewTranslation = *(const VECTOR3*)camera->transform.t;

    viewOffset->coord.t[0] = 0;
    viewOffset->coord.t[1] = 0;
    viewOffset->coord.t[2] = 0;

    gDisplayState.screenDistance = camera->screenDistance;
    gte_SetGeomScreen(camera->screenDistance);
    gte_SetGeomOffset(0, 0);

    viewOffset->composeStamp                                    = GRAPHICS_COORD_DIRTY;
    PARENT_OF(viewRotation, GfxCoord, coord.m)->composeStamp    = GRAPHICS_COORD_DIRTY;
    PARENT_OF(viewTranslation, GfxCoord, coord.t)->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies the camera immediately before a one-based area cursor.
///
/// Destination component views belong to the initialized active view nodes.
/// The cursor must follow a readable camera in the loaded area array; the
/// record and the destination storage must be disjoint. Resets projection and
/// invalidates all three caches, retaining no pointers. Rotation coefficients
/// have 12 fractional bits; origin XYZ uses signed game units. The destination
/// views must be `gGfxViewRotCoord.coord`, `gGfxViewCoord.coord.t` and
/// `Gfx_ViewOffsetCoord` respectively. Projection distance retains its low 16 bits.
static __inline__ void _viewApplyCameraCursor(const ViewCamera* cursor, MATRIX* rotation, VECTOR3* translation, GfxCoord* offset)
{
    // Copy the 18-byte coefficient and 12-byte origin views, excluding alignment bytes.
    *(_ViewRotation*)rotation->m = *(const _ViewRotation*)(cursor - 1)->transform.m;
    *translation                 = *MATRIX_TRANS(&(cursor - 1)->transform);
    cursor--;
    offset->coord.t[0]           = 0;
    offset->coord.t[1]           = 0;
    offset->coord.t[2]           = 0;
    gDisplayState.screenDistance = cursor->screenDistance;
    gte_SetGeomScreen(cursor->screenDistance);
    gte_SetGeomOffset(0, 0);
    offset->composeStamp                                    = GRAPHICS_COORD_DIRTY;
    PARENT_OF(rotation, GfxCoord, coord)->composeStamp      = GRAPHICS_COORD_DIRTY;
    PARENT_OF(translation, GfxCoord, coord.t)->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Uploads the radar footprint's sixteen RGB555 entries for a scaled radius.
///
/// `scaledRange` is a signed radius with eight fractional bits per pixel,
/// already adjusted for radar zoom. Clamps the radius to 19 pixels, floors it
/// and subtracts the texture's four-pixel bias, then clamps the edge index to
/// at least one. Indices before that edge are active; the edge gets a dimmer
/// colour unless it is the final entry, which always remains transparent.
/// Negative ranges therefore use the same minimum footprint as small ranges.
///
/// Uploads all sixteen RGB555 entries to (32 VRAM words, 242 rows), one row
/// high. The global palette and transfer rectangle must remain unchanged until
/// GPU consumption; the palette's word alignment serves the SDK upload API.
static inline void _hudUploadRadarRangePalette(s32 scaledRange)
{
    s32 paletteEdgeIndex;
    s32 paletteIndex;

    paletteEdgeIndex = scaledRange;
    if (paletteEdgeIndex > HUD_RADAR_RANGE_FIXED) {
        paletteEdgeIndex = HUD_RADAR_RANGE_FIXED;
    }
    paletteEdgeIndex >>= HUD_RADAR_POSITION_FRACTION_BITS;
    paletteEdgeIndex  -= HUD_RADAR_PALETTE_RADIUS_BIAS;
    if (paletteEdgeIndex <= 0) {
        paletteEdgeIndex = 1;
    }
    for (paletteIndex = 0; paletteIndex < ARRAY_SIZE(D_80114BB0); paletteIndex++) {
        if (paletteIndex < paletteEdgeIndex) {
            D_80114BB0[paletteIndex] = HUD_RADAR_PALETTE_ACTIVE;
        } else {
            D_80114BB0[paletteIndex] = HUD_RADAR_PALETTE_INACTIVE;
        }
        if (paletteIndex == paletteEdgeIndex && paletteIndex != ARRAY_SIZE(D_80114BB0) - 1) {
            D_80114BB0[paletteIndex] = HUD_RADAR_PALETTE_EDGE;
        }
    }
    D_80114BD0.x = HUD_RADAR_RANGE_PALETTE_X_WORDS;
    D_80114BD0.y = HUD_RADAR_RANGE_PALETTE_Y_ROWS;
    D_80114BD0.w = ARRAY_SIZE(D_80114BB0);
    D_80114BD0.h = 1;
    // LoadImage consumes RGB555 halfwords through its word-oriented payload API.
    LoadImage(&D_80114BD0, (u_long*)D_80114BB0);
}

void hudDrawRadar(HudState* hud)
{
    _WorldTargetPlayerFrameScratch* playerFrame;
    WorldTargetNode*                target;
    s32                             hasArmorMotionDetector;
    s32                             radarX;
    s32                             centerX;
    s32                             centerY;
    s32                             radarY;
    s16                             scaledX;
    s32                             scaledZ;
    s32                             scaledRange;
    DR_TPAGE*                       texturePage;
    SPRT*                           detectorSprite;
    SPRT*                           rangeSprite;
    POLY_GT4*                       radarQuad;

    radarX  = 0x61;
    radarY  = -0x6C;
    radarY -= gDisplayState.vramYOffset;
    centerX = radarX + 0x23;
    centerY = radarY + 0x23;
    hudDrawRadarMarker(centerX, centerY, HUD_RADAR_MARKER_PLAYER);
    target                 = gWorldTargetListHead;
    playerFrame            = SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetPlayerFrameScratch);
    hasArmorMotionDetector = equipmentHasEffect(EQUIPMENT_EFFECT_ARMOR_MOTION_DETECTOR);
    // Map tracked enemy offsets into the player-oriented radar circle.
    for (; target != NULL; target = target->next) {
        if ((target->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
            playerFrame->position.vx = GP_NODE_ENEMY(target)->playerRelPos.vx;
            playerFrame->position.vz = GP_NODE_ENEMY(target)->playerRelPos.vz;
            playerFrame->position.vy = 0;
            if (hasArmorMotionDetector == 0) {
                gte_lddp(HUD_RADAR_NORMAL_ZOOM_Q12);
                gte_ldsv(&playerFrame->position);
                gte_gpf12();
                gte_stsv(&playerFrame->position);
            } else {
                gte_lddp(HUD_RADAR_MOTION_DETECTOR_ZOOM_Q12);
                gte_ldsv(&playerFrame->position);
                gte_gpf12();
                gte_stsv(&playerFrame->position);
            }
            if (target->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
                continue;
            }
            scaledX = playerFrame->position.vx;
            if (scaledX < -HUD_RADAR_RANGE_FIXED || scaledX > HUD_RADAR_RANGE_FIXED) {
                continue;
            }
            scaledZ = playerFrame->position.vz;
            if (scaledZ > HUD_RADAR_RANGE_FIXED) {
                continue;
            }
            if (scaledZ < -HUD_RADAR_RANGE_FIXED) {
                continue;
            }
            if (scaledX * scaledX + scaledZ * scaledZ > HUD_RADAR_RANGE_SQUARED_MAX) {
                continue;
            }
            playerFrame->position.vx = (s16)(scaledX + HUD_RADAR_PIXEL_ROUNDING) >> HUD_RADAR_POSITION_FRACTION_BITS;
            scaledZ                  = (s16)(playerFrame->position.vz + HUD_RADAR_PIXEL_ROUNDING) >> HUD_RADAR_POSITION_FRACTION_BITS;
            playerFrame->position.vz = scaledZ;
            if (target->state.parts.targeted != 0) {
                hudDrawRadarMarker(centerX + playerFrame->position.vx, centerY - scaledZ, HUD_RADAR_MARKER_TARGETED);
            } else {
                hudDrawRadarMarker(centerX + playerFrame->position.vx, centerY - scaledZ, HUD_RADAR_MARKER_ENEMY);
            }
        }
    }
    scaledRange = hud->radarRange;
    if (hasArmorMotionDetector == 0) {
        scaledRange *= 2;
    }
    texturePage    = gGpuPrimCursor;
    gGpuPrimCursor = texturePage + 1;
    setlen(texturePage, HUD_RADAR_TPAGE_PAYLOAD_WORDS);
    texturePage->code[0] = HUD_RADAR_TPAGE_COMMAND;
    addPrim(gGpuCurrentOt + HUD_RADAR_MAIN_TAG, texturePage);
    texturePage    = gGpuPrimCursor;
    gGpuPrimCursor = texturePage + 1;
    setlen(texturePage, HUD_RADAR_TPAGE_PAYLOAD_WORDS);
    texturePage->code[0] = HUD_RADAR_TPAGE_COMMAND;
    addPrim(gGpuCurrentOt + HUD_RADAR_RANGE_TAG, texturePage);
    if (hasArmorMotionDetector == 1) {
        detectorSprite       = gGpuPrimCursor;
        gGpuPrimCursor       = detectorSprite + 1;
        detectorSprite->x0   = radarX + 0xD;
        detectorSprite->y0   = radarY + 0xC;
        detectorSprite->h    = 0x28;
        detectorSprite->w    = 0x28;
        detectorSprite->u0   = 0x60;
        detectorSprite->v0   = 0xC0;
        detectorSprite->clut = HUD_RADAR_TEXTURE_CLUT;
        setlen(detectorSprite, HUD_RADAR_SPRITE_PAYLOAD_WORDS);
        setcode(detectorSprite, HUD_RADAR_DETECTOR_SPRITE_CODE);
        addPrim(gGpuCurrentOt + HUD_RADAR_MAIN_TAG, detectorSprite);
    }
    radarQuad                              = gGpuPrimCursor;
    gGpuPrimCursor                         = radarQuad + 1;
    GPU_PRIMITIVE_COLOR_WORD(radarQuad, 2) = GPU_PACK_COLOR_WORD(0xc0, 0xc0, 0xc0, 0);
    GPU_PRIMITIVE_COLOR_WORD(radarQuad, 3) = GPU_PACK_COLOR_WORD(0x80, 0x80, 0x80, 0);
    GPU_PRIMITIVE_COLOR_WORD(radarQuad, 0) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0x40, 0);
    GPU_PRIMITIVE_COLOR_WORD(radarQuad, 1) = GPU_PACK_COLOR_WORD(0x30, 0x30, 0x30, 0);
    radarQuad->x1 = radarQuad->x3 = radarX + 0x40;
    radarQuad->y2 = radarQuad->y3 = radarY + 0x40;
    radarQuad->tpage              = HUD_RADAR_TEXTURE_PAGE;
    radarQuad->clut               = HUD_RADAR_TEXTURE_CLUT;
    setUV4(radarQuad, 0x60, 0x80, 0xA0, 0x80, 0x60, 0xC0, 0xA0, 0xC0);
    setPolyGT4(radarQuad);
    radarQuad->x0 = radarQuad->x2 = radarX;
    radarQuad->y0 = radarQuad->y1 = radarY;
    addPrim(gGpuCurrentOt + HUD_RADAR_MAIN_TAG, radarQuad);
    // Draw the requested ability footprint and upload its radial palette.
    if (hud->radarRangeIcon != HUD_RADAR_RANGE_NONE) {
        rangeSprite     = gGpuPrimCursor;
        gGpuPrimCursor  = rangeSprite + 1;
        rangeSprite->x0 = radarX + 0xD;
        rangeSprite->y0 = radarY + 0xC;
        rangeSprite->h  = 0x28;
        rangeSprite->w  = 0x28;
        if (hud->radarRangeIcon != HUD_RADAR_RANGE_PROJECTILE) {
            if (hud->radarRangeIcon == HUD_RADAR_RANGE_AROUND) {
                rangeSprite->u0 = 0x88;
            } else {
                rangeSprite->u0 = 0xD8;
            }
        } else {
            rangeSprite->u0 = 0xB0;
        }
        rangeSprite->v0   = 0xC0;
        rangeSprite->clut = HUD_RADAR_RANGE_CLUT;
        setlen(rangeSprite, HUD_RADAR_SPRITE_PAYLOAD_WORDS);
        setcode(rangeSprite, HUD_RADAR_RANGE_SPRITE_CODE);
        addPrim(gGpuCurrentOt + HUD_RADAR_RANGE_TAG, rangeSprite);
        uiQueueTexturePage(HUD_RADAR_RANGE_TAG, GPU_BLEND_ADD);
        _hudUploadRadarRangePalette(scaledRange);
        hud->radarRangeIcon = HUD_RADAR_RANGE_NONE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetPlayerFrameScratch);
}

/// Draws a readout amount, clamping a writable amount lvalue before text conversion.
///
/// Captures the function's `scratch` slot and `HUD_HP_TEXT_COLOR_RGB`.
/// Pen X/Y are each evaluated once. `amountValue` is read by the clamp and
/// conversion, so it must be a local lvalue without side effects.
/// Writes only the slot's value view.
#define HUD_DRAW_HP_AMOUNT(amountValue, penX, penY)                                                    \
    {                                                                                                  \
        if ((amountValue) < 0) {                                                                       \
            (amountValue) = 0;                                                                         \
        }                                                                                              \
        scratch.value.request.x          = (penX);                                                     \
        scratch.value.request.y          = (penY);                                                     \
        scratch.value.request.otIndex    = -2;                                                         \
        scratch.value.request.colorRgb   = HUD_HP_TEXT_COLOR_RGB;                                      \
        scratch.value.request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                    \
        scratch.value.request.alignment  = TEXT_ALIGNMENT_RIGHT;                                       \
        scratch.value.request.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;                             \
        textDrawString(&scratch.value.request, textItoaUnsigned(scratch.value.digits, (amountValue))); \
    }

void hudDrawHpReadout(s32 x, s32 y, s32 hp, s32 hpMax, s32 layout)
{
    enum {
        HUD_HP_COMPANION_BAR_PIXELS  = 37,
        HUD_HP_ENEMY_BAR_PIXELS      = 45,
        HUD_HP_FRAME_STYLE           = 0x40000 | USER_INTERFACE_PANEL_TITLE_STYLE,
        HUD_HP_FILL_TILE_CODE        = 0x60,
        HUD_HP_CAP_RAW_SPRITE_CODE   = 0x75,
        HUD_HP_BAR_RAW_QUAD_CODE     = 0x2D,
        HUD_HP_TEXT_COLOR_RGB        = 0x606060,
        HUD_HP_HIDDEN_TEXT_COLOR_RGB = 0x037A78,
        HUD_HP_BAR_CLUT              = 0x3C0B,
        HUD_HP_BAR_TPAGE             = 0x3E,
        HUD_HP_COMPANION_FILL_COLOR  = GPU_PACK_COLOR_WORD(0x1F, 0x74, 0x01, 0),
        HUD_HP_ENEMY_FILL_COLOR      = GPU_PACK_COLOR_WORD(0x80, 0, 0, 0),
        HUD_HP_FILL_PAYLOAD_WORDS    = sizeof(TILE) / sizeof(u32) - 1,
        HUD_HP_CAP_PAYLOAD_WORDS     = sizeof(SPRT_8) / sizeof(u32) - 1,
        HUD_HP_BAR_PAYLOAD_WORDS     = sizeof(POLY_FT4) / sizeof(u32) - 1
    };
    _HudHpReadoutScratch scratch;
    TextDrawReq          labelRequest;
    TILE*                fillTile;
    SPRT*                capSprite;
    POLY_FT4*            barQuad;
    s32                  barSpan;
    s32                  barRight;
    s32                  fillWidth;
    s32                  frameOtIndex;

    barSpan = HUD_HP_COMPANION_BAR_PIXELS;
    if (hp < 0) {
        hp = 0;
    }
    y -= gDisplayState.vramYOffset;
    if (Pad_RemapState->hideHud != 0) {
        return;
    }

    // Panel view of the readout slot. The amount text and frame reuse these bytes.
    frameOtIndex                                        = -3;
    scratch.uiObject.panel.contentOriginX.unsignedValue = 0;
    scratch.uiObject.panel.contentOriginY.unsignedValue = 0;
    scratch.uiObject.panel.otIndex.signedValue          = frameOtIndex;
    scratch.uiObject.panel.state                        = USER_INTERFACE_PANEL_INITIAL;

    labelRequest.x          = x + 4;
    labelRequest.y          = y + 8;
    labelRequest.otIndex    = -2;
    labelRequest.colorRgb   = HUD_HP_TEXT_COLOR_RGB;
    labelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    labelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    labelRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&labelRequest, Gp_StrHP);

    if (hpMax >= 0) {
        s32 amount = hp;

        if (layout == HUD_HP_READOUT_COMPANION) {
            s32 amountX = x + 0x2B;
            s32 amountY = y + 0xA;

            HUD_DRAW_HP_AMOUNT(amount, amountX, amountY);
        } else {
            s32 amountX = x + 0x33;
            s32 amountY = y + 0xA;

            HUD_DRAW_HP_AMOUNT(amount, amountX, amountY);
            barSpan = HUD_HP_ENEMY_BAR_PIXELS;
        }

        // Scale known HP to the layout's pixel span, capped at a full bar.
        if (hpMax == 0) {
            fillWidth = barSpan;
        } else {
            fillWidth = hp * barSpan / hpMax;
        }

        if (fillWidth > 0) {
            fillTile       = gGpuPrimCursor;
            gGpuPrimCursor = fillTile + 1;
            if (barSpan < fillWidth) {
                fillWidth = barSpan;
            }
            fillTile->x0 = x + 5;
            fillTile->y0 = y + 0xE;
            fillTile->w  = fillWidth;
            fillTile->h  = 2;
            if (layout == HUD_HP_READOUT_COMPANION) {
                GPU_PRIMITIVE_COLOR_WORD(fillTile, 0) = HUD_HP_COMPANION_FILL_COLOR;
            } else {
                GPU_PRIMITIVE_COLOR_WORD(fillTile, 0) = HUD_HP_ENEMY_FILL_COLOR;
            }
            setlen(fillTile, HUD_HP_FILL_PAYLOAD_WORDS);
            setcode(fillTile, HUD_HP_FILL_TILE_CODE);
            addPrim(gGpuCurrentOt - 2, fillTile);
        }

        // Fixed-size sprites reserve SPRT slots but transmit only the SPRT_8 prefix.
        capSprite       = gGpuPrimCursor;
        gGpuPrimCursor  = capSprite + 1;
        capSprite->x0   = x + 4;
        capSprite->u0   = 0x98;
        capSprite->y0   = y + 0xB;
        capSprite->v0   = 0x68;
        capSprite->clut = HUD_HP_BAR_CLUT;
        setlen(capSprite, HUD_HP_CAP_PAYLOAD_WORDS);
        setcode(capSprite, HUD_HP_CAP_RAW_SPRITE_CODE);
        addPrim(gGpuCurrentOt - 2, capSprite);

        capSprite       = gGpuPrimCursor;
        gGpuPrimCursor  = capSprite + 1;
        barRight        = (barSpan + x) - 2;
        capSprite->x0   = barRight;
        capSprite->y0   = y + 0xB;
        capSprite->clut = HUD_HP_BAR_CLUT;
        capSprite->u0   = 0xA8;
        capSprite->v0   = 0x68;
        setlen(capSprite, HUD_HP_CAP_PAYLOAD_WORDS);
        setcode(capSprite, HUD_HP_CAP_RAW_SPRITE_CODE);
        addPrim(gGpuCurrentOt - 2, capSprite);

        barQuad        = gGpuPrimCursor;
        gGpuPrimCursor = barQuad + 1;
        barQuad->x0 = barQuad->x2 = x + 0xC;
        barQuad->x1 = barQuad->x3 = barRight;
        barQuad->y2 = barQuad->y3 = y + 0x13;
        barQuad->u2 = barQuad->u0 = 0xA0;
        barQuad->v3 = barQuad->v2 = 0x70;
        barQuad->tpage            = HUD_HP_BAR_TPAGE;
        barQuad->y0 = barQuad->y1 = y + 0xB;
        barQuad->v0               = 0x68;
        barQuad->u1               = 0xA8;
        barQuad->v1               = 0x68;
        barQuad->clut             = HUD_HP_BAR_CLUT;
        barQuad->u3               = 0xA8;
        setlen(barQuad, HUD_HP_BAR_PAYLOAD_WORDS);
        setcode(barQuad, HUD_HP_BAR_RAW_QUAD_CODE);
        addPrim(gGpuCurrentOt - 2, barQuad);
    } else {
        scratch.hiddenAmount.x          = x + 0x33;
        scratch.hiddenAmount.y          = y + 0xA;
        scratch.hiddenAmount.otIndex    = -2;
        scratch.hiddenAmount.colorRgb   = HUD_HP_HIDDEN_TEXT_COLOR_RGB;
        scratch.hiddenAmount.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        scratch.hiddenAmount.alignment  = TEXT_ALIGNMENT_RIGHT;
        scratch.hiddenAmount.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&scratch.hiddenAmount, D_800938AC);
        barSpan = HUD_HP_ENEMY_BAR_PIXELS;
    }

    // The frame reuses the value request's bytes after the amount has been drawn.
    scratch.frame.rect.x = x;
    scratch.frame.rect.y = y;
    scratch.frame.rect.w = barSpan + 0xA;
    scratch.frame.rect.h = 0x14;
    uiDrawRectFrame(&scratch.frame.rect, -1, HUD_HP_FRAME_STYLE, NULL);
}

#undef HUD_DRAW_HP_AMOUNT

/// Draws one enemy's HP at the radar-dependent anchor, easing its stored position.
///
/// Borrows the live enemy and writable readout; the saved enemy pointer is an
/// identity comparison only. Uses one complete scratch reservation until the
/// draw ends. A missing parameter record retains placement without drawing.
static void _hudDrawTargetHpReadout(Enemy* enemy, HudTargetHpReadout* readout)
{
    enum {
        HUD_TARGET_HP_ANCHOR_X               = 106,
        HUD_TARGET_HP_ANCHOR_Y_WITH_RADAR    = -53,
        HUD_TARGET_HP_ANCHOR_Y_WITHOUT_RADAR = -100,
        HUD_TARGET_HP_EASE_SHIFT             = 3
    };
    _HudTargetHpReadoutScratch* scratch;
    s32                         hpMax;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_HudTargetHpReadoutScratch);
    if (equipmentHasEffect(EQUIPMENT_EFFECT_MOTION_DETECTOR) != 0) {
        scratch->x = HUD_TARGET_HP_ANCHOR_X;
        scratch->y = HUD_TARGET_HP_ANCHOR_Y_WITH_RADAR;
    } else {
        scratch->x = HUD_TARGET_HP_ANCHOR_X;
        scratch->y = HUD_TARGET_HP_ANCHOR_Y_WITHOUT_RADAR;
    }
    if (readout->enemy != enemy) {
        // A new target starts at the anchor. The second store also lands in
        // `x`; both members are rewritten after the draw.
        readout->enemy = enemy;
        readout->x     = scratch->x;
        readout->x     = scratch->y;
    } else {
        // The same target eases an eighth of the way from where it was last
        // drawn toward the anchor.
        scratch->stepX   = scratch->x - readout->x;
        scratch->stepY   = scratch->y - readout->y;
        scratch->stepX >>= HUD_TARGET_HP_EASE_SHIFT;
        scratch->stepY >>= HUD_TARGET_HP_EASE_SHIFT;
        scratch->x       = readout->x + scratch->stepX;
        scratch->y       = readout->y + scratch->stepY;
    }
    if (enemy->param != NULL) {
        hpMax = enemy->param->hpMax;
        if (enemy->node.state.parts.flags & WORLD_TARGET_HIDE_HP) {
            hpMax = HUD_HP_READOUT_HIDDEN_MAX;
        }
        hudDrawHpReadout(scratch->x - 8, scratch->y, enemy->hp, hpMax, HUD_HP_READOUT_ENEMY);
    }
    readout->x = scratch->x;
    readout->y = scratch->y;
    SCRATCH_STACK_RELEASE_BLOCK(_HudTargetHpReadoutScratch);
}

/// Rotates a tracked position in place with GTE signed-halfword saturation.
///
/// `rotation` uses ONE (4096) for 1.0; translation is ignored. Reads the
/// complete eight-byte position through a snapshot and replaces XYZ only,
/// preserving its pad. Changes GTE rotation, vector and arithmetic state.
/// The matrix must be word-aligned and the position halfword-aligned.
static inline void _worldTargetRotatePosition(const MATRIX* rotation, SVECTOR* position)
{
    SVECTOR inputPosition;

    inputPosition = *position;
    gte_ApplyMatrixSV(rotation, &inputPosition, position);
}

void worldTargetUpdatePlayerRelativePositions(void)
{
    /// Places a live embedded enemy's body anchor in its composed cache frame.
    ///
    /// Requires a current coordinate cache and separate halfword-aligned SVECTOR.
    /// Narrows local XYZ, saturates rotation, then wraps translation to signed
    /// halfwords. Preserves the pad and cache stamp. Arguments are evaluated
    /// repeatedly and must have no side effects; captures no local identifiers.
#define WORLD_TARGET_WRITE_BODY_IN_COMPOSED_FRAME(targetNode, position)                   \
    {                                                                                     \
        (position)->vx = GP_NODE_ENEMY(targetNode)->bodyPos.vx;                           \
        (position)->vy = GP_NODE_ENEMY(targetNode)->bodyPos.vy;                           \
        (position)->vz = GP_NODE_ENEMY(targetNode)->bodyPos.vz;                           \
        _worldTargetRotatePosition(&GP_NODE_ENEMY(targetNode)->coord->workm, (position)); \
        (position)->vx += GP_NODE_ENEMY(targetNode)->coord->workm.t[0];                   \
        (position)->vy += GP_NODE_ENEMY(targetNode)->coord->workm.t[1];                   \
        (position)->vz += GP_NODE_ENEMY(targetNode)->coord->workm.t[2];                   \
    }
    WorldTargetNode*                node;
    Task*                           playerTask;
    GfxCoord*                       playerCoord;
    _WorldTargetPlayerFrameScratch* scratch;

    node       = gWorldTargetListHead;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (playerTask == NULL) {
        return;
    }
    playerCoord = playerTask->extra.tmd->coords;
    scratch     = SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetPlayerFrameScratch);
    TransposeMatrix(&playerCoord->workm, &scratch->playerInverseRotation);
    for (; node != NULL; node = node->next) {
        if ((node->state.word & WORLD_TARGET_SCAN_MASK) == WORLD_TARGET_NOT_LOCKABLE) {
            continue;
        }
        // Move the local anchor through the common composed frame.
        WORLD_TARGET_WRITE_BODY_IN_COMPOSED_FRAME(node, &scratch->position);

        // Subtract the player origin before rotating into the player's axes.
        scratch->position.vx -= playerCoord->workm.t[0];
        scratch->position.vy -= playerCoord->workm.t[1];
        scratch->position.vz -= playerCoord->workm.t[2];
        _worldTargetRotatePosition(&scratch->playerInverseRotation, &scratch->position);
        GP_NODE_ENEMY(node)->playerRelPos.vx = scratch->position.vx;
        GP_NODE_ENEMY(node)->playerRelPos.vy = scratch->position.vy;
        GP_NODE_ENEMY(node)->playerRelPos.vz = scratch->position.vz;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetPlayerFrameScratch);
#undef WORLD_TARGET_WRITE_BODY_IN_COMPOSED_FRAME
}

void playClockAdvanceDeathSound(s16* completed)
{
    enum {
        PLAY_CLOCK_DEATH_SOUND_BANK_ENTRY      = 0x70000001,
        PLAY_CLOCK_COMPANION_DEATH_BANK_OFFSET = 0x31,
        PLAY_CLOCK_DEATH_SOUND_PENDING         = 0,
        PLAY_CLOCK_DEATH_SOUND_COMPLETE        = 1,
        PLAY_CLOCK_COMPANION_KYLE              = 1,
        PLAY_CLOCK_COMPANION_GROWTH_ROOM       = 3
    };
    PlayerStatus* player;
    s8            companionType;
    u8            restartMode;

    player      = &gPlayerStatus;
    restartMode = gGameSession->restartMode;
    if (restartMode == GAME_SESSION_RESTART_PRESERVE_DISPLAY || restartMode == GAME_SESSION_RESTART_ENDING || !cdCmdIsIdle() || *completed != PLAY_CLOCK_DEATH_SOUND_PENDING) {
        return;
    }
    if (gGameSession->deathSoundCountdown == GAME_SESSION_DEATH_SOUND_HOLD) {
        *completed = PLAY_CLOCK_DEATH_SOUND_COMPLETE;
        return;
    }
    // CD work and the caller's latch must clear before the signed byte advances.
    gGameSession->deathSoundCountdown--;
    if (gGameSession->deathSoundCountdown >= 0) {
        return;
    }
    if (player->hp <= 0) {
        sndEvtRequestScriptStart((gGameSession->deathVariant << 16) | PLAY_CLOCK_DEATH_SOUND_BANK_ENTRY, 0, 0);
    } else {
        companionType = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType;
        if (companionType == PLAY_CLOCK_COMPANION_KYLE) {
            sndEvtRequestScriptStart(((gGameSession->deathVariant + PLAY_CLOCK_COMPANION_DEATH_BANK_OFFSET) << 16) | PLAY_CLOCK_DEATH_SOUND_BANK_ENTRY, 0, 0);
        } else if (companionType == PLAY_CLOCK_COMPANION_GROWTH_ROOM) {
            sndEvtRequestScriptStop(SOUND_AREA_BANK_ALL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_DEATH, 0, 0);
        }
    }
    *completed = PLAY_CLOCK_DEATH_SOUND_COMPLETE;
}

const u8* attachmentGetLearnedLevels(void)
{
    PlayerStatus* player;
    s32           training;

    player   = &gPlayerStatus;
    training = _attachmentUsesTrainingLevels(player);
    if (training == 0) {
        return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    }
    return Gp_DebugAttachLevels;
}

s32 attachmentIsTrainingMode(void)
{
    enum { ATTACHMENT_TRAINING_RESOURCE_VARIANT = 4 };
    PlayerStatus* player;

    player = &gPlayerStatus;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) !=
        GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0, 0)) {
        return 0;
    }
    return player->resourceVariant == ATTACHMENT_TRAINING_RESOURCE_VARIANT;
}

s32 sceneIsBattleActive(void)
{
    SceneCombatState* combat;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
        return 1;
    }
    return 0;
}

s32 func_800A7550(void)
{
    Gp_ApplyAttachStats(1, NULL);
    return 0;
}

void hudReset(HudState* hud)
{
    PlayerStatus*    player;
    HudHpMp*         displayedVitals;
    AttachmentState* attachment;

    // Start from the live vitals without retaining a previous radar preview.
    player                                = &gPlayerStatus;
    displayedVitals                       = &Gp_HpMpWork;
    displayedVitals->hp                   = player->hp;
    displayedVitals->mp                   = player->mp;
    hud->radarRangeIcon                   = HUD_RADAR_RANGE_NONE;
    hud->radarRange                       = 0;
    attachment                            = &Gp_StateC08;
    attachment->antibodyTicks             = 0;
    attachment->antibodyCombo             = 0;
    attachment->energyShotTicks           = 0;
    attachment->energyShotCombo           = 0;
    attachment->queuedIndex               = 0;
    attachment->metabolismTicks           = 0;
    attachment->metabolismCombo           = 0;
    attachment->mindWard                  = 0;
    attachment->bodyWard                  = 0;
    attachment->mode                      = ATTACHMENT_MODE_IDLE;
    gGameSession->battleResetPending      = 0;
    Gp_ItemGrantCooldown                  = 0;
    gDisplayState.suppressDisconnectPause = 1;
    attachment->flags                    &= ~ATTACHMENT_FLAG_SWAP_LOCK;
}

/// Starts deterministic demo input replay from the loaded save-and-input resource.
///
/// Resets both random sequences and display timing counters. The stream follows
/// the serialized save, live player record and flag banks; its halfwords are
/// button/duration pairs. Requires the selected buffer to remain loaded and
/// halfword-aligned throughout replay. The fixed replay uses extended RAM.
static void _gameDebugStartInputReplay(void)
{
    enum {
        GAME_DEBUG_REPLAY_RANDOM_SEED         = 1,
        GAME_DEBUG_REPLAY_BUTTONS_INVALID     = 0xFFFF,
        GAME_DEBUG_REPLAY_STREAM_OFFSET_BYTES = sizeof(McSaveData) + PLAYER_STATUS_SAVE_RECORD_BYTES +
                                                GAME_FLAG_ACROPOLIS_BANK_BYTES + GAME_FLAG_DRYFIELD_BANK_BYTES + GAME_FLAG_DRYFIELD_NIGHT_BANK_BYTES +
                                                GAME_FLAG_MINE_SHELTER_BANK_BYTES + GAME_FLAG_NEO_ARK_BANK_BYTES + sizeof(GameFlagNibbleBank)
    };
    DisplayState* display;

    srand(GAME_DEBUG_REPLAY_RANDOM_SEED);
    gRandomLcgState          = 0;
    display                  = &gDisplayState;
    display->animFrame       = 0;
    gDisplayState.frameCount = 0;
    display->gameTick        = 0;
    display->loopCount       = 0;
    display->vsyncCount      = 0;
    display->loopTicks       = 0;

    // The resource prefix is serialized bytes; only the input stream is u16 data.
    if (display->demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
        Gp_ReplayCursor = (u16*)(FILE_SYSTEM_FIXED_REPLAY_BASE + GAME_DEBUG_REPLAY_STREAM_OFFSET_BYTES);
    } else {
        Gp_ReplayCursor = (u16*)((u8*)Fs_ActorLoadBase2 + GAME_DEBUG_REPLAY_STREAM_OFFSET_BYTES);
    }
    // Force the first record to install its duration before the countdown steps.
    Gp_ReplayButtons                  = GAME_DEBUG_REPLAY_BUTTONS_INVALID;
    Gp_ReplayFramesLeft               = 1;
    Pad_RemapState->inputOverrideMode = GAME_DEBUG_INPUT_OVERRIDE_REPLAY;
}

void playClockStartDeathFade(Task* task)
{
    GameSession* session;

    task->killCountdown--;
    if (task->killCountdown <= 0) {
        task->killCountdown = 0;
        // The expired delay becomes the completion latch used by later phases.
        playClockAdvanceDeathSound(&task->killCountdown);
        session                 = gGameSession;
        Gp_StateC08.effectPhase = ATTACHMENT_EFFECT_IDLE;
        if (session->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
            _playClockQueueDeathFade(session);
        }
        task->spawnArg1.value = 0;
        task->state++;
    }
}

void playClockWaitDeathFade(Task* task)
{
    enum { PLAY_CLOCK_DEATH_PRESENTATION_UPDATES = 64 };
    playClockAdvanceDeathSound(&task->killCountdown);
    task->spawnArg1.value++;
    if (task->spawnArg1.value == PLAY_CLOCK_DEATH_PRESENTATION_UPDATES) {
        if (gGameSession->restartMode == GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
            gDisplayState.skipDraw = 1;
        }
        task->spawnArg1.value = 0;
        task->state++;
    }
}

void playClockTask(Task* task)
{
    TaskFuncTable6 states;

    states = Gp_PlayClockStates;
    states.funcs[task->state](task);
}

void attachmentPreviewProjectile(s32 release, s32 radius, s32 extent)
{
    if (release == 0) {
        attachmentDrawAreaWireframe(0, radius, extent, ATTACHMENT_AREA_WIREFRAME_CYLINDER | ATTACHMENT_AREA_WIREFRAME_PROJECTILE);
    }
}

void hudDrawLockedTargetHp(HudTargetHpReadout* readout)
{
    WorldTargetNode* lockedTarget;
    Task*            playerTask;
    GameActor*       playerActor;
    WorldTargetNode* targetNode;

    playerTask   = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    lockedTarget = NULL;
    if (playerTask != NULL) {
        playerActor = playerTask->work;
        if (playerActor != NULL) {
            lockedTarget = playerActor->targetNode;
        }
        targetNode = gWorldTargetListHead;
        if (targetNode != NULL) {
            do {
                if (targetNode == lockedTarget) {
                    if (!(targetNode->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
                        _hudDrawTargetHpReadout(GP_NODE_ENEMY(targetNode), readout);
                        return;
                    }
                }
                targetNode = targetNode->next;
            } while (targetNode != NULL);
        }
    }
}

/// Returns whether the scene's post-battle hold has no frames remaining.
static s32 _sceneIsBattleEndDelayClear(void)
{
    return gSceneCombatState.signals.bytes.endDelayFrames == 0;
}

void attachmentEnqueueHealingSoundLoad(void)
{
    sndLoadEnqueuePeFile(attachmentGetEffectiveLevel(ATTACHMENT_INDEX_HEALING) + ATTACHMENT_INDEX_HEALING * ATTACHMENT_AREA_LEVEL_COUNT);
}

void itemPickupNoticeTask(Task* task)
{
    enum { ITEM_PICKUP_NOTICE_BONUS   = 2,
           ITEM_PICKUP_NOTICE_INITIAL = 0 };
    UiObject* panelObject;

    panelObject = task->spawnArg2.pointer;
    if (task->spawnArg1.value == ITEM_PICKUP_NOTICE_BONUS) {
        if (task->state == ITEM_PICKUP_NOTICE_INITIAL) {
            // The bonus label needs a wider panel and a shifted placement once.
            uiSetPanelContentSize(&panelObject->panel, textMeasureLineWidth(Gp_StrBonusItem) + 0xA, 0);
            panelObject->panel.bounds.unsignedRect.x -= 0xF;
            panelObject->panel.bounds.unsignedRect.y += 9;
            task->state++;
        }
        textDrawUiLine(panelObject, panelObject->panel.contentLeft.signedValue + 6, 7, Gp_StrBonusItem, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    } else {
        textDrawUiLine(panelObject, panelObject->panel.contentLeft.signedValue + 6, 7, Gp_StrItemObtained, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
}

void itemPickupTitleTask(Task* task)
{
    UiObject* panelObject;

    panelObject         = task->spawnArg2.pointer;
    panelObject->result = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&panelObject->panel, Gp_StrItem);
    if (panelObject->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            panelObject->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void sceneQueueBattleEscapeResult(void)
{
    enum { SCENE_BATTLE_RESULT_ESCAPE = 1 };
    u8 battlePhase;

    battlePhase = gSceneCombatState.signals.bytes.battlePhase;
    if ((battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) || (battlePhase == SCENE_COMBAT_BATTLE_RESUMED)) {
        if (gGameSession->battleResetPending == 0) {
            playerStateSetStatusEffects(1, PLAYER_STATUS_ALL_EFFECTS);
            roomEffectRequestCancelPe();
            gDisplayState.suppressDisconnectPause = 0;
            displayQueueModeTask(&D_8010CABC, SCENE_BATTLE_RESULT_ESCAPE, 0, STAGE_ENTRY_RELOAD_FORCED);
        }
    }
}

/// Packs an ability index and level into its Parasite Energy text identifier.
///
/// Ability indices 0..17 occupy groups of three in bits 4 and above, with
/// the within-group index in bits 2..3. Level 0..3 occupies bits 0..1;
/// text lookup treats level 0 as level 1. Inputs are not checked or masked.
static s32 _attachmentMakeTextId(s32 abilityIndex, s32 level)
{
    enum { ATTACHMENT_TEXT_ID_BASE = 0x300 };

    return (abilityIndex / 3) * 16 + (abilityIndex % 3) * 4 + level + ATTACHMENT_TEXT_ID_BASE;
}

s32 attachmentGetEffectiveLevel(s32 abilityIndex)
{
    enum { ATTACHMENT_MIN_EFFECTIVE_LEVEL = 1 };
    PlayerStatus* player;
    s32           training;
    s32           level;
    const u8*     learnedLevels;

    level = ATTACHMENT_MIN_EFFECTIVE_LEVEL;
    if (abilityIndex < ATTACHMENT_SPELL_COUNT) {
        player   = &gPlayerStatus;
        training = _attachmentUsesTrainingLevels(player);
        if (training == 0) {
            learnedLevels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
        } else {
            learnedLevels = Gp_DebugAttachLevels;
        }
        level = learnedLevels[abilityIndex];
        if (level == 0) {
            level = ATTACHMENT_MIN_EFFECTIVE_LEVEL;
        }
        if (player->statusFlags & PLAYER_STATUS_BERSERKER) {
            if (level < ATTACHMENT_AREA_LEVEL_COUNT) {
                level++;
            }
        }
    }
    return level;
}

/// Moves through the twelve wheel spells, skipping unlearned entries unless cheats are enabled.
///
/// `abilityIndex` is 0..11; signed `steps` counts eligible positions forward or
/// backward, wrapping within the wheel. Uses training levels in the shooting
/// gallery. A nonzero step requires at least one learned spell or cheat mode;
/// otherwise the search does not terminate. Zero steps retains the input index.
static s32 _attachmentStepLearnedSpell(s32 abilityIndex, s32 steps)
{
    PlayerStatus* player;
    McSaveData*   liveSave;
    s32           training;
    const u8*     levels;

    player   = &gPlayerStatus;
    training = _attachmentUsesTrainingLevels(player);
    if (training == 0) {
        levels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    } else {
        levels = Gp_DebugAttachLevels;
    }
    if (steps != 0) {
        liveSave = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        do {
            if (steps > 0) {
                do {
                    abilityIndex++;
                    if (abilityIndex >= ATTACHMENT_SPELL_COUNT) {
                        abilityIndex = 0;
                    }
                } while (levels[abilityIndex] == 0 && liveSave->state.cheatMode == 0);
                steps--;
            } else {
                do {
                    abilityIndex--;
                    if (abilityIndex < 0) {
                        abilityIndex += ATTACHMENT_SPELL_COUNT;
                    }
                } while (levels[abilityIndex] == 0 && liveSave->state.cheatMode == 0);
                steps++;
            }
        } while (steps != 0);
    }
    return abilityIndex;
}

s32 attachmentSoundLoadStub(s32 unusedFileIndex)
{
    SceneCombatState* combat;
    s32               battleActive;

    combat       = &gSceneCombatState;
    battleActive = _sceneHasBattleHoldOrEndDelay(combat);
    // Both outcomes return zero; the binary still evaluates the battle gate.
    if (battleActive) {
        return 0;
    }
    return 0;
}

/// Queues a PE sound file only during an engaged battle hold or the battle-end delay.
///
/// `fileIndex` selects category-5 stage-zero files 0..63. Reuses an already
/// requested file; otherwise the caller must leave room in the CD request ring.
static void _attachmentEnqueueBattleSoundLoad(u8 fileIndex)
{
    SceneCombatState* combat;
    s32               battleActive;

    combat       = &gSceneCombatState;
    battleActive = _sceneHasBattleHoldOrEndDelay(combat);
    if (battleActive) {
        sndLoadEnqueuePeFile(fileIndex);
    }
}

/// Allows attachment use once battle sound loading has finished, or immediately outside battle.
///
/// Engaged battle holds and the battle-end delay require normal CD dispatch
/// with an empty request ring. Returns 0 or 1; no request or state is changed.
static s32 _attachmentIsBattleSoundLoadReady(void)
{
    SceneCombatState* combat;
    s32               battleActive;

    combat       = &gSceneCombatState;
    battleActive = _sceneHasBattleHoldOrEndDelay(combat);
    if (battleActive) {
        return (u16)cdCmdIsIdle();
    }
    return 1;
}

void attachmentQueueIndex(s32 abilityIndex)
{
    if (!(Gp_StateC08.flags & ATTACHMENT_FLAG_EVENT_LOCK)) {
        Gp_StateC08.queuedIndex = abilityIndex;
    }
}

void attachmentCancel(void)
{
    enum { ATTACHMENT_NO_QUEUED_INDEX  = 0,
           ATTACHMENT_NO_PREVIEW_SOUND = 0 };
    AttachmentState* attachment;

    // Return CD positioning and actor updates to ordinary play before the next HUD tick.
    cdCmdEnqueueDisplayResource(0, 0, CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW);
    attachment = &Gp_StateC08;
    if (attachment->mode >= ATTACHMENT_MODE_ARMED) {
        attachment->effectPhase = ATTACHMENT_EFFECT_CANCELLED;
    }
    attachment->queuedIndex        = ATTACHMENT_NO_QUEUED_INDEX;
    attachment->mode               = ATTACHMENT_MODE_IDLE;
    D_80115768                     = 0;
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
    attachment->previewSound       = ATTACHMENT_NO_PREVIEW_SOUND;
    attachment->soundStep          = ATTACHMENT_SOUND_IDLE;
}

void hudDelayInputAfterMenu(void)
{
    enum { HUD_MENU_EXIT_INPUT_DELAY_UPDATES = 5 };

    Gp_ItemGrantCooldown = HUD_MENU_EXIT_INPUT_DELAY_UPDATES;
}

/// Tests whether HUD category switching is allowed by the player and attachment gates.
///
/// Requires a live player task's work when the slot is occupied. Normal or aimed
/// locomotion is eligible, including movement; direction actions and interaction
/// presses block it. The HUD input delay and battle-end delay must have expired.
/// Nonzero `ignoreSwapLock` bypasses only the attachment swap lock. Returns 0 or 1.
static s32 _hudCanSwitchCategory(s32 ignoreSwapLock)
{
    enum { HUD_SWITCH_PLAYER_LOCOMOTION_STATE     = 0,
           HUD_SWITCH_PLAYER_AIM_LOCOMOTION_STATE = 2 };
    Task*         playerTask;
    GameActor*    actor;
    PlayerStatus* player;
    s32           eligible;

    eligible   = 0;
    playerTask = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (playerTask != NULL) {
        actor  = playerTask->work;
        player = &gPlayerStatus;
        if (actor->mode == GAME_ACTOR_MODE_NORMAL) {
            if (actor->state == HUD_SWITCH_PLAYER_LOCOMOTION_STATE || actor->state == HUD_SWITCH_PLAYER_AIM_LOCOMOTION_STATE) {
                if (gGameSession->dirActionBusy == 0) {
                    if (player->interactionPressed == 0) {
                        eligible = 1;
                    }
                }
            }
        }
    }
    if (ignoreSwapLock == 0) {
        if (Gp_StateC08.flags & ATTACHMENT_FLAG_SWAP_LOCK) {
            eligible = 0;
        }
    }
    if (eligible != 0) {
        if (Gp_ItemGrantCooldown <= 0) {
            if (gSceneCombatState.signals.bytes.endDelayFrames == 0) {
                return 1;
            }
        }
    }
    return 0;
}

void viewChangeStub(void)
{
}

/// Subtracts 16 from a signed value; its domain and purpose are unproven.
static s32 func_800A7F2C(s32 value)
{
    return value - 0x10;
}

s32 playerStateSpendMp(s32 amount)
{
    PlayerStatus* player;
    s32           fullyPaid;

    player    = &gPlayerStatus;
    fullyPaid = 1;
    if (player->mp >= amount) {
        player->mp -= amount;
    } else {
        player->mp = 0;
        fullyPaid  = 0;
    }
    return fullyPaid;
}

/// Writes a coordinate's transform relative to another composed coordinate.
///
/// Refreshes `coord` then `reference` through their live, acyclic parent chains.
/// Their caches must compose into the same frame. Writes only `outCoord->coord`
/// rotation and translation: transpose(reference.workm.m) times coord.workm.m
/// and the origin difference. The transpose inverts an orthonormal rotation;
/// coefficients use ONE (4096), translations signed game-coordinate units.
/// Other output fields and matrix alignment bytes remain untouched. The output
/// local matrix must be disjoint from both input cached matrices and scratch;
/// `outCoord` may be either input node. The caller invalidates its composition
/// stamp when needed. Requires one 48-byte scratch block, released before return;
/// changes GTE rotation and arithmetic state and retains no pointers.
static __inline__ void _gfxCoordToReference(GfxCoord* coord, GfxCoord* reference, GfxCoord* outCoord)
{
    _GfxRelativeTransformScratch* scratch;
    MATRIX*                       referenceMatrix;
    MATRIX*                       coordMatrix;
    MATRIX*                       outMatrix;

    actorRenderComposeCoord(coord);
    actorRenderComposeCoord(reference);

    referenceMatrix = &reference->workm;
    coordMatrix     = &coord->workm;
    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_GfxRelativeTransformScratch);
    outMatrix       = &outCoord->coord;

    gte_TransposeMatrix(referenceMatrix, &scratch->transposedRotation);

    gte_MulMatrix0(&scratch->transposedRotation, coordMatrix, outMatrix);

    _gfxWriteRelativeTranslation(referenceMatrix, coordMatrix, outMatrix, scratch);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxRelativeTransformScratch);
}

/// Sets the active view from a camera coordinate and an optional outer offset.
///
/// The view applies negated origin before transposed rotation, in signed game
/// units and ONE-scaled coefficients. A direct child of `gGfxViewCoord` uses
/// its local transform; other coordinates are composed relative to that node.
/// The latter path requires a live acyclic chain and 48 free scratch bytes.
/// `offset` supplies XYZ after rotation, or NULL clears it. Invalidates the
/// source and all three view caches; projection settings and parents survive.
/// Input storage must be disjoint from the active view nodes and scratch stack.
static void _viewSetFromCoord(GfxCoord* cameraCoord, const VECTOR* offset)
{
    GfxCoord* viewOrigin;
    GfxCoord* cameraParent;
    GfxCoord  relative;

    if (offset != NULL) {
        Gfx_ViewOffsetCoord.coord.t[0] = offset->vx;
        Gfx_ViewOffsetCoord.coord.t[1] = offset->vy;
        Gfx_ViewOffsetCoord.coord.t[2] = offset->vz;
    } else {
        Gfx_ViewOffsetCoord.coord.t[0] = 0;
        Gfx_ViewOffsetCoord.coord.t[1] = 0;
        Gfx_ViewOffsetCoord.coord.t[2] = 0;
    }

    // A direct child already expresses its camera pose in the required frame.
    cameraParent = cameraCoord->parent;
    viewOrigin   = &gGfxViewCoord;
    if (cameraParent == viewOrigin) {
        gte_TransposeMatrix(&cameraCoord->coord, &gGfxViewRotCoord.coord);
        viewOrigin->coord.t[0] = -cameraCoord->coord.t[0];
        viewOrigin->coord.t[1] = -cameraCoord->coord.t[1];
        viewOrigin->coord.t[2] = -cameraCoord->coord.t[2];
    } else {
        _gfxCoordToReference(cameraCoord, viewOrigin, &relative);
        gte_TransposeMatrix(&relative.coord, &gGfxViewRotCoord.coord);
        viewOrigin->coord.t[0] = -relative.coord.t[0];
        viewOrigin->coord.t[1] = -relative.coord.t[1];
        viewOrigin->coord.t[2] = -relative.coord.t[2];
    }
    cameraCoord->composeStamp = GRAPHICS_COORD_DIRTY;

    Gfx_ViewOffsetCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    gGfxViewRotCoord.composeStamp    = GRAPHICS_COORD_DIRTY;
    gGfxViewCoord.composeStamp       = GRAPHICS_COORD_DIRTY;
}

/// Queues a camera coordinate pose and optional outer offset for one view update.
///
/// Bank-0 slot 0x0E owns a coordinate body and a separately allocated VECTOR.
/// Copies offset XYZ in game units (NULL means zero); retains neither input.
/// Transposes the pose rotation and negates its origin for the view chain,
/// expressing non-direct children relative to `gGfxViewCoord` first. Rotations
/// use ONE-scaled coefficients; inversion requires an orthonormal camera frame.
/// Non-direct children require live acyclic chains and 48 free scratch bytes.
/// Returns 1 on success; allocation failure returns 0 and releases any task.
/// The task applies the snapshot without changing projection, then frees it.
static s32 _viewQueueCoord(GfxCoord* cameraCoord, const VECTOR* offset)
{
    /// Writes the pose components used by the translation-before-rotation view chain.
    ///
    /// Requires disjoint word-aligned matrices. Transposes ONE-scaled rotation
    /// and negates origin XYZ, preserving alignment bytes. An orthonormal pose
    /// gives the inverse view. Arguments are evaluated repeatedly and must have
    /// no side effects; captures no locals and does not invalidate either cache.
#define VIEW_WRITE_INVERSE_POSE(cameraPose, viewPose)  \
    {                                                  \
        gte_TransposeMatrix((cameraPose), (viewPose)); \
        (viewPose)->t[0] = -(cameraPose)->t[0];        \
        (viewPose)->t[1] = -(cameraPose)->t[1];        \
        (viewPose)->t[2] = -(cameraPose)->t[2];        \
    }
    enum { VIEW_COORD_TASK_BANK = 0,
           VIEW_COORD_TASK_TYPE = 0xE };
    GfxCoord* inverseCamera;
    Task*     task;
    VECTOR*   ownedOffset;
    GfxCoord* viewOrigin;
    GfxCoord* cameraParent;
    GfxCoord  relative;

    task = taskSpawn(VIEW_COORD_TASK_BANK, VIEW_COORD_TASK_TYPE, 0, 0);
    if (task == NULL) {
        return 0;
    }
    ownedOffset = memCalloc(sizeof(*ownedOffset), 0);
    if (ownedOffset == NULL) {
        taskKill(task);
        return 0;
    }
    task->work    = ownedOffset;
    inverseCamera = task->extra.coordBody->coord;
    if (offset != NULL) {
        ownedOffset->vx = offset->vx;
        ownedOffset->vy = offset->vy;
        ownedOffset->vz = offset->vz;
    } else {
        ownedOffset->vx = 0;
        ownedOffset->vy = 0;
        ownedOffset->vz = 0;
    }

    // A direct child supplies its pose without touching its composition cache.
    cameraParent = cameraCoord->parent;
    viewOrigin   = &gGfxViewCoord;
    if (cameraParent == viewOrigin) {
        VIEW_WRITE_INVERSE_POSE(&cameraCoord->coord, &inverseCamera->coord);
    } else {
        _gfxCoordToReference(cameraCoord, viewOrigin, &relative);
        VIEW_WRITE_INVERSE_POSE(&relative.coord, &inverseCamera->coord);
    }
    return 1;
#undef VIEW_WRITE_INVERSE_POSE
}

void viewApplyCoordTask(Task* task)
{
    const VECTOR*         offset;
    const GfxCoord*       cameraCoord;
    GfxCoord*             viewOffset;
    GfxCoord*             viewOrigin;
    ModelObjectCoordBody* body;
    s32                   row;
    s32                   column;

    row                    = 0;
    viewOffset             = &Gfx_ViewOffsetCoord;
    body                   = task->extra.coordBody;
    offset                 = task->work;
    cameraCoord            = body->coord;
    viewOffset->coord.t[0] = offset->vx;
    viewOffset->coord.t[1] = offset->vy;
    viewOffset->coord.t[2] = offset->vz;

    // Transfer only the nine coefficients; the origin belongs to its own node.
    for (; row < (s32)ARRAY_SIZE(gGfxViewRotCoord.coord.m); row++) {
        for (column = 0; column < (s32)ARRAY_SIZE(gGfxViewRotCoord.coord.m[row]); column++) {
            gGfxViewRotCoord.coord.m[row][column] = cameraCoord->coord.m[row][column];
        }
    }

    viewOrigin             = &gGfxViewCoord;
    viewOrigin->coord.t[0] = cameraCoord->coord.t[0];
    viewOrigin->coord.t[1] = cameraCoord->coord.t[1];
    viewOrigin->coord.t[2] = cameraCoord->coord.t[2];

    Gfx_ViewOffsetCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    gGfxViewRotCoord.composeStamp    = GRAPHICS_COORD_DIRTY;
    gGfxViewCoord.composeStamp       = GRAPHICS_COORD_DIRTY;
    taskKill(task);
}

void viewApplyCurrentCamera(void)
{
    GameLocationKey* location;
    ViewCameraTable* cameraTable;
    ViewCamera*      cameras;
    GfxCoord*        viewOffset;
    MATRIX*          viewRotation;
    VECTOR3*         viewTranslation;
    u8               cameraIndex;

    location    = &gGameSession->location.loc;
    cameraTable = Gp_ViewTables[location->stage - 1];
    cameras     = cameraTable->cameras[location->area - 1];
    cameraIndex = viewGetMappedIndex();

    viewRotation    = &gGfxViewRotCoord.coord;
    viewTranslation = MATRIX_TRANS(&gGfxViewCoord.coord);
    viewOffset      = &Gfx_ViewOffsetCoord;

    _viewApplyCameraCursor(&cameras[cameraIndex], viewRotation, viewTranslation, viewOffset);
}

void gfxMakeRelativeTransform(const MATRIX* reference, const MATRIX* target, MATRIX* out)
{
    _GfxRelativeTransformScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxRelativeTransformScratch);

    // Save the reference rotation before writing a possibly aliased output.
    gte_TransposeMatrix(reference, &scratch->transposedRotation);
    gte_MulMatrix0(&scratch->transposedRotation, target, out);
    _gfxWriteRelativeTranslation(reference, target, out, scratch);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxRelativeTransformScratch);
}

s32 viewQueueCamera(const ViewCamera* camera)
{
    return taskSpawn(VIEW_CAMERA_TASK_BANK, VIEW_CAMERA_TASK_TYPE, 0, camera) != NULL;
}

void viewApplyCamera(const ViewCamera* camera)
{
    _viewWriteCameraState(camera);
}

/// Restores identity view rotation and zero origin with outer Z at ONE (4096 game units).
///
/// Invalidates the three caches while preserving their parent links and all
/// projection settings.
static void _viewResetTransform(void)
{
    MATRIX*   rotation;
    GfxCoord* viewOffset;
    GfxCoord* viewRotation;
    GfxCoord* viewOrigin;
    s32       one;

    viewOffset             = &Gfx_ViewOffsetCoord;
    one                    = ONE;
    viewOffset->coord.t[0] = 0;
    viewOffset->coord.t[1] = 0;
    viewOffset->coord.t[2] = one;

    rotation     = &gGfxViewRotCoord.coord;
    viewRotation = PARENT_OF(rotation, GfxCoord, coord);
    gfxSetRotIdentity(rotation);

    viewOrigin                 = &gGfxViewCoord;
    viewOrigin->coord.t[0]     = 0;
    viewOrigin->coord.t[1]     = 0;
    viewOrigin->coord.t[2]     = 0;
    viewOffset->composeStamp   = GRAPHICS_COORD_DIRTY;
    viewRotation->composeStamp = GRAPHICS_COORD_DIRTY;
    viewOrigin->composeStamp   = GRAPHICS_COORD_DIRTY;
}

void viewQueueCurrentCameraAndPackets(void)
{
    const GameLocationKey* location;
    ViewCameraTable*       cameraTable;
    const ViewCamera*      cameras;
    u8                     cameraIndex;

    location    = &gGameSession->location.loc;
    cameraTable = Gp_ViewTables[location->stage - 1];
    cameras     = cameraTable->cameras[location->area - 1];
    cameraIndex = viewGetMappedIndex();
    _viewQueueCameraCursorAndPackets(&cameras[cameraIndex], VIEW_PACKET_LIST_SELECTED);
}

ViewCamera* viewGetMappedCamera(const GameLocationKey* location)
{
    ViewCameraTable* cameraTable;
    ViewCamera*      cameras;
    u8               cameraIndex;

    cameraTable = Gp_ViewTables[location->stage - 1];
    cameras     = cameraTable->cameras[location->area - 1];
    cameraIndex = viewGetMappedIndex();
    return &cameras[cameraIndex - 1];
}

void viewApplyCameraTask(Task* task)
{
    const ViewCamera* camera;

    camera = task->spawnArg2.pointer;
    _viewWriteCameraState(camera);
    taskKill(task);
}

/// Queues identity view rotation, zero origin and an outer Z offset of ONE game units.
///
/// Keeps projection settings and discards the queue operation's success status.
static void _viewQueueDefaultTransform(void)
{
    VECTOR   offset;
    GfxCoord cameraCoord;

    offset.vx          = 0;
    offset.vy          = 0;
    offset.vz          = ONE;
    cameraCoord.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&cameraCoord.coord);
    cameraCoord.coord.t[0] = 0;
    cameraCoord.coord.t[1] = 0;
    cameraCoord.coord.t[2] = 0;
    _viewQueueCoord(&cameraCoord, &offset);
}

void viewQueueCurrentCamera(s32 packetListMode)
{
    const GameLocationKey* location;
    ViewCameraTable*       cameraTable;
    const ViewCamera*      cameras;
    u8                     cameraIndex;

    location    = &gGameSession->location.loc;
    cameraTable = Gp_ViewTables[location->stage - 1];
    cameras     = cameraTable->cameras[location->area - 1];
    cameraIndex = viewGetMappedIndex();
    _viewQueueCameraCursorAndPackets(&cameras[cameraIndex], packetListMode);
}

void viewTransitionGateTask(Task* task)
{
    enum {
        VIEW_TRANSITION_INITIAL          = 0,
        VIEW_TRANSITION_ACQUIRE_HOLD     = 1,
        VIEW_TRANSITION_WAIT_SETTLE      = 2,
        VIEW_TRANSITION_MONITOR          = 3,
        VIEW_TRANSITION_SETTLE_UPDATES   = 2,
        VIEW_TRANSITION_LOADER_TASK_BANK = 0,
        VIEW_TRANSITION_LOADER_TASK_TYPE = 0x1E
    };
    GameSession* session;
    McSaveData*  liveSave;
    CdCmdQueue*  cdQueue;
    s32          admittedView;

    gGameSession->viewReady = 0;
    if (task->state == VIEW_TRANSITION_INITIAL) {
        task->state = VIEW_TRANSITION_MONITOR;
    }
    liveSave = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    if (task->spawnArg1.value != liveSave->state.location.loc.view) {
        gGameSession->viewDirty = 1;
    }
    session = gGameSession;
    if (session->viewDirty != 0) {
        cdQueue = &gCdCmdQueue;
        // Keep the current view while its reusable scene payload is incomplete.
        if ((cdQueue->scenePayloadAvailable == 0) || (cdQueue->scenePayloadLoading == 0)) {
            session->location.loc.view = liveSave->state.location.loc.view;
            padStartInputBlock(0);
            viewQueueCurrentCameraAndPackets();
            if (displaySpawnTask(VIEW_TRANSITION_LOADER_TASK_BANK, VIEW_TRANSITION_LOADER_TASK_TYPE, 0, 0) != NULL) {
                admittedView          = gGameSession->location.loc.view;
                task->killCountdown   = VIEW_TRANSITION_SETTLE_UPDATES;
                task->spawnArg1.value = admittedView;
                if (task->state == VIEW_TRANSITION_MONITOR) {
                    task->state = VIEW_TRANSITION_ACQUIRE_HOLD;
                }
            }
        }
    }
    // These phases may run in the same update that admits the loader.
    if (task->state == VIEW_TRANSITION_ACQUIRE_HOLD) {
        displayAcquireMenuHold();
        task->state += 1;
    }
    if (task->state == VIEW_TRANSITION_WAIT_SETTLE) {
        task->killCountdown--;
        if (task->killCountdown == 0) {
            displayReleaseMenuHold();
            gGameSession->viewReady = 1;
            task->state             = VIEW_TRANSITION_MONITOR;
        }
    }
}
