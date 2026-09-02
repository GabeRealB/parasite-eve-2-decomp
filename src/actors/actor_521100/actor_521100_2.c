#include "common.h"

#include "actors/actor_521100.h"

void      Gp_UpdateCoord(Actor521100Coord* arg0);
void      func_actor_521100_801322F8(Actor521100* arg0, Actor521100Obj2C* arg1, s32 arg2);
void      func_actor_521100_80134C38(Actor521100* arg0);
void      func_actor_521100_80134D88(Actor521100* arg0);
void      func_actor_521100_80134EDC(Actor521100* arg0);
void      func_actor_521100_80135024(Actor521100* arg0);
void      func_actor_521100_801358D4(Actor521100* arg0);
void      func_actor_521100_80135964(Actor521100* arg0);
void      func_actor_521100_80135A34(Actor521100* arg0);
void      func_actor_521100_80135A90(Actor521100* arg0);
extern u8 D_801153F4;
void      func_actor_521100_80132958(void);
void      func_actor_521100_80132DE8(void);
void      func_actor_521100_801339B0(void);
void      func_actor_521100_8013570C(void);
void      func_actor_521100_80134658(void);
void      func_actor_521100_801357F0(void);
void      func_actor_521100_80134774(void);
void      Gp_IncStateF0Ref(s32 arg0);

void func_actor_521100_801355C8(Actor521100* arg0)
{
    s16 temp_v1;

    temp_v1 = arg0->field_1C->field_69E;
    switch (temp_v1) {
        case 0:
            func_actor_521100_80132958();
            return;
        case 1:
            func_actor_521100_80132DE8();
            return;
        case 2:
            func_actor_521100_801339B0();
            return;
        case 3:
            func_actor_521100_8013570C();
            return;
        case 4:
            func_actor_521100_80134658();
            return;
        case 5:
            func_actor_521100_801357F0();
            return;
        case 6:
            func_actor_521100_80134774();
        default:
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135680);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_8013570C);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_801357F0);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_801358D4);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135964);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135A34);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135A90);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135AE4);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135B40);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135B80);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135BEC);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135C14);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135CAC);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135D10);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135D58);

s32 func_actor_521100_80135D9C(Actor521100* arg0)
{
    arg0->field_1C->field_6B0 = 1;
    Gp_IncStateF0Ref(0);
    return 0;
}

s16 func_actor_521100_80135DC8(Actor521100* arg0)
{
    return arg0->field_1C->field_6B2;
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135F2C);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_801360C4);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80136290);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80136404);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80136604);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80136680);
