#include "common.h"

#include "actors/actor_101100.h"
#include "actors/actors_shared_801384ac.h"
#include "actors/actors_shared_801385e0.h"
#include "actors/actors_shared_80138774.h"
#include "actors/actors_shared_801388e8.h"
#include "actors/actors_shared_80138efc.h"
#include "actors/actors_shared_801511c8.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"

#include <psyq/inline_c.h>

extern u32       Gp_LcgState;
extern u8        Actor01100_D15660[];
extern GpU16Pair Actor01100_D074D0[];

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix with no translation
/// vector added. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
/// `gpf 1`: scale IR1..3 by IR0. Same reason as above for spelling out the word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

#define ACTOR_COPY_MATRIX_COLUMN_TO_SV(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, %2(%0);"                                 \
        "lhu $13, %3(%0);"                                 \
        "lhu $14, %4(%0);"                                 \
        "sh $12, 0(%1);"                                   \
        "sh $13, 2(%1);"                                   \
        "sh $14, 4(%1)"                                    \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

#define ACTOR_COPY_SV_TO_MATRIX_COLUMN(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, 0(%0);"                                  \
        "lhu $13, 2(%0);"                                  \
        "lhu $14, 4(%0);"                                  \
        "sh $12, %2(%1);"                                  \
        "sh $13, %3(%1);"                                  \
        "sh $14, %4(%1)"                                   \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

#define SCALE_COL(m, sv, o0, o1, o2, fac)              \
    ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, sv, o0, o1, o2); \
    gte_lddp(fac);                                     \
    gte_ldsv(sv);                                      \
    gte_gpf12_real();                                  \
    gte_stsv(sv);                                      \
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(sv, m, o0, o1, o2)

/// Block `_actor01100PushOut` carves below the scratchpad head: the push-out
/// delta `func_800E0C10` reports, then whether it moved the model on X or Z.
typedef struct {
    GpDeltaScratch delta;
    s32            moved;
} _Actor01100PushScratch;

void Actor01100_Fn05678(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);
s32  Actor01100_Fn00F58(GpEnemy*, Task*, Actor104900SpawnWork*, Actor104900ShotArg*);

INCLUDE_RODATA("actors/nonmatchings/actor_01100/actor_101100", Actor01100_D00004);

const Actor104900ScaleRodata Actor01100_D00010 = { { 0x1400, 0x1400, 0x1400, 0 }, 0 };

/// First enemy-task state: allocates the 0xBCC work block, enqueues the
/// overlay's sound CD command once while `Gp_StateF0.field_25` is clear,
/// seeds both animation contexts, and hangs the work coordinate off model
/// part 1. Spawn state 1/2 then writes 0x7F into slots 1..20 of each
/// context. Placement `entryId` 0x31 selects the second param table and
/// scales the identity matrix by 0x1400.
void Actor01100_Fn0097C(GpEnemy* enemy, Task* task)
{
    Actor104900SpawnWork*     work;
    TmdObject*                extra;
    GsCOORDINATE2*            parts;
    GpMtxWords*               mtx;
    GpStateF0*                st;
    u8                        param1[8];
    u8                        param2[8];
    ActorsShared801385e0Scale scale;
    u8                        entryId;
    u32                       actorId;
    u32                       map;
    u16                       hp;
    s32                       i;
    s32                       j;
    s32                       offA;
    s32                       offB;
    u8                        rate;
    GpAnimSlot*               slotA;
    GpAnimSlot*               slotB;
    GsCOORDINATE2*            endCoords;

    extra            = (TmdObject*)task->extra;
    parts            = extra->coords;
    task->spawnArg1 &= 0xFFFF0000;
    work             = (Actor104900SpawnWork*)memCalloc(sizeof(Actor104900SpawnWork), 0);
    if (work == NULL) {
        Task_CallExit(task);
        return;
    }

    map = D_8007216C;
    SOFT_BARRIER();
    param1[2] = 0xA;
    param2[0] = 0xB;
    param1[3] = 0;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    if ((map & 0xFFFF0000) == 0x03200000) {
        param1[0]       = 2;
        work->field_BB8 = 1;
    } else {
        work->field_BB8 = 0;
        param1[0]       = 1;
    }

    st = &Gp_StateF0;
    if ((s8)st->field_25 == 0) {
        CdCmd_Enqueue(0x21, param1, param2);
        st->field_25 = 1;
    }

    task->work      = work;
    entryId         = enemy->place->entryId;
    work->field_BBB = entryId;
    if ((s8)entryId == 0x31) {
        enemy->param = &Actor01100_D07510;
    } else {
        enemy->param = &Actor01100_D074E8;
    }

    actorId                             = enemy->placeKey >> 12;
    work->actorId                       = actorId;
    *(s32*)&ActorsShared80137fb8ActorId = actorId;
    extra->lightMtx                     = &work->lightMtx;
    extra->colorMtx                     = &work->colorMtx;
    func_800B3F84(&work->anim, Actor01100_D15604, extra, work->poses, work->slots);
    func_800B3F84(&work->anim2, Actor01100_D15604, extra, work->poses2, work->slots2);
    work->field_BA5 = 1;
    work->field_BA4 = 1;

    mtx     = (GpMtxWords*)&work->coord.coord;
    mtx->w0 = 0x1000;
    mtx->w1 = 0;
    mtx->w2 = 0x1000;
    mtx->w3 = 0;
    mtx->h4 = 0x1000;
    if ((s8)work->field_BBB == 0x31) {
        scale = Actor01100_D00010.scale;
        ActorsShared801385e0(&work->coord.coord, &scale);
    }
    work->coord.coord.t[0] = 0;
    work->coord.coord.t[1] = 0;
    work->coord.coord.t[2] = 0;
    work->coord.sub        = parts;
    work->coord.flg        = 0;

    hp              = enemy->param->hpMax;
    work->field_B92 = hp;
    enemy->hp       = hp;
    SOFT_USE_REG(enemy);
    ((TmdObject*)task->extra)->coords[1].sub = (GsCOORDINATE2*)work;
    ((TmdObject*)task->extra)->coords[1].flg = 0;

    i = 1;
    do {
        Gp_AnimResetSlot(&work->anim, i, work->field_BA4);
        Gp_AnimResetSlot(&work->anim2, i, work->field_BA4);
        i++;
    } while (i < 0x15);

    switch (enemy->spawnState) {
        case 1:
            rate            = 0x7F;
            j               = 1;
            offB            = 0x538;
            offA            = 0x8C;
            work->field_BAE = 0;
            work->state     = 0x18;
            do {
                slotA       = (GpAnimSlot*)((u8*)work + offA);
                slotA->rate = rate;
                SOFT_BARRIER();
                slotB = (GpAnimSlot*)((u8*)work + offB);
                offB += 0x28;
                j++;
                slotB->rate = rate;
                offA       += 0x28;
            } while (j < 0x15);
            break;
        case 2:
            work->field_BAE = 1;
            rate            = 0x7F;
            j               = 1;
            offB            = 0x538;
            offA            = 0x8C;
            work->state     = 0x18;
            do {
                slotA       = (GpAnimSlot*)((u8*)work + offA);
                slotA->rate = rate;
                SOFT_BARRIER();
                slotB = (GpAnimSlot*)((u8*)work + offB);
                offB += 0x28;
                j++;
                slotB->rate = rate;
                offA       += 0x28;
            } while (j < 0x15);
            break;
    }

    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    endCoords               = ((TmdObject*)task->extra)->coords;
    work->effArg.spawnArgLo = 0x400;
    work->effArg.spawnArgHi = 3;
    work->effArg.coord      = endCoords + 4;
    task->state++;
}

/// Arms the enemy's four display nodes the first time the state handler runs
/// with the CD command queue idle: the enemy's own link node is put back on the
/// list, node 0 takes the model's root coordinate and node 3 the pose 3 slots
/// along it, both linked as kind 2 with their `flags` halves ORed in and a
/// three-entry collision table each, and nodes 1 and 2 are linked as kind 3
/// with a `Gp_PackObjPair` payload, the first of the two taking pose 0xC and
/// the second pose 8 of the model's 0x50-byte coordinate records. The task then
/// takes `ActorsShared801384ac` as its
/// exit callback, the model's hidden bit is lifted, `msgTable` is pointed at
/// this overlay's message table and the state advances.
void Actor01100_Fn00CF0(GpEnemy* enemy, Task* task, ActorShared801384acWork* work)
{
    GpObj* obj;
    s32    reach;
    s32    recOff;
    s32    i;
    s32    idx;

    if (CdCmd_IsIdle() & 0xFFFF) {
        Gp_LinkNode(&enemy->node);
        obj           = &work->field_9A8[0];
        obj->coord    = ((TmdObject*)task->extra)->coords;
        obj->ctx.recs = &work->field_A28[0][0];
        obj->pos.vx   = 0;
        obj->pos.vy   = -0x1D8;
        obj->pos.vz   = 0;
        obj->key      = 0x30000;
        obj->radius   = 0x258;
        obj->flags    = 1;
        Gp_LinkObj(2, obj);
        obj->flags |= 0x4000;
        Gp_InitRec18Table(obj->ctx.recs, 3, 0);

        obj           = &work->field_9A8[3];
        obj->coord    = &((TmdObject*)task->extra)->coords[3];
        obj->ctx.recs = &work->field_A28[3][0];
        obj->pos.vx   = 0;
        obj->pos.vy   = 0;
        obj->pos.vz   = 0;
        obj->key      = 0x3000B;
        obj->radius   = 0x1C2;
        obj->flags    = 1;
        Gp_LinkObj(2, obj);
        obj->flags |= 0x8000;
        Gp_InitRec18Table(obj->ctx.recs, 3, 0);
        obj->pos.vx = 0;
        obj->pos.vy = -0xC8;
        obj->pos.vz = 0xC8;

        i      = 0;
        reach  = 0x12C;
        recOff = 0xA70;
        obj    = &work->field_9A8[1];
        do {
            idx = 8;
            if (i == 0) {
                idx = 0xC;
            }
            obj->coord    = &((TmdObject*)task->extra)->coords[idx];
            obj->ctx.recs = (GpRec18*)((u8*)work + recOff);
            do {
                if (i == 0) {
                    obj->pos.vx = -0x12C;
                } else {
                    obj->pos.vx = reach;
                }
                obj->pos.vy = 0;
                obj->pos.vz = 0;
                obj->radius = reach;
                obj->key    = Gp_PackObjPair((GpObj50*)enemy, 1);
                obj->flags  = 1;
                Gp_LinkObj(3, obj);
                obj->flags &= 0x3FFF;
                Gp_InitRec18Table(obj->ctx.recs, 3, 0);
                recOff += 0x48;
                i++;
                obj = &work->field_9A8[i + 1];
            } while (0);
        } while (i < 2);

        enemy->recs                      = &work->field_A28[3][0];
        task->exitCallback               = ActorsShared801384ac;
        ((TmdObject*)task->extra)->flags = (u16)(((TmdObject*)task->extra)->flags & 0xFF7F);
        task->msgTable                   = &Actor01100_D15660;
        task->state++;
        enemy->reactionFlags = 0;
    }
}

/// Scans one three-entry contact table for its first class-2 contact, stopping
/// at the first empty entry. The contact's position is copied into `out` and
/// its key returned; 0 when there is none.
static __inline__ s32 _actor01100FindClass2Contact(SVECTOR* out, GpRec18* contacts)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (contacts[i].key == 0) {
            break;
        }
        if ((contacts[i].key & 0xFFFF0000) == 0x20000) {
            out->vx = contacts[i].point.vx;
            out->vy = contacts[i].point.vy;
            out->vz = contacts[i].point.vz;
            return contacts[i].key;
        }
    }
    return 0;
}

/// Pushes `coord` out of the world contacts in `contacts` with
/// `func_800E0C10`, stepping each nonzero fractional X/Z delta one unit away
/// from zero, and raises the height by 0x80 for the caller to restore.
/// Returns nonzero when the push moved the model on X or Z; always 0 while
/// `D_80072729` is 1.
static __inline__ s32 _actor01100PushOut(GsCOORDINATE2* coord, GpRec18* contacts)
{
    _Actor01100PushScratch* head;
    _Actor01100PushScratch* blk;

    if (D_80072729 == 1) {
        return 0;
    }
    head                                       = *(_Actor01100PushScratch**)G_SCRATCH_HEAD;
    *(_Actor01100PushScratch**)G_SCRATCH_HEAD -= 1;
    blk                                        = *(_Actor01100PushScratch**)G_SCRATCH_HEAD;
    blk->moved                                 = 0;
    if (func_800E0C10(contacts, &blk->delta, 3, NULL) != 0) {
        coord->coord.t[0] += head[-1].delta.vx.h.hi;
        coord->coord.t[1] += blk->delta.vy.h.hi;
        coord->coord.t[2] += blk->delta.vz.h.hi;
        if (head[-1].delta.vx.w & 0xFFFF) {
            if (head[-1].delta.vx.w > 0) {
                coord->coord.t[0] += 1;
            } else {
                coord->coord.t[0] -= 1;
            }
        }
        if (blk->delta.vz.w & 0xFFFF) {
            if (blk->delta.vz.w > 0) {
                coord->coord.t[2] += 1;
            } else {
                coord->coord.t[2] -= 1;
            }
        }
    }
    coord->coord.t[1] += 0x80;
    if ((blk->delta.vx.w != 0) || (blk->delta.vz.w != 0)) {
        blk->moved = 1;
    }
    *(_Actor01100PushScratch**)G_SCRATCH_HEAD += 1;
    return blk->moved;
}

/// Clears the 0xC000 pair from the `flags` of both collision objects.
static __inline__ void _actor01100ClearObjPair(Actor104900SpawnWork* work)
{
    s32 i;

    for (i = 0; i < 2; i++) {
        GpObj* obj  = &work->objs[i];
        obj->flags &= 0x3FFF;
    }
}

/// Per-frame hit handler. Counts down the spark timer at 0xBBE (re-spawning
/// the hit sparks every eighth frame), then takes the first class-2 contact
/// from the last contact table: its damage is scaled by the source's distance,
/// doubled in state 4 unless the source key has bit 15 set, and quadrupled by
/// a successful `Gp_RollEnemyChance`. Damage-over-time ticks add to it, and the
/// 0xBC4 cooldown discards it. Nonzero damage picks a reaction from the id's
/// kind, the damage and the running total at 0xB9C, subtracts from the hit
/// points (playing the death cue and releasing the placement at zero), and
/// stages the reaction's state. Every frame it then pushes the model out of
/// the first contact table and clears all four. Returns 1 when damage landed.
s32 Actor01100_Fn00F58(GpEnemy* enemy, Task* task, Actor104900SpawnWork* work, Actor104900ShotArg* arg)
{
    s32            damaged;
    s32            fromBehind;
    s32            dotDamage;
    s32            doubleDamage;
    s32            rollParam;
    s32            kind7;
    s32            kind4or6;
    s32            died;
    s32            sndId;
    s32            rate;
    GpAnimSlot*    slot;
    GpRec18*       world;
    GsCOORDINATE2* coord;
    s32            savedY;
    s32            moved;
    s16            timer;
    s16            hp;
    s32            dist;
    s32            key;
    s32            cooldown;
    s32            kind;
    s32            i;
    s32            n;
    s32            reaction;
    s32            sparkLevel;
    s32            sourceKey;
    s32            yaw;
    s32            level;
    u32            idKind;
    u32            hitDamage;
    u32            damage;
    u32            hitKey;
    u8             flags;
    u8             mode;
    u8             staged;

    damage       = 0;
    sparkLevel   = -1;
    damaged      = 0;
    fromBehind   = 0;
    dotDamage    = 0;
    doubleDamage = 0;
    rollParam    = 1;
    kind7        = 0;
    kind4or6     = 0;
    sourceKey    = 0;
    if (work->field_B9C > 0) {
        work->field_B9C = (u16)work->field_B9C - 1;
    }
    if (work->field_BBE > 0) {
        timer           = (u16)work->field_BBE - 1;
        work->field_BBE = timer;
        if (!(timer & 7)) {
            func_800FDB18((u16)work->field_BC0, &((TmdObject*)task->extra)->coords[4], NULL, &work->effArg);
            func_800FDB18((u16)work->field_BC0, &((TmdObject*)task->extra)->coords[4], NULL, &work->effArg);
        }
    }
    hitKey = _actor01100FindClass2Contact(&arg->offset, work->contacts[3]);
    if (hitKey != 0) {
        dist = ActorsShared801388e8(((TmdObject*)task->extra)->coords);
        for (i = 0; i < 3; i++) {
            key = work->contacts[3][i].key;
            if (key != 0) {
                sourceKey = key;
                break;
            }
        }
        dist = SquareRoot0(dist);
        yaw  = ActorsShared80138774(((TmdObject*)task->extra)->coords, (sourceKey >> 7) & 1);
        if (yaw < 0) {
            yaw = -yaw;
        }
        fromBehind = yaw >= 0x401;
        if ((work->state == 4) && !(sourceKey & 0x8000)) {
            doubleDamage = 1;
            if (sparkLevel < 3) {
                sparkLevel = 3;
            }
            rollParam = 5;
        }
        hitDamage = Gp_ComputeDamage(hitKey, (u32)dist, 0, 0x1000);
        if (Gp_RollEnemyChance(enemy, hitKey, rollParam) != 0) {
            if (sparkLevel < 0) {
                sparkLevel = 0;
            }
            hitDamage *= 4;
        }
        damage += hitDamage;
    }
    if (enemy->reactionFlags & 0xC) {
        dotDamage = Gp_TickObjFlag4((GpObj5C*)enemy);
        if (dotDamage > 0) {
            Gp_SpawnEff(0x60055, &((TmdObject*)task->extra)->coords[4], 0x11112400, 0);
            damage += dotDamage;
        }
    }
    if (doubleDamage != 0) {
        damage *= 2;
    }
    cooldown = work->field_BC4;
    if (cooldown > 0) {
        work->field_BC4 = cooldown - 1;
        damage          = 0;
    } else if (hitKey != 0) {
        work->field_BC4 = Gp_GetIdParam2((s32)hitKey);
    }
    if (damage == 0) {
        kind = Gp_GetIdParam0((s32)hitKey) & 0xFFFF;
        if (kind < 0xA) {
            if (kind >= 8) {
                func_800DA6E8(&enemy->node, 0, 0);
            }
        }
    } else if ((s32)damage > 0) {
        died            = 0;
        work->field_BC0 = Gp_GetIdParam1((s32)hitKey) & 0xFFFF;
        reaction        = 2;
        work->field_BBE = 0;
        level           = (u16)work->field_B9C + damage;
        work->field_B9C = level;
        if ((s32)damage < 0x1D) {
            reaction = 1;
        }
        if ((s16)level < 0x3D) {
            level = 1;
        } else if ((s16)level < 0x65) {
            level = 2;
        } else {
            level = 3;
        }
        damaged = 1;
        if (reaction < level) {
            reaction = level;
        }
        idKind = Gp_GetIdParam0((s32)hitKey) & 0xFFFF;
        switch (idKind) {
            case 0:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                if (!(enemy->reactionFlags & 2)) {
                    Gp_SetObjFlag2((GpObj5D*)enemy, sourceKey, 0);
                    if ((enemy->reactionFlags & 2) && (work->field_BAB != 5)) {
                        reaction = 3;
                    }
                } else {
                    Gp_SetObjFlag2((GpObj5D*)enemy, sourceKey, 0);
                }
                break;
            case 3:
                Gp_SpawnEff(0x60055, &((TmdObject*)task->extra)->coords[4], 0x11112400, 0);
                Gp_SetObjFlag4((GpObj5C*)enemy, sourceKey, 0);
                break;
            case 5:
                if (doubleDamage == 0) {
                    damage *= 2;
                    if (sparkLevel < 2) {
                        sparkLevel = 2;
                    }
                } else {
                    damage += (s32)damage / 2;
                }
                work->field_BBE = 0x1E;
                break;
            case 4:
            case 6:
                kind4or6 = 1;
                break;
            case 7:
                if (doubleDamage == 0) {
                    damage *= 2;
                    if (sparkLevel < 2) {
                        sparkLevel = 2;
                    }
                } else {
                    damage += (s32)damage / 2;
                }
                Gp_SpawnEff(0x60070, &((TmdObject*)task->extra)->coords[4], 0x80023300, 0);
                kind7 = 1;
                Gp_SpawnEff(0x60070, &((TmdObject*)task->extra)->coords[4], 0x80023300, 0);
                break;
            case 8:
            case 9:
                break;
        }
        if (sparkLevel >= 0) {
            Gp_SpawnEff(0x6009C, &((TmdObject*)task->extra)->coords[4], sparkLevel, 0);
        }
        if ((reaction == 1) && (work->field_BA6 == 0)) {
            reaction = 2;
        }
        flags = enemy->reactionFlags;
        if (flags & 1) {
            reaction             = 3;
            enemy->reactionFlags = flags & 0xFE;
        }
        if ((enemy->reactionFlags & 0xC) && (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0)) {
            enemy->reactionFlags &= 0xF3;
        }
        func_800E2C78((GpObj40*)enemy, (s32)hitKey, (s32)damage, 0);
        func_800DA6E8(&enemy->node, (s32)damage, 0);
        if (work->field_B92 > 0) {
            hp              = (u16)work->field_B92 - damage;
            work->field_B92 = hp;
            enemy->hp       = hp;
            if (work->field_B92 <= 0) {
                if ((D_8007216C & 0xFFFF0000) == 0x05180000) {
                    work->field_BC8 = 0;
                } else {
                    Gp_ReleaseStateF0Add((GpObj20E*)task, (s8)work->field_BBB);
                }
                died   = 1;
                sndId  = (work->field_BB8 << 0x16) | 0x400B0006;
                sndId |= (u8)work->actorId << 8;
                SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                reaction = 3;
                if (work->field_BAF == 2) {
                    reaction = 4;
                }
                if (kind4or6 != 0) {
                    enemy->spawnState = 3;
                } else if (kind7 != 0) {
                    work->field_BBE   = 0x5A;
                    enemy->spawnState = 0x10;
                }
            }
        } else {
            reaction = 0;
        }
        if (died == 0) {
            mode = work->field_BAF;
            if (mode == 1) {
                reaction = 0;
            } else if (mode == 2) {
                if (reaction >= 2) {
                    reaction = 4;
                } else {
                    reaction = 6;
                }
            }
        }
        if ((reaction == 1) && (dotDamage != 0)) {
            reaction = 2;
        }
        if (work->field_BAB == 5) {
            reaction = 5;
        }
        work->field_BAB = reaction;
        if (reaction != 0) {
            rate = 0x10;
            for (n = 1; n < 0x15; n++) {
                slot       = &work->slots[n];
                slot->rate = rate;
                slot       = &work->slots2[n];
                slot->rate = rate;
            }
        }
        staged = work->field_BAB;
        switch (staged) {
            case 1:
                work->field_BA3 = 1;
                if (work->field_B92 > 0) {
                    sndId  = (work->field_BB8 << 0x16) | 0x400B0007;
                    sndId |= (u8)work->actorId << 8;
                    SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                }
                work->field_BAE = fromBehind;
                func_800FDB18((u16)work->field_BC0, &((TmdObject*)task->extra)->coords[4], NULL, &work->effArg);
                break;
            case 2:
                if (work->field_B92 > 0) {
                    sndId  = (work->field_BB8 << 0x16) | 0x400B0007;
                    sndId |= (u8)work->actorId << 8;
                    SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                }
                work->state = 0x15;
                _actor01100ClearObjPair(work);
                work->field_BA6 = 2;
                work->field_BA8 = 0;
                work->field_BAE = fromBehind;
                func_800FDB18((u16)work->field_BC0, &((TmdObject*)task->extra)->coords[4], NULL, &work->effArg);
                break;
            case 3:
                if (work->field_B92 > 0) {
                    sndId  = (work->field_BB8 << 0x16) | 0x400B0007;
                    sndId |= (u8)work->actorId << 8;
                    SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                }
                work->field_BA3 = 0;
                work->state     = 0x16;
                if (enemy->spawnState == 3) {
                    work->state = 0x18;
                }
                _actor01100ClearObjPair(work);
                work->field_BA6 = 2;
                work->field_BA8 = 0;
                work->field_BAE = fromBehind;
                func_800FDB18((u16)work->field_BC0, &((TmdObject*)task->extra)->coords[4], NULL, &work->effArg);
                break;
            case 4:
                if (work->field_B92 > 0) {
                    sndId  = (work->field_BB8 << 0x16) | 0x400B0007;
                    sndId |= (u8)work->actorId << 8;
                    SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                }
                work->field_BA3 = 0;
                work->state     = 0x19;
                if (enemy->spawnState == 3) {
                    if (work->field_BAE != 0) {
                        work->field_BA4 = 0x14;
                    } else {
                        work->field_BA4 = 0x13;
                    }
                    work->state = 0x18;
                }
                _actor01100ClearObjPair(work);
                work->field_BA6 = 2;
                work->field_BA8 = 0;
                func_800FDB18((u16)work->field_BC0, &((TmdObject*)task->extra)->coords[4], NULL, &work->effArg);
                break;
            case 5:
                if (work->field_B92 > 0) {
                    sndId  = (work->field_BB8 << 0x16) | 0x400B0007;
                    sndId |= (u8)work->actorId << 8;
                    SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                }
                func_800FDB18((u16)work->field_BC0, &((TmdObject*)task->extra)->coords[4], NULL, &work->effArg);
                break;
            case 6:
                if (work->field_B92 > 0) {
                    sndId  = (work->field_BB8 << 0x16) | 0x400B0007;
                    sndId |= (u8)work->actorId << 8;
                    SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                }
                func_800FDB18((u16)work->field_BC0, &((TmdObject*)task->extra)->coords[4], NULL, &work->effArg);
                break;
        }
    }
    world  = work->contacts[0];
    coord  = ((TmdObject*)task->extra)->coords;
    savedY = coord->coord.t[1];
    moved  = _actor01100PushOut(coord, world);
    if (moved != 0) {
        coord->flg       = 0;
        work->field_BAC += 1;
    } else {
        work->field_BAC = 0;
    }
    coord->coord.t[1] = savedY;
    Gp_ClearRec18Occupied(work->contacts[0]);
    Gp_ClearRec18Occupied(work->contacts[1]);
    Gp_ClearRec18Occupied(work->contacts[2]);
    Gp_ClearRec18Occupied(work->contacts[3]);
    if ((enemy->reactionFlags & 2) && (Gp_TickObjFlag2((GpObj5D*)enemy) != 0)) {
        enemy->reactionFlags &= 0xFD;
    }
    return damaged;
}

/// First of the 0xA pair the dispatcher at 0x80134780 runs while the latch at
/// 0xBA6 is still clear: it re-arms the link transform and decides from the
/// squared distance `ActorsShared801388e8` measures to the model's part-3
/// coordinate whether the actor closes in this frame.
///
/// The walk offset at 0xB8E steps back toward zero - 0x10 off either end of the
/// +-0x10 band, or straight to zero inside it - and the counter at 0xBAA is
/// cleared. `Task::spawnArg1` then picks the threshold: 0 takes 0x5F5E0F
/// outright, 0x20000 takes 0x3D08FF, and anything else 0xF423FF while the
/// player's `GameActor::field_958` reads 3 and 0xF423F otherwise; the 0x20000
/// case also closes in whenever the player flag at `D_801153F2` reads 1
/// without measuring at all. Either way the link transform is re-armed exactly
/// as its siblings arm it - model part 3 through `TmdObject::coords[3]`, the
/// 0xC8-box local offset through `src` - and the state machine at 0x80132D78
/// runs last; its nonzero answer also closes the actor in.
///
/// Closing in while `field_B92` still counts masks the 0xC000 pair back out of
/// the two `GpObj` nodes in the motion block and, the first time only, stages
/// the 0xA state through `field_BA6`: that is what hands the next frame to the
/// 0x80133BB8 body.
///
/// Each arm declares its own player and actor locals: the two arms must reach
/// the compiler as distinct quantities, since one of them is live across the
/// flag byte's address and cannot share the call's result register.
void Actor01100_Fn01B90(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpLinkXform* xform;
    s32          flag;
    s32          off;
    s32          i;
    u32          dist;
    s16          walk;

    flag = 0;
    dist = ActorsShared801388e8(((TmdObject*)task->extra)->coords);
    walk = work->field_B8E;
    if (walk >= 0x11) {
        work->field_B8E = (s16)((u16)work->field_B8E - 0x10);
    } else if (walk < -0x10) {
        work->field_B8E = (s16)((u16)work->field_B8E + 0x10);
    } else {
        work->field_B8E = 0;
    }
    work->field_BAA = 0;

    if (task->spawnArg1 == 0) {
        if (dist <= 0x5F5E0F) {
            flag = 1;
        }
    } else if (task->spawnArg1 == 0x20000) {
        Task*      player = gameGetPtrSlot(3);
        GameActor* actor;

        if (player != NULL) {
            actor = (GameActor*)player->work;
            if (((D_801153F2 ^ 1) == 0) || (((u16)actor->field_958 == 3) && dist <= 0x3D08FF)) {
                flag = 1;
            }
        }
    } else {
        Task*      player = gameGetPtrSlot(3);
        GameActor* actor;

        if (player != NULL) {
            actor = (GameActor*)player->work;
            if ((((u16)actor->field_958 == 3) && dist <= 0xF423FF) || dist <= 0xF423F) {
                flag = 1;
            }
        }
    }

    xform             = (GpLinkXform*)&enemy->node;
    enemy->node.flags = 0;
    xform->coord      = &((TmdObject*)task->extra)->coords[3];
    xform->src.vx     = 0;
    xform->src.vy     = -0xC8;
    xform->src.vz     = 0xC8;
    if (Actor01100_Fn00F58(enemy, task, (Actor104900SpawnWork*)work, (Actor104900ShotArg*)arg) != 0) {
        flag = 1;
    }
    if (flag && (work->field_B92 > 0)) {
        work->field_BAA = 0;
        i               = 0;
        off             = 0x9C8;
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += 0x20;
            i++;
        } while (i < 2);
        if (work->field_BA6 == 0) {
            work->field_BA6 = 1;
            work->state     = 0xA;
            work->field_BA8 = 0;
        }
    }
}

/// State-0xA pose: clamps the walk at 0xB8E, splits it as Y rotations across
/// model parts 4 and its two `sub` nodes, then GPF-scales the arm chains at
/// parts 6 and 10. 0xB9A/0xB98 scale the child then the parent by the
/// reciprocal; 0xB96/0xB94 scale the parent in place (column 0 at 1+delta,
/// columns 1-2 at 1+delta/4).
void Actor01100_Fn01D98(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GsCOORDINATE2* part;
    GsCOORDINATE2* coords;
    s32            walk;
    s32            rest;
    register s32   t asm("v1");

    walk = work->field_B8E;
    part = ((TmdObject*)task->extra)->coords + 4;
    if (walk < -0x600) {
        walk = -0x600;
    } else if (walk >= 0x601) {
        walk = 0x600;
    }
    if ((u32)(walk + 0x2FF) < 0x5FFU) {
        rest = walk / 3;
        Gfx_RotMatrixY(&part->coord, walk - rest, 0);
        part->flg = 0;
    } else if (walk > 0) {
        Gfx_RotMatrixY(&part->coord, 0x200, 0);
        part->flg = 0;
        rest      = walk - 0x200;
    } else {
        Gfx_RotMatrixY(&part->coord, -0x200, 0);
        part->flg = 0;
        rest      = walk + 0x200;
    }
    rest >>= 1;
    Gfx_RotMatrixY(&part->sub->coord, rest, 0);
    part->sub->flg = 0;
    Gfx_RotMatrixY(&part->sub->sub->coord, rest, 0);
    part->sub->sub->flg = 0;

    t = work->field_B9A;
    if (t != 0) {
        GsCOORDINATE2*    node;
        GsCOORDINATE2*    sub;
        MATRIX*           m;
        MATRIX*           parent;
        register SVECTOR* sv asm("v0");
        register s32      scale asm("a1");
        register s32      inv asm("a0");

        scale  = t + 0x1000;
        coords = ((TmdObject*)task->extra)->coords;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv                 = *(SVECTOR**)((u8*)sv + 0x3FC);
        ((VECTOR*)arg)->vz = scale;
        ((VECTOR*)arg)->vy = scale;
        ((VECTOR*)arg)->vx = scale;
        node               = coords + 6;
        m                  = (MATRIX*)node->sub;
        SCHED_BARRIER();
        sv--;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        m = &((GsCOORDINATE2*)m)->coord;
        SCALE_COL(m, sv, 0, 6, 12, ((VECTOR*)arg)->vx);
        SCALE_COL(m, sv, 2, 8, 14, ((VECTOR*)arg)->vy);
        SCALE_COL(m, sv, 4, 10, 16, ((VECTOR*)arg)->vz);
        inv    = 0x01000000 / scale;
        parent = &coords[6].coord;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv  = *(SVECTOR**)((u8*)sv + 0x3FC);
        sub = node->sub;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv + 1) : "memory");
        sub->flg = 0;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        ((VECTOR*)arg)->vz = inv;
        SCHED_BARRIER();
        ((VECTOR*)arg)->vy = inv;
        SCHED_BARRIER();
        ((VECTOR*)arg)->vx = inv;
        SCALE_COL(parent, sv, 0, 6, 12, ((VECTOR*)arg)->vx);
        SCALE_COL(parent, sv, 2, 8, 14, ((VECTOR*)arg)->vy);
        SCALE_COL(parent, sv, 4, 10, 16, ((VECTOR*)arg)->vz);
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv = *(SVECTOR**)((u8*)sv + 0x3FC);
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv + 1) : "memory");
    }

    t = work->field_B98;
    if (t != 0) {
        GsCOORDINATE2*    node;
        GsCOORDINATE2*    sub;
        MATRIX*           m;
        MATRIX*           parent;
        register SVECTOR* sv asm("v0");
        register s32      scale asm("a1");
        register s32      inv asm("a0");

        scale  = t + 0x1000;
        coords = ((TmdObject*)task->extra)->coords;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv                 = *(SVECTOR**)((u8*)sv + 0x3FC);
        ((VECTOR*)arg)->vz = scale;
        ((VECTOR*)arg)->vy = scale;
        ((VECTOR*)arg)->vx = scale;
        node               = coords + 10;
        m                  = (MATRIX*)node->sub;
        SCHED_BARRIER();
        sv--;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        m = &((GsCOORDINATE2*)m)->coord;
        SCALE_COL(m, sv, 0, 6, 12, ((VECTOR*)arg)->vx);
        SCALE_COL(m, sv, 2, 8, 14, ((VECTOR*)arg)->vy);
        SCALE_COL(m, sv, 4, 10, 16, ((VECTOR*)arg)->vz);
        inv    = 0x01000000 / scale;
        parent = &coords[10].coord;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv  = *(SVECTOR**)((u8*)sv + 0x3FC);
        sub = node->sub;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv + 1) : "memory");
        sub->flg = 0;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        ((VECTOR*)arg)->vz = inv;
        SCHED_BARRIER();
        ((VECTOR*)arg)->vy = inv;
        SCHED_BARRIER();
        ((VECTOR*)arg)->vx = inv;
        SCALE_COL(parent, sv, 0, 6, 12, ((VECTOR*)arg)->vx);
        SCALE_COL(parent, sv, 2, 8, 14, ((VECTOR*)arg)->vy);
        SCALE_COL(parent, sv, 4, 10, 16, ((VECTOR*)arg)->vz);
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv = *(SVECTOR**)((u8*)sv + 0x3FC);
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv + 1) : "memory");
    }

    if (work->field_B96 != 0) {
        MATRIX*        m;
        SVECTOR*       sv;
        GsCOORDINATE2* c;

        sv = (SVECTOR*)((VECTOR*)arg + 1);
        c  = ((TmdObject*)task->extra)->coords;
        m  = &c[6].coord;
        SCALE_COL(m, sv, 0, 6, 12, work->field_B96 + 0x1000);
        SCALE_COL(m, sv, 2, 8, 14, (work->field_B96 >> 2) + 0x1000);
        SCALE_COL(m, sv, 4, 10, 16, (work->field_B96 >> 2) + 0x1000);
        c[6].flg = 0;
    }

    if (work->field_B94 != 0) {
        MATRIX*        m;
        SVECTOR*       sv;
        GsCOORDINATE2* c;

        sv = (SVECTOR*)((VECTOR*)arg + 1);
        c  = ((TmdObject*)task->extra)->coords;
        m  = &c[10].coord;
        SCALE_COL(m, sv, 0, 6, 12, work->field_B94 + 0x1000);
        SCALE_COL(m, sv, 2, 8, 14, (work->field_B94 >> 2) + 0x1000);
        SCALE_COL(m, sv, 4, 10, 16, (work->field_B94 >> 2) + 0x1000);
        c[10].flg = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn02960);

/// Collision-arm handler: the first frame the latch at 0xBA8 is still clear it
/// sets motion 2, zeroes the countdown at 0xB8C and steps the latch. Every
/// later frame increments that countdown. On frame 0x1A it writes a
/// `Gp_PackObjPair` payload into the first motion node's `key` and ORs the
/// 0xC000 pair-pass bits into its `flags`. While the countdown sits in
/// `[0x1B, 0x36]` and the latch is still 1, a hit on the recs table at 0xA70
/// masks those bits back out of both motion nodes and steps the latch; frame
/// 0x37 does the same mask unconditionally. The frame block's scratch byte at
/// 0x64 takes 0xC either way, and the trigger at 0xBA9 ends the sub-state by
/// clearing `state` and the latch.
void Actor01100_Fn035E4(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpObj* obj;
    s32    i;
    s32    off;
    u16    time;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 2;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    time            = (u16)work->field_B8C + 1;
    work->field_B8C = time;
    if ((s16)time == 0x1A) {
        obj         = &work->motion.objs[0];
        obj->key    = Gp_PackObjPair((GpObj50*)enemy, 1);
        obj->flags |= 0xC000;
    }
    if ((u32)((u16)work->field_B8C - 0x1B) < 0x1C) {
        if ((work->field_BA8 == 1) && (Gp_FindRec18((GpRec18*)((u8*)work + 0xA70), 0) != 0)) {
            i   = 0;
            off = 0x9C8;
            do {
                ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
                off                                += 0x20;
                i++;
            } while (i < 2);
            work->field_BA8 = (u8)work->field_BA8 + 1;
        }
    }
    arg->field_64 = 0xC;
    if (work->field_B8C == 0x37) {
        i   = 0;
        off = 0x9C8;
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += 0x20;
            i++;
        } while (i < 2);
    }
    if (work->field_BA9 == 1) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

/// Collision-arm handler for the second motion node: the first frame the latch
/// at 0xBA8 is still clear it sets motion 3, zeroes the countdown at 0xB8C and
/// steps the latch. Every later frame increments that countdown. On frame 0x1A
/// it calls `Gp_PackObjPair` with pair 2 and ORs the 0xC000 pair-pass bits into
/// the second motion node's `flags`. While the countdown sits in `[0x1B, 0x36]`
/// and the latch is still 1, a hit on the recs table at 0xAB8 masks those bits
/// back out of both motion nodes and steps the latch; frame 0x37 does the same
/// mask unconditionally. The frame block's scratch byte at 0x64 takes 8 either
/// way, and the trigger at 0xBA9 ends the sub-state by clearing `state` and
/// the latch.
void Actor01100_Fn03740(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpObj* obj;
    s32    i;
    s32    off;
    u16    time;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 3;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    time            = (u16)work->field_B8C + 1;
    work->field_B8C = time;
    if ((s16)time == 0x1A) {
        obj = &work->motion.objs[1];
        Gp_PackObjPair((GpObj50*)enemy, 2);
        obj->flags |= 0xC000;
    }
    if ((u32)((u16)work->field_B8C - 0x1B) < 0x1C) {
        if ((work->field_BA8 == 1) && (Gp_FindRec18((GpRec18*)((u8*)work + 0xAB8), 0) != 0)) {
            i   = 0;
            off = 0x9C8;
            do {
                ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
                off                                += 0x20;
                i++;
            } while (i < 2);
            work->field_BA8 = (u8)work->field_BA8 + 1;
        }
    }
    arg->field_64 = 8;
    if (work->field_B8C == 0x37) {
        i   = 0;
        off = 0x9C8;
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += 0x20;
            i++;
        } while (i < 2);
    }
    if (work->field_BA9 == 1) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

/// Spin-about handler: on the frame the latch at 0xBA8 is still clear it draws
/// a nibble from `Gp_LcgState` and arms one of the six spin rates - the
/// 0x200 / 0x400 / 0x600 triple, negative on odd draws - into the countdown at
/// 0xB8C, then acts its motion 4. Every later frame turns the model's yaw at
/// 0x46 by 0x10 towards that countdown, rebuilds the Y rotation over the pose
/// and clears `flg`, and when the countdown reaches zero it drops the state at
/// 0xBA7 and the latch, ending the spin about.
void Actor01100_Fn0389C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    GpCoordPose* pose;
    s32          idx;
    u32          rng;
    u16          angle;

    pose = (GpCoordPose*)((TmdObject*)task->extra)->coords;
    if (work->field_BA8 == 0) {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        idx         = (rng >> 0x10) & 0xF;
        if (idx < 3) {
            work->field_B8C = 0x200;
        } else if (idx < 6) {
            work->field_B8C = -0x200;
        } else if (idx < 9) {
            work->field_B8C = 0x400;
        } else if (idx < 0xC) {
            work->field_B8C = -0x400;
        } else if (idx < 0xE) {
            work->field_B8C = 0x600;
        } else {
            work->field_B8C = -0x600;
        }
        work->field_BA4 = 4;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (work->field_B8C > 0) {
        angle          = ((u16)pose->field_46 - 0x10) & 0xFFF;
        pose->field_46 = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg       = 0;
        work->field_B8C = (u16)work->field_B8C - 0x10;
    } else {
        angle          = ((u16)pose->field_46 + 0x10) & 0xFFF;
        pose->field_46 = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg       = 0;
        work->field_B8C = (u16)work->field_B8C + 0x10;
    }
    if (work->field_B8C == 0) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_01100/actor_101100", Actor01100_D000CC);

INCLUDE_RODATA("actors/nonmatchings/actor_01100/actor_101100", ActorsShared80138404Table);
