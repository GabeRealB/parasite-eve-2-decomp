#include "gameplay/scene_runtime.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libcd.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "scene_runtime.h"
#include "gameplay/world_targets.h"
#include "world_targets.h"

#include "main/cdaudio.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/random.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/tmd.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

#include "rooms/dryfield_motel_room_6.h"

#include "rooms/dryfield_night_motel_room_6.h"

#include "rooms/mist_shooting_gallery.h"

#include "rooms/neo_ark_bridge.h"

#include "rooms/neo_ark_garden.h"

#include "rooms/neo_ark_island.h"

#include "rooms/neo_ark_observatory.h"

#include "rooms/neo_ark_pavilion.h"

#include "rooms/neo_ark_r31.h"

#include "rooms/neo_ark_submarine_gallery.h"

#include "rooms/neo_ark_submarine_tunnel.h"

#include "rooms/neo_ark_woodland_path.h"

#include "rooms/shelter_b1_control_room.h"

#include "rooms/shelter_r48.h"

/// Low flags nibble selecting a model-part track's pose bank and encoding.
///
/// Slot initialization copies these bits from the first keyframe into
/// `AnimationSlot.poseEncoding`; subsequent records retain that selector.
/// Encoding 1 supplies translation and rotation, and encoding 4 supplies
/// packed rotation only. Cue and control bits are excluded.
///
/// Masking preserves values 0..15 without validating them. Playback indexes
/// `AnimationSet.poseBanks` before decoder dispatch, so the selected value
/// must be below `ANIMATION_POSE_BANK_COUNT` even for unsupported encodings.
enum { ANIMATION_RECORD_POSE_ENCODING_MASK = 0x0F };

/// Root-to-head chain endpoint used by the model head-aim routines.
enum { ANIMATION_HEAD_PART_INDEX = 4 };

/// Lowest unsigned record-flags value that ends a forward animation-track walk.
///
/// Values 0xC0..0xFF have both control and stop bits set; the low six bits do
/// not affect termination. Values 0x80..0xBF instead jump to `wordOffset`.
/// An end record keeps the slot's previously selected next record index and
/// ignores its own `wordOffset` and `durationFrames`. This is a threshold,
/// so comparison with `AnimationRecord.flags` must retain its unsigned value.
enum { ANIMATION_RECORD_END_THRESHOLD = ANIMATION_RECORD_CONTROL | ANIMATION_RECORD_STOP };

enum {
    /// Fractional bits of animation playback time, measured in normal-rate frames.
    ///
    /// Shifting a whole-frame keyframe or transition duration left by this
    /// count converts it to the sixteenth-frame units shared by
    /// `AnimationSlot.timeSpan`, signed `AnimationSlot.timeLeft` and signed
    /// per-tick `AnimationSlot.rate`. Normal playback consumes
    /// `ANIMATION_RATE_ONE` units per tick; negative rates walk backwards.
    /// An 8-bit record duration expands to at most 4080 units. Explicit
    /// transition durations must fit the signed remaining time after scaling
    /// (0..2047 whole frames). Interpolation weights use the separate
    /// `ANIMATION_BLEND_FRACTION_BITS` scale.
    ANIMATION_TIME_FRACTION_BITS = 4
};
STATIC_ASSERT((1 << ANIMATION_TIME_FRACTION_BITS) == ANIMATION_RATE_ONE, animation_time_fraction_matches_rate_one);

/// Track encoding that supplies both local translation and Euler rotation.
///
/// The initial keyframe's low flags nibble supplies this selector, which also
/// indexes the slot's pose bank. `AnimationPackedPose` stores six signed
/// halfwords: X/Y/Z translation in model integer units, then X/Y/Z angles in
/// 1/4096 turns. A record's word offset must leave all three four-byte words
/// within the live, word-aligned bank. Buffered endpoints store the same
/// 12-byte format at the start of a slot's 16-byte entry. Applying an unpacked
/// pose writes translation only for this encoding.
enum { ANIMATION_POSE_TRANSLATION_ROTATION = 1 };

/// Track encoding that supplies packed Euler rotation without translation.
///
/// The initial keyframe's low flags nibble selects bank 4. Each
/// `AnimationRecord.wordOffset` addresses one complete, word-aligned
/// `AnimationPackedRotation`: signed X/Y/Z angles in 11/10/11 bits, with
/// eight 1/4096-turn angle units per stored step. The loaded bank must cover
/// that entire four-byte word. Buffered endpoints and optional encoded outputs
/// use the same format; a buffered pose occupies the start of its slot's
/// 16-byte entry. Applying this encoding preserves the model coordinate's or
/// unpacked destination's local translation.
enum { ANIMATION_POSE_PACKED_ROTATION = 4 };

/// Packed saved placement key: high nibble placement, next nibble stage, low byte area.
enum {
    AREA_PLACEMENT_INDEX_SHIFT     = 12,
    AREA_PLACEMENT_STAGE_SHIFT     = 8,
    AREA_PLACEMENT_STAGE_AREA_MASK = 0xFFF
};

/// Empty saved-pose slot, fallback resume state and discarded Euler-angle bits.
enum {
    AREA_SAVED_ENEMY_POSE_FREE          = 0,
    AREA_SAVED_ENEMY_POSE_DEFAULT_STATE = 1,
    AREA_SAVED_ENEMY_POSE_ANGLE_SHIFT   = 8
};

/// Borrowed endpoints and output destinations for one model-part pose blend.
///
/// `AnimationSlot.poseEncoding` selects the pointer views: encoding 1 uses
/// `translationRotation` (12 bytes), and encoding 4 uses `packedRotation`
/// (4 bytes). `bytes` resolves word offsets in a bank or addresses a slot's
/// 16-byte buffer entry; `address` accepts the caller's optional encoded output.
/// All encoded addresses must be word-aligned and cover the complete pose.
/// Translation uses model integer units; decoded rotations use 1/4096 turns.
///
/// A null unpacked destination updates the model coordinate. A non-null one
/// receives the pose instead, preserving its translation for encoding 4.
/// The encoded destination is independent and may alias an endpoint: encoded
/// writes follow decoding both endpoints. A zero-duration segment writes neither
/// output. The request and its borrowed pointers remain live only for this call.
typedef struct {
    union {
        const u8*                      bytes;               // Word-aligned bank or buffered-pose address
        const AnimationPackedPose*     translationRotation; // Encoding 1: translation and full-resolution angles
        const AnimationPackedRotation* packedRotation;      // Encoding 4: signed 11/10/11-bit angles
    } currentPose, nextPose;                                // Segment's starting and destination poses, in the same encoding
    union {
        void*                    address;                   // Optional caller-supplied encoded output (NULL skips it)
        AnimationPackedPose*     translationRotation;       // Encoding 1: writable 12-byte pose
        AnimationPackedRotation* packedRotation;            // Encoding 4: writable 4-byte rotation
    } encodedDestination;                                   // Optional encoded output, independent of the unpacked output
    AnimationPose* unpackedDestination;                     // Optional 16-byte pose output (NULL updates the model coordinate)
    u8             refreshRotationDelta;                    // Relative rotation (0 reuse/unused, 1 rebuild on entering buffered endpoints)
} _AnimationBlendRequest;
STATIC_ASSERT_SIZEOF(_AnimationBlendRequest, 0x14);

/// Scratch-stack workspace for blending one model part's pose.
///
/// Both decoders reserve it uninitialized for that call and release it before
/// returning. Rotations are Euler angles in 4096 units per turn. The current
/// weight is the remaining segment time divided by the segment length, in
/// 1/4096 units, and the next weight is `ONE` minus that. Identical endpoint
/// addresses instead use 0 and `ONE`. A rotation-only blend does not read
/// `translation`. Matrices are used only when an endpoint is buffered, and
/// only their 3x3 rotations are read.
typedef struct {
    SVECTOR translation;     // Blended local translation, in model integer units
    SVECTOR currentRotation; // Current endpoint's Euler angles, in 1/4096 turns
    SVECTOR nextRotation;    // Next endpoint's angles, then the blended result, in 1/4096 turns
    MATRIX  currentMatrix;   // Current endpoint's rotation on the buffered path
    MATRIX  nextMatrix;      // Next endpoint's rotation, when refreshing the relative rotation
    MATRIX  deltaMatrix;     // Relative rotation, then the composed rotation if the coordinate stays unchanged
    s32     currentWeight;   // Current endpoint's weight, in 1/4096 units
    s32     nextWeight;      // Next endpoint's weight, `ONE` minus `currentWeight`
} _AnimationBlendScratch;
STATIC_ASSERT_SIZEOF(_AnimationBlendScratch, 0x80);

/// Scratch-stack reservation holding one slot tick's pose-blend request.
///
/// The word-aligned block is reserved uninitialized for the tick. All request
/// fields are populated before synchronous pose dispatch; the endpoints and
/// destinations are borrowed only until dispatch returns. Decoders reserve
/// their workspace below this live block and release it before the tick releases
/// this reservation. The leading bytes are untouched and have no proven role.
typedef struct {
    u8                     field_0[4]; // Uninterpreted leading storage; role unproven
    _AnimationBlendRequest request;    // Borrowed endpoints, outputs and buffered-rotation refresh request
} _AnimationTickScratch;
STATIC_ASSERT_SIZEOF(_AnimationTickScratch, 0x18);

/// Fractional bits of a keyframe interpolation weight.
///
/// For distinct endpoints, `(timeLeft << ANIMATION_BLEND_FRACTION_BITS) /
/// timeSpan` is the current endpoint's weight in the same 1/4096 units as
/// `ONE`. Division truncates toward zero, and the other endpoint's weight is
/// `ONE` minus that quotient, so the discarded fraction stays with the next
/// endpoint. `gte_gpf12` and `gte_gpl12` consume this scale. Identical
/// endpoints use weights 0 and `ONE` instead. This is not
/// `ANIMATION_TIME_FRACTION_BITS`, and it does not select
/// `ANIMATION_BLEND_RESET` or `ANIMATION_BLEND_INTERPOLATE`.
enum { ANIMATION_BLEND_FRACTION_BITS = 12 };
STATIC_ASSERT((1 << ANIMATION_BLEND_FRACTION_BITS) == ONE, animation_blend_fraction_matches_one);

/// One bit of a scene stream's sound-bank mask and the sound-bank type it selects.
///
/// A scene stream's `soundBankMask` names the script-sound bank types that
/// must stay silent while its CD audio plays. Before the audio starts, every
/// type whose bit is set has its running sounds stopped and its request gate
/// closed; when the scene ends or is cancelled the same gates reopen. Sounds
/// stopped at the start are not restarted.
///
/// `SOUND_BANK_TYPE_ALL_NON_AMBIENT` selects every type at once. Its stop
/// spares the ambient type, but its request gate covers all sixteen types.
/// A table of these rows ends at the first row whose `mask` is zero.
typedef struct {
    s32 mask;       // The `soundBankMask` bit that selects this row; zero ends the table
    s32 bankTypeId; // Sound request id with only its bank type set (bits 28..31), naming every bank of that type
} _CdCmdSceneSoundBankBit;
STATIC_ASSERT_SIZEOF(_CdCmdSceneSoundBankBit, 8);

/// One unpacked RGB555 colour on the scratch stack, in GTE short-vector form.
///
/// A blend reserves three and releases them together: the colour weighted by
/// the blend factor, the colour weighted by `ONE` minus that factor, then the
/// interpolated result. Each channel holds its 5-bit component in bits 7..11,
/// the scale `gte_gpf12` and `gte_gpl12` interpolate. `gte_ldsv` and
/// `gte_stsv` transfer only those three channels. Semi-transparency is not
/// stored; the packed result copies bit 15 from either source colour.
typedef struct {
    u16 r;   // Red, 5-bit component in bits 7..11
    u16 g;   // Green, 5-bit component in bits 7..11
    u16 b;   // Blue, 5-bit component in bits 7..11
    u16 pad; // Unused. Present so the record is an 8-byte short-vector slot
} _Rgb555Scratch;
STATIC_ASSERT_SIZEOF(_Rgb555Scratch, 8);

/// Scratch-stack workspace for turning one collision contact into an offset
/// expressed through the view coordinate's rotation.
///
/// `delta` is worked on in place through three stages: the contact point minus
/// the queried position, then that vector normalized, then the normalized
/// vector rotated by `transposedView`. The offset written to the caller is the
/// last stage scaled by a signed distance, so nothing in the block outlives
/// the call. Reserve the complete, word-aligned block and release it before
/// returning.
typedef struct {
    SVECTOR delta;          // Separation in world-coordinate units truncated to signed halfwords, then its direction with 4096 for one unit
    MATRIX  transposedView; // Transpose of `gGfxViewCoord.workm`; only its 3x3 rotation is loaded into the GTE
} _WorldCollisionContactViewOffsetScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionContactViewOffsetScratch, 0x28);

/// Scratch-stack workspace for projecting the ground shadow of one model.
///
/// The shadow is a square in the XZ plane of a model coordinate's own frame,
/// so `vertices` are local positions and the GTE, loaded with that
/// coordinate's composed matrix, projects them as they stand. Corners and
/// screen positions share indices 0..3 in GPU quad strip order.
///
/// Each corner takes its own RTPS, and `depth`, `depthCue` and
/// `projectionFlags` are overwritten by every one of them. Only the last
/// corner's FLAG word is therefore tested, and a negative one rejects the
/// whole shadow. `farthestDepth` starts at zero and keeps the largest
/// `depth` seen, which orders the primitive behind all four corners.
///
/// Reserve the complete, word-aligned block and release it before the drawer
/// returns. Pointers into the block must not survive release.
typedef struct {
    SVECTOR vertices[4];     // Square's corners in the coordinate's own frame, all at one height
    s32     depth;           // Latest corner's SZ3 / 4
    s32     depthCue;        // GTE IR0 depth-cue coefficient of the latest corner, with 12 fractional bits; stored, never read
    s32     projectionFlags; // GTE FLAG word of the latest corner; bit 31 makes it negative and rejects the shadow
    u32     screenXy[4];     // Projected corners, one GTE screen word each (X in bits 0..15, Y in bits 16..31)
    s32     farthestDepth;   // Largest corner depth so far, never below zero; selects the ordering-table entry
} _ActorRenderGroundShadowScratch;
STATIC_ASSERT_SIZEOF(_ActorRenderGroundShadowScratch, 0x40);

u8* D_80114D10;

u16 D_80114D14[2];

s16 D_80114D18;

s16 D_80114D1A;

s16 D_80114D1C;

s32 D_80114D20;

/// 0-terminated `_CdCmdSceneSoundBankBit` table walked by `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks`.
extern _CdCmdSceneSoundBankBit Gp_SndMaskTable[];

/// Printed when an enemy's work block cannot be allocated.
static const char Gp_StrNewEnemyNull[];

/// Three-entry dispatcher table: `_enemyStartTeardownDelay`, `_enemyWaitTick`, `enemyDestroy`.
static const EnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

static const TaskFuncTable3 Gp_StageLoadStates;

static const VECTOR D_80093A28;

static const TaskFuncTable3 D_80093A38;

static const char _gAnimationUnsupportedPoseDiagnostic[24];

static const TaskFuncTable3 D_80093A5C;

/// Pixel extent of the fixed gameplay screen effects.
enum { DISPLAY_EFFECT_WIDTH  = 320,
       DISPLAY_EFFECT_HEIGHT = 240 };

/// Fixed pulse counter and the shift from counter steps to byte intensity.
enum { FADE_PULSE_STEPS           = 32,
       FADE_PULSE_LAST_STEP       = FADE_PULSE_STEPS - 1,
       FADE_PULSE_INTENSITY_SHIFT = 3 };

/// Previous-frame redraw modes and optional fade timing in task ticks.
enum { DISPLAY_FRAME_REDRAW_FADE            = 1,
       DISPLAY_FRAME_REDRAW_DOUBLE_PASS     = 16,
       DISPLAY_FRAME_REDRAW_FADE_DELAY      = 60,
       DISPLAY_FRAME_REDRAW_FADE_STEP       = 8,
       DISPLAY_FRAME_REDRAW_SUPPRESSED_DEMO = 1,
       DISPLAY_FRAME_REDRAW_MASK_OT_TAG     = 1023,
       DISPLAY_FRAME_REDRAW_TEXTURE_DEPTH   = 2,
       DISPLAY_FRAME_REDRAW_RIGHT_PAGE_X    = 128,
       DISPLAY_FRAME_REDRAW_RIGHT_U         = 32,
       DISPLAY_FRAME_REDRAW_LOWER_BUFFER_V  = 16 };

/// GPU packet commands used by the fixed screen effects.
///
/// Draw modes retain dithering; the redraw modes additionally permit drawing to display.
enum { GPU_SCREEN_SEMITRANSPARENT_TILE     = 0x62,
       GPU_SCREEN_RAW_SEMITRANSPARENT_QUAD = 0x2F,
       GPU_SCREEN_DRAW_MODE_SUBTRACT       = 0xE1000200 | (GPU_BLEND_SUBTRACT << 5),
       GPU_SCREEN_DRAW_MODE_ADD            = 0xE1000200 | (GPU_BLEND_ADD << 5),
       GPU_SCREEN_DRAW_MODE_DITHER_DISPLAY = 0xE1000600,
       GPU_SCREEN_DRAW_MODE_DISPLAY        = 0xE1000400 };

/// RGB555 format masks and the channel scale used by the GTE blend.
enum { GPU_RGB555_CHANNEL_MASK      = 0x1F,
       GPU_RGB555_GTE_CHANNEL_SHIFT = 7,
       GPU_RGB555_GTE_CHANNEL_MASK  = GPU_RGB555_CHANNEL_MASK << GPU_RGB555_GTE_CHANNEL_SHIFT,
       GPU_RGB555_STP_MASK          = 0x8000 };

/// Scene-child query results.
enum { SCENE_CHILD_LOOKUP_FOUND     = 0,
       SCENE_CHILD_LOOKUP_NOT_FOUND = -1 };

/// Fractional bits of the Q12 unit vectors used for Euler extraction.
enum { GRAPHICS_FIXED_FRACTION_BITS = 12 };

/// High byte of `Enemy::workType` for an actor the scene manager placed.
enum {
    SCENE_ACTOR_BANK_SHIFT  = 8,
    SCENE_PLACED_ACTOR_BANK = ENEMY_WORK_PLAIN >> SCENE_ACTOR_BANK_SHIFT,
};

extern TaskMessageEntry Gp_Slot4MsgTable[5];

static s32 _sceneFindPlacedActor(Task* scene, s32 messageId, s32 placeKey, Task** reply);

static s32 _sceneFindOtherChild(Task* scene, s32 messageId, s32 childId, Task** reply);

static s32 _sceneExitPlacedActors(Task* scene, s32 messageId, s32 firstArg, s32 secondArg);

static s32 _sceneBroadcastToPlacedActors(Task* scene, s32 messageId, s32 payload, s32 childMessage);

static void Gp_ApplySndMasks(u16 arg0);

static Enemy* Gp_SpawnEnemy(s32 bank, s32 type, s32 arg2, Enemy* parent);

static Enemy* Gp_AllocEnemy(Task* task, Enemy* parent);

static void _enemyStartTeardownDelay(Enemy* enemy, Task* task);

static void _enemyWaitTick(Enemy* enemy, Task* task);

static s32 Gp_TryEnqueueSndCd(s32 arg0);

void func_800B06F0(Task* arg0);

static void Gp_StartStageLoad(Task* task);

static void Gp_FinishStageLoad(Task* task);

static void Gp_StageLoadState2(Task* task);

static void _fadeTickPulse(Task* task);

static void _gpuBlendRgb555(const u16* first, const u16* second, s32 firstWeight, u16* destination);

static void _gpuBlendRgb555ClutRowMasked(const u16* first, const u16* second, s32 firstWeight, u16* destination, s32 entryMask);

static void _fadeStartPulse(Task* task);

static void _animationBlendTranslationRotation(const _AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot);

static void _animationBlendPackedRotation(const _AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot);

static void Gp_AnimAdvanceSlot(AnimationContext* context, s32 arg1);

static inline void _animationSeekSlotWithBlend(AnimationContext* context, s32 slotIndex, u16 setIndex, s32 trackRecordOffset, s32 blendFrames);

static void _animationSeekSlotWithRecordDuration(AnimationContext* context, s32 slotIndex, s32 setIndex, s32 trackRecordOffset);

static void _animationTickDirectSlot(AnimationContext* context, AnimationSlot* slot);

static void _animationStartDirectTrack(AnimationContext* context, AnimationSlot* slot, s32 partIndex, s32 setIndex);

static void _animationStartDirectRecord(AnimationContext* context, AnimationSlot* slot, s32 partIndex, s32 setIndex, s32 trackRecordOffset);

static void _animationSeekTrackStart(AnimationContext* context, s32 slotIndex, s32 setIndex);

static void _animationSelectNextPoseRecord(const AnimationContext* unusedContext, AnimationSlot* slot, u16 setIndex, u16 recordIndex);

static void _animationSelectCurrentPoseRecord(const AnimationContext* unusedContext, AnimationSlot* slot, u16 setIndex, u16 recordIndex);

static void _displayRedrawPreviousFrame(Task* task);

static void Gp_SetCurAreaFlag2(s32 useSavedPoses);

static AreaSavedState* Gp_GetAreaObj(GameLocationKey* key);

static void _areaPrepareSpawnState(GameLocationKey* key, AreaSavedState* areaState);

static AreaResource* Gp_GetNestedAreaObj(GameLocationKey* key);

static void Gp_KillSlot4Children(void);

static void func_800B6014(void);

static void _displayStartPreviousFrameRedraw(Task* task);

/// The 2-bit state of entry `arg0` in the current stage's `Gp_Bit2Banks` flags.
static inline s32 _gpGetCurBit2Flag(s32 arg0);

static inline void _gpSpawnPlace(AreaObjectSpawn* spawn, AreaObjectPlace* place);

/// Inline form of `Gp_GetRelatedQty`: the most of a related item weapon
/// `item` can hold, from bank `bank`'s table, or 0 for a non-weapon id.
static inline s32 _gpRelatedQty(s32 item, s32 bank);

/// How much of `item` the rows `scan` selects hold: the stack count for
/// stackable ids (0xA0 and up), otherwise 1 if any row carries it and 0 if not.
static inline s16 _gpScanHeldQty(InventoryItemRow* table, InventoryItemRange* scan, s32 item);

static void _sceneInitializeManager(Task* sceneTask);

static void _worldTargetDrawOverlayTask(Task* unusedTask);

extern TaskDesc D_aya_20900_80115D9C[];

extern TaskDesc D_replay_bonus_80119218;

extern TaskDesc D_replay_bonus_8011922C[];

extern TaskDesc D_actor_361100_801637C8;

_CdCmdSceneSoundBankBit Gp_SndMaskTable[7] = {
    { 1, SOUND_ID(SOUND_BANK_TYPE_COMMON, 0, 0) },
    { 4, SOUND_SCRIPT_REQUEST_TYPE_1 },
    { 8, SOUND_AREA_BANK_ALL },
    { 2, SOUND_BANK_TYPE_WEAPON_ALL },
    { 16, SOUND_BANK_TYPE_CHARACTER_ALL },
    { 32, SOUND_BANK_TYPE_ALL_NON_AMBIENT },
    { 0, 0 },
};
TaskDesc D_8010D1FC = { { { TASK_BODY_NONE, 192 } }, func_800B06F0, { NULL } };

static const TaskFuncTable3 Gp_StageLoadStates;
static const VECTOR         D_80093A28;
static const TaskFuncTable3 D_80093A38;
static const TaskFuncTable3 D_80093A5C;

static const char Gp_StrNewEnemyNull[];

static const EnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

/// Blends Euler pitch/yaw and clamps them without pulling an existing pose inward.
///
/// The writable angles and read-only current pose use 4096 units per turn;
/// weight uses 1/4096 units. Current roll is copied. The angle storage must
/// be disjoint, and limits are nonnegative. No wrap correction is performed.
///
/// Expands to a compound statement for standalone calls. Requires writable
/// aggregate and limit lvalues without side effects: each is evaluated repeatedly.
/// Captures four caller-declared s32 locals: currentPitchMagnitude,
/// currentYawMagnitude, pitchMagnitude and yawMagnitude. Each receives a
/// current or candidate angle magnitude for its corresponding axis.
#define ANIMATION_BLEND_LIMITED_HEAD_ANGLES(aimAngles, currentAngles, maxYaw, maxPitch, blendWeight)              \
    {                                                                                                             \
        (aimAngles).vx        = (currentAngles).vx + ((aimAngles).vx - (currentAngles).vx) * (blendWeight) / ONE; \
        (aimAngles).vy        = (currentAngles).vy + ((aimAngles).vy - (currentAngles).vy) * (blendWeight) / ONE; \
        (aimAngles).vz        = (currentAngles).vz;                                                               \
        currentPitchMagnitude = (currentAngles).vx >= 0 ? (currentAngles).vx : -(currentAngles).vx;               \
        if ((maxPitch) < currentPitchMagnitude) {                                                                 \
            (maxPitch) = currentPitchMagnitude;                                                                   \
        }                                                                                                         \
        currentYawMagnitude = (currentAngles).vy >= 0 ? (currentAngles).vy : -(currentAngles).vy;                 \
        if ((maxYaw) < currentYawMagnitude) {                                                                     \
            (maxYaw) = currentYawMagnitude;                                                                       \
        }                                                                                                         \
        pitchMagnitude = (aimAngles).vx >= 0 ? (aimAngles).vx : -(aimAngles).vx;                                  \
        if ((maxPitch) < pitchMagnitude) {                                                                        \
            (aimAngles).vx = (aimAngles).vx < 0 ? -(maxPitch) : (maxPitch);                                       \
        }                                                                                                         \
        yawMagnitude = (aimAngles).vy >= 0 ? (aimAngles).vy : -(aimAngles).vy;                                    \
        if ((maxYaw) < yawMagnitude) {                                                                            \
            (aimAngles).vy = (aimAngles).vy < 0 ? -(maxYaw) : (maxYaw);                                           \
        }                                                                                                         \
    }

/// Measures the Q12 cross product of two blended matrix rows.
///
/// Both row vectors stay live through the call. The writable vectors receive
/// the cross product and its normalized direction; the return is its length.
/// The two output vectors must be distinct. Changes GTE state.
///
/// This GNU statement expression returns the length. Pointer arguments must be
/// stable and side-effect-free; rowCrossProduct is evaluated twice and the
/// other pointers once. Its local vector operands are provided by the caller.
#define GRAPHICS_MEASURE_ROW_INDEPENDENCE(firstRow, secondRow, rowCrossProduct, normalizedCrossProduct) \
    ({                                                                                                  \
        gte_ldopv1((firstRow));                                                                         \
        gte_ldopv2((secondRow));                                                                        \
        gte_op12();                                                                                     \
        gte_stlvnl((rowCrossProduct));                                                                  \
        VectorNormal((rowCrossProduct), (normalizedCrossProduct));                                      \
    })

/// Primes mapped playback indices without selecting the first record or pose encoding.
///
/// Indices must fit their slot fields and corresponding borrowed arrays.
/// The caller binds the set table, selects endpoints and clears playback status.
///
/// Pointer and index arguments must be stable and side-effect-free. Evaluates
/// slot seven times and targetSetIndex twice; the track and coordinate indices
/// once each. Stores narrow to the existing byte/halfword fields. Captures no locals.
#define ANIMATION_PRIME_MAPPED_SLOT(slot, targetSetIndex, sourceTrackIndex, destinationCoordIndex) \
    do {                                                                                           \
        (slot)->rate                            = ANIMATION_RATE_ONE;                              \
        (slot)->timeLeft                        = 0;                                               \
        (slot)->currentPose.indices.setIndex    = (targetSetIndex);                                \
        (slot)->currentPose.indices.recordIndex = 0;                                               \
        (slot)->coordIndex                      = (destinationCoordIndex);                         \
        (slot)->trackIndex                      = (sourceTrackIndex);                              \
        (slot)->nextPose.indices.setIndex       = (targetSetIndex);                                \
    } while (0)

/// Selects the demo transition's target record while retaining the capture tick's flags.
///
/// Candidate and fallback indices must fit the target record array. Jump controls
/// install their absolute wordOffset; stop retains the slot's prior next index.
/// The control chain must terminate; record storage remains borrowed.
///
/// candidateIndex is a writable u16 lvalue and is updated by the walk. All
/// arguments must be stable and side-effect-free; slot and the index are
/// evaluated repeatedly; records is read at the flag test and again for a
/// visited control. Captures no locals.
#define ANIMATION_SELECT_DIRECT_BLEND_RECORD(slot, records, candidateIndex)          \
    do {                                                                             \
        const AnimationRecord* controlRecord;                                        \
                                                                                     \
        while ((s8)(records)[(candidateIndex)].flags < 0) {                          \
            controlRecord = (records) - -(s32)(candidateIndex);                      \
            if (controlRecord->flags < ANIMATION_RECORD_END_THRESHOLD) {             \
                (candidateIndex) = controlRecord->wordOffset;                        \
                if ((candidateIndex) == (slot)->nextPose.indices.recordIndex) {      \
                    (slot)->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;  \
                }                                                                    \
                (slot)->status.fields.flags |= ANIMATION_SLOT_FOLLOWED_JUMP;         \
            } else {                                                                 \
                (candidateIndex)             = (slot)->nextPose.indices.recordIndex; \
                (slot)->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;      \
                break;                                                               \
            }                                                                        \
        }                                                                            \
        (slot)->nextPose.indices.recordIndex = (candidateIndex);                     \
    } while (0)

/// Projects one local shadow corner with the composed frame already loaded in the GTE.
///
/// `cornerIndex` selects 0..3 in the same scratch block's vertex and screen arrays.
/// Overwrites the latest depth, depth cue and flags; keeps the largest depth so far.
///
/// Arguments must be stable and side-effect-free: scratch is evaluated
/// repeatedly and cornerIndex twice. Captures no caller locals. GTE rotation,
/// translation and projection parameters must already describe the desired view.
#define ACTOR_RENDER_PROJECT_SHADOW_CORNER(scratch, cornerIndex) \
    do {                                                         \
        gte_ldv0(&(scratch)->vertices[(cornerIndex)]);           \
        gte_rtps();                                              \
        gte_stsxy(&(scratch)->screenXy[(cornerIndex)]);          \
        gte_stdp(&(scratch)->depthCue);                          \
        gte_stflg(&(scratch)->projectionFlags);                  \
        gte_stszotz(&(scratch)->depth);                          \
        if ((scratch)->depth > (scratch)->farthestDepth) {       \
            (scratch)->farthestDepth = (scratch)->depth;         \
        }                                                        \
    } while (0)

s32 func_800AF590(s32 unused0, s32 unused1)
{
    StreamSceneImageHeader header;
    CdCmdQueue*            p;

    p = &gCdCmdQueue;
    switch (D_80114D14[0]) {
        case 0:
            // Read the serialized header before choosing or reusing its payload buffer.
            CdGetSector(&header, sizeof(header) / sizeof(u_long));
            D_80114D1A = 1;
            D_80114D1C = header.forceReload;
            if ((D_80114D1C == 0) && ((s16)p->scenePayloadReusable != 0)) {
                D_80114D1A = 0;
            }
            if ((s16)header.sectorCount != 0) {
                p->scenePayloadLoading = 1;
                (*(D_80114D14 + 1))    = header.bufferKind;
                if (D_80114D1A != 0) {
                    memCopyBytes(&header, &p->sceneImageHeaders[(*(D_80114D14 + 1))], sizeof(header));
                }
                switch ((*(D_80114D14 + 1))) {
                    case STREAM_SCENE_BUFFER_DECODE:
                        D_80114D10               = p->decodeBuffer;
                        p->nextDecodeBufferBytes = header.nextDecodeBufferBytes;
                    default:
                        break;
                    case STREAM_SCENE_BUFFER_ACTOR_0:
                        D_80114D10 = (u8*)Fs_ActorLoadBase0;
                        if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_0) {
                            D_80114D10 = (u8*)Fs_ActorLoadBase0 + STREAM_VLC_TABLE_BYTES;
                        }
                        if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_0) {
                            D_80114D10 += p->sceneStream->data.scene.timingBufferBytes;
                        }
                        break;
                    case STREAM_SCENE_BUFFER_ACTOR_1:
                        D_80114D10 = (u8*)Fs_ActorLoadBase1;
                        if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_1) {
                            D_80114D10 = (u8*)Fs_ActorLoadBase1 + STREAM_VLC_TABLE_BYTES;
                        }
                        if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_1) {
                            D_80114D10 += p->sceneStream->data.scene.timingBufferBytes;
                        }
                        break;
                    case STREAM_SCENE_BUFFER_ACTOR_2:
                        D_80114D10 = (u8*)Fs_ActorLoadBase2;
                        if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_2) {
                            D_80114D10 = (u8*)Fs_ActorLoadBase2 + STREAM_VLC_TABLE_BYTES;
                        }
                        if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_2) {
                            D_80114D10 += p->sceneStream->data.scene.timingBufferBytes;
                        }
                        break;
                    case STREAM_SCENE_BUFFER_EXTERNAL:
                        D_80114D10 = p->externalScenePayloadBuffer;
                        break;
                }
                if (D_80114D1A != 0) {
                    CdGetSector(D_80114D10, SECTOR_SIZE - sizeof(header) / sizeof(u_long));
                }
                D_80114D10   += SECTOR_SIZE * sizeof(u_long) - sizeof(header);
                D_80114D14[0] = 1U;
                D_80114D18    = header.sectorCount - 1;
            }
            break;
        case 1:
            if (D_80114D18 > 0) {
                if (D_80114D1A != 0) {
                    CdGetSector(D_80114D10, SECTOR_SIZE);
                }
                D_80114D10 += SECTOR_SIZE * sizeof(u_long);
                D_80114D18  = (u16)D_80114D18 - 1;
            }
            if (D_80114D18 == 0) {
                D_80114D18 = (u16)D_80114D18 - 1;
                if (gCdCmdQueue.sceneEnded == 0) {
                    gCdCmdQueue.scenePayloadAvailable = 1;
                }
                gCdCmdQueue.scenePayloadLoading = 0;
                if (D_80114D1C == 0) {
                    gCdCmdQueue.scenePayloadReusable = 1;
                }
                D_80114D14[0] = 0U;
            }
            break;
    }
    return 0;
}

s16 Gp_FindStreamSlot(u16 arg0, u16 arg1, u16 arg2, u16 arg3)
{
    CdCmdQueue* p;
    StreamSlot* slot;
    u16         count;
    u16         i;
    u16         found;
    s32         decodeBufferBytes;
    u16         vlcTableMode;
    u32         savedRandomState;

    p = &gCdCmdQueue;
    if (arg0 == 0) {
        slot  = Fs_Streams;
        count = ARRAY_SIZE(Fs_Streams);
    } else {
        slot  = Stream_Slots;
        count = ARRAY_SIZE(Stream_Slots);
    }

    for (i = 0, found = 0; i < count; i++, slot++) {
        if (slot->kind == STREAM_KIND_SCENE_AUDIO && slot->startSector != 0 && slot->key.parts.group == arg0 && slot->key.parts.id == arg1 &&
            slot->subId == arg2 && slot->data.scene.subId2 == arg3) {
            found = 1;
            break;
        }
    }

    if (found == 0) {
        return -1;
    }

    memFillBytes(p->sceneImageHeaders, 0, sizeof(p->sceneImageHeaders));
    p->sceneStream        = slot;
    p->sceneBuffersNeeded = 1;
    decodeBufferBytes     = slot->source.decodeBufferBytes;
    if (decodeBufferBytes != 0) {
        p->decodeBufferBytes = decodeBufferBytes;
    } else {
        p->decodeBufferBytes = 0;
    }
    p->sceneEnded          = 0;
    p->vlcTableBuilt       = 0;
    p->scenePayloadLoading = 0;
    p->imageLayout         = FILE_SYSTEM_IMAGE_CONTIGUOUS;
    vlcTableMode           = slot->data.scene.vlcTableMode;
    savedRandomState       = gRandomLcgState;
    *D_80114D14            = 0;
    p->sceneVlcTableMode   = vlcTableMode;
    p->savedLcgState       = savedRandomState;
    p->savedRandSeed       = rand();
    gRandomLcgState        = 0;
    srand(1);
    D_80114D20 = 0xFFFF;
    return i;
}

void Gp_StepCdAudioCmd(void)
{
    enum { STREAM_CD_SECTOR_BYTES = 0x800 };
    s32         one;
    s32         i_s1;
    CdCmdQueue* p;
    s32         seed;
    s16         ret;
    s32         save23;
    s32         sector;

    p = &gCdCmdQueue;
    switch (p->entries[p->readIdx].cmd) {
        case CD_COMMAND_EMPTY:
            break;
        case CD_COMMAND_PLAY_SCENE_AUDIO:
        case CD_COMMAND_START_SCENE_AUDIO:
            switch (p->step) {
                case 0:
                    cdCmdSetBusy();
                    p->sceneAudioMode = CD_COMMAND_SCENE_STARTING_AUDIO;
                    ret               = CdCmd_PollStatus(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            if (ret == 0) {
                                return;
                            }
                            break;
                        }
                        if (ret != 2) {
                            break;
                        }
                        CdFlush();
                    }
                    if (p->sceneAudioStarted == 0) {
                        p->step = p->step + 1;
                        break;
                    }
                    p->step = 6;
                    goto case6;
                case 1:
                case 2:
                    p->step = p->step + 1;
                    break;
                case 3: {
                    StreamSlot* sceneStream;

                    sceneStream           = p->sceneStream;
                    p->cdOperationPending = 1;
                    if (sceneStream->control.scene.timingBufferKind != STREAM_TIMING_BUFFER_NONE) {
                        sector = sceneStream->startSector;
                        if ((sceneStream->data.scene.timingBytes - 1) / STREAM_CD_SECTOR_BYTES != 0) {
                            sector += 1 + (sceneStream->data.scene.timingBytes - 1) / STREAM_CD_SECTOR_BYTES;
                        }
                        Fs_ReadSectorEx(p->sceneStream->startSector, sector, p->timingBuffer, 0);
                        p->step = p->step + 1;
                    } else {
                        p->step = 5;
                    }
                    break;
                }
                case 4:
                    if (Fs_CdOpStatus != 0xFF) {
                        break;
                    }
                    ret = CdCmd_PollStatus(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            if (ret == 0) {
                                return;
                            }
                            break;
                        }
                        if (ret != 2) {
                            break;
                        }
                        CdFlush();
                        p->step = 3;
                        break;
                    }
                    p->step = p->step + 1;
                    break;
                case 5: {
                    StreamSlot*              sceneStream;
                    s32                      bits;
                    u16                      maskbits;
                    _CdCmdSceneSoundBankBit* entry;

                    sceneStream = p->sceneStream;
                    sector      = sceneStream->startSector;
                    if (sceneStream->control.scene.timingBufferKind != STREAM_TIMING_BUFFER_NONE) {
                        sector += 1;
                        sector += (sceneStream->data.scene.timingBytes - 1) / STREAM_CD_SECTOR_BYTES;
                    }
                    CdAudio_StartTrack(sector, p->sceneStream->control.scene.volumeIndex);
                    i_s1     = 0;
                    maskbits = p->sceneStream->data.scene.soundBankMask;
                    if (Gp_SndMaskTable[0].mask != 0) {
                        bits = maskbits;
                        do {
                            entry = &Gp_SndMaskTable[(u16)i_s1];
                            if (bits & entry->mask) {
                                sndEvtRequestScriptStop(entry->bankTypeId, SOUND_SCRIPT_STOP_NO_FADE);
                                SndBank_SetEnableFlags(0, entry->bankTypeId);
                            }
                            i_s1++;
                        } while (Gp_SndMaskTable[(u16)i_s1].mask != 0);
                    }
                    p->releasePauseBlockAfterFade = 0;
                    p->blockGamePause             = 1;
                    p->step                       = p->step + 1;
                    break;
                }
                case 6:
                case6: {
                    s32 cmd;

                    if (CdAudio_Phase.openStep != CD_AUDIO_OPEN_STEP_DONE) {
                        break;
                    }
                    one                  = 1;
                    p->sceneAudioStarted = one;
                    cmd                  = p->entries[p->readIdx].cmd;
                    if (cmd == CD_COMMAND_START_SCENE_AUDIO) {
                        cdCmdSaveHeadRequest();
                        cdCmdCompleteHeadRequest();
                        break;
                    }
                    if (cmd != CD_COMMAND_PLAY_SCENE_AUDIO) {
                        break;
                    }
                    save23            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene;
                    p->sceneAudioMode = one;
                    if (save23 != 0) {
                        sndEvtRequestScriptStart(0, 0, 0);
                    }
                    if (p->sceneStream->control.scene.timingBufferKind != STREAM_TIMING_BUFFER_NONE) {
                        p->paceToSceneTiming = one;
                    }
                    p->timingElapsedLines = 0;
                    CdAudio_RequestStopB();
                    p->blockGamePause     = one;
                    p->cdOperationPending = 0;
                    p->step               = p->step + 1;
                    break;
                }
                case 7: {
                    StreamSlot*              sceneStream;
                    s32                      i;
                    s32                      bits;
                    u16                      maskbits;
                    _CdCmdSceneSoundBankBit* entry;

                    if (CdAudio_Phase.playStep != CD_AUDIO_PLAY_STEP_DONE) {
                        break;
                    }
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0) {
                        sndEvtRequestScriptStart(0, 0, 0);
                    }
                    memFillBytes(&p->activeRequest, 0, sizeof(p->activeRequest));
                    sceneStream             = p->sceneStream;
                    p->replacementEntry.cmd = CD_COMMAND_EMPTY;
                    if (sceneStream->data.scene.resumeSectorOffset != 0) {
                        CdAudio_JumpToSector(sceneStream->startSector + sceneStream->data.scene.resumeSectorOffset);
                        p->cdOperationPending = 1;
                        p->step               = p->step + 1;
                        break;
                    }
                    i        = 0;
                    maskbits = sceneStream->data.scene.soundBankMask;
                    if (Gp_SndMaskTable[0].mask != 0) {
                        bits = maskbits;
                        do {
                            entry = &Gp_SndMaskTable[(u16)i];
                            if (bits & entry->mask) {
                                SndBank_SetEnableFlags(1, entry->bankTypeId);
                            }
                            i++;
                        } while (Gp_SndMaskTable[(u16)i].mask != 0);
                    }
                    {
                        CdCmdQueue* q;
                        s32         ff;
                        q                        = &gCdCmdQueue;
                        seed                     = q->savedRandSeed;
                        ff                       = 0xFF;
                        p->blockGamePause        = 0;
                        p->sceneAudioMode        = CD_COMMAND_SCENE_INACTIVE;
                        q->imageLoadStatus       = ff;
                        q->sceneEnded            = 1;
                        q->scenePayloadAvailable = 0;
                        q->sceneAudioStarted     = 0;
                        q->sceneBuffersNeeded    = 0;
                        q->paceToSceneTiming     = 0;
                        gRandomLcgState          = q->savedLcgState;
                        srand(seed);
                    }
                    cdCmdCompleteHeadRequest();
                    break;
                }
                case 8: {
                    s32                      i;
                    s32                      bits;
                    u16                      maskbits;
                    _CdCmdSceneSoundBankBit* entry;

                    if (CdAudio_Phase.waveLoadStep != CD_AUDIO_WAVE_LOAD_STEP_DONE) {
                        break;
                    }
                    i        = 0;
                    maskbits = p->sceneStream->data.scene.soundBankMask;
                    if (Gp_SndMaskTable[0].mask != 0) {
                        bits = maskbits;
                        do {
                            entry = &Gp_SndMaskTable[(u16)i];
                            if (bits & entry->mask) {
                                SndBank_SetEnableFlags(1, entry->bankTypeId);
                            }
                            i++;
                        } while (Gp_SndMaskTable[(u16)i].mask != 0);
                    }
                    {
                        CdCmdQueue* q;
                        s32         ff;
                        q                        = &gCdCmdQueue;
                        seed                     = q->savedRandSeed;
                        ff                       = 0xFF;
                        p->cdOperationPending    = 0;
                        p->blockGamePause        = 0;
                        p->sceneAudioMode        = CD_COMMAND_SCENE_INACTIVE;
                        q->imageLoadStatus       = ff;
                        q->sceneEnded            = 1;
                        q->scenePayloadAvailable = 0;
                        q->sceneAudioStarted     = 0;
                        q->sceneBuffersNeeded    = 0;
                        q->paceToSceneTiming     = 0;
                        gRandomLcgState          = q->savedLcgState;
                        srand(seed);
                    }
                    cdCmdCompleteHeadRequest();
                    break;
                }
            }
            break;
    }
    CdCmd_StepVlcRebuild();
}

static void Gp_ApplySndMasks(u16 arg0)
{
    s32                      i;
    s32                      bits;
    _CdCmdSceneSoundBankBit* entry;

    i = 0;
    if (Gp_SndMaskTable[0].mask != 0) {
        bits = arg0;
        do {
            entry = &Gp_SndMaskTable[(u16)i];
            if (bits & entry->mask) {
                sndEvtRequestScriptStop(entry->bankTypeId, SOUND_SCRIPT_STOP_NO_FADE);
                SndBank_SetEnableFlags(0, entry->bankTypeId);
            }
            i++;
        } while (Gp_SndMaskTable[(u16)i].mask != 0);
    }
}

void Gp_ApplySndBankMasks(u16 arg0)
{
    s32                      i;
    s32                      bits;
    _CdCmdSceneSoundBankBit* entry;

    i = 0;
    if (Gp_SndMaskTable[0].mask != 0) {
        bits = arg0;
        do {
            entry = &Gp_SndMaskTable[(u16)i];
            if (bits & entry->mask) {
                SndBank_SetEnableFlags(1, entry->bankTypeId);
            }
            i++;
        } while (Gp_SndMaskTable[(u16)i].mask != 0);
    }
}

void Gp_RestoreStreamRng(void)
{
    CdCmdQueue* p;

    p                        = &gCdCmdQueue;
    p->imageLoadStatus       = CD_COMMAND_IMAGE_COMPLETE;
    p->sceneEnded            = 1;
    p->scenePayloadAvailable = 0;
    p->sceneAudioStarted     = 0;
    p->sceneBuffersNeeded    = 0;
    p->paceToSceneTiming     = 0;
    gRandomLcgState          = p->savedLcgState;
    srand(p->savedRandSeed);
}

s32 func_800B0118(s32 arg0, s32 arg1)
{
    s16 temp;

    temp = arg0;
    if (temp != 0) {
        D_80114D20          = temp;
        GameMain_HaltFlags |= 8;
    } else {
        GameMain_HaltFlags &= ~8;
    }
    return 0;
}

void streamSetExternalScenePayloadBuffer(u8* payloadBuffer)
{
    gCdCmdQueue.externalScenePayloadBuffer = payloadBuffer;
}

static Enemy* Gp_SpawnEnemy(s32 bank, s32 type, s32 arg2, Enemy* parent)
{
    Task*  task;
    Enemy* ret;

    task = Task_Spawn(bank, type, arg2, 0);
    if (task != NULL) {
        ret = Gp_AllocEnemy(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

Enemy* Gp_SpawnEnemyFromTable(TaskDesc* table, s32 idx, s32 arg2, Enemy* parent)
{
    Task*  task;
    Enemy* ret;

    task = taskSpawnFromTable(table, idx, arg2, 0);
    if (task != NULL) {
        ret = Gp_AllocEnemy(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

/// Detaches target tracking and frees a live primary-heap enemy work object.
///
/// `enemy` must be non-NULL. Its target entry may already be off the list.
/// Actor locks are released and the entry is removed before the allocation is
/// freed, so the node is read only while the object is still live. The owning
/// task keeps running; the caller starts teardown. `enemy` is invalid on return.
static inline void _enemyReleaseWork(Enemy* enemy)
{
    worldTargetUnlinkNode(&enemy->node);
    memFree(enemy);
}

void enemyDestroy(Enemy* enemy, Task* task)
{
    _enemyReleaseWork(enemy);
    taskKill(task);
}

void enemyTaskExit(Task* task)
{
    Enemy* enemy = task->spawnArg2.pointer;

    // Release the enemy before task teardown dispatches child exit handlers.
    _enemyReleaseWork(enemy);
    taskKill(task);
}

Task* actorRenderCopyCoordBodyTransform(Task* task, GfxCoord* sourceCoord, const SVECTOR* localOffset)
{
    enum { ACTOR_RENDER_COORD_COPY_SCRATCH_BYTES = 8 };

    ModelObjectCoordBody* body;
    GfxCoord*             destinationCoord;
    GfxCoord*             worldParent;

    if (task == NULL) {
        return NULL;
    }

    SCRATCH_STACK_RESERVE_BYTES(ACTOR_RENDER_COORD_COPY_SCRATCH_BYTES);
    worldParent      = &gGfxViewCoord;
    body             = task->extra.coordBody;
    destinationCoord = body->coord;
    if (sourceCoord->parent == worldParent) {
        destinationCoord->coord = sourceCoord->coord;
        gte_SetRotMatrix(&sourceCoord->coord);
        gte_SetTransMatrix(&sourceCoord->coord);
        gte_ldv0(localOffset);
        gte_rtv0tr();
        gte_stlvnl(destinationCoord->coord.t);
    } else {
        // Compose through the view, then recover a world-parented local matrix.
        actorRenderComposeCoord(sourceCoord);
        destinationCoord->workm = sourceCoord->workm;
        gte_SetRotMatrix(&sourceCoord->workm);
        gte_SetTransMatrix(&sourceCoord->workm);
        gte_ldv0(localOffset);
        gte_rtv0tr();
        gte_stlvnl(destinationCoord->workm.t);
        gfxMakeRelativeTransform(&worldParent->workm, &destinationCoord->workm, &destinationCoord->coord);
    }
    destinationCoord->parent       = &gGfxViewCoord;
    destinationCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_RENDER_COORD_COPY_SCRATCH_BYTES);
    return task;
}

static Enemy* Gp_AllocEnemy(Task* task, Enemy* parent)
{
    Enemy* enemy;

    enemy = memCalloc(sizeof(Enemy), 0);
    if (enemy == NULL) {
        printf(Gp_StrNewEnemyNull);
        taskKill(task);
        return NULL;
    }

    task->exitCallback      = enemyTaskExit;
    task->spawnArg2.pointer = enemy;
    enemy->task             = task;
    enemy->coord            = &gGfxViewCoord;
    if (parent != NULL) {
        taskReparent(parent->task, task);
    } else {
        taskReparent(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), enemy->task);
    }
    return enemy;
}

/// Starts the 120-frame delay before a bodyless enemy is destroyed.
///
/// `enemy` is the task's live work object. Sets its countdown and advances
/// the task from initialization to the countdown state; neither object is freed.
static void _enemyStartTeardownDelay(Enemy* enemy, Task* task)
{
    enemy->waitTicks = ENEMY_WAIT_FRAMES;
    task->state++;
}

/// Counts down one frame of an enemy's bodyless teardown delay.
///
/// Requires a positive `waitTicks` set by the preceding state. Reaching zero
/// advances the task to its destruction state; this tick frees no storage.
static void _enemyWaitTick(Enemy* enemy, Task* task)
{
    enemy->waitTicks--;
    if (enemy->waitTicks == 0) {
        task->state++;
    }
}

void enemyTeardownDelayTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = Gp_EnemyWaitFuncs;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

static s32 Gp_TryEnqueueSndCd(s32 arg0)
{
    u8 param1[8];
    u8 param2[8];

    if (cdCmdIsIdle() & 0xFFFF) {
        param1[0] = arg0;
        param1[3] = 0;
        param1[2] = 5;
        param2[0] = 1;
        param2[1] = 1;
        param2[3] = 0;
        param2[2] = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        D_800626E8 = 1;
        return 0;
    }
    return 0xFF;
}

void Gp_EnqueueSndCd(u8 arg0)
{
    u8  param1[8];
    u8  param2[8];
    s32 flag;

    if (gGameSession->loadedSndId != arg0) {
        sndEvtRequestScriptStop(SOUND_BANK_TYPE_PE_ALL, 8);
        flag      = 1;
        param1[3] = 0;
        param1[2] = 5;
        param1[0] = arg0;
        param2[0] = flag;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        D_800626E8                = flag;
        gGameSession->loadedSndId = arg0;
    }
}

void func_800B06F0(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Gp_StageLoadStates;
    sp.funcs[arg0->state](arg0);
}

static void Gp_StartStageLoad(Task* task)
{
    s32             i;
    u8              param1[8];
    u8              param2[8];
    FsResourceSlot* resourceSlots;
    s32             fileId;

    if (midiIsSequenceBusy(0) == 0) {
        gDisplayState.suppressDisconnectPause = 1;
        i                                     = 0;
        resourceSlots                         = D_8006C338;
        do {
            resourceSlots[(u8)i].kind = FILE_SYSTEM_RESOURCE_NONE;
            i++;
        } while ((u8)i < ARRAY_SIZE(D_8006C338));

        fileId = 0xA;
        if (gGameSession->restartMode != GAME_SESSION_RESTART_ENDING) {
            param1[2] = 4;
            param1[0] = 0x62;
            param1[3] = 0;
            param2[0] = 1;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            fileId = 9;
        }
        CdCmd_EnqueueLoadFile(fileId, 0, 3);
        gDisplayState.skipDraw = 0;
        task->state++;
    }
}

static void Gp_FinishStageLoad(Task* task)
{
    if (cdCmdIsIdle() & 0xFFFF) {
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        if (gGameSession->restartMode == GAME_SESSION_RESTART_ENDING) {
            taskSpawnFromTable(D_replay_bonus_8011922C, 0, 0, 0);
            taskKill(task);
        } else {
            task->spawnArg2.pointer = taskSpawnFromTable(D_aya_20900_80115D9C, 0, 0, 0);
            sndEvtRequestMidiStart(0x62, 0);
        }
        task->state++;
    }
}

static void Gp_StageLoadState2(Task* task)
{
    s32           out;
    DisplayState* ds;

    if (Task_PollKill(task->spawnArg2.pointer, &out) != 0) {
        ds                          = &gDisplayState;
        task->killCountdown         = 0;
        ds->gameMode                = DISPLAY_GAME_RESTART;
        ds->suppressDisconnectPause = 0;
        taskKill(task);
    }
}

void animationAimHeadAtTask(Task* subject, Task* targetTask, s32 maxYaw, s32 maxPitch, s32 blendWeight)
{
    VECTOR    transformedTranslation;
    VECTOR    subjectPosition;
    VECTOR    targetPosition;
    SVECTOR   targetDelta;
    SVECTOR   aimAngles;
    SVECTOR   currentAngles;
    MATRIX    subjectRotation;
    MATRIX    targetRotation;
    MATRIX    inverseSubjectRotation;
    s32       partIndex;
    GfxCoord* subjectPart;
    GfxCoord* targetPart;
    s32       currentPitchMagnitude;
    s32       currentYawMagnitude;
    s32       pitchMagnitude;
    s32       yawMagnitude;
    MATRIX*   subjectRotationStorage;
    MATRIX*   targetRotationStorage;
    GfxCoord* subjectParts;
    MATRIX*   headRotation;

    // Accumulate the pre-head translations in the retained local * accumulated order.
    partIndex                                 = 0;
    subjectRotationStorage                    = &subjectRotation;
    MATRIX_PAIR(&subjectRotation, 0, 0)       = ONE;
    MATRIX_PAIR(&subjectRotation, 0, 2)       = 0;
    MATRIX_PAIR(subjectRotationStorage, 1, 1) = ONE;
    MATRIX_PAIR(&subjectRotation, 2, 0)       = 0;
    subjectRotationStorage->m[2][2]           = ONE;
    subjectPosition.vx                        = 0;
    subjectPosition.vy                        = 0;
    subjectPosition.vz                        = 0;
    for (partIndex = 0; partIndex < ANIMATION_HEAD_PART_INDEX; partIndex++) {
        subjectPart = &subject->extra.tmd->coords[partIndex];
        ApplyMatrixLV(&subjectRotation, (VECTOR*)subjectPart->coord.t, &transformedTranslation);
        subjectPosition.vx += transformedTranslation.vx;
        subjectPosition.vy += transformedTranslation.vy;
        subjectPosition.vz += transformedTranslation.vz;
        MulMatrix0(&subjectPart->coord, &subjectRotation, &subjectRotation);
    }
    subjectPart = &subject->extra.tmd->coords[partIndex];
    ApplyMatrixLV(&subjectRotation, (VECTOR*)subjectPart->coord.t, &transformedTranslation);
    partIndex                                = 0;
    targetRotationStorage                    = &targetRotation;
    MATRIX_PAIR(&targetRotation, 0, 0)       = ONE;
    MATRIX_PAIR(&targetRotation, 0, 2)       = 0;
    MATRIX_PAIR(targetRotationStorage, 1, 1) = ONE;
    MATRIX_PAIR(&targetRotation, 2, 0)       = 0;
    targetRotationStorage->m[2][2]           = ONE;
    targetPosition.vx                        = 0;
    targetPosition.vy                        = 0;
    targetPosition.vz                        = 0;
    for (partIndex = 0; partIndex < ANIMATION_HEAD_PART_INDEX; partIndex++) {
        targetPart = &targetTask->extra.tmd->coords[partIndex];
        ApplyMatrixLV(&targetRotation, (VECTOR*)targetPart->coord.t, &transformedTranslation);
        targetPosition.vx += transformedTranslation.vx;
        targetPosition.vy += transformedTranslation.vy;
        targetPosition.vz += transformedTranslation.vz;
        MulMatrix0(&targetPart->coord, &targetRotation, &targetRotation);
    }
    targetPart = &targetTask->extra.tmd->coords[partIndex];
    ApplyMatrixLV(&targetRotation, (VECTOR*)targetPart->coord.t, &transformedTranslation);

    // Truncate separation before expressing it in the subject rotation frame.
    targetDelta.vx = (u16)targetPosition.vx - (u16)subjectPosition.vx;
    targetDelta.vy = (u16)targetPosition.vy - (u16)subjectPosition.vy;
    targetDelta.vz = (u16)targetPosition.vz - (u16)subjectPosition.vz;
    TransposeMatrix(&subjectRotation, &inverseSubjectRotation);
    ApplyMatrix(&inverseSubjectRotation, &targetDelta, &subjectPosition);

    aimAngles.vx = ratan2(-subjectPosition.vy, subjectPosition.vz >= 0 ? subjectPosition.vz : -subjectPosition.vz);
    aimAngles.vy = ratan2(subjectPosition.vx, subjectPosition.vz);
    aimAngles.vz = 0;

    subjectParts = subject->extra.tmd->coords;
    subjectPart  = subjectParts + ANIMATION_HEAD_PART_INDEX;
    gfxExtractEulerAngles(&subjectParts[ANIMATION_HEAD_PART_INDEX].coord, &currentAngles);

    // Preserve existing roll and widen limits to contain the current pose.
    ANIMATION_BLEND_LIMITED_HEAD_ANGLES(aimAngles, currentAngles, maxYaw, maxPitch, blendWeight);

    headRotation                           = &subjectPart->coord;
    MATRIX_PAIR(&subjectPart->coord, 0, 0) = ONE;
    MATRIX_PAIR(headRotation, 0, 2)        = 0;
    MATRIX_PAIR(headRotation, 1, 1)        = ONE;
    MATRIX_PAIR(headRotation, 2, 0)        = 0;
    headRotation->m[2][2]                  = ONE;
    RotMatrix(&aimAngles, headRotation);
    subjectPart->composeStamp = GRAPHICS_COORD_DIRTY;
}

void animationAimHeadAtPoint(Task* subject, const GfxCoord* targetPointFrame, s32 maxYaw, s32 maxPitch, s32 blendWeight)
{
    VECTOR    transformedTranslation;
    VECTOR    headPosition;
    VECTOR    targetPosition;
    SVECTOR   targetDelta;
    SVECTOR   aimAngles;
    SVECTOR   currentAngles;
    MATRIX    subjectRotation;
    MATRIX    inverseSubjectRotation;
    MATRIX*   subjectRotationStorage;
    MATRIX*   headRotation;
    GfxCoord* headPart;
    s32       partIndex;
    s32       pitchMagnitude;
    s32       yawMagnitude;
    s32       currentPitchMagnitude;
    s32       currentYawMagnitude;

    // Compose the complete five-part chain without the view transform.
    subjectRotationStorage                    = &subjectRotation;
    MATRIX_PAIR(&subjectRotation, 0, 0)       = ONE;
    MATRIX_PAIR(&subjectRotation, 0, 2)       = 0;
    MATRIX_PAIR(subjectRotationStorage, 1, 1) = ONE;
    MATRIX_PAIR(&subjectRotation, 2, 0)       = 0;
    subjectRotationStorage->m[2][2]           = ONE;
    headPosition.vx                           = 0;
    headPosition.vy                           = 0;
    headPosition.vz                           = 0;
    for (partIndex = 0; partIndex < ANIMATION_HEAD_PART_INDEX + 1; partIndex++) {
        headPart = &subject->extra.tmd->coords[partIndex];
        ApplyMatrixLV(&subjectRotation, (VECTOR*)headPart->coord.t, &transformedTranslation);
        headPosition.vx += transformedTranslation.vx;
        headPosition.vy += transformedTranslation.vy;
        headPosition.vz += transformedTranslation.vz;
        MulMatrix0(&subjectRotation, &headPart->coord, &subjectRotation);
    }
    targetPosition.vx = targetPointFrame->coord.t[0];
    targetPosition.vy = targetPointFrame->coord.t[1];
    targetPosition.vz = targetPointFrame->coord.t[2];
    // Truncate separation before expressing it in the subject rotation frame.
    targetDelta.vx = targetPosition.vx - headPosition.vx;
    targetDelta.vy = targetPosition.vy - headPosition.vy;
    targetDelta.vz = targetPosition.vz - headPosition.vz;
    TransposeMatrix(&subjectRotation, &inverseSubjectRotation);
    ApplyMatrix(&inverseSubjectRotation, &targetDelta, &headPosition);
    aimAngles.vx = -ratan2(headPosition.vy, headPosition.vz);
    aimAngles.vy = ratan2(headPosition.vx, headPosition.vz);
    aimAngles.vz = 0;
    headPart     = &subject->extra.tmd->coords[ANIMATION_HEAD_PART_INDEX];
    gfxExtractEulerAngles(&headPart->coord, &currentAngles);
    ANIMATION_BLEND_LIMITED_HEAD_ANGLES(aimAngles, currentAngles, maxYaw, maxPitch, blendWeight);

    headRotation                        = &headPart->coord;
    MATRIX_PAIR(&headPart->coord, 0, 0) = ONE;
    MATRIX_PAIR(headRotation, 0, 2)     = 0;
    MATRIX_PAIR(headRotation, 1, 1)     = ONE;
    MATRIX_PAIR(headRotation, 2, 0)     = 0;
    headRotation->m[2][2]               = ONE;
    RotMatrix(&aimAngles, headRotation);
}

void gfxExtractEulerAngles(const MATRIX* matrix, SVECTOR* angles)
{
    SVECTOR axis;
    SVECTOR rotatedAxis;
    MATRIX  rotation;
    s32     unitScale;
    s16     yzLength;

    rotation      = *matrix;
    unitScale     = ONE;
    rotation.t[2] = 0;
    rotation.t[1] = 0;
    rotation.t[0] = 0;
    // The transformed +Z axis determines pitch and yaw.
    axis.vx = 0;
    axis.vy = 0;
    axis.vz = unitScale;
    ApplyMatrixSV(&rotation, &axis, &rotatedAxis);
    angles->vx = -ratan2(rotatedAxis.vy, rotatedAxis.vz);
    yzLength   = SquareRoot12((rotatedAxis.vz * rotatedAxis.vz + rotatedAxis.vy * rotatedAxis.vy) >> GRAPHICS_FIXED_FRACTION_BITS);
    angles->vy = ratan2(rotatedAxis.vx, yzLength);
    // Cancel pitch and yaw from transformed +Y to recover roll.
    axis.vx = 0;
    axis.vy = unitScale;
    axis.vz = 0;
    ApplyMatrixSV(&rotation, &axis, &rotatedAxis);
    axis.vx = -angles->vx;
    axis.vy = -angles->vy;
    axis.vz = 0;
    RotMatrixZYX(&axis, &rotation);
    ApplyMatrixSV(&rotation, &rotatedAxis, &axis);
    angles->vz = -ratan2(axis.vx, axis.vy);
}

SVECTOR* gfxExtractSmallestEuler(SVECTOR* angles, const MATRIX* matrix)
{
    // Half a turn in the 4096-unit angle system: the other solution's X.
    enum { GRAPHICS_EULER_HALF_TURN = 0x800 };

    SVECTOR principal;
    SVECTOR alternate;
    s32     principalSin;
    s32     principalCos;
    s32     alternateSin;
    s32     alternateCos;

    // X cancels column 2's Y against its Z. Half a turn from that X is the
    // other solution of Rx(x) * Ry(y) * Rz(z).
    principal.vx = -ratan2(matrix->m[1][2], matrix->m[2][2]);
    alternate.vx = (principal.vx <= 0) ? principal.vx + GRAPHICS_EULER_HALF_TURN : principal.vx - GRAPHICS_EULER_HALF_TURN;

    principalSin = rsin(principal.vx);
    principalCos = rcos(principal.vx);
    alternateSin = rsin(alternate.vx);
    alternateCos = rcos(alternate.vx);

    // Y and Z are the residual rotation once that X is removed.
    principal.vy = ratan2(matrix->m[0][2], (matrix->m[2][2] * principalCos) / ONE - (matrix->m[1][2] * principalSin) / ONE);
    alternate.vy = ratan2(matrix->m[0][2], (matrix->m[2][2] * alternateCos) / ONE - (matrix->m[1][2] * alternateSin) / ONE);

    principal.vz = ratan2((matrix->m[1][0] * principalCos) / ONE + (matrix->m[2][0] * principalSin) / ONE,
                          (matrix->m[1][1] * principalCos) / ONE + (matrix->m[2][1] * principalSin) / ONE);
    alternate.vz = ratan2((matrix->m[1][0] * alternateCos) / ONE + (matrix->m[2][0] * alternateSin) / ONE,
                          (matrix->m[1][1] * alternateCos) / ONE + (matrix->m[2][1] * alternateSin) / ONE);

    // Nearer zero wins. A tie keeps the half-turn X. The sums reuse the
    // principal sine and cosine locals so the comparison keeps those registers.
    principalSin = ABS(principal.vx) + ABS(principal.vy) + ABS(principal.vz);
    principalCos = ABS(alternate.vx) + ABS(alternate.vy) + ABS(alternate.vz);
    if (principalSin < principalCos) {
        *angles = principal;
    } else {
        *angles = alternate;
    }
    return angles;
}

void gfxBlendOrthonormalRotation(const MATRIX* fromRotation, const MATRIX* toRotation, MATRIX* destination, s32 blendWeight)
{
    MATRIX selectedRows;
    MATRIX rowDeltas;
    VECTOR blendedRows[3];
    VECTOR rowCrossProduct;
    VECTOR normalizedCrossProduct;
    s32    rowIndex;
    s32    rebuildRow;
    s32    largestCrossLength;
    s32    crossLength;

    // Interpolate each row after the signed-halfword difference truncation.
    rebuildRow = 0;
    for (rowIndex = 0; rowIndex < ARRAY_SIZE(blendedRows); rowIndex++) {
        rowDeltas.m[rowIndex][0] = toRotation->m[rowIndex][0] - fromRotation->m[rowIndex][0];
        rowDeltas.m[rowIndex][1] = toRotation->m[rowIndex][1] - fromRotation->m[rowIndex][1];
        rowDeltas.m[rowIndex][2] = toRotation->m[rowIndex][2] - fromRotation->m[rowIndex][2];
    }
    for (rowIndex = 0; rowIndex < ARRAY_SIZE(blendedRows); rowIndex++) {
        blendedRows[rowIndex].vx = fromRotation->m[rowIndex][0] + (rowDeltas.m[rowIndex][0] * blendWeight) / ONE;
        blendedRows[rowIndex].vy = fromRotation->m[rowIndex][1] + (rowDeltas.m[rowIndex][1] * blendWeight) / ONE;
        blendedRows[rowIndex].vz = fromRotation->m[rowIndex][2] + (rowDeltas.m[rowIndex][2] * blendWeight) / ONE;
    }

    // Keep the most independent pair; a later equal length does not replace it.
    largestCrossLength = -1;

    crossLength = GRAPHICS_MEASURE_ROW_INDEPENDENCE(&blendedRows[0], &blendedRows[1], &rowCrossProduct, &normalizedCrossProduct);
    if (largestCrossLength < crossLength) {
        largestCrossLength = crossLength;
        rebuildRow         = 2;
    }

    crossLength = GRAPHICS_MEASURE_ROW_INDEPENDENCE(&blendedRows[1], &blendedRows[2], &rowCrossProduct, &normalizedCrossProduct);
    if (largestCrossLength < crossLength) {
        largestCrossLength = crossLength;
        // Interpolate each row after the signed-halfword difference truncation.
        rebuildRow = 0;
    }

    if (largestCrossLength < GRAPHICS_MEASURE_ROW_INDEPENDENCE(&blendedRows[0], &blendedRows[2], &rowCrossProduct, &normalizedCrossProduct)) {
        rebuildRow = 1;
    }

    // Rebuild the missing row and normalize the resulting basis.
    switch (rebuildRow) {
        case 0:
            selectedRows.m[1][0] = blendedRows[1].vx;
            selectedRows.m[1][1] = blendedRows[1].vy;
            selectedRows.m[1][2] = blendedRows[1].vz;
            selectedRows.m[2][0] = blendedRows[2].vx;
            selectedRows.m[2][1] = blendedRows[2].vy;
            selectedRows.m[2][2] = blendedRows[2].vz;
            MatrixNormal_1(&selectedRows, destination);
            break;
        case 1:
            selectedRows.m[0][0] = blendedRows[0].vx;
            selectedRows.m[0][1] = blendedRows[0].vy;
            selectedRows.m[0][2] = blendedRows[0].vz;
            selectedRows.m[2][0] = blendedRows[2].vx;
            selectedRows.m[2][1] = blendedRows[2].vy;
            selectedRows.m[2][2] = blendedRows[2].vz;
            MatrixNormal_2(&selectedRows, destination);
            break;
        case 2:
            selectedRows.m[0][0] = blendedRows[0].vx;
            selectedRows.m[0][1] = blendedRows[0].vy;
            selectedRows.m[0][2] = blendedRows[0].vz;
            selectedRows.m[1][0] = blendedRows[1].vx;
            selectedRows.m[1][1] = blendedRows[1].vy;
            selectedRows.m[1][2] = blendedRows[1].vz;
            MatrixNormal_0(&selectedRows, destination);
            break;
    }
}

void func_800B17D4(Task* arg0, Task* arg1, AnimationHeadAim* arg2)
{
    VECTOR    tmp;
    VECTOR    acc0;
    VECTOR    acc1;
    SVECTOR   delta;
    SVECTOR   ang;
    SVECTOR   euler;
    MATRIX    mtx0;
    MATRIX    mtx1;
    MATRIX    tmtx;
    VECTOR    probe;
    s32       rate;
    s32       lastPitchValid;
    s32       i;
    GfxCoord* rec;
    GfxCoord* rec1;
    s32       pitchLimit;
    s32       yawLimit;
    s32       curPitch;
    s32       curYaw;
    s32       newPitch;
    s32       newYaw;
    MATRIX*   m0;
    MATRIX*   m1;
    GfxCoord* base;
    MATRIX*   m;

    i              = 0;
    m0             = &mtx0;
    probe          = D_80093A28;
    yawLimit       = arg2->yawLimit;
    pitchLimit     = arg2->pitchLimit;
    rate           = arg2->rate;
    lastPitchValid = arg2->lastPitchValid;

    *(s32*)&mtx0             = ONE;
    MATRIX_PAIR(&mtx0, 0, 2) = 0;
    MATRIX_PAIR(m0, 1, 1)    = ONE;
    MATRIX_PAIR(&mtx0, 2, 0) = 0;
    m0->m[2][2]              = ONE;
    acc0.vx                  = 0;
    acc0.vy                  = 0;
    acc0.vz                  = 0;
    for (i = 0; i < 5; i++) {
        rec = &arg0->extra.tmd->coords[i];
        ApplyMatrixLV(&mtx0, (VECTOR*)rec->coord.t, &tmp);
        acc0.vx += tmp.vx;
        acc0.vy += tmp.vy;
        acc0.vz += tmp.vz;
        MulMatrix0(&mtx0, &rec->coord, &mtx0);
    }
    ApplyMatrixLV(&mtx0, &probe, &tmp);
    acc0.vx += tmp.vx;
    acc0.vy += tmp.vy;
    acc0.vz += tmp.vz;

    i                        = 0;
    m1                       = &mtx1;
    *(s32*)&mtx1             = ONE;
    MATRIX_PAIR(&mtx1, 0, 2) = 0;
    MATRIX_PAIR(m1, 1, 1)    = ONE;
    MATRIX_PAIR(&mtx1, 2, 0) = 0;
    m1->m[2][2]              = ONE;
    acc1.vx                  = 0;
    acc1.vy                  = 0;
    acc1.vz                  = 0;
    for (i = 0; i < 5; i++) {
        rec1 = &arg1->extra.tmd->coords[i];
        ApplyMatrixLV(&mtx1, (VECTOR*)rec1->coord.t, &tmp);
        acc1.vx += tmp.vx;
        acc1.vy += tmp.vy;
        acc1.vz += tmp.vz;
        MulMatrix0(&mtx1, &rec1->coord, &mtx1);
    }
    ApplyMatrixLV(&mtx1, &probe, &tmp);
    acc1.vx += tmp.vx;
    acc1.vy += tmp.vy;
    acc1.vz += tmp.vz;

    delta.vx = (u16)acc1.vx - (u16)acc0.vx;
    delta.vy = (u16)acc1.vy - (u16)acc0.vy;
    delta.vz = (u16)acc1.vz - (u16)acc0.vz;
    TransposeMatrix(&mtx0, &tmtx);
    ApplyMatrix(&tmtx, &delta, &acc0);

    ang.vx = ratan2(-acc0.vy, acc0.vz);
    ang.vy = ratan2(acc0.vx, acc0.vz);
    ang.vz = 0;
    if (delta.vy < 0) {
        if (ang.vx < -0x400) {
            ang.vx = (u16)ang.vx + 0x1000;
        }
    } else if (ang.vx >= 0x400) {
        ang.vx = (u16)ang.vx - 0x1000;
    }

    if (lastPitchValid != 0) {
        if (ABS(ang.vx - arg2->lastPitch) > 0x800) {
            while (ang.vx >= 0x800) {
                ang.vx -= 0x1000;
            }
            while (ang.vx < -0x800) {
                ang.vx += 0x1000;
            }
        }
    } else {
        arg2->lastPitchValid = 1;
    }
    arg2->lastPitch = ang.vx;

    base = arg0->extra.tmd->coords;
    rec  = base + 4;
    gfxExtractSmallestEuler(&euler, &base[4].coord);

    ang.vx   = euler.vx + (ang.vx - euler.vx) * rate / 4096;
    ang.vy   = euler.vy + (ang.vy - euler.vy) * rate / 4096;
    ang.vz   = euler.vz;
    curPitch = euler.vx >= 0 ? euler.vx : -euler.vx;
    if (pitchLimit < curPitch) {
        pitchLimit = curPitch;
    }
    curYaw = euler.vy >= 0 ? euler.vy : -euler.vy;
    if (yawLimit < curYaw) {
        yawLimit = curYaw;
    }
    newPitch = ang.vx >= 0 ? ang.vx : -ang.vx;
    if (pitchLimit < newPitch) {
        ang.vx = ang.vx < 0 ? -pitchLimit : pitchLimit;
    }
    newYaw = ang.vy >= 0 ? ang.vy : -ang.vy;
    if (yawLimit < newYaw) {
        ang.vy = ang.vy < 0 ? -yawLimit : yawLimit;
    }

    m                    = &rec->coord;
    *(s32*)&rec->coord   = ONE;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = ONE;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = ONE;
    RotMatrix(&ang, m);
    rec->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Appends a node's local rotation to the parent rotation already loaded in the GTE.
///
/// Both matrices use Q12 rotation and must be halfword-aligned. Multiplies
/// parent * local, loading and writing one column at a time; the destination
/// may be the node's own rotation. Translation and padding are untouched.
/// Requires the complete parent rotation already loaded in the GTE and
/// changes its vector/arithmetic registers without replacing that rotation.
static inline void _gfxComposeNodeRotation(const GfxCoord* node, MATRIX* worldRotation)
{
    gte_ldclmv(&node->coord.m[0][0]);
    gte_rtir();
    gte_stclmv(&worldRotation->m[0][0]);
    gte_ldclmv(&node->coord.m[0][1]);
    gte_rtir();
    gte_stclmv(&worldRotation->m[0][1]);
    gte_ldclmv(&node->coord.m[0][2]);
    gte_rtir();
    gte_stclmv(&worldRotation->m[0][2]);
}

void gfxComposeNodeWorldTransform(const GfxCoord* node, MATRIX* worldRotation, SVECTOR* worldTranslation)
{
    SVECTOR localTranslation;
    s32     unitScale;

    if (node->parent != &gGfxViewCoord) {
        gfxComposeNodeWorldTransform(node->parent, worldRotation, worldTranslation);
    } else {
        unitScale                        = ONE;
        MATRIX_PAIR(worldRotation, 0, 0) = unitScale;
        MATRIX_PAIR(worldRotation, 0, 2) = 0;
        MATRIX_PAIR(worldRotation, 1, 1) = unitScale;
        MATRIX_PAIR(worldRotation, 2, 0) = 0;
        worldRotation->m[2][2]           = unitScale;
        worldTranslation->vx             = 0;
        worldTranslation->vy             = 0;
        worldTranslation->vz             = 0;
    }

    // Each hierarchy level truncates translations to signed halfwords.
    localTranslation.vx = (u16)node->coord.t[0];
    localTranslation.vy = (u16)node->coord.t[1];
    localTranslation.vz = (u16)node->coord.t[2];
    gte_SetRotMatrix(worldRotation);
    gte_ldv0(&localTranslation);
    gte_rtv0();
    gte_stsv(&localTranslation);
    worldTranslation->vx += localTranslation.vx;
    worldTranslation->vy += localTranslation.vy;
    worldTranslation->vz += localTranslation.vz;
    _gfxComposeNodeRotation(node, worldRotation);
}

/// Emits a screen-pulse tile and its add/subtract draw mode at tag 0.
///
/// `task->spawnArg2.value == SCREEN_FADE_SUBTRACT` selects subtraction;
/// every other value selects addition. `intensity` is a byte RGB shade.
/// Requires room for a TILE and DR_TPAGE in the current frame arena, and a
/// writable ordering-table head. Packets stay live until the frame is drawn.
/// Vertical screen shake offsets the tile; no task state is changed.
static inline void _fadeDrawPulse(Task* task, u8 intensity)
{
    TILE*     tile;
    DR_TPAGE* blendCommand;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setlen(tile, sizeof(*tile) / sizeof(u32) - 1);
    setcode(tile, GPU_SCREEN_SEMITRANSPARENT_TILE);
    setXY0(tile, -DISPLAY_EFFECT_WIDTH / 2, -DISPLAY_EFFECT_HEIGHT / 2);
    tile->y0 -= gDisplayState.vramYOffset;
    tile->b0  = intensity;
    tile->g0  = intensity;
    tile->r0  = intensity;
    setWH(tile, DISPLAY_EFFECT_WIDTH, DISPLAY_EFFECT_HEIGHT);

    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    if (task->spawnArg2.value == SCREEN_FADE_SUBTRACT) {
        setlen(blendCommand, sizeof(*blendCommand) / sizeof(u32) - 1);
        blendCommand->code[0] = GPU_SCREEN_DRAW_MODE_SUBTRACT;
    } else {
        setlen(blendCommand, sizeof(*blendCommand) / sizeof(u32) - 1);
        blendCommand->code[0] = GPU_SCREEN_DRAW_MODE_ADD;
    }
    addPrim(gGpuCurrentOt, tile);
    addPrim(gGpuCurrentOt, blendCommand);
}

/// Draws one frame of a timed full-screen colour pulse.
///
/// `spawnArg1.value` is the remaining peak hold in frames. While it is positive,
/// `killCountdown` falls toward zero to raise the intensity; after the hold it
/// rises to 31 to end the pulse. Intensity truncates to a byte after complementing
/// the counter shifted by three. A zero `spawnArg2.value` subtracts toward black;
/// any other value adds toward white. The tile compensates vertical screen shake.
/// Packets borrow the current frame arena and are linked at ordering-table tag 0.
static void _fadeTickPulse(Task* task)
{
    u8 intensity;

    if (task->spawnArg1.value > 0) {
        if (task->killCountdown > 0) {
            task->killCountdown--;
            intensity = ~(task->killCountdown << FADE_PULSE_INTENSITY_SHIFT);
        } else {
            task->spawnArg1.value--;
            intensity = 0xFF;
        }
    } else {
        task->killCountdown++;
        intensity = ~(task->killCountdown << FADE_PULSE_INTENSITY_SHIFT);
        if (task->killCountdown >= FADE_PULSE_LAST_STEP) {
            task->state++;
        }
    }

    _fadeDrawPulse(task, intensity);
}

/// Expands RGB555 channels into the short-vector scale used by the Q12 GTE blend.
///
/// Writes only r/g/b as unsigned 5-bit values shifted left seven (0..3968).
/// Packed bit 15 is ignored and the scratch slot's fourth halfword is untouched.
/// The caller supplies live writable channel storage; no GTE state is changed.
static inline void _gpuUnpackRgb555(u16 color, _Rgb555Scratch* channels)
{
    channels->b = color;
    channels->g = color;
    channels->r = (color & GPU_RGB555_CHANNEL_MASK) << GPU_RGB555_GTE_CHANNEL_SHIFT;
    channels->g = (channels->g << 2) & GPU_RGB555_GTE_CHANNEL_MASK;
    channels->b = (channels->b >> 3) & GPU_RGB555_GTE_CHANNEL_MASK;
}

/// Interpolates one RGB555 colour and preserves either source's STP bit.
///
/// `firstWeight` is in 1/4096 units (0 selects `second`, `ONE` selects `first`).
/// Both source halfwords must remain live and distinct from `destination`.
/// Uses and releases three scratch-stack short vectors; no pointer is retained.
static void _gpuBlendRgb555(const u16* first, const u16* second, s32 firstWeight, u16* destination)
{
    u8*             scratchEnd;
    _Rgb555Scratch* firstColor;
    _Rgb555Scratch* secondColor;
    _Rgb555Scratch* blendedColor;
    u16             packed;

    // Three slots below the saved cursor: first source, second source, result.
    scratchEnd                           = SCRATCH_STACK_CURSOR(u8);
    firstColor                           = (_Rgb555Scratch*)(scratchEnd - 3 * sizeof(_Rgb555Scratch));
    SCRATCH_STACK_CURSOR(_Rgb555Scratch) = firstColor;

    // Place each 5-bit channel in bits 7..11.
    _gpuUnpackRgb555(*first, firstColor);

    secondColor = (_Rgb555Scratch*)(scratchEnd - 2 * sizeof(_Rgb555Scratch));
    _gpuUnpackRgb555(*second, secondColor);

    // Weight the first colour by the factor, then add the second by its complement.
    gte_LoadAverageShort12(firstColor, secondColor, firstWeight, ONE - firstWeight,
                           blendedColor = (_Rgb555Scratch*)(scratchEnd - sizeof(_Rgb555Scratch)));

    // Stage blue in bits 5..9, then shift green and blue into place and insert red.
    packed       = ((blendedColor->b >> 2) & 0x3E0) | ((blendedColor->g >> GPU_RGB555_GTE_CHANNEL_SHIFT) & GPU_RGB555_CHANNEL_MASK);
    packed       = (packed << 5) | ((blendedColor->r >> GPU_RGB555_GTE_CHANNEL_SHIFT) & GPU_RGB555_CHANNEL_MASK);
    *destination = packed;
    // Bit 15 is semi-transparency. Either source sets it on the packed result.
    if ((s16)*first < 0 || (s16)*second < 0) {
        *destination = packed | GPU_RGB555_STP_MASK;
    }
    SCRATCH_STACK_RELEASE_BYTES(3 * sizeof(_Rgb555Scratch));
}

/// Links a full-screen fade's blend command and tile into one ordering-table tag.
///
/// `otTagOffset` borrows a signed word, live and unchanged during this call.
/// Its value counts four-byte tags from `gGpuCurrentOt`. Nonzero offsets must
/// address a live tag in that table. Zero selects tag 0 at either
/// `Gpu_OrderingTables` root; otherwise it selects tag -10, requiring ten live
/// tags before the current base. The normal frame base has 32 reserved tags.
///
/// Both packets must be initialized, distinct, word-aligned and writable, and
/// must remain live until this frame is drawn. Only their DMA links and the
/// selected tag's link change. Prepending the command last makes it execute
/// before the tile; neither packet nor the borrowed table is allocated or freed.
static inline void _fadeLinkScreenPackets(const s32* otTagOffset, TILE* tile, DR_TPAGE* blendCommand)
{
    enum {
        FADE_SCREEN_AUTO_OT_OFFSET      = 0,
        FADE_SCREEN_FOREGROUND_OT_INDEX = -10
    };

    if (*otTagOffset != FADE_SCREEN_AUTO_OT_OFFSET) {
        u_long* orderingTable;

        orderingTable = gGpuCurrentOt;
        addPrim(&orderingTable[*otTagOffset], tile);
        addPrim(&orderingTable[*otTagOffset], blendCommand);
    } else {
        u_long* orderingTable;

        orderingTable = gGpuCurrentOt;
        // Presentation starts at tag zero; the frame depth base follows its foreground tags.
        if ((orderingTable == (u_long*)Gpu_OrderingTables[0].org) || (orderingTable == (u_long*)Gpu_OrderingTables[1].org)) {
            addPrim(orderingTable, tile);
            addPrim(orderingTable, blendCommand);
        } else {
            addPrim(&orderingTable[FADE_SCREEN_FOREGROUND_OT_INDEX], tile);
            addPrim(&orderingTable[FADE_SCREEN_FOREGROUND_OT_INDEX], blendCommand);
        }
    }
}

void fadeScreenTask(Task* task)
{
    enum {
        FADE_SCREEN_STATE_START             = 0,
        FADE_SCREEN_STATE_RAMP_UP           = 1,
        FADE_SCREEN_STATE_HOLD              = 2,
        FADE_SCREEN_STATE_RAMP_DOWN         = 3,
        FADE_SCREEN_INTENSITY_FRACTION_BITS = 4,
        FADE_SCREEN_PEAK_INTENSITY          = 255 << FADE_SCREEN_INTENSITY_FRACTION_BITS
    };

    ScreenFade* fade;
    TILE*       tile;
    DR_TPAGE*   blendCommand;
    s32         intensity;
    s16         topY;
    s8          shakeY;

    fade = task->spawnArg2.pointer;

    if (task->state == FADE_SCREEN_STATE_START) {
        task->killCountdown = 0;
        if (fade->rampFrames <= 0) {
            fade->rampFrames = SCREEN_FADE_DEFAULT_FRAMES;
        }
        task->state++;
    }
    // A new return length starts at full coverage, even if it differs from ramp-up.
    if ((task->state == FADE_SCREEN_STATE_HOLD) && (fade->phase == SCREEN_FADE_RETURN)) {
        task->killCountdown = fade->rampFrames;
    }

    // Emit this frame's coverage before advancing the ramp counter.
    intensity      = (task->killCountdown * FADE_SCREEN_PEAK_INTENSITY) / fade->rampFrames;
    tile           = gGpuPrimCursor;
    topY           = -DISPLAY_EFFECT_HEIGHT / 2;
    tile->y0       = topY;
    gGpuPrimCursor = tile + 1;
    setlen(tile, sizeof(*tile) / sizeof(u32) - 1);
    setcode(tile, GPU_SCREEN_SEMITRANSPARENT_TILE);
    tile->x0     = -DISPLAY_EFFECT_WIDTH / 2;
    shakeY       = gDisplayState.vramYOffset;
    tile->w      = DISPLAY_EFFECT_WIDTH;
    tile->h      = DISPLAY_EFFECT_HEIGHT;
    intensity    = intensity >> FADE_SCREEN_INTENSITY_FRACTION_BITS;
    tile->b0     = intensity;
    tile->g0     = intensity;
    tile->r0     = intensity;
    blendCommand = gGpuPrimCursor;
    tile->y0     = topY - shakeY;

    gGpuPrimCursor = blendCommand + 1;
    if (fade->blend == SCREEN_FADE_SUBTRACT) {
        setlen(blendCommand, sizeof(*blendCommand) / sizeof(u32) - 1);
        blendCommand->code[0] = GPU_SCREEN_DRAW_MODE_SUBTRACT;
    } else {
        setlen(blendCommand, sizeof(*blendCommand) / sizeof(u32) - 1);
        blendCommand->code[0] = GPU_SCREEN_DRAW_MODE_ADD;
    }

    _fadeLinkScreenPackets(&task->spawnArg1.value, tile, blendCommand);

    switch (task->state) {
        case FADE_SCREEN_STATE_RAMP_UP:
            task->killCountdown++;
            if (task->killCountdown == fade->rampFrames) {
                task->state++;
            }
            break;
        case FADE_SCREEN_STATE_HOLD:
            if (fade->phase == SCREEN_FADE_RETURN) {
                task->state = FADE_SCREEN_STATE_RAMP_DOWN;
            }
            break;
        case FADE_SCREEN_STATE_RAMP_DOWN:
            task->killCountdown--;
            if (task->killCountdown <= 0) {
                fade->phase = SCREEN_FADE_DONE;
                taskKill(task);
            }
            break;
        default:
            taskKill(task);
            break;
    }
}

void func_800B25B0(void)
{
    switch (GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) {
        case GAME_LOCATION_KEY(5, 27, 0, 0):
            taskSpawnFromTable(&D_neo_ark_bridge_80181F18, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 15, 0, 0):
            taskSpawnFromTable(&D_neo_ark_garden_80181398, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 14, 0, 0):
            taskSpawnFromTable(&D_neo_ark_island_80181B30, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 13, 0, 0):
            taskSpawnFromTable(&D_neo_ark_pavilion_8018384C, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 12, 0, 0):
            taskSpawnFromTable(&D_neo_ark_submarine_tunnel_801810E4, 1, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 7, 0, 0):
            taskSpawnFromTable(D_neo_ark_observatory_80180DBC, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(2, 30, 0, 0):
            taskSpawnFromTable(D_dryfield_motel_room_6_80182D0C, 0, 1, 0);
            break;
        case GAME_LOCATION_KEY(3, 30, 0, 0):
            taskSpawnFromTable(D_dryfield_night_motel_room_6_80182E74, 0, 1, 0);
            break;
        case GAME_LOCATION_KEY(4, 18, 0, 0):
            taskSpawnFromTable(&D_shelter_b1_control_room_80181B88, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 31, 0, 0):
            taskSpawnFromTable(&D_neo_ark_r31_8017D9E8, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 30, 0, 0):
            taskSpawnFromTable(&D_neo_ark_submarine_gallery_8018186C, 0, 0, 0);
            taskSpawnFromTable(&D_neo_ark_submarine_gallery_8018186C, 1, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 29, 0, 0):
            taskSpawnFromTable(&D_neo_ark_woodland_path_80181638, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(4, 22, 0, 0):
            taskSpawnFromTable(&D_actor_361100_801637C8, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(4, 48, 0, 0):
            taskSpawnFromTable(&D_shelter_r48_80182FAC, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(1, 20, 0, 0):
            func_mist_shooting_gallery_8017FBD8();
            break;
    }
}

void gpuBlendRgb555ClutRow(const u16* first, const u16* second, s32 firstWeight, u16* destination)
{
    s32 entryIndex;

    for (entryIndex = 0; entryIndex < GPU_RGB555_CLUT_ROW_COLORS; entryIndex++) {
        _gpuBlendRgb555(first, second, firstWeight, destination);
        first++;
        second++;
        destination++;
    }
}

/// Interpolates the selected entries of a sixteen-colour RGB555 CLUT row.
///
/// Bit i of `entryMask` selects entry i; bits above 15 are ignored and other
/// destination entries remain unchanged. `firstWeight` uses 1/4096 units,
/// normally 0..`ONE`, with second weight `ONE - firstWeight`. Selected outputs keep
/// bit 15 when either source has it. Both source rows must contain sixteen live
/// halfwords and the destination sixteen writable halfwords, without overlap
/// with either source. Requires initialized scratch and GTE state, as does the
/// per-colour blend; no pointers are retained.
static void _gpuBlendRgb555ClutRowMasked(const u16* first, const u16* second, s32 firstWeight, u16* destination, s32 entryMask)
{
    s32 entryIndex;

    for (entryIndex = 0; entryIndex < GPU_RGB555_CLUT_ROW_COLORS; entryIndex++) {
        if ((1 << entryIndex) & entryMask) {
            _gpuBlendRgb555(first, second, firstWeight, destination);
        }
        first++;
        second++;
        destination++;
    }
}

/// Starts the fixed 32-step screen pulse and draws its first frame immediately.
///
/// The task's first spawn word is the peak hold in frames; its second selects
/// subtract (zero) or add (nonzero). Nonpositive holds retain the immediate
/// return-path behavior of `_fadeTickPulse`. The next state ticks the pulse.
static void _fadeStartPulse(Task* task)
{
    task->killCountdown = FADE_PULSE_STEPS;
    task->state++;
    _fadeTickPulse(task);
}

void fadePulseTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = D_80093A38;

    stateHandlers.funcs[task->state](task);
}

Task* func_800B2968(void)
{
    return taskSpawnFromTable(&D_replay_bonus_80119218, 0, 0, 0);
}

/// Blends one part's decoded rotations into its local matrix or an unpacked pose.
///
/// `scratch` supplies both XYZ Euler endpoints in 4096 units per turn and their
/// complementary 12-fractional-bit weights (0..ONE). Bank-only endpoints blend
/// their components directly, without wrapping the angles. Buffered endpoints
/// instead scale the slot's cached next * inverse(current) Euler rotation, then
/// compose it with the current rotation. Refreshing that cache requires
/// `request->refreshRotationDelta` to equal 1; otherwise it must already describe
/// this transition.
///
/// A null unpacked destination updates `coord->coord.m` and marks its composition
/// stale; a non-null destination receives the rotation instead. Translation and
/// parent links are preserved. `scratch->nextRotation` holds the blended Euler
/// result whenever an unpacked or encoded destination is non-null. With only a
/// matrix output on the buffered path, it retains the scaled relative rotation.
/// The caller encodes any requested compact output after this call.
/// Unpacked output copies the full `SVECTOR`, including its untouched pad.
///
/// All objects are borrowed for the call. The caller owns the live scratch-stack
/// block and leaves room for matrix-to-Euler conversion's nested reservation;
/// this helper neither reserves nor releases it. GTE registers are clobbered.
static void _animationBlendRotation(const _AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot,
                                    _AnimationBlendScratch* scratch)
{
    /// Rebuilds the cached relative rotation on entry to buffered endpoints.
    ///
    /// The request byte must equal this value exactly; bank-only blends ignore it.
    enum { ANIMATION_ROTATION_DELTA_REFRESH = 1 };

    /// Caches the relative Euler rotation for a buffered pose transition.
    ///
    /// `scratch` is a live, word-aligned `_AnimationBlendScratch*` supplying
    /// `currentMatrix`'s starting rotation and `nextRotation`'s destination XYZ
    /// Euler angles in 4096 units per turn. The product next * transpose(current),
    /// using Q12 matrix elements, is decomposed into the separate writable
    /// `SVECTOR* rotationDelta` in the same angle units for later weighted blending.
    /// Its `pad` is preserved. Overwrites `nextMatrix.m` and `deltaMatrix.m`;
    /// input rotations and matrix translations are preserved.
    ///
    /// `scratch` is evaluated repeatedly and must be a stable expression without
    /// side effects. `rotationDelta` is evaluated once, after the matrix operations.
    /// Captures no caller locals. The initialized scratch stack must have room
    /// for `gfxMatrixToEuler`'s temporary reservation. GTE registers are clobbered.
#define ANIMATION_CACHE_ROTATION_DELTA(rotationDelta, scratch)                                    \
    do {                                                                                          \
        RotMatrix_gte(&(scratch)->nextRotation, &(scratch)->nextMatrix);                          \
        TransposeMatrix(&(scratch)->currentMatrix, &(scratch)->deltaMatrix);                      \
        gte_MulMatrix0(&(scratch)->nextMatrix, &(scratch)->deltaMatrix, &(scratch)->deltaMatrix); \
        gfxMatrixToEuler(&(scratch)->deltaMatrix, (rotationDelta));                               \
    } while (0)

    if (slot->usesBufferedPose != 0) {
        // Scale the buffered transition's relative rotation, then compose with its start.
        RotMatrix_gte(&scratch->currentRotation, &scratch->currentMatrix);
        if (request->refreshRotationDelta == ANIMATION_ROTATION_DELTA_REFRESH) {
            ANIMATION_CACHE_ROTATION_DELTA(&slot->bufferedRotationDelta, scratch);
        }
#undef ANIMATION_CACHE_ROTATION_DELTA
        gte_lddp(scratch->nextWeight);
        gte_ldsv(&slot->bufferedRotationDelta);
        gte_gpf12();
        gte_stsv(&scratch->nextRotation);
        RotMatrix_gte(&scratch->nextRotation, &scratch->deltaMatrix);
        if (request->unpackedDestination == NULL) {
            gte_MulMatrix0(&scratch->deltaMatrix, &scratch->currentMatrix, &coord->coord);
            if (request->encodedDestination.address != NULL) {
                gfxMatrixToEuler(&coord->coord, &scratch->nextRotation);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        } else {
            gte_MulMatrix0(&scratch->deltaMatrix, &scratch->currentMatrix, &scratch->deltaMatrix);
            gfxMatrixToEuler(&scratch->deltaMatrix, &scratch->nextRotation);
            request->unpackedDestination->rotation = scratch->nextRotation;
        }
    } else {
        // Blend bank keyframes directly in Euler space, preserving their angle representation.
        gte_LoadAverageShort12(&scratch->currentRotation, &scratch->nextRotation,
                               scratch->currentWeight, scratch->nextWeight, &scratch->nextRotation);
        if (request->unpackedDestination == NULL) {
            RotMatrix_gte(&scratch->nextRotation, &coord->coord);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        } else {
            request->unpackedDestination->rotation = scratch->nextRotation;
        }
    }
}

/// Initializes complementary pose-endpoint weights in 1/4096 units.
///
/// Distinct endpoint addresses use `(timeLeft << ANIMATION_BLEND_FRACTION_BITS)
/// / timeSpan` for the current weight and `ONE` minus that for the next weight.
/// The signed division truncates toward zero. The caller supplies nonzero
/// `timeSpan` and remaining time in 0..`timeSpan`, both in sixteenths of a frame;
/// this helper does not normalize or clamp them. Equal endpoint addresses use
/// weights 0 and `ONE` without reading slot timing or dereferencing either pose.
///
/// The request and slot are borrowed read-only. The caller owns the live,
/// word-aligned workspace; only `currentWeight` and `nextWeight` are written.
/// The scaled remaining time is stored before its normalized weight replaces
/// it. This helper neither reserves nor releases scratch space and uses no GTE
/// registers.
static inline void _animationSetBlendWeights(const _AnimationBlendRequest* request, const AnimationSlot* slot,
                                             _AnimationBlendScratch* scratch)
{
    if (request->currentPose.bytes != request->nextPose.bytes) {
        scratch->currentWeight  = slot->timeLeft << ANIMATION_BLEND_FRACTION_BITS;
        scratch->currentWeight /= slot->timeSpan;
        scratch->nextWeight     = ONE - scratch->currentWeight;
    } else {
        scratch->currentWeight = 0;
        scratch->nextWeight    = ONE;
    }
}

/// Blends one model part's encoding-1 translation and Euler rotation.
///
/// Both borrowed endpoints must address a complete, word-aligned
/// `AnimationPackedPose`: signed translation in model integer units and
/// Euler angles in 4096 units per turn. For a normalized segment, `timeLeft`
/// is in 0..`timeSpan`, in sixteenths of a frame; its fraction of `timeSpan`
/// weights the current endpoint in Q12. Identical endpoint addresses select
/// the next endpoint with full weight. Zero `timeSpan` returns without
/// reading either pose, reserving scratch space or writing any output.
///
/// A null unpacked destination updates `coord`'s local translation and rotation
/// and marks its composition stale. Otherwise the unpacked destination receives
/// both components and `coord` is unused. Translation writes preserve that
/// destination's pad; rotation copies the full `SVECTOR`, including the scratch
/// vector's untouched pad. An unpacked destination must be separate from the
/// encoded pose storage. `_animationBlendRotation` blends bank angles directly
/// or composes the slot's cached relative rotation for buffered endpoints,
/// refreshing it when requested on entry to a buffered transition.
///
/// The independent optional encoded destination must hold a writable,
/// word-aligned 12-byte pose. It may alias either endpoint: both translations
/// and rotations are decoded before the six output halfwords are written.
/// All objects are borrowed for this call. The initialized scratch stack must
/// fit one `_AnimationBlendScratch` and nested matrix conversion workspace;
/// the reservation is released before returning. GTE registers are clobbered.
static void _animationBlendTranslationRotation(const _AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot)
{
    _AnimationBlendScratch*    scratch;
    const AnimationPackedPose* encodedPose;

    if (slot->timeSpan != 0) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(_AnimationBlendScratch);
        _animationSetBlendWeights(request, slot, scratch);
        // The compact translation occupies six bytes; the GTE load reads three halfwords.
        gte_LoadAverageShort12(request->currentPose.translationRotation, request->nextPose.translationRotation,
                               scratch->currentWeight, scratch->nextWeight, &scratch->translation);
        if (request->unpackedDestination == NULL) {
            coord->coord.t[0]   = scratch->translation.vx;
            coord->coord.t[1]   = scratch->translation.vy;
            coord->coord.t[2]   = scratch->translation.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        } else {
            request->unpackedDestination->translation.vx = scratch->translation.vx;
            request->unpackedDestination->translation.vy = scratch->translation.vy;
            request->unpackedDestination->translation.vz = scratch->translation.vz;
        }
        // Decode full-resolution angles before the shared rotation blend.
        encodedPose                 = request->currentPose.translationRotation;
        scratch->currentRotation.vx = encodedPose->rotationX;
        scratch->currentRotation.vy = encodedPose->rotationY;
        scratch->currentRotation.vz = encodedPose->rotationZ;
        encodedPose                 = request->nextPose.translationRotation;
        scratch->nextRotation.vx    = encodedPose->rotationX;
        scratch->nextRotation.vy    = encodedPose->rotationY;
        scratch->nextRotation.vz    = encodedPose->rotationZ;
        _animationBlendRotation(request, coord, slot, scratch);
        // Write the compact result only after consuming both endpoints, allowing in-place output.
        encodedPose = request->encodedDestination.translationRotation;
        if (encodedPose != NULL) {
            AnimationPackedPose* destinationPose;

            destinationPose               = request->encodedDestination.translationRotation;
            destinationPose->translationX = scratch->translation.vx;
            destinationPose->translationY = scratch->translation.vy;
            destinationPose->translationZ = scratch->translation.vz;
            destinationPose->rotationX    = scratch->nextRotation.vx;
            destinationPose->rotationY    = scratch->nextRotation.vy;
            destinationPose->rotationZ    = scratch->nextRotation.vz;
        }
        SCRATCH_STACK_RELEASE_BLOCK(_AnimationBlendScratch);
    }
}

/// Blends a model part's encoding-4 rotation while preserving its local translation.
///
/// Both borrowed endpoints must address a complete, word-aligned
/// `AnimationPackedRotation`. Signed 11/10/11-bit components expand from
/// eight-unit steps to Euler angles in 4096 units per turn. For a normalized
/// segment, `timeLeft` is in 0..`timeSpan`, in sixteenths of a frame; the
/// remaining-time fraction weights the current endpoint. Identical endpoint
/// addresses select the next endpoint with full weight. Zero `timeSpan`
/// returns without decoding, reserving scratch space or writing any output.
///
/// `_animationBlendRotation` applies the result to `coord` when the unpacked
/// destination is NULL, or writes that destination's rotation instead. Buffered
/// endpoints use the slot's relative-rotation cache and its refresh request.
/// Unpacked output copies the full rotation `SVECTOR`, including the scratch
/// vector's untouched pad. `coord` is required only for coordinate output.
/// The optional encoded destination must be a readable, writable, word-aligned
/// four-byte word. It may alias either endpoint: both are decoded before the
/// result is packed, discarding the low three angle bits. Translation is
/// preserved in either output mode.
///
/// The request and its pointers are borrowed for this call. The initialized
/// scratch stack must fit one `_AnimationBlendScratch` and any nested matrix
/// conversion workspace. Its reservation is released before returning; GTE
/// registers are clobbered.
static void _animationBlendPackedRotation(const _AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot)
{
    /// Low angle bits omitted from encoding-4 packed rotations.
    ///
    /// All three signed components use 512 steps per turn. Shifting left by
    /// this count converts them to 4096-unit Euler angles for blending. Shifting
    /// the result right discards its low bits, rounding negative values down on
    /// this compiler, before the 11/10/11-bit fields retain their own widths.
    enum { ANIMATION_PACKED_ANGLE_SHIFT = 3 };

    _AnimationBlendScratch*        scratch;
    const AnimationPackedRotation* sourcePose;
    AnimationPackedRotation*       destinationPose;

    if (slot->timeSpan != 0) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(_AnimationBlendScratch);
        _animationSetBlendWeights(request, slot, scratch);
        // Expand the 11/10/11-bit angles to the same units as encoding 1.
        sourcePose                  = request->currentPose.packedRotation;
        scratch->currentRotation.vx = sourcePose->rx << ANIMATION_PACKED_ANGLE_SHIFT;
        scratch->currentRotation.vy = sourcePose->ry << ANIMATION_PACKED_ANGLE_SHIFT;
        scratch->currentRotation.vz = sourcePose->rz << ANIMATION_PACKED_ANGLE_SHIFT;
        sourcePose                  = request->nextPose.packedRotation;
        scratch->nextRotation.vx    = sourcePose->rx << ANIMATION_PACKED_ANGLE_SHIFT;
        scratch->nextRotation.vy    = sourcePose->ry << ANIMATION_PACKED_ANGLE_SHIFT;
        scratch->nextRotation.vz    = sourcePose->rz << ANIMATION_PACKED_ANGLE_SHIFT;
        _animationBlendRotation(request, coord, slot, scratch);
        destinationPose = request->encodedDestination.packedRotation;
        // Arithmetic shifts discard the low three bits; the fields retain their widths.
        if (destinationPose != NULL) {
            destinationPose->rx = scratch->nextRotation.vx >> ANIMATION_PACKED_ANGLE_SHIFT;
            destinationPose->ry = scratch->nextRotation.vy >> ANIMATION_PACKED_ANGLE_SHIFT;
            destinationPose->rz = scratch->nextRotation.vz >> ANIMATION_PACKED_ANGLE_SHIFT;
        }
        SCRATCH_STACK_RELEASE_BLOCK(_AnimationBlendScratch);
    }
}

static void Gp_AnimAdvanceSlot(AnimationContext* context, s32 arg1)
{
    AnimationSlot*         slot;
    AnimationSet**         sets;
    const AnimationRecord* recs;
    const AnimationRecord* rec;
    u16                    recordIndex;
    s32                    setIndex;
    u16                    segmentTime;

    slot                      = &context->slots[arg1];
    slot->status.fields.flags = 0;
    if (slot->currentPose.key != slot->nextPose.key) {
        sets = slot->sets;
        do {
            slot->currentPose.key = slot->nextPose.key;
            recordIndex           = slot->nextPose.indices.recordIndex + 1;
            setIndex              = slot->nextPose.indices.setIndex;
            recs                  = sets[setIndex]->records;
            while ((s8)recs[recordIndex].flags < 0) {
                // Negated-index subtraction preserves the address-add operand order.
                rec = recs - -(s32)recordIndex;
                if (rec->flags < ANIMATION_RECORD_END_THRESHOLD) {
                    recordIndex = rec->wordOffset;
                    if (recordIndex == slot->nextPose.indices.recordIndex) {
                        slot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
                    }
                    slot->status.fields.flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
                } else {
                    recordIndex                = slot->nextPose.indices.recordIndex;
                    slot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
                    break;
                }
            }
            slot->nextPose.indices.recordIndex = recordIndex;
            slot->nextPose.indices.setIndex    = setIndex;
            if (slot->status.fields.flags & ANIMATION_SLOT_BOUNDARY_MASK) {
                break;
            }
        } while (slot->currentPose.key != slot->nextPose.key);
    }

    segmentTime    = slot->sets[slot->nextPose.indices.setIndex]->records[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan = segmentTime;
    slot->timeLeft = segmentTime;
    animationTickSlotPose(context, arg1, 0, 0);
}

void animationTickSlotPose(AnimationContext* context, s32 slotIndex, AnimationPose* unpackedDestination, void* encodedDestination)
{
    /// Leaves the buffered transition's cached relative rotation unchanged.
    ///
    /// In `_AnimationBlendRequest.refreshRotationDelta`, zero reuses the slot's
    /// `bufferedRotationDelta` when either endpoint is buffered; that cache must
    /// already describe the transition. Bank-only blends ignore the request.
    /// A tick requests reuse unless it enters buffered endpoints from a cleared
    /// `usesBufferedPose` flag, when it requests a refresh with value 1 instead.
    enum { ANIMATION_ROTATION_DELTA_REUSE = 0 };

    /// Unsupported track encoding that reports an error instead of producing a pose.
    ///
    /// The initial keyframe's low flags nibble supplies this selector. Playback
    /// still processes timing and resolves both endpoints before reporting the
    /// error; it neither decodes pose bytes nor writes the model coordinate or
    /// either destination. Bank endpoints select bank 2 and use four-byte word
    /// offsets, so their set and record indices must still be valid. Buffered
    /// endpoints use the slot's existing entry. No pose layout or encoded byte
    /// extent is established for this encoding.
    enum { ANIMATION_POSE_UNSUPPORTED = 2 };

    _AnimationTickScratch* scratch;
    AnimationSlot*         slot;
    GfxCoord*              coord;
    AnimationSet*          set;
    const AnimationRecord* records;
    const u8*              poseBytes;
    u16                    nextRecordIndex;
    u16                    previousRecordIndex;
    s32                    nextSetIndex;
    s32                    previousSetIndex;
    u16                    firstRecordIndex;
    u16                    segmentDuration;
    s16                    remainingTime;
    s32                    poseEncoding;
    u16                    timeLeftMinusOne;

    /// Resolves a forward control chain to this tick's next keyframe or retained endpoint.
    ///
    /// `playbackSlot` is a live writable `AnimationSlot*`; `recordArray` is its
    /// next set's borrowed `const AnimationRecord*`. `candidateIndex` is a
    /// writable `u16` absolute element index, separate from the slot and records.
    /// All visited indices and the slot's prior next index must fit that array;
    /// no length is available here. The control chain must reach a keyframe or stop.
    /// Jumps replace the index and add `ANIMATION_SLOT_FOLLOWED_JUMP`; a jump to
    /// the prior next index also adds `ANIMATION_SLOT_REACHED_BOUNDARY`. Stops
    /// restore that prior index, add the boundary flag and ignore their offset.
    /// The endpoint itself is left for the caller to install; flags accumulate.
    ///
    /// Arguments are evaluated repeatedly: pointer values and the index lvalue
    /// must stay stable, have no side effects and not refer to `controlRecord`,
    /// the block-local temporary. Only the candidate index and slot flags change;
    /// the internal break leaves only the control walk. The signed-byte cast tests
    /// the control bit; the stop threshold compares the original unsigned flags.
    /// Negating the `u16` index in `s32` is representable and preserves the matching
    /// address-add operand order without converting a pointer to an integer.
#define ANIMATION_RESOLVE_TICK_NEXT_RECORD(playbackSlot, recordArray, candidateIndex)                \
    do {                                                                                             \
        const AnimationRecord* controlRecord;                                                        \
                                                                                                     \
        while ((s8)(recordArray)[(candidateIndex)].flags < 0) {                                      \
            controlRecord = (recordArray) - -(s32)(candidateIndex);                                  \
            if (controlRecord->flags < ANIMATION_RECORD_END_THRESHOLD) {                             \
                (candidateIndex) = controlRecord->wordOffset;                                        \
                if ((candidateIndex) == (playbackSlot)->nextPose.indices.recordIndex) {              \
                    (playbackSlot)->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;          \
                }                                                                                    \
                (playbackSlot)->status.fields.flags |= ANIMATION_SLOT_FOLLOWED_JUMP;                 \
            } else {                                                                                 \
                (candidateIndex)                     = (playbackSlot)->nextPose.indices.recordIndex; \
                (playbackSlot)->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;              \
                break;                                                                               \
            }                                                                                        \
        }                                                                                            \
    } while (0)

    slot                      = &context->slots[slotIndex];
    coord                     = &context->coords[slot->coordIndex];
    scratch                   = SCRATCH_STACK_RESERVE_BLOCK(_AnimationTickScratch);
    slot->status.fields.flags = 0;
    // A latched hold whose endpoints still agree reports only the hold and does not step time.
    if (slot->atEnd == 1) {
        if (slot->nextPose.key == slot->currentPose.key) {
            slot->status.fields.flags = ANIMATION_SLOT_SETTLED;
        } else {
            slot->atEnd = 0;
        }
    } else {
        if (gGameSession->deathVariant != 0) {
            // Keep the halfword truncation before the signed half-rate subtraction.
            timeLeftMinusOne = slot->timeLeft - 1;
            slot->timeLeft   = timeLeftMinusOne - ((slot->rate - 1) >> 1);
        } else {
            slot->timeLeft -= slot->rate;
        }
    }

    // Resolve track jumps and ends before assigning another interpolation segment.
    remainingTime = slot->timeLeft;
    if (remainingTime <= 0) {
        slot->field_A = 0;
        while (slot->timeLeft <= 0) {
            slot->currentPose.key = slot->nextPose.key;
            nextRecordIndex       = slot->nextPose.indices.recordIndex + 1;
            nextSetIndex          = slot->nextPose.indices.setIndex;
            records               = slot->sets[nextSetIndex]->records;
            ANIMATION_RESOLVE_TICK_NEXT_RECORD(slot, records, nextRecordIndex);
#undef ANIMATION_RESOLVE_TICK_NEXT_RECORD
            slot->nextPose.indices.setIndex    = nextSetIndex;
            slot->nextPose.indices.recordIndex = nextRecordIndex;
            records                            = slot->sets[slot->nextPose.indices.setIndex]->records;
            segmentDuration                    = records[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
            slot->timeSpan                     = segmentDuration;
            slot->timeLeft                    += segmentDuration;
        }
        if (slot->status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            // The forward walk reached a boundary: latch the hold.
            slot->atEnd                = 1;
            slot->status.fields.flags |= ANIMATION_SLOT_SETTLED;
        } else {
            slot->atEnd = 0;
        }
    } else if (slot->timeSpan < remainingTime) {
        // Reverse traversal uses physical predecessor records, without following controls.
        slot->field_A = 0;
        while (slot->timeLeft > slot->timeSpan) {
            slot->timeLeft     -= slot->timeSpan;
            slot->nextPose.key  = slot->currentPose.key;
            previousSetIndex    = slot->currentPose.indices.setIndex;
            previousRecordIndex = slot->currentPose.indices.recordIndex - 1;
            firstRecordIndex    = slot->sets[previousSetIndex]->trackStartIndices[slot->trackIndex];
            if (previousRecordIndex < firstRecordIndex) {
                previousRecordIndex        = firstRecordIndex;
                slot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            slot->currentPose.indices.recordIndex = previousRecordIndex;
            slot->currentPose.indices.setIndex    = previousSetIndex;
            records                               = slot->sets[slot->nextPose.indices.setIndex]->records;
            segmentDuration                       = records[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
            slot->timeSpan                        = segmentDuration;
        }
        if (slot->status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            // The reverse walk reached the track start: latch the hold.
            slot->atEnd                = 1;
            slot->status.fields.flags |= ANIMATION_SLOT_SETTLED;
        } else {
            slot->atEnd = 0;
        }
    }

    // Preserve the previous buffered-endpoint flag in the request byte while resolving this tick's endpoints.
    // Bank offsets count words; buffered entries reserve four words per slot.
    poseEncoding                          = slot->poseEncoding;
    scratch->request.refreshRotationDelta = slot->usesBufferedPose;
    slot->usesBufferedPose                = 0;
    if (slot->currentPose.indices.setIndex == ANIMATION_SET_BUFFERED_POSE) {
        scratch->request.currentPose.bytes = context->poseBuffer[slotIndex];
        slot->usesBufferedPose             = 1;
    } else {
        records                            = slot->sets[slot->currentPose.indices.setIndex]->records;
        poseBytes                          = slot->sets[slot->currentPose.indices.setIndex]->poseBanks[poseEncoding];
        scratch->request.currentPose.bytes = &poseBytes[records[slot->currentPose.indices.recordIndex].wordOffset * sizeof(u32)];
    }
    if (slot->nextPose.indices.setIndex == ANIMATION_SET_BUFFERED_POSE) {
        scratch->request.nextPose.bytes = context->poseBuffer[slotIndex];
        slot->usesBufferedPose          = 1;
    } else {
        set                             = slot->sets[slot->nextPose.indices.setIndex];
        records                         = set->records;
        poseBytes                       = set->poseBanks[poseEncoding];
        scratch->request.nextPose.bytes = &poseBytes[records[slot->nextPose.indices.recordIndex].wordOffset * sizeof(u32)];
    }
    // Convert the previous flag into a refresh request; continuing buffered blends retain their cached delta.
    if ((scratch->request.refreshRotationDelta == ANIMATION_ROTATION_DELTA_REUSE) && (slot->usesBufferedPose == 1)) {
        scratch->request.refreshRotationDelta = slot->usesBufferedPose;
    } else {
        scratch->request.refreshRotationDelta = ANIMATION_ROTATION_DELTA_REUSE;
    }
    scratch->request.encodedDestination.address = encodedDestination;
    scratch->request.unpackedDestination        = unpackedDestination;
    switch (poseEncoding) {
        case ANIMATION_POSE_TRANSLATION_ROTATION:
            _animationBlendTranslationRotation(&scratch->request, coord, slot);
            break;
        case ANIMATION_POSE_UNSUPPORTED:
            printf(_gAnimationUnsupportedPoseDiagnostic);
            break;
        case ANIMATION_POSE_PACKED_ROTATION:
            _animationBlendPackedRotation(&scratch->request, coord, slot);
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_AnimationTickScratch);
}

/// Selects a seek's destination record, following controls and accumulating walk flags.
///
/// `candidateIndex` is an absolute element index in the target set's borrowed
/// `recordArray`. Jump controls replace it with their unsigned `wordOffset`
/// and add `ANIMATION_SLOT_FOLLOWED_JUMP`. A jump to the slot's prior next
/// record index also adds `ANIMATION_SLOT_REACHED_BOUNDARY`; set indices are
/// not compared. Stop controls ignore their offset, retain that prior record
/// index and add the boundary flag. Existing flags, including the capture
/// tick's results, remain set.
///
/// Installs only `nextPose.indices.recordIndex`; the caller installs the target
/// set index afterwards. Every visited index and the retained prior index must
/// fit the target array, even when its set differs from the prior endpoint's
/// set. The chain must reach a keyframe or stop. Record storage must remain
/// readable and the slot writable throughout the call; no bounds are checked.
static inline void _animationSelectSeekRecord(AnimationSlot* playbackSlot, const AnimationRecord* recordArray, u16 candidateIndex)
{
    const AnimationRecord* controlRecord;

    // Test the control bit as signed, but classify jump versus stop as unsigned.
    while ((s8)recordArray[candidateIndex].flags < 0) {
        // Negating the promoted u16 preserves the matching address-add operand order.
        controlRecord = recordArray - -candidateIndex;
        if (controlRecord->flags < ANIMATION_RECORD_END_THRESHOLD) {
            candidateIndex = controlRecord->wordOffset;
            if (candidateIndex == playbackSlot->nextPose.indices.recordIndex) {
                playbackSlot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            playbackSlot->status.fields.flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            candidateIndex                     = playbackSlot->nextPose.indices.recordIndex;
            playbackSlot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            break;
        }
    }
    playbackSlot->nextPose.indices.recordIndex = candidateIndex;
}

/// Captures a slot's ticked pose and starts a timed blend toward a track-relative record.
///
/// First ticks playback with model-coordinate and encoded-buffer outputs, then
/// replaces the current endpoint with this slot's buffer entry. Its record
/// index is retained. Zero span or unsupported encoding skips the capture's
/// pose writes, but the buffered endpoint is still installed.
/// The target is `trackRecordOffset` elements from the selected set's track
/// start, narrowed to `u16` before following controls.
/// Jumps add walk flags; a stop retains the capture tick's next record index
/// within the newly selected set. The capture tick's flags and boundary latch
/// are retained, as are the playback rate and pose encoding. Clearing the
/// buffered-endpoint flag requests a fresh rotation delta when a later tick
/// blends from the buffer.
///
/// `slotIndex` must be nonnegative and fit the slot and pose-buffer arrays.
/// `setIndex` selects a loaded slot set, excluding `ANIMATION_SET_BUFFERED_POSE`;
/// its track start, all visited records and the retained next index must fit
/// that set. The track-start sum must be representable in `s32`. The control
/// chain must terminate, and the target pose bank must support the slot's
/// existing encoding. Borrowed storage, full encoded-pose
/// bounds, scratch capacity and GTE requirements are those of `animationTickSlotPose`.
/// The slot's buffer entry must remain live until no endpoint refers to it.
///
/// `blendFrames` counts whole normal-rate frames. It is shifted in `s32` and
/// narrowed through `u16` before storing both time fields; 0..2047 keeps the
/// signed remaining time nonnegative. Zero starts with no transition time.
static inline void _animationSeekSlotWithBlend(AnimationContext* context, s32 slotIndex, u16 setIndex, s32 trackRecordOffset, s32 blendFrames)
{
    AnimationSlot*         slot;
    AnimationSet*          set;
    const AnimationRecord* records;
    u16                    recordIndex;
    u16                    blendDuration;
    u8(*bufferedPose)[ANIMATION_POSE_BUFFER_BYTES];

    // Capture this slot's encoded blend before replacing its destination keyframe.
    bufferedPose = context->poseBuffer + slotIndex;
    slot         = &context->slots[slotIndex];
    animationTickSlotPose(context, slotIndex, NULL, bufferedPose);
    slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;

    // Resolve controls in the target track without clearing the capture tick's results.
    set         = slot->sets[setIndex];
    records     = set->records;
    recordIndex = set->trackStartIndices[slot->trackIndex] + trackRecordOffset;
    _animationSelectSeekRecord(slot, records, recordIndex);
    slot->nextPose.indices.setIndex = setIndex;
    blendDuration                   = blendFrames << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan                  = blendDuration;
    slot->timeLeft                  = blendDuration;
    slot->usesBufferedPose          = 0;
}

/// Captures a slot's pose and blends toward a track-relative record for its own duration.
///
/// Performs `_animationSeekSlotWithBlend`'s capture and control walk, then
/// replaces both time fields with the resolved keyframe's duration in
/// sixteenths of a frame. The duration is 1..255 whole normal-rate frames.
/// `setIndex` must fit the slot table as s32 and u16, excluding the buffered
/// sentinel: the duration lookup uses the full argument, while the seek narrows
/// it. Slot/buffer capacities, borrowed lifetimes, target encoding, terminating
/// control chains and scratch/GTE requirements follow the capture helper.
static void _animationSeekSlotWithRecordDuration(AnimationContext* context, s32 slotIndex, s32 setIndex, s32 trackRecordOffset)
{
    enum { ANIMATION_PROVISIONAL_BLEND_FRAMES = 1 };

    AnimationSlot*         slot;
    const AnimationRecord* records;
    u16                    segmentTime;

    slot    = &context->slots[slotIndex];
    records = slot->sets[setIndex]->records;
    _animationSeekSlotWithBlend(context, slotIndex, setIndex, trackRecordOffset, ANIMATION_PROVISIONAL_BLEND_FRAMES);
    // The resolved keyframe determines the final transition duration.
    segmentTime    = records[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan = segmentTime;
    slot->timeLeft = segmentTime;
}

void animationStartDirectSlot(AnimationContext* context, AnimationSlot* directSlot, s32 partIndex, s32 requestedSetIndex, s32 trackRecordOffset, s32 blendFrames)
{
    enum { ANIMATION_DEMO_BLEND_SCENE = 1 };

    AnimationSlot*         slot;
    AnimationSet**         sets;
    AnimationSet*          set;
    const AnimationRecord* records;
    u16                    recordIndex;
    u16                    blendDuration;
    u8                     recordFlags;
    s32                    selectedSetIndex;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == ANIMATION_DEMO_BLEND_SCENE) {
        u8 slotIndex;
        u8(*bufferedPose)[ANIMATION_POSE_BUFFER_BYTES];

        slotIndex              = directSlot->trackIndex;
        selectedSetIndex       = requestedSetIndex;
        context->slots         = directSlot - slotIndex;
        directSlot->coordIndex = partIndex;
        slotIndex              = directSlot->trackIndex;
        // Capture this slot's encoded blend before replacing its destination keyframe.
        bufferedPose = context->poseBuffer + slotIndex;
        slot         = &context->slots[slotIndex];
        animationTickSlotPose(context, slotIndex, NULL, bufferedPose);
        slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
        set                                = slot->sets[(u16)selectedSetIndex];
        records                            = set->records;
        recordIndex                        = set->trackStartIndices[slot->trackIndex] + trackRecordOffset;
        ANIMATION_SELECT_DIRECT_BLEND_RECORD(slot, records, recordIndex);
        slot->nextPose.indices.setIndex = selectedSetIndex;
        blendDuration                   = blendFrames << ANIMATION_TIME_FRACTION_BITS;
        slot->timeSpan                  = blendDuration;
        slot->timeLeft                  = blendDuration;
        slot->usesBufferedPose          = 0;
    } else {
        if (requestedSetIndex == 0) {
            requestedSetIndex = 1;
        } else if (requestedSetIndex < 0) {
            requestedSetIndex = -requestedSetIndex;
        }

        ANIMATION_PRIME_MAPPED_SLOT(directSlot, requestedSetIndex, partIndex, partIndex);
        sets                                        = context->sets;
        directSlot->sets                            = sets;
        directSlot->nextPose.indices.recordIndex    = sets[requestedSetIndex]->trackStartIndices[directSlot->trackIndex];
        directSlot->currentPose.indices.recordIndex = directSlot->sets[requestedSetIndex]->trackStartIndices[directSlot->trackIndex];
        recordFlags                                 = directSlot->sets[directSlot->nextPose.indices.setIndex]->records[directSlot->nextPose.indices.recordIndex].flags;
        directSlot->status.fields.field_12          = 0;
        directSlot->status.fields.flags             = 0;
        directSlot->atEnd                           = 0;
        directSlot->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_ENCODING_MASK;
    }
}

void animationBindModelContext(AnimationContext* context, AnimationSet** setTable, TmdObject* model,
                               u8 (*poseBuffer)[ANIMATION_POSE_BUFFER_BYTES])
{
    context->sets       = setTable;
    context->coords     = PARENT_OF(model, TmdAllocation, object)->coords;
    context->poseBuffer = poseBuffer;
    context->partCount  = model->partCount;
}

void animationInitDirectSlot(AnimationContext* context, AnimationSlot* slot, s32 partIndex, s32 setIndex)
{
    AnimationSet** sets;
    u8             recordFlags;

    if (setIndex == 0) {
        setIndex = 1;
    } else if (setIndex < 0) {
        setIndex = -setIndex;
    }

    ANIMATION_PRIME_MAPPED_SLOT(slot, setIndex, partIndex, partIndex);
    sets                                  = context->sets;
    slot->sets                            = sets;
    slot->nextPose.indices.recordIndex    = sets[setIndex]->trackStartIndices[slot->trackIndex];
    slot->currentPose.indices.recordIndex = slot->sets[setIndex]->trackStartIndices[slot->trackIndex];
    recordFlags                           = slot->sets[slot->nextPose.indices.setIndex]->records[slot->nextPose.indices.recordIndex].flags;
    slot->status.fields.field_12          = 0;
    slot->status.fields.flags             = 0;
    slot->atEnd                           = 0;
    slot->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_ENCODING_MASK;
}

void animationTickDirectSlot(AnimationContext* context, AnimationSlot* slot)
{
    u8 slotIndex;

    slotIndex      = slot->trackIndex;
    context->slots = slot - slotIndex;
    animationTickSlotPose(context, slotIndex, NULL, NULL);
}

void animationTickPlayerSlot(AnimationContext* context, AnimationSlot* slot)
{
    u8 slotIndex;

    slotIndex      = slot->trackIndex;
    context->slots = slot - slotIndex;
    animationTickSlotPose(context, slotIndex, NULL, NULL);
}

/// Ticks a directly supplied slot into its model coordinate, retaining the recovered array base.
///
/// `slot->trackIndex` must equal its playback-array index. The live array must
/// extend back to slot - slot->trackIndex, which replaces context->slots.
/// No unpacked or encoded output is requested. Index, buffer, borrowed-storage,
/// scratch and GTE requirements are those of `animationTickSlotPose`.
static void _animationTickDirectSlot(AnimationContext* context, AnimationSlot* slot)
{
    u8 slotIndex;

    slotIndex      = slot->trackIndex;
    context->slots = slot - slotIndex;
    animationTickSlotPose(context, slotIndex, NULL, NULL);
}

/// Starts a direct slot at its track start, replacing its span with the selected record's duration.
///
/// Uses `animationStartDirectSlot`'s demo capture or ordinary initialization,
/// then installs the selected next record's duration (1..255 normal-rate frames)
/// in both time fields. The record array is borrowed from the slot's pre-call
/// sets[setIndex], even if ordinary initialization rebinds the slot to the
/// context table. That original table index and final record index must be valid;
/// set zero is looked up unchanged here even though ordinary setup selects set 1.
/// All other index, lifetime, buffer and GTE requirements follow the starter.
static void _animationStartDirectTrack(AnimationContext* context, AnimationSlot* slot, s32 partIndex, s32 setIndex)
{
    enum { ANIMATION_PROVISIONAL_BLEND_FRAMES = 8 };

    const AnimationRecord* records;
    u16                    segmentTime;

    records = slot->sets[setIndex]->records;
    animationStartDirectSlot(context, slot, partIndex, setIndex, 0, ANIMATION_PROVISIONAL_BLEND_FRAMES);
    segmentTime    = records[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan = segmentTime;
    slot->timeLeft = segmentTime;
}

/// Starts a direct slot and uses its resolved next record's duration for the transition.
///
/// `trackRecordOffset` counts records from the selected track start in demo
/// scene 1; ordinary initialization ignores it. The final span is 1..255
/// normal-rate frames, read from the slot's pre-call sets[setIndex] record array,
/// even if initialization rebinds that table. The original set index is not
/// normalized for this lookup, and the resolved next index must fit that array.
/// Other bounds, capture behavior and borrowed lifetimes follow
/// `animationStartDirectSlot`. Does not perform an additional pose tick.
static void _animationStartDirectRecord(AnimationContext* context, AnimationSlot* slot, s32 partIndex, s32 setIndex, s32 trackRecordOffset)
{
    enum { ANIMATION_PROVISIONAL_BLEND_FRAMES = 8 };

    const AnimationRecord* records;
    u16                    segmentTime;

    records = slot->sets[setIndex]->records;
    animationStartDirectSlot(context, slot, partIndex, setIndex, trackRecordOffset, ANIMATION_PROVISIONAL_BLEND_FRAMES);
    segmentTime    = records[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan = segmentTime;
    slot->timeLeft = segmentTime;
}

void animationBindContext(AnimationContext* context, AnimationSet** setTable, TmdObject* model,
                          u8 (*poseBuffer)[ANIMATION_POSE_BUFFER_BYTES], AnimationSlot* slots)
{
    context->sets       = setTable;
    context->coords     = PARENT_OF(model, TmdAllocation, object)->coords;
    context->poseBuffer = poseBuffer;
    context->partCount  = model->partCount;
    context->slots      = slots;
}

void animationInitContext(AnimationContext* context, AnimationSet** setTable, TmdObject* model,
                          u8 (*poseBuffer)[ANIMATION_POSE_BUFFER_BYTES], AnimationSlot* slots)
{
    animationBindContext(context, setTable, model, poseBuffer, slots);
}

/// Primes a slot's rate, time and endpoint sets for a model-part track restart.
///
/// `slot` is writable. `partIndex` selects both the source track and destination
/// coordinate and must fit their arrays. `setIndex` is a loaded set-table index,
/// including zero, excluding `ANIMATION_SET_BUFFERED_POSE`.
///
/// The caller must bind the set table, install the track start as the next
/// record, select its pose encoding and clear the boundary state before ticking.
/// Zero remaining time makes the first forward tick begin its segment walk
/// from that track start; the zero current record is replaced before pose lookup.
static inline void _animationPrimeSlotTrack(AnimationSlot* slot, u8 partIndex, u16 setIndex)
{
    slot->rate                            = ANIMATION_RATE_ONE;
    slot->timeLeft                        = 0;
    slot->currentPose.indices.setIndex    = setIndex;
    slot->currentPose.indices.recordIndex = 0;
    slot->coordIndex                      = partIndex;
    slot->trackIndex                      = partIndex;
    slot->nextPose.indices.setIndex       = setIndex;
}

void animationResetSlot(AnimationContext* context, s32 slotIndex, s32 setIndex)
{
    AnimationSlot* slot;
    AnimationSet** sets;
    u8             recordFlags;

    slot = &context->slots[slotIndex];
    _animationPrimeSlotTrack(slot, slotIndex, setIndex);
    // Bind the track start and its encoding before clearing the prior playback results.
    sets                               = context->sets;
    slot->sets                         = sets;
    slot->nextPose.indices.recordIndex = sets[setIndex]->trackStartIndices[slot->trackIndex];
    recordFlags                        = slot->sets[slot->nextPose.indices.setIndex]->records[slot->nextPose.indices.recordIndex].flags;
    slot->status.fields.flags          = 0;
    slot->atEnd                        = 0;
    slot->status.fields.field_12       = 0;
    slot->poseEncoding                 = recordFlags & ANIMATION_RECORD_POSE_ENCODING_MASK;
}

void animationResetRemappedSlot(AnimationContext* context, s32 slotIndex, s32 setIndex, s32 trackIndex, s32 coordIndex)
{
    AnimationSlot* slot;
    AnimationSet** sets;
    u8             recordFlags;

    slot = &context->slots[slotIndex];
    ANIMATION_PRIME_MAPPED_SLOT(slot, setIndex, trackIndex, coordIndex);
    sets                               = context->sets;
    slot->sets                         = sets;
    slot->nextPose.indices.recordIndex = sets[setIndex]->trackStartIndices[slot->trackIndex];
    recordFlags                        = slot->sets[slot->nextPose.indices.setIndex]->records[slot->nextPose.indices.recordIndex].flags;
    slot->status.fields.flags          = 0;
    slot->atEnd                        = 0;
    slot->status.fields.field_12       = 0;
    slot->poseEncoding                 = recordFlags & ANIMATION_RECORD_POSE_ENCODING_MASK;
}

/// Captures a slot's pose and blends to its track start using the resolved record's duration.
///
/// Selects track-relative offset zero; control records may redirect that start.
/// Bounds, retained flags, storage lifetimes and scratch/GTE requirements are
/// those of `_animationSeekSlotWithRecordDuration`.
static void _animationSeekTrackStart(AnimationContext* context, s32 slotIndex, s32 setIndex)
{
    _animationSeekSlotWithRecordDuration(context, slotIndex, setIndex, 0);
}

void animationSeekSlotWithBlend(AnimationContext* context, s32 slotIndex, s32 setIndex, s32 trackRecordOffset, s32 blendFrames)
{
    _animationSeekSlotWithBlend(context, slotIndex, (u16)setIndex, trackRecordOffset, blendFrames);
}

void animationApplyBlendedPose(AnimationContext* context, s32 slotIndex, const AnimationPose* firstPose, const AnimationPose* secondPose, s32 firstWeight,
                               s32 secondWeight)
{
    AnimationPose*       scratchEnd;
    const AnimationSlot* slot;
    GfxCoord*            destinationCoord;
    SVECTOR*             blendedTranslation;
    SVECTOR*             blendedRotation;
    s32                  coordIndex;

    slot                                = &context->slots[slotIndex];
    scratchEnd                          = SCRATCH_STACK_CURSOR(AnimationPose);
    coordIndex                          = slot->coordIndex;
    SCRATCH_STACK_CURSOR(AnimationPose) = scratchEnd - 1;
    destinationCoord                    = &context->coords[coordIndex];
    blendedTranslation                  = &scratchEnd[-1].translation;
    // Only translation-and-rotation tracks replace the local translation.
    if (slot->poseEncoding == ANIMATION_POSE_TRANSLATION_ROTATION) {
        gte_LoadAverageShort12(&firstPose->translation, &secondPose->translation, firstWeight, secondWeight,
                               blendedTranslation);
        destinationCoord->coord.t[0] = blendedTranslation->vx;
        destinationCoord->coord.t[1] = blendedTranslation->vy;
        destinationCoord->coord.t[2] = blendedTranslation->vz;
    }
    // Blend Euler components before constructing and invalidating the coordinate.
    gte_LoadAverageShort12(&firstPose->rotation, &secondPose->rotation, firstWeight, secondWeight,
                           blendedRotation = &scratchEnd[-1].rotation);
    RotMatrix_gte(blendedRotation, &destinationCoord->coord);
    destinationCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(AnimationPose);
}

void animationApplyPoseWithBlendedRotation(AnimationContext* context, s32 slotIndex, const AnimationPose* sourcePose, const AnimationPose* rotationPose, s32 sourceWeight,
                                           s32 rotationWeight)
{
    AnimationPose*       scratchEnd;
    const AnimationSlot* slot;
    GfxCoord*            destinationCoord;
    SVECTOR*             blendedRotation;
    s32                  coordIndex;

    slot                                = &context->slots[slotIndex];
    scratchEnd                          = SCRATCH_STACK_CURSOR(AnimationPose);
    coordIndex                          = slot->coordIndex;
    SCRATCH_STACK_CURSOR(AnimationPose) = scratchEnd - 1;
    destinationCoord                    = &context->coords[coordIndex];
    // Translation comes only from the source pose; blend both rotations below.
    if (slot->poseEncoding == ANIMATION_POSE_TRANSLATION_ROTATION) {
        destinationCoord->coord.t[0] = sourcePose->translation.vx;
        destinationCoord->coord.t[1] = sourcePose->translation.vy;
        destinationCoord->coord.t[2] = sourcePose->translation.vz;
    }
    gte_LoadAverageShort12(&sourcePose->rotation, &rotationPose->rotation, sourceWeight, rotationWeight,
                           blendedRotation = &scratchEnd[-1].rotation);
    RotMatrix_gte(blendedRotation, &destinationCoord->coord);
    destinationCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(AnimationPose);
}

void animationTickSlot(AnimationContext* context, s32 slotIndex)
{
    animationTickSlotPose(context, slotIndex, NULL, NULL);
}

/// Selects a captured-pose transition's next record while retaining tick flags.
///
/// `recordIndex` is an absolute unsigned 16-bit element index in borrowed
/// `records`. Control flags 0x80..0xBF jump to their absolute `wordOffset` index
/// and add `ANIMATION_SLOT_FOLLOWED_JUMP`; a jump to the prior next index also
/// adds `ANIMATION_SLOT_REACHED_BOUNDARY`, without comparing set indices.
/// Flags 0xC0..0xFF stop at that prior index and add only the boundary flag.
/// Every visited index and the fallback must fit the requested set's records,
/// and jump chains must reach a keyframe or stop. The caller installs the set.
/// Changes no timing, decoded pose, boundary latch or buffered-rotation cache; all
/// preexisting status bits survive. Retains no pointer and uses no scratch or GTE.
static inline void _animationSelectCapturedRecord(AnimationSlot* slot, const AnimationRecord* records, u16 recordIndex)
{
    const AnimationRecord* controlRecord;

    while ((s8)records[recordIndex].flags < 0) {
        controlRecord = records - -recordIndex;
        if (controlRecord->flags < ANIMATION_RECORD_END_THRESHOLD) {
            recordIndex = controlRecord->wordOffset;
            if (recordIndex == slot->nextPose.indices.recordIndex) {
                slot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            slot->status.fields.flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            recordIndex                = slot->nextPose.indices.recordIndex;
            slot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            break;
        }
    }
    slot->nextPose.indices.recordIndex = recordIndex;
}

void animationCaptureSlotWithBlend(AnimationContext* context, s32 slotIndex, AnimationPose* unpackedDestination, u16 setIndex, s32 trackRecordOffset, s32 unusedArgument, s32 blendFrames)
{
    AnimationSlot*         slot;
    AnimationSet*          set;
    const AnimationRecord* records;
    u16                    recordIndex;
    u16                    blendTime;
    u8(*bufferedPose)[ANIMATION_POSE_BUFFER_BYTES];

    // Capture this slot's encoded blend before replacing its destination keyframe.
    bufferedPose = context->poseBuffer + slotIndex;
    slot         = &context->slots[slotIndex];
    animationTickSlotPose(context, slotIndex, unpackedDestination, bufferedPose);
    slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
    set                                = slot->sets[setIndex];
    records                            = set->records;
    recordIndex                        = set->trackStartIndices[slot->trackIndex] + trackRecordOffset;
    // Preserve the capture tick's flags while resolving controls in the new track.
    _animationSelectCapturedRecord(slot, records, recordIndex);
    slot->nextPose.indices.setIndex = setIndex;
    blendTime                       = blendFrames << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan                  = blendTime;
    slot->timeLeft                  = blendTime;
    slot->usesBufferedPose          = 0;
}

const AnimationRecord* animationGetCurrentRecord(const AnimationContext* unusedContext, const AnimationSlot* slot)
{
    u16                    setIndex;
    const AnimationRecord* record;

    setIndex = slot->currentPose.indices.setIndex;
    if (setIndex == ANIMATION_SET_BUFFERED_POSE) {
        return NULL;
    }
    record  = slot->sets[setIndex]->records;
    record += slot->currentPose.indices.recordIndex;
    return record;
}

/// Selects a bank-backed next endpoint by following absolute record jumps and stops.
///
/// Accumulates flags without clearing them. Jumps add `ANIMATION_SLOT_FOLLOWED_JUMP`;
/// a jump to the prior next index also adds `ANIMATION_SLOT_REACHED_BOUNDARY`.
/// A stop retains that index in the requested set and adds only the boundary flag.
/// Does not tick, change timing, clear the boundary latch or update the pose-buffer cache.
/// `setIndex` must select a loaded slot set, excluding the buffer sentinel;
/// track controls and the fallback record must fit it and the chain must
/// terminate. The slot is writable and clip storage remains borrowed.
/// `unusedContext` is ignored and may be NULL.
static void _animationSelectNextPoseRecord(const AnimationContext* unusedContext, AnimationSlot* slot, u16 setIndex, u16 recordIndex)
{
    const AnimationRecord* records;
    const AnimationRecord* controlRecord;

    records = slot->sets[setIndex]->records;
    while ((s8)records[recordIndex].flags < 0) {
        controlRecord = records - -(s32)recordIndex;
        if (controlRecord->flags < ANIMATION_RECORD_END_THRESHOLD) {
            recordIndex = controlRecord->wordOffset;
            if (recordIndex == slot->nextPose.indices.recordIndex) {
                slot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            slot->status.fields.flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            recordIndex                = slot->nextPose.indices.recordIndex;
            slot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            break;
        }
    }
    slot->nextPose.indices.recordIndex = recordIndex;
    slot->nextPose.indices.setIndex    = setIndex;
}

/// Selects the current bank endpoint, clamping an absolute index to its track start.
///
/// An index below the start becomes the start and adds `ANIMATION_SLOT_REACHED_BOUNDARY` to
/// existing flags. Installs the requested set unchanged; does not follow control
/// records, tick or change timing, boundary hold or buffer-cache state.
/// `setIndex` must select a loaded slot set (not the buffer sentinel), and
/// trackIndex must fit its start table. The selected record must be a valid
/// endpoint for later playback. The slot is writable and the table borrowed.
/// `unusedContext` is ignored and may be NULL.
static void _animationSelectCurrentPoseRecord(const AnimationContext* unusedContext, AnimationSlot* slot, u16 setIndex, u16 recordIndex)
{
    u16 trackStartIndex;

    trackStartIndex = slot->sets[setIndex]->trackStartIndices[slot->trackIndex];
    if (recordIndex < trackStartIndex) {
        recordIndex                = trackStartIndex;
        slot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
    }
    slot->currentPose.indices.recordIndex = recordIndex;
    slot->currentPose.indices.setIndex    = setIndex;
}

/// Selects a play-with-blend's next record, following controls without clearing flags.
///
/// `playbackSlot` is the writable slot and `recordArray` the target set's borrowed
/// records. `candidateIndex` is a by-value absolute element index in that array;
/// the caller's variable is left unchanged. While `ANIMATION_RECORD_CONTROL` is
/// set, flags below `ANIMATION_RECORD_END_THRESHOLD` are a jump: the local index
/// becomes `wordOffset`, an absolute record index, and the walk adds
/// `ANIMATION_SLOT_FOLLOWED_JUMP`. A jump whose record index equals the slot's
/// prior next record index also adds `ANIMATION_SLOT_REACHED_BOUNDARY`; set
/// indices are not compared. A stop ignores `wordOffset`, retains that prior
/// record index and adds only the boundary flag. Flags already set, including
/// the capture tick's results, stay set.
///
/// Installs only `nextPose.indices.recordIndex`. The caller installs the target
/// set index afterwards. Every visited index and a stop's retained prior index
/// must fit `recordArray`, even when that array belongs to a different set from
/// the prior endpoint. The chain must reach a keyframe or stop. Records must
/// remain readable and the slot writable throughout the call; no bounds are
/// checked.
static inline void _animationSelectPlayRecord(AnimationSlot* playbackSlot, const AnimationRecord* recordArray,
                                              u16 candidateIndex)
{
    const AnimationRecord* controlRecord;

    // The signed byte tests the ANIMATION_RECORD_CONTROL bit; the unsigned threshold separates stops.
    while ((s8)recordArray[candidateIndex].flags < 0) {
        // Subtracting the negated promoted index preserves the address-add operand order.
        controlRecord = recordArray - -candidateIndex;
        if (controlRecord->flags < ANIMATION_RECORD_END_THRESHOLD) {
            candidateIndex = controlRecord->wordOffset;
            if (candidateIndex == playbackSlot->nextPose.indices.recordIndex) {
                playbackSlot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            playbackSlot->status.fields.flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            candidateIndex                     = playbackSlot->nextPose.indices.recordIndex;
            playbackSlot->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            break;
        }
    }
    playbackSlot->nextPose.indices.recordIndex = candidateIndex;
}

void animationPlaySlotWithBlend(AnimationContext* context, s32 slotIndex, AnimationPose* unpackedDestination,
                                u16 setIndex, s32 trackRecordOffset, s32 unusedArgument, s32 blendFrames,
                                AnimationSet** replacementSetTable)
{
    AnimationSlot*         slot;
    AnimationSet*          set;
    const AnimationRecord* records;
    u16                    blendDuration;
    u8(*bufferedPose)[ANIMATION_POSE_BUFFER_BYTES];

    // Capture this slot's encoded blend before replacing its destination keyframe.
    bufferedPose = context->poseBuffer + slotIndex;
    slot         = &context->slots[slotIndex];
    animationTickSlotPose(context, slotIndex, unpackedDestination, bufferedPose);
    slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
    if (replacementSetTable != NULL) {
        context->sets = replacementSetTable;
        slot->sets    = replacementSetTable;
    }
    // Resolve the new track using the replacement table only after capturing the old pose.
    set               = slot->sets[setIndex];
    records           = set->records;
    trackRecordOffset = (u16)(set->trackStartIndices[slot->trackIndex] + trackRecordOffset);
    _animationSelectPlayRecord(slot, records, trackRecordOffset);
    slot->nextPose.indices.setIndex = setIndex;
    blendDuration                   = blendFrames << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan                  = blendDuration;
    slot->timeLeft                  = blendDuration;
    slot->usesBufferedPose          = 0;
}

void Gp_SaveEnemyPose(Enemy* enemy)
{
    AreaSavedEnemyPose* savedPose;
    GameLocationKey*    savedLocation;
    TmdObject*          model;
    GfxCoord*           coord;
    SVECTOR*            euler;
    u16                 placementKey;
    s32                 poseIndex;

    savedPose     = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses;
    savedLocation = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
    model         = enemy->task->extra.tmd;
    coord         = model->coords;
    if (enemy->spawnState == 0) {
        enemy->spawnState = AREA_SAVED_ENEMY_POSE_DEFAULT_STATE;
    }
    placementKey = enemy->placeKey;
    // Keep the first saved transform and resume state for this placement.
    for (poseIndex = 0; poseIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses); poseIndex++, savedPose++) {
        if (savedPose->placeKey == placementKey) {
            return;
        }
    }

    euler     = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    savedPose = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses;
    for (poseIndex = 0; poseIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses); poseIndex++, savedPose++) {
        if (savedPose->resumeState == AREA_SAVED_ENEMY_POSE_FREE) {
            break;
        }
    }
    if (poseIndex == ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses)) {
        u32 stageAreaKey;

        // Evict a pose from another area, using the final slot as the fallback.
        savedPose    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses;
        stageAreaKey = (savedLocation->stage << AREA_PLACEMENT_STAGE_SHIFT) | savedLocation->area;
        for (poseIndex = 0; poseIndex < (ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses) - 1); poseIndex++, savedPose++) {
            if ((savedPose->placeKey & AREA_PLACEMENT_STAGE_AREA_MASK) != stageAreaKey) {
                break;
            }
        }
        for (; poseIndex < (ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses) - 1); poseIndex++, savedPose++) {
            savedPose[0] = savedPose[1];
        }
    }
    savedPose->resumeState = enemy->spawnState;
    savedPose->placeKey    = enemy->placeKey;
    savedPose->x           = coord->coord.t[0];
    savedPose->y           = coord->coord.t[1];
    savedPose->z           = coord->coord.t[2];
    // Quantize the root's Euler angles to their high bytes.
    gfxMatrixToEuler(&coord->coord, euler);
    euler->vx        = euler->vx >> AREA_SAVED_ENEMY_POSE_ANGLE_SHIFT;
    savedPose->pitch = euler->vx;
    euler->vy        = euler->vy >> AREA_SAVED_ENEMY_POSE_ANGLE_SHIFT;
    savedPose->yaw   = euler->vy;
    euler->vz        = euler->vz >> AREA_SAVED_ENEMY_POSE_ANGLE_SHIFT;
    savedPose->roll  = euler->vz;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

void Gp_SpawnArea(GameLocationKey* location)
{
    AreaRecord*     areaRecords;
    AreaVariant*    variants;
    AreaSavedState* areaState;
    AreaPlacement*  placement;
    AreaResource*   resource;
    Enemy*          enemy;
    Task*           task;
    TmdObject*      model;
    GfxCoord*       coord;
    u16             resourceId;
    s8              placementIndex;
    s32             poseIndex;

    areaRecords = Gp_AreaTables[location->stage];
    worldTargetResetAreaTracking();
    if (areaRecords == NULL) {
        return;
    }
    variants  = areaRecords[location->area].variants;
    areaState = areaRecords[location->area].savedState;
    if (variants == NULL) {
        return;
    }
    _areaPrepareSpawnState(location, areaState);
    placement      = variants[location->variant].placements;
    placementIndex = 0;
    if (placement == NULL) {
        return;
    }
    if (placement->entryId == AREA_PLACEMENT_END) {
        return;
    }
    // Match each placement with the resource entry that defines its actor.
    do {
        resource   = variants[location->variant].resources;
        resourceId = resource->entryId;
        if (resourceId != AREA_PLACEMENT_END) {
            do {
                if (resourceId == placement->entryId) {
                    if (areaState->spawnFlags & AREA_SPAWN_RESTORE_SAVED_POSES) {
                        const AreaSavedEnemyPose* savedPose;
                        s32                       poseFound;
                        s32                       savedPoseIndex;

                        savedPose = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses;
                        poseFound = 0;
                        for (savedPoseIndex = 0; savedPoseIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses); savedPoseIndex++, savedPose++) {
                            if (savedPose->placeKey == ((placementIndex << AREA_PLACEMENT_INDEX_SHIFT) | (location->stage << AREA_PLACEMENT_STAGE_SHIFT) | location->area)) {
                                poseFound = 1;
                                break;
                            }
                        }
                        if (poseFound == 0) {
                            break;
                        }
                    }
                    enemy = Gp_SpawnEnemyFromTable(resource->taskTable, resource->taskIndex,
                                                   (placement->variant << 16) | placement->mode, NULL);
                    if (enemy != NULL) {
                        u16 placementKey;

                        placementKey    = (placementIndex << AREA_PLACEMENT_INDEX_SHIFT) | (location->stage << AREA_PLACEMENT_STAGE_SHIFT) | location->area;
                        enemy->workType = ENEMY_WORK_PLAIN;
                        enemy->place    = placement;
                        enemy->placeKey = placementKey;
                        task            = enemy->task;
                        if (task->bodyKind != TASK_BODY_NONE) {
                            model = task->extra.tmd;
                            coord = model->coords;
                            if (task->bodyKind == TASK_BODY_TMD) {
                                model->texturePageOffset = placement->texturePageOffset;
                                model->clutRowOffset     = placement->clutRowOffset;
                                if (model->buffer != NULL) {
                                    tmdBuildBufferHalf(model);
                                    tmdBuildBufferHalf(model);
                                }
                            }
                            if (!(areaState->spawnFlags & AREA_SPAWN_RESTORE_SAVED_POSES)) {
                                coord->coord.t[0]   = placement->x;
                                coord->coord.t[1]   = placement->y;
                                coord->coord.t[2]   = placement->z;
                                coord->param.rot.vy = placement->yaw;
                                gfxRotMatrixY(&coord->coord, placement->yaw, 1);
                            } else {
                                const AreaSavedEnemyPose* savedPose;

                                savedPose = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses;
                                poseIndex = 0;
                                do {
                                    if (savedPose->placeKey == enemy->placeKey) {
                                        // Restore the signed translations and expand the packed angles.
                                        coord->coord.t[0]   = savedPose->x;
                                        coord->coord.t[1]   = savedPose->y;
                                        coord->coord.t[2]   = savedPose->z;
                                        coord->param.rot.vx = savedPose->pitch << AREA_SAVED_ENEMY_POSE_ANGLE_SHIFT;
                                        coord->param.rot.vy = savedPose->yaw << AREA_SAVED_ENEMY_POSE_ANGLE_SHIFT;
                                        coord->param.rot.vz = savedPose->roll << AREA_SAVED_ENEMY_POSE_ANGLE_SHIFT;
                                        RotMatrix_gte(&coord->param.rot,
                                                      &coord->coord);
                                        enemy->spawnState = savedPose->resumeState;
                                        break;
                                    }
                                    poseIndex++;
                                    savedPose++;
                                } while (poseIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses));
                                if (poseIndex == ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses)) {
                                    enemyDestroy(enemy, enemy->task);
                                }
                            }
                        }
                    }
                    break;
                }
                resource++;
                resourceId = resource->entryId;
            } while (resourceId != AREA_PLACEMENT_END);
        }
        placementIndex++;
        placement++;
    } while (placement->entryId != AREA_PLACEMENT_END);
}

void actorRenderDrawGroundShadow(GfxCoord* frame, u32 side, const SVECTOR* centreOffset)
{
    enum {
        ACTOR_RENDER_GROUND_SHADOW_PACKET_CODE = 0x2E,
        ACTOR_RENDER_GROUND_SHADOW_U_LEFT      = 0xC0,
        ACTOR_RENDER_GROUND_SHADOW_U_RIGHT     = 0xF7,
        ACTOR_RENDER_GROUND_SHADOW_V_TOP       = 0x98,
        ACTOR_RENDER_GROUND_SHADOW_V_BOTTOM    = 0xCF,
        ACTOR_RENDER_GROUND_SHADOW_SHADE       = 0xC0,
        ACTOR_RENDER_GROUND_SHADOW_OT_SHIFT    = 4
    };

    _ActorRenderGroundShadowScratch* scratch;
    POLY_FT4*                        shadowQuad;

    // Construct the square in the model part's frame before composing it for projection.
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_ActorRenderGroundShadowScratch);
    if (centreOffset == NULL) {
        scratch->vertices[0].vx = -(side >> 1);
        scratch->vertices[0].vy = 0;
        scratch->vertices[0].vz = -(side >> 1);
    } else {
        scratch->vertices[0].vx = centreOffset->vx - (side >> 1);
        scratch->vertices[0].vy = centreOffset->vy;
        scratch->vertices[0].vz = centreOffset->vz - (side >> 1);
    }
    scratch->vertices[3].vy = scratch->vertices[2].vy = scratch->vertices[1].vy = scratch->vertices[0].vy;
    scratch->vertices[1].vx = scratch->vertices[3].vx = scratch->vertices[0].vx + side;
    scratch->vertices[2].vx                           = scratch->vertices[0].vx;
    scratch->vertices[2].vz = scratch->vertices[3].vz = scratch->vertices[0].vz + side;
    scratch->vertices[1].vz                           = scratch->vertices[0].vz;
    actorRenderComposeCoord(frame);
    gte_SetRotMatrix(&frame->workm);
    gte_SetTransMatrix(&frame->workm);
    scratch->farthestDepth = 0;

    // Keep every corner's depth while the projection diagnostics describe only the last.
    ACTOR_RENDER_PROJECT_SHADOW_CORNER(scratch, 0);

    ACTOR_RENDER_PROJECT_SHADOW_CORNER(scratch, 1);

    ACTOR_RENDER_PROJECT_SHADOW_CORNER(scratch, 2);

    ACTOR_RENDER_PROJECT_SHADOW_CORNER(scratch, 3);

    if (scratch->projectionFlags >= 0) {
        shadowQuad     = gGpuPrimCursor;
        gGpuPrimCursor = shadowQuad + 1;
        setlen(shadowQuad, sizeof(*shadowQuad) / sizeof(u32) - 1);
        setcode(shadowQuad, ACTOR_RENDER_GROUND_SHADOW_PACKET_CODE);
        GPU_PRIMITIVE_XY_WORD(shadowQuad, 0) = scratch->screenXy[0];
        GPU_PRIMITIVE_XY_WORD(shadowQuad, 1) = scratch->screenXy[1];
        GPU_PRIMITIVE_XY_WORD(shadowQuad, 2) = scratch->screenXy[2];
        GPU_PRIMITIVE_XY_WORD(shadowQuad, 3) = scratch->screenXy[3];
        setUV4(shadowQuad, ACTOR_RENDER_GROUND_SHADOW_U_LEFT, ACTOR_RENDER_GROUND_SHADOW_V_TOP,
               ACTOR_RENDER_GROUND_SHADOW_U_RIGHT, ACTOR_RENDER_GROUND_SHADOW_V_TOP,
               ACTOR_RENDER_GROUND_SHADOW_U_LEFT, ACTOR_RENDER_GROUND_SHADOW_V_BOTTOM,
               ACTOR_RENDER_GROUND_SHADOW_U_RIGHT, ACTOR_RENDER_GROUND_SHADOW_V_BOTTOM);
        shadowQuad->tpage = getTPage(0, GPU_BLEND_SUBTRACT, 512, 0);
        shadowQuad->g0    = ACTOR_RENDER_GROUND_SHADOW_SHADE;
        shadowQuad->b0    = ACTOR_RENDER_GROUND_SHADOW_SHADE;
        shadowQuad->r0    = ACTOR_RENDER_GROUND_SHADOW_SHADE;
        shadowQuad->clut  = getClut(48, 266);
        addPrim(&gGpuCurrentOt[scratch->farthestDepth >> ACTOR_RENDER_GROUND_SHADOW_OT_SHIFT], shadowQuad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ActorRenderGroundShadowScratch);
}

/// Selects both raw 16-bit texture halves from the framebuffer opposite the draw buffer.
///
/// Zero `drawBufferIndex` samples the lower framebuffer at VRAM Y=272;
/// any nonzero value samples the upper one at Y=0. The right half starts at X=160.
/// Writes only texture-page and UV fields of two distinct writable quads.
/// No CLUT is needed for these 16-bit textures. Retains no pointer and does
/// not allocate packets, enqueue GPU commands or read framebuffer pixels.
static inline void _displaySetPreviousFrameTextures(POLY_FT4* leftQuad, POLY_FT4* rightQuad, u16 drawBufferIndex)
{
    enum { DISPLAY_FRAME_REDRAW_LOWER_PAGE_Y = 256 };

    if (drawBufferIndex) {
        leftQuad->tpage = getTPage(DISPLAY_FRAME_REDRAW_TEXTURE_DEPTH, GPU_BLEND_AVERAGE, 0, 0);
        leftQuad->u0 = leftQuad->u2 = 0;
        leftQuad->u1 = leftQuad->u3 = DISPLAY_EFFECT_WIDTH / 2;
        leftQuad->v0 = leftQuad->v1 = 0;
        leftQuad->v2 = leftQuad->v3 = DISPLAY_EFFECT_HEIGHT - 1;
        rightQuad->tpage            = getTPage(DISPLAY_FRAME_REDRAW_TEXTURE_DEPTH, GPU_BLEND_AVERAGE, DISPLAY_FRAME_REDRAW_RIGHT_PAGE_X, 0);
        rightQuad->u0 = rightQuad->u2 = DISPLAY_FRAME_REDRAW_RIGHT_U;
        rightQuad->u1 = rightQuad->u3 = DISPLAY_FRAME_REDRAW_RIGHT_U + DISPLAY_EFFECT_WIDTH / 2 - 1;
        rightQuad->v0 = rightQuad->v1 = 0;
        rightQuad->v2 = rightQuad->v3 = DISPLAY_EFFECT_HEIGHT - 1;
    } else {
        leftQuad->tpage = getTPage(DISPLAY_FRAME_REDRAW_TEXTURE_DEPTH, GPU_BLEND_AVERAGE, 0, DISPLAY_FRAME_REDRAW_LOWER_PAGE_Y);
        leftQuad->u0 = leftQuad->u2 = 0;
        leftQuad->u1 = leftQuad->u3 = DISPLAY_EFFECT_WIDTH / 2;
        leftQuad->v0 = leftQuad->v1 = DISPLAY_FRAME_REDRAW_LOWER_BUFFER_V;
        leftQuad->v2 = leftQuad->v3 = DISPLAY_FRAME_REDRAW_LOWER_BUFFER_V + DISPLAY_EFFECT_HEIGHT - 1;
        rightQuad->tpage            = getTPage(DISPLAY_FRAME_REDRAW_TEXTURE_DEPTH, GPU_BLEND_AVERAGE, DISPLAY_FRAME_REDRAW_RIGHT_PAGE_X, DISPLAY_FRAME_REDRAW_LOWER_PAGE_Y);
        rightQuad->u0 = rightQuad->u2 = DISPLAY_FRAME_REDRAW_RIGHT_U;
        rightQuad->u1 = rightQuad->u3 = DISPLAY_FRAME_REDRAW_RIGHT_U + DISPLAY_EFFECT_WIDTH / 2 - 1;
        rightQuad->v0 = rightQuad->v1 = DISPLAY_FRAME_REDRAW_LOWER_BUFFER_V;
        rightQuad->v2 = rightQuad->v3 = DISPLAY_FRAME_REDRAW_LOWER_BUFFER_V + DISPLAY_EFFECT_HEIGHT - 1;
    }
}

/// Blends the previous framebuffer over the current 320 by 240 frame.
///
/// The opposite of `drawBuffer` supplies two raw, semitransparent 16-bit texture
/// quads. `spawnArg1.value == 16` draws the pair twice; bit 0 enables a black fade
/// after 60 ticks, adding eight intensity units per tick and saturating at 255.
/// Saved demo scene 1 suppresses all drawing and the fade counter. This state
/// runs until its owner exits it; it never advances the task state.
/// The frame arena must fit the packets and the ordering table tags 0, 1 and 1023.
static void _displayRedrawPreviousFrame(Task* task)
{
    s32       passCount;
    s32       passIndex;
    s32       centerX;
    s32       rightEdgeBase;
    s32       borderSize;
    s32       drawBuffer;
    u8        drawBufferByte;
    u16       drawBufferHalfword;
    s32       fadeIntensity;
    s32       rightEdge;
    TILE*     fadeTile;
    DR_TPAGE* drawCommand;
    DR_TPAGE* fadeCommand;
    DR_STP*   maskCommand;
    POLY_FT4* leftQuad;
    POLY_FT4* rightQuad;

    drawBuffer    = gDisplayState.drawBuffer;
    passCount     = 1;
    centerX       = 0;
    borderSize    = 0;
    rightEdgeBase = 0;
    if (task->spawnArg1.value == DISPLAY_FRAME_REDRAW_DOUBLE_PASS) {
        passCount = 2;
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == DISPLAY_FRAME_REDRAW_SUPPRESSED_DEMO) {
        return;
    }

    // Darken the feedback after its initial hold when the fade mode is enabled.
    if (task->spawnArg1.value & DISPLAY_FRAME_REDRAW_FADE) {
        task->killCountdown++;
        if (task->killCountdown > DISPLAY_FRAME_REDRAW_FADE_DELAY) {
            fadeIntensity  = task->killCountdown - DISPLAY_FRAME_REDRAW_FADE_DELAY;
            fadeIntensity *= DISPLAY_FRAME_REDRAW_FADE_STEP;
            fadeTile       = gGpuPrimCursor;
            gGpuPrimCursor = fadeTile + 1;
            setlen(fadeTile, 3);
            setcode(fadeTile, GPU_SCREEN_SEMITRANSPARENT_TILE);
            if (fadeIntensity >= 0x100) {
                fadeIntensity = 0xFF;
            }
            fadeTile->x0 = -DISPLAY_EFFECT_WIDTH / 2;
            fadeTile->y0 = -DISPLAY_EFFECT_HEIGHT / 2;
            fadeTile->w  = DISPLAY_EFFECT_WIDTH;
            fadeTile->h  = DISPLAY_EFFECT_HEIGHT;
            fadeTile->b0 = fadeIntensity;
            fadeTile->g0 = fadeIntensity;
            fadeTile->r0 = fadeIntensity;
            addPrim(&gGpuCurrentOt[1], fadeTile);

            fadeCommand    = gGpuPrimCursor;
            gGpuPrimCursor = fadeCommand + 1;
            setlen(fadeCommand, 1);
            fadeCommand->code[0] = GPU_SCREEN_DRAW_MODE_SUBTRACT;
            addPrim(&gGpuCurrentOt[1], fadeCommand);
        }
    }

    // Mark scene pixels for the next texture blend, restoring the mask at the foreground end.
    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, 0);
    addPrim(&gGpuCurrentOt[0], maskCommand);

    drawCommand    = gGpuPrimCursor;
    gGpuPrimCursor = drawCommand + 1;
    setlen(drawCommand, 1);
    drawCommand->code[0] = GPU_SCREEN_DRAW_MODE_DITHER_DISPLAY;
    addPrim(&gGpuCurrentOt[0], drawCommand);

    // Redraw the opposite framebuffer in two horizontal texture-page spans.
    for (passIndex = 0; passIndex < passCount; passIndex++) {
        leftQuad       = gGpuPrimCursor;
        rightQuad      = leftQuad + 1;
        gGpuPrimCursor = leftQuad + 2;
        setlen(leftQuad, 9);
        setcode(leftQuad, GPU_SCREEN_RAW_SEMITRANSPARENT_QUAD);
        setlen(rightQuad, 9);
        setcode(rightQuad, GPU_SCREEN_RAW_SEMITRANSPARENT_QUAD);
        leftQuad->x0 = leftQuad->x2 = centerX - (DISPLAY_EFFECT_WIDTH / 2 + borderSize);
        leftQuad->x1 = leftQuad->x3 = centerX;
        leftQuad->y0 = leftQuad->y1 = -DISPLAY_EFFECT_HEIGHT / 2 - borderSize;
        leftQuad->y2 = leftQuad->y3 = borderSize + DISPLAY_EFFECT_HEIGHT / 2 - 1;
        rightEdge                   = rightEdgeBase + DISPLAY_EFFECT_WIDTH / 2 - 1;
        rightQuad->x0 = rightQuad->x2 = centerX;
        rightQuad->x1 = rightQuad->x3 = rightEdge;
        drawBufferByte                = drawBuffer;
        drawBufferHalfword            = drawBufferByte;
        rightQuad->y0 = rightQuad->y1 = -DISPLAY_EFFECT_HEIGHT / 2 - borderSize;
        rightQuad->y2 = rightQuad->y3 = borderSize + DISPLAY_EFFECT_HEIGHT / 2 - 1;
        _displaySetPreviousFrameTextures(leftQuad, rightQuad, drawBufferHalfword);
        addPrim(&gGpuCurrentOt[0], leftQuad);
        addPrim(&gGpuCurrentOt[0], rightQuad);
    }

    drawCommand    = gGpuPrimCursor;
    gGpuPrimCursor = drawCommand + 1;
    setlen(drawCommand, 1);
    drawCommand->code[0] = GPU_SCREEN_DRAW_MODE_DISPLAY;
    addPrim(&gGpuCurrentOt[0], drawCommand);

    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, 1);
    addPrim(&gGpuCurrentOt[DISPLAY_FRAME_REDRAW_MASK_OT_TAG], maskCommand);
}

void Gp_ApplyAreaTmdFlags(void)
{
    Task*            head;
    Task*            iter;
    GameLocationKey* key;
    AreaRecord*      rec;
    AreaVariant*     variants;
    AreaResource*    table;
    AreaResource*    entry;
    Enemy*           enemy;
    AreaPlacement*   place;
    TmdObject*       extra;
    u16              id;
    u16              flags;
    u16              limit;
    u8               idx;

    head = (gameGetTaskSlot(GAME_TASK_SLOT_SCENE))->firstChild;
    if (head != NULL) {
        iter = head;
        do {
            enemy = iter->spawnArg2.pointer;
            if (iter->bodyKind == TASK_BODY_TMD) {
                key   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
                idx   = key->stage;
                extra = iter->extra.tmd;
                rec   = Gp_AreaTables[idx];
                place = enemy->place;
                table = NULL;
                if (rec != NULL) {
                    variants = rec[key->area].variants;
                    if (variants != NULL) {
                        table = variants[key->variant].resources;
                    }
                }
                entry = table;
                id    = entry->entryId;
                if (id != AREA_PLACEMENT_END) {
                    limit = AREA_PLACEMENT_END;
                    do {
                        if (id == place->entryId) {
                            flags = entry->taskTable->header.fields.flags;
                            if (flags == TASK_BODY_TMD) {
                                extra->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
                            } else if (flags == (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER)) {
                                extra->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            }
                            break;
                        }
                        entry++;
                        id = entry->entryId;
                    } while (id != limit);
                }
            }
            iter = iter->nextSibling;
        } while (iter != head);
    }
}

void gfxReparentCoord(GfxCoord* newParent, GfxCoord* coord)
{
    GfxCoord* destinationCoord;

    destinationCoord = coord;
    if (destinationCoord->parent != newParent) {
        actorRenderComposeCoord(newParent);
        actorRenderComposeCoord(destinationCoord);
        destinationCoord->parent = newParent;
        gfxMakeRelativeTransform(&newParent->workm, &destinationCoord->workm, &destinationCoord->coord);
        destinationCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

Enemy* sceneFindEnemyByPlaceKey(u16 placeKey)
{
    Task*  head;
    Task*  iter;
    Enemy* enemy;
    s32    key;

    enemy = NULL;
    head  = gameGetTaskSlot(GAME_TASK_SLOT_SCENE)->firstChild;
    if (head != NULL) {
        iter = head;
        key  = placeKey;
        do {
            enemy = iter->spawnArg2.pointer;
            if (enemy->placeKey == key) {
                break;
            }
            iter  = iter->nextSibling;
            enemy = NULL;
        } while (iter != head);
    }
    return enemy;
}

void tmdSetTextureOffsets(TmdObject* model, s32 texturePageOffset, s32 clutRowOffset)
{
    model->texturePageOffset = texturePageOffset;
    model->clutRowOffset     = clutRowOffset;
    // Each build flips the half selector; rebuilding twice refreshes both halves.
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

static void Gp_SetCurAreaFlag2(s32 useSavedPoses)
{
    AreaRecord*      areaRecords;
    AreaSavedState*  areaState;
    GameLocationKey* key;

    key         = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].savedState;
        if (areaState != NULL) {
            if (areaState->variant == key->variant) {
                if (useSavedPoses == 0) {
                    areaState->spawnFlags &= 0xFF ^ AREA_SPAWN_RESTORE_SAVED_POSES;
                    return;
                }
                areaState->spawnFlags |= AREA_SPAWN_RESTORE_SAVED_POSES;
            }
        }
    }
}

s32 Gp_GetAreaFlag2(GameLocationKey* key)
{
    AreaRecord*     areaRecords;
    AreaSavedState* areaState;
    s32             savedPoseFlag;

    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].savedState;
        if (areaState != NULL) {
            savedPoseFlag = areaState->spawnFlags & AREA_SPAWN_RESTORE_SAVED_POSES;
            return savedPoseFlag != 0;
        }
    }
    return 0;
}

static AreaSavedState* Gp_GetAreaObj(GameLocationKey* key)
{
    AreaRecord*     areaRecords;
    AreaSavedState* areaState;

    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords == NULL) {
        areaState = NULL;
    } else {
        areaState = areaRecords[key->area].savedState;
    }
    return areaState;
}

/// Initializes the placement selector and applies a requested saved-pose reset.
static void _areaPrepareSpawnState(GameLocationKey* key, AreaSavedState* areaState)
{
    s32                 shiftIndex;
    s32                 poseIndex;
    AreaSavedEnemyPose* savedPoses;

    if (areaState->variant == 0) {
        areaState->variant     = AREA_DEFAULT_VARIANT;
        areaState->spawnFlags |= AREA_SPAWN_RESET_SAVED_POSES;
    }
    // A changed layout invalidates saved poses for every placement in this area.
    if (areaState->spawnFlags & AREA_SPAWN_RESET_SAVED_POSES) {
        areaState->spawnFlags &= 0xFF ^ (AREA_SPAWN_RESET_SAVED_POSES | AREA_SPAWN_RESTORE_SAVED_POSES);
        poseIndex              = (ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses) - 1);
        savedPoses             = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses;
        do {
            if ((savedPoses[poseIndex].placeKey & AREA_PLACEMENT_STAGE_AREA_MASK) == ((key->stage << AREA_PLACEMENT_STAGE_SHIFT) | key->area)) {
                if (poseIndex != (ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses) - 1)) {
                    for (shiftIndex = poseIndex; shiftIndex < (ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses) - 1); shiftIndex++) {
                        savedPoses[shiftIndex] = savedPoses[shiftIndex + 1];
                    }
                }
                savedPoses[(ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses) - 1)].resumeState = AREA_SAVED_ENEMY_POSE_FREE;
                savedPoses[(ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.enemyPoses) - 1)].placeKey    = 0;
            }
            poseIndex--;
        } while (poseIndex >= 0);
    }
}

void areaSetPlacementVariant(GameLocationKey* key, s32 variant, s32 resetMode)
{
    AreaRecord*     areaRecords;
    AreaSavedState* areaState;

    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].savedState;
        if (areaState != NULL) {
            if (resetMode != AREA_VARIANT_RESET_IF_CHANGED) {
                areaState->variant = variant;
                if (resetMode == AREA_VARIANT_SKIP_POSE_RESET) {
                    areaState->spawnFlags &= 0xFF ^ AREA_SPAWN_RESET_SAVED_POSES;
                } else {
                    areaState->spawnFlags |= AREA_SPAWN_RESET_SAVED_POSES;
                }
                areaState->spawnFlags &= 0xFF ^ AREA_SPAWN_RESTORE_SAVED_POSES;
            } else if (areaState->variant != variant) {
                areaState->variant    = variant;
                areaState->spawnFlags = (areaState->spawnFlags | AREA_SPAWN_RESET_SAVED_POSES) & (0xFF ^ AREA_SPAWN_RESTORE_SAVED_POSES);
            } else {
                areaState->spawnFlags &= 0xFF ^ AREA_SPAWN_RESET_SAVED_POSES;
            }
            _areaPrepareSpawnState(key, areaState);
        }
    }
}

void Gp_SetAreaFlag2(s32 useSavedPoses, GameLocationKey* key)
{
    AreaRecord*     areaRecords;
    AreaSavedState* areaState;

    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].savedState;
        if (areaState != NULL) {
            if (areaState->variant == key->variant) {
                if (useSavedPoses == 0) {
                    areaState->spawnFlags &= 0xFF ^ AREA_SPAWN_RESTORE_SAVED_POSES;
                    return;
                }
                areaState->spawnFlags |= AREA_SPAWN_RESTORE_SAVED_POSES;
            }
        }
    }
}

static AreaResource* Gp_GetNestedAreaObj(GameLocationKey* key)
{
    AreaRecord*   areaRecords;
    AreaVariant*  variants;
    AreaResource* resources;

    areaRecords = Gp_AreaTables[key->stage];
    resources   = NULL;
    if (areaRecords != NULL) {
        variants = areaRecords[key->area].variants;
        if (variants != NULL) {
            resources = variants[key->variant].resources;
        }
    }
    return resources;
}

AreaVariant* Gp_GetNestedAreaRec(GameLocationKey* key)
{
    AreaRecord*  areaRecords;
    AreaVariant* variants;

    areaRecords = Gp_AreaTables[key->stage];
    variants    = NULL;
    if (areaRecords != NULL) {
        variants = areaRecords[key->area].variants;
        if (variants != NULL) {
            variants = &variants[key->variant];
        }
    }
    return variants;
}

void Gp_SetAreaFlag0(GameLocationKey* location)
{
    u32             stageAreaKey;
    AreaRecord*     areaRecords;
    AreaSavedState* areaState;

    stageAreaKey = GAME_LOCATION_WORD(*location) & GAME_LOCATION_STAGE_AREA_MASK;
    areaRecords  = Gp_AreaTables[location->stage];
    if (stageAreaKey != GAME_LOCATION_KEY(3, 0x26, 0, 0)) {
        if (areaRecords != NULL) {
            areaState = areaRecords[location->area].savedState;
            if (areaState != NULL) {
                areaState->spawnFlags |= AREA_SPAWN_RESET_SAVED_POSES;
            }
        }
    }
}

void sceneManagerTask(Task* sceneTask)
{
    enum {
        SCENE_MANAGER_STATE_INITIALIZE = 0,
        SCENE_MANAGER_STATE_RUNNING    = 1,
        SCENE_MANAGER_STATE_COUNT
    };
    const TaskFunc stateHandlers[SCENE_MANAGER_STATE_COUNT] = {
        [SCENE_MANAGER_STATE_INITIALIZE] = _sceneInitializeManager,
        [SCENE_MANAGER_STATE_RUNNING]    = _worldTargetDrawOverlayTask
    };

    stateHandlers[sceneTask->state](sceneTask);
}

/// Finds the scene child in the placed-actor bank with the complete placement key.
///
/// `placeKey` packs area (bits 0..7), stage (8..11) and instance (12..15).
/// `reply` must address one writable `Task*`; it receives the borrowed child or
/// NULL. Returns 0 on a match and -1 otherwise. The message ID is ignored.
/// Every child must publish a live `Enemy` through `spawnArg2.pointer`.
static s32 _sceneFindPlacedActor(Task* scene, s32 messageId, s32 placeKey, Task** reply)
{
    Enemy* enemy = NULL;
    Task*  head;
    Task*  child;
    s32    result;

    *reply = NULL;
    head   = scene->firstChild;
    result = SCENE_CHILD_LOOKUP_NOT_FOUND;
    if (head == NULL) {
        return result;
    }
    child = head;
    do {
        // The child publishes its enemy work through spawnArg2.
        enemy = child->spawnArg2.pointer;
        if ((enemy->workType >> SCENE_ACTOR_BANK_SHIFT) == SCENE_PLACED_ACTOR_BANK && enemy->placeKey == placeKey) {
            *reply = child;
            result = SCENE_CHILD_LOOKUP_FOUND;
            break;
        }
        child = child->nextSibling;
    } while (child != head);
    return result;
}

/// Finds a scene child outside the placed-actor bank by its low placement-key byte.
///
/// `childId` is an integer byte ID (0..255); it is compared without truncating
/// that argument. `reply` must address one writable `Task*`; it receives the
/// borrowed child or NULL. Returns 0 on a match and -1 otherwise. The message ID
/// is ignored. Every child must publish a live `Enemy` through `spawnArg2.pointer`.
static s32 _sceneFindOtherChild(Task* scene, s32 messageId, s32 childId, Task** reply)
{
    Enemy* enemy = NULL;
    Task*  head;
    Task*  child;
    s32    result;

    *reply = NULL;
    head   = scene->firstChild;
    result = SCENE_CHILD_LOOKUP_NOT_FOUND;
    if (head == NULL) {
        return result;
    }
    child = head;
    do {
        // The child publishes its enemy work through spawnArg2.
        enemy = child->spawnArg2.pointer;
        if ((enemy->workType >> SCENE_ACTOR_BANK_SHIFT) != SCENE_PLACED_ACTOR_BANK && (u8)enemy->placeKey == childId) {
            *reply = child;
            result = SCENE_CHILD_LOOKUP_FOUND;
            break;
        }
        child = child->nextSibling;
    } while (child != head);
    return result;
}

/// Runs the exit routine of every placed actor among the scene task's children.
///
/// All message words are ignored. Children publish live `Enemy` work through
/// `spawnArg2.pointer`. Save the next sibling before exit can unlink the child;
/// exit callbacks must leave that saved sibling and the traversal boundary usable.
/// Returns 0, including for an empty ring.
static s32 _sceneExitPlacedActors(Task* scene, s32 messageId, s32 firstArg, s32 secondArg)
{
    Task*  head;
    Task*  child;
    Task*  next;
    Enemy* enemy;
    u32    bank;

    head = scene->firstChild;
    if (head == NULL) {
        return 0;
    }
    child = head;
    do {
        enemy = child->spawnArg2.pointer;
        bank  = enemy->workType >> SCENE_ACTOR_BANK_SHIFT;
        next  = child->nextSibling;
        if (bank == SCENE_PLACED_ACTOR_BANK) {
            taskCallExit(child);
        }
        child = next;
    } while (child != head);
    return 0;
}

/// Forwards a message and its first payload to every placed actor in the scene.
///
/// The incoming message ID is ignored. `childMessage` selects each child's
/// handler; `payload` carries its first integer or address word, with zero as
/// its second word. Borrowed payloads must stay live through synchronous dispatch.
/// Children publish live `Enemy` work. Save the next sibling before dispatch can
/// unlink the child; callbacks must leave it and the traversal boundary usable.
/// Returns 0 and discards child results.
static s32 _sceneBroadcastToPlacedActors(Task* scene, s32 messageId, s32 payload, s32 childMessage)
{
    Task*  head;
    Task*  child;
    Task*  next;
    Enemy* enemy;
    u32    bank;

    head = scene->firstChild;
    if (head == NULL) {
        return 0;
    }
    child = head;
    do {
        enemy = child->spawnArg2.pointer;
        bank  = enemy->workType >> SCENE_ACTOR_BANK_SHIFT;
        next  = child->nextSibling;
        if (bank == SCENE_PLACED_ACTOR_BANK) {
            taskMessageDispatch(child, childMessage, payload, 0);
        }
        child = next;
    } while (child != head);
    return 0;
}

static void Gp_KillSlot4Children(void)
{
    taskCallChildExits(gameGetTaskSlot(GAME_TASK_SLOT_SCENE));
}

static void func_800B6014(void)
{
}

void areaSyncLocationVariant(GameLocationKey* key)
{
    AreaRecord*     areaRecords;
    AreaVariant*    variants;
    AreaSavedState* areaState;

    areaRecords  = Gp_AreaTables[key->stage];
    key->variant = AREA_DEFAULT_VARIANT;
    if (areaRecords == NULL) {
        return;
    }
    variants  = areaRecords[key->area].variants;
    areaState = areaRecords[key->area].savedState;
    if (variants == NULL) {
        return;
    }
    if (areaState->variant == 0) {
        areaState->variant     = AREA_DEFAULT_VARIANT;
        areaState->spawnFlags |= AREA_SPAWN_RESET_SAVED_POSES;
    }
    key->variant = areaState->variant;
}

/// Enters the previous-frame redraw state, clearing its optional fade timer.
///
/// Only spawn-word bit 0 requests the timer reset. Other modes leave that field
/// untouched; the redraw state does not use it unless the fade is enabled.
static void _displayStartPreviousFrameRedraw(Task* task)
{
    if (task->spawnArg1.value & DISPLAY_FRAME_REDRAW_FADE) {
        task->killCountdown = 0;
    }
    task->state++;
}

void displayBlendPreviousFrameTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = D_80093A5C;

    stateHandlers.funcs[task->state](task);
}

void worldCollisionCalcContactWorldOffset(const SVECTOR* position, const WorldCollisionContact* contact, SVECTOR* offset)
{
    _WorldCollisionContactViewOffsetScratch* scratch;
    SVECTOR*                                 delta;
    GfxCoord*                                viewCoord;
    s32                                      signedPushLength;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionContactViewOffsetScratch);
    delta   = &scratch->delta;
    // Measure separation after the signed-halfword coordinate truncation.
    delta->vx        = contact->point.vx - position->vx;
    delta->vy        = contact->point.vy - position->vy;
    delta->vz        = contact->point.vz - position->vz;
    viewCoord        = &gGfxViewCoord;
    signedPushLength = SquareRoot0(gfxDotProduct(delta, delta)) - contact->distance;
    if (signedPushLength >= 0) {
        signedPushLength = -signedPushLength;
    }
    // Rotate and signedPushLength the offset in the view coordinate frame.
    VectorNormalSS(&scratch->delta, &scratch->delta);
    TransposeMatrix(&viewCoord->workm, &scratch->transposedView);
    _gfxLoadRotSv(&scratch->transposedView, &scratch->delta);
    gte_rtv0();
    gte_stsv(delta);
    gte_lddp(signedPushLength);
    gte_ldsv(delta);
    gte_gpf12();
    gte_stsv(offset);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionContactViewOffsetScratch);
}

void sceneFreeActorPrimitiveBuffers(void)
{
    Task*      firstChild;
    Task*      child;
    TmdObject* model;

    firstChild = gameGetTaskSlot(GAME_TASK_SLOT_SCENE)->firstChild;
    if (firstChild != NULL) {
        child = firstChild;
        do {
            if (child->bodyKind == TASK_BODY_TMD) {
                model = child->extra.tmd;
                // Keep drawing from recreating the buffer while movie decoding uses the heap.
                model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                tmdFreePrimitiveBuffer(model);
            }
            child = child->nextSibling;
        } while (child != firstChild);
    }
}

/// The 2-bit state of entry `arg0` in the current stage's `Gp_Bit2Banks` flags.
static inline s32 _gpGetCurBit2Flag(s32 arg0)
{
    u32* p;
    u32  word;
    s32  shift;

    p      = &Gp_Bit2Banks[gGameSession->location.loc.stage].objectStates[arg0 >> 4];
    shift  = (arg0 & 0xF) * 2;
    word   = *p;
    word  &= 3 << shift;
    word >>= shift;
    return word;
}

/// Spawns the first `spawn` table entry whose kind equals `place->kind`, at that place.
static inline void _gpSpawnPlace(AreaObjectSpawn* spawn, AreaObjectPlace* place)
{
    Enemy*     enemy;
    Task*      task;
    TmdObject* extra;
    GfxCoord*  coord;
    u16        id;

    id = spawn->kind;
    while (id != AREA_OBJECT_SPAWN_END) {
        if (id == place->kind) {
            enemy = Gp_SpawnEnemyFromTable(&spawn->taskDesc, 0, spawn->kind, NULL);
            if (enemy != NULL) {
                task = enemy->task;
                if (task->bodyKind != TASK_BODY_NONE) {
                    extra               = task->extra.tmd;
                    coord               = extra->coords;
                    enemy->placeKey     = place->flagIndex | (place->placeKeyHigh << ENEMY_PLACE_STAGE_SHIFT);
                    enemy->workType     = place->kind;
                    coord->coord.t[0]   = place->x;
                    coord->coord.t[1]   = place->y;
                    coord->coord.t[2]   = place->z;
                    coord->param.rot.vy = place->yaw;
                    if (coord->param.rot.vy != 0) {
                        gfxRotMatrixY(&coord->coord, (s16)place->yaw, 1);
                    }
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
            return;
        }
        spawn++;
        id = spawn->kind;
    }
}

/// Inline form of `Gp_GetRelatedQty`: the most of a related item weapon
/// `item` can hold, from bank `bank`'s table, or 0 for a non-weapon id.
static inline s32 _gpRelatedQty(s32 item, s32 bank)
{
    s32 ret;

    item -= EQUIPMENT_WEAPON_ITEM_FIRST;
    ret   = 0;
    if ((u32)item < ARRAY_SIZE(Gp_RelatedQty0.rows)) {
        if (bank == 0) {
            ret = Gp_RelatedQty0.rows[item].capacity;
        } else {
            ret = Gp_RelatedQty1.rows[item].capacity;
        }
    }
    return ret;
}

/// How much of `item` the rows `scan` selects hold: the stack count for
/// stackable ids (0xA0 and up), otherwise 1 if any row carries it and 0 if not.
static inline s16 _gpScanHeldQty(InventoryItemRow* table, InventoryItemRange* scan, s32 item)
{
    s32 index;
    s32 found;
    s32 i;

    found = 0;
    if (item >= 0xA0) {
        index = scan->firstRow;
        return Gp_FindScanQty(table, scan, &index, item);
    }
    for (i = scan->firstRow; i < scan->firstRow + scan->rowCount; i++) {
        if (table[i].itemId == item) {
            found = 1;
            break;
        }
    }
    return found;
}

/// Printed when an enemy's work block cannot be allocated.
static const char Gp_StrNewEnemyNull[] = "new_enemy ---> NULL\n";

/// Three-entry dispatcher table: `_enemyStartTeardownDelay`, `_enemyWaitTick`, `enemyDestroy`.
static const EnemyTaskFuncTable3 Gp_EnemyWaitFuncs = { {
    _enemyStartTeardownDelay,
    _enemyWaitTick,
    enemyDestroy,
} };

static const TaskFuncTable3 Gp_StageLoadStates = { {
    Gp_StartStageLoad,
    Gp_FinishStageLoad,
    Gp_StageLoadState2,
} };

static const VECTOR D_80093A28 = { 0, -100, 0, 0 };

static const TaskFuncTable3 D_80093A38 = { {
    _fadeStartPulse,
    _fadeTickPulse,
    taskCallExit,
} };

/// Error message for animation tracks with unsupported pose encoding 2.
///
/// The NUL-terminated message occupies 21 bytes. The final three stored bytes
/// preserve the original read-only data and are not part of the printed text.
static const char _gAnimationUnsupportedPoseDiagnostic[24] = "ERROR: ex_pdriver_2\n\0\xB7\xB0\x34";

static const TaskFuncTable3 D_80093A5C = { {
    _displayStartPreviousFrameRedraw,
    _displayRedrawPreviousFrame,
    taskCallExit,
} };

/// Message table the scene task installs while it is the scene manager.
TaskMessageEntry Gp_Slot4MsgTable[5] = {
    { SCENE_MESSAGE_FIND_PLACED_ACTOR, _sceneFindPlacedActor },
    { SCENE_MESSAGE_FIND_OTHER_CHILD, _sceneFindOtherChild },
    { SCENE_MESSAGE_EXIT_PLACED_ACTORS, _sceneExitPlacedActors },
    { SCENE_MESSAGE_BROADCAST_TO_ACTORS, _sceneBroadcastToPlacedActors },
    { TASK_MESSAGE_TABLE_END, NULL },
};
AreaObjectStage Gp_Bit2Banks[6] = { { NULL, NULL }, { D_map_akropolis_8017A7FC, GameFlag_AcropolisBanks[0].header.objectStates }, { D_map_dryfield_8017A564, GameFlag_DryfieldBanks[0].header.objectStates }, { D_map_dryfield_full_8017A46C, GameFlag_DryfieldBanks[0].header.objectStates }, { D_map_shelter_8017A998, GameFlag_ShelterBanks[0].header.objectStates }, { D_map_neo_ark_8017A6EC, GameFlag_NeoArkBanks[0].header.objectStates } };

/// Registers the scene manager and its actor-message routes, then enters running state.
///
/// Requires a live task in state 0. Publishes its borrowed pointer in
/// `GAME_TASK_SLOT_SCENE` and installs the scene message table, then
/// advances to state 1. The scene owner keeps the task live while the slot is used.
/// Allocates no storage and preserves its children, body, work and spawn words.
static void _sceneInitializeManager(Task* sceneTask)
{
    gameSetTaskSlot(sceneTask, GAME_TASK_SLOT_SCENE);
    sceneTask->msgTable = Gp_Slot4MsgTable;
    sceneTask->state++;
}

/// Draws and updates the world-target overlay during the scene manager's running state.
///
/// `unusedTask` is ignored; target, view, scratch and GPU state requirements are
/// those of `worldTargetDrawOverlay`. Advances no task state and frees no storage.
static void _worldTargetDrawOverlayTask(Task* unusedTask)
{
    worldTargetDrawOverlay();
}
