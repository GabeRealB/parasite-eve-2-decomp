#ifndef GAMEPLAY_1BC_H
#define GAMEPLAY_1BC_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// 0x60-byte enemy work object allocated by `Gp_AllocEnemy`
/// ("new_enemy ---> NULL"). Stored in `Task::spawnArg2`; `task` back-points
/// at the owner. `node` is the `Gp_UnlinkNode` list entry at +0x10.
/// `field_8` / `field_A` are the same work-id / type halfwords as
/// `GpWorkObj` (`Gp_SpawnAtPlace` packs `GpEnemyPlace.field_0` /
/// `field_4` into `field_8` and copies `field_2` into `field_A`).
/// Same object family as `GpObj4C` / `GpObj54`: `func_800A4904` ORs bit
/// 0x80 into `field_4E` or claims a `field_54` slot via `Gp_ClaimSlot18`.
/// `field_40` is the signed value passed to `Gp_DrawHudNumbers`; `field_50`
/// is the same `GpPairSrcE*` slot as `GpObj50` / `GpObj5C` / `GpObj5D`.
/// `field_18` is a `GsCOORDINATE2*` (`&Gfx_ViewCoord` from `Gp_AllocEnemy`);
/// `Gp_UpdateLinkXforms` reads it from the `GpLinkNode` overlay as `coord`.
/// `field_1C` / `field_2C` are local / player-relative `VECTOR3`s filled
/// by `Gp_UpdateLinkXforms`.
/// `field_3C` is the `GpAreaPlace*` stored by `Gp_SpawnArea` (same slot as
/// `GpWorkObj.field_3C`).
/// `field_4B` is a non-zero occupancy tag written into `McPosRec.field_3`
/// by `Gp_SaveEnemyPose` (defaulted to 1 when the caller left it 0).
/// `Gp_RemapActorColor` remaps the actor color matrix from `field_4E` mode
/// bits; bit 0x80 with `field_4B == 0` is a sine flicker, and
/// `field_4C & 0xC` enables the default remap. `field_4F` is the 0x10
/// blend timer started by `Gp_SetLightMode`; `Gp_UpdateActorColor` GPF/GPL-lerps
/// the previous mode (bits 2-3) toward the current mode (bits 0-1)
/// while it is positive.
typedef struct _GpEnemy {
    /* 0x00 */ Task*          task;
    /* 0x04 */ byte           pad_4[4];
    /* 0x08 */ u16            field_8;
    /* 0x0A */ u16            field_A;
    /* 0x0C */ s32            field_C;
    /* 0x10 */ GpLinkNode     node;
    /* 0x18 */ GsCOORDINATE2* field_18;
    /* 0x1C */ VECTOR3        field_1C;
    /* 0x28 */ byte           pad_28[4];
    /* 0x2C */ VECTOR3        field_2C;
    /* 0x38 */ byte           pad_38[4];
    /* 0x3C */ void*          field_3C;
    /* 0x40 */ s16            field_40;
    /* 0x42 */ byte           pad_42[9];
    /* 0x4B */ u8             field_4B;
    /* 0x4C */ u8             field_4C;
    /* 0x4D */ byte           pad_4D;
    /* 0x4E */ u8             field_4E;
    /* 0x4F */ u8             field_4F;
    /* 0x50 */ GpPairSrcE*    field_50;
    /* 0x54 */ byte           pad_54[0xC];
} GpEnemy;
STATIC_ASSERT_SIZEOF(GpEnemy, 0x60);

/// Callback for GpEnemy + Task state handlers (entries in `Gp_EnemyWaitFuncs`).
typedef void (*GpEnemyTaskFunc)(GpEnemy* enemy, Task* task);

/// Fixed-size table of `GpEnemyTaskFunc` callbacks. Copied onto the stack by
/// `Gp_EnemyDispatch` so the call uses a local jump table.
typedef struct {
    GpEnemyTaskFunc funcs[3];
} GpEnemyTaskFuncTable3;

/// Three-entry dispatcher table: `Gp_EnemyWaitStart`, `Gp_EnemyWaitTick`, `Gp_DestroyEnemy`.
extern GpEnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

void func_800B25B0(void);

/// Source object for `Gp_AnimInitCtx` / `Gp_AnimInitCtxSlots`. Word at 0x30 is
/// copied into the dest context; the address of 0x34 is stored as dest
/// `field_4` (base of 0x50-byte `GpAnimMtxRec` records in `func_800B3448`
/// / `Gp_AnimWritePoseBlend` / `Gp_AnimWritePoseCopy`).
typedef struct _GpAnimObj {
    /* 0x00 */ byte  pad_0[0x30];
    /* 0x30 */ void* field_30;
    /* 0x34 */ byte  field_34;
} GpAnimObj;

/// Pose pair used by `Gp_AnimWritePoseBlend` / `Gp_AnimWritePoseCopy`. Translation is
/// GPF/GPL-blended (`Gp_AnimWritePoseBlend`) or copied (`Gp_AnimWritePoseCopy`) into
/// `GpAnimMtxRec.mtx.t` when `GpAnimSlot.field_B == 1`; rotation is
/// GPF/GPL-blended with the other pose and fed to `RotMatrix_gte`.
typedef struct _GpAnimPose {
    /* 0x00 */ SVECTOR trans;
    /* 0x08 */ SVECTOR rot;
} GpAnimPose;
STATIC_ASSERT_SIZEOF(GpAnimPose, 0x10);

/// Packed 11-10-11 signed vector (one word). `Gp_AnimBlendPacked` unpacks each
/// component `<< 3` into an `SVECTOR` and packs the interpolated result
/// back. `func_800B3448` dispatches here when `GpAnimSlot.field_B == 4`.
typedef struct _GpPackedSvec {
    s32 vx : 11;
    s32 vy : 10;
    s32 vz : 11;
} GpPackedSvec;
STATIC_ASSERT_SIZEOF(GpPackedSvec, 4);

/// Packed translation + rotation (no `SVECTOR` pad). `Gp_AnimBlendPose`
/// GPF/GPL-blends `vx`/`vy`/`vz` and copies `rx`/`ry`/`rz` into
/// `GpAnimScratch80.vec0` / `vec1`. `func_800B3448` dispatches here when
/// `GpAnimSlot.field_B == 1`.
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
/// sources (`GpPackedSvec` when `field_B == 4`, `GpPackedPose` when
/// `field_B == 1`); `field_8` is an optional packed dest (`arg3` of
/// `func_800B3448`). `field_C` is `arg2` of `func_800B3448` (optional
/// translation dest); `field_10` is a copy of `GpAnimSlot.field_17`.
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

/// 0x50-byte dest record at `GpAnimCtx.field_4`, indexed by
/// `GpAnimSlot.field_14`. `Gp_AnimWritePoseBlend` / `Gp_AnimWritePoseCopy` write `mtx`
/// (rotation at +4, translation at +0x18) and clear `field_0`.
typedef struct _GpAnimMtxRec {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ MATRIX mtx;
    /* 0x24 */ byte   pad_24[0x2C];
} GpAnimMtxRec;
STATIC_ASSERT_SIZEOF(GpAnimMtxRec, 0x50);

/// 4-byte animation record. `func_800B3E74` / `func_800B3EE8` index this by
/// `GpAnimSlot::field_6` and copy `field_2 << 4` into the slot's
/// `field_C` / `field_E`. `field_3` is the opcode-like byte tested by
/// `func_800B3AA4` / `func_800B46A4` (signed < 0 continues a `field_0`
/// chain; `>= 0xC0` aborts) and stored into `field_B` by `Gp_AnimResetSlot`.
typedef struct _GpAnimRec {
    /* 0x00 */ u16 field_0;
    /* 0x02 */ u8  field_2;
    /* 0x03 */ u8  field_3;
} GpAnimRec;
STATIC_ASSERT_SIZEOF(GpAnimRec, 4);

/// Object behind each pointer in `GpAnimSlot::field_20` (same table as
/// `GpAnimCtx::field_0`). `field_0` is the base of 4-byte records.
/// `field_4` is a u16 table indexed by `GpAnimSlot::field_15`.
/// `field_8` is a table of pose banks indexed by `GpAnimSlot::field_B`
/// (`func_800B3448`); each bank is addressed with a 4-byte stride, so it is
/// `GpPackedSvec*` for `field_B == 4` and `GpPackedPose*` for `field_B == 1`.
typedef struct _GpAnimSet {
    /* 0x00 */ GpAnimRec*    field_0;
    /* 0x04 */ u16*          field_4;
    /* 0x08 */ GpPackedSvec* field_8[1];
} GpAnimSet;

/// 0x18-byte scratch `func_800B3448` allocates from `G_SCRATCH_HEAD` before
/// dispatching to `Gp_AnimBlendPose` / `Gp_AnimBlendPacked`; only `src` is
/// written.
typedef struct _GpAnimScratch18 {
    /* 0x00 */ s32            field_0;
    /* 0x04 */ GpAnimBlendSrc src;
} GpAnimScratch18;
STATIC_ASSERT_SIZEOF(GpAnimScratch18, 0x18);

/// 0x28-byte animation slot. `field_15` is this slot's index in the
/// `GpAnimCtx::field_C` array; `Gp_AnimTickSlot` / `Gp_AnimTickSlot2` /
/// `Gp_AnimTickSlot3` recover the base as `slot - slot->field_15`.
/// `field_0 == 0x7FFF` marks the slot inactive. `field_0`/`field_2` and
/// `field_4`/`field_6` are the two (set, frame) pairs. `field_10` is a
/// flags word (`func_800B4754` sets bit 0 when clamping `field_2`;
/// `Gp_AnimAdvanceSlot` / `Gp_AnimSeekSlotEx` / `func_800B46A4` / `func_800B4114` /
/// `func_800B4538` / `Gp_AnimPlaySlot` set bit 0/1 while walking `field_3`
/// links). `Gp_AnimSeekSlotEx` / `func_800B4114` / `func_800B4538` /
/// `Gp_AnimPlaySlot` also clear `field_17` and write a `<< 4` value into
/// `field_C` / `field_E` (`Gp_AnimSeekSlotEx` then overwrites both from
/// `recs[field_6].field_2`). `Gp_AnimPlaySlot` installs a
/// non-NULL last arg into `GpAnimCtx::field_0` and `field_20` first.
/// `Gp_AnimInitSlot` inits a passed-in slot (set index 0 becomes 1; a
/// negative set index is negated) and also writes `field_2` from the
/// same `field_4[field_15]` table as `field_6`. `Gp_AnimResetSlot` writes
/// `field_9 = 0x10`, copies `arg1` to both `field_14` and `field_15`,
/// and stores `recs[field_6].field_3 & 0xF` in `field_B`.
/// `Gp_AnimResetSlotEx` is the same init with separate `field_14` /
/// `field_15` arguments. `Gp_AnimWritePoseBlend` / `Gp_AnimWritePoseCopy` index
/// `GpAnimCtx.field_4` by `field_14`; `Gp_AnimWritePoseBlend` GPF/GPL-blends
/// pose translation when `field_B == 1`, `Gp_AnimWritePoseCopy` copies it.
/// `func_800B3448` dispatches `field_B == 1` to `Gp_AnimBlendPose` and
/// `field_B == 4` to `Gp_AnimBlendPacked`.
typedef struct _GpAnimSlot {
    /* 0x00 */ u16         field_0;
    /* 0x02 */ u16         field_2;
    /* 0x04 */ u16         field_4;
    /* 0x06 */ u16         field_6;
    /* 0x08 */ byte        pad_8;
    /* 0x09 */ u8          field_9;
    /* 0x0A */ byte        pad_A;
    /* 0x0B */ u8          field_B;
    /* 0x0C */ u16         field_C;
    /* 0x0E */ u16         field_E;
    /* 0x10 */ u16         field_10;
    /* 0x12 */ u16         field_12;
    /* 0x14 */ u8          field_14;
    /* 0x15 */ u8          field_15;
    /* 0x16 */ u8          field_16;
    /* 0x17 */ u8          field_17;
    /* 0x18 */ SVECTOR     field_18;
    /* 0x20 */ GpAnimSet** field_20;
    /* 0x24 */ byte        pad_24[4];
} GpAnimSlot;
STATIC_ASSERT_SIZEOF(GpAnimSlot, 0x28);

/// 0x14-byte context filled by `Gp_AnimInitCtx` (no `field_C`) and
/// `Gp_AnimInitCtxSlots` (also writes `field_C`). Nearby helpers index
/// `field_C` as a 0x28-byte slot array, `field_4` as a 0x50-byte
/// `GpAnimMtxRec` array, and `field_8` at a 0x10 stride.
typedef struct _GpAnimCtx {
    /* 0x00 */ GpAnimSet** field_0;
    /* 0x04 */ void*       field_4;
    /* 0x08 */ void*       field_8;
    /* 0x0C */ GpAnimSlot* field_C;
    /* 0x10 */ void*       field_10;
} GpAnimCtx;
STATIC_ASSERT_SIZEOF(GpAnimCtx, 0x14);

/// Object returned by `Gp_GetAreaObj` (`GpAreaRec.field_4`). `field_0` is a
/// signed id compared with `GpAreaKey.field_5`; `field_1` is a flags byte
/// (bits 0/1/2/4 in nearby 1BC / 1A8 helpers; `Gp_GetAreaFlag2` returns bit 1).
/// Full size unknown.
typedef struct _GpAreaObj {
    /* 0x00 */ s8 field_0;
    /* 0x01 */ u8 field_1;
} GpAreaObj;

/// 0x10-byte placement record in the 0xFF-terminated table at nested
/// `GpAreaRec.field_0` (`Gp_SpawnArea`). `field_0` is matched against
/// `GpAreaTmdRec.field_0`. `field_1` / `field_2` pack into `Gp_SpawnEnemyFromTable`
/// arg2. `field_4` / `field_6` / `field_8` are default world XYZ;
/// `field_A` is yaw (`GpCoordPlace.field_46` / `Gfx_RotMatrixY`).
/// `field_D` / `field_E` are copied to `TmdObject.field_24` / `field_25`
/// when `Task::spawnType == 1`.
typedef struct _GpAreaPlace {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u16 field_2;
    /* 0x04 */ s16 field_4;
    /* 0x06 */ s16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  pad_C;
    /* 0x0D */ u8  field_D;
    /* 0x0E */ u8  field_E;
    /* 0x0F */ u8  pad_F;
} GpAreaPlace;
STATIC_ASSERT_SIZEOF(GpAreaPlace, 0x10);

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
/// `GpWorkObj.field_3C` / `GpAreaPlace.field_0`. `field_5` is the
/// `Gp_SpawnEnemyFromTable` table index. `field_8` points at a halfword whose value 1
/// clears `TmdObject.field_C` bit 2 and 0x101 sets it (`Gp_ApplyAreaTmdFlags`), or
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
/// `GpAreaKey.field_2`. `field_0` is a nested table (`Gp_GetNestedAreaRec`
/// returns the entry at `field_5`; `Gp_GetNestedAreaObj` returns that entry's
/// `field_4`, a `GpAreaTmdRec` table). Outer `field_4` is the object
/// `Gp_GetAreaObj` returns.
typedef struct _GpAreaRec {
    /* 0x00 */ struct _GpAreaRec* field_0;
    /* 0x04 */ GpAreaObj*         field_4;
} GpAreaRec;
STATIC_ASSERT_SIZEOF(GpAreaRec, 8);

/// Location key used to index `Gp_AreaTables`. Bytes 2/3 select the table and
/// record; byte 5 indexes a nested table (`Gp_GetNestedAreaRec` / `Gp_GetNestedAreaObj`)
/// and is written from `GpAreaObj.field_0` by `Gp_SyncAreaKeyIndex`. Same 4-byte
/// prefix as `GameSessionFrom4` / `Mc_SaveData.field_4`. Full size unknown.
typedef struct _GpAreaKey {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
    /* 0x04 */ u8 field_4;
    /* 0x05 */ u8 field_5;
} GpAreaKey;

/// Overlay of `Task::spawnArg2` for sibling walkers. `field_A` high byte is
/// the work type (`Gp_FindChildType9` / `Gp_ExitChildrenType9` / `Gp_SendMsgType9` match 9;
/// `Gp_FindChildExceptType9` skips 9). `field_8` is the id compared against the search
/// key (`as_u16` / `as_u8`; `Gp_FindWorkById` matches `as_u16` on slot 4's
/// children). `field_3C` is a byte pointer compared with `GpAreaTmdRec.field_0`
/// by `Gp_ApplyAreaTmdFlags` on spawnType-1 children of slot 4. Full size unknown.
typedef struct _GpWorkObj {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ union {
        u16 as_u16;
        u8  as_u8;
    } field_8;
    /* 0x0A */ u16  field_A;
    /* 0x0C */ byte pad_C[0x30];
    /* 0x3C */ u8*  field_3C;
} GpWorkObj;

/// 8-byte mask/flag record. `Gp_SndMaskTable` is a 0-terminated table of these.
/// `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks` walk it: if `arg0 & mask`, apply `flags`
/// to `SndEvt_EnqueueType7` / `SndBank_SetEnableFlags`.
typedef struct _GpSndMaskRec {
    /* 0x0 */ s32 mask;
    /* 0x4 */ s32 flags;
} GpSndMaskRec;
STATIC_ASSERT_SIZEOF(GpSndMaskRec, 8);

/// Per-area pointer table. Index is `GpAreaKey.field_3` (also
/// `GameSession.field_7` via `&Game_Session->field_4`).
extern GpAreaRec* Gp_AreaTables[];

/// 0-terminated `GpSndMaskRec` table walked by `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks`.
extern GpSndMaskRec Gp_SndMaskTable[];

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
/// adding `arg2` in that space. If `arg1->sub` is world (`Gfx_ViewCoord`),
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
/// Walks `arg0->sub` up to world (`Gfx_ViewCoord`), composing each node's
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
/// 4-byte work at `Task::spawnArg2` for `func_800B2200`. `field_0`
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
/// `Gpu_CurrentOt[Task::spawnArg1]`, or (`spawnArg1 == 0`) into the head
/// of the current ordering table, backing up 0xA entries when the current
/// OT is not one of the two `Gpu_OrderingTables` roots.
void       func_800B2200(Task* arg0);
void       Gp_BlendAnimRot(GpAnimBlendSrc* arg0, GpAnimMtxRec* arg1, GpAnimSlot* arg2,
                           GpAnimScratch80* arg3);
void       Gp_AnimBlendPose(GpAnimBlendSrc* arg0, GpAnimMtxRec* arg1, GpAnimSlot* arg2);
void       Gp_AnimBlendPacked(GpAnimBlendSrc* arg0, GpAnimMtxRec* arg1, GpAnimSlot* arg2);
void       Gp_AnimAdvanceSlot(GpAnimCtx* arg0, s32 arg1);
void       Gp_AnimSeekSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800B3AA4(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       Gp_AnimInitCtx(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3);
void       Gp_AnimInitSlot(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3);
void       Gp_AnimTickSlot(GpAnimCtx* arg0, GpAnimSlot* arg1);
void       Gp_AnimTickSlot2(GpAnimCtx* arg0, GpAnimSlot* arg1);
void       Gp_AnimTickSlot3(GpAnimCtx* arg0, GpAnimSlot* arg1);
void       func_800B3E74(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3);
void       func_800B3EE8(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4);
void       Gp_AnimInitCtxSlots(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4);
void       func_800B3F84(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4);
void       Gp_AnimResetSlot(GpAnimCtx* arg0, s32 arg1, s32 arg2);
void       Gp_AnimResetSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       func_800B4114(GpAnimCtx* arg0, s32 arg1, u16 arg2, s32 arg3, s32 arg4);
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
/// `mtx` is the transpose of `Gfx_ViewCoord.workm`.
typedef struct _GpDirScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ MATRIX  mtx;
} GpDirScratch;
STATIC_ASSERT_SIZEOF(GpDirScratch, 0x28);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `func_800B4E54`.
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
/// `NULL`), transformed by `arg0->workm` and linked into `Gpu_CurrentOt`
/// at the largest corner `otz`.
void func_800B4E54(GsCOORDINATE2* arg0, u32 arg1, SVECTOR* arg2);
/// Builds a camera-space offset from `arg0` toward `arg1->pos`, scaled
/// by `-abs(length - arg1->field_2)`, and writes it to `arg2`.
void Gp_MakeDirOffset(SVECTOR* arg0, GpDirSrc* arg1, SVECTOR* arg2);
void Gp_FreeSlot4TmdBuffers(void);
/// Looks up `arg0` as `GpBit2Rec.field_0` in
/// `Gp_Bit2Banks[Mc_SaveData.field_7]`. On a hit, publishes the record's
/// item id / extra / stack count into `Gp_PubItemId` / `Gp_PubItemLoc` /
/// `D_80114DDE` / `Gp_PubItemQty` and returns 1.
s32 Gp_LookupBit2Item(s32 arg0);
/// Walks `Gp_Bit2Banks[Mc_SaveData.field_6 / field_7]` for a `GpEnemyPlace`
/// whose `field_0` equals `arg0`. If the packed 2-bit flag at
/// `Gp_Bit2Banks[Game_Session->field_7].field_4` is non-zero, spawns that
/// placement via `Gp_SpawnEnemyFromTable` (same coord/yaw writeback as `Gp_SpawnPlaces`).
void Gp_SpawnPlaceById(u16 arg0);
void Gp_SpawnPlaces(GameSessionFrom4* arg0);
void Gp_ApplyItemMap(void);
s32  Gp_ConsumeSlotQty(s32 arg0, s32 arg1);
s32  Gp_EquipRelatedBank(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
/// Equips related item `arg2` (ids `0xA0..0xBF`) onto save-slot `arg1`
/// (ids `0x80..0x9F`) in the table selected by `arg0`. Tries `Gp_QtyById0`
/// then `Gp_QtyById1` for a matching related id. `arg3 < 0` uses that
/// row's max qty. Returns the stored count, 0 if `arg3 == 0`, or -1.
s32 Gp_EquipRelatedItem(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);

#endif // GAMEPLAY_1BC_H
