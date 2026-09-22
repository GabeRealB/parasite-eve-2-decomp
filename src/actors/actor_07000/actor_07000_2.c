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
