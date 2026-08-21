#ifndef GAMEPLAY_GAMEPLAY_H
#define GAMEPLAY_GAMEPLAY_H

#include "common.h"

#include "main/session.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/pad.h"
#include "main/tmd.h"

struct _GpEnemy;

/// 8-byte follow state passed to `func_800A6F38` / `func_800A784C`.
/// `field_0` is the last `GpEnemy` drawn; `field_4` / `field_6` are the
/// previous screen X/Y that `func_800A6F38` lerps toward 0x6A, -0x35
/// (or -0x64 when `func_800B9D80(0x100000)` is 0).
typedef struct _GpHudTrack {
    /* 0x0 */ struct _GpEnemy* field_0;
    /* 0x4 */ s16              field_4;
    /* 0x6 */ s16              field_6;
} GpHudTrack;
STATIC_ASSERT_SIZEOF(GpHudTrack, 8);

/// 0x1C-byte scratch from `G_SCRATCH_HEAD` used by `func_800A6F38`.
/// `field_14` / `field_16` are the current screen X/Y; `field_18` /
/// `field_1A` hold the signed deltas before and after `>> 3`.
typedef struct _GpHudScratch {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s16  field_16;
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
} GpHudScratch;
STATIC_ASSERT_SIZEOF(GpHudScratch, 0x1C);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `func_800A8864`.
/// `mat` is the transpose of the parent rotation; `vec` is
/// `child.t - parent.t` before `ApplyMatrixLV` rotates it into dest translation.
typedef struct _GpRelMatScratch {
    /* 0x00 */ MATRIX mat;
    /* 0x20 */ VECTOR vec;
} GpRelMatScratch;
STATIC_ASSERT_SIZEOF(GpRelMatScratch, 0x30);

/// 0x48-byte scratch from `G_SCRATCH_HEAD` used by `func_800A70A4`.
/// `mat` is the transpose of the player `workm`; `vec` at +0x40 is the
/// packed SVECTOR that `gte_stsv` / translation add-sub share. The
/// `stsv` dest pointer is `original_head - 8`, the same address as `vec`.
typedef struct _GpXformScratch {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ byte    pad_20[0x20];
    /* 0x40 */ SVECTOR vec;
} GpXformScratch;
STATIC_ASSERT_SIZEOF(GpXformScratch, 0x48);

/// Overlay of a `D_80115268` `GpLinkNode` (embedded at `GpEnemy.node`)
/// used by `func_800A70A4`. `field_4` is the word at node+4 (same
/// `(flags & 5) == 1` skip as `func_800A4904`). `coord` is
/// `GpEnemy.field_18`. `src` / `dst` overlay `GpEnemy.field_1C` /
/// `field_2C`: local XYZ in, player-relative XYZ out.
typedef struct _GpLinkXform {
    /* 0x00 */ struct _GpLinkXform* next;
    /* 0x04 */ s32                  field_4;
    /* 0x08 */ GsCOORDINATE2*       coord;
    /* 0x0C */ VECTOR3              src;
    /* 0x18 */ byte                 pad_18[4];
    /* 0x1C */ VECTOR3              dst;
} GpLinkXform;
STATIC_ASSERT_SIZEOF(GpLinkXform, 0x28);

/// Global at `D_80114C08`. `field_0` is a u16 loaded by many helpers.
/// `field_2` is a signed byte (`lb` as splat `D_80114C0A`); `func_800A1F64`
/// writes the low byte of `func_800A1558(3)`, replacing it with 1 when
/// that value is <= 0. `field_3` is a signed state byte (`lb`);
/// `func_80109290` compares it to -2 and `func_80109374` requires 0.
/// `field_5` is a signed category index (`lb` as splat `D_80114C0D`);
/// `func_800A1558` uses it to pick a `D_8011398C` row when it is `< 0xC`.
/// `field_B` is the same kind of signed index (`lb`); `func_800A1634`
/// uses `field_5` when its first arg is 1 and `field_B` otherwise.
/// `field_6` is a flags byte (bit 0 gates `func_800A7DB8` writing
/// `field_E`; bit 1 is cleared by `func_800A7574` and forces
/// `func_800A7E5C` to 0 when that function's arg is 0). `field_9` is
/// cleared by `func_800A1F64`. `field_A` is a signed byte (`lb`, splat
/// `D_80114C12`); `func_800A7DE0` sets `field_3 = 2` when it is >= 2,
/// then clears it. `func_80109FC4` loads it unsigned (`lbu`) and skips
/// the `field_25` bit `0x80` timer when the value is 2 or 3.
/// `func_800A7574` also zeros `field_A`, `field_C`..`field_F`,
/// `field_10`/`field_12`/`field_14`, and `field_16`/`field_17`. Those
/// two bytes are also the item 4 / item 8 gates in `func_800D6170`
/// (`lb`). `func_800A45F0` packs a nibble plus `field_0 % 10` into
/// `field_C` / `field_D` / `field_F` and stores a table duration in
/// `field_10` / `field_12` / `field_14`.
typedef struct _GpStateC08 {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ s8   field_2;
    /* 0x03 */ s8   field_3;
    /* 0x04 */ byte pad_4;
    /* 0x05 */ s8   field_5;
    /* 0x06 */ u8   field_6;
    /* 0x07 */ u8   field_7;
    /* 0x08 */ u8   field_8;
    /* 0x09 */ u8   field_9;
    /* 0x0A */ s8   field_A;
    /* 0x0B */ s8   field_B;
    /* 0x0C */ s8   field_C;
    /* 0x0D */ u8   field_D;
    /* 0x0E */ u8   field_E;
    /* 0x0F */ u8   field_F;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s8   field_16;
    /* 0x17 */ u8   field_17;
} GpStateC08;
STATIC_ASSERT_SIZEOF(GpStateC08, 0x18);

extern GpStateC08 D_80114C08;

/// Pair of s32 working copies at `D_80114BE8`. `func_800A7574` (and
/// `func_800B996C` / `func_800B9B40`) sign-extend `Wip_SysConfig.field_18` /
/// `field_1c` into `field_0` / `field_4`. Splat also emits `D_80114BEC` at +4.
typedef struct _GpStateBE8 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} GpStateBE8;
STATIC_ASSERT_SIZEOF(GpStateBE8, 0x8);

extern GpStateBE8 D_80114BE8;
extern s32        D_80114BEC; // splat overlay of D_80114BE8.field_4

/// +0xC overlay of the 0x30-byte record `func_8009FEDC` allocates with
/// `Mem_Calloc(0x30, 0)` and stores at `Task::idMap`. `func_800A7574` is
/// called with that pointer + 0xC; it writes `field_16 = -1` and clears
/// `field_18`.
typedef struct _GpIdMapC {
    /* 0x00 */ byte pad_0[0x16];
    /* 0x16 */ s8   field_16;
    /* 0x17 */ byte pad_17;
    /* 0x18 */ s16  field_18;
} GpIdMapC;
STATIC_ASSERT_SIZEOF(GpIdMapC, 0x1A);

/// 8-byte dispatch record selected by `func_800A1634` as
/// `(GpRec8*)D_80113D38 + idx * 3 + ret`. `field_0` is the switch key
/// (0..4). `field_2` / `field_4` are scaled by 100 into the follow-up
/// calls. `field_6` is passed as `lh` and also read as `lbu` + 2 into
/// `GpIdMapC.field_16`.
typedef struct _GpRec8 {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} GpRec8;
STATIC_ASSERT_SIZEOF(GpRec8, 8);

/// 8-byte item-effect row used by `func_800A45F0`. Indexed by
/// `D_80114C08.field_0 % 10`. `field_6` is loaded `lhu` into
/// `GpStateC08.field_10` / `field_12` / `field_14`.
typedef struct _GpItemRec8 {
    /* 0x0 */ u16 pad_0[3];
    /* 0x6 */ u16 field_6;
} GpItemRec8;
STATIC_ASSERT_SIZEOF(GpItemRec8, 8);

extern GpItemRec8 D_80113DC8[];
extern GpItemRec8 D_80113E10[];
extern GpItemRec8 D_80113E28[];

/// 0x30-byte play-clock work `func_8009FEDC` stores at `Task::idMap`.
/// `field_0` / `field_4` are `Mc_SaveData.field_C` split into minutes and
/// seconds. `field_8` snapshots `Display_State.field_4`. `extra` is the
/// +0xC overlay passed to `func_800A7574`.
typedef struct _GpIdMap30 {
    /* 0x00 */ s32     field_0;
    /* 0x04 */ s32     field_4;
    /* 0x08 */ s32     field_8;
    /* 0x0C */ GpIdMapC extra;
    /* 0x26 */ byte    pad_26[0xA];
} GpIdMap30;
STATIC_ASSERT_SIZEOF(GpIdMap30, 0x30);

/// 4-byte spawn arg at `D_80114BD8`. `func_800A76A4` zeros `field_0` / `field_1`,
/// writes `GameSession.field_12E` as a signed halfword to `field_2`, then passes
/// the record to `Task_Spawn(1, 0x31, 0, ...)`.
typedef struct _GpStateBD8 {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} GpStateBD8;
STATIC_ASSERT_SIZEOF(GpStateBD8, 0x4);

extern GpStateBD8 D_80114BD8;

/// 0x24-byte camera/view record in tables pointed to by `D_8010CB2C`.
/// Indexed 1-based by `func_800AD284()`. `mtx` rotation is copied to
/// `D_80070E44` and translation to `D_80070F28` by `func_800A8724` /
/// `func_800A8A48` / `func_800A8C74`; `field_20` is `lhu` into
/// `Display_State.field_110` and `lw` into GTE H (`gte_SetGeomScreen`).
typedef struct _GpCb2CRec {
    /* 0x00 */ MATRIX mtx;
    /* 0x20 */ u32    field_20;
} GpCb2CRec;
STATIC_ASSERT_SIZEOF(GpCb2CRec, 0x24);

/// Per-stage wrapper. `field_0` is an array of `GpCb2CRec*`, indexed by
/// `GameSession.field_6 - 1` / `GameSessionFrom4.field_2 - 1`.
typedef struct _GpCb2CTbl {
    /* 0x0 */ GpCb2CRec** field_0;
} GpCb2CTbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb2CTbl* D_8010CB2C[];

/// `GsCOORDINATE2` overlay embedded in `GpDisp2d` at +0x10. Same 0x50 layout
/// as libgs, but offset 0x44 (`param` / first half of `super`) is an `SVECTOR`
/// of zeros written by `func_80099098`. `sub` is still the parent coordinate
/// (`&D_80070F10`).
typedef struct _GpDisp2dCoord {
    /* 0x00 */ u32            flg;
    /* 0x04 */ MATRIX         mtx;
    /* 0x24 */ MATRIX         workm;
    /* 0x44 */ SVECTOR        rot;
    /* 0x4C */ GsCOORDINATE2* sub;
} GpDisp2dCoord;
STATIC_ASSERT_SIZEOF(GpDisp2dCoord, 0x50);

/// 0x60-byte spawnType-2 extra (`Mem_Calloc` in `func_80099098`, fail string
/// `"new_disp_2d ----> NULL"`). Linked onto `Tmd_ListAlt`. `field_8` points at
/// the embedded coord; `field_C` is stored as a word 1.
typedef struct _GpDisp2d {
    /* 0x00 */ TmdObject*     next;
    /* 0x04 */ TmdListHead*   prev;
    /* 0x08 */ GpDisp2dCoord* field_8;
    /* 0x0C */ s32            field_C;
    /* 0x10 */ GpDisp2dCoord  coord;
} GpDisp2d;
STATIC_ASSERT_SIZEOF(GpDisp2d, 0x60);

/// 4-byte recorded pad pair in the demo/replay stream at `D_80114C38`.
/// `func_8009FD74` copies `buttons` into `PadScratch` and counts `duration`
/// frames before advancing. `0xFFFF` buttons is the end marker.
typedef struct _GpPadReplay {
    /* 0x0 */ u16 buttons;
    /* 0x2 */ u16 duration;
} GpPadReplay;
STATIC_ASSERT_SIZEOF(GpPadReplay, 0x4);

/// Current replay buttons / remaining frame count / stream cursor.
extern u16          D_80114C02;
extern u16          D_80114C04;
/// Word cleared by `func_800A1F64`; `func_800A2F60` increments and tests it.
extern s32          D_80114C34;
extern GpPadReplay* D_80114C38;

void func_80098F58(GsCOORDINATE2* arg0);
void func_80098F98(GsCOORDINATE2* arg0, s32 arg1);
Task* func_8009988C(GsCOORDINATE2* arg0);
void func_8009FD74(s32 arg0, PadScratch* arg1);
void func_8009FEDC(Task* task);
void func_800A0094(Task* task);
void func_800A110C(Task* arg0);
u16  func_800A1558(s32 arg0);
void func_800A1634(s32 arg0, GpIdMapC* arg1);
void func_800A45F0(s32 arg0);
s32  func_800A1CD0(s32 arg0);
void func_800A1F64(s32 arg0);
void func_800A3AF0(GpIdMapC* arg0);
void func_800A7320(s16* arg0);
u8*  func_800A746C(void);
s32  func_800A74C4(void);
void func_800A7574(GpIdMapC* arg0);
s32  func_800A7B20(s32 arg0);
s32  func_800A7BBC(s32 arg0, s32 arg1);
void func_800A6F38(struct _GpEnemy* arg0, GpHudTrack* arg1);
void func_800A70A4(void);
void func_800A784C(GpHudTrack* arg0);
void func_800A78EC(void);
void func_800A7A64(void);
void func_800A7DB8(s32 arg0);
void func_800A7DE0(void);
s32  func_800A7E5C(s32 arg0);
void func_800A8654(Task* task);
void func_800A8724(void);
void func_800A8864(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2);
void func_800A8A48(GpCb2CRec* arg0);
void func_800A8B14(void);
void func_800A8B6C(void);
GpCb2CRec* func_800A8C08(GameSessionFrom4* arg0);
void func_800A8C74(Task* task);
void func_800A8D5C(void);
void func_800A8DC0(s32 arg0);
void func_800A8E8C(Task* task);
void func_800A9010(Task* task);
void func_800A91CC(Task* task);

/// Overlay of `Task::spawnArg2` for `func_800A0718` / `func_800A110C`.
/// `func_800A0718` sets `field_4` to 1 on the first run (state 0).
/// `func_800A110C` zeros both words when `spawnArg1` is 0.
typedef struct _GpEndWork {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
} GpEndWork;

/// Two halfwords at `D_8007A39C`. `func_800A0718` zeros both before spawning
/// the bank-load task. `field_0` is the u16 sound param used by
/// `Task_AllocIdMap`.
typedef struct _GpSndParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpSndParam;
STATIC_ASSERT_SIZEOF(GpSndParam, 4);

#endif // GAMEPLAY_GAMEPLAY_H
