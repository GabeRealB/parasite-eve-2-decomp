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
    /* 0x20 */ byte          pad_20[0xC];
    /* 0x2C */ GameActorExt* extra; // Task::extra
} GpActorWork;

typedef void (*GpActorFunc)(GpActorWork* arg0);

/// 3-entry callback table copied onto the stack by `func_801083A0`.
typedef struct {
    GpActorFunc funcs[3];
} GpActorFuncTable3;

/// 8-entry callback table copied onto the stack by `func_80106838`.
typedef struct {
    GpActorFunc funcs[8];
} GpActorFuncTable8;

/// 4-entry callback table copied onto the stack by `func_80108E40`.
typedef struct {
    GpActorFunc funcs[4];
} GpActorFuncTable4;

/// 12-entry callback table copied onto the stack by `func_80108ED4`.
typedef struct {
    GpActorFunc funcs[12];
} GpActorFuncTable12;

/// 33-entry callback table copied onto the stack by `func_8010615C`.
typedef struct {
    GpActorFunc funcs[33];
} GpActorFuncTable33;

/// 0x10-byte spawn argument for `func_8010BAC8` / `func_801036FC`. `field_0`
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

/// Companion spawn record for `func_801036FC`. `field_0` is copied to
/// `GameActor.field_93C`. Nonzero `field_2` sets `field_954` to 2.
typedef struct _GpActorFlags {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ byte pad_3;
} GpActorFlags;
STATIC_ASSERT_SIZEOF(GpActorFlags, 0x4);

/// 0x18-byte record at `GpActorD4.field_88`. `func_8010C1FC` copies `arg1`
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

/// 0xD4-byte block allocated by `func_8010BAC8` (`Mem_Set` size 0xD4) and
/// stored at `GameActor.field_910`. `func_8010C1FC` copies a `GsCOORDINATE2`
/// into `field_18`, treats `field_68` as a `GpObj`, fills `field_88`, and
/// points `field_88.field_14` at `field_A0`. `func_8010BF7C` writes `field_C4`.
/// `func_8010B79C` writes `field_CD` from `D_80167230[Mc_SaveData.field_5C7]`.
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
    /* 0xCE */ byte         pad_CE[6];
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
/// as vx/vy/vz). `func_80100FCC` zeros them after `Gfx_RotMatrixX` of
/// `field_3D4.workm`.
typedef struct _GpActorSvec {
    /* 0x000 */ byte pad[0x418];
    /* 0x418 */ s16  field_418;
    /* 0x41A */ s16  field_41A;
    /* 0x41C */ s16  field_41C;
} GpActorSvec;

/// 8-byte argument record for `func_800FDB18`. `field_0` is a coordinate
/// (fallback `D_80070F10`); `field_4` / `field_6` are packed into the
/// `func_800EA478` argument. The third `func_800FDB18` argument is an
/// `SVECTOR*` (or NULL).
typedef struct _GpEffArg {
    /* 0x0 */ struct _GsCOORDINATE2* field_0;
    /* 0x4 */ s16                    field_4;
    /* 0x6 */ s16                    field_6;
} GpEffArg;
STATIC_ASSERT_SIZEOF(GpEffArg, 0x8);

/// 0x2C-byte work at `Task::spawnArg2` for `func_800F1364` / `func_800F1A9C` /
/// `func_800F5184` / `func_800F75BC` / `func_800F77F8` / `func_800FB67C` /
/// `func_800FB7E4` / `func_800FBEBC` / `func_800FC74C` / `func_800FE41C`
/// (`Mem_Calloc(0x2C)` in `func_800EA478`).
/// `field_0` is the spawned `Task*` (`func_800EA478` stores it; `func_801034C0`
/// copies it onto `GameActor.field_914`).
/// `field_8` is the parent coordinate copied onto `GsCOORDINATE2.sub`.
/// `field_10` is the 3-halfword overlay `func_800FE41C` passes to
/// `func_800EA478`; `func_800FBEBC` also zeros `field_10` / `field_14` on
/// first run. `func_800F9474` treats `field_10` / `field_18` as `SVECTOR`s
/// (`VectorNormalSS` of `field_18` into `field_10`, then GPF-scales `field_10`
/// onto the spawned work). `field_12` is the per-frame Y step (`0xFFF0` minus
/// an LCG nibble in `func_800FBEBC`). `field_18` / `field_1A` / `field_1C` are
/// sign-extended into `coord.t[]` on first run. If they are all zero,
/// `func_800F9474` fills them from three LCG draws centered on 0. `field_20` is the spawn-wave count
/// (`func_800FC74C`), the draw-step counter that `func_800FBEBC` increments
/// every 4 `field_22` ticks and kills at 8, or
/// `(D_80114C08.field_0 % 10) - 1` (`func_800FB7E4`). `field_22` is the step
/// counter (`func_800F1364` / `func_800F1A9C` / `func_800F5184` /
/// `func_800FB7E4` / `func_800FBEBC` / `func_800FC74C`).
/// `field_24` is the lifetime (`func_800F1364`, `spawnArg1 >> 16` or 0xC),
/// the current scale stepped toward `field_26` (`func_800F75BC`),
/// the LCG angle (`func_800F1A9C`), a 0x10 start that decays by 2
/// (`func_800F5184`), a 0x80 start that decays by 8 (`func_800FB67C`), the
/// spawn/wait phase flag (`func_800FC74C`), or the LCG draw param
/// (`func_800FBEBC`). `func_800FE41C` copies `spawnArg1` into `field_24` /
/// `field_26` and sets `field_28 = field_26 << 2`. `func_800FB67C` inits
/// `field_26` to 0x100 and adds 0x80 each frame, and copies
/// `D_80112C6C[field_2 & 3]` into `field_28`. `func_800FBEBC` copies
/// `spawnArg1 & 0xFFF` into `field_26` and `spawnArg1 & 0xF000` into
/// `field_28` (bit `0x8000` selects the LCG `| 0x1000` draw path).
/// `func_800F5184` inits `field_26` to 0x20 and adds `field_2A` each frame.
/// `func_800FB7E4` inits `field_26` to 0x20, `field_28` to
/// `(field_20 << 7) + 0x180`, and `field_2A` to `(field_20 << 8) + 0x400`.
/// `func_800FC74C` uses `field_26` as the inter-wave wait timer. `field_2A`
/// is the packed parameter passed through to `func_800F7AD4`, the per-frame
/// `field_26` step, or `func_800FB7E4`'s `func_800EA478` spawn arg.
typedef struct _GpEffWork {
    /* 0x00 */ struct _Task*          field_0;
    /* 0x04 */ byte                   pad_4[4];
    /* 0x08 */ struct _GsCOORDINATE2* field_8;
    /* 0x0C */ byte                  pad_C[4];
    /* 0x10 */ s16                   field_10;
    /* 0x12 */ s16                   field_12;
    /* 0x14 */ s16                   field_14;
    /* 0x16 */ s16                   pad_16;
    /* 0x18 */ s16                   field_18;
    /* 0x1A */ s16                   field_1A;
    /* 0x1C */ s16                   field_1C;
    /* 0x1E */ byte                  pad_1E[2];
    /* 0x20 */ s16                   field_20;
    /* 0x22 */ s16                   field_22;
    /* 0x24 */ s16                   field_24;
    /* 0x26 */ s16                   field_26;
    /* 0x28 */ s16                   field_28;
    /* 0x2A */ s16                   field_2A;
} GpEffWork;
STATIC_ASSERT_SIZEOF(GpEffWork, 0x2C);

/// 4-byte row of `D_8011291C`, indexed by `Task::spawnArg1`.
/// `func_800F5184` copies `field_0` / `field_2` into `GpEffWork.field_28` /
/// `field_2A` (draw param for `func_800F52B4` and per-frame `field_26` step).
typedef struct _GpEffRec {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpEffRec;
STATIC_ASSERT_SIZEOF(GpEffRec, 4);

extern GpEffRec D_8011291C[];

/// Spawn-id words indexed by the 3-digit packing of `D_80114C08.field_0`
/// `(hundreds-1)*9 + (tens-1)*3 + ones - 1`. `func_800FA7CC` uses this
/// when `field_3 == 1`, and `D_80112A50` when `field_3 == -1`.
extern s32 D_80112978[];
extern s32 D_80112A50[];

/// `GsCOORDINATE2` index parallel to `D_80112978`. `func_800FA7CC` adds
/// it onto `GameActorExt.field_8` when `field_3 == 1`.
extern u16 D_80112B28[];

/// 4 packed RGB-nibble colors. `func_800FB67C` indexes with
/// `GpEffSpawnArg.field_2 & 3` and stores the halfword in `GpEffWork.field_28`.
extern u16 D_80112C6C[];

/// Overlay of `Task::spawnArg1` for `func_800F75BC` / `func_800FB67C` /
/// `func_800FBEBC`. `func_800F75BC` uses `field_0 & 0xFFF` as the target
/// scale and `field_2 & 0xF` as the draw parameter; the parent word's
/// `0x20000000` / `0x10000000` bits pick the start state. `func_800FB67C`
/// uses the parent word's low 12 bits as a Z rotation and `field_2 & 3` as
/// an index into `D_80112C6C`. `func_800FBEBC` copies `field_0 & 0xFFF` to
/// `GpEffWork.field_26` and `field_0 & 0xF000` to `field_28`.
typedef struct _GpEffSpawnArg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ s16 field_2;
} GpEffSpawnArg;
STATIC_ASSERT_SIZEOF(GpEffSpawnArg, 4);

/// Record whose word at 0x4 is the id `func_8010B2D4` passes to
/// `func_800E2438` (and `func_8010B348` passes to `func_800E2CD4`).
/// `func_8010B348` also switches on the low 16 bits (2/4 vs 3) before
/// that call.
typedef struct _GpIdRec {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  field_4;
} GpIdRec;

/// Direction argument for `func_80103B88`. `field_0` / `field_8` are the
/// X/Z components passed to `ratan2`. `field_10 == 7` selects the
/// facing-vs-direction update of `GameActor.field_973`.
typedef struct _GpDirArg {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[4];
    /* 0x10 */ s16  field_10;
} GpDirArg;

/// Argument for `func_801053A0` / `func_8010C81C`. Same XYZ words as
/// `GpDirArg` (added onto `GsCOORDINATE2.coord.t` and passed through to
/// `func_80103B88`). `field_10` is copied to `GameActor.field_983`.
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

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `func_80107E1C`.
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

/// 0x68-byte scratch from `G_SCRATCH_HEAD` used by `func_8010BE5C`.
/// `vec` overlays a `VECTOR3` for `func_80103C74` / `ratan2`. `rot` is
/// the zeroed `SVECTOR` passed to `func_801040A0`. The remaining 0x50
/// bytes are a temp `GsCOORDINATE2` at `head - 0x50`.
typedef struct _GpAimScratch {
    /* 0x00 */ VECTOR3  vec;
    /* 0x0C */ s32      pad_C;
    /* 0x10 */ SVECTOR3 rot;
    /* 0x16 */ s16      pad_16;
    /* 0x18 */ byte     pad_18[0x50];
} GpAimScratch;
STATIC_ASSERT_SIZEOF(GpAimScratch, 0x68);

/// 8-byte rotation row (`SVECTOR` layout). `D_801131B4` is indexed by
/// `func_80102D20` arg1 (`D_80167218[Mc_SaveData.field_5C7]`) and by
/// `Wip_SysConfig.field_21` in `func_80102348`.
typedef struct _GpAimRot {
    /* 0x0 */ s16 vx;
    /* 0x2 */ s16 vy;
    /* 0x4 */ s16 vz;
    /* 0x6 */ s16 pad;
} GpAimRot;
STATIC_ASSERT_SIZEOF(GpAimRot, 8);

/// 0x84-byte scratch from `G_SCRATCH_HEAD` used by `func_80102D20` and
/// `func_80102F10`. The first 0x50 bytes are a temp `GsCOORDINATE2`.
/// `delta` is lock position minus that coord's translation; `lock` is
/// `func_800DAE50` output; `rot` is the `SVECTOR` passed to
/// `func_801040A0` (table row in `func_80102D20`, zeros in
/// `func_80102F10`). `angle` holds `ratan2` then the clamped pitch
/// delta applied to `GameActor.field_70` / `field_78`; `dist` is the
/// XZ length of `delta`.
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

/// Argument for `func_80105070`. `field_0` / `field_4` / `field_8` are
/// copied onto `GameActor.field_20` / `field_24` / `field_28`.
typedef struct _GpVecArg {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
} GpVecArg;
STATIC_ASSERT_SIZEOF(GpVecArg, 0xC);

/// Optional companion for `func_80105070`. `field_0` / `field_4` are copied
/// onto `GameActor.field_93C` / `field_93E`. NULL zeros both halfwords.
typedef struct _GpOverrideArg {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ u16  field_4;
} GpOverrideArg;
STATIC_ASSERT_SIZEOF(GpOverrideArg, 6);

/// Argument for `func_80104F5C`. `field_0` (low 16 bits) is copied onto
/// `GameActor.field_80`; `field_4` is copied onto `GameActor.field_82`.
/// Nonzero `field_0` selects anim 0x25, else 0x24, via `func_8010397C`.
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
/// an index into `D_80112D6C` / `D_80113368` and only reinstall when that
/// pointer differs. `field_8 == 0` runs `func_801038F8` (plus `func_800B3F84`
/// in `func_80104B54` / `func_80104CAC`, and in `func_80104508` /
/// `func_8010C4F0` only on a table miss); otherwise
/// `func_80103A18(..., field_4, 0 or 1, field_C)`. `field_10` selects
/// `field_983` (7 if nonzero, `0x38` if zero).
typedef struct _GpAnimArg {
    /* 0x00 */ void* field_0;
    /* 0x04 */ s32   field_4;
    /* 0x08 */ s32   field_8;
    /* 0x0C */ s32   field_C;
    /* 0x10 */ s32   field_10;
} GpAnimArg;
STATIC_ASSERT_SIZEOF(GpAnimArg, 0x14);

/// Animation data pointed to by `D_80112D6C` / `D_80113368` /
/// `GameActor.field_928`. `func_80105914` and `func_8010C858` copy up to
/// 0x20 words onto `field_BC`.
typedef struct _GpAnimBlk {
    /* 0x00 */ byte pad_0[0xBC];
    /* 0xBC */ s32  field_BC[0x20];
} GpAnimBlk;

/// Word-copy argument for `func_80105914` / `func_8010C858`. `field_0` is
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
/// work (set/cleared by `func_80100B78` / `func_80101408`). Walked as a
/// pair by `func_800DB0D8`, `func_800DAB38`, `func_800DAC54`, and
/// `func_800DACF8`. `func_800DACAC` assigns `field_90C` on slot 0 only.
extern GpActorWork* volatile D_80115760[2];

/// Flag byte cleared by `func_800A7DE0` / `func_801036FC`.
extern u8 D_80115768;

/// Four-entry `Task::state` dispatcher: `func_80100B78`, `func_80100E40`,
/// `func_801013FC`, `func_80101408`.
extern TaskFuncTable4 D_80097848;

/// Four-entry `Task::state` dispatcher: `func_8010B590`, `func_8010B5C0`,
/// `func_8010B5E4`, `func_8010B5F0`.
extern TaskFuncTable4 D_80097AB0;

/// `Wip_SysConfig.field_21` dispatcher copied by `func_8010615C`. Unused
/// slots are `func_801065A0`; others are weapon-overlay entry points.
extern GpActorFuncTable33 D_800978BC;

/// `field_954` dispatcher: `func_80106838`, `func_80108E40`, `func_80108ED4`.
extern GpActorFuncTable3 D_80097940;

/// `field_956` dispatcher copied by `func_80106838`.
extern GpActorFuncTable8 D_8009794C;

/// `field_96C` dispatcher: three slots of `func_80109170`, then `func_80109208`.
extern GpActorFuncTable4 D_800979F8;

/// `field_956` dispatcher copied by `func_80108ED4`.
extern GpActorFuncTable12 D_80097A08;

/// u8 Task_Spawn type bases. `func_80104258` indexes
/// `D_80112DFC[arg2 + Wip_SysConfig.field_26 - 2]`.
extern u8 D_80112DFC[];

/// Pad-event templates for `func_801041FC` (`D_80112E28[arg1 & 0xFFFF]`).
extern GpPadEvt D_80112E28[];

/// 2-wide rows indexed by `Mc_SaveData.field_22`. `func_80108224` passes
/// `D_80112E04[field_22][1]` to `func_80105894`.
extern u8 D_80112E04[][2];

/// 2-wide rows of `GsCOORDINATE2` indices. `func_8010403C` indexes
/// `D_80112E2C[Mc_SaveData.field_22 - 1][arg0]`.
extern u8 D_80112E2C[][2];

/// NULL-terminated `GpImgRec*` lists for `func_801030CC`. Indexed as
/// `table[type * 4 + Wip_SysConfig.field_26 - 5][frame]`. `D_80112E74` is
/// the `field_987` sequence; `D_80112EB4` is the `field_98A` sequence.
extern struct _GpImgRec** D_80112E74[];
extern struct _GpImgRec** D_80112EB4[];

/// 2-wide rows indexed by `Wip_SysConfig.field_21`. Zero at `[i][0]`
/// makes `func_801088D4` abort the item-use path (`field_95E = 0x3E8`).
extern u8 D_80112F1C[][2];

/// u16 table indexed by `func_80100FCC` arg1 and added onto
/// `GpActorD4Rec.field_C` when filling `field_4`.
extern u16 D_80112F60[];

/// 0x10-byte `VECTOR` rows indexed by `func_80100FCC` arg1. Copied through
/// scratch; the low 16 bits of `vx`/`vy`/`vz` seed `GpActorD4Rec.field_8` /
/// `field_A` / `field_C`.
extern VECTOR D_80112FA4[];

/// Overlay-imported s16 table indexed by `Mc_SaveData.field_5C7` and passed
/// to `func_80106350` (`func_8010C46C` / `func_8010C4F0` / `func_8010C75C`).
extern s16 D_80167218[];

/// Overlay-imported s16 table indexed by `Mc_SaveData.field_5C7` and passed
/// as the third argument of `func_80100FCC` (`func_8010B79C`).
extern s16 D_80167224[];

/// Overlay-imported u8 table indexed by `Mc_SaveData.field_5C7` and stored
/// at `GpActorD4.field_CD` (`func_8010B79C`).
extern u8 D_80167230[];

/// 8-byte `GpAimRot` rows copied onto `GpPitchScratch.rot`.
extern GpAimRot D_801131B4[];

/// u8 table indexed by `Mc_SaveData.field_5C7`. Non-zero selects
/// `func_80102634`; zero uses `D_80167218` with `func_80102D20`.
extern u8 D_80113388[];

void func_800F5184(Task* arg0);
void func_800F52B4(struct _GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u16 arg3);
void func_800F75BC(Task* arg0);
void func_800F7AD4(struct _GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u16 arg3);
void func_800F9474(Task* arg0);
void func_800FA7CC(Task* arg0);
void func_800FB67C(Task* arg0);
void func_800FB7E4(Task* arg0);
void func_800FBEBC(Task* arg0);
void func_800FC500(Task* arg0);
void func_800FC6C0(void);
void func_800FC74C(Task* arg0);
void func_800FE41C(Task* arg0);
void func_801005D8(Task* arg0);
void func_80100784(struct _GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
s32  func_801011D0(struct _GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32* arg3);
void func_80100FCC(GpActorWork* arg0, s32 arg1, s32 arg2);
void func_80101408(GpActorWork* arg0);
void func_801030CC(GpActorWork* arg0);
void func_801041FC(GpActorWork* arg0, s32 arg1);
s32  func_801034C0(void);
s32  func_8010B79C(void);
void func_80106350(GpActorWork* arg0, s32 arg1, s32 arg2);
void func_801088D4(GpActorWork* arg0, s32 arg1, s32 arg2);
/// Overlay import. `func_801088D4` calls it with `Game_GetPtrSlot(0xA)` when
/// `Mc_SaveData.field_13 == 1`.
void func_80166E94(void* arg0, s32 arg1);
void func_80109170(GpActorWork* arg0);
s32  func_80109290(GpActorWork* arg0);
void func_8010A1B0(s32 arg0, s32 arg1);
void func_8010A42C(GpActorWork* arg0, s32 arg1);
void func_80103B5C(GpActorWork* arg0);
s32  func_80103B88(GpActorWork* arg0, GpDirArg* arg1);
s32  func_80104E00(GpActorWork* arg0, s32 arg1, GpXformArg* arg2);
s32  func_80105070(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3);
s32  func_801053A0(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2);
void func_8010C81C(GpActorWork* arg0, s32 arg1, GpMoveArg* arg2);
void func_8010B210(GpActorWork* arg0);
void func_8010C1FC(GpActorWork* arg0, SVECTOR3* arg1, s32 arg2);
s32  func_8010C30C(GpActorWork* arg0);
Task* func_801036FC(GpActorArg* arg0, u16 arg1, s32 arg2, GpActorFlags* arg3);
Task* func_8010BAC8(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3);

#endif // GAMEPLAY_3FB8_H
