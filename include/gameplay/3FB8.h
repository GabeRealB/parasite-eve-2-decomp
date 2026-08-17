#ifndef GAMEPLAY_3FB8_H
#define GAMEPLAY_3FB8_H

#include "common.h"

#include "main/session.h"
#include "main/task.h"

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

/// 4-entry callback table copied onto the stack by `func_80108E40`.
typedef struct {
    GpActorFunc funcs[4];
} GpActorFuncTable4;

/// 12-entry callback table copied onto the stack by `func_80108ED4`.
typedef struct {
    GpActorFunc funcs[12];
} GpActorFuncTable12;

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
typedef struct _GpActorD4 {
    /* 0x00 */ byte         pad_0[0x18];
    /* 0x18 */ byte         field_18[0x50]; // GsCOORDINATE2
    /* 0x68 */ byte         field_68[0x20]; // GpObj
    /* 0x88 */ GpActorD4Rec field_88;
    /* 0xA0 */ GpRec18      field_A0;
    /* 0xB8 */ byte         pad_B8[0xC];
    /* 0xC4 */ s16          field_C4;
    /* 0xC6 */ byte         pad_C6[0xE];
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
    /* 0x46 */ byte pad_46[6];
    /* 0x4C */ s32* sub;
} GpCoordExt;
STATIC_ASSERT_SIZEOF(GpCoordExt, 0x50);

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

/// 0x2C-byte work at `Task::spawnArg2` for `func_800F1A9C` / `func_800F5184` /
/// `func_800F75BC` / `func_800F77F8` / `func_800FE41C` (`Mem_Calloc(0x2C)` in
/// `func_800EA478`). `field_8` is the parent coordinate copied onto
/// `GsCOORDINATE2.sub`. `field_10` is the 3-halfword overlay `func_800FE41C`
/// passes to `func_800EA478`. `field_18` / `field_1A` / `field_1C` are
/// sign-extended into `coord.t[]` on first run. `field_22` is the step
/// counter (`func_800F1A9C` / `func_800F5184`). `field_24` is the current
/// scale stepped toward `field_26` (`func_800F75BC`), the LCG angle
/// (`func_800F1A9C`), or a 0x10 start that decays by 2 (`func_800F5184`).
/// `func_800FE41C` copies `spawnArg1` into `field_24` / `field_26` and sets
/// `field_28 = field_26 << 2`. `func_800F5184` inits `field_26` to 0x20 and
/// adds `field_2A` each frame. `field_2A` is the packed parameter passed
/// through to `func_800F7AD4`, or the per-frame `field_26` step.
typedef struct _GpEffWork {
    /* 0x00 */ byte                  pad_0[8];
    /* 0x08 */ struct _GsCOORDINATE2* field_8;
    /* 0x0C */ byte                  pad_C[4];
    /* 0x10 */ s16                   field_10;
    /* 0x12 */ s16                   field_12;
    /* 0x14 */ s16                   field_14;
    /* 0x16 */ s16                   pad_16;
    /* 0x18 */ s16                   field_18;
    /* 0x1A */ s16                   field_1A;
    /* 0x1C */ s16                   field_1C;
    /* 0x1E */ byte                  pad_1E[4];
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

/// Overlay of `Task::spawnArg1` for `func_800F75BC`. `field_0 & 0xFFF` is
/// the target scale; `field_2 & 0xF` is the draw parameter. The parent
/// word's `0x20000000` / `0x10000000` bits pick the start state.
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

/// 0x14-byte argument for `func_80104CAC`. `field_0` is copied to
/// `GameActor.field_928`. `field_8 == 0` runs `func_800B3F84` +
/// `func_801038F8`; otherwise `func_80103A18(..., field_4, 0, field_C)`.
/// `field_10` selects `field_983` (7 if nonzero, `0x38` if zero).
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

/// Position + rotation argument for `func_80104D68`. `field_0` / `field_4` /
/// `field_8` are copied onto `GsCOORDINATE2.coord.t`; `field_10` / `field_12` /
/// `field_14` are copied onto `GameActor` 0x50 and passed to `RotMatrix`.
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

/// `field_954` dispatcher: `func_80106838`, `func_80108E40`, `func_80108ED4`.
extern GpActorFuncTable3 D_80097940;

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

/// 2-wide rows indexed by `Wip_SysConfig.field_21`. Zero at `[i][0]`
/// makes `func_801088D4` abort the item-use path (`field_95E = 0x3E8`).
extern u8 D_80112F1C[][2];

/// Overlay-imported s16 table indexed by `Mc_SaveData.field_5C7` and passed
/// to `func_80106350` (`func_8010C46C` / `func_8010C4F0` / `func_8010C75C`).
extern s16 D_80167218[];

/// u8 table indexed by `Mc_SaveData.field_5C7`. Non-zero selects
/// `func_80102634`; zero uses `D_80167218` with `func_80102D20`.
extern u8 D_80113388[];

void func_800F5184(Task* arg0);
void func_800F52B4(struct _GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u16 arg3);
void func_800F75BC(Task* arg0);
void func_800F7AD4(struct _GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u16 arg3);
void func_800FE41C(Task* arg0);
void func_800FC6C0(void);
void func_80101408(GpActorWork* arg0);
void func_801041FC(GpActorWork* arg0, s32 arg1);
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
void func_8010B210(GpActorWork* arg0);
void func_8010C1FC(GpActorWork* arg0, SVECTOR3* arg1, s32 arg2);
Task* func_801036FC(GpActorArg* arg0, u16 arg1, s32 arg2, GpActorFlags* arg3);
Task* func_8010BAC8(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3);

#endif // GAMEPLAY_3FB8_H
