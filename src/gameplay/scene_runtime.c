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

/// Three-entry dispatcher table: `Gp_EnemyWaitStart`, `Gp_EnemyWaitTick`, `enemyDestroy`.
static const EnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

static const TaskFuncTable3 Gp_StageLoadStates;

static const VECTOR D_80093A28;

static const TaskFuncTable3 D_80093A38;

static const char _gAnimationUnsupportedPoseDiagnostic[24];

static const TaskFuncTable3 D_80093A5C;

/// High byte of `Enemy::workType` for an actor the scene manager placed.
enum {
    SCENE_PLACED_ACTOR_BANK = 9,
};

extern TaskMessageEntry Gp_Slot4MsgTable[5];

s32 Gp_FindChildType9(Task* scene, s32 messageId, s32 selector, Task** reply);

s32 Gp_FindChildExceptType9(Task* scene, s32 messageId, s32 selector, Task** reply);

s32 Gp_ExitChildrenType9(Task* scene, s32 messageId, s32 firstArg, s32 secondArg);

s32 Gp_SendMsgType9(Task* scene, s32 messageId, s32 payload, s32 childMessage);

static void Gp_ApplySndMasks(u16 arg0);

static Enemy* Gp_SpawnEnemy(s32 bank, s32 type, s32 arg2, Enemy* parent);

static Enemy* Gp_AllocEnemy(Task* task, Enemy* parent);

static void Gp_EnemyWaitStart(Enemy* enemy, Task* task);

static void Gp_EnemyWaitTick(Enemy* enemy, Task* task);

static s32 Gp_TryEnqueueSndCd(s32 arg0);

void func_800B06F0(Task* arg0);

static void Gp_StartStageLoad(Task* task);

static void Gp_FinishStageLoad(Task* task);

static void Gp_StageLoadState2(Task* task);

static void func_800B1EFC(Task* t);

/// Unpacks two RGB555 colors, GPF/GPL-blends them by `arg2` / `0x1000 -
/// arg2`, packs the result into `*arg3`, and copies the STP bit if
/// either source has it set.
static void Gp_BlendRgb555(u16* arg0, u16* arg1, s32 arg2, u16* arg3);

static void Gp_BlendRgb555ClutMasked(u16* arg0, u16* arg1, s32 arg2, u16* arg3, s32 arg4);

static void func_800B28E0(Task* task);

static void _animationBlendTranslationRotation(const _AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot);

static void _animationBlendPackedRotation(const _AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot);

static void Gp_AnimAdvanceSlot(AnimationContext* context, s32 arg1);

static inline void _animationSeekSlotWithBlend(AnimationContext* context, s32 slotIndex, u16 setIndex, s32 trackRecordOffset, s32 blendFrames);

static void Gp_AnimSeekSlotEx(AnimationContext* context, s32 arg1, s32 arg2, s32 arg3);

static void Gp_AnimTickSlot3(AnimationContext* context, AnimationSlot* arg1);

static void func_800B3E74(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3);

static void func_800B3EE8(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3, s32 arg4);

static void Gp_AnimSeekSlot(AnimationContext* context, s32 arg1, s32 arg2);

static void func_800B46A4(AnimationContext* unusedContext, AnimationSlot* arg1, u16 arg2, u16 arg3);

static void func_800B4754(AnimationContext* unusedContext, AnimationSlot* arg1, u16 arg2, u16 arg3);

static void func_800B51F4(Task* task);

static void Gp_SetCurAreaFlag2(s32 useSavedPoses);

static AreaSavedState* Gp_GetAreaObj(GameLocationKey* key);

static void _areaPrepareSpawnState(GameLocationKey* key, AreaSavedState* areaState);

static AreaResource* Gp_GetNestedAreaObj(GameLocationKey* key);

static void Gp_KillSlot4Children(void);

static void func_800B6014(void);

static void func_800B6094(Task* task);

/// The 2-bit state of entry `arg0` in the current stage's `Gp_Bit2Banks` flags.
static inline s32 _gpGetCurBit2Flag(s32 arg0);

static inline void _gpSpawnPlace(AreaObjectSpawn* spawn, AreaObjectPlace* place);

/// Inline form of `Gp_GetRelatedQty`: the most of a related item weapon
/// `item` can hold, from bank `bank`'s table, or 0 for a non-weapon id.
static inline s32 _gpRelatedQty(s32 item, s32 bank);

/// How much of `item` the rows `scan` selects hold: the stack count for
/// stackable ids (0xA0 and up), otherwise 1 if any row carries it and 0 if not.
static inline s16 _gpScanHeldQty(InventoryItemRow* table, InventoryItemRange* scan, s32 item);

void Gp_BindSlot4(Task* task);

void func_800B6398(Task* task);

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
                    Mem_CopyUnaligned(&header, &p->sceneImageHeaders[(*(D_80114D14 + 1))], sizeof(header));
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
    {
        s32 cmd;
        cmd = p->entries[p->readIdx].cmd;
        if (cmd == CD_COMMAND_EMPTY) {
            goto end_check;
        }
        if (cmd < 0) {
            goto end_check;
        }
        if (cmd >= CD_COMMAND_START_SCENE_AUDIO + 1) {
            goto end_check;
        }
        if (cmd < CD_COMMAND_PLAY_SCENE_AUDIO) {
            goto end_check;
        }
    }

    switch (p->step) {
        case 0:
            CdCmd_SetBusy();
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
                        SndEvt_EnqueueType7(entry->bankTypeId, 0);
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
                CdCmd_LoadActiveEntry();
                CdCmd_AdvanceRead();
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
            CdCmd_AdvanceRead();
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
            CdCmd_AdvanceRead();
            break;
        }
    }

end_check:
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
                SndEvt_EnqueueType7(entry->bankTypeId, 0);
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

void Gp_SetStreamBuf(void* arg0)
{
    gCdCmdQueue.externalScenePayloadBuffer = arg0;
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

    task = Task_SpawnFromTable(table, idx, arg2, 0);
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

Task* Gp_CopyCoordOffset(Task* arg0, GfxCoord* arg1, SVECTOR* arg2)
{
    ModelObjectCoordBody* body;
    GfxCoord*             dest;
    GfxCoord*             world;

    if (arg0 == NULL) {
        return NULL;
    }

    SCRATCH_STACK_RESERVE_BYTES(8);
    world = &gGfxViewCoord;
    body  = arg0->extra.coordBody;
    dest  = body->coord;
    if (arg1->parent == world) {
        dest->coord = arg1->coord;
        gte_SetRotMatrix(&arg1->coord);
        gte_SetTransMatrix(&arg1->coord);
        gte_ldv0(arg2);
        gte_rtv0tr();
        gte_stlvnl(dest->coord.t);
    } else {
        actorRenderComposeCoord(arg1);
        dest->workm = arg1->workm;
        gte_SetRotMatrix(&arg1->workm);
        gte_SetTransMatrix(&arg1->workm);
        gte_ldv0(arg2);
        gte_rtv0tr();
        gte_stlvnl(dest->workm.t);
        gfxMakeRelativeTransform(&world->workm, &dest->workm, &dest->coord);
    }
    dest->parent       = &gGfxViewCoord;
    dest->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(8);
    return arg0;
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

static void Gp_EnemyWaitStart(Enemy* enemy, Task* task)
{
    enemy->waitTicks = ENEMY_WAIT_FRAMES;
    task->state++;
}

static void Gp_EnemyWaitTick(Enemy* enemy, Task* task)
{
    enemy->waitTicks--;
    if (enemy->waitTicks == 0) {
        task->state++;
    }
}

void Gp_EnemyDispatch(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Gp_EnemyWaitFuncs;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static s32 Gp_TryEnqueueSndCd(s32 arg0)
{
    u8 param1[8];
    u8 param2[8];

    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[0] = arg0;
        param1[3] = 0;
        param1[2] = 5;
        param2[0] = 1;
        param2[1] = 1;
        param2[3] = 0;
        param2[2] = 0;
        CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
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
        SndEvt_EnqueueType7(SOUND_BANK_TYPE_PE_ALL, 8);
        flag      = 1;
        param1[3] = 0;
        param1[2] = 5;
        param1[0] = arg0;
        param2[0] = flag;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
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

    if (Midi_IsBusy(0) == 0) {
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
            CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            fileId = 9;
        }
        CdCmd_EnqueueLoadFile(fileId, 0, 3);
        gDisplayState.skipDraw = 0;
        task->state++;
    }
}

static void Gp_FinishStageLoad(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        if (gGameSession->restartMode == GAME_SESSION_RESTART_ENDING) {
            Task_SpawnFromTable(D_replay_bonus_8011922C, 0, 0, 0);
            taskKill(task);
        } else {
            task->spawnArg2.pointer = Task_SpawnFromTable(D_aya_20900_80115D9C, 0, 0, 0);
            SndEvt_EnqueueType1(0x62, 0);
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

void func_800B0928(Task* arg0, Task* arg1, s32 arg2, s32 arg3, s32 arg4)
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
    s32       i;
    GfxCoord* rec;
    GfxCoord* rec1;
    s32       pitchLimit;
    s32       yawLimit;
    s32       pitchMagnitude;
    s32       yawMagnitude;
    MATRIX*   m0;
    MATRIX*   m1;
    GfxCoord* base;
    MATRIX*   m;

    i                        = 0;
    m0                       = &mtx0;
    *(s32*)&mtx0             = ONE;
    MATRIX_PAIR(&mtx0, 0, 2) = 0;
    MATRIX_PAIR(m0, 1, 1)    = ONE;
    MATRIX_PAIR(&mtx0, 2, 0) = 0;
    m0->m[2][2]              = ONE;
    acc0.vx                  = 0;
    acc0.vy                  = 0;
    acc0.vz                  = 0;
    for (i = 0; i < 4; i++) {
        rec = &arg0->extra.tmd->coords[i];
        ApplyMatrixLV(&mtx0, (VECTOR*)rec->coord.t, &tmp);
        acc0.vx += tmp.vx;
        acc0.vy += tmp.vy;
        acc0.vz += tmp.vz;
        MulMatrix0(&rec->coord, &mtx0, &mtx0);
    }
    rec = &arg0->extra.tmd->coords[i];
    ApplyMatrixLV(&mtx0, (VECTOR*)rec->coord.t, &tmp);
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
    for (i = 0; i < 4; i++) {
        rec1 = &arg1->extra.tmd->coords[i];
        ApplyMatrixLV(&mtx1, (VECTOR*)rec1->coord.t, &tmp);
        acc1.vx += tmp.vx;
        acc1.vy += tmp.vy;
        acc1.vz += tmp.vz;
        MulMatrix0(&rec1->coord, &mtx1, &mtx1);
    }
    rec1 = &arg1->extra.tmd->coords[i];
    ApplyMatrixLV(&mtx1, (VECTOR*)rec1->coord.t, &tmp);

    delta.vx = (u16)acc1.vx - (u16)acc0.vx;
    delta.vy = (u16)acc1.vy - (u16)acc0.vy;
    delta.vz = (u16)acc1.vz - (u16)acc0.vz;
    TransposeMatrix(&mtx0, &tmtx);
    ApplyMatrix(&tmtx, &delta, &acc0);

    ang.vx = ratan2(-acc0.vy, acc0.vz >= 0 ? acc0.vz : -acc0.vz);
    ang.vy = ratan2(acc0.vx, acc0.vz);
    ang.vz = 0;

    base = arg0->extra.tmd->coords;
    rec  = base + 4;
    Gp_MtxToEuler(&base[4].coord, &euler);

    ang.vx     = euler.vx + (ang.vx - euler.vx) * arg4 / 4096;
    ang.vy     = euler.vy + (ang.vy - euler.vy) * arg4 / 4096;
    ang.vz     = euler.vz;
    pitchLimit = euler.vx >= 0 ? euler.vx : -euler.vx;
    if (arg3 < pitchLimit) {
        arg3 = pitchLimit;
    }
    yawLimit = euler.vy >= 0 ? euler.vy : -euler.vy;
    if (arg2 < yawLimit) {
        arg2 = yawLimit;
    }
    pitchMagnitude = ang.vx >= 0 ? ang.vx : -ang.vx;
    if (arg3 < pitchMagnitude) {
        ang.vx = ang.vx < 0 ? -arg3 : arg3;
    }
    yawMagnitude = ang.vy >= 0 ? ang.vy : -ang.vy;
    if (arg2 < yawMagnitude) {
        ang.vy = ang.vy < 0 ? -arg2 : arg2;
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

void func_800B0CF4(Task* arg0, GfxCoord* arg1, s32 arg2, s32 arg3, s32 arg4)
{
    VECTOR    transformed;
    VECTOR    position;
    VECTOR    target;
    SVECTOR   offset;
    SVECTOR   angles;
    SVECTOR   current;
    MATRIX    world;
    MATRIX    inverse;
    MATRIX*   mtx;
    MATRIX*   outMtx;
    GfxCoord* part;
    s32       i;
    s32       pitchMagnitude;
    s32       yawMagnitude;
    s32       pitchLimit;
    s32       yawLimit;

    mtx                       = &world;
    MATRIX_PAIR(&world, 0, 0) = 0x1000;
    MATRIX_PAIR(&world, 0, 2) = 0;
    MATRIX_PAIR(mtx, 1, 1)    = 0x1000;
    MATRIX_PAIR(&world, 2, 0) = 0;
    mtx->m[2][2]              = 0x1000;
    position.vx               = 0;
    position.vy               = 0;
    position.vz               = 0;
    for (i = 0; i < 5; i++) {
        part = &arg0->extra.tmd->coords[i];
        ApplyMatrixLV(&world, (VECTOR*)part->coord.t, &transformed);
        position.vx += transformed.vx;
        position.vy += transformed.vy;
        position.vz += transformed.vz;
        MulMatrix0(&world, &part->coord, &world);
    }
    target.vx = arg1->coord.t[0];
    target.vy = arg1->coord.t[1];
    target.vz = arg1->coord.t[2];
    offset.vx = target.vx - position.vx;
    offset.vy = target.vy - position.vy;
    offset.vz = target.vz - position.vz;
    TransposeMatrix(&world, &inverse);
    ApplyMatrix(&inverse, &offset, &position);
    angles.vx = -ratan2(position.vy, position.vz);
    angles.vy = ratan2(position.vx, position.vz);
    angles.vz = 0;
    part      = &arg0->extra.tmd->coords[4];
    Gp_MtxToEuler(&part->coord, &current);
    angles.vx  = current.vx + (angles.vx - current.vx) * arg4 / 4096;
    angles.vy  = current.vy + (angles.vy - current.vy) * arg4 / 4096;
    angles.vz  = current.vz;
    pitchLimit = current.vx >= 0 ? current.vx : -current.vx;
    if (arg3 < pitchLimit) {
        arg3 = pitchLimit;
    }
    yawLimit = current.vy >= 0 ? current.vy : -current.vy;
    if (arg2 < yawLimit) {
        arg2 = yawLimit;
    }
    pitchMagnitude = angles.vx >= 0 ? angles.vx : -angles.vx;
    if (arg3 < pitchMagnitude) {
        angles.vx = angles.vx < 0 ? -arg3 : arg3;
    }
    yawMagnitude = angles.vy >= 0 ? angles.vy : -angles.vy;
    if (arg2 < yawMagnitude) {
        angles.vy = angles.vy < 0 ? -arg2 : arg2;
    }
    outMtx                          = &part->coord;
    MATRIX_PAIR(&part->coord, 0, 0) = 0x1000;
    MATRIX_PAIR(outMtx, 0, 2)       = 0;
    MATRIX_PAIR(outMtx, 1, 1)       = 0x1000;
    MATRIX_PAIR(outMtx, 2, 0)       = 0;
    outMtx->m[2][2]                 = 0x1000;
    RotMatrix(&angles, outMtx);
}

void Gp_MtxToEuler(MATRIX* arg0, SVECTOR* arg1)
{
    SVECTOR in;
    SVECTOR out;
    MATRIX  mtx;
    s32     one;
    s16     len;

    mtx      = *arg0;
    one      = 0x1000;
    mtx.t[2] = 0;
    mtx.t[1] = 0;
    mtx.t[0] = 0;
    in.vx    = 0;
    in.vy    = 0;
    in.vz    = one;
    ApplyMatrixSV(&mtx, &in, &out);
    arg1->vx = -ratan2(out.vy, out.vz);
    len      = SquareRoot12((out.vz * out.vz + out.vy * out.vy) >> 12);
    arg1->vy = ratan2(out.vx, len);
    in.vx    = 0;
    in.vy    = one;
    in.vz    = 0;
    ApplyMatrixSV(&mtx, &in, &out);
    in.vx = -arg1->vx;
    in.vy = -arg1->vy;
    in.vz = 0;
    RotMatrixZYX(&in, &mtx);
    ApplyMatrixSV(&mtx, &out, &in);
    arg1->vz = -ratan2(in.vx, in.vy);
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

void Gp_LerpOrthonormal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2, s32 arg3)
{
    MATRIX mtx;
    MATRIX diffs;
    VECTOR vec[3];
    VECTOR tmp;
    VECTOR nrm;
    s32    i;
    s32    best;
    s32    len;
    s32    ret;

    best = 0;
    for (i = 0; i < 3; i++) {
        diffs.m[i][0] = arg1->m[i][0] - arg0->m[i][0];
        diffs.m[i][1] = arg1->m[i][1] - arg0->m[i][1];
        diffs.m[i][2] = arg1->m[i][2] - arg0->m[i][2];
    }
    for (i = 0; i < 3; i++) {
        vec[i].vx = arg0->m[i][0] + (diffs.m[i][0] * arg3) / ONE;
        vec[i].vy = arg0->m[i][1] + (diffs.m[i][1] * arg3) / ONE;
        vec[i].vz = arg0->m[i][2] + (diffs.m[i][2] * arg3) / ONE;
    }

    len = -1;

    gte_ldopv1(&vec[0]);
    gte_ldopv2(&vec[1]);
    gte_op12();
    gte_stlvnl(&tmp);
    ret = VectorNormal(&tmp, &nrm);
    if (len < ret) {
        len  = ret;
        best = 2;
    }

    gte_ldopv1(&vec[1]);
    gte_ldopv2(&vec[2]);
    gte_op12();
    gte_stlvnl(&tmp);
    ret = VectorNormal(&tmp, &nrm);
    if (len < ret) {
        len  = ret;
        best = 0;
    }

    gte_ldopv1(&vec[0]);
    gte_ldopv2(&vec[2]);
    gte_op12();
    gte_stlvnl(&tmp);
    if (len < VectorNormal(&tmp, &nrm)) {
        best = 1;
    }

    switch (best) {
        case 0:
            mtx.m[1][0] = vec[1].vx;
            mtx.m[1][1] = vec[1].vy;
            mtx.m[1][2] = vec[1].vz;
            mtx.m[2][0] = vec[2].vx;
            mtx.m[2][1] = vec[2].vy;
            mtx.m[2][2] = vec[2].vz;
            MatrixNormal_1(&mtx, arg2);
            break;
        case 1:
            mtx.m[0][0] = vec[0].vx;
            mtx.m[0][1] = vec[0].vy;
            mtx.m[0][2] = vec[0].vz;
            mtx.m[2][0] = vec[2].vx;
            mtx.m[2][1] = vec[2].vy;
            mtx.m[2][2] = vec[2].vz;
            MatrixNormal_2(&mtx, arg2);
            break;
        case 2:
            mtx.m[0][0] = vec[0].vx;
            mtx.m[0][1] = vec[0].vy;
            mtx.m[0][2] = vec[0].vz;
            mtx.m[1][0] = vec[1].vx;
            mtx.m[1][1] = vec[1].vy;
            mtx.m[1][2] = vec[1].vz;
            MatrixNormal_0(&mtx, arg2);
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

void Gp_ComposeParentWorld(GfxCoord* arg0, MATRIX* arg1, SVECTOR* arg2)
{
    SVECTOR tmp;
    MATRIX* m;
    s32     one;

    if (arg0->parent != &gGfxViewCoord) {
        Gp_ComposeParentWorld(arg0->parent, arg1, arg2);
    } else {
        one                  = ONE;
        m                    = arg1;
        *(s32*)m             = one;
        MATRIX_PAIR(m, 0, 2) = 0;
        MATRIX_PAIR(m, 1, 1) = one;
        MATRIX_PAIR(m, 2, 0) = 0;
        m->m[2][2]           = one;
        arg2->vx             = 0;
        arg2->vy             = 0;
        arg2->vz             = 0;
    }

    tmp.vx = (u16)arg0->coord.t[0];
    tmp.vy = (u16)arg0->coord.t[1];
    tmp.vz = (u16)arg0->coord.t[2];
    gte_SetRotMatrix(arg1);
    gte_ldv0(&tmp);
    gte_rtv0();
    gte_stsv(&tmp);
    arg2->vx += tmp.vx;
    arg2->vy += tmp.vy;
    arg2->vz += tmp.vz;
    gte_ldclmv(&arg0->coord);
    gte_rtir();
    gte_stclmv(arg1);
    gte_ldclmv(&arg0->coord.m[0][1]);
    gte_rtir();
    gte_stclmv(&arg1->m[0][1]);
    gte_ldclmv(&arg0->coord.m[0][2]);
    gte_rtir();
    gte_stclmv(&arg1->m[0][2]);
}

static void func_800B1EFC(Task* t)
{
    TILE*     p;
    DR_TPAGE* dr;
    u8        color;

    if (t->spawnArg1.value > 0) {
        if (t->killCountdown > 0) {
            t->killCountdown--;
            color = ~(t->killCountdown << 3);
        } else {
            t->spawnArg1.value--;
            color = 0xFF;
        }
    } else {
        t->killCountdown++;
        color = ~(t->killCountdown << 3);
        if (t->killCountdown >= 0x1F) {
            t->state++;
        }
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 3);
    setcode(p, 0x62);
    setXY0(p, -0xA0, -0x78);
    p->y0 -= gDisplayState.vramYOffset;
    p->b0  = color;
    p->g0  = color;
    p->r0  = color;
    setWH(p, 0x140, 0xF0);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    if (t->spawnArg2.pointer == 0) {
        setlen(dr, 1);
        dr->code[0] = 0xE1000240;
    } else {
        setlen(dr, 1);
        dr->code[0] = 0xE1000220;
    }
    addPrim(gGpuCurrentOt, p);
    addPrim(gGpuCurrentOt, dr);
}

/// Unpacks two RGB555 colors, GPF/GPL-blends them by `arg2` / `0x1000 -
/// arg2`, packs the result into `*arg3`, and copies the STP bit if
/// either source has it set.
static void Gp_BlendRgb555(u16* arg0, u16* arg1, s32 arg2, u16* arg3)
{
    u8*             head;
    _Rgb555Scratch* firstColor;
    _Rgb555Scratch* secondColor;
    _Rgb555Scratch* blendedColor;
    u16             color;
    u16             packed;

    // Three slots below the saved cursor: first source, second source, result.
    head                                 = SCRATCH_STACK_CURSOR(u8);
    firstColor                           = (_Rgb555Scratch*)(head - 3 * sizeof(_Rgb555Scratch));
    SCRATCH_STACK_CURSOR(_Rgb555Scratch) = firstColor;

    // Place each 5-bit channel in bits 7..11.
    color         = *arg0;
    firstColor->b = color;
    firstColor->g = color;
    firstColor->r = (color & 0x1F) << 7;
    firstColor->g = (firstColor->g << 2) & 0xF80;
    firstColor->b = (firstColor->b >> 3) & 0xF80;

    secondColor    = (_Rgb555Scratch*)(head - 2 * sizeof(_Rgb555Scratch));
    color          = *arg1;
    secondColor->b = color;
    secondColor->g = color;
    secondColor->r = (color & 0x1F) << 7;
    secondColor->g = (secondColor->g << 2) & 0xF80;
    secondColor->b = (secondColor->b >> 3) & 0xF80;

    // Weight the first colour by the factor, then add the second by its complement.
    gte_lddp(arg2);
    gte_ldsv(firstColor);
    gte_gpf12();
    gte_lddp(ONE - arg2);
    gte_ldsv(secondColor);
    gte_gpl12();
    blendedColor = (_Rgb555Scratch*)(head - sizeof(_Rgb555Scratch));
    gte_stsv(blendedColor);

    // Stage blue in bits 5..9, then shift green and blue into place and insert red.
    packed = ((blendedColor->b >> 2) & 0x3E0) | ((blendedColor->g >> 7) & 0x1F);
    packed = (packed << 5) | ((blendedColor->r >> 7) & 0x1F);
    *arg3  = packed;
    // Bit 15 is semi-transparency. Either source sets it on the packed result.
    if ((s16)*arg0 < 0 || (s16)*arg1 < 0) {
        *arg3 = packed | 0x8000;
    }
    SCRATCH_STACK_RELEASE_BYTES(3 * sizeof(_Rgb555Scratch));
}

/// Full-screen fade quad. Ramps a 0x140 by 0xF0 semi-transparent `TILE` from
/// grey 0 to 255 over `rampFrames`, holds that coverage while `phase` stays
/// running, then ramps the grey back to 0 once the owner requests the return.
/// Zero `blend` subtracts the grey toward black; any other value adds it
/// toward white. Sorted into `gGpuCurrentOt[Task::spawnArg1]`, or
/// (`spawnArg1 == 0`) into the head of the current ordering table, backing up
/// 0xA entries when the current OT is not one of the two `Gpu_OrderingTables`
/// roots.
void Gp_FadeWorkTask(Task* t)
{
    ScreenFade* work;
    TILE*       tile;
    DR_TPAGE*   dr;
    s32         color;
    s16         y;
    s8          yoff;

    work = t->spawnArg2.pointer;

    if (t->state == 0) {
        t->killCountdown = 0;
        if (work->rampFrames <= 0) {
            work->rampFrames = SCREEN_FADE_DEFAULT_FRAMES;
        }
        t->state = t->state + 1;
    }
    if ((t->state == 2) && (work->phase == SCREEN_FADE_RETURN)) {
        t->killCountdown = work->rampFrames;
    }

    color          = (t->killCountdown * 0xFF0) / work->rampFrames;
    tile           = gGpuPrimCursor;
    y              = -0x78;
    tile->y0       = y;
    gGpuPrimCursor = tile + 1;
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    yoff     = gDisplayState.vramYOffset;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    color    = color >> 4;
    tile->b0 = color;
    tile->g0 = color;
    tile->r0 = color;
    dr       = gGpuPrimCursor;
    tile->y0 = y - yoff;

    gGpuPrimCursor = dr + 1;
    if (work->blend == SCREEN_FADE_SUBTRACT) {
        setlen(dr, 1);
        dr->code[0] = 0xE1000240;
    } else {
        setlen(dr, 1);
        dr->code[0] = 0xE1000220;
    }

    if (t->spawnArg1.value != 0) {
        u_long* ot;

        ot = gGpuCurrentOt;
        addPrim(&ot[t->spawnArg1.value], tile);
        addPrim(&ot[t->spawnArg1.value], dr);
    } else {
        u_long* ot;

        ot = gGpuCurrentOt;
        if ((ot == (u_long*)Gpu_OrderingTables[0].org) || (ot == (u_long*)Gpu_OrderingTables[1].org)) {
            addPrim(ot, tile);
            addPrim(ot, dr);
        } else {
            addPrim(&ot[-0xA], tile);
            addPrim(&ot[-0xA], dr);
        }
    }

    switch (t->state) {
        case 1:
            t->killCountdown = t->killCountdown + 1;
            if (t->killCountdown == work->rampFrames) {
                t->state = t->state + 1;
            }
            break;
        case 2:
            if (work->phase == SCREEN_FADE_RETURN) {
                t->state = 3;
            }
            break;
        case 3:
            t->killCountdown = t->killCountdown - 1;
            if (t->killCountdown <= 0) {
                work->phase = SCREEN_FADE_DONE;
                taskKill(t);
            }
            break;
        default:
            taskKill(t);
            break;
    }
}

void func_800B25B0(void)
{
    switch (GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) {
        case GAME_LOCATION_KEY(5, 27, 0, 0):
            Task_SpawnFromTable(&D_neo_ark_bridge_80181F18, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 15, 0, 0):
            Task_SpawnFromTable(&D_neo_ark_garden_80181398, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 14, 0, 0):
            Task_SpawnFromTable(&D_neo_ark_island_80181B30, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 13, 0, 0):
            Task_SpawnFromTable(&D_neo_ark_pavilion_8018384C, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 12, 0, 0):
            Task_SpawnFromTable(&D_neo_ark_submarine_tunnel_801810E4, 1, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 7, 0, 0):
            Task_SpawnFromTable(D_neo_ark_observatory_80180DBC, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(2, 30, 0, 0):
            Task_SpawnFromTable(D_dryfield_motel_room_6_80182D0C, 0, 1, 0);
            break;
        case GAME_LOCATION_KEY(3, 30, 0, 0):
            Task_SpawnFromTable(D_dryfield_night_motel_room_6_80182E74, 0, 1, 0);
            break;
        case GAME_LOCATION_KEY(4, 18, 0, 0):
            Task_SpawnFromTable(&D_shelter_b1_control_room_80181B88, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 31, 0, 0):
            Task_SpawnFromTable(&D_neo_ark_r31_8017D9E8, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 30, 0, 0):
            Task_SpawnFromTable(&D_neo_ark_submarine_gallery_8018186C, 0, 0, 0);
            Task_SpawnFromTable(&D_neo_ark_submarine_gallery_8018186C, 1, 0, 0);
            break;
        case GAME_LOCATION_KEY(5, 29, 0, 0):
            Task_SpawnFromTable(&D_neo_ark_woodland_path_80181638, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(4, 22, 0, 0):
            Task_SpawnFromTable(&D_actor_361100_801637C8, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(4, 48, 0, 0):
            Task_SpawnFromTable(&D_shelter_r48_80182FAC, 0, 0, 0);
            break;
        case GAME_LOCATION_KEY(1, 20, 0, 0):
            func_mist_shooting_gallery_8017FBD8();
            break;
    }
}

void Gp_BlendRgb555Clut(u16* arg0, u16* arg1, s32 arg2, u16* arg3)
{
    s32 i;

    for (i = 0; i < 0x10; i++) {
        Gp_BlendRgb555(arg0, arg1, arg2, arg3);
        arg0++;
        arg1++;
        arg3++;
    }
}

static void Gp_BlendRgb555ClutMasked(u16* arg0, u16* arg1, s32 arg2, u16* arg3, s32 arg4)
{
    s32 i;

    for (i = 0; i < 0x10; i++) {
        if ((1 << i) & arg4) {
            Gp_BlendRgb555(arg0, arg1, arg2, arg3);
        }
        arg0++;
        arg1++;
        arg3++;
    }
}

static void func_800B28E0(Task* task)
{
    task->killCountdown = 0x20;
    task->state++;
    func_800B1EFC(task);
}

void func_800B2910(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80093A38;
    sp.funcs[arg0->state](arg0);
}

Task* func_800B2968(void)
{
    return Task_SpawnFromTable(&D_replay_bonus_80119218, 0, 0, 0);
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

static void Gp_AnimSeekSlotEx(AnimationContext* context, s32 arg1, s32 arg2, s32 arg3)
{
    AnimationSlot*         slot;
    const AnimationRecord* recs;
    u16                    segmentTime;

    slot = &context->slots[arg1];
    recs = slot->sets[arg2]->records;
    _animationSeekSlotWithBlend(context, arg1, arg2, arg3, 1);
    segmentTime    = recs[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan = segmentTime;
    slot->timeLeft = segmentTime;
}

void func_800B3AA4(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    AnimationSlot*         slot;
    AnimationSet**         sets;
    AnimationSet*          set;
    const AnimationRecord* recs;
    const AnimationRecord* rec;
    u16                    recordIndex;
    u16                    blendTime;
    u8                     recordFlags;
    s32                    setIndex;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 1) {
        u8 slotIndex;
        u8(*bufferedPose)[ANIMATION_POSE_BUFFER_BYTES];

        slotIndex        = arg1->trackIndex;
        setIndex         = arg3;
        context->slots   = arg1 - slotIndex;
        arg1->coordIndex = arg2;
        slotIndex        = arg1->trackIndex;
        // Capture this slot's encoded blend before replacing its destination keyframe.
        bufferedPose = context->poseBuffer + slotIndex;
        slot         = &context->slots[slotIndex];
        animationTickSlotPose(context, slotIndex, 0, bufferedPose);
        slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
        set                                = slot->sets[(u16)setIndex];
        recs                               = set->records;
        recordIndex                        = set->trackStartIndices[slot->trackIndex] + arg4;
        while ((s8)recs[recordIndex].flags < 0) {
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
        blendTime                          = arg5 << ANIMATION_TIME_FRACTION_BITS;
        slot->timeSpan                     = blendTime;
        slot->timeLeft                     = blendTime;
        slot->usesBufferedPose             = 0;
    } else {
        if (arg3 == 0) {
            arg3 = 1;
        } else if (arg3 < 0) {
            arg3 = -arg3;
        }

        arg1->rate                            = ANIMATION_RATE_ONE;
        arg1->timeLeft                        = 0;
        arg1->currentPose.indices.setIndex    = arg3;
        arg1->currentPose.indices.recordIndex = 0;
        arg1->coordIndex                      = arg2;
        arg1->trackIndex                      = arg2;
        arg1->nextPose.indices.setIndex       = arg3;
        sets                                  = context->sets;
        arg1->sets                            = sets;
        arg1->nextPose.indices.recordIndex    = sets[arg3]->trackStartIndices[arg1->trackIndex];
        arg1->currentPose.indices.recordIndex = arg1->sets[arg3]->trackStartIndices[arg1->trackIndex];
        recordFlags                           = arg1->sets[arg1->nextPose.indices.setIndex]->records[arg1->nextPose.indices.recordIndex].flags;
        arg1->status.fields.field_12          = 0;
        arg1->status.fields.flags             = 0;
        arg1->atEnd                           = 0;
        arg1->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_ENCODING_MASK;
    }
}

void Gp_AnimInitCtx(AnimationContext* ctx, void* sets, TmdObject* model, void* poses)
{
    ctx->sets       = sets;
    ctx->coords     = PARENT_OF(model, TmdAllocation, object)->coords;
    ctx->poseBuffer = poses;
    ctx->partCount  = model->partCount;
}

void Gp_AnimInitSlot(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3)
{
    AnimationSet** sets;
    u8             recordFlags;

    if (arg3 == 0) {
        arg3 = 1;
    } else if (arg3 < 0) {
        arg3 = -arg3;
    }

    arg1->rate                            = ANIMATION_RATE_ONE;
    arg1->timeLeft                        = 0;
    arg1->currentPose.indices.setIndex    = arg3;
    arg1->currentPose.indices.recordIndex = 0;
    arg1->coordIndex                      = arg2;
    arg1->trackIndex                      = arg2;
    arg1->nextPose.indices.setIndex       = arg3;
    sets                                  = context->sets;
    arg1->sets                            = sets;
    arg1->nextPose.indices.recordIndex    = sets[arg3]->trackStartIndices[arg1->trackIndex];
    arg1->currentPose.indices.recordIndex = arg1->sets[arg3]->trackStartIndices[arg1->trackIndex];
    recordFlags                           = arg1->sets[arg1->nextPose.indices.setIndex]->records[arg1->nextPose.indices.recordIndex].flags;
    arg1->status.fields.field_12          = 0;
    arg1->status.fields.flags             = 0;
    arg1->atEnd                           = 0;
    arg1->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_ENCODING_MASK;
}

void Gp_AnimTickSlot(AnimationContext* context, AnimationSlot* arg1)
{
    u8 idx;

    idx            = arg1->trackIndex;
    context->slots = arg1 - idx;
    animationTickSlotPose(context, idx, 0, 0);
}

void Gp_AnimTickSlot2(AnimationContext* context, AnimationSlot* arg1)
{
    u8 idx;

    idx            = arg1->trackIndex;
    context->slots = arg1 - idx;
    animationTickSlotPose(context, idx, 0, 0);
}

static void Gp_AnimTickSlot3(AnimationContext* context, AnimationSlot* arg1)
{
    u8 idx;

    idx            = arg1->trackIndex;
    context->slots = arg1 - idx;
    animationTickSlotPose(context, idx, 0, 0);
}

static void func_800B3E74(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3)
{
    const AnimationRecord* recs;
    u16                    segmentTime;

    recs = arg1->sets[arg3]->records;
    func_800B3AA4(context, arg1, arg2, arg3, 0, 8);
    segmentTime    = recs[arg1->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
    arg1->timeSpan = segmentTime;
    arg1->timeLeft = segmentTime;
}

static void func_800B3EE8(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3, s32 arg4)
{
    const AnimationRecord* recs;
    u16                    segmentTime;

    recs = arg1->sets[arg3]->records;
    func_800B3AA4(context, arg1, arg2, arg3, arg4, 8);
    segmentTime    = recs[arg1->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
    arg1->timeSpan = segmentTime;
    arg1->timeLeft = segmentTime;
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

void Gp_AnimResetSlotEx(AnimationContext* context, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    AnimationSlot* slot;
    AnimationSet** sets;
    u8             recordFlags;

    slot                                  = &context->slots[arg1];
    slot->rate                            = ANIMATION_RATE_ONE;
    slot->timeLeft                        = 0;
    slot->currentPose.indices.setIndex    = arg2;
    slot->currentPose.indices.recordIndex = 0;
    slot->coordIndex                      = arg4;
    slot->trackIndex                      = arg3;
    slot->nextPose.indices.setIndex       = arg2;
    sets                                  = context->sets;
    slot->sets                            = sets;
    slot->nextPose.indices.recordIndex    = sets[arg2]->trackStartIndices[slot->trackIndex];
    recordFlags                           = slot->sets[slot->nextPose.indices.setIndex]->records[slot->nextPose.indices.recordIndex].flags;
    slot->status.fields.flags             = 0;
    slot->atEnd                           = 0;
    slot->status.fields.field_12          = 0;
    slot->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_ENCODING_MASK;
}

static void Gp_AnimSeekSlot(AnimationContext* context, s32 arg1, s32 arg2)
{
    Gp_AnimSeekSlotEx(context, arg1, arg2, 0);
}

void animationSeekSlotWithBlend(AnimationContext* context, s32 slotIndex, s32 setIndex, s32 trackRecordOffset, s32 blendFrames)
{
    _animationSeekSlotWithBlend(context, slotIndex, (u16)setIndex, trackRecordOffset, blendFrames);
}

void Gp_AnimWritePoseBlend(AnimationContext* context, s32 arg1, AnimationPose* arg2, AnimationPose* arg3, s32 arg4,
                           s32 arg5)
{
    void**         scratch;
    AnimationPose* head;
    AnimationSlot* slot;
    GfxCoord*      dest;
    SVECTOR*       trans;
    SVECTOR*       rot;
    s32            idx;

    scratch                        = SCRATCH_HEAD_ADDR;
    slot                           = &context->slots[arg1];
    head                           = SCRATCH_HEAD_AT(scratch, AnimationPose);
    idx                            = slot->coordIndex;
    SCRATCH_HEAD_AT(scratch, void) = head - 1;
    dest                           = &context->coords[idx];
    trans                          = &head[-1].translation;
    if (slot->poseEncoding == ANIMATION_POSE_TRANSLATION_ROTATION) {
        gte_lddp(arg4);
        gte_ldsv(&arg2->translation);
        gte_gpf12();
        gte_lddp(arg5);
        gte_ldsv(&arg3->translation);
        gte_gpl12();
        gte_stsv(trans);
        dest->coord.t[0] = trans->vx;
        dest->coord.t[1] = trans->vy;
        dest->coord.t[2] = trans->vz;
    }
    gte_lddp(arg4);
    gte_ldsv(&arg2->rotation);
    gte_gpf12();
    gte_lddp(arg5);
    gte_ldsv(&arg3->rotation);
    gte_gpl12();
    rot = &head[-1].rotation;
    gte_stsv(rot);
    RotMatrix_gte(rot, &dest->coord);
    dest->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(AnimationPose);
}

void Gp_AnimWritePoseCopy(AnimationContext* context, s32 arg1, AnimationPose* arg2, AnimationPose* arg3, s32 arg4,
                          s32 arg5)
{
    void**         scratch;
    AnimationPose* head;
    AnimationSlot* slot;
    GfxCoord*      dest;
    SVECTOR*       rot;
    s32            idx;

    scratch                        = SCRATCH_HEAD_ADDR;
    slot                           = &context->slots[arg1];
    head                           = SCRATCH_HEAD_AT(scratch, AnimationPose);
    idx                            = slot->coordIndex;
    SCRATCH_HEAD_AT(scratch, void) = head - 1;
    dest                           = &context->coords[idx];
    if (slot->poseEncoding == ANIMATION_POSE_TRANSLATION_ROTATION) {
        dest->coord.t[0] = arg2->translation.vx;
        dest->coord.t[1] = arg2->translation.vy;
        dest->coord.t[2] = arg2->translation.vz;
    }
    gte_lddp(arg4);
    gte_ldsv(&arg2->rotation);
    gte_gpf12();
    gte_lddp(arg5);
    gte_ldsv(&arg3->rotation);
    gte_gpl12();
    rot = &head[-1].rotation;
    gte_stsv(rot);
    RotMatrix_gte(rot, &dest->coord);
    dest->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(AnimationPose);
}

void animationTickSlot(AnimationContext* context, s32 slotIndex)
{
    animationTickSlotPose(context, slotIndex, NULL, NULL);
}

void func_800B4538(AnimationContext* context, s32 arg1, AnimationPose* arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6)
{
    AnimationSlot*         slot;
    AnimationSet*          set;
    const AnimationRecord* recs;
    const AnimationRecord* rec;
    u16                    recordIndex;
    u16                    blendTime;
    u8(*bufferedPose)[ANIMATION_POSE_BUFFER_BYTES];

    // Capture this slot's encoded blend before replacing its destination keyframe.
    bufferedPose = context->poseBuffer + arg1;
    slot         = &context->slots[arg1];
    animationTickSlotPose(context, arg1, arg2, bufferedPose);
    slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
    set                                = slot->sets[arg3];
    recs                               = set->records;
    recordIndex                        = set->trackStartIndices[slot->trackIndex] + arg4;
    while ((s8)recs[recordIndex].flags < 0) {
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
    slot->nextPose.indices.setIndex    = arg3;
    blendTime                          = arg6 << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan                     = blendTime;
    slot->timeLeft                     = blendTime;
    slot->usesBufferedPose             = 0;
}

const AnimationRecord* Gp_AnimGetRec(AnimationContext* unusedContext, AnimationSlot* slot)
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

static void func_800B46A4(AnimationContext* unusedContext, AnimationSlot* arg1, u16 arg2, u16 arg3)
{
    const AnimationRecord* recs;
    const AnimationRecord* rec;

    recs = arg1->sets[arg2]->records;
    while ((s8)recs[arg3].flags < 0) {
        rec = recs - -(s32)arg3;
        if (rec->flags < ANIMATION_RECORD_END_THRESHOLD) {
            arg3 = rec->wordOffset;
            if (arg3 == arg1->nextPose.indices.recordIndex) {
                arg1->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            arg1->status.fields.flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            arg3                       = arg1->nextPose.indices.recordIndex;
            arg1->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            break;
        }
    }
    arg1->nextPose.indices.recordIndex = arg3;
    arg1->nextPose.indices.setIndex    = arg2;
}

static void func_800B4754(AnimationContext* unusedContext, AnimationSlot* arg1, u16 arg2, u16 arg3)
{
    u16 limit;

    limit = arg1->sets[arg2]->trackStartIndices[arg1->trackIndex];
    if (arg3 < limit) {
        arg3                       = limit;
        arg1->status.fields.flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
    }
    arg1->currentPose.indices.recordIndex = arg3;
    arg1->currentPose.indices.setIndex    = arg2;
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
    Gp_ResetLinkState();
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

void Gp_DrawFloorQuad(GfxCoord* arg0, u32 arg1, SVECTOR* arg2)
{
    _ActorRenderGroundShadowScratch* scratch;
    POLY_FT4*                        prim;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_ActorRenderGroundShadowScratch);
    if (arg2 == NULL) {
        scratch->vertices[0].vx = -(arg1 >> 1);
        scratch->vertices[0].vy = 0;
        scratch->vertices[0].vz = -(arg1 >> 1);
    } else {
        scratch->vertices[0].vx = arg2->vx - (arg1 >> 1);
        scratch->vertices[0].vy = arg2->vy;
        scratch->vertices[0].vz = arg2->vz - (arg1 >> 1);
    }
    scratch->vertices[3].vy = scratch->vertices[2].vy = scratch->vertices[1].vy = scratch->vertices[0].vy;
    scratch->vertices[1].vx = scratch->vertices[3].vx = scratch->vertices[0].vx + arg1;
    scratch->vertices[2].vx                           = scratch->vertices[0].vx;
    scratch->vertices[2].vz = scratch->vertices[3].vz = scratch->vertices[0].vz + arg1;
    scratch->vertices[1].vz                           = scratch->vertices[0].vz;
    actorRenderComposeCoord(arg0);
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    scratch->farthestDepth = 0;

    gte_ldv0(&scratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&scratch->screenXy[0]);
    gte_stdp(&scratch->depthCue);
    gte_stflg(&scratch->projectionFlags);
    gte_stszotz(&scratch->depth);
    if (scratch->depth > scratch->farthestDepth) {
        scratch->farthestDepth = scratch->depth;
    }

    gte_ldv0(&scratch->vertices[1]);
    gte_rtps();
    gte_stsxy(&scratch->screenXy[1]);
    gte_stdp(&scratch->depthCue);
    gte_stflg(&scratch->projectionFlags);
    gte_stszotz(&scratch->depth);
    if (scratch->depth > scratch->farthestDepth) {
        scratch->farthestDepth = scratch->depth;
    }

    gte_ldv0(&scratch->vertices[2]);
    gte_rtps();
    gte_stsxy(&scratch->screenXy[2]);
    gte_stdp(&scratch->depthCue);
    gte_stflg(&scratch->projectionFlags);
    gte_stszotz(&scratch->depth);
    if (scratch->depth > scratch->farthestDepth) {
        scratch->farthestDepth = scratch->depth;
    }

    gte_ldv0(&scratch->vertices[3]);
    gte_rtps();
    gte_stsxy(&scratch->screenXy[3]);
    gte_stdp(&scratch->depthCue);
    gte_stflg(&scratch->projectionFlags);
    gte_stszotz(&scratch->depth);
    if (scratch->depth > scratch->farthestDepth) {
        scratch->farthestDepth = scratch->depth;
    }

    if (scratch->projectionFlags >= 0) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        GPU_PRIMITIVE_XY_WORD(prim, 0) = scratch->screenXy[0];
        GPU_PRIMITIVE_XY_WORD(prim, 1) = scratch->screenXy[1];
        GPU_PRIMITIVE_XY_WORD(prim, 2) = scratch->screenXy[2];
        GPU_PRIMITIVE_XY_WORD(prim, 3) = scratch->screenXy[3];
        setUV4(prim, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        prim->tpage = 0x48;
        prim->g0    = 0xC0;
        prim->b0    = 0xC0;
        prim->r0    = 0xC0;
        prim->clut  = 0x4283;
        addPrim(&gGpuCurrentOt[scratch->farthestDepth >> 4], prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ActorRenderGroundShadowScratch);
}

static void func_800B51F4(Task* task)
{
    s32       count;
    s32       i;
    s32       x;
    s32       cx;
    s32       y;
    s32       mode;
    u8        flag;
    u16       flag2;
    s32       color;
    s32       right;
    TILE*     tile;
    DR_TPAGE* dr;
    DR_TPAGE* fadeDr;
    DR_STP*   stp;
    POLY_FT4* p0;
    POLY_FT4* p1;

    mode  = gDisplayState.drawBuffer;
    count = 1;
    x     = 0;
    y     = 0;
    cx    = 0;
    if (task->spawnArg1.value == 0x10) {
        count = 2;
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 1) {
        return;
    }

    if (task->spawnArg1.value & 1) {
        task->killCountdown++;
        if (task->killCountdown >= 0x3D) {
            color          = task->killCountdown - 0x3C;
            color         *= 8;
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x62);
            if (color >= 0x100) {
                color = 0xFF;
            }
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            tile->b0 = color;
            tile->g0 = color;
            tile->r0 = color;
            addPrim(&gGpuCurrentOt[1], tile);

            fadeDr         = gGpuPrimCursor;
            gGpuPrimCursor = fadeDr + 1;
            setlen(fadeDr, 1);
            fadeDr->code[0] = 0xE1000240;
            addPrim(&gGpuCurrentOt[1], fadeDr);
        }
    }

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000600;
    addPrim(&gGpuCurrentOt[0], dr);

    for (i = 0; i < count; i++) {
        p0             = gGpuPrimCursor;
        p1             = p0 + 1;
        gGpuPrimCursor = p0 + 2;
        setlen(p0, 9);
        setcode(p0, 0x2F);
        setlen(p1, 9);
        setcode(p1, 0x2F);
        p0->x0 = p0->x2 = x - (0xA0 + y);
        p0->x1 = p0->x3 = x;
        p0->y0 = p0->y1 = -0x78 - y;
        p0->y2 = p0->y3 = y + 0x77;
        right           = cx + 0x9F;
        p1->x0 = p1->x2 = x;
        p1->x1 = p1->x3 = right;
        flag            = mode;
        flag2           = flag;
        p1->y0 = p1->y1 = -0x78 - y;
        p1->y2 = p1->y3 = y + 0x77;
        if (flag2) {
            p0->tpage = 0x100;
            p0->u0 = p0->u2 = 0;
            p0->u1 = p0->u3 = 0xA0;
            p0->v0 = p0->v1 = 0;
            p0->v2 = p0->v3 = 0xEF;
            p1->tpage       = 0x102;
            p1->u0 = p1->u2 = 0x20;
            p1->u1 = p1->u3 = 0xBF;
            p1->v0 = p1->v1 = 0;
            p1->v2 = p1->v3 = 0xEF;
        } else {
            p0->tpage = 0x110;
            p0->u0 = p0->u2 = 0;
            p0->u1 = p0->u3 = 0xA0;
            p0->v0 = p0->v1 = 0x10;
            p0->v2 = p0->v3 = 0xFF;
            p1->tpage       = 0x112;
            p1->u0 = p1->u2 = 0x20;
            p1->u1 = p1->u3 = 0xBF;
            p1->v0 = p1->v1 = 0x10;
            p1->v2 = p1->v3 = 0xFF;
        }
        addPrim(&gGpuCurrentOt[0], p0);
        addPrim(&gGpuCurrentOt[0], p1);
    }

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000400;
    addPrim(&gGpuCurrentOt[0], dr);

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[0x3FF], stp);
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

void Gp_ReparentCoord(GfxCoord* arg0, GfxCoord* arg1)
{
    GfxCoord* dest;

    dest = arg1;
    if (dest->parent != arg0) {
        actorRenderComposeCoord(arg0);
        actorRenderComposeCoord(dest);
        dest->parent = arg0;
        gfxMakeRelativeTransform(&arg0->workm, &dest->workm, &dest->coord);
        dest->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

Enemy* Gp_FindWorkById(u16 arg0)
{
    Task*  head;
    Task*  iter;
    Enemy* enemy;
    s32    key;

    enemy = NULL;
    head  = (gameGetTaskSlot(GAME_TASK_SLOT_SCENE))->firstChild;
    if (head != NULL) {
        iter  = head;
        enemy = iter->spawnArg2.pointer;
        key   = arg0;
        if (enemy->placeKey != key) {
        loop:
            iter  = iter->nextSibling;
            enemy = NULL;
            if (iter != head) {
                enemy = iter->spawnArg2.pointer;
                if (enemy->placeKey != key) {
                    goto loop;
                }
            }
        }
    }
    return enemy;
}

void Gp_SetTmdBytes(TmdObject* arg0, s32 arg1, s32 arg2)
{
    arg0->texturePageOffset = arg1;
    arg0->clutRowOffset     = arg2;
    if (arg0->buffer != NULL) {
        tmdBuildBufferHalf(arg0);
        tmdBuildBufferHalf(arg0);
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

void func_800B5DB8(Task* arg0)
{
    TaskFunc funcs[2] = { Gp_BindSlot4, func_800B6398 };

    funcs[arg0->state](arg0);
}

/// Finds the placed actor whose `placeKey` equals the selector.
///
/// The message ID is not read. The walk follows the scene task's child ring.
s32 Gp_FindChildType9(Task* scene, s32 messageId, s32 selector, Task** reply)
{
    Task* head;
    Task* child;
    s32   result;

    *reply = NULL;
    head   = scene->firstChild;
    result = -1;
    if (head == NULL) {
        return result;
    }
    child = head;
    do {
        // The child publishes its enemy work through spawnArg2.
        scene = child->spawnArg2.pointer;
        if ((((Enemy*)scene)->workType >> 8) == SCENE_PLACED_ACTOR_BANK && ((Enemy*)scene)->placeKey == selector) {
            *reply = child;
            result = 0;
            break;
        }
        child = child->nextSibling;
    } while (child != head);
    return result;
}

/// Finds a child outside the placed-actor bank whose id byte equals the selector.
///
/// The id is the low byte of `placeKey`. The message ID is not read.
s32 Gp_FindChildExceptType9(Task* scene, s32 messageId, s32 selector, Task** reply)
{
    Task* head;
    Task* child;
    s32   result;

    *reply = NULL;
    head   = scene->firstChild;
    result = -1;
    if (head == NULL) {
        return result;
    }
    child = head;
    do {
        // The child publishes its enemy work through spawnArg2.
        scene = child->spawnArg2.pointer;
        if ((((Enemy*)scene)->workType >> 8) != SCENE_PLACED_ACTOR_BANK && (u8)((Enemy*)scene)->placeKey == selector) {
            *reply = child;
            result = 0;
            break;
        }
        child = child->nextSibling;
    } while (child != head);
    return result;
}

/// Runs the exit routine of every placed actor among the scene task's children.
///
/// The message ID and both argument words are not read. A child's exit routine
/// may unlink it, so the next sibling is taken before that call.
s32 Gp_ExitChildrenType9(Task* scene, s32 messageId, s32 firstArg, s32 secondArg)
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
        bank  = enemy->workType >> 8;
        next  = child->nextSibling;
        if (bank == SCENE_PLACED_ACTOR_BANK) {
            Task_CallExit(child);
        }
        child = next;
    } while (child != head);
    return 0;
}

/// Forwards one message to every placed actor among the scene task's children.
///
/// The message ID is not read. The second word is the message sent to each
/// child and the first word is that message's payload; the child's own second
/// argument is zero. A handler may unlink the child, so the next sibling is
/// taken before the send. Returns 0.
s32 Gp_SendMsgType9(Task* scene, s32 messageId, s32 payload, s32 childMessage)
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
        bank  = enemy->workType >> 8;
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
    Task_KillChildren(gameGetTaskSlot(GAME_TASK_SLOT_SCENE));
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

static void func_800B6094(Task* task)
{
    if (task->spawnArg1.value & 1) {
        task->killCountdown = 0;
    }
    task->state++;
}

void func_800B60C0(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80093A5C;
    sp.funcs[arg0->state](arg0);
}

void worldCollisionCalcContactViewOffset(SVECTOR* position, WorldCollisionContact* contact, SVECTOR* offset)
{
    _WorldCollisionContactViewOffsetScratch* scratch;
    SVECTOR*                                 delta;
    GfxCoord*                                viewCoord;
    s32                                      scale;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionContactViewOffsetScratch);
    delta   = &scratch->delta;
    // Measure separation after the signed-halfword coordinate truncation.
    delta->vx = contact->point.vx - position->vx;
    delta->vy = contact->point.vy - position->vy;
    delta->vz = contact->point.vz - position->vz;
    viewCoord = &gGfxViewCoord;
    scale     = SquareRoot0(Gfx_ApplyMatrixNoSf(delta, delta)) - contact->distance;
    if (scale >= 0) {
        scale = -scale;
    }
    // Rotate and scale the offset in the view coordinate frame.
    VectorNormalSS(&scratch->delta, &scratch->delta);
    TransposeMatrix(&viewCoord->workm, &scratch->transposedView);
    _gfxLoadRotSv(&scratch->transposedView, &scratch->delta);
    gte_rtv0();
    gte_stsv(delta);
    gte_lddp(scale);
    gte_ldsv(delta);
    gte_gpf12();
    gte_stsv(offset);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionContactViewOffsetScratch);
}

void Gp_FreeSlot4TmdBuffers(void)
{
    Task*      child;
    Task*      iter;
    TmdObject* obj;

    child = (gameGetTaskSlot(GAME_TASK_SLOT_SCENE))->firstChild;
    if (child != NULL) {
        iter = child;
        do {
            if (iter->bodyKind == TASK_BODY_TMD) {
                obj         = iter->extra.tmd;
                obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                tmdFreePrimitiveBuffer(obj);
            }
            iter = iter->nextSibling;
        } while (iter != child);
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

/// Three-entry dispatcher table: `Gp_EnemyWaitStart`, `Gp_EnemyWaitTick`, `enemyDestroy`.
static const EnemyTaskFuncTable3 Gp_EnemyWaitFuncs = { {
    Gp_EnemyWaitStart,
    Gp_EnemyWaitTick,
    enemyDestroy,
} };

static const TaskFuncTable3 Gp_StageLoadStates = { {
    Gp_StartStageLoad,
    Gp_FinishStageLoad,
    Gp_StageLoadState2,
} };

static const VECTOR D_80093A28 = { 0, -100, 0, 0 };

static const TaskFuncTable3 D_80093A38 = { {
    func_800B28E0,
    func_800B1EFC,
    Task_CallExit,
} };

/// Error message for animation tracks with unsupported pose encoding 2.
///
/// The NUL-terminated message occupies 21 bytes. The final three stored bytes
/// preserve the original read-only data and are not part of the printed text.
static const char _gAnimationUnsupportedPoseDiagnostic[24] = "ERROR: ex_pdriver_2\n\0\xB7\xB0\x34";

static const TaskFuncTable3 D_80093A5C = { {
    func_800B6094,
    func_800B51F4,
    Task_CallExit,
} };

/// Message table the scene task installs while it is the scene manager.
TaskMessageEntry Gp_Slot4MsgTable[5] = {
    { SCENE_MESSAGE_FIND_PLACED_ACTOR, Gp_FindChildType9 },
    { SCENE_MESSAGE_FIND_OTHER_CHILD, Gp_FindChildExceptType9 },
    { SCENE_MESSAGE_EXIT_PLACED_ACTORS, Gp_ExitChildrenType9 },
    { SCENE_MESSAGE_BROADCAST_TO_ACTORS, Gp_SendMsgType9 },
    { TASK_MESSAGE_TABLE_END, NULL },
};
AreaObjectStage Gp_Bit2Banks[6] = { { NULL, NULL }, { D_map_akropolis_8017A7FC, GameFlag_AcropolisBanks[0].header.objectStates }, { D_map_dryfield_8017A564, GameFlag_DryfieldBanks[0].header.objectStates }, { D_map_dryfield_full_8017A46C, GameFlag_DryfieldBanks[0].header.objectStates }, { D_map_shelter_8017A998, GameFlag_ShelterBanks[0].header.objectStates }, { D_map_neo_ark_8017A6EC, GameFlag_NeoArkBanks[0].header.objectStates } };

void Gp_BindSlot4(Task* task)
{
    gameSetTaskSlot(task, GAME_TASK_SLOT_SCENE);
    task->msgTable = Gp_Slot4MsgTable;
    task->state++;
}

void func_800B6398(Task* task)
{
    Gp_DrawTargetCursor();
}
