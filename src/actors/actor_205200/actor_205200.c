#include "common.h"

#include "actors/actor_205200.h"
#include "actors/actors_shared_80134ff0.h"
#include "gameplay/1BC.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/sound.h"

extern u16                 D_actor_205200_8014C9CC[];
extern TaskDesc            D_actor_205200_8014CA44;
extern Actor205200SpawnRec D_actor_205200_8015B458;
extern GpPairSrcE          D_actor_205200_8014C9BC;
extern SVECTOR*            D_actor_205200_8014CA24[];
extern u16*                D_actor_205200_8014CA34[];
extern GsCOORDINATE2       Gfx_ViewCoord;

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_80149E54);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", ActorsShared80131e24Sub0);

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
    view           = Gp_GetStageView(&Game_Session->field_4);
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

    coord = ((TmdObject*)arg1->extra)->field_8;
    pwork = (Actor205200Work*)arg1->parent->idMap;
    part  = Mem_Calloc(0x7CU, false);
    if (part == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap    = (TaskIdMap*)part;
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

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B048);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B484);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E24);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E30);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", jtbl_actor_205200_80149E40);
