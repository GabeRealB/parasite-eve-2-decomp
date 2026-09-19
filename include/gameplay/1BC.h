#ifndef GAMEPLAY_1BC_H
#define GAMEPLAY_1BC_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/areaplace.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// One enemy: the work object `Gp_AllocEnemy` allocates and hangs off
/// `Task::spawnArg2`, which the enemy's task frees again when it dies.
///
/// An actor keeps its own state in a work block of its own and publishes here
/// only what the gameplay systems read back: the coordinate and body position
/// it moves the enemy through, its hit points, the placement record its spawn
/// parameters come from, and the parameter record its kind is defined by.
/// Those systems find the enemy through `node` rather than through its task -
/// the lock-on scan, the HP readout and the damage reactions all walk the list
/// it is on - and by `placeKey`, which names the placement it was spawned from
/// and is the enemy's identity for the walkers of its parent's children.
///
/// `Gp_SaveEnemyPose` files the enemy's pose under that key, and `spawnState`
/// beside it says which state the spawn handler resumes the enemy in, so a
/// room the player leaves and re-enters restores its enemies where they were
/// rather than placing them again.
///
/// Several helpers reach this object through a narrower view of it - the
/// reaction, pair and lock-on helpers take a prefix type and cast - so the
/// fields those views name at the same offsets are this type's.
typedef struct GpEnemy {
    Task*          task;          // Owning task; the one whose `spawnArg2` is this object
    MATRIX*        field_4;       // Role unproven: actors store a model part's matrix here, nothing reads it back
    u16            placeKey;      // Key of the placement the enemy was spawned from: area, stage, and that placement's own number in the high nibble
    u16            workType;      // Work type the enemy was spawned as, bank in the high byte and type in the low (0x900 is the plain enemy)
    s32            waitTicks;     // Frames an enemy with no actor body waits before it is torn down
    GpLinkNode     node;          // Lock-on link: the entry the aim scan, HP readout and damage reactions reach the enemy by
    GsCOORDINATE2* coord;         // Coordinate the body sits at, usually one of the actor's model parts
    VECTOR3        bodyPos;       // Body position in `coord`'s frame: the point distance and damage-chance rolls measure from
    byte           pad_28[4];
    VECTOR3        playerRelPos;  // `bodyPos` brought to world space and made relative to the player, refreshed each frame; the aim and lock-on scans take their angle and distance from it
    byte           pad_38[4];
    GpAreaPlace*   place;         // Placement record behind the enemy's spawn parameters (an actor may publish a table of its own here)
    s16            hp;            // Hit points left; damage subtracts from it and the readout shows it against `hpMax`
    u16            hpMax;         // Hit points the enemy is spawned with; a damage reaction is picked by fractions of it
    byte           pad_44[4];
    u8             field_48;      // Role unproven: every spawn handler clears it, nothing reads it back
    byte           pad_49[2];
    u8             spawnState;    // State the enemy is respawned in: saved with its pose and restored by the spawn handlers
    u8             reactionFlags; // Reactions a landed hit asked for: bits 0-1 stagger the body, bits 2-3 the damage-over-time reaction, cleared as the body consumes them
    u8             field_4D;      // Role unproven: cleared beside `reactionFlags` on spawn, nothing reads it back
    u8             colorMode;     // Colour remap the body is drawn with: current mode in bits 0-1, previous in bits 2-3, bit 7 a pending hit flash
    u8             colorBlend;    // Frames a colour remap change is blended over, in sixteenths; 0 switches at once
    GpPairSrcE*    param;         // Parameter record the enemy's kind is defined by, shared with every enemy of that kind, `NULL` where the kind has none
    GpRec18*       recs;          // The enemy's own contact records; its collision bodies point at the table and the Parasite Energy targeting claims entries in it
    byte           pad_58[8];
} GpEnemy;
STATIC_ASSERT_SIZEOF(GpEnemy, 0x60);

/// Callback for GpEnemy + Task state handlers (entries in `Gp_EnemyWaitFuncs`).
typedef void (*GpEnemyTaskFunc)(GpEnemy* enemy, Task* task);

/// Fixed-size table of `GpEnemyTaskFunc` callbacks. Copied onto the stack by
/// `Gp_EnemyDispatch` so the call uses a local jump table.
typedef struct {
    GpEnemyTaskFunc funcs[3];
} GpEnemyTaskFuncTable3;

/// Four-entry form of `GpEnemyTaskFuncTable3`, for actors whose dispatcher has
/// an extra state beyond spawn/tick/teardown.
typedef struct {
    GpEnemyTaskFunc funcs[4];
} GpEnemyTaskFuncTable4;

/// Five-entry form of `GpEnemyTaskFuncTable3`, for actors with two extra
/// states beyond spawn/tick/teardown.
typedef struct {
    GpEnemyTaskFunc funcs[5];
} GpEnemyTaskFuncTable5;

/// Three-entry dispatcher table: `Gp_EnemyWaitStart`, `Gp_EnemyWaitTick`, `Gp_DestroyEnemy`.
extern GpEnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

void func_800B25B0(void);

/// Pose pair used by `Gp_AnimWritePoseBlend` / `Gp_AnimWritePoseCopy`. Translation is
/// GPF/GPL-blended (`Gp_AnimWritePoseBlend`) or copied (`Gp_AnimWritePoseCopy`) into
/// `GsCOORDINATE2.coord.t` when `GpAnimSlot.poseKind == 1`; rotation is
/// GPF/GPL-blended with the other pose and fed to `RotMatrix_gte`.
typedef struct _GpAnimPose {
    /* 0x00 */ SVECTOR trans;
    /* 0x08 */ SVECTOR rot;
} GpAnimPose;
STATIC_ASSERT_SIZEOF(GpAnimPose, 0x10);

/// One bone pose of a track that only rotates: the three Euler angles of the
/// pose's `SVECTOR`, packed into a single 11/10/11-bit word at a resolution of
/// 8 angle units.
///
/// The rotation-only companion of `GpPackedPose`, which carries a translation
/// beside the same angles and so takes three words per pose to this type's one.
typedef struct {
    s32 rx : 11;
    s32 ry : 10;
    s32 rz : 11;
} GpPackedSvec;
STATIC_ASSERT_SIZEOF(GpPackedSvec, 4);

/// Packed translation + rotation (no `SVECTOR` pad). `Gp_AnimBlendPose`
/// GPF/GPL-blends `vx`/`vy`/`vz` and copies `rx`/`ry`/`rz` into
/// `GpAnimScratch80.vec0` / `vec1`. `func_800B3448` dispatches here when
/// `GpAnimSlot.poseKind == 1`.
typedef struct _GpPackedPose {
    /* 0x00 */ s16 vx;
    /* 0x02 */ s16 vy;
    /* 0x04 */ s16 vz;
    /* 0x06 */ s16 rx;
    /* 0x08 */ s16 ry;
    /* 0x0A */ s16 rz;
} GpPackedPose;
STATIC_ASSERT_SIZEOF(GpPackedPose, 0xC);

/// Source/dest pointers for `Gp_AnimBlendPacked` / `Gp_AnimBlendPose`. Lives at
/// offset 4 of the 0x18-byte scratch `func_800B3448` allocates from
/// `G_SCRATCH_HEAD`. `field_0` / `field_4` are the current and next-frame
/// sources (`GpPackedSvec` when the slot's `poseKind` is 4, `GpPackedPose`
/// when it is 1); `field_8` is an optional packed dest (`arg3` of
/// `func_800B3448`). `field_C` is `arg2` of `func_800B3448` (optional
/// translation dest); `field_10` is a copy of `GpAnimSlot.bufPose`.
typedef struct _GpAnimBlendSrc {
    /* 0x00 */ GpPackedSvec* field_0;
    /* 0x04 */ GpPackedSvec* field_4;
    /* 0x08 */ GpPackedSvec* field_8;
    /* 0x0C */ GpAnimPose*   field_C;
    /* 0x10 */ u8            field_10;
} GpAnimBlendSrc;
STATIC_ASSERT_SIZEOF(GpAnimBlendSrc, 0x14);

/// 0x80-byte scratch from `G_SCRATCH_HEAD` used by `Gp_AnimBlendPacked` /
/// `Gp_AnimBlendPose` / `Gp_BlendAnimRot`. `trans` is the GPF/GPL-blended
/// translation (`Gp_AnimBlendPose`); `vec0` / `vec1` are unpacked from
/// `GpAnimBlendSrc.field_0` / `field_4`; `blend` / `invBlend` are the
/// 12-bit GPF/GPL weights. `Gp_BlendAnimRot` also uses the matrices.
typedef struct _GpAnimScratch80 {
    /* 0x00 */ SVECTOR trans;
    /* 0x08 */ SVECTOR vec0;
    /* 0x10 */ SVECTOR vec1;
    /* 0x18 */ MATRIX  mtx0;
    /* 0x38 */ MATRIX  mtx1;
    /* 0x58 */ MATRIX  mtx2;
    /* 0x78 */ s32     blend;
    /* 0x7C */ s32     invBlend;
} GpAnimScratch80;
STATIC_ASSERT_SIZEOF(GpAnimScratch80, 0x80);

/// One entry of an animation set's 4-byte record array (`GpAnimSet.recs`),
/// walked by the slot code to find the pose a clip is showing. A keyframe entry
/// names that pose, how many frames it is held, and how the pose is encoded;
/// the two cue bits in `flags` mark keyframes a frame handler wants to know
/// about, since it tests them and fires whatever cue it makes them mean.
///
/// A control entry is not shown at all: `flags` bit 7 marks it and the walk
/// follows it instead. With bit 6 clear it continues at `pose`, which is how a
/// clip loops; with bit 6 set it ends the clip and holds the pose reached. A
/// control entry's `duration` is written but never read.
///
/// `pose` counts **4-byte words, not poses**, in the pose bank and in the
/// record array alike: the record array and a `GpPackedSvec` bank are one word
/// per element, a `GpPackedPose` bank three, so the latter's poses sit at every
/// third offset.
typedef struct GpAnimRec {
    /* 0x00 */ u16 pose;     // word offset into the set's pose bank; a control entry's continuation record
    /* 0x02 */ u8  duration; // frames this keyframe is held
    /* 0x03 */ u8  flags;    // 0-3 pose encoding (0 control, 1 GpPackedPose, 4 GpPackedSvec), 4-5 cue bits, 7 control entry, 6 end of clip
} GpAnimRec;
STATIC_ASSERT_SIZEOF(GpAnimRec, 4);

/// `Task::spawnArg2` payload of `func_800B65B0`, the pickup-confirm task.
/// field_0 is the `GpBit2Rec` item id passed to `Gp_LookupBit2Item`;
/// field_2 is set to 1 when the task finishes, field_3 to 1 when the player
/// confirmed (`UiObject.field_2C == 0x33`), and field_4 is the spawn mode
/// (0 when `D_80114DDE` bit 9 is set, else 1; passed inverted to
/// `Ui_SpawnFromDesc`).
typedef struct _GpPickupWork {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
} GpPickupWork;
STATIC_ASSERT_SIZEOF(GpPickupWork, 6);

/// One animation of a model: the clip data behind a single pointer of the table
/// at `GpAnimSlot.sets` (the same table as `GpAnimCtx.sets`), indexed
/// by animation id.
///
/// An animation carries one track per model part, each a run of `recs`
/// keyframes that begins at the record `trackStart` names, plus one pose bank
/// per pose encoding, which those records index into by 4-byte word.
typedef struct {
    GpAnimRec*    recs;         // keyframe records of every track, one run per model part
    u16*          trackStart;   // record index each track begins at, indexed by `GpAnimSlot.trackIndex`
    GpPackedSvec* poseBanks[8]; // pose bank per pose encoding, indexed by `GpAnimRec.flags & 0xF` (1 `GpPackedPose`, 4 `GpPackedSvec`)
} GpAnimSet;
STATIC_ASSERT_SIZEOF(GpAnimSet, 0x28);

/// 0x18-byte scratch `func_800B3448` allocates from `G_SCRATCH_HEAD` before
/// dispatching to `Gp_AnimBlendPose` / `Gp_AnimBlendPacked`; only `src` is
/// written.
typedef struct _GpAnimScratch18 {
    /* 0x00 */ s32            field_0;
    /* 0x04 */ GpAnimBlendSrc src;
} GpAnimScratch18;
STATIC_ASSERT_SIZEOF(GpAnimScratch18, 0x18);

/// One animation slot: the playback state of one model part's animation.
///
/// The slot walks the keyframe records of a single track, blending the pose it
/// has reached (`curSet`/`curRec`) into the one it is heading for
/// (`nextSet`/`nextRec`), which takes that keyframe's `duration` as the length
/// of the segment. `timeLeft` counts the segment down under `rate` and is the
/// blend's numerator, `timeSpan` its denominator. Records that are control
/// entries rather than poses are not shown: the walk follows them and reports
/// what it did in `flags`.
///
/// A slot's track and its transform are separate: `trackIndex` names the track
/// it reads and `mtxIndex` the coordinate it writes, the same part unless a
/// caller pairs a slot with another part's track. Slots sit in the array
/// `GpAnimCtx.slots` points at, and the tick helpers recover that array as
/// `slot - slot->trackIndex`, so a slot following another part's track cannot
/// be ticked through a pointer alone.
///
/// Either keyframe may instead be a pose the caller supplies, kept per slot in
/// the context's pose buffer and marked by the 0x7FFF sentinel; `bufPose` says
/// one of the two is that kind.
typedef struct {
    /* 0x00 */ u16         curSet;      // set of the keyframe the slot has reached; 0x7FFF takes the pose from the context's pose buffer
    /* 0x02 */ u16         curRec;      // that keyframe's record index
    /* 0x04 */ u16         nextSet;     // set of the keyframe it is heading for; 0x7FFF as in `curSet`
    /* 0x06 */ u16         nextRec;     // that keyframe's record index
    /* 0x08 */ byte        pad_8;
    /* 0x09 */ u8          rate;        // segment advance per tick in 16ths of a frame (0x10 one frame), read signed, so a negative rate runs the segment backwards
    /* 0x0A */ u8          field_A;     // role unproven: written 0 by the walk, never read
    /* 0x0B */ u8          poseKind;    // pose encoding of the keyframe being headed for (`GpAnimRec.flags & 0xF`): the `GpAnimSet.poseBanks` entry its pose comes from
    /* 0x0C */ s16         timeLeft;    // frames left in the segment, in 16ths; it runs past zero until the walk catches up
    /* 0x0E */ u16         timeSpan;    // that keyframe's `duration` in the same units; the blend's denominator
    /* 0x10 */ u16         flags;       // bit 0 the walk took the clip's end, bit 1 it followed a control entry, bit 8 the clip has ended and settled on its last pose
    /* 0x12 */ u16         field_12;    // role unproven: written 0 by every initialiser, never read
    /* 0x14 */ u8          mtxIndex;    // `GpAnimCtx.coords` entry the slot writes: the model part whose transform it drives
    /* 0x15 */ u8          trackIndex;  // track the slot reads: the model part whose keyframes it follows
    /* 0x16 */ u8          atEnd;       // the clip has run to its end: the slot holds its last pose and does not advance
    /* 0x17 */ u8          bufPose;     // the pose came from the context's pose buffer rather than a pose bank
    /* 0x18 */ SVECTOR     bufRotDelta; // Euler angles of the rotation from the previous buffered pose to the current, applied while both ticks are buffered
    /* 0x20 */ GpAnimSet** sets;        // the animation set table `curSet` and `nextSet` index (the context's)
    /* 0x24 */ byte        pad_24[4];
} GpAnimSlot;
STATIC_ASSERT_SIZEOF(GpAnimSlot, 0x28);

/// One model's animation state: what its playback reads and the slots that walk
/// it.
///
/// A context is built once from the model body it animates and the animation
/// tables its slots index, and is handed to every later animation call on that
/// model. It borrows the model's own per-part coordinate array and part count,
/// so a slot tick needs nothing but the context.
///
/// The slots and the pose buffer are the caller's: one playback slot per model
/// part, and one pose record per slot, where a slot keeps a pose that no
/// keyframe supplies.
typedef struct _GpAnimCtx {
    GpAnimSet**    sets;      // Set table the slots index by animation id
    GsCOORDINATE2* coords;    // The model's per-part coordinate array: each slot writes the transform of the part it drives
    GpPackedSvec*  poses;     // Pose buffer, one 0x10-byte record per slot, in the encoding that slot's `GpAnimSlot.poseKind` names
    GpAnimSlot*    slots;     // Playback state, one slot per model part
    s32            partCount; // Parts the model is divided into, mirrored from `TmdObject.partCount`
} GpAnimCtx;
STATIC_ASSERT_SIZEOF(GpAnimCtx, 0x14);

/// Object returned by `Gp_GetAreaObj` (`GpAreaRec.field_4`). `field_0` is a
/// signed id compared with `GpAreaKey.place`; `field_1` is a flags byte
/// (bits 0/1/2/4 in nearby 1BC / 1A8 helpers; `Gp_GetAreaFlag2` returns bit 1).
/// Full size unknown.
typedef struct _GpAreaObj {
    /* 0x00 */ s8 field_0;
    /* 0x01 */ u8 field_1;
} GpAreaObj;

/// Overlay of `GsCOORDINATE2` at `Task::extra->field_8` used by
/// `Gp_SpawnArea`. `coord` is `GsCOORDINATE2.coord`; `field_44` /
/// `field_46` / `field_48` overlay `param` as packed euler (restore
/// from `McPosRec`) or yaw (`field_46` + `Gfx_RotMatrixY`).
typedef struct _GpCoordPose {
    /* 0x00 */ s32    flg;
    /* 0x04 */ MATRIX coord;
    /* 0x24 */ byte   pad_24[0x20];
    /* 0x44 */ s16    field_44;
    /* 0x46 */ s16    field_46;
    /* 0x48 */ s16    field_48;
    /* 0x4A */ byte   pad_4A[2];
} GpCoordPose;
STATIC_ASSERT_SIZEOF(GpCoordPose, 0x4C);

/// 0xC-byte record in the 0xFF-terminated table at nested `GpAreaRec.field_4`
/// (`Gp_ApplyAreaTmdFlags` / `Gp_SpawnArea`). `field_0` is compared with the byte at
/// `GpWorkObj.field_3C` / `GpAreaPlace.entryId`. `field_5` is the
/// `Gp_SpawnEnemyFromTable` table index. `field_8` points at a halfword whose value 1
/// clears `TmdObject.flags` bit 2 and 0x101 sets it (`Gp_ApplyAreaTmdFlags`), or
/// at a `TaskDesc` table (`Gp_SpawnArea`).
typedef struct _GpAreaTmdRec {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ byte pad_2[3];
    /* 0x05 */ u8   field_5;
    /* 0x06 */ byte pad_6[2];
    /* 0x08 */ u16* field_8;
} GpAreaTmdRec;
STATIC_ASSERT_SIZEOF(GpAreaTmdRec, 0xC);

/// 8-byte record in tables pointed to by `Gp_AreaTables`. Indexed by
/// `GpAreaKey.area`. `field_0` is a nested table (`Gp_GetNestedAreaRec`
/// returns the entry at `GpAreaKey.place`; `Gp_GetNestedAreaObj` returns that
/// entry's `field_4`, a `GpAreaTmdRec` table). Outer `field_4` is the object
/// `Gp_GetAreaObj` returns.
typedef struct _GpAreaRec {
    /* 0x00 */ struct _GpAreaRec* field_0;
    /* 0x04 */ GpAreaObj*         field_4;
} GpAreaRec;
STATIC_ASSERT_SIZEOF(GpAreaRec, 8);

/// Overlay of `Task::spawnArg2` for sibling walkers. `field_A` high byte is
/// the work type (`Gp_FindChildType9` / `Gp_ExitChildrenType9` / `Gp_SendMsgType9` match 9;
/// `Gp_FindChildExceptType9` skips 9). `field_8` is the id compared against the search
/// key (`as_u16` / `as_u8`; `Gp_FindWorkById` matches `as_u16` on slot 4's
/// children). `field_3C` is the placement record the children of slot 4 were
/// spawned from, whose `entryId` `Gp_ApplyAreaTmdFlags` matches. Full size unknown.
typedef struct _GpWorkObj {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[4];
    /* 0x08 */ union {
        u16 as_u16;
        u8  as_u8;
    } field_8;
    /* 0x0A */ u16          field_A;
    /* 0x0C */ byte         pad_C[0x30];
    /* 0x3C */ GpAreaPlace* field_3C;
} GpWorkObj;

/// 8-byte mask/flag record. `Gp_SndMaskTable` is a 0-terminated table of these.
/// `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks` walk it: if `arg0 & mask`, apply `flags`
/// to `SndEvt_EnqueueType7` / `SndBank_SetEnableFlags`.
typedef struct _GpSndMaskRec {
    /* 0x0 */ s32 mask;
    /* 0x4 */ s32 flags;
} GpSndMaskRec;
STATIC_ASSERT_SIZEOF(GpSndMaskRec, 8);

/// Per-area pointer table. Index is `GpAreaKey.stage`.
extern GpAreaRec* Gp_AreaTables[];

/// 0-terminated `GpSndMaskRec` table walked by `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks`.
extern GpSndMaskRec Gp_SndMaskTable[];

/// 0x3C-byte stream header read before the sector payload and copied into
/// `CdCmdQueue.field_58`. The payload uses `field_30` sectors and buffer kind
/// `field_34`; `field_36` controls whether an already-loaded part is skipped.
typedef struct _GpSectorHeader {
    /* 0x00 */ u8    pad_0[0x20];
    /* 0x20 */ void* field_20;
    /* 0x24 */ u8    pad_24[0xC];
    /* 0x30 */ s16   field_30;
    /* 0x32 */ u16   field_32;
    /* 0x34 */ s16   field_34;
    /* 0x36 */ s16   field_36;
    /* 0x38 */ u8    pad_38[4];
} GpSectorHeader;
STATIC_ASSERT_SIZEOF(GpSectorHeader, 0x3C);

s32      func_800AF590(void);
s16      Gp_FindStreamSlot(u16 arg0, u16 arg1, u16 arg2, u16 arg3);
void     Gp_StepCdAudioCmd(void);
void     Gp_ApplySndMasks(u16 arg0);
void     Gp_ApplySndBankMasks(u16 arg0);
void     Gp_RestoreStreamRng(void);
s32      func_800B0118(s32 arg0, s32 arg1);
GpEnemy* Gp_SpawnEnemy(s32 bank, s32 type, s32 arg2, GpEnemy* parent);
GpEnemy* Gp_SpawnEnemyFromTable(TaskDesc* table, s32 idx, s32 arg2, GpEnemy* parent);
void     Gp_DestroyEnemy(GpEnemy* enemy, Task* task);
void     Gp_EnemyTaskExit(Task* task);
/// Copies `arg1`'s matrix onto the coordinate at `Task::extra->field_8`,
/// adding `arg2` in that space. If `arg1->sub` is world (`gGfxViewCoord`),
/// copies `coord` and transforms in place; otherwise computes `workm`
/// via `Gp_UpdateCoord`, transforms there, and converts to local with
/// `Gp_WorldToLocal`. Always parents the dest to world and clears `flg`.
/// Returns `arg0` (or NULL).
Task*    Gp_CopyCoordOffset(Task* arg0, GsCOORDINATE2* arg1, SVECTOR* arg2);
GpEnemy* Gp_AllocEnemy(Task* task, GpEnemy* parent);
void     Gp_EnemyWaitStart(GpEnemy* enemy, Task* task);
void     Gp_EnemyWaitTick(GpEnemy* enemy, Task* task);
void     Gp_EnemyDispatch(Task* arg0);
s32      Gp_TryEnqueueSndCd(s32 arg0);
void     Gp_EnqueueSndCd(u8 arg0);
void     Gp_MtxToEuler(MATRIX* arg0, SVECTOR* arg1);
/// Extracts ZYX Euler angles from `arg1`'s rotation into `arg0`. Tries `vx`
/// and `vx ± 0x800` (the other Euler solution) and keeps the candidate with
/// the smaller sum of absolute angles. Returns `arg0`.
SVECTOR* Gp_ExtractEuler(SVECTOR* arg0, MATRIX* arg1);
/// Lerps the 3x3 rotation of `arg0` toward `arg1` by `arg3 / ONE`, then
/// orthonormalizes into `arg2`. Outer products of each interpolated row
/// pair pick the two most independent axes; `MatrixNormal_0` / `_1` / `_2`
/// reconstructs the missing row.
void Gp_LerpOrthonormal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2, s32 arg3);
/// Walks `arg0->sub` up to world (`gGfxViewCoord`), composing each node's
/// `coord` rotation into `arg1` and accumulating the rotated translation
/// into `arg2`. The world parent initializes `arg1` to identity and
/// `arg2` to zero.
void Gp_ComposeParentWorld(GsCOORDINATE2* arg0, MATRIX* arg1, SVECTOR* arg2);
/// 8-byte RGB555-unpacked vector. `Gp_BlendRgb555` allocates three of
/// these (0x18 bytes) from `G_SCRATCH_HEAD`: src0, src1, then the GTE
/// lerp result. Channels are 5-bit values shifted left 7.
typedef struct _GpRgbScratch {
    /* 0x00 */ u16 r;
    /* 0x02 */ u16 g;
    /* 0x04 */ u16 b;
    /* 0x06 */ u16 pad;
} GpRgbScratch;
STATIC_ASSERT_SIZEOF(GpRgbScratch, 8);
/// Unpacks two RGB555 colors, GPF/GPL-blends them by `arg2` / `0x1000 -
/// arg2`, packs the result into `*arg3`, and copies the STP bit if
/// either source has it set.
void Gp_BlendRgb555(u16* arg0, u16* arg1, s32 arg2, u16* arg3);
void Gp_BlendRgb555Clut(u16* arg0, u16* arg1, s32 arg2, u16* arg3);
void Gp_BlendRgb555ClutMasked(u16* arg0, u16* arg1, s32 arg2, u16* arg3, s32 arg4);
/// 4-byte work at `Task::spawnArg2` for `Gp_FadeWorkTask`. `field_0`
/// selects the semi-transparency rate of the trailing `DR_TPAGE`
/// (`0xE1000240` when 0, `0xE1000220` otherwise); `field_1` is the
/// handshake flag the owner sets to 1 to start the fade-out and the task
/// sets to 2 once it is done; `field_2` is the fade length in frames
/// (defaulted to 0x20) and doubles as the ramp divisor.
typedef struct _GpFadeWork {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} GpFadeWork;
STATIC_ASSERT_SIZEOF(GpFadeWork, 4);
/// Full-screen fade quad. Ramps a 0x140x0xF0 `TILE` from black to
/// `field_2`-scaled white over `field_2` frames, holds until the owner
/// raises `field_1`, then ramps back down and kills the task. Sorted into
/// `gGpuCurrentOt[Task::spawnArg1]`, or (`spawnArg1 == 0`) into the head
/// of the current ordering table, backing up 0xA entries when the current
/// OT is not one of the two `Gpu_OrderingTables` roots.
void Gp_FadeWorkTask(Task* arg0);
void Gp_BlendAnimRot(GpAnimBlendSrc* arg0, GsCOORDINATE2* arg1, GpAnimSlot* arg2,
                     GpAnimScratch80* arg3);
void Gp_AnimBlendPose(GpAnimBlendSrc* arg0, GsCOORDINATE2* arg1, GpAnimSlot* arg2);
void Gp_AnimBlendPacked(GpAnimBlendSrc* arg0, GsCOORDINATE2* arg1, GpAnimSlot* arg2);
void Gp_AnimAdvanceSlot(GpAnimCtx* arg0, s32 arg1);
void Gp_AnimSeekSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800B3AA4(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
/// Fills `arg0` from the model body `arg2` animates and the animation tables
/// `arg1` names. The context borrows the model's per-part coordinate array and
/// part count, and takes the caller's pose buffer; the slots are filled in
/// separately by `Gp_AnimInitSlot`.
void Gp_AnimInitCtx(GpAnimCtx* arg0, void* arg1, TmdObject* arg2, void* arg3);
void Gp_AnimInitSlot(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3);
void Gp_AnimTickSlot(GpAnimCtx* arg0, GpAnimSlot* arg1);
void Gp_AnimTickSlot2(GpAnimCtx* arg0, GpAnimSlot* arg1);
void Gp_AnimTickSlot3(GpAnimCtx* arg0, GpAnimSlot* arg1);
void func_800B3E74(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3);
void func_800B3EE8(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4);
/// `Gp_AnimInitCtx` with the model's playback slots handed in as well, for a
/// caller whose slot array is part of a block of its own.
void Gp_AnimInitCtxSlots(GpAnimCtx* arg0, void* arg1, TmdObject* arg2, void* arg3, GpAnimSlot* arg4);
/// Forwards to `Gp_AnimInitCtxSlots`, which most callers reach by this name
/// rather than its own.
void func_800B3F84(GpAnimCtx* arg0, void* arg1, TmdObject* arg2, void* arg3, GpAnimSlot* arg4);
void Gp_AnimResetSlot(GpAnimCtx* arg0, s32 arg1, s32 arg2);
void Gp_AnimResetSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
/// `func_800B4114` is deliberately not declared here. Its definition in
/// `gameplay/1BC.c` takes `arg2` as `u16` — that is what makes the callee's
/// own body match — but every caller passes a sign-extended `s16` animation
/// id, so a `u16` prototype in scope adds a zero-extension the callers do not
/// have. Callers declare it locally with a signed `arg2` instead (see
/// `src/actors/lib/actor_1037*.c`, `src/rooms/acropolis_bridge/*.c`).
void       Gp_AnimWritePoseBlend(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
                                 s32 arg5);
void       Gp_AnimWritePoseCopy(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
                                s32 arg5);
void       Gp_AnimTickIndex(GpAnimCtx* arg0, s32 arg1);
void       func_800B4538(GpAnimCtx* arg0, s32 arg1, s32 arg2, u16 arg3, s32 arg4, s32 arg5,
                         s32 arg6);
GpAnimRec* Gp_AnimGetRec(GpAnimCtx* arg0, GpAnimSlot* arg1);
void       func_800B46A4(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3);
void       func_800B4754(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3);
void       Gp_AnimPlaySlot(GpAnimCtx* arg0, s32 arg1, s32 arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6,
                           void* arg7);
void       Gp_SaveEnemyPose(GpEnemy* arg0);
void       Gp_SpawnArea(GpAreaKey* arg0);
void       Gp_ApplyAreaTmdFlags(void);
void       Gp_ReparentCoord(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1);
GpWorkObj* Gp_FindWorkById(u16 arg0);
void       Gp_SetTmdBytes(TmdObject* arg0, s32 arg1, s32 arg2);
void       Gp_SetCurAreaFlag2(s32 arg0);
s32        Gp_GetAreaFlag2(GpAreaKey* arg0);
GpAreaObj* Gp_GetAreaObj(GpAreaKey* arg0);
void       func_800B5A48(GpAreaKey* arg0, GpAreaObj* arg1);
void       Gp_SetAreaObjId(GpAreaKey* arg0, s32 arg1, s32 arg2);
void       Gp_SetAreaFlag2(s32 arg0, GpAreaKey* arg1);
GpAreaObj* Gp_GetNestedAreaObj(GpAreaKey* arg0);
GpAreaRec* Gp_GetNestedAreaRec(GpAreaKey* arg0);
void       Gp_SetAreaFlag0(GpAreaKey* arg0);
void       Gp_SyncAreaKeyIndex(GpAreaKey* arg0);
s32        Gp_FindChildType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3);
s32        Gp_FindChildExceptType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3);
s32        Gp_ExitChildrenType9(Task* arg0);
s32        Gp_SendMsgType9(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Input for `Gp_MakeDirOffset`. `field_2` is the signed length subtracted
/// from `SquareRoot0(Gfx_ApplyMatrixNoSf(delta, delta))` (the difference
/// is then forced `<= 0`). `pos` is the far end of that delta.
typedef struct _GpDirSrc {
    /* 0x00 */ byte    pad_0[2];
    /* 0x02 */ s16     field_2;
    /* 0x04 */ byte    pad_4[4];
    /* 0x08 */ SVECTOR pos;
} GpDirSrc;
/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by `Gp_MakeDirOffset`.
/// `vec` is the `arg1->pos - arg0` delta (normalized in place);
/// `mtx` is the transpose of `gGfxViewCoord.workm`.
typedef struct _GpDirScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ MATRIX  mtx;
} GpDirScratch;
STATIC_ASSERT_SIZEOF(GpDirScratch, 0x28);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawFloorQuad`.
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

/// Draws a semi-transparent textured square of side `arg1` on the XZ plane,
/// anchored at `arg2` (or at the coordinate's own origin when `arg2` is
/// `NULL`), transformed by `arg0->workm` and linked into `gGpuCurrentOt`
/// at the largest corner `otz`.
void Gp_DrawFloorQuad(GsCOORDINATE2* arg0, u32 arg1, SVECTOR* arg2);
/// Builds a camera-space offset from `arg0` toward `arg1->pos`, scaled
/// by `-abs(length - arg1->field_2)`, and writes it to `arg2`.
void Gp_MakeDirOffset(SVECTOR* arg0, GpDirSrc* arg1, SVECTOR* arg2);
void Gp_FreeSlot4TmdBuffers(void);
/// Looks up `arg0` as `GpBit2Rec.field_0` in
/// `Gp_Bit2Banks[Mc_SaveData.at4.loc.stage]`. On a hit, publishes the record's
/// item id / extra / stack count into `Gp_PubItemId` / `Gp_PubItemLoc` /
/// `D_80114DDE` / `Gp_PubItemQty` and returns 1.
s32 Gp_LookupBit2Item(s32 arg0);
/// Walks `Gp_Bit2Banks[Mc_SaveData.at4.loc.area / stage]` for a `GpEnemyPlace`
/// whose `field_0` equals `arg0`. If the packed 2-bit flag at
/// `Gp_Bit2Banks[gGameSession->at4.loc.stage].field_4` is non-zero, spawns that
/// placement via `Gp_SpawnEnemyFromTable` (same coord/yaw writeback as `Gp_SpawnPlaces`).
void Gp_SpawnPlaceById(u16 arg0);
void Gp_SpawnPlaces(GpAreaKey* arg0);
void Gp_ApplyItemMap(void);
s32  Gp_ConsumeSlotQty(s32 arg0, s32 arg1);
s32  Gp_EquipRelatedBank(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
/// Equips related item `arg2` (ids `0xA0..0xBF`) onto save-slot `arg1`
/// (ids `0x80..0x9F`) in the table selected by `arg0`. Tries `Gp_QtyById0`
/// then `Gp_QtyById1` for a matching related id. `arg3 < 0` uses that
/// row's max qty. Returns the stored count, 0 if `arg3 == 0`, or -1.
s32 Gp_EquipRelatedItem(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);

// Functions defined in this module but not previously declared anywhere.
// Without a prototype m2c cannot type a call to them and the decompiled
// seed fails to compile ('invalid use of void expression') - the single
// largest cause of unusable seeds in the bulk m2c pass.

void  func_800B3448(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);
Task* func_800B2968(void);
void  Gp_SetStreamBuf(void* arg0);
void  func_800B1EFC(Task* arg0);
void  func_800B0928(Task* arg0, Task* arg1, s32 arg2, s32 arg3, s32 arg4);
/// Turns the slot-3 skeleton's head toward the world point in `arg1`'s
/// translation (`coord.t`). Sums the first five `GsCOORDINATE2` transforms of
/// `arg0->extra` to get the head's own position and orientation, takes the
/// offset to the target through `ratan2` as a yaw/pitch pair, steps toward it
/// by `arg4 / 0x1000` of the remaining angle and clamps the result to `arg2`
/// yaw and `arg3` pitch before writing the rotation with `RotMatrix`.
void func_800B0CF4(Task* arg0, GsCOORDINATE2* arg1, s32 arg2, s32 arg3, s32 arg4);
/// Persistent head-tracking state for `func_800B17D4`. `yawLimit` /
/// `pitchLimit` are the base clamps (widened to the head's current pose each
/// step), `rate` the per-step fraction of the remaining angle in `/ 0x1000`,
/// `lastPitch` the previous unwrapped pitch and `inited` whether it is valid.
typedef struct _GpHeadAim {
    /* 0x0 */ s16 yawLimit;
    /* 0x2 */ s16 pitchLimit;
    /* 0x4 */ s16 rate;
    /* 0x6 */ s16 lastPitch;
    /* 0x8 */ s8  inited;
} GpHeadAim;
STATIC_ASSERT_SIZEOF(GpHeadAim, 0xA);

/// `func_800B0928` with the limits and step taken from `arg2` and the target
/// being `arg1`'s head: composes the first five `GsCOORDINATE2` transforms of
/// both tasks (plus the `D_80093A28` head offset) to get each head's world
/// position, takes the offset in `arg0`'s head frame through `ratan2`, unwraps
/// the pitch against `arg2->lastPitch` when it jumps by more than 0x800, steps
/// toward it by `arg2->rate / 0x1000`, clamps, and writes the head rotation.
void func_800B17D4(Task* arg0, Task* arg1, GpHeadAim* arg2);

#endif // GAMEPLAY_1BC_H
