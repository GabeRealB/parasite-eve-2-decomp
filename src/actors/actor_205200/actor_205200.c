#include "common.h"

#include "actors/actor_205200.h"
#include "actors/actors_shared_80134ff0.h"
#include "gameplay/1BC.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

extern u16                 D_actor_205200_8014C9CC[];
extern s16                 D_actor_205200_8014CA1C[];
extern TaskDesc            D_actor_205200_8014CA60;
extern u8                  D_actor_205200_8014CA78[];
extern TaskDesc            D_actor_205200_8014CA44;
extern Actor205200SpawnRec D_actor_205200_8015B458;
extern GpPairSrcE          D_actor_205200_8014C9BC;
extern SVECTOR*            D_actor_205200_8014CA24[];
extern u16*                D_actor_205200_8014CA34[];
extern GsCOORDINATE2       Gfx_ViewCoord;
extern u16                 D_80071078;
extern u32                 Gp_LcgState;

void func_8017E090(s32, s32);
void func_8017EE08(s32, s32);
void func_80182A14(s32, s32);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_80149E54);

void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
    Actor205200Work* work;
    u16              kind;
    s32              i;
    u16              timer;

    kind = ((u16*)enemy->field_3C)[1];
    if ((u16)(kind - 1) >= 3) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    work = Mem_Calloc(0x30, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work                      = (TaskIdMap*)work;
    work->field_1E                  = kind;
    D_actor_205200_8015B458.field_4 = 2;
    for (i = 0; i < D_actor_205200_8014CA1C[work->field_1E]; i++) {
        Gp_SpawnEnemyFromTable(&D_actor_205200_8014CA60, 1, 0, enemy);
    }
    timer          = D_actor_205200_8014C9CC[D_actor_205200_8014CA1C[work->field_1E]];
    work->field_2A = 5;
    work->field_22 = timer;
    /* The empty `case 0` is load-bearing: a fourth case node makes GCC root
       the decision tree at 1 (`beq 1; slti <2`) instead of at 2. */
    switch (work->field_1E) {
        case 1:
            func_8017E090(0, 0);
            func_8017E090(1, 0);
            GameFlag_SetNibble(0x142, 0);
            GameFlag_SetNibble(0x143, 0);
            break;
        case 2:
            func_8017EE08(0, 0);
            func_8017EE08(1, 0);
            func_8017EE08(2, 0);
            GameFlag_SetNibble(0x144, 0);
            GameFlag_SetNibble(0x145, 0);
            break;
        case 3:
            func_80182A14(0, 0);
            func_80182A14(1, 0);
            GameFlag_SetNibble(0x153, 0);
            GameFlag_SetNibble(0x154, 0);
            break;
        case 0:
            break;
    }
    task->msgTable = D_actor_205200_8014CA78;
    task->state    = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", ActorsShared80131e24Sub1);

void func_actor_205200_8014AB98(Actor205200* arg0)
{
    Actor205200Work* work  = arg0->field_1C;
    s32              state = work->field_26;

    switch (state) {
        case 0:
            if ((s16)--work->field_22 <= 0) {
                if (D_actor_205200_8015B458.field_4 == 2) {
                    D_actor_205200_8015B458.field_0 = 0xF;
                    D_actor_205200_8015B458.field_2 = 0xA0;
                    Task_SpawnFromTable(&D_actor_205200_8014CA44, 0, 0, (s32)&D_actor_205200_8015B458);
                    Gp_ArmStateF0(1);
                    work->field_28 = 1;
                    SndEvt_EnqueueType6(((arg0->field_20->field_8 >> 12) << 8) | 0x40340002, 0, 0);
                }
                work->field_22 = 20;
                work->field_26 = 1;
            }
            break;
        case 1:
            if ((s16)--work->field_22 <= 0) {
                D_actor_205200_8015B458.field_4 = state;
                work->field_22                  = D_actor_205200_8014C9CC[work->field_20];
                work->field_26                  = 0;
                Gp_SpendMp(1);
                work->field_28 = 0;
            }
            break;
    }
}

void func_actor_205200_8014ACD4(Actor205200* arg0)
{
    Actor205200Work* work = arg0->field_1C;
    GpViewRec*       view;
    VECTOR           d;
    u32              dist;
    s32              i;

    work->field_C  = NULL;
    work->field_10 = -1;
    view           = Gp_GetStageView(&gGameSession->at4.loc.view);
    for (i = 0; i < 3; i++) {
        if (work->field_18[i] == 1) {
            work->field_0[i]->flg = 0;
            Gp_UpdateCoord(work->field_0[i]);
            d.vx = view->mtx.t[0] + work->field_0[i]->coord.t[0];
            d.vy = view->mtx.t[1] + work->field_0[i]->coord.t[1];
            d.vz = view->mtx.t[2] + work->field_0[i]->coord.t[2];
            dist = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
            if (dist < work->field_10) {
                work->field_C  = work->field_0[i];
                work->field_10 = dist;
            }
        }
    }
}

void func_actor_205200_8014AE0C(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*   coord;
    Actor205200Work* pwork;
    Actor205200Part* part;
    SVECTOR*         pos;
    SVECTOR          rot;
    MATRIX*          mat;
    u16*             tbl;

    coord = ((TmdObject*)arg1->extra)->coords;
    pwork = (Actor205200Work*)arg1->parent->work;
    part  = Mem_Calloc(0x7CU, false);
    if (part == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)part;
    part->field_78 = pwork->field_20;
    pwork->field_20++;
    pwork->field_0[part->field_78]  = coord;
    pwork->field_18[part->field_78] = 1;
    tbl                             = D_actor_205200_8014CA34[pwork->field_1E];
    rot.vx                          = 0;
    mat                             = &coord->coord;
    rot.vy                          = tbl[part->field_78];
    rot.vz                          = 0;
    RotMatrix(&rot, mat);
    pos               = D_actor_205200_8014CA24[pwork->field_1E];
    coord->coord.t[0] = pos[part->field_78].vx;
    coord->coord.t[1] = pos[part->field_78].vy;
    coord->coord.t[2] = pos[part->field_78].vz;
    coord->sub        = &Gfx_ViewCoord;
    coord->flg        = 0;
    arg0->field_4     = mat;
    arg0->field_48    = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18    = coord;
    arg0->field_1C.vx = 0;
    arg0->field_1C.vy = 0;
    arg0->field_1C.vz = 0;
    arg0->field_50    = &D_actor_205200_8014C9BC;
    arg0->field_54    = (s32)part->recs;
    arg0->field_40    = D_actor_205200_8014C9BC.field_4;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    part->field_6C     = 0x400;
    part->field_6E     = 3;
    part->field_68     = coord;
    part->obj.field_8  = coord;
    part->obj.field_C  = part->recs;
    part->obj.field_10 = 0;
    part->obj.field_12 = 0;
    part->obj.field_14 = 0;
    part->obj.field_18 = 0x30034;
    part->obj.field_1C = 0x1C2;
    part->obj.flags    = 1;
    Gp_LinkObj(2, &part->obj);
    Gp_InitRec18Table(part->recs, 3, 0);
    part->obj.flags |= 0x8000;
    arg1->state      = 1;
}

void func_actor_205200_8014B048(Actor205200* arg0, s32 arg1)
{
    VECTOR*          vec;
    Actor205200Part* part;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    Actor205200Work* parentWork;
    s32              damage;
    s32              i;
    s32              snd;
    s32              hitTime;
    s32              clamped;

    vec   = --*(VECTOR**)0x1F8003FC;
    coord = arg0->field_2C->field_8;
    part  = (Actor205200Part*)arg0->field_1C;
    enemy = (GpEnemy*)arg0->field_20;
    if (part->field_70 != 0) {
        part->field_70--;
        if (part->field_70 <= 0) {
            part->field_70 = 0;
        }
    }
    if (part->field_76 != 0) {
        part->field_76--;
    }
    if (part->field_70 == 0) {
        for (i = 0; i < 3; i++) {
            if ((part->recs[i].field_4 & 0xFFFF0000) != 0x20000) {
                continue;
            }
            if (part->recs[i].field_4 & 0x8000) {
                func_800DA6E8(&enemy->node, 0, 0);
                break;
            }
            vec->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            vec->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            damage  = Gp_ComputeDamage(part->recs[i].field_4, SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz), 0, 0);
            if (Gp_RollEnemyChance(enemy, part->recs[i].field_4, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, coord, 0, NULL);
            }
            func_800DA6E8(&enemy->node, damage, 0);
            enemy->field_40 -= damage;
            if (enemy->field_40 <= 0) {
                arg0->field_30                                                    = 2;
                part->field_72                                                    = 0;
                ((Actor205200Work*)arg0->field_8->work)->field_18[part->field_78] = 0;
                ((Actor205200Work*)arg0->field_8->work)->field_0[part->field_78]  = NULL;
                Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
                Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
                Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
                Gp_SpawnEff(0x6005C, coord, 0x02002600, NULL);
                Gp_SpawnEff(0x6005C, coord, 0x02002600, NULL);
                snd = ((enemy->field_8 >> 12) << 8) | 0x40340004;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                Gp_SpawnPadLerp(10, 0xFF, 0x80);
            } else if (damage > 0) {
                if (part->field_76 == 0) {
                    if ((Gp_GetIdParam0(part->recs[i].field_4) & 0xFFFF) == 7) {
                        func_800FDB18(3, coord, NULL, (GpEffArg*)&part->field_68);
                    }
                    func_800FDB18(7, coord, NULL, (GpEffArg*)&part->field_68);
                    part->field_76 = 10;
                }
                if (damage < 201) {
                    clamped = damage;
                } else {
                    clamped = 200;
                }
                part->field_74 = (clamped * 120) / 200 + 30;
                hitTime        = Gp_GetIdParam2(part->recs[i].field_4);
                if (hitTime > 0) {
                    part->field_70 = hitTime;
                }
                snd = ((enemy->field_8 >> 12) << 8) | 0x40340003;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
        }
    }
    Gp_ClearRec18Occupied(part->recs);
    *(VECTOR**)0x1F8003FC += 1;
}

void func_actor_205200_8014B484(GpEnemy* arg0, Task* arg1)
{
    Actor205200Part* part;
    GsCOORDINATE2*   coord;
    Actor205200Work* work;
    GpViewRec*       view;
    VECTOR           d;
    s32              dist;
    s32              snd;
    s32              pan;
    s32              vol;

    part  = (Actor205200Part*)arg1->work;
    coord = ((TmdObject*)arg1->extra)->coords;
    work  = (Actor205200Work*)arg1->parent->work;
    if (Gp_StateF0.field_4 != 0) {
        return;
    }
    switch (part->field_72) {
        case 0:
            Gp_SpawnEff(0x60070, coord, 0x32001400, NULL);
            Gp_SpawnEff(0x60070, coord, 0x32001400, NULL);
            Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
            Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x34);
            arg0->field_54 = 0;
            work->field_2C = 1;
            work->field_20--;
            Gp_StateF0.field_1D |= 1;
            switch (work->field_1E) {
                case 1:
                    func_8017E090((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x142, 1);
                    break;
                case 2:
                    func_8017EE08((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x144, 1);
                    break;
                case 3:
                    func_80182A14((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x153, 1);
                    break;
                case 0:
                    break;
            }
            part->field_72 = 1;
            part->field_74 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F) + 0x1E;
            part->field_76 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F) + 0x1E;
            break;
        case 1:
            if ((s16)--part->field_74 <= 0) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                part->field_74 = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
                func_800FDB18(7, coord, NULL, (GpEffArg*)&part->field_68);
                Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
                view = Gp_GetStageView(&gGameSession->at4.loc.view);
                d.vx = view->mtx.t[0] + coord->coord.t[0];
                d.vy = view->mtx.t[1] + coord->coord.t[1];
                d.vz = view->mtx.t[2] + coord->coord.t[2];
                dist = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
                snd  = ((arg0->field_8 >> 12) << 8) | 0x40340005;
                pan  = (s8)Gp_GetObjPan((GpObj38*)coord);
                vol  = dist - D_80071078;
                if (vol >= 0x7FFF) {
                    vol = 0x7FFF;
                }
                if (vol < -0x7FFF) {
                    vol = -0x7FFF;
                }
                SndEvt_EnqueueType6(snd, pan, (s16)vol >> 8);
            }
            if ((s16)--part->field_76 <= 0) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                part->field_76 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
                Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
                Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
            }
            break;
        case 2:
            ((void (*)(Task*, s32))Gp_ReleaseStateF0)(arg1, 0x34);
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            part->field_72 = 3;
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E24);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E30);
