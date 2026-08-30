#ifndef GAMEPLAY_3FB8_H
#define GAMEPLAY_3FB8_H

#include "common.h"

#include "main/session.h"
#include "main/task.h"

#include <psyq/libgte.h>

struct _GsCOORDINATE2;

/// Work object whose `actor` pointer sits at 0x1C (same slot as `Task::idMap`).
typedef struct _GpActorWork {
    /* 0x00 */ byte          pad_0[0x18];
    /* 0x18 */ void*         field_18; // Task::exitCallback; cleared before self-kill
    /* 0x1C */ GameActor*    actor;
    /* 0x20 */ byte          pad_20[4];
    /* 0x24 */ void*         field_24; // Task::field_24; GpMsgEntry table
    /* 0x28 */ byte          pad_28[4];
    /* 0x2C */ GameActorExt* extra;    // Task::extra
    /* 0x30 */ s32           state;    // Task::state
} GpActorWork;

typedef void (*GpActorFunc)(GpActorWork* arg0);

/// 3-entry callback table copied onto the stack by `Gp_TickPlayerActor`.
typedef struct {
    GpActorFunc funcs[3];
} GpActorFuncTable3;

/// 8-entry callback table copied onto the stack by `Gp_TickPlayerNormal`.
typedef struct {
    GpActorFunc funcs[8];
} GpActorFuncTable8;

/// 4-entry callback table copied onto the stack by `Gp_TickPlayerMode1`.
typedef struct {
    GpActorFunc funcs[4];
} GpActorFuncTable4;

/// 12-entry callback table copied onto the stack by `Gp_TickPlayerMode2`.
typedef struct {
    GpActorFunc funcs[12];
} GpActorFuncTable12;

/// 33-entry callback table copied onto the stack by `func_8010615C`.
typedef struct {
    GpActorFunc funcs[33];
} GpActorFuncTable33;

/// 0x10-byte spawn argument for `Gp_SpawnAlly` / `Gp_SpawnPlayer`. `field_0`
/// is copied to `GameActor.field_52`; `field_4` / `field_8` / `field_C` are
/// copied to the extra coordinate translation.
typedef struct _GpActorArg {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ s32  field_4;
    /* 0x8 */ s32  field_8;
    /* 0xC */ s32  field_C;
} GpActorArg;
STATIC_ASSERT_SIZEOF(GpActorArg, 0x10);

/// Companion spawn record for `Gp_SpawnPlayer`. `field_0` is copied to
/// `GameActor.field_93C`. Nonzero `field_2` sets `field_954` to 2.
typedef struct _GpActorFlags {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ byte pad_3;
} GpActorFlags;
STATIC_ASSERT_SIZEOF(GpActorFlags, 0x4);

/// 0x18-byte record at `GpActorD4.field_88`. `Gp_BindActorD4` copies `arg1`
/// into `field_8` / `field_A` / `field_C`, mirrors `field_8` / `field_A`
/// into `field_0` / `field_2`, stores `arg2` at `field_4`, writes 0x80 to
/// `field_10` / `field_12`, and points `field_14` at `field_A0`.
typedef struct _GpActorD4Rec {
    /* 0x00 */ s16      field_0;
    /* 0x02 */ s16      field_2;
    /* 0x04 */ s16      field_4;
    /* 0x06 */ s16      pad_6;
    /* 0x08 */ s16      field_8;
    /* 0x0A */ s16      field_A;
    /* 0x0C */ s16      field_C;
    /* 0x0E */ s16      pad_E;
    /* 0x10 */ s16      field_10;
    /* 0x12 */ s16      field_12;
    /* 0x14 */ GpRec18* field_14;
} GpActorD4Rec;
STATIC_ASSERT_SIZEOF(GpActorD4Rec, 0x18);

/// 0xD4-byte block allocated by `Gp_SpawnAlly` (`Mem_Set` size 0xD4) and
/// stored at `GameActor.field_910`. `Gp_BindActorD4` copies a `GsCOORDINATE2`
/// into `field_18`, treats `field_68` as a `GpObj`, fills `field_88`, and
/// points `field_88.field_14` at `field_A0`. `func_8010BF7C` writes `field_C4`.
/// `Gp_SetupAllyWeapon` writes `field_CD` from `D_80167230[Mc_SaveData.field_5C7]`.
/// `field_D0` is an `lb`/`sb` flag (`func_actor_800200_80165644` / `_8016599C`).
typedef struct _GpActorD4 {
    /* 0x00 */ byte         pad_0[0x18];
    /* 0x18 */ byte         field_18[0x50]; // GsCOORDINATE2
    /* 0x68 */ byte         field_68[0x20]; // GpObj
    /* 0x88 */ GpActorD4Rec field_88;
    /* 0xA0 */ GpRec18      field_A0;
    /* 0xB8 */ byte         pad_B8[0xC];
    /* 0xC4 */ s16          field_C4;
    /* 0xC6 */ byte         pad_C6[7];
    /* 0xCD */ u8           field_CD;
    /* 0xCE */ byte         pad_CE[2];
    /* 0xD0 */ s8           field_D0;
    /* 0xD1 */ byte         pad_D1[3];
} GpActorD4;
STATIC_ASSERT_SIZEOF(GpActorD4, 0xD4);

/// Overlay of `GsCOORDINATE2` at `GameActorExt.field_8`. `flg` is the same
/// word cleared by `*field_8 = 0`. Offset 0x44 (`param` in libgs) is an s16
/// flag (`lh`/`sh`) in `func_8010B590` / `func_80104364`. `sub` is the parent
/// coordinate pointer (`GsCOORDINATE2.sub` at 0x4C).
typedef struct _GpCoordExt {
    /* 0x00 */ s32  flg;
    /* 0x04 */ byte pad_4[0x40];
    /* 0x44 */ s16  field_44;
    /* 0x46 */ s16  field_46;
    /* 0x48 */ s16  field_48;
    /* 0x4A */ byte pad_4A[2];
    /* 0x4C */ s32* sub;
} GpCoordExt;
STATIC_ASSERT_SIZEOF(GpCoordExt, 0x50);

/// Overlay of `GameActor` for the three s16s at 0x418 (`GsCOORDINATE2.param`
/// as vx/vy/vz). `Gp_AttachActorObj` zeros them after `Gfx_RotMatrixX` of
/// `field_3D4.workm`.
typedef struct _GpActorSvec {
    /* 0x000 */ byte pad[0x418];
    /* 0x418 */ s16  field_418;
    /* 0x41A */ s16  field_41A;
    /* 0x41C */ s16  field_41C;
} GpActorSvec;

/// 8-byte argument record for `func_800FDB18`. `field_0` is a coordinate
/// (fallback `Gfx_ViewCoord`); `field_4` / `field_6` are packed into the
/// `Gp_SpawnEff` argument. The third `func_800FDB18` argument is an
/// `SVECTOR*` (or NULL).
typedef struct _GpEffArg {
    /* 0x0 */ struct _GsCOORDINATE2* field_0;
    /* 0x4 */ s16                    field_4;
    /* 0x6 */ s16                    field_6;
} GpEffArg;
STATIC_ASSERT_SIZEOF(GpEffArg, 0x8);

/// 0x2C-byte work at `Task::spawnArg2` for `Gp_EffCtlTask6E` / `Gp_EffCtlTask3B` /
/// `Gp_EffPolyTask9C` / `Gp_EffSprTask46` / `Gp_DrawEffSprite81` / `Gp_EffSprTask81` /
/// `Gp_EffCtlTaskC1` /
/// `Gp_EffCtlTaskF3` / `Gp_EffCtlTaskF4` / `Gp_EffCtlTaskA5` / `Gp_EffCtlTaskA6` /
/// `Gp_EffCtlTaskE3` (`Mem_Calloc(0x2C)` in `Gp_SpawnEff`).
/// `field_0` is the spawned `Task*` (`Gp_SpawnEff` stores it; `Gp_SpawnWeaponEff`
/// copies it onto `GameActor.field_914`).
/// `field_8` is the parent coordinate copied onto `GsCOORDINATE2.sub`.
/// `field_10` is the 3-halfword overlay `Gp_EffCtlTaskE3` passes to
/// `Gp_SpawnEff`; `Gp_EffCtlTaskF4` also zeros `field_10` / `field_14` on
/// first run. `Gp_EffCtlTask9B` treats `field_10` / `field_18` as `SVECTOR`s
/// (`VectorNormalSS` of `field_18` into `field_10`, then GPF-scales `field_10`
/// onto the spawned work). `Gp_EffCtlTaskA6` rotates `field_10` by `coord` and
/// adds the result at `field_18` onto `coord.t[]`; `field_14` is also a
/// lifetime that starts at `(field_24 & 0x1F) % (spawnArg1 * 3) + 7` and
/// decays by `(field_22 & 3) / 3`. `field_12` is the per-frame Y step (`0xFFF0` minus
/// an LCG nibble in `Gp_EffCtlTaskF4`). `field_18` / `field_1A` / `field_1C` are
/// sign-extended into `coord.t[]` on first run. If they are all zero,
/// `Gp_EffCtlTask9B` fills them from three LCG draws centered on 0. `field_20` is the spawn-wave count
/// (`Gp_EffCtlTaskA5`), the draw-step counter that `Gp_EffCtlTaskF4` increments
/// every 4 `field_22` ticks and kills at 8, or
/// `(Gp_StateC08.field_0 % 10) - 1` (`Gp_EffCtlTaskF3` / `Gp_EffCtlTaskAC`). `field_22` is the step
/// counter (`Gp_EffCtlTask6E` / `Gp_EffCtlTask3B` / `Gp_EffPolyTask9C` /
/// `Gp_EffCtlTaskF3` / `Gp_EffCtlTaskF4` / `Gp_EffCtlTaskAC` / `Gp_EffCtlTaskA5` / `Gp_EffCtlTaskA6`).
/// `field_24` is the lifetime (`Gp_EffCtlTask6E`, `spawnArg1 >> 16` or 0xC),
/// the current scale stepped toward `field_26` (`Gp_EffSprTask46`),
/// the LCG angle (`Gp_EffCtlTask3B`), a 0x10 start that decays by 2
/// (`Gp_EffPolyTask9C`), a 0x80 start that decays by 8 (`Gp_EffCtlTaskC1`), the
/// spawn/wait phase flag (`Gp_EffCtlTaskA5`), or the LCG draw param
/// (`Gp_EffCtlTaskF4`). `Gp_EffCtlTaskA6` uses `field_24` as the LCG Y angle
/// (`Gfx_RotMatrixY` with `& 0xFF0`), `field_26` as `(field_24 & 0xF) + 8`
/// then a state-2 decay of `field_22 & 1`, `field_28` as
/// `-(spawnArg1 << 4) - (LCG >> 16 & 0x7F)` stepped by +2, and `field_2A`
/// as `spawnArg1 * 24 + 0xC0` stepped by +2 (also `* 3 + 0x3000` into
/// `Gp_SpawnEff`). `Gp_EffCtlTaskE3` copies `spawnArg1` into `field_24` /
/// `field_26` and sets `field_28 = field_26 << 2`. `Gp_EffCtlTaskC1` inits
/// `field_26` to 0x100 and adds 0x80 each frame, and copies
/// `D_80112C6C[field_2 & 3]` into `field_28`. `Gp_EffCtlTaskF4` copies
/// `spawnArg1 & 0xFFF` into `field_26` and `spawnArg1 & 0xF000` into
/// `field_28` (bit `0x8000` selects the LCG `| 0x1000` draw path).
/// `Gp_EffPolyTask9C` inits `field_26` to 0x20 and adds `field_2A` each frame.
/// `Gp_EffCtlTaskF3` inits `field_26` to 0x20, `field_28` to
/// `(field_20 << 7) + 0x180`, and `field_2A` to `(field_20 << 8) + 0x400`.
/// `Gp_EffCtlTaskAC` inits `field_26` to 0x20, `field_28` to
/// `((field_20 + 1) * 3) << 7`, and `field_2A` to `Wip_SysConfig.field_18`.
/// `Gp_EffCtlTaskA5` uses `field_26` as the inter-wave wait timer. `field_2A`
/// is the packed parameter passed through to `Gp_DrawEffSprite46`, the per-frame
/// `field_26` step, or `Gp_EffCtlTaskF3`'s `Gp_SpawnEff` spawn arg.
typedef struct _GpEffWork {
    /* 0x00 */ struct _Task*          field_0;
    /* 0x04 */ s32                    field_4;
    /* 0x08 */ struct _GsCOORDINATE2* field_8;
    /* 0x0C */ SVECTOR*               field_C;
    /* 0x10 */ s16                    field_10;
    /* 0x12 */ s16                    field_12;
    /* 0x14 */ s16                    field_14;
    /* 0x16 */ s16                    pad_16;
    /* 0x18 */ s16                    field_18;
    /* 0x1A */ s16                    field_1A;
    /* 0x1C */ s16                    field_1C;
    /* 0x1E */ byte                   pad_1E[2];
    /* 0x20 */ s16                    field_20;
    /* 0x22 */ s16                    field_22;
    /* 0x24 */ s16                    field_24;
    /* 0x26 */ s16                    field_26;
    /* 0x28 */ s16                    field_28;
    /* 0x2A */ s16                    field_2A;
} GpEffWork;
STATIC_ASSERT_SIZEOF(GpEffWork, 0x2C);

/// 32-bit view of a `MATRIX` rotation block. `Gp_EffSprTask30` writes an
/// identity rotation into `GsCOORDINATE2.coord` with word-sized stores:
/// `w0` covers `m[0][0]` / `m[0][1]`, `w1` covers `m[0][2]` / `m[1][0]`,
/// `w2` covers `m[1][1]` / `m[1][2]`, `w3` covers `m[2][0]` / `m[2][1]`,
/// and `h4` is `m[2][2]`.
typedef struct _GpMtxWords {
    /* 0x00 */ s32 w0;
    /* 0x04 */ s32 w1;
    /* 0x08 */ s32 w2;
    /* 0x0C */ s32 w3;
    /* 0x10 */ s16 h4;
} GpMtxWords;

/// 4-byte row of `D_8011291C`, indexed by `Task::spawnArg1`.
/// `Gp_EffPolyTask9C` copies `field_0` / `field_2` into `GpEffWork.field_28` /
/// `field_2A` (draw param for `Gp_DrawEffShard` and per-frame `field_26` step).
typedef struct _GpEffRec {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpEffRec;
STATIC_ASSERT_SIZEOF(GpEffRec, 4);

extern GpEffRec D_8011291C[];

/// 0xC-byte sprite frame of `Gp_EffSprRecs`, indexed by `GpEffWork.field_22`.
/// `w` is both the UV quad size and the billboard scale factor. `u` / `v` are
/// the UV origin. `clutX` / `clutY` feed `getClut`; `tpageX` feeds
/// `getTPage(0, 1, tpageX, 0)`.
typedef struct _GpEffSprRec {
    /* 0x0 */ u16 w;
    /* 0x2 */ u8  u;
    /* 0x3 */ u8  pad3;
    /* 0x4 */ u8  v;
    /* 0x5 */ u8  pad5;
    /* 0x6 */ u16 clutX;
    /* 0x8 */ u16 clutY;
    /* 0xA */ u16 tpageX;
} GpEffSprRec;
STATIC_ASSERT_SIZEOF(GpEffSprRec, 0xC);

extern GpEffSprRec Gp_EffSprRecs[];

/// 8-byte sprite frame of `D_80111E48`, indexed by
/// `GpEffWork.field_22 / GpEffWork.field_28` in `Gp_EffSprTask5C`.
/// `u` / `v` are the UV origin of a 0x28-wide quad; `clutX` / `clutY` feed
/// `getClut`. TPage is hardcoded to 0x29.
typedef struct _GpEffUv8 {
    /* 0x0 */ u8  u;
    /* 0x1 */ u8  pad1;
    /* 0x2 */ u8  v;
    /* 0x3 */ u8  pad3;
    /* 0x4 */ u16 clutX;
    /* 0x6 */ u16 clutY;
} GpEffUv8;
STATIC_ASSERT_SIZEOF(GpEffUv8, 8);

extern GpEffUv8 D_80111E48[];

/// Overlay of `D_80112964` at `u16` index `GpEffWork.field_2A`.
/// `Gp_DrawEffSprite81` loads `field_4`, shifts it into a CLUT X nibble, and
/// ORs `0x4280`. `Gp_DrawEffQuadT29` uses the same table at byte offset 8.
typedef struct _GpEffClutOff {
    /* 0x0 */ u16 pad_0;
    /* 0x2 */ u16 pad_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 pad_6;
    /* 0x8 */ u16 field_8;
} GpEffClutOff;

/// u16 CLUT-source table. Indexed as `&D_80112964[field_2A]` then overlaid
/// with `GpEffClutOff` so the load is `lhu 4(base + field_2A * 2)`.
extern u16 D_80112964[];

/// Spawn-id words indexed by the 3-digit packing of `Gp_StateC08.field_0`
/// `(hundreds-1)*9 + (tens-1)*3 + ones - 1`. `Gp_EffTask07State1` uses this
/// when `field_3 == 1`, and `D_80112A50` when `field_3 == -1`.
extern s32 D_80112978[];
extern s32 D_80112A50[];

/// Spawn-id words for `Gp_EffCtlTaskAE`, indexed with the same 3-digit packing
/// of `Gp_StateC08.field_0` as `D_80112978`; the value becomes the task's
/// `Task::spawnArg1` sound id.
extern s32 D_80112B94[];

/// `GsCOORDINATE2` index parallel to `D_80112978`. `Gp_EffTask07State1` adds
/// it onto `GameActorExt.field_8` when `field_3 == 1`.
extern u16 D_80112B28[];

/// 4 packed RGB-nibble colors. `Gp_EffCtlTaskC1` indexes with
/// `GpEffSpawnArg.field_2 & 3` and stores the halfword in `GpEffWork.field_28`.
extern u16 D_80112C6C[];

/// Message-handler table stored in `Task::field_24` by `Gp_InitPlayerWork`.
extern s32 Gp_PlayerMsgTable[];

/// Overlay of `Task::spawnArg1` for `Gp_EffSprTask46` / `Gp_EffCtlTaskC1` /
/// `Gp_EffCtlTaskF4`. `Gp_EffSprTask46` uses `field_0 & 0xFFF` as the target
/// scale and `field_2 & 0xF` as the draw parameter; the parent word's
/// `0x20000000` / `0x10000000` bits pick the start state. `Gp_EffCtlTaskC1`
/// uses the parent word's low 12 bits as a Z rotation and `field_2 & 3` as
/// an index into `D_80112C6C`. `Gp_EffCtlTaskF4` copies `field_0 & 0xFFF` to
/// `GpEffWork.field_26` and `field_0 & 0xF000` to `field_28`.
typedef struct _GpEffSpawnArg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ s16 field_2;
} GpEffSpawnArg;
STATIC_ASSERT_SIZEOF(GpEffSpawnArg, 4);

/// Overlay of `Task::spawnArg1` when the high byte is an `lb` nibble.
/// `Gp_EffSprTask5C` copies `field_3 & 0xF` into `GpEffWork.field_20`.
typedef struct _GpEffSpawnArgHi {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  pad_2;
    /* 0x3 */ s8  field_3;
} GpEffSpawnArgHi;
STATIC_ASSERT_SIZEOF(GpEffSpawnArgHi, 4);

/// Record whose word at 0x4 is the id `func_8010B2D4` passes to
/// `Gp_ScaleDamage` (and `func_8010B348` passes to `Gp_LookupIdField`).
/// `func_8010B348` also switches on the low 16 bits (2/4 vs 3) before
/// that call.
typedef struct _GpIdRec {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  field_4;
} GpIdRec;

/// Direction argument for `Gp_ApplyDirArg`. `field_0` / `field_8` are the
/// X/Z components passed to `ratan2`. `field_10 == 7` selects the
/// facing-vs-direction update of `GameActor.field_973`.
typedef struct _GpDirArg {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[4];
    /* 0x10 */ s16  field_10;
} GpDirArg;

/// Argument for `Gp_MoveActorBy` / `Gp_MoveActorByKeep`. Same XYZ words as
/// `GpDirArg` (added onto `GsCOORDINATE2.coord.t` and passed through to
/// `Gp_ApplyDirArg`). `field_10` is copied to `GameActor.field_983`.
/// `field_12 == 0` runs the shared actor-state reset.
typedef struct _GpMoveArg {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[4];
    /* 0x10 */ u8   field_10;
    /* 0x11 */ byte pad_11;
    /* 0x12 */ u8   field_12;
} GpMoveArg;

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawEffSprite81` and
/// `Gp_EffSprTask8D`.
/// `vec` is `workm.t[]` truncated to s16 for `gte_ldv0`. `otz` is
/// `gte_stszotz` then incremented; `flag` is `gte_stflg`; `size` is
/// `(field_24 * 15 / otz) >> 1` (`Gp_DrawEffSprite81`) or `field_28 * 23 / otz`
/// (`Gp_EffSprTask8D`); `sx`/`sy` are `gte_stsxy`.
typedef struct _GpEffFt4Scratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     size;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} GpEffFt4Scratch;
STATIC_ASSERT_SIZEOF(GpEffFt4Scratch, 0x18);

/// One corner of the unit quad in `D_80111E38`: a signed XZ pair scaled by
/// the caller's half-size before being rotated into world space.
typedef struct _GpQuadCorner {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
} GpQuadCorner;
STATIC_ASSERT_SIZEOF(GpQuadCorner, 0x4);

/// Unit quad corners `(-1, 1)`, `(1, 1)`, `(-1, -1)`, `(1, -1)`.
extern GpQuadCorner D_80111E38[4];

/// 0x38-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawEffSprite7C`.
/// `vec[]` holds the four rotated + translated quad corners fed to the GTE;
/// `otz` is `gte_stszotz` then incremented, `flag` is `gte_stflg`, and
/// `sxy0` (RTPS of `vec[0]`) plus `sxy1`..`sxy3` (RTPT of the rest) are the
/// projected screen positions copied into the `POLY_FT4`.
typedef struct _GpQuadScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ DVECTOR sxy0;
    /* 0x2C */ DVECTOR sxy1;
    /* 0x30 */ DVECTOR sxy2;
    /* 0x34 */ DVECTOR sxy3;
} GpQuadScratch;
STATIC_ASSERT_SIZEOF(GpQuadScratch, 0x38);

/// 0x1C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawEffSprite6C`,
/// `Gp_EffSprTask54`, `Gp_EffSprTask55`, `Gp_EffSprTask42`, `Gp_EffSprTask3F`,
/// `Gp_EffSprTaskE0`, and `Gp_DrawEffSpriteE2`.
/// `vec` is the coordinate's `workm.t[]` truncated to s16 and fed to
/// `gte_ldv0`. `otz` is `gte_stszotz`, `flag` is `gte_stflg` and `sxy` is
/// `gte_stsxy` of the single RTPS. `dx` / `dy` are the rotated half-extents
/// `(size * 55 / otz) * rsin/rcos(angle) >> 12` that offset `sxy` into the
/// four corners of the billboard `POLY_FT4`.
typedef struct _GpEffBeamScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ DVECTOR sxy;
} GpEffBeamScratch;
STATIC_ASSERT_SIZEOF(GpEffBeamScratch, 0x1C);

/// 0x1C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_EffSprTaskA7`. Same
/// contents as `GpEffBeamScratch` in a different field order: `vec` is the
/// coordinate's `workm.t[]` truncated to s16 and fed to `gte_ldv0`, `otz` is
/// `gte_stszotz` then incremented, `flag` is `gte_stflg` and `sxy` is
/// `gte_stsxy` of the single RTPS. `dx` / `dy` are the rotated half-extents
/// `(field_26 * 31 / otz) * rsin/rcos(angle) >> 12` that offset `sxy` into
/// the four corners of the billboard `POLY_FT4`.
typedef struct _GpEffFlareScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     dx;
    /* 0x08 */ s32     dy;
    /* 0x0C */ s32     flag;
    /* 0x10 */ SVECTOR vec;
    /* 0x18 */ DVECTOR sxy;
} GpEffFlareScratch;
STATIC_ASSERT_SIZEOF(GpEffFlareScratch, 0x1C);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `Gp_EffTileTaskA4`.
/// `vec` is the coordinate's `workm.t[]` truncated to s16 and fed to
/// `gte_ldv0`. `otz` receives `gte_stszotz`, `flag` `gte_stflg` and `sxy`
/// `gte_stsxy` of the single RTPS that places the spark `TILE`.
typedef struct _GpEffTileScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ DVECTOR sxy;
} GpEffTileScratch;
STATIC_ASSERT_SIZEOF(GpEffTileScratch, 0x14);

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `Gp_EffLineTask92` and
/// `Gp_EffLineTaskA3`.
/// `vec0` is the coordinate's current `workm.t[]` truncated to s16.
/// `Gp_EffLineTask92` puts the previous-frame position (`GpEffWork.field_18`
/// ..`field_1C`) in `vec1`. `Gp_EffLineTaskA3` rotates `field_10` through
/// `field_8->coord` and `Gfx_ViewWorldMtx`, scales by `field_22 << 11 + 0x1000`,
/// and adds `vec0` into `vec1`. Each vector is projected with its own RTPS:
/// `sxy0` / `sxy1` receive `gte_stsxy`, `flag` `gte_stflg` and `otz`
/// `gte_stszotz`, giving the two endpoints of a trail `LINE_F2` /
/// `LINE_G2`.
typedef struct _GpEffLineScratch {
    /* 0x00 */ SVECTOR vec0;
    /* 0x08 */ SVECTOR vec1;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s32     flag;
    /* 0x18 */ DVECTOR sxy0;
    /* 0x1C */ DVECTOR sxy1;
} GpEffLineScratch;
STATIC_ASSERT_SIZEOF(GpEffLineScratch, 0x20);

/// 0x10-byte scratch from `G_SCRATCH_HEAD` used by `func_8010133C`.
/// `field_0` / `field_4` are the outer/inner loop counters. `field_8` is
/// a color word (`0x808008`, then `0x37A78`). `field_C` / `field_E` are
/// stepped s16 coordinates (`x += 0x40`, `y -= 0x50`).
typedef struct _GpScratch10 {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s16 field_E;
} GpScratch10;
STATIC_ASSERT_SIZEOF(GpScratch10, 0x10);

/// Word + high-half overlay for a 16.16 component written by `func_800E0FEC`.
typedef union {
    s32 w;
    struct {
        s16 lo;
        s16 hi;
    } h;
} GpFixed16;

/// 0x10-byte scratch from `G_SCRATCH_HEAD` used by `func_801011D0`.
/// Words at 0/4/8 are the 16.16 deltas from `func_800E0FEC`; if the
/// fractional half is nonzero they are stepped away from zero by 0x10000
/// and the high half is added onto `GsCOORDINATE2.coord.t[]`.
typedef struct _GpDeltaScratch {
    /* 0x00 */ GpFixed16 vx;
    /* 0x04 */ GpFixed16 vy;
    /* 0x08 */ GpFixed16 vz;
    /* 0x0C */ s32       pad;
} GpDeltaScratch;
STATIC_ASSERT_SIZEOF(GpDeltaScratch, 0x10);

/// 0xC-byte scratch from `G_SCRATCH_HEAD` used by `func_80103E7C`.
/// `field_0` / `field_4` / `field_8` are the wrap candidates
/// `tgt - cur`, `tgt - cur + 0x1000`, and `tgt - cur - 0x1000`.
/// The function returns the candidate with the smallest absolute value.
typedef struct _GpAngleScratch {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
} GpAngleScratch;
STATIC_ASSERT_SIZEOF(GpAngleScratch, 0xC);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `Gp_PlayerMode2State4`.
/// `field_0` is the clamped `func_80103E7C` turn delta applied to
/// `GameActor.field_52`. `vec` is the target-minus-current offset
/// (`GameActor.field_20/24/28` minus `GsCOORDINATE2.coord.t`).
typedef struct _GpApproachScratch {
    /* 0x00 */ s32     field_0;
    /* 0x04 */ VECTOR3 vec;
    /* 0x10 */ s32     pad;
} GpApproachScratch;
STATIC_ASSERT_SIZEOF(GpApproachScratch, 0x14);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `func_8010BD88`.
/// `vx`/`vy`/`vz` overlay a `VECTOR3` for `func_80103C74`; `angle` holds
/// the `ratan2` result and the clamped turn delta applied to
/// `GameActor.field_52`.
typedef struct _GpTurnScratch {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
    /* 0x10 */ s32 angle;
} GpTurnScratch;
STATIC_ASSERT_SIZEOF(GpTurnScratch, 0x14);

/// `GameActor.field_17C` collision record (a `GpRec18`) as `func_80109BB4`
/// reads it: `flags` bit 0 marks the slot occupied and its high nibble is
/// the index of the `GpObj` node in `GameActor.field_AC` that produced the
/// hit, `dist` is the contact radius, and `id` / `kind` are the low / high
/// halves of `GpRec18.field_4`. `kind` 3 is the push-back case (only for
/// `id < 0x46` entries whose `D_80113F9C` entry is 1), 4 and 5 dispatch to
/// `func_8010B2D4` / `func_8010B348`. `x` / `y` / `z` are the world-space
/// contact point (`GpRec18.field_8`).
typedef struct _GpHitRec {
    /* 0x00 */ u16  flags;
    /* 0x02 */ s16  dist;
    /* 0x04 */ u16  id;
    /* 0x06 */ u16  kind;
    /* 0x08 */ s16  x;
    /* 0x0A */ s16  y;
    /* 0x0C */ s16  z;
    /* 0x0E */ byte pad_E[0xA];
} GpHitRec;
STATIC_ASSERT_SIZEOF(GpHitRec, 0x18);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `Gp_StepPlayerMove`.
/// `scale` is `D_80112E10[field_958]` (signed, stored as a word). `angle`
/// holds `0x640000` then the yaw passed to `Gfx_RotMatrixY`. `saved` is a
/// copy of `GsCOORDINATE2.coord` around that rotate. `vec` is the matrix
/// column from `Gfx_MatrixCol2` / `VectorNormalSS`, later the Manhattan
/// `|dx|+|dz|` to the lock point. `lock` is `Gp_GetLockPos` output.
typedef struct _GpMoveScratch {
    /* 0x00 */ s32     scale;
    /* 0x04 */ s32     angle;
    /* 0x08 */ MATRIX  saved;
    /* 0x28 */ SVECTOR vec;
    /* 0x30 */ VECTOR3 lock;
    /* 0x3C */ s32     pad;
} GpMoveScratch;
STATIC_ASSERT_SIZEOF(GpMoveScratch, 0x40);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `func_80109BB4`.
/// `pos` is the world position of the colliding `GpObj` (its 0x10 SVECTOR
/// rotated by `field_8->workm` plus that matrix's translation), later
/// reused to save the actor's pre-push `coord.t[0]` / `t[2]`. `delta` is
/// `pos` minus the contact point, `unit` its `VectorNormal`, and `local`
/// that direction in grid space via `Gp_GridParams->field_0->workm`.
typedef struct _GpPushBackScratch {
    /* 0x00 */ VECTOR pos;
    /* 0x10 */ VECTOR delta;
    /* 0x20 */ VECTOR local;
    /* 0x30 */ VECTOR unit;
} GpPushBackScratch;
STATIC_ASSERT_SIZEOF(GpPushBackScratch, 0x40);

/// 0x68-byte scratch from `G_SCRATCH_HEAD` used by `func_8010BE5C`.
/// `vec` overlays a `VECTOR3` for `func_80103C74` / `ratan2`. `rot` is
/// the zeroed `SVECTOR` passed to `Gp_PlaceCoordOffset`. The remaining 0x50
/// bytes are a temp `GsCOORDINATE2` at `head - 0x50`.
typedef struct _GpAimScratch {
    /* 0x00 */ VECTOR3  vec;
    /* 0x0C */ s32      pad_C;
    /* 0x10 */ SVECTOR3 rot;
    /* 0x16 */ s16      pad_16;
    /* 0x18 */ byte     pad_18[0x50];
} GpAimScratch;
STATIC_ASSERT_SIZEOF(GpAimScratch, 0x68);

/// 0x68-byte scratch from `G_SCRATCH_HEAD` used by `Gp_PickNearestRec18`.
/// The first 0x10 bytes are the `GpDeltaScratch` passed to `func_800E0FEC`.
/// Offset 0x10 is a temp `GsCOORDINATE2` (`flg` at 0x10, `workm.t[]` at
/// 0x48, `sub` at 0x5C) passed to `Gp_SpawnEff`. `offset` is three
/// `rand() & 7` halfwords passed as that call's last argument and added
/// onto `arg2->workm.t[]` when `arg2` is non-NULL.
typedef struct _GpPickScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ u32     flg;
    /* 0x14 */ byte    pad_14[0x34];
    /* 0x48 */ s32     t[3];
    /* 0x54 */ byte    pad_54[8];
    /* 0x5C */ void*   sub;
    /* 0x60 */ SVECTOR offset;
} GpPickScratch;
STATIC_ASSERT_SIZEOF(GpPickScratch, 0x68);

/// 8-byte rotation row (`SVECTOR` layout). `D_801131B4` is indexed by
/// `Gp_AimPitchRec` arg1 (`D_80167218[Mc_SaveData.field_5C7]`) and by
/// `Wip_SysConfig.field_21` in `Gp_AimYawToLock`.
typedef struct _GpAimRot {
    /* 0x0 */ s16 vx;
    /* 0x2 */ s16 vy;
    /* 0x4 */ s16 vz;
    /* 0x6 */ s16 pad;
} GpAimRot;
STATIC_ASSERT_SIZEOF(GpAimRot, 8);

/// 0x6C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_AimYawToLock`.
/// The first 0x50 bytes are a temp `GsCOORDINATE2`. `delta` is
/// `Gp_GetLockPos` output minus that coord's translation (computed in
/// place). `rot` is the `SVECTOR` passed to `Gp_PlaceCoordOffset` (table row
/// `D_801131B4[Wip_SysConfig.field_21]`). `angle` holds `ratan2` then
/// the wrapped, clamped yaw delta applied to `GameActor.field_52`.
typedef struct _GpYawScratch {
    /* 0x00 */ byte     pad_0[0x50];
    /* 0x50 */ VECTOR3  delta;
    /* 0x5C */ s32      pad_5C;
    /* 0x60 */ SVECTOR3 rot;
    /* 0x66 */ s16      pad_66;
    /* 0x68 */ s32      angle;
} GpYawScratch;
STATIC_ASSERT_SIZEOF(GpYawScratch, 0x6C);

/// 0x2C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_PlayerMode2State3`.
/// `mtx` receives a copy of the actor coordinate's `coord` matrix, pitched by
/// `Gfx_RotMatrixX`; `dir` (at `head - 0xC`) is that matrix's third column
/// normalized by `VectorNormalSS`, and `div` is the frame count the direction
/// is divided by to produce `GameActor.field_0` / `field_4` / `field_8`.
typedef struct _GpDashScratch {
    /* 0x00 */ MATRIX  mtx;
    /* 0x20 */ SVECTOR dir;
    /* 0x28 */ s32     div;
} GpDashScratch;
STATIC_ASSERT_SIZEOF(GpDashScratch, 0x2C);

/// 0x84-byte scratch from `G_SCRATCH_HEAD` used by `Gp_AimPitchToLock`,
/// `Gp_AimPitchToLockAlt`, `Gp_AimPitchRec`, and `Gp_AimPitchDirect`. The first 0x50 bytes are a
/// temp `GsCOORDINATE2`. `delta` is lock position minus that coord's
/// translation; `lock` is `Gp_GetLockPos` output; `rot` is the
/// `SVECTOR` passed to `Gp_PlaceCoordOffset` (zeros then table row in
/// `Gp_AimPitchToLockAlt`, table row in `Gp_AimPitchRec`, zeros in
/// `Gp_AimPitchDirect`). `angle` holds `ratan2` then the clamped pitch
/// delta applied to `GameActor.field_58` / `field_5C` / `field_60` /
/// `field_64` / `field_70` / `field_78`; `dist` is the XZ length of
/// `delta`. `Gp_AimPitchToLock` also derives `field_5C` / `field_64` from
/// the updated `field_58` / `field_60` (`/ 5` scaled by 3 then 2).
typedef struct _GpPitchScratch {
    /* 0x00 */ byte     pad_0[0x50];
    /* 0x50 */ VECTOR3  delta;
    /* 0x5C */ s32      pad_5C;
    /* 0x60 */ VECTOR3  lock;
    /* 0x6C */ s32      pad_6C;
    /* 0x70 */ SVECTOR3 rot;
    /* 0x76 */ s16      pad_76;
    /* 0x78 */ s32      angle;
    /* 0x7C */ s32      dist;
    /* 0x80 */ s32      pad_80;
} GpPitchScratch;
STATIC_ASSERT_SIZEOF(GpPitchScratch, 0x84);

/// Argument for `Gp_SetActorDest`. `field_0` / `field_4` / `field_8` are
/// copied onto `GameActor.field_20` / `field_24` / `field_28`.
typedef struct _GpVecArg {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
} GpVecArg;
STATIC_ASSERT_SIZEOF(GpVecArg, 0xC);

/// Optional companion for `Gp_SetActorDest`. `field_0` / `field_4` are copied
/// onto `GameActor.field_93C` / `field_93E`. NULL zeros both halfwords.
typedef struct _GpOverrideArg {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ u16  field_4;
} GpOverrideArg;
STATIC_ASSERT_SIZEOF(GpOverrideArg, 6);

/// Argument for `func_80104F5C`. `field_0` (low 16 bits) is copied onto
/// `GameActor.field_80`; `field_4` is copied onto `GameActor.field_82`.
/// Nonzero `field_0` selects anim 0x25, else 0x24, via `Gp_AnimPlayChildSlots`.
typedef struct _GpFacingArg {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s16 field_4;
} GpFacingArg;
STATIC_ASSERT_SIZEOF(GpFacingArg, 8);

/// Argument for `func_801052B8`. `field_0` is copied onto
/// `GameActor.field_93E`; `field_4` is copied onto `GameActor.field_934`.
typedef struct _GpCountArg {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ s32  field_4;
} GpCountArg;
STATIC_ASSERT_SIZEOF(GpCountArg, 8);

/// Argument for `func_801054D8` / `func_8010C75C`. `field_14` is copied
/// onto `GameActor.field_934` (frame delay); `field_93E` is cleared.
typedef struct _GpDelayArg {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} GpDelayArg;

/// 0x14-byte argument for `func_80104508` / `func_80104B54` / `func_80104CAC` /
/// `func_8010C4F0`. `func_80104B54` / `func_80104CAC` copy `field_0` to
/// `GameActor.field_928`. `func_80104508` / `func_8010C4F0` treat `field_0` as
/// an index into `Gp_PlayerAnimBlkTbl` / `Gp_AnimBlkTbl` and only reinstall when that
/// pointer differs. `field_8 == 0` runs `Gp_AnimResetChildSlots` (plus `func_800B3F84`
/// in `func_80104B54` / `func_80104CAC`, and in `func_80104508` /
/// `func_8010C4F0` only on a table miss); otherwise
/// `Gp_AnimPlayChildSlotsEx(..., field_4, 0 or 1, field_C)`. `field_10` selects
/// `field_983` (7 if nonzero, `0x38` if zero).
typedef struct _GpAnimArg {
    /* 0x00 */ void* field_0;
    /* 0x04 */ s32   field_4;
    /* 0x08 */ s32   field_8;
    /* 0x0C */ s32   field_C;
    /* 0x10 */ s32   field_10;
} GpAnimArg;
STATIC_ASSERT_SIZEOF(GpAnimArg, 0x14);

/// Animation data pointed to by `Gp_PlayerAnimBlkTbl` / `Gp_AnimBlkTbl` /
/// `GameActor.field_928`. `Gp_CopyPlayerAnim` and `Gp_CopyAllyAnim` copy up to
/// 0x20 words onto `field_BC`.
typedef struct _GpAnimBlk {
    /* 0x00 */ byte pad_0[0xBC];
    /* 0xBC */ s32  field_BC[0x20];
} GpAnimBlk;

/// Word-copy argument for `Gp_CopyPlayerAnim` / `Gp_CopyAllyAnim`. `field_0` is
/// the source array; `field_4` is the count (must be < 0x21).
typedef struct _GpCopyArg {
    /* 0x0 */ s32* field_0;
    /* 0x4 */ s32  field_4;
} GpCopyArg;
STATIC_ASSERT_SIZEOF(GpCopyArg, 8);

/// Position + rotation argument for `func_80104D68` / `func_80104E00`.
/// `field_0` / `field_4` / `field_8` are copied onto `GsCOORDINATE2.coord.t`;
/// `field_10` / `field_12` / `field_14` are copied onto `GameActor` 0x50 and
/// passed to `RotMatrix`. `func_80104E00` copies `field_12` onto
/// `GameActor.field_82` and picks anim 6 or 5 from the `func_80103E7C` delta.
typedef struct _GpXformArg {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[4];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
} GpXformArg;

/// 4-byte pad-event template indexed by `func_801041FC`. `field_0` / `field_2`
/// are passed to `Pad_PostEvent` (`lbu` / `lh`).
typedef struct _GpPadEvt {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} GpPadEvt;
STATIC_ASSERT_SIZEOF(GpPadEvt, 0x4);

/// 2-slot table of current actor-work pointers. Slot 0 is the primary
/// work (set/cleared by `Gp_InitPlayerWork` / `Gp_TeardownSlot0`). Walked as a
/// pair by `Gp_ClearSlotNodeFlags`, `Gp_UnlinkNode`, `Gp_NodeSlotMask`, and
/// `Gp_ClearNodeSlots`. `Gp_AssignNodeSlot0` assigns `field_90C` on slot 0 only.
extern GpActorWork* volatile Gp_ActorSlots[2];

/// Flag byte cleared by `func_800A7DE0` / `Gp_SpawnPlayer`.
extern u8 D_80115768;

/// Four-entry `Task::state` dispatcher: `Gp_InitPlayerWork`, `Gp_PlayerWorkState1`,
/// `Gp_PlayerWorkState2`, `Gp_TeardownSlot0`.
extern TaskFuncTable4 Gp_PlayerWorkStates;

/// Four-entry `Task::state` dispatcher: `func_8010B590`, `func_8010B5C0`,
/// `func_8010B5E4`, `func_8010B5F0`.
extern TaskFuncTable4 D_80097AB0;

/// `Wip_SysConfig.field_21` dispatcher copied by `func_8010615C`. Unused
/// slots are `func_801065A0`; others are weapon-overlay entry points.
extern GpActorFuncTable33 D_800978BC;

/// `field_954` dispatcher: `Gp_TickPlayerNormal`, `Gp_TickPlayerMode1`, `Gp_TickPlayerMode2`.
extern GpActorFuncTable3 Gp_PlayerModeFns;

/// `field_956` dispatcher copied by `Gp_TickPlayerNormal`.
extern GpActorFuncTable8 D_8009794C;

/// `field_96C` dispatcher: three slots of `Gp_PlayerMode1State0`, then `Gp_PlayerMode1State3`.
extern GpActorFuncTable4 Gp_PlayerMode1States;

/// `field_956` dispatcher copied by `Gp_TickPlayerMode2`.
extern GpActorFuncTable12 Gp_PlayerMode2States;

/// u8 Task_Spawn type bases. `func_80104258` indexes
/// `D_80112DFC[arg2 + Wip_SysConfig.field_26 - 2]`.
extern u8 D_80112DFC[];

/// Pad-event templates for `func_801041FC` (`D_80112E28[arg1 & 0xFFFF]`).
extern GpPadEvt D_80112E28[];

/// 2-wide rows indexed by `Mc_SaveData.field_22`. `Gp_PlayerMode2StateB` passes
/// `D_80112E04[field_22][1]` to `func_80105894`.
extern u8 D_80112E04[][2];

/// s16 scale rows indexed by `GameActor.field_958`. `Gp_StepPlayerMove` divides
/// the normalized matrix-column by `D_80112E10[field_958]`.
extern s16 D_80112E10[];

/// u16 facing-step rows indexed by `GameActor.field_95A`. `Gp_TurnPlayer`
/// adds `D_80112E20[field_95A] * field_975` onto `field_52` (masked `0xFFF`).
extern u16 D_80112E20[];

/// 2-wide rows of `GsCOORDINATE2` indices. `func_8010403C` indexes
/// `D_80112E2C[Mc_SaveData.field_22 - 1][arg0]`.
extern u8 D_80112E2C[][2];

/// u16 turn-rate rows indexed by `Wip_SysConfig.field_21`. `Gp_AimYawToLock`
/// clamps the wrapped yaw delta to this value (or 1.5x when
/// `func_800B9D80(0x2000)` is set).
extern u16 D_80112E30[];

/// NULL-terminated `GpImgRec*` lists for `func_801030CC`. Indexed as
/// `table[type * 4 + Wip_SysConfig.field_26 - 5][frame]`. `D_80112E74` is
/// the `field_987` sequence; `D_80112EB4` is the `field_98A` sequence.
extern struct _GpImgRec** D_80112E74[];
extern struct _GpImgRec** D_80112EB4[];

/// Per-item flag byte indexed by `Wip_SysConfig.field_21`. Nonzero makes
/// `Gp_PlayerNormalState2` / `Gp_PlayerMode2StateA` pass `GameActor.field_97F` (the current
/// aim direction) to `func_80106264` instead of the default 1.
extern u8 D_80112EF8[];

/// 2-wide rows indexed by `Wip_SysConfig.field_21`. Zero at `[i][0]`
/// makes `func_801088D4` abort the item-use path (`field_95E = 0x3E8`).
extern u8 D_80112F1C[][2];

/// u16 table indexed by `Gp_AttachActorObj` arg1 and added onto
/// `GpActorD4Rec.field_C` when filling `field_4`.
extern u16 D_80112F60[];

/// 0x10-byte `VECTOR` rows indexed by `Gp_AttachActorObj` arg1. Copied through
/// scratch; the low 16 bits of `vx`/`vy`/`vz` seed `GpActorD4Rec.field_8` /
/// `field_A` / `field_C`.
extern VECTOR D_80112FA4[];

/// Overlay-imported s16 table indexed by `Mc_SaveData.field_5C7` and passed
/// to `func_80106350` (`func_8010C46C` / `func_8010C4F0` / `func_8010C75C`).
extern s16 D_80167218[];

/// Overlay-imported s16 table indexed by `Mc_SaveData.field_5C7` and passed
/// as the third argument of `Gp_AttachActorObj` (`Gp_SetupAllyWeapon`).
extern s16 D_80167224[];

/// Overlay-imported u8 table indexed by `Mc_SaveData.field_5C7` and stored
/// at `GpActorD4.field_CD` (`Gp_SetupAllyWeapon`).
extern u8 D_80167230[];

/// 8-byte `GpAimRot` rows copied onto `GpPitchScratch.rot`.
extern GpAimRot D_801131B4[];

/// u8 table indexed by `Mc_SaveData.field_5C7`. Non-zero selects
/// `Gp_AimPitchToLock`; zero uses `D_80167218` with `Gp_AimPitchRec`.
extern u8 D_80113388[];

void Gp_EffPolyTask9C(Task* arg0);
void Gp_DrawEffShard(struct _GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u16 arg3);
void Gp_EffSprTask46(Task* arg0);
void Gp_DrawEffSprite81(Task* arg0);
void Gp_DrawEffSprite46(struct _GsCOORDINATE2* arg0, s32 arg1, s16 arg2, u16 arg3);
void Gp_EffSprTask81(Task* arg0);
void Gp_EffSprTask55(Task* arg0);
void Gp_EffSprTask42(Task* arg0);
void Gp_EffCtlTask9B(Task* arg0);
void Gp_EffSprTask30(Task* arg0);
void Gp_DrawEffSpark(Task* arg0, s32 arg1, u8* arg2);
void Gp_DrawEffQuadT29(struct _GsCOORDINATE2* arg0, s32 arg1, u16 arg2, u16 arg3);
void Gp_EffTask07State1(Task* arg0);
void Gp_EffCtlTaskC1(Task* arg0);
void Gp_EffCtlTaskF3(Task* arg0);
void Gp_EffCtlTaskF4(Task* arg0);
void Gp_EffCtlTaskAC(Task* arg0);
void Gp_EffCtlTask0E(Task* arg0);
void Gp_PulseState1C80(void);
void Gp_EffCtlTaskA5(Task* arg0);
void Gp_EffCtlTaskA6(Task* arg0);
void Gp_EffCtlTaskE3(Task* arg0);
void Gp_EffSprTaskE0(Task* arg0);
void Gp_EffSprTaskE2(Task* arg0);
/// Hand-written GTE routine. `arg2` is a full 32-bit word: the high half picks
/// the CLUT (palette column) and the low 12 bits are the billboard size, so it
/// must not be declared `s16` (that makes callers emit a spurious `sll`/`sra`
/// truncation). It is unsigned because the size is divided by `otz` with `divu`.
void  Gp_DrawEffSpriteE2(struct _GsCOORDINATE2* arg0, u16 arg1, u32 arg2, s16 arg3);
s32   func_801011D0(struct _GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32* arg3);
void  Gp_InitPlayerWork(GpActorWork* arg0);
void  Gp_AttachActorObj(GpActorWork* arg0, s32 arg1, s32 arg2);
void  Gp_TeardownSlot0(GpActorWork* arg0);
void  Gp_BindActorAnim(GpActorWork* arg0);
Task* func_80104258(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
/// `arg3` is unused; the actor-init caller passes 0 so the `jal` delay
/// slot of the `field_93A` load is `move a3, a1`.
s32  func_80104508(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2, s32 arg3);
void func_801030CC(GpActorWork* arg0);
void func_801041FC(GpActorWork* arg0, s32 arg1);
s32  Gp_SpawnWeaponEff(void);
s32  Gp_SetupAllyWeapon(void);
void func_80106350(GpActorWork* arg0, s32 arg1, s32 arg2);
void func_801088D4(GpActorWork* arg0, s32 arg1, s32 arg2);
/// Overlay import. `func_801088D4` calls it with `Game_GetPtrSlot(0xA)` when
/// `Mc_SaveData.field_13 == 1`.
void  func_80166E94(void* arg0, s32 arg1);
void  Gp_PlayerMode1State0(GpActorWork* arg0);
s32   func_80109290(GpActorWork* arg0);
void  Gp_TriggerPeState(s32 arg0, s32 arg1);
void  func_8010A42C(GpActorWork* arg0, s32 arg1);
void  Gp_DetachLinkNode(GpActorWork* arg0);
s32   Gp_ApplyDirArg(GpActorWork* arg0, GpDirArg* arg1);
s32   func_80104E00(GpActorWork* arg0, s32 arg1, GpXformArg* arg2);
s32   Gp_SetActorDest(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3);
s32   Gp_MoveActorBy(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2);
s32   Gp_PickNearestRec18(GpRec18* arg0, struct _GsCOORDINATE2* arg1, struct _GsCOORDINATE2* arg2);
void  Gp_MoveActorByKeep(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2);
void  func_8010B210(GpActorWork* arg0);
void  Gp_BindActorD4(GpActorWork* arg0, SVECTOR3* arg1, s32 arg2);
s32   func_8010C30C(GpActorWork* arg0);
Task* Gp_SpawnPlayer(GpActorArg* arg0, u16 arg1, s32 arg2, GpActorFlags* arg3);
Task* Gp_SpawnAlly(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3);

#endif // GAMEPLAY_3FB8_H
