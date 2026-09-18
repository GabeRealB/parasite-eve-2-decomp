#include "common.h"

#include "actors/actor_403200.h"
#include "actors/actor_403200_view.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/tmd.h"
#include <psyq/inline_c.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern MATRIX* D_80073B8C;
/// Global freeze flag: 1 while the game is halted, which stops the per-frame
/// body below from walking its model out.
extern u8 D_80072729;
/// The script pair the per-frame body's two one-shot sound cues spawn.
extern s32 D_actor_403200_80141C5C;
extern s32 D_actor_403200_80141C64;
/// Non-zero while the overlay is shutting down, which is what makes the spawn
/// state below tear its enemy down instead of standing it up.
extern s16 D_actor_403200_80141C50;
/// LCG state the spawn state below rolls a random yaw out of.
extern u32 Gp_LcgState;

/// Walk `coord` 0x19/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flag it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor403200_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head       = (u8*)SCRATCH_SP;
    dir        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_SP = (u32)dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x19);
    gte_ldsv(dir);
    gte_gpf12_real();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    SCRATCH_SP = (u32)((u8*)SCRATCH_SP + sizeof(SVECTOR));
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_801339FC);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80133B80);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80133DD8);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80134044);

s32 func_actor_403200_801341E8(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->field_8;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if (view == 0x21) {
                value = 0x20;
                flag  = dist < 0x189D;
                if (flag) {
                    value = 0x21;
                }
                return value;
            }
            value = 0x21;
            flag  = dist < 0x1770;
            if (!flag) {
                value = 0x20;
            }
            return value;
        case 1:
            if ((view != 9) && (view != 10)) {
                value = 9;
                flag  = dist < 0x27D8;
            } else {
                flag = view;
                if (flag == 9) {
                    value = 0xA;
                    flag  = dist < 0x27D9;
                    if (flag) {
                        value = 9;
                    }
                    return value;
                }
                if (flag == 10) {
                    value = 9;
                    flag  = dist < 0x24EA;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xA;
            }
            return value;
        case 2:
            return 0x1B;
    }
    return 1;
}

s32 func_actor_403200_80134374(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->field_8;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
        case 1:
            if ((view != 7) && (view != 8)) {
                value = 7;
                flag  = dist < 0x26AC;
            } else {
                flag = view;
                if (flag == 7) {
                    value = 8;
                    flag  = dist < 0x26AD;
                    if (flag) {
                        value = 7;
                    }
                    return value;
                }
                if (flag == 8) {
                    value = 7;
                    flag  = dist < 0x2328;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 8;
            }
            return value;
        case 2:
            return 0x1A;
    }
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_801344C4);

s32 func_actor_403200_80134748(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->field_8;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 5) && (view != 6)) {
                value = 5;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 5) {
                    value = 6;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 5;
                    }
                    return value;
                }
                if (flag == 6) {
                    value = 5;
                    flag  = dist < 0x2198;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 6;
            }
            return value;
        case 1:
            if ((view != 0xB) && (view != 0xC)) {
                value = 0xB;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 0xB) {
                    value = 0xC;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 0xB;
                    }
                    return value;
                }
                if (flag == 0xC) {
                    value = 0xB;
                    flag  = dist < 0x1A90;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xC;
            }
            return value;
        case 2:
            return 0x1C;
    }
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80134900);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80134A14);

/// The enemy's walk-out state: a reset request re-arms the block (the two
/// 0xEF4 counters, the 0x7B0 pose flag, the 0xEFA re-arm flag, pose 2 and the
/// 0xE96 yaw target), then the per-frame body runs and the animation frame the
/// mask leaves is tested against 0x12 and 0x18 -- each one-shot cue spawning a
/// script and a type-6 sound with the enemy's pan and half its depth, once per
/// arrival -- before being latched into `field_7D8`. Unless the game is frozen
/// the model is then stepped 0x19/0x1000 forward along its own facing, its
/// `flg` cleared, and once it has run out to x 0x1CCA in state 0 or 0x2882 in
/// state 1 the step advances and re-arms `field_0`.
void func_actor_403200_80134D40(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   model;
    s16              frame;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;

    if (work->field_4 != 0) {
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        work->field_7B3 = 2;
        work->field_7B0 = 1;
        work->field_EFE = 0;
        work->field_E96 = 0xE74;
    }

    SCRATCH_SP -= 0xC;
    func_actor_403200_80133DD8(arg0);

    frame = work->field_72 & 0x3FF;
    if (frame == 0x12 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
        id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(id, pan,
                            (s8)(Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8) / 2));
    }

    frame = work->field_72 & 0x3FF;
    if (frame == 0x18 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
        id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(id, pan,
                            (s8)(Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8) / 2));
    }

    work->field_7D8 = work->field_72 & 0x3FF;

    model = ((TmdObject*)arg0->extra)->field_8;
    if (D_80072729 != 1) {
        Actor403200_StepForward(model);
    }
    ((TmdObject*)arg0->extra)->field_8->flg = 0;

    switch (work->field_F08) {
        case 0:
            if (((TmdObject*)arg0->extra)->field_8->coord.t[0] >= 0x1CCA) {
                work->field_F08++;
                work->field_0 = 3;
            }
            break;
        case 1:
            if (((TmdObject*)arg0->extra)->field_8->coord.t[0] >= 0x2882) {
                work->field_F08++;
                work->field_0 = 3;
            }
            break;
    }

    if (work->field_6 > 0) {
        work->field_F06 = 8;
    }

    SCRATCH_SP += 0xC;
}

/// Link one of the work block's display nodes: it hangs off the model's own
/// coordinate, carries `rec` as its collision-record table and sits at `pos`
/// in that coordinate's space with `field1C` as its extent.
static __inline__ void Actor403200_LinkWorkObj(GsCOORDINATE2* coord, GpObj* obj, GpRec18* rec,
                                               SVECTOR* pos, s16 field1C, s32 prio, s32 kind)
{
    obj->field_8  = coord;
    obj->field_C  = rec;
    obj->field_10 = pos->vx;
    obj->field_12 = pos->vy;
    obj->field_14 = pos->vz;
    obj->field_1C = field1C;
    obj->flags    = 1;
    Gp_LinkObj(prio, obj);
    Gp_InitRec18Table(obj->field_C, kind, 0);
}

/// Second entry of the spawn table `D_actor_403200_80131E90`: allocate the
/// work block and stand the model up where the host's first escort's part 1 is,
/// in view space.
///
/// The model is reparented to `Gfx_ViewCoord`, so both halves of that escort's
/// part 1 have to be resolved by hand: `Actor403200_AccumulateRotation` walks
/// the part's coordinate chain up to the view coordinate for the rotation and
/// `Actor403200_LocalToView` carries its origin along the same chain for the
/// translation. The model is then spun by 0x80 of the 0x1000-unit circle, its
/// single display node is linked with a 0x394 extent, and that node is paired
/// with the owning enemy so collisions against it reach this task.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down, the
/// host actor has left the grab states, or the work block cannot be allocated.
void func_actor_403200_8013509C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Actor403200Work*     host;
    SVECTOR              pos;
    SVECTOR              vec;

    owner = task->parent->spawnArg2;
    host  = (Actor403200Work*)owner->task->work;

    if (D_actor_403200_80141C50 == 1 || host->field_0 == 0x10 || host->field_0 == 5 ||
        host->field_0 == 0xC || host->field_0 == 0x12 ||
        (work = Mem_Calloc(sizeof(Actor403200GrabWork), false), task->work = (TaskIdMap*)work,
         work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work->field_1AC                         = 0;
    ((TmdObject*)task->extra)->field_8->sub = &Gfx_ViewCoord;
    ((TmdObject*)task->extra)->field_C      = 0;

    Actor403200_AccumulateRotation(&((TmdObject*)host->field_ECC[0]->task->extra)->field_8[1],
                                   &((TmdObject*)task->extra)->field_8->coord);

    vec.vx = vec.vy = vec.vz = 0;
    Actor403200_LocalToView(&((TmdObject*)host->field_ECC[0]->task->extra)->field_8[1], &vec);

    ((TmdObject*)task->extra)->field_8->coord.t[0] = vec.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = vec.vz;
    ((TmdObject*)task->extra)->field_8->flg        = 0;

    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, 0x80, 0);
    Gp_UpdateCoord(((TmdObject*)task->extra)->field_8);

    pos.vx = pos.vy = pos.vz = 0;
    Actor403200_LinkWorkObj(((TmdObject*)task->extra)->field_8, &work->obj0, &work->rec0, &pos, 0x394, 3,
                            1);

    work->obj0.flags   &= 0x7FFF;
    work->obj0.field_18 = Gp_PackObjPair((GpObj50*)owner, 2);
    work->field_1A8     = 1;
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_801354A4);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80135854);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80135CB8);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80135F98);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_801364F4);

/// Spawn state of this enemy: allocate the work block, drop the model onto the
/// floor of the view coordinate and hang the two display nodes off it.
///
/// The model is reparented to `Gfx_ViewCoord` and its translation replaced by
/// the world position of part 3 of the owning enemy's model, so the body starts
/// where that part is. `field_1AA` is a ninth of that height and `vel` the
/// horizontal gap to the player, which the later states spend a fifteenth at a
/// time. The landing cue is enqueued at the model's own pan and depth with the
/// owner's id in its high half, the model is spun to a random yaw, and the two
/// nodes are linked with their collision-record tables before the task's colour
/// and light matrices are pointed into the work block.
void func_actor_403200_8013669C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    player = Game_GetPtrSlot(3);

    if (D_actor_403200_80141C50 == 1 ||
        (work = Mem_Calloc(sizeof(Actor403200GrabWork), false), task->work = (TaskIdMap*)work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((TmdObject*)task->extra)->field_8->sub = &Gfx_ViewCoord;
    ((TmdObject*)task->extra)->field_C      = 0;

    vec.vx = vec.vy = vec.vz = 0;
    Actor403200_LocalToView(&((TmdObject*)owner->task->extra)->field_8[3], &vec);

    ((TmdObject*)task->extra)->field_8->coord.t[0] = vec.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = vec.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = vec.vz;
    ((TmdObject*)task->extra)->field_8->flg        = 0;

    work->field_1AA = ((TmdObject*)task->extra)->field_8->coord.t[1] / 9;
    work->vel.vx =
        ((TmdObject*)player->extra)->field_8->coord.t[0] - ((TmdObject*)task->extra)->field_8->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        ((TmdObject*)player->extra)->field_8->coord.t[2] - ((TmdObject*)task->extra)->field_8->coord.t[2];
    work->field_1AC = 0;
    task->state++;

    sfx = ((owner->field_8 >> 0xC) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
    SndEvt_EnqueueType6(sfx, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, ((u32)Gp_LcgState >> 0x10) & 0x1FF, 1);

    vec.vx = vec.vy = vec.vz = 0;

    Actor403200_LinkWorkObj(((TmdObject*)task->extra)->field_8, &work->obj0, &work->rec0, &vec, 0x100, 3, 1);

    work->obj1.field_8  = ((TmdObject*)task->extra)->field_8;
    work->obj1.field_C  = &work->rec1;
    work->obj1.field_10 = 0;
    work->obj1.field_12 = 0;
    work->obj1.field_14 = 0;
    work->obj1.field_18 = 0x3000A;
    work->obj1.field_1C = 0x100;
    work->obj1.flags    = 1;
    Gp_LinkObj(2, &work->obj1);

    work->obj0.flags |= 0x8000;
    Gp_InitRec18Table(work->obj1.field_C, 3, 0);
    work->obj1.flags   |= 0x4000;
    work->obj0.field_18 = Gp_PackObjPair((GpObj50*)owner, 5);

    ((TmdObject*)task->extra)->field_1C = &work->lightMtx;
    ((TmdObject*)task->extra)->field_20 = &work->colorMtx;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80136ACC);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80136D94);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_8013709C);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80137600);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80137788);
