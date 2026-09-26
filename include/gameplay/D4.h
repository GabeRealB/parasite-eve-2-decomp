#ifndef GAMEPLAY_D4_H
#define GAMEPLAY_D4_H

#include "common.h"

#include "gameplay/areaplace.h"
#include "gameplay/message.h"
#include "main/display.h"
#include "main/task.h"

struct GpAreaKey;
struct _GpActorArg;
struct _GpAreaFlagRec;
struct _GpAreaRec;
struct _GpGridParams;
struct _GpObj4A;
struct _GpObj3A;

/// Direction-action handlers copied to the stack by func_800AD6BC.
typedef struct _GpDirActionTable {
    void (*funcs[7])(void);
} GpDirActionTable;
STATIC_ASSERT_SIZEOF(GpDirActionTable, 0x1C);

extern const GpDirActionTable Gp_DirActionFns;

/// 8-byte id/handler record. `Task::msgTable` points at a table of these
/// (`Gp_Slot4MsgTable`, `D_8010FB90`, …). `Gp_DispatchMsg` walks it and calls the
/// matching handler with the same four arguments. Terminator id is
/// `0x7FFFFFFF`.
typedef s32 (*GpMsgHandler)(Task* task, s32 msgId, s32 arg2, s32 arg3);

typedef struct _GpMsgEntry {
    /* 0x0 */ s32          id;
    /* 0x4 */ GpMsgHandler handler;
} GpMsgEntry;
STATIC_ASSERT_SIZEOF(GpMsgEntry, 8);

/// Fallback message handlers installed by `func_800E31E8` for pointer slot 7.
extern GpMsgEntry D_8010FAD4[];

/// Per-index flag object pointed to by `Gp_FlagBanks`. `field_4[0]` / `[1]` are
/// bitmasks (ids 1–32 and 33–64) cleared by `Gp_ClearFlagBank` and set by
/// `Gp_MarkAreaVisited`.
typedef struct _GpFlagBank {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ s32  field_4[2];
} GpFlagBank;
STATIC_ASSERT_SIZEOF(GpFlagBank, 0xC);

/// Main-executable table of `GpFlagBank*`, indexed by slot / session field_7.
extern GpFlagBank* Gp_FlagBanks[];

/// Alias for `Gp_FlagBanks[2]`, combined with stage 3 flags for map visibility.
extern GpFlagBank* D_80060A38;

/// Per-stage wrapper. `field_0` is a 3-level table of bytes, indexed
/// 1-based by `GameSession.at4.loc.area` / `at4.loc.room` / `at4.loc.view`.
/// `Gp_GetViewIndex` returns the innermost byte (camera / view index).
typedef struct _GpViewIndexTbl {
    /* 0x0 */ u8*** field_0;
} GpViewIndexTbl;

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`.
extern GpViewIndexTbl* Gp_ViewIndexTables[];

/// 8-byte command record. `GpSprtRec.field_4` points at a 0xFFFF-terminated
/// list of these. `Gp_ViewSprtCmdEmpty` returns whether `field_2` is zero; when it
/// is, `Gp_LinkViewSprts` skips the first record, otherwise it clears
/// `gDisplayState.at100.flags.imageSource`. `field_0` is the start index into
/// `GpSprtRec.field_0`; `field_2` is the count. `field_4` nonzero skips
/// OT-linking each prim. `field_5` nonzero skips `Gp_LinkSprtCmd` and
/// `Gp_SetSprtShadeBits`.
typedef struct _GpSprtCmd {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ u8   field_5;
    /* 0x6 */ byte pad_6[2];
} GpSprtCmd;
STATIC_ASSERT_SIZEOF(GpSprtCmd, 8);

/// 0x14-byte SPRT source record. `GpSprtRec.field_0` is an array of these.
/// `Gp_LinkSprtCmd` / `Gp_EmitSprts` index from `GpSprtCmd.field_0` for
/// `field_2` entries. `otz` is the OT depth. `Gp_EmitSprts` copies the
/// remaining fields into a merged `DR_TPAGE`+`SPRT` in `gGpuPrimCursor`.
/// `flags` bit 0 skips the RGB copy (shade-tex); the byte is OR'd into
/// the SPRT code.
typedef struct _GpSprtElem {
    /* 0x00 */ u16 tpage;
    /* 0x02 */ u16 clut;
    /* 0x04 */ s16 w;
    /* 0x06 */ s16 h;
    /* 0x08 */ s16 x0;
    /* 0x0A */ s16 y0;
    /* 0x0C */ u16 otz;
    /* 0x0E */ u8  u0;
    /* 0x0F */ u8  v0;
    /* 0x10 */ u8  r0;
    /* 0x11 */ u8  g0;
    /* 0x12 */ u8  b0;
    /* 0x13 */ u8  flags;
} GpSprtElem;
STATIC_ASSERT_SIZEOF(GpSprtElem, 0x14);

/// 10-byte draw-area record. `GpSprtRec.field_8` points at a list terminated
/// by depth 0xFFFF. Each rectangle clips the view sprites up to its OT depth.
typedef struct _GpDrawAreaRec {
    /* 0x0 */ RECT rect;
    /* 0x8 */ u16  depth;
} GpDrawAreaRec;
STATIC_ASSERT_SIZEOF(GpDrawAreaRec, 0xA);

/// 12-byte per-view record in tables pointed to by `Gp_SprtTables`.
/// Indexed 1-based by the `Gp_ViewIndexTables` camera / view byte.
/// `Gp_GetViewSprtExtra` returns `field_8`. `Gp_ViewSprtCmdEmpty` reads `field_4`.
typedef struct _GpSprtRec {
    /* 0x0 */ GpSprtElem*    field_0;
    /* 0x4 */ GpSprtCmd*     field_4;
    /* 0x8 */ GpDrawAreaRec* field_8;
} GpSprtRec;
STATIC_ASSERT_SIZEOF(GpSprtRec, 0xC);

/// Per-stage wrapper. `field_0` is an array of `GpSprtRec*`, indexed
/// 1-based by `GameSession.at4.loc.area` / `GpAreaKey.area`.
typedef struct _GpSprtTbl {
    /* 0x0 */ GpSprtRec** field_0;
} GpSprtTbl;

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`.
extern GpSprtTbl* Gp_SprtTables[];

/// 0x10-byte per-room record in tables pointed to by `Gp_RoomObjTables`.
/// Indexed 1-based by `GameSession.at4.loc.room` / `GpAreaKey.room`.
/// `Gp_LinkRoomObjects` / `Gp_LinkRoomObjectsSpawn` parent `field_0` to `&gGfxViewCoord` and
/// link the `field_4` / `field_8` (`GpObj4A`) and `field_C` (`GpObj3A`) arrays.
typedef struct _GpRoomObjRec {
    /* 0x0 */ struct _GpGridParams* field_0;
    /* 0x4 */ struct _GpObj4A*      field_4;
    /* 0x8 */ struct _GpObj4A*      field_8;
    /* 0xC */ struct _GpObj3A*      field_C;
} GpRoomObjRec;
STATIC_ASSERT_SIZEOF(GpRoomObjRec, 0x10);

/// Per-stage wrapper. `field_0` is an array of `GpRoomObjRec*`, indexed
/// 1-based by `GameSession.at4.loc.area` / `GpAreaKey.area`.
typedef struct _GpRoomObjTbl {
    /* 0x0 */ GpRoomObjRec** field_0;
} GpRoomObjTbl;

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`.
extern GpRoomObjTbl* Gp_RoomObjTables[];

/// 0x38-byte record in tables pointed to by `Gp_WarpTables`. Indexed
/// 1-based by `GpAreaKey.stage` / `area`, then
/// `(Gp_DirNibble >> 4)`. `Gp_CommitWarp` copies one record onto the
/// stack and writes `field_36` into `GpSaveLoc.field_6`. Leading `s32`
/// keeps the type 4-aligned so the 56-byte assign is `lw`/`sw`.
/// func_800AA548 uses the transforms at 0x00 / 0x14 to spawn the player /
/// companion, field_28 as a sound event, and field_34 as the initial view.
typedef struct _GpWarpRec {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x10];
    /* 0x14 */ s32  field_14;
    /* 0x18 */ byte pad_18[0x10];
    /* 0x28 */ s32  field_28;
    /* 0x2C */ s32  field_2C;
    /* 0x30 */ s32  field_30;
    /* 0x34 */ u8   field_34;
    /* 0x35 */ u8   field_35;
    /* 0x36 */ u16  field_36;
} GpWarpRec;
STATIC_ASSERT_SIZEOF(GpWarpRec, 0x38);

/// Per-stage pointer table. Index is `GameSession.at4.loc.stage - 1`. Each
/// entry is an array of `GpWarpRec*`, indexed 1-based by
/// `GameSession.at4.loc.area` / `GpAreaKey.area`.
extern GpWarpRec** Gp_WarpTables[];

/// Spawn transform with a full-word yaw followed by X/Y/Z. Passed to
/// Gp_SpawnPlayer as GpActorArg, whose yaw reader uses the low halfword.
typedef struct _GpSpawnTransform {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} GpSpawnTransform;
STATIC_ASSERT_SIZEOF(GpSpawnTransform, 0x10);

extern GpSpawnTransform D_80114CB0;

/// 0x1C-byte primitive slot in the `Gp_SprtLists` lists. `Gp_LinkSprtCmd`
/// OT-links each slot and advances `Gp_SprtCursor` by one. `Gp_SetSprtShadeBits`
/// walks the current view's records and sets or clears bit 0 of `field_F`
/// on `field_2` consecutive slots (nonzero arg sets, zero arg clears).
typedef struct _GpSprtPrim {
    /* 0x00 */ u32  tag;
    /* 0x04 */ byte pad_4[0xB];
    /* 0x0F */ u8   field_F;
    /* 0x10 */ byte pad_10[0xC];
} GpSprtPrim;
STATIC_ASSERT_SIZEOF(GpSprtPrim, 0x1C);

/// Merged `DR_TPAGE` + `SPRT` (0x1C) written into `gGpuPrimCursor` by
/// `Gp_EmitSprts`. `MargePrim` concatenates the tpage packet onto the
/// sprite so they share one OT entry.
typedef struct _GpTpageSprt {
    /* 0x00 */ DR_TPAGE tpage;
    /* 0x08 */ SPRT     sprt;
} GpTpageSprt;
STATIC_ASSERT_SIZEOF(GpTpageSprt, 0x1C);

/// Dual-buffer primitive list heads, indexed by `gDisplayState.drawBuffer`.
/// Allocated by `Gp_AllocSprtLists`; `Gp_SprtLists[1]` is the second half of
/// the same block.
extern GpSprtPrim* Gp_SprtLists[];

/// Cursor into the current `Gp_SprtLists` list. Set by `Gp_LinkViewSprts` /
/// `Gp_SetSprtShadeBits`, advanced by `Gp_LinkSprtCmd`.
extern GpSprtPrim* Gp_SprtCursor;

/// 0xC-byte 0xFF-terminated list at inner `GpAreaRec.field_4`, also
/// stored in `D_80114C68`. `Gp_PollAreaCdLoads` matches `field_0` against
/// `GpAreaPlace.entryId`. `field_2` is the packed CdCmd 0x21 location
/// (`% 100` / `/ 100` when `>= 100`); `field_4` indexes `D_8010CAD0`.
typedef struct _GpCdRec0C {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ byte pad_5[7];
} GpCdRec0C;
STATIC_ASSERT_SIZEOF(GpCdRec0C, 0xC);

/// Inner `Gp_GetNestedAreaRec` record as used by `Gp_PollAreaCdLoads`: `field_0` is
/// the placement list, `field_4` is the 0xC-byte list.
typedef struct _GpCdAreaRec {
    /* 0x0 */ GpAreaPlace* field_0;
    /* 0x4 */ GpCdRec0C*   field_4;
} GpCdAreaRec;
STATIC_ASSERT_SIZEOF(GpCdAreaRec, 8);

/// 2-byte table at `D_8010CAD0`. `Gp_PollAreaCdLoads` reads `field_0` at
/// `GpCdRec0C.field_4` (stride 2) as the CdCmd 0x21 param1[2] base.
typedef struct _GpTbl2 {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GpTbl2;
STATIC_ASSERT_SIZEOF(GpTbl2, 2);

extern GpTbl2 D_8010CAD0[];

/// Phase for `Gp_PollAreaCdLoads` (0 init, 1 walk/enqueue, 2 wait idle).
/// `Gp_LoadWaitAreaCd` clears it when phase 1 (`func_800AA120`) finishes
/// so phase 2 can start.
extern s16 Gp_AreaCdPhase;

/// Inner area rec from `Gp_GetNestedAreaRec`, shared by `Gp_PollAreaCdLoads` /
/// `func_800AA120`.
extern GpCdAreaRec* D_80114C64;

/// Cursor into the inner rec's 0xC-byte list (`GpCdAreaRec.field_4`).
extern GpCdRec0C* D_80114C68;

/// Cursor into the inner rec's placement list (`GpCdAreaRec.field_0`), walked
/// as `GpAreaPlace`: `entryId` names the entry of the 0xC-byte list being loaded
/// (0 skips the record), and the tail bytes are forwarded into the load command
/// - `pad_C` as param1[0] (0 skips), `tpage` / `clut` as param2[2] / param2[3].
extern GpAreaPlace* Gp_CdRecCur;

/// Phase for `func_800AA120`. `Gp_LoadWaitAreaCd` clears it when entering
/// its own phase 1.
extern u16 D_80114C70;

/// Match-found flag in the halfword immediately after the phase. Keep its
/// compiler symbol separate so loop optimization does not retain the phase
/// address across CdCmd_Enqueue; the assembler alias preserves the data layout.
extern u16 D_80114C72 asm("D_80114C70+2");

/// Phase for `Gp_LoadWaitAreaCd` (0 init, 1 `func_800AA120`, 2 `Gp_PollAreaCdLoads`).
/// `Gp_LoadWaitSave` clears it when advancing to this task state.
extern u16 D_80114C74;

/// Dual-buffer fullscreen TILE overlay, indexed by `gDisplayState.otBuffer`.
/// Paired with `Gp_FadeTpages`. Used by `Gp_FadeGrayHold` and the neighboring
/// D4 fade-overlay task states.
extern TILE Gp_FadeTiles[2];

/// Dual-buffer `DR_TPAGE` (code `0xE1000240`) paired with `Gp_FadeTiles`.
extern DR_TPAGE Gp_FadeTpages[2];

/// 5-byte table at `Gp_ConfigCdTable`. `Gp_EnqueueConfigCd` copies it to the stack and
/// indexes it 1-based by `Player_Status.field_26`; the byte is CdCmd 0x21
/// param2[0].
typedef struct _GpTbl5 {
    /* 0x0 */ u8 field_0[5];
} GpTbl5;
STATIC_ASSERT_SIZEOF(GpTbl5, 5);

/// 8-byte pair of byte-table pointers at `D_801149FC`. `Gp_MsgPlayerDirFacing`
/// indexes by `(Gp_DirByte & 0x70) >> 4`. `Gp_DirFlags & 0x100` selects
/// `field_4` over `field_0`. The byte at `(Gp_DirByte & 0xF) -
/// GameActor.field_82` is stored into `GameActor.field_930`.
typedef struct _GpDirPair {
    /* 0x0 */ u8* field_0;
    /* 0x4 */ u8* field_4;
} GpDirPair;
STATIC_ASSERT_SIZEOF(GpDirPair, 8);

extern GpDirPair D_801149FC[];

/// 8-byte record in `D_80114198` / `D_801141F0` / `D_80114248`. Indexed by
/// `GameFlag_GetNibble(0x4B / 0x4C / 0x4D)`. `field_0` is a per-room byte
/// list, 1-based by `Mc_SaveData.at4.loc.area`; `field_4` is the stage id
/// (`Mc_SaveData.at4.loc.stage`). `Gp_ApplyNpcRoomSnd` tests the room byte (second
/// table with `& 0xF`) to choose the `Snd_SetModeFlag` argument.
/// `Gp_PickCompanion` uses the same tables to pick `Mc_SaveData.companionType`.
typedef struct _GpNpcRoomRec {
    /* 0x0 */ u8*  field_0;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ byte pad_5[3];
} GpNpcRoomRec;
STATIC_ASSERT_SIZEOF(GpNpcRoomRec, 8);

extern GpNpcRoomRec D_80114198[];
extern GpNpcRoomRec D_801141F0[];
extern GpNpcRoomRec D_80114248[];

/// 4-byte record in 0xFF-terminated lists walked by `Gp_ApplyAreaRecs`.
/// `field_0` indexes `Gp_AreaTables` (same role as `GpAreaKey.stage`);
/// `field_1` indexes that table (same role as `GpAreaKey.area`);
/// `field_2` is the id written by `Gp_SetAreaObjId`. High nibble of `field_3`
/// is a `Mc_SaveData.gameMode` filter (0 = always, 0x10 if 0 or 2, 0x20 if
/// 1 or 3); low nibble nonzero sets `GpAreaObj.field_1` bit 2, else clears.
typedef struct _GpAreaApplyRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} GpAreaApplyRec;
STATIC_ASSERT_SIZEOF(GpAreaApplyRec, 4);

void Gp_EnqueueViewCd(Task* task);
void Gp_PostDirIfCapIdle(void);
void Gp_RunDirAction(void);
void Gp_SetupDirWarp(void);
void Gp_FadeDirWaitMsg(void);
void Gp_CommitWarp(void);
void Gp_WarpPhase4(void);
void Gp_MsgPlayerDirFacing(void);
void Gp_CommitDirWarp(void);
void Gp_PumpTmdStream(Task* task);
s32  Gp_DispatchMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void Gp_LinkViewSprts(void);
/// Alloc dual-buffer merged `DR_TPAGE`+`SPRT` lists into `Gp_SprtLists`
/// from the current view's `GpSprtRec` records. Byte size is the sum of
/// each record's `field_2`, times two 0x1C slots. Records with
/// `field_5` set are skipped. RGB is `0x8000`; SPRT code is `0x65`.
void Gp_AllocSprtLists(void);
/// 1-based index of `(u8)arg0` in the current room's `Gp_ViewIndexTables` byte
/// list. Length is the `Gp_ViewCountTables` cell as an s16. Returns 0 if absent.
s8    Gp_FindViewIndex(s32 arg0);
s32   Gp_GetViewIndex(void);
void* Gp_GetViewSprtExtra(void);
void  Gp_ApplyAreaRecs(GpAreaApplyRec* arg0);

/// 0xFF-terminated `GpAreaFlagRec` lists applied by `Gp_ApplyNewGameAreaFlags` to
/// `Gp_AreaTableStg1` / `Gp_AreaTableStg2` / `Gp_AreaTableStg4` / `Gp_AreaTableStg5` (stages 1, 2,
/// 4, 5 of `Gp_AreaTables`).
extern struct _GpAreaFlagRec Gp_NewGameFlagsStg1[];
extern struct _GpAreaFlagRec Gp_NewGameFlagsStg2[];
extern struct _GpAreaFlagRec Gp_NewGameFlagsStg4[];
extern struct _GpAreaFlagRec Gp_NewGameFlagsStg5[];

/// `Gp_AreaTables[1]`, `[2]`, `[4]`, `[5]`. Splat labels the later slots as
/// their own symbols; `Gp_ApplyNewGameAreaFlags` loads each as a `GpAreaRec*`.
extern struct _GpAreaRec* Gp_AreaTableStg1;
extern struct _GpAreaRec* Gp_AreaTableStg2;
extern struct _GpAreaRec* Gp_AreaTableStg4;
extern struct _GpAreaRec* Gp_AreaTableStg5;

/// Per-stage signed counts, indexed by `GameSession.at4.loc.stage - 1`.
/// `Gp_RebuildAreaIdBits` loops area ids `1..count` when the stage is 1–5.
extern s8 Gp_AreaIdCounts[];

/// For each area id `1..Gp_AreaIdCounts[stage-1]`, set or clear the matching
/// bit in `Gp_AreaIdBits`. The bit is set only when that area object's
/// `field_1` bit 2 is set and `Gp_GetAreaFlag2` returns 0.
void Gp_RebuildAreaIdBits(void);

// Functions defined in this module but not previously declared anywhere.
// Without a prototype m2c cannot type a call to them and the decompiled
// seed fails to compile ('invalid use of void expression') - the single
// largest cause of unusable seeds in the bulk m2c pass.

void Gp_FinishLoadWait(Task* task);
void func_800A99B4(void);
void Gp_EnqueueHeldWeaponCd(void);
void func_800ABFF8(void);
void func_800AC000(void);
void Gp_EnqueueConfigCd(s32 arg0);
void Gp_LoadViewAndCd(s32 arg0);
void Gp_LoadViewImages(void);

void Gp_AllocSprtListsTask(Task* task);
void Gp_CommitSpawnLoc(Task* task);
void Gp_FlashWhiteTask(Task* task);
void Gp_LoadStateTask(Task* task);
void Gp_LoadWaitDispatch(Task* task);
void Gp_SetupSprtDisplay(Task* task);
void func_800AC0F0(Task* task);
void func_800AD50C(Task* task);
void func_800AD5B8(Task* task);

#endif // GAMEPLAY_D4_H
