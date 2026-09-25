#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "actors/actor.h"
#include "actors/actors_shared_80137fb8.h"
#include "actors/actors_shared_801384ac.h"
#include "actors/actors_shared_801385e0.h"
#include "actors/actors_shared_8013852c.h"
#include "actors/actors_shared_8013898c.h"
#include "actors/actors_shared_80138efc.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "gameplay/pairsrc.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/* The loops that step the display nodes, the contact tables or the animation
   slots of the work block walk a scalar byte offset from the block rather than
   indexing the array: the ROM adds the base to the offset on every pass, which
   loop.c produces for a scalar offset but strength-reduces away for an array
   index. */

extern GpPairSrcE Actor01100_D074E8;
extern GpPairSrcE Actor01100_D07510;
extern u8         Actor01100_D15604[];

/// Effect ids `Actor01100_Fn02960` passes to `Gp_SpawnEff`: `D_8011574C`
/// at the model root when the low four bits of `D_80070F70` are clear, and
/// `D_80115738` for the splash.
extern s32 D_80115738;
extern s32 D_8011574C;

/// Word whose low bits `Actor01100_Fn02960` (bits 0-3) and
/// `Actor01100_Fn06F38` (bit 0) test; what sets it is outside this entry.
extern s32 D_80070F70;

/// Actor id the set-up `Actor01100_Fn0097C` stores for the secondary tasks,
/// which shift it into bits 8-15 of their sound ids.
extern u8 Actor01100_D15670;

/// Pair table the spawn state packs into the display node's `GpObj.key`.
extern GpU16Pair Actor01100_D074F8;

/// One of the actor's three state handlers - spawn/setup, per-frame tick and
/// teardown. Wider than the usual two-argument `GpEnemyTaskFunc` shape: the
/// handlers also take the actor's work block (`Task::work`) and a 0x68-byte
/// scratchpad buffer the dispatcher allocates around the call.
typedef void (*Actor101100StateFunc)(GpEnemy* enemy, Task* task, void* work, void* scratch);

/// Fixed-size table of `Actor101100StateFunc` callbacks. Copied onto the stack
/// by `Actor01100_Fn06554` so the call uses a local jump table.
typedef struct {
    Actor101100StateFunc funcs[3];
} Actor101100StateFuncTable3;

/// Scratchpad stack pointer, initialised by GameMain.

extern u8        Actor01100_D15660[];
extern GpU16Pair Actor01100_D074D0[];
extern TaskDesc  Actor01100_D155E0[];
extern u8        Actor01100_D0D8F4;
extern u8        Actor01100_D0E4DC;
extern u8        D_80071075;
extern s8        D_80114C12;

typedef struct {
    void* tmd;
} Actor104900EffSlot;

extern Actor104900EffSlot D_80067330;

/// Reads the halfwords `r0` holds at offsets `o0` / `o1` / `o2` - one column
/// of a matrix's 3x3 part, 6 bytes apart - into the `SVECTOR` at `r1`, through
/// `$12`..`$14`.
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

/// The way back: the three halfwords of the `SVECTOR` at `r0` are stored into
/// the matrix column at `o0` / `o1` / `o2` of `r1`.
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

/// Scales one matrix column by `fac` through the GTE's GPF.
#define SCALE_COL(m, sv, o0, o1, o2, fac)              \
    ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, sv, o0, o1, o2); \
    gte_lddp(fac);                                     \
    gte_ldsv(sv);                                      \
    gte_gpf12();                                       \
    gte_stsv(sv);                                      \
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(sv, m, o0, o1, o2)

void Actor01100_Fn0097C(GpEnemy* enemy, Task* task);
void Actor01100_Fn00CF0(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);
s32  Actor01100_Fn00F58(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn02960(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn035E4(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn03740(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn0389C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn039D0(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn03BAC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn041BC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn04410(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn048C8(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn04DB4(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn0516C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn05678(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn05CFC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn05E68(Task* task);
void Actor01100_Fn06198(Task* task);
void Actor01100_Fn0638C(Task* task);
void Actor01100_Fn0668C(Task* task);
void Actor01100_Fn067C0(MATRIX* arg0, ActorsShared801385e0Scale* arg1);
s32  Actor01100_Fn06954(GsCOORDINATE2* arg0, s32 arg1);
s32  Actor01100_Fn06AC8(GsCOORDINATE2* arg0);
void Actor01100_Fn06E4C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn06F38(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn07014(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn070DC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn07148(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn072B8(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);
void Actor01100_Fn0736C(Task* task);
void Actor01100_Fn073A8(Task* task);
void Actor01100_Fn073DC(Task* task);

/// The enemy task's three states - set-up, the per-frame dispatcher and
/// teardown - which `Actor01100_Fn06554` runs by `Task::state`.
const Actor101100StateFuncTable3 Actor01100_D00004 = { {
    (Actor101100StateFunc)Actor01100_Fn0097C,
    (Actor101100StateFunc)Actor01100_Fn00CF0,
    (Actor101100StateFunc)Actor01100_Fn02960,
} };

/// Scale copied onto the stack and passed to `Actor01100_Fn067C0` when the
/// placement `entryId` is 0x31: 0x1400 on each axis.
const ActorsShared801385e0Scale Actor01100_D00010 = { 0x1400, 0x1400, 0x1400, 0 };

/// Walks the first `count` records of `recs`, up to an empty key, and for
/// every kind 0x10000 or 0x30000 record computes the XZ push-out of the
/// coordinate's world position from it; the last such push is kept in the
/// scratch block, and its length is scaled down to 0x100 when longer. Returns
/// whether any record of those kinds was met. Does nothing, returning 0, while
/// `Mc_SaveData.field_5C1` or the session's `viewReady` is 1.
s32 Actor01100_Fn000E8(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    ActorRepelScratch* head;
    ActorRepelScratch* s;
    ActorRepelScratch* blk;
    SVECTOR*           offset;

    if (Mc_SaveData.field_5C1 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                      = 0;
    head                            = SCRATCH_HEAD(ActorRepelScratch);
    blk                             = head - 1;
    SCRATCH_HEAD(ActorRepelScratch) = blk;
    s                               = blk;
    Gp_UpdateCoord(coord);
    s->pos.vx  = coord->workm.t[0];
    s->pos.vy  = coord->workm.t[1];
    s->pos.vz  = coord->workm.t[2];
    s->last.vz = 0;
    s->last.vy = 0;
    s->last.vx = 0;
    s->hit     = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            actorCalcPush(&s->pos, &recs[s->i], &s->offset);
            s->last.vx = s->offset.vx;
            s->last.vz = s->offset.vz;
        }
    }
    s->len = SquareRoot0(s->offset.vx * s->offset.vx + s->offset.vy * s->offset.vy +
                         s->offset.vz * s->offset.vz);
    if (s->len > 0x100) {
        offset = &s->offset;
        VectorNormalSS(offset, offset);
        gte_lddp(0x100);
        gte_ldsv(offset);
        gte_gpf12();
        gte_stsv(offset);
    }
    coord->flg = 0;
    SCRATCH_POP(ActorRepelScratch);
    return s->hit;
}

/// Pushes `coord` away from the obstacle records in `recs` (the first `count`,
/// stopping at an empty key). Every kind 0x10000 or 0x30000 record contributes
/// its bearing from the coordinate's view-space origin; bearings closer than
/// 0x400 to another cancel each other, and each remaining one moves the
/// coordinate 10 units along it in the XZ plane. `pos` receives the total
/// displacement. Returns whether a kind 0x10000 record was among them. Does
/// nothing, returning 0, while the session's `viewReady` or `Mc_SaveData.field_5C1` is 1.
s32 Actor01100_Fn00430(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (gGameSession->viewReady == 1 || Mc_SaveData.field_5C1 == 1) {
        return 0;
    }

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(OverlayAvoidScratch);
    s                = (OverlayAvoidScratch*)SCRATCH_HEAD(u8);
    s->blocked       = 0;
    pos->vz          = 0;
    pos->vy          = 0;
    pos->vx          = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_POP_BYTES(sizeof(OverlayAvoidScratch));
    return s->blocked != 0;
}

/// First enemy-task state: allocates the 0xBCC work block, enqueues the
/// overlay's sound CD command once while `Gp_StateF0.field_25` is clear,
/// seeds both animation contexts, and hangs the work coordinate off model
/// part 1. Spawn state 1/2 then writes 0x7F into slots 1..20 of each
/// context. Placement `entryId` 0x31 selects the second param table and
/// scales the identity matrix by 0x1400.
void Actor01100_Fn0097C(GpEnemy* enemy, Task* task)
{
    ActorsShared80138efcWork* work;
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

    extra            = task->extra.tmd;
    parts            = extra->coords;
    task->spawnArg1 &= 0xFFFF0000;
    work             = (ActorsShared80138efcWork*)memCalloc(sizeof(ActorsShared80138efcWork), 0);
    if (work == NULL) {
        Task_CallExit(task);
        return;
    }

    map = *(u32*)&Mc_SaveData.at4.loc;
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

    actorId                   = enemy->placeKey >> 12;
    work->actorId             = actorId;
    *(s32*)&Actor01100_D15670 = actorId;
    extra->lightMtx           = &work->lightMtx;
    extra->colorMtx           = &work->colorMtx;
    func_800B3F84(&work->anim, Actor01100_D15604, extra, work->poses, work->slots);
    func_800B3F84(&work->anim2, Actor01100_D15604, extra, work->poses2, work->slots2);
    work->field_BA5 = 1;
    work->field_BA4 = 1;

    mtx          = (GpMtxWords*)&work->coord.coord;
    mtx->m00_m01 = 0x1000;
    mtx->m02_m10 = 0;
    mtx->m11_m12 = 0x1000;
    mtx->m20_m21 = 0;
    mtx->m22     = 0x1000;
    if ((s8)work->field_BBB == 0x31) {
        scale = Actor01100_D00010;
        Actor01100_Fn067C0(&work->coord.coord, &scale);
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
    task->extra.tmd->coords[1].sub = (GsCOORDINATE2*)work;
    task->extra.tmd->coords[1].flg = 0;

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
            offB            = OFFSET_OF(ActorsShared80138efcWork, slots2[1]);
            offA            = OFFSET_OF(ActorsShared80138efcWork, slots[1]);
            work->field_BAE = 0;
            work->state     = 0x18;
            do {
                slotA       = (GpAnimSlot*)((u8*)work + offA);
                slotA->rate = rate;
                SOFT_BARRIER();
                slotB = (GpAnimSlot*)((u8*)work + offB);
                offB += sizeof(GpAnimSlot);
                j++;
                slotB->rate = rate;
                offA       += sizeof(GpAnimSlot);
            } while (j < 0x15);
            break;
        case 2:
            work->field_BAE = 1;
            rate            = 0x7F;
            j               = 1;
            offB            = OFFSET_OF(ActorsShared80138efcWork, slots2[1]);
            offA            = OFFSET_OF(ActorsShared80138efcWork, slots[1]);
            work->state     = 0x18;
            do {
                slotA       = (GpAnimSlot*)((u8*)work + offA);
                slotA->rate = rate;
                SOFT_BARRIER();
                slotB = (GpAnimSlot*)((u8*)work + offB);
                offB += sizeof(GpAnimSlot);
                j++;
                slotB->rate = rate;
                offA       += sizeof(GpAnimSlot);
            } while (j < 0x15);
            break;
    }

    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    endCoords               = task->extra.tmd->coords;
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
/// takes `Actor01100_Fn0668C` as its
/// exit callback, the model's hidden bit is lifted, `msgTable` is pointed at
/// this overlay's message table and the state advances.
void Actor01100_Fn00CF0(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    GpObj* obj;
    s32    reach;
    s32    recOff;
    s32    i;
    s32    idx;

    if (CdCmd_IsIdle() & 0xFFFF) {
        Gp_LinkNode(&enemy->node);
        obj           = &work->objs[0];
        obj->coord    = task->extra.tmd->coords;
        obj->ctx.recs = &work->contacts[0][0];
        obj->pos.vx   = 0;
        obj->pos.vy   = -0x1D8;
        obj->pos.vz   = 0;
        obj->key      = 0x30000;
        obj->radius   = 0x258;
        obj->flags    = 1;
        Gp_LinkObj(2, obj);
        obj->flags |= 0x4000;
        Gp_InitRec18Table(obj->ctx.recs, 3, 0);

        obj           = &work->objs[3];
        obj->coord    = &task->extra.tmd->coords[3];
        obj->ctx.recs = &work->contacts[3][0];
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
        recOff = OFFSET_OF(ActorsShared80138efcWork, contacts[1]);
        obj    = &work->objs[1];
        do {
            idx = 8;
            if (i == 0) {
                idx = 0xC;
            }
            obj->coord    = &task->extra.tmd->coords[idx];
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
                obj->key    = Gp_PackObjPair(enemy, 1);
                obj->flags  = 1;
                Gp_LinkObj(3, obj);
                obj->flags &= 0x3FFF;
                Gp_InitRec18Table(obj->ctx.recs, 3, 0);
                recOff += sizeof(work->contacts[0]);
                i++;
                obj = &work->objs[i + 1];
            } while (0);
        } while (i < 2);

        enemy->recs            = &work->contacts[3][0];
        task->exitCallback     = Actor01100_Fn0668C;
        task->extra.tmd->flags = (u16)(task->extra.tmd->flags & 0xFF7F);
        task->msgTable         = &Actor01100_D15660;
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
/// `Mc_SaveData.field_5C1` is 1.
static __inline__ s32 _actor01100PushOut(GsCOORDINATE2* coord, GpRec18* contacts)
{
    OverlayDeltaFlag* head;
    OverlayDeltaFlag* blk;

    if (Mc_SaveData.field_5C1 == 1) {
        return 0;
    }
    head = SCRATCH_HEAD(OverlayDeltaFlag);
    SCRATCH_PUSH(OverlayDeltaFlag);
    blk        = SCRATCH_HEAD(OverlayDeltaFlag);
    blk->moved = 0;
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
    SCRATCH_POP(OverlayDeltaFlag);
    return blk->moved;
}

/// Clears the 0xC000 pair from the `flags` of both collision objects.
static __inline__ void _actor01100ClearObjPair(ActorsShared80138efcWork* work)
{
    s32 i;

    for (i = 0; i < 2; i++) {
        GpObj* obj  = &work->objs[i + 1];
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
s32 Actor01100_Fn00F58(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
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
            func_800FDB18((u16)work->field_BC0, &task->extra.tmd->coords[4], NULL, &work->effArg);
            func_800FDB18((u16)work->field_BC0, &task->extra.tmd->coords[4], NULL, &work->effArg);
        }
    }
    hitKey = _actor01100FindClass2Contact(&arg->vec, work->contacts[3]);
    if (hitKey != 0) {
        dist = Actor01100_Fn06AC8(task->extra.tmd->coords);
        for (i = 0; i < 3; i++) {
            key = work->contacts[3][i].key;
            if (key != 0) {
                sourceKey = key;
                break;
            }
        }
        dist = SquareRoot0(dist);
        yaw  = Actor01100_Fn06954(task->extra.tmd->coords, (sourceKey >> 7) & 1);
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
        dotDamage = Gp_TickObjFlag4(enemy);
        if (dotDamage > 0) {
            Gp_SpawnEff(0x60055, &task->extra.tmd->coords[4], 0x11112400, 0);
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
                Gp_SetObjFlag1(enemy);
                break;
            case 2:
                if (!(enemy->reactionFlags & 2)) {
                    Gp_SetObjFlag2(enemy, sourceKey, 0);
                    if ((enemy->reactionFlags & 2) && (work->field_BAB != 5)) {
                        reaction = 3;
                    }
                } else {
                    Gp_SetObjFlag2(enemy, sourceKey, 0);
                }
                break;
            case 3:
                Gp_SpawnEff(0x60055, &task->extra.tmd->coords[4], 0x11112400, 0);
                Gp_SetObjFlag4(enemy, sourceKey, 0);
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
                Gp_SpawnEff(0x60070, &task->extra.tmd->coords[4], 0x80023300, 0);
                kind7 = 1;
                Gp_SpawnEff(0x60070, &task->extra.tmd->coords[4], 0x80023300, 0);
                break;
            case 8:
            case 9:
                break;
        }
        if (sparkLevel >= 0) {
            Gp_SpawnEff(0x6009C, &task->extra.tmd->coords[4], sparkLevel, 0);
        }
        if ((reaction == 1) && (work->field_BA6 == 0)) {
            reaction = 2;
        }
        flags = enemy->reactionFlags;
        if (flags & 1) {
            reaction             = 3;
            enemy->reactionFlags = flags & 0xFE;
        }
        if ((enemy->reactionFlags & 0xC) && (Gp_ObjFlag4Expired(enemy) != 0)) {
            enemy->reactionFlags &= 0xF3;
        }
        func_800E2C78(enemy, (s32)hitKey, (s32)damage, 0);
        func_800DA6E8(&enemy->node, (s32)damage, 0);
        if (work->field_B92 > 0) {
            hp              = (u16)work->field_B92 - damage;
            work->field_B92 = hp;
            enemy->hp       = hp;
            if (work->field_B92 <= 0) {
                if ((*(u32*)&Mc_SaveData.at4.loc & 0xFFFF0000) == 0x05180000) {
                    work->field_BC8 = 0;
                } else {
                    Gp_ReleaseStateF0Add(task, (s8)work->field_BBB);
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
                func_800FDB18((u16)work->field_BC0, &task->extra.tmd->coords[4], NULL, &work->effArg);
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
                func_800FDB18((u16)work->field_BC0, &task->extra.tmd->coords[4], NULL, &work->effArg);
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
                func_800FDB18((u16)work->field_BC0, &task->extra.tmd->coords[4], NULL, &work->effArg);
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
                func_800FDB18((u16)work->field_BC0, &task->extra.tmd->coords[4], NULL, &work->effArg);
                break;
            case 5:
                if (work->field_B92 > 0) {
                    sndId  = (work->field_BB8 << 0x16) | 0x400B0007;
                    sndId |= (u8)work->actorId << 8;
                    SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                }
                func_800FDB18((u16)work->field_BC0, &task->extra.tmd->coords[4], NULL, &work->effArg);
                break;
            case 6:
                if (work->field_B92 > 0) {
                    sndId  = (work->field_BB8 << 0x16) | 0x400B0007;
                    sndId |= (u8)work->actorId << 8;
                    SndEvt_EnqueueType6(sndId, arg->pan, arg->depth);
                }
                func_800FDB18((u16)work->field_BC0, &task->extra.tmd->coords[4], NULL, &work->effArg);
                break;
        }
    }
    world  = work->contacts[0];
    coord  = task->extra.tmd->coords;
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
    if ((enemy->reactionFlags & 2) && (Gp_TickObjFlag2(enemy) != 0)) {
        enemy->reactionFlags &= 0xFD;
    }
    return damaged;
}

/// First of the 0xA pair the dispatcher `Actor01100_Fn02960` runs while the latch at
/// 0xBA6 is still clear: it re-arms the link transform and decides from the
/// squared distance `Actor01100_Fn06AC8` measures to the model's part-3
/// coordinate whether the actor closes in this frame.
///
/// The walk offset at 0xB8E steps back toward zero - 0x10 off either end of the
/// +-0x10 band, or straight to zero inside it - and the counter at 0xBAA is
/// cleared. `Task::spawnArg1` then picks the threshold: 0 takes 0x5F5E0F
/// outright, 0x20000 takes 0x3D08FF, and anything else 0xF423FF while the
/// player's `GameActor::field_958` reads 3 and 0xF423F otherwise; the 0x20000
/// case also closes in whenever the player flag at `Gp_StateF0.field_2` reads 1
/// without measuring at all. Either way the link transform is re-armed exactly
/// as its siblings arm it - model part 3 through `TmdObject::coords[3]`, the
/// 0xC8-box local offset through `src` - and `Actor01100_Fn00F58` runs last;
/// its nonzero answer also closes the actor in.
///
/// Closing in while `field_B92` still counts masks the 0xC000 pair back out of
/// the two middle display nodes and, the first time only, stages
/// the 0xA state through `field_BA6`: that is what hands the next frame to
/// `Actor01100_Fn01D98`.
///
/// Each arm declares its own player and actor locals: the two arms must reach
/// the compiler as distinct quantities, since one of them is live across the
/// flag byte's address and cannot share the call's result register.
void Actor01100_Fn01B90(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpLinkNode* lockNode;
    s32         flag;
    s32         off;
    s32         i;
    u32         dist;
    s16         walk;

    flag = 0;
    dist = Actor01100_Fn06AC8(task->extra.tmd->coords);
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
            if (((Gp_StateF0.field_2 ^ 1) == 0) || (((u16)actor->field_958 == 3) && dist <= 0x3D08FF)) {
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

    lockNode                            = &enemy->node;
    enemy->node.state.b.flags           = 0;
    GP_NODE_ENEMY(lockNode)->coord      = &task->extra.tmd->coords[3];
    GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
    GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
    GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
    if (Actor01100_Fn00F58(enemy, task, work, arg) != 0) {
        flag = 1;
    }
    if (flag && (work->field_B92 > 0)) {
        work->field_BAA = 0;
        i               = 0;
        off             = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += sizeof(GpObj);
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
    part = task->extra.tmd->coords + 4;
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
        coords = task->extra.tmd->coords;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv          = *(SVECTOR**)((u8*)sv + 0x3FC);
        arg->pos.vz = scale;
        arg->pos.vy = scale;
        arg->pos.vx = scale;
        node        = coords + 6;
        m           = (MATRIX*)node->sub;
        SCHED_BARRIER();
        sv--;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        m = &((GsCOORDINATE2*)m)->coord;
        SCALE_COL(m, sv, 0, 6, 12, arg->pos.vx);
        SCALE_COL(m, sv, 2, 8, 14, arg->pos.vy);
        SCALE_COL(m, sv, 4, 10, 16, arg->pos.vz);
        inv    = 0x01000000 / scale;
        parent = &coords[6].coord;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv  = *(SVECTOR**)((u8*)sv + 0x3FC);
        sub = node->sub;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv + 1) : "memory");
        sub->flg = 0;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        arg->pos.vz = inv;
        SCHED_BARRIER();
        arg->pos.vy = inv;
        SCHED_BARRIER();
        arg->pos.vx = inv;
        SCALE_COL(parent, sv, 0, 6, 12, arg->pos.vx);
        SCALE_COL(parent, sv, 2, 8, 14, arg->pos.vy);
        SCALE_COL(parent, sv, 4, 10, 16, arg->pos.vz);
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
        coords = task->extra.tmd->coords;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv          = *(SVECTOR**)((u8*)sv + 0x3FC);
        arg->pos.vz = scale;
        arg->pos.vy = scale;
        arg->pos.vx = scale;
        node        = coords + 10;
        m           = (MATRIX*)node->sub;
        SCHED_BARRIER();
        sv--;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        m = &((GsCOORDINATE2*)m)->coord;
        SCALE_COL(m, sv, 0, 6, 12, arg->pos.vx);
        SCALE_COL(m, sv, 2, 8, 14, arg->pos.vy);
        SCALE_COL(m, sv, 4, 10, 16, arg->pos.vz);
        inv    = 0x01000000 / scale;
        parent = &coords[10].coord;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv  = *(SVECTOR**)((u8*)sv + 0x3FC);
        sub = node->sub;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv + 1) : "memory");
        sub->flg = 0;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        arg->pos.vz = inv;
        SCHED_BARRIER();
        arg->pos.vy = inv;
        SCHED_BARRIER();
        arg->pos.vx = inv;
        SCALE_COL(parent, sv, 0, 6, 12, arg->pos.vx);
        SCALE_COL(parent, sv, 2, 8, 14, arg->pos.vy);
        SCALE_COL(parent, sv, 4, 10, 16, arg->pos.vz);
        __asm__ volatile("lui %0, 0x1F80" : "=r"(sv));
        sv = *(SVECTOR**)((u8*)sv + 0x3FC);
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv + 1) : "memory");
    }

    if (work->field_B96 != 0) {
        MATRIX*        m;
        SVECTOR*       sv;
        GsCOORDINATE2* c;

        sv = &arg->vec;
        c  = task->extra.tmd->coords;
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

        sv = &arg->vec;
        c  = task->extra.tmd->coords;
        m  = &c[10].coord;
        SCALE_COL(m, sv, 0, 6, 12, work->field_B94 + 0x1000);
        SCALE_COL(m, sv, 2, 8, 14, (work->field_B94 >> 2) + 0x1000);
        SCALE_COL(m, sv, 4, 10, 16, (work->field_B94 >> 2) + 0x1000);
        c[10].flg = 0;
    }
}

/// Per-frame state handlers `Actor01100_Fn02960` copies to its stack and
/// indexes by `ActorsShared80138efcWork::state`.
const ActorsShared80138efcStateTable Actor01100_D00064 = { {
    Actor01100_Fn06E4C,
    Actor01100_Fn035E4,
    Actor01100_Fn03740,
    Actor01100_Fn0389C,
    Actor01100_Fn06F38,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn03BAC,
    Actor01100_Fn04DB4,
    Actor01100_Fn04410,
    Actor01100_Fn048C8,
    Actor01100_Fn0516C,
    Actor01100_Fn041BC,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn07014,
    Actor01100_Fn070DC,
    Actor01100_Fn07148,
    Actor01100_Fn072B8,
    Actor01100_Fn05678,
    Actor01100_Fn05CFC,
} };

/// Rotates `v` in place by the rotation part of `m`, with no translation.
static __inline__ void _actor01100RotSv(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_SetRotMatrix(m);
    gte_ldv0(&tmp);
    gte_rtv0();
    gte_stsv(v);
}

/// Per-frame update. Does nothing while `field_BA0` is set. Otherwise it
/// refreshes model part 3 and, while `Gp_StateF0.field_4` is 0, steps both animation
/// contexts over parts 1-20 (restarting the motion in `field_BA4` when it
/// changed, and blending the second context in by `field_BA2`), fills
/// `pan`/`depth` from part 1, runs the handler for `state`, then the arm
/// `field_BA6` selects, and while `field_BB8` is 1 spawns the splash effects
/// and sound. While `Gp_StateF0.field_4` is 1 it only pushes the root out of its
/// world contacts. Whatever the mode, it then refreshes the root, updates
/// the actor colour from the root position, draws the floor quad unless bit
/// 1 of the model's flags is set, and exits the task once `field_BA6`
/// reaches 0x10.
void Actor01100_Fn02960(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    ActorsShared80138efcStateTable table;
    GsCOORDINATE2*                 part;
    GsCOORDINATE2*                 root;
    s32                            restart;
    s32                            randBit;
    s32                            slot;
    s32                            pose;
    s32                            blend;
    s32                            animId;
    s32                            i;
    s32                            off;
    s32                            savedY;
    s32                            eff;
    s16                            dy;
    s16                            walk;

    table   = Actor01100_D00064;
    restart = 0;
    if (work->field_BA0 != 0) {
        return;
    }
    Gp_UpdateCoord(&task->extra.tmd->coords[3]);
    if (Gp_StateF0.field_4 == 0) {
        randBit         = rand() & 1;
        work->field_BA9 = 0;
        work->field_BC9 = work->field_BA6;
        if (work->field_BA4 != work->field_BA5) {
            restart         = 1;
            work->field_BA5 = work->field_BA4;
        }
        switch (work->field_BA3) {
            case 0:
                work->field_BA2 = 0;
                break;
            case 1:
                animId = 0xE;
                if (work->field_BAE == 0) {
                    animId = 0xB;
                }
                slot = 1;
                do {
                    func_800B4538(&work->anim2, slot, (s32)&arg->poses[1], animId, 0, 0, 0);
                    slot++;
                } while (slot < 0x15);
                work->field_BA3++;
                break;
            case 2:
                if (work->field_BA2 < 0x40) {
                    work->field_BA2 += 4;
                }
                if (work->slots2[1].flags & 1) {
                    work->field_BA3++;
                }
                break;
            case 3:
                if (work->field_BA2 > 0) {
                    work->field_BA2 -= 4;
                    if (work->field_BA2 > 0) {
                        break;
                    }
                }
                work->field_BA3++;
                break;
            default:
                work->field_BA2 = 0;
                work->field_BA3 = 0;
                break;
        }
        slot = 1;
        do {
            if ((slot == 6) || (slot == 0xA)) {
                pose = 0;
            } else {
                pose = 0;
                if (work->field_BA2 != 0) {
                    pose = (s32)&arg->poses[0];
                }
            }
            if (restart != 0) {
                if ((u32)(work->field_BAB - 2) >= 0xE) {
                    func_800B4538(&work->anim, slot, pose, work->field_BA4, 0, 0, randBit + 8);
                } else if ((u32)((u8)work->field_BA4 - 0x15) < 2) {
                    pose = 0;
                    Gp_AnimResetSlot(&work->anim, slot, work->field_BA4);
                } else if ((slot != 6) && (slot != 0xA)) {
                    func_800B4538(&work->anim, slot, pose, work->field_BA4, 0, 0, 1);
                } else {
                    func_800B4538(&work->anim, slot, pose, work->field_BA4, 3, 0, 0x1E);
                }
            } else {
                func_800B3448(&work->anim, slot, pose, 0);
            }
            if (pose != 0) {
                blend = work->field_BA2 << 5;
                func_800B3448(&work->anim2, slot, (s32)&arg->poses[1], 0);
                Gp_AnimWritePoseBlend(&work->anim, slot, &arg->poses[0],
                                      &arg->poses[1], 0x1000 - blend, blend);
            }
            slot++;
        } while (slot < 0x15);
        if (work->slots[1].flags & 1) {
            work->field_BA9 = 1;
        }
        part               = task->extra.tmd->coords;
        part              += 1;
        *(s16*)&arg->pan   = Gp_GetObjPan(part);
        *(s16*)&arg->depth = gpGetObjDepth(part);
        table.funcs[work->state](enemy, task, work, arg);
        Actor01100_Fn01D98(enemy, task, work, arg);
        switch (work->field_BA6) {
            case 0:
                Actor01100_Fn01B90(enemy, task, work, arg);
                break;
            case 1: {
                GsCOORDINATE2* c;
                GpLinkNode*    lockNode;

                enemy->node.state.b.flags           = 0;
                c                                   = task->extra.tmd->coords;
                lockNode                            = &enemy->node;
                GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
                GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
                GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
                GP_NODE_ENEMY(lockNode)->coord      = c + 3;
                if (Actor01100_Fn00F58(enemy, task, work, arg) == 0 && task->spawnArg1 == 0 && work->field_BC9 == 1 && work->field_BA9 == 1 && Actor01100_Fn06AC8(task->extra.tmd->coords) > 0xA62B10) {
                    i   = 0;
                    off = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
                    do {
                        ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
                        off                                += sizeof(GpObj);
                        i++;
                    } while (i < 2);
                    work->field_BA6 = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    if (((Gp_LcgState >> 0x10) & 0xF) < 0xC) {
                        work->state = 4;
                    } else {
                        work->state = 0;
                    }
                    work->field_BA8 = 0;
                }
                break;
            }
            case 2: {
                GsCOORDINATE2* c;
                GpLinkNode*    lockNode;

                if (work->field_BAB != 1) {
                    if (work->field_B96 >= 0x400) {
                        work->field_B96 -= 0x400;
                    } else {
                        work->field_B96 = 0;
                    }
                    if (work->field_B94 >= 0x400) {
                        work->field_B94 -= 0x400;
                    } else {
                        work->field_B94 = 0;
                    }
                    if (work->field_B98 >= 0x100) {
                        work->field_B98 -= 0x100;
                    } else {
                        work->field_B98 = 0;
                    }
                    if (work->field_B9A >= 0x100) {
                        work->field_B9A -= 0x100;
                    } else {
                        work->field_B9A = 0;
                    }
                    if (work->field_BAF != 0) {
                        walk = work->field_B8E;
                        if (walk > 0x30) {
                            work->field_B8E -= 0x30;
                        } else if (walk < -0x30) {
                            work->field_B8E += 0x30;
                        }
                    }
                }
                c                                   = task->extra.tmd->coords;
                lockNode                            = &enemy->node;
                GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
                GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
                GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
                GP_NODE_ENEMY(lockNode)->coord      = c + 3;
                Actor01100_Fn00F58(enemy, task, work, arg);
                break;
            }
        }
        if (work->field_BB8 == 1) {
            TransposeMatrix(&Gfx_ViewWorldMtx, &arg->mtx);
            if (!(D_80070F70 & 0xF)) {
                GsCOORDINATE2* c;

                c           = task->extra.tmd->coords;
                arg->vec.vx = 0;
                arg->vec.vy = -0x1E0;
                arg->vec.vz = 0;
                Gp_SpawnEff(D_8011574C, c, 0xC0, &arg->vec);
            }
            if ((u8)arg->field_64 != 0) {
                if (work->field_BBA == 0 || ((u8)arg->field_64 == 3 && work->field_BBA != (u8)arg->field_64)) {
                    work->field_BBC = 0;
                }
                work->field_BBA = arg->field_64;
            }
            if (work->field_BB9 != 0 || (u8)arg->field_64 != 0) {
                eff  = 0x11402300;
                part = &task->extra.tmd->coords[work->field_BBA];
                Gp_UpdateCoord(part);
                arg->vec.vx = 0;
                arg->vec.vy = 0;
                arg->vec.vz = 0;
                if (work->field_BBA == 8) {
                    arg->vec.vx = 0x190;
                } else if (work->field_BBA == 0xC) {
                    arg->vec.vx = -0x190;
                }
                _actor01100RotSv(&part->workm, &arg->vec);
                arg->vec.vx += part->workm.t[0];
                arg->vec.vy += part->workm.t[1];
                arg->vec.vz += part->workm.t[2];
                arg->vec.vx -= gGfxViewCoord.workm.t[0];
                arg->vec.vy -= gGfxViewCoord.workm.t[1];
                arg->vec.vz -= gGfxViewCoord.workm.t[2];
                _actor01100RotSv(&arg->mtx, &arg->vec);
                dy = gGameSession->waterY - arg->vec.vy;
                if (work->field_BBC * dy < 0) {
                    work->field_BB9 = 5;
                }
                work->field_BBC = dy;
                if (work->field_BBA == 3) {
                    arg->vec.vx += rand() % 1200 - 0x258;
                    eff          = 0x11602480;
                    arg->vec.vz += rand() % 1200 - 0x258;
                }
                if (work->field_BB9 != 0) {
                    work->field_BB9--;
                    if (work->field_BB9 == 0) {
                        work->field_BBA = 0;
                    }
                    Gp_SpawnEff(D_80115738, &gGfxViewCoord, eff, &arg->vec);
                    SndEvt_EnqueueType6(((u8)work->actorId << 8) | 0x404B000D, arg->pan, arg->depth);
                }
            }
        }
    } else if (Gp_StateF0.field_4 == 1) {
        root   = task->extra.tmd->coords;
        savedY = root->coord.t[1];
        Gp_UpdateCoord(root);
        if (_actor01100PushOut(root, work->contacts[0])) {
            root->flg = 0;
        }
        root->coord.t[1] = savedY;
        Gp_ClearRec18Occupied(work->contacts[0]);
    }
    root = task->extra.tmd->coords;
    Gp_UpdateCoord(root);
    arg->pos.vx = root->workm.t[0];
    arg->pos.vy = root->workm.t[1] - 0x320;
    arg->pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &arg->pos, 0, 0);
    if (!(task->extra.tmd->flags & 2)) {
        Gp_DrawFloorQuad(task->extra.tmd->coords, 0x600, NULL);
    }
    if (work->field_BA6 >= 0x10) {
        Task_CallExit(task);
    }
}

/// Collision-arm handler: the first frame the latch at 0xBA8 is still clear it
/// sets motion 2, zeroes the countdown at 0xB8C and steps the latch. Every
/// later frame increments that countdown. On frame 0x1A it writes a
/// `Gp_PackObjPair` payload into display node 1's `key` and ORs the
/// 0xC000 pair-pass bits into its `flags`. While the countdown sits in
/// `[0x1B, 0x36]` and the latch is still 1, a hit on the recs table at 0xA70
/// masks those bits back out of both middle display nodes and steps the latch; frame
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
        obj         = &work->objs[1];
        obj->key    = Gp_PackObjPair(enemy, 1);
        obj->flags |= 0xC000;
    }
    if ((u32)((u16)work->field_B8C - 0x1B) < 0x1C) {
        if ((work->field_BA8 == 1) && (Gp_FindRec18(work->contacts[1], 0) != 0)) {
            i   = 0;
            off = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
            do {
                ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
                off                                += sizeof(GpObj);
                i++;
            } while (i < 2);
            work->field_BA8 = (u8)work->field_BA8 + 1;
        }
    }
    arg->field_64 = 0xC;
    if (work->field_B8C == 0x37) {
        i   = 0;
        off = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += sizeof(GpObj);
            i++;
        } while (i < 2);
    }
    if (work->field_BA9 == 1) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

/// Collision-arm handler for display node 2: the first frame the latch
/// at 0xBA8 is still clear it sets motion 3, zeroes the countdown at 0xB8C and
/// steps the latch. Every later frame increments that countdown. On frame 0x1A
/// it calls `Gp_PackObjPair` with pair 2 and ORs the 0xC000 pair-pass bits into
/// display node 2's `flags`. While the countdown sits in `[0x1B, 0x36]`
/// and the latch is still 1, a hit on the recs table at 0xAB8 masks those bits
/// back out of both middle display nodes and steps the latch; frame 0x37 does the same
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
        obj = &work->objs[2];
        Gp_PackObjPair(enemy, 2);
        obj->flags |= 0xC000;
    }
    if ((u32)((u16)work->field_B8C - 0x1B) < 0x1C) {
        if ((work->field_BA8 == 1) && (Gp_FindRec18(work->contacts[2], 0) != 0)) {
            i   = 0;
            off = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
            do {
                ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
                off                                += sizeof(GpObj);
                i++;
            } while (i < 2);
            work->field_BA8 = (u8)work->field_BA8 + 1;
        }
    }
    arg->field_64 = 8;
    if (work->field_B8C == 0x37) {
        i   = 0;
        off = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += sizeof(GpObj);
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
void Actor01100_Fn0389C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpCoordExt* pose;
    s32         idx;
    u32         rng;
    u16         angle;

    pose = (GpCoordExt*)task->extra.tmd->coords;
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
        angle              = ((u16)pose->param.rot.vy - 0x10) & 0xFFF;
        pose->param.rot.vy = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg       = 0;
        work->field_B8C = (u16)work->field_B8C - 0x10;
    } else {
        angle              = ((u16)pose->param.rot.vy + 0x10) & 0xFFF;
        pose->param.rot.vy = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg       = 0;
        work->field_B8C = (u16)work->field_B8C + 0x10;
    }
    if (work->field_B8C == 0) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

void Actor01100_Fn039D0(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    SVECTOR              local;
    GsCOORDINATE2*       self;
    GsCOORDINATE2*       other;
    MATRIX*              selfWorkm;
    s32                  angle;
    s32                  target;
    s16                  cur;
    ActorBearingScratch* head;
    ActorBearingScratch* head2;
    ActorBearingScratch* deltaX;
    s32                  otherY;
    s32                  selfY;
    ActorBearingScratch* vec;
    void*                matrix;

    self = task->extra.tmd->coords;
    if (Gp_ActorSlots[0] == NULL) {
        SCHED_BARRIER();
        angle = 0;
    } else {
        other     = Gp_ActorSlots[0]->extra.tmd->coords;
        selfWorkm = &self->workm;
        __asm__("lui %0, 0x1F80" : "=r"(head) : "r"(other));
        head   = *(ActorBearingScratch**)((u8*)head + 0x3FC);
        deltaX = head - 1;
        vec    = head - 1;

        deltaX->delta.vx = (s16)(other->workm.t[0] - self->workm.t[0]);
        otherY           = (u16)other->workm.t[1];
        selfY            = (u16)self->workm.t[1];
        __asm__("addiu %0, %1, -0x20" : "=r"(matrix) : "r"(head), "r"(otherY), "r"(selfY));
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec) : "memory");
        vec->delta.vy = (s16)(otherY - selfY);
        vec->delta.vz = (s16)(other->workm.t[2] - self->workm.t[2]);
        TransposeMatrix(selfWorkm, matrix);

        local = vec->delta;
        gte_SetRotMatrix(matrix);
        __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
        gte_rtv0();
        gte_stsv(&vec->delta);

        angle = ratan2(deltaX->delta.vx, vec->delta.vz);
        if (angle >= 0x801) {
            angle -= 0x1000;
        } else if (angle < -0x800) {
            angle += 0x1000;
        }
        __asm__("lui %0, 0x1F80" : "=r"(head2));
        head2 = *(ActorBearingScratch**)((u8*)head2 + 0x3FC);
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(head2 + 1) : "memory");
    }

    work->field_B90 = angle;
    angle           = (s16)angle;
    if (angle < -0x300) {
        angle = -0x300;
    } else if (angle >= 0x301) {
        angle = 0x300;
    }

    target = angle - 0xC0;
    cur    = work->field_B8E;
    if (cur < target) {
        work->field_B8E = (u16)work->field_B8E + 0xC0;
    } else if (angle + 0xC0 < cur) {
        work->field_B8E = (u16)work->field_B8E - 0xC0;
    } else {
        work->field_B8E = angle;
    }
}

void Actor01100_Fn03BAC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    SVECTOR              local;
    GsCOORDINATE2*       self0;
    GsCOORDINATE2*       self;
    GsCOORDINATE2*       other;
    GsCOORDINATE2*       self2;
    TmdObject*           playerObj;
    MATRIX*              selfWorkm;
    Task*                player;
    SVECTOR*             sv;
    s32                  angle;
    s32                  wrapped;
    s32                  angle3;
    s32                  target;
    s32                  yaw1;
    s32                  yaw;
    s32                  dist;
    s32                  otherY;
    s32                  selfY;
    s32                  state;
    s16                  cur;
    s16                  next;
    s8                   latch;
    u16                  timer;
    s32                  t;
    s32                  xSelf;
    s32                  xOther;
    ActorBearingScratch* head0;
    ActorBearingScratch* head3;
    ActorBearingScratch* pop0;
    ActorBearingScratch* pop3;
    SVECTOR*             pop8;
    ActorBearingScratch* deltaX0;
    ActorBearingScratch* deltaX3;
    ActorBearingScratch* vec0;
    ActorBearingScratch* vec3;
    void*                matrix0;
    void*                matrix3;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 1;
        self0           = task->extra.tmd->coords;
        if (Gp_ActorSlots[0] == NULL) {
            SCHED_BARRIER();
            angle = 0;
        } else {
            other     = Gp_ActorSlots[0]->extra.tmd->coords;
            selfWorkm = &self0->workm;
            __asm__("lui %0, 0x1F80" : "=r"(head0) : "r"(other));
            head0   = *(ActorBearingScratch**)((u8*)head0 + 0x3FC);
            deltaX0 = head0 - 1;
            vec0    = head0 - 1;

            deltaX0->delta.vx = (s16)(other->workm.t[0] - self0->workm.t[0]);
            otherY            = (u16)other->workm.t[1];
            selfY             = (u16)self0->workm.t[1];
            __asm__("addiu %0, %1, -0x20" : "=r"(matrix0) : "r"(head0), "r"(otherY), "r"(selfY));
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec0) : "memory");
            vec0->delta.vy = (s16)(otherY - selfY);
            vec0->delta.vz = (s16)(other->workm.t[2] - self0->workm.t[2]);
            TransposeMatrix(selfWorkm, matrix0);

            local = vec0->delta;
            gte_SetRotMatrix(matrix0);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0();
            gte_stsv(&vec0->delta);

            wrapped = ratan2(deltaX0->delta.vx, vec0->delta.vz);
            if (wrapped >= 0x801) {
                wrapped -= 0x1000;
            } else if (wrapped < -0x800) {
                wrapped += 0x1000;
            }
            __asm__("lui %0, 0x1F80" : "=r"(pop0));
            pop0  = *(ActorBearingScratch**)((u8*)pop0 + 0x3FC);
            angle = wrapped;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(pop0 + 1) : "memory");
        }
        work->field_B90 = angle;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_B8C = ((Gp_LcgState >> 0x10) & 0x1F) + 2;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }

    latch = work->field_BA8;
    if (latch == 1) {
        yaw1 = work->field_B90;
        if (yaw1 < -0x600) {
            yaw1 = -0x600;
        } else if (yaw1 >= 0x601) {
            yaw1 = 0x600;
        }
        cur = work->field_B8E;
        if (cur < yaw1) {
            next            = (u16)work->field_B8E + 0xC0;
            work->field_B8E = next;
            if (yaw1 < next) {
                work->field_B8E = yaw1;
            }
        } else if (yaw1 < cur) {
            next            = (u16)work->field_B8E - 0xC0;
            work->field_B8E = next;
            if (next < yaw1) {
                work->field_B8E = yaw1;
            }
        } else {
            Gp_ArmStateF0(1);
            work->field_BA8 = (u8)work->field_BA8 + 1;
        }
    } else if (latch == 2) {
        timer           = (u16)work->field_B8C - 1;
        work->field_B8C = timer;
        if ((s16)timer < 0) {
            work->field_BA4 = 4;
            work->field_BA8 = (u8)work->field_BA8 + 1;
        }
    }

    if (work->field_BA8 == 3) {
        self = task->extra.tmd->coords;
        if (Gp_ActorSlots[0] == NULL) {
            SCHED_BARRIER();
            angle3 = 0;
        } else {
            other     = Gp_ActorSlots[0]->extra.tmd->coords;
            selfWorkm = &self->workm;
            __asm__("lui %0, 0x1F80" : "=r"(head3) : "r"(other));
            head3   = *(ActorBearingScratch**)((u8*)head3 + 0x3FC);
            deltaX3 = head3 - 1;
            vec3    = head3 - 1;

            deltaX3->delta.vx = (s16)(other->workm.t[0] - self->workm.t[0]);
            otherY            = (u16)other->workm.t[1];
            selfY             = (u16)self->workm.t[1];
            __asm__("addiu %0, %1, -0x20" : "=r"(matrix3) : "r"(head3), "r"(otherY), "r"(selfY));
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec3) : "memory");
            vec3->delta.vy = (s16)(otherY - selfY);
            vec3->delta.vz = (s16)(other->workm.t[2] - self->workm.t[2]);
            TransposeMatrix(selfWorkm, matrix3);

            local = vec3->delta;
            gte_SetRotMatrix(matrix3);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0();
            gte_stsv(&vec3->delta);

            angle3 = ratan2(deltaX3->delta.vx, vec3->delta.vz);
            if (angle3 >= 0x801) {
                angle3 -= 0x1000;
            } else if (angle3 < -0x800) {
                angle3 += 0x1000;
            }
            __asm__("lui %0, 0x1F80" : "=r"(pop3));
            pop3 = *(ActorBearingScratch**)((u8*)pop3 + 0x3FC);
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(pop3 + 1) : "memory");
        }

        yaw             = (s16)angle3;
        work->field_B90 = angle3;
        if (yaw < -0x600) {
            yaw = -0x600;
        } else if (yaw >= 0x601) {
            yaw = 0x600;
        }
        target = yaw - 0xC0;
        cur    = work->field_B8E;
        if (cur < target) {
            work->field_B8E = (u16)work->field_B8E + 0xC0;
        } else if (yaw + 0xC0 < cur) {
            work->field_B8E = (u16)work->field_B8E - 0xC0;
        } else {
            work->field_B8E = yaw;
        }

        yaw = work->field_B90;
        if (yaw >= 0x11) {
            ((GpCoordExt*)self)->param.rot.vy = (u16)((GpCoordExt*)self)->param.rot.vy + 0x10;
        } else if (yaw < -0x10) {
            ((GpCoordExt*)self)->param.rot.vy = (u16)((GpCoordExt*)self)->param.rot.vy - 0x10;
        } else {
            ((GpCoordExt*)self)->param.rot.vy = (u16)((GpCoordExt*)self)->param.rot.vy + yaw;
        }
        timer                             = (u16)((GpCoordExt*)self)->param.rot.vy & 0xFFF;
        ((GpCoordExt*)self)->param.rot.vy = timer;
        Gfx_RotMatrixY(&self->coord, timer, 1);
        self->flg = 0;

        if ((u32)(((u16)work->field_B90 + 0x7F) & 0xFFFF) < 0xFFU) {
            if (task->spawnArg1 != 0) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                t           = ((Gp_LcgState >> 0x10) % 3) & 0xFFFF;
                if (t <= 0) {
                    SCHED_BARRIER();
                    state = 0xC;
                } else if (t < 2) {
                    state = 0xD;
                } else {
                    SOFT_BARRIER();
                    state = 0xB;
                }
            } else {
                self2  = task->extra.tmd->coords;
                player = gameGetPtrSlot(3);
                if (player == NULL) {
                    dist = 0x7FFFFFFF;
                } else {
                    playerObj = player->extra.tmd;
                    TOUCH_REG(playerObj);
                    other  = playerObj->coords;
                    xSelf  = (u16)self2->workm.t[0];
                    xOther = (u16)other->workm.t[0];
                    __asm__("lui %0, 0x1F80" : "=r"(sv) : "r"(xSelf), "r"(xOther));
                    sv        = *(SVECTOR**)((u8*)sv + 0x3FC);
                    sv[-1].vx = (s16)(xOther - xSelf);
                    otherY    = (u16)other->workm.t[1];
                    selfY     = (u16)self2->workm.t[1];
                    __asm__("addiu %0, %1, -8" : "=r"(sv) : "r"(sv), "r"(otherY), "r"(selfY));
                    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
                    sv->vy = (s16)(otherY - selfY);
                    sv->vz = (s16)((u16)other->workm.t[2] - (u16)self2->workm.t[2]);
                    dist   = Gfx_ApplyMatrixNoSf(sv, sv);
                    __asm__("lui %0, 0x1F80" : "=r"(pop8));
                    pop8 = *(SVECTOR**)((u8*)pop8 + 0x3FC);
                    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(pop8 + 1) : "memory");
                }
                if (dist <= 0xA62B0F) {
                    state = 0xD;
                    if (yaw < 0) {
                        state = 0xC;
                    }
                } else {
                    state = 0xE;
                }
            }
            work->state     = state;
            work->field_BA8 = 0;
        }
    }
}

void Actor01100_Fn041BC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* yaw;
    s32            oy;
    s32            sy;
    GsCOORDINATE2* other;
    Task*          player;
    SVECTOR*       vec;
    u8*            head;
    u8*            tail;
    s32            delta;
    s32            lt;
    u16            angle;
    u16            time;
    s32            dist;
    s32            ret;
    u16            selfT;
    u16            otherT;
    s8             kind;
    u16            wait;
    u32            rng;

    wait = 0x3C;
    if ((s8)work->field_BBB == 0x31) {
        wait = 0xA;
    }
    if (work->field_BA8 == 0) {
        work->field_BA4     = 4;
        work->field_BA8     = (u8)work->field_BA8 + 1;
        task->killCountdown = wait;
    }
    Actor01100_Fn039D0(enemy, task, work, arg);

    delta = work->field_B90;
    lt    = delta < 0x11;
    yaw   = task->extra.tmd->coords;
    if (lt == 0) {
        angle = (u16)((GpCoordExt*)yaw)->param.rot.vy + 0x10;
    } else if (delta < -0x10) {
        angle = (u16)((GpCoordExt*)yaw)->param.rot.vy - 0x10;
    } else {
        angle = (u16)((GpCoordExt*)yaw)->param.rot.vy + delta;
    }
    ((GpCoordExt*)yaw)->param.rot.vy = angle;
    SCHED_BARRIER();
    angle                            = *(volatile u16*)&((GpCoordExt*)yaw)->param.rot.vy & 0xFFF;
    ((GpCoordExt*)yaw)->param.rot.vy = angle;
    Gfx_RotMatrixY(&yaw->coord, angle, 1);
    yaw->flg = 0;

    if ((u16)(work->field_B90 + 0x7F) < 0xFFU) {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        work->state = ((rng >> 0x10) & 4) ? 0xC : 0xD;
        Gp_ArmStateF0(1);
        work->field_BA8 = 0;
        return;
    }

    time                = (u16)task->killCountdown - 1;
    task->killCountdown = time;
    if ((s16)time > 0) {
        return;
    }

    coord  = task->extra.tmd->coords;
    player = gameGetPtrSlot(3);
    if (player == NULL) {
        dist = 0x7FFFFFFF;
    } else {
        other  = player->extra.tmd->coords;
        selfT  = (u16)coord->workm.t[0];
        otherT = (u16)other->workm.t[0];
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head                       = *(u8**)(head + 0x3FC);
        ((SVECTOR*)(head - 8))->vx = otherT - selfT;
        oy                         = (u16)other->workm.t[1];
        sy                         = (u16)coord->workm.t[1];
        SOFT_TOUCH_REG2(oy, sy);
        vec = (SVECTOR*)(head - 8);
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec) : "memory");
        vec->vy = oy - sy;
        vec->vz = (u16)other->workm.t[2] - (u16)coord->workm.t[2];
        ret     = Gfx_ApplyMatrixNoSf(vec, vec);
        __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
        tail = *(u8**)(tail + 0x3FC);
        dist = ret;
        tail = tail + 8;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
    }

    kind = (s8)work->field_BBB;
    if (((kind == 0xB) && (dist <= 0x89543F)) || ((kind == 0x31) && (dist <= 0x22550F))) {
        Gp_ArmStateF0(1);
        work->state         = 0xB;
        work->field_BA8     = 0;
        task->killCountdown = 0;
        return;
    }
    task->killCountdown = wait;
}

/// Scale `Actor01100_Fn05678` applies to the model's matrix: 0x10 on each axis.
const ActorsShared801385e0Scale Actor01100_D000CC = { 0x10, 0x10, 0x10, 0 };

/// First-frame distance handler: while the latch at 0xBA8 is still clear it
/// sets motion 6, zeroes the countdown at 0xB8C and steps the latch. If actor
/// slot 3 is live it rotates `(0x12C, 0, 0)` through model part 10's `workm`,
/// adds the player-part-1 versus part-10 translation, and maps
/// `SquareRoot0(Gfx_ApplyMatrixNoSf)` into `field_B9E` — 0 inside 0x384, 0x2000
/// past 0xA8C, otherwise `((dist - 0x384) << 9) / 100`.
///
/// Every later frame increments the countdown, asks `Actor01100_Fn039D0` for
/// the yaw at 0xB90 and turns the model's `field_46` toward it by at most 0x10,
/// then rebuilds the Y rotation. Frame 0x16 packs pair 3 into display node
/// 1 and ORs the 0xC000 bits; frame 0x20 posts `0x400B0008`. While the
/// countdown sits in `[0x17, 0x2B]` and the latch is still 1, a high-bit hit on
/// the recs at 0xA70 steps the latch to 3. `field_B98` / `field_B94` ramp with
/// the countdown, the frame block's scratch byte at 0x64 takes 0xC, and frame
/// 0x2C masks those bits back out of both middle display nodes. The trigger at 0xBA9
/// writes rate 0x10 onto slots `[1, 0x14]` of both animation runs and then
/// either stages state 0xE, or, while the latch is 3, a 1-in-4 draw of that
/// state versus restarting the motion through `field_BA5`.
///
/// The stack copy of the offset is what the first `lwc2` pair reads, and it is
/// written with the sibling bodies' raw asm: `gte_ldv0` of a stack local leaves
/// its `addiu` free for sched2 to hoist. `vec` is assigned only inside the
/// player-present arm so it is a local quantity and keeps `$a0` for
/// `Gfx_ApplyMatrixNoSf`.
void Actor01100_Fn04410(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    SVECTOR        local;
    SVECTOR*       vec;
    GsCOORDINATE2* actorCoords;
    GsCOORDINATE2* playerCoords;
    GsCOORDINATE2* actorPart;
    GsCOORDINATE2* playerPart;
    GpCoordExt*    pose;
    GpObj*         obj;
    GpAnimSlot*    slotA;
    GpAnimSlot*    slotB;
    Task*          player;
    s32            dist;
    s32            i;
    s32            off;
    s32            off2;
    s32            yaw;
    s32            rate;
    u16            angle;
    u16            time;
    u16            reach;
    u32            rng;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 6;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        player          = gameGetPtrSlot(3);
        if (player == NULL) {
            work->field_B9E = 0;
        } else {
            vec          = &arg->vec;
            actorCoords  = task->extra.tmd->coords;
            playerCoords = player->extra.tmd->coords;
            arg->vec.vx  = 0x12C;
            arg->vec.vy  = 0;
            arg->vec.vz  = 0;
            local        = arg->vec;
            actorPart    = &actorCoords[10];
            playerPart   = &playerCoords[1];
            gte_SetRotMatrix(&actorPart->workm);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0();
            gte_stsv(vec);
            arg->vec.vx += (u16)playerPart->workm.t[0] - (u16)actorPart->workm.t[0];
            arg->vec.vy += (u16)playerPart->workm.t[1] - (u16)actorPart->workm.t[1];
            arg->vec.vz += (u16)playerPart->workm.t[2] - (u16)actorPart->workm.t[2];
            dist         = SquareRoot0(Gfx_ApplyMatrixNoSf(vec, vec));
            if (dist < 0x384) {
                work->field_B9E = 0;
            } else if (dist >= 0xA8D) {
                work->field_B9E = 0x2000;
            } else {
                work->field_B9E = ((dist - 0x384) << 9) / 100;
            }
        }
    }
    work->field_B8C = (u16)work->field_B8C + 1;
    Actor01100_Fn039D0(enemy, task, work, arg);
    yaw  = work->field_B90;
    pose = (GpCoordExt*)task->extra.tmd->coords;
    if (yaw >= 0x11) {
        pose->param.rot.vy = (u16)pose->param.rot.vy + 0x10;
    } else if (yaw < -0x10) {
        pose->param.rot.vy = (u16)pose->param.rot.vy - 0x10;
    } else {
        pose->param.rot.vy = (u16)pose->param.rot.vy + yaw;
    }
    angle              = (u16)pose->param.rot.vy & 0xFFF;
    pose->param.rot.vy = angle;
    Gfx_RotMatrixY(&pose->coord, angle, 1);
    pose->flg = 0;
    if (work->field_B8C == 0x16) {
        obj         = &work->objs[1];
        obj->key    = Gp_PackObjPair(enemy, 3);
        obj->flags |= 0xC000;
    } else if (work->field_B8C == 0x20) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | (((u8)work->actorId << 8) | 0x400B0008), arg->pan, arg->depth);
    }
    if (((u32)((u16)work->field_B8C - 0x17) < 0x15U) && (work->field_BA8 == 1) &&
        (Gp_CountRec18Hi(work->contacts[1], 0x10000) != 0)) {
        work->field_BA8 = 3;
    }
    time = work->field_B8C;
    if ((u32)(time - 1) < 0x1DU) {
        if (work->field_B98 < 0x800) {
            work->field_B98 = (s16)((u16)work->field_B98 + 0x40);
        }
    } else if ((s16)time >= 0x1E) {
        if (work->field_B98 >= 0x100) {
            work->field_B98 = (s16)((u16)work->field_B98 - 0x100);
        } else {
            work->field_B98 = 0;
        }
    }
    time = work->field_B8C;
    if ((u32)(time - 0x1E) < 0xEU) {
        reach = work->field_B9E;
        if (work->field_B94 < ((s32)(reach << 0x10) >> 0x10)) {
            work->field_B94 = (s16)((u16)work->field_B94 + ((s32)(reach << 0x10) >> 0x13));
        }
    } else if ((s16)time >= 0x2C) {
        if (work->field_B94 >= 0x200) {
            work->field_B94 = (s16)((u16)work->field_B94 - 0x200);
        } else {
            work->field_B94 = 0;
        }
    }
    arg->field_64 = 0xC;
    if (work->field_B8C == 0x2C) {
        i   = 0;
        off = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += sizeof(GpObj);
            i++;
        } while (i < 2);
    }
    if (work->field_BA9 != 0) {
        rate = 0x10;
        i    = 1;
        off  = OFFSET_OF(ActorsShared80138efcWork, slots2[1]);
        off2 = OFFSET_OF(ActorsShared80138efcWork, slots[1]);
        do {
            slotA       = (GpAnimSlot*)((u8*)work + off2);
            slotA->rate = rate;
            SOFT_BARRIER();
            slotB = (GpAnimSlot*)((u8*)work + off);
            off  += sizeof(GpAnimSlot);
            i++;
            slotB->rate = rate;
            off2       += sizeof(GpAnimSlot);
        } while (i < 0x15);
        if (work->field_BA8 != 3) {
            work->state     = 0xE;
            work->field_BA8 = 0;
            work->field_BAA = (u8)work->field_BAA + 1;
            return;
        }
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if (!((rng >> 0x10) & 3)) {
            work->state = 0xE;
        } else {
            work->field_BA5 = 1;
        }
        work->field_BA8 = 0;
    }
}

/// First-frame distance handler: while the latch at 0xBA8 is still clear it
/// sets motion 7, zeroes the countdown at 0xB8C and steps the latch. If actor
/// slot 3 is live it rotates `(0x12C, 0, 0)` through model part 6's `workm`,
/// adds the player-part-1 versus part-6 translation, and maps
/// `SquareRoot0(Gfx_ApplyMatrixNoSf)` into `field_B9E` — 0 inside 0x384, 0x2000
/// past 0xA8C, otherwise `((dist - 0x384) << 9) / 100`.
///
/// Every frame then asks `Actor01100_Fn039D0` for the yaw at 0xB90 and turns
/// the model's `field_46` toward it by at most 0x10, rebuilds the Y rotation,
/// increments the countdown and asks again. Frame 0x23 packs pair 4 into
/// display node 2 and ORs the 0xC000 bits; frame 0x2D posts `0x400B0008`.
/// While the countdown sits in `[0x24, 0x3B]` and the latch is still 1, a
/// high-bit hit on the recs at 0xAB8 steps the latch to 3. `field_B9A` /
/// `field_B96` ramp with the countdown, the frame block's scratch byte at 0x64
/// takes 8 (with a same-value write on frame 0x2F), and frame 0x3C masks those
/// bits back out of both middle display nodes. The trigger at 0xBA9 writes rate 0x10
/// onto slots `[1, 0x14]` of both animation runs, zeroes `field_B96`, and then
/// either stages state 0xE, or, while the latch is 3, a 1-in-4 draw of that
/// state versus restarting the motion through `field_BA5`.
///
/// The stack copy of the offset is what the first `lwc2` pair reads, and it is
/// written with the sibling bodies' raw asm: `gte_ldv0` of a stack local leaves
/// its `addiu` free for sched2 to hoist. `vec` is assigned only inside the
/// player-present arm so it is a local quantity and keeps `$a0` for
/// `Gfx_ApplyMatrixNoSf`.
void Actor01100_Fn048C8(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    SVECTOR        local;
    SVECTOR*       vec;
    GsCOORDINATE2* actorCoords;
    GsCOORDINATE2* playerCoords;
    GsCOORDINATE2* actorPart;
    GsCOORDINATE2* playerPart;
    GpCoordExt*    pose;
    GpObj*         obj;
    GpAnimSlot*    slotA;
    GpAnimSlot*    slotB;
    Task*          player;
    s32            dist;
    s32            i;
    s32            off;
    s32            off2;
    s32            yaw;
    s32            rate;
    u16            angle;
    u16            time;
    u16            reach;
    u32            rng;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 7;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        player          = gameGetPtrSlot(3);
        if (player == NULL) {
            work->field_B9E = 0;
        } else {
            vec          = &arg->vec;
            actorCoords  = task->extra.tmd->coords;
            playerCoords = player->extra.tmd->coords;
            arg->vec.vx  = 0x12C;
            arg->vec.vy  = 0;
            arg->vec.vz  = 0;
            local        = arg->vec;
            actorPart    = &actorCoords[6];
            playerPart   = &playerCoords[1];
            gte_SetRotMatrix(&actorPart->workm);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0();
            gte_stsv(vec);
            arg->vec.vx += (u16)playerPart->workm.t[0] - (u16)actorPart->workm.t[0];
            arg->vec.vy += (u16)playerPart->workm.t[1] - (u16)actorPart->workm.t[1];
            arg->vec.vz += (u16)playerPart->workm.t[2] - (u16)actorPart->workm.t[2];
            dist         = SquareRoot0(Gfx_ApplyMatrixNoSf(vec, vec));
            if (dist < 0x384) {
                work->field_B9E = 0;
            } else if (dist >= 0xA8D) {
                work->field_B9E = 0x2000;
            } else {
                work->field_B9E = ((dist - 0x384) << 9) / 100;
            }
        }
    }
    Actor01100_Fn039D0(enemy, task, work, arg);
    yaw  = work->field_B90;
    pose = (GpCoordExt*)task->extra.tmd->coords;
    if (yaw >= 0x11) {
        pose->param.rot.vy = (u16)pose->param.rot.vy + 0x10;
    } else if (yaw < -0x10) {
        pose->param.rot.vy = (u16)pose->param.rot.vy - 0x10;
    } else {
        pose->param.rot.vy = (u16)pose->param.rot.vy + yaw;
    }
    angle              = (u16)pose->param.rot.vy & 0xFFF;
    pose->param.rot.vy = angle;
    Gfx_RotMatrixY(&pose->coord, angle, 1);
    pose->flg       = 0;
    work->field_B8C = (u16)work->field_B8C + 1;
    Actor01100_Fn039D0(enemy, task, work, arg);
    if (work->field_B8C == 0x23) {
        obj         = &work->objs[2];
        obj->key    = Gp_PackObjPair(enemy, 4);
        obj->flags |= 0xC000;
    } else if (work->field_B8C == 0x2D) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | (((u8)work->actorId << 8) | 0x400B0008), arg->pan, arg->depth);
    }
    if (((u32)((u16)work->field_B8C - 0x24) < 0x18U) && (work->field_BA8 == 1) &&
        (Gp_CountRec18Hi(work->contacts[2], 0x10000) != 0)) {
        work->field_BA8 = 3;
    }
    time = work->field_B8C;
    if ((u32)(time - 1) < 0x28U) {
        if (work->field_B9A < 0x800) {
            work->field_B9A = (s16)((u16)work->field_B9A + 0x40);
        }
    } else if ((s16)time >= 0x29) {
        if (work->field_B9A >= 0x100) {
            work->field_B9A = (s16)((u16)work->field_B9A - 0x100);
        } else {
            work->field_B9A = 0;
        }
    }
    time = work->field_B8C;
    if ((u32)(time - 0x29) < 0x13U) {
        reach = work->field_B9E;
        if (work->field_B96 < ((s32)(reach << 0x10) >> 0x10)) {
            work->field_B96 = (s16)((u16)work->field_B96 + ((s32)(reach << 0x10) >> 0x13));
        }
    } else if ((s16)time >= 0x3C) {
        if (work->field_B96 >= 0x200) {
            work->field_B96 = (s16)((u16)work->field_B96 - 0x200);
        } else {
            work->field_B96 = 0;
        }
    }
    if (work->field_B8C == 0x2F) {
        arg->field_64 = 8;
    }
    arg->field_64 = 8;
    if (work->field_B8C == 0x3C) {
        i   = 0;
        off = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += sizeof(GpObj);
            i++;
        } while (i < 2);
    }
    if (work->field_BA9 != 0) {
        rate = 0x10;
        i    = 1;
        off  = OFFSET_OF(ActorsShared80138efcWork, slots2[1]);
        off2 = OFFSET_OF(ActorsShared80138efcWork, slots[1]);
        do {
            slotA       = (GpAnimSlot*)((u8*)work + off2);
            slotA->rate = rate;
            SOFT_BARRIER();
            slotB = (GpAnimSlot*)((u8*)work + off);
            off  += sizeof(GpAnimSlot);
            i++;
            slotB->rate = rate;
            off2       += sizeof(GpAnimSlot);
        } while (i < 0x15);
        work->field_B96 = 0;
        if (work->field_BA8 != 3) {
            work->state     = 0xE;
            work->field_BA8 = 0;
            work->field_BAA = (u8)work->field_BAA + 1;
            return;
        }
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if (!((rng >> 0x10) & 3)) {
            work->state = 0xE;
        } else {
            work->field_BA5 = 1;
        }
        work->field_BA8 = 0;
    }
}

/// Countdown handler for the latch at 0xBA8. While the previous count is
/// below 0x1F, `field_B98` climbs by 0x40 toward 0x800; from 0x20 it falls by
/// 0x80, and each frame is mirrored into `field_B9A`.
///
/// Frame 0x20 aims a yaw at actor slot 0 — scratchpad delta, transpose,
/// `ratan2`, wrapped into [-0x800, 0x800) — then spawns from
/// `Actor01100_D155E0`. Placement `field_BBB` 0x31 is a pair of shots;
/// otherwise one fan of three, each at ±0x12C on model part 4, with cue
/// `0x400B000A`. A set `field_BA9` stages state 0xE.
void Actor01100_Fn04DB4(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    SVECTOR              local;
    GsCOORDINATE2*       self;
    GsCOORDINATE2*       other;
    GsCOORDINATE2*       part;
    MATRIX*              selfWorkm;
    Task*                spawned;
    s32                  angle;
    s32                  nOuter;
    s32                  yaw;
    s32                  kind;
    s32                  nInner;
    s32                  i;
    s32                  j;
    s32                  dir;
    u16                  prev;
    u16                  time;
    ActorBearingScratch* head;
    ActorBearingScratch* head2;
    ActorBearingScratch* deltaX;
    s32                  otherY;
    s32                  selfY;
    ActorBearingScratch* vec;
    void*                matrix;
    s32                  bridge;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 8;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    prev            = work->field_B8C;
    time            = prev + 1;
    work->field_B8C = time;
    if ((u32)(prev & 0xFFFF) < 0x1F) {
        if (work->field_B98 < 0x800) {
            work->field_B98 = (u16)work->field_B98 + 0x40;
        }
    } else if ((s16)time >= 0x20) {
        if (work->field_B98 >= 0x80) {
            work->field_B98 = (u16)work->field_B98 - 0x80;
        } else {
            work->field_B98 = 0;
        }
    }
    work->field_B9A = work->field_B98;
    if (work->field_B8C == 0x20) {
        self = task->extra.tmd->coords;
        if (Gp_ActorSlots[0] == NULL) {
            bridge = 0;
        } else {
            other     = Gp_ActorSlots[0]->extra.tmd->coords;
            selfWorkm = &self->workm;
            __asm__("lui %0, 0x1F80" : "=r"(head) : "r"(other));
            head   = *(ActorBearingScratch**)((u8*)head + 0x3FC);
            deltaX = head - 1;
            vec    = head - 1;

            deltaX->delta.vx = (s16)(other->workm.t[0] - self->workm.t[0]);
            otherY           = (u16)other->workm.t[1];
            selfY            = (u16)self->workm.t[1];
            __asm__("addiu %0, %2, -0x20" : "=r"(matrix), "+r"(otherY) : "r"(head), "r"(selfY));
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec) : "memory");
            vec->delta.vy = (s16)(otherY - selfY);
            vec->delta.vz = (s16)(other->workm.t[2] - self->workm.t[2]);
            TransposeMatrix(selfWorkm, matrix);

            local = vec->delta;
            gte_SetRotMatrix(matrix);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0();
            gte_stsv(&vec->delta);

            angle = ratan2(deltaX->delta.vx, vec->delta.vz);
            if (angle >= 0x801) {
                angle -= 0x1000;
            } else if (angle < -0x800) {
                angle += 0x1000;
            }
            __asm__("lui %0, 0x1F80" : "=r"(head2));
            head2  = *(ActorBearingScratch**)((u8*)head2 + 0x3FC);
            bridge = angle;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(head2 + 1), "r"(angle) : "memory");
        }
        yaw = bridge;

        kind = 1;
        if ((s8)work->field_BBB == 0x31) {
            kind = 2;
        }
        if (kind == 1) {
            nOuter = 1;
            nInner = 3;
        } else {
            nOuter = 2;
            nInner = 1;
        }
        i = 0;
        if (nOuter != 0) {
            do {
                dir = i;
                if (kind == 1) {
                    dir = (u32)~yaw >> 31;
                }
                j    = 0;
                part = &task->extra.tmd->coords[4];
                if (nInner != 0) {
                    do {
                        arg->vec.vx = (dir != 0) ? 0x12C : -0x12C;
                        arg->vec.vy = 0;
                        arg->vec.vz = 0;
                        spawned     = Task_SpawnFromTable(&Actor01100_D155E0, kind, yaw, 0);
                        if (spawned != NULL) {
                            Gp_CopyCoordOffset(spawned, part, &arg->vec);
                            Task_Reparent(task, spawned);
                        }
                        selfY = 0x400B000A;
                        j    += 1;
                        SndEvt_EnqueueType6(((work->field_BB8 << 22) | selfY) | ((u8)work->actorId << 8), arg->pan, arg->depth);
                    } while (j < nInner);
                }
                i += 1;
            } while (i < nOuter);
        }
    }
    Actor01100_Fn039D0(enemy, task, work, arg);
    if (work->field_BA9 != 0) {
        work->state     = 0xE;
        work->field_BA8 = 0;
        work->field_BAA = work->field_BAA + 1;
    }
}

/// Distance to actor slot 3, squared, through the scratch pool. Each access of
/// `G_SCRATCH_HEAD` is its own inline so the address stays a rematerialized
/// `lui`/`lw` of `0x1F8003FC`, and the macro writes the caller's variable so
/// the distance is one pseudo.
static __inline__ u8* Actor104900_ScratchRead(void)
{
    return SCRATCH_HEAD(u8);
}

static __inline__ void Actor104900_ScratchWrite(u8* p)
{
    SCRATCH_HEAD(u8) = p;
}

#define Actor104900_DistToPlayer(arg0, out)                           \
    {                                                                 \
        u8*            head;                                          \
        SVECTOR*       vec;                                           \
        GsCOORDINATE2* coord;                                         \
        Task*          slot;                                          \
                                                                      \
        slot = gameGetPtrSlot(3);                                     \
        if (slot == NULL) {                                           \
            out = 0x7FFFFFFF;                                         \
        } else {                                                      \
            coord   = slot->extra.tmd->coords;                        \
            head    = Actor104900_ScratchRead();                      \
            vec     = (SVECTOR*)(head - 8);                           \
            vec->vx = (u16)coord->workm.t[0] - (u16)arg0->workm.t[0]; \
            vec->vy = (u16)coord->workm.t[1] - (u16)arg0->workm.t[1]; \
            Actor104900_ScratchWrite((u8*)vec);                       \
            vec->vz = (u16)coord->workm.t[2] - (u16)arg0->workm.t[2]; \
            out     = Gfx_ApplyMatrixNoSf(vec, vec);                  \
            Actor104900_ScratchWrite(Actor104900_ScratchRead() + 8);  \
        }                                                             \
    }

/// Inlined `Gfx_MatrixCol2` plus the GPF that scales the copied column. The
/// matrix is pinned to `$v1` and the destination is born next; the empty `+r`
/// keeps that `addiu` ahead of the loads, and the GTE ops stay on the same
/// pointer. `$t4`..`$t6` are the column, as in the out-of-line copy.
static __inline__ void Actor104900_MatrixCol2(MATRIX* arg0, volatile SVECTOR* arg1, s32 scale)
{
    register MATRIX*  src asm("v1");
    register short    t4 asm("t4");
    register short    t5 asm("t5");
    register short    t6 asm("t6");
    volatile SVECTOR* out;

    src = arg0;
    out = arg1;
    asm("" : "+r"(out));
    t4      = src->m[0][2];
    t5      = src->m[1][2];
    t6      = src->m[2][2];
    out->vx = t4;
    out->vy = t5;
    out->vz = t6;
    gte_lddp(scale);
    gte_ldsv((SVECTOR*)out);
    gte_gpf12();
    gte_stsv((SVECTOR*)out);
}

/// Lunge. The first frame, while the latch at 0xBA8 is clear, measures the
/// squared distance to actor slot 3. No spawn argument and a target inside
/// 0xA62B0F, or any target inside 0x1DE83F, consumes one `rand` in the first
/// of those cases and stages state 0xF. Otherwise motion 9 is armed, a nibble
/// of `Gp_LcgState` picks a 1/2/3 countdown at 0xB8C (under 5, under 0xC,
/// else), the frame at 0xBAD is armed to -1 and the latch is stepped. The
/// empty asm before the 3 is not a single set, so that arm stays a fallthrough
/// `li`.
///
/// Later frames step 0xBAD while the motion id still matches and the clip has
/// not finished, then `Actor01100_Fn039D0` supplies the yaw at 0xB90. While
/// the frame sits in [1, 0x2E) the model's `field_46` turns toward that yaw by
/// at most 0x10 and the Y rotation is rebuilt. The same window steps
/// `((frame - 13) * 900) / 33` and, while `Mc_SaveData.field_5C1` is clear, adds the
/// scaled facing column's X/Z onto the translation through the frame block's
/// vector at 0x10. Frame 1 cues `0x400B0002` and frame 0x2E cues `0x400B0001`.
///
/// Frame 0x2E remeasures the distance. Seven draws in eight leave the lunge:
/// `field_BAC` below 0xB and a yaw inside ±0x300 stage 0xC or 0xD from the
/// sign, flipped by a further one draw in eight, and only while the new
/// distance is inside 0xA62B0F. Anything else stages 0xB inside 0x89543F and
/// 0xF beyond it.
void Actor01100_Fn0516C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GsCOORDINATE2* actorCoords;
    GsCOORDINATE2* coords;
    GpCoordExt*    pose;
    GpAnimSlot*    motion;
    s32            dist;
    s32            dist2;
    s32            turn;
    s32            yaw;
    s32            yaw2;
    s32            scale;
    s32            frame;
    s32            n;
    s32            snd;
    s16            count;
    u16            angle;
    u32            rng;

    if (work->field_BA8 == 0) {
        actorCoords = task->extra.tmd->coords;
        Actor104900_DistToPlayer(actorCoords, dist);
        if ((task->spawnArg1 == 0) && (dist <= 0xA62B0F)) {
            rand();
            work->state     = 0xF;
            work->field_BA8 = 0;
            return;
        }
        if (dist <= 0x1DE83F) {
            work->state     = 0xF;
            work->field_BA8 = 0;
            return;
        }
        rng             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rng;
        work->field_BA4 = 9;
        n               = (rng >> 16) & 0xF;
        if (n < 5) {
            count = 1;
        } else if (n < 0xC) {
            count = 2;
        } else {
            asm("");
            count = 3;
        }
        work->field_B8C = count;
        work->field_BAD = -1;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    } else {
        motion = &work->slots[1];
        if ((work->slots[1].curSet != work->field_BA4) ||
            (work->field_BAD = (u8)work->field_BAD + 1, (motion->curRec > motion->nextRec))) {
            work->field_BAD = -1;
        }
    }

    Actor01100_Fn039D0(enemy, task, work, arg);
    if ((u32)((u8)work->field_BAD - 1) < 0x2EU) {
        turn = work->field_B90;
        pose = (GpCoordExt*)task->extra.tmd->coords;
        if (turn >= 0x11) {
            pose->param.rot.vy = (u16)pose->param.rot.vy + 0x10;
        } else if (turn < -0x10) {
            pose->param.rot.vy = (u16)pose->param.rot.vy - 0x10;
        } else {
            pose->param.rot.vy = (u16)pose->param.rot.vy + turn;
        }
        angle              = (u16)pose->param.rot.vy & 0xFFF;
        pose->param.rot.vy = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg = 0;
        frame     = work->field_BAD;
        scale     = ((frame - 13) * 900) / 33 - ((frame - 14) * 900) / 33;
        coords    = task->extra.tmd->coords;
        if (Mc_SaveData.field_5C1 == 0) {
            Actor104900_MatrixCol2(&coords->coord, (volatile SVECTOR*)&arg->vec, scale);
            coords->coord.t[0] += arg->vec.vx;
            coords->coord.t[2] += arg->vec.vz;
            coords->flg         = 0;
        }
    }
    if (work->field_BAD == 0x2E) {
        snd = 0x400B0001;
        goto do_sound;
    }
    if (work->field_BAD == 1) {
        snd = 0x400B0002;
    do_sound:
        SndEvt_EnqueueType6((work->field_BB8 << 22) | snd | ((u8)work->actorId << 8), arg->pan, arg->depth);
    }
    if (work->field_BAD == 0x2E) {
        actorCoords = task->extra.tmd->coords;
        Actor104900_DistToPlayer(actorCoords, dist2);
        if (rand() & 7) {
            if (work->field_BAC < 0xBU) {
                yaw = work->field_B90;
                if (yaw < -0x300) {
                    goto far_state;
                }
                if (yaw < 0x301) {
                    goto close_state;
                }
            }
        far_state:
            if (dist2 <= 0x89543F) {
                work->state = 0xB;
            } else {
                work->state = 0xF;
            }
            work->field_BA8 = 0;
            return;
        close_state:
            if (dist2 <= 0xA62B0F) {
                yaw2 = yaw;
                if (!(rand() & 7)) {
                    yaw2 = -yaw2;
                }
                if (yaw2 < 0) {
                    work->state = 0xC;
                } else {
                    work->state = 0xD;
                }
                work->field_BA8 = 0;
            }
        }
    }
}

void Actor01100_Fn05678(
    GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    TmdObject*                 extra;
    TmdObject*                 spawned;
    TmdObject*                 tmd;
    GsCOORDINATE2*             coords;
    GpEffWork*                 eff;
    PlayerStatus*              status;
    GameActor*                 actor;
    ActorsShared801385e0Scale  scale;
    ActorsShared801385e0Scale* sc;
    ActorsShared801385e0Scale* sym;
    MATRIX*                    mtx;
    SVECTOR*                   sv;
    u8*                        head;
    u32                        scratch;
    s16                        time;
    s16                        walk;
    s16                        nextTime;
    s32                        i;
    s32                        off;
    u8                         saved;
    u8                         latch;
    u8*                        tmdA;
    u8*                        tmdB;
    u8*                        tmdC;

    extra = task->extra.tmd;
    if (((*(u32*)&Mc_SaveData.at4.loc & 0xFFFF0000) == 0x05180000) && (work->field_BC8 == 0)) {
        actor  = gameGetPtrSlot(3)->work;
        status = &Player_Status;
        if ((actor->field_954 != 2) && (D_80114C12 != 1) && (D_80071075 == 0) && (status->hp > 0)) {
            Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
            work->field_BC8 = 1;
        }
    }

    if (work->field_BA8 == 0) {
        i               = 0;
        off             = OFFSET_OF(ActorsShared80138efcWork, objs[0]);
        work->field_B92 = 0;
        enemy->hp       = 0;
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += sizeof(GpObj);
            i++;
        } while (i < 4);
        if (enemy->spawnState == 0x10) {
            Gp_SetLightMode(enemy, 2);
            enemy->spawnState = 0;
        } else {
            Gp_SetLightMode(enemy, 1);
        }
        if (enemy->spawnState == 0) {
            enemy->spawnState = work->field_BAE + 1;
        }
        work->field_BA6           = 3;
        work->field_BAB           = 0x20;
        enemy->node.state.b.flags = 1;
        if (enemy->spawnState == 3) {
            tmdA           = &Actor01100_D0D8F4;
            D_80067330.tmd = tmdA;
            eff            = Gp_SpawnEff(0x10032, &task->extra.tmd->coords[6], 0x200, 0);
            if (eff != NULL) {
                spawned    = task->extra.tmd;
                tmd        = eff->task->extra.tmd;
                tmd->tpage = spawned->tpage;
                tmd->clut  = spawned->clut;
                if (tmd->buffer != NULL) {
                    tmdProcessStream(tmd);
                    tmdProcessStream(tmd);
                }
            }
            tmdB           = &Actor01100_D0E4DC;
            D_80067330.tmd = tmdB;
            eff            = Gp_SpawnEff(0x10032, &task->extra.tmd->coords[6], 0x200, 0);
            if (eff != NULL) {
                spawned    = task->extra.tmd;
                tmd        = eff->task->extra.tmd;
                tmd->tpage = spawned->tpage;
                tmd->clut  = spawned->clut;
                if (tmd->buffer != NULL) {
                    tmdProcessStream(tmd);
                    tmdProcessStream(tmd);
                }
            }
            tmdC           = &Actor01100_D0D8F4;
            D_80067330.tmd = tmdC;
            eff            = Gp_SpawnEff(0x10032, &task->extra.tmd->coords[6], 0x200, 0);
            if (eff != NULL) {
                spawned    = task->extra.tmd;
                tmd        = eff->task->extra.tmd;
                tmd->tpage = spawned->tpage;
                tmd->clut  = spawned->clut;
                if (tmd->buffer != NULL) {
                    tmdProcessStream(tmd);
                    tmdProcessStream(tmd);
                }
            }
            latch    = (u8)work->field_BA8;
            nextTime = 0x34;
            goto bump;
        }
        if (enemy->spawnState == 1) {
            work->field_BA4 = 0x11;
        } else {
            work->field_BA4 = 0x12;
        }
        latch    = (u8)work->field_BA8;
        nextTime = 0x20;
        goto bump;
    } else if (work->field_BA8 == 1) {
        time            = (u16)work->field_B8C - 1;
        work->field_B8C = time;
        if (time == 0xC) {
            Gp_SpawnEff(0x600A5, task->extra.tmd->coords, 5, 0);
        } else if (time <= 0) {
            extra->flags |= 2;
            Gp_SetLightMode(enemy, 2);
            latch = (u8)work->field_BA8;
            TOUCH_REG(latch);
            nextTime = 0x20;
            goto bump;
        }
    } else if (work->field_BA8 == 2) {
        time            = (u16)work->field_B8C - 1;
        work->field_B8C = time;
        if ((time << 16) == 0) {
            spawned         = task->extra.tmd;
            spawned->flags |= 0x80;
            latch           = (u8)work->field_BA8;
            __asm__("addiu %0, $zero, 4" : "=r"(nextTime) : "r"(latch), "m"(spawned->flags));
        bump:
            work->field_B8C = nextTime;
            __asm__("addiu %0, %0, 1" : "+r"(latch) : "m"(work->field_B8C));
            work->field_BA8 = latch;
        }
    } else {
        time            = (u16)work->field_B8C - 1;
        work->field_B8C = time;
        if (((time << 16) == 0) && ((*(u32*)&Mc_SaveData.at4.loc & 0xFFFF0000) != 0x05180000)) {
            work->field_BA6 = 0x10;
        }
    }

    walk = work->field_B8E;
    if (walk >= 0x31) {
        work->field_B8E = (s16)((u16)work->field_B8E - 0x30);
    } else if (walk < -0x30) {
        work->field_B8E = (s16)((u16)work->field_B8E + 0x30);
    }

    arg->field_64 = 3;
    saved         = enemy->spawnState;
    if (saved == 3) {
        sym    = &Actor01100_D000CC;
        coords = task->extra.tmd->coords;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        scratch = *(u32*)(head + 0x3FC);
        scale   = *sym;
        sc      = &scale;
        sv      = (SVECTOR*)(scratch - 8);
        mtx     = &coords[3].coord;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
        TOUCH_REG(sv);

        COMPILER_BARRIER();
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(mtx, sv, 0, 6, 12);
        gte_lddp(sc->vx);
        gte_ldsv(sv);
        gte_gpf12();
        gte_stsv(sv);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(sv, mtx, 0, 6, 12);

        COMPILER_BARRIER();
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(mtx, sv, 2, 8, 14);
        gte_lddp(sc->vy);
        gte_ldsv(sv);
        gte_gpf12();
        gte_stsv(sv);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(sv, mtx, 2, 8, 14);

        COMPILER_BARRIER();
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(mtx, sv, 4, 10, 16);
        gte_lddp(sc->vz);
        gte_ldsv(sv);
        gte_gpf12();
        gte_stsv(sv);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(sv, mtx, 4, 10, 16);

        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        scratch       = *(u32*)(head + 0x3FC);
        coords[3].flg = 0;
        scratch      += 8;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(scratch) : "memory");
        if (enemy->spawnState == saved) {
            if (!(extra->flags & 2)) {
                goto skip_fade;
            }
        }
    }

    if (work->coord.coord.m[1][1] >= 0x801) {
        work->coord.coord.m[1][1] = (s16)((u16)work->coord.coord.m[1][1] - 0x20);
        work->coord.flg           = 0;
        work->coord.coord.t[1]    = work->coord.coord.t[1] + 2;
    }
skip_fade:
    return;
}

/// Countdown handler built around the halfword at 0xB8C.
///
/// The first frame arms the motion pair: `field_BA4` takes 0x13, or 0x14 while
/// the flag at 0xBAE is set, `field_BA5` and `field_BAF` both take 1 and the
/// countdown is zeroed, with the run-once latch at 0xBA8 stepped either way.
/// Every later frame moves the countdown up by one and, on the frame it reaches
/// 5, cues the 0x400B0003 event - the actor's id byte at 0xB88 in bits 8..15
/// and the variant at 0xBB8 in bit 22, pan and depth from the frame block -
/// then parks the countdown at -0x7FFF so it fires only once. The frame block's
/// scratch byte at 0x64 takes 3 either way, and the trigger at 0xBA9 ends the
/// sub-state: while `field_B92` still counts it keeps the state on the 0x17
/// motion with the 0x10 pair when the enemy is not carrying flag 0x2 in
/// `field_4C`, and stages the 0x14 motion through `field_BA6` when it is; once
/// that count has run out it hands the frame to `Actor01100_Fn05678` on state
/// 0x18 instead.
void Actor01100_Fn05CFC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    u16 time;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0x13;
        } else {
            work->field_BA4 = 0x14;
        }
        work->field_BA5 = 1;
        work->field_BAF = 1;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    time            = (u16)work->field_B8C + 1;
    work->field_B8C = time;
    if ((s16)time >= 5) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | (((u8)work->actorId << 8) | 0x400B0003), arg->pan, arg->depth);
        work->field_B8C = -0x7FFF;
    }
    arg->field_64 = 3;
    if (work->field_BA9 != 0) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->reactionFlags & 2)) {
                work->field_BAB = 0x10;
                work->field_BAF = 2;
                work->state     = 0x17;
            } else {
                work->field_BA6 = 2;
                work->field_BAB = 5;
                work->state     = 0x14;
            }
            work->field_BA8 = 0;
            return;
        }
        work->state     = 0x18;
        work->field_BA8 = 0;
        Actor01100_Fn05678(enemy, task, work, arg);
    }
}

/// Spawns the effect this actor's next state rides on and re-homes the actor.
///
/// The 0x58-byte work block goes in `Task::work` and the effect task comes back
/// from `Gp_SpawnEff` as `0x60081` parented to the model's trailing coordinate;
/// that task becomes `Task::spawnArg2` and the actor's parent, and the actor arms
/// its own 0x5A kill countdown.
///
/// The effect's velocity is a random direction in the actor's frame: an SVECTOR
/// is built 8 bytes into the scratchpad pool below its published head, with X
/// and Z from `rsin` / `rcos` of the spawn argument and Y a 9-bit draw hung below
/// 0xE000, rotated through the actor's current `coord` and then scaled by
/// `((Gp_LcgState >> 16) & 0x1F) + 0x28` of 0x1000, which the work block keeps.
/// The coordinate is reset to the identity first - a 0x1000 diagonal, the
/// off-diagonal pairs written as zeroed words - then the velocity's X and Z are
/// added to its translation and a 7-bit draw to the Y, and `flg` is cleared.
///
/// The display node is linked as kind 3 pointing at the coordinate and at the
/// 0x28 record, which takes 0x96 for `end0Radius` / `end1Radius` and points
/// `recs` at the one-entry collision table `Gp_InitRec18Table` zeroes, and
/// its `0xC000` flag pair is ORed in on top of `Gp_LinkObj`'s `flags = 3`. The
/// actor takes `Actor01100_Fn073A8` as its exit callback and steps on to the
/// next state, which it also runs immediately.
///
/// The stack copy of the vector is what the first `lwc2` pair reads, and it is
/// written with the sibling bodies' raw asm: `gte_ldv0` of a stack local leaves
/// its `addiu` free for sched2 to hoist, which this body's schedule does not.
void Actor01100_Fn05E68(Task* task)
{
    ActorsShared80137fb8Work* work;
    GpActorD4Rec*             rec;
    GsCOORDINATE2*            coord;
    GpEffWork*                eff;
    GpObj*                    obj;
    MATRIX*                   mtx;
    SVECTOR                   local;
    SVECTOR*                  vec;
    u8*                       head;
    s32                       angle;

    coord = task->extra.tmd->coords;
    work  = memCalloc(0x58, 0);
    if (work == NULL) {
        Task_CallExit(task);
        return;
    }
    task->work = (TaskIdMap*)work;
    eff        = Gp_SpawnEff(0x60081, coord, 0, 0);
    if (eff == NULL) {
        Task_CallExit(task);
        return;
    }
    task->spawnArg2 = eff->task;
    Task_Reparent(task, eff->task);
    angle               = task->spawnArg1;
    task->killCountdown = 0x5A;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 8;
    vec              = (SVECTOR*)(head - 8);
    Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
    vec->vy          = 0xE000 - ((Gp_LcgState >> 16) & 0x1FF);
    vec->vx          = rsin(angle);
    vec->vz          = rcos(angle);

    mtx   = &coord->coord;
    local = *(SVECTOR*)(head - 8);
    gte_SetRotMatrix(mtx);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0();
    gte_stsv(vec);

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    gte_lddp(((Gp_LcgState >> 16) & 0x1F) + 0x28);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(&work->vel);

    *(s32*)&mtx->m[0][0] = 0x1000;
    *(s32*)&mtx->m[0][2] = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    *(s32*)&mtx->m[2][0] = 0;
    mtx->m[2][2]         = 0x1000;

    coord->coord.t[0] += work->vel.vx;
    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
    coord->coord.t[1] += (Gp_LcgState >> 16) & 0x7F;
    coord->coord.t[2] += work->vel.vz;
    coord->flg         = 0;

    obj            = &work->obj;
    rec            = &work->rec;
    obj->coord     = coord;
    obj->ctx.d4rec = rec;
    obj->pos.vx    = 0;
    obj->pos.vy    = 0;
    obj->pos.vz    = 0;
    obj->radius    = 0;
    obj->key       = Gp_PackPair(&Actor01100_D074D0[0], 5);
    obj->flags     = 3;

    rec->recs       = work->rec18;
    rec->end1.vx    = 0;
    rec->end1.vy    = 0;
    rec->end1.vz    = 0;
    rec->end0.vx    = 0;
    rec->end0.vy    = 0;
    rec->end0.vz    = 0;
    rec->end0Radius = 0x96;
    rec->end1Radius = 0x96;
    Gp_InitRec18Table(work->rec18, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags |= 0xC000;

    task->exitCallback = Actor01100_Fn073A8;
    SCRATCH_POP_BYTES(8);
    task->state += 1;
    Actor01100_Fn06198(task);
}

void Actor01100_Fn06198(Task* task)
{
    ActorsShared80137fb8Work* work;
    GpActorD4Rec*             d4;
    GpRec18*                  rec;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            soundCoord;
    Task*                     child;
    u32                       map;
    u32                       mask;
    s32                       flag;
    s32                       id;
    s16                       countdown;

    work = (ActorsShared80137fb8Work*)task->work;
    map  = *(u32*)&Mc_SaveData.at4.loc;
    mask = 0xFFFF0000;
    SOFT_TOUCH_REG(mask);
    map       &= mask;
    flag       = map == 0x03200000;
    coord      = task->extra.tmd->coords;
    soundCoord = (GsCOORDINATE2*)coord;
    d4         = &work->rec;
    if (Gp_StateF0.field_4 == 0) {
        d4->end1.vx        = -(u16)work->vel.vx;
        d4->end1.vy        = -(u16)work->vel.vy;
        d4->end1.vz        = -(u16)work->vel.vz;
        coord->coord.t[0] += (s16)work->vel.vx;
        rec                = work->rec18;
        coord->coord.t[1] += (s16)work->vel.vy;
        coord->coord.t[2] += (s16)work->vel.vz;
        coord->flg         = 0;
        work->vel.vy       = (u16)work->vel.vy + 0xA;
        if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
            child = task->firstChild;
            if (child != NULL) {
                child->spawnArg1 = 3;
            }
            goto fire;
        }
        if (Gp_FindRec18(rec, 0) != 0) {
            child = task->firstChild;
            if (child != NULL) {
                if (rec->at10.normal.vy >= -0xC00) {
                    child->spawnArg1 = 3;
                } else {
                    child->spawnArg1 = 2;
                }
            }
        fire:
            id = (flag << 22) | (0x400B000B | (Actor01100_D15670 << 8));
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(soundCoord), (s8)gpGetObjDepth(soundCoord));
            work->obj.flags    &= 0x3FFF;
            task->killCountdown = 0x1E;
            task->state        += 1;
        }
        Gp_ClearRec18Occupied(work->rec18);
        countdown           = (u16)task->killCountdown - 1;
        task->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            Task_CallExit(task);
        }
    }
}

void Actor01100_Fn0638C(Task* task)
{
    GpEffWork*                effect;
    s32                       variant;
    s32                       soundBase;
    GpRec18*                  rec;
    ActorsShared80137fb8Work* work;
    s32                       area;
    s32                       sound;
    s32                       pan;
    GpObj*                    obj;
    GsCOORDINATE2*            coord;
    GpMtxWords*               rotation;

    coord   = task->extra.tmd->coords;
    area    = *(u32*)&Mc_SaveData.at4.loc & 0xFFFF0000;
    variant = area == 0x03200000;
    work    = memCalloc(sizeof(ActorsShared80137fb8Work), 0);
    if (work == NULL) {
        Task_CallExit(task);
        return;
    }
    task->work = work;
    soundBase  = (variant << 22) | 0x400B000C;
    sound      = soundBase | (Actor01100_D15670 << 8);
    pan        = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
    effect = Gp_SpawnEff(0x60070, coord, 0xC0031FFF, NULL);
    if (effect != NULL) {
        Task_Reparent(task, effect->task);
    }
    task->killCountdown = 0x5A;
    rotation            = (GpMtxWords*)&coord->coord;
    obj                 = &work->obj;
    rotation->m00_m01   = 0x1000;
    rotation->m02_m10   = 0;
    rotation->m11_m12   = 0x1000;
    rotation->m20_m21   = 0;
    rotation->m22       = 0x1000;
    rec                 = work->rec18;
    coord->flg          = 0;
    coord->coord.t[1]  += 0x30;
    obj->coord          = coord;
    obj->ctx.recs       = rec;
    obj->pos.vx         = 0;
    obj->pos.vy         = 0;
    obj->pos.vz         = 0;
    obj->radius         = 0x2EE;
    obj->key            = Gp_PackPair(&Actor01100_D074F8, 5);
    obj->flags          = 1;
    Gp_InitRec18Table(rec, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags        |= 0xC000;
    task->exitCallback = Actor01100_Fn073A8;
    task->state++;
    Actor01100_Fn073DC(task);
}

/// Runs the actor's current state handler, copying the table onto the stack
/// before the call. The handler is handed a 0x68-byte scratchpad buffer,
/// borrowed from the scratchpad stack for the duration of the call, with its
/// last byte cleared.
void Actor01100_Fn06554(Task* task)
{
    Actor101100StateFuncTable3 sp;
    GpEnemy*                   enemy;
    void*                      work;
    u8*                        scratch;

    sp      = Actor01100_D00004;
    enemy   = task->spawnArg2;
    work    = task->work;
    scratch = (u8*)SCRATCH_PUSH_BYTES(0x68);

    scratch[0x64] = 0;
    sp.funcs[task->state](enemy, task, work, scratch);
    SCRATCH_POP_BYTES(0x68);
}

/// State handlers of the secondary task this entry spawns: set-up
/// (`Actor01100_Fn05E68`), per-frame tick (`Actor01100_Fn06198`) and the
/// countdown to exit (`Actor01100_Fn0736C`).
const TaskFuncTable3 Actor01100_D000DC = { {
    Actor01100_Fn05E68,
    Actor01100_Fn06198,
    Actor01100_Fn0736C,
} };

/// Runs the task's current state handler out of `Actor01100_D000DC`, copying
/// the table onto the stack before the call.
void Actor01100_Fn065E4(Task* task)
{
    TaskFuncTable3 sp;

    sp = Actor01100_D000DC;
    sp.funcs[task->state](task);
}

/// Runs the task's current state handler from a two-entry table built on the
/// stack: set-up (`Actor01100_Fn0638C`), then the per-frame countdown
/// (`Actor01100_Fn073DC`).
void Actor01100_Fn0663C(Task* task)
{
    TaskFunc funcs[2] = {
        Actor01100_Fn0638C,
        Actor01100_Fn073DC,
    };

    funcs[task->state](task);
}

/// Exit callback: unlink the four display nodes, relink the second part coord
/// under the model's root, then let gameplay tear the enemy down.
void Actor01100_Fn0668C(Task* task)
{
    ActorsShared80138efcWork* work;
    GpEnemy*                  enemy;
    GsCOORDINATE2*            coord;
    s32                       i;

    enemy = task->spawnArg2;
    work  = (ActorsShared80138efcWork*)task->work;
    for (i = 0; i < 4; i++) {
        Gp_UnlinkObj(&work->objs[i]);
    }
    coord        = task->extra.tmd->coords;
    coord[1].sub = coord;
    Gp_DestroyEnemy(enemy, task);
}

/// Message 0x7D5 handler: switches the enemy's model and display nodes between
/// hidden and shown. `flags ^ 1` is the requested mode, latched in `field_BA0`
/// so only a change acts. Mode 1 hides the model and releases the enemy's link
/// node slot, saving its `field_4` first, and clears the 0xC000 pair off all
/// four display nodes; mode 0 puts the saved `field_4` back, lifts the hidden
/// bit, and sets those bits on the first and last display node.
///
/// The value register is pinned to `$v0` on purpose: local-alloc sorts a block
/// of exactly three quantities with a hand-rolled sort that undoes its own first
/// exchange, so the quantities are placed in birth order and the address would
/// take `$v0` ahead of the value it feeds. With `$v0` live over the value the
/// address lands in `$v1` instead, as the original does.
s32 Actor01100_Fn0670C(Task* task, s32 arg1, s32 flags)
{
    ActorsShared80138efcWork* work;
    GpEnemy*                  enemy;
    TmdObject*                model;
    GpObj*                    obj;
    s32                       i;
    s32                       mode;

    mode  = flags ^ 1;
    work  = (ActorsShared80138efcWork*)task->work;
    model = task->extra.tmd;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_BA0 != mode) {
        work->field_BA0 = mode;
        if ((mode << 0x18) == 0) {
            model->flags              = (u16)(model->flags & 0xFF7F);
            enemy->node.state.b.flags = work->field_BA1;
            obj                       = &work->objs[0];
            obj->flags                = (u16)(obj->flags | 0xC000);
            obj                       = &work->objs[3];
            obj->flags                = (u16)(obj->flags | 0xC000);
        } else {
            register s32 value asm("v0");

            model->flags              = (u16)(model->flags | 0x80);
            work->field_BA1           = enemy->node.state.b.flags;
            enemy->node.state.b.flags = 1;
            for (i = 0; i < 4; i++) {
                obj        = &work->objs[i];
                value      = obj->flags;
                value     &= 0x3FFF;
                obj->flags = (u16)value;
            }
        }
    }
    return 0;
}

void Actor01100_Fn067C0(MATRIX* arg0, ActorsShared801385e0Scale* arg1)
{
    void**   scratch;
    SVECTOR* head;
    SVECTOR* vec;

    scratch                           = SCRATCH_HEAD_ADDR;
    head                              = SCRATCH_HEAD_AT(scratch, SVECTOR);
    vec                               = head - 1;
    SCRATCH_HEAD_AT(scratch, SVECTOR) = vec;

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(arg0, vec, 0, 6, 12);
    gte_lddp(arg1->vx);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, arg0, 0, 6, 12);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(arg0, vec, 2, 8, 14);
    gte_lddp(arg1->vy);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, arg0, 2, 8, 14);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(arg0, vec, 4, 10, 16);
    gte_lddp(arg1->vz);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, arg0, 4, 10, 16);

    head                              = SCRATCH_HEAD_AT(scratch, SVECTOR);
    SCRATCH_HEAD_AT(scratch, SVECTOR) = head + 1;
}

/// Angle from `arg0`'s coordinate to the coordinate at
/// `Gp_ActorSlots[arg1]->extra->field_8`, measured in `arg0`'s own frame. The
/// three translation components, target minus self, are written into the
/// 8-byte scratch vector at `head - 0x40`, `TransposeMatrix` builds the inverse
/// of `arg0->workm` into the 0x20 bytes above it, `mvmva` rotates the delta
/// through that inverse and the three results are read back over the vector;
/// `ratan2` of its X and Z then gives the angle, wrapped into (-0x800, 0x800].
/// The 0x40-byte scratch block is handed back before returning.
///
/// The scratch pointer is published between the second and third halfword
/// stores rather than after all three: consecutive stores carry an output
/// dependency, so their program order survives both schedulers, and moving it
/// later lets it take the call's delay slot instead of the third store.
s32 Actor01100_Fn06954(GsCOORDINATE2* arg0, s32 arg1)
{
    SVECTOR              local;
    Task*                actor;
    GsCOORDINATE2*       coord;
    s32                  angle;
    s32                  result;
    ActorBearingScratch* head;
    ActorBearingScratch* blk;
    MATRIX*              matrix;

    actor = Gp_ActorSlots[arg1];
    if (actor == NULL) {
        return 0;
    }
    coord = actor->extra.tmd->coords;
    head  = SCRATCH_HEAD(ActorBearingScratch);
    blk   = head - 1;

    (head - 1)->delta.vx              = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    blk->delta.vy                     = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    SCRATCH_HEAD(ActorBearingScratch) = blk;
    blk->delta.vz                     = (s16)(coord->workm.t[2] - arg0->workm.t[2]);

    matrix = &blk->frame;
    TransposeMatrix(&arg0->workm, matrix);

    local = blk->delta;
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0();
    gte_stsv(&blk->delta);

    angle  = ratan2(blk->delta.vx, blk->delta.vz);
    result = angle;
    if (angle >= 0x801) {
        result = angle - 0x1000;
    } else if (angle < -0x800) {
        result = angle + 0x1000;
    }
    SCRATCH_POP(ActorBearingScratch);
    return result;
}

/// Squared distance from `arg0` to the slot-3 (player) task's root part coord,
/// or `0x7FFFFFFF` when that task is gone. The delta is staged in an `SVECTOR`
/// carved off the scratchpad stack and squared with `Gfx_ApplyMatrixNoSf`.
s32 Actor01100_Fn06AC8(GsCOORDINATE2* arg0)
{
    void**         scratch;
    u8*            head;
    SVECTOR*       vec;
    GsCOORDINATE2* coord;
    Task*          task;
    s32            ret;

    task = gameGetPtrSlot(3);
    if (task != NULL) {
        coord                          = task->extra.tmd->coords;
        scratch                        = SCRATCH_HEAD_ADDR;
        head                           = SCRATCH_HEAD_AT(scratch, void);
        vec                            = (SVECTOR*)(head - 8);
        vec->vx                        = (u16)coord->workm.t[0] - (u16)arg0->workm.t[0];
        vec->vy                        = (u16)coord->workm.t[1] - (u16)arg0->workm.t[1];
        SCRATCH_HEAD_AT(scratch, void) = vec;
        vec->vz                        = (u16)coord->workm.t[2] - (u16)arg0->workm.t[2];
        ret                            = Gfx_ApplyMatrixNoSf(vec, vec);
        SCRATCH_POP_BYTES_AT(scratch, 8);
    } else {
        ret = 0x7FFFFFFF;
    }
    return ret;
}

/// Steps the actor along its own forward axis: column 2 of the coordinate's
/// rotation is copied into the caller's vector, scaled by GPF with `arg2` as
/// the distance, and added back into the coordinate's X and Z translation. Y is
/// left alone, so the step stays in the ground plane, and clearing `flg` asks
/// the next coordinate update to rebuild the world matrix. Does nothing while
/// the movement freeze flag is set.
///
/// Nothing in the entry calls it, so the argument types are the ones the body
/// itself needs and no call site confirms them.
void Actor01100_Fn06B6C(GsCOORDINATE2* arg0, ActorsShared8013898cVec* arg1, s32 arg2)
{
    if (Mc_SaveData.field_5C1 == 0) {
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(&arg0->coord, &arg1->vec, 4, 10, 16);
        gte_lddp(arg2);
        gte_ldsv(&arg1->vec);
        gte_gpf12();
        gte_stsv(&arg1->vec);
        arg0->coord.t[0] += arg1->vec.vx;
        arg0->coord.t[2] += arg1->vec.vz;
        arg0->flg         = 0;
    }
}

/// Points the enemy's body at the model's fourth part coordinate - the same
/// `TmdObject::coords[3]` that `Gp_UpdateLinkXforms` reads back through
/// `GpEnemy.coord` - and sets the body position the actor spawns inside, and
/// clears the lock-on node's flags.
///
/// The restart path then needs three things at once: `Actor01100_Fn00F58`
/// idle, `Task::spawnArg1` clear, and the work block's trigger pair
/// (`field_BC9`, `field_BA9`) both at 1. With them, and only while the squared
/// distance to the player's slot-3 coordinate stays above 0xA62B10, the
/// 0xC000 pair is masked back out of both `GpObj` nodes in the motion block,
/// and one LCG draw picks the next state: 4 for three draws in four, else 0.
/// `field_BA8` is cleared either way, so the sub-state re-arms from the top.
void Actor01100_Fn06C0C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpLinkNode* lockNode;
    s32         off;
    s32         i;
    u8          trigger;

    lockNode                            = &enemy->node;
    enemy->node.state.b.flags           = 0;
    GP_NODE_ENEMY(lockNode)->coord      = &task->extra.tmd->coords[3];
    GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
    GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
    GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
    if ((Actor01100_Fn00F58(enemy, task, work, arg) == 0) && (task->spawnArg1 == 0)) {
        trigger = work->field_BC9;
        if ((trigger == 1) && (work->field_BA9 == trigger)) {
            if (Actor01100_Fn06AC8(task->extra.tmd->coords) > 0xA62B10) {
                i   = 0;
                off = OFFSET_OF(ActorsShared80138efcWork, objs[1]);
                do {
                    ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
                    off                                += sizeof(GpObj);
                    i++;
                } while (i < 2);
                work->field_BA6 = 0;
                Gp_LcgState     = (Gp_LcgState * 5) + 0x71357911;
                if (((Gp_LcgState >> 0x10) & 0xF) < 0xC) {
                    work->state = 4;
                } else {
                    work->state = 0;
                }
                work->field_BA8 = 0;
            }
        }
    }
}

/// Decays the four counters at 0xB94..0xB9A - the axis pair by 0x400, the two
/// after them by 0x100, each clamped at zero once it falls below its step - and
/// walks `field_B8E` 0x30 back toward zero from either end of the +-0x30 band.
/// `field_BAF` gates the walk, `field_BAB` the whole block, which is why the
/// locals read signed for the test and unsigned for the step.
///
/// Either way the link transform is re-armed exactly as `Actor01100_Fn06C0C`
/// arms it - model part 3 through `TmdObject::coords[3]` as `coord`, the
/// 0xC8-box local offset through `src` - and `Actor01100_Fn00F58` runs last.
void Actor01100_Fn06D3C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpLinkNode* lockNode;
    s16         walk;

    if (work->field_BAB != 1) {
        if (work->field_B96 >= 0x400) {
            work->field_B96 = (s16)((u16)work->field_B96 - 0x400);
        } else {
            work->field_B96 = 0;
        }
        if (work->field_B94 >= 0x400) {
            work->field_B94 = (s16)((u16)work->field_B94 - 0x400);
        } else {
            work->field_B94 = 0;
        }
        if (work->field_B98 >= 0x100) {
            work->field_B98 = (s16)((u16)work->field_B98 - 0x100);
        } else {
            work->field_B98 = 0;
        }
        if (work->field_B9A >= 0x100) {
            work->field_B9A = (s16)((u16)work->field_B9A - 0x100);
        } else {
            work->field_B9A = 0;
        }
        if (work->field_BAF != 0) {
            walk = work->field_B8E;
            if (walk >= 0x31) {
                work->field_B8E = (s16)((u16)work->field_B8E - 0x30);
            } else if (walk < -0x30) {
                work->field_B8E = (s16)((u16)work->field_B8E + 0x30);
            }
        }
    }
    lockNode                            = &enemy->node;
    GP_NODE_ENEMY(lockNode)->coord      = &task->extra.tmd->coords[3];
    GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
    GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
    GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
    Actor01100_Fn00F58(enemy, task, work, arg);
}

void Actor01100_Fn06E4C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    s32 t;
    s32 t2;
    u16 timer;

    if (work->field_BA8 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        t           = (Gp_LcgState >> 0x10) & 0xF;
        if (t < 4) {
            work->field_B8C = 2;
        } else if (t < 8) {
            work->field_B8C = 0x3C;
        } else if (t < 0xE) {
            work->field_B8C = 0x78;
        } else {
            work->field_B8C = 0xB4;
        }
        work->field_BA4 = 1;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    timer           = (u16)work->field_B8C - 1;
    work->field_B8C = timer;
    if (((u32)timer << 0x10) == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        t2          = (Gp_LcgState >> 0x10) & 0xF;
        if (t2 < 3) {
            work->state = 1;
        } else if (t2 < 6) {
            work->state = 2;
        } else if (t2 < 0xE) {
            work->state = 3;
        } else {
            work->state = 4;
        }
        work->field_BA8 = 0;
    }
}

/// First frame of the sub-state arms motion 5 and the 0x64-frame countdown at
/// 0xB8C, then bumps the run-once latch at 0xBA8. Every later frame steps that
/// countdown, and on the frame it reaches zero cues the 0x400B0004 event - the
/// actor's id byte at 0xB88 in bits 8..15, the variant byte at 0xBB8 in bit 22,
/// pan and depth from the frame block - through `SndEvt_EnqueueType6`. The
/// scratch byte at 0x64 then takes 0xC while bit 0 of `D_80070F70` is set and 8
/// otherwise, and the trigger at 0xBA9 ends the sub-state by clearing both the
/// state and the latch.
void Actor01100_Fn06F38(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    if (work->field_BA8 == 0) {
        work->field_BA4 = 5;
        work->field_B8C = 0x64;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        return;
    }
    if (work->field_B8C != 0) {
        work->field_B8C--;
        if (work->field_B8C == 0) {
            SndEvt_EnqueueType6((work->field_BB8 << 22) | (((u8)work->actorId << 8) | 0x400B0004), arg->pan, arg->depth);
        }
    }
    if (D_80070F70 & 1) {
        arg->field_64 = 0xC;
    } else {
        arg->field_64 = 8;
    }
    if (work->field_BA9 == 1) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

/// Arms the 0x15 / 0x16 motion pair on the first frame of the sub-state, then
/// runs the sub-state proper: while bit 1 of the motion flags at 0x9C is set,
/// either keeps the state on the 0x17 motion once `field_B92` has run out and
/// the enemy is not carrying flag 0x2 in `field_4C`, or hands the frame to
/// `Actor01100_Fn05678` on the 0x18 motion.
void Actor01100_Fn07014(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpAnimSlot* motion = &work->slots[1];

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0x15;
        } else {
            work->field_BA4 = 0x16;
        }
        work->field_BA5 = 1;
        work->field_B8C = 0xA;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (motion->flags & 2) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->reactionFlags & 2)) {
                work->field_BA6 = 1;
                work->field_BAB = 0x10;
                work->state     = 0x17;
                work->field_BA8 = 0;
                work->field_BAF = 2;
            }
        } else {
            work->state     = 0x18;
            work->field_BA8 = 0;
            Actor01100_Fn05678(enemy, task, work, arg);
        }
    }
}

/// Arms the motion pair for the current sub-state when `field_BA8` is still
/// clear, and switches to state 0xF when `field_BA9` is set.
void Actor01100_Fn070DC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0xB;
        } else {
            work->field_BA4 = 0xE;
        }
        work->field_BA5 = 1;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (work->field_BA9 != 0) {
        work->field_BA6 = 1;
        work->field_B9C = 0;
        work->field_BAB = 0;
        work->state     = 0xF;
        work->field_BA8 = 0;
    }
}

/// Countdown handler built around the halfword at 0xB8C, the entry before
/// `Actor01100_Fn072B8` in `Actor01100_D00064`.
///
/// The first frame arms the motion pair: `field_BA4` takes 0xA, or 0xD while
/// the flag at 0xBAE is set, `field_BAF` takes 1 and the countdown is zeroed,
/// with the run-once latch at 0xBA8 stepped in both cases. Every later frame
/// moves the countdown up by one and, on the frame it reaches 0xD, cues the
/// 0x400B0003 event - the actor's id byte at 0xB88 in bits 8..15 and the
/// variant at 0xBB8 in bit 22, pan and depth from the frame block - then parks
/// the countdown at -0x7FFF so it fires only once. The frame block's scratch
/// byte at 0x64 takes 3 either way, and the trigger at 0xBA9 ends the
/// sub-state: while `field_B92` still counts it keeps the state on the 0x17
/// motion with the 0x10 pair when the enemy is not carrying flag 0x2 in
/// `field_4C`, and stages the 0x14 motion through `field_BA6` when it is; once
/// that count has run out it hands the frame to `Actor01100_Fn05678` on the
/// 0x18 motion instead.
void Actor01100_Fn07148(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    u16 time;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0xA;
        } else {
            work->field_BA4 = 0xD;
        }
        work->field_BAF = 1;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    time            = (u16)work->field_B8C + 1;
    work->field_B8C = time;
    if ((s16)time >= 0xD) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | (((u8)work->actorId << 8) | 0x400B0003), arg->pan, arg->depth);
        work->field_B8C = -0x7FFF;
    }
    arg->field_64 = 3;
    if (work->field_BA9 != 0) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->reactionFlags & 2)) {
                work->field_BAB = 0x10;
                work->state     = 0x17;
                work->field_BA8 = 0;
                work->field_BAF = 2;
                return;
            }
            work->field_BA6 = 2;
            work->field_BAB = 5;
            work->state     = 0x14;
            work->field_BA8 = 0;
            return;
        }
        work->state     = 0x18;
        work->field_BA8 = 0;
        Actor01100_Fn05678(enemy, task, work, arg);
    }
}

/// Sub-state handler built around the halfword countdown at 0xB8C.
///
/// The first frame arms it: `field_BA4` takes 0x10, or 0xF while the flag at
/// 0xBAE is clear, `field_BAF` is set to 2 and the countdown is zeroed, and the
/// run-once latch at 0xBA8 is stepped. Every later frame moves the countdown up
/// by one and, on the frame it reaches 0x28 - 0x3C while 0xBAE is set - clears
/// `field_BAF` again. The frame block's scratch byte at 0x64 takes 3 either
/// way, and the trigger at 0xBA9 ends the sub-state by dropping the enemy out
/// of its link-node slot, latching `field_BA6` and switching `state` to 0xF.
///
/// Both halves store into the field from inside each arm rather than through a
/// shared local: a local's first definition would land before the branch on
/// 0xBAE, and jump.c's arm collapse hoists one arm's constant in front of that
/// branch, which then keeps the flag and the value in separate registers.
void Actor01100_Fn072B8(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    u16 count;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0xF;
        } else {
            work->field_BA4 = 0x10;
        }
        work->field_BAF = 2;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        return;
    }
    count           = (u16)work->field_B8C + 1;
    work->field_B8C = count;
    if (work->field_BAE == 0) {
        if ((s16)count == 0x28) {
            work->field_BAF = 0;
        }
    } else {
        if ((s16)count == 0x3C) {
            work->field_BAF = 0;
        }
    }
    arg->field_64 = 3;
    if (work->field_BA9 != 0) {
        enemy->node.state.b.flags = 0;
        work->field_BA6           = 1;
        work->field_BAB           = 0;
        work->state               = 0xF;
        work->field_BA8           = 0;
    }
}

/// Teardown state: counts `killCountdown` down and calls the task's exit
/// callback once it reaches zero.
void Actor01100_Fn0736C(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        Task_CallExit(arg0);
    }
}

/// Exit callback of the secondary tasks: takes the work block's display node
/// back off the object list and kills the task.
void Actor01100_Fn073A8(Task* arg0)
{
    Gp_UnlinkObj(&((ActorsShared80137fb8Work*)arg0->work)->obj);
    taskKill(arg0);
}

/// Per-frame state of the actor's two-state controller (state 1). While
/// `Gp_StateF0.field_4` is zero the actor runs its self-destruct countdown: from
/// `killCountdown` 0x15 and above it throws an effect burst (0x60070) at the
/// model's root coordinate on every other frame and reparents the spawned
/// effect onto itself, and a collision hit on the work block's `GpRec18` table
/// -- masked to the 0x10000 slot -- or the countdown reaching 0x14 clears the
/// two 0xC000 bits the spawn state set in the object's flags. The countdown
/// then ticks down and the task calls its exit callback once it reaches zero.
void Actor01100_Fn073DC(Task* task)
{
    ActorsShared80137fb8Work* work;
    GsCOORDINATE2*            coord;
    GpObj*                    obj;
    struct GpEffWork*         eff;
    s16                       countdown;

    work  = (ActorsShared80137fb8Work*)task->work;
    coord = task->extra.tmd->coords;

    if (Gp_StateF0.field_4 == 0) {
        if (task->killCountdown >= 0x15) {
            if (((u16)task->killCountdown & 1) == 0) {
                eff = Gp_SpawnEff(0x60070, coord, 0xC0031FFF, NULL);
                if (eff != NULL) {
                    Task_Reparent(task, eff->task);
                }
            }
            if (Gp_CountRec18Hi(&work->rec18[0], 0x10000) != 0) {
                obj         = &work->obj;
                obj->flags &= 0x3FFF;
            }
        }
        if (task->killCountdown == 0x14) {
            obj         = &work->obj;
            obj->flags &= 0x3FFF;
        }
        countdown           = (u16)task->killCountdown - 1;
        task->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            Task_CallExit(task);
        }
    }
}
