#ifndef GAMEPLAY_D4_H
#define GAMEPLAY_D4_H

#include "common.h"

#include "main/display.h"
#include "main/task.h"

struct _GameSessionFrom4;
struct _GpActorArg;
struct _GpAreaKey;
struct _GpAreaFlagRec;
struct _GpAreaRec;
struct _GpGridParams;
struct _GpObj4A;
struct _GpObj3A;

/// 8-byte id/handler record. `Task::field_24` points at a table of these
/// (`D_8010D208`, `D_8010FB90`, …). `func_800AC464` walks it and calls the
/// matching handler with the same four arguments. Terminator id is
/// `0x7FFFFFFF`.
typedef s32 (*GpMsgHandler)(Task* task, s32 msgId, s32 arg2, s32 arg3);

typedef struct _GpMsgEntry {
    /* 0x0 */ s32          id;
    /* 0x4 */ GpMsgHandler handler;
} GpMsgEntry;
STATIC_ASSERT_SIZEOF(GpMsgEntry, 8);

/// Per-index flag object pointed to by `D_80060A30`. `field_4[0]` / `[1]` are
/// bitmasks (ids 1–32 and 33–64) cleared by `func_800ABEF8` and set by
/// `func_800ABF1C`.
typedef struct _GpFlagBank {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ s32  field_4[2];
} GpFlagBank;
STATIC_ASSERT_SIZEOF(GpFlagBank, 0xC);

/// Main-executable table of `GpFlagBank*`, indexed by slot / session field_7.
extern GpFlagBank* D_80060A30[];

/// Per-stage wrapper. `field_0` is a 3-level table of bytes, indexed
/// 1-based by `GameSession.field_6` / `field_5` / `field_4`.
/// `func_800AD284` returns the innermost byte (camera / view index).
typedef struct _GpCb54Tbl {
    /* 0x0 */ u8*** field_0;
} GpCb54Tbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb54Tbl* D_8010CB54[];

/// 8-byte command record. `GpCb68Rec.field_4` points at a 0xFFFF-terminated
/// list of these. `func_800ACF8C` returns whether `field_2` is zero; when it
/// is, `func_800AC688` skips the first record, otherwise it clears
/// `Display_State.field_100`. `field_0` is the start index into
/// `GpCb68Rec.field_0`; `field_2` is the count. `field_4` nonzero skips
/// OT-linking each prim. `field_5` nonzero skips `func_800AD410` and
/// `func_800AC960`.
typedef struct _GpCb68Obj {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ u8   field_5;
    /* 0x6 */ byte pad_6[2];
} GpCb68Obj;
STATIC_ASSERT_SIZEOF(GpCb68Obj, 8);

/// 0x14-byte SPRT source record. `GpCb68Rec.field_0` is an array of these.
/// `func_800AD410` / `func_800AC790` index from `GpCb68Obj.field_0` for
/// `field_2` entries. `otz` is the OT depth. `func_800AC790` copies the
/// remaining fields into a merged `DR_TPAGE`+`SPRT` in `D_80071190`.
/// `flags` bit 0 skips the RGB copy (shade-tex); the byte is OR'd into
/// the SPRT code.
typedef struct _GpCb68Elem {
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
} GpCb68Elem;
STATIC_ASSERT_SIZEOF(GpCb68Elem, 0x14);

/// 12-byte per-view record in tables pointed to by `D_8010CB68`.
/// Indexed 1-based by the `D_8010CB54` camera / view byte.
/// `func_800AD2E8` returns `field_8`. `func_800ACF8C` reads `field_4`.
typedef struct _GpCb68Rec {
    /* 0x0 */ GpCb68Elem* field_0;
    /* 0x4 */ GpCb68Obj*  field_4;
    /* 0x8 */ void*       field_8;
} GpCb68Rec;
STATIC_ASSERT_SIZEOF(GpCb68Rec, 0xC);

/// Per-stage wrapper. `field_0` is an array of `GpCb68Rec*`, indexed
/// 1-based by `GameSession.field_6` / `GameSessionFrom4.field_2`.
typedef struct _GpCb68Tbl {
    /* 0x0 */ GpCb68Rec** field_0;
} GpCb68Tbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb68Tbl* D_8010CB68[];

/// 0x10-byte per-room record in tables pointed to by `D_8010CB7C`.
/// Indexed 1-based by `GameSession.field_5` / `GameSessionFrom4.field_1`.
/// `func_800ACD2C` / `func_800AC4D8` parent `field_0` to `&D_80070F10` and
/// link the `field_4` / `field_8` (`GpObj4A`) and `field_C` (`GpObj3A`) arrays.
typedef struct _GpCb7CRec {
    /* 0x0 */ struct _GpGridParams* field_0;
    /* 0x4 */ struct _GpObj4A*      field_4;
    /* 0x8 */ struct _GpObj4A*      field_8;
    /* 0xC */ struct _GpObj3A*      field_C;
} GpCb7CRec;
STATIC_ASSERT_SIZEOF(GpCb7CRec, 0x10);

/// Per-stage wrapper. `field_0` is an array of `GpCb7CRec*`, indexed
/// 1-based by `GameSession.field_6` / `GameSessionFrom4.field_2`.
typedef struct _GpCb7CTbl {
    /* 0x0 */ GpCb7CRec** field_0;
} GpCb7CTbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb7CTbl* D_8010CB7C[];

/// 0x38-byte record in tables pointed to by `D_8010CB90`. Indexed
/// 1-based by `GameSessionFrom4.field_3` / `field_2`, then
/// `(D_80114CD9 >> 4)`. `func_800ADF3C` copies one record onto the
/// stack and writes `field_36` into `GpSaveLoc.field_6`. Leading `s32`
/// keeps the type 4-aligned so the 56-byte assign is `lw`/`sw`.
typedef struct _GpCb90Rec {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x32];
    /* 0x36 */ u16  field_36;
} GpCb90Rec;
STATIC_ASSERT_SIZEOF(GpCb90Rec, 0x38);

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`. Each
/// entry is an array of `GpCb90Rec*`, indexed 1-based by
/// `GameSession.field_6` / `GameSessionFrom4.field_2`.
extern GpCb90Rec** D_8010CB90[];

/// 0x1C-byte primitive slot in the `D_8010CAE8` lists. `func_800AD410`
/// OT-links each slot and advances `D_80114CC8` by one. `func_800AC960`
/// walks the current view's records and sets or clears bit 0 of `field_F`
/// on `field_2` consecutive slots (nonzero arg sets, zero arg clears).
typedef struct _GpPrim1C {
    /* 0x00 */ u32  tag;
    /* 0x04 */ byte pad_4[0xB];
    /* 0x0F */ u8   field_F;
    /* 0x10 */ byte pad_10[0xC];
} GpPrim1C;
STATIC_ASSERT_SIZEOF(GpPrim1C, 0x1C);

/// Merged `DR_TPAGE` + `SPRT` (0x1C) written into `D_80071190` by
/// `func_800AC790`. `MargePrim` concatenates the tpage packet onto the
/// sprite so they share one OT entry.
typedef struct _GpTpageSprt {
    /* 0x00 */ DR_TPAGE tpage;
    /* 0x08 */ SPRT     sprt;
} GpTpageSprt;
STATIC_ASSERT_SIZEOF(GpTpageSprt, 0x1C);

/// Dual-buffer primitive list heads, indexed by `Display_State.field_1f`.
extern GpPrim1C* D_8010CAE8[];

/// Cursor into the current `D_8010CAE8` list. Set by `func_800AC688` /
/// `func_800AC960`, advanced by `func_800AD410`.
extern GpPrim1C* D_80114CC8;

/// 0x10-byte 0xFF-terminated CdCmd 0x21 source list at inner
/// `GpAreaRec.field_0`. Walked by `func_800A9E44`: `field_0` matches
/// `GpCdRec0C.field_0` (0 skips the record); `field_C` is param1[0]
/// (0 skips); `field_D` / `field_E` are param2[2] / param2[3].
typedef struct _GpCdRec10 {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ byte pad_1[0xB];
    /* 0x0C */ u8   field_C;
    /* 0x0D */ u8   field_D;
    /* 0x0E */ u8   field_E;
    /* 0x0F */ byte pad_F;
} GpCdRec10;
STATIC_ASSERT_SIZEOF(GpCdRec10, 0x10);

/// 0xC-byte 0xFF-terminated list at inner `GpAreaRec.field_4`, also
/// stored in `D_80114C68`. `func_800A9E44` matches `field_0` against
/// `GpCdRec10.field_0`. `field_2` is the packed CdCmd 0x21 location
/// (`% 100` / `/ 100` when `>= 100`); `field_4` indexes `D_8010CAD0`.
typedef struct _GpCdRec0C {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ byte pad_5[7];
} GpCdRec0C;
STATIC_ASSERT_SIZEOF(GpCdRec0C, 0xC);

/// Inner `func_800B5CE8` record as used by `func_800A9E44`: `field_0` is
/// the 0x10-byte list, `field_4` is the 0xC-byte list.
typedef struct _GpCdAreaRec {
    /* 0x0 */ GpCdRec10* field_0;
    /* 0x4 */ GpCdRec0C* field_4;
} GpCdAreaRec;
STATIC_ASSERT_SIZEOF(GpCdAreaRec, 8);

/// 2-byte table at `D_8010CAD0`. `func_800A9E44` reads `field_0` at
/// `GpCdRec0C.field_4` (stride 2) as the CdCmd 0x21 param1[2] base.
typedef struct _GpTbl2 {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GpTbl2;
STATIC_ASSERT_SIZEOF(GpTbl2, 2);

extern GpTbl2 D_8010CAD0[];

/// Phase for `func_800A9E44` (0 init, 1 walk/enqueue, 2 wait idle).
/// `func_800AB5F4` clears it when phase 1 (`func_800AA120`) finishes
/// so phase 2 can start.
extern s16 D_80114C60;

/// Inner area rec from `func_800B5CE8`, shared by `func_800A9E44` /
/// `func_800AA120`.
extern GpCdAreaRec* D_80114C64;

/// Cursor into the inner rec's 0xC-byte list (`GpCdAreaRec.field_4`).
extern GpCdRec0C* D_80114C68;

/// Cursor into the inner rec's 0x10-byte list (`GpCdAreaRec.field_0`).
extern GpCdRec10* D_80114C6C;

/// Phase for `func_800AA120`. `func_800AB5F4` clears it when entering
/// its own phase 1.
extern u16 D_80114C70;

/// Phase for `func_800AB5F4` (0 init, 1 `func_800AA120`, 2 `func_800A9E44`).
/// `func_800AB3A8` clears it when advancing to this task state.
extern u16 D_80114C74;

/// Dual-buffer fullscreen TILE overlay, indexed by `Display_State.field_114`.
/// Paired with `D_80114CA0`. Used by `func_800AB828` and the neighboring
/// D4 fade-overlay task states.
extern TILE D_80114C80[2];

/// Dual-buffer `DR_TPAGE` (code `0xE1000240`) paired with `D_80114C80`.
extern DR_TPAGE D_80114CA0[2];

/// 5-byte table at `D_800938CC`. `func_800A9B3C` copies it to the stack and
/// indexes it 1-based by `Wip_SysConfig.field_26`; the byte is CdCmd 0x21
/// param2[0].
typedef struct _GpTbl5 {
    /* 0x0 */ u8 field_0[5];
} GpTbl5;
STATIC_ASSERT_SIZEOF(GpTbl5, 5);

extern GpTbl5 D_800938CC;

/// 8-byte pair of byte-table pointers at `D_801149FC`. `func_800AE1F0`
/// indexes by `(D_80114CD8 & 0x70) >> 4`. `D_80114CD2 & 0x100` selects
/// `field_4` over `field_0`. The byte at `(D_80114CD8 & 0xF) -
/// GameActor.field_82` is stored into `GameActor.field_930`.
typedef struct _GpDirPair {
    /* 0x0 */ u8* field_0;
    /* 0x4 */ u8* field_4;
} GpDirPair;
STATIC_ASSERT_SIZEOF(GpDirPair, 8);

extern GpDirPair D_801149FC[];

/// 8-byte record in `D_80114198` / `D_801141F0` / `D_80114248`. Indexed by
/// `GameFlag_GetNibble(0x4B / 0x4C / 0x4D)`. `field_0` is a per-room byte
/// list, 1-based by `Mc_SaveData.field_6`; `field_4` is the stage id
/// (`Mc_SaveData.field_7`). `func_800ABCC8` tests the room byte (second
/// table with `& 0xF`) to choose the `Snd_SetModeFlag` argument.
/// `func_800ABA4C` uses the same tables to pick `Mc_SaveData.field_13`.
typedef struct _GpNpcRoomRec {
    /* 0x0 */ u8*  field_0;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ byte pad_5[3];
} GpNpcRoomRec;
STATIC_ASSERT_SIZEOF(GpNpcRoomRec, 8);

extern GpNpcRoomRec D_80114198[];
extern GpNpcRoomRec D_801141F0[];
extern GpNpcRoomRec D_80114248[];

/// 4-byte record in 0xFF-terminated lists walked by `func_800AE62C`.
/// `field_0` indexes `D_8010CBCC` (same role as `GpAreaKey.field_3`);
/// `field_1` indexes that table (same role as `GpAreaKey.field_2`);
/// `field_2` is the id written by `func_800B5B30`. High nibble of `field_3`
/// is a `Mc_SaveData.field_F` filter (0 = always, 0x10 if 0 or 2, 0x20 if
/// 1 or 3); low nibble nonzero sets `GpAreaObj.field_1` bit 2, else clears.
typedef struct _GpAreaApplyRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} GpAreaApplyRec;
STATIC_ASSERT_SIZEOF(GpAreaApplyRec, 4);

/// Maps `Wip_SysConfig.field_21` / `field_22` (and the 0x1B attach id) to a
/// CdCmd 0x21 payload. No-op when `field_21` is 0 or the mapped byte is 0.
void func_800A9310(void);
void func_800A954C(Task* task);
void func_800A9DF0(Task* task);
/// Walk the inner area rec's 0x10-byte CdCmd 0x21 list (`D_80114C6C`),
/// matching each id against the 0xC-byte list (`D_80114C68`). Returns 1
/// when the list is exhausted or missing, else 0 (still in flight).
s32  func_800A9E44(void);
/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `Display_State.field_114`. Draws while `CdCmd_Queue.field_224` is 0.
/// Sets `Pad_RemapState->field_3`. When the CD queue is idle and
/// `func_80042500` returns 0: sets `CdCmd_Queue.field_22E`, starts the
/// boot load if a command is queued, clears `Stream_Slots`, refreshes
/// `GameSession.field_11C` / `field_11E` from save/config (enqueueing
/// CdCmd 0x21 via `func_800A9B3C` / `func_800A9BE4` if stale), then
/// `func_800A78EC` and advances `task->state`.
void func_800AABB0(Task* task);
/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `Display_State.field_114`. Draws while `CdCmd_Queue.field_224` is 0.
/// When the CD queue is idle, enqueues a stage reload if
/// `GameSession.field_7` differs from the cached `field_78`, then
/// advances `task->state`.
void func_800AADDC(Task* task);
/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `Display_State.field_114`. Draws while `CdCmd_Queue.field_224` is 0.
/// When the CD queue is idle, enqueues CdCmd 0x21 with the current
/// session location (`field_5` / `field_6` / `field_7`), then
/// `func_800ABA4C`. If that returns a companion type, stores it in
/// `GameSession.field_124` and calls `func_800A9CBC` with
/// `Mc_SaveData.field_13` / `field_5C7`. Then advances `task->state`.
void func_800AB1C8(Task* task);
/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `Display_State.field_114`. Draws while `CdCmd_Queue.field_224` is 0.
/// When the CD queue is idle, if the session location high word is
/// `0x3010000` and `field_5 >= 4`, re-inits stage sound and enqueues
/// CdCmd 0x21 (`param1[0] = 0x16`). If `field_4C` is 1, applies
/// `Mc_SaveData.field_9` via `func_800B5B30` and clears the flag. Then
/// applies the save location (`func_800ABF1C` / `func_800B601C`), copies
/// `Mc_SaveData.field_9` into `GameSession.field_9`, builds the stream
/// VLC, clears `D_80114C74`, and advances `task->state`.
void func_800AB3A8(Task* task);
/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `Display_State.field_114`. Draws while `CdCmd_Queue.field_224` is 0.
/// Then walks `D_80114C74`: phase 0 resets `D_80114C70` and falls into
/// phase 1 (`func_800AA120`); when that finishes, phase 2 runs
/// `func_800A9E44`. On success, resets TMD lists / the current OT,
/// advances `task->state`, and if `Mc_SaveData.field_5C3` is set enables
/// interlace on both `DISPENV` slots.
void func_800AB5F4(Task* task);
/// Dual-buffer TILE / DR_TPAGE overlay (gray 0x64), indexed by
/// `Display_State.field_114`. Draws while `CdCmd_Queue.field_224` is 0,
/// then after 7 frames clears `CdCmd_Queue.field_22E` and advances state.
void func_800AB828(Task* task);
void func_800AB980(struct _GameSessionFrom4* arg0);
/// Pick companion type into `Mc_SaveData.field_13` from the NPC room tables.
/// Returns 0 if already current or none; else 1/2/3 for the caller to store
/// in `GameSession.field_124`.
s32  func_800ABA4C(void);
void func_800ABCC8(void);
void func_800ABE68(struct _GpActorArg* arg0, u16* arg1);
void func_800ABEF8(s32 arg0);
void func_800ABF1C(struct _GpAreaKey* arg0);
s32   func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Same room-object link as `func_800ACD2C`, then spawn type 0x1B as a
/// child, clear `GameSession.field_76`, and increment `task->state`.
void  func_800AC4D8(Task* task);
void  func_800AC688(void);
/// Build merged `DR_TPAGE`+`SPRT` packets into `D_80071190` from
/// `arg0[arg1->field_0]` for `arg1->field_2` entries, and OT-link each.
void  func_800AC790(GpCb68Elem* arg0, GpCb68Obj* arg1);
void  func_800AC960(s32 arg0);
void  func_800ACD2C(Task* task);
/// 1-based index of `(u8)arg0` in the current room's `D_8010CB54` byte
/// list. Length is the `D_8010CB40` cell as an s16. Returns 0 if absent.
s8    func_800ACEBC(s32 arg0);
s32   func_800ACF8C(void);
s32   func_800AD284(void);
void* func_800AD2E8(void);
void  func_800AD410(GpCb68Elem* arg0, GpCb68Obj* arg1);
void  func_800AE62C(GpAreaApplyRec* arg0);

/// 0xFF-terminated `GpAreaFlagRec` lists applied by `func_800AE7AC` to
/// `D_8010CBD0` / `D_8010CBD4` / `D_8010CBDC` / `D_8010CBE0` (stages 1, 2,
/// 4, 5 of `D_8010CBCC`).
extern struct _GpAreaFlagRec D_80114098[];
extern struct _GpAreaFlagRec D_801140C0[];
extern struct _GpAreaFlagRec D_801140F8[];
extern struct _GpAreaFlagRec D_80114154[];

/// `D_8010CBCC[1]`, `[2]`, `[4]`, `[5]`. Splat labels the later slots as
/// their own symbols; `func_800AE7AC` loads each as a `GpAreaRec*`.
extern struct _GpAreaRec* D_8010CBD0;
extern struct _GpAreaRec* D_8010CBD4;
extern struct _GpAreaRec* D_8010CBDC;
extern struct _GpAreaRec* D_8010CBE0;

/// New-game init: for each of the four stage flag lists, OR bit 2 into
/// `GpAreaObj.field_1` on every record whose apply flag is set.
void func_800AE7AC(void);

/// Per-stage signed counts, indexed by `GameSession.field_7 - 1`.
/// `func_800AE9B0` loops area ids `1..count` when the stage is 1–5.
extern s8 D_8010CAF0[];

/// For each area id `1..D_8010CAF0[stage-1]`, set or clear the matching
/// bit in `D_80114D00`. The bit is set only when that area object's
/// `field_1` bit 2 is set and `func_800B59A8` returns 0.
void func_800AE9B0(void);

#endif // GAMEPLAY_D4_H
