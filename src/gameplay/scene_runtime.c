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

/// Lowest unsigned record-flags value that ends a forward animation-track walk.
///
/// Values 0xC0..0xFF have both control and stop bits set; the low six bits do
/// not affect termination. Values 0x80..0xBF instead jump to `wordOffset`.
/// An end record keeps the slot's previously selected next record index and
/// ignores its own `wordOffset` and `durationFrames`. This is a threshold,
/// so comparison with `AnimationRecord.flags` must retain its unsigned value.
enum { ANIMATION_RECORD_END_THRESHOLD = ANIMATION_RECORD_CONTROL | ANIMATION_RECORD_STOP };

// Playback timing in sixteenths of a normal-rate frame.
enum {
    ANIMATION_TIME_FRACTION_BITS   = 4,
    ANIMATION_TIME_UNITS_PER_FRAME = 1 << ANIMATION_TIME_FRACTION_BITS
};

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

/// Relative-rotation cache decision for a buffered pose blend.
enum {
    ANIMATION_ROTATION_DELTA_REUSE = 0
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

/// Scratch reservation for advancing a slot and dispatching its pose blend.
typedef struct {
    s32                    field_0; // Not accessed by playback; role unproven
    _AnimationBlendRequest request; // Inputs and destinations for this tick
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

/// 8-byte mask/flag record. `Gp_SndMaskTable` is a 0-terminated table of these.
/// `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks` walk it: if `arg0 & mask`, apply `flags`
/// to `SndEvt_EnqueueType7` / `SndBank_SetEnableFlags`.
typedef struct _GpSndMaskRec {
    /* 0x0 */ s32 mask;
    /* 0x4 */ s32 flags;
} GpSndMaskRec;
STATIC_ASSERT_SIZEOF(GpSndMaskRec, 8);

/// 8-byte RGB555-unpacked vector. `Gp_BlendRgb555` allocates three of
/// these (0x18 bytes) from the scratch stack: src0, src1, then the GTE
/// lerp result. Channels are 5-bit values shifted left 7.
typedef struct _GpRgbScratch {
    /* 0x00 */ u16 r;
    /* 0x02 */ u16 g;
    /* 0x04 */ u16 b;
    /* 0x06 */ u16 pad;
} GpRgbScratch;
STATIC_ASSERT_SIZEOF(GpRgbScratch, 8);

/// 0x28-byte scratch from the scratch stack used by `worldCollisionCalcContactViewOffset`.
/// `vec` is the `arg1->pos - arg0` delta (normalized in place);
/// `mtx` is the transpose of `gGfxViewCoord.workm`.
typedef struct _GpDirScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ MATRIX  mtx;
} GpDirScratch;
STATIC_ASSERT_SIZEOF(GpDirScratch, 0x28);

/// 0x40-byte scratch from the scratch stack used by `Gp_DrawFloorQuad`.
/// `vec[]` holds the four corners of an axis-aligned XZ square of side
/// `size` anchored at the caller's origin; each is projected with a
/// separate RTPS. `dp` / `flag` / `otz` receive `gte_stdp` / `gte_stflg` /
/// `gte_stszotz` of the current corner, `sxy0`..`sxy3` the projected screen
/// positions copied into the `POLY_FT4`, and `maxotz` the running maximum
/// `otz` used as the OT bucket.
typedef struct _GpFloorQuadScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     dp;
    /* 0x28 */ s32     flag;
    /* 0x2C */ DVECTOR sxy0;
    /* 0x30 */ DVECTOR sxy1;
    /* 0x34 */ DVECTOR sxy2;
    /* 0x38 */ DVECTOR sxy3;
    /* 0x3C */ s32     maxotz;
} GpFloorQuadScratch;
STATIC_ASSERT_SIZEOF(GpFloorQuadScratch, 0x40);

u8* D_80114D10;

u16 D_80114D14[2];

s16 D_80114D18;

s16 D_80114D1A;

s16 D_80114D1C;

s32 D_80114D20;

/// 0-terminated `GpSndMaskRec` table walked by `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks`.
extern GpSndMaskRec Gp_SndMaskTable[];

/// Printed when an enemy's work block cannot be allocated.
static const char Gp_StrNewEnemyNull[];

/// Three-entry dispatcher table: `Gp_EnemyWaitStart`, `Gp_EnemyWaitTick`, `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

static const TaskFuncTable3 Gp_StageLoadStates;

static const VECTOR D_80093A28;

static const TaskFuncTable3 D_80093A38;

/// "ERROR: ex_pdriver_2\n". The three bytes after the terminator are not zero:
/// the original toolchain left them in the alignment gap.
static const char D_80093A44[24];

static const TaskFuncTable3 D_80093A5C;

typedef struct {
    s32 id;
    union {
        s32 (*find)(Task*, Task*, s32, Task**);
        s32 (*exit)(Task*);
        s32 (*send)(Task*, s32, s32, s32);
    } handler;
} GpSlot4MessageEntry;

extern GpSlot4MessageEntry Gp_Slot4MsgTable[5];

s32 Gp_FindChildType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3);

s32 Gp_FindChildExceptType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3);

s32 Gp_ExitChildrenType9(Task* arg0);

s32 Gp_SendMsgType9(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

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

static void _animationBlendTranslationRotation(_AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot);

static void _animationBlendPackedRotation(_AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot);

static void Gp_AnimAdvanceSlot(AnimationContext* context, s32 arg1);

static inline void _gpAnimSeekSlot(AnimationContext* context, s32 arg1, u16 arg2, s32 arg3, s32 arg4);

static void Gp_AnimSeekSlotEx(AnimationContext* context, s32 arg1, s32 arg2, s32 arg3);

static void Gp_AnimTickSlot3(AnimationContext* context, AnimationSlot* arg1);

static void func_800B3E74(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3);

static void func_800B3EE8(AnimationContext* context, AnimationSlot* arg1, s32 arg2, s32 arg3, s32 arg4);

static void Gp_AnimSeekSlot(AnimationContext* context, s32 arg1, s32 arg2);

static void func_800B46A4(AnimationContext* unusedContext, AnimationSlot* arg1, u16 arg2, u16 arg3);

static void func_800B4754(AnimationContext* unusedContext, AnimationSlot* arg1, u16 arg2, u16 arg3);

static void func_800B51F4(Task* task);

static void Gp_SetCurAreaFlag2(s32 useSavedPoses);

static GpAreaObj* Gp_GetAreaObj(GameLocationKey* key);

static void _areaPrepareSpawnState(GameLocationKey* key, GpAreaObj* areaState);

static GpAreaTmdRec* Gp_GetNestedAreaObj(GameLocationKey* key);

static void Gp_KillSlot4Children(void);

static void func_800B6014(void);

static void func_800B6094(Task* task);

/// The 2-bit state of entry `arg0` in the current stage's `Gp_Bit2Banks` flags.
static inline s32 _gpGetCurBit2Flag(s32 arg0);

/// Finds the record in the 0xFFFF-terminated `desc` table whose id is
/// `place->field_2` and spawns that enemy at `place`.
static inline void _gpSpawnPlace(GpEnemyDesc* desc, GpBit2Rec* place);

/// Inline form of `Gp_GetRelatedQty`: the most of a related item weapon
/// `item` can hold, from bank `bank`'s table, or 0 for a non-weapon id.
static inline s32 _gpRelatedQty(s32 item, s32 bank);

/// How much of `item` the rows `scan` selects hold: the stack count for
/// stackable ids (0xA0 and up), otherwise 1 if any row carries it and 0 if not.
static inline s16 _gpScanHeldQty(InventoryItemRow* table, InventoryItemRange* scan, s32 item);

void Gp_BindSlot4(Task* task);

void func_800B6398(Task* task);

extern TaskDesc D_80115D9C[];

extern TaskDesc D_80119218[];

extern TaskDesc D_8011922C[];

extern TaskDesc D_801637C8[];

GpSndMaskRec Gp_SndMaskTable[7] = {
    { 1, 0 },
    { 4, SOUND_SCRIPT_REQUEST_TYPE_1 },
    { 8, 0x50000000 },
    { 2, 0x20000000 },
    { 16, 0x40000000 },
    { 32, -0x80000000 },
    { 0, 0 },
};
TaskDesc D_8010D1FC = { { { TASK_BODY_NONE, 192 } }, func_800B06F0, { NULL } };

static const char           D_80093A44[];
static const TaskFuncTable3 Gp_StageLoadStates;
static const VECTOR         D_80093A28;
static const TaskFuncTable3 D_80093A38;
static const TaskFuncTable3 D_80093A5C;

static const char Gp_StrNewEnemyNull[];

static const GpEnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

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

    Mem_Set(p->sceneImageHeaders, 0, sizeof(p->sceneImageHeaders));
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
            StreamSlot*   sceneStream;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

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
                        SndEvt_EnqueueType7(entry->flags, 0);
                        SndBank_SetEnableFlags(0, entry->flags);
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

            if (CdAudio_Phase.field_0 != 3) {
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
                SndEvt_EnqueueType6(0, 0, 0);
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
            StreamSlot*   sceneStream;
            s32           i;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

            if (CdAudio_Phase.field_1 != 4) {
                break;
            }
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0) {
                SndEvt_EnqueueType6(0, 0, 0);
            }
            Mem_Set(&p->activeRequest, 0, sizeof(p->activeRequest));
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
                        SndBank_SetEnableFlags(1, entry->flags);
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
            s32           i;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

            if (CdAudio_Phase.field_4 != 0xA) {
                break;
            }
            i        = 0;
            maskbits = p->sceneStream->data.scene.soundBankMask;
            if (Gp_SndMaskTable[0].mask != 0) {
                bits = maskbits;
                do {
                    entry = &Gp_SndMaskTable[(u16)i];
                    if (bits & entry->mask) {
                        SndBank_SetEnableFlags(1, entry->flags);
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
    s32           i;
    s32           bits;
    GpSndMaskRec* entry;

    i = 0;
    if (Gp_SndMaskTable[0].mask != 0) {
        bits = arg0;
        do {
            entry = &Gp_SndMaskTable[(u16)i];
            if (bits & entry->mask) {
                SndEvt_EnqueueType7(entry->flags, 0);
                SndBank_SetEnableFlags(0, entry->flags);
            }
            i++;
        } while (Gp_SndMaskTable[(u16)i].mask != 0);
    }
}

void Gp_ApplySndBankMasks(u16 arg0)
{
    s32           i;
    s32           bits;
    GpSndMaskRec* entry;

    i = 0;
    if (Gp_SndMaskTable[0].mask != 0) {
        bits = arg0;
        do {
            entry = &Gp_SndMaskTable[(u16)i];
            if (bits & entry->mask) {
                SndBank_SetEnableFlags(1, entry->flags);
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

void Gp_DestroyEnemy(Enemy* enemy, Task* task)
{
    Gp_UnlinkNode(&enemy->node);
    memFree(enemy);
    taskKill(task);
}

void Gp_EnemyTaskExit(Task* task)
{
    Enemy* enemy;

    enemy = task->spawnArg2.pointer;
    Gp_UnlinkNode(&enemy->node);
    memFree(enemy);
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
        Gp_UpdateCoord(arg1);
        dest->workm = arg1->workm;
        gte_SetRotMatrix(&arg1->workm);
        gte_SetTransMatrix(&arg1->workm);
        gte_ldv0(arg2);
        gte_rtv0tr();
        gte_stlvnl(dest->workm.t);
        Gp_WorldToLocal(&world->workm, &dest->workm, &dest->coord);
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

    task->exitCallback      = Gp_EnemyTaskExit;
    task->spawnArg2.pointer = enemy;
    enemy->task             = task;
    enemy->coord            = &gGfxViewCoord;
    if (parent != NULL) {
        Task_Reparent(parent->task, task);
    } else {
        Task_Reparent(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), enemy->task);
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
    GpEnemyTaskFuncTable3 sp;

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
        CdCmd_Enqueue(0x21, param1, param2);
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
        SndEvt_EnqueueType7(0xE0000000, 8);
        flag      = 1;
        param1[3] = 0;
        param1[2] = 5;
        param1[0] = arg0;
        param2[0] = flag;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
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
    s32           i;
    u8            param1[8];
    u8            param2[8];
    FsFolderSlot* table;
    s32           fileId;

    if (Midi_IsBusy(0) == 0) {
        gDisplayState.suppressDisconnectPause = 1;
        i                                     = 0;
        table                                 = D_8006C338;
        do {
            table[(u8)i].field_0 = 0;
            i++;
        } while ((u8)i < 0x32);

        fileId = 0xA;
        if (gGameSession->restartMode != GAME_SESSION_RESTART_ENDING) {
            param1[2] = 4;
            param1[0] = 0x62;
            param1[3] = 0;
            param2[0] = 1;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
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
            Task_SpawnFromTable(D_8011922C, 0, 0, 0);
            taskKill(task);
        } else {
            task->spawnArg2.pointer = Task_SpawnFromTable(D_80115D9C, 0, 0, 0);
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

SVECTOR* Gp_ExtractEuler(SVECTOR* arg0, MATRIX* arg1)
{
    SVECTOR ang0;
    SVECTOR ang1;
    s32     sin0;
    s32     cos0;
    s32     sin1;
    s32     cos1;

    ang0.vx = -ratan2(arg1->m[1][2], arg1->m[2][2]);
    ang1.vx = (ang0.vx <= 0) ? ang0.vx + 0x800 : ang0.vx - 0x800;

    sin0 = rsin(ang0.vx);
    cos0 = rcos(ang0.vx);
    sin1 = rsin(ang1.vx);
    cos1 = rcos(ang1.vx);

    ang0.vy = ratan2(arg1->m[0][2], (arg1->m[2][2] * cos0) / 4096 - (arg1->m[1][2] * sin0) / 4096);
    ang1.vy = ratan2(arg1->m[0][2], (arg1->m[2][2] * cos1) / 4096 - (arg1->m[1][2] * sin1) / 4096);

    ang0.vz = ratan2((arg1->m[1][0] * cos0) / 4096 + (arg1->m[2][0] * sin0) / 4096,
                     (arg1->m[1][1] * cos0) / 4096 + (arg1->m[2][1] * sin0) / 4096);
    ang1.vz = ratan2((arg1->m[1][0] * cos1) / 4096 + (arg1->m[2][0] * sin1) / 4096,
                     (arg1->m[1][1] * cos1) / 4096 + (arg1->m[2][1] * sin1) / 4096);

    sin0 = ABS(ang0.vx) + ABS(ang0.vy) + ABS(ang0.vz);
    cos0 = ABS(ang1.vx) + ABS(ang1.vy) + ABS(ang1.vz);
    if (sin0 < cos0) {
        *arg0 = ang0;
    } else {
        *arg0 = ang1;
    }
    return arg0;
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

void func_800B17D4(Task* arg0, Task* arg1, GpHeadAim* arg2)
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
    s32       inited;
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

    i          = 0;
    m0         = &mtx0;
    probe      = D_80093A28;
    yawLimit   = arg2->yawLimit;
    pitchLimit = arg2->pitchLimit;
    rate       = arg2->rate;
    inited     = arg2->inited;

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

    if (inited != 0) {
        if (ABS(ang.vx - arg2->lastPitch) > 0x800) {
            while (ang.vx >= 0x800) {
                ang.vx -= 0x1000;
            }
            while (ang.vx < -0x800) {
                ang.vx += 0x1000;
            }
        }
    } else {
        arg2->inited = 1;
    }
    arg2->lastPitch = ang.vx;

    base = arg0->extra.tmd->coords;
    rec  = base + 4;
    Gp_ExtractEuler(&euler, &base[4].coord);

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
    u8*           head;
    GpRgbScratch* c0;
    GpRgbScratch* c1;
    GpRgbScratch* out;
    u16           color;
    u16           packed;

    head                               = SCRATCH_STACK_CURSOR(u8);
    c0                                 = (GpRgbScratch*)(head - 0x18);
    SCRATCH_STACK_CURSOR(GpRgbScratch) = c0;

    color = *arg0;
    c0->b = color;
    c0->g = color;
    c0->r = (color & 0x1F) << 7;
    c0->g = (c0->g << 2) & 0xF80;
    c0->b = (c0->b >> 3) & 0xF80;

    c1    = (GpRgbScratch*)(head - 0x10);
    color = *arg1;
    c1->b = color;
    c1->g = color;
    c1->r = (color & 0x1F) << 7;
    c1->g = (c1->g << 2) & 0xF80;
    c1->b = (c1->b >> 3) & 0xF80;

    gte_lddp(arg2);
    gte_ldsv(c0);
    gte_gpf12();
    gte_lddp(0x1000 - arg2);
    gte_ldsv(c1);
    gte_gpl12();
    out = (GpRgbScratch*)(head - 8);
    gte_stsv(out);

    packed = ((out->b >> 2) & 0x3E0) | ((out->g >> 7) & 0x1F);
    packed = (packed << 5) | ((out->r >> 7) & 0x1F);
    *arg3  = packed;
    if ((s16)*arg0 < 0 || (s16)*arg1 < 0) {
        *arg3 = packed | 0x8000;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
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
            Task_SpawnFromTable(D_801637C8, 0, 0, 0);
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
    return Task_SpawnFromTable(D_80119218, 0, 0, 0);
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

/// Blends encoding 1, writing a local transform and optionally a compact pose.
static void _animationBlendTranslationRotation(_AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot)
{
    _AnimationBlendScratch*    scratch;
    const AnimationPackedPose* encodedPose;
    s32                        currentWeight;

    if (slot->timeSpan != 0) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(_AnimationBlendScratch);
        if (request->currentPose.bytes != request->nextPose.bytes) {
            // Store the scaled remaining time, then replace it with the truncated quotient.
            currentWeight          = slot->timeLeft << ANIMATION_BLEND_FRACTION_BITS;
            scratch->currentWeight = currentWeight;
            currentWeight          = currentWeight / slot->timeSpan;
            scratch->currentWeight = currentWeight;
            scratch->nextWeight    = ONE - currentWeight;
        } else {
            scratch->currentWeight = 0;
            scratch->nextWeight    = ONE;
        }
        // The compact translation occupies six bytes; the GTE load reads three halfwords.
        gte_lddp(scratch->currentWeight);
        gte_ldsv(request->currentPose.translationRotation);
        gte_gpf12();
        gte_lddp(scratch->nextWeight);
        gte_ldsv(request->nextPose.translationRotation);
        gte_gpl12();
        gte_stsv(&scratch->translation);
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

/// Blends encoding 4 without changing the part's local translation.
static void _animationBlendPackedRotation(_AnimationBlendRequest* request, GfxCoord* coord, AnimationSlot* slot)
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
    s32                            currentWeight;

    if (slot->timeSpan != 0) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(_AnimationBlendScratch);
        if (request->currentPose.bytes != request->nextPose.bytes) {
            // Store the scaled remaining time, then replace it with the truncated quotient.
            currentWeight          = slot->timeLeft << ANIMATION_BLEND_FRACTION_BITS;
            scratch->currentWeight = currentWeight;
            currentWeight          = currentWeight / slot->timeSpan;
            scratch->currentWeight = currentWeight;
            scratch->nextWeight    = ONE - currentWeight;
        } else {
            scratch->currentWeight = 0;
            scratch->nextWeight    = ONE;
        }
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

    slot        = &context->slots[arg1];
    slot->flags = 0;
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
                        slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
                    }
                    slot->flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
                } else {
                    recordIndex  = slot->nextPose.indices.recordIndex;
                    slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
                    break;
                }
            }
            slot->nextPose.indices.recordIndex = recordIndex;
            slot->nextPose.indices.setIndex    = setIndex;
            if (slot->flags & ANIMATION_SLOT_BOUNDARY_MASK) {
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
    // Encoding 2 reports a diagnostic; its pose layout is unproven.
    enum { ANIMATION_POSE_UNSUPPORTED = 2 };

    _AnimationTickScratch* scratch;
    AnimationSlot*         slot;
    GfxCoord*              coord;
    AnimationSet*          set;
    const AnimationRecord* records;
    const AnimationRecord* record;
    const u8*              poseBytes;
    u16                    nextRecordIndex;
    u16                    previousRecordIndex;
    s32                    nextSetIndex;
    s32                    previousSetIndex;
    u16                    firstRecordIndex;
    u16                    segmentDuration;
    s16                    remainingTime;
    s32                    poseEncoding;
    u16                    decrementedTime;

    slot  = &context->slots[slotIndex];
    coord = &context->coords[slot->coordIndex];
    SCRATCH_STACK_RESERVE_BLOCK(_AnimationTickScratch);
    scratch     = SCRATCH_STACK_CURSOR(_AnimationTickScratch);
    slot->flags = 0;
    // A latched hold whose endpoints still agree reports only the hold and does not step time.
    if (slot->atEnd == 1) {
        if (slot->nextPose.key == slot->currentPose.key) {
            slot->flags = ANIMATION_SLOT_SETTLED;
        } else {
            slot->atEnd = 0;
        }
    } else {
        if (gGameSession->deathVariant != 0) {
            decrementedTime = slot->timeLeft - 1;
            slot->timeLeft  = decrementedTime - ((slot->rate - 1) >> 1);
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
            while ((s8)records[nextRecordIndex].flags < 0) {
                record = records - -(s32)nextRecordIndex;
                if (record->flags < ANIMATION_RECORD_END_THRESHOLD) {
                    nextRecordIndex = record->wordOffset;
                    if (nextRecordIndex == slot->nextPose.indices.recordIndex) {
                        slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
                    }
                    slot->flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
                } else {
                    nextRecordIndex = slot->nextPose.indices.recordIndex;
                    slot->flags    |= ANIMATION_SLOT_REACHED_BOUNDARY;
                    break;
                }
            }
            slot->nextPose.indices.setIndex    = nextSetIndex;
            slot->nextPose.indices.recordIndex = nextRecordIndex;
            records                            = slot->sets[slot->nextPose.indices.setIndex]->records;
            segmentDuration                    = records[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
            slot->timeSpan                     = segmentDuration;
            slot->timeLeft                    += segmentDuration;
        }
        if (slot->flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            // The forward walk reached a boundary: latch the hold.
            slot->atEnd  = 1;
            slot->flags |= ANIMATION_SLOT_SETTLED;
        } else {
            slot->atEnd = 0;
        }
    } else if (slot->timeSpan < remainingTime) {
        slot->field_A = 0;
        while (slot->timeLeft > slot->timeSpan) {
            slot->timeLeft     -= slot->timeSpan;
            slot->nextPose.key  = slot->currentPose.key;
            previousSetIndex    = slot->currentPose.indices.setIndex;
            previousRecordIndex = slot->currentPose.indices.recordIndex - 1;
            firstRecordIndex    = slot->sets[previousSetIndex]->trackStartIndices[slot->trackIndex];
            if (previousRecordIndex < firstRecordIndex) {
                previousRecordIndex = firstRecordIndex;
                slot->flags        |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            slot->currentPose.indices.recordIndex = previousRecordIndex;
            slot->currentPose.indices.setIndex    = previousSetIndex;
            records                               = slot->sets[slot->nextPose.indices.setIndex]->records;
            segmentDuration                       = records[slot->nextPose.indices.recordIndex].durationFrames << ANIMATION_TIME_FRACTION_BITS;
            slot->timeSpan                        = segmentDuration;
        }
        if (slot->flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            // The reverse walk reached the track start: latch the hold.
            slot->atEnd  = 1;
            slot->flags |= ANIMATION_SLOT_SETTLED;
        } else {
            slot->atEnd = 0;
        }
    }

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
    // Rebuild the cache only when this tick enters buffered endpoints.
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
            printf(D_80093A44);
            break;
        case ANIMATION_POSE_PACKED_ROTATION:
            _animationBlendPackedRotation(&scratch->request, coord, slot);
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_AnimationTickScratch);
}

static inline void _gpAnimSeekSlot(AnimationContext* context, s32 arg1, u16 arg2, s32 arg3, s32 arg4)
{
    AnimationSlot*         slot;
    AnimationSet*          set;
    const AnimationRecord* recs;
    const AnimationRecord* rec;
    u16                    recordIndex;
    u16                    blendTime;
    s32                    poseBufferOffset;

    poseBufferOffset = arg1 * ANIMATION_POSE_BUFFER_BYTES;
    slot             = &context->slots[arg1];
    animationTickSlotPose(context, arg1, 0, (u8*)context->poseBuffer + poseBufferOffset);
    slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
    set                                = slot->sets[arg2];
    recs                               = set->records;
    recordIndex                        = set->trackStartIndices[slot->trackIndex] + arg3;
    while ((s8)recs[recordIndex].flags < 0) {
        rec = recs - -(s32)recordIndex;
        if (rec->flags < ANIMATION_RECORD_END_THRESHOLD) {
            recordIndex = rec->wordOffset;
            if (recordIndex == slot->nextPose.indices.recordIndex) {
                slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            slot->flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            recordIndex  = slot->nextPose.indices.recordIndex;
            slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            break;
        }
    }
    slot->nextPose.indices.recordIndex = recordIndex;
    slot->nextPose.indices.setIndex    = arg2;
    blendTime                          = arg4 << ANIMATION_TIME_FRACTION_BITS;
    slot->timeSpan                     = blendTime;
    slot->timeLeft                     = blendTime;
    slot->usesBufferedPose             = 0;
}

static void Gp_AnimSeekSlotEx(AnimationContext* context, s32 arg1, s32 arg2, s32 arg3)
{
    AnimationSlot*         slot;
    const AnimationRecord* recs;
    u16                    segmentTime;

    slot = &context->slots[arg1];
    recs = slot->sets[arg2]->records;
    _gpAnimSeekSlot(context, arg1, arg2, arg3, 1);
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
        u8  slotIndex;
        s32 poseBufferOffset;

        slotIndex        = arg1->trackIndex;
        setIndex         = arg3;
        context->slots   = arg1 - slotIndex;
        arg1->coordIndex = arg2;
        slotIndex        = arg1->trackIndex;
        poseBufferOffset = slotIndex * ANIMATION_POSE_BUFFER_BYTES;
        slot             = &context->slots[slotIndex];
        animationTickSlotPose(context, slotIndex, 0, (u8*)context->poseBuffer + poseBufferOffset);
        slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
        set                                = slot->sets[(u16)setIndex];
        recs                               = set->records;
        recordIndex                        = set->trackStartIndices[slot->trackIndex] + arg4;
        while ((s8)recs[recordIndex].flags < 0) {
            rec = recs - -(s32)recordIndex;
            if (rec->flags < ANIMATION_RECORD_END_THRESHOLD) {
                recordIndex = rec->wordOffset;
                if (recordIndex == slot->nextPose.indices.recordIndex) {
                    slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
                }
                slot->flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
            } else {
                recordIndex  = slot->nextPose.indices.recordIndex;
                slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
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

        arg1->rate                            = ANIMATION_TIME_UNITS_PER_FRAME;
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
        arg1->field_12                        = 0;
        arg1->flags                           = 0;
        arg1->atEnd                           = 0;
        arg1->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_KIND_MASK;
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

    arg1->rate                            = ANIMATION_TIME_UNITS_PER_FRAME;
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
    arg1->field_12                        = 0;
    arg1->flags                           = 0;
    arg1->atEnd                           = 0;
    arg1->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_KIND_MASK;
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

void Gp_AnimInitCtxSlots(AnimationContext* ctx, void* sets, TmdObject* model, void* poses, AnimationSlot* slots)
{
    ctx->sets       = sets;
    ctx->coords     = PARENT_OF(model, TmdAllocation, object)->coords;
    ctx->poseBuffer = poses;
    ctx->partCount  = model->partCount;
    ctx->slots      = slots;
}

void func_800B3F84(AnimationContext* context, void* arg1, TmdObject* arg2, void* arg3, AnimationSlot* arg4)
{
    Gp_AnimInitCtxSlots(context, arg1, arg2, arg3, arg4);
}

void Gp_AnimResetSlot(AnimationContext* context, s32 arg1, s32 arg2)
{
    AnimationSlot* slot;
    AnimationSet** sets;
    u8             recordFlags;

    slot                                  = &context->slots[arg1];
    slot->rate                            = ANIMATION_TIME_UNITS_PER_FRAME;
    slot->timeLeft                        = 0;
    slot->currentPose.indices.setIndex    = arg2;
    slot->currentPose.indices.recordIndex = 0;
    slot->coordIndex                      = arg1;
    slot->trackIndex                      = arg1;
    slot->nextPose.indices.setIndex       = arg2;
    sets                                  = context->sets;
    slot->sets                            = sets;
    slot->nextPose.indices.recordIndex    = sets[arg2]->trackStartIndices[slot->trackIndex];
    recordFlags                           = slot->sets[slot->nextPose.indices.setIndex]->records[slot->nextPose.indices.recordIndex].flags;
    slot->flags                           = 0;
    slot->atEnd                           = 0;
    slot->field_12                        = 0;
    slot->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_KIND_MASK;
}

void Gp_AnimResetSlotEx(AnimationContext* context, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    AnimationSlot* slot;
    AnimationSet** sets;
    u8             recordFlags;

    slot                                  = &context->slots[arg1];
    slot->rate                            = ANIMATION_TIME_UNITS_PER_FRAME;
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
    slot->flags                           = 0;
    slot->atEnd                           = 0;
    slot->field_12                        = 0;
    slot->poseEncoding                    = recordFlags & ANIMATION_RECORD_POSE_KIND_MASK;
}

static void Gp_AnimSeekSlot(AnimationContext* context, s32 arg1, s32 arg2)
{
    Gp_AnimSeekSlotEx(context, arg1, arg2, 0);
}

void func_800B4114(AnimationContext* context, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    _gpAnimSeekSlot(context, arg1, arg2, arg3, arg4);
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

void Gp_AnimTickIndex(AnimationContext* context, s32 arg1)
{
    animationTickSlotPose(context, arg1, 0, 0);
}

void func_800B4538(AnimationContext* context, s32 arg1, AnimationPose* arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6)
{
    AnimationSlot*         slot;
    AnimationSet*          set;
    const AnimationRecord* recs;
    const AnimationRecord* rec;
    u16                    recordIndex;
    u16                    blendTime;
    s32                    poseBufferOffset;

    poseBufferOffset = arg1 * ANIMATION_POSE_BUFFER_BYTES;
    slot             = &context->slots[arg1];
    animationTickSlotPose(context, arg1, arg2, (u8*)context->poseBuffer + poseBufferOffset);
    slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
    set                                = slot->sets[arg3];
    recs                               = set->records;
    recordIndex                        = set->trackStartIndices[slot->trackIndex] + arg4;
    while ((s8)recs[recordIndex].flags < 0) {
        rec = recs - -(s32)recordIndex;
        if (rec->flags < ANIMATION_RECORD_END_THRESHOLD) {
            recordIndex = rec->wordOffset;
            if (recordIndex == slot->nextPose.indices.recordIndex) {
                slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            slot->flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            recordIndex  = slot->nextPose.indices.recordIndex;
            slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
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
                arg1->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            arg1->flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            arg3         = arg1->nextPose.indices.recordIndex;
            arg1->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
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
        arg3         = limit;
        arg1->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
    }
    arg1->currentPose.indices.recordIndex = arg3;
    arg1->currentPose.indices.setIndex    = arg2;
}

void Gp_AnimPlaySlot(AnimationContext* context, s32 arg1, AnimationPose* arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6,
                     void* arg7)
{
    AnimationSlot*         slot;
    AnimationSet*          set;
    const AnimationRecord* recs;
    const AnimationRecord* rec;
    u16                    recordIndex;
    u16                    blendTime;
    s32                    poseBufferOffset;

    poseBufferOffset = arg1 * ANIMATION_POSE_BUFFER_BYTES;
    slot             = &context->slots[arg1];
    animationTickSlotPose(context, arg1, arg2, (u8*)context->poseBuffer + poseBufferOffset);
    slot->currentPose.indices.setIndex = ANIMATION_SET_BUFFERED_POSE;
    if (arg7 != NULL) {
        context->sets = arg7;
        slot->sets    = arg7;
    }
    set  = slot->sets[arg3];
    recs = set->records;
    /* arg4 is an offset into the track's records; rebase it to a record index. */
    arg4        = (u16)(set->trackStartIndices[slot->trackIndex] + arg4);
    recordIndex = arg4;
    while ((s8)recs[recordIndex].flags < 0) {
        rec = recs - -(s32)recordIndex;
        if (rec->flags < ANIMATION_RECORD_END_THRESHOLD) {
            recordIndex = rec->wordOffset;
            if (recordIndex == slot->nextPose.indices.recordIndex) {
                slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
            }
            slot->flags |= ANIMATION_SLOT_FOLLOWED_JUMP;
        } else {
            recordIndex  = slot->nextPose.indices.recordIndex;
            slot->flags |= ANIMATION_SLOT_REACHED_BOUNDARY;
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
    GpAreaRec*     areaRecords;
    GpAreaVariant* variants;
    GpAreaObj*     areaState;
    AreaPlacement* placement;
    GpAreaTmdRec*  resource;
    Enemy*         enemy;
    Task*          task;
    TmdObject*     model;
    GfxCoord*      coord;
    u16            resourceId;
    s8             placementIndex;
    s32            poseIndex;

    areaRecords = Gp_AreaTables[location->stage];
    Gp_ResetLinkState();
    if (areaRecords == NULL) {
        return;
    }
    variants  = areaRecords[location->area].field_0;
    areaState = areaRecords[location->area].field_4;
    if (variants == NULL) {
        return;
    }
    _areaPrepareSpawnState(location, areaState);
    placement      = variants[location->variant].field_0;
    placementIndex = 0;
    if (placement == NULL) {
        return;
    }
    if (placement->entryId == AREA_PLACEMENT_END) {
        return;
    }
    // Match each placement with the resource entry that defines its actor.
    do {
        resource   = variants[location->variant].field_4;
        resourceId = resource->field_0;
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
                    enemy = Gp_SpawnEnemyFromTable(resource->field_8, resource->field_5,
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
                                    tmdProcessStream(model);
                                    tmdProcessStream(model);
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
                                    Gp_DestroyEnemy(enemy, enemy->task);
                                }
                            }
                        }
                    }
                    break;
                }
                resource++;
                resourceId = resource->field_0;
            } while (resourceId != AREA_PLACEMENT_END);
        }
        placementIndex++;
        placement++;
    } while (placement->entryId != AREA_PLACEMENT_END);
}

void Gp_DrawFloorQuad(GfxCoord* arg0, u32 arg1, SVECTOR* arg2)
{
    GpFloorQuadScratch* block;
    POLY_FT4*           prim;

    block = SCRATCH_STACK_RESERVE_BLOCK(GpFloorQuadScratch);
    if (arg2 == NULL) {
        block->vec[0].vx = -(arg1 >> 1);
        block->vec[0].vy = 0;
        block->vec[0].vz = -(arg1 >> 1);
    } else {
        block->vec[0].vx = arg2->vx - (arg1 >> 1);
        block->vec[0].vy = arg2->vy;
        block->vec[0].vz = arg2->vz - (arg1 >> 1);
    }
    block->vec[3].vy = block->vec[2].vy = block->vec[1].vy = block->vec[0].vy;
    block->vec[1].vx = block->vec[3].vx = block->vec[0].vx + arg1;
    block->vec[2].vx                    = block->vec[0].vx;
    block->vec[2].vz = block->vec[3].vz = block->vec[0].vz + arg1;
    block->vec[1].vz                    = block->vec[0].vz;
    Gp_UpdateCoord(arg0);
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    block->maxotz = 0;

    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[1]);
    gte_rtps();
    gte_stsxy(&block->sxy1);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[2]);
    gte_rtps();
    gte_stsxy(&block->sxy2);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[3]);
    gte_rtps();
    gte_stsxy(&block->sxy3);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    if (block->flag >= 0) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        GPU_PRIMITIVE_XY_WORD(prim, 0) = *(u32*)&block->sxy0;
        GPU_PRIMITIVE_XY_WORD(prim, 1) = *(u32*)&block->sxy1;
        GPU_PRIMITIVE_XY_WORD(prim, 2) = *(u32*)&block->sxy2;
        GPU_PRIMITIVE_XY_WORD(prim, 3) = *(u32*)&block->sxy3;
        setUV4(prim, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        prim->tpage = 0x48;
        prim->g0    = 0xC0;
        prim->b0    = 0xC0;
        prim->r0    = 0xC0;
        prim->clut  = 0x4283;
        addPrim(&gGpuCurrentOt[block->maxotz >> 4], prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpFloorQuadScratch);
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
    GpAreaRec*       rec;
    GpAreaVariant*   nested;
    GpAreaTmdRec*    table;
    GpAreaTmdRec*    entry;
    GpWorkObj*       work;
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
            work = iter->spawnArg2.pointer;
            if (iter->bodyKind == TASK_BODY_TMD) {
                key   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
                idx   = key->stage;
                extra = iter->extra.tmd;
                rec   = Gp_AreaTables[idx];
                place = work->field_3C;
                table = NULL;
                if (rec != NULL) {
                    nested = rec[key->area].field_0;
                    if (nested != NULL) {
                        table = nested[key->variant].field_4;
                    }
                }
                entry = table;
                id    = entry->field_0;
                if (id != AREA_PLACEMENT_END) {
                    limit = AREA_PLACEMENT_END;
                    do {
                        if (id == place->entryId) {
                            flags = entry->field_8->header.fields.flags;
                            if (flags == TASK_BODY_TMD) {
                                extra->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
                            } else if (flags == (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER)) {
                                extra->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            }
                            break;
                        }
                        entry++;
                        id = entry->field_0;
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
        Gp_UpdateCoord(arg0);
        Gp_UpdateCoord(dest);
        dest->parent = arg0;
        Gp_WorldToLocal(&arg0->workm, &dest->workm, &dest->coord);
        dest->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

GpWorkObj* Gp_FindWorkById(u16 arg0)
{
    Task*      head;
    Task*      iter;
    GpWorkObj* work;
    s32        key;

    work = NULL;
    head = (gameGetTaskSlot(GAME_TASK_SLOT_SCENE))->firstChild;
    if (head != NULL) {
        iter = head;
        work = iter->spawnArg2.pointer;
        key  = arg0;
        if (work->field_8.as_u16 != key) {
        loop:
            iter = iter->nextSibling;
            work = NULL;
            if (iter != head) {
                work = iter->spawnArg2.pointer;
                if (work->field_8.as_u16 != key) {
                    goto loop;
                }
            }
        }
    }
    return work;
}

void Gp_SetTmdBytes(TmdObject* arg0, s32 arg1, s32 arg2)
{
    arg0->texturePageOffset = arg1;
    arg0->clutRowOffset     = arg2;
    if (arg0->buffer != NULL) {
        tmdProcessStream(arg0);
        tmdProcessStream(arg0);
    }
}

static void Gp_SetCurAreaFlag2(s32 useSavedPoses)
{
    GpAreaRec*       areaRecords;
    GpAreaObj*       areaState;
    GameLocationKey* key;

    key         = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].field_4;
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
    GpAreaRec* areaRecords;
    GpAreaObj* areaState;
    s32        savedPoseFlag;

    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].field_4;
        if (areaState != NULL) {
            savedPoseFlag = areaState->spawnFlags & AREA_SPAWN_RESTORE_SAVED_POSES;
            return savedPoseFlag != 0;
        }
    }
    return 0;
}

static GpAreaObj* Gp_GetAreaObj(GameLocationKey* key)
{
    GpAreaRec* areaRecords;
    GpAreaObj* areaState;

    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords == NULL) {
        areaState = NULL;
    } else {
        areaState = areaRecords[key->area].field_4;
    }
    return areaState;
}

/// Initializes the placement selector and applies a requested saved-pose reset.
static void _areaPrepareSpawnState(GameLocationKey* key, GpAreaObj* areaState)
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
    GpAreaRec* areaRecords;
    GpAreaObj* areaState;

    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].field_4;
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
    GpAreaRec* areaRecords;
    GpAreaObj* areaState;

    areaRecords = Gp_AreaTables[key->stage];
    if (areaRecords != NULL) {
        areaState = areaRecords[key->area].field_4;
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

static GpAreaTmdRec* Gp_GetNestedAreaObj(GameLocationKey* key)
{
    GpAreaRec*     areaRecords;
    GpAreaVariant* variants;
    GpAreaTmdRec*  resources;

    areaRecords = Gp_AreaTables[key->stage];
    resources   = NULL;
    if (areaRecords != NULL) {
        variants = areaRecords[key->area].field_0;
        if (variants != NULL) {
            resources = variants[key->variant].field_4;
        }
    }
    return resources;
}

GpAreaVariant* Gp_GetNestedAreaRec(GameLocationKey* key)
{
    GpAreaRec*     areaRecords;
    GpAreaVariant* variants;

    areaRecords = Gp_AreaTables[key->stage];
    variants    = NULL;
    if (areaRecords != NULL) {
        variants = areaRecords[key->area].field_0;
        if (variants != NULL) {
            variants = &variants[key->variant];
        }
    }
    return variants;
}

void Gp_SetAreaFlag0(GameLocationKey* location)
{
    u32        stageAreaKey;
    GpAreaRec* areaRecords;
    GpAreaObj* areaState;

    stageAreaKey = GAME_LOCATION_WORD(*location) & GAME_LOCATION_STAGE_AREA_MASK;
    areaRecords  = Gp_AreaTables[location->stage];
    if (stageAreaKey != GAME_LOCATION_KEY(3, 0x26, 0, 0)) {
        if (areaRecords != NULL) {
            areaState = areaRecords[location->area].field_4;
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

s32 Gp_FindChildType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3)
{
    Task* child;
    s32   ret;

    *arg3 = NULL;
    child = arg0->firstChild;
    ret   = -1;
    if (child == NULL) {
        return ret;
    }
    arg1 = child;
    do {
        arg0 = arg1->spawnArg2.pointer;
        if (((((GpWorkObj*)arg0)->field_A >> 8) == 9) && (((GpWorkObj*)arg0)->field_8.as_u16 == arg2)) {
            *arg3 = arg1;
            ret   = 0;
            break;
        }
        arg1 = arg1->nextSibling;
    } while (arg1 != child);
    return ret;
}

s32 Gp_FindChildExceptType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3)
{
    Task* child;
    s32   ret;

    *arg3 = NULL;
    child = arg0->firstChild;
    ret   = -1;
    if (child == NULL) {
        return ret;
    }
    arg1 = child;
    do {
        arg0 = arg1->spawnArg2.pointer;
        if (((((GpWorkObj*)arg0)->field_A >> 8) != 9) && (((GpWorkObj*)arg0)->field_8.as_u8 == arg2)) {
            *arg3 = arg1;
            ret   = 0;
            break;
        }
        arg1 = arg1->nextSibling;
    } while (arg1 != child);
    return ret;
}

s32 Gp_ExitChildrenType9(Task* arg0)
{
    Task*      child;
    Task*      next;
    GpWorkObj* work;
    u32        type;

    child = arg0->firstChild;
    if (child == NULL) {
        return 0;
    }
    arg0 = child;
    do {
        work = (GpWorkObj*)arg0->spawnArg2.pointer;
        type = work->field_A >> 8;
        next = arg0->nextSibling;
        if (type == 9) {
            Task_CallExit(arg0);
        }
        arg0 = next;
    } while (arg0 != child);
    return 0;
}

s32 Gp_SendMsgType9(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*      child;
    Task*      next;
    GpWorkObj* work;
    u32        type;

    child = arg0->firstChild;
    if (child == NULL) {
        return 0;
    }
    arg0 = child;
    do {
        work = (GpWorkObj*)arg0->spawnArg2.pointer;
        type = work->field_A >> 8;
        next = arg0->nextSibling;
        if (type == 9) {
            Gp_DispatchMsg(arg0, arg3, arg2, 0);
        }
        arg0 = next;
    } while (arg0 != child);
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
    GpAreaRec*     areaRecords;
    GpAreaVariant* variants;
    GpAreaObj*     areaState;

    areaRecords  = Gp_AreaTables[key->stage];
    key->variant = AREA_DEFAULT_VARIANT;
    if (areaRecords == NULL) {
        return;
    }
    variants  = areaRecords[key->area].field_0;
    areaState = areaRecords[key->area].field_4;
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
    GpDirScratch* scratch;
    SVECTOR*      delta;
    GfxCoord*     viewCoord;
    s32           scale;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GpDirScratch);
    delta   = &scratch->vec;
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
    VectorNormalSS(&scratch->vec, &scratch->vec);
    TransposeMatrix(&viewCoord->workm, &scratch->mtx);
    gfxLoadRotSv(&scratch->mtx, &scratch->vec);
    gte_rtv0();
    gte_stsv(delta);
    gte_lddp(scale);
    gte_ldsv(delta);
    gte_gpf12();
    gte_stsv(offset);
    SCRATCH_STACK_RELEASE_BLOCK(GpDirScratch);
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
                Tmd_FreeBuffers(obj);
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

    p      = &Gp_Bit2Banks[gGameSession->location.loc.stage].field_4[arg0 >> 4];
    shift  = (arg0 & 0xF) * 2;
    word   = *p;
    word  &= 3 << shift;
    word >>= shift;
    return word;
}

/// Finds the record in the 0xFFFF-terminated `desc` table whose id is
/// `place->field_2` and spawns that enemy at `place`.
static inline void _gpSpawnPlace(GpEnemyDesc* desc, GpBit2Rec* place)
{
    Enemy*     enemy;
    Task*      task;
    TmdObject* extra;
    GfxCoord*  coord;
    u16        id;

    id = desc->field_0;
    while (id != 0xFFFF) {
        if (id == place->field_2) {
            enemy = Gp_SpawnEnemyFromTable(&desc->field_4, 0, desc->field_0, NULL);
            if (enemy != NULL) {
                task = enemy->task;
                if (task->bodyKind != TASK_BODY_NONE) {
                    extra               = task->extra.tmd;
                    coord               = extra->coords;
                    enemy->placeKey     = place->field_0 | (place->field_4 << ENEMY_PLACE_STAGE_SHIFT);
                    enemy->workType     = place->field_2;
                    coord->coord.t[0]   = place->field_8;
                    coord->coord.t[1]   = place->field_A;
                    coord->coord.t[2]   = place->field_C;
                    coord->param.rot.vy = place->field_E;
                    if (coord->param.rot.vy != 0) {
                        gfxRotMatrixY(&coord->coord, (s16)place->field_E, 1);
                    }
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
            return;
        }
        desc++;
        id = desc->field_0;
    }
}

/// Walks `Gp_Bit2Banks[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area / stage]` for a `GpBit2Rec`
/// whose `field_0` equals `arg0`. If the packed 2-bit flag at
/// `Gp_Bit2Banks[gGameSession->location.loc.stage].field_4` is non-zero, spawns that
/// placement via `Gp_SpawnEnemyFromTable` (same coord/yaw writeback as `Gp_SpawnPlaces`).

/// Inline form of `Gp_GetRelatedQty`: the most of a related item weapon
/// `item` can hold, from bank `bank`'s table, or 0 for a non-weapon id.
static inline s32 _gpRelatedQty(s32 item, s32 bank)
{
    s32 ret;

    item -= 0x80;
    ret   = 0;
    if ((u32)item < 0x20) {
        if (bank == 0) {
            ret = Gp_RelatedQty0.rows[item].field_0;
        } else {
            ret = Gp_RelatedQty1.rows[item].field_0;
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

/// Three-entry dispatcher table: `Gp_EnemyWaitStart`, `Gp_EnemyWaitTick`, `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 Gp_EnemyWaitFuncs = { {
    Gp_EnemyWaitStart,
    Gp_EnemyWaitTick,
    Gp_DestroyEnemy,
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

/// "ERROR: ex_pdriver_2\n". The three bytes after the terminator are not zero:
/// the original toolchain left them in the alignment gap.
static const char D_80093A44[24] = "ERROR: ex_pdriver_2\n\0\xB7\xB0\x34";

static const TaskFuncTable3 D_80093A5C = { {
    func_800B6094,
    func_800B51F4,
    Task_CallExit,
} };

GpSlot4MessageEntry Gp_Slot4MsgTable[5] = {
    { 2000, { .find = Gp_FindChildType9 } },
    { 2008, { .find = Gp_FindChildExceptType9 } },
    { 2009, { .exit = Gp_ExitChildrenType9 } },
    { 2010, { .send = Gp_SendMsgType9 } },
    { 0x7FFFFFFF, { .exit = NULL } },
};
GpBit2Bank Gp_Bit2Banks[6] = { { NULL, NULL }, { D_map_akropolis_8017A7FC, GameFlag_AcropolisBanks[0].header.entryStates }, { D_map_dryfield_8017A564, GameFlag_DryfieldBanks[0].header.entryStates }, { D_map_dryfield_full_8017A46C, GameFlag_DryfieldBanks[0].header.entryStates }, { D_map_shelter_8017A998, GameFlag_ShelterBanks[0].header.entryStates }, { D_map_neo_ark_8017A6EC, GameFlag_NeoArkBanks[0].header.entryStates } };

void Gp_BindSlot4(Task* task)
{
    Game_SetPtrSlot(task, 4);
    task->msgTable = Gp_Slot4MsgTable;
    task->state++;
}

void func_800B6398(Task* task)
{
    Gp_DrawTargetCursor();
}
