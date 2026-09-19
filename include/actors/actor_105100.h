#ifndef ACTOR_105100_H
#define ACTOR_105100_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"

/// The model object in `Actor105100::field_2C` (`Task::extra`), seen through
/// this overlay: `field_8` is the object's trailing `GsCOORDINATE2` array and
/// `field_1C` / `field_20` the light and colour matrices the spawn hands the
/// two `MATRIX`es inside the work block.
typedef struct Actor105100Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
    /* 0x0E */ byte           pad_E[0xE];
    /* 0x1C */ MATRIX*        field_1C;
    /* 0x20 */ MATRIX*        field_20;
} Actor105100Obj2C;

/// Animation view of the work block's prefix. `func_800B3F84` is handed the
/// block as a `GpAnimCtx` and the nineteen `GpAnimSlot`s live at 0x14, over the
/// bytes the list units see as `Actor105100Work::obj0` / `obj38`: the handler
/// reaches slot 1 as `&work->obj38.prev`.
typedef struct Actor105100Anim {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[0x13];
} Actor105100Anim;
STATIC_ASSERT_SIZEOF(Actor105100Anim, 0x30C);

/// The actor's animation work area. `field_58E` is the pose the animation
/// tables are indexed by and `field_598` the step of the schedule that drives
/// it. `field_592` is unsigned in this overlay's view -- the accumulation in
/// `func_actor_105100_80136408` loads it `lhu` and adds with `addu` -- so the
/// signed compares against it in `func_actor_105100_801360AC` and
/// `func_actor_105100_801361C4` cast at the use (`(s16)work->field_592`)
/// instead of retyping the field.
///
/// The three objects at 0x47C / 0x4E4 / 0x51C are `GpObj` collision bodies,
/// each with the `GpRec18` run that follows it as its table: the first hangs
/// off `&coord[3]`, the second off the model's own coordinate and the third
/// off the third-party model's.
typedef struct Actor105100Work {
    /* 0x000 */ GpObj          obj0;
    /* 0x020 */ byte           pad_20[4];
    /* 0x024 */ s32            field_24;
    /* 0x028 */ byte           pad_28[0x10];
    /* 0x038 */ GpObj          obj38;
    /* 0x058 */ byte           pad_58[0x2B4];
    /* 0x30C */ byte           field_30C[0x130];
    /* 0x43C */ MATRIX         field_43C;
    /* 0x45C */ MATRIX         field_45C;
    /* 0x47C */ GpObj          obj47C;
    /* 0x49C */ GpRec18        field_49C[3];
    /* 0x4E4 */ GpObj          obj4E4;
    /* 0x504 */ GpRec18        field_504[1];
    /* 0x51C */ GpObj          obj51C;
    /* 0x53C */ GpRec18        field_53C[1];
    /* 0x554 */ GsCOORDINATE2* field_554;
    /* 0x558 */ u16            field_558;
    /* 0x55A */ u16            field_55A;
    /* 0x55C */ GpEffWork*     field_55C;
    /* 0x560 */ MATRIX         field_560;
    /* 0x580 */ s32            field_580;
    /* 0x584 */ s32            field_584;
    /* 0x588 */ s32            field_588;
    /* 0x58C */ byte           pad_58C[2];
    /* 0x58E */ u16            field_58E;
    /* 0x590 */ s16            field_590;
    /* 0x592 */ u16            field_592;
    /* 0x594 */ s16            field_594;
    /* 0x596 */ s16            field_596;
    /* 0x598 */ s16            field_598;
    /* 0x59A */ u16            field_59A;
    /* 0x59C */ u16            field_59C;
    /* 0x59E */ u16            field_59E;
    /* 0x5A0 */ byte           pad_5A0[2];
    /* 0x5A2 */ s16            field_5A2;
    /* 0x5A4 */ byte           pad_5A4[4];
    /* 0x5A8 */ s16            field_5A8;
    /* 0x5AA */ s16            field_5AA;
    /* 0x5AC */ s16            field_5AC;
    /* 0x5AE */ u16            field_5AE;
    /* 0x5B0 */ byte           pad_5B0[2];
    /* 0x5B2 */ s16            field_5B2;
    /* 0x5B4 */ s16            field_5B4;
    /* 0x5B6 */ s16            field_5B6;
    /* 0x5B8 */ u16            field_5B8;
    /* 0x5BA */ byte           pad_5BA[2];
    /* 0x5BC */ s16            field_5BC;
    /* 0x5BE */ byte           pad_5BE[4];
    /* 0x5C2 */ s16            field_5C2;
    /* 0x5C4 */ byte           pad_5C4[4];
} Actor105100Work;

/// Second view of the work area's 0x38 record, held by the per-frame handler
/// `func_actor_105100_801354E8`: `field_40` is the reaction it dispatches on,
/// `field_48` the countdown that reaction runs for and `field_4E` the pose the
/// schedule is stepped through. Those bytes are `obj38`, a `GpObj`, to the
/// list units, so the handler casts the work pointer to this view rather than
/// reaching them through `Actor105100Work`.
///
/// The reaction sub-handlers keep their own state in the same record:
/// `field_44` is the approach point `Actor105100Work`'s `obj38` vector is
/// aimed at and `field_46` the pass they are on -- 0 builds that aim, 1 walks
/// the coordinate along it. A `GpObj` cannot carry either, `field_44` sitting
/// over its `ctx.recs` collision pointer, which is why
/// `func_actor_105100_801359B4` reads them here and takes the aim vector
/// itself from `Actor105100Work::obj38`.
typedef struct Actor105100Rec {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ s16  field_40;
    /* 0x42 */ byte pad_42[2];
    /* 0x44 */ s16  field_44;
    /* 0x46 */ s16  field_46;
    /* 0x48 */ s16  field_48;
    /* 0x4A */ byte pad_4A[4];
    /* 0x4E */ u16  field_4E;
} Actor105100Rec;

/// Third view of the work area, held by the schedule entry
/// `func_actor_105100_8013329C`: the `field_5A8` / `field_5AA` pair taken as
/// one word. Every other handler reads the halves apart, so they are `s16`
/// fields of `Actor105100Work`; this entry gates on both at once (a single
/// `lw` at 0x5A8) and therefore reaches the pair through this view.
typedef struct Actor105100Gate {
    /* 0x000 */ byte pad_0[0x5A8];
    /* 0x5A8 */ s32  field_5A8;
} Actor105100Gate;
STATIC_ASSERT_SIZEOF(Actor105100Gate, 0x5AC);

/// List entry at +0x10 of `Actor105100Ctx`, linked by the state-0 setup.
/// `field_4` is the flag byte previously named `field_14` (`sb` at 0x14).
typedef struct Actor105100Node {
    /* 0x0 */ struct Actor105100Node* next;
    /* 0x4 */ u8                      field_4;
    /* 0x5 */ byte                    pad_5[3];
} Actor105100Node;
STATIC_ASSERT_SIZEOF(Actor105100Node, 0x8);

/// Task context (`Task::spawnArg2`), which is also `Actor105100::field_20`.
/// `field_8` carries the actor id in bits 12+ (the sound event ids are
/// `(field_8 >> 12) << 8 | 0x40330000`), `field_40` is HP and `field_4C` the
/// state flags the per-frame handlers test. `field_40` is unsigned in this
/// overlay's view: the heal in `func_actor_105100_80135FCC` and the damage in
/// `func_actor_105100_80135E54` both load it unsigned.
typedef struct Actor105100Ctx {
    /* 0x00 */ byte            pad_0[4];
    /* 0x04 */ MATRIX*         field_4;
    /* 0x08 */ u16             field_8;
    /* 0x0A */ byte            pad_A[6];
    /* 0x10 */ Actor105100Node node;
    /* 0x18 */ GsCOORDINATE2*  field_18;
    /* 0x1C */ s32             field_1C;
    /* 0x20 */ s32             field_20;
    /* 0x24 */ s32             field_24;
    /* 0x28 */ byte            pad_28[0x14];
    /* 0x3C */ byte            pad_3C[4];
    /* 0x40 */ u16             field_40;
    /* 0x42 */ byte            pad_42[6];
    /* 0x48 */ u8              field_48;
    /* 0x49 */ byte            pad_49[3];
    /* 0x4C */ u8              field_4C;
    /* 0x4D */ byte            pad_4D[3];
    /* 0x50 */ void*           field_50;
    /* 0x54 */ s32             field_54;
} Actor105100Ctx;
STATIC_ASSERT_SIZEOF(Actor105100Ctx, 0x58);

/// The enemy's task, the same `Task` layout as `actor_444000`'s: `field_20` is
/// the `Task::spawnArg2` context the dispatchers hand their handlers, `field_2C`
/// the `Task::extra` slot and `state` the `Task::state` dispatcher index.
/// `func_actor_105100_80136318` is the one writer of `state` here, holding the
/// task on handler 2 for as long as the fight lasts.
typedef struct Actor105100 {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Task*             parent;
    /* 0x0C */ byte              pad_C[0x10];
    /* 0x1C */ Actor105100Work*  field_1C;
    /* 0x20 */ Actor105100Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor105100Obj2C* field_2C;
    /* 0x30 */ s32               state;
} Actor105100;

/// The gameplay LCG the reroll steps draw from, `state = state * 5 + 0x71357911`.
/// Unsigned here for the same reason as `Gp_LcgState` elsewhere: the draws are
/// logical shifts of the high half (`srl`), which a signed declaration would
/// turn into an arithmetic one.
extern u32 Gp_LcgState;

void func_actor_105100_80132AA0(Actor105100Ctx* arg0, Actor105100* arg1);

#endif
