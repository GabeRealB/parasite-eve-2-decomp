#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "gameplay/3CD8.h"
#include "actors/actor_206100.h"

void func_actor_206100_8014DEAC(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_80149ED0);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014A70C);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014AB3C);

void func_actor_206100_8014AF74(Task* task)
{
    Actor206100Work* work;
    TmdObject*       tmd;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              hp;

    tmd                   = task->extra;
    work                  = (Actor206100Work*)task->idMap;
    enemy                 = (GpEnemy*)task->spawnArg2;
    tmd->field_E          = 0xA;
    tmd->field_1C         = &work->lightMtx;
    tmd->field_C          = 0;
    tmd->field_20         = &work->colorMtx;
    coord                 = tmd->field_8;
    work->eff_4C0.field_0 = &((TmdObject*)task->extra)->field_8[1];
    work->eff_4C0.field_4 = 0x580;
    work->eff_4C0.field_6 = 3;
    enemy->field_4        = &coord->coord;
    enemy->field_48       = 0;
    enemy->field_1C.vx    = 0;
    enemy->field_1C.vy    = 0;
    enemy->field_1C.vz    = 0;
    enemy->field_18       = &((TmdObject*)task->extra)->field_8[4];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_54     = (s32)(GpRec18*)work->pad_384;
    enemy->field_50     = &D_actor_206100_80155198;
    hp                  = D_actor_206100_80155198.field_4;
    enemy->field_42     = hp;
    enemy->field_40     = hp;
    coord->sub          = &Gfx_ViewCoord;
    func_800B3F84(&work->anim, D_actor_206100_80158B24, (GpAnimObj*)tmd, work->animAux, work->slots);
    func_actor_206100_8014F18C(task);
    work->field_43E = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    work->field_557 = 4;
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014B0AC);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014B698);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014B8B4);

INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", D_actor_206100_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", ActorsShared80138404Table);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014BAA8);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014BEC4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014C274);

INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", D_actor_206100_80149E5C);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014C458);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014CB68);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014CD08);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014CE60);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014CFF4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D14C);

INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", D_actor_206100_80149E94);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D380);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D574);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D6F4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014D8E8);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014DA28);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014DD3C);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014DEAC);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E0C0);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E228);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E7D4);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E964);

void func_actor_206100_8014EA8C(Task* task, s16 arg1, s16 arg2)
{
    ((TmdObject*)task->extra)->field_8->coord.t[0] += ((rsin(arg2) << 4) * arg1) >> 16;
    ((TmdObject*)task->extra)->field_8->coord.t[2] += ((rcos(arg2) << 4) * arg1) >> 16;
    ((TmdObject*)task->extra)->field_8->flg         = 0;
}

void func_actor_206100_8014EB48(Task* task, s16 arg1)
{
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    work->field_556 = 1;
    work->field_554 = 1;
    work->field_544 = arg1;
}

void func_actor_206100_8014EB60(Task* task)
{
    Actor206100Work*  work;
    GsCOORDINATE2*    coords;
    SVECTOR           rot;
    Actor206100Matrix matrix;
    MATRIX*           dest;
    MATRIX*           mtx;

    work   = (Actor206100Work*)task->idMap;
    coords = ((TmdObject*)task->extra)->field_8;
    dest   = &coords[5].coord;
    mtx    = &matrix.mat;

    matrix.ident.m00_m01 = 0x1000;
    matrix.ident.m02_m10 = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    matrix.ident.m20_m21 = 0;
    mtx->m[2][2]         = 0x1000;

    Gp_MtxToEuler(dest, &rot);
    rot.vx += work->field_540;
    RotMatrix(&rot, &matrix.mat);
    dest->m[0][0] = matrix.mat.m[0][0];
    dest->m[0][1] = matrix.mat.m[0][1];
    dest->m[0][2] = matrix.mat.m[0][2];
    dest->m[1][0] = matrix.mat.m[1][0];
    dest->m[1][1] = matrix.mat.m[1][1];
    dest->m[1][2] = matrix.mat.m[1][2];
    dest->m[2][0] = matrix.mat.m[2][0];
    dest->m[2][1] = matrix.mat.m[2][1];
    dest->m[2][2] = matrix.mat.m[2][2];
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014EC54);

void func_actor_206100_8014ED3C(Task* task, s16 arg1)
{
    Actor206100Work*        work;
    GsCOORDINATE2*          coord;
    Actor206100DistScratch* head;
    Actor206100DistScratch* scratch;

    head                                      = *(Actor206100DistScratch**)G_SCRATCH_HEAD;
    scratch                                   = head - 1;
    *(Actor206100DistScratch**)G_SCRATCH_HEAD = scratch;
    work                                      = (Actor206100Work*)task->idMap;
    coord                                     = ((TmdObject*)task->extra)->field_8;
    func_actor_206100_8014EA8C(task, arg1, work->field_43E);
    scratch->delta.vx = -(u16)coord->coord.t[0];
    scratch->delta.vz = -(u16)coord->coord.t[2];
    scratch->dist     = SquareRoot0(scratch->delta.vx * scratch->delta.vx +
                                    scratch->delta.vz * scratch->delta.vz);
    if (scratch->dist >= 0x191) {
        coord->coord.t[0] = work->field_434;
        coord->coord.t[2] = work->field_438;
    }
    *(Actor206100DistScratch**)G_SCRATCH_HEAD += 1;
}

extern TaskDesc D_80147E48;

GpEnemy* func_actor_206100_8014EE2C(s32 arg0)
{
    GpEnemy*   enemy;
    TmdObject* obj;

    enemy = Gp_SpawnEnemyFromTable(&D_80147E48, 0, 3, NULL);
    if (enemy != NULL) {
        enemy->field_8  = arg0 << 12;
        enemy->field_3C = &D_actor_206100_80155134[(s16)arg0];
        obj             = (TmdObject*)enemy->task->extra;
        obj->field_24   = 0;
        obj->field_25   = 2;
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
        return enemy;
    }
    return NULL;
}

void func_actor_206100_8014EEC0(Task* task)
{
    Actor206100ChildWork* child;
    GpRec18*              rec;
    GsCOORDINATE2*        coord;

    child               = (Actor206100ChildWork*)task->idMap;
    coord               = ((TmdObject*)task->extra)->field_8;
    task->killCountdown = 0;
    child->field_64     = 0x100;
    child->field_60     = 0;
    coord->sub          = &Gfx_ViewCoord;
    coord->flg          = 0;
    child->obj.field_18 = Gp_PackPair(&D_actor_206100_80155194, 0);
    child->obj.field_8  = ((TmdObject*)task->extra)->field_8;
    rec                 = child->rec;
    child->obj.field_C  = rec;
    child->obj.field_10 = 0;
    child->obj.field_12 = 0;
    child->obj.field_14 = 0;
    child->obj.field_1C = 0x140;
    child->obj.flags    = 1;
    Gp_LinkObj(3, &child->obj);
    Gp_InitRec18Table(rec, 2, 0);
    child->obj.flags |= 0xC000;
    Gp_UpdateCoord(coord);
    func_actor_206100_8014A70C(coord, (u16)child->field_60, 0, child->field_64 + 0x10002000);
    task->state++;
}
