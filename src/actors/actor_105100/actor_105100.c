#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "actors/actor_105100.h"
#include "actors/actors_shared_80134ff0.h"
#include "actors/actors_shared_80136574.h"
#include "main/sound.h"

#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105100_80131E24;

void func_8017FC40(GsCOORDINATE2* arg0, s32 arg1, u16 arg2);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_8018294C(Actor105100* arg0);
void func_actor_105100_80132C2C(Actor105100* arg0);
void func_actor_105100_80133134(Actor105100* arg0);
void func_actor_105100_8013329C(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_8013345C(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_801336B8(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_80133A14(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_80133CE4(Actor105100* arg0);
void func_actor_105100_80134130(Actor105100* arg0);
void func_actor_105100_80135674(Actor105100* arg0);
void func_actor_105100_801359B4(Actor105100* arg0);
void func_actor_105100_80135B40(Actor105100* arg0);
void func_actor_105100_80135E54(Actor105100* arg0);
void func_actor_105100_80135F50(Actor105100* arg0);
void func_actor_105100_80135FCC(Actor105100* arg0);
void func_actor_105100_801360AC(Actor105100* arg0);
void func_actor_105100_801361C4(Actor105100* arg0);
void func_actor_105100_80136318(Actor105100* arg0);
void func_actor_105100_80136408(Actor105100* arg0);
void func_actor_105100_80136524(Actor105100* arg0);

void func_800B4114(Actor105100Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;

/// Main-executable global with no module header yet: bit 2 asks the per-frame
/// handler for the post-hit reaction, which is why `func_actor_105100_80133134`
/// runs `func_actor_105100_80135FCC` off it on every frame it is set.
extern u8 D_8011540D;

/// Main-executable global with no module header yet: the remaining-enemy count
/// `func_actor_105100_80136318` tests to decide whether the fight is over.
extern s16 D_80073BA0;

/// The run of HP caps at 0x8014139C; `func_actor_105100_80135FCC` reads the
/// first entry. Declared as an aggregate on purpose: a bare `extern u16` makes
/// `true_dependence` (`sched.c:846`) drop the dependence between the store to
/// `Actor105100Ctx::field_40` and this load -- the store is in-struct with a
/// varying address, this load a scalar MEM at a fixed one -- and sched2 then
/// hoists this load above the store, ahead of the `sll`.
extern u16 D_actor_105100_8014139C[1];

/// The s16 animation-id run at 0x801414C8, one entry per work state at
/// `Actor105100Work::field_58E`; `func_actor_105100_80136408` reads the entry
/// the new state selects before it re-queues every slot.
extern s16 D_actor_105100_801414C8[];

/// The spawn's pair tables. `Gp_PackPair` packs the `GpU16Pair` at 0x80141380
/// into the work's third list node (`Actor105100Work::obj4E4.key`), and the
/// `GpPairSrcE` at 0x80141398 is the pair source the context points at with
/// `Actor105100Ctx::field_50` -- its `hpMax` seeds the enemy's HP.
extern GpU16Pair  D_actor_105100_80141380;
extern GpPairSrcE D_actor_105100_80141398;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_105100_80141488[];

/// The approach points the `field_40 == 1` reaction walks the model through,
/// indexed by `Actor105100Rec::field_44`. Only the x and z halves are read: the
/// reaction subtracts the model's current position and walks the resulting
/// planar delta.
extern SVECTOR D_actor_105100_80141418[6];

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80131EBC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80132414);

/// Spawn/setup handler. It allocates the 0x5C4-byte work block and hangs it off
/// the task, points the model object at the block's two `MATRIX`es (0x45C the
/// light matrix, 0x43C the colour one) and fills the context's coordinate,
/// pair source and HP (`field_40`, seeded from the record's `hpMax`).
///
/// The block's 0x14-prefix then becomes the `GpAnimCtx`: `func_800B3F84` loads
/// the animation data into it over the nineteen `GpAnimSlot`s, and slots 1..18
/// are reset. The three list nodes at 0x47C / 0x4E4 / 0x51C are linked into the
/// global object lists with their collision tables (`Gp_InitRec18Table`), which
/// also sets each node's 0x8000 "last element" flag -- then the second node's is
/// cleared again. `&coord[3]` -- the actor's fourth coordinate -- is what the
/// first node, `field_554` and the context's `field_18` all hang off.
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler; otherwise the task moves to the tick handler (`state` 1).
void func_actor_105100_801327B4(Actor105100Ctx* arg0, Actor105100* arg1)
{
    Actor105100Work*  work;
    Actor105100Obj2C* obj;
    GsCOORDINATE2*    coord;
    GpRec18*          records1;
    GpRec18*          records2;
    GpRec18*          records3;
    s32               i;

    obj   = arg1->field_2C;
    coord = obj->field_8;
    work  = memCalloc(0x5C4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->field_1C = work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_1C  = &work->field_45C;
    obj->field_20  = &work->field_43C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18             = &arg1->field_2C->field_8[3];
    arg0->field_1C             = 0;
    arg0->field_20             = 0x64;
    arg0->field_24             = 0;
    arg0->field_50             = &D_actor_105100_80141398;
    arg0->field_54             = (s32)work->field_49C;
    arg0->field_40             = D_actor_105100_80141398.hpMax;
    work->field_554.coord      = &arg1->field_2C->field_8[3];
    work->field_554.spawnArgLo = 0x500;
    work->field_554.spawnArgHi = 3;
    func_800B3F84((GpAnimCtx*)work, D_actor_105100_80141488, (TmdObject*)obj, work->field_30C,
                  ((Actor105100Anim*)work)->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_560       = coord->coord;
    work->field_594       = 0x2800;
    work->field_5A8       = 1;
    work->field_59E       = 0xF;
    work->field_59A       = 0x96;
    work->obj47C.coord    = &arg1->field_2C->field_8[3];
    records1              = work->field_49C;
    work->obj47C.ctx.recs = records1;
    work->obj47C.pos.vx   = 0;
    work->obj47C.pos.vy   = 0x1F4;
    work->obj47C.pos.vz   = 0;
    work->obj47C.key      = 0x30033;
    work->obj47C.radius   = 0x320;
    work->obj47C.flags    = 1U;
    Gp_LinkObj(2, &work->obj47C);
    Gp_InitRec18Table(records1, 3, 0);
    work->obj47C.flags    = (u16)(work->obj47C.flags | 0x8000);
    work->obj51C.coord    = arg1->field_2C->field_8;
    records2              = work->field_53C;
    work->obj51C.ctx.recs = records2;
    work->obj51C.pos.vx   = 0;
    work->obj51C.pos.vy   = 0;
    work->obj51C.pos.vz   = -0x12C;
    work->obj51C.key      = 0;
    work->obj51C.radius   = 0x4B0;
    work->obj51C.flags    = 1U;
    Gp_LinkObj(2, &work->obj51C);
    Gp_InitRec18Table(records2, 1, 0);
    work->obj51C.flags    = (u16)(work->obj51C.flags | 0x8000);
    work->obj4E4.coord    = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    records3              = work->field_504;
    work->obj4E4.ctx.recs = records3;
    work->obj4E4.pos.vx   = 0;
    work->obj4E4.pos.vy   = 0;
    work->obj4E4.pos.vz   = 0;
    work->obj4E4.key      = Gp_PackPair(&D_actor_105100_80141380, 5);
    work->obj4E4.radius   = 0x1F4;
    work->obj4E4.flags    = 1U;
    Gp_LinkObj(3, &work->obj4E4);
    Gp_InitRec18Table(records3, 1, 0);
    work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
    arg1->state        = 1;
}

void func_actor_105100_80132AA0(Actor105100Ctx* arg0, Actor105100* arg1)
{
    GsCOORDINATE2*    coord;
    Actor105100Obj2C* obj;
    Actor105100Work*  work;
    s32               state;
    s32               one;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->field_C     = 0;
    arg0->node.flags = 8;
    if (work->field_5BC != 0) {
        SndEvt_EnqueueType9(0x40000000);
        work->field_5BC = 0;
    }
    goto default_body;
case1:
    ActorsShared80134ff0((ActorShared80134ff0*)arg1);
    func_actor_105100_80136524(arg1);
    goto join_12;
case2:
    obj->field_C     = 0x80;
    arg0->node.flags = one;
join_12:
    SOFT_USE_REG(work);
    if (work->field_5BC == 0) {
        SndEvt_EnqueueType8(0x40000000);
    }
    work->field_5BC = state;
    return;
default_body:
    if (arg0->field_4C != 0) {
        func_actor_105100_80135E54(arg1);
    }
    func_actor_105100_80132C2C(arg1);
    func_actor_105100_80133134(arg1);
    if (work->field_5A2 != 0) {
        func_actor_105100_80133CE4(arg1);
    }
    func_actor_105100_80136408(arg1);
    func_actor_105100_80134130(arg1);
    ActorsShared80136574((ActorShared80136574*)arg1, &work->field_560, work->field_594, 1);
    if (work->field_5A8 != 0) {
        func_8018294C(arg1);
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    ActorsShared80134ff0((ActorShared80134ff0*)arg1);
    func_actor_105100_80136524(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80132C2C);

/// The enemy's step dispatcher, run every frame out of the `field_596` schedule
/// the three handlers below this one step through. Bit 3 of `Gp_StateF0.field_1D`
/// is a reset request: it is cleared here and the block is put back on step 6
/// with the schedule and the animation re-arm both dropped.
///
/// Step 7 is terminal -- `func_actor_105100_80136318` retires the enemy and the
/// task stops being dispatched -- so it falls straight through to the tail, as
/// does a step outside 0..7. The tail runs the shared post-hit reaction
/// (`D_8011540D` bit 2) and steps the `field_5AA` timer down while it is
/// positive. Step 0 also raises bit 1 of `Gp_StateF0.field_1D` once the HP drops
/// under the cap in `D_actor_105100_8014139C`.
void func_actor_105100_80133134(Actor105100* arg0)
{
    Actor105100Work* work;
    Actor105100Ctx*  ctx;
    s16              state;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (Gp_StateF0.field_1D & 8) {
        Gp_StateF0.field_1D &= 0xF7;
        work->field_596      = 6;
        work->field_598      = 0;
        work->field_5A8      = 0;
    }
    state = work->field_596;
    switch (state) {
        case 0:
            if ((s16)ctx->field_40 < (s32)*D_actor_105100_8014139C) {
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
    if (D_8011540D & 4) {
        func_actor_105100_80135FCC(arg0);
    }
    if (work->field_5AA > 0) {
        work->field_5AA = (u16)work->field_5AA - 1;
    }
}

/// The run of poses at 0x801413A8 this step's reroll picks from, one `s16`
/// entry per draw. Declared as an aggregate on purpose: a bare `extern u16`
/// makes `true_dependence` (`sched.c:846`) drop the dependence between the
/// entry load and the `sh` to `Actor105100Work::field_598`, and sched2 then
/// sinks that store past the `sw` of the LCG state instead of leaving the
/// lookup and the pose store adjacent at the end of the block.
extern u16 D_actor_105100_801413A8[16];

/// The enemy's aim-retry step, run every frame the schedule is on step 1.
///
/// The `field_59E` countdown at the top is the aim timer: it is stepped down
/// whenever the battle is not paused (`Gp_StateF0::field_0`), and on the frame
/// it runs out the arming state drops to 1 (aimed) and the 0xA-frame hold is
/// armed through `D_80062735`. `field_5A8` is the pair of gate flags and is
/// tested as one word -- see `Actor105100Gate`.
///
/// The reroll itself is the LCG: the state advances, the pose is the table
/// entry the high nibble selects, and the `field_5B2` interval counter counts
/// attempts until it reaches 3, which sends the schedule on to step 3 (the
/// thrown pose) instead of back to the reroll.
void func_actor_105100_8013329C(Actor105100* arg0, Actor105100Ctx* arg1)
{
    Actor105100Work* work;
    Actor105100Gate* gate;
    s16              state;
    s16              pose;
    u16              timer;
    u16              count;

    work = arg0->field_1C;
    if (Gp_StateF0.field_0 == 0) {
        timer           = work->field_59E - 1;
        work->field_59E = timer;
        if ((timer << 16) <= 0) {
            Gp_ArmStateF0(1);
            D_80062735 = 0xA;
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

/// The enemy descriptor run at 0x80141464 the spawn below draws from. Declared
/// as a scalar rather than an aggregate on purpose: only its address is taken,
/// so the two words `Gp_SpawnEnemyFromTable` splits it into are the function's
/// addend, not a load this unit has to model.
extern TaskDesc D_actor_105100_80141464;

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
void func_actor_105100_8013345C(Actor105100* arg0, Actor105100Ctx* arg1)
{
    Actor105100Work* work;
    GsCOORDINATE2*   coord;
    s16              step;
    s32              pan;
    u16              spawnTimer;
    u16              aimTimer;
    u32              rnd;
    u32              spawnRnd;
    u32              resetRnd;

    work  = arg0->field_1C;
    step  = work->field_598;
    coord = arg0->field_2C->field_8;
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
                    Gp_SpawnEnemyFromTable(&D_actor_105100_80141464, 1, 0,
                                           (GpEnemy*)arg0->field_20);
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
                work->field_580 = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x40330004;
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

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801336B8);

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
void func_actor_105100_80133A14(Actor105100* arg0, Actor105100Ctx* arg1)
{
    Actor105100Work* work;
    GsCOORDINATE2*   self;
    SVECTOR          pos;
    s32              snd;
    u16              timer;
    u32              rnd;

    work = arg0->field_1C;
    self = arg0->field_2C->field_8;
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
            work->field_55C = Gp_SpawnEff(0x800601A9, arg0->field_2C->field_8, (s16)work->field_59A + 0xA, &pos);
            work->field_588 = ((arg0->field_20->field_8 >> 12) << 8) | 0x40330009;
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
                snd             = ((arg0->field_20->field_8 >> 12) << 8) | 0x4033000B;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
                Gp_SpawnPadLerp(0xF, 8, 0xFF);
            }
            if ((s16)work->field_59A == 0x5A) {
                snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x4033000A;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            }
            break;
        case 2:
            if ((s16)work->field_592 == 0xC) {
                work->field_55C     = NULL;
                work->obj4E4.flags |= 0x8000;
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
            } else {
                work->obj4E4.flags &= 0x7FFF;
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

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80133CE4);

void func_actor_105100_80134130(Actor105100* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor105100Work* work;
    GsCOORDINATE2*   self;
    GpAnimRec*       rec;

    work = arg0->field_1C;
    self = arg0->field_2C->field_8;
    rec  = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->obj38.prev);
    if (rec != NULL) {
        if (!(rec->flags & 0x20) && (work->field_5B8 & 0x20)) {
            snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x40330001;
            pan = (s8)Gp_GetObjPan(self);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
        }
        if (!(rec->flags & 0x10) && (work->field_5B8 & 0x10)) {
            snd  = ((arg0->field_20->field_8 >> 12) << 8) | 0x40330002;
            pan2 = (s8)Gp_GetObjPan(self);
            SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(self));
        }
        work->field_5B8 = (u16)(rec->flags & 0x30);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80134284);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801347D4);

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100", D_actor_105100_80131E90);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80134B00);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135278);

/// The per-frame handler the `state == 1` dispatch runs: it hands the reaction
/// `field_40` selects to one of the `80135674` / `801359B4` / `80135B40`
/// sub-handlers, retimes the pose every 6/0xB/0x10 frames of the countdown in
/// `field_48`, and ends the fight (`state = 2`) once that countdown, the work's
/// `field_24` and the parent's `field_5AC` all say so.
void func_actor_105100_801354E8(Actor105100Ctx* arg0, Actor105100* arg1)
{
    Actor105100Rec*  rec;
    Actor105100Work* parentWork;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;
    s16              timer;
    u16              count;

    rec        = (Actor105100Rec*)arg1->field_1C;
    parentWork = ((Actor105100*)arg1->parent)->field_1C;
    state      = D_801153F4;
    coord      = arg1->field_2C->field_8;
    one        = 1;

    if (state == one) {
        func_8017FC40(coord, 0x80, rec->field_4E);
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
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    timer = rec->field_48;
    if (timer < 6) {
        rec->field_4E = 0;
    } else if (timer < 0xB) {
        rec->field_4E = 1;
    } else if (timer < 0x10) {
        rec->field_4E = 2;
    }
    func_8017FC40(coord, 0x80, rec->field_4E);
    count         = (u16)rec->field_48 - 1;
    rec->field_48 = count;
    if ((count << 16) <= 0 || ((Actor105100Work*)rec)->field_24 != 0 ||
        parentWork->field_5AC == 0) {
        parentWork->field_5AE = parentWork->field_5AE - 1;
        arg1->state           = 2;
    }
done:
    return;
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135674);

/// Reaction 1's handler (`field_40 == 1`), which walks the model towards the
/// approach point `field_44` selects from `D_actor_105100_80141418`. The first
/// pass (`field_46 == 0`) builds the planar delta in 16 bytes of scratch,
/// normalises it into the record's own 0x38 vector and stores the step it then
/// travels per frame -- the delta's length over `obj38.field_10`; the second
/// (`field_46 == 1`) applies that step to the coordinate every frame.
void func_actor_105100_801359B4(Actor105100* arg0)
{
    Actor105100Work* work;
    Actor105100Rec*  rec;
    GsCOORDINATE2*   coord;
    VECTOR*          head;
    VECTOR*          vec;
    s16              state;
    s32              dx;
    s32              dz;
    s32              speed;

    head                  = *(VECTOR**)0x1F8003FC;
    vec                   = head - 1;
    *(VECTOR**)0x1F8003FC = vec;
    work                  = arg0->field_1C;
    rec                   = (Actor105100Rec*)work;
    state                 = rec->field_46;
    coord                 = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            vec->vx = D_actor_105100_80141418[rec->field_44].vx - coord->coord.t[0];
            vec->vy = 0;
            vec->vz = D_actor_105100_80141418[rec->field_44].vz - coord->coord.t[2];
            VectorNormalS(vec, (SVECTOR*)&work->obj38);
            dx                 = vec->vx;
            dz                 = vec->vz;
            speed              = SquareRoot0(dx * dx + dz * dz) / work->obj38.pos.vx;
            rec->field_46      = 1;
            work->obj38.pos.vz = speed;
            break;
        case 1:
            coord->coord.t[0] += (((SVECTOR*)&work->obj38)->vx * work->obj38.pos.vz) >> 12;
            coord->coord.t[2] += (((SVECTOR*)&work->obj38)->vz * work->obj38.pos.vz) >> 12;
            break;
    }
    *(VECTOR**)0x1F8003FC += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135B40);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135CEC);

void func_actor_105100_80135DF8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135E54);

/// Second step of the `field_598` schedule: arms pose 2 with the `field_59A`
/// timer at 0x3C frames, then, when the timer runs out, hands the pose back to
/// the schedule entry step and returns it to 0.
void func_actor_105100_80135F50(Actor105100* arg0)
{
    Actor105100Work* work;
    s32              state;
    u16              timer;

    work  = arg0->field_1C;
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

void func_actor_105100_80135FCC(Actor105100* arg0)
{
    Actor105100Ctx* enemy;
    GsCOORDINATE2*  coord;
    s32             snd;
    s32             pan;
    u16             hp;

    enemy                = arg0->field_20;
    coord                = arg0->field_2C->field_8;
    Gp_StateF0.field_1D &= 0xFB;
    hp                   = enemy->field_40 + 0x50;
    enemy->field_40      = hp;
    if (D_actor_105100_8014139C[0] < (s16)hp) {
        enemy->field_40 = D_actor_105100_8014139C[0];
    }
    func_800DA6E8(&enemy->node, -0x50, 0);
    Gp_SpawnEff(0x601AF, NULL, 0, NULL);
    snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x4033000C;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
}

/// Opening stage of the `field_598` schedule: arms pose 8, releases the held
/// effect slot and drops the `obj4E4.flags` pose bit, then waits on
/// `Gp_TickObjFlag2` before clearing the enemy's flag-2 bit. On the last stage
/// it waits out the `field_592` timer and returns the schedule to step 0.
void func_actor_105100_801360AC(Actor105100* arg0)
{
    Actor105100Work* work;
    Actor105100Ctx*  enemy;
    GpEffWork*       eff;
    s32              state;

    work  = arg0->field_1C;
    state = work->field_598;
    enemy = arg0->field_20;
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
            work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
            if (eff != NULL) {
                eff->task->state = 4;
                work->field_55C  = NULL;
            }
            if (work->field_5B6 == 0) {
                work->field_5AA = 0x1E;
            }
            break;
        case 1:
            if (Gp_TickObjFlag2((GpObj5D*)enemy) != 0) {
                work->field_58E  = 9;
                work->field_598  = 2;
                work->field_5C2  = 0;
                enemy->field_4C &= 0xFD;
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
void func_actor_105100_801361C4(Actor105100* arg0)
{
    Actor105100Work* work;
    GpEffWork*       eff;
    s32              state;

    work  = arg0->field_1C;
    state = work->field_598;
    switch (state) {
        case 0:
            work->field_58E = 6;
            work->field_598 = 1;
            work->field_5B4 = 0;
            work->field_5AC = 0;
            func_actor_105100_801362A0(arg0);
            eff                = work->field_55C;
            work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
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

void func_actor_105100_801362A0(Actor105100* arg0)
{
    Actor105100Work* work;
    s32              snd;

    work = arg0->field_1C;

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
/// once the count is spent it puts the enemy's HP (`Actor105100Ctx::field_40`)
/// at 1 and arms pose 6, leaving `state` alone.
///
/// The work block is read twice on purpose. The two loads do not CSE (the
/// `field_5B4` / `field_5AC` stores sit between them), and the first pointer is
/// still live at the tail for `obj4E4.flags` and `field_55C`, so the second one
/// needs a register of its own.
void func_actor_105100_80136318(Actor105100* arg0)
{
    Actor105100Work* work;
    Actor105100Work* sndWork;
    GpEffWork*       eff;
    s32              snd;

    work = arg0->field_1C;
    if (D_80073BA0 <= 0) {
        arg0->field_20->field_40 = 1;
        work->field_596          = 6;
        work->field_598          = 0;
        return;
    }

    work->field_5B4 = 0;
    work->field_5AC = 0;

    sndWork = arg0->field_1C;

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
    work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
    if (eff != NULL) {
        eff->task->state = 4;
        work->field_55C  = NULL;
    }

    arg0->state = 2;
}

void func_actor_105100_80136408(Actor105100* arg0)
{
    Actor105100Work* work;
    s32              i;
    s32              val;

    work = arg0->field_1C;
    i    = 1;
    if ((s16)work->field_58E != work->field_590) {
        work->field_590 = work->field_58E;
        work->field_592 = 0;
        val             = D_actor_105100_801414C8[(s16)work->field_58E];
        do {
            func_800B4114(work, i, (s16)work->field_58E, 0, val);
            i++;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        work->field_592 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}
