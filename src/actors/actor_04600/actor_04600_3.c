#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_104600.h"

void Actor04600_Fn00EC8(Task* arg0, s32 arg1);
void Actor04600_Fn00FD8(Task* arg0);
void Actor04600_Fn01604(Task* arg0, u8 arg1);

/// Live handler of the first enemy, dispatched on its stage `field_2C8`.
/// Stage 1 counts `field_2D0` down to an idle sound, re-rolled to 0x50..0xB3
/// frames with `field_2D6` picking the sound set, then walks: step length 0x14,
/// animation 2, a turn toward the player and a step of the root, with the
/// animation's frame count restarting at 0x1D. Stage 2 counts `field_2D4` up
/// and grows the scale factor by 0xC8 a frame; on the fifth frame the enemy is
/// killed through `Actor04600_Fn01604`, with a five-frame countdown, the death
/// phase reset, the task put into the stage's state and the HP cleared.
void Actor04600_Fn007B0(Task* arg0)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s16              mode;
    s32              soundId;
    u32              rng;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor104600Work*)arg0->work;
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
            Actor04600_Fn00FD8(arg0);
            Actor04600_Fn0272C(arg0);
            if ((s16)work->field_2BC >= 0x1D) {
                work->field_2BC = 0;
            }
            break;
        case 2:
            work->field_2D4 = work->field_2D4 + 1;
            work->field_2AC = work->field_2AC + 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                Actor04600_Fn01604(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = mode;
                enemy->hp           = 0;
            }
            break;
    }
}

/// Contact handler of the first enemy, with 0x4C bytes of scratch. The
/// `func_800E0C10` push-back from its contact table moves the root (response
/// 1) or restores the position the last step started from (response 2), and
/// the hit cooldown ticks down. Coming within 0x320 of the player moves a live
/// enemy to its dying stage. Each of the four contacts is then handled by
/// class: 0x10000 does the same, 0x20000 (outside the cooldown) either kills
/// the enemy outright on a critical roll or applies the damage, the id's side
/// effect, the hit effect and the id's cooldown, and 0x30000 pushes the root
/// out of the wall along the contact normal while the enemy walks. The table
/// is released, and a flagged hit on the third body's record clears that
/// body's 0x8000 bit.
void Actor04600_Fn00978(Task* arg0)
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
    Actor104600Work*           work;
    GsCOORDINATE2*             coord;
    void*                      scratchHead;
    Actor104600ContactScratch* scratch;
    Actor104600Work*           contact;

    work        = (Actor104600Work*)arg0->work;
    scratchHead = (void*)(*(u32*)0x1F8003FC -= 0x4C);
    enemy       = arg0->spawnArg2;
    object      = arg0->extra;
    SOFT_TOUCH_REG_USE(object, scratchHead);
    coord    = object->coords;
    scratch  = scratchHead;
    movement = func_800E0C10(work->rec154, &scratch->delta, 4, &scratch->result);
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
        id = contact->rec154[0].key;
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
                    if (Gp_RollEnemyChance(arg0->spawnArg2, contact->rec154[0].key, 0) != 0) {
                        Actor04600_Fn01604(arg0, 1U);
                        arg0->killCountdown = 5;
                        arg0->state         = damageState;
                        work->field_2B4     = 0;
                        enemy->hp           = -1;
                    } else {
                        func_800E2C78((GpObj40*)enemy, (s32)contact->rec154[0].key, (s32)damage, 0);
                        Actor04600_Fn00EC8(arg0, (s32)damage);
                        effect = Gp_GetIdParam0((s32)contact->rec154[0].key) & 0xFFFF;
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
                        Gp_SetObjFlag4((GpObj5C*)enemy, contact->rec154[0].key, 0);
                        goto effect_done;
                    effect_flag2:
                        Gp_SetObjFlag2((GpObj5D*)enemy, contact->rec154[0].key, 0);
                    effect_done:
                        if (enemy->hp > 0) {
                            func_800FDB18(Gp_GetIdParam1((s32)contact->rec154[0].key) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 1, NULL, &work->field_284);
                        }
                        hitCooldown = Gp_GetIdParam2((s32)contact->rec154[0].key);
                        if ((hitCooldown << 0x10) > 0) {
                            work->field_2CE = (s16)hitCooldown;
                        }
                    }
                }
                break;
            case 0x30000:
                wallDx              = coord->workm.t[0] - contact->rec154[0].point.vx;
                scratch->delta.vy.w = 0;
                scratch->delta.vx.w = wallDx;
                wallDz              = coord->workm.t[2] - contact->rec154[0].point.vz;
                scratch->delta.vz.w = wallDz;
                distance            = contact->rec154[0].depth - SquareRoot0((wallDx * wallDx) + (wallDz * wallDz));
                boundedDepth        = distance;
                if (distance <= 0) {
                    boundedDepth = 0;
                }
                SOFT_TOUCH_REG_USE(boundedDepth, distance);
                distance            = boundedDepth;
                scratch->delta.vx.w = (s32)(coord->workm.t[0] - contact->rec154[0].point.vx);
                normal              = &scratch->normal;
                scratch->delta.vy.w = (s32)(coord->workm.t[1] - contact->rec154[0].point.vy);
                scratch->delta.vz.w = (s32)(coord->workm.t[2] - contact->rec154[0].point.vz);
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
        contact = (Actor104600Work*)((u8*)contact + 0x18);
        if ((s32)contact < (s32)((u8*)work + 0x60))
            goto contact_loop;
    } while (0);
    Gp_ClearRec18Occupied(work->rec154);
    effectRec = &work->rec1D4;
    if ((work->field_2C8 != 0) && (Gp_FindRec18(effectRec, 0) != 0)) {
        work->obj1B4.flags = (u16)((u16)work->obj1B4.flags & 0x7FFF);
        Gp_ClearRec18Occupied(effectRec);
    }
    *(u32*)0x1F8003FC += 0x4C;
}

/// Damage reaction of the first enemy: `arg1` comes off its HP and goes
/// through `func_800DA6E8`. A depleted enemy is killed through
/// `Actor04600_Fn01604` and put into its death state with a five-frame
/// countdown. A live one plays the hurt sound from the set `field_2D6` picks,
/// re-arms `field_2CC`, and while animation 1 plays latches `field_2D8`.
void Actor04600_Fn00EC8(Task* arg0, s32 arg1)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              anim;
    s32              soundId;

    enemy      = arg0->spawnArg2;
    obj        = arg0->extra;
    coord      = obj->coords;
    work       = (Actor104600Work*)arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp < 0) {
        Actor04600_Fn01604(arg0, 0);
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
