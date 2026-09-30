#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b6_training_room.h"

/// Main-executable counter whose lowest bit the flicker alternates on.

/// 0x30-byte scratch `func_actor_105100_80132C2C` takes from the scratch stack:
/// `delta` is the player offset whose length feeds `Gp_ComputeDamage`, and
/// `ofs` is the spark offset handed to `Gp_SpawnEff`.
typedef struct Actor105100HitScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ byte    pad_10[0x10];
    /* 0x20 */ SVECTOR ofs;
    /* 0x28 */ byte    pad_28[8];
} Actor105100HitScratch;
STATIC_ASSERT_SIZEOF(Actor105100HitScratch, 0x30);

/// The work block of the glowing projectile this overlay spawns as a second
/// enemy task, the 0x80 bytes its spawn handler asks `memCalloc` for. `obj0`
/// is the body the collision lists carry and `rec20` the contact record whose
/// key ends the flight; `obj38` is the second body, unlinked beside the first
/// when the task is destroyed. This child allocation is distinct from the
/// parent's animation work block and the smaller reaction task's work.
///
/// `field_70` accumulates the per-axis jitter the hover step applies to the
/// coordinate, and the step holds that accumulation inside a fixed bound.
/// `field_78` counts frames within the current step and `field_7A` selects
/// it, `field_7C` is the speed the flight doubles each frame up to a cap, and
/// `field_7E` is the size the billboard is drawn at.
typedef struct Actor105100ProjWork {
    /* 0x00 */ WorldCollisionBody    obj0;
    /* 0x20 */ WorldCollisionContact rec20;
    /* 0x38 */ WorldCollisionBody    obj38;
    /* 0x58 */ GpActorD4Rec          pose;
    /* 0x70 */ SVECTOR               field_70;
    /* 0x78 */ u16                   field_78;
    /* 0x7A */ s16                   field_7A;
    /* 0x7C */ u16                   field_7C;
    /* 0x7E */ s16                   field_7E;
} Actor105100ProjWork;
STATIC_ASSERT_SIZEOF(Actor105100ProjWork, 0x80);

/// 0x38-byte scratch the projectile's per-frame handler takes from
/// the scratch stack: `rot` is the jitter offset it adds to the coordinate and,
/// in the launch step, the rotation `RotMatrix` turns into `mat` before the
/// GTE multiplies it into the coordinate; `vec` is the offset to the player
/// the aiming step orients along. The size is pinned by the handler, which
/// claims and releases the block by decrementing and incrementing the scratch
/// head a whole element at a time.
typedef struct Actor105100ProjScratch {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ VECTOR  vec;
    /* 0x30 */ SVECTOR rot;
} Actor105100ProjScratch;
STATIC_ASSERT_SIZEOF(Actor105100ProjScratch, 0x38);

/// The actor's animation work area. `field_58E` is the pose the animation
/// tables are indexed by and `field_598` the step of the schedule that drives
/// it. `field_592` is unsigned in this overlay's view -- the accumulation in
/// `func_actor_105100_80136408` loads it `lhu` and adds with `addu` -- so the
/// signed compares against it in `func_actor_105100_801360AC` and
/// `func_actor_105100_801361C4` cast at the use (`(s16)work->field_592`)
/// instead of retyping the field.
///
/// The three objects at 0x47C / 0x4E4 / 0x51C are `WorldCollisionBody` collision bodies,
/// each with the `WorldCollisionContact` run that follows it as its table: the first hangs
/// off `&coord[3]`, the second off the model's own coordinate and the third
/// off the third-party model's.
typedef struct Actor105100Work {
    /* 0x000 */ GpAnimCtx             anim;
    /* 0x014 */ AnimationSlot         slots[19];
    /* 0x30C */ byte                  field_30C[0x130];
    /* 0x43C */ MATRIX                field_43C;
    /* 0x45C */ MATRIX                field_45C;
    /* 0x47C */ WorldCollisionBody    obj47C;
    /* 0x49C */ WorldCollisionContact field_49C[3];
    /* 0x4E4 */ WorldCollisionBody    obj4E4;
    /* 0x504 */ WorldCollisionContact field_504[1];
    /* 0x51C */ WorldCollisionBody    obj51C;
    /* 0x53C */ WorldCollisionContact field_53C[1];
    /* 0x554 */ GpEffArg              field_554; // record the death effect is spawned with
    /* 0x55C */ GpEffWork*            field_55C;
    /* 0x560 */ MATRIX                field_560;
    /* 0x580 */ s32                   field_580;
    /* 0x584 */ s32                   field_584;
    /* 0x588 */ s32                   field_588;
    /* 0x58C */ s16                   field_58C; // hit cooldown; armed from `Gp_GetIdParam2` of the hitting record
    /* 0x58E */ u16                   field_58E;
    /* 0x590 */ s16                   field_590;
    /* 0x592 */ u16                   field_592;
    /* 0x594 */ s16                   field_594;
    /* 0x596 */ s16                   field_596;
    /* 0x598 */ s16                   field_598;
    /* 0x59A */ u16                   field_59A;
    /* 0x59C */ u16                   field_59C;
    /* 0x59E */ u16                   field_59E;
    /* 0x5A0 */ s16                   field_5A0; // sign of the player offset dotted with the player's facing axis
    /* 0x5A2 */ s16                   field_5A2; // non-zero while the attack body is running; the body clears it when it finishes
    /* 0x5A4 */ s16                   field_5A4; // state of the attack body `func_actor_105100_80133CE4`
    /* 0x5A6 */ u16                   field_5A6; // its frame counter
    /* 0x5A8 */ s16                   field_5A8;
    /* 0x5AA */ s16                   field_5AA;
    /* 0x5AC */ s16                   field_5AC;
    /* 0x5AE */ u16                   field_5AE;
    /* 0x5B0 */ s16                   field_5B0;
    /* 0x5B2 */ s16                   field_5B2;
    /* 0x5B4 */ s16                   field_5B4;
    /* 0x5B6 */ s16                   field_5B6;
    /* 0x5B8 */ u16                   field_5B8;
    /* 0x5BA */ s16                   field_5BA; // 1 while the death cutscene message is pending; cleared after 0x13F4, gates step 3
    /* 0x5BC */ s16                   field_5BC;
    /* 0x5BE */ u16                   field_5BE; // accumulated damage toward the 0x1A4 stagger threshold
    /* 0x5C0 */ u16                   field_5C0; // frames the stagger window stays open; loaded 0xBC on a hit
    /* 0x5C2 */ s16                   field_5C2;
} Actor105100Work;
STATIC_ASSERT_SIZEOF(Actor105100Work, 0x5C4);

/// The child spawner allocates this 0x50-byte collision and reaction block:
/// a WorldCollisionBody, one contact record, and the state the reaction handlers drive.
/// `func_actor_105100_801354E8` dispatches on `field_40`, decrements the
/// `field_48` countdown and chooses the displayed pose in `field_4E`.
/// The movement handlers aim `direction` at approach point `field_44`;
/// `field_46` selects their phase, `travelTicks` counts down the first leg,
/// and `step` is the distance advanced per frame. Only the leading `obj`
/// participates in collision; the tail is movement state, not another body.
typedef struct Actor105100Rec {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[1];
    /* 0x38 */ SVECTOR               direction;
    /* 0x40 */ s16                   field_40;
    /* 0x42 */ s16                   field_42;
    /* 0x44 */ s16                   field_44;
    /* 0x46 */ s16                   field_46;
    /* 0x48 */ s16                   field_48;
    /* 0x4A */ s16                   travelTicks;
    /* 0x4C */ s16                   step;
    /* 0x4E */ u16                   field_4E;
} Actor105100Rec;
STATIC_ASSERT_SIZEOF(Actor105100Rec, 0x50);

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

static void func_actor_105100_80132414(GfxCoord* arg0, s32 arg1);
static void func_actor_105100_801327B4(GpEnemy* arg0, Task* arg1);
static void func_actor_105100_80132AA0(GpEnemy* arg0, Task* arg1);
static void func_actor_105100_80132C2C(Task* arg0);
static void func_actor_105100_80133134(Task* arg0);
static void func_actor_105100_8013329C(Task* arg0, GpEnemy* arg1);
static void func_actor_105100_8013345C(Task* arg0, GpEnemy* arg1);
static void func_actor_105100_801336B8(Task* arg0, GpEnemy* arg1);
static void func_actor_105100_80133A14(Task* arg0, GpEnemy* arg1);
static void func_actor_105100_80133CE4(Task* arg0);
static void func_actor_105100_80134130(Task* arg0);
static void func_actor_105100_80134284(GpEnemy* arg0, Task* arg1);
static void func_actor_105100_801347D4(GpEnemy* arg0, Task* arg1);
static void func_actor_105100_80134B00(GpEnemy* arg0, Task* arg1);
static void func_actor_105100_80135278(GpEnemy* arg0, Task* arg1);
static void func_actor_105100_801354E8(GpEnemy* arg0, Task* arg1);
static void func_actor_105100_80135674(Task* arg0);
static void func_actor_105100_801359B4(Task* arg0);
static void func_actor_105100_80135B40(Task* arg0);
static void func_actor_105100_80135E54(Task* arg0);
static void func_actor_105100_80135F50(Task* arg0);
static void func_actor_105100_80135FCC(Task* arg0);
static void func_actor_105100_801360AC(Task* arg0);
static void func_actor_105100_801361C4(Task* arg0);
static void func_actor_105100_801362A0(Task* arg0);
static void func_actor_105100_80136318(Task* arg0);
static void func_actor_105100_80136408(Task* arg0);
static void func_actor_105100_801364CC(Task* arg0);
static void func_actor_105100_80136524(Task* arg0);
static void func_actor_105100_80136574(Task* arg0, MATRIX* arg1, s16 arg2, s32 arg3);
static void func_actor_105100_801366D8(GpEnemy* arg0, Task* arg1);
static void func_actor_105100_80136788(GpEnemy* arg0, Task* arg1);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Main-executable globals with no module header yet: a `Gp_StateC08.field_A` of 1 or a
/// live `gDisplayState.pendingMode` means a cutscene is already up, so the death handler skips
/// message 0x13F4.

/// Main-executable global with no module header yet: the remaining-enemy count
/// `func_actor_105100_80136318` tests to decide whether the fight is over.

/// The run of HP caps at 0x8014139C; `func_actor_105100_80135FCC` reads the
/// first entry. Declared as an aggregate on purpose: a bare `extern u16` makes
/// `true_dependence` (`sched.c:846`) drop the dependence between the store to
/// `GpEnemy::hp` and this load -- the store is in-struct with a
/// varying address, this load a scalar MEM at a fixed one -- and sched2 then
/// hoists this load above the store, ahead of the `sll`.

/// The s16 animation-id run at 0x801414C8, one entry per work state at
/// `Actor105100Work::field_58E`; `func_actor_105100_80136408` reads the entry
/// the new state selects before it re-queues every slot.
extern s16 D_actor_105100_801414C8[];

/// The spawn's pair tables. `Gp_PackPair` packs the `GpU16Pair` at 0x80141380
/// into the work's third list node (`Actor105100Work::obj4E4.key`), and the
/// `GpPairSrcE` at 0x80141398 is the pair source the context points at with
/// `GpEnemy::param` -- its `hpMax` seeds the enemy's HP.
extern GpU16Pair  D_actor_105100_80141380[6];
extern GpPairSrcE D_actor_105100_80141398;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_105100_80141488[];

/// The approach points the `field_40 == 1` reaction walks the model through,
/// indexed by `Actor105100Rec::field_44`. Only the x and z halves are read: the
/// reaction subtracts the model's current position and walks the resulting
/// planar delta.
extern SVECTOR D_actor_105100_80141418[6];

extern SVECTOR D_actor_105100_801413E8[];
extern s16     D_actor_105100_80141448[];
extern s16     D_actor_105100_80141450[];

/// Where each spawned projectile starts relative to the parent's coordinate,
/// indexed by `Actor105100Work::field_5AE`.
extern SVECTOR D_actor_105100_801414E0[];

/// The enemy task's state handlers, indexed by `Task::state`: spawn/setup,
/// per-frame tick and teardown.
static const GpEnemyTaskFuncTable3 D_actor_105100_80131E24 = {
    {
        func_actor_105100_801327B4,
        func_actor_105100_80132AA0,
        func_actor_105100_80134284,
    },
};

extern TmdSource D_actor_105100_8013B19C;
void             func_actor_105100_80135DF8(Task*);

extern AnimationSet D_actor_105100_8013FB9C;
extern AnimationSet D_actor_105100_80140338;
extern AnimationSet D_actor_105100_80140B44;
extern AnimationSet D_actor_105100_80141358;

TmdBone D_actor_105100_801367CC[19] = {
#include "assets/actor_105100_model_0937C_skeleton.inc"
};

u32 D_actor_105100_80136A78[19] = {
#include "assets/actor_105100_model_0937C_partVerts.inc"
};

SVECTOR D_actor_105100_80136AC4[250] = {
#include "assets/actor_105100_model_0937C_verts.inc"
};

SVECTOR D_actor_105100_80137294[257] = {
#include "assets/actor_105100_model_0937C_normals.inc"
};

u32 D_actor_105100_80137A9C[3520] = {
#include "assets/actor_105100_model_0937C_stream.inc"
};

TmdSource D_actor_105100_8013B19C = {
    0,
    17784,
    6072,
    19,
    D_actor_105100_80136A78,
    D_actor_105100_80136AC4,
    D_actor_105100_80137294,
    D_actor_105100_801367CC,
    D_actor_105100_80137A9C,
};

AnimationPackedPose D_actor_105100_8013B1C0[10] = {
#include "assets/actor_105100_animation_09B8C_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013B238[167] = {
#include "assets/actor_105100_animation_09B8C_bank4.inc"
};

AnimationRecord D_actor_105100_8013B4D4[300] = {
#include "assets/actor_105100_animation_09B8C_records.inc"
};

u16 D_actor_105100_8013B984[20] = {
#include "assets/actor_105100_animation_09B8C_indices.inc"
};

AnimationSet D_actor_105100_8013B9AC = {
    D_actor_105100_8013B4D4,
    D_actor_105100_8013B984,
    { NULL, D_actor_105100_8013B1C0, NULL, NULL, D_actor_105100_8013B238, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013B9D4[14] = {
#include "assets/actor_105100_animation_0A30C_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013BA7C[170] = {
#include "assets/actor_105100_animation_0A30C_bank4.inc"
};

AnimationRecord D_actor_105100_8013BD24[248] = {
#include "assets/actor_105100_animation_0A30C_records.inc"
};

u16 D_actor_105100_8013C104[20] = {
#include "assets/actor_105100_animation_0A30C_indices.inc"
};

AnimationSet D_actor_105100_8013C12C = {
    D_actor_105100_8013BD24,
    D_actor_105100_8013C104,
    { NULL, D_actor_105100_8013B9D4, NULL, NULL, D_actor_105100_8013BA7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013C154[16] = {
#include "assets/actor_105100_animation_0AFDC_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013C214[298] = {
#include "assets/actor_105100_animation_0AFDC_bank4.inc"
};

AnimationRecord D_actor_105100_8013C6BC[454] = {
#include "assets/actor_105100_animation_0AFDC_records.inc"
};

u16 D_actor_105100_8013CDD4[20] = {
#include "assets/actor_105100_animation_0AFDC_indices.inc"
};

AnimationSet D_actor_105100_8013CDFC = {
    D_actor_105100_8013C6BC,
    D_actor_105100_8013CDD4,
    { NULL, D_actor_105100_8013C154, NULL, NULL, D_actor_105100_8013C214, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013CE24[7] = {
#include "assets/actor_105100_animation_0B48C_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013CE78[114] = {
#include "assets/actor_105100_animation_0B48C_bank4.inc"
};

AnimationRecord D_actor_105100_8013D040[145] = {
#include "assets/actor_105100_animation_0B48C_records.inc"
};

u16 D_actor_105100_8013D284[20] = {
#include "assets/actor_105100_animation_0B48C_indices.inc"
};

AnimationSet D_actor_105100_8013D2AC = {
    D_actor_105100_8013D040,
    D_actor_105100_8013D284,
    { NULL, D_actor_105100_8013CE24, NULL, NULL, D_actor_105100_8013CE78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013D2D4[8] = {
#include "assets/actor_105100_animation_0B8C8_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013D334[97] = {
#include "assets/actor_105100_animation_0B8C8_bank4.inc"
};

AnimationRecord D_actor_105100_8013D4B8[130] = {
#include "assets/actor_105100_animation_0B8C8_records.inc"
};

u16 D_actor_105100_8013D6C0[20] = {
#include "assets/actor_105100_animation_0B8C8_indices.inc"
};

AnimationSet D_actor_105100_8013D6E8 = {
    D_actor_105100_8013D4B8,
    D_actor_105100_8013D6C0,
    { NULL, D_actor_105100_8013D2D4, NULL, NULL, D_actor_105100_8013D334, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013D710[12] = {
#include "assets/actor_105100_animation_0BFD8_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013D7A0[171] = {
#include "assets/actor_105100_animation_0BFD8_bank4.inc"
};

AnimationRecord D_actor_105100_8013DA4C[225] = {
#include "assets/actor_105100_animation_0BFD8_records.inc"
};

u16 D_actor_105100_8013DDD0[20] = {
#include "assets/actor_105100_animation_0BFD8_indices.inc"
};

AnimationSet D_actor_105100_8013DDF8 = {
    D_actor_105100_8013DA4C,
    D_actor_105100_8013DDD0,
    { NULL, D_actor_105100_8013D710, NULL, NULL, D_actor_105100_8013D7A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013DE20[7] = {
#include "assets/actor_105100_animation_0C53C_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013DE74[108] = {
#include "assets/actor_105100_animation_0C53C_bank4.inc"
};

AnimationRecord D_actor_105100_8013E024[196] = {
#include "assets/actor_105100_animation_0C53C_records.inc"
};

u16 D_actor_105100_8013E334[20] = {
#include "assets/actor_105100_animation_0C53C_indices.inc"
};

AnimationSet D_actor_105100_8013E35C = {
    D_actor_105100_8013E024,
    D_actor_105100_8013E334,
    { NULL, D_actor_105100_8013DE20, NULL, NULL, D_actor_105100_8013DE74, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013E384[4] = {
#include "assets/actor_105100_animation_0C7E4_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013E3B4[55] = {
#include "assets/actor_105100_animation_0C7E4_bank4.inc"
};

AnimationRecord D_actor_105100_8013E490[83] = {
#include "assets/actor_105100_animation_0C7E4_records.inc"
};

u16 D_actor_105100_8013E5DC[20] = {
#include "assets/actor_105100_animation_0C7E4_indices.inc"
};

AnimationSet D_actor_105100_8013E604 = {
    D_actor_105100_8013E490,
    D_actor_105100_8013E5DC,
    { NULL, D_actor_105100_8013E384, NULL, NULL, D_actor_105100_8013E3B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013E62C[7] = {
#include "assets/actor_105100_animation_0CBFC_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013E680[95] = {
#include "assets/actor_105100_animation_0CBFC_bank4.inc"
};

AnimationRecord D_actor_105100_8013E7FC[126] = {
#include "assets/actor_105100_animation_0CBFC_records.inc"
};

u16 D_actor_105100_8013E9F4[20] = {
#include "assets/actor_105100_animation_0CBFC_indices.inc"
};

AnimationSet D_actor_105100_8013EA1C = {
    D_actor_105100_8013E7FC,
    D_actor_105100_8013E9F4,
    { NULL, D_actor_105100_8013E62C, NULL, NULL, D_actor_105100_8013E680, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013EA44[17] = {
#include "assets/actor_105100_animation_0D534_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013EB10[230] = {
#include "assets/actor_105100_animation_0D534_bank4.inc"
};

AnimationRecord D_actor_105100_8013EEA8[289] = {
#include "assets/actor_105100_animation_0D534_records.inc"
};

u16 D_actor_105100_8013F32C[20] = {
#include "assets/actor_105100_animation_0D534_indices.inc"
};

AnimationSet D_actor_105100_8013F354 = {
    D_actor_105100_8013EEA8,
    D_actor_105100_8013F32C,
    { NULL, D_actor_105100_8013EA44, NULL, NULL, D_actor_105100_8013EB10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013F37C[21] = {
#include "assets/actor_105100_animation_0DD7C_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013F478[193] = {
#include "assets/actor_105100_animation_0DD7C_bank4.inc"
};

AnimationRecord D_actor_105100_8013F77C[254] = {
#include "assets/actor_105100_animation_0DD7C_records.inc"
};

u16 D_actor_105100_8013FB74[20] = {
#include "assets/actor_105100_animation_0DD7C_indices.inc"
};

AnimationSet D_actor_105100_8013FB9C = {
    D_actor_105100_8013F77C,
    D_actor_105100_8013FB74,
    { NULL, D_actor_105100_8013F37C, NULL, NULL, D_actor_105100_8013F478, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_8013FBC4[20] = {
#include "assets/actor_105100_animation_0E518_bank1.inc"
};

AnimationPackedRotation D_actor_105100_8013FCB4[172] = {
#include "assets/actor_105100_animation_0E518_bank4.inc"
};

AnimationRecord D_actor_105100_8013FF64[235] = {
#include "assets/actor_105100_animation_0E518_records.inc"
};

u16 D_actor_105100_80140310[20] = {
#include "assets/actor_105100_animation_0E518_indices.inc"
};

AnimationSet D_actor_105100_80140338 = {
    D_actor_105100_8013FF64,
    D_actor_105100_80140310,
    { NULL, D_actor_105100_8013FBC4, NULL, NULL, D_actor_105100_8013FCB4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_80140360[15] = {
#include "assets/actor_105100_animation_0ED24_bank1.inc"
};

AnimationPackedRotation D_actor_105100_80140414[206] = {
#include "assets/actor_105100_animation_0ED24_bank4.inc"
};

AnimationRecord D_actor_105100_8014074C[244] = {
#include "assets/actor_105100_animation_0ED24_records.inc"
};

u16 D_actor_105100_80140B1C[20] = {
#include "assets/actor_105100_animation_0ED24_indices.inc"
};

AnimationSet D_actor_105100_80140B44 = {
    D_actor_105100_8014074C,
    D_actor_105100_80140B1C,
    { NULL, D_actor_105100_80140360, NULL, NULL, D_actor_105100_80140414, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105100_80140B6C[14] = {
#include "assets/actor_105100_animation_0F538_bank1.inc"
};

AnimationPackedRotation D_actor_105100_80140C14[207] = {
#include "assets/actor_105100_animation_0F538_bank4.inc"
};

AnimationRecord D_actor_105100_80140F50[248] = {
#include "assets/actor_105100_animation_0F538_records.inc"
};

u16 D_actor_105100_80141330[20] = {
#include "assets/actor_105100_animation_0F538_indices.inc"
};

AnimationSet D_actor_105100_80141358 = {
    D_actor_105100_80140F50,
    D_actor_105100_80141330,
    { NULL, D_actor_105100_80140B6C, NULL, NULL, D_actor_105100_80140C14, NULL, NULL, NULL },
};

GpU16Pair D_actor_105100_80141380[6] = {
    { 32, 6 },
    { 32, 6 },
    { 25, 10 },
    { 25, 1 },
    { 25, 2 },
    { 40, 6 },
};

GpPairSrcE D_actor_105100_80141398 = { D_actor_105100_80141380, 4000, 1000, 500, 100, 200, 5, 100, 4, 0 };

u16 D_actor_105100_801413A8[16] = {
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

u16 D_actor_105100_801413C8[16] = {
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

SVECTOR D_actor_105100_801413E8[6] = {
    { -1000, -100, -1200, 0 },
    { 1000, -100, -1200, 0 },
    { 0, -100, -1200, 0 },
    { 1000, -100, -1200, 0 },
    { -1000, -100, -1200, 0 },
    { 0, -100, -1200, 0 },
};

SVECTOR D_actor_105100_80141418[6] = {
    { 0, 0, 6000, 0 },
    { 5000, 0, 6000, 0 },
    { 2500, 0, 0, 0 },
    { 5000, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 2500, 0, 0, 0 },
};

s16 D_actor_105100_80141448[4] = {
    60,
    60,
    120,
    0,
};

s16 D_actor_105100_80141450[10] = {
    0,
    1,
    -1,
    2,
    3,
    4,
    5,
    -1,
    -1,
    0,
};

void func_actor_105100_80135DF8(Task*);
void func_actor_105100_8013667C(Task*);
void func_actor_105100_8013672C(Task*);

TaskDesc D_actor_105100_80141464[3] = {
    { 1, 96, func_actor_105100_80135DF8, { .model = &D_actor_105100_8013B19C } },
    { 2, 96, func_actor_105100_8013667C, { .model = NULL } },
    { 2, 96, func_actor_105100_8013672C, { .model = NULL } },
};

u8 D_actor_105100_80141488[44] = {
    0,
    0,
    0,
    0,
    172,
    185,
    19,
    128,
    44,
    193,
    19,
    128,
    252,
    205,
    19,
    128,
    172,
    210,
    19,
    128,
    232,
    214,
    19,
    128,
    248,
    221,
    19,
    128,
    84,
    243,
    19,
    128,
    92,
    227,
    19,
    128,
    4,
    230,
    19,
    128,
    28,
    234,
    19,
    128,
};

/// Borrowed player clips for the scripted attack; entry zero is unused.
static AnimationSet* _gActor105100PlayerAnimationSets[5] = {
    NULL,
    &D_actor_105100_8013FB9C,
    &D_actor_105100_80140338,
    &D_actor_105100_80140B44,
    &D_actor_105100_80141358,
};

s16 D_actor_105100_801414C8[12] = {
    0,
    8,
    4,
    4,
    0,
    0,
    2,
    2,
    2,
    0,
    0,
    0,
};

SVECTOR D_actor_105100_801414E0[7] = {
    { 0, 0, 0, 0 },
    { 0, -4000, -2000, 0 },
    { -500, -4000, -2000, 0 },
    { 500, -4000, -2000, 0 },
    { 0, -3500, -2000, 0 },
    { -500, -3500, -2000, 0 },
    { 500, -3500, -2000, 0 },
};

/// The run of poses at 0x801413A8 this step's reroll picks from, one `s16`
/// entry per draw. Declared as an aggregate on purpose: a bare `extern u16`
/// makes `true_dependence` (`sched.c:846`) drop the dependence between the
/// entry load and the `sh` to `Actor105100Work::field_598`, and sched2 then
/// sinks that store past the `sw` of the LCG state instead of leaving the
/// lookup and the pose store adjacent at the end of the block.
extern u16 D_actor_105100_801413A8[16];

/// The enemy descriptor run at 0x80141464 the spawn below draws from. Declared
/// as a scalar rather than an aggregate on purpose: only its address is taken,
/// so the two words `Gp_SpawnEnemyFromTable` splits it into are the function's
/// addend, not a load this function has to model.
extern TaskDesc D_actor_105100_80141464[];

/// The 16-entry run at 0x801413C8 this step's LCG draw picks `field_5B0`
/// from. Four entries are 0 (two children), five are 1 (three), seven are 2
/// (one).
extern u16 D_actor_105100_801413C8[16];

static void        func_actor_105100_80131EBC(GfxCoord* coord, s16 size);
static inline void _actor105100AnimUpdate(Task* task);
static void        func_actor_105100_80135CEC(GfxCoord* arg0, s32 arg1);

/// Projects `coord` onto two `POLY_FT4` billboards, lights `Gp_RoomCoords[2]`
/// as a point light at that position, and traces the ground for the ground-quad
/// helper when `Gp_State1C->groundTraceEnabled` is set.
static void func_actor_105100_80131EBC(GfxCoord* coord, s16 size)
{
    GfxCoord       ground;
    POLY_FT4*      prim;
    s16            intensity;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* sc;

    slot                                  = &Gp_RoomCoords[2];
    slot->framesLeft                      = 2;
    light                                 = &slot->light;
    light->inner                          = 0x300;
    light->outer                          = 0x3000;
    random                                = (Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState                           = random;
    intensity                             = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r                         = intensity;
    light->head.g                         = intensity >> 1;
    light->head.b                         = intensity >> 2;
    light->head.u.at.local.t[0]           = (s32)coord->coord.t[0];
    light->head.u.at.local.t[1]           = (s32)coord->coord.t[1];
    light->head.u.at.local.t[2]           = coord->coord.t[2];
    slot->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_PUSH(GpRingScratch);
    sc         = SCRATCH_HEAD(GpRingScratch);
    sc->vec.vx = coord->workm.t[0];
    sc->vec.vy = coord->workm.t[1];
    sc->vec.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&sc->sx);
    gte_stflg(&sc->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&sc->otz);
        prim           = gGpuPrimCursor;
        sc->otz        = (s32)(sc->otz + 1);
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = sc->sx - sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = sc->sx + sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = sc->sy - sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = sc->sy + sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = sc->sx - sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = sc->sx + sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = sc->sy - sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = sc->sy + sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        if (Gp_State1C->groundTraceEnabled != 0) {
            if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                func_actor_105100_80132414(&ground, (s32)(s16)(outerSize * 2));
            }
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a flat textured quad on the ground under the actor: the corners of
/// the unit quad `D_80111E38`, scaled by `arg1` and turned into view
/// orientation, are placed around `arg0`'s world translation and projected.
/// When all four project, a semi-transparent `POLY_FT4` is queued one step
/// behind their depth, its texture alternating between two frames with the
/// display's animation frame.
static void func_actor_105100_80132414(GfxCoord* arg0, s32 arg1)
{
    OverlayGroundScratch* sc;
    POLY_FT4*             prim;
    s32                   i;
    s32                   otz;
    s32                   flag;
    s32                   u;

    sc = SCRATCH_PUSH(OverlayGroundScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        sc->vec[i].vx = D_80111E38[i].x * arg1;
        sc->vec[i].vy = 0;
        sc->vec[i].vz = D_80111E38[i].y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&sc->vec[i]);
        gte_rtv0();
        gte_stsv(&sc->vec[i]);
        sc->vec[i].vx += arg0->workm.t[0];
        sc->vec[i].vy += arg0->workm.t[1];
        sc->vec[i].vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy0);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_ldv3(&sc->vec[1], &sc->vec[2], &sc->vec[3]);
        gte_rtpt();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_stszotz(&otz);
            otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);

            prim->r0    = 0x30;
            prim->g0    = 0x20;
            prim->b0    = 0x20;
            prim->tpage = 0x28;
            prim->clut  = 0x428C;
            setSemiTrans(prim, 1);
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
            prim->v0 = 0x38;
            prim->u0 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
            prim->v1 = 0x38;
            prim->u1 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
            prim->v2 = 0x57;
            prim->u2 = u;
            u        = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
            prim->v3 = 0x57;
            prim->u3 = u;
            prim->x0 = sc->sxy0.vx;
            prim->y0 = sc->sxy0.vy;
            prim->x1 = sc->sxy1.vx;
            prim->y1 = sc->sxy1.vy;
            prim->x2 = sc->sxy2.vx;
            prim->y2 = sc->sxy2.vy;
            prim->x3 = sc->sxy3.vx;
            prim->y3 = sc->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_POP(OverlayGroundScratch);
}

/// Spawn/setup handler. It allocates the 0x5C4-byte work block and hangs it off
/// the task, points the model object at the block's two `MATRIX`es (0x45C the
/// light matrix, 0x43C the colour one) and fills the context's coordinate,
/// pair source and HP (`field_40`, seeded from the record's `hpMax`).
///
/// The block's 0x14-prefix then becomes the `GpAnimCtx`: `func_800B3F84` loads
/// the animation data into it over the nineteen `AnimationSlot`s, and slots 1..18
/// are reset. The three list nodes at 0x47C / 0x4E4 / 0x51C are linked into the
/// global object lists with their collision tables (`Gp_InitRec18Table`), which
/// also sets each node's 0x8000 "last element" flag -- then the second node's is
/// cleared again. `&coord[3]` -- the actor's fourth coordinate -- is what the
/// first node, `field_554` and the context's `field_18` all hang off.
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler; otherwise the task moves to the tick handler (`state` 1).
static void func_actor_105100_801327B4(GpEnemy* arg0, Task* arg1)
{
    Actor105100Work*       work;
    TmdObject*             obj;
    GfxCoord*              coord;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x5C4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_45C;
    obj->colorMtx       = &work->field_43C;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &arg1->extra.tmd->coords[3];
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0x64;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &D_actor_105100_80141398;
    arg0->recs                 = work->field_49C;
    arg0->hp                   = D_actor_105100_80141398.hpMax;
    work->field_554.coord      = &arg1->extra.tmd->coords[3];
    work->field_554.spawnArgLo = 0x500;
    work->field_554.spawnArgHi = 3;
    func_800B3F84(&work->anim, D_actor_105100_80141488, obj, work->field_30C,
                  work->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    work->field_560               = coord->coord;
    work->field_594               = 0x2800;
    work->field_5A8               = 1;
    work->field_59E               = 0xF;
    work->field_59A               = 0x96;
    work->obj47C.coord            = &arg1->extra.tmd->coords[3];
    records1                      = work->field_49C;
    work->obj47C.context.contacts = records1;
    work->obj47C.pos.vx           = 0;
    work->obj47C.pos.vy           = 0x1F4;
    work->obj47C.pos.vz           = 0;
    work->obj47C.key              = 0x30033;
    work->obj47C.radius           = 0x320;
    work->obj47C.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj47C);
    Gp_InitRec18Table(records1, 3, 0);
    work->obj47C.flags            = (u16)(work->obj47C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj51C.coord            = arg1->extra.tmd->coords;
    records2                      = work->field_53C;
    work->obj51C.context.contacts = records2;
    work->obj51C.pos.vx           = 0;
    work->obj51C.pos.vy           = 0;
    work->obj51C.pos.vz           = -0x12C;
    work->obj51C.key              = 0;
    work->obj51C.radius           = 0x4B0;
    work->obj51C.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj51C);
    Gp_InitRec18Table(records2, 1, 0);
    work->obj51C.flags            = (u16)(work->obj51C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj4E4.coord            = (gameGetPtrSlot(3))->extra.tmd->coords;
    records3                      = work->field_504;
    work->obj4E4.context.contacts = records3;
    work->obj4E4.pos.vx           = 0;
    work->obj4E4.pos.vy           = 0;
    work->obj4E4.pos.vz           = 0;
    work->obj4E4.key              = Gp_PackPair(D_actor_105100_80141380, 5);
    work->obj4E4.radius           = 0x1F4;
    work->obj4E4.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj4E4);
    Gp_InitRec18Table(records3, 1, 0);
    work->obj4E4.flags = work->obj4E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg1->state        = 1;
}

static void func_actor_105100_80132AA0(GpEnemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor105100Work* work;
    s32              state;

    obj   = arg1->extra.tmd;
    state = Gp_StateF0.field_4;
    work  = arg1->work;
    coord = obj->coords;
    switch (state) {
        case 0:
            obj->flags               = 0;
            arg0->node.state.b.flags = 8;
            if (work->field_5BC != 0) {
                SndEvt_EnqueueType9(0x40000000);
                work->field_5BC = 0;
            }
            break;
        case 1:
            func_actor_105100_801364CC(arg1);
            func_actor_105100_80136524(arg1);
            if (work->field_5BC == 0) {
                SndEvt_EnqueueType8(0x40000000);
            }
            work->field_5BC = state;
            return;
        case 2:
            obj->flags               = TMD_OBJECT_HIDDEN;
            arg0->node.state.b.flags = 1;
            if (work->field_5BC == 0) {
                SndEvt_EnqueueType8(0x40000000);
            }
            work->field_5BC = state;
            return;
    }
    if (arg0->reactionFlags != 0) {
        func_actor_105100_80135E54(arg1);
    }
    func_actor_105100_80132C2C(arg1);
    func_actor_105100_80133134(arg1);
    if (work->field_5A2 != 0) {
        func_actor_105100_80133CE4(arg1);
    }
    func_actor_105100_80136408(arg1);
    func_actor_105100_80134130(arg1);
    func_actor_105100_80136574(arg1, &work->field_560, work->field_594, 1);
    if (work->field_5A8 != 0) {
        func_shelter_b6_training_room_8018294C(arg1);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    func_actor_105100_801364CC(arg1);
    func_actor_105100_80136524(arg1);
}

/// Per-frame hit handler: walks the three `field_49C` contact records. A type-2 id lands only while the
/// `field_58C` cooldown is clear. Damage is the player distance through
/// `Gp_ComputeDamage`, quadrupled on a successful `Gp_RollEnemyChance`, and
/// halved (or zeroed for 0x8000 ids) while `field_5A8` is 1. The id parameter
/// may set a stagger flag, `Gp_SetObjFlag2`, or `Gp_SetObjFlag4`. HP is applied
/// through `func_800E2C78` / `func_800DA6E8`; at 0 the schedule goes to step 7,
/// and accumulated `field_5BE` past 0x1A4 (or the stagger flag) sends it to
/// step 6. A new id sparks `func_800FDB18` once, and `Gp_GetIdParam2` arms the
/// cooldown. The tail releases both record tables and steps `field_5C0`.
static void func_actor_105100_80132C2C(Task* arg0)
{
    s32                    flag;
    s32                    lastId;
    Actor105100HitScratch* sc;
    Actor105100Work*       work;
    GpEnemy*               ctx;
    GfxCoord*              coord;
    s32                    i;
    s16                    damage;
    s32                    snd;
    s32                    wait;

    flag   = 0;
    sc     = SCRATCH_PUSH(Actor105100HitScratch);
    lastId = 0;
    coord  = arg0->extra.tmd->coords;
    work   = arg0->work;
    ctx    = arg0->spawnArg2.pointer;
    if (work->field_58C != 0) {
        work->field_58C--;
        if (work->field_58C <= 0) {
            work->field_58C = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if ((u16)(work->field_49C[i].key.value >> 16) == 2 && work->field_58C == 0) {
            sc->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->delta.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            sc->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            damage       = Gp_ComputeDamage(work->field_49C[i].key.value,
                                            SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vy * sc->delta.vy +
                                                        sc->delta.vz * sc->delta.vz),
                                            0, 0);
            if (Gp_RollEnemyChance(ctx, work->field_49C[i].key.value, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
            }
            if (work->field_5A8 == 1) {
                if (work->field_49C[i].key.value & 0x8000) {
                    damage = 0;
                } else {
                    damage /= 2;
                }
                sc->ofs.vx = 0;
                sc->ofs.vy = 0;
                sc->ofs.vz = 0xC8;
                Gp_SpawnEff(0x601AC, &arg0->extra.tmd->coords[3], 0, &sc->ofs);
                snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x4033000D;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            func_800DA6E8(&ctx->node, damage, 0);
            if (damage != 0) {
                switch (Gp_GetIdParam0(work->field_49C[i].key.value) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        if ((work->field_49C[i].key.value & 0x3F) != 0x1C) {
                            flag = 1;
                        }
                        break;
                    case 2:
                        if (work->field_5A8 == 0) {
                            Gp_SetObjFlag2(ctx, work->field_49C[i].key.value, 0);
                        }
                        break;
                    case 3:
                        if (work->field_5A8 == 0) {
                            Gp_SetObjFlag4(ctx, work->field_49C[i].key.value, 0);
                        }
                        break;
                    case 4:
                        flag = 1;
                        break;
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                }
                func_800E2C78(ctx, work->field_49C[i].key.value, damage, 0);
                ctx->hp -= damage;
                if (ctx->hp <= 0) {
                    work->field_596 = 7;
                    work->field_598 = 0;
                    if (work->field_55C != NULL) {
                        work->field_55C->task->state = 4;
                        work->field_55C              = NULL;
                    }
                } else {
                    work->field_5BE += damage;
                    work->field_5C0  = 0xBC;
                    if ((s16)work->field_5BE >= 0x1A4 || flag == 1) {
                        work->field_5C0 = 0;
                        work->field_5BE = 0;
                        work->field_596 = 6;
                        work->field_598 = 0;
                        if (work->field_55C != NULL) {
                            work->field_55C->task->state = 4;
                            work->field_55C              = NULL;
                        }
                        if (work->field_5B4 != 0) {
                            work->field_5B4 = 0;
                            work->field_5B6 = 1;
                            work->field_5AA = 0;
                        }
                        work->field_5AC = 0;
                    }
                }
                work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (lastId != work->field_49C[i].key.value) {
                    lastId = work->field_49C[i].key.value;
                    func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL,
                                  &work->field_554);
                }
                wait = Gp_GetIdParam2(work->field_49C[i].key.value);
                if (wait > 0) {
                    work->field_58C = wait;
                }
            }
        }
    }
    Gp_ClearRec18Occupied(work->field_49C);
    work->field_5C0--;
    if ((s16)work->field_5C0 <= 0) {
        work->field_5BE = 0;
    }
    if (work->field_53C[0].flags & 1) {
        if ((work->field_53C[0].key.value & 0xFFFF0000) == 0x10000 && Player_Status.hp > 0) {
            work->field_5A2      = 1;
            Gp_StateC08.field_6 |= 1;
        }
        Gp_ClearRec18Occupied(work->field_53C);
    }
    SCRATCH_POP(Actor105100HitScratch);
}

/// The enemy's step dispatcher, run every frame out of the `field_596` schedule
/// the three handlers below this one step through. Bit 3 of `Gp_StateF0.field_1D`
/// is a reset request: it is cleared here and the block is put back on step 6
/// with the schedule and the animation re-arm both dropped.
///
/// Step 7 is terminal -- `func_actor_105100_80136318` retires the enemy and the
/// task stops being dispatched -- so it falls straight through to the tail, as
/// does a step outside 0..7. The tail runs the shared post-hit reaction
/// (`Gp_StateF0.field_1D` bit 2) and steps the `field_5AA` timer down while it is
/// positive. Step 0 also raises bit 1 of `Gp_StateF0.field_1D` once the HP drops
/// under the cap in `D_actor_105100_80141398.hpMax`.
static void func_actor_105100_80133134(Task* arg0)
{
    Actor105100Work* work;
    GpEnemy*         ctx;
    s16              state;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (Gp_StateF0.field_1D & 8) {
        Gp_StateF0.field_1D &= 0xF7;
        work->field_596      = 6;
        work->field_598      = 0;
        work->field_5A8      = 0;
    }
    state = work->field_596;
    switch (state) {
        case 0:
            if (ctx->hp < (s32)D_actor_105100_80141398.hpMax) {
                Gp_StateF0.field_1D |= 2;
            }
            func_actor_105100_8013329C(arg0, ctx);
            break;
        case 1:
            func_actor_105100_8013345C(arg0, ctx);
            break;
        case 2:
            func_actor_105100_801336B8(arg0, ctx);
            break;
        case 3:
            func_actor_105100_80133A14(arg0, ctx);
            break;
        case 4:
            func_actor_105100_80135F50(arg0);
            break;
        case 5:
            func_actor_105100_801360AC(arg0);
            break;
        case 6:
            func_actor_105100_801361C4(arg0);
            break;
        case 7:
            func_actor_105100_80136318(arg0);
        default:
            break;
    }
    if (Gp_StateF0.field_1D & 4) {
        func_actor_105100_80135FCC(arg0);
    }
    if (work->field_5AA > 0) {
        work->field_5AA = (u16)work->field_5AA - 1;
    }
}

/// The enemy's aim-retry step, run every frame the schedule is on step 1.
///
/// The `field_59E` countdown at the top is the aim timer: it is stepped down
/// whenever the battle is not paused (`Gp_StateF0::field_0`), and on the frame
/// it runs out the arming state drops to 1 (aimed) and scene music entry 0xA
/// is selected in `gStageSceneMusicEntry`. `field_5A8` is the pair of gate flags and is
/// tested as one word -- see `Actor105100Gate`.
///
/// The reroll itself is the LCG: the state advances, the pose is the table
/// entry the high nibble selects, and the `field_5B2` interval counter counts
/// attempts until it reaches 3, which sends the schedule on to step 3 (the
/// thrown pose) instead of back to the reroll.
static void func_actor_105100_8013329C(Task* arg0, GpEnemy* arg1)
{
    Actor105100Work* work;
    Actor105100Gate* gate;
    s16              state;
    s16              pose;
    u16              timer;
    u16              count;

    work = arg0->work;
    if (Gp_StateF0.prefix.bytes.field_0 == 0) {
        timer           = work->field_59E - 1;
        work->field_59E = timer;
        if ((timer << 16) <= 0) {
            Gp_ArmStateF0(1);
            gStageSceneMusicEntry = 0xA;
        }
    }
    gate = (Actor105100Gate*)work;
    if (gate->field_5A8 == 0) {
        work->field_596 = 4;
        work->field_598 = 0;
        return;
    }
    state = work->field_598;
    switch (state) {
        case 0:
            count           = work->field_59A - 1;
            work->field_59A = count;
            if ((count << 16) <= 0) {
                work->field_59A = 0;
                if (work->field_5B6 == 0) {
                    state = 2;
                    if (work->field_5B2 < 3) {
                        state = 1;
                    }
                    work->field_598 = state;
                    return;
                }
                work->field_598 = 3;
                work->field_5B6 = 0;
                return;
            }
            return;
        case 1: {
            u16* tbl = D_actor_105100_801413A8;
            u32  rnd = (Gp_LcgState * 5) + 0x71357911;

            pose            = (s16)tbl[(rnd >> 16) & 0xF];
            count           = (u16)work->field_5B2;
            Gp_LcgState     = rnd;
            work->field_598 = 0;
            count           = count + 1;
            work->field_5B2 = count;
            work->field_596 = pose;
            return;
        }
        case 2:
            work->field_596 = 3;
            work->field_598 = 0;
            work->field_5B2 = 0;
            return;
        case 3: {
            u16* tbl = D_actor_105100_801413A8;
            u32  rnd = (Gp_LcgState * 5) + 0x71357911;

            Gp_LcgState     = rnd;
            work->field_596 = (s16)tbl[(rnd >> 16) & 0xF];
            work->field_598 = 0;
            break;
        }
    }
}

/// The enemy's summon step, run every frame the schedule is on step 1. It is
/// the half of the appearance that runs before the model shows: sub-step 0
/// seeds the closing pose (3) and zeroes the spawn timer `field_59C` and the
/// spawned count `field_5AE`, arms the `field_5AC` gate the spawn tests, and
/// draws the LCG into `field_59A`, the aim window (`0x9E` .. `0xBD`).
///
/// Sub-step 1 holds everything on the animation frame counter `field_592`: not
/// until it passes `0x58` does the spawn timer start counting, and every expiry
/// sends one enemy out through `Gp_SpawnEnemyFromTable` -- up to four, gated on
/// `field_5AC` still being 1 -- with a fresh `0xF` .. `0x1E` interval drawn the
/// same way. The aim window steps down in parallel: at `0xF` it latches the
/// gate to 2, and at zero the step moves on to 2 with the hold pose `0xA`. The
/// single frame `field_592 == 0x58` is the cue: it builds the enemy's own id
/// into `field_580` and fires the type-6 event with the model coordinate's pan
/// and depth, which is what plays the summon as the model becomes visible.
///
/// Sub-step 2 waits out `field_592` to `0x1A`, then drops the whole step back
/// to pose 1, sub-step 0 and gate 0, rerolls `field_59A` to a `0` .. `0x3F`
/// window and fires the type-7 event on the id sub-step 1 built, clearing it.
static void func_actor_105100_8013345C(Task* arg0, GpEnemy* arg1)
{
    Actor105100Work* work;
    GfxCoord*        coord;
    s16              step;
    s32              pan;
    u16              spawnTimer;
    u16              aimTimer;
    u32              rnd;
    u32              spawnRnd;
    u32              resetRnd;

    work  = arg0->work;
    step  = work->field_598;
    coord = arg0->extra.tmd->coords;
    switch (step) {
        case 0:
            work->field_58E = 3;
            work->field_59C = 0;
            work->field_5AE = 0;
            work->field_598 = 1;
            work->field_5AC = 1;
            rnd             = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState     = rnd;
            work->field_59A = ((rnd >> 16) & 0x1F) + 0x9E;
            return;
        case 1:
            if ((s16)work->field_592 >= 0x58) {
                spawnTimer      = work->field_59C - 1;
                work->field_59C = spawnTimer;
                if ((spawnTimer << 16) <= 0 && (s16)work->field_5AE < 4 && work->field_5AC == 1) {
                    Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 1, 0,
                                           (GpEnemy*)arg0->spawnArg2.pointer);
                    work->field_5AE += 1;
                    spawnRnd         = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState      = spawnRnd;
                    work->field_59C  = ((spawnRnd >> 16) & 0xF) + 0xF;
                }
            }
            if ((s16)work->field_59A == 0xF) {
                work->field_5AC = 2;
            }
            aimTimer        = work->field_59A - 1;
            work->field_59A = aimTimer;
            if ((aimTimer << 16) <= 0) {
                work->field_598 = 2;
                work->field_58E = 0xA;
            }
            if ((s16)work->field_592 == 0x58) {
                work->field_580 = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330004;
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_580, pan,
                                    (s8)gpGetObjDepth(coord));
                return;
            }
            return;
        case 2:
            if ((s16)work->field_592 >= 0x1A) {
                work->field_58E = 1;
                work->field_596 = 0;
                work->field_598 = 0;
                work->field_5AC = 0;
                resetRnd        = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = resetRnd;
                work->field_59A = (resetRnd >> 16) & 0x3F;
                SndEvt_EnqueueType7(work->field_580, 1);
                work->field_580 = 0;
            }
            break;
    }
}

/// The enemy's split-spawn step, run every frame the schedule is on step 2.
/// It sits between the summon (`func_actor_105100_8013345C`) and the show
/// (`func_actor_105100_80133A14`).
///
/// Sub-step 0 seeds the closing pose (3), arms the `field_5AC` gate to 3,
/// zeroes the spawned count `field_5AE`, and draws the LCG into `field_5B0`
/// from `D_actor_105100_801413C8`.
///
/// Sub-step 1 holds on the animation frame counter `field_592`: the single
/// frame `field_592 == 0x1E` spawns the `0x800601A8` puff at the model's
/// coordinate -- offset 0/-0x6D6/0x320, life 0x3C -- and plays `...0007`.
/// Once the frame reaches `0x5A` it emits 2, 3 or 1 children through
/// `Gp_SpawnEnemyFromTable` according to `field_5B0`, loads the hold timer
/// from `D_actor_105100_80141448`, and plays `...0008` into `field_584`.
///
/// Sub-step 2 waits out `field_59A`, or bails as soon as `field_5AE` is still
/// 0, then moves on to pose `0xA` and fires the type-7 event on the id
/// sub-step 1 built.
///
/// Sub-step 3 waits `field_592` to `0x1A`, kills the puff, drops the gate,
/// and rerolls `field_59A` to a `0` .. `0x3F` window.
static void func_actor_105100_801336B8(Task* arg0, GpEnemy* arg1)
{
    Actor105100Work* work;
    GfxCoord*        coord;
    SVECTOR          pos;
    s16              step;
    s32              pan;
    s32              pan2;
    s32              snd;
    u16              timer;

    work  = arg0->work;
    step  = work->field_598;
    coord = arg0->extra.tmd->coords;
    switch (step) {
        case 0: {
            u16* tbl;
            u32  rnd;
            s16  kind;

            work->field_58E = 3;
            work->field_598 = 1;
            tbl             = D_actor_105100_801413C8;
            rnd             = (Gp_LcgState * 5) + 0x71357911;
            kind            = tbl[(rnd >> 16) & 0xF];
            Gp_LcgState     = rnd;
            work->field_5AE = 0;
            work->field_5AC = 3;
            work->field_5B0 = kind;
            return;
        }
        case 1:
            if ((s16)work->field_592 == 0x1E) {
                pos.vx          = 0;
                pos.vy          = -0x6D6;
                pos.vz          = 0x320;
                work->field_55C = Gp_SpawnEff(0x800601A8, arg0->extra.tmd->coords, 0x3C, &pos);
                snd             = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330007;
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_592 >= 0x5A) {
                switch (work->field_5B0) {
                    case 0:
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2.pointer);
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2.pointer);
                        break;
                    case 1:
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2.pointer);
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2.pointer);
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2.pointer);
                        break;
                    case 2:
                        Gp_SpawnEnemyFromTable(D_actor_105100_80141464, 2, 0,
                                               (GpEnemy*)arg0->spawnArg2.pointer);
                        break;
                }
                work->field_59A = D_actor_105100_80141448[work->field_5B0];
                work->field_598 = 2;
                work->field_584 = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330008;
                pan2            = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_584, pan2, (s8)gpGetObjDepth(coord));
                return;
            }
            return;
        case 2:
            timer           = work->field_59A - 1;
            work->field_59A = timer;
            if ((timer << 16) <= 0 || (s16)work->field_5AE == 0) {
                work->field_598 = 3;
                work->field_58E = 0xA;
                SndEvt_EnqueueType7(work->field_584, 1);
                work->field_584 = 0;
            }
            break;
        case 3: {
            u32        rnd;
            GpEffWork* eff;

            if ((s16)work->field_592 >= 0x1A) {
                work->field_58E = 1;
                work->field_596 = 0;
                work->field_598 = 0;
                rnd             = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rnd;
                eff             = work->field_55C;
                work->field_59A = (rnd >> 16) & 0x3F;
                if (eff != NULL) {
                    eff->task->state = 4;
                }
                work->field_55C = NULL;
                work->field_5AC = 0;
            }
            break;
        }
    }
}

/// The appearance handler, the step the aim-retry schedule hands to once it
/// wants the enemy to show up. `field_598` is the sub-state it walks through:
///
/// Step 0 seeds the show pose (`field_58E` 3), the `field_59A` timer at its
/// high-water 0xBC and the `field_5AA` aim window, then spawns the
/// `0x800601A9` puff at the model's coordinate -- lifted to the top of the
/// model by the 0x1F4/-0x6D6/0 z-offset vector -- and announces the
/// appearance on the `...0009` sound. Step 1 waits the timer out and, on the
/// 0x5A midpoint, plays the `...000A` sound; when the timer expires it moves
/// the schedule to step 2, plays `...000B` and runs the pad lerp in. Step 2
/// holds the enemy on the `0x8000` list flag while `field_592` is 0xC and
/// releases it after, moving to step 3 once it passes 0x1B. Step 3 clears
/// both flags and, past 0x1C, puts the schedule back on step 0 with a fresh
/// timer drawn from the gameplay LCG.
///
/// The pan and depth are cast at the call rather than through locals: the
/// sign extension then occupies the argument's own temporary (`$s0`) instead
/// of `work`'s register, which is what the original allocation needs.
static void func_actor_105100_80133A14(Task* arg0, GpEnemy* arg1)
{
    Actor105100Work* work;
    GfxCoord*        self;
    SVECTOR          pos;
    s32              snd;
    u16              timer;
    u32              rnd;

    work = arg0->work;
    self = arg0->extra.tmd->coords;
    switch (work->field_598) {
        case 0:
            work->field_58E = 3;
            work->field_59A = 0xBC;
            work->field_5AA = 0x1E;
            work->field_5A8 = 0;
            work->field_5B4 = 1;
            work->field_598 = 1;
            pos.vx          = 0;
            pos.vy          = -0x6D6;
            pos.vz          = 0x1F4;
            work->field_55C = Gp_SpawnEff(0x800601A9, arg0->extra.tmd->coords, (s16)work->field_59A + 0xA, &pos);
            work->field_588 = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330009;
            SndEvt_EnqueueType6(work->field_588, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            break;
        case 1:
            timer           = work->field_59A - 1;
            work->field_59A = timer;
            if ((timer << 16) <= 0) {
                work->field_598 = 2;
                work->field_58E = 4;
                SndEvt_EnqueueType7(work->field_588, 1);
                work->field_588 = 0;
                snd             = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x4033000B;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
                Gp_SpawnPadLerp(0xF, 8, 0xFF);
            }
            if ((s16)work->field_59A == 0x5A) {
                snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x4033000A;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            }
            break;
        case 2:
            if ((s16)work->field_592 == 0xC) {
                work->field_55C     = NULL;
                work->obj4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            } else {
                work->obj4E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if ((s16)work->field_592 >= 0x1B) {
                work->field_598 = 3;
                work->field_58E = 5;
            }
            break;
        case 3:
            work->field_5B4 = 0;
            if ((s16)work->field_592 >= 0x1C) {
                work->field_58E = 1;
                work->field_596 = 0;
                work->field_598 = 0;
                rnd             = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rnd;
                work->field_59A = (rnd >> 16) & 0x3F;
            }
            break;
    }
}

/// The attack body, run while `field_5A2` is set. It carves an
/// `ActorAttackScratch` from the scratch stack and steps `field_5A4`:
/// state 0 records which side of the player it is on (`field_5A0`), plays its
/// grab animation and spawns the effect; state 1 drags the player towards the
/// actor for 0x10 frames and hands over after 0x1E/0x20; state 2 waits for the
/// animation to finish and clears `field_5A2`.
static void func_actor_105100_80133CE4(Task* arg0)
{
    Actor105100Work*    work;
    GfxCoord*           coord;
    Task*               player;
    GfxCoord*           target;
    ActorAttackScratch* scratch;
    void*               head;
    s32                 sound;
    s32                 count;

    work               = arg0->work;
    player             = gameGetPtrSlot(3);
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = (u8*)head - sizeof(ActorAttackScratch);
    scratch            = SCRATCH_HEAD(ActorAttackScratch);
    coord              = arg0->extra.tmd->coords;
    target             = player->extra.tmd->coords;

    switch (work->field_5A4) {
        case 0:
            if (((GameActor*)player->work)->field_954 != 2) {
                scratch->delta.vx                  = target->coord.t[0] - coord->coord.t[0];
                scratch->delta.vy                  = 0;
                scratch->delta.vz                  = target->coord.t[2] - coord->coord.t[2];
                work->field_5A0                    = (scratch->delta.vx * target->coord.m[0][2] + scratch->delta.vz * target->coord.m[2][2]) > 0;
                scratch->anim.source.sets          = _gActor105100PlayerAnimationSets;
                scratch->anim.animationId          = work->field_5A0 + 1;
                scratch->anim.blend                = 0;
                scratch->anim.blendFrames          = 0;
                scratch->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, 0x3F4, scratch, 0);
                work->field_5A4 = 1;
                work->field_5A6 = 0;
                Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
                sound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 7;
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                scratch->dir.vx = 0;
                scratch->dir.vy = -1000;
                scratch->dir.vz = 0;
                Gp_SpawnEff(0x601AC, player->extra.tmd->coords, 0, &scratch->dir);
            } else {
                work->field_5A2 = 0;
            }
            break;
        case 1:
            if ((s16)work->field_5A6 < 0x10) {
                scratch->delta.vx = target->coord.t[0] - coord->coord.t[0];
                scratch->delta.vy = target->coord.t[1] - coord->coord.t[1];
                scratch->delta.vz = target->coord.t[2] - coord->coord.t[2];
                VectorNormalS(&scratch->delta, &scratch->dir);
                scratch->place.pos.vx = target->coord.t[0] + ((scratch->dir.vx * 25) >> 10);
                scratch->place.pos.vy = 0;
                scratch->place.pos.vz = target->coord.t[2] + ((scratch->dir.vz * 25) >> 10);
                scratch->place.rot.vx = 0;
                if (work->field_5A0 == 0) {
                    scratch->place.rot.vy = (ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                } else {
                    scratch->place.rot.vy = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                }
                scratch->place.rot.vz = 0;
                Gp_DispatchMsgPtr(player, 0x3E9, &scratch->place, 0);
            }
            if ((s16)work->field_5A6 == 0x10) {
                sound = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x55190003;
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            count = (s16)++work->field_5A6;
            if ((work->field_5A0 != 0 && count >= 0x1E) || (work->field_5A0 == 0 && count >= 0x20)) {
                scratch->anim.source.sets          = _gActor105100PlayerAnimationSets;
                scratch->anim.animationId          = work->field_5A0 + 3;
                scratch->anim.blend                = 0;
                scratch->anim.blendFrames          = 0;
                scratch->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, 0x3F4, scratch, 0);
                work->field_5A4 = 2;
                work->field_5A6 = 0;
            }
            break;
        case 2:
            if ((s16)++work->field_5A6 >= 0x25) {
                if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                    Gp_DispatchMsg(player, 0x3F1, 0, 0);
                    work->field_5A4 = 0;
                    work->field_5A6 = 0;
                    work->field_5A2 = 0;
                }
            }
            break;
    }
    SCRATCH_POP_BYTES(sizeof(ActorAttackScratch));
}

static void func_actor_105100_80134130(Task* arg0)
{
    s32                    snd;
    s32                    pan;
    s32                    pan2;
    Actor105100Work*       work;
    GfxCoord*              self;
    const AnimationRecord* rec;

    work = arg0->work;
    self = arg0->extra.tmd->coords;
    rec  = Gp_AnimGetRec(&work->anim, &work->slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->field_5B8 & ANIMATION_RECORD_CUE_2)) {
            snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330001;
            pan = (s8)Gp_GetObjPan(self);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
        }
        if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->field_5B8 & ANIMATION_RECORD_CUE_1)) {
            snd  = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330002;
            pan2 = (s8)Gp_GetObjPan(self);
            SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(self));
        }
        work->field_5B8 = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

/// Advances the work block's animation by a frame. A new animation id in
/// `field_58E` reseeds slots 1..18 from it, with the `D_actor_105100_801414C8`
/// entry that id selects, and restarts the frame count in `field_592`;
/// otherwise the count steps on and every slot is ticked.
static inline void _actor105100AnimUpdate(Task* task)
{
    Actor105100Work* work;
    s32              i;
    s32              val;

    work = task->work;
    if ((s16)work->field_58E != work->field_590) {
        work->field_590 = work->field_58E;
        work->field_592 = 0;
        val             = D_actor_105100_801414C8[(s16)work->field_58E];
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->anim, i, (s16)work->field_58E, 0, val);
        }
    } else {
        work->field_592++;
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Teardown handler in `D_actor_105100_80131E24`. Mode 1 of `Gp_StateF0.field_4` only
/// refreshes the actor colour; mode 2 hides the model and returns. Otherwise it
/// walks `field_598`: unlink the collision bodies, play the death clip, fire
/// the 0x13F4 cutscene, shrink the model, then destroy the enemy.
static void func_actor_105100_80134284(GpEnemy* arg0, Task* arg1)
{
    SVECTOR          dir;
    VECTOR           pos;
    Task*            actor;
    TmdObject*       obj;
    Actor105100Work* work;
    GfxCoord*        coord;
    Task*            player;
    s32              state;
    s32              snd;
    s16              flag;
    GfxCoord*        colorCoord;

    actor  = arg1;
    obj    = actor->extra.tmd;
    work   = actor->work;
    coord  = obj->coords;
    player = gameGetPtrSlot(3);
    state  = Gp_StateF0.field_4;
    if (state == 1) {
        goto color_update;
    }
    if (state >= 2) {
        if (state == 2) {
            actor->extra.tmd->flags = TMD_OBJECT_HIDDEN;
            return;
        }
    }
    if (work->field_5A2 != 0) {
        func_actor_105100_80133CE4(actor);
    }
    switch (work->field_598) {
        case 0:
            work->field_58E = 1;
            work->field_594 = 0x1000;
            work->field_560 = coord->coord;
            arg0->recs      = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->obj47C);
            Gp_UnlinkObj(&work->obj51C);
            Gp_UnlinkObj(&work->obj4E4);
            Gp_SetLightMode(arg0, 1);
            work->field_59A = 0;
            work->field_598 = 1;
            goto color_update;
        case 1:
            if ((s16)++work->field_59A == 0xA) {
                obj->flags = (u16)obj->flags | TMD_OBJECT_SEMI_TRANS;
            }
            if ((s16)work->field_59A >= 0x1F) {
                work->field_58E = 7;
                work->field_598 = 2;
                Gp_ReleaseStateF0Add(actor, 0x33);
                work->field_5BA = 1;
            }
            _actor105100AnimUpdate(actor);
            goto color_update;
        case 2:
            flag = work->field_5BA;
            if ((flag == 1) && (((GameActor*)player->work)->field_954 != 2) && (Gp_StateC08.field_A != flag) &&
                (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
                work->field_5BA = 0;
            }
            if ((s16)work->field_592 == 0xB) {
                snd = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x4033000E;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                Gp_SpawnPadLerp(0xA, 0xFF, 0x40);
            }
            if ((s16)work->field_592 == 0x28) {
                snd = ((((GpEnemy*)actor->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330003;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            }
            if ((s16)work->field_592 == 0x36) {
                dir.vx = 0;
                dir.vy = 0;
                dir.vz = 0x96;
                Gp_SpawnEff(0x600A5, &actor->extra.tmd->coords[3], 5, &dir);
                work->field_59A = 0;
            }
            if (((s16)work->field_592 >= 0x36) && (work->field_5BA == 0)) {
                work->field_598 = 3;
            }
            _actor105100AnimUpdate(actor);
            goto color_update;
        case 3:
            if (work->field_594 >= 0x201) {
                work->field_594 = (u16)work->field_594 - 0x50;
            }
            func_actor_105100_80136574(actor, &work->field_560, work->field_594, 0);
            if ((s16)++work->field_59A >= 0x3C) {
                work->field_598 = 4;
            }
        color_update:
            colorCoord = actor->extra.tmd->coords;
            pos.vx     = colorCoord->workm.t[0];
            pos.vy     = colorCoord->workm.t[1];
            pos.vz     = colorCoord->workm.t[2];
            Gp_UpdateActorColor(actor->spawnArg2.pointer, &pos, 0, 0);
            return;
        case 4:
            Gp_DestroyEnemy(arg0, actor);
            return;
    }
}

/// Setup handler of the projectile task. It allocates the task's
/// `Actor105100ProjWork` and, if that fails, tears the enemy down and stays on
/// this handler.
///
/// The model's coordinate starts as a copy of the parent's, moved by the
/// `D_actor_105100_801414E0` entry the parent's `field_5AE` selects and then
/// jittered on each axis by up to 127 units either way from the gameplay LCG.
/// The two list nodes are linked into list 3 - `obj0` on the model coordinate,
/// `obj38` on the pose segment - the collision table is initialised, the
/// billboard size and a random frame count are seeded, and the task moves to
/// `state` 1.
///
/// `seed`, `transY`, `index` and `temp` are shared or split the way they are
/// because the original's register allocation and scheduling depend on it:
/// the state is read before the Y store, which goes through a plain `long*`,
/// and the third table index and address live in temporaries reused later.
static void func_actor_105100_801347D4(GpEnemy* arg0, Task* arg1)
{
    Task*                parent;
    Actor105100Work*     parentWork;
    Actor105100ProjWork* work;
    GfxCoord*            coord;
    GfxCoord*            parentCoord;
    long*                transY;
    void*                temp;
    s32                  index;
    s32                  offsetY;
    u32                  seed;
    u32                  rollX;
    u32                  rollY;
    u32                  rollZ;
    u32                  rollA;
    u32                  rollB;
    s32                  amountX;
    s32                  amountY;
    s32                  amountZ;
    s32                  signX;
    s32                  signY;
    s32                  signZ;
    s32                  posX;
    s32                  posY;
    s32                  posZ;

    parent      = arg1->parent;
    coord       = arg1->extra.tmd->coords;
    parentCoord = parent->extra.tmd->coords;
    parentWork  = (Actor105100Work*)parent->work;
    work        = (Actor105100ProjWork*)memCalloc(0x80, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    arg1->work        = work;
    coord->parent     = &gGfxViewCoord;
    coord->coord      = parentCoord->coord;
    coord->coord.t[0] = parentCoord->coord.t[0] + D_actor_105100_801414E0[(s16)parentWork->field_5AE].vx;
    offsetY           = D_actor_105100_801414E0[(s16)parentWork->field_5AE].vy;
    seed              = Gp_LcgState;
    transY            = &coord->coord.t[1];
    *transY           = parentCoord->coord.t[1] + offsetY;
    rollX             = (Gp_LcgState = seed * 5 + 0x71357911) >> 16;
    index             = (s16)parentWork->field_5AE;
    temp              = &D_actor_105100_801414E0[index];
    coord->coord.t[2] = parentCoord->coord.t[2] + (amountX = ((SVECTOR*)temp)->vz);
    amountX           = rollX & 0x7F;
    signX             = rollX & 0x80;
    posX              = coord->coord.t[0];
    coord->coord.t[0] = !signX ? posX - amountX : posX + amountX;

    rollY             = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
    amountY           = rollY & 0x7F;
    signY             = rollY & 0x80;
    posY              = coord->coord.t[1];
    coord->coord.t[1] = !signY ? posY - amountY : posY + amountY;

    rollZ             = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
    amountZ           = rollZ & 0x7F;
    signZ             = rollZ & 0x80;
    posZ              = coord->coord.t[2];
    coord->coord.t[2] = !signZ ? posZ - amountZ : posZ + amountZ;

    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    work->obj0.coord            = arg1->extra.tmd->coords;
    work->obj0.context.contacts = &work->rec20;
    work->obj0.pos.vx           = 0;
    work->obj0.pos.vy           = 0;
    work->obj0.pos.vz           = 0;
    work->obj0.key              = Gp_PackPair(D_actor_105100_80141380, 0);
    work->obj0.radius           = 0xC8;
    work->obj0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj0);
    work->pose.end0.vx          = 0;
    work->pose.end0.vy          = 0;
    work->pose.end0.vz          = 0;
    work->pose.end1.vx          = 0;
    work->pose.end1.vy          = 0;
    work->pose.end1.vz          = -0x190;
    work->pose.end0Radius       = 1;
    work->pose.end1Radius       = 1;
    work->pose.recs             = &work->rec20;
    index                       = work->obj0.flags;
    index                      &= 0x7FFF;
    work->obj0.flags            = index;
    temp                        = arg1->extra.tmd->coords;
    work->obj38.context.capsule = &work->pose;
    work->obj38.pos.vx          = 0;
    work->obj38.pos.vy          = 0;
    work->obj38.pos.vz          = 0;
    work->obj38.key             = 0;
    work->obj38.radius          = 0;
    work->obj38.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->obj38.coord           = temp;
    Gp_LinkObj(3, &work->obj38);
    work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->field_7E = 0x190;
    rollA          = (Gp_LcgState * 5) + 0x71357911;
    rollB          = (rollA * 5) + 0x71357911;
    Gp_LcgState    = rollB;
    work->field_78 = ((rollA >> 16) & 0xF) + ((rollB >> 16) & 7);
    arg1->state    = 1;
}

/// The projectile task's state handlers, indexed by `Task::state`: setup,
/// per-frame flight and teardown.
static const GpEnemyTaskFuncTable3 D_actor_105100_80131E90 = {
    {
        func_actor_105100_801347D4,
        func_actor_105100_80134B00,
        func_actor_105100_801366D8,
    },
};

/// Per-frame handler of the glowing projectile this overlay spawns as its
/// second enemy task, the middle entry of `D_actor_105100_80131E90`. Mode 1 of
/// `Gp_StateF0.field_4` only redraws the billboard and mode 2 skips the frame.
///
/// `field_7A` steps the projectile through its life. It first hovers, jittering
/// its coordinate by a per-axis offset the gameplay LCG draws and taking each
/// offset only while the accumulated jitter stays inside its bound, and waits
/// there for the parent's state: gone, and the task ends; ready, and a
/// countdown launches it. It then turns onto its heading and starts
/// accelerating, aims itself at the player, and flies, arming its two
/// collision bodies once it is clear of the ground. The flight ends when the
/// contact record reports a hit or the time runs out: the projectile is
/// re-keyed and widened to the burst, which is spawned with its own effect and
/// sound, and a last step fades the body out and destroys the task.
static void func_actor_105100_80134B00(GpEnemy* arg0, Task* arg1)
{
    Actor105100ProjWork*    work;
    Actor105100Work*        parentWork;
    GfxCoord*               coord;
    Actor105100ProjScratch* scratch;
    s32                     state;
    u32                     rng;
    u32                     hi;
    s32                     val;
    u16                     speed;
    u16                     timer;
    s32                     snd;
    s32                     n;

    work       = arg1->work;
    coord      = arg1->extra.tmd->coords;
    parentWork = (arg1->parent)->work;
    state      = Gp_StateF0.field_4;
    if (state == 1) {
        func_actor_105100_80131EBC(coord, work->field_7E);
        return;
    }
    if (state < 2) {
        goto body;
    }
    if (state == 2) {
        return;
    }
body:
    SCRATCH_PUSH(Actor105100ProjScratch);
    scratch = SCRATCH_HEAD(Actor105100ProjScratch);
    switch (work->field_7A) {
        case 0:
            rng         = Gp_LcgState * 5 + 0x71357911;
            hi          = rng >> 16;
            val         = hi & 0x3F;
            Gp_LcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vx = val;
            if (ABS(work->field_70.vx + (s16)val) < 0x1F4) {
                work->field_70.vx += val;
                coord->coord.t[0] += scratch->rot.vx;
            }
            rng         = Gp_LcgState * 5 + 0x71357911;
            hi          = rng >> 16;
            val         = hi & 0x3F;
            Gp_LcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vy = val;
            if (ABS(work->field_70.vy + (s16)val) < 0x1F4) {
                work->field_70.vy += val;
                coord->coord.t[1] += scratch->rot.vy;
            }
            rng         = Gp_LcgState * 5 + 0x71357911;
            hi          = rng >> 16;
            val         = hi & 0x3F;
            Gp_LcgState = rng;
            if (!(hi & 0x40)) {
                val = -val;
            }
            scratch->rot.vz = val;
            if (ABS(work->field_70.vz + (s16)val) < 0x1F4) {
                work->field_70.vz += val;
                coord->coord.t[2] += scratch->rot.vz;
            }
            if (parentWork->field_5AC == 0) {
                arg1->state = 2;
            }
            if (parentWork->field_5AC == 2) {
                timer          = work->field_78 - 1;
                work->field_78 = timer;
                if ((timer << 16) <= 0) {
                    work->field_7A = 1;
                    work->field_7C = 1;
                    work->field_78 = 0;
                }
            }
            goto update;
        case 1:
            scratch->rot.vx = 0x20;
            scratch->rot.vy = 0;
            scratch->rot.vz = 0;
            RotMatrix(&scratch->rot, &scratch->mat);
            gte_SetRotMatrix(&coord->coord);
            gte_ldclmv(&scratch->mat);
            gte_rtir();
            gte_stclmv(&coord->coord);
            gte_ldclmv((char*)&scratch->mat + 2);
            gte_rtir();
            gte_stclmv((char*)&coord->coord + 2);
            gte_ldclmv((char*)&scratch->mat + 4);
            gte_rtir();
            gte_stclmv((char*)&coord->coord + 4);
            speed          = work->field_7C * 2;
            work->field_7C = speed;
            if ((s16)speed >= 0x33) {
                work->field_7C = 0x32;
            }
            coord->coord.t[0] += (coord->coord.m[0][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[1] += (coord->coord.m[1][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * (s16)work->field_7C) >> 12;
            timer              = work->field_78 + 1;
            work->field_78     = timer;
            if ((s16)timer >= 0x10) {
                work->field_7A = 2;
                work->field_78 = 0;
            }
            goto update;
        case 2:
            timer          = work->field_78 + 1;
            work->field_78 = timer;
            if ((s16)timer >= 3) {
                scratch->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                scratch->vec.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                scratch->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                Gp_OrientAlong(&scratch->vec, &coord->coord, 0);
                work->field_7A = 3;
                work->field_78 = 0;
                work->field_7C = 1;
            }
            goto update;
        update:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            func_actor_105100_80131EBC(coord, work->field_7E);
            break;
        case 3:
            n = 4;
            if ((s16)++work->field_78 == n) {
                snd = ((((GpEnemy*)arg1->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330005;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (coord->coord.t[1] >= -0xF9F) {
                work->obj0.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->obj38.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            speed          = work->field_7C * 2;
            work->field_7C = speed;
            if ((s16)speed >= 0x12D) {
                work->field_7C = 0x12C;
            }
            coord->coord.t[0]  += (coord->coord.m[0][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[1]  += (coord->coord.m[1][2] * (s16)work->field_7C) >> 12;
            coord->coord.t[2]  += (coord->coord.m[2][2] * (s16)work->field_7C) >> 12;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            func_actor_105100_80131EBC(coord, work->field_7E);
            if (work->rec20.key.value != 0 || (s16)work->field_78 >= 0x1A) {
                work->obj38.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                Gp_ClearRec18Occupied(&work->rec20);
                work->obj0.key    = Gp_PackPair(D_actor_105100_80141380, 1);
                work->obj0.radius = 0x1F4;
                work->field_78    = 0x1E;
                work->field_7A    = n;
                Gp_SpawnEff(0x601A7, coord, 0, NULL);
                snd = ((((GpEnemy*)arg1->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40330006;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            break;
        case 4:
            if ((s16)work->field_78 == 0x14) {
                work->obj0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            timer          = work->field_78 - 1;
            work->field_78 = timer;
            if ((timer << 16) <= 0) {
                arg1->state = 2;
            }
            break;
    }
    SCRATCH_POP(Actor105100ProjScratch);
}

static void func_actor_105100_80135278(GpEnemy* arg0, Task* arg1)
{
    Task*            parent;
    Actor105100Work* work;
    Actor105100Rec*  obj;
    GfxCoord*        dst;
    GfxCoord*        src;

    parent = arg1->parent;
    work   = (Actor105100Work*)parent->work;
    dst    = arg1->extra.tmd->coords;
    src    = parent->extra.tmd->coords;

    if (D_actor_105100_80141450[work->field_5B0 * 3 + (s16)work->field_5AE] == -1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    obj = (Actor105100Rec*)memCalloc(0x50, 0);
    if (obj == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    arg1->work    = obj;
    obj->field_40 = work->field_5B0;
    obj->field_42 = work->field_5AE;
    work->field_5AE++;
    obj->field_44             = D_actor_105100_80141450[work->field_5B0 * 3 + obj->field_42];
    obj->field_48             = D_actor_105100_80141448[obj->field_40];
    obj->field_4E             = 3;
    dst->parent               = &gGfxViewCoord;
    dst->coord                = src->coord;
    dst->coord.t[0]           = src->coord.t[0] + D_actor_105100_801413E8[obj->field_44].vx;
    dst->coord.t[1]           = src->coord.t[1] + D_actor_105100_801413E8[obj->field_44].vy;
    dst->coord.t[2]           = src->coord.t[2] + D_actor_105100_801413E8[obj->field_44].vz;
    dst->composeStamp         = GRAPHICS_COORD_DIRTY;
    obj->obj.coord            = arg1->extra.tmd->coords;
    obj->obj.context.contacts = obj->rec;
    obj->obj.pos.vx           = 0;
    obj->obj.pos.vy           = 0;
    obj->obj.pos.vz           = 0;
    obj->obj.key              = Gp_PackPair(D_actor_105100_80141380, obj->field_40 + 2);
    obj->obj.radius           = 0xC8;
    obj->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &obj->obj);
    Gp_InitRec18Table(obj->rec, 1, 0);
    obj->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    arg1->state     = 1;
}

/// The per-frame handler the `state == 1` dispatch runs: it hands the reaction
/// `field_40` selects to one of the `80135674` / `801359B4` / `80135B40`
/// sub-handlers, retimes the pose every 6/0xB/0x10 frames of the countdown in
/// `field_48`, and ends the fight (`state = 2`) once that countdown, the work's
/// `field_24` and the parent's `field_5AC` all say so.
static void func_actor_105100_801354E8(GpEnemy* arg0, Task* arg1)
{
    Actor105100Rec*  rec;
    Actor105100Work* parentWork;
    GfxCoord*        coord;
    s32              state;
    s32              one;
    s16              timer;
    u16              count;

    rec        = arg1->work;
    parentWork = (arg1->parent)->work;
    state      = Gp_StateF0.field_4;
    coord      = arg1->extra.tmd->coords;
    one        = 1;

    if (state == one) {
        func_shelter_b6_training_room_8017FC40(coord, 0x80, rec->field_4E);
        return;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto done;
    }
default_body:
    if (rec->field_40 == one) {
        goto rec1;
    }
    if (rec->field_40 >= 2) {
        goto ge2;
    }
    if (rec->field_40 == 0) {
        goto rec0;
    }
    goto join;
ge2:
    if (rec->field_40 == 2) {
        goto rec2;
    }
    goto join;
rec0:
    func_actor_105100_80135674(arg1);
    goto join;
rec1:
    func_actor_105100_801359B4(arg1);
    goto join;
rec2:
    func_actor_105100_80135B40(arg1);
join:
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    timer = rec->field_48;
    if (timer < 6) {
        rec->field_4E = 0;
    } else if (timer < 0xB) {
        rec->field_4E = 1;
    } else if (timer < 0x10) {
        rec->field_4E = 2;
    }
    func_shelter_b6_training_room_8017FC40(coord, 0x80, rec->field_4E);
    count         = (u16)rec->field_48 - 1;
    rec->field_48 = count;
    if ((count << 16) <= 0 || rec->rec[0].key.value != 0 ||
        parentWork->field_5AC == 0) {
        parentWork->field_5AE = parentWork->field_5AE - 1;
        arg1->state           = 2;
    }
done:
    return;
}

/// Reaction 0's handler (`field_40 == 0`), which walks the model along a
/// two-leg path through `D_actor_105100_80141418`: `field_44`, then
/// `field_44 + 3`. Pass 0 builds the first-leg aim, measures both legs and
/// stores the per-frame step (total length over `field_48`) plus how many
/// frames the first leg takes; pass 1 walks that step and re-aims at the
/// second point when the countdown hits 0; pass 2 keeps walking.
static void func_actor_105100_80135674(Task* arg0)
{
    Actor105100Rec* rec;
    GfxCoord*       coord;
    VECTOR*         head;
    VECTOR*         vec;
    s16             state;
    s32             dx;
    s32             dz;
    s32             dx2;
    s32             dz2;
    s32             dist;
    s32             speed;
    s16             timer;

    head                 = SCRATCH_HEAD(VECTOR);
    vec                  = head - 1;
    SCRATCH_HEAD(VECTOR) = vec;
    rec                  = arg0->work;
    state                = rec->field_46;
    coord                = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            vec->vx = D_actor_105100_80141418[rec->field_44].vx - coord->coord.t[0];
            vec->vy = 0;
            vec->vz = D_actor_105100_80141418[rec->field_44].vz - coord->coord.t[2];
            VectorNormalS(vec, &rec->direction);
            dx      = vec->vx;
            dz      = vec->vz;
            dist    = SquareRoot0(dx * dx + dz * dz);
            vec->vx = D_actor_105100_80141418[rec->field_44 + 3].vx -
                      D_actor_105100_80141418[rec->field_44].vx;
            vec->vy = 0;
            dz2     = D_actor_105100_80141418[rec->field_44 + 3].vz -
                  D_actor_105100_80141418[rec->field_44].vz;
            vec->vz          = dz2;
            dx2              = vec->vx;
            speed            = (dist + SquareRoot0(dx2 * dx2 + dz2 * dz2)) / rec->field_48;
            rec->field_46    = 1;
            rec->step        = speed;
            rec->travelTicks = dist / (s16)speed;
            break;
        case 1:
            coord->coord.t[0] += (rec->direction.vx * rec->step) >> 12;
            coord->coord.t[2] += (rec->direction.vz * rec->step) >> 12;
            timer              = (u16)rec->travelTicks - 1;
            rec->travelTicks   = timer;
            if ((timer << 16) <= 0) {
                vec->vx = D_actor_105100_80141418[rec->field_44 + 3].vx - coord->coord.t[0];
                vec->vy = 0;
                vec->vz = D_actor_105100_80141418[rec->field_44 + 3].vz - coord->coord.t[2];
                VectorNormalS(vec, &rec->direction);
                rec->field_46 = 2;
            }
            break;
        case 2:
            coord->coord.t[0] += (rec->direction.vx * rec->step) >> 12;
            coord->coord.t[2] += (rec->direction.vz * rec->step) >> 12;
            break;
    }
    SCRATCH_POP(VECTOR);
}

/// Reaction 1's handler (`field_40 == 1`), which walks the model towards the
/// approach point `field_44` selects from `D_actor_105100_80141418`. The first
/// pass (`field_46 == 0`) builds the planar delta in 16 bytes of scratch,
/// normalises it into the record's own 0x38 vector and stores the step it then
/// travels per frame -- the delta's length over `field_48`; the second
/// (`field_46 == 1`) applies that step to the coordinate every frame.
static void func_actor_105100_801359B4(Task* arg0)
{
    Actor105100Rec* rec;
    GfxCoord*       coord;
    VECTOR*         head;
    VECTOR*         vec;
    s16             state;
    s32             dx;
    s32             dz;
    s32             speed;

    head                 = SCRATCH_HEAD(VECTOR);
    vec                  = head - 1;
    SCRATCH_HEAD(VECTOR) = vec;
    rec                  = arg0->work;
    state                = rec->field_46;
    coord                = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            vec->vx = D_actor_105100_80141418[rec->field_44].vx - coord->coord.t[0];
            vec->vy = 0;
            vec->vz = D_actor_105100_80141418[rec->field_44].vz - coord->coord.t[2];
            VectorNormalS(vec, &rec->direction);
            dx            = vec->vx;
            dz            = vec->vz;
            speed         = SquareRoot0(dx * dx + dz * dz) / rec->field_48;
            rec->field_46 = 1;
            rec->step     = speed;
            break;
        case 1:
            coord->coord.t[0] += (rec->direction.vx * rec->step) >> 12;
            coord->coord.t[2] += (rec->direction.vz * rec->step) >> 12;
            break;
    }
    SCRATCH_POP(VECTOR);
}

static void func_actor_105100_80135B40(Task* arg0)
{
    ActorFaceScratch* sc;
    GfxCoord*         coord;
    s32               ang;
    s32               cur;
    s16               target;
    s16               diff;
    s32               adiff;
    s16               snap;
    s32               next;
    s32               step;

    sc           = (ActorFaceScratch*)SCRATCH_PUSH_BYTES(0x18);
    coord        = arg0->extra.tmd->coords;
    sc->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = 0;
    sc->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    ang          = ratan2((s32)(s16)sc->delta.vx, (s32)(s16)sc->delta.vz) & 0xFFF;
    snap         = ang;
    cur          = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    target       = cur;
    diff         = ang - cur;
    adiff        = diff >= 0 ? diff : -diff;
    if (adiff < 0x800) {
        target = ang;
        if (adiff >= 0x51) {
            next = (s16)cur;
            if (diff > 0) {
                target = next + 0x50;
            } else {
                target = next - 0x50;
            }
        }
    } else {
        if (diff > 0) {
            if (0x1000 - diff < 0x51) {
                goto snapTurn;
            } else {
                goto turn;
            }
        } else if (0x1000 + diff < 0x51) {
            goto snapTurn;
        } else {
            goto turn;
        }
    snapTurn:
        target = snap;
        goto done;
    turn:
        step = (s16)target;
        if (diff > 0) {
            target = step - 0x50;
        } else {
            target = step + 0x50;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = target;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    coord->coord.t[0] += (coord->coord.m[0][2] * 0xF) >> 0xA;
    coord->coord.t[2] += (coord->coord.m[2][2] * 0xF) >> 0xA;
    SCRATCH_POP_BYTES(0x18);
}

/// Unless the player is in an event, draws from the gameplay LCG and on one
/// frame in four spawns effect `D_80115728` on `arg0`, offset in a random
/// horizontal direction; `arg1` is or-ed into the spawn flags.
static void func_actor_105100_80135CEC(GfxCoord* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (Gp_State1C->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            ang         = ((u32)Gp_LcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            Gp_SpawnEff(D_80115728, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}

void func_actor_105100_80135DF8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_105100_80135E54(Task* arg0)
{
    GpEnemy*         enemy;
    Actor105100Work* work;
    s32              state;
    s32              damage;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags & 1) {
        enemy->reactionFlags = flags & 0xFE;
    }
    if ((enemy->reactionFlags & 2) && (work->field_596 != 5)) {
        work->field_596 = 5;
        work->field_598 = 0;
        work->field_5C2 = 1;
    }
    if (enemy->reactionFlags & 0xC) {
        tick = Gp_TickObjFlag4(enemy) << 0x10;
        if (tick != 0) {
            damage = tick >> 0x12;
            func_800DA6E8(&enemy->node, damage, 0);
            state     = (u16)enemy->hp - damage;
            enemy->hp = state;
            state   <<= 0x10;
            if (state <= 0) {
                state = 7;
            } else {
                state = 6;
            }
            work->field_596 = state;
            work->field_598 = 0;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }
}

/// Second step of the `field_598` schedule: arms pose 2 with the `field_59A`
/// timer at 0x3C frames, then, when the timer runs out, hands the pose back to
/// the schedule entry step and returns it to 0.
static void func_actor_105100_80135F50(Task* arg0)
{
    Actor105100Work* work;
    s32              state;
    u16              timer;

    work  = arg0->work;
    state = work->field_598;
    switch (state) {
        case 0:
            work->field_58E = 2;
            work->field_59A = 0x3C;
            work->field_598 = 1;
            break;
        case 1:
            timer           = work->field_59A - 1;
            work->field_59A = timer;
            if ((timer << 16) <= 0) {
                work->field_5A8 = state;
                work->field_58E = state;
                work->field_596 = 0;
                work->field_598 = 0;
                work->field_59A = 0;
            }
            break;
    }
}

static void func_actor_105100_80135FCC(Task* arg0)
{
    GpEnemy*  enemy;
    GfxCoord* coord;
    s32       snd;
    s32       pan;
    u16       hp;

    enemy                = arg0->spawnArg2.pointer;
    coord                = arg0->extra.tmd->coords;
    Gp_StateF0.field_1D &= 0xFB;
    hp                   = enemy->hp + 0x50;
    enemy->hp            = hp;
    if (D_actor_105100_80141398.hpMax < (s16)hp) {
        enemy->hp = D_actor_105100_80141398.hpMax;
    }
    func_800DA6E8(&enemy->node, -0x50, 0);
    Gp_SpawnEff(0x601AF, NULL, 0, NULL);
    snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x4033000C;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
}

/// Opening stage of the `field_598` schedule: arms pose 8, releases the held
/// effect slot and drops the `obj4E4.flags` pose bit, then waits on
/// `Gp_TickObjFlag2` before clearing the enemy's flag-2 bit. On the last stage
/// it waits out the `field_592` timer and returns the schedule to step 0.
static void func_actor_105100_801360AC(Task* arg0)
{
    Actor105100Work* work;
    GpEnemy*         enemy;
    GpEffWork*       eff;
    s32              state;

    work  = arg0->work;
    state = work->field_598;
    enemy = arg0->spawnArg2.pointer;
    switch (state) {
        case 0:
            work->field_58E = 8;
            work->field_598 = 1;
            if (work->field_5B4 != 0) {
                work->field_5B4 = 0;
                work->field_5B6 = 1;
                work->field_5AA = 0;
            }
            work->field_5AC = 0;
            func_actor_105100_801362A0(arg0);
            eff                = work->field_55C;
            work->obj4E4.flags = work->obj4E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (eff != NULL) {
                eff->task->state = 4;
                work->field_55C  = NULL;
            }
            if (work->field_5B6 == 0) {
                work->field_5AA = 0x1E;
            }
            break;
        case 1:
            if (Gp_TickObjFlag2(enemy) != 0) {
                work->field_58E       = 9;
                work->field_598       = 2;
                work->field_5C2       = 0;
                enemy->reactionFlags &= 0xFD;
            }
            break;
        case 2:
            if ((s16)work->field_592 >= 0xB) {
                work->field_596 = 0;
                work->field_598 = 0;
                work->field_58E = 1;
            }
            break;
    }
}

/// First stage of the `field_598` schedule: arms the pose and the effect slot,
/// and on the next stage waits out the `field_592` timer before handing the
/// state back on, either aborting (0) or resuming (8) depending on `field_5C2`.
static void func_actor_105100_801361C4(Task* arg0)
{
    Actor105100Work* work;
    GpEffWork*       eff;
    s32              state;

    work  = arg0->work;
    state = work->field_598;
    switch (state) {
        case 0:
            work->field_58E = 6;
            work->field_598 = 1;
            work->field_5B4 = 0;
            work->field_5AC = 0;
            func_actor_105100_801362A0(arg0);
            eff                = work->field_55C;
            work->obj4E4.flags = work->obj4E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (eff != NULL) {
                eff->task->state = 4;
                work->field_55C  = NULL;
            }
            if (work->field_5B6 == 0) {
                work->field_5AA = 0x1E;
            }
            break;
        case 1:
            if ((s16)work->field_592 >= 0x1D) {
                if (work->field_5C2 == 0) {
                    work->field_596 = 0;
                    work->field_58E = state;
                } else {
                    work->field_596 = 5;
                    work->field_58E = 8;
                }
                work->field_598 = 0;
            }
            break;
    }
}

static void func_actor_105100_801362A0(Task* arg0)
{
    Actor105100Work* work;
    s32              snd;

    work = arg0->work;

    snd = work->field_580;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_580 = 0;
    }
    snd = work->field_584;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_584 = 0;
    }
    snd = work->field_588;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_588 = 0;
    }
}

/// Last-enemy handler. While the remaining-enemy count is still positive it
/// retires the queued sound events, unlinks the running effect, drops the
/// 0x8000 bit of `obj4E4.flags` and pins the task to the tick handler (`state` 2);
/// once the count is spent it puts the enemy's HP (`GpEnemy::hp`)
/// at 1 and arms pose 6, leaving `state` alone.
///
/// The work block is read twice on purpose. The two loads do not CSE (the
/// `field_5B4` / `field_5AC` stores sit between them), and the first pointer is
/// still live at the tail for `obj4E4.flags` and `field_55C`, so the second one
/// needs a register of its own.
static void func_actor_105100_80136318(Task* arg0)
{
    Actor105100Work* work;
    Actor105100Work* sndWork;
    GpEffWork*       eff;
    s32              snd;

    work = arg0->work;
    if (Player_Status.hp <= 0) {
        ((GpEnemy*)arg0->spawnArg2.pointer)->hp = 1;
        work->field_596                         = 6;
        work->field_598                         = 0;
        return;
    }

    work->field_5B4 = 0;
    work->field_5AC = 0;

    sndWork = arg0->work;

    snd = sndWork->field_580;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_580 = 0;
    }
    snd = sndWork->field_584;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_584 = 0;
    }
    snd = sndWork->field_588;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_588 = 0;
    }

    eff                = work->field_55C;
    work->obj4E4.flags = work->obj4E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (eff != NULL) {
        eff->task->state = 4;
        work->field_55C  = NULL;
    }

    arg0->state = 2;
}

static void func_actor_105100_80136408(Task* arg0)
{
    _actor105100AnimUpdate(arg0);
}

/// Relights the actor at its model's world position: copies the model
/// coordinate's translation into a `VECTOR` and hands it with the context to
/// `Gp_UpdateActorColor`, with no blend parameters.
static void func_actor_105100_801364CC(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// The child collision task's state handlers, indexed by `Task::state`:
/// setup, per-frame reaction and teardown.
static const GpEnemyTaskFuncTable3 D_actor_105100_80131EB0 = {
    {
        func_actor_105100_80135278,
        func_actor_105100_801354E8,
        func_actor_105100_80136788,
    },
};

static void func_actor_105100_80136524(Task* arg0)
{
    GfxCoord* coord;
    VECTOR3   vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x9C4, 0x80);
}

/// Sets the model's root coordinate to `arg1` scaled by `arg2`, and marks it
/// for recomputation. The scale is built in a 0x30-byte block borrowed from
/// the scratchpad: an identity rotation is written word-wise and
/// `ScaleMatrix` scales it, on all three axes when `arg3` is non-zero and on
/// Y alone when it is zero.
///
/// The scratchpad head is written twice, from two separate computations of
/// `head - 0x30`. CSE cannot substitute a value that holds no register, so
/// the store keeps the block-local `$v1` while `blk` - which crosses both
/// calls - is copied into `$s0` by `reload_cse_regs`. Folding the two into one
/// variable allocates `blk`'s register for the store as well and loses the
/// copy, the delay-slot fill and the frame layout.
static void func_actor_105100_80136574(Task* arg0, MATRIX* arg1, s16 arg2, s32 arg3)
{
    ActorScaleScratch* head;
    ActorScaleScratch* blk;
    GfxCoord*          coord;

    head                            = SCRATCH_HEAD(ActorScaleScratch);
    SCRATCH_HEAD(ActorScaleScratch) = head - 1;
    blk                             = head - 1;
    coord                           = arg0->extra.tmd->coords;

    if (arg3 == 0) {
        blk->scale.vx = 0x1000;
        blk->scale.vy = arg2;
        blk->scale.vz = 0x1000;
    } else {
        blk->scale.vx = arg2;
        blk->scale.vy = arg2;
        blk->scale.vz = arg2;
    }

    coord->coord = *arg1;

    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;

    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_POP(ActorScaleScratch);
}

void func_actor_105100_8013667C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_105100_801366D8(GpEnemy* arg0, Task* arg1)
{
    Actor105100ProjWork* work;

    work = arg1->work;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    Gp_DestroyEnemy(arg0, arg1);
}

void func_actor_105100_8013672C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131EB0;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_105100_80136788(GpEnemy* arg0, Task* arg1)
{
    Gp_UnlinkObj(arg1->work);
    Gp_DestroyEnemy(arg0, arg1);
}
