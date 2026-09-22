#include "actors/actors_shared_80134810.h"
#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actor_107000_anim.h"
#include "actors/actors_shared_8013454c.h"
#include "actors/actors_shared_8014ca28.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void Actor07000_Fn00654(Task* arg0);
void Actor07000_Fn016A8(Task* arg0, u8 arg1);
void Actor07000_Fn0107C(Task* arg0);

/// Node 3's pair table, packed by `Gp_PackPair` into `obj1B4`, and the enemy
/// record whose `pairTable` points at it; its `hpMax` seeds the enemy's
/// `field_40`.
extern GpU16Pair  Actor07000_D06924;
extern GpPairSrcE Actor07000_D06928;

/// Message dispatch table the spawn parks in `Task::msgTable`.
extern u8 Actor07000_D08030[];

/// The animation data `func_800B3F84` seeds the work block's slots from.
extern u8 Actor07000_D08058[];

/// Offset the collapse arms spawn the 0x60080 effect at.
extern SVECTOR Actor07000_D08070;

extern u32     Actor07000_D06938[];
extern u32     Actor07000_D06944[];
extern SVECTOR Actor07000_D08068;

// actor_104600 (func_actor_104600_80131E68), actor_204600
// (func_actor_204600_80149E68) and actor_207000 (func_actor_207000_80149F0C)
// carry the same body, refused promotion for the reason its sibling below is:
// the pair table, the animation bank and the node-3 record it names -
// Actor07000_D06924, Actor07000_D06928 and
// Actor07000_D08058 - are this overlay's own data, so one shared object
// could not link into the other three.

/// Spawn handler of the specimen, the `GpEnemyTaskFunc` the task dispatch runs
/// first: it allocates the `Actor107000SpawnWork` block, wires the enemy's four
/// `GpObj` render nodes and their `GpRec18` tables into it and hands the task
/// over to `ActorsShared8014ca28`. The spawn arg's high halfword is the variant
/// the model was spawned as - when it is 1 the specimen is killed instead, and
/// the same halfword plus the low one seed `field_2DC`/`field_2D6`. Variant 1
/// with a matching `spawnType` is the one that carries a streamed model: its
/// texture page and CLUT row are stepped before the model is re-streamed twice.
void Actor07000_Fn000EC(GpEnemy* arg0, Task* arg1)
{
    Actor107000SpawnWork* work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part;
    u16                   v;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    if ((s16)(arg1->spawnArg1 >> 16) == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_DC;
    obj->colorMtx  = &work->field_BC;
    arg0->field_4  = &coord[1].coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = part;
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &Actor07000_D06928;
    arg0->recs       = &work->rec154[0];
    arg0->hp         = Actor07000_D06928.hpMax;
    func_800B3F84((GpAnimCtx*)work, Actor07000_D08058, obj, work->field_8C,
                  (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8      = 1;
    work->field_2BA      = 1;
    work->field_2AC      = 0x1000;
    work->field_2DA      = 0;
    work->field_2CE      = 0;
    work->field_2D4      = 0;
    work->field_2D2      = 0;
    work->field_2CC      = 0;
    arg1->killCountdown  = 0;
    work->field_284      = &((TmdObject*)arg1->extra)->coords[1];
    work->field_288      = 0x100;
    work->field_28A      = 1;
    work->objFC.coord    = coord;
    work->objFC.ctx.recs = &work->rec11C;
    work->objFC.pos.vx   = 0;
    work->objFC.pos.vy   = 0;
    work->objFC.pos.vz   = 0;
    work->objFC.key      = 0;
    work->objFC.radius   = 0xBB8;
    work->objFC.flags    = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.coord    = coord;
    work->obj134.ctx.recs = &work->rec154[0];
    work->obj134.pos.vx   = 0;
    work->obj134.pos.vy   = -0xC8;
    work->obj134.pos.vz   = 0;
    work->obj134.key      = 0x3002E;
    work->obj134.radius   = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags | 0x8000);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord    = coord;
    work->obj1B4.ctx.recs = &work->rec1D4;
    work->obj1B4.pos.vx   = 0;
    work->obj1B4.pos.vy   = 0;
    work->obj1B4.pos.vz   = 0;
    work->obj134.flags    = (u16)(work->obj134.flags | 0xC200);
    work->obj1B4.key      = Gp_PackPair(&Actor07000_D06924, 0);
    work->obj1B4.radius   = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.coord    = coord;
    work->obj1EC.ctx.recs = &work->rec20C;
    work->obj1EC.pos.vx   = 0;
    work->obj1EC.pos.vy   = 0;
    work->obj1EC.pos.vz   = 0;
    work->obj1EC.key      = 0x22323;
    work->obj1EC.radius   = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    work->field_2DC    = (s16)(arg1->spawnArg1 >> 16);
    v                  = (u16)arg1->spawnArg1;
    work->field_2D6    = v;
    if ((s16)v == 1 && arg1->spawnType == (s16)v) {
        obj->tpage = obj->tpage + 1;
        obj->clut  = obj->clut + 1;
        if (obj->buffer != 0) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    work->field_2B4    = 0;
    arg1->exitCallback = ActorsShared8014ca28;
    arg1->state       += 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_07000/actor_107000", ActorsShared80135df4Table);

INCLUDE_RODATA("actors/nonmatchings/actor_07000/actor_107000", Actor07000_D00010);

// actor_104600 (func_actor_104600_801321F4), actor_204600
// (func_actor_204600_8014A1F4) and actor_207000 (func_actor_207000_8014A298)
// carry the same body, refused promotion for the reason its sibling below is:
// its three remaining calls - Actor07000_Fn00654,
// Actor07000_Fn00854 and ActorsShared80134810 - and the effect
// offset it spawns at are named in this overlay only, so one shared object
// could not link into the other three.

/// Per-frame dispatch of the caged specimen, on the reaction state in
/// `field_2B2`: 0 is the dormant arm `Actor07000_Fn00654` and 1 the
/// live handler `Actor07000_Fn00854`. 3 is the arm the reaction
/// dispatch shoots when the enemy's flag byte carries bit 0x2 - it suppresses
/// the rebind, waits out the generic flag-2 helper on the spawn arg and, once
/// that expires, wakes the specimen: the rebind is released and
/// `field_2B2`/`field_2C8` move to 1, the live stage. The arm ends in
/// `ActorsShared8013454c` either way.
///
/// 4 and 5 are the two collapse arms. Both drive the model's second coordinate
/// through `ActorsShared80134810`, count `field_2BC` up and spawn the
/// 0x60080 effect on the model's coordinate every 0x10 frames; 5 also counts
/// `field_2D4` and, on the third count, writes the same death sequence the
/// reaction dispatch does - a five-frame countdown, `field_2B4` cleared and the
/// task moved to state 2 - with the spawn arg's `field_40` cleared alongside.
/// Both arms end by re-suppressing the rebind, and the join the compiler builds
/// from their two assignments is what the original binary shows.
void Actor07000_Fn00478(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    u16              frames;

    work = (Actor107000Work*)arg0->work;
    switch (work->field_2B2) {
        case 0:
            Actor07000_Fn00654(arg0);
            return;
        case 1:
            Actor07000_Fn00854(arg0);
            return;
        case 3:
            work->field_2D2 = 1;
            if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
                work->field_2D2 = 0;
                work->field_2B2 = 1;
                work->field_2C8 = 1;
                work->field_2BE = 0;
            }
            ActorsShared8013454c(arg0);
            return;
        case 4:
            work->field_2AC = 0x1000;
            ActorsShared80134810(arg0, &((TmdObject*)arg0->extra)->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->coords, 0x400, &Actor07000_D08070);
                work->field_2BC = 0;
            }
            goto suppress_rebind;
        default:
            return;
        case 5:
            work->field_2AC = 0x1000;
            ActorsShared80134810(arg0, &((TmdObject*)arg0->extra)->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->coords, 0x400, &Actor07000_D08070);
                work->field_2BC = 0;
                frames          = work->field_2D4 + 1;
                work->field_2D4 = frames;
                if ((s16)frames >= 3) {
                    enemy               = arg0->spawnArg2;
                    arg0->killCountdown = 5;
                    work->field_2B4     = 0;
                    arg0->state         = 2;
                    enemy->hp           = 0;
                }
            }
        suppress_rebind:
            work->field_2D2 = 1;
    }
}

// actor_104600 (func_actor_104600_801323D0), actor_204600
// (func_actor_204600_8014A3D0) and actor_207000 (func_actor_207000_8014A474)
// carry the same body. Unlike the siblings around it, nothing this one names is
// overlay-local - every call it makes is already shared, so one shared object
// *would* link into all four carriers. It is the span that cannot be placed:
// 0x654..0x854 sits inside this overlay's first code unit, and this overlay and
// actor_207000 already carry a hand-placed `rodata` cut, which is the layout
// `bulk_m2c_promote.py` refuses to re-derive (`already has hand-placed
// rodata/units cuts; splitting a unit there needs them re-derived by hand`).
// So the body stays matched in each carrier until that pass is done with a
// person re-deriving the cut.

/// Dormant arm of the caged specimen: the `field_2B2 == 0` arm of the per-frame
/// dispatch, run while the specimen is still caged. The collision record at
/// `field_11C` is polled for a 0x10000-kind occupant and, when one appears,
/// `field_2D8` latches - that latch is what wakes the specimen. It moves the
/// reaction stage and the animation to the live handler's, clears the render
/// node's 0x8000 flag, and arms the global state, which is the same three
/// writes the spawn handler's own node setup makes. The record is released
/// either way.
///
/// While the work plays animation 1 the arm also runs the sound cue the live
/// handler runs: `field_2D0` is counted down and, when it expires, re-rolled
/// from `Gp_LcgState` as `(state >> 16) % 100 + 0x50` frames - between 0x50 and
/// 0xB3 - with `field_2D6` picking between the two half-ids and the model's
/// pan and depth passed alongside. The whole assignment *and* its call are
/// written out in both arms, the way `Actor07000_Fn00854` writes them.
/// `field_2BE` is then re-armed from the frames spent on the current id (see
/// the two 0x29-frame windows below) and the id is restarted once it has spent
/// 0x63 frames.
void Actor07000_Fn00654(Task* arg0)
{
    Actor107000Work* work;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s32              soundId;
    u32              rng;

    coord              = ((TmdObject*)arg0->extra)->coords;
    work               = (Actor107000Work*)arg0->work;
    *(u32*)0x1F8003FC -= 8;
    if (Gp_CountRec18Hi(&work->field_11C, 0x10000) != 0) {
        work->field_2D8 = 1;
    }
    if (work->field_2D8 != 0) {
        work->field_2B2 = 1;
        work->field_2C8 = 1;
        work->field_11A = (u16)(work->field_11A & 0x7FFF);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(&work->field_11C);
    if (work->field_2B8 == 1) {
        countdown       = work->field_2D0 - 1;
        work->field_2D0 = countdown;
        if ((countdown << 16) <= 0) {
            rng             = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState     = rng;
            work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
            if (work->field_2D6 != 0) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460009;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x402E0001;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
        work->field_2C6 = 1;
        work->field_2BE = 0;
        if ((u32)(work->field_2BC - 1) < 0x29) {
            work->field_2BE = 0x14;
        }
        if ((u32)(work->field_2BC - 0x33) < 0x29) {
            work->field_2BE = -0x14;
        }
        if ((s16)work->field_2BC >= 0x63) {
            work->field_2BC = 0;
        }
        ActorsShared8013454c(arg0);
    }
    *(u32*)0x1F8003FC += 8;
}

// actor_104600 (func_actor_104600_801325D0), actor_204600
// (func_actor_204600_8014A5D0) and actor_207000 (func_actor_207000_8014A674)
// carry the same body, refused promotion because both of its remaining calls -
// Actor07000_Fn0107C and Actor07000_Fn016A8 - are named in this
// overlay only, so one shared object could not link into the other three.

/// Per-frame handler of the caged specimen, dispatched on the reaction stage in
/// `field_2C8`: 1 is the live specimen, 2 the death throes. Every stage ends in
/// the shared epilogue, so the switch's default is a jump straight there.
///
/// Stage 1 ticks `field_2D0` down and, when it runs out, re-rolls it from
/// `Gp_LcgState` as `(state >> 16) % 100 + 0x50` frames - between 0x50 and 0xB3.
/// The re-roll also cues a sound event: `field_2D6` picks between the two
/// half-ids, the actor id in bits 12+ of the context's `field_8` supplies the
/// sound bank, and the pan and depth of the model's coordinate are passed
/// alongside. As in `Actor07000_Fn00F6C`, the whole assignment *and* its
/// call are written out in both arms - the join the compiler builds from them is
/// what the original binary shows. The stage then re-arms `field_2BE`, switches
/// the animation to 2, runs the frame through `Actor07000_Fn0107C` and
/// `ActorsShared8013454c`, and restarts `field_2BC` once it has spent 0x1D
/// frames on the id.
///
/// Stage 2 counts `field_2D4` up and advances `field_2AC` by 0xC8 a frame; on
/// the fifth frame the specimen is killed - `Actor07000_Fn016A8` runs
/// with a zero argument, the kill countdown is armed to 5, the reaction flag
/// `field_2B4` is cleared, the task state latches the stage it just ran, and the
/// enemy's `field_40` HP is zeroed.
void Actor07000_Fn00854(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s16              mode;
    s32              soundId;
    u32              rng;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor107000Work*)arg0->work;
    enemy = arg0->spawnArg2;
    mode  = work->field_2C8;
    switch (mode) {
        case 1:
            countdown       = work->field_2D0 - 1;
            work->field_2D0 = countdown;
            if ((countdown << 16) <= 0) {
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
                if (work->field_2D6 != 0) {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460009;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0001;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
            }
            work->field_2BE = 0x14;
            work->field_2B8 = 2;
            Actor07000_Fn0107C(arg0);
            ActorsShared8013454c(arg0);
            if ((s16)work->field_2BC >= 0x1D) {
                work->field_2BC = 0;
            }
            break;
        case 2:
            work->field_2D4 = work->field_2D4 + 1;
            work->field_2AC = work->field_2AC + 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                Actor07000_Fn016A8(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = mode;
                enemy->hp           = 0;
            }
            break;
    }
}

void Actor07000_Fn00A1C(Task* arg0)
{
    s32                        damageState;
    TmdObject*                 object;
    GpEnemy*                   enemy;
    GpRec18*                   effectRec;
    VECTOR*                    normal;
    VECTOR*                    delta;
    s16                        cooldown;
    s32                        stage;
    s32                        contactStage;
    s32                        effect;
    s32                        pushY;
    s32                        movement;
    s32                        dx;
    s32                        dz;
    s32                        wallDx;
    s32                        wallDz;
    s32                        hitCooldown;
    s32                        boundedDepth;
    s32                        distance;
    s32                        z;
    u32                        id;
    u32                        damage;
    Actor107000Work*           work;
    GsCOORDINATE2*             coord;
    void*                      scratchHead;
    Actor107000ContactScratch* scratch;
    Actor107000Work*           contact;

    work        = (Actor107000Work*)arg0->work;
    scratchHead = (void*)(*(u32*)0x1F8003FC -= 0x4C);
    enemy       = arg0->spawnArg2;
    object      = arg0->extra;
    SOFT_TOUCH_REG_USE(object, scratchHead);
    coord    = object->coords;
    scratch  = scratchHead;
    movement = func_800E0C10(work->field_154, &scratch->delta, 4, &scratch->result);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] = (s32)(coord->coord.t[0] + scratch->delta.vx.h.hi);
            coord->coord.t[1] = (s32)(coord->coord.t[1] + scratch->delta.vy.h.hi);
            z                 = coord->coord.t[2] + scratch->delta.vz.h.hi;
            coord->coord.t[2] = z;
            break;
        case 2:
            coord->coord.t[0] = (s32)work->field_274.vx;
            coord->coord.t[1] = (s32)work->field_274.vy;
            z                 = work->field_274.vz;
            coord->coord.t[2] = z;
            break;
    }
    if (work->field_2CE != 0) {
        cooldown        = (u16)work->field_2CE - 1;
        work->field_2CE = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_2CE = 0;
        }
    }
    dx                  = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vx.w = dx;
    scratch->delta.vy.w = (s32)(Player_Status.coordMtx->t[1] - coord->coord.t[1]);
    dz                  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    scratch->delta.vz.w = dz;
    distance            = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x320) {
        stage = work->field_2C8;
        delta = (VECTOR*)&scratch->delta;
        if (stage == 1) {
            work->field_2D2 = stage;
            work->field_2C8 = 2;
            goto delta_ready;
        }
    } else {
    delta_ready:
        delta = (VECTOR*)&scratch->delta;
    }
    damageState = 2;
    contact     = work;
contact_loop:
    do {
        id = contact->field_154[0].key;
        switch (id & 0xFFFF0000) {
            case 0x10000:
                contactStage = work->field_2C8;
                if (contactStage == 1) {
                    work->field_2D2 = contactStage;
                    work->field_2C8 = 2;
                }
                break;
            case 0x20000:
                if (work->field_2CE == 0) {
                    damage = Gp_ComputeDamage(id, (u32)distance, 0, 0);
                    if (Gp_RollEnemyChance(arg0->spawnArg2, contact->field_154[0].key, 0) != 0) {
                        Actor07000_Fn016A8(arg0, 1U);
                        arg0->killCountdown = 5;
                        arg0->state         = damageState;
                        work->field_2B4     = 0;
                        enemy->hp           = -1;
                    } else {
                        func_800E2C78((GpObj40*)enemy, (s32)contact->field_154[0].key, (s32)damage, 0);
                        Actor07000_Fn00F6C(arg0, (s32)damage);
                        effect = Gp_GetIdParam0((s32)contact->field_154[0].key) & 0xFFFF;
                        if (effect == damageState)
                            goto effect_flag2;
                        if (effect < 3) {
                            if (effect == 1)
                                goto effect_react;
                        } else {
                            if (effect == 3)
                                goto effect_flag4;
                            if (effect == 9)
                                goto effect_flag2;
                        }
                        goto effect_done;
                    effect_react:
                        work->field_2D2 = effect;
                        work->field_2C8 = damageState;
                        goto effect_done;
                    effect_flag4:
                        Gp_SetObjFlag4((GpObj5C*)enemy, contact->field_154[0].key, 0);
                        goto effect_done;
                    effect_flag2:
                        Gp_SetObjFlag2((GpObj5D*)enemy, contact->field_154[0].key, 0);
                    effect_done:
                        if (enemy->hp > 0) {
                            func_800FDB18(Gp_GetIdParam1((s32)contact->field_154[0].key) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 1, NULL, &work->field_284);
                        }
                        hitCooldown = Gp_GetIdParam2((s32)contact->field_154[0].key);
                        if ((hitCooldown << 0x10) > 0) {
                            work->field_2CE = (s16)hitCooldown;
                        }
                    }
                }
                break;
            case 0x30000:
                wallDx              = coord->workm.t[0] - contact->field_154[0].point.vx;
                scratch->delta.vy.w = 0;
                scratch->delta.vx.w = wallDx;
                wallDz              = coord->workm.t[2] - contact->field_154[0].point.vz;
                scratch->delta.vz.w = wallDz;
                distance            = contact->field_154[0].depth - SquareRoot0((wallDx * wallDx) + (wallDz * wallDz));
                boundedDepth        = distance;
                if (distance <= 0) {
                    boundedDepth = 0;
                }
                SOFT_TOUCH_REG_USE(boundedDepth, distance);
                distance            = boundedDepth;
                scratch->delta.vx.w = (s32)(coord->workm.t[0] - contact->field_154[0].point.vx);
                normal              = &scratch->normal;
                scratch->delta.vy.w = (s32)(coord->workm.t[1] - contact->field_154[0].point.vy);
                scratch->delta.vz.w = (s32)(coord->workm.t[2] - contact->field_154[0].point.vz);
                VectorNormal(delta, normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, delta);
                if ((u32)((u16)work->field_2B8 - 1) < 2U) {
                    coord->coord.t[0] = (s32)(coord->coord.t[0] + ((s32)(distance * scratch->delta.vx.w) >> 0xC));
                    pushY             = distance * scratch->delta.vy.w;
                    if (pushY < 0) {
                        coord->coord.t[1] = (s32)(coord->coord.t[1] + (pushY >> 0xC));
                    }
                    coord->coord.t[2] = (s32)(coord->coord.t[2] + ((s32)(distance * scratch->delta.vz.w) >> 0xC));
                }
                break;
        }
        contact = (Actor107000Work*)((u8*)contact + 0x18);
        if ((s32)contact < (s32)((u8*)work + 0x60))
            goto contact_loop;
    } while (0);
    Gp_ClearRec18Occupied(work->field_154);
    effectRec = &work->field_1D4;
    if ((work->field_2C8 != 0) && (Gp_FindRec18(effectRec, 0) != 0)) {
        work->field_1D2 = (u16)((u16)work->field_1D2 & 0x7FFF);
        Gp_ClearRec18Occupied(effectRec);
    }
    *(u32*)0x1F8003FC += 0x4C;
}

/// Damage reaction of the caged specimen. `arg1` is taken off the context's
/// HP, the same amount is pushed through the lock-slot updater, and a depleted
/// specimen switches the task to its death state (2) with a five-frame
/// countdown while `field_2B4` is cleared on the work.
///
/// A live one cues a sound event instead: `field_2D6` picks between the two
/// half-ids, the actor id in bits 12+ of the context's `field_8` supplies the
/// sound bank, and the pan and depth of the model's coordinate are passed
/// alongside. The same call statement is written out in both arms - the join
/// the compiler builds from it is what the original binary shows.
///
/// `field_2CC` is then re-armed and, for the id the animation is playing
/// (`field_2B8 == 1`), latched into `field_2D8`.
void Actor07000_Fn00F6C(Task* arg0, s32 arg1)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              anim;
    s32              soundId;

    enemy      = arg0->spawnArg2;
    obj        = arg0->extra;
    coord      = obj->coords;
    work       = (Actor107000Work*)arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp < 0) {
        Actor07000_Fn016A8(arg0, 0);
        arg0->state         = 2;
        arg0->killCountdown = 5;
        work->field_2B4     = 0;
        return;
    }
    if (work->field_2D6 != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000A;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    } else {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0002;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
    anim            = work->field_2B8;
    work->field_2CC = 0xF;
    if (anim == 1) {
        work->field_2D8 = anim;
    }
}

// actor_104600 (func_actor_104600_80132DF8), actor_204600
// (func_actor_204600_8014ADF8) and actor_207000 (func_actor_207000_8014AE9C)
// carry the same body. It is promotable - its only externals are `Player_Status`
// and the libgte pair, none of them overlay-local - but the shared span
// renumbers every unit after it in this overlay and in the three carriers, and
// splat never rewrites a `.c` that exists, so it needs the pass that rehomes
// the carriers' `INCLUDE_*` lines at the same time.

/// Turns the caged specimen toward the player, one 0x20 step a frame. The
/// 0x18-byte scratch the model's heading is rebuilt in carries the XZ offset
/// from the model's coordinate to the player (`Player_Status.coordMtx`), and the
/// heading `field_2B0` is re-steered against it: at most 0x21 off, the target
/// heading is taken outright; otherwise the field is stepped one 0x20 toward it,
/// with a difference of 0x801 or more first folded through the 0x1000 wrap so
/// the specimen turns the short way round. The result is written back as the
/// scratch's Y rotation and `RotMatrix` rebuilds the coordinate from the
/// scratch's angle triple.
void Actor07000_Fn0107C(Task* arg0)
{
    Actor107000Work*       work;
    GsCOORDINATE2*         coord;
    Actor107000RotScratch* sc;
    s16                    cur;
    s32                    want;
    s16                    diff;
    s32                    adiff;
    s16                    turn;
    s16                    wrap;
    s32                    current;

    coord      = ((TmdObject*)arg0->extra)->coords;
    work       = (Actor107000Work*)arg0->work;
    sc         = (Actor107000RotScratch*)(*(u32*)0x1F8003FC -= 0x18);
    sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->vec.vy = 0;
    sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
    cur        = work->field_2B0 & 0xFFF;
    diff       = want - cur;
    adiff      = diff >= 0 ? diff : -diff;
    turn       = diff;
    if (adiff < 0x21) {
        work->field_2B0 = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = work->field_2B0;
        if (turn <= 0) {
            cur = current - 0x20;
        } else {
            cur = current + 0x20;
        }
        work->field_2B0 = cur;
    }
    sc->rot.vx = 0;
    sc->rot.vy = work->field_2B0;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    *(u32*)0x1F8003FC += 0x18;
}

void Actor07000_Fn011B4(GpEnemy* enemy, Task* task)
{
    TmdObject*       model;
    Actor107000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj   = task->extra;
    work  = (Actor107000Work*)task->work;
    coord = obj->coords;
    model = obj;
    switch (D_801153F4) {
        case 1:
            break;
        case 2:
            model->flags     |= 0x80;
            enemy->node.flags = 1;
            break;
        case 0:
        default:
            switch (work->field_2B4) {
                case 0:
                    work->field_20A &= 0x7FFF;
                    work->field_2AC -= 0x12C;
                    task->killCountdown--;
                    if ((u32)((u16)work->field_2B2 - 5) >= 2 && task->killCountdown == 3) {
                        model->flags = 0x80;
                    }
                    if (work->field_2B2 == 6) {
                        work->field_2B8 = 1;
                        Actor107000_TickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->field_2D6 != 0) {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x4046000D;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        } else {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x402E0005;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        }
                        task->killCountdown = 0;
                        Gp_ReleaseStateF0Add(task, 0x2E);
                        if (work->field_2DA != 0) {
                            Gp_SpawnEff(0x6009E, ((TmdObject*)task->extra)->coords, 0, NULL);
                        }
                        work->field_2B4 = 1;
                        work->field_2B6 = 0;
                        work->field_2CA = 0x1000;
                        work->field_28C = coord->coord;
                        enemy->recs     = NULL;
                        Gp_UnlinkNode(&enemy->node);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->objFC);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj134);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj1B4);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj1EC);
                    }
                    break;
                case 1:
                    if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                        work->field_2B4 = 2;
                    }
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        work->field_2B4 = 2;
                    }
                    ActorsShared801349d8(task);
                    if (work->field_2B6 == 0xA) {
                        ((TmdObject*)task->extra)->flags = 2;
                    }
                    break;
                case 2:
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        Gp_DestroyEnemy(enemy, task);
                    }
                    return;
            }
            if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                Actor107000_TickAnim(task);
                ActorsShared80134810(task, &((TmdObject*)task->extra)->coords[1]);
                ((TmdObject*)task->extra)->coords[0].flg = 0;
                ((TmdObject*)task->extra)->coords[1].flg = 0;
                Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
                Actor107000_UpdateColor(enemy, &((TmdObject*)task->extra)->coords[1]);
            }
            break;
    }
}

void Actor07000_Fn016A8(Task* arg0, u8 arg1)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj         = arg0->extra;
    enemy       = arg0->spawnArg2;
    work        = (Actor107000Work*)arg0->work;
    coord       = obj->coords;
    enemy->hp   = 0;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((Gp_LcgState >> 0x10) & 2) || (arg1 & 0xFF)) {
        if (work->field_2D6 != 0) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000B;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0003;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_1D2 |= 0x8000;
        work->field_20A |= 0x8000;
        Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords, 1, NULL);
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x300, &Actor07000_D08068);
        Gp_SpawnScript18((s32)&Actor07000_D06938, (s32)&Actor07000_D06944);
        work->field_2DA = 1;
    } else {
        if (work->field_2D6 != 0) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000C;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0004;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_2B2 = 6;
    }
}

// actor_104600 (func_actor_104600_801335EC), actor_204600
// (func_actor_204600_8014B5EC) and actor_207000 (func_actor_207000_8014B690)
// carry the same body, refused promotion because it reads its own overlay's
// data - the two pair records, the animation source and the message table are
// named per overlay, so one shared object could not resolve them in the other
// three.

/// Spawn/setup handler of the caged specimen, entry 0 of
/// `Actor07000_D00010`. A task already on this handler (`spawnArg1`'s
/// high halfword reads 1) is torn down instead of spawned.
///
/// Otherwise it allocates the 0x2E4-byte work block and hangs it off the task:
/// the model's coordinate array feeds `field_18` with its second element, the
/// context's `field_4` with that element's matrix, and the block's two
/// `MATRIX`es become the model's colour and light matrices. The work's own
/// collision record is re-rolled from the same coordinate, and the four list
/// nodes are linked into the global object lists with their `GpRec18` tables -
/// the first three leave the 0x8000 last-element bit clear, and the second
/// node's 0x4000 bit is cleared once the third has been built.
///
/// The animation context is then seeded from `func_800B3F84` over the three
/// slots, slots 1 and 2 are reset, and the task moves to handler 3.
void Actor07000_Fn01870(GpEnemy* arg0, Task* arg1)
{
    Actor107000SpawnWork* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part;
    TmdObject*            obj;
    s32                   one;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    one   = 1;
    if ((s16)(arg1->spawnArg1 >> 16) == one) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    work->field_2DC = (s16)(arg1->spawnArg1 >> 16);
    work->field_2D6 = (u16)arg1->spawnArg1;
    obj->flags      = 0x80;
    coord->flg      = 0;
    obj->lightMtx   = &work->field_DC;
    obj->colorMtx   = &work->field_BC;
    arg0->field_4   = &coord[1].coord;
    arg0->field_48  = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = part;
    arg0->node.flags = one;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &Actor07000_D06928;
    arg0->recs       = &work->rec154[0];
    arg0->hp         = Actor07000_D06928.hpMax;
    func_800B3F84((GpAnimCtx*)work, Actor07000_D08058, obj, work->field_8C,
                  (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8      = 1;
    work->field_2BA      = 1;
    work->field_2AC      = 0x1000;
    work->field_2DA      = 0;
    work->field_2CE      = 0;
    work->field_2D4      = 0;
    work->field_2D2      = 0;
    work->field_2CC      = 0;
    arg1->killCountdown  = 0;
    work->field_284      = &((TmdObject*)arg1->extra)->coords[1];
    work->field_288      = 0x100;
    work->field_28A      = 1;
    work->objFC.coord    = coord;
    work->objFC.ctx.recs = &work->rec11C;
    work->objFC.pos.vx   = 0;
    work->objFC.pos.vy   = 0;
    work->objFC.pos.vz   = 0;
    work->objFC.key      = 0;
    work->objFC.radius   = 0xBB8;
    work->objFC.flags    = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.coord    = coord;
    work->obj134.ctx.recs = &work->rec154[0];
    work->obj134.pos.vx   = 0;
    work->obj134.pos.vy   = -0xC8;
    work->obj134.pos.vz   = 0;
    work->obj134.key      = 0x3002E;
    work->obj134.radius   = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags & 0x7FFF);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord    = coord;
    work->obj1B4.ctx.recs = &work->rec1D4;
    work->obj1B4.pos.vx   = 0;
    work->obj1B4.pos.vy   = 0;
    work->obj1B4.pos.vz   = 0;
    work->obj134.flags    = (u16)(work->obj134.flags & 0x3DFF);
    work->obj1B4.key      = Gp_PackPair(&Actor07000_D06924, 0);
    work->obj1B4.radius   = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.coord    = coord;
    work->obj1EC.ctx.recs = &work->rec20C;
    work->obj1EC.pos.vx   = 0;
    work->obj1EC.pos.vy   = 0;
    work->obj1EC.pos.vz   = 0;
    work->obj1EC.key      = 0x22323;
    work->obj1EC.radius   = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->field_2E2    = 0;
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    arg1->msgTable     = Actor07000_D08030;
    arg1->state        = 3;
}

INCLUDE_RODATA("actors/nonmatchings/actor_07000/actor_107000", Actor07000_D0003C);
INCLUDE_RODATA("actors/nonmatchings/actor_07000/actor_107000", ActorsShared801385d4Table);
