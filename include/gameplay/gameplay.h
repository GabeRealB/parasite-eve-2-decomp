#ifndef GAMEPLAY_GAMEPLAY_H
#define GAMEPLAY_GAMEPLAY_H

#include "common.h"

#include "main/session.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/pad.h"
#include "main/text.h"
#include "main/tmd.h"

struct _GpEnemy;

/// 8-byte follow state passed to `Gp_HudTrackEnemy` / `Gp_HudTrackSlot0`.
/// `field_0` is the last `GpEnemy` drawn; `field_4` / `field_6` are the
/// previous screen X/Y that `Gp_HudTrackEnemy` lerps toward 0x6A, -0x35
/// (or -0x64 when `func_800B9D80(0x100000)` is 0).
typedef struct _GpHudTrack {
    /* 0x0 */ struct _GpEnemy* field_0;
    /* 0x4 */ s16              field_4;
    /* 0x6 */ s16              field_6;
} GpHudTrack;
STATIC_ASSERT_SIZEOF(GpHudTrack, 8);

/// 0x1C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_HudTrackEnemy`.
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

/// 0x30-byte stack scratch shared by the enemy HP-bar HUD (`Gp_DrawHudNumbers`).
/// The block is first initialised as a `UiObject` (`baseX` / `baseY` /
/// `drawOrder` / `mode`), then reused: `text.buf` is the `Text_ItoaUnsigned`
/// digit buffer with `text.req` (at +0x10) the matching draw request, while the
/// "????" case draws through `bar.req` (at +0) and the trailing
/// `Ui_DrawTextInRect` rectangle is `bar.rect` (also at +0x10).
typedef union GpHudBarScratch {
    UiObject obj;
    struct {
        /* 0x00 */ u8          buf[0x10];
        /* 0x10 */ TextDrawReq req;
    } text;
    struct {
        /* 0x00 */ TextDrawReq req;
        /* 0x10 */ RECT        rect;
    } bar;
} GpHudBarScratch;
STATIC_ASSERT_SIZEOF(GpHudBarScratch, 0x30);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `Gp_WorldToLocal`.
/// `mat` is the transpose of the parent rotation; `vec` is
/// `child.t - parent.t` before `ApplyMatrixLV` rotates it into dest translation.
typedef struct _GpRelMatScratch {
    /* 0x00 */ MATRIX mat;
    /* 0x20 */ VECTOR vec;
} GpRelMatScratch;
STATIC_ASSERT_SIZEOF(GpRelMatScratch, 0x30);

/// 0x48-byte scratch from `G_SCRATCH_HEAD` used by `Gp_UpdateLinkXforms`.
/// `mat` is the transpose of the player `workm`; `vec` at +0x40 is the
/// packed SVECTOR that `gte_stsv` / translation add-sub share. The
/// `stsv` dest pointer is `original_head - 8`, the same address as `vec`.
typedef struct _GpXformScratch {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ byte    pad_20[0x20];
    /* 0x40 */ SVECTOR vec;
} GpXformScratch;
STATIC_ASSERT_SIZEOF(GpXformScratch, 0x48);

/// 0x60-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawAimCircle` to draw the
/// wireframe targeting sphere. `vec` is the point being rotated / projected,
/// `mat` the rotation loaded into the GTE, `rx` / `ry` the two radii taken from
/// the caller and `radius` the per-ring radius derived from them. `dp` / `flag`
/// / `otz` / `sxy` receive `gte_stdp` / `gte_stflg` / `gte_stszotz` /
/// `gte_stsxy` of each RTPS, and `sxyPrev` keeps the previous point so the two
/// form a `LINE_F2`. `trans` is the GTE translation vector (`gte_SetTransVector`).
typedef struct _GpCircleScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ MATRIX  mat;
    /* 0x28 */ s32     rx;
    /* 0x2C */ s32     ry;
    /* 0x30 */ s32     radius;
    /* 0x34 */ s32     dp;
    /* 0x38 */ s32     flag;
    /* 0x3C */ s32     otz;
    /* 0x40 */ DVECTOR sxyPrev;
    /* 0x44 */ DVECTOR sxy;
    /* 0x48 */ byte    pad_48[8];
    /* 0x50 */ VECTOR  trans;
} GpCircleScratch;
STATIC_ASSERT_SIZEOF(GpCircleScratch, 0x60);

/// Overlay of a `Gp_LinkList` `GpLinkNode` (embedded at `GpEnemy.node`)
/// used by `Gp_UpdateLinkXforms`. `field_4` is the word at node+4 (same
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

/// Global at `Gp_StateC08`. `field_0` is a u16 loaded by many helpers.
/// `field_2` is a signed byte (`lb` as splat `D_80114C0A`); `Gp_SetAttachState`
/// writes the low byte of `Gp_GetAttachParam(3)`, replacing it with 1 when
/// that value is <= 0. `field_3` is a signed state byte (`lb`);
/// `func_80109290` compares it to -2 and `func_80109374` requires 0.
/// `field_5` is a signed category index (`lb` as splat `D_80114C0D`);
/// `Gp_GetAttachParam` uses it to pick a `Gp_IdParamHi` row when it is `< 0xC`.
/// `field_B` is the same kind of signed index (`lb`); `Gp_ApplyAttachStats`
/// uses `field_5` when its first arg is 1 and `field_B` otherwise.
/// `field_6` is a flags byte (bit 0 gates `func_800A7DB8` writing
/// `field_E`; bit 1 is cleared by `Gp_ResetHudFx` and forces
/// `func_800A7E5C` to 0 when that function's arg is 0). `field_9` is
/// cleared by `Gp_SetAttachState`. `field_A` is a signed byte (`lb`, splat
/// `D_80114C12`); `func_800A7DE0` sets `field_3 = 2` when it is >= 2,
/// then clears it. `func_80109FC4` loads it unsigned (`lbu`) and skips
/// the `field_25` bit `0x80` timer when the value is 2 or 3.
/// `Gp_ResetHudFx` also zeros `field_A`, `field_C`..`field_F`,
/// `field_10`/`field_12`/`field_14`, and `field_16`/`field_17`. Those
/// two bytes are also the item 4 / item 8 gates in `Gp_ItemIsUnusable`
/// (`lb`). `Gp_UpdateAttachCombo` packs a nibble plus `field_0 % 10` into
/// `field_C` / `field_D` / `field_F` and stores a table duration in
/// `field_10` / `field_12` / `field_14`. `field_7` and `field_8` are signed
/// bytes (`lb`): `Gp_UseItemTask` treats `field_7` as a positive-only sound id
/// (`blez` clears it) and steps `field_8` 1 -> 2 -> 0 as the attach sound is
/// queued and the category is committed. `field_E` is a signed pending
/// category (`lb`), copied into `field_5` / `field_B` once the pad is idle,
/// and `field_10` / `field_12` / `field_14` are `s16` countdowns that clear
/// `field_C` / `field_D` / `field_F` when they reach 0.
typedef struct _GpStateC08 {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ s8   field_2;
    /* 0x03 */ s8   field_3;
    /* 0x04 */ byte pad_4;
    /* 0x05 */ s8   field_5;
    /* 0x06 */ u8   field_6;
    /* 0x07 */ s8   field_7;
    /* 0x08 */ s8   field_8;
    /* 0x09 */ u8   field_9;
    /* 0x0A */ s8   field_A;
    /* 0x0B */ s8   field_B;
    /* 0x0C */ s8   field_C;
    /* 0x0D */ u8   field_D;
    /* 0x0E */ s8   field_E;
    /* 0x0F */ u8   field_F;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s8   field_16;
    /* 0x17 */ u8   field_17;
} GpStateC08;
STATIC_ASSERT_SIZEOF(GpStateC08, 0x18);

extern GpStateC08 Gp_StateC08;

/// Pair of s32 working copies at `Gp_HpMpWork`. `Gp_ResetHudFx` (and
/// `Gp_UiBoostMp` / `Gp_UiBoostHp`) sign-extend `Wip_SysConfig.field_18` /
/// `field_1c` into `field_0` / `field_4`.
typedef struct _GpStateBE8 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} GpStateBE8;
STATIC_ASSERT_SIZEOF(GpStateBE8, 0x8);

extern GpStateBE8 Gp_HpMpWork;

/// +0xC overlay of the 0x30-byte record `Gp_InitPlayClock` allocates with
/// `Mem_Calloc(0x30, 0)` and stores at `Task::idMap`. `Gp_ResetHudFx` is
/// called with that pointer + 0xC; it writes `field_16 = -1` and clears
/// `field_18`. `Gp_UseItemTask` clears `field_10` (word) and `field_E` (`sb`)
/// on entry and reads `field_15` (`lb`) as a gate on the pad poll.
typedef struct _GpIdMapC {
    /* 0x00 */ s32        field_0;
    /* 0x04 */ s32        field_4;
    /* 0x08 */ s32        field_8;
    /* 0x0C */ byte       pad_C;
    /* 0x0D */ s8         field_D;
    /* 0x0E */ s8         field_E;
    /* 0x0F */ byte       pad_F;
    /* 0x10 */ s32        field_10;
    /* 0x14 */ u8         field_14;
    /* 0x15 */ s8         field_15;
    /* 0x16 */ s8         field_16;
    /* 0x17 */ byte       pad_17;
    /* 0x18 */ s16        field_18;
    /* 0x1A */ byte       pad_1A[2];
    /* 0x1C */ GpHudTrack field_1C;
} GpIdMapC;
STATIC_ASSERT_SIZEOF(GpIdMapC, 0x24);

/// 8-byte dispatch record selected by `Gp_ApplyAttachStats` as
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

/// 8-byte item-effect row used by `Gp_UpdateAttachCombo`. Indexed by
/// `Gp_StateC08.field_0 % 10`. `field_6` is loaded `lhu` into
/// `GpStateC08.field_10` / `field_12` / `field_14`.
typedef struct _GpItemRec8 {
    /* 0x0 */ u16 pad_0[3];
    /* 0x6 */ u16 field_6;
} GpItemRec8;
STATIC_ASSERT_SIZEOF(GpItemRec8, 8);

extern GpItemRec8 D_80113DC8[];
extern GpItemRec8 D_80113E10[];
extern GpItemRec8 D_80113E28[];

/// 0x30-byte play-clock work `Gp_InitPlayClock` stores at `Task::idMap`.
/// `field_0` / `field_4` are `Mc_SaveData.field_C` split into minutes and
/// seconds. `field_8` snapshots `Display_State.field_4`. `extra` is the
/// +0xC overlay passed to `Gp_ResetHudFx`.
typedef struct _GpIdMap30 {
    /* 0x00 */ s32      field_0;
    /* 0x04 */ s32      field_4;
    /* 0x08 */ s32      field_8;
    /* 0x0C */ GpIdMapC extra;
} GpIdMap30;
STATIC_ASSERT_SIZEOF(GpIdMap30, 0x30);

/// 4-byte spawn arg at `D_80114BD8`. `Gp_PlayClockState2` zeros `field_0` / `field_1`,
/// writes `GameSession.field_12E` as a signed halfword to `field_2`, then passes
/// the record to `Task_Spawn(1, 0x31, 0, ...)`.
typedef struct _GpStateBD8 {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} GpStateBD8;
STATIC_ASSERT_SIZEOF(GpStateBD8, 0x4);

extern GpStateBD8 D_80114BD8;

/// 0x24-byte camera/view record in tables pointed to by `Gp_ViewTables`.
/// Indexed 1-based by `Gp_GetViewIndex()`. `mtx` rotation is copied to
/// `Gfx_ViewRotMtx` and translation to `D_80070F28` by `Gp_LoadStageView` /
/// `Gp_ApplyView` / `Gp_ApplyViewTask`; `field_20` is `lhu` into
/// `Display_State.field_110` and `lw` into GTE H (`gte_SetGeomScreen`).
typedef struct _GpViewRec {
    /* 0x00 */ MATRIX mtx;
    /* 0x20 */ u32    field_20;
} GpViewRec;
STATIC_ASSERT_SIZEOF(GpViewRec, 0x24);

/// Per-stage wrapper. `field_0` is an array of `GpViewRec*`, indexed by
/// `GameSession.field_6 - 1` / `GameSessionFrom4.field_2 - 1`.
typedef struct _GpViewTbl {
    /* 0x0 */ GpViewRec** field_0;
} GpViewTbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpViewTbl* Gp_ViewTables[];

/// `GsCOORDINATE2` overlay embedded in `GpDisp2d` at +0x10. Same 0x50 layout
/// as libgs, but offset 0x44 (`param` / first half of `super`) is an `SVECTOR`
/// of zeros written by `Gp_AttachDisp2d`. `sub` is still the parent coordinate
/// (`&Gfx_ViewCoord`).
typedef struct _GpDisp2dCoord {
    /* 0x00 */ u32            flg;
    /* 0x04 */ MATRIX         mtx;
    /* 0x24 */ MATRIX         workm;
    /* 0x44 */ SVECTOR        rot;
    /* 0x4C */ GsCOORDINATE2* sub;
} GpDisp2dCoord;
STATIC_ASSERT_SIZEOF(GpDisp2dCoord, 0x50);

/// View of `GsCOORDINATE2` starting at `workm.t`. `sub` is at +0x14 (coord
/// +0x4C). Size is the coord stride so `tail++` walks the `field_8` array.
typedef struct {
    /* 0x00 */ long           t[3];
    /* 0x0C */ GsCOORD2PARAM* param;
    /* 0x10 */ GsCOORDINATE2* super;
    /* 0x14 */ GsCOORDINATE2* sub;
    /* 0x18 */ byte           pad[0x38];
} GpCoordFromT;
STATIC_ASSERT_SIZEOF(GpCoordFromT, 0x50);

/// 0x60-byte spawnType-2 extra (`Mem_Calloc` in `Gp_AttachDisp2d`, fail string
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

/// 4-byte recorded pad pair in the demo/replay stream at `Gp_ReplayCursor`.
/// `Gp_ApplyPadReplay` copies `buttons` into `PadScratch` and counts `duration`
/// frames before advancing. `0xFFFF` buttons is the end marker.
typedef struct _GpPadReplay {
    /* 0x0 */ u16 buttons;
    /* 0x2 */ u16 duration;
} GpPadReplay;
STATIC_ASSERT_SIZEOF(GpPadReplay, 0x4);

/// Current replay buttons / remaining frame count / stream cursor.
extern u16 Gp_ReplayButtons;
extern u16 Gp_ReplayFramesLeft;
/// Word cleared by `Gp_SetAttachState`; `Gp_UseItemTask` increments and tests it.
extern s32          D_80114C34;
extern GpPadReplay* Gp_ReplayCursor;

void       Gp_UpdateCoordTree(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void       Gp_UpdateCoord(GsCOORDINATE2* arg0);
void       Gp_UpdateCoordEx(GsCOORDINATE2* arg0, s32 arg1);
void*      Gp_AttachTmd(Task* task, TmdSource* src);
void*      Gp_AttachDisp2d(Task* task);
void*      Gp_AttachTmdFlags(Task* task, TmdSource* src, s32 flags);
void       Gp_UnlinkTmd(TmdListHead* arg0);
void       Gp_FreeTmd(TmdObject* arg0);
void       Gp_UnlinkDisp2d(TmdListHead* arg0);
void       Gp_FreeDisp2d(void* arg0);
void       Gp_StashTmdLists(void);
void       Gp_RestoreTmdLists(void);
Task*      Gp_FindTaskByCoord(GsCOORDINATE2* arg0);
void       Gp_ApplyPadReplay(s32 arg0, PadScratch* arg1);
void       Gp_InitPlayClock(Task* task);
void       Gp_TickPlayClock(Task* task);
void       Gp_AreaEnterTask(Task* arg0);
u16        Gp_GetAttachParam(s32 arg0);
void       Gp_ApplyAttachStats(s32 arg0, GpIdMapC* arg1);
void       Gp_UpdateAttachCombo(s32 arg0);
s32        Gp_CheckAttachThreshold(s32 arg0);
void       Gp_SetAttachState(s32 arg0);
void       Gp_HudTask(GpIdMapC* arg0);
void       Gp_StartAreaBgm(s16* arg0);
u8*        Gp_GetAttachLevels(void);
s32        Gp_IsDebugAttachRoom(void);
void       Gp_ResetHudFx(GpIdMapC* arg0);
s32        Gp_GetAttachLevel(s32 arg0);
s32        Gp_StepAttachSlot(s32 arg0, s32 arg1);
void       Gp_HudTrackEnemy(struct _GpEnemy* arg0, GpHudTrack* arg1);
void       Gp_UpdateLinkXforms(void);
void       Gp_HudTrackSlot0(GpHudTrack* arg0);
void       Gp_EnqueueAttach7Cd(void);
void       Gp_TriggerPeIfArmed(void);
void       func_800A7DB8(s32 arg0);
void       func_800A7DE0(void);
s32        func_800A7E5C(s32 arg0);
void       func_800A8654(Task* task);
void       Gp_LoadStageView(void);
void       Gp_WorldToLocal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2);
s32        Gp_TrySpawnViewTask(s32 arg0);
void       Gp_ApplyView(GpViewRec* arg0);
void       Gp_ResetView(void);
void       Gp_SpawnViewTasks(void);
GpViewRec* Gp_GetStageView(GameSessionFrom4* arg0);
void       Gp_ApplyViewTask(Task* task);
void       func_800A8D5C(void);
void       Gp_SpawnCurView(s32 arg0);
void       Gp_ViewGateTask(Task* task);
void       Gp_ViewBeginLoad(Task* task);
void       Gp_ViewLoadImage(Task* task);

/// Overlay of `Task::spawnArg2` for `Gp_EndingTask` / `Gp_AreaEnterTask`.
/// `Gp_EndingTask` sets `field_4` to 1 on the first run (state 0).
/// `Gp_AreaEnterTask` zeros both words when `spawnArg1` is 0.
typedef struct _GpEndWork {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
} GpEndWork;

/// Two halfwords at `D_8007A39C`. `Gp_EndingTask` zeros both before spawning
/// the bank-load task. `field_0` is the u16 sound param used by
/// `Task_AllocIdMap`.
typedef struct _GpSndParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpSndParam;
STATIC_ASSERT_SIZEOF(GpSndParam, 4);

#endif // GAMEPLAY_GAMEPLAY_H
