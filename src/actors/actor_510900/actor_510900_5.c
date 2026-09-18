#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/session.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "actors/actor_510900.h"
#include "actors/actors_shared_8013bbe4.h"

extern u8 D_801153F4;

void func_actor_510900_80135744(Actor510900* arg0);
void func_actor_510900_801355B4(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013864C(Actor510900* arg0);
void func_actor_510900_801387F4(Actor510900* arg0);
void func_actor_510900_80138978(Actor510900* arg0);
void func_actor_510900_80138A9C(Actor510900* arg0);
void func_actor_510900_80138BF0(Actor510900* arg0);
void func_actor_510900_80138D38(Actor510900* arg0);
void func_actor_510900_80138F44(Actor510900* arg0);
void func_actor_510900_8013B804(Actor510900* arg0);
void func_actor_510900_8013B870(Actor510900* arg0);
void func_actor_510900_8013BB20(Actor510900* arg0);
void func_actor_510900_8013BC38(Actor510900* arg0, Actor510900Coord* arg1);
void func_actor_510900_8013BC80(Actor510900* arg0);

/// The child task's grab state machine, stepped by
/// `func_actor_510900_8013AF38`.
/// State 1 watches the parent's `field_5C4` phase and its own collision record
/// for a 0x20000 hit; landing one spawns the grab effects, reparents this task
/// under the effect's task and hands the victim the state in `field_78`. The
/// timer in `field_76` then runs the hold out (state 3), and states 4 and 5
/// finish or release the victim.
void func_actor_510900_8013B0D8(Task* arg0)
{
    Actor510900ChildWork* work;
    Actor510900Coord*     coord;
    Actor510900Work*      parent;
    Actor510900Ctx*       ctx;
    GpEffWork*            eff;
    Task*                 held;
    Task*                 ending;
    Task*                 dropped;
    Task*                 released;
    Task*                 spawned;
    s32                   hit;
    s32                   tag;
    s32                   snd;
    s32                   pan;
    s32                   next;
    u16                   left;

    work   = (Actor510900ChildWork*)arg0->work;
    coord  = ((Actor510900Obj2C*)arg0->extra)->field_8;
    parent = (Actor510900Work*)arg0->parent->work;
    ctx    = (Actor510900Ctx*)arg0->spawnArg2;

    switch (work->field_74) {
        case 0:
            next = parent->field_5C4;
            if (next == 1) {
                work->field_74 = next;
            }
            break;
        case 1:
            ctx->node.field_4 = Gp_StateF0.field_0 != 1;
            hit               = work->rec20.field_4;
            work->obj0.flags |= 0x8000;
            if ((hit & ~0x7FFF) == 0x20000) {
                tag = ctx->node.field_5;
                if (tag == 1 && Gp_ComputeDamage(hit, 0x3E8, 0, 0) != 0) {
                    work->field_74 = 2;
                    work->field_76 = 0x3C;
                    Gp_SpawnEff(0x6003B, &coord->field_0, 0, NULL);
                    eff = Gp_SpawnEff(0x8006005F, &coord->field_0, 0, NULL);
                    if (eff != NULL) {
                        spawned        = eff->field_0;
                        work->field_70 = spawned;
                        Task_Reparent(arg0, spawned);
                    }
                    work->obj0.flags  &= 0x7FFF;
                    work->obj38.flags |= 0x8000;
                    if (parent->field_5C4 == 2) {
                        work->field_78       = tag;
                        work->obj38.field_18 = 0x50003;
                    } else {
                        work->field_78       = 0;
                        work->obj38.field_18 = 0x50004;
                    }
                    snd = (((u16)ctx->field_8 >> 0xC) << 8) | 0x51100002;
                    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            }
            Gp_ClearRec18Occupied(&work->rec20);
            break;
        case 2:
            held           = work->field_70;
            work->field_74 = 3;
            if (held != NULL) {
                held->state = work->field_78;
            }
            if (Gp_FindRec18(&work->rec58, 0) != 0) {
                work->obj38.flags &= 0x7FFF;
            }
            Gp_ClearRec18Occupied(&work->rec58);
            break;
        case 3:
            if (Gp_FindRec18(&work->rec58, 0) != 0) {
                work->obj38.flags &= 0x7FFF;
            }
            Gp_ClearRec18Occupied(&work->rec58);
            left           = work->field_76 - 1;
            work->field_76 = left;
            if ((s16)left <= 0) {
                work->field_74     = 4;
                work->obj38.flags &= 0x7FFF;
                ending             = work->field_70;
                if (ending != NULL) {
                    ending->state = 2;
                }
                break;
            }
            if (parent->field_592 == 0) {
                work->field_74     = 5;
                work->obj38.flags &= 0x7FFF;
                dropped            = work->field_70;
                if (dropped != NULL) {
                    dropped->state = 3;
                    work->field_70 = NULL;
                }
            }
            break;
        case 4:
            if (parent->field_592 == 0) {
                work->field_74 = 5;
                released       = work->field_70;
                if (released != NULL) {
                    released->state = 3;
                    work->field_70  = NULL;
                }
            }
            break;
        case 5:
            break;
    }
}

void func_actor_510900_801350F8(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B658(Actor510900Ctx* arg0, Actor510900* arg1);

void func_actor_510900_8013B3D0(Task* task)
{
    void (*fns[2])(Actor510900Ctx*, Actor510900*) = { func_actor_510900_801350F8, func_actor_510900_8013B658 };

    fns[task->state](task->spawnArg2, task);
}

/// Adds (arg0 == 1) or clears the extra collision-grid face this actor edits
/// in, extending the three faces `func_actor_510900_8013B524` restores with a
/// fourth. Clearing zeroes only the vertices and normal; the face record stays.
void func_actor_510900_8013B424(s32 arg0)
{
    s32         i;
    SVECTOR*    normals = Gp_GridParams->field_4;
    SVECTOR*    verts   = Gp_GridParams->field_8;
    GpGridFace* faces   = Gp_GridParams->field_C;

    if (arg0 == 1) {
        for (i = 0; i < 4; i++) {
            verts[12 + i] = D_actor_510900_80167C68[i];
        }
        normals[3] = D_actor_510900_80167C60;
        faces[3]   = D_actor_510900_80167C88;
    } else {
        normals[3].vx = 0;
        normals[3].vy = 0;
        normals[3].vz = 0;
        for (i = 0; i < 4; i++) {
            verts[12 + i].vx = 0;
            verts[12 + i].vy = 0;
            verts[12 + i].vz = 0;
        }
    }
}

/// Restores the collision-grid faces this actor edited. The spawn handler
/// passes its task, which this never reads; the parameter is kept because the
/// call site materialises it.
void func_actor_510900_8013B524(Actor510900* arg0)
{
    s32         i;
    SVECTOR*    normals = Gp_GridParams->field_4;
    SVECTOR*    verts   = Gp_GridParams->field_8;
    GpGridFace* faces   = Gp_GridParams->field_C;

    for (i = 0; i < 12; i++) {
        verts[i] = D_actor_510900_80167BDC[i];
    }

    for (i = 0; i < 3; i++) {
        normals[i] = D_actor_510900_80167BC4[i];
        faces[i]   = D_actor_510900_80167C3C[i];
    }
}

void func_actor_510900_8013B608(Actor510900* arg0)
{
    Actor510900Work* work = arg0->field_1C;

    Gp_UnlinkObj(&work->obj47C);
    Gp_UnlinkObj(&work->obj4E4);
    Gp_UnlinkObj(&work->obj504);
    Gp_DestroyEnemy(arg0->field_20, (Task*)arg0);
}

void func_actor_510900_8013B658(Actor510900Ctx* arg0, Actor510900* arg1)
{
    if (gGameSession->eventState != 0) {
        func_actor_510900_801355B4(arg0, arg1);
        return;
    }
    func_actor_510900_8013B6A0(arg0, arg1);
}

void func_actor_510900_8013B6A0(Actor510900Ctx* arg0, Actor510900* arg1)
{
    Actor510900Coord* temp_s1;
    Actor510900Obj2C* temp_a1;
    Actor510900Work*  temp_s2;
    s32               one;
    s32               sp;

    temp_s2 = arg1->field_1C;
    temp_a1 = arg1->field_2C;
    temp_s1 = temp_a1->field_8;
    if (temp_s2->field_5A4 != 0) {
        sp  = D_801153F4;
        one = 1;
        if (sp == one) {
            goto case1;
        }
        if (sp >= 2) {
            goto ge2;
        }
        if (sp == 0) {
            goto case0;
        }
        goto default_body;
    ge2:
        if (sp == 2) {
            goto case2;
        }
        goto default_body;
    case0:
        temp_a1->field_C   = 0;
        arg0->node.field_4 = 8;
        goto default_body;
    case1:
        ActorsShared8013bbe4((ActorShared8013bbe4*)arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        return;
    case2:
        temp_a1->field_C   = 0x80;
        arg0->node.field_4 = one;
        return;
    default_body:
        if (arg0->field_4C != 0) {
            func_actor_510900_8013B804(arg1);
        }
        func_actor_510900_80135744(arg1);
        func_actor_510900_8013864C(arg1);
        func_actor_510900_8013B870(arg1);
        func_actor_510900_801387F4(arg1);
        func_actor_510900_80138978(arg1);
        func_actor_510900_80138A9C(arg1);
        func_actor_510900_8013BB20(arg1);
        func_actor_510900_80138BF0(arg1);
        if (temp_s2->field_584 != 0) {
            func_actor_510900_80138D38(arg1);
        }
        temp_s1->field_0.flg                  = 0;
        arg1->field_2C->field_8->field_50.flg = 0;
        Gp_UpdateCoord(&temp_s1->field_0);
        ActorsShared8013bbe4((ActorShared8013bbe4*)arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        func_actor_510900_80138F44(arg1);
        func_actor_510900_8013BC80(arg1);
    }
}

void func_actor_510900_8013B804(Actor510900* arg0)
{
    Actor510900Work* work;
    GpEnemy*         enemy;
    u8               flags;

    enemy = arg0->field_20;
    flags = enemy->field_4C;
    work  = arg0->field_1C;
    if (flags & 1) {
        enemy->field_4C = flags & 0xFE;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        work->field_58E  = 7;
        work->field_590  = 0;
        work->field_5B8  = 1;
    }
    flags = enemy->field_4C;
    if (flags & 0xC) {
        enemy->field_4C = flags & 0xF3;
    }
}
